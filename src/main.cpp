// simplewall
// Copyright (c) 2016-2019 Henry++

#include "global.hpp"

rapp app (APP_NAME, APP_NAME_SHORT, APP_VERSION, APP_COPYRIGHT);

STATIC_DATA config;

FWPM_SESSION session;

OBJECTS_MAP apps;
OBJECTS_MAP apps_helper;
OBJECTS_VEC rules_arr;
OBJECTS_MAP rules_config;
OBJECTS_MAP network_map;

OBJECTS_MAP cache_arpa;
OBJECTS_MAP cache_signatures;
OBJECTS_MAP cache_versions;
OBJECTS_MAP cache_dns;
OBJECTS_MAP cache_hosts;
TYPES_MAP cache_types;

THREADS_VEC threads_pool;

OBJECTS_VEC colors;
std::vector<time_t> timers;

GUIDS_VEC filter_ids;

ITEM_LIST_HEAD log_stack;

_R_FASTLOCK lock_access;
_R_FASTLOCK lock_apply;
_R_FASTLOCK lock_cache;
_R_FASTLOCK lock_checkbox;
_R_FASTLOCK lock_logbusy;
_R_FASTLOCK lock_logthread;
_R_FASTLOCK lock_threadpool;
_R_FASTLOCK lock_transaction;
_R_FASTLOCK lock_writelog;

EXTERN_C const IID IID_IImageList2;

const UINT WM_FINDMSGSTRING = RegisterWindowMessage (FINDMSGSTRING);

void _app_listviewresize (HWND hwnd, INT listview_id, bool is_forced = false)
{
	if (!listview_id || (!is_forced && !app.ConfigGet (L"AutoSizeColumns", true).AsBool ()))
		return;

	RECT rect = {0};
	GetClientRect (GetDlgItem (hwnd, listview_id), &rect);

	const INT total_width = _R_RECT_WIDTH (&rect);
	const INT spacing = _r_dc_getdpi (_R_SIZE_ICON16);

	const HWND hlistview = GetDlgItem (hwnd, listview_id);
	const HWND hheader = (HWND)SendMessage (hlistview, LVM_GETHEADER, 0, 0);

	const INT column_count = _r_listview_getcolumncount (hwnd, listview_id);
	const INT item_count = _r_listview_getitemcount (hwnd, listview_id);

	const bool is_tableview = (SendMessage (hlistview, LVM_GETVIEW, 0, 0) == LV_VIEW_DETAILS);

	const HDC hdc_listview = GetDC (hlistview);
	const HDC hdc_header = GetDC (hheader);

	SelectObject (hdc_listview, (HFONT)SendMessage (hlistview, WM_GETFONT, 0, 0)); // fix
	SelectObject (hdc_header, (HFONT)SendMessage (hheader, WM_GETFONT, 0, 0)); // fix

	if (column_count)
	{
		const INT column_max_width = _r_dc_getdpi (120);
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
				column_width = std::clamp (total_width - calculated_width, column_min_width, total_width);
			}
			else
			{
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

				column_width = std::clamp (column_width, column_min_width, column_max_width);
				calculated_width += column_width;
			}

			_r_listview_setcolumn (hwnd, listview_id, i, nullptr, column_width);
		}
	}

	ReleaseDC (hlistview, hdc_listview);
	ReleaseDC (hheader, hdc_header);
}

void _app_listviewsetview (HWND hwnd, INT listview_id)
{
	const bool is_mainview = (listview_id != IDC_APPS_LV) && (listview_id != IDC_NETWORK);
	const bool is_tableview = !is_mainview || app.ConfigGet (L"IsTableView", true).AsBool ();

	const INT icons_size = (listview_id != IDC_APPS_LV) ? std::clamp (app.ConfigGet (L"IconSize", SHIL_SYSSMALL).AsInt (), SHIL_LARGE, SHIL_LAST) : SHIL_SYSSMALL;
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

	SendDlgItemMessage (hwnd, listview_id, LVM_SETVIEW, is_tableview ? LV_VIEW_DETAILS : LV_VIEW_ICON, 0);
	SendDlgItemMessage (hwnd, listview_id, LVM_SCROLL, 0, GetScrollPos (GetDlgItem (hwnd, listview_id), SB_VERT)); // HACK!!!
}

bool _app_listviewinitfont (PLOGFONT plf)
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
				plf->lfHeight = _r_dc_fontsizetoheight (rlink.AsInt ());

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

		_app_listviewinitfont (&lf);

		config.hfont = CreateFontIndirect (&lf);
	}

	if (config.hfont)
		SendDlgItemMessage (hwnd, listview_id, WM_SETFONT, (WPARAM)config.hfont, TRUE);
}

COLORREF _app_getcolorvalue (size_t color_hash)
{
	if (!color_hash)
		return 0;

	for (size_t i = 0; i < colors.size (); i++)
	{
		PR_OBJECT ptr_clr_object = _r_obj_reference (colors.at (i));

		if (!ptr_clr_object)
			continue;

		const PITEM_COLOR ptr_clr = (PITEM_COLOR)ptr_clr_object->pdata;

		if (ptr_clr && ptr_clr->clr_hash == color_hash)
		{
			const COLORREF result = ptr_clr->new_clr ? ptr_clr->new_clr : ptr_clr->default_clr;
			_r_obj_dereference (ptr_clr_object);

			return result;
		}

		_r_obj_dereference (ptr_clr_object);
	}

	return 0;
}

COLORREF _app_getcolor (INT listview_id, size_t app_hash)
{
	rstring color_value;

	PR_OBJECT ptr_app_object = _app_getappitem (app_hash);

	if (!ptr_app_object)
		return 0;

	const bool is_appslist = (listview_id == IDC_APPS_LV);
	const bool is_networkslist = (listview_id == IDC_NETWORK);

	PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

	if (ptr_app)
	{
		if (app.ConfigGet (L"IsHighlightTimer", true, L"colors").AsBool () && _app_istimeractive (ptr_app))
			color_value = L"ColorTimer";

		else if (app.ConfigGet (L"IsHighlightInvalid", true, L"colors").AsBool () && !_app_isappexists (ptr_app))
			color_value = L"ColorInvalid";

		else if (!is_networkslist && !ptr_app->is_silent && app.ConfigGet (L"IsHighlightConnection", true, L"colors").AsBool () && _app_isapphaveconnection (app_hash))
			color_value = L"ColorConnection";

		else if (app.ConfigGet (L"IsHighlightSigned", true, L"colors").AsBool () && !ptr_app->is_silent && app.ConfigGet (L"IsCertificatesEnabled", false).AsBool () && ptr_app->is_signed)
			color_value = L"ColorSigned";

		else if (app.ConfigGet (L"IsHighlightSpecial", true, L"colors").AsBool () && _app_isapphaverule (app_hash))
			color_value = L"ColorSpecial";

		else if (!is_networkslist && !is_appslist && app.ConfigGet (L"IsHighlightSilent", true, L"colors").AsBool () && ptr_app->is_silent)
			color_value = L"ColorSilent";

		else if ((is_appslist || is_networkslist) && app.ConfigGet (L"IsHighlightService", true, L"colors").AsBool () && ptr_app->type == DataAppService)
			color_value = L"ColorService";

		else if ((is_appslist || is_networkslist) && app.ConfigGet (L"IsHighlightPackage", true, L"colors").AsBool () && ptr_app->type == DataAppUWP)
			color_value = L"ColorPackage";

		else if (app.ConfigGet (L"IsHighlightPico", true, L"colors").AsBool () && ptr_app->type == DataAppPico)
			color_value = L"ColorPico";

		else if (app.ConfigGet (L"IsHighlightSystem", true, L"colors").AsBool () && ptr_app->is_system)
			color_value = L"ColorSystem";
	}

	_r_obj_dereference (ptr_app_object);

	return _app_getcolorvalue (color_value.Hash ());
}

//bool _app_canihaveaccess ()
//{
//	bool result = false;
//
//	_r_fastlock_acquireshared (&lock_access);
//
//	PITEM_APP const ptr_app = _app_getappitem (config.my_hash);
//
//	if (ptr_app)
//	{
//		const EnumMode mode = (EnumMode)app.ConfigGet (L"Mode", ModeWhitelist).AsUint ();
//
//		result = (mode == ModeWhitelist && ptr_app->is_enabled || mode == ModeBlacklist && !ptr_app->is_enabled);
//	}
//
//	_r_fastlock_releaseshared (&lock_access);
//
//	return result;
//}

UINT WINAPI ApplyThread (LPVOID lparam)
{
	PINSTALL_CONTEXT pcontext = (PINSTALL_CONTEXT)lparam;

	if (pcontext)
	{
		// dropped packets logging (win7+)
		if (config.is_neteventset)
			_wfp_logunsubscribe ();

		if (pcontext->is_install)
		{
			if (_wfp_initialize (true))
				_wfp_installfilters ();
		}
		else
		{
			if (_wfp_initialize (false))
				_wfp_destroyfilters (_wfp_getenginehandle ());

			_wfp_uninitialize (true);
		}

		// dropped packets logging (win7+)
		if (config.is_neteventset)
			_wfp_logsubscribe ();

		_app_restoreinterfacestate (pcontext->hwnd, true);
		_app_setinterfacestate (pcontext->hwnd);

		_app_profile_save ();

		SetEvent (config.done_evt);

		SAFE_DELETE (pcontext);
	}

	_endthreadex (0);

	return ERROR_SUCCESS;
}

UINT WINAPI NetworkMonitorThread (LPVOID lparam)
{
	DWORD network_timeout = app.ConfigGet (L"NetworkTimeout", NETWORK_TIMEOUT).AsUlong ();

	if (network_timeout && network_timeout != INFINITE)
	{
		const HWND hwnd = (HWND)lparam;
		const INT network_listview_id = IDC_NETWORK;

		HASHER_MAP checker_map;

		network_timeout = std::clamp (network_timeout, 500UL, 60UL * 1000); // set allowed range

		while (true)
		{
			_app_generate_connections (network_map, checker_map);

			const bool is_highlighting_enabled = app.ConfigGet (L"IsEnableHighlighting", true).AsBool () && app.ConfigGet (L"IsHighlightConnection", true, L"colors").AsBool ();
			bool is_refresh = false;

			// add new connections into list
			for (auto &p : network_map)
			{
				if (checker_map.find (p.first) == checker_map.end () || !checker_map[p.first])
					continue;

				PR_OBJECT ptr_network_object = _r_obj_reference (p.second);

				if (!ptr_network_object)
					continue;

				SendDlgItemMessage (hwnd, network_listview_id, WM_SETREDRAW, FALSE, 0);

				PITEM_NETWORK ptr_network = (PITEM_NETWORK)ptr_network_object->pdata;

				if (ptr_network)
				{
					const INT item = _r_listview_getitemcount (hwnd, network_listview_id);

					_r_listview_additem (hwnd, network_listview_id, item, 0, _r_path_getfilename (ptr_network->path), ptr_network->icon_id, INVALID_INT, p.first);

					_r_listview_setitem (hwnd, network_listview_id, item, 1, ptr_network->local_fmt);
					_r_listview_setitem (hwnd, network_listview_id, item, 3, ptr_network->remote_fmt);
					_r_listview_setitem (hwnd, network_listview_id, item, 5, _app_getprotoname (ptr_network->protocol, ptr_network->af));
					_r_listview_setitem (hwnd, network_listview_id, item, 6, _app_getstatename (ptr_network->state));

					if (ptr_network->local_port)
						_r_listview_setitem (hwnd, network_listview_id, item, 2, _app_formatport (ptr_network->local_port, nullptr));

					if (ptr_network->remote_port)
						_r_listview_setitem (hwnd, network_listview_id, item, 4, _app_formatport (ptr_network->remote_port, nullptr));

					// redraw listview item
					if (is_highlighting_enabled)
					{
						INT app_listview_id = 0;

						if (_app_getappinfo (ptr_network->app_hash, InfoListviewId, &app_listview_id, sizeof (app_listview_id)))
						{
							if (IsWindowVisible (GetDlgItem (hwnd, app_listview_id)))
							{
								const INT item_pos = _app_getposition (hwnd, app_listview_id, ptr_network->app_hash);

								if (item_pos != INVALID_INT)
									_r_listview_redraw (hwnd, app_listview_id, item_pos, item_pos);
							}
						}
					}
				}

				_r_obj_dereference (ptr_network_object);

				SendDlgItemMessage (hwnd, network_listview_id, WM_SETREDRAW, TRUE, 0);

				is_refresh = true;
			}

			// remove closed connections from list
			const INT item_count = _r_listview_getitemcount (hwnd, network_listview_id);

			if (item_count)
			{
				for (INT i = item_count - 1; i != INVALID_INT; i--)
				{
					const size_t network_hash = _r_listview_getitemlparam (hwnd, network_listview_id, i);

					if (checker_map.find (network_hash) == checker_map.end ())
					{
						SendDlgItemMessage (hwnd, network_listview_id, WM_SETREDRAW, FALSE, 0);

						SendDlgItemMessage (hwnd, network_listview_id, LVM_DELETEITEM, (WPARAM)i, 0);

						PR_OBJECT ptr_network_object = _r_obj_reference (network_map[network_hash]);

						network_map.erase (network_hash);

						_r_obj_dereferenceex (ptr_network_object, 2);

						// redraw listview item
						if (is_highlighting_enabled)
						{
							const size_t app_hash = _app_getnetworkapp (network_hash);
							INT app_listview_id = 0;

							if (_app_getappinfo (app_hash, InfoListviewId, &app_listview_id, sizeof (app_listview_id)))
							{
								if (IsWindowVisible (GetDlgItem (hwnd, app_listview_id)))
								{
									const INT item_pos = _app_getposition (hwnd, app_listview_id, app_hash);

									if (item_pos != INVALID_INT)
										_r_listview_redraw (hwnd, app_listview_id, item_pos, item_pos);
								}
							}
						}

						SendDlgItemMessage (hwnd, network_listview_id, WM_SETREDRAW, TRUE, 0);
					}
				}
			}

			if (is_refresh)
			{
				const INT current_listview_id = _app_gettab_id (hwnd);

				if (current_listview_id != network_listview_id)
				{
					_r_listview_redraw (hwnd, current_listview_id);
				}
				else
				{
					_app_listviewresize (hwnd, network_listview_id);
					_app_listviewsort (hwnd, network_listview_id);
				}
			}

			WaitForSingleObjectEx (GetCurrentThread (), network_timeout, FALSE);
		}
	}

	_endthreadex (0);

	return ERROR_SUCCESS;
}

bool _app_changefilters (HWND hwnd, bool is_install, bool is_forced)
{
	if (_wfp_isfiltersapplying ())
		return false;

	const INT listview_id = _app_gettab_id (hwnd);

	_app_listviewsort (hwnd, listview_id);

	if (is_forced || _wfp_isfiltersinstalled ())
	{
		_r_fastlock_acquireshared (&lock_apply);

		_app_initinterfacestate (hwnd, true);

		_r_fastlock_acquireexclusive (&lock_threadpool);
		_app_freethreadpool (&threads_pool);
		_r_fastlock_releaseexclusive (&lock_threadpool);

		PINSTALL_CONTEXT pcontext = new INSTALL_CONTEXT;

		pcontext->hwnd = hwnd;
		pcontext->is_install = is_install;

		const HANDLE hthread = _r_createthread (&ApplyThread, (LPVOID)pcontext, true, THREAD_PRIORITY_HIGHEST);

		if (hthread)
		{
			_r_fastlock_acquireexclusive (&lock_threadpool);
			threads_pool.push_back (hthread);
			_r_fastlock_releaseexclusive (&lock_threadpool);

			ResumeThread (hthread);
		}
		else
		{
			SAFE_DELETE (pcontext);
		}

		_r_fastlock_releaseshared (&lock_apply);

		return hthread != nullptr;
	}

	_app_profile_save ();

	_r_listview_redraw (hwnd, listview_id);

	return false;
}

void addcolor (UINT locale_id, LPCWSTR config_name, bool is_enabled, LPCWSTR config_value, COLORREF default_clr)
{
	PITEM_COLOR ptr_clr = new ITEM_COLOR;

	if (config_name)
		_r_str_alloc (&ptr_clr->pcfg_name, INVALID_SIZE_T, config_name);

	if (config_value)
	{
		_r_str_alloc (&ptr_clr->pcfg_value, INVALID_SIZE_T, config_value);

		ptr_clr->clr_hash = _r_str_hash (config_value);
		ptr_clr->new_clr = app.ConfigGet (config_value, default_clr, L"colors").AsUlong ();
	}

	ptr_clr->is_enabled = is_enabled;
	ptr_clr->locale_id = locale_id;
	ptr_clr->default_clr = default_clr;

	colors.push_back (_r_obj_allocate (ptr_clr, &_app_dereferencecolor));
}

