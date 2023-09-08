// simplewall
// Copyright (c) 2016-2023 Henry++

#include "global.h"

_Success_ (return)
BOOLEAN _app_getappinfo (
	_In_ PITEM_APP ptr_app,
	_In_ ENUM_INFO_DATA info_data,
	_Out_writes_bytes_all_ (size) PVOID buffer,
	_In_ SIZE_T size
)
{
	RtlZeroMemory (buffer, size);

	switch (info_data)
	{
		case INFO_PATH:
		{
			PVOID ptr;

			if (size != sizeof (PVOID))
				return FALSE;

			if (ptr_app->real_path)
			{
				ptr = _r_obj_reference (ptr_app->real_path);

				RtlCopyMemory (buffer, &ptr, size);

				return TRUE;
			}

			break;
		}

		case INFO_DISPLAY_NAME:
		{
			PVOID ptr;

			if (size != sizeof (PVOID))
				return FALSE;

			if (ptr_app->display_name)
			{
				ptr = _r_obj_reference (ptr_app->display_name);

				RtlCopyMemory (buffer, &ptr, size);

				return TRUE;
			}
			else if (ptr_app->original_path)
			{
				ptr = _r_obj_reference (ptr_app->original_path);

				RtlCopyMemory (buffer, &ptr, size);

				return TRUE;
			}

			break;
		}

		case INFO_HASH:
		{
			PVOID ptr;

			if (size != sizeof (PVOID))
				return FALSE;

			if (ptr_app->hash)
			{
				ptr = _r_obj_reference (ptr_app->hash);

				RtlCopyMemory (buffer, &ptr, size);

				return TRUE;
			}

			break;
		}

		case INFO_TIMESTAMP:
		{
			if (size != sizeof (LONG64))
				return FALSE;

			RtlCopyMemory (buffer, &ptr_app->timestamp, size);

			return TRUE;
		}

		case INFO_TIMER:
		{
			if (size != sizeof (LONG64))
				return FALSE;

			RtlCopyMemory (buffer, &ptr_app->timer, size);

			return TRUE;
		}

		case INFO_LISTVIEW_ID:
		{
			INT listview_id;

			if (size != sizeof (INT))
				return FALSE;

			listview_id = _app_listview_getbytype (ptr_app->type);

			RtlCopyMemory (buffer, &listview_id, size);

			return TRUE;
		}

		case INFO_IS_ENABLED:
		{
			BOOLEAN is_enabled;

			if (size != sizeof (BOOLEAN))
				return FALSE;

			is_enabled = !!ptr_app->is_enabled;

			RtlCopyMemory (buffer, &is_enabled, size);

			return TRUE;
		}

		case INFO_IS_SILENT:
		{
			BOOLEAN is_silent;

			if (size != sizeof (BOOLEAN))
				return FALSE;

			is_silent = !!ptr_app->is_silent;

			RtlCopyMemory (buffer, &is_silent, size);

			return TRUE;
		}

		case INFO_IS_UNDELETABLE:
		{
			BOOLEAN is_undeletable;

			if (size != sizeof (BOOLEAN))
				return FALSE;

			is_undeletable = !!ptr_app->is_undeletable;

			RtlCopyMemory (buffer, &is_undeletable, size);

			return TRUE;
		}
	}

	return FALSE;
}

_Success_ (return)
BOOLEAN _app_getappinfobyhash (
	_In_ ULONG_PTR app_hash,
	_In_ ENUM_INFO_DATA info_data,
	_Out_writes_bytes_all_ (size) PVOID buffer,
	_In_ SIZE_T size
)
{
	PITEM_APP ptr_app;
	BOOLEAN is_success;

	ptr_app = _app_getappitem (app_hash);

	if (!ptr_app)
		return FALSE;

	is_success = _app_getappinfo (ptr_app, info_data, buffer, size);

	_r_obj_dereference (ptr_app);

	return is_success;
}

VOID _app_setappinfo (
	_In_ PITEM_APP ptr_app,
	_In_ ENUM_INFO_DATA info_data,
	_In_ PVOID value
)
{
	LONG64 timestamp;

	switch (info_data)
	{
		case INFO_BYTES_DATA:
		{
			_r_obj_movereference (&ptr_app->bytes, value);
			break;
		}

		case INFO_HASH:
		{
			_r_obj_movereference (&ptr_app->hash, value);
			break;
		}

		case INFO_TIMESTAMP:
		{
			ptr_app->timestamp = *((PLONG64)value);
			break;
		}

		case INFO_TIMER:
		{
			timestamp = *((PLONG64)value);

			// check timer expiration
			if (timestamp <= _r_unixtime_now ())
			{
				ptr_app->timer = 0;
				ptr_app->is_enabled = FALSE;
			}
			else
			{
				ptr_app->timer = timestamp;
				ptr_app->is_enabled = TRUE;
			}

			_app_listview_updateitemby_param (_r_app_gethwnd (), ptr_app->app_hash, TRUE);

			break;
		}

		case INFO_DISABLE:
		{
			HANDLE engine_handle;

			ptr_app->is_enabled = FALSE;

			engine_handle = _wfp_getenginehandle ();

			_wfp_destroyfilters_array (engine_handle, ptr_app->guids, DBG_ARG);

			_app_listview_updateitemby_param (_r_app_gethwnd (), ptr_app->app_hash, TRUE);

			break;
		}

		case INFO_IS_ENABLED:
		{
			ptr_app->is_enabled = (PtrToInt (value) ? TRUE : FALSE);

			_app_listview_updateitemby_param (_r_app_gethwnd (), ptr_app->app_hash, TRUE);

			break;
		}

		case INFO_IS_SILENT:
		{
			ptr_app->is_silent = (PtrToInt (value) ? TRUE : FALSE);

			if (ptr_app->is_silent)
				_app_notify_freeobject (NULL, ptr_app);

			break;
		}

		case INFO_IS_UNDELETABLE:
		{
			ptr_app->is_undeletable = (PtrToInt (value) ? TRUE : FALSE);

			break;
		}
	}
}

VOID _app_setappinfobyhash (
	_In_ ULONG_PTR app_hash,
	_In_ ENUM_INFO_DATA info_data,
	_In_ PVOID value
)
{
	PITEM_APP ptr_app;

	ptr_app = _app_getappitem (app_hash);

	if (!ptr_app)
		return;

	_app_setappinfo (ptr_app, info_data, value);

	_r_obj_dereference (ptr_app);
}

