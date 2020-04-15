// simplewall
// Copyright (c) 2016-2020 Henry++

#include "global.hpp"

const UINT WM_FINDMSGSTRING = RegisterWindowMessage (FINDMSGSTRING);

UINT WINAPI ApplyThread (LPVOID lparam)
{
	PINSTALL_CONTEXT pcontext = (PINSTALL_CONTEXT)lparam;

	_r_fastlock_acquireshared (&lock_apply);

	const HANDLE& hengine = _wfp_getenginehandle ();

	// dropped packets logging (win7+)
	if (config.is_neteventset)
		_wfp_logunsubscribe (hengine);

	if (pcontext->is_install)
	{
		if (_wfp_initialize (true))
			_wfp_installfilters ();
	}
	else
	{
		if (_wfp_initialize (false))
			_wfp_destroyfilters (hengine);

		_wfp_uninitialize (true);
	}

	// dropped packets logging (win7+)
	if (config.is_neteventset)
		_wfp_logsubscribe (hengine);

	_app_restoreinterfacestate (pcontext->hwnd, true);
	_app_setinterfacestate (pcontext->hwnd);

	_app_profile_save ();

	SetEvent (config.done_evt);

	_r_mem_free (pcontext);

	_r_fastlock_releaseshared (&lock_apply);

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

		network_timeout = std::clamp (network_timeout, 500UL, 60UL * 1000UL); // set allowed range

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

				PITEM_NETWORK ptr_network = (PITEM_NETWORK)ptr_network_object->pdata;

				if (ptr_network)
				{
					const INT item = _r_listview_getitemcount (hwnd, network_listview_id);

					_r_listview_additem (hwnd, network_listview_id, item, 0, _r_path_getfilename (ptr_network->path), ptr_network->icon_id, I_GROUPIDNONE, p.first);

					_r_listview_setitem (hwnd, network_listview_id, item, 1, ptr_network->local_fmt);
					_r_listview_setitem (hwnd, network_listview_id, item, 3, ptr_network->remote_fmt);
					_r_listview_setitem (hwnd, network_listview_id, item, 5, _app_getprotoname (ptr_network->protocol, ptr_network->af));
					_r_listview_setitem (hwnd, network_listview_id, item, 6, _app_getstatename (ptr_network->state));

					if (ptr_network->local_port)
						_r_listview_setitem (hwnd, network_listview_id, item, 2, _app_formatport (ptr_network->local_port, true));

					if (ptr_network->remote_port)
						_r_listview_setitem (hwnd, network_listview_id, item, 4, _app_formatport (ptr_network->remote_port, true));

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

					is_refresh = true;
				}

				_r_obj_dereference (ptr_network_object);
			}

			// refresh network tab as well
			if (is_refresh)
			{
				const INT current_listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

				if (current_listview_id == network_listview_id)
				{
					SendDlgItemMessage (hwnd, network_listview_id, WM_SETREDRAW, FALSE, 0);

					_app_listviewresize (hwnd, network_listview_id, false);
					_app_listviewsort (hwnd, network_listview_id);

					SendDlgItemMessage (hwnd, network_listview_id, WM_SETREDRAW, TRUE, 0);
				}
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
					}
				}
			}

			WaitForSingleObjectEx (NtCurrentThread (), network_timeout, FALSE);
		}
	}

	_endthreadex (0);

	return ERROR_SUCCESS;
}

bool _app_changefilters (HWND hwnd, bool is_install, bool is_forced)
{
	if (_wfp_isfiltersapplying ())
		return false;

	const INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

	_app_listviewsort (hwnd, listview_id);

	if (is_forced || _wfp_isfiltersinstalled ())
	{
		_app_initinterfacestate (hwnd, true);

		_app_freethreadpool (&threads_pool);

		PINSTALL_CONTEXT pcontext = (PINSTALL_CONTEXT)_r_mem_allocex (sizeof (INSTALL_CONTEXT), HEAP_ZERO_MEMORY);

		if (pcontext)
		{
			pcontext->hwnd = hwnd;
			pcontext->is_install = is_install;

			const HANDLE hthread = _r_createthread (&ApplyThread, (LPVOID)pcontext, true, THREAD_PRIORITY_HIGHEST);

			if (hthread)
			{
				threads_pool.push_back (hthread);
				ResumeThread (hthread);
			}
			else
			{
				_r_mem_free (pcontext);
			}

			return hthread != nullptr;
		}
	}

	_app_profile_save ();

	_r_listview_redraw (hwnd, listview_id);

	return false;
}

