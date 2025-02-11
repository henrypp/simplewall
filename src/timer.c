// simplewall
// Copyright (c) 2016-2024 Henry++

#include "global.h"

BOOLEAN _app_istimersactive ()
{
	PITEM_APP ptr_app = NULL;
	ULONG_PTR enum_key = 0;

	_r_queuedlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtablepointer (apps_table, &ptr_app, NULL, &enum_key))
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
	PTP_TIMER htimer;
	FILETIME file_time;
	LARGE_INTEGER li;
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

		_r_unixtime_to_filetime (current_time + seconds, &file_time);

		_r_calc_filetime2largeinteger (&file_time, &li);

		if (ptr_app->htimer)
		{
			TpSetTimer (ptr_app->htimer, &li, 0, 0);

			is_created = TRUE;
		}
		else
		{
			status = TpAllocTimer (&htimer, &_app_timer_callback, (PVOID)ptr_app->app_hash, NULL);

			if (NT_SUCCESS (status))
			{
				TpSetTimer (htimer, &li, 0, 0);

				ptr_app->htimer = htimer;

				is_created = TRUE;
			}
		}

		if (is_created)
		{
			ptr_app->is_enabled = TRUE;
			ptr_app->timer = current_time + seconds;
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

VOID CALLBACK _app_timer_callback (
	_Inout_ PTP_CALLBACK_INSTANCE instance,
	_Inout_opt_ PVOID context,
	_Inout_ PTP_TIMER timer
)
{
	HANDLE hengine;
	HWND hwnd;
	PITEM_APP ptr_app;
	PR_LIST rules;
	PR_STRING string;
	WCHAR buffer[256];
	ULONG icon_id;
	HRESULT status;

	ptr_app = _app_getappitem ((ULONG_PTR)context);

	if (!ptr_app)
		return;

	status = CoInitializeEx (NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	hwnd = _r_app_gethwnd ();

	_app_timer_reset (hwnd, ptr_app);

	hengine = _wfp_getenginehandle ();

	rules = _r_obj_createlist (2, NULL);

	_r_obj_addlistitem (rules, ptr_app);

	_wfp_create3filters (hengine, rules, DBG_ARG, FALSE);

	_r_obj_dereference (rules);

	_app_listview_updateby_id (hwnd, ptr_app->type, PR_UPDATE_TYPE | PR_UPDATE_FORCE);

	_r_obj_dereference (ptr_app);

	_app_profile_save (hwnd);

	if (_r_config_getboolean (L"IsNotificationsTimer", TRUE, NULL))
	{
		icon_id = NIIF_INFO;

		if (!_r_config_getboolean (L"IsNotificationsSound", TRUE, NULL))
			icon_id |= NIIF_NOSOUND;

		string = _app_getappdisplayname (ptr_app, TRUE);

		_r_str_printf (
			buffer,
			RTL_NUMBER_OF (buffer),
			L"%s - %s",
			_r_app_getname (),
			_r_obj_getstringorempty (string)
		);

		_r_tray_popup (hwnd, &GUID_TrayIcon, icon_id, buffer, _r_locale_getstring (IDS_STATUS_TIMER_DONE));

		if (string)
			_r_obj_dereference (string);
	}

	if (status == S_OK || status == S_FALSE)
		CoUninitialize ();
}