bool _app_installmessage (HWND hwnd, bool is_install)
{
	WCHAR flag[64] = {0};

	WCHAR button_text_1[128] = {0};
	WCHAR button_text_2[128] = {0};

	WCHAR main[256] = {0};

	INT result = 0;
	BOOL is_flagchecked = FALSE;

	TASKDIALOGCONFIG tdc = {0};
	TASKDIALOG_BUTTON td_buttons[2] = {0};

	tdc.cbSize = sizeof (tdc);
	tdc.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_NO_SET_FOREGROUND;
	tdc.hwndParent = hwnd;
	tdc.pszWindowTitle = APP_NAME;
	tdc.pszMainIcon = is_install ? TD_INFORMATION_ICON : TD_WARNING_ICON;
	//tdc.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
	tdc.pszContent = main;
	tdc.pszVerificationText = flag;
	tdc.pfCallback = &_r_msg_callback;
	tdc.lpCallbackData = MAKELONG (0, 1);
	tdc.cButtons = _countof (td_buttons);

	tdc.pButtons = td_buttons;

	_r_str_copy (button_text_1, _countof (button_text_1), app.LocaleString (is_install ? IDS_TRAY_START : IDS_TRAY_STOP, nullptr));
	_r_str_copy (button_text_2, _countof (button_text_2), app.LocaleString (IDS_CLOSE, nullptr));

	td_buttons[0].nButtonID = IDYES;
	td_buttons[0].pszButtonText = button_text_1;

	td_buttons[1].nButtonID = IDNO;
	td_buttons[1].pszButtonText = button_text_2;

	tdc.nDefaultButton = is_install ? IDYES : IDNO;

	if (is_install)
	{
		_r_str_copy (main, _countof (main), app.LocaleString (IDS_QUESTION_START, nullptr));
		_r_str_copy (flag, _countof (flag), app.LocaleString (IDS_DISABLEWINDOWSFIREWALL_CHK, nullptr));

		if (app.ConfigGet (L"IsDisableWindowsFirewallChecked", true).AsBool ())
			tdc.dwFlags |= TDF_VERIFICATION_FLAG_CHECKED;
	}
	else
	{
		_r_str_copy (main, _countof (main), app.LocaleString (IDS_QUESTION_STOP, nullptr));
		_r_str_copy (flag, _countof (flag), app.LocaleString (IDS_ENABLEWINDOWSFIREWALL_CHK, nullptr));

		if (app.ConfigGet (L"IsEnableWindowsFirewallChecked", true).AsBool ())
			tdc.dwFlags |= TDF_VERIFICATION_FLAG_CHECKED;
	}

	if (_r_msg_taskdialog (&tdc, &result, nullptr, &is_flagchecked))
	{
		if (result == IDYES)
		{
			if (is_install)
			{
				app.ConfigSet (L"IsDisableWindowsFirewallChecked", is_flagchecked ? true : false);

				if (is_flagchecked)
					_mps_changeconfig2 (false);
			}
			else
			{
				app.ConfigSet (L"IsEnableWindowsFirewallChecked", is_flagchecked ? true : false);

				if (is_flagchecked)
					_mps_changeconfig2 (true);
			}

			return true;
		}
	}

	return false;
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
			const bool is_tableview = (SendMessage (lpnmlv->nmcd.hdr.hwndFrom, LVM_GETVIEW, 0, 0) == LV_VIEW_DETAILS);
			const INT listview_id = PtrToInt ((void*)lpnmlv->nmcd.hdr.idFrom);

			if ((listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP) || listview_id == IDC_APPS_LV || listview_id == IDC_NETWORK)
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
					const COLORREF new_clr = _app_getcolor (listview_id, app_hash);

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
				PR_OBJECT ptr_rule_object = _app_getrulebyid (rule_idx);

				if (!ptr_rule_object)
					return CDRF_DODEFAULT;

				PITEM_RULE ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

				if (ptr_rule)
				{
					COLORREF new_clr = 0;

					if (ptr_rule->is_enabled && ptr_rule->is_haveerrors)
						new_clr = _app_getcolorvalue (_r_str_hash (L"ColorInvalid"));

					else if (ptr_rule->is_forservices || !ptr_rule->apps.empty ())
						new_clr = _app_getcolorvalue (_r_str_hash (L"ColorSpecial"));

					if (new_clr)
					{
						lpnmlv->clrText = _r_dc_getcolorbrightness (new_clr);
						lpnmlv->clrTextBk = new_clr;

						if (is_tableview)
							_r_dc_fillrect (lpnmlv->nmcd.hdc, &lpnmlv->nmcd.rc, new_clr);

						_r_obj_dereference (ptr_rule_object);
						return CDRF_NEWFONT;
					}
				}

				_r_obj_dereference (ptr_rule_object);
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

INT_PTR CALLBACK EditorProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static PR_OBJECT ptr_rule_object = nullptr;
	static PITEM_RULE ptr_rule = nullptr;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			ptr_rule_object = (PR_OBJECT)lparam;

			if (!ptr_rule_object)
			{
				EndDialog (hwnd, 0);
				return FALSE;
			}

			ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

			if (!ptr_rule)
			{
				EndDialog (hwnd, 0);
				return FALSE;
			}

			// configure window
			_r_wnd_center (hwnd, GetParent (hwnd));

			SendMessage (hwnd, WM_SETICON, ICON_SMALL, (LPARAM)app.GetSharedImage (app.GetHINSTANCE (), IDI_MAIN, _r_dc_getdpi (_R_SIZE_ICON16)));
			SendMessage (hwnd, WM_SETICON, ICON_BIG, (LPARAM)app.GetSharedImage (app.GetHINSTANCE (), IDI_MAIN, _r_dc_getdpi (_R_SIZE_ICON32)));

			// localize window
			SetWindowText (hwnd, app.LocaleString (IDS_EDITOR, (!_r_str_isempty (ptr_rule->pname) ? _r_fmt (L" - \"%s\"", ptr_rule->pname).GetString () : nullptr)));

			SetDlgItemText (hwnd, IDC_NAME, app.LocaleString (IDS_NAME, L":"));
			SetDlgItemText (hwnd, IDC_RULE_REMOTE, app.LocaleString (IDS_RULE, L" (" SZ_DIRECTION_REMOTE L"):"));
			SetDlgItemText (hwnd, IDC_RULE_LOCAL, app.LocaleString (IDS_RULE, L" (" SZ_DIRECTION_LOCAL L"):"));
			SetDlgItemText (hwnd, IDC_DIRECTION, app.LocaleString (IDS_DIRECTION, L":"));
			SetDlgItemText (hwnd, IDC_PROTOCOL, app.LocaleString (IDS_PROTOCOL, L":"));
			SetDlgItemText (hwnd, IDC_PORTVERSION, app.LocaleString (IDS_PORTVERSION, L":"));
			SetDlgItemText (hwnd, IDC_ACTION, app.LocaleString (IDS_ACTION, L":"));

			SetDlgItemText (hwnd, IDC_DISABLE_CHK, app.LocaleString (IDS_DISABLE_CHK, nullptr));
			SetDlgItemText (hwnd, IDC_ENABLE_CHK, app.LocaleString (IDS_ENABLE_CHK, nullptr));
			SetDlgItemText (hwnd, IDC_ENABLEFORAPPS_CHK, app.LocaleString (IDS_ENABLEFORAPPS_CHK, nullptr));

			SetDlgItemText (hwnd, IDC_WIKI, app.LocaleString (IDS_WIKI, nullptr));
			SetDlgItemText (hwnd, IDC_SAVE, app.LocaleString (IDS_SAVE, nullptr));
			SetDlgItemText (hwnd, IDC_CLOSE, app.LocaleString (IDS_CLOSE, nullptr));

			// configure listview
			_app_listviewsetfont (hwnd, IDC_APPS_LV, false);
			_app_listviewsetview (hwnd, IDC_APPS_LV);

			_r_listview_setstyle (hwnd, IDC_APPS_LV, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_CHECKBOXES);

			_r_listview_addcolumn (hwnd, IDC_APPS_LV, 0, app.LocaleString (IDS_NAME, nullptr), -95, LVCFMT_LEFT);

			// name
			if (!_r_str_isempty (ptr_rule->pname))
				SetDlgItemText (hwnd, IDC_NAME_EDIT, ptr_rule->pname);

			// rule_remote (remote)
			if (!_r_str_isempty (ptr_rule->prule_remote))
				SetDlgItemText (hwnd, IDC_RULE_REMOTE_EDIT, ptr_rule->prule_remote);

			// rule_remote (local)
			if (!_r_str_isempty (ptr_rule->prule_local))
				SetDlgItemText (hwnd, IDC_RULE_LOCAL_EDIT, ptr_rule->prule_local);

			// apps (apply to)
			{
				INT item = 0;

				_r_fastlock_acquireshared (&lock_access);

				for (auto &p : apps)
				{
					PR_OBJECT ptr_app_object = _r_obj_reference (p.second);

					if (!ptr_app_object)
						continue;

					PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

					if (!ptr_app)
					{
						_r_obj_dereference (ptr_app_object);
						continue;
					}

					// windows store apps (win8+)
					if (ptr_app->type == DataAppUWP && !_r_sys_validversion (6, 2))
					{
						_r_obj_dereference (ptr_app_object);
						continue;
					}

					if (ptr_rule->is_forservices && (p.first == config.ntoskrnl_hash || p.first == config.svchost_hash))
					{
						_r_obj_dereference (ptr_app_object);
						continue;
					}

					const bool is_enabled = !ptr_rule->apps.empty () && (ptr_rule->apps.find (p.first) != ptr_rule->apps.end ());

					_r_fastlock_acquireshared (&lock_checkbox);

					_r_listview_additem (hwnd, IDC_APPS_LV, item, 0, _r_path_getfilename (ptr_app->display_name), ptr_app->icon_id, INVALID_INT, p.first);
					_r_listview_setitemcheck (hwnd, IDC_APPS_LV, item, is_enabled);

					_r_fastlock_releaseshared (&lock_checkbox);

					item += 1;

					_r_obj_dereference (ptr_app_object);
				}

				_r_fastlock_releaseshared (&lock_access);

				// resize column
				_r_listview_setcolumn (hwnd, IDC_APPS_LV, 0, nullptr, -100);

				// sort column
				_app_listviewsort (hwnd, IDC_APPS_LV);
			}

			if (ptr_rule->type != DataRuleCustom)
			{
				_r_ctrl_enable (hwnd, IDC_ENABLEFORAPPS_CHK, false);
				_r_ctrl_enable (hwnd, IDC_APPS_LV, false);
			}

			const rstring text_any = app.LocaleString (IDS_ANY, nullptr);

			// direction
			SendDlgItemMessage (hwnd, IDC_DIRECTION_EDIT, CB_INSERTSTRING, 0, (LPARAM)app.LocaleString (IDS_DIRECTION_1, nullptr).GetString ());
			SendDlgItemMessage (hwnd, IDC_DIRECTION_EDIT, CB_INSERTSTRING, 1, (LPARAM)app.LocaleString (IDS_DIRECTION_2, nullptr).GetString ());
			SendDlgItemMessage (hwnd, IDC_DIRECTION_EDIT, CB_INSERTSTRING, 2, (LPARAM)text_any.GetString ());

			SendDlgItemMessage (hwnd, IDC_DIRECTION_EDIT, CB_SETCURSEL, (WPARAM)ptr_rule->dir, 0);

			// protocols
			{
				const UINT8 protos[] = {
					IPPROTO_ICMP,
					IPPROTO_IGMP,
					IPPROTO_IPV4,
					IPPROTO_TCP,
					IPPROTO_UDP,
					IPPROTO_RDP,
					IPPROTO_IPV6,
					IPPROTO_ICMPV6,
					IPPROTO_L2TP,
					IPPROTO_SCTP,
				};

				SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_INSERTSTRING, 0, (LPARAM)text_any.GetString ());
				SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_SETCURSEL, 0, 0);

				for (size_t i = 0; i < _countof (protos); i++)
				{
					const UINT8 proto = protos[i];

					SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_INSERTSTRING, i + 1, (LPARAM)_r_fmt (L"%s (%d)", _app_getprotoname (proto, AF_UNSPEC).GetString (), proto).GetString ());
					SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_SETITEMDATA, i + 1, (LPARAM)proto);

					if (ptr_rule->protocol == proto)
						SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_SETCURSEL, (WPARAM)i + 1, 0);
				}
			}

			// family (ports-only)
			SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_INSERTSTRING, 0, (LPARAM)text_any.GetString ());
			SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_SETITEMDATA, 0, (LPARAM)AF_UNSPEC);

			SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_INSERTSTRING, 1, (LPARAM)L"IPv4");
			SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_SETITEMDATA, 1, (LPARAM)AF_INET);

			SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_INSERTSTRING, 2, (LPARAM)L"IPv6");
			SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_SETITEMDATA, 2, (LPARAM)AF_INET6);

			if (ptr_rule->af == AF_UNSPEC)
				SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_SETCURSEL, (WPARAM)0, 0);

			else if (ptr_rule->af == AF_INET)
				SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_SETCURSEL, (WPARAM)1, 0);

			else if (ptr_rule->af == AF_INET6)
				SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_SETCURSEL, (WPARAM)2, 0);

			// action
			SendDlgItemMessage (hwnd, IDC_ACTION_EDIT, CB_INSERTSTRING, 0, (LPARAM)app.LocaleString (IDS_ACTION_ALLOW, nullptr).GetString ());
			SendDlgItemMessage (hwnd, IDC_ACTION_EDIT, CB_INSERTSTRING, 1, (LPARAM)app.LocaleString (IDS_ACTION_BLOCK, nullptr).GetString ());

			SendDlgItemMessage (hwnd, IDC_ACTION_EDIT, CB_SETCURSEL, (WPARAM)ptr_rule->is_block, 0);

			// state
			{
				INT ctrl_id = IDC_DISABLE_CHK;

				if (ptr_rule->is_enabled)
					ctrl_id = ptr_rule->apps.empty () ? IDC_ENABLE_CHK : IDC_ENABLEFORAPPS_CHK;

				CheckRadioButton (hwnd, IDC_DISABLE_CHK, IDC_ENABLEFORAPPS_CHK, ctrl_id);
			}

			// set limitation
			SendDlgItemMessage (hwnd, IDC_NAME_EDIT, EM_LIMITTEXT, RULE_NAME_CCH_MAX - 1, 0);
			SendDlgItemMessage (hwnd, IDC_RULE_REMOTE_EDIT, EM_LIMITTEXT, RULE_RULE_CCH_MAX - 1, 0);
			SendDlgItemMessage (hwnd, IDC_RULE_LOCAL_EDIT, EM_LIMITTEXT, RULE_RULE_CCH_MAX - 1, 0);

			// set read-only
			SendDlgItemMessage (hwnd, IDC_NAME_EDIT, EM_SETREADONLY, ptr_rule->is_readonly, 0);
			SendDlgItemMessage (hwnd, IDC_RULE_REMOTE_EDIT, EM_SETREADONLY, ptr_rule->is_readonly, 0);
			SendDlgItemMessage (hwnd, IDC_RULE_LOCAL_EDIT, EM_SETREADONLY, ptr_rule->is_readonly, 0);

			_r_ctrl_enable (hwnd, IDC_PORTVERSION_EDIT, !ptr_rule->is_readonly);
			_r_ctrl_enable (hwnd, IDC_PROTOCOL_EDIT, !ptr_rule->is_readonly);
			_r_ctrl_enable (hwnd, IDC_DIRECTION_EDIT, !ptr_rule->is_readonly);
			_r_ctrl_enable (hwnd, IDC_ACTION_EDIT, !ptr_rule->is_readonly);

			_r_wnd_addstyle (hwnd, IDC_WIKI, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (hwnd, IDC_SAVE, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (hwnd, IDC_CLOSE, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

			if (!ptr_rule->is_readonly)
				_r_ctrl_enable (hwnd, IDC_SAVE, (SendDlgItemMessage (hwnd, IDC_NAME_EDIT, WM_GETTEXTLENGTH, 0, 0) > 0) && ((SendDlgItemMessage (hwnd, IDC_RULE_REMOTE_EDIT, WM_GETTEXTLENGTH, 0, 0) > 0) || (SendDlgItemMessage (hwnd, IDC_RULE_LOCAL_EDIT, WM_GETTEXTLENGTH, 0, 0) > 0))); // enable apply button

			ptr_rule->is_haveerrors = false;

#ifndef _APP_NO_DARKTHEME
			_r_wnd_setdarktheme (hwnd);
#endif // _APP_NO_DARKTHEME

			break;
		}

#ifndef _APP_NO_DARKTHEME
		case WM_SYSCOLORCHANGE:
		{
			_r_wnd_setdarktheme (hwnd);
			break;
		}
#endif // _APP_NO_DARKTHEME

		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lparam;

			switch (nmlp->code)
			{
				case NM_CUSTOMDRAW:
				{
					LONG result = CDRF_DODEFAULT;

					if (_r_fastlock_tryacquireshared (&lock_access))
					{
						result = _app_nmcustdraw_listview ((LPNMLVCUSTOMDRAW)lparam);
						_r_fastlock_releaseshared (&lock_access);
					}

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, result);
					return result;
				}

				case NM_RCLICK:
				{
					LPNMITEMACTIVATE lpnmlv = (LPNMITEMACTIVATE)lparam;

					if (lpnmlv->iItem == INVALID_INT)
						break;

					const INT listview_id = PtrToInt ((void*)lpnmlv->hdr.idFrom);

					if (listview_id != IDC_APPS_LV)
						break;

					const HMENU hsubmenu = CreatePopupMenu ();

					AppendMenu (hsubmenu, MF_BYCOMMAND, IDM_CHECK, app.LocaleString (IDS_CHECK, nullptr));
					AppendMenu (hsubmenu, MF_BYCOMMAND, IDM_UNCHECK, app.LocaleString (IDS_UNCHECK, nullptr));

					POINT pt = {0};
					GetCursorPos (&pt);

					TrackPopupMenuEx (hsubmenu, TPM_RIGHTBUTTON | TPM_LEFTBUTTON, pt.x, pt.y, hwnd, nullptr);

					DestroyMenu (hsubmenu);

					break;
				}

				case LVN_COLUMNCLICK:
				{
					LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lparam;
					_app_listviewsort (hwnd, PtrToInt ((void*)lpnmlv->hdr.idFrom), lpnmlv->iSubItem, true);

					break;
				}

				case LVN_ITEMCHANGED:
				{
					LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lparam;

					if (IsWindowVisible (lpnmlv->hdr.hwndFrom) && (lpnmlv->uChanged & LVIF_STATE) && (lpnmlv->uNewState == 8192 || lpnmlv->uNewState == 4096) && lpnmlv->uNewState != lpnmlv->uOldState)
					{
						if (_r_fastlock_islocked (&lock_checkbox))
							break;

						const INT listview_id = PtrToInt ((void*)lpnmlv->hdr.idFrom);

						const bool is_havechecks = !!_r_listview_getitemcount (hwnd, listview_id, true);
						CheckRadioButton (hwnd, IDC_DISABLE_CHK, IDC_ENABLEFORAPPS_CHK, is_havechecks ? IDC_ENABLEFORAPPS_CHK : IDC_DISABLE_CHK);

						_app_listviewsort (hwnd, listview_id);
					}

					break;
				}

				case LVN_GETINFOTIP:
				{
					if (_r_fastlock_tryacquireshared (&lock_access))
					{
						LPNMLVGETINFOTIP lpnmlv = (LPNMLVGETINFOTIP)lparam;

						const INT listview_id = PtrToInt ((void*)lpnmlv->hdr.idFrom);

						const size_t idx = _r_listview_getitemlparam (hwnd, listview_id, lpnmlv->iItem);

						_r_str_copy (lpnmlv->pszText, lpnmlv->cchTextMax, _app_gettooltip (listview_id, idx));

						_r_fastlock_releaseshared (&lock_access);
					}

					break;
				}

				case LVN_GETEMPTYMARKUP:
				{
					NMLVEMPTYMARKUP* lpnmlv = (NMLVEMPTYMARKUP*)lparam;

					lpnmlv->dwFlags = EMF_CENTERED;
					_r_str_copy (lpnmlv->szMarkup, _countof (lpnmlv->szMarkup), app.LocaleString (IDS_STATUS_EMPTY, nullptr));

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
					return TRUE;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			if (HIWORD (wparam) == EN_CHANGE)
			{
				if (!ptr_rule->is_readonly)
					_r_ctrl_enable (hwnd, IDC_SAVE, (SendDlgItemMessage (hwnd, IDC_NAME_EDIT, WM_GETTEXTLENGTH, 0, 0) > 0) && ((SendDlgItemMessage (hwnd, IDC_RULE_REMOTE_EDIT, WM_GETTEXTLENGTH, 0, 0) > 0) || (SendDlgItemMessage (hwnd, IDC_RULE_LOCAL_EDIT, WM_GETTEXTLENGTH, 0, 0) > 0))); // enable apply button

				return FALSE;
			}

			switch (LOWORD (wparam))
			{
				case IDM_CHECK:
				case IDM_UNCHECK:
				{
					const bool new_val = (LOWORD (wparam) == IDM_CHECK) ? true : false;
					INT item = INVALID_INT;

					_r_fastlock_acquireshared (&lock_checkbox);

					while ((item = (INT)SendDlgItemMessage (hwnd, IDC_APPS_LV, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
						_r_listview_setitemcheck (hwnd, IDC_APPS_LV, item, new_val);

					_r_fastlock_releaseshared (&lock_checkbox);

					_app_listviewsort (hwnd, IDC_APPS_LV, 0);

					break;
				}

				case IDC_WIKI:
				{
					ShellExecute (hwnd, nullptr, WIKI_URL, nullptr, nullptr, SW_SHOWDEFAULT);
					break;
				}

				case IDOK: // process Enter key
				case IDC_SAVE:
				{
					// do not change read-only rules
					if (!ptr_rule->is_readonly)
					{
						if (!SendDlgItemMessage (hwnd, IDC_NAME_EDIT, WM_GETTEXTLENGTH, 0, 0) || (!SendDlgItemMessage (hwnd, IDC_RULE_REMOTE_EDIT, WM_GETTEXTLENGTH, 0, 0) && !SendDlgItemMessage (hwnd, IDC_RULE_LOCAL_EDIT, WM_GETTEXTLENGTH, 0, 0)))
							return FALSE;

						rstring rule_remote = _r_ctrl_gettext (hwnd, IDC_RULE_REMOTE_EDIT);
						_r_str_trim (rule_remote, DIVIDER_TRIM DIVIDER_RULE);

						rstring rule_local = _r_ctrl_gettext (hwnd, IDC_RULE_LOCAL_EDIT);
						_r_str_trim (rule_local, DIVIDER_TRIM DIVIDER_RULE);

						size_t rule_remote_length;
						size_t rule_local_length;

						// set correct remote rule
						{
							rstring rule_remote_fixed;

							rstringvec rvc;
							_r_str_split (rule_remote, rule_remote.GetLength (), DIVIDER_RULE[0], rvc);

							for (size_t i = 0; i < rvc.size (); i++)
							{
								rstring& rule_single = rvc.at (i);

								_r_str_trim (rule_single, DIVIDER_TRIM DIVIDER_RULE);

								if (rule_single.IsEmpty () || !_app_parserulestring (rule_single, nullptr))
								{
									_r_ctrl_showtip (hwnd, IDC_RULE_REMOTE_EDIT, TTI_ERROR, APP_NAME, _r_fmt (app.LocaleString (IDS_STATUS_SYNTAX_ERROR, nullptr), rule_single.GetString ()));
									_r_ctrl_enable (hwnd, IDC_SAVE, false);

									return FALSE;
								}

								rule_remote_fixed.AppendFormat (L"%s" DIVIDER_RULE, rule_single.GetString ());
							}

							_r_str_trim (rule_remote_fixed, L" " DIVIDER_TRIM DIVIDER_RULE);

							rule_remote = std::move (rule_remote_fixed);
							rule_remote_length = min (rule_remote.GetLength (), RULE_RULE_CCH_MAX);
						}

						// set correct local rule
						{
							rstring rule_local_fixed;

							rstringvec rvc;
							_r_str_split (rule_local, rule_local.GetLength (), DIVIDER_RULE[0], rvc);

							for (size_t i = 0; i < rvc.size (); i++)
							{
								rstring& rule_single = rvc.at (i);

								_r_str_trim (rule_single, L" " DIVIDER_TRIM DIVIDER_RULE);

								if (rule_single.IsEmpty () || !_app_parserulestring (rule_single, nullptr))
								{
									_r_ctrl_showtip (hwnd, IDC_RULE_LOCAL_EDIT, TTI_ERROR, APP_NAME, _r_fmt (app.LocaleString (IDS_STATUS_SYNTAX_ERROR, nullptr), rule_single.GetString ()));
									_r_ctrl_enable (hwnd, IDC_SAVE, false);

									return FALSE;
								}

								rule_local_fixed.AppendFormat (L"%s" DIVIDER_RULE, rule_single.GetString ());
							}

							_r_str_trim (rule_local_fixed, L" " DIVIDER_TRIM DIVIDER_RULE);

							rule_local = std::move (rule_local_fixed);
							rule_local_length = min (rule_local.GetLength (), RULE_RULE_CCH_MAX);
						}

						// set rule remote address
						_r_str_alloc (&ptr_rule->prule_remote, rule_remote_length, rule_remote);
						_r_str_alloc (&ptr_rule->prule_local, rule_local_length, rule_local);

						// set rule name
						{
							rstring name = _r_ctrl_gettext (hwnd, IDC_NAME_EDIT);
							_r_str_trim (name, DIVIDER_TRIM DIVIDER_RULE);

							if (!name.IsEmpty ())
							{
								const size_t name_length = min (name.GetLength (), RULE_NAME_CCH_MAX);

								_r_str_alloc (&ptr_rule->pname, name_length, name);
							}
						}

						ptr_rule->protocol = (UINT8)SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_GETITEMDATA, SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_GETCURSEL, 0, 0), 0);
						ptr_rule->af = (ADDRESS_FAMILY)SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_GETITEMDATA, SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_GETCURSEL, 0, 0), 0);

						ptr_rule->dir = (FWP_DIRECTION)SendDlgItemMessage (hwnd, IDC_DIRECTION_EDIT, CB_GETCURSEL, 0, 0);
						ptr_rule->is_block = !!SendDlgItemMessage (hwnd, IDC_ACTION_EDIT, CB_GETCURSEL, 0, 0);

						if (ptr_rule->type == DataRuleCustom)
							ptr_rule->weight = (ptr_rule->is_block ? FILTER_WEIGHT_CUSTOM_BLOCK : FILTER_WEIGHT_CUSTOM);
					}

					_app_ruleenable (ptr_rule, !!(IsDlgButtonChecked (hwnd, IDC_DISABLE_CHK) == BST_UNCHECKED));

					// save rule apps
					if (ptr_rule->type == DataRuleCustom)
					{
						ptr_rule->apps.clear ();

						const bool is_enable = (IsDlgButtonChecked (hwnd, IDC_ENABLE_CHK) != BST_CHECKED);

						for (INT i = 0; i < _r_listview_getitemcount (hwnd, IDC_APPS_LV); i++)
						{
							const size_t app_hash = _r_listview_getitemlparam (hwnd, IDC_APPS_LV, i);

							if (_app_isappfound (app_hash))
							{
								const bool is_apply = is_enable && _r_listview_isitemchecked (hwnd, IDC_APPS_LV, i);

								if (is_apply)
									ptr_rule->apps[app_hash] = true;
							}
						}
					}

					// apply filter
					{
						OBJECTS_VEC rules;
						rules.push_back (ptr_rule_object);

						_wfp_create4filters (_wfp_getenginehandle (), rules, __LINE__);

						// note: do not needed!
						//_app_dereferenceobjects (rules, &_app_dereferencerule);
					}

					EndDialog (hwnd, 1);

					break;
				}

				case IDCANCEL: // process Esc key
				case IDC_CLOSE:
				{
					EndDialog (hwnd, 0);
					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

void _app_config_apply (HWND hwnd, INT ctrl_id)
{
	bool new_val;

	switch (ctrl_id)
	{
		case IDC_RULE_BLOCKOUTBOUND:
		case IDM_RULE_BLOCKOUTBOUND:
		{
			new_val = !app.ConfigGet (L"BlockOutboundConnections", true).AsBool ();
			break;
		}

		case IDC_RULE_BLOCKINBOUND:
		case IDM_RULE_BLOCKINBOUND:
		{
			new_val = !app.ConfigGet (L"BlockInboundConnections", true).AsBool ();
			break;
		}

		case IDC_RULE_ALLOWLOOPBACK:
		case IDM_RULE_ALLOWLOOPBACK:
		{
			new_val = !app.ConfigGet (L"AllowLoopbackConnections", true).AsBool ();
			break;
		}

		case IDC_RULE_ALLOW6TO4:
		case IDM_RULE_ALLOW6TO4:
		{
			new_val = !app.ConfigGet (L"AllowIPv6", true).AsBool ();
			break;
		}

		case IDC_SECUREFILTERS_CHK:
		case IDM_SECUREFILTERS_CHK:
		{
			new_val = !app.ConfigGet (L"IsSecureFilters", true).AsBool ();
			break;
		}

		case IDC_USESTEALTHMODE_CHK:
		case IDM_USESTEALTHMODE_CHK:
		{
			new_val = !app.ConfigGet (L"UseStealthMode", true).AsBool ();
			break;
		}

		case IDC_INSTALLBOOTTIMEFILTERS_CHK:
		case IDM_INSTALLBOOTTIMEFILTERS_CHK:
		{
			new_val = !app.ConfigGet (L"InstallBoottimeFilters", true).AsBool ();
			break;
		}

		case IDC_USENETWORKRESOLUTION_CHK:
		case IDM_USENETWORKRESOLUTION_CHK:
		{
			new_val = !app.ConfigGet (L"IsNetworkResolutionsEnabled", false).AsBool ();
			break;
		}

		case IDC_USECERTIFICATES_CHK:
		case IDM_USECERTIFICATES_CHK:
		{
			new_val = !app.ConfigGet (L"IsCertificatesEnabled", false).AsBool ();
			break;
		}

		case IDC_USEREFRESHDEVICES_CHK:
		case IDM_USEREFRESHDEVICES_CHK:
		{
			new_val = !app.ConfigGet (L"IsRefreshDevices", true).AsBool ();
			break;
		}

		default:
		{
			return;
		}
	}

	const HMENU hmenu = GetMenu (hwnd);

	switch (ctrl_id)
	{
		case IDC_RULE_BLOCKOUTBOUND:
		case IDM_RULE_BLOCKOUTBOUND:
		{
			app.ConfigSet (L"BlockOutboundConnections", new_val);
			CheckMenuItem (hmenu, IDM_RULE_BLOCKOUTBOUND, MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));

			break;
		}

		case IDC_RULE_BLOCKINBOUND:
		case IDM_RULE_BLOCKINBOUND:
		{
			app.ConfigSet (L"BlockInboundConnections", new_val);
			CheckMenuItem (hmenu, IDM_RULE_BLOCKINBOUND, MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));

			break;
		}

		case IDC_RULE_ALLOWLOOPBACK:
		case IDM_RULE_ALLOWLOOPBACK:
		{
			app.ConfigSet (L"AllowLoopbackConnections", new_val);
			CheckMenuItem (hmenu, IDM_RULE_ALLOWLOOPBACK, MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));

			break;
		}

		case IDC_RULE_ALLOW6TO4:
		case IDM_RULE_ALLOW6TO4:
		{
			app.ConfigSet (L"AllowIPv6", new_val);
			CheckMenuItem (hmenu, IDM_RULE_ALLOW6TO4, MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));

			break;
		}

		case IDC_USESTEALTHMODE_CHK:
		case IDM_USESTEALTHMODE_CHK:
		{
			app.ConfigSet (L"UseStealthMode", new_val);
			CheckMenuItem (hmenu, IDM_USESTEALTHMODE_CHK, MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));

			break;
		}

		case IDC_SECUREFILTERS_CHK:
		case IDM_SECUREFILTERS_CHK:
		{
			app.ConfigSet (L"IsSecureFilters", new_val);
			CheckMenuItem (hmenu, IDM_SECUREFILTERS_CHK, MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));

			if (new_val ? config.pacl_secure : config.pacl_default)
			{
				PACL& pacl = new_val ? config.pacl_secure : config.pacl_default;

				const HANDLE hengine = _wfp_getenginehandle ();

				GUIDS_VEC filter_all;

				if (_wfp_dumpfilters (hengine, &GUID_WfpProvider, &filter_all))
				{
					for (size_t i = 0; i < filter_all.size (); i++)
						_wfp_setfiltersecurity (hengine, filter_all.at (i), pacl, __LINE__);
				}

				// set security information
				if (config.padminsid)
				{
					FwpmProviderSetSecurityInfoByKey (hengine, &GUID_WfpProvider, OWNER_SECURITY_INFORMATION, (const SID*)config.padminsid, nullptr, nullptr, nullptr);
					FwpmSubLayerSetSecurityInfoByKey (hengine, &GUID_WfpSublayer, OWNER_SECURITY_INFORMATION, (const SID*)config.padminsid, nullptr, nullptr, nullptr);
				}

				if (pacl)
				{
					FwpmProviderSetSecurityInfoByKey (hengine, &GUID_WfpProvider, DACL_SECURITY_INFORMATION, nullptr, nullptr, pacl, nullptr);
					FwpmSubLayerSetSecurityInfoByKey (hengine, &GUID_WfpSublayer, DACL_SECURITY_INFORMATION, nullptr, nullptr, pacl, nullptr);
				}
			}

			break;
		}

		case IDC_INSTALLBOOTTIMEFILTERS_CHK:
		case IDM_INSTALLBOOTTIMEFILTERS_CHK:
		{
			app.ConfigSet (L"InstallBoottimeFilters", new_val);
			CheckMenuItem (hmenu, IDM_INSTALLBOOTTIMEFILTERS_CHK, MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));

			break;
		}

		case IDC_USENETWORKRESOLUTION_CHK:
		case IDM_USENETWORKRESOLUTION_CHK:
		{
			app.ConfigSet (L"IsNetworkResolutionsEnabled", new_val);
			CheckMenuItem (hmenu, IDM_USENETWORKRESOLUTION_CHK, MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));

			break;
		}

		case IDC_USECERTIFICATES_CHK:
		case IDM_USECERTIFICATES_CHK:
		{
			app.ConfigSet (L"IsCertificatesEnabled", new_val);
			CheckMenuItem (hmenu, IDM_USECERTIFICATES_CHK, MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));

			if (new_val)
			{
				_r_fastlock_acquireshared (&lock_access);

				for (auto &p : apps)
				{
					PR_OBJECT ptr_app_object = _r_obj_reference (p.second);

					if (!ptr_app_object)
						continue;

					PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

					if (ptr_app)
					{
						PR_OBJECT ptr_signature_object = _app_getsignatureinfo (p.first, ptr_app);

						if (ptr_signature_object)
							_r_obj_dereference (ptr_signature_object);
					}

					_r_obj_dereference (ptr_app_object);
				}

				_r_fastlock_releaseshared (&lock_access);
			}

			_r_listview_redraw (app.GetHWND (), _app_gettab_id (app.GetHWND ()));

			break;
		}

		case IDC_USEREFRESHDEVICES_CHK:
		case IDM_USEREFRESHDEVICES_CHK:
		{
			app.ConfigSet (L"IsRefreshDevices", new_val);
			CheckMenuItem (hmenu, IDM_USEREFRESHDEVICES_CHK, MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));

			break;
		}
	}

	switch (ctrl_id)
	{
		case IDC_SECUREFILTERS_CHK:
		case IDM_SECUREFILTERS_CHK:
		case IDC_USENETWORKRESOLUTION_CHK:
		case IDM_USENETWORKRESOLUTION_CHK:
		case IDC_USECERTIFICATES_CHK:
		case IDM_USECERTIFICATES_CHK:
		case IDC_USEREFRESHDEVICES_CHK:
		case IDM_USEREFRESHDEVICES_CHK:
		{
			return;
		}
	}

	_wfp_create2filters (_wfp_getenginehandle (), __LINE__);
}

INT_PTR CALLBACK SettingsProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static PAPP_SETTINGS_PAGE ptr_page = nullptr;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			ptr_page = (PAPP_SETTINGS_PAGE)lparam;

#ifndef _APP_NO_DARKTHEME
			_r_wnd_setdarktheme (hwnd);
