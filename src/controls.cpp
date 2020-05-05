// simplewall
// Copyright (c) 2016-2020 Henry++

#include "global.hpp"

void _app_settab_id (HWND hwnd, INT page_id)
{
	if (!page_id || ((INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT) == page_id && IsWindowVisible (GetDlgItem (hwnd, page_id))))
		return;

	for (INT i = 0; i < (INT)SendDlgItemMessage (hwnd, IDC_TAB, TCM_GETITEMCOUNT, 0, 0); i++)
	{
		const INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, i);

		if (listview_id == page_id)
		{
			_r_tab_selectitem (hwnd, IDC_TAB, i);
			return;
		}
	}

	_app_settab_id (hwnd, IDC_APPS_PROFILE);
}

UINT _app_getinterfacestatelocale (EnumInstall install_type)
{
	if (install_type == InstallDisabled)
		return IDS_STATUS_FILTERS_INACTIVE;

	else if (install_type == InstallEnabled)
		return IDS_STATUS_FILTERS_ACTIVE;

	else if (install_type == InstallEnabledTemporary)
		return IDS_STATUS_FILTERS_ACTIVE_TEMP;

	return 0;
}

bool _app_initinterfacestate (HWND hwnd, bool is_forced)
{
	if (!hwnd)
		return false;

	if (is_forced || !!SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_ISBUTTONENABLED, IDM_TRAY_START, 0))
	{
		SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_ENABLEBUTTON, IDM_TRAY_START, MAKELPARAM (FALSE, 0));
		SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_ENABLEBUTTON, IDM_REFRESH, MAKELPARAM (FALSE, 0));

		_r_status_settext (hwnd, IDC_STATUSBAR, 0, app.LocaleString (IDS_STATUS_FILTERS_PROCESSING, L"..."));

		return true;
	}

	return false;
}

void _app_restoreinterfacestate (HWND hwnd, bool is_enabled)
{
	if (!is_enabled)
		return;

	SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_ENABLEBUTTON, IDM_TRAY_START, MAKELPARAM (TRUE, 0));
	SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_ENABLEBUTTON, IDM_REFRESH, MAKELPARAM (TRUE, 0));

	const EnumInstall install_type = _wfp_isfiltersinstalled ();

	_r_status_settext (hwnd, IDC_STATUSBAR, 0, app.LocaleString (_app_getinterfacestatelocale (install_type), nullptr));
}

void _app_setinterfacestate (HWND hwnd)
{
	const EnumInstall install_type = _wfp_isfiltersinstalled ();

	const bool is_filtersinstalled = (install_type != InstallDisabled);
	const INT icon_id = is_filtersinstalled ? IDI_ACTIVE : IDI_INACTIVE;

	const HICON hico_sm = app.GetSharedImage (app.GetHINSTANCE (), icon_id, _r_dc_getsystemmetrics (hwnd, SM_CXSMICON));
	const HICON hico_big = app.GetSharedImage (app.GetHINSTANCE (), icon_id, _r_dc_getsystemmetrics (hwnd, SM_CXICON));

	SendMessage (hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hico_sm);
	SendMessage (hwnd, WM_SETICON, ICON_BIG, (LPARAM)hico_big);

	//SendDlgItemMessage (hwnd, IDC_STATUSBAR, SB_SETICON, 0, (LPARAM)hico_sm);

	if (!_wfp_isfiltersapplying ())
		_r_status_settext (hwnd, IDC_STATUSBAR, 0, app.LocaleString (_app_getinterfacestatelocale (install_type), nullptr));

	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_START, app.LocaleString (is_filtersinstalled ? IDS_TRAY_STOP : IDS_TRAY_START, nullptr), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, 0, is_filtersinstalled ? 1 : 0);

	_r_tray_setinfo (hwnd, UID, hico_sm, APP_NAME);
}

