// simplewall
// Copyright (c) 2016-2019 Henry++

#include "global.hpp"

void _app_notifycreatewindow ()
{
	WNDCLASSEX wcex = {0};

	wcex.cbSize = sizeof (wcex);
	wcex.style = CS_VREDRAW | CS_HREDRAW;
	wcex.hInstance = app.GetHINSTANCE ();
	wcex.hCursor = LoadCursor (nullptr, IDC_ARROW);
	wcex.hbrBackground = GetSysColorBrush (COLOR_WINDOW);
	wcex.lpszClassName = NOTIFY_CLASS_DLG;
	wcex.lpfnWndProc = &NotificationProc;

	if (!RegisterClassEx (&wcex))
		return;

	static const UINT wnd_width = app.GetDPI (NOTIFY_WIDTH);
	static const UINT wnd_height = app.GetDPI (NOTIFY_HEIGHT);

	static const INT title_font_height = 10;
	static const INT text_font_height = 9;

	static const INT cxsmIcon = GetSystemMetrics (SM_CXSMICON);
	static const INT IconSize = app.GetDPI (20);

	config.hnotification = CreateWindowEx (WS_EX_TOPMOST | WS_EX_TOOLWINDOW, NOTIFY_CLASS_DLG, nullptr, WS_POPUP, 0, 0, wnd_width, wnd_height, nullptr, nullptr, wcex.hInstance, nullptr);

	if (!config.hnotification)
		return;

	HFONT hfont_title = nullptr;
	HFONT hfont_main = nullptr;
	HFONT hfont_text = nullptr;

	// load system font
	{
		NONCLIENTMETRICS ncm = {0};
		ncm.cbSize = sizeof (ncm);

		if (SystemParametersInfo (SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0))
		{
			hfont_title = _app_notifyinitfont (&ncm.lfCaptionFont, title_font_height, FW_NORMAL, UI_FONT_NOTIFICATION, false);
			hfont_main = _app_notifyinitfont (&ncm.lfMessageFont, text_font_height, FW_NORMAL, UI_FONT_NOTIFICATION, true);
			hfont_text = _app_notifyinitfont (&ncm.lfMessageFont, text_font_height, FW_NORMAL, UI_FONT_NOTIFICATION, false);
		}
	}

	HWND hctrl = CreateWindow (WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_CENTER | SS_ICON, app.GetDPI (6), app.GetDPI (4), IconSize, IconSize, config.hnotification, (HMENU)IDC_ICON_ID, nullptr, nullptr);
	SendMessage (hctrl, STM_SETIMAGE, IMAGE_ICON, (WPARAM)app.GetSharedImage (app.GetHINSTANCE (), IDI_MAIN, cxsmIcon));

	hctrl = CreateWindow (WC_STATIC, APP_NAME, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_WORDELLIPSIS, IconSize + app.GetDPI (10), app.GetDPI (4), wnd_width - app.GetDPI (150), IconSize, config.hnotification, (HMENU)IDC_TITLE_ID, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_title, MAKELPARAM (TRUE, 0));

	hctrl = CreateWindow (WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_BITMAP | SS_NOTIFY, wnd_width - IconSize * 4 - app.GetDPI (18), app.GetDPI (4), IconSize, IconSize, config.hnotification, (HMENU)IDC_MENU_BTN, nullptr, nullptr);
	SendMessage (hctrl, STM_SETIMAGE, IMAGE_BITMAP, (WPARAM)_app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_MENU), cxsmIcon));

	hctrl = CreateWindow (WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_BITMAP | SS_NOTIFY, wnd_width - IconSize * 3 - app.GetDPI (14), app.GetDPI (4), IconSize, IconSize, config.hnotification, (HMENU)IDC_TIMER_BTN, nullptr, nullptr);
	SendMessage (hctrl, STM_SETIMAGE, IMAGE_BITMAP, (WPARAM)_app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_TIMER), cxsmIcon));

	hctrl = CreateWindow (WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_BITMAP | SS_NOTIFY, wnd_width - IconSize * 2 - app.GetDPI (10), app.GetDPI (4), IconSize, IconSize, config.hnotification, (HMENU)IDC_RULES_BTN, nullptr, nullptr);
	SendMessage (hctrl, STM_SETIMAGE, IMAGE_BITMAP, (WPARAM)_app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_RULES), cxsmIcon));

	hctrl = CreateWindow (WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_BITMAP | SS_NOTIFY, wnd_width - IconSize - app.GetDPI (6), app.GetDPI (4), IconSize, IconSize, config.hnotification, (HMENU)IDC_CLOSE_BTN, nullptr, nullptr);
	SendMessage (hctrl, STM_SETIMAGE, IMAGE_BITMAP, (WPARAM)_app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_CLOSE), cxsmIcon));

	hctrl = CreateWindow (WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, app.GetDPI (12), app.GetDPI (44), wnd_width - app.GetDPI (24), app.GetDPI (16), config.hnotification, (HMENU)IDC_FILE_ID, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_text, MAKELPARAM (TRUE, 0));

	hctrl = CreateWindow (WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_RIGHT | SS_NOTIFY, app.GetDPI (12), app.GetDPI (44), wnd_width - app.GetDPI (24), app.GetDPI (16), config.hnotification, (HMENU)IDC_FILE_TEXT, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_main, MAKELPARAM (TRUE, 0));

	hctrl = CreateWindow (WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, app.GetDPI (12), app.GetDPI (64), wnd_width - app.GetDPI (24), app.GetDPI (16), config.hnotification, (HMENU)IDC_SIGNATURE_ID, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_text, MAKELPARAM (TRUE, 0));

	hctrl = CreateWindow (WC_EDIT, nullptr, WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL | ES_RIGHT, app.GetDPI (12), app.GetDPI (64), wnd_width - app.GetDPI (24), app.GetDPI (16), config.hnotification, (HMENU)IDC_SIGNATURE_TEXT, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_text, MAKELPARAM (TRUE, 0));
	SendMessage (hctrl, EM_SETMARGINS, EC_LEFTMARGIN, 0);
	SendMessage (hctrl, EM_SETMARGINS, EC_RIGHTMARGIN, 0);

	hctrl = CreateWindow (WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, app.GetDPI (12), app.GetDPI (84), wnd_width - app.GetDPI (24), app.GetDPI (16), config.hnotification, (HMENU)IDC_ADDRESS_REMOTE_ID, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_text, MAKELPARAM (TRUE, 0));

	hctrl = CreateWindow (WC_EDIT, nullptr, WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL | ES_RIGHT, app.GetDPI (12), app.GetDPI (84), wnd_width - app.GetDPI (24), app.GetDPI (16), config.hnotification, (HMENU)IDC_ADDRESS_REMOTE_TEXT, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_text, MAKELPARAM (TRUE, 0));
	SendMessage (hctrl, EM_SETMARGINS, EC_LEFTMARGIN, 0);
	SendMessage (hctrl, EM_SETMARGINS, EC_RIGHTMARGIN, 0);

	hctrl = CreateWindow (WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, app.GetDPI (12), app.GetDPI (104), wnd_width - app.GetDPI (24), app.GetDPI (16), config.hnotification, (HMENU)IDC_ADDRESS_LOCAL_ID, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_text, MAKELPARAM (TRUE, 0));

	hctrl = CreateWindow (WC_EDIT, nullptr, WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL | ES_RIGHT, app.GetDPI (12), app.GetDPI (104), wnd_width - app.GetDPI (24), app.GetDPI (16), config.hnotification, (HMENU)IDC_ADDRESS_LOCAL_TEXT, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_text, MAKELPARAM (TRUE, 0));
	SendMessage (hctrl, EM_SETMARGINS, EC_LEFTMARGIN, 0);
	SendMessage (hctrl, EM_SETMARGINS, EC_RIGHTMARGIN, 0);

	hctrl = CreateWindow (WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, app.GetDPI (12), app.GetDPI (124), wnd_width - app.GetDPI (24), app.GetDPI (16), config.hnotification, (HMENU)IDC_PROTOCOL_ID, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_text, MAKELPARAM (TRUE, 0));

	hctrl = CreateWindow (WC_EDIT, nullptr, WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL | ES_RIGHT, app.GetDPI (12), app.GetDPI (124), wnd_width - app.GetDPI (24), app.GetDPI (16), config.hnotification, (HMENU)IDC_PROTOCOL_TEXT, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_text, MAKELPARAM (TRUE, 0));
	SendMessage (hctrl, EM_SETMARGINS, EC_LEFTMARGIN, 0);
	SendMessage (hctrl, EM_SETMARGINS, EC_RIGHTMARGIN, 0);

	hctrl = CreateWindow (WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, app.GetDPI (12), app.GetDPI (144), wnd_width - app.GetDPI (24), app.GetDPI (16), config.hnotification, (HMENU)IDC_FILTER_ID, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_text, MAKELPARAM (TRUE, 0));

	hctrl = CreateWindow (WC_EDIT, nullptr, WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL | ES_RIGHT, app.GetDPI (12), app.GetDPI (144), wnd_width - app.GetDPI (24), app.GetDPI (16), config.hnotification, (HMENU)IDC_FILTER_TEXT, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_text, MAKELPARAM (TRUE, 0));
	SendMessage (hctrl, EM_SETMARGINS, EC_LEFTMARGIN, 0);
	SendMessage (hctrl, EM_SETMARGINS, EC_RIGHTMARGIN, 0);

	hctrl = CreateWindow (WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, app.GetDPI (12), app.GetDPI (164), wnd_width - app.GetDPI (24), app.GetDPI (16), config.hnotification, (HMENU)IDC_DATE_ID, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_text, MAKELPARAM (TRUE, 0));

	hctrl = CreateWindow (WC_EDIT, nullptr, WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL | ES_RIGHT, app.GetDPI (12), app.GetDPI (164), wnd_width - app.GetDPI (24), app.GetDPI (16), config.hnotification, (HMENU)IDC_DATE_TEXT, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_text, MAKELPARAM (TRUE, 0));
	SendMessage (hctrl, EM_SETMARGINS, EC_LEFTMARGIN, 0);
	SendMessage (hctrl, EM_SETMARGINS, EC_RIGHTMARGIN, 0);

	static const UINT btn_height = 54;

	hctrl = CreateWindow (WC_BUTTON, nullptr, WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_COMMANDLINK | BS_BITMAP, app.GetDPI (8), wnd_height - app.GetDPI (btn_height * 3 + 19), wnd_width - app.GetDPI (8 * 2), app.GetDPI (btn_height), config.hnotification, (HMENU)IDC_ALLOW_BTN, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_text, MAKELPARAM (TRUE, 0));
	SendMessage (hctrl, BM_SETIMAGE, IMAGE_BITMAP, (WPARAM)_app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_ALLOW), IconSize));

	hctrl = CreateWindow (WC_BUTTON, nullptr, WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_COMMANDLINK | BS_BITMAP, app.GetDPI (8), wnd_height - app.GetDPI (btn_height * 2 + 16), wnd_width - app.GetDPI (8 * 2), app.GetDPI (btn_height), config.hnotification, (HMENU)IDC_BLOCK_BTN, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_text, MAKELPARAM (TRUE, 0));
	SendMessage (hctrl, BM_SETIMAGE, IMAGE_BITMAP, (WPARAM)_app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_BLOCK), IconSize));

	hctrl = CreateWindow (WC_BUTTON, nullptr, WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_COMMANDLINK | BS_BITMAP, app.GetDPI (8), wnd_height - app.GetDPI (btn_height + 12), wnd_width - app.GetDPI (8 * 2), app.GetDPI (btn_height), config.hnotification, (HMENU)IDC_LATER_BTN, nullptr, nullptr);
	SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_text, MAKELPARAM (TRUE, 0));
	SendMessage (hctrl, BM_SETIMAGE, IMAGE_BITMAP, (WPARAM)_app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_BLOCK), IconSize));

	const HWND htip = _r_ctrl_createtip (config.hnotification);

	_r_ctrl_settip (htip, config.hnotification, IDC_MENU_BTN, LPSTR_TEXTCALLBACK);
	_r_ctrl_settip (htip, config.hnotification, IDC_RULES_BTN, LPSTR_TEXTCALLBACK);
	_r_ctrl_settip (htip, config.hnotification, IDC_TIMER_BTN, LPSTR_TEXTCALLBACK);
	_r_ctrl_settip (htip, config.hnotification, IDC_CLOSE_BTN, LPSTR_TEXTCALLBACK);

	_r_ctrl_settip (htip, config.hnotification, IDC_FILE_TEXT, LPSTR_TEXTCALLBACK);
	_r_ctrl_settip (htip, config.hnotification, IDC_SIGNATURE_TEXT, LPSTR_TEXTCALLBACK);
	_r_ctrl_settip (htip, config.hnotification, IDC_ADDRESS_LOCAL_TEXT, LPSTR_TEXTCALLBACK);
	_r_ctrl_settip (htip, config.hnotification, IDC_ADDRESS_REMOTE_TEXT, LPSTR_TEXTCALLBACK);
	_r_ctrl_settip (htip, config.hnotification, IDC_PROTOCOL_TEXT, LPSTR_TEXTCALLBACK);
	_r_ctrl_settip (htip, config.hnotification, IDC_FILTER_TEXT, LPSTR_TEXTCALLBACK);
	_r_ctrl_settip (htip, config.hnotification, IDC_DATE_TEXT, LPSTR_TEXTCALLBACK);

	_app_notifyhide (config.hnotification);
}

