// simplewall
// Copyright (c) 2016-2026 Henry++

#include "global.h"

_Ret_maybenull_
PITEM_TAB_CONTEXT _app_listview_getcontext (
	_In_ HWND hwnd,
	_In_ INT tab_id
)
{
	if (tab_id == INT_ERROR)
	{
		tab_id = _r_tab_getcurrentitem (hwnd, IDC_TAB);

		if (tab_id == INT_ERROR)
			tab_id = 0;
	}

	return (PITEM_TAB_CONTEXT)_r_tab_getitemlparam (hwnd, IDC_TAB, tab_id);
}

_Success_ (return != 0)
INT _app_listview_getbytype (
	_In_ ENUM_TYPE_DATA type
)
{
	switch (type)
	{
		case DATA_APP_REGULAR:
		case DATA_APP_DEVICE:
		case DATA_APP_NETWORK:
		case DATA_APP_PICO:
		{
			return IDC_APPS_PROFILE;
		}

		case DATA_APP_SERVICE:
		{
			return IDC_APPS_SERVICE;
		}

		case DATA_APP_UWP:
		{
			return IDC_APPS_UWP;
		}

		case DATA_RULE_BLOCKLIST:
		{
			return IDC_RULES_BLOCKLIST;
		}

		case DATA_RULE_SYSTEM:
		{
			return IDC_RULES_SYSTEM;
		}

		case DATA_RULE_SYSTEM_USER:
		case DATA_RULE_USER:
		{
			return IDC_RULES_CUSTOM;
		}

		default:
		{
			return 0;
		}
	}
}

VOID _app_listview_additems (
	_In_ HWND hwnd
)
{
	PITEM_APP ptr_app = NULL;
	PITEM_RULE ptr_rule;
	ULONG_PTR enum_key = 0;

	// add apps
	_r_queuedlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtablepointer (apps_table, (PVOID_PTR)&ptr_app, NULL, &enum_key))
	{
		_app_listview_addappitem (hwnd, ptr_app);

		// install timer
		if (ptr_app->timer > 0)
			_app_timer_set (hwnd, ptr_app, ptr_app->timer - _r_unixtime_now ());
	}

	_r_queuedlock_releaseshared (&lock_apps);

	// add rules
	_r_queuedlock_acquireshared (&lock_rules);

	for (ULONG_PTR i = 0; i < _r_obj_getlistsize (rules_list); i++)
	{
		ptr_rule = (PITEM_RULE)_r_obj_getlistitem (rules_list, i);

		if (ptr_rule)
			_app_listview_addruleitem (hwnd, ptr_rule, i, FALSE);
	}

	_r_queuedlock_releaseshared (&lock_rules);
}

VOID _app_listview_addappitem (
	_In_ HWND hwnd,
	_In_ PITEM_APP ptr_app
)
{
	INT item_id, listview_id;

	listview_id = _app_listview_getbytype (ptr_app->type);

	if (!listview_id)
		return;

	item_id = _r_listview_getitemcount (hwnd, listview_id);

	_app_listview_lock (hwnd, listview_id);

	_r_listview_additem (hwnd, listview_id, item_id, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK, I_GROUPIDCALLBACK, _app_listview_createcontext (ptr_app->app_hash));
	_app_setappiteminfo (hwnd, listview_id, item_id, ptr_app);

	_app_listview_unlock (hwnd, listview_id);
}

VOID _app_listview_addruleitem (
	_In_ HWND hwnd,
	_In_ PITEM_RULE ptr_rule,
	_In_ ULONG_PTR rule_idx,
	_In_ BOOLEAN is_forapp
)
{
	INT item_id, listview_id;

	listview_id = _app_listview_getbytype (ptr_rule->type);

	if (!listview_id)
		return;

	item_id = _r_listview_getitemcount (hwnd, listview_id);

	_app_listview_lock (hwnd, listview_id);

	_r_listview_additem (hwnd, listview_id, item_id, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK, I_GROUPIDCALLBACK, _app_listview_createcontext (rule_idx));
	_app_setruleiteminfo (hwnd, listview_id, item_id, ptr_rule, is_forapp);

	_app_listview_unlock (hwnd, listview_id);
}