void _app_listviewresize (HWND hwnd, INT listview_id, bool is_forced)
{
	if (!listview_id || (!is_forced && !app.ConfigGet (L"AutoSizeColumns", true).AsBool ()))
		return;

	const INT column_count = _r_listview_getcolumncount (hwnd, listview_id);

	if (!column_count)
		return;

	const HWND hlistview = GetDlgItem (hwnd, listview_id);
	const HWND hheader = (HWND)SendMessage (hlistview, LVM_GETHEADER, 0, 0);

	RECT rc_client = {0};
	GetClientRect (hlistview, &rc_client);

	// get device context and fix font set
	const HDC hdc_listview = GetDC (hlistview);
	const HDC hdc_header = GetDC (hheader);

	if (hdc_listview)
		SelectObject (hdc_listview, (HFONT)SendMessage (hlistview, WM_GETFONT, 0, 0)); // fix

	if (hdc_header)
		SelectObject (hdc_header, (HFONT)SendMessage (hheader, WM_GETFONT, 0, 0)); // fix

	INT calculated_width = 0;

	// set general column id
	INT column_general_id = 0;

	const bool is_tableview = (SendMessage (hlistview, LVM_GETVIEW, 0, 0) == LV_VIEW_DETAILS);

	const INT total_width = _R_RECT_WIDTH (&rc_client);
	const INT spacing = _r_dc_getsystemmetrics (hwnd, SM_CXSMICON);

	const INT column_max_width = _r_dc_getdpi (hwnd, 174);

	for (INT i = 0; i < column_count; i++)
	{
		if (i == column_general_id)
			continue;

		// get column text width
		const rstring column_text = _r_listview_getcolumntext (hwnd, listview_id, i);
		INT column_width = _r_dc_fontwidth (hdc_header, column_text, column_text.GetLength ()) + spacing;

		if (column_width >= column_max_width)
		{
			column_width = column_max_width;
		}
		else
		{
			// calculate max width of listview subitems (only for details view)
			if (is_tableview)
			{
				for (INT j = 0; j < _r_listview_getitemcount (hwnd, listview_id); j++)
				{
					const rstring item_text = _r_listview_getitemtext (hwnd, listview_id, j, i);
					const INT text_width = _r_dc_fontwidth (hdc_listview, item_text, item_text.GetLength ()) + spacing;

					// do not continue reaching higher and higher values for performance reason!
					if (text_width >= column_max_width)
					{
						column_width = column_max_width;
						break;
					}

					if (text_width > column_width)
						column_width = text_width;
				}
			}
		}

		_r_listview_setcolumn (hwnd, listview_id, i, nullptr, column_width);

		calculated_width += column_width;
	}

	// set general column width
	_r_listview_setcolumn (hwnd, listview_id, column_general_id, nullptr, (std::max)(total_width - calculated_width, column_max_width));

	if (hdc_listview)
		ReleaseDC (hlistview, hdc_listview);

	if (hdc_header)
		ReleaseDC (hheader, hdc_header);
}

void _app_listviewsetview (HWND hwnd, INT listview_id)
{
	const bool is_mainview = (listview_id >= IDC_APPS_PROFILE) && (listview_id <= IDC_RULES_CUSTOM);
	const INT view_type = is_mainview ? std::clamp (app.ConfigGet (L"ViewType", LV_VIEW_DETAILS).AsInt (), LV_VIEW_ICON, LV_VIEW_MAX) : LV_VIEW_DETAILS;

	const INT icons_size = (is_mainview || listview_id == IDC_NETWORK) ? std::clamp (app.ConfigGet (L"IconSize", SHIL_SYSSMALL).AsInt (), SHIL_LARGE, SHIL_LAST) : SHIL_SYSSMALL;
	HIMAGELIST himg = nullptr;

	if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM)
	{
		if (icons_size == SHIL_SMALL || icons_size == SHIL_SYSSMALL)
			himg = config.himg_rules_small;

		else
			himg = config.himg_rules_large;
	}
	else
	{
		SHGetImageList (icons_size, IID_IImageList2, (void**)&himg);
	}

	if (himg)
	{
		SendDlgItemMessage (hwnd, listview_id, LVM_SETIMAGELIST, LVSIL_NORMAL, (LPARAM)himg);
		SendDlgItemMessage (hwnd, listview_id, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)himg);
	}

	SendDlgItemMessage (hwnd, listview_id, LVM_SETVIEW, (WPARAM)view_type, 0);
	SendDlgItemMessage (hwnd, listview_id, LVM_SCROLL, 0, (LPARAM)GetScrollPos (GetDlgItem (hwnd, listview_id), SB_VERT)); // HACK!!!
}

