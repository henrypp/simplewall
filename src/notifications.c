// simplewall
// Copyright (c) 2016-2021 Henry++

#include "global.h"

VOID _app_notify_createwindow ()
{
	HWND hwnd;

	hwnd = CreateDialogParam (NULL, MAKEINTRESOURCE (IDD_NOTIFICATION), NULL, &NotificationProc, 0);

	InterlockedCompareExchangePointer (&config.hnotification, hwnd, NULL);
}

VOID _app_notify_destroywindow ()
{
	HWND hwnd;

	hwnd = InterlockedCompareExchangePointer (&config.hnotification, NULL, config.hnotification);

	if (hwnd)
		DestroyWindow (hwnd);
}

_Ret_maybenull_
HWND _app_notify_getwindow ()
{
	return InterlockedCompareExchangePointer (&config.hnotification, NULL, NULL);
}

BOOLEAN _app_notify_command (_In_ HWND hwnd, _In_ INT button_id, _In_ LONG64 seconds)
{
	ULONG_PTR app_hash;
	PITEM_APP ptr_app;
	PR_LIST rules;

	app_hash = _app_notify_getapp_id (hwnd);
	ptr_app = _app_getappitem (app_hash);

	if (!ptr_app)
		return FALSE;

	_app_notify_freeobject (ptr_app);

	rules = _r_obj_createlist (NULL);

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
			_app_updateitembylparam (_r_app_gethwnd (), app_hash, TRUE);
		}

		_r_obj_addlistitem (rules, ptr_app);
	}
	else if (button_id == IDC_LATER_BTN)
	{
		// NOTHING!
	}
	else if (button_id == IDM_DISABLENOTIFICATIONS)
	{
		ptr_app->is_silent = TRUE;
	}

	ptr_app->last_notify = _r_unixtime_now ();

	if (rules->count)
	{
		if (_wfp_isfiltersinstalled ())
		{
			HANDLE hengine = _wfp_getenginehandle ();

			if (hengine)
				_wfp_create3filters (hengine, rules, __LINE__, FALSE);
		}
	}

	_app_updatelistviewbylparam (_r_app_gethwnd (), ptr_app->type, PR_UPDATE_TYPE);

	_r_obj_dereference (ptr_app);
	_r_obj_dereference (rules);

	_app_profile_save ();

	return TRUE;
}

BOOLEAN _app_notify_addobject (_In_ PITEM_LOG ptr_log, _In_ PITEM_APP ptr_app)
{
	HWND hnotify;
	LONG64 current_time;
	LONG64 notification_timeout;

	current_time = _r_unixtime_now ();
	notification_timeout = _r_config_getlong64 (L"NotificationsTimeout", NOTIFY_TIMEOUT_DEFAULT);

	// check for last display time
	if (notification_timeout && ((current_time - ptr_app->last_notify) <= notification_timeout))
		return FALSE;

	hnotify = _app_notify_getwindow ();

	if (!hnotify)
		return FALSE;

	ptr_app->last_notify = current_time;

	_r_obj_movereference (&ptr_app->notification, _r_obj_reference (ptr_log));

	if (_r_config_getboolean (L"IsNotificationsSound", TRUE))
	{
		if (!_r_config_getboolean (L"IsNotificationsFullscreenSilentMode", TRUE) || !_r_wnd_isfullscreenmode ())
		{
			_app_notify_playsound ();
		}
	}

	if (!_r_wnd_isundercursor (hnotify))
		_app_notify_show (hnotify, ptr_log, TRUE, TRUE);

	return TRUE;
}

VOID _app_notify_freeobject (_Inout_ PITEM_APP ptr_app)
{
	HWND hnotify;
	ULONG_PTR app_hash;

	hnotify = _app_notify_getwindow ();

	if (ptr_app->notification)
		_r_obj_clearreference (&ptr_app->notification);

	if (hnotify)
	{
		app_hash = _app_notify_getnextapp_id (hnotify);

		if (app_hash)
		{
			_app_notify_refresh (hnotify, TRUE);
		}
		else
		{
			_app_notify_hide (hnotify);
		}
	}
}

_Ret_maybenull_
PITEM_LOG _app_notify_getobject (_In_ ULONG_PTR app_hash)
{
	PITEM_APP ptr_app;
	PITEM_LOG notification;

	ptr_app = _app_getappitem (app_hash);

	if (ptr_app)
	{
		notification = _r_obj_referencesafe (ptr_app->notification);

		_r_obj_dereference (ptr_app);

		return notification;
	}

	return NULL;
}

_Ret_maybenull_
HICON _app_notify_getapp_icon (_In_ HWND hwnd)
{
	PNOTIFY_CONTEXT context;
	HICON hicon;

	context = (PNOTIFY_CONTEXT)GetWindowLongPtr (hwnd, GWLP_USERDATA);

	if (context)
	{
		hicon = context->hicon;
	}
	else
	{
		hicon = NULL;
	}

	if (!hicon)
		hicon = _app_getdefaultapphicon ();

	return hicon;
}

