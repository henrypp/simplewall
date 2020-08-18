// simplewall
// Copyright (c) 2016-2020 Henry++

#include "global.hpp"

UINT WM_FINDMSGSTRING = RegisterWindowMessage (FINDMSGSTRING);

THREAD_FN ApplyThread (PVOID lparam)
{
	PITEM_CONTEXT pcontext = (PITEM_CONTEXT)lparam;

	_r_fastlock_acquireshared (&lock_apply);

	HANDLE hengine = _wfp_getenginehandle ();

	if (hengine)
	{
		// dropped packets logging (win7+)
		if (config.is_neteventset)
			_wfp_logunsubscribe (hengine);

		if (pcontext->is_install)
		{
			if (_wfp_initialize (hengine, TRUE))
				_wfp_installfilters (hengine);
		}
		else
		{
			_wfp_destroyfilters (hengine);
			_wfp_uninitialize (hengine, TRUE);
		}

		// dropped packets logging (win7+)
		if (config.is_neteventset)
			_wfp_logsubscribe (hengine);
	}

	_app_restoreinterfacestate (pcontext->hwnd, TRUE);
	_app_setinterfacestate (pcontext->hwnd);

	_app_profile_save ();

	SetEvent (config.done_evt);

	_r_mem_free (pcontext);

	_r_fastlock_releaseshared (&lock_apply);

	return _r_sys_endthread (ERROR_SUCCESS);
}

THREAD_FN NetworkMonitorThread (PVOID lparam)
{
	DWORD network_timeout = _r_config_getulong (L"NetworkTimeout", NETWORK_TIMEOUT);

	if (network_timeout && network_timeout != INFINITE)
	{
		HASHER_MAP checker_map;
		PITEM_NETWORK ptr_network;
		PR_STRING localAddressString;
		PR_STRING localPortString;
		PR_STRING remoteAddressString;
		PR_STRING remotePortString;
		HWND hwnd;
		INT network_listview_id;
		INT current_listview_id;
		INT item_id;
		BOOLEAN is_highlighting_enabled;
		BOOLEAN is_refresh;

		hwnd = (HWND)lparam;
		network_listview_id = IDC_NETWORK;

		network_timeout = _r_calc_clamp (DWORD, network_timeout, 500, 60 * 1000); // set allowed range

		while (TRUE)
		{
			_app_generate_connections (&network_map, &checker_map);

			is_highlighting_enabled = _r_config_getboolean (L"IsEnableHighlighting", TRUE) && _r_config_getboolean (L"IsHighlightConnection", TRUE, L"colors");
			current_listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);
			is_refresh = FALSE;

			// add new connections into list
			for (auto it = network_map.begin (); it != network_map.end (); ++it)
			{
				if (!it->second)
					continue;

				if (checker_map.find (it->first) == checker_map.end () || !checker_map.at (it->first))
					continue;

				ptr_network = (PITEM_NETWORK)_r_obj_reference (it->second);

				localAddressString = _app_formataddress (ptr_network->af, 0, &ptr_network->local_addr, 0, 0);
				localPortString = _app_formatport (ptr_network->local_port, TRUE);
				remoteAddressString = _app_formataddress (ptr_network->af, 0, &ptr_network->remote_addr, 0, 0);
				remotePortString = _app_formatport (ptr_network->remote_port, TRUE);

				item_id = _r_listview_getitemcount (hwnd, network_listview_id, FALSE);

				_r_listview_additemex (hwnd, network_listview_id, item_id, 0, _r_path_getbasename (_r_obj_getstring (ptr_network->path)), ptr_network->icon_id, _app_getnetworkgroup (ptr_network), it->first);

				_r_listview_setitem (hwnd, network_listview_id, item_id, 1, _r_obj_getstringordefault (localAddressString, SZ_EMPTY));
				_r_listview_setitem (hwnd, network_listview_id, item_id, 3, _r_obj_getstringordefault (remoteAddressString, SZ_EMPTY));
				_r_listview_setitem (hwnd, network_listview_id, item_id, 5, _app_getprotoname (ptr_network->protocol, ptr_network->af, SZ_EMPTY));
				_r_listview_setitem (hwnd, network_listview_id, item_id, 6, _app_getconnectionstatusname (ptr_network->state, NULL));

				if (localPortString)
					_r_listview_setitem (hwnd, network_listview_id, item_id, 2, _r_obj_getstringorempty (localPortString));

				if (remotePortString)
					_r_listview_setitem (hwnd, network_listview_id, item_id, 4, _r_obj_getstringorempty (remotePortString));

				SAFE_DELETE_REFERENCE (localAddressString);
				SAFE_DELETE_REFERENCE (localPortString);
				SAFE_DELETE_REFERENCE (remoteAddressString);
				SAFE_DELETE_REFERENCE (remotePortString);

				// redraw listview item
				if (is_highlighting_enabled)
				{
					INT app_listview_id = (INT)_app_getappinfo (ptr_network->app_hash, InfoListviewId);

					if (app_listview_id && current_listview_id == app_listview_id)
					{
						INT item_pos = _app_getposition (hwnd, app_listview_id, ptr_network->app_hash);

						if (item_pos != INVALID_INT)
							_r_listview_redraw (hwnd, app_listview_id, item_pos);
					}
				}

				is_refresh = TRUE;

				_r_obj_dereference (ptr_network);
			}

			// refresh network tab as well
			if (is_refresh)
			{
				if (current_listview_id == network_listview_id)
				{
					SendDlgItemMessage (hwnd, network_listview_id, WM_SETREDRAW, FALSE, 0);

					_app_refreshgroups (hwnd, network_listview_id);

					_app_listviewsort (hwnd, network_listview_id, INVALID_INT, FALSE);
					_app_listviewresize (hwnd, network_listview_id, FALSE);

					SendDlgItemMessage (hwnd, network_listview_id, WM_SETREDRAW, TRUE, 0);
				}
			}

			// remove closed connections from list
			INT item_count = _r_listview_getitemcount (hwnd, network_listview_id, FALSE);

			if (item_count)
			{
				for (INT i = item_count - 1; i != INVALID_INT; i--)
				{
					SIZE_T network_hash = _r_listview_getitemlparam (hwnd, network_listview_id, i);

					if (checker_map.find (network_hash) != checker_map.end ())
						continue;

					SendDlgItemMessage (hwnd, network_listview_id, LVM_DELETEITEM, (WPARAM)i, 0);

					ptr_network = _app_getnetworkitem (network_hash);

					// redraw listview item
					if (ptr_network)
					{
						if (is_highlighting_enabled)
						{
							INT app_listview_id = (INT)_app_getappinfo (ptr_network->app_hash, InfoListviewId);

							if (app_listview_id && current_listview_id == app_listview_id)
							{
								INT item_pos = _app_getposition (hwnd, app_listview_id, ptr_network->app_hash);

								if (item_pos != INVALID_INT)
									_r_listview_redraw (hwnd, app_listview_id, item_pos);
							}
						}
					}

					network_map.erase (network_hash);

					_r_obj_dereferenceex (ptr_network, 2);
				}
			}

			WaitForSingleObjectEx (NtCurrentThread (), network_timeout, FALSE);
		}
	}

	return _r_sys_endthread (ERROR_SUCCESS);
}

BOOLEAN _app_changefilters (HWND hwnd, BOOLEAN is_install, BOOLEAN is_forced)
{
	if (_wfp_isfiltersapplying ())
		return FALSE;

	INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

	_app_listviewsort (hwnd, listview_id, INVALID_INT, FALSE);

	if (is_forced || _wfp_isfiltersinstalled ())
	{
		_app_initinterfacestate (hwnd, TRUE);

		_app_freethreadpool (&threads_pool);

		PITEM_CONTEXT pcontext = (PITEM_CONTEXT)_r_mem_allocatezero (sizeof (ITEM_CONTEXT));

		pcontext->hwnd = hwnd;
		pcontext->is_install = is_install;

		HANDLE hthread = _r_sys_createthreadex (&ApplyThread, (PVOID)pcontext, TRUE, THREAD_PRIORITY_HIGHEST);

		if (!hthread)
		{
			_r_mem_free (pcontext);
			return FALSE;
		}

		threads_pool.emplace_back (hthread);
		_r_sys_resumethread (hthread);

		return TRUE;
	}

	_app_profile_save ();

	_r_listview_redraw (hwnd, listview_id, INVALID_INT);

	return FALSE;
}

VOID addcolor (UINT locale_id, LPCWSTR configName, BOOLEAN is_enabled, LPCWSTR configValue, COLORREF default_clr)
{
	PITEM_COLOR ptr_clr = (PITEM_COLOR)_r_mem_allocatezero (sizeof (ITEM_COLOR));

	if (!_r_str_isempty (configName))
		ptr_clr->configName = _r_obj_createstring (configName);

	if (!_r_str_isempty (configValue))
	{
		ptr_clr->configValue = _r_obj_createstring (configValue);

		ptr_clr->hash = _r_str_hash (configValue);
		ptr_clr->new_clr = _r_config_getulong (configValue, default_clr, L"colors");
	}

	ptr_clr->default_clr = default_clr;
	ptr_clr->locale_id = locale_id;
	ptr_clr->is_enabled = is_enabled;

	colors.emplace_back (ptr_clr);
}

BOOLEAN _app_installmessage (HWND hwnd, BOOLEAN is_install)
{
	WCHAR radio_text_1[128];
	WCHAR radio_text_2[128];

	WCHAR str_main[256];

	WCHAR str_flag[128];

	WCHAR str_button_text_1[64];
	WCHAR str_button_text_2[64];

	TASKDIALOGCONFIG tdc = {0};

	TASKDIALOG_BUTTON td_buttons[2];
	TASKDIALOG_BUTTON td_radios[2];

	tdc.cbSize = sizeof (tdc);
	tdc.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_NO_SET_FOREGROUND;
	tdc.hwndParent = hwnd;
	tdc.pszWindowTitle = APP_NAME;
	tdc.pszMainIcon = is_install ? TD_INFORMATION_ICON : TD_WARNING_ICON;
	//tdc.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
	tdc.pszContent = str_main;
	tdc.pszVerificationText = str_flag;
	tdc.pfCallback = &_r_msg_callback;
	tdc.lpCallbackData = MAKELONG (0, TRUE); // on top

	tdc.pButtons = td_buttons;
	tdc.cButtons = RTL_NUMBER_OF (td_buttons);

	tdc.nDefaultButton = IDYES;

	td_buttons[0].nButtonID = IDYES;
	td_buttons[0].pszButtonText = str_button_text_1;

	td_buttons[1].nButtonID = IDNO;
	td_buttons[1].pszButtonText = str_button_text_2;

	_r_str_copy (str_button_text_1, RTL_NUMBER_OF (str_button_text_1), _r_locale_getstring (is_install ? IDS_TRAY_START : IDS_TRAY_STOP));
	_r_str_copy (str_button_text_2, RTL_NUMBER_OF (str_button_text_2), _r_locale_getstring (IDS_CLOSE));

	if (is_install)
	{
		_r_str_copy (str_main, RTL_NUMBER_OF (str_main), _r_locale_getstring (IDS_QUESTION_START));
		_r_str_copy (str_flag, RTL_NUMBER_OF (str_flag), _r_locale_getstring (IDS_DISABLEWINDOWSFIREWALL_CHK));

		tdc.pRadioButtons = td_radios;
		tdc.cRadioButtons = RTL_NUMBER_OF (td_radios);

		tdc.nDefaultRadioButton = IDYES;

		td_radios[0].nButtonID = IDYES;
		td_radios[0].pszButtonText = radio_text_1;

		td_radios[1].nButtonID = IDNO;
		td_radios[1].pszButtonText = radio_text_2;

		_r_str_copy (radio_text_1, RTL_NUMBER_OF (radio_text_1), _r_locale_getstring (IDS_INSTALL_PERMANENT));
		_r_str_copy (radio_text_2, RTL_NUMBER_OF (radio_text_2), _r_locale_getstring (IDS_INSTALL_TEMPORARY));

		if (_r_config_getboolean (L"IsDisableWindowsFirewallChecked", TRUE))
			tdc.dwFlags |= TDF_VERIFICATION_FLAG_CHECKED;
	}
	else
	{
		_r_str_copy (str_main, RTL_NUMBER_OF (str_main), _r_locale_getstring (IDS_QUESTION_STOP));
		_r_str_copy (str_flag, RTL_NUMBER_OF (str_flag), _r_locale_getstring (IDS_ENABLEWINDOWSFIREWALL_CHK));

		if (_r_config_getboolean (L"IsEnableWindowsFirewallChecked", TRUE))
			tdc.dwFlags |= TDF_VERIFICATION_FLAG_CHECKED;
	}

	INT command_id;
	INT radio_id;
	BOOL is_flagchecked;

	if (_r_msg_taskdialog (&tdc, &command_id, &radio_id, &is_flagchecked))
	{
		if (command_id == IDYES)
		{
			if (is_install)
			{
				config.is_filterstemporary = (radio_id == IDNO);

				_r_config_setboolean (L"IsDisableWindowsFirewallChecked", !!is_flagchecked);

				if (is_flagchecked)
					_mps_changeconfig2 (FALSE);
			}
			else
			{
				_r_config_setboolean (L"IsEnableWindowsFirewallChecked", !!is_flagchecked);

				if (is_flagchecked)
					_mps_changeconfig2 (TRUE);
			}

			return TRUE;
		}
	}

	return FALSE;
}

