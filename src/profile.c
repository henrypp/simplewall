// simplewall
// Copyright (c) 2016-2026 Henry++

#include "global.h"

_Success_ (return)
BOOLEAN _app_getappinfo (
	_In_ PITEM_APP ptr_app,
	_In_ ENUM_INFO_DATA info_data,
	_Out_writes_bytes_all_ (length) PVOID buffer,
	_In_ ULONG_PTR length
)
{
	RtlZeroMemory (buffer, length);

	switch (info_data)
	{
		case INFO_PATH:
		{
			PVOID ptr;

			if (length != sizeof (PR_STRING))
				return FALSE;

			if (ptr_app->real_path)
			{
				ptr = _r_obj_reference (ptr_app->real_path);

				RtlCopyMemory (buffer, &ptr, length);

				return TRUE;
			}

			break;
		}

		case INFO_DISPLAY_NAME:
		{
			PVOID ptr;

			if (length != sizeof (PR_STRING))
				return FALSE;

			if (ptr_app->display_name)
			{
				ptr = _r_obj_reference (ptr_app->display_name);

				RtlCopyMemory (buffer, &ptr, length);

				return TRUE;
			}
			else if (ptr_app->original_path)
			{
				ptr = _r_obj_reference (ptr_app->original_path);

				RtlCopyMemory (buffer, &ptr, length);

				return TRUE;
			}

			break;
		}

		case INFO_HASH:
		{
			PVOID ptr;

			if (length != sizeof (PR_STRING))
				return FALSE;

			if (ptr_app->hash)
			{
				ptr = _r_obj_reference (ptr_app->hash);

				RtlCopyMemory (buffer, &ptr, length);

				return TRUE;
			}

			break;
		}

		case INFO_TIMESTAMP:
		{
			if (length != sizeof (LONG64))
				return FALSE;

			RtlCopyMemory (buffer, &ptr_app->timestamp, length);

			return TRUE;
		}

		case INFO_TIMER:
		{
			if (length != sizeof (LONG64))
				return FALSE;

			RtlCopyMemory (buffer, &ptr_app->timer, length);

			return TRUE;
		}

		case INFO_LISTVIEW_ID:
		{
			INT listview_id;

			if (length != sizeof (INT))
				return FALSE;

			listview_id = _app_listview_getbytype (ptr_app->type);

			if (listview_id == 0)
				return FALSE;

			RtlCopyMemory (buffer, &listview_id, length);

			return TRUE;
		}

		case INFO_IS_ENABLED:
		{
			BOOLEAN is_enabled;

			if (length != sizeof (BOOLEAN))
				return FALSE;

			is_enabled = (ptr_app->is_enabled != 0);

			RtlCopyMemory (buffer, &is_enabled, length);

			return TRUE;
		}

		case INFO_IS_SILENT:
		{
			BOOLEAN is_silent;

			if (length != sizeof (BOOLEAN))
				return FALSE;

			is_silent = (ptr_app->is_silent != 0);

			RtlCopyMemory (buffer, &is_silent, length);

			return TRUE;
		}

		case INFO_IS_UNDELETABLE:
		{
			BOOLEAN is_undeletable;

			if (length != sizeof (BOOLEAN))
				return FALSE;

			is_undeletable = (ptr_app->is_undeletable != 0);

			RtlCopyMemory (buffer, &is_undeletable, length);

			return TRUE;
		}

		default:
		{
			break;
		}
	}

	return FALSE;
}

_Success_ (return)
BOOLEAN _app_getappinfobyhash (
	_In_ ULONG app_hash,
	_In_ ENUM_INFO_DATA info_data,
	_Out_writes_bytes_all_ (length) PVOID buffer,
	_In_ ULONG_PTR length
)
{
	PITEM_APP ptr_app;
	BOOLEAN is_success;

	RtlZeroMemory (buffer, length);

	ptr_app = _app_getappitem (app_hash);

	if (!ptr_app)
		return FALSE;

	is_success = _app_getappinfo (ptr_app, info_data, buffer, length);

	_r_obj_dereference (ptr_app);

	return is_success;
}

_Success_ (return)
BOOLEAN _app_setappinfo (
	_In_ PITEM_APP ptr_app,
	_In_ ENUM_INFO_DATA info_data,
	_In_opt_ PVOID value
)
{
	switch (info_data)
	{
		case INFO_BYTES_DATA:
		{
			_r_obj_movereference ((PVOID_PTR)&ptr_app->bytes, value);

			return TRUE;
		}

		case INFO_COMMENT:
		{
			_r_obj_movereference ((PVOID_PTR)&ptr_app->comment, value);

			return TRUE;
		}

		case INFO_HASH:
		{
			_r_obj_movereference ((PVOID_PTR)&ptr_app->hash, value);

			return TRUE;
		}

		case INFO_TIMESTAMP:
		{
			if (!value)
				return FALSE;

			ptr_app->timestamp = *((PLONG64)value);

			return TRUE;
		}

		case INFO_TIMER:
		{
			LONG64 timestamp;

			if (!value)
				return FALSE;

			timestamp = *((PLONG64)value);

			// check timer expiration
			if (timestamp <= _r_unixtime_now ())
			{
				_wfp_destroyfilters_array (_wfp_getenginehandle (), ptr_app->guids, DBG_ARG);

				ptr_app->timer = 0;
				ptr_app->is_enabled = FALSE;
			}
			else
			{
				ptr_app->timer = timestamp;
				ptr_app->is_enabled = TRUE;
			}

			_app_listview_updateitemby_param (_r_app_gethwnd (), ptr_app->app_hash, TRUE);

			return TRUE;
		}

		case INFO_DISABLE:
		{
			ptr_app->is_enabled = FALSE;

			_wfp_destroyfilters_array (_wfp_getenginehandle (), ptr_app->guids, DBG_ARG);

			_app_listview_updateitemby_param (_r_app_gethwnd (), ptr_app->app_hash, TRUE);

			return TRUE;
		}

		case INFO_IS_ENABLED:
		{
			ptr_app->is_enabled = (PtrToLong (value) ? TRUE : FALSE);

			_app_listview_updateitemby_param (_r_app_gethwnd (), ptr_app->app_hash, TRUE);

			return TRUE;
		}

		case INFO_IS_SILENT:
		{
			ptr_app->is_silent = (PtrToLong (value) ? TRUE : FALSE);

			if (ptr_app->is_silent)
				_app_notify_freeobject (NULL, ptr_app);

			return TRUE;
		}

		case INFO_IS_UNDELETABLE:
		{
			ptr_app->is_undeletable = (PtrToLong (value) ? TRUE : FALSE);

			return TRUE;
		}

		default:
		{
			break;
		}
	}

	return FALSE;
}

_Success_ (return)
BOOLEAN _app_setappinfobyhash (
	_In_ ULONG app_hash,
	_In_ ENUM_INFO_DATA info_data,
	_In_opt_ PVOID value
)
{
	PITEM_APP ptr_app;
	BOOLEAN is_success;

	ptr_app = _app_getappitem (app_hash);

	if (!ptr_app)
		return FALSE;

	is_success = _app_setappinfo (ptr_app, info_data, value);

	_r_obj_dereference (ptr_app);

	return is_success;
}

_Success_ (return)
BOOLEAN _app_getruleinfo (
	_In_ PITEM_RULE ptr_rule,
	_In_ ENUM_INFO_DATA info_data,
	_Out_writes_bytes_all_ (length) PVOID buffer,
	_In_ ULONG_PTR length
)
{
	RtlZeroMemory (buffer, length);

	switch (info_data)
	{
		case INFO_LISTVIEW_ID:
		{
			INT listview_id;

			if (length != sizeof (INT))
				return FALSE;

			listview_id = _app_listview_getbytype (ptr_rule->type);

			if (listview_id == 0)
				return FALSE;

			RtlCopyMemory (buffer, &listview_id, length);

			return TRUE;
		}

		case INFO_IS_READONLY:
		{
			BOOLEAN is_readonly;

			if (length != sizeof (BOOLEAN))
				return FALSE;

			is_readonly = !!ptr_rule->is_readonly;

			RtlCopyMemory (buffer, &is_readonly, length);

			return TRUE;
		}

		default:
		{
			return FALSE;
		}
	}
}

_Success_ (return)
BOOLEAN _app_getruleinfobyid (
	_In_ ULONG_PTR index,
	_In_ ENUM_INFO_DATA info_data,
	_Out_writes_bytes_all_ (length) PVOID buffer,
	_In_ ULONG_PTR length
)
{
	PITEM_RULE ptr_rule;
	BOOLEAN is_success;

	ptr_rule = _app_getrulebyid (index);

	if (!ptr_rule)
		return FALSE;

	is_success = _app_getruleinfo (ptr_rule, info_data, buffer, length);

	_r_obj_dereference (ptr_rule);

	return is_success;
}

