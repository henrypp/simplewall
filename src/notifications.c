// simplewall
// Copyright (c) 2016-2023 Henry++

#include "global.h"

_Ret_maybenull_
HWND _app_notify_getwindow (
	_In_opt_ PITEM_LOG ptr_log
)
{
	LARGE_INTEGER timeout;
	HWND current_hwnd;
	HWND new_hwnd;

	current_hwnd = _InterlockedCompareExchangePointer (&config.hnotification, NULL, NULL);

	if (current_hwnd)
	{
		if (ptr_log)
			_app_notify_show (current_hwnd, ptr_log);

		return current_hwnd;
	}

	if (!ptr_log)
		return NULL;

	new_hwnd = _r_wnd_createwindow (_r_sys_getimagebase (), MAKEINTRESOURCE (IDD_NOTIFICATION), NULL, &NotificationProc, ptr_log);

	_r_calc_millisecondstolargeinteger (&timeout, 2000);

	NtWaitForSingleObject (config.hnotify_evt, FALSE, &timeout);

	current_hwnd = _InterlockedCompareExchangePointer (&config.hnotification, NULL, NULL);

	return current_hwnd;
}

_Ret_maybenull_
PNOTIFY_CONTEXT _app_notify_getcontext (
	_In_ HWND hwnd
)
{
	PNOTIFY_CONTEXT context;

	context = _r_wnd_getcontext (hwnd, SHORT_MAX);

	return context;
}

VOID _app_notify_setcontext (
	_In_ HWND hwnd,
	_In_opt_ PNOTIFY_CONTEXT context
)
{
	if (context)
		context->hwnd = hwnd;

	if (context)
	{
		_r_wnd_setcontext (hwnd, SHORT_MAX, context);
	}
	else
	{
		_r_wnd_removecontext (hwnd, SHORT_MAX);
	}
}

BOOLEAN _app_notify_command (
	_In_ HWND hwnd,
	_In_ INT button_id,
	_In_opt_ LONG64 seconds
)
{
	PITEM_APP ptr_app;
	PR_LIST rules;
	HANDLE hengine;
	ULONG_PTR app_hash;

	app_hash = _app_notify_getapp_id (hwnd);
	ptr_app = _app_getappitem (app_hash);

	if (!ptr_app)
		return FALSE;

	_app_notify_freeobject (hwnd, ptr_app);

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
			_app_listview_updateitemby_param (_r_app_gethwnd (), app_hash, TRUE);
		}

		_r_obj_addlistitem (rules, ptr_app);
	}
	else if (button_id == IDC_NEXT_BTN)
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
			hengine = _wfp_getenginehandle ();

			if (hengine)
				_wfp_create3filters (hengine, rules, DBG_ARG, FALSE);
		}
	}

	_app_listview_updateby_id (_r_app_gethwnd (), ptr_app->type, PR_UPDATE_TYPE);

	_r_obj_dereference (ptr_app);
	_r_obj_dereference (rules);

	_app_profile_save (hwnd);

	return TRUE;
}

BOOLEAN _app_notify_addobject (
	_In_ HWND hwnd,
	_In_ PITEM_LOG ptr_log,
	_Inout_ PITEM_APP ptr_app
)
{
	LONG64 current_time;
	LONG64 notification_timeout;

	current_time = _r_unixtime_now ();
	notification_timeout = _r_config_getlong64 (L"NotificationsTimeout", NOTIFY_TIMEOUT_DEFAULT);

	// check for last display time
	if (notification_timeout && ((current_time - ptr_app->last_notify) <= notification_timeout))
		return FALSE;

	ptr_app->last_notify = current_time;

	_r_obj_swapreference (&ptr_app->notification, ptr_log);

	if (SendMessage (hwnd, WM_NOTIFICATION, 0, (LPARAM)ptr_app->notification))
	{
		if (_r_config_getboolean (L"IsNotificationsSound", TRUE))
		{
			if (!_r_config_getboolean (L"IsNotificationsFullscreenSilentMode", TRUE) || !_r_wnd_isfullscreenmode ())
				_app_notify_playsound ();
		}

		return TRUE;
	}

	return FALSE;
}

VOID _app_notify_freeobject (
	_In_opt_ HWND hwnd,
	_Inout_ PITEM_APP ptr_app
)
{
	ULONG_PTR app_hash;

	if (!hwnd)
		hwnd = _app_notify_getwindow (NULL);

	if (ptr_app->notification)
		_r_obj_clearreference (&ptr_app->notification);

	if (!hwnd)
		return;

	app_hash = _app_notify_getnextapp_id (hwnd);

	if (app_hash)
	{
		_app_notify_refresh (hwnd);
	}
	else
	{
		DestroyWindow (hwnd);
	}
}

_Ret_maybenull_
PITEM_LOG _app_notify_getobject (
	_In_ ULONG_PTR app_hash
)
{
	PITEM_APP ptr_app;
	PITEM_LOG ptr_log;

	ptr_app = _app_getappitem (app_hash);

	if (!ptr_app)
		return NULL;

	ptr_log = _r_obj_referencesafe (ptr_app->notification);

	_r_obj_dereference (ptr_app);

	return ptr_log;
}

_Ret_maybenull_
HICON _app_notify_getapp_icon (
	_In_ HWND hwnd
)
{
	PNOTIFY_CONTEXT context;

	context = _app_notify_getcontext (hwnd);

	if (!context)
		return NULL;

	return context->hicon;
}

