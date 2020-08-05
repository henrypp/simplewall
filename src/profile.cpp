// simplewall
// Copyright (c) 2016-2020 Henry++

#include "global.hpp"

LONG_PTR _app_getappinfo (SIZE_T app_hash, ENUM_INFO_DATA info_key)
{
	LONG_PTR result;
	PITEM_APP ptr_app;

	ptr_app = _app_getappitem (app_hash);

	if (!ptr_app)
		return 0;

	result = 0;

	if (info_key == InfoPath)
	{
		if (!_r_str_isempty (ptr_app->real_path))
			result = (LONG_PTR)ptr_app->real_path->Buffer;
	}
	else if (info_key == InfoName)
	{
		if (!_r_str_isempty (ptr_app->display_name))
			result = (LONG_PTR)ptr_app->display_name->Buffer;

		else if (!_r_str_isempty (ptr_app->original_path))
			result = (LONG_PTR)ptr_app->original_path->Buffer;
	}
	else if (info_key == InfoTimestampPtr)
	{
		result = (LONG_PTR)&ptr_app->timestamp;
	}
	else if (info_key == InfoTimerPtr)
	{
		result = (LONG_PTR)&ptr_app->timer;
	}
	else if (info_key == InfoIconId)
	{
		result = (LONG_PTR)ptr_app->icon_id;
	}
	else if (info_key == InfoListviewId)
	{
		result = (LONG_PTR)_app_getlistview_id (ptr_app->type);
	}
	else if (info_key == InfoIsSilent)
	{
		result = ptr_app->is_silent ? TRUE : FALSE;
	}
	else if (info_key == InfoIsUndeletable)
	{
		result = ptr_app->is_undeletable ? TRUE : FALSE;
	}

	_r_obj_dereference (ptr_app);

	return result;
}

BOOLEAN _app_setappinfo (SIZE_T app_hash, ENUM_INFO_DATA info_key, LONG_PTR info_value)
{
	PITEM_APP ptr_app = _app_getappitem (app_hash);

	if (!ptr_app)
		return FALSE;

	if (info_key == InfoIsUndeletable)
	{
		ptr_app->is_undeletable = info_value ? TRUE : FALSE;
	}

	_r_obj_dereference (ptr_app);

	return TRUE;
}

SIZE_T _app_addapplication (HWND hwnd, LPCWSTR path, time_t timestamp, time_t timer, time_t last_notify, BOOLEAN is_silent, BOOLEAN is_enabled)
{
	if (_r_str_isempty (path) || PathIsDirectory (path))
		return 0;

	WCHAR path_full[1024];
	PITEM_APP ptr_app;
	PR_STRING signatureString;
	SIZE_T app_length;
	SIZE_T app_hash;
	BOOLEAN is_ntoskrnl;

	app_length = _r_str_length (path);

	// prevent possible duplicate apps entries with short path (issue #640)
	if (_r_str_findchar (path, app_length, L'~') != INVALID_SIZE_T)
	{
		if (GetLongPathName (path, path_full, RTL_NUMBER_OF (path_full)))
		{
			path = path_full;
			app_length = _r_str_length (path_full);
		}
	}

	app_hash = _r_str_hash (path);

	if (_app_isappfound (app_hash))
		return app_hash; // already exists

	ptr_app = (PITEM_APP)_r_obj_allocateex (sizeof (ITEM_APP), &_app_dereferenceapp);
	is_ntoskrnl = (app_hash == config.ntoskrnl_hash);

	if (_r_str_compare_length (path, L"\\device\\", 8) == 0) // device path
	{
		ptr_app->real_path = _r_obj_createstring (path);
		ptr_app->type = DataAppDevice;
	}
	else if (_r_str_compare_length (path, L"S-1-", 4) == 0) // windows store (win8+)
	{
		_app_item_get (DataAppUWP, app_hash, NULL, &ptr_app->real_path, timestamp ? NULL : &timestamp, NULL);
		ptr_app->type = DataAppUWP;
	}
	else
	{
		if (!is_ntoskrnl && _r_str_findchar (path, app_length, OBJ_NAME_PATH_SEPARATOR) == INVALID_SIZE_T)
		{
			if (_app_item_get (DataAppService, app_hash, NULL, &ptr_app->real_path, timestamp ? NULL : &timestamp, NULL))
			{
				ptr_app->type = DataAppService;
			}
			else
			{
				ptr_app->real_path = _r_obj_createstring (path);
				ptr_app->type = DataAppPico;
			}
		}
		else
		{
			if (is_ntoskrnl) // "System" process
			{
				ptr_app->real_path = _r_obj_createstring2 (config.ntoskrnl_path);
			}
			else
			{
				ptr_app->real_path = _r_obj_createstring (path);
			}

			if (PathIsNetworkPath (path))
			{
				ptr_app->type = DataAppNetwork; // network path
			}
			else
			{
				ptr_app->type = DataAppRegular;
			}
		}
	}

	if (!_r_str_isempty (ptr_app->real_path) && ptr_app->type == DataAppRegular)
	{
		DWORD dwAttr = GetFileAttributes (_r_obj_getstring (ptr_app->real_path));

		ptr_app->is_system = is_ntoskrnl || (dwAttr & FILE_ATTRIBUTE_SYSTEM) != 0 || (_r_str_compare_length (_r_obj_getstring (ptr_app->real_path), config.windows_dir, config.wd_length) == 0);
	}

	ptr_app->original_path = _r_obj_createstringex (path, app_length * sizeof (WCHAR));

	if (is_ntoskrnl && !_r_str_isempty (ptr_app->original_path))
	{
		_r_str_tolower (ptr_app->original_path);
		ptr_app->original_path->Buffer[0] = _r_str_upper (ptr_app->original_path->Buffer[0]); // fix "System" lowercase
	}

	// get display name
	_r_obj_movereference (&ptr_app->display_name, _app_getdisplayname (app_hash, ptr_app));

	// get signature information
	if (_r_config_getboolean (L"IsCertificatesEnabled", FALSE))
	{
		signatureString = _app_getsignatureinfo (app_hash, ptr_app);

		if (signatureString)
			_r_obj_dereference (signatureString);
	}

	ptr_app->guids = new GUIDS_VEC; // initialize stl

	ptr_app->is_enabled = is_enabled;
	ptr_app->is_silent = is_silent;

	ptr_app->timestamp = timestamp ? timestamp : _r_unixtime_now ();
	ptr_app->timer = timer;
	ptr_app->last_notify = last_notify;

	if (ptr_app->type == DataAppService || ptr_app->type == DataAppUWP)
		ptr_app->is_undeletable = TRUE;

	// insert object into the map
	apps.emplace (app_hash, ptr_app);

	// insert item
	if (hwnd)
	{
		INT listview_id = _app_getlistview_id (ptr_app->type);

		if (listview_id)
		{
			_r_fastlock_acquireshared (&lock_checkbox);

			_r_listview_additemex (hwnd, listview_id, 0, 0, _r_obj_getstringordefault (ptr_app->display_name, SZ_EMPTY), ptr_app->icon_id, _app_getappgroup (app_hash, ptr_app), app_hash);
			_app_setappiteminfo (hwnd, listview_id, 0, app_hash, ptr_app);

			_r_fastlock_releaseshared (&lock_checkbox);
		}
	}

	return app_hash;
}

PITEM_APP _app_getappitem (SIZE_T app_hash)
{
	if (_app_isappfound (app_hash))
	{
		PVOID pdata = apps.at (app_hash);

		if (pdata)
			return (PITEM_APP)_r_obj_reference (pdata);
	}

	return NULL;
}

PITEM_APP_HELPER _app_getapphelperitem (SIZE_T app_hash)
{
	if (_app_isapphelperfound (app_hash))
	{
		PVOID pdata = apps_helper.at (app_hash);

		if (pdata)
			return (PITEM_APP_HELPER)_r_obj_reference (pdata);
	}

	return NULL;
}

PITEM_RULE _app_getrulebyid (SIZE_T idx)
{
	if (idx != INVALID_SIZE_T && idx < rules_arr.size ())
	{
		PITEM_RULE pdata = rules_arr.at (idx);

		if (pdata)
			return (PITEM_RULE)_r_obj_reference (pdata);
	}

	return NULL;
}

PITEM_RULE _app_getrulebyhash (SIZE_T rule_hash)
{
	if (!rule_hash)
		return NULL;

	for (auto it = rules_arr.begin (); it != rules_arr.end (); ++it)
	{
		if (!*it)
			continue;

		PITEM_RULE ptr_rule = (PITEM_RULE)_r_obj_reference (*it);

		if (ptr_rule->is_readonly)
		{
			if (_r_str_hash (ptr_rule->name) == rule_hash)
				return ptr_rule;
		}

		_r_obj_dereference (ptr_rule);
	}

	return NULL;
}

PITEM_NETWORK _app_getnetworkitem (SIZE_T network_hash)
{
	if (network_map.find (network_hash) != network_map.end ())
	{
		PVOID pdata = network_map.at (network_hash);

		if (pdata)
			return (PITEM_NETWORK)_r_obj_reference (pdata);
	}

	return NULL;
}