_Ret_maybenull_
PITEM_APP _app_addapplication (
	_In_opt_ HWND hwnd,
	_In_ ENUM_TYPE_DATA type,
	_In_ PR_STRING path,
	_In_opt_ PR_STRING display_name,
	_In_opt_ PR_STRING real_path
)
{
	WCHAR path_full[0x400];
	R_STRINGREF path_sr;
	PITEM_APP ptr_app;
	ULONG app_hash;
	BOOLEAN is_ntoskrnl;

	if (_r_obj_isstringempty2 (path) || (_app_isappvalidpath (path) && _r_fs_isdirectory (&path->sr)))
		return NULL;

	_r_obj_initializestringref2 (&path_sr, &path->sr);

	// prevent possible duplicate apps entries with short path (issue #640)
	if (_r_str_findchar (&path_sr, L'~', FALSE) != SIZE_MAX)
	{
		if (GetLongPathNameW (path_sr.buffer, path_full, RTL_NUMBER_OF (path_full)))
			_r_obj_initializestringref (&path_sr, path_full);
	}

	app_hash = _r_str_gethash (&path_sr, TRUE);

	if (_app_isappfound (app_hash))
		return _app_getappitem (app_hash); // already exists

	ptr_app = (PITEM_APP)_r_obj_allocate (sizeof (ITEM_APP), &_app_dereferenceapp);
	is_ntoskrnl = (app_hash == config.ntoskrnl_hash);

	ptr_app->app_hash = app_hash;

	if (type == DATA_UNKNOWN)
	{
		if (_r_str_isstartswith2 (&path_sr, L"S-1-", TRUE)) // uwp (win8+)
			type = DATA_APP_UWP;
	}

	if (type == DATA_APP_SERVICE || type == DATA_APP_UWP)
	{
		ptr_app->type = type;

		if (display_name)
			ptr_app->display_name = _r_obj_createstring2 (&display_name->sr);

		if (real_path)
			ptr_app->real_path = _r_obj_createstring2 (&real_path->sr);
	}
	else if (_r_str_isstartswith2 (&path_sr, L"\\device\\", TRUE)) // device path
	{
		ptr_app->type = DATA_APP_DEVICE;
		ptr_app->real_path = _r_obj_createstring2 (&path_sr);
	}
	else
	{
		if (!is_ntoskrnl && _r_str_findchar (&path_sr, OBJ_NAME_PATH_SEPARATOR, FALSE) == SIZE_MAX)
		{
			ptr_app->type = DATA_APP_PICO;
		}
		else
		{
			ptr_app->type = _r_path_isnetwork (&path_sr) ? DATA_APP_NETWORK : DATA_APP_REGULAR;
		}

		ptr_app->real_path = _r_obj_createstring2 (is_ntoskrnl ? &config.ntoskrnl_path->sr : &path_sr);
	}

	ptr_app->original_path = _r_obj_createstring2 (&path_sr);

	// fix "System" lowercase
	if (is_ntoskrnl)
	{
		_r_str_tolower (&ptr_app->original_path->sr);

		ptr_app->original_path->buffer[0] = _r_str_upper (ptr_app->original_path->buffer[0]);
	}

	//if (ptr_app->type == DATA_APP_REGULAR || ptr_app->type == DATA_APP_DEVICE || ptr_app->type == DATA_APP_NETWORK)
	{
		ptr_app->short_name = _r_path_getbasenamestring (&path_sr);
	}

	ptr_app->guids = _r_obj_createarray (sizeof (GUID), 0x02, NULL); // initialize guids array
	ptr_app->timestamp = _r_unixtime_now ();

	// insert object into the table
	//_r_queuedlock_acquireexclusive (&lock_apps);
	_r_obj_addhashtablepointer (apps_table, app_hash, ptr_app);
	//_r_queuedlock_releaseexclusive (&lock_apps);

	// insert item
	if (hwnd)
		_app_listview_addappitem (hwnd, ptr_app);

	// queue file information
	if (!_r_obj_isstringempty (ptr_app->real_path) && _app_isappvalidbinary (ptr_app->real_path))
		_app_getfileinformation (ptr_app->real_path, app_hash, ptr_app->type, _app_listview_getbytype (ptr_app->type));

	return (PITEM_APP)_r_obj_reference (ptr_app);
}

PITEM_RULE _app_addrule (
	_In_opt_ PR_STRING name,
	_In_opt_ PR_STRING rule_remote,
	_In_opt_ PR_STRING rule_local,
	_In_ FWP_DIRECTION direction,
	_In_ FWP_ACTION_TYPE action,
	_In_ UINT8 protocol,
	_In_ ADDRESS_FAMILY af
)
{
	PITEM_RULE ptr_rule;
	LPCWSTR proto;

	ptr_rule = (PITEM_RULE)_r_obj_allocate (sizeof (ITEM_RULE), &_app_dereferencerule);

	ptr_rule->apps = _r_obj_createhashtable (sizeof (SHORT), 0x02, NULL); // initialize apps hashtable
	ptr_rule->guids = _r_obj_createarray (sizeof (GUID), 0x02, NULL); // initialize filters array

	ptr_rule->type = DATA_RULE_USER;

	// set rule name
	ptr_rule->name = !_r_obj_isstringempty (name) ? _r_obj_createstring2 (&name->sr) : _r_obj_createstring (L"<noname>");

	if (_r_str_getlength2 (&ptr_rule->name->sr) > RULE_NAME_CCH_MAX)
		_r_str_setlength (&ptr_rule->name->sr, RULE_NAME_CCH_MAX * sizeof (WCHAR));

	ptr_rule->rule_hash = _r_str_gethash (&ptr_rule->name->sr, TRUE);

	// set rule destination
	if (rule_remote)
	{
		ptr_rule->rule_remote = _r_obj_createstring2 (&rule_remote->sr);

		if (_r_str_getlength2 (&ptr_rule->rule_remote->sr) > RULE_RULE_CCH_MAX)
			_r_str_setlength (&ptr_rule->rule_remote->sr, RULE_RULE_CCH_MAX * sizeof (WCHAR));
	}

	// set rule source
	if (rule_local)
	{
		ptr_rule->rule_local = _r_obj_createstring2 (&rule_local->sr);

		if (_r_str_getlength2 (&ptr_rule->rule_local->sr) > RULE_RULE_CCH_MAX)
			_r_str_setlength (&ptr_rule->rule_local->sr, RULE_RULE_CCH_MAX * sizeof (WCHAR));
	}

	// set configuration
	ptr_rule->direction = direction;
	ptr_rule->action = action;
	ptr_rule->protocol = protocol;
	ptr_rule->af = af;

	proto = _app_db_getprotoname (protocol, af, NULL);

	ptr_rule->protocol_str = proto ? _r_obj_createstring (proto) : NULL;

	return ptr_rule;
}

_Ret_maybenull_
PITEM_RULE_CONFIG _app_addruleconfigtable (
	_In_ PR_HASHTABLE hashtable,
	_In_ ULONG rule_hash,
	_In_ PR_STRING rule_name,
	_In_ BOOLEAN is_enabled
)
{
	ITEM_RULE_CONFIG entry = {0};

	entry.name = _r_obj_reference (rule_name);
	entry.is_enabled = is_enabled;

	return (PITEM_RULE_CONFIG)_r_obj_addhashtableitem (hashtable, rule_hash, &entry);
}

_Ret_maybenull_
PITEM_APP _app_getappitem (
	_In_ ULONG app_hash
)
{
	PITEM_APP ptr_app;

	if (!app_hash)
		return NULL;

	_r_queuedlock_acquireshared (&lock_apps);
	ptr_app = (PITEM_APP)_r_obj_findhashtablepointer (apps_table, app_hash);
	_r_queuedlock_releaseshared (&lock_apps);

	return ptr_app;
}

_Ret_maybenull_
PITEM_RULE _app_getrulebyid (
	_In_ ULONG_PTR index
)
{
	PITEM_RULE ptr_rule;

	_r_queuedlock_acquireshared (&lock_rules);
	ptr_rule = (PITEM_RULE)_r_obj_getlistitem (rules_list, index);
	_r_queuedlock_releaseshared (&lock_rules);

	return (PITEM_RULE)_r_obj_referencesafe (ptr_rule);
}

_Ret_maybenull_
PITEM_RULE _app_getrulebyhash (
	_In_ ULONG rule_hash
)
{
	PITEM_RULE ptr_rule;

	if (!rule_hash)
		return NULL;

	_r_queuedlock_acquireshared (&lock_rules);

	for (ULONG_PTR i = 0; i < _r_obj_getlistsize (rules_list); i++)
	{
		ptr_rule = (PITEM_RULE)_r_obj_getlistitem (rules_list, i);

		if (ptr_rule && ptr_rule->is_readonly && ptr_rule->rule_hash == rule_hash)
		{
			_r_queuedlock_releaseshared (&lock_rules);

			return (PITEM_RULE)_r_obj_reference (ptr_rule);
		}
	}

	_r_queuedlock_releaseshared (&lock_rules);

	return NULL;
}

_Ret_maybenull_
PITEM_RULE_CONFIG _app_getruleconfigitem (
	_In_ ULONG rule_hash
)
{
	PITEM_RULE_CONFIG ptr_rule_config;

	_r_queuedlock_acquireshared (&lock_rules_config);
	ptr_rule_config = (PITEM_RULE_CONFIG)_r_obj_findhashtable (rules_config, rule_hash);
	_r_queuedlock_releaseshared (&lock_rules_config);

	return ptr_rule_config;
}

_Ret_maybenull_
PITEM_LOG _app_getlogitem (
	_In_ ULONG log_hash
)
{
	PITEM_LOG ptr_log;

	_r_queuedlock_acquireshared (&lock_loglist);
	ptr_log = (PITEM_LOG)_r_obj_findhashtablepointer (log_table, log_hash);
	_r_queuedlock_releaseshared (&lock_loglist);

	return ptr_log;
}

_Success_ (return != 0)
ULONG _app_getlogapp (
	_In_ ULONG index
)
{
	PITEM_LOG ptr_log;
	ULONG app_hash = 0;

	ptr_log = _app_getlogitem (index);

	if (ptr_log)
	{
		app_hash = ptr_log->app_hash;

		_r_obj_dereference (ptr_log);
	}

	return app_hash;
}

