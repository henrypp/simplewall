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
		return IntToPtr (_app_getlistviewbytype_id (ptr_app->type));
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
PVOID _app_getappinfobyhash (_In_ ULONG_PTR app_hash, _In_ ENUM_INFO_DATA info_data)
{
	PITEM_APP ptr_app;
	PVOID result;

	ptr_app = _app_getappitem (app_hash);

	if (ptr_app)
	{
		result = _app_getappinfo (ptr_app, info_data);

		_r_obj_dereference (ptr_app);

		return result;
	}

	return NULL;
}

VOID _app_setappinfo (_In_ PITEM_APP ptr_app, _In_ ENUM_INFO_DATA info_data, _In_ PVOID value)
{
	if (info_data == InfoBytesData)
	{
		_r_obj_movereference (&ptr_app->pbytes, value);
	}
	else if (info_data == InfoTimestampPtr)
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

		if (ptr_app->is_silent)
			_app_freenotify (ptr_app);
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

VOID _app_setappinfobyhash (_In_ ULONG_PTR app_hash, _In_ ENUM_INFO_DATA info_data, _In_ PVOID value)
{
	PITEM_APP ptr_app;

	ptr_app = _app_getappitem (app_hash);

	if (ptr_app)
	{
		_app_setappinfo (ptr_app, info_data, value);

		_r_obj_dereference (ptr_app);
	}
}

_Ret_maybenull_
PVOID _app_getruleinfo (_In_ PITEM_RULE ptr_rule, _In_ ENUM_INFO_DATA info_data)
{
	if (info_data == InfoListviewId)
	{
		return IntToPtr (_app_getlistviewbytype_id (ptr_rule->type));
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
	PITEM_RULE ptr_rule;
	PVOID info;

	ptr_rule = _app_getrulebyid (index);

	if (ptr_rule)
	{
		info = _app_getruleinfo (ptr_rule, info_data);

		_r_obj_dereference (ptr_rule);

		return info;
	}

	return NULL;
}

THREAD_API _app_queuefileinformation (_In_ PVOID arglist)
{
	PITEM_APP ptr_app;
	HWND hwnd;

	ptr_app = arglist;

	if (_r_config_getboolean (L"IsCertificatesEnabled", FALSE))
	{
		_app_getsignatureinfo (ptr_app);
	}

	_app_getappicon (ptr_app, TRUE, &ptr_app->icon_id, NULL);

	_app_getversioninfo (ptr_app);

	hwnd = _r_app_gethwnd ();

	if (hwnd && _r_wnd_isvisible (hwnd))
		_app_setlistviewbylparam (hwnd, ptr_app->app_hash, PR_SETITEM_REDRAW, TRUE);

	_r_obj_dereference (ptr_app);

	return STATUS_SUCCESS;
}

ULONG_PTR _app_addapplication (_In_opt_ HWND hwnd, _In_ ENUM_TYPE_DATA type, _In_ PR_STRINGREF path, _In_opt_ PR_STRING display_name, _In_opt_ PR_STRING real_path)
{
	R_STRINGREF path_temp;
	WCHAR path_full[1024];
	PITEM_APP ptr_app;
	ULONG_PTR app_hash;
	BOOLEAN is_ntoskrnl;

	if (!path->length)
		return 0;

	if (_app_isappvalidpath (path) && PathIsDirectory (path->buffer))
		return 0;

	_r_obj_initializestringref3 (&path_temp, path);

	// prevent possible duplicate apps entries with short path (issue #640)
	if (_r_str_findchar (&path_temp, L'~', FALSE) != SIZE_MAX)
	{
		if (GetLongPathName (path_temp.buffer, path_full, RTL_NUMBER_OF (path_full)))
		{
			_r_obj_initializestringref (&path_temp, path_full);
		}
	}

	app_hash = _r_obj_getstringrefhash (&path_temp);

	if (_app_isappfound (app_hash))
	{
		return app_hash; // already exists
	}

	ptr_app = _r_obj_allocate (sizeof (ITEM_APP), &_app_dereferenceapp);
	is_ntoskrnl = (app_hash == config.ntoskrnl_hash);

	ptr_app->app_hash = app_hash;

	if (_r_str_isstartswith2 (&path_temp, L"S-1-", TRUE)) // uwp (win8+)
	{
		type = DataAppUWP;
	}

	if (type == DataAppService || type == DataAppUWP)
	{
		ptr_app->type = type;

		if (display_name)
		{
			ptr_app->display_name = _r_obj_reference (display_name);
		}

		if (real_path)
		{
			ptr_app->real_path = _r_obj_reference (real_path);
		}
	}
	else if (_r_str_isstartswith2 (&path_temp, L"\\device\\", TRUE)) // device path
	{
		ptr_app->type = DataAppDevice;
		ptr_app->real_path = _r_obj_createstring3 (&path_temp);
	}
	else
	{
		if (!is_ntoskrnl && _r_str_findchar (&path_temp, OBJ_NAME_PATH_SEPARATOR, FALSE) == SIZE_MAX)
		{
			ptr_app->type = DataAppPico;
		}
		else
		{
			ptr_app->type = PathIsNetworkPath (path_temp.buffer) ? DataAppNetwork : DataAppRegular;
		}

		ptr_app->real_path = is_ntoskrnl ? _r_obj_createstring2 (config.ntoskrnl_path) : _r_obj_createstring3 (&path_temp);
	}

	ptr_app->original_path = _r_obj_createstring3 (&path_temp);

	// fix "System" lowercase
	if (is_ntoskrnl)
	{
		_r_str_tolower (&ptr_app->original_path->sr);
		ptr_app->original_path->buffer[0] = _r_str_upper (ptr_app->original_path->buffer[0]);
	}

	if (ptr_app->type == DataAppRegular || ptr_app->type == DataAppDevice || ptr_app->type == DataAppNetwork)
	{
		ptr_app->short_name = _r_path_getbasenamestring (&path_temp);
	}

	ptr_app->guids = _r_obj_createarray (sizeof (GUID), NULL); // initialize array
	ptr_app->timestamp = _r_unixtime_now ();

	if (ptr_app->type == DataAppService || ptr_app->type == DataAppUWP)
	{
		ptr_app->is_undeletable = TRUE;
	}

	// insert object into the table
	_r_queuedlock_acquireexclusive (&lock_apps);

	_r_obj_addhashtablepointer (apps_table, app_hash, ptr_app);

	_r_queuedlock_releaseexclusive (&lock_apps);

	// queue file information
	_r_workqueue_queueitem (&file_queue, &_app_queuefileinformation, _r_obj_reference (ptr_app));

	// insert item
	if (hwnd)
	{
		_app_addlistviewapp (hwnd, ptr_app, app_hash);
	}

	return app_hash;
}

PITEM_RULE _app_addrule (_In_opt_ PR_STRING name, _In_opt_ PR_STRING rule_remote, _In_opt_ PR_STRING rule_local, _In_ FWP_DIRECTION direction, _In_ UINT8 protocol, _In_ ADDRESS_FAMILY af)
{
	PITEM_RULE ptr_rule;

	ptr_rule = _r_obj_allocate (sizeof (ITEM_RULE), &_app_dereferencerule);

	ptr_rule->apps = _r_obj_createhashtable (sizeof (SHORT), NULL); // initialize hashtable
	ptr_rule->guids = _r_obj_createarray (sizeof (GUID), NULL); // initialize array

	ptr_rule->type = DataRuleUser;

	// set rule name
	if (name)
	{
		ptr_rule->name = _r_obj_reference (name);

		if (_r_obj_getstringlength (ptr_rule->name) > RULE_NAME_CCH_MAX)
			_r_obj_setstringlength (ptr_rule->name, RULE_NAME_CCH_MAX * sizeof (WCHAR));
	}

	// set rule destination
	if (rule_remote)
	{
		ptr_rule->rule_remote = _r_obj_reference (rule_remote);

		if (_r_obj_getstringlength (ptr_rule->rule_remote) > RULE_RULE_CCH_MAX)
			_r_obj_setstringlength (ptr_rule->rule_remote, RULE_RULE_CCH_MAX * sizeof (WCHAR));
	}

	// set rule source
	if (rule_local)
	{
		ptr_rule->rule_local = _r_obj_reference (rule_local);

		if (_r_obj_getstringlength (ptr_rule->rule_local) > RULE_RULE_CCH_MAX)
			_r_obj_setstringlength (ptr_rule->rule_local, RULE_RULE_CCH_MAX * sizeof (WCHAR));
	}

	// set configuration
	ptr_rule->direction = direction;
	ptr_rule->protocol = protocol;
	ptr_rule->af = af;

	return ptr_rule;
}

_Ret_maybenull_
PITEM_RULE_CONFIG _app_addruleconfigtable (_In_ PR_HASHTABLE hashtable, _In_ ULONG_PTR rule_hash, _In_opt_ PR_STRING name, _In_ BOOLEAN is_enabled)
{
	ITEM_RULE_CONFIG entry = {0};

	entry.name = name;
	entry.is_enabled = is_enabled;

	return _r_obj_addhashtableitem (hashtable, rule_hash, &entry);
}

_Ret_maybenull_
PITEM_APP _app_getappitem (_In_ ULONG_PTR app_hash)
{
	PITEM_APP ptr_app;

	_r_queuedlock_acquireshared (&lock_apps);

	ptr_app = _r_obj_findhashtablepointer (apps_table, app_hash);

	_r_queuedlock_releaseshared (&lock_apps);

	return ptr_app;
}

_Ret_maybenull_
PITEM_RULE _app_getrulebyid (_In_ SIZE_T index)
{
	PITEM_RULE ptr_rule;

	_r_queuedlock_acquireshared (&lock_rules);

	ptr_rule = _r_obj_getlistitem (rules_list, index);

	_r_queuedlock_releaseshared (&lock_rules);

	if (ptr_rule)
		return _r_obj_reference (ptr_rule);

	return NULL;
}

_Ret_maybenull_
PITEM_RULE _app_getrulebyhash (_In_ ULONG_PTR rule_hash)
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
				if (_r_obj_getstringhash (ptr_rule->name) == rule_hash)
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
PITEM_RULE_CONFIG _app_getruleconfigitem (_In_ ULONG_PTR rule_hash)
{
	PITEM_RULE_CONFIG ptr_rule_config;

	_r_queuedlock_acquireshared (&lock_rules_config);

	ptr_rule_config = _r_obj_findhashtable (rules_config, rule_hash);

	_r_queuedlock_releaseshared (&lock_rules_config);

	return ptr_rule_config;
}

ULONG_PTR _app_getnetworkapp (_In_ ULONG_PTR network_hash)
{
	PITEM_NETWORK ptr_network;
	ULONG_PTR hash_code;

	ptr_network = _app_getnetworkitem (network_hash);

	if (ptr_network)
	{
		hash_code = ptr_network->app_hash;

		_r_obj_dereference (ptr_network);

		return hash_code;
	}

	return 0;
}

_Ret_maybenull_
PITEM_NETWORK _app_getnetworkitem (_In_ ULONG_PTR network_hash)
{
	PITEM_NETWORK ptr_network;

	_r_queuedlock_acquireshared (&lock_network);

	ptr_network = _r_obj_findhashtablepointer (network_table, network_hash);

	_r_queuedlock_releaseshared (&lock_network);

	return ptr_network;
}

_Ret_maybenull_
PITEM_LOG _app_getlogitem (_In_ ULONG_PTR log_hash)
{
	PITEM_LOG ptr_log;

	_r_queuedlock_acquireshared (&lock_loglist);

	ptr_log = _r_obj_findhashtablepointer (log_table, log_hash);

	_r_queuedlock_releaseshared (&lock_loglist);

	return ptr_log;
}

ULONG_PTR _app_getlogapp (_In_ SIZE_T index)
{
	PITEM_LOG ptr_log;
	ULONG_PTR app_hash;

	ptr_log = _app_getlogitem (index);

	if (ptr_log)
	{
		app_hash = ptr_log->app_hash;

		_r_obj_dereference (ptr_log);

		return app_hash;
	}

	return 0;
}

COLORREF _app_getappcolor (_In_ INT listview_id, _In_ ULONG_PTR app_hash, _In_ BOOLEAN is_systemapp, _In_ BOOLEAN is_validconnection)
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
		if (_r_config_getbooleanex (L"IsHighlightInvalid", TRUE, L"colors") && !_app_isappexists (ptr_app))
		{
			color_hash = config.color_invalid;
			goto CleanupExit;
		}

		if (_r_config_getbooleanex (L"IsHighlightTimer", TRUE, L"colors") && _app_istimerset (ptr_app->htimer))
		{
			color_hash = config.color_timer;
			goto CleanupExit;
		}
	}

	if (_r_config_getbooleanex (L"IsHighlightConnection", TRUE, L"colors") && is_validconnection)
	{
		color_hash = config.color_network;
		goto CleanupExit;
	}

	if (ptr_app)
	{
		if (_r_config_getbooleanex (L"IsHighlightSigned", TRUE, L"colors") && !_r_obj_isstringempty (ptr_app->signature))
		{
			color_hash = config.color_signed;
			goto CleanupExit;
		}

		if (!is_profilelist && (_r_config_getbooleanex (L"IsHighlightSpecial", TRUE, L"colors") && _app_isapphaverule (app_hash, FALSE)))
		{
			color_hash = config.color_special;
			goto CleanupExit;
		}

		if (_r_config_getbooleanex (L"IsHighlightPico", TRUE, L"colors") && ptr_app->type == DataAppPico)
		{
			color_hash = config.color_pico;
			goto CleanupExit;
		}
	}

	if (_r_config_getbooleanex (L"IsHighlightSystem", TRUE, L"colors") && is_systemapp)
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

VOID _app_freeapplication (_In_ ULONG_PTR app_hash)
{
	PITEM_RULE ptr_rule;

	_r_queuedlock_acquireshared (&lock_rules);

	for (SIZE_T i = 0; i < _r_obj_getlistsize (rules_list); i++)
	{
		ptr_rule = _r_obj_getlistitem (rules_list, i);

		if (!ptr_rule)
			continue;

		if (ptr_rule->type == DataRuleUser)
		{
			if (_r_obj_findhashtable (ptr_rule->apps, app_hash))
			{
				_r_obj_removehashtableitem (ptr_rule->apps, app_hash);

				if (ptr_rule->is_enabled && _r_obj_ishashtableempty (ptr_rule->apps))
				{
					ptr_rule->is_enabled = FALSE;
					ptr_rule->is_haveerrors = FALSE;
				}

				_app_updateitembylparam (_r_app_gethwnd (), i, FALSE);
			}
		}
	}

	_r_queuedlock_releaseshared (&lock_rules);

	_r_obj_removehashtablepointer (apps_table, app_hash);
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

	_r_queuedlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtablepointer (apps_table, &ptr_app, NULL, &enum_key))
	{
		is_used = _app_isappused (ptr_app);

		if (_app_istimerset (ptr_app->htimer))
			status->apps_timer_count += 1;

		if (!ptr_app->is_undeletable && (!_app_isappexists (ptr_app) || !is_used) && !(ptr_app->type == DataAppService || ptr_app->type == DataAppUWP))
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

	_r_queuedlock_releaseshared (&lock_rules);
}

COLORREF _app_getrulecolor (_In_ INT listview_id, _In_ SIZE_T rule_idx)
{
	PITEM_RULE ptr_rule;
	ULONG_PTR color_hash;

	ptr_rule = _app_getrulebyid (rule_idx);

	if (!ptr_rule)
		return 0;

	color_hash = 0;

	if (_r_config_getbooleanex (L"IsHighlightInvalid", TRUE, L"colors") && ptr_rule->is_enabled && ptr_rule->is_haveerrors)
		color_hash = config.color_invalid;

	else if (_r_config_getbooleanex (L"IsHighlightSpecial", TRUE, L"colors") && (ptr_rule->is_forservices || !_r_obj_ishashtableempty (ptr_rule->apps)))
		color_hash = config.color_special;

	_r_obj_dereference (ptr_rule);

	if (color_hash)
		return _app_getcolorvalue (color_hash);

	return 0;
}

BOOLEAN _app_getdisplayinfo (_In_ HWND hwnd, _In_ INT listview_id, _Inout_ LPNMLVDISPINFOW lpnmlv)
{
	PITEM_NETWORK ptr_network;
	PITEM_RULE ptr_rule;
	PITEM_APP ptr_app;
	PITEM_LOG ptr_log;

	if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP || listview_id == IDC_RULE_APPS_ID)
	{
		ptr_app = _app_getappitem (lpnmlv->item.lParam);

		if (ptr_app)
		{
			if ((lpnmlv->item.mask & LVIF_IMAGE))
			{
				lpnmlv->item.iImage = ptr_app->icon_id;
			}

			if ((lpnmlv->item.mask & LVIF_GROUPID))
			{
				if (listview_id == IDC_RULE_APPS_ID)
				{
					if (ptr_app->type == DataAppUWP)
					{
						lpnmlv->item.iGroupId = 2;
					}
					else if (ptr_app->type == DataAppService)
					{
						lpnmlv->item.iGroupId = 1;
					}
					else
					{
						lpnmlv->item.iGroupId = 0;
					}
				}
				else
				{
					// apps with special rule
					if (_app_isapphaverule (ptr_app->app_hash, FALSE))
					{
						lpnmlv->item.iGroupId = 1;
					}
					else if (ptr_app->is_enabled)
					{
						lpnmlv->item.iGroupId = 0;
					}
					else
					{
						// silent apps without rules and not enabled added to different group
						if (ptr_app->is_silent)
						{
							lpnmlv->item.iGroupId = 3;
						}
						else
						{
							lpnmlv->item.iGroupId = 2;
						}
					}
				}
			}

			_r_obj_dereference (ptr_app);
		}

		SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
		return TRUE;
	}
	else if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM || listview_id == IDC_APP_RULES_ID)
	{
		ptr_rule = _app_getrulebyid (lpnmlv->item.lParam);

		if (ptr_rule)
		{
			if ((lpnmlv->item.mask & LVIF_IMAGE))
			{
				lpnmlv->item.iImage = (ptr_rule->action == FWP_ACTION_BLOCK) ? 1 : 0;
			}

			if ((lpnmlv->item.mask & LVIF_GROUPID))
			{
				if (listview_id == IDC_APP_RULES_ID)
				{
					lpnmlv->item.iGroupId = ptr_rule->is_readonly ? 0 : 1;
				}
				else
				{
					lpnmlv->item.iGroupId = ptr_rule->is_enabled ? 0 : 2;
				}
			}

			_r_obj_dereference (ptr_rule);
		}

		SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
		return TRUE;
	}
	else if (listview_id == IDC_NETWORK)
	{
		ptr_network = _app_getnetworkitem (lpnmlv->item.lParam);

		if (ptr_network)
		{
			if ((lpnmlv->item.mask & LVIF_IMAGE))
			{
				ptr_app = _app_getappitem (ptr_network->app_hash);

				if (ptr_app)
				{
					lpnmlv->item.iImage = ptr_app->icon_id;

					_r_obj_dereference (ptr_app);
				}
				else
				{
					lpnmlv->item.iImage = ptr_network->icon_id;
				}
			}

			if ((lpnmlv->item.mask & LVIF_GROUPID))
			{
				if (ptr_network->type == DataAppService)
				{
					lpnmlv->item.iGroupId = 1;
				}
				else if (ptr_network->type == DataAppUWP)
				{
					lpnmlv->item.iGroupId = 2;
				}
				else
				{
					lpnmlv->item.iGroupId = 0;
				}
			}

			_r_obj_dereference (ptr_network);

			SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
			return TRUE;
		}
	}
	else if (listview_id == IDC_LOG)
	{
		if ((lpnmlv->item.mask & LVIF_IMAGE))
		{
			ptr_log = _app_getlogitem (lpnmlv->item.lParam);

			if (ptr_log)
			{
				ptr_app = _app_getappitem (ptr_log->app_hash);

				if (ptr_app)
				{
					lpnmlv->item.iImage = ptr_app->icon_id;

					_r_obj_dereference (ptr_app);
				}
				else
				{
					lpnmlv->item.iImage = ptr_log->icon_id;
				}
			}

			SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
			return TRUE;
		}
	}

	return FALSE;
}

