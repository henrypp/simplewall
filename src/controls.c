// simplewall
// Copyright (c) 2016-2021 Henry++

#include "global.h"

INT _app_getlistviewbytab_id (_In_ HWND hwnd, _In_ INT tab_id)
{
	INT listview_id;

	if (tab_id == -1)
		tab_id = _r_tab_getcurrentitem (hwnd, IDC_TAB);

	listview_id = (INT)_r_tab_getitemlparam (hwnd, IDC_TAB, tab_id);

	return listview_id;
}

INT _app_getlistviewbytype_id (_In_ ENUM_TYPE_DATA type)
{
	if (type == DATA_APP_REGULAR || type == DATA_APP_DEVICE || type == DATA_APP_NETWORK || type == DATA_APP_PICO)
	{
		return IDC_APPS_PROFILE;
	}
	else if (type == DATA_APP_SERVICE)
	{
		return IDC_APPS_SERVICE;
	}
	else if (type == DATA_APP_UWP)
	{
		return IDC_APPS_UWP;
	}
	else if (type == DATA_RULE_BLOCKLIST)
	{
		return IDC_RULES_BLOCKLIST;
	}
	else if (type == DATA_RULE_SYSTEM)
	{
		return IDC_RULES_SYSTEM;
	}
	else if (type == DATA_RULE_SYSTEM_USER || type == DATA_RULE_USER)
	{
		return IDC_RULES_CUSTOM;
	}

	return 0;
}

VOID _app_setlistviewbylparam (_In_ HWND hwnd, _In_ ULONG_PTR lparam, _In_ ULONG flags, _In_ BOOLEAN is_app)
{
	INT listview_id;

	if (is_app)
	{
		listview_id = PtrToInt (_app_getappinfobyhash (lparam, INFO_LISTVIEW_ID));
	}
	else
	{
		listview_id = PtrToInt (_app_getruleinfobyid (lparam, INFO_LISTVIEW_ID));
	}

	if (listview_id)
	{
		if ((flags & PR_SETITEM_UPDATE))
		{
			_app_updatelistviewbylparam (hwnd, listview_id, 0);
		}

		if ((flags & PR_SETITEM_REDRAW))
		{
			if (listview_id == _app_getcurrentlistview_id (hwnd))
			{
				_r_listview_redraw (hwnd, listview_id, -1);
			}
		}
	}
}

VOID _app_updatelistviewbylparam (_In_ HWND hwnd, _In_ INT lparam, _In_ ULONG flags)
{
	INT listview_id;
	ENUM_TYPE_DATA type;

	if (flags & PR_UPDATE_TYPE)
	{
		type = lparam;

		if (type == DATA_LISTVIEW_CURRENT)
		{
			listview_id = _app_getcurrentlistview_id (hwnd);
		}
		else
		{
			listview_id = _app_getlistviewbytype_id (type);
		}
	}
	else
	{
		listview_id = lparam;
	}

	if ((flags & PR_UPDATE_FORCE) || (listview_id == _app_getcurrentlistview_id (hwnd)))
	{
		if (!(flags & PR_UPDATE_NOREDRAW))
			_r_listview_redraw (hwnd, listview_id, -1);

		if (!(flags & PR_UPDATE_NOSETVIEW))
		{
			_app_listviewsetfont (hwnd, listview_id);
			_app_listviewsetview (hwnd, listview_id);
		}

		if (!(flags & PR_UPDATE_NOREFRESH))
			_app_refreshgroups (hwnd, listview_id);

		if (!(flags & PR_UPDATE_NOSORT))
			_app_listviewsort (hwnd, listview_id, -1, FALSE);

		if (!(flags & PR_UPDATE_NORESIZE))
			_app_listviewresize (hwnd, listview_id, FALSE);
	}

	_app_refreshstatus (hwnd);
}

VOID _app_addlistviewapp (_In_ HWND hwnd, _In_ PITEM_APP ptr_app)
{
	INT listview_id;
	INT item_id;

	listview_id = _app_getlistviewbytype_id (ptr_app->type);

	if (listview_id)
	{
		item_id = _r_listview_getitemcount (hwnd, listview_id);

		_app_setcheckboxlock (hwnd, listview_id, TRUE);

		_r_listview_additem_ex (hwnd, listview_id, item_id, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK, I_GROUPIDCALLBACK, _app_createlistviewcontext (ptr_app->app_hash));
		_app_setappiteminfo (hwnd, listview_id, item_id, ptr_app);

		_app_setcheckboxlock (hwnd, listview_id, FALSE);
	}
}

VOID _app_addlistviewrule (_In_ HWND hwnd, _In_ PITEM_RULE ptr_rule, _In_ SIZE_T rule_idx, _In_ BOOLEAN is_forapp)
{
	INT listview_id;
	INT item_id;

	listview_id = _app_getlistviewbytype_id (ptr_rule->type);

	if (listview_id)
	{
		item_id = _r_listview_getitemcount (hwnd, listview_id);

		_app_setcheckboxlock (hwnd, listview_id, TRUE);

		_r_listview_additem_ex (hwnd, listview_id, item_id, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK, I_GROUPIDCALLBACK, _app_createlistviewcontext (rule_idx));
		_app_setruleiteminfo (hwnd, listview_id, item_id, ptr_rule, is_forapp);

		_app_setcheckboxlock (hwnd, listview_id, FALSE);
	}
}

VOID _app_showitembylparam (_In_ HWND hwnd, _In_ ULONG_PTR lparam, _In_ BOOLEAN is_app)
{
	INT listview_id;
	INT item_id;

	if (is_app)
	{
		listview_id = PtrToInt (_app_getappinfobyhash (lparam, INFO_LISTVIEW_ID));
	}
	else
	{
		listview_id = PtrToInt (_app_getruleinfobyid (lparam, INFO_LISTVIEW_ID));
	}

	if (listview_id)
	{
		if (listview_id != _app_getcurrentlistview_id (hwnd))
		{
			_app_listviewsort (hwnd, listview_id, -1, FALSE);
			_app_listviewresize (hwnd, listview_id, FALSE);
		}

		item_id = _app_getposition (hwnd, listview_id, lparam);

		if (item_id != -1)
		{
			_app_showitem (hwnd, listview_id, item_id, -1);
			_r_wnd_toggle (hwnd, TRUE);
		}
	}
}

