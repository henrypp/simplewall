// simplewall
// Copyright (c) 2016-2026 Henry++

#include "global.h"

BOOLEAN _app_istimersactive ()
{
	PITEM_APP ptr_app = NULL;
	ULONG_PTR enum_key = 0;

	_r_queuedlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtablepointer (apps_table, (PVOID_PTR)&ptr_app, NULL, &enum_key))
	{
		if (_app_istimerset (ptr_app))
		{
			_r_queuedlock_releaseshared (&lock_apps);

			return TRUE;
		}
	}

	_r_queuedlock_releaseshared (&lock_apps);

	return FALSE;
}

BOOLEAN _app_istimerset (
	_In_ PITEM_APP ptr_app
)
{
	if (!ptr_app->htimer)
		return FALSE;

	return !!IsThreadpoolTimerSet (ptr_app->htimer);
}

VOID _app_timer_set (
	_In_opt_ HWND hwnd,
	_Inout_ PITEM_APP ptr_app,
	_In_ LONG64 seconds
)
{
	LARGE_INTEGER timestamp;
	FILETIME file_time;
	PTP_TIMER htimer;
	LONG64 current_time;
	BOOLEAN is_created = FALSE;
	NTSTATUS status;

	if (seconds <= 0)
	{
		_app_timer_reset (NULL, ptr_app);
	}
	else
	{
		current_time = _r_unixtime_now ();

		_r_unixtime_to_filetime (&file_time, current_time + seconds);

		_r_calc_filetime2largeinteger (&timestamp, &file_time);

		if (ptr_app->htimer)
		{
			TpSetTimer (ptr_app->htimer, &timestamp, 0, 0);

			is_created = TRUE;
		}
		else
		{
			status = TpAllocTimer (&htimer, &_app_timer_callback, ULongToPtr (ptr_app->app_hash), NULL);

			if (NT_SUCCESS (status))
			{
				TpSetTimer (htimer, &timestamp, 0, 0);

				ptr_app->htimer = htimer;

				is_created = TRUE;
			}
			else
			{
				if (hwnd)
				{
					_r_show_errormessage (hwnd, L"Could not allocate timer!", status, NULL, ET_NATIVE);
				}
				else
				{
					_r_log (LOG_LEVEL_ERROR, NULL, L"TpAllocTimer", status, NULL);
				}
			}
		}

		if (is_created)
		{
			ptr_app->is_enabled = TRUE;
			ptr_app->timer = (current_time + seconds);
		}
		else
		{
			_app_timer_reset (NULL, ptr_app);
		}
	}

	if (hwnd)
		_app_listview_updateitemby_param (hwnd, ptr_app->app_hash, TRUE);
}

VOID _app_timer_reset (
	_In_opt_ HWND hwnd,
	_Inout_ PITEM_APP ptr_app
)
{
	ptr_app->is_enabled = FALSE;
	ptr_app->is_haveerrors = FALSE;

	ptr_app->timer = 0;

	if (_app_istimerset (ptr_app))
		_app_timer_remove (ptr_app);

	if (hwnd)
		_app_listview_updateitemby_param (hwnd, ptr_app->app_hash, TRUE);
}

VOID _app_timer_remove (
	_Inout_ PITEM_APP ptr_app
)
{
	PTP_TIMER current_timer;

	current_timer = ptr_app->htimer;

	ptr_app->htimer = NULL;

	if (current_timer)
		TpReleaseTimer (current_timer);
}

VOID NTAPI _app_timer_callback (
	_Inout_ PTP_CALLBACK_INSTANCE instance,
	_Inout_opt_ PVOID context,
	_Inout_ PTP_TIMER timer
)
{
	WCHAR buffer[0x100];
	PR_STRING display_name;
	PITEM_APP ptr_app;
	PR_LIST rules;
	HWND hwnd;
	ULONG icon_id = NIIF_INFO;
	HRESULT status;

	ptr_app = _app_getappitem (PtrToUlong (context));

	if (!ptr_app)
		return;

	status = CoInitializeEx (NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	hwnd = _r_app_gethwnd ();

	_app_timer_reset (hwnd, ptr_app);

	rules = _r_obj_createlist (0x01, NULL);

	_r_obj_addlistitem (rules, ptr_app, NULL);

	_wfp_createappfilters (_wfp_getenginehandle (), rules, DBG_ARG, FALSE);

	if (hwnd)
		_app_listview_updateby_id (hwnd, ptr_app->type, PR_UPDATE_TYPE | PR_UPDATE_FORCE);

	_app_profile_save (hwnd);

	if (_r_config_getboolean (L"IsNotificationsTimer", TRUE, NULL))
	{
		if (!_r_config_getboolean (L"IsNotificationsSound", TRUE, NULL))
			icon_id |= NIIF_NOSOUND;

		display_name = _app_getappdisplayname (ptr_app, TRUE);

		_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%s - %s", _r_app_getname (), _r_obj_getstringordefault (display_name, L"<noname>"));

		_r_tray_popup (hwnd, &GUID_TrayIcon, icon_id, buffer, _r_locale_getstring (IDS_STATUS_TIMER_DONE));

		if (display_name)
			_r_obj_dereference (display_name);
	}

	if (status == S_OK || status == S_FALSE)
		CoUninitialize ();

	_r_obj_dereference (ptr_app);
	_r_obj_dereference (rules);
}