VOID _app_listview_addnetworkitem (
	_In_ HWND hwnd,
	_In_ ULONG network_hash
)
{
	_r_listview_additem (hwnd, IDC_NETWORK, INT_ERROR, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK, I_GROUPIDCALLBACK, _app_listview_createcontext (network_hash));
}

VOID _app_listview_addlogitem (
	_In_ HWND hwnd,
	_In_ PITEM_LOG ptr_log,
	_In_ ULONG log_hash
)
{
	// increment value
	_InterlockedIncrement (&config.log_id);

	ptr_log->log_id = _InterlockedCompareExchange (&config.log_id, 0, 0);

	_r_listview_additem (hwnd, IDC_LOG, INT_ERROR, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK, 0, _app_listview_createcontext (log_hash));
}

VOID _app_listview_clearitems (
	_In_ HWND hwnd
)
{
	for (INT i = IDC_APPS_PROFILE; i <= IDC_RULES_CUSTOM; i++)
	{
		_r_listview_deleteallitems (hwnd, i);
	}
}

BOOLEAN _app_listview_islocked (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	return (_r_wnd_getcontext (hwnd, (USHORT_MAX - ctrl_id)) != NULL);
}

BOOLEAN _app_listview_lock (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	if (_app_listview_islocked (hwnd, ctrl_id)) // check first!
		return TRUE;

	return _r_wnd_setcontext (hwnd, (USHORT_MAX - ctrl_id), INVALID_HANDLE_VALUE);
}

BOOLEAN _app_listview_unlock (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	return _r_wnd_removecontext (hwnd, (USHORT_MAX - ctrl_id));
}

LONG_PTR _app_listview_createcontext (
	_In_ ULONG_PTR id_code
)
{
	PITEM_LISTVIEW_CONTEXT context;

	context = (PITEM_LISTVIEW_CONTEXT)_r_freelist_allocateitem (&listview_free_list);

	context->id_code = id_code;

	return (LONG_PTR)context;
}

VOID _app_listview_destroycontext (
	_In_ PITEM_LISTVIEW_CONTEXT context
)
{
	_r_freelist_deleteitem (&listview_free_list, context);
}

ULONG_PTR _app_listview_getcontextcode (
	_In_ LONG_PTR lparam
)
{
	PITEM_LISTVIEW_CONTEXT context = (PITEM_LISTVIEW_CONTEXT)lparam;

	return context->id_code;
}

_Success_ (return != 0)
ULONG _app_listview_getappcontext (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ INT item_id
)
{
	PITEM_NETWORK ptr_network;
	PITEM_LOG ptr_log;
	LPARAM lparam;
	ULONG app_hash = 0, context;

	lparam = _r_listview_getitemlparam (hwnd, listview_id, item_id);

	if (lparam == 0)
		return 0;

	context = (ULONG)_app_listview_getcontextcode (lparam);

	switch (listview_id)
	{
		case IDC_NETWORK:
		{
			ptr_network = _app_network_getitem (context);

			if (ptr_network)
			{
				app_hash = ptr_network->app_hash;

				_r_obj_dereference (ptr_network);
			}

			break;
		}

		case IDC_LOG:
		{
			ptr_log = _app_getlogitem (context);

			if (ptr_log)
			{
				app_hash = ptr_log->app_hash;

				_r_obj_dereference (ptr_log);
			}

			break;
		}

		default:
		{
			app_hash = context;
			break;
		}
	}

	return app_hash;
}

_Success_ (return != 0)
ULONG_PTR _app_listview_getitemcontext (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ INT item_id
)
{
	LONG_PTR lparam;

	lparam = _r_listview_getitemlparam (hwnd, listview_id, item_id);

	return (!lparam) ? 0 : _app_listview_getcontextcode (lparam);
}

BOOLEAN _app_listview_isitemhidden (
	_In_ LPARAM lparam
)
{
	PITEM_LISTVIEW_CONTEXT context = (PITEM_LISTVIEW_CONTEXT)lparam;

	if (!context)
		return FALSE;

	return !!context->is_hidden;
}

