// simplewall
// Copyright (c) 2016-2021 Henry++

#include "global.h"

PVOID _app_getappinfo (PITEM_APP ptr_app, ENUM_INFO_DATA info_data)
{
	if (info_data == InfoPath)
	{
		if (!_r_obj_isstringempty (ptr_app->real_path))
			return _r_obj_reference (ptr_app->real_path);
	}
	else if (info_data == InfoName)
	{
		if (!_r_obj_isstringempty (ptr_app->display_name))
			return _r_obj_reference (ptr_app->display_name);

		else if (!_r_obj_isstringempty (ptr_app->original_path))
			return _r_obj_reference (ptr_app->original_path);
	}
	else if (info_data == InfoTimestampPtr)
	{
		return &ptr_app->timestamp;
	}
	else if (info_data == InfoTimerPtr)
	{
		return &ptr_app->timer;
	}
	else if (info_data == InfoIconId)
	{
		if (ptr_app->icon_id)
			return IntToPtr (ptr_app->icon_id);

		return IntToPtr (config.icon_id);
	}
	else if (info_data == InfoListviewId)
	{
		return IntToPtr (_app_getlistview_id (ptr_app->type));
	}
	else if (info_data == InfoIsEnabled)
	{
		return IntToPtr (ptr_app->is_enabled ? TRUE : FALSE);
	}
	else if (info_data == InfoIsSilent)
	{
		return IntToPtr (ptr_app->is_silent ? TRUE : FALSE);
	}
	else if (info_data == InfoIsTimerSet)
	{
		return IntToPtr (_app_istimerset (ptr_app->htimer) ? TRUE : FALSE);
	}
	else if (info_data == InfoIsUndeletable)
	{
		return IntToPtr (ptr_app->is_undeletable ? TRUE : FALSE);
	}

	return NULL;
}

PVOID _app_getappinfobyhash (SIZE_T app_hash, ENUM_INFO_DATA info_data)
{
	PITEM_APP ptr_app;

	ptr_app = _r_obj_findhashtable (apps, app_hash);

	if (!ptr_app)
		return NULL;

	return _app_getappinfo (ptr_app, info_data);
}

VOID _app_setappinfo (PITEM_APP ptr_app, ENUM_INFO_DATA info_data, PVOID value)
{
	if (info_data == InfoTimestampPtr)
	{
		ptr_app->timestamp = *((PLONG64)value);
	}
	else if (info_data == InfoTimerPtr)
	{
		LONG64 timestamp = *((PLONG64)value);

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
	else if (info_data == InfoIsSilent)
	{
		ptr_app->is_silent = (PtrToInt (value) ? TRUE : FALSE);
	}
	else if (info_data == InfoIsEnabled)
	{
		ptr_app->is_enabled = (PtrToInt (value) ? TRUE : FALSE);
	}
	else if (info_data == InfoIsUndeletable)
	{
		ptr_app->is_undeletable = (PtrToInt (value) ? TRUE : FALSE);
	}
}

VOID _app_setappinfobyhash (SIZE_T app_hash, ENUM_INFO_DATA info_data, PVOID value)
{
	PITEM_APP ptr_app = _r_obj_findhashtable (apps, app_hash);

	if (ptr_app)
		_app_setappinfo (ptr_app, info_data, value);
}

PITEM_APP _app_addapplication (HWND hwnd, ENUM_TYPE_DATA type, LPCWSTR path, PR_STRING display_name, PR_STRING real_path)
{
	if (_r_str_isempty (path) || PathIsDirectory (path))
		return NULL;

	WCHAR path_full[1024];
	PITEM_APP ptr_app;
	PR_STRING signature_string;
	SIZE_T path_length;
	SIZE_T app_hash;
	BOOLEAN is_ntoskrnl;

	path_length = _r_str_length (path);

	// prevent possible duplicate apps entries with short path (issue #640)
	if (_r_str_findchar (path, path_length, L'~') != SIZE_MAX)
	{
		if (GetLongPathName (path, path_full, RTL_NUMBER_OF (path_full)))
		{
			path = path_full;
			path_length = _r_str_length (path_full);
		}
	}

	app_hash = _r_str_hash (path, path_length);
	ptr_app = _r_obj_findhashtable (apps, app_hash);

	if (ptr_app)
		return ptr_app; // already exists

	PVOID original_memory = _r_mem_allocatezero (sizeof (ITEM_APP));
	ptr_app = original_memory;
	is_ntoskrnl = (app_hash == config.ntoskrnl_hash);

	ptr_app->app_hash = app_hash;

	if (_r_str_compare_length (path, L"S-1-", 4) == 0) // uwp (win8+)
		type = DataAppUWP;

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
		ptr_app->real_path = _r_obj_createstringex (path, path_length * sizeof (WCHAR));
	}
	else
	{
		if (!is_ntoskrnl && _r_str_findchar (path, path_length, OBJ_NAME_PATH_SEPARATOR) == SIZE_MAX)
		{
			ptr_app->type = DataAppPico;
		}
		else
		{
			ptr_app->type = PathIsNetworkPath (path) ? DataAppNetwork : DataAppRegular;
		}

		ptr_app->real_path = is_ntoskrnl ? _r_obj_createstring2 (config.ntoskrnl_path) : _r_obj_createstringex (path, path_length * sizeof (WCHAR));
	}

	ptr_app->original_path = _r_obj_createstringex (path, path_length * sizeof (WCHAR));

	// fix "System" lowercase
	if (is_ntoskrnl)
	{
		_r_str_tolower (ptr_app->original_path->buffer, _r_obj_getstringlength (ptr_app->original_path));
		ptr_app->original_path->buffer[0] = _r_str_upper (ptr_app->original_path->buffer[0]);
	}

	if (ptr_app->type == DataAppRegular || ptr_app->type == DataAppDevice || ptr_app->type == DataAppNetwork)
		ptr_app->short_name = _r_obj_createstring (_r_path_getbasename (path));

	// get signature information
	if (_r_config_getboolean (L"IsCertificatesEnabled", FALSE))
	{
		signature_string = _app_getsignatureinfo (ptr_app);

		if (signature_string)
			_r_obj_dereference (signature_string);
	}

	ptr_app->guids = _r_obj_createarrayex (sizeof (GUID), 0x10, NULL); // initialize array

	ptr_app->timestamp = _r_unixtime_now ();

	if (ptr_app->type == DataAppService || ptr_app->type == DataAppUWP)
		ptr_app->is_undeletable = TRUE;

	// insert object into the map
	ptr_app = _r_obj_addhashtableitem (apps, app_hash, ptr_app);

	_r_mem_free (original_memory);

	if (!ptr_app)
		return NULL;

	// insert item
	if (hwnd)
	{
		INT listview_id = _app_getlistview_id (ptr_app->type);

		if (listview_id)
		{
			_r_spinlock_acquireshared (&lock_checkbox);

			_r_listview_additemex (hwnd, listview_id, 0, 0, SZ_EMPTY, ptr_app->icon_id, _app_getappgroup (ptr_app), app_hash);
			_app_setappiteminfo (hwnd, listview_id, 0, ptr_app);

			_r_spinlock_releaseshared (&lock_checkbox);
		}
	}

	return ptr_app;
}

PITEM_RULE _app_addrule (PR_STRING name, PR_STRING rule_remote, PR_STRING rule_local, FWP_DIRECTION direction, UINT8 protocol, ADDRESS_FAMILY af)
{
	PITEM_RULE ptr_rule = _r_mem_allocatezero (sizeof (ITEM_RULE));

	ptr_rule->apps = _r_obj_createhashtable (sizeof (R_HASHSTORE), NULL); // initialize hashtable
	ptr_rule->guids = _r_obj_createarray (sizeof (GUID), NULL); // initialize array

	// set rule name
	if (name)
	{
		ptr_rule->name = _r_obj_reference (name);

		if (_r_obj_getstringlength (ptr_rule->name) > RULE_NAME_CCH_MAX)
			_r_obj_setstringsize (ptr_rule->name, RULE_NAME_CCH_MAX * sizeof (WCHAR));
	}

	// set rule destination
	if (rule_remote)
	{
		ptr_rule->rule_remote = _r_obj_reference (rule_remote);

		if (_r_obj_getstringlength (ptr_rule->rule_remote) > RULE_RULE_CCH_MAX)
			_r_obj_setstringsize (ptr_rule->rule_remote, RULE_RULE_CCH_MAX * sizeof (WCHAR));
	}

	// set rule source
	if (rule_local)
	{
		ptr_rule->rule_local = _r_obj_reference (rule_local);

		if (_r_obj_getstringlength (ptr_rule->rule_local) > RULE_RULE_CCH_MAX)
			_r_obj_setstringsize (ptr_rule->rule_local, RULE_RULE_CCH_MAX * sizeof (WCHAR));
	}

	// set configuration
	ptr_rule->direction = direction;
	ptr_rule->protocol = protocol;
	ptr_rule->af = af;

	return ptr_rule;
}

PITEM_RULE_CONFIG _app_addruleconfigtable (PR_HASHTABLE table, SIZE_T rule_hash, PR_STRING name, BOOLEAN is_enabled)
{
	ITEM_RULE_CONFIG entry = {0};

	entry.name = name;
	entry.is_enabled = is_enabled;

	return _r_obj_addhashtableitem (table, rule_hash, &entry);
}