void addcolor (UINT locale_id, LPCWSTR config_name, bool is_enabled, LPCWSTR config_value, COLORREF default_clr)
{
	PITEM_COLOR ptr_clr = new ITEM_COLOR;
	RtlSecureZeroMemory (ptr_clr, sizeof (ITEM_COLOR));

	if (!_r_str_isempty (config_name))
		_r_str_alloc (&ptr_clr->pcfg_name, _r_str_length (config_name), config_name);

	if (!_r_str_isempty (config_value))
	{
		_r_str_alloc (&ptr_clr->pcfg_value, _r_str_length (config_value), config_value);

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
	tdc.lpCallbackData = MAKELONG (0, TRUE); // on top
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
			_r_menu_checkitem (hmenu, IDM_RULE_BLOCKOUTBOUND, 0, MF_BYCOMMAND, new_val);

			break;
		}

		case IDC_RULE_BLOCKINBOUND:
		case IDM_RULE_BLOCKINBOUND:
		{
			app.ConfigSet (L"BlockInboundConnections", new_val);
			_r_menu_checkitem (hmenu, IDM_RULE_BLOCKINBOUND, 0, MF_BYCOMMAND, new_val);

			break;
		}

		case IDC_RULE_ALLOWLOOPBACK:
		case IDM_RULE_ALLOWLOOPBACK:
		{
			app.ConfigSet (L"AllowLoopbackConnections", new_val);
			_r_menu_checkitem (hmenu, IDM_RULE_ALLOWLOOPBACK, 0, MF_BYCOMMAND, new_val);

			break;
		}

		case IDC_RULE_ALLOW6TO4:
		case IDM_RULE_ALLOW6TO4:
		{
			app.ConfigSet (L"AllowIPv6", new_val);
			_r_menu_checkitem (hmenu, IDM_RULE_ALLOW6TO4, 0, MF_BYCOMMAND, new_val);

			break;
		}

		case IDC_USESTEALTHMODE_CHK:
		case IDM_USESTEALTHMODE_CHK:
		{
			app.ConfigSet (L"UseStealthMode", new_val);
			_r_menu_checkitem (hmenu, IDM_USESTEALTHMODE_CHK, 0, MF_BYCOMMAND, new_val);

			break;
		}

		case IDC_SECUREFILTERS_CHK:
		case IDM_SECUREFILTERS_CHK:
		{
			app.ConfigSet (L"IsSecureFilters", new_val);
			_r_menu_checkitem (hmenu, IDM_SECUREFILTERS_CHK, 0, MF_BYCOMMAND, new_val);

			PACL& pacl = new_val ? config.pacl_secure : config.pacl_default;

			if (pacl)
			{
				const HANDLE hengine = _wfp_getenginehandle ();

				GUIDS_VEC filter_all;

				if (_wfp_dumpfilters (hengine, &GUID_WfpProvider, &filter_all))
				{
					for (auto &id : filter_all)
						_wfp_setfiltersecurity (hengine, &id, pacl, __LINE__);
				}

				// set security information
				if (config.padminsid)
				{
					FwpmProviderSetSecurityInfoByKey (hengine, &GUID_WfpProvider, OWNER_SECURITY_INFORMATION, (const SID*)config.padminsid, nullptr, nullptr, nullptr);
					FwpmSubLayerSetSecurityInfoByKey (hengine, &GUID_WfpSublayer, OWNER_SECURITY_INFORMATION, (const SID*)config.padminsid, nullptr, nullptr, nullptr);
				}

				FwpmProviderSetSecurityInfoByKey (hengine, &GUID_WfpProvider, DACL_SECURITY_INFORMATION, nullptr, nullptr, pacl, nullptr);
				FwpmSubLayerSetSecurityInfoByKey (hengine, &GUID_WfpSublayer, DACL_SECURITY_INFORMATION, nullptr, nullptr, pacl, nullptr);
			}

			break;
		}

		case IDC_INSTALLBOOTTIMEFILTERS_CHK:
		case IDM_INSTALLBOOTTIMEFILTERS_CHK:
		{
			app.ConfigSet (L"InstallBoottimeFilters", new_val);
			_r_menu_checkitem (hmenu, IDM_INSTALLBOOTTIMEFILTERS_CHK, 0, MF_BYCOMMAND, new_val);

			break;
		}

		case IDC_USENETWORKRESOLUTION_CHK:
		case IDM_USENETWORKRESOLUTION_CHK:
		{
			app.ConfigSet (L"IsNetworkResolutionsEnabled", new_val);
			_r_menu_checkitem (hmenu, IDM_USENETWORKRESOLUTION_CHK, 0, MF_BYCOMMAND, new_val);

			break;
		}

		case IDC_USECERTIFICATES_CHK:
		case IDM_USECERTIFICATES_CHK:
		{
			app.ConfigSet (L"IsCertificatesEnabled", new_val);
			_r_menu_checkitem (hmenu, IDM_USECERTIFICATES_CHK, 0, MF_BYCOMMAND, new_val);

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

			_r_listview_redraw (app.GetHWND (), (INT)_r_tab_getlparam (app.GetHWND (), IDC_TAB, INVALID_INT));

			break;
		}

		case IDC_USEREFRESHDEVICES_CHK:
		case IDM_USEREFRESHDEVICES_CHK:
		{
			app.ConfigSet (L"IsRefreshDevices", new_val);
			_r_menu_checkitem (hmenu, IDM_USEREFRESHDEVICES_CHK, 0, MF_BYCOMMAND, new_val);

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
	switch (msg)
	{
		case WM_INITDIALOG:
		{
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
					CheckDlgButton (hwnd, IDC_ALWAYSONTOP_CHK, app.ConfigGet (L"AlwaysOnTop", _APP_ALWAYSONTOP).AsBool () ? BST_CHECKED : BST_UNCHECKED);

#if defined(_APP_HAVE_AUTORUN)
					CheckDlgButton (hwnd, IDC_LOADONSTARTUP_CHK, app.AutorunIsEnabled () ? BST_CHECKED : BST_UNCHECKED);
#endif // _APP_HAVE_AUTORUN

					CheckDlgButton (hwnd, IDC_STARTMINIMIZED_CHK, app.ConfigGet (L"IsStartMinimized", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

#if defined(_APP_HAVE_SKIPUAC)
					CheckDlgButton (hwnd, IDC_SKIPUACWARNING_CHK, app.SkipUacIsEnabled () ? BST_CHECKED : BST_UNCHECKED);
#endif // _APP_HAVE_SKIPUAC

					CheckDlgButton (hwnd, IDC_CHECKUPDATES_CHK, app.ConfigGet (L"CheckUpdates", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

#if defined(_DEBUG) || defined(_APP_BETA)
					CheckDlgButton (hwnd, IDC_CHECKUPDATESBETA_CHK, BST_CHECKED);
					_r_ctrl_enable (hwnd, IDC_CHECKUPDATESBETA_CHK, false);
#else
					CheckDlgButton (hwnd, IDC_CHECKUPDATESBETA_CHK, app.ConfigGet (L"CheckUpdatesBeta", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					if (!app.ConfigGet (L"CheckUpdates", true).AsBool ())
						_r_ctrl_enable (hwnd, IDC_CHECKUPDATESBETA_CHK, false);
#endif // _DEBUG || _APP_BETA

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

					if (htip)
					{
						_r_ctrl_settip (htip, hwnd, IDC_RULE_BLOCKOUTBOUND, LPSTR_TEXTCALLBACK);
						_r_ctrl_settip (htip, hwnd, IDC_RULE_BLOCKINBOUND, LPSTR_TEXTCALLBACK);
						_r_ctrl_settip (htip, hwnd, IDC_RULE_ALLOWLOOPBACK, LPSTR_TEXTCALLBACK);
						_r_ctrl_settip (htip, hwnd, IDC_RULE_ALLOW6TO4, LPSTR_TEXTCALLBACK);

						_r_ctrl_settip (htip, hwnd, IDC_USESTEALTHMODE_CHK, LPSTR_TEXTCALLBACK);
						_r_ctrl_settip (htip, hwnd, IDC_INSTALLBOOTTIMEFILTERS_CHK, LPSTR_TEXTCALLBACK);
						_r_ctrl_settip (htip, hwnd, IDC_SECUREFILTERS_CHK, LPSTR_TEXTCALLBACK);
					}

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

					break;
				}

				case IDD_SETTINGS_HIGHLIGHTING:
				{
					// configure listview
					_r_listview_setstyle (hwnd, IDC_COLORS, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_CHECKBOXES);

					_app_listviewsetview (hwnd, IDC_COLORS);

					_r_listview_deleteallitems (hwnd, IDC_COLORS);
					_r_listview_deleteallcolumns (hwnd, IDC_COLORS);

					_r_listview_addcolumn (hwnd, IDC_COLORS, 0, nullptr, -95, LVCFMT_LEFT);

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

								_r_listview_additem (hwnd, IDC_COLORS, item, 0, app.LocaleString (ptr_clr->locale_id, nullptr), config.icon_id, I_GROUPIDNONE, i);
								_r_listview_setitemcheck (hwnd, IDC_COLORS, item, app.ConfigGet (ptr_clr->pcfg_name, ptr_clr->is_enabled, L"colors").AsBool ());

								_r_fastlock_releaseshared (&lock_checkbox);

								item += 1;
							}

							_r_obj_dereference (ptr_color_object);
						}
					}

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
					SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_SETRANGE32, 64, _R_BYTESIZE_KB * 512);
					SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_SETPOS32, 0, (LPARAM)app.ConfigGet (L"LogSizeLimitKb", LOG_SIZE_LIMIT_DEFAULT).AsUlong ());

					CheckDlgButton (hwnd, IDC_EXCLUDESTEALTH_CHK, app.ConfigGet (L"IsExcludeStealth", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, app.ConfigGet (L"IsExcludeClassifyAllow", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					// win8+
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

					_r_ctrl_settext (hwnd, IDC_BLOCKLIST_INFO, L"Author: <a href=\"%s\">WindowsSpyBlocker</a> - block spying and tracking on Windows systems.", WINDOWSSPYBLOCKER_URL, WINDOWSSPYBLOCKER_URL);

					break;
				}

				case IDD_SETTINGS_INTERFACE:
				{
					SetDlgItemText (hwnd, IDC_CONFIRMEXIT_CHK, app.LocaleString (IDS_CONFIRMEXIT_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_CONFIRMEXITTIMER_CHK, app.LocaleString (IDS_CONFIRMEXITTIMER_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_CONFIRMLOGCLEAR_CHK, app.LocaleString (IDS_CONFIRMLOGCLEAR_CHK, nullptr));

					break;
				}

				case IDD_SETTINGS_HIGHLIGHTING:
				{
					_app_listviewsetfont (hwnd, IDC_COLORS, false);

					_r_listview_setcolumn (hwnd, IDC_COLORS, 0, nullptr, -100);

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

					SetDlgItemText (hwnd, IDC_COLORS_HINT, app.LocaleString (IDS_COLORS_HINT, nullptr));

					break;
				}

				case IDD_SETTINGS_NOTIFICATIONS:
				{

					SetDlgItemText (hwnd, IDC_ENABLENOTIFICATIONS_CHK, app.LocaleString (IDS_ENABLENOTIFICATIONS_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_NOTIFICATIONSOUND_CHK, app.LocaleString (IDS_NOTIFICATIONSOUND_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_NOTIFICATIONONTRAY_CHK, app.LocaleString (IDS_NOTIFICATIONONTRAY_CHK, nullptr));

					SetDlgItemText (hwnd, IDC_NOTIFICATIONTIMEOUT_HINT, app.LocaleString (IDS_NOTIFICATIONTIMEOUT_HINT, nullptr));

					const rstring exclude = app.LocaleString (IDS_TITLE_EXCLUDE, nullptr);

					SetDlgItemText (hwnd, IDC_EXCLUDEBLOCKLIST_CHK, _r_fmt (L"%s %s", exclude.GetString (), app.LocaleString (IDS_EXCLUDEBLOCKLIST_CHK, nullptr).GetString ()));
					SetDlgItemText (hwnd, IDC_EXCLUDECUSTOM_CHK, _r_fmt (L"%s %s", exclude.GetString (), app.LocaleString (IDS_EXCLUDECUSTOM_CHK, nullptr).GetString ()));

					break;
				}

				case IDD_SETTINGS_LOGGING:
				{
					SetDlgItemText (hwnd, IDC_ENABLELOG_CHK, app.LocaleString (IDS_ENABLELOG_CHK, nullptr));

					SetDlgItemText (hwnd, IDC_LOGVIEWER_HINT, app.LocaleString (IDS_LOGVIEWER_HINT, L":"));
					SetDlgItemText (hwnd, IDC_LOGSIZELIMIT_HINT, app.LocaleString (IDS_LOGSIZELIMIT_HINT, nullptr));

					const rstring exclude = app.LocaleString (IDS_TITLE_EXCLUDE, nullptr);

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
						const INT ctrl_id = GetDlgCtrlID (reinterpret_cast<HWND>(lpnmdi->hdr.idFrom));

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

					const INT listview_id = static_cast<INT>(lpnmlv->hdr.idFrom);

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

							_r_listview_redraw (app.GetHWND (), (INT)_r_tab_getlparam (app.GetHWND (), IDC_TAB, INVALID_INT));
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

					const INT listview_id = static_cast<INT>(lpnmlv->hdr.idFrom);

					if (lpnmlv->iItem == INVALID_INT || listview_id != IDC_COLORS)
						break;

					const size_t idx = _r_listview_getitemlparam (hwnd, listview_id, lpnmlv->iItem);
					PR_OBJECT ptr_clr_object_current = _r_obj_reference (colors.at (idx));

					PITEM_COLOR ptr_clr_current = nullptr;

					if (ptr_clr_object_current)
						ptr_clr_current = (PITEM_COLOR)ptr_clr_object_current->pdata;

					CHOOSECOLOR cc = {0};
					COLORREF cust[16] = {0};

					size_t index = 0;

					for (auto &p : colors)
					{
						PR_OBJECT ptr_clr_object = _r_obj_reference (p);

						if (ptr_clr_object)
						{
							PITEM_COLOR ptr_clr = (PITEM_COLOR)ptr_clr_object->pdata;

							if (ptr_clr)
								cust[index] = ptr_clr->default_clr;

							_r_obj_dereference (ptr_clr_object);
						}

						index += 1;
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
						_r_listview_redraw (app.GetHWND (), (INT)_r_tab_getlparam (app.GetHWND (), IDC_TAB, INVALID_INT));
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
			const INT ctrl_id = LOWORD (wparam);
			const INT notify_code = HIWORD (wparam);

			switch (ctrl_id)
			{
				case IDC_ALWAYSONTOP_CHK:
				{
					const bool is_enabled = !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

					app.ConfigSet (L"AlwaysOnTop", is_enabled);
					_r_menu_checkitem (GetMenu (app.GetHWND ()), IDM_ALWAYSONTOP_CHK, 0, MF_BYCOMMAND, is_enabled);

					break;
				}

#if defined(_APP_HAVE_AUTORUN)
				case IDC_LOADONSTARTUP_CHK:
				{
					app.AutorunEnable (hwnd, IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);
					CheckDlgButton (hwnd, ctrl_id, app.AutorunIsEnabled () ? BST_CHECKED : BST_UNCHECKED);

					break;
				}
#endif // _APP_HAVE_AUTORUN

				case IDC_STARTMINIMIZED_CHK:
				{
					app.ConfigSet (L"IsStartMinimized", !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}

#if defined(_APP_HAVE_SKIPUAC)
				case IDC_SKIPUACWARNING_CHK:
				{
					app.SkipUacEnable (hwnd, IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);
					CheckDlgButton (hwnd, ctrl_id, app.SkipUacIsEnabled () ? BST_CHECKED : BST_UNCHECKED);

					break;
				}
#endif // _APP_HAVE_SKIPUAC

				case IDC_CHECKUPDATES_CHK:
				{
					const bool is_enabled = !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

					app.ConfigSet (L"CheckUpdates", is_enabled);

#if !defined(_DEBUG) && !defined(_APP_BETA)
					_r_ctrl_enable (hwnd, IDC_CHECKUPDATESBETA_CHK, is_enabled ? true : false);
#endif // !_DEBUG && !_APP_BETA

					break;
				}

#if !defined(_DEBUG) && !defined(_APP_BETA)
				case IDC_CHECKUPDATESBETA_CHK:
				{
					app.ConfigSet (L"CheckUpdatesBeta", !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}
#endif // !_DEBUG && !_APP_BETA

				case IDC_LANGUAGE:
				{
					if (notify_code == CBN_SELCHANGE)
						app.LocaleApplyFromControl (hwnd, ctrl_id);

					break;
				}

				case IDC_CONFIRMEXIT_CHK:
				{
					app.ConfigSet (L"ConfirmExit2", !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}

				case IDC_CONFIRMEXITTIMER_CHK:
				{
					app.ConfigSet (L"ConfirmExitTimer", !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}

				case IDC_CONFIRMLOGCLEAR_CHK:
				{
					app.ConfigSet (L"ConfirmLogClear", !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}

				case IDC_RULE_BLOCKOUTBOUND:
				case IDC_RULE_BLOCKINBOUND:
				case IDC_RULE_ALLOWLOOPBACK:
				case IDC_RULE_ALLOW6TO4:
				case IDC_USESTEALTHMODE_CHK:
				case IDC_INSTALLBOOTTIMEFILTERS_CHK:
				case IDC_SECUREFILTERS_CHK:
				case IDC_USECERTIFICATES_CHK:
				case IDC_USENETWORKRESOLUTION_CHK:
				case IDC_USEREFRESHDEVICES_CHK:
				{
					_app_config_apply (app.GetHWND (), ctrl_id);
					break;
				}

				case IDC_BLOCKLIST_SPY_DISABLE:
				case IDC_BLOCKLIST_SPY_ALLOW:
				case IDC_BLOCKLIST_SPY_BLOCK:
				case IDC_BLOCKLIST_UPDATE_DISABLE:
				case IDC_BLOCKLIST_UPDATE_ALLOW:
				case IDC_BLOCKLIST_UPDATE_BLOCK:
				case IDC_BLOCKLIST_EXTRA_DISABLE:
				case IDC_BLOCKLIST_EXTRA_ALLOW:
				case IDC_BLOCKLIST_EXTRA_BLOCK:
				{
					const HMENU hmenu = GetMenu (app.GetHWND ());

					if (ctrl_id >= IDC_BLOCKLIST_SPY_DISABLE && ctrl_id <= IDC_BLOCKLIST_SPY_BLOCK)
					{
						const INT new_state = std::clamp (_r_ctrl_isradiobuttonchecked (hwnd, IDC_BLOCKLIST_SPY_DISABLE, IDC_BLOCKLIST_SPY_BLOCK) - IDC_BLOCKLIST_SPY_DISABLE, 0, 2);

						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_SPY_DISABLE, IDM_BLOCKLIST_SPY_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_SPY_DISABLE + new_state);

						app.ConfigSet (L"BlocklistSpyState", new_state);

						_r_fastlock_acquireshared (&lock_access);
						_app_ruleblocklistset (app.GetHWND (), new_state, INVALID_INT, INVALID_INT, true);
						_r_fastlock_releaseshared (&lock_access);
					}
					else if (ctrl_id >= IDC_BLOCKLIST_UPDATE_DISABLE && ctrl_id <= IDC_BLOCKLIST_UPDATE_BLOCK)
					{
						const INT new_state = std::clamp (_r_ctrl_isradiobuttonchecked (hwnd, IDC_BLOCKLIST_UPDATE_DISABLE, IDC_BLOCKLIST_UPDATE_BLOCK) - IDC_BLOCKLIST_UPDATE_DISABLE, 0, 2);

						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_UPDATE_DISABLE, IDM_BLOCKLIST_UPDATE_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_UPDATE_DISABLE + new_state);

						app.ConfigSet (L"BlocklistUpdateState", new_state);

						_r_fastlock_acquireshared (&lock_access);
						_app_ruleblocklistset (app.GetHWND (), INVALID_INT, new_state, INVALID_INT, true);
						_r_fastlock_releaseshared (&lock_access);
					}
					else if (ctrl_id >= IDC_BLOCKLIST_EXTRA_DISABLE && ctrl_id <= IDC_BLOCKLIST_EXTRA_BLOCK)
					{
						const INT new_state = std::clamp (_r_ctrl_isradiobuttonchecked (hwnd, IDC_BLOCKLIST_EXTRA_DISABLE, IDC_BLOCKLIST_EXTRA_BLOCK) - IDC_BLOCKLIST_EXTRA_DISABLE, 0, 2);

						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_EXTRA_DISABLE, IDM_BLOCKLIST_EXTRA_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_EXTRA_DISABLE + new_state);

						app.ConfigSet (L"BlocklistExtraState", new_state);

						_r_fastlock_acquireshared (&lock_access);
						_app_ruleblocklistset (app.GetHWND (), INVALID_INT, INVALID_INT, new_state, true);
						_r_fastlock_releaseshared (&lock_access);
					}

					break;
				}

				case IDC_ENABLELOG_CHK:
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

					// win8+
					if (_r_sys_validversion (6, 2))
						_r_ctrl_enable (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, is_enabled);

					_app_loginit (is_enabled);

					break;
				}

				case IDC_LOGPATH:
				{
					if (notify_code == EN_KILLFOCUS)
					{
						rstring logpath = _r_ctrl_gettext (hwnd, ctrl_id);

						if (!logpath.IsEmpty ())
						{
							app.ConfigSet (L"LogPath", _r_path_unexpand (logpath));

							_app_loginit (app.ConfigGet (L"IsLogEnabled", false));
						}
					}

					break;
				}

				case IDC_LOGPATH_BTN:
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

					break;
				}

				case IDC_LOGVIEWER:
				{
					if (notify_code == EN_KILLFOCUS)
					{
						rstring logviewer = _r_ctrl_gettext (hwnd, ctrl_id);

						if (!logviewer.IsEmpty ())
							app.ConfigSet (L"LogViewer", _r_path_unexpand (logviewer));
					}

					break;
				}

				case IDC_LOGVIEWER_BTN:
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

					break;
				}

				case IDC_LOGSIZELIMIT_CTRL:
				{
					if (notify_code == EN_KILLFOCUS)
						app.ConfigSet (L"LogSizeLimitKb", (DWORD)SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_GETPOS32, 0, 0));

					break;
				}

				case IDC_ENABLENOTIFICATIONS_CHK:
				{
					const bool is_enabled = !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

					app.ConfigSet (L"IsNotificationsEnabled", is_enabled);
					SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_PRESSBUTTON, IDM_TRAY_ENABLENOTIFICATIONS_CHK, MAKELPARAM (is_enabled, 0));

					_r_ctrl_enable (hwnd, IDC_NOTIFICATIONSOUND_CHK, is_enabled);
					_r_ctrl_enable (hwnd, IDC_NOTIFICATIONONTRAY_CHK, is_enabled);

					HWND hbuddy = (HWND)SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_GETBUDDY, 0, 0);

					if (hbuddy)
						EnableWindow (hbuddy, is_enabled);

					_r_ctrl_enable (hwnd, IDC_EXCLUDEBLOCKLIST_CHK, is_enabled);
					_r_ctrl_enable (hwnd, IDC_EXCLUDECUSTOM_CHK, is_enabled);

					_app_notifyrefresh (config.hnotification, false);

					break;
				}

				case IDC_NOTIFICATIONSOUND_CHK:
				{
					app.ConfigSet (L"IsNotificationsSound", !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}

				case IDC_NOTIFICATIONONTRAY_CHK:
				{
					app.ConfigSet (L"IsNotificationsOnTray", !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));

					if (IsWindowVisible (config.hnotification))
						_app_notifysetpos (config.hnotification, true);

					break;
				}

				case IDC_NOTIFICATIONTIMEOUT_CTRL:
				{
					if (notify_code == EN_KILLFOCUS)
						app.ConfigSet (L"NotificationsTimeout", (time_t)SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_GETPOS32, 0, 0));

					break;
				}

				case IDC_EXCLUDESTEALTH_CHK:
				{
					app.ConfigSet (L"IsExcludeStealth", !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}

				case IDC_EXCLUDECLASSIFYALLOW_CHK:
				{
					app.ConfigSet (L"IsExcludeClassifyAllow", !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}

				case IDC_EXCLUDEBLOCKLIST_CHK:
				{
					app.ConfigSet (L"IsExcludeBlocklist", !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}

				case IDC_EXCLUDECUSTOM_CHK:
				{
					app.ConfigSet (L"IsExcludeCustomRules", !!(IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

void _app_resizewindow (HWND hwnd, LPARAM lparam)
{
	SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_AUTOSIZE, 0, 0);
	SendDlgItemMessage (hwnd, IDC_STATUSBAR, WM_SIZE, 0, 0);

	RECT rc = {0};
	GetClientRect (GetDlgItem (hwnd, IDC_STATUSBAR), &rc);

	const INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);
	const INT statusbar_height = _R_RECT_HEIGHT (&rc);
	const INT rebar_height = (INT)SendDlgItemMessage (hwnd, IDC_REBAR, RB_GETBARHEIGHT, 0, 0);

	HDWP hdefer = BeginDeferWindowPos (2);

	hdefer = DeferWindowPos (hdefer, config.hrebar, nullptr, 0, 0, LOWORD (lparam), rebar_height, SWP_NOZORDER | SWP_NOOWNERZORDER);
	hdefer = DeferWindowPos (hdefer, GetDlgItem (hwnd, IDC_TAB), nullptr, 0, rebar_height, LOWORD (lparam), HIWORD (lparam) - rebar_height - statusbar_height, SWP_NOZORDER | SWP_NOOWNERZORDER);

	EndDeferWindowPos (hdefer);

	_r_tab_adjustchild (hwnd, IDC_TAB, GetDlgItem (hwnd, listview_id));
	_app_listviewresize (hwnd, listview_id, false);

	_app_refreshstatus (hwnd, 0);
}

void _app_imagelist_init (HWND hwnd)
{
	const INT icon_size_small = _r_dc_getsystemmetrics (hwnd, SM_CXSMICON);
	const INT icon_size_large = _r_dc_getsystemmetrics (hwnd, SM_CXICON);
	const INT icon_size_toolbar = std::clamp (_r_dc_getdpi (hwnd, app.ConfigGet (L"ToolbarSize", _R_SIZE_ITEMHEIGHT).AsInt ()), icon_size_small, icon_size_large);

	SAFE_DELETE_OBJECT (config.hbmp_enable);
	SAFE_DELETE_OBJECT (config.hbmp_disable);
	SAFE_DELETE_OBJECT (config.hbmp_allow);
	SAFE_DELETE_OBJECT (config.hbmp_block);
	SAFE_DELETE_OBJECT (config.hbmp_cross);
	SAFE_DELETE_OBJECT (config.hbmp_rules);
	SAFE_DELETE_OBJECT (config.hbmp_checked);
	SAFE_DELETE_OBJECT (config.hbmp_unchecked);

	SAFE_DELETE_ICON (config.hicon_large);
	SAFE_DELETE_ICON (config.hicon_small);
	SAFE_DELETE_ICON (config.hicon_package);

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
			config.hicon_package = CopyIcon (config.hicon_small);
		}
	}

	config.hbmp_enable = _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_SHIELD_ENABLE), icon_size_small);
	config.hbmp_disable = _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_SHIELD_DISABLE), icon_size_small);

	config.hbmp_allow = _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_ALLOW), icon_size_small);
	config.hbmp_block = _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_BLOCK), icon_size_small);
	config.hbmp_cross = _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_CROSS), icon_size_small);
	config.hbmp_rules = _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_RULES), icon_size_small);

	config.hbmp_checked = _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_CHECKED), icon_size_small);
	config.hbmp_unchecked = _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_UNCHECKED), icon_size_small);

	// toolbar imagelist
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

	// rules imagelist (small)
	if (config.himg_rules_small)
		ImageList_SetIconSize (config.himg_rules_small, icon_size_small, icon_size_small);
	else
		config.himg_rules_small = ImageList_Create (icon_size_small, icon_size_small, ILC_COLOR32 | ILC_HIGHQUALITYSCALE, 2, 2);

	if (config.himg_rules_small)
	{
		ImageList_Add (config.himg_rules_small, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_ALLOW), icon_size_small), nullptr);
		ImageList_Add (config.himg_rules_small, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_BLOCK), icon_size_small), nullptr);
	}

	// rules imagelist (large)
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

	if (!app.ConfigGet (L"IsInternalRulesDisabled", false).AsBool ())
	{
		CreateWindowEx (0, WC_LISTVIEW, nullptr, listview_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_RULES_BLOCKLIST, hinst, nullptr);
		_r_tab_additem (hwnd, IDC_TAB, tabs_count++, app.LocaleString (IDS_TRAY_BLOCKLIST_RULES, nullptr), INVALID_INT, IDC_RULES_BLOCKLIST);

		CreateWindowEx (0, WC_LISTVIEW, nullptr, listview_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_RULES_SYSTEM, hinst, nullptr);
		_r_tab_additem (hwnd, IDC_TAB, tabs_count++, app.LocaleString (IDS_TRAY_SYSTEM_RULES, nullptr), INVALID_INT, IDC_RULES_SYSTEM);
	}

	CreateWindowEx (0, WC_LISTVIEW, nullptr, listview_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_RULES_CUSTOM, hinst, nullptr);
	_r_tab_additem (hwnd, IDC_TAB, tabs_count++, app.LocaleString (IDS_TRAY_USER_RULES, nullptr), INVALID_INT, IDC_RULES_CUSTOM);

	CreateWindowEx (0, WC_LISTVIEW, nullptr, listview_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_NETWORK, hinst, nullptr);
	_r_tab_additem (hwnd, IDC_TAB, tabs_count++, app.LocaleString (IDS_TAB_NETWORK, nullptr), INVALID_INT, IDC_NETWORK);

	for (INT i = 0; i < tabs_count; i++)
	{
		const INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, i);

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

		HWND hlistview = GetDlgItem (hwnd, listview_id);

		if (hlistview)
		{
			_r_tab_adjustchild (hwnd, IDC_TAB, hlistview);

			BringWindowToTop (hlistview); // HACK!!!
		}
	}
}