_Success_ (return != INT_ERROR)
INT _app_listview_finditem (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ ULONG_PTR id_code
)
{
	ULONG_PTR current_code;

	if ((listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_LOG) || listview_id == IDC_RULE_APPS_ID || listview_id == IDC_APP_RULES_ID)
	{
		for (INT i = 0; i < _r_listview_getitemcount (hwnd, listview_id); i++)
		{
			current_code = _app_listview_getitemcontext (hwnd, listview_id, i);

			if (current_code == id_code)
				return i;
		}
	}
	else
	{
		return _r_listview_finditem (hwnd, listview_id, INT_ERROR, id_code);
	}

	return INT_ERROR;
}

VOID _app_listview_showitemby_id (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ INT item_id,
	_In_opt_ LONG scroll_pos
)
{
	INT total_count;

	_app_settab_id (hwnd, listview_id);

	total_count = _r_listview_getitemcount (hwnd, listview_id);

	if (!total_count)
		return;

	if (item_id != INT_ERROR)
	{
		item_id = _r_calc_clamp (item_id, 0, total_count - 1);

		_r_listview_setitemvisible (hwnd, listview_id, item_id);
	}

	// restore scroll position
	if (scroll_pos > 0)
		_r_listview_scroll (hwnd, listview_id, scroll_pos);
}

VOID _app_listview_showitemby_param (
	_In_ HWND hwnd,
	_In_ ULONG_PTR lparam,
	_In_ BOOLEAN is_app
)
{
	PITEM_TAB_CONTEXT tab_context;
	INT item_id, listview_id = 0;

	if (is_app)
	{
		if (!_app_getappinfobyhash ((ULONG)lparam, INFO_LISTVIEW_ID, &listview_id, sizeof (INT)))
			return;
	}
	else
	{
		if (!_app_getruleinfobyid (lparam, INFO_LISTVIEW_ID, &listview_id, sizeof (INT)))
			return;
	}

	tab_context = _app_listview_getcontext (hwnd, INT_ERROR);

	if (tab_context && listview_id != tab_context->listview_id)
	{
		_app_listview_sort (hwnd, listview_id, INT_ERROR, FALSE);
		_app_listview_resize (hwnd, listview_id, FALSE);
	}

	item_id = _app_listview_finditem (hwnd, listview_id, lparam);

	if (item_id != INT_ERROR)
	{
		_app_listview_showitemby_id (hwnd, listview_id, item_id, INT_ERROR);

		_r_wnd_toggle (hwnd, TRUE);
	}
}

VOID _app_listview_updateby_id (
	_In_ HWND hwnd,
	_In_ INT lparam,
	_In_ ULONG flags
)
{
	PITEM_TAB_CONTEXT tab_context;
	ENUM_TYPE_DATA type;
	INT listview_id;

	tab_context = _app_listview_getcontext (hwnd, INT_ERROR);

	if (!tab_context)
		return;

	if (flags & PR_UPDATE_TYPE)
	{
		type = (ENUM_TYPE_DATA)lparam;

		listview_id = (type == DATA_LISTVIEW_CURRENT) ? tab_context->listview_id : _app_listview_getbytype (type);

		if (listview_id == 0)
			return;
	}
	else
	{
		listview_id = lparam;
	}

	if ((flags & PR_UPDATE_FORCE) || (listview_id == tab_context->listview_id))
	{
		if (!(flags & PR_UPDATE_NOREDRAW))
			_r_listview_redraw (hwnd, listview_id);

		if (!(flags & PR_UPDATE_NOSETVIEW))
		{
			_app_listview_setfont (hwnd, listview_id);
			_app_listview_setview (hwnd, listview_id);
		}

		if (!(flags & PR_UPDATE_NOREFRESH))
			_app_listview_refreshgroups (hwnd, listview_id);

		if (!(flags & PR_UPDATE_NOSORT))
			_app_listview_sort (hwnd, listview_id, INT_ERROR, FALSE);

		if (!(flags & PR_UPDATE_NORESIZE))
			_app_listview_resize (hwnd, listview_id, FALSE);
	}

	_app_refreshstatus (hwnd);
}