SIZE_T _app_getnetworkapp (SIZE_T network_hash)
{
	PITEM_NETWORK ptr_network = _app_getnetworkitem (network_hash);

	if (ptr_network)
	{
		SIZE_T app_hash = ptr_network->app_hash;

		_r_obj_dereference (ptr_network);

		return app_hash;
	}

	return 0;
}

PITEM_LOG _app_getlogitem (SIZE_T idx)
{
	if (idx != INVALID_SIZE_T && idx < log_arr.size ())
	{
		PVOID pdata = log_arr.at (idx);

		if (pdata)
			return (PITEM_LOG)_r_obj_reference (pdata);
	}

	return NULL;
}

SIZE_T _app_getlogapp (SIZE_T idx)
{
	PITEM_LOG ptr_log = _app_getlogitem (idx);

	if (ptr_log)
	{
		SIZE_T app_hash = ptr_log->app_hash;

		_r_obj_dereference (ptr_log);

		return app_hash;
	}

	return 0;
}

COLORREF _app_getappcolor (INT listview_id, SIZE_T app_hash, BOOLEAN is_validconnection)
{
	PITEM_APP ptr_app = _app_getappitem (app_hash);

	if (!ptr_app)
		return 0;

	LPCWSTR colorValue = NULL;

	BOOLEAN is_profilelist = (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_RULES_CUSTOM);

	if (_r_config_getboolean (L"IsHighlightInvalid", TRUE, L"colors") && !_app_isappexists (ptr_app))
		colorValue = L"ColorInvalid";

	else if (_r_config_getboolean (L"IsHighlightTimer", TRUE, L"colors") && _app_istimeractive (ptr_app))
		colorValue = L"ColorTimer";

	else if (_r_config_getboolean (L"IsHighlightConnection", TRUE, L"colors") && is_validconnection)
		colorValue = L"ColorConnection";

	else if (_r_config_getboolean (L"IsHighlightSigned", TRUE, L"colors") && !ptr_app->is_silent && _r_config_getboolean (L"IsCertificatesEnabled", FALSE) && ptr_app->is_signed)
		colorValue = L"ColorSigned";

	else if (!is_profilelist && (_r_config_getboolean (L"IsHighlightSpecial", TRUE, L"colors") && _app_isapphaverule (app_hash)))
		colorValue = L"ColorSpecial";

	else if (is_profilelist && _r_config_getboolean (L"IsHighlightSilent", TRUE, L"colors") && ptr_app->is_silent)
		colorValue = L"ColorSilent";

	else if (_r_config_getboolean (L"IsHighlightPico", TRUE, L"colors") && ptr_app->type == DataAppPico)
		colorValue = L"ColorPico";

	else if (_r_config_getboolean (L"IsHighlightSystem", TRUE, L"colors") && ptr_app->is_system)
		colorValue = L"ColorSystem";

	_r_obj_dereference (ptr_app);

	if (colorValue)
		return _app_getcolorvalue (_r_str_hash (colorValue));

	return 0;
}

VOID _app_freeapplication (SIZE_T app_hash)
{
	if (!app_hash)
		return;

	INT index = INVALID_INT;

	for (auto it = rules_arr.begin (); it != rules_arr.end (); ++it)
	{
		index += 1;

		if (!*it)
			continue;

		PITEM_RULE ptr_rule = (PITEM_RULE)_r_obj_reference (*it);

		if (ptr_rule->type == DataRuleCustom)
		{
			if (ptr_rule->apps->find (app_hash) != ptr_rule->apps->end ())
			{
				ptr_rule->apps->erase (app_hash);

				if (ptr_rule->is_enabled && ptr_rule->apps->empty ())
				{
					ptr_rule->is_enabled = FALSE;
					ptr_rule->is_haveerrors = FALSE;
				}

				INT rule_listview_id = _app_getlistview_id (ptr_rule->type);

				if (rule_listview_id)
				{
					INT item_pos = _app_getposition (_r_app_gethwnd (), rule_listview_id, index);

					if (item_pos != INVALID_INT)
					{
						_r_fastlock_acquireshared (&lock_checkbox);
						_app_setruleiteminfo (_r_app_gethwnd (), rule_listview_id, item_pos, ptr_rule, FALSE);
						_r_fastlock_releaseshared (&lock_checkbox);
					}
				}
			}
		}

		_r_obj_dereference (ptr_rule);
	}

	apps.erase (app_hash);
}

VOID _app_getcount (PITEM_STATUS ptr_status)
{
	PITEM_APP ptr_app;
	PITEM_RULE ptr_rule;
	SIZE_T app_hash;
	BOOLEAN is_used;

	for (auto it = apps.begin (); it != apps.end (); ++it)
	{
		if (!it->second)
			continue;

		app_hash = it->first;
		ptr_app = (PITEM_APP)_r_obj_reference (it->second);
		is_used = _app_isappused (ptr_app, app_hash);

		if (_app_istimeractive (ptr_app))
			ptr_status->apps_timer_count += 1;

		if (!ptr_app->is_undeletable && (!_app_isappexists (ptr_app) || !is_used) && !(ptr_app->type == DataAppService || ptr_app->type == DataAppUWP))
			ptr_status->apps_unused_count += 1;

		if (is_used)
			ptr_status->apps_count += 1;

		_r_obj_dereference (ptr_app);
	}

	for (auto it = rules_arr.begin (); it != rules_arr.end (); ++it)
	{
		if (!*it)
			continue;

		ptr_rule = (PITEM_RULE)_r_obj_reference (*it);

		if (ptr_rule->type == DataRuleCustom)
		{
			if (ptr_rule->is_enabled && !ptr_rule->apps->empty ())
				ptr_status->rules_global_count += 1;

			if (ptr_rule->is_readonly)
				ptr_status->rules_predefined_count += 1;

			else
				ptr_status->rules_user_count += 1;

			ptr_status->rules_count += 1;
		}

		_r_obj_dereference (ptr_rule);
	}
}

INT _app_getappgroup (SIZE_T app_hash, PITEM_APP ptr_app)
{
	// apps with special rule
	if (_app_isapphaverule (app_hash))
		return 1;

	if (!ptr_app->is_enabled)
		return 2;

	return 0;
}

INT _app_getnetworkgroup (PITEM_NETWORK ptr_network)
{
	if (ptr_network->type == DataAppService)
		return 1;

	if (ptr_network->type == DataAppUWP)
		return 2;

	return 0;
}

INT _app_getrulegroup (PITEM_RULE ptr_rule)
{
	if (!ptr_rule->is_enabled)
		return 2;

	return 0;
}

INT _app_getruleicon (PITEM_RULE ptr_rule)
{
	if (ptr_rule->is_block)
		return 1;

	return 0;
}

COLORREF _app_getrulecolor (INT listview_id, SIZE_T rule_idx)
{
	PITEM_RULE ptr_rule = _app_getrulebyid (rule_idx);

	if (!ptr_rule)
		return 0;

	LPCWSTR colorValue = NULL;

	if (_r_config_getboolean (L"IsHighlightInvalid", TRUE, L"colors") && ptr_rule->is_enabled && ptr_rule->is_haveerrors)
		colorValue = L"ColorInvalid";

	else if (_r_config_getboolean (L"IsHighlightSpecial", TRUE, L"colors") && !ptr_rule->apps->empty ())
		colorValue = L"ColorSpecial";

	_r_obj_dereference (ptr_rule);

	if (colorValue)
		return _app_getcolorvalue (_r_str_hash (colorValue));

	return 0;
}