void _app_initialize ()
{
	pugi::set_memory_management_functions (&_r_mem_alloc, &_r_mem_free); // set allocation routine

	// initialize spinlocks
	_r_fastlock_initialize (&lock_access);
	_r_fastlock_initialize (&lock_apply);
	_r_fastlock_initialize (&lock_cache);
	_r_fastlock_initialize (&lock_checkbox);
	_r_fastlock_initialize (&lock_logbusy);
	_r_fastlock_initialize (&lock_logthread);
	_r_fastlock_initialize (&lock_transaction);
	_r_fastlock_initialize (&lock_writelog);

	// set privileges
	{
		DWORD privileges[] = {
			SE_SECURITY_PRIVILEGE,
			SE_TAKE_OWNERSHIP_PRIVILEGE,
			SE_BACKUP_PRIVILEGE,
			SE_RESTORE_PRIVILEGE,
			SE_DEBUG_PRIVILEGE,
		};

		_r_sys_setprivilege (privileges, _countof (privileges), true);
	}

	// set process priority
	SetPriorityClass (NtCurrentProcess (), ABOVE_NORMAL_PRIORITY_CLASS);

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

		config.padminsid = (PISID)_r_mem_alloc (size);

		if (!CreateWellKnownSid (WinBuiltinAdministratorsSid, nullptr, config.padminsid, &size))
		{
			_r_mem_free (config.padminsid);
			config.padminsid = nullptr;
		}
	}

	// get current user security identifier
	if (_r_str_isempty (config.title))
	{
		// get user sid
		HANDLE htoken = nullptr;
		DWORD token_length = 0;
		PTOKEN_USER token_user = nullptr;

		if (OpenProcessToken (NtCurrentProcess (), TOKEN_QUERY, &htoken))
		{
			GetTokenInformation (htoken, TokenUser, nullptr, 0, &token_length);

			if (GetLastError () == ERROR_INSUFFICIENT_BUFFER)
			{
				token_user = (PTOKEN_USER)_r_mem_alloc (token_length);

				if (token_user)
				{
					if (GetTokenInformation (htoken, TokenUser, token_user, token_length, &token_length))
					{
						SID_NAME_USE sid_type;

						WCHAR username[MAX_PATH] = {0};
						WCHAR domain[MAX_PATH] = {0};

						DWORD length1 = _countof (username);
						DWORD length2 = _countof (domain);

						if (LookupAccountSid (nullptr, token_user->User.Sid, username, &length1, domain, &length2, &sid_type))
							_r_str_printf (config.title, _countof (config.title), L"%s [%s\\%s]", APP_NAME, domain, username);
					}

					_r_mem_free (token_user);
				}
			}

			CloseHandle (htoken);
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

		timers.push_back (_R_SECONDSCLOCK_MIN (2));
		timers.push_back (_R_SECONDSCLOCK_MIN (5));
		timers.push_back (_R_SECONDSCLOCK_MIN (10));
		timers.push_back (_R_SECONDSCLOCK_MIN (30));
		timers.push_back (_R_SECONDSCLOCK_HOUR (1));
		timers.push_back (_R_SECONDSCLOCK_HOUR (2));
		timers.push_back (_R_SECONDSCLOCK_HOUR (4));
		timers.push_back (_R_SECONDSCLOCK_HOUR (6));
	}

	// initialize thread objects
	if (!config.done_evt)
		config.done_evt = CreateEventEx (nullptr, nullptr, 0, EVENT_ALL_ACCESS);
}