VOID _app_listview_updateby_param (
	_In_ HWND hwnd,
	_In_ ULONG_PTR lparam,
	_In_ ULONG flags,
	_In_ BOOLEAN is_app
)
{
	PITEM_TAB_CONTEXT tab_context;
	INT listview_id = 0;

	if (is_app)
	{
		if (!_app_getappinfobyhash ((ULONG)lparam, INFO_LISTVIEW_ID, &listview_id, sizeof (INT)))
			return;
	}
	else
	{
		if (!_app_getruleinfobyid (lparam, INFO_LISTVIEW_ID, &listview_id, sizeof (INT)))
			return;
	}

	if ((flags & PR_SETITEM_UPDATE))
		_app_listview_updateby_id (hwnd, listview_id, 0);

	if ((flags & PR_SETITEM_REDRAW))
	{
		tab_context = _app_listview_getcontext (hwnd, INT_ERROR);

		if (tab_context)
		{
			if (listview_id == tab_context->listview_id)
				_r_listview_redraw (hwnd, listview_id);
		}
	}
}

VOID _app_listview_updateitemby_param (
	_In_ HWND hwnd,
	_In_ ULONG_PTR lparam,
	_In_ BOOLEAN is_app
)
{
	PITEM_RULE ptr_rule;
	PITEM_APP ptr_app;
	INT item_id, listview_id = 0;

	if (is_app)
	{
		if (!_app_getappinfobyhash ((ULONG)lparam, INFO_LISTVIEW_ID, &listview_id, sizeof (INT)))
			return;
	}
	else
	{
		if (!_app_getruleinfobyid (lparam, INFO_LISTVIEW_ID, &listview_id, sizeof (INT)))
			return;
	}

	item_id = _app_listview_finditem (hwnd, listview_id, lparam);

	if (item_id == INT_ERROR)
		return;

	if (is_app)
	{
		ptr_app = _app_getappitem ((ULONG)lparam);

		if (!ptr_app)
			return;

		_app_listview_lock (hwnd, listview_id);
		_app_setappiteminfo (hwnd, listview_id, item_id, ptr_app);
		_app_listview_unlock (hwnd, listview_id);

		_r_obj_dereference (ptr_app);
	}
	else
	{
		ptr_rule = _app_getrulebyid (lparam);

		if (!ptr_rule)
			return;

		_app_listview_lock (hwnd, listview_id);
		_app_setruleiteminfo (hwnd, listview_id, item_id, ptr_rule, FALSE);
		_app_listview_unlock (hwnd, listview_id);

		_r_obj_dereference (ptr_rule);
	}
}

VOID _app_listview_updateitemby_id (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ INT item_id
)
{
	_r_listview_setitem (hwnd, listview_id, item_id, 0, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK, I_GROUPIDCALLBACK, I_DEFAULT);
}

VOID _app_listview_loadfont (
	_In_ LONG dpi_value,
	_In_ BOOLEAN is_forced
)
{
	LOGFONT logfont = {0};

	if (is_forced || !config.hfont)
	{
		SAFE_DELETE_OBJECT (config.hfont);

		_r_config_getfont (L"Font", &logfont, dpi_value, NULL);

		config.hfont = _app_createfont (&logfont, 0, FALSE, 0);
	}
}