PR_STRING _app_gettooltip (HWND hwnd, LPNMLVGETINFOTIP lpnmlv)
{
	PR_STRING string = NULL;

	INT listview_id = PtrToInt ((PVOID)lpnmlv->hdr.idFrom);
	BOOLEAN is_appslist = (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP);
	BOOLEAN is_ruleslist = (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM);

	if (is_appslist || listview_id == IDC_RULE_APPS_ID)
	{
		LPARAM lparam = _r_listview_getitemlparam (hwnd, listview_id, lpnmlv->iItem);
		PITEM_APP ptr_app = _app_getappitem (lparam);
		PR_STRING displayName = NULL;

		if (ptr_app)
		{
			string = _r_format_string (L"%s\r\n", _r_obj_getstring (!_r_str_isempty (ptr_app->real_path) ? ptr_app->real_path : (!_r_str_isempty (ptr_app->display_name) ? ptr_app->display_name : ptr_app->original_path)));

			// app information
			{
				PR_STRING infoString;

				infoString = _r_obj_createstringbuilder ();

				if (ptr_app->type == DataAppRegular)
				{
					PR_STRING versionString = _app_getversioninfo (lparam, ptr_app);

					if (versionString)
					{
						if (!_r_str_isempty (versionString))
							_r_string_appendformat (&infoString, SZ_TAB L"%s\r\n", versionString->Buffer);

						_r_obj_dereference (versionString);
					}
				}
				else if (ptr_app->type == DataAppService)
				{
					if (_app_item_get (ptr_app->type, lparam, &displayName, NULL, NULL, NULL))
					{
						_r_string_appendformat (&infoString, SZ_TAB L"%s" SZ_TAB_CRLF L"%s\r\n", _r_obj_getstringorempty (ptr_app->original_path), _r_obj_getstringorempty (displayName));

						if (displayName)
							_r_obj_dereference (displayName);
					}
				}
				else if (ptr_app->type == DataAppUWP)
				{
					if (_app_item_get (ptr_app->type, lparam, &displayName, NULL, NULL, NULL))
					{
						_r_string_appendformat (&infoString, SZ_TAB L"%s\r\n", _r_obj_getstringorempty (displayName));

						if (displayName)
							_r_obj_dereference (displayName);
					}
				}

				// signature information
				if (_r_config_getboolean (L"IsCertificatesEnabled", FALSE) && ptr_app->is_signed)
				{
					PR_STRING signatureString = _app_getsignatureinfo (lparam, ptr_app);

					if (signatureString)
					{
						if (!_r_str_isempty (signatureString))
							_r_string_appendformat (&infoString, SZ_TAB L"%s: %s\r\n", _r_locale_getstring (IDS_SIGNATURE), signatureString->Buffer);

						_r_obj_dereference (signatureString);
					}
				}

				if (!_r_str_isempty (infoString))
				{
					_r_string_insertformat (&infoString, 0, L"%s:\r\n", _r_locale_getstring (IDS_FILE));
					_r_string_append2 (&string, infoString);
				}

				_r_obj_dereference (infoString);
			}

			// app timer
			if (_app_istimeractive (ptr_app))
			{
				WCHAR intervalString[128];
				_r_format_interval (intervalString, RTL_NUMBER_OF (intervalString), ptr_app->timer - _r_unixtime_now (), 3);

				_r_string_appendformat (&string, L"%s:" SZ_TAB_CRLF L"%s\r\n", _r_locale_getstring (IDS_TIMELEFT), intervalString);
			}

			// app rules
			{
				PR_STRING appRulesString = _app_appexpandrules (lparam, SZ_TAB_CRLF);

				if (appRulesString)
				{
					if (!_r_str_isempty (appRulesString))
						_r_string_appendformat (&string, L"%s:" SZ_TAB_CRLF L"%s\r\n", _r_locale_getstring (IDS_RULE), appRulesString->Buffer);

					_r_obj_dereference (appRulesString);
				}
			}

			// app notes
			{
				PR_STRING notesString;

				notesString = _r_obj_createstringbuilder ();

				// app type
				if (ptr_app->type == DataAppNetwork)
					_r_string_appendformat (&notesString, SZ_TAB L"%s\r\n", _r_locale_getstring (IDS_HIGHLIGHT_NETWORK));

				else if (ptr_app->type == DataAppPico)
					_r_string_appendformat (&notesString, SZ_TAB L"%s\r\n", _r_locale_getstring (IDS_HIGHLIGHT_PICO));

				// app settings
				if (ptr_app->is_system)
					_r_string_appendformat (&notesString, SZ_TAB L"%s\r\n", _r_locale_getstring (IDS_HIGHLIGHT_SYSTEM));

				if (_app_isapphaveconnection (lparam))
					_r_string_appendformat (&notesString, SZ_TAB L"%s\r\n", _r_locale_getstring (IDS_HIGHLIGHT_CONNECTION));

				if (is_appslist && ptr_app->is_silent)
					_r_string_appendformat (&notesString, SZ_TAB L"%s\r\n", _r_locale_getstring (IDS_HIGHLIGHT_SILENT));

				if (!_app_isappexists (ptr_app))
					_r_string_appendformat (&notesString, SZ_TAB L"%s\r\n", _r_locale_getstring (IDS_HIGHLIGHT_INVALID));

				if (!_r_str_isempty (notesString))
				{
					_r_string_insertformat (&notesString, 0, L"%s:\r\n", _r_locale_getstring (IDS_NOTES));
					_r_string_append2 (&string, notesString);
				}

				_r_obj_dereference (notesString);
			}

			_r_obj_dereference (ptr_app);
		}
	}
	else if (is_ruleslist)
	{
		LPARAM lparam = _r_listview_getitemlparam (hwnd, listview_id, lpnmlv->iItem);
		PITEM_RULE ptr_rule = _app_getrulebyid (lparam);

		if (ptr_rule)
		{
			LPCWSTR empty;

			PR_STRING ruleRemoteString = _app_rulesexpandrules (ptr_rule->rule_remote, L"\r\n" SZ_TAB);
			PR_STRING ruleLocalString = _app_rulesexpandrules (ptr_rule->rule_local, L"\r\n" SZ_TAB);

			empty = _r_locale_getstring (IDS_STATUS_EMPTY);

			// rule information
			string = _r_format_string (L"%s (#%" TEXT (PR_SIZE_T) L")\r\n%s (" SZ_DIRECTION_REMOTE L"):\r\n%s%s\r\n%s (" SZ_DIRECTION_LOCAL L"):\r\n%s%s",
									   _r_obj_getstringordefault (ptr_rule->name, empty),
									   lparam,
									   _r_locale_getstring (IDS_RULE),
									   SZ_TAB,
									   _r_obj_getstringordefault (ruleRemoteString, empty),
									   _r_locale_getstring (IDS_RULE),
									   SZ_TAB,
									   _r_obj_getstringordefault (ruleLocalString, empty)
			);

			SAFE_DELETE_REFERENCE (ruleRemoteString);
			SAFE_DELETE_REFERENCE (ruleLocalString);

			// rule apps
			if (ptr_rule->is_forservices || !ptr_rule->apps->empty ())
			{
				PR_STRING ruleAppsString = _app_rulesexpandapps (ptr_rule, TRUE, SZ_TAB_CRLF);

				if (ruleAppsString)
				{
					if (!_r_str_isempty (ruleAppsString))
						_r_string_appendformat (&string, L"\r\n%s:\r\n%s%s", _r_locale_getstring (IDS_TAB_APPS), SZ_TAB, ruleAppsString->Buffer);

					_r_obj_dereference (ruleAppsString);
				}
			}

			// rule notes
			if (ptr_rule->is_readonly && ptr_rule->type == DataRuleCustom)
			{
				_r_string_appendformat (&string, SZ_TAB L"\r\n%s:\r\n" SZ_TAB L"%s\r\n", _r_locale_getstring (IDS_NOTES), SZ_RULE_INTERNAL_TITLE);
			}

			_r_obj_dereference (ptr_rule);
		}
	}
	else if (listview_id == IDC_NETWORK)
	{
		LPARAM lparam = _r_listview_getitemlparam (hwnd, listview_id, lpnmlv->iItem);
		PITEM_NETWORK ptr_network = _app_getnetworkitem (lparam);

		if (ptr_network)
		{
			LPCWSTR empty;

			PR_STRING localAddressString;
			PR_STRING remoteAddressString;

			empty = _r_locale_getstring (IDS_STATUS_EMPTY);

			localAddressString = _app_formataddress (ptr_network->af, 0, &ptr_network->local_addr, ptr_network->local_port, 0);
			remoteAddressString = _app_formataddress (ptr_network->af, 0, &ptr_network->remote_addr, ptr_network->remote_port, 0);

			string = _r_format_string (L"%s\r\n%s (" SZ_DIRECTION_LOCAL L"):\r\n" SZ_TAB L"%s\r\n%s (" SZ_DIRECTION_REMOTE L"):\r\n" SZ_TAB L"%s\r\n%s:\r\n" SZ_TAB L"%s\r\n%s:\r\n" SZ_TAB L"%s",
									   _r_obj_getstringordefault (ptr_network->path, empty),
									   _r_locale_getstring (IDS_ADDRESS),
									   _r_obj_getstringordefault (localAddressString, empty),
									   _r_locale_getstring (IDS_ADDRESS),
									   _r_obj_getstringordefault (remoteAddressString, empty),
									   _r_locale_getstring (IDS_PROTOCOL),
									   _app_getprotoname (ptr_network->protocol, ptr_network->af, empty),
									   _r_locale_getstring (IDS_STATE),
									   _app_getconnectionstatusname (ptr_network->state, empty)
			);

			SAFE_DELETE_REFERENCE (localAddressString);
			SAFE_DELETE_REFERENCE (remoteAddressString);

			_r_obj_dereference (ptr_network);
		}

	}
	else if (listview_id == IDC_LOG)
	{
		LPARAM lparam = _r_listview_getitemlparam (hwnd, listview_id, lpnmlv->iItem);
		PITEM_LOG ptr_log = _app_getlogitem (lparam);

		if (ptr_log)
		{
			WCHAR dateString[256];

			LPCWSTR empty;

			PR_STRING localAddressString;
			PR_STRING remoteAddressString;
			PR_STRING directionString;

			_r_format_dateex (dateString, RTL_NUMBER_OF (dateString), ptr_log->timestamp, FDTF_LONGDATE | FDTF_LONGTIME);

			empty = _r_locale_getstring (IDS_STATUS_EMPTY);

			localAddressString = _app_formataddress (ptr_log->af, 0, &ptr_log->local_addr, ptr_log->local_port, 0);
			remoteAddressString = _app_formataddress (ptr_log->af, 0, &ptr_log->remote_addr, ptr_log->remote_port, 0);
			directionString = _app_getdirectionname (ptr_log->direction, ptr_log->is_loopback, TRUE);

			string = _r_format_string (L"%s\r\n%s:\r\n" SZ_TAB L"%s\r\n%s (" SZ_DIRECTION_LOCAL L"):\r\n" SZ_TAB L"%s\r\n%s (" SZ_DIRECTION_REMOTE L"):\r\n" SZ_TAB L"%s\r\n%s:\r\n" SZ_TAB L"%s\r\n%s:\r\n" SZ_TAB L"%s\r\n%s:\r\n" SZ_TAB L"%s\r\n%s:\r\n" SZ_TAB L"%s",
									   _r_obj_getstringordefault (ptr_log->path, empty),
									   _r_locale_getstring (IDS_DATE),
									   dateString,
									   _r_locale_getstring (IDS_ADDRESS),
									   _r_obj_getstringordefault (localAddressString, empty),
									   _r_locale_getstring (IDS_ADDRESS),
									   _r_obj_getstringordefault (remoteAddressString, empty),
									   _r_locale_getstring (IDS_PROTOCOL),
									   _app_getprotoname (ptr_log->protocol, ptr_log->af, empty),
									   _r_locale_getstring (IDS_FILTER),
									   _r_obj_getstringordefault (ptr_log->filter_name, empty),
									   _r_locale_getstring (IDS_DIRECTION),
									   _r_obj_getstringorempty (directionString),
									   _r_locale_getstring (IDS_STATE),
									   (ptr_log->is_allow ? SZ_STATE_ALLOW : SZ_STATE_BLOCK)
			);

			SAFE_DELETE_REFERENCE (localAddressString);
			SAFE_DELETE_REFERENCE (remoteAddressString);
			SAFE_DELETE_REFERENCE (directionString);

			_r_obj_dereference (ptr_log);
		}
	}
	else if (listview_id == IDC_RULE_REMOTE_ID || listview_id == IDC_RULE_LOCAL_ID)
	{
		PR_STRING itemText = _r_listview_getitemtext (hwnd, listview_id, lpnmlv->iItem, 0);

		if (itemText)
		{
			string = _r_obj_reference (itemText);

			_r_obj_dereference (itemText);
		}
	}

	return string;
}