bool _app_notifycommand (HWND hwnd, UINT ctrl_id, size_t timer_idx)
{
	_r_fastlock_acquireexclusive (&lock_notification);

	size_t app_hash = 0;
	const size_t notify_idx = _app_notifygetcurrent (hwnd);

	if (notify_idx != LAST_VALUE)
	{
		PITEM_LOG ptr_log = notifications.at (notify_idx);

		if (ptr_log)
			app_hash = ptr_log->hash;

		_app_freenotify (notify_idx, true, false);

		if (app_hash)
			_app_freenotify (app_hash, false, false);

		_app_notifyrefresh (hwnd, true);
	}

	_r_fastlock_releaseexclusive (&lock_notification);

	if (!app_hash)
		return false;

	PITEM_APP ptr_app = _app_getapplication (app_hash);

	if (!ptr_app)
		return false;

	MFILTER_APPS rules;

	const UINT listview_id = _app_getlistview_id (ptr_app->type);
	const size_t item = _app_getposition (app.GetHWND (), listview_id, app_hash);

	_r_fastlock_acquireexclusive (&lock_access);

	if (ctrl_id == IDC_ALLOW_BTN || ctrl_id == IDC_BLOCK_BTN)
	{
		ptr_app->is_enabled = (ctrl_id == IDC_ALLOW_BTN);
		ptr_app->is_silent = (ctrl_id == IDC_BLOCK_BTN);

		if (item != LAST_VALUE)
		{
			_r_fastlock_acquireshared (&lock_checkbox);

			_r_listview_setitem (app.GetHWND (), listview_id, item, 0, nullptr, LAST_VALUE, _app_getappgroup (app_hash, ptr_app));
			_r_listview_setitemcheck (app.GetHWND (), listview_id, item, ptr_app->is_enabled);

			_r_fastlock_releaseshared (&lock_checkbox);
		}

		rules.push_back (ptr_app);
	}
	else if (ctrl_id == IDM_DISABLENOTIFICATIONS)
	{
		ptr_app->is_silent = true;
	}
	else if (ctrl_id == IDC_LATER_BTN)
	{
		// TODO
	}

	ptr_app->last_notify = _r_unixtime_now ();

	_r_fastlock_releaseexclusive (&lock_access);

	// create filters
	if (timer_idx != LAST_VALUE)
		_app_timer_create (app.GetHWND (), rules, timers.at (timer_idx));

	else
		_wfp_create3filters (rules, __LINE__);

	_app_refreshstatus (app.GetHWND ());
	_app_profile_save ();

	if (_app_gettab_id (app.GetHWND ()) == listview_id)
	{
		_app_listviewsort (app.GetHWND (), listview_id);
		_r_listview_redraw (app.GetHWND (), listview_id);
	}

	_r_fastlock_releaseexclusive (&lock_access);

	return true;

}