VOID _app_listview_refreshgroups (
	_In_ HWND hwnd,
	_In_ INT listview_id
)
{
	WCHAR buffer1[0x80], buffer2[0x80];
	ULONG group1_title = 0, group2_title = 0, group3_title = 0, group4_title = 0, group5_title = 0;
	INT group1_count = 0, group2_count = 0, group3_count = 0, group4_count = 0, group5_count = 0, total_count, group_id;
	BOOLEAN is_rules;

	if (!_r_listview_isgroupviewenabled (hwnd, listview_id))
		return;

	is_rules = (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM);

	if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
	{
		group1_title = IDS_GROUP_ALLOWED;
		group2_title = IDS_GROUP_TIMER;
		group3_title = IDS_GROUP_SPECIAL_APPS;
		group4_title = IDS_GROUP_BLOCKED;
		group5_title = IDS_GROUP_BLOCKED;
	}
	else if (is_rules)
	{
		group1_title = IDS_GROUP_ENABLED;
		group2_title = IDS_GROUP_ENABLED;
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
		return; // unknown listview!
	}

	total_count = _r_listview_getitemcount (hwnd, listview_id);

	for (INT i = 0; i < total_count; i++)
	{
		if (listview_id == IDC_RULE_APPS_ID || listview_id == IDC_APP_RULES_ID)
		{
			if (_r_listview_isitemchecked (hwnd, listview_id, i))
				group1_count = group2_count = group3_count += 1;
		}
		else
		{
			group_id = _r_listview_getitemgroup (hwnd, listview_id, i);

			if (group_id == 4)
			{
				group5_count += 1;
			}
			else if (group_id == 3)
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
			else if (group_id == 0)
			{
				group1_count += 1;
			}
		}
	}

	if (total_count)
	{
		// set group 1 and 2 titles
		if (is_rules)
		{
			_r_str_printf (buffer1, RTL_NUMBER_OF (buffer1), L"%s (%d/%d) [%s]", _r_locale_getstring (group1_title), group1_count, total_count, _r_locale_getstring (IDS_RULE_FOR_ALL));
			_r_str_printf (buffer2, RTL_NUMBER_OF (buffer2), L"%s (%d/%d) [%s]", _r_locale_getstring (group2_title), group2_count, total_count, _r_locale_getstring (IDS_RULE_FOR_ALL));
		}
		else
		{
			_r_str_printf (buffer1, RTL_NUMBER_OF (buffer1), L"%s (%d/%d)", _r_locale_getstring (group1_title), group1_count, total_count);
			_r_str_printf (buffer2, RTL_NUMBER_OF (buffer2), L"%s (%d/%d)", _r_locale_getstring (group2_title), group2_count, total_count);
		}

		_r_listview_setgroup (hwnd, listview_id, 0, buffer1, 0, 0);
		_r_listview_setgroup (hwnd, listview_id, 1, buffer2, 0, 0);

		// set group 3 title
		if (group3_title)
		{
			_r_str_printf (buffer1, RTL_NUMBER_OF (buffer1), L"%s (%d/%d)", _r_locale_getstring (group3_title), group3_count, total_count);

			_r_listview_setgroup (hwnd, listview_id, 2, buffer1, 0, 0);
		}

		// set group 4 title
		if (group4_title)
		{
			_r_str_printf (buffer1, RTL_NUMBER_OF (buffer1), L"%s (%d/%d)", _r_locale_getstring (group4_title), group4_count, total_count);

			_r_listview_setgroup (hwnd, listview_id, 3, buffer1, 0, 0);
		}

		// set group 5 title
		if (group5_title)
		{
			_r_str_printf (buffer1, RTL_NUMBER_OF (buffer1), L"%s (%d/%d) [silent]", _r_locale_getstring (group5_title), group5_count, total_count);

			_r_listview_setgroup (hwnd, listview_id, 4, buffer1, 0, 0);
		}
	}
}

VOID _app_listview_resize (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ BOOLEAN is_forced
)
{
	HDC hdc_listview = NULL, hdc_header = NULL;
	HWND header = NULL, hlistview;
	PR_STRING string;
	LONG column_general_id = 0; // set general column id
	LONG calculated_width = 0, column_count, column_width, dpi_value, item_count, spacing, text_width, total_width, max_width;
	BOOLEAN is_tableview;

	if (!is_forced && !_r_config_getboolean (L"AutoSizeColumns", TRUE, NULL))
		return;

	hlistview = GetDlgItem (hwnd, listview_id);

	if (!hlistview)
		return;

	column_count = _r_listview_getcolumncount (hwnd, listview_id);

	if (!column_count)
		return;

	// get device context
	hdc_listview = GetDC (hlistview);

	if (!hdc_listview)
		goto CleanupExit;

	header = _r_listview_getheader (hwnd, listview_id);

	if (!header)
		goto CleanupExit;

	hdc_header = GetDC (header);

	if (!hdc_header)
		goto CleanupExit;

	_r_dc_fixfont (hdc_listview, hwnd, listview_id); // fix font!
	_r_dc_fixfont (hdc_header, header, 0); // fix font!

	is_tableview = (_r_listview_getview (hwnd, listview_id) == LV_VIEW_DETAILS);

	dpi_value = _r_dc_getwindowdpi (hwnd);

	max_width = _r_dc_getdpi (158, dpi_value);
	spacing = _r_dc_getsystemmetrics (SM_CXSMICON, dpi_value);

	total_width = _r_ctrl_getwidth (hwnd, listview_id);
	item_count = _r_listview_getitemcount (hwnd, listview_id);

	for (LONG i = 0; i < column_count; i++)
	{
		if (i == column_general_id)
			continue;

		// get column text width
		string = _r_listview_getcolumntext (hwnd, listview_id, i);

		if (!string)
			continue;

		column_width = _r_dc_getfontwidth (hdc_header, &string->sr, NULL) + spacing;

		_r_obj_dereference (string);

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
					// check for number column
					if ((i == 0) && (listview_id == IDC_LOG))
					{
						text_width = _r_dc_getdpi (50, dpi_value);
					}
					else
					{
						string = _r_listview_getitemtext (hwnd, listview_id, j, i);

						if (!string)
							continue;

						text_width = _r_dc_getfontwidth (hdc_listview, &string->sr, NULL) + spacing;

						_r_obj_dereference (string);
					}

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

		_r_listview_setcolumn (hwnd, listview_id, i, NULL, column_width);

		calculated_width += column_width;
	}

	// set general column width
	_r_listview_setcolumn (hwnd, listview_id, column_general_id, NULL, max (total_width - calculated_width, max_width));

CleanupExit:

	if (hdc_listview)
		ReleaseDC (hlistview, hdc_listview);

	if (hdc_header)
		ReleaseDC (header, hdc_header);
}

