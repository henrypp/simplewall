// simplewall
// Copyright (c) 2016-2021 Henry++

#include "global.h"

VOID _app_settab_id (HWND hwnd, INT page_id)
{
	if (!page_id || ((INT)_r_tab_getlparam (hwnd, IDC_TAB, -1) == page_id && IsWindowVisible (GetDlgItem (hwnd, page_id))))
		return;

	for (INT i = 0; i < (INT)SendDlgItemMessage (hwnd, IDC_TAB, TCM_GETITEMCOUNT, 0, 0); i++)
	{
		INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, i);

		if (listview_id == page_id)
		{
			_r_tab_selectitem (hwnd, IDC_TAB, i);
			return;
		}
	}

	_app_settab_id (hwnd, IDC_APPS_PROFILE);
}

UINT _app_getinterfacestatelocale (ENUM_INSTALL_TYPE install_type)
{
	if (install_type == InstallEnabled)
		return IDS_STATUS_FILTERS_ACTIVE;

	else if (install_type == InstallEnabledTemporary)
		return IDS_STATUS_FILTERS_ACTIVE_TEMP;

	// InstallDisabled
	return IDS_STATUS_FILTERS_INACTIVE;
}

BOOLEAN _app_initinterfacestate (HWND hwnd, BOOLEAN is_forced)
{
	if (!hwnd)
		return FALSE;

	if (is_forced || !!SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_ISBUTTONENABLED, IDM_TRAY_START, 0))
	{
		SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_ENABLEBUTTON, IDM_TRAY_START, MAKELPARAM (FALSE, 0));
		SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_ENABLEBUTTON, IDM_REFRESH, MAKELPARAM (FALSE, 0));

		_r_status_settextformat (hwnd, IDC_STATUSBAR, 0, L"%s...", _r_locale_getstring (IDS_STATUS_FILTERS_PROCESSING));

		return TRUE;
	}

	return FALSE;
}

VOID _app_restoreinterfacestate (HWND hwnd, BOOLEAN is_enabled)
{
	if (!is_enabled)
		return;

	SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_ENABLEBUTTON, IDM_TRAY_START, MAKELPARAM (TRUE, 0));
	SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_ENABLEBUTTON, IDM_REFRESH, MAKELPARAM (TRUE, 0));

	ENUM_INSTALL_TYPE install_type = _wfp_isfiltersinstalled ();

	_r_status_settext (hwnd, IDC_STATUSBAR, 0, _r_locale_getstring (_app_getinterfacestatelocale (install_type)));
}

VOID _app_setinterfacestate (HWND hwnd)
{
	ENUM_INSTALL_TYPE install_type = _wfp_isfiltersinstalled ();

	BOOLEAN is_filtersinstalled = (install_type != InstallDisabled);
	INT icon_id = is_filtersinstalled ? IDI_ACTIVE : IDI_INACTIVE;
	UINT string_id = is_filtersinstalled ? IDS_TRAY_STOP : IDS_TRAY_START;

	HICON hico_sm = _r_app_getsharedimage (_r_sys_getimagebase (), icon_id, _r_dc_getsystemmetrics (hwnd, SM_CXSMICON));
	HICON hico_big = _r_app_getsharedimage (_r_sys_getimagebase (), icon_id, _r_dc_getsystemmetrics (hwnd, SM_CXICON));

	SendMessage (hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hico_sm);
	SendMessage (hwnd, WM_SETICON, ICON_BIG, (LPARAM)hico_big);

	//SendDlgItemMessage (hwnd, IDC_STATUSBAR, SB_SETICON, 0, (LPARAM)hico_sm);

	if (!_wfp_isfiltersapplying ())
		_r_status_settext (hwnd, IDC_STATUSBAR, 0, _r_locale_getstring (_app_getinterfacestatelocale (install_type)));

	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_START, _r_locale_getstring (string_id), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, 0, is_filtersinstalled ? 1 : 0);

	_r_tray_setinfo (hwnd, UID, hico_sm, APP_NAME);
}

