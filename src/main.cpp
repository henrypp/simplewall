// simplewall
// Copyright (c) 2016-2019 Henry++

#include "global.hpp"

rapp app (APP_NAME, APP_NAME_SHORT, APP_VERSION, APP_COPYRIGHT);

STATIC_DATA config;

FWPM_SESSION session;

MAPPS_MAP apps;
std::vector<PITEM_RULE> rules_arr;
std::unordered_map<size_t, PITEM_RULE_CONFIG> rules_config;

MCACHE_MAP cache_signatures;
MCACHE_MAP cache_versions;
MCACHE_MAP cache_dns;
MCACHE_MAP cache_hosts;
MCACHETYPES_MAP cache_types;

MTHREADPOOL threads_pool;

std::vector<PITEM_COLOR> colors;
std::vector<PITEM_PROTOCOL> protocols;
std::vector<PITEM_ADD> items;
std::vector<time_t> timers;

std::vector<PITEM_LOG> notifications;

MARRAY filter_ids;

ITEM_LIST_HEAD log_stack;

_R_FASTLOCK lock_access;
_R_FASTLOCK lock_apply;
_R_FASTLOCK lock_cache;
_R_FASTLOCK lock_checkbox;
_R_FASTLOCK lock_logbusy;
_R_FASTLOCK lock_logthread;
_R_FASTLOCK lock_notification;
_R_FASTLOCK lock_threadpool;
_R_FASTLOCK lock_transaction;
_R_FASTLOCK lock_writelog;

EXTERN_C const IID IID_IImageList2;

void _app_listviewresize (HWND hwnd, UINT listview_id)
{
	if (!app.ConfigGet (L"AutoSizeColumns", true).AsBool ())
		return;

	RECT rect = {0};
	GetWindowRect (GetDlgItem (hwnd, listview_id), &rect);

	const INT total_width = _R_RECT_WIDTH (&rect) - GetSystemMetrics (SM_CXVSCROLL);

	if (
		listview_id == IDC_APPS_PROFILE ||
		listview_id == IDC_APPS_SERVICE ||
		listview_id == IDC_APPS_PACKAGE
		)
	{
		const INT cx2 = max (app.GetDPI (90), min (app.GetDPI (110), _R_PERCENT_VAL (28, total_width)));
		const INT cx1 = total_width - cx2;

		_r_listview_setcolumn (hwnd, listview_id, 0, nullptr, cx1);
		_r_listview_setcolumn (hwnd, listview_id, 1, nullptr, cx2);
	}
	else if (
		listview_id == IDC_RULES_BLOCKLIST ||
		listview_id == IDC_RULES_SYSTEM ||
		listview_id == IDC_RULES_CUSTOM
		)
	{
		const INT cx3 = max (app.GetDPI (90), min (app.GetDPI (110), _R_PERCENT_VAL (28, total_width)));
		const INT cx2 = max (app.GetDPI (90), min (app.GetDPI (110), _R_PERCENT_VAL (28, total_width)));
		const INT cx1 = total_width - cx2 - cx3;

		_r_listview_setcolumn (hwnd, listview_id, 0, nullptr, cx1);
		_r_listview_setcolumn (hwnd, listview_id, 1, nullptr, cx2);
		_r_listview_setcolumn (hwnd, listview_id, 2, nullptr, cx3);
	}
}