VOID _app_config_apply (HWND hwnd, INT ctrl_id)
{
	BOOLEAN new_val;

	switch (ctrl_id)
	{
		case IDC_RULE_BLOCKOUTBOUND:
		case IDM_RULE_BLOCKOUTBOUND:
		{
			new_val = !_r_config_getboolean (L"BlockOutboundConnections", TRUE);
			break;
		}

		case IDC_RULE_BLOCKINBOUND:
		case IDM_RULE_BLOCKINBOUND:
		{
			new_val = !_r_config_getboolean (L"BlockInboundConnections", TRUE);
			break;
		}

		case IDC_RULE_ALLOWLOOPBACK:
		case IDM_RULE_ALLOWLOOPBACK:
		{
			new_val = !_r_config_getboolean (L"AllowLoopbackConnections", TRUE);
			break;
		}

		case IDC_RULE_ALLOW6TO4:
		case IDM_RULE_ALLOW6TO4:
		{
			new_val = !_r_config_getboolean (L"AllowIPv6", TRUE);
			break;
		}

		case IDC_SECUREFILTERS_CHK:
		case IDM_SECUREFILTERS_CHK:
		{
			new_val = !_r_config_getboolean (L"IsSecureFilters", TRUE);
			break;
		}

		case IDC_USESTEALTHMODE_CHK:
		case IDM_USESTEALTHMODE_CHK:
		{
			new_val = !_r_config_getboolean (L"UseStealthMode", TRUE);
			break;
		}

		case IDC_INSTALLBOOTTIMEFILTERS_CHK:
		case IDM_INSTALLBOOTTIMEFILTERS_CHK:
		{
			new_val = !_r_config_getboolean (L"InstallBoottimeFilters", TRUE);
			break;
		}

		case IDC_USENETWORKRESOLUTION_CHK:
		case IDM_USENETWORKRESOLUTION_CHK:
		{
			new_val = !_r_config_getboolean (L"IsNetworkResolutionsEnabled", FALSE);
			break;
		}

		case IDC_USECERTIFICATES_CHK:
		case IDM_USECERTIFICATES_CHK:
		{
			new_val = !_r_config_getboolean (L"IsCertificatesEnabled", FALSE);
			break;
		}

		case IDC_USEREFRESHDEVICES_CHK:
		case IDM_USEREFRESHDEVICES_CHK:
		{
			new_val = !_r_config_getboolean (L"IsRefreshDevices", TRUE);
			break;
		}

		default:
		{
			return;
		}
	}

	HMENU hmenu = GetMenu (hwnd);

	switch (ctrl_id)
	{
		case IDC_RULE_BLOCKOUTBOUND:
		case IDM_RULE_BLOCKOUTBOUND:
		{
			_r_config_setboolean (L"BlockOutboundConnections", new_val);
			_r_menu_checkitem (hmenu, IDM_RULE_BLOCKOUTBOUND, 0, MF_BYCOMMAND, new_val);

			break;
		}

		case IDC_RULE_BLOCKINBOUND:
		case IDM_RULE_BLOCKINBOUND:
		{
			_r_config_setboolean (L"BlockInboundConnections", new_val);
			_r_menu_checkitem (hmenu, IDM_RULE_BLOCKINBOUND, 0, MF_BYCOMMAND, new_val);

			break;
		}

		case IDC_RULE_ALLOWLOOPBACK:
		case IDM_RULE_ALLOWLOOPBACK:
		{
			_r_config_setboolean (L"AllowLoopbackConnections", new_val);
			_r_menu_checkitem (hmenu, IDM_RULE_ALLOWLOOPBACK, 0, MF_BYCOMMAND, new_val);

			break;
		}

		case IDC_RULE_ALLOW6TO4:
		case IDM_RULE_ALLOW6TO4:
		{
			_r_config_setboolean (L"AllowIPv6", new_val);
			_r_menu_checkitem (hmenu, IDM_RULE_ALLOW6TO4, 0, MF_BYCOMMAND, new_val);

			break;
		}

		case IDC_USESTEALTHMODE_CHK:
		case IDM_USESTEALTHMODE_CHK:
		{
			_r_config_setboolean (L"UseStealthMode", new_val);
			_r_menu_checkitem (hmenu, IDM_USESTEALTHMODE_CHK, 0, MF_BYCOMMAND, new_val);

			break;
		}

		case IDC_SECUREFILTERS_CHK:
		case IDM_SECUREFILTERS_CHK:
		{
			_r_config_setboolean (L"IsSecureFilters", new_val);
			_r_menu_checkitem (hmenu, IDM_SECUREFILTERS_CHK, 0, MF_BYCOMMAND, new_val);

			if (_wfp_isfiltersinstalled ())
			{
				HANDLE hengine = _wfp_getenginehandle ();

				if (hengine)
				{
					GUIDS_VEC filter_all;

					_app_setsecurityinfoforprovider (hengine, &GUID_WfpProvider, new_val);
					_app_setsecurityinfoforsublayer (hengine, &GUID_WfpSublayer, new_val);

					if (_wfp_dumpfilters (hengine, &GUID_WfpProvider, &filter_all))
					{
						for (auto it = filter_all.begin (); it != filter_all.end (); ++it)
							_app_setsecurityinfoforfilter (hengine, &(*it), new_val, __LINE__);
					}
				}
			}

			break;
		}

		case IDC_INSTALLBOOTTIMEFILTERS_CHK:
		case IDM_INSTALLBOOTTIMEFILTERS_CHK:
		{
			_r_config_setboolean (L"InstallBoottimeFilters", new_val);
			_r_menu_checkitem (hmenu, IDM_INSTALLBOOTTIMEFILTERS_CHK, 0, MF_BYCOMMAND, new_val);

			break;
		}

		case IDC_USENETWORKRESOLUTION_CHK:
		case IDM_USENETWORKRESOLUTION_CHK:
		{
			_r_config_setboolean (L"IsNetworkResolutionsEnabled", new_val);
			_r_menu_checkitem (hmenu, IDM_USENETWORKRESOLUTION_CHK, 0, MF_BYCOMMAND, new_val);

			break;
		}

		case IDC_USECERTIFICATES_CHK:
		case IDM_USECERTIFICATES_CHK:
		{
			_r_config_setboolean (L"IsCertificatesEnabled", new_val);
			_r_menu_checkitem (hmenu, IDM_USECERTIFICATES_CHK, 0, MF_BYCOMMAND, new_val);

			if (new_val)
			{
				PR_STRING signatureString;
				PITEM_APP ptr_app;
				SIZE_T app_hash;

				for (auto it = apps.begin (); it != apps.end (); ++it)
				{
					if (!it->second)
						continue;

					app_hash = it->first;
					ptr_app = (PITEM_APP)_r_obj_reference (it->second);

					signatureString = _app_getsignatureinfo (app_hash, ptr_app);

					SAFE_DELETE_REFERENCE (signatureString);

					_r_obj_dereference (ptr_app);
				}
			}

			_r_listview_redraw (_r_app_gethwnd (), (INT)_r_tab_getlparam (_r_app_gethwnd (), IDC_TAB, INVALID_INT), INVALID_INT);

			break;
		}

		case IDC_USEREFRESHDEVICES_CHK:
		case IDM_USEREFRESHDEVICES_CHK:
		{
			_r_config_setboolean (L"IsRefreshDevices", new_val);
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

	if (_wfp_isfiltersinstalled ())
	{
		HANDLE hengine = _wfp_getenginehandle ();

		if (hengine)
			_wfp_create2filters (hengine, __LINE__);
	}
}

INT_PTR CALLBACK SettingsProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
		case WM_INITDIALOG:
		{
#if defined(_APP_HAVE_DARKTHEME)
			_r_wnd_setdarktheme (hwnd);
#endif // _APP_HAVE_DARKTHEME

			break;
		}

		case RM_INITIALIZE:
		{
			INT dialog_id = (INT)wparam;

			switch (dialog_id)
			{
				case IDD_SETTINGS_GENERAL:
				{
					CheckDlgButton (hwnd, IDC_ALWAYSONTOP_CHK, _r_config_getboolean (L"AlwaysOnTop", _APP_ALWAYSONTOP) ? BST_CHECKED : BST_UNCHECKED);

#if defined(_APP_HAVE_AUTORUN)
					CheckDlgButton (hwnd, IDC_LOADONSTARTUP_CHK, _r_autorun_isenabled () ? BST_CHECKED : BST_UNCHECKED);
#endif // _APP_HAVE_AUTORUN

					CheckDlgButton (hwnd, IDC_STARTMINIMIZED_CHK, _r_config_getboolean (L"IsStartMinimized", FALSE) ? BST_CHECKED : BST_UNCHECKED);

#if defined(_APP_HAVE_SKIPUAC)
					CheckDlgButton (hwnd, IDC_SKIPUACWARNING_CHK, _r_skipuac_isenabled () ? BST_CHECKED : BST_UNCHECKED);
#endif // _APP_HAVE_SKIPUAC

					CheckDlgButton (hwnd, IDC_CHECKUPDATES_CHK, _r_config_getboolean (L"CheckUpdates", TRUE) ? BST_CHECKED : BST_UNCHECKED);

#if defined(_DEBUG) || defined(_APP_BETA)
					CheckDlgButton (hwnd, IDC_CHECKUPDATESBETA_CHK, BST_CHECKED);
					_r_ctrl_enable (hwnd, IDC_CHECKUPDATESBETA_CHK, FALSE);
#else
					CheckDlgButton (hwnd, IDC_CHECKUPDATESBETA_CHK, _r_config_getboolean (L"CheckUpdatesBeta", FALSE) ? BST_CHECKED : BST_UNCHECKED);

					if (!_r_config_getboolean (L"CheckUpdates", TRUE))
						_r_ctrl_enable (hwnd, IDC_CHECKUPDATESBETA_CHK, FALSE);
#endif // _DEBUG || _APP_BETA

					_r_locale_enum (hwnd, IDC_LANGUAGE, 0);

					break;
				}

				case IDD_SETTINGS_RULES:
				{
					CheckDlgButton (hwnd, IDC_RULE_BLOCKOUTBOUND, _r_config_getboolean (L"BlockOutboundConnections", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_RULE_BLOCKINBOUND, _r_config_getboolean (L"BlockInboundConnections", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_RULE_ALLOWLOOPBACK, _r_config_getboolean (L"AllowLoopbackConnections", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_RULE_ALLOW6TO4, _r_config_getboolean (L"AllowIPv6", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_SECUREFILTERS_CHK, _r_config_getboolean (L"IsSecureFilters", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_USESTEALTHMODE_CHK, _r_config_getboolean (L"UseStealthMode", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_INSTALLBOOTTIMEFILTERS_CHK, _r_config_getboolean (L"InstallBoottimeFilters", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_USECERTIFICATES_CHK, _r_config_getboolean (L"IsCertificatesEnabled", FALSE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_USENETWORKRESOLUTION_CHK, _r_config_getboolean (L"IsNetworkResolutionsEnabled", FALSE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_USEREFRESHDEVICES_CHK, _r_config_getboolean (L"IsRefreshDevices", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					HWND htip = _r_ctrl_createtip (hwnd);

					if (htip)
					{
						_r_ctrl_settiptext (htip, hwnd, IDC_RULE_BLOCKOUTBOUND, LPSTR_TEXTCALLBACK);
						_r_ctrl_settiptext (htip, hwnd, IDC_RULE_BLOCKINBOUND, LPSTR_TEXTCALLBACK);
						_r_ctrl_settiptext (htip, hwnd, IDC_RULE_ALLOWLOOPBACK, LPSTR_TEXTCALLBACK);
						_r_ctrl_settiptext (htip, hwnd, IDC_RULE_ALLOW6TO4, LPSTR_TEXTCALLBACK);

						_r_ctrl_settiptext (htip, hwnd, IDC_USESTEALTHMODE_CHK, LPSTR_TEXTCALLBACK);
						_r_ctrl_settiptext (htip, hwnd, IDC_INSTALLBOOTTIMEFILTERS_CHK, LPSTR_TEXTCALLBACK);
						_r_ctrl_settiptext (htip, hwnd, IDC_SECUREFILTERS_CHK, LPSTR_TEXTCALLBACK);
					}

					break;
				}

				case IDD_SETTINGS_BLOCKLIST:
				{
					for (INT i = IDC_BLOCKLIST_SPY_DISABLE; i <= IDC_BLOCKLIST_EXTRA_BLOCK; i++)
						CheckDlgButton (hwnd, i, BST_UNCHECKED); // HACK!!! reset button checkboxes!

					CheckDlgButton (hwnd, (IDC_BLOCKLIST_SPY_DISABLE + _r_calc_clamp (INT, _r_config_getinteger (L"BlocklistSpyState", 2), 0, 2)), BST_CHECKED);
					CheckDlgButton (hwnd, (IDC_BLOCKLIST_UPDATE_DISABLE + _r_calc_clamp (INT, _r_config_getinteger (L"BlocklistUpdateState", 0), 0, 2)), BST_CHECKED);
					CheckDlgButton (hwnd, (IDC_BLOCKLIST_EXTRA_DISABLE + _r_calc_clamp (INT, _r_config_getinteger (L"BlocklistExtraState", 0), 0, 2)), BST_CHECKED);

					break;
				}

				case IDD_SETTINGS_INTERFACE:
				{
					CheckDlgButton (hwnd, IDC_CONFIRMEXIT_CHK, _r_config_getboolean (L"ConfirmExit2", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_CONFIRMEXITTIMER_CHK, _r_config_getboolean (L"ConfirmExitTimer", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_CONFIRMLOGCLEAR_CHK, _r_config_getboolean (L"ConfirmLogClear", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					break;
				}

				case IDD_SETTINGS_HIGHLIGHTING:
				{
					// configure listview
					_r_listview_setstyle (hwnd, IDC_COLORS, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_CHECKBOXES, FALSE);

					_app_listviewsetview (hwnd, IDC_COLORS);

					_r_listview_deleteallitems (hwnd, IDC_COLORS);
					_r_listview_deleteallcolumns (hwnd, IDC_COLORS);

					_r_listview_addcolumn (hwnd, IDC_COLORS, 0, L"", 0, LVCFMT_LEFT);

					INT item = 0;
					PITEM_COLOR ptr_clr;

					for (auto it = colors.begin (); it != colors.end (); ++it)
					{
						ptr_clr = *it;
						ptr_clr->new_clr = _r_config_getulong (_r_obj_getstring (ptr_clr->configValue), ptr_clr->default_clr, L"colors");

						_r_fastlock_acquireshared (&lock_checkbox);

						_r_listview_additemex (hwnd, IDC_COLORS, item, 0, _r_locale_getstring (ptr_clr->locale_id), config.icon_id, I_GROUPIDNONE, (LPARAM)ptr_clr);
						_r_listview_setitemcheck (hwnd, IDC_COLORS, item, _r_config_getboolean (_r_obj_getstring (ptr_clr->configName), ptr_clr->is_enabled, L"colors"));

						_r_fastlock_releaseshared (&lock_checkbox);

						item += 1;
					}

					break;
				}

				case IDD_SETTINGS_NOTIFICATIONS:
				{
					CheckDlgButton (hwnd, IDC_ENABLENOTIFICATIONS_CHK, _r_config_getboolean (L"IsNotificationsEnabled", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_NOTIFICATIONSOUND_CHK, _r_config_getboolean (L"IsNotificationsSound", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_NOTIFICATIONFULLSCREENSILENTMODE_CHK, _r_config_getboolean (L"IsNotificationsFullscreenSilentMode", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_NOTIFICATIONONTRAY_CHK, _r_config_getboolean (L"IsNotificationsOnTray", FALSE) ? BST_CHECKED : BST_UNCHECKED);

					SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_SETRANGE32, 0, _r_calc_days2seconds (LPARAM, 7));
					SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_SETPOS32, 0, (LPARAM)_r_config_getulong (L"NotificationsTimeout", NOTIFY_TIMEOUT_DEFAULT));

					CheckDlgButton (hwnd, IDC_EXCLUDEBLOCKLIST_CHK, _r_config_getboolean (L"IsExcludeBlocklist", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_EXCLUDECUSTOM_CHK, _r_config_getboolean (L"IsExcludeCustomRules", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDC_ENABLENOTIFICATIONS_CHK, 0), 0);

					break;
				}

				case IDD_SETTINGS_LOGGING:
				{
					CheckDlgButton (hwnd, IDC_ENABLELOG_CHK, _r_config_getboolean (L"IsLogEnabled", FALSE) ? BST_CHECKED : BST_UNCHECKED);

					_r_ctrl_settext (hwnd, IDC_LOGPATH, _r_config_getstring (L"LogPath", LOG_PATH_DEFAULT));
					_r_ctrl_settext (hwnd, IDC_LOGVIEWER, _r_config_getstring (L"LogViewer", LOG_VIEWER_DEFAULT));

					UDACCEL ud = {0};
					ud.nInc = 64; // set step to 64kb

					SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_SETACCEL, 1, (LPARAM)&ud);
					SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_SETRANGE32, 64, _r_calc_kilobytes2bytes (LPARAM, 512));
					SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_SETPOS32, 0, (LPARAM)_r_config_getulong (L"LogSizeLimitKb", LOG_SIZE_LIMIT_DEFAULT));

					CheckDlgButton (hwnd, IDC_ENABLEUILOG_CHK, _r_config_getboolean (L"IsLogUiEnabled", FALSE) ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_EXCLUDESTEALTH_CHK, _r_config_getboolean (L"IsExcludeStealth", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, _r_config_getboolean (L"IsExcludeClassifyAllow", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					// win8+
					if (!_r_sys_isosversiongreaterorequal (WINDOWS_8))
						_r_ctrl_enable (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, FALSE);

					PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDC_ENABLELOG_CHK, 0), 0);

					break;
				}

				break;
			}

			break;
		}

		case RM_LOCALIZE:
		{
			INT dialog_id = (INT)wparam;

			// localize titles
			_r_ctrl_settextformat (hwnd, IDC_TITLE_GENERAL, L"%s:", _r_locale_getstring (IDS_TITLE_GENERAL));
			_r_ctrl_settextformat (hwnd, IDC_TITLE_LANGUAGE, L"%s: (Language)", _r_locale_getstring (IDS_TITLE_LANGUAGE));
			_r_ctrl_settextformat (hwnd, IDC_TITLE_BLOCKLIST_SPY, L"%s:", _r_locale_getstring (IDS_BLOCKLIST_SPY));
			_r_ctrl_settextformat (hwnd, IDC_TITLE_BLOCKLIST_UPDATE, L"%s:", _r_locale_getstring (IDS_BLOCKLIST_UPDATE));
			_r_ctrl_settextformat (hwnd, IDC_TITLE_BLOCKLIST_EXTRA, L"%s: (Skype, Bing, Live, Outlook, etc.)", _r_locale_getstring (IDS_BLOCKLIST_EXTRA));
			_r_ctrl_settextformat (hwnd, IDC_TITLE_CONNECTIONS, L"%s:", _r_locale_getstring (IDS_TAB_NETWORK));
			_r_ctrl_settextformat (hwnd, IDC_TITLE_SECURITY, L"%s:", _r_locale_getstring (IDS_TITLE_SECURITY));
			_r_ctrl_settextformat (hwnd, IDC_TITLE_CONFIRMATIONS, L"%s:", _r_locale_getstring (IDS_TITLE_CONFIRMATIONS));
			_r_ctrl_settextformat (hwnd, IDC_TITLE_HIGHLIGHTING, L"%s:", _r_locale_getstring (IDS_TITLE_HIGHLIGHTING));
			_r_ctrl_settextformat (hwnd, IDC_TITLE_NOTIFICATIONS, L"%s:", _r_locale_getstring (IDS_TITLE_NOTIFICATIONS));
			_r_ctrl_settextformat (hwnd, IDC_TITLE_LOGGING, L"%s:", _r_locale_getstring (IDS_TITLE_LOGGING));
			_r_ctrl_settextformat (hwnd, IDC_TITLE_INTERFACE, L"%s:", _r_locale_getstring (IDS_TITLE_INTERFACE));
			_r_ctrl_settextformat (hwnd, IDC_TITLE_EXCLUDE, L"%s:", _r_locale_getstring (IDS_TITLE_EXCLUDE));
			_r_ctrl_settextformat (hwnd, IDC_TITLE_ADVANCED, L"%s:", _r_locale_getstring (IDS_TITLE_ADVANCED));

			switch (dialog_id)
			{
				case IDD_SETTINGS_GENERAL:
				{
					_r_ctrl_settext (hwnd, IDC_ALWAYSONTOP_CHK, _r_locale_getstring (IDS_ALWAYSONTOP_CHK));
					_r_ctrl_settext (hwnd, IDC_LOADONSTARTUP_CHK, _r_locale_getstring (IDS_LOADONSTARTUP_CHK));
					_r_ctrl_settext (hwnd, IDC_STARTMINIMIZED_CHK, _r_locale_getstring (IDS_STARTMINIMIZED_CHK));
					_r_ctrl_settext (hwnd, IDC_SKIPUACWARNING_CHK, _r_locale_getstring (IDS_SKIPUACWARNING_CHK));
					_r_ctrl_settext (hwnd, IDC_CHECKUPDATES_CHK, _r_locale_getstring (IDS_CHECKUPDATES_CHK));
					_r_ctrl_settext (hwnd, IDC_CHECKUPDATESBETA_CHK, _r_locale_getstring (IDS_CHECKUPDATESBETA_CHK));

					_r_ctrl_settext (hwnd, IDC_LANGUAGE_HINT, _r_locale_getstring (IDS_LANGUAGE_HINT));

					break;
				}

				case IDD_SETTINGS_RULES:
				{
					LPCWSTR recommendedString = _r_locale_getstring (IDS_RECOMMENDED);

					_r_ctrl_settextformat (hwnd, IDC_RULE_BLOCKOUTBOUND, L"%s (%s)", _r_locale_getstring (IDS_RULE_BLOCKOUTBOUND), recommendedString);
					_r_ctrl_settextformat (hwnd, IDC_RULE_BLOCKINBOUND, L"%s (%s)", _r_locale_getstring (IDS_RULE_BLOCKINBOUND), recommendedString);

					_r_ctrl_settextformat (hwnd, IDC_RULE_ALLOWLOOPBACK, L"%s (%s)", _r_locale_getstring (IDS_RULE_ALLOWLOOPBACK), recommendedString);
					_r_ctrl_settextformat (hwnd, IDC_RULE_ALLOW6TO4, L"%s (%s)", _r_locale_getstring (IDS_RULE_ALLOW6TO4), recommendedString);

					_r_ctrl_settextformat (hwnd, IDC_SECUREFILTERS_CHK, L"%s (%s)", _r_locale_getstring (IDS_SECUREFILTERS_CHK), recommendedString);
					_r_ctrl_settextformat (hwnd, IDC_USESTEALTHMODE_CHK, L"%s (%s)", _r_locale_getstring (IDS_USESTEALTHMODE_CHK), recommendedString);
					_r_ctrl_settextformat (hwnd, IDC_INSTALLBOOTTIMEFILTERS_CHK, L"%s (%s)", _r_locale_getstring (IDS_INSTALLBOOTTIMEFILTERS_CHK), recommendedString);

					_r_ctrl_settext (hwnd, IDC_USENETWORKRESOLUTION_CHK, _r_locale_getstring (IDS_USENETWORKRESOLUTION_CHK));
					_r_ctrl_settext (hwnd, IDC_USECERTIFICATES_CHK, _r_locale_getstring (IDS_USECERTIFICATES_CHK));
					_r_ctrl_settextformat (hwnd, IDC_USEREFRESHDEVICES_CHK, L"%s (%s)", _r_locale_getstring (IDS_USEREFRESHDEVICES_CHK), recommendedString);

					break;
				}

				case IDD_SETTINGS_BLOCKLIST:
				{
					LPCWSTR recommendedString = _r_locale_getstring (IDS_RECOMMENDED);

					LPCWSTR disableString = _r_locale_getstring (IDS_DISABLE);
					LPCWSTR allowString = _r_locale_getstring (IDS_ACTION_ALLOW);
					LPCWSTR blockString = _r_locale_getstring (IDS_ACTION_BLOCK);

					_r_ctrl_settext (hwnd, IDC_BLOCKLIST_SPY_DISABLE, disableString);
					_r_ctrl_settext (hwnd, IDC_BLOCKLIST_SPY_ALLOW, allowString);
					_r_ctrl_settextformat (hwnd, IDC_BLOCKLIST_SPY_BLOCK, L"%s (%s)", blockString, recommendedString);

					_r_ctrl_settextformat (hwnd, IDC_BLOCKLIST_UPDATE_DISABLE, L"%s (%s)", disableString, recommendedString);
					_r_ctrl_settext (hwnd, IDC_BLOCKLIST_UPDATE_ALLOW, allowString);
					_r_ctrl_settext (hwnd, IDC_BLOCKLIST_UPDATE_BLOCK, blockString);

					_r_ctrl_settextformat (hwnd, IDC_BLOCKLIST_EXTRA_DISABLE, L"%s (%s)", disableString, recommendedString);
					_r_ctrl_settext (hwnd, IDC_BLOCKLIST_EXTRA_ALLOW, allowString);
					_r_ctrl_settext (hwnd, IDC_BLOCKLIST_EXTRA_BLOCK, blockString);

					_r_ctrl_settextformat (hwnd, IDC_BLOCKLIST_INFO, L"Author: <a href=\"%s\">WindowsSpyBlocker</a> - block spying and tracking on Windows systems.", WINDOWSSPYBLOCKER_URL);

					break;
				}

				case IDD_SETTINGS_INTERFACE:
				{
					_r_ctrl_settext (hwnd, IDC_CONFIRMEXIT_CHK, _r_locale_getstring (IDS_CONFIRMEXIT_CHK));
					_r_ctrl_settext (hwnd, IDC_CONFIRMEXITTIMER_CHK, _r_locale_getstring (IDS_CONFIRMEXITTIMER_CHK));
					_r_ctrl_settext (hwnd, IDC_CONFIRMLOGCLEAR_CHK, _r_locale_getstring (IDS_CONFIRMLOGCLEAR_CHK));

					break;
				}

				case IDD_SETTINGS_HIGHLIGHTING:
				{
					_r_listview_setcolumn (hwnd, IDC_COLORS, 0, NULL, -100);

					for (INT i = 0; i < _r_listview_getitemcount (hwnd, IDC_COLORS, FALSE); i++)
					{
						PITEM_COLOR ptr_clr = (PITEM_COLOR)_r_listview_getitemlparam (hwnd, IDC_COLORS, i);

						if (ptr_clr)
						{
							_r_fastlock_acquireshared (&lock_checkbox);
							_r_listview_setitem (hwnd, IDC_COLORS, i, 0, _r_locale_getstring (ptr_clr->locale_id));
							_r_fastlock_releaseshared (&lock_checkbox);
						}
					}

					_r_ctrl_settext (hwnd, IDC_COLORS_HINT, _r_locale_getstring (IDS_COLORS_HINT));

					break;
				}

				case IDD_SETTINGS_NOTIFICATIONS:
				{
					_r_ctrl_settext (hwnd, IDC_ENABLENOTIFICATIONS_CHK, _r_locale_getstring (IDS_ENABLENOTIFICATIONS_CHK));
					_r_ctrl_settext (hwnd, IDC_NOTIFICATIONSOUND_CHK, _r_locale_getstring (IDS_NOTIFICATIONSOUND_CHK));
					_r_ctrl_settext (hwnd, IDC_NOTIFICATIONFULLSCREENSILENTMODE_CHK, _r_locale_getstring (IDS_NOTIFICATIONFULLSCREENSILENTMODE_CHK));
					_r_ctrl_settext (hwnd, IDC_NOTIFICATIONONTRAY_CHK, _r_locale_getstring (IDS_NOTIFICATIONONTRAY_CHK));

					_r_ctrl_settext (hwnd, IDC_NOTIFICATIONTIMEOUT_HINT, _r_locale_getstring (IDS_NOTIFICATIONTIMEOUT_HINT));

					_r_ctrl_settextformat (hwnd, IDC_EXCLUDEBLOCKLIST_CHK, L"%s %s", _r_locale_getstring (IDS_TITLE_EXCLUDE), _r_locale_getstring (IDS_EXCLUDEBLOCKLIST_CHK));
					_r_ctrl_settextformat (hwnd, IDC_EXCLUDECUSTOM_CHK, L"%s %s", _r_locale_getstring (IDS_TITLE_EXCLUDE), _r_locale_getstring (IDS_EXCLUDECUSTOM_CHK));

					break;
				}

				case IDD_SETTINGS_LOGGING:
				{
					_r_ctrl_settext (hwnd, IDC_ENABLELOG_CHK, _r_locale_getstring (IDS_ENABLELOG_CHK));

					_r_ctrl_settextformat (hwnd, IDC_LOGVIEWER_HINT, L"%s:", _r_locale_getstring (IDS_LOGVIEWER_HINT));
					_r_ctrl_settext (hwnd, IDC_LOGSIZELIMIT_HINT, _r_locale_getstring (IDS_LOGSIZELIMIT_HINT));

					_r_ctrl_settextformat (hwnd, IDC_ENABLEUILOG_CHK, L"%s (session only)", _r_locale_getstring (IDS_ENABLEUILOG_CHK));

					_r_ctrl_settextformat (hwnd, IDC_EXCLUDESTEALTH_CHK, L"%s %s", _r_locale_getstring (IDS_TITLE_EXCLUDE), _r_locale_getstring (IDS_EXCLUDESTEALTH_CHK));
					_r_ctrl_settextformat (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, L"%s %s [win8+]", _r_locale_getstring (IDS_TITLE_EXCLUDE), _r_locale_getstring (IDS_EXCLUDECLASSIFYALLOW_CHK));

					BOOLEAN is_classic = _r_app_isclassicui ();

					_r_wnd_addstyle (hwnd, IDC_LOGPATH_BTN, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
					_r_wnd_addstyle (hwnd, IDC_LOGVIEWER_BTN, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

					break;
				}
			}

			break;
		}

		case WM_VSCROLL:
		case WM_HSCROLL:
		{
			INT ctrl_id = GetDlgCtrlID ((HWND)lparam);

			if (ctrl_id == IDC_LOGSIZELIMIT)
				_r_config_setulong (L"LogSizeLimitKb", (DWORD)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));

			else if (ctrl_id == IDC_NOTIFICATIONTIMEOUT)
				_r_config_setulong (L"NotificationsTimeout", (DWORD)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));

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
						INT ctrl_id = GetDlgCtrlID ((HWND)(lpnmdi->hdr.idFrom));

						if (ctrl_id == IDC_RULE_BLOCKOUTBOUND)
							_r_str_copy (buffer, RTL_NUMBER_OF (buffer), _r_locale_getstring (IDS_RULE_BLOCKOUTBOUND));

						else if (ctrl_id == IDC_RULE_BLOCKINBOUND)
							_r_str_copy (buffer, RTL_NUMBER_OF (buffer), _r_locale_getstring (IDS_RULE_BLOCKINBOUND));

						else if (ctrl_id == IDC_RULE_ALLOWLOOPBACK)
							_r_str_copy (buffer, RTL_NUMBER_OF (buffer), _r_locale_getstring (IDS_RULE_ALLOWLOOPBACK_HINT));

						else if (ctrl_id == IDC_USESTEALTHMODE_CHK)
							_r_str_copy (buffer, RTL_NUMBER_OF (buffer), _r_locale_getstring (IDS_USESTEALTHMODE_HINT));

						else if (ctrl_id == IDC_INSTALLBOOTTIMEFILTERS_CHK)
							_r_str_copy (buffer, RTL_NUMBER_OF (buffer), _r_locale_getstring (IDS_INSTALLBOOTTIMEFILTERS_HINT));

						else if (ctrl_id == IDC_SECUREFILTERS_CHK)
							_r_str_copy (buffer, RTL_NUMBER_OF (buffer), _r_locale_getstring (IDS_SECUREFILTERS_HINT));

						if (!_r_str_isempty (buffer))
							lpnmdi->lpszText = buffer;
					}

					break;
				}

				case LVN_ITEMCHANGED:
				{
					LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lparam;

					INT listview_id = PtrToInt ((PVOID)lpnmlv->hdr.idFrom);

					if ((lpnmlv->uChanged & LVIF_STATE) != 0)
					{
						if (listview_id == IDC_COLORS)
						{
							if ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (1) || ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (2)))
							{
								if (_r_fastlock_islocked (&lock_checkbox))
									break;

								BOOLEAN is_enabled = (lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (2);

								PITEM_COLOR ptr_clr = (PITEM_COLOR)lpnmlv->lParam;

								if (ptr_clr)
								{
									_r_config_setboolean (_r_obj_getstring (ptr_clr->configName), is_enabled, L"colors");

									_r_listview_redraw (_r_app_gethwnd (), (INT)_r_tab_getlparam (_r_app_gethwnd (), IDC_TAB, INVALID_INT), INVALID_INT);
								}
							}
						}
					}

					break;
				}

				case NM_CUSTOMDRAW:
				{
					LONG_PTR result = _app_nmcustdraw_listview ((LPNMLVCUSTOMDRAW)lparam);

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, result);
					return result;
				}

				case NM_DBLCLK:
				{
					LPNMITEMACTIVATE lpnmlv = (LPNMITEMACTIVATE)lparam;

					INT listview_id = PtrToInt ((PVOID)lpnmlv->hdr.idFrom);

					if (lpnmlv->iItem == INVALID_INT)
						break;

					if (listview_id == IDC_COLORS)
					{
						PITEM_COLOR ptr_clr_current = (PITEM_COLOR)_r_listview_getitemlparam (hwnd, listview_id, lpnmlv->iItem);

						if (ptr_clr_current)
						{
							CHOOSECOLOR cc = {0};
							COLORREF cust[16] = {0};

							SIZE_T index = 0;
							PITEM_COLOR ptr_clr;

							for (auto it = colors.begin (); it != colors.end (); ++it)
							{
								ptr_clr = *it;
								cust[index++] = ptr_clr->default_clr;
							}

							cc.lStructSize = sizeof (cc);
							cc.Flags = CC_RGBINIT | CC_FULLOPEN;
							cc.hwndOwner = hwnd;
							cc.lpCustColors = cust;
							cc.rgbResult = ptr_clr_current->new_clr;

							if (ChooseColor (&cc))
							{
								ptr_clr_current->new_clr = cc.rgbResult;
								_r_config_setulong (_r_obj_getstring (ptr_clr_current->configValue), cc.rgbResult, L"colors");

								_r_listview_redraw (hwnd, IDC_COLORS, INVALID_INT);
								_r_listview_redraw (_r_app_gethwnd (), (INT)_r_tab_getlparam (_r_app_gethwnd (), IDC_TAB, INVALID_INT), INVALID_INT);
							}
						}
					}

					break;
				}

				case NM_CLICK:
				case NM_RETURN:
				{
					PNMLINK pnmlink = (PNMLINK)lparam;

					if (!_r_str_isempty (pnmlink->item.szUrl))
						ShellExecute (NULL, NULL, pnmlink->item.szUrl, NULL, NULL, SW_SHOWNORMAL);

					break;
				}

				case LVN_GETEMPTYMARKUP:
				{
					NMLVEMPTYMARKUP* lpnmlv = (NMLVEMPTYMARKUP*)lparam;

					lpnmlv->dwFlags = EMF_CENTERED;
					_r_str_copy (lpnmlv->szMarkup, RTL_NUMBER_OF (lpnmlv->szMarkup), _r_locale_getstring (IDS_STATUS_EMPTY));

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
					return TRUE;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			INT ctrl_id = LOWORD (wparam);
			INT notify_code = HIWORD (wparam);

			switch (ctrl_id)
			{
				case IDC_ALWAYSONTOP_CHK:
				{
					BOOLEAN is_enabled = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

					_r_config_setboolean (L"AlwaysOnTop", is_enabled);
					_r_menu_checkitem (GetMenu (_r_app_gethwnd ()), IDM_ALWAYSONTOP_CHK, 0, MF_BYCOMMAND, is_enabled);

					break;
				}

#if defined(_APP_HAVE_AUTORUN)
				case IDC_LOADONSTARTUP_CHK:
				{
					_r_autorun_enable (hwnd, (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					CheckDlgButton (hwnd, ctrl_id, _r_autorun_isenabled () ? BST_CHECKED : BST_UNCHECKED);

					break;
				}
#endif // _APP_HAVE_AUTORUN

				case IDC_STARTMINIMIZED_CHK:
				{
					_r_config_setboolean (L"IsStartMinimized", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}

#if defined(_APP_HAVE_SKIPUAC)
				case IDC_SKIPUACWARNING_CHK:
				{
					_r_skipuac_enable (hwnd, (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					CheckDlgButton (hwnd, ctrl_id, _r_skipuac_isenabled () ? BST_CHECKED : BST_UNCHECKED);

					break;
				}
#endif // _APP_HAVE_SKIPUAC

				case IDC_CHECKUPDATES_CHK:
				{
					BOOLEAN is_enabled = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

					_r_config_setboolean (L"CheckUpdates", is_enabled);

#if !defined(_DEBUG) && !defined(_APP_BETA)
					_r_ctrl_enable (hwnd, IDC_CHECKUPDATESBETA_CHK, is_enabled);
#endif // !_DEBUG && !_APP_BETA

					break;
				}

#if !defined(_DEBUG) && !defined(_APP_BETA)
				case IDC_CHECKUPDATESBETA_CHK:
				{
					_r_config_setboolean (L"CheckUpdatesBeta", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}
#endif // !_DEBUG && !_APP_BETA

				case IDC_LANGUAGE:
				{
					if (notify_code == CBN_SELCHANGE)
						_r_locale_applyfromcontrol (hwnd, ctrl_id);

					break;
				}

				case IDC_CONFIRMEXIT_CHK:
				{
					_r_config_setboolean (L"ConfirmExit2", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}

				case IDC_CONFIRMEXITTIMER_CHK:
				{
					_r_config_setboolean (L"ConfirmExitTimer", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}

				case IDC_CONFIRMLOGCLEAR_CHK:
				{
					_r_config_setboolean (L"ConfirmLogClear", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
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
					_app_config_apply (_r_app_gethwnd (), ctrl_id);
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
					HMENU hmenu = GetMenu (_r_app_gethwnd ());

					if (ctrl_id >= IDC_BLOCKLIST_SPY_DISABLE && ctrl_id <= IDC_BLOCKLIST_SPY_BLOCK)
					{
						INT new_state = _r_calc_clamp (INT, _r_ctrl_isradiobuttonchecked (hwnd, IDC_BLOCKLIST_SPY_DISABLE, IDC_BLOCKLIST_SPY_BLOCK) - IDC_BLOCKLIST_SPY_DISABLE, 0, 2);

						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_SPY_DISABLE, IDM_BLOCKLIST_SPY_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_SPY_DISABLE + new_state);

						_r_config_setinteger (L"BlocklistSpyState", new_state);

						_app_ruleblocklistset (_r_app_gethwnd (), new_state, INVALID_INT, INVALID_INT, TRUE);
					}
					else if (ctrl_id >= IDC_BLOCKLIST_UPDATE_DISABLE && ctrl_id <= IDC_BLOCKLIST_UPDATE_BLOCK)
					{
						INT new_state = _r_calc_clamp (INT, _r_ctrl_isradiobuttonchecked (hwnd, IDC_BLOCKLIST_UPDATE_DISABLE, IDC_BLOCKLIST_UPDATE_BLOCK) - IDC_BLOCKLIST_UPDATE_DISABLE, 0, 2);

						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_UPDATE_DISABLE, IDM_BLOCKLIST_UPDATE_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_UPDATE_DISABLE + new_state);

						_r_config_setinteger (L"BlocklistUpdateState", new_state);

						_app_ruleblocklistset (_r_app_gethwnd (), INVALID_INT, new_state, INVALID_INT, TRUE);
					}
					else if (ctrl_id >= IDC_BLOCKLIST_EXTRA_DISABLE && ctrl_id <= IDC_BLOCKLIST_EXTRA_BLOCK)
					{
						INT new_state = _r_calc_clamp (INT, _r_ctrl_isradiobuttonchecked (hwnd, IDC_BLOCKLIST_EXTRA_DISABLE, IDC_BLOCKLIST_EXTRA_BLOCK) - IDC_BLOCKLIST_EXTRA_DISABLE, 0, 2);

						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_EXTRA_DISABLE, IDM_BLOCKLIST_EXTRA_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_EXTRA_DISABLE + new_state);

						_r_config_setinteger (L"BlocklistExtraState", new_state);

						_app_ruleblocklistset (_r_app_gethwnd (), INVALID_INT, INVALID_INT, new_state, TRUE);
					}

					break;
				}

				case IDC_ENABLELOG_CHK:
				{
					BOOLEAN is_checked = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);
					BOOLEAN is_enabled = is_checked || (IsDlgButtonChecked (hwnd, IDC_ENABLEUILOG_CHK) == BST_CHECKED);

					_r_config_setboolean (L"IsLogEnabled", is_checked);
					SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_PRESSBUTTON, IDM_TRAY_ENABLELOG_CHK, MAKELPARAM (is_checked, 0));

					_r_ctrl_enable (hwnd, IDC_LOGPATH, is_checked); // input
					_r_ctrl_enable (hwnd, IDC_LOGPATH_BTN, is_checked); // button

					_r_ctrl_enable (hwnd, IDC_LOGVIEWER, is_checked); // input
					_r_ctrl_enable (hwnd, IDC_LOGVIEWER_BTN, is_checked); // button

					EnableWindow ((HWND)SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_GETBUDDY, 0, 0), is_checked);

					_r_ctrl_enable (hwnd, IDC_EXCLUDESTEALTH_CHK, is_enabled);

					// win8+
					if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
						_r_ctrl_enable (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, is_enabled);

					_app_loginit (is_enabled);

					break;
				}

				case IDC_ENABLEUILOG_CHK:
				{
					BOOLEAN is_checked = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);
					BOOLEAN is_enabled = is_checked || (IsDlgButtonChecked (hwnd, IDC_ENABLELOG_CHK) == BST_CHECKED);

					_r_config_setboolean (L"IsLogUiEnabled", is_checked);
					SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_PRESSBUTTON, IDM_TRAY_ENABLEUILOG_CHK, MAKELPARAM (is_checked, 0));

					_r_ctrl_enable (hwnd, IDC_EXCLUDESTEALTH_CHK, is_enabled);

					// win8+
					if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
						_r_ctrl_enable (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, is_enabled);

					break;
				}

				case IDC_LOGPATH:
				{
					if (notify_code == EN_KILLFOCUS)
					{
						PR_STRING logpath = _r_ctrl_gettext (hwnd, ctrl_id);

						if (logpath)
						{
							_r_obj_movereference (&logpath, _r_path_unexpand (_r_obj_getstring (logpath)));

							_r_config_setstring (L"LogPath", _r_obj_getstring (logpath));

							_app_loginit (_r_config_getboolean (L"IsLogEnabled", FALSE));

							_r_obj_dereference (logpath);
						}
					}

					break;
				}

				case IDC_LOGPATH_BTN:
				{
					OPENFILENAME ofn = {0};

					WCHAR path[512];
					PR_STRING expandedPath;

					expandedPath = _r_ctrl_gettext (hwnd, IDC_LOGPATH);

					_r_obj_movereference (&expandedPath, _r_path_expand (_r_obj_getstring (expandedPath)));
					_r_str_copy (path, RTL_NUMBER_OF (path), _r_obj_getstring (expandedPath));

					ofn.lStructSize = sizeof (ofn);
					ofn.hwndOwner = hwnd;
					ofn.lpstrFile = path;
					ofn.nMaxFile = RTL_NUMBER_OF (path);
					ofn.lpstrFileTitle = APP_NAME_SHORT;
					ofn.lpstrFilter = L"*." LOG_PATH_EXT L"\0*." LOG_PATH_EXT L"\0\0";
					ofn.lpstrDefExt = LOG_PATH_EXT;
					ofn.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

					if (GetSaveFileName (&ofn))
					{
						_r_obj_movereference (&expandedPath, _r_path_unexpand (path));

						_r_config_setstring (L"LogPath", _r_obj_getstring (expandedPath));
						_r_ctrl_settext (hwnd, IDC_LOGPATH, _r_obj_getstringorempty (expandedPath));

						_app_loginit (_r_config_getboolean (L"IsLogEnabled", FALSE));
					}

					if (expandedPath)
						_r_obj_dereference (expandedPath);

					break;
				}

				case IDC_LOGVIEWER:
				{
					if (notify_code == EN_KILLFOCUS)
					{
						PR_STRING logviewer = _r_ctrl_gettext (hwnd, ctrl_id);

						if (logviewer)
						{
							_r_obj_movereference (&logviewer, _r_path_unexpand (_r_obj_getstring (logviewer)));

							_r_config_setstring (L"LogViewer", _r_obj_getstring (logviewer));

							_r_obj_dereference (logviewer);
						}
					}

					break;
				}

				case IDC_LOGVIEWER_BTN:
				{
					OPENFILENAME ofn = {0};

					WCHAR path[512];
					PR_STRING expandedPath;

					expandedPath = _r_ctrl_gettext (hwnd, IDC_LOGVIEWER);

					_r_obj_movereference (&expandedPath, _r_path_expand (_r_obj_getstring (expandedPath)));
					_r_str_copy (path, RTL_NUMBER_OF (path), _r_obj_getstring (expandedPath));

					ofn.lStructSize = sizeof (ofn);
					ofn.hwndOwner = hwnd;
					ofn.lpstrFile = path;
					ofn.nMaxFile = RTL_NUMBER_OF (path);
					ofn.lpstrFileTitle = APP_NAME_SHORT;
					ofn.lpstrFilter = L"*.exe\0*.exe\0\0";
					ofn.lpstrDefExt = L"exe";
					ofn.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

					if (GetOpenFileName (&ofn))
					{
						_r_obj_movereference (&expandedPath, _r_path_unexpand (path));

						_r_config_setstring (L"LogViewer", _r_obj_getstringorempty (expandedPath));
						_r_ctrl_settext (hwnd, IDC_LOGVIEWER, _r_obj_getstringorempty (expandedPath));
					}

					if (expandedPath)
						_r_obj_dereference (expandedPath);

					break;
				}

				case IDC_LOGSIZELIMIT_CTRL:
				{
					if (notify_code == EN_KILLFOCUS)
						_r_config_setulong (L"LogSizeLimitKb", (DWORD)SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_GETPOS32, 0, 0));

					break;
				}

				case IDC_ENABLENOTIFICATIONS_CHK:
				{
					BOOLEAN is_enabled = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

					_r_config_setboolean (L"IsNotificationsEnabled", is_enabled);
					SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_PRESSBUTTON, IDM_TRAY_ENABLENOTIFICATIONS_CHK, MAKELPARAM (is_enabled, 0));

					_r_ctrl_enable (hwnd, IDC_NOTIFICATIONSOUND_CHK, is_enabled);
					_r_ctrl_enable (hwnd, IDC_NOTIFICATIONONTRAY_CHK, is_enabled);

					HWND hbuddy = (HWND)SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_GETBUDDY, 0, 0);

					if (hbuddy)
						EnableWindow (hbuddy, is_enabled);

					_r_ctrl_enable (hwnd, IDC_EXCLUDEBLOCKLIST_CHK, is_enabled);
					_r_ctrl_enable (hwnd, IDC_EXCLUDECUSTOM_CHK, is_enabled);

					PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDC_NOTIFICATIONSOUND_CHK, 0), 0);

					_app_notifyrefresh (config.hnotification, FALSE);

					break;
				}

				case IDC_NOTIFICATIONSOUND_CHK:
				{
					BOOLEAN is_checked = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

					CheckDlgButton (hwnd, IDC_NOTIFICATIONFULLSCREENSILENTMODE_CHK, _r_config_getboolean (L"IsNotificationsFullscreenSilentMode", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					_r_config_setboolean (L"IsNotificationsSound", is_checked);

					_r_ctrl_enable (hwnd, IDC_NOTIFICATIONFULLSCREENSILENTMODE_CHK, _r_ctrl_isenabled (hwnd, ctrl_id) && is_checked);

					break;
				}

				case IDC_NOTIFICATIONFULLSCREENSILENTMODE_CHK:
				{
					_r_config_setboolean (L"IsNotificationsFullscreenSilentMode", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}

				case IDC_NOTIFICATIONONTRAY_CHK:
				{
					_r_config_setboolean (L"IsNotificationsOnTray", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));

					if (IsWindowVisible (config.hnotification))
						_app_notifysetpos (config.hnotification, TRUE);

					break;
				}

				case IDC_NOTIFICATIONTIMEOUT_CTRL:
				{
					if (notify_code == EN_KILLFOCUS)
						_r_config_setulong (L"NotificationsTimeout", (DWORD)SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_GETPOS32, 0, 0));

					break;
				}

				case IDC_EXCLUDESTEALTH_CHK:
				{
					_r_config_setboolean (L"IsExcludeStealth", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}

				case IDC_EXCLUDECLASSIFYALLOW_CHK:
				{
					_r_config_setboolean (L"IsExcludeClassifyAllow", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}

				case IDC_EXCLUDEBLOCKLIST_CHK:
				{
					_r_config_setboolean (L"IsExcludeBlocklist", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}

				case IDC_EXCLUDECUSTOM_CHK:
				{
					_r_config_setboolean (L"IsExcludeCustomRules", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

VOID _app_resizewindow (HWND hwnd, LPARAM lparam)
{
	SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_AUTOSIZE, 0, 0);
	SendDlgItemMessage (hwnd, IDC_STATUSBAR, WM_SIZE, 0, 0);

	RECT rc = {0};
	GetClientRect (GetDlgItem (hwnd, IDC_STATUSBAR), &rc);

	INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);
	INT statusbar_height = _r_calc_rectheight (INT, &rc);
	INT rebar_height = (INT)SendDlgItemMessage (hwnd, IDC_REBAR, RB_GETBARHEIGHT, 0, 0);

	HDWP hdefer = BeginDeferWindowPos (2);

	hdefer = DeferWindowPos (hdefer, config.hrebar, NULL, 0, 0, LOWORD (lparam), rebar_height, SWP_NOZORDER | SWP_NOOWNERZORDER);
	hdefer = DeferWindowPos (hdefer, GetDlgItem (hwnd, IDC_TAB), NULL, 0, rebar_height, LOWORD (lparam), HIWORD (lparam) - rebar_height - statusbar_height, SWP_NOZORDER | SWP_NOOWNERZORDER);

	EndDeferWindowPos (hdefer);

	if (listview_id)
	{
		_r_tab_adjustchild (hwnd, IDC_TAB, GetDlgItem (hwnd, listview_id));
		_app_listviewresize (hwnd, listview_id, FALSE);
	}

	_app_refreshstatus (hwnd, 0);
}

VOID _app_imagelist_init (HWND hwnd)
{
	INT icon_size_small = _r_dc_getsystemmetrics (hwnd, SM_CXSMICON);
	INT icon_size_large = _r_dc_getsystemmetrics (hwnd, SM_CXICON);
	INT icon_size_toolbar = _r_calc_clamp (INT, _r_dc_getdpi (hwnd, _r_config_getinteger (L"ToolbarSize", _R_SIZE_ITEMHEIGHT)), icon_size_small, icon_size_large);

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
	SAFE_DELETE_ICON (config.hicon_uwp);

	// get default icon for executable
	_app_getfileicon (_r_obj_getstring (config.ntoskrnl_path), FALSE, &config.icon_id, &config.hicon_large);
	_app_getfileicon (_r_obj_getstring (config.ntoskrnl_path), TRUE, NULL, &config.hicon_small);
	_app_getfileicon (_r_obj_getstring (config.shell32_path), FALSE, &config.icon_service_id, NULL);

	// get default icon for windows store package (win8+)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
	{
		if (!_app_getfileicon (_r_obj_getstring (config.winstore_path), TRUE, &config.icon_uwp_id, &config.hicon_uwp))
		{
			config.icon_uwp_id = config.icon_id;
			config.hicon_uwp = CopyIcon (config.hicon_small);
		}
	}

	config.hbmp_enable = _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_SHIELD_ENABLE), icon_size_small);
	config.hbmp_disable = _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_SHIELD_DISABLE), icon_size_small);

	config.hbmp_allow = _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_ALLOW), icon_size_small);
	config.hbmp_block = _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_BLOCK), icon_size_small);
	config.hbmp_cross = _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_CROSS), icon_size_small);
	config.hbmp_rules = _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_SETTINGS), icon_size_small);

	config.hbmp_checked = _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_CHECKED), icon_size_small);
	config.hbmp_unchecked = _app_bitmapfrompng (NULL, MAKEINTRESOURCE (IDP_UNCHECKED), icon_size_small);

	// toolbar imagelist
	if (config.himg_toolbar)
		ImageList_SetIconSize (config.himg_toolbar, icon_size_toolbar, icon_size_toolbar);
	else
		config.himg_toolbar = ImageList_Create (icon_size_toolbar, icon_size_toolbar, ILC_COLOR32 | ILC_HIGHQUALITYSCALE, 0, 0);

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

VOID _app_toolbar_init (HWND hwnd)
{
	config.hrebar = GetDlgItem (hwnd, IDC_REBAR);

	_r_toolbar_setstyle (hwnd, IDC_TOOLBAR, TBSTYLE_EX_DOUBLEBUFFER | TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_HIDECLIPPEDBUTTONS);

	SendDlgItemMessage (hwnd, IDC_TOOLBAR, TB_SETIMAGELIST, 0, (LPARAM)config.himg_toolbar);

	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_TRAY_START, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, I_IMAGENONE);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, 0, BTNS_SEP, NULL, 0, I_IMAGENONE);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_OPENRULESEDITOR, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, 8);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, 0, BTNS_SEP, NULL, 0, I_IMAGENONE);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_TRAY_ENABLENOTIFICATIONS_CHK, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, 4);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_TRAY_ENABLELOG_CHK, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, 5);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_TRAY_ENABLEUILOG_CHK, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, 10);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, 0, BTNS_SEP, NULL, 0, I_IMAGENONE);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_REFRESH, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, 2);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_SETTINGS, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, 3);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, 0, BTNS_SEP, NULL, 0, I_IMAGENONE);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_TRAY_LOGSHOW, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, 6);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, IDM_TRAY_LOGCLEAR, 0, BTNS_BUTTON | BTNS_AUTOSIZE, TBSTATE_ENABLED, 7);
	_r_toolbar_addbutton (hwnd, IDC_TOOLBAR, 0, BTNS_SEP, NULL, 0, I_IMAGENONE);
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

	rbi.cbSize = sizeof (rbi);

	SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_AUTOSIZE, 0, 0);

	DWORD button_size = (DWORD)SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_GETBUTTONSIZE, 0, 0);

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