VOID _app_imagelist_init (HWND hwnd)
{
	INT icon_size_small = _r_dc_getsystemmetrics (hwnd, SM_CXSMICON);
	INT icon_size_large = _r_dc_getsystemmetrics (hwnd, SM_CXICON);
	INT icon_size_toolbar = _r_calc_clamp (_r_dc_getdpi (hwnd, _r_config_getinteger (L"ToolbarSize", PR_SIZE_ITEMHEIGHT)), icon_size_small, icon_size_large);

	SAFE_DELETE_OBJECT (config.hbmp_enable);
	SAFE_DELETE_OBJECT (config.hbmp_disable);
	SAFE_DELETE_OBJECT (config.hbmp_allow);
	SAFE_DELETE_OBJECT (config.hbmp_block);
	SAFE_DELETE_OBJECT (config.hbmp_cross);
	SAFE_DELETE_OBJECT (config.hbmp_rules);

	SAFE_DELETE_ICON (config.hicon_large);
	SAFE_DELETE_ICON (config.hicon_small);
	SAFE_DELETE_ICON (config.hicon_uwp);

	// get default icon for executable
	if (!_r_obj_isstringempty (config.ntoskrnl_path))
	{
		_app_getfileicon (config.ntoskrnl_path->buffer, FALSE, &config.icon_id, &config.hicon_large);
		_app_getfileicon (config.ntoskrnl_path->buffer, TRUE, NULL, &config.hicon_small);
	}

	// get default icon for windows store package (win8+)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
	{
		if (!_r_obj_isstringempty (config.winstore_path))
		{
			if (!_app_getfileicon (config.winstore_path->buffer, TRUE, &config.icon_uwp_id, &config.hicon_uwp))
			{
				config.icon_uwp_id = config.icon_id;
				config.hicon_uwp = CopyIcon (config.hicon_small);
			}
		}
	}

	config.hbmp_enable = _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_SHIELD_ENABLE), icon_size_small);
	config.hbmp_disable = _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_SHIELD_DISABLE), icon_size_small);

	config.hbmp_allow = _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_ALLOW), icon_size_small);
	config.hbmp_block = _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_BLOCK), icon_size_small);
	config.hbmp_cross = _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_CROSS), icon_size_small);
	config.hbmp_rules = _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_SETTINGS), icon_size_small);

	// toolbar imagelist
	if (config.himg_toolbar)
	{
		ImageList_SetIconSize (config.himg_toolbar, icon_size_toolbar, icon_size_toolbar);
	}
	else
	{
		config.himg_toolbar = ImageList_Create (icon_size_toolbar, icon_size_toolbar, ILC_COLOR32 | ILC_HIGHQUALITYSCALE, 0, 0);
	}

	if (config.himg_toolbar)
	{
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_SHIELD_ENABLE), icon_size_toolbar), NULL);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_SHIELD_DISABLE), icon_size_toolbar), NULL);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_REFRESH), icon_size_toolbar), NULL);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_SETTINGS), icon_size_toolbar), NULL);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_NOTIFICATIONS), icon_size_toolbar), NULL);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_LOG), icon_size_toolbar), NULL);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_LOGOPEN), icon_size_toolbar), NULL);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_LOGCLEAR), icon_size_toolbar), NULL);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_ADD), icon_size_toolbar), NULL);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_DONATE), icon_size_toolbar), NULL);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_LOGUI), icon_size_toolbar), NULL);
	}

	// rules imagelist (small)
	if (config.himg_rules_small)
	{
		ImageList_SetIconSize (config.himg_rules_small, icon_size_small, icon_size_small);
	}
	else
	{
		config.himg_rules_small = ImageList_Create (icon_size_small, icon_size_small, ILC_COLOR32 | ILC_HIGHQUALITYSCALE, 2, 2);
	}

	if (config.himg_rules_small)
	{
		ImageList_Add (config.himg_rules_small, _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_ALLOW), icon_size_small), NULL);
		ImageList_Add (config.himg_rules_small, _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_BLOCK), icon_size_small), NULL);
	}

	// rules imagelist (large)
	if (config.himg_rules_large)
	{
		ImageList_SetIconSize (config.himg_rules_large, icon_size_large, icon_size_large);
	}
	else
	{
		config.himg_rules_large = ImageList_Create (icon_size_large, icon_size_large, ILC_COLOR32 | ILC_HIGHQUALITYSCALE, 2, 2);
	}

	if (config.himg_rules_large)
	{
		ImageList_Add (config.himg_rules_large, _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_ALLOW), icon_size_large), NULL);
		ImageList_Add (config.himg_rules_large, _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_BLOCK), icon_size_large), NULL);
	}
}

