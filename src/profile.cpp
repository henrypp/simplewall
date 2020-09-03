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

	result = _app_getappinfo (ptr_app, info_key);

	_r_obj_dereference (ptr_app);

	return result;
}

LONG_PTR _app_getappinfo (PITEM_APP ptr_app, ENUM_INFO_DATA info_key)
{
	if (info_key == InfoPath)
	{
		if (!_r_str_isempty (ptr_app->real_path))
			return (LONG_PTR)_r_obj_reference (ptr_app->real_path);
	}
	else if (info_key == InfoName)
	{
		if (!_r_str_isempty (ptr_app->display_name))
			return (LONG_PTR)_r_obj_reference (ptr_app->display_name);

		else if (!_r_str_isempty (ptr_app->original_path))
			return (LONG_PTR)_r_obj_reference (ptr_app->original_path);
	}
	else if (info_key == InfoTimestampPtr)
	{
		return (LONG_PTR)&ptr_app->timestamp;
	}
	else if (info_key == InfoTimerPtr)
	{
		return (LONG_PTR)&ptr_app->timer;
	}
	else if (info_key == InfoIconId)
	{
		if (ptr_app->icon_id)
			return (LONG_PTR)ptr_app->icon_id;

		return (LONG_PTR)config.icon_id;
	}
	else if (info_key == InfoListviewId)
	{
		return (LONG_PTR)_app_getlistview_id (ptr_app->type);
	}
	else if (info_key == InfoIsSilent)
	{
		return (LONG_PTR)(ptr_app->is_silent ? TRUE : FALSE);
	}
	else if (info_key == InfoIsEnabled)
	{
		return (LONG_PTR)(ptr_app->is_enabled ? TRUE : FALSE);
	}
	else if (info_key == InfoIsUndeletable)
	{
		return (LONG_PTR)(ptr_app->is_undeletable ? TRUE : FALSE);
	}

	return 0;
}

VOID _app_setappinfo (SIZE_T app_hash, ENUM_INFO_DATA info_key, LONG_PTR info_value)
{
	PITEM_APP ptr_app = _app_getappitem (app_hash);

	if (!ptr_app)
		return;

	_app_setappinfo (ptr_app, info_key, info_value);

	_r_obj_dereference (ptr_app);
}

VOID _app_setappinfo (PITEM_APP ptr_app, ENUM_INFO_DATA info_key, LONG_PTR info_value)
{
	if (info_key == InfoTimestampPtr)
	{
		ptr_app->timestamp = *((time_t*)info_value);
	}
	else if (info_key == InfoTimerPtr)
	{
		time_t timestamp = *((time_t*)info_value);

		// check timer expiration
		if (timestamp <= _r_unixtime_now ())
		{
			ptr_app->timer = 0;
			ptr_app->is_enabled = FALSE;
		}
		else
		{
			ptr_app->timer = timestamp;
		}
	}
	else if (info_key == InfoIsSilent)
	{
		ptr_app->is_silent = info_value ? TRUE : FALSE;
	}
	else if (info_key == InfoIsEnabled)
	{
		ptr_app->is_enabled = info_value ? TRUE : FALSE;
	}
	else if (info_key == InfoIsUndeletable)
	{
		ptr_app->is_undeletable = info_value ? TRUE : FALSE;
	}
}