VOID _app_tabs_init (HWND hwnd)
{
	RECT rc = {0};
	GetClientRect (hwnd, &rc);

	SetWindowPos (GetDlgItem (hwnd, IDC_TAB), NULL, 0, 0, _r_calc_rectwidth (INT, &rc), _r_calc_rectheight (INT, &rc), SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);

	PR_STRING localizedString = NULL;
	HINSTANCE hinst = _r_sys_getimagebase ();
	DWORD listview_ex_style = LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_CHECKBOXES | LVS_EX_HEADERINALLVIEWS;
	DWORD listview_style = WS_CHILD | WS_TABSTOP | LVS_SHOWSELALWAYS | LVS_REPORT | LVS_SHAREIMAGELISTS | LVS_AUTOARRANGE;
	INT tabs_count = 0;

	CreateWindowEx (0, WC_LISTVIEW, NULL, listview_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_APPS_PROFILE, hinst, NULL);
	_r_tab_additem (hwnd, IDC_TAB, tabs_count++, _r_locale_getstring (IDS_TAB_APPS), I_IMAGENONE, IDC_APPS_PROFILE);

	CreateWindowEx (0, WC_LISTVIEW, NULL, listview_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_APPS_SERVICE, hinst, NULL);
	_r_tab_additem (hwnd, IDC_TAB, tabs_count++, _r_locale_getstring (IDS_TAB_SERVICES), I_IMAGENONE, IDC_APPS_SERVICE);

	// uwp apps (win8+)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
	{
		CreateWindowEx (0, WC_LISTVIEW, NULL, listview_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_APPS_UWP, hinst, NULL);
		_r_tab_additem (hwnd, IDC_TAB, tabs_count++, _r_locale_getstring (IDS_TAB_PACKAGES), I_IMAGENONE, IDC_APPS_UWP);
	}

	if (!_r_config_getboolean (L"IsInternalRulesDisabled", FALSE))
	{
		CreateWindowEx (0, WC_LISTVIEW, NULL, listview_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_RULES_BLOCKLIST, hinst, NULL);
		_r_tab_additem (hwnd, IDC_TAB, tabs_count++, _r_locale_getstring (IDS_TRAY_BLOCKLIST_RULES), I_IMAGENONE, IDC_RULES_BLOCKLIST);

		CreateWindowEx (0, WC_LISTVIEW, NULL, listview_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_RULES_SYSTEM, hinst, NULL);
		_r_tab_additem (hwnd, IDC_TAB, tabs_count++, _r_locale_getstring (IDS_TRAY_SYSTEM_RULES), I_IMAGENONE, IDC_RULES_SYSTEM);
	}

	CreateWindowEx (0, WC_LISTVIEW, NULL, listview_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_RULES_CUSTOM, hinst, NULL);
	_r_tab_additem (hwnd, IDC_TAB, tabs_count++, _r_locale_getstring (IDS_TRAY_USER_RULES), I_IMAGENONE, IDC_RULES_CUSTOM);

	CreateWindowEx (0, WC_LISTVIEW, NULL, listview_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_NETWORK, hinst, NULL);
	_r_tab_additem (hwnd, IDC_TAB, tabs_count++, _r_locale_getstring (IDS_TAB_NETWORK), I_IMAGENONE, IDC_NETWORK);

	CreateWindowEx (0, WC_LISTVIEW, NULL, listview_style | LVS_NOSORTHEADER, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_LOG, hinst, NULL);
	_r_tab_additem (hwnd, IDC_TAB, tabs_count++, _r_locale_getstring (IDS_TITLE_LOGGING), I_IMAGENONE, IDC_LOG);

	for (INT i = 0; i < tabs_count; i++)
	{
		INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, i);

		if (!listview_id)
			continue;

		HWND hlistview = GetDlgItem (hwnd, listview_id);

		if (!hlistview)
			continue;

		SetWindowPos (hlistview, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_FRAMECHANGED);

		_app_listviewsetfont (hwnd, listview_id, FALSE);

		if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_RULES_CUSTOM)
		{
			_r_listview_setstyle (hwnd, listview_id, listview_ex_style, TRUE);

			if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
			{
				_r_listview_addcolumn (hwnd, listview_id, 0, _r_locale_getstring (IDS_NAME), 0, LVCFMT_LEFT);
				_r_listview_addcolumn (hwnd, listview_id, 1, _r_locale_getstring (IDS_ADDED), 0, LVCFMT_RIGHT);
			}
			else
			{
				_r_listview_addcolumn (hwnd, listview_id, 0, _r_locale_getstring (IDS_NAME), 0, LVCFMT_LEFT);
				_r_listview_addcolumn (hwnd, listview_id, 1, _r_locale_getstring (IDS_PROTOCOL), 0, LVCFMT_RIGHT);
				_r_listview_addcolumn (hwnd, listview_id, 2, _r_locale_getstring (IDS_DIRECTION), 0, LVCFMT_RIGHT);
			}

			_r_listview_addgroup (hwnd, listview_id, 0, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
			_r_listview_addgroup (hwnd, listview_id, 1, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
			_r_listview_addgroup (hwnd, listview_id, 2, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
		}
		else if (listview_id == IDC_NETWORK || listview_id == IDC_LOG)
		{
			_r_listview_setstyle (hwnd, listview_id, listview_ex_style & ~LVS_EX_CHECKBOXES, (listview_id == IDC_NETWORK)); // no checkboxes

			_r_listview_addcolumn (hwnd, listview_id, 0, _r_locale_getstring (IDS_NAME), 0, LVCFMT_LEFT);

			_r_obj_movereference (&localizedString, _r_format_string (L"%s (" SZ_DIRECTION_LOCAL L")", _r_locale_getstring (IDS_ADDRESS)));
			_r_listview_addcolumn (hwnd, listview_id, 1, _r_obj_getstringorempty (localizedString), 0, LVCFMT_LEFT);

			_r_obj_movereference (&localizedString, _r_format_string (L"%s (" SZ_DIRECTION_LOCAL L")", _r_locale_getstring (IDS_PORT)));
			_r_listview_addcolumn (hwnd, listview_id, 2, _r_obj_getstringorempty (localizedString), 0, LVCFMT_RIGHT);

			_r_obj_movereference (&localizedString, _r_format_string (L"%s (" SZ_DIRECTION_REMOTE L")", _r_locale_getstring (IDS_ADDRESS)));
			_r_listview_addcolumn (hwnd, listview_id, 3, _r_obj_getstringorempty (localizedString), 0, LVCFMT_LEFT);

			_r_obj_movereference (&localizedString, _r_format_string (L"%s (" SZ_DIRECTION_REMOTE L")", _r_locale_getstring (IDS_PORT)));
			_r_listview_addcolumn (hwnd, listview_id, 4, _r_obj_getstringorempty (localizedString), 0, LVCFMT_RIGHT);

			_r_listview_addcolumn (hwnd, listview_id, 5, _r_locale_getstring (IDS_PROTOCOL), 0, LVCFMT_RIGHT);

			if (listview_id == IDC_NETWORK)
			{
				_r_listview_addcolumn (hwnd, listview_id, 6, _r_locale_getstring (IDS_STATE), 0, LVCFMT_RIGHT);

				_r_listview_addgroup (hwnd, listview_id, 0, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
				_r_listview_addgroup (hwnd, listview_id, 1, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
				_r_listview_addgroup (hwnd, listview_id, 2, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
			}
			else if (listview_id == IDC_LOG)
			{
				_r_listview_addcolumn (hwnd, listview_id, 6, _r_locale_getstring (IDS_FILTER), 0, LVCFMT_LEFT);
				_r_listview_addcolumn (hwnd, listview_id, 7, _r_locale_getstring (IDS_DIRECTION), 0, LVCFMT_RIGHT);
				_r_listview_addcolumn (hwnd, listview_id, 8, _r_locale_getstring (IDS_STATE), 0, LVCFMT_RIGHT);
			}
		}

		_r_tab_adjustchild (hwnd, IDC_TAB, hlistview);
	}

	if (localizedString)
		_r_obj_dereference (localizedString);
}

VOID _app_initialize ()
{
	pugi::set_memory_management_functions ((pugi::allocation_function)_r_mem_allocatezero, _r_mem_free); // set allocation routine

	// initialize spinlocks
	_r_fastlock_initialize (&lock_apply);
	_r_fastlock_initialize (&lock_checkbox);
	_r_fastlock_initialize (&lock_logbusy);
	_r_fastlock_initialize (&lock_logthread);
	_r_fastlock_initialize (&lock_transaction);

	// set privileges
	{
		ULONG privileges[] = {
			SE_SECURITY_PRIVILEGE,
			SE_TAKE_OWNERSHIP_PRIVILEGE,
			SE_BACKUP_PRIVILEGE,
			SE_RESTORE_PRIVILEGE,
			SE_DEBUG_PRIVILEGE,
		};

		_r_sys_setprivilege (privileges, RTL_NUMBER_OF (privileges), TRUE);
	}

	// set process priority
	SetPriorityClass (NtCurrentProcess (), ABOVE_NORMAL_PRIORITY_CLASS);

	// static initializer
	config.wd_length = GetWindowsDirectory (config.windows_dir, RTL_NUMBER_OF (config.windows_dir));

	_r_str_printf (config.profile_path, RTL_NUMBER_OF (config.profile_path), L"%s\\" XML_PROFILE, _r_app_getprofiledirectory ());
	_r_str_printf (config.profile_path_backup, RTL_NUMBER_OF (config.profile_path_backup), L"%s\\" XML_PROFILE L".bak", _r_app_getprofiledirectory ());
	_r_str_printf (config.profile_internal_path, RTL_NUMBER_OF (config.profile_internal_path), L"%s\\" XML_PROFILE_INTERNAL, _r_app_getprofiledirectory ());

	config.svchost_path = _r_path_expand (PATH_SVCHOST);
	config.ntoskrnl_path = _r_path_expand (PATH_NTOSKRNL);
	config.shell32_path = _r_path_expand (PATH_SHELL32);
	config.winstore_path = _r_path_expand (PATH_WINSTORE);

	config.my_hash = _r_str_hash (_r_sys_getimagepathname ());
	config.ntoskrnl_hash = _r_str_hash (PROC_SYSTEM_NAME);
	config.svchost_hash = _r_str_hash (config.svchost_path);

	// initialize timers
	{
		if (config.htimer)
			DeleteTimerQueue (config.htimer);

		config.htimer = CreateTimerQueue ();

		timers.clear ();

		timers.emplace_back (_r_calc_minutes2seconds (time_t, 2));
		timers.emplace_back (_r_calc_minutes2seconds (time_t, 5));
		timers.emplace_back (_r_calc_minutes2seconds (time_t, 10));
		timers.emplace_back (_r_calc_hours2seconds (time_t, 1));
		timers.emplace_back (_r_calc_hours2seconds (time_t, 2));
		timers.emplace_back (_r_calc_hours2seconds (time_t, 4));
		timers.emplace_back (_r_calc_hours2seconds (time_t, 6));
	}

	// initialize thread objects
	if (!config.done_evt)
		config.done_evt = CreateEventEx (NULL, NULL, 0, EVENT_ALL_ACCESS);

	_app_generate_credentials ();
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
		LPFINDREPLACE lpfr = (LPFINDREPLACE)lparam;

		if ((lpfr->Flags & FR_DIALOGTERM) != 0)
		{
			config.hfind = NULL;
		}
		else if ((lpfr->Flags & FR_FINDNEXT) != 0)
		{
			INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

			if (!listview_id)
				return FALSE;

			INT total_count = _r_listview_getitemcount (hwnd, listview_id, FALSE);

			if (!total_count)
				return FALSE;

			BOOLEAN is_wrap = TRUE;

			INT selected_item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)INVALID_INT, LVNI_SELECTED);

			INT current_item = selected_item + 1;
			INT last_item = total_count;

find_wrap:

			for (; current_item < last_item; current_item++)
			{
				PR_STRING item_text = _r_listview_getitemtext (hwnd, listview_id, current_item, 0);

				if (item_text)
				{
					if (StrStrNIW (_r_obj_getstring (item_text), lpfr->lpstrFindWhat, (UINT)_r_obj_getstringlength (item_text)) != NULL)
					{
						_app_showitem (hwnd, listview_id, current_item, INVALID_INT);
						_r_obj_dereference (item_text);

						return FALSE;
					}

					_r_obj_dereference (item_text);
				}
			}

			if (is_wrap)
			{
				is_wrap = FALSE;

				current_item = 0;
				last_item = min (selected_item + 1, total_count);

				goto find_wrap;
			}
		}

		return FALSE;
	}

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			SetWindowText (hwnd, APP_NAME);

			_app_initialize ();

			_r_wnd_center (hwnd, NULL);

			// init buffered paint
			BufferedPaintInit ();

			// allow drag&drop support
			DragAcceptFiles (hwnd, TRUE);

			// initialize settings
			_r_settings_addpage (IDD_SETTINGS_GENERAL, IDS_SETTINGS_GENERAL);
			_r_settings_addpage (IDD_SETTINGS_INTERFACE, IDS_TITLE_INTERFACE);
			_r_settings_addpage (IDD_SETTINGS_HIGHLIGHTING, IDS_TITLE_HIGHLIGHTING);
			_r_settings_addpage (IDD_SETTINGS_RULES, IDS_TRAY_RULES);

			if (!_r_config_getboolean (L"IsInternalRulesDisabled", FALSE))
				_r_settings_addpage (IDD_SETTINGS_BLOCKLIST, IDS_TRAY_BLOCKLIST_RULES);

			// dropped packets logging (win7+)
			_r_settings_addpage (IDD_SETTINGS_NOTIFICATIONS, IDS_TITLE_NOTIFICATIONS);
			_r_settings_addpage (IDD_SETTINGS_LOGGING, IDS_TITLE_LOGGING);

			// initialize colors
			addcolor (IDS_HIGHLIGHT_TIMER, L"IsHighlightTimer", TRUE, L"ColorTimer", LISTVIEW_COLOR_TIMER);
			addcolor (IDS_HIGHLIGHT_INVALID, L"IsHighlightInvalid", TRUE, L"ColorInvalid", LISTVIEW_COLOR_INVALID);
			addcolor (IDS_HIGHLIGHT_SPECIAL, L"IsHighlightSpecial", TRUE, L"ColorSpecial", LISTVIEW_COLOR_SPECIAL);
			addcolor (IDS_HIGHLIGHT_SILENT, L"IsHighlightSilent", TRUE, L"ColorSilent", LISTVIEW_COLOR_SILENT);
			addcolor (IDS_HIGHLIGHT_SIGNED, L"IsHighlightSigned", TRUE, L"ColorSigned", LISTVIEW_COLOR_SIGNED);
			addcolor (IDS_HIGHLIGHT_PICO, L"IsHighlightPico", TRUE, L"ColorPico", LISTVIEW_COLOR_PICO);
			addcolor (IDS_HIGHLIGHT_SYSTEM, L"IsHighlightSystem", TRUE, L"ColorSystem", LISTVIEW_COLOR_SYSTEM);
			addcolor (IDS_HIGHLIGHT_CONNECTION, L"IsHighlightConnection", TRUE, L"ColorConnection", LISTVIEW_COLOR_CONNECTION);

			// restore window size and position (required!)
			_r_window_restoreposition (hwnd, L"window");

			// initialize imagelist
			_app_imagelist_init (hwnd);

			// initialize toolbar
			_app_toolbar_init (hwnd);
			_app_toolbar_resize ();

			// initialize tabs
			_app_tabs_init (hwnd);

#if defined(_APP_HAVE_DARKTHEME)
			_r_wnd_setdarktheme (hwnd);
#endif // _APP_HAVE_DARKTHEME

			// load profile
			_app_profile_load (hwnd, NULL);

			// add blocklist to update
			if (!_r_config_getboolean (L"IsInternalRulesDisabled", FALSE))
			{
				WCHAR internalProfileVersion[128];
				_r_str_printf (internalProfileVersion, RTL_NUMBER_OF (internalProfileVersion), L"%" TEXT (PR_LONG64), config.profile_internal_timestamp);

				_r_update_addcomponent (L"Internal rules", L"profile_internal", internalProfileVersion, config.profile_internal_path, FALSE);
			}

			// initialize tab
			_app_settab_id (hwnd, _r_config_getinteger (L"CurrentTab", IDC_APPS_PROFILE));

			// initialize dropped packets log callback thread (win7+)
			RtlSecureZeroMemory (&log_stack, sizeof (log_stack));
			RtlInitializeSListHead (&log_stack.ListHead);

			// create notification window
			_app_notifycreatewindow ();

			// create network monitor thread
			_r_sys_createthreadex (&NetworkMonitorThread, (PVOID)hwnd, FALSE, THREAD_PRIORITY_LOWEST);

			// install filters
			{
				ENUM_INSTALL_TYPE install_type = _wfp_isfiltersinstalled ();

				if (install_type != InstallDisabled)
				{
					if (install_type == InstallEnabledTemporary)
						config.is_filterstemporary = TRUE;

					//if (_r_config_getboolean (L"IsDisableWindowsFirewallChecked", TRUE))
					//	_mps_changeconfig2 (FALSE);

					_app_changefilters (hwnd, TRUE, TRUE);
				}
				else
				{
					_r_status_settext (hwnd, IDC_STATUSBAR, 0, _r_locale_getstring (_app_getinterfacestatelocale (install_type)));
				}
			}

			// set column size when "auto-size" option are disabled
			if (!_r_config_getboolean (L"AutoSizeColumns", TRUE))
			{
				for (INT i = 0; i < (INT)SendDlgItemMessage (hwnd, IDC_TAB, TCM_GETITEMCOUNT, 0, 0); i++)
				{
					INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, i);

					if (listview_id)
						_app_listviewresize (hwnd, listview_id, TRUE);
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
			_r_tray_create (hwnd, UID, WM_TRAYICON, _r_app_getsharedimage (_r_sys_getimagebase (), (_wfp_isfiltersinstalled () != InstallDisabled) ? IDI_ACTIVE : IDI_INACTIVE, _r_dc_getsystemmetrics (hwnd, SM_CXSMICON)), APP_NAME, FALSE);

			HMENU hmenu = GetMenu (hwnd);

			if (hmenu)
			{
				if (_r_config_getboolean (L"IsInternalRulesDisabled", FALSE))
					_r_menu_enableitem (hmenu, 4, MF_BYPOSITION, FALSE);

				_r_menu_checkitem (hmenu, IDM_ALWAYSONTOP_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"AlwaysOnTop", _APP_ALWAYSONTOP));
				_r_menu_checkitem (hmenu, IDM_SHOWFILENAMESONLY_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"ShowFilenames", TRUE));
				_r_menu_checkitem (hmenu, IDM_AUTOSIZECOLUMNS_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"AutoSizeColumns", TRUE));

				{
					UINT menu_id;
					INT view_type = _r_calc_clamp (INT, _r_config_getinteger (L"ViewType", LV_VIEW_DETAILS), LV_VIEW_ICON, LV_VIEW_MAX);

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
					INT icon_size = _r_calc_clamp (INT, _r_config_getinteger (L"IconSize", SHIL_SYSSMALL), SHIL_LARGE, SHIL_LAST);

					if (icon_size == SHIL_EXTRALARGE)
						menu_id = IDM_SIZE_EXTRALARGE;

					else if (icon_size == SHIL_LARGE)
						menu_id = IDM_SIZE_LARGE;

					else
						menu_id = IDM_SIZE_SMALL;

					_r_menu_checkitem (hmenu, IDM_SIZE_SMALL, IDM_SIZE_EXTRALARGE, MF_BYCOMMAND, menu_id);
				}

				_r_menu_checkitem (hmenu, IDM_ICONSISHIDDEN, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsIconsHidden", FALSE));

				_r_menu_checkitem (hmenu, IDM_RULE_BLOCKOUTBOUND, 0, MF_BYCOMMAND, _r_config_getboolean (L"BlockOutboundConnections", TRUE));
				_r_menu_checkitem (hmenu, IDM_RULE_BLOCKINBOUND, 0, MF_BYCOMMAND, _r_config_getboolean (L"BlockInboundConnections", TRUE));
				_r_menu_checkitem (hmenu, IDM_RULE_ALLOWLOOPBACK, 0, MF_BYCOMMAND, _r_config_getboolean (L"AllowLoopbackConnections", TRUE));
				_r_menu_checkitem (hmenu, IDM_RULE_ALLOW6TO4, 0, MF_BYCOMMAND, _r_config_getboolean (L"AllowIPv6", TRUE));

				_r_menu_checkitem (hmenu, IDM_SECUREFILTERS_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsSecureFilters", TRUE));
				_r_menu_checkitem (hmenu, IDM_USESTEALTHMODE_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"UseStealthMode", TRUE));
				_r_menu_checkitem (hmenu, IDM_INSTALLBOOTTIMEFILTERS_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"InstallBoottimeFilters", TRUE));

				_r_menu_checkitem (hmenu, IDM_USECERTIFICATES_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsCertificatesEnabled", FALSE));
				_r_menu_checkitem (hmenu, IDM_USENETWORKRESOLUTION_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsNetworkResolutionsEnabled", FALSE));
				_r_menu_checkitem (hmenu, IDM_USEREFRESHDEVICES_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsRefreshDevices", TRUE));

				_r_menu_checkitem (hmenu, IDM_BLOCKLIST_SPY_DISABLE, IDM_BLOCKLIST_SPY_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_SPY_DISABLE + _r_calc_clamp (INT, _r_config_getinteger (L"BlocklistSpyState", 2), 0, 2));
				_r_menu_checkitem (hmenu, IDM_BLOCKLIST_UPDATE_DISABLE, IDM_BLOCKLIST_UPDATE_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_UPDATE_DISABLE + _r_calc_clamp (INT, _r_config_getinteger (L"BlocklistUpdateState", 0), 0, 2));
				_r_menu_checkitem (hmenu, IDM_BLOCKLIST_EXTRA_DISABLE, IDM_BLOCKLIST_EXTRA_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_EXTRA_DISABLE + _r_calc_clamp (INT, _r_config_getinteger (L"BlocklistExtraState", 0), 0, 2));
			}

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLENOTIFICATIONS_CHK, NULL, 0, _r_config_getboolean (L"IsNotificationsEnabled", TRUE) ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED, I_IMAGENONE);
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLELOG_CHK, NULL, 0, _r_config_getboolean (L"IsLogEnabled", FALSE) ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED, I_IMAGENONE);
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLEUILOG_CHK, NULL, 0, _r_config_getboolean (L"IsLogUiEnabled", FALSE) ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED, I_IMAGENONE);

			break;
		}

		case RM_UNINITIALIZE:
		{
			_r_tray_destroy (hwnd, UID);
			break;
		}

		case RM_LOCALIZE:
		{
			HMENU hmenu = GetMenu (hwnd);

			if (hmenu)
			{
				_r_menu_setitemtext (hmenu, 0, TRUE, _r_locale_getstring (IDS_FILE));
				_r_menu_setitemtext (hmenu, 1, TRUE, _r_locale_getstring (IDS_EDIT));
				_r_menu_setitemtext (hmenu, 2, TRUE, _r_locale_getstring (IDS_VIEW));
				_r_menu_setitemtext (hmenu, 3, TRUE, _r_locale_getstring (IDS_TRAY_RULES));
				_r_menu_setitemtext (hmenu, 4, TRUE, _r_locale_getstring (IDS_TRAY_BLOCKLIST_RULES));
				_r_menu_setitemtext (hmenu, 5, TRUE, _r_locale_getstring (IDS_HELP));

				_r_menu_setitemtextformat (hmenu, IDM_SETTINGS, FALSE, L"%s...\tF2", _r_locale_getstring (IDS_SETTINGS));
				_r_menu_setitemtextformat (hmenu, IDM_ADD_FILE, FALSE, L"%s...", _r_locale_getstring (IDS_ADD_FILE));
				_r_menu_setitemtextformat (hmenu, IDM_IMPORT, FALSE, L"%s...\tCtrl+O", _r_locale_getstring (IDS_IMPORT));
				_r_menu_setitemtextformat (hmenu, IDM_EXPORT, FALSE, L"%s...\tCtrl+S", _r_locale_getstring (IDS_EXPORT));
				_r_menu_setitemtextformat (hmenu, IDM_EXIT, FALSE, _r_locale_getstring (IDS_EXIT));
				_r_menu_setitemtextformat (hmenu, IDM_PURGE_UNUSED, FALSE, L"%s\tCtrl+Shift+X", _r_locale_getstring (IDS_PURGE_UNUSED));
				_r_menu_setitemtextformat (hmenu, IDM_PURGE_TIMERS, FALSE, L"%s\tCtrl+Shift+T", _r_locale_getstring (IDS_PURGE_TIMERS));
				_r_menu_setitemtextformat (hmenu, IDM_FIND, FALSE, L"%s\tCtrl+F", _r_locale_getstring (IDS_FIND));
				_r_menu_setitemtextformat (hmenu, IDM_FINDNEXT, FALSE, L"%s\tF3", _r_locale_getstring (IDS_FINDNEXT));
				_r_menu_setitemtextformat (hmenu, IDM_REFRESH, FALSE, L"%s\tF5", _r_locale_getstring (IDS_REFRESH));

				_r_menu_setitemtext (hmenu, IDM_ALWAYSONTOP_CHK, FALSE, _r_locale_getstring (IDS_ALWAYSONTOP_CHK));
				_r_menu_setitemtext (hmenu, IDM_SHOWFILENAMESONLY_CHK, FALSE, _r_locale_getstring (IDS_SHOWFILENAMESONLY_CHK));
				_r_menu_setitemtext (hmenu, IDM_AUTOSIZECOLUMNS_CHK, FALSE, _r_locale_getstring (IDS_AUTOSIZECOLUMNS_CHK));

				_r_menu_setitemtext (GetSubMenu (hmenu, 2), 4, TRUE, _r_locale_getstring (IDS_ICONS));

				_r_menu_setitemtext (hmenu, IDM_SIZE_SMALL, FALSE, _r_locale_getstring (IDS_ICONSSMALL));
				_r_menu_setitemtext (hmenu, IDM_SIZE_LARGE, FALSE, _r_locale_getstring (IDS_ICONSLARGE));
				_r_menu_setitemtext (hmenu, IDM_SIZE_EXTRALARGE, FALSE, _r_locale_getstring (IDS_ICONSEXTRALARGE));

				_r_menu_setitemtext (hmenu, IDM_VIEW_ICON, FALSE, _r_locale_getstring (IDS_VIEW_ICON));
				_r_menu_setitemtext (hmenu, IDM_VIEW_DETAILS, FALSE, _r_locale_getstring (IDS_VIEW_DETAILS));
				_r_menu_setitemtext (hmenu, IDM_VIEW_TILE, FALSE, _r_locale_getstring (IDS_VIEW_TILE));

				_r_menu_setitemtext (hmenu, IDM_ICONSISHIDDEN, FALSE, _r_locale_getstring (IDS_ICONSISHIDDEN));

				_r_menu_setitemtextformat (GetSubMenu (hmenu, 2), LANG_MENU, TRUE, L"%s (Language)", _r_locale_getstring (IDS_LANGUAGE));

				_r_menu_setitemtextformat (hmenu, IDM_FONT, FALSE, L"%s...", _r_locale_getstring (IDS_FONT));

				LPCWSTR recommendedString = _r_locale_getstring (IDS_RECOMMENDED);

				_r_menu_setitemtext (hmenu, IDM_CONNECTIONS_TITLE, FALSE, _r_locale_getstring (IDS_TAB_NETWORK));
				_r_menu_setitemtext (hmenu, IDM_SECURITY_TITLE, FALSE, _r_locale_getstring (IDS_TITLE_SECURITY));
				_r_menu_setitemtext (hmenu, IDM_ADVANCED_TITLE, FALSE, _r_locale_getstring (IDS_TITLE_ADVANCED));

				_r_menu_enableitem (hmenu, IDM_CONNECTIONS_TITLE, MF_BYCOMMAND, FALSE);
				_r_menu_enableitem (hmenu, IDM_SECURITY_TITLE, MF_BYCOMMAND, FALSE);
				_r_menu_enableitem (hmenu, IDM_ADVANCED_TITLE, MF_BYCOMMAND, FALSE);

				_r_menu_setitemtextformat (hmenu, IDM_RULE_BLOCKOUTBOUND, FALSE, L"%s (%s)", _r_locale_getstring (IDS_RULE_BLOCKOUTBOUND), recommendedString);
				_r_menu_setitemtextformat (hmenu, IDM_RULE_BLOCKINBOUND, FALSE, L"%s (%s)", _r_locale_getstring (IDS_RULE_BLOCKINBOUND), recommendedString);
				_r_menu_setitemtextformat (hmenu, IDM_RULE_ALLOWLOOPBACK, FALSE, L"%s (%s)", _r_locale_getstring (IDS_RULE_ALLOWLOOPBACK), recommendedString);
				_r_menu_setitemtextformat (hmenu, IDM_RULE_ALLOW6TO4, FALSE, L"%s (%s)", _r_locale_getstring (IDS_RULE_ALLOW6TO4), recommendedString);
				_r_menu_setitemtextformat (hmenu, IDM_SECUREFILTERS_CHK, FALSE, L"%s (%s)", _r_locale_getstring (IDS_SECUREFILTERS_CHK), recommendedString);
				_r_menu_setitemtextformat (hmenu, IDM_USESTEALTHMODE_CHK, FALSE, L"%s (%s)", _r_locale_getstring (IDS_USESTEALTHMODE_CHK), recommendedString);
				_r_menu_setitemtextformat (hmenu, IDM_INSTALLBOOTTIMEFILTERS_CHK, FALSE, L"%s (%s)", _r_locale_getstring (IDS_INSTALLBOOTTIMEFILTERS_CHK), recommendedString);

				_r_menu_setitemtext (hmenu, IDM_USENETWORKRESOLUTION_CHK, FALSE, _r_locale_getstring (IDS_USENETWORKRESOLUTION_CHK));
				_r_menu_setitemtext (hmenu, IDM_USECERTIFICATES_CHK, FALSE, _r_locale_getstring (IDS_USECERTIFICATES_CHK));
				_r_menu_setitemtextformat (hmenu, IDM_USEREFRESHDEVICES_CHK, FALSE, L"%s (%s)", _r_locale_getstring (IDS_USEREFRESHDEVICES_CHK), recommendedString);

				_r_menu_setitemtext (hmenu, IDM_BLOCKLIST_SPY_TITLE, FALSE, _r_locale_getstring (IDS_BLOCKLIST_SPY));
				_r_menu_setitemtext (hmenu, IDM_BLOCKLIST_UPDATE_TITLE, FALSE, _r_locale_getstring (IDS_BLOCKLIST_UPDATE));
				_r_menu_setitemtext (hmenu, IDM_BLOCKLIST_EXTRA_TITLE, FALSE, _r_locale_getstring (IDS_BLOCKLIST_EXTRA));

				_r_menu_enableitem (hmenu, IDM_BLOCKLIST_SPY_TITLE, MF_BYCOMMAND, FALSE);
				_r_menu_enableitem (hmenu, IDM_BLOCKLIST_UPDATE_TITLE, MF_BYCOMMAND, FALSE);
				_r_menu_enableitem (hmenu, IDM_BLOCKLIST_EXTRA_TITLE, MF_BYCOMMAND, FALSE);

				_r_menu_setitemtext (hmenu, IDM_BLOCKLIST_SPY_DISABLE, FALSE, _r_locale_getstring (IDS_DISABLE));
				_r_menu_setitemtext (hmenu, IDM_BLOCKLIST_SPY_ALLOW, FALSE, _r_locale_getstring (IDS_ACTION_ALLOW));
				_r_menu_setitemtextformat (hmenu, IDM_BLOCKLIST_SPY_BLOCK, FALSE, L"%s (%s)", _r_locale_getstring (IDS_ACTION_BLOCK), recommendedString);
				_r_menu_setitemtextformat (hmenu, IDM_BLOCKLIST_UPDATE_DISABLE, FALSE, L"%s (%s)", _r_locale_getstring (IDS_DISABLE), recommendedString);

				_r_menu_setitemtext (hmenu, IDM_BLOCKLIST_UPDATE_ALLOW, FALSE, _r_locale_getstring (IDS_ACTION_ALLOW));
				_r_menu_setitemtext (hmenu, IDM_BLOCKLIST_UPDATE_BLOCK, FALSE, _r_locale_getstring (IDS_ACTION_BLOCK));

				_r_menu_setitemtextformat (hmenu, IDM_BLOCKLIST_EXTRA_DISABLE, FALSE, L"%s (%s)", _r_locale_getstring (IDS_DISABLE), recommendedString);

				_r_menu_setitemtext (hmenu, IDM_BLOCKLIST_EXTRA_ALLOW, FALSE, _r_locale_getstring (IDS_ACTION_ALLOW));
				_r_menu_setitemtext (hmenu, IDM_BLOCKLIST_EXTRA_BLOCK, FALSE, _r_locale_getstring (IDS_ACTION_BLOCK));

				_r_menu_setitemtext (hmenu, IDM_WEBSITE, FALSE, _r_locale_getstring (IDS_WEBSITE));
				_r_menu_setitemtext (hmenu, IDM_CHECKUPDATES, FALSE, _r_locale_getstring (IDS_CHECKUPDATES));

				_r_menu_setitemtextformat (hmenu, IDM_ABOUT, FALSE, L"%s\tF1", _r_locale_getstring (IDS_ABOUT));

				_r_locale_enum ((HWND)GetSubMenu (hmenu, 2), LANG_MENU, IDX_LANGUAGE); // enum localizations
			}

			PR_STRING localizedString = NULL;

			// localize toolbar
			_app_setinterfacestate (hwnd);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_REFRESH, _r_locale_getstring (IDS_REFRESH), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, 0, I_IMAGENONE);
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_SETTINGS, _r_locale_getstring (IDS_SETTINGS), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, 0, I_IMAGENONE);

			_r_obj_movereference (&localizedString, _r_format_string (L"%s...", _r_locale_getstring (IDS_OPENRULESEDITOR)));
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_OPENRULESEDITOR, _r_obj_getstringorempty (localizedString), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLENOTIFICATIONS_CHK, _r_locale_getstring (IDS_ENABLENOTIFICATIONS_CHK), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLELOG_CHK, _r_locale_getstring (IDS_ENABLELOG_CHK), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);

			_r_obj_movereference (&localizedString, _r_format_string (L"%s (session only)", _r_locale_getstring (IDS_ENABLEUILOG_CHK)));
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLEUILOG_CHK, _r_obj_getstringorempty (localizedString), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);

			_r_obj_movereference (&localizedString, _r_format_string (L"%s (Ctrl+I)", _r_locale_getstring (IDS_LOGSHOW)));
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_LOGSHOW, _r_obj_getstringorempty (localizedString), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

			_r_obj_movereference (&localizedString, _r_format_string (L"%s (Ctrl+X)", _r_locale_getstring (IDS_LOGCLEAR)));
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_LOGCLEAR, _r_obj_getstringorempty (localizedString), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_DONATE, _r_locale_getstring (IDS_DONATE), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

			// set rebar size
			_app_toolbar_resize ();

			// localize tabs
			for (INT i = 0; i < (INT)SendDlgItemMessage (hwnd, IDC_TAB, TCM_GETITEMCOUNT, 0, 0); i++)
			{
				INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, i);

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

				else if (listview_id == IDC_LOG)
					locale_id = IDS_TITLE_LOGGING;

				else
					continue;

				_r_tab_setitem (hwnd, IDC_TAB, i, _r_locale_getstring (locale_id), I_IMAGENONE, 0);

				if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
				{
					_r_listview_setcolumn (hwnd, listview_id, 0, _r_locale_getstring (IDS_NAME), 0);
					_r_listview_setcolumn (hwnd, listview_id, 1, _r_locale_getstring (IDS_ADDED), 0);
				}
				else if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM)
				{
					_r_listview_setcolumn (hwnd, listview_id, 0, _r_locale_getstring (IDS_NAME), 0);
					_r_listview_setcolumn (hwnd, listview_id, 1, _r_locale_getstring (IDS_PROTOCOL), 0);
					_r_listview_setcolumn (hwnd, listview_id, 2, _r_locale_getstring (IDS_DIRECTION), 0);
				}
				else if (listview_id == IDC_NETWORK)
				{
					_r_listview_setcolumn (hwnd, listview_id, 0, _r_locale_getstring (IDS_NAME), 0);

					_r_obj_movereference (&localizedString, _r_format_string (L"%s (" SZ_DIRECTION_LOCAL L")", _r_locale_getstring (IDS_ADDRESS)));
					_r_listview_setcolumn (hwnd, listview_id, 1, _r_obj_getstringorempty (localizedString), 0);

					_r_obj_movereference (&localizedString, _r_format_string (L"%s (" SZ_DIRECTION_LOCAL L")", _r_locale_getstring (IDS_PORT)));
					_r_listview_setcolumn (hwnd, listview_id, 2, _r_obj_getstringorempty (localizedString), 0);

					_r_obj_movereference (&localizedString, _r_format_string (L"%s (" SZ_DIRECTION_REMOTE L")", _r_locale_getstring (IDS_ADDRESS)));
					_r_listview_setcolumn (hwnd, listview_id, 3, _r_obj_getstringorempty (localizedString), 0);

					_r_obj_movereference (&localizedString, _r_format_string (L"%s (" SZ_DIRECTION_REMOTE L")", _r_locale_getstring (IDS_PORT)));
					_r_listview_setcolumn (hwnd, listview_id, 4, _r_obj_getstringorempty (localizedString), 0);

					_r_listview_setcolumn (hwnd, listview_id, 5, _r_locale_getstring (IDS_PROTOCOL), 0);
					_r_listview_setcolumn (hwnd, listview_id, 6, _r_locale_getstring (IDS_STATE), 0);
				}
				else if (listview_id == IDC_LOG)
				{
					_r_listview_setcolumn (hwnd, listview_id, 0, _r_locale_getstring (IDS_NAME), 0);

					_r_obj_movereference (&localizedString, _r_format_string (L"%s (" SZ_DIRECTION_LOCAL L")", _r_locale_getstring (IDS_ADDRESS)));
					_r_listview_setcolumn (hwnd, listview_id, 1, _r_obj_getstringorempty (localizedString), 0);

					_r_obj_movereference (&localizedString, _r_format_string (L"%s (" SZ_DIRECTION_LOCAL L")", _r_locale_getstring (IDS_PORT)));
					_r_listview_setcolumn (hwnd, listview_id, 2, _r_obj_getstringorempty (localizedString), 0);

					_r_obj_movereference (&localizedString, _r_format_string (L"%s (" SZ_DIRECTION_REMOTE L")", _r_locale_getstring (IDS_ADDRESS)));
					_r_listview_setcolumn (hwnd, listview_id, 3, _r_obj_getstringorempty (localizedString), 0);

					_r_obj_movereference (&localizedString, _r_format_string (L"%s (" SZ_DIRECTION_REMOTE L")", _r_locale_getstring (IDS_PORT)));
					_r_listview_setcolumn (hwnd, listview_id, 4, _r_obj_getstringorempty (localizedString), 0);

					_r_listview_setcolumn (hwnd, listview_id, 5, _r_locale_getstring (IDS_PROTOCOL), 0);
					_r_listview_setcolumn (hwnd, listview_id, 6, _r_locale_getstring (IDS_FILTER), 0);
					_r_listview_setcolumn (hwnd, listview_id, 7, _r_locale_getstring (IDS_DIRECTION), 0);
					_r_listview_setcolumn (hwnd, listview_id, 8, _r_locale_getstring (IDS_STATE), 0);
				}

				SendDlgItemMessage (hwnd, listview_id, LVM_RESETEMPTYTEXT, 0, 0);
			}

			INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

			if (listview_id)
			{
				_app_listviewresize (hwnd, listview_id, FALSE);
				_app_refreshstatus (hwnd, listview_id);
			}

			// refresh notification
			BOOLEAN is_classic = _r_app_isclassicui ();

			_r_wnd_addstyle (config.hnotification, IDC_RULES_BTN, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (config.hnotification, IDC_ALLOW_BTN, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (config.hnotification, IDC_BLOCK_BTN, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (config.hnotification, IDC_LATER_BTN, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

			_app_notifyrefresh (config.hnotification, FALSE);

			SAFE_DELETE_REFERENCE (localizedString);

			break;
		}

		case RM_TASKBARCREATED:
		{
			// refresh tray icon
			_r_tray_destroy (hwnd, UID);
			_r_tray_create (hwnd, UID, WM_TRAYICON, _r_app_getsharedimage (_r_sys_getimagebase (), (_wfp_isfiltersinstalled () != InstallDisabled) ? IDI_ACTIVE : IDI_INACTIVE, _r_dc_getsystemmetrics (hwnd, SM_CXSMICON)), APP_NAME, FALSE);

			break;
		}

		case RM_DPICHANGED:
		{
			PR_STRING localizedString = NULL;

			_app_imagelist_init (hwnd);

			SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_SETIMAGELIST, 0, (LPARAM)config.himg_toolbar);

			// reset toolbar information
			_app_setinterfacestate (hwnd);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_REFRESH, _r_locale_getstring (IDS_REFRESH), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, 0, I_IMAGENONE);
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_SETTINGS, _r_locale_getstring (IDS_SETTINGS), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, 0, I_IMAGENONE);

			_r_obj_movereference (&localizedString, _r_format_string (L"%s...", _r_locale_getstring (IDS_OPENRULESEDITOR)));
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_OPENRULESEDITOR, _r_obj_getstringorempty (localizedString), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLENOTIFICATIONS_CHK, _r_locale_getstring (IDS_ENABLENOTIFICATIONS_CHK), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLELOG_CHK, _r_locale_getstring (IDS_ENABLELOG_CHK), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLEUILOG_CHK, _r_locale_getstring (IDS_ENABLEUILOG_CHK), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);

			_r_obj_movereference (&localizedString, _r_format_string (L"%s (Ctrl+I)", _r_locale_getstring (IDS_LOGSHOW)));
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_LOGSHOW, _r_obj_getstringorempty (localizedString), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

			_r_obj_movereference (&localizedString, _r_format_string (L"%s (Ctrl+X)", _r_locale_getstring (IDS_LOGCLEAR)));
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_LOGCLEAR, _r_obj_getstringorempty (localizedString), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_DONATE, _r_locale_getstring (IDS_DONATE), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

			_app_toolbar_resize ();

			INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

			if (listview_id)
			{
				_app_listviewsetview (hwnd, listview_id);
				_app_listviewsetfont (hwnd, listview_id, TRUE);
				_app_listviewresize (hwnd, listview_id, FALSE);
			}

			_app_refreshstatus (hwnd, 0);

			SAFE_DELETE_REFERENCE (localizedString);

			break;
		}

		case RM_CONFIG_UPDATE:
		{
			_app_profile_save ();
			_app_profile_load (hwnd, NULL);

			_app_refreshstatus (hwnd, INVALID_INT);

			_app_changefilters (hwnd, TRUE, FALSE);

			break;
		}

		case RM_CONFIG_RESET:
		{
			time_t current_timestamp = (time_t)lparam;

			_app_freerulesconfig_map (&rules_config);

			_r_fs_makebackup (config.profile_path, current_timestamp, TRUE);

			_app_profile_load (hwnd, NULL);

			_app_refreshstatus (hwnd, INVALID_INT);

			_app_changefilters (hwnd, TRUE, FALSE);

			break;
		}

		case WM_CLOSE:
		{
			if (_wfp_isfiltersinstalled ())
			{
				if (_app_istimersactive () ?
					!_r_show_confirmmessage (hwnd, NULL, _r_locale_getstring (IDS_QUESTION_TIMER), L"ConfirmExitTimer") :
					!_r_show_confirmmessage (hwnd, NULL, _r_locale_getstring (IDS_QUESTION_EXIT), L"ConfirmExit2"))
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
			_r_config_setinteger (L"CurrentTab", (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT));

			if (config.hnotification)
				DestroyWindow (config.hnotification);

			if (config.htimer)
				DeleteTimerQueue (config.htimer);

			_r_tray_destroy (hwnd, UID);

			_app_loginit (FALSE);
			_app_freelogstack ();

			if (config.done_evt)
			{
				if (_wfp_isfiltersapplying ())
					WaitForSingleObjectEx (config.done_evt, FILTERS_TIMEOUT, FALSE);

				CloseHandle (config.done_evt);
			}

			HANDLE hengine = _wfp_getenginehandle ();

			if (hengine)
				_wfp_uninitialize (hengine, FALSE);

			ImageList_Destroy (config.himg_toolbar);
			ImageList_Destroy (config.himg_rules_small);
			ImageList_Destroy (config.himg_rules_large);

			BufferedPaintUnInit ();

			PostQuitMessage (0);

			break;
		}

		case WM_DROPFILES:
		{
			PR_STRING string;
			UINT numfiles;
			UINT length;
			SIZE_T app_hash = 0;

			numfiles = DragQueryFile ((HDROP)wparam, UINT32_MAX, NULL, 0);

			for (UINT i = 0; i < numfiles; i++)
			{
				length = DragQueryFile ((HDROP)wparam, i, NULL, 0);
				string = _r_obj_createstringex (NULL, length * sizeof (WCHAR));

				if (DragQueryFile ((HDROP)wparam, i, string->Buffer, length + 1))
					app_hash = _app_addapplication (hwnd, string->Buffer, 0, 0, 0, FALSE, FALSE);

				_r_obj_dereference (string);
			}

			DragFinish ((HDROP)wparam);

			_app_profile_save ();
			_app_refreshstatus (hwnd, INVALID_INT);

			{
				INT app_listview_id = (INT)_app_getappinfo (app_hash, InfoListviewId);

				if (app_listview_id)
				{
					if (app_listview_id == (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT))
						_app_listviewsort (hwnd, app_listview_id, INVALID_INT, FALSE);

					_app_showitem (hwnd, app_listview_id, _app_getposition (hwnd, app_listview_id, app_hash), INVALID_INT);
				}
			}

			break;
		}

		case WM_SETTINGCHANGE:
		{
			_app_notifyrefresh (config.hnotification, FALSE);
			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lparam;

			switch (nmlp->code)
			{
				case TCN_SELCHANGING:
				{
					INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

					if (!listview_id)
						break;

					HWND hlistview = GetDlgItem (hwnd, listview_id);

					if (!hlistview)
						break;

					ShowWindow (hlistview, SW_HIDE);

					break;
				}

				case TCN_SELCHANGE:
				{
					INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

					if (!listview_id)
						break;

					HWND hlistview = GetDlgItem (hwnd, listview_id);

					if (!hlistview)
						break;

					_r_tab_adjustchild (hwnd, IDC_TAB, hlistview);

					_app_listviewsetview (hwnd, listview_id);
					_app_listviewsetfont (hwnd, listview_id, FALSE);

					_app_listviewsort (hwnd, listview_id, INVALID_INT, FALSE);

					_app_listviewresize (hwnd, listview_id, FALSE);
					_app_refreshstatus (hwnd, listview_id);

					_r_listview_redraw (hwnd, listview_id, INVALID_INT);

					ShowWindow (hlistview, SW_SHOWNA);

					if (IsWindowVisible (hwnd) && !IsIconic (hwnd)) // HACK!!!
						SetFocus (hlistview);

					break;
				}

				case NM_CUSTOMDRAW:
				{
					LONG_PTR result = CDRF_DODEFAULT;
					LPNMLVCUSTOMDRAW lpnmcd = (LPNMLVCUSTOMDRAW)lparam;

					INT ctrl_id = PtrToInt ((PVOID)lpnmcd->nmcd.hdr.idFrom);

					if (ctrl_id == IDC_TOOLBAR)
					{
						result = _app_nmcustdraw_toolbar (lpnmcd);
					}
					else
					{
						result = _app_nmcustdraw_listview (lpnmcd);
					}

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, result);
					return result;
				}

				case LVN_COLUMNCLICK:
				{
					LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lparam;

					INT ctrl_id = PtrToInt ((PVOID)lpnmlv->hdr.idFrom);

					_app_listviewsort (hwnd, ctrl_id, lpnmlv->iSubItem, TRUE);

					break;
				}

				case LVN_GETINFOTIP:
				{
					LPNMLVGETINFOTIP lpnmlv = (LPNMLVGETINFOTIP)lparam;
					PR_STRING string = _app_gettooltip (hwnd, (INT)lpnmlv->hdr.idFrom, lpnmlv->iItem);

					if (string)
					{
						_r_str_copy (lpnmlv->pszText, lpnmlv->cchTextMax, string->Buffer);
						_r_obj_dereference (string);
					}

					break;
				}

				case LVN_ITEMCHANGED:
				{
					LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lparam;

					if ((lpnmlv->uChanged & LVIF_STATE) != 0)
					{
						if ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (1) || ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (2)))
						{
							if (_r_fastlock_islocked (&lock_checkbox))
								break;

							INT listview_id = PtrToInt ((PVOID)lpnmlv->hdr.idFrom);
							BOOLEAN is_changed = FALSE;

							BOOLEAN is_enabled = (lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (2);

							if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
							{
								SIZE_T app_hash = lpnmlv->lParam;
								PITEM_APP ptr_app = _app_getappitem (app_hash);

								if (!ptr_app)
									break;

								OBJECTS_APP_VECTOR rules;

								if (ptr_app->is_enabled != is_enabled)
								{
									ptr_app->is_enabled = is_enabled;

									_r_fastlock_acquireshared (&lock_checkbox);
									_app_setappiteminfo (hwnd, listview_id, lpnmlv->iItem, app_hash, ptr_app);
									_r_fastlock_releaseshared (&lock_checkbox);

									if (is_enabled)
										_app_freenotify (app_hash, ptr_app);

									if (!is_enabled && _app_istimeractive (ptr_app))
										_app_timer_reset (hwnd, ptr_app);

									rules.emplace_back (ptr_app);

									if (_wfp_isfiltersinstalled ())
									{
										HANDLE hengine = _wfp_getenginehandle ();

										if (hengine)
											_wfp_create3filters (hengine, &rules, __LINE__);
									}

									is_changed = TRUE;
								}

								_r_obj_dereference (ptr_app);
							}
							else if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM)
							{
								OBJECTS_RULE_VECTOR rules;

								SIZE_T rule_idx = lpnmlv->lParam;
								PITEM_RULE ptr_rule = _app_getrulebyid (rule_idx);

								if (!ptr_rule)
									break;

								if (ptr_rule->is_enabled != is_enabled)
								{
									_r_fastlock_acquireshared (&lock_checkbox);

									_app_ruleenable (ptr_rule, is_enabled);
									_app_setruleiteminfo (hwnd, listview_id, lpnmlv->iItem, ptr_rule, TRUE);

									_r_fastlock_releaseshared (&lock_checkbox);

									rules.emplace_back (ptr_rule);

									if (_wfp_isfiltersinstalled ())
									{
										HANDLE hengine = _wfp_getenginehandle ();

										if (hengine)
											_wfp_create4filters (hengine, &rules, __LINE__);
									}

									is_changed = TRUE;
								}

								_r_obj_dereference (ptr_rule);
							}

							if (is_changed)
							{
								_app_listviewsort (hwnd, listview_id, INVALID_INT, FALSE);
								_app_refreshstatus (hwnd, listview_id);

								_app_profile_save ();
							}
						}
					}

					break;
				}

				case LVN_GETEMPTYMARKUP:
				{
					NMLVEMPTYMARKUP* lpnmlv = (NMLVEMPTYMARKUP*)lparam;

					lpnmlv->dwFlags = EMF_CENTERED;
					_r_str_copy (lpnmlv->szMarkup, RTL_NUMBER_OF (lpnmlv->szMarkup), _r_locale_getstring (IDS_STATUS_EMPTY));

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
					return TRUE;
				}

				case NM_DBLCLK:
				{
					LPNMITEMACTIVATE lpnmlv = (LPNMITEMACTIVATE)lparam;

					if (lpnmlv->iItem == INVALID_INT)
						break;

					INT command_id = 0;
					INT ctrl_id = PtrToInt ((PVOID)lpnmlv->hdr.idFrom);

					if (ctrl_id >= IDC_APPS_PROFILE && ctrl_id <= IDC_LOG)
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

					INT listview_id = PtrToInt ((PVOID)lpnmlv->hdr.idFrom);

					if (!(listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_LOG))
						break;

					HMENU hmenu = CreatePopupMenu ();

					if (!hmenu)
						break;

					PR_STRING localizedString = NULL;
					PR_STRING columnText = NULL;

					HMENU hsubmenu1 = NULL;
					HMENU hsubmenu2 = NULL;

					SIZE_T hash_item = _r_listview_getitemlparam (hwnd, listview_id, lpnmlv->iItem);
					INT lv_column_current = lpnmlv->iSubItem;

					if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
					{
						hsubmenu1 = CreatePopupMenu ();
						hsubmenu2 = CreatePopupMenu ();

						// show rules
						AppendMenu (hsubmenu1, MF_STRING, IDM_DISABLENOTIFICATIONS, _r_locale_getstring (IDS_DISABLENOTIFICATIONS));

						_app_generate_rulesmenu (hsubmenu1, hash_item);

						// show timers
						AppendMenu (hsubmenu2, MF_STRING, IDM_DISABLETIMER, _r_locale_getstring (IDS_DISABLETIMER));
						AppendMenu (hsubmenu2, MF_SEPARATOR, 0, NULL);

						_app_generate_timermenu (hsubmenu2, hash_item);

						_r_obj_movereference (&localizedString, _r_format_string (L"%s\tEnter", _r_locale_getstring (IDS_EXPLORE)));
						AppendMenu (hmenu, MF_STRING, IDM_PROPERTIES, _r_obj_getstringorempty (localizedString));

						AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);
						AppendMenu (hmenu, MF_POPUP, (UINT_PTR)hsubmenu1, _r_locale_getstring (IDS_TRAY_RULES));
						AppendMenu (hmenu, MF_POPUP, (UINT_PTR)hsubmenu2, _r_locale_getstring (IDS_TIMER));
						AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

						_r_obj_movereference (&localizedString, _r_format_string (L"%s\tDel", _r_locale_getstring (IDS_DELETE)));
						AppendMenu (hmenu, MF_STRING, IDM_DELETE, _r_obj_getstringorempty (localizedString));

						AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);
						AppendMenu (hmenu, MF_STRING, IDM_CHECK, _r_locale_getstring (IDS_CHECK));
						AppendMenu (hmenu, MF_STRING, IDM_UNCHECK, _r_locale_getstring (IDS_UNCHECK));
						AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

						_r_obj_movereference (&localizedString, _r_format_string (L"%s\tCtrl+A", _r_locale_getstring (IDS_SELECT_ALL)));
						AppendMenu (hmenu, MF_STRING, IDM_SELECT_ALL, _r_obj_getstringorempty (localizedString));

						AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

						_r_obj_movereference (&localizedString, _r_format_string (L"%s\tCtrl+C", _r_locale_getstring (IDS_COPY)));
						AppendMenu (hmenu, MF_STRING, IDM_COPY, _r_obj_getstringorempty (localizedString));

						columnText = _r_listview_getcolumntext (hwnd, listview_id, lv_column_current);

						if (columnText)
						{
							_r_obj_movereference (&localizedString, _r_format_string (L"%s \"%s\"", _r_locale_getstring (IDS_COPY), _r_obj_getstringorempty (columnText)));
							AppendMenu (hmenu, MF_STRING, IDM_COPY2, _r_obj_getstringorempty (localizedString));

							_r_obj_dereference (columnText);
						}

						SetMenuDefaultItem (hmenu, IDM_PROPERTIES, MF_BYCOMMAND);

						_r_menu_checkitem (hmenu, IDM_DISABLENOTIFICATIONS, 0, MF_BYCOMMAND, _app_getappinfo (hash_item, InfoIsSilent) != FALSE);

						if (listview_id != IDC_APPS_PROFILE)
							_r_menu_enableitem (hmenu, IDM_DELETE, MF_BYCOMMAND, FALSE);
					}
					else if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM)
					{
						if (listview_id == IDC_RULES_CUSTOM)
						{
							_r_obj_movereference (&localizedString, _r_format_string (L"%s...", _r_locale_getstring (IDS_ADD)));
							AppendMenu (hmenu, MF_STRING, IDM_OPENRULESEDITOR, _r_obj_getstringorempty (localizedString));
						}

						_r_obj_movereference (&localizedString, _r_format_string (L"%s...\tEnter", _r_locale_getstring (IDS_EDIT2)));
						AppendMenu (hmenu, MF_STRING, IDM_PROPERTIES, _r_obj_getstringorempty (localizedString));

						if (listview_id == IDC_RULES_CUSTOM)
						{
							_r_obj_movereference (&localizedString, _r_format_string (L"%s\tDel", _r_locale_getstring (IDS_DELETE)));
							AppendMenu (hmenu, MF_STRING, IDM_DELETE, _r_obj_getstringorempty (localizedString));

							PITEM_RULE ptr_rule = _app_getrulebyid (hash_item);

							if (ptr_rule)
							{
								if (ptr_rule->is_readonly)
									_r_menu_enableitem (hmenu, IDM_DELETE, MF_BYCOMMAND, FALSE);

								_r_obj_dereference (ptr_rule);
							}
						}

						AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);
						AppendMenu (hmenu, MF_STRING, IDM_CHECK, _r_locale_getstring (IDS_CHECK));
						AppendMenu (hmenu, MF_STRING, IDM_UNCHECK, _r_locale_getstring (IDS_UNCHECK));
						AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

						_r_obj_movereference (&localizedString, _r_format_string (L"%s\tCtrl+A", _r_locale_getstring (IDS_SELECT_ALL)));
						AppendMenu (hmenu, MF_STRING, IDM_SELECT_ALL, _r_obj_getstringorempty (localizedString));

						AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

						_r_obj_movereference (&localizedString, _r_format_string (L"%s\tCtrl+C", _r_locale_getstring (IDS_COPY)));
						AppendMenu (hmenu, MF_STRING, IDM_COPY, _r_obj_getstringorempty (localizedString));

						columnText = _r_listview_getcolumntext (hwnd, listview_id, lv_column_current);

						if (columnText)
						{
							_r_obj_movereference (&localizedString, _r_format_string (L"%s \"%s\"", _r_locale_getstring (IDS_COPY), _r_obj_getstringorempty (columnText)));
							AppendMenu (hmenu, MF_STRING, IDM_COPY2, _r_obj_getstringorempty (localizedString));

							_r_obj_dereference (columnText);
						}

						SetMenuDefaultItem (hmenu, IDM_PROPERTIES, MF_BYCOMMAND);
					}
					else if (listview_id == IDC_NETWORK)
					{
						_r_obj_movereference (&localizedString, _r_format_string (L"%s\tEnter", _r_locale_getstring (IDS_SHOWINLIST)));
						AppendMenu (hmenu, MF_STRING, IDM_PROPERTIES, _r_obj_getstringorempty (localizedString));

						_r_obj_movereference (&localizedString, _r_format_string (L"%s...", _r_locale_getstring (IDS_OPENRULESEDITOR)));
						AppendMenu (hmenu, MF_STRING, IDM_OPENRULESEDITOR, _r_obj_getstringorempty (localizedString));

						AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);
						AppendMenu (hmenu, MF_STRING, IDM_NETWORK_CLOSE, _r_locale_getstring (IDS_NETWORK_CLOSE));
						AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

						_r_obj_movereference (&localizedString, _r_format_string (L"%s\tCtrl+A", _r_locale_getstring (IDS_SELECT_ALL)));
						AppendMenu (hmenu, MF_STRING, IDM_SELECT_ALL, _r_obj_getstringorempty (localizedString));

						AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

						_r_obj_movereference (&localizedString, _r_format_string (L"%s\tCtrl+C", _r_locale_getstring (IDS_COPY)));
						AppendMenu (hmenu, MF_STRING, IDM_COPY, _r_obj_getstringorempty (localizedString));

						columnText = _r_listview_getcolumntext (hwnd, listview_id, lv_column_current);

						if (columnText)
						{
							_r_obj_movereference (&localizedString, _r_format_string (L"%s \"%s\"", _r_locale_getstring (IDS_COPY), _r_obj_getstringorempty (columnText)));
							AppendMenu (hmenu, MF_STRING, IDM_COPY2, _r_obj_getstringorempty (localizedString));

							_r_obj_dereference (columnText);
						}

						SetMenuDefaultItem (hmenu, IDM_PROPERTIES, MF_BYCOMMAND);

						PITEM_NETWORK ptr_network = _app_getnetworkitem (hash_item);

						if (ptr_network)
						{
							if (ptr_network->af != AF_INET || ptr_network->state != MIB_TCP_STATE_ESTAB)
								_r_menu_enableitem (hmenu, IDM_NETWORK_CLOSE, MF_BYCOMMAND, FALSE);

							_r_obj_dereference (ptr_network);
						}

					}
					else if (listview_id == IDC_LOG)
					{
						_r_obj_movereference (&localizedString, _r_format_string (L"%s\tEnter", _r_locale_getstring (IDS_SHOWINLIST)));
						AppendMenu (hmenu, MF_STRING, IDM_PROPERTIES, _r_obj_getstringorempty (localizedString));

						_r_obj_movereference (&localizedString, _r_format_string (L"%s...", _r_locale_getstring (IDS_OPENRULESEDITOR)));
						AppendMenu (hmenu, MF_STRING, IDM_OPENRULESEDITOR, _r_obj_getstringorempty (localizedString));

						_r_obj_movereference (&localizedString, _r_format_string (L"%s\tCtrl+X", _r_locale_getstring (IDS_LOGCLEAR)));
						AppendMenu (hmenu, MF_STRING, IDM_TRAY_LOGCLEAR, _r_obj_getstringorempty (localizedString));

						AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

						_r_obj_movereference (&localizedString, _r_format_string (L"%s\tCtrl+A", _r_locale_getstring (IDS_SELECT_ALL)));
						AppendMenu (hmenu, MF_STRING, IDM_SELECT_ALL, _r_obj_getstringorempty (localizedString));

						AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

						_r_obj_movereference (&localizedString, _r_format_string (L"%s\tCtrl+C", _r_locale_getstring (IDS_COPY)));
						AppendMenu (hmenu, MF_STRING, IDM_COPY, _r_obj_getstringorempty (localizedString));

						columnText = _r_listview_getcolumntext (hwnd, listview_id, lv_column_current);

						if (columnText)
						{
							_r_obj_movereference (&localizedString, _r_format_string (L"%s \"%s\"", _r_locale_getstring (IDS_COPY), _r_obj_getstringorempty (columnText)));
							AppendMenu (hmenu, MF_STRING, IDM_COPY2, _r_obj_getstringorempty (localizedString));

							_r_obj_dereference (columnText);
						}

						SetMenuDefaultItem (hmenu, IDM_PROPERTIES, MF_BYCOMMAND);
					}

					INT command_id = _r_menu_popup (hmenu, hwnd, NULL, FALSE);

					if (hsubmenu1)
						DestroyMenu (hsubmenu1);

					if (hsubmenu2)
						DestroyMenu (hsubmenu2);

					DestroyMenu (hmenu);

					if (command_id)
						PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (command_id, 0), (LPARAM)lv_column_current);

					SAFE_DELETE_REFERENCE (localizedString);

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
					PITEM_LOG ptr_log = _app_notifyget_obj (_app_notifyget_id (config.hnotification, FALSE));

					if (ptr_log)
					{
						_app_notifyshow (config.hnotification, ptr_log, TRUE, FALSE);

						_r_obj_dereference (ptr_log);
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
					_r_wnd_toggle (hwnd, FALSE);
					break;
				}

				case WM_CONTEXTMENU:
				{
					SetForegroundWindow (hwnd); // don't touch

#define NOTIFICATIONS_ID 4
#define LOGGING_ID 5
#define ERRLOG_ID 6
					HMENU hmenu = LoadMenu (NULL, MAKEINTRESOURCE (IDM_TRAY));
					HMENU hsubmenu = GetSubMenu (hmenu, 0);

					BOOLEAN is_filtersinstalled = (_wfp_isfiltersinstalled () != InstallDisabled);

					_r_menu_setitembitmap (hsubmenu, IDM_TRAY_START, FALSE, is_filtersinstalled ? config.hbmp_disable : config.hbmp_enable);

					// localize
					_r_menu_setitemtext (hsubmenu, IDM_TRAY_SHOW, FALSE, _r_locale_getstring (IDS_TRAY_SHOW));
					_r_menu_setitemtext (hsubmenu, IDM_TRAY_START, FALSE, _r_locale_getstring (is_filtersinstalled ? IDS_TRAY_STOP : IDS_TRAY_START));

					_r_menu_setitemtext (hsubmenu, NOTIFICATIONS_ID, TRUE, _r_locale_getstring (IDS_TITLE_NOTIFICATIONS));
					_r_menu_setitemtext (hsubmenu, LOGGING_ID, TRUE, _r_locale_getstring (IDS_TITLE_LOGGING));

					_r_menu_setitemtext (hsubmenu, IDM_TRAY_ENABLENOTIFICATIONS_CHK, FALSE, _r_locale_getstring (IDS_ENABLENOTIFICATIONS_CHK));
					_r_menu_setitemtext (hsubmenu, IDM_TRAY_ENABLENOTIFICATIONSSOUND_CHK, FALSE, _r_locale_getstring (IDS_NOTIFICATIONSOUND_CHK));
					_r_menu_setitemtext (hsubmenu, IDM_TRAY_NOTIFICATIONFULLSCREENSILENTMODE_CHK, FALSE, _r_locale_getstring (IDS_NOTIFICATIONFULLSCREENSILENTMODE_CHK));
					_r_menu_setitemtext (hsubmenu, IDM_TRAY_NOTIFICATIONONTRAY_CHK, FALSE, _r_locale_getstring (IDS_NOTIFICATIONONTRAY_CHK));

					_r_menu_setitemtext (hsubmenu, IDM_TRAY_ENABLELOG_CHK, FALSE, _r_locale_getstring (IDS_ENABLELOG_CHK));

					_r_menu_setitemtextformat (hsubmenu, IDM_TRAY_ENABLEUILOG_CHK, FALSE, L"%s (session only)", _r_locale_getstring (IDS_ENABLEUILOG_CHK));

					_r_menu_setitemtext (hsubmenu, IDM_TRAY_LOGSHOW, FALSE, _r_locale_getstring (IDS_LOGSHOW));
					_r_menu_setitemtext (hsubmenu, IDM_TRAY_LOGCLEAR, FALSE, _r_locale_getstring (IDS_LOGCLEAR));

					if (_r_fs_exists (_r_app_getlogpath ()))
					{
						_r_menu_setitemtext (hsubmenu, ERRLOG_ID, TRUE, _r_locale_getstring (IDS_TRAY_LOGERR));

						_r_menu_setitemtext (hsubmenu, IDM_TRAY_LOGSHOW_ERR, FALSE, _r_locale_getstring (IDS_LOGSHOW));
						_r_menu_setitemtext (hsubmenu, IDM_TRAY_LOGCLEAR_ERR, FALSE, _r_locale_getstring (IDS_LOGCLEAR));
					}
					else
					{
						DeleteMenu (hsubmenu, ERRLOG_ID, MF_BYPOSITION);
					}

					_r_menu_setitemtextformat (hsubmenu, IDM_TRAY_SETTINGS, FALSE, L"%s...", _r_locale_getstring (IDS_SETTINGS));

					_r_menu_setitemtext (hsubmenu, IDM_TRAY_WEBSITE, FALSE, _r_locale_getstring (IDS_WEBSITE));
					_r_menu_setitemtext (hsubmenu, IDM_TRAY_ABOUT, FALSE, _r_locale_getstring (IDS_ABOUT));
					_r_menu_setitemtext (hsubmenu, IDM_TRAY_EXIT, FALSE, _r_locale_getstring (IDS_EXIT));

					_r_menu_checkitem (hsubmenu, IDM_TRAY_ENABLENOTIFICATIONS_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsNotificationsEnabled", TRUE));
					_r_menu_checkitem (hsubmenu, IDM_TRAY_ENABLENOTIFICATIONSSOUND_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsNotificationsSound", TRUE));
					_r_menu_checkitem (hsubmenu, IDM_TRAY_NOTIFICATIONFULLSCREENSILENTMODE_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsNotificationsFullscreenSilentMode", TRUE));

					_r_menu_checkitem (hsubmenu, IDM_TRAY_NOTIFICATIONONTRAY_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsNotificationsOnTray", FALSE));

					_r_menu_checkitem (hsubmenu, IDM_TRAY_ENABLELOG_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsLogEnabled", FALSE));
					_r_menu_checkitem (hsubmenu, IDM_TRAY_ENABLEUILOG_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsLogUiEnabled", FALSE));

					if (!_r_config_getboolean (L"IsNotificationsEnabled", TRUE))
					{
						_r_menu_enableitem (hsubmenu, IDM_TRAY_ENABLENOTIFICATIONSSOUND_CHK, MF_BYCOMMAND, FALSE);
						_r_menu_enableitem (hsubmenu, IDM_TRAY_NOTIFICATIONFULLSCREENSILENTMODE_CHK, MF_BYCOMMAND, FALSE);
						_r_menu_enableitem (hsubmenu, IDM_TRAY_NOTIFICATIONONTRAY_CHK, MF_BYCOMMAND, FALSE);
					}
					else if (!_r_config_getboolean (L"IsNotificationsSound", TRUE))
					{
						_r_menu_enableitem (hsubmenu, IDM_TRAY_NOTIFICATIONFULLSCREENSILENTMODE_CHK, MF_BYCOMMAND, FALSE);
					}

					if (_wfp_isfiltersapplying ())
						_r_menu_enableitem (hsubmenu, IDM_TRAY_START, MF_BYCOMMAND, FALSE);

					_r_menu_popup (hsubmenu, hwnd, NULL, TRUE);

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
					if (config.is_neteventset)
					{
						HANDLE hengine = _wfp_getenginehandle ();

						if (hengine)
							_wfp_logunsubscribe (hengine);
					}

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
					return TRUE;
				}

				case PBT_APMRESUMECRITICAL:
				case PBT_APMRESUMESUSPEND:
				{
					if (config.is_neteventset)
					{
						HANDLE hengine = _wfp_getenginehandle ();

						if (hengine)
							_wfp_logsubscribe (hengine);
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
					if (!_r_config_getboolean (L"IsRefreshDevices", TRUE))
						break;

					PDEV_BROADCAST_HDR lbhdr = (PDEV_BROADCAST_HDR)lparam;

					if (lbhdr && lbhdr->dbch_devicetype == DBT_DEVTYP_VOLUME)
					{
						PDEV_BROADCAST_VOLUME lpdbv = (PDEV_BROADCAST_VOLUME)lparam;

						if (wparam == DBT_DEVICEARRIVAL)
						{
							if (_wfp_isfiltersinstalled () && !_wfp_isfiltersapplying ())
							{
								BOOLEAN is_appexist = _app_isapphavedrive (FirstDriveFromMask (lpdbv->dbcv_unitmask));

								if (is_appexist)
									_app_changefilters (hwnd, TRUE, FALSE);
							}
							else
							{
								if (IsWindowVisible (hwnd))
									_r_listview_redraw (hwnd, (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT), INVALID_INT);
							}
						}
						else if (wparam == DBT_DEVICEREMOVECOMPLETE)
						{
							if (IsWindowVisible (hwnd))
								_r_listview_redraw (hwnd, (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT), INVALID_INT);
						}
					}

					break;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			INT ctrl_id = LOWORD (wparam);

			if (HIWORD (wparam) == 0 && ctrl_id >= IDX_LANGUAGE && ctrl_id <= (IDX_LANGUAGE + (INT)_r_locale_getcount ()))
			{
				_r_locale_applyfrommenu (GetSubMenu (GetSubMenu (GetMenu (hwnd), 2), LANG_MENU), ctrl_id);
				return FALSE;
			}
			else if ((ctrl_id >= IDX_RULES_SPECIAL && ctrl_id <= (IDX_RULES_SPECIAL + (INT)rules_arr.size ())))
			{
				INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

				if (!SendDlgItemMessage (hwnd, listview_id, LVM_GETSELECTEDCOUNT, 0, 0))
					return FALSE;

				INT item = INVALID_INT;
				BOOL is_remove = INVALID_INT;

				SIZE_T rule_idx = (ctrl_id - IDX_RULES_SPECIAL);
				PITEM_RULE ptr_rule = _app_getrulebyid (rule_idx);

				if (!ptr_rule)
					return FALSE;

				while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
				{
					SIZE_T app_hash = _r_listview_getitemlparam (hwnd, listview_id, item);

					if (ptr_rule->is_forservices && (app_hash == config.ntoskrnl_hash || app_hash == config.svchost_hash))
						continue;

					PITEM_APP ptr_app = _app_getappitem (app_hash);

					if (!ptr_app)
						continue;

					_app_freenotify (app_hash, ptr_app);

					if (is_remove == INVALID_INT)
						is_remove = !!(ptr_rule->is_enabled && (ptr_rule->apps->find (app_hash) != ptr_rule->apps->end ()));

					if (is_remove)
					{
						ptr_rule->apps->erase (app_hash);

						if (ptr_rule->apps->empty ())
							_app_ruleenable (ptr_rule, FALSE);
					}
					else
					{
						ptr_rule->apps->emplace (app_hash, TRUE);

						_app_ruleenable (ptr_rule, TRUE);
					}

					_r_fastlock_acquireshared (&lock_checkbox);
					_app_setappiteminfo (hwnd, listview_id, item, app_hash, ptr_app);
					_r_fastlock_releaseshared (&lock_checkbox);

					_r_obj_dereference (ptr_app);
				}

				INT rule_listview_id = _app_getlistview_id (ptr_rule->type);

				if (rule_listview_id)
				{
					INT item_pos = _app_getposition (hwnd, rule_listview_id, rule_idx);

					if (item_pos != INVALID_INT)
					{
						_r_fastlock_acquireshared (&lock_checkbox);
						_app_setruleiteminfo (hwnd, rule_listview_id, item_pos, ptr_rule, TRUE);
						_r_fastlock_releaseshared (&lock_checkbox);
					}
				}

				OBJECTS_RULE_VECTOR rules;
				rules.emplace_back (ptr_rule);

				if (_wfp_isfiltersinstalled ())
				{
					HANDLE hengine = _wfp_getenginehandle ();

					if (hengine)
						_wfp_create4filters (hengine, &rules, __LINE__);
				}

				_r_obj_dereference (ptr_rule);

				_app_listviewsort (hwnd, listview_id, INVALID_INT, FALSE);
				_app_refreshstatus (hwnd, listview_id);

				_app_profile_save ();

				return FALSE;
			}
			else if ((ctrl_id >= IDX_TIMER && ctrl_id <= (IDX_TIMER + (INT)timers.size ())))
			{
				INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

				if (!listview_id || !SendDlgItemMessage (hwnd, listview_id, LVM_GETSELECTEDCOUNT, 0, 0))
					return FALSE;

				SIZE_T timer_idx = (ctrl_id - IDX_TIMER);
				time_t seconds = timers.at (timer_idx);
				INT item = INVALID_INT;
				OBJECTS_APP_VECTOR rules;

				while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
				{
					SIZE_T app_hash = _r_listview_getitemlparam (hwnd, listview_id, item);
					PITEM_APP ptr_app = _app_getappitem (app_hash);

					if (!ptr_app)
						continue;

					_app_timer_set (hwnd, ptr_app, seconds);

					rules.emplace_back (ptr_app);
				}

				if (_wfp_isfiltersinstalled ())
				{
					HANDLE hengine = _wfp_getenginehandle ();

					if (hengine)
						_wfp_create3filters (hengine, &rules, __LINE__);
				}

				_app_freeapps_vec (&rules);

				_app_listviewsort (hwnd, listview_id, INVALID_INT, FALSE);
				_app_refreshstatus (hwnd, listview_id);

				_app_profile_save ();

				return FALSE;
			}

			switch (ctrl_id)
			{
				case IDCANCEL: // process Esc key
				case IDM_TRAY_SHOW:
				{
					_r_wnd_toggle (hwnd, FALSE);
					break;
				}

				case IDM_SETTINGS:
				case IDM_TRAY_SETTINGS:
				{
					_r_settings_createwindow (hwnd, &SettingsProc);
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
					ShellExecute (hwnd, NULL, _APP_WEBSITE_URL, NULL, NULL, SW_SHOWDEFAULT);
					break;
				}

				case IDM_CHECKUPDATES:
				{
					_r_update_check (hwnd);
					break;
				}

				case IDM_DONATE:
				{
					ShellExecute (hwnd, NULL, _APP_DONATE_NEWURL, NULL, NULL, SW_SHOWDEFAULT);
					break;
				}

				case IDM_ABOUT:
				case IDM_TRAY_ABOUT:
				{
					_r_show_aboutmessage (hwnd);
					break;
				}

				case IDM_IMPORT:
				{
					WCHAR path[MAX_PATH];
					_r_str_copy (path, RTL_NUMBER_OF (path), XML_PROFILE);

					WCHAR title[MAX_PATH];
					_r_str_printf (title, RTL_NUMBER_OF (title), L"%s %s...", _r_locale_getstring (IDS_IMPORT), path);

					OPENFILENAME ofn = {0};

					ofn.lStructSize = sizeof (ofn);
					ofn.hwndOwner = hwnd;
					ofn.lpstrFile = path;
					ofn.nMaxFile = RTL_NUMBER_OF (path);
					ofn.lpstrFilter = L"*.xml;*.xml.bak\0*.xml;*.xml.bak\0*.*\0*.*\0\0";
					ofn.lpstrDefExt = L"xml";
					ofn.lpstrTitle = title;
					ofn.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_PATHMUSTEXIST | OFN_FORCESHOWHIDDEN;

					if (GetOpenFileName (&ofn))
					{
						if (!_app_profile_load_check (path, XmlProfileV3, TRUE))
						{
							_r_show_errormessage (hwnd, L"Import failure!", ERROR_INVALID_DATA, path, NULL);
						}
						else
						{
							// made backup
							_r_fs_remove (config.profile_path_backup, _R_FLAG_REMOVE_FORCE);
							_app_profile_save ();

							_app_profile_load (hwnd, path); // load profile

							_app_refreshstatus (hwnd, INVALID_INT);

							_app_changefilters (hwnd, TRUE, FALSE);
						}
					}

					break;
				}

				case IDM_EXPORT:
				{
					WCHAR path[MAX_PATH];
					_r_str_copy (path, RTL_NUMBER_OF (path), XML_PROFILE);

					WCHAR title[MAX_PATH];
					_r_str_printf (title, RTL_NUMBER_OF (title), L"%s %s...", _r_locale_getstring (IDS_EXPORT), path);

					OPENFILENAME ofn = {0};

					ofn.lStructSize = sizeof (ofn);
					ofn.hwndOwner = hwnd;
					ofn.lpstrFile = path;
					ofn.nMaxFile = RTL_NUMBER_OF (path);
					ofn.lpstrFilter = L"*.xml\0*.xml\0\0";
					ofn.lpstrDefExt = L"xml";
					ofn.lpstrTitle = title;
					ofn.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

					if (GetSaveFileName (&ofn))
					{
						_app_profile_save ();

						// added information for export profile failure (issue #707)
						if (!_r_fs_copy (config.profile_path, path, 0))
							_r_show_errormessage (hwnd, L"Export failure!", GetLastError (), path, NULL);
					}

					break;
				}

				case IDM_ALWAYSONTOP_CHK:
				{
					BOOLEAN new_val = !_r_config_getboolean (L"AlwaysOnTop", _APP_ALWAYSONTOP);

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
					_r_config_setboolean (L"AlwaysOnTop", new_val);

					_r_wnd_top (hwnd, new_val);

					break;
				}

				case IDM_AUTOSIZECOLUMNS_CHK:
				{
					BOOLEAN new_val = !_r_config_getboolean (L"AutoSizeColumns", TRUE);

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
					_r_config_setboolean (L"AutoSizeColumns", new_val);

					if (new_val)
						_app_listviewresize (hwnd, (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT), FALSE);

					break;
				}

				case IDM_SHOWFILENAMESONLY_CHK:
				{
					BOOLEAN new_val = !_r_config_getboolean (L"ShowFilenames", TRUE);

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
					_r_config_setboolean (L"ShowFilenames", new_val);

					// regroup apps
					for (auto it = apps.begin (); it != apps.end (); ++it)
					{
						if (!it->second)
							continue;

						SIZE_T app_hash = it->first;
						PITEM_APP ptr_app = (PITEM_APP)_r_obj_reference (it->second);

						_r_obj_movereference (&ptr_app->display_name, _app_getdisplayname (app_hash, ptr_app, FALSE));

						INT listview_id = _app_getlistview_id (ptr_app->type);

						if (listview_id)
						{
							INT item_pos = _app_getposition (hwnd, listview_id, app_hash);

							if (item_pos != INVALID_INT)
							{
								_r_fastlock_acquireshared (&lock_checkbox);
								_app_setappiteminfo (hwnd, listview_id, item_pos, app_hash, ptr_app);
								_r_fastlock_releaseshared (&lock_checkbox);
							}
						}

						_r_obj_dereference (ptr_app);
					}

					_app_listviewsort (hwnd, (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT), INVALID_INT, FALSE);

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
					_r_config_setinteger (L"ViewType", view_type);

					INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

					if (listview_id)
					{
						_app_listviewsetview (hwnd, listview_id);
						_app_listviewresize (hwnd, listview_id, FALSE);

						_r_listview_redraw (hwnd, listview_id, INVALID_INT);
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
					_r_config_setinteger (L"IconSize", icon_size);

					INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

					if (listview_id)
					{
						_app_listviewsetview (hwnd, listview_id);
						_app_listviewresize (hwnd, listview_id, FALSE);

						_r_listview_redraw (hwnd, listview_id, INVALID_INT);
					}

					break;
				}

				case IDM_ICONSISHIDDEN:
				{
					BOOLEAN new_val = !_r_config_getboolean (L"IsIconsHidden", FALSE);

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
					_r_config_setboolean (L"IsIconsHidden", new_val);

					for (auto it = apps.begin (); it != apps.end (); ++it)
					{
						if (!it->second)
							continue;

						SIZE_T app_hash = it->first;
						PITEM_APP ptr_app = (PITEM_APP)_r_obj_reference (it->second);

						INT listview_id = _app_getlistview_id (ptr_app->type);

						if (listview_id)
						{
							INT item_pos = _app_getposition (hwnd, listview_id, app_hash);

							if (item_pos != INVALID_INT)
							{
								_r_fastlock_acquireshared (&lock_checkbox);
								_app_setappiteminfo (hwnd, listview_id, item_pos, app_hash, ptr_app);
								_r_fastlock_releaseshared (&lock_checkbox);
							}
						}

						_r_obj_dereference (ptr_app);
					}

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

					_r_config_getfont (L"Font", hwnd, &lf);

					if (ChooseFont (&cf))
					{
						_r_config_setfont (L"Font", hwnd, &lf);

						SAFE_DELETE_OBJECT (config.hfont);

						INT current_page = (INT)SendDlgItemMessage (hwnd, IDC_TAB, TCM_GETCURSEL, 0, 0);

						for (INT i = 0; i < (INT)SendDlgItemMessage (hwnd, IDC_TAB, TCM_GETITEMCOUNT, 0, 0); i++)
						{
							INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, i);

							if (listview_id)
							{
								_app_listviewsetfont (hwnd, listview_id, FALSE);

								if (i == current_page)
									_app_listviewresize (hwnd, listview_id, FALSE);
							}
						}

						RedrawWindow (hwnd, NULL, NULL, RDW_NOFRAME | RDW_NOINTERNALPAINT | RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
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
						fr.wFindWhatLen = RTL_NUMBER_OF (config.search_string) - 1;
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

					_r_config_initialize ();

					_app_profile_load (hwnd, NULL);
					_app_refreshstatus (hwnd, INVALID_INT);

					_app_changefilters (hwnd, TRUE, FALSE);

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
					HMENU hmenu = GetMenu (hwnd);

					if (ctrl_id >= IDM_BLOCKLIST_SPY_DISABLE && ctrl_id <= IDM_BLOCKLIST_SPY_BLOCK)
					{
						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_SPY_DISABLE, IDM_BLOCKLIST_SPY_BLOCK, MF_BYCOMMAND, ctrl_id);

						INT new_state = _r_calc_clamp (INT, ctrl_id - IDM_BLOCKLIST_SPY_DISABLE, 0, 2);

						_r_config_setinteger (L"BlocklistSpyState", new_state);

						_app_ruleblocklistset (hwnd, new_state, INVALID_INT, INVALID_INT, TRUE);
					}
					else if (ctrl_id >= IDM_BLOCKLIST_UPDATE_DISABLE && ctrl_id <= IDM_BLOCKLIST_UPDATE_BLOCK)
					{
						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_UPDATE_DISABLE, IDM_BLOCKLIST_UPDATE_BLOCK, MF_BYCOMMAND, ctrl_id);

						INT new_state = _r_calc_clamp (INT, ctrl_id - IDM_BLOCKLIST_UPDATE_DISABLE, 0, 2);

						_r_config_setinteger (L"BlocklistUpdateState", new_state);

						_app_ruleblocklistset (hwnd, INVALID_INT, new_state, INVALID_INT, TRUE);
					}
					else if (ctrl_id >= IDM_BLOCKLIST_EXTRA_DISABLE && ctrl_id <= IDM_BLOCKLIST_EXTRA_BLOCK)
					{
						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_EXTRA_DISABLE, IDM_BLOCKLIST_EXTRA_BLOCK, MF_BYCOMMAND, ctrl_id);

						INT new_state = _r_calc_clamp (INT, ctrl_id - IDM_BLOCKLIST_EXTRA_DISABLE, 0, 2);

						_r_config_setinteger (L"BlocklistExtraState", new_state);

						_app_ruleblocklistset (hwnd, INVALID_INT, INVALID_INT, new_state, TRUE);
					}

					break;
				}

				case IDM_TRAY_ENABLELOG_CHK:
				{
					BOOLEAN new_val = !_r_config_getboolean (L"IsLogEnabled", FALSE);

					_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, ctrl_id, NULL, 0, new_val ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED, I_IMAGENONE);
					_r_config_setboolean (L"IsLogEnabled", new_val);

					_app_loginit (new_val);

					break;
				}

				case IDM_TRAY_ENABLEUILOG_CHK:
				{
					BOOLEAN new_val = !_r_config_getboolean (L"IsLogUiEnabled", FALSE);

					_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, ctrl_id, NULL, 0, new_val ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED, I_IMAGENONE);
					_r_config_setboolean (L"IsLogUiEnabled", new_val);

					break;
				}

				case IDM_TRAY_ENABLENOTIFICATIONS_CHK:
				{
					BOOLEAN new_val = !_r_config_getboolean (L"IsNotificationsEnabled", TRUE);

					_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, ctrl_id, NULL, 0, new_val ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED, I_IMAGENONE);
					_r_config_setboolean (L"IsNotificationsEnabled", new_val);

					_app_notifyrefresh (config.hnotification, TRUE);

					break;
				}

				case IDM_TRAY_ENABLENOTIFICATIONSSOUND_CHK:
				{
					BOOLEAN new_val = !_r_config_getboolean (L"IsNotificationsSound", TRUE);

					_r_config_setboolean (L"IsNotificationsSound", new_val);

					break;
				}

				case IDM_TRAY_NOTIFICATIONFULLSCREENSILENTMODE_CHK:
				{
					BOOLEAN new_val = !_r_config_getboolean (L"IsNotificationsFullscreenSilentMode", TRUE);

					_r_config_setboolean (L"IsNotificationsFullscreenSilentMode", new_val);

					break;
				}

				case IDM_TRAY_NOTIFICATIONONTRAY_CHK:
				{
					BOOLEAN new_val = !_r_config_getboolean (L"IsNotificationsOnTray", FALSE);

					_r_config_setboolean (L"IsNotificationsOnTray", new_val);

					if (IsWindowVisible (config.hnotification))
						_app_notifysetpos (config.hnotification, TRUE);

					break;
				}

				case IDM_TRAY_LOGSHOW:
				{
					if (_r_config_getboolean (L"IsLogUiEnabled", FALSE))
					{
						_r_wnd_toggle (hwnd, TRUE);

						if (!log_arr.empty ())
						{
							_app_showitem (hwnd, IDC_LOG, _app_getposition (hwnd, IDC_LOG, (log_arr.size () - 1)), INVALID_INT);
						}
						else
						{
							_app_settab_id (hwnd, IDC_LOG);
						}
					}
					else
					{
						PR_STRING logPath;
						PR_STRING viewerPath;
						PR_STRING processPath;

						logPath = _r_path_expand (_r_config_getstring (L"LogPath", LOG_PATH_DEFAULT));

						if (!logPath)
							return FALSE;

						if (!_r_fs_exists (logPath->Buffer))
						{
							_r_obj_dereference (logPath);
							return FALSE;
						}

						if (_r_fs_isvalidhandle (config.hlogfile))
							FlushFileBuffers (config.hlogfile);

						viewerPath = _app_getlogviewer ();

						if (viewerPath)
						{
							processPath = _r_format_string (L"%s \"%s\"", _r_obj_getstring (viewerPath), logPath->Buffer);

							if (!_r_sys_createprocess (NULL, processPath->Buffer, NULL))
								_r_show_errormessage (hwnd, NULL, GetLastError (), viewerPath->Buffer, NULL);

							_r_obj_dereference (processPath);
							_r_obj_dereference (viewerPath);
						}

						_r_obj_dereference (logPath);
					}

					break;
				}

				case IDM_TRAY_LOGCLEAR:
				{
					PR_STRING path = _r_path_expand (_r_config_getstring (L"LogPath", LOG_PATH_DEFAULT));

					if (_r_fs_isvalidhandle (config.hlogfile) || (path && _r_fs_exists (path->Buffer)) || !log_arr.empty ())
					{
						if (_r_show_confirmmessage (hwnd, NULL, _r_locale_getstring (IDS_QUESTION), L"ConfirmLogClear"))
						{
							_app_freelogstack ();

							_app_logclear ();
							_app_logclear_ui (hwnd);
						}
					}

					SAFE_DELETE_REFERENCE (path);

					break;
				}

				case IDM_TRAY_LOGSHOW_ERR:
				{
					PR_STRING viewerPath;
					PR_STRING processPath;
					LPCWSTR logPath;

					logPath = _r_app_getlogpath ();

					if (_r_fs_exists (logPath))
					{
						viewerPath = _app_getlogviewer ();

						if (viewerPath)
						{
							processPath = _r_format_string (L"%s \"%s\"", viewerPath->Buffer, logPath);

							if (!_r_sys_createprocess (NULL, processPath->Buffer, NULL))
								_r_show_errormessage (hwnd, NULL, GetLastError (), viewerPath->Buffer, NULL);

							_r_obj_dereference (processPath);
							_r_obj_dereference (viewerPath);
						}
					}

					break;
				}

				case IDM_TRAY_LOGCLEAR_ERR:
				{
					if (!_r_show_confirmmessage (hwnd, NULL, _r_locale_getstring (IDS_QUESTION), L"ConfirmLogClear"))
						break;

					LPCWSTR path = _r_app_getlogpath ();

					if (!_r_fs_exists (path))
						break;

					_r_fs_remove (path, _R_FLAG_REMOVE_FORCE);

					break;
				}

				case IDM_TRAY_START:
				{
					if (_wfp_isfiltersapplying ())
						break;

					BOOLEAN is_filtersinstalled = !(_wfp_isfiltersinstalled () != InstallDisabled);

					if (_app_installmessage (hwnd, is_filtersinstalled))
						_app_changefilters (hwnd, is_filtersinstalled, TRUE);

					break;
				}

				case IDM_ADD_FILE:
				{
					WCHAR files[8192] = {0};
					OPENFILENAME ofn = {0};

					ofn.lStructSize = sizeof (ofn);
					ofn.hwndOwner = hwnd;
					ofn.lpstrFile = files;
					ofn.nMaxFile = RTL_NUMBER_OF (files);
					ofn.lpstrFilter = L"*.exe\0*.exe\0*.*\0*.*\0\0";
					ofn.Flags = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_ENABLESIZING | OFN_PATHMUSTEXIST | OFN_FORCESHOWHIDDEN;

					if (GetOpenFileName (&ofn))
					{
						SIZE_T app_hash = 0;

						if (files[ofn.nFileOffset - 1] != 0)
						{
							app_hash = _app_addapplication (hwnd, files, 0, 0, 0, FALSE, FALSE);
						}
						else
						{
							LPWSTR p = files;

							WCHAR dir[MAX_PATH] = {0};
							WCHAR full_path[512];

							GetCurrentDirectory (RTL_NUMBER_OF (dir), dir);

							while (*p)
							{
								p += _r_str_length (p) + 1;

								if (*p)
								{
									_r_str_printf (full_path, RTL_NUMBER_OF (full_path), L"%s\\%s", dir, p);
									app_hash = _app_addapplication (hwnd, full_path, 0, 0, 0, FALSE, FALSE);
								}
							}
						}

						{
							INT app_listview_id = (INT)_app_getappinfo (app_hash, InfoListviewId);

							if (app_listview_id)
							{
								if (app_listview_id == (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT))
									_app_listviewsort (hwnd, app_listview_id, INVALID_INT, FALSE);

								_app_showitem (hwnd, app_listview_id, _app_getposition (hwnd, app_listview_id, app_hash), INVALID_INT);
							}
						}

						_app_refreshstatus (hwnd, INVALID_INT);
						_app_profile_save ();
					}

					break;
				}

				case IDM_DISABLENOTIFICATIONS:
				case IDM_DISABLETIMER:
				{
					INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

					// note: these commands only for profile...
					if (!(listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP))
						break;

					INT item = INVALID_INT;
					BOOL new_val = INVALID_INT;

					while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
					{
						SIZE_T app_hash = _r_listview_getitemlparam (hwnd, listview_id, item);
						PITEM_APP ptr_app = _app_getappitem (app_hash);

						if (!ptr_app)
							continue;

						if (ctrl_id == IDM_DISABLENOTIFICATIONS)
						{
							if (new_val == INVALID_INT)
								new_val = !ptr_app->is_silent;

							ptr_app->is_silent = !!new_val;

							if (new_val)
								_app_freenotify (app_hash, ptr_app);
						}
						else if (ctrl_id == IDM_DISABLETIMER)
						{
							_app_timer_reset (hwnd, ptr_app);
						}

						_r_obj_dereference (ptr_app);
					}

					_app_listviewsort (hwnd, listview_id, INVALID_INT, FALSE);
					_app_refreshstatus (hwnd, listview_id);

					_app_profile_save ();

					break;
				}

				case IDM_COPY:
				case IDM_COPY2:
				{
					INT listview_id;
					INT item;

					INT column_count;
					INT column_current;

					PR_STRING buffer;
					PR_STRING string;

					listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);
					item = INVALID_INT;

					column_count = _r_listview_getcolumncount (hwnd, listview_id);
					column_current = (INT)lparam;

					buffer = _r_obj_createstringbuilder ();

					while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
					{
						if (ctrl_id == IDM_COPY)
						{
							for (INT column_id = 0; column_id < column_count; column_id++)
							{
								string = _r_listview_getitemtext (hwnd, listview_id, item, column_id);

								if (string)
								{
									if (!_r_str_isempty (string))
										_r_string_appendformat (&buffer, L"%s%s", string->Buffer, DIVIDER_COPY);

									_r_obj_dereference (string);
								}
							}

							_r_str_trim (buffer, DIVIDER_COPY);
						}
						else
						{
							string = _r_listview_getitemtext (hwnd, listview_id, item, column_current);

							if (string)
							{
								if (!_r_str_isempty (string))
									_r_string_append (&buffer, string->Buffer);

								_r_obj_dereference (string);
							}
						}

						_r_string_append (&buffer, L"\r\n");
					}

					_r_str_trim (buffer, DIVIDER_TRIM);

					if (!_r_str_isempty (buffer))
						_r_clipboard_set (hwnd, buffer->Buffer, _r_obj_getstringlength (buffer));

					_r_obj_dereference (buffer);

					break;
				}

				case IDM_CHECK:
				case IDM_UNCHECK:
				{
					INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);
					BOOLEAN new_val = (ctrl_id == IDM_CHECK);

					BOOLEAN is_changed = FALSE;

					if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
					{
						OBJECTS_APP_VECTOR rules;
						INT item = INVALID_INT;

						while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
						{
							SIZE_T app_hash = _r_listview_getitemlparam (hwnd, listview_id, item);
							PITEM_APP ptr_app = _app_getappitem (app_hash);

							if (!ptr_app)
								continue;

							if (ptr_app->is_enabled != new_val)
							{
								if (!new_val)
									_app_timer_reset (hwnd, ptr_app);

								else
									_app_freenotify (app_hash, ptr_app);

								ptr_app->is_enabled = new_val;

								_r_fastlock_acquireshared (&lock_checkbox);
								_app_setappiteminfo (hwnd, listview_id, item, app_hash, ptr_app);
								_r_fastlock_releaseshared (&lock_checkbox);

								rules.emplace_back (ptr_app);

								is_changed = TRUE;

								// do not reset reference counter
							}
							else
							{
								_r_obj_dereference (ptr_app);
							}
						}

						if (is_changed)
						{
							if (_wfp_isfiltersinstalled ())
							{
								HANDLE hengine = _wfp_getenginehandle ();

								if (hengine)
									_wfp_create3filters (hengine, &rules, __LINE__);
							}

							_app_freeapps_vec (&rules);
						}
					}
					else if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM)
					{
						OBJECTS_RULE_VECTOR rules;
						INT item = INVALID_INT;

						while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
						{
							SIZE_T rule_idx = _r_listview_getitemlparam (hwnd, listview_id, item);
							PITEM_RULE ptr_rule = _app_getrulebyid (rule_idx);

							if (!ptr_rule)
								continue;

							if (ptr_rule->is_enabled != new_val)
							{
								_app_ruleenable (ptr_rule, new_val);

								_r_fastlock_acquireshared (&lock_checkbox);
								_app_setruleiteminfo (hwnd, listview_id, item, ptr_rule, TRUE);
								_r_fastlock_releaseshared (&lock_checkbox);

								rules.emplace_back (ptr_rule);

								is_changed = TRUE;

								// do not reset reference counter
							}
							else
							{
								_r_obj_dereference (ptr_rule);
							}
						}

						if (is_changed)
						{
							if (_wfp_isfiltersinstalled ())
							{
								HANDLE hengine = _wfp_getenginehandle ();

								if (hengine)
									_wfp_create4filters (hengine, &rules, __LINE__);
							}

							_app_freerules_vec (&rules);
						}
					}

					if (is_changed)
					{
						_app_listviewsort (hwnd, listview_id, INVALID_INT, FALSE);
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
					PITEM_RULE ptr_rule = (PITEM_RULE)_r_obj_allocateex (sizeof (ITEM_RULE), &_app_dereferencerule);

					// initialize stl
					ptr_rule->apps = new HASHER_MAP;
					ptr_rule->guids = new GUIDS_VEC;

					_app_ruleenable (ptr_rule, TRUE);

					ptr_rule->type = DataRuleCustom;
					ptr_rule->is_block = FALSE;

					INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

					if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
					{
						INT item = INVALID_INT;

						while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
						{
							SIZE_T app_hash = _r_listview_getitemlparam (hwnd, listview_id, item);

							if (_app_isappfound (app_hash))
								ptr_rule->apps->emplace (app_hash, TRUE);
						}
					}
					else if (listview_id == IDC_NETWORK)
					{
						ptr_rule->is_block = TRUE;

						INT item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)INVALID_INT, LVNI_SELECTED);

						if (item != INVALID_INT)
						{
							SIZE_T network_hash = _r_listview_getitemlparam (hwnd, listview_id, item);
							PITEM_NETWORK ptr_network = _app_getnetworkitem (network_hash);

							if (ptr_network)
							{
								_r_obj_movereference (&ptr_rule->name, _r_listview_getitemtext (hwnd, listview_id, item, 0));

								if (ptr_network->app_hash && !_r_str_isempty (ptr_network->path))
								{
									if (!_app_isappfound (ptr_network->app_hash))
									{
										_app_addapplication (hwnd, ptr_network->path->Buffer, 0, 0, 0, FALSE, FALSE);

										_app_refreshstatus (hwnd, listview_id);
										_app_profile_save ();
									}

									ptr_rule->apps->emplace (ptr_network->app_hash, TRUE);
								}

								ptr_rule->protocol = ptr_network->protocol;

								_r_obj_movereference (&ptr_rule->rule_remote, _app_formataddress (ptr_network->af, 0, &ptr_network->remote_addr, ptr_network->remote_port, FMTADDR_AS_RULE));
								_r_obj_movereference (&ptr_rule->rule_local, _app_formataddress (ptr_network->af, 0, &ptr_network->local_addr, ptr_network->local_port, FMTADDR_AS_RULE));

								_r_obj_dereference (ptr_network);
							}
						}
					}
					else if (listview_id == IDC_LOG)
					{
						//ptr_rule->is_block = FALSE;

						INT item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)INVALID_INT, LVNI_SELECTED);

						if (item != INVALID_INT)
						{
							SIZE_T log_idx = _r_listview_getitemlparam (hwnd, listview_id, item);
							PITEM_LOG ptr_log = _app_getlogitem (log_idx);

							if (ptr_log)
							{
								if (_r_str_isempty (ptr_rule->name))
								{
									PR_STRING itemText = _r_listview_getitemtext (hwnd, listview_id, item, 0);

									if (itemText)
										_r_obj_movereference (&ptr_rule->name, itemText);
								}

								if (ptr_log->app_hash && !_r_str_isempty (ptr_log->path))
								{
									if (!_app_isappfound (ptr_log->app_hash))
									{
										_app_addapplication (hwnd, ptr_log->path->Buffer, 0, 0, 0, FALSE, FALSE);

										_app_refreshstatus (hwnd, listview_id);
										_app_profile_save ();
									}

									ptr_rule->apps->emplace (ptr_log->app_hash, TRUE);
								}

								ptr_rule->protocol = ptr_log->protocol;
								ptr_rule->direction = ptr_log->direction;

								_r_obj_movereference (&ptr_rule->rule_remote, _app_formataddress (ptr_log->af, 0, &ptr_log->remote_addr, ptr_log->remote_port, FMTADDR_AS_RULE));
								_r_obj_movereference (&ptr_rule->rule_local, _app_formataddress (ptr_log->af, 0, &ptr_log->local_addr, ptr_log->local_port, FMTADDR_AS_RULE));

								_r_obj_dereference (ptr_log);
							}
						}
					}

					if (DialogBoxParam (NULL, MAKEINTRESOURCE (IDD_EDITOR), hwnd, &EditorProc, (LPARAM)ptr_rule))
					{
						SIZE_T rule_idx = rules_arr.size ();
						rules_arr.emplace_back ((PITEM_RULE)_r_obj_reference (ptr_rule));

						INT listview_rules_id = _app_getlistview_id (DataRuleCustom);

						if (listview_rules_id)
						{
							INT item_id = _r_listview_getitemcount (hwnd, listview_rules_id, FALSE);

							_r_fastlock_acquireshared (&lock_checkbox);

							_r_listview_additemex (hwnd, listview_rules_id, item_id, 0, _r_obj_getstringordefault (ptr_rule->name, SZ_EMPTY), _app_getruleicon (ptr_rule), _app_getrulegroup (ptr_rule), rule_idx);
							_app_setruleiteminfo (hwnd, listview_rules_id, item_id, ptr_rule, TRUE);

							_r_fastlock_releaseshared (&lock_checkbox);
						}

						_app_listviewsort (hwnd, listview_id, INVALID_INT, FALSE);
						_app_refreshstatus (hwnd, listview_id);

						_app_profile_save ();
					}

					_r_obj_dereference (ptr_rule);

					break;
				}

				case IDM_PROPERTIES:
				{
					INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);
					INT item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)INVALID_INT, LVNI_SELECTED);

					if (item == INVALID_INT)
						break;

					if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
					{
						do
						{
							SIZE_T app_hash = _r_listview_getitemlparam (hwnd, listview_id, item);

							PR_STRING path = (PR_STRING)_app_getappinfo (app_hash, InfoPath);

							if (path)
							{
								if (!_r_str_isempty (path))
									_r_path_explore (path->Buffer);

								_r_obj_dereference (path);
							}
						}
						while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT);
					}
					else if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM)
					{
						SIZE_T rule_idx = _r_listview_getitemlparam (hwnd, listview_id, item);
						PITEM_RULE ptr_rule = _app_getrulebyid (rule_idx);

						if (!ptr_rule)
							break;

						if (DialogBoxParam (NULL, MAKEINTRESOURCE (IDD_EDITOR), hwnd, &EditorProc, (LPARAM)ptr_rule))
						{
							_app_setruleiteminfo (hwnd, listview_id, item, ptr_rule, TRUE);

							_app_listviewsort (hwnd, listview_id, INVALID_INT, FALSE);
							_app_refreshstatus (hwnd, listview_id);

							_app_profile_save ();
						}

						_r_obj_dereference (ptr_rule);
					}
					else if (listview_id == IDC_NETWORK)
					{
						SIZE_T network_hash = _r_listview_getitemlparam (hwnd, listview_id, item);
						PITEM_NETWORK ptr_network = _app_getnetworkitem (network_hash);

						if (!ptr_network)
							break;

						if (ptr_network->app_hash && !_r_str_isempty (ptr_network->path))
						{
							if (!_app_isappfound (ptr_network->app_hash))
							{
								_app_addapplication (hwnd, ptr_network->path->Buffer, 0, 0, 0, FALSE, FALSE);

								_app_refreshstatus (hwnd, listview_id);
								_app_profile_save ();
							}

							INT app_listview_id = (INT)_app_getappinfo (ptr_network->app_hash, InfoListviewId);

							if (app_listview_id)
							{
								_app_listviewsort (hwnd, app_listview_id, INVALID_INT, FALSE);
								_app_showitem (hwnd, app_listview_id, _app_getposition (hwnd, app_listview_id, ptr_network->app_hash), INVALID_INT);
							}
						}

						_r_obj_dereference (ptr_network);
					}
					else if (listview_id == IDC_LOG)
					{
						SIZE_T log_idx = _r_listview_getitemlparam (hwnd, listview_id, item);
						PITEM_LOG ptr_log = _app_getlogitem (log_idx);

						if (!ptr_log)
							break;

						if (ptr_log->app_hash && !_r_str_isempty (ptr_log->path))
						{
							if (!_app_isappfound (ptr_log->app_hash))
							{
								_app_addapplication (hwnd, ptr_log->path->Buffer, 0, 0, 0, FALSE, FALSE);

								_app_refreshstatus (hwnd, listview_id);
								_app_profile_save ();
							}

							INT app_listview_id = (INT)_app_getappinfo (ptr_log->app_hash, InfoListviewId);

							if (app_listview_id)
							{
								_app_listviewsort (hwnd, app_listview_id, INVALID_INT, FALSE);
								_app_showitem (hwnd, app_listview_id, _app_getposition (hwnd, app_listview_id, ptr_log->app_hash), INVALID_INT);
							}
						}

						_r_obj_dereference (ptr_log);
					}

					break;
				}

				case IDM_NETWORK_CLOSE:
				{
					INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

					if (listview_id != IDC_NETWORK)
						break;

					INT selected = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETSELECTEDCOUNT, 0, 0);

					if (!selected)
						break;

					INT item = INVALID_INT;

					MIB_TCPROW tcprow;

					while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
					{
						SIZE_T network_hash = _r_listview_getitemlparam (hwnd, listview_id, item);
						PITEM_NETWORK ptr_network = _app_getnetworkitem (network_hash);

						if (!ptr_network)
							continue;

						if (ptr_network->af == AF_INET && ptr_network->state == MIB_TCP_STATE_ESTAB)
						{
							RtlSecureZeroMemory (&tcprow, sizeof (tcprow));

							tcprow.dwState = MIB_TCP_STATE_DELETE_TCB;
							tcprow.dwLocalAddr = ptr_network->local_addr.S_un.S_addr;
							tcprow.dwLocalPort = _r_byteswap_ushort ((USHORT)ptr_network->local_port);
							tcprow.dwRemoteAddr = ptr_network->remote_addr.S_un.S_addr;
							tcprow.dwRemotePort = _r_byteswap_ushort ((USHORT)ptr_network->remote_port);

							if (SetTcpEntry (&tcprow) == NO_ERROR)
							{
								SendDlgItemMessage (hwnd, listview_id, LVM_DELETEITEM, (WPARAM)item, 0);

								network_map.erase (network_hash);
								_r_obj_dereferenceex (ptr_network, 2);

								continue;
							}
						}

						_r_obj_dereference (ptr_network);
					}

					break;
				}

				case IDM_DELETE:
				{
					INT listview_id;
					INT selected;
					INT count;
					WCHAR messageText[512];

					listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

					if (listview_id != IDC_APPS_PROFILE && listview_id != IDC_RULES_CUSTOM)
						break;

					if (!(selected = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETSELECTEDCOUNT, 0, 0)))
						break;

					_r_str_printf (messageText, RTL_NUMBER_OF (messageText), _r_locale_getstring (IDS_QUESTION_DELETE), selected);

					if (_r_show_message (hwnd, MB_YESNO | MB_ICONEXCLAMATION, NULL, NULL, messageText) != IDYES)
						break;

					count = _r_listview_getitemcount (hwnd, listview_id, FALSE) - 1;

					GUIDS_VEC guids;

					for (INT i = count; i != INVALID_INT; i--)
					{
						if ((UINT)SendDlgItemMessage (hwnd, listview_id, LVM_GETITEMSTATE, (WPARAM)i, LVNI_SELECTED) == LVNI_SELECTED)
						{
							if (listview_id == IDC_APPS_PROFILE)
							{
								SIZE_T app_hash = _r_listview_getitemlparam (hwnd, listview_id, i);
								PITEM_APP ptr_app = _app_getappitem (app_hash);

								if (!ptr_app)
									continue;

								if (!ptr_app->is_undeletable) // skip "undeletable" apps
								{
									guids.insert (guids.end (), ptr_app->guids->begin (), ptr_app->guids->end ());

									SendDlgItemMessage (hwnd, listview_id, LVM_DELETEITEM, (WPARAM)i, 0);

									_app_timer_reset (hwnd, ptr_app);
									_app_freenotify (app_hash, ptr_app);

									_app_freeapplication (app_hash);

									_r_obj_dereferenceex (ptr_app, 2);
								}
								else
								{
									_r_obj_dereference (ptr_app);
								}
							}
							else if (listview_id == IDC_RULES_CUSTOM)
							{
								SIZE_T rule_idx = _r_listview_getitemlparam (hwnd, listview_id, i);
								PITEM_RULE ptr_rule = _app_getrulebyid (rule_idx);

								if (!ptr_rule)
									continue;

								HASHER_MAP apps_checker;

								if (!ptr_rule->is_readonly) // skip "read-only" rules
								{
									guids.insert (guids.end (), ptr_rule->guids->begin (), ptr_rule->guids->end ());

									for (auto it = ptr_rule->apps->begin (); it != ptr_rule->apps->end (); ++it)
										apps_checker.emplace (it->first, TRUE);

									SendDlgItemMessage (hwnd, listview_id, LVM_DELETEITEM, (WPARAM)i, 0);

									rules_arr.at (rule_idx) = NULL;

									_r_obj_dereferenceex (ptr_rule, 2);
								}
								else
								{
									_r_obj_dereference (ptr_rule);
								}

								for (auto it = apps_checker.begin (); it != apps_checker.end (); ++it)
								{
									SIZE_T app_hash = it->first;
									PITEM_APP ptr_app = _app_getappitem (app_hash);

									if (!ptr_app)
										continue;

									INT app_listview_id = _app_getlistview_id (ptr_app->type);

									if (app_listview_id)
									{
										INT item_pos = _app_getposition (hwnd, app_listview_id, app_hash);

										if (item_pos != INVALID_INT)
										{
											_r_fastlock_acquireshared (&lock_checkbox);
											_app_setappiteminfo (hwnd, app_listview_id, item_pos, app_hash, ptr_app);
											_r_fastlock_releaseshared (&lock_checkbox);
										}
									}

									_r_obj_dereference (ptr_app);
								}
							}
						}
					}

					if (_wfp_isfiltersinstalled ())
					{
						HANDLE hengine = _wfp_getenginehandle ();

						if (hengine)
							_wfp_destroyfilters_array (hengine, &guids, __LINE__);
					}

					_app_refreshstatus (hwnd, INVALID_INT);
					_app_profile_save ();

					break;
				}

				case IDM_PURGE_UNUSED:
				{
					BOOLEAN is_deleted = FALSE;

					GUIDS_VEC guids;
					HASH_VEC apps_list;

					for (auto it = apps.begin (); it != apps.end (); ++it)
					{
						if (!it->second)
							continue;

						SIZE_T app_hash = it->first;
						PITEM_APP ptr_app = (PITEM_APP)_r_obj_reference (it->second);

						if (!ptr_app->is_undeletable && (!_app_isappexists (ptr_app) || ((ptr_app->type != DataAppService && ptr_app->type != DataAppUWP) && !_app_isappused (ptr_app, app_hash))))
						{
							INT app_listview_id = _app_getlistview_id (ptr_app->type);

							if (app_listview_id)
							{
								INT item_pos = _app_getposition (hwnd, app_listview_id, app_hash);

								if (item_pos != INVALID_INT)
									SendDlgItemMessage (hwnd, app_listview_id, LVM_DELETEITEM, (WPARAM)item_pos, 0);
							}

							_app_timer_reset (hwnd, ptr_app);
							_app_freenotify (app_hash, ptr_app);

							guids.insert (guids.end (), ptr_app->guids->begin (), ptr_app->guids->end ());

							apps_list.emplace_back (app_hash);

							is_deleted = TRUE;
						}

						_r_obj_dereference (ptr_app);
					}

					for (auto it = apps_list.begin (); it != apps_list.end (); ++it)
						_app_freeapplication (*it);

					if (is_deleted)
					{
						if (_wfp_isfiltersinstalled ())
						{
							HANDLE hengine = _wfp_getenginehandle ();

							if (hengine)
								_wfp_destroyfilters_array (hengine, &guids, __LINE__);
						}

						_app_refreshstatus (hwnd, INVALID_INT);
						_app_profile_save ();
					}

					break;
				}

				case IDM_PURGE_TIMERS:
				{
					if (!_app_istimersactive () || _r_show_message (hwnd, MB_YESNO | MB_ICONEXCLAMATION, NULL, NULL, _r_locale_getstring (IDS_QUESTION_TIMERS)) != IDYES)
						break;

					OBJECTS_APP_VECTOR rules;

					for (auto it = apps.begin (); it != apps.end (); ++it)
					{
						if (!it->second)
							continue;

						PITEM_APP ptr_app = (PITEM_APP)_r_obj_reference (it->second);

						if (_app_istimeractive (ptr_app))
						{
							_app_timer_reset (hwnd, ptr_app);

							rules.emplace_back (ptr_app);
						}
						else
						{
							_r_obj_dereference (ptr_app);
						}
					}

					if (_wfp_isfiltersinstalled ())
					{
						HANDLE hengine = _wfp_getenginehandle ();

						if (hengine)
							_wfp_create3filters (hengine, &rules, __LINE__);
					}

					_app_freeapps_vec (&rules);

					_app_profile_save ();

					INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

					if (listview_id)
					{
						_app_listviewsort (hwnd, listview_id, INVALID_INT, FALSE);
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
					PITEM_LOG ptr_log = (PITEM_LOG)_r_obj_allocateex (sizeof (ITEM_LOG), &_app_dereferencelog);

					ptr_log->app_hash = config.my_hash;
					ptr_log->timestamp = _r_unixtime_now ();

					ptr_log->af = AF_INET;
					ptr_log->protocol = IPPROTO_TCP;

					ptr_log->filter_id = 777;
					ptr_log->direction = FWP_DIRECTION_OUTBOUND;

					InetPton (ptr_log->af, RM_AD, &ptr_log->remote_addr);
					ptr_log->remote_port = RP_AD;

					ptr_log->path = _r_obj_createstring (_r_app_getbinarypath ());
					ptr_log->filter_name = _r_obj_createstring (FN_AD);

					//_app_formataddress (ptr_log->af, ptr_log->protocol, &ptr_log->remote_addr, 0, &ptr_log->remote_fmt, FMTADDR_USE_PROTOCOL | FMTADDR_RESOLVE_HOST);
					//_app_formataddress (ptr_log->af, ptr_log->protocol, &ptr_log->local_addr, 0, &ptr_log->local_fmt, FMTADDR_USE_PROTOCOL | FMTADDR_RESOLVE_HOST);

					PITEM_APP ptr_app = _app_getappitem (config.my_hash);

					if (ptr_app)
					{
						ptr_app->last_notify = 0;

						_app_notifyadd (config.hnotification, ptr_log, ptr_app);

						_r_obj_dereference (ptr_app);
					}

					break;
				}

				case 999:
				{
					UINT32 flags = FWPM_NET_EVENT_FLAG_APP_ID_SET |
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

					PR_STRING ntPath = NULL;
					_r_path_ntpathfromdos (_r_app_getbinarypath (), &ntPath);

					UINT16 layer_id = 0;
					UINT64 filter_id = 0;

					HANDLE hengine = _wfp_getenginehandle ();

					if (hengine)
					{
						FWPM_LAYER *layer = NULL;
						FWPM_FILTER *filter = NULL;

						if (FwpmLayerGetByKey (hengine, &FWPM_LAYER_ALE_AUTH_CONNECT_V4, &layer) == ERROR_SUCCESS && layer)
							layer_id = layer->layerId;

						if (!filter_ids.empty ())
						{
							if (FwpmFilterGetByKey (hengine, &filter_ids.at (filter_ids.size () - 1), &filter) == ERROR_SUCCESS && filter)
								filter_id = filter->filterId;
						}

						if (layer)
							FwpmFreeMemory ((PVOID*)&layer);

						if (filter)
							FwpmFreeMemory ((PVOID*)&filter);
					}

					LPCWSTR terminator = NULL;

					for (UINT i = 0; i < 255; i++)
					{
						WCHAR addressLocalString[256];
						WCHAR addressRemoteString[256];

						_r_str_printf (addressLocalString, RTL_NUMBER_OF (addressLocalString), LM_AD2, i + 1);
						_r_str_printf (addressRemoteString, RTL_NUMBER_OF (addressRemoteString), RM_AD2, i + 1);

						RtlIpv4StringToAddress (addressLocalString, TRUE, &terminator, &ipv4_local);
						RtlIpv4StringToAddress (addressRemoteString, TRUE, &terminator, &ipv4_remote);

						UINT32 remote_addr = _r_byteswap_ulong (ipv4_remote.S_un.S_addr);
						UINT32 local_addr = _r_byteswap_ulong (ipv4_local.S_un.S_addr);

						_wfp_logcallback (flags, &ft, (UINT8*)_r_obj_getstring (ntPath), NULL, (SID*)config.pbuiltin_admins_sid, IPPROTO_TCP, FWP_IP_VERSION_V4, remote_addr, NULL, RP_AD, local_addr, NULL, LP_AD, layer_id, filter_id, FWP_DIRECTION_OUTBOUND, FALSE, FALSE);
					}

					SAFE_DELETE_REFERENCE (ntPath);

					break;
				}
#endif // _DEBUG || _APP_BETA
			}

			break;
		}
	}

	return FALSE;
}

