// simplewall
// Copyright (c) 2016-2020 Henry++

#include "global.hpp"

HFONT hfont_title = nullptr;
HFONT hfont_link = nullptr;
HFONT hfont_text = nullptr;

void _app_notifycreatewindow (HWND hwnd)
{
	config.hnotification = CreateDialog (app.GetHINSTANCE (), MAKEINTRESOURCE (IDD_NOTIFICATION), hwnd, &NotificationProc);
}

bool _app_notifycommand (HWND hwnd, INT button_id, time_t seconds)
{
	size_t app_hash = _app_notifyget_id (hwnd, 0);
	PR_OBJECT ptr_app_object = _app_getappitem (app_hash);

	if (!ptr_app_object)
		return false;

	PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

	if (!ptr_app)
	{
		_r_obj_dereference (ptr_app_object);
		return false;
	}

	_app_freenotify (app_hash, ptr_app);

	OBJECTS_VEC rules;

	const INT listview_id = _app_getlistview_id (ptr_app->type);
	const INT item_pos = _app_getposition (app.GetHWND (), listview_id, app_hash);

	if (button_id == IDC_ALLOW_BTN || button_id == IDC_BLOCK_BTN)
	{
		ptr_app->is_enabled = (button_id == IDC_ALLOW_BTN);
		ptr_app->is_silent = (button_id == IDC_BLOCK_BTN);

		if (ptr_app->is_enabled && seconds)
		{
			_app_timer_set (app.GetHWND (), ptr_app, seconds);
		}
		else
		{
			if (item_pos != INVALID_INT)
			{
				_r_fastlock_acquireshared (&lock_checkbox);
				_app_setappiteminfo (app.GetHWND (), listview_id, item_pos, app_hash, ptr_app);
				_r_fastlock_releaseshared (&lock_checkbox);
			}
		}

		rules.push_back (ptr_app_object);
	}
	else if (button_id == IDC_LATER_BTN)
	{
		// TODO: do somethig!!!
	}
	else if (button_id == IDM_DISABLENOTIFICATIONS)
	{
		ptr_app->is_silent = true;
	}

	ptr_app->last_notify = _r_unixtime_now ();

	_wfp_create3filters (_wfp_getenginehandle (), rules, __LINE__);
	_app_freeobjects_vec (rules);

	_app_refreshstatus (app.GetHWND (), listview_id);
	_app_profile_save ();

	if (listview_id && _app_gettab_id (app.GetHWND ()) == listview_id)
	{
		_app_listviewsort (app.GetHWND (), listview_id);
		_r_listview_redraw (app.GetHWND (), listview_id);
	}

	return true;
}

bool _app_notifyadd (HWND hwnd, PR_OBJECT ptr_log_object, PITEM_APP ptr_app)
{
	if (!ptr_app || !ptr_log_object)
	{
		_r_obj_dereference (ptr_log_object);
		return false;
	}

	// check for last display time
	const time_t current_time = _r_unixtime_now ();
	const time_t notification_timeout = app.ConfigGet (L"NotificationsTimeout", NOTIFY_TIMEOUT_DEFAULT).AsLonglong ();

	if (notification_timeout && ((current_time - ptr_app->last_notify) <= notification_timeout))
	{
		_r_obj_dereference (ptr_log_object);
		return false;
	}

	PITEM_LOG ptr_log = (PITEM_LOG)ptr_log_object->pdata;

	if (!ptr_log)
	{
		_r_obj_dereference (ptr_log_object);
		return false;
	}

	ptr_app->last_notify = current_time;

	if (!ptr_log->hicon)
		_app_getappicon (ptr_app, false, nullptr, &ptr_log->hicon);

	// remove existing log item (if exists)
	if (ptr_app->pnotification)
	{
		_r_obj_dereference (ptr_app->pnotification);
		ptr_app->pnotification = nullptr;
	}

	ptr_app->pnotification = ptr_log_object;

	if (app.ConfigGet (L"IsNotificationsSound", true).AsBool ())
		_app_notifyplaysound ();

	if (!_r_wnd_isundercursor (hwnd))
		_app_notifyshow (hwnd, ptr_log_object, true, true);

	return true;
}

void _app_freenotify (size_t app_hash, PITEM_APP ptr_app, bool is_refresh)
{
	const HWND hwnd = config.hnotification;

	if (app_hash == _app_notifyget_id (hwnd, 0))
	{
		SetWindowLongPtr (hwnd, GWLP_USERDATA, 0); // required temp
		SetWindowLongPtr (hwnd, GWLP_USERDATA, _app_notifyget_id (hwnd, INVALID_SIZE_T));
	}

	if (ptr_app)
	{
		if (ptr_app->pnotification)
		{
			_r_obj_dereference (ptr_app->pnotification);
			ptr_app->pnotification = nullptr;

			if (is_refresh)
				_app_notifyrefresh (hwnd, true);
		}
	}
}

size_t _app_notifyget_id (HWND hwnd, size_t current_id)
{
	const size_t app_hash_current = (size_t)GetWindowLongPtr (hwnd, GWLP_USERDATA);

	if (current_id == INVALID_SIZE_T)
	{
		for (auto &p : apps)
		{
			if (p.first == app_hash_current) // exclude current app from enumeration
				continue;

			PR_OBJECT ptr_app_object = _r_obj_reference (p.second);

			if (!ptr_app_object)
				continue;

			PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

			if (ptr_app && ptr_app->pnotification)
			{
				_r_obj_dereference (ptr_app_object);
				return p.first;
			}

			_r_obj_dereference (ptr_app_object);
		}

		return 0;
	}

	return app_hash_current;
}