#endif // _APP_NO_DARKTHEME

			break;
		}

		case RM_INITIALIZE:
		{
			const INT dialog_id = (INT)wparam;

			switch (dialog_id)
			{
				case IDD_SETTINGS_GENERAL:
				{
					CheckDlgButton (hwnd, IDC_ALWAYSONTOP_CHK, app.ConfigGet (L"AlwaysOnTop", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

#ifdef _APP_HAVE_AUTORUN
					CheckDlgButton (hwnd, IDC_LOADONSTARTUP_CHK, app.AutorunIsEnabled () ? BST_CHECKED : BST_UNCHECKED);
#endif // _APP_HAVE_AUTORUN

					CheckDlgButton (hwnd, IDC_STARTMINIMIZED_CHK, app.ConfigGet (L"IsStartMinimized", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

#ifdef _APP_HAVE_SKIPUAC
					CheckDlgButton (hwnd, IDC_SKIPUACWARNING_CHK, app.SkipUacIsEnabled () ? BST_CHECKED : BST_UNCHECKED);
#endif // _APP_HAVE_SKIPUAC

					CheckDlgButton (hwnd, IDC_CHECKUPDATES_CHK, app.ConfigGet (L"CheckUpdates", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

#if defined(_APP_BETA) || defined(_APP_BETA_RC)
					CheckDlgButton (hwnd, IDC_CHECKUPDATESBETA_CHK, BST_CHECKED);
					_r_ctrl_enable (hwnd, IDC_CHECKUPDATESBETA_CHK, false);
#else
					CheckDlgButton (hwnd, IDC_CHECKUPDATESBETA_CHK, app.ConfigGet (L"CheckUpdatesBeta", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					if (!app.ConfigGet (L"CheckUpdates", true).AsBool ())
						_r_ctrl_enable (hwnd, IDC_CHECKUPDATESBETA_CHK, false);
#endif

					app.LocaleEnum (hwnd, IDC_LANGUAGE, false, 0);

					break;
				}

				case IDD_SETTINGS_RULES:
				{
					CheckDlgButton (hwnd, IDC_RULE_BLOCKOUTBOUND, app.ConfigGet (L"BlockOutboundConnections", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_RULE_BLOCKINBOUND, app.ConfigGet (L"BlockInboundConnections", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_RULE_ALLOWLOOPBACK, app.ConfigGet (L"AllowLoopbackConnections", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_RULE_ALLOW6TO4, app.ConfigGet (L"AllowIPv6", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_SECUREFILTERS_CHK, app.ConfigGet (L"IsSecureFilters", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_USESTEALTHMODE_CHK, app.ConfigGet (L"UseStealthMode", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_INSTALLBOOTTIMEFILTERS_CHK, app.ConfigGet (L"InstallBoottimeFilters", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_USECERTIFICATES_CHK, app.ConfigGet (L"IsCertificatesEnabled", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_USENETWORKRESOLUTION_CHK, app.ConfigGet (L"IsNetworkResolutionsEnabled", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_USEREFRESHDEVICES_CHK, app.ConfigGet (L"IsRefreshDevices", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					const HWND htip = _r_ctrl_createtip (hwnd);

					_r_ctrl_settip (htip, hwnd, IDC_RULE_BLOCKOUTBOUND, LPSTR_TEXTCALLBACK);
					_r_ctrl_settip (htip, hwnd, IDC_RULE_BLOCKINBOUND, LPSTR_TEXTCALLBACK);
					_r_ctrl_settip (htip, hwnd, IDC_RULE_ALLOWLOOPBACK, LPSTR_TEXTCALLBACK);
					_r_ctrl_settip (htip, hwnd, IDC_RULE_ALLOW6TO4, LPSTR_TEXTCALLBACK);

					_r_ctrl_settip (htip, hwnd, IDC_USESTEALTHMODE_CHK, LPSTR_TEXTCALLBACK);
					_r_ctrl_settip (htip, hwnd, IDC_INSTALLBOOTTIMEFILTERS_CHK, LPSTR_TEXTCALLBACK);
					_r_ctrl_settip (htip, hwnd, IDC_SECUREFILTERS_CHK, LPSTR_TEXTCALLBACK);

					break;
				}

				case IDD_SETTINGS_BLOCKLIST:
				{
					for (INT i = IDC_BLOCKLIST_SPY_DISABLE; i <= IDC_BLOCKLIST_EXTRA_BLOCK; i++)
						CheckDlgButton (hwnd, i, BST_UNCHECKED); // HACK!!! reset button checkboxes!

					CheckDlgButton (hwnd, (IDC_BLOCKLIST_SPY_DISABLE + std::clamp (app.ConfigGet (L"BlocklistSpyState", 2).AsInt (), 0, 2)), BST_CHECKED);
					CheckDlgButton (hwnd, (IDC_BLOCKLIST_UPDATE_DISABLE + std::clamp (app.ConfigGet (L"BlocklistUpdateState", 0).AsInt (), 0, 2)), BST_CHECKED);
					CheckDlgButton (hwnd, (IDC_BLOCKLIST_EXTRA_DISABLE + std::clamp (app.ConfigGet (L"BlocklistExtraState", 0).AsInt (), 0, 2)), BST_CHECKED);

					break;
				}

				case IDD_SETTINGS_INTERFACE:
				{
					CheckDlgButton (hwnd, IDC_CONFIRMEXIT_CHK, app.ConfigGet (L"ConfirmExit2", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_CONFIRMEXITTIMER_CHK, app.ConfigGet (L"ConfirmExitTimer", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_CONFIRMLOGCLEAR_CHK, app.ConfigGet (L"ConfirmLogClear", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					// configure listview
					_r_listview_setstyle (hwnd, IDC_COLORS, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_CHECKBOXES);

					_app_listviewsetview (hwnd, IDC_COLORS);

					_r_listview_deleteallitems (hwnd, IDC_COLORS);
					_r_listview_deleteallcolumns (hwnd, IDC_COLORS);

					_r_listview_addcolumn (hwnd, IDC_COLORS, 0, nullptr, 0, LVCFMT_LEFT);

					INT item = 0;

					for (size_t i = 0; i < colors.size (); i++)
					{
						PR_OBJECT ptr_color_object = _r_obj_reference (colors.at (i));

						if (ptr_color_object)
						{
							PITEM_COLOR ptr_clr = (PITEM_COLOR)ptr_color_object->pdata;

							if (ptr_clr)
							{
								ptr_clr->new_clr = app.ConfigGet (ptr_clr->pcfg_value, ptr_clr->default_clr, L"colors").AsUlong ();

								_r_fastlock_acquireshared (&lock_checkbox);

								_r_listview_additem (hwnd, IDC_COLORS, item, 0, app.LocaleString (ptr_clr->locale_id, nullptr), config.icon_id, INVALID_INT, i);
								_r_listview_setitemcheck (hwnd, IDC_COLORS, item, app.ConfigGet (ptr_clr->pcfg_name, ptr_clr->is_enabled, L"colors").AsBool ());

								_r_fastlock_releaseshared (&lock_checkbox);

								item += 1;
							}

							_r_obj_dereference (ptr_color_object);
						}
					}

					_r_listview_setcolumn (hwnd, IDC_COLORS, 0, nullptr, -100);

					break;
				}

				case IDD_SETTINGS_NOTIFICATIONS:
				{
					CheckDlgButton (hwnd, IDC_ENABLENOTIFICATIONS_CHK, app.ConfigGet (L"IsNotificationsEnabled", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_NOTIFICATIONSOUND_CHK, app.ConfigGet (L"IsNotificationsSound", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_NOTIFICATIONONTRAY_CHK, app.ConfigGet (L"IsNotificationsOnTray", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_SETRANGE32, 0, _R_SECONDSCLOCK_DAY (7));
					SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_SETPOS32, 0, (LPARAM)app.ConfigGet (L"NotificationsTimeout", NOTIFY_TIMEOUT_DEFAULT).AsLonglong ());

					CheckDlgButton (hwnd, IDC_EXCLUDEBLOCKLIST_CHK, app.ConfigGet (L"IsExcludeBlocklist", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_EXCLUDECUSTOM_CHK, app.ConfigGet (L"IsExcludeCustomRules", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDC_ENABLENOTIFICATIONS_CHK, 0), 0);

					break;
				}

				case IDD_SETTINGS_LOGGING:
				{
					CheckDlgButton (hwnd, IDC_ENABLELOG_CHK, app.ConfigGet (L"IsLogEnabled", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					SetDlgItemText (hwnd, IDC_LOGPATH, app.ConfigGet (L"LogPath", LOG_PATH_DEFAULT));
					SetDlgItemText (hwnd, IDC_LOGVIEWER, app.ConfigGet (L"LogViewer", LOG_VIEWER_DEFAULT));

					UDACCEL ud = {0};
					ud.nInc = 64; // set step to 64kb

					SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_SETACCEL, 1, (LPARAM)&ud);
					SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_SETRANGE32, 64, 8192);
					SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_SETPOS32, 0, (LPARAM)app.ConfigGet (L"LogSizeLimitKb", LOG_SIZE_LIMIT_DEFAULT).AsUlong ());

					CheckDlgButton (hwnd, IDC_EXCLUDESTEALTH_CHK, app.ConfigGet (L"IsExcludeStealth", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, app.ConfigGet (L"IsExcludeClassifyAllow", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					if (!_r_sys_validversion (6, 2))
						_r_ctrl_enable (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, false);

					PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDC_ENABLELOG_CHK, 0), 0);

					break;
				}

				break;
			}

			break;
		}

		case RM_LOCALIZE:
		{
			const INT dialog_id = (INT)wparam;
			const rstring recommended = app.LocaleString (IDS_RECOMMENDED, nullptr);

			// localize titles
			SetDlgItemText (hwnd, IDC_TITLE_GENERAL, app.LocaleString (IDS_TITLE_GENERAL, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_LANGUAGE, app.LocaleString (IDS_TITLE_LANGUAGE, L": (Language)"));
			SetDlgItemText (hwnd, IDC_TITLE_BLOCKLIST_SPY, app.LocaleString (IDS_BLOCKLIST_SPY, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_BLOCKLIST_UPDATE, app.LocaleString (IDS_BLOCKLIST_UPDATE, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_BLOCKLIST_EXTRA, app.LocaleString (IDS_BLOCKLIST_EXTRA, L": (Skype, Bing, Live, Outlook, etc.)"));
			SetDlgItemText (hwnd, IDC_TITLE_CONNECTIONS, app.LocaleString (IDS_TAB_NETWORK, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_SECURITY, app.LocaleString (IDS_TITLE_SECURITY, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_CONFIRMATIONS, app.LocaleString (IDS_TITLE_CONFIRMATIONS, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_HIGHLIGHTING, app.LocaleString (IDS_TITLE_HIGHLIGHTING, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_NOTIFICATIONS, app.LocaleString (IDS_TITLE_NOTIFICATIONS, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_LOGGING, app.LocaleString (IDS_TITLE_LOGGING, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_EXCLUDE, app.LocaleString (IDS_TITLE_EXCLUDE, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_ADVANCED, app.LocaleString (IDS_TITLE_ADVANCED, L":"));

			switch (dialog_id)
			{
				case IDD_SETTINGS_GENERAL:
				{
					SetDlgItemText (hwnd, IDC_ALWAYSONTOP_CHK, app.LocaleString (IDS_ALWAYSONTOP_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_LOADONSTARTUP_CHK, app.LocaleString (IDS_LOADONSTARTUP_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_STARTMINIMIZED_CHK, app.LocaleString (IDS_STARTMINIMIZED_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_SKIPUACWARNING_CHK, app.LocaleString (IDS_SKIPUACWARNING_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_CHECKUPDATES_CHK, app.LocaleString (IDS_CHECKUPDATES_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_CHECKUPDATESBETA_CHK, app.LocaleString (IDS_CHECKUPDATESBETA_CHK, nullptr));

					SetDlgItemText (hwnd, IDC_LANGUAGE_HINT, app.LocaleString (IDS_LANGUAGE_HINT, nullptr));

					break;
				}

				case IDD_SETTINGS_RULES:
				{
					SetDlgItemText (hwnd, IDC_RULE_BLOCKOUTBOUND, app.LocaleString (IDS_RULE_BLOCKOUTBOUND, _r_fmt (L" (%s)", recommended.GetString ())));
					SetDlgItemText (hwnd, IDC_RULE_BLOCKINBOUND, app.LocaleString (IDS_RULE_BLOCKINBOUND, _r_fmt (L" (%s)", recommended.GetString ())));

					SetDlgItemText (hwnd, IDC_RULE_ALLOWLOOPBACK, app.LocaleString (IDS_RULE_ALLOWLOOPBACK, _r_fmt (L" (%s)", recommended.GetString ())));
					SetDlgItemText (hwnd, IDC_RULE_ALLOW6TO4, app.LocaleString (IDS_RULE_ALLOW6TO4, _r_fmt (L" (%s)", recommended.GetString ())));

					SetDlgItemText (hwnd, IDC_SECUREFILTERS_CHK, app.LocaleString (IDS_SECUREFILTERS_CHK, _r_fmt (L" (%s)", recommended.GetString ())));
					SetDlgItemText (hwnd, IDC_USESTEALTHMODE_CHK, app.LocaleString (IDS_USESTEALTHMODE_CHK, _r_fmt (L" (%s)", recommended.GetString ())));
					SetDlgItemText (hwnd, IDC_INSTALLBOOTTIMEFILTERS_CHK, app.LocaleString (IDS_INSTALLBOOTTIMEFILTERS_CHK, _r_fmt (L" (%s)", recommended.GetString ())));

					SetDlgItemText (hwnd, IDC_USENETWORKRESOLUTION_CHK, app.LocaleString (IDS_USENETWORKRESOLUTION_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_USECERTIFICATES_CHK, app.LocaleString (IDS_USECERTIFICATES_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_USEREFRESHDEVICES_CHK, app.LocaleString (IDS_USEREFRESHDEVICES_CHK, _r_fmt (L" (%s)", recommended.GetString ())));

					break;
				}

				case IDD_SETTINGS_BLOCKLIST:
				{
					SetDlgItemText (hwnd, IDC_BLOCKLIST_SPY_DISABLE, app.LocaleString (IDS_DISABLE, nullptr));
					SetDlgItemText (hwnd, IDC_BLOCKLIST_SPY_ALLOW, app.LocaleString (IDS_ACTION_ALLOW, nullptr));
					SetDlgItemText (hwnd, IDC_BLOCKLIST_SPY_BLOCK, app.LocaleString (IDS_ACTION_BLOCK, _r_fmt (L" (%s)", recommended.GetString ())));

					SetDlgItemText (hwnd, IDC_BLOCKLIST_UPDATE_DISABLE, app.LocaleString (IDS_DISABLE, _r_fmt (L" (%s)", recommended.GetString ())));
					SetDlgItemText (hwnd, IDC_BLOCKLIST_UPDATE_ALLOW, app.LocaleString (IDS_ACTION_ALLOW, nullptr));
					SetDlgItemText (hwnd, IDC_BLOCKLIST_UPDATE_BLOCK, app.LocaleString (IDS_ACTION_BLOCK, nullptr));

					SetDlgItemText (hwnd, IDC_BLOCKLIST_EXTRA_DISABLE, app.LocaleString (IDS_DISABLE, _r_fmt (L" (%s)", recommended.GetString ())));
					SetDlgItemText (hwnd, IDC_BLOCKLIST_EXTRA_ALLOW, app.LocaleString (IDS_ACTION_ALLOW, nullptr));
					SetDlgItemText (hwnd, IDC_BLOCKLIST_EXTRA_BLOCK, app.LocaleString (IDS_ACTION_BLOCK, nullptr));

					_r_ctrl_settext (hwnd, IDC_BLOCKLIST_INFO, L"(c) <a href=\"%s\">%s</a>", WINDOWSSPYBLOCKER_URL, WINDOWSSPYBLOCKER_URL);

					break;
				}

				case IDD_SETTINGS_INTERFACE:
				{
					SetDlgItemText (hwnd, IDC_CONFIRMEXIT_CHK, app.LocaleString (IDS_CONFIRMEXIT_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_CONFIRMEXITTIMER_CHK, app.LocaleString (IDS_CONFIRMEXITTIMER_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_CONFIRMLOGCLEAR_CHK, app.LocaleString (IDS_CONFIRMLOGCLEAR_CHK, nullptr));

					SetDlgItemText (hwnd, IDC_COLORS_HINT, app.LocaleString (IDS_COLORS_HINT, nullptr));

					_app_listviewsetfont (hwnd, IDC_COLORS, false);

					for (INT i = 0; i < _r_listview_getitemcount (hwnd, IDC_COLORS); i++)
					{
						const size_t clr_idx = _r_listview_getitemlparam (hwnd, IDC_COLORS, i);
						PR_OBJECT ptr_clr_object = _r_obj_reference (colors.at (clr_idx));

						if (ptr_clr_object)
						{
							PITEM_COLOR ptr_clr = (PITEM_COLOR)ptr_clr_object->pdata;

							if (ptr_clr)
							{
								_r_fastlock_acquireshared (&lock_checkbox);
								_r_listview_setitem (hwnd, IDC_COLORS, i, 0, app.LocaleString (ptr_clr->locale_id, nullptr));
								_r_fastlock_releaseshared (&lock_checkbox);
							}

							_r_obj_dereference (ptr_clr_object);
						}
					}

					break;
				}

				case IDD_SETTINGS_NOTIFICATIONS:
				case IDD_SETTINGS_LOGGING:
				{
					SetDlgItemText (hwnd, IDC_ENABLELOG_CHK, app.LocaleString (IDS_ENABLELOG_CHK, nullptr));

					SetDlgItemText (hwnd, IDC_LOGVIEWER_HINT, app.LocaleString (IDS_LOGVIEWER_HINT, L":"));
					SetDlgItemText (hwnd, IDC_LOGSIZELIMIT_HINT, app.LocaleString (IDS_LOGSIZELIMIT_HINT, nullptr));

					SetDlgItemText (hwnd, IDC_ENABLENOTIFICATIONS_CHK, app.LocaleString (IDS_ENABLENOTIFICATIONS_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_NOTIFICATIONSOUND_CHK, app.LocaleString (IDS_NOTIFICATIONSOUND_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_NOTIFICATIONONTRAY_CHK, app.LocaleString (IDS_NOTIFICATIONONTRAY_CHK, nullptr));

					SetDlgItemText (hwnd, IDC_NOTIFICATIONTIMEOUT_HINT, app.LocaleString (IDS_NOTIFICATIONTIMEOUT_HINT, nullptr));

					const rstring exclude = app.LocaleString (IDS_TITLE_EXCLUDE, nullptr);

					SetDlgItemText (hwnd, IDC_EXCLUDEBLOCKLIST_CHK, _r_fmt (L"%s %s", exclude.GetString (), app.LocaleString (IDS_EXCLUDEBLOCKLIST_CHK, nullptr).GetString ()));
					SetDlgItemText (hwnd, IDC_EXCLUDECUSTOM_CHK, _r_fmt (L"%s %s", exclude.GetString (), app.LocaleString (IDS_EXCLUDECUSTOM_CHK, nullptr).GetString ()));

					SetDlgItemText (hwnd, IDC_EXCLUDESTEALTH_CHK, _r_fmt (L"%s %s", exclude.GetString (), app.LocaleString (IDS_EXCLUDESTEALTH_CHK, nullptr).GetString ()));
					SetDlgItemText (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, _r_fmt (L"%s %s", exclude.GetString (), app.LocaleString (IDS_EXCLUDECLASSIFYALLOW_CHK, (_r_sys_validversion (6, 2) ? nullptr : L" [win8+]")).GetString ()));

					_r_wnd_addstyle (hwnd, IDC_LOGPATH_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
					_r_wnd_addstyle (hwnd, IDC_LOGVIEWER_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

					break;
				}
			}

			break;
		}

		case WM_VSCROLL:
		case WM_HSCROLL:
		{
			const INT ctrl_id = GetDlgCtrlID ((HWND)lparam);

			if (ctrl_id == IDC_LOGSIZELIMIT)
				app.ConfigSet (L"LogSizeLimitKb", (DWORD)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));

			else if (ctrl_id == IDC_NOTIFICATIONTIMEOUT)
				app.ConfigSet (L"NotificationsTimeout", (time_t)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lparam;

			switch (nmlp->code)
			{
				case TTN_GETDISPINFO:
				{
					LPNMTTDISPINFO lpnmdi = (LPNMTTDISPINFO)lparam;

					if ((lpnmdi->uFlags & TTF_IDISHWND) == TTF_IDISHWND)
					{
						WCHAR buffer[1024] = {0};
						const INT ctrl_id = GetDlgCtrlID ((HWND)lpnmdi->hdr.idFrom);

						if (ctrl_id == IDC_RULE_BLOCKOUTBOUND)
							_r_str_copy (buffer, _countof (buffer), app.LocaleString (IDS_RULE_BLOCKOUTBOUND, nullptr));

						else if (ctrl_id == IDC_RULE_BLOCKINBOUND)
							_r_str_copy (buffer, _countof (buffer), app.LocaleString (IDS_RULE_BLOCKINBOUND, nullptr));

						else if (ctrl_id == IDC_RULE_ALLOWLOOPBACK)
							_r_str_copy (buffer, _countof (buffer), app.LocaleString (IDS_RULE_ALLOWLOOPBACK_HINT, nullptr));

						else if (ctrl_id == IDC_USESTEALTHMODE_CHK)
							_r_str_copy (buffer, _countof (buffer), app.LocaleString (IDS_USESTEALTHMODE_HINT, nullptr));

						else if (ctrl_id == IDC_INSTALLBOOTTIMEFILTERS_CHK)
							_r_str_copy (buffer, _countof (buffer), app.LocaleString (IDS_INSTALLBOOTTIMEFILTERS_HINT, nullptr));

						else if (ctrl_id == IDC_SECUREFILTERS_CHK)
							_r_str_copy (buffer, _countof (buffer), app.LocaleString (IDS_SECUREFILTERS_HINT, nullptr));

						if (!_r_str_isempty (buffer))
							lpnmdi->lpszText = buffer;
					}

					break;
				}

				case LVN_ITEMCHANGED:
				{
					LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lparam;

					const INT listview_id = PtrToInt ((void*)lpnmlv->hdr.idFrom);

					if (listview_id != IDC_COLORS)
						break;

					if (IsWindowVisible (lpnmlv->hdr.hwndFrom) && (lpnmlv->uChanged & LVIF_STATE) && (lpnmlv->uNewState == 8192 || lpnmlv->uNewState == 4096) && lpnmlv->uNewState != lpnmlv->uOldState)
					{
						if (_r_fastlock_islocked (&lock_checkbox))
							break;

						const bool new_val = (lpnmlv->uNewState == 8192) ? true : false;

						const size_t idx = lpnmlv->lParam;
						PR_OBJECT ptr_clr_object = _r_obj_reference (colors.at (idx));

						if (ptr_clr_object)
						{
							PITEM_COLOR ptr_clr = (PITEM_COLOR)ptr_clr_object->pdata;

							if (ptr_clr)
								app.ConfigSet (ptr_clr->pcfg_name, new_val, L"colors");

							_r_obj_dereference (ptr_clr_object);

							_r_listview_redraw (app.GetHWND (), _app_gettab_id (app.GetHWND ()));
						}
					}

					break;
				}

				case NM_CUSTOMDRAW:
				{
					const LONG result = _app_nmcustdraw_listview ((LPNMLVCUSTOMDRAW)lparam);

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, result);
					return result;
				}

				case NM_DBLCLK:
				{
					LPNMITEMACTIVATE lpnmlv = (LPNMITEMACTIVATE)lparam;

					const INT listview_id = PtrToInt ((void*)lpnmlv->hdr.idFrom);

					if (lpnmlv->iItem == INVALID_INT || listview_id != IDC_COLORS)
						break;

					const size_t idx = _r_listview_getitemlparam (hwnd, listview_id, lpnmlv->iItem);
					PR_OBJECT ptr_clr_object_current = _r_obj_reference (colors.at (idx));

					PITEM_COLOR ptr_clr_current = nullptr;

					if (ptr_clr_object_current)
						ptr_clr_current = (PITEM_COLOR)ptr_clr_object_current->pdata;

					CHOOSECOLOR cc = {0};
					COLORREF cust[16] = {0};

					for (size_t i = 0; i < min (_countof (cust), colors.size ()); i++)
					{
						PR_OBJECT ptr_clr_object = _r_obj_reference (colors.at (i));

						if (ptr_clr_object)
						{
							PITEM_COLOR ptr_clr = (PITEM_COLOR)ptr_clr_object->pdata;

							if (ptr_clr)
								cust[i] = ptr_clr->default_clr;

							_r_obj_dereference (ptr_clr_object);
						}
					}

					cc.lStructSize = sizeof (cc);
					cc.Flags = CC_RGBINIT | CC_FULLOPEN;
					cc.hwndOwner = hwnd;
					cc.lpCustColors = cust;
					cc.rgbResult = ptr_clr_current ? ptr_clr_current->new_clr : 0;

					if (ChooseColor (&cc))
					{
						if (ptr_clr_current)
						{
							ptr_clr_current->new_clr = cc.rgbResult;
							app.ConfigSet (ptr_clr_current->pcfg_value, cc.rgbResult, L"colors");
						}

						_r_listview_redraw (hwnd, IDC_COLORS);
						_r_listview_redraw (app.GetHWND (), _app_gettab_id (app.GetHWND ()));
					}

					_r_obj_dereference (ptr_clr_object_current);

					break;
				}

				case NM_CLICK:
				case NM_RETURN:
				{
					PNMLINK pnmlink = (PNMLINK)lparam;

					if (!_r_str_isempty (pnmlink->item.szUrl))
						ShellExecute (nullptr, nullptr, pnmlink->item.szUrl, nullptr, nullptr, SW_SHOWNORMAL);

					break;
				}

				case LVN_GETEMPTYMARKUP:
				{
					NMLVEMPTYMARKUP* lpnmlv = (NMLVEMPTYMARKUP*)lparam;

					lpnmlv->dwFlags = EMF_CENTERED;
					_r_str_copy (lpnmlv->szMarkup, _countof (lpnmlv->szMarkup), app.LocaleString (IDS_STATUS_EMPTY, nullptr));

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
					return TRUE;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			switch (LOWORD (wparam))
			{
				case IDC_ALWAYSONTOP_CHK:
				case IDC_STARTMINIMIZED_CHK:
				case IDC_LOADONSTARTUP_CHK:
				case IDC_SKIPUACWARNING_CHK:
				case IDC_CHECKUPDATES_CHK:
				case IDC_CHECKUPDATESBETA_CHK:
				case IDC_LANGUAGE:
				case IDC_CONFIRMEXIT_CHK:
				case IDC_CONFIRMEXITTIMER_CHK:
				case IDC_CONFIRMLOGCLEAR_CHK:
				case IDC_USESTEALTHMODE_CHK:
				case IDC_INSTALLBOOTTIMEFILTERS_CHK:
				case IDC_SECUREFILTERS_CHK:
				case IDC_USECERTIFICATES_CHK:
				case IDC_USENETWORKRESOLUTION_CHK:
				case IDC_USEREFRESHDEVICES_CHK:
				case IDC_RULE_BLOCKOUTBOUND:
				case IDC_RULE_BLOCKINBOUND:
				case IDC_RULE_ALLOWLOOPBACK:
				case IDC_RULE_ALLOW6TO4:
				case IDC_BLOCKLIST_SPY_DISABLE:
				case IDC_BLOCKLIST_SPY_ALLOW:
				case IDC_BLOCKLIST_SPY_BLOCK:
				case IDC_BLOCKLIST_UPDATE_DISABLE:
				case IDC_BLOCKLIST_UPDATE_ALLOW:
				case IDC_BLOCKLIST_UPDATE_BLOCK:
				case IDC_BLOCKLIST_EXTRA_DISABLE:
				case IDC_BLOCKLIST_EXTRA_ALLOW:
				case IDC_BLOCKLIST_EXTRA_BLOCK:
				case IDC_ENABLELOG_CHK:
				case IDC_LOGPATH:
				case IDC_LOGPATH_BTN:
				case IDC_LOGVIEWER:
				case IDC_LOGVIEWER_BTN:
				case IDC_LOGSIZELIMIT_CTRL:
				case IDC_ENABLENOTIFICATIONS_CHK:
				case IDC_NOTIFICATIONSOUND_CHK:
				case IDC_NOTIFICATIONONTRAY_CHK:
				case IDC_NOTIFICATIONTIMEOUT_CTRL:
				case IDC_EXCLUDESTEALTH_CHK:
				case IDC_EXCLUDECLASSIFYALLOW_CHK:
				case IDC_EXCLUDEBLOCKLIST_CHK:
				case IDC_EXCLUDECUSTOM_CHK:
				{
					const INT ctrl_id = LOWORD (wparam);
					const INT notify_code = HIWORD (wparam);

					if (ctrl_id == IDC_ALWAYSONTOP_CHK)
					{
						app.ConfigSet (L"AlwaysOnTop", !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
						CheckMenuItem (GetMenu (app.GetHWND ()), IDM_ALWAYSONTOP_CHK, MF_BYCOMMAND | ((IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? MF_CHECKED : MF_UNCHECKED));
					}
					else if (ctrl_id == IDC_STARTMINIMIZED_CHK)
					{
						app.ConfigSet (L"IsStartMinimized", !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					}
					else if (ctrl_id == IDC_LOADONSTARTUP_CHK)
					{
						app.AutorunEnable (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);
					}
					else if (ctrl_id == IDC_SKIPUACWARNING_CHK)
					{
#ifdef _APP_HAVE_SKIPUAC
						app.SkipUacEnable (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);
#endif // _APP_HAVE_SKIPUAC
					}
					else if (ctrl_id == IDC_CHECKUPDATES_CHK)
					{
						app.ConfigSet (L"CheckUpdates", !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));

#if !defined(_APP_BETA) && !defined(_APP_BETA_RC)
						_r_ctrl_enable (hwnd, IDC_CHECKUPDATESBETA_CHK, (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
#endif
					}
					else if (ctrl_id == IDC_CHECKUPDATESBETA_CHK)
					{
						app.ConfigSet (L"CheckUpdatesBeta", !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					}
					else if (ctrl_id == IDC_LANGUAGE && notify_code == CBN_SELCHANGE)
					{
						app.LocaleApplyFromControl (hwnd, ctrl_id);
					}
					else if (ctrl_id == IDC_CONFIRMEXIT_CHK)
					{
						app.ConfigSet (L"ConfirmExit2", !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					}
					else if (ctrl_id == IDC_CONFIRMEXITTIMER_CHK)
					{
						app.ConfigSet (L"ConfirmExitTimer", !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					}
					else if (ctrl_id == IDC_CONFIRMLOGCLEAR_CHK)
					{
						app.ConfigSet (L"ConfirmLogClear", !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					}
					else if (
						ctrl_id == IDC_RULE_BLOCKOUTBOUND ||
						ctrl_id == IDC_RULE_BLOCKINBOUND ||
						ctrl_id == IDC_RULE_ALLOWLOOPBACK ||
						ctrl_id == IDC_RULE_ALLOW6TO4 ||
						ctrl_id == IDC_SECUREFILTERS_CHK ||
						ctrl_id == IDC_USESTEALTHMODE_CHK ||
						ctrl_id == IDC_INSTALLBOOTTIMEFILTERS_CHK ||
						ctrl_id == IDC_USENETWORKRESOLUTION_CHK ||
						ctrl_id == IDC_USECERTIFICATES_CHK ||
						ctrl_id == IDC_USEREFRESHDEVICES_CHK
						)
					{
						_app_config_apply (app.GetHWND (), ctrl_id);
					}
					else if (ctrl_id >= IDC_BLOCKLIST_SPY_DISABLE && ctrl_id <= IDC_BLOCKLIST_EXTRA_BLOCK)
					{
						const HMENU hmenu = GetMenu (app.GetHWND ());

						if (ctrl_id >= IDC_BLOCKLIST_SPY_DISABLE && ctrl_id <= IDC_BLOCKLIST_SPY_BLOCK)
						{
							const INT new_state = std::clamp (_r_ctrl_isradiobuttonchecked (hwnd, IDC_BLOCKLIST_SPY_DISABLE, IDC_BLOCKLIST_SPY_BLOCK) - IDC_BLOCKLIST_SPY_DISABLE, 0, 2);

							CheckMenuRadioItem (hmenu, IDM_BLOCKLIST_SPY_DISABLE, IDM_BLOCKLIST_SPY_BLOCK, IDM_BLOCKLIST_SPY_DISABLE + new_state, MF_BYCOMMAND);

							app.ConfigSet (L"BlocklistSpyState", new_state);

							_r_fastlock_acquireshared (&lock_access);
							_app_ruleblocklistset (app.GetHWND (), new_state, INVALID_INT, INVALID_INT, true);
							_r_fastlock_releaseshared (&lock_access);
						}
						else if (ctrl_id >= IDC_BLOCKLIST_UPDATE_DISABLE && ctrl_id <= IDC_BLOCKLIST_UPDATE_BLOCK)
						{
							const INT new_state = std::clamp (_r_ctrl_isradiobuttonchecked (hwnd, IDC_BLOCKLIST_UPDATE_DISABLE, IDC_BLOCKLIST_UPDATE_BLOCK) - IDC_BLOCKLIST_UPDATE_DISABLE, 0, 2);

							CheckMenuRadioItem (hmenu, IDM_BLOCKLIST_UPDATE_DISABLE, IDM_BLOCKLIST_UPDATE_BLOCK, IDM_BLOCKLIST_UPDATE_DISABLE + new_state, MF_BYCOMMAND);

							app.ConfigSet (L"BlocklistUpdateState", new_state);

							_r_fastlock_acquireshared (&lock_access);
							_app_ruleblocklistset (app.GetHWND (), INVALID_INT, new_state, INVALID_INT, true);
							_r_fastlock_releaseshared (&lock_access);
						}
						else if (ctrl_id >= IDC_BLOCKLIST_EXTRA_DISABLE && ctrl_id <= IDC_BLOCKLIST_EXTRA_BLOCK)
						{
							const INT new_state = std::clamp (_r_ctrl_isradiobuttonchecked (hwnd, IDC_BLOCKLIST_EXTRA_DISABLE, IDC_BLOCKLIST_EXTRA_BLOCK) - IDC_BLOCKLIST_EXTRA_DISABLE, 0, 2);

							CheckMenuRadioItem (hmenu, IDM_BLOCKLIST_EXTRA_DISABLE, IDM_BLOCKLIST_EXTRA_BLOCK, IDM_BLOCKLIST_EXTRA_DISABLE + new_state, MF_BYCOMMAND);

							app.ConfigSet (L"BlocklistExtraState", new_state);

							_r_fastlock_acquireshared (&lock_access);
							_app_ruleblocklistset (app.GetHWND (), INVALID_INT, INVALID_INT, new_state, true);
							_r_fastlock_releaseshared (&lock_access);
						}
					}
					else if (ctrl_id == IDC_ENABLELOG_CHK)
					{
						const bool is_enabled = !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

						app.ConfigSet (L"IsLogEnabled", is_enabled);
						SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_PRESSBUTTON, IDM_TRAY_ENABLELOG_CHK, MAKELPARAM (is_enabled, 0));

						_r_ctrl_enable (hwnd, IDC_LOGPATH, is_enabled); // input
						_r_ctrl_enable (hwnd, IDC_LOGPATH_BTN, is_enabled); // button

						_r_ctrl_enable (hwnd, IDC_LOGVIEWER, is_enabled); // input
						_r_ctrl_enable (hwnd, IDC_LOGVIEWER_BTN, is_enabled); // button

						EnableWindow ((HWND)SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_GETBUDDY, 0, 0), is_enabled);

						_r_ctrl_enable (hwnd, IDC_EXCLUDESTEALTH_CHK, is_enabled);

						if (_r_sys_validversion (6, 2))
							_r_ctrl_enable (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, is_enabled);

						_app_loginit (is_enabled);
					}
					else if (ctrl_id == IDC_LOGPATH && notify_code == EN_KILLFOCUS)
					{
						rstring logpath = _r_ctrl_gettext (hwnd, ctrl_id);

						if (!logpath.IsEmpty ())
						{
							app.ConfigSet (L"LogPath", _r_path_unexpand (logpath));

							_app_loginit (app.ConfigGet (L"IsLogEnabled", false));
						}
					}
					else if (ctrl_id == IDC_LOGPATH_BTN)
					{
						OPENFILENAME ofn = {0};

						WCHAR path[MAX_PATH] = {0};
						GetDlgItemText (hwnd, IDC_LOGPATH, path, _countof (path));
						_r_str_copy (path, _countof (path), _r_path_expand (path));

						ofn.lStructSize = sizeof (ofn);
						ofn.hwndOwner = hwnd;
						ofn.lpstrFile = path;
						ofn.nMaxFile = _countof (path);
						ofn.lpstrFileTitle = APP_NAME_SHORT;
						ofn.lpstrFilter = L"*." LOG_PATH_EXT L"\0*." LOG_PATH_EXT L"\0\0";
						ofn.lpstrDefExt = LOG_PATH_EXT;
						ofn.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

						if (GetSaveFileName (&ofn))
						{
							_r_str_copy (path, _countof (path), _r_path_unexpand (path));

							app.ConfigSet (L"LogPath", path);
							SetDlgItemText (hwnd, IDC_LOGPATH, path);

							_app_loginit (app.ConfigGet (L"IsLogEnabled", false));
						}
					}
					else if (ctrl_id == IDC_LOGVIEWER && notify_code == EN_KILLFOCUS)
					{
						rstring logviewer = _r_ctrl_gettext (hwnd, ctrl_id);

						if (!logviewer.IsEmpty ())
							app.ConfigSet (L"LogViewer", _r_path_unexpand (logviewer));
					}
					else if (ctrl_id == IDC_LOGVIEWER_BTN)
					{
						OPENFILENAME ofn = {0};

						WCHAR path[MAX_PATH] = {0};
						GetDlgItemText (hwnd, IDC_LOGVIEWER, path, _countof (path));
						_r_str_copy (path, _countof (path), _r_path_expand (path));

						ofn.lStructSize = sizeof (ofn);
						ofn.hwndOwner = hwnd;
						ofn.lpstrFile = path;
						ofn.nMaxFile = _countof (path);
						ofn.lpstrFileTitle = APP_NAME_SHORT;
						ofn.lpstrFilter = L"*.exe\0*.exe\0\0";
						ofn.lpstrDefExt = L"exe";
						ofn.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

						if (GetOpenFileName (&ofn))
						{
							_r_str_copy (path, _countof (path), _r_path_unexpand (path));

							app.ConfigSet (L"LogViewer", path);
							SetDlgItemText (hwnd, IDC_LOGVIEWER, path);
						}
					}
					else if (ctrl_id == IDC_LOGSIZELIMIT_CTRL && notify_code == EN_KILLFOCUS)
					{
						app.ConfigSet (L"LogSizeLimitKb", (DWORD)SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_GETPOS32, 0, 0));
					}
					else if (ctrl_id == IDC_ENABLENOTIFICATIONS_CHK)
					{
						const bool is_enabled = !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

						app.ConfigSet (L"IsNotificationsEnabled", is_enabled);
						SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_PRESSBUTTON, IDM_TRAY_ENABLENOTIFICATIONS_CHK, MAKELPARAM (is_enabled, 0));

						_r_ctrl_enable (hwnd, IDC_NOTIFICATIONSOUND_CHK, is_enabled);
						_r_ctrl_enable (hwnd, IDC_NOTIFICATIONONTRAY_CHK, is_enabled);

						EnableWindow ((HWND)SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_GETBUDDY, 0, 0), is_enabled);

						_r_ctrl_enable (hwnd, IDC_EXCLUDEBLOCKLIST_CHK, is_enabled);
						_r_ctrl_enable (hwnd, IDC_EXCLUDECUSTOM_CHK, is_enabled);

						_app_notifyrefresh (config.hnotification, false);
					}
					else if (ctrl_id == IDC_NOTIFICATIONSOUND_CHK)
					{
						app.ConfigSet (L"IsNotificationsSound", !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					}
					else if (ctrl_id == IDC_NOTIFICATIONONTRAY_CHK)
					{
						app.ConfigSet (L"IsNotificationsOnTray", !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));

						if (IsWindowVisible (config.hnotification))
							_app_notifysetpos (config.hnotification, true);
					}
					else if (ctrl_id == IDC_NOTIFICATIONTIMEOUT_CTRL && notify_code == EN_KILLFOCUS)
					{
						app.ConfigSet (L"NotificationsTimeout", (time_t)SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_GETPOS32, 0, 0));
					}
					else if (ctrl_id == IDC_EXCLUDESTEALTH_CHK)
					{
						app.ConfigSet (L"IsExcludeStealth", !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					}
					else if (ctrl_id == IDC_EXCLUDECLASSIFYALLOW_CHK)
					{
						app.ConfigSet (L"IsExcludeClassifyAllow", !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					}
					else if (ctrl_id == IDC_EXCLUDEBLOCKLIST_CHK)
					{
						app.ConfigSet (L"IsExcludeBlocklist", !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					}
					else if (ctrl_id == IDC_EXCLUDECUSTOM_CHK)
					{
						app.ConfigSet (L"IsExcludeCustomRules", !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					}

					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

void _app_listviewsetpos (HWND hwnd, INT tab_id, INT listview_id, bool is_setfocus)
{
	HWND htab = GetDlgItem (hwnd, tab_id);
	HWND hlistview = GetDlgItem (hwnd, listview_id);

	if (!htab || !hlistview)
		return;

	RECT rc_tab = {0};
	RECT rc_listview = {0};

	GetWindowRect (htab, &rc_tab);
	MapWindowPoints (nullptr, hwnd, (LPPOINT)&rc_tab, 2);

	GetClientRect (htab, &rc_listview);

	rc_listview.left += rc_tab.left;
	rc_listview.top += rc_tab.top;

	rc_listview.right += rc_tab.left;
	rc_listview.bottom += rc_tab.top;

	SendMessage (htab, TCM_ADJUSTRECT, FALSE, (LPARAM)&rc_listview);

	_r_wnd_resize (nullptr, hlistview, nullptr, rc_listview.left, rc_listview.top, _R_RECT_WIDTH (&rc_listview), _R_RECT_HEIGHT (&rc_listview), 0);

	if (is_setfocus)
	{
		if (IsWindowVisible (hwnd) && !IsIconic (hwnd)) // HACK!!!
			SetFocus (hlistview);
	}
}

void _app_resizewindow (HWND hwnd, INT width, INT height)
{
	RECT rc = {0};
	SendDlgItemMessage (hwnd, IDC_STATUSBAR, SB_GETRECT, 0, (LPARAM)&rc);

	const UINT statusbar_height = _R_RECT_HEIGHT (&rc);
	const UINT rebar_height = (UINT)SendDlgItemMessage (hwnd, IDC_REBAR, RB_GETBARHEIGHT, 0, 0);

	HDWP hdefer = BeginDeferWindowPos (2);

	_r_wnd_resize (&hdefer, config.hrebar, nullptr, 0, 0, width, rebar_height, 0);
	_r_wnd_resize (&hdefer, GetDlgItem (hwnd, IDC_TAB), nullptr, 0, rebar_height, width, height - rebar_height - statusbar_height, 0);

	EndDeferWindowPos (hdefer);

	_app_listviewsetpos (hwnd, IDC_TAB, _app_gettab_id (hwnd), false);

	// resize statusbar
	SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_AUTOSIZE, 0, 0);
	SendDlgItemMessage (hwnd, IDC_STATUSBAR, WM_SIZE, 0, 0);
}

void _app_imagelist_init ()
{
	const INT icon_size_small = _r_dc_getdpi (_R_SIZE_ICON16);
	const INT icon_size_large = _r_dc_getdpi (_R_SIZE_ICON32);
	const INT icon_size_toolbar = _r_dc_getdpi (std::clamp (_r_dc_getdpi (app.ConfigGet (L"ToolbarSize", _R_SIZE_ITEMHEIGHT).AsInt ()), _R_SIZE_ICON16, _R_SIZE_ICON32));

	SAFE_DELETE_OBJECT (config.hbmp_enable);
	SAFE_DELETE_OBJECT (config.hbmp_disable);
	SAFE_DELETE_OBJECT (config.hbmp_allow);
	SAFE_DELETE_OBJECT (config.hbmp_block);
	SAFE_DELETE_OBJECT (config.hbmp_rules);
	SAFE_DELETE_OBJECT (config.hbmp_checked);
	SAFE_DELETE_OBJECT (config.hbmp_unchecked);

	config.hbmp_enable = _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_SHIELD_ENABLE), icon_size_small);
	config.hbmp_disable = _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_SHIELD_DISABLE), icon_size_small);

	config.hbmp_allow = _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_ALLOW), icon_size_small);
	config.hbmp_block = _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_BLOCK), icon_size_small);
	config.hbmp_rules = _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_RULES), icon_size_small);

	config.hbmp_checked = _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_CHECKED), icon_size_small);
	config.hbmp_unchecked = _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_UNCHECKED), icon_size_small);

	if (config.himg_toolbar)
		ImageList_SetIconSize (config.himg_toolbar, icon_size_toolbar, icon_size_toolbar);
	else
		config.himg_toolbar = ImageList_Create (icon_size_toolbar, icon_size_toolbar, ILC_COLOR32 | ILC_HIGHQUALITYSCALE, 0, 0);

	if (config.himg_toolbar)
	{
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_SHIELD_ENABLE), icon_size_toolbar), nullptr);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_SHIELD_DISABLE), icon_size_toolbar), nullptr);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_REFRESH), icon_size_toolbar), nullptr);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_SETTINGS), icon_size_toolbar), nullptr);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_NOTIFICATIONS), icon_size_toolbar), nullptr);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_LOG), icon_size_toolbar), nullptr);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_LOGOPEN), icon_size_toolbar), nullptr);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_LOGCLEAR), icon_size_toolbar), nullptr);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_ADD), icon_size_toolbar), nullptr);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_HEARTH), icon_size_toolbar), nullptr);
	}

	if (config.himg_rules_small)
		ImageList_SetIconSize (config.himg_rules_small, icon_size_small, icon_size_small);
	else
		config.himg_rules_small = ImageList_Create (icon_size_small, icon_size_small, ILC_COLOR32 | ILC_HIGHQUALITYSCALE, 2, 2);

	if (config.himg_rules_small)
	{
		ImageList_Add (config.himg_rules_small, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_ALLOW), icon_size_small), nullptr);
		ImageList_Add (config.himg_rules_small, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_BLOCK), icon_size_small), nullptr);
	}

	if (config.himg_rules_large)
		ImageList_SetIconSize (config.himg_rules_large, icon_size_large, icon_size_large);
	else
		config.himg_rules_large = ImageList_Create (icon_size_large, icon_size_large, ILC_COLOR32 | ILC_HIGHQUALITYSCALE, 2, 2);

	if (config.himg_rules_large)
	{
		ImageList_Add (config.himg_rules_large, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_ALLOW), icon_size_large), nullptr);
		ImageList_Add (config.himg_rules_large, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_BLOCK), icon_size_large), nullptr);
	}
}

void _app_toolbar_init (HWND hwnd)
{
	config.hrebar = GetDlgItem (hwnd, IDC_REBAR);

	_r_toolbar_setstyle (hwnd, IDC_TOOLBAR, TBSTYLE_EX_DOUBLEBUFFER | TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_HIDECLIPPEDBUTTONS);

	SendDlgItemMessage (hwnd, IDC_TOOLBAR, TB_SETIMAGELIST, 0, (LPARAM)config.himg_toolbar);

	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_TRAY_START, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, 0, BTNS_SEP);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_OPENRULESEDITOR, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, 8);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, 0, BTNS_SEP);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_TRAY_ENABLENOTIFICATIONS_CHK, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, 4);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_TRAY_ENABLELOG_CHK, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, 5);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, 0, BTNS_SEP);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_REFRESH, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, 2);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_SETTINGS, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, 3);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, 0, BTNS_SEP);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_TRAY_LOGSHOW, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, 6);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_TRAY_LOGCLEAR, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, 7);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, 0, BTNS_SEP);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_DONATE, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, 9);

	REBARBANDINFO rbi = {0};

	rbi.cbSize = sizeof (rbi);
	rbi.fMask = RBBIM_STYLE | RBBIM_CHILD;
	rbi.fStyle = RBBS_CHILDEDGE | RBBS_USECHEVRON | RBBS_VARIABLEHEIGHT | RBBS_NOGRIPPER;
	rbi.hwndChild = GetDlgItem (hwnd, IDC_TOOLBAR);

	SendMessage (config.hrebar, RB_INSERTBAND, 0, (LPARAM)&rbi);
}