_Success_ (return != 0)
COLORREF _app_getappcolor (
	_In_ INT listview_id,
	_In_ ULONG app_hash,
	_In_ BOOLEAN is_systemapp,
	_In_ BOOLEAN is_validconnection
)
{
	PITEM_APP ptr_app;
	ULONG color_hash = 0;
	BOOLEAN is_networklist, is_profilelist;

	ptr_app = _app_getappitem (app_hash);

	is_profilelist = (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_RULES_CUSTOM);
	is_networklist = (listview_id == IDC_NETWORK || listview_id == IDC_LOG);

	if (ptr_app && !is_networklist)
	{
		if (_r_config_getboolean (L"IsHighlightInvalid", TRUE, L"colors") && !_app_isappexists (ptr_app))
		{
			color_hash = config.color_invalid;

			goto CleanupExit;
		}
	}

	if (_r_config_getboolean (L"IsHighlightConnection", TRUE, L"colors") && is_validconnection)
	{
		color_hash = config.color_network;

		goto CleanupExit;
	}

	if (_r_config_getboolean (L"IsHighlightSigned", TRUE, L"colors") && _app_isappsigned (app_hash))
	{
		color_hash = config.color_signed;

		goto CleanupExit;
	}

	if (ptr_app)
	{
		if (!is_profilelist && (_r_config_getboolean (L"IsHighlightSpecial", TRUE, L"colors") && _app_isapphaverule (app_hash, FALSE)))
		{
			color_hash = config.color_special;

			goto CleanupExit;
		}

		if (_r_config_getboolean (L"IsHighlightPico", TRUE, L"colors") && ptr_app->type == DATA_APP_PICO)
		{
			color_hash = config.color_pico;

			goto CleanupExit;
		}

		if (_r_config_getboolean (L"IsHighlightUndelete", TRUE, L"colors") && ptr_app->is_undeletable)
		{
			color_hash = config.color_nonremovable;

			goto CleanupExit;
		}
	}

	if (_r_config_getboolean (L"IsHighlightSystem", TRUE, L"colors") && is_systemapp)
	{
		color_hash = config.color_system;

		goto CleanupExit;
	}

CleanupExit:

	if (ptr_app)
		_r_obj_dereference (ptr_app);

	return color_hash ? _app_getcolorvalue (color_hash) : 0;
}

VOID _app_deleteappitem (
	_In_ HWND hwnd,
	_In_ ENUM_TYPE_DATA type,
	_In_ ULONG_PTR id_code
)
{
	INT item_id, listview_id;

	listview_id = _app_listview_getbytype (type);

	if (!listview_id)
		return;

	item_id = _app_listview_finditem (hwnd, listview_id, id_code);

	if (item_id != INT_ERROR)
		_r_listview_deleteitem (hwnd, listview_id, item_id);
}

VOID _app_freeapplication (
	_In_opt_ HWND hwnd,
	_In_ ULONG app_hash
)
{
	PITEM_RULE ptr_rule;

	if (hwnd)
	{
		_r_queuedlock_acquireshared (&lock_rules);

		for (ULONG_PTR i = 0; i < _r_obj_getlistsize (rules_list); i++)
		{
			ptr_rule = (PITEM_RULE)_r_obj_getlistitem (rules_list, i);

			if (!ptr_rule || ptr_rule->type != DATA_RULE_USER)
				continue;

			_app_ruleremoveapp (hwnd, i, ptr_rule, app_hash);
		}

		_r_queuedlock_releaseshared (&lock_rules);
	}

	_r_obj_removehashtableitem (apps_table, app_hash);
}