PR_OBJECT _app_notifyget_obj (size_t app_hash)
{
	PR_OBJECT ptr_app_object = _app_getappitem (app_hash);

	if (ptr_app_object)
	{
		PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

		if (ptr_app)
		{
			PR_OBJECT ptr_log_object = _r_obj_reference (ptr_app->pnotification);

			_r_obj_dereference (ptr_app_object);

			return ptr_log_object;
		}
	}

	return nullptr;
}

bool _app_notifyshow (HWND hwnd, PR_OBJECT ptr_log_object, bool is_forced, bool is_safety)
{
	if (!app.ConfigGet (L"IsNotificationsEnabled", true).AsBool () || !ptr_log_object)
		return false;

	PITEM_LOG ptr_log = (PITEM_LOG)ptr_log_object->pdata;

	if (!ptr_log)
		return false;

	PR_OBJECT ptr_app_object = _app_getappitem (ptr_log->app_hash);

	if (!ptr_app_object)
		return false;

	PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

	if (!ptr_app)
	{
		_r_obj_dereference (ptr_app_object);
		return false;
	}

	rstring is_signed;
	const rstring empty_text = app.LocaleString (IDS_STATUS_EMPTY, nullptr);

	if (app.ConfigGet (L"IsCertificatesEnabled", false).AsBool ())
	{
		if (ptr_app->is_signed)
		{
			PR_OBJECT ptr_signature_object = _app_getsignatureinfo (ptr_log->app_hash, ptr_app);

			if (ptr_signature_object)
			{
				if (ptr_signature_object->pdata)
					is_signed = (LPCWSTR)ptr_signature_object->pdata;

				else
					is_signed = app.LocaleString (IDS_SIGN_SIGNED, nullptr);

				_r_obj_dereference (ptr_signature_object);
			}
			else
			{
				is_signed = app.LocaleString (IDS_SIGN_SIGNED, nullptr);
			}
		}
		else
		{
			is_signed = app.LocaleString (IDS_SIGN_UNSIGNED, nullptr);
		}
	}

	SetWindowText (hwnd, _r_fmt (L"%s - " APP_NAME, app.LocaleString (IDS_NOTIFY_TITLE, nullptr).GetString ()));

	// print table text
	{
		const bool is_inbound = (ptr_log->direction == FWP_DIRECTION_INBOUND);

		_r_ctrl_settabletext (hwnd, IDC_FILE_ID, app.LocaleString (IDS_NAME, L":"), IDC_FILE_TEXT, !_r_str_isempty (ptr_app->display_name) ? _r_path_getfilename (ptr_app->display_name) : empty_text);
		_r_ctrl_settabletext (hwnd, IDC_SIGNATURE_ID, app.LocaleString (IDS_SIGNATURE, L":"), IDC_SIGNATURE_TEXT, is_signed.IsEmpty () ? empty_text : is_signed);
		_r_ctrl_settabletext (hwnd, IDC_ADDRESS_ID, app.LocaleString (IDS_ADDRESS, L":"), IDC_ADDRESS_TEXT, !_r_str_isempty (ptr_log->remote_fmt) ? ptr_log->remote_fmt : empty_text);
		_r_ctrl_settabletext (hwnd, IDC_PORT_ID, app.LocaleString (IDS_PORT, L":"), IDC_PORT_TEXT, ptr_log->remote_port ? _app_formatport (ptr_log->remote_port, false).GetString () : empty_text);
		_r_ctrl_settabletext (hwnd, IDC_DIRECTION_ID, app.LocaleString (IDS_DIRECTION, L":"), IDC_DIRECTION_TEXT, app.LocaleString (is_inbound ? IDS_DIRECTION_2 : IDS_DIRECTION_1, ptr_log->is_loopback ? L" (Loopback)" : nullptr));
		_r_ctrl_settabletext (hwnd, IDC_FILTER_ID, app.LocaleString (IDS_FILTER, L":"), IDC_FILTER_TEXT, !_r_str_isempty (ptr_log->filter_name) ? ptr_log->filter_name : empty_text);
		_r_ctrl_settabletext (hwnd, IDC_DATE_ID, app.LocaleString (IDS_DATE, L":"), IDC_DATE_TEXT, _r_fmt_date (ptr_log->date, FDTF_SHORTDATE | FDTF_LONGTIME));
	}

	SetWindowLongPtr (hwnd, GWLP_USERDATA, (LONG_PTR)ptr_log->app_hash);
	SetWindowLongPtr (GetDlgItem (hwnd, IDC_HEADER_ID), GWLP_USERDATA, (LONG_PTR)ptr_log->hicon);

	_r_ctrl_settext (hwnd, IDC_RULES_BTN, app.LocaleString (IDS_TRAY_RULES, nullptr));
	_r_ctrl_settext (hwnd, IDC_ALLOW_BTN, app.LocaleString (IDS_ACTION_ALLOW, nullptr));
	_r_ctrl_settext (hwnd, IDC_BLOCK_BTN, app.LocaleString (IDS_ACTION_BLOCK, nullptr));

	_r_ctrl_enable (hwnd, IDC_RULES_BTN, !is_safety);
	_r_ctrl_enable (hwnd, IDC_ALLOW_BTN, !is_safety);
	_r_ctrl_enable (hwnd, IDC_BLOCK_BTN, !is_safety);
	_r_ctrl_enable (hwnd, IDC_LATER_BTN, !is_safety);

	if (is_safety)
		SetTimer (hwnd, NOTIFY_TIMER_SAFETY_ID, NOTIFY_TIMER_SAFETY_TIMEOUT, nullptr);

	else
		KillTimer (hwnd, NOTIFY_TIMER_SAFETY_ID);

	_app_notifysetpos (hwnd, false);

	// prevent fullscreen apps lose focus
	const bool is_fullscreenmode = _r_wnd_isfullscreenmode ();

	if (is_forced && is_fullscreenmode)
		is_forced = false;

	RedrawWindow (GetDlgItem (hwnd, IDC_HEADER_ID), nullptr, nullptr, RDW_NOFRAME | RDW_ERASE | RDW_INVALIDATE);
	InvalidateRect (hwnd, nullptr, TRUE);

	_r_wnd_top (hwnd, !is_fullscreenmode);

	ShowWindow (hwnd, is_forced ? SW_SHOW : SW_SHOWNA);

	if (is_forced && !is_fullscreenmode)
	{
		SetForegroundWindow (hwnd);
		SetFocus (hwnd);
	}

	return true;
}