_Success_ (return)
BOOLEAN _app_getruleinfo (
	_In_ PITEM_RULE ptr_rule,
	_In_ ENUM_INFO_DATA info_data,
	_Out_writes_bytes_all_ (size) PVOID buffer,
	_In_ SIZE_T size
)
{
	switch (info_data)
	{
		case INFO_LISTVIEW_ID:
		{
			INT listview_id;

			if (size != sizeof (INT))
				return FALSE;

			listview_id = _app_listview_getbytype (ptr_rule->type);

			RtlCopyMemory (buffer, &listview_id, size);

			return TRUE;
		}

		case INFO_IS_READONLY:
		{
			INT is_readonly;

			if (size != sizeof (INT))
				return FALSE;

			is_readonly = !!ptr_rule->is_readonly;

			RtlCopyMemory (buffer, &is_readonly, size);

			return TRUE;
		}
	}

	return FALSE;
}

_Success_ (return)
BOOLEAN _app_getruleinfobyid (
	_In_ SIZE_T index,
	_In_ ENUM_INFO_DATA info_data,
	_Out_writes_bytes_all_ (size) PVOID buffer,
	_In_ SIZE_T size
)
{
	PITEM_RULE ptr_rule;
	BOOLEAN is_success;

	ptr_rule = _app_getrulebyid (index);

	if (!ptr_rule)
		return FALSE;

	is_success = _app_getruleinfo (ptr_rule, info_data, buffer, size);

	_r_obj_dereference (ptr_rule);

	return is_success;
}

_Success_ (return != 0)
ULONG_PTR _app_addapplication (
	_In_opt_ HWND hwnd,
	_In_ ENUM_TYPE_DATA type,
	_In_ PR_STRING path,
	_In_opt_ PR_STRING display_name,
	_In_opt_ PR_STRING real_path
)
{
	WCHAR path_full[1024];
	R_STRINGREF path_temp;
	PITEM_APP ptr_app;
	ULONG_PTR app_hash;
	BOOLEAN is_ntoskrnl;

	if (_r_obj_isstringempty2 (path))
		return 0;

	if (_app_isappvalidpath (path) && PathIsDirectory (path->buffer))
		return 0;

	_r_obj_initializestringref2 (&path_temp, path);

	// prevent possible duplicate apps entries with short path (issue #640)
	if (_r_str_findchar (&path_temp, L'~', FALSE) != SIZE_MAX)
	{
		if (GetLongPathName (path_temp.buffer, path_full, RTL_NUMBER_OF (path_full)))
			_r_obj_initializestringref (&path_temp, path_full);
	}

	app_hash = _r_str_gethash3 (&path_temp, TRUE);

	if (_app_isappfound (app_hash))
		return app_hash; // already exists

	ptr_app = _r_obj_allocate (sizeof (ITEM_APP), &_app_dereferenceapp);
	is_ntoskrnl = (app_hash == config.ntoskrnl_hash);

	ptr_app->app_hash = app_hash;

	if (type == DATA_UNKNOWN)
	{
		if (_r_str_isstartswith2 (&path_temp, L"S-1-", TRUE)) // uwp (win8+)
			type = DATA_APP_UWP;
	}

	if (type == DATA_APP_SERVICE || type == DATA_APP_UWP)
	{
		ptr_app->type = type;

		if (display_name)
			ptr_app->display_name = _r_obj_reference (display_name);

		if (real_path)
			ptr_app->real_path = _r_obj_reference (real_path);
	}
	else if (_r_str_isstartswith2 (&path_temp, L"\\device\\", TRUE)) // device path
	{
		ptr_app->type = DATA_APP_DEVICE;
		ptr_app->real_path = _r_obj_createstring3 (&path_temp);
	}
	else
	{
		if (!is_ntoskrnl && _r_str_findchar (&path_temp, OBJ_NAME_PATH_SEPARATOR, FALSE) == SIZE_MAX)
		{
			ptr_app->type = DATA_APP_PICO;
		}
		else
		{
			ptr_app->type = PathIsNetworkPath (path_temp.buffer) ? DATA_APP_NETWORK : DATA_APP_REGULAR;
		}

		if (is_ntoskrnl)
		{
			ptr_app->real_path = _r_obj_createstring2 (config.ntoskrnl_path);
		}
		else
		{
			ptr_app->real_path = _r_obj_createstring3 (&path_temp);
		}
	}

	ptr_app->original_path = _r_obj_createstring3 (&path_temp);

	// fix "System" lowercase
	if (is_ntoskrnl)
	{
		_r_str_tolower (&ptr_app->original_path->sr);

		ptr_app->original_path->buffer[0] = _r_str_upper (ptr_app->original_path->buffer[0]);
	}

	if (ptr_app->type == DATA_APP_REGULAR || ptr_app->type == DATA_APP_DEVICE || ptr_app->type == DATA_APP_NETWORK)
		ptr_app->short_name = _r_path_getbasenamestring (&path_temp);

	ptr_app->guids = _r_obj_createarray (sizeof (GUID), NULL); // initialize array
	ptr_app->timestamp = _r_unixtime_now ();

	if (ptr_app->type == DATA_APP_SERVICE || ptr_app->type == DATA_APP_UWP)
		ptr_app->is_undeletable = TRUE;

	// insert object into the table
	_r_queuedlock_acquireexclusive (&lock_apps);
	_r_obj_addhashtablepointer (apps_table, app_hash, ptr_app);
	_r_queuedlock_releaseexclusive (&lock_apps);

	// insert item
	if (hwnd)
		_app_listview_addappitem (hwnd, ptr_app);

	// queue file information
	if (ptr_app->real_path)
		_app_queue_fileinformation (ptr_app->real_path, app_hash, ptr_app->type, _app_listview_getbytype (ptr_app->type));

	return app_hash;
}

