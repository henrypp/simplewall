// simplewall
// Copyright (c) 2016-2021 Henry++

#include "global.h"

VOID _app_timer_set (HWND hwnd, PITEM_APP ptr_app, LONG64 seconds)
{
	INT listview_id = _app_getlistview_id (ptr_app->type);
	INT item_pos = _app_getposition (hwnd, listview_id, ptr_app->app_hash);

	if (seconds <= 0)
	{
		ptr_app->is_enabled = FALSE;
		ptr_app->is_haveerrors = FALSE;

		ptr_app->timer = 0;

		if (_app_istimerset (ptr_app->htimer))
		{
			_app_timer_remove (&ptr_app->htimer);
		}
	}
	else
	{
		FILETIME file_time = {0};
		LONG64 current_time = _r_unixtime_now ();
		BOOLEAN is_created = FALSE;

		_r_unixtime_to_filetime (current_time + seconds, &file_time);

		if (_app_istimerset (ptr_app->htimer))
		{
			SetThreadpoolTimer (ptr_app->htimer, &file_time, 0, 0);
			is_created = TRUE;
		}
		else
		{
			PTP_TIMER timer = CreateThreadpoolTimer (&_app_timer_callback, (PVOID)ptr_app->app_hash, NULL);

			if (timer)
			{
				SetThreadpoolTimer (timer, &file_time, 0, 0);
				ptr_app->htimer = timer;
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
			ptr_app->is_enabled = FALSE;
			ptr_app->is_haveerrors = FALSE;
			ptr_app->timer = 0;

			if (_app_istimerset (ptr_app->htimer))
			{
				_app_timer_remove (&ptr_app->htimer);
			}
		}
	}

	if (item_pos != -1)
	{
		_r_spinlock_acquireshared (&lock_checkbox);

		_r_listview_setitemex (hwnd, listview_id, item_pos, 0, NULL, I_IMAGENONE, _app_getappgroup (ptr_app), 0);
		_r_listview_setitemcheck (hwnd, listview_id, item_pos, ptr_app->is_enabled);

		_r_spinlock_releaseshared (&lock_checkbox);
	}
}

VOID _app_timer_reset (HWND hwnd, PITEM_APP ptr_app)
{
	ptr_app->is_enabled = FALSE;
	ptr_app->is_haveerrors = FALSE;

	ptr_app->timer = 0;

	if (_app_istimerset (ptr_app->htimer))
	{
		_app_timer_remove (&ptr_app->htimer);
	}

	INT listview_id = _app_getlistview_id (ptr_app->type);

	if (listview_id)
	{
		INT item_pos = _app_getposition (hwnd, listview_id, ptr_app->app_hash);

		if (item_pos != -1)
		{
			_r_spinlock_acquireshared (&lock_checkbox);
			_app_setappiteminfo (hwnd, listview_id, item_pos, ptr_app);
			_r_spinlock_releaseshared (&lock_checkbox);
		}
	}
}

VOID _app_timer_remove (PTP_TIMER* timer)
{
	PTP_TIMER current_timer = *timer;

	*timer = NULL;

	CloseThreadpoolTimer (current_timer);
}

BOOLEAN _app_istimerset (PTP_TIMER timer)
{
	return timer && IsThreadpoolTimerSet (timer);
}

BOOLEAN _app_istimersactive ()
{
	PITEM_APP ptr_app;
	SIZE_T enum_key = 0;

	while (_r_obj_enumhashtable (apps, &ptr_app, NULL, &enum_key))
	{
		if (_app_istimerset (ptr_app->htimer))
			return TRUE;
	}

	return FALSE;
}

VOID CALLBACK _app_timer_callback (PTP_CALLBACK_INSTANCE instance, PVOID context, PTP_TIMER timer)
{
	HANDLE hengine;
	HWND hwnd;
	PITEM_APP ptr_app;
	PR_LIST rules;
	INT listview_id;

	ptr_app = _r_obj_findhashtable (apps, (SIZE_T)context);

	if (!ptr_app)
		return;

	hwnd = _r_app_gethwnd ();

	_app_timer_reset (hwnd, ptr_app);

	listview_id = _app_getlistview_id (ptr_app->type);
	hengine = _wfp_getenginehandle ();

	if (hengine)
	{
		rules = _r_obj_createlist (NULL);

		_r_obj_addlistitem (rules, ptr_app);

		_wfp_create3filters (hengine, rules, __LINE__, FALSE);

		_r_obj_dereference (rules);
	}

	if (listview_id)
	{
		_app_listviewsort (hwnd, listview_id, -1, FALSE);
		_app_refreshstatus (hwnd, listview_id);

		_r_listview_redraw (hwnd, listview_id, -1);
	}

	_app_profile_save ();

	if (_r_config_getboolean (L"IsNotificationsTimer", TRUE))
		_r_tray_popupformat (hwnd, UID, NIIF_INFO | (_r_config_getboolean (L"IsNotificationsSound", TRUE) ? 0 : NIIF_NOSOUND), APP_NAME, _r_locale_getstring (IDS_STATUS_TIMER_DONE), _app_getdisplayname (ptr_app, TRUE));
}
