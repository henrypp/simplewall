// simplewall
// Copyright (c) 2016-2020 Henry++

#include "global.hpp"

HFONT hfont_title = NULL;
HFONT hfont_link = NULL;
HFONT hfont_text = NULL;

VOID _app_notifycreatewindow ()
{
	config.hnotification = CreateDialog (NULL, MAKEINTRESOURCE (IDD_NOTIFICATION), NULL, &NotificationProc);
}

BOOLEAN _app_notifycommand (HWND hwnd, INT button_id, time_t seconds)
{
	SIZE_T app_hash = _app_notifyget_id (hwnd, FALSE);
	PITEM_APP ptr_app = _app_getappitem (app_hash);

	if (!ptr_app)
		return FALSE;

	_app_freenotify (app_hash, ptr_app);

	OBJECTS_APP_VECTOR rules;

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
			if (item_pos != INVALID_INT)
			{
				_r_fastlock_acquireshared (&lock_checkbox);
				_app_setappiteminfo (_r_app_gethwnd (), listview_id, item_pos, app_hash, ptr_app);
				_r_fastlock_releaseshared (&lock_checkbox);
			}
		}

		rules.emplace_back (ptr_app);
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

	if (_wfp_isfiltersinstalled ())
	{
		HANDLE hengine = _wfp_getenginehandle ();

		if (hengine)
			_wfp_create3filters (hengine, &rules, __LINE__);
	}

	_app_freeapps_vec (&rules);

	_app_refreshstatus (_r_app_gethwnd (), listview_id);
	_app_profile_save ();

	if (listview_id && (INT)_r_tab_getlparam (_r_app_gethwnd (), IDC_TAB, INVALID_INT) == listview_id)
	{
		_app_listviewsort (_r_app_gethwnd (), listview_id, INVALID_INT, FALSE);
		_r_listview_redraw (_r_app_gethwnd (), listview_id, INVALID_INT);
	}

	return TRUE;
}

BOOLEAN _app_notifyadd (HWND hwnd, PITEM_LOG ptr_log, PITEM_APP ptr_app)
{
	// check for last display time
	time_t current_time = _r_unixtime_now ();
	time_t notification_timeout = _r_config_getlong64 (L"NotificationsTimeout", NOTIFY_TIMEOUT_DEFAULT);

	if (notification_timeout && ((current_time - ptr_app->last_notify) <= notification_timeout))
		return FALSE;

	ptr_app->last_notify = current_time;

	if (!ptr_log->hicon)
		_app_getappicon (ptr_app, FALSE, NULL, &ptr_log->hicon);

	_r_obj_movereference ((PVOID*)&ptr_app->pnotification, _r_obj_reference (ptr_log));

	if (_r_config_getboolean (L"IsNotificationsSound", TRUE))
	{
		if (!_r_config_getboolean (L"IsNotificationsFullscreenSilentMode", TRUE) || !_r_wnd_isfullscreenmode ())
			_app_notifyplaysound ();
	}

	if (!_r_wnd_isundercursor (hwnd))
		_app_notifyshow (hwnd, ptr_log, TRUE, TRUE);

	return TRUE;
}

VOID _app_freenotify (SIZE_T app_hash, PITEM_APP ptr_app)
{
	HWND hwnd = config.hnotification;

	if (ptr_app)
	{
		SAFE_DELETE_REFERENCE (ptr_app->pnotification);
	}

	if (_app_notifyget_id (hwnd, FALSE) == app_hash)
		_app_notifyget_id (hwnd, TRUE);

	_app_notifyrefresh (hwnd, TRUE);
}

