// simplewall
// Copyright (c) 2016-2021 Henry++

#include "global.h"

INT _app_listview_getcurrent (_In_ HWND hwnd)
{
	INT listview_id;

	listview_id = _app_listview_getbytab (hwnd, -1);

	return listview_id;
}

INT _app_listview_getbytab (_In_ HWND hwnd, _In_ INT tab_id)
{
	INT listview_id;

	if (tab_id == -1)
		tab_id = _r_tab_getcurrentitem (hwnd, IDC_TAB);

	listview_id = (INT)_r_tab_getitemlparam (hwnd, IDC_TAB, tab_id);

	return listview_id;
}

INT _app_listview_getbytype (_In_ ENUM_TYPE_DATA type)
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
	}

	return 0;
}

VOID _app_listview_additems (_In_ HWND hwnd)
{
	PITEM_APP ptr_app;
	PITEM_RULE ptr_rule;
	LONG64 current_time;
	SIZE_T enum_key;

	current_time = _r_unixtime_now ();

	enum_key = 0;

	// add apps
	_r_queuedlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtablepointer (apps_table, &ptr_app, NULL, &enum_key))
	{
		_app_listview_addappitem (hwnd, ptr_app);

		// install timer
		if (ptr_app->timer)
			_app_timer_set (hwnd, ptr_app, ptr_app->timer - current_time);
	}

	_r_queuedlock_releaseshared (&lock_apps);

	// add rules
	_r_queuedlock_acquireshared (&lock_rules);

	for (SIZE_T i = 0; i < _r_obj_getlistsize (rules_list); i++)
	{
		ptr_rule = _r_obj_getlistitem (rules_list, i);

		if (ptr_rule)
			_app_listview_addruleitem (hwnd, ptr_rule, i, FALSE);
	}

	_r_queuedlock_releaseshared (&lock_rules);
}

VOID _app_listview_clearitems (_In_ HWND hwnd)
{
	INT listview_id;

	listview_id = _app_listview_getcurrent (hwnd);

	if (!listview_id)
		return;

	for (INT i = IDC_APPS_PROFILE; i <= IDC_RULES_CUSTOM; i++)
		_r_listview_deleteallitems (hwnd, i);
}

VOID _app_listview_addappitem (_In_ HWND hwnd, _In_ PITEM_APP ptr_app)
{
	LPARAM listview_context;
	INT listview_id;
	INT item_id;

	listview_id = _app_listview_getbytype (ptr_app->type);

	if (listview_id)
	{
		item_id = _r_listview_getitemcount (hwnd, listview_id);

		_app_listview_lock (hwnd, listview_id, TRUE);

		listview_context = _app_listview_createcontext (ptr_app->app_hash);

		_r_listview_additem_ex (hwnd, listview_id, item_id, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK, I_GROUPIDCALLBACK, listview_context);
		_app_setappiteminfo (hwnd, listview_id, item_id, ptr_app);

		_app_listview_lock (hwnd, listview_id, FALSE);
	}
}

VOID _app_listview_addruleitem (_In_ HWND hwnd, _In_ PITEM_RULE ptr_rule, _In_ SIZE_T rule_idx, _In_ BOOLEAN is_forapp)
{
	LPARAM listview_context;
	INT listview_id;
	INT item_id;

	listview_id = _app_listview_getbytype (ptr_rule->type);

	if (listview_id)
	{
		item_id = _r_listview_getitemcount (hwnd, listview_id);

		_app_listview_lock (hwnd, listview_id, TRUE);

		listview_context = _app_listview_createcontext (rule_idx);

		_r_listview_additem_ex (hwnd, listview_id, item_id, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK, I_GROUPIDCALLBACK, listview_context);
		_app_setruleiteminfo (hwnd, listview_id, item_id, ptr_rule, is_forapp);

		_app_listview_lock (hwnd, listview_id, FALSE);
	}
}

VOID _app_listview_addnetworkitem (_In_ HWND hwnd, _In_ PITEM_NETWORK ptr_network, _In_ ULONG_PTR network_hash)
{
	LPARAM listview_context;
	INT item_id;

	UNREFERENCED_PARAMETER (ptr_network);

	item_id = _r_listview_getitemcount (hwnd, IDC_NETWORK);

	listview_context = _app_listview_createcontext (network_hash);

	_r_listview_additem_ex (hwnd, IDC_NETWORK, item_id, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK, I_GROUPIDCALLBACK, listview_context);
}