void _app_notifyhide (HWND hwnd)
{
	ShowWindow (hwnd, SW_HIDE);
}

// Play notification sound even if system have "nosound" mode
void _app_notifyplaysound ()
{
	bool result = false;
	static WCHAR notify_snd_path[MAX_PATH] = {0};

	if (_r_str_isempty (notify_snd_path) || !_r_fs_exists (notify_snd_path))
	{
		HKEY hkey = nullptr;
		notify_snd_path[0] = 0;

#define NOTIFY_SOUND_NAME L"MailBeep"

		if (RegOpenKeyEx (HKEY_CURRENT_USER, L"AppEvents\\Schemes\\Apps\\.Default\\" NOTIFY_SOUND_NAME L"\\.Default", 0, KEY_READ, &hkey) == ERROR_SUCCESS)
		{
			rstring path = _r_reg_querystring (hkey, nullptr);

			if (!path.IsEmpty ())
			{
				path = _r_path_expand (path);

				if (_r_fs_exists (path))
				{
					_r_str_copy (notify_snd_path, _countof (notify_snd_path), path);
					result = true;
				}
			}

			RegCloseKey (hkey);
		}
	}
	else
	{
		result = true;
	}

	if (!result || !_r_fs_exists (notify_snd_path) || !PlaySound (notify_snd_path, nullptr, SND_ASYNC | SND_NODEFAULT | SND_FILENAME | SND_SENTRY))
		PlaySound (NOTIFY_SOUND_NAME, nullptr, SND_ASYNC | SND_NODEFAULT | SND_SENTRY);
}

void _app_notifyrefresh (HWND hwnd, bool is_safety)
{
	if (!app.ConfigGet (L"IsNotificationsEnabled", true).AsBool () || !IsWindowVisible (hwnd))
	{
		_app_notifyhide (hwnd);
		return;
	}

	PR_OBJECT ptr_log_object = _app_notifyget_obj (_app_notifyget_id (hwnd, 0));

	if (!ptr_log_object)
	{
		_app_notifyhide (hwnd);
		return;
	}

	_app_notifyshow (hwnd, ptr_log_object, true, is_safety);

	_r_obj_dereference (ptr_log_object);
}

