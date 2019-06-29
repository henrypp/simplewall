// simplewall
// Copyright (c) 2016-2019 Henry++

#include "global.hpp"

bool _app_timer_set (HWND hwnd, PITEM_APP ptr_app, time_t seconds)
{
	if (!config.hengine || !ptr_app)
		return false;

	if (seconds <= 0)
	{
		ptr_app->timer = 0;
		return false;
	}

	const time_t current_time = _r_unixtime_now ();

	const size_t app_hash = _r_str_hash (ptr_app->original_path); // note: be carefull (!)
	BOOL is_created = FALSE;

	if (ptr_app->htimer)
	{
		is_created = ChangeTimerQueueTimer (config.htimer, ptr_app->htimer, DWORD (seconds * _R_SECONDSCLOCK_MSEC), 0);
	}
	else
	{
		is_created = CreateTimerQueueTimer (&ptr_app->htimer, config.htimer, &_app_timer_callback, (PVOID)app_hash, DWORD (seconds * _R_SECONDSCLOCK_MSEC), 0, WT_EXECUTEONLYONCE | WT_EXECUTEINTIMERTHREAD);
	}

	if (is_created)
	{
		ptr_app->is_enabled = true;
		ptr_app->timer = current_time + seconds;
	}
	else
	{
		ptr_app->is_enabled = false;
		ptr_app->timer = 0;

		if (ptr_app->htimer)
		{
			DeleteTimerQueueTimer (config.htimer, ptr_app->htimer, nullptr);
			ptr_app->htimer = nullptr;
		}
	}

	const UINT listview_id = _app_getlistview_id (ptr_app->type);
	const size_t item = _app_getposition (hwnd, listview_id, app_hash);

	if (item != LAST_VALUE)
	{
		_r_fastlock_acquireshared (&lock_checkbox);

		_r_listview_setitem (hwnd, listview_id, item, 0, nullptr, LAST_VALUE, _app_getappgroup (app_hash, ptr_app));
		_r_listview_setitemcheck (hwnd, listview_id, item, ptr_app->is_enabled);

		_r_fastlock_releaseshared (&lock_checkbox);
	}

	return true;
}

bool _app_timer_reset (HWND hwnd, PITEM_APP ptr_app)
{
	if (!config.hengine || !ptr_app)
		return false;

	if (!_app_istimeractive (ptr_app))
		return false;

	ptr_app->is_haveerrors = false;

	if (ptr_app->htimer)
	{
		DeleteTimerQueueTimer (config.htimer, ptr_app->htimer, nullptr);
		ptr_app->htimer = nullptr;
	}

	ptr_app->is_enabled = false;
	ptr_app->timer = 0;

	const size_t app_hash = _r_str_hash (ptr_app->original_path); // note: be carefull (!)
	const UINT listview_id = _app_getlistview_id (ptr_app->type);

	if (listview_id)
	{
		const size_t item = _app_getposition (hwnd, listview_id, app_hash);

		if (item != LAST_VALUE)
		{
			_r_fastlock_acquireshared (&lock_checkbox);
			_app_setappiteminfo (hwnd, listview_id, item, app_hash, ptr_app);
			_r_fastlock_releaseshared (&lock_checkbox);
		}
	}

	return true;
}

bool _app_istimeractive (PITEM_APP ptr_app)
{
	return ptr_app->htimer || (ptr_app->timer && (ptr_app->timer > _r_unixtime_now ()));
}

bool _app_istimersactive ()
{
	_r_fastlock_acquireshared (&lock_access);

	for (auto &p : apps)
	{
		PR_OBJECT ptr_app_object = _r_obj_reference (p.second);

		if (!ptr_app_object)
			continue;

		PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

		if (ptr_app && _app_istimeractive (ptr_app))
		{
			_r_obj_dereference (ptr_app_object, &_app_dereferenceapp);
			_r_fastlock_releaseshared (&lock_access);

			return true;
		}

		_r_obj_dereference (ptr_app_object, &_app_dereferenceapp);
	}

	_r_fastlock_releaseshared (&lock_access);

	return false;
}

void CALLBACK _app_timer_callback (PVOID lparam, BOOLEAN)
{
	const HWND hwnd = app.GetHWND ();
	const size_t app_hash = (size_t)lparam;

	_r_fastlock_acquireshared (&lock_access);
	PR_OBJECT ptr_app_object = _app_getappitem (app_hash);
	_r_fastlock_releaseshared (&lock_access);

	if (!ptr_app_object)
		return;

	PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

	if (!ptr_app)
	{
		_r_obj_dereference (ptr_app_object, &_app_dereferenceapp);
		return;
	}

	OBJECTS_VEC rules;
	rules.push_back (ptr_app_object);

	_app_timer_reset (hwnd, ptr_app);
	_wfp_create3filters (rules, __LINE__);

	const UINT listview_id = _app_gettab_id (hwnd);

	_app_listviewsort (hwnd, listview_id);

	_app_refreshstatus (hwnd);
	_app_profile_save ();

	_r_listview_redraw (hwnd, listview_id);

	if (app.ConfigGet (L"IsNotificationsTimer", true).AsBool ())
		app.TrayPopup (hwnd, UID, nullptr, NIIF_USER | (app.ConfigGet (L"IsNotificationsSound", true).AsBool () ? 0 : NIIF_NOSOUND), APP_NAME, _r_fmt (app.LocaleString (IDS_STATUS_TIMER_DONE, nullptr), ptr_app->display_name));
}