VOID _app_listview_addlogitem (_In_ HWND hwnd, _In_ PITEM_LOG ptr_log, _In_ ULONG_PTR log_hash)
{
	LPARAM listview_context;
	INT item_id;

	UNREFERENCED_PARAMETER (ptr_log);

	item_id = _r_listview_getitemcount (hwnd, IDC_LOG);

	listview_context = _app_listview_createcontext (log_hash);

	_r_listview_additem_ex (hwnd, IDC_LOG, item_id, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK, 0, listview_context);
}

BOOLEAN _app_listview_islocked (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	PVOID context;
	ULONG property_id;

	property_id = (USHORT_MAX - ctrl_id);

	context = _r_wnd_getcontext (hwnd, property_id);

	return (context != NULL);
}

VOID _app_listview_lock (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ BOOLEAN is_lock)
{
	ULONG property_id;

	property_id = (USHORT_MAX - ctrl_id);

	if (is_lock)
	{
		_r_wnd_setcontext (hwnd, property_id, INVALID_HANDLE_VALUE);
	}
	else
	{
		_r_wnd_removecontext (hwnd, property_id);
	}
}

LPARAM _app_listview_createcontext (_In_ ULONG_PTR id_code)
{
	PITEM_LISTVIEW_CONTEXT context;

	context = _r_freelist_allocateitem (&listview_free_list);

	context->id_code = id_code;

	return (LPARAM)context;
}

VOID _app_listview_destroycontext (_In_ PITEM_LISTVIEW_CONTEXT context)
{
	_r_freelist_deleteitem (&listview_free_list, context);
}

ULONG_PTR _app_listview_getcontextcode (_In_ LPARAM lparam)
{
	PITEM_LISTVIEW_CONTEXT context;

	context = (PITEM_LISTVIEW_CONTEXT)lparam;

	return context->id_code;
}

ULONG_PTR _app_listview_getitemcontext (_In_ HWND hwnd, _In_ INT listview_id, _In_ INT item_id)
{
	LPARAM lparam;

	lparam = _r_listview_getitemlparam (hwnd, listview_id, item_id);

	if (!lparam)
		return 0;

	return _app_listview_getcontextcode (lparam);
}

BOOLEAN _app_listview_isitemhidden (_In_ LPARAM lparam)
{
	PITEM_LISTVIEW_CONTEXT context;

	context = (PITEM_LISTVIEW_CONTEXT)lparam;

	if (!context)
		return FALSE;

	return !!context->is_hidden;
}

_Success_ (return != -1)
INT _app_listview_finditem (_In_ HWND hwnd, _In_ INT listview_id, _In_ ULONG_PTR id_code)
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

				if (context->id_code == id_code)
				{
					item_id = i;
					break;
				}
			}
		}
	}
	else
	{
		item_id = _r_listview_finditem (hwnd, listview_id, -1, id_code);
	}

	return item_id;
}

VOID _app_listview_showitemby_id (_In_ HWND hwnd, _In_ INT listview_id, _In_ INT item_id, _In_ INT scroll_pos)
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
		SendDlgItemMessage (hwnd, listview_id, LVM_SCROLL, 0, (LPARAM)scroll_pos); // restore scroll position
}

VOID _app_listview_showitemby_param (_In_ HWND hwnd, _In_ ULONG_PTR lparam, _In_ BOOLEAN is_app)
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
		if (listview_id != _app_listview_getcurrent (hwnd))
		{
			_app_listview_sort (hwnd, listview_id);
			_app_listview_resize (hwnd, listview_id);
		}

		item_id = _app_listview_finditem (hwnd, listview_id, lparam);

		if (item_id != -1)
		{
			_app_listview_showitemby_id (hwnd, listview_id, item_id, -1);
			_r_wnd_toggle (hwnd, TRUE);
		}
	}
}

VOID _app_listview_updateby_id (_In_ HWND hwnd, _In_ INT lparam, _In_ ULONG flags)
{
	INT listview_id;
	ENUM_TYPE_DATA type;

	if (flags & PR_UPDATE_TYPE)
	{
		type = lparam;

		if (type == DATA_LISTVIEW_CURRENT)
		{
			listview_id = _app_listview_getcurrent (hwnd);
		}
		else
		{
			listview_id = _app_listview_getbytype (type);
		}
	}
	else
	{
		listview_id = lparam;
	}

	if ((flags & PR_UPDATE_FORCE) || (listview_id == _app_listview_getcurrent (hwnd)))
	{
		if (!(flags & PR_UPDATE_NOREDRAW))
			_r_listview_redraw (hwnd, listview_id, -1);

		if (!(flags & PR_UPDATE_NOSETVIEW))
		{
			_app_listview_setfont (hwnd, listview_id);
			_app_listview_setview (hwnd, listview_id);
		}

		if (!(flags & PR_UPDATE_NOREFRESH))
			_app_listview_refreshgroups (hwnd, listview_id);

		if (!(flags & PR_UPDATE_NOSORT))
			_app_listview_sort (hwnd, listview_id);

		if (!(flags & PR_UPDATE_NORESIZE))
			_app_listview_resize (hwnd, listview_id);
	}

	_app_refreshstatus (hwnd);
}