ULONG_PTR _app_notify_getapp_id (_In_ HWND hwnd)
{
	PNOTIFY_CONTEXT context;

	context = (PNOTIFY_CONTEXT)GetWindowLongPtr (hwnd, GWLP_USERDATA);

	if (context)
		return context->app_hash;

	return 0;
}

ULONG_PTR _app_notify_getnextapp_id (_In_ HWND hwnd)
{
	PNOTIFY_CONTEXT context;
	PITEM_APP ptr_app;
	SIZE_T enum_key;
	ULONG_PTR app_hash;

	context = (PNOTIFY_CONTEXT)GetWindowLongPtr (hwnd, GWLP_USERDATA);

	if (context)
	{
		app_hash = context->app_hash;
	}
	else
	{
		app_hash = 0;
	}

	enum_key = 0;

	_r_queuedlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtablepointer (apps_table, &ptr_app, NULL, &enum_key))
	{
		if (ptr_app->app_hash == app_hash) // exclude current app from enumeration
			continue;

		if (ptr_app->notification)
		{
			app_hash = ptr_app->app_hash;

			_r_queuedlock_releaseshared (&lock_apps);

			if (context)
				context->app_hash = app_hash;

			return app_hash;
		}
	}

	_r_queuedlock_releaseshared (&lock_apps);

	if (context)
		context->app_hash = 0;

	return 0;
}

VOID _app_notify_setapp_icon (_In_ HWND hwnd, _In_opt_ HICON hicon, _In_ BOOLEAN is_redraw)
{
	PNOTIFY_CONTEXT context;

	context = (PNOTIFY_CONTEXT)GetWindowLongPtr (hwnd, GWLP_USERDATA);

	if (!context)
		return;

	context->hicon = hicon;

	if (is_redraw)
		RedrawWindow (hwnd, NULL, NULL, RDW_ALLCHILDREN | RDW_ERASE | RDW_INVALIDATE);
}

VOID _app_notify_setapp_id (_In_ HWND hwnd, _In_opt_ ULONG_PTR app_hash)
{
	PNOTIFY_CONTEXT context;

	context = (PNOTIFY_CONTEXT)GetWindowLongPtr (hwnd, GWLP_USERDATA);

	if (!context)
		return;

	context->app_hash = app_hash;
}