void _app_listviewinitfont (HWND hwnd, PLOGFONT plf)
{
	const rstring buffer = app.ConfigGet (L"Font", UI_FONT_DEFAULT);

	if (!buffer.IsEmpty ())
	{
		rstringvec rvc;
		_r_str_split (buffer, buffer.GetLength (), L';', rvc);

		if (!rvc.empty ())
		{
			_r_str_copy (plf->lfFaceName, LF_FACESIZE, rvc.at (0)); // face name

			if (rvc.size () >= 2)
			{
				plf->lfHeight = _r_dc_fontsizetoheight (hwnd, rvc.at (1).AsInt ()); // size

				if (rvc.size () >= 3)
					plf->lfWeight = rvc.at (2).AsInt (); // weight
			}
		}
	}

	// fill missed font values
	{
		NONCLIENTMETRICS ncm = {0};
		ncm.cbSize = sizeof (ncm);

		if (SystemParametersInfo (SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0))
		{
			PLOGFONT pdeflf = &ncm.lfMessageFont;

			if (_r_str_isempty (plf->lfFaceName))
				_r_str_copy (plf->lfFaceName, LF_FACESIZE, pdeflf->lfFaceName);

			if (!plf->lfHeight)
				plf->lfHeight = pdeflf->lfHeight;

			if (!plf->lfWeight)
				plf->lfWeight = pdeflf->lfWeight;

			// set default values
			plf->lfCharSet = pdeflf->lfCharSet;
			plf->lfQuality = pdeflf->lfQuality;
		}
	}
}

void _app_listviewsetfont (HWND hwnd, INT listview_id, bool is_redraw)
{
	LOGFONT lf = {0};

	if (is_redraw || !config.hfont)
	{
		SAFE_DELETE_OBJECT (config.hfont);

		_app_listviewinitfont (hwnd, &lf);

		config.hfont = CreateFontIndirect (&lf);
	}

	if (config.hfont)
		SendDlgItemMessage (hwnd, listview_id, WM_SETFONT, (WPARAM)config.hfont, TRUE);
}

INT CALLBACK _app_listviewcompare_callback (LPARAM lparam1, LPARAM lparam2, LPARAM lparam)
{
	const HWND hlistview = (HWND)lparam;
	const HWND hparent = GetParent (hlistview);
	const INT listview_id = GetDlgCtrlID (hlistview);

	const INT item1 = _app_getposition (hparent, listview_id, lparam1);
	const INT item2 = _app_getposition (hparent, listview_id, lparam2);

	if (item1 == INVALID_INT || item2 == INVALID_INT)
		return 0;

	const rstring cfg_name = _r_fmt (L"listview\\%04" PRIX32, listview_id);

	const INT column_id = app.ConfigGet (L"SortColumn", 0, cfg_name).AsInt ();
	const bool is_descend = app.ConfigGet (L"SortIsDescending", false, cfg_name).AsBool ();

	INT result = 0;

	if ((SendMessage (hlistview, LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0) & LVS_EX_CHECKBOXES) != 0)
	{
		const bool is_checked1 = _r_listview_isitemchecked (hparent, listview_id, item1);
		const bool is_checked2 = _r_listview_isitemchecked (hparent, listview_id, item2);

		if (is_checked1 != is_checked2)
		{
			if (is_checked1 && !is_checked2)
				result = is_descend ? 1 : -1;

			else if (!is_checked1 && is_checked2)
				result = is_descend ? -1 : 1;
		}
	}

	if (!result)
	{
		// timestamp sorting
		if ((listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP) && column_id == 1)
		{
			const time_t* timestamp1 = (time_t*)_app_getappinfo (lparam1, InfoTimestamp);
			const time_t* timestamp2 = (time_t*)_app_getappinfo (lparam2, InfoTimestamp);

			if (timestamp1 && timestamp2)
			{
				if (*timestamp1 < *timestamp2)
					result = -1;

				else if (*timestamp1 > * timestamp2)
					result = 1;
			}
		}
	}

	if (!result)
	{
		result = _r_str_compare_logical (
			_r_listview_getitemtext (hparent, listview_id, item1, column_id),
			_r_listview_getitemtext (hparent, listview_id, item2, column_id)
		);
	}

	return is_descend ? -result : result;
}