VOID _app_listviewresize (HWND hwnd, INT listview_id, BOOLEAN is_forced)
{
	if (!listview_id || (!is_forced && !_r_config_getboolean (L"AutoSizeColumns", TRUE)))
		return;

	INT column_count = _r_listview_getcolumncount (hwnd, listview_id);

	if (!column_count)
		return;

	RECT rc_client;
	PR_STRING column_text;
	PR_STRING item_text;
	HWND hlistview;
	HWND hheader;
	HDC hdc_listview;
	HDC hdc_header;
	INT column_width;
	INT text_width;
	INT calculated_width;
	INT column_general_id;
	INT total_width;
	INT max_width;
	INT spacing;
	BOOLEAN is_tableview;

	hlistview = GetDlgItem (hwnd, listview_id);
	hheader = (HWND)SendMessage (hlistview, LVM_GETHEADER, 0, 0);

	GetClientRect (hlistview, &rc_client);

	// get device context and fix font set
	hdc_listview = GetDC (hlistview);
	hdc_header = GetDC (hheader);

	if (hdc_listview)
		SelectObject (hdc_listview, (HFONT)SendMessage (hlistview, WM_GETFONT, 0, 0)); // fix

	if (hdc_header)
		SelectObject (hdc_header, (HFONT)SendMessage (hheader, WM_GETFONT, 0, 0)); // fix

	calculated_width = 0;
	column_general_id = 0; // set general column id

	is_tableview = ((INT)SendMessage (hlistview, LVM_GETVIEW, 0, 0) == LV_VIEW_DETAILS);

	total_width = _r_calc_rectwidth (&rc_client);

	max_width = _r_dc_getdpi (hwnd, 158);
	spacing = _r_dc_getsystemmetrics (hwnd, SM_CXSMICON);

	for (INT i = 0; i < column_count; i++)
	{
		if (i == column_general_id)
			continue;

		// get column text width
		column_text = _r_listview_getcolumntext (hwnd, listview_id, i);

		if (!column_text)
			continue;

		column_width = _r_dc_getfontwidth (hdc_header, _r_obj_getstring (column_text), _r_obj_getstringlength (column_text)) + spacing;

		if (column_width >= max_width)
		{
			column_width = max_width;
		}
		else
		{
			// calculate max width of listview subitems (only for details view)
			if (is_tableview)
			{
				for (INT j = 0; j < _r_listview_getitemcount (hwnd, listview_id); j++)
				{
					item_text = _r_listview_getitemtext (hwnd, listview_id, j, i);

					if (item_text)
					{
						text_width = _r_dc_getfontwidth (hdc_listview, _r_obj_getstring (item_text), _r_obj_getstringlength (item_text)) + spacing;
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

	if (hdc_listview)
		ReleaseDC (hlistview, hdc_listview);

	if (hdc_header)
		ReleaseDC (hheader, hdc_header);
}

VOID _app_listviewsetview (HWND hwnd, INT listview_id)
{
	BOOLEAN is_mainview = (listview_id >= IDC_APPS_PROFILE) && (listview_id <= IDC_RULES_CUSTOM);
	INT view_type = is_mainview ? _r_calc_clamp (_r_config_getinteger (L"ViewType", LV_VIEW_DETAILS), LV_VIEW_ICON, LV_VIEW_MAX) : LV_VIEW_DETAILS;
	INT icons_size = is_mainview ? _r_calc_clamp (_r_config_getinteger (L"IconSize", SHIL_SYSSMALL), SHIL_LARGE, SHIL_LAST) : SHIL_SYSSMALL;
	HIMAGELIST himg = NULL;

	if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM || listview_id == IDC_APP_RULES_ID)
	{
		himg = (icons_size == SHIL_SMALL || icons_size == SHIL_SYSSMALL) ? config.himg_rules_small : config.himg_rules_large;
	}
	else
	{
		SHGetImageList (icons_size, &IID_IImageList2, &himg);
	}

	if (himg)
	{
		SendDlgItemMessage (hwnd, listview_id, LVM_SETIMAGELIST, LVSIL_NORMAL, (LPARAM)himg);
		SendDlgItemMessage (hwnd, listview_id, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)himg);
	}

	SendDlgItemMessage (hwnd, listview_id, LVM_SETVIEW, (WPARAM)view_type, 0);
}

VOID _app_listviewsetfont (HWND hwnd, INT listview_id, BOOLEAN is_forced)
{
	LOGFONT lf = {0};

	if (is_forced || !config.hfont)
	{
		SAFE_DELETE_OBJECT (config.hfont);

		_r_config_getfont (L"Font", hwnd, &lf, NULL);

		config.hfont = CreateFontIndirect (&lf);
	}

	if (config.hfont)
		SendDlgItemMessage (hwnd, listview_id, WM_SETFONT, (WPARAM)config.hfont, TRUE);
}

INT CALLBACK _app_listviewcompare_callback (LPARAM lparam1, LPARAM lparam2, LPARAM lparam)
{
	HWND hlistview = (HWND)lparam;
	HWND hparent = GetParent (hlistview);
	INT listview_id = GetDlgCtrlID (hlistview);

	INT item1 = _app_getposition (hparent, listview_id, lparam1);
	INT item2 = _app_getposition (hparent, listview_id, lparam2);

	if (item1 == -1 || item2 == -1)
		return 0;

	WCHAR config_name[128];
	_r_str_printf (config_name, RTL_NUMBER_OF (config_name), L"listview\\%04" TEXT (PRIX32), listview_id);

	INT column_id = _r_config_getintegerex (L"SortColumn", 0, config_name);
	BOOLEAN is_descend = _r_config_getbooleanex (L"SortIsDescending", FALSE, config_name);

	INT result = 0;

	if ((SendMessage (hlistview, LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0) & LVS_EX_CHECKBOXES) != 0)
	{
		BOOLEAN is_checked1 = _r_listview_isitemchecked (hparent, listview_id, item1);
		BOOLEAN is_checked2 = _r_listview_isitemchecked (hparent, listview_id, item2);

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
			PVOID timer1_ptr = _app_getappinfobyhash (lparam1, InfoTimestampPtr);
			PVOID timer2_ptr = _app_getappinfobyhash (lparam2, InfoTimestampPtr);

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
	}

	if (!result)
	{
		PR_STRING item_text_1 = _r_listview_getitemtext (hparent, listview_id, item1, column_id);
		PR_STRING item_text_2 = _r_listview_getitemtext (hparent, listview_id, item2, column_id);

		if (item_text_1 && item_text_2)
		{
			result = _r_str_compare_logical (item_text_1->buffer, item_text_2->buffer);
		}

		if (item_text_1)
			_r_obj_dereference (item_text_1);

		if (item_text_2)
			_r_obj_dereference (item_text_2);
	}

	return is_descend ? -result : result;
}

VOID _app_listviewsort (HWND hwnd, INT listview_id, INT column_id, BOOLEAN is_notifycode)
{
	HWND hlistview = GetDlgItem (hwnd, listview_id);

	if (!hlistview)
		return;

	if ((GetWindowLongPtr (hlistview, GWL_STYLE) & (LVS_NOSORTHEADER | LVS_OWNERDATA)) != 0)
		return;

	INT column_count = _r_listview_getcolumncount (hwnd, listview_id);

	if (!column_count)
		return;

	WCHAR config_name[128];
	_r_str_printf (config_name, RTL_NUMBER_OF (config_name), L"listview\\%04" TEXT (PRIX32), listview_id);

	BOOLEAN is_descend = _r_config_getbooleanex (L"SortIsDescending", FALSE, config_name);

	if (is_notifycode)
		is_descend = !is_descend;

	if (column_id == -1)
		column_id = _r_config_getintegerex (L"SortColumn", 0, config_name);

	column_id = _r_calc_clamp (column_id, 0, column_count - 1); // set range

	if (is_notifycode)
	{
		_r_config_setbooleanex (L"SortIsDescending", is_descend, config_name);
		_r_config_setintegerex (L"SortColumn", column_id, config_name);
	}

	for (INT i = 0; i < column_count; i++)
		_r_listview_setcolumnsortindex (hwnd, listview_id, i, 0);

	_r_listview_setcolumnsortindex (hwnd, listview_id, column_id, is_descend ? -1 : 1);

	SendMessage (hlistview, LVM_SORTITEMS, (WPARAM)hlistview, (LPARAM)&_app_listviewcompare_callback);
}

VOID _app_toolbar_init (HWND hwnd)
{
	config.hrebar = GetDlgItem (hwnd, IDC_REBAR);

	_r_toolbar_setstyle (hwnd, IDC_TOOLBAR, TBSTYLE_EX_DOUBLEBUFFER | TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_HIDECLIPPEDBUTTONS);

	SendDlgItemMessage (hwnd, IDC_TOOLBAR, TB_SETIMAGELIST, 0, (LPARAM)config.himg_toolbar);

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

	REBARBANDINFO rbi = {0};

	rbi.cbSize = sizeof (rbi);
	rbi.fMask = RBBIM_STYLE | RBBIM_CHILD;
	rbi.fStyle = RBBS_CHILDEDGE | RBBS_USECHEVRON | RBBS_VARIABLEHEIGHT | RBBS_NOGRIPPER;
	rbi.hwndChild = GetDlgItem (hwnd, IDC_TOOLBAR);

	SendMessage (config.hrebar, RB_INSERTBAND, 0, (LPARAM)&rbi);
}

VOID _app_toolbar_resize ()
{
	REBARBANDINFO rbi = {0};
	SIZE ideal_size = {0};
	ULONG button_size;

	rbi.cbSize = sizeof (rbi);

	SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_AUTOSIZE, 0, 0);

	button_size = (ULONG)SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_GETBUTTONSIZE, 0, 0);

	if (button_size)
	{
		rbi.fMask |= RBBIM_CHILDSIZE;
		rbi.cxMinChild = LOWORD (button_size);
		rbi.cyMinChild = HIWORD (button_size);
	}

	if (SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_GETIDEALSIZE, FALSE, (LPARAM)&ideal_size))
	{
		rbi.fMask |= RBBIM_SIZE | RBBIM_IDEALSIZE;
		rbi.cx = (UINT)ideal_size.cx;
		rbi.cxIdeal = (UINT)ideal_size.cx;
	}

	SendMessage (config.hrebar, RB_SETBANDINFO, 0, (LPARAM)&rbi);
}