void _app_toolbar_resize ()
{
	REBARBANDINFO rbi = {0};

	rbi.cbSize = sizeof (rbi);

	SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_AUTOSIZE, 0, 0);
	const DWORD button_size = (DWORD)SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_GETBUTTONSIZE, 0, 0);

	if (button_size)
	{
		rbi.fMask |= RBBIM_CHILDSIZE;
		rbi.cxMinChild = LOWORD (button_size);
		rbi.cyMinChild = HIWORD (button_size);
	}

	SIZE idealWidth = {0};

	if (SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_GETIDEALSIZE, FALSE, (LPARAM)&idealWidth))
	{
		rbi.fMask |= RBBIM_SIZE | RBBIM_IDEALSIZE;
		rbi.cx = (UINT)idealWidth.cx;
		rbi.cxIdeal = (UINT)idealWidth.cx;
	}

	SendMessage (config.hrebar, RB_SETBANDINFO, 0, (LPARAM)&rbi);
}

void _app_tabs_init (HWND hwnd)
{
	const HINSTANCE hinst = app.GetHINSTANCE ();
	const DWORD listview_ex_style = LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_CHECKBOXES | LVS_EX_HEADERINALLVIEWS;
	const DWORD listview_style = WS_CHILD | WS_TABSTOP | LVS_SHOWSELALWAYS | LVS_REPORT | LVS_SHAREIMAGELISTS | LVS_AUTOARRANGE;
	INT tabs_count = 0;

	CreateWindowEx (0, WC_LISTVIEW, nullptr, listview_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_APPS_PROFILE, hinst, nullptr);
	_r_tab_additem (hwnd, IDC_TAB, tabs_count++, app.LocaleString (IDS_TAB_APPS, nullptr), INVALID_INT, IDC_APPS_PROFILE);

	CreateWindowEx (0, WC_LISTVIEW, nullptr, listview_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_APPS_SERVICE, hinst, nullptr);
	_r_tab_additem (hwnd, IDC_TAB, tabs_count++, app.LocaleString (IDS_TAB_SERVICES, nullptr), INVALID_INT, IDC_APPS_SERVICE);

	// uwp apps (win8+)
	if (_r_sys_validversion (6, 2))
	{
		CreateWindowEx (0, WC_LISTVIEW, nullptr, listview_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_APPS_UWP, hinst, nullptr);
		_r_tab_additem (hwnd, IDC_TAB, tabs_count++, app.LocaleString (IDS_TAB_PACKAGES, nullptr), INVALID_INT, IDC_APPS_UWP);
	}

	CreateWindowEx (0, WC_LISTVIEW, nullptr, listview_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_RULES_BLOCKLIST, hinst, nullptr);
	_r_tab_additem (hwnd, IDC_TAB, tabs_count++, app.LocaleString (IDS_TRAY_SYSTEM_RULES, nullptr), INVALID_INT, IDC_RULES_BLOCKLIST);

	CreateWindowEx (0, WC_LISTVIEW, nullptr, listview_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_RULES_SYSTEM, hinst, nullptr);
	_r_tab_additem (hwnd, IDC_TAB, tabs_count++, app.LocaleString (IDS_TRAY_SYSTEM_RULES, nullptr), INVALID_INT, IDC_RULES_SYSTEM);

	CreateWindowEx (0, WC_LISTVIEW, nullptr, listview_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_RULES_CUSTOM, hinst, nullptr);
	_r_tab_additem (hwnd, IDC_TAB, tabs_count++, app.LocaleString (IDS_TRAY_USER_RULES, nullptr), INVALID_INT, IDC_RULES_CUSTOM);

	CreateWindowEx (0, WC_LISTVIEW, nullptr, listview_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_NETWORK, hinst, nullptr);
	_r_tab_additem (hwnd, IDC_TAB, tabs_count++, app.LocaleString (IDS_TAB_NETWORK, nullptr), INVALID_INT, IDC_NETWORK);

	for (INT i = 0; i < tabs_count; i++)
	{
		const INT listview_id = _app_gettab_id (hwnd, i);

		if (!listview_id)
			continue;

		_app_listviewsetfont (hwnd, listview_id, false);

		if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_RULES_CUSTOM)
		{
			_r_listview_setstyle (hwnd, listview_id, listview_ex_style);

			if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
			{
				_r_listview_addcolumn (hwnd, listview_id, 0, app.LocaleString (IDS_NAME, nullptr), 0, LVCFMT_LEFT);
				_r_listview_addcolumn (hwnd, listview_id, 1, app.LocaleString (IDS_ADDED, nullptr), 0, LVCFMT_RIGHT);
			}
			else
			{
				_r_listview_addcolumn (hwnd, listview_id, 0, app.LocaleString (IDS_NAME, nullptr), 0, LVCFMT_LEFT);
				_r_listview_addcolumn (hwnd, listview_id, 1, app.LocaleString (IDS_PROTOCOL, nullptr), 0, LVCFMT_RIGHT);
				_r_listview_addcolumn (hwnd, listview_id, 2, app.LocaleString (IDS_DIRECTION, nullptr), 0, LVCFMT_RIGHT);
			}

			_r_listview_addgroup (hwnd, listview_id, 0, L"", 0, LVGS_COLLAPSIBLE);
			_r_listview_addgroup (hwnd, listview_id, 1, L"", 0, LVGS_COLLAPSIBLE);
			_r_listview_addgroup (hwnd, listview_id, 2, L"", 0, LVGS_COLLAPSIBLE);
		}
		else if (listview_id == IDC_NETWORK)
		{
			_r_listview_setstyle (hwnd, listview_id, listview_ex_style & ~LVS_EX_CHECKBOXES); // no checkboxes for network tab

			_r_listview_addcolumn (hwnd, listview_id, 0, app.LocaleString (IDS_NAME, nullptr), 0, LVCFMT_LEFT);
			_r_listview_addcolumn (hwnd, listview_id, 1, app.LocaleString (IDS_ADDRESS_LOCAL, nullptr), 0, LVCFMT_LEFT);
			_r_listview_addcolumn (hwnd, listview_id, 2, app.LocaleString (IDS_PORT, nullptr), 0, LVCFMT_LEFT);
			_r_listview_addcolumn (hwnd, listview_id, 3, app.LocaleString (IDS_ADDRESS_REMOTE, nullptr), 0, LVCFMT_LEFT);
			_r_listview_addcolumn (hwnd, listview_id, 4, app.LocaleString (IDS_PORT, nullptr), 0, LVCFMT_LEFT);
			_r_listview_addcolumn (hwnd, listview_id, 5, app.LocaleString (IDS_PROTOCOL, nullptr), 0, LVCFMT_RIGHT);
			_r_listview_addcolumn (hwnd, listview_id, 6, app.LocaleString (IDS_STATE, nullptr), 0, LVCFMT_RIGHT);
		}

		_app_listviewsetpos (hwnd, IDC_TAB, listview_id, false);

		BringWindowToTop (GetDlgItem (hwnd, listview_id)); // HACK!!!
	}
}

void _app_initialize ()
{
	// initialize spinlocks
	_r_fastlock_initialize (&lock_access);
	_r_fastlock_initialize (&lock_apply);
	_r_fastlock_initialize (&lock_cache);
	_r_fastlock_initialize (&lock_checkbox);
	_r_fastlock_initialize (&lock_logbusy);
	_r_fastlock_initialize (&lock_logthread);
	_r_fastlock_initialize (&lock_threadpool);
	_r_fastlock_initialize (&lock_transaction);
	_r_fastlock_initialize (&lock_writelog);

	// set privileges
	{
		LPCWSTR privileges[] = {
			SE_BACKUP_NAME,
			SE_DEBUG_NAME,
			SE_SECURITY_NAME,
			SE_TAKE_OWNERSHIP_NAME,
		};

		_r_sys_setprivilege (privileges, _countof (privileges), true);
	}

	// set process priority
	SetPriorityClass (GetCurrentProcess (), ABOVE_NORMAL_PRIORITY_CLASS);

	// static initializer
	config.wd_length = GetWindowsDirectory (config.windows_dir, _countof (config.windows_dir));

	_r_str_printf (config.profile_path, _countof (config.profile_path), L"%s\\" XML_PROFILE, app.GetProfileDirectory ());
	_r_str_printf (config.profile_internal_path, _countof (config.profile_internal_path), L"%s\\" XML_PROFILE_INTERNAL, app.GetProfileDirectory ());

	_r_str_copy (config.profile_path_backup, _countof (config.profile_path_backup), _r_fmt (L"%s.bak", config.profile_path));

	_r_str_printf (config.apps_path, _countof (config.apps_path), L"%s\\" XML_APPS, app.GetProfileDirectory ());
	_r_str_printf (config.rules_custom_path, _countof (config.rules_custom_path), L"%s\\" XML_RULES_CUSTOM, app.GetProfileDirectory ());
	_r_str_printf (config.rules_config_path, _countof (config.rules_config_path), L"%s\\" XML_RULES_CONFIG, app.GetProfileDirectory ());

	_r_str_printf (config.apps_path_backup, _countof (config.apps_path_backup), L"%s\\" XML_APPS L".bak", app.GetProfileDirectory ());
	_r_str_printf (config.rules_config_path_backup, _countof (config.rules_config_path_backup), L"%s\\" XML_RULES_CONFIG L".bak", app.GetProfileDirectory ());
	_r_str_printf (config.rules_custom_path_backup, _countof (config.rules_custom_path_backup), L"%s\\" XML_RULES_CUSTOM L".bak", app.GetProfileDirectory ());

	config.ntoskrnl_hash = _r_str_hash (PROC_SYSTEM_NAME);
	config.svchost_hash = _r_str_hash (_r_path_expand (PATH_SVCHOST));
	config.my_hash = _r_str_hash (app.GetBinaryPath ());

	// get the Administrators group security identifier
	if (!config.padminsid)
	{
		DWORD size = SECURITY_MAX_SID_SIZE;

		config.padminsid = (PISID)new BYTE[size];

		if (!CreateWellKnownSid (WinBuiltinAdministratorsSid, nullptr, config.padminsid, &size))
			SAFE_DELETE_ARRAY (config.padminsid);
	}

	// get current user security identifier
	if (_r_str_isempty (config.title))
	{
		// get user sid
		HANDLE token = nullptr;
		DWORD token_length = 0;
		PTOKEN_USER token_user = nullptr;

		if (OpenProcessToken (GetCurrentProcess (), TOKEN_QUERY, &token))
		{
			GetTokenInformation (token, TokenUser, nullptr, 0, &token_length);

			if (GetLastError () == ERROR_INSUFFICIENT_BUFFER)
			{
				token_user = new TOKEN_USER[token_length];

				if (GetTokenInformation (token, TokenUser, token_user, token_length, &token_length))
				{
					SID_NAME_USE sid_type;

					WCHAR username[MAX_PATH] = {0};
					WCHAR domain[MAX_PATH] = {0};

					DWORD length1 = _countof (username);
					DWORD length2 = _countof (domain);

					if (LookupAccountSid (nullptr, token_user->User.Sid, username, &length1, domain, &length2, &sid_type))
						_r_str_printf (config.title, _countof (config.title), L"%s [%s\\%s]", APP_NAME, domain, username);
				}

				SAFE_DELETE_ARRAY (token_user);
			}

			CloseHandle (token);
		}

		if (_r_str_isempty (config.title))
			_r_str_copy (config.title, _countof (config.title), APP_NAME); // fallback
	}

	// initialize timers
	{
		if (config.htimer)
			DeleteTimerQueue (config.htimer);

		config.htimer = CreateTimerQueue ();

		timers.clear ();

#if defined(_APP_BETA) || defined(_APP_BETA_RC)
		timers.push_back (_R_SECONDSCLOCK_MIN (1));
#endif // _APP_BETA || _APP_BETA_RC

		timers.push_back (_R_SECONDSCLOCK_MIN (10));
		timers.push_back (_R_SECONDSCLOCK_MIN (20));
		timers.push_back (_R_SECONDSCLOCK_MIN (30));
		timers.push_back (_R_SECONDSCLOCK_HOUR (1));
		timers.push_back (_R_SECONDSCLOCK_HOUR (2));
		timers.push_back (_R_SECONDSCLOCK_HOUR (4));
		timers.push_back (_R_SECONDSCLOCK_HOUR (6));
	}

	// initialize thread objects
	config.done_evt = CreateEventEx (nullptr, nullptr, 0, EVENT_ALL_ACCESS);
}