void _app_listviewsort (HWND hwnd, INT listview_id, INT column_id, bool is_notifycode)
{
	const HWND hlistview = GetDlgItem (hwnd, listview_id);

	if (!hlistview)
		return;

	if ((GetWindowLongPtr (hlistview, GWL_STYLE) & (LVS_NOSORTHEADER | LVS_OWNERDATA)) != 0)
		return;

	const INT column_count = _r_listview_getcolumncount (hwnd, listview_id);

	if (!column_count)
		return;

	const rstring cfg_name = _r_fmt (L"listview\\%04" PRIX32, listview_id);
	bool is_descend = app.ConfigGet (L"SortIsDescending", false, cfg_name).AsBool ();

	if (is_notifycode)
		is_descend = !is_descend;

	if (column_id == INVALID_INT)
		column_id = app.ConfigGet (L"SortColumn", 0, cfg_name).AsInt ();

	column_id = std::clamp (column_id, 0, column_count - 1); // set range

	if (is_notifycode)
	{
		app.ConfigSet (L"SortIsDescending", is_descend, cfg_name);
		app.ConfigSet (L"SortColumn", column_id, cfg_name);
	}

	for (INT i = 0; i < column_count; i++)
		_r_listview_setcolumnsortindex (hwnd, listview_id, i, 0);

	_r_listview_setcolumnsortindex (hwnd, listview_id, column_id, is_descend ? -1 : 1);

	SendMessage (hlistview, LVM_SORTITEMS, (WPARAM)hlistview, (LPARAM)&_app_listviewcompare_callback);
}

void _app_refreshgroups (HWND hwnd, INT listview_id)
{
	UINT group1_title;
	UINT group2_title;
	UINT group3_title;

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
		group2_title = IDS_GROUP_SPECIAL;
		group3_title = IDS_GROUP_DISABLED;
	}
	else if (listview_id == IDC_RULE_APPS_ID)
	{
		group1_title = IDS_TAB_APPS;
		group2_title = IDS_TAB_SERVICES;
		group3_title = IDS_TAB_PACKAGES;
	}
	else
	{
		return;
	}

	const INT total_count = _r_listview_getitemcount (hwnd, listview_id);

	INT group1_count = 0;
	INT group2_count = 0;
	INT group3_count = 0;

	for (INT i = 0; i < total_count; i++)
	{
		if (listview_id == IDC_RULE_APPS_ID)
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
					group3_count += 1;

				else if (lvi.iGroupId == 1)
					group2_count += 1;

				else
					group1_count += 1;
			}
		}
	}

	_r_listview_setgroup (hwnd, listview_id, 0, app.LocaleString (group1_title, total_count ? _r_fmt (L" (%d/%d)", group1_count, total_count).GetString () : nullptr), 0, 0);
	_r_listview_setgroup (hwnd, listview_id, 1, app.LocaleString (group2_title, total_count ? _r_fmt (L" (%d/%d)", group2_count, total_count).GetString () : nullptr), 0, 0);
	_r_listview_setgroup (hwnd, listview_id, 2, app.LocaleString (group3_title, total_count ? _r_fmt (L" (%d/%d)", group3_count, total_count).GetString () : nullptr), 0, 0);
}

void _app_refreshstatus (HWND hwnd, INT listview_id)
{
	PITEM_STATUS pstatus = (PITEM_STATUS)_r_mem_allocex (sizeof (ITEM_STATUS), HEAP_ZERO_MEMORY);

	if (pstatus)
		_app_getcount (pstatus);

	const HWND hstatus = GetDlgItem (hwnd, IDC_STATUSBAR);
	const HDC hdc = GetDC (hstatus);

	// item count
	if (hdc)
	{
		SelectObject (hdc, (HFONT)SendMessage (hstatus, WM_GETFONT, 0, 0)); // fix

		const INT parts_count = 3;
		const INT spacing = _r_dc_getdpi (hwnd, 12);

		rstring text[parts_count];
		INT parts[parts_count] = {0};
		LONG size[parts_count] = {0};
		LONG lay = 0;

		for (INT i = 0; i < parts_count; i++)
		{
			switch (i)
			{
				case 1:
				{
					if (pstatus)
						text[i].Format (L"%s: %" PR_SIZE_T, app.LocaleString (IDS_STATUS_UNUSED_APPS, nullptr).GetString (), pstatus->apps_unused_count);

					break;
				}

				case 2:
				{
					if (pstatus)
						text[i].Format (L"%s: %" PR_SIZE_T, app.LocaleString (IDS_STATUS_TIMER_APPS, nullptr).GetString (), pstatus->apps_timer_count);

					break;
				}
			}

			if (i)
			{
				size[i] = _r_dc_fontwidth (hdc, text[i], text[i].GetLength ()) + spacing;
				lay += size[i];
			}
		}

		RECT rc_client = {0};
		GetClientRect (hstatus, &rc_client);

		parts[0] = _R_RECT_WIDTH (&rc_client) - lay - _r_dc_getsystemmetrics (hwnd, SM_CXVSCROLL) - (_r_dc_getsystemmetrics (hwnd, SM_CXBORDER) * 2);
		parts[1] = parts[0] + size[1];
		parts[2] = parts[1] + size[2];

		SendMessage (hstatus, SB_SETPARTS, parts_count, (LPARAM)parts);

		for (INT i = 1; i < parts_count; i++)
			_r_status_settext (hwnd, IDC_STATUSBAR, i, text[i]);

		ReleaseDC (hstatus, hdc);
	}

	// group information
	if (listview_id)
	{
		if (listview_id == INVALID_INT)
			listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

		_app_refreshgroups (hwnd, listview_id);
	}

	if (pstatus)
		_r_mem_free (pstatus);
}

