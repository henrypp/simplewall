// simplewall
// Copyright (c) 2016-2021 Henry++

#include "global.h"

HFONT hfont_title = NULL;
HFONT hfont_link = NULL;
HFONT hfont_text = NULL;

VOID _app_notifycreatewindow ()
{
	config.hnotification = CreateDialog (NULL, MAKEINTRESOURCE (IDD_NOTIFICATION), NULL, &NotificationProc);
}

BOOLEAN _app_notifycommand (_In_ HWND hwnd, _In_ INT button_id, _In_ LONG64 seconds)
{
	SIZE_T app_hash = _app_notifyget_id (hwnd, FALSE);
	PITEM_APP ptr_app = _app_getappitem (app_hash);

	if (!ptr_app)
		return FALSE;

	_app_freenotify (ptr_app);

	PR_LIST rules = _r_obj_createlist (NULL);

	INT listview_id = _app_getlistview_id (ptr_app->type);
	INT item_pos = _app_getposition (_r_app_gethwnd (), listview_id, app_hash);

	if (button_id == IDC_ALLOW_BTN || button_id == IDC_BLOCK_BTN)
	{
		ptr_app->is_enabled = (button_id == IDC_ALLOW_BTN);
		ptr_app->is_silent = (button_id == IDC_BLOCK_BTN);

		if (ptr_app->is_enabled && seconds)
		{
			_app_timer_set (_r_app_gethwnd (), ptr_app, seconds);
		}
		else
		{
			if (item_pos != -1)
			{
				_r_spinlock_acquireshared (&lock_checkbox);
				_app_setappiteminfo (_r_app_gethwnd (), listview_id, item_pos, ptr_app);
				_r_spinlock_releaseshared (&lock_checkbox);
			}
		}

		_r_obj_addlistitem (rules, ptr_app);
	}
	else if (button_id == IDC_LATER_BTN)
	{
		// TODO: do somethig!!!
	}
	else if (button_id == IDM_DISABLENOTIFICATIONS)
	{
		ptr_app->is_silent = TRUE;
	}

	ptr_app->last_notify = _r_unixtime_now ();

	if (!_r_obj_islistempty (rules))
	{
		if (_wfp_isfiltersinstalled ())
		{
			HANDLE hengine = _wfp_getenginehandle ();

			if (hengine)
				_wfp_create3filters (hengine, rules, __LINE__, FALSE);
		}
	}

	_r_obj_dereference (rules);

	_app_refreshstatus (_r_app_gethwnd (), listview_id);
	_app_profile_save ();

	if (listview_id && (INT)_r_tab_getitemlparam (_r_app_gethwnd (), IDC_TAB, -1) == listview_id)
	{
		_app_listviewsort (_r_app_gethwnd (), listview_id, -1, FALSE);
		_r_listview_redraw (_r_app_gethwnd (), listview_id, -1);
	}

	return TRUE;
}

BOOLEAN _app_notifyadd (_In_ HWND hwnd, _In_ PITEM_LOG ptr_log, _In_ PITEM_APP ptr_app)
{
	// check for last display time
	LONG64 current_time = _r_unixtime_now ();
	LONG64 notification_timeout = _r_config_getlong64 (L"NotificationsTimeout", NOTIFY_TIMEOUT_DEFAULT);

	if (notification_timeout && ((current_time - ptr_app->last_notify) <= notification_timeout))
		return FALSE;

	ptr_app->last_notify = current_time;

	if (!ptr_log->hicon)
		_app_getappicon (ptr_app, FALSE, NULL, &ptr_log->hicon);

	_r_obj_movereference (&ptr_app->pnotification, _r_obj_reference (ptr_log));

	if (_r_config_getboolean (L"IsNotificationsSound", TRUE))
	{
		if (!_r_config_getboolean (L"IsNotificationsFullscreenSilentMode", TRUE) || !_r_wnd_isfullscreenmode ())
			_app_notifyplaysound ();
	}

	if (!_r_wnd_isundercursor (hwnd))
		_app_notifyshow (hwnd, ptr_log, TRUE, TRUE);

	return TRUE;
}

VOID _app_freenotify (_Inout_ PITEM_APP ptr_app)
{
	HWND hwnd = config.hnotification;

	SAFE_DELETE_REFERENCE (ptr_app->pnotification);

	if (_app_notifyget_id (hwnd, FALSE) == ptr_app->app_hash)
		_app_notifyget_id (hwnd, TRUE);

	_app_notifyrefresh (hwnd, TRUE);
}

SIZE_T _app_notifyget_id (_In_ HWND hwnd, _In_ BOOLEAN is_nearest)
{
	SIZE_T app_hash_current = (SIZE_T)GetWindowLongPtr (hwnd, GWLP_USERDATA);

	if (is_nearest)
	{
		PITEM_APP ptr_app;
		SIZE_T enum_key = 0;

		_r_spinlock_acquireshared (&lock_apps);

		while (_r_obj_enumhashtable (apps, &ptr_app, NULL, &enum_key))
		{
			if (ptr_app->app_hash == app_hash_current) // exclude current app from enumeration
				continue;

			if (ptr_app->pnotification)
			{
				_r_spinlock_releaseshared (&lock_apps);

				SetWindowLongPtr (hwnd, GWLP_USERDATA, (LONG_PTR)ptr_app->app_hash);

				return ptr_app->app_hash;
			}
		}

		_r_spinlock_releaseshared (&lock_apps);

		SetWindowLongPtr (hwnd, GWLP_USERDATA, 0);
		return 0;
	}

	return app_hash_current;
}