INT FirstDriveFromMask (DWORD unitmask)
{
	INT i;

	for (i = 0; i < 26; ++i)
	{
		if (unitmask & 0x1)
			break;

		unitmask = unitmask >> 1;
	}

	return i;
}

INT_PTR CALLBACK DlgProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_FINDMSGSTRING)
	{
		const LPFINDREPLACE lpfr = (LPFINDREPLACE)lparam;

		if (!lpfr)
			return FALSE;

		if ((lpfr->Flags & FR_DIALOGTERM) == FR_DIALOGTERM)
		{
			config.hfind = nullptr;
		}
		else if ((lpfr->Flags & FR_FINDNEXT) == FR_FINDNEXT)
		{
			const INT listview_id = _app_gettab_id (hwnd);
			const INT total_count = _r_listview_getitemcount (hwnd, listview_id);

			const INT selected_item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)INVALID_INT, LVNI_SELECTED) + 1;

			for (INT i = selected_item; i < total_count; i++)
			{
				const rstring item_text = _r_listview_getitemtext (hwnd, listview_id, i, 0);

				if (StrStrNIW (item_text, lpfr->lpstrFindWhat, (UINT)_r_str_length (lpfr->lpstrFindWhat)) != nullptr)
				{
					_app_showitem (hwnd, listview_id, i);
					break;
				}
			}
		}

		return FALSE;
	}

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			_app_initialize ();

			// init buffered paint
			BufferedPaintInit ();

			// allow drag&drop support
			DragAcceptFiles (hwnd, TRUE);

			// get default icon for executable
			_app_getfileicon (_r_path_expand (PATH_NTOSKRNL), false, &config.icon_id, &config.hicon_large);
			_app_getfileicon (_r_path_expand (PATH_NTOSKRNL), true, nullptr, &config.hicon_small);
			_app_getfileicon (_r_path_expand (PATH_SHELL32), false, &config.icon_service_id, nullptr);

			// get default icon for windows store package (win8+)
			if (_r_sys_validversion (6, 2))
			{
				if (!_app_getfileicon (_r_path_expand (PATH_WINSTORE), true, &config.icon_uwp_id, &config.hicon_package))
				{
					config.icon_uwp_id = config.icon_id;
					config.hicon_package = config.hicon_small;
				}
			}

			// initialize settings
			app.SettingsAddPage (IDD_SETTINGS_GENERAL, IDS_SETTINGS_GENERAL);
			app.SettingsAddPage (IDD_SETTINGS_RULES, IDS_TRAY_RULES);
			app.SettingsAddPage (IDD_SETTINGS_BLOCKLIST, IDS_TRAY_BLOCKLIST_RULES);
			app.SettingsAddPage (IDD_SETTINGS_INTERFACE, IDS_TITLE_INTERFACE);

			// dropped packets logging (win7+)
			app.SettingsAddPage (IDD_SETTINGS_NOTIFICATIONS, IDS_TITLE_NOTIFICATIONS);
			app.SettingsAddPage (IDD_SETTINGS_LOGGING, IDS_TITLE_LOGGING);

			// initialize colors
			{
				addcolor (IDS_HIGHLIGHT_TIMER, L"IsHighlightTimer", true, L"ColorTimer", LISTVIEW_COLOR_TIMER);
				addcolor (IDS_HIGHLIGHT_INVALID, L"IsHighlightInvalid", true, L"ColorInvalid", LISTVIEW_COLOR_INVALID);
				addcolor (IDS_HIGHLIGHT_SPECIAL, L"IsHighlightSpecial", true, L"ColorSpecial", LISTVIEW_COLOR_SPECIAL);
				addcolor (IDS_HIGHLIGHT_SILENT, L"IsHighlightSilent", true, L"ColorSilent", LISTVIEW_COLOR_SILENT);
				addcolor (IDS_HIGHLIGHT_SIGNED, L"IsHighlightSigned", true, L"ColorSigned", LISTVIEW_COLOR_SIGNED);
				addcolor (IDS_HIGHLIGHT_PICO, L"IsHighlightPico", true, L"ColorPico", LISTVIEW_COLOR_PICO);
				addcolor (IDS_HIGHLIGHT_SYSTEM, L"IsHighlightSystem", true, L"ColorSystem", LISTVIEW_COLOR_SYSTEM);
				addcolor (IDS_HIGHLIGHT_SERVICE, L"IsHighlightService", true, L"ColorService", LISTVIEW_COLOR_SERVICE);
				addcolor (IDS_HIGHLIGHT_PACKAGE, L"IsHighlightPackage", true, L"ColorPackage", LISTVIEW_COLOR_PACKAGE);
				addcolor (IDS_HIGHLIGHT_CONNECTION, L"IsHighlightConnection", true, L"ColorConnection", LISTVIEW_COLOR_CONNECTION);
			}

			// restore window size and position (required!)
			app.RestoreWindowPosition (hwnd, L"window");

			// add blocklist to update
			app.UpdateAddComponent (L"Internal rules", L"profile_internal", _r_fmt (L"%" PRId64, config.profile_internal_timestamp), config.profile_internal_path, false);

			// initialize imagelist
			_app_imagelist_init ();

			// initialize toolbar
			_app_toolbar_init (hwnd);
			_app_toolbar_resize ();

			// initialize tabs
			_app_tabs_init (hwnd);

			// load profile
			_app_profile_load (hwnd);

			// initialize tab
			_app_settab_id (hwnd, app.ConfigGet (L"CurrentTab", IDC_APPS_PROFILE).AsInt ());

			// initialize dropped packets log callback thread (win7+)
			{
				SecureZeroMemory (&log_stack, sizeof (log_stack));

				log_stack.item_count = 0;
				log_stack.thread_count = 0;

				InitializeSListHead (&log_stack.ListHead);

				// create notification window
				_app_notifycreatewindow ();
			}

			// create network monitor thread
			_r_createthread (&NetworkMonitorThread, (LPVOID)hwnd, false, THREAD_PRIORITY_LOWEST);

			// install filters
			if (_wfp_isfiltersinstalled ())
			{
				if (app.ConfigGet (L"IsDisableWindowsFirewallChecked", true).AsBool ())
					_mps_changeconfig2 (false);

				_app_changefilters (hwnd, true, true);
			}

			// set column size when "auto-size" option are disabled
			if (!app.ConfigGet (L"AutoSizeColumns", true).AsBool ())
			{
				for (INT i = 0; i < (INT)SendDlgItemMessage (hwnd, IDC_TAB, TCM_GETITEMCOUNT, 0, 0); i++)
				{
					const INT listview_id = _app_gettab_id (hwnd, i);

					if (listview_id)
						_app_listviewresize (hwnd, listview_id, true);
				}
			}

#ifndef _APP_NO_DARKTHEME
			_r_wnd_setdarktheme (hwnd);