VOID _app_notify_show (_In_ HWND hwnd, _In_ PITEM_LOG ptr_log, _In_ BOOLEAN is_forced, _In_ BOOLEAN is_safety)
{
	static R_STRINGREF loading_text = PR_STRINGREF_INIT (SZ_LOADING);

	WCHAR window_title[128];
	PITEM_CONTEXT context;
	PITEM_APP ptr_app;
	PR_STRING string;
	PR_STRING localized_string;
	R_STRINGREF empty_string;
	R_STRINGREF display_name;
	BOOLEAN is_fullscreenmode;

	if (!_r_config_getboolean (L"IsNotificationsEnabled", TRUE))
		return;

	ptr_app = _app_getappitem (ptr_log->app_hash);

	if (!ptr_app)
		return;

	string = NULL;
	localized_string = NULL;

	// set window title
	_r_str_printf (window_title, RTL_NUMBER_OF (window_title), L"%s - %s", _r_locale_getstring (IDS_NOTIFY_TITLE), _r_app_getname ());

	SetWindowText (hwnd, window_title);

	// set notification information
	_app_notify_setapp_id (hwnd, ptr_log->app_hash);
	_app_notify_setapp_icon (hwnd, NULL, FALSE);

	_r_obj_initializestringrefconst (&empty_string, _r_locale_getstring (IDS_STATUS_EMPTY));
	_r_obj_initializestringrefconst (&display_name, _app_getappdisplayname (ptr_app, TRUE));

	// print name
	_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_NAME), L":"));
	_r_ctrl_settablestring (hwnd, IDC_FILE_ID, &localized_string->sr, IDC_FILE_TEXT, &display_name);

	// print signature
	_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_SIGNATURE), L":"));
	_r_ctrl_settablestring (hwnd, IDC_SIGNATURE_ID, &localized_string->sr, IDC_SIGNATURE_TEXT, &loading_text);

	// print address
	_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_ADDRESS), L":"));
	_r_obj_movereference (&string, _app_formataddress (ptr_log->af, ptr_log->protocol, &ptr_log->remote_addr, 0, FMTADDR_USE_PROTOCOL));

	_r_ctrl_settablestring (hwnd, IDC_ADDRESS_ID, &localized_string->sr, IDC_ADDRESS_TEXT, string ? &string->sr : &empty_string);

	// print host
	_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_HOST), L":"));
	_r_ctrl_settablestring (hwnd, IDC_HOST_ID, &localized_string->sr, IDC_HOST_TEXT, &loading_text);

	// print port
	_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_PORT), L":"));
	_r_obj_movereference (&string, _app_formatport (ptr_log->remote_port, ptr_log->protocol));

	_r_ctrl_settablestring (hwnd, IDC_PORT_ID, &localized_string->sr, IDC_PORT_TEXT, string ? &string->sr : &empty_string);

	// print direction
	_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_DIRECTION), L":"));
	_r_obj_movereference (&string, _app_getdirectionname (ptr_log->direction, ptr_log->is_loopback, TRUE));

	_r_ctrl_settablestring (hwnd, IDC_DIRECTION_ID, &localized_string->sr, IDC_DIRECTION_TEXT, string ? &string->sr : &empty_string);

	// print filter name
	_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_FILTER), L":"));
	_r_ctrl_settablestring (hwnd, IDC_FILTER_ID, &localized_string->sr, IDC_FILTER_TEXT, ptr_log->filter_name ? &ptr_log->filter_name->sr : &empty_string);

	// print date
	_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_DATE), L":"));
	_r_obj_movereference (&string, _r_format_unixtime_ex (ptr_log->timestamp, FDTF_SHORTDATE | FDTF_LONGTIME));

	_r_ctrl_settablestring (hwnd, IDC_DATE_ID, &localized_string->sr, IDC_DATE_TEXT, string ? &string->sr : &empty_string);

	_r_ctrl_setstring (hwnd, IDC_RULES_BTN, _r_locale_getstring (IDS_TRAY_RULES));
	_r_ctrl_setstring (hwnd, IDC_ALLOW_BTN, _r_locale_getstring (IDS_ACTION_ALLOW));
	_r_ctrl_setstring (hwnd, IDC_BLOCK_BTN, _r_locale_getstring (IDS_ACTION_BLOCK));

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

	_app_notify_setposition (hwnd, FALSE);

	// prevent fullscreen apps lose focus
	is_fullscreenmode = _r_wnd_isfullscreenmode ();

	if (is_forced && is_fullscreenmode)
		is_forced = FALSE;

	_r_wnd_top (hwnd, !is_fullscreenmode);

	ShowWindow (hwnd, is_forced ? SW_SHOW : SW_SHOWNA);

	RedrawWindow (hwnd, NULL, NULL, RDW_ALLCHILDREN | RDW_ERASE | RDW_INVALIDATE);

	// query busy information
	context = _r_freelist_allocateitem (&context_free_list);

	context->hwnd = hwnd;
	context->ptr_log = _r_obj_reference (ptr_log);

	_r_workqueue_queueitem (&resolve_notify_queue, &_app_queuenotifyinformation, context);

	if (string)
		_r_obj_dereference (string);

	if (localized_string)
		_r_obj_dereference (localized_string);

	_r_obj_dereference (ptr_app);
}

VOID _app_notify_hide (_In_ HWND hwnd)
{
	ShowWindow (hwnd, SW_HIDE);
}

// Play notification sound even if system have "nosound" mode
VOID _app_notify_playsound ()
{
	static PR_STRING path = NULL;

	if (_r_obj_isstringempty (path) || !_r_fs_exists (path->buffer))
	{
#define NOTIFY_SOUND_NAME L"MailBeep"

		HKEY hkey;
		PR_STRING expanded_string;

		if (RegOpenKeyEx (HKEY_CURRENT_USER, L"AppEvents\\Schemes\\Apps\\.Default\\" NOTIFY_SOUND_NAME L"\\.Default", 0, KEY_READ, &hkey) == ERROR_SUCCESS)
		{
			_r_obj_movereference (&path, _r_reg_querystring (hkey, NULL, NULL));

			if (path)
			{
				expanded_string = _r_str_expandenvironmentstring (&path->sr);

				if (expanded_string)
					_r_obj_movereference (&path, expanded_string);
			}

			RegCloseKey (hkey);
		}
	}

	if (_r_obj_isstringempty (path) || !PlaySound (path->buffer, NULL, SND_ASYNC | SND_NODEFAULT | SND_NOWAIT | SND_FILENAME | SND_SENTRY))
		PlaySound (NOTIFY_SOUND_NAME, NULL, SND_ASYNC | SND_NODEFAULT | SND_NOWAIT | SND_SENTRY);
}

VOID _app_notify_refresh (_In_ HWND hwnd, _In_ BOOLEAN is_safety)
{
	PITEM_LOG ptr_log;
	ULONG_PTR app_hash;

	if (!_r_wnd_isvisible (hwnd) || !_r_config_getboolean (L"IsNotificationsEnabled", TRUE))
	{
		_app_notify_hide (hwnd);
		return;
	}

	app_hash = _app_notify_getapp_id (hwnd);
	ptr_log = _app_notify_getobject (app_hash);

	if (!ptr_log)
	{
		_app_notify_hide (hwnd);
		return;
	}

	_app_notify_show (hwnd, ptr_log, TRUE, is_safety);

	_r_obj_dereference (ptr_log);
}