VOID _app_getcount (
	_Out_ PITEM_STATUS status
)
{
	PITEM_APP ptr_app = NULL;
	PITEM_RULE ptr_rule;
	ULONG_PTR enum_key = 0;
	BOOLEAN is_used;

	RtlZeroMemory (status, sizeof (ITEM_STATUS));

	_r_queuedlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtablepointer (apps_table, (PVOID_PTR)&ptr_app, NULL, &enum_key))
	{
		is_used = _app_isappused (ptr_app);

		if (_app_istimerset (ptr_app))
			status->apps_timer_count += 1;

		if (!ptr_app->is_undeletable && (!_app_isappexists (ptr_app) || !is_used) && !(ptr_app->type == DATA_APP_SERVICE || ptr_app->type == DATA_APP_UWP))
			status->apps_unused_count += 1;

		if (is_used)
			status->apps_count += 1;
	}

	_r_queuedlock_releaseshared (&lock_apps);

	_r_queuedlock_acquireshared (&lock_rules);

	for (ULONG_PTR i = 0; i < _r_obj_getlistsize (rules_list); i++)
	{
		ptr_rule = (PITEM_RULE)_r_obj_getlistitem (rules_list, i);

		if (!ptr_rule)
			continue;

		if (ptr_rule->type == DATA_RULE_USER)
		{
			if (ptr_rule->is_enabled && !_r_obj_isempty (ptr_rule->apps))
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

_Success_ (return != 0)
COLORREF _app_getrulecolor (
	_In_ INT listview_id,
	_In_ ULONG_PTR rule_idx
)
{
	PITEM_RULE ptr_rule;
	ULONG color_hash = 0;

	ptr_rule = _app_getrulebyid (rule_idx);

	if (!ptr_rule)
		return 0;

	if (_r_config_getboolean (L"IsHighlightInvalid", TRUE, L"colors") && ptr_rule->is_enabled && ptr_rule->is_haveerrors)
	{
		color_hash = config.color_invalid;
	}
	else if (_r_config_getboolean (L"IsHighlightSpecial", TRUE, L"colors") && (ptr_rule->is_fordriver || ptr_rule->is_forservice || !_r_obj_isempty (ptr_rule->apps)))
	{
		color_hash = config.color_special;
	}

	_r_obj_dereference (ptr_rule);

	return color_hash ? _app_getcolorvalue (color_hash) : 0;
}

VOID _app_setappiteminfo (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ INT item_id,
	_In_ PITEM_APP ptr_app
)
{
	_r_listview_setitem (hwnd, listview_id, item_id, 0, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK, I_GROUPIDCALLBACK, I_DEFAULT);

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
	ULONG_PTR enum_key = 0;
	ULONG hash_code;

	_r_listview_setitem (hwnd, listview_id, item_id, 0, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK, I_GROUPIDCALLBACK, I_DEFAULT);

	_r_listview_setitemcheck (hwnd, listview_id, item_id, !!ptr_rule->is_enabled);

	if (!include_apps)
		return;

	while (_r_obj_enumhashtable (ptr_rule->apps, NULL, &hash_code, &enum_key))
	{
		if (hash_code != 0)
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

	ptr_rule->is_enabled = is_enable;

	if (!ptr_rule->is_readonly)
		return;

	ptr_config = _app_getruleconfigitem (ptr_rule->rule_hash);

	if (ptr_config)
	{
		ptr_config->is_enabled = is_enable;
	}
	else
	{
		if (is_createconfig)
		{
			_r_queuedlock_acquireexclusive (&lock_rules_config);
			ptr_config = _app_addruleconfigtable (rules_config, ptr_rule->rule_hash, ptr_rule->name, is_enable);
			_r_queuedlock_releaseexclusive (&lock_rules_config);

			if (ptr_config)
				ptr_config->is_enabled = is_enable;
		}
	}
}

VOID _app_ruleremoveapp (
	_In_opt_ HWND hwnd,
	_In_ ULONG_PTR item_id,
	_In_ PITEM_RULE ptr_rule,
	_In_ ULONG app_hash
)
{
	if (!ptr_rule->apps || !_r_obj_removehashtableitem (ptr_rule->apps, app_hash))
		return;

	if (ptr_rule->is_enabled && _r_obj_isempty2 (ptr_rule->apps))
	{
		ptr_rule->is_enabled = FALSE;
		ptr_rule->is_haveerrors = FALSE;

		if (hwnd && item_id != SIZE_MAX)
			_app_listview_updateitemby_param (hwnd, item_id, FALSE);
	}
}

BOOLEAN _app_ruleblocklistsetchange (
	_Inout_ PITEM_RULE ptr_rule,
	_In_ LONG new_state
)
{
	BOOLEAN is_block;

	if (new_state == INT_ERROR)
		return FALSE; // don't change

	if (new_state == 0 && !ptr_rule->is_enabled)
		return FALSE; // not changed

	is_block = (ptr_rule->action == FWP_ACTION_BLOCK);

	if (new_state == 1 && ptr_rule->is_enabled && !is_block)
		return FALSE; // not changed

	if (new_state == 2 && ptr_rule->is_enabled && is_block)
		return FALSE; // not changed

	ptr_rule->action = (new_state != 1) ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT;

	ptr_rule->is_enabled_default = ptr_rule->is_enabled; // set default value for rule
	ptr_rule->is_enabled = (new_state != 0);

	return TRUE;
}

BOOLEAN _app_ruleblocklistsetstate (
	_Inout_ PITEM_RULE ptr_rule,
	_In_ LONG spy_state,
	_In_ LONG update_state,
	_In_ LONG extra_state
)
{
	if (ptr_rule->type != DATA_RULE_BLOCKLIST)
		return FALSE;

	if (_r_str_isstartswith2 (&ptr_rule->name->sr, L"spy_", TRUE))
	{
		return _app_ruleblocklistsetchange (ptr_rule, spy_state);
	}
	else if (_r_str_isstartswith2 (&ptr_rule->name->sr, L"update_", TRUE))
	{
		return _app_ruleblocklistsetchange (ptr_rule, update_state);
	}
	else if (_r_str_isstartswith2 (&ptr_rule->name->sr, L"extra_", TRUE))
	{
		return _app_ruleblocklistsetchange (ptr_rule, extra_state);
	}

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
	PITEM_RULE ptr_rule;
	PR_LIST rules;
	ULONG_PTR changes_count = 0;

	rules = _r_obj_createlist (0x10, &_r_obj_dereference);

	_r_queuedlock_acquireshared (&lock_rules);

	for (ULONG_PTR i = 0; i < _r_obj_getlistsize (rules_list); i++)
	{
		ptr_rule = (PITEM_RULE)_r_obj_getlistitem (rules_list, i);

		if (!ptr_rule || ptr_rule->type != DATA_RULE_BLOCKLIST)
			continue;

		if (!_app_ruleblocklistsetstate (ptr_rule, spy_state, update_state, extra_state))
			continue;

		_app_ruleenable (ptr_rule, !!ptr_rule->is_enabled, FALSE);

		changes_count += 1;

		if (hwnd)
			_app_listview_updateitemby_param (hwnd, i, FALSE);

		if (is_instantapply)
			_r_obj_addlistitem (rules, _r_obj_reference (ptr_rule), NULL); // dereference later!
	}

	_r_queuedlock_releaseshared (&lock_rules);

	if (changes_count)
	{
		if (hwnd)
			_app_listview_updateby_id (hwnd, DATA_RULE_BLOCKLIST, PR_UPDATE_TYPE | PR_UPDATE_NORESIZE);

		if (is_instantapply)
		{
			if (!_r_obj_isempty2 (rules) && _wfp_isfiltersinstalled ())
				_wfp_createrulefilters (_wfp_getenginehandle (), rules, DBG_ARG, FALSE);
		}

		_app_profile_save (hwnd); // required!
	}

	_r_obj_dereference (rules);
}

_Ret_maybenull_
PR_STRING _app_appexpandrules (
	_In_ ULONG app_hash,
	_In_ LPCWSTR delimeter
)
{
	R_STRINGREF delimeter_sr;
	PITEM_RULE ptr_rule;
	R_STRINGBUILDER sb;
	PR_STRING string;

	_r_obj_initializestringbuilder (&sb, 0);

	_r_obj_initializestringref (&delimeter_sr, delimeter);

	_r_queuedlock_acquireshared (&lock_rules);

	for (ULONG_PTR i = 0; i < _r_obj_getlistsize (rules_list); i++)
	{
		ptr_rule = (PITEM_RULE)_r_obj_getlistitem (rules_list, i);

		if (ptr_rule && ptr_rule->is_enabled && ptr_rule->type == DATA_RULE_USER && _r_obj_findhashtable (ptr_rule->apps, app_hash))
		{
			_r_obj_appendstringbuilder2 (&sb, &ptr_rule->name->sr);

			if (ptr_rule->is_readonly)
				_r_obj_appendstringbuilder (&sb, SZ_RULE_INTERNAL_MENU);

			_r_obj_appendstringbuilder2 (&sb, &delimeter_sr);
		}
	}

	_r_queuedlock_releaseshared (&lock_rules);

	string = _r_obj_finalstringbuilder (&sb);

	_r_str_trimstring (&string->sr, &delimeter_sr, 0);

	if (!_r_obj_isstringempty2 (string))
		return string;

	_r_obj_deletestringbuilder (&sb);

	return NULL;
}

_Ret_maybenull_
PR_STRING _app_rulesexpandapps (
	_In_ PITEM_RULE ptr_rule,
	_In_ BOOLEAN is_fordisplay,
	_In_ LPCWSTR delimeter
)
{
	R_STRINGREF delimeter_sr;
	R_STRINGBUILDER sb;
	PITEM_APP ptr_app;
	PR_STRING string;
	ULONG_PTR enum_key = 0;
	ULONG hash_code;

	_r_obj_initializestringbuilder (&sb, 0);

	_r_obj_initializestringref (&delimeter_sr, delimeter);

	if (is_fordisplay)
	{
		if (ptr_rule->is_fordriver)
		{
			string = _r_obj_concatstrings (2, PROC_SYSTEM_NAME, delimeter);

			_r_obj_appendstringbuilder2 (&sb, &string->sr);

			_r_obj_dereference (string);
		}

		if (ptr_rule->is_forservice)
		{
			string = _r_obj_concatstrings (2, _r_obj_getstring (config.svchost_path), delimeter);

			_r_obj_appendstringbuilder2 (&sb, &string->sr);

			_r_obj_dereference (string);
		}
	}

	while (_r_obj_enumhashtable (ptr_rule->apps, NULL, &hash_code, &enum_key))
	{
		ptr_app = _app_getappitem (hash_code);

		if (!ptr_app)
			continue;

		string = NULL;

		if (is_fordisplay && ptr_app->type == DATA_APP_UWP && ptr_app->display_name)
			string = _r_path_compact (&ptr_app->display_name->sr, 0x40);

		if (!string && is_fordisplay && ptr_app->original_path)
			string = _r_path_compact (&ptr_app->original_path->sr, 0x40);

		if (!string)
			string = _r_obj_referencesafe (ptr_app->original_path);

		if (string)
		{
			_r_obj_appendstringbuilder2 (&sb, &string->sr);
			_r_obj_appendstringbuilder2 (&sb, &delimeter_sr);

			_r_obj_dereference (string);
		}

		_r_obj_dereference (ptr_app);
	}

	string = _r_obj_finalstringbuilder (&sb);

	_r_str_trimstring (&string->sr, &delimeter_sr, PR_TRIM_END_ONLY);

	if (!_r_obj_isstringempty2 (string))
		return string;

	_r_obj_deletestringbuilder (&sb);

	return NULL;
}

_Ret_maybenull_
PR_STRING _app_rulesexpandrules (
	_In_opt_ PR_STRING rule
)
{
	R_STRINGREF delimeter_sr, first_part, remaining_part;
	R_STRINGBUILDER sb;
	PR_STRING string;

	if (_r_obj_isstringempty (rule))
		return NULL;

	_r_obj_initializestringbuilder (&sb, 0);

	_r_obj_initializestringref (&delimeter_sr, SZ_CRLF SZ_TAB);
	_r_obj_initializestringref2 (&remaining_part, &rule->sr);

	while (remaining_part.length != 0)
	{
		_r_str_splitatchar (&remaining_part, DIVIDER_RULE[0], &first_part, &remaining_part);

		_r_obj_appendstringbuilder2 (&sb, &first_part);
		_r_obj_appendstringbuilder2 (&sb, &delimeter_sr);
	}

	string = _r_obj_finalstringbuilder (&sb);

	_r_str_trimstring (&string->sr, &delimeter_sr, 0);

	if (!_r_obj_isstringempty2 (string))
		return string;

	_r_obj_deletestringbuilder (&sb);

	return NULL;
}

BOOLEAN _app_isappfromsystem (
	_In_opt_ PR_STRING path,
	_In_ ULONG app_hash
)
{
	if (_app_issystemhash (app_hash))
		return TRUE;

	if (path && _r_str_isstartswith (&path->sr, &config.windows_dir, TRUE))
		return TRUE;

	return FALSE;
}

BOOLEAN _app_isapphavedrive (
	_In_ INT letter
)
{
	PITEM_APP ptr_app = NULL;
	ULONG_PTR enum_key = 0;
	INT drive_id;
	BOOLEAN is_havedrive = FALSE;

	_r_queuedlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtablepointer (apps_table, (PVOID_PTR)&ptr_app, NULL, &enum_key))
	{
		if (_r_obj_isstringempty (ptr_app->original_path))
			continue;

		drive_id = (ptr_app->type == DATA_APP_REGULAR) ? _r_path_getdrivenumber (&ptr_app->original_path->sr) : INT_ERROR;

		if (ptr_app->type == DATA_APP_DEVICE || (drive_id != INT_ERROR && drive_id == letter))
		{
			if (ptr_app->is_enabled || _app_isapphaverule (ptr_app->app_hash, FALSE))
			{
				is_havedrive = TRUE;
				break;
			}
		}
	}

	_r_queuedlock_releaseshared (&lock_apps);

	return is_havedrive;
}

BOOLEAN _app_isapphaverule (
	_In_ ULONG app_hash,
	_In_ BOOLEAN is_countdisabled
)
{
	PITEM_RULE ptr_rule;
	BOOLEAN is_haverule = FALSE;

	_r_queuedlock_acquireshared (&lock_rules);

	for (ULONG_PTR i = 0; i < _r_obj_getlistsize (rules_list); i++)
	{
		ptr_rule = (PITEM_RULE)_r_obj_getlistitem (rules_list, i);

		if (ptr_rule && ptr_rule->type == DATA_RULE_USER && (is_countdisabled || ptr_rule->is_enabled))
		{
			if (_r_obj_findhashtable (ptr_rule->apps, app_hash))
			{
				is_haverule = TRUE;
				break;
			}
		}
	}

	_r_queuedlock_releaseshared (&lock_rules);

	return is_haverule;
}

BOOLEAN _app_isappexists (
	_In_ PITEM_APP ptr_app
)
{
	if (ptr_app->is_undeletable)
		return TRUE;

	if (ptr_app->is_enabled && ptr_app->is_haveerrors)
		return FALSE;

	switch (ptr_app->type)
	{
		case DATA_APP_REGULAR:
		{
			if (ptr_app->real_path && !_r_fs_isexists (&ptr_app->real_path->sr))
				return FALSE;

			return TRUE;
		}

		case DATA_APP_DEVICE:
		case DATA_APP_NETWORK:
		case DATA_APP_SERVICE:
		case DATA_APP_UWP:
		case DATA_APP_PICO:
		{
			return TRUE; // services and UWP apps are already undeletable!
		}

		default:
		{
			return FALSE; // unknown app!
		}
	}
}

BOOLEAN _app_isappfound (
	_In_ ULONG app_hash
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

	if (!_app_isappused (ptr_app))
		return TRUE;

	return FALSE;
}

BOOLEAN _app_isappused (
	_In_ PITEM_APP ptr_app
)
{
	return (ptr_app->is_enabled || ptr_app->is_silent || ptr_app->is_undeletable || _app_isapphaverule (ptr_app->app_hash, TRUE)) ? TRUE : FALSE;
}

BOOLEAN _app_issystemhash (
	_In_ ULONG app_hash
)
{
	return (app_hash == config.ntoskrnl_hash || app_hash == config.svchost_hash);
}

BOOLEAN _app_isrulesupportedbyos (
	_In_ PCR_STRINGREF os_version
)
{
	static PR_STRING version_string = NULL;

	PR_STRING current_version, new_version;

	current_version = (PR_STRING)_InterlockedCompareExchangePointer ((volatile PVOID_PTR)&version_string, NULL, NULL);

	if (!current_version)
	{
		new_version = _r_format_string (L"%d.%d", NtCurrentPeb ()->OSMajorVersion, NtCurrentPeb ()->OSMinorVersion);

		current_version = (PR_STRING)_InterlockedCompareExchangePointer ((volatile PVOID_PTR)&version_string, new_version, NULL);

		if (!current_version)
		{
			current_version = new_version;
		}
		else
		{
			_r_obj_dereference (new_version);
		}
	}

	return (_r_str_versioncompare (&current_version->sr, os_version) != INT_ERROR);
}

VOID _app_profile_initialize ()
{
	R_STRINGREF profile_internal_sr = PR_STRINGREF_INIT (XML_PROFILE_INTERNAL), profile_bak_sr = PR_STRINGREF_INIT (XML_PROFILE_FILE L".bak");
	R_STRINGREF profile_sr = PR_STRINGREF_INIT (XML_PROFILE_FILE), separator_sr = PR_STRINGREF_INIT (L"\\");
	PR_STRING path;

	path = _r_app_getprofiledirectory ();

	_r_obj_movereference ((PVOID_PTR)&profile_info.profile_path_internal, _r_obj_concatstringrefs (3, path, &separator_sr, &profile_internal_sr));
	_r_obj_movereference ((PVOID_PTR)&profile_info.profile_path_backup, _r_obj_concatstringrefs (3, path, &separator_sr, &profile_bak_sr));
	_r_obj_movereference ((PVOID_PTR)&profile_info.profile_path, _r_obj_concatstringrefs (3, path, &separator_sr, &profile_sr));
}

NTSTATUS _app_profile_load_fromresource (
	_Out_ PDB_INFORMATION db_info,
	_In_ LPCWSTR resource_name
)
{
	R_STORAGE storage;
	NTSTATUS status;

	status = _app_db_initialize (db_info, TRUE);

	if (NT_SUCCESS (status))
	{
		status = _r_res_loadresource (&storage, _r_sys_getimagebase (), RT_RCDATA, resource_name, 0);

		if (NT_SUCCESS (status))
			status = _app_db_openfrombuffer (db_info, &storage, XML_TYPE_PROFILE_INTERNAL);
	}

	return status;
}

VOID _app_profile_load_fallback ()
{
	PITEM_APP ptr_app;

	if (!_app_isappfound (config.my_hash))
	{
		ptr_app = _app_addapplication (NULL, DATA_UNKNOWN, config.my_path, NULL, NULL);

		if (ptr_app)
		{
			_app_setappinfo (ptr_app, INFO_IS_ENABLED, LongToPtr (TRUE));

			_r_obj_dereference (ptr_app);
		}
	}

	// disable deletion for this shit ;)
	if (!_r_config_getboolean (L"IsInternalRulesDisabled", FALSE, NULL))
	{
		if (!_app_isappfound (config.ntoskrnl_hash) && !_r_obj_isstringempty (config.system_path))
		{
			ptr_app = _app_addapplication (NULL, DATA_UNKNOWN, config.system_path, NULL, NULL);

			if (ptr_app)
				_r_obj_dereference (ptr_app);
		}

		if (!_app_isappfound (config.svchost_hash) && !_r_obj_isstringempty (config.svchost_path))
		{
			ptr_app = _app_addapplication (NULL, DATA_UNKNOWN, config.svchost_path, NULL, NULL);

			if (ptr_app)
				_r_obj_dereference (ptr_app);
		}
	}

	_app_setappinfobyhash (config.ntoskrnl_hash, INFO_IS_UNDELETABLE, LongToPtr (TRUE));
	_app_setappinfobyhash (config.svchost_hash, INFO_IS_UNDELETABLE, LongToPtr (TRUE));
	_app_setappinfobyhash (config.my_hash, INFO_IS_UNDELETABLE, LongToPtr (TRUE));
}

VOID _app_profile_load_internal (
	_In_opt_ HWND hwnd,
	_In_ PR_STRING path,
	_In_ LPCWSTR resource_name,
	_Out_ PLONG64 out_timestamp
)
{
	DB_INFORMATION *db_info, db_info_buffer, db_info_file = {0};
	LONG64 timestamp = 0;
	BOOLEAN is_loadfromresource;
	NTSTATUS status, status_file, status_res;

	status_file = _app_db_initialize (&db_info_file, TRUE);

	if (_r_fs_isexists (&path->sr))
	{
		status_file = SUCCEEDED (status_file) ? _app_db_openfromfile (&db_info_file, path, XML_TYPE_PROFILE_INTERNAL) : STATUS_INVALID_PARAMETER;
	}
	else
	{
		status_file = STATUS_OBJECT_PATH_NOT_FOUND;
	}

	status_res = _app_profile_load_fromresource (&db_info_buffer, resource_name);

	// NOTE: prefer new profile version for 3.4+
	is_loadfromresource = (!NT_SUCCESS (status_file)) ? TRUE : (db_info_file.version < db_info_buffer.version) || (db_info_file.timestamp < db_info_buffer.timestamp);

	db_info = is_loadfromresource ? &db_info_buffer : &db_info_file;

	status = is_loadfromresource ? status_res : status_file;

	if (NT_SUCCESS (status))
	{
		if (_app_db_parse (db_info, XML_TYPE_PROFILE_INTERNAL))
			timestamp = db_info->timestamp;
	}
	else
	{
		if (status != STATUS_OBJECT_NAME_NOT_FOUND && status != STATUS_OBJECT_PATH_NOT_FOUND)
		{
			if (hwnd)
			{
				_r_show_errormessage (hwnd, L"Could not load internal profile!", NULL, status, ET_NATIVE);
			}
			else
			{
				_r_log (LOG_LEVEL_ERROR, NULL, L"_app_profile_load_internal", NULL, status);
			}
		}
	}

	*out_timestamp = timestamp;

	_app_db_destroy (&db_info_buffer);
	_app_db_destroy (&db_info_file);
}

static VOID _app_profile_refresh (
	_In_opt_ HWND hwnd
)
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

	// clear auto rules
	_app_autorules_clear ();

	if (!hwnd)
		hwnd = _r_app_gethwnd ();

	// generate services list
	_app_package_getserviceslist (hwnd);

	// generate uwp packages list (win8+)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
		_app_package_getpackageslist (hwnd);
}

NTSTATUS _app_profile_load (
	_In_opt_ HWND hwnd,
	_In_opt_ PR_STRING path_custom
)
{
	DB_INFORMATION db_info;
	BOOLEAN is_update;
	NTSTATUS status;

	status = _app_db_initialize (&db_info, TRUE);

	if (FAILED (status))
	{
		if (hwnd)
		{
			_r_show_errormessage (hwnd, L"Could not initialize XML library!", NULL, status, ET_WINDOWS);
		}
		else
		{
			_r_log (LOG_LEVEL_ERROR, NULL, L"_app_db_initialize", NULL, status);
		}

		goto CleanupExit;
	}

	if (path_custom && _r_fs_isexists (&path_custom->sr))
	{
		status = _app_db_openfromfile (&db_info, path_custom, XML_TYPE_PROFILE);
	}
	else
	{
		_r_queuedlock_acquireshared (&lock_profile);

		status = _app_db_openfromfile (&db_info, profile_info.profile_path, XML_TYPE_PROFILE);

		if (!NT_SUCCESS (status))
			status = _app_db_openfromfile (&db_info, profile_info.profile_path_backup, XML_TYPE_PROFILE);

		_r_queuedlock_releaseshared (&lock_profile);
	}

CleanupExit:

	is_update = (NT_SUCCESS (status) || !path_custom);

	if (is_update)
	{
		if (hwnd)
			_app_listview_clearitems (hwnd);

		_app_profile_refresh (hwnd);
	}

	if (NT_SUCCESS (status))
	{
		_app_db_parse (&db_info, XML_TYPE_PROFILE);
	}
	else
	{
		if (status != STATUS_OBJECT_NAME_NOT_FOUND && status != STATUS_OBJECT_PATH_NOT_FOUND)
		{
			if (hwnd)
			{
				_r_show_errormessage (hwnd, L"Could not load profile!", NULL, status, ET_NATIVE);
			}
			else
			{
				_r_log (LOG_LEVEL_ERROR, NULL, TEXT (__FUNCTION__), NULL, status);
			}
		}
	}

	if (is_update)
	{
		// load internal rules!
		if (!_r_config_getboolean (L"IsInternalRulesDisabled", FALSE, NULL))
			_app_profile_load_internal (hwnd, profile_info.profile_path_internal, MAKEINTRESOURCE (IDR_PROFILE_INTERNAL), &profile_info.profile_internal_timestamp);

		_app_profile_load_fallback ();

		if (hwnd)
			_app_listview_additems (hwnd);
	}

	_app_db_destroy (&db_info);

	return status;
}

NTSTATUS _app_profile_save (
	_In_opt_ HWND hwnd
)
{
	DB_INFORMATION db_info;
	LONG64 timestamp;
	BOOLEAN is_backuprequired = FALSE;
	NTSTATUS status;

	status = _app_db_initialize (&db_info, FALSE);

	if (FAILED (status))
	{
		if (hwnd)
		{
			_r_show_errormessage (hwnd, L"Could not initialize XML library!", NULL, status, ET_WINDOWS);
		}
		else
		{
			_r_log (LOG_LEVEL_ERROR, NULL, L"_app_db_initialize", NULL, status);
		}

		return status;
	}

	timestamp = _r_unixtime_now ();

	if (_r_config_getboolean (L"IsBackupProfile", TRUE, NULL))
	{
		if (!_r_fs_isexists (&profile_info.profile_path_backup->sr))
			is_backuprequired = TRUE;

		if (!is_backuprequired && (timestamp - _r_config_getlong64 (L"BackupTimestamp", 0, NULL) >= _r_config_getlong64 (L"BackupPeriod", BACKUP_HOURS_PERIOD, NULL)))
			is_backuprequired = TRUE;
	}

	_r_queuedlock_acquireexclusive (&lock_profile);
	status = _app_db_savetofile (&db_info, profile_info.profile_path, XML_VERSION_MAXIMUM, XML_TYPE_PROFILE, timestamp);
	_r_queuedlock_releaseexclusive (&lock_profile);

	if (!NT_SUCCESS (status))
	{
		if (hwnd)
		{
			_r_show_errormessage (hwnd, L"Could not save profile!", profile_info.profile_path->buffer, status, ET_NATIVE);
		}
		else
		{
			_r_log (LOG_LEVEL_ERROR, NULL, L"_app_db_savetofile", profile_info.profile_path->buffer, status);
		}
	}

	_app_db_destroy (&db_info);

	// make backup
	if (is_backuprequired)
	{
		_r_fs_copyfile (&profile_info.profile_path->sr, &profile_info.profile_path_backup->sr, FALSE);

		_r_config_setlong64 (L"BackupTimestamp", timestamp, NULL);
	}

	return status;
}

//
// Auto rules: one decision (allow or block) that applies to every
// application signed by a publisher, or living under a path pattern.
//


VOID NTAPI _app_dereferenceautorule (
	_In_ PVOID entry
)
{
	PITEM_AUTORULE ptr_item = (PITEM_AUTORULE)entry;

	if (ptr_item->publisher)
		_r_obj_dereference (ptr_item->publisher);

	if (ptr_item->path_pattern)
		_r_obj_dereference (ptr_item->path_pattern);

	if (ptr_item->path_expanded)
		_r_obj_dereference (ptr_item->path_expanded);
}

VOID _app_autorules_initialize ()
{
	if (!autorules_list)
		autorules_list = _r_obj_createlist (0x04, &_r_obj_dereference);
}

// forget the "already checked" mark on every app, so the next blocked
// packet re-evaluates it against the current set of auto rules
static VOID _app_autorules_resetchecked ()
{
	PITEM_APP ptr_app = NULL;
	ULONG_PTR enum_key = 0;

	_r_queuedlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtablepointer (apps_table, (PVOID_PTR)&ptr_app, NULL, &enum_key))
	{
		ptr_app->is_autorulechecked = FALSE;
	}

	_r_queuedlock_releaseshared (&lock_apps);
}

