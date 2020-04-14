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

	_r_status_settext (hwnd, IDC_STATUSBAR, 0, app.LocaleString (_wfp_isfiltersinstalled () ? IDS_STATUS_FILTERS_ACTIVE : IDS_STATUS_FILTERS_INACTIVE, nullptr));
}

void _app_setinterfacestate (HWND hwnd)
{
	const bool is_filtersinstalled = _wfp_isfiltersinstalled ();
	const INT icon_id = is_filtersinstalled ? IDI_ACTIVE : IDI_INACTIVE;

	const HICON hico_sm = app.GetSharedImage (app.GetHINSTANCE (), icon_id, _r_dc_getsystemmetrics (hwnd, SM_CXSMICON));
	const HICON hico_big = app.GetSharedImage (app.GetHINSTANCE (), icon_id, _r_dc_getsystemmetrics (hwnd, SM_CXICON));

	SendMessage (hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hico_sm);
	SendMessage (hwnd, WM_SETICON, ICON_BIG, (LPARAM)hico_big);

	//SendDlgItemMessage (hwnd, IDC_STATUSBAR, SB_SETICON, 0, (LPARAM)hico_sm);

	if (!_wfp_isfiltersapplying ())
		_r_status_settext (hwnd, IDC_STATUSBAR, 0, app.LocaleString (is_filtersinstalled ? IDS_STATUS_FILTERS_ACTIVE : IDS_STATUS_FILTERS_INACTIVE, nullptr));

	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_START, app.LocaleString (is_filtersinstalled ? IDS_TRAY_STOP : IDS_TRAY_START, nullptr), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, 0, is_filtersinstalled ? 1 : 0);

	_r_tray_setinfo (hwnd, UID, hico_sm, APP_NAME);
}

