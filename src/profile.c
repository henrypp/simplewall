// simplewall
// Copyright (c) 2016-2021 Henry++

#include "global.h"

_Ret_maybenull_
PVOID _app_getappinfo (_In_ PITEM_APP ptr_app, _In_ ENUM_INFO_DATA info_data)
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

_Ret_maybenull_
PVOID _app_getappinfobyhash (_In_ SIZE_T app_hash, _In_ ENUM_INFO_DATA info_data)
{
	PITEM_APP ptr_app;

	ptr_app = _app_getappitem (app_hash);

	if (!ptr_app)
		return NULL;

	return _app_getappinfo (ptr_app, info_data);
}

VOID _app_setappinfo (_In_ PITEM_APP ptr_app, _In_ ENUM_INFO_DATA info_data, _In_ PVOID value)
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

VOID _app_setappinfobyhash (_In_ SIZE_T app_hash, _In_ ENUM_INFO_DATA info_data, _In_ PVOID value)
{
	PITEM_APP ptr_app = _app_getappitem (app_hash);

	if (ptr_app)
		_app_setappinfo (ptr_app, info_data, value);
}

_Ret_maybenull_
PVOID _app_getruleinfo (_In_ PITEM_RULE ptr_rule, _In_ ENUM_INFO_DATA info_data)
{
	if (info_data == InfoListviewId)
	{
		return IntToPtr (_app_getlistview_id (ptr_rule->type));
	}
	else if (info_data == InfoIsReadonly)
	{
		return IntToPtr (ptr_rule->is_readonly ? TRUE : FALSE);
	}

	return NULL;
}

_Ret_maybenull_
PVOID _app_getruleinfobyid (_In_ SIZE_T index, _In_ ENUM_INFO_DATA info_data)
{
	PITEM_RULE ptr_rule = _app_getrulebyid (index);

	if (ptr_rule)
		return _app_getruleinfo (ptr_rule, info_data);

	return NULL;
}

_Ret_maybenull_
PITEM_APP _app_addapplication (_In_opt_ HWND hwnd, _In_ ENUM_TYPE_DATA type, _In_ LPCWSTR path, _In_opt_ PR_STRING display_name, _In_opt_ PR_STRING real_path)
{
	if (_r_str_isempty (path) || PathIsDirectory (path))
		return NULL;

	WCHAR path_full[1024];
	PITEM_APP ptr_app;
	PITEM_APP ptr_app_added;
	PR_STRING signature_string;
	SIZE_T path_length;
	SIZE_T app_hash;
	BOOLEAN is_ntoskrnl;

	path_length = _r_str_length (path);

	// prevent possible duplicate apps entries with short path (issue #640)
	if (_r_str_findchar (path, L'~') != SIZE_MAX)
	{
		if (GetLongPathName (path, path_full, RTL_NUMBER_OF (path_full)))
		{
			path = path_full;
			path_length = _r_str_length (path_full);
		}
	}

	app_hash = _r_str_hash (path);
	ptr_app = _app_getappitem (app_hash);

	if (ptr_app)
		return ptr_app; // already exists

	ptr_app = _r_mem_allocatezero (sizeof (ITEM_APP));
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
		if (!is_ntoskrnl && _r_str_findchar (path, OBJ_NAME_PATH_SEPARATOR) == SIZE_MAX)
		{
			ptr_app->type = DataAppPico;
		}
		else
		{
			ptr_app->type = PathIsNetworkPath (path) ? DataAppNetwork : DataAppRegular;
		}

		ptr_app->real_path = is_ntoskrnl ? _r_obj_createstringfromstring (config.ntoskrnl_path) : _r_obj_createstringex (path, path_length * sizeof (WCHAR));
	}

	ptr_app->original_path = _r_obj_createstringex (path, path_length * sizeof (WCHAR));

	// fix "System" lowercase
	if (is_ntoskrnl)
	{
		_r_str_tolower (ptr_app->original_path->buffer);
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

	// insert object into the table
	_r_spinlock_acquireexclusive (&lock_apps);

	ptr_app_added = _r_obj_addhashtableitem (apps, app_hash, ptr_app);

	_r_spinlock_releaseexclusive (&lock_apps);

	_r_mem_free (ptr_app);

	if (!ptr_app_added)
		return NULL;

	// insert item
	if (hwnd)
	{
		INT listview_id = _app_getlistview_id (ptr_app_added->type);

		if (listview_id)
		{
			_r_spinlock_acquireshared (&lock_checkbox);

			_r_listview_additemex (hwnd, listview_id, 0, 0, SZ_EMPTY, 0, 0, app_hash);
			_app_setappiteminfo (hwnd, listview_id, 0, ptr_app_added);

			_r_spinlock_releaseshared (&lock_checkbox);
		}
	}

	return ptr_app_added;
}

PITEM_RULE _app_addrule (_In_opt_ PR_STRING name, _In_opt_ PR_STRING rule_remote, _In_opt_ PR_STRING rule_local, _In_ FWP_DIRECTION direction, _In_ UINT8 protocol, _In_ ADDRESS_FAMILY af)
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

PITEM_RULE_CONFIG _app_addruleconfigtable (_In_ PR_HASHTABLE hashtable, _In_ SIZE_T rule_hash, _In_opt_ PR_STRING name, _In_ BOOLEAN is_enabled)
{
	ITEM_RULE_CONFIG entry = {0};

	entry.name = name;
	entry.is_enabled = is_enabled;

	return _r_obj_addhashtableitem (hashtable, rule_hash, &entry);
}

_Ret_maybenull_
PITEM_APP _app_getappitem (_In_ SIZE_T app_hash)
{
	PITEM_APP ptr_app;

	_r_spinlock_acquireshared (&lock_apps);

	ptr_app = _r_obj_findhashtable (apps, app_hash);

	_r_spinlock_releaseshared (&lock_apps);

	return ptr_app;
}