VOID _app_autorules_clear ()
{
	_r_queuedlock_acquireexclusive (&lock_autorules);
	_r_obj_clearlist (autorules_list);
	_r_queuedlock_releaseexclusive (&lock_autorules);
}

BOOLEAN _app_autorules_isempty ()
{
	BOOLEAN is_empty;

	_r_queuedlock_acquireshared (&lock_autorules);
	is_empty = (_r_obj_getlistsize (autorules_list) == 0);
	_r_queuedlock_releaseshared (&lock_autorules);

	return is_empty;
}

BOOLEAN _app_autorules_havepublisherrules ()
{
	PITEM_AUTORULE ptr_rule;
	BOOLEAN is_found = FALSE;

	_r_queuedlock_acquireshared (&lock_autorules);

	for (ULONG_PTR i = 0; i < _r_obj_getlistsize (autorules_list); i++)
	{
		ptr_rule = (PITEM_AUTORULE)_r_obj_getlistitem (autorules_list, i);

		if (ptr_rule && !_r_obj_isstringempty (ptr_rule->publisher))
		{
			is_found = TRUE;
			break;
		}
	}

	_r_queuedlock_releaseshared (&lock_autorules);

	return is_found;
}

_Ret_maybenull_
PITEM_AUTORULE _app_autorules_add (
	_In_opt_ PR_STRING publisher,
	_In_opt_ PR_STRING path_pattern,
	_In_ BOOLEAN is_allow
)
{
	PITEM_AUTORULE ptr_rule;
	PR_STRING string;
	NTSTATUS status;

	if (_r_obj_isstringempty (publisher) && _r_obj_isstringempty (path_pattern))
		return NULL;

	// a rule is either a publisher rule or a path rule, never both
	if (!_r_obj_isstringempty (publisher))
		path_pattern = NULL;

	// do not add duplicates, just update the action
	_r_queuedlock_acquireexclusive (&lock_autorules);

	for (ULONG_PTR i = 0; i < _r_obj_getlistsize (autorules_list); i++)
	{
		ptr_rule = (PITEM_AUTORULE)_r_obj_getlistitem (autorules_list, i);

		if (!ptr_rule)
			continue;

		if (publisher && ptr_rule->publisher && _r_str_isequal (&ptr_rule->publisher->sr, &publisher->sr, TRUE))
		{
			ptr_rule->is_allow = is_allow;
			_r_queuedlock_releaseexclusive (&lock_autorules);

			_app_autorules_resetchecked ();

			return ptr_rule;
		}

		if (path_pattern && ptr_rule->path_pattern && _r_str_isequal (&ptr_rule->path_pattern->sr, &path_pattern->sr, TRUE))
		{
			ptr_rule->is_allow = is_allow;
			_r_queuedlock_releaseexclusive (&lock_autorules);

			_app_autorules_resetchecked ();

			return ptr_rule;
		}
	}

	ptr_rule = (PITEM_AUTORULE)_r_obj_allocate (sizeof (ITEM_AUTORULE), &_app_dereferenceautorule);

	ptr_rule->is_allow = is_allow;

	if (publisher)
	{
		ptr_rule->publisher = _r_obj_createstring2 (&publisher->sr);
	}
	else
	{
		ptr_rule->path_pattern = _r_obj_createstring2 (&path_pattern->sr);

		// expand environment variables only for matching, keep the saved form portable
		if (_r_str_findchar (&path_pattern->sr, L'%', FALSE) != SIZE_MAX)
		{
			status = _r_str_environmentexpandstring (&string, NULL, &path_pattern->sr);

			if (NT_SUCCESS (status))
				ptr_rule->path_expanded = string;
		}

		if (!ptr_rule->path_expanded)
			ptr_rule->path_expanded = _r_obj_createstring2 (&path_pattern->sr);
	}

	_r_obj_addlistitem (autorules_list, ptr_rule, NULL);

	_r_queuedlock_releaseexclusive (&lock_autorules);

	_app_autorules_resetchecked ();

	return ptr_rule;
}