SIZE_T _app_notifyget_id (HWND hwnd, BOOLEAN is_nearest)
{
	SIZE_T app_hash_current = (SIZE_T)GetWindowLongPtr (hwnd, GWLP_USERDATA);

	if (is_nearest)
	{
		for (auto it = apps.begin (); it != apps.end (); ++it)
		{
			if (!it->second || it->first == app_hash_current) // exclude current app from enumeration
				continue;

			SIZE_T app_hash = it->first;
			PITEM_APP ptr_app = (PITEM_APP)_r_obj_reference (it->second);

			if (ptr_app->pnotification)
			{
				_r_obj_dereference (ptr_app);

				SetWindowLongPtr (hwnd, GWLP_USERDATA, (LONG_PTR)app_hash);

				return app_hash;
			}

			_r_obj_dereference (ptr_app);
		}

		SetWindowLongPtr (hwnd, GWLP_USERDATA, 0);
		return 0;
	}

	return app_hash_current;
}

PITEM_LOG _app_notifyget_obj (SIZE_T app_hash)
{
	PITEM_APP ptr_app = _app_getappitem (app_hash);

	if (ptr_app)
	{
		PITEM_LOG ptr_log = (PITEM_LOG)_r_obj_reference (ptr_app->pnotification);

		_r_obj_dereference (ptr_app);

		return ptr_log;
	}

	return NULL;
}