VOID _app_setappiteminfo (HWND hwnd, INT listview_id, INT item, SIZE_T app_hash, PITEM_APP ptr_app)
{
	if (!listview_id || item == INVALID_INT)
		return;

	_app_getappicon (ptr_app, TRUE, &ptr_app->icon_id, NULL);

	_r_listview_setitemex (hwnd, listview_id, item, 0, _r_obj_getstringordefault (ptr_app->display_name, SZ_EMPTY), ptr_app->icon_id, _app_getappgroup (app_hash, ptr_app), 0);

	WCHAR dateString[256];
	_r_format_dateex (dateString, RTL_NUMBER_OF (dateString), ptr_app->timestamp, FDTF_SHORTDATE | FDTF_SHORTTIME);

	_r_listview_setitem (hwnd, listview_id, item, 1, dateString);

	_r_listview_setitemcheck (hwnd, listview_id, item, ptr_app->is_enabled);
}

VOID _app_setruleiteminfo (HWND hwnd, INT listview_id, INT item, PITEM_RULE ptr_rule, BOOLEAN include_apps)
{
	if (!listview_id || item == INVALID_INT)
		return;

	INT ruleIconId;
	INT ruleGroupId;

	ruleIconId = _app_getruleicon (ptr_rule);
	ruleGroupId = _app_getrulegroup (ptr_rule);

	LPCWSTR ruleNamePtr = NULL;
	WCHAR ruleName[RULE_NAME_CCH_MAX];
	PR_STRING directionString;

	if (!_r_str_isempty (ptr_rule->name))
	{
		if (ptr_rule->is_readonly && ptr_rule->type == DataRuleCustom)
		{
			_r_str_printf (ruleName, RTL_NUMBER_OF (ruleName), L"%s" SZ_RULE_INTERNAL_MENU, ptr_rule->name->Buffer);
			ruleNamePtr = ruleName;
		}
		else
		{
			ruleNamePtr = ptr_rule->name->Buffer;
		}
	}

	directionString = _app_getdirectionname (ptr_rule->direction, FALSE, TRUE);

	_r_listview_setitemex (hwnd, listview_id, item, 0, ruleNamePtr, ruleIconId, ruleGroupId, 0);
	_r_listview_setitem (hwnd, listview_id, item, 1, ptr_rule->protocol ? _app_getprotoname (ptr_rule->protocol, AF_UNSPEC, NULL) : _r_locale_getstring (IDS_ANY));
	_r_listview_setitem (hwnd, listview_id, item, 2, _r_obj_getstringorempty (directionString));

	_r_listview_setitemcheck (hwnd, listview_id, item, ptr_rule->is_enabled);

	if (directionString)
		_r_obj_dereference (directionString);

	if (include_apps)
	{
		SIZE_T app_hash;
		PITEM_APP ptr_app;
		INT app_listview_id;
		INT item_pos;

		for (auto it = ptr_rule->apps->begin (); it != ptr_rule->apps->end (); ++it)
		{
			app_hash = it->first;
			ptr_app = _app_getappitem (app_hash);

			if (!ptr_app)
				continue;

			app_listview_id = _app_getlistview_id (ptr_app->type);

			if (app_listview_id)
			{
				item_pos = _app_getposition (_r_app_gethwnd (), app_listview_id, app_hash);

				if (item_pos != INVALID_INT)
					_app_setappiteminfo (hwnd, app_listview_id, item_pos, app_hash, ptr_app);
			}

			_r_obj_dereference (ptr_app);
		}
	}
}

VOID _app_ruleenable (PITEM_RULE ptr_rule, BOOLEAN is_enable)
{
	ptr_rule->is_enabled = is_enable;

	if (ptr_rule->is_readonly && !_r_str_isempty (ptr_rule->name))
	{
		PVOID pdata;
		PITEM_RULE_CONFIG ptr_config;
		SIZE_T rule_hash = _r_str_hash (ptr_rule->name);

		if (rule_hash)
		{
			if (rules_config.find (rule_hash) != rules_config.end ())
			{
				pdata = rules_config[rule_hash];

				if (pdata)
				{
					ptr_config = (PITEM_RULE_CONFIG)_r_obj_reference (pdata);
					ptr_config->is_enabled = is_enable;

					_r_obj_dereference (ptr_config);
				}
				else
				{
					ptr_config = (PITEM_RULE_CONFIG)_r_obj_allocateex (sizeof (ITEM_RULE_CONFIG), &_app_dereferenceruleconfig);

					ptr_config->is_enabled = is_enable;
					ptr_config->name = _r_obj_createstring2 (ptr_rule->name);

					rules_config[rule_hash] = ptr_config;
				}
			}
			else
			{
				ptr_config = (PITEM_RULE_CONFIG)_r_obj_allocateex (sizeof (ITEM_RULE_CONFIG), &_app_dereferenceruleconfig);

				ptr_config->is_enabled = is_enable;
				ptr_config->name = _r_obj_createstring2 (ptr_rule->name);

				rules_config[rule_hash] = ptr_config;
			}
		}
	}
}

VOID _app_ruleenable2 (PITEM_RULE ptr_rule, BOOLEAN is_enable)
{
	ptr_rule->is_enabled = is_enable;

	if (ptr_rule->is_readonly && !_r_str_isempty (ptr_rule->name))
	{
		SIZE_T rule_hash = _r_str_hash (ptr_rule->name);

		if (rule_hash)
		{
			if (rules_config.find (rule_hash) != rules_config.end ())
			{
				PITEM_RULE_CONFIG ptr_config = (PITEM_RULE_CONFIG)_r_obj_reference (rules_config[rule_hash]);

				if (ptr_config)
				{
					ptr_config->is_enabled = is_enable;

					_r_obj_dereference (ptr_config);
				}
			}
		}
	}
}

BOOLEAN _app_ruleblocklistsetchange (PITEM_RULE ptr_rule, INT new_state)
{
	if (new_state == INVALID_INT)
		return FALSE; // don't change

	if (new_state == 0 && !ptr_rule->is_enabled)
		return FALSE; // not changed

	if (new_state == 1 && ptr_rule->is_enabled && !ptr_rule->is_block)
		return FALSE; // not changed

	if (new_state == 2 && ptr_rule->is_enabled && ptr_rule->is_block)
		return FALSE; // not changed

	ptr_rule->is_enabled = (new_state != 0);
	ptr_rule->is_enabled_default = ptr_rule->is_enabled; // set default value for rule

	if (new_state)
		ptr_rule->is_block = (new_state != 1);

	return TRUE;
}

BOOLEAN _app_ruleblocklistsetstate (PITEM_RULE ptr_rule, INT spy_state, INT update_state, INT extra_state)
{
	if (ptr_rule->type != DataRuleBlocklist || _r_str_isempty (ptr_rule->name))
		return FALSE;

	LPCWSTR ruleName = ptr_rule->name->Buffer;

	if (!ruleName)
		return FALSE;

	if (_r_str_compare_length (ruleName, L"spy_", 4) == 0)
		return _app_ruleblocklistsetchange (ptr_rule, spy_state);

	else if (_r_str_compare_length (ruleName, L"update_", 7) == 0)
		return _app_ruleblocklistsetchange (ptr_rule, update_state);

	else if (_r_str_compare_length (ruleName, L"extra_", 6) == 0)
		return _app_ruleblocklistsetchange (ptr_rule, extra_state);

	// fallback: block rules with other names by default!
	return _app_ruleblocklistsetchange (ptr_rule, 2);
}

