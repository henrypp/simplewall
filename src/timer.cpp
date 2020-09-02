// simplewall
// Copyright (c) 2016-2020 Henry++

#include "global.hpp"

VOID _app_timer_set (HWND hwnd, PITEM_APP ptr_app, time_t seconds)
{
	SIZE_T app_hash = _r_str_hash (ptr_app->original_path); // note: be carefull (!)

	INT listview_id = _app_getlistview_id (ptr_app->type);
	INT item_pos = _app_getposition (hwnd, listview_id, app_hash);

	if (seconds <= 0)
	{
		ptr_app->is_enabled = FALSE;
		ptr_app->is_haveerrors = FALSE;

		ptr_app->timer = 0;

		if (_r_fs_isvalidhandle (ptr_app->htimer))
		{
			DeleteTimerQueueTimer (config.htimer, ptr_app->htimer, NULL);
			ptr_app->htimer = NULL;
		}
	}
	else
	{
		time_t current_time = _r_unixtime_now ();
		BOOLEAN is_created = FALSE;

		if (_r_fs_isvalidhandle (ptr_app->htimer))
		{
			is_created = !!ChangeTimerQueueTimer (config.htimer, ptr_app->htimer, _r_calc_seconds2milliseconds (DWORD, seconds), 0);
		}
		else
		{
			is_created = !!CreateTimerQueueTimer (&ptr_app->htimer, config.htimer, &_app_timer_callback, (PVOID)app_hash, _r_calc_seconds2milliseconds (DWORD, seconds), 0, WT_EXECUTEONLYONCE | WT_EXECUTEINTIMERTHREAD);
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

			if (_r_fs_isvalidhandle (ptr_app->htimer))
			{
				DeleteTimerQueueTimer (config.htimer, ptr_app->htimer, NULL);
				ptr_app->htimer = NULL;
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
	if (!_app_istimeractive (ptr_app))
		return;

	ptr_app->is_enabled = FALSE;
	ptr_app->is_haveerrors = FALSE;

	ptr_app->timer = 0;

	if (_r_fs_isvalidhandle (ptr_app->htimer))
	{
		DeleteTimerQueueTimer (config.htimer, ptr_app->htimer, NULL);
		ptr_app->htimer = NULL;
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

BOOLEAN _app_istimeractive (const PITEM_APP ptr_app)
{
	return _r_fs_isvalidhandle (ptr_app->htimer) || (ptr_app->timer && (ptr_app->timer > _r_unixtime_now ()));
}

BOOLEAN _app_istimersactive ()
{
	for (auto it = apps.begin (); it != apps.end (); ++it)
	{
		PITEM_APP ptr_app = (PITEM_APP)_r_obj_referencesafe (it->second);

		if (!ptr_app)
			continue;

		if (_app_istimeractive (ptr_app))
		{
			_r_obj_dereference (ptr_app);
			return TRUE;
		}

		_r_obj_dereference (ptr_app);
	}

	return FALSE;
}

VOID CALLBACK _app_timer_callback (PVOID lpParameter, BOOLEAN TimerOrWaitFired)
{
	HWND hwnd = _r_app_gethwnd ();
	SIZE_T app_hash = (SIZE_T)lpParameter;

	PITEM_APP ptr_app = _app_getappitem (app_hash);

	if (!ptr_app)
		return;

	OBJECTS_APP_VECTOR rules;
	rules.push_back (ptr_app);

	_app_timer_reset (hwnd, ptr_app);

	HANDLE hengine = _wfp_getenginehandle ();

	if (hengine)
		_wfp_create3filters (hengine, &rules, __LINE__);

	_r_obj_dereference (ptr_app);

	INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

	_app_listviewsort (hwnd, listview_id, INVALID_INT, FALSE);
	_app_refreshstatus (hwnd, listview_id);

	_app_profile_save ();

	_r_listview_redraw (hwnd, listview_id, INVALID_INT);

	if (_r_config_getboolean (L"IsNotificationsTimer", TRUE))
		_r_tray_popupformat (hwnd, UID, NIIF_INFO | (_r_config_getboolean (L"IsNotificationsSound", TRUE) ? 0 : NIIF_NOSOUND), APP_NAME, _r_locale_getstring (IDS_STATUS_TIMER_DONE), _r_obj_getstringordefault (ptr_app->display_name, SZ_EMPTY));
}
