// simplewall
// Copyright (c) 2016-2019 Henry++

#include "global.hpp"

HFONT hfont_title = nullptr;

void _app_setbuttonmargins (HWND hwnd, UINT ctrl_id)
{
	// set icons margin
	{
		RECT rc = {0};
		rc.left = rc.right = app.GetDPI (4);

		SendDlgItemMessage (hwnd, ctrl_id, BCM_SETTEXTMARGIN, 0, (LPARAM)& rc);
	}

	// set split info
	{
		BUTTON_SPLITINFO bsi = {0};

		bsi.mask = BCSIF_SIZE | BCSIF_STYLE;
		bsi.uSplitStyle = BCSS_STRETCH;

		bsi.size.cx = app.GetDPI (18);
		bsi.size.cy = 0;

		SendDlgItemMessage (hwnd, ctrl_id, BCM_SETSPLITINFO, 0, (LPARAM)& bsi);
	}
}

void _app_notifycreatewindow ()
{
	WNDCLASSEX wcex = {0};

	wcex.cbSize = sizeof (wcex);
	wcex.style = CS_VREDRAW | CS_HREDRAW;
	wcex.hInstance = app.GetHINSTANCE ();
	wcex.hCursor = LoadCursor (nullptr, IDC_ARROW);
	wcex.hIcon = app.GetSharedImage (nullptr, 32515, GetSystemMetrics (SM_CXICON));
	wcex.hbrBackground = GetSysColorBrush (COLOR_WINDOW);
	wcex.lpszClassName = NOTIFY_CLASS_DLG;
	wcex.lpfnWndProc = &NotificationProc;

	if (!RegisterClassEx (&wcex))
		return;

	const INT title_font_height = 12;
	const INT text_font_height = 9;

	config.hnotification = CreateWindowEx (WS_EX_APPWINDOW | WS_EX_TOPMOST, NOTIFY_CLASS_DLG, nullptr, WS_POPUPWINDOW | WS_CAPTION, CW_USEDEFAULT, CW_USEDEFAULT, app.GetDPI (NOTIFY_WIDTH), app.GetDPI (NOTIFY_HEIGHT), nullptr, nullptr, wcex.hInstance, nullptr);

	if (!config.hnotification)
		return;

	RECT rc = {0};
	GetClientRect (config.hnotification, &rc);

	const INT wnd_width = _R_RECT_WIDTH (&rc);
	const INT wnd_height = _R_RECT_HEIGHT (&rc);
	const INT wnd_icon_size = GetSystemMetrics (SM_CXICON);
	const INT wnd_spacing = app.GetDPI (12);

	const INT header_size = app.GetDPI (NOTIFY_HEADER_HEIGHT);

	const INT btn_width = app.GetDPI (122);
	const INT btn_width_small = app.GetDPI (38);
	const INT btn_height = app.GetDPI (24);
	const INT btn_icon_size = GetSystemMetrics (SM_CXSMICON);

	const UINT text_height = app.GetDPI (16);
	const UINT text_spacing = app.GetDPI (22);
	const UINT text_top = header_size - app.GetDPI (10);

	HFONT hfont_link = nullptr;
	HFONT hfont_normal = nullptr;

	// load system font
	{
		NONCLIENTMETRICS ncm = {0};
		ncm.cbSize = sizeof (ncm);

		if (SystemParametersInfo (SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0))
		{
			hfont_title = _app_notifyinitfont (&ncm.lfCaptionFont, title_font_height, FW_NORMAL, UI_FONT, false);
			hfont_link = _app_notifyinitfont (&ncm.lfMessageFont, text_font_height, FW_NORMAL, UI_FONT, true);
			hfont_normal = _app_notifyinitfont (&ncm.lfMessageFont, text_font_height, FW_NORMAL, UI_FONT, false);
		}
	}

	CreateWindow (WC_STATIC, nullptr, WS_CHILD | SS_CENTERIMAGE | SS_CENTER | SS_ICON, wnd_width - wnd_icon_size - wnd_spacing, (wnd_icon_size / 2) - (wnd_icon_size / 2), wnd_icon_size, header_size, config.hnotification, (HMENU)IDC_ICON_ID, nullptr, nullptr);

	HWND hctrl = CreateWindow (WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS | SS_CENTERIMAGE, wnd_spacing, text_top + text_spacing, wnd_width - app.GetDPI (24), text_height, config.hnotification, (HMENU)IDC_FILE_ID, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_normal, 0);

	hctrl = CreateWindow (WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_NOTIFY | SS_ENDELLIPSIS | SS_RIGHT, wnd_spacing, text_top + text_spacing, wnd_width - app.GetDPI (24), text_height, config.hnotification, (HMENU)IDC_FILE_TEXT, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_link, 0);

	hctrl = CreateWindow (WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS | SS_CENTERIMAGE, wnd_spacing, text_top + text_spacing * 2, wnd_width - app.GetDPI (24), text_height, config.hnotification, (HMENU)IDC_SIGNATURE_ID, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_normal, 0);

	hctrl = CreateWindow (WC_EDIT, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_READONLY | ES_AUTOHSCROLL | ES_RIGHT, wnd_spacing, text_top + text_spacing * 2, wnd_width - app.GetDPI (24), text_height, config.hnotification, (HMENU)IDC_SIGNATURE_TEXT, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_normal, 0);
	SendMessage (hctrl, EM_SETMARGINS, EC_LEFTMARGIN, 0);
	SendMessage (hctrl, EM_SETMARGINS, EC_RIGHTMARGIN, 0);

	hctrl = CreateWindow (WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS | SS_CENTERIMAGE, wnd_spacing, text_top + text_spacing * 3, wnd_width - app.GetDPI (24), text_height, config.hnotification, (HMENU)IDC_ADDRESS_ID, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_normal, 0);

	hctrl = CreateWindow (WC_EDIT, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_READONLY | ES_AUTOHSCROLL | ES_RIGHT, wnd_spacing, text_top + text_spacing * 3, wnd_width - app.GetDPI (24), text_height, config.hnotification, (HMENU)IDC_ADDRESS_TEXT, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_normal, 0);
	SendMessage (hctrl, EM_SETMARGINS, EC_LEFTMARGIN, 0);
	SendMessage (hctrl, EM_SETMARGINS, EC_RIGHTMARGIN, 0);

	hctrl = CreateWindow (WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS | SS_CENTERIMAGE, wnd_spacing, text_top + text_spacing * 4, wnd_width - app.GetDPI (24), text_height, config.hnotification, (HMENU)IDC_PORT_ID, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_normal, 0);

	hctrl = CreateWindow (WC_EDIT, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_READONLY | ES_AUTOHSCROLL | ES_RIGHT, wnd_spacing, text_top + text_spacing * 4, wnd_width - app.GetDPI (24), text_height, config.hnotification, (HMENU)IDC_PORT_TEXT, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_normal, 0);
	SendMessage (hctrl, EM_SETMARGINS, EC_LEFTMARGIN, 0);
	SendMessage (hctrl, EM_SETMARGINS, EC_RIGHTMARGIN, 0);

	hctrl = CreateWindow (WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS | SS_CENTERIMAGE, wnd_spacing, text_top + text_spacing * 5, wnd_width - app.GetDPI (24), text_height, config.hnotification, (HMENU)IDC_DIRECTION_ID, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_normal, 0);

	hctrl = CreateWindow (WC_EDIT, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_READONLY | ES_AUTOHSCROLL | ES_RIGHT, wnd_spacing, text_top + text_spacing * 5, wnd_width - app.GetDPI (24), text_height, config.hnotification, (HMENU)IDC_DIRECTION_TEXT, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_normal, 0);
	SendMessage (hctrl, EM_SETMARGINS, EC_LEFTMARGIN, 0);
	SendMessage (hctrl, EM_SETMARGINS, EC_RIGHTMARGIN, 0);

	hctrl = CreateWindow (WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS | SS_CENTERIMAGE, wnd_spacing, text_top + text_spacing * 6, wnd_width - app.GetDPI (24), text_height, config.hnotification, (HMENU)IDC_FILTER_ID, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_normal, 0);

	hctrl = CreateWindow (WC_EDIT, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_READONLY | ES_AUTOHSCROLL | ES_RIGHT, wnd_spacing, text_top + text_spacing * 6, wnd_width - app.GetDPI (24), text_height, config.hnotification, (HMENU)IDC_FILTER_TEXT, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_normal, 0);
	SendMessage (hctrl, EM_SETMARGINS, EC_LEFTMARGIN, 0);
	SendMessage (hctrl, EM_SETMARGINS, EC_RIGHTMARGIN, 0);

	hctrl = CreateWindow (WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS | SS_CENTERIMAGE, wnd_spacing, text_top + text_spacing * 7, wnd_width - app.GetDPI (24), text_height, config.hnotification, (HMENU)IDC_DATE_ID, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_normal, 0);

	hctrl = CreateWindow (WC_EDIT, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_READONLY | ES_AUTOHSCROLL | ES_RIGHT, wnd_spacing, text_top + text_spacing * 7, wnd_width - app.GetDPI (24), text_height, config.hnotification, (HMENU)IDC_DATE_TEXT, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_normal, 0);
	SendMessage (hctrl, EM_SETMARGINS, EC_LEFTMARGIN, 0);
	SendMessage (hctrl, EM_SETMARGINS, EC_RIGHTMARGIN, 0);

	hctrl = CreateWindowEx (app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WC_BUTTON, nullptr, WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_SPLITBUTTON, wnd_spacing, wnd_height - app.GetDPI (36), btn_width, btn_height, config.hnotification, (HMENU)IDC_RULES_BTN, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_normal, 0);
	SendMessage (hctrl, BM_SETIMAGE, IMAGE_BITMAP, (WPARAM)_app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_RULES), btn_icon_size));

	hctrl = CreateWindowEx (app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WC_BUTTON, nullptr, WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_BITMAP, wnd_spacing + btn_width + wnd_spacing / 2, wnd_height - app.GetDPI (36), btn_width_small, btn_height, config.hnotification, (HMENU)IDC_NEXT_BTN, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_normal, 0);
	SendMessage (hctrl, BM_SETIMAGE, IMAGE_BITMAP, (WPARAM)_app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_NEXT), btn_icon_size));

	hctrl = CreateWindowEx (app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WC_BUTTON, nullptr, WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_SPLITBUTTON, wnd_width - (((btn_width * 2) + app.GetDPI (20))), wnd_height - app.GetDPI (36), btn_width, btn_height, config.hnotification, (HMENU)IDC_ALLOW_BTN, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_normal, 0);
	SendMessage (hctrl, BM_SETIMAGE, IMAGE_BITMAP, (WPARAM)_app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_ALLOW), btn_icon_size));

	hctrl = CreateWindowEx (app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WC_BUTTON, nullptr, WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, wnd_width - btn_width - wnd_spacing, wnd_height - app.GetDPI (36), btn_width, btn_height, config.hnotification, (HMENU)IDC_BLOCK_BTN, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_normal, 0);
	SendMessage (hctrl, BM_SETIMAGE, IMAGE_BITMAP, (WPARAM)_app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_BLOCK), btn_icon_size));

	_app_setbuttonmargins (config.hnotification, IDC_RULES_BTN);
	_app_setbuttonmargins (config.hnotification, IDC_ALLOW_BTN);
	_app_setbuttonmargins (config.hnotification, IDC_BLOCK_BTN);

	const HWND htip = _r_ctrl_createtip (config.hnotification);

	_r_ctrl_settip (htip, config.hnotification, IDC_FILE_TEXT, LPSTR_TEXTCALLBACK);

	_app_notifyhide (config.hnotification);