void _app_listviewsetview (HWND hwnd, UINT ctrl_id)
{
	const bool is_iconshidden = app.ConfigGet (L"IsIconsHidden", false).AsBool ();
	const bool is_tableview = app.ConfigGet (L"IsTableView", true).AsBool () && ctrl_id != IDC_APPS_LV;

	const INT icons_size = (ctrl_id != IDC_APPS_LV ? app.ConfigGet (L"IconSize", SHIL_SYSSMALL).AsInt () : SHIL_SYSSMALL);

	HIMAGELIST himg = nullptr;

	if (
		ctrl_id == IDC_RULES_BLOCKLIST ||
		ctrl_id == IDC_RULES_SYSTEM ||
		ctrl_id == IDC_RULES_CUSTOM
		)
	{
		himg = config.himg;
	}
	else
	{
		SHGetImageList (icons_size, IID_IImageList2, (void **)& himg);
	}

	if (himg)
	{
		SendDlgItemMessage (hwnd, ctrl_id, LVM_SETIMAGELIST, LVSIL_NORMAL, (LPARAM)himg);
		SendDlgItemMessage (hwnd, ctrl_id, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)himg);
	}

	if (ctrl_id == IDC_APPS_LV)
		return;

	//SendDlgItemMessage (hwnd, ctrl_id, LVM_SCROLL, 0, GetScrollPos (GetDlgItem (hwnd, ctrl_id), SB_VERT)); // HACK!!!

	SendDlgItemMessage (hwnd, ctrl_id, LVM_SETVIEW, is_tableview ? LV_VIEW_DETAILS : LV_VIEW_ICON, NULL);

	{
		UINT menu_id;

		if (icons_size == SHIL_EXTRALARGE)
			menu_id = IDM_ICONSEXTRALARGE;

		else if (icons_size == SHIL_LARGE)
			menu_id = IDM_ICONSLARGE;

		else
			menu_id = IDM_ICONSSMALL;

		CheckMenuRadioItem (GetMenu (hwnd), IDM_ICONSSMALL, IDM_ICONSEXTRALARGE, menu_id, MF_BYCOMMAND);
	}

	CheckMenuItem (GetMenu (hwnd), IDM_ICONSISTABLEVIEW, MF_BYCOMMAND | (is_tableview ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem (GetMenu (hwnd), IDM_ICONSISHIDDEN, MF_BYCOMMAND | (is_iconshidden ? MF_CHECKED : MF_UNCHECKED));
}

void _app_setbuttonmargins (HWND hwnd, UINT ctrl_id)
{
	// set icons margin
	{
		RECT rc = {0};
		rc.left = rc.right = app.GetDPI (4);

		SendDlgItemMessage (hwnd, ctrl_id, BCM_SETTEXTMARGIN, 0, (LPARAM)& rc);
	}

	// set split info
	{
		BUTTON_SPLITINFO bsi = {0};

		bsi.mask = BCSIF_SIZE | BCSIF_STYLE;
		bsi.uSplitStyle = BCSS_STRETCH;

		bsi.size.cx = app.GetDPI (18);
		bsi.size.cy = 0;

		SendDlgItemMessage (hwnd, ctrl_id, BCM_SETSPLITINFO, 0, (LPARAM)& bsi);
	}
}

bool _app_listviewinitfont (PLOGFONT plf)
{
	const rstring buffer = app.ConfigGet (L"Font", UI_FONT_DEFAULT);

	if (!buffer.IsEmpty ())
	{
		rstring::rvector vc = buffer.AsVector (L";");

		for (size_t i = 0; i < vc.size (); i++)
		{
			vc.at (i).Trim (L" \r\n");

			if (vc.at (i).IsEmpty ())
				continue;

			if (i == 0)
				StringCchCopy (plf->lfFaceName, LF_FACESIZE, vc.at (i));

			else if (i == 1)
				plf->lfHeight = _r_dc_fontsizetoheight (vc.at (i).AsInt ());

			else if (i == 2)
				plf->lfWeight = vc.at (i).AsInt ();

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

			if (!plf->lfFaceName[0])
				StringCchCopy (plf->lfFaceName, LF_FACESIZE, pdeflf->lfFaceName);

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

void _app_listviewsetfont (HWND hwnd, UINT ctrl_id, bool is_redraw)
{
	LOGFONT lf = {0};

	if (is_redraw || !config.hfont)
	{
		if (config.hfont)
		{
			DeleteObject (config.hfont);
			config.hfont = nullptr;
		}

		_app_listviewinitfont (&lf);

		config.hfont = CreateFontIndirect (&lf);
	}

	if (config.hfont)
		SendDlgItemMessage (hwnd, ctrl_id, WM_SETFONT, (WPARAM)config.hfont, TRUE);
}

COLORREF _app_getcolorvalue (size_t hash)
{
	if (!hash)
		return 0;

	for (size_t i = 0; i < colors.size (); i++)
	{
		PITEM_COLOR ptr_clr = colors.at (i);

		if (ptr_clr && ptr_clr->hash == hash)
			return colors.at (i)->clr;
	}

	return 0;
}

COLORREF _app_getcolor (size_t hash, bool is_excludesilent)
{
	_r_fastlock_acquireshared (&lock_access);

	rstring color_value;
	PITEM_APP const ptr_app = _app_getapplication (hash);

	if (ptr_app)
	{
		if (app.ConfigGet (L"IsHighlightTimer", true).AsBool () && _app_istimeractive (ptr_app))
			color_value = L"ColorTimer";

		else if (app.ConfigGet (L"IsHighlightInvalid", true).AsBool () && !_app_isappexists (ptr_app))
			color_value = L"ColorInvalid";

		else if (app.ConfigGet (L"IsHighlightSpecial", true).AsBool () && _app_isapphaverule (hash))
			color_value = L"ColorSpecial";

		else if (!is_excludesilent && ptr_app->is_silent && app.ConfigGet (L"IsHighlightSilent", true).AsBool ())
			color_value = L"ColorSilent";

		else if (ptr_app->is_signed && app.ConfigGet (L"IsHighlightSigned", true).AsBool () && app.ConfigGet (L"IsCerificatesEnabled", false).AsBool ())
			color_value = L"ColorSigned";

		//else if ((ptr_app->type == AppService) && app.ConfigGet (L"IsHighlightService", true).AsBool ())
		//	color_value = L"ColorService";

		//else if ((ptr_app->type == AppPackage) && app.ConfigGet (L"IsHighlightPackage", true).AsBool ())
		//	color_value = L"ColorPackage";

		else if ((ptr_app->type == AppPico) && app.ConfigGet (L"IsHighlightPico", true).AsBool ())
			color_value = L"ColorPico";

		else if (ptr_app->is_system && app.ConfigGet (L"IsHighlightSystem", true).AsBool ())
			color_value = L"ColorSystem";
	}

	_r_fastlock_releaseshared (&lock_access);

	return _app_getcolorvalue (color_value.Hash ());
}

void _app_setinterfacestate ()
{
	const bool is_filtersinstalled = _wfp_isfiltersinstalled ();

	SendMessage (app.GetHWND (), WM_SETICON, ICON_SMALL, (LPARAM)app.GetSharedIcon (app.GetHINSTANCE (), is_filtersinstalled ? IDI_ACTIVE : IDI_INACTIVE, GetSystemMetrics (SM_CXSMICON)));
	SendMessage (app.GetHWND (), WM_SETICON, ICON_BIG, (LPARAM)app.GetSharedIcon (app.GetHINSTANCE (), is_filtersinstalled ? IDI_ACTIVE : IDI_INACTIVE, GetSystemMetrics (SM_CXICON)));

	SendDlgItemMessage (app.GetHWND (), IDC_START_BTN, BM_SETIMAGE, IMAGE_ICON, (WPARAM)app.GetSharedIcon (app.GetHINSTANCE (), is_filtersinstalled ? IDI_INACTIVE : IDI_ACTIVE, GetSystemMetrics (SM_CXSMICON)));

	app.TraySetInfo (app.GetHWND (), UID, nullptr, app.GetSharedIcon (app.GetHINSTANCE (), is_filtersinstalled ? IDI_ACTIVE : IDI_INACTIVE, GetSystemMetrics (SM_CXSMICON)), nullptr);
	app.TrayToggle (app.GetHWND (), UID, nullptr, true);

	SetDlgItemText (app.GetHWND (), IDC_START_BTN, app.LocaleString (is_filtersinstalled ? IDS_TRAY_STOP : IDS_TRAY_START, nullptr));
}

//bool _app_canihaveaccess ()
//{
//	bool result = false;
//
//	_r_fastlock_acquireshared (&lock_access);
//
//	PITEM_APP const ptr_app = _app_getapplication (config.myhash);
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

void _wfp_installfilters ()
{
	// set security information
	if (config.pusersid)
	{
		FwpmProviderSetSecurityInfoByKey (config.hengine, &GUID_WfpProvider, OWNER_SECURITY_INFORMATION, (const SID *)config.pusersid, nullptr, nullptr, nullptr);
		FwpmSubLayerSetSecurityInfoByKey (config.hengine, &GUID_WfpSublayer, OWNER_SECURITY_INFORMATION, (const SID *)config.pusersid, nullptr, nullptr, nullptr);
	}

	if (config.pacl_default)
	{
		FwpmProviderSetSecurityInfoByKey (config.hengine, &GUID_WfpProvider, DACL_SECURITY_INFORMATION, nullptr, nullptr, config.pacl_default, nullptr);
		FwpmSubLayerSetSecurityInfoByKey (config.hengine, &GUID_WfpSublayer, DACL_SECURITY_INFORMATION, nullptr, nullptr, config.pacl_default, nullptr);
	}

	_wfp_destroyfilters (); // destroy all installed filters first

	_r_fastlock_acquireexclusive (&lock_transaction);

	bool is_intransact = _wfp_transact_start (__LINE__);

	// apply internal rules
	_wfp_create2filters (__LINE__, is_intransact);

	// apply apps rules
	{
		MFILTER_APPS arr;

		for (auto &p : apps)
		{
			PITEM_APP ptr_app = &p.second;

			if (ptr_app->is_enabled)
				arr.push_back (ptr_app);
		}

		_wfp_create3filters (&arr, __LINE__, is_intransact);
	}

	// apply system/custom/blocklist rules
	{
		MFILTER_RULES arr;

		for (size_t i = 0; i < rules_arr.size (); i++)
		{
			PITEM_RULE ptr_rule = rules_arr.at (i);

			if (ptr_rule && ptr_rule->is_enabled)
				arr.push_back (ptr_rule);
		}

		_wfp_create4filters (&arr, __LINE__, is_intransact);
	}

	if (is_intransact)
		_wfp_transact_commit (__LINE__);

	{
		const bool is_secure = app.ConfigGet (L"IsSecureFilters", false).AsBool ();

		if (is_secure ? config.pacl_secure : config.pacl_default)
		{
			MARRAY filter_all;

			if (_wfp_dumpfilters (&GUID_WfpProvider, &filter_all))
			{
				for (size_t i = 0; i < filter_all.size (); i++)
					_wfp_setfiltersecurity (config.hengine, &filter_all.at (i), config.pusersid, is_secure ? config.pacl_secure : config.pacl_default, __LINE__);
			}
		}

		// set security information
		if (config.pusersid)
		{
			FwpmProviderSetSecurityInfoByKey (config.hengine, &GUID_WfpProvider, OWNER_SECURITY_INFORMATION, (const SID *)config.pusersid, nullptr, nullptr, nullptr);
			FwpmSubLayerSetSecurityInfoByKey (config.hengine, &GUID_WfpSublayer, OWNER_SECURITY_INFORMATION, (const SID *)config.pusersid, nullptr, nullptr, nullptr);
		}

		if (is_secure ? config.pacl_secure : config.pacl_default)
		{
			FwpmProviderSetSecurityInfoByKey (config.hengine, &GUID_WfpProvider, DACL_SECURITY_INFORMATION, nullptr, nullptr, is_secure ? config.pacl_secure : config.pacl_default, nullptr);
			FwpmSubLayerSetSecurityInfoByKey (config.hengine, &GUID_WfpSublayer, DACL_SECURITY_INFORMATION, nullptr, nullptr, is_secure ? config.pacl_secure : config.pacl_default, nullptr);
		}
	}

	_r_fastlock_releaseexclusive (&lock_transaction);
}

UINT WINAPI ApplyThread (LPVOID lparam)
{
	const bool is_install = lparam ? true : false;

	_r_ctrl_enable (app.GetHWND (), IDC_START_BTN, false);

	// dropped packets logging (win7+)
	if (config.is_neteventset && _r_sys_validversion (6, 1))
		_wfp_logunsubscribe ();

	_r_fastlock_acquireexclusive (&lock_access);

	if (is_install)
	{
		if (_wfp_initialize (true))
			_wfp_installfilters ();
	}
	else
	{
		if (_wfp_initialize (false))
			_wfp_destroyfilters ();

		_wfp_uninitialize (true);
	}

	_r_fastlock_releaseexclusive (&lock_access);

	// dropped packets logging (win7+)
	if (config.is_neteventset && _r_sys_validversion (6, 1))
		_wfp_logsubscribe ();

	_app_setinterfacestate ();

	_app_profile_save (app.GetHWND ());

	_r_listview_redraw (app.GetHWND (), _app_gettab_id (app.GetHWND ()));

	_r_ctrl_enable (app.GetHWND (), IDC_START_BTN, true);

	SetEvent (config.done_evt);

	_endthreadex (0);

	return ERROR_SUCCESS;
}

bool _app_changefilters (HWND hwnd, bool is_install, bool is_forced)
{
	if (_wfp_isfiltersapplying ())
		return false;

	const UINT listview_id = _app_gettab_id (hwnd);

	_app_listviewsort (hwnd, listview_id, -1, false);

	if (!is_install || ((is_install && is_forced) || _wfp_isfiltersinstalled ()))
	{
		_r_ctrl_enable (hwnd, IDC_START_BTN, false);

		_r_fastlock_acquireshared (&lock_apply);
		_r_fastlock_acquireexclusive (&lock_threadpool);

		_app_freethreadpool (&threads_pool);

		const HANDLE hthread = _r_createthread (&ApplyThread, (LPVOID)is_install, true, THREAD_PRIORITY_HIGHEST);

		if (hthread)
		{
			threads_pool.push_back (hthread);
			ResumeThread (hthread);
		}

		_r_fastlock_releaseexclusive (&lock_threadpool);
		_r_fastlock_releaseshared (&lock_apply);

		return true;
	}

	_app_profile_save (hwnd);

	_r_listview_redraw (hwnd, listview_id);

	return false;
}

void addcolor (UINT locale_id, LPCWSTR config_name, bool is_enabled, LPCWSTR config_value, COLORREF default_clr)
{
	PITEM_COLOR ptr_clr = new ITEM_COLOR;

	if (ptr_clr)
	{
		if (config_name)
			_r_str_alloc (&ptr_clr->pcfg_name, _r_str_length (config_name), config_name);

		if (config_value)
			_r_str_alloc (&ptr_clr->pcfg_value, _r_str_length (config_value), config_value);

		ptr_clr->hash = _r_str_hash (config_value);
		ptr_clr->is_enabled = is_enabled;
		ptr_clr->locale_id = locale_id;
		ptr_clr->default_clr = default_clr;
		ptr_clr->clr = app.ConfigGet (config_value, default_clr).AsUlong ();

		colors.push_back (ptr_clr);
	}
}

void addprotocol (LPCWSTR name, UINT8 id)
{
	PITEM_PROTOCOL ptr_proto = new ITEM_PROTOCOL;

	if (ptr_proto)
	{
		if (name)
			_r_str_alloc (&ptr_proto->pname, _r_str_length (name), name);

		ptr_proto->id = id;

		protocols.push_back (ptr_proto);
	}
}

bool _app_installmessage (HWND hwnd, bool is_install)
{
	WCHAR flag[64] = {0};

	WCHAR button_text_1[128] = {0};
	WCHAR button_text_2[128] = {0};
	WCHAR button_text_3[128] = {0};

	WCHAR main[256] = {0};

	INT result = 0;
	BOOL is_flagchecked = FALSE;

	TASKDIALOGCONFIG tdc = {0};
	TASKDIALOG_BUTTON td_buttons[3] = {0};

	tdc.cbSize = sizeof (tdc);
	tdc.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | (is_install ? TDF_USE_COMMAND_LINKS : 0);
	tdc.hwndParent = hwnd;
	tdc.pszWindowTitle = APP_NAME;
	tdc.pszMainIcon = is_install ? TD_INFORMATION_ICON : TD_WARNING_ICON;
	//tdc.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
	tdc.pszContent = main;
	tdc.pszVerificationText = flag;
	tdc.pfCallback = &_r_msg_callback;
	tdc.lpCallbackData = MAKELONG (0, 1);

	tdc.pButtons = td_buttons;

	if (is_install)
	{
		tdc.cButtons = _countof (td_buttons);

		td_buttons[0].nButtonID = IDM_TRAY_MODEWHITELIST;
		td_buttons[0].pszButtonText = button_text_1;

		td_buttons[1].nButtonID = IDM_TRAY_MODEBLACKLIST;
		td_buttons[1].pszButtonText = button_text_2;

		td_buttons[2].nButtonID = IDNO;
		td_buttons[2].pszButtonText = button_text_3;

		StringCchCopy (button_text_1, _countof (button_text_1), app.LocaleString (IDS_MODE_WHITELIST, nullptr));
		StringCchCopy (button_text_2, _countof (button_text_2), app.LocaleString (IDS_MODE_BLACKLIST, nullptr));
		StringCchCopy (button_text_3, _countof (button_text_3), app.LocaleString (IDS_CLOSE, nullptr));

		tdc.nDefaultButton = IDM_TRAY_MODEWHITELIST + app.ConfigGet (L"Mode", ModeWhitelist).AsUint ();
	}
	else
	{
		tdc.cButtons = _countof (td_buttons) - 1;

		StringCchCopy (button_text_1, _countof (button_text_1), app.LocaleString (IDS_TRAY_STOP, nullptr));
		StringCchCopy (button_text_2, _countof (button_text_2), app.LocaleString (IDS_CLOSE, nullptr));

		td_buttons[0].nButtonID = IDYES;
		td_buttons[0].pszButtonText = button_text_1;

		td_buttons[1].nButtonID = IDNO;
		td_buttons[1].pszButtonText = button_text_2;

		tdc.nDefaultButton = IDNO;
	}

	if (is_install)
	{
		StringCchCopy (main, _countof (main), app.LocaleString (IDS_QUESTION_START, nullptr));
		StringCchCopy (flag, _countof (flag), app.LocaleString (IDS_DISABLEWINDOWSFIREWALL_CHK, nullptr));

		if (app.ConfigGet (L"IsDisableWindowsFirewallChecked", true).AsBool ())
			tdc.dwFlags |= TDF_VERIFICATION_FLAG_CHECKED;
	}
	else
	{
		StringCchCopy (main, _countof (main), app.LocaleString (IDS_QUESTION_STOP, nullptr));
		StringCchCopy (flag, _countof (flag), app.LocaleString (IDS_ENABLEWINDOWSFIREWALL_CHK, nullptr));

		if (app.ConfigGet (L"IsEnableWindowsFirewallChecked", true).AsBool ())
			tdc.dwFlags |= TDF_VERIFICATION_FLAG_CHECKED;
	}

	if (_r_msg_taskdialog (&tdc, &result, nullptr, &is_flagchecked))
	{
		if ((result == IDYES) || (result == IDM_TRAY_MODEWHITELIST) || (result == IDM_TRAY_MODEBLACKLIST))
		{
			if (is_install)
			{
				app.ConfigSet (L"IsDisableWindowsFirewallChecked", is_flagchecked ? true : false);

				app.ConfigSet (L"Mode", DWORD ((result == IDM_TRAY_MODEWHITELIST) ? ModeWhitelist : ModeBlacklist));
				CheckMenuRadioItem (GetMenu (hwnd), IDM_TRAY_MODEWHITELIST, IDM_TRAY_MODEBLACKLIST, IDM_TRAY_MODEWHITELIST + app.ConfigGet (L"Mode", ModeWhitelist).AsUint (), MF_BYCOMMAND);

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

LONG _app_nmcustdraw (LPNMLVCUSTOMDRAW lpnmlv)
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

			if (
				lpnmlv->nmcd.hdr.idFrom == IDC_APPS_LV ||
				lpnmlv->nmcd.hdr.idFrom == IDC_APPS_PROFILE ||
				lpnmlv->nmcd.hdr.idFrom == IDC_APPS_SERVICE ||
				lpnmlv->nmcd.hdr.idFrom == IDC_APPS_PACKAGE
				)
			{
				const size_t hash = lpnmlv->nmcd.lItemlParam;

				if (hash)
				{
					const COLORREF new_clr = (COLORREF)_app_getcolor (hash, lpnmlv->nmcd.hdr.idFrom == IDC_APPS_LV);

					if (new_clr)
					{
						lpnmlv->clrText = _r_dc_getcolorbrightness (new_clr);
						lpnmlv->clrTextBk = new_clr;

						if (lpnmlv->nmcd.hdr.idFrom == IDC_APPS_LV || is_tableview)
							_r_dc_fillrect (lpnmlv->nmcd.hdc, &lpnmlv->nmcd.rc, new_clr);

						return CDRF_NEWFONT;
					}
				}
			}
			else if (
				lpnmlv->nmcd.hdr.idFrom == IDC_RULES_BLOCKLIST ||
				lpnmlv->nmcd.hdr.idFrom == IDC_RULES_SYSTEM ||
				lpnmlv->nmcd.hdr.idFrom == IDC_RULES_CUSTOM
				)
			{
				const size_t rule_idx = lpnmlv->nmcd.lItemlParam;
				PITEM_RULE ptr_rule = rules_arr.at (rule_idx);

				if (ptr_rule)
				{
					COLORREF clr = 0;

					if (ptr_rule->is_enabled && ptr_rule->is_haveerrors)
						clr = _app_getcolorvalue (_r_str_hash (L"ColorInvalid"));

					else if (ptr_rule->is_forservices || !ptr_rule->apps.empty ())
						clr = _app_getcolorvalue (_r_str_hash (L"ColorSpecial"));

					if (clr)
					{
						lpnmlv->clrText = _r_dc_getcolorbrightness (clr);
						lpnmlv->clrTextBk = clr;

						if (is_tableview)
							_r_dc_fillrect (lpnmlv->nmcd.hdc, &lpnmlv->nmcd.rc, clr);

						return CDRF_NEWFONT;
					}
				}
			}
			else if (lpnmlv->nmcd.hdr.idFrom == IDC_COLORS)
			{
				PITEM_COLOR const ptr_clr = colors.at (lpnmlv->nmcd.lItemlParam);

				if (ptr_clr)
				{
					lpnmlv->clrText = _r_dc_getcolorbrightness (ptr_clr->clr);
					lpnmlv->clrTextBk = ptr_clr->clr;

					_r_dc_fillrect (lpnmlv->nmcd.hdc, &lpnmlv->nmcd.rc, lpnmlv->clrTextBk);
				}

				return CDRF_NEWFONT;
			}

			break;
		}
	}

	return CDRF_DODEFAULT;
}

INT_PTR CALLBACK EditorProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static PITEM_RULE ptr_rule = nullptr;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			ptr_rule = (PITEM_RULE)lparam;

#ifndef _APP_NO_DARKTHEME
			_r_wnd_setdarktheme (hwnd);
#endif // _APP_NO_DARKTHEME

			// configure window
			_r_wnd_center (hwnd, GetParent (hwnd));

			SendMessage (hwnd, WM_SETICON, ICON_SMALL, (LPARAM)app.GetSharedIcon (app.GetHINSTANCE (), IDI_MAIN, GetSystemMetrics (SM_CXSMICON)));
			SendMessage (hwnd, WM_SETICON, ICON_BIG, (LPARAM)app.GetSharedIcon (app.GetHINSTANCE (), IDI_MAIN, GetSystemMetrics (SM_CXICON)));

			// localize window
			SetWindowText (hwnd, (ptr_rule && ptr_rule->pname ? _r_fmt (L"%s - \"%s\"", app.LocaleString (IDS_EDITOR, nullptr).GetString (), ptr_rule->pname) : app.LocaleString (IDS_EDITOR, nullptr)));

			SetDlgItemText (hwnd, IDC_NAME, app.LocaleString (IDS_NAME, L":"));
			SetDlgItemText (hwnd, IDC_RULE_REMOTE, app.LocaleString (IDS_RULE, L" (" SZ_LOG_REMOTE_ADDRESS L"):"));
			SetDlgItemText (hwnd, IDC_RULE_LOCAL, app.LocaleString (IDS_RULE, L" (" SZ_LOG_LOCAL_ADDRESS L"):"));
			SetDlgItemText (hwnd, IDC_REGION, app.LocaleString (IDS_REGION, L":"));
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
			_r_listview_setstyle (hwnd, IDC_APPS_LV, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_CHECKBOXES);

			_r_listview_addcolumn (hwnd, IDC_APPS_LV, 0, nullptr, 95, LVCFMT_LEFT);

			_app_listviewsetview (hwnd, IDC_APPS_LV);
			_app_listviewsetfont (hwnd, IDC_APPS_LV, false);

			// name
			if (ptr_rule && ptr_rule->pname)
				SetDlgItemText (hwnd, IDC_NAME_EDIT, ptr_rule->pname);

			// rule (remote)
			if (ptr_rule && ptr_rule->prule_remote)
				SetDlgItemText (hwnd, IDC_RULE_REMOTE_EDIT, ptr_rule->prule_remote);

			// rule (local)
			if (ptr_rule && ptr_rule->prule_local)
				SetDlgItemText (hwnd, IDC_RULE_LOCAL_EDIT, ptr_rule->prule_local);

			// apps (apply to)
			{
				size_t item = 0;

				_r_fastlock_acquireshared (&lock_access);

				for (auto &p : apps)
				{
					PITEM_APP const ptr_app = &p.second;

					// windows store apps (win8+)
					if (ptr_app->type == AppPackage && !_r_sys_validversion (6, 2))
						continue;

					if (ptr_rule && ptr_rule->is_forservices && (p.first == config.ntoskrnl_hash || p.first == config.svchost_hash))
						continue;

					_r_fastlock_acquireshared (&lock_checkbox);

					const bool is_enabled = ptr_rule && !ptr_rule->apps.empty () && (ptr_rule->apps.find (p.first) != ptr_rule->apps.end ());

					_r_listview_additem (hwnd, IDC_APPS_LV, item, 0, _r_path_extractfile (ptr_app->display_name), ptr_app->icon_id, LAST_VALUE, p.first);
					_r_listview_setitemcheck (hwnd, IDC_APPS_LV, item, is_enabled);

					_r_fastlock_releaseshared (&lock_checkbox);

					item += 1;
				}

				_r_fastlock_releaseshared (&lock_access);

				// sort column
				_app_listviewsort (hwnd, IDC_APPS_LV, 0, false);

				// resize column
				RECT rc = {0};
				GetClientRect (GetDlgItem (hwnd, IDC_APPS_LV), &rc);

				_r_listview_setcolumn (hwnd, IDC_APPS_LV, 0, nullptr, _R_RECT_WIDTH (&rc));
			}

			// direction
			SendDlgItemMessage (hwnd, IDC_DIRECTION_EDIT, CB_INSERTSTRING, 0, (LPARAM)app.LocaleString (IDS_DIRECTION_1, nullptr).GetString ());
			SendDlgItemMessage (hwnd, IDC_DIRECTION_EDIT, CB_INSERTSTRING, 1, (LPARAM)app.LocaleString (IDS_DIRECTION_2, nullptr).GetString ());
			SendDlgItemMessage (hwnd, IDC_DIRECTION_EDIT, CB_INSERTSTRING, 2, (LPARAM)app.LocaleString (IDS_DIRECTION_3, nullptr).GetString ());

			if (ptr_rule)
				SendDlgItemMessage (hwnd, IDC_DIRECTION_EDIT, CB_SETCURSEL, (WPARAM)ptr_rule->dir, 0);

			// protocol
			SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_INSERTSTRING, 0, (LPARAM)app.LocaleString (IDS_ALL, nullptr).GetString ());
			SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_SETCURSEL, 0, 0);

			for (size_t i = 0; i < protocols.size (); i++)
			{
				PITEM_PROTOCOL ptr_protocol = protocols.at (i);

				if (ptr_protocol)
				{
					SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_INSERTSTRING, i + 1, (LPARAM)_r_fmt (L"#%03d - %s", ptr_protocol->id, ptr_protocol->pname).GetString ());
					SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_SETITEMDATA, i + 1, (LPARAM)ptr_protocol->id);

					if (ptr_rule && ptr_rule->protocol == ptr_protocol->id)
						SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_SETCURSEL, (WPARAM)i + 1, 0);
				}
			}

			// family (ports-only)
			SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_INSERTSTRING, 0, (LPARAM)app.LocaleString (IDS_ALL, nullptr).GetString ());
			SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_SETITEMDATA, 0, (LPARAM)AF_UNSPEC);

			SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_INSERTSTRING, 1, (LPARAM)L"IPv4");
			SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_SETITEMDATA, 1, (LPARAM)AF_INET);

			SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_INSERTSTRING, 2, (LPARAM)L"IPv6");
			SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_SETITEMDATA, 2, (LPARAM)AF_INET6);

			if (ptr_rule)
			{
				if (ptr_rule->af == AF_UNSPEC)
					SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_SETCURSEL, (WPARAM)0, 0);

				else if (ptr_rule->af == AF_INET)
					SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_SETCURSEL, (WPARAM)1, 0);

				else if (ptr_rule->af == AF_INET6)
					SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_SETCURSEL, (WPARAM)2, 0);
			}

			// action
			SendDlgItemMessage (hwnd, IDC_ACTION_EDIT, CB_INSERTSTRING, 0, (LPARAM)app.LocaleString (IDS_ACTION_ALLOW, nullptr).GetString ());
			SendDlgItemMessage (hwnd, IDC_ACTION_EDIT, CB_INSERTSTRING, 1, (LPARAM)app.LocaleString (IDS_ACTION_BLOCK, nullptr).GetString ());

			if (ptr_rule)
				SendDlgItemMessage (hwnd, IDC_ACTION_EDIT, CB_SETCURSEL, (WPARAM)ptr_rule->is_block, 0);

			// state
			{
				UINT ctrl_id = IDC_DISABLE_CHK;

				if (ptr_rule && ptr_rule->is_enabled)
					ctrl_id = ptr_rule->apps.empty () ? IDC_ENABLE_CHK : IDC_ENABLEFORAPPS_CHK;

				CheckRadioButton (hwnd, IDC_DISABLE_CHK, IDC_ENABLEFORAPPS_CHK, ctrl_id);
			}

			// set limitation
			SendDlgItemMessage (hwnd, IDC_NAME_EDIT, EM_LIMITTEXT, RULE_NAME_CCH_MAX - 1, 0);
			SendDlgItemMessage (hwnd, IDC_RULE_REMOTE_EDIT, EM_LIMITTEXT, RULE_RULE_CCH_MAX - 1, 0);
			SendDlgItemMessage (hwnd, IDC_RULE_LOCAL_EDIT, EM_LIMITTEXT, RULE_RULE_CCH_MAX - 1, 0);

			// set read-only
			if (ptr_rule)
			{
				SendDlgItemMessage (hwnd, IDC_NAME_EDIT, EM_SETREADONLY, ptr_rule->is_readonly, 0);
				SendDlgItemMessage (hwnd, IDC_RULE_REMOTE_EDIT, EM_SETREADONLY, ptr_rule->is_readonly, 0);
				SendDlgItemMessage (hwnd, IDC_RULE_LOCAL_EDIT, EM_SETREADONLY, ptr_rule->is_readonly, 0);

				_r_ctrl_enable (hwnd, IDC_PORTVERSION_EDIT, !ptr_rule->is_readonly);
				_r_ctrl_enable (hwnd, IDC_PROTOCOL_EDIT, !ptr_rule->is_readonly);
				_r_ctrl_enable (hwnd, IDC_DIRECTION_EDIT, !ptr_rule->is_readonly);
				_r_ctrl_enable (hwnd, IDC_ACTION_EDIT, !ptr_rule->is_readonly);
			}

			_r_wnd_addstyle (hwnd, IDC_WIKI, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (hwnd, IDC_SAVE, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (hwnd, IDC_CLOSE, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

			_r_ctrl_enable (hwnd, IDC_SAVE, (SendDlgItemMessage (hwnd, IDC_NAME_EDIT, WM_GETTEXTLENGTH, 0, 0) > 0) && ((SendDlgItemMessage (hwnd, IDC_RULE_REMOTE_EDIT, WM_GETTEXTLENGTH, 0, 0) > 0) || (SendDlgItemMessage (hwnd, IDC_RULE_LOCAL_EDIT, WM_GETTEXTLENGTH, 0, 0) > 0))); // enable apply button

			break;
		}

#ifndef _APP_NO_DARKTHEME
		case WM_SYSCOLORCHANGE:
		{
			_r_wnd_setdarktheme (hwnd);
			break;
		}
#endif // _APP_NO_DARKTHEME

		case WM_CONTEXTMENU:
		{
			const UINT ctrl_id = GetDlgCtrlID ((HWND)wparam);

			if (ctrl_id != IDC_APPS_LV)
				break;

			const HMENU hmenu = CreateMenu ();
			const HMENU hsubmenu = CreateMenu ();

			AppendMenu (hmenu, MF_POPUP, (UINT_PTR)hsubmenu, L" ");

			AppendMenu (hsubmenu, MF_BYCOMMAND, IDM_CHECK, app.LocaleString (IDS_CHECK, nullptr));
			AppendMenu (hsubmenu, MF_BYCOMMAND, IDM_UNCHECK, app.LocaleString (IDS_UNCHECK, nullptr));

			if (!SendDlgItemMessage (hwnd, ctrl_id, LVM_GETSELECTEDCOUNT, 0, 0))
			{
				EnableMenuItem (hsubmenu, IDM_CHECK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
				EnableMenuItem (hsubmenu, IDM_UNCHECK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
			}

			POINT pt = {0};
			GetCursorPos (&pt);

			TrackPopupMenuEx (hsubmenu, TPM_RIGHTBUTTON | TPM_LEFTBUTTON, pt.x, pt.y, hwnd, nullptr);

			DestroyMenu (hsubmenu);
			DestroyMenu (hmenu);

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lparam;

			switch (nmlp->code)
			{
				case NM_CUSTOMDRAW:
				{
					_r_fastlock_acquireshared (&lock_access);
					const LONG result = _app_nmcustdraw ((LPNMLVCUSTOMDRAW)lparam);
					_r_fastlock_releaseshared (&lock_access);

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, result);
					return result;
				}

				case LVN_ITEMCHANGED:
				{
					if (_r_fastlock_islocked (&lock_checkbox))
						break;

					LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lparam;

					if (lpnmlv->uNewState == 8192 || lpnmlv->uNewState == 4096)
					{
						const bool is_havechecks = _r_listview_getitemcount (hwnd, IDC_APPS_LV, true);

						UINT ctrl_id = IDC_DISABLE_CHK;

						if (!is_havechecks && (!ptr_rule->is_readonly || ptr_rule->is_forservices))
							ctrl_id = IDC_ENABLE_CHK;

						else if (is_havechecks)
							ctrl_id = IDC_ENABLEFORAPPS_CHK;

						CheckRadioButton (hwnd, IDC_DISABLE_CHK, IDC_ENABLEFORAPPS_CHK, ctrl_id);

						_app_listviewsort (hwnd, IDC_APPS_LV, 0, false);
					}

					break;
				}

				case LVN_GETINFOTIP:
				{
					LPNMLVGETINFOTIP lpnmlv = (LPNMLVGETINFOTIP)lparam;

					const size_t idx = (size_t)_r_listview_getitemlparam (hwnd, (UINT)lpnmlv->hdr.idFrom, lpnmlv->iItem);

					if (idx)
						StringCchCopy (lpnmlv->pszText, lpnmlv->cchTextMax, _app_gettooltip ((UINT)lpnmlv->hdr.idFrom, idx));

					break;
				}

				case LVN_GETEMPTYMARKUP:
				{
					NMLVEMPTYMARKUP *lpnmlv = (NMLVEMPTYMARKUP *)lparam;

					lpnmlv->dwFlags = EMF_CENTERED;
					StringCchCopy (lpnmlv->szMarkup, _countof (lpnmlv->szMarkup), app.LocaleString (IDS_STATUS_EMPTY, nullptr));

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
				_r_ctrl_enable (hwnd, IDC_SAVE, (SendDlgItemMessage (hwnd, IDC_NAME_EDIT, WM_GETTEXTLENGTH, 0, 0) > 0) && ((SendDlgItemMessage (hwnd, IDC_RULE_REMOTE_EDIT, WM_GETTEXTLENGTH, 0, 0) > 0) || (SendDlgItemMessage (hwnd, IDC_RULE_LOCAL_EDIT, WM_GETTEXTLENGTH, 0, 0) > 0))); // enable apply button
				return FALSE;
			}

			switch (LOWORD (wparam))
			{
				case IDM_CHECK:
				case IDM_UNCHECK:
				{
					const bool new_val = (LOWORD (wparam) == IDM_CHECK) ? true : false;
					size_t item = LAST_VALUE;

					_r_fastlock_acquireshared (&lock_checkbox);

					while ((item = (size_t)SendDlgItemMessage (hwnd, IDC_APPS_LV, LVM_GETNEXTITEM, item, LVNI_SELECTED)) != LAST_VALUE)
						_r_listview_setitemcheck (hwnd, IDC_APPS_LV, item, new_val);

					_r_fastlock_releaseshared (&lock_checkbox);

					_app_listviewsort (hwnd, IDC_APPS_LV, 0, false);

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
					if (!SendDlgItemMessage (hwnd, IDC_NAME_EDIT, WM_GETTEXTLENGTH, 0, 0) || (!SendDlgItemMessage (hwnd, IDC_RULE_REMOTE_EDIT, WM_GETTEXTLENGTH, 0, 0) && !SendDlgItemMessage (hwnd, IDC_RULE_LOCAL_EDIT, WM_GETTEXTLENGTH, 0, 0)))
						return FALSE;

					// rule destination
					{
						rstring rule_remote = _r_ctrl_gettext (hwnd, IDC_RULE_REMOTE_EDIT).Trim (L"\r\n " RULE_DELIMETER);
						size_t rule_remote_length;

						rstring rule_local = _r_ctrl_gettext (hwnd, IDC_RULE_LOCAL_EDIT).Trim (L"\r\n " RULE_DELIMETER);
						size_t rule_local_length;

						// here we parse and check rule syntax
						{
							rstring::rvector arr = rule_remote.AsVector (RULE_DELIMETER);
							rstring rule_remote_fixed;

							for (size_t i = 0; i < arr.size (); i++)
							{
								LPCWSTR rule_single = arr.at (i).Trim (L" " RULE_DELIMETER);

								if (!_app_parserulestring (rule_single, nullptr))
								{
									_r_ctrl_showtip (hwnd, IDC_RULE_REMOTE_EDIT, TTI_ERROR, APP_NAME, _r_fmt (app.LocaleString (IDS_STATUS_SYNTAX_ERROR, nullptr), rule_single));
									_r_ctrl_enable (hwnd, IDC_SAVE, false);

									return FALSE;
								}

								rule_remote_fixed.AppendFormat (L"%s" RULE_DELIMETER, rule_single);
							}

							rule_remote = rule_remote_fixed.Trim (L" " RULE_DELIMETER);
							rule_remote_length = min (rule_remote.GetLength (), RULE_RULE_CCH_MAX);
						}

						// here we parse and check rule syntax
						{
							rstring::rvector arr = rule_local.AsVector (RULE_DELIMETER);
							rstring rule_local_fixed;

							for (size_t i = 0; i < arr.size (); i++)
							{
								LPCWSTR rule_single = arr.at (i).Trim (L" " RULE_DELIMETER);

								if (!_app_parserulestring (rule_single, nullptr))
								{
									_r_ctrl_showtip (hwnd, IDC_RULE_LOCAL_EDIT, TTI_ERROR, APP_NAME, _r_fmt (app.LocaleString (IDS_STATUS_SYNTAX_ERROR, nullptr), rule_single));
									_r_ctrl_enable (hwnd, IDC_SAVE, false);

									return FALSE;
								}

								rule_local_fixed.AppendFormat (L"%s" RULE_DELIMETER, rule_single);
							}

							rule_local = rule_local_fixed.Trim (L" " RULE_DELIMETER);
							rule_local_length = min (rule_local.GetLength (), RULE_RULE_CCH_MAX);
						}

						_r_fastlock_acquireexclusive (&lock_access);

						// save rule (remote)
						_r_str_alloc (&ptr_rule->prule_remote, rule_remote_length, rule_remote);
						_r_str_alloc (&ptr_rule->prule_local, rule_local_length, rule_local);
					}

					// save rule name
					{
						const rstring name = _r_ctrl_gettext (hwnd, IDC_NAME_EDIT).Trim (L"\r\n " RULE_DELIMETER);

						if (!name.IsEmpty ())
						{
							const size_t name_length = min (name.GetLength (), RULE_NAME_CCH_MAX);

							_r_str_alloc (&ptr_rule->pname, name_length, name);
						}
					}

					// save rule apps
					{
						ptr_rule->apps.clear ();

						const bool is_enable = (IsDlgButtonChecked (hwnd, IDC_ENABLE_CHK) != BST_CHECKED);

						for (size_t i = 0; i < _r_listview_getitemcount (hwnd, IDC_APPS_LV); i++)
						{
							const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, IDC_APPS_LV, i);

							if (hash)
							{
								bool is_apply = is_enable && _r_listview_isitemchecked (hwnd, IDC_APPS_LV, i);

								if (is_apply)
									ptr_rule->apps[hash] = true;

								_r_listview_setitem (app.GetHWND (), IDC_APPS_PROFILE, _app_getposition (app.GetHWND (), hash), 0, nullptr, LAST_VALUE, _app_getappgroup (hash, _app_getapplication (hash)));
							}
						}
					}

					ptr_rule->protocol = (UINT8)SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_GETITEMDATA, SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_GETCURSEL, 0, 0), 0);
					ptr_rule->af = (ADDRESS_FAMILY)SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_GETITEMDATA, SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_GETCURSEL, 0, 0), 0);

					ptr_rule->dir = (FWP_DIRECTION)SendDlgItemMessage (hwnd, IDC_DIRECTION_EDIT, CB_GETCURSEL, 0, 0);
					ptr_rule->is_block = SendDlgItemMessage (hwnd, IDC_ACTION_EDIT, CB_GETCURSEL, 0, 0) ? true : false;
					ptr_rule->weight = (ptr_rule->is_block ? FILTER_WEIGHT_CUSTOM_BLOCK : FILTER_WEIGHT_CUSTOM);

					_app_ruleenable (ptr_rule, (IsDlgButtonChecked (hwnd, IDC_DISABLE_CHK) == BST_UNCHECKED) ? true : false);

					_r_fastlock_releaseexclusive (&lock_access);

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
			const UINT dialog_id = (UINT)wparam;

			switch (dialog_id)
			{
				case IDD_SETTINGS_GENERAL:
				{
					CheckDlgButton (hwnd, IDC_ALWAYSONTOP_CHK, app.ConfigGet (L"AlwaysOnTop", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_STARTMINIMIZED_CHK, app.ConfigGet (L"IsStartMinimized", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

#ifdef _APP_HAVE_AUTORUN
					CheckDlgButton (hwnd, IDC_LOADONSTARTUP_CHK, app.AutorunIsEnabled () ? BST_CHECKED : BST_UNCHECKED);
#endif // _APP_HAVE_AUTORUN

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
					CheckDlgButton (hwnd, IDC_USECERTIFICATES_CHK, app.ConfigGet (L"IsCerificatesEnabled", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_USENETWORKRESOLUTION_CHK, app.ConfigGet (L"IsNetworkResolutionsEnabled", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_USEREFRESHDEVICES_CHK, app.ConfigGet (L"IsRefreshDevices", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_RULE_ALLOWINBOUND, app.ConfigGet (L"AllowInboundConnections", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_RULE_ALLOWLISTEN, app.ConfigGet (L"AllowListenConnections2", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_RULE_ALLOWLOOPBACK, app.ConfigGet (L"AllowLoopbackConnections", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_RULE_ALLOW6TO4, app.ConfigGet (L"AllowIPv6", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_USESTEALTHMODE_CHK, app.ConfigGet (L"UseStealthMode", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_INSTALLBOOTTIMEFILTERS_CHK, app.ConfigGet (L"InstallBoottimeFilters", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_SECUREFILTERS_CHK, app.ConfigGet (L"IsSecureFilters", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					_r_ctrl_settip (hwnd, IDC_RULE_ALLOWINBOUND, LPSTR_TEXTCALLBACK);
					_r_ctrl_settip (hwnd, IDC_RULE_ALLOWLISTEN, LPSTR_TEXTCALLBACK);
					_r_ctrl_settip (hwnd, IDC_RULE_ALLOWLOOPBACK, LPSTR_TEXTCALLBACK);
					_r_ctrl_settip (hwnd, IDC_RULE_ALLOW6TO4, LPSTR_TEXTCALLBACK);

					_r_ctrl_settip (hwnd, IDC_USESTEALTHMODE_CHK, LPSTR_TEXTCALLBACK);
					_r_ctrl_settip (hwnd, IDC_INSTALLBOOTTIMEFILTERS_CHK, LPSTR_TEXTCALLBACK);
					_r_ctrl_settip (hwnd, IDC_SECUREFILTERS_CHK, LPSTR_TEXTCALLBACK);

					break;
				}

				case IDD_SETTINGS_INTERFACE:
				{
					CheckDlgButton (hwnd, IDC_CONFIRMEXIT_CHK, app.ConfigGet (L"ConfirmExit2", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_CONFIRMEXITTIMER_CHK, app.ConfigGet (L"ConfirmExitTimer", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_CONFIRMDELETE_CHK, app.ConfigGet (L"ConfirmDelete", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_CONFIRMLOGCLEAR_CHK, app.ConfigGet (L"ConfirmLogClear", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					break;
				}

				case IDD_SETTINGS_HIGHLIGHTING:
				{
					// configure listview
					_r_listview_setstyle (hwnd, IDC_COLORS, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_CHECKBOXES);

					_app_listviewsetview (hwnd, IDC_COLORS);

					_r_listview_deleteallitems (hwnd, IDC_COLORS);
					_r_listview_deleteallcolumns (hwnd, IDC_COLORS);

					_r_listview_addcolumn (hwnd, IDC_COLORS, 0, app.LocaleString (IDS_NAME, nullptr), 100, LVCFMT_LEFT);

					{
						for (size_t i = 0; i < colors.size (); i++)
						{
							PITEM_COLOR ptr_clr = colors.at (i);

							if (ptr_clr)
							{
								ptr_clr->clr = app.ConfigGet (ptr_clr->pcfg_value, ptr_clr->default_clr).AsUlong ();

								_r_fastlock_acquireshared (&lock_checkbox);

								_r_listview_additem (hwnd, IDC_COLORS, i, 0, app.LocaleString (ptr_clr->locale_id, nullptr), config.icon_id, LAST_VALUE, i);
								_r_listview_setitemcheck (hwnd, IDC_COLORS, i, app.ConfigGet (ptr_clr->pcfg_name, ptr_clr->is_enabled).AsBool ());

								_r_fastlock_releaseshared (&lock_checkbox);
							}
						}
					}

					break;
				}

				case IDD_SETTINGS_LOG:
				{
					CheckDlgButton (hwnd, IDC_ENABLELOG_CHK, app.ConfigGet (L"IsLogEnabled", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					SetDlgItemText (hwnd, IDC_LOGPATH, app.ConfigGet (L"LogPath", LOG_PATH_DEFAULT));

					UDACCEL ud = {0};
					ud.nInc = 64; // set step to 64kb

					SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_SETACCEL, 1, (LPARAM)& ud);
					SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_SETRANGE32, 64, 8192);
					SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_SETPOS32, 0, app.ConfigGet (L"LogSizeLimitKb", LOG_SIZE_LIMIT).AsUint ());

					CheckDlgButton (hwnd, IDC_ENABLENOTIFICATIONS_CHK, app.ConfigGet (L"IsNotificationsEnabled", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_NOTIFICATIONSOUND_CHK, app.ConfigGet (L"IsNotificationsSound", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					SendDlgItemMessage (hwnd, IDC_NOTIFICATIONDISPLAYTIMEOUT, UDM_SETRANGE32, 0, _R_SECONDSCLOCK_MIN (30));
					SendDlgItemMessage (hwnd, IDC_NOTIFICATIONDISPLAYTIMEOUT, UDM_SETPOS32, 0, app.ConfigGet (L"NotificationsDisplayTimeout", NOTIFY_TIMER_DEFAULT).AsUint ());

					SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_SETRANGE32, 0, _R_SECONDSCLOCK_DAY (7));
					SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_SETPOS32, 0, app.ConfigGet (L"NotificationsTimeout", NOTIFY_TIMEOUT_DEFAULT).AsUint ());

					CheckDlgButton (hwnd, IDC_EXCLUDESTEALTH_CHK, app.ConfigGet (L"IsExcludeStealth", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, app.ConfigGet (L"IsExcludeClassifyAllow", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_EXCLUDEBLOCKLIST_CHK, app.ConfigGet (L"IsExcludeBlocklist", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_EXCLUDECUSTOM_CHK, app.ConfigGet (L"IsExcludeCustomRules", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					if (!_r_sys_validversion (6, 2))
						_r_ctrl_enable (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, false);

					// dropped packets logging (win7+)
					if (!_r_sys_validversion (6, 1))
					{
						_r_ctrl_enable (hwnd, IDC_ENABLELOG_CHK, false);
						_r_ctrl_enable (hwnd, IDC_ENABLENOTIFICATIONS_CHK, false);
					}
					else
					{
						PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDC_ENABLELOG_CHK, 0), 0);
						PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDC_ENABLENOTIFICATIONS_CHK, 0), 0);
					}

					break;
				}
			}

			break;
		}

		case RM_LOCALIZE:
		{
			const UINT dialog_id = (UINT)wparam;

			// localize titles
			SetDlgItemText (hwnd, IDC_TITLE_GENERAL, app.LocaleString (IDS_TITLE_GENERAL, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_LANGUAGE, app.LocaleString (IDS_TITLE_LANGUAGE, L": (Language)"));
			SetDlgItemText (hwnd, IDC_TITLE_CONFIRMATIONS, app.LocaleString (IDS_TITLE_CONFIRMATIONS, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_HIGHLIGHTING, app.LocaleString (IDS_TITLE_HIGHLIGHTING, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_ADVANCED, app.LocaleString (IDS_TITLE_ADVANCED, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_EXPERTS, app.LocaleString (IDS_TITLE_EXPERTS, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_LOGGING, app.LocaleString (IDS_TITLE_LOGGING, _r_sys_validversion (6, 1) ? L":" : L": (win7+)"));
			SetDlgItemText (hwnd, IDC_TITLE_NOTIFICATIONS, app.LocaleString (IDS_TITLE_NOTIFICATIONS, _r_sys_validversion (6, 1) ? L":" : L": (win7+)"));

			switch (dialog_id)
			{
				case IDD_SETTINGS_GENERAL:
				{
					SetDlgItemText (hwnd, IDC_ALWAYSONTOP_CHK, app.LocaleString (IDS_ALWAYSONTOP_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_STARTMINIMIZED_CHK, app.LocaleString (IDS_STARTMINIMIZED_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_LOADONSTARTUP_CHK, app.LocaleString (IDS_LOADONSTARTUP_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_SKIPUACWARNING_CHK, app.LocaleString (IDS_SKIPUACWARNING_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_CHECKUPDATES_CHK, app.LocaleString (IDS_CHECKUPDATES_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_CHECKUPDATESBETA_CHK, app.LocaleString (IDS_CHECKUPDATESBETA_CHK, nullptr));

					SetDlgItemText (hwnd, IDC_LANGUAGE_HINT, app.LocaleString (IDS_LANGUAGE_HINT, nullptr));

					break;
				}

				case IDD_SETTINGS_INTERFACE:
				{
					SetDlgItemText (hwnd, IDC_CONFIRMEXIT_CHK, app.LocaleString (IDS_CONFIRMEXIT_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_CONFIRMEXITTIMER_CHK, app.LocaleString (IDS_CONFIRMEXITTIMER_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_CONFIRMDELETE_CHK, app.LocaleString (IDS_CONFIRMDELETE_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_CONFIRMLOGCLEAR_CHK, app.LocaleString (IDS_CONFIRMLOGCLEAR_CHK, nullptr));

					break;
				}

				case IDD_SETTINGS_HIGHLIGHTING:
				{
					SetDlgItemText (hwnd, IDC_COLORS_HINT, app.LocaleString (IDS_COLORS_HINT, nullptr));

					for (size_t i = 0; i < _r_listview_getitemcount (hwnd, IDC_COLORS); i++)
					{
						const size_t idx = (size_t)_r_listview_getitemlparam (hwnd, IDC_COLORS, i);
						PITEM_COLOR const ptr_clr = colors.at (idx);

						if (ptr_clr)
							_r_listview_setitem (hwnd, IDC_COLORS, i, 0, app.LocaleString (ptr_clr->locale_id, nullptr));
					}

					_app_listviewsetfont (hwnd, IDC_COLORS, false);

					break;
				}

				case IDD_SETTINGS_RULES:
				{
					const rstring recommended = app.LocaleString (IDS_RECOMMENDED, nullptr);

					SetDlgItemText (hwnd, IDC_RULE_ALLOWINBOUND, app.LocaleString (IDS_RULE_ALLOWINBOUND, nullptr));
					SetDlgItemText (hwnd, IDC_RULE_ALLOWLISTEN, app.LocaleString (IDS_RULE_ALLOWLISTEN, _r_fmt (L" (%s)", recommended.GetString ())));
					SetDlgItemText (hwnd, IDC_RULE_ALLOWLOOPBACK, app.LocaleString (IDS_RULE_ALLOWLOOPBACK, _r_fmt (L" (%s)", recommended.GetString ())));
					SetDlgItemText (hwnd, IDC_RULE_ALLOW6TO4, app.LocaleString (IDS_RULE_ALLOW6TO4, _r_fmt (L" (%s)", recommended.GetString ())));

					SetDlgItemText (hwnd, IDC_USECERTIFICATES_CHK, app.LocaleString (IDS_USECERTIFICATES_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_USENETWORKRESOLUTION_CHK, app.LocaleString (IDS_USENETWORKRESOLUTION_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_USEREFRESHDEVICES_CHK, app.LocaleString (IDS_USEREFRESHDEVICES_CHK, _r_fmt (L" (%s)", app.LocaleString (IDS_RECOMMENDED, nullptr).GetString ())));

					SetDlgItemText (hwnd, IDC_USESTEALTHMODE_CHK, app.LocaleString (IDS_USESTEALTHMODE_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_INSTALLBOOTTIMEFILTERS_CHK, app.LocaleString (IDS_INSTALLBOOTTIMEFILTERS_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_SECUREFILTERS_CHK, app.LocaleString (IDS_SECUREFILTERS_CHK, nullptr));

					break;
				}

				case IDD_SETTINGS_LOG:
				{
					SetDlgItemText (hwnd, IDC_ENABLELOG_CHK, app.LocaleString (IDS_ENABLELOG_CHK, nullptr));

					SetDlgItemText (hwnd, IDC_LOGSIZELIMIT_HINT, app.LocaleString (IDS_LOGSIZELIMIT_HINT, nullptr));

					SetDlgItemText (hwnd, IDC_ENABLENOTIFICATIONS_CHK, app.LocaleString (IDS_ENABLENOTIFICATIONS_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_NOTIFICATIONSOUND_CHK, app.LocaleString (IDS_NOTIFICATIONSOUND_CHK, nullptr));

					SetDlgItemText (hwnd, IDC_NOTIFICATIONDISPLAYTIMEOUT_HINT, app.LocaleString (IDS_NOTIFICATIONDISPLAYTIMEOUT_HINT, nullptr));
					SetDlgItemText (hwnd, IDC_NOTIFICATIONTIMEOUT_HINT, app.LocaleString (IDS_NOTIFICATIONTIMEOUT_HINT, nullptr));

					SetDlgItemText (hwnd, IDC_EXCLUDE1, app.LocaleString (IDS_TITLE_EXCLUDE, L":"));
					SetDlgItemText (hwnd, IDC_EXCLUDE2, app.LocaleString (IDS_TITLE_EXCLUDE, L":"));

					SetDlgItemText (hwnd, IDC_EXCLUDESTEALTH_CHK, app.LocaleString (IDS_EXCLUDESTEALTH_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, app.LocaleString (IDS_EXCLUDECLASSIFYALLOW_CHK, (_r_sys_validversion (6, 2) ? nullptr : L" [win8+]")));
					SetDlgItemText (hwnd, IDC_EXCLUDEBLOCKLIST_CHK, app.LocaleString (IDS_EXCLUDEBLOCKLIST_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_EXCLUDECUSTOM_CHK, app.LocaleString (IDS_EXCLUDECUSTOM_CHK, nullptr));

					_r_wnd_addstyle (hwnd, IDC_LOGPATH_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

					break;
				}
			}

			break;
		}

		case WM_VSCROLL:
		case WM_HSCROLL:
		{
			const UINT ctrl_id = GetDlgCtrlID ((HWND)lparam);

			if (ctrl_id == IDC_LOGSIZELIMIT)
				app.ConfigSet (L"LogSizeLimitKb", (DWORD)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));

			else if (ctrl_id == IDC_NOTIFICATIONDISPLAYTIMEOUT)
				app.ConfigSet (L"NotificationsDisplayTimeout", (DWORD)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));

			else if (ctrl_id == IDC_NOTIFICATIONTIMEOUT)
				app.ConfigSet (L"NotificationsTimeout", (DWORD)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));

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

					if ((lpnmdi->uFlags & TTF_IDISHWND) != 0)
					{
						WCHAR buffer[1024] = {0};
						const UINT ctrl_id = GetDlgCtrlID ((HWND)lpnmdi->hdr.idFrom);

						if (ctrl_id == IDC_RULE_ALLOWINBOUND)
							StringCchCopy (buffer, _countof (buffer), app.LocaleString (IDS_RULE_ALLOWINBOUND_HINT, nullptr));

						else if (ctrl_id == IDC_RULE_ALLOWLISTEN)
							StringCchCopy (buffer, _countof (buffer), app.LocaleString (IDS_RULE_ALLOWLISTEN_HINT, nullptr));

						else if (ctrl_id == IDC_RULE_ALLOWLOOPBACK)
							StringCchCopy (buffer, _countof (buffer), app.LocaleString (IDS_RULE_ALLOWLOOPBACK_HINT, nullptr));

						else if (ctrl_id == IDC_USESTEALTHMODE_CHK)
							StringCchCopy (buffer, _countof (buffer), app.LocaleString (IDS_USESTEALTHMODE_HINT, nullptr));

						else if (ctrl_id == IDC_INSTALLBOOTTIMEFILTERS_CHK)
							StringCchCopy (buffer, _countof (buffer), app.LocaleString (IDS_INSTALLBOOTTIMEFILTERS_HINT, nullptr));

						else if (ctrl_id == IDC_SECUREFILTERS_CHK)
							StringCchCopy (buffer, _countof (buffer), app.LocaleString (IDS_SECUREFILTERS_HINT, nullptr));

						if (buffer[0])
							lpnmdi->lpszText = buffer;
					}

					break;
				}

				case LVN_ITEMCHANGED:
				{
					if (_r_fastlock_islocked (&lock_checkbox))
						break;

					LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lparam;

					if (lpnmlv->hdr.idFrom != IDC_COLORS)
						break;

					if (lpnmlv->uNewState == 8192 || lpnmlv->uNewState == 4096)
					{
						const bool new_val = (lpnmlv->uNewState == 8192) ? true : false;

						const size_t idx = lpnmlv->lParam;
						PITEM_COLOR ptr_clr = colors.at (idx);

						if (ptr_clr)
							app.ConfigSet (ptr_clr->pcfg_name, new_val);

						_r_listview_redraw (app.GetHWND (), _app_gettab_id (app.GetHWND ()));
					}

					break;
				}

				case NM_CUSTOMDRAW:
				{
					_r_fastlock_acquireshared (&lock_access);
					const LONG result = _app_nmcustdraw ((LPNMLVCUSTOMDRAW)lparam);
					_r_fastlock_releaseshared (&lock_access);

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, result);
					return result;
				}

				case NM_DBLCLK:
				{
					LPNMITEMACTIVATE lpnmlv = (LPNMITEMACTIVATE)lparam;

					if (lpnmlv->iItem == -1 || nmlp->idFrom != IDC_COLORS)
						break;

					const size_t idx = (size_t)_r_listview_getitemlparam (hwnd, (UINT)nmlp->idFrom, lpnmlv->iItem);

					CHOOSECOLOR cc = {0};
					COLORREF cust[16] = {0};

					for (size_t i = 0; i < min (_countof (cust), colors.size ()); i++)
						cust[i] = colors.at (i)->default_clr;

					cc.lStructSize = sizeof (cc);
					cc.Flags = CC_RGBINIT | CC_FULLOPEN;
					cc.hwndOwner = hwnd;
					cc.lpCustColors = cust;
					cc.rgbResult = colors.at (idx)->clr;

					if (ChooseColor (&cc))
					{
						PITEM_COLOR ptr_clr = colors.at (idx);

						if (ptr_clr)
						{
							ptr_clr->clr = cc.rgbResult;
							app.ConfigSet (ptr_clr->pcfg_value, cc.rgbResult);
						}

						_r_listview_redraw (hwnd, IDC_COLORS);
						_r_listview_redraw (app.GetHWND (), _app_gettab_id (app.GetHWND ()));
					}

					break;
				}

				case LVN_GETEMPTYMARKUP:
				{
					NMLVEMPTYMARKUP *lpnmlv = (NMLVEMPTYMARKUP *)lparam;

					lpnmlv->dwFlags = EMF_CENTERED;
					StringCchCopy (lpnmlv->szMarkup, _countof (lpnmlv->szMarkup), app.LocaleString (IDS_STATUS_EMPTY, nullptr));

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
					return TRUE;
				}
			}

			break;
		}

		case WM_CONTEXTMENU:
		{
			const UINT ctrl_id = GetDlgCtrlID ((HWND)wparam);
			const LONG_PTR dialog_id = GetWindowLongPtr (hwnd, GWLP_USERDATA);

			if (ctrl_id != IDC_COLORS)
				break;

			const HMENU hmenu = CreateMenu ();
			const HMENU hsubmenu = CreateMenu ();

			AppendMenu (hmenu, MF_POPUP, (UINT_PTR)hsubmenu, L" ");

			AppendMenu (hsubmenu, MF_BYCOMMAND, IDM_CHECK, app.LocaleString (IDS_CHECK, nullptr));
			AppendMenu (hsubmenu, MF_BYCOMMAND, IDM_UNCHECK, app.LocaleString (IDS_UNCHECK, nullptr));

			if (!SendDlgItemMessage (hwnd, ctrl_id, LVM_GETSELECTEDCOUNT, 0, 0))
			{
				EnableMenuItem (hsubmenu, IDM_CHECK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
				EnableMenuItem (hsubmenu, IDM_UNCHECK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
			}

			POINT pt = {0};
			GetCursorPos (&pt);

			TrackPopupMenuEx (hsubmenu, TPM_RIGHTBUTTON | TPM_LEFTBUTTON, pt.x, pt.y, hwnd, nullptr);

			DestroyMenu (hsubmenu);
			DestroyMenu (hmenu);

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
				case IDC_CONFIRMDELETE_CHK:
				case IDC_CONFIRMLOGCLEAR_CHK:
				case IDC_USESTEALTHMODE_CHK:
				case IDC_INSTALLBOOTTIMEFILTERS_CHK:
				case IDC_SECUREFILTERS_CHK:
				case IDC_USECERTIFICATES_CHK:
				case IDC_USENETWORKRESOLUTION_CHK:
				case IDC_USEREFRESHDEVICES_CHK:
				case IDC_RULE_ALLOWINBOUND:
				case IDC_RULE_ALLOWLISTEN:
				case IDC_RULE_ALLOWLOOPBACK:
				case IDC_RULE_ALLOW6TO4:
				case IDC_ENABLELOG_CHK:
				case IDC_LOGPATH:
				case IDC_LOGPATH_BTN:
				case IDC_LOGSIZELIMIT_CTRL:
				case IDC_ENABLENOTIFICATIONS_CHK:
				case IDC_NOTIFICATIONSOUND_CHK:
				case IDC_NOTIFICATIONDISPLAYTIMEOUT_CTRL:
				case IDC_NOTIFICATIONTIMEOUT_CTRL:
				case IDC_EXCLUDESTEALTH_CHK:
				case IDC_EXCLUDECLASSIFYALLOW_CHK:
				case IDC_EXCLUDEBLOCKLIST_CHK:
				case IDC_EXCLUDECUSTOM_CHK:
				{
					const UINT ctrl_id = LOWORD (wparam);
					const UINT notify_code = HIWORD (wparam);

					if (ctrl_id == IDC_ALWAYSONTOP_CHK)
					{
						app.ConfigSet (L"AlwaysOnTop", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
						CheckMenuItem (GetMenu (app.GetHWND ()), IDM_ALWAYSONTOP_CHK, MF_BYCOMMAND | ((IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? MF_CHECKED : MF_UNCHECKED));
					}
					else if (ctrl_id == IDC_STARTMINIMIZED_CHK)
					{
						app.ConfigSet (L"IsStartMinimized", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
					}
					else if (ctrl_id == IDC_LOADONSTARTUP_CHK)
					{
						app.AutorunEnable (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);
					}
					else if (ctrl_id == IDC_SKIPUACWARNING_CHK)
					{
						app.SkipUacEnable (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);
					}
					else if (ctrl_id == IDC_CHECKUPDATES_CHK)
					{
						app.ConfigSet (L"CheckUpdates", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);

#if !defined(_APP_BETA) && !defined(_APP_BETA_RC)
						_r_ctrl_enable (hwnd, IDC_CHECKUPDATESBETA_CHK, (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
#endif
					}
					else if (ctrl_id == IDC_CHECKUPDATESBETA_CHK)
					{
						app.ConfigSet (L"CheckUpdatesBeta", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
					}
					else if (ctrl_id == IDC_LANGUAGE && notify_code == CBN_SELCHANGE)
					{
						app.LocaleApplyFromControl (hwnd, ctrl_id);
					}
					else if (ctrl_id == IDC_CONFIRMEXIT_CHK)
					{
						app.ConfigSet (L"ConfirmExit2", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
					}
					else if (ctrl_id == IDC_CONFIRMEXITTIMER_CHK)
					{
						app.ConfigSet (L"ConfirmExitTimer", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
					}
					else if (ctrl_id == IDC_CONFIRMDELETE_CHK)
					{
						app.ConfigSet (L"ConfirmDelete", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
					}
					else if (ctrl_id == IDC_CONFIRMLOGCLEAR_CHK)
					{
						app.ConfigSet (L"ConfirmLogClear", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
					}
					else if (ctrl_id == IDC_USECERTIFICATES_CHK)
					{
						app.ConfigSet (L"IsCerificatesEnabled", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);

						if (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED)
						{
							_r_fastlock_acquireexclusive (&lock_access);

							for (auto &p : apps)
							{
								PITEM_APP ptr_app = &p.second;

								ptr_app->is_signed = _app_getsignatureinfo (p.first, ptr_app->real_path, &ptr_app->signer);
							}

							_r_fastlock_releaseexclusive (&lock_access);
						}

						_r_listview_redraw (app.GetHWND (), _app_gettab_id (app.GetHWND ()));
					}
					else if (ctrl_id == IDC_USENETWORKRESOLUTION_CHK)
					{
						app.ConfigSet (L"IsNetworkResolutionsEnabled", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);

						_app_notifyrefresh (config.hnotification, false);
					}
					else if (ctrl_id == IDC_USEREFRESHDEVICES_CHK)
					{
						app.ConfigSet (L"IsRefreshDevices", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
					}
					else if (ctrl_id == IDC_RULE_ALLOWINBOUND)
					{
						if (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED && !app.ConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION_EXPERT, nullptr), L"ConfirmExpert"))
						{
							CheckDlgButton (hwnd, ctrl_id, BST_UNCHECKED);
							return TRUE;
						}

						app.ConfigSet (L"AllowInboundConnections", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);

						_wfp_create2filters (__LINE__);
					}
					else if (ctrl_id == IDC_RULE_ALLOWLISTEN)
					{
						if (IsDlgButtonChecked (hwnd, ctrl_id) == BST_UNCHECKED && !app.ConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION_EXPERT, nullptr), L"ConfirmExpert"))
						{
							CheckDlgButton (hwnd, ctrl_id, BST_CHECKED);
							return TRUE;
						}

						app.ConfigSet (L"AllowListenConnections2", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);

						_wfp_create2filters (__LINE__);
					}
					else if (ctrl_id == IDC_RULE_ALLOWLOOPBACK)
					{
						if (IsDlgButtonChecked (hwnd, ctrl_id) == BST_UNCHECKED && !app.ConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION_EXPERT, nullptr), L"ConfirmExpert"))
						{
							CheckDlgButton (hwnd, ctrl_id, BST_CHECKED);
							return TRUE;
						}

						app.ConfigSet (L"AllowLoopbackConnections", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);

						_wfp_create2filters (__LINE__);
					}
					else if (ctrl_id == IDC_RULE_ALLOW6TO4)
					{
						if (IsDlgButtonChecked (hwnd, ctrl_id) == BST_UNCHECKED && !app.ConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION_EXPERT, nullptr), L"ConfirmExpert"))
						{
							CheckDlgButton (hwnd, ctrl_id, BST_CHECKED);
							return TRUE;
						}

						app.ConfigSet (L"AllowIPv6", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);

						_wfp_create2filters (__LINE__);
					}
					else if (ctrl_id == IDC_USESTEALTHMODE_CHK)
					{
						if (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED && !app.ConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION_EXPERT, nullptr), L"ConfirmExpert"))
						{
							CheckDlgButton (hwnd, ctrl_id, BST_UNCHECKED);
							return TRUE;
						}

						app.ConfigSet (L"UseStealthMode", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);

						_wfp_create2filters (__LINE__);
					}
					else if (ctrl_id == IDC_INSTALLBOOTTIMEFILTERS_CHK)
					{
						if (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED && !app.ConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION_EXPERT, nullptr), L"ConfirmExpert"))
						{
							CheckDlgButton (hwnd, ctrl_id, BST_UNCHECKED);
							return true;
						}

						app.ConfigSet (L"InstallBoottimeFilters", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);

						_wfp_create2filters (__LINE__);
					}
					else if (ctrl_id == IDC_SECUREFILTERS_CHK)
					{
						if (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED && !app.ConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION_EXPERT, nullptr), L"ConfirmExpert"))
						{
							CheckDlgButton (hwnd, ctrl_id, BST_UNCHECKED);
							return true;
						}

						app.ConfigSet (L"IsSecureFilters", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);

						_app_changefilters (app.GetHWND (), true, false);
					}
					else if (ctrl_id == IDC_ENABLELOG_CHK)
					{
						const bool is_enabled = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

						app.ConfigSet (L"IsLogEnabled", is_enabled);
						CheckMenuItem (GetMenu (app.GetHWND ()), IDM_ENABLELOG_CHK, MF_BYCOMMAND | (is_enabled ? MF_CHECKED : MF_UNCHECKED));

						_r_ctrl_enable (hwnd, IDC_LOGPATH, is_enabled); // input
						_r_ctrl_enable (hwnd, IDC_LOGPATH_BTN, is_enabled); // button

						EnableWindow ((HWND)SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_GETBUDDY, 0, 0), is_enabled);

						_r_ctrl_enable (hwnd, IDC_EXCLUDESTEALTH_CHK, is_enabled);

						if (_r_sys_validversion (6, 2))
							_r_ctrl_enable (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, is_enabled);

						_app_loginit (is_enabled);
					}
					else if (ctrl_id == IDC_LOGPATH && notify_code == EN_KILLFOCUS)
					{
						app.ConfigSet (L"LogPath", _r_ctrl_gettext (hwnd, ctrl_id));

						_app_loginit (app.ConfigGet (L"IsLogEnabled", false));
					}
					else if (ctrl_id == IDC_LOGPATH_BTN)
					{
						OPENFILENAME ofn = {0};

						WCHAR path[MAX_PATH] = {0};
						GetDlgItemText (hwnd, IDC_LOGPATH, path, _countof (path));
						StringCchCopy (path, _countof (path), _r_path_expand (path));

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
							StringCchCopy (path, _countof (path), _r_path_unexpand (path));

							app.ConfigSet (L"LogPath", path);
							SetDlgItemText (hwnd, IDC_LOGPATH, path);

							_app_loginit (app.ConfigGet (L"IsLogEnabled", false));

						}
					}
					else if (ctrl_id == IDC_LOGSIZELIMIT_CTRL && notify_code == EN_KILLFOCUS)
					{
						app.ConfigSet (L"LogSizeLimitKb", (DWORD)SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_GETPOS32, 0, 0));
					}
					else if (ctrl_id == IDC_ENABLENOTIFICATIONS_CHK)
					{
						const bool is_enabled = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

						app.ConfigSet (L"IsNotificationsEnabled", is_enabled);
						CheckMenuItem (GetMenu (app.GetHWND ()), IDM_ENABLENOTIFICATIONS_CHK, MF_BYCOMMAND | (is_enabled ? MF_CHECKED : MF_UNCHECKED));

						_r_ctrl_enable (hwnd, IDC_NOTIFICATIONSOUND_CHK, is_enabled);

						EnableWindow ((HWND)SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_GETBUDDY, 0, 0), is_enabled);
						EnableWindow ((HWND)SendDlgItemMessage (hwnd, IDC_NOTIFICATIONDISPLAYTIMEOUT, UDM_GETBUDDY, 0, 0), is_enabled);

						_r_ctrl_enable (hwnd, IDC_EXCLUDEBLOCKLIST_CHK, is_enabled);
						_r_ctrl_enable (hwnd, IDC_EXCLUDECUSTOM_CHK, is_enabled);

						_app_notifyrefresh (config.hnotification, false);
					}
					else if (ctrl_id == IDC_NOTIFICATIONSOUND_CHK)
					{
						app.ConfigSet (L"IsNotificationsSound", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					}
					else if (ctrl_id == IDC_NOTIFICATIONDISPLAYTIMEOUT_CTRL && notify_code == EN_KILLFOCUS)
					{
						app.ConfigSet (L"NotificationsDisplayTimeout", (DWORD)SendDlgItemMessage (hwnd, IDC_NOTIFICATIONDISPLAYTIMEOUT, UDM_GETPOS32, 0, 0));
					}
					else if (ctrl_id == IDC_NOTIFICATIONTIMEOUT_CTRL && notify_code == EN_KILLFOCUS)
					{
						app.ConfigSet (L"NotificationsTimeout", (DWORD)SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_GETPOS32, 0, 0));
					}
					else if (ctrl_id == IDC_EXCLUDESTEALTH_CHK)
					{
						app.ConfigSet (L"IsExcludeStealth", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					}
					else if (ctrl_id == IDC_EXCLUDECLASSIFYALLOW_CHK)
					{
						app.ConfigSet (L"IsExcludeClassifyAllow", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					}
					else if (ctrl_id == IDC_EXCLUDEBLOCKLIST_CHK)
					{
						app.ConfigSet (L"IsExcludeBlocklist", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					}
					else if (ctrl_id == IDC_EXCLUDECUSTOM_CHK)
					{
						app.ConfigSet (L"IsExcludeCustomRules", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					}

					break;
				}

				case IDM_CHECK:
				case IDM_UNCHECK:
				{
					const LONG_PTR dialog_id = GetWindowLongPtr (hwnd, GWLP_USERDATA);

					if (dialog_id == IDD_SETTINGS_HIGHLIGHTING)
					{
						size_t item = LAST_VALUE;
						const bool new_val = (LOWORD (wparam) == IDM_CHECK) ? true : false;

						_r_fastlock_acquireshared (&lock_checkbox);

						while ((item = (size_t)SendDlgItemMessage (hwnd, IDC_COLORS, LVM_GETNEXTITEM, item, LVNI_SELECTED)) != LAST_VALUE)
							_r_listview_setitemcheck (hwnd, IDC_COLORS, item, new_val);

						_r_fastlock_releaseshared (&lock_checkbox);
					}

					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

void _app_resizewindow (HWND hwnd, INT width, INT height)
{
	RECT rc = {0};
	RECT tab_rc = {0};

	GetClientRect (GetDlgItem (hwnd, IDC_EXIT_BTN), &rc);
	const INT button_width = _R_RECT_WIDTH (&rc);
	const INT padding_size = GetSystemMetrics (SM_CXVSCROLL) - (GetSystemMetrics (SM_CXBORDER) * 2);

	GetClientRect (GetDlgItem (hwnd, IDC_STATUSBAR), &rc);
	const INT statusbar_height = _R_RECT_HEIGHT (&rc);

	const INT button_top = height - statusbar_height - app.GetDPI (1 + 34);

	{
		RECT rc2 = {0};
		GetWindowRect (GetDlgItem (hwnd, IDC_TAB), &rc2);
		MapWindowPoints (nullptr, hwnd, (LPPOINT)& rc2, 2);

		GetClientRect (GetDlgItem (hwnd, IDC_TAB), &tab_rc);

		tab_rc.left += rc2.left;
		tab_rc.top += rc2.top;

		tab_rc.right += rc2.left;
		tab_rc.bottom += rc2.top;

		TabCtrl_AdjustRect (GetDlgItem (hwnd, IDC_TAB), 0, &tab_rc);
	}

	HDWP hdefer = BeginDeferWindowPos (5);
	const UINT listview_id = _app_gettab_id (hwnd);

	_r_wnd_resize (&hdefer, GetDlgItem (hwnd, IDC_TAB), nullptr, 0, 0, width, height - statusbar_height - app.GetDPI (1 + 46), SWP_NOREDRAW);
	_r_wnd_resize (&hdefer, GetDlgItem (hwnd, listview_id), nullptr, tab_rc.left, tab_rc.top, _R_RECT_WIDTH (&tab_rc), _R_RECT_HEIGHT (&tab_rc), SWP_NOREDRAW);
	_r_wnd_resize (&hdefer, GetDlgItem (hwnd, IDC_START_BTN), nullptr, padding_size, button_top, 0, 0, SWP_NOREDRAW);
	_r_wnd_resize (&hdefer, GetDlgItem (hwnd, IDC_SETTINGS_BTN), nullptr, width - padding_size - button_width - button_width - app.GetDPI (6), button_top, 0, 0, SWP_NOREDRAW);
	_r_wnd_resize (&hdefer, GetDlgItem (hwnd, IDC_EXIT_BTN), nullptr, width - padding_size - button_width, button_top, 0, 0, SWP_NOREDRAW);

	EndDeferWindowPos (hdefer);

	// resize statusbar
	SendDlgItemMessage (hwnd, IDC_STATUSBAR, WM_SIZE, 0, 0);
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
	_r_fastlock_initialize (&lock_notification);
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
	config.tmp1_length = GetTempPath (_countof (config.tmp1_dir), config.tmp1_dir);
	GetLongPathName (rstring (config.tmp1_dir), config.tmp1_dir, _countof (config.tmp1_dir));

	StringCchPrintf (config.apps_path, _countof (config.apps_path), L"%s\\" XML_APPS, app.GetProfileDirectory ());
	StringCchPrintf (config.rules_blocklist_path, _countof (config.rules_blocklist_path), L"%s\\" XML_BLOCKLIST, app.GetProfileDirectory ());
	StringCchPrintf (config.rules_system_path, _countof (config.rules_system_path), L"%s\\" XML_RULES_SYSTEM, app.GetProfileDirectory ());
	StringCchPrintf (config.rules_custom_path, _countof (config.rules_custom_path), L"%s\\" XML_RULES_CUSTOM, app.GetProfileDirectory ());
	StringCchPrintf (config.rules_config_path, _countof (config.rules_config_path), L"%s\\" XML_RULES_CONFIG, app.GetProfileDirectory ());

	StringCchPrintf (config.apps_path_backup, _countof (config.apps_path_backup), L"%s\\" XML_APPS L".bak", app.GetProfileDirectory ());
	StringCchPrintf (config.rules_config_path_backup, _countof (config.rules_config_path_backup), L"%s\\" XML_RULES_CONFIG L".bak", app.GetProfileDirectory ());
	StringCchPrintf (config.rules_custom_path_backup, _countof (config.rules_custom_path_backup), L"%s\\" XML_RULES_CUSTOM L".bak", app.GetProfileDirectory ());

	config.ntoskrnl_hash = _r_str_hash (PROC_SYSTEM_NAME);
	config.svchost_hash = _r_str_hash (_r_path_expand (PATH_SVCHOST));
	config.myhash = _r_str_hash (app.GetBinaryPath ());

	// get current user security identifier
	if (!config.pusersid)
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

				if (token_user)
				{
					if (GetTokenInformation (token, TokenUser, token_user, token_length, &token_length))
					{
						SID_NAME_USE sid_type;

						WCHAR username[MAX_PATH] = {0};
						WCHAR domain[MAX_PATH] = {0};

						DWORD length1 = _countof (username);
						DWORD length2 = _countof (domain);

						if (LookupAccountSid (nullptr, token_user->User.Sid, username, &length1, domain, &length2, &sid_type))
							StringCchPrintf (config.title, _countof (config.title), L"%s [%s\\%s]", APP_NAME, domain, username);

						config.pusersid = new BYTE[SECURITY_MAX_SID_SIZE];

						if (config.pusersid)
							CopyMemory (config.pusersid, token_user->User.Sid, SECURITY_MAX_SID_SIZE);
					}

					SAFE_DELETE_ARRAY (token_user);
				}
			}

			CloseHandle (token);
		}

		if (!config.title[0])
			StringCchCopy (config.title, _countof (config.title), APP_NAME); // fallback
	}

	// initialize timers
	{
		if (config.htimer)
			DeleteTimerQueue (config.htimer);

		config.htimer = CreateTimerQueue ();

		timers.clear ();

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

	// initialize winsock (required by getnameinfo)
	if (!config.is_wsainit)
	{
		WSADATA wsaData = {0};

		if (WSAStartup (WINSOCK_VERSION, &wsaData) == ERROR_SUCCESS)
			config.is_wsainit = true;
	}
}

INT_PTR CALLBACK DlgProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
		case WM_INITDIALOG:
		{
			_app_initialize ();

#ifndef _APP_NO_DARKTHEME
			_r_wnd_setdarktheme (hwnd);
#endif // _APP_NO_DARKTHEME

			// init buffered paint
			BufferedPaintInit ();

			// allow drag&drop support
			DragAcceptFiles (hwnd, TRUE);

			// get default icon for executable
			_app_getfileicon (_r_path_expand (PATH_NTOSKRNL), false, &config.icon_id, &config.hicon_large);
			_app_getfileicon (_r_path_expand (PATH_NTOSKRNL), true, nullptr, &config.hicon_small);

			// get default icon for windows store package (win8+)
			if (_r_sys_validversion (6, 2))
			{
				if (!_app_getfileicon (_r_path_expand (PATH_WINSTORE), true, &config.icon_package_id, &config.hicon_package))
				{
					config.icon_package_id = config.icon_id;
					config.hicon_package = config.hicon_small;
				}
			}

			// load settings imagelist
			{
				static const INT cx_width = GetSystemMetrics (SM_CXSMICON);

				config.himg = ImageList_Create (cx_width, cx_width, ILC_COLOR32 | ILC_MASK, 0, 5);

				if (config.himg)
				{
					ImageList_AddIcon (config.himg, app.GetSharedIcon (app.GetHINSTANCE (), IDI_ALLOW, cx_width));
					ImageList_AddIcon (config.himg, app.GetSharedIcon (app.GetHINSTANCE (), IDI_BLOCK, cx_width));
				}
			}

			// initialize settings
			app.SettingsAddPage (IDD_SETTINGS_GENERAL, IDS_SETTINGS_GENERAL);
			app.SettingsAddPage (IDD_SETTINGS_INTERFACE, IDS_TITLE_INTERFACE);
			app.SettingsAddPage (IDD_SETTINGS_HIGHLIGHTING, IDS_TITLE_HIGHLIGHTING);
			app.SettingsAddPage (IDD_SETTINGS_RULES, IDS_TRAY_RULES);

			// dropped packets logging (win7+)
			app.SettingsAddPage (IDD_SETTINGS_LOG, IDS_TRAY_LOG);

			// initialize colors
			{
				addcolor (IDS_HIGHLIGHT_TIMER, L"IsHighlightTimer", true, L"ColorTimer", LISTVIEW_COLOR_TIMER);
				addcolor (IDS_HIGHLIGHT_INVALID, L"IsHighlightInvalid", true, L"ColorInvalid", LISTVIEW_COLOR_INVALID);
				addcolor (IDS_HIGHLIGHT_SPECIAL, L"IsHighlightSpecial", true, L"ColorSpecial", LISTVIEW_COLOR_SPECIAL);
				addcolor (IDS_HIGHLIGHT_SILENT, L"IsHighlightSilent", true, L"ColorSilent", LISTVIEW_COLOR_SILENT);
				addcolor (IDS_HIGHLIGHT_SIGNED, L"IsHighlightSigned", true, L"ColorSigned", LISTVIEW_COLOR_SIGNED);
				addcolor (IDS_HIGHLIGHT_PICO, L"IsHighlightPico", true, L"ColorPico", LISTVIEW_COLOR_PICO);
				addcolor (IDS_HIGHLIGHT_SYSTEM, L"IsHighlightSystem", true, L"ColorSystem", LISTVIEW_COLOR_SYSTEM);
			}

			// initialize protocols
			{
				addprotocol (L"TCP", IPPROTO_TCP);
				addprotocol (L"UDP", IPPROTO_UDP);
				addprotocol (L"ICMPv4", IPPROTO_ICMP);
				addprotocol (L"ICMPv6", IPPROTO_ICMPV6);
				addprotocol (L"IPv4", IPPROTO_IPV4);
				addprotocol (L"IPv6", IPPROTO_IPV6);
				addprotocol (L"IGMP", IPPROTO_IGMP);
				addprotocol (L"L2TP", IPPROTO_L2TP);
				addprotocol (L"SCTP", IPPROTO_SCTP);
				addprotocol (L"RDP", IPPROTO_RDP);
				addprotocol (L"RAW", IPPROTO_RAW);
			}

			// initialize dropped packets log callback thread (win7+)
			if (_r_sys_validversion (6, 1))
			{
				// create notification window
				_app_notifycreatewindow ();

				// initialize slist
				{
					SecureZeroMemory (&log_stack, sizeof (log_stack));

					log_stack.item_count = 0;
					log_stack.thread_count = 0;

					RtlInitializeSListHead (&log_stack.ListHead);
				}
			}

			// configure tabs
			_r_tab_additem (hwnd, IDC_TAB, 0, nullptr, LAST_VALUE, IDC_APPS_PROFILE);
			_r_tab_additem (hwnd, IDC_TAB, 1, nullptr, LAST_VALUE, IDC_APPS_SERVICE);

			// windows store apps (win8+)
			if (_r_sys_validversion (6, 2))
				_r_tab_additem (hwnd, IDC_TAB, 2, nullptr, LAST_VALUE, IDC_APPS_PACKAGE);

			_r_tab_additem (hwnd, IDC_TAB, 3, nullptr, LAST_VALUE, IDC_RULES_BLOCKLIST);
			_r_tab_additem (hwnd, IDC_TAB, 4, nullptr, LAST_VALUE, IDC_RULES_SYSTEM);
			_r_tab_additem (hwnd, IDC_TAB, 5, nullptr, LAST_VALUE, IDC_RULES_CUSTOM);

			// configure listview
			for (size_t i = 0; i < (size_t)SendDlgItemMessage (hwnd, IDC_TAB, TCM_GETITEMCOUNT, 0, 0); i++)
			{
				const UINT listview_id = _app_gettab_id (hwnd, i);

				if (
					listview_id == IDC_APPS_PROFILE ||
					listview_id == IDC_APPS_SERVICE ||
					listview_id == IDC_APPS_PACKAGE ||
					listview_id == IDC_RULES_BLOCKLIST ||
					listview_id == IDC_RULES_SYSTEM ||
					listview_id == IDC_RULES_CUSTOM
					)
				{
					_r_listview_setstyle (hwnd, listview_id, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_CHECKBOXES | LVS_EX_HEADERINALLVIEWS);

					if (
						listview_id == IDC_APPS_PROFILE ||
						listview_id == IDC_APPS_SERVICE ||
						listview_id == IDC_APPS_PACKAGE
						)
					{
						_r_listview_addcolumn (hwnd, listview_id, 0, app.LocaleString (IDS_FILEPATH, nullptr), 70, LVCFMT_LEFT);
						_r_listview_addcolumn (hwnd, listview_id, 1, app.LocaleString (IDS_ADDED, nullptr), 26, LVCFMT_RIGHT);
					}
					else if (
						listview_id == IDC_RULES_BLOCKLIST ||
						listview_id == IDC_RULES_SYSTEM ||
						listview_id == IDC_RULES_CUSTOM
						)
					{
						_r_listview_addcolumn (hwnd, listview_id, 0, app.LocaleString (IDS_NAME, nullptr), 49, LVCFMT_LEFT);
						_r_listview_addcolumn (hwnd, listview_id, 1, app.LocaleString (IDS_DIRECTION, nullptr), 26, LVCFMT_RIGHT);
						_r_listview_addcolumn (hwnd, listview_id, 2, app.LocaleString (IDS_PROTOCOL, nullptr), 20, LVCFMT_RIGHT);
					}

					_r_listview_addgroup (hwnd, listview_id, 0, nullptr, 0, LVGS_COLLAPSIBLE | (app.ConfigGet (L"Group1IsCollaped", false).AsBool () ? LVGS_COLLAPSED : LVGS_NORMAL));
					_r_listview_addgroup (hwnd, listview_id, 1, nullptr, 0, LVGS_COLLAPSIBLE | (app.ConfigGet (L"Group2IsCollaped", false).AsBool () ? LVGS_COLLAPSED : LVGS_NORMAL));
					_r_listview_addgroup (hwnd, listview_id, 2, nullptr, 0, LVGS_COLLAPSIBLE | (app.ConfigGet (L"Group3IsCollaped", false).AsBool () ? LVGS_COLLAPSED : LVGS_NORMAL));
				}
				else
				{
					_r_listview_setstyle (hwnd, listview_id, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_HEADERINALLVIEWS);

					_r_listview_addcolumn (hwnd, listview_id, 0, app.LocaleString (IDS_FILEPATH, nullptr), 70, LVCFMT_LEFT);
					_r_listview_addcolumn (hwnd, listview_id, 1, app.LocaleString (IDS_ADDED, nullptr), 26, LVCFMT_RIGHT);
				}

				_app_listviewsetfont (hwnd, listview_id, false);
			}

			// load profile
			_app_profile_load (hwnd);

			// add blocklist to update
			app.UpdateAddComponent (L"Blocklist", L"blocklist", _r_fmt (L"%I64u", config.blocklist_timestamp), config.rules_blocklist_path, false);
			app.UpdateAddComponent (L"System rules", L"rules_system", _r_fmt (L"%I64u", config.rule_system_timestamp), config.rules_system_path, false);

			// install filters
			if (_wfp_isfiltersinstalled ())
			{
				if (app.ConfigGet (L"IsDisableWindowsFirewallChecked", true).AsBool ())
					_mps_changeconfig2 (false);

				_app_changefilters (hwnd, true, true);
			}

			// initialize tab
			{
				SendDlgItemMessage (hwnd, IDC_TAB, TCM_SETCURSEL, app.ConfigGet (L"CurrentTab", 0).AsInt (), 0);

				NMHDR hdr = {0};

				hdr.code = TCN_SELCHANGE;
				hdr.idFrom = IDC_TAB;

				SendMessage (hwnd, WM_NOTIFY, 0, (LPARAM)& hdr);
			}

			_app_setbuttonmargins (hwnd, IDC_START_BTN);

			break;
		}

		case RM_INITIALIZE:
		{
			if (app.ConfigGet (L"IsShowTitleID", true).AsBool ())
				SetWindowText (hwnd, config.title);

			else
				SetWindowText (hwnd, APP_NAME);

			if (!app.ConfigGet (L"AutoSizeColumns", true).AsBool ())
			{
				_r_listview_setcolumn (hwnd, IDC_APPS_PROFILE, 0, nullptr, -(app.ConfigGet (L"Column1Width", 70).AsInt ()));
				_r_listview_setcolumn (hwnd, IDC_APPS_PROFILE, 1, nullptr, -(app.ConfigGet (L"Column2Width", 26).AsInt ()));
			}

			app.TrayCreate (hwnd, UID, nullptr, WM_TRAYICON, app.GetSharedIcon (app.GetHINSTANCE (), IDI_ACTIVE, GetSystemMetrics (SM_CXSMICON)), true);

			_app_setinterfacestate ();

			CheckMenuItem (GetMenu (hwnd), IDM_ALWAYSONTOP_CHK, MF_BYCOMMAND | (app.ConfigGet (L"AlwaysOnTop", false).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (GetMenu (hwnd), IDM_SHOWFILENAMESONLY_CHK, MF_BYCOMMAND | (app.ConfigGet (L"ShowFilenames", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (GetMenu (hwnd), IDM_AUTOSIZECOLUMNS_CHK, MF_BYCOMMAND | (app.ConfigGet (L"AutoSizeColumns", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (GetMenu (hwnd), IDM_ENABLESPECIALGROUP_CHK, MF_BYCOMMAND | (app.ConfigGet (L"IsEnableSpecialGroup", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));

			CheckMenuRadioItem (GetMenu (hwnd), IDM_TRAY_MODEWHITELIST, IDM_TRAY_MODEBLACKLIST, IDM_TRAY_MODEWHITELIST + app.ConfigGet (L"Mode", ModeWhitelist).AsUint (), MF_BYCOMMAND);

			CheckMenuItem (GetMenu (hwnd), IDM_ENABLELOG_CHK, MF_BYCOMMAND | (app.ConfigGet (L"IsLogEnabled", false).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (GetMenu (hwnd), IDM_ENABLENOTIFICATIONS_CHK, MF_BYCOMMAND | (app.ConfigGet (L"IsNotificationsEnabled", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));

			// dropped packets logging (win7+)
			if (!_r_sys_validversion (6, 1))
			{
				EnableMenuItem (GetMenu (hwnd), IDM_ENABLELOG_CHK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
				EnableMenuItem (GetMenu (hwnd), IDM_ENABLENOTIFICATIONS_CHK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
			}

			break;
		}

		case RM_LOCALIZE:
		{
			const HMENU hmenu = GetMenu (hwnd);

			app.LocaleMenu (hmenu, IDS_FILE, 0, true, nullptr);
			app.LocaleMenu (GetSubMenu (hmenu, 0), IDS_EXPORT, 0, true, nullptr);
			app.LocaleMenu (GetSubMenu (hmenu, 0), IDS_IMPORT, 1, true, nullptr);
			app.LocaleMenu (hmenu, IDS_EXPORT, IDM_EXPORT_APPS, false, L" " XML_APPS L"\tCtrl+S");
			app.LocaleMenu (hmenu, IDS_EXPORT, IDM_EXPORT_RULES, false, L" " XML_RULES_CUSTOM L"\tCtrl+Shift+S");
			app.LocaleMenu (hmenu, IDS_IMPORT, IDM_IMPORT_APPS, false, L" " XML_APPS L"\tCtrl+O");
			app.LocaleMenu (hmenu, IDS_IMPORT, IDM_IMPORT_RULES, false, L" " XML_RULES_CUSTOM L"\tCtrl+Shift+O");
			app.LocaleMenu (hmenu, IDS_EXIT, IDM_EXIT, false, L"\tAlt+F4");

			app.LocaleMenu (hmenu, IDS_EDIT, 1, true, nullptr);

			app.LocaleMenu (hmenu, IDS_PURGE_UNUSED, IDM_PURGE_UNUSED, false, L"\tCtrl+Shift+X");
			app.LocaleMenu (hmenu, IDS_PURGE_TIMERS, IDM_PURGE_TIMERS, false, L"\tCtrl+Shift+T");

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

			app.LocaleMenu (hmenu, IDS_SETTINGS, 3, true, nullptr);

			app.LocaleMenu (GetSubMenu (hmenu, 3), IDS_TRAY_MODE, 0, true, nullptr);

			app.LocaleMenu (hmenu, IDS_MODE_WHITELIST, IDM_TRAY_MODEWHITELIST, false, nullptr);
			app.LocaleMenu (hmenu, IDS_MODE_BLACKLIST, IDM_TRAY_MODEBLACKLIST, false, nullptr);

			app.LocaleMenu (GetSubMenu (hmenu, 3), IDS_TRAY_LOG, 1, true, nullptr);

			app.LocaleMenu (hmenu, IDS_ENABLELOG_CHK, IDM_ENABLELOG_CHK, false, nullptr);
			app.LocaleMenu (hmenu, IDS_ENABLENOTIFICATIONS_CHK, IDM_ENABLENOTIFICATIONS_CHK, false, nullptr);
			app.LocaleMenu (hmenu, IDS_LOGSHOW, IDM_LOGSHOW, false, L"\tCtrl+I");
			app.LocaleMenu (hmenu, IDS_LOGCLEAR, IDM_LOGCLEAR, false, L"\tCtrl+X");

			app.LocaleMenu (hmenu, IDS_SETTINGS, IDM_SETTINGS, false, L"...\tF2");

			app.LocaleMenu (hmenu, IDS_HELP, 4, true, nullptr);
			app.LocaleMenu (hmenu, IDS_WEBSITE, IDM_WEBSITE, false, nullptr);
			app.LocaleMenu (hmenu, IDS_CHECKUPDATES, IDM_CHECKUPDATES, false, nullptr);
			app.LocaleMenu (hmenu, IDS_ABOUT, IDM_ABOUT, false, L"\tF1");

			app.LocaleEnum ((HWND)GetSubMenu (hmenu, 2), LANG_MENU, true, IDX_LANGUAGE); // enum localizations

			SetDlgItemText (hwnd, IDC_START_BTN, app.LocaleString (_wfp_isfiltersinstalled () ? IDS_TRAY_STOP : IDS_TRAY_START, nullptr));

			SetDlgItemText (hwnd, IDC_SETTINGS_BTN, app.LocaleString (IDS_SETTINGS, nullptr));
			SetDlgItemText (hwnd, IDC_EXIT_BTN, app.LocaleString (IDS_EXIT, nullptr));

			_r_wnd_addstyle (hwnd, IDC_START_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (hwnd, IDC_SETTINGS_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (hwnd, IDC_EXIT_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

			for (size_t i = 0; i < (size_t)SendDlgItemMessage (hwnd, IDC_TAB, TCM_GETITEMCOUNT, 0, 0); i++)
			{
				const UINT listview_id = _app_gettab_id (hwnd, i);

				UINT locale_id;

				if (listview_id == IDC_APPS_SERVICE)
					locale_id = IDS_TAB_SERVICES;

				else if (listview_id == IDC_APPS_PACKAGE)
					locale_id = IDS_TAB_PACKAGES;

				else if (listview_id == IDC_DROPPEDPACKETS)
					locale_id = IDS_TRAY_LOG;

				else if (listview_id == IDC_NETWORK)
					locale_id = IDS_TAB_NETWORK;

				else if (listview_id == IDC_RULES_BLOCKLIST)
					locale_id = IDS_TRAY_BLOCKLIST_RULES;

				else if (listview_id == IDC_RULES_SYSTEM)
					locale_id = IDS_TRAY_SYSTEM_RULES;

				else if (listview_id == IDC_RULES_CUSTOM)
					locale_id = IDS_TRAY_USER_RULES;

				else
					locale_id = IDS_TAB_APPS;

				_r_tab_setitem (hwnd, IDC_TAB, i, app.LocaleString (locale_id, nullptr));

				if (
					listview_id == IDC_APPS_PROFILE ||
					listview_id == IDC_APPS_SERVICE ||
					listview_id == IDC_APPS_PACKAGE
					)
				{
					_r_listview_setcolumn (hwnd, listview_id, 0, app.LocaleString (IDS_FILEPATH, nullptr), 0);
					_r_listview_setcolumn (hwnd, listview_id, 1, app.LocaleString (IDS_ADDED, nullptr), 0);
				}
				else if (
					listview_id == IDC_RULES_BLOCKLIST ||
					listview_id == IDC_RULES_SYSTEM ||
					listview_id == IDC_RULES_CUSTOM
					)
				{
					_r_listview_setcolumn (hwnd, listview_id, 0, app.LocaleString (IDS_NAME, nullptr), 0);
					_r_listview_setcolumn (hwnd, listview_id, 1, app.LocaleString (IDS_DIRECTION, nullptr), 0);
					_r_listview_setcolumn (hwnd, listview_id, 2, app.LocaleString (IDS_PROTOCOL, nullptr), 0);
				}

				SendDlgItemMessage (hwnd, listview_id, LVM_RESETEMPTYTEXT, 0, 0);
			}

			_app_notifyrefresh (config.hnotification, false);
			_app_refreshstatus (hwnd);

			break;
		}

		case RM_UNINITIALIZE:
		{
			app.TrayDestroy (hwnd, UID, nullptr);
			break;
		}

		case RM_UPDATE_DONE:
		{
			_app_profile_save (hwnd);
			_app_profile_load (hwnd);

			_app_changefilters (hwnd, true, false);

			break;
		}

		case RM_RESET_DONE:
		{
			for (auto &p : rules_config)
				SAFE_DELETE (p.second);

			rules_config.clear ();

			_r_fs_delete (config.rules_config_path, false);
			_r_fs_delete (config.rules_config_path_backup, false);

			_app_profile_save (hwnd);
			_app_profile_load (hwnd);

			_app_changefilters (hwnd, true, false);

			break;
		}

		case WM_DROPFILES:
		{
			const UINT numfiles = DragQueryFile ((HDROP)wparam, UINT32_MAX, nullptr, 0);
			size_t item = 0;

			_r_fastlock_acquireexclusive (&lock_access);

			for (UINT i = 0; i < numfiles; i++)
			{
				const UINT length = DragQueryFile ((HDROP)wparam, i, nullptr, 0) + 1;

				LPWSTR file = new WCHAR[length];

				if (file)
				{
					DragQueryFile ((HDROP)wparam, i, file, length);

					item = _app_addapplication (hwnd, file, 0, 0, 0, false, false, false);

					SAFE_DELETE_ARRAY (file);
				}
			}

			_r_fastlock_releaseexclusive (&lock_access);

			DragFinish ((HDROP)wparam);

			const bool is_visible = IsWindowVisible (GetDlgItem (hwnd, IDC_APPS_PROFILE));

			if (is_visible)
				_app_listviewsort (hwnd, IDC_APPS_PROFILE, -1, false);

			_app_profile_save (hwnd);

			if (is_visible)
				_app_showitem (hwnd, IDC_APPS_PROFILE, _app_getposition (hwnd, item), -1);

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
			if (config.hnotification)
				DestroyWindow (config.hnotification);

			UnregisterClass (NOTIFY_CLASS_DLG, app.GetHINSTANCE ());

			if (config.htimer)
				DeleteTimerQueue (config.htimer);

			app.TrayDestroy (hwnd, UID, nullptr);

			app.ConfigSet (L"Column1Width", (DWORD)_r_listview_getcolumnwidth (hwnd, IDC_APPS_PROFILE, 0));
			app.ConfigSet (L"Column2Width", (DWORD)_r_listview_getcolumnwidth (hwnd, IDC_APPS_PROFILE, 1));

			app.ConfigSet (L"Group1IsCollaped", ((SendDlgItemMessage (hwnd, IDC_APPS_PROFILE, LVM_GETGROUPSTATE, 0, LVGS_COLLAPSED) & LVGS_COLLAPSED) != 0) ? true : false);
			app.ConfigSet (L"Group2IsCollaped", ((SendDlgItemMessage (hwnd, IDC_APPS_PROFILE, LVM_GETGROUPSTATE, 1, LVGS_COLLAPSED) & LVGS_COLLAPSED) != 0) ? true : false);
			app.ConfigSet (L"Group3IsCollaped", ((SendDlgItemMessage (hwnd, IDC_APPS_PROFILE, LVM_GETGROUPSTATE, 2, LVGS_COLLAPSED) & LVGS_COLLAPSED) != 0) ? true : false);

			_app_profile_save (hwnd);

			if (config.done_evt)
			{
				if (_wfp_isfiltersapplying ())
					WaitForSingleObjectEx (config.done_evt, FILTERS_TIMEOUT, FALSE);

				CloseHandle (config.done_evt);
			}

			if (_r_sys_validversion (6, 1))
				_app_freelogstack ();

			_wfp_uninitialize (false);

			ImageList_Destroy (config.himg);
			BufferedPaintUnInit ();

			PostQuitMessage (0);

			break;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps = {0};
			const HDC hdc = BeginPaint (hwnd, &ps);

			RECT rc = {0};
			GetWindowRect (GetDlgItem (hwnd, IDC_TAB), &rc);

			for (INT i = 0; i < _R_RECT_WIDTH (&rc); i++)
				SetPixel (hdc, i, _R_RECT_HEIGHT (&rc), GetSysColor (COLOR_APPWORKSPACE));

			EndPaint (hwnd, &ps);

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
				case TCN_SELCHANGING:
				{
					_r_wnd_resize (nullptr, GetDlgItem (hwnd, _app_gettab_id (hwnd)), nullptr, 0, 0, 0, 0, SWP_NOREDRAW | SWP_NOMOVE | SWP_HIDEWINDOW);
					break;
				}

				case TCN_SELCHANGE:
				{
					app.ConfigSet (L"CurrentTab", (DWORD)SendDlgItemMessage (hwnd, IDC_TAB, TCM_GETCURSEL, 0, 0));

					const UINT listview_id = _app_gettab_id (hwnd);

					{
						RECT tab_rc1 = {0};
						RECT tab_rc2 = {0};

						GetWindowRect (GetDlgItem (hwnd, IDC_TAB), &tab_rc1);
						MapWindowPoints (nullptr, hwnd, (LPPOINT)& tab_rc1, 2);

						GetClientRect (GetDlgItem (hwnd, IDC_TAB), &tab_rc2);

						tab_rc2.left += tab_rc1.left;
						tab_rc2.top += tab_rc1.top;

						tab_rc2.right += tab_rc1.left;
						tab_rc2.bottom += tab_rc1.top;

						TabCtrl_AdjustRect (GetDlgItem (hwnd, IDC_TAB), 0, &tab_rc2);

						_r_wnd_resize (nullptr, GetDlgItem (hwnd, listview_id), nullptr, tab_rc2.left, tab_rc2.top, _R_RECT_WIDTH (&tab_rc2), _R_RECT_HEIGHT (&tab_rc2), SWP_NOREDRAW | SWP_SHOWWINDOW);

						SetFocus (GetDlgItem (hwnd, listview_id));
					}

					_app_listviewsetview (hwnd, listview_id);
					_app_listviewresize (hwnd, listview_id);
					_app_listviewsort (hwnd, listview_id, -1, false);

					_app_refreshstatus (hwnd);

					_r_listview_redraw (hwnd, listview_id);

					break;
				}

				case NM_CUSTOMDRAW:
				{
					_r_fastlock_acquireshared (&lock_access);
					const LONG result = _app_nmcustdraw ((LPNMLVCUSTOMDRAW)lparam);
					_r_fastlock_releaseshared (&lock_access);

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, result);
					return result;
				}

				case LVN_COLUMNCLICK:
				{
					LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lparam;

					if (
						nmlp->idFrom == IDC_APPS_PROFILE ||
						nmlp->idFrom == IDC_APPS_SERVICE ||
						nmlp->idFrom == IDC_APPS_PACKAGE ||
						nmlp->idFrom == IDC_RULES_BLOCKLIST ||
						nmlp->idFrom == IDC_RULES_SYSTEM ||
						nmlp->idFrom == IDC_RULES_CUSTOM
						)
					{
						_app_listviewsort (hwnd, (UINT)pnmv->hdr.idFrom, pnmv->iSubItem, true);
					}

					break;
				}

				case LVN_GETINFOTIP:
				{
					LPNMLVGETINFOTIP lpnmlv = (LPNMLVGETINFOTIP)lparam;

					const size_t idx = (size_t)_r_listview_getitemlparam (hwnd, (UINT)lpnmlv->hdr.idFrom, lpnmlv->iItem);

					if (idx)
						StringCchCopy (lpnmlv->pszText, lpnmlv->cchTextMax, _app_gettooltip ((UINT)lpnmlv->hdr.idFrom, idx));

					break;
				}

				case LVN_ITEMCHANGED:
				{
					if (_r_fastlock_islocked (&lock_checkbox))
						break;

					LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lparam;

					if (lpnmlv->uNewState == 8192 || lpnmlv->uNewState == 4096)
					{
						const UINT listview_id = _app_gettab_id (hwnd);
						const bool new_val = (lpnmlv->uNewState == 8192) ? true : false;

						bool is_changed = false;

						if (
							listview_id == IDC_APPS_PROFILE ||
							listview_id == IDC_APPS_SERVICE ||
							listview_id == IDC_APPS_PACKAGE
							)
						{
							_r_fastlock_acquireexclusive (&lock_access);

							const size_t hash = lpnmlv->lParam;
							PITEM_APP ptr_app = _app_getapplication (hash);

							if (ptr_app)
							{
								if (ptr_app->is_enabled != new_val)
								{
									ptr_app->is_enabled = new_val;
									_r_listview_setitem (hwnd, listview_id, lpnmlv->iItem, 0, nullptr, LAST_VALUE, _app_getappgroup (hash, ptr_app));

									_r_fastlock_acquireexclusive (&lock_notification);
									_app_freenotify (hash, false);
									_r_fastlock_releaseexclusive (&lock_notification);

									if ((lpnmlv->uNewState == 4096) && _app_istimeractive (ptr_app))
									{
										MFILTER_APPS rules;
										rules.push_back (ptr_app);

										_app_timer_remove (hwnd, &rules);
									}

									MFILTER_APPS rules;
									rules.push_back (ptr_app);

									_wfp_create3filters (&rules, __LINE__);

									is_changed = true;
								}
							}

							_r_fastlock_releaseexclusive (&lock_access);
						}
						else if (
							listview_id == IDC_RULES_BLOCKLIST ||
							listview_id == IDC_RULES_SYSTEM ||
							listview_id == IDC_RULES_CUSTOM
							)
						{
							_r_fastlock_acquireexclusive (&lock_access);

							const size_t rule_idx = lpnmlv->lParam;
							PITEM_RULE ptr_rule = rules_arr.at (rule_idx);

							if (ptr_rule)
							{
								if (ptr_rule->is_enabled != new_val)
								{
									_app_ruleenable (ptr_rule, new_val);
									_r_listview_setitem (hwnd, listview_id, lpnmlv->iItem, 0, nullptr, LAST_VALUE, _app_getrulegroup (ptr_rule));

									MFILTER_RULES rules;
									rules.push_back (ptr_rule);

									_wfp_create4filters (&rules, __LINE__);

									is_changed = true;
								}
							}

							_r_fastlock_releaseexclusive (&lock_access);
						}

						if (is_changed)
						{
							_app_notifyrefresh (config.hnotification, false);

							_app_listviewsort (hwnd, listview_id, -1, false);
							_app_profile_save (hwnd);

							_app_refreshstatus (hwnd);

							_r_listview_redraw (hwnd, listview_id);
						}
					}

					break;
				}

				case LVN_GETEMPTYMARKUP:
				{
					NMLVEMPTYMARKUP *lpnmlv = (NMLVEMPTYMARKUP *)lparam;

					lpnmlv->dwFlags = EMF_CENTERED;
					StringCchCopy (lpnmlv->szMarkup, _countof (lpnmlv->szMarkup), app.LocaleString (IDS_STATUS_EMPTY, nullptr));

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
					return TRUE;
				}

				case NM_DBLCLK:
				{
					LPNMITEMACTIVATE lpnmlv = (LPNMITEMACTIVATE)lparam;
					UINT command_id = 0;

					if (lpnmlv->iItem == -1)
						break;

					if (lpnmlv->hdr.idFrom == IDC_STATUSBAR)
					{
						LPNMMOUSE nmouse = (LPNMMOUSE)lparam;

						if (nmouse->dwItemSpec == 0)
							command_id = IDM_SELECT_ALL;

						else if (nmouse->dwItemSpec == 1)
							command_id = IDM_PURGE_UNUSED;

						else if (nmouse->dwItemSpec == 2)
							command_id = IDM_PURGE_TIMERS;
					}
					else if (
						lpnmlv->hdr.idFrom == IDC_APPS_PROFILE ||
						lpnmlv->hdr.idFrom == IDC_APPS_SERVICE ||
						lpnmlv->hdr.idFrom == IDC_APPS_PACKAGE
						)
					{
						command_id = IDM_EXPLORE;
					}
					else if (
						lpnmlv->hdr.idFrom == IDC_RULES_CUSTOM
						)
					{
						command_id = IDM_PROPERTIES;
					}

					if (command_id)
						PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (command_id, 0), 0);

					break;
				}
			}

			break;
		}

		//case WM_MOUSEHWHEEL:
		//{
		//	RDBG (L"%d", GET_KEYSTATE_WPARAM (wparam));

		//	if ((GET_KEYSTATE_WPARAM (wparam) & MK_CONTROL) == 0)
		//		break;


		//	bool is_tableview = app.ConfigGet (L"IsTableView", true).AsBool ();
		//	DWORD icons_size = app.ConfigGet (L"IconSize", SHIL_SYSSMALL).AsInt ();

		//	const bool is_down = GET_WHEEL_DELTA_WPARAM (wparam) < 0;

		//	//SHIL_EXTRALARGE
		//	//SHIL_LARGE
		//	//SHIL_SYSSMALL

		//	if (is_down)
		//	{
		//		if (icons_size == SHIL_SYSSMALL)
		//		{
		//			is_tableview = !is_tableview;
		//			icons_size = SHIL_EXTRALARGE;
		//		}
		//		else if (icons_size == SHIL_LARGE)
		//		{
		//			icons_size = SHIL_SYSSMALL;
		//		}
		//		else if (icons_size == SHIL_EXTRALARGE)
		//		{
		//			icons_size = SHIL_LARGE;
		//		}
		//	}
		//	else
		//	{
		//		if (icons_size == SHIL_EXTRALARGE)
		//		{
		//			is_tableview = !is_tableview;
		//			icons_size = SHIL_SYSSMALL;
		//		}
		//		else if (icons_size == SHIL_LARGE)
		//		{
		//			icons_size = SHIL_SYSSMALL;
		//		}
		//		else if (icons_size == SHIL_SYSSMALL)
		//		{
		//			icons_size = SHIL_EXTRALARGE;
		//		}
		//	}

		//	app.ConfigSet (L"IsTableView", is_tableview);
		//	app.ConfigSet (L"IconSize", icons_size);

		//	_app_listviewsetimagelist (hwnd, _app_gettab_id (hwnd));

		//	SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
		//	return TRUE;
		//}

		case WM_SIZE:
		{
			_app_resizewindow (hwnd, LOWORD (lparam), HIWORD (lparam));
			_app_listviewresize (hwnd, _app_gettab_id (hwnd));

			_app_refreshstatus (hwnd);

			RedrawWindow (hwnd, nullptr, nullptr, RDW_NOFRAME | RDW_NOINTERNALPAINT | RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);

			break;
		}

		case WM_CONTEXTMENU:
		{
			const UINT ctrl_id = GetDlgCtrlID ((HWND)wparam);
			const UINT selected_count = (UINT)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETSELECTEDCOUNT, 0, 0);

			if (
				ctrl_id == IDC_APPS_PROFILE ||
				ctrl_id == IDC_APPS_SERVICE ||
				ctrl_id == IDC_APPS_PACKAGE
				)
			{
				const bool is_filtersinstalled = _wfp_isfiltersinstalled ();

				static const UINT usettings_id = 2;
				static const UINT utimer_id = 3;


				const HMENU hmenu = LoadMenu (nullptr, MAKEINTRESOURCE (IDM_LISTVIEW));
				const HMENU hsubmenu = GetSubMenu (hmenu, 0);
				const HMENU hsubmenu_settings = GetSubMenu (hsubmenu, usettings_id);
				const HMENU hsubmenu_timer = GetSubMenu (hsubmenu, utimer_id);

				// set icons
				{
					static HBITMAP hbmp_rules = nullptr;
					static HBITMAP hbmp_timer = nullptr;

					if (!hbmp_rules)
						hbmp_rules = _app_ico2bmp (app.GetSharedIcon (app.GetHINSTANCE (), IDI_RULES, GetSystemMetrics (SM_CXSMICON)));

					if (!hbmp_timer)
						hbmp_timer = _app_ico2bmp (app.GetSharedIcon (app.GetHINSTANCE (), IDI_TIMER, GetSystemMetrics (SM_CXSMICON)));

					MENUITEMINFO mii = {0};

					mii.cbSize = sizeof (mii);
					mii.fMask = MIIM_BITMAP;

					mii.hbmpItem = hbmp_rules;
					SetMenuItemInfo (hsubmenu, usettings_id, TRUE, &mii);

					mii.hbmpItem = hbmp_timer;
					SetMenuItemInfo (hsubmenu, utimer_id, TRUE, &mii);
				}

				// localize
				app.LocaleMenu (hsubmenu, IDS_ADD_FILE, IDM_ADD_FILE, false, L"...");
				app.LocaleMenu (hsubmenu, IDS_TRAY_RULES, usettings_id, true, nullptr);
				app.LocaleMenu (hsubmenu, IDS_DISABLENOTIFICATIONS, IDM_DISABLENOTIFICATIONS, false, nullptr);
				app.LocaleMenu (hsubmenu, IDS_TIMER, utimer_id, true, nullptr);
				app.LocaleMenu (hsubmenu, IDS_DISABLETIMER, IDM_DISABLETIMER, false, nullptr);
				app.LocaleMenu (hsubmenu, IDS_REFRESH, IDM_REFRESH2, false, L"\tF5");
				app.LocaleMenu (hsubmenu, IDS_EXPLORE, IDM_EXPLORE, false, L"\tCtrl+E");
				app.LocaleMenu (hsubmenu, IDS_COPY, IDM_COPY, false, L"\tCtrl+C");
				app.LocaleMenu (hsubmenu, IDS_DELETE, IDM_DELETE, false, L"\tDel");
				app.LocaleMenu (hsubmenu, IDS_CHECK, IDM_CHECK, false, nullptr);
				app.LocaleMenu (hsubmenu, IDS_UNCHECK, IDM_UNCHECK, false, nullptr);
				app.LocaleMenu (hsubmenu, IDS_PROPERTIES, IDM_PROPERTIES, false, L"\tEnter");

				if (!selected_count)
				{
					EnableMenuItem (hsubmenu, usettings_id, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);
					EnableMenuItem (hsubmenu, utimer_id, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);
					EnableMenuItem (hsubmenu, IDM_EXPLORE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					EnableMenuItem (hsubmenu, IDM_COPY, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					EnableMenuItem (hsubmenu, IDM_DELETE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					EnableMenuItem (hsubmenu, IDM_CHECK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					EnableMenuItem (hsubmenu, IDM_UNCHECK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					EnableMenuItem (hsubmenu, IDM_PROPERTIES, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
				}

				if (_wfp_isfiltersapplying ())
					EnableMenuItem (hsubmenu, IDM_REFRESH2, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

				// show configuration
				if (selected_count)
				{
					const size_t item = (size_t)SendDlgItemMessage (hwnd, ctrl_id, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED); // get first item
					const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, ctrl_id, item);

					_r_fastlock_acquireshared (&lock_access);

					PITEM_APP const ptr_app = _app_getapplication (hash);

					if (ptr_app)
					{
						CheckMenuItem (hsubmenu, IDM_DISABLENOTIFICATIONS, MF_BYCOMMAND | (ptr_app->is_silent ? MF_CHECKED : MF_UNCHECKED));

						_app_generate_rulesmenu (hsubmenu_settings, hash);
					}

					// show timers
					bool is_checked = false;
					const time_t current_time = _r_unixtime_now ();

					for (size_t i = 0; i < timers.size (); i++)
					{
						MENUITEMINFO mii = {0};

						WCHAR buffer[128] = {0};
						StringCchCopy (buffer, _countof (buffer), _r_fmt_interval (timers.at (i) + 1, 1));

						mii.cbSize = sizeof (mii);
						mii.fMask = MIIM_ID | MIIM_FTYPE | MIIM_STATE | MIIM_STRING;
						mii.fType = MFT_STRING;
						mii.dwTypeData = buffer;
						mii.fState = MF_ENABLED;
						mii.wID = IDX_TIMER + UINT (i);

						if (!is_filtersinstalled)
							mii.fState = MF_DISABLED | MF_GRAYED;

						InsertMenuItem (hsubmenu_timer, mii.wID, FALSE, &mii);

						if (!is_checked && ptr_app->timer > current_time && ptr_app->timer <= (current_time + timers.at (i)))
						{
							CheckMenuRadioItem (hsubmenu_timer, IDX_TIMER, IDX_TIMER + UINT (timers.size ()), mii.wID, MF_BYCOMMAND);
							is_checked = true;
						}
					}

					if (!is_checked)
						CheckMenuRadioItem (hsubmenu_timer, IDM_DISABLETIMER, IDM_DISABLETIMER, IDM_DISABLETIMER, MF_BYCOMMAND);

					_r_fastlock_releaseshared (&lock_access);
				}

				if (ctrl_id != IDC_APPS_PROFILE)
				{
					EnableMenuItem (hsubmenu, IDM_ADD_FILE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					EnableMenuItem (hsubmenu, IDM_DELETE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
				}

				POINT pt = {0};
				GetCursorPos (&pt);

				TrackPopupMenuEx (hsubmenu, TPM_RIGHTBUTTON | TPM_LEFTBUTTON, pt.x, pt.y, hwnd, nullptr);

				DestroyMenu (hmenu);
			}
			else if (
				ctrl_id == IDC_RULES_BLOCKLIST ||
				ctrl_id == IDC_RULES_SYSTEM ||
				ctrl_id == IDC_RULES_CUSTOM
				)
			{
				const HMENU hmenu = LoadMenu (nullptr, MAKEINTRESOURCE (IDM_RULES));
				const HMENU hsubmenu = GetSubMenu (hmenu, 0);

				// localize
				app.LocaleMenu (hsubmenu, IDS_CHECK, IDM_CHECK, false, nullptr);
				app.LocaleMenu (hsubmenu, IDS_UNCHECK, IDM_UNCHECK, false, nullptr);
				app.LocaleMenu (hsubmenu, IDS_REFRESH, IDM_REFRESH2, false, L"\tF5");
				app.LocaleMenu (hsubmenu, IDS_COPY, IDM_COPY, false, L"\tCtrl+C");

				if (!SendDlgItemMessage (hwnd, ctrl_id, LVM_GETSELECTEDCOUNT, 0, 0))
				{
					EnableMenuItem (hsubmenu, IDM_COPY, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					EnableMenuItem (hsubmenu, IDM_PROPERTIES, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					EnableMenuItem (hsubmenu, IDM_DELETE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					EnableMenuItem (hsubmenu, IDM_CHECK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					EnableMenuItem (hsubmenu, IDM_UNCHECK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
				}

				if (ctrl_id == IDC_RULES_CUSTOM)
				{
					app.LocaleMenu (hsubmenu, IDS_OPENRULESEDITOR, IDM_OPENRULESEDITOR, false, L"...");
					app.LocaleMenu (hsubmenu, IDS_DELETE, IDM_DELETE, false, L"\tDel");
					app.LocaleMenu (hsubmenu, IDS_PROPERTIES, IDM_PROPERTIES, false, L"\tEnter");
				}
				else
				{
					DeleteMenu (hsubmenu, IDM_OPENRULESEDITOR, MF_BYCOMMAND);
					DeleteMenu (hsubmenu, IDM_PROPERTIES, MF_BYCOMMAND);
					DeleteMenu (hsubmenu, IDM_DELETE, MF_BYCOMMAND);
					DeleteMenu (hsubmenu, 0, MF_BYPOSITION);
				}

				POINT pt = {0};
				GetCursorPos (&pt);

				TrackPopupMenuEx (hsubmenu, TPM_RIGHTBUTTON | TPM_LEFTBUTTON, pt.x, pt.y, hwnd, nullptr);

				DestroyMenu (hmenu);
			}

			break;
		}

		case WM_TRAYICON:
		{
			switch (LOWORD (lparam))
			{
				case NIN_POPUPOPEN:
				{
					if (_app_notifyshow (config.hnotification, _app_notifygetcurrent (config.hnotification), true, false))
						_app_notifysettimeout (config.hnotification, NOTIFY_TIMER_POPUP_ID, false, 0);

					break;
				}

				case NIN_POPUPCLOSE:
				{
					_app_notifysettimeout (config.hnotification, NOTIFY_TIMER_POPUP_ID, true, NOTIFY_TIMER_POPUP);
					break;
				}

				case WM_MBUTTONUP:
				{
					PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDM_LOGSHOW, 0), 0);
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

					constexpr auto mode_id = 3;
					constexpr auto add_id = 5;
					constexpr auto delete_id = 6;
					constexpr auto notifications_id = 8;
					constexpr auto errlog_id = 9;

					const bool is_filtersinstalled = _wfp_isfiltersinstalled ();

					const HMENU hmenu = LoadMenu (nullptr, MAKEINTRESOURCE (IDM_TRAY));
					const HMENU hsubmenu = GetSubMenu (hmenu, 0);

					{
						static HBITMAP henabled = nullptr;
						static HBITMAP hdisabled = nullptr;

						if (!henabled)
							henabled = _app_ico2bmp (app.GetSharedIcon (app.GetHINSTANCE (), IDI_ACTIVE, GetSystemMetrics (SM_CXSMICON)));

						if (!hdisabled)
							hdisabled = _app_ico2bmp (app.GetSharedIcon (app.GetHINSTANCE (), IDI_INACTIVE, GetSystemMetrics (SM_CXSMICON)));

						MENUITEMINFO mii = {0};

						mii.cbSize = sizeof (mii);
						mii.fMask = MIIM_BITMAP;
						mii.hbmpItem = is_filtersinstalled ? hdisabled : henabled;

						SetMenuItemInfo (hsubmenu, IDM_TRAY_START, FALSE, &mii);
					}

					// localize
					app.LocaleMenu (hsubmenu, IDS_TRAY_SHOW, IDM_TRAY_SHOW, false, nullptr);
					app.LocaleMenu (hsubmenu, is_filtersinstalled ? IDS_TRAY_STOP : IDS_TRAY_START, IDM_TRAY_START, false, L"...");
					app.LocaleMenu (hsubmenu, IDS_TRAY_MODE, mode_id, true, nullptr);
					app.LocaleMenu (hsubmenu, IDS_MODE_WHITELIST, IDM_TRAY_MODEWHITELIST, false, nullptr);
					app.LocaleMenu (hsubmenu, IDS_MODE_BLACKLIST, IDM_TRAY_MODEBLACKLIST, false, nullptr);

					app.LocaleMenu (hsubmenu, IDS_ADD, add_id, true, nullptr);

					{
						_r_fastlock_acquireshared (&lock_access);

						ITEM_STATUS itemStat = {0};
						_app_getcount (&itemStat);

						_r_fastlock_releaseshared (&lock_access);

						const size_t total_count = itemStat.unused_count + itemStat.timers_count;

						app.LocaleMenu (hsubmenu, IDS_DELETE, delete_id, true, total_count ? _r_fmt (L" (%d)", total_count).GetString () : nullptr);

						if (!total_count)
						{
							EnableMenuItem (hsubmenu, delete_id, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);
						}
						else
						{
							app.LocaleMenu (hsubmenu, IDS_PURGE_UNUSED, IDM_PURGE_UNUSED, false, itemStat.unused_count ? _r_fmt (L" (%d)", itemStat.unused_count).GetString () : nullptr);
							app.LocaleMenu (hsubmenu, IDS_PURGE_TIMERS, IDM_PURGE_TIMERS, false, itemStat.timers_count ? _r_fmt (L" (%d)", itemStat.timers_count).GetString () : nullptr);

							if (!itemStat.unused_count)
								EnableMenuItem (hsubmenu, IDM_PURGE_UNUSED, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

							if (!itemStat.timers_count)
								EnableMenuItem (hsubmenu, IDM_PURGE_TIMERS, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
						}
					}

					app.LocaleMenu (hsubmenu, IDS_TRAY_LOG, notifications_id, true, nullptr);
					app.LocaleMenu (hsubmenu, IDS_ENABLELOG_CHK, IDM_TRAY_ENABLELOG_CHK, false, nullptr);
					app.LocaleMenu (hsubmenu, IDS_ENABLENOTIFICATIONS_CHK, IDM_TRAY_ENABLENOTIFICATIONS_CHK, false, nullptr);
					app.LocaleMenu (hsubmenu, IDS_LOGSHOW, IDM_TRAY_LOGSHOW, false, nullptr);
					app.LocaleMenu (hsubmenu, IDS_LOGCLEAR, IDM_TRAY_LOGCLEAR, false, nullptr);

					app.LocaleMenu (hsubmenu, IDS_TRAY_LOGERR, errlog_id, true, nullptr);

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
						app.LocaleMenu (hsubmenu, IDS_LOGSHOW, IDM_TRAY_LOGSHOW_ERR, false, nullptr);
						app.LocaleMenu (hsubmenu, IDS_LOGCLEAR, IDM_TRAY_LOGCLEAR_ERR, false, nullptr);
					}
					else
					{
						EnableMenuItem (hsubmenu, errlog_id, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);
					}

					app.LocaleMenu (hsubmenu, IDS_SETTINGS, IDM_TRAY_SETTINGS, false, L"...");
					app.LocaleMenu (hsubmenu, IDS_WEBSITE, IDM_TRAY_WEBSITE, false, nullptr);
					app.LocaleMenu (hsubmenu, IDS_ABOUT, IDM_TRAY_ABOUT, false, nullptr);
					app.LocaleMenu (hsubmenu, IDS_EXIT, IDM_TRAY_EXIT, false, nullptr);

					CheckMenuItem (hsubmenu, IDM_TRAY_ENABLELOG_CHK, MF_BYCOMMAND | (app.ConfigGet (L"IsLogEnabled", false).AsBool () ? MF_CHECKED : MF_UNCHECKED));
					CheckMenuItem (hsubmenu, IDM_TRAY_ENABLENOTIFICATIONS_CHK, MF_BYCOMMAND | (app.ConfigGet (L"IsNotificationsEnabled", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));

					if (_wfp_isfiltersapplying ())
						EnableMenuItem (hsubmenu, IDM_TRAY_START, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

					// dropped packets logging (win7+)
					if (!_r_sys_validversion (6, 1))
					{
						EnableMenuItem (hsubmenu, IDM_TRAY_ENABLELOG_CHK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
						EnableMenuItem (hsubmenu, IDM_TRAY_ENABLENOTIFICATIONS_CHK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					}

					CheckMenuRadioItem (hsubmenu, IDM_TRAY_MODEWHITELIST, IDM_TRAY_MODEBLACKLIST, IDM_TRAY_MODEWHITELIST + app.ConfigGet (L"Mode", ModeWhitelist).AsUint (), MF_BYCOMMAND);

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
			switch (wparam)
			{
				case PBT_APMSUSPEND:
				{
					if (!_wfp_isfiltersapplying ())
					{
						_app_profile_save (hwnd);
						_wfp_uninitialize (false);
					}

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
					return TRUE;
				}

				case PBT_APMRESUMECRITICAL:
				case PBT_APMRESUMESUSPEND:
				{
					app.ConfigInit ();

					_app_profile_load (hwnd);

					if (_wfp_isfiltersinstalled ())
					{
						if (!_wfp_isfiltersapplying () && _wfp_initialize (true))
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
			if (!app.ConfigGet (L"IsRefreshDevices", true).AsBool ())
			{
				SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
				return TRUE;
			}

			switch (wparam)
			{
				case DBT_DEVICEARRIVAL:
				case DBT_DEVICEREMOVECOMPLETE:
				{
					const PDEV_BROADCAST_HDR lbhdr = (PDEV_BROADCAST_HDR)lparam;

					if (lbhdr && lbhdr->dbch_devicetype == DBT_DEVTYP_VOLUME)
					{
						if (wparam == DBT_DEVICEARRIVAL)
						{
							if (_wfp_isfiltersinstalled () && !_wfp_isfiltersapplying ())
							{
								_app_profile_save (hwnd);
								_app_profile_load (hwnd);

								_app_changefilters (hwnd, true, false);
							}
						}
						else if (wparam == DBT_DEVICEREMOVECOMPLETE)
						{
							if (IsWindowVisible (hwnd))
								_r_listview_redraw (hwnd, _app_gettab_id (hwnd));
						}
					}

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
					return TRUE;
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
				size_t item = LAST_VALUE;
				BOOL is_remove = (BOOL)-1;

				_r_fastlock_acquireexclusive (&lock_access);

				const UINT listview_id = _app_gettab_id (hwnd);

				const size_t rule_idx = (LOWORD (wparam) - IDX_RULES_SPECIAL);
				PITEM_RULE ptr_rule = rules_arr.at (rule_idx);

				if (ptr_rule)
				{
					while ((item = (size_t)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, item, LVNI_SELECTED)) != LAST_VALUE)
					{
						const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, listview_id, item);

						if (ptr_rule->is_forservices && (hash == config.ntoskrnl_hash || hash == config.svchost_hash))
							continue;

						PITEM_APP ptr_app = _app_getapplication (hash);

						if (ptr_app)
						{
							if (is_remove == (BOOL)-1)
								is_remove = ptr_rule->is_enabled && (ptr_rule->apps.find (hash) != ptr_rule->apps.end ());

							if (is_remove)
							{
								ptr_rule->apps.erase (hash);

								if (ptr_rule->apps.empty ())
									_app_ruleenable (ptr_rule, false);
							}
							else
							{
								ptr_rule->apps[hash] = true;

								_app_ruleenable (ptr_rule, true);
							}

							_r_fastlock_acquireexclusive (&lock_notification);
							_app_freenotify (hash, false);
							_r_fastlock_releaseexclusive (&lock_notification);
						}
					}

					MFILTER_RULES rules;
					rules.push_back (ptr_rule);

					_wfp_create4filters (&rules, __LINE__);
				}

				_r_fastlock_releaseexclusive (&lock_access);

				_app_notifyrefresh (config.hnotification, false);

				_app_listviewsort (hwnd, listview_id, -1, false);
				_app_profile_save (hwnd);

				_r_listview_redraw (hwnd, listview_id);

				return FALSE;
			}
			else if ((LOWORD (wparam) >= IDX_TIMER && LOWORD (wparam) <= IDX_TIMER + timers.size ()))
			{
				const UINT listview_id = _app_gettab_id (hwnd);

				if (!SendDlgItemMessage (hwnd, listview_id, LVM_GETSELECTEDCOUNT, 0, 0))
					break;

				const size_t idx = (LOWORD (wparam) - IDX_TIMER);

				size_t item = LAST_VALUE;

				_r_fastlock_acquireexclusive (&lock_access);

				MFILTER_APPS rules;

				while ((item = (size_t)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, item, LVNI_SELECTED)) != LAST_VALUE)
				{
					const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, listview_id, item);
					const PITEM_APP ptr_app = _app_getapplication (hash);

					rules.push_back (ptr_app);
				}

				_app_timer_create (hwnd, &rules, timers.at (idx));

				_r_fastlock_releaseexclusive (&lock_access);

				_app_listviewsort (hwnd, listview_id, -1, false);
				_app_profile_save (hwnd);

				_r_listview_redraw (hwnd, listview_id);

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
				case IDC_SETTINGS_BTN:
				{
					app.CreateSettingsWindow (&SettingsProc);
					break;
				}

				case IDM_EXIT:
				case IDM_TRAY_EXIT:
				case IDC_EXIT_BTN:
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

				case IDM_ABOUT:
				case IDM_TRAY_ABOUT:
				{
					app.CreateAboutWindow (hwnd);
					break;
				}

				case IDM_EXPORT_APPS:
				case IDM_EXPORT_RULES:
				{
					WCHAR path[MAX_PATH] = {0};
					StringCchCopy (path, _countof (path), ((LOWORD (wparam) == IDM_EXPORT_APPS) ? XML_APPS : XML_RULES_CUSTOM));

					WCHAR title[MAX_PATH] = {0};
					StringCchPrintf (title, _countof (title), L"%s %s...", app.LocaleString (IDS_EXPORT, nullptr).GetString (), path);

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
						if (LOWORD (wparam) == IDM_EXPORT_APPS)
							_r_fs_copy (config.apps_path, path);

						else if (LOWORD (wparam) == IDM_EXPORT_RULES)
							_r_fs_copy (config.rules_custom_path, path);
					}

					break;
				}

				case IDM_IMPORT_APPS:
				case IDM_IMPORT_RULES:
				{
					WCHAR path[MAX_PATH] = {0};
					StringCchCopy (path, _countof (path), ((LOWORD (wparam) == IDM_IMPORT_APPS) ? XML_APPS : XML_RULES_CUSTOM));

					WCHAR title[MAX_PATH] = {0};
					StringCchPrintf (title, _countof (title), L"%s %s...", app.LocaleString (IDS_IMPORT, nullptr).GetString (), path);

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
						// make backup
						if (LOWORD (wparam) == IDM_IMPORT_APPS)
							_r_fs_copy (config.apps_path, config.apps_path_backup);

						else if (LOWORD (wparam) == IDM_IMPORT_RULES)
							_r_fs_copy (config.rules_custom_path, config.rules_custom_path_backup);

						_app_profile_load (hwnd, ((LOWORD (wparam) == IDM_IMPORT_APPS) ? path : nullptr), ((LOWORD (wparam) == IDM_IMPORT_RULES) ? path : nullptr));

						_app_listviewsort (hwnd, _app_gettab_id (hwnd), -1, false);
						_app_profile_save (hwnd);

						_app_notifyrefresh (config.hnotification, false);

						_app_changefilters (hwnd, true, false);
					}

					break;
				}

				case IDM_ALWAYSONTOP_CHK:
				{
					const bool new_val = !app.ConfigGet (L"AlwaysOnTop", false).AsBool ();

					CheckMenuItem (GetMenu (hwnd), IDM_ALWAYSONTOP_CHK, MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));
					app.ConfigSet (L"AlwaysOnTop", new_val);

					_r_wnd_top (hwnd, new_val);

					break;
				}

				case IDM_SHOWFILENAMESONLY_CHK:
				{
					const bool new_val = !app.ConfigGet (L"ShowFilenames", true).AsBool ();

					CheckMenuItem (GetMenu (hwnd), IDM_SHOWFILENAMESONLY_CHK, MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));
					app.ConfigSet (L"ShowFilenames", new_val);

					const UINT listview_id = _app_gettab_id (hwnd);

					_r_fastlock_acquireexclusive (&lock_access);

					for (size_t i = 0; i < _r_listview_getitemcount (hwnd, listview_id); i++)
					{
						const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, listview_id, i);
						PITEM_APP ptr_app = _app_getapplication (hash);

						if (ptr_app)
						{
							_app_getdisplayname (hash, ptr_app, &ptr_app->display_name);

							_r_listview_setitem (hwnd, listview_id, i, 0, ptr_app->display_name);
						}
					}

					_r_fastlock_releaseexclusive (&lock_access);

					_app_notifyrefresh (config.hnotification, false);
					_app_listviewsort (hwnd, listview_id, -1, false);

					_r_listview_redraw (hwnd, listview_id);

					break;
				}

				case IDM_AUTOSIZECOLUMNS_CHK:
				{
					const bool new_val = !app.ConfigGet (L"AutoSizeColumns", true).AsBool ();

					CheckMenuItem (GetMenu (hwnd), IDM_AUTOSIZECOLUMNS_CHK, MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));
					app.ConfigSet (L"AutoSizeColumns", new_val);

					_app_listviewresize (hwnd, _app_gettab_id (hwnd));

					break;
				}

				case IDM_ENABLESPECIALGROUP_CHK:
				{
					const bool new_val = !app.ConfigGet (L"IsEnableSpecialGroup", true).AsBool ();

					CheckMenuItem (GetMenu (hwnd), IDM_ENABLESPECIALGROUP_CHK, MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));
					app.ConfigSet (L"IsEnableSpecialGroup", new_val);

					_app_listviewsort (hwnd, _app_gettab_id (hwnd), -1, false);

					break;
				}

				case IDM_ICONSSMALL:
				case IDM_ICONSLARGE:
				case IDM_ICONSEXTRALARGE:
				{
					const UINT listview_id = _app_gettab_id (hwnd);

					DWORD icon_size;

					if ((LOWORD (wparam) == IDM_ICONSLARGE))
						icon_size = SHIL_LARGE;

					else if ((LOWORD (wparam) == IDM_ICONSEXTRALARGE))
						icon_size = SHIL_EXTRALARGE;

					else
						icon_size = SHIL_SYSSMALL;

					app.ConfigSet (L"IconSize", icon_size);

					_app_listviewsetview (hwnd, listview_id);

					_r_listview_redraw (hwnd, listview_id);

					break;
				}

				case IDM_ICONSISTABLEVIEW:
				{
					const UINT listview_id = _app_gettab_id (hwnd);

					app.ConfigSet (L"IsTableView", !app.ConfigGet (L"IsTableView", true).AsBool ());

					_app_listviewsetview (hwnd, listview_id);

					_r_listview_redraw (hwnd, listview_id);

					break;
				}

				case IDM_ICONSISHIDDEN:
				{
					const UINT listview_id = _app_gettab_id (hwnd);

					const bool new_val = !app.ConfigGet (L"IsIconsHidden", false).AsBool ();

					app.ConfigSet (L"IsIconsHidden", new_val);

					CheckMenuItem (GetMenu (hwnd), IDM_ICONSISHIDDEN, MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));

					_r_fastlock_acquireexclusive (&lock_access);

					for (size_t i = 0; i < _r_listview_getitemcount (hwnd, listview_id); i++)
					{
						const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, listview_id, i);
						PITEM_APP ptr_app = _app_getapplication (hash);

						if (ptr_app)
						{
							size_t icon_id;
							_app_getappicon (ptr_app, false, &icon_id, nullptr);

							_r_listview_setitem (hwnd, listview_id, i, 0, nullptr, icon_id);
						}
					}

					_r_fastlock_releaseexclusive (&lock_access);

					_r_listview_redraw (hwnd, listview_id);

					break;
				}

				case IDM_FONT:
				{
					CHOOSEFONT cf = {0};

					LOGFONT lf = {0};

					cf.lStructSize = sizeof (cf);
					cf.hwndOwner = hwnd;
					cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_NOSCRIPTSEL | CF_LIMITSIZE | CF_NOVERTFONTS;
					cf.nSizeMax = 14;
					cf.nSizeMin = 8;
					cf.lpLogFont = &lf;

					_app_listviewinitfont (&lf);

					if (ChooseFont (&cf))
					{
						app.ConfigSet (L"Font", lf.lfFaceName[0] ? _r_fmt (L"%s;%d;%d", lf.lfFaceName, _r_dc_fontheighttosize (lf.lfHeight), lf.lfWeight) : UI_FONT_DEFAULT);

						for (size_t i = 0; i < (size_t)SendDlgItemMessage (hwnd, IDC_TAB, TCM_GETITEMCOUNT, 0, 0); i++)
						{
							const UINT listview_id = _app_gettab_id (hwnd, i);

							_app_listviewsetfont (hwnd, _app_gettab_id (hwnd), false);
						}
					}

					break;
				}

				case IDM_TRAY_MODEWHITELIST:
				case IDM_TRAY_MODEBLACKLIST:
				{
					DWORD current_mode = ModeWhitelist;

					if (LOWORD (wparam) == IDM_TRAY_MODEBLACKLIST)
						current_mode = ModeBlacklist;

					if ((app.ConfigGet (L"Mode", ModeWhitelist).AsUint () == current_mode) || (_wfp_isfiltersinstalled () && _r_msg (hwnd, MB_YESNO | MB_ICONEXCLAMATION | MB_TOPMOST, APP_NAME, nullptr, app.LocaleString (IDS_QUESTION_MODE, nullptr), app.LocaleString ((current_mode == ModeWhitelist) ? IDS_MODE_WHITELIST : IDS_MODE_BLACKLIST, nullptr).GetString ()) != IDYES))
						break;

					app.ConfigSet (L"Mode", current_mode);

					CheckMenuRadioItem (GetMenu (hwnd), IDM_TRAY_MODEWHITELIST, IDM_TRAY_MODEBLACKLIST, IDM_TRAY_MODEWHITELIST + current_mode, MF_BYCOMMAND);

					_app_changefilters (hwnd, true, false);
					_app_refreshstatus (hwnd);

					break;
				}

				case IDM_REFRESH:
				case IDM_REFRESH2:
				{
					if (!_wfp_isfiltersapplying ())
					{
						app.ConfigInit ();

						_app_profile_load (hwnd);
						_app_profile_save (hwnd);

						_app_changefilters (hwnd, true, false);
					}

					break;
				}

				case IDM_ENABLELOG_CHK:
				case IDM_TRAY_ENABLELOG_CHK:
				{
					const bool new_val = !app.ConfigGet (L"IsLogEnabled", false).AsBool ();

					CheckMenuItem (GetMenu (hwnd), IDM_ENABLELOG_CHK, MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));
					app.ConfigSet (L"IsLogEnabled", new_val);

					_app_loginit (new_val);

					break;
				}

				case IDM_ENABLENOTIFICATIONS_CHK:
				case IDM_TRAY_ENABLENOTIFICATIONS_CHK:
				{
					const bool new_val = !app.ConfigGet (L"IsNotificationsEnabled", true).AsBool ();

					CheckMenuItem (GetMenu (hwnd), IDM_ENABLENOTIFICATIONS_CHK, MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));
					app.ConfigSet (L"IsNotificationsEnabled", new_val);

					_app_notifyrefresh (config.hnotification, false);

					break;
				}

				case IDM_LOGSHOW:
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

				case IDM_LOGCLEAR:
				case IDM_TRAY_LOGCLEAR:
				{
					const rstring path = _r_path_expand (app.ConfigGet (L"LogPath", LOG_PATH_DEFAULT));

					if ((config.hlogfile != nullptr && config.hlogfile != INVALID_HANDLE_VALUE) || _r_fs_exists (path) || InterlockedCompareExchange (&log_stack.item_count, 0, 0))
					{
						if (!app.ConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION, nullptr), L"ConfirmLogClear"))
							break;

						_app_freelogstack ();
						_app_logclear ();
					}

					break;
				}

				case IDM_TRAY_LOGSHOW_ERR:
				{
					static const rstring path = _r_dbg_getpath ();

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
				case IDC_START_BTN:
				{
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
						_r_fastlock_acquireexclusive (&lock_access);

						size_t hash = 0;

						if (files[ofn.nFileOffset - 1] != 0)
						{
							hash = _app_addapplication (hwnd, files, 0, 0, 0, false, false, false);
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
									hash = _app_addapplication (hwnd, _r_fmt (L"%s\\%s", dir, p), 0, 0, 0, false, false, false);
							}
						}

						_r_fastlock_releaseexclusive (&lock_access);

						_app_listviewsort (hwnd, IDC_APPS_PROFILE, -1, false);
						_app_profile_save (hwnd);

						_app_showitem (hwnd, IDC_APPS_PROFILE, _app_getposition (hwnd, hash), -1);
					}

					break;
				}

				case IDM_DISABLENOTIFICATIONS:
				case IDM_DISABLETIMER:
				case IDM_EXPLORE:
				{
					const UINT listview_id = _app_gettab_id (hwnd);
					const UINT ctrl_id = LOWORD (wparam);

					if (
						listview_id != IDC_APPS_PROFILE &&
						listview_id != IDC_APPS_SERVICE &&
						listview_id != IDC_APPS_PACKAGE
						)
					{
						// note: these commands only for profile...
						break;
					}

					MFILTER_APPS rules;

					size_t item = LAST_VALUE;
					BOOL new_val = BOOL (-1);

					_r_fastlock_acquireexclusive (&lock_access);

					while ((item = (size_t)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, item, LVNI_SELECTED)) != LAST_VALUE)
					{
						const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, listview_id, item);
						PITEM_APP ptr_app = _app_getapplication (hash);

						if (!ptr_app)
							continue;

						if (ctrl_id == IDM_EXPLORE)
						{
							if (ptr_app->type != AppPico && ptr_app->type != AppDevice && (ptr_app->real_path && ptr_app->real_path[0]))
							{
								if (_r_fs_exists (ptr_app->real_path))
									_r_run (nullptr, _r_fmt (L"\"explorer.exe\" /select,\"%s\"", ptr_app->real_path));

								else if (_r_fs_exists (_r_path_extractdir (ptr_app->real_path)))
									ShellExecute (hwnd, nullptr, _r_path_extractdir (ptr_app->real_path), nullptr, nullptr, SW_SHOWDEFAULT);
							}
						}
						else if (ctrl_id == IDM_DISABLENOTIFICATIONS)
						{
							if (new_val == BOOL (-1))
								new_val = !ptr_app->is_silent;

							if ((BOOL)ptr_app->is_silent != new_val)
							{
								ptr_app->is_silent = new_val;

								_r_fastlock_acquireexclusive (&lock_notification);
								_app_freenotify (hash, false);
								_r_fastlock_releaseexclusive (&lock_notification);
							}
						}
						else if (ctrl_id == IDM_DISABLETIMER)
						{
							rules.push_back (ptr_app);
						}
					}

					if (ctrl_id == IDM_DISABLETIMER)
						_app_timer_remove (hwnd, &rules);

					_r_fastlock_releaseexclusive (&lock_access);

					if (
						ctrl_id == IDM_DISABLETIMER ||
						ctrl_id == IDM_DISABLENOTIFICATIONS
						)
					{
						_app_notifyrefresh (config.hnotification, false);
						_app_refreshstatus (hwnd);

						_app_listviewsort (hwnd, listview_id, -1, false);
						_app_profile_save (hwnd);

						_r_listview_redraw (hwnd, listview_id);
					}

					break;
				}

				case IDM_COPY:
				{
					const UINT listview_id = _app_gettab_id (hwnd);
					size_t item = LAST_VALUE;

					const size_t column_count = _r_listview_getcolumncount (hwnd, listview_id);

					rstring buffer;

					while ((item = (size_t)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, item, LVNI_SELECTED)) != LAST_VALUE)
					{
						for (size_t column_id = 0; column_id < column_count; column_id++)
						{
							buffer.Append (_r_listview_getitemtext (hwnd, listview_id, item, column_id)).Append (L" ");

						}

						buffer.Trim (L" ").Append (L"\r\n");
					}

					buffer.Trim (L"\r\n");

					_r_clipboard_set (hwnd, buffer, buffer.GetLength ());

					break;
				}

				case IDM_CHECK:
				case IDM_UNCHECK:
				{
					const UINT listview_id = _app_gettab_id (hwnd);
					const UINT ctrl_id = LOWORD (wparam);

					const bool new_val = (LOWORD (wparam) == IDM_CHECK) ? true : false;

					bool is_changed = false;

					MFILTER_APPS timer_apps;

					if (
						listview_id == IDC_APPS_PROFILE ||
						listview_id == IDC_APPS_SERVICE ||
						listview_id == IDC_APPS_PACKAGE
						)
					{
						size_t item = LAST_VALUE;

						_r_fastlock_acquireexclusive (&lock_access);

						MFILTER_APPS rules;

						while ((item = (size_t)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, item, LVNI_SELECTED)) != LAST_VALUE)
						{
							const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, listview_id, item);
							PITEM_APP ptr_app = _app_getapplication (hash);

							if (ptr_app)
							{
								if (ptr_app->is_enabled != new_val)
								{
									ptr_app->is_enabled = new_val;

									_r_fastlock_acquireshared (&lock_checkbox);

									_r_listview_setitem (hwnd, listview_id, item, 0, nullptr, LAST_VALUE, _app_getappgroup (hash, ptr_app));
									_r_listview_setitemcheck (hwnd, listview_id, item, new_val);

									_r_fastlock_releaseshared (&lock_checkbox);

									rules.push_back (ptr_app);

									if (ctrl_id == IDM_UNCHECK)
										timer_apps.push_back (ptr_app);

									_r_fastlock_acquireexclusive (&lock_notification);
									_app_freenotify (hash, false);
									_r_fastlock_releaseexclusive (&lock_notification);

									if (!is_changed)
										is_changed = true;
								}
							}
						}

						if (is_changed)
							_wfp_create3filters (&rules, __LINE__);

						_r_fastlock_releaseexclusive (&lock_access);
					}
					else if (
						listview_id == IDC_RULES_BLOCKLIST ||
						listview_id == IDC_RULES_SYSTEM ||
						listview_id == IDC_RULES_CUSTOM
						)
					{
						size_t item = LAST_VALUE;

						_r_fastlock_acquireexclusive (&lock_access);

						MFILTER_RULES rules;

						while ((item = (size_t)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, item, LVNI_SELECTED)) != LAST_VALUE)
						{
							const size_t rule_idx = (size_t)_r_listview_getitemlparam (hwnd, listview_id, item);
							PITEM_RULE ptr_rule = rules_arr.at (rule_idx);

							if (ptr_rule)
							{
								if (ptr_rule->is_enabled != new_val)
								{
									_app_ruleenable (ptr_rule, new_val);

									_r_fastlock_acquireshared (&lock_checkbox);

									_r_listview_setitem (hwnd, listview_id, item, 0, nullptr, _app_getruleicon (ptr_rule), _app_getrulegroup (ptr_rule));
									_r_listview_setitemcheck (hwnd, listview_id, item, new_val);

									_r_fastlock_releaseshared (&lock_checkbox);

									rules.push_back (ptr_rule);

									if (!is_changed)
										is_changed = true;
								}
							}
						}

						if (is_changed)
							_wfp_create4filters (&rules, __LINE__);

						_r_fastlock_releaseexclusive (&lock_access);
					}

					if (is_changed)
					{
						_app_timer_remove (hwnd, &timer_apps);

						_app_notifyrefresh (config.hnotification, false);

						_app_listviewsort (hwnd, listview_id, -1, false);
						_app_profile_save (hwnd);

						_app_refreshstatus (hwnd);

						_r_listview_redraw (hwnd, listview_id);
					}

					break;
				}

				case IDM_EDITRULES:
				{
					_app_settab_id (hwnd, IDC_RULES_CUSTOM);
					break;
				}

				case IDM_OPENRULESEDITOR:
				{
					PITEM_RULE ptr_rule = new ITEM_RULE;

					if (ptr_rule)
					{
						_app_ruleenable (ptr_rule, true);

						ptr_rule->type = TypeCustom;
						ptr_rule->is_block = ((app.ConfigGet (L"Mode", ModeWhitelist).AsUint () == ModeWhitelist) ? false : true);

						const UINT listview_id = _app_gettab_id (hwnd);

						if (
							listview_id == IDC_APPS_PROFILE ||
							listview_id == IDC_APPS_SERVICE ||
							listview_id == IDC_APPS_PACKAGE
							)
						{
							size_t item = LAST_VALUE;

							while ((item = (size_t)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, item, LVNI_SELECTED)) != LAST_VALUE)
							{
								const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, listview_id, item);

								if (hash)
									ptr_rule->apps[hash] = true;
							}
						}

						if (DialogBoxParam (nullptr, MAKEINTRESOURCE (IDD_EDITOR), hwnd, &EditorProc, (LPARAM)ptr_rule))
						{
							_r_fastlock_acquireexclusive (&lock_access);

							rules_arr.push_back (ptr_rule);

							MFILTER_RULES rules;
							rules.push_back (ptr_rule);

							_wfp_create4filters (&rules, __LINE__);

							_r_fastlock_releaseexclusive (&lock_access);

							_app_listviewsort (hwnd, listview_id, -1, false);
							_app_profile_save (hwnd);

							_r_listview_redraw (hwnd, listview_id);
						}
						else
						{
							_app_freerule (&ptr_rule);
						}
					}

					break;
				}

				case IDM_PROPERTIES:
				{
					const UINT listview_id = _app_gettab_id (hwnd);
					const size_t item = (size_t)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);

					if (
						listview_id == IDC_APPS_PROFILE ||
						listview_id == IDC_APPS_SERVICE ||
						listview_id == IDC_APPS_PACKAGE
						)
					{
						const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, listview_id, item);
						const PITEM_APP ptr_app = _app_getapplication (hash);

						if (ptr_app->type != AppPico && ptr_app->type != AppDevice)
						{
							if (_r_fs_exists (ptr_app->real_path))
							{
								SHELLEXECUTEINFO shex = {0};

								shex.cbSize = sizeof (shex);
								shex.fMask = SEE_MASK_UNICODE | SEE_MASK_NOZONECHECKS | SEE_MASK_INVOKEIDLIST;
								shex.hwnd = hwnd;
								shex.lpVerb = L"properties";
								shex.nShow = SW_NORMAL;
								shex.lpFile = ptr_app->real_path;

								ShellExecuteEx (&shex);
							}
						}
					}
					else if (listview_id == IDC_RULES_CUSTOM)
					{
						const size_t rule_idx = (size_t)_r_listview_getitemlparam (hwnd, listview_id, item);
						const PITEM_RULE ptr_rule = rules_arr.at (rule_idx);

						if (ptr_rule)
						{
							if (DialogBoxParam (nullptr, MAKEINTRESOURCE (IDD_EDITOR), hwnd, &EditorProc, (LPARAM)ptr_rule))
							{
								_r_fastlock_acquireexclusive (&lock_access);

								MFILTER_RULES rules;
								rules.push_back (ptr_rule);

								_wfp_create4filters (&rules, __LINE__);

								_r_fastlock_acquireshared (&lock_checkbox);

								_app_setruleitem (hwnd, listview_id, item, ptr_rule);

								_r_fastlock_releaseshared (&lock_checkbox);

								_r_fastlock_releaseexclusive (&lock_access);

								_app_listviewsort (hwnd, listview_id, -1, false);
								_app_profile_save (hwnd);

								_r_listview_redraw (hwnd, listview_id);
							}
						}
					}

					break;
				}

				case IDM_DELETE:
				{
					const UINT listview_id = _app_gettab_id (hwnd);

					if (listview_id != IDC_APPS_PROFILE && listview_id != IDC_RULES_CUSTOM)
						break;

					const UINT selected = (UINT)SendDlgItemMessage (hwnd, listview_id, LVM_GETSELECTEDCOUNT, 0, 0);

					if (!selected || !app.ConfirmMessage (hwnd, nullptr, _r_fmt (app.LocaleString (IDS_QUESTION_DELETE, nullptr), selected), L"ConfirmDelete"))
						break;

					const size_t count = _r_listview_getitemcount (hwnd, listview_id) - 1;

					size_t item = LAST_VALUE;

					MARRAY ids;

					_r_fastlock_acquireexclusive (&lock_access);

					for (size_t i = count; i != LAST_VALUE; i--)
					{
						if (ListView_GetItemState (GetDlgItem (hwnd, listview_id), i, LVNI_SELECTED))
						{
							if (listview_id == IDC_APPS_PROFILE)
							{
								const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, listview_id, i);
								PITEM_APP ptr_app = _app_getapplication (hash);

								if (ptr_app && !ptr_app->is_undeletable) // skip "undeletable" apps
								{
									ids.insert (ids.end (), ptr_app->mfarr.begin (), ptr_app->mfarr.end ());
									ptr_app->mfarr.clear ();

									SendDlgItemMessage (hwnd, listview_id, LVM_DELETEITEM, i, 0);
									_app_freeapplication (hash);

									item = i;
								}
							}
							else if (listview_id == IDC_RULES_CUSTOM)
							{
								const size_t rule_idx = (size_t)_r_listview_getitemlparam (hwnd, listview_id, i);
								PITEM_RULE ptr_rule = rules_arr.at (rule_idx);

								if (ptr_rule && !ptr_rule->is_readonly) // skip "read-only" apps
								{
									ids.insert (ids.end (), ptr_rule->mfarr.begin (), ptr_rule->mfarr.end ());
									ptr_rule->mfarr.clear ();
								}

								SendDlgItemMessage (hwnd, listview_id, LVM_DELETEITEM, i, 0);
								_app_freerule (&rules_arr.at (rule_idx));

								item = i;
							}
						}
					}

					_wfp_destroy2filters (&ids, __LINE__);

					_r_fastlock_releaseexclusive (&lock_access);

					if (item != LAST_VALUE)
						_app_showitem (hwnd, listview_id, min (item, _r_listview_getitemcount (hwnd, listview_id) - 1), -1);

					_app_profile_save (hwnd);

					_app_notifyrefresh (config.hnotification, false);
					_app_refreshstatus (hwnd);

					_r_listview_redraw (hwnd, listview_id);

					break;
				}

				case IDM_PURGE_UNUSED:
				{
					bool is_deleted = false;

					static const UINT listview_ids[] = {
						IDC_APPS_PROFILE,
						IDC_APPS_SERVICE,
						IDC_APPS_PACKAGE,
					};

					MARRAY ids;

					_r_fastlock_acquireexclusive (&lock_access);

					for (size_t i = 0; i < _countof (listview_ids); i++)
					{
						const size_t count = _r_listview_getitemcount (hwnd, listview_ids[i]) - 1;

						for (size_t j = count; j != LAST_VALUE; j--)
						{
							const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, listview_ids[i], j);
							PITEM_APP ptr_app = _app_getapplication (hash);

							if (ptr_app)
							{
								if (!ptr_app->is_undeletable && (ptr_app->type != AppService && ptr_app->type != AppPackage) && (!_app_isappexists (ptr_app) || !_app_isappused (ptr_app, hash)))
								{
									ids.insert (ids.end (), ptr_app->mfarr.begin (), ptr_app->mfarr.end ());
									ptr_app->mfarr.clear ();

									_app_freeapplication (hash);

									if (!is_deleted)
										is_deleted = true;
								}
							}
						}
					}

					_wfp_destroy2filters (&ids, __LINE__);

					_r_fastlock_releaseexclusive (&lock_access);

					if (is_deleted)
					{
						_app_profile_save (hwnd);
						_app_profile_load (hwnd);

						_app_notifyrefresh (config.hnotification, false);
						_app_refreshstatus (hwnd);

						_r_listview_redraw (hwnd, _app_gettab_id (hwnd));
					}

					break;
				}

				case IDM_PURGE_TIMERS:
				{
					if (!_app_istimersactive () || !app.ConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION_TIMERS, nullptr), L"ConfirmTimers"))
						break;

					_r_fastlock_acquireexclusive (&lock_access);

					MFILTER_APPS rules;

					for (auto &p : apps)
					{
						if (_app_istimeractive (&p.second))
							rules.push_back (&p.second);
					}

					_app_timer_remove (hwnd, &rules);

					_r_fastlock_releaseexclusive (&lock_access);

					const UINT listview_id = _app_gettab_id (hwnd);

					_app_listviewsort (hwnd, listview_id, -1, false);
					_app_profile_save (hwnd);

					_r_listview_redraw (hwnd, listview_id);

					break;
				}

				case IDM_SELECT_ALL:
				{
					ListView_SetItemState (GetDlgItem (hwnd, _app_gettab_id (hwnd)), -1, LVIS_SELECTED, LVIS_SELECTED);
					break;
				}

				case IDM_ZOOM:
				{
					ShowWindow (hwnd, IsZoomed (hwnd) ? SW_RESTORE : SW_MAXIMIZE);
					break;
				}

#if defined(_APP_BETA) || defined(_APP_BETA_RC)

#define ID_AD 9246740
#define FN_AD L"<test filter>"
#define RM_AD L"195.210.46.%d"
#define RP_AD 443
#define LM_AD L"192.168.2.%d"
#define LP_AD 0

				case 999:
				{
					static const UINT32 flags = FWPM_NET_EVENT_FLAG_APP_ID_SET |
						FWPM_NET_EVENT_FLAG_IP_PROTOCOL_SET |
						FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET |
						FWPM_NET_EVENT_FLAG_LOCAL_PORT_SET |
						FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET |
						FWPM_NET_EVENT_FLAG_REMOTE_PORT_SET |
						FWPM_NET_EVENT_FLAG_USER_ID_SET |
						FWPM_NET_EVENT_FLAG_IP_VERSION_SET;

					FILETIME ft = {0};
					GetSystemTimeAsFileTime (&ft);

					sockaddr_in ipv4_remote = {0};
					sockaddr_in ipv4_local = {0};
					INT len1 = sizeof (ipv4_remote);
					INT len2 = sizeof (ipv4_local);

					rstring path = app.GetBinaryPath ();;
					_r_path_ntpathfromdos (path);

					for (int i = 0; i < 255; i++)
					{
						WSAStringToAddress (_r_fmt (RM_AD, i + 1).GetBuffer (), AF_INET, nullptr, (sockaddr *)& ipv4_remote, &len1);
						WSAStringToAddress (_r_fmt (LM_AD, i + 1).GetBuffer (), AF_INET, nullptr, (sockaddr *)& ipv4_local, &len2);

						_wfp_logcallback (flags, &ft, (UINT8 *)path.GetString (), nullptr, nullptr, IPPROTO_TCP, FWP_IP_VERSION_V4, ntohl (ipv4_remote.sin_addr.S_un.S_addr), nullptr, RP_AD, 0, nullptr, LP_AD, 0, 9240001 + i, FWP_DIRECTION_INBOUND, false, false);
					}

					break;
				}

				case 998:
				{
					apps[config.myhash].last_notify = 0;

					PITEM_LOG ptr_log = new ITEM_LOG;

					if (ptr_log)
					{
						ptr_log->hash = config.myhash;
						ptr_log->date = _r_unixtime_now ();

						ptr_log->af = AF_INET;
						ptr_log->protocol = IPPROTO_TCP;

						ptr_log->filter_id = ID_AD;

						InetPton (ptr_log->af, RM_AD, &ptr_log->remote_addr);
						ptr_log->remote_port = 443;

						InetPton (ptr_log->af, LM_AD, &ptr_log->local_addr);
						ptr_log->local_port = 80;

						_r_str_alloc (&ptr_log->path, _r_str_length (app.GetBinaryPath ()), app.GetBinaryPath ());
						_r_str_alloc (&ptr_log->filter_name, _r_str_length (FN_AD), FN_AD);

						_app_formataddress (ptr_log, FWP_DIRECTION_OUTBOUND, ptr_log->remote_port, &ptr_log->remote_fmt, true);
						_app_formataddress (ptr_log, FWP_DIRECTION_INBOUND, ptr_log->local_port, &ptr_log->local_fmt, true);

						_app_notifyadd (config.hnotification, ptr_log, &apps[config.myhash]);
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
		LPWSTR *arga = CommandLineToArgvW (GetCommandLine (), &numargs);

		bool is_install = false;
		bool is_uninstall = false;
		bool is_silent = false;

		for (INT i = 0; i < numargs; i++)
		{
			if (_wcsicmp (arga[i], L"/install") == 0)
				is_install = true;

			else if (_wcsicmp (arga[i], L"/uninstall") == 0)
				is_uninstall = true;

			else if (_wcsicmp (arga[i], L"/silent") == 0)
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

				return ERROR_SUCCESS;
			}
			else if (is_uninstall)
			{
				if (app.IsAdmin () && _wfp_isfiltersinstalled () && _app_installmessage (nullptr, false))
				{
					if (_wfp_initialize (false))
						_wfp_destroyfilters ();

					_wfp_uninitialize (true);
				}

				return ERROR_SUCCESS;
			}
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