VOID _app_setappiteminfo (_In_ HWND hwnd, _In_ INT listview_id, _In_ INT item_id, _Inout_ PITEM_APP ptr_app)
{
	PR_STRING date_string;

	if (!listview_id || item_id == -1)
		return;

	_r_listview_setitemex (hwnd, listview_id, item_id, 0, _app_getappdisplayname (ptr_app, FALSE), I_IMAGECALLBACK, I_GROUPIDCALLBACK, 0);

	date_string = _r_format_unixtimeex (ptr_app->timestamp, FDTF_SHORTDATE | FDTF_SHORTTIME);

	if (date_string)
	{
		_r_listview_setitem (hwnd, listview_id, item_id, 1, date_string->buffer);

		_r_obj_dereference (date_string);
	}

	_r_listview_setitemcheck (hwnd, listview_id, item_id, !!ptr_app->is_enabled);
}

VOID _app_setruleiteminfo (_In_ HWND hwnd, _In_ INT listview_id, _In_ INT item_id, _In_ PITEM_RULE ptr_rule, _In_ BOOLEAN include_apps)
{
	if (!listview_id || item_id == -1)
		return;

	WCHAR rule_name[RULE_NAME_CCH_MAX];
	LPCWSTR rule_name_ptr = NULL;
	PR_STRING direction_string;

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

	_r_listview_setitemex (hwnd, listview_id, item_id, 0, rule_name_ptr, I_IMAGECALLBACK, I_GROUPIDCALLBACK, 0);
	_r_listview_setitem (hwnd, listview_id, item_id, 1, ptr_rule->protocol ? _app_getprotoname (ptr_rule->protocol, AF_UNSPEC, NULL) : _r_locale_getstring (IDS_ANY));
	_r_listview_setitem (hwnd, listview_id, item_id, 2, _r_obj_getstringorempty (direction_string));

	_r_listview_setitemcheck (hwnd, listview_id, item_id, !!ptr_rule->is_enabled);

	if (direction_string)
		_r_obj_dereference (direction_string);

	if (include_apps)
	{
		ULONG_PTR hash_code;
		SIZE_T enum_key = 0;

		while (_r_obj_enumhashtable (ptr_rule->apps, NULL, &hash_code, &enum_key))
		{
			_app_updateitembylparam (hwnd, hash_code, TRUE);
		}
	}
}