SIZE_T _app_addapplication (HWND hwnd, ENUM_TYPE_DATA type, LPCWSTR path, PR_STRING display_name, PR_STRING real_path)
{
	if (_r_str_isempty (path) || PathIsDirectory (path))
		return 0;

	WCHAR path_full[1024];
	PITEM_APP ptr_app;
	PR_STRING signatureString;
	SIZE_T path_length;
	SIZE_T app_hash;
	BOOLEAN is_ntoskrnl;

	path_length = _r_str_length (path);

	// prevent possible duplicate apps entries with short path (issue #640)
	if (_r_str_findchar (path, path_length, L'~') != INVALID_SIZE_T)
	{
		if (GetLongPathName (path, path_full, RTL_NUMBER_OF (path_full)))
		{
			path = path_full;
			path_length = _r_str_length (path_full);
		}
	}

	app_hash = _r_str_hash (path);

	if (_app_isappfound (app_hash))
		return app_hash; // already exists

	ptr_app = (PITEM_APP)_r_obj_allocateex (sizeof (ITEM_APP), &_app_dereferenceapp);
	is_ntoskrnl = (app_hash == config.ntoskrnl_hash);

	if (type == DataAppService || type == DataAppUWP)
	{
		ptr_app->type = type;

		if (display_name)
			ptr_app->display_name = _r_obj_reference (display_name);

		if (real_path)
			ptr_app->real_path = _r_obj_reference (real_path);
	}
	else if (_r_str_compare_length (path, L"\\device\\", 8) == 0) // device path
	{
		ptr_app->type = DataAppDevice;
		ptr_app->real_path = _r_obj_createstring (path);
	}
	else
	{
		if (!is_ntoskrnl && _r_str_findchar (path, path_length, OBJ_NAME_PATH_SEPARATOR) == INVALID_SIZE_T)
		{
			ptr_app->type = DataAppPico;
		}
		else
		{
			ptr_app->type = PathIsNetworkPath (path) ? DataAppNetwork : DataAppRegular;
		}

		ptr_app->real_path = is_ntoskrnl ? _r_obj_createstring2 (config.ntoskrnl_path) : _r_obj_createstring (path);
	}

	ptr_app->original_path = _r_obj_createstringex (path, path_length * sizeof (WCHAR));

	// fix "System" lowercase
	if (is_ntoskrnl)
	{
		_r_str_tolower (ptr_app->original_path);
		ptr_app->original_path->Buffer[0] = _r_str_upper (ptr_app->original_path->Buffer[0]);
	}

	if (ptr_app->type == DataAppRegular || ptr_app->type == DataAppDevice || ptr_app->type == DataAppNetwork)
		ptr_app->short_name = _r_obj_createstring (_r_path_getbasename (path));

	// get signature information
	if (_r_config_getboolean (L"IsCertificatesEnabled", FALSE))
	{
		signatureString = _app_getsignatureinfo (app_hash, ptr_app);

		if (signatureString)
			_r_obj_dereference (signatureString);
	}

	ptr_app->guids = new GUIDS_VEC; // initialize stl

	ptr_app->timestamp = _r_unixtime_now ();

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

			_r_listview_additemex (hwnd, listview_id, 0, 0, SZ_EMPTY, ptr_app->icon_id, _app_getappgroup (app_hash, ptr_app), app_hash);
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
		return (PITEM_APP)_r_obj_referencesafe (apps.at (app_hash));
	}

	return NULL;
}

PITEM_RULE _app_getrulebyid (SIZE_T idx)
{
	if (idx != INVALID_SIZE_T && idx < rules_arr.size ())
	{
		return (PITEM_RULE)_r_obj_referencesafe (rules_arr.at (idx));
	}

	return NULL;
}

PITEM_RULE _app_getrulebyhash (SIZE_T rule_hash)
{
	if (!rule_hash)
		return NULL;

	for (auto it = rules_arr.begin (); it != rules_arr.end (); ++it)
	{
		PITEM_RULE ptr_rule = (PITEM_RULE)_r_obj_referencesafe (*it);

		if (ptr_rule)
		{
			if (ptr_rule->is_readonly)
			{
				if (_r_str_hash (ptr_rule->name) == rule_hash)
					return ptr_rule;
			}

			_r_obj_dereference (ptr_rule);
		}
	}

	return NULL;
}