PITEM_RULE _app_addrule (
	_In_opt_ PR_STRING name,
	_In_opt_ PR_STRING rule_remote,
	_In_opt_ PR_STRING rule_local,
	_In_ FWP_DIRECTION direction,
	_In_ UINT8 protocol,
	_In_ ADDRESS_FAMILY af
)
{
	PITEM_RULE ptr_rule;

	ptr_rule = _r_obj_allocate (sizeof (ITEM_RULE), &_app_dereferencerule);

	ptr_rule->apps = _r_obj_createhashtable (sizeof (SHORT), NULL); // initialize hashtable
	ptr_rule->guids = _r_obj_createarray (sizeof (GUID), NULL); // initialize array

	ptr_rule->type = DATA_RULE_USER;

	// set rule name
	if (name)
	{
		ptr_rule->name = _r_obj_reference (name);

		if (_r_str_getlength2 (ptr_rule->name) > RULE_NAME_CCH_MAX)
			_r_obj_setstringlength (ptr_rule->name, RULE_NAME_CCH_MAX * sizeof (WCHAR));
	}

	// set rule destination
	if (rule_remote)
	{
		ptr_rule->rule_remote = _r_obj_reference (rule_remote);

		if (_r_str_getlength2 (ptr_rule->rule_remote) > RULE_RULE_CCH_MAX)
			_r_obj_setstringlength (ptr_rule->rule_remote, RULE_RULE_CCH_MAX * sizeof (WCHAR));
	}

	// set rule source
	if (rule_local)
	{
		ptr_rule->rule_local = _r_obj_reference (rule_local);

		if (_r_str_getlength2 (ptr_rule->rule_local) > RULE_RULE_CCH_MAX)
			_r_obj_setstringlength (ptr_rule->rule_local, RULE_RULE_CCH_MAX * sizeof (WCHAR));
	}

	// set configuration
	ptr_rule->direction = direction;
	ptr_rule->protocol = protocol;
	ptr_rule->af = af;

	ptr_rule->protocol_str = _app_db_getprotoname (protocol, af, FALSE);

	return ptr_rule;
}

_Ret_maybenull_
PITEM_RULE_CONFIG _app_addruleconfigtable (
	_In_ PR_HASHTABLE hashtable,
	_In_ ULONG_PTR rule_hash,
	_In_ PR_STRING rule_name,
	_In_ BOOLEAN is_enabled
)
{
	ITEM_RULE_CONFIG entry = {0};

	entry.name = _r_obj_reference (rule_name);
	entry.is_enabled = is_enabled;

	return _r_obj_addhashtableitem (hashtable, rule_hash, &entry);
}

_Ret_maybenull_
PITEM_APP _app_getappitem (
	_In_ ULONG_PTR app_hash
)
{
	PITEM_APP ptr_app;

	_r_queuedlock_acquireshared (&lock_apps);
	ptr_app = _r_obj_findhashtablepointer (apps_table, app_hash);
	_r_queuedlock_releaseshared (&lock_apps);

	return ptr_app;
}

_Ret_maybenull_
PITEM_RULE _app_getrulebyid (
	_In_ SIZE_T index
)
{
	PITEM_RULE ptr_rule;

	_r_queuedlock_acquireshared (&lock_rules);

	if (index < _r_obj_getlistsize (rules_list))
	{
		ptr_rule = _r_obj_getlistitem (rules_list, index);
	}
	else
	{
		ptr_rule = NULL;
	}

	_r_queuedlock_releaseshared (&lock_rules);

	return _r_obj_referencesafe (ptr_rule);
}

_Ret_maybenull_
PITEM_RULE _app_getrulebyhash (
	_In_ ULONG_PTR rule_hash
)
{
	PITEM_RULE ptr_rule;

	if (!rule_hash)
		return NULL;

	_r_queuedlock_acquireshared (&lock_rules);

	for (SIZE_T i = 0; i < _r_obj_getlistsize (rules_list); i++)
	{
		ptr_rule = _r_obj_getlistitem (rules_list, i);

		if (ptr_rule)
		{
			if (ptr_rule->is_readonly)
			{
				if (_r_str_gethash2 (ptr_rule->name, TRUE) == rule_hash)
				{
					_r_queuedlock_releaseshared (&lock_rules);

					return _r_obj_reference (ptr_rule);
				}
			}
		}
	}

	_r_queuedlock_releaseshared (&lock_rules);

	return NULL;
}

_Ret_maybenull_
PITEM_RULE_CONFIG _app_getruleconfigitem (
	_In_ ULONG_PTR rule_hash
)
{
	PITEM_RULE_CONFIG ptr_rule_config;

	_r_queuedlock_acquireshared (&lock_rules_config);

	ptr_rule_config = _r_obj_findhashtable (rules_config, rule_hash);

	_r_queuedlock_releaseshared (&lock_rules_config);

	return ptr_rule_config;
}

_Ret_maybenull_
PITEM_LOG _app_getlogitem (
	_In_ ULONG_PTR log_hash
)
{
	PITEM_LOG ptr_log;

	_r_queuedlock_acquireshared (&lock_loglist);

	ptr_log = _r_obj_findhashtablepointer (log_table, log_hash);

	_r_queuedlock_releaseshared (&lock_loglist);

	return ptr_log;
}

_Success_ (return != 0)
ULONG_PTR _app_getlogapp (
	_In_ SIZE_T index
)
{
	PITEM_LOG ptr_log;
	ULONG_PTR app_hash;

	ptr_log = _app_getlogitem (index);

	if (!ptr_log)
		return 0;

	app_hash = ptr_log->app_hash;

	_r_obj_dereference (ptr_log);

	return app_hash;
}

COLORREF _app_getappcolor (
	_In_ INT listview_id,
	_In_ ULONG_PTR app_hash,
	_In_ BOOLEAN is_systemapp,
	_In_ BOOLEAN is_validconnection
)
{
	PITEM_APP ptr_app;
	ULONG_PTR color_hash;
	BOOLEAN is_profilelist;
	BOOLEAN is_networklist;

	ptr_app = _app_getappitem (app_hash);
	color_hash = 0;

	is_profilelist = (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_RULES_CUSTOM);
	is_networklist = (listview_id == IDC_NETWORK || listview_id == IDC_LOG);

	if (ptr_app && !is_networklist)
	{
		if (_r_config_getboolean_ex (L"IsHighlightInvalid", TRUE, L"colors") && !_app_isappexists (ptr_app))
		{
			color_hash = config.color_invalid;
			goto CleanupExit;
		}
	}

	if (_r_config_getboolean_ex (L"IsHighlightConnection", TRUE, L"colors") && is_validconnection)
	{
		color_hash = config.color_network;
		goto CleanupExit;
	}

	if (_r_config_getboolean_ex (L"IsHighlightSigned", TRUE, L"colors") && _app_isappsigned (app_hash))
	{
		color_hash = config.color_signed;
		goto CleanupExit;
	}

	if (ptr_app)
	{
		if (!is_profilelist && (_r_config_getboolean_ex (L"IsHighlightSpecial", TRUE, L"colors") && _app_isapphaverule (app_hash, FALSE)))
		{
			color_hash = config.color_special;
			goto CleanupExit;
		}

		if (_r_config_getboolean_ex (L"IsHighlightPico", TRUE, L"colors") && ptr_app->type == DATA_APP_PICO)
		{
			color_hash = config.color_pico;
			goto CleanupExit;
		}
	}

	if (_r_config_getboolean_ex (L"IsHighlightSystem", TRUE, L"colors") && is_systemapp)
	{
		color_hash = config.color_system;
		goto CleanupExit;
	}

CleanupExit:

	if (ptr_app)
		_r_obj_dereference (ptr_app);

	if (color_hash)
		return _app_getcolorvalue (color_hash);

	return 0;
}