PITEM_LOG _app_notifyget_obj (_In_ SIZE_T app_hash)
{
	PITEM_APP ptr_app = _app_getappitem (app_hash);

	if (ptr_app)
	{
		return _r_obj_referencesafe (ptr_app->pnotification);
	}

	return NULL;
}

BOOLEAN _app_notifyshow (_In_ HWND hwnd, _In_ PITEM_LOG ptr_log, _In_ BOOLEAN is_forced, _In_ BOOLEAN is_safety)
{
	if (!_r_config_getboolean (L"IsNotificationsEnabled", TRUE))
		return FALSE;

	PITEM_APP ptr_app = _app_getappitem (ptr_log->app_hash);

	if (!ptr_app)
		return FALSE;

	WCHAR window_title[128];
	WCHAR date_string[128];
	PR_STRING signature_string = NULL;
	PR_STRING remote_address_string;
	PR_STRING remote_port_string;
	PR_STRING direction_string;
	PR_STRING localized_string = NULL;
	LPCWSTR empty_string;

	if (_r_config_getboolean (L"IsCertificatesEnabled", FALSE))
	{
		if (ptr_app->is_signed)
		{
			PR_STRING signature_cache_string = _app_getsignatureinfo (ptr_app);

			if (signature_cache_string)
			{
				_r_obj_movereference (&signature_string, signature_cache_string);
			}
			else
			{
				signature_string = _r_obj_createstring (_r_locale_getstring (IDS_SIGN_UNSIGNED));
			}
		}
		else
		{
			signature_string = _r_obj_createstring (_r_locale_getstring (IDS_SIGN_UNSIGNED));
		}
	}

	_r_str_printf (window_title, RTL_NUMBER_OF (window_title), L"%s - " APP_NAME, _r_locale_getstring (IDS_NOTIFY_TITLE));

	SetWindowText (hwnd, window_title);

	SetWindowLongPtr (hwnd, GWLP_USERDATA, (LONG_PTR)ptr_log->app_hash);
	SetWindowLongPtr (GetDlgItem (hwnd, IDC_HEADER_ID), GWLP_USERDATA, (LONG_PTR)ptr_log->hicon);

	// print table text
	remote_address_string = _app_formataddress (ptr_log->af, 0, &ptr_log->remote_addr, 0, FMTADDR_RESOLVE_HOST);
	remote_port_string = _app_formatport (ptr_log->remote_port, ptr_log->protocol, FALSE);
	direction_string = _app_getdirectionname (ptr_log->direction, ptr_log->is_loopback, TRUE);
	empty_string = _r_locale_getstring (IDS_STATUS_EMPTY);

	_r_format_unixtimeex (date_string, RTL_NUMBER_OF (date_string), ptr_log->timestamp, FDTF_SHORTDATE | FDTF_LONGTIME);

	_r_obj_movereference (&localized_string, _r_format_string (L"%s:", _r_locale_getstring (IDS_NAME)));
	_r_ctrl_settabletext (hwnd, IDC_FILE_ID, _r_obj_getstring (localized_string), IDC_FILE_TEXT, _app_getdisplayname (ptr_app, TRUE));

	_r_obj_movereference (&localized_string, _r_format_string (L"%s:", _r_locale_getstring (IDS_SIGNATURE)));
	_r_ctrl_settabletext (hwnd, IDC_SIGNATURE_ID, _r_obj_getstring (localized_string), IDC_SIGNATURE_TEXT, _r_obj_getstringordefault (signature_string, empty_string));

	_r_obj_movereference (&localized_string, _r_format_string (L"%s:", _r_locale_getstring (IDS_ADDRESS)));
	_r_ctrl_settabletext (hwnd, IDC_ADDRESS_ID, _r_obj_getstring (localized_string), IDC_ADDRESS_TEXT, _r_obj_getstringordefault (remote_address_string, empty_string));

	_r_obj_movereference (&localized_string, _r_format_string (L"%s:", _r_locale_getstring (IDS_PORT)));
	_r_ctrl_settabletext (hwnd, IDC_PORT_ID, _r_obj_getstring (localized_string), IDC_PORT_TEXT, _r_obj_getstringordefault (remote_port_string, empty_string));

	_r_obj_movereference (&localized_string, _r_format_string (L"%s:", _r_locale_getstring (IDS_DIRECTION)));
	_r_ctrl_settabletext (hwnd, IDC_DIRECTION_ID, _r_obj_getstring (localized_string), IDC_DIRECTION_TEXT, _r_obj_getstringordefault (direction_string, empty_string));

	_r_obj_movereference (&localized_string, _r_format_string (L"%s:", _r_locale_getstring (IDS_FILTER)));
	_r_ctrl_settabletext (hwnd, IDC_FILTER_ID, _r_obj_getstring (localized_string), IDC_FILTER_TEXT, _r_obj_getstringordefault (ptr_log->filter_name, empty_string));

	_r_obj_movereference (&localized_string, _r_format_string (L"%s:", _r_locale_getstring (IDS_DATE)));
	_r_ctrl_settabletext (hwnd, IDC_DATE_ID, _r_obj_getstring (localized_string), IDC_DATE_TEXT, date_string);

	_r_ctrl_settext (hwnd, IDC_RULES_BTN, _r_locale_getstring (IDS_TRAY_RULES));
	_r_ctrl_settext (hwnd, IDC_ALLOW_BTN, _r_locale_getstring (IDS_ACTION_ALLOW));
	_r_ctrl_settext (hwnd, IDC_BLOCK_BTN, _r_locale_getstring (IDS_ACTION_BLOCK));

	_r_ctrl_enable (hwnd, IDC_RULES_BTN, !is_safety);
	_r_ctrl_enable (hwnd, IDC_ALLOW_BTN, !is_safety);
	_r_ctrl_enable (hwnd, IDC_BLOCK_BTN, !is_safety);
	_r_ctrl_enable (hwnd, IDC_LATER_BTN, !is_safety);

	if (is_safety)
	{
		SetTimer (hwnd, NOTIFY_TIMER_SAFETY_ID, NOTIFY_TIMER_SAFETY_TIMEOUT, NULL);
	}
	else
	{
		KillTimer (hwnd, NOTIFY_TIMER_SAFETY_ID);
	}

	_app_notifysetpos (hwnd, FALSE);

	// prevent fullscreen apps lose focus
	BOOLEAN is_fullscreenmode = _r_wnd_isfullscreenmode ();

	if (is_forced && is_fullscreenmode)
		is_forced = FALSE;

	InvalidateRect (GetDlgItem (hwnd, IDC_HEADER_ID), NULL, TRUE);
	InvalidateRect (GetDlgItem (hwnd, IDC_FILE_TEXT), NULL, TRUE);
	InvalidateRect (hwnd, NULL, TRUE);

	_r_wnd_top (hwnd, !is_fullscreenmode);

	ShowWindow (hwnd, is_forced ? SW_SHOW : SW_SHOWNA);

	SAFE_DELETE_REFERENCE (signature_string);
	SAFE_DELETE_REFERENCE (remote_address_string);
	SAFE_DELETE_REFERENCE (remote_port_string);
	SAFE_DELETE_REFERENCE (direction_string);
	SAFE_DELETE_REFERENCE (localized_string);

	return TRUE;
}