VOID _app_ruleblocklistset (HWND hwnd, INT spy_state, INT update_state, INT extra_state, BOOLEAN is_instantapply)
{
	OBJECTS_RULE_VECTOR rules;

	INT listview_id = _app_getlistview_id (DataRuleBlocklist);

	SIZE_T changes_count = 0;
	INT index = INVALID_INT; // negative initial value is required for correct array indexing

	for (auto it = rules_arr.begin (); it != rules_arr.end (); ++it)
	{
		index += 1;

		if (!*it)
			continue;

		PITEM_RULE ptr_rule = (PITEM_RULE)_r_obj_reference (*it);

		if (ptr_rule->type != DataRuleBlocklist)
		{
			_r_obj_dereference (ptr_rule);
			continue;
		}

		if (!_app_ruleblocklistsetstate (ptr_rule, spy_state, update_state, extra_state))
		{
			_r_obj_dereference (ptr_rule);
			continue;
		}

		changes_count += 1;
		_app_ruleenable2 (ptr_rule, ptr_rule->is_enabled);

		if (hwnd)
		{
			INT item_pos = _app_getposition (hwnd, listview_id, index);

			if (item_pos != INVALID_INT)
			{
				_r_fastlock_acquireshared (&lock_checkbox);
				_app_setruleiteminfo (hwnd, listview_id, item_pos, ptr_rule, FALSE);
				_r_fastlock_releaseshared (&lock_checkbox);
			}
		}

		if (is_instantapply)
		{
			rules.emplace_back (ptr_rule); // be freed later!
			continue;
		}

		_r_obj_dereference (ptr_rule);
	}

	if (changes_count)
	{
		if (hwnd)
		{
			if (listview_id == (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT))
				_app_listviewsort (hwnd, listview_id, INVALID_INT, FALSE);

			_app_refreshstatus (hwnd, listview_id);
		}

		if (is_instantapply)
		{
			HANDLE hengine = _wfp_getenginehandle ();

			if (hengine)
				_wfp_create4filters (hengine, &rules, __LINE__);

			_app_freerules_vec (&rules);
		}

		_app_profile_save (); // required!
	}
}

PR_STRING _app_appexpandrules (SIZE_T app_hash, LPCWSTR delimeter)
{
	PR_STRING string;

	string = _r_obj_createstringbuilder ();

	for (auto it = rules_arr.begin (); it != rules_arr.end (); ++it)
	{
		if (!*it)
			continue;

		PITEM_RULE ptr_rule = (PITEM_RULE)_r_obj_reference (*it);

		if (ptr_rule->is_enabled && ptr_rule->type == DataRuleCustom && ptr_rule->apps->find (app_hash) != ptr_rule->apps->end ())
		{
			if (!_r_str_isempty (ptr_rule->name))
			{
				_r_string_append2 (&string, ptr_rule->name);

				if (ptr_rule->is_readonly)
					_r_string_append (&string, SZ_RULE_INTERNAL_MENU);

				_r_string_append (&string, delimeter);
			}
		}

		_r_obj_dereference (ptr_rule);
	}

	_r_str_trim (string, delimeter);

	if (_r_str_isempty (string))
	{
		_r_obj_dereference (string);
		return NULL;
	}

	return string;
}

PR_STRING _app_rulesexpandapps (const PITEM_RULE ptr_rule, BOOLEAN is_fordisplay, LPCWSTR delimeter)
{
	PR_STRING string;

	string = _r_obj_createstringbuilder ();

	if (is_fordisplay && ptr_rule->is_forservices)
	{
		_r_string_appendformat (&string, L"%s%s%s%s", PROC_SYSTEM_NAME, delimeter, _r_obj_getstring (config.svchost_path), delimeter);
	}

	for (auto it = ptr_rule->apps->begin (); it != ptr_rule->apps->end (); ++it)
	{
		PITEM_APP ptr_app = _app_getappitem (it->first);

		if (!ptr_app)
			continue;

		if (is_fordisplay)
		{
			if (ptr_app->type == DataAppUWP || ptr_app->type == DataAppService)
			{
				if (!_r_str_isempty (ptr_app->display_name))
					_r_string_append2 (&string, ptr_app->display_name);
			}
			else
			{
				if (!_r_str_isempty (ptr_app->original_path))
					_r_string_append2 (&string, ptr_app->original_path);
			}
		}
		else
		{
			if (!_r_str_isempty (ptr_app->original_path))
				_r_string_append2 (&string, ptr_app->original_path);
		}

		_r_string_append (&string, delimeter);

		_r_obj_dereference (ptr_app);
	}

	_r_str_trim (string, delimeter);

	if (_r_str_isempty (string))
	{
		_r_obj_dereference (string);
		return NULL;
	}

	return string;
}

PR_STRING _app_rulesexpandrules (PR_STRING rule, LPCWSTR delimeter)
{
	PR_STRING string;

	if (_r_str_isempty (rule))
		return NULL;

	string = _r_obj_createstringbuilder ();

	R_STRINGREF remainingPart;
	PR_STRING rulePart;

	_r_stringref_initialize2 (&remainingPart, rule);

	while (remainingPart.Length != 0)
	{
		rulePart = _r_str_splitatchar (&remainingPart, &remainingPart, DIVIDER_RULE[0]);

		if (rulePart)
		{
			_r_str_trim (rulePart, DIVIDER_TRIM);

			if (!_r_str_isempty (rulePart))
				_r_string_appendformat (&string, L"%s%s", rulePart->Buffer, delimeter);

			_r_obj_dereference (rulePart);
		}
	}

	_r_str_trim (string, delimeter);

	if (_r_str_isempty (string))
	{
		_r_obj_dereference (string);
		return NULL;
	}

	return string;
}

BOOLEAN _app_isappfound (SIZE_T app_hash)
{
	return app_hash && apps.find (app_hash) != apps.end ();
}

BOOLEAN _app_isapphelperfound (SIZE_T app_hash)
{
	return app_hash && apps_helper.find (app_hash) != apps_helper.end ();
}

BOOLEAN _app_isapphaveconnection (SIZE_T app_hash)
{
	if (!app_hash)
		return FALSE;

	for (auto it = network_map.begin (); it != network_map.end (); ++it)
	{
		if (!it->second)
			continue;

		PITEM_NETWORK ptr_network = (PITEM_NETWORK)_r_obj_reference (it->second);

		if (ptr_network->app_hash == app_hash)
		{
			if (ptr_network->is_connection)
			{
				_r_obj_dereference (ptr_network);
				return TRUE;
			}
		}

		_r_obj_dereference (ptr_network);
	}

	return FALSE;
}

BOOLEAN _app_isapphavedrive (INT letter)
{
	for (auto it = apps.begin (); it != apps.end (); ++it)
	{
		if (!it->second)
			continue;

		SIZE_T app_hash = it->first;
		PITEM_APP ptr_app = (PITEM_APP)_r_obj_reference (it->second);

		if (!_r_str_isempty (ptr_app->original_path))
		{
			INT drive_id = PathGetDriveNumber (ptr_app->original_path->Buffer);

			if ((drive_id != INVALID_INT && drive_id == letter) || ptr_app->type == DataAppDevice)
			{
				if (ptr_app->is_enabled || _app_isapphaverule (app_hash))
				{
					_r_obj_dereference (ptr_app);
					return TRUE;
				}
			}
		}

		_r_obj_dereference (ptr_app);
	}

	return FALSE;
}

BOOLEAN _app_isapphaverule (SIZE_T app_hash)
{
	if (!app_hash)
		return FALSE;

	for (auto it = rules_arr.begin (); it != rules_arr.end (); ++it)
	{
		if (!*it)
			continue;

		PITEM_RULE ptr_rule = (PITEM_RULE)_r_obj_reference (*it);

		if (ptr_rule->is_enabled && ptr_rule->type == DataRuleCustom && ptr_rule->apps->find (app_hash) != ptr_rule->apps->end ())
		{
			_r_obj_dereference (ptr_rule);
			return TRUE;
		}

		_r_obj_dereference (ptr_rule);
	}

	return FALSE;
}

BOOLEAN _app_isappused (const PITEM_APP ptr_app, SIZE_T app_hash)
{
	if (ptr_app && (ptr_app->is_enabled || ptr_app->is_silent) || _app_isapphaverule (app_hash))
		return TRUE;

	return FALSE;
}

BOOLEAN _app_isappexists (const PITEM_APP ptr_app)
{
	if (ptr_app->is_undeletable)
		return TRUE;

	if (ptr_app->is_enabled && ptr_app->is_haveerrors)
		return FALSE;

	if (ptr_app->type == DataAppDevice || ptr_app->type == DataAppNetwork || ptr_app->type == DataAppPico)
		return TRUE;

	else if (ptr_app->type == DataAppRegular)
		return !_r_str_isempty (ptr_app->real_path) && _r_fs_exists (ptr_app->real_path->Buffer);

	else if (ptr_app->type == DataAppService || ptr_app->type == DataAppUWP)
		return _app_item_get (ptr_app->type, _r_str_hash (ptr_app->original_path), NULL, NULL, NULL, NULL);

	return TRUE;
}