bool _app_notifyadd (HWND hwnd, PITEM_LOG const ptr_log, PITEM_APP const ptr_app)
{
	if (!ptr_app || !ptr_log)
		return false;

	const time_t current_time = _r_unixtime_now ();

	// check for last display time
	{
		const time_t notification_timeout = app.ConfigGet (L"NotificationsTimeout", NOTIFY_TIMEOUT_DEFAULT).AsLonglong ();

		if (notification_timeout && ((current_time - ptr_app->last_notify) < notification_timeout))
			return false;
	}

	_r_fastlock_acquireexclusive (&lock_notification);

	ptr_app->last_notify = current_time;

	_app_freenotify (ptr_log->hash, false, false); // remove existing log item (if exists)

	// prevent notifications overflow
	if ((notifications.size () >= NOTIFY_LIMIT_SIZE))
		_app_freenotify (0, true, false);

	notifications.push_back (ptr_log);
	size_t idx = notifications.size () - 1;

	_r_fastlock_releaseexclusive (&lock_notification);

	if (app.ConfigGet (L"IsNotificationsSound", true).AsBool ())
		_app_notifyplaysound ();

	if (!_r_wnd_undercursor (hwnd) && _app_notifyshow (hwnd, idx, true, true))
	{
		const UINT display_timeout = app.ConfigGet (L"NotificationsDisplayTimeout", NOTIFY_TIMER_DEFAULT).AsUint ();

		if (display_timeout)
			_app_notifysettimeout (hwnd, NOTIFY_TIMER_TIMEOUT_ID, true, (display_timeout * _R_SECONDSCLOCK_MSEC));
	}

	return true;
}

void _app_freenotify (size_t idx_orhash, bool is_idx, bool is_refresh)
{
	const size_t count = notifications.size ();

	if (!count)
		return;

	if (is_idx)
	{
		if (idx_orhash != LAST_VALUE)
		{
			PITEM_LOG ptr_log = notifications.at (idx_orhash);

			SAFE_DELETE (ptr_log);

			notifications.erase (notifications.begin () + idx_orhash);
		}
	}
	else
	{
		if (idx_orhash)
		{
			for (size_t i = (count - 1); i != LAST_VALUE; i--)
			{
				PITEM_LOG ptr_log = notifications.at (i);

				if (!ptr_log || ptr_log->hash == idx_orhash)
				{
					SAFE_DELETE (ptr_log);

					notifications.erase (notifications.begin () + i);
				}
			}
		}
	}

	if (is_refresh)
		_app_notifyrefresh (config.hnotification, true);
}