VOID _app_ruleenable (_Inout_ PITEM_RULE ptr_rule, _In_ BOOLEAN is_enable, _In_ BOOLEAN is_createconfig)
{
	ptr_rule->is_enabled = is_enable;

	if (ptr_rule->is_readonly && !_r_obj_isstringempty (ptr_rule->name))
	{
		PITEM_RULE_CONFIG ptr_config;
		ULONG_PTR rule_hash = _r_obj_getstringhash (ptr_rule->name);

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
				_r_queuedlock_acquireexclusive (&lock_rules_config);

				ptr_config = _app_addruleconfigtable (rules_config, rule_hash, _r_obj_createstring2 (ptr_rule->name), is_enable);

				_r_queuedlock_releaseexclusive (&lock_rules_config);
			}
		}
	}
}

BOOLEAN _app_ruleblocklistsetchange (_Inout_ PITEM_RULE ptr_rule, _In_ INT new_state)
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

	ptr_rule->is_enabled = (new_state != 0);
	ptr_rule->is_enabled_default = ptr_rule->is_enabled; // set default value for rule

	if ((new_state != 1))
	{
		ptr_rule->action = FWP_ACTION_BLOCK;
	}
	else
	{
		ptr_rule->action = FWP_ACTION_PERMIT;
	}

	return TRUE;
}