ULONG_PTR _app_notify_getapp_id (
	_In_ HWND hwnd
)
{
	PNOTIFY_CONTEXT context;

	context = _app_notify_getcontext (hwnd);

	if (!context)
		return 0;

	return context->app_hash;
}

ULONG_PTR _app_notify_getnextapp_id (
	_In_ HWND hwnd
)
{
	PNOTIFY_CONTEXT context;
	PITEM_APP ptr_app = NULL;
	ULONG_PTR enum_key = 0;
	ULONG_PTR app_hash = 0;

	context = _app_notify_getcontext (hwnd);

	if (context)
		app_hash = context->app_hash;

	_r_queuedlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtablepointer (apps_table, &ptr_app, NULL, &enum_key))
	{
		if (!ptr_app->notification)
			continue;

		// exclude current app from enumeration
		if (ptr_app->app_hash == app_hash)
			continue;

		app_hash = ptr_app->app_hash;
		break;
	}

	_r_queuedlock_releaseshared (&lock_apps);

	if (context)
		context->app_hash = app_hash;

	return app_hash;
}

VOID _app_notify_setapp_icon (
	_In_ HWND hwnd,
	_In_opt_ HICON hicon
)
{
	PNOTIFY_CONTEXT context;
	HICON hicon_prev;

	context = _app_notify_getcontext (hwnd);

	if (!context)
		return;

	if (!hicon)
		hicon = _app_icons_getdefaultapp_hicon ();

	hicon_prev = context->hicon;
	context->hicon = hicon;

	RedrawWindow (hwnd, NULL, NULL, RDW_ALLCHILDREN | RDW_ERASE | RDW_INVALIDATE);

	if (hicon_prev)
		DestroyIcon (hicon_prev);
}

VOID _app_notify_setapp_id (
	_In_ HWND hwnd,
	_In_opt_ ULONG_PTR app_hash
)
{
	PNOTIFY_CONTEXT context;

	context = _app_notify_getcontext (hwnd);

	if (!context)
		return;

	context->app_hash = app_hash;
}

VOID _app_notify_show (
	_In_ HWND hwnd,
	_In_ PITEM_LOG ptr_log
)
{
	static R_STRINGREF loading_sr = PR_STRINGREF_INIT (SZ_LOADING);
	static R_STRINGREF empty_sr = PR_STRINGREF_INIT (SZ_EMPTY);

	WCHAR window_title[128];
	PITEM_APP ptr_app;
	PR_STRING localized_string = NULL;
	PR_STRING string = NULL;
	HDWP hdefer;
	BOOLEAN is_fullscreenmode;

	ptr_app = _app_getappitem (ptr_log->app_hash);

	if (!ptr_app)
	{
		DestroyWindow (hwnd);

		return;
	}

	// set notification information
	_app_notify_setapp_id (hwnd, ptr_log->app_hash);
	_app_notify_setapp_icon (hwnd, NULL);

	// set window title
	_r_str_printf (
		window_title,
		RTL_NUMBER_OF (window_title),
		L"%s - %s",
		_r_locale_getstring (IDS_NOTIFY_TITLE),
		_r_app_getname ()
	);

	_r_ctrl_setstring (hwnd, 0, window_title);

	hdefer = BeginDeferWindowPos (2);

	// print name
	_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_NAME), L":"));
	_r_obj_movereference (&string, _app_getappdisplayname (ptr_app, TRUE));

	_r_ctrl_settablestring (hwnd, &hdefer, IDC_FILE_ID, &localized_string->sr, IDC_FILE_TEXT, string ? &string->sr : &empty_sr);

	// print signature
	_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_SIGNATURE), L":"));
	_r_ctrl_settablestring (hwnd, &hdefer, IDC_SIGNATURE_ID, &localized_string->sr, IDC_SIGNATURE_TEXT, &loading_sr);

	// print address
	_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_ADDRESS), L":"));
	_r_ctrl_settablestring (hwnd, &hdefer, IDC_ADDRESS_ID, &localized_string->sr, IDC_ADDRESS_TEXT, &loading_sr);

	// print host
	_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_HOST), L":"));
	_r_ctrl_settablestring (hwnd, &hdefer, IDC_HOST_ID, &localized_string->sr, IDC_HOST_TEXT, &loading_sr);

	// print port
	_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_PORT), L":"));
	_r_obj_movereference (&string, _app_formatport (ptr_log->remote_port, ptr_log->protocol));

	_r_ctrl_settablestring (hwnd, &hdefer, IDC_PORT_ID, &localized_string->sr, IDC_PORT_TEXT, string ? &string->sr : &empty_sr);

	// print direction
	_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_DIRECTION), L":"));
	_r_obj_movereference (&string, _app_db_getdirectionname (ptr_log->direction, ptr_log->is_loopback, TRUE));

	_r_ctrl_settablestring (hwnd, &hdefer, IDC_DIRECTION_ID, &localized_string->sr, IDC_DIRECTION_TEXT, string ? &string->sr : &empty_sr);

	// print filter name
	_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_FILTER), L":"));
	_r_ctrl_settablestring (hwnd, &hdefer, IDC_FILTER_ID, &localized_string->sr, IDC_FILTER_TEXT, ptr_log->filter_name ? &ptr_log->filter_name->sr : &empty_sr);

	// print date
	_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_DATE), L":"));
	_r_obj_movereference (&string, _r_format_unixtime_ex (ptr_log->timestamp, FDTF_SHORTDATE | FDTF_LONGTIME));

	_r_ctrl_settablestring (hwnd, &hdefer, IDC_DATE_ID, &localized_string->sr, IDC_DATE_TEXT, string ? &string->sr : &empty_sr);

	if (hdefer)
		EndDeferWindowPos (hdefer);

	//_r_ctrl_setstring (hwnd, IDC_RULES_BTN, _r_locale_getstring (IDS_TRAY_RULES));
	_r_ctrl_setstring (hwnd, IDC_ALLOW_BTN, _r_locale_getstring (IDS_ACTION_ALLOW));
	_r_ctrl_setstring (hwnd, IDC_BLOCK_BTN, _r_locale_getstring (IDS_ACTION_BLOCK));

	// prevent fullscreen apps lose focus
	is_fullscreenmode = _r_wnd_isfullscreenmode ();

	if (!is_fullscreenmode)
		_r_wnd_top (hwnd, TRUE);

	// set safety timeout
	_app_notify_settimeout (hwnd);

	// set correct position
	_app_notify_setposition (hwnd, FALSE);

	ShowWindow (hwnd, SW_SHOWNOACTIVATE);

	InvalidateRect (hwnd, NULL, TRUE);

	// query busy information
	_app_notify_queueinfo (hwnd, ptr_log);

	if (string)
		_r_obj_dereference (string);

	if (localized_string)
		_r_obj_dereference (localized_string);

	_r_obj_dereference (ptr_app);
}