VOID _app_notifyhide (_In_ HWND hwnd)
{
	ShowWindow (hwnd, SW_HIDE);
}

// Play notification sound even if system have "nosound" mode
VOID _app_notifyplaysound ()
{
	static PR_STRING path = NULL;

	if (_r_obj_isstringempty (path) || !_r_fs_exists (path->buffer))
	{
#define NOTIFY_SOUND_NAME L"MailBeep"

		HKEY hkey;

		if (RegOpenKeyEx (HKEY_CURRENT_USER, L"AppEvents\\Schemes\\Apps\\.Default\\" NOTIFY_SOUND_NAME L"\\.Default", 0, KEY_READ, &hkey) == ERROR_SUCCESS)
		{
			_r_obj_movereference (&path, _r_reg_querystring (hkey, NULL, NULL));

			if (path)
			{
				PR_STRING expanded_string = _r_str_expandenvironmentstring (path->buffer);

				if (expanded_string)
					_r_obj_movereference (&path, expanded_string);
			}

			RegCloseKey (hkey);
		}
	}

	if (_r_obj_isstringempty (path) || !PlaySound (path->buffer, NULL, SND_ASYNC | SND_NODEFAULT | SND_NOWAIT | SND_FILENAME | SND_SENTRY))
		PlaySound (NOTIFY_SOUND_NAME, NULL, SND_ASYNC | SND_NODEFAULT | SND_NOWAIT | SND_SENTRY);
}

VOID _app_notifyrefresh (_In_ HWND hwnd, _In_ BOOLEAN is_safety)
{
	if (!IsWindowVisible (hwnd) || !_r_config_getboolean (L"IsNotificationsEnabled", TRUE))
	{
		_app_notifyhide (hwnd);
		return;
	}

	PITEM_LOG ptr_log = _app_notifyget_obj (_app_notifyget_id (hwnd, FALSE));

	if (!ptr_log)
	{
		_app_notifyhide (hwnd);
		return;
	}

	_app_notifyshow (hwnd, ptr_log, TRUE, is_safety);

	_r_obj_dereference (ptr_log);
}