VOID _app_freeapplication (
	_In_opt_ HWND hwnd,
	_In_ ULONG_PTR app_hash
)
{
	PITEM_RULE ptr_rule;

	_r_queuedlock_acquireshared (&lock_rules);

	for (SIZE_T i = 0; i < _r_obj_getlistsize (rules_list); i++)
	{
		ptr_rule = _r_obj_getlistitem (rules_list, i);

		if (!ptr_rule)
			continue;

		if (ptr_rule->type != DATA_RULE_USER)
			continue;

		if (hwnd)
			_app_ruleremoveapp (hwnd, i, ptr_rule, app_hash);
	}

	_r_queuedlock_releaseshared (&lock_rules);

	_r_obj_removehashtablepointer (apps_table, app_hash);
}

VOID _app_getcount (
	_Out_ PITEM_STATUS status
)
{
	PITEM_APP ptr_app = NULL;
	PITEM_RULE ptr_rule;
	SIZE_T enum_key = 0;
	BOOLEAN is_used;

	status->apps_count = 0;
	status->apps_timer_count = 0;
	status->apps_unused_count = 0;
	status->rules_count = 0;
	status->rules_global_count = 0;
	status->rules_predefined_count = 0;
	status->rules_user_count = 0;

	_r_queuedlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtablepointer (apps_table, &ptr_app, NULL, &enum_key))
	{
		is_used = _app_isappused (ptr_app, TRUE);

		if (_app_istimerset (ptr_app))
			status->apps_timer_count += 1;

		if (!ptr_app->is_undeletable && (!_app_isappexists (ptr_app) || !is_used) && !(ptr_app->type == DATA_APP_SERVICE || ptr_app->type == DATA_APP_UWP))
			status->apps_unused_count += 1;

		if (is_used)
			status->apps_count += 1;
	}

	_r_queuedlock_releaseshared (&lock_apps);

	_r_queuedlock_acquireshared (&lock_rules);

	for (SIZE_T i = 0; i < _r_obj_getlistsize (rules_list); i++)
	{
		ptr_rule = _r_obj_getlistitem (rules_list, i);

		if (!ptr_rule)
			continue;

		if (ptr_rule->type == DATA_RULE_USER)
		{
			if (ptr_rule->is_enabled && !_r_obj_ishashtableempty (ptr_rule->apps))
				status->rules_global_count += 1;

			if (ptr_rule->is_readonly)
			{
				status->rules_predefined_count += 1;
			}
			else
			{
				status->rules_user_count += 1;
			}

			status->rules_count += 1;
		}
	}

	_r_queuedlock_releaseshared (&lock_rules);
}

COLORREF _app_getrulecolor (
	_In_ INT listview_id,
	_In_ SIZE_T rule_idx
)
{
	PITEM_RULE ptr_rule;
	ULONG_PTR color_hash;

	ptr_rule = _app_getrulebyid (rule_idx);

	if (!ptr_rule)
		return 0;

	color_hash = 0;

	if (_r_config_getboolean_ex (L"IsHighlightInvalid", TRUE, L"colors") &&
		ptr_rule->is_enabled && ptr_rule->is_haveerrors)
	{
		color_hash = config.color_invalid;
	}
	else if (_r_config_getboolean_ex (L"IsHighlightSpecial", TRUE, L"colors") &&
			 (ptr_rule->is_forservices || !_r_obj_ishashtableempty (ptr_rule->apps)))
	{
		color_hash = config.color_special;
	}

	_r_obj_dereference (ptr_rule);

	if (color_hash)
		return _app_getcolorvalue (color_hash);

	return 0;
}

VOID _app_setappiteminfo (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ INT item_id,
	_In_ PITEM_APP ptr_app
)
{
	_r_listview_setitem_ex (hwnd, listview_id, item_id, 0, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK, I_GROUPIDCALLBACK, 0);

	_r_listview_setitemcheck (hwnd, listview_id, item_id, !!ptr_app->is_enabled);
}

VOID _app_setruleiteminfo (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ INT item_id,
	_In_ PITEM_RULE ptr_rule,
	_In_ BOOLEAN include_apps
)
{
	ULONG_PTR hash_code;
	SIZE_T enum_key;

	_r_listview_setitem_ex (hwnd, listview_id, item_id, 0, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK, I_GROUPIDCALLBACK, 0);

	_r_listview_setitemcheck (hwnd, listview_id, item_id, !!ptr_rule->is_enabled);

	if (!include_apps)
		return;

	enum_key = 0;

	while (_r_obj_enumhashtable (ptr_rule->apps, NULL, &hash_code, &enum_key))
	{
		_app_listview_updateitemby_param (hwnd, hash_code, TRUE);
	}
}

VOID _app_ruleenable (
	_Inout_ PITEM_RULE ptr_rule,
	_In_ BOOLEAN is_enable,
	_In_ BOOLEAN is_createconfig
)
{
	PITEM_RULE_CONFIG ptr_config;
	ULONG_PTR rule_hash;

	ptr_rule->is_enabled = is_enable;

	if (!ptr_rule->is_readonly || !ptr_rule->name)
		return;

	rule_hash = _r_str_gethash2 (ptr_rule->name, TRUE);

	if (!rule_hash)
		return;

	ptr_config = _app_getruleconfigitem (rule_hash);

	if (ptr_config)
	{
		ptr_config->is_enabled = is_enable;
	}
	else
	{
		if (is_createconfig)
		{
			_r_queuedlock_acquireexclusive (&lock_rules_config);
			ptr_config = _app_addruleconfigtable (rules_config, rule_hash, ptr_rule->name, is_enable);
			_r_queuedlock_releaseexclusive (&lock_rules_config);
		}
	}
}