VOID _app_updateitembyidx (_In_ HWND hwnd, _In_ INT listview_id, _In_ INT item_id)
{
	_r_listview_setitem_ex (hwnd, listview_id, item_id, 0, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK, I_GROUPIDCALLBACK, 0);
}

VOID _app_updateitembylparam (_In_ HWND hwnd, _In_ ULONG_PTR lparam, _In_ BOOLEAN is_app)
{
	INT listview_id;
	INT item_id;

	if (is_app)
	{
		listview_id = PtrToInt (_app_getappinfobyhash (lparam, INFO_LISTVIEW_ID));
	}
	else
	{
		listview_id = PtrToInt (_app_getruleinfobyid (lparam, INFO_LISTVIEW_ID));
	}

	if (listview_id)
	{
		item_id = _app_getposition (hwnd, listview_id, lparam);

		if (item_id != -1)
		{
			if (is_app)
			{
				PITEM_APP ptr_app = _app_getappitem (lparam);

				if (ptr_app)
				{
					_app_setcheckboxlock (hwnd, listview_id, TRUE);
					_app_setappiteminfo (hwnd, listview_id, item_id, ptr_app);
					_app_setcheckboxlock (hwnd, listview_id, FALSE);

					_r_obj_dereference (ptr_app);
				}
			}
			else
			{
				PITEM_RULE ptr_rule = _app_getrulebyid (lparam);

				if (ptr_rule)
				{
					_app_setcheckboxlock (hwnd, listview_id, TRUE);
					_app_setruleiteminfo (hwnd, listview_id, item_id, ptr_rule, FALSE);
					_app_setcheckboxlock (hwnd, listview_id, FALSE);

					_r_obj_dereference (ptr_rule);
				}
			}
		}
	}
}

VOID _app_getapptooltipstring (_Inout_ PR_STRINGBUILDER buffer, _In_ ULONG_PTR app_hash, _In_opt_ PITEM_NETWORK ptr_network, _In_opt_ PITEM_LOG ptr_log)
{
	PITEM_APP_INFO ptr_app_info;
	PITEM_APP ptr_app;
	PR_STRING tmp_string1;
	PR_STRING tmp_string2;
	PR_STRING tmp_string3;
	R_STRINGBUILDER sb;

	ptr_app = _app_getappitem (app_hash);
	ptr_app_info = _app_getappinfobyhash2 (app_hash);

	// file path
	tmp_string3 = NULL;

	if (ptr_app)
	{
		if (ptr_app->real_path)
		{
			tmp_string3 = ptr_app->real_path;
		}
		else if (ptr_app->display_name)
		{
			tmp_string3 = ptr_app->display_name;
		}
		else
		{
			tmp_string3 = ptr_app->original_path;
		}
	}
	else if (ptr_network)
	{
		if (ptr_network->path)
		{
			tmp_string3 = ptr_network->path;
		}
	}
	else if (ptr_log)
	{
		if (ptr_log->path)
		{
			tmp_string3 = ptr_log->path;
		}
	}

	if (tmp_string3)
	{
		_r_obj_appendstringbuilder2 (buffer, tmp_string3);
		_r_obj_appendstringbuilder (buffer, L"\r\n");
	}

	// file information
	_r_obj_initializestringbuilder (&sb);

	// file display name
	if (ptr_app)
	{
		if (ptr_app->display_name)
		{
			_r_obj_appendstringbuilder (&sb, SZ_TAB);
			_r_obj_appendstringbuilder2 (&sb, ptr_app->display_name);
			_r_obj_appendstringbuilder (&sb, L"\r\n");
		}
	}

	// file version
	if (ptr_app_info)
	{
		if (!_r_obj_isstringempty (ptr_app_info->version_info))
		{
			_r_obj_appendstringbuilder (&sb, SZ_TAB);
			_r_obj_appendstringbuilder2 (&sb, ptr_app_info->version_info);
			_r_obj_appendstringbuilder (&sb, L"\r\n");
		}
	}

	// compile
	if (!_r_obj_isstringempty2 (sb.string))
	{
		tmp_string1 = _r_obj_concatstrings (2, _r_locale_getstring (IDS_FILE), L":\r\n");

		_r_obj_insertstringbuilder2 (&sb, 0, tmp_string1);
		_r_obj_appendstringbuilder2 (buffer, sb.string);

		_r_obj_dereference (tmp_string1);
	}

	// file signature
	if (ptr_app_info)
	{
		if (!_r_obj_isstringempty (ptr_app_info->signature_info))
		{
			tmp_string1 = _r_obj_concatstrings (4, _r_locale_getstring (IDS_SIGNATURE), L":\r\n" SZ_TAB, ptr_app_info->signature_info->buffer, L"\r\n");

			_r_obj_appendstringbuilder2 (buffer, tmp_string1);

			_r_obj_dereference (tmp_string1);
		}
	}

	_r_obj_deletestringbuilder (&sb);

	// app timer
	if (ptr_app)
	{
		if (_app_istimerset (ptr_app->htimer))
		{
			tmp_string2 = _r_format_interval (ptr_app->timer - _r_unixtime_now (), 3);

			if (tmp_string2)
			{
				tmp_string1 = _r_obj_concatstrings (4, _r_locale_getstring (IDS_TIMELEFT), L":" SZ_TAB_CRLF, tmp_string2->buffer, L"\r\n");

				_r_obj_appendstringbuilder2 (buffer, tmp_string1);

				_r_obj_dereference (tmp_string2);
				_r_obj_dereference (tmp_string1);
			}
		}
	}

	// app rules
	tmp_string2 = _app_appexpandrules (app_hash, SZ_TAB_CRLF);

	if (tmp_string2)
	{
		tmp_string1 = _r_obj_concatstrings (4, _r_locale_getstring (IDS_RULE), L":" SZ_TAB_CRLF, tmp_string2->buffer, L"\r\n");

		_r_obj_appendstringbuilder2 (buffer, tmp_string1);

		_r_obj_dereference (tmp_string2);
		_r_obj_dereference (tmp_string1);
	}

	// app notes
	if (ptr_app)
	{
		_r_obj_initializestringbuilder (&sb);

		// app type
		if (ptr_app->type == DATA_APP_NETWORK)
		{
			_r_obj_appendstringbuilder (&sb, SZ_TAB);
			_r_obj_appendstringbuilder (&sb, _r_locale_getstring (IDS_HIGHLIGHT_NETWORK));
			_r_obj_appendstringbuilder (&sb, L"\r\n");
		}
		else if (ptr_app->type == DATA_APP_PICO)
		{
			_r_obj_appendstringbuilder (&sb, SZ_TAB);
			_r_obj_appendstringbuilder (&sb, _r_locale_getstring (IDS_HIGHLIGHT_PICO));
			_r_obj_appendstringbuilder (&sb, L"\r\n");
		}

		// app settings
		if (_app_isappfromsystem (ptr_app->real_path, app_hash))
		{
			_r_obj_appendstringbuilder (&sb, SZ_TAB);
			_r_obj_appendstringbuilder (&sb, _r_locale_getstring (IDS_HIGHLIGHT_SYSTEM));
			_r_obj_appendstringbuilder (&sb, L"\r\n");
		}

		if (_app_isapphaveconnection (app_hash))
		{
			_r_obj_appendstringbuilder (&sb, SZ_TAB);
			_r_obj_appendstringbuilder (&sb, _r_locale_getstring (IDS_HIGHLIGHT_CONNECTION));
			_r_obj_appendstringbuilder (&sb, L"\r\n");
		}

		if (ptr_app->is_silent)
		{
			_r_obj_appendstringbuilder (&sb, SZ_TAB);
			_r_obj_appendstringbuilder (&sb, _r_locale_getstring (IDS_HIGHLIGHT_SILENT));
			_r_obj_appendstringbuilder (&sb, L"\r\n");
		}

		if (!_app_isappexists (ptr_app))
		{
			_r_obj_appendstringbuilder (&sb, SZ_TAB);
			_r_obj_appendstringbuilder (&sb, _r_locale_getstring (IDS_HIGHLIGHT_INVALID));
			_r_obj_appendstringbuilder (&sb, L"\r\n");
		}

		tmp_string1 = _r_obj_finalstringbuilder (&sb);

		if (!_r_obj_isstringempty2 (tmp_string1))
		{
			_r_obj_insertstringbuilder (&sb, 0, L":\r\n");
			_r_obj_insertstringbuilder (&sb, 0, _r_locale_getstring (IDS_NOTES));

			_r_obj_appendstringbuilder2 (buffer, tmp_string1);
		}

		_r_obj_deletestringbuilder (&sb);

		_r_obj_dereference (ptr_app);
	}
}