#endif // _APP_NO_DARKTHEME

			break;
		}

		case RM_INITIALIZE:
		{
			if (app.ConfigGet (L"IsShowTitleID", true).AsBool ())
				SetWindowText (hwnd, config.title);

			else
				SetWindowText (hwnd, APP_NAME);

			_r_tray_create (hwnd, UID, WM_TRAYICON, app.GetSharedImage (app.GetHINSTANCE (), IDI_ACTIVE, _r_dc_getdpi (_R_SIZE_ICON16)), APP_NAME, true);

			const HMENU hmenu = GetMenu (hwnd);

			CheckMenuItem (hmenu, IDM_ALWAYSONTOP_CHK, MF_BYCOMMAND | (app.ConfigGet (L"AlwaysOnTop", false).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (hmenu, IDM_SHOWFILENAMESONLY_CHK, MF_BYCOMMAND | (app.ConfigGet (L"ShowFilenames", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (hmenu, IDM_AUTOSIZECOLUMNS_CHK, MF_BYCOMMAND | (app.ConfigGet (L"AutoSizeColumns", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (hmenu, IDM_ENABLESPECIALGROUP_CHK, MF_BYCOMMAND | (app.ConfigGet (L"IsEnableSpecialGroup", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));

			CheckMenuItem (hmenu, IDM_ICONSISTABLEVIEW, MF_BYCOMMAND | (app.ConfigGet (L"IsTableView", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (hmenu, IDM_ICONSISHIDDEN, MF_BYCOMMAND | (app.ConfigGet (L"IsIconsHidden", false).AsBool () ? MF_CHECKED : MF_UNCHECKED));

			{
				UINT menu_id;
				const INT icon_size = std::clamp (app.ConfigGet (L"IconSize", SHIL_SYSSMALL).AsInt (), SHIL_LARGE, SHIL_LAST);

				if (icon_size == SHIL_EXTRALARGE)
					menu_id = IDM_ICONSEXTRALARGE;

				else if (icon_size == SHIL_LARGE)
					menu_id = IDM_ICONSLARGE;

				else
					menu_id = IDM_ICONSSMALL;

				CheckMenuRadioItem (hmenu, IDM_ICONSSMALL, IDM_ICONSEXTRALARGE, menu_id, MF_BYCOMMAND);
			}

			CheckMenuItem (hmenu, IDM_RULE_BLOCKOUTBOUND, MF_BYCOMMAND | (app.ConfigGet (L"BlockOutboundConnections", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (hmenu, IDM_RULE_BLOCKINBOUND, MF_BYCOMMAND | (app.ConfigGet (L"BlockInboundConnections", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (hmenu, IDM_RULE_ALLOWLOOPBACK, MF_BYCOMMAND | (app.ConfigGet (L"AllowLoopbackConnections", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (hmenu, IDM_RULE_ALLOW6TO4, MF_BYCOMMAND | (app.ConfigGet (L"AllowIPv6", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));

			CheckMenuItem (hmenu, IDM_SECUREFILTERS_CHK, MF_BYCOMMAND | (app.ConfigGet (L"IsSecureFilters", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (hmenu, IDM_USESTEALTHMODE_CHK, MF_BYCOMMAND | (app.ConfigGet (L"UseStealthMode", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (hmenu, IDM_INSTALLBOOTTIMEFILTERS_CHK, MF_BYCOMMAND | (app.ConfigGet (L"InstallBoottimeFilters", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));

			CheckMenuItem (hmenu, IDM_USECERTIFICATES_CHK, MF_BYCOMMAND | (app.ConfigGet (L"IsCertificatesEnabled", false).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (hmenu, IDM_USENETWORKRESOLUTION_CHK, MF_BYCOMMAND | (app.ConfigGet (L"IsNetworkResolutionsEnabled", false).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (hmenu, IDM_USEREFRESHDEVICES_CHK, MF_BYCOMMAND | (app.ConfigGet (L"IsRefreshDevices", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));

			CheckMenuRadioItem (hmenu, IDM_BLOCKLIST_SPY_DISABLE, IDM_BLOCKLIST_SPY_BLOCK, IDM_BLOCKLIST_SPY_DISABLE + std::clamp (app.ConfigGet (L"BlocklistSpyState", 2).AsInt (), 0, 2), MF_BYCOMMAND);
			CheckMenuRadioItem (hmenu, IDM_BLOCKLIST_UPDATE_DISABLE, IDM_BLOCKLIST_UPDATE_BLOCK, IDM_BLOCKLIST_UPDATE_DISABLE + std::clamp (app.ConfigGet (L"BlocklistUpdateState", 0).AsInt (), 0, 2), MF_BYCOMMAND);
			CheckMenuRadioItem (hmenu, IDM_BLOCKLIST_EXTRA_DISABLE, IDM_BLOCKLIST_EXTRA_BLOCK, IDM_BLOCKLIST_EXTRA_DISABLE + std::clamp (app.ConfigGet (L"BlocklistExtraState", 0).AsInt (), 0, 2), MF_BYCOMMAND);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLENOTIFICATIONS_CHK, nullptr, 0, app.ConfigGet (L"IsNotificationsEnabled", true).AsBool () ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED);
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLELOG_CHK, nullptr, 0, app.ConfigGet (L"IsLogEnabled", false).AsBool () ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED);

			_app_setinterfacestate (hwnd);

			break;
		}

		case RM_LOCALIZE:
		{
			const HMENU hmenu = GetMenu (hwnd);

			app.LocaleMenu (hmenu, IDS_FILE, 0, true, nullptr);
			app.LocaleMenu (hmenu, IDS_SETTINGS, IDM_SETTINGS, false, L"...\tF2");
			app.LocaleMenu (GetSubMenu (hmenu, 0), IDS_EXPORT, 2, true, nullptr);
			app.LocaleMenu (GetSubMenu (hmenu, 0), IDS_IMPORT, 3, true, nullptr);
			app.LocaleMenu (hmenu, IDS_IMPORT, IDM_IMPORT, false, L"...\tCtrl+O");
			app.LocaleMenu (hmenu, IDS_EXPORT, IDM_EXPORT, false, L"...\tCtrl+S");
			app.LocaleMenu (hmenu, IDS_EXIT, IDM_EXIT, false, nullptr);

			app.LocaleMenu (hmenu, IDS_EDIT, 1, true, nullptr);

			app.LocaleMenu (hmenu, IDS_PURGE_UNUSED, IDM_PURGE_UNUSED, false, L"\tCtrl+Shift+X");
			app.LocaleMenu (hmenu, IDS_PURGE_TIMERS, IDM_PURGE_TIMERS, false, L"\tCtrl+Shift+T");

			app.LocaleMenu (hmenu, IDS_FIND, IDM_FIND, false, L"...\tCtrl+F");
			app.LocaleMenu (hmenu, IDS_FINDNEXT, IDM_FINDNEXT, false, L"\tF3");

			app.LocaleMenu (hmenu, IDS_REFRESH, IDM_REFRESH, false, L"\tF5");

			app.LocaleMenu (hmenu, IDS_VIEW, 2, true, nullptr);

			app.LocaleMenu (hmenu, IDS_ALWAYSONTOP_CHK, IDM_ALWAYSONTOP_CHK, false, nullptr);
			app.LocaleMenu (hmenu, IDS_SHOWFILENAMESONLY_CHK, IDM_SHOWFILENAMESONLY_CHK, false, nullptr);
			app.LocaleMenu (hmenu, IDS_AUTOSIZECOLUMNS_CHK, IDM_AUTOSIZECOLUMNS_CHK, false, nullptr);
			app.LocaleMenu (hmenu, IDS_ENABLESPECIALGROUP_CHK, IDM_ENABLESPECIALGROUP_CHK, false, nullptr);

			app.LocaleMenu (GetSubMenu (hmenu, 2), IDS_ICONS, 5, true, nullptr);
			app.LocaleMenu (hmenu, IDS_ICONSSMALL, IDM_ICONSSMALL, false, nullptr);
			app.LocaleMenu (hmenu, IDS_ICONSLARGE, IDM_ICONSLARGE, false, nullptr);
			app.LocaleMenu (hmenu, IDS_ICONSEXTRALARGE, IDM_ICONSEXTRALARGE, false, nullptr);
			app.LocaleMenu (hmenu, IDS_ICONSISTABLEVIEW, IDM_ICONSISTABLEVIEW, false, nullptr);
			app.LocaleMenu (hmenu, IDS_ICONSISHIDDEN, IDM_ICONSISHIDDEN, false, nullptr);

			app.LocaleMenu (GetSubMenu (hmenu, 2), IDS_LANGUAGE, LANG_MENU, true, L" (Language)");

			app.LocaleMenu (hmenu, IDS_FONT, IDM_FONT, false, L"...");

			const rstring recommended = app.LocaleString (IDS_RECOMMENDED, nullptr);

			app.LocaleMenu (hmenu, IDS_TRAY_RULES, 3, true, nullptr);

			app.LocaleMenu (hmenu, IDS_TAB_NETWORK, IDM_CONNECTIONS_TITLE, false, nullptr);
			app.LocaleMenu (hmenu, IDS_TITLE_SECURITY, IDM_SECURITY_TITLE, false, nullptr);
			app.LocaleMenu (hmenu, IDS_TITLE_ADVANCED, IDM_ADVANCED_TITLE, false, nullptr);

			EnableMenuItem (hmenu, IDM_CONNECTIONS_TITLE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
			EnableMenuItem (hmenu, IDM_SECURITY_TITLE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
			EnableMenuItem (hmenu, IDM_ADVANCED_TITLE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

			app.LocaleMenu (hmenu, IDS_RULE_BLOCKOUTBOUND, IDM_RULE_BLOCKOUTBOUND, false, _r_fmt (L" (%s)", recommended.GetString ()));
			app.LocaleMenu (hmenu, IDS_RULE_BLOCKINBOUND, IDM_RULE_BLOCKINBOUND, false, _r_fmt (L" (%s)", recommended.GetString ()));
			app.LocaleMenu (hmenu, IDS_RULE_ALLOWLOOPBACK, IDM_RULE_ALLOWLOOPBACK, false, _r_fmt (L" (%s)", recommended.GetString ()));
			app.LocaleMenu (hmenu, IDS_RULE_ALLOW6TO4, IDM_RULE_ALLOW6TO4, false, _r_fmt (L" (%s)", recommended.GetString ()));

			app.LocaleMenu (hmenu, IDS_SECUREFILTERS_CHK, IDM_SECUREFILTERS_CHK, false, _r_fmt (L" (%s)", recommended.GetString ()));
			app.LocaleMenu (hmenu, IDS_USESTEALTHMODE_CHK, IDM_USESTEALTHMODE_CHK, false, _r_fmt (L" (%s)", recommended.GetString ()));
			app.LocaleMenu (hmenu, IDS_INSTALLBOOTTIMEFILTERS_CHK, IDM_INSTALLBOOTTIMEFILTERS_CHK, false, _r_fmt (L" (%s)", recommended.GetString ()));

			app.LocaleMenu (hmenu, IDS_USENETWORKRESOLUTION_CHK, IDM_USENETWORKRESOLUTION_CHK, false, nullptr);
			app.LocaleMenu (hmenu, IDS_USECERTIFICATES_CHK, IDM_USECERTIFICATES_CHK, false, nullptr);
			app.LocaleMenu (hmenu, IDS_USEREFRESHDEVICES_CHK, IDM_USEREFRESHDEVICES_CHK, false, _r_fmt (L" (%s)", recommended.GetString ()));

			app.LocaleMenu (hmenu, IDS_TRAY_BLOCKLIST_RULES, 4, true, nullptr);

			app.LocaleMenu (hmenu, IDS_BLOCKLIST_SPY, IDM_BLOCKLIST_SPY_TITLE, false, nullptr);
			app.LocaleMenu (hmenu, IDS_BLOCKLIST_UPDATE, IDM_BLOCKLIST_UPDATE_TITLE, false, nullptr);
			app.LocaleMenu (hmenu, IDS_BLOCKLIST_EXTRA, IDM_BLOCKLIST_EXTRA_TITLE, false, nullptr);

			EnableMenuItem (hmenu, IDM_BLOCKLIST_SPY_TITLE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
			EnableMenuItem (hmenu, IDM_BLOCKLIST_UPDATE_TITLE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
			EnableMenuItem (hmenu, IDM_BLOCKLIST_EXTRA_TITLE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

			app.LocaleMenu (hmenu, IDS_DISABLE, IDM_BLOCKLIST_SPY_DISABLE, false, nullptr);
			app.LocaleMenu (hmenu, IDS_ACTION_ALLOW, IDM_BLOCKLIST_SPY_ALLOW, false, nullptr);
			app.LocaleMenu (hmenu, IDS_ACTION_BLOCK, IDM_BLOCKLIST_SPY_BLOCK, false, _r_fmt (L" (%s)", recommended.GetString ()));

			app.LocaleMenu (hmenu, IDS_DISABLE, IDM_BLOCKLIST_UPDATE_DISABLE, false, _r_fmt (L" (%s)", recommended.GetString ()));
			app.LocaleMenu (hmenu, IDS_ACTION_ALLOW, IDM_BLOCKLIST_UPDATE_ALLOW, false, nullptr);
			app.LocaleMenu (hmenu, IDS_ACTION_BLOCK, IDM_BLOCKLIST_UPDATE_BLOCK, false, nullptr);

			app.LocaleMenu (hmenu, IDS_DISABLE, IDM_BLOCKLIST_EXTRA_DISABLE, false, _r_fmt (L" (%s)", recommended.GetString ()));
			app.LocaleMenu (hmenu, IDS_ACTION_ALLOW, IDM_BLOCKLIST_EXTRA_ALLOW, false, nullptr);
			app.LocaleMenu (hmenu, IDS_ACTION_BLOCK, IDM_BLOCKLIST_EXTRA_BLOCK, false, nullptr);

			app.LocaleMenu (hmenu, IDS_HELP, 5, true, nullptr);
			app.LocaleMenu (hmenu, IDS_WEBSITE, IDM_WEBSITE, false, nullptr);
			app.LocaleMenu (hmenu, IDS_CHECKUPDATES, IDM_CHECKUPDATES, false, nullptr);
			app.LocaleMenu (hmenu, IDS_ABOUT, IDM_ABOUT, false, L"\tF1");

			// localize toolbar
			_app_setinterfacestate (hwnd);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_REFRESH, app.LocaleString (IDS_REFRESH, nullptr), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT);
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_SETTINGS, app.LocaleString (IDS_SETTINGS, nullptr), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_OPENRULESEDITOR, app.LocaleString (IDS_OPENRULESEDITOR, L"..."), BTNS_BUTTON | BTNS_AUTOSIZE);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLENOTIFICATIONS_CHK, app.LocaleString (IDS_ENABLENOTIFICATIONS_CHK, nullptr), BTNS_CHECK | BTNS_AUTOSIZE);
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLELOG_CHK, app.LocaleString (IDS_ENABLELOG_CHK, nullptr), BTNS_CHECK | BTNS_AUTOSIZE);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_LOGSHOW, app.LocaleString (IDS_LOGSHOW, L" (Ctrl+I)"), BTNS_BUTTON | BTNS_AUTOSIZE);
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_LOGCLEAR, app.LocaleString (IDS_LOGCLEAR, L" (Ctrl+X)"), BTNS_BUTTON | BTNS_AUTOSIZE);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_DONATE, app.LocaleString (IDS_DONATE, nullptr), BTNS_BUTTON | BTNS_AUTOSIZE);

			// set rebar size
			_app_toolbar_resize ();

			// localize tabs
			for (INT i = 0; i < (INT)SendDlgItemMessage (hwnd, IDC_TAB, TCM_GETITEMCOUNT, 0, 0); i++)
			{
				const INT listview_id = _app_gettab_id (hwnd, i);

				UINT locale_id;

				if (listview_id == IDC_APPS_PROFILE)
					locale_id = IDS_TAB_APPS;

				else if (listview_id == IDC_APPS_SERVICE)
					locale_id = IDS_TAB_SERVICES;

				else if (listview_id == IDC_APPS_UWP)
					locale_id = IDS_TAB_PACKAGES;

				else if (listview_id == IDC_RULES_BLOCKLIST)
					locale_id = IDS_TRAY_BLOCKLIST_RULES;

				else if (listview_id == IDC_RULES_SYSTEM)
					locale_id = IDS_TRAY_SYSTEM_RULES;

				else if (listview_id == IDC_RULES_CUSTOM)
					locale_id = IDS_TRAY_USER_RULES;

				else if (listview_id == IDC_NETWORK)
					locale_id = IDS_TAB_NETWORK;

				else
					continue;

				_r_tab_setitem (hwnd, IDC_TAB, i, app.LocaleString (locale_id, nullptr));

				if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
				{
					_r_listview_setcolumn (hwnd, listview_id, 0, app.LocaleString (IDS_NAME, nullptr), 0);
					_r_listview_setcolumn (hwnd, listview_id, 1, app.LocaleString (IDS_ADDED, nullptr), 0);

					for (INT j = 0; j < _r_listview_getitemcount (hwnd, listview_id); j++)
					{
						const size_t app_hash = _r_listview_getitemlparam (hwnd, listview_id, j);
						PR_OBJECT ptr_app_object = _app_getappitem (app_hash);

						if (!ptr_app_object)
							continue;

						PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

						if (ptr_app)
						{
							_r_fastlock_acquireshared (&lock_checkbox);
							_app_setappiteminfo (hwnd, listview_id, j, app_hash, ptr_app);
							_r_fastlock_releaseshared (&lock_checkbox);
						}

						_r_obj_dereference (ptr_app_object);
					}
				}
				else if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM)
				{
					_r_listview_setcolumn (hwnd, listview_id, 0, app.LocaleString (IDS_NAME, nullptr), 0);
					_r_listview_setcolumn (hwnd, listview_id, 1, app.LocaleString (IDS_PROTOCOL, nullptr), 0);
					_r_listview_setcolumn (hwnd, listview_id, 2, app.LocaleString (IDS_DIRECTION, nullptr), 0);

					for (INT j = 0; j < _r_listview_getitemcount (hwnd, listview_id); j++)
					{
						const size_t rule_idx = _r_listview_getitemlparam (hwnd, listview_id, j);
						PR_OBJECT ptr_rule_object = _app_getrulebyid (rule_idx);

						if (!ptr_rule_object)
							continue;

						PITEM_RULE ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

						if (ptr_rule)
						{
							_r_fastlock_acquireshared (&lock_checkbox);
							_app_setruleiteminfo (hwnd, listview_id, j, ptr_rule, false);
							_r_fastlock_releaseshared (&lock_checkbox);
						}

						_r_obj_dereference (ptr_rule_object);
					}
				}
				else if (listview_id == IDC_NETWORK)
				{
					_r_listview_setcolumn (hwnd, listview_id, 0, app.LocaleString (IDS_NAME, nullptr), 0);
					_r_listview_setcolumn (hwnd, listview_id, 1, app.LocaleString (IDS_ADDRESS_LOCAL, nullptr), 0);
					_r_listview_setcolumn (hwnd, listview_id, 2, app.LocaleString (IDS_PORT, nullptr), 0);
					_r_listview_setcolumn (hwnd, listview_id, 3, app.LocaleString (IDS_ADDRESS_REMOTE, nullptr), 0);
					_r_listview_setcolumn (hwnd, listview_id, 4, app.LocaleString (IDS_PORT, nullptr), 0);
					_r_listview_setcolumn (hwnd, listview_id, 5, app.LocaleString (IDS_PROTOCOL, nullptr), 0);
					_r_listview_setcolumn (hwnd, listview_id, 6, app.LocaleString (IDS_STATE, nullptr), 0);
				}

				SendDlgItemMessage (hwnd, listview_id, LVM_RESETEMPTYTEXT, 0, 0);
			}

			app.LocaleEnum ((HWND)GetSubMenu (hmenu, 2), LANG_MENU, true, IDX_LANGUAGE); // enum localizations

			_app_listviewresize (hwnd, _app_gettab_id (hwnd));
			_app_refreshstatus (hwnd);

			// refresh notification
			_r_wnd_addstyle (config.hnotification, IDC_RULES_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (config.hnotification, IDC_ALLOW_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (config.hnotification, IDC_BLOCK_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

			_app_notifyrefresh (config.hnotification, false);

			break;
		}

		case RM_DPICHANGED:
		{
			const INT listview_id = _app_gettab_id (hwnd);

			_app_imagelist_init ();

			SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_SETIMAGELIST, 0, (LPARAM)config.himg_toolbar);

			// reset toolbar information
			_app_setinterfacestate (hwnd);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_REFRESH, app.LocaleString (IDS_REFRESH, nullptr), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT);
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_SETTINGS, app.LocaleString (IDS_SETTINGS, nullptr), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_OPENRULESEDITOR, app.LocaleString (IDS_OPENRULESEDITOR, L"..."), BTNS_BUTTON | BTNS_AUTOSIZE);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLENOTIFICATIONS_CHK, app.LocaleString (IDS_ENABLENOTIFICATIONS_CHK, nullptr), BTNS_CHECK | BTNS_AUTOSIZE);
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLELOG_CHK, app.LocaleString (IDS_ENABLELOG_CHK, nullptr), BTNS_CHECK | BTNS_AUTOSIZE);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_LOGSHOW, app.LocaleString (IDS_LOGSHOW, L" (Ctrl+I)"), BTNS_BUTTON | BTNS_AUTOSIZE);
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_LOGCLEAR, app.LocaleString (IDS_LOGCLEAR, L" (Ctrl+X)"), BTNS_BUTTON | BTNS_AUTOSIZE);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_DONATE, app.LocaleString (IDS_DONATE, nullptr), BTNS_BUTTON | BTNS_AUTOSIZE);

			_app_toolbar_resize ();

			_app_listviewsetview (hwnd, listview_id);
			_app_listviewsetfont (hwnd, listview_id, true);
			_app_listviewresize (hwnd, listview_id);

			_app_refreshstatus (hwnd, false);

			break;
		}

		case RM_UNINITIALIZE:
		{
			_r_tray_destroy (hwnd, UID);
			break;
		}

		case RM_UPDATE_DONE:
		{
			_app_profile_save ();
			_app_profile_load (hwnd);

			_app_refreshstatus (hwnd);

			_app_changefilters (hwnd, true, false);

			break;
		}

		case RM_RESET_DONE:
		{
			const time_t current_timestamp = (time_t)lparam;

			_app_freeobjects_map (rules_config, true);

			_r_fs_makebackup (config.profile_path, current_timestamp);
			_r_fs_makebackup (config.profile_path_backup, current_timestamp);

			_app_profile_load (hwnd);

			_app_refreshstatus (hwnd);

			_app_changefilters (hwnd, true, false);

			break;
		}

		case WM_CLOSE:
		{
			if (_wfp_isfiltersinstalled ())
			{
				if (_app_istimersactive () ?
					!app.ConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION_TIMER, nullptr), L"ConfirmExitTimer") :
					!app.ConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION_EXIT, nullptr), L"ConfirmExit2"))
				{
					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
					return TRUE;
				}
			}

			DestroyWindow (hwnd);

			break;
		}

		case WM_DESTROY:
		{
			app.ConfigSet (L"CurrentTab", _app_gettab_id (hwnd));

			if (config.hnotification)
				DestroyWindow (config.hnotification);

			UnregisterClass (NOTIFY_CLASS_DLG, app.GetHINSTANCE ());

			if (config.htimer)
				DeleteTimerQueue (config.htimer);

			_r_tray_destroy (hwnd, UID);

			_app_loginit (false);
			_app_freelogstack ();

			if (config.done_evt)
			{
				if (_wfp_isfiltersapplying ())
					WaitForSingleObjectEx (config.done_evt, FILTERS_TIMEOUT, FALSE);

				CloseHandle (config.done_evt);
			}

			_wfp_uninitialize (false);

			ImageList_Destroy (config.himg_toolbar);
			ImageList_Destroy (config.himg_rules_small);
			ImageList_Destroy (config.himg_rules_large);

			BufferedPaintUnInit ();

			PostQuitMessage (0);

			break;
		}

		case WM_DROPFILES:
		{
			const UINT numfiles = DragQueryFile ((HDROP)wparam, UINT32_MAX, nullptr, 0);
			size_t app_hash = 0;

			for (UINT i = 0; i < numfiles; i++)
			{
				const UINT length = DragQueryFile ((HDROP)wparam, i, nullptr, 0) + 1;

				LPWSTR file = new WCHAR[length];

				DragQueryFile ((HDROP)wparam, i, file, length);

				_r_fastlock_acquireexclusive (&lock_access);
				app_hash = _app_addapplication (hwnd, file, 0, 0, 0, false, false, false);
				_r_fastlock_releaseexclusive (&lock_access);

				SAFE_DELETE_ARRAY (file);
			}

			DragFinish ((HDROP)wparam);

			_app_profile_save ();
			_app_refreshstatus (hwnd);

			{
				INT app_listview_id = 0;

				if (_app_getappinfo (app_hash, InfoListviewId, &app_listview_id, sizeof (app_listview_id)))
				{
					if (app_listview_id == _app_gettab_id (hwnd))
						_app_listviewsort (hwnd, app_listview_id);

					_app_showitem (hwnd, app_listview_id, _app_getposition (hwnd, app_listview_id, app_hash));
				}
			}

			break;
		}

		case WM_SETTINGCHANGE:
		{
			_app_notifyrefresh (config.hnotification, false);
			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lparam;

			switch (nmlp->code)
			{
				case TCN_SELCHANGE:
				{
					const INT listview_id = _app_gettab_id (hwnd);
					const HWND hlistview = GetDlgItem (hwnd, listview_id);

					if (IsWindowVisible (hlistview))
					{
						SetFocus (hlistview);
						break;
					}

					// hide tabs
					{
						const INT tab_count = (INT)SendDlgItemMessage (hwnd, IDC_TAB, TCM_GETITEMCOUNT, 0, 0);

						for (INT i = 0; i < tab_count; i++)
						{
							const INT current_id = _app_gettab_id (hwnd, i);

							if (current_id)
								ShowWindow (GetDlgItem (hwnd, current_id), SW_HIDE);
						}
					}

					_app_listviewsetpos (hwnd, IDC_TAB, listview_id, true);

					_app_listviewsort (hwnd, listview_id);
					_app_listviewsetview (hwnd, listview_id);
					_app_listviewsetfont (hwnd, listview_id, false);

					_app_listviewresize (hwnd, listview_id);
					_app_refreshstatus (hwnd);

					_r_listview_redraw (hwnd, listview_id);

					ShowWindow (hlistview, SW_SHOW);

					break;
				}

				case NM_CUSTOMDRAW:
				{
					LONG result = CDRF_DODEFAULT;
					LPNMLVCUSTOMDRAW lpnmcd = (LPNMLVCUSTOMDRAW)lparam;

					const INT ctrl_id = PtrToInt ((void*)lpnmcd->nmcd.hdr.idFrom);

					if (ctrl_id == IDC_TOOLBAR)
					{
						result = _app_nmcustdraw_toolbar (lpnmcd);
					}
					else
					{
						if (_r_fastlock_tryacquireshared (&lock_access))
						{
							result = _app_nmcustdraw_listview (lpnmcd);
							_r_fastlock_releaseshared (&lock_access);
						}
					}

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, result);
					return result;
				}

				case LVN_COLUMNCLICK:
				{
					LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lparam;
					_app_listviewsort (hwnd, PtrToInt ((void*)lpnmlv->hdr.idFrom), lpnmlv->iSubItem, true);

					break;
				}

				case LVN_GETINFOTIP:
				{
					if (_r_fastlock_tryacquireshared (&lock_access))
					{
						LPNMLVGETINFOTIP lpnmlv = (LPNMLVGETINFOTIP)lparam;

						const INT listview_id = PtrToInt ((void*)lpnmlv->hdr.idFrom);
						const size_t idx = _r_listview_getitemlparam (hwnd, listview_id, lpnmlv->iItem);

						_r_str_copy (lpnmlv->pszText, lpnmlv->cchTextMax, _app_gettooltip (listview_id, idx));

						_r_fastlock_releaseshared (&lock_access);
					}

					break;
				}

				case LVN_ITEMCHANGED:
				{
					LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lparam;

					if (IsWindowVisible (lpnmlv->hdr.hwndFrom) && (lpnmlv->uChanged & LVIF_STATE) && (lpnmlv->uNewState == 8192 || lpnmlv->uNewState == 4096) && lpnmlv->uNewState != lpnmlv->uOldState)
					{
						if (_r_fastlock_islocked (&lock_checkbox))
							break;

						const INT listview_id = PtrToInt ((void*)lpnmlv->hdr.idFrom);
						bool is_changed = false;

						const bool new_val = (lpnmlv->uNewState == 8192) ? true : false;

						if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
						{
							const size_t app_hash = lpnmlv->lParam;
							PR_OBJECT ptr_app_object = _app_getappitem (app_hash);

							if (!ptr_app_object)
								break;

							PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

							OBJECTS_VEC rules;

							if (ptr_app)
							{
								if (ptr_app->is_enabled != new_val)
								{
									ptr_app->is_enabled = new_val;

									_r_fastlock_acquireshared (&lock_checkbox);
									_app_setappiteminfo (hwnd, listview_id, lpnmlv->iItem, app_hash, ptr_app);
									_r_fastlock_releaseshared (&lock_checkbox);

									if (new_val)
										_app_freenotify (app_hash, ptr_app);

									if (!new_val && _app_istimeractive (ptr_app))
										_app_timer_reset (hwnd, ptr_app);

									rules.push_back (ptr_app_object);
									_wfp_create3filters (_wfp_getenginehandle (), rules, __LINE__);

									is_changed = true;
								}
							}

							_r_obj_dereference (ptr_app_object);
						}
						else if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM)
						{
							OBJECTS_VEC rules;

							const size_t rule_idx = lpnmlv->lParam;
							PR_OBJECT ptr_rule_object = _app_getrulebyid (rule_idx);

							if (!ptr_rule_object)
								break;

							PITEM_RULE ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

							if (ptr_rule)
							{
								if (ptr_rule->is_enabled != new_val)
								{
									_r_fastlock_acquireshared (&lock_checkbox);

									_app_ruleenable (ptr_rule, new_val);
									_app_setruleiteminfo (hwnd, listview_id, lpnmlv->iItem, ptr_rule, true);

									_r_fastlock_releaseshared (&lock_checkbox);

									rules.push_back (ptr_rule_object);
									_wfp_create4filters (_wfp_getenginehandle (), rules, __LINE__);

									is_changed = true;
								}
							}

							_r_obj_dereference (ptr_rule_object);
						}

						if (is_changed)
						{
							_app_listviewsort (hwnd, listview_id);

							_app_profile_save ();
							_app_refreshstatus (hwnd);
						}
					}

					break;
				}

				case LVN_GETEMPTYMARKUP:
				{
					NMLVEMPTYMARKUP* lpnmlv = (NMLVEMPTYMARKUP*)lparam;

					lpnmlv->dwFlags = EMF_CENTERED;
					_r_str_copy (lpnmlv->szMarkup, _countof (lpnmlv->szMarkup), app.LocaleString (IDS_STATUS_EMPTY, nullptr));

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
					return TRUE;
				}

				case NM_DBLCLK:
				{
					LPNMITEMACTIVATE lpnmlv = (LPNMITEMACTIVATE)lparam;

					if (lpnmlv->iItem == INVALID_INT)
						break;

					INT command_id = 0;
					const INT ctrl_id = PtrToInt ((void*)lpnmlv->hdr.idFrom);

					if (ctrl_id == IDC_STATUSBAR)
					{
						LPNMMOUSE nmouse = (LPNMMOUSE)lparam;

						if (nmouse->dwItemSpec == 0)
							command_id = IDM_SELECT_ALL;

						else if (nmouse->dwItemSpec == 1)
							command_id = IDM_PURGE_UNUSED;

						else if (nmouse->dwItemSpec == 2)
							command_id = IDM_PURGE_TIMERS;
					}
					else if (ctrl_id >= IDC_APPS_PROFILE && ctrl_id <= IDC_APPS_UWP)
					{
						command_id = IDM_EXPLORE;
					}
					else if (ctrl_id >= IDC_RULES_BLOCKLIST && ctrl_id <= IDC_NETWORK)
					{
						command_id = IDM_PROPERTIES;
					}

					if (command_id)
						PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (command_id, 0), 0);

					break;
				}

				case NM_RCLICK:
				{
					LPNMITEMACTIVATE lpnmlv = (LPNMITEMACTIVATE)lparam;

					if (lpnmlv->iItem == INVALID_INT)
						break;

					const INT listview_id = PtrToInt ((void*)lpnmlv->hdr.idFrom);

					UINT menu_id;

					if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
						menu_id = IDM_APPS;

					else if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM)
						menu_id = IDM_RULES;

					else if (listview_id == IDC_NETWORK)
						menu_id = IDM_NETWORK;

					else
						break;

					const size_t hash_item = _r_listview_getitemlparam (hwnd, listview_id, lpnmlv->iItem);
					const INT lv_column_current = std::clamp (lpnmlv->iSubItem, 0, _r_listview_getcolumncount (hwnd, listview_id) - 1);

					const HMENU hmenu = LoadMenu (nullptr, MAKEINTRESOURCE (menu_id));
					const HMENU hsubmenu = GetSubMenu (hmenu, 0);

					// localize
					app.LocaleMenu (hsubmenu, IDS_ADD_FILE, IDM_ADD_FILE, false, L"...");
					app.LocaleMenu (hsubmenu, IDS_DISABLENOTIFICATIONS, IDM_DISABLENOTIFICATIONS, false, nullptr);
					app.LocaleMenu (hsubmenu, IDS_DISABLETIMER, IDM_DISABLETIMER, false, nullptr);
					app.LocaleMenu (hsubmenu, IDS_COPY, IDM_COPY, false, L"\tCtrl+C");
					app.LocaleMenu (hsubmenu, IDS_COPY, IDM_COPY2, false, _r_fmt (L" \"%s\"", _r_listview_getcolumntext (hwnd, listview_id, lv_column_current).GetString ()));
					app.LocaleMenu (hsubmenu, IDS_DELETE, IDM_DELETE, false, L"\tDel");
					app.LocaleMenu (hsubmenu, IDS_SELECT_ALL, IDM_SELECT_ALL, false, L"\tCtrl+A");
					app.LocaleMenu (hsubmenu, IDS_CHECK, IDM_CHECK, false, nullptr);
					app.LocaleMenu (hsubmenu, IDS_UNCHECK, IDM_UNCHECK, false, nullptr);
					app.LocaleMenu (hsubmenu, IDS_NETWORK_CLOSE, IDM_NETWORK_CLOSE, false, nullptr);

					app.LocaleMenu (hsubmenu, menu_id == IDM_RULES ? IDS_ADD : IDS_OPENRULESEDITOR, IDM_OPENRULESEDITOR, false, L"...");

					if (menu_id == IDM_NETWORK)
					{
						app.LocaleMenu (hsubmenu, IDS_SHOWINLIST, IDM_PROPERTIES, false, L"\tEnter");
						app.LocaleMenu (hsubmenu, IDS_EXPLORE, IDM_EXPLORE, false, L"\tCtrl+E");
					}
					else if (menu_id == IDM_RULES)
					{
						app.LocaleMenu (hsubmenu, IDS_EDIT2, IDM_PROPERTIES, false, L"...\tEnter");
					}
					else if (menu_id == IDM_APPS)
					{
						app.LocaleMenu (hsubmenu, IDS_EXPLORE, IDM_EXPLORE, false, L"\tEnter");
					}

					if (menu_id == IDM_APPS)
					{
						SetMenuDefaultItem (hsubmenu, IDM_EXPLORE, FALSE);

						const bool is_filtersinstalled = _wfp_isfiltersinstalled ();
						const time_t current_time = _r_unixtime_now ();

#define RULES_ID 2
#define TIMER_ID 3

						const HMENU hsubmenu_rules = GetSubMenu (hsubmenu, RULES_ID);
						const HMENU hsubmenu_timer = GetSubMenu (hsubmenu, TIMER_ID);

						// set icons
						{
							MENUITEMINFO mii = {0};

							mii.cbSize = sizeof (mii);
							mii.fMask = MIIM_BITMAP;

							mii.hbmpItem = config.hbmp_rules;
							SetMenuItemInfo (hsubmenu, RULES_ID, TRUE, &mii);
						}

						// localize
						app.LocaleMenu (hsubmenu, IDS_TRAY_RULES, RULES_ID, true, nullptr);
						app.LocaleMenu (hsubmenu, IDS_TIMER, TIMER_ID, true, nullptr);

						// show configuration
						PITEM_APP ptr_app = nullptr;
						PR_OBJECT ptr_app_object = _app_getappitem (hash_item);

						if (ptr_app_object)
						{
							ptr_app = (PITEM_APP)ptr_app_object->pdata;

							if (ptr_app)
								CheckMenuItem (hsubmenu, IDM_DISABLENOTIFICATIONS, MF_BYCOMMAND | (ptr_app->is_silent ? MF_CHECKED : MF_UNCHECKED));
						}

						// show rules
						_app_generate_rulesmenu (hsubmenu_rules, hash_item);

						// show timers
						{
							bool is_checked = false;

							for (size_t i = 0; i < timers.size (); i++)
							{
								MENUITEMINFO mii = {0};

								WCHAR buffer[128] = {0};
								_r_str_copy (buffer, _countof (buffer), _r_fmt_interval (timers.at (i) + 1, 1));

								mii.cbSize = sizeof (mii);
								mii.fMask = MIIM_ID | MIIM_FTYPE | MIIM_STATE | MIIM_STRING;
								mii.fType = MFT_STRING;
								mii.dwTypeData = buffer;
								mii.fState = MF_ENABLED;
								mii.wID = IDX_TIMER + UINT (i);

								InsertMenuItem (hsubmenu_timer, mii.wID, FALSE, &mii);

								if (ptr_app)
								{
									if (!is_checked && ptr_app->timer > current_time && ptr_app->timer <= (current_time + timers.at (i)))
									{
										CheckMenuRadioItem (hsubmenu_timer, IDX_TIMER, IDX_TIMER + UINT (timers.size ()), mii.wID, MF_BYCOMMAND);
										is_checked = true;
									}
								}
							}

							if (!is_checked)
								CheckMenuRadioItem (hsubmenu_timer, IDM_DISABLETIMER, IDM_DISABLETIMER, IDM_DISABLETIMER, MF_BYCOMMAND);
						}

						_r_obj_dereference (ptr_app_object);

						if (listview_id != IDC_APPS_PROFILE)
							EnableMenuItem (hsubmenu, IDM_DELETE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					}
					else if (menu_id == IDM_RULES)
					{
						SetMenuDefaultItem (hsubmenu, IDM_PROPERTIES, FALSE);

						if (listview_id == IDC_RULES_CUSTOM)
						{
							PR_OBJECT ptr_rule_object = _app_getrulebyid (hash_item);

							if (!ptr_rule_object)
								break;

							PITEM_RULE ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

							if (ptr_rule && ptr_rule->is_readonly)
								EnableMenuItem (hsubmenu, IDM_DELETE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

							_r_obj_dereference (ptr_rule_object);
						}
						else
						{
							DeleteMenu (hsubmenu, IDM_OPENRULESEDITOR, MF_BYCOMMAND);
							DeleteMenu (hsubmenu, IDM_DELETE, MF_BYCOMMAND);
						}
					}
					else if (menu_id == IDM_NETWORK)
					{
						SetMenuDefaultItem (hsubmenu, IDM_PROPERTIES, FALSE);

						PR_OBJECT ptr_network_object = _r_obj_reference (network_map[hash_item]);

						if (ptr_network_object)
						{
							PITEM_NETWORK ptr_network = (PITEM_NETWORK)ptr_network_object->pdata;

							if (ptr_network)
							{
								if (ptr_network->af != AF_INET || ptr_network->state != MIB_TCP_STATE_ESTAB)
									EnableMenuItem (hsubmenu, IDM_NETWORK_CLOSE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

								if (!ptr_network->app_hash || !ptr_network->path)
									EnableMenuItem (hsubmenu, IDM_EXPLORE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
							}

							_r_obj_dereference (ptr_network_object);
						}
					}

					POINT pt = {0};
					GetCursorPos (&pt);

					const INT cmd = TrackPopupMenuEx (hsubmenu, TPM_RIGHTBUTTON | TPM_LEFTBUTTON | TPM_RETURNCMD, pt.x, pt.y, hwnd, nullptr);

					if (cmd)
						PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (cmd, 0), (LPARAM)lv_column_current);

					DestroyMenu (hmenu);

					break;
				}
			}

			break;
		}

		case WM_SIZE:
		{
			_app_resizewindow (hwnd, LOWORD (lparam), HIWORD (lparam));

			_app_refreshstatus (hwnd, false);

			if (wparam == SIZE_RESTORED || wparam == SIZE_MAXIMIZED)
				_app_listviewresize (hwnd, _app_gettab_id (hwnd));

			break;
		}

		case WM_TRAYICON:
		{
			switch (LOWORD (lparam))
			{
				case NIN_POPUPOPEN:
				{
					PR_OBJECT ptr_log_object = _app_notifyget_obj (_app_notifyget_id (config.hnotification, 0));

					if (ptr_log_object)
					{
						_app_notifyshow (config.hnotification, ptr_log_object, true, false);
						_r_obj_dereference (ptr_log_object);
					}

					break;
				}

				case WM_MBUTTONUP:
				{
					PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDM_TRAY_LOGSHOW, 0), 0);
					break;
				}

				case WM_LBUTTONUP:
				{
					SetForegroundWindow (hwnd);
					break;
				}

				case WM_LBUTTONDBLCLK:
				{
					_r_wnd_toggle (hwnd, false);
					break;
				}

				case WM_RBUTTONUP:
				{
					SetForegroundWindow (hwnd); // don't touch

#define NOTIFICATIONS_ID 4
#define LOGGING_ID 5
#define ERRLOG_ID 6

					const bool is_filtersinstalled = _wfp_isfiltersinstalled ();

					const HMENU hmenu = LoadMenu (nullptr, MAKEINTRESOURCE (IDM_TRAY));
					const HMENU hsubmenu = GetSubMenu (hmenu, 0);

					{
						MENUITEMINFO mii = {0};

						mii.cbSize = sizeof (mii);
						mii.fMask = MIIM_BITMAP;

						mii.hbmpItem = is_filtersinstalled ? config.hbmp_disable : config.hbmp_enable;
						SetMenuItemInfo (hsubmenu, IDM_TRAY_START, FALSE, &mii);
					}

					// localize
					app.LocaleMenu (hsubmenu, IDS_TRAY_SHOW, IDM_TRAY_SHOW, false, nullptr);
					app.LocaleMenu (hsubmenu, is_filtersinstalled ? IDS_TRAY_STOP : IDS_TRAY_START, IDM_TRAY_START, false, nullptr);

					app.LocaleMenu (hsubmenu, IDS_TITLE_NOTIFICATIONS, NOTIFICATIONS_ID, true, nullptr);
					app.LocaleMenu (hsubmenu, IDS_TITLE_LOGGING, LOGGING_ID, true, nullptr);

					app.LocaleMenu (hsubmenu, IDS_ENABLENOTIFICATIONS_CHK, IDM_TRAY_ENABLENOTIFICATIONS_CHK, false, nullptr);
					app.LocaleMenu (hsubmenu, IDS_NOTIFICATIONSOUND_CHK, IDM_TRAY_ENABLENOTIFICATIONSSOUND_CHK, false, nullptr);
					app.LocaleMenu (hsubmenu, IDS_NOTIFICATIONONTRAY_CHK, IDM_TRAY_NOTIFICATIONONTRAY_CHK, false, nullptr);

					app.LocaleMenu (hsubmenu, IDS_ENABLELOG_CHK, IDM_TRAY_ENABLELOG_CHK, false, nullptr);
					app.LocaleMenu (hsubmenu, IDS_LOGSHOW, IDM_TRAY_LOGSHOW, false, nullptr);
					app.LocaleMenu (hsubmenu, IDS_LOGCLEAR, IDM_TRAY_LOGCLEAR, false, nullptr);

					{
						const rstring path = _r_path_expand (app.ConfigGet (L"LogPath", LOG_PATH_DEFAULT));

						if (!_r_fs_exists (path))
						{
							EnableMenuItem (hsubmenu, IDM_TRAY_LOGSHOW, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
							EnableMenuItem (hsubmenu, IDM_TRAY_LOGCLEAR, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
						}
					}

					if (_r_fs_exists (_r_dbg_getpath ()))
					{
						app.LocaleMenu (hsubmenu, IDS_TRAY_LOGERR, ERRLOG_ID, true, nullptr);

						app.LocaleMenu (hsubmenu, IDS_LOGSHOW, IDM_TRAY_LOGSHOW_ERR, false, nullptr);
						app.LocaleMenu (hsubmenu, IDS_LOGCLEAR, IDM_TRAY_LOGCLEAR_ERR, false, nullptr);
					}
					else
					{
						DeleteMenu (hsubmenu, ERRLOG_ID, MF_BYPOSITION);
					}

					app.LocaleMenu (hsubmenu, IDS_SETTINGS, IDM_TRAY_SETTINGS, false, L"...");
					app.LocaleMenu (hsubmenu, IDS_WEBSITE, IDM_TRAY_WEBSITE, false, nullptr);
					app.LocaleMenu (hsubmenu, IDS_ABOUT, IDM_TRAY_ABOUT, false, nullptr);
					app.LocaleMenu (hsubmenu, IDS_EXIT, IDM_TRAY_EXIT, false, nullptr);

					CheckMenuItem (hsubmenu, IDM_TRAY_ENABLENOTIFICATIONS_CHK, MF_BYCOMMAND | (app.ConfigGet (L"IsNotificationsEnabled", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));
					CheckMenuItem (hsubmenu, IDM_TRAY_ENABLENOTIFICATIONSSOUND_CHK, MF_BYCOMMAND | (app.ConfigGet (L"IsNotificationsSound", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));
					CheckMenuItem (hsubmenu, IDM_TRAY_NOTIFICATIONONTRAY_CHK, MF_BYCOMMAND | (app.ConfigGet (L"IsNotificationsOnTray", false).AsBool () ? MF_CHECKED : MF_UNCHECKED));

					CheckMenuItem (hsubmenu, IDM_TRAY_ENABLELOG_CHK, MF_BYCOMMAND | (app.ConfigGet (L"IsLogEnabled", false).AsBool () ? MF_CHECKED : MF_UNCHECKED));

					if (!app.ConfigGet (L"IsNotificationsEnabled", true).AsBool ())
					{
						EnableMenuItem (hsubmenu, IDM_TRAY_ENABLENOTIFICATIONSSOUND_CHK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
						EnableMenuItem (hsubmenu, IDM_TRAY_NOTIFICATIONONTRAY_CHK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					}

					if (_wfp_isfiltersapplying ())
						EnableMenuItem (hsubmenu, IDM_TRAY_START, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

					POINT pt = {0};
					GetCursorPos (&pt);

					TrackPopupMenuEx (hsubmenu, TPM_RIGHTBUTTON | TPM_LEFTBUTTON, pt.x, pt.y, hwnd, nullptr);

					DestroyMenu (hmenu);

					break;
				}
			}

			break;
		}

		case WM_POWERBROADCAST:
		{
			if (_wfp_isfiltersapplying ())
				break;

			switch (wparam)
			{
				case PBT_APMSUSPEND:
				{
					_wfp_uninitialize (false);

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
					return TRUE;
				}

				case PBT_APMRESUMECRITICAL:
				case PBT_APMRESUMESUSPEND:
				{
					if (_wfp_isfiltersapplying ())
						break;

					app.ConfigInit ();

					_app_profile_load (hwnd);
					_app_refreshstatus (hwnd);

					if (_wfp_isfiltersinstalled ())
					{
						if (_wfp_initialize (true))
							_app_changefilters (hwnd, true, true);
					}
					else
					{
						_r_listview_redraw (hwnd, _app_gettab_id (hwnd));
					}

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
					return TRUE;
				}
			}

			break;
		}

		case WM_DEVICECHANGE:
		{
			switch (wparam)
			{
				case DBT_DEVICEARRIVAL:
				case DBT_DEVICEREMOVECOMPLETE:
				{
					if (!app.ConfigGet (L"IsRefreshDevices", true).AsBool ())
						break;

					const PDEV_BROADCAST_HDR lbhdr = (PDEV_BROADCAST_HDR)lparam;

					if (lbhdr && lbhdr->dbch_devicetype == DBT_DEVTYP_VOLUME)
					{
						PDEV_BROADCAST_VOLUME lpdbv = (PDEV_BROADCAST_VOLUME)lparam;

						if (wparam == DBT_DEVICEARRIVAL)
						{
							if (_wfp_isfiltersinstalled () && !_wfp_isfiltersapplying ())
							{
								_r_fastlock_acquireshared (&lock_access);
								bool is_appexist = _app_isapphavedrive (FirstDriveFromMask (lpdbv->dbcv_unitmask));
								_r_fastlock_releaseshared (&lock_access);

								if (is_appexist)
									_app_changefilters (hwnd, true, false);
							}
							else
							{
								if (IsWindowVisible (hwnd))
									_r_listview_redraw (hwnd, _app_gettab_id (hwnd));
							}
						}
						else if (wparam == DBT_DEVICEREMOVECOMPLETE)
						{
							if (IsWindowVisible (hwnd))
								_r_listview_redraw (hwnd, _app_gettab_id (hwnd));
						}
					}

					break;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			if (HIWORD (wparam) == 0 && LOWORD (wparam) >= IDX_LANGUAGE && LOWORD (wparam) <= IDX_LANGUAGE + app.LocaleGetCount ())
			{
				app.LocaleApplyFromMenu (GetSubMenu (GetSubMenu (GetMenu (hwnd), 2), LANG_MENU), LOWORD (wparam), IDX_LANGUAGE);
				return FALSE;
			}
			else if ((LOWORD (wparam) >= IDX_RULES_SPECIAL && LOWORD (wparam) <= IDX_RULES_SPECIAL + rules_arr.size ()))
			{
				const INT app_listview_id = _app_gettab_id (hwnd);

				if (!SendDlgItemMessage (hwnd, app_listview_id, LVM_GETSELECTEDCOUNT, 0, 0))
					return FALSE;

				INT item = INVALID_INT;
				BOOL is_remove = INVALID_INT;

				const size_t rule_idx = (LOWORD (wparam) - IDX_RULES_SPECIAL);
				PR_OBJECT ptr_rule_object = _app_getrulebyid (rule_idx);

				if (!ptr_rule_object)
					return FALSE;

				PITEM_RULE ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

				if (ptr_rule)
				{
					while ((item = (INT)SendDlgItemMessage (hwnd, app_listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
					{
						const size_t app_hash = _r_listview_getitemlparam (hwnd, app_listview_id, item);

						if (ptr_rule->is_forservices && (app_hash == config.ntoskrnl_hash || app_hash == config.svchost_hash))
							continue;

						PR_OBJECT ptr_app_object = _app_getappitem (app_hash);

						if (!ptr_app_object)
							continue;

						PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

						if (ptr_app)
						{
							_app_freenotify (app_hash, ptr_app);

							if (is_remove == INVALID_INT)
								is_remove = ptr_rule->is_enabled && (ptr_rule->apps.find (app_hash) != ptr_rule->apps.end ());

							if (is_remove)
							{
								ptr_rule->apps.erase (app_hash);

								if (ptr_rule->apps.empty ())
									_app_ruleenable (ptr_rule, false);
							}
							else
							{
								ptr_rule->apps[app_hash] = true;

								_app_ruleenable (ptr_rule, true);
							}

							_r_fastlock_acquireshared (&lock_checkbox);
							_app_setappiteminfo (hwnd, app_listview_id, item, app_hash, ptr_app);
							_r_fastlock_releaseshared (&lock_checkbox);
						}

						_r_obj_dereference (ptr_app_object);
					}

					const INT rule_listview_id = _app_getlistview_id (ptr_rule->type);

					if (rule_listview_id)
					{
						const INT item_pos = _app_getposition (hwnd, rule_listview_id, rule_idx);

						if (item_pos != INVALID_INT)
						{
							_r_fastlock_acquireshared (&lock_checkbox);
							_app_setruleiteminfo (hwnd, rule_listview_id, item_pos, ptr_rule, true);
							_r_fastlock_releaseshared (&lock_checkbox);
						}
					}

					OBJECTS_VEC rules;
					rules.push_back (ptr_rule_object);

					_wfp_create4filters (_wfp_getenginehandle (), rules, __LINE__);
				}

				_r_obj_dereference (ptr_rule_object);

				_app_listviewsort (hwnd, app_listview_id);

				_app_refreshstatus (hwnd);
				_app_profile_save ();

				return FALSE;
			}
			else if ((LOWORD (wparam) >= IDX_TIMER && LOWORD (wparam) <= IDX_TIMER + timers.size ()))
			{
				const INT listview_id = _app_gettab_id (hwnd);

				if (!SendDlgItemMessage (hwnd, listview_id, LVM_GETSELECTEDCOUNT, 0, 0))
					return FALSE;

				const size_t timer_idx = (LOWORD (wparam) - IDX_TIMER);
				const time_t seconds = timers.at (timer_idx);
				INT item = INVALID_INT;
				OBJECTS_VEC rules;

				while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
				{
					const size_t app_hash = _r_listview_getitemlparam (hwnd, listview_id, item);

					_r_fastlock_acquireshared (&lock_access);
					PR_OBJECT ptr_app_object = _app_getappitem (app_hash);
					_r_fastlock_releaseshared (&lock_access);

					if (!ptr_app_object)
						continue;

					PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

					if (ptr_app)
					{
						_app_timer_set (hwnd, ptr_app, seconds);
						rules.push_back (ptr_app_object);
					}
					else
					{
						_r_obj_dereference (ptr_app_object);
					}
				}

				_wfp_create3filters (_wfp_getenginehandle (), rules, __LINE__);
				_app_freeobjects_vec (rules);

				_app_listviewsort (hwnd, listview_id);

				_app_refreshstatus (hwnd);
				_app_profile_save ();

				return FALSE;
			}

			switch (LOWORD (wparam))
			{
				case IDCANCEL: // process Esc key
				case IDM_TRAY_SHOW:
				{
					_r_wnd_toggle (hwnd, false);
					break;
				}

				case IDM_SETTINGS:
				case IDM_TRAY_SETTINGS:
				{
					app.CreateSettingsWindow (&SettingsProc);
					break;
				}

				case IDM_EXIT:
				case IDM_TRAY_EXIT:
				{
					SendMessage (hwnd, WM_CLOSE, 0, 0);
					break;
				}

				case IDM_WEBSITE:
				case IDM_TRAY_WEBSITE:
				{
					ShellExecute (hwnd, nullptr, _APP_WEBSITE_URL, nullptr, nullptr, SW_SHOWDEFAULT);
					break;
				}

				case IDM_CHECKUPDATES:
				{
					app.UpdateCheck (true);
					break;
				}

				case IDM_DONATE:
				{
					ShellExecute (hwnd, nullptr, _r_fmt (_APP_DONATE_URL, APP_NAME_SHORT), nullptr, nullptr, SW_SHOWDEFAULT);
					break;
				}

				case IDM_ABOUT:
				case IDM_TRAY_ABOUT:
				{
					app.CreateAboutWindow (hwnd);
					break;
				}

				case IDM_IMPORT:
				{
					WCHAR path[MAX_PATH] = {0};
					_r_str_copy (path, _countof (path), XML_PROFILE);

					WCHAR title[MAX_PATH] = {0};
					_r_str_printf (title, _countof (title), L"%s %s...", app.LocaleString (IDS_IMPORT, nullptr).GetString (), path);

					OPENFILENAME ofn = {0};

					ofn.lStructSize = sizeof (ofn);
					ofn.hwndOwner = hwnd;
					ofn.lpstrFile = path;
					ofn.nMaxFile = _countof (path);
					ofn.lpstrFilter = L"*.xml;*.xml.bak\0*.xml;*.xml.bak\0*.*\0*.*\0\0";
					ofn.lpstrDefExt = L"xml";
					ofn.lpstrTitle = title;
					ofn.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_PATHMUSTEXIST | OFN_FORCESHOWHIDDEN;

					if (GetOpenFileName (&ofn))
					{
						if (!_app_profile_load_check (path, XmlProfileV3, true))
						{
							_r_msg (hwnd, MB_OK | MB_ICONERROR, APP_NAME, L"Profile loading error!", L"File \"%s\" is incorrect!", path);
						}
						else
						{
							_app_profile_save (config.profile_path_backup); // made backup
							_app_profile_load (hwnd, path); // load profile

							_app_refreshstatus (hwnd);

							_app_changefilters (hwnd, true, false);
						}
					}

					break;
				}

				case IDM_EXPORT:
				{
					WCHAR path[MAX_PATH] = {0};
					_r_str_copy (path, _countof (path), XML_PROFILE);

					WCHAR title[MAX_PATH] = {0};
					_r_str_printf (title, _countof (title), L"%s %s...", app.LocaleString (IDS_EXPORT, nullptr).GetString (), path);

					OPENFILENAME ofn = {0};

					ofn.lStructSize = sizeof (ofn);
					ofn.hwndOwner = hwnd;
					ofn.lpstrFile = path;
					ofn.nMaxFile = _countof (path);
					ofn.lpstrFilter = L"*.xml\0*.xml\0\0";
					ofn.lpstrDefExt = L"xml";
					ofn.lpstrTitle = title;
					ofn.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

					if (GetSaveFileName (&ofn))
					{
						_app_profile_save (path);
					}

					break;
				}

				case IDM_ALWAYSONTOP_CHK:
				{
					const bool new_val = !app.ConfigGet (L"AlwaysOnTop", false).AsBool ();

					CheckMenuItem (GetMenu (hwnd), LOWORD (wparam), MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));
					app.ConfigSet (L"AlwaysOnTop", new_val);

					_r_wnd_top (hwnd, new_val);

					break;
				}

				case IDM_AUTOSIZECOLUMNS_CHK:
				{
					const bool new_val = !app.ConfigGet (L"AutoSizeColumns", true).AsBool ();

					CheckMenuItem (GetMenu (hwnd), LOWORD (wparam), MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));
					app.ConfigSet (L"AutoSizeColumns", new_val);

					if (new_val)
						_app_listviewresize (hwnd, _app_gettab_id (hwnd));

					break;
				}

				case IDM_SHOWFILENAMESONLY_CHK:
				{
					const bool new_val = !app.ConfigGet (L"ShowFilenames", true).AsBool ();

					CheckMenuItem (GetMenu (hwnd), LOWORD (wparam), MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));
					app.ConfigSet (L"ShowFilenames", new_val);

					_r_fastlock_acquireshared (&lock_access);

					// regroup apps
					for (auto &p : apps)
					{
						PR_OBJECT ptr_app_object = _r_obj_reference (p.second);

						if (!ptr_app_object)
							continue;

						const size_t app_hash = p.first;
						PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

						if (ptr_app)
						{
							_app_getdisplayname (app_hash, ptr_app, &ptr_app->display_name);

							const INT listview_id = _app_getlistview_id (ptr_app->type);

							if (listview_id)
							{
								const INT item_pos = _app_getposition (hwnd, listview_id, app_hash);

								if (item_pos != INVALID_INT)
								{
									_r_fastlock_acquireshared (&lock_checkbox);
									_app_setappiteminfo (hwnd, listview_id, item_pos, app_hash, ptr_app);
									_r_fastlock_releaseshared (&lock_checkbox);
								}
							}
						}

						_r_obj_dereference (ptr_app_object);
					}

					_r_fastlock_releaseshared (&lock_access);

					_app_listviewsort (hwnd, _app_gettab_id (hwnd));

					break;
				}

				case IDM_ENABLESPECIALGROUP_CHK:
				{
					const bool new_val = !app.ConfigGet (L"IsEnableSpecialGroup", true).AsBool ();

					CheckMenuItem (GetMenu (hwnd), LOWORD (wparam), MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));
					app.ConfigSet (L"IsEnableSpecialGroup", new_val);

					_r_fastlock_acquireshared (&lock_access);

					// regroup apps
					for (auto &p : apps)
					{
						PR_OBJECT ptr_app_object = _r_obj_reference (p.second);

						if (!ptr_app_object)
							continue;

						const size_t app_hash = p.first;
						PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

						if (ptr_app)
						{
							const INT listview_id = _app_getlistview_id (ptr_app->type);

							if (listview_id)
							{
								const INT item_pos = _app_getposition (hwnd, listview_id, app_hash);

								if (item_pos != INVALID_INT)
								{
									_r_fastlock_acquireshared (&lock_checkbox);
									_app_setappiteminfo (hwnd, listview_id, item_pos, app_hash, ptr_app);
									_r_fastlock_releaseshared (&lock_checkbox);
								}
							}
						}

						_r_obj_dereference (ptr_app_object);
					}

					// regroup rules
					for (size_t i = 0; i < rules_arr.size (); i++)
					{
						PR_OBJECT ptr_rule_object = _r_obj_reference (rules_arr.at (i));

						if (!ptr_rule_object)
							continue;

						const PITEM_RULE ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

						if (ptr_rule)
						{
							const INT listview_id = _app_getlistview_id (ptr_rule->type);

							if (listview_id)
							{
								const INT item_pos = _app_getposition (hwnd, listview_id, i);

								if (item_pos != INVALID_INT)
								{
									_r_fastlock_acquireshared (&lock_checkbox);
									_app_setruleiteminfo (hwnd, listview_id, item_pos, ptr_rule, false);
									_r_fastlock_releaseshared (&lock_checkbox);
								}
							}
						}

						_r_obj_dereference (ptr_rule_object);
					}

					_r_fastlock_releaseshared (&lock_access);

					_app_listviewsort (hwnd, _app_gettab_id (hwnd));
					_app_refreshstatus (hwnd);

					break;
				}

				case IDM_ICONSSMALL:
				case IDM_ICONSLARGE:
				case IDM_ICONSEXTRALARGE:
				{
					const INT listview_id = _app_gettab_id (hwnd);

					INT icon_size;

					if (LOWORD (wparam) == IDM_ICONSLARGE)
						icon_size = SHIL_LARGE;

					else if (LOWORD (wparam) == IDM_ICONSEXTRALARGE)
						icon_size = SHIL_EXTRALARGE;

					else
						icon_size = SHIL_SYSSMALL;

					CheckMenuRadioItem (GetMenu (hwnd), IDM_ICONSSMALL, IDM_ICONSEXTRALARGE, LOWORD (wparam), MF_BYCOMMAND);
					app.ConfigSet (L"IconSize", std::clamp (icon_size, SHIL_LARGE, SHIL_LAST));

					_app_listviewsetview (hwnd, listview_id);
					_app_listviewresize (hwnd, listview_id);

					_r_listview_redraw (hwnd, listview_id);

					break;
				}

				case IDM_ICONSISTABLEVIEW:
				{
					const bool new_val = !app.ConfigGet (L"IsTableView", true).AsBool ();

					CheckMenuItem (GetMenu (hwnd), LOWORD (wparam), MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));
					app.ConfigSet (L"IsTableView", new_val);

					const INT listview_id = _app_gettab_id (hwnd);

					if (listview_id)
					{
						_app_listviewsetview (hwnd, listview_id);
						_app_listviewresize (hwnd, listview_id);

						_r_listview_redraw (hwnd, listview_id);
					}

					break;
				}

				case IDM_ICONSISHIDDEN:
				{
					const bool new_val = !app.ConfigGet (L"IsIconsHidden", false).AsBool ();

					CheckMenuItem (GetMenu (hwnd), LOWORD (wparam), MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));
					app.ConfigSet (L"IsIconsHidden", new_val);

					_r_fastlock_acquireshared (&lock_access);

					for (auto &p : apps)
					{
						PR_OBJECT ptr_app_object = _r_obj_reference (p.second);

						if (!ptr_app_object)
							continue;

						const size_t app_hash = p.first;
						PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

						if (ptr_app)
						{
							const INT listview_id = _app_getlistview_id (ptr_app->type);

							if (listview_id)
							{
								const INT item_pos = _app_getposition (hwnd, listview_id, app_hash);

								if (item_pos != INVALID_INT)
								{
									_r_fastlock_acquireshared (&lock_checkbox);
									_app_setappiteminfo (hwnd, listview_id, item_pos, app_hash, ptr_app);
									_r_fastlock_releaseshared (&lock_checkbox);
								}
							}
						}

						_r_obj_dereference (ptr_app_object);
					}

					_r_fastlock_releaseshared (&lock_access);

					break;
				}

				case IDM_FONT:
				{
					CHOOSEFONT cf = {0};

					LOGFONT lf = {0};

					cf.lStructSize = sizeof (cf);
					cf.hwndOwner = hwnd;
					cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_NOSCRIPTSEL | CF_LIMITSIZE | CF_NOVERTFONTS;
					cf.nSizeMax = 16;
					cf.nSizeMin = 8;
					cf.lpLogFont = &lf;

					_app_listviewinitfont (&lf);

					if (ChooseFont (&cf))
					{
						app.ConfigSet (L"Font", !_r_str_isempty (lf.lfFaceName) ? _r_fmt (L"%s;%d;%d", lf.lfFaceName, _r_dc_fontheighttosize (lf.lfHeight), lf.lfWeight) : UI_FONT_DEFAULT);

						SAFE_DELETE_OBJECT (config.hfont);

						for (INT i = 0; i < (INT)SendDlgItemMessage (hwnd, IDC_TAB, TCM_GETITEMCOUNT, 0, 0); i++)
							_app_listviewsetfont (hwnd, _app_gettab_id (hwnd, i), false);

						_app_listviewresize (hwnd, _app_gettab_id (hwnd));

						RedrawWindow (hwnd, nullptr, nullptr, RDW_NOFRAME | RDW_NOINTERNALPAINT | RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
					}

					break;
				}

				case IDM_FIND:
				{
					if (!config.hfind)
					{
						static FINDREPLACE fr = {0}; // "static" is required for WM_FINDMSGSTRING

						fr.lStructSize = sizeof (fr);
						fr.hwndOwner = hwnd;
						fr.lpstrFindWhat = config.search_string;
						fr.wFindWhatLen = _countof (config.search_string) - 1;
						fr.Flags = FR_HIDEWHOLEWORD | FR_HIDEMATCHCASE | FR_HIDEUPDOWN;

						config.hfind = FindText (&fr);
					}
					else
					{
						SetFocus (config.hfind);
					}

					break;
				}

				case IDM_FINDNEXT:
				{
					if (_r_str_isempty (config.search_string))
					{
						SendMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDM_FIND, 0), 0);
					}
					else
					{
						FINDREPLACE fr = {0};

						fr.Flags = FR_FINDNEXT;
						fr.lpstrFindWhat = config.search_string;

						SendMessage (hwnd, WM_FINDMSGSTRING, 0, (LPARAM)&fr);
					}

					break;
				}

				case IDM_REFRESH:
				{
					if (_wfp_isfiltersapplying ())
						break;

					app.ConfigInit ();

					_app_profile_load (hwnd);
					_app_refreshstatus (hwnd);

					_app_changefilters (hwnd, true, false);

					break;
				}

				case IDM_RULE_BLOCKOUTBOUND:
				case IDM_RULE_BLOCKINBOUND:
				case IDM_RULE_ALLOWLOOPBACK:
				case IDM_RULE_ALLOW6TO4:
				case IDM_SECUREFILTERS_CHK:
				case IDM_USESTEALTHMODE_CHK:
				case IDM_INSTALLBOOTTIMEFILTERS_CHK:
				case IDM_USENETWORKRESOLUTION_CHK:
				case IDM_USECERTIFICATES_CHK:
				case IDM_USEREFRESHDEVICES_CHK:
				{
					_app_config_apply (hwnd, LOWORD (wparam));
					break;
				}

				case IDM_BLOCKLIST_SPY_DISABLE:
				case IDM_BLOCKLIST_SPY_ALLOW:
				case IDM_BLOCKLIST_SPY_BLOCK:
				case IDM_BLOCKLIST_UPDATE_DISABLE:
				case IDM_BLOCKLIST_UPDATE_ALLOW:
				case IDM_BLOCKLIST_UPDATE_BLOCK:
				case IDM_BLOCKLIST_EXTRA_DISABLE:
				case IDM_BLOCKLIST_EXTRA_ALLOW:
				case IDM_BLOCKLIST_EXTRA_BLOCK:
				{
					const INT ctrl_id = LOWORD (wparam);
					const HMENU hmenu = GetMenu (hwnd);

					if (ctrl_id >= IDM_BLOCKLIST_SPY_DISABLE && ctrl_id <= IDM_BLOCKLIST_SPY_BLOCK)
					{
						CheckMenuRadioItem (hmenu, IDM_BLOCKLIST_SPY_DISABLE, IDM_BLOCKLIST_SPY_BLOCK, ctrl_id, MF_BYCOMMAND);

						const INT new_state = std::clamp (ctrl_id - IDM_BLOCKLIST_SPY_DISABLE, 0, 2);

						app.ConfigSet (L"BlocklistSpyState", new_state);

						_r_fastlock_acquireshared (&lock_access);
						_app_ruleblocklistset (hwnd, new_state, INVALID_INT, INVALID_INT, true);
						_r_fastlock_releaseshared (&lock_access);
					}
					else if (ctrl_id >= IDM_BLOCKLIST_UPDATE_DISABLE && ctrl_id <= IDM_BLOCKLIST_UPDATE_BLOCK)
					{
						CheckMenuRadioItem (hmenu, IDM_BLOCKLIST_UPDATE_DISABLE, IDM_BLOCKLIST_UPDATE_BLOCK, ctrl_id, MF_BYCOMMAND);

						const INT new_state = std::clamp (ctrl_id - IDM_BLOCKLIST_UPDATE_DISABLE, 0, 2);

						app.ConfigSet (L"BlocklistUpdateState", new_state);

						_r_fastlock_acquireshared (&lock_access);
						_app_ruleblocklistset (hwnd, INVALID_INT, new_state, INVALID_INT, true);
						_r_fastlock_releaseshared (&lock_access);
					}
					else if (ctrl_id >= IDM_BLOCKLIST_EXTRA_DISABLE && ctrl_id <= IDM_BLOCKLIST_EXTRA_BLOCK)
					{
						CheckMenuRadioItem (hmenu, IDM_BLOCKLIST_EXTRA_DISABLE, IDM_BLOCKLIST_EXTRA_BLOCK, ctrl_id, MF_BYCOMMAND);

						const INT new_state = std::clamp (ctrl_id - IDM_BLOCKLIST_EXTRA_DISABLE, 0, 2);

						app.ConfigSet (L"BlocklistExtraState", new_state);

						_r_fastlock_acquireshared (&lock_access);
						_app_ruleblocklistset (hwnd, INVALID_INT, INVALID_INT, new_state, true);
						_r_fastlock_releaseshared (&lock_access);
					}

					break;
				}

				case IDM_TRAY_ENABLELOG_CHK:
				{
					const bool new_val = !app.ConfigGet (L"IsLogEnabled", false).AsBool ();

					_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, LOWORD (wparam), nullptr, 0, new_val ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED);
					app.ConfigSet (L"IsLogEnabled", new_val);

					_app_loginit (new_val);

					break;
				}

				case IDM_TRAY_ENABLENOTIFICATIONS_CHK:
				{
					const bool new_val = !app.ConfigGet (L"IsNotificationsEnabled", true).AsBool ();

					_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, LOWORD (wparam), nullptr, 0, new_val ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED);
					app.ConfigSet (L"IsNotificationsEnabled", new_val);

					_app_notifyrefresh (config.hnotification, true);

					break;
				}

				case IDM_TRAY_ENABLENOTIFICATIONSSOUND_CHK:
				{
					const bool new_val = !app.ConfigGet (L"IsNotificationsSound", true).AsBool ();

					app.ConfigSet (L"IsNotificationsSound", new_val);

					break;
				}

				case IDM_TRAY_NOTIFICATIONONTRAY_CHK:
				{
					const bool new_val = !app.ConfigGet (L"IsNotificationsOnTray", false).AsBool ();

					app.ConfigSet (L"IsNotificationsOnTray", new_val);

					if (IsWindowVisible (config.hnotification))
						_app_notifysetpos (config.hnotification, true);

					break;
				}

				case IDM_TRAY_LOGSHOW:
				{
					const rstring path = _r_path_expand (app.ConfigGet (L"LogPath", LOG_PATH_DEFAULT));

					if (!_r_fs_exists (path))
						return FALSE;

					if (config.hlogfile != nullptr && config.hlogfile != INVALID_HANDLE_VALUE)
						FlushFileBuffers (config.hlogfile);

					_r_run (nullptr, _r_fmt (L"%s \"%s\"", _app_getlogviewer ().GetString (), path.GetString ()));

					break;
				}

				case IDM_TRAY_LOGCLEAR:
				{
					const rstring path = _r_path_expand (app.ConfigGet (L"LogPath", LOG_PATH_DEFAULT));

					if ((config.hlogfile != nullptr && config.hlogfile != INVALID_HANDLE_VALUE) || _r_fs_exists (path) || InterlockedCompareExchange (&log_stack.item_count, 0, 0))
					{
						if (!app.ConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION, nullptr), L"ConfirmLogClear"))
							break;

						_app_freelogstack ();

						_r_fastlock_acquireexclusive (&lock_writelog);
						_app_logclear ();
						_r_fastlock_releaseexclusive (&lock_writelog);
					}

					break;
				}

				case IDM_TRAY_LOGSHOW_ERR:
				{
					const rstring path = _r_dbg_getpath ();

					if (_r_fs_exists (path))
						_r_run (nullptr, _r_fmt (L"%s \"%s\"", _app_getlogviewer ().GetString (), path.GetString ()));

					break;
				}

				case IDM_TRAY_LOGCLEAR_ERR:
				{
					static const rstring path = _r_dbg_getpath ();

					if (!_r_fs_exists (path) || !app.ConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION, nullptr), L"ConfirmLogClear"))
						break;

					_r_fs_delete (path, false);

					break;
				}

				case IDM_TRAY_START:
				{
					if (_wfp_isfiltersapplying ())
						break;

					const bool is_filtersinstalled = !_wfp_isfiltersinstalled ();

					if (_app_installmessage (hwnd, is_filtersinstalled))
						_app_changefilters (hwnd, is_filtersinstalled, true);

					break;
				}

				case IDM_ADD_FILE:
				{
					WCHAR files[_R_BUFFER_LENGTH] = {0};
					OPENFILENAME ofn = {0};

					ofn.lStructSize = sizeof (ofn);
					ofn.hwndOwner = hwnd;
					ofn.lpstrFile = files;
					ofn.nMaxFile = _countof (files);
					ofn.lpstrFilter = L"*.exe\0*.exe\0*.*\0*.*\0\0";
					ofn.Flags = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_ENABLESIZING | OFN_PATHMUSTEXIST | OFN_FORCESHOWHIDDEN;

					if (GetOpenFileName (&ofn))
					{
						size_t app_hash = 0;

						if (files[ofn.nFileOffset - 1] != 0)
						{
							_r_fastlock_acquireexclusive (&lock_access);
							app_hash = _app_addapplication (hwnd, files, 0, 0, 0, false, false, false);
							_r_fastlock_releaseexclusive (&lock_access);
						}
						else
						{
							LPWSTR p = files;
							WCHAR dir[MAX_PATH] = {0};
							GetCurrentDirectory (_countof (dir), dir);

							while (*p)
							{
								p += _r_str_length (p) + 1;

								if (*p)
								{
									_r_fastlock_acquireexclusive (&lock_access);
									app_hash = _app_addapplication (hwnd, _r_fmt (L"%s\\%s", dir, p), 0, 0, 0, false, false, false);
									_r_fastlock_releaseexclusive (&lock_access);
								}
							}
						}

						{
							INT app_listview_id = 0;

							if (_app_getappinfo (app_hash, InfoListviewId, &app_listview_id, sizeof (app_listview_id)))
							{
								if (app_listview_id == _app_gettab_id (hwnd))
									_app_listviewsort (hwnd, app_listview_id);

								_app_showitem (hwnd, app_listview_id, _app_getposition (hwnd, app_listview_id, app_hash));
							}
						}

						_app_refreshstatus (hwnd);
						_app_profile_save ();
					}

					break;
				}

				case IDM_DISABLENOTIFICATIONS:
				case IDM_DISABLETIMER:
				case IDM_EXPLORE:
				{
					const INT listview_id = _app_gettab_id (hwnd);
					const INT ctrl_id = LOWORD (wparam);

					if (
						listview_id != IDC_APPS_PROFILE &&
						listview_id != IDC_APPS_SERVICE &&
						listview_id != IDC_APPS_UWP &&
						listview_id != IDC_NETWORK
						)
					{
						// note: these commands only for profile...
						break;
					}

					INT item = INVALID_INT;
					BOOL new_val = BOOL (-1);

					while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
					{
						size_t app_hash = 0;

						if (listview_id == IDC_NETWORK)
						{
							const size_t network_hash = _r_listview_getitemlparam (hwnd, listview_id, item);
							PR_OBJECT ptr_network_object = _app_getnetworkitem (network_hash);

							if (!ptr_network_object)
								continue;

							PITEM_NETWORK ptr_network = (PITEM_NETWORK)ptr_network_object->pdata;

							if (ptr_network)
							{
								app_hash = ptr_network->app_hash;

								if (!_app_isappfound (app_hash))
								{
									_r_path_explore (ptr_network->path);
									_r_obj_dereference (ptr_network_object);

									continue;
								}
							}

							_r_obj_dereference (ptr_network_object);
						}
						else
						{
							app_hash = _r_listview_getitemlparam (hwnd, listview_id, item);
						}

						_r_fastlock_acquireshared (&lock_access);
						PR_OBJECT ptr_app_object = _app_getappitem (app_hash);
						_r_fastlock_releaseshared (&lock_access);

						if (!ptr_app_object)
							continue;

						PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

						if (!ptr_app)
						{
							_r_obj_dereference (ptr_app_object);
							continue;
						}

						if (ctrl_id == IDM_EXPLORE)
						{
							if (ptr_app->type != DataAppPico && ptr_app->type != DataAppDevice)
								_r_path_explore (ptr_app->real_path);
						}
						else if (ctrl_id == IDM_DISABLENOTIFICATIONS)
						{
							if (new_val == BOOL (-1))
								new_val = !ptr_app->is_silent;

							if ((BOOL)ptr_app->is_silent != new_val)
								ptr_app->is_silent = new_val;

							if (new_val)
								_app_freenotify (app_hash, ptr_app);
						}
						else if (ctrl_id == IDM_DISABLETIMER)
						{
							_app_timer_reset (hwnd, ptr_app);
						}

						_r_obj_dereference (ptr_app_object);
					}

					if (
						ctrl_id == IDM_DISABLETIMER ||
						ctrl_id == IDM_DISABLENOTIFICATIONS
						)
					{
						_app_listviewsort (hwnd, listview_id);

						_app_refreshstatus (hwnd);
						_app_profile_save ();
					}

					break;
				}

				case IDM_COPY:
				case IDM_COPY2:
				{
					const INT listview_id = _app_gettab_id (hwnd);
					INT item = INVALID_INT;

					const INT column_count = _r_listview_getcolumncount (hwnd, listview_id);
					const INT column_current = (INT)lparam;
					const rstring divider = _r_fmt (L"%c ", DIVIDER_COPY);

					rstring buffer;

					while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
					{
						if (LOWORD (wparam) == IDM_COPY)
						{
							for (INT column_id = 0; column_id < column_count; column_id++)
								buffer.Append (_r_listview_getitemtext (hwnd, listview_id, item, column_id)).Append (divider);
						}
						else
						{
							buffer.Append (_r_listview_getitemtext (hwnd, listview_id, item, column_current)).Append (divider);
						}

						_r_str_trim (buffer, divider);
						buffer.Append (L"\r\n");
					}

					_r_str_trim (buffer, DIVIDER_TRIM);

					_r_clipboard_set (hwnd, buffer, buffer.GetLength ());

					break;
				}

				case IDM_CHECK:
				case IDM_UNCHECK:
				{
					const INT listview_id = _app_gettab_id (hwnd);
					const INT ctrl_id = LOWORD (wparam);

					const bool new_val = (LOWORD (wparam) == IDM_CHECK) ? true : false;

					bool is_changed = false;

					if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
					{
						OBJECTS_VEC rules;
						INT item = INVALID_INT;

						while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
						{
							const size_t app_hash = _r_listview_getitemlparam (hwnd, listview_id, item);
							PR_OBJECT ptr_app_object = _app_getappitem (app_hash);

							if (!ptr_app_object)
								continue;

							PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

							if (ptr_app && ptr_app->is_enabled != new_val)
							{
								if (!new_val)
									_app_timer_reset (hwnd, ptr_app);

								else
									_app_freenotify (app_hash, ptr_app);

								ptr_app->is_enabled = new_val;

								_r_fastlock_acquireshared (&lock_checkbox);
								_app_setappiteminfo (hwnd, listview_id, item, app_hash, ptr_app);
								_r_fastlock_releaseshared (&lock_checkbox);

								rules.push_back (ptr_app_object);

								is_changed = true;

								// do not reset reference counter
							}
							else
							{
								_r_obj_dereference (ptr_app_object);
							}
						}

						if (is_changed)
						{
							_wfp_create3filters (_wfp_getenginehandle (), rules, __LINE__);
							_app_freeobjects_vec (rules);
						}
					}
					else if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM)
					{
						OBJECTS_VEC rules;
						INT item = INVALID_INT;

						while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
						{
							const size_t rule_idx = _r_listview_getitemlparam (hwnd, listview_id, item);
							PR_OBJECT ptr_rule_object = _app_getrulebyid (rule_idx);

							if (!ptr_rule_object)
								continue;

							PITEM_RULE ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

							if (ptr_rule && ptr_rule->is_enabled != new_val)
							{
								_app_ruleenable (ptr_rule, new_val);

								_r_fastlock_acquireshared (&lock_checkbox);
								_app_setruleiteminfo (hwnd, listview_id, item, ptr_rule, true);
								_r_fastlock_releaseshared (&lock_checkbox);

								rules.push_back (ptr_rule_object);

								is_changed = true;

								// do not reset reference counter
							}
							else
							{
								_r_obj_dereference (ptr_rule_object);
							}
						}

						if (is_changed)
						{
							_wfp_create4filters (_wfp_getenginehandle (), rules, __LINE__);
							_app_freeobjects_vec (rules);
						}
					}

					if (is_changed)
					{
						_app_listviewsort (hwnd, listview_id);

						_app_refreshstatus (hwnd);
						_app_profile_save ();
					}

					break;
				}

				case IDM_EDITRULES:
				{
					_app_settab_id (hwnd, _app_getlistview_id (DataRuleCustom));
					break;
				}

				case IDM_OPENRULESEDITOR:
				{
					PITEM_RULE ptr_rule = new ITEM_RULE;
					PR_OBJECT ptr_rule_object = _r_obj_allocate (ptr_rule, &_app_dereferencerule);

					_app_ruleenable (ptr_rule, true);

					ptr_rule->type = DataRuleCustom;
					ptr_rule->is_block = false;

					const INT listview_id = _app_gettab_id (hwnd);

					if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
					{
						INT item = INVALID_INT;

						while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
						{
							const size_t app_hash = _r_listview_getitemlparam (hwnd, listview_id, item);

							if (app_hash)
								ptr_rule->apps[app_hash] = true;
						}
					}
					else if (listview_id == IDC_NETWORK)
					{
						INT item = INVALID_INT;

						rstring rule_remote;
						rstring rule_local;

						ptr_rule->is_block = true;

						while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
						{
							const size_t network_hash = _r_listview_getitemlparam (hwnd, listview_id, item);
							PR_OBJECT ptr_network_object = _app_getnetworkitem (network_hash);

							if (!ptr_network_object)
								continue;

							PITEM_NETWORK ptr_network = (PITEM_NETWORK)ptr_network_object->pdata;

							if (ptr_network)
							{
								if (!ptr_rule->pname)
								{
									const rstring item_text = _r_listview_getitemtext (hwnd, listview_id, item, 0);

									_r_str_alloc (&ptr_rule->pname, item_text.GetLength (), item_text);
								}

								if (ptr_network->app_hash && ptr_network->path)
								{
									if (!_app_isappfound (ptr_network->app_hash))
									{
										_r_fastlock_acquireexclusive (&lock_access);
										_app_addapplication (hwnd, ptr_network->path, 0, 0, 0, false, false, true);
										_r_fastlock_releaseexclusive (&lock_access);

										_app_refreshstatus (hwnd);
										_app_profile_save ();
									}

									ptr_rule->apps[ptr_network->app_hash] = true;
								}

								if (!ptr_rule->protocol && ptr_network->protocol)
									ptr_rule->protocol = ptr_network->protocol;

								LPWSTR fmt = nullptr;

								if (_app_formataddress (ptr_network->af, 0, &ptr_network->remote_addr, ptr_network->remote_port, &fmt, FMTADDR_AS_RULE))
									rule_remote.AppendFormat (L"%s" DIVIDER_RULE, fmt);

								if (_app_formataddress (ptr_network->af, 0, &ptr_network->local_addr, ptr_network->local_port, &fmt, FMTADDR_AS_RULE))
									rule_local.AppendFormat (L"%s" DIVIDER_RULE, fmt);

								SAFE_DELETE_ARRAY (fmt);
							}

							_r_obj_dereference (ptr_network_object);
						}

						_r_str_trim (rule_remote, DIVIDER_RULE);
						_r_str_trim (rule_local, DIVIDER_RULE);

						_r_str_alloc (&ptr_rule->prule_remote, rule_remote.GetLength (), rule_remote);
						_r_str_alloc (&ptr_rule->prule_local, rule_local.GetLength (), rule_local);
					}

					if (DialogBoxParam (nullptr, MAKEINTRESOURCE (IDD_EDITOR), hwnd, &EditorProc, (LPARAM)ptr_rule_object))
					{
						_r_fastlock_acquireexclusive (&lock_access);

						const size_t rule_idx = rules_arr.size ();
						rules_arr.push_back (ptr_rule_object);

						_r_fastlock_releaseexclusive (&lock_access);

						const INT listview_rules_id = _app_getlistview_id (DataRuleCustom);

						if (listview_rules_id)
						{
							const INT new_item = _r_listview_getitemcount (hwnd, listview_rules_id);

							_r_fastlock_acquireshared (&lock_checkbox);

							_r_listview_additem (hwnd, listview_rules_id, new_item, 0, ptr_rule->pname, _app_getruleicon (ptr_rule), _app_getrulegroup (ptr_rule), rule_idx);
							_app_setruleiteminfo (hwnd, listview_rules_id, new_item, ptr_rule, true);

							_r_fastlock_releaseshared (&lock_checkbox);
						}

						_app_listviewsort (hwnd, listview_id);

						_app_refreshstatus (hwnd);
						_app_profile_save ();
					}
					else
					{
						_r_obj_dereference (ptr_rule_object);
					}

					break;
				}

				case IDM_PROPERTIES:
				{
					const INT listview_id = _app_gettab_id (hwnd);
					const INT item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)INVALID_INT, LVNI_SELECTED);

					if (item == INVALID_INT)
						break;

					if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
					{
						PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDM_EXPLORE, 0), 0);
					}
					else if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM)
					{
						const size_t rule_idx = _r_listview_getitemlparam (hwnd, listview_id, item);
						PR_OBJECT ptr_rule_object = _app_getrulebyid (rule_idx);

						if (!ptr_rule_object)
							break;

						if (DialogBoxParam (nullptr, MAKEINTRESOURCE (IDD_EDITOR), hwnd, &EditorProc, (LPARAM)ptr_rule_object))
						{
							PITEM_RULE ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

							if (ptr_rule)
							{
								_r_fastlock_acquireshared (&lock_checkbox);
								_app_setruleiteminfo (hwnd, listview_id, item, ptr_rule, true);
								_r_fastlock_releaseshared (&lock_checkbox);
							}

							_app_listviewsort (hwnd, listview_id);

							_app_refreshstatus (hwnd);
							_app_profile_save ();
						}

						_r_obj_dereference (ptr_rule_object);
					}
					else if (listview_id == IDC_NETWORK)
					{
						const size_t network_hash = _r_listview_getitemlparam (hwnd, listview_id, item);
						PR_OBJECT ptr_network_object = _app_getnetworkitem (network_hash);

						if (!ptr_network_object)
							break;

						PITEM_NETWORK ptr_network = (PITEM_NETWORK)ptr_network_object->pdata;

						if (ptr_network && ptr_network->app_hash && ptr_network->path)
						{
							const size_t app_hash = ptr_network->app_hash;

							if (!_app_isappfound (app_hash))
							{
								_r_fastlock_acquireshared (&lock_access);
								_app_addapplication (hwnd, ptr_network->path, 0, 0, 0, false, false, true);
								_r_fastlock_releaseshared (&lock_access);

								_app_refreshstatus (hwnd);
								_app_profile_save ();
							}

							INT app_listview_id = 0;

							if (_app_getappinfo (app_hash, InfoListviewId, &app_listview_id, sizeof (app_listview_id)))
							{
								_app_listviewsort (hwnd, app_listview_id);
								_app_showitem (hwnd, app_listview_id, _app_getposition (hwnd, app_listview_id, app_hash));
							}
						}

						_r_obj_dereference (ptr_network_object);
					}

					break;
				}

				case IDM_NETWORK_CLOSE:
				{
					const INT listview_id = _app_gettab_id (hwnd);

					if (listview_id != IDC_NETWORK)
						break;

					const INT selected = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETSELECTEDCOUNT, 0, 0);

					if (!selected)
						break;

					INT item = INVALID_INT;

					MIB_TCPROW tcprow;

					while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
					{
						const size_t network_hash = _r_listview_getitemlparam (hwnd, listview_id, item);
						PR_OBJECT ptr_network_object = _r_obj_reference (network_map[network_hash]);

						if (!ptr_network_object)
							continue;

						PITEM_NETWORK ptr_network = (PITEM_NETWORK)ptr_network_object->pdata;

						if (ptr_network)
						{
							if (ptr_network->af == AF_INET && ptr_network->state == MIB_TCP_STATE_ESTAB)
							{
								SecureZeroMemory (&tcprow, sizeof (tcprow));

								tcprow.dwState = MIB_TCP_STATE_DELETE_TCB;
								tcprow.dwLocalAddr = ptr_network->local_addr.S_un.S_addr;
								tcprow.dwLocalPort = _byteswap_ushort ((USHORT)ptr_network->local_port);
								tcprow.dwRemoteAddr = ptr_network->remote_addr.S_un.S_addr;
								tcprow.dwRemotePort = _byteswap_ushort ((USHORT)ptr_network->remote_port);

								if (SetTcpEntry (&tcprow) == NO_ERROR)
								{
									SendDlgItemMessage (hwnd, listview_id, LVM_DELETEITEM, (WPARAM)item, 0);

									network_map.erase (network_hash);
									_r_obj_dereferenceex (ptr_network_object, 2);

									continue;
								}
							}
						}

						_r_obj_dereference (ptr_network_object);
					}

					break;
				}

				case IDM_DELETE:
				{
					const INT listview_id = _app_gettab_id (hwnd);

					if (listview_id != IDC_APPS_PROFILE && listview_id != IDC_RULES_CUSTOM)
						break;

					const INT selected = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETSELECTEDCOUNT, 0, 0);

					if (!selected || _r_msg (hwnd, MB_YESNO | MB_ICONEXCLAMATION, APP_NAME, nullptr, app.LocaleString (IDS_QUESTION_DELETE, nullptr), selected) != IDYES)
						break;

					const INT count = _r_listview_getitemcount (hwnd, listview_id) - 1;

					GUIDS_VEC guids;

					for (INT i = count; i != INVALID_INT; i--)
					{
						if (ListView_GetItemState (GetDlgItem (hwnd, listview_id), i, LVNI_SELECTED))
						{
							if (listview_id == IDC_APPS_PROFILE)
							{
								const size_t app_hash = _r_listview_getitemlparam (hwnd, listview_id, i);
								PR_OBJECT ptr_app_object = _app_getappitem (app_hash);

								if (!ptr_app_object)
									continue;

								PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

								if (ptr_app && !ptr_app->is_undeletable) // skip "undeletable" apps
								{
									guids.insert (guids.end (), ptr_app->guids.begin (), ptr_app->guids.end ());

									SendDlgItemMessage (hwnd, listview_id, LVM_DELETEITEM, (WPARAM)i, 0);

									_app_timer_reset (hwnd, ptr_app);
									_app_freenotify (app_hash, ptr_app);

									_r_fastlock_acquireexclusive (&lock_access);
									_app_freeapplication (app_hash);
									_r_fastlock_releaseexclusive (&lock_access);

									_r_obj_dereferenceex (ptr_app_object, 2);
								}
								else
								{
									_r_obj_dereference (ptr_app_object);
								}
							}
							else if (listview_id == IDC_RULES_CUSTOM)
							{
								const size_t rule_idx = _r_listview_getitemlparam (hwnd, listview_id, i);
								PR_OBJECT ptr_rule_object = _app_getrulebyid (rule_idx);

								if (!ptr_rule_object)
									continue;

								PITEM_RULE ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

								HASHER_MAP apps_checker;

								if (ptr_rule && !ptr_rule->is_readonly) // skip "read-only" rules
								{
									guids.insert (guids.end (), ptr_rule->guids.begin (), ptr_rule->guids.end ());

									for (auto &p : ptr_rule->apps)
										apps_checker[p.first] = true;

									SendDlgItemMessage (hwnd, listview_id, LVM_DELETEITEM, (WPARAM)i, 0);

									_r_fastlock_acquireshared (&lock_access);
									rules_arr.at (rule_idx) = nullptr;
									_r_fastlock_releaseshared (&lock_access);

									_r_obj_dereferenceex (ptr_rule_object, 2);
								}
								else
								{
									_r_obj_dereference (ptr_rule_object);
								}

								for (auto &p : apps_checker)
								{
									const size_t app_hash = p.first;
									PR_OBJECT ptr_app_object = _app_getappitem (app_hash);

									if (!ptr_app_object)
										continue;

									PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

									if (ptr_app)
									{
										const INT app_listview_id = _app_getlistview_id (ptr_app->type);

										if (app_listview_id)
										{
											const INT item_pos = _app_getposition (hwnd, app_listview_id, app_hash);

											if (item_pos != INVALID_INT)
											{
												_r_fastlock_acquireshared (&lock_checkbox);
												_app_setappiteminfo (hwnd, app_listview_id, item_pos, app_hash, ptr_app);
												_r_fastlock_releaseshared (&lock_checkbox);
											}
										}
									}

									_r_obj_dereference (ptr_app_object);
								}
							}
						}
					}

					_wfp_destroyfilters_array (_wfp_getenginehandle (), guids, __LINE__);

					_app_refreshstatus (hwnd);
					_app_profile_save ();

					break;
				}

				case IDM_PURGE_UNUSED:
				{
					bool is_deleted = false;

					GUIDS_VEC guids;
					std::vector<size_t> apps_list;

					_r_fastlock_acquireshared (&lock_access);

					for (auto &p : apps)
					{
						PR_OBJECT ptr_app_object = _r_obj_reference (p.second);

						if (!ptr_app_object)
							continue;

						const size_t app_hash = p.first;
						PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

						if (ptr_app && !ptr_app->is_undeletable && (!_app_isappexists (ptr_app) || ((ptr_app->type != DataAppService && ptr_app->type != DataAppUWP) && !_app_isappused (ptr_app, app_hash))))
						{
							const INT app_listview_id = _app_getlistview_id (ptr_app->type);

							if (app_listview_id)
							{
								const INT item_pos = _app_getposition (hwnd, app_listview_id, app_hash);

								if (item_pos != INVALID_INT)
									SendDlgItemMessage (hwnd, app_listview_id, LVM_DELETEITEM, (WPARAM)item_pos, 0);
							}

							_app_timer_reset (hwnd, ptr_app);
							_app_freenotify (app_hash, ptr_app);

							guids.insert (guids.end (), ptr_app->guids.begin (), ptr_app->guids.end ());

							apps_list.push_back (app_hash);

							is_deleted = true;
						}

						_r_obj_dereference (ptr_app_object);
					}

					for (size_t i = 0; i < apps_list.size (); i++)
						_app_freeapplication (apps_list.at (i));

					_r_fastlock_releaseshared (&lock_access);

					if (is_deleted)
					{
						_wfp_destroyfilters_array (_wfp_getenginehandle (), guids, __LINE__);

						_app_refreshstatus (hwnd);
						_app_profile_save ();
					}

					break;
				}

				case IDM_PURGE_TIMERS:
				{
					if (!_app_istimersactive () || _r_msg (hwnd, MB_YESNO | MB_ICONEXCLAMATION, APP_NAME, nullptr, app.LocaleString (IDS_QUESTION_TIMERS, nullptr)) != IDYES)
						break;

					OBJECTS_VEC rules;

					_r_fastlock_acquireshared (&lock_access);

					for (auto &p : apps)
					{
						PR_OBJECT ptr_app_object = _r_obj_reference (p.second);

						if (!ptr_app_object)
							continue;

						PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

						if (ptr_app && _app_istimeractive (ptr_app))
						{
							_app_timer_reset (hwnd, ptr_app);

							rules.push_back (ptr_app_object);
						}
						else
						{
							_r_obj_dereference (ptr_app_object);
						}
					}

					_r_fastlock_releaseshared (&lock_access);

					_wfp_create3filters (_wfp_getenginehandle (), rules, __LINE__);
					_app_freeobjects_vec (rules);

					_app_listviewsort (hwnd, _app_gettab_id (hwnd));

					_app_refreshstatus (hwnd);
					_app_profile_save ();

					break;
				}

				case IDM_SELECT_ALL:
				{
					ListView_SetItemState (GetDlgItem (hwnd, _app_gettab_id (hwnd)), INVALID_INT, LVIS_SELECTED, LVIS_SELECTED);
					break;
				}

				case IDM_ZOOM:
				{
					ShowWindow (hwnd, IsZoomed (hwnd) ? SW_RESTORE : SW_MAXIMIZE);
					break;
				}

#if defined(_APP_BETA) || defined(_APP_BETA_RC)

#define FN_AD L"<test filter>"
#define RM_AD L"195.210.46.95"
#define RM_AD2 L"195.210.46.%d"
#define LM_AD2 L"192.168.1.%d"
#define RP_AD 443
#define LP_AD 65535

				// Here is debugging content

				case 998:
				{
					PITEM_LOG ptr_log = new ITEM_LOG;

					ptr_log->app_hash = config.my_hash;
					ptr_log->date = _r_unixtime_now ();

					ptr_log->af = AF_INET;
					ptr_log->protocol = IPPROTO_TCP;

					ptr_log->filter_id = 777;
					ptr_log->direction = FWP_DIRECTION_OUTBOUND;

					InetPton (ptr_log->af, RM_AD, &ptr_log->remote_addr);
					ptr_log->remote_port = RP_AD;

					_r_str_alloc (&ptr_log->path, INVALID_SIZE_T, app.GetBinaryPath ());
					_r_str_alloc (&ptr_log->filter_name, INVALID_SIZE_T, FN_AD);

					_app_formataddress (ptr_log->af, ptr_log->protocol, &ptr_log->remote_addr, 0, &ptr_log->remote_fmt, FMTADDR_USE_PROTOCOL | FMTADDR_RESOLVE_HOST);
					_app_formataddress (ptr_log->af, ptr_log->protocol, &ptr_log->local_addr, 0, &ptr_log->local_fmt, FMTADDR_USE_PROTOCOL | FMTADDR_RESOLVE_HOST);

					PR_OBJECT ptr_app_object = _app_getappitem (config.my_hash);

					if (ptr_app_object)
					{
						PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

						if (ptr_app)
						{
							ptr_app->last_notify = 0;

							_app_notifyadd (config.hnotification, _r_obj_allocate (ptr_log, &_app_dereferencelog), ptr_app);
						}

						_r_obj_dereference (ptr_app_object);
					}

					break;
				}

				case 999:
				{
					const UINT32 flags = FWPM_NET_EVENT_FLAG_APP_ID_SET |
						FWPM_NET_EVENT_FLAG_IP_PROTOCOL_SET |
						FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET |
						FWPM_NET_EVENT_FLAG_REMOTE_PORT_SET |
						FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET |
						FWPM_NET_EVENT_FLAG_LOCAL_PORT_SET |
						FWPM_NET_EVENT_FLAG_USER_ID_SET |
						FWPM_NET_EVENT_FLAG_IP_VERSION_SET;

					FILETIME ft = {0};
					GetSystemTimeAsFileTime (&ft);

					IN_ADDR ipv4_remote = {0};
					IN_ADDR ipv4_local = {0};

					rstring path = app.GetBinaryPath ();
					_r_path_ntpathfromdos (path);

					UINT16 layer_id = 0;
					UINT64 filter_id = 0;

					FWPM_LAYER *layer = nullptr;
					FWPM_FILTER *filter = nullptr;

					if (FwpmLayerGetByKey (config.hengine, &FWPM_LAYER_ALE_AUTH_CONNECT_V4, &layer) == ERROR_SUCCESS && layer)
						layer_id = layer->layerId;

					if (!filter_ids.empty ())
					{
						if (FwpmFilterGetByKey (config.hengine, &filter_ids.at (filter_ids.size () - 1), &filter) == ERROR_SUCCESS && filter)
							filter_id = filter->filterId;
					}

					if (layer)
						FwpmFreeMemory ((void **)&layer);

					if (filter)
						FwpmFreeMemory ((void **)&filter);

					LPCWSTR terminator = nullptr;

					for (UINT i = 0; i < 255; i++)
					{
						RtlIpv4StringToAddress (_r_fmt (RM_AD2, i + 1), TRUE, &terminator, &ipv4_remote);
						RtlIpv4StringToAddress (_r_fmt (LM_AD2, i + 1), TRUE, &terminator, &ipv4_local);

						UINT32 remote_addr = _byteswap_ulong (ipv4_remote.S_un.S_addr);
						UINT32 local_addr = _byteswap_ulong (ipv4_local.S_un.S_addr);

						_wfp_logcallback (flags, &ft, (UINT8*)path.GetString (), nullptr, (SID*)config.padminsid, IPPROTO_TCP, FWP_IP_VERSION_V4, &remote_addr, RP_AD, &local_addr, LP_AD, layer_id, filter_id, FWP_DIRECTION_OUTBOUND, false, false);
					}

					break;
				}
#endif // _APP_BETA || _APP_BETA_RC
			}

			break;
		}
	}

	return FALSE;
}

INT APIENTRY wWinMain (HINSTANCE, HINSTANCE, LPWSTR, INT)
{
	MSG msg = {0};

	// parse arguments
	{
		INT numargs = 0;
		LPWSTR* arga = CommandLineToArgvW (GetCommandLine (), &numargs);

		bool is_install = false;
		bool is_uninstall = false;
		bool is_silent = false;

		for (INT i = 0; i < numargs; i++)
		{
			if (_r_str_compare (arga[i], L"/install", 8) == 0)
				is_install = true;

			else if (_r_str_compare (arga[i], L"/uninstall", 10) == 0)
				is_uninstall = true;

			else if (_r_str_compare (arga[i], L"/silent", 7) == 0)
				is_silent = true;
		}

		SAFE_LOCAL_FREE (arga);

		if (is_install || is_uninstall)
		{
			if (is_install)
			{
				if (app.IsAdmin () && (is_silent || (!_wfp_isfiltersinstalled () && _app_installmessage (nullptr, true))))
				{
					_app_initialize ();
					_app_profile_load (nullptr);

					if (_wfp_initialize (true))
						_wfp_installfilters ();

					_wfp_uninitialize (false);
				}
			}
			else if (is_uninstall)
			{
				if (app.IsAdmin () && _wfp_isfiltersinstalled () && _app_installmessage (nullptr, false))
				{
					if (_wfp_initialize (false))
						_wfp_destroyfilters (_wfp_getenginehandle ());

					_wfp_uninitialize (true);
				}
			}

			return ERROR_SUCCESS;
		}
	}

	if (app.CreateMainWindow (IDD_MAIN, IDI_MAIN, &DlgProc))
	{
		const HACCEL haccel = LoadAccelerators (app.GetHINSTANCE (), MAKEINTRESOURCE (IDA_MAIN));

		if (haccel)
		{
			while (GetMessage (&msg, nullptr, 0, 0) > 0)
			{
				TranslateAccelerator (app.GetHWND (), haccel, &msg);

				if (!IsDialogMessage (app.GetHWND (), &msg))
				{
					TranslateMessage (&msg);
					DispatchMessage (&msg);
				}
			}

			DestroyAcceleratorTable (haccel);
		}
	}

	return (INT)msg.wParam;
}