BOOLEAN _app_notifyshow (HWND hwnd, PITEM_LOG ptr_log, BOOLEAN is_forced, BOOLEAN is_safety)
{
	if (!_r_config_getboolean (L"IsNotificationsEnabled", TRUE))
		return FALSE;

	PITEM_APP ptr_app = _app_getappitem (ptr_log->app_hash);

	if (!ptr_app)
		return FALSE;

	WCHAR windowTitle[128];
	WCHAR dateString[128];
	PR_STRING signatureString = NULL;
	PR_STRING nameString;
	PR_STRING remoteAddressString;
	PR_STRING remotePortString;
	PR_STRING directionString;
	PR_STRING localizedString = NULL;
	LPCWSTR emptyString;

	if (_r_config_getboolean (L"IsCertificatesEnabled", FALSE))
	{
		if (ptr_app->is_signed)
		{
			PR_STRING signatureCacheString = _app_getsignatureinfo (ptr_log->app_hash, ptr_app);

			if (signatureCacheString)
			{
				_r_obj_movereference (&signatureString, signatureCacheString);
			}
			else
			{
				signatureString = _r_obj_createstring (_r_locale_getstring (IDS_SIGN_UNSIGNED));
			}
		}
		else
		{
			signatureString = _r_obj_createstring (_r_locale_getstring (IDS_SIGN_UNSIGNED));
		}
	}

	_r_str_printf (windowTitle, RTL_NUMBER_OF (windowTitle), L"%s - " APP_NAME, _r_locale_getstring (IDS_NOTIFY_TITLE));

	SetWindowText (hwnd, windowTitle);

	SetWindowLongPtr (hwnd, GWLP_USERDATA, (LONG_PTR)ptr_log->app_hash);
	SetWindowLongPtr (GetDlgItem (hwnd, IDC_HEADER_ID), GWLP_USERDATA, (LONG_PTR)ptr_log->hicon);

	// print table text
	nameString = _app_getdisplayname (ptr_log->app_hash, ptr_app, TRUE);
	remoteAddressString = _app_formataddress (ptr_log->af, 0, &ptr_log->remote_addr, 0, FMTADDR_RESOLVE_HOST);
	remotePortString = _app_formatport (ptr_log->remote_port, FALSE);
	directionString = _app_getdirectionname (ptr_log->direction, ptr_log->is_loopback, TRUE);
	emptyString = _r_locale_getstring (IDS_STATUS_EMPTY);

	_r_format_dateex (dateString, RTL_NUMBER_OF (dateString), ptr_log->timestamp, FDTF_SHORTDATE | FDTF_LONGTIME);

	_r_obj_movereference (&localizedString, _r_format_string (L"%s:", _r_locale_getstring (IDS_NAME)));
	_r_ctrl_settabletext (hwnd, IDC_FILE_ID, _r_obj_getstring (localizedString), IDC_FILE_TEXT, _r_obj_getstringordefault (nameString, emptyString));

	_r_obj_movereference (&localizedString, _r_format_string (L"%s:", _r_locale_getstring (IDS_SIGNATURE)));
	_r_ctrl_settabletext (hwnd, IDC_SIGNATURE_ID, _r_obj_getstring (localizedString), IDC_SIGNATURE_TEXT, _r_obj_getstringordefault (signatureString, emptyString));

	_r_obj_movereference (&localizedString, _r_format_string (L"%s:", _r_locale_getstring (IDS_ADDRESS)));
	_r_ctrl_settabletext (hwnd, IDC_ADDRESS_ID, _r_obj_getstring (localizedString), IDC_ADDRESS_TEXT, _r_obj_getstringordefault (remoteAddressString, emptyString));

	_r_obj_movereference (&localizedString, _r_format_string (L"%s:", _r_locale_getstring (IDS_PORT)));
	_r_ctrl_settabletext (hwnd, IDC_PORT_ID, _r_obj_getstring (localizedString), IDC_PORT_TEXT, _r_obj_getstringordefault (remotePortString, emptyString));

	_r_obj_movereference (&localizedString, _r_format_string (L"%s:", _r_locale_getstring (IDS_DIRECTION)));
	_r_ctrl_settabletext (hwnd, IDC_DIRECTION_ID, _r_obj_getstring (localizedString), IDC_DIRECTION_TEXT, _r_obj_getstringordefault (directionString, emptyString));

	_r_obj_movereference (&localizedString, _r_format_string (L"%s:", _r_locale_getstring (IDS_FILTER)));
	_r_ctrl_settabletext (hwnd, IDC_FILTER_ID, _r_obj_getstring (localizedString), IDC_FILTER_TEXT, _r_obj_getstringordefault (ptr_log->filter_name, emptyString));

	_r_obj_movereference (&localizedString, _r_format_string (L"%s:", _r_locale_getstring (IDS_DATE)));
	_r_ctrl_settabletext (hwnd, IDC_DATE_ID, _r_obj_getstring (localizedString), IDC_DATE_TEXT, dateString);

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
	InvalidateRect (hwnd, NULL, TRUE);

	_r_wnd_top (hwnd, !is_fullscreenmode);

	ShowWindow (hwnd, is_forced ? SW_SHOW : SW_SHOWNA);

	SAFE_DELETE_REFERENCE (nameString);
	SAFE_DELETE_REFERENCE (signatureString);
	SAFE_DELETE_REFERENCE (remoteAddressString);
	SAFE_DELETE_REFERENCE (remotePortString);
	SAFE_DELETE_REFERENCE (directionString);
	SAFE_DELETE_REFERENCE (localizedString);
	SAFE_DELETE_REFERENCE (ptr_app);

	return TRUE;
}

VOID _app_notifyhide (HWND hwnd)
{
	ShowWindow (hwnd, SW_HIDE);
}

// Play notification sound even if system have "nosound" mode
VOID _app_notifyplaysound ()
{
	static PR_STRING path = NULL;

	if (_r_str_isempty (path) || !_r_fs_exists (path->Buffer))
	{
#define NOTIFY_SOUND_NAME L"MailBeep"

		HKEY hkey;

		if (RegOpenKeyEx (HKEY_CURRENT_USER, L"AppEvents\\Schemes\\Apps\\.Default\\" NOTIFY_SOUND_NAME L"\\.Default", 0, KEY_READ, &hkey) == ERROR_SUCCESS)
		{
			_r_obj_movereference (&path, _r_reg_querystring (hkey, NULL));

			if (path)
			{
				PR_STRING expandedString = _r_str_expandenvironmentstring (path);

				if (expandedString)
					_r_obj_movereference (&path, expandedString);
			}

			RegCloseKey (hkey);
		}
	}

	if (_r_str_isempty (path) || !PlaySound (path->Buffer, NULL, SND_ASYNC | SND_NODEFAULT | SND_NOWAIT | SND_FILENAME | SND_SENTRY))
		PlaySound (NOTIFY_SOUND_NAME, NULL, SND_ASYNC | SND_NODEFAULT | SND_NOWAIT | SND_SENTRY);
}