BOOLEAN _app_isrulehost (LPCWSTR rule)
{
	NET_ADDRESS_INFO addressInfo;
	memset (&addressInfo, 0, sizeof (NET_ADDRESS_INFO));

	USHORT port;
	BYTE prefix_length;

	DWORD types = NET_STRING_NAMED_ADDRESS | NET_STRING_NAMED_SERVICE;
	DWORD code = ParseNetworkString (rule, types, &addressInfo, &port, &prefix_length);

	return (code == ERROR_SUCCESS);
}

BOOLEAN _app_isruleip (LPCWSTR rule)
{
	NET_ADDRESS_INFO addressInfo;
	memset (&addressInfo, 0, sizeof (NET_ADDRESS_INFO));

	USHORT port;
	BYTE prefix_length;

	DWORD types = NET_STRING_IP_ADDRESS | NET_STRING_IP_SERVICE | NET_STRING_IP_NETWORK | NET_STRING_IP_ADDRESS_NO_SCOPE | NET_STRING_IP_SERVICE_NO_SCOPE;
	DWORD code = ParseNetworkString (rule, types, &addressInfo, &port, &prefix_length);

	return (code == ERROR_SUCCESS);
}

BOOLEAN _app_isruleport (LPCWSTR rule)
{
	for (SIZE_T i = 0; i < _r_str_length (rule); i++)
	{
		if (iswdigit (rule[i]) == 0 && rule[i] != DIVIDER_RULE_RANGE)
			return FALSE;
	}

	return TRUE;
}

BOOLEAN _app_isrulevalidchars (LPCWSTR rule)
{
	WCHAR valid_chars[] = {
		L'.',
		L':',
		L'[',
		L']',
		L'/',
		L'-',
		L'_',
	};

	for (SIZE_T i = 0; i < _r_str_length (rule); i++)
	{
		if (iswalnum (rule[i]) == 0)
		{
			BOOLEAN is_valid = FALSE;

			for (auto chr : valid_chars)
			{
				if (rule[i] == chr)
				{
					is_valid = TRUE;
					break;
				}
			}

			if (is_valid)
				continue;

			return FALSE;
		}
	}

	return TRUE;
}

BOOLEAN _app_profile_load_check_node (pugi::xml_node& root, ENUM_TYPE_XML type, BOOLEAN is_strict)
{
	if (root)
	{
		if (is_strict)
			return (root.attribute (L"type").as_int () == type);

		return (root.attribute (L"type").empty () || (root.attribute (L"type").as_int () == type));
	}

	return FALSE;
}

BOOLEAN _app_profile_load_check (LPCWSTR path, ENUM_TYPE_XML type, BOOLEAN is_strict)
{
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file (path, PUGIXML_LOAD_FLAGS, PUGIXML_LOAD_ENCODING);

	if (result)
	{
		pugi::xml_node root = doc.child (L"root");

		return _app_profile_load_check_node (root, type, is_strict);
	}

	return FALSE;
}

VOID _app_profile_load_fallback ()
{
	if (!_app_isappfound (config.my_hash))
		_app_addapplication (NULL, _r_app_getbinarypath (), 0, 0, 0, FALSE, TRUE);

	_app_setappinfo (config.my_hash, InfoIsUndeletable, TRUE);

	// disable deletion for this shit ;)
	if (!_r_config_getboolean (L"IsInternalRulesDisabled", FALSE))
	{
		if (!_app_isappfound (config.ntoskrnl_hash))
			_app_addapplication (NULL, PROC_SYSTEM_NAME, 0, 0, 0, FALSE, FALSE);

		if (!_app_isappfound (config.svchost_hash))
			_app_addapplication (NULL, _r_obj_getstring (config.svchost_path), 0, 0, 0, FALSE, FALSE);

		_app_setappinfo (config.ntoskrnl_hash, InfoIsUndeletable, TRUE);
		_app_setappinfo (config.svchost_hash, InfoIsUndeletable, TRUE);
	}
}

VOID _app_profile_load_helper (pugi::xml_node& root, ENUM_TYPE_DATA type, UINT version)
{
	INT blocklist_spy_state = _r_calc_clamp (INT, _r_config_getinteger (L"BlocklistSpyState", 2), 0, 2);
	INT blocklist_update_state = _r_calc_clamp (INT, _r_config_getinteger (L"BlocklistUpdateState", 0), 0, 2);
	INT blocklist_extra_state = _r_calc_clamp (INT, _r_config_getinteger (L"BlocklistExtraState", 0), 0, 2);

	for (pugi::xml_node item = root.child (L"item"); item; item = item.next_sibling (L"item"))
	{
		if (type == DataAppRegular)
		{
			_app_addapplication (NULL, item.attribute (L"path").as_string (), item.attribute (L"timestamp").as_llong (), item.attribute (L"timer").as_llong (), 0, item.attribute (L"is_silent").as_bool (), item.attribute (L"is_enabled").as_bool ());
		}
		else if (type == DataRuleBlocklist || type == DataRuleSystem || type == DataRuleCustom)
		{
			PITEM_RULE ptr_rule;
			PITEM_RULE_CONFIG ptr_config = NULL;
			SIZE_T rule_hash;
			BOOLEAN is_internal;

			ptr_rule = (PITEM_RULE)_r_obj_allocateex (sizeof (ITEM_RULE), &_app_dereferencerule);

			// initialize stl
			ptr_rule->apps = new HASHER_MAP;
			ptr_rule->guids = new GUIDS_VEC;

			// set rule name
			if (!item.attribute (L"name").empty ())
			{
				ptr_rule->name = _r_obj_createstring (item.attribute (L"name").as_string ());

				if (_r_obj_getstringlength (ptr_rule->name) > RULE_NAME_CCH_MAX)
					_r_string_setsize (ptr_rule->name, RULE_NAME_CCH_MAX * sizeof (WCHAR));

				rule_hash = _r_str_hash (ptr_rule->name);
			}

			// set rule destination
			if (!item.attribute (L"rule").empty ())
			{
				ptr_rule->rule_remote = _r_obj_createstring (item.attribute (L"rule").as_string ());

				if (_r_obj_getstringlength (ptr_rule->rule_remote) > RULE_RULE_CCH_MAX)
					_r_string_setsize (ptr_rule->rule_remote, RULE_RULE_CCH_MAX * sizeof (WCHAR));
			}

			// set rule source
			if (!item.attribute (L"rule_local").empty ())
			{
				ptr_rule->rule_local = _r_obj_createstring (item.attribute (L"rule_local").as_string ());

				if (_r_obj_getstringlength (ptr_rule->rule_local) > RULE_RULE_CCH_MAX)
					_r_string_setsize (ptr_rule->rule_local, RULE_RULE_CCH_MAX * sizeof (WCHAR));
			}

			ptr_rule->direction = (FWP_DIRECTION)item.attribute (L"dir").as_int ();
			ptr_rule->protocol = (UINT8)item.attribute (L"protocol").as_int ();
			ptr_rule->af = (ADDRESS_FAMILY)item.attribute (L"version").as_int ();

			ptr_rule->type = ((type == DataRuleSystem && item.attribute (L"is_custom").as_bool ()) ? DataRuleCustom : type);
			ptr_rule->is_block = item.attribute (L"is_block").as_bool ();
			ptr_rule->is_forservices = item.attribute (L"is_services").as_bool ();
			ptr_rule->is_readonly = (type != DataRuleCustom);

			// calculate rule weight
			if (type == DataRuleBlocklist)
				ptr_rule->weight = FILTER_WEIGHT_BLOCKLIST;

			else if (type == DataRuleSystem)
				ptr_rule->weight = FILTER_WEIGHT_SYSTEM;

			else if (type == DataRuleCustom)
				ptr_rule->weight = ptr_rule->is_block ? FILTER_WEIGHT_CUSTOM_BLOCK : FILTER_WEIGHT_CUSTOM;

			ptr_rule->is_enabled = item.attribute (L"is_enabled").as_bool ();

			if (type == DataRuleBlocklist)
			{
				_app_ruleblocklistsetstate (ptr_rule, blocklist_spy_state, blocklist_update_state, blocklist_extra_state);
			}
			else
			{
				ptr_rule->is_enabled_default = ptr_rule->is_enabled; // set default value for rule
			}

			// load rules config
			is_internal = (type == DataRuleBlocklist || type == DataRuleSystem);

			if (is_internal)
			{
				// internal rules
				if (rules_config.find (rule_hash) != rules_config.end ())
				{
					ptr_config = (PITEM_RULE_CONFIG)_r_obj_reference (rules_config[rule_hash]);

					if (ptr_config)
						ptr_rule->is_enabled = ptr_config->is_enabled;
				}
			}

			// load apps
			{
				PR_STRING ruleApps;

				ruleApps = _r_obj_createstringbuilder ();

				if (!item.attribute (L"apps").empty ())
					_r_string_append (&ruleApps, item.attribute (L"apps").as_string ());

				if (is_internal && ptr_config && !_r_str_isempty (ptr_config->apps))
				{
					if (!_r_str_isempty (ruleApps))
					{
						_r_string_appendformat (&ruleApps, L"%s%s", DIVIDER_APP, ptr_config->apps->Buffer);
					}
					else
					{
						_r_string_append2 (&ruleApps, ptr_config->apps);
					}
				}

				if (ptr_config)
					_r_obj_dereference (ptr_config);

				if (!_r_str_isempty (ruleApps))
				{
					if (version < XML_PROFILE_VER_3)
						_r_str_replacechar (ruleApps->Buffer, DIVIDER_RULE[0], DIVIDER_APP[0]); // for compat with old profiles

					R_STRINGREF remainingPart;
					PR_STRING appPath;
					PR_STRING expandedPath;
					SIZE_T app_hash;

					_r_stringref_initialize2 (&remainingPart, ruleApps);

					while (remainingPart.Length != 0)
					{
						appPath = _r_str_splitatchar (&remainingPart, &remainingPart, DIVIDER_APP[0]);

						if (appPath)
						{
							expandedPath = _r_str_expandenvironmentstring (appPath);

							if (expandedPath)
								_r_obj_movereference (&appPath, expandedPath);

							if (appPath)
							{
								app_hash = _r_str_hash (appPath);

								if (app_hash)
								{
									if (item.attribute (L"is_services").as_bool () && (app_hash == config.ntoskrnl_hash || app_hash == config.svchost_hash))
									{
										_r_obj_dereference (appPath);
										continue;
									}

									if (!_app_isappfound (app_hash))
										app_hash = _app_addapplication (NULL, _r_obj_getstring (appPath), 0, 0, 0, FALSE, FALSE);

									if (ptr_rule->type == DataRuleSystem)
										_app_setappinfo (app_hash, InfoIsUndeletable, TRUE);

									ptr_rule->apps->emplace (app_hash, TRUE);
								}

								_r_obj_dereference (appPath);
							}
						}
					}
				}

				_r_obj_dereference (ruleApps);
			}

			rules_arr.emplace_back (ptr_rule);
		}
		else if (type == DataRulesConfig)
		{
			LPCWSTR rule_name = item.attribute (L"name").as_string ();

			if (_r_str_isempty (rule_name))
				continue;

			SIZE_T rule_hash = _r_str_hash (rule_name);

			if (rule_hash && rules_config.find (rule_hash) == rules_config.end ())
			{
				PITEM_RULE_CONFIG ptr_config = (PITEM_RULE_CONFIG)_r_obj_allocateex (sizeof (ITEM_RULE_CONFIG), &_app_dereferenceruleconfig);

				ptr_config->is_enabled = item.attribute (L"is_enabled").as_bool ();

				ptr_config->name = _r_obj_createstring (rule_name);
				ptr_config->apps = _r_obj_createstring (item.attribute (L"apps").as_string ());

				if (version < XML_PROFILE_VER_3)
					_r_str_replacechar (ptr_config->apps->Buffer, DIVIDER_RULE[0], DIVIDER_APP[0]); // for compat with old profiles

				rules_config.emplace (rule_hash, ptr_config);
			}
		}
	}
}