VOID _app_listview_updateby_param (_In_ HWND hwnd, _In_ ULONG_PTR lparam, _In_ ULONG flags, _In_ BOOLEAN is_app)
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
			_app_listview_updateby_id (hwnd, listview_id, 0);

		if ((flags & PR_SETITEM_REDRAW))
		{
			if (listview_id == _app_listview_getcurrent (hwnd))
				_r_listview_redraw (hwnd, listview_id, -1);
		}
	}
}

VOID _app_listview_updateitemby_param (_In_ HWND hwnd, _In_ ULONG_PTR lparam, _In_ BOOLEAN is_app)
{
	PITEM_APP ptr_app;
	PITEM_RULE ptr_rule;
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
		item_id = _app_listview_finditem (hwnd, listview_id, lparam);

		if (item_id != -1)
		{
			if (is_app)
			{
				ptr_app = _app_getappitem (lparam);

				if (ptr_app)
				{
					_app_listview_lock (hwnd, listview_id, TRUE);
					_app_setappiteminfo (hwnd, listview_id, item_id, ptr_app);
					_app_listview_lock (hwnd, listview_id, FALSE);

					_r_obj_dereference (ptr_app);
				}
			}
			else
			{
				ptr_rule = _app_getrulebyid (lparam);

				if (ptr_rule)
				{
					_app_listview_lock (hwnd, listview_id, TRUE);
					_app_setruleiteminfo (hwnd, listview_id, item_id, ptr_rule, FALSE);
					_app_listview_lock (hwnd, listview_id, FALSE);

					_r_obj_dereference (ptr_rule);
				}
			}
		}
	}
}

VOID _app_listview_updateitemby_id (_In_ HWND hwnd, _In_ INT listview_id, _In_ INT item_id)
{
	_r_listview_setitem_ex (hwnd, listview_id, item_id, 0, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK, I_GROUPIDCALLBACK, 0);
}

VOID _app_listview_loadfont (_In_ LONG dpi_value, _In_ BOOLEAN is_forced)
{
	LOGFONT logfont = {0};

	if (is_forced || !config.hfont)
	{
		SAFE_DELETE_OBJECT (config.hfont);

		_r_config_getfont (L"Font", &logfont, dpi_value);

		config.hfont = _app_createfont (&logfont, 0, FALSE, 0);
	}
}

VOID _app_listview_refreshgroups (_In_ HWND hwnd, _In_ INT listview_id)
{
	UINT group1_title;
	UINT group2_title;
	UINT group3_title;
	UINT group4_title;

	INT total_count;

	INT group1_count;
	INT group2_count;
	INT group3_count;
	INT group4_count;

	INT group_id;

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
		group2_title = 0;
		group3_title = IDS_GROUP_DISABLED;
		group4_title = 0;
	}
	else if (listview_id == IDC_RULE_APPS_ID || listview_id == IDC_NETWORK)
	{
		group1_title = IDS_TAB_APPS;
		group2_title = IDS_TAB_SERVICES;
		group3_title = IDS_TAB_PACKAGES;
		group4_title = 0;
	}
	else if (listview_id == IDC_APP_RULES_ID)
	{
		group1_title = IDS_TRAY_SYSTEM_RULES;
		group2_title = IDS_TRAY_USER_RULES;
		group3_title = 0;
		group4_title = 0;
	}
	else
	{
		return;
	}

	group1_count = 0;
	group2_count = 0;
	group3_count = 0;
	group4_count = 0;

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
			else if (group_id == 0)
			{
				group1_count += 1;
			}
		}
	}

	WCHAR group1_string[128] = {0};
	WCHAR group2_string[128] = {0};
	WCHAR group3_string[128] = {0};
	WCHAR group4_string[128] = {0};

	if (total_count)
	{
		_r_str_printf (group1_string, RTL_NUMBER_OF (group1_string), L"%s (%d/%d)", _r_locale_getstring (group1_title), group1_count, total_count);

		if (group2_title)
			_r_str_printf (group2_string, RTL_NUMBER_OF (group2_string), L"%s (%d/%d)", _r_locale_getstring (group2_title), group2_count, total_count);

		if (group3_title)
			_r_str_printf (group3_string, RTL_NUMBER_OF (group3_string), L"%s (%d/%d)", _r_locale_getstring (group3_title), group3_count, total_count);

		if (group4_title)
			_r_str_printf (group4_string, RTL_NUMBER_OF (group4_string), L"%s (%d/%d) [silent]", _r_locale_getstring (group4_title), group4_count, total_count);
	}

	_r_listview_setgroup (hwnd, listview_id, 0, group1_string, 0, 0);
	_r_listview_setgroup (hwnd, listview_id, 1, group2_string, 0, 0);

	if (group3_title)
		_r_listview_setgroup (hwnd, listview_id, 2, group3_string, 0, 0);

	if (group4_title)
		_r_listview_setgroup (hwnd, listview_id, 3, group4_string, 0, 0);
}