// Play notification sound even if system have "nosound" mode
VOID _app_notify_playsound ()
{
	static volatile PR_STRING cached_path = NULL;

	PR_STRING current_path;
	PR_STRING new_path;
	PR_STRING expanded_string;
	HANDLE hkey;
	ULONG flags;
	NTSTATUS status;

	current_path = _InterlockedCompareExchangePointer (&cached_path, NULL, NULL);

	if (!current_path || !_r_fs_exists (current_path->buffer))
	{
		status = _r_reg_openkey (HKEY_CURRENT_USER, L"AppEvents\\Schemes\\Apps\\.Default\\" NOTIFY_SOUND_NAME L"\\.Default", KEY_READ, &hkey);

		if (NT_SUCCESS (status))
		{
			status = _r_reg_querystring (hkey, NULL, &new_path);

			if (NT_SUCCESS (status))
			{
				status = _r_str_environmentexpandstring (&new_path->sr, &expanded_string);

				if (NT_SUCCESS (status))
					_r_obj_movereference (&new_path, expanded_string);

				current_path = _InterlockedCompareExchangePointer (&cached_path, new_path, current_path);

				if (current_path)
					_r_obj_dereference (new_path);
			}

			NtClose (hkey);
		}
	}

	flags = SND_ASYNC | SND_NODEFAULT | SND_NOWAIT | SND_SENTRY;

	if (_r_obj_isstringempty (current_path) || !PlaySound (current_path->buffer, NULL, flags | SND_FILENAME))
		PlaySound (NOTIFY_SOUND_NAME, NULL, flags);
}

VOID _app_notify_queueinfo (
	_In_ HWND hwnd,
	_In_ PITEM_LOG ptr_log
)
{
	PITEM_CONTEXT context;

	context = _r_freelist_allocateitem (&context_free_list);

	context->hwnd = hwnd;
	context->base_address = _r_obj_reference (ptr_log);

	_r_workqueue_queueitem (&resolve_notify_queue, &_app_queuenotifyinformation, context);
}

VOID _app_notify_killprocess (
	_In_ HWND hwnd
)
{
	PSYSTEM_PROCESS_INFORMATION spi;
	PSYSTEM_PROCESS_INFORMATION process;
	PNOTIFY_CONTEXT context;
	HANDLE process_handle;
	PITEM_APP ptr_app;
	PR_STRING path;
	NTSTATUS status;

	context = _app_notify_getcontext (hwnd);

	if (!context)
		return;

	ptr_app = _app_getappitem (context->app_hash);

	if (!ptr_app)
		return;

	status = _r_sys_enumprocesses (&spi);

	if (!NT_SUCCESS (status))
	{
		_r_show_errormessage (hwnd, L"Cannot enumerate processes!", status, NULL);

		_r_log (LOG_LEVEL_ERROR, NULL, L"_r_sys_enumprocesses", status, NULL);

		return;
	}

	process = PR_FIRST_PROCESS (spi);

	do
	{
		if (process->ImageName.Buffer)
		{
			if (_r_str_compare (process->ImageName.Buffer, 0, ptr_app->short_name->buffer, 0) == 0)
			{
				status = _r_sys_getprocessimagepath (process->UniqueProcessId, TRUE, &path);

				if (NT_SUCCESS (status))
				{
					if (_r_str_compare (path->buffer, 0, ptr_app->real_path->buffer, 0) == 0)
					{
						status = _r_sys_openprocess (process->UniqueProcessId, PROCESS_TERMINATE, &process_handle);

						if (NT_SUCCESS (status))
						{
							status = NtTerminateProcess (process_handle, STATUS_SUCCESS);

							if (!NT_SUCCESS (status))
								_r_show_errormessage (hwnd, L"Cannot terminate process!", status, NULL);

							NtClose (process_handle);
						}
						else
						{
							_r_show_errormessage (hwnd, L"Cannot open process!", status, NULL);
						}
					}

					_r_obj_dereference (path);
				}
			}
		}
	}
	while (process = PR_NEXT_PROCESS (process));

	_r_mem_free (spi);
}