void _app_notifysetpos (HWND hwnd, bool is_forced)
{
	if (!is_forced && IsWindowVisible (hwnd))
	{
		RECT windowRect = {0};
		GetWindowRect (hwnd, &windowRect);

		_r_wnd_adjustwindowrect (hwnd, &windowRect);
		SetWindowPos (hwnd, nullptr, windowRect.left, windowRect.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

		return;
	}

	const bool is_intray = app.ConfigGet (L"IsNotificationsOnTray", false).AsBool ();

	if (is_intray)
	{
		RECT windowRect = {0};
		GetWindowRect (hwnd, &windowRect);

		RECT desktopRect = {0};
		SystemParametersInfo (SPI_GETWORKAREA, 0, &desktopRect, 0);

		APPBARDATA abd = {0};

		abd.cbSize = sizeof (abd);

		if (SHAppBarMessage (ABM_GETTASKBARPOS, &abd))
		{
			const INT border_x = _r_dc_getsystemmetrics (hwnd, SM_CXBORDER);

			if (abd.uEdge == ABE_LEFT)
			{
				windowRect.left = abd.rc.right + border_x;
				windowRect.top = (desktopRect.bottom - _R_RECT_HEIGHT (&windowRect)) - border_x;
			}
			else if (abd.uEdge == ABE_TOP)
			{
				windowRect.left = (desktopRect.right - _R_RECT_WIDTH (&windowRect)) - border_x;
				windowRect.top = abd.rc.bottom + border_x;
			}
			else if (abd.uEdge == ABE_RIGHT)
			{
				windowRect.left = (desktopRect.right - _R_RECT_WIDTH (&windowRect)) - border_x;
				windowRect.top = (desktopRect.bottom - _R_RECT_HEIGHT (&windowRect)) - border_x;
			}
			else/* if (appbar.uEdge == ABE_BOTTOM)*/
			{
				windowRect.left = (desktopRect.right - (windowRect.right - windowRect.left)) - border_x;
				windowRect.top = (desktopRect.bottom - _R_RECT_HEIGHT (&windowRect)) - border_x;
			}

			SetWindowPos (hwnd, nullptr, windowRect.left, windowRect.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
			return;
		}
	}

	_r_wnd_center (hwnd, nullptr); // display window on center (depends on error, config etc...)
}

HFONT _app_notifyfontinit (HWND hwnd, PLOGFONT plf, LONG height, LONG weight, LPCWSTR name, BYTE is_underline)
{
	if (height)
		plf->lfHeight = _r_dc_fontsizetoheight (hwnd, height);

	plf->lfWeight = weight;
	plf->lfUnderline = is_underline;

	plf->lfCharSet = DEFAULT_CHARSET;
	plf->lfQuality = DEFAULT_QUALITY;

	if (!_r_str_isempty (name))
		_r_str_copy (plf->lfFaceName, LF_FACESIZE, name);

	return CreateFontIndirect (plf);
}

void _app_notifyfontset (HWND hwnd)
{
	SendMessage (hwnd, WM_SETICON, ICON_SMALL, (LPARAM)app.GetSharedImage (nullptr, SIH_EXCLAMATION, _r_dc_getsystemmetrics (hwnd, SM_CXSMICON)));
	SendMessage (hwnd, WM_SETICON, ICON_BIG, (LPARAM)app.GetSharedImage (nullptr, SIH_EXCLAMATION, _r_dc_getsystemmetrics (hwnd, SM_CXICON)));

	const INT title_font_height = 12;
	const INT text_font_height = 9;

	NONCLIENTMETRICS ncm = {0};
	ncm.cbSize = sizeof (ncm);

	if (SystemParametersInfo (SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0))
	{
		SAFE_DELETE_OBJECT (hfont_title);
		SAFE_DELETE_OBJECT (hfont_link);
		SAFE_DELETE_OBJECT (hfont_text);

		hfont_title = _app_notifyfontinit (hwnd, &ncm.lfCaptionFont, title_font_height, FW_NORMAL, UI_FONT, FALSE);
		hfont_link = _app_notifyfontinit (hwnd, &ncm.lfMessageFont, text_font_height, FW_NORMAL, UI_FONT, TRUE);
		hfont_text = _app_notifyfontinit (hwnd, &ncm.lfMessageFont, text_font_height, FW_NORMAL, UI_FONT, FALSE);

		SendDlgItemMessage (hwnd, IDC_HEADER_ID, WM_SETFONT, (WPARAM)hfont_title, TRUE);
		SendDlgItemMessage (hwnd, IDC_FILE_TEXT, WM_SETFONT, (WPARAM)hfont_link, TRUE);

		for (INT i = IDC_SIGNATURE_TEXT; i <= IDC_DATE_TEXT; i++)
			SendDlgItemMessage (hwnd, i, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, 0);

		for (INT i = IDC_SIGNATURE_TEXT; i <= IDC_LATER_BTN; i++)
			SendDlgItemMessage (hwnd, i, WM_SETFONT, (WPARAM)hfont_text, TRUE);
	}

	// set button images
	SendDlgItemMessage (hwnd, IDC_RULES_BTN, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)config.hbmp_rules);
	SendDlgItemMessage (hwnd, IDC_ALLOW_BTN, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)config.hbmp_allow);
	SendDlgItemMessage (hwnd, IDC_BLOCK_BTN, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)config.hbmp_block);
	SendDlgItemMessage (hwnd, IDC_LATER_BTN, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)config.hbmp_cross);

	_r_ctrl_setbuttonmargins (hwnd, IDC_RULES_BTN);
	_r_ctrl_setbuttonmargins (hwnd, IDC_ALLOW_BTN);
	_r_ctrl_setbuttonmargins (hwnd, IDC_BLOCK_BTN);
	_r_ctrl_setbuttonmargins (hwnd, IDC_LATER_BTN);

	_r_wnd_addstyle (hwnd, IDC_RULES_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
	_r_wnd_addstyle (hwnd, IDC_ALLOW_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
	_r_wnd_addstyle (hwnd, IDC_BLOCK_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
	_r_wnd_addstyle (hwnd, IDC_LATER_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

	InvalidateRect (hwnd, nullptr, TRUE);
}

void DrawGradient (HDC hdc, LPRECT const lprc, COLORREF rgb1, COLORREF rgb2, ULONG mode)
{
	GRADIENT_RECT gradientRect = {0};
	TRIVERTEX triVertext[2] = {0};

	gradientRect.LowerRight = 1;

	triVertext[0].x = lprc->left - 1;
	triVertext[0].y = lprc->top - 1;
	triVertext[0].Red = GetRValue (rgb1) << 8;
	triVertext[0].Green = GetGValue (rgb1) << 8;
	triVertext[0].Blue = GetBValue (rgb1) << 8;

	triVertext[1].x = lprc->right;
	triVertext[1].y = lprc->bottom;
	triVertext[1].Red = GetRValue (rgb2) << 8;
	triVertext[1].Green = GetGValue (rgb2) << 8;
	triVertext[1].Blue = GetBValue (rgb2) << 8;

	GradientFill (hdc, triVertext, _countof (triVertext), &gradientRect, 1, mode);
}

INT_PTR CALLBACK NotificationProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
		case WM_INITDIALOG:
		{
#if !defined(_APP_NO_DARKTHEME)
			_r_wnd_setdarktheme (hwnd);
#endif // !_APP_NO_DARKTHEME

			_app_notifyfontset (hwnd);

			const HWND htip = _r_ctrl_createtip (hwnd);

			if (htip)
			{
				_r_ctrl_settip (htip, hwnd, IDC_FILE_TEXT, LPSTR_TEXTCALLBACK);
				_r_ctrl_settip (htip, hwnd, IDC_RULES_BTN, LPSTR_TEXTCALLBACK);
				_r_ctrl_settip (htip, hwnd, IDC_ALLOW_BTN, LPSTR_TEXTCALLBACK);
				_r_ctrl_settip (htip, hwnd, IDC_BLOCK_BTN, LPSTR_TEXTCALLBACK);
				_r_ctrl_settip (htip, hwnd, IDC_LATER_BTN, LPSTR_TEXTCALLBACK);
			}

			break;
		}

		case WM_NCCREATE:
		{
			_r_wnd_enablenonclientscaling (hwnd);
			break;
		}

		case WM_DPICHANGED:
		{
			_app_notifyfontset (hwnd);
			_app_notifyrefresh (hwnd, false);

			break;
		}

		case WM_CLOSE:
		{
			_app_notifyhide (hwnd);

			SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
			return TRUE;
		}

		case WM_ACTIVATE:
		{
			switch (wparam)
			{
				case WA_ACTIVE:
				case WA_CLICKACTIVE:
				{
					_r_wnd_top (hwnd, true);
					SetActiveWindow (hwnd);
					break;
				}
			}

			break;
		}

		case WM_TIMER:
		{
			if (wparam == NOTIFY_TIMER_SAFETY_ID)
			{
				KillTimer (hwnd, wparam);

				_r_ctrl_enable (hwnd, IDC_RULES_BTN, true);
				_r_ctrl_enable (hwnd, IDC_ALLOW_BTN, true);
				_r_ctrl_enable (hwnd, IDC_BLOCK_BTN, true);
				_r_ctrl_enable (hwnd, IDC_LATER_BTN, true);
			}

			break;
		}

		case WM_SETTINGCHANGE:
		{
			_r_wnd_changesettings (hwnd, wparam, lparam);
			break;
		}

		case WM_ERASEBKGND:
		{
			RECT rc = {0};
			GetClientRect (hwnd, &rc);

			_r_dc_fillrect ((HDC)wparam, &rc, GetSysColor (COLOR_WINDOW));

			SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
			return TRUE;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps = {0};
			HDC hdc = BeginPaint (hwnd, &ps);

			if (hdc)
			{
				RECT rc = {0};
				GetClientRect (hwnd, &rc);

				const INT wnd_width = _R_RECT_WIDTH (&rc);
				const INT wnd_height = _R_RECT_HEIGHT (&rc);

				SetRect (&rc, 0, wnd_height - _r_dc_getdpi (hwnd, _R_SIZE_FOOTERHEIGHT), wnd_width, wnd_height);
				_r_dc_fillrect (hdc, &rc, GetSysColor (COLOR_3DFACE));

				for (INT i = 0; i < wnd_width; i++)
					SetPixelV (hdc, i, rc.top, GetSysColor (COLOR_APPWORKSPACE));

				EndPaint (hwnd, &ps);
			}

			break;
		}

		case WM_CTLCOLORDLG:
		{
			return (INT_PTR)GetSysColorBrush (COLOR_WINDOW);
		}

		case WM_CTLCOLORSTATIC:
		{
			HDC hdc = (HDC)wparam;
			INT ctrl_id = GetDlgCtrlID ((HWND)lparam);

			SetBkMode (hdc, TRANSPARENT); // HACK!!!

			if (ctrl_id == IDC_FILE_TEXT)
				SetTextColor (hdc, GetSysColor (COLOR_HIGHLIGHT));

			else
				SetTextColor (hdc, GetSysColor (COLOR_WINDOWTEXT));

			return (INT_PTR)GetSysColorBrush (COLOR_WINDOW);
		}

		case WM_DRAWITEM:
		{
			LPDRAWITEMSTRUCT drawInfo = (LPDRAWITEMSTRUCT)lparam;

			if (drawInfo->CtlID != IDC_HEADER_ID)
				break;

			const INT wnd_icon_size = _r_dc_getsystemmetrics (hwnd, SM_CXICON);
			const INT wnd_spacing = _r_dc_getdpi (hwnd, 12);

			RECT rcText = {0};
			RECT rcIcon = {0};

			SetRect (&rcText, wnd_spacing, 0, _R_RECT_WIDTH (&drawInfo->rcItem) - (wnd_spacing * 3) - wnd_icon_size, _R_RECT_HEIGHT (&drawInfo->rcItem));
			SetRect (&rcIcon, _R_RECT_WIDTH (&drawInfo->rcItem) - wnd_icon_size - wnd_spacing, (_R_RECT_HEIGHT (&drawInfo->rcItem) / 2) - (wnd_icon_size / 2), wnd_icon_size, wnd_icon_size);

			SetBkMode (drawInfo->hDC, TRANSPARENT);

			// draw background
			DrawGradient (drawInfo->hDC, &drawInfo->rcItem, NOTIFY_GRADIENT_1, NOTIFY_GRADIENT_2, GRADIENT_FILL_RECT_H);

			// draw title text
			WCHAR text[128] = {0};
			_r_str_printf (text, _countof (text), app.LocaleString (IDS_NOTIFY_HEADER, nullptr), APP_NAME);

			COLORREF clr_prev = SetTextColor (drawInfo->hDC, GetSysColor (COLOR_WINDOW));
			DrawTextEx (drawInfo->hDC, text, (INT)_r_str_length (text), &rcText, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOCLIP | DT_NOPREFIX, nullptr);
			SetTextColor (drawInfo->hDC, clr_prev);

			// draw icon
			HICON hicon = (HICON)GetWindowLongPtr (drawInfo->hwndItem, GWLP_USERDATA);

			if (!hicon)
				hicon = config.hicon_large;

			if (hicon)
				DrawIconEx (drawInfo->hDC, rcIcon.left, rcIcon.top, hicon, rcIcon.right, rcIcon.bottom, 0, nullptr, DI_IMAGE | DI_MASK);

			SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
			return TRUE;
		}

		case WM_SETCURSOR:
		{
			const INT ctrl_id = GetDlgCtrlID ((HWND)wparam);

			if (ctrl_id == IDC_FILE_TEXT)
			{
				SetCursor (LoadCursor (nullptr, IDC_HAND));

				SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
				return TRUE;
			}

			break;
		}

		case WM_LBUTTONDOWN:
		{
			PostMessage (hwnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
			break;
		}

		case WM_ENTERSIZEMOVE:
		case WM_EXITSIZEMOVE:
		case WM_CAPTURECHANGED:
		{
			LONG_PTR exstyle = GetWindowLongPtr (hwnd, GWL_EXSTYLE);

			if ((exstyle & WS_EX_LAYERED) == 0)
				SetWindowLongPtr (hwnd, GWL_EXSTYLE, exstyle | WS_EX_LAYERED);

			SetLayeredWindowAttributes (hwnd, 0, (msg == WM_ENTERSIZEMOVE) ? 100 : 255, LWA_ALPHA);
			SetCursor (LoadCursor (nullptr, (msg == WM_ENTERSIZEMOVE) ? IDC_SIZEALL : IDC_ARROW));

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lparam;

			switch (nmlp->code)
			{
				case BCN_DROPDOWN:
				{
					const INT ctrl_id = static_cast<INT>(nmlp->idFrom);

					if (!_r_ctrl_isenabled (hwnd, ctrl_id) || (ctrl_id != IDC_RULES_BTN && ctrl_id != IDC_ALLOW_BTN))
						break;

					const HMENU hsubmenu = CreatePopupMenu ();

					if (nmlp->idFrom == IDC_RULES_BTN)
					{
						AppendMenu (hsubmenu, MF_STRING, IDM_DISABLENOTIFICATIONS, app.LocaleString (IDS_DISABLENOTIFICATIONS, nullptr));

						_app_generate_rulesmenu (hsubmenu, _app_notifyget_id (hwnd, 0));
					}
					else if (nmlp->idFrom == IDC_ALLOW_BTN)
					{
						AppendMenu (hsubmenu, MF_BYPOSITION, IDC_ALLOW_BTN, app.LocaleString (IDS_ACTION_ALLOW, nullptr));
						AppendMenu (hsubmenu, MF_SEPARATOR, 0, nullptr);

						for (size_t i = 0; i < timers.size (); i++)
							AppendMenu (hsubmenu, MF_BYPOSITION, UINT_PTR (IDX_TIMER + i), _r_fmt_interval (timers.at (i) + 1, 1));

						CheckMenuRadioItem (hsubmenu, IDC_ALLOW_BTN, IDC_ALLOW_BTN, IDC_ALLOW_BTN, MF_BYCOMMAND);
					}

					RECT buttonRect = {0};

					GetClientRect (nmlp->hwndFrom, &buttonRect);
					ClientToScreen (nmlp->hwndFrom, (LPPOINT)&buttonRect);

					_r_wnd_adjustwindowrect (nmlp->hwndFrom, &buttonRect);

					TrackPopupMenuEx (hsubmenu, TPM_RIGHTBUTTON | TPM_LEFTBUTTON, buttonRect.left, buttonRect.top, hwnd, nullptr);

					DestroyMenu (hsubmenu);

					break;
				}

				case TTN_GETDISPINFO:
				{
					LPNMTTDISPINFO lpnmdi = (LPNMTTDISPINFO)lparam;

					if ((lpnmdi->uFlags & TTF_IDISHWND) == 0)
						break;

					WCHAR buffer[1024] = {0};
					const INT ctrl_id = GetDlgCtrlID ((HWND)lpnmdi->hdr.idFrom);

					if (ctrl_id == IDC_FILE_TEXT)
					{
						PR_OBJECT ptr_log_object = _app_notifyget_obj (_app_notifyget_id (hwnd, 0));

						if (ptr_log_object)
						{
							PITEM_LOG ptr_log = (PITEM_LOG)ptr_log_object->pdata;

							if (ptr_log && ptr_log->app_hash)
								_r_str_copy (buffer, _countof (buffer), _app_gettooltip (IDC_APPS_PROFILE, ptr_log->app_hash));

							_r_obj_dereference (ptr_log_object);
						}
					}
					else if (ctrl_id == IDC_RULES_BTN)
					{
						_r_str_copy (buffer, _countof (buffer), app.LocaleString (IDS_NOTIFY_TOOLTIP, nullptr));
					}
					else if (ctrl_id == IDC_ALLOW_BTN)
					{
						_r_str_copy (buffer, _countof (buffer), app.LocaleString (IDS_ACTION_ALLOW_HINT, nullptr));
					}
					else if (ctrl_id == IDC_BLOCK_BTN)
					{
						_r_str_copy (buffer, _countof (buffer), app.LocaleString (IDS_ACTION_BLOCK_HINT, nullptr));
					}
					else if (ctrl_id == IDC_LATER_BTN)
					{
						_r_str_copy (buffer, _countof (buffer), app.LocaleString (IDS_ACTION_LATER_HINT, nullptr));
					}
					else
					{
						_r_str_copy (buffer, _countof (buffer), _r_ctrl_gettext (hwnd, ctrl_id));
					}

					if (!_r_str_isempty (buffer))
						lpnmdi->lpszText = buffer;

					break;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			const INT ctrl_id = LOWORD (wparam);

			if ((LOWORD (wparam) >= IDX_RULES_SPECIAL && LOWORD (wparam) <= INT (IDX_RULES_SPECIAL + rules_arr.size ())))
			{
				const size_t rule_idx = (LOWORD (wparam) - IDX_RULES_SPECIAL);
				PR_OBJECT ptr_rule_object = _app_getrulebyid (rule_idx);

				if (!ptr_rule_object)
					return FALSE;

				const PITEM_RULE ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

				if (ptr_rule)
				{
					PR_OBJECT ptr_log_object = _app_notifyget_obj (_app_notifyget_id (hwnd, 0));

					if (ptr_log_object)
					{
						PITEM_LOG ptr_log = (PITEM_LOG)ptr_log_object->pdata;

						if (ptr_log)
						{
							if (ptr_log->app_hash && !(ptr_rule->is_forservices && (ptr_log->app_hash == config.ntoskrnl_hash || ptr_log->app_hash == config.svchost_hash)))
							{
								PR_OBJECT ptr_app_object = _app_getappitem (ptr_log->app_hash);

								if (ptr_app_object)
								{
									PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

									if (ptr_app)
									{
										_app_freenotify (ptr_log->app_hash, ptr_app);

										const bool is_remove = ptr_rule->is_enabled && (ptr_rule->apps.find (ptr_log->app_hash) != ptr_rule->apps.end ());

										if (is_remove)
										{
											ptr_rule->apps.erase (ptr_log->app_hash);

											if (ptr_rule->apps.empty ())
												_app_ruleenable (ptr_rule, false);
										}
										else
										{
											ptr_rule->apps[ptr_log->app_hash] = true;
											_app_ruleenable (ptr_rule, true);
										}

										const INT listview_id = _app_gettab_id (app.GetHWND ());
										const INT app_listview_id = _app_getlistview_id (ptr_app->type);
										const INT rule_listview_id = _app_getlistview_id (ptr_rule->type);

										{
											const INT item_pos = _app_getposition (app.GetHWND (), app_listview_id, ptr_log->app_hash);

											if (item_pos != INVALID_INT)
											{
												_r_fastlock_acquireshared (&lock_checkbox);
												_app_setappiteminfo (app.GetHWND (), app_listview_id, item_pos, ptr_log->app_hash, ptr_app);
												_r_fastlock_releaseshared (&lock_checkbox);
											}
										}

										{
											const INT item_pos = _app_getposition (app.GetHWND (), rule_listview_id, rule_idx);

											if (item_pos != INVALID_INT)
											{
												_r_fastlock_acquireshared (&lock_checkbox);
												_app_setruleiteminfo (app.GetHWND (), rule_listview_id, item_pos, ptr_rule, false);
												_r_fastlock_releaseshared (&lock_checkbox);
											}
										}

										OBJECTS_VEC rules;
										rules.push_back (ptr_rule_object);

										_wfp_create4filters (_wfp_getenginehandle (), rules, __LINE__);

										if (listview_id == app_listview_id || listview_id == rule_listview_id)
										{
											_app_listviewsort (app.GetHWND (), listview_id);
											_r_listview_redraw (app.GetHWND (), listview_id);
										}

										_app_refreshstatus (app.GetHWND (), listview_id);
										_app_profile_save ();
									}

									_r_obj_dereference (ptr_app_object);
								}
							}
						}

						_r_obj_dereference (ptr_log_object);
					}
				}

				_r_obj_dereference (ptr_rule_object);

				return FALSE;
			}
			else if ((ctrl_id >= IDX_TIMER && ctrl_id <= INT (IDX_TIMER + timers.size ())))
			{
				if (!_r_ctrl_isenabled (hwnd, IDC_ALLOW_BTN))
					return FALSE;

				const size_t timer_idx = (ctrl_id - IDX_TIMER);

				_app_notifycommand (hwnd, IDC_ALLOW_BTN, timers.at (timer_idx));

				return FALSE;
			}

			switch (ctrl_id)
			{
				case IDCANCEL: // process Esc key
				{
					_app_notifyhide (hwnd);
					break;
				}

				case IDC_FILE_TEXT:
				{
					PR_OBJECT ptr_log_object = _app_notifyget_obj (_app_notifyget_id (hwnd, 0));

					if (ptr_log_object)
					{
						PITEM_LOG ptr_log = (PITEM_LOG)ptr_log_object->pdata;

						if (ptr_log)
						{
							PR_OBJECT ptr_app_object = _app_getappitem (ptr_log->app_hash);

							if (ptr_app_object)
							{
								PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

								if (ptr_app)
								{
									const INT listview_id = _app_getlistview_id (ptr_app->type);

									if (listview_id)
										_app_showitem (app.GetHWND (), listview_id, _app_getposition (app.GetHWND (), listview_id, ptr_log->app_hash));

									_r_wnd_toggle (app.GetHWND (), true);
								}

								_r_obj_dereference (ptr_app_object);
							}
						}

						_r_obj_dereference (ptr_log_object);
					}

					break;
				}

				case IDC_RULES_BTN:
				{
					if (_r_ctrl_isenabled (hwnd, ctrl_id))
					{
						// HACK!!!
						NMHDR hdr = {0};

						hdr.code = BCN_DROPDOWN;
						hdr.idFrom = ctrl_id;
						hdr.hwndFrom = GetDlgItem (hwnd, ctrl_id);

						SendMessage (hwnd, WM_NOTIFY, TRUE, (LPARAM)&hdr);
					}

					break;
				}

				case IDC_ALLOW_BTN:
				case IDC_BLOCK_BTN:
				case IDC_LATER_BTN:
				{
					if (_r_ctrl_isenabled (hwnd, ctrl_id))
						_app_notifycommand (hwnd, ctrl_id, 0);

					break;
				}

				case IDM_DISABLENOTIFICATIONS:
				{
					_app_notifycommand (hwnd, ctrl_id, 0);
					break;
				}

				case IDM_EDITRULES:
				{
					_r_wnd_toggle (app.GetHWND (), true);
					_app_settab_id (app.GetHWND (), _app_getlistview_id (DataRuleCustom));

					break;
				}

				case IDM_OPENRULESEDITOR:
				{
					PITEM_RULE ptr_rule = new ITEM_RULE;
					PR_OBJECT ptr_rule_object = _r_obj_allocate (ptr_rule, &_app_dereferencerule);

					size_t app_hash = 0;
					PR_OBJECT ptr_log_object = _app_notifyget_obj (_app_notifyget_id (hwnd, 0));

					if (ptr_log_object)
					{
						PITEM_LOG ptr_log = (PITEM_LOG)ptr_log_object->pdata;

						if (ptr_log)
						{
							app_hash = ptr_log->app_hash;

							ptr_rule->apps[app_hash] = true;
							ptr_rule->protocol = ptr_log->protocol;
							ptr_rule->dir = ptr_log->direction;

							PR_OBJECT ptr_app_object = _app_getappitem (app_hash);

							if (ptr_app_object)
							{
								PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

								if (ptr_app)
									_app_freenotify (app_hash, ptr_app);

								_r_obj_dereference (ptr_app_object);
							}

							LPWSTR prule = nullptr;

							if (_app_formataddress (ptr_log->af, 0, &ptr_log->remote_addr, ptr_log->remote_port, &prule, FMTADDR_AS_RULE))
							{
								size_t len = _r_str_length (prule);

								_r_str_alloc (&ptr_rule->pname, len, prule);
								_r_str_alloc (&ptr_rule->prule_remote, len, prule);
							}

							SAFE_DELETE_ARRAY (prule);
						}

						_r_obj_dereference (ptr_log_object);
					}
					else
					{
						_r_obj_dereference (ptr_rule_object);
						break;
					}

					ptr_rule->type = DataRuleCustom;
					ptr_rule->is_block = false;

					_app_ruleenable (ptr_rule, true);

					if (DialogBoxParam (nullptr, MAKEINTRESOURCE (IDD_EDITOR), app.GetHWND (), &EditorProc, (LPARAM)ptr_rule_object))
					{
						_r_fastlock_acquireshared (&lock_access);

						const size_t rule_idx = rules_arr.size ();
						rules_arr.push_back (ptr_rule_object);

						_r_fastlock_releaseshared (&lock_access);

						const INT listview_id = _app_gettab_id (app.GetHWND ());

						// set rule information
						{
							const INT rules_listview_id = _app_getlistview_id (ptr_rule->type);

							if (rules_listview_id)
							{
								const INT new_item = _r_listview_getitemcount (hwnd, rules_listview_id);

								_r_fastlock_acquireshared (&lock_checkbox);

								_r_listview_additem (app.GetHWND (), rules_listview_id, new_item, 0, ptr_rule->pname, _app_getruleicon (ptr_rule), _app_getrulegroup (ptr_rule), rule_idx);
								_app_setruleiteminfo (app.GetHWND (), rules_listview_id, new_item, ptr_rule, true);

								_r_fastlock_releaseshared (&lock_checkbox);

								if (rules_listview_id == listview_id)
									_app_listviewsort (app.GetHWND (), listview_id);
							}
						}

						// set app information
						{
							PR_OBJECT ptr_app_object = _app_getappitem (app_hash);

							if (ptr_app_object)
							{
								PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

								if (ptr_app)
								{
									const INT app_listview_id = _app_getlistview_id (ptr_app->type);

									if (app_listview_id)
									{
										const INT item_pos = _app_getposition (app.GetHWND (), app_listview_id, app_hash);

										if (item_pos != INVALID_INT)
										{
											_r_fastlock_acquireshared (&lock_checkbox);
											_app_setappiteminfo (app.GetHWND (), app_listview_id, item_pos, app_hash, ptr_app);
											_r_fastlock_releaseshared (&lock_checkbox);
										}

										if (app_listview_id == listview_id)
											_app_listviewsort (app.GetHWND (), listview_id);
									}
								}

								_r_obj_dereference (ptr_app_object);
							}
						}

						_app_refreshstatus (app.GetHWND (), listview_id);
						_app_profile_save ();
					}
					else
					{
						_r_obj_dereference (ptr_rule_object);
					}

					break;
				}

				case IDM_COPY: // ctrl+c
				case IDM_SELECT_ALL: // ctrl+a
				{
					const HWND hedit = GetFocus ();

					if (hedit)
					{
						WCHAR class_name[64] = {0};

						if (GetClassName (hedit, class_name, _countof (class_name)) > 0)
						{
							if (_r_str_compare (class_name, WC_EDIT) == 0)
							{
								// edit control hotkey for "ctrl+c" (issue #597)
								if (ctrl_id == IDM_COPY)
									SendMessage (hedit, WM_COPY, 0, 0);

								// edit control hotkey for "ctrl+a"
								else if (ctrl_id == IDM_SELECT_ALL)
									SendMessage (hedit, EM_SETSEL, 0, (LPARAM)-1);
							}
						}
					}

					break;
				}
			}

			break;
		}
	}

	return FALSE;
}