#ifndef _APP_NO_DARKTHEME
	_r_wnd_setdarktheme (config.hnotification);
#endif // _APP_NO_DARKTHEME
}

bool _app_notifycommand (HWND hwnd, UINT button_id, time_t seconds)
{
	size_t app_hash = _app_notifyget_id (hwnd, 0);
	PR_OBJECT ptr_app_object = _app_getappitem (app_hash);

	if (!ptr_app_object)
		return false;

	PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

	if (!ptr_app)
	{
		_r_obj_dereference (ptr_app_object, &_app_dereferenceapp);
		return false;
	}

	_app_freenotify (ptr_app, true);

	OBJECTS_VEC rules;

	const UINT listview_id = _app_getlistview_id (ptr_app->type);
	const size_t item = _app_getposition (app.GetHWND (), listview_id, app_hash);

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
			if (item != LAST_VALUE)
			{
				_r_fastlock_acquireshared (&lock_checkbox);
				_app_setappiteminfo (app.GetHWND (), listview_id, item, app_hash, ptr_app);
				_r_fastlock_releaseshared (&lock_checkbox);
			}
		}

		rules.push_back (ptr_app_object);
	}
	else if (button_id == IDM_DISABLENOTIFICATIONS)
	{
		ptr_app->is_silent = true;
	}

	ptr_app->last_notify = _r_unixtime_now ();

	_wfp_create3filters (rules, __LINE__);

	_r_obj_dereference (ptr_app_object, &_app_dereferenceapp);

	_app_refreshstatus (app.GetHWND ());
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
	if (!ptr_app || ptr_app->pnotification || !ptr_log_object)
	{
		_r_obj_dereference (ptr_log_object, &_app_dereferencelog);
		return false;
	}

	// check for last display time
	const time_t current_time = _r_unixtime_now ();
	const time_t notification_timeout = app.ConfigGet (L"NotificationsTimeout", NOTIFY_TIMEOUT_DEFAULT).AsLonglong ();

	if (notification_timeout && ((current_time - ptr_app->last_notify) <= notification_timeout))
	{
		_r_obj_dereference (ptr_log_object, &_app_dereferencelog);
		return false;
	}

	PITEM_LOG ptr_log = (PITEM_LOG)ptr_log_object->pdata;

	if (!ptr_log)
	{
		_r_obj_dereference (ptr_log_object, &_app_dereferencelog);
		return false;
	}

	ptr_app->last_notify = current_time;

	if (!ptr_log->hicon)
		_app_getappicon (ptr_app, false, nullptr, &ptr_log->hicon);

	// remove existing log item (if exists)
	if (ptr_app->pnotification)
		_r_obj_dereference (ptr_app->pnotification, &_app_dereferencelog);

	ptr_app->pnotification = ptr_log_object;

	if (app.ConfigGet (L"IsNotificationsSound", true).AsBool ())
		_app_notifyplaysound ();

	if (!_r_wnd_undercursor (hwnd))
		_app_notifyshow (hwnd, ptr_log_object, true, true);

	return true;
}