VOID _app_notifyrefresh (HWND hwnd, BOOLEAN is_safety)
{
	if (!_r_config_getboolean (L"IsNotificationsEnabled", TRUE) || !IsWindowVisible (hwnd))
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

VOID _app_notifysetpos (HWND hwnd, BOOLEAN is_forced)
{
	if (!is_forced && IsWindowVisible (hwnd))
	{
		RECT windowRect;

		if (GetWindowRect (hwnd, &windowRect))
		{
			_r_wnd_adjustwindowrect (hwnd, &windowRect);
			SetWindowPos (hwnd, NULL, windowRect.left, windowRect.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);

			return;
		}
	}

	BOOLEAN is_intray = _r_config_getboolean (L"IsNotificationsOnTray", FALSE);

	if (is_intray)
	{
		RECT windowRect;

		if (GetWindowRect (hwnd, &windowRect))
		{
			RECT desktopRect;

			if (SystemParametersInfo (SPI_GETWORKAREA, 0, &desktopRect, 0))
			{
				APPBARDATA abd;
				abd.cbSize = sizeof (abd);

				if (SHAppBarMessage (ABM_GETTASKBARPOS, &abd))
				{
					INT border_x = _r_dc_getsystemmetrics (hwnd, SM_CXBORDER);

					if (abd.uEdge == ABE_LEFT)
					{
						windowRect.left = abd.rc.right + border_x;
						windowRect.top = (desktopRect.bottom - _r_calc_rectheight (LONG, &windowRect)) - border_x;
					}
					else if (abd.uEdge == ABE_TOP)
					{
						windowRect.left = (desktopRect.right - _r_calc_rectwidth (LONG, &windowRect)) - border_x;
						windowRect.top = abd.rc.bottom + border_x;
					}
					else if (abd.uEdge == ABE_RIGHT)
					{
						windowRect.left = (desktopRect.right - _r_calc_rectwidth (LONG, &windowRect)) - border_x;
						windowRect.top = (desktopRect.bottom - _r_calc_rectheight (LONG, &windowRect)) - border_x;
					}
					else/* if (abd.uEdge == ABE_BOTTOM)*/
					{
						windowRect.left = (desktopRect.right - (windowRect.right - windowRect.left)) - border_x;
						windowRect.top = (desktopRect.bottom - _r_calc_rectheight (LONG, &windowRect)) - border_x;
					}

					SetWindowPos (hwnd, NULL, windowRect.left, windowRect.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
					return;
				}
			}
		}
	}

	_r_wnd_center (hwnd, NULL); // display window on center (depends on error, config etc...)
}

HFONT _app_notifyfontinit (HWND hwnd, PLOGFONT plf, LONG height, LONG weight, LPCWSTR name, BOOLEAN is_underline)
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

VOID _app_notifyfontset (HWND hwnd)
{
	SendMessage (hwnd, WM_SETICON, ICON_SMALL, (LPARAM)_r_app_getsharedimage (NULL, SIH_EXCLAMATION, _r_dc_getsystemmetrics (hwnd, SM_CXSMICON)));
	SendMessage (hwnd, WM_SETICON, ICON_BIG, (LPARAM)_r_app_getsharedimage (NULL, SIH_EXCLAMATION, _r_dc_getsystemmetrics (hwnd, SM_CXICON)));

	INT title_font_height = 12;
	INT text_font_height = 9;

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

	BOOLEAN is_classic = _r_app_isclassicui ();

	_r_wnd_addstyle (hwnd, IDC_RULES_BTN, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
	_r_wnd_addstyle (hwnd, IDC_ALLOW_BTN, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
	_r_wnd_addstyle (hwnd, IDC_BLOCK_BTN, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
	_r_wnd_addstyle (hwnd, IDC_LATER_BTN, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

	InvalidateRect (hwnd, NULL, TRUE);
}

VOID DrawGradient (HDC hdc, const LPRECT lprc, COLORREF rgb1, COLORREF rgb2, ULONG mode)
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

	GradientFill (hdc, triVertext, RTL_NUMBER_OF (triVertext), &gradientRect, 1, mode);
}

INT_PTR CALLBACK NotificationProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
		case WM_INITDIALOG:
		{
#if defined(_APP_HAVE_DARKTHEME)
			_r_wnd_setdarktheme (hwnd);
#endif // _APP_HAVE_DARKTHEME

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

		case WM_ERASEBKGND:
		{
			RECT clientRect;

			if (GetClientRect (hwnd, &clientRect))
				_r_dc_fillrect ((HDC)wparam, &clientRect, GetSysColor (COLOR_WINDOW));

			SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
			return TRUE;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps = {0};
			HDC hdc = BeginPaint (hwnd, &ps);

			if (hdc)
			{
				RECT clientRect;

				if (GetClientRect (hwnd, &clientRect))
				{
					INT wnd_width = _r_calc_rectwidth (INT, &clientRect);
					INT wnd_height = _r_calc_rectheight (INT, &clientRect);

					SetRect (&clientRect, 0, wnd_height - _r_dc_getdpi (hwnd, _R_SIZE_FOOTERHEIGHT), wnd_width, wnd_height);
					_r_dc_fillrect (hdc, &clientRect, GetSysColor (COLOR_BTNFACE));

					for (INT i = 0; i < wnd_width; i++)
						SetPixelV (hdc, i, clientRect.top, GetSysColor (COLOR_APPWORKSPACE));
				}

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

			INT wnd_icon_size = _r_dc_getsystemmetrics (hwnd, SM_CXICON);
			INT wnd_spacing = _r_dc_getdpi (hwnd, 12);

			RECT textRect;
			RECT iconRect;

			SetRect (&textRect, wnd_spacing, 0, _r_calc_rectwidth (INT, &drawInfo->rcItem) - (wnd_spacing * 3) - wnd_icon_size, _r_calc_rectheight (INT, &drawInfo->rcItem));
			SetRect (&iconRect, _r_calc_rectwidth (INT, &drawInfo->rcItem) - wnd_icon_size - wnd_spacing, (_r_calc_rectheight (INT, &drawInfo->rcItem) / 2) - (wnd_icon_size / 2), wnd_icon_size, wnd_icon_size);

			SetBkMode (drawInfo->hDC, TRANSPARENT);

			// draw background
			DrawGradient (drawInfo->hDC, &drawInfo->rcItem, _r_config_getulong (L"NotificationBackground1", NOTIFY_GRADIENT_1), _r_config_getulong (L"NotificationBackground2", NOTIFY_GRADIENT_2), GRADIENT_FILL_RECT_H);

			// draw title text
			WCHAR text[128];
			_r_str_printf (text, RTL_NUMBER_OF (text), _r_locale_getstring (IDS_NOTIFY_HEADER), APP_NAME);

			COLORREF clr_prev = SetTextColor (drawInfo->hDC, RGB (255, 255, 255));
			DrawTextEx (drawInfo->hDC, text, (INT)_r_str_length (text), &textRect, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOCLIP | DT_NOPREFIX, NULL);
			SetTextColor (drawInfo->hDC, clr_prev);

			// draw icon
			HICON hicon = (HICON)GetWindowLongPtr (drawInfo->hwndItem, GWLP_USERDATA);

			if (!hicon)
				hicon = config.hicon_large;

			if (hicon)
				DrawIconEx (drawInfo->hDC, iconRect.left, iconRect.top, hicon, iconRect.right, iconRect.bottom, 0, NULL, DI_IMAGE | DI_MASK);

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
			LONG_PTR exstyle = GetWindowLongPtr (hwnd, GWL_EXSTYLE);

			if ((exstyle & WS_EX_LAYERED) == 0)
				SetWindowLongPtr (hwnd, GWL_EXSTYLE, exstyle | WS_EX_LAYERED);

			SetLayeredWindowAttributes (hwnd, 0, (msg == WM_ENTERSIZEMOVE) ? 100 : 255, LWA_ALPHA);
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
					INT ctrl_id = PtrToInt ((PVOID)nmlp->idFrom);

					if (!_r_ctrl_isenabled (hwnd, ctrl_id) || (ctrl_id != IDC_RULES_BTN && ctrl_id != IDC_ALLOW_BTN))
						break;

					HMENU hsubmenu = CreatePopupMenu ();

					if (!hsubmenu)
						break;

					if (ctrl_id == IDC_RULES_BTN)
					{
						AppendMenu (hsubmenu, MF_STRING, IDM_DISABLENOTIFICATIONS, _r_locale_getstring (IDS_DISABLENOTIFICATIONS));

						_app_generate_rulesmenu (hsubmenu, _app_notifyget_id (hwnd, FALSE));
					}
					else if (ctrl_id == IDC_ALLOW_BTN)
					{
						AppendMenu (hsubmenu, MF_STRING, IDC_ALLOW_BTN, _r_locale_getstring (IDS_ACTION_ALLOW));
						AppendMenu (hsubmenu, MF_SEPARATOR, 0, NULL);

						_app_generate_timermenu (hsubmenu, 0);

						_r_menu_checkitem (hsubmenu, IDC_ALLOW_BTN, IDC_ALLOW_BTN, MF_BYCOMMAND, IDC_ALLOW_BTN);
					}

					RECT buttonRect;

					if (GetClientRect (nmlp->hwndFrom, &buttonRect))
					{
						ClientToScreen (nmlp->hwndFrom, (LPPOINT)&buttonRect);

						_r_wnd_adjustwindowrect (nmlp->hwndFrom, &buttonRect);
					}

					_r_menu_popup (hsubmenu, hwnd, (LPPOINT)&buttonRect, TRUE);

					DestroyMenu (hsubmenu);

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
							INT listview_id = (INT)_app_getappinfo (ptr_log->app_hash, InfoListviewId);

							if (listview_id)
							{
								INT item_id = _app_getposition (_r_app_gethwnd (), listview_id, ptr_log->app_hash);

								if (item_id != INVALID_INT)
								{
									PR_STRING string = _app_gettooltip (_r_app_gethwnd (), listview_id, item_id);

									if (string)
									{
										_r_str_copy (buffer, RTL_NUMBER_OF (buffer), string->Buffer);
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
							_r_str_copy (buffer, RTL_NUMBER_OF (buffer), _r_obj_getstring (string));
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

			if ((LOWORD (wparam) >= IDX_RULES_SPECIAL && LOWORD (wparam) <= IDX_RULES_SPECIAL + (INT)rules_arr.size ()))
			{
				SIZE_T rule_idx = (LOWORD (wparam) - IDX_RULES_SPECIAL);
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
							_app_freenotify (ptr_log->app_hash, ptr_app);

							BOOLEAN is_remove = ptr_rule->is_enabled && (ptr_rule->apps->find (ptr_log->app_hash) != ptr_rule->apps->end ());

							if (is_remove)
							{
								ptr_rule->apps->erase (ptr_log->app_hash);

								if (ptr_rule->apps->empty ())
									_app_ruleenable (ptr_rule, FALSE);
							}
							else
							{
								ptr_rule->apps->emplace (ptr_log->app_hash, TRUE);
								_app_ruleenable (ptr_rule, TRUE);
							}

							INT listview_id = (INT)_r_tab_getlparam (_r_app_gethwnd (), IDC_TAB, INVALID_INT);
							INT app_listview_id = _app_getlistview_id (ptr_app->type);
							INT rule_listview_id = _app_getlistview_id (ptr_rule->type);

							{
								INT item_pos = _app_getposition (_r_app_gethwnd (), app_listview_id, ptr_log->app_hash);

								if (item_pos != INVALID_INT)
								{
									_r_fastlock_acquireshared (&lock_checkbox);
									_app_setappiteminfo (_r_app_gethwnd (), app_listview_id, item_pos, ptr_log->app_hash, ptr_app);
									_r_fastlock_releaseshared (&lock_checkbox);
								}
							}

							{
								INT item_pos = _app_getposition (_r_app_gethwnd (), rule_listview_id, rule_idx);

								if (item_pos != INVALID_INT)
								{
									_r_fastlock_acquireshared (&lock_checkbox);
									_app_setruleiteminfo (_r_app_gethwnd (), rule_listview_id, item_pos, ptr_rule, FALSE);
									_r_fastlock_releaseshared (&lock_checkbox);
								}
							}

							OBJECTS_RULE_VECTOR rules;
							rules.emplace_back (ptr_rule);

							if (_wfp_isfiltersinstalled ())
							{
								HANDLE hengine = _wfp_getenginehandle ();

								if (hengine)
									_wfp_create4filters (hengine, &rules, __LINE__);
							}

							if (listview_id == app_listview_id || listview_id == rule_listview_id)
							{
								_app_listviewsort (_r_app_gethwnd (), listview_id, INVALID_INT, FALSE);
								_r_listview_redraw (_r_app_gethwnd (), listview_id, INVALID_INT);
							}

							_app_refreshstatus (_r_app_gethwnd (), listview_id);
							_app_profile_save ();

							_r_obj_dereference (ptr_app);
						}
					}

					_r_obj_dereference (ptr_log);
				}

				_r_obj_dereference (ptr_rule);

				return FALSE;
			}
			else if ((ctrl_id >= IDX_TIMER && ctrl_id <= IDX_TIMER + (INT)timers.size ()))
			{
				if (!_r_ctrl_isenabled (hwnd, IDC_ALLOW_BTN))
					return FALSE;

				SIZE_T timer_idx = (ctrl_id - IDX_TIMER);

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
					PITEM_LOG ptr_log = _app_notifyget_obj (_app_notifyget_id (hwnd, FALSE));

					if (ptr_log)
					{
						PITEM_APP ptr_app = _app_getappitem (ptr_log->app_hash);

						if (ptr_app)
						{
							INT listview_id = _app_getlistview_id (ptr_app->type);

							if (listview_id)
								_app_showitem (_r_app_gethwnd (), listview_id, _app_getposition (_r_app_gethwnd (), listview_id, ptr_log->app_hash), INVALID_INT);

							_r_wnd_toggle (_r_app_gethwnd (), TRUE);

							_r_obj_dereference (ptr_app);
						}

						_r_obj_dereference (ptr_log);
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
						hdr.idFrom = (UINT_PTR)ctrl_id;
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
					if (_r_ctrl_isenabled (hwnd, IDC_RULES_BTN))
						_app_notifycommand (hwnd, ctrl_id, 0);

					break;
				}

				case IDM_EDITRULES:
				{
					_r_wnd_toggle (_r_app_gethwnd (), TRUE);
					_app_settab_id (_r_app_gethwnd (), _app_getlistview_id (DataRuleCustom));

					break;
				}

				case IDM_OPENRULESEDITOR:
				{
					PITEM_APP ptr_app;
					PITEM_RULE ptr_rule;
					PITEM_LOG ptr_log;
					SIZE_T app_hash;

					ptr_rule = (PITEM_RULE)_r_obj_allocateex (sizeof (ITEM_RULE), &_app_dereferencerule);

					// initialize stl
					ptr_rule->apps = new HASHER_MAP;
					ptr_rule->guids = new GUIDS_VEC;

					ptr_log = _app_notifyget_obj (_app_notifyget_id (hwnd, FALSE));

					if (ptr_log)
					{
						app_hash = ptr_log->app_hash;

						ptr_rule->apps->emplace (app_hash, TRUE);
						ptr_rule->protocol = ptr_log->protocol;
						ptr_rule->direction = ptr_log->direction;

						ptr_app = _app_getappitem (app_hash);

						if (ptr_app)
						{
							_app_freenotify (app_hash, ptr_app);
							_r_obj_dereference (ptr_app);
						}

						PR_STRING remoteRuleString;

						remoteRuleString = _app_formataddress (ptr_log->af, 0, &ptr_log->remote_addr, ptr_log->remote_port, FMTADDR_AS_RULE);

						if (remoteRuleString)
						{
							ptr_rule->name = _r_obj_createstring2 (remoteRuleString);

							_r_obj_movereference (&ptr_rule->rule_remote, remoteRuleString);
						}

						_r_obj_dereference (ptr_log);
					}
					else
					{
						_r_obj_dereference (ptr_rule);
						break;
					}

					ptr_rule->type = DataRuleCustom;
					ptr_rule->is_block = FALSE;

					_app_ruleenable (ptr_rule, TRUE);

					if (DialogBoxParam (NULL, MAKEINTRESOURCE (IDD_EDITOR), _r_app_gethwnd (), &EditorProc, (LPARAM)ptr_rule))
					{
						SIZE_T rule_idx = rules_arr.size ();
						rules_arr.emplace_back ((PITEM_RULE)_r_obj_reference (ptr_rule));

						INT listview_id = (INT)_r_tab_getlparam (_r_app_gethwnd (), IDC_TAB, INVALID_INT);

						// set rule information
						INT rules_listview_id = _app_getlistview_id (ptr_rule->type);

						if (rules_listview_id)
						{
							INT item_id = _r_listview_getitemcount (hwnd, rules_listview_id, FALSE);

							_r_fastlock_acquireshared (&lock_checkbox);

							_r_listview_additemex (_r_app_gethwnd (), rules_listview_id, item_id, 0, _r_obj_getstringorempty (ptr_rule->name), _app_getruleicon (ptr_rule), _app_getrulegroup (ptr_rule), rule_idx);
							_app_setruleiteminfo (_r_app_gethwnd (), rules_listview_id, item_id, ptr_rule, TRUE);

							_r_fastlock_releaseshared (&lock_checkbox);

							if (rules_listview_id == listview_id)
								_app_listviewsort (_r_app_gethwnd (), listview_id, INVALID_INT, FALSE);
						}

						// set app information
						ptr_app = _app_getappitem (app_hash);

						if (ptr_app)
						{
							INT app_listview_id = _app_getlistview_id (ptr_app->type);

							if (app_listview_id)
							{
								INT item_pos = _app_getposition (_r_app_gethwnd (), app_listview_id, app_hash);

								if (item_pos != INVALID_INT)
								{
									_r_fastlock_acquireshared (&lock_checkbox);
									_app_setappiteminfo (_r_app_gethwnd (), app_listview_id, item_pos, app_hash, ptr_app);
									_r_fastlock_releaseshared (&lock_checkbox);
								}

								if (app_listview_id == listview_id)
									_app_listviewsort (_r_app_gethwnd (), listview_id, INVALID_INT, FALSE);
							}

							_r_obj_dereference (ptr_app);
						}

						_app_refreshstatus (_r_app_gethwnd (), listview_id);
						_app_profile_save ();
					}

					_r_obj_dereference (ptr_rule);

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