PITEM_RULE _app_getrulebyid (SIZE_T idx)
{
	if (idx != SIZE_MAX && idx < _r_obj_getarraysize (rules_arr))
	{
		return _r_obj_getarrayitem (rules_arr, idx);
	}

	return NULL;
}

PITEM_RULE _app_getrulebyhash (SIZE_T rule_hash)
{
	if (!rule_hash)
		return NULL;

	for (SIZE_T i = 0; i < _r_obj_getarraysize (rules_arr); i++)
	{
		PITEM_RULE ptr_rule = _r_obj_getarrayitem (rules_arr, i);

		if (ptr_rule)
		{
			if (ptr_rule->is_readonly)
			{
				if (_r_obj_getstringhash (ptr_rule->name) == rule_hash)
					return ptr_rule;
			}
		}
	}

	return NULL;
}

SIZE_T _app_getnetworkapp (SIZE_T network_hash)
{
	PITEM_NETWORK ptr_network = _r_obj_findhashtable (network_map, network_hash);

	if (ptr_network)
	{
		return ptr_network->app_hash;
	}

	return 0;
}

PITEM_LOG _app_getlogitem (SIZE_T idx)
{
	if (idx != SIZE_MAX && idx < _r_obj_getlistsize (log_arr))
	{
		return _r_obj_referencesafe (_r_obj_getlistitem (log_arr, idx));
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
	PITEM_APP ptr_app = _r_obj_findhashtable (apps, app_hash);
	LPCWSTR color_value = NULL;
	BOOLEAN is_profilelist = (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_RULES_CUSTOM);
	BOOLEAN is_networklist = (listview_id == IDC_NETWORK || listview_id == IDC_LOG);

	if (ptr_app && !is_networklist)
	{
		if (_r_config_getbooleanex (L"IsHighlightInvalid", TRUE, L"colors") && !_app_isappexists (ptr_app))
		{
			color_value = L"ColorInvalid";
			goto CleanupExit;
		}

		if (_r_config_getbooleanex (L"IsHighlightTimer", TRUE, L"colors") && _app_istimerset (ptr_app->htimer))
		{
			color_value = L"ColorTimer";
			goto CleanupExit;
		}
	}

	if (_r_config_getbooleanex (L"IsHighlightConnection", TRUE, L"colors") && is_validconnection)
	{
		color_value = L"ColorConnection";
		goto CleanupExit;
	}

	if (ptr_app)
	{
		if (_r_config_getbooleanex (L"IsHighlightSigned", TRUE, L"colors") && !ptr_app->is_silent && _r_config_getboolean (L"IsCertificatesEnabled", FALSE) && ptr_app->is_signed)
		{
			color_value = L"ColorSigned";
			goto CleanupExit;
		}

		if (!is_profilelist && (_r_config_getbooleanex (L"IsHighlightSpecial", TRUE, L"colors") && _app_isapphaverule (app_hash, FALSE)))
		{
			color_value = L"ColorSpecial";
			goto CleanupExit;
		}

		if (!is_networklist && _r_config_getbooleanex (L"IsHighlightSilent", TRUE, L"colors") && ptr_app->is_silent)
		{
			color_value = L"ColorSilent";
			goto CleanupExit;
		}

		if (_r_config_getbooleanex (L"IsHighlightPico", TRUE, L"colors") && ptr_app->type == DataAppPico)
		{
			color_value = L"ColorPico";
			goto CleanupExit;
		}
	}

	if (_r_config_getbooleanex (L"IsHighlightSystem", TRUE, L"colors") && is_systemapp)
	{
		color_value = L"ColorSystem";
		goto CleanupExit;
	}

CleanupExit:

	if (color_value)
		return _app_getcolorvalue (_r_str_hash (color_value, _r_str_length (color_value)));

	return 0;
}

VOID _app_freeapplication (SIZE_T app_hash)
{
	if (!app_hash)
		return;

	for (SIZE_T i = 0; i < _r_obj_getarraysize (rules_arr); i++)
	{
		PITEM_RULE ptr_rule = _r_obj_getarrayitem (rules_arr, i);

		if (ptr_rule)
		{
			if (ptr_rule->type == DataRuleUser)
			{
				if (_r_obj_findhashtable (ptr_rule->apps, app_hash))
				{
					_r_obj_removehashtableentry (ptr_rule->apps, app_hash);

					if (ptr_rule->is_enabled && _r_obj_ishashtableempty (ptr_rule->apps))
					{
						ptr_rule->is_enabled = FALSE;
						ptr_rule->is_haveerrors = FALSE;
					}

					INT rule_listview_id = _app_getlistview_id (ptr_rule->type);

					if (rule_listview_id)
					{
						INT item_pos = _app_getposition (_r_app_gethwnd (), rule_listview_id, i);

						if (item_pos != -1)
						{
							_r_spinlock_acquireshared (&lock_checkbox);
							_app_setruleiteminfo (_r_app_gethwnd (), rule_listview_id, item_pos, ptr_rule, FALSE);
							_r_spinlock_releaseshared (&lock_checkbox);
						}
					}
				}
			}
		}
	}

	_r_obj_removehashtableentry (apps, app_hash);
}

VOID _app_getcount (PITEM_STATUS ptr_status)
{
	PITEM_APP ptr_app;
	PITEM_RULE ptr_rule;
	SIZE_T enum_key = 0;
	BOOLEAN is_used;

	while (_r_obj_enumhashtable (apps, &ptr_app, NULL, &enum_key))
	{
		is_used = _app_isappused (ptr_app);

		if (_app_istimerset (ptr_app->htimer))
			ptr_status->apps_timer_count += 1;

		if (!ptr_app->is_undeletable && (!_app_isappexists (ptr_app) || !is_used) && !(ptr_app->type == DataAppService || ptr_app->type == DataAppUWP))
			ptr_status->apps_unused_count += 1;

		if (is_used)
			ptr_status->apps_count += 1;
	}

	for (SIZE_T i = 0; i < _r_obj_getarraysize (rules_arr); i++)
	{
		ptr_rule = _r_obj_getarrayitem (rules_arr, i);

		if (ptr_rule)
		{
			if (ptr_rule->type == DataRuleUser)
			{
				if (ptr_rule->is_enabled && !_r_obj_ishashtableempty (ptr_rule->apps))
					ptr_status->rules_global_count += 1;

				if (ptr_rule->is_readonly)
				{
					ptr_status->rules_predefined_count += 1;
				}
				else
				{
					ptr_status->rules_user_count += 1;
				}

				ptr_status->rules_count += 1;
			}
		}
	}
}

INT _app_getappgroup (PITEM_APP ptr_app)
{
	// apps with special rule
	if (_app_isapphaverule (ptr_app->app_hash, FALSE))
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

	LPCWSTR color_value = NULL;

	if (_r_config_getbooleanex (L"IsHighlightInvalid", TRUE, L"colors") && ptr_rule->is_enabled && ptr_rule->is_haveerrors)
		color_value = L"ColorInvalid";

	else if (_r_config_getbooleanex (L"IsHighlightSpecial", TRUE, L"colors") && ptr_rule->type == DataRuleUser && !_r_obj_ishashtableempty (ptr_rule->apps))
		color_value = L"ColorSpecial";

	if (color_value)
		return _app_getcolorvalue (_r_str_hash (color_value, _r_str_length (color_value)));

	return 0;
}

PR_STRING _app_gettooltip (HWND hwnd, INT listview_id, INT item_id)
{
	R_STRINGBUILDER buffer = {0};
	PR_STRING string;

	BOOLEAN is_appslist = (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP);
	BOOLEAN is_ruleslist = (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM) || listview_id == IDC_APP_RULES_ID;

	if (is_appslist || listview_id == IDC_RULE_APPS_ID)
	{
		SIZE_T app_hash = _r_listview_getitemlparam (hwnd, listview_id, item_id);
		PITEM_APP ptr_app = _r_obj_findhashtable (apps, app_hash);

		if (ptr_app)
		{
			_r_obj_initializestringbuilder (&buffer);

			// app path
			{
				PR_STRING path_string = _r_format_string (L"%s\r\n", _r_obj_getstring (!_r_obj_isstringempty (ptr_app->real_path) ? ptr_app->real_path : (!_r_obj_isstringempty (ptr_app->display_name) ? ptr_app->display_name : ptr_app->original_path)));

				if (path_string)
				{
					_r_obj_appendstringbuilder2 (&buffer, path_string);

					_r_obj_dereference (path_string);
				}
			}

			// app information
			{
				R_STRINGBUILDER info_string;

				_r_obj_initializestringbuilder (&info_string);

				if (ptr_app->type == DataAppRegular)
				{
					PR_STRING version_string = _app_getversioninfo (ptr_app);

					if (version_string)
					{
						if (!_r_obj_isstringempty (version_string))
							_r_obj_appendstringbuilderformat (&info_string, SZ_TAB L"%s\r\n", version_string->buffer);

						_r_obj_dereference (version_string);
					}
				}
				else if (ptr_app->type == DataAppService)
				{
					_r_obj_appendstringbuilderformat (&info_string, SZ_TAB L"%s" SZ_TAB_CRLF L"%s\r\n", _r_obj_getstringorempty (ptr_app->original_path), _r_obj_getstringorempty (ptr_app->display_name));
				}
				else if (ptr_app->type == DataAppUWP)
				{
					_r_obj_appendstringbuilderformat (&info_string, SZ_TAB L"%s\r\n", _r_obj_getstringorempty (ptr_app->display_name));
				}

				// signature information
				if (_r_config_getboolean (L"IsCertificatesEnabled", FALSE) && ptr_app->is_signed)
				{
					PR_STRING signature_string = _app_getsignatureinfo (ptr_app);

					if (signature_string)
					{
						if (!_r_obj_isstringempty (signature_string))
							_r_obj_appendstringbuilderformat (&info_string, SZ_TAB L"%s: %s\r\n", _r_locale_getstring (IDS_SIGNATURE), signature_string->buffer);

						_r_obj_dereference (signature_string);
					}
				}

				string = _r_obj_finalstringbuilder (&info_string);

				if (!_r_obj_isstringempty (string))
				{
					_r_obj_insertstringbuilderformat (&info_string, 0, L"%s:\r\n", _r_locale_getstring (IDS_FILE));
					_r_obj_appendstringbuilder2 (&buffer, _r_obj_finalstringbuilder (&info_string));
				}

				_r_obj_deletestringbuilder (&info_string);
			}

			// app timer
			if (_app_istimerset (ptr_app->htimer))
			{
				WCHAR interval_string[128];
				_r_format_interval (interval_string, RTL_NUMBER_OF (interval_string), ptr_app->timer - _r_unixtime_now (), 3);

				_r_obj_appendstringbuilderformat (&buffer, L"%s:" SZ_TAB_CRLF L"%s\r\n", _r_locale_getstring (IDS_TIMELEFT), interval_string);
			}

			// app rules
			{
				PR_STRING app_rules_string = _app_appexpandrules (app_hash, SZ_TAB_CRLF);

				if (app_rules_string)
				{
					if (!_r_obj_isstringempty (app_rules_string))
						_r_obj_appendstringbuilderformat (&buffer, L"%s:" SZ_TAB_CRLF L"%s\r\n", _r_locale_getstring (IDS_RULE), app_rules_string->buffer);

					_r_obj_dereference (app_rules_string);
				}
			}

			// app notes
			{
				R_STRINGBUILDER notes_string;

				_r_obj_initializestringbuilder (&notes_string);

				// app type
				if (ptr_app->type == DataAppNetwork)
				{
					_r_obj_appendstringbuilderformat (&notes_string, SZ_TAB L"%s\r\n", _r_locale_getstring (IDS_HIGHLIGHT_NETWORK));
				}
				else if (ptr_app->type == DataAppPico)
				{
					_r_obj_appendstringbuilderformat (&notes_string, SZ_TAB L"%s\r\n", _r_locale_getstring (IDS_HIGHLIGHT_PICO));
				}

				// app settings
				if (_app_isappfromsystem (_r_obj_getstring (ptr_app->real_path), app_hash))
					_r_obj_appendstringbuilderformat (&notes_string, SZ_TAB L"%s\r\n", _r_locale_getstring (IDS_HIGHLIGHT_SYSTEM));

				if (_app_isapphaveconnection (app_hash))
					_r_obj_appendstringbuilderformat (&notes_string, SZ_TAB L"%s\r\n", _r_locale_getstring (IDS_HIGHLIGHT_CONNECTION));

				if (is_appslist && ptr_app->is_silent)
					_r_obj_appendstringbuilderformat (&notes_string, SZ_TAB L"%s\r\n", _r_locale_getstring (IDS_HIGHLIGHT_SILENT));

				if (!_app_isappexists (ptr_app))
					_r_obj_appendstringbuilderformat (&notes_string, SZ_TAB L"%s\r\n", _r_locale_getstring (IDS_HIGHLIGHT_INVALID));

				string = _r_obj_finalstringbuilder (&notes_string);

				if (!_r_obj_isstringempty (string))
				{
					_r_obj_insertstringbuilderformat (&notes_string, 0, L"%s:\r\n", _r_locale_getstring (IDS_NOTES));
					_r_obj_appendstringbuilder2 (&buffer, _r_obj_finalstringbuilder (&notes_string));
				}

				_r_obj_deletestringbuilder (&notes_string);
			}

			string = _r_obj_finalstringbuilder (&buffer);

			if (!_r_obj_isstringempty (string))
				return string;

			_r_obj_deletestringbuilder (&buffer);
		}
	}
	else if (is_ruleslist)
	{
		LPARAM lparam = _r_listview_getitemlparam (hwnd, listview_id, item_id);
		PITEM_RULE ptr_rule = _app_getrulebyid (lparam);

		if (ptr_rule)
		{
			_r_obj_initializestringbuilder (&buffer);

			LPCWSTR empty_string;

			PR_STRING info_string;
			PR_STRING rule_remote_string = _app_rulesexpandrules (ptr_rule->rule_remote, L"\r\n" SZ_TAB);
			PR_STRING rule_local_string = _app_rulesexpandrules (ptr_rule->rule_local, L"\r\n" SZ_TAB);

			empty_string = _r_locale_getstring (IDS_STATUS_EMPTY);

			// rule information
			info_string = _r_format_string (L"%s (#%" TEXT (PR_SIZE_T) L")\r\n%s (" SZ_DIRECTION_REMOTE L"):\r\n%s%s\r\n%s (" SZ_DIRECTION_LOCAL L"):\r\n%s%s",
											_r_obj_getstringordefault (ptr_rule->name, empty_string),
											lparam,
											_r_locale_getstring (IDS_RULE),
											SZ_TAB,
											_r_obj_getstringordefault (rule_remote_string, empty_string),
											_r_locale_getstring (IDS_RULE),
											SZ_TAB,
											_r_obj_getstringordefault (rule_local_string, empty_string)
			);

			_r_obj_appendstringbuilder2 (&buffer, info_string);

			SAFE_DELETE_REFERENCE (info_string);
			SAFE_DELETE_REFERENCE (rule_remote_string);
			SAFE_DELETE_REFERENCE (rule_local_string);

			// rule apps
			if (ptr_rule->is_forservices || !_r_obj_ishashtableempty (ptr_rule->apps))
			{
				PR_STRING rule_apps_string = _app_rulesexpandapps (ptr_rule, TRUE, SZ_TAB_CRLF);

				if (rule_apps_string)
				{
					if (!_r_obj_isstringempty (rule_apps_string))
						_r_obj_appendstringbuilderformat (&buffer, L"\r\n%s:\r\n%s%s", _r_locale_getstring (IDS_TAB_APPS), SZ_TAB, rule_apps_string->buffer);

					_r_obj_dereference (rule_apps_string);
				}
			}

			// rule notes
			if (ptr_rule->is_readonly && ptr_rule->type == DataRuleUser)
			{
				_r_obj_appendstringbuilderformat (&buffer, SZ_TAB L"\r\n%s:\r\n" SZ_TAB L"%s\r\n", _r_locale_getstring (IDS_NOTES), SZ_RULE_INTERNAL_TITLE);
			}

			string = _r_obj_finalstringbuilder (&buffer);

			if (!_r_obj_isstringempty (string))
				return string;

			_r_obj_deletestringbuilder (&buffer);
		}
	}
	else if (listview_id == IDC_NETWORK)
	{
		LPARAM lparam = _r_listview_getitemlparam (hwnd, listview_id, item_id);
		PITEM_NETWORK ptr_network = _r_obj_findhashtable (network_map, lparam);

		if (ptr_network)
		{
			LPCWSTR empty_string;

			PR_STRING info_string;
			PR_STRING local_address_string;
			PR_STRING remote_address_string;

			empty_string = _r_locale_getstring (IDS_STATUS_EMPTY);

			local_address_string = _app_formataddress (ptr_network->af, 0, &ptr_network->local_addr, ptr_network->local_port, 0);
			remote_address_string = _app_formataddress (ptr_network->af, 0, &ptr_network->remote_addr, ptr_network->remote_port, 0);

			info_string = _r_format_string (L"%s\r\n%s (" SZ_DIRECTION_LOCAL L"): %s\r\n%s (" SZ_DIRECTION_REMOTE L"): %s\r\n%s: %s\r\n%s: %s",
											_r_obj_getstringordefault (ptr_network->path, empty_string),
											_r_locale_getstring (IDS_ADDRESS),
											_r_obj_getstringordefault (local_address_string, empty_string),
											_r_locale_getstring (IDS_ADDRESS),
											_r_obj_getstringordefault (remote_address_string, empty_string),
											_r_locale_getstring (IDS_PROTOCOL),
											_app_getprotoname (ptr_network->protocol, ptr_network->af, empty_string),
											_r_locale_getstring (IDS_STATE),
											_app_getconnectionstatusname (ptr_network->state, empty_string)
			);

			SAFE_DELETE_REFERENCE (local_address_string);
			SAFE_DELETE_REFERENCE (remote_address_string);

			return info_string;
		}

	}
	else if (listview_id == IDC_LOG)
	{
		LPARAM lparam = _r_listview_getitemlparam (hwnd, listview_id, item_id);
		PITEM_LOG ptr_log = _app_getlogitem (lparam);

		if (ptr_log)
		{
			WCHAR date_string[256];

			LPCWSTR empty_string;

			PR_STRING info_string;
			PR_STRING local_address_string;
			PR_STRING remote_address_string;
			PR_STRING direction_string;

			_r_format_unixtimeex (date_string, RTL_NUMBER_OF (date_string), ptr_log->timestamp, FDTF_LONGDATE | FDTF_LONGTIME);

			empty_string = _r_locale_getstring (IDS_STATUS_EMPTY);

			local_address_string = _app_formataddress (ptr_log->af, 0, &ptr_log->local_addr, ptr_log->local_port, 0);
			remote_address_string = _app_formataddress (ptr_log->af, 0, &ptr_log->remote_addr, ptr_log->remote_port, 0);
			direction_string = _app_getdirectionname (ptr_log->direction, ptr_log->is_loopback, TRUE);

			info_string = _r_format_string (L"%s\r\n%s: %s\r\n%s (" SZ_DIRECTION_LOCAL L"): %s\r\n%s (" SZ_DIRECTION_REMOTE L"): %s\r\n%s: %s\r\n%s: %s\r\n%s: %s\r\n%s: %s",
											_r_obj_getstringordefault (ptr_log->path, empty_string),
											_r_locale_getstring (IDS_DATE),
											date_string,
											_r_locale_getstring (IDS_ADDRESS),
											_r_obj_getstringordefault (local_address_string, empty_string),
											_r_locale_getstring (IDS_ADDRESS),
											_r_obj_getstringordefault (remote_address_string, empty_string),
											_r_locale_getstring (IDS_PROTOCOL),
											_app_getprotoname (ptr_log->protocol, ptr_log->af, empty_string),
											_r_locale_getstring (IDS_FILTER),
											_r_obj_getstringordefault (ptr_log->filter_name, empty_string),
											_r_locale_getstring (IDS_DIRECTION),
											_r_obj_getstringorempty (direction_string),
											_r_locale_getstring (IDS_STATE),
											(ptr_log->is_allow ? SZ_STATE_ALLOW : SZ_STATE_BLOCK)
			);

			SAFE_DELETE_REFERENCE (local_address_string);
			SAFE_DELETE_REFERENCE (remote_address_string);
			SAFE_DELETE_REFERENCE (direction_string);

			_r_obj_dereference (ptr_log);

			return info_string;
		}
	}
	else if (listview_id == IDC_RULE_REMOTE_ID || listview_id == IDC_RULE_LOCAL_ID)
	{
		return _r_listview_getitemtext (hwnd, listview_id, item_id, 0);
	}

	return NULL;
}

VOID _app_setappiteminfo (HWND hwnd, INT listview_id, INT item, PITEM_APP ptr_app)
{
	if (!listview_id || item == -1)
		return;

	_app_getappicon (ptr_app, TRUE, &ptr_app->icon_id, NULL);

	_r_listview_setitemex (hwnd, listview_id, item, 0, _app_getdisplayname (ptr_app, FALSE), ptr_app->icon_id, _app_getappgroup (ptr_app), 0);

	WCHAR date_string[256];
	_r_format_unixtimeex (date_string, RTL_NUMBER_OF (date_string), ptr_app->timestamp, FDTF_SHORTDATE | FDTF_SHORTTIME);

	_r_listview_setitem (hwnd, listview_id, item, 1, date_string);

	_r_listview_setitemcheck (hwnd, listview_id, item, ptr_app->is_enabled);
}

VOID _app_setruleiteminfo (HWND hwnd, INT listview_id, INT item, PITEM_RULE ptr_rule, BOOLEAN include_apps)
{
	if (!listview_id || item == -1)
		return;

	WCHAR rule_name[RULE_NAME_CCH_MAX];
	LPCWSTR rule_name_ptr = NULL;
	PR_STRING direction_string;
	INT rule_icon_id;
	INT rule_group_id;

	rule_icon_id = _app_getruleicon (ptr_rule);
	rule_group_id = _app_getrulegroup (ptr_rule);

	if (!_r_obj_isstringempty (ptr_rule->name))
	{
		if (ptr_rule->is_readonly && ptr_rule->type == DataRuleUser)
		{
			_r_str_printf (rule_name, RTL_NUMBER_OF (rule_name), L"%s" SZ_RULE_INTERNAL_MENU, ptr_rule->name->buffer);
			rule_name_ptr = rule_name;
		}
		else
		{
			rule_name_ptr = ptr_rule->name->buffer;
		}
	}

	direction_string = _app_getdirectionname (ptr_rule->direction, FALSE, TRUE);

	_r_listview_setitemex (hwnd, listview_id, item, 0, rule_name_ptr, rule_icon_id, rule_group_id, 0);
	_r_listview_setitem (hwnd, listview_id, item, 1, ptr_rule->protocol ? _app_getprotoname (ptr_rule->protocol, AF_UNSPEC, NULL) : _r_locale_getstring (IDS_ANY));
	_r_listview_setitem (hwnd, listview_id, item, 2, _r_obj_getstringorempty (direction_string));

	_r_listview_setitemcheck (hwnd, listview_id, item, ptr_rule->is_enabled);

	if (direction_string)
		_r_obj_dereference (direction_string);

	if (include_apps)
	{
		PITEM_APP ptr_app;
		PR_HASHSTORE hashstore;
		SIZE_T hash_code;
		SIZE_T enum_key = 0;
		INT app_listview_id;
		INT item_pos;

		while (_r_obj_enumhashtable (ptr_rule->apps, &hashstore, &hash_code, &enum_key))
		{
			ptr_app = _r_obj_findhashtable (apps, hash_code);

			if (!ptr_app)
				continue;

			app_listview_id = _app_getlistview_id (ptr_app->type);

			if (app_listview_id)
			{
				item_pos = _app_getposition (_r_app_gethwnd (), app_listview_id, ptr_app->app_hash);

				if (item_pos != -1)
					_app_setappiteminfo (hwnd, app_listview_id, item_pos, ptr_app);
			}
		}
	}
}

VOID _app_ruleenable (PITEM_RULE ptr_rule, BOOLEAN is_enable, BOOLEAN is_createconfig)
{
	ptr_rule->is_enabled = is_enable;

	if (ptr_rule->is_readonly && !_r_obj_isstringempty (ptr_rule->name))
	{
		PITEM_RULE_CONFIG ptr_config;
		SIZE_T rule_hash = _r_obj_getstringhash (ptr_rule->name);

		if (rule_hash)
		{
			ptr_config = _r_obj_findhashtable (rules_config, rule_hash);

			if (ptr_config)
			{
				ptr_config->is_enabled = is_enable;

				return;
			}

			if (is_createconfig)
			{
				ptr_config = _app_addruleconfigtable (rules_config, rule_hash, _r_obj_createstring2 (ptr_rule->name), is_enable);
			}
		}
	}
}

BOOLEAN _app_ruleblocklistsetchange (PITEM_RULE ptr_rule, INT new_state)
{
	if (new_state == -1)
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
	if (ptr_rule->type != DataRuleBlocklist || _r_obj_isstringempty (ptr_rule->name))
		return FALSE;

	LPCWSTR rule_name = ptr_rule->name->buffer;

	if (!rule_name)
		return FALSE;

	if (_r_str_compare_length (rule_name, L"spy_", 4) == 0)
		return _app_ruleblocklistsetchange (ptr_rule, spy_state);

	else if (_r_str_compare_length (rule_name, L"update_", 7) == 0)
		return _app_ruleblocklistsetchange (ptr_rule, update_state);

	else if (_r_str_compare_length (rule_name, L"extra_", 6) == 0)
		return _app_ruleblocklistsetchange (ptr_rule, extra_state);

	// fallback: block rules with other names by default!
	return _app_ruleblocklistsetchange (ptr_rule, 2);
}

VOID _app_ruleblocklistset (HWND hwnd, INT spy_state, INT update_state, INT extra_state, BOOLEAN is_instantapply)
{
	PR_LIST rules = _r_obj_createlistex (0x200, NULL);
	SIZE_T changes_count = 0;
	INT listview_id = _app_getlistview_id (DataRuleBlocklist);

	for (SIZE_T i = 0; i < _r_obj_getarraysize (rules_arr); i++)
	{
		PITEM_RULE ptr_rule = _r_obj_getarrayitem (rules_arr, i);

		if (!ptr_rule)
			continue;

		if (ptr_rule->type != DataRuleBlocklist)
			continue;

		if (!_app_ruleblocklistsetstate (ptr_rule, spy_state, update_state, extra_state))
			continue;

		changes_count += 1;
		_app_ruleenable (ptr_rule, ptr_rule->is_enabled, FALSE);

		if (hwnd)
		{
			INT item_pos = _app_getposition (hwnd, listview_id, i);

			if (item_pos != -1)
			{
				_r_spinlock_acquireshared (&lock_checkbox);
				_app_setruleiteminfo (hwnd, listview_id, item_pos, ptr_rule, FALSE);
				_r_spinlock_releaseshared (&lock_checkbox);
			}
		}

		if (is_instantapply)
		{
			_r_obj_addlistitem (rules, ptr_rule); // be freed later!
		}
	}

	if (changes_count)
	{
		if (hwnd)
		{
			if (listview_id == (INT)_r_tab_getlparam (hwnd, IDC_TAB, -1))
				_app_listviewsort (hwnd, listview_id, -1, FALSE);

			_app_refreshstatus (hwnd, listview_id);
		}

		if (is_instantapply && !_r_obj_islistempty (rules))
		{
			if (_wfp_isfiltersinstalled ())
			{
				HANDLE hengine = _wfp_getenginehandle ();

				if (hengine)
					_wfp_create4filters (hengine, rules, __LINE__, FALSE);
			}
		}

		_app_profile_save (); // required!
	}

	_r_obj_dereference (rules);
}

PR_STRING _app_appexpandrules (SIZE_T app_hash, LPCWSTR delimeter)
{
	R_STRINGBUILDER buffer;
	PR_STRING string;

	_r_obj_initializestringbuilder (&buffer);

	for (SIZE_T i = 0; i < _r_obj_getarraysize (rules_arr); i++)
	{
		PITEM_RULE ptr_rule = _r_obj_getarrayitem (rules_arr, i);

		if (!ptr_rule)
			continue;

		if (ptr_rule->is_enabled && ptr_rule->type == DataRuleUser && _r_obj_findhashtable (ptr_rule->apps, app_hash))
		{
			if (!_r_obj_isstringempty (ptr_rule->name))
			{
				if (ptr_rule->is_readonly)
				{
					_r_obj_appendstringbuilderformat (&buffer, L"%s" SZ_RULE_INTERNAL_MENU, ptr_rule->name->buffer);
				}
				else
				{
					_r_obj_appendstringbuilder2 (&buffer, ptr_rule->name);
				}

				_r_obj_appendstringbuilder (&buffer, delimeter);
			}
		}
	}

	string = _r_obj_finalstringbuilder (&buffer);

	_r_obj_trimstring (string, delimeter);

	if (!_r_obj_isstringempty (string))
		return string;

	_r_obj_deletestringbuilder (&buffer);

	return NULL;

}

PR_STRING _app_rulesexpandapps (const PITEM_RULE ptr_rule, BOOLEAN is_fordisplay, LPCWSTR delimeter)
{
	R_STRINGBUILDER buffer;
	PR_STRING string;
	PITEM_APP ptr_app;
	PR_HASHSTORE hashstore;
	SIZE_T hash_code;
	SIZE_T enum_key = 0;

	_r_obj_initializestringbuilder (&buffer);

	if (is_fordisplay && ptr_rule->is_forservices)
	{
		_r_obj_appendstringbuilderformat (&buffer, L"%s%s%s%s", PROC_SYSTEM_NAME, delimeter, _r_obj_getstring (config.svchost_path), delimeter);
	}

	while (_r_obj_enumhashtable (ptr_rule->apps, &hashstore, &hash_code, &enum_key))
	{
		ptr_app = _r_obj_findhashtable (apps, hash_code);

		if (!ptr_app)
			continue;

		if (is_fordisplay)
		{
			if (ptr_app->type == DataAppUWP)
			{
				if (!_r_obj_isstringempty (ptr_app->display_name))
					_r_obj_appendstringbuilder2 (&buffer, ptr_app->display_name);
			}
			else
			{
				if (!_r_obj_isstringempty (ptr_app->original_path))
					_r_obj_appendstringbuilder2 (&buffer, ptr_app->original_path);
			}
		}
		else
		{
			if (!_r_obj_isstringempty (ptr_app->original_path))
				_r_obj_appendstringbuilder2 (&buffer, ptr_app->original_path);
		}

		_r_obj_appendstringbuilder (&buffer, delimeter);
	}

	string = _r_obj_finalstringbuilder (&buffer);

	_r_obj_trimstring (string, delimeter);

	if (!_r_obj_isstringempty (string))
		return string;

	_r_obj_deletestringbuilder (&buffer);

	return NULL;
}

PR_STRING _app_rulesexpandrules (PR_STRING rule, LPCWSTR delimeter)
{
	R_STRINGBUILDER buffer;
	R_STRINGREF remaining_part;
	PR_STRING string;

	if (_r_obj_isstringempty (rule))
		return NULL;

	_r_obj_initializestringbuilder (&buffer);

	_r_obj_initializestringref2 (&remaining_part, rule);

	while (remaining_part.length != 0)
	{
		string = _r_str_splitatchar (&remaining_part, &remaining_part, DIVIDER_RULE[0]);

		if (string)
		{
			_r_obj_trimstring (string, DIVIDER_TRIM);

			if (!_r_obj_isstringempty (string))
				_r_obj_appendstringbuilderformat (&buffer, L"%s%s", string->buffer, delimeter);

			_r_obj_dereference (string);
		}
	}

	string = _r_obj_finalstringbuilder (&buffer);

	_r_obj_trimstring (string, delimeter);

	if (!_r_obj_isstringempty (string))
		return string;

	_r_obj_deletestringbuilder (&buffer);

	return NULL;
}

BOOLEAN _app_isappfromsystem (LPCWSTR path, SIZE_T app_hash)
{
	if (!app_hash)
		return FALSE;

	if (app_hash == config.ntoskrnl_hash || app_hash == config.svchost_hash)
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

	PITEM_NETWORK ptr_network;
	SIZE_T enum_key = 0;

	while (_r_obj_enumhashtable (network_map, &ptr_network, NULL, &enum_key))
	{
		if (ptr_network->app_hash == app_hash)
		{
			if (ptr_network->is_connection)
				return TRUE;
		}
	}

	return FALSE;
}

BOOLEAN _app_isapphavedrive (INT letter)
{
	PITEM_APP ptr_app;
	SIZE_T enum_key = 0;

	while (_r_obj_enumhashtable (apps, &ptr_app, NULL, &enum_key))
	{
		if (!_r_obj_isstringempty (ptr_app->original_path))
		{
			INT drive_id = PathGetDriveNumber (ptr_app->original_path->buffer);

			if ((drive_id != -1 && drive_id == letter) || ptr_app->type == DataAppDevice)
			{
				if (ptr_app->is_enabled || _app_isapphaverule (ptr_app->app_hash, FALSE))
					return TRUE;
			}
		}
	}

	return FALSE;
}

BOOLEAN _app_isapphaverule (SIZE_T app_hash, BOOLEAN is_countdisabled)
{
	if (!app_hash)
		return FALSE;

	for (SIZE_T i = 0; i < _r_obj_getarraysize (rules_arr); i++)
	{
		PITEM_RULE ptr_rule = _r_obj_getarrayitem (rules_arr, i);

		if (ptr_rule)
		{
			if (is_countdisabled || (ptr_rule->is_enabled && ptr_rule->type == DataRuleUser))
			{
				if (_r_obj_findhashtable (ptr_rule->apps, app_hash))
					return TRUE;
			}
		}
	}

	return FALSE;
}

BOOLEAN _app_isappused (const PITEM_APP ptr_app)
{
	if (ptr_app && (ptr_app->is_enabled || ptr_app->is_silent || _app_isapphaverule (ptr_app->app_hash, TRUE)))
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
		return !_r_obj_isstringempty (ptr_app->real_path) && _r_fs_exists (ptr_app->real_path->buffer);

	if (ptr_app->type == DataAppDevice || ptr_app->type == DataAppNetwork || ptr_app->type == DataAppPico)
		return TRUE;

	if (ptr_app->type == DataAppService || ptr_app->type == DataAppUWP)
		return TRUE;

	return FALSE;
}

BOOLEAN _app_isruletype (LPCWSTR rule, ULONG types)
{
	ULONG code;
	NET_ADDRESS_INFO address_info;

	RtlSecureZeroMemory (&address_info, sizeof (address_info));

	// host - NET_STRING_NAMED_ADDRESS | NET_STRING_NAMED_SERVICE;
	// ip - NET_STRING_IP_ADDRESS | NET_STRING_IP_SERVICE | NET_STRING_IP_NETWORK | NET_STRING_IP_ADDRESS_NO_SCOPE | NET_STRING_IP_SERVICE_NO_SCOPE
	code = ParseNetworkString (rule, types, &address_info, NULL, NULL);

	return (code == ERROR_SUCCESS);
}

VOID _app_openappdirectory (const PITEM_APP ptr_app)
{
	PR_STRING path = _app_getappinfo (ptr_app, InfoPath);

	if (path)
	{
		if (!_r_obj_isstringempty (path))
			_r_path_explore (path->buffer);

		_r_obj_dereference (path);
	}
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

			for (SIZE_T j = 0; j < RTL_NUMBER_OF (valid_chars); j++)
			{
				if (rule[i] == valid_chars[j])
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

BOOLEAN _app_profile_load_check_node (mxml_node_t* root_node, ENUM_TYPE_XML type)
{
	return (_r_str_tointeger_a (mxmlElementGetAttr (root_node, "type")) == type);
}

BOOLEAN _app_profile_load_check (LPCWSTR path, ENUM_TYPE_XML type)
{
	HANDLE hfile = CreateFile (path, FILE_GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (_r_fs_isvalidhandle (hfile))
	{
		mxml_node_t *xml_node;
		mxml_node_t *root_node;

		BOOLEAN is_success = FALSE;

		xml_node = mxmlLoadFd (NULL, hfile, MXML_OPAQUE_CALLBACK);

		if (xml_node)
		{
			root_node = mxmlFindElement (xml_node, xml_node, "root", NULL, NULL, MXML_DESCEND);

			if (root_node)
			{
				is_success = _app_profile_load_check_node (root_node, type);
			}

			mxmlDelete (xml_node);
		}

		CloseHandle (hfile);

		return is_success;
	}

	return FALSE;
}

VOID _app_profile_load_fallback ()
{
	PITEM_APP ptr_app;

	if (!_r_obj_findhashtable (apps, config.my_hash))
	{
		ptr_app = _app_addapplication (NULL, DataUnknown, _r_sys_getimagepathname (), NULL, NULL);

		if (ptr_app)
			_app_setappinfo (ptr_app, InfoIsEnabled, IntToPtr (TRUE));
	}

	_app_setappinfobyhash (config.my_hash, InfoIsUndeletable, IntToPtr (TRUE));

	// disable deletion for this shit ;)
	if (!_r_config_getboolean (L"IsInternalRulesDisabled", FALSE))
	{
		if (!_r_obj_findhashtable (apps, config.ntoskrnl_hash))
			_app_addapplication (NULL, DataUnknown, PROC_SYSTEM_NAME, NULL, NULL);

		if (!_r_obj_findhashtable (apps, config.svchost_hash))
			_app_addapplication (NULL, DataUnknown, _r_obj_getstring (config.svchost_path), NULL, NULL);

		_app_setappinfobyhash (config.ntoskrnl_hash, InfoIsUndeletable, IntToPtr (TRUE));
		_app_setappinfobyhash (config.svchost_hash, InfoIsUndeletable, IntToPtr (TRUE));
	}
}

VOID _app_profile_load_helper (mxml_node_t* root_node, ENUM_TYPE_DATA type, UINT version)
{
	PITEM_APP ptr_app = NULL;
	PITEM_RULE ptr_rule = NULL;
	PITEM_RULE_CONFIG ptr_config = NULL;
	PR_STRING string = NULL;
	SIZE_T rule_hash = 0;
	LPCSTR text;
	INT blocklist_spy_state;
	INT blocklist_update_state;
	INT blocklist_extra_state;

	blocklist_spy_state = _r_calc_clamp (_r_config_getinteger (L"BlocklistSpyState", 2), 0, 2);
	blocklist_update_state = _r_calc_clamp (_r_config_getinteger (L"BlocklistUpdateState", 0), 0, 2);
	blocklist_extra_state = _r_calc_clamp (_r_config_getinteger (L"BlocklistExtraState", 0), 0, 2);

	for (mxml_node_t* item_node = mxmlGetFirstChild (root_node); item_node; item_node = mxmlGetNextSibling (item_node))
	{
		if (type == DataAppRegular)
		{
			text = mxmlElementGetAttr (item_node, "path");

			if (_r_str_isempty_a (text))
				continue;

			string = _r_str_multibyte2unicode (text);

			if (!string)
				continue;

			ptr_app = _app_addapplication (NULL, DataUnknown, string->buffer, NULL, NULL);

			if (ptr_app)
			{
				LONG64 time;

				text = mxmlElementGetAttr (item_node, "is_silent");

				if (text)
					_app_setappinfo (ptr_app, InfoIsSilent, IntToPtr (_r_str_toboolean_a (text)));

				text = mxmlElementGetAttr (item_node, "is_enabled");

				if (text)
					_app_setappinfo (ptr_app, InfoIsEnabled, IntToPtr (_r_str_toboolean_a (text)));

				text = mxmlElementGetAttr (item_node, "timestamp");

				if (text)
				{
					time = _r_str_tolong64_a (text);
					_app_setappinfo (ptr_app, InfoTimestampPtr, &time);
				}

				text = mxmlElementGetAttr (item_node, "timer");

				if (text)
				{
					time = _r_str_tolong64_a (text);
					_app_setappinfo (ptr_app, InfoTimerPtr, &time);
				}
			}

			_r_obj_dereference (string);
		}
		else if (type == DataRuleBlocklist || type == DataRuleSystem || type == DataRuleUser)
		{
			BOOLEAN is_internal = FALSE;

			text = mxmlElementGetAttr (item_node, "name");

			if (_r_str_isempty_a (text))
				continue;

			string = _r_str_multibyte2unicode (text);

			if (!string)
				continue;

			PR_STRING rule_remote = _r_str_multibyte2unicode (mxmlElementGetAttr (item_node, "rule"));
			PR_STRING rule_local = _r_str_multibyte2unicode (mxmlElementGetAttr (item_node, "rule_local"));
			FWP_DIRECTION direction = (FWP_DIRECTION)_r_str_tointeger_a (mxmlElementGetAttr (item_node, "dir"));
			UINT8 protocol = (UINT8)_r_str_tointeger_a (mxmlElementGetAttr (item_node, "protocol"));
			ADDRESS_FAMILY af = (ADDRESS_FAMILY)_r_str_tointeger_a (mxmlElementGetAttr (item_node, "version"));
			PITEM_RULE ptr_rule = _app_addrule (string, rule_remote, rule_local, direction, protocol, af);

			_r_obj_dereference (string);

			if (rule_remote)
				_r_obj_dereference (rule_remote);

			if (rule_local)
				_r_obj_dereference (rule_local);

			if (!ptr_rule)
				continue;

			rule_hash = _r_obj_getstringhash (ptr_rule->name);

			ptr_rule->type = ((type == DataRuleSystem && _r_str_toboolean_a (mxmlElementGetAttr (item_node, "is_custom"))) ? DataRuleUser : type);
			ptr_rule->is_block = _r_str_toboolean_a (mxmlElementGetAttr (item_node, "is_block"));
			ptr_rule->is_forservices = _r_str_toboolean_a (mxmlElementGetAttr (item_node, "is_services"));
			ptr_rule->is_readonly = (type != DataRuleUser);

			// calculate rule weight
			if (type == DataRuleBlocklist)
			{
				ptr_rule->weight = FILTER_WEIGHT_BLOCKLIST;
			}
			else if (type == DataRuleSystem)
			{
				ptr_rule->weight = FILTER_WEIGHT_SYSTEM;
			}
			else if (type == DataRuleUser)
			{
				ptr_rule->weight = ptr_rule->is_block ? FILTER_WEIGHT_CUSTOM_BLOCK : FILTER_WEIGHT_CUSTOM;
			}

			ptr_rule->is_enabled = _r_str_toboolean_a (mxmlElementGetAttr (item_node, "is_enabled"));

			if (type == DataRuleBlocklist)
			{
				_app_ruleblocklistsetstate (ptr_rule, blocklist_spy_state, blocklist_update_state, blocklist_extra_state);
			}
			else
			{
				ptr_rule->is_enabled_default = ptr_rule->is_enabled; // set default value for rule
			}

			// reset (required!)
			ptr_config = NULL;

			// load rules config
			is_internal = (type == DataRuleBlocklist || type == DataRuleSystem);

			if (is_internal)
			{
				// internal rules
				ptr_config = _r_obj_findhashtable (rules_config, rule_hash);

				if (ptr_config)
				{
					ptr_rule->is_enabled = ptr_config->is_enabled;
				}
			}

			// load apps
			{
				R_STRINGBUILDER rule_apps;

				_r_obj_initializestringbuilder (&rule_apps);

				text = mxmlElementGetAttr (item_node, "apps");

				if (!_r_str_isempty_a (text))
				{
					string = _r_str_multibyte2unicode (text);

					if (string)
					{
						_r_obj_appendstringbuilder2 (&rule_apps, string);

						_r_obj_dereference (string);
					}
				}

				if (is_internal && ptr_config && !_r_obj_isstringempty (ptr_config->apps))
				{
					if (!_r_obj_isstringempty (_r_obj_finalstringbuilder (&rule_apps)))
					{
						_r_obj_appendstringbuilderformat (&rule_apps, L"%s%s", DIVIDER_APP, ptr_config->apps->buffer);
					}
					else
					{
						_r_obj_appendstringbuilder2 (&rule_apps, ptr_config->apps);
					}
				}

				string = _r_obj_finalstringbuilder (&rule_apps);

				if (!_r_obj_isstringempty (string))
				{
					if (version < XML_PROFILE_VER_3)
						_r_str_replacechar (string->buffer, _r_obj_getstringlength (string), DIVIDER_RULE[0], DIVIDER_APP[0]); // for compat with old profiles

					R_STRINGREF remaining_part;
					PR_STRING expanded_path;
					SIZE_T app_hash;

					_r_obj_initializestringref2 (&remaining_part, string);

					while (remaining_part.length != 0)
					{
						PR_STRING path_string = _r_str_splitatchar (&remaining_part, &remaining_part, DIVIDER_APP[0]);

						if (path_string)
						{
							expanded_path = _r_str_expandenvironmentstring (path_string->buffer);

							if (expanded_path)
								_r_obj_movereference (&path_string, expanded_path);

							app_hash = _r_obj_getstringhash (path_string);

							if (app_hash)
							{
								if (_r_str_toboolean_a (mxmlElementGetAttr (item_node, "is_services")) && (app_hash == config.ntoskrnl_hash || app_hash == config.svchost_hash))
								{
									_r_obj_dereference (path_string);
									continue;
								}

								ptr_app = _r_obj_findhashtable (apps, app_hash);

								if (!ptr_app)
								{
									ptr_app = _app_addapplication (NULL, DataUnknown, _r_obj_getstring (path_string), NULL, NULL);

									if (ptr_app)
										app_hash = ptr_app->app_hash;
								}

								if (ptr_rule->type == DataRuleSystem && ptr_app)
									_app_setappinfo (ptr_app, InfoIsUndeletable, IntToPtr (TRUE));

								_app_addcachetable (ptr_rule->apps, app_hash, NULL, 0);
							}

							_r_obj_dereference (path_string);
						}
					}
				}

				_r_obj_deletestringbuilder (&rule_apps);
			}

			_r_obj_addarrayitem (rules_arr, ptr_rule);
		}
		else if (type == DataRulesConfig)
		{
			text = mxmlElementGetAttr (item_node, "name");

			if (_r_str_isempty_a (text))
				continue;

			string = _r_str_multibyte2unicode (text);

			if (!string)
				continue;

			rule_hash = _r_obj_getstringhash (string);

			if (rule_hash)
			{
				ptr_config = _r_obj_findhashtable (rules_config, rule_hash);

				if (!ptr_config)
				{
					ptr_config = _app_addruleconfigtable (rules_config, rule_hash, _r_obj_reference (string), _r_str_toboolean_a (mxmlElementGetAttr (item_node, "is_enabled")));

					text = mxmlElementGetAttr (item_node, "apps");

					if (!_r_str_isempty_a (text))
					{
						ptr_config->apps = _r_str_multibyte2unicode (text);

						if (ptr_config->apps && version < XML_PROFILE_VER_3)
							_r_str_replacechar (ptr_config->apps->buffer, _r_obj_getstringlength (ptr_config->apps), DIVIDER_RULE[0], DIVIDER_APP[0]); // for compat with old profiles
					}
				}
			}

			_r_obj_dereference (string);
		}
	}
}

VOID _app_profile_load_internal (LPCWSTR path, LPCWSTR resource_name, PLONG64 ptimestamp)
{
	HANDLE hfile;

	mxml_node_t *xml_file_node = NULL;
	mxml_node_t *xml_resource_node = NULL;

	mxml_node_t *file_root_node = NULL;
	mxml_node_t *resource_root_node = NULL;

	mxml_node_t *root_node = NULL;

	LONG64 timestamp_file = 0;
	LONG64 timestamp_resource = 0;

	if (path)
	{
		hfile = CreateFile (path, FILE_GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (_r_fs_isvalidhandle (hfile))
		{
			xml_file_node = mxmlLoadFd (NULL, hfile, MXML_OPAQUE_CALLBACK);

			if (xml_file_node)
			{
				file_root_node = mxmlFindElement (xml_file_node, xml_file_node, "root", NULL, NULL, MXML_DESCEND);
			}

			CloseHandle (hfile);
		}
	}

	if (resource_name)
	{
		PVOID pbuffer = _r_loadresource (NULL, resource_name, RT_RCDATA, NULL);

		if (pbuffer)
		{
			xml_resource_node = mxmlLoadString (NULL, pbuffer, MXML_OPAQUE_CALLBACK);

			if (xml_resource_node)
			{
				resource_root_node = mxmlFindElement (xml_resource_node, xml_resource_node, "root", NULL, NULL, MXML_DESCEND);
			}
		}
	}

	if (file_root_node)
	{
		timestamp_file = _r_str_tolong64_a (mxmlElementGetAttr (file_root_node, "timestamp"));
	}

	if (resource_root_node)
	{
		timestamp_resource = _r_str_tolong64_a (mxmlElementGetAttr (resource_root_node, "timestamp"));
	}

	root_node = (timestamp_file > timestamp_resource) ? file_root_node : resource_root_node;

	if (!root_node)
	{
		_r_log (Error, UID, L"mxmlLoadFd", GetLastError (), path);
	}
	else
	{
		if (_app_profile_load_check_node (root_node, XmlProfileInternalV3))
		{
			INT version = _r_str_tointeger_a (mxmlElementGetAttr (root_node, "version"));

			if (ptimestamp)
			{
				*ptimestamp = _r_str_tolong64_a (mxmlElementGetAttr (root_node, "timestamp"));
			}

			mxml_node_t *rules_system_node;
			mxml_node_t *rules_blocklist_node;

			rules_system_node = mxmlFindElement (root_node, root_node, "rules_system", NULL, NULL, MXML_DESCEND);
			rules_blocklist_node = mxmlFindElement (root_node, root_node, "rules_blocklist", NULL, NULL, MXML_DESCEND);

			// load system rules
			if (rules_system_node)
			{
				_app_profile_load_helper (rules_system_node, DataRuleSystem, version);
			}

			// load blocklist
			if (rules_blocklist_node)
			{
				_app_profile_load_helper (rules_blocklist_node, DataRuleBlocklist, version);
			}
		}
	}

	if (xml_file_node)
		mxmlDelete (xml_file_node);

	if (xml_resource_node)
		mxmlDelete (xml_resource_node);
}

VOID _app_profile_load (HWND hwnd, LPCWSTR path_custom)
{
	INT current_listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, -1);
	INT selected_item = (INT)SendDlgItemMessage (hwnd, current_listview_id, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
	INT scroll_pos = GetScrollPos (GetDlgItem (hwnd, current_listview_id), SB_VERT);

	// clean listview
	if (hwnd)
	{
		for (INT i = IDC_APPS_PROFILE; i <= IDC_RULES_CUSTOM; i++)
			_r_listview_deleteallitems (hwnd, i);
	}

	_r_spinlock_acquireexclusive (&lock_apply);

	// clear apps
	_r_obj_clearhashtable (apps);

	// clear rules config
	_r_obj_clearhashtable (rules_config);

	// clear rules
	_r_obj_cleararray (rules_arr);

	// generate uwp apps list (win8+)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
		_app_generate_packages ();

	// generate services list
	_app_generate_services ();

	_r_spinlock_releaseexclusive (&lock_apply);

	// load profile
	HANDLE hfile;

	mxml_node_t *xml_node = NULL;
	mxml_node_t *root_node = NULL;

	hfile = CreateFile (path_custom ? path_custom : config.profile_path, FILE_GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (_r_fs_isvalidhandle (hfile))
	{
		xml_node = mxmlLoadFd (NULL, hfile, MXML_OPAQUE_CALLBACK);
		CloseHandle (hfile);
	}

	// load backup
	if (!xml_node && !path_custom)
	{
		hfile = CreateFile (config.profile_path_backup, FILE_GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (_r_fs_isvalidhandle (hfile))
		{
			xml_node = mxmlLoadFd (NULL, hfile, MXML_OPAQUE_CALLBACK);
			CloseHandle (hfile);
		}
	}

	if (!xml_node)
	{
		_r_log (Error, UID, L"mxmlLoadFd", GetLastError (), path_custom ? path_custom : config.profile_path);
	}
	else
	{
		if (mxmlGetType (xml_node) == MXML_ELEMENT)
		{
			root_node = mxmlFindElement (xml_node, xml_node, "root", NULL, NULL, MXML_DESCEND);

			if (root_node)
			{
				if (_app_profile_load_check_node (root_node, XmlProfileV3))
				{
					INT version = _r_str_tointeger_a (mxmlElementGetAttr (root_node, "version"));

					mxml_node_t *apps_node;
					mxml_node_t *rules_config_node;
					mxml_node_t *rules_custom_node;

					apps_node = mxmlFindElement (root_node, root_node, "apps", NULL, NULL, MXML_DESCEND);
					rules_config_node = mxmlFindElement (root_node, root_node, "rules_config", NULL, NULL, MXML_DESCEND);
					rules_custom_node = mxmlFindElement (root_node, root_node, "rules_custom", NULL, NULL, MXML_DESCEND);

					// load apps
					if (apps_node)
					{
						_app_profile_load_helper (apps_node, DataAppRegular, version);
					}

					// load rules config
					if (rules_config_node)
					{
						_app_profile_load_helper (rules_config_node, DataRulesConfig, version);
					}

					// load user rules
					if (rules_custom_node)
					{
						_app_profile_load_helper (rules_custom_node, DataRuleUser, version);
					}
				}
			}
		}

		mxmlDelete (xml_node);
	}

	_app_profile_load_fallback ();

	// load internal rules (new!)
	if (!_r_config_getboolean (L"IsInternalRulesDisabled", FALSE))
		_app_profile_load_internal (config.profile_internal_path, MAKEINTRESOURCE (IDR_PROFILE_INTERNAL), &config.profile_internal_timestamp);

	if (hwnd)
	{
		PITEM_APP ptr_app;
		PITEM_RULE ptr_rule;
		SIZE_T enum_key = 0;
		LONG64 current_time = _r_unixtime_now ();
		INT listview_id;

		// add apps
		_r_spinlock_acquireshared (&lock_apps);

		while (_r_obj_enumhashtable (apps, &ptr_app, NULL, &enum_key))
		{
			listview_id = _app_getlistview_id (ptr_app->type);

			if (listview_id)
			{
				_r_spinlock_acquireshared (&lock_checkbox);

				_r_listview_additemex (hwnd, listview_id, 0, 0, SZ_EMPTY, ptr_app->icon_id, _app_getappgroup (ptr_app), ptr_app->app_hash);
				_app_setappiteminfo (hwnd, listview_id, 0, ptr_app);

				_r_spinlock_releaseshared (&lock_checkbox);
			}

			// install timer
			if (ptr_app->timer)
				_app_timer_set (hwnd, ptr_app, ptr_app->timer - current_time);
		}

		_r_spinlock_releaseshared (&lock_apps);

		// add rules
		_r_spinlock_acquireshared (&lock_rules);

		for (SIZE_T i = 0; i < _r_obj_getarraysize (rules_arr); i++)
		{
			ptr_rule = _r_obj_getarrayitem (rules_arr, i);

			if (!ptr_rule)
				continue;

			listview_id = _app_getlistview_id (ptr_rule->type);

			if (listview_id)
			{
				_r_spinlock_acquireshared (&lock_checkbox);

				_r_listview_additemex (hwnd, listview_id, 0, 0, SZ_EMPTY, _app_getruleicon (ptr_rule), _app_getrulegroup (ptr_rule), i);
				_app_setruleiteminfo (hwnd, listview_id, 0, ptr_rule, FALSE);

				_r_spinlock_releaseshared (&lock_checkbox);
			}
		}

		_r_spinlock_releaseshared (&lock_rules);
	}

	if (hwnd && current_listview_id)
	{
		INT new_listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, -1);

		if (new_listview_id)
		{
			_app_listviewsort (hwnd, new_listview_id, -1, FALSE);

			if (current_listview_id == new_listview_id)
				_app_showitem (hwnd, current_listview_id, selected_item, scroll_pos);
		}
	}
}

LPCSTR _app_profile_save_callback (mxml_node_t *node, INT position)
{
	LPCSTR element_name = mxmlGetElement (node);

	if (!element_name)
		return NULL;

	if (position == MXML_WS_BEFORE_OPEN || position == MXML_WS_BEFORE_CLOSE)
	{
		if (_stricmp (element_name, "apps") == 0 || _stricmp (element_name, "rules_custom") == 0 || _stricmp (element_name, "rules_config") == 0)
		{
			return "\t";
		}
		else if (_stricmp (element_name, "item") == 0)
		{
			return "\t\t";
		}
	}
	else if (position == MXML_WS_AFTER_OPEN || position == MXML_WS_AFTER_CLOSE)
	{
		return "\r\n";
	}

	return NULL;
}

VOID _app_profile_save ()
{
	HANDLE hfile = NULL;

	mxml_node_t* xml_node = NULL;
	mxml_node_t* root_node = NULL;
	mxml_node_t* apps_node = NULL;
	mxml_node_t* rules_custom_node = NULL;
	mxml_node_t* rules_config_node = NULL;
	mxml_node_t* item_node = NULL;

	PR_STRING string = NULL;
	LONG64 current_time = 0;
	BOOLEAN is_backuprequired = FALSE;

	hfile = CreateFile (config.profile_path, FILE_GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (!_r_fs_isvalidhandle (hfile))
	{
		_r_log (Error, UID, L"CreateFile", GetLastError (), config.profile_path);
		return;
	}

	mxmlSetWrapMargin (0x2000);

	current_time = _r_unixtime_now ();
	is_backuprequired = _r_config_getboolean (L"IsBackupProfile", TRUE) && (!_r_fs_exists (config.profile_path_backup) || ((current_time - _r_config_getlong64 (L"BackupTimestamp", 0)) >= _r_config_getlong64 (L"BackupPeriod", BACKUP_HOURS_PERIOD)));

	xml_node = mxmlNewXML ("1.0");
	root_node = mxmlNewElement (xml_node, "root");

	mxmlElementSetAttrf (root_node, "timestamp", "%" PR_LONG64, current_time);
	mxmlElementSetAttrf (root_node, "type", "%" PRIi32, XmlProfileV3);
	mxmlElementSetAttrf (root_node, "version", "%" PRIi32, XML_PROFILE_VER_CURRENT);

	apps_node = mxmlNewElement (root_node, "apps");
	rules_custom_node = mxmlNewElement (root_node, "rules_custom");
	rules_config_node = mxmlNewElement (root_node, "rules_config");

	// save apps
	PITEM_APP ptr_app;
	PITEM_RULE_CONFIG ptr_config;
	SIZE_T enum_key = 0;
	BOOLEAN is_keepunusedapps = _r_config_getboolean (L"IsKeepUnusedApps", TRUE);
	BOOLEAN is_usedapp = FALSE;

	while (_r_obj_enumhashtable (apps, &ptr_app, NULL, &enum_key))
	{
		if (_r_obj_isstringempty (ptr_app->original_path))
			continue;

		is_usedapp = _app_isappused (ptr_app);

		// do not save unused apps/uwp apps...
		if (!is_usedapp && (!is_keepunusedapps || (ptr_app->type == DataAppService || ptr_app->type == DataAppUWP)))
			continue;

		item_node = mxmlNewElement (apps_node, "item");

		mxmlElementSetAttrf (item_node, "path", "%ws", ptr_app->original_path->buffer);

		if (ptr_app->timestamp)
			mxmlElementSetAttrf (item_node, "timestamp", "%" PR_LONG64, ptr_app->timestamp);

		// set timer (if presented)
		if (ptr_app->timer && _app_istimerset (ptr_app->htimer))
			mxmlElementSetAttrf (item_node, "timer", "%" PR_LONG64, ptr_app->timer);

		// ffu!
		if (ptr_app->profile)
			mxmlElementSetAttrf (item_node, "profile", "%" PRIu8, ptr_app->profile);

		if (ptr_app->is_silent)
			mxmlElementSetAttrf (item_node, "is_silent", "%" PRIu8, ptr_app->is_silent);

		if (ptr_app->is_enabled)
			mxmlElementSetAttrf (item_node, "is_enabled", "%" PRIu8, ptr_app->is_enabled);
	}

	// save user rules
	for (SIZE_T i = 0; i < _r_obj_getarraysize (rules_arr); i++)
	{
		PITEM_RULE ptr_rule = _r_obj_getarrayitem (rules_arr, i);

		if (!ptr_rule || ptr_rule->is_readonly || _r_obj_isstringempty (ptr_rule->name))
			continue;

		item_node = mxmlNewElement (rules_custom_node, "item");

		mxmlElementSetAttrf (item_node, "name", "%ws", ptr_rule->name->buffer);

		if (!_r_obj_isstringempty (ptr_rule->rule_remote))
			mxmlElementSetAttrf (item_node, "rule", "%ws", ptr_rule->rule_remote->buffer);

		if (!_r_obj_isstringempty (ptr_rule->rule_local))
			mxmlElementSetAttrf (item_node, "rule_local", "%ws", ptr_rule->rule_local->buffer);

		// ffu!
		if (ptr_rule->profile)
			mxmlElementSetAttrf (item_node, "profile", "%" PRIu8, ptr_rule->profile);

		if (ptr_rule->direction != FWP_DIRECTION_OUTBOUND)
			mxmlElementSetAttrf (item_node, "dir", "%" PRIu8, ptr_rule->direction);

		if (ptr_rule->protocol != 0)
			mxmlElementSetAttrf (item_node, "protocol", "%" PRIu8, ptr_rule->protocol);

		if (ptr_rule->af != AF_UNSPEC)
			mxmlElementSetAttrf (item_node, "version", "%" PRIu8, ptr_rule->af);

		// add apps attribute
		if (!_r_obj_ishashtableempty (ptr_rule->apps))
		{
			string = _app_rulesexpandapps (ptr_rule, FALSE, DIVIDER_APP);

			if (string)
			{
				if (!_r_obj_isstringempty (string))
					mxmlElementSetAttrf (item_node, "apps", "%ws", string->buffer);

				_r_obj_clearreference (&string);
			}
		}

		if (ptr_rule->is_block)
			mxmlElementSetAttrf (item_node, "is_block", "%" PRIu8, ptr_rule->is_block);

		if (ptr_rule->is_enabled)
			mxmlElementSetAttrf (item_node, "is_enabled", "%" PRIu8, ptr_rule->is_enabled);
	}

	// save rules config
	enum_key = 0;

	while (_r_obj_enumhashtable (rules_config, &ptr_config, NULL, &enum_key))
	{
		if (_r_obj_isstringempty (ptr_config->name))
			continue;

		BOOLEAN is_enabled_default = ptr_config->is_enabled;
		SIZE_T rule_hash = _r_obj_getstringhash (ptr_config->name);
		PITEM_RULE ptr_rule = _app_getrulebyhash (rule_hash);

		if (ptr_rule)
		{
			is_enabled_default = ptr_rule->is_enabled_default;

			if (ptr_rule->type == DataRuleUser && !_r_obj_ishashtableempty (ptr_rule->apps))
			{
				string = _app_rulesexpandapps (ptr_rule, FALSE, DIVIDER_APP);
			}
		}

		// skip saving untouched configuration
		if (ptr_config->is_enabled == is_enabled_default && _r_obj_isstringempty (string))
		{
			if (string)
				_r_obj_clearreference (&string);

			continue;
		}

		item_node = mxmlNewElement (rules_config_node, "item");

		mxmlElementSetAttrf (item_node, "name", "%ws", ptr_config->name->buffer);

		if (string)
		{
			if (!_r_obj_isstringempty (string))
				mxmlElementSetAttrf (item_node, "apps", "%ws", string->buffer);

			_r_obj_clearreference (&string);
		}

		mxmlElementSetAttrf (item_node, "is_enabled", "%" PRIu8, ptr_config->is_enabled);
	}

	mxmlSaveFd (xml_node, hfile, &_app_profile_save_callback);
	mxmlDelete (xml_node);

	CloseHandle (hfile);

	// make backup
	if (is_backuprequired)
	{
		_r_fs_copy (config.profile_path, config.profile_path_backup, 0);
		_r_config_setlong64 (L"BackupTimestamp", current_time);
	}
}