VOID _app_listview_resize_ex (_In_ HWND hwnd, _In_ INT listview_id, _In_ BOOLEAN is_forced)
{
	PR_STRING column_text;
	PR_STRING item_text;
	HWND hlistview;
	HWND hheader;
	HDC hdc_listview;
	HDC hdc_header;
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

	if (!is_forced && !_r_config_getboolean (L"AutoSizeColumns", TRUE))
		return;

	hheader = NULL;
	hdc_listview = NULL;
	hdc_header = NULL;

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

VOID _app_listview_setfont (_In_ HWND hwnd, _In_ INT listview_id)
{
	if (config.hfont)
		SendDlgItemMessage (hwnd, listview_id, WM_SETFONT, (WPARAM)config.hfont, TRUE);
}

VOID _app_listview_setview (_In_ HWND hwnd, _In_ INT listview_id)
{
	HIMAGELIST himg;
	LONG view_type;
	LONG icons_size;
	BOOLEAN is_mainview;

	is_mainview = (listview_id >= IDC_APPS_PROFILE) && (listview_id <= IDC_RULES_CUSTOM);
	view_type = is_mainview ? _r_calc_clamp (_r_config_getlong (L"ViewType", LV_VIEW_DETAILS), LV_VIEW_ICON, LV_VIEW_MAX) : LV_VIEW_DETAILS;
	icons_size = is_mainview ? _r_calc_clamp (_r_config_getlong (L"IconSize", SHIL_SMALL), SHIL_LARGE, SHIL_LAST) : SHIL_SMALL;

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

INT CALLBACK _app_listview_compare_callback (_In_ LPARAM lparam1, _In_ LPARAM lparam2, _In_ LPARAM lparam)
{
	WCHAR config_name[128];
	HWND hwnd;
	LONG column_id;
	INT listview_id;
	INT result;
	INT item_id1;
	INT item_id2;
	BOOLEAN is_descend;

	PR_STRING item_text_1;
	PR_STRING item_text_2;
	PITEM_LOG ptr_log1;
	PITEM_LOG ptr_log2;
	PVOID timer1_ptr;
	PVOID timer2_ptr;
	LONG64 timestamp1;
	LONG64 timestamp2;
	BOOLEAN is_checked1;
	BOOLEAN is_checked2;

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

	if (!result)
	{
		// timestamp sorting
		if ((listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP) && column_id == 1)
		{
			timer1_ptr = _app_getappinfobyhash (_app_listview_getitemcontext (hwnd, listview_id, item_id1), INFO_TIMESTAMP_PTR);
			timer2_ptr = _app_getappinfobyhash (_app_listview_getitemcontext (hwnd, listview_id, item_id2), INFO_TIMESTAMP_PTR);

			if (timer1_ptr && timer2_ptr)
			{
				timestamp1 = *((PLONG64)timer1_ptr);
				timestamp2 = *((PLONG64)timer2_ptr);

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
			ptr_log1 = _app_getlogitem (_app_listview_getitemcontext (hwnd, listview_id, item_id1));
			ptr_log2 = _app_getlogitem (_app_listview_getitemcontext (hwnd, listview_id, item_id2));

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

VOID _app_listview_sort_ex (_In_ HWND hwnd, _In_ INT listview_id, _In_ LONG column_id, _In_ BOOLEAN is_notifycode)
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

	column_id = _r_calc_clamp (column_id, 0, column_count - 1); // set range

	if (is_notifycode)
	{
		_r_config_setboolean_ex (L"SortIsDescending", is_descend, config_name);
		_r_config_setlong_ex (L"SortColumn", column_id, config_name);
	}

	for (INT i = 0; i < column_count; i++)
		_r_listview_setcolumnsortindex (hwnd, listview_id, i, 0);

	_r_listview_setcolumnsortindex (hwnd, listview_id, column_id, is_descend ? -1 : 1);

	SendMessage (hlistview, LVM_SORTITEMSEX, (WPARAM)hlistview, (LPARAM)&_app_listview_compare_callback);
}