_Ret_maybenull_
PR_STRING _app_gettooltipbylparam (_In_ HWND hwnd, _In_ INT listview_id, _In_ ULONG_PTR lparam)
{
	R_STRINGBUILDER buffer;
	PR_STRING string;
	PR_STRING string_tmp;

	_r_obj_initializestringbuilder (&buffer);

	if ((listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP) || listview_id == IDC_RULE_APPS_ID)
	{
		_app_getapptooltipstring (&buffer, lparam, NULL, NULL);
	}
	else if ((listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM) || listview_id == IDC_APP_RULES_ID)
	{
		PITEM_RULE ptr_rule;

		ptr_rule = _app_getrulebyid (lparam);

		if (ptr_rule)
		{
			PR_STRING rule_remote_string;
			PR_STRING rule_local_string;
			LPCWSTR empty_string;

			rule_remote_string = _app_rulesexpandrules (ptr_rule->rule_remote, L"\r\n" SZ_TAB);
			rule_local_string = _app_rulesexpandrules (ptr_rule->rule_local, L"\r\n" SZ_TAB);

			empty_string = _r_locale_getstring (IDS_STATUS_EMPTY);

			// rule information
			_r_obj_appendstringbuilderformat (&buffer, L"%s (#%" TEXT (PR_ULONG_PTR) L")\r\n%s (" SZ_DIRECTION_REMOTE L"):\r\n%s%s\r\n%s (" SZ_DIRECTION_LOCAL L"):\r\n%s%s",
											  _r_obj_getstringordefault (ptr_rule->name, empty_string),
											  lparam,
											  _r_locale_getstring (IDS_RULE),
											  SZ_TAB,
											  _r_obj_getstringordefault (rule_remote_string, empty_string),
											  _r_locale_getstring (IDS_RULE),
											  SZ_TAB,
											  _r_obj_getstringordefault (rule_local_string, empty_string)
			);

			if (rule_remote_string)
				_r_obj_dereference (rule_remote_string);

			if (rule_local_string)
				_r_obj_dereference (rule_local_string);

			// rule apps
			if (ptr_rule->is_forservices || !_r_obj_ishashtableempty (ptr_rule->apps))
			{
				string = _app_rulesexpandapps (ptr_rule, TRUE, SZ_TAB_CRLF);

				if (string)
				{
					string_tmp = _r_obj_concatstrings (4, L"\r\n", _r_locale_getstring (IDS_TAB_APPS), L":\r\n" SZ_TAB, string->buffer);

					_r_obj_appendstringbuilder2 (&buffer, string_tmp);

					_r_obj_dereference (string_tmp);
					_r_obj_dereference (string);
				}
			}

			// rule notes
			if (ptr_rule->is_readonly && ptr_rule->type == DATA_RULE_USER)
			{
				string_tmp = _r_obj_concatstrings (5, SZ_TAB L"\r\n", _r_locale_getstring (IDS_NOTES), L":\r\n" SZ_TAB, SZ_RULE_INTERNAL_TITLE, L"\r\n");

				_r_obj_appendstringbuilder2 (&buffer, string_tmp);

				_r_obj_dereference (string_tmp);
			}

			_r_obj_dereference (ptr_rule);
		}
	}
	else if (listview_id == IDC_NETWORK)
	{
		PITEM_NETWORK ptr_network;

		ptr_network = _app_getnetworkitem (lparam);

		if (ptr_network)
		{
			_app_getapptooltipstring (&buffer, ptr_network->app_hash, ptr_network, NULL);

			_r_obj_dereference (ptr_network);
		}
	}
	else if (listview_id == IDC_LOG)
	{
		PITEM_LOG ptr_log;

		ptr_log = _app_getlogitem (lparam);

		if (ptr_log)
		{
			_app_getapptooltipstring (&buffer, ptr_log->app_hash, NULL, ptr_log);

			_r_obj_dereference (ptr_log);
		}
	}

	string = _r_obj_finalstringbuilder (&buffer);

	if (!_r_obj_isstringempty2 (string))
		return string;

	_r_obj_deletestringbuilder (&buffer);

	return NULL;
}