BOOLEAN _app_autorules_remove (
	_In_ ULONG_PTR index
)
{
	PITEM_AUTORULE ptr_rule;
	PR_LIST new_list;
	BOOLEAN is_removed = FALSE;

	new_list = _r_obj_createlist (0x04, &_r_obj_dereference);

	_r_queuedlock_acquireexclusive (&lock_autorules);

	for (ULONG_PTR i = 0; i < _r_obj_getlistsize (autorules_list); i++)
	{
		ptr_rule = (PITEM_AUTORULE)_r_obj_getlistitem (autorules_list, i);

		if (!ptr_rule)
			continue;

		if (i == index)
		{
			is_removed = TRUE;
			continue;
		}

		_r_obj_addlistitem (new_list, _r_obj_reference (ptr_rule), NULL);
	}

	if (is_removed)
	{
		_r_obj_movereference ((PVOID_PTR)&autorules_list, new_list);
	}
	else
	{
		_r_obj_dereference (new_list);
	}

	_r_queuedlock_releaseexclusive (&lock_autorules);

	if (is_removed)
		_app_autorules_resetchecked ();

	return is_removed;
}

_Ret_maybenull_
PR_STRING _app_autorules_getdescription (
	_In_ PITEM_AUTORULE ptr_rule
)
{
	LPCWSTR action;

	action = _r_locale_getstring (ptr_rule->is_allow ? IDS_ACTION_ALLOW : IDS_ACTION_BLOCK);

	if (!_r_obj_isstringempty (ptr_rule->publisher))
		return _r_format_string (L"%s: %s \"%s\"", action, _r_locale_getstring (IDS_AUTORULE_KIND_PUBLISHER), ptr_rule->publisher->buffer);

	if (!_r_obj_isstringempty (ptr_rule->path_pattern))
		return _r_format_string (L"%s: %s %s", action, _r_locale_getstring (IDS_AUTORULE_KIND_PATH), ptr_rule->path_pattern->buffer);

	return NULL;
}

static BOOLEAN _app_autorules_isversionchar (
	_In_ WCHAR chr
)
{
	return (chr >= L'0' && chr <= L'9') || chr == L'.';
}