size_t _app_notifygetcurrent (HWND hwnd)
{
	size_t new_idx = LAST_VALUE;
	const size_t count = notifications.size ();

	if (count)
	{
		if (count == 1)
		{
			new_idx = 0;
		}
		else
		{
			const size_t idx = (size_t)GetWindowLongPtr (hwnd, GWLP_USERDATA);
			new_idx = max (0, min (idx, count - 1));
		}
	}

	SetWindowLongPtr (hwnd, GWLP_USERDATA, new_idx);

	return new_idx;
}

bool _app_notifyshow (HWND hwnd, size_t idx, bool is_forced, bool is_safety)
{
	if (!app.ConfigGet (L"IsNotificationsEnabled", true).AsBool () || notifications.empty () || idx == LAST_VALUE)
	{
		if (notifications.empty () || idx == LAST_VALUE)
			SetWindowLongPtr (hwnd, GWLP_USERDATA, LAST_VALUE);

		return false;
	}

	// destroy safety timer to prevent race condition
	KillTimer (hwnd, NOTIFY_TIMER_SAFETY_ID);

	// prevent fullscreen apps lose focus
	if (is_forced && _r_wnd_isfullscreenmode ())
		is_forced = false;

	const size_t total_size = notifications.size ();

	idx = (total_size == 1) ? 0 : max (0, min (idx, total_size - 1));

	PITEM_LOG const ptr_log = notifications.at (idx);

	if (ptr_log)
	{
		PITEM_APP const ptr_app = _app_getapplication (ptr_log->hash);

		if (ptr_app)
		{
			SetWindowLongPtr (hwnd, GWLP_USERDATA, idx);

			rstring is_signed;
			const rstring empty_text = app.LocaleString (IDS_STATUS_EMPTY, nullptr);

			if (app.ConfigGet (L"IsCertificatesEnabled", false).AsBool ())
			{
				if (ptr_app->is_signed)
					is_signed = (ptr_app->signer && ptr_app->signer[0]) ? ptr_app->signer : app.LocaleString (IDS_SIGN_SIGNED, nullptr);

				else
					is_signed = app.LocaleString (IDS_SIGN_UNSIGNED, nullptr);
			}

			_r_ctrl_settext (hwnd, IDC_TITLE_ID, APP_NAME);

			{
				const HDC hdc = GetDC (hwnd);

				_app_notifysettext (hdc, hwnd, IDC_FILE_ID, app.LocaleString (IDS_NAME, L":"), IDC_FILE_TEXT, (ptr_app->display_name && ptr_app->display_name[0]) ? _r_path_extractfile (ptr_app->display_name) : empty_text);
				_app_notifysettext (hdc, hwnd, IDC_SIGNATURE_ID, app.LocaleString (IDS_SIGNATURE, L":"), IDC_SIGNATURE_TEXT, is_signed.IsEmpty () ? empty_text : is_signed);
				_app_notifysettext (hdc, hwnd, IDC_ADDRESS_REMOTE_ID, app.LocaleString (IDS_ADDRESS_REMOTE, L":"), IDC_ADDRESS_REMOTE_TEXT, (ptr_log->remote_fmt && ptr_log->remote_fmt[0]) ? ptr_log->remote_fmt : empty_text);
				_app_notifysettext (hdc, hwnd, IDC_ADDRESS_LOCAL_ID, app.LocaleString (IDS_ADDRESS_LOCAL, L":"), IDC_ADDRESS_LOCAL_TEXT, (ptr_log->local_fmt && ptr_log->local_fmt[0]) ? ptr_log->local_fmt : empty_text);
				_app_notifysettext (hdc, hwnd, IDC_PROTOCOL_ID, app.LocaleString (IDS_PROTOCOL, L":"), IDC_PROTOCOL_TEXT, _app_getprotoname (ptr_log->protocol));
				_app_notifysettext (hdc, hwnd, IDC_FILTER_ID, app.LocaleString (IDS_FILTER, L":"), IDC_FILTER_TEXT, (ptr_log->filter_name && ptr_log->filter_name[0]) ? ptr_log->filter_name : empty_text);
				_app_notifysettext (hdc, hwnd, IDC_DATE_ID, app.LocaleString (IDS_DATE, L":"), IDC_DATE_TEXT, _r_fmt_date (ptr_log->date, FDTF_SHORTDATE | FDTF_LONGTIME));

				ReleaseDC (hwnd, hdc);
			}

			_r_ctrl_settext (hwnd, IDC_ALLOW_BTN, app.LocaleString (IDS_ACTION_ALLOW, nullptr));
			_r_ctrl_settext (hwnd, IDC_BLOCK_BTN, app.LocaleString (IDS_ACTION_BLOCK, nullptr));
			_r_ctrl_settext (hwnd, IDC_LATER_BTN, app.LocaleString (IDS_ACTION_LATER, nullptr));

			_app_notifysetnote (hwnd, IDC_ALLOW_BTN, app.LocaleString (IDS_ACTION_ALLOW_HINT, nullptr));
			_app_notifysetnote (hwnd, IDC_BLOCK_BTN, app.LocaleString (IDS_ACTION_BLOCK_HINT, nullptr));
			_app_notifysetnote (hwnd, IDC_LATER_BTN, app.LocaleString (IDS_ACTION_LATER_HINT, nullptr));

			_app_notifysetpos (hwnd);

			_r_ctrl_enable (hwnd, IDC_ALLOW_BTN, !is_safety);
			_r_ctrl_enable (hwnd, IDC_BLOCK_BTN, !is_safety);
			_r_ctrl_enable (hwnd, IDC_LATER_BTN, !is_safety);

			if (is_safety)
				SetTimer (hwnd, NOTIFY_TIMER_SAFETY_ID, NOTIFY_TIMER_SAFETY, nullptr);

			ShowWindow (hwnd, is_forced ? SW_SHOW : SW_SHOWNA);

			return true;
		}
	}

	return false;
}