BOOLEAN _app_ruleblocklistsetstate (_Inout_ PITEM_RULE ptr_rule, _In_ INT spy_state, _In_ INT update_state, _In_ INT extra_state)
{
	if (ptr_rule->type != DataRuleBlocklist || _r_obj_isstringempty (ptr_rule->name))
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

VOID _app_ruleblocklistset (_In_opt_ HWND hwnd, _In_ INT spy_state, _In_ INT update_state, _In_ INT extra_state, _In_ BOOLEAN is_instantapply)
{
	PR_LIST rules;
	PITEM_RULE ptr_rule;
	SIZE_T changes_count = 0;

	rules = _r_obj_createlist (&_r_obj_dereference);

	_r_queuedlock_acquireshared (&lock_rules);

	for (SIZE_T i = 0; i < _r_obj_getlistsize (rules_list); i++)
	{
		ptr_rule = _r_obj_getlistitem (rules_list, i);

		if (!ptr_rule)
			continue;

		if (ptr_rule->type != DataRuleBlocklist)
			continue;

		if (!_app_ruleblocklistsetstate (ptr_rule, spy_state, update_state, extra_state))
			continue;

		_app_ruleenable (ptr_rule, !!ptr_rule->is_enabled, FALSE);
		changes_count += 1;

		if (hwnd)
		{
			_app_updateitembylparam (hwnd, i, FALSE);
		}

		if (is_instantapply)
		{
			_r_obj_addlistitem (rules, _r_obj_reference (ptr_rule)); // dereference later!
		}
	}

	_r_queuedlock_releaseshared (&lock_rules);

	if (changes_count)
	{
		if (hwnd)
		{
			_app_updatelistviewbylparam (hwnd, DataRuleBlocklist, PR_UPDATE_TYPE);
		}

		if (is_instantapply)
		{
			if (rules->count)
			{
				if (_wfp_isfiltersinstalled ())
				{
					HANDLE hengine = _wfp_getenginehandle ();

					if (hengine)
						_wfp_create4filters (hengine, rules, __LINE__, FALSE);
				}
			}
		}

		_app_profile_save (); // required!
	}

	_r_obj_dereference (rules);
}

_Ret_maybenull_
PR_STRING _app_appexpandrules (_In_ ULONG_PTR app_hash, _In_ LPCWSTR delimeter)
{
	R_STRINGBUILDER buffer;
	R_STRINGREF delimeter_sr;
	PR_STRING string;
	PITEM_RULE ptr_rule;

	_r_obj_initializestringbuilder (&buffer);
	_r_obj_initializestringrefconst (&delimeter_sr, delimeter);

	_r_queuedlock_acquireshared (&lock_rules);

	for (SIZE_T i = 0; i < _r_obj_getlistsize (rules_list); i++)
	{
		ptr_rule = _r_obj_getlistitem (rules_list, i);

		if (!ptr_rule)
			continue;

		if (ptr_rule->is_enabled && ptr_rule->type == DataRuleUser && !_r_obj_isstringempty (ptr_rule->name) && _r_obj_findhashtable (ptr_rule->apps, app_hash))
		{
			_r_obj_appendstringbuilder2 (&buffer, ptr_rule->name);

			if (ptr_rule->is_readonly)
			{
				_r_obj_appendstringbuilder (&buffer, SZ_RULE_INTERNAL_MENU);
			}

			_r_obj_appendstringbuilder3 (&buffer, &delimeter_sr);
		}
	}

	_r_queuedlock_releaseshared (&lock_rules);

	string = _r_obj_finalstringbuilder (&buffer);

	_r_str_trimstring (string, &delimeter_sr);

	if (!_r_obj_isstringempty (string))
		return string;

	_r_obj_deletestringbuilder (&buffer);

	return NULL;

}

_Ret_maybenull_
PR_STRING _app_rulesexpandapps (_In_ PITEM_RULE ptr_rule, _In_ BOOLEAN is_fordisplay, _In_ LPCWSTR delimeter)
{
	R_STRINGBUILDER buffer;
	R_STRINGREF delimeter_sr;
	PR_STRING string;
	PITEM_APP ptr_app;
	ULONG_PTR hash_code;
	SIZE_T enum_key = 0;

	_r_obj_initializestringbuilder (&buffer);

	_r_obj_initializestringrefconst (&delimeter_sr, delimeter);

	if (is_fordisplay && ptr_rule->is_forservices)
	{
		string = _r_obj_concatstrings (4, PROC_SYSTEM_NAME, delimeter, _r_obj_getstring (config.svchost_path), delimeter);

		_r_obj_appendstringbuilder2 (&buffer, string);

		_r_obj_dereference (string);
	}

	while (_r_obj_enumhashtable (ptr_rule->apps, NULL, &hash_code, &enum_key))
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

		_r_obj_appendstringbuilder3 (&buffer, &delimeter_sr);

		_r_obj_dereference (ptr_app);
	}

	string = _r_obj_finalstringbuilder (&buffer);

	_r_str_trimstring (string, &delimeter_sr);

	if (!_r_obj_isstringempty (string))
		return string;

	_r_obj_deletestringbuilder (&buffer);

	return NULL;
}