INT FirstDriveFromMask (DWORD unitmask)
{
	INT i;

	for (i = 0; i < _R_DEVICE_COUNT; ++i)
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

		if ((lpfr->Flags & FR_DIALOGTERM) != 0)
		{
			config.hfind = nullptr;
		}
		else if ((lpfr->Flags & FR_FINDNEXT) != 0)
		{
			const INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

			if (!listview_id)
				return FALSE;

			const INT total_count = _r_listview_getitemcount (hwnd, listview_id);

			if (!total_count)
				return FALSE;

			bool is_wrap = true;

			const INT selected_item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)INVALID_INT, LVNI_SELECTED);

			INT current_item = selected_item + 1;
			INT last_item = total_count;

find_wrap:

			for (; current_item < last_item; current_item++)
			{
				const rstring item_text = _r_listview_getitemtext (hwnd, listview_id, current_item, 0);

				if (item_text.IsEmpty ())
					continue;

				if (StrStrNIW (item_text, lpfr->lpstrFindWhat, (UINT)item_text.GetLength ()) != nullptr)
				{
					_app_showitem (hwnd, listview_id, current_item);
					return FALSE;
				}
			}

			if (is_wrap)
			{
				is_wrap = false;

				current_item = 0;
				last_item = (std::min) (selected_item + 1, total_count);

				goto find_wrap;
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

			// initialize settings
			app.SettingsAddPage (IDD_SETTINGS_GENERAL, IDS_SETTINGS_GENERAL);
			app.SettingsAddPage (IDD_SETTINGS_INTERFACE, IDS_TITLE_INTERFACE);
			app.SettingsAddPage (IDD_SETTINGS_HIGHLIGHTING, IDS_TITLE_HIGHLIGHTING);
			app.SettingsAddPage (IDD_SETTINGS_RULES, IDS_TRAY_RULES);
			app.SettingsAddPage (IDD_SETTINGS_BLOCKLIST, IDS_TRAY_BLOCKLIST_RULES);

			// dropped packets logging (win7+)
			app.SettingsAddPage (IDD_SETTINGS_NOTIFICATIONS, IDS_TITLE_NOTIFICATIONS);
			app.SettingsAddPage (IDD_SETTINGS_LOGGING, IDS_TITLE_LOGGING);

			// initialize colors
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

			// restore window size and position (required!)
			app.RestoreWindowPosition (hwnd, L"window");

			// initialize imagelist
			_app_imagelist_init (hwnd);

			// initialize toolbar
			_app_toolbar_init (hwnd);
			_app_toolbar_resize ();

			// initialize tabs
			_app_tabs_init (hwnd);

#ifndef _APP_NO_DARKTHEME
			_r_wnd_setdarktheme (hwnd);
#endif // _APP_NO_DARKTHEME

			// load profile
			_app_profile_load (hwnd);

			// add blocklist to update
			if (!app.ConfigGet (L"IsInternalRulesDisabled", false).AsBool ())
				app.UpdateAddComponent (L"Internal rules", L"profile_internal", _r_fmt (L"%" PRIi64, config.profile_internal_timestamp), config.profile_internal_path, false);

			// initialize tab
			_app_settab_id (hwnd, app.ConfigGet (L"CurrentTab", IDC_APPS_PROFILE).AsInt ());

			// initialize dropped packets log callback thread (win7+)
			RtlSecureZeroMemory (&log_stack, sizeof (log_stack));
			RtlInitializeSListHead (&log_stack.ListHead);

			// create notification window
			_app_notifycreatewindow (hwnd);

			// create network monitor thread
			_r_createthread (&NetworkMonitorThread, (LPVOID)hwnd, false, THREAD_PRIORITY_LOWEST);

			// install filters
			if (_wfp_isfiltersinstalled ())
			{
				//if (app.ConfigGet (L"IsDisableWindowsFirewallChecked", true).AsBool ())
				//	_mps_changeconfig2 (false);

				_app_changefilters (hwnd, true, true);
			}
			else
			{
				_r_status_settext (hwnd, IDC_STATUSBAR, 0, app.LocaleString (IDS_STATUS_FILTERS_INACTIVE, nullptr));
			}

			// set column size when "auto-size" option are disabled
			if (!app.ConfigGet (L"AutoSizeColumns", true).AsBool ())
			{
				for (INT i = 0; i < (INT)SendDlgItemMessage (hwnd, IDC_TAB, TCM_GETITEMCOUNT, 0, 0); i++)
				{
					const INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, i);

					if (listview_id)
						_app_listviewresize (hwnd, listview_id, true);
				}
			}

			break;
		}

		case WM_NCCREATE:
		{
			_r_wnd_enablenonclientscaling (hwnd);
			break;
		}

		case RM_INITIALIZE:
		{
			if (app.ConfigGet (L"IsShowTitleID", true).AsBool ())
				SetWindowText (hwnd, config.title);

			else
				SetWindowText (hwnd, APP_NAME);

			_r_tray_create (hwnd, UID, WM_TRAYICON, app.GetSharedImage (app.GetHINSTANCE (), _wfp_isfiltersinstalled () ? IDI_ACTIVE : IDI_INACTIVE, _r_dc_getsystemmetrics (hwnd, SM_CXSMICON)), APP_NAME, false);

			const HMENU hmenu = GetMenu (hwnd);

			if (hmenu)
			{
				_r_menu_checkitem (hmenu, IDM_ALWAYSONTOP_CHK, 0, MF_BYCOMMAND, app.ConfigGet (L"AlwaysOnTop", _APP_ALWAYSONTOP).AsBool ());
				_r_menu_checkitem (hmenu, IDM_SHOWFILENAMESONLY_CHK, 0, MF_BYCOMMAND, app.ConfigGet (L"ShowFilenames", true).AsBool ());
				_r_menu_checkitem (hmenu, IDM_AUTOSIZECOLUMNS_CHK, 0, MF_BYCOMMAND, app.ConfigGet (L"AutoSizeColumns", true).AsBool ());
				_r_menu_checkitem (hmenu, IDM_ENABLESPECIALGROUP_CHK, 0, MF_BYCOMMAND, app.ConfigGet (L"IsEnableSpecialGroup", true).AsBool ());

				{
					UINT menu_id;
					const INT view_type = std::clamp (app.ConfigGet (L"ViewType", LV_VIEW_DETAILS).AsInt (), LV_VIEW_ICON, LV_VIEW_MAX);

					if (view_type == LV_VIEW_ICON)
						menu_id = IDM_VIEW_ICON;

					else if (view_type == LV_VIEW_TILE)
						menu_id = IDM_VIEW_TILE;

					else
						menu_id = IDM_VIEW_DETAILS;

					_r_menu_checkitem (hmenu, IDM_VIEW_ICON, IDM_VIEW_TILE, MF_BYCOMMAND, menu_id);
				}

				{
					UINT menu_id;
					const INT icon_size = std::clamp (app.ConfigGet (L"IconSize", SHIL_SYSSMALL).AsInt (), SHIL_LARGE, SHIL_LAST);

					if (icon_size == SHIL_EXTRALARGE)
						menu_id = IDM_SIZE_EXTRALARGE;

					else if (icon_size == SHIL_LARGE)
						menu_id = IDM_SIZE_LARGE;

					else
						menu_id = IDM_SIZE_SMALL;

					_r_menu_checkitem (hmenu, IDM_SIZE_SMALL, IDM_SIZE_EXTRALARGE, MF_BYCOMMAND, menu_id);
				}

				_r_menu_checkitem (hmenu, IDM_ICONSISHIDDEN, 0, MF_BYCOMMAND, app.ConfigGet (L"IsIconsHidden", false).AsBool ());

				_r_menu_checkitem (hmenu, IDM_RULE_BLOCKOUTBOUND, 0, MF_BYCOMMAND, app.ConfigGet (L"BlockOutboundConnections", true).AsBool ());
				_r_menu_checkitem (hmenu, IDM_RULE_BLOCKINBOUND, 0, MF_BYCOMMAND, app.ConfigGet (L"BlockInboundConnections", true).AsBool ());
				_r_menu_checkitem (hmenu, IDM_RULE_ALLOWLOOPBACK, 0, MF_BYCOMMAND, app.ConfigGet (L"AllowLoopbackConnections", true).AsBool ());
				_r_menu_checkitem (hmenu, IDM_RULE_ALLOW6TO4, 0, MF_BYCOMMAND, app.ConfigGet (L"AllowIPv6", true).AsBool ());

				_r_menu_checkitem (hmenu, IDM_SECUREFILTERS_CHK, 0, MF_BYCOMMAND, app.ConfigGet (L"IsSecureFilters", true).AsBool ());
				_r_menu_checkitem (hmenu, IDM_USESTEALTHMODE_CHK, 0, MF_BYCOMMAND, app.ConfigGet (L"UseStealthMode", true).AsBool ());
				_r_menu_checkitem (hmenu, IDM_INSTALLBOOTTIMEFILTERS_CHK, 0, MF_BYCOMMAND, app.ConfigGet (L"InstallBoottimeFilters", true).AsBool ());

				_r_menu_checkitem (hmenu, IDM_USECERTIFICATES_CHK, 0, MF_BYCOMMAND, app.ConfigGet (L"IsCertificatesEnabled", false).AsBool ());
				_r_menu_checkitem (hmenu, IDM_USENETWORKRESOLUTION_CHK, 0, MF_BYCOMMAND, app.ConfigGet (L"IsNetworkResolutionsEnabled", false).AsBool ());
				_r_menu_checkitem (hmenu, IDM_USEREFRESHDEVICES_CHK, 0, MF_BYCOMMAND, app.ConfigGet (L"IsRefreshDevices", true).AsBool ());

				_r_menu_checkitem (hmenu, IDM_BLOCKLIST_SPY_DISABLE, IDM_BLOCKLIST_SPY_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_SPY_DISABLE + std::clamp (app.ConfigGet (L"BlocklistSpyState", 2).AsInt (), 0, 2));
				_r_menu_checkitem (hmenu, IDM_BLOCKLIST_UPDATE_DISABLE, IDM_BLOCKLIST_UPDATE_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_UPDATE_DISABLE + std::clamp (app.ConfigGet (L"BlocklistUpdateState", 0).AsInt (), 0, 2));
				_r_menu_checkitem (hmenu, IDM_BLOCKLIST_EXTRA_DISABLE, IDM_BLOCKLIST_EXTRA_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_EXTRA_DISABLE + std::clamp (app.ConfigGet (L"BlocklistExtraState", 0).AsInt (), 0, 2));
			}

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLENOTIFICATIONS_CHK, nullptr, 0, app.ConfigGet (L"IsNotificationsEnabled", true).AsBool () ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED);
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLELOG_CHK, nullptr, 0, app.ConfigGet (L"IsLogEnabled", false).AsBool () ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED);

			_app_setinterfacestate (hwnd);

			break;
		}

		case RM_UNINITIALIZE:
		{
			_r_tray_destroy (hwnd, UID);
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
			app.LocaleMenu (hmenu, IDS_ICONSSMALL, IDM_SIZE_SMALL, false, nullptr);
			app.LocaleMenu (hmenu, IDS_ICONSLARGE, IDM_SIZE_LARGE, false, nullptr);
			app.LocaleMenu (hmenu, IDS_ICONSEXTRALARGE, IDM_SIZE_EXTRALARGE, false, nullptr);

			app.LocaleMenu (hmenu, IDS_VIEW_ICON, IDM_VIEW_ICON, false, nullptr);
			app.LocaleMenu (hmenu, IDS_VIEW_DETAILS, IDM_VIEW_DETAILS, false, nullptr);
			app.LocaleMenu (hmenu, IDS_VIEW_TILE, IDM_VIEW_TILE, false, nullptr);

			app.LocaleMenu (hmenu, IDS_ICONSISHIDDEN, IDM_ICONSISHIDDEN, false, nullptr);

			app.LocaleMenu (GetSubMenu (hmenu, 2), IDS_LANGUAGE, LANG_MENU, true, L" (Language)");

			app.LocaleMenu (hmenu, IDS_FONT, IDM_FONT, false, L"...");

			const rstring recommended = app.LocaleString (IDS_RECOMMENDED, nullptr);

			app.LocaleMenu (hmenu, IDS_TRAY_RULES, 3, true, nullptr);

			app.LocaleMenu (hmenu, IDS_TAB_NETWORK, IDM_CONNECTIONS_TITLE, false, nullptr);
			app.LocaleMenu (hmenu, IDS_TITLE_SECURITY, IDM_SECURITY_TITLE, false, nullptr);
			app.LocaleMenu (hmenu, IDS_TITLE_ADVANCED, IDM_ADVANCED_TITLE, false, nullptr);

			_r_menu_enableitem (hmenu, IDM_CONNECTIONS_TITLE, MF_BYCOMMAND, false);
			_r_menu_enableitem (hmenu, IDM_SECURITY_TITLE, MF_BYCOMMAND, false);
			_r_menu_enableitem (hmenu, IDM_ADVANCED_TITLE, MF_BYCOMMAND, false);

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

			_r_menu_enableitem (hmenu, IDM_BLOCKLIST_SPY_TITLE, MF_BYCOMMAND, false);
			_r_menu_enableitem (hmenu, IDM_BLOCKLIST_UPDATE_TITLE, MF_BYCOMMAND, false);
			_r_menu_enableitem (hmenu, IDM_BLOCKLIST_EXTRA_TITLE, MF_BYCOMMAND, false);

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
				const INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, i);

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

			const INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

			if (listview_id)
			{
				_app_listviewresize (hwnd, listview_id, false);
				_app_refreshstatus (hwnd, listview_id);
			}

			// refresh notification
			_r_wnd_addstyle (config.hnotification, IDC_RULES_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (config.hnotification, IDC_ALLOW_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (config.hnotification, IDC_BLOCK_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (config.hnotification, IDC_LATER_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

			_app_notifyrefresh (config.hnotification, false);

			break;
		}

		case RM_TASKBARCREATED:
		{
			// refresh tray icon
			_r_tray_destroy (hwnd, UID);
			_r_tray_create (hwnd, UID, WM_TRAYICON, app.GetSharedImage (app.GetHINSTANCE (), _wfp_isfiltersinstalled () ? IDI_ACTIVE : IDI_INACTIVE, _r_dc_getsystemmetrics (hwnd, SM_CXSMICON)), APP_NAME, false);

			break;
		}

		case RM_DPICHANGED:
		{
			_app_imagelist_init (hwnd);

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

			const INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

			if (listview_id)
			{
				_app_listviewsetview (hwnd, listview_id);
				_app_listviewsetfont (hwnd, listview_id, true);
				_app_listviewresize (hwnd, listview_id, false);
			}

			_app_refreshstatus (hwnd, 0);

			break;
		}

		case RM_CONFIG_UPDATE:
		{
			_app_profile_save ();
			_app_profile_load (hwnd);

			_app_refreshstatus (hwnd);

			_app_changefilters (hwnd, true, false);

			break;
		}

		case RM_CONFIG_RESET:
		{
			const time_t current_timestamp = (time_t)lparam;

			_app_freeobjects_map (rules_config, true);

			_r_fs_makebackup (config.profile_path, current_timestamp);

			_r_fs_remove (config.profile_path, RFS_FORCEREMOVE);
			_r_fs_remove (config.profile_path_backup, RFS_FORCEREMOVE);

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
					!app.ShowConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION_TIMER, nullptr), L"ConfirmExitTimer") :
					!app.ShowConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION_EXIT, nullptr), L"ConfirmExit2"))
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
			app.ConfigSet (L"CurrentTab", (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT));

			if (config.hnotification)
				DestroyWindow (config.hnotification);

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

				_r_fastlock_acquireshared (&lock_access);
				app_hash = _app_addapplication (hwnd, file, 0, 0, 0, false, false);
				_r_fastlock_releaseshared (&lock_access);

				SAFE_DELETE_ARRAY (file);
			}

			DragFinish ((HDROP)wparam);

			_app_profile_save ();
			_app_refreshstatus (hwnd);

			{
				INT app_listview_id = 0;

				if (_app_getappinfo (app_hash, InfoListviewId, &app_listview_id, sizeof (app_listview_id)))
				{
					if (app_listview_id == _r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT))
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
				case TCN_SELCHANGING:
				{
					const INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

					if (!listview_id)
						break;

					const HWND hlistview = GetDlgItem (hwnd, listview_id);

					if (!hlistview)
						break;

					ShowWindow (hlistview, SW_HIDE);

					break;
				}

				case TCN_SELCHANGE:
				{
					const INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

					if (!listview_id)
						break;

					const HWND hlistview = GetDlgItem (hwnd, listview_id);

					if (!hlistview)
						break;

					_r_tab_adjustchild (hwnd, IDC_TAB, hlistview);

					_app_listviewsort (hwnd, listview_id);
					_app_listviewsetview (hwnd, listview_id);
					_app_listviewsetfont (hwnd, listview_id, false);

					_app_listviewresize (hwnd, listview_id, false);
					_app_refreshstatus (hwnd, listview_id);

					_r_listview_redraw (hwnd, listview_id);

					ShowWindow (hlistview, SW_SHOWNA);

					if (IsWindowVisible (hwnd) && !IsIconic (hwnd)) // HACK!!!
						SetFocus (hlistview);

					break;
				}

				case NM_CUSTOMDRAW:
				{
					LONG result = CDRF_DODEFAULT;
					LPNMLVCUSTOMDRAW lpnmcd = (LPNMLVCUSTOMDRAW)lparam;

					if (static_cast<INT>(lpnmcd->nmcd.hdr.idFrom) == IDC_TOOLBAR)
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
					_app_listviewsort (hwnd, static_cast<INT>(lpnmlv->hdr.idFrom), lpnmlv->iSubItem, true);

					break;
				}

				case LVN_GETINFOTIP:
				{
					if (_r_fastlock_tryacquireshared (&lock_access))
					{
						LPNMLVGETINFOTIP lpnmlv = (LPNMLVGETINFOTIP)lparam;

						const INT listview_id = static_cast<INT>(lpnmlv->hdr.idFrom);
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

						const INT listview_id = static_cast<INT>(lpnmlv->hdr.idFrom);
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
							_app_refreshstatus (hwnd, listview_id);

							_app_profile_save ();
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
					const INT ctrl_id = static_cast<INT>(lpnmlv->hdr.idFrom);

					if (ctrl_id >= IDC_APPS_PROFILE && ctrl_id <= IDC_APPS_UWP)
					{
						command_id = IDM_EXPLORE;
					}
					else if (ctrl_id >= IDC_RULES_BLOCKLIST && ctrl_id <= IDC_NETWORK)
					{
						command_id = IDM_PROPERTIES;
					}
					else if (ctrl_id == IDC_STATUSBAR)
					{
						LPNMMOUSE nmouse = (LPNMMOUSE)lparam;

						if (nmouse->dwItemSpec == 0)
							command_id = IDM_SELECT_ALL;

						else if (nmouse->dwItemSpec == 1)
							command_id = IDM_PURGE_UNUSED;

						else if (nmouse->dwItemSpec == 2)
							command_id = IDM_PURGE_TIMERS;
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

					const INT listview_id = static_cast<INT>(lpnmlv->hdr.idFrom);

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
					const INT lv_column_current = lpnmlv->iSubItem;

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
								_r_menu_checkitem (hsubmenu, IDM_DISABLENOTIFICATIONS, 0, MF_BYCOMMAND, ptr_app->is_silent);
						}

						// show rules
						_app_generate_rulesmenu (hsubmenu_rules, hash_item);

						// show timers
						{
							bool is_checked = false;

							UINT index = 0;

							for (auto &timer : timers)
							{
								MENUITEMINFO mii = {0};

								WCHAR buffer[128] = {0};
								_r_str_copy (buffer, _countof (buffer), _r_fmt_interval (timer + 1, 1));

								mii.cbSize = sizeof (mii);
								mii.fMask = MIIM_ID | MIIM_FTYPE | MIIM_STATE | MIIM_STRING;
								mii.fType = MFT_STRING;
								mii.dwTypeData = buffer;
								mii.fState = MF_ENABLED;
								mii.wID = IDX_TIMER + index;

								InsertMenuItem (hsubmenu_timer, mii.wID, FALSE, &mii);

								if (ptr_app)
								{
									if (!is_checked && ptr_app->timer > current_time && ptr_app->timer <= (current_time + timer))
									{
										_r_menu_checkitem (hsubmenu_timer, IDX_TIMER, IDX_TIMER + UINT (timers.size ()), MF_BYCOMMAND, mii.wID);
										is_checked = true;
									}
								}

								index += 1;
							}

							if (!is_checked)
								_r_menu_checkitem (hsubmenu_timer, IDM_DISABLETIMER, IDM_DISABLETIMER, MF_BYCOMMAND, IDM_DISABLETIMER);
						}

						_r_obj_dereference (ptr_app_object);

						if (listview_id != IDC_APPS_PROFILE)
							_r_menu_enableitem (hsubmenu, IDM_DELETE, MF_BYCOMMAND, false);
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
								_r_menu_enableitem (hsubmenu, IDM_DELETE, MF_BYCOMMAND, false);

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
									_r_menu_enableitem (hsubmenu, IDM_NETWORK_CLOSE, MF_BYCOMMAND, false);

								if (!ptr_network->app_hash || !ptr_network->path)
									_r_menu_enableitem (hsubmenu, IDM_EXPLORE, MF_BYCOMMAND, false);
							}

							_r_obj_dereference (ptr_network_object);
						}
					}

					const INT command_id = _r_menu_popup (hsubmenu, hwnd, nullptr, false);

					DestroyMenu (hmenu);

					if (command_id)
						PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (command_id, 0), (LPARAM)lv_column_current);

					break;
				}
			}

			break;
		}

		case WM_SIZE:
		{
			_app_resizewindow (hwnd, lparam);
			break;
		}

		case WM_TRAYICON:
		{
			switch (LOWORD (lparam))
			{
				case NIN_POPUPOPEN:
				{
					PR_OBJECT ptr_log_object = _app_notifyget_obj (_app_notifyget_id (config.hnotification, false));

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

				case WM_CONTEXTMENU:
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
							_r_menu_enableitem (hsubmenu, IDM_TRAY_LOGSHOW, MF_BYCOMMAND, false);
							_r_menu_enableitem (hsubmenu, IDM_TRAY_LOGCLEAR, MF_BYCOMMAND, false);
						}
					}

					if (_r_fs_exists (app.GetLogPath ()))
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

					_r_menu_checkitem (hsubmenu, IDM_TRAY_ENABLENOTIFICATIONS_CHK, 0, MF_BYCOMMAND, app.ConfigGet (L"IsNotificationsEnabled", true).AsBool ());
					_r_menu_checkitem (hsubmenu, IDM_TRAY_ENABLENOTIFICATIONSSOUND_CHK, 0, MF_BYCOMMAND, app.ConfigGet (L"IsNotificationsSound", true).AsBool ());
					_r_menu_checkitem (hsubmenu, IDM_TRAY_NOTIFICATIONONTRAY_CHK, 0, MF_BYCOMMAND, app.ConfigGet (L"IsNotificationsOnTray", false).AsBool ());

					_r_menu_checkitem (hsubmenu, IDM_TRAY_ENABLELOG_CHK, 0, MF_BYCOMMAND, app.ConfigGet (L"IsLogEnabled", false).AsBool ());

					if (!app.ConfigGet (L"IsNotificationsEnabled", true).AsBool ())
					{
						_r_menu_enableitem (hsubmenu, IDM_TRAY_ENABLENOTIFICATIONSSOUND_CHK, MF_BYCOMMAND, false);
						_r_menu_enableitem (hsubmenu, IDM_TRAY_NOTIFICATIONONTRAY_CHK, MF_BYCOMMAND, false);
					}

					if (_wfp_isfiltersapplying ())
						_r_menu_enableitem (hsubmenu, IDM_TRAY_START, MF_BYCOMMAND, false);

					_r_menu_popup (hsubmenu, hwnd, nullptr, true);

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
					if (_wfp_isfiltersinstalled () && !_wfp_getenginehandle ())
						_wfp_initialize (true);

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
									_r_listview_redraw (hwnd, (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT));
							}
						}
						else if (wparam == DBT_DEVICEREMOVECOMPLETE)
						{
							if (IsWindowVisible (hwnd))
								_r_listview_redraw (hwnd, (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT));
						}
					}

					break;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			const INT ctrl_id = LOWORD (wparam);

			if (HIWORD (wparam) == 0 && ctrl_id >= IDX_LANGUAGE && ctrl_id <= INT (IDX_LANGUAGE + app.LocaleGetCount ()))
			{
				app.LocaleApplyFromMenu (GetSubMenu (GetSubMenu (GetMenu (hwnd), 2), LANG_MENU), ctrl_id, IDX_LANGUAGE);
				return FALSE;
			}
			else if ((ctrl_id >= IDX_RULES_SPECIAL && ctrl_id <= INT (IDX_RULES_SPECIAL + rules_arr.size ())))
			{
				const INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

				if (!SendDlgItemMessage (hwnd, listview_id, LVM_GETSELECTEDCOUNT, 0, 0))
					return FALSE;

				INT item = INVALID_INT;
				BOOL is_remove = INVALID_INT;

				const size_t rule_idx = (ctrl_id - IDX_RULES_SPECIAL);
				PR_OBJECT ptr_rule_object = _app_getrulebyid (rule_idx);

				if (!ptr_rule_object)
					return FALSE;

				PITEM_RULE ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

				if (ptr_rule)
				{
					while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
					{
						const size_t app_hash = _r_listview_getitemlparam (hwnd, listview_id, item);

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
							_app_setappiteminfo (hwnd, listview_id, item, app_hash, ptr_app);
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

				_app_listviewsort (hwnd, listview_id);
				_app_refreshstatus (hwnd, listview_id);

				_app_profile_save ();

				return FALSE;
			}
			else if ((ctrl_id >= IDX_TIMER && ctrl_id <= INT (IDX_TIMER + timers.size ())))
			{
				const INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

				if (!listview_id || !SendDlgItemMessage (hwnd, listview_id, LVM_GETSELECTEDCOUNT, 0, 0))
					return FALSE;

				const size_t timer_idx = (ctrl_id - IDX_TIMER);
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
				_app_refreshstatus (hwnd, listview_id);

				_app_profile_save ();

				return FALSE;
			}

			switch (ctrl_id)
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
					app.CreateSettingsWindow (hwnd, &SettingsProc);
					break;
				}

				case IDM_EXIT:
				case IDM_TRAY_EXIT:
				{
					PostMessage (hwnd, WM_CLOSE, 0, 0);
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
					app.UpdateCheck (hwnd);
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
							app.ShowErrorMessage (hwnd, L"Profile loading error!", ERROR_INVALID_DATA, nullptr);
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
					const bool new_val = !app.ConfigGet (L"AlwaysOnTop", _APP_ALWAYSONTOP).AsBool ();

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
					app.ConfigSet (L"AlwaysOnTop", new_val);

					_r_wnd_top (hwnd, new_val);

					break;
				}

				case IDM_AUTOSIZECOLUMNS_CHK:
				{
					const bool new_val = !app.ConfigGet (L"AutoSizeColumns", true).AsBool ();

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
					app.ConfigSet (L"AutoSizeColumns", new_val);

					if (new_val)
						_app_listviewresize (hwnd, (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT), false);

					break;
				}

				case IDM_SHOWFILENAMESONLY_CHK:
				{
					const bool new_val = !app.ConfigGet (L"ShowFilenames", true).AsBool ();

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
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

					_app_listviewsort (hwnd, (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT));

					break;
				}

				case IDM_ENABLESPECIALGROUP_CHK:
				{
					const bool new_val = !app.ConfigGet (L"IsEnableSpecialGroup", true).AsBool ();

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
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

					_r_fastlock_releaseshared (&lock_access);

					const INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

					if (listview_id)
					{
						_app_listviewsort (hwnd, listview_id);
						_app_refreshstatus (hwnd, listview_id);
					}

					break;
				}

				case IDM_VIEW_ICON:
				case IDM_VIEW_DETAILS:
				case IDM_VIEW_TILE:
				{
					INT view_type;

					if (ctrl_id == IDM_VIEW_ICON)
						view_type = LV_VIEW_ICON;

					else if (ctrl_id == IDM_VIEW_TILE)
						view_type = LV_VIEW_TILE;

					else
						view_type = LV_VIEW_DETAILS;

					_r_menu_checkitem (GetMenu (hwnd), IDM_VIEW_ICON, IDM_VIEW_TILE, MF_BYCOMMAND, ctrl_id);
					app.ConfigSet (L"ViewType", view_type);

					const INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

					if (listview_id)
					{
						_app_listviewsetview (hwnd, listview_id);
						_app_listviewresize (hwnd, listview_id, false);

						_r_listview_redraw (hwnd, listview_id);
					}

					break;
				}

				case IDM_SIZE_SMALL:
				case IDM_SIZE_LARGE:
				case IDM_SIZE_EXTRALARGE:
				{
					INT icon_size;

					if (ctrl_id == IDM_SIZE_LARGE)
						icon_size = SHIL_LARGE;

					else if (ctrl_id == IDM_SIZE_EXTRALARGE)
						icon_size = SHIL_EXTRALARGE;

					else
						icon_size = SHIL_SYSSMALL;

					_r_menu_checkitem (GetMenu (hwnd), IDM_SIZE_SMALL, IDM_SIZE_EXTRALARGE, MF_BYCOMMAND, ctrl_id);
					app.ConfigSet (L"IconSize", icon_size);

					const INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

					if (listview_id)
					{
						_app_listviewsetview (hwnd, listview_id);
						_app_listviewresize (hwnd, listview_id, false);

						_r_listview_redraw (hwnd, listview_id);
					}

					break;
				}

				case IDM_ICONSISHIDDEN:
				{
					const bool new_val = !app.ConfigGet (L"IsIconsHidden", false).AsBool ();

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
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

					_app_listviewinitfont (hwnd, &lf);

					if (ChooseFont (&cf))
					{
						app.ConfigSet (L"Font", !_r_str_isempty (lf.lfFaceName) ? _r_fmt (L"%s;%d;%d", lf.lfFaceName, _r_dc_fontheighttosize (hwnd, lf.lfHeight), lf.lfWeight) : UI_FONT_DEFAULT);

						SAFE_DELETE_OBJECT (config.hfont);

						INT current_page = (INT)SendDlgItemMessage (hwnd, IDC_TAB, TCM_GETCURSEL, 0, 0);

						for (INT i = 0; i < (INT)SendDlgItemMessage (hwnd, IDC_TAB, TCM_GETITEMCOUNT, 0, 0); i++)
						{
							const INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, i);

							if (listview_id)
							{
								_app_listviewsetfont (hwnd, listview_id, false);

								if (i == current_page)
									_app_listviewresize (hwnd, listview_id, false);
							}
						}


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
						PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDM_FIND, 0), 0);
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
					_app_config_apply (hwnd, ctrl_id);
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
					const HMENU hmenu = GetMenu (hwnd);

					if (ctrl_id >= IDM_BLOCKLIST_SPY_DISABLE && ctrl_id <= IDM_BLOCKLIST_SPY_BLOCK)
					{
						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_SPY_DISABLE, IDM_BLOCKLIST_SPY_BLOCK, MF_BYCOMMAND, ctrl_id);

						const INT new_state = std::clamp (ctrl_id - IDM_BLOCKLIST_SPY_DISABLE, 0, 2);

						app.ConfigSet (L"BlocklistSpyState", new_state);

						_r_fastlock_acquireshared (&lock_access);
						_app_ruleblocklistset (hwnd, new_state, INVALID_INT, INVALID_INT, true);
						_r_fastlock_releaseshared (&lock_access);
					}
					else if (ctrl_id >= IDM_BLOCKLIST_UPDATE_DISABLE && ctrl_id <= IDM_BLOCKLIST_UPDATE_BLOCK)
					{
						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_UPDATE_DISABLE, IDM_BLOCKLIST_UPDATE_BLOCK, MF_BYCOMMAND, ctrl_id);

						const INT new_state = std::clamp (ctrl_id - IDM_BLOCKLIST_UPDATE_DISABLE, 0, 2);

						app.ConfigSet (L"BlocklistUpdateState", new_state);

						_r_fastlock_acquireshared (&lock_access);
						_app_ruleblocklistset (hwnd, INVALID_INT, new_state, INVALID_INT, true);
						_r_fastlock_releaseshared (&lock_access);
					}
					else if (ctrl_id >= IDM_BLOCKLIST_EXTRA_DISABLE && ctrl_id <= IDM_BLOCKLIST_EXTRA_BLOCK)
					{
						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_EXTRA_DISABLE, IDM_BLOCKLIST_EXTRA_BLOCK, MF_BYCOMMAND, ctrl_id);

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

					_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, ctrl_id, nullptr, 0, new_val ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED);
					app.ConfigSet (L"IsLogEnabled", new_val);

					_app_loginit (new_val);

					break;
				}

				case IDM_TRAY_ENABLENOTIFICATIONS_CHK:
				{
					const bool new_val = !app.ConfigGet (L"IsNotificationsEnabled", true).AsBool ();

					_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, ctrl_id, nullptr, 0, new_val ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED);
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

					if (_r_fs_isvalidhandle (config.hlogfile))
						FlushFileBuffers (config.hlogfile);

					_r_run (nullptr, _r_fmt (L"%s \"%s\"", _app_getlogviewer ().GetString (), path.GetString ()));

					break;
				}

				case IDM_TRAY_LOGCLEAR:
				{
					const rstring path = _r_path_expand (app.ConfigGet (L"LogPath", LOG_PATH_DEFAULT));

					const bool is_validhandle = _r_fs_isvalidhandle (config.hlogfile);

					if ((is_validhandle && _r_fs_size (config.hlogfile) != (LONG64)((_r_str_length (SZ_LOG_TITLE) + 1) * sizeof (WCHAR))) || (!is_validhandle && _r_fs_exists (path)))
					{
						if (!app.ShowConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION, nullptr), L"ConfirmLogClear"))
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
					LPCWSTR path = app.GetLogPath ();

					if (_r_fs_exists (path))
						_r_run (nullptr, _r_fmt (L"%s \"%s\"", _app_getlogviewer ().GetString (), path));

					break;
				}

				case IDM_TRAY_LOGCLEAR_ERR:
				{
					if (!app.ShowConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION, nullptr), L"ConfirmLogClear"))
						break;

					LPCWSTR path = app.GetLogPath ();

					if (!_r_fs_exists (path))
						break;

					_r_fs_remove (path, RFS_FORCEREMOVE);

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
							_r_fastlock_acquireshared (&lock_access);
							app_hash = _app_addapplication (hwnd, files, 0, 0, 0, false, false);
							_r_fastlock_releaseshared (&lock_access);
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
									_r_fastlock_acquireshared (&lock_access);
									app_hash = _app_addapplication (hwnd, _r_fmt (L"%s\\%s", dir, p), 0, 0, 0, false, false);
									_r_fastlock_releaseshared (&lock_access);
								}
							}
						}

						{
							INT app_listview_id = 0;

							if (_app_getappinfo (app_hash, InfoListviewId, &app_listview_id, sizeof (app_listview_id)))
							{
								if (app_listview_id == (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT))
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
					const INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

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
						_app_refreshstatus (hwnd, listview_id);

						_app_profile_save ();
					}

					break;
				}

				case IDM_COPY:
				case IDM_COPY2:
				{
					const INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);
					INT item = INVALID_INT;

					const INT column_count = _r_listview_getcolumncount (hwnd, listview_id);
					const INT column_current = (INT)lparam;
					const rstring divider = _r_fmt (L"%c ", DIVIDER_COPY);

					rstring buffer;

					while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
					{
						if (ctrl_id == IDM_COPY)
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
					const INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

					const bool new_val = (ctrl_id == IDM_CHECK) ? true : false;

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
						_app_refreshstatus (hwnd, listview_id);

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

					const INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

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
								if (_r_str_isempty (ptr_rule->pname))
								{
									const rstring item_text = _r_listview_getitemtext (hwnd, listview_id, item, 0);

									_r_str_alloc (&ptr_rule->pname, item_text.GetLength (), item_text);
								}

								if (ptr_network->app_hash && !_r_str_isempty (ptr_network->path))
								{
									if (!_app_isappfound (ptr_network->app_hash))
									{
										_r_fastlock_acquireexclusive (&lock_access);
										_app_addapplication (hwnd, ptr_network->path, 0, 0, 0, false, false);
										_r_fastlock_releaseexclusive (&lock_access);

										_app_refreshstatus (hwnd, listview_id);
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
						_r_fastlock_acquireshared (&lock_access);

						const size_t rule_idx = rules_arr.size ();
						rules_arr.push_back (ptr_rule_object);

						_r_fastlock_releaseshared (&lock_access);

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
						_app_refreshstatus (hwnd, listview_id);

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
					const INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);
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
							_app_refreshstatus (hwnd, listview_id);

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
								_app_addapplication (hwnd, ptr_network->path, 0, 0, 0, false, false);
								_r_fastlock_releaseshared (&lock_access);

								_app_refreshstatus (hwnd, listview_id);
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
					const INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

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
								RtlSecureZeroMemory (&tcprow, sizeof (tcprow));

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
					const INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

					if (listview_id != IDC_APPS_PROFILE && listview_id != IDC_RULES_CUSTOM)
						break;

					const INT selected = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETSELECTEDCOUNT, 0, 0);

					if (!selected || app.ShowMessage (hwnd, MB_YESNO | MB_ICONEXCLAMATION, nullptr, nullptr, _r_fmt (app.LocaleString (IDS_QUESTION_DELETE, nullptr), selected)) != IDYES)
						break;

					const INT count = _r_listview_getitemcount (hwnd, listview_id) - 1;

					GUIDS_VEC guids;

					for (INT i = count; i != INVALID_INT; i--)
					{
						if ((UINT)SendDlgItemMessage (hwnd, listview_id, LVM_GETITEMSTATE, i, LVNI_SELECTED) == LVNI_SELECTED)
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

									_r_fastlock_acquireshared (&lock_access);
									_app_freeapplication (app_hash);
									_r_fastlock_releaseshared (&lock_access);

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

					for (auto &p : apps_list)
						_app_freeapplication (p);

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
					if (!_app_istimersactive () || app.ShowMessage (hwnd, MB_YESNO | MB_ICONEXCLAMATION, nullptr, nullptr, app.LocaleString (IDS_QUESTION_TIMERS, nullptr)) != IDYES)
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

					_app_profile_save ();

					const INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

					if (listview_id)
					{
						_app_listviewsort (hwnd, listview_id);
						_app_refreshstatus (hwnd, listview_id);
					}

					break;
				}

				case IDM_SELECT_ALL:
				{
					ListView_SetItemState (GetDlgItem (hwnd, (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT)), INVALID_INT, LVIS_SELECTED, LVIS_SELECTED);
					break;
				}

				case IDM_ZOOM:
				{
					ShowWindow (hwnd, IsZoomed (hwnd) ? SW_RESTORE : SW_MAXIMIZE);
					break;
				}

#if defined(_DEBUG) || defined(_APP_BETA)

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

					_r_str_alloc (&ptr_log->path, _r_str_length (app.GetBinaryPath ()), app.GetBinaryPath ());
					_r_str_alloc (&ptr_log->filter_name, _r_str_length (FN_AD), FN_AD);

					_app_formataddress (ptr_log->af, ptr_log->protocol, &ptr_log->remote_addr, 0, &ptr_log->remote_fmt, FMTADDR_USE_PROTOCOL | FMTADDR_RESOLVE_HOST);
					//_app_formataddress (ptr_log->af, ptr_log->protocol, &ptr_log->local_addr, 0, &ptr_log->local_fmt, FMTADDR_USE_PROTOCOL | FMTADDR_RESOLVE_HOST);

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

						_wfp_logcallback (flags, &ft, (UINT8*)path.GetString (), nullptr, (SID*)config.padminsid, IPPROTO_TCP, FWP_IP_VERSION_V4, remote_addr, nullptr, RP_AD, local_addr, nullptr, LP_AD, layer_id, filter_id, FWP_DIRECTION_OUTBOUND, false, false);
					}

					break;
				}