void _app_listviewresize (HWND hwnd, INT listview_id, bool is_forced)
{
	if (!listview_id || (!is_forced && !app.ConfigGet (L"AutoSizeColumns", true).AsBool ()))
		return;

	const INT column_count = _r_listview_getcolumncount (hwnd, listview_id);
	const INT item_count = _r_listview_getitemcount (hwnd, listview_id);

	if (!column_count)
		return;

	const HWND hlistview = GetDlgItem (hwnd, listview_id);
	const HWND hheader = (HWND)SendMessage (hlistview, LVM_GETHEADER, 0, 0);

	RECT rc_client = {0};
	GetClientRect (hlistview, &rc_client);

	const INT total_width = _R_RECT_WIDTH (&rc_client);
	const INT spacing = _r_dc_getsystemmetrics (hwnd, SM_CXSMICON);

	const bool is_tableview = (SendMessage (hlistview, LVM_GETVIEW, 0, 0) == LV_VIEW_DETAILS);

	// get device context and fix font set
	const HDC hdc_listview = GetDC (hlistview);
	const HDC hdc_header = GetDC (hheader);

	if (hdc_listview)
		SelectObject (hdc_listview, (HFONT)SendMessage (hlistview, WM_GETFONT, 0, 0)); // fix

	if (hdc_header)
		SelectObject (hdc_header, (HFONT)SendMessage (hheader, WM_GETFONT, 0, 0)); // fix

	const INT column_max_width = _r_dc_getdpi (hwnd, 120);
	INT column_min_width;

	INT column_width;
	INT calculated_width = 0;

	for (INT i = column_count - 1; i != INVALID_INT; i--)
	{
		// get column text width
		{
			const rstring column_text = _r_listview_getcolumntext (hwnd, listview_id, i);
			column_width = _r_dc_fontwidth (hdc_header, column_text, column_text.GetLength ()) + spacing;
			column_min_width = column_width;
		}

		if (i == 0)
		{
			column_width = std::clamp (total_width - calculated_width, (std::min) (column_min_width, total_width), total_width);
		}
		else
		{
			// calculate max width of listview subitems (only for details view)
			if (is_tableview)
			{
				for (INT j = 0; j < item_count; j++)
				{
					const rstring item_text = _r_listview_getitemtext (hwnd, listview_id, j, i);
					const INT text_width = _r_dc_fontwidth (hdc_listview, item_text, item_text.GetLength ()) + spacing;

					if (text_width > column_width)
						column_width = text_width;
				}
			}

			column_width = std::clamp (column_width, column_min_width, (std::max) (column_min_width, column_max_width));
			calculated_width += column_width;
		}

		_r_listview_setcolumn (hwnd, listview_id, i, nullptr, column_width);
	}

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
		if (icons_size == SHIL_SYSSMALL)
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

bool _app_listviewinitfont (HWND hwnd, PLOGFONT plf)
{
	const rstring buffer = app.ConfigGet (L"Font", UI_FONT_DEFAULT);

	if (!buffer.IsEmpty ())
	{
		rstringvec rvc;
		_r_str_split (buffer, buffer.GetLength (), L';', rvc);

		for (size_t i = 0; i < rvc.size (); i++)
		{
			rstring& rlink = rvc.at (i);

			_r_str_trim (rlink, DIVIDER_TRIM);

			if (rlink.IsEmpty ())
				continue;

			if (i == 0)
				_r_str_copy (plf->lfFaceName, LF_FACESIZE, rlink);

			else if (i == 1)
				plf->lfHeight = _r_dc_fontsizetoheight (hwnd, rlink.AsInt ());

			else if (i == 2)
				plf->lfWeight = rlink.AsInt ();

			else
				break;
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

	return true;
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
			time_t timestamp1 = 0;
			time_t timestamp2 = 0;

			_app_getappinfo (lparam1, InfoTimestamp, &timestamp1, sizeof (timestamp1));
			_app_getappinfo (lparam2, InfoTimestamp, &timestamp2, sizeof (timestamp2));

			if (timestamp1 < timestamp2)
				result = -1;

			else if (timestamp1 > timestamp2)
				result = 1;
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
	if (!listview_id)
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

	SendDlgItemMessage (hwnd, listview_id, LVM_SORTITEMS, (WPARAM)GetDlgItem (hwnd, listview_id), (LPARAM)&_app_listviewcompare_callback);
}

INT _app_getposition (HWND hwnd, INT listview_id, LPARAM lparam)
{
	LVFINDINFO lvfi = {0};

	lvfi.flags = LVFI_PARAM;
	lvfi.lParam = lparam;

	INT pos = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_FINDITEM, (WPARAM)INVALID_INT, (LPARAM)&lvfi);

	return pos;
}

void _app_showitem (HWND hwnd, INT listview_id, INT item, INT scroll_pos)
{
	if (!listview_id)
		return;

	_app_settab_id (hwnd, listview_id);

	const INT total_count = _r_listview_getitemcount (hwnd, listview_id);

	if (!total_count)
		return;

	const HWND hlistview = GetDlgItem (hwnd, listview_id);

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
			const bool is_tableview = view_type == LV_VIEW_DETAILS || view_type == LV_VIEW_SMALLICON || view_type == LV_VIEW_TILE;

			const INT listview_id = static_cast<INT>(lpnmlv->nmcd.hdr.idFrom);

			if ((listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP) || listview_id == IDC_RULE_APPS || listview_id == IDC_NETWORK)
			{
				size_t app_hash;

				if (listview_id == IDC_NETWORK)
				{
					app_hash = _app_getnetworkapp (lpnmlv->nmcd.lItemlParam); // initialize
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
				PR_OBJECT ptr_object = _r_obj_reference (colors.at (lpnmlv->nmcd.lItemlParam));

				if (ptr_object)
				{
					const PITEM_COLOR ptr_clr = (PITEM_COLOR)ptr_object->pdata;

					if (ptr_clr)
					{
						const COLORREF new_clr = ptr_clr->new_clr ? ptr_clr->new_clr : ptr_clr->default_clr;

						if (new_clr)
						{
							lpnmlv->clrText = _r_dc_getcolorbrightness (new_clr);
							lpnmlv->clrTextBk = new_clr;

							if (is_tableview)
								_r_dc_fillrect (lpnmlv->nmcd.hdc, &lpnmlv->nmcd.rc, lpnmlv->clrTextBk);

							_r_obj_dereference (ptr_object);
							return CDRF_NEWFONT;
						}
					}

					_r_obj_dereference (ptr_object);
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