static BOOLEAN _app_autorules_isalnum (
	_In_ WCHAR chr
)
{
	return (chr >= L'0' && chr <= L'9') || (chr >= L'a' && chr <= L'z') || (chr >= L'A' && chr <= L'Z');
}

// Turns the folder of an executable into a pattern that survives updates by
// replacing version numbers in folder names with a wildcard, e.g.
//   C:\Program Files (x86)\Google\GoogleUpdater\140.0.7339.0\updater.exe
//     -> C:\Program Files (x86)\Google\GoogleUpdater\*
//   C:\Program Files\WindowsApps\MSTeams_25290.205.4069.4894_x64__8wekyb3d8bbwe\ms-teams.exe
//     -> C:\Program Files\WindowsApps\MSTeams_*_x64__8wekyb3d8bbwe\*
// Returns NULL when the folder has no version number (the exact path is then
// already handled by the normal per-app decision) or lives under Windows.
_Ret_maybenull_
PR_STRING _app_autorules_makepathpattern (
	_In_ PR_STRING path
)
{
	WCHAR buffer[0x400];
	R_STRINGREF dir_sr;
	ULONG_PTR dir_length, out_length = 0, last_slash = 0, i, j, run_end, dots;
	BOOLEAN is_changed = FALSE, is_lastwildcard = FALSE;

	if (_r_obj_isstringempty (path))
		return NULL;

	i = _r_str_findlastchar (&path->sr, OBJ_NAME_PATH_SEPARATOR, FALSE);

	if (i == SIZE_MAX || i < 3)
		return NULL;

	dir_length = i;

	// never generalize system folders
	_r_obj_initializestringref_ex (&dir_sr, path->buffer, dir_length * sizeof (WCHAR));

	if (config.windows_dir.length && _r_str_isstartswith (&dir_sr, &config.windows_dir, TRUE))
		return NULL;

	if (dir_length + 3 >= RTL_NUMBER_OF (buffer))
		return NULL;

	i = 0;

	while (i < dir_length)
	{
		WCHAR chr = path->buffer[i];

		if (chr == OBJ_NAME_PATH_SEPARATOR)
			last_slash = out_length;

		// a version starts with a digit that does not continue a word
		if (chr >= L'0' && chr <= L'9' && (i == 0 || !_app_autorules_isalnum (path->buffer[i - 1])))
		{
			j = i;

			while (j < dir_length && _app_autorules_isversionchar (path->buffer[j]))
				j += 1;

			run_end = j;

			// do not eat a trailing dot
			while (run_end > i && path->buffer[run_end - 1] == L'.')
				run_end -= 1;

			dots = 0;

			for (ULONG_PTR k = i; k < run_end; k++)
			{
				if (path->buffer[k] == L'.')
					dots += 1;
			}

			// at least "1.2" and not glued to a following word
			if (dots >= 1 && (run_end >= dir_length || !_app_autorules_isalnum (path->buffer[run_end])))
			{
				buffer[out_length++] = L'*';
				is_changed = TRUE;
				i = run_end;

				continue;
			}

			// not a version, copy as is
			while (i < run_end)
				buffer[out_length++] = path->buffer[i++];

			continue;
		}

		buffer[out_length++] = chr;
		i += 1;
	}

	if (!is_changed)
		return NULL;

	// if the last folder became just "*", it already matches every file below
	is_lastwildcard = (out_length >= 2 && buffer[out_length - 1] == L'*' && last_slash == out_length - 2);

	if (!is_lastwildcard)
	{
		buffer[out_length++] = OBJ_NAME_PATH_SEPARATOR;
		buffer[out_length++] = L'*';
	}

	buffer[out_length] = UNICODE_NULL;

	return _r_obj_createstring_ex (buffer, out_length * sizeof (WCHAR));
}

_Ret_maybenull_
PR_STRING _app_autorules_getappsignature (
	_In_ PITEM_APP ptr_app,
	_In_ BOOLEAN is_allowsync
)
{
	PITEM_APP_INFO ptr_app_info;
	PR_STRING string = NULL;
	HANDLE hfile;
	NTSTATUS status;

	if (ptr_app->type != DATA_APP_REGULAR || _r_obj_isstringempty (ptr_app->real_path))
		return NULL;

	// cached by the file information queue
	if (_app_getappinfoparam2 (ptr_app->app_hash, 0, INFO_SIGNATURE_STRING, &string, sizeof (PR_STRING)))
		return string;

	if (!is_allowsync || !_app_isappvalidbinary (ptr_app->real_path))
		return NULL;

	string = NULL;

	status = _r_fs_openfile (&hfile, &ptr_app->real_path->sr, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE, 0, FALSE);

	if (!NT_SUCCESS (status))
		return NULL;

	// the cache entry is created by _app_addapplication for every valid binary
	ptr_app_info = _app_getappinfobyhash2 (ptr_app->app_hash);

	if (ptr_app_info)
	{
		// store into cache, so the next lookup is free
		_app_getfilesignatureinfo (ptr_app_info, hfile);

		if (!_r_obj_isstringempty (ptr_app_info->signature_info))
			string = _r_obj_reference (ptr_app_info->signature_info);

		_r_obj_dereference (ptr_app_info);
	}

	NtClose (hfile);

	return string;
}

static BOOLEAN _app_autorules_matchrule (
	_In_ PITEM_AUTORULE ptr_rule,
	_In_ PR_STRING path,
	_In_opt_ PR_STRING signature
)
{
	if (!_r_obj_isstringempty (ptr_rule->publisher))
		return (signature && _r_str_isequal (&signature->sr, &ptr_rule->publisher->sr, TRUE));

	if (!_r_obj_isstringempty (ptr_rule->path_expanded))
		return _r_str_match (path->buffer, ptr_rule->path_expanded->buffer, TRUE);

	return FALSE;
}

_Success_ (return)
BOOLEAN _app_autorules_match (
	_In_ PITEM_APP ptr_app,
	_In_opt_ PR_STRING signature,
	_Out_ PBOOLEAN is_allow
)
{
	PITEM_AUTORULE ptr_rule;
	BOOLEAN is_found = FALSE;

	*is_allow = FALSE;

	if (_r_obj_isstringempty (ptr_app->real_path))
		return FALSE;

	_r_queuedlock_acquireshared (&lock_autorules);

	// path rules are more specific, so they win over publisher rules
	for (UINT pass = 0; pass < 2 && !is_found; pass++)
	{
		for (ULONG_PTR i = 0; i < _r_obj_getlistsize (autorules_list); i++)
		{
			ptr_rule = (PITEM_AUTORULE)_r_obj_getlistitem (autorules_list, i);

			if (!ptr_rule)
				continue;

			if ((pass == 0) != _r_obj_isstringempty (ptr_rule->publisher))
				continue;

			if (_app_autorules_matchrule (ptr_rule, ptr_app->real_path, signature))
			{
				*is_allow = ptr_rule->is_allow;
				is_found = TRUE;

				break;
			}
		}
	}

	_r_queuedlock_releaseshared (&lock_autorules);

	return is_found;
}

// Applies the matching auto rule to an app nobody decided about yet.
// Returns TRUE when a decision was made (the caller should save the profile).
BOOLEAN _app_autorules_applytoapp (
	_In_opt_ HWND hwnd,
	_In_ PITEM_APP ptr_app,
	_In_ BOOLEAN is_allowsync
)
{
	PR_STRING signature = NULL;
	PR_LIST rules;
	HWND hmain;
	BOOLEAN is_allow;

	UNREFERENCED_PARAMETER (hwnd);

	if (_app_autorules_isempty ())
		return FALSE;

	if (ptr_app->type != DATA_APP_REGULAR)
		return FALSE;

	// only apps without a decision: never override what the user chose
	if (ptr_app->is_enabled || ptr_app->is_silent || ptr_app->is_undeletable || _app_istimerset (ptr_app))
		return FALSE;

	// expensive signature checks happen once per app until the rules change
	if (ptr_app->is_autorulechecked)
		is_allowsync = FALSE;

	if (_app_autorules_havepublisherrules ())
		signature = _app_autorules_getappsignature (ptr_app, is_allowsync);

	if (is_allowsync)
		ptr_app->is_autorulechecked = TRUE;

	if (!_app_autorules_match (ptr_app, signature, &is_allow))
	{
		if (signature)
			_r_obj_dereference (signature);

		return FALSE;
	}

	if (signature)
		_r_obj_dereference (signature);

	ptr_app->is_enabled = is_allow;
	ptr_app->is_silent = !is_allow;
	ptr_app->last_notify = _r_unixtime_now ();

	if (_wfp_isfiltersinstalled ())
	{
		rules = _r_obj_createlist (0x02, NULL);

		_r_obj_addlistitem (rules, ptr_app, NULL);

		_wfp_createappfilters (_wfp_getenginehandle (), rules, DBG_ARG, FALSE);

		_r_obj_dereference (rules);
	}

	hmain = _r_app_gethwnd ();

	if (hmain)
	{
		_app_listview_updateitemby_param (hmain, ptr_app->app_hash, TRUE);
		_app_listview_updateby_id (hmain, ptr_app->type, PR_UPDATE_TYPE);
	}

	return TRUE;
}