void _app_notifyhide (HWND hwnd)
{
	_app_notifysettimeout (hwnd, 0, false, 0);

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

		if (RegOpenKeyEx (HKEY_CURRENT_USER, L"AppEvents\\Schemes\\Apps\\.Default\\" NOTIFY_SOUND_DEFAULT L"\\.Default", 0, KEY_READ, &hkey) == ERROR_SUCCESS)
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
		PlaySound (NOTIFY_SOUND_DEFAULT, nullptr, SND_ASYNC);
}

bool _app_notifyrefresh (HWND hwnd, bool is_safety)
{
	if (!app.ConfigGet (L"IsNotificationsEnabled", true).AsBool ())
	{
		_app_notifyhide (config.hnotification);
		return true;
	}

	const size_t notify_idx = _app_notifygetcurrent (hwnd);

	if (!IsWindowVisible (hwnd) || notifications.empty () || notify_idx == LAST_VALUE)
	{
		_app_notifyhide (hwnd);
		return false;
	}

	return _app_notifyshow (hwnd, notify_idx, false, is_safety);
}

void _app_notifysetpos (HWND hwnd)
{
	RECT windowRect = {0};
	GetWindowRect (hwnd, &windowRect);

	RECT desktopRect = {0};
	SystemParametersInfo (SPI_GETWORKAREA, 0, &desktopRect, 0);

	APPBARDATA appbar = {0};
	appbar.cbSize = sizeof (appbar);
	appbar.hWnd = FindWindow (L"Shell_TrayWnd", nullptr);

	SHAppBarMessage (ABM_GETTASKBARPOS, &appbar);

	const UINT border_x = GetSystemMetrics (SM_CXBORDER);
	const UINT border_y = GetSystemMetrics (SM_CYBORDER);

	if (appbar.uEdge == ABE_LEFT)
	{
		windowRect.left = appbar.rc.right + border_x;
		windowRect.top = (desktopRect.bottom - (windowRect.bottom - windowRect.top)) - border_y;
	}
	else if (appbar.uEdge == ABE_TOP)
	{
		windowRect.left = (desktopRect.right - (windowRect.right - windowRect.left)) - border_x;
		windowRect.top = appbar.rc.bottom + border_y;
	}
	else if (appbar.uEdge == ABE_RIGHT)
	{
		windowRect.left = (desktopRect.right - (windowRect.right - windowRect.left)) - border_x;
		windowRect.top = (desktopRect.bottom - (windowRect.bottom - windowRect.top)) - border_y;
	}
	else/* if (appbar.uEdge == ABE_BOTTOM)*/
	{
		windowRect.left = (desktopRect.right - (windowRect.right - windowRect.left)) - border_x;
		windowRect.top = (desktopRect.bottom - (windowRect.bottom - windowRect.top)) - border_y;
	}

	SetWindowPos (hwnd, nullptr, windowRect.left, windowRect.top, 0, 0, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE | SWP_NOSIZE | SWP_FRAMECHANGED);
}

bool _app_notifysettimeout (HWND hwnd, UINT_PTR timer_id, bool is_create, UINT timeout)
{
	if (is_create)
	{
		if (!hwnd || !timer_id)
			return false;

		if (timer_id == NOTIFY_TIMER_TIMEOUT_ID)
			config.is_notifytimeout = true;

		SetTimer (hwnd, timer_id, timeout, nullptr);
	}
	else
	{
		if (!timer_id || timer_id == NOTIFY_TIMER_TIMEOUT_ID)
			config.is_notifytimeout = false;

		if (timer_id)
		{
			KillTimer (hwnd, timer_id);
		}
		else
		{
			KillTimer (hwnd, NOTIFY_TIMER_POPUP_ID);
			KillTimer (hwnd, NOTIFY_TIMER_TIMEOUT_ID);
		}
	}

	return true;
}

void _app_notifysetnote (HWND hwnd, UINT ctrl_id, rstring str)
{
	static const size_t len = 48;

	if (str.GetLength () > len)
	{
		str.Insert (len, L"...");
		str.SetLength (len + 3);
	}

	SendDlgItemMessage (hwnd, ctrl_id, BCM_SETNOTE, 0, (LPARAM)str.GetString ());
}