VOID _app_refreshgroups (HWND hwnd, INT listview_id)
{
	UINT group1_title = 0;
	UINT group2_title = 0;
	UINT group3_title = 0;

	if (!SendDlgItemMessage (hwnd, listview_id, LVM_ISGROUPVIEWENABLED, 0, 0))
		return;

	if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
	{
		group1_title = IDS_GROUP_ALLOWED;
		group2_title = IDS_GROUP_SPECIAL_APPS;
		group3_title = IDS_GROUP_BLOCKED;
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

	INT total_count = _r_listview_getitemcount (hwnd, listview_id);

	INT group1_count = 0;
	INT group2_count = 0;
	INT group3_count = 0;

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
			LVITEM lvi = {0};

			lvi.mask = LVIF_GROUPID;
			lvi.iItem = i;

			if (SendDlgItemMessage (hwnd, listview_id, LVM_GETITEM, 0, (LPARAM)&lvi))
			{
				if (lvi.iGroupId == 2)
				{
					group3_count += 1;
				}
				else if (lvi.iGroupId == 1)
				{
					group2_count += 1;
				}
				else
				{
					group1_count += 1;
				}
			}
		}
	}

	WCHAR group1_string[128] = {0};
	WCHAR group2_string[128] = {0};
	WCHAR group3_string[128] = {0};
	PR_STRING localized_string = NULL;

	if (total_count)
	{
		_r_str_printf (group1_string, RTL_NUMBER_OF (group1_string), L" (%d/%d)", group1_count, total_count);
		_r_str_printf (group2_string, RTL_NUMBER_OF (group1_string), L" (%d/%d)", group2_count, total_count);

		if (group3_title)
			_r_str_printf (group3_string, RTL_NUMBER_OF (group1_string), L" (%d/%d)", group3_count, total_count);
	}

	_r_obj_movereference (&localized_string, _r_format_string (L"%s%s", _r_locale_getstring (group1_title), group1_string));
	_r_listview_setgroup (hwnd, listview_id, 0, _r_obj_getstringorempty (localized_string), 0, 0);

	_r_obj_movereference (&localized_string, _r_format_string (L"%s%s", _r_locale_getstring (group2_title), group2_string));
	_r_listview_setgroup (hwnd, listview_id, 1, _r_obj_getstringorempty (localized_string), 0, 0);

	if (group3_title)
	{
		_r_obj_movereference (&localized_string, _r_format_string (L"%s%s", _r_locale_getstring (group3_title), group3_string));
		_r_listview_setgroup (hwnd, listview_id, 2, _r_obj_getstringorempty (localized_string), 0, 0);
	}

	SAFE_DELETE_REFERENCE (localized_string);
}