VOID _app_notify_setposition (_In_ HWND hwnd, _In_ BOOLEAN is_forced)
{
	R_RECTANGLE window_rect;
	UINT swp_flags;
	BOOLEAN is_intray;

	swp_flags = SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER;

	if (!is_forced && _r_wnd_isvisible (hwnd))
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
			MONITORINFO monitor_info = {0};
			APPBARDATA taskbar_rect = {0};

			HMONITOR hmonitor;
			PRECT rect;
			LONG dpi_value;
			LONG border_x;

			dpi_value = _r_dc_getwindowdpi (hwnd);

			monitor_info.cbSize = sizeof (monitor_info);
			hmonitor = MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST);

			if (GetMonitorInfo (hmonitor, &monitor_info))
			{
				taskbar_rect.cbSize = sizeof (taskbar_rect);

				if (SHAppBarMessage (ABM_GETTASKBARPOS, &taskbar_rect))
				{
					border_x = _r_dc_getsystemmetrics (SM_CXBORDER, dpi_value);
					rect = &monitor_info.rcWork;

					if (taskbar_rect.uEdge == ABE_LEFT)
					{
						window_rect.left = taskbar_rect.rc.right + border_x;
						window_rect.top = (rect->bottom - window_rect.height) - border_x;
					}
					else if (taskbar_rect.uEdge == ABE_TOP)
					{
						window_rect.left = (rect->right - window_rect.width) - border_x;
						window_rect.top = taskbar_rect.rc.bottom + border_x;
					}
					else if (taskbar_rect.uEdge == ABE_RIGHT)
					{
						window_rect.left = (rect->right - window_rect.width) - border_x;
						window_rect.top = (rect->bottom - window_rect.height) - border_x;
					}
					else //if (taskbar_rect.uEdge == ABE_BOTTOM)
					{
						window_rect.left = (rect->right - window_rect.width) - border_x;
						window_rect.top = (rect->bottom - window_rect.height) - border_x;
					}

					_r_wnd_adjustworkingarea (NULL, &window_rect);

					SetWindowPos (hwnd, NULL, window_rect.left, window_rect.top, 0, 0, swp_flags);
					return;
				}
			}
		}
	}

	_r_wnd_center (hwnd, NULL); // display window on center (depends on error, config etc...)
}

HFONT _app_notify_fontcreate (_Inout_ PLOGFONT logfont, _In_ LONG dpi_value, _In_ LONG size, _In_ BOOLEAN is_underline)
{
	if (size)
		logfont->lfHeight = _r_dc_fontsizetoheight (size, dpi_value);

	logfont->lfUnderline = is_underline;

	logfont->lfCharSet = DEFAULT_CHARSET;
	logfont->lfQuality = DEFAULT_QUALITY;

	return CreateFontIndirect (logfont);
}

VOID _app_notify_initializefont (_In_ HWND hwnd, _Inout_ PNOTIFY_CONTEXT context, _In_ LONG dpi_value)
{
	NONCLIENTMETRICS ncm = {0};

	LONG title_font_height;
	LONG text_font_height;

	ncm.cbSize = sizeof (ncm);

	SAFE_DELETE_OBJECT (context->hfont_title);
	SAFE_DELETE_OBJECT (context->hfont_link);
	SAFE_DELETE_OBJECT (context->hfont_text);

	if (!_r_dc_getsystemparametersinfo (SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, dpi_value))
		return;

	title_font_height = 12;
	text_font_height = 9;

	context->hfont_title = _app_notify_fontcreate (&ncm.lfCaptionFont, dpi_value, title_font_height, FALSE);
	context->hfont_link = _app_notify_fontcreate (&ncm.lfMessageFont, dpi_value, text_font_height, TRUE);
	context->hfont_text = _app_notify_fontcreate (&ncm.lfMessageFont, dpi_value, text_font_height, FALSE);

	SendDlgItemMessage (hwnd, IDC_HEADER_ID, WM_SETFONT, (WPARAM)context->hfont_title, TRUE);
	SendDlgItemMessage (hwnd, IDC_FILE_TEXT, WM_SETFONT, (WPARAM)context->hfont_link, TRUE);

	for (INT i = IDC_SIGNATURE_TEXT; i <= IDC_DATE_TEXT; i++)
		SendDlgItemMessage (hwnd, i, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, 0);

	for (INT i = IDC_SIGNATURE_TEXT; i <= IDC_LATER_BTN; i++)
		SendDlgItemMessage (hwnd, i, WM_SETFONT, (WPARAM)context->hfont_text, TRUE);
}