VOID _app_notifysetpos (_In_ HWND hwnd, _In_ BOOLEAN is_forced)
{
	R_RECTANGLE window_rect;
	RECT desktop_rect;
	UINT swp_flags;
	BOOLEAN is_intray;

	swp_flags = SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER;

	if (!is_forced && IsWindowVisible (hwnd))
	{
		if (_r_wnd_getposition (hwnd, &window_rect))
		{
			_r_wnd_adjustworkingarea (NULL, &window_rect);
			SetWindowPos (hwnd, NULL, window_rect.left, window_rect.top, 0, 0, swp_flags);

			return;
		}
	}

	is_intray = _r_config_getboolean (L"IsNotificationsOnTray", FALSE);

	if (is_intray)
	{
		if (_r_wnd_getposition (hwnd, &window_rect))
		{
			if (SystemParametersInfo (SPI_GETWORKAREA, 0, &desktop_rect, 0))
			{
				APPBARDATA abd = {0};
				abd.cbSize = sizeof (abd);

				if (SHAppBarMessage (ABM_GETTASKBARPOS, &abd))
				{
					INT border_x = _r_dc_getsystemmetrics (hwnd, SM_CXBORDER);

					if (abd.uEdge == ABE_LEFT)
					{
						window_rect.left = abd.rc.right + border_x;
						window_rect.top = (desktop_rect.bottom - window_rect.height) - border_x;
					}
					else if (abd.uEdge == ABE_TOP)
					{
						window_rect.left = (desktop_rect.right - window_rect.width) - border_x;
						window_rect.top = abd.rc.bottom + border_x;
					}
					else if (abd.uEdge == ABE_RIGHT)
					{
						window_rect.left = (desktop_rect.right - window_rect.width) - border_x;
						window_rect.top = (desktop_rect.bottom - window_rect.height) - border_x;
					}
					else/* if (abd.uEdge == ABE_BOTTOM)*/
					{
						window_rect.left = (desktop_rect.right - window_rect.width) - border_x;
						window_rect.top = (desktop_rect.bottom - window_rect.height) - border_x;
					}

					SetWindowPos (hwnd, NULL, window_rect.left, window_rect.top, 0, 0, swp_flags);
					return;
				}
			}
		}
	}

	_r_wnd_center (hwnd, NULL); // display window on center (depends on error, config etc...)
}

HFONT _app_notifyfontinit (_In_ HWND hwnd, _In_ PLOGFONT plf, _In_ LONG height, _In_ LONG weight, _In_ BOOLEAN is_underline)
{
	if (height)
		plf->lfHeight = _r_dc_fontsizetoheight (hwnd, height);

	plf->lfWeight = weight;
	plf->lfUnderline = is_underline;

	plf->lfCharSet = DEFAULT_CHARSET;
	plf->lfQuality = DEFAULT_QUALITY;

	return CreateFontIndirect (plf);
}