INT APIENTRY wWinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, INT nCmdShow)
{
	MSG msg;

	if (_r_app_initialize (APP_NAME, APP_NAME_SHORT, APP_VERSION, APP_COPYRIGHT))
	{
		// parse arguments
		{
			INT numargs;
			LPWSTR* arga = CommandLineToArgvW (_r_sys_getimagecommandline (), &numargs);

			if (arga)
			{
				BOOLEAN is_install = FALSE;
				BOOLEAN is_uninstall = FALSE;
				BOOLEAN is_silent = FALSE;
				BOOLEAN is_temporary = FALSE;

				for (INT i = 0; i < numargs; i++)
				{
					if (_r_str_compare (arga[i], L"/install") == 0)
						is_install = TRUE;

					else if (_r_str_compare (arga[i], L"/uninstall") == 0)
						is_uninstall = TRUE;

					else if (_r_str_compare (arga[i], L"/silent") == 0)
						is_silent = TRUE;

					else if (_r_str_compare (arga[i], L"/temp") == 0)
						is_temporary = TRUE;
				}

				LocalFree (arga);

				if (is_install || is_uninstall)
				{
					if (_r_sys_iselevated ())
					{
						_app_initialize ();

						HANDLE hengine = _wfp_getenginehandle ();

						if (hengine)
						{
							if (is_install)
							{
								if (is_silent || ((_wfp_isfiltersinstalled () == InstallDisabled) && _app_installmessage (NULL, TRUE)))
								{
									if (is_temporary)
										config.is_filterstemporary = TRUE;

									_app_profile_load (NULL, NULL);

									if (_wfp_initialize (hengine, TRUE))
										_wfp_installfilters (hengine);

									_wfp_uninitialize (hengine, FALSE);
								}
							}
							else if (is_uninstall)
							{
								if (is_silent || ((_wfp_isfiltersinstalled () != InstallDisabled) && _app_installmessage (NULL, FALSE)))
								{
									_wfp_destroyfilters (hengine);
									_wfp_uninitialize (hengine, TRUE);
								}
							}
						}
					}
					else
					{
						return ERROR_ACCESS_DENIED;
					}

					return ERROR_SUCCESS;
				}
			}
		}

		if (_r_app_createwindow (IDD_MAIN, IDI_MAIN, &DlgProc))
		{
			HACCEL haccel = LoadAccelerators (_r_sys_getimagebase (), MAKEINTRESOURCE (IDA_MAIN));

			if (haccel)
			{
				while (GetMessage (&msg, NULL, 0, 0) > 0)
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

	return ERROR_SUCCESS;
}