VOID _app_notify_refresh (
	_In_ HWND hwnd
)
{
	PITEM_LOG ptr_log;
	ULONG_PTR app_hash;

	if (!_r_wnd_isvisible (hwnd))
	{
		DestroyWindow (hwnd);

		return;
	}

	if (!_r_config_getboolean (L"IsNotificationsEnabled", TRUE))
	{
		DestroyWindow (hwnd);

		return;
	}

	app_hash = _app_notify_getapp_id (hwnd);
	ptr_log = _app_notify_getobject (app_hash);

	if (!ptr_log)
	{
		DestroyWindow (hwnd);

		return;
	}

	_app_notify_show (hwnd, ptr_log);

	_r_obj_dereference (ptr_log);
}

VOID _app_notify_setposition (
	_In_ HWND hwnd,
	_In_ BOOLEAN is_forced
)
{
	MONITORINFO monitor_info = {0};
	APPBARDATA taskbar_rect = {0};
	R_RECTANGLE window_rect;
	HMONITOR hmonitor;
	PRECT rect;
	LONG dpi_value;
	LONG border_x;
	BOOLEAN is_intray;

	_r_wnd_getposition (hwnd, &window_rect);

	if (!is_forced && _r_wnd_isvisible (hwnd))
	{
		_r_wnd_adjustrectangletoworkingarea (&window_rect, NULL);
		_r_wnd_setposition (hwnd, &window_rect.position, NULL);

		return;
	}

	is_intray = _r_config_getboolean (L"IsNotificationsOnTray", FALSE);

	if (is_intray)
	{
		monitor_info.cbSize = sizeof (monitor_info);

		hmonitor = MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST);

		if (GetMonitorInfo (hmonitor, &monitor_info))
		{
			taskbar_rect.cbSize = sizeof (taskbar_rect);

			if (SHAppBarMessage (ABM_GETTASKBARPOS, &taskbar_rect))
			{
				dpi_value = _r_dc_getwindowdpi (hwnd);

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

				_r_wnd_adjustrectangletoworkingarea (&window_rect, NULL);
				_r_wnd_setposition (hwnd, &window_rect.position, NULL);

				return;
			}
		}
	}

	// display window on center (depends on error, config etc...)
	_r_wnd_center (hwnd, NULL);
}

VOID _app_notify_settimeout (
	_In_ HWND hwnd
)
{
	_r_ctrl_enable (hwnd, IDC_RULES_BTN, FALSE);
	_r_ctrl_enable (hwnd, IDC_KILLPROCESS_BTN, FALSE);
	_r_ctrl_enable (hwnd, IDC_ALLOW_BTN, FALSE);
	_r_ctrl_enable (hwnd, IDC_BLOCK_BTN, FALSE);
	_r_ctrl_enable (hwnd, IDC_NEXT_BTN, FALSE);

	SetTimer (hwnd, NOTIFY_TIMER_SAFETY_ID, NOTIFY_TIMER_SAFETY_TIMEOUT, NULL);
}

VOID _app_notify_initialize (
	_Inout_ PNOTIFY_CONTEXT context,
	_In_ LONG dpi_value
)
{
	NONCLIENTMETRICS ncm = {0};
	LONG icon_small;
	LONG icon_large;

	// destroy previous resources
	SAFE_DELETE_OBJECT (context->hfont_title);
	SAFE_DELETE_OBJECT (context->hfont_link);
	SAFE_DELETE_OBJECT (context->hfont_text);

	SAFE_DELETE_OBJECT (context->hbmp_allow);
	SAFE_DELETE_OBJECT (context->hbmp_block);
	SAFE_DELETE_OBJECT (context->hbmp_cross);
	SAFE_DELETE_OBJECT (context->hbmp_next);
	SAFE_DELETE_OBJECT (context->hbmp_rules);

	icon_small = _r_dc_getsystemmetrics (SM_CXSMICON, dpi_value);
	icon_large = _r_dc_getsystemmetrics (SM_CXICON, dpi_value);

	// set window icon
	_r_wnd_seticon (
		context->hwnd,
		_r_sys_loadsharedicon (NULL, MAKEINTRESOURCE (SIH_EXCLAMATION), icon_small),
		_r_sys_loadsharedicon (NULL, MAKEINTRESOURCE (SIH_EXCLAMATION), icon_large)
	);

	// set window font
	ncm.cbSize = sizeof (ncm);

	if (_r_dc_getsystemparametersinfo (SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, dpi_value))
	{
		context->hfont_title = _app_createfont (&ncm.lfCaptionFont, 12, FALSE, dpi_value);
		context->hfont_link = _app_createfont (&ncm.lfMessageFont, 9, TRUE, dpi_value);
		context->hfont_text = _app_createfont (&ncm.lfMessageFont, 9, FALSE, dpi_value);

		SendMessage (context->hwnd, WM_SETFONT, (WPARAM)context->hfont_text, TRUE);

		SendDlgItemMessage (context->hwnd, IDC_HEADER_ID, WM_SETFONT, (WPARAM)context->hfont_title, TRUE);
		SendDlgItemMessage (context->hwnd, IDC_FILE_TEXT, WM_SETFONT, (WPARAM)context->hfont_link, TRUE);

		for (INT i = IDC_SIGNATURE_TEXT; i <= IDC_DATE_TEXT; i++)
			SendDlgItemMessage (context->hwnd, i, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, 0);

		for (INT i = IDC_SIGNATURE_TEXT; i <= IDC_NEXT_BTN; i++)
			SendDlgItemMessage (context->hwnd, i, WM_SETFONT, (WPARAM)context->hfont_text, TRUE);
	}

	// load images
	context->hbmp_rules = _app_bitmapfrompng (_r_sys_getimagebase (), MAKEINTRESOURCE (IDP_SETTINGS), icon_small);
	context->hbmp_allow = _app_bitmapfrompng (_r_sys_getimagebase (), MAKEINTRESOURCE (IDP_ALLOW), icon_small);
	context->hbmp_block = _app_bitmapfrompng (_r_sys_getimagebase (), MAKEINTRESOURCE (IDP_BLOCK), icon_small);
	context->hbmp_cross = _app_bitmapfrompng (_r_sys_getimagebase (), MAKEINTRESOURCE (IDP_CROSS), icon_small);
	context->hbmp_next = _app_bitmapfrompng (_r_sys_getimagebase (), MAKEINTRESOURCE (IDP_NEXT), icon_small);

	// set button configuration
	SendDlgItemMessage (context->hwnd, IDC_RULES_BTN, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)context->hbmp_rules);
	SendDlgItemMessage (context->hwnd, IDC_KILLPROCESS_BTN, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)context->hbmp_cross);
	SendDlgItemMessage (context->hwnd, IDC_ALLOW_BTN, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)context->hbmp_allow);
	SendDlgItemMessage (context->hwnd, IDC_BLOCK_BTN, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)context->hbmp_block);
	SendDlgItemMessage (context->hwnd, IDC_NEXT_BTN, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)context->hbmp_next);

	_r_ctrl_setbuttonmargins (context->hwnd, IDC_RULES_BTN, dpi_value);
	_r_ctrl_setbuttonmargins (context->hwnd, IDC_KILLPROCESS_BTN, dpi_value);
	_r_ctrl_setbuttonmargins (context->hwnd, IDC_ALLOW_BTN, dpi_value);
	_r_ctrl_setbuttonmargins (context->hwnd, IDC_BLOCK_BTN, dpi_value);
	_r_ctrl_setbuttonmargins (context->hwnd, IDC_NEXT_BTN, dpi_value);
}