VOID _app_notifyfontset (_In_ HWND hwnd)
{
	_r_wnd_seticon (hwnd,
					_r_app_getsharedimage (NULL, SIH_EXCLAMATION, _r_dc_getsystemmetrics (hwnd, SM_CXSMICON)),
					_r_app_getsharedimage (NULL, SIH_EXCLAMATION, _r_dc_getsystemmetrics (hwnd, SM_CXICON))
	);

	INT title_font_height = 12;
	INT text_font_height = 9;

	NONCLIENTMETRICS ncm = {0};
	ncm.cbSize = sizeof (ncm);

	if (SystemParametersInfo (SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0))
	{
		SAFE_DELETE_OBJECT (hfont_title);
		SAFE_DELETE_OBJECT (hfont_link);
		SAFE_DELETE_OBJECT (hfont_text);

		hfont_title = _app_notifyfontinit (hwnd, &ncm.lfCaptionFont, title_font_height, FW_NORMAL, FALSE);
		hfont_link = _app_notifyfontinit (hwnd, &ncm.lfMessageFont, text_font_height, FW_NORMAL, TRUE);
		hfont_text = _app_notifyfontinit (hwnd, &ncm.lfMessageFont, text_font_height, FW_NORMAL, FALSE);

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

	BOOLEAN is_classic = _r_app_isclassicui ();

	_r_wnd_addstyle (hwnd, IDC_RULES_BTN, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
	_r_wnd_addstyle (hwnd, IDC_ALLOW_BTN, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
	_r_wnd_addstyle (hwnd, IDC_BLOCK_BTN, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
	_r_wnd_addstyle (hwnd, IDC_LATER_BTN, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

	InvalidateRect (hwnd, NULL, TRUE);
}

VOID _app_notifydrawgradient (_In_ HDC hdc, _In_ LPRECT lprc, _In_ COLORREF rgb1, _In_ COLORREF rgb2, _In_ ULONG mode)
{
	GRADIENT_RECT gradient_rect = {0};
	TRIVERTEX trivertx[2] = {0};

	gradient_rect.LowerRight = 1;

	trivertx[0].x = lprc->left - 1;
	trivertx[0].y = lprc->top - 1;
	trivertx[0].Red = GetRValue (rgb1) << 8;
	trivertx[0].Green = GetGValue (rgb1) << 8;
	trivertx[0].Blue = GetBValue (rgb1) << 8;

	trivertx[1].x = lprc->right;
	trivertx[1].y = lprc->bottom;
	trivertx[1].Red = GetRValue (rgb2) << 8;
	trivertx[1].Green = GetGValue (rgb2) << 8;
	trivertx[1].Blue = GetBValue (rgb2) << 8;

	GradientFill (hdc, trivertx, RTL_NUMBER_OF (trivertx), &gradient_rect, 1, mode);
}

INT_PTR CALLBACK NotificationProc (_In_ HWND hwnd, _In_ UINT msg, _In_ WPARAM wparam, _In_ LPARAM lparam)
{
	switch (msg)
	{
		case WM_INITDIALOG:
		{
			_app_notifyfontset (hwnd);

			HWND htip = _r_ctrl_createtip (hwnd);

			if (htip)
			{
				_r_ctrl_settiptext (htip, hwnd, IDC_FILE_TEXT, LPSTR_TEXTCALLBACK);
				_r_ctrl_settiptext (htip, hwnd, IDC_RULES_BTN, LPSTR_TEXTCALLBACK);
				_r_ctrl_settiptext (htip, hwnd, IDC_ALLOW_BTN, LPSTR_TEXTCALLBACK);
				_r_ctrl_settiptext (htip, hwnd, IDC_BLOCK_BTN, LPSTR_TEXTCALLBACK);
				_r_ctrl_settiptext (htip, hwnd, IDC_LATER_BTN, LPSTR_TEXTCALLBACK);
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
			_app_notifyrefresh (hwnd, FALSE);

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
					_r_wnd_top (hwnd, TRUE);
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

				_r_ctrl_enable (hwnd, IDC_RULES_BTN, TRUE);
				_r_ctrl_enable (hwnd, IDC_ALLOW_BTN, TRUE);
				_r_ctrl_enable (hwnd, IDC_BLOCK_BTN, TRUE);
				_r_ctrl_enable (hwnd, IDC_LATER_BTN, TRUE);
			}

			break;
		}

		case WM_SETTINGCHANGE:
		{
			_r_wnd_changesettings (hwnd, wparam, lparam);
			break;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			RECT rect;

			HDC hdc = BeginPaint (hwnd, &ps);

			if (hdc)
			{
				if (GetClientRect (hwnd, &rect))
				{
					LONG wnd_width = rect.right;
					LONG wnd_height = rect.bottom;
					LONG footer_height = _r_dc_getdpi (hwnd, PR_SIZE_FOOTERHEIGHT);

					SetRect (&rect, 0, wnd_height - footer_height, wnd_width, wnd_height);

					_r_dc_fillrect (hdc, &rect, GetSysColor (COLOR_BTNFACE));

					for (INT i = 0; i < wnd_width; i++)
						SetPixelV (hdc, i, rect.top, GetSysColor (COLOR_APPWORKSPACE));
				}

				EndPaint (hwnd, &ps);
			}

			break;
		}

		case WM_CTLCOLOREDIT:
		case WM_CTLCOLORDLG:
		{
			return (INT_PTR)GetSysColorBrush (COLOR_WINDOW);
		}

		case WM_CTLCOLORSTATIC:
		{
			HDC hdc = (HDC)wparam;
			INT ctrl_id = GetDlgCtrlID ((HWND)lparam);

			SetBkMode (hdc, TRANSPARENT); // HACK!!!
			SetTextColor (hdc, GetSysColor ((ctrl_id == IDC_FILE_TEXT) ? COLOR_HIGHLIGHT : COLOR_WINDOWTEXT));

			return (INT_PTR)GetSysColorBrush (COLOR_WINDOW);
		}

		case WM_DRAWITEM:
		{
			LPDRAWITEMSTRUCT draw_info = (LPDRAWITEMSTRUCT)lparam;

			if (draw_info->CtlID != IDC_HEADER_ID)
				break;

			INT wnd_icon_size = _r_dc_getsystemmetrics (hwnd, SM_CXICON);
			INT wnd_spacing = _r_dc_getdpi (hwnd, 12);

			RECT text_rect;
			RECT icon_rect;

			SetRect (&text_rect, wnd_spacing, 0, _r_calc_rectwidth (&draw_info->rcItem) - (wnd_spacing * 3) - wnd_icon_size, _r_calc_rectheight (&draw_info->rcItem));
			SetRect (&icon_rect, _r_calc_rectwidth (&draw_info->rcItem) - wnd_icon_size - wnd_spacing, (_r_calc_rectheight (&draw_info->rcItem) / 2) - (wnd_icon_size / 2), wnd_icon_size, wnd_icon_size);

			SetBkMode (draw_info->hDC, TRANSPARENT);

			// draw background
			_app_notifydrawgradient (draw_info->hDC, &draw_info->rcItem, _r_config_getulong (L"NotificationBackground1", NOTIFY_GRADIENT_1), _r_config_getulong (L"NotificationBackground2", NOTIFY_GRADIENT_2), GRADIENT_FILL_RECT_H);

			// draw title text
			WCHAR text[128];
			_r_str_printf (text, RTL_NUMBER_OF (text), _r_locale_getstring (IDS_NOTIFY_HEADER), APP_NAME);

			COLORREF clr_prev = SetTextColor (draw_info->hDC, RGB (255, 255, 255));
			DrawTextEx (draw_info->hDC, text, (INT)_r_str_length (text), &text_rect, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOCLIP | DT_NOPREFIX, NULL);
			SetTextColor (draw_info->hDC, clr_prev);

			// draw icon
			HICON hicon = (HICON)GetWindowLongPtr (draw_info->hwndItem, GWLP_USERDATA);

			if (!hicon)
				hicon = config.hicon_large;

			if (hicon)
				DrawIconEx (draw_info->hDC, icon_rect.left, icon_rect.top, hicon, icon_rect.right, icon_rect.bottom, 0, NULL, DI_IMAGE | DI_MASK);

			SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
			return TRUE;
		}

		case WM_SETCURSOR:
		{
			INT ctrl_id = GetDlgCtrlID ((HWND)wparam);

			if (ctrl_id == IDC_FILE_TEXT)
			{
				SetCursor (LoadCursor (NULL, IDC_HAND));

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
			LONG_PTR exstyle = _r_wnd_getstyle_ex (hwnd);

			if (!(exstyle & WS_EX_LAYERED))
				_r_wnd_setstyle_ex (hwnd, exstyle | WS_EX_LAYERED);

			SetLayeredWindowAttributes (hwnd, 0, (msg == WM_ENTERSIZEMOVE) ? 150 : 255, LWA_ALPHA);
			SetCursor (LoadCursor (NULL, (msg == WM_ENTERSIZEMOVE) ? IDC_SIZEALL : IDC_ARROW));

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lparam;

			switch (nmlp->code)
			{
				case BCN_DROPDOWN:
				{
					RECT rect;
					R_RECTANGLE rectangle;
					HMENU hsubmenu;
					INT ctrl_id;

					ctrl_id = (INT)(INT_PTR)nmlp->idFrom;

					if (!_r_ctrl_isenabled (hwnd, ctrl_id) || (ctrl_id != IDC_ALLOW_BTN && ctrl_id != IDC_RULES_BTN))
						break;

					hsubmenu = CreatePopupMenu ();

					if (hsubmenu)
					{
						if (ctrl_id == IDC_RULES_BTN)
						{
							AppendMenu (hsubmenu, MF_STRING, IDM_DISABLENOTIFICATIONS, _r_locale_getstring (IDS_DISABLENOTIFICATIONS));

							_app_generate_rulescontrol (hsubmenu, _app_notifyget_id (hwnd, FALSE));
						}
						else if (ctrl_id == IDC_ALLOW_BTN)
						{
							AppendMenu (hsubmenu, MF_STRING, IDC_ALLOW_BTN, _r_locale_getstring (IDS_ACTION_ALLOW));
							AppendMenu (hsubmenu, MF_SEPARATOR, 0, NULL);

							_app_generate_timerscontrol (hsubmenu, NULL);

							_r_menu_checkitem (hsubmenu, IDC_ALLOW_BTN, IDC_ALLOW_BTN, MF_BYCOMMAND, IDC_ALLOW_BTN);
						}

						if (GetClientRect (nmlp->hwndFrom, &rect))
						{
							ClientToScreen (nmlp->hwndFrom, (PPOINT)&rect);

							_r_wnd_recttorectangle (&rectangle, &rect);
							_r_wnd_adjustworkingarea (nmlp->hwndFrom, &rectangle);
							_r_wnd_rectangletorect (&rect, &rectangle);

							_r_menu_popup (hsubmenu, hwnd, (PPOINT)&rect, TRUE);
						}

						DestroyMenu (hsubmenu);
					}

					break;
				}

				case TTN_GETDISPINFO:
				{
					LPNMTTDISPINFO lpnmdi = (LPNMTTDISPINFO)lparam;

					if ((lpnmdi->uFlags & TTF_IDISHWND) == 0)
						break;

					WCHAR buffer[1024] = {0};
					INT ctrl_id = GetDlgCtrlID ((HWND)lpnmdi->hdr.idFrom);

					if (ctrl_id == IDC_FILE_TEXT)
					{
						PITEM_LOG ptr_log = _app_notifyget_obj (_app_notifyget_id (hwnd, FALSE));

						if (ptr_log)
						{
							INT listview_id = PtrToInt (_app_getappinfobyhash (ptr_log->app_hash, InfoListviewId));

							if (listview_id)
							{
								INT item_id = _app_getposition (_r_app_gethwnd (), listview_id, ptr_log->app_hash);

								if (item_id != -1)
								{
									PR_STRING string = _app_gettooltip (_r_app_gethwnd (), listview_id, item_id);

									if (string)
									{
										_r_str_copy (buffer, RTL_NUMBER_OF (buffer), string->buffer);

										_r_obj_dereference (string);
									}
								}
							}

							_r_obj_dereference (ptr_log);
						}
					}
					else if (ctrl_id == IDC_RULES_BTN)
					{
						_r_str_copy (buffer, RTL_NUMBER_OF (buffer), _r_locale_getstring (IDS_NOTIFY_TOOLTIP));
					}
					else if (ctrl_id == IDC_ALLOW_BTN)
					{
						_r_str_copy (buffer, RTL_NUMBER_OF (buffer), _r_locale_getstring (IDS_ACTION_ALLOW_HINT));
					}
					else if (ctrl_id == IDC_BLOCK_BTN)
					{
						_r_str_copy (buffer, RTL_NUMBER_OF (buffer), _r_locale_getstring (IDS_ACTION_BLOCK_HINT));
					}
					else if (ctrl_id == IDC_LATER_BTN)
					{
						_r_str_copy (buffer, RTL_NUMBER_OF (buffer), _r_locale_getstring (IDS_ACTION_LATER_HINT));
					}
					else
					{
						PR_STRING string = _r_ctrl_gettext (hwnd, ctrl_id);

						if (string)
						{
							_r_str_copy (buffer, RTL_NUMBER_OF (buffer), string->buffer);
							_r_obj_dereference (string);
						}
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
			INT ctrl_id = LOWORD (wparam);

			if ((ctrl_id >= IDX_RULES_SPECIAL && ctrl_id <= IDX_RULES_SPECIAL + (INT)_r_obj_getarraysize (rules_arr)))
			{
				SIZE_T rule_idx = (SIZE_T)ctrl_id - IDX_RULES_SPECIAL;
				PITEM_RULE ptr_rule = _app_getrulebyid (rule_idx);

				if (!ptr_rule)
					return FALSE;

				PITEM_LOG ptr_log = _app_notifyget_obj (_app_notifyget_id (hwnd, FALSE));

				if (ptr_log)
				{
					if (ptr_log->app_hash && !(ptr_rule->is_forservices && (ptr_log->app_hash == config.ntoskrnl_hash || ptr_log->app_hash == config.svchost_hash)))
					{
						PITEM_APP ptr_app = _app_getappitem (ptr_log->app_hash);

						if (ptr_app)
						{
							_app_freenotify (ptr_app);

							BOOLEAN is_remove = ptr_rule->is_enabled && _r_obj_findhashtable (ptr_rule->apps, ptr_log->app_hash);

							if (is_remove)
							{
								_r_obj_removehashtableentry (ptr_rule->apps, ptr_log->app_hash);

								if (_r_obj_ishashtableempty (ptr_rule->apps))
									_app_ruleenable (ptr_rule, FALSE, TRUE);
							}
							else
							{
								_app_addcachetable (ptr_rule->apps, ptr_log->app_hash, NULL, 0);

								_app_ruleenable (ptr_rule, TRUE, TRUE);
							}

							INT listview_id = (INT)_r_tab_getitemlparam (_r_app_gethwnd (), IDC_TAB, -1);
							INT app_listview_id = _app_getlistview_id (ptr_app->type);
							INT rule_listview_id = _app_getlistview_id (ptr_rule->type);

							{
								INT item_pos = _app_getposition (_r_app_gethwnd (), app_listview_id, ptr_log->app_hash);

								if (item_pos != -1)
								{
									_r_spinlock_acquireshared (&lock_checkbox);
									_app_setappiteminfo (_r_app_gethwnd (), app_listview_id, item_pos, ptr_app);
									_r_spinlock_releaseshared (&lock_checkbox);
								}
							}

							{
								INT item_pos = _app_getposition (_r_app_gethwnd (), rule_listview_id, rule_idx);

								if (item_pos != -1)
								{
									_r_spinlock_acquireshared (&lock_checkbox);
									_app_setruleiteminfo (_r_app_gethwnd (), rule_listview_id, item_pos, ptr_rule, FALSE);
									_r_spinlock_releaseshared (&lock_checkbox);
								}
							}

							if (_wfp_isfiltersinstalled ())
							{
								HANDLE hengine = _wfp_getenginehandle ();

								if (hengine)
								{
									PR_LIST rules = _r_obj_createlist (NULL);

									_r_obj_addlistitem (rules, ptr_rule);

									_wfp_create4filters (hengine, rules, __LINE__, FALSE);

									_r_obj_dereference (rules);
								}
							}

							if (listview_id == app_listview_id || listview_id == rule_listview_id)
							{
								_app_listviewsort (_r_app_gethwnd (), listview_id, -1, FALSE);
								_r_listview_redraw (_r_app_gethwnd (), listview_id, -1);
							}

							_app_refreshstatus (_r_app_gethwnd (), listview_id);
							_app_profile_save ();
						}
					}

					_r_obj_dereference (ptr_log);
				}

				return FALSE;
			}
			else if ((ctrl_id >= IDX_TIMER && ctrl_id <= IDX_TIMER + (INT)_r_obj_getarraysize (timers)))
			{
				if (!_r_ctrl_isenabled (hwnd, IDC_ALLOW_BTN))
					return FALSE;

				SIZE_T timer_idx = (SIZE_T)ctrl_id - IDX_TIMER;
				PLONG64 seconds = _r_obj_getarrayitem (timers, timer_idx);

				if (seconds)
					_app_notifycommand (hwnd, IDC_ALLOW_BTN, *seconds);

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
					PITEM_LOG ptr_log = _app_notifyget_obj (_app_notifyget_id (hwnd, FALSE));

					if (ptr_log)
					{
						PITEM_APP ptr_app = _app_getappitem (ptr_log->app_hash);

						if (ptr_app)
						{
							INT listview_id = _app_getlistview_id (ptr_app->type);

							if (listview_id)
								_app_showitem (_r_app_gethwnd (), listview_id, _app_getposition (_r_app_gethwnd (), listview_id, ptr_log->app_hash), -1);

							_r_wnd_toggle (_r_app_gethwnd (), TRUE);
						}

						_r_obj_dereference (ptr_log);
					}

					break;
				}

				case IDC_RULES_BTN:
				{
					ITEM_CONTEXT context = {0};
					SIZE_T app_hash = _app_notifyget_id (hwnd, FALSE);
					PITEM_APP ptr_app = _app_getappitem (app_hash);

					if (ptr_app)
					{
						context.is_settorules = FALSE;
						context.ptr_app = ptr_app;

						if (DialogBoxParam (NULL, MAKEINTRESOURCE (IDD_EDITOR), hwnd, &PropertiesProc, (LPARAM)&context))
						{

						}
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

				case IDM_OPENRULESEDITOR:
				{
					PITEM_APP ptr_app;
					PITEM_RULE ptr_rule;
					PITEM_LOG ptr_log;
					SIZE_T app_hash;

					ptr_log = _app_notifyget_obj (_app_notifyget_id (hwnd, FALSE));

					if (!ptr_log)
						break;

					PR_STRING rule_name = NULL;
					PR_STRING rule_string;

					app_hash = ptr_log->app_hash;
					ptr_app = _app_getappitem (app_hash);

					if (ptr_app)
						rule_name = _r_obj_createstring (_app_getdisplayname (ptr_app, TRUE));

					rule_string = _app_formataddress (ptr_log->af, 0, &ptr_log->remote_addr, ptr_log->remote_port, FMTADDR_AS_RULE);

					ptr_rule = _app_addrule (rule_name, rule_string, NULL, ptr_log->direction, ptr_log->protocol, ptr_log->af);

					_app_addcachetable (ptr_rule->apps, app_hash, NULL, 0);

					ptr_rule->type = DataRuleUser;
					ptr_rule->is_block = FALSE;

					if (ptr_app)
						_app_freenotify (ptr_app);

					SAFE_DELETE_REFERENCE (rule_name);
					SAFE_DELETE_REFERENCE (rule_string);

					_r_obj_dereference (ptr_log);

					_app_ruleenable (ptr_rule, TRUE, TRUE);

					ITEM_CONTEXT context = {0};

					context.is_settorules = TRUE;
					context.ptr_rule = ptr_rule;

					if (DialogBoxParam (NULL, MAKEINTRESOURCE (IDD_EDITOR), _r_app_gethwnd (), &PropertiesProc, (LPARAM)&context))
					{
						SIZE_T rule_idx = _r_obj_addarrayitem (rules_arr, ptr_rule);
						INT listview_id = (INT)_r_tab_getitemlparam (_r_app_gethwnd (), IDC_TAB, -1);

						// set rule information
						INT rules_listview_id = _app_getlistview_id (ptr_rule->type);

						if (rules_listview_id)
						{
							INT item_id = _r_listview_getitemcount (hwnd, rules_listview_id);

							_r_spinlock_acquireshared (&lock_checkbox);

							_r_listview_additemex (_r_app_gethwnd (), rules_listview_id, item_id, 0, SZ_EMPTY, 0, 0, rule_idx);
							_app_setruleiteminfo (_r_app_gethwnd (), rules_listview_id, item_id, ptr_rule, TRUE);

							_r_spinlock_releaseshared (&lock_checkbox);

							if (rules_listview_id == listview_id)
								_app_listviewsort (_r_app_gethwnd (), listview_id, -1, FALSE);
						}

						// set app information
						if (ptr_app)
						{
							INT app_listview_id = _app_getlistview_id (ptr_app->type);

							if (app_listview_id)
							{
								INT item_pos = _app_getposition (_r_app_gethwnd (), app_listview_id, app_hash);

								if (item_pos != -1)
								{
									_r_spinlock_acquireshared (&lock_checkbox);
									_app_setappiteminfo (_r_app_gethwnd (), app_listview_id, item_pos, ptr_app);
									_r_spinlock_releaseshared (&lock_checkbox);
								}

								if (app_listview_id == listview_id)
									_app_listviewsort (_r_app_gethwnd (), listview_id, -1, FALSE);
							}
						}

						_app_refreshstatus (_r_app_gethwnd (), listview_id);
						_app_profile_save ();
					}

					_r_mem_free (ptr_rule);

					break;
				}

				case IDM_COPY: // ctrl+c
				case IDM_SELECT_ALL: // ctrl+a
				{
					HWND hedit = GetFocus ();

					if (hedit)
					{
						WCHAR class_name[128];

						if (GetClassName (hedit, class_name, RTL_NUMBER_OF (class_name)) > 0)
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