VOID _app_listview_setfont (
	_In_ HWND hwnd,
	_In_ INT listview_id
)
{
	if (config.hfont)
		_r_ctrl_setfont (hwnd, listview_id, config.hfont);
}

VOID _app_listview_setview (
	_In_ HWND hwnd,
	_In_ INT listview_id
)
{
	HIMAGELIST himg = NULL;
	LONG icons_size, view_type;
	BOOLEAN is_mainview;

	is_mainview = (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_RULES_CUSTOM);

	view_type = is_mainview ? _r_calc_clamp (_r_config_getlong (L"ViewType", LV_VIEW_DETAILS, NULL), LV_VIEW_ICON, LV_VIEW_MAX) : LV_VIEW_DETAILS;
	icons_size = is_mainview ? _r_calc_clamp (_r_config_getlong (L"IconSize", SHIL_SMALL, NULL), SHIL_LARGE, SHIL_LAST) : SHIL_SMALL;

	if ((listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM) || listview_id == IDC_APP_RULES_ID)
	{
		himg = (icons_size == SHIL_SMALL || icons_size == SHIL_SYSSMALL) ? config.himg_rules_small : config.himg_rules_large;
	}
	else
	{
		_r_imagelist_getsystem (&himg, icons_size);
	}

	if (himg)
		_r_listview_setimagelist (hwnd, listview_id, himg);

	_r_listview_setview (hwnd, listview_id, view_type);
}