//VOID _app_rulecleanapp (
//	_In_opt_ HWND hwnd,
//	_In_ SIZE_T item_id,
//	_In_ PITEM_RULE ptr_rule,
//	_In_opt_ ULONG_PTR app_hash
//)
//{
//	PITEM_APP ptr_app;
//	SIZE_T enum_key;
//	ULONG_PTR hash_code;
//	BOOLEAN is_remove;
//
//	if (ptr_rule->type != DATA_RULE_USER)
//		return;
//
//	if (_r_obj_ishashtableempty (ptr_rule->apps))
//		return;
//
//	if (app_hash)
//	{
//		_app_ruleremoveapp (hwnd, item_id, ptr_rule, app_hash);
//	}
//	else
//	{
//		enum_key = 0;
//
//		while (_r_obj_enumhashtable (ptr_rule->apps, NULL, &hash_code, &enum_key))
//		{
//			ptr_app = _app_getappitem (hash_code);
//			is_remove = FALSE;
//
//			if (ptr_app)
//			{
//				if (!_app_isappexists (ptr_app))
//					is_remove = TRUE;
//
//				_r_obj_dereference (ptr_app);
//			}
//			else
//			{
//				// app was not found, so delete iteration!
//				is_remove = TRUE;
//			}
//
//			if (is_remove)
//				_app_ruleremoveapp (hwnd, item_id, ptr_rule, hash_code);
//		}
//	}
//}

VOID _app_ruleremoveapp (
	_In_opt_ HWND hwnd,
	_In_ SIZE_T item_id,
	_In_ PITEM_RULE ptr_rule,
	_In_ ULONG_PTR app_hash
)
{
	if (!ptr_rule->apps)
		return;

	if (!_r_obj_removehashtableitem (ptr_rule->apps, app_hash))
		return;

	if (ptr_rule->is_enabled && _r_obj_ishashtableempty (ptr_rule->apps))
	{
		ptr_rule->is_enabled = FALSE;
		ptr_rule->is_haveerrors = FALSE;

		if (hwnd)
		{
			if (item_id != SIZE_MAX)
				_app_listview_updateitemby_param (hwnd, item_id, FALSE);
		}
	}
}

BOOLEAN _app_ruleblocklistsetchange (
	_Inout_ PITEM_RULE ptr_rule,
	_In_ LONG new_state
)
{
	BOOLEAN is_block;

	if (new_state == -1)
		return FALSE; // don't change

	if (new_state == 0 && !ptr_rule->is_enabled)
		return FALSE; // not changed

	is_block = (ptr_rule->action == FWP_ACTION_BLOCK);

	if (new_state == 1 && ptr_rule->is_enabled && !is_block)
		return FALSE; // not changed

	if (new_state == 2 && ptr_rule->is_enabled && is_block)
		return FALSE; // not changed

	ptr_rule->action = (new_state != 1) ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT;

	ptr_rule->is_enabled = (new_state != 0);
	ptr_rule->is_enabled_default = ptr_rule->is_enabled; // set default value for rule

	return TRUE;
}

BOOLEAN _app_ruleblocklistsetstate (
	_Inout_ PITEM_RULE ptr_rule,
	_In_ LONG spy_state,
	_In_ LONG update_state,
	_In_ LONG extra_state
)
{
	if (ptr_rule->type != DATA_RULE_BLOCKLIST || _r_obj_isstringempty (ptr_rule->name))
		return FALSE;

	if (_r_str_isstartswith2 (&ptr_rule->name->sr, L"spy_", TRUE))
		return _app_ruleblocklistsetchange (ptr_rule, spy_state);

	else if (_r_str_isstartswith2 (&ptr_rule->name->sr, L"update_", TRUE))
		return _app_ruleblocklistsetchange (ptr_rule, update_state);

	else if (_r_str_isstartswith2 (&ptr_rule->name->sr, L"extra_", TRUE))
		return _app_ruleblocklistsetchange (ptr_rule, extra_state);

	// fallback: block rules with other names by default!
	return _app_ruleblocklistsetchange (ptr_rule, 2);
}

VOID _app_ruleblocklistset (
	_In_opt_ HWND hwnd,
	_In_ LONG spy_state,
	_In_ LONG update_state,
	_In_ LONG extra_state,
	_In_ BOOLEAN is_instantapply
)
{
	PR_LIST rules;
	PITEM_RULE ptr_rule;
	HANDLE hengine;
	SIZE_T changes_count = 0;

	rules = _r_obj_createlist (&_r_obj_dereference);

	_r_queuedlock_acquireshared (&lock_rules);

	for (SIZE_T i = 0; i < _r_obj_getlistsize (rules_list); i++)
	{
		ptr_rule = _r_obj_getlistitem (rules_list, i);

		if (!ptr_rule)
			continue;

		if (ptr_rule->type != DATA_RULE_BLOCKLIST)
			continue;

		if (!_app_ruleblocklistsetstate (ptr_rule, spy_state, update_state, extra_state))
			continue;

		_app_ruleenable (ptr_rule, !!ptr_rule->is_enabled, FALSE);
		changes_count += 1;

		if (hwnd)
			_app_listview_updateitemby_param (hwnd, i, FALSE);

		if (is_instantapply)
			_r_obj_addlistitem (rules, _r_obj_reference (ptr_rule)); // dereference later!
	}

	_r_queuedlock_releaseshared (&lock_rules);

	if (changes_count)
	{
		if (hwnd)
			_app_listview_updateby_id (hwnd, DATA_RULE_BLOCKLIST, PR_UPDATE_TYPE | PR_UPDATE_NORESIZE);

		if (is_instantapply)
		{
			if (rules->count)
			{
				if (_wfp_isfiltersinstalled ())
				{
					hengine = _wfp_getenginehandle ();

					if (hengine)
						_wfp_create4filters (hengine, rules, DBG_ARG, FALSE);
				}
			}
		}

		_app_profile_save (); // required!
	}

	_r_obj_dereference (rules);
}

_Ret_maybenull_
PR_STRING _app_appexpandrules (
	_In_ ULONG_PTR app_hash,
	_In_ LPCWSTR delimeter
)
{
	R_STRINGBUILDER buffer;
	R_STRINGREF delimeter_sr;
	PR_STRING string;
	PITEM_RULE ptr_rule;

	_r_obj_initializestringbuilder (&buffer, 256);
	_r_obj_initializestringrefconst (&delimeter_sr, delimeter);

	_r_queuedlock_acquireshared (&lock_rules);

	for (SIZE_T i = 0; i < _r_obj_getlistsize (rules_list); i++)
	{
		ptr_rule = _r_obj_getlistitem (rules_list, i);

		if (!ptr_rule)
			continue;

		if (ptr_rule->is_enabled && ptr_rule->type == DATA_RULE_USER &&
			!_r_obj_isstringempty (ptr_rule->name) && _r_obj_findhashtable (ptr_rule->apps, app_hash))
		{
			_r_obj_appendstringbuilder2 (&buffer, ptr_rule->name);

			if (ptr_rule->is_readonly)
				_r_obj_appendstringbuilder (&buffer, SZ_RULE_INTERNAL_MENU);

			_r_obj_appendstringbuilder3 (&buffer, &delimeter_sr);
		}
	}

	_r_queuedlock_releaseshared (&lock_rules);

	string = _r_obj_finalstringbuilder (&buffer);

	_r_str_trimstring (string, &delimeter_sr, 0);

	if (!_r_obj_isstringempty2 (string))
		return string;

	_r_obj_deletestringbuilder (&buffer);

	return NULL;

}