#endif // _DEBUG || _APP_BETA
			}

			break;
		}
	}

	return FALSE;
}

INT APIENTRY wWinMain (HINSTANCE, HINSTANCE, LPWSTR, INT)
{
	MSG msg = {0};

	if (app.Initialize (APP_NAME, APP_NAME_SHORT, APP_VERSION, APP_COPYRIGHT))
	{
		// parse arguments
		{
			INT numargs = 0;
			LPWSTR* arga = CommandLineToArgvW (GetCommandLine (), &numargs);

			if (arga)
			{
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

				if ((is_install || is_uninstall) && _r_sys_iselevated ())
				{
					_app_initialize ();

					if (is_install)
					{
						if (is_silent || (!_wfp_isfiltersinstalled () && _app_installmessage (nullptr, true)))
						{
							_app_profile_load (nullptr);

							if (_wfp_initialize (true))
								_wfp_installfilters ();

							_wfp_uninitialize (false);
						}
					}
					else if (is_uninstall)
					{
						if (is_silent || (_wfp_isfiltersinstalled () && _app_installmessage (nullptr, false)))
						{
							if (_wfp_initialize (false))
								_wfp_destroyfilters (_wfp_getenginehandle ());

							_wfp_uninitialize (true);
						}
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
					HWND hwnd = GetActiveWindow ();

					if (!TranslateAccelerator (hwnd, haccel, &msg) && !IsDialogMessage (hwnd, &msg))
					{
						TranslateMessage (&msg);
						DispatchMessage (&msg);
					}
				}

				DestroyAcceleratorTable (haccel);
			}
		}
	}

	return (INT)msg.wParam;
}