// How many other apps are waiting in the notification queue and would be
// covered by this rule (uses cached signatures only, it runs on the UI thread).
ULONG _app_autorules_countpending (
	_In_ PITEM_AUTORULE ptr_rule,
	_In_ ULONG exclude_hash
)
{
	PITEM_APP ptr_app = NULL;
	PR_STRING signature;
	ULONG_PTR enum_key = 0;
	ULONG count = 0;

	_r_queuedlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtablepointer (apps_table, (PVOID_PTR)&ptr_app, NULL, &enum_key))
	{
		if (!ptr_app->notification || ptr_app->app_hash == exclude_hash)
			continue;

		if (ptr_app->type != DATA_APP_REGULAR || _r_obj_isstringempty (ptr_app->real_path))
			continue;

		signature = NULL;

		if (!_r_obj_isstringempty (ptr_rule->publisher))
			_app_getappinfoparam2 (ptr_app->app_hash, 0, INFO_SIGNATURE_STRING, &signature, sizeof (PR_STRING));

		if (_app_autorules_matchrule (ptr_rule, ptr_app->real_path, signature))
			count += 1;

		if (signature)
			_r_obj_dereference (signature);
	}

	_r_queuedlock_releaseshared (&lock_apps);

	return count;
}

static VOID _app_autorules_addmenuitem (
	_In_ HMENU hmenu,
	_In_ UINT item_id,
	_In_ PR_STRING text,
	_In_ ULONG pending
)
{
	PR_STRING string;

	if (pending)
	{
		string = _r_format_string (_r_locale_getstring (IDS_AUTORULE_PENDING), text->buffer, pending);

		_r_menu_additem (hmenu, item_id, string->buffer);

		_r_obj_dereference (string);
	}
	else
	{
		_r_menu_additem (hmenu, item_id, text->buffer);
	}
}

// Adds "everything from this publisher" / "everything in this folder" items to
// the Allow or Block dropdown of the notification window.
VOID _app_autorules_generatemenu (
	_In_ HMENU hmenu,
	_In_ ULONG app_hash,
	_In_ BOOLEAN is_allow
)
{
	ITEM_AUTORULE temp_rule = {0};
	PITEM_APP ptr_app;
	PR_STRING signature, pattern, text;
	UINT id_publisher, id_path;

	ptr_app = _app_getappitem (app_hash);

	if (!ptr_app)
		return;

	if (ptr_app->type != DATA_APP_REGULAR || _r_obj_isstringempty (ptr_app->real_path))
	{
		_r_obj_dereference (ptr_app);
		return;
	}

	id_publisher = is_allow ? IDM_AUTORULE_ALLOW_PUBLISHER : IDM_AUTORULE_BLOCK_PUBLISHER;
	id_path = is_allow ? IDM_AUTORULE_ALLOW_PATH : IDM_AUTORULE_BLOCK_PATH;

	if (is_allow && GetMenuItemCount (hmenu) > 0)
		_r_menu_addseparator (hmenu);

	// publisher
	signature = _app_autorules_getappsignature (ptr_app, TRUE);

	if (signature)
	{
		text = _r_format_string (_r_locale_getstring (is_allow ? IDS_AUTORULE_ALLOW_PUBLISHER : IDS_AUTORULE_BLOCK_PUBLISHER), signature->buffer);

		temp_rule.publisher = signature;

		_app_autorules_addmenuitem (hmenu, id_publisher, text, _app_autorules_countpending (&temp_rule, app_hash));

		temp_rule.publisher = NULL;

		_r_obj_dereference (text);
		_r_obj_dereference (signature);
	}
	else
	{
		_r_menu_additem (hmenu, id_publisher, _r_locale_getstring (IDS_AUTORULE_NOPUBLISHER));
		_r_menu_enableitem (hmenu, id_publisher, FALSE, FALSE);
	}

	// versioned folder
	pattern = _app_autorules_makepathpattern (ptr_app->real_path);

	if (pattern)
	{
		text = _r_format_string (_r_locale_getstring (is_allow ? IDS_AUTORULE_ALLOW_PATH : IDS_AUTORULE_BLOCK_PATH), pattern->buffer);

		temp_rule.path_expanded = pattern;

		_app_autorules_addmenuitem (hmenu, id_path, text, _app_autorules_countpending (&temp_rule, app_hash));

		temp_rule.path_expanded = NULL;

		_r_obj_dereference (text);
		_r_obj_dereference (pattern);
	}
	else
	{
		_r_menu_additem (hmenu, id_path, _r_locale_getstring (IDS_AUTORULE_NOPATH));
		_r_menu_enableitem (hmenu, id_path, FALSE, FALSE);
	}

	_r_obj_dereference (ptr_app);
}

// Creates an auto rule from the app shown in the notification window and
// applies it right away to that app, to every other pending notification it
// covers, and to every undecided app already in the list.
BOOLEAN _app_autorules_createfromnotify (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	PITEM_AUTORULE ptr_rule = NULL;
	PITEM_APP ptr_app, ptr_item = NULL;
	PR_STRING signature, pattern, unexpanded;
	PR_LIST apps;
	ULONG_PTR enum_key = 0;
	ULONG app_hash;
	BOOLEAN is_allow, is_publisher, is_changed = FALSE;

	is_allow = (ctrl_id == IDM_AUTORULE_ALLOW_PUBLISHER || ctrl_id == IDM_AUTORULE_ALLOW_PATH);
	is_publisher = (ctrl_id == IDM_AUTORULE_ALLOW_PUBLISHER || ctrl_id == IDM_AUTORULE_BLOCK_PUBLISHER);

	app_hash = _app_notify_getapp_id (hwnd);
	ptr_app = _app_getappitem (app_hash);

	if (!ptr_app)
		return FALSE;

	if (is_publisher)
	{
		signature = _app_autorules_getappsignature (ptr_app, TRUE);

		if (signature)
		{
			ptr_rule = _app_autorules_add (signature, NULL, is_allow);

			_r_obj_dereference (signature);
		}
	}
	else
	{
		pattern = _app_autorules_makepathpattern (ptr_app->real_path);

		if (pattern)
		{
			unexpanded = _r_str_environmentunexpandstring (&pattern->sr);

			ptr_rule = _app_autorules_add (NULL, unexpanded ? unexpanded : pattern, is_allow);

			if (unexpanded)
				_r_obj_dereference (unexpanded);

			_r_obj_dereference (pattern);
		}
	}

	_r_obj_dereference (ptr_app);

	if (!ptr_rule)
		return FALSE;

	// collect apps first, filters and listview updates must not run under the apps lock
	apps = _r_obj_createlist (0x10, &_r_obj_dereference);

	_r_queuedlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtablepointer (apps_table, (PVOID_PTR)&ptr_item, NULL, &enum_key))
	{
		if (ptr_item->type != DATA_APP_REGULAR)
			continue;

		if (ptr_item->is_enabled || ptr_item->is_silent || ptr_item->is_undeletable)
			continue;

		_r_obj_addlistitem (apps, _r_obj_reference (ptr_item), NULL);
	}

	_r_queuedlock_releaseshared (&lock_apps);

	for (ULONG_PTR i = 0; i < _r_obj_getlistsize (apps); i++)
	{
		ptr_item = (PITEM_APP)_r_obj_getlistitem (apps, i);

		if (!ptr_item)
			continue;

		// apps waiting for an answer get a real signature check, the rest use the cache
		if (_app_autorules_applytoapp (hwnd, ptr_item, (ptr_item->notification != NULL) || (ptr_item->app_hash == app_hash)))
		{
			if (ptr_item->notification)
				_r_obj_clearreference ((PVOID_PTR)&ptr_item->notification);

			is_changed = TRUE;
		}
	}

	_r_obj_dereference (apps);

	// show the next pending notification, or close the window
	if (_app_notify_getnextapp_id (hwnd) != 0)
	{
		_app_notify_refresh (hwnd);
	}
	else
	{
		DestroyWindow (hwnd);
	}

	_app_profile_save (NULL);

	return is_changed;
}

// "Edit > Auto rules": a popup with every rule, clicking one deletes it.
VOID _app_autorules_showmanager (
	_In_ HWND hwnd
)
{
	PITEM_AUTORULE ptr_rule;
	PR_STRING string;
	HMENU hmenu;
	ULONG_PTR count;

	hmenu = CreatePopupMenu ();

	if (!hmenu)
		return;

	_r_queuedlock_acquireshared (&lock_autorules);

	count = _r_obj_getlistsize (autorules_list);

	if (count == 0)
	{
		_r_menu_additem (hmenu, IDX_AUTORULES, _r_locale_getstring (IDS_AUTORULES_EMPTY));
		_r_menu_enableitem (hmenu, IDX_AUTORULES, FALSE, FALSE);
	}
	else
	{
		_r_menu_additem (hmenu, 0, _r_locale_getstring (IDS_AUTORULES_HINT));
		_r_menu_enableitem (hmenu, 0, TRUE, FALSE);
		_r_menu_addseparator (hmenu);

		for (ULONG_PTR i = 0; i < count && i < AUTORULES_MAX; i++)
		{
			ptr_rule = (PITEM_AUTORULE)_r_obj_getlistitem (autorules_list, i);

			if (!ptr_rule)
				continue;

			string = _app_autorules_getdescription (ptr_rule);

			if (string)
			{
				_r_menu_additem (hmenu, IDX_AUTORULES + (UINT)i, string->buffer);

				_r_obj_dereference (string);
			}
		}
	}

	_r_queuedlock_releaseshared (&lock_autorules);

	_r_menu_popup (hmenu, hwnd, NULL, 0);

	DestroyMenu (hmenu);
}

VOID _app_autorules_removefrommenu (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	ULONG_PTR index;

	if (ctrl_id < IDX_AUTORULES || ctrl_id >= IDX_AUTORULES + AUTORULES_MAX)
		return;

	index = (ULONG_PTR)(ctrl_id - IDX_AUTORULES);

	if (_r_show_message (hwnd, MB_YESNO | MB_ICONEXCLAMATION, NULL, _r_locale_getstring (IDS_QUESTION_DELETE)) != IDYES)
		return;

	if (_app_autorules_remove (index))
		_app_profile_save (hwnd);
}