VOID _app_settab_id (_In_ HWND hwnd, _In_ INT page_id)
{
	HWND hctrl;
	INT item_count;
	INT listview_id;

	if (!page_id)
		return;

	hctrl = GetDlgItem (hwnd, page_id);

	if (!hctrl || (_app_getcurrentlistview_id (hwnd) == page_id && _r_wnd_isvisible (hctrl)))
		return;

	item_count = _r_tab_getitemcount (hwnd, IDC_TAB);

	if (!item_count)
		return;

	for (INT i = 0; i < item_count; i++)
	{
		listview_id = _app_getlistviewbytab_id (hwnd, i);

		if (listview_id == page_id)
		{
			_r_tab_selectitem (hwnd, IDC_TAB, i);
			return;
		}
	}

	if (page_id != IDC_APPS_PROFILE)
		_app_settab_id (hwnd, IDC_APPS_PROFILE);
}

UINT _app_getinterfacestatelocale (_In_ ENUM_INSTALL_TYPE install_type)
{
	if (install_type == INSTALL_ENABLED)
		return IDS_STATUS_FILTERS_ACTIVE;

	else if (install_type == INSTALL_ENABLED_TEMPORARY)
		return IDS_STATUS_FILTERS_ACTIVE_TEMP;

	// INSTALL_DISABLED
	return IDS_STATUS_FILTERS_INACTIVE;
}

BOOLEAN _app_initinterfacestate (_In_ HWND hwnd, _In_ BOOLEAN is_forced)
{
	if (is_forced || _r_toolbar_isbuttonenabled (config.hrebar, IDC_TOOLBAR, IDM_TRAY_START))
	{
		_r_toolbar_enablebutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_START, FALSE);
		_r_toolbar_enablebutton (config.hrebar, IDC_TOOLBAR, IDM_REFRESH, FALSE);

		_r_status_settextformat (hwnd, IDC_STATUSBAR, 0, L"%s...", _r_locale_getstring (IDS_STATUS_FILTERS_PROCESSING));

		return TRUE;
	}

	return FALSE;
}

VOID _app_restoreinterfacestate (_In_ HWND hwnd, _In_ BOOLEAN is_enabled)
{
	if (!is_enabled)
		return;

	_r_toolbar_enablebutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_START, TRUE);
	_r_toolbar_enablebutton (config.hrebar, IDC_TOOLBAR, IDM_REFRESH, TRUE);

	_r_status_settext (hwnd, IDC_STATUSBAR, 0, _r_locale_getstring (_app_getinterfacestatelocale (_wfp_isfiltersinstalled ())));
}

VOID _app_setinterfacestate (_In_ HWND hwnd)
{
	ENUM_INSTALL_TYPE install_type;

	HICON hico_sm;
	HICON hico_big;


	LONG dpi_value;

	LONG icon_small_x;
	LONG icon_small_y;

	LONG icon_large_x;
	LONG icon_large_y;

	UINT string_id;
	UINT icon_id;

	BOOLEAN is_filtersinstalled;

	install_type = _wfp_isfiltersinstalled ();
	is_filtersinstalled = (install_type != INSTALL_DISABLED);

	dpi_value = _r_dc_getsystemdpi ();

	icon_small_x = _r_dc_getsystemmetrics (SM_CXSMICON, dpi_value);
	icon_small_y = _r_dc_getsystemmetrics (SM_CYSMICON, dpi_value);

	icon_large_x = _r_dc_getsystemmetrics (SM_CXICON, dpi_value);
	icon_large_y = _r_dc_getsystemmetrics (SM_CYICON, dpi_value);

	string_id = is_filtersinstalled ? IDS_TRAY_STOP : IDS_TRAY_START;
	icon_id = is_filtersinstalled ? IDI_ACTIVE : IDI_INACTIVE;

	hico_sm = _r_sys_loadsharedicon (_r_sys_getimagebase (), MAKEINTRESOURCE (icon_id), icon_small_x, icon_small_y);
	hico_big = _r_sys_loadsharedicon (_r_sys_getimagebase (), MAKEINTRESOURCE (icon_id), icon_large_x, icon_large_y);

	_r_wnd_seticon (hwnd, hico_sm, hico_big);

	//SendDlgItemMessage (hwnd, IDC_STATUSBAR, SB_SETICON, 0, (LPARAM)hico_sm);

	if (!_wfp_isfiltersapplying ())
		_r_status_settext (hwnd, IDC_STATUSBAR, 0, _r_locale_getstring (_app_getinterfacestatelocale (install_type)));

	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_START, _r_locale_getstring (string_id), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, 0, is_filtersinstalled ? 1 : 0);

	_r_tray_setinfo (hwnd, &GUID_TrayIcon, hico_sm, _r_app_getname ());
}