void _app_freenotify (size_t app_hash, bool is_refresh)
{
	PR_OBJECT ptr_app_object = _app_getappitem (app_hash);

	if (ptr_app_object)
	{
		PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

		_app_freenotify (ptr_app, is_refresh);

		_r_obj_dereference (ptr_app_object, &_app_dereferenceapp);
	}
}

void _app_freenotify (PITEM_APP ptr_app, bool is_refresh)
{
	if (ptr_app)
	{
		if (ptr_app->pnotification)
		{
			_r_obj_dereference (ptr_app->pnotification, &_app_dereferencelog);
			ptr_app->pnotification = nullptr;

			if (is_refresh)
				_app_notifyrefresh (config.hnotification, true);
		}
	}
}

size_t _app_notifyget_id (HWND hwnd, size_t current_id)
{
	size_t app_hash_current = (size_t)GetWindowLongPtr (hwnd, GWLP_USERDATA);

	if (current_id == LAST_VALUE)
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
				_r_obj_dereference (ptr_app_object, &_app_dereferenceapp);

				return p.first;
			}

			_r_obj_dereference (ptr_app_object, &_app_dereferenceapp);
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

			_r_obj_dereference (ptr_app_object, &_app_dereferenceapp);

			return ptr_log_object;
		}
	}

	return nullptr;
}