void _app_notifysettext (HDC hdc, HWND hwnd, UINT ctrl_id1, LPCWSTR text1, UINT ctrl_id2, LPCWSTR text2)
{
	RECT rc_wnd = {0};
	RECT rc_ctrl = {0};

	const HWND hctrl1 = GetDlgItem (hwnd, ctrl_id1);
	const HWND hctrl2 = GetDlgItem (hwnd, ctrl_id2);

	static const INT padding = app.GetDPI (12);
	static const INT border = padding / 2;
	static const INT caption_width = GetSystemMetrics (SM_CYSMCAPTION);

	SelectObject (hdc, (HFONT)SendMessage (hctrl1, WM_GETFONT, 0, 0)); // fix
	SelectObject (hdc, (HFONT)SendMessage (hctrl2, WM_GETFONT, 0, 0)); // fix

	GetWindowRect (hwnd, &rc_wnd);
	GetWindowRect (hctrl1, &rc_ctrl);

	MapWindowPoints (HWND_DESKTOP, hwnd, (LPPOINT)& rc_ctrl, 2);

	const INT wnd_width = _R_RECT_WIDTH (&rc_wnd) - (padding);
	const INT ctrl1_width = _r_dc_fontwidth (hdc, text1, _r_str_length (text1)) + caption_width;

	SetWindowPos (hctrl1, nullptr, padding, rc_ctrl.top, ctrl1_width + border, _R_RECT_HEIGHT (&rc_ctrl), SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
	SetWindowPos (hctrl2, nullptr, padding + ctrl1_width + border, rc_ctrl.top, wnd_width - ctrl1_width - padding - border, _R_RECT_HEIGHT (&rc_ctrl), SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

	SetWindowText (hctrl1, text1);
	SetWindowText (hctrl2, text2);
}

HFONT _app_notifyinitfont (PLOGFONT plf, LONG height, LONG weight, LPCWSTR name, BOOL is_underline)
{
	plf->lfHeight = _r_dc_fontsizetoheight (height);

	plf->lfWeight = weight;
	plf->lfUnderline = (BYTE)is_underline;

	//plf->lfCharSet = DEFAULT_CHARSET;
	//plf->lfQuality = DEFAULT_QUALITY;

	if (name)
		StringCchCopy (plf->lfFaceName, LF_FACESIZE, name);

	return CreateFontIndirect (plf);
}

void DrawFrameBorder (HDC hdc, HWND hwnd, COLORREF clr)
{
	RECT rc = {0};
	GetWindowRect (hwnd, &rc);

	const HPEN hpen = CreatePen (PS_INSIDEFRAME, GetSystemMetrics (SM_CXBORDER), clr);

	const HPEN old_pen = (HPEN)SelectObject (hdc, hpen);
	const HBRUSH old_brush = (HBRUSH)SelectObject (hdc, GetStockObject (NULL_BRUSH));

	Rectangle (hdc, 0, 0, _R_RECT_WIDTH (&rc), _R_RECT_HEIGHT (&rc));

	SelectObject (hdc, old_pen);
	SelectObject (hdc, old_brush);

	DeleteObject (hpen);
}

LRESULT CALLBACK NotificationProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
		case WM_CREATE:
		{
#ifndef _APP_NO_DARKTHEME
			_r_wnd_setdarktheme (config.hnotification);
#endif // _APP_NO_DARKTHEME

			break;
		}

		case WM_CLOSE:
		{
			_app_notifyhide (hwnd);

			SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
			return TRUE;
		}

		case WM_MOUSEMOVE:
		{
			if (!config.is_notifytimeout)
				_app_notifysettimeout (hwnd, 0, false, 0);

			break;
		}

		case WM_ACTIVATE:
		{
			switch (LOWORD (wparam))
			{
				case WA_INACTIVE:
				{
					if (!config.is_notifytimeout && !_r_wnd_undercursor (hwnd))
						_app_notifyhide (hwnd);

					break;
				}
			}

			break;
		}

		case WM_TIMER:
		{
			if (wparam == NOTIFY_TIMER_SAFETY_ID)
			{
				_r_ctrl_enable (hwnd, IDC_ALLOW_BTN, true);
				_r_ctrl_enable (hwnd, IDC_BLOCK_BTN, true);
				_r_ctrl_enable (hwnd, IDC_LATER_BTN, true);

				KillTimer (hwnd, wparam);

				return FALSE;
			}

			if (config.is_notifytimeout && wparam != NOTIFY_TIMER_TIMEOUT_ID)
				return FALSE;

			if (wparam == NOTIFY_TIMER_TIMEOUT_ID)
			{
				if (_r_wnd_undercursor (hwnd))
				{
					_app_notifysettimeout (hwnd, wparam, false, 0);
					return FALSE;
				}
			}

			if (wparam == NOTIFY_TIMER_POPUP_ID || wparam == NOTIFY_TIMER_TIMEOUT_ID)
				_app_notifyhide (hwnd);

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
			const HDC hdc = BeginPaint (hwnd, &ps);

			RECT rc = {0};
			GetClientRect (hwnd, &rc);

			rc.bottom = app.GetDPI (28);

			_r_dc_fillrect (hdc, &rc, NOTIFY_CLR_TITLE_BG);

			for (INT i = 0; i < _R_RECT_WIDTH (&rc); i++)
				SetPixel (hdc, i, rc.bottom, _r_dc_getcolorshade (NOTIFY_CLR_TITLE_BG, 90));

			DrawFrameBorder (hdc, hwnd, NOTIFY_CLR_BORDER);

			EndPaint (hwnd, &ps);

			break;
		}

		case WM_CTLCOLORBTN:
		case WM_CTLCOLOREDIT:
		case WM_CTLCOLORSTATIC:
		{
			const UINT ctrl_id = GetDlgCtrlID ((HWND)lparam);

			SetBkMode ((HDC)wparam, TRANSPARENT); // HACK!!!

			if (
				ctrl_id == IDC_ICON_ID ||
				ctrl_id == IDC_TITLE_ID ||
				ctrl_id == IDC_MENU_BTN ||
				ctrl_id == IDC_RULES_BTN ||
				ctrl_id == IDC_TIMER_BTN ||
				ctrl_id == IDC_CLOSE_BTN
				)
			{
				SetTextColor ((HDC)wparam, NOTIFY_CLR_TITLE_TEXT);

				static HBRUSH hbrush = nullptr;

				if (!hbrush)
					hbrush = CreateSolidBrush (NOTIFY_CLR_TITLE_BG);

				return (INT_PTR)hbrush;
			}
			else if (ctrl_id == IDC_FILE_TEXT)
				SetTextColor ((HDC)wparam, GetSysColor (COLOR_HIGHLIGHT));

			else
				SetTextColor ((HDC)wparam, NOTIFY_CLR_TEXT);

			return (INT_PTR)NOTIFY_CLR_BG_BRUSH;
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

			if (
				ctrl_id == IDC_MENU_BTN ||
				ctrl_id == IDC_RULES_BTN ||
				ctrl_id == IDC_TIMER_BTN ||
				ctrl_id == IDC_CLOSE_BTN ||
				ctrl_id == IDC_FILE_TEXT
				)
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
				case TTN_GETDISPINFO:
				{
					LPNMTTDISPINFO lpnmdi = (LPNMTTDISPINFO)lparam;

					if ((lpnmdi->uFlags & TTF_IDISHWND) != 0)
					{
						WCHAR buffer[1024] = {0};
						const UINT ctrl_id = GetDlgCtrlID ((HWND)lpnmdi->hdr.idFrom);

						if (
							ctrl_id == IDC_MENU_BTN ||
							ctrl_id == IDC_RULES_BTN ||
							ctrl_id == IDC_TIMER_BTN ||
							ctrl_id == IDC_CLOSE_BTN ||
							ctrl_id == IDC_FILE_TEXT ||
							ctrl_id == IDC_SIGNATURE_TEXT ||
							ctrl_id == IDC_ADDRESS_LOCAL_TEXT ||
							ctrl_id == IDC_ADDRESS_REMOTE_TEXT ||
							ctrl_id == IDC_PROTOCOL_TEXT ||
							ctrl_id == IDC_FILTER_TEXT ||
							ctrl_id == IDC_DATE_TEXT
							)
						{
							if (ctrl_id == IDC_RULES_BTN)
							{
								StringCchCopy (buffer, _countof (buffer), app.LocaleString (IDS_TRAY_RULES, nullptr));
							}
							else if (ctrl_id == IDC_TIMER_BTN)
							{
								StringCchCopy (buffer, _countof (buffer), app.LocaleString (IDS_TIMER, nullptr));
							}
							else if (ctrl_id == IDC_CLOSE_BTN)
							{
								StringCchCopy (buffer, _countof (buffer), app.LocaleString (IDS_CLOSE, nullptr));
							}
							else if (ctrl_id == IDC_FILE_TEXT)
							{
								if (_r_fastlock_tryacquireshared (&lock_notification))
								{
									size_t app_hash = 0;
									const size_t notify_idx = _app_notifygetcurrent (hwnd);

									if (notify_idx != LAST_VALUE)
									{
										PITEM_LOG const ptr_log = notifications.at (notify_idx);

										if (ptr_log)
											app_hash = ptr_log->hash;
									}

									_r_fastlock_releaseshared (&lock_notification);

									if (app_hash)
										StringCchCopy (buffer, _countof (buffer), _app_gettooltip (IDC_APPS_PROFILE, app_hash));
								}
							}
							else
							{
								StringCchCopy (buffer, _countof (buffer), _r_ctrl_gettext (hwnd, ctrl_id));
							}

							if (buffer[0])
								lpnmdi->lpszText = buffer;
						}
					}

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
				PITEM_RULE ptr_rule = rules_arr.at (rule_idx);

				if (!ptr_rule)
					return FALSE;

				_r_fastlock_acquireexclusive (&lock_notification);

				size_t app_hash = 0;
				const size_t notify_idx = _app_notifygetcurrent (hwnd);

				if (notify_idx != LAST_VALUE)
				{
					PITEM_LOG ptr_log = notifications.at (notify_idx);

					if (ptr_log)
						app_hash = ptr_log->hash;

					if (app_hash)
						_app_freenotify (app_hash, false, false);

					else
						_app_freenotify (notify_idx, true, false);

					_app_notifyrefresh (hwnd, true);
				}

				_r_fastlock_releaseexclusive (&lock_notification);

				if (!app_hash)
					return false;

				//if (ptr_rule->is_forservices && (app_hash == config.ntoskrnl_hash || app_hash == config.svchost_hash))
				//	continue;

				PITEM_APP ptr_app = _app_getapplication (app_hash);

				if (!ptr_app)
					return false;

				_r_fastlock_acquireexclusive (&lock_access);

				const UINT listview_id = _app_getlistview_id (ptr_app->type);
				const size_t item = _app_getposition (app.GetHWND (), listview_id, app_hash);
				const bool is_remove = ptr_rule->is_enabled && (ptr_rule->apps.find (app_hash) != ptr_rule->apps.end ());

				if (is_remove)
				{
					ptr_rule->apps.erase (app_hash);

					if (ptr_rule->apps.empty ())
						_app_ruleenable (ptr_rule, false);
				}
				else
				{
					ptr_rule->apps[app_hash] = true;
					_app_ruleenable (ptr_rule, true);
				}

				if (item != LAST_VALUE)
				{
					_r_fastlock_acquireshared (&lock_checkbox);
					_app_setappiteminfo (app.GetHWND (), listview_id, item, app_hash, ptr_app);
					_r_fastlock_releaseshared (&lock_checkbox);
				}

				_r_fastlock_releaseexclusive (&lock_access);

				MFILTER_RULES rules;
				rules.push_back (ptr_rule);

				_wfp_create4filters (rules, __LINE__);

				if (_app_gettab_id (app.GetHWND ()) == listview_id)
				{
					_app_listviewsort (app.GetHWND (), listview_id);
					_r_listview_redraw (app.GetHWND (), listview_id);
				}

				_app_profile_save ();

				return FALSE;
			}
			else if ((LOWORD (wparam) >= IDX_TIMER && LOWORD (wparam) <= IDX_TIMER + timers.size ()))
			{
				const size_t idx = (LOWORD (wparam) - IDX_TIMER);

				_app_notifycommand (hwnd, IDC_ALLOW_BTN, idx);

				return FALSE;
			}
			else if ((LOWORD (wparam) >= IDX_NOTIFICATIONS && LOWORD (wparam) <= IDX_NOTIFICATIONS + notifications.size ()))
			{
				const size_t idx = (LOWORD (wparam) - IDX_NOTIFICATIONS);

				_app_notifyshow (hwnd, idx, true, false);

				return FALSE;
			}

			switch (LOWORD (wparam))
			{
				case IDC_FILE_TEXT:
				{
					_r_fastlock_acquireshared (&lock_notification);

					size_t app_hash = 0;
					const size_t notify_idx = _app_notifygetcurrent (hwnd);

					if (notify_idx != LAST_VALUE)
					{
						PITEM_LOG ptr_log = notifications.at (notify_idx);

						if (ptr_log)
							app_hash = ptr_log->hash;
					}

					_r_fastlock_releaseshared (&lock_notification);

					const PITEM_APP ptr_app = _app_getapplication (app_hash);

					if (ptr_app)
					{
						const UINT listview_id = _app_getlistview_id (ptr_app->type);
						_app_showitem (app.GetHWND (), listview_id, _app_getposition (app.GetHWND (), listview_id, app_hash));

						_r_wnd_toggle (app.GetHWND (), true);
					}

					break;
				}

				case IDC_MENU_BTN:
				case IDC_RULES_BTN:
				case IDC_TIMER_BTN:
				{
					const HMENU hmenu = CreateMenu ();
					const HMENU hsubmenu = CreateMenu ();

					const HWND hctrl = (HWND)lparam;

					AppendMenu (hmenu, MF_POPUP, (UINT_PTR)hsubmenu, L" ");

					if (LOWORD (wparam) == IDC_MENU_BTN)
					{
						_r_fastlock_acquireshared (&lock_notification);

						const size_t current_idx = _app_notifygetcurrent (hwnd);

						for (size_t i = 0; i < notifications.size (); i++)
						{
							PITEM_LOG ptr_log = notifications.at (i);

							if (ptr_log)
							{
								AppendMenu (hsubmenu, MF_BYPOSITION, IDX_NOTIFICATIONS + UINT (i), _r_fmt (L"%s - %s", ptr_log->path ? _r_path_extractfile (ptr_log->path).GetString () : SZ_EMPTY, ptr_log->remote_fmt));

								if (i == current_idx)
									CheckMenuRadioItem (hsubmenu, IDX_NOTIFICATIONS, IDX_NOTIFICATIONS + UINT (i), IDX_NOTIFICATIONS + UINT (i), MF_BYCOMMAND);
							}
						}

						if (notifications.size () == 1)
							EnableMenuItem (hsubmenu, IDX_NOTIFICATIONS, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

						_r_fastlock_releaseshared (&lock_notification);
					}
					else if (LOWORD (wparam) == IDC_RULES_BTN)
					{
						_r_fastlock_acquireshared (&lock_notification);

						const size_t notify_idx = _app_notifygetcurrent (hwnd);
						size_t app_hash = 0;

						if (notify_idx != LAST_VALUE)
						{
							PITEM_LOG const ptr_log = notifications.at (notify_idx);

							if (ptr_log)
								app_hash = ptr_log->hash;
						}

						_r_fastlock_releaseshared (&lock_notification);

						_r_fastlock_acquireshared (&lock_access);

						AppendMenu (hsubmenu, MF_STRING, IDM_DISABLENOTIFICATIONS, app.LocaleString (IDS_DISABLENOTIFICATIONS, nullptr));

						_app_generate_rulesmenu (hsubmenu, app_hash);

						_r_fastlock_releaseshared (&lock_access);
					}
					else if (LOWORD (wparam) == IDC_TIMER_BTN)
					{
						for (size_t i = 0; i < timers.size (); i++)
							AppendMenu (hsubmenu, MF_BYPOSITION, IDX_TIMER + UINT (i), _r_fmt_interval (timers.at (i) + 1, 1));
					}

					RECT buttonRect = {0};

					GetClientRect (hctrl, &buttonRect);
					ClientToScreen (hctrl, (LPPOINT)& buttonRect);

					buttonRect.left -= app.GetDPI (2);
					buttonRect.top += app.GetDPI (24);

					_r_wnd_adjustwindowrect (hctrl, &buttonRect);

					TrackPopupMenuEx (hsubmenu, TPM_RIGHTBUTTON | TPM_LEFTBUTTON, buttonRect.left, buttonRect.top, hwnd, nullptr);

					DestroyMenu (hsubmenu);
					DestroyMenu (hmenu);

					break;
				}

				case IDC_ALLOW_BTN:
				case IDC_BLOCK_BTN:
				case IDC_LATER_BTN:
				case IDM_DISABLENOTIFICATIONS:
				{
					_app_notifycommand (hwnd, LOWORD (wparam), LAST_VALUE);
					break;
				}

				case IDM_EDITRULES:
				{
					_app_settab_id (app.GetHWND (), _app_getlistview_id (DataRuleCustom));
					break;
				}

				case IDM_OPENRULESEDITOR:
				{
					PITEM_RULE ptr_rule = new ITEM_RULE;

					_r_fastlock_acquireexclusive (&lock_notification);

					const size_t notify_idx = _app_notifygetcurrent (hwnd);
					size_t app_hash = 0;

					if (notify_idx != LAST_VALUE)
					{
						const PITEM_LOG ptr_log = notifications.at (notify_idx);

						if (ptr_log)
						{
							ptr_rule->apps[ptr_log->hash] = true;
							ptr_rule->protocol = ptr_log->protocol;
							app_hash = ptr_log->hash;

							LPWSTR rule = nullptr;

							if (_app_formataddress (ptr_log->af, &ptr_log->remote_addr, ptr_log->remote_port, &rule, false))
							{
								_r_str_alloc (&ptr_rule->pname, _r_str_length (rule), rule);
								_r_str_alloc (&ptr_rule->prule_remote, _r_str_length (rule), rule);
							}

							SAFE_DELETE_ARRAY (rule);
						}

						if (app_hash)
							_app_freenotify (app_hash, false, false);

						else
							_app_freenotify (notify_idx, true, false);

						_app_notifyrefresh (hwnd, true);
					}

					_r_fastlock_releaseexclusive (&lock_notification);

					ptr_rule->type = DataRuleCustom;
					ptr_rule->is_block = false;

					_app_ruleenable (ptr_rule, true);

					if (DialogBoxParam (nullptr, MAKEINTRESOURCE (IDD_EDITOR), app.GetHWND (), &EditorProc, (LPARAM)ptr_rule))
					{
						_r_fastlock_acquireexclusive (&lock_access);
						const size_t rule_idx = rules_arr.size ();
						rules_arr.push_back (ptr_rule);
						_r_fastlock_releaseexclusive (&lock_access);

						// set rule information
						{
							_r_fastlock_acquireshared (&lock_checkbox);

							const UINT listview_rules_id = _app_getlistview_id (ptr_rule->type);
							const size_t new_item = _r_listview_getitemcount (hwnd, listview_rules_id);

							_r_listview_additem (app.GetHWND (), listview_rules_id, new_item, 0, ptr_rule->pname, _app_getruleicon (ptr_rule), _app_getrulegroup (ptr_rule), rule_idx);
							_app_setruleiteminfo (app.GetHWND (), listview_rules_id, new_item, ptr_rule, true);

							_r_fastlock_releaseshared (&lock_checkbox);
						}

						// set app information
						{
							const PITEM_APP ptr_app = _app_getapplication (app_hash);

							if (ptr_app)
							{
								const UINT listview_id = _app_getlistview_id (ptr_app->type);

								_r_fastlock_acquireshared (&lock_checkbox);
								_app_setappiteminfo (app.GetHWND (), listview_id, _app_getposition (app.GetHWND (), listview_id, app_hash), app_hash, ptr_app);
								_r_fastlock_releaseshared (&lock_checkbox);

								if (listview_id == _app_gettab_id (app.GetHWND ()))
									_app_listviewsort (app.GetHWND (), listview_id);
							}
						}

						_app_refreshstatus (app.GetHWND ());
						_app_profile_save ();
					}
					else
					{
						_app_freerule (&ptr_rule);
					}

					break;
				}

				case IDC_CLOSE_BTN:
				{
					_app_notifyhide (hwnd);
					break;
				}
			}

			break;
		}
	}

	return DefWindowProc (hwnd, msg, wparam, lparam);
}
