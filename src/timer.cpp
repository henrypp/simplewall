// simplewall
// Copyright (c) 2016-2020 Henry++

#include "global.h"

VOID _app_timer_set (HWND hwnd, PITEM_APP ptr_app, LONG64 seconds)
{
	SIZE_T app_hash = _r_str_hash (ptr_app->original_path); // note: be carefull (!)

	INT listview_id = _app_getlistview_id (ptr_app->type);
	INT item_pos = _app_getposition (hwnd, listview_id, app_hash);

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
		LONG64 current_time = _r_unixtime_now ();
		BOOLEAN is_created = FALSE;
		FILETIME file_time = {0};

		_r_unixtime_to_filetime (current_time + seconds, &file_time);

		if (_app_istimerset (ptr_app->htimer))
		{
			SetThreadpoolTimer (ptr_app->htimer, &file_time, 0, 0);
			is_created = TRUE;
		}
		else
		{
			PTP_TIMER tptimer = CreateThreadpoolTimer (&_app_timer_callback, (PVOID)app_hash, NULL);

			if (tptimer)
			{
				SetThreadpoolTimer (tptimer, &file_time, 0, 0);
				ptr_app->htimer = tptimer;
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

	if (item_pos != INVALID_INT)
	{
		_r_fastlock_acquireshared (&lock_checkbox);

		_r_listview_setitemex (hwnd, listview_id, item_pos, 0, NULL, I_IMAGENONE, _app_getappgroup (app_hash, ptr_app), 0);
		_r_listview_setitemcheck (hwnd, listview_id, item_pos, ptr_app->is_enabled);

		_r_fastlock_releaseshared (&lock_checkbox);
	}
}

VOID _app_timer_reset (HWND hwnd, PITEM_APP ptr_app)
{
	if (!_app_istimerset (ptr_app->htimer))
		return;

	ptr_app->is_enabled = FALSE;
	ptr_app->is_haveerrors = FALSE;

	ptr_app->timer = 0;

	if (_app_istimerset (ptr_app->htimer))
	{
		_app_timer_remove (&ptr_app->htimer);
	}

	SIZE_T app_hash = _r_str_hash (ptr_app->original_path); // note: be carefull (!)
	INT listview_id = _app_getlistview_id (ptr_app->type);

	if (listview_id)
	{
		INT item_pos = _app_getposition (hwnd, listview_id, app_hash);

		if (item_pos != INVALID_INT)
		{
			_r_fastlock_acquireshared (&lock_checkbox);
			_app_setappiteminfo (hwnd, listview_id, item_pos, app_hash, ptr_app);
			_r_fastlock_releaseshared (&lock_checkbox);
		}
	}
}

VOID _app_timer_remove (PTP_TIMER* ptptimer)
{
	PTP_TIMER current_timer = *ptptimer;

	*ptptimer = NULL;

	CloseThreadpoolTimer (current_timer);
}

BOOLEAN _app_istimerset (PTP_TIMER tptimer)
{
	return tptimer && IsThreadpoolTimerSet (tptimer);
}

BOOLEAN _app_istimersactive ()
{
	for (auto it = apps.begin (); it != apps.end (); ++it)
	{
		PITEM_APP ptr_app = (PITEM_APP)_r_obj_referencesafe (it->second);

		if (!ptr_app)
			continue;

		if (_app_istimerset (ptr_app->htimer))
		{
			_r_obj_dereference (ptr_app);
			return TRUE;
		}

		_r_obj_dereference (ptr_app);
	}

	return FALSE;
}

VOID CALLBACK _app_timer_callback (PTP_CALLBACK_INSTANCE instance, PVOID context, PTP_TIMER timer)
{
	HWND hwnd = _r_app_gethwnd ();
	SIZE_T app_hash = (SIZE_T)context;

	PITEM_APP ptr_app = _app_getappitem (app_hash);

	if (!ptr_app)
		return;

	_app_timer_reset (hwnd, ptr_app);

	INT listview_id = _app_getlistview_id (ptr_app->type);

	HANDLE hengine = _wfp_getenginehandle ();

	if (hengine)
	{
		PR_LIST rules = _r_obj_createlist ();

		_r_obj_addlistitem (rules, ptr_app);

		_wfp_create3filters (hengine, rules, __LINE__, FALSE);

		_r_obj_dereference (rules);
	}

	_r_obj_dereference (ptr_app);

	if (listview_id)
	{
		_app_listviewsort (hwnd, listview_id, INVALID_INT, FALSE);
		_app_refreshstatus (hwnd, listview_id);

		_r_listview_redraw (hwnd, listview_id, INVALID_INT);
	}

	_app_profile_save ();

	if (_r_config_getboolean (L"IsNotificationsTimer", TRUE))
		_r_tray_popupformat (hwnd, UID, NIIF_INFO | (_r_config_getboolean (L"IsNotificationsSound", TRUE) ? 0 : NIIF_NOSOUND), APP_NAME, _r_locale_getstring (IDS_STATUS_TIMER_DONE), _app_getdisplayname (0, ptr_app, TRUE));
}