VOID _app_profile_load_internal (LPCWSTR path, LPCWSTR path_backup, time_t* ptimestamp)
{
	pugi::xml_document doc_original;
	pugi::xml_document doc_backup;

	pugi::xml_parse_result load_original = doc_original.load_file (path, PUGIXML_LOAD_FLAGS, PUGIXML_LOAD_ENCODING);
	pugi::xml_parse_result load_backup;

	pugi::xml_node root;

	// if file not found or parsing error, load from backup
	if (path_backup)
	{
		DWORD size = 0;
		PVOID pbuffer = _r_loadresource (NULL, path_backup, RT_RCDATA, &size);

		if (pbuffer)
			load_backup = doc_backup.load_buffer (pbuffer, size, PUGIXML_LOAD_FLAGS, PUGIXML_LOAD_ENCODING);

		if (load_backup)
		{
			if (!load_original || doc_backup.child (L"root").attribute (L"timestamp").as_llong () > doc_original.child (L"root").attribute (L"timestamp").as_llong ())
			{
				root = doc_backup.child (L"root");
				load_original = load_backup;
			}
			else
			{
				root = doc_original.child (L"root");
			}
		}
	}

	// show only syntax, memory and i/o errors...
	if (!load_original && load_original.status != pugi::status_file_not_found && load_original.status != pugi::status_no_document_element)
		_r_logerror_v (UID, L"pugi::load_file", 0, L"status: %d,offset: %" TEXT (PR_PTRDIFF) L",text: %hs,file: %s", load_original.status, load_original.offset, load_original.description (), path);

	if (load_original && root)
	{
		if (_app_profile_load_check_node (root, XmlProfileInternalV3, TRUE))
		{
			UINT version = root.attribute (L"version").as_uint ();

			if (ptimestamp)
				*ptimestamp = root.attribute (L"timestamp").as_llong ();

			pugi::xml_node root_rules_system = root.child (L"rules_system");

			if (root_rules_system)
				_app_profile_load_helper (root_rules_system, DataRuleSystem, version);

			pugi::xml_node root_rules_blocklist = root.child (L"rules_blocklist");

			if (root_rules_blocklist)
				_app_profile_load_helper (root_rules_blocklist, DataRuleBlocklist, version);
		}
	}
}

VOID _app_profile_load (HWND hwnd, LPCWSTR path_custom)
{
	INT current_listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);
	INT selected_item = (INT)SendDlgItemMessage (hwnd, current_listview_id, LVM_GETNEXTITEM, (WPARAM)INVALID_INT, LVNI_SELECTED);
	INT scroll_pos = GetScrollPos (GetDlgItem (hwnd, current_listview_id), SB_VERT);

	// clean listview
	if (hwnd)
	{
		for (INT i = IDC_APPS_PROFILE; i <= IDC_RULES_CUSTOM; i++)
			_r_listview_deleteallitems (hwnd, i);
	}

	_r_fastlock_acquireexclusive (&lock_apply);

	// clear apps
	_app_freeapps_map (&apps);

	// clear services/uwp apps
	_app_freeappshelper_map (&apps_helper);

	// clear rules config
	_app_freerulesconfig_map (&rules_config);

	// clear rules
	_app_freerules_vec (&rules_arr);

	// generate uwp apps list (win8+)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
		_app_generate_packages ();

	// generate services list
	_app_generate_services ();

	_r_fastlock_releaseexclusive (&lock_apply);

	// load profile
	if (path_custom || _r_fs_exists (config.profile_path) || _r_fs_exists (config.profile_path_backup))
	{
		pugi::xml_document doc;
		pugi::xml_parse_result result = doc.load_file (path_custom ? path_custom : config.profile_path, PUGIXML_LOAD_FLAGS, PUGIXML_LOAD_ENCODING);

		// load backup
		if (!result)
			result = doc.load_file (config.profile_path_backup, PUGIXML_LOAD_FLAGS, PUGIXML_LOAD_ENCODING);

		if (!result)
		{
			// show only syntax, memory and i/o errors...
			if (result.status != pugi::status_file_not_found && result.status != pugi::status_no_document_element)
				_r_logerror_v (UID, L"pugi::load_file", 0, L"status: %d,offset: %" TEXT (PR_PTRDIFF) L",text: %hs,file: %s", result.status, result.offset, result.description (), !_r_str_isempty (path_custom) ? path_custom : config.profile_path);
		}
		else
		{
			pugi::xml_node root = doc.child (L"root");

			if (root)
			{
				UINT version = root.attribute (L"version").as_uint ();

				if (_app_profile_load_check_node (root, XmlProfileV3, TRUE))
				{
					// load apps (new!)
					pugi::xml_node root_apps = root.child (L"apps");

					if (root_apps)
						_app_profile_load_helper (root_apps, DataAppRegular, version);

					// load rules config (new!)
					pugi::xml_node root_rules_config = root.child (L"rules_config");

					if (root_rules_config)
						_app_profile_load_helper (root_rules_config, DataRulesConfig, version);

					// load user rules (new!)
					pugi::xml_node root_rules_custom = root.child (L"rules_custom");

					if (root_rules_custom)
						_app_profile_load_helper (root_rules_custom, DataRuleCustom, version);
				}
			}
		}
	}

	_app_profile_load_fallback ();

	// load internal rules (new!)
	if (!_r_config_getboolean (L"IsInternalRulesDisabled", FALSE))
		_app_profile_load_internal (config.profile_internal_path, MAKEINTRESOURCE (IDR_PROFILE_INTERNAL), &config.profile_internal_timestamp);

	if (hwnd)
	{
		time_t current_time = _r_unixtime_now ();

		// add apps
		for (auto it = apps.begin (); it != apps.end (); ++it)
		{
			if (!it->second)
				continue;

			SIZE_T app_hash = it->first;
			PITEM_APP ptr_app = (PITEM_APP)_r_obj_reference (it->second);

			INT listview_id = _app_getlistview_id (ptr_app->type);

			if (listview_id)
			{
				_r_fastlock_acquireshared (&lock_checkbox);

				_r_listview_additemex (hwnd, listview_id, 0, 0, _r_obj_getstringordefault (ptr_app->display_name, SZ_EMPTY), ptr_app->icon_id, _app_getappgroup (app_hash, ptr_app), app_hash);
				_app_setappiteminfo (hwnd, listview_id, 0, app_hash, ptr_app);

				_r_fastlock_releaseshared (&lock_checkbox);
			}

			// install timer
			if (ptr_app->timer)
				_app_timer_set (hwnd, ptr_app, ptr_app->timer - current_time);

			_r_obj_dereference (ptr_app);
		}

		// add services and store apps
		for (auto it = apps_helper.begin (); it != apps_helper.end (); ++it)
		{
			if (!it->second)
				continue;

			PITEM_APP_HELPER ptr_app_item = (PITEM_APP_HELPER)_r_obj_reference (it->second);

			if (_r_str_isempty (ptr_app_item->internal_name))
			{
				_r_obj_dereference (ptr_app_item);
				continue;
			}

			_app_addapplication (hwnd, ptr_app_item->internal_name->Buffer, ptr_app_item->timestamp, 0, 0, FALSE, FALSE);

			_r_obj_dereference (ptr_app_item);
		}

		// add rules
		for (SIZE_T i = 0; i < rules_arr.size (); i++)
		{
			PVOID pdata = rules_arr.at (i);

			if (!pdata)
				continue;

			PITEM_RULE ptr_rule = (PITEM_RULE)_r_obj_reference (pdata);

			INT listview_id = _app_getlistview_id (ptr_rule->type);

			if (listview_id)
			{
				_r_fastlock_acquireshared (&lock_checkbox);

				_r_listview_additemex (hwnd, listview_id, 0, 0, _r_obj_getstringordefault (ptr_rule->name, SZ_EMPTY), _app_getruleicon (ptr_rule), _app_getrulegroup (ptr_rule), i);
				_app_setruleiteminfo (hwnd, listview_id, 0, ptr_rule, FALSE);

				_r_fastlock_releaseshared (&lock_checkbox);
			}

			_r_obj_dereference (ptr_rule);
		}
	}

	if (hwnd && current_listview_id)
	{
		INT new_listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

		if (new_listview_id)
		{
			_app_listviewsort (hwnd, new_listview_id, INVALID_INT, FALSE);

			if (current_listview_id == new_listview_id)
				_app_showitem (hwnd, current_listview_id, selected_item, scroll_pos);
		}
	}
}