VOID _app_notify_destroy (
	_In_ HWND hwnd
)
{
	PNOTIFY_CONTEXT context;

	context = _app_notify_getcontext (hwnd);

	if (!context)
		return;

	_app_notify_setcontext (hwnd, NULL);

	SAFE_DELETE_ICON (context->hicon);

	SAFE_DELETE_OBJECT (context->hfont_title);
	SAFE_DELETE_OBJECT (context->hfont_link);
	SAFE_DELETE_OBJECT (context->hfont_text);

	SAFE_DELETE_OBJECT (context->hbmp_allow);
	SAFE_DELETE_OBJECT (context->hbmp_block);
	SAFE_DELETE_OBJECT (context->hbmp_cross);
	SAFE_DELETE_OBJECT (context->hbmp_rules);

	_r_mem_free (context);
}

VOID _app_notify_drawgradient (
	_In_ HDC hdc,
	_In_ LPCRECT rect
)
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

INT_PTR CALLBACK NotificationProc (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
)
{
	switch (msg)
	{
		case WM_INITDIALOG:
		{
			PNOTIFY_CONTEXT context;
			PITEM_LOG ptr_log;
			HWND current_hwnd;
			HWND htip;
			LONG dpi_value;

			current_hwnd = _InterlockedCompareExchangePointer (&config.hnotification, hwnd, config.hnotification);

			if (current_hwnd)
				DestroyWindow (current_hwnd);

			// initialize window context
			context = _r_mem_allocate (sizeof (NOTIFY_CONTEXT));

			_app_notify_setcontext (hwnd, context);

			// set correct position
			_app_notify_setposition (hwnd, FALSE);

			dpi_value = _r_dc_getwindowdpi (hwnd);

			_app_notify_initialize (context, dpi_value);

			// initialize tips
			htip = _r_ctrl_createtip (hwnd);

			if (htip)
			{
				_r_ctrl_settiptext (htip, hwnd, IDC_FILE_TEXT, LPSTR_TEXTCALLBACK);
				_r_ctrl_settiptext (htip, hwnd, IDC_RULES_BTN, LPSTR_TEXTCALLBACK);
				_r_ctrl_settiptext (htip, hwnd, IDC_KILLPROCESS_BTN, LPSTR_TEXTCALLBACK);
				_r_ctrl_settiptext (htip, hwnd, IDC_ALLOW_BTN, LPSTR_TEXTCALLBACK);
				_r_ctrl_settiptext (htip, hwnd, IDC_BLOCK_BTN, LPSTR_TEXTCALLBACK);
			}

			// display log information
			ptr_log = (PITEM_LOG)lparam;

			_app_notify_show (hwnd, ptr_log);

			NtSetEvent (config.hnotify_evt, NULL);

			_r_obj_dereference (ptr_log);

			break;
		}

		case WM_CLOSE:
		{
			DestroyWindow (hwnd);
			break;
		}

		case WM_NCDESTROY:
		{
			_InterlockedCompareExchangePointer (&config.hnotification, NULL, config.hnotification);

			_app_notify_destroy (hwnd);

			break;
		}

		case WM_TIMER:
		{
			KillTimer (hwnd, wparam);

			if (wparam != NOTIFY_TIMER_SAFETY_ID)
				break;

			_r_ctrl_enable (hwnd, IDC_RULES_BTN, TRUE);
			_r_ctrl_enable (hwnd, IDC_KILLPROCESS_BTN, TRUE);
			_r_ctrl_enable (hwnd, IDC_ALLOW_BTN, TRUE);
			_r_ctrl_enable (hwnd, IDC_BLOCK_BTN, TRUE);
			_r_ctrl_enable (hwnd, IDC_NEXT_BTN, TRUE);

			break;
		}

		case WM_DPICHANGED:
		{
			PNOTIFY_CONTEXT context;

			context = _app_notify_getcontext (hwnd);

			_r_wnd_message_dpichanged (hwnd, wparam, lparam);

			if (context)
				_app_notify_initialize (context, LOWORD (wparam));

			_app_notify_refresh (hwnd);

			break;
		}

		case WM_SETTINGCHANGE:
		{
			_r_wnd_message_settingchange (hwnd, wparam, lparam);
			break;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc;

			hdc = BeginPaint (hwnd, &ps);

			if (!hdc)
				break;

			_r_dc_drawwindow (hdc, hwnd, 0, TRUE);

			EndPaint (hwnd, &ps);

			break;
		}

		case WM_ERASEBKGND:
		{
			return TRUE;
		}

		case WM_CTLCOLORDLG:
		case WM_CTLCOLORSTATIC:
		{
			HDC hdc;
			INT text_clr;

			hdc = (HDC)wparam;

			if (msg == WM_CTLCOLORSTATIC && (GetDlgCtrlID ((HWND)lparam) == IDC_FILE_TEXT))
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
			RECT rect;
			LONG dpi_value;
			LONG icon_size_x;
			LONG wnd_spacing;
			PR_STRING string;
			HICON hicon;

			draw_info = (LPDRAWITEMSTRUCT)lparam;

			if (draw_info->CtlID != IDC_HEADER_ID)
				break;

			dpi_value = _r_dc_getwindowdpi (hwnd);

			icon_size_x = _r_dc_getsystemmetrics (SM_CXICON, dpi_value);
			wnd_spacing = _r_dc_getdpi (12, dpi_value);

			SetBkMode (draw_info->hDC, TRANSPARENT); // HACK!!!

			// draw title gradient
			_app_notify_drawgradient (draw_info->hDC, &draw_info->rcItem);

			// draw title text
			string = _r_locale_getstring_ex (IDS_NOTIFY_HEADER);

			if (string)
			{
				SetTextColor (draw_info->hDC, RGB (255, 255, 255));

				SetRect (
					&rect,
					wnd_spacing,
					0,
					_r_calc_rectwidth (&draw_info->rcItem) - (wnd_spacing * 3) - icon_size_x,
					_r_calc_rectheight (&draw_info->rcItem)
				);

				DrawTextEx (
					draw_info->hDC,
					string->buffer,
					(INT)(INT_PTR)_r_str_getlength2 (string),
					&rect,
					DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOCLIP | DT_NOPREFIX,
					NULL
				);

				_r_obj_dereference (string);
			}

			// draw title icon
			hicon = _app_notify_getapp_icon (hwnd);

			if (hicon)
			{
				SetRect (
					&rect,
					_r_calc_rectwidth (&draw_info->rcItem) - icon_size_x - wnd_spacing,
					(_r_calc_rectheight (&draw_info->rcItem) / 2) - (icon_size_x / 2),
					icon_size_x,
					icon_size_x
				);

				DrawIconEx (
					draw_info->hDC,
					rect.left,
					rect.top,
					hicon,
					rect.right,
					rect.bottom,
					0,
					NULL,
					DI_IMAGE | DI_MASK
				);
			}

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
			HCURSOR hcursor;
			LONG_PTR exstyle;

			exstyle = _r_wnd_getstyle_ex (hwnd);

			if (!(exstyle & WS_EX_LAYERED))
				_r_wnd_setstyle_ex (hwnd, exstyle | WS_EX_LAYERED);

			hcursor = LoadCursor (NULL, (msg == WM_ENTERSIZEMOVE) ? IDC_SIZEALL : IDC_ARROW);

			SetLayeredWindowAttributes (hwnd, 0, (msg == WM_ENTERSIZEMOVE) ? 150 : 255, LWA_ALPHA);
			SetCursor (hcursor);

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
					PNOTIFY_CONTEXT context;
					PITEM_LOG ptr_log;
					R_RECTANGLE rectangle;
					RECT rect;
					HMENU hsubmenu;
					INT ctrl_id;

					ctrl_id = (INT)(INT_PTR)nmlp->idFrom;

					if (!_r_ctrl_isenabled (hwnd, ctrl_id))
						break;

					if (ctrl_id != IDC_ALLOW_BTN && ctrl_id != IDC_RULES_BTN)
						break;

					context = _app_notify_getcontext (hwnd);

					if (!context)
						break;

					ptr_log = _app_notify_getobject (context->app_hash);

					hsubmenu = CreatePopupMenu ();

					if (!hsubmenu)
						break;

					if (ctrl_id == IDC_RULES_BTN)
					{
						_app_generate_rulescontrol (hsubmenu, context->app_hash, ptr_log);
					}
					else if (ctrl_id == IDC_ALLOW_BTN)
					{
						_app_generate_timerscontrol (hsubmenu, context->app_hash);
					}

					if (GetClientRect (nmlp->hwndFrom, &rect))
					{
						ClientToScreen (nmlp->hwndFrom, (PPOINT)&rect);

						_r_wnd_recttorectangle (&rectangle, &rect);
						_r_wnd_adjustrectangletoworkingarea (&rectangle, nmlp->hwndFrom);
						_r_wnd_rectangletorect (&rect, &rectangle);

						_r_menu_popup (hsubmenu, hwnd, (PPOINT)&rect, TRUE);
					}

					DestroyMenu (hsubmenu);

					break;
				}

				case TTN_GETDISPINFO:
				{
					LPNMTTDISPINFO lpnmdi;

					WCHAR buffer[1024] = {0};
					PR_STRING string;
					ULONG_PTR app_hash;
					INT ctrl_id;
					INT listview_id = 0;

					lpnmdi = (LPNMTTDISPINFO)lparam;

					if ((lpnmdi->uFlags & TTF_IDISHWND) == 0)
						break;

					ctrl_id = GetDlgCtrlID ((HWND)lpnmdi->hdr.idFrom);

					if (ctrl_id == IDC_FILE_TEXT)
					{
						app_hash = _app_notify_getapp_id (hwnd);

						if (_app_getappinfobyhash (app_hash, INFO_LISTVIEW_ID, &listview_id, sizeof (listview_id)))
						{
							string = _app_gettooltipbylparam (_r_app_gethwnd (), listview_id, app_hash);

							if (string)
							{
								_r_str_copy (buffer, RTL_NUMBER_OF (buffer), string->buffer);
								_r_obj_dereference (string);
							}
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
					else if (ctrl_id == IDC_KILLPROCESS_BTN)
					{
						_r_str_copy (buffer, RTL_NUMBER_OF (buffer), _r_locale_getstring (IDS_ACTION_TERMINATE_HINT));
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

					if (!_r_str_isempty2 (buffer))
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

			if (ctrl_id >= IDX_RULES_SPECIAL && ctrl_id <= IDX_RULES_SPECIAL + (INT)(INT_PTR)_r_obj_getlistsize (rules_list) + 1)
			{
				HANDLE hengine;
				PR_LIST rules;
				PITEM_RULE ptr_rule;
				PITEM_APP ptr_app;
				PITEM_LOG ptr_log;
				PR_STRING rule;
				ULONG_PTR rule_idx;
				ULONG_PTR app_hash;
				BOOLEAN is_remove;

				app_hash = _app_notify_getapp_id (hwnd);
				ptr_app = _app_getappitem (app_hash);

				if (!ptr_app)
					return FALSE;

				rule_idx = (ULONG_PTR)ctrl_id - IDX_RULES_SPECIAL;
				ptr_rule = _app_getrulebyid (rule_idx);

				if (!ptr_rule)
				{
					ptr_log = _app_notify_getobject (app_hash);

					if (ptr_log)
					{
						rule = _app_formataddress (
							ptr_log->af,
							0,
							&ptr_log->remote_addr,
							ptr_log->remote_port,
							FMTADDR_AS_RULE
						);

						ptr_rule = _app_addrule (NULL, NULL, NULL, FWP_DIRECTION_OUTBOUND, ptr_log->protocol, ptr_log->af);

						ptr_rule->name = _r_obj_createstring2 (rule);
						ptr_rule->rule_remote = _r_obj_createstring2 (rule);

						ptr_rule->action = FWP_ACTION_PERMIT;

						_app_ruleenable (ptr_rule, TRUE, FALSE);

						_r_obj_addhashtableitem (ptr_rule->apps, app_hash, NULL);

						_r_obj_dereference (rule);

						_r_queuedlock_acquireexclusive (&lock_rules);
						_r_obj_addlistitem_ex (rules_list, ptr_rule, &rule_idx);
						_r_queuedlock_releaseexclusive (&lock_rules);

						if (rule_idx != SIZE_MAX)
						{
							_app_listview_addruleitem (_r_app_gethwnd (), ptr_rule, rule_idx, TRUE);
							_app_listview_updateby_id (_r_app_gethwnd (), DATA_LISTVIEW_CURRENT, PR_UPDATE_TYPE);

							_app_profile_save (hwnd);
						}
					}

					_app_notify_freeobject (hwnd, ptr_app);

					break;
				}

				_app_notify_freeobject (hwnd, ptr_app);

				if (!(ptr_rule->is_forservices && _app_issystemhash (app_hash)))
				{
					is_remove = ptr_rule->is_enabled &&
						_r_obj_findhashtable (ptr_rule->apps, app_hash);

					if (is_remove)
					{
						_r_obj_removehashtableitem (ptr_rule->apps, app_hash);

						if (_r_obj_isempty (ptr_rule->apps))
							_app_ruleenable (ptr_rule, FALSE, TRUE);
					}
					else
					{
						_r_obj_addhashtableitem (ptr_rule->apps, app_hash, NULL);

						_app_ruleenable (ptr_rule, TRUE, TRUE);
					}

					_app_listview_updateitemby_param (_r_app_gethwnd (), app_hash, TRUE);
					_app_listview_updateitemby_param (_r_app_gethwnd (), rule_idx, FALSE);

					if (_wfp_isfiltersinstalled ())
					{
						hengine = _wfp_getenginehandle ();

						if (hengine)
						{
							rules = _r_obj_createlist (NULL);

							_r_obj_addlistitem (rules, ptr_rule);

							_wfp_create4filters (hengine, rules, DBG_ARG, FALSE);

							_r_obj_dereference (rules);
						}
					}

					_app_listview_updateby_id (_r_app_gethwnd (), DATA_LISTVIEW_CURRENT, PR_UPDATE_TYPE);

					_r_obj_dereference (ptr_app);
					_r_obj_dereference (ptr_rule);

					_app_profile_save (hwnd);
				}

				return FALSE;
			}
			else if (ctrl_id >= IDX_TIMER && ctrl_id <= (IDX_TIMER + (RTL_NUMBER_OF (timer_array) - 1)))
			{
				ULONG_PTR timer_idx;
				LONG64 seconds;

				if (!_r_ctrl_isenabled (hwnd, IDC_ALLOW_BTN))
					return FALSE;

				timer_idx = (ULONG_PTR)ctrl_id - IDX_TIMER;
				seconds = timer_array[timer_idx];

				_app_notify_command (hwnd, IDC_ALLOW_BTN, seconds);

				return FALSE;
			}

			switch (ctrl_id)
			{
				case IDCANCEL: // process Esc key
				{
					DestroyWindow (hwnd);

					break;
				}

				case IDC_FILE_TEXT:
				{
					ULONG_PTR app_hash;

					app_hash = _app_notify_getapp_id (hwnd);

					if (app_hash)
						_app_listview_showitemby_param (_r_app_gethwnd (), app_hash, TRUE);

					break;
				}

				case IDC_RULES_BTN:
				{
					PEDITOR_CONTEXT context;
					PITEM_APP ptr_app;
					ULONG_PTR app_hash;

					app_hash = _app_notify_getapp_id (hwnd);
					ptr_app = _app_getappitem (app_hash);

					if (!ptr_app)
						break;

					context = _app_editor_createwindow (_r_app_gethwnd (), ptr_app, 1, FALSE);

					if (context)
						_app_editor_deletewindow (context);

					_app_notify_freeobject (hwnd, ptr_app);

					_r_obj_dereference (ptr_app);

					break;
				}

				case IDC_ALLOW_BTN:
				case IDC_BLOCK_BTN:
				case IDC_NEXT_BTN:
				{
					if (_r_ctrl_isenabled (hwnd, ctrl_id))
						_app_notify_command (hwnd, ctrl_id, 0);

					break;
				}

				case IDC_KILLPROCESS_BTN:
				{
					_app_notify_killprocess (hwnd);

					_r_ctrl_enable (hwnd, ctrl_id, FALSE);

					break;
				}

				case IDM_OPENRULESEDITOR:
				{
					PEDITOR_CONTEXT context;
					PITEM_APP ptr_app;
					PITEM_RULE ptr_rule;
					PITEM_LOG ptr_log;
					PR_STRING app_name;
					PR_STRING rule_name;
					PR_STRING rule_string;
					ULONG_PTR app_hash;
					ULONG_PTR rule_idx;

					app_hash = _app_notify_getapp_id (hwnd);
					ptr_log = _app_notify_getobject (app_hash);

					if (!ptr_log)
						break;

					app_hash = ptr_log->app_hash;
					ptr_app = _app_getappitem (app_hash);

					if (!ptr_app)
					{
						_r_obj_dereference (ptr_log);

						break;
					}

					app_name = _app_getappdisplayname (ptr_app, TRUE);

					rule_string = _app_formataddress (ptr_log->af, 0, &ptr_log->remote_addr, ptr_log->remote_port, FMTADDR_AS_RULE);

					rule_name = _r_format_string (L"%s - %s", _r_obj_getstring (app_name), _r_obj_getstring (rule_string));

					ptr_rule = _app_addrule (rule_name, rule_string, NULL, ptr_log->direction, ptr_log->protocol, ptr_log->af);

					_r_obj_addhashtableitem (ptr_rule->apps, app_hash, NULL);

					_app_ruleenable (ptr_rule, TRUE, TRUE);

					context = _app_editor_createwindow (_r_app_gethwnd (), ptr_rule, 0, TRUE);

					if (context)
					{
						_r_queuedlock_acquireexclusive (&lock_rules);
						_r_obj_addlistitem_ex (rules_list, _r_obj_reference (ptr_rule), &rule_idx);
						_r_queuedlock_releaseexclusive (&lock_rules);

						// set rule information
						_app_listview_addruleitem (_r_app_gethwnd (), ptr_rule, rule_idx, TRUE);

						// update app information
						_app_listview_updateitemby_param (_r_app_gethwnd (), app_hash, TRUE);

						_app_listview_updateby_id (_r_app_gethwnd (), DATA_LISTVIEW_CURRENT, PR_UPDATE_TYPE);

						_app_profile_save (hwnd);

						_app_editor_deletewindow (context);
					}

					_app_notify_freeobject (hwnd, ptr_app);

					if (app_name)
						_r_obj_dereference (app_name);

					if (rule_string)
						_r_obj_dereference (rule_string);

					_r_obj_dereference (rule_name);
					_r_obj_dereference (ptr_rule);
					_r_obj_dereference (ptr_log);
					_r_obj_dereference (ptr_app);

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

					if (_r_str_compare (class_name, 0, WC_EDIT, 0) != 0)
						break;

					// edit control hotkey for "ctrl+c" (issue #597)
					if (ctrl_id == IDM_COPY)
					{
						SendMessage (hedit, WM_COPY, 0, 0);
					}
					// edit control hotkey for "ctrl+a"
					else if (ctrl_id == IDM_SELECT_ALL)
					{
						SendMessage (hedit, EM_SETSEL, 0, (LPARAM)-1);
					}

					break;
				}
			}

			break;
		}
	}

	return FALSE;
}