_Ret_maybenull_
PR_STRING _app_rulesexpandapps (
	_In_ PITEM_RULE ptr_rule,
	_In_ BOOLEAN is_fordisplay,
	_In_ LPCWSTR delimeter
)
{
	R_STRINGBUILDER sr;
	R_STRINGREF delimeter_sr;
	PR_STRING string;
	PITEM_APP ptr_app;
	ULONG_PTR hash_code;
	SIZE_T enum_key;

	_r_obj_initializestringbuilder (&sr, 256);

	_r_obj_initializestringrefconst (&delimeter_sr, delimeter);

	if (is_fordisplay && ptr_rule->is_forservices)
	{
		string = _r_obj_concatstrings (
			4,
			PROC_SYSTEM_NAME,
			delimeter,
			_r_obj_getstring (config.svchost_path),
			delimeter
		);

		_r_obj_appendstringbuilder2 (&sr, string);

		_r_obj_dereference (string);
	}

	enum_key = 0;

	while (_r_obj_enumhashtable (ptr_rule->apps, NULL, &hash_code, &enum_key))
	{
		ptr_app = _app_getappitem (hash_code);

		if (!ptr_app)
			continue;

		string = NULL;

		if (is_fordisplay)
		{
			if (ptr_app->type == DATA_APP_UWP)
			{
				if (ptr_app->display_name)
					string = _r_path_compact (ptr_app->display_name, 64);
			}
		}

		if (!string)
		{
			if (is_fordisplay)
			{
				if (ptr_app->original_path)
					string = _r_path_compact (ptr_app->original_path, 64);
			}
		}

		if (!string)
			string = ptr_app->original_path;

		if (string)
		{
			_r_obj_appendstringbuilder2 (&sr, string);
			_r_obj_appendstringbuilder3 (&sr, &delimeter_sr);
		}

		_r_obj_dereference (ptr_app);
	}

	string = _r_obj_finalstringbuilder (&sr);

	_r_str_trimstring (string, &delimeter_sr, PR_TRIM_END_ONLY);

	if (!_r_obj_isstringempty2 (string))
		return string;

	_r_obj_deletestringbuilder (&sr);

	return NULL;
}

_Ret_maybenull_
PR_STRING _app_rulesexpandrules (
	_In_opt_ PR_STRING rule,
	_In_ LPCWSTR delimeter
)
{
	R_STRINGBUILDER sb;
	R_STRINGREF delimeter_sr;
	R_STRINGREF remaining_part;
	R_STRINGREF first_part;
	PR_STRING string;

	if (_r_obj_isstringempty (rule))
		return NULL;

	_r_obj_initializestringbuilder (&sb, 256);

	_r_obj_initializestringrefconst (&delimeter_sr, delimeter);

	_r_obj_initializestringref2 (&remaining_part, rule);

	while (remaining_part.length != 0)
	{
		_r_str_splitatchar (&remaining_part, DIVIDER_RULE[0], &first_part, &remaining_part);

		_r_obj_appendstringbuilder3 (&sb, &first_part);
		_r_obj_appendstringbuilder3 (&sb, &delimeter_sr);
	}

	string = _r_obj_finalstringbuilder (&sb);

	_r_str_trimstring (string, &delimeter_sr, 0);

	if (!_r_obj_isstringempty2 (string))
		return string;

	_r_obj_deletestringbuilder (&sb);

	return NULL;
}

BOOLEAN _app_isappfromsystem (
	_In_opt_ PR_STRING path,
	_In_ ULONG_PTR app_hash
)
{
	if (_app_issystemhash (app_hash))
		return TRUE;

	if (path)
	{
		if (_r_str_isstartswith (&path->sr, &config.windows_dir, TRUE))
			return TRUE;
	}

	return FALSE;
}

BOOLEAN _app_isapphavedrive (
	_In_ INT letter
)
{
	PITEM_APP ptr_app = NULL;
	SIZE_T enum_key;
	INT drive_id;

	enum_key = 0;

	_r_queuedlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtablepointer (apps_table, &ptr_app, NULL, &enum_key))
	{
		if (_r_obj_isstringempty (ptr_app->original_path))
			continue;

		if (ptr_app->type == DATA_APP_REGULAR)
		{
			drive_id = PathGetDriveNumber (ptr_app->original_path->buffer);
		}
		else
		{
			drive_id = -1;
		}

		if (ptr_app->type == DATA_APP_DEVICE || (drive_id != -1 && drive_id == letter))
		{
			if (ptr_app->is_enabled || _app_isapphaverule (ptr_app->app_hash, FALSE))
			{
				_r_queuedlock_releaseshared (&lock_apps);

				return TRUE;
			}
		}
	}

	_r_queuedlock_releaseshared (&lock_apps);

	return FALSE;
}

BOOLEAN _app_isapphaverule (
	_In_ ULONG_PTR app_hash,
	_In_ BOOLEAN is_countdisabled
)
{
	PITEM_RULE ptr_rule;

	_r_queuedlock_acquireshared (&lock_rules);

	for (SIZE_T i = 0; i < _r_obj_getlistsize (rules_list); i++)
	{
		ptr_rule = _r_obj_getlistitem (rules_list, i);

		if (ptr_rule)
		{
			if (ptr_rule->type == DATA_RULE_USER && (is_countdisabled || ptr_rule->is_enabled))
			{
				if (_r_obj_findhashtable (ptr_rule->apps, app_hash))
				{
					_r_queuedlock_releaseshared (&lock_rules);

					return TRUE;
				}
			}
		}
	}

	_r_queuedlock_releaseshared (&lock_rules);

	return FALSE;
}