VOID _app_imagelist_init (_In_ HWND hwnd)
{
	static UINT toolbar_ids[] = {
		IDP_SHIELD_ENABLE,
		IDP_SHIELD_DISABLE,
		IDP_REFRESH,
		IDP_SETTINGS,
		IDP_NOTIFICATIONS,
		IDP_LOG,
		IDP_LOGOPEN,
		IDP_LOGCLEAR,
		IDP_ADD,
		IDP_DONATE,
		IDP_LOGUI,
	};

	static UINT rules_ids[] = {
		IDP_ALLOW,
		IDP_BLOCK
	};

	HBITMAP hbitmap;

	LONG dpi_value;

	LONG icon_small_x;
	LONG icon_small_y;

	LONG icon_large_x;
	LONG icon_large_y;

	LONG icon_size_toolbar;

	SAFE_DELETE_OBJECT (config.hbmp_enable);
	SAFE_DELETE_OBJECT (config.hbmp_disable);
	SAFE_DELETE_OBJECT (config.hbmp_allow);
	SAFE_DELETE_OBJECT (config.hbmp_block);

	dpi_value = _r_dc_getwindowdpi (hwnd);

	icon_small_x = _r_dc_getsystemmetrics (SM_CXSMICON, dpi_value);
	icon_small_y = _r_dc_getsystemmetrics (SM_CYSMICON, dpi_value);

	icon_large_x = _r_dc_getsystemmetrics (SM_CXICON, dpi_value);
	icon_large_y = _r_dc_getsystemmetrics (SM_CYICON, dpi_value);

	icon_size_toolbar = _r_calc_clamp32 (_r_dc_getdpi (_r_config_getlong (L"ToolbarSize", PR_SIZE_ITEMHEIGHT), dpi_value), icon_small_x, icon_large_x);

	config.hbmp_enable = _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_SHIELD_ENABLE), icon_small_x, icon_small_y);
	config.hbmp_disable = _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_SHIELD_DISABLE), icon_small_x, icon_small_y);

	config.hbmp_allow = _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_ALLOW), icon_small_x, icon_small_y);
	config.hbmp_block = _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_BLOCK), icon_small_x, icon_small_y);

	// toolbar imagelist
	if (config.himg_toolbar)
	{
		ImageList_SetIconSize (config.himg_toolbar, icon_size_toolbar, icon_size_toolbar);
	}
	else
	{
		config.himg_toolbar = ImageList_Create (icon_size_toolbar, icon_size_toolbar, ILC_COLOR32 | ILC_HIGHQUALITYSCALE, RTL_NUMBER_OF (toolbar_ids), RTL_NUMBER_OF (toolbar_ids));
	}

	if (config.himg_toolbar)
	{
		for (SIZE_T i = 0; i < RTL_NUMBER_OF (toolbar_ids); i++)
		{
			hbitmap = _app_bitmapfrompng (NULL, MAKEINTRESOURCE (toolbar_ids[i]), icon_size_toolbar, icon_size_toolbar);

			if (hbitmap)
				ImageList_Add (config.himg_toolbar, hbitmap, NULL);
		}
	}

	SendDlgItemMessage (GetDlgItem (hwnd, IDC_TOOLBAR) ? hwnd : config.hrebar, IDC_TOOLBAR, TB_SETIMAGELIST, 0, (LPARAM)config.himg_toolbar);

	// rules imagelist (small)
	if (config.himg_rules_small)
	{
		ImageList_SetIconSize (config.himg_rules_small, icon_small_x, icon_small_y);
	}
	else
	{
		config.himg_rules_small = ImageList_Create (icon_small_x, icon_small_y, ILC_COLOR32 | ILC_HIGHQUALITYSCALE, RTL_NUMBER_OF (rules_ids), RTL_NUMBER_OF (rules_ids));
	}

	if (config.himg_rules_small)
	{
		for (SIZE_T i = 0; i < RTL_NUMBER_OF (rules_ids); i++)
		{
			hbitmap = _app_bitmapfrompng (NULL, MAKEINTRESOURCE (rules_ids[i]), icon_small_x, icon_small_y);

			if (hbitmap)
				ImageList_Add (config.himg_rules_small, hbitmap, NULL);
		}
	}

	// rules imagelist (large)
	if (config.himg_rules_large)
	{
		ImageList_SetIconSize (config.himg_rules_large, icon_large_x, icon_large_y);
	}
	else
	{
		config.himg_rules_large = ImageList_Create (icon_large_x, icon_large_y, ILC_COLOR32 | ILC_HIGHQUALITYSCALE, RTL_NUMBER_OF (rules_ids), RTL_NUMBER_OF (rules_ids));
	}

	if (config.himg_rules_large)
	{
		for (SIZE_T i = 0; i < RTL_NUMBER_OF (rules_ids); i++)
		{
			hbitmap = _app_bitmapfrompng (NULL, MAKEINTRESOURCE (rules_ids[i]), icon_large_x, icon_large_y);

			if (hbitmap)
				ImageList_Add (config.himg_rules_large, hbitmap, NULL);
		}
	}
}

VOID _app_listviewresize (_In_ HWND hwnd, _In_ INT listview_id, _In_ BOOLEAN is_forced)
{
	if (!is_forced && !_r_config_getboolean (L"AutoSizeColumns", TRUE))
		return;

	PR_STRING column_text;
	PR_STRING item_text;
	HWND hlistview;
	HWND hheader = NULL;
	HDC hdc_listview = NULL;
	HDC hdc_header = NULL;
	LONG dpi_value;
	INT column_count;
	INT item_count;
	INT column_width;
	INT text_width;
	INT calculated_width;
	INT column_general_id;
	INT total_width;
	INT max_width;
	INT spacing;
	BOOLEAN is_tableview;

	hlistview = GetDlgItem (hwnd, listview_id);

	if (!hlistview)
		return;

	column_count = _r_listview_getcolumncount (hwnd, listview_id);

	if (!column_count)
		return;

	// get device context and fix font set
	hdc_listview = GetDC (hlistview);

	if (!hdc_listview)
		goto CleanupExit;

	hheader = (HWND)SendMessage (hlistview, LVM_GETHEADER, 0, 0);

	hdc_header = GetDC (hheader);

	if (!hdc_header)
		goto CleanupExit;

	_r_dc_fixwindowfont (hdc_listview, hlistview); // fix
	_r_dc_fixwindowfont (hdc_header, hheader); // fix

	calculated_width = 0;
	column_general_id = 0; // set general column id

	is_tableview = (_r_listview_getview (hwnd, listview_id) == LV_VIEW_DETAILS);

	dpi_value = _r_dc_getwindowdpi (hwnd);

	max_width = _r_dc_getdpi (158, dpi_value);
	spacing = _r_dc_getsystemmetrics (SM_CXSMICON, dpi_value);

	total_width = _r_ctrl_getwidth (hwnd, listview_id);
	item_count = _r_listview_getitemcount (hwnd, listview_id);

	for (INT i = 0; i < column_count; i++)
	{
		if (i == column_general_id)
			continue;

		// get column text width
		column_text = _r_listview_getcolumntext (hwnd, listview_id, i);

		if (!column_text)
			continue;

		column_width = _r_dc_getfontwidth (hdc_header, &column_text->sr) + spacing;

		if (column_width >= max_width)
		{
			column_width = max_width;
		}
		else
		{
			// calculate max width of listview subitems (only for details view)
			if (is_tableview)
			{
				for (INT j = 0; j < item_count; j++)
				{
					item_text = _r_listview_getitemtext (hwnd, listview_id, j, i);

					if (item_text)
					{
						text_width = _r_dc_getfontwidth (hdc_listview, &item_text->sr) + spacing;

						_r_obj_dereference (item_text);

						// do not continue reaching higher and higher values for performance reason!
						if (text_width >= max_width)
						{
							column_width = max_width;
							break;
						}

						if (text_width > column_width)
							column_width = text_width;
					}
				}
			}
		}

		_r_listview_setcolumn (hwnd, listview_id, i, NULL, column_width);

		calculated_width += column_width;

		_r_obj_dereference (column_text);
	}

	// set general column width
	_r_listview_setcolumn (hwnd, listview_id, column_general_id, NULL, max (total_width - calculated_width, max_width));

CleanupExit:

	if (hdc_listview)
		ReleaseDC (hlistview, hdc_listview);

	if (hdc_header)
		ReleaseDC (hheader, hdc_header);
}