INT _app_getposition (HWND hwnd, INT listview_id, LPARAM lparam)
{
	LVFINDINFO lvfi = {0};

	lvfi.flags = LVFI_PARAM;
	lvfi.lParam = lparam;

	return (INT)SendDlgItemMessage (hwnd, listview_id, LVM_FINDITEM, (WPARAM)INVALID_INT, (LPARAM)&lvfi);
}

void _app_showitem (HWND hwnd, INT listview_id, INT item, INT scroll_pos)
{
	const HWND hlistview = GetDlgItem (hwnd, listview_id);

	if (!hlistview)
		return;

	_app_settab_id (hwnd, listview_id);

	const INT total_count = _r_listview_getitemcount (hwnd, listview_id);

	if (!total_count)
		return;

	if (item != INVALID_INT)
	{
		item = std::clamp (item, 0, total_count - 1);

		PostMessage (hlistview, LVM_ENSUREVISIBLE, (WPARAM)item, TRUE); // ensure item visible

		ListView_SetItemState (hlistview, INVALID_INT, 0, LVIS_SELECTED | LVIS_FOCUSED); // deselect all
		ListView_SetItemState (hlistview, (WPARAM)item, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED); // select item
	}

	if (scroll_pos > 0)
		PostMessage (hlistview, LVM_SCROLL, 0, (LPARAM)scroll_pos); // restore scroll position
}

LONG _app_nmcustdraw_listview (LPNMLVCUSTOMDRAW lpnmlv)
{
	if (!app.ConfigGet (L"IsEnableHighlighting", true).AsBool ())
		return CDRF_DODEFAULT;

	switch (lpnmlv->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
		{
			return CDRF_NOTIFYITEMDRAW;
		}

		case CDDS_ITEMPREPAINT:
		{
			const INT view_type = (INT)SendMessage (lpnmlv->nmcd.hdr.hwndFrom, LVM_GETVIEW, 0, 0);
			const bool is_tableview = (view_type == LV_VIEW_DETAILS || view_type == LV_VIEW_SMALLICON || view_type == LV_VIEW_TILE);

			const INT listview_id = static_cast<INT>(lpnmlv->nmcd.hdr.idFrom);

			if ((listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP) || listview_id == IDC_RULE_APPS_ID || listview_id == IDC_NETWORK || listview_id == IDC_LOG)
			{
				size_t app_hash;

				if (listview_id == IDC_NETWORK)
				{
					app_hash = _app_getnetworkapp (lpnmlv->nmcd.lItemlParam); // initialize
				}
				else if (listview_id == IDC_LOG)
				{
					app_hash = _app_getlogapp (lpnmlv->nmcd.lItemlParam); // initialize
				}
				else
				{
					app_hash = lpnmlv->nmcd.lItemlParam;
				}

				if (app_hash)
				{
					const COLORREF new_clr = _app_getappcolor (listview_id, app_hash);

					if (new_clr)
					{
						lpnmlv->clrText = _r_dc_getcolorbrightness (new_clr);
						lpnmlv->clrTextBk = new_clr;

						if (is_tableview)
							_r_dc_fillrect (lpnmlv->nmcd.hdc, &lpnmlv->nmcd.rc, new_clr);

						return CDRF_NEWFONT;
					}
				}
			}
			else if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM)
			{
				const size_t rule_idx = lpnmlv->nmcd.lItemlParam;

				COLORREF new_clr = _app_getrulecolor (listview_id, rule_idx);

				if (new_clr)
				{
					lpnmlv->clrText = _r_dc_getcolorbrightness (new_clr);
					lpnmlv->clrTextBk = new_clr;

					if (is_tableview)
						_r_dc_fillrect (lpnmlv->nmcd.hdc, &lpnmlv->nmcd.rc, new_clr);

					return CDRF_NEWFONT;
				}

			}
			else if (listview_id == IDC_COLORS)
			{
				const PITEM_COLOR ptr_clr = (PITEM_COLOR)lpnmlv->nmcd.lItemlParam;

				if (ptr_clr)
				{
					const COLORREF new_clr = ptr_clr->new_clr ? ptr_clr->new_clr : ptr_clr->default_clr;

					if (new_clr)
					{
						lpnmlv->clrText = _r_dc_getcolorbrightness (new_clr);
						lpnmlv->clrTextBk = new_clr;

						if (is_tableview)
							_r_dc_fillrect (lpnmlv->nmcd.hdc, &lpnmlv->nmcd.rc, lpnmlv->clrTextBk);

						return CDRF_NEWFONT;
					}
				}
			}

			break;
		}
	}

	return CDRF_DODEFAULT;
}