INT CALLBACK _app_listview_compare_callback (
	_In_ LPARAM lparam1,
	_In_ LPARAM lparam2,
	_In_ LPARAM lparam
)
{
	PR_STRING item_text_1, item_text_2;
	PITEM_LOG ptr_log1, ptr_log2;
	PITEM_NETWORK ptr_network1, ptr_network2;
	WCHAR section_name[0x80];
	HWND hwnd;
	LONG64 timestamp1 = 0, timestamp2 = 0;
	LONG64 value1 = 0, value2 = 0;
	ULONG context1, context2;
	LONG column_id;
	INT item_id1, item_id2, listview_id, result = 0;
	BOOLEAN is_checked1, is_checked2, is_descend;

	hwnd = GetParent ((HWND)lparam);

	if (!hwnd)
		return 0;

	listview_id = GetDlgCtrlID ((HWND)lparam);

	if (!listview_id)
		return 0;

	item_id1 = (INT)(INT_PTR)lparam1;
	item_id2 = (INT)(INT_PTR)lparam2;

	_r_str_printf (section_name, RTL_NUMBER_OF (section_name), L"listview\\%04" TEXT (PRIX32), listview_id);

	is_descend = _r_config_getboolean (L"SortIsDescending", FALSE, section_name);
	column_id = _r_config_getlong (L"SortColumn", 0, section_name);

	if ((_r_listview_getstyle_ex (hwnd, listview_id) & LVS_EX_CHECKBOXES) != 0)
	{
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

	context1 = (ULONG)_app_listview_getitemcontext (hwnd, listview_id, item_id1);
	context2 = (ULONG)_app_listview_getitemcontext (hwnd, listview_id, item_id2);

	if (!result)
	{
		// timestamp sorting
		if ((listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP) && column_id == 1)
		{
			if (_app_getappinfobyhash (context1, INFO_TIMESTAMP, &timestamp1, sizeof (LONG64)) &&
				_app_getappinfobyhash (context2, INFO_TIMESTAMP, &timestamp2, sizeof (LONG64)))
			{
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
		else if (listview_id == IDC_LOG && column_id == 1)
		{
			ptr_log1 = _app_getlogitem (context1);
			ptr_log2 = _app_getlogitem (context2);

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
		else if (listview_id == IDC_NETWORK && column_id >= 9 && column_id <= 11)
		{
			ptr_network1 = _app_network_getitem (context1);
			ptr_network2 = _app_network_getitem (context2);

			if (ptr_network1 && ptr_network2)
			{
				if (column_id == 9)
				{
					value1 = _InterlockedCompareExchange64 (&ptr_network1->download_speed, 0, 0);
					value2 = _InterlockedCompareExchange64 (&ptr_network2->download_speed, 0, 0);
				}
				else if (column_id == 10)
				{
					value1 = _InterlockedCompareExchange64 (&ptr_network1->upload_speed, 0, 0);
					value2 = _InterlockedCompareExchange64 (&ptr_network2->upload_speed, 0, 0);
				}
				else
				{
					value1 = _InterlockedCompareExchange64 (&ptr_network1->download_total, 0, 0) + _InterlockedCompareExchange64 (&ptr_network1->upload_total, 0, 0);
					value2 = _InterlockedCompareExchange64 (&ptr_network2->download_total, 0, 0) + _InterlockedCompareExchange64 (&ptr_network2->upload_total, 0, 0);
				}

				if (value1 < value2)
					result = -1;
				else if (value1 > value2)
					result = 1;
			}

			if (ptr_network1)
				_r_obj_dereference (ptr_network1);

			if (ptr_network2)
				_r_obj_dereference (ptr_network2);
		}
	}

	if (!result)
	{
		item_text_1 = _r_listview_getitemtext (hwnd, listview_id, item_id1, column_id);
		item_text_2 = _r_listview_getitemtext (hwnd, listview_id, item_id2, column_id);

		if (item_text_1 && item_text_2)
			result = _r_str_compare_logical (item_text_1->buffer, item_text_2->buffer);

		if (item_text_1)
			_r_obj_dereference (item_text_1);

		if (item_text_2)
			_r_obj_dereference (item_text_2);
	}

	return is_descend ? -result : result;
}

VOID _app_listview_sort (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ LONG column_id,
	_In_ BOOLEAN is_notifycode
)
{
	WCHAR config_name[0x80];
	HWND hlistview;
	INT column_count;
	BOOLEAN is_descend;

	hlistview = GetDlgItem (hwnd, listview_id);

	// there is no sort header, so don't sort!
	if ((!hlistview || _r_wnd_getstyle (hlistview, GWL_STYLE) & (LVS_NOSORTHEADER | LVS_OWNERDATA)) != 0)
		return;

	column_count = _r_listview_getcolumncount (hwnd, listview_id);

	// there is no columns, so don't sort!
	if (!column_count)
		return;

	_r_str_printf (config_name, RTL_NUMBER_OF (config_name), L"listview\\%04" TEXT (PRIX32), listview_id);

	is_descend = _r_config_getboolean (L"SortIsDescending", FALSE, config_name);

	if (is_notifycode)
		is_descend = !is_descend;

	if (column_id == INT_ERROR)
		column_id = _r_config_getlong (L"SortColumn", 0, config_name);

	column_id = _r_calc_clamp (column_id, 0, column_count - 1); // set range

	if (is_notifycode)
	{
		_r_config_setboolean (L"SortIsDescending", is_descend, config_name);
		_r_config_setlong (L"SortColumn", column_id, config_name);
	}

	for (INT i = 0; i < column_count; i++)
	{
		_r_listview_setcolumnsortindex (hwnd, listview_id, i, 0);
	}

	_r_listview_setcolumnsortindex (hwnd, listview_id, column_id, is_descend ? -1 : 1);

	_r_listview_sort (hwnd, listview_id, &_app_listview_compare_callback, (WPARAM)hlistview);
}