VOID _app_listviewsetview (_In_ HWND hwnd, _In_ INT listview_id)
{
	HIMAGELIST himg;
	LONG view_type;
	LONG icons_size;
	BOOLEAN is_mainview;

	is_mainview = (listview_id >= IDC_APPS_PROFILE) && (listview_id <= IDC_RULES_CUSTOM);
	view_type = is_mainview ? _r_calc_clamp32 (_r_config_getlong (L"ViewType", LV_VIEW_DETAILS), LV_VIEW_ICON, LV_VIEW_MAX) : LV_VIEW_DETAILS;
	icons_size = is_mainview ? _r_calc_clamp32 (_r_config_getlong (L"IconSize", SHIL_SMALL), SHIL_LARGE, SHIL_LAST) : SHIL_SMALL;

	if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM || listview_id == IDC_APP_RULES_ID)
	{
		himg = (icons_size == SHIL_SMALL || icons_size == SHIL_SYSSMALL) ? config.himg_rules_small : config.himg_rules_large;
	}
	else
	{
		SHGetImageList (icons_size, &IID_IImageList2, &himg);
	}

	if (himg)
		_r_listview_setimagelist (hwnd, listview_id, himg);

	_r_listview_setview (hwnd, listview_id, view_type);
}

VOID _app_listviewloadfont (_In_ HWND hwnd, _In_ BOOLEAN is_forced)
{
	LOGFONT logfont = {0};

	if (is_forced || !config.hfont)
	{
		SAFE_DELETE_OBJECT (config.hfont);

		_r_config_getfont (L"Font", &logfont, _r_dc_getwindowdpi (hwnd));

		config.hfont = CreateFontIndirect (&logfont);
	}
}

VOID _app_listviewsetfont (_In_ HWND hwnd, _In_ INT listview_id)
{
	if (config.hfont)
		SendDlgItemMessage (hwnd, listview_id, WM_SETFONT, (WPARAM)config.hfont, TRUE);
}

INT CALLBACK _app_listviewcompare_callback (_In_ LPARAM lparam1, _In_ LPARAM lparam2, _In_ LPARAM lparam)
{
	WCHAR config_name[128];
	HWND hwnd;
	LONG column_id;
	INT listview_id;
	INT result;
	INT item_id1;
	INT item_id2;
	BOOLEAN is_descend;

	hwnd = GetParent ((HWND)lparam);

	if (!hwnd)
		return 0;

	listview_id = GetDlgCtrlID ((HWND)lparam);

	if (!listview_id)
		return 0;

	item_id1 = (INT)(INT_PTR)lparam1;
	item_id2 = (INT)(INT_PTR)lparam2;

	_r_str_printf (config_name, RTL_NUMBER_OF (config_name), L"listview\\%04" TEXT (PRIX32), listview_id);

	column_id = _r_config_getlong_ex (L"SortColumn", 0, config_name);
	is_descend = _r_config_getboolean_ex (L"SortIsDescending", FALSE, config_name);

	result = 0;

	if ((_r_listview_getexstyle (hwnd, listview_id) & LVS_EX_CHECKBOXES) != 0)
	{
		BOOLEAN is_checked1;
		BOOLEAN is_checked2;

		is_checked1 = _r_listview_isitemchecked (hwnd, listview_id, item_id1);
		is_checked2 = _r_listview_isitemchecked (hwnd, listview_id, item_id2);

		if (is_checked1 != is_checked2)
		{
			if (is_checked1 && !is_checked2)
			{
				result = is_descend ? 1 : -1;
			}
			else if (!is_checked1 && is_checked2)
			{
				result = is_descend ? -1 : 1;
			}
		}
	}

	if (!result)
	{
		// timestamp sorting
		if ((listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP) && column_id == 1)
		{
			PVOID timer1_ptr;
			PVOID timer2_ptr;

			timer1_ptr = _app_getappinfobyhash (_app_getlistviewitemcontext (hwnd, listview_id, item_id1), INFO_TIMESTAMP_PTR);
			timer2_ptr = _app_getappinfobyhash (_app_getlistviewitemcontext (hwnd, listview_id, item_id2), INFO_TIMESTAMP_PTR);

			if (timer1_ptr && timer2_ptr)
			{
				LONG64 timestamp1 = *((PLONG64)timer1_ptr);
				LONG64 timestamp2 = *((PLONG64)timer2_ptr);

				if (timestamp1 < timestamp2)
				{
					result = -1;
				}
				else if (timestamp1 > timestamp2)
				{
					result = 1;
				}
			}
		}
		else if (listview_id == IDC_LOG && column_id == 11)
		{
			PITEM_LOG ptr_log1;
			PITEM_LOG ptr_log2;

			ptr_log1 = _app_getlogitem (_app_getlistviewitemcontext (hwnd, listview_id, item_id1));
			ptr_log2 = _app_getlogitem (_app_getlistviewitemcontext (hwnd, listview_id, item_id2));

			if (ptr_log1 && ptr_log2)
			{
				if (ptr_log1->timestamp < ptr_log2->timestamp)
				{
					result = -1;
				}
				else if (ptr_log1->timestamp > ptr_log2->timestamp)
				{
					result = 1;
				}
			}

			if (ptr_log1)
				_r_obj_dereference (ptr_log1);

			if (ptr_log2)
				_r_obj_dereference (ptr_log2);
		}
	}

	if (!result)
	{
		PR_STRING item_text_1;
		PR_STRING item_text_2;

		item_text_1 = _r_listview_getitemtext (hwnd, listview_id, item_id1, column_id);
		item_text_2 = _r_listview_getitemtext (hwnd, listview_id, item_id2, column_id);

		if (item_text_1 && item_text_2)
		{
			result = _r_str_compare_logical (item_text_1, item_text_2);
		}

		if (item_text_1)
			_r_obj_dereference (item_text_1);

		if (item_text_2)
			_r_obj_dereference (item_text_2);
	}

	return is_descend ? -result : result;
}