VOID _app_profile_save ()
{
	time_t current_time = _r_unixtime_now ();
	BOOLEAN is_backuprequired = _r_config_getboolean (L"IsBackupProfile", TRUE) && (!_r_fs_exists (config.profile_path_backup) || ((current_time - _r_config_getlong64 (L"BackupTimestamp", 0)) >= _r_config_getlong64 (L"BackupPeriod", BACKUP_HOURS_PERIOD)));

	pugi::xml_document doc;
	pugi::xml_node root = doc.append_child (L"root");

	if (root)
	{
		root.append_attribute (L"timestamp").set_value (current_time);
		root.append_attribute (L"type").set_value (XmlProfileV3);
		root.append_attribute (L"version").set_value (XML_PROFILE_VER_CURRENT);

		pugi::xml_node root_apps = root.append_child (L"apps");
		pugi::xml_node root_rules_custom = root.append_child (L"rules_custom");
		pugi::xml_node root_rule_config = root.append_child (L"rules_config");

		// save apps
		if (root_apps)
		{
			for (auto it = apps.begin (); it != apps.end (); ++it)
			{
				if (!it->second)
					continue;

				PITEM_APP ptr_app = (PITEM_APP)_r_obj_reference (it->second);

				if (_r_str_isempty (ptr_app->original_path) || (!_app_isappused (ptr_app, it->first) && (ptr_app->type == DataAppService || ptr_app->type == DataAppUWP)))
				{
					_r_obj_dereference (ptr_app);
					continue;
				}

				pugi::xml_node item = root_apps.append_child (L"item");

				if (item)
				{
					item.append_attribute (L"path").set_value (ptr_app->original_path->Buffer);

					if (ptr_app->timestamp)
						item.append_attribute (L"timestamp").set_value (ptr_app->timestamp);

					// set timer (if presented)
					if (ptr_app->timer && _app_istimeractive (ptr_app))
						item.append_attribute (L"timer").set_value (ptr_app->timer);

					// ffu!
					if (ptr_app->profile)
						item.append_attribute (L"profile").set_value (ptr_app->profile);

					if (ptr_app->is_silent)
						item.append_attribute (L"is_silent").set_value (ptr_app->is_silent);

					if (ptr_app->is_enabled)
						item.append_attribute (L"is_enabled").set_value (ptr_app->is_enabled);
				}

				_r_obj_dereference (ptr_app);
			}
		}

		// save user rules
		if (root_rules_custom)
		{
			for (auto it = rules_arr.begin (); it != rules_arr.end (); ++it)
			{
				if (!*it)
					continue;

				PITEM_RULE ptr_rule = (PITEM_RULE)_r_obj_reference (*it);

				if (ptr_rule->is_readonly || _r_str_isempty (ptr_rule->name))
				{
					_r_obj_dereference (ptr_rule);
					continue;
				}

				pugi::xml_node item = root_rules_custom.append_child (L"item");

				if (item)
				{
					item.append_attribute (L"name").set_value (ptr_rule->name->Buffer);

					if (!_r_str_isempty (ptr_rule->rule_remote))
						item.append_attribute (L"rule").set_value (ptr_rule->rule_remote->Buffer);

					if (!_r_str_isempty (ptr_rule->rule_local))
						item.append_attribute (L"rule_local").set_value (ptr_rule->rule_local->Buffer);

					// ffu!
					if (ptr_rule->profile)
						item.append_attribute (L"profile").set_value (ptr_rule->profile);

					if (ptr_rule->direction != FWP_DIRECTION_OUTBOUND)
						item.append_attribute (L"dir").set_value (ptr_rule->direction);

					if (ptr_rule->protocol != 0)
						item.append_attribute (L"protocol").set_value (ptr_rule->protocol);

					if (ptr_rule->af != AF_UNSPEC)
						item.append_attribute (L"version").set_value (ptr_rule->af);

					// add apps attribute
					if (!ptr_rule->apps->empty ())
					{
						PR_STRING rule_apps = _app_rulesexpandapps (ptr_rule, FALSE, DIVIDER_APP);

						if (rule_apps)
						{
							if (!_r_str_isempty (rule_apps))
								item.append_attribute (L"apps").set_value (rule_apps->Buffer);

							_r_obj_dereference (rule_apps);
						}
					}

					if (ptr_rule->is_block)
						item.append_attribute (L"is_block").set_value (ptr_rule->is_block);

					if (ptr_rule->is_enabled)
						item.append_attribute (L"is_enabled").set_value (ptr_rule->is_enabled);
				}

				_r_obj_dereference (ptr_rule);
			}
		}

		// save rules config
		if (root_rule_config)
		{
			for (auto it = rules_config.begin (); it != rules_config.end (); ++it)
			{
				if (!it->second)
					continue;

				PITEM_RULE_CONFIG ptr_config = (PITEM_RULE_CONFIG)_r_obj_reference (it->second);

				if (_r_str_isempty (ptr_config->name))
				{
					_r_obj_dereference (ptr_config);
					continue;
				}

				PR_STRING rule_apps = NULL;
				BOOLEAN is_enabled_default = ptr_config->is_enabled;

				{
					SIZE_T rule_hash = _r_str_hash (ptr_config->name);
					PITEM_RULE ptr_rule = _app_getrulebyhash (rule_hash);

					if (ptr_rule)
					{
						is_enabled_default = ptr_rule->is_enabled_default;

						if (ptr_rule->type == DataRuleCustom && !ptr_rule->apps->empty ())
						{
							rule_apps = _app_rulesexpandapps (ptr_rule, FALSE, DIVIDER_APP);
						}

						_r_obj_dereference (ptr_rule);
					}
				}

				// skip saving untouched configuration
				if (ptr_config->is_enabled == is_enabled_default && _r_str_isempty (rule_apps))
				{
					if (rule_apps)
						_r_obj_clearreference (&rule_apps);

					_r_obj_dereference (ptr_config);

					continue;
				}

				pugi::xml_node item = root_rule_config.append_child (L"item");

				if (item)
				{
					item.append_attribute (L"name").set_value (ptr_config->name->Buffer);

					if (rule_apps)
					{
						if (!_r_str_isempty (rule_apps))
							item.append_attribute (L"apps").set_value (rule_apps->Buffer);

						_r_obj_clearreference (&rule_apps);
					}

					item.append_attribute (L"is_enabled").set_value (ptr_config->is_enabled);
				}

				_r_obj_dereference (ptr_config);
			}
		}

		doc.save_file (config.profile_path, L"\t", PUGIXML_SAVE_FLAGS, PUGIXML_SAVE_ENCODING);

		// make backup
		if (is_backuprequired)
		{
			doc.save_file (config.profile_path_backup, L"\t", PUGIXML_SAVE_FLAGS, PUGIXML_SAVE_ENCODING);
			_r_config_setlong64 (L"BackupTimestamp", current_time);
		}
	}
}