BOOLEAN _app_isappfileexists (
	_In_ LPCWSTR path
)
{
	PITEM_APP ptr_app = NULL;
	SIZE_T enum_key = 0;
	BOOLEAN is_found = FALSE;

	_r_queuedlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtablepointer (apps_table, &ptr_app, NULL, &enum_key))
	{
		if (_r_str_compare (path, ptr_app->real_path->buffer) == 0)
		{
			is_found = TRUE;

			break;
		}
	}

	_r_queuedlock_releaseshared (&lock_apps);

	return FALSE;
}

BOOLEAN _app_isappexists (
	_In_ PITEM_APP ptr_app
)
{
	if (ptr_app->is_undeletable)
		return TRUE;

	if (ptr_app->is_haveerrors)
		return FALSE;

	switch (ptr_app->type)
	{
		case DATA_APP_REGULAR:
		{
			return ptr_app->real_path && _r_fs_exists (ptr_app->real_path->buffer);
		}

		case DATA_APP_DEVICE:
		case DATA_APP_NETWORK:
		case DATA_APP_PICO:
		{
			if (ptr_app->type == DATA_APP_DEVICE || ptr_app->type == DATA_APP_NETWORK || ptr_app->type == DATA_APP_PICO)
				return TRUE;
		}
	}

	return FALSE;
}

BOOLEAN _app_isappfound (
	_In_ ULONG_PTR app_hash
)
{
	BOOLEAN is_found;

	_r_queuedlock_acquireshared (&lock_apps);

	is_found = (_r_obj_findhashtable (apps_table, app_hash) != NULL);

	_r_queuedlock_releaseshared (&lock_apps);

	return is_found;
}

BOOLEAN _app_isappunused (
	_In_ PITEM_APP ptr_app
)
{
	if (ptr_app->is_undeletable)
		return FALSE;

	if (!_app_isappexists (ptr_app))
		return TRUE;

	if (!_app_isappused (ptr_app, TRUE))
		return TRUE;

	return FALSE;
}

BOOLEAN _app_isappused (
	_In_ PITEM_APP ptr_app,
	_In_ BOOLEAN is_checkall
)
{
	if (is_checkall && ptr_app->is_undeletable)
		return TRUE;

	if (ptr_app->is_enabled || (is_checkall && ptr_app->is_silent))
		return TRUE;

	if (_app_isapphaverule (ptr_app->app_hash, TRUE))
		return TRUE;

	return FALSE;
}

BOOLEAN _app_issystemhash (
	_In_ ULONG_PTR app_hash
)
{
	return (app_hash == config.ntoskrnl_hash || app_hash == config.svchost_hash);
}

BOOLEAN _app_isrulesupportedbyos (
	_In_ PR_STRINGREF os_version
)
{
	static PR_STRING version_string = NULL;

	PR_STRING current_version;
	PR_STRING new_version;

	current_version = InterlockedCompareExchangePointer (&version_string, NULL, NULL);

	if (!current_version)
	{
		new_version = _r_format_string (L"%d.%d", NtCurrentPeb ()->OSMajorVersion, NtCurrentPeb ()->OSMinorVersion);

		current_version = InterlockedCompareExchangePointer (&version_string, new_version, NULL);

		if (!current_version)
		{
			current_version = new_version;
		}
		else
		{
			_r_obj_dereference (new_version);
		}
	}

	return (_r_str_versioncompare (&current_version->sr, os_version) != -1);
}

VOID _app_profile_initialize ()
{
	static R_STRINGREF profile_sr = PR_STRINGREF_INIT (XML_PROFILE_FILE);
	static R_STRINGREF profile_bak_sr = PR_STRINGREF_INIT (XML_PROFILE_FILE L".bak");
	static R_STRINGREF profile_internal_sr = PR_STRINGREF_INIT (XML_PROFILE_INTERNAL);
	static R_STRINGREF separator_sr = PR_STRINGREF_INIT (L"\\");

	PR_STRING path;

	path = _r_app_getprofiledirectory ();

	_r_obj_movereference (&profile_info.profile_path, _r_obj_concatstringrefs (3, &path->sr, &separator_sr, &profile_sr));
	_r_obj_movereference (&profile_info.profile_path_backup, _r_obj_concatstringrefs (3, &path->sr, &separator_sr, &profile_bak_sr));
	_r_obj_movereference (&profile_info.profile_path_internal, _r_obj_concatstringrefs (3, &path->sr, &separator_sr, &profile_internal_sr));
}

NTSTATUS _app_profile_load_fromresource (
	_In_ LPCWSTR resource_name,
	_Out_ PDB_INFORMATION out_buffer
)
{
	PDB_INFORMATION db_info;
	R_STORAGE bytes;
	NTSTATUS status;

	db_info = out_buffer;

	status = _app_db_initialize (db_info, TRUE);

	if (NT_SUCCESS (status))
	{
		status = _r_res_loadresource (_r_sys_getimagebase (), RT_RCDATA, resource_name, &bytes);

		if (NT_SUCCESS (status))
			status = _app_db_openfrombuffer (db_info, &bytes, XML_VERSION_CURRENT, XML_TYPE_PROFILE_INTERNAL);
	}

	return status;
}

VOID _app_profile_load_fallback ()
{
	ULONG_PTR app_hash;

	if (!_app_isappfound (config.my_hash))
	{
		app_hash = _app_addapplication (NULL, DATA_UNKNOWN, config.my_path, NULL, NULL);

		if (app_hash)
			_app_setappinfobyhash (app_hash, INFO_IS_ENABLED, IntToPtr (TRUE));
	}

	_app_setappinfobyhash (config.my_hash, INFO_IS_UNDELETABLE, IntToPtr (TRUE));

	// disable deletion for this shit ;)
	if (!_r_config_getboolean (L"IsInternalRulesDisabled", FALSE))
	{
		if (!_app_isappfound (config.ntoskrnl_hash) && config.system_path)
			_app_addapplication (NULL, DATA_UNKNOWN, config.system_path, NULL, NULL);

		if (!_app_isappfound (config.svchost_hash) && config.svchost_path)
			_app_addapplication (NULL, DATA_UNKNOWN, config.svchost_path, NULL, NULL);

		_app_setappinfobyhash (config.ntoskrnl_hash, INFO_IS_UNDELETABLE, IntToPtr (TRUE));
		_app_setappinfobyhash (config.svchost_hash, INFO_IS_UNDELETABLE, IntToPtr (TRUE));
	}
}