VOID _app_listviewsort (_In_ HWND hwnd, _In_ INT listview_id, _In_ LONG column_id, _In_ BOOLEAN is_notifycode)
{
	HWND hlistview;
	INT column_count;
	BOOLEAN is_descend;

	hlistview = GetDlgItem (hwnd, listview_id);

	if (!hlistview)
		return;

	if ((GetWindowLongPtr (hlistview, GWL_STYLE) & (LVS_NOSORTHEADER | LVS_OWNERDATA)) != 0)
		return;

	column_count = _r_listview_getcolumncount (hwnd, listview_id);

	if (!column_count)
		return;

	WCHAR config_name[128];
	_r_str_printf (config_name, RTL_NUMBER_OF (config_name), L"listview\\%04" TEXT (PRIX32), listview_id);

	is_descend = _r_config_getboolean_ex (L"SortIsDescending", FALSE, config_name);

	if (is_notifycode)
		is_descend = !is_descend;

	if (column_id == -1)
		column_id = _r_config_getlong_ex (L"SortColumn", 0, config_name);

	column_id = _r_calc_clamp32 (column_id, 0, column_count - 1); // set range

	if (is_notifycode)
	{
		_r_config_setboolean_ex (L"SortIsDescending", is_descend, config_name);
		_r_config_setlong_ex (L"SortColumn", column_id, config_name);
	}

	for (INT i = 0; i < column_count; i++)
		_r_listview_setcolumnsortindex (hwnd, listview_id, i, 0);

	_r_listview_setcolumnsortindex (hwnd, listview_id, column_id, is_descend ? -1 : 1);

	SendMessage (hlistview, LVM_SORTITEMSEX, (WPARAM)hlistview, (LPARAM)&_app_listviewcompare_callback);
}

VOID _app_toolbar_init (_In_ HWND hwnd)
{
	HWND hctrl;
	ULONG button_size;

	config.hrebar = GetDlgItem (hwnd, IDC_REBAR);

	_r_toolbar_setstyle (hwnd, IDC_TOOLBAR, TBSTYLE_EX_DOUBLEBUFFER | TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_HIDECLIPPEDBUTTONS);

	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_TRAY_START, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, I_IMAGENONE);
	_r_toolbar_addseparator (hwnd, IDC_TOOLBAR);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_OPENRULESEDITOR, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, 8);
	_r_toolbar_addseparator (hwnd, IDC_TOOLBAR);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_TRAY_ENABLENOTIFICATIONS_CHK, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, 4);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_TRAY_ENABLELOG_CHK, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, 5);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_TRAY_ENABLEUILOG_CHK, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, 10);
	_r_toolbar_addseparator (hwnd, IDC_TOOLBAR);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_REFRESH, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, 2);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_SETTINGS, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, 3);
	_r_toolbar_addseparator (hwnd, IDC_TOOLBAR);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_TRAY_LOGSHOW, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, 6);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_TRAY_LOGCLEAR, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, 7);
	_r_toolbar_addseparator (hwnd, IDC_TOOLBAR);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_DONATE, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, 9);

	_r_toolbar_resize (hwnd, IDC_TOOLBAR);

	// insert toolbar
	hctrl = GetDlgItem (hwnd, IDC_TOOLBAR);

	if (hctrl)
	{
		button_size = _r_toolbar_getbuttonsize (hwnd, IDC_TOOLBAR);

		_r_rebar_insertband (hwnd, IDC_REBAR, 0, hctrl, RBBS_VARIABLEHEIGHT | RBBS_NOGRIPPER | RBBS_USECHEVRON, LOWORD (button_size), HIWORD (button_size));
	}

	// insert searchbar
	hctrl = GetDlgItem (hwnd, IDC_SEARCH);

	if (hctrl)
	{
		_app_search_initialize (hctrl);

		_r_rebar_insertband (hwnd, IDC_REBAR, 1, hctrl, RBBS_VARIABLEHEIGHT | RBBS_NOGRIPPER | RBBS_USECHEVRON, _r_dc_getdpi (180, _r_dc_getwindowdpi (hwnd)), _r_rebar_getheight (hwnd, IDC_REBAR));
	}
}

VOID _app_toolbar_resize ()
{
	REBARBANDINFO rbi = {0};
	SIZE ideal_size = {0};
	UINT index;

	SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_AUTOSIZE, 0, 0);

	index = (UINT)SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, RB_IDTOINDEX, 0, 0);

	if (index == UINT_MAX)
		return;

	rbi.cbSize = sizeof (rbi);
	rbi.fMask |= RBBIM_IDEALSIZE;

	if (SendMessage (config.hrebar, RB_GETBANDINFO, (WPARAM)index, (LPARAM)&rbi))
	{
		if (SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_GETIDEALSIZE, FALSE, (LPARAM)&ideal_size))
		{
			rbi.cxIdeal = (UINT)ideal_size.cx;

			SendMessage (config.hrebar, RB_SETBANDINFO, (WPARAM)index, (LPARAM)&rbi);
		}
	}
}