_Ret_maybenull_
PR_STRING _app_rulesexpandrules (_In_ PR_STRING rule, _In_ LPCWSTR delimeter)
{
	R_STRINGBUILDER buffer;
	R_STRINGREF delimeter_sr;
	R_STRINGREF remaining_part;
	R_STRINGREF first_part;
	PR_STRING string;

	if (_r_obj_isstringempty (rule))
		return NULL;

	_r_obj_initializestringbuilder (&buffer);

	_r_obj_initializestringrefconst (&delimeter_sr, delimeter);

	_r_obj_initializestringref2 (&remaining_part, rule);

	while (remaining_part.length != 0)
	{
		_r_str_splitatchar (&remaining_part, DIVIDER_RULE[0], &first_part, &remaining_part);

		_r_obj_appendstringbuilder3 (&buffer, &first_part);
		_r_obj_appendstringbuilder3 (&buffer, &delimeter_sr);
	}

	string = _r_obj_finalstringbuilder (&buffer);

	_r_str_trimstring (string, &delimeter_sr);

	if (!_r_obj_isstringempty (string))
		return string;

	_r_obj_deletestringbuilder (&buffer);

	return NULL;
}

BOOLEAN _app_isappfromsystem (_In_opt_ PR_STRING path, _In_ ULONG_PTR app_hash)
{
	R_STRINGREF sr;

	if (app_hash == config.ntoskrnl_hash || app_hash == config.svchost_hash)
		return TRUE;

	if (path)
	{
		_r_obj_initializestringrefex (&sr, config.windows_dir, config.wd_length * sizeof (WCHAR));

		if (_r_str_isstartswith (&path->sr, &sr, TRUE))
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOLEAN _app_isapphaveconnection (_In_ ULONG_PTR app_hash)
{
	PITEM_NETWORK ptr_network;
	SIZE_T enum_key = 0;

	_r_queuedlock_acquireshared (&lock_network);

	while (_r_obj_enumhashtablepointer (network_table, &ptr_network, NULL, &enum_key))
	{
		if (ptr_network->app_hash == app_hash)
		{
			if (ptr_network->is_connection)
			{
				_r_queuedlock_releaseshared (&lock_network);

				return TRUE;
			}
		}
	}

	_r_queuedlock_releaseshared (&lock_network);

	return FALSE;
}

BOOLEAN _app_isapphavedrive (_In_ INT letter)
{
	PITEM_APP ptr_app;
	SIZE_T enum_key = 0;
	INT drive_id;

	_r_queuedlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtablepointer (apps_table, &ptr_app, NULL, &enum_key))
	{
		if (_r_obj_isstringempty (ptr_app->original_path))
			continue;

		if (ptr_app->type == DataAppRegular)
		{
			drive_id = PathGetDriveNumber (ptr_app->original_path->buffer);
		}
		else
		{
			drive_id = -1;
		}

		if (ptr_app->type == DataAppDevice || (drive_id != -1 && drive_id == letter))
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

BOOLEAN _app_isapphaverule (_In_ ULONG_PTR app_hash, _In_ BOOLEAN is_countdisabled)
{
	PITEM_RULE ptr_rule;

	_r_queuedlock_acquireshared (&lock_rules);

	for (SIZE_T i = 0; i < _r_obj_getlistsize (rules_list); i++)
	{
		ptr_rule = _r_obj_getlistitem (rules_list, i);

		if (ptr_rule)
		{
			if (ptr_rule->type == DataRuleUser && (is_countdisabled || (ptr_rule->is_enabled)))
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

	// Service and UWP is already undeletable
	//if (ptr_app->type == DataAppService || ptr_app->type == DataAppUWP)
	//	return TRUE;

	return FALSE;
}

BOOLEAN _app_isappfound (_In_ ULONG_PTR app_hash)
{
	BOOLEAN is_found;

	_r_queuedlock_acquireshared (&lock_apps);

	is_found = (_r_obj_findhashtable (apps_table, app_hash) != NULL);

	_r_queuedlock_releaseshared (&lock_apps);

	return is_found;
}

BOOLEAN _app_isappunused (_In_ PITEM_APP ptr_app)
{
	if (!ptr_app->is_undeletable)
	{
		if (!_app_isappexists (ptr_app))
			return TRUE;

		if (!_app_isappused (ptr_app))
			return TRUE;
	}

	return FALSE;
}

BOOLEAN _app_isnetworkfound (_In_ ULONG_PTR network_hash)
{
	BOOLEAN is_found;

	_r_queuedlock_acquireshared (&lock_network);

	is_found = (_r_obj_findhashtable (network_table, network_hash) != NULL);

	_r_queuedlock_releaseshared (&lock_network);

	return is_found;
}

BOOLEAN _app_profile_load_check (_In_ LPCWSTR path)
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
			if (_r_xml_getattribute_integer (&xml_library, L"type") == XmlProfileV3)
			{
				// min supported is v3
				if (_r_xml_getattribute_integer (&xml_library, L"version") >= XML_PROFILE_VER_3)

					is_success = TRUE;
			}
		}
	}

	_r_xml_destroylibrary (&xml_library);

	return is_success;
}

BOOLEAN _app_profile_load_check_node (_Inout_ PR_XML_LIBRARY xml_library, _In_ ENUM_TYPE_XML type)
{
	if (!xml_library->stream)
		return FALSE;

	return (_r_xml_getattribute_integer (xml_library, L"type") == type);
}

VOID _app_profile_load_fallback ()
{
	ULONG_PTR app_hash;

	if (!_app_isappfound (config.my_hash))
	{
		app_hash = _app_addapplication (NULL, DataUnknown, &config.my_path->sr, NULL, NULL);

		if (app_hash)
			_app_setappinfobyhash (app_hash, InfoIsEnabled, IntToPtr (TRUE));
	}

	_app_setappinfobyhash (config.my_hash, InfoIsUndeletable, IntToPtr (TRUE));

	// disable deletion for this shit ;)
	if (!_r_config_getboolean (L"IsInternalRulesDisabled", FALSE))
	{
		if (!_app_isappfound (config.ntoskrnl_hash) && config.system_path)
			_app_addapplication (NULL, DataUnknown, &config.system_path->sr, NULL, NULL);

		if (!_app_isappfound (config.svchost_hash) && config.svchost_path)
			_app_addapplication (NULL, DataUnknown, &config.svchost_path->sr, NULL, NULL);

		_app_setappinfobyhash (config.ntoskrnl_hash, InfoIsUndeletable, IntToPtr (TRUE));
		_app_setappinfobyhash (config.svchost_hash, InfoIsUndeletable, IntToPtr (TRUE));
	}
}

BOOLEAN _app_isrulesupportedbyos (_In_ PR_STRINGREF os_version)
{
	static PR_STRING current_version = NULL;
	INT result;

	if (!current_version)
		current_version = _r_format_string (L"%d.%d", NtCurrentPeb ()->OSMajorVersion, NtCurrentPeb ()->OSMinorVersion);

	result = _r_str_versioncompare (&current_version->sr, os_version);

	return (result != -1);
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

		string = _r_xml_getattribute_string (xml_library, L"path");

		if (!string)
			return;

		// workaround for native paths
		// https://github.com/henrypp/simplewall/issues/817
		if (_r_str_isstartswith2 (&string->sr, L"\\device\\", TRUE))
		{
			PR_STRING dos_path = _r_path_dospathfromnt (string->buffer);

			if (dos_path)
				_r_obj_movereference (&string, dos_path);
		}

		if (!_r_obj_isstringempty (string))
		{
			PITEM_APP ptr_app;
			ULONG_PTR app_hash;

			app_hash = _app_addapplication (NULL, DataUnknown, &string->sr, NULL, NULL);

			if (app_hash)
			{
				ptr_app = _app_getappitem (app_hash);

				if (ptr_app)
				{
					is_enabled = _r_xml_getattribute_boolean (xml_library, L"is_enabled");
					is_silent = _r_xml_getattribute_boolean (xml_library, L"is_silent");

					timestamp = _r_xml_getattribute_long64 (xml_library, L"timestamp");
					timer = _r_xml_getattribute_long64 (xml_library, L"timer");

					if (is_silent)
					{
						_app_setappinfo (ptr_app, InfoIsSilent, IntToPtr (is_silent));
					}

					if (is_enabled)
					{
						_app_setappinfo (ptr_app, InfoIsEnabled, IntToPtr (is_enabled));
					}

					if (timestamp)
					{
						_app_setappinfo (ptr_app, InfoTimestampPtr, &timestamp);
					}

					if (timer)
					{
						_app_setappinfo (ptr_app, InfoTimerPtr, &timer);
					}

					_r_obj_dereference (ptr_app);
				}
			}
		}

		_r_obj_dereference (string);
	}
	else if (type == DataRuleBlocklist || type == DataRuleSystem || type == DataRuleSystemUser || type == DataRuleUser)
	{
		PR_STRING rule_name;
		PR_STRING rule_remote;
		PR_STRING rule_local;
		R_STRINGREF os_version;
		FWP_DIRECTION direction;
		UINT8 protocol;
		ADDRESS_FAMILY af;
		PITEM_RULE ptr_rule;
		ULONG_PTR rule_hash;

		rule_name = _r_xml_getattribute_string (xml_library, L"name");

		if (!rule_name)
			return;

		// check support version
		if (_r_xml_getattribute (xml_library, L"os_version", &os_version))
		{
			if (!_app_isrulesupportedbyos (&os_version))
				return;
		}

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

		rule_hash = _r_obj_getstringhash (ptr_rule->name);

		ptr_rule->type = (type == DataRuleSystemUser) ? DataRuleUser : type;
		ptr_rule->action = _r_xml_getattribute_boolean (xml_library, L"is_block") ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT;
		ptr_rule->is_forservices = _r_xml_getattribute_boolean (xml_library, L"is_services");
		ptr_rule->is_readonly = (type != DataRuleUser);

		// calculate rule weight
		if (type == DataRuleBlocklist)
		{
			ptr_rule->weight = FW_WEIGHT_RULE_BLOCKLIST;
		}
		else if (type == DataRuleSystem || type == DataRuleSystemUser)
		{
			ptr_rule->weight = FW_WEIGHT_RULE_SYSTEM;
		}
		else if (type == DataRuleUser)
		{
			ptr_rule->weight = (ptr_rule->action == FWP_ACTION_BLOCK) ? FW_WEIGHT_RULE_USER_BLOCK : FW_WEIGHT_RULE_USER;
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

			is_internal = (type == DataRuleBlocklist || type == DataRuleSystem || type == DataRuleSystemUser);

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
						_r_obj_appendstringbuilder (&rule_apps, DIVIDER_APP);
					}

					_r_obj_appendstringbuilder2 (&rule_apps, ptr_config->apps);
				}

				string = _r_obj_finalstringbuilder (&rule_apps);

				if (!_r_obj_isstringempty (string))
				{
					if (version < XML_PROFILE_VER_3)
					{
						_r_str_replacechar (&string->sr, DIVIDER_RULE[0], DIVIDER_APP[0]); // for compat with old profiles
					}

					R_STRINGREF remaining_part;
					R_STRINGREF first_part;
					ULONG_PTR app_hash;

					_r_obj_initializestringref2 (&remaining_part, string);

					while (remaining_part.length != 0)
					{
						_r_str_splitatchar (&remaining_part, DIVIDER_APP[0], &first_part, &remaining_part);

						PR_STRING path_string = _r_str_expandenvironmentstring (&first_part);

						if (!path_string)
							path_string = _r_obj_createstring3 (&first_part);

						app_hash = _r_obj_getstringhash (path_string);

						if (app_hash)
						{
							if (ptr_rule->is_forservices && (app_hash == config.ntoskrnl_hash || app_hash == config.svchost_hash))
							{
								_r_obj_dereference (path_string);
								continue;
							}

							if (!_app_isappfound (app_hash))
							{
								app_hash = _app_addapplication (NULL, DataUnknown, &path_string->sr, NULL, NULL);
							}

							if (ptr_rule->type == DataRuleSystem)
							{
								_app_setappinfobyhash (app_hash, InfoIsUndeletable, IntToPtr (TRUE));
							}

							_r_obj_addhashtableitem (ptr_rule->apps, app_hash, NULL);
						}

						_r_obj_dereference (path_string);
					}
				}

				_r_obj_deletestringbuilder (&rule_apps);
			}
		}

		_r_queuedlock_acquireexclusive (&lock_rules);

		_r_obj_addlistitem (rules_list, ptr_rule);

		_r_queuedlock_releaseexclusive (&lock_rules);
	}
	else if (type == DataRulesConfig)
	{
		PR_STRING rule_name;
		PITEM_RULE_CONFIG ptr_config;
		ULONG_PTR rule_hash;

		rule_name = _r_xml_getattribute_string (xml_library, L"name");

		if (!rule_name)
			return;

		rule_hash = _r_obj_getstringhash (rule_name);

		if (rule_hash)
		{
			ptr_config = _app_getruleconfigitem (rule_hash);

			if (!ptr_config)
			{
				_r_queuedlock_acquireexclusive (&lock_rules_config);

				ptr_config = _app_addruleconfigtable (rules_config, rule_hash, _r_obj_reference (rule_name), _r_xml_getattribute_boolean (xml_library, L"is_enabled"));

				_r_queuedlock_releaseexclusive (&lock_rules_config);

				if (ptr_config)
				{
					ptr_config->apps = _r_xml_getattribute_string (xml_library, L"apps");

					if (ptr_config->apps && version < XML_PROFILE_VER_3)
					{
						_r_str_replacechar (&ptr_config->apps->sr, DIVIDER_RULE[0], DIVIDER_APP[0]); // for compat with old profiles
					}
				}
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
	LONG64 timestamp_file;
	LONG64 timestamp_resource;
	INT version_file;
	INT version_resource;
	INT version;
	BOOLEAN is_loadfromresource;

	_r_xml_initializelibrary (&xml_file, TRUE, NULL);
	_r_xml_initializelibrary (&xml_resource, TRUE, NULL);

	timestamp_file = 0;
	timestamp_resource = 0;

	version_file = 0;
	version_resource = 0;

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

	// NOTE: prefer new profile version for 3.4+
	is_loadfromresource = (version_file < version_resource) || (timestamp_file < timestamp_resource);

	xml_library = is_loadfromresource ? &xml_resource : &xml_file;
	version = is_loadfromresource ? version_resource : version_file;

	if (_app_profile_load_check_node (xml_library, XmlProfileInternalV3))
	{
		if (timestamp)
			*timestamp = (timestamp_file > timestamp_resource) ? timestamp_file : timestamp_resource;

		// load system rules
		if (_r_xml_findchildbytagname (xml_library, L"rules_system"))
		{
			while (_r_xml_enumchilditemsbytagname (xml_library, L"item"))
			{
				_app_profile_load_helper (xml_library, DataRuleSystem, version);
			}
		}

		// load internal custom rules
		if (_r_xml_findchildbytagname (xml_library, L"rules_custom"))
		{
			while (_r_xml_enumchilditemsbytagname (xml_library, L"item"))
			{
				_app_profile_load_helper (xml_library, DataRuleSystemUser, version);
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

	_r_xml_destroylibrary (&xml_file);
	_r_xml_destroylibrary (&xml_resource);
}

VOID _app_profile_load (_In_opt_ HWND hwnd, _In_opt_ LPCWSTR path_custom)
{
	R_XML_LIBRARY xml_library;
	HRESULT hr;

	INT current_listview_id;
	INT new_listview_id;
	INT selected_item;
	INT scroll_pos;

	// clean listview
	if (hwnd)
	{
		current_listview_id = _app_getcurrentlistview_id (hwnd);
		selected_item = (INT)SendDlgItemMessage (hwnd, current_listview_id, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
		scroll_pos = GetScrollPos (GetDlgItem (hwnd, current_listview_id), SB_VERT);

		for (INT i = IDC_APPS_PROFILE; i <= IDC_RULES_CUSTOM; i++)
			_r_listview_deleteallitems (hwnd, i);
	}

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

	// generate uwp apps list (win8+)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
		_app_generate_packages ();

	// generate services list
	_app_generate_services ();

	// load profile
	_r_xml_initializelibrary (&xml_library, TRUE, NULL);

	_r_queuedlock_acquireshared (&lock_profile);

	hr = _r_xml_parsefile (&xml_library, path_custom ? path_custom : config.profile_path);

	// load backup
	if (hr != S_OK && !path_custom)
	{
		hr = _r_xml_parsefile (&xml_library, config.profile_path_backup);
	}

	_r_queuedlock_releaseshared (&lock_profile);

	if (hr != S_OK)
	{
		if (hr != HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND))
		{
			_r_log (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"_r_xml_parsefile", hr, path_custom ? path_custom : config.profile_path);
		}
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
		LONG64 current_time;

		current_time = _r_unixtime_now ();

		// add apps
		_r_queuedlock_acquireshared (&lock_apps);

		while (_r_obj_enumhashtablepointer (apps_table, &ptr_app, NULL, &enum_key))
		{
			_app_addlistviewapp (hwnd, ptr_app, ptr_app->app_hash);

			// install timer
			if (ptr_app->timer)
			{
				_app_timer_set (hwnd, ptr_app, ptr_app->timer - current_time);
			}
		}

		_r_queuedlock_releaseshared (&lock_apps);

		// add rules
		_r_queuedlock_acquireshared (&lock_rules);

		for (SIZE_T i = 0; i < _r_obj_getlistsize (rules_list); i++)
		{
			ptr_rule = _r_obj_getlistitem (rules_list, i);

			if (!ptr_rule)
				continue;

			_app_addlistviewrule (hwnd, ptr_rule, i, FALSE);
		}

		_r_queuedlock_releaseshared (&lock_rules);
	}

	if (hwnd)
	{
		new_listview_id = _app_getcurrentlistview_id (hwnd);

		_app_updatelistviewbylparam (hwnd, new_listview_id, 0);

		if (_r_wnd_isvisible (hwnd) && current_listview_id == new_listview_id)
		{
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
		_r_log (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"_r_xml_initializelibrary", hr, NULL);

		return;
	}

	hr = _r_xml_createfile (&xml_library, config.profile_path);

	if (hr != S_OK)
	{
		_r_log (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"_r_xml_createfile", hr, config.profile_path);

		return;
	}

	_r_queuedlock_acquireexclusive (&lock_profile);

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

	_r_queuedlock_acquireshared (&lock_apps);

	enum_key = 0;

	while (_r_obj_enumhashtablepointer (apps_table, &ptr_app, NULL, &enum_key))
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
			_r_xml_setattribute_boolean (&xml_library, L"is_silent", !!ptr_app->is_silent);

		if (ptr_app->is_enabled)
			_r_xml_setattribute_boolean (&xml_library, L"is_enabled", !!ptr_app->is_enabled);

		_r_xml_writeendelement (&xml_library);
	}

	_r_queuedlock_releaseshared (&lock_apps);

	_r_xml_writewhitespace (&xml_library, L"\n\t");

	_r_xml_writeendelement (&xml_library);

	_r_xml_writewhitespace (&xml_library, L"\n\t");

	// save rules
	_r_xml_writestartelement (&xml_library, L"rules_custom");

	_r_queuedlock_acquireshared (&lock_rules);

	// save user rules
	for (SIZE_T i = 0; i < _r_obj_getlistsize (rules_list); i++)
	{
		ptr_rule = _r_obj_getlistitem (rules_list, i);

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

		if ((ptr_rule->action == FWP_ACTION_BLOCK))
			_r_xml_setattribute_boolean (&xml_library, L"is_block", (ptr_rule->action == FWP_ACTION_BLOCK));

		if (ptr_rule->is_enabled)
			_r_xml_setattribute_boolean (&xml_library, L"is_enabled", !!ptr_rule->is_enabled);

		_r_xml_writeendelement (&xml_library);
	}

	_r_queuedlock_releaseshared (&lock_rules);

	_r_xml_writewhitespace (&xml_library, L"\n\t");

	_r_xml_writeendelement (&xml_library);

	_r_xml_writewhitespace (&xml_library, L"\n\t");

	// save rules config
	_r_xml_writestartelement (&xml_library, L"rules_config");

	enum_key = 0;

	_r_queuedlock_acquireshared (&lock_rules_config);

	while (_r_obj_enumhashtable (rules_config, &ptr_config, NULL, &enum_key))
	{
		if (_r_obj_isstringempty (ptr_config->name))
			continue;

		BOOLEAN is_enabled_default;
		ULONG_PTR rule_hash;

		is_enabled_default = ptr_config->is_enabled;
		rule_hash = _r_obj_getstringhash (ptr_config->name);

		ptr_rule = _app_getrulebyhash (rule_hash);

		if (ptr_rule)
		{
			is_enabled_default = !!ptr_rule->is_enabled_default;

			if (ptr_rule->type == DataRuleUser && !_r_obj_ishashtableempty (ptr_rule->apps))
			{
				string = _app_rulesexpandapps (ptr_rule, FALSE, DIVIDER_APP);
			}

			_r_obj_dereference (ptr_rule);
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

	_r_queuedlock_releaseshared (&lock_rules_config);

	_r_xml_writewhitespace (&xml_library, L"\n\t");

	_r_xml_writeendelement (&xml_library);

	_r_xml_writewhitespace (&xml_library, L"\n");

	_r_xml_writeendelement (&xml_library);

	_r_xml_writewhitespace (&xml_library, L"\n");

	_r_xml_writeenddocument (&xml_library);

	_r_queuedlock_releaseexclusive (&lock_profile);

	_r_xml_destroylibrary (&xml_library);

	// make backup
	if (is_backuprequired)
	{
		_r_fs_copyfile (config.profile_path, config.profile_path_backup, 0);
		_r_config_setlong64 (L"BackupTimestamp", current_time);
	}
}