_Ret_maybenull_
PITEM_RULE _app_getrulebyid (_In_ SIZE_T index)
{
	PITEM_RULE ptr_rule = NULL;

	_r_spinlock_acquireshared (&lock_rules);

	if (index != SIZE_MAX && index < _r_obj_getarraysize (rules_arr))
		ptr_rule = _r_obj_getarrayitem (rules_arr, index);

	_r_spinlock_releaseshared (&lock_rules);

	return ptr_rule;
}

_Ret_maybenull_
PITEM_RULE _app_getrulebyhash (_In_ SIZE_T rule_hash)
{
	PITEM_RULE ptr_rule;

	if (!rule_hash)
		return NULL;

	_r_spinlock_acquireshared (&lock_rules);

	for (SIZE_T i = 0; i < _r_obj_getarraysize (rules_arr); i++)
	{
		ptr_rule = _r_obj_getarrayitem (rules_arr, i);

		if (ptr_rule)
		{
			if (ptr_rule->is_readonly)
			{
				if (_r_obj_getstringhash (ptr_rule->name) == rule_hash)
				{
					_r_spinlock_releaseshared (&lock_rules);

					return ptr_rule;
				}
			}
		}
	}

	_r_spinlock_releaseshared (&lock_rules);

	return NULL;
}

_Ret_maybenull_
PITEM_RULE_CONFIG _app_getruleconfigitem (_In_ SIZE_T rule_hash)
{
	PITEM_RULE_CONFIG ptr_rule_config;

	if (!rule_hash)
		return NULL;

	_r_spinlock_acquireshared (&lock_rules_config);

	ptr_rule_config = _r_obj_findhashtable (rules_config, rule_hash);

	_r_spinlock_releaseshared (&lock_rules_config);

	return ptr_rule_config;
}

SIZE_T _app_getnetworkapp (_In_ SIZE_T network_hash)
{
	PITEM_NETWORK ptr_network = _app_getnetworkitem (network_hash);

	if (ptr_network)
		return ptr_network->app_hash;

	return 0;
}

_Ret_maybenull_
PITEM_NETWORK _app_getnetworkitem (_In_ SIZE_T network_hash)
{
	PITEM_NETWORK ptr_network;

	_r_spinlock_acquireshared (&lock_network);

	ptr_network = _r_obj_findhashtable (network_table, network_hash);

	_r_spinlock_releaseshared (&lock_network);

	return ptr_network;
}

_Ret_maybenull_
PITEM_LOG _app_getlogitem (_In_ SIZE_T index)
{
	PITEM_LOG ptr_log = NULL;

	_r_spinlock_acquireshared (&lock_loglist);

	if (index != SIZE_MAX && index < _r_obj_getlistsize (log_arr))
	{
		ptr_log = _r_obj_getlistitem (log_arr, index);
	}

	_r_spinlock_releaseshared (&lock_loglist);

	return _r_obj_referencesafe (ptr_log);
}

SIZE_T _app_getlogapp (_In_ SIZE_T index)
{
	PITEM_LOG ptr_log = _app_getlogitem (index);

	if (ptr_log)
	{
		SIZE_T app_hash = ptr_log->app_hash;

		_r_obj_dereference (ptr_log);

		return app_hash;
	}

	return 0;
}

COLORREF _app_getappcolor (_In_ INT listview_id, _In_ SIZE_T app_hash, _In_ BOOLEAN is_systemapp, _In_ BOOLEAN is_validconnection)
{
	PITEM_APP ptr_app = _app_getappitem (app_hash);
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
		return _app_getcolorvalue (_r_str_hash (color_value));

	return 0;
}

VOID _app_freeapplication (_In_ SIZE_T app_hash)
{
	PITEM_RULE ptr_rule;

	_r_spinlock_acquireshared (&lock_rules);

	for (SIZE_T i = 0; i < _r_obj_getarraysize (rules_arr); i++)
	{
		ptr_rule = _r_obj_getarrayitem (rules_arr, i);

		if (!ptr_rule)
			continue;

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

	_r_spinlock_releaseshared (&lock_rules);

	_r_obj_removehashtableentry (apps, app_hash);
}

VOID _app_getcount (_Out_ PITEM_STATUS status)
{
	PITEM_APP ptr_app;
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

	_r_spinlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtable (apps, &ptr_app, NULL, &enum_key))
	{
		is_used = _app_isappused (ptr_app);

		if (_app_istimerset (ptr_app->htimer))
			status->apps_timer_count += 1;

		if (!ptr_app->is_undeletable && (!_app_isappexists (ptr_app) || !is_used) && !(ptr_app->type == DataAppService || ptr_app->type == DataAppUWP))
			status->apps_unused_count += 1;

		if (is_used)
			status->apps_count += 1;
	}

	_r_spinlock_releaseshared (&lock_apps);

	_r_spinlock_acquireshared (&lock_rules);

	for (SIZE_T i = 0; i < _r_obj_getarraysize (rules_arr); i++)
	{
		ptr_rule = _r_obj_getarrayitem (rules_arr, i);

		if (!ptr_rule)
			continue;

		if (ptr_rule->type == DataRuleUser)
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

	_r_spinlock_releaseshared (&lock_rules);
}

COLORREF _app_getrulecolor (_In_ INT listview_id, _In_ SIZE_T rule_idx)
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
		return _app_getcolorvalue (_r_str_hash (color_value));

	return 0;
}