LONG _app_nmcustdraw_toolbar (LPNMLVCUSTOMDRAW lpnmlv)
{
	switch (lpnmlv->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
		{
			return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
		}

		case CDDS_ITEMPREPAINT:
		{
			TBBUTTONINFO tbi = {0};

			tbi.cbSize = sizeof (tbi);
			tbi.dwMask = TBIF_STYLE | TBIF_STATE | TBIF_IMAGE;

			if ((INT)SendMessage (lpnmlv->nmcd.hdr.hwndFrom, TB_GETBUTTONINFO, (WPARAM)lpnmlv->nmcd.dwItemSpec, (LPARAM)&tbi) == INVALID_INT)
				break;

			if ((tbi.fsState & TBSTATE_ENABLED) == 0)
			{
				INT icon_size_x = 0;
				INT icon_size_y = 0;

				SelectObject (lpnmlv->nmcd.hdc, (HFONT)SendMessage (lpnmlv->nmcd.hdr.hwndFrom, WM_GETFONT, 0, 0)); // fix

				SetBkMode (lpnmlv->nmcd.hdc, TRANSPARENT);
				SetTextColor (lpnmlv->nmcd.hdc, GetSysColor (COLOR_GRAYTEXT));

				if (tbi.iImage != I_IMAGENONE)
				{
					const HIMAGELIST himglist = (HIMAGELIST)SendMessage (lpnmlv->nmcd.hdr.hwndFrom, TB_GETIMAGELIST, 0, 0);

					if (himglist)
					{
						const DWORD padding = (DWORD)SendMessage (lpnmlv->nmcd.hdr.hwndFrom, TB_GETPADDING, 0, 0);
						const UINT rebar_height = (UINT)SendMessage (GetParent (lpnmlv->nmcd.hdr.hwndFrom), RB_GETBARHEIGHT, 0, 0);

						ImageList_GetIconSize (himglist, &icon_size_x, &icon_size_y);

						IMAGELISTDRAWPARAMS ildp = {0};

						ildp.cbSize = sizeof (ildp);
						ildp.himl = himglist;
						ildp.hdcDst = lpnmlv->nmcd.hdc;
						ildp.i = tbi.iImage;
						ildp.x = lpnmlv->nmcd.rc.left + (LOWORD (padding) / 2);
						ildp.y = (rebar_height / 2) - (icon_size_y / 2);
						ildp.fState = ILS_SATURATE; // grayscale
						ildp.fStyle = ILD_NORMAL | ILD_ASYNC;

						ImageList_DrawIndirect (&ildp);
					}
				}

				if ((tbi.fsStyle & BTNS_SHOWTEXT) == BTNS_SHOWTEXT)
				{
					WCHAR text[MAX_PATH] = {0};
					SendMessage (lpnmlv->nmcd.hdr.hwndFrom, TB_GETBUTTONTEXT, (WPARAM)lpnmlv->nmcd.dwItemSpec, (LPARAM)text);

					if (tbi.iImage != I_IMAGENONE)
						lpnmlv->nmcd.rc.left += icon_size_x;

					DrawTextEx (lpnmlv->nmcd.hdc, text, (INT)_r_str_length (text), &lpnmlv->nmcd.rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_HIDEPREFIX, nullptr);
				}

				return CDRF_SKIPDEFAULT;
			}

			break;
		}
	}

	return CDRF_DODEFAULT;
}