VOID _app_refreshgroups (_In_ HWND hwnd, _In_ INT listview_id)
{
	UINT group1_title = 0;
	UINT group2_title = 0;
	UINT group3_title = 0;
	UINT group4_title = 0;

	if (!_r_listview_isgroupviewenabled (hwnd, listview_id))
		return;

	if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
	{
		group1_title = IDS_GROUP_ALLOWED;
		group2_title = IDS_GROUP_SPECIAL_APPS;
		group3_title = IDS_GROUP_BLOCKED;
		group4_title = IDS_GROUP_BLOCKED;
	}
	else if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM)
	{
		group1_title = IDS_GROUP_ENABLED;
		group2_title = IDS_STATUS_EMPTY;
		group3_title = IDS_GROUP_DISABLED;
	}
	else if (listview_id == IDC_RULE_APPS_ID || listview_id == IDC_NETWORK)
	{
		group1_title = IDS_TAB_APPS;
		group2_title = IDS_TAB_SERVICES;
		group3_title = IDS_TAB_PACKAGES;
	}
	else if (listview_id == IDC_APP_RULES_ID)
	{
		group1_title = IDS_TRAY_SYSTEM_RULES;
		group2_title = IDS_TRAY_USER_RULES;
	}
	else
	{
		return;
	}

	INT total_count;

	INT group1_count = 0;
	INT group2_count = 0;
	INT group3_count = 0;
	INT group4_count = 0;
	INT group_id;

	total_count = _r_listview_getitemcount (hwnd, listview_id);

	for (INT i = 0; i < total_count; i++)
	{
		if (listview_id == IDC_RULE_APPS_ID || listview_id == IDC_APP_RULES_ID)
		{
			if (_r_listview_isitemchecked (hwnd, listview_id, i))
			{
				group1_count = group2_count = group3_count += 1;
			}
		}
		else
		{
			group_id = _r_listview_getitemgroup (hwnd, listview_id, i);

			if (group_id == 3)
			{
				group4_count += 1;
			}
			else if (group_id == 2)
			{
				group3_count += 1;
			}
			else if (group_id == 1)
			{
				group2_count += 1;
			}
			else
			{
				group1_count += 1;
			}
		}
	}

	WCHAR group1_string[128] = {0};
	WCHAR group2_string[128] = {0};
	WCHAR group3_string[128] = {0};
	WCHAR group4_string[128] = {0};
	PR_STRING localized_string = NULL;

	if (total_count)
	{
		_r_str_printf (group1_string, RTL_NUMBER_OF (group1_string), L" (%d/%d)", group1_count, total_count);
		_r_str_printf (group2_string, RTL_NUMBER_OF (group2_string), L" (%d/%d)", group2_count, total_count);

		if (group3_title)
			_r_str_printf (group3_string, RTL_NUMBER_OF (group3_string), L" (%d/%d)", group3_count, total_count);

		if (group4_title)
			_r_str_printf (group4_string, RTL_NUMBER_OF (group4_string), L" (%d/%d) [silent]", group4_count, total_count);
	}

	_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (group1_title), group1_string));
	_r_listview_setgroup (hwnd, listview_id, 0, localized_string->buffer, 0, 0);

	_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (group2_title), group2_string));
	_r_listview_setgroup (hwnd, listview_id, 1, localized_string->buffer, 0, 0);

	if (group3_title)
	{
		_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (group3_title), group3_string));
		_r_listview_setgroup (hwnd, listview_id, 2, localized_string->buffer, 0, 0);
	}

	if (group4_title)
	{
		_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (group4_title), group4_string));
		_r_listview_setgroup (hwnd, listview_id, 3, localized_string->buffer, 0, 0);
	}

	if (localized_string)
		_r_obj_dereference (localized_string);
}

VOID _app_refreshstatus (_In_ HWND hwnd)
{
	ITEM_STATUS status;
	HWND hstatus;
	HDC hdc;
	LONG dpi_value;

	hstatus = GetDlgItem (hwnd, IDC_STATUSBAR);

	if (!hstatus)
		return;

	hdc = GetDC (hstatus);

	if (!hdc)
		return;

	_app_getcount (&status);

	dpi_value = _r_dc_getwindowdpi (hwnd);

	_r_dc_fixwindowfont (hdc, hstatus); // fix

	PR_STRING text[UI_STATUSBAR_PARTS_COUNT] = {0};
	INT parts[UI_STATUSBAR_PARTS_COUNT] = {0};
	LONG size[UI_STATUSBAR_PARTS_COUNT] = {0};
	LONG calculated_width = 0;
	LONG spacing = _r_dc_getdpi (16, dpi_value);

	for (INT i = 0; i < UI_STATUSBAR_PARTS_COUNT; i++)
	{
		switch (i)
		{
			case 1:
			{
				text[i] = _r_format_string (L"%s: %" TEXT (PR_SIZE_T), _r_locale_getstring (IDS_STATUS_UNUSED_APPS), status.apps_unused_count);
				break;
			}

			case 2:
			{
				text[i] = _r_format_string (L"%s: %" TEXT (PR_SIZE_T), _r_locale_getstring (IDS_STATUS_TIMER_APPS), status.apps_timer_count);
				break;
			}
		}

		if (i)
		{
			if (text[i])
			{
				size[i] = _r_dc_getfontwidth (hdc, &text[i]->sr) + spacing;
				calculated_width += size[i];
			}
		}
	}

	parts[0] = _r_ctrl_getwidth (hstatus, 0) - calculated_width - _r_dc_getsystemmetrics (SM_CXVSCROLL, dpi_value) - (_r_dc_getsystemmetrics (SM_CXBORDER, dpi_value) * 4);
	parts[1] = parts[0] + size[1];
	parts[2] = parts[1] + size[2];

	_r_status_setparts (hwnd, IDC_STATUSBAR, UI_STATUSBAR_PARTS_COUNT, parts);

	for (INT i = 1; i < UI_STATUSBAR_PARTS_COUNT; i++)
	{
		if (text[i])
		{
			_r_status_settext (hwnd, IDC_STATUSBAR, i, text[i]->buffer);

			_r_obj_dereference (text[i]);
		}
	}

	ReleaseDC (hstatus, hdc);
}

_Success_ (return != -1)
INT _app_getposition (_In_ HWND hwnd, _In_ INT listview_id, _In_ ULONG_PTR lparam)
{
	PITEM_LISTVIEW_CONTEXT context;
	INT item_count;
	INT item_id;

	if ((listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_LOG) || listview_id == IDC_RULE_APPS_ID || listview_id == IDC_APP_RULES_ID)
	{
		item_id = -1;

		item_count = _r_listview_getitemcount (hwnd, listview_id);

		if (item_count)
		{
			for (INT i = 0; i < item_count; i++)
			{
				context = (PITEM_LISTVIEW_CONTEXT)_r_listview_getitemlparam (hwnd, listview_id, i);

				if (!context)
					continue;

				if (context->id_code == lparam)
				{
					item_id = i;
					break;
				}
			}
		}
	}
	else
	{
		item_id = _r_listview_finditem (hwnd, listview_id, -1, lparam);
	}

	return item_id;
}

VOID _app_showitem (_In_ HWND hwnd, _In_ INT listview_id, _In_ INT item_id, _In_ INT scroll_pos)
{
	INT total_count;

	_app_settab_id (hwnd, listview_id);

	total_count = _r_listview_getitemcount (hwnd, listview_id);

	if (!total_count)
		return;

	if (item_id != -1)
	{
		item_id = _r_calc_clamp (item_id, 0, total_count - 1);

		_r_listview_setitemvisible (hwnd, listview_id, item_id);
	}

	if (scroll_pos > 0)
	{
		SendDlgItemMessage (hwnd, listview_id, LVM_SCROLL, 0, (LPARAM)scroll_pos); // restore scroll position
	}
}