VOID _app_notify_initialize (_In_ HWND hwnd)
{
	PNOTIFY_CONTEXT context;

	LONG dpi_value;

	LONG icon_small_x;
	LONG icon_small_y;

	LONG icon_large_x;
	LONG icon_large_y;

	context = (PNOTIFY_CONTEXT)GetWindowLongPtr (hwnd, GWLP_USERDATA);

	if (!context)
		return;

	dpi_value = _r_dc_getwindowdpi (hwnd);

	icon_small_x = _r_dc_getsystemmetrics (SM_CXSMICON, dpi_value);
	icon_small_y = _r_dc_getsystemmetrics (SM_CYSMICON, dpi_value);

	icon_large_x = _r_dc_getsystemmetrics (SM_CXICON, dpi_value);
	icon_large_y = _r_dc_getsystemmetrics (SM_CYICON, dpi_value);

	// set window icon
	_r_wnd_seticon (hwnd,
					_r_sys_loadsharedicon (NULL, MAKEINTRESOURCE (SIH_EXCLAMATION), icon_small_x, icon_small_y),
					_r_sys_loadsharedicon (NULL, MAKEINTRESOURCE (SIH_EXCLAMATION), icon_large_x, icon_large_y)
	);

	// load images
	SAFE_DELETE_OBJECT (context->hbmp_allow);
	SAFE_DELETE_OBJECT (context->hbmp_block);
	SAFE_DELETE_OBJECT (context->hbmp_cross);
	SAFE_DELETE_OBJECT (context->hbmp_rules);

	context->hbmp_allow = _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_ALLOW), icon_small_x, icon_small_y);
	context->hbmp_block = _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_BLOCK), icon_small_x, icon_small_y);
	context->hbmp_cross = _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_CROSS), icon_small_x, icon_small_y);
	context->hbmp_rules = _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_SETTINGS), icon_small_x, icon_small_y);

	// set button images
	SendDlgItemMessage (hwnd, IDC_RULES_BTN, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)context->hbmp_rules);
	SendDlgItemMessage (hwnd, IDC_ALLOW_BTN, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)context->hbmp_allow);
	SendDlgItemMessage (hwnd, IDC_BLOCK_BTN, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)context->hbmp_block);
	SendDlgItemMessage (hwnd, IDC_LATER_BTN, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)context->hbmp_cross);

	_r_ctrl_setbuttonmargins (hwnd, IDC_RULES_BTN);
	_r_ctrl_setbuttonmargins (hwnd, IDC_ALLOW_BTN);
	_r_ctrl_setbuttonmargins (hwnd, IDC_BLOCK_BTN);
	_r_ctrl_setbuttonmargins (hwnd, IDC_LATER_BTN);

	// load font
	_app_notify_initializefont (hwnd, context, dpi_value);

	SendMessage (hwnd, WM_THEMECHANGED, 0, 0);
}

VOID _app_notify_drawgradient (_In_ HDC hdc, _In_ LPCRECT rect)
{
	static COLORREF gradient_arr[] = {
		RGB (0, 68, 112),
		RGB (7, 111, 95),
	};

	GRADIENT_RECT gradient_rect = {0};
	TRIVERTEX trivertx[2] = {0};

	C_ASSERT (RTL_NUMBER_OF (gradient_arr) == RTL_NUMBER_OF (trivertx));

	gradient_rect.LowerRight = 1;
	//gradient_rect.UpperLeft = 0;

	for (ULONG i = 0; i < RTL_NUMBER_OF (trivertx); i++)
	{
		trivertx[i].Red = GetRValue (gradient_arr[i]) << 8;
		trivertx[i].Green = GetGValue (gradient_arr[i]) << 8;
		trivertx[i].Blue = GetBValue (gradient_arr[i]) << 8;

		if (i == 0)
		{
			trivertx[i].x = -1;
			trivertx[i].y = -1;
		}
		else
		{
			trivertx[i].x = rect->right;
			trivertx[i].y = rect->bottom;
		}
	}

	GradientFill (hdc, trivertx, RTL_NUMBER_OF (trivertx), &gradient_rect, 1, GRADIENT_FILL_RECT_H);
}