PITEM_NETWORK _app_getnetworkitem (SIZE_T network_hash)
{
	if (network_map.find (network_hash) != network_map.end ())
	{
		return (PITEM_NETWORK)_r_obj_referencesafe (network_map.at (network_hash));
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
		return (PITEM_LOG)_r_obj_referencesafe (log_arr.at (idx));
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

COLORREF _app_getappcolor (INT listview_id, SIZE_T app_hash, BOOLEAN is_systemapp, BOOLEAN is_validconnection)
{
	PITEM_APP ptr_app = _app_getappitem (app_hash);
	LPCWSTR colorValue = NULL;
	BOOLEAN is_profilelist = (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_RULES_CUSTOM);

	if (ptr_app)
	{
		if (_r_config_getboolean (L"IsHighlightInvalid", TRUE, L"colors") && !_app_isappexists (ptr_app))
		{
			colorValue = L"ColorInvalid";
			goto CleanupExit;
		}

		if (_r_config_getboolean (L"IsHighlightTimer", TRUE, L"colors") && _app_istimeractive (ptr_app))
		{
			colorValue = L"ColorTimer";
			goto CleanupExit;
		}
	}

	if (_r_config_getboolean (L"IsHighlightConnection", TRUE, L"colors") && is_validconnection)
	{
		colorValue = L"ColorConnection";
		goto CleanupExit;
	}

	if (ptr_app)
	{
		if (_r_config_getboolean (L"IsHighlightSigned", TRUE, L"colors") && !ptr_app->is_silent && _r_config_getboolean (L"IsCertificatesEnabled", FALSE) && ptr_app->is_signed)
		{
			colorValue = L"ColorSigned";
			goto CleanupExit;
		}

		if (!is_profilelist && (_r_config_getboolean (L"IsHighlightSpecial", TRUE, L"colors") && _app_isapphaverule (app_hash)))
		{
			colorValue = L"ColorSpecial";
			goto CleanupExit;
		}

		if (is_profilelist && _r_config_getboolean (L"IsHighlightSilent", TRUE, L"colors") && ptr_app->is_silent)
		{
			colorValue = L"ColorSilent";
			goto CleanupExit;
		}

		if (_r_config_getboolean (L"IsHighlightPico", TRUE, L"colors") && ptr_app->type == DataAppPico)
		{
			colorValue = L"ColorPico";
			goto CleanupExit;
		}
	}

	if (_r_config_getboolean (L"IsHighlightSystem", TRUE, L"colors") && is_systemapp)
	{
		colorValue = L"ColorSystem";
		goto CleanupExit;
	}

	if (ptr_app)
		_r_obj_dereference (ptr_app);

CleanupExit:

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

		PITEM_RULE ptr_rule = (PITEM_RULE)_r_obj_referencesafe (*it);

		if (ptr_rule)
		{
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
		ptr_app = (PITEM_APP)_r_obj_referencesafe (it->second);

		if (!ptr_app)
			continue;

		app_hash = it->first;
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
		ptr_rule = (PITEM_RULE)_r_obj_referencesafe (*it);

		if (ptr_rule)
		{
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

	else if (_r_config_getboolean (L"IsHighlightSpecial", TRUE, L"colors") && ptr_rule->type == DataRuleCustom && !ptr_rule->apps->empty ())
		colorValue = L"ColorSpecial";

	_r_obj_dereference (ptr_rule);

	if (colorValue)
		return _app_getcolorvalue (_r_str_hash (colorValue));

	return 0;
}

PR_STRING _app_gettooltip (HWND hwnd, INT listview_id, INT item_id)
{
	R_STRINGBUILDER string = {0};

	BOOLEAN is_appslist = (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP);
	BOOLEAN is_ruleslist = (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM);

	if (is_appslist || listview_id == IDC_RULE_APPS_ID)
	{
		SIZE_T app_hash = _r_listview_getitemlparam (hwnd, listview_id, item_id);
		PITEM_APP ptr_app = _app_getappitem (app_hash);

		if (ptr_app)
		{
			_r_obj_createstringbuilder (&string);

			// app path
			{
				PR_STRING pathString = _r_format_string (L"%s\r\n", _r_obj_getstring (!_r_str_isempty (ptr_app->real_path) ? ptr_app->real_path : (!_r_str_isempty (ptr_app->display_name) ? ptr_app->display_name : ptr_app->original_path)));

				_r_string_append2 (&string, pathString);

				_r_obj_dereference (pathString);
			}

			// app information
			{
				R_STRINGBUILDER infoString;

				_r_obj_createstringbuilder (&infoString);

				if (ptr_app->type == DataAppRegular)
				{
					PR_STRING versionString = _app_getversioninfo (app_hash, ptr_app);

					if (versionString)
					{
						if (!_r_str_isempty (versionString))
							_r_string_appendformat (&infoString, SZ_TAB L"%s\r\n", versionString->Buffer);

						_r_obj_dereference (versionString);
					}
				}
				else if (ptr_app->type == DataAppService)
				{
					_r_string_appendformat (&infoString, SZ_TAB L"%s" SZ_TAB_CRLF L"%s\r\n", _r_obj_getstringorempty (ptr_app->original_path), _r_obj_getstringorempty (ptr_app->display_name));
				}
				else if (ptr_app->type == DataAppUWP)
				{
					_r_string_appendformat (&infoString, SZ_TAB L"%s\r\n", _r_obj_getstringorempty (ptr_app->display_name));
				}

				// signature information
				if (_r_config_getboolean (L"IsCertificatesEnabled", FALSE) && ptr_app->is_signed)
				{
					PR_STRING signatureString = _app_getsignatureinfo (app_hash, ptr_app);

					if (signatureString)
					{
						if (!_r_str_isempty (signatureString))
							_r_string_appendformat (&infoString, SZ_TAB L"%s: %s\r\n", _r_locale_getstring (IDS_SIGNATURE), signatureString->Buffer);

						_r_obj_dereference (signatureString);
					}
				}

				if (!_r_str_isempty (&infoString))
				{
					_r_string_insertformat (&infoString, 0, L"%s:\r\n", _r_locale_getstring (IDS_FILE));
					_r_string_append2 (&string, _r_obj_finalstringbuilder (&infoString));
				}

				_r_obj_deletestringbuilder (&infoString);
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
				PR_STRING appRulesString = _app_appexpandrules (app_hash, SZ_TAB_CRLF);

				if (appRulesString)
				{
					if (!_r_str_isempty (appRulesString))
						_r_string_appendformat (&string, L"%s:" SZ_TAB_CRLF L"%s\r\n", _r_locale_getstring (IDS_RULE), appRulesString->Buffer);

					_r_obj_dereference (appRulesString);
				}
			}

			// app notes
			{
				R_STRINGBUILDER notesString;

				_r_obj_createstringbuilder (&notesString);

				// app type
				if (ptr_app->type == DataAppNetwork)
					_r_string_appendformat (&notesString, SZ_TAB L"%s\r\n", _r_locale_getstring (IDS_HIGHLIGHT_NETWORK));

				else if (ptr_app->type == DataAppPico)
					_r_string_appendformat (&notesString, SZ_TAB L"%s\r\n", _r_locale_getstring (IDS_HIGHLIGHT_PICO));

				// app settings
				if (_app_isappfromsystem (_r_obj_getstring (ptr_app->real_path), app_hash))
					_r_string_appendformat (&notesString, SZ_TAB L"%s\r\n", _r_locale_getstring (IDS_HIGHLIGHT_SYSTEM));

				if (_app_isapphaveconnection (app_hash))
					_r_string_appendformat (&notesString, SZ_TAB L"%s\r\n", _r_locale_getstring (IDS_HIGHLIGHT_CONNECTION));

				if (is_appslist && ptr_app->is_silent)
					_r_string_appendformat (&notesString, SZ_TAB L"%s\r\n", _r_locale_getstring (IDS_HIGHLIGHT_SILENT));

				if (!_app_isappexists (ptr_app))
					_r_string_appendformat (&notesString, SZ_TAB L"%s\r\n", _r_locale_getstring (IDS_HIGHLIGHT_INVALID));

				if (!_r_str_isempty (&notesString))
				{
					_r_string_insertformat (&notesString, 0, L"%s:\r\n", _r_locale_getstring (IDS_NOTES));
					_r_string_append2 (&string, _r_obj_finalstringbuilder (&notesString));
				}

				_r_obj_deletestringbuilder (&notesString);
			}

			_r_obj_dereference (ptr_app);

			if (!_r_str_isempty (&string))
				return _r_obj_finalstringbuilder (&string);

			_r_obj_deletestringbuilder (&string);
		}
	}
	else if (is_ruleslist)
	{
		LPARAM lparam = _r_listview_getitemlparam (hwnd, listview_id, item_id);
		PITEM_RULE ptr_rule = _app_getrulebyid (lparam);

		if (ptr_rule)
		{
			_r_obj_createstringbuilder (&string);

			LPCWSTR empty;

			PR_STRING infoString;
			PR_STRING ruleRemoteString = _app_rulesexpandrules (ptr_rule->rule_remote, L"\r\n" SZ_TAB);
			PR_STRING ruleLocalString = _app_rulesexpandrules (ptr_rule->rule_local, L"\r\n" SZ_TAB);

			empty = _r_locale_getstring (IDS_STATUS_EMPTY);

			// rule information
			infoString = _r_format_string (L"%s (#%" TEXT (PR_SIZE_T) L")\r\n%s (" SZ_DIRECTION_REMOTE L"):\r\n%s%s\r\n%s (" SZ_DIRECTION_LOCAL L"):\r\n%s%s",
										   _r_obj_getstringordefault (ptr_rule->name, empty),
										   lparam,
										   _r_locale_getstring (IDS_RULE),
										   SZ_TAB,
										   _r_obj_getstringordefault (ruleRemoteString, empty),
										   _r_locale_getstring (IDS_RULE),
										   SZ_TAB,
										   _r_obj_getstringordefault (ruleLocalString, empty)
			);

			_r_string_append2 (&string, infoString);

			SAFE_DELETE_REFERENCE (infoString);
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

			if (!_r_str_isempty (&string))
				return _r_obj_finalstringbuilder (&string);

			_r_obj_deletestringbuilder (&string);
		}
	}
	else if (listview_id == IDC_NETWORK)
	{
		LPARAM lparam = _r_listview_getitemlparam (hwnd, listview_id, item_id);
		PITEM_NETWORK ptr_network = _app_getnetworkitem (lparam);

		if (ptr_network)
		{
			LPCWSTR empty;

			PR_STRING infoString;
			PR_STRING localAddressString;
			PR_STRING remoteAddressString;

			empty = _r_locale_getstring (IDS_STATUS_EMPTY);

			localAddressString = _app_formataddress (ptr_network->af, 0, &ptr_network->local_addr, ptr_network->local_port, 0);
			remoteAddressString = _app_formataddress (ptr_network->af, 0, &ptr_network->remote_addr, ptr_network->remote_port, 0);

			infoString = _r_format_string (L"%s\r\n%s (" SZ_DIRECTION_LOCAL L"):\r\n" SZ_TAB L"%s\r\n%s (" SZ_DIRECTION_REMOTE L"):\r\n" SZ_TAB L"%s\r\n%s:\r\n" SZ_TAB L"%s\r\n%s:\r\n" SZ_TAB L"%s",
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

			return infoString;
		}

	}
	else if (listview_id == IDC_LOG)
	{
		LPARAM lparam = _r_listview_getitemlparam (hwnd, listview_id, item_id);
		PITEM_LOG ptr_log = _app_getlogitem (lparam);

		if (ptr_log)
		{
			WCHAR dateString[256];

			LPCWSTR empty;

			PR_STRING infoString;
			PR_STRING localAddressString;
			PR_STRING remoteAddressString;
			PR_STRING directionString;

			_r_format_dateex (dateString, RTL_NUMBER_OF (dateString), ptr_log->timestamp, FDTF_LONGDATE | FDTF_LONGTIME);

			empty = _r_locale_getstring (IDS_STATUS_EMPTY);

			localAddressString = _app_formataddress (ptr_log->af, 0, &ptr_log->local_addr, ptr_log->local_port, 0);
			remoteAddressString = _app_formataddress (ptr_log->af, 0, &ptr_log->remote_addr, ptr_log->remote_port, 0);
			directionString = _app_getdirectionname (ptr_log->direction, ptr_log->is_loopback, TRUE);

			infoString = _r_format_string (L"%s\r\n%s:\r\n" SZ_TAB L"%s\r\n%s (" SZ_DIRECTION_LOCAL L"):\r\n" SZ_TAB L"%s\r\n%s (" SZ_DIRECTION_REMOTE L"):\r\n" SZ_TAB L"%s\r\n%s:\r\n" SZ_TAB L"%s\r\n%s:\r\n" SZ_TAB L"%s\r\n%s:\r\n" SZ_TAB L"%s\r\n%s:\r\n" SZ_TAB L"%s",
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

			return infoString;
		}
	}
	else if (listview_id == IDC_RULE_REMOTE_ID || listview_id == IDC_RULE_LOCAL_ID)
	{
		return _r_listview_getitemtext (hwnd, listview_id, item_id, 0);
	}

	return NULL;
}

VOID _app_setappiteminfo (HWND hwnd, INT listview_id, INT item, SIZE_T app_hash, PITEM_APP ptr_app)
{
	if (!listview_id || item == INVALID_INT)
		return;

	_app_getappicon (ptr_app, TRUE, &ptr_app->icon_id, NULL);

	_r_listview_setitemex (hwnd, listview_id, item, 0, _app_getdisplayname (app_hash, ptr_app, FALSE), ptr_app->icon_id, _app_getappgroup (app_hash, ptr_app), 0);

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

VOID _app_ruleenable (PITEM_RULE ptr_rule, BOOLEAN is_enable, BOOLEAN is_createconfig)
{
	ptr_rule->is_enabled = is_enable;

	if (ptr_rule->is_readonly && !_r_str_isempty (ptr_rule->name))
	{
		PITEM_RULE_CONFIG ptr_config;
		SIZE_T rule_hash = _r_str_hash (ptr_rule->name);

		if (rule_hash)
		{
			if (rules_config.find (rule_hash) != rules_config.end ())
			{
				ptr_config = (PITEM_RULE_CONFIG)_r_obj_referencesafe (rules_config.at (rule_hash));

				if (ptr_config)
				{
					ptr_config->is_enabled = is_enable;

					_r_obj_dereference (ptr_config);

					return;
				}
			}

			if (is_createconfig)
			{
				ptr_config = (PITEM_RULE_CONFIG)_r_obj_allocateex (sizeof (ITEM_RULE_CONFIG), &_app_dereferenceruleconfig);

				ptr_config->is_enabled = is_enable;
				ptr_config->name = _r_obj_createstring2 (ptr_rule->name);

				rules_config.insert_or_assign (rule_hash, ptr_config);
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

		PITEM_RULE ptr_rule = (PITEM_RULE)_r_obj_referencesafe (*it);

		if (!ptr_rule)
			continue;

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
		_app_ruleenable (ptr_rule, ptr_rule->is_enabled, FALSE);

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
			rules.push_back (ptr_rule); // be freed later!
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
			if (_wfp_isfiltersinstalled ())
			{
				HANDLE hengine = _wfp_getenginehandle ();

				if (hengine)
					_wfp_create4filters (hengine, &rules, __LINE__);
			}

			_app_freerules_vec (&rules);
		}

		_app_profile_save (); // required!
	}
}

PR_STRING _app_appexpandrules (SIZE_T app_hash, LPCWSTR delimeter)
{
	R_STRINGBUILDER string;

	_r_obj_createstringbuilder (&string);

	for (auto it = rules_arr.begin (); it != rules_arr.end (); ++it)
	{
		PITEM_RULE ptr_rule = (PITEM_RULE)_r_obj_referencesafe (*it);

		if (!ptr_rule)
			continue;

		if (ptr_rule->is_enabled && ptr_rule->type == DataRuleCustom && ptr_rule->apps->find (app_hash) != ptr_rule->apps->end ())
		{
			if (!_r_str_isempty (ptr_rule->name))
			{
				if (ptr_rule->is_readonly)
				{
					_r_string_append2 (&string, ptr_rule->name);
				}
				else
				{
					_r_string_appendformat (&string, L"%s" SZ_RULE_INTERNAL_MENU, ptr_rule->name->Buffer);
				}

				_r_string_append (&string, delimeter);
			}
		}

		_r_obj_dereference (ptr_rule);
	}

	_r_str_trim (&string, delimeter);

	if (_r_str_isempty (&string))
	{
		_r_obj_deletestringbuilder (&string);
		return NULL;
	}

	return _r_obj_finalstringbuilder (&string);
}

PR_STRING _app_rulesexpandapps (const PITEM_RULE ptr_rule, BOOLEAN is_fordisplay, LPCWSTR delimeter)
{
	R_STRINGBUILDER string;

	_r_obj_createstringbuilder (&string);

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
			if (ptr_app->type == DataAppUWP)
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

	_r_str_trim (&string, delimeter);

	if (_r_str_isempty (&string))
	{
		_r_obj_deletestringbuilder (&string);
		return NULL;
	}

	return _r_obj_finalstringbuilder (&string);
}

PR_STRING _app_rulesexpandrules (PR_STRING rule, LPCWSTR delimeter)
{
	R_STRINGBUILDER string;

	if (_r_str_isempty (rule))
		return NULL;

	_r_obj_createstringbuilder (&string);

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

	_r_str_trim (&string, delimeter);

	if (_r_str_isempty (&string))
	{
		_r_obj_deletestringbuilder (&string);
		return NULL;
	}

	return _r_obj_finalstringbuilder (&string);
}

BOOLEAN _app_isappfound (SIZE_T app_hash)
{
	return app_hash && apps.find (app_hash) != apps.end ();
}

BOOLEAN _app_isappfromsystem (LPCWSTR path, SIZE_T app_hash)
{
	if (!app_hash)
		return FALSE;

	if (app_hash == config.ntoskrnl_hash)
		return TRUE;

	if (path)
	{
		if (_r_str_compare_length (path, config.windows_dir, config.wd_length) == 0)
			return TRUE;

		ULONG attr = GetFileAttributes (path);

		if ((attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_SYSTEM) != 0)
			return TRUE;
	}

	return FALSE;
}

BOOLEAN _app_isapphaveconnection (SIZE_T app_hash)
{
	if (!app_hash)
		return FALSE;

	for (auto it = network_map.begin (); it != network_map.end (); ++it)
	{
		PITEM_NETWORK ptr_network = (PITEM_NETWORK)_r_obj_referencesafe (it->second);

		if (!ptr_network)
			continue;

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
	PITEM_APP ptr_app;
	SIZE_T app_hash;

	for (auto it = apps.begin (); it != apps.end (); ++it)
	{
		ptr_app = (PITEM_APP)_r_obj_referencesafe (it->second);

		if (!ptr_app)
			continue;

		app_hash = it->first;

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
		PITEM_RULE ptr_rule = (PITEM_RULE)_r_obj_referencesafe (*it);

		if (ptr_rule)
		{
			if (ptr_rule->is_enabled && ptr_rule->type == DataRuleCustom && ptr_rule->apps->find (app_hash) != ptr_rule->apps->end ())
			{
				_r_obj_dereference (ptr_rule);
				return TRUE;
			}

			_r_obj_dereference (ptr_rule);
		}
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

	if (ptr_app->type == DataAppRegular)
		return !_r_str_isempty (ptr_app->real_path) && _r_fs_exists (ptr_app->real_path->Buffer);

	if (ptr_app->type == DataAppDevice || ptr_app->type == DataAppNetwork || ptr_app->type == DataAppPico)
		return TRUE;

	if (ptr_app->type == DataAppService || ptr_app->type == DataAppUWP)
		return TRUE;

	return FALSE;
}

BOOLEAN _app_isruletype (LPCWSTR rule, ULONG types)
{
	ULONG code;
	PNET_ADDRESS_INFO addressInfo;

	addressInfo = (PNET_ADDRESS_INFO)_r_mem_allocatezero (sizeof (NET_ADDRESS_INFO));

	// host - NET_STRING_NAMED_ADDRESS | NET_STRING_NAMED_SERVICE;
	// ip - NET_STRING_IP_ADDRESS | NET_STRING_IP_SERVICE | NET_STRING_IP_NETWORK | NET_STRING_IP_ADDRESS_NO_SCOPE | NET_STRING_IP_SERVICE_NO_SCOPE
	code = ParseNetworkString (rule, types, addressInfo, NULL, NULL);

	_r_mem_free (addressInfo);

	return (code == ERROR_SUCCESS);
}

BOOLEAN _app_isruleport (LPCWSTR rule, SIZE_T length)
{
	for (SIZE_T i = 0; i < length; i++)
	{
		if (iswdigit (rule[i]) == 0 && rule[i] != DIVIDER_RULE_RANGE)
			return FALSE;
	}

	return TRUE;
}

BOOLEAN _app_isrulevalid (LPCWSTR rule, SIZE_T length)
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

	for (SIZE_T i = 0; i < length; i++)
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

BOOLEAN _app_profile_load_check_node (pugi::xml_node* root, ENUM_TYPE_XML type, BOOLEAN is_strict)
{
	return is_strict ? (root->attribute (L"type").as_int () == type) : (root->attribute (L"type").empty () || (root->attribute (L"type").as_int () == type));
}

BOOLEAN _app_profile_load_check (LPCWSTR path, ENUM_TYPE_XML type, BOOLEAN is_strict)
{
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file (path, PUGIXML_LOAD_FLAGS, PUGIXML_LOAD_ENCODING);

	if (result)
	{
		pugi::xml_node root = doc.child (L"root");

		return _app_profile_load_check_node (&root, type, is_strict);
	}

	return FALSE;
}

VOID _app_profile_load_fallback ()
{
	if (!_app_isappfound (config.my_hash))
	{
		_app_addapplication (NULL, DataUnknown, _r_sys_getimagepathname (), NULL, NULL);
		_app_setappinfo (config.my_hash, InfoIsEnabled, TRUE);
	}

	_app_setappinfo (config.my_hash, InfoIsUndeletable, TRUE);

	// disable deletion for this shit ;)
	if (!_r_config_getboolean (L"IsInternalRulesDisabled", FALSE))
	{
		if (!_app_isappfound (config.ntoskrnl_hash))
			_app_addapplication (NULL, DataUnknown, PROC_SYSTEM_NAME, NULL, NULL);

		if (!_app_isappfound (config.svchost_hash))
			_app_addapplication (NULL, DataUnknown, _r_obj_getstring (config.svchost_path), NULL, NULL);

		_app_setappinfo (config.ntoskrnl_hash, InfoIsUndeletable, TRUE);
		_app_setappinfo (config.svchost_hash, InfoIsUndeletable, TRUE);
	}
}

VOID _app_profile_load_helper (pugi::xml_node* root, ENUM_TYPE_DATA type, UINT version)
{
	INT blocklist_spy_state = _r_calc_clamp (INT, _r_config_getinteger (L"BlocklistSpyState", 2), 0, 2);
	INT blocklist_update_state = _r_calc_clamp (INT, _r_config_getinteger (L"BlocklistUpdateState", 0), 0, 2);
	INT blocklist_extra_state = _r_calc_clamp (INT, _r_config_getinteger (L"BlocklistExtraState", 0), 0, 2);
	PITEM_APP ptr_app;
	SIZE_T app_hash;

	for (pugi::xml_node item = root->child (L"item"); item; item = item.next_sibling (L"item"))
	{
		if (type == DataAppRegular)
		{
			app_hash = _app_addapplication (NULL, DataUnknown, item.attribute (L"path").as_string (), NULL, NULL);
			ptr_app = _app_getappitem (app_hash);

			if (ptr_app)
			{
				time_t time;

				_app_setappinfo (ptr_app, InfoIsSilent, (LONG_PTR)item.attribute (L"is_silent").as_bool ());
				_app_setappinfo (ptr_app, InfoIsEnabled, (LONG_PTR)item.attribute (L"is_enabled").as_bool ());

				if (!item.attribute (L"timestamp").empty ())
				{
					time = item.attribute (L"timestamp").as_llong ();
					_app_setappinfo (ptr_app, InfoTimestampPtr, (LONG_PTR)&time);
				}

				if (!item.attribute (L"timer").empty ())
				{
					time = item.attribute (L"timer").as_llong ();
					_app_setappinfo (ptr_app, InfoTimerPtr, (LONG_PTR)&time);
				}

				_r_obj_dereference (ptr_app);
			}
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
					ptr_config = (PITEM_RULE_CONFIG)_r_obj_referencesafe (rules_config.at (rule_hash));

					if (ptr_config)
					{
						ptr_rule->is_enabled = ptr_config->is_enabled;
					}
				}
			}

			// load apps
			{
				R_STRINGBUILDER ruleApps;

				_r_obj_createstringbuilder (&ruleApps);

				if (!item.attribute (L"apps").empty ())
					_r_string_append (&ruleApps, item.attribute (L"apps").as_string ());

				if (is_internal && ptr_config && !_r_str_isempty (ptr_config->apps))
				{
					if (!_r_str_isempty (&ruleApps))
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

				if (!_r_str_isempty (&ruleApps))
				{
					if (version < XML_PROFILE_VER_3)
						_r_str_replacechar (ruleApps.String->Buffer, DIVIDER_RULE[0], DIVIDER_APP[0]); // for compat with old profiles

					R_STRINGREF remainingPart;
					PR_STRING appPath;
					PR_STRING expandedPath;
					SIZE_T app_hash;

					_r_stringref_initialize2 (&remainingPart, _r_obj_finalstringbuilder (&ruleApps));

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
										app_hash = _app_addapplication (NULL, DataUnknown, _r_obj_getstring (appPath), NULL, NULL);

									if (ptr_rule->type == DataRuleSystem)
										_app_setappinfo (app_hash, InfoIsUndeletable, TRUE);

									ptr_rule->apps->emplace (app_hash, TRUE);
								}

								_r_obj_dereference (appPath);
							}
						}
					}
				}

				_r_obj_deletestringbuilder (&ruleApps);
			}

			rules_arr.push_back (ptr_rule);
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
		_r_logerror_v (Error, UID, L"pugi::load_file", 0, L"status: %d,offset: %" TEXT (PR_PTRDIFF) L",text: %hs,file: %s", load_original.status, load_original.offset, load_original.description (), path);

	if (load_original && root)
	{
		if (_app_profile_load_check_node (&root, XmlProfileInternalV3, TRUE))
		{
			UINT version = root.attribute (L"version").as_uint ();

			if (ptimestamp)
				*ptimestamp = root.attribute (L"timestamp").as_llong ();

			pugi::xml_node root_rules_system = root.child (L"rules_system");

			if (root_rules_system)
				_app_profile_load_helper (&root_rules_system, DataRuleSystem, version);

			pugi::xml_node root_rules_blocklist = root.child (L"rules_blocklist");

			if (root_rules_blocklist)
				_app_profile_load_helper (&root_rules_blocklist, DataRuleBlocklist, version);
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
				_r_logerror_v (Error, UID, L"pugi::load_file", 0, L"status: %d,offset: %" TEXT (PR_PTRDIFF) L",text: %hs,file: %s", result.status, result.offset, result.description (), !_r_str_isempty (path_custom) ? path_custom : config.profile_path);
		}
		else
		{
			pugi::xml_node root = doc.child (L"root");

			if (root)
			{
				UINT version = root.attribute (L"version").as_uint ();

				if (_app_profile_load_check_node (&root, XmlProfileV3, TRUE))
				{
					// load apps (new!)
					pugi::xml_node root_apps = root.child (L"apps");

					if (root_apps)
						_app_profile_load_helper (&root_apps, DataAppRegular, version);

					// load rules config (new!)
					pugi::xml_node root_rules_config = root.child (L"rules_config");

					if (root_rules_config)
						_app_profile_load_helper (&root_rules_config, DataRulesConfig, version);

					// load user rules (new!)
					pugi::xml_node root_rules_custom = root.child (L"rules_custom");

					if (root_rules_custom)
						_app_profile_load_helper (&root_rules_custom, DataRuleCustom, version);
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
		PITEM_APP ptr_app;
		PITEM_RULE ptr_rule;
		SIZE_T app_hash;
		time_t current_time = _r_unixtime_now ();

		// add apps
		for (auto it = apps.begin (); it != apps.end (); ++it)
		{
			ptr_app = (PITEM_APP)_r_obj_referencesafe (it->second);

			if (!ptr_app)
				continue;

			app_hash = it->first;

			INT listview_id = _app_getlistview_id (ptr_app->type);

			if (listview_id)
			{
				_r_fastlock_acquireshared (&lock_checkbox);

				_r_listview_additemex (hwnd, listview_id, 0, 0, SZ_EMPTY, ptr_app->icon_id, _app_getappgroup (app_hash, ptr_app), app_hash);
				_app_setappiteminfo (hwnd, listview_id, 0, app_hash, ptr_app);

				_r_fastlock_releaseshared (&lock_checkbox);
			}

			// install timer
			if (ptr_app->timer)
				_app_timer_set (hwnd, ptr_app, ptr_app->timer - current_time);

			_r_obj_dereference (ptr_app);
		}

		// add rules
		for (SIZE_T i = 0; i < rules_arr.size (); i++)
		{
			ptr_rule = (PITEM_RULE)_r_obj_referencesafe (rules_arr.at (i));

			if (!ptr_rule)
				continue;

			INT listview_id = _app_getlistview_id (ptr_rule->type);

			if (listview_id)
			{
				_r_fastlock_acquireshared (&lock_checkbox);

				_r_listview_additemex (hwnd, listview_id, 0, 0, SZ_EMPTY, _app_getruleicon (ptr_rule), _app_getrulegroup (ptr_rule), i);
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
				PITEM_APP ptr_app = (PITEM_APP)_r_obj_referencesafe (it->second);

				if (!ptr_app)
					continue;

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
				PITEM_RULE ptr_rule = (PITEM_RULE)_r_obj_referencesafe (*it);

				if (!ptr_rule)
					continue;

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
				PITEM_RULE_CONFIG ptr_config = (PITEM_RULE_CONFIG)_r_obj_referencesafe (it->second);

				if (!ptr_config)
					continue;

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