_Ret_maybenull_
PR_STRING _app_gettooltip (_In_ HWND hwnd, _In_ INT listview_id, _In_ INT item_id)
{
	R_STRINGBUILDER buffer = {0};
	PR_STRING string;

	BOOLEAN is_appslist = (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP);
	BOOLEAN is_ruleslist = (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM) || listview_id == IDC_APP_RULES_ID;

	if (is_appslist || listview_id == IDC_RULE_APPS_ID)
	{
		SIZE_T app_hash = _r_listview_getitemlparam (hwnd, listview_id, item_id);
		PITEM_APP ptr_app = _app_getappitem (app_hash);

		if (ptr_app)
		{
			_r_obj_initializestringbuilder (&buffer);

			// app path
			{
				PR_STRING path_string = _r_format_string (L"%s\r\n", _r_obj_getstring (ptr_app->real_path ? ptr_app->real_path : ptr_app->display_name ? ptr_app->display_name : ptr_app->original_path));

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
			info_string = _r_format_string (L"%s (#%" PR_SIZE_T L")\r\n%s (" SZ_DIRECTION_REMOTE L"):\r\n%s%s\r\n%s (" SZ_DIRECTION_LOCAL L"):\r\n%s%s",
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
		PITEM_NETWORK ptr_network = _app_getnetworkitem (lparam);

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

VOID _app_setappiteminfo (_In_ HWND hwnd, _In_ INT listview_id, _In_ INT item_id, _Inout_ PITEM_APP ptr_app)
{
	if (!listview_id || item_id == -1)
		return;

	_app_getappicon (ptr_app, TRUE, &ptr_app->icon_id, NULL);

	_r_listview_setitemex (hwnd, listview_id, item_id, 0, _app_getdisplayname (ptr_app, FALSE), ptr_app->icon_id, _app_getappgroup (ptr_app), 0);

	WCHAR date_string[256];
	_r_format_unixtimeex (date_string, RTL_NUMBER_OF (date_string), ptr_app->timestamp, FDTF_SHORTDATE | FDTF_SHORTTIME);

	_r_listview_setitem (hwnd, listview_id, item_id, 1, date_string);

	_r_listview_setitemcheck (hwnd, listview_id, item_id, ptr_app->is_enabled);
}

VOID _app_setruleiteminfo (_In_ HWND hwnd, _In_ INT listview_id, _In_ INT item_id, _In_ PITEM_RULE ptr_rule, _In_ BOOLEAN include_apps)
{
	if (!listview_id || item_id == -1)
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

	_r_listview_setitemex (hwnd, listview_id, item_id, 0, rule_name_ptr, rule_icon_id, rule_group_id, 0);
	_r_listview_setitem (hwnd, listview_id, item_id, 1, ptr_rule->protocol ? _app_getprotoname (ptr_rule->protocol, AF_UNSPEC, NULL) : _r_locale_getstring (IDS_ANY));
	_r_listview_setitem (hwnd, listview_id, item_id, 2, _r_obj_getstringorempty (direction_string));

	_r_listview_setitemcheck (hwnd, listview_id, item_id, ptr_rule->is_enabled);

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
			ptr_app = _app_getappitem (hash_code);

			if (!ptr_app)
				continue;

			app_listview_id = _app_getlistview_id (ptr_app->type);

			if (app_listview_id)
			{
				item_pos = _app_getposition (hwnd, app_listview_id, ptr_app->app_hash);

				if (item_pos != -1)
					_app_setappiteminfo (hwnd, app_listview_id, item_pos, ptr_app);
			}
		}
	}
}

VOID _app_ruleenable (_Inout_ PITEM_RULE ptr_rule, _In_ BOOLEAN is_enable, _In_ BOOLEAN is_createconfig)
{
	ptr_rule->is_enabled = is_enable;

	if (ptr_rule->is_readonly && !_r_obj_isstringempty (ptr_rule->name))
	{
		PITEM_RULE_CONFIG ptr_config;
		SIZE_T rule_hash = _r_obj_getstringhash (ptr_rule->name);

		if (rule_hash)
		{
			ptr_config = _app_getruleconfigitem (rule_hash);

			if (ptr_config)
			{
				ptr_config->is_enabled = is_enable;

				return;
			}

			if (is_createconfig)
			{
				_r_spinlock_acquireexclusive (&lock_rules_config);

				ptr_config = _app_addruleconfigtable (rules_config, rule_hash, _r_obj_createstringfromstring (ptr_rule->name), is_enable);

				_r_spinlock_releaseexclusive (&lock_rules_config);
			}
		}
	}
}

BOOLEAN _app_ruleblocklistsetchange (_Inout_ PITEM_RULE ptr_rule, _In_ INT new_state)
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

BOOLEAN _app_ruleblocklistsetstate (_Inout_ PITEM_RULE ptr_rule, _In_ INT spy_state, _In_ INT update_state, _In_ INT extra_state)
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

VOID _app_ruleblocklistset (_In_opt_ HWND hwnd, _In_ INT spy_state, _In_ INT update_state, _In_ INT extra_state, _In_ BOOLEAN is_instantapply)
{
	PR_LIST rules = _r_obj_createlistex (0x200, NULL);
	PITEM_RULE ptr_rule;
	SIZE_T changes_count = 0;
	INT listview_id = _app_getlistview_id (DataRuleBlocklist);

	_r_spinlock_acquireshared (&lock_rules);

	for (SIZE_T i = 0; i < _r_obj_getarraysize (rules_arr); i++)
	{
		ptr_rule = _r_obj_getarrayitem (rules_arr, i);

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

	_r_spinlock_releaseshared (&lock_rules);

	if (changes_count)
	{
		if (hwnd)
		{
			if (listview_id == (INT)_r_tab_getitemlparam (hwnd, IDC_TAB, -1))
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

_Ret_maybenull_
PR_STRING _app_appexpandrules (_In_ SIZE_T app_hash, _In_ LPCWSTR delimeter)
{
	R_STRINGBUILDER buffer;
	PR_STRING string;
	PITEM_RULE ptr_rule;

	_r_obj_initializestringbuilder (&buffer);

	_r_spinlock_acquireshared (&lock_rules);

	for (SIZE_T i = 0; i < _r_obj_getarraysize (rules_arr); i++)
	{
		ptr_rule = _r_obj_getarrayitem (rules_arr, i);

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

	_r_spinlock_releaseshared (&lock_rules);

	string = _r_obj_finalstringbuilder (&buffer);

	_r_obj_trimstring (string, delimeter);

	if (!_r_obj_isstringempty (string))
		return string;

	_r_obj_deletestringbuilder (&buffer);

	return NULL;

}

_Ret_maybenull_
PR_STRING _app_rulesexpandapps (_In_ PITEM_RULE ptr_rule, _In_ BOOLEAN is_fordisplay, _In_ LPCWSTR delimeter)
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
		ptr_app = _app_getappitem (hash_code);

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

_Ret_maybenull_
PR_STRING _app_rulesexpandrules (_In_ PR_STRING rule, _In_ LPCWSTR delimeter)
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

BOOLEAN _app_isappfromsystem (_In_ LPCWSTR path, _In_ SIZE_T app_hash)
{
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

BOOLEAN _app_isapphaveconnection (_In_ SIZE_T app_hash)
{
	PITEM_NETWORK ptr_network;
	SIZE_T enum_key = 0;

	_r_spinlock_acquireshared (&lock_network);

	while (_r_obj_enumhashtable (network_table, &ptr_network, NULL, &enum_key))
	{
		if (ptr_network->app_hash == app_hash)
		{
			if (ptr_network->is_connection)
			{
				_r_spinlock_releaseshared (&lock_network);

				return TRUE;
			}
		}
	}

	_r_spinlock_releaseshared (&lock_network);

	return FALSE;
}

BOOLEAN _app_isapphavedrive (_In_ INT letter)
{
	PITEM_APP ptr_app;
	SIZE_T enum_key = 0;

	_r_spinlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtable (apps, &ptr_app, NULL, &enum_key))
	{
		if (!_r_obj_isstringempty (ptr_app->original_path))
		{
			INT drive_id = PathGetDriveNumber (ptr_app->original_path->buffer);

			if ((drive_id != -1 && drive_id == letter) || ptr_app->type == DataAppDevice)
			{
				if (ptr_app->is_enabled || _app_isapphaverule (ptr_app->app_hash, FALSE))
				{
					_r_spinlock_releaseshared (&lock_apps);

					return TRUE;
				}
			}
		}
	}

	_r_spinlock_releaseshared (&lock_apps);

	return FALSE;
}

BOOLEAN _app_isapphaverule (_In_ SIZE_T app_hash, _In_ BOOLEAN is_countdisabled)
{
	PITEM_RULE ptr_rule;

	_r_spinlock_acquireshared (&lock_rules);

	for (SIZE_T i = 0; i < _r_obj_getarraysize (rules_arr); i++)
	{
		ptr_rule = _r_obj_getarrayitem (rules_arr, i);

		if (ptr_rule)
		{
			if (ptr_rule->type == DataRuleUser && (is_countdisabled || (ptr_rule->is_enabled)))
			{
				if (_r_obj_findhashtable (ptr_rule->apps, app_hash))
				{
					_r_spinlock_releaseshared (&lock_rules);

					return TRUE;
				}
			}
		}
	}

	_r_spinlock_releaseshared (&lock_rules);

	return FALSE;
}

BOOLEAN _app_isappexists (_In_ PITEM_APP ptr_app)
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

VOID _app_openappdirectory (_In_ PITEM_APP ptr_app)
{
	PR_STRING path = _app_getappinfo (ptr_app, InfoPath);

	if (path)
	{
		if (!_r_obj_isstringempty (path))
			_r_shell_openfile (path->buffer);

		_r_obj_dereference (path);
	}
}

BOOLEAN _app_profile_load_check (_In_ LPCWSTR path, _In_ ENUM_TYPE_XML type)
{
	R_XML_LIBRARY xml_library;
	BOOLEAN is_success;

	if (_r_xml_initializelibrary (&xml_library, TRUE, NULL) != S_OK)
		return FALSE;

	is_success = FALSE;

	if (_r_xml_parsefile (&xml_library, path) == S_OK)
	{
		if (_r_xml_findchildbytagname (&xml_library, L"root"))
		{
			is_success = _app_profile_load_check_node (&xml_library, type);
		}
	}

	_r_xml_destroylibrary (&xml_library);

	return is_success;
}

VOID _app_profile_load_fallback ()
{
	PITEM_APP ptr_app;

	if (!_app_getappitem (config.my_hash))
	{
		ptr_app = _app_addapplication (NULL, DataUnknown, _r_sys_getimagepathname (), NULL, NULL);

		if (ptr_app)
			_app_setappinfo (ptr_app, InfoIsEnabled, IntToPtr (TRUE));
	}

	_app_setappinfobyhash (config.my_hash, InfoIsUndeletable, IntToPtr (TRUE));

	// disable deletion for this shit ;)
	if (!_r_config_getboolean (L"IsInternalRulesDisabled", FALSE))
	{
		if (!_app_getappitem (config.ntoskrnl_hash))
			_app_addapplication (NULL, DataUnknown, PROC_SYSTEM_NAME, NULL, NULL);

		if (!_app_getappitem (config.svchost_hash) && config.svchost_path)
			_app_addapplication (NULL, DataUnknown, config.svchost_path->buffer, NULL, NULL);

		_app_setappinfobyhash (config.ntoskrnl_hash, InfoIsUndeletable, IntToPtr (TRUE));
		_app_setappinfobyhash (config.svchost_hash, InfoIsUndeletable, IntToPtr (TRUE));
	}
}

VOID _app_profile_load_helper (_Inout_ PR_XML_LIBRARY xml_library, _In_ ENUM_TYPE_DATA type, _In_ UINT version)
{
	if (type == DataAppRegular)
	{
		PR_STRING string;
		LONG64 timestamp;
		LONG64 timer;
		BOOLEAN is_enabled;
		BOOLEAN is_silent;
		PITEM_APP ptr_app;

		string = _r_xml_getattribute_string (xml_library, L"path");

		if (!string)
			return;

		// workaround for native paths
		// https://github.com/henrypp/simplewall/issues/817
		if (_r_obj_getstringlength (string) > 2 && _r_obj_getstringhash (string) != config.ntoskrnl_hash && string->buffer[1] != L':')
		{
			PR_STRING dos_path = _r_path_dospathfromnt (string->buffer);

			if (dos_path)
				_r_obj_movereference (&string, dos_path);
		}

		if (!_r_obj_isstringempty (string))
		{
			ptr_app = _app_addapplication (NULL, DataUnknown, string->buffer, NULL, NULL);

			if (ptr_app)
			{
				is_enabled = _r_xml_getattribute_boolean (xml_library, L"is_enabled");
				is_silent = _r_xml_getattribute_boolean (xml_library, L"is_silent");

				timestamp = _r_xml_getattribute_long64 (xml_library, L"timestamp");
				timer = _r_xml_getattribute_long64 (xml_library, L"timer");

				if (is_silent)
					_app_setappinfo (ptr_app, InfoIsSilent, IntToPtr (is_silent));

				if (is_enabled)
					_app_setappinfo (ptr_app, InfoIsEnabled, IntToPtr (is_enabled));

				if (timestamp)
					_app_setappinfo (ptr_app, InfoTimestampPtr, &timestamp);

				if (timer)
					_app_setappinfo (ptr_app, InfoTimerPtr, &timer);
			}
		}

		_r_obj_dereference (string);
	}
	else if (type == DataRuleBlocklist || type == DataRuleSystem || type == DataRuleUser)
	{
		PR_STRING rule_name;
		PR_STRING rule_remote;
		PR_STRING rule_local;
		FWP_DIRECTION direction;
		UINT8 protocol;
		ADDRESS_FAMILY af;
		PITEM_RULE ptr_rule;
		PITEM_APP ptr_app;
		SIZE_T rule_hash;

		rule_name = _r_xml_getattribute_string (xml_library, L"name");

		if (!rule_name)
			return;

		rule_remote = _r_xml_getattribute_string (xml_library, L"rule");
		rule_local = _r_xml_getattribute_string (xml_library, L"rule_local");
		direction = (FWP_DIRECTION)_r_xml_getattribute_integer (xml_library, L"dir");
		protocol = (UINT8)_r_xml_getattribute_integer (xml_library, L"protocol");
		af = (ADDRESS_FAMILY)_r_xml_getattribute_integer (xml_library, L"version");

		ptr_rule = _app_addrule (rule_name, rule_remote, rule_local, direction, protocol, af);

		_r_obj_dereference (rule_name);

		if (rule_remote)
			_r_obj_dereference (rule_remote);

		if (rule_local)
			_r_obj_dereference (rule_local);

		if (!ptr_rule)
			return;

		rule_hash = _r_obj_getstringhash (ptr_rule->name);

		ptr_rule->type = ((type == DataRuleSystem && _r_xml_getattribute_boolean (xml_library, L"is_custom")) ? DataRuleUser : type);
		ptr_rule->is_block = _r_xml_getattribute_boolean (xml_library, L"is_block");
		ptr_rule->is_forservices = _r_xml_getattribute_boolean (xml_library, L"is_services");
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

		ptr_rule->is_enabled = _r_xml_getattribute_boolean (xml_library, L"is_enabled");

		if (type == DataRuleBlocklist)
		{
			INT blocklist_spy_state = _r_calc_clamp (_r_config_getinteger (L"BlocklistSpyState", 2), 0, 2);
			INT blocklist_update_state = _r_calc_clamp (_r_config_getinteger (L"BlocklistUpdateState", 0), 0, 2);
			INT blocklist_extra_state = _r_calc_clamp (_r_config_getinteger (L"BlocklistExtraState", 0), 0, 2);

			_app_ruleblocklistsetstate (ptr_rule, blocklist_spy_state, blocklist_update_state, blocklist_extra_state);
		}
		else
		{
			ptr_rule->is_enabled_default = ptr_rule->is_enabled; // set default value for rule
		}

		// load rules config
		{
			PITEM_RULE_CONFIG ptr_config = NULL;
			BOOLEAN is_internal;

			is_internal = (type == DataRuleBlocklist || type == DataRuleSystem);

			if (is_internal)
			{
				// internal rules
				ptr_config = _app_getruleconfigitem (rule_hash);

				if (ptr_config)
				{
					ptr_rule->is_enabled = ptr_config->is_enabled;
				}
			}

			// load apps
			{
				R_STRINGBUILDER rule_apps;
				PR_STRING string;

				_r_obj_initializestringbuilder (&rule_apps);

				string = _r_xml_getattribute_string (xml_library, L"apps");

				if (!_r_obj_isstringempty (string))
				{
					_r_obj_appendstringbuilder2 (&rule_apps, string);

					_r_obj_dereference (string);
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
						_r_str_replacechar (string->buffer, DIVIDER_RULE[0], DIVIDER_APP[0]); // for compat with old profiles

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
								if (ptr_rule->is_forservices && (app_hash == config.ntoskrnl_hash || app_hash == config.svchost_hash))
								{
									_r_obj_dereference (path_string);
									continue;
								}

								ptr_app = _app_getappitem (app_hash);

								if (!ptr_app)
								{
									ptr_app = _app_addapplication (NULL, DataUnknown, path_string->buffer, NULL, NULL);

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
		}

		_r_spinlock_acquireexclusive (&lock_rules);

		_r_obj_addarrayitem (rules_arr, ptr_rule);

		_r_spinlock_releaseexclusive (&lock_rules);
	}
	else if (type == DataRulesConfig)
	{
		PR_STRING rule_name;
		PITEM_RULE_CONFIG ptr_config;
		SIZE_T rule_hash;

		rule_name = _r_xml_getattribute_string (xml_library, L"name");

		if (!rule_name)
			return;

		rule_hash = _r_obj_getstringhash (rule_name);

		if (rule_hash)
		{
			ptr_config = _app_getruleconfigitem (rule_hash);

			if (!ptr_config)
			{
				_r_spinlock_acquireexclusive (&lock_rules_config);

				ptr_config = _app_addruleconfigtable (rules_config, rule_hash, _r_obj_reference (rule_name), _r_xml_getattribute_boolean (xml_library, L"is_enabled"));

				_r_spinlock_releaseexclusive (&lock_rules_config);

				ptr_config->apps = _r_xml_getattribute_string (xml_library, L"apps");

				if (ptr_config->apps && version < XML_PROFILE_VER_3)
					_r_str_replacechar (ptr_config->apps->buffer, DIVIDER_RULE[0], DIVIDER_APP[0]); // for compat with old profiles
			}
		}

		_r_obj_dereference (rule_name);
	}
}

VOID _app_profile_load_internal (_In_ LPCWSTR path, _In_ LPCWSTR resource_name, _Inout_opt_ PLONG64 timestamp)
{
	R_XML_LIBRARY xml_file;
	R_XML_LIBRARY xml_resource;

	PR_XML_LIBRARY xml_library;

	LONG64 timestamp_file = 0;
	LONG64 timestamp_resource = 0;

	INT version_file = 0;
	INT version_resource = 0;
	INT version = 0;

	_r_xml_initializelibrary (&xml_file, TRUE, NULL);
	_r_xml_initializelibrary (&xml_resource, TRUE, NULL);

	if (path)
	{
		if (_r_xml_parsefile (&xml_file, path) == S_OK)
		{
			if (_r_xml_findchildbytagname (&xml_file, L"root"))
			{
				version_file = _r_xml_getattribute_integer (&xml_file, L"version");
				timestamp_file = _r_xml_getattribute_long64 (&xml_file, L"timestamp");
			}
		}
	}

	if (resource_name)
	{
		PVOID buffer;
		ULONG buffer_size;

		buffer = _r_res_loadresource (NULL, resource_name, RT_RCDATA, &buffer_size);

		if (buffer)
		{
			if (_r_xml_parsestring (&xml_resource, buffer, buffer_size) == S_OK)
			{
				if (_r_xml_findchildbytagname (&xml_resource, L"root"))
				{
					version_resource = _r_xml_getattribute_integer (&xml_resource, L"version");
					timestamp_resource = _r_xml_getattribute_long64 (&xml_resource, L"timestamp");
				}
			}
		}
	}

	xml_library = (timestamp_file > timestamp_resource) ? &xml_file : &xml_resource;
	version = (timestamp_file > timestamp_resource) ? version_file : version_resource;

	if (xml_library->stream)
	{
		if (_app_profile_load_check_node (xml_library, XmlProfileInternalV3))
		{
			if (timestamp)
				*timestamp = (timestamp_file > timestamp_resource) ? timestamp_file : timestamp_resource;

			LPCWSTR parent_tag = NULL;
			BOOLEAN is_found = FALSE;

			INT version = _r_xml_getattribute_integer (xml_library, L"version");

			// load system rules
			if (_r_xml_findchildbytagname (xml_library, L"rules_system"))
			{
				while (_r_xml_enumchilditemsbytagname (xml_library, L"item"))
				{
					_app_profile_load_helper (xml_library, DataRuleSystem, version);
				}
			}

			// load blocklist rules
			if (_r_xml_findchildbytagname (xml_library, L"rules_blocklist"))
			{
				while (_r_xml_enumchilditemsbytagname (xml_library, L"item"))
				{
					_app_profile_load_helper (xml_library, DataRuleBlocklist, version);
				}
			}
		}
	}

	_r_xml_destroylibrary (&xml_file);

	_r_xml_destroylibrary (&xml_resource);
}

VOID _app_profile_load (_In_opt_ HWND hwnd, _In_opt_ LPCWSTR path_custom)
{
	INT current_listview_id = (INT)_r_tab_getitemlparam (hwnd, IDC_TAB, -1);
	INT selected_item = (INT)SendDlgItemMessage (hwnd, current_listview_id, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
	INT scroll_pos = GetScrollPos (GetDlgItem (hwnd, current_listview_id), SB_VERT);

	R_XML_LIBRARY xml_library;
	HRESULT hr;

	// clean listview
	if (hwnd)
	{
		for (INT i = IDC_APPS_PROFILE; i <= IDC_RULES_CUSTOM; i++)
			_r_listview_deleteallitems (hwnd, i);
	}

	_r_spinlock_acquireexclusive (&lock_apply);

	// clear apps
	_r_spinlock_acquireexclusive (&lock_apps);

	_r_obj_clearhashtable (apps);

	_r_spinlock_releaseexclusive (&lock_apps);

	// clear rules
	_r_spinlock_acquireexclusive (&lock_rules);

	_r_obj_cleararray (rules_arr);

	_r_spinlock_releaseexclusive (&lock_rules);

	// clear rules config
	_r_spinlock_acquireexclusive (&lock_rules_config);

	_r_obj_clearhashtable (rules_config);

	_r_spinlock_releaseexclusive (&lock_rules_config);

	// generate uwp apps list (win8+)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
		_app_generate_packages ();

	// generate services list
	_app_generate_services ();

	_r_spinlock_releaseexclusive (&lock_apply);

	// load profile
	_r_xml_initializelibrary (&xml_library, TRUE, NULL);

	_r_spinlock_acquireshared (&lock_profile);

	hr = _r_xml_parsefile (&xml_library, path_custom ? path_custom : config.profile_path);

	// load backup
	if (hr != S_OK && !path_custom)
	{
		hr = _r_xml_parsefile (&xml_library, config.profile_path_backup);
	}

	_r_spinlock_releaseshared (&lock_profile);

	if (hr != S_OK)
	{
		if (hr != HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND))
			_r_log (LOG_LEVEL_ERROR, UID, L"_r_xml_parsefile", hr, path_custom ? path_custom : config.profile_path);
	}
	else
	{
		if (_r_xml_findchildbytagname (&xml_library, L"root"))
		{
			if (_app_profile_load_check_node (&xml_library, XmlProfileV3))
			{
				INT version = _r_xml_getattribute_integer (&xml_library, L"version");

				// load apps
				if (_r_xml_findchildbytagname (&xml_library, L"apps"))
				{
					while (_r_xml_enumchilditemsbytagname (&xml_library, L"item"))
					{
						_app_profile_load_helper (&xml_library, DataAppRegular, version);
					}
				}

				// load rules config
				if (_r_xml_findchildbytagname (&xml_library, L"rules_config"))
				{
					while (_r_xml_enumchilditemsbytagname (&xml_library, L"item"))
					{
						_app_profile_load_helper (&xml_library, DataRulesConfig, version);
					}
				}

				// load user rules
				if (_r_xml_findchildbytagname (&xml_library, L"rules_custom"))
				{
					while (_r_xml_enumchilditemsbytagname (&xml_library, L"item"))
					{
						_app_profile_load_helper (&xml_library, DataRuleUser, version);
					}
				}
			}
		}
	}

	_r_xml_destroylibrary (&xml_library);

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

				_r_listview_additemex (hwnd, listview_id, 0, 0, SZ_EMPTY, 0, 0, ptr_app->app_hash);
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

				_r_listview_additemex (hwnd, listview_id, 0, 0, SZ_EMPTY, 0, 0, i);
				_app_setruleiteminfo (hwnd, listview_id, 0, ptr_rule, FALSE);

				_r_spinlock_releaseshared (&lock_checkbox);
			}
		}

		_r_spinlock_releaseshared (&lock_rules);
	}

	if (hwnd && current_listview_id)
	{
		INT new_listview_id = (INT)_r_tab_getitemlparam (hwnd, IDC_TAB, -1);

		if (new_listview_id)
		{
			_app_listviewsort (hwnd, new_listview_id, -1, FALSE);

			if (current_listview_id == new_listview_id)
				_app_showitem (hwnd, current_listview_id, selected_item, scroll_pos);
		}
	}
}

VOID _app_profile_save ()
{
	R_XML_LIBRARY xml_library;
	LONG64 current_time;
	BOOLEAN is_backuprequired;
	HRESULT hr;

	current_time = _r_unixtime_now ();
	is_backuprequired = _r_config_getboolean (L"IsBackupProfile", TRUE) && (!_r_fs_exists (config.profile_path_backup) || ((current_time - _r_config_getlong64 (L"BackupTimestamp", 0)) >= _r_config_getlong64 (L"BackupPeriod", BACKUP_HOURS_PERIOD)));

	hr = _r_xml_initializelibrary (&xml_library, FALSE, NULL);

	if (hr != S_OK)
	{
		_r_log (LOG_LEVEL_ERROR, UID, L"_r_xml_initializelibrary", hr, NULL);

		return;
	}

	hr = _r_xml_createfile (&xml_library, config.profile_path);

	if (hr != S_OK)
	{
		_r_log (LOG_LEVEL_ERROR, UID, L"_r_xml_createfile", hr, config.profile_path);

		return;
	}

	_r_spinlock_acquireexclusive (&lock_profile);

	_r_xml_writestartdocument (&xml_library);

	_r_xml_writestartelement (&xml_library, L"root");

	_r_xml_setattribute_long64 (&xml_library, L"timestamp", current_time);
	_r_xml_setattribute_integer (&xml_library, L"type", XmlProfileV3);
	_r_xml_setattribute_integer (&xml_library, L"version", XML_PROFILE_VER_CURRENT);

	_r_xml_writewhitespace (&xml_library, L"\n\t");

	_r_xml_writestartelement (&xml_library, L"apps");

	// save apps
	PITEM_APP ptr_app;
	PITEM_RULE ptr_rule;
	PITEM_RULE_CONFIG ptr_config;
	PR_STRING string = NULL;
	SIZE_T enum_key;
	BOOLEAN is_keepunusedapps = _r_config_getboolean (L"IsKeepUnusedApps", TRUE);
	BOOLEAN is_usedapp = FALSE;

	_r_spinlock_acquireshared (&lock_apps);

	enum_key = 0;

	while (_r_obj_enumhashtable (apps, &ptr_app, NULL, &enum_key))
	{
		if (_r_obj_isstringempty (ptr_app->original_path))
			continue;

		is_usedapp = _app_isappused (ptr_app);

		// do not save unused apps/uwp apps...
		if (!is_usedapp && (!is_keepunusedapps || (ptr_app->type == DataAppService || ptr_app->type == DataAppUWP)))
			continue;

		_r_xml_writewhitespace (&xml_library, L"\n\t\t");

		_r_xml_writestartelement (&xml_library, L"item");

		_r_xml_setattribute (&xml_library, L"path", ptr_app->original_path->buffer);

		if (ptr_app->timestamp)
			_r_xml_setattribute_long64 (&xml_library, L"timestamp", ptr_app->timestamp);

		// set timer (if presented)
		if (ptr_app->timer && _app_istimerset (ptr_app->htimer))
			_r_xml_setattribute_long64 (&xml_library, L"timer", ptr_app->timer);

		// ffu!
		if (ptr_app->profile)
			_r_xml_setattribute_integer (&xml_library, L"profile", ptr_app->profile);

		if (ptr_app->is_silent)
			_r_xml_setattribute_boolean (&xml_library, L"is_silent", ptr_app->is_silent);

		if (ptr_app->is_enabled)
			_r_xml_setattribute_boolean (&xml_library, L"is_enabled", ptr_app->is_enabled);

		_r_xml_writeendelement (&xml_library);
	}

	_r_spinlock_releaseshared (&lock_apps);

	_r_xml_writewhitespace (&xml_library, L"\n\t");

	_r_xml_writeendelement (&xml_library);

	_r_xml_writewhitespace (&xml_library, L"\n\t");

	// save rules
	_r_xml_writestartelement (&xml_library, L"rules_custom");

	_r_spinlock_acquireshared (&lock_rules);

	// save user rules
	for (SIZE_T i = 0; i < _r_obj_getarraysize (rules_arr); i++)
	{
		ptr_rule = _r_obj_getarrayitem (rules_arr, i);

		if (!ptr_rule || ptr_rule->is_readonly || _r_obj_isstringempty (ptr_rule->name))
			continue;

		_r_xml_writewhitespace (&xml_library, L"\n\t\t");

		_r_xml_writestartelement (&xml_library, L"item");

		_r_xml_setattribute (&xml_library, L"name", ptr_rule->name->buffer);

		if (!_r_obj_isstringempty (ptr_rule->rule_remote))
			_r_xml_setattribute (&xml_library, L"rule", ptr_rule->rule_remote->buffer);

		if (!_r_obj_isstringempty (ptr_rule->rule_local))
			_r_xml_setattribute (&xml_library, L"rule_local", ptr_rule->rule_local->buffer);

		// ffu!
		if (ptr_rule->profile)
			_r_xml_setattribute_integer (&xml_library, L"profile", ptr_rule->profile);

		if (ptr_rule->direction != FWP_DIRECTION_OUTBOUND)
			_r_xml_setattribute_integer (&xml_library, L"dir", ptr_rule->direction);

		if (ptr_rule->protocol != 0)
			_r_xml_setattribute_integer (&xml_library, L"protocol", ptr_rule->protocol);

		if (ptr_rule->af != AF_UNSPEC)
			_r_xml_setattribute_integer (&xml_library, L"version", ptr_rule->af);

		// add apps attribute
		if (!_r_obj_ishashtableempty (ptr_rule->apps))
		{
			string = _app_rulesexpandapps (ptr_rule, FALSE, DIVIDER_APP);

			if (string)
			{
				if (!_r_obj_isstringempty (string))
					_r_xml_setattribute (&xml_library, L"apps", string->buffer);

				_r_obj_clearreference (&string);
			}
		}

		if (ptr_rule->is_block)
			_r_xml_setattribute_boolean (&xml_library, L"is_block", ptr_rule->is_block);

		if (ptr_rule->is_enabled)
			_r_xml_setattribute_boolean (&xml_library, L"is_enabled", ptr_rule->is_enabled);

		_r_xml_writeendelement (&xml_library);
	}

	_r_spinlock_releaseshared (&lock_rules);

	_r_xml_writewhitespace (&xml_library, L"\n\t");

	_r_xml_writeendelement (&xml_library);

	_r_xml_writewhitespace (&xml_library, L"\n\t");

	// save rules config
	_r_xml_writestartelement (&xml_library, L"rules_config");

	enum_key = 0;

	_r_spinlock_acquireshared (&lock_rules_config);

	while (_r_obj_enumhashtable (rules_config, &ptr_config, NULL, &enum_key))
	{
		if (_r_obj_isstringempty (ptr_config->name))
			continue;

		BOOLEAN is_enabled_default = ptr_config->is_enabled;
		SIZE_T rule_hash = _r_obj_getstringhash (ptr_config->name);
		ptr_rule = _app_getrulebyhash (rule_hash);

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

		_r_xml_writewhitespace (&xml_library, L"\n\t\t");

		_r_xml_writestartelement (&xml_library, L"item");

		_r_xml_setattribute (&xml_library, L"name", ptr_config->name->buffer);

		if (string)
		{
			if (!_r_obj_isstringempty (string))
				_r_xml_setattribute (&xml_library, L"apps", string->buffer);

			_r_obj_clearreference (&string);
		}

		_r_xml_setattribute_boolean (&xml_library, L"is_enabled", ptr_config->is_enabled);

		_r_xml_writeendelement (&xml_library);
	}

	_r_spinlock_releaseshared (&lock_rules_config);

	_r_xml_writewhitespace (&xml_library, L"\n\t");

	_r_xml_writeendelement (&xml_library);

	_r_xml_writewhitespace (&xml_library, L"\n");

	_r_xml_writeendelement (&xml_library);

	_r_xml_writewhitespace (&xml_library, L"\n");

	_r_xml_writeenddocument (&xml_library);

	_r_spinlock_releaseexclusive (&lock_profile);

	_r_xml_destroylibrary (&xml_library);

	// make backup
	if (is_backuprequired)
	{
		_r_fs_copyfile (config.profile_path, config.profile_path_backup, 0);
		_r_config_setlong64 (L"BackupTimestamp", current_time);
	}
}