VOID _app_refreshstatus (HWND hwnd, INT listview_id)
{
	ITEM_STATUS status = {0};

	_app_getcount (&status);

	HWND hstatus = GetDlgItem (hwnd, IDC_STATUSBAR);
	HDC hdc = GetDC (hstatus);

	// item count
	if (hdc)
	{
		SelectObject (hdc, (HFONT)SendMessage (hstatus, WM_GETFONT, 0, 0)); // fix

		INT spacing = _r_dc_getdpi (hwnd, 12);

		PR_STRING text[UI_STATUSBAR_PARTS_COUNT] = {0};
		INT parts[UI_STATUSBAR_PARTS_COUNT] = {0};
		LONG size[UI_STATUSBAR_PARTS_COUNT] = {0};
		LONG lay = 0;

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
				size[i] = _r_dc_getfontwidth (hdc, _r_obj_getstringorempty (text[i]), _r_obj_getstringlength (text[i])) + spacing;
				lay += size[i];
			}
		}

		RECT rc_client = {0};
		GetClientRect (hstatus, &rc_client);

		parts[0] = _r_calc_rectwidth (&rc_client) - lay - _r_dc_getsystemmetrics (hwnd, SM_CXVSCROLL) - (_r_dc_getsystemmetrics (hwnd, SM_CXBORDER) * 2);
		parts[1] = parts[0] + size[1];
		parts[2] = parts[1] + size[2];

		SendMessage (hstatus, SB_SETPARTS, UI_STATUSBAR_PARTS_COUNT, (LPARAM)parts);

		for (INT i = 1; i < UI_STATUSBAR_PARTS_COUNT; i++)
		{
			if (text[i])
			{
				_r_status_settext (hwnd, IDC_STATUSBAR, i, _r_obj_getstringorempty (text[i]));

				_r_obj_dereference (text[i]);
			}
		}

		ReleaseDC (hstatus, hdc);
	}

	// group information
	if (listview_id)
	{
		if (listview_id == -1)
			listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, -1);

		_app_refreshgroups (hwnd, listview_id);
	}
}