INT_PTR CALLBACK NotificationProc (_In_ HWND hwnd, _In_ UINT msg, _In_ WPARAM wparam, _In_ LPARAM lparam)
{
	switch (msg)
	{
		case WM_INITDIALOG:
		{
			HWND htip;
			PNOTIFY_CONTEXT context;

			// initialize context
			context = _r_mem_allocatezero (sizeof (NOTIFY_CONTEXT));

			SetWindowLongPtr (hwnd, GWLP_USERDATA, (LONG_PTR)context);

			_app_notify_initialize (hwnd);

			// initialize tips
			htip = _r_ctrl_createtip (hwnd);

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

		case WM_CLOSE:
		{
			_app_notify_hide (hwnd);

			SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
			return TRUE;
		}

		case WM_NCDESTROY:
		{
			PNOTIFY_CONTEXT context;

			context = (PNOTIFY_CONTEXT)GetWindowLongPtr (hwnd, GWLP_USERDATA);

			SAFE_DELETE_OBJECT (context->hfont_title);
			SAFE_DELETE_OBJECT (context->hfont_link);
			SAFE_DELETE_OBJECT (context->hfont_text);

			SAFE_DELETE_OBJECT (context->hbmp_allow);
			SAFE_DELETE_OBJECT (context->hbmp_block);
			SAFE_DELETE_OBJECT (context->hbmp_cross);
			SAFE_DELETE_OBJECT (context->hbmp_rules);

			SetWindowLongPtr (hwnd, GWLP_USERDATA, 0);

			_r_mem_free (context);

			break;
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
				KillTimer (hwnd, NOTIFY_TIMER_SAFETY_ID);

				_r_ctrl_enable (hwnd, IDC_RULES_BTN, TRUE);
				_r_ctrl_enable (hwnd, IDC_ALLOW_BTN, TRUE);
				_r_ctrl_enable (hwnd, IDC_BLOCK_BTN, TRUE);
				_r_ctrl_enable (hwnd, IDC_LATER_BTN, TRUE);
			}

			break;
		}

		case WM_DPICHANGED:
		{
			_app_notify_initialize (hwnd);
			_app_notify_refresh (hwnd, FALSE);

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
			HDC hdc;

			hdc = BeginPaint (hwnd, &ps);

			if (hdc)
			{
				_r_dc_drawwindowdefault (hdc, hwnd, TRUE);

				EndPaint (hwnd, &ps);
			}

			break;
		}

		case WM_ERASEBKGND:
		{
			return TRUE;
		}

		case WM_CTLCOLORDLG:
		{
			HDC hdc;

			hdc = (HDC)wparam;

			SetBkMode (hdc, TRANSPARENT); // HACK!!!

			SetTextColor (hdc, GetSysColor (COLOR_WINDOWTEXT));
			SetDCBrushColor (hdc, GetSysColor (COLOR_WINDOW));

			return (INT_PTR)GetStockObject (DC_BRUSH);
		}

		case WM_CTLCOLORSTATIC:
		{
			HDC hdc;
			INT text_clr;

			hdc = (HDC)wparam;

			if (GetDlgCtrlID ((HWND)lparam) == IDC_FILE_TEXT)
			{
				text_clr = COLOR_HIGHLIGHT;
			}
			else
			{
				text_clr = COLOR_WINDOWTEXT;
			}

			SetBkMode (hdc, TRANSPARENT); // HACK!!!

			SetTextColor (hdc, GetSysColor (text_clr));
			SetDCBrushColor (hdc, GetSysColor (COLOR_WINDOW));

			return (INT_PTR)GetStockObject (DC_BRUSH);
		}

		case WM_DRAWITEM:
		{
			LPDRAWITEMSTRUCT draw_info;

			RECT text_rect;
			RECT icon_rect;

			LONG dpi_value;

			LONG icon_size_x;
			LONG wnd_spacing;
			INT bk_mode_prev;

			WCHAR text[128];
			COLORREF clr_prev;
			HICON hicon;

			draw_info = (LPDRAWITEMSTRUCT)lparam;

			if (draw_info->CtlID != IDC_HEADER_ID)
				break;

			dpi_value = _r_dc_getwindowdpi (hwnd);

			icon_size_x = _r_dc_getsystemmetrics (SM_CXICON, dpi_value);
			wnd_spacing = _r_dc_getdpi (12, dpi_value);

			bk_mode_prev = SetBkMode (draw_info->hDC, TRANSPARENT); // HACK!!!

			// draw title gradient
			_app_notify_drawgradient (draw_info->hDC, &draw_info->rcItem);

			// set rectangles
			SetRect (&text_rect, wnd_spacing, 0, _r_calc_rectwidth (&draw_info->rcItem) - (wnd_spacing * 3) - icon_size_x, _r_calc_rectheight (&draw_info->rcItem));
			SetRect (&icon_rect, _r_calc_rectwidth (&draw_info->rcItem) - icon_size_x - wnd_spacing, (_r_calc_rectheight (&draw_info->rcItem) / 2) - (icon_size_x / 2), icon_size_x, icon_size_x);

			// draw title text
			_r_str_printf (text, RTL_NUMBER_OF (text), _r_locale_getstring (IDS_NOTIFY_HEADER), _r_app_getname ());

			clr_prev = SetTextColor (draw_info->hDC, RGB (255, 255, 255));
			DrawTextEx (draw_info->hDC, text, (INT)(INT_PTR)_r_str_getlength (text), &text_rect, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOCLIP | DT_NOPREFIX, NULL);
			SetTextColor (draw_info->hDC, clr_prev);

			// draw icon
			hicon = _app_notify_getapp_icon (hwnd);

			if (hicon)
				DrawIconEx (draw_info->hDC, icon_rect.left, icon_rect.top, hicon, icon_rect.right, icon_rect.bottom, 0, NULL, DI_IMAGE | DI_MASK);

			SetBkMode (draw_info->hDC, bk_mode_prev);

			SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
			return TRUE;
		}

		case WM_SETCURSOR:
		{
			INT ctrl_id;

			ctrl_id = GetDlgCtrlID ((HWND)wparam);

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
			LONG_PTR exstyle;

			exstyle = _r_wnd_getstyle_ex (hwnd);

			if (!(exstyle & WS_EX_LAYERED))
				_r_wnd_setstyle_ex (hwnd, exstyle | WS_EX_LAYERED);

			SetLayeredWindowAttributes (hwnd, 0, (msg == WM_ENTERSIZEMOVE) ? 150 : 255, LWA_ALPHA);
			SetCursor (LoadCursor (NULL, (msg == WM_ENTERSIZEMOVE) ? IDC_SIZEALL : IDC_ARROW));

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp;

			nmlp = (LPNMHDR)lparam;

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

							_app_generate_rulescontrol (hsubmenu, _app_notify_getapp_id (hwnd));
						}
						else //if (ctrl_id == IDC_ALLOW_BTN)
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
					LPNMTTDISPINFO lpnmdi;

					WCHAR buffer[1024] = {0};
					PR_STRING string;
					INT ctrl_id;

					lpnmdi = (LPNMTTDISPINFO)lparam;

					if ((lpnmdi->uFlags & TTF_IDISHWND) == 0)
						break;

					ctrl_id = GetDlgCtrlID ((HWND)lpnmdi->hdr.idFrom);

					if (ctrl_id == IDC_FILE_TEXT)
					{
						ULONG_PTR app_hash;

						app_hash = _app_notify_getapp_id (hwnd);
						string = _app_gettooltipbylparam (_r_app_gethwnd (), PtrToInt (_app_getappinfobyhash (app_hash, INFO_LISTVIEW_ID)), app_hash);

						if (string)
						{
							_r_str_copy (buffer, RTL_NUMBER_OF (buffer), string->buffer);
							_r_obj_dereference (string);
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
						string = _r_ctrl_getstring (hwnd, ctrl_id);

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
			INT ctrl_id;

			ctrl_id = LOWORD (wparam);

			if ((ctrl_id >= IDX_RULES_SPECIAL && ctrl_id <= IDX_RULES_SPECIAL + (INT)(INT_PTR)_r_obj_getlistsize (rules_list)))
			{
				PITEM_RULE ptr_rule;
				PITEM_LOG ptr_log;
				PITEM_APP ptr_app;
				SIZE_T rule_idx;
				BOOLEAN is_remove;

				rule_idx = (SIZE_T)ctrl_id - IDX_RULES_SPECIAL;
				ptr_rule = _app_getrulebyid (rule_idx);

				if (!ptr_rule)
					return FALSE;

				ptr_log = _app_notify_getobject (_app_notify_getapp_id (hwnd));

				if (ptr_log)
				{
					if (ptr_log->app_hash && !(ptr_rule->is_forservices && _app_issystemhash (ptr_log->app_hash)))
					{
						ptr_app = _app_getappitem (ptr_log->app_hash);

						if (ptr_app)
						{
							_app_notify_freeobject (ptr_app);

							is_remove = ptr_rule->is_enabled && _r_obj_findhashtable (ptr_rule->apps, ptr_log->app_hash);

							if (is_remove)
							{
								_r_obj_removehashtableitem (ptr_rule->apps, ptr_log->app_hash);

								if (_r_obj_ishashtableempty (ptr_rule->apps))
									_app_ruleenable (ptr_rule, FALSE, TRUE);
							}
							else
							{
								_r_obj_addhashtableitem (ptr_rule->apps, ptr_log->app_hash, NULL);

								_app_ruleenable (ptr_rule, TRUE, TRUE);
							}

							_app_updateitembylparam (_r_app_gethwnd (), ptr_log->app_hash, TRUE);
							_app_updateitembylparam (_r_app_gethwnd (), rule_idx, FALSE);

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

							_app_updatelistviewbylparam (_r_app_gethwnd (), DATA_LISTVIEW_CURRENT, PR_UPDATE_TYPE);

							_r_obj_dereference (ptr_app);
							_r_obj_dereference (ptr_rule);

							_app_profile_save ();
						}
					}

					_r_obj_dereference (ptr_log);
				}

				return FALSE;
			}
			else if ((ctrl_id >= IDX_TIMER && ctrl_id <= (IDX_TIMER + (RTL_NUMBER_OF (timer_array) - 1))))
			{
				SIZE_T timer_idx;
				LONG64 seconds;

				if (!_r_ctrl_isenabled (hwnd, IDC_ALLOW_BTN))
					return FALSE;

				timer_idx = (SIZE_T)ctrl_id - IDX_TIMER;
				seconds = timer_array[timer_idx];

				_app_notify_command (hwnd, IDC_ALLOW_BTN, seconds);

				return FALSE;
			}

			switch (ctrl_id)
			{
				case IDCANCEL: // process Esc key
				{
					_app_notify_hide (hwnd);
					break;
				}

				case IDC_FILE_TEXT:
				{
					ULONG_PTR app_hash;

					app_hash = _app_notify_getapp_id (hwnd);

					if (app_hash)
					{
						_app_showitembylparam (_r_app_gethwnd (), app_hash, TRUE);
					}

					break;
				}

				case IDC_RULES_BTN:
				{
					ITEM_CONTEXT context = {0};
					PITEM_APP ptr_app;
					ULONG_PTR app_hash;

					app_hash = _app_notify_getapp_id (hwnd);
					ptr_app = _app_getappitem (app_hash);

					if (ptr_app)
					{
						context.is_settorules = FALSE;
						context.ptr_app = ptr_app;
						context.page_id = 1; // open rules tab

						if (DialogBoxParam (NULL, MAKEINTRESOURCE (IDD_EDITOR), hwnd, &PropertiesProc, (LPARAM)&context))
						{
							_app_notify_hide (hwnd);
						}

						_r_obj_dereference (ptr_app);
					}

					break;
				}

				case IDC_ALLOW_BTN:
				case IDC_BLOCK_BTN:
				case IDC_LATER_BTN:
				{
					if (_r_ctrl_isenabled (hwnd, ctrl_id))
						_app_notify_command (hwnd, ctrl_id, 0);

					break;
				}

				case IDM_OPENRULESEDITOR:
				{
					PITEM_APP ptr_app;
					PITEM_RULE ptr_rule;
					PITEM_LOG ptr_log;
					PR_STRING rule_name;
					PR_STRING rule_string;
					ULONG_PTR app_hash;

					ptr_log = _app_notify_getobject (_app_notify_getapp_id (hwnd));

					if (!ptr_log)
						break;

					app_hash = ptr_log->app_hash;
					ptr_app = _app_getappitem (app_hash);

					if (ptr_app)
					{
						rule_name = _r_obj_createstring (_app_getappdisplayname (ptr_app, TRUE));
					}
					else
					{
						rule_name = NULL;
					}

					rule_string = _app_formataddress (ptr_log->af, 0, &ptr_log->remote_addr, ptr_log->remote_port, FMTADDR_AS_RULE);

					ptr_rule = _app_addrule (rule_name, rule_string, NULL, ptr_log->direction, ptr_log->protocol, ptr_log->af);

					_r_obj_addhashtableitem (ptr_rule->apps, app_hash, NULL);

					if (ptr_app)
						_app_notify_freeobject (ptr_app);

					if (rule_name)
						_r_obj_dereference (rule_name);

					if (rule_string)
						_r_obj_dereference (rule_string);

					_r_obj_dereference (ptr_log);

					_app_ruleenable (ptr_rule, TRUE, TRUE);

					ITEM_CONTEXT context = {0};

					context.is_settorules = TRUE;
					context.ptr_rule = ptr_rule;

					if (DialogBoxParam (NULL, MAKEINTRESOURCE (IDD_EDITOR), _r_app_gethwnd (), &PropertiesProc, (LPARAM)&context))
					{
						SIZE_T rule_idx;

						_r_queuedlock_acquireexclusive (&lock_rules);

						_r_obj_addlistitem_ex (rules_list, _r_obj_reference (ptr_rule), &rule_idx);

						_r_queuedlock_releaseexclusive (&lock_rules);

						// set rule information
						_app_addlistviewrule (_r_app_gethwnd (), ptr_rule, rule_idx, TRUE);

						// update app information
						_app_updateitembylparam (_r_app_gethwnd (), app_hash, TRUE);

						_app_updatelistviewbylparam (_r_app_gethwnd (), DATA_LISTVIEW_CURRENT, PR_UPDATE_TYPE);

						_app_profile_save ();
					}

					if (ptr_app)
						_r_obj_dereference (ptr_app);

					_r_obj_dereference (ptr_rule);

					break;
				}

				case IDM_COPY: // ctrl+c
				case IDM_SELECT_ALL: // ctrl+a
				{
					WCHAR class_name[128];
					HWND hedit;

					hedit = GetFocus ();

					if (!hedit)
						break;

					if (!GetClassName (hedit, class_name, RTL_NUMBER_OF (class_name)))
						break;

					if (_r_str_compare (class_name, WC_EDIT) == 0)
					{
						// edit control hotkey for "ctrl+c" (issue #597)
						if (ctrl_id == IDM_COPY)
						{
							SendMessage (hedit, WM_COPY, 0, 0);
						}
						// edit control hotkey for "ctrl+a"
						else //if (ctrl_id == IDM_SELECT_ALL)
						{
							SendMessage (hedit, EM_SETSEL, 0, (LPARAM)-1);
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