bool _app_notifyshow (HWND hwnd, PR_OBJECT ptr_log_object, bool is_forced, bool is_safety)
{
	if (!app.ConfigGet (L"IsNotificationsEnabled", true).AsBool ())
		return false;

	if (!ptr_log_object)
		return false;

	PITEM_LOG ptr_log = (PITEM_LOG)ptr_log_object->pdata;

	if (ptr_log)
	{
		PR_OBJECT ptr_app_object = _app_getappitem (ptr_log->app_hash);

		if (!ptr_app_object)
			return false;

		PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

		if (!ptr_app)
		{
			_r_obj_dereference (ptr_app_object, &_app_dereferenceapp);
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

					_r_obj_dereference (ptr_signature_object, &_app_dereferencestring);
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
			const HDC hdc = GetDC (hwnd);

			const bool is_inbound = (ptr_log->direction == FWP_DIRECTION_INBOUND);

			_app_notifysettext (hdc, hwnd, IDC_FILE_ID, app.LocaleString (IDS_NAME, L":"), IDC_FILE_TEXT, (ptr_app->display_name && ptr_app->display_name[0]) ? _r_path_extractfile (ptr_app->display_name) : empty_text);
			_app_notifysettext (hdc, hwnd, IDC_SIGNATURE_ID, app.LocaleString (IDS_SIGNATURE, L":"), IDC_SIGNATURE_TEXT, is_signed.IsEmpty () ? empty_text : is_signed);
			_app_notifysettext (hdc, hwnd, IDC_ADDRESS_ID, app.LocaleString (IDS_ADDRESS, L":"), IDC_ADDRESS_TEXT, (ptr_log->addr_fmt && ptr_log->addr_fmt[0]) ? ptr_log->addr_fmt : empty_text);
			_app_notifysettext (hdc, hwnd, IDC_PORT_ID, app.LocaleString (IDS_PORT, L":"), IDC_PORT_TEXT, ptr_log->port ? _r_fmt (L"%" PRIu16 " (%s)", ptr_log->port, _app_getservicename (ptr_log->port).GetString ()) : empty_text);
			_app_notifysettext (hdc, hwnd, IDC_DIRECTION_ID, app.LocaleString (IDS_DIRECTION, L":"), IDC_DIRECTION_TEXT, app.LocaleString (is_inbound ? IDS_DIRECTION_2 : IDS_DIRECTION_1, ptr_log->is_loopback ? L" (Loopback)" : nullptr));
			_app_notifysettext (hdc, hwnd, IDC_FILTER_ID, app.LocaleString (IDS_FILTER, L":"), IDC_FILTER_TEXT, (ptr_log->filter_name && ptr_log->filter_name[0]) ? ptr_log->filter_name : empty_text);
			_app_notifysettext (hdc, hwnd, IDC_DATE_ID, app.LocaleString (IDS_DATE, L":"), IDC_DATE_TEXT, _r_fmt_date (ptr_log->date, FDTF_SHORTDATE | FDTF_LONGTIME));

			ReleaseDC (hwnd, hdc);
		}

		SetWindowLongPtr (GetDlgItem (hwnd, IDC_ICON_ID), GWLP_USERDATA, (LONG_PTR)ptr_log->hicon);
		SetWindowLongPtr (hwnd, GWLP_USERDATA, (LONG_PTR)ptr_log->app_hash);

		_r_ctrl_enable (hwnd, IDC_RULES_BTN, !is_safety);
		_r_ctrl_enable (hwnd, IDC_NEXT_BTN, !is_safety);
		_r_ctrl_enable (hwnd, IDC_ALLOW_BTN, !is_safety);
		_r_ctrl_enable (hwnd, IDC_BLOCK_BTN, !is_safety);

		ShowWindow (GetDlgItem (hwnd, IDC_NEXT_BTN), _app_notifyget_id (hwnd, LAST_VALUE) ? SW_SHOW : SW_HIDE);

		_r_ctrl_settext (hwnd, IDC_RULES_BTN, app.LocaleString (IDS_TRAY_RULES, nullptr));
		_r_ctrl_settext (hwnd, IDC_ALLOW_BTN, app.LocaleString (IDS_ACTION_ALLOW, nullptr));
		_r_ctrl_settext (hwnd, IDC_BLOCK_BTN, app.LocaleString (IDS_ACTION_BLOCK, nullptr));

		if (is_safety)
			SetTimer (hwnd, NOTIFY_TIMER_SAFETY_ID, NOTIFY_TIMER_SAFETY_TIMEOUT, nullptr);

		else
			KillTimer (hwnd, NOTIFY_TIMER_SAFETY_ID);

		_app_notifysetpos (hwnd, false);

		// prevent fullscreen apps lose focus
		if (is_forced && _r_wnd_isfullscreenmode ())
			is_forced = false;

		InvalidateRect (hwnd, nullptr, TRUE);

		ShowWindow (hwnd, is_forced ? SW_SHOW : SW_SHOWNA);

		return true;
	}

	return false;
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

	if (!notify_snd_path[0] || !_r_fs_exists (notify_snd_path))
	{
		HKEY hkey = nullptr;
		notify_snd_path[0] = 0;

#define NOTIFY_SOUND_NAME L"MailBeep"

		if (RegOpenKeyEx (HKEY_CURRENT_USER, L"AppEvents\\Schemes\\Apps\\.Default\\" NOTIFY_SOUND_NAME L"\\.Default", 0, KEY_READ, &hkey) == ERROR_SUCCESS)
		{
			DWORD size = _countof (notify_snd_path) * sizeof (WCHAR);

			if (RegQueryValueEx (hkey, nullptr, nullptr, nullptr, (LPBYTE)notify_snd_path, &size) == ERROR_SUCCESS)
			{
				const rstring path = _r_path_expand (notify_snd_path);

				if (_r_fs_exists (path))
				{
					StringCchCopy (notify_snd_path, _countof (notify_snd_path), path);
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

	if (!result || !_r_fs_exists (notify_snd_path) || !PlaySound (notify_snd_path, nullptr, SND_FILENAME | SND_ASYNC))
		PlaySound (NOTIFY_SOUND_NAME, nullptr, SND_ASYNC);
}

void _app_notifyrefresh (HWND hwnd, bool is_safety)
{
	if (!app.ConfigGet (L"IsNotificationsEnabled", true).AsBool () || !IsWindowVisible (hwnd))
		_app_notifyhide (config.hnotification);

	PR_OBJECT ptr_log_object = _app_notifyget_obj (_app_notifyget_id (hwnd, LAST_VALUE));

	if (!ptr_log_object)
	{
		_app_notifyhide (hwnd);
		return;
	}

	_app_notifyshow (hwnd, ptr_log_object, true, is_safety);

	_r_obj_dereference (ptr_log_object, &_app_dereferencelog);
}

void _app_notifysetpos (HWND hwnd, bool is_forced)
{
	if (!is_forced && IsWindowVisible (hwnd))
	{
		RECT windowRect = {0};
		GetWindowRect (hwnd, &windowRect);

		_r_wnd_adjustwindowrect (hwnd, &windowRect);

		SetWindowPos (hwnd, nullptr, windowRect.left, windowRect.top, 0, 0, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE | SWP_NOSIZE);

		return;
	}

	const bool is_intray = app.ConfigGet (L"IsNotificationsOnTray", false).AsBool ();

	if (is_intray)
	{
		RECT windowRect = {0};
		GetWindowRect (hwnd, &windowRect);

		RECT desktopRect = {0};
		SystemParametersInfo (SPI_GETWORKAREA, 0, &desktopRect, 0);

		APPBARDATA appbar = {0};

		appbar.cbSize = sizeof (appbar);
		appbar.hWnd = FindWindow (L"Shell_TrayWnd", nullptr);

		SHAppBarMessage (ABM_GETTASKBARPOS, &appbar);

		const INT border_x = GetSystemMetrics (SM_CXBORDER);
		const INT border_y = GetSystemMetrics (SM_CYBORDER);

		if (appbar.uEdge == ABE_LEFT)
		{
			windowRect.left = appbar.rc.right + border_x;
			windowRect.top = (desktopRect.bottom - _R_RECT_HEIGHT (&windowRect)) - border_y;
		}
		else if (appbar.uEdge == ABE_TOP)
		{
			windowRect.left = (desktopRect.right - _R_RECT_WIDTH (&windowRect)) - border_x;
			windowRect.top = appbar.rc.bottom + border_y;
		}
		else if (appbar.uEdge == ABE_RIGHT)
		{
			windowRect.left = (desktopRect.right - _R_RECT_WIDTH (&windowRect)) - border_x;
			windowRect.top = (desktopRect.bottom - _R_RECT_HEIGHT (&windowRect)) - border_y;
		}
		else/* if (appbar.uEdge == ABE_BOTTOM)*/
		{
			windowRect.left = (desktopRect.right - (windowRect.right - windowRect.left)) - border_x;
			windowRect.top = (desktopRect.bottom - _R_RECT_HEIGHT (&windowRect)) - border_y;
		}

		SetWindowPos (hwnd, nullptr, windowRect.left, windowRect.top, 0, 0, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE | SWP_NOSIZE);
	}
	else
	{
		_r_wnd_center (hwnd, nullptr);
	}
}

void _app_notifysettext (HDC hdc, HWND hwnd, UINT ctrl_id1, LPCWSTR text1, UINT ctrl_id2, LPCWSTR text2)
{
	RECT rc_wnd = {0};
	RECT rc_ctrl = {0};

	const HWND hctrl1 = GetDlgItem (hwnd, ctrl_id1);
	const HWND hctrl2 = GetDlgItem (hwnd, ctrl_id2);

	static const INT wnd_spacing = app.GetDPI (12);

	SelectObject (hdc, (HFONT)SendMessage (hctrl1, WM_GETFONT, 0, 0)); // fix
	SelectObject (hdc, (HFONT)SendMessage (hctrl2, WM_GETFONT, 0, 0)); // fix

	GetClientRect (hwnd, &rc_wnd);
	GetWindowRect (hctrl1, &rc_ctrl);

	MapWindowPoints (HWND_DESKTOP, hwnd, (LPPOINT)& rc_ctrl, 2);

	const INT wnd_width = _R_RECT_WIDTH (&rc_wnd) - (wnd_spacing * 2);

	INT ctrl1_width = _r_dc_fontwidth (hdc, text1, _r_str_length (text1));
	INT ctrl2_width = _r_dc_fontwidth (hdc, text2, _r_str_length (text2));

	ctrl1_width = min (ctrl1_width, wnd_width - ctrl2_width - wnd_spacing);
	ctrl2_width = min (ctrl2_width, wnd_width - ctrl1_width - wnd_spacing);

	HDWP hdefer = BeginDeferWindowPos (2);

	_r_wnd_resize (&hdefer, hctrl1, nullptr, wnd_spacing, rc_ctrl.top, ctrl1_width, _R_RECT_HEIGHT (&rc_ctrl), SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
	_r_wnd_resize (&hdefer, hctrl2, nullptr, wnd_width - ctrl2_width, rc_ctrl.top, ctrl2_width + wnd_spacing, _R_RECT_HEIGHT (&rc_ctrl), SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);

	EndDeferWindowPos (hdefer);

	SetWindowText (hctrl1, text1);
	SetWindowText (hctrl2, text2);
}

HFONT _app_notifyinitfont (PLOGFONT plf, LONG height, LONG weight, LPCWSTR name, BOOL is_underline)
{
	if (height)
		plf->lfHeight = _r_dc_fontsizetoheight (height);

	plf->lfWeight = weight;
	plf->lfUnderline = (BYTE)is_underline;

	plf->lfCharSet = DEFAULT_CHARSET;
	plf->lfQuality = DEFAULT_QUALITY;

	if (name)
		StringCchCopy (plf->lfFaceName, LF_FACESIZE, name);

	return CreateFontIndirect (plf);
}

void DrawGradient (HDC hdc, const LPRECT lprc, COLORREF rgb1, COLORREF rgb2, ULONG mode)
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

LRESULT CALLBACK NotificationProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
		case WM_CLOSE:
		{
			_app_notifyhide (hwnd);

			SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
			return TRUE;
		}

		case WM_TIMER:
		{
			if (wparam == NOTIFY_TIMER_SAFETY_ID)
			{
				_r_ctrl_enable (hwnd, IDC_RULES_BTN, true);
				_r_ctrl_enable (hwnd, IDC_NEXT_BTN, true);
				_r_ctrl_enable (hwnd, IDC_ALLOW_BTN, true);
				_r_ctrl_enable (hwnd, IDC_BLOCK_BTN, true);

				KillTimer (hwnd, wparam);

				break;
			}

			break;
		}

		case WM_KEYDOWN:
		{
			switch (wparam)
			{
				case VK_ESCAPE:
				{
					_app_notifyhide (hwnd);
					break;
				}
			}

			break;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps = {0};
			HDC hdc = BeginPaint (hwnd, &ps);

			RECT rc = {0};
			GetClientRect (hwnd, &rc);

			const INT wnd_width = _R_RECT_WIDTH (&rc);
			const INT wnd_height = _R_RECT_HEIGHT (&rc);
			const INT wnd_spacing = app.GetDPI (12);

			const INT icon_size = GetSystemMetrics (SM_CXICON);
			const INT header_size = app.GetDPI (NOTIFY_HEADER_HEIGHT);
			const INT footer_height = app.GetDPI (48);

			// draw header gradient
			{
				SetRect (&rc, 0, 0, wnd_width, header_size);
				DrawGradient (hdc, &rc, NOTIFY_GRADIENT_1, NOTIFY_GRADIENT_2, GRADIENT_FILL_RECT_H);
			}

			// draw header title
			{
				HGDIOBJ hprev = SelectObject (hdc, hfont_title);

				SetBkMode (hdc, TRANSPARENT); // HACK!!!
				SetTextColor (hdc, GetSysColor (COLOR_WINDOW));

				WCHAR text[128] = {0};
				StringCchPrintf (text, _countof (text), app.LocaleString (IDS_NOTIFY_HEADER, nullptr), APP_NAME);

				SetRect (&rc, wnd_spacing, 0, wnd_width - (wnd_spacing * 2) - icon_size, header_size);
				DrawTextEx (hdc, text, (INT)_r_str_length (text), &rc, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOCLIP | DT_NOPREFIX, nullptr);

				SelectObject (hdc, hprev);
			}

			// draw header icon
			{
				HICON hicon = (HICON)GetWindowLongPtr (GetDlgItem (hwnd, IDC_ICON_ID), GWLP_USERDATA);

				if (hicon)
				{
					SetRect (&rc, wnd_width - icon_size - wnd_spacing, (header_size / 2) - (icon_size / 2), icon_size, icon_size);
					DrawIconEx (hdc, rc.left, rc.top, hicon, rc.right, rc.bottom, 0, nullptr, DI_IMAGE | DI_MASK);
				}
			}

			// draw footer
			{
				SetRect (&rc, 0, wnd_height - footer_height, wnd_width, wnd_height);
				_r_dc_fillrect (hdc, &rc, GetSysColor (COLOR_3DFACE));

				for (LONG i = 0; i < rc.right; i++)
					SetPixel (hdc, i, rc.top, GetSysColor (COLOR_APPWORKSPACE));
			}

			EndPaint (hwnd, &ps);

			break;
		}

		case WM_CTLCOLORSTATIC:
		{
			HDC hdc = (HDC)wparam;
			UINT ctrl_id = GetDlgCtrlID ((HWND)lparam);

			SetBkMode (hdc, TRANSPARENT); // HACK!!!

			if (ctrl_id == IDC_FILE_TEXT)
				SetTextColor (hdc, GetSysColor (COLOR_HIGHLIGHT));

			else
				SetTextColor (hdc, GetSysColor (COLOR_WINDOWTEXT));

			return (INT_PTR)GetSysColorBrush (COLOR_WINDOW);
		}

#ifndef _APP_NO_DARKTHEME
		case WM_SETTINGCHANGE:
		case WM_SYSCOLORCHANGE:
		{
			_r_wnd_setdarktheme (hwnd);
			break;
		}
#endif // _APP_NO_DARKTHEME

		case WM_SETCURSOR:
		{
			const UINT ctrl_id = GetDlgCtrlID ((HWND)wparam);

			if (ctrl_id == IDC_FILE_TEXT)
			{
				SetCursor (LoadCursor (nullptr, IDC_HAND));

				SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
				return TRUE;
			}

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lparam;

			switch (nmlp->code)
			{
				case BCN_DROPDOWN:
				{
					if (nmlp->idFrom != IDC_RULES_BTN && nmlp->idFrom != IDC_ALLOW_BTN && !_r_ctrl_isenabled (hwnd, (UINT)nmlp->idFrom))
						break;

					const HMENU hsubmenu = CreatePopupMenu ();

					if (nmlp->idFrom == IDC_RULES_BTN)
					{
						AppendMenu (hsubmenu, MF_STRING, IDM_DISABLENOTIFICATIONS, app.LocaleString (IDS_DISABLENOTIFICATIONS, nullptr));

						_app_generate_rulesmenu (hsubmenu, _app_notifyget_id (hwnd, 0));
					}
					else if (nmlp->idFrom == IDC_ALLOW_BTN)
					{
						for (size_t i = 0; i < timers.size (); i++)
							AppendMenu (hsubmenu, MF_BYPOSITION, IDX_TIMER + UINT (i), _r_fmt_interval (timers.at (i) + 1, 1));
					}

					RECT buttonRect = {0};

					GetClientRect (nmlp->hwndFrom, &buttonRect);
					ClientToScreen (nmlp->hwndFrom, (LPPOINT)& buttonRect);

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
					const UINT ctrl_id = GetDlgCtrlID ((HWND)lpnmdi->hdr.idFrom);

					if (ctrl_id == IDC_FILE_TEXT)
					{
						PR_OBJECT ptr_log_object = _app_notifyget_obj (_app_notifyget_id (hwnd, 0));

						if (ptr_log_object)
						{
							PITEM_LOG ptr_log = (PITEM_LOG)ptr_log_object->pdata;

							if (ptr_log && ptr_log->app_hash)
								StringCchCopy (buffer, _countof (buffer), _app_gettooltip (IDC_APPS_PROFILE, ptr_log->app_hash));

							_r_obj_dereference (ptr_log_object, &_app_dereferencelog);
						}
					}
					else
					{
						StringCchCopy (buffer, _countof (buffer), _r_ctrl_gettext (hwnd, ctrl_id));
					}

					if (buffer[0])
						lpnmdi->lpszText = buffer;

					break;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			if ((LOWORD (wparam) >= IDX_RULES_SPECIAL && LOWORD (wparam) <= IDX_RULES_SPECIAL + rules_arr.size ()))
			{
				const size_t rule_idx = (LOWORD (wparam) - IDX_RULES_SPECIAL);
				PR_OBJECT ptr_rule_object = _app_getrulebyid (rule_idx);

				if (!ptr_rule_object)
					break;

				const PITEM_RULE ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

				if (ptr_rule)
				{
					PR_OBJECT ptr_log_object = _app_notifyget_obj (_app_notifyget_id (hwnd, 0));

					if (ptr_log_object)
					{
						PITEM_LOG ptr_log = (PITEM_LOG)ptr_log_object->pdata;

						if (ptr_log)
						{
							_app_freenotify (ptr_log->app_hash, true);

							if (ptr_log->app_hash && !(ptr_rule->is_forservices && (ptr_log->app_hash == config.ntoskrnl_hash || ptr_log->app_hash == config.svchost_hash)))
							{
								PR_OBJECT ptr_app_object = _app_getappitem (ptr_log->app_hash);

								if (ptr_app_object)
								{
									PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

									if (ptr_app)
									{
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

										const UINT listview_id = _app_gettab_id (app.GetHWND ());
										const UINT app_listview_id = _app_getlistview_id (ptr_app->type);
										const UINT rule_listview_id = _app_getlistview_id (ptr_rule->type);

										{
											const size_t app_item = _app_getposition (app.GetHWND (), app_listview_id, ptr_log->app_hash);

											if (app_item != LAST_VALUE)
											{
												_r_fastlock_acquireshared (&lock_checkbox);
												_app_setappiteminfo (app.GetHWND (), app_listview_id, app_item, ptr_log->app_hash, ptr_app);
												_r_fastlock_releaseshared (&lock_checkbox);
											}
										}

										{
											const size_t rule_item = _app_getposition (app.GetHWND (), rule_listview_id, rule_idx);

											if (rule_item != LAST_VALUE)
											{
												_r_fastlock_acquireshared (&lock_checkbox);
												_app_setruleiteminfo (app.GetHWND (), rule_listview_id, rule_item, ptr_rule, false);
												_r_fastlock_releaseshared (&lock_checkbox);
											}
										}

										OBJECTS_VEC rules;
										rules.push_back (ptr_rule_object);

										_wfp_create4filters (rules, __LINE__);

										if (listview_id == app_listview_id || listview_id == rule_listview_id)
										{
											_app_listviewsort (app.GetHWND (), listview_id);
											_r_listview_redraw (app.GetHWND (), listview_id);
										}

										_app_refreshstatus (app.GetHWND ());
										_app_profile_save ();
									}

									_r_obj_dereference (ptr_app_object, &_app_dereferenceapp);
								}
							}
						}

						_r_obj_dereference (ptr_log_object, &_app_dereferencelog);
					}
				}

				_r_obj_dereference (ptr_rule_object, &_app_dereferencerule);

				break;
			}
			else if ((LOWORD (wparam) >= IDX_TIMER && LOWORD (wparam) <= IDX_TIMER + timers.size ()))
			{
				if (!_r_ctrl_isenabled (hwnd, IDC_ALLOW_BTN))
					break;

				const size_t timer_idx = (LOWORD (wparam) - IDX_TIMER);

				_app_notifycommand (hwnd, IDC_ALLOW_BTN, timers.at (timer_idx));

				break;
			}

			switch (LOWORD (wparam))
			{
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
									const UINT listview_id = _app_getlistview_id (ptr_app->type);

									if (listview_id)
										_app_showitem (app.GetHWND (), listview_id, _app_getposition (app.GetHWND (), listview_id, ptr_log->app_hash));

									_r_wnd_toggle (app.GetHWND (), true);
								}

								_r_obj_dereference (ptr_app_object, &_app_dereferenceapp);
							}
						}

						_r_obj_dereference (ptr_log_object, &_app_dereferencelog);
					}

					break;
				}

				case IDC_RULES_BTN:
				{
					const UINT ctrl_id = LOWORD (wparam);

					if (_r_ctrl_isenabled (hwnd, ctrl_id))
					{
						// HACK!!!
						NMHDR hdr = {0};

						hdr.code = BCN_DROPDOWN;
						hdr.idFrom = ctrl_id;
						hdr.hwndFrom = GetDlgItem (hwnd, ctrl_id);

						SendMessage (hwnd, WM_NOTIFY, TRUE, (LPARAM)& hdr);
					}

					break;
				}

				case IDC_NEXT_BTN:
				{
					PR_OBJECT ptr_log_object = _app_notifyget_obj (_app_notifyget_id (hwnd, LAST_VALUE));

					if (ptr_log_object)
					{
						_app_notifyshow (hwnd, ptr_log_object, true, true);
						_r_obj_dereference (ptr_log_object, &_app_dereferencelog);
					}

					break;
				}

				case IDC_ALLOW_BTN:
				case IDC_BLOCK_BTN:
				case IDM_DISABLENOTIFICATIONS:
				{
					const UINT ctrl_id = LOWORD (wparam);

					if (_r_ctrl_isenabled (hwnd, ctrl_id))
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
					PR_OBJECT ptr_rule_object = _r_obj_allocate (ptr_rule);

					size_t app_hash = 0;
					PR_OBJECT ptr_log_object = _app_notifyget_obj (_app_notifyget_id (hwnd, 0));

					if (ptr_log_object)
					{
						PITEM_LOG ptr_log = (PITEM_LOG)ptr_log_object->pdata;

						if (ptr_log)
						{
							app_hash = ptr_log->app_hash;

							ptr_rule->apps[ptr_log->app_hash] = true;
							ptr_rule->protocol = ptr_log->protocol;

							_app_freenotify (app_hash, true);

							LPWSTR prule = nullptr;

							if (_app_formataddress (ptr_log->af, 0, &ptr_log->addr, ptr_log->port, &prule, FMTADDR_AS_RULE))
							{
								_r_str_alloc (&ptr_rule->pname, _r_str_length (prule), prule);
								_r_str_alloc (&ptr_rule->prule_remote, _r_str_length (prule), prule);
							}

							SAFE_DELETE_ARRAY (prule);
						}

						_r_obj_dereference (ptr_log_object, &_app_dereferencelog);
					}
					else
					{
						_r_obj_dereference (ptr_rule_object, &_app_dereferencerule);
						break;
					}

					ptr_rule->type = DataRuleCustom;
					ptr_rule->is_block = false;

					_app_ruleenable (ptr_rule, true);

					if (DialogBoxParam (nullptr, MAKEINTRESOURCE (IDD_EDITOR), app.GetHWND (), &EditorProc, (LPARAM)ptr_rule_object))
					{
						_r_fastlock_acquireexclusive (&lock_access);

						const size_t rule_idx = rules_arr.size ();
						rules_arr.push_back (ptr_rule_object);

						_r_fastlock_releaseexclusive (&lock_access);

						const UINT listview_id = _app_gettab_id (app.GetHWND ());

						// set rule information
						{
							const UINT rules_listview_id = _app_getlistview_id (ptr_rule->type);

							if (rules_listview_id)
							{
								const size_t new_item = _r_listview_getitemcount (hwnd, rules_listview_id);

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
									const UINT app_listview_id = _app_getlistview_id (ptr_app->type);

									if (app_listview_id)
									{
										_r_fastlock_acquireshared (&lock_checkbox);
										_app_setappiteminfo (app.GetHWND (), app_listview_id, _app_getposition (app.GetHWND (), app_listview_id, app_hash), app_hash, ptr_app);
										_r_fastlock_releaseshared (&lock_checkbox);

										if (app_listview_id == listview_id)
											_app_listviewsort (app.GetHWND (), listview_id);
									}
								}

								_r_obj_dereference (ptr_app_object, &_app_dereferenceapp);
							}
						}

						_app_refreshstatus (app.GetHWND ());
						_app_profile_save ();
					}
					else
					{
						_r_obj_dereference (ptr_rule_object, &_app_dereferencerule);
					}

					break;
				}
			}

			break;
		}
	}

	return DefWindowProc (hwnd, msg, wparam, lparam);
}