INT _app_getposition (HWND hwnd, INT listview_id, LPARAM lparam)
{
	LVFINDINFO lvfi = {0};

	lvfi.flags = LVFI_PARAM;
	lvfi.lParam = lparam;

	return (INT)SendDlgItemMessage (hwnd, listview_id, LVM_FINDITEM, (WPARAM)-1, (LPARAM)&lvfi);
}

VOID _app_showitem (HWND hwnd, INT listview_id, INT item, INT scroll_pos)
{
	HWND hlistview = GetDlgItem (hwnd, listview_id);

	if (!hlistview)
		return;

	_app_settab_id (hwnd, listview_id);

	INT total_count = _r_listview_getitemcount (hwnd, listview_id);

	if (!total_count)
		return;

	if (item != -1)
	{
		item = _r_calc_clamp (item, 0, total_count - 1);

		PostMessage (hlistview, LVM_ENSUREVISIBLE, (WPARAM)item, TRUE); // ensure item visible

		ListView_SetItemState (hlistview, -1, 0, LVIS_SELECTED | LVIS_FOCUSED); // deselect all
		ListView_SetItemState (hlistview, (WPARAM)item, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED); // select item
	}

	if (scroll_pos > 0)
		PostMessage (hlistview, LVM_SCROLL, 0, (LPARAM)scroll_pos); // restore scroll position
}

BOOLEAN _app_showappitem (HWND hwnd, PITEM_APP ptr_app)
{
	INT listview_id = PtrToInt (_app_getappinfo (ptr_app, InfoListviewId));

	if (listview_id)
	{
		if (listview_id == (INT)_r_tab_getlparam (hwnd, IDC_TAB, -1))
			_app_listviewsort (hwnd, listview_id, -1, FALSE);

		_app_showitem (hwnd, listview_id, _app_getposition (hwnd, listview_id, ptr_app->app_hash), -1);

		return TRUE;
	}

	return FALSE;
}