VOID _app_profile_load_internal (
	_In_opt_ HWND hwnd,
	_In_ PR_STRING path,
	_In_ LPCWSTR resource_name,
	_Out_ PLONG64 timestamp
)
{
	DB_INFORMATION db_info_file;
	DB_INFORMATION db_info_buffer;
	PDB_INFORMATION db_info;
	BOOLEAN is_loadfromresource;
	NTSTATUS status_file;
	NTSTATUS status_res;
	NTSTATUS status;

	*timestamp = 0;

	status_file = _app_db_initialize (&db_info_file, TRUE);

	if (_r_fs_exists (path->buffer))
	{
		if (NT_SUCCESS (status_file))
			status_file = _app_db_openfromfile (&db_info_file, path, XML_VERSION_CURRENT, XML_TYPE_PROFILE_INTERNAL);
	}
	else
	{
		status_file = STATUS_OBJECT_PATH_NOT_FOUND;
	}

	status_res = _app_profile_load_fromresource (resource_name, &db_info_buffer);

	// NOTE: prefer new profile version for 3.4+
	if (!NT_SUCCESS (status_file))
	{
		is_loadfromresource = TRUE;
	}
	else
	{
		is_loadfromresource = (db_info_file.version < db_info_buffer.version) || (db_info_file.timestamp < db_info_buffer.timestamp);
	}

	db_info = is_loadfromresource ? &db_info_buffer : &db_info_file;

	status = is_loadfromresource ? status_res : status_file;

	if (NT_SUCCESS (status))
	{
		if (_app_db_parse (db_info, XML_TYPE_PROFILE_INTERNAL))
			*timestamp = db_info->timestamp;
	}
	else
	{
		if (status != STATUS_OBJECT_NAME_NOT_FOUND)
		{
			if (hwnd)
				_r_show_errormessage (hwnd, L"Could not load internal profile!", status, NULL);

			_r_log (LOG_LEVEL_ERROR, NULL, L"_app_profile_load_internal", status, NULL);
		}
	}

	_app_db_destroy (&db_info_file);
	_app_db_destroy (&db_info_buffer);
}

VOID _app_profile_refresh ()
{
	// clear apps
	_r_queuedlock_acquireexclusive (&lock_apps);
	_r_obj_clearhashtable (apps_table);
	_r_queuedlock_releaseexclusive (&lock_apps);

	// clear rules
	_r_queuedlock_acquireexclusive (&lock_rules);
	_r_obj_clearlist (rules_list);
	_r_queuedlock_releaseexclusive (&lock_rules);

	// clear rules config
	_r_queuedlock_acquireexclusive (&lock_rules_config);
	_r_obj_clearhashtable (rules_config);
	_r_queuedlock_releaseexclusive (&lock_rules_config);

	// generate services list
	_app_package_getserviceslist ();

	// generate uwp apps list (win8+)
	_app_package_getpackageslist ();
}

NTSTATUS _app_profile_load (
	_In_opt_ HWND hwnd,
	_In_opt_ PR_STRING path_custom
)
{
	DB_INFORMATION db_info;
	NTSTATUS status;
	BOOLEAN is_update;

	status = _app_db_initialize (&db_info, TRUE);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	if (path_custom)
	{
		status = _app_db_openfromfile (&db_info, path_custom, XML_VERSION_MINIMAL, XML_TYPE_PROFILE);
	}
	else
	{
		_r_queuedlock_acquireshared (&lock_profile);

		status = _app_db_openfromfile (&db_info, profile_info.profile_path, XML_VERSION_MINIMAL, XML_TYPE_PROFILE);

		if (!NT_SUCCESS (status))
			status = _app_db_openfromfile (&db_info, profile_info.profile_path_backup, XML_VERSION_MINIMAL, XML_TYPE_PROFILE);

		_r_queuedlock_releaseshared (&lock_profile);
	}

CleanupExit:

	is_update = (NT_SUCCESS (status) || !path_custom);

	if (is_update)
	{
		if (hwnd)
			_app_listview_clearitems (hwnd);

		_app_profile_refresh ();
	}

	if (NT_SUCCESS (status))
	{
		_app_db_parse (&db_info, XML_TYPE_PROFILE);
	}
	else
	{
		if (status != STATUS_OBJECT_NAME_NOT_FOUND)
		{
			if (hwnd)
				_r_show_errormessage (hwnd, L"Could not load profile!", status, NULL);

			_r_log (LOG_LEVEL_ERROR, NULL, L"_app_profile_load", status, NULL);
		}
	}

	if (is_update)
	{
		// load internal rules (new!)
		if (!_r_config_getboolean (L"IsInternalRulesDisabled", FALSE))
			_app_profile_load_internal (hwnd, profile_info.profile_path_internal, MAKEINTRESOURCE (IDR_PROFILE_INTERNAL), &profile_info.profile_internal_timestamp);

		_app_profile_load_fallback ();

		if (hwnd)
			_app_listview_additems (hwnd);
	}

	_app_db_destroy (&db_info);

	return status;
}

NTSTATUS _app_profile_save ()
{
	DB_INFORMATION db_info;
	LONG64 timestamp;
	NTSTATUS status;
	BOOLEAN is_backuprequired = FALSE;

	status = _app_db_initialize (&db_info, FALSE);

	if (!NT_SUCCESS (status))
	{
		_r_log (LOG_LEVEL_ERROR, NULL, L"_app_db_initialize", status, NULL);

		return status;
	}

	timestamp = _r_unixtime_now ();

	if (_r_config_getboolean (L"IsBackupProfile", TRUE))
	{
		if (!_r_fs_exists (profile_info.profile_path_backup->buffer))
			is_backuprequired = TRUE;

		if (!is_backuprequired)
		{
			if (timestamp - _r_config_getlong64 (L"BackupTimestamp", 0) >= _r_config_getlong64 (L"BackupPeriod", BACKUP_HOURS_PERIOD))
				is_backuprequired = TRUE;
		}
	}

	_r_queuedlock_acquireexclusive (&lock_profile);
	status = _app_db_savetofile (&db_info, profile_info.profile_path, XML_VERSION_CURRENT, XML_TYPE_PROFILE, timestamp);
	_r_queuedlock_releaseexclusive (&lock_profile);

	if (!NT_SUCCESS (status))
		_r_log (LOG_LEVEL_ERROR, NULL, L"_app_db_savetofile", status, profile_info.profile_path->buffer);

	_app_db_destroy (&db_info);

	// make backup
	if (is_backuprequired)
	{
		_r_fs_copyfile (profile_info.profile_path->buffer, profile_info.profile_path_backup->buffer);

		_r_config_setlong64 (L"BackupTimestamp", timestamp);
	}

	return status;
}
