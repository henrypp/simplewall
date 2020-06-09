// simplewall
// Copyright (c) 2016-2020 Henry++

#include "global.hpp"

UINT WM_FINDMSGSTRING = RegisterWindowMessage (FINDMSGSTRING);

THREAD_FN ApplyThread (LPVOID lparam)
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

	_app_profile_save (NULL);

	SetEvent (config.done_evt);

	_r_mem_free (pcontext);

	_r_fastlock_releaseshared (&lock_apply);

	return _r_sys_endthread (ERROR_SUCCESS);
}

THREAD_FN NetworkMonitorThread (LPVOID lparam)
{
	DWORD network_timeout = app.ConfigGetUlong (L"NetworkTimeout", NETWORK_TIMEOUT);

	if (network_timeout && network_timeout != INFINITE)
	{
		HWND hwnd = (HWND)lparam;
		INT network_listview_id = IDC_NETWORK;

		HASHER_MAP checker_map;

		network_timeout = _r_calc_clamp (DWORD, network_timeout, 500, 60 * 1000); // set allowed range

		while (TRUE)
		{
			_app_generate_connections (network_map, checker_map);

			BOOLEAN is_highlighting_enabled = app.ConfigGetBoolean (L"IsEnableHighlighting", TRUE) && app.ConfigGetBoolean (L"IsHighlightConnection", TRUE, L"colors");
			INT current_listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);
			BOOLEAN is_refresh = FALSE;

			// add new connections into list
			for (auto &p : network_map)
			{
				if (checker_map.find (p.first) == checker_map.end () || !checker_map[p.first])
					continue;

				PITEM_NETWORK ptr_network = (PITEM_NETWORK)_r_obj_reference (p.second);

				if (!ptr_network)
					continue;

				LPWSTR local_fmt = NULL;
				LPWSTR remote_fmt = NULL;

				_app_formataddress (ptr_network->af, 0, &ptr_network->local_addr, 0, &local_fmt, 0);
				_app_formataddress (ptr_network->af, 0, &ptr_network->remote_addr, 0, &remote_fmt, 0);

				INT item_id = _r_listview_getitemcount (hwnd, network_listview_id, FALSE);

				_r_listview_additem (hwnd, network_listview_id, item_id, 0, _r_path_getfilename (ptr_network->path), ptr_network->icon_id, I_GROUPIDNONE, p.first);

				_r_listview_setitem (hwnd, network_listview_id, item_id, 1, !_r_str_isempty (local_fmt) ? local_fmt : SZ_EMPTY);
				_r_listview_setitem (hwnd, network_listview_id, item_id, 3, !_r_str_isempty (remote_fmt) ? remote_fmt : SZ_EMPTY);
				_r_listview_setitem (hwnd, network_listview_id, item_id, 5, _app_getprotoname (ptr_network->protocol, ptr_network->af));
				_r_listview_setitem (hwnd, network_listview_id, item_id, 6, _app_getstatename (ptr_network->state, NULL));

				if (ptr_network->local_port)
					_r_listview_setitem (hwnd, network_listview_id, item_id, 2, _app_formatport (ptr_network->local_port, TRUE));

				if (ptr_network->remote_port)
					_r_listview_setitem (hwnd, network_listview_id, item_id, 4, _app_formatport (ptr_network->remote_port, TRUE));

				SAFE_DELETE_MEMORY (local_fmt);
				SAFE_DELETE_MEMORY (remote_fmt);

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

					PITEM_NETWORK ptr_network = _app_getnetworkitem (network_hash);

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

		_app_freethreadpool (threads_pool);

		PITEM_CONTEXT pcontext = (PITEM_CONTEXT)_r_mem_allocatezero (sizeof (ITEM_CONTEXT));

		pcontext->hwnd = hwnd;
		pcontext->is_install = is_install;

		HANDLE hthread = _r_sys_createthreadex (&ApplyThread, (LPVOID)pcontext, TRUE, THREAD_PRIORITY_HIGHEST);

		if (!hthread)
		{
			_r_mem_free (pcontext);
			return FALSE;
		}

		threads_pool.push_back (hthread);
		_r_sys_resumethread (hthread);

		return TRUE;
	}

	_app_profile_save (NULL);

	_r_listview_redraw (hwnd, listview_id, INVALID_INT);

	return FALSE;
}

VOID addcolor (UINT locale_id, LPCWSTR config_name, BOOLEAN is_enabled, LPCWSTR config_value, COLORREF default_clr)
{
	PITEM_COLOR ptr_clr = (PITEM_COLOR)_r_mem_allocatezero (sizeof (ITEM_COLOR));

	if (!_r_str_isempty (config_name))
		_r_str_alloc (&ptr_clr->pcfg_name, _r_str_length (config_name), config_name);

	if (!_r_str_isempty (config_value))
	{
		_r_str_alloc (&ptr_clr->pcfg_value, _r_str_length (config_value), config_value);

		ptr_clr->clr_hash = _r_str_hash (config_value);
		ptr_clr->new_clr = app.ConfigGetUlong (config_value, default_clr, L"colors");
	}

	ptr_clr->default_clr = default_clr;
	ptr_clr->locale_id = locale_id;
	ptr_clr->is_enabled = is_enabled;

	colors.push_back (ptr_clr);
}

BOOLEAN _app_installmessage (HWND hwnd, BOOLEAN is_install)
{
	WCHAR flag[64] = {0};

	WCHAR button_text_1[64] = {0};
	WCHAR button_text_2[64] = {0};

	WCHAR radio_text_1[128] = {0};
	WCHAR radio_text_2[128] = {0};

	WCHAR main[256] = {0};

	TASKDIALOGCONFIG tdc = {0};
	TASKDIALOG_BUTTON td_buttons[2];
	TASKDIALOG_BUTTON td_radios[2];

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

	tdc.pButtons = td_buttons;
	tdc.cButtons = RTL_NUMBER_OF (td_buttons);

	tdc.nDefaultButton = IDYES;

	td_buttons[0].nButtonID = IDYES;
	td_buttons[0].pszButtonText = button_text_1;

	td_buttons[1].nButtonID = IDNO;
	td_buttons[1].pszButtonText = button_text_2;

	_r_str_copy (button_text_1, RTL_NUMBER_OF (button_text_1), app.LocaleString (is_install ? IDS_TRAY_START : IDS_TRAY_STOP, NULL));
	_r_str_copy (button_text_2, RTL_NUMBER_OF (button_text_2), app.LocaleString (IDS_CLOSE, NULL));

	if (is_install)
	{
		_r_str_copy (main, RTL_NUMBER_OF (main), app.LocaleString (IDS_QUESTION_START, NULL));
		_r_str_copy (flag, RTL_NUMBER_OF (flag), app.LocaleString (IDS_DISABLEWINDOWSFIREWALL_CHK, NULL));

		tdc.pRadioButtons = td_radios;
		tdc.cRadioButtons = RTL_NUMBER_OF (td_radios);

		tdc.nDefaultRadioButton = IDYES;

		td_radios[0].nButtonID = IDYES;
		td_radios[0].pszButtonText = radio_text_1;

		td_radios[1].nButtonID = IDNO;
		td_radios[1].pszButtonText = radio_text_2;

		_r_str_copy (radio_text_1, RTL_NUMBER_OF (radio_text_1), app.LocaleString (IDS_INSTALL_PERMANENT, NULL));
		_r_str_copy (radio_text_2, RTL_NUMBER_OF (radio_text_2), app.LocaleString (IDS_INSTALL_TEMPORARY, NULL));

		if (app.ConfigGetBoolean (L"IsDisableWindowsFirewallChecked", TRUE))
			tdc.dwFlags |= TDF_VERIFICATION_FLAG_CHECKED;
	}
	else
	{
		_r_str_copy (main, RTL_NUMBER_OF (main), app.LocaleString (IDS_QUESTION_STOP, NULL));
		_r_str_copy (flag, RTL_NUMBER_OF (flag), app.LocaleString (IDS_ENABLEWINDOWSFIREWALL_CHK, NULL));

		if (app.ConfigGetBoolean (L"IsEnableWindowsFirewallChecked", TRUE))
			tdc.dwFlags |= TDF_VERIFICATION_FLAG_CHECKED;
	}

	INT message_code;
	INT radio_code;
	BOOL is_flagchecked;

	if (_r_msg_taskdialog (&tdc, &message_code, &radio_code, &is_flagchecked))
	{
		if (message_code == IDYES)
		{
			if (is_install)
			{
				config.is_filterstemporary = (radio_code == IDNO);

				app.ConfigSetBoolean (L"IsDisableWindowsFirewallChecked", !!is_flagchecked);

				if (is_flagchecked)
					_mps_changeconfig2 (FALSE);
			}
			else
			{
				app.ConfigSetBoolean (L"IsEnableWindowsFirewallChecked", !!is_flagchecked);

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
			new_val = !app.ConfigGetBoolean (L"BlockOutboundConnections", TRUE);
			break;
		}

		case IDC_RULE_BLOCKINBOUND:
		case IDM_RULE_BLOCKINBOUND:
		{
			new_val = !app.ConfigGetBoolean (L"BlockInboundConnections", TRUE);
			break;
		}

		case IDC_RULE_ALLOWLOOPBACK:
		case IDM_RULE_ALLOWLOOPBACK:
		{
			new_val = !app.ConfigGetBoolean (L"AllowLoopbackConnections", TRUE);
			break;
		}

		case IDC_RULE_ALLOW6TO4:
		case IDM_RULE_ALLOW6TO4:
		{
			new_val = !app.ConfigGetBoolean (L"AllowIPv6", TRUE);
			break;
		}

		case IDC_SECUREFILTERS_CHK:
		case IDM_SECUREFILTERS_CHK:
		{
			new_val = !app.ConfigGetBoolean (L"IsSecureFilters", TRUE);
			break;
		}

		case IDC_USESTEALTHMODE_CHK:
		case IDM_USESTEALTHMODE_CHK:
		{
			new_val = !app.ConfigGetBoolean (L"UseStealthMode", TRUE);
			break;
		}

		case IDC_INSTALLBOOTTIMEFILTERS_CHK:
		case IDM_INSTALLBOOTTIMEFILTERS_CHK:
		{
			new_val = !app.ConfigGetBoolean (L"InstallBoottimeFilters", TRUE);
			break;
		}

		case IDC_USENETWORKRESOLUTION_CHK:
		case IDM_USENETWORKRESOLUTION_CHK:
		{
			new_val = !app.ConfigGetBoolean (L"IsNetworkResolutionsEnabled", FALSE);
			break;
		}

		case IDC_USECERTIFICATES_CHK:
		case IDM_USECERTIFICATES_CHK:
		{
			new_val = !app.ConfigGetBoolean (L"IsCertificatesEnabled", FALSE);
			break;
		}

		case IDC_USEREFRESHDEVICES_CHK:
		case IDM_USEREFRESHDEVICES_CHK:
		{
			new_val = !app.ConfigGetBoolean (L"IsRefreshDevices", TRUE);
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
			app.ConfigSetBoolean (L"BlockOutboundConnections", new_val);
			_r_menu_checkitem (hmenu, IDM_RULE_BLOCKOUTBOUND, 0, MF_BYCOMMAND, new_val);

			break;
		}

		case IDC_RULE_BLOCKINBOUND:
		case IDM_RULE_BLOCKINBOUND:
		{
			app.ConfigSetBoolean (L"BlockInboundConnections", new_val);
			_r_menu_checkitem (hmenu, IDM_RULE_BLOCKINBOUND, 0, MF_BYCOMMAND, new_val);

			break;
		}

		case IDC_RULE_ALLOWLOOPBACK:
		case IDM_RULE_ALLOWLOOPBACK:
		{
			app.ConfigSetBoolean (L"AllowLoopbackConnections", new_val);
			_r_menu_checkitem (hmenu, IDM_RULE_ALLOWLOOPBACK, 0, MF_BYCOMMAND, new_val);

			break;
		}

		case IDC_RULE_ALLOW6TO4:
		case IDM_RULE_ALLOW6TO4:
		{
			app.ConfigSetBoolean (L"AllowIPv6", new_val);
			_r_menu_checkitem (hmenu, IDM_RULE_ALLOW6TO4, 0, MF_BYCOMMAND, new_val);

			break;
		}

		case IDC_USESTEALTHMODE_CHK:
		case IDM_USESTEALTHMODE_CHK:
		{
			app.ConfigSetBoolean (L"UseStealthMode", new_val);
			_r_menu_checkitem (hmenu, IDM_USESTEALTHMODE_CHK, 0, MF_BYCOMMAND, new_val);

			break;
		}

		case IDC_SECUREFILTERS_CHK:
		case IDM_SECUREFILTERS_CHK:
		{
			app.ConfigSetBoolean (L"IsSecureFilters", new_val);
			_r_menu_checkitem (hmenu, IDM_SECUREFILTERS_CHK, 0, MF_BYCOMMAND, new_val);

			HANDLE hengine = _wfp_getenginehandle ();

			if (hengine)
			{
				GUIDS_VEC filter_all;

				_app_setsecurityinfoforprovider (hengine, &GUID_WfpProvider, new_val);
				_app_setsecurityinfoforsublayer (hengine, &GUID_WfpSublayer, new_val);

				if (_wfp_dumpfilters (hengine, &GUID_WfpProvider, &filter_all))
				{
					for (auto &filter_id : filter_all)
						_app_setsecurityinfoforfilter (hengine, &filter_id, new_val, __LINE__);
				}
			}

			break;
		}

		case IDC_INSTALLBOOTTIMEFILTERS_CHK:
		case IDM_INSTALLBOOTTIMEFILTERS_CHK:
		{
			app.ConfigSetBoolean (L"InstallBoottimeFilters", new_val);
			_r_menu_checkitem (hmenu, IDM_INSTALLBOOTTIMEFILTERS_CHK, 0, MF_BYCOMMAND, new_val);

			break;
		}

		case IDC_USENETWORKRESOLUTION_CHK:
		case IDM_USENETWORKRESOLUTION_CHK:
		{
			app.ConfigSetBoolean (L"IsNetworkResolutionsEnabled", new_val);
			_r_menu_checkitem (hmenu, IDM_USENETWORKRESOLUTION_CHK, 0, MF_BYCOMMAND, new_val);

			break;
		}

		case IDC_USECERTIFICATES_CHK:
		case IDM_USECERTIFICATES_CHK:
		{
			app.ConfigSetBoolean (L"IsCertificatesEnabled", new_val);
			_r_menu_checkitem (hmenu, IDM_USECERTIFICATES_CHK, 0, MF_BYCOMMAND, new_val);

			if (new_val)
			{
				for (auto &p : apps)
				{
					PR_OBJECT ptr_app_object = _r_obj2_reference (p.second);

					if (!ptr_app_object)
						continue;

					PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

					if (ptr_app)
					{
						PR_OBJECT ptr_signature_object = _app_getsignatureinfo (p.first, ptr_app);

						if (ptr_signature_object)
							_r_obj2_dereference (ptr_signature_object);
					}

					_r_obj2_dereference (ptr_app_object);
				}
			}

			_r_listview_redraw (app.GetHWND (), (INT)_r_tab_getlparam (app.GetHWND (), IDC_TAB, INVALID_INT), INVALID_INT);

			break;
		}

		case IDC_USEREFRESHDEVICES_CHK:
		case IDM_USEREFRESHDEVICES_CHK:
		{
			app.ConfigSetBoolean (L"IsRefreshDevices", new_val);
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

	HANDLE hengine = _wfp_getenginehandle ();

	if (hengine)
		_wfp_create2filters (hengine, __LINE__);
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
			INT dialog_id = (INT)wparam;

			switch (dialog_id)
			{
				case IDD_SETTINGS_GENERAL:
				{
					CheckDlgButton (hwnd, IDC_ALWAYSONTOP_CHK, app.ConfigGetBoolean (L"AlwaysOnTop", _APP_ALWAYSONTOP) ? BST_CHECKED : BST_UNCHECKED);

#if defined(_APP_HAVE_AUTORUN)
					CheckDlgButton (hwnd, IDC_LOADONSTARTUP_CHK, app.AutorunIsEnabled () ? BST_CHECKED : BST_UNCHECKED);
#endif // _APP_HAVE_AUTORUN

					CheckDlgButton (hwnd, IDC_STARTMINIMIZED_CHK, app.ConfigGetBoolean (L"IsStartMinimized", FALSE) ? BST_CHECKED : BST_UNCHECKED);

#if defined(_APP_HAVE_SKIPUAC)
					CheckDlgButton (hwnd, IDC_SKIPUACWARNING_CHK, app.SkipUacIsEnabled () ? BST_CHECKED : BST_UNCHECKED);
#endif // _APP_HAVE_SKIPUAC

					CheckDlgButton (hwnd, IDC_CHECKUPDATES_CHK, app.ConfigGetBoolean (L"CheckUpdates", TRUE) ? BST_CHECKED : BST_UNCHECKED);

#if defined(_DEBUG) || defined(_APP_BETA)
					CheckDlgButton (hwnd, IDC_CHECKUPDATESBETA_CHK, BST_CHECKED);
					_r_ctrl_enable (hwnd, IDC_CHECKUPDATESBETA_CHK, FALSE);
#else
					CheckDlgButton (hwnd, IDC_CHECKUPDATESBETA_CHK, app.ConfigGetBoolean (L"CheckUpdatesBeta", FALSE) ? BST_CHECKED : BST_UNCHECKED);

					if (!app.ConfigGetBoolean (L"CheckUpdates", TRUE))
						_r_ctrl_enable (hwnd, IDC_CHECKUPDATESBETA_CHK, FALSE);
#endif // _DEBUG || _APP_BETA

					app.LocaleEnum (hwnd, IDC_LANGUAGE, FALSE, 0);

					break;
				}

				case IDD_SETTINGS_RULES:
				{
					CheckDlgButton (hwnd, IDC_RULE_BLOCKOUTBOUND, app.ConfigGetBoolean (L"BlockOutboundConnections", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_RULE_BLOCKINBOUND, app.ConfigGetBoolean (L"BlockInboundConnections", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_RULE_ALLOWLOOPBACK, app.ConfigGetBoolean (L"AllowLoopbackConnections", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_RULE_ALLOW6TO4, app.ConfigGetBoolean (L"AllowIPv6", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_SECUREFILTERS_CHK, app.ConfigGetBoolean (L"IsSecureFilters", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_USESTEALTHMODE_CHK, app.ConfigGetBoolean (L"UseStealthMode", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_INSTALLBOOTTIMEFILTERS_CHK, app.ConfigGetBoolean (L"InstallBoottimeFilters", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_USECERTIFICATES_CHK, app.ConfigGetBoolean (L"IsCertificatesEnabled", FALSE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_USENETWORKRESOLUTION_CHK, app.ConfigGetBoolean (L"IsNetworkResolutionsEnabled", FALSE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_USEREFRESHDEVICES_CHK, app.ConfigGetBoolean (L"IsRefreshDevices", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					HWND htip = _r_ctrl_createtip (hwnd);

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

					CheckDlgButton (hwnd, (IDC_BLOCKLIST_SPY_DISABLE + _r_calc_clamp (INT, app.ConfigGetInteger (L"BlocklistSpyState", 2), 0, 2)), BST_CHECKED);
					CheckDlgButton (hwnd, (IDC_BLOCKLIST_UPDATE_DISABLE + _r_calc_clamp (INT, app.ConfigGetInteger (L"BlocklistUpdateState", 0), 0, 2)), BST_CHECKED);
					CheckDlgButton (hwnd, (IDC_BLOCKLIST_EXTRA_DISABLE + _r_calc_clamp (INT, app.ConfigGetInteger (L"BlocklistExtraState", 0), 0, 2)), BST_CHECKED);

					break;
				}

				case IDD_SETTINGS_INTERFACE:
				{
					CheckDlgButton (hwnd, IDC_CONFIRMEXIT_CHK, app.ConfigGetBoolean (L"ConfirmExit2", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_CONFIRMEXITTIMER_CHK, app.ConfigGetBoolean (L"ConfirmExitTimer", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_CONFIRMLOGCLEAR_CHK, app.ConfigGetBoolean (L"ConfirmLogClear", TRUE) ? BST_CHECKED : BST_UNCHECKED);

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

					for (auto &ptr_clr : colors)
					{
						ptr_clr->new_clr = app.ConfigGetUlong (ptr_clr->pcfg_value, ptr_clr->default_clr, L"colors");

						_r_fastlock_acquireshared (&lock_checkbox);

						_r_listview_additem (hwnd, IDC_COLORS, item, 0, app.LocaleString (ptr_clr->locale_id, NULL), config.icon_id, I_GROUPIDNONE, (LPARAM)ptr_clr);
						_r_listview_setitemcheck (hwnd, IDC_COLORS, item, app.ConfigGetBoolean (ptr_clr->pcfg_name, ptr_clr->is_enabled, L"colors"));

						_r_fastlock_releaseshared (&lock_checkbox);

						item += 1;
					}

					break;
				}

				case IDD_SETTINGS_NOTIFICATIONS:
				{
					CheckDlgButton (hwnd, IDC_ENABLENOTIFICATIONS_CHK, app.ConfigGetBoolean (L"IsNotificationsEnabled", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_NOTIFICATIONSOUND_CHK, app.ConfigGetBoolean (L"IsNotificationsSound", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_NOTIFICATIONONTRAY_CHK, app.ConfigGetBoolean (L"IsNotificationsOnTray", FALSE) ? BST_CHECKED : BST_UNCHECKED);

					SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_SETRANGE32, 0, _R_SECONDSCLOCK_DAY (7));
					SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_SETPOS32, 0, (LPARAM)app.ConfigGetUlong (L"NotificationsTimeout", NOTIFY_TIMEOUT_DEFAULT));

					CheckDlgButton (hwnd, IDC_EXCLUDEBLOCKLIST_CHK, app.ConfigGetBoolean (L"IsExcludeBlocklist", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_EXCLUDECUSTOM_CHK, app.ConfigGetBoolean (L"IsExcludeCustomRules", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDC_ENABLENOTIFICATIONS_CHK, 0), 0);

					break;
				}

				case IDD_SETTINGS_LOGGING:
				{
					CheckDlgButton (hwnd, IDC_ENABLELOG_CHK, app.ConfigGetBoolean (L"IsLogEnabled", FALSE) ? BST_CHECKED : BST_UNCHECKED);

					SetDlgItemText (hwnd, IDC_LOGPATH, app.ConfigGetString (L"LogPath", LOG_PATH_DEFAULT));
					SetDlgItemText (hwnd, IDC_LOGVIEWER, app.ConfigGetString (L"LogViewer", LOG_VIEWER_DEFAULT));

					UDACCEL ud = {0};
					ud.nInc = 64; // set step to 64kb

					SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_SETACCEL, 1, (LPARAM)&ud);
					SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_SETRANGE32, 64, _R_BYTESIZE_KB * 512);
					SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_SETPOS32, 0, (LPARAM)app.ConfigGetUlong (L"LogSizeLimitKb", LOG_SIZE_LIMIT_DEFAULT));

					CheckDlgButton (hwnd, IDC_ENABLEUILOG_CHK, app.ConfigGetBoolean (L"IsLogUiEnabled", FALSE) ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_EXCLUDESTEALTH_CHK, app.ConfigGetBoolean (L"IsExcludeStealth", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, app.ConfigGetBoolean (L"IsExcludeClassifyAllow", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					// win8+
					if (!_r_sys_validversion (6, 2))
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
			rstring recommended = app.LocaleString (IDS_RECOMMENDED, NULL);

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
			SetDlgItemText (hwnd, IDC_TITLE_INTERFACE, app.LocaleString (IDS_TITLE_INTERFACE, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_EXCLUDE, app.LocaleString (IDS_TITLE_EXCLUDE, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_ADVANCED, app.LocaleString (IDS_TITLE_ADVANCED, L":"));

			switch (dialog_id)
			{
				case IDD_SETTINGS_GENERAL:
				{
					SetDlgItemText (hwnd, IDC_ALWAYSONTOP_CHK, app.LocaleString (IDS_ALWAYSONTOP_CHK, NULL));
					SetDlgItemText (hwnd, IDC_LOADONSTARTUP_CHK, app.LocaleString (IDS_LOADONSTARTUP_CHK, NULL));
					SetDlgItemText (hwnd, IDC_STARTMINIMIZED_CHK, app.LocaleString (IDS_STARTMINIMIZED_CHK, NULL));
					SetDlgItemText (hwnd, IDC_SKIPUACWARNING_CHK, app.LocaleString (IDS_SKIPUACWARNING_CHK, NULL));
					SetDlgItemText (hwnd, IDC_CHECKUPDATES_CHK, app.LocaleString (IDS_CHECKUPDATES_CHK, NULL));
					SetDlgItemText (hwnd, IDC_CHECKUPDATESBETA_CHK, app.LocaleString (IDS_CHECKUPDATESBETA_CHK, NULL));

					SetDlgItemText (hwnd, IDC_LANGUAGE_HINT, app.LocaleString (IDS_LANGUAGE_HINT, NULL));

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

					SetDlgItemText (hwnd, IDC_USENETWORKRESOLUTION_CHK, app.LocaleString (IDS_USENETWORKRESOLUTION_CHK, NULL));
					SetDlgItemText (hwnd, IDC_USECERTIFICATES_CHK, app.LocaleString (IDS_USECERTIFICATES_CHK, NULL));
					SetDlgItemText (hwnd, IDC_USEREFRESHDEVICES_CHK, app.LocaleString (IDS_USEREFRESHDEVICES_CHK, _r_fmt (L" (%s)", recommended.GetString ())));

					break;
				}

				case IDD_SETTINGS_BLOCKLIST:
				{
					SetDlgItemText (hwnd, IDC_BLOCKLIST_SPY_DISABLE, app.LocaleString (IDS_DISABLE, NULL));
					SetDlgItemText (hwnd, IDC_BLOCKLIST_SPY_ALLOW, app.LocaleString (IDS_ACTION_ALLOW, NULL));
					SetDlgItemText (hwnd, IDC_BLOCKLIST_SPY_BLOCK, app.LocaleString (IDS_ACTION_BLOCK, _r_fmt (L" (%s)", recommended.GetString ())));

					SetDlgItemText (hwnd, IDC_BLOCKLIST_UPDATE_DISABLE, app.LocaleString (IDS_DISABLE, _r_fmt (L" (%s)", recommended.GetString ())));
					SetDlgItemText (hwnd, IDC_BLOCKLIST_UPDATE_ALLOW, app.LocaleString (IDS_ACTION_ALLOW, NULL));
					SetDlgItemText (hwnd, IDC_BLOCKLIST_UPDATE_BLOCK, app.LocaleString (IDS_ACTION_BLOCK, NULL));

					SetDlgItemText (hwnd, IDC_BLOCKLIST_EXTRA_DISABLE, app.LocaleString (IDS_DISABLE, _r_fmt (L" (%s)", recommended.GetString ())));
					SetDlgItemText (hwnd, IDC_BLOCKLIST_EXTRA_ALLOW, app.LocaleString (IDS_ACTION_ALLOW, NULL));
					SetDlgItemText (hwnd, IDC_BLOCKLIST_EXTRA_BLOCK, app.LocaleString (IDS_ACTION_BLOCK, NULL));

					_r_ctrl_settext (hwnd, IDC_BLOCKLIST_INFO, L"Author: <a href=\"%s\">WindowsSpyBlocker</a> - block spying and tracking on Windows systems.", WINDOWSSPYBLOCKER_URL, WINDOWSSPYBLOCKER_URL);

					break;
				}

				case IDD_SETTINGS_INTERFACE:
				{
					SetDlgItemText (hwnd, IDC_CONFIRMEXIT_CHK, app.LocaleString (IDS_CONFIRMEXIT_CHK, NULL));
					SetDlgItemText (hwnd, IDC_CONFIRMEXITTIMER_CHK, app.LocaleString (IDS_CONFIRMEXITTIMER_CHK, NULL));
					SetDlgItemText (hwnd, IDC_CONFIRMLOGCLEAR_CHK, app.LocaleString (IDS_CONFIRMLOGCLEAR_CHK, NULL));

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
							_r_listview_setitem (hwnd, IDC_COLORS, i, 0, app.LocaleString (ptr_clr->locale_id, NULL));
							_r_fastlock_releaseshared (&lock_checkbox);
						}
					}

					SetDlgItemText (hwnd, IDC_COLORS_HINT, app.LocaleString (IDS_COLORS_HINT, NULL));

					break;
				}

				case IDD_SETTINGS_NOTIFICATIONS:
				{

					SetDlgItemText (hwnd, IDC_ENABLENOTIFICATIONS_CHK, app.LocaleString (IDS_ENABLENOTIFICATIONS_CHK, NULL));
					SetDlgItemText (hwnd, IDC_NOTIFICATIONSOUND_CHK, app.LocaleString (IDS_NOTIFICATIONSOUND_CHK, NULL));
					SetDlgItemText (hwnd, IDC_NOTIFICATIONONTRAY_CHK, app.LocaleString (IDS_NOTIFICATIONONTRAY_CHK, NULL));

					SetDlgItemText (hwnd, IDC_NOTIFICATIONTIMEOUT_HINT, app.LocaleString (IDS_NOTIFICATIONTIMEOUT_HINT, NULL));

					rstring exclude = app.LocaleString (IDS_TITLE_EXCLUDE, NULL);

					SetDlgItemText (hwnd, IDC_EXCLUDEBLOCKLIST_CHK, _r_fmt (L"%s %s", exclude.GetString (), app.LocaleString (IDS_EXCLUDEBLOCKLIST_CHK, NULL).GetString ()));
					SetDlgItemText (hwnd, IDC_EXCLUDECUSTOM_CHK, _r_fmt (L"%s %s", exclude.GetString (), app.LocaleString (IDS_EXCLUDECUSTOM_CHK, NULL).GetString ()));

					break;
				}

				case IDD_SETTINGS_LOGGING:
				{
					SetDlgItemText (hwnd, IDC_ENABLELOG_CHK, app.LocaleString (IDS_ENABLELOG_CHK, NULL));

					SetDlgItemText (hwnd, IDC_LOGVIEWER_HINT, app.LocaleString (IDS_LOGVIEWER_HINT, L":"));
					SetDlgItemText (hwnd, IDC_LOGSIZELIMIT_HINT, app.LocaleString (IDS_LOGSIZELIMIT_HINT, NULL));

					SetDlgItemText (hwnd, IDC_ENABLEUILOG_CHK, app.LocaleString (IDS_ENABLEUILOG_CHK, L" (session only)"));

					rstring exclude = app.LocaleString (IDS_TITLE_EXCLUDE, NULL);

					SetDlgItemText (hwnd, IDC_EXCLUDESTEALTH_CHK, _r_fmt (L"%s %s", exclude.GetString (), app.LocaleString (IDS_EXCLUDESTEALTH_CHK, NULL).GetString ()));
					SetDlgItemText (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, _r_fmt (L"%s %s", exclude.GetString (), app.LocaleString (IDS_EXCLUDECLASSIFYALLOW_CHK, (_r_sys_validversion (6, 2) ? NULL : L" [win8+]")).GetString ()));

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
			INT ctrl_id = GetDlgCtrlID ((HWND)lparam);

			if (ctrl_id == IDC_LOGSIZELIMIT)
				app.ConfigSetUlong (L"LogSizeLimitKb", (DWORD)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));

			else if (ctrl_id == IDC_NOTIFICATIONTIMEOUT)
				app.ConfigSetUlong (L"NotificationsTimeout", (DWORD)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));

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
							_r_str_copy (buffer, RTL_NUMBER_OF (buffer), app.LocaleString (IDS_RULE_BLOCKOUTBOUND, NULL));

						else if (ctrl_id == IDC_RULE_BLOCKINBOUND)
							_r_str_copy (buffer, RTL_NUMBER_OF (buffer), app.LocaleString (IDS_RULE_BLOCKINBOUND, NULL));

						else if (ctrl_id == IDC_RULE_ALLOWLOOPBACK)
							_r_str_copy (buffer, RTL_NUMBER_OF (buffer), app.LocaleString (IDS_RULE_ALLOWLOOPBACK_HINT, NULL));

						else if (ctrl_id == IDC_USESTEALTHMODE_CHK)
							_r_str_copy (buffer, RTL_NUMBER_OF (buffer), app.LocaleString (IDS_USESTEALTHMODE_HINT, NULL));

						else if (ctrl_id == IDC_INSTALLBOOTTIMEFILTERS_CHK)
							_r_str_copy (buffer, RTL_NUMBER_OF (buffer), app.LocaleString (IDS_INSTALLBOOTTIMEFILTERS_HINT, NULL));

						else if (ctrl_id == IDC_SECUREFILTERS_CHK)
							_r_str_copy (buffer, RTL_NUMBER_OF (buffer), app.LocaleString (IDS_SECUREFILTERS_HINT, NULL));

						if (!_r_str_isempty (buffer))
							lpnmdi->lpszText = buffer;
					}

					break;
				}

				case LVN_ITEMCHANGED:
				{
					LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lparam;

					INT listview_id = PtrToInt ((LPVOID)lpnmlv->hdr.idFrom);

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
									app.ConfigSetBoolean (ptr_clr->pcfg_name, is_enabled, L"colors");

									_r_listview_redraw (app.GetHWND (), (INT)_r_tab_getlparam (app.GetHWND (), IDC_TAB, INVALID_INT), INVALID_INT);
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

					INT listview_id = PtrToInt ((LPVOID)lpnmlv->hdr.idFrom);

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

							for (auto &ptr_clr : colors)
							{
								if (ptr_clr)
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
								app.ConfigSetUlong (ptr_clr_current->pcfg_value, cc.rgbResult, L"colors");

								_r_listview_redraw (hwnd, IDC_COLORS, INVALID_INT);
								_r_listview_redraw (app.GetHWND (), (INT)_r_tab_getlparam (app.GetHWND (), IDC_TAB, INVALID_INT), INVALID_INT);
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
					_r_str_copy (lpnmlv->szMarkup, RTL_NUMBER_OF (lpnmlv->szMarkup), app.LocaleString (IDS_STATUS_EMPTY, NULL));

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

					app.ConfigSetBoolean (L"AlwaysOnTop", is_enabled);
					_r_menu_checkitem (GetMenu (app.GetHWND ()), IDM_ALWAYSONTOP_CHK, 0, MF_BYCOMMAND, is_enabled);

					break;
				}

#if defined(_APP_HAVE_AUTORUN)
				case IDC_LOADONSTARTUP_CHK:
				{
					app.AutorunEnable (hwnd, (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					CheckDlgButton (hwnd, ctrl_id, app.AutorunIsEnabled () ? BST_CHECKED : BST_UNCHECKED);

					break;
				}
#endif // _APP_HAVE_AUTORUN

				case IDC_STARTMINIMIZED_CHK:
				{
					app.ConfigSetBoolean (L"IsStartMinimized", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}

#if defined(_APP_HAVE_SKIPUAC)
				case IDC_SKIPUACWARNING_CHK:
				{
					app.SkipUacEnable (hwnd, (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					CheckDlgButton (hwnd, ctrl_id, app.SkipUacIsEnabled () ? BST_CHECKED : BST_UNCHECKED);

					break;
				}
#endif // _APP_HAVE_SKIPUAC

				case IDC_CHECKUPDATES_CHK:
				{
					BOOLEAN is_enabled = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

					app.ConfigSetBoolean (L"CheckUpdates", is_enabled);

#if !defined(_DEBUG) && !defined(_APP_BETA)
					_r_ctrl_enable (hwnd, IDC_CHECKUPDATESBETA_CHK, is_enabled);
#endif // !_DEBUG && !_APP_BETA

					break;
				}

#if !defined(_DEBUG) && !defined(_APP_BETA)
				case IDC_CHECKUPDATESBETA_CHK:
				{
					app.ConfigSetBoolean (L"CheckUpdatesBeta", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
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
					app.ConfigSetBoolean (L"ConfirmExit2", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}

				case IDC_CONFIRMEXITTIMER_CHK:
				{
					app.ConfigSetBoolean (L"ConfirmExitTimer", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}

				case IDC_CONFIRMLOGCLEAR_CHK:
				{
					app.ConfigSetBoolean (L"ConfirmLogClear", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
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
					HMENU hmenu = GetMenu (app.GetHWND ());

					if (ctrl_id >= IDC_BLOCKLIST_SPY_DISABLE && ctrl_id <= IDC_BLOCKLIST_SPY_BLOCK)
					{
						INT new_state = _r_calc_clamp (INT, _r_ctrl_isradiobuttonchecked (hwnd, IDC_BLOCKLIST_SPY_DISABLE, IDC_BLOCKLIST_SPY_BLOCK) - IDC_BLOCKLIST_SPY_DISABLE, 0, 2);

						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_SPY_DISABLE, IDM_BLOCKLIST_SPY_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_SPY_DISABLE + new_state);

						app.ConfigSetInteger (L"BlocklistSpyState", new_state);

						_app_ruleblocklistset (app.GetHWND (), new_state, INVALID_INT, INVALID_INT, TRUE);
					}
					else if (ctrl_id >= IDC_BLOCKLIST_UPDATE_DISABLE && ctrl_id <= IDC_BLOCKLIST_UPDATE_BLOCK)
					{
						INT new_state = _r_calc_clamp (INT, _r_ctrl_isradiobuttonchecked (hwnd, IDC_BLOCKLIST_UPDATE_DISABLE, IDC_BLOCKLIST_UPDATE_BLOCK) - IDC_BLOCKLIST_UPDATE_DISABLE, 0, 2);

						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_UPDATE_DISABLE, IDM_BLOCKLIST_UPDATE_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_UPDATE_DISABLE + new_state);

						app.ConfigSetInteger (L"BlocklistUpdateState", new_state);

						_app_ruleblocklistset (app.GetHWND (), INVALID_INT, new_state, INVALID_INT, TRUE);
					}
					else if (ctrl_id >= IDC_BLOCKLIST_EXTRA_DISABLE && ctrl_id <= IDC_BLOCKLIST_EXTRA_BLOCK)
					{
						INT new_state = _r_calc_clamp (INT, _r_ctrl_isradiobuttonchecked (hwnd, IDC_BLOCKLIST_EXTRA_DISABLE, IDC_BLOCKLIST_EXTRA_BLOCK) - IDC_BLOCKLIST_EXTRA_DISABLE, 0, 2);

						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_EXTRA_DISABLE, IDM_BLOCKLIST_EXTRA_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_EXTRA_DISABLE + new_state);

						app.ConfigSetInteger (L"BlocklistExtraState", new_state);

						_app_ruleblocklistset (app.GetHWND (), INVALID_INT, INVALID_INT, new_state, TRUE);
					}

					break;
				}

				case IDC_ENABLELOG_CHK:
				{
					BOOLEAN is_checked = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);
					BOOLEAN is_enabled = is_checked || (IsDlgButtonChecked (hwnd, IDC_ENABLEUILOG_CHK) == BST_CHECKED);

					app.ConfigSetBoolean (L"IsLogEnabled", is_checked);
					SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_PRESSBUTTON, IDM_TRAY_ENABLELOG_CHK, MAKELPARAM (is_checked, 0));

					_r_ctrl_enable (hwnd, IDC_LOGPATH, is_checked); // input
					_r_ctrl_enable (hwnd, IDC_LOGPATH_BTN, is_checked); // button

					_r_ctrl_enable (hwnd, IDC_LOGVIEWER, is_checked); // input
					_r_ctrl_enable (hwnd, IDC_LOGVIEWER_BTN, is_checked); // button

					EnableWindow ((HWND)SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_GETBUDDY, 0, 0), is_checked);

					_r_ctrl_enable (hwnd, IDC_EXCLUDESTEALTH_CHK, is_enabled);

					// win8+
					if (_r_sys_validversion (6, 2))
						_r_ctrl_enable (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, is_enabled);

					_app_loginit (is_enabled);

					break;
				}

				case IDC_ENABLEUILOG_CHK:
				{
					BOOLEAN is_checked = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);
					BOOLEAN is_enabled = is_checked || (IsDlgButtonChecked (hwnd, IDC_ENABLELOG_CHK) == BST_CHECKED);

					app.ConfigSetBoolean (L"IsLogUiEnabled", is_checked);
					SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_PRESSBUTTON, IDM_TRAY_ENABLEUILOG_CHK, MAKELPARAM (is_checked, 0));

					_r_ctrl_enable (hwnd, IDC_EXCLUDESTEALTH_CHK, is_enabled);

					// win8+
					if (_r_sys_validversion (6, 2))
						_r_ctrl_enable (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, is_enabled);

					break;
				}

				case IDC_LOGPATH:
				{
					if (notify_code == EN_KILLFOCUS)
					{
						rstring logpath = _r_ctrl_gettext (hwnd, ctrl_id);

						if (!logpath.IsEmpty ())
						{
							app.ConfigSetString (L"LogPath", _r_path_unexpand (logpath));

							_app_loginit (app.ConfigGetBoolean (L"IsLogEnabled", FALSE));
						}
					}

					break;
				}

				case IDC_LOGPATH_BTN:
				{
					OPENFILENAME ofn = {0};

					WCHAR path[MAX_PATH] = {0};
					GetDlgItemText (hwnd, IDC_LOGPATH, path, RTL_NUMBER_OF (path));
					_r_str_copy (path, RTL_NUMBER_OF (path), _r_path_expand (path));

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
						_r_str_copy (path, RTL_NUMBER_OF (path), _r_path_unexpand (path));

						app.ConfigSetString (L"LogPath", path);
						SetDlgItemText (hwnd, IDC_LOGPATH, path);

						_app_loginit (app.ConfigGetBoolean (L"IsLogEnabled", FALSE));
					}

					break;
				}

				case IDC_LOGVIEWER:
				{
					if (notify_code == EN_KILLFOCUS)
					{
						rstring logviewer = _r_ctrl_gettext (hwnd, ctrl_id);

						if (!logviewer.IsEmpty ())
							app.ConfigSetString (L"LogViewer", _r_path_unexpand (logviewer));
					}

					break;
				}

				case IDC_LOGVIEWER_BTN:
				{
					OPENFILENAME ofn = {0};

					WCHAR path[MAX_PATH] = {0};
					GetDlgItemText (hwnd, IDC_LOGVIEWER, path, RTL_NUMBER_OF (path));
					_r_str_copy (path, RTL_NUMBER_OF (path), _r_path_expand (path));

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
						_r_str_copy (path, RTL_NUMBER_OF (path), _r_path_unexpand (path));

						app.ConfigSetString (L"LogViewer", path);
						SetDlgItemText (hwnd, IDC_LOGVIEWER, path);
					}

					break;
				}

				case IDC_LOGSIZELIMIT_CTRL:
				{
					if (notify_code == EN_KILLFOCUS)
						app.ConfigSetUlong (L"LogSizeLimitKb", (DWORD)SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_GETPOS32, 0, 0));

					break;
				}

				case IDC_ENABLENOTIFICATIONS_CHK:
				{
					BOOLEAN is_enabled = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

					app.ConfigSetBoolean (L"IsNotificationsEnabled", is_enabled);
					SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_PRESSBUTTON, IDM_TRAY_ENABLENOTIFICATIONS_CHK, MAKELPARAM (is_enabled, 0));

					_r_ctrl_enable (hwnd, IDC_NOTIFICATIONSOUND_CHK, is_enabled);
					_r_ctrl_enable (hwnd, IDC_NOTIFICATIONONTRAY_CHK, is_enabled);

					HWND hbuddy = (HWND)SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_GETBUDDY, 0, 0);

					if (hbuddy)
						EnableWindow (hbuddy, is_enabled);

					_r_ctrl_enable (hwnd, IDC_EXCLUDEBLOCKLIST_CHK, is_enabled);
					_r_ctrl_enable (hwnd, IDC_EXCLUDECUSTOM_CHK, is_enabled);

					_app_notifyrefresh (config.hnotification, FALSE);

					break;
				}

				case IDC_NOTIFICATIONSOUND_CHK:
				{
					app.ConfigSetBoolean (L"IsNotificationsSound", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}

				case IDC_NOTIFICATIONONTRAY_CHK:
				{
					app.ConfigSetBoolean (L"IsNotificationsOnTray", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));

					if (IsWindowVisible (config.hnotification))
						_app_notifysetpos (config.hnotification, TRUE);

					break;
				}

				case IDC_NOTIFICATIONTIMEOUT_CTRL:
				{
					if (notify_code == EN_KILLFOCUS)
						app.ConfigSetUlong (L"NotificationsTimeout", (DWORD)SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_GETPOS32, 0, 0));

					break;
				}

				case IDC_EXCLUDESTEALTH_CHK:
				{
					app.ConfigSetBoolean (L"IsExcludeStealth", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}

				case IDC_EXCLUDECLASSIFYALLOW_CHK:
				{
					app.ConfigSetBoolean (L"IsExcludeClassifyAllow", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}

				case IDC_EXCLUDEBLOCKLIST_CHK:
				{
					app.ConfigSetBoolean (L"IsExcludeBlocklist", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}

				case IDC_EXCLUDECUSTOM_CHK:
				{
					app.ConfigSetBoolean (L"IsExcludeCustomRules", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
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
	INT icon_size_toolbar = _r_calc_clamp (INT, _r_dc_getdpi (hwnd, app.ConfigGetInteger (L"ToolbarSize", _R_SIZE_ITEMHEIGHT)), icon_size_small, icon_size_large);

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
	_app_getfileicon (_r_path_expand (PATH_NTOSKRNL), FALSE, &config.icon_id, &config.hicon_large);
	_app_getfileicon (_r_path_expand (PATH_NTOSKRNL), TRUE, NULL, &config.hicon_small);
	_app_getfileicon (_r_path_expand (PATH_SHELL32), FALSE, &config.icon_service_id, NULL);

	// get default icon for windows store package (win8+)
	if (_r_sys_validversion (6, 2))
	{
		if (!_app_getfileicon (_r_path_expand (PATH_WINSTORE), TRUE, &config.icon_uwp_id, &config.hicon_package))
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
	config.hbmp_rules = _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_SETTINGS), icon_size_small);

	config.hbmp_checked = _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_CHECKED), icon_size_small);
	config.hbmp_unchecked = _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_UNCHECKED), icon_size_small);

	// toolbar imagelist
	if (config.himg_toolbar)
		ImageList_SetIconSize (config.himg_toolbar, icon_size_toolbar, icon_size_toolbar);
	else
		config.himg_toolbar = ImageList_Create (icon_size_toolbar, icon_size_toolbar, ILC_COLOR32 | ILC_HIGHQUALITYSCALE, 0, 0);

	if (config.himg_toolbar)
	{
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_SHIELD_ENABLE), icon_size_toolbar), NULL);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_SHIELD_DISABLE), icon_size_toolbar), NULL);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_REFRESH), icon_size_toolbar), NULL);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_SETTINGS), icon_size_toolbar), NULL);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_NOTIFICATIONS), icon_size_toolbar), NULL);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_LOG), icon_size_toolbar), NULL);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_LOGOPEN), icon_size_toolbar), NULL);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_LOGCLEAR), icon_size_toolbar), NULL);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_ADD), icon_size_toolbar), NULL);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_DONATE), icon_size_toolbar), NULL);
		ImageList_Add (config.himg_toolbar, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_LOGUI), icon_size_toolbar), NULL);
	}

	// rules imagelist (small)
	if (config.himg_rules_small)
		ImageList_SetIconSize (config.himg_rules_small, icon_size_small, icon_size_small);
	else
		config.himg_rules_small = ImageList_Create (icon_size_small, icon_size_small, ILC_COLOR32 | ILC_HIGHQUALITYSCALE, 2, 2);

	if (config.himg_rules_small)
	{
		ImageList_Add (config.himg_rules_small, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_ALLOW), icon_size_small), NULL);
		ImageList_Add (config.himg_rules_small, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_BLOCK), icon_size_small), NULL);
	}

	// rules imagelist (large)
	if (config.himg_rules_large)
		ImageList_SetIconSize (config.himg_rules_large, icon_size_large, icon_size_large);
	else
		config.himg_rules_large = ImageList_Create (icon_size_large, icon_size_large, ILC_COLOR32 | ILC_HIGHQUALITYSCALE, 2, 2);

	if (config.himg_rules_large)
	{
		ImageList_Add (config.himg_rules_large, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_ALLOW), icon_size_large), NULL);
		ImageList_Add (config.himg_rules_large, _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_BLOCK), icon_size_large), NULL);
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

	SetWindowPos (GetDlgItem (hwnd, IDC_TAB), NULL, 0, 0, _r_calc_rectwidth (INT, &rc), _r_calc_rectheight (INT, &rc), SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

	HINSTANCE hinst = app.GetHINSTANCE ();
	DWORD listview_ex_style = LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_CHECKBOXES | LVS_EX_HEADERINALLVIEWS;
	DWORD listview_style = WS_CHILD | WS_TABSTOP | LVS_SHOWSELALWAYS | LVS_REPORT | LVS_SHAREIMAGELISTS | LVS_AUTOARRANGE;
	INT tabs_count = 0;

	CreateWindowEx (0, WC_LISTVIEW, NULL, listview_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_APPS_PROFILE, hinst, NULL);
	_r_tab_additem (hwnd, IDC_TAB, tabs_count++, app.LocaleString (IDS_TAB_APPS, NULL), I_IMAGENONE, IDC_APPS_PROFILE);

	CreateWindowEx (0, WC_LISTVIEW, NULL, listview_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_APPS_SERVICE, hinst, NULL);
	_r_tab_additem (hwnd, IDC_TAB, tabs_count++, app.LocaleString (IDS_TAB_SERVICES, NULL), I_IMAGENONE, IDC_APPS_SERVICE);

	// uwp apps (win8+)
	if (_r_sys_validversion (6, 2))
	{
		CreateWindowEx (0, WC_LISTVIEW, NULL, listview_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_APPS_UWP, hinst, NULL);
		_r_tab_additem (hwnd, IDC_TAB, tabs_count++, app.LocaleString (IDS_TAB_PACKAGES, NULL), I_IMAGENONE, IDC_APPS_UWP);
	}

	if (!app.ConfigGetBoolean (L"IsInternalRulesDisabled", FALSE))
	{
		CreateWindowEx (0, WC_LISTVIEW, NULL, listview_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_RULES_BLOCKLIST, hinst, NULL);
		_r_tab_additem (hwnd, IDC_TAB, tabs_count++, app.LocaleString (IDS_TRAY_BLOCKLIST_RULES, NULL), I_IMAGENONE, IDC_RULES_BLOCKLIST);

		CreateWindowEx (0, WC_LISTVIEW, NULL, listview_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_RULES_SYSTEM, hinst, NULL);
		_r_tab_additem (hwnd, IDC_TAB, tabs_count++, app.LocaleString (IDS_TRAY_SYSTEM_RULES, NULL), I_IMAGENONE, IDC_RULES_SYSTEM);
	}

	CreateWindowEx (0, WC_LISTVIEW, NULL, listview_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_RULES_CUSTOM, hinst, NULL);
	_r_tab_additem (hwnd, IDC_TAB, tabs_count++, app.LocaleString (IDS_TRAY_USER_RULES, NULL), I_IMAGENONE, IDC_RULES_CUSTOM);

	CreateWindowEx (0, WC_LISTVIEW, NULL, listview_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_NETWORK, hinst, NULL);
	_r_tab_additem (hwnd, IDC_TAB, tabs_count++, app.LocaleString (IDS_TAB_NETWORK, NULL), I_IMAGENONE, IDC_NETWORK);

	CreateWindowEx (0, WC_LISTVIEW, NULL, listview_style | LVS_NOSORTHEADER, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, (HMENU)IDC_LOG, hinst, NULL);
	_r_tab_additem (hwnd, IDC_TAB, tabs_count++, app.LocaleString (IDS_TITLE_LOGGING, NULL), I_IMAGENONE, IDC_LOG);

	for (INT i = 0; i < tabs_count; i++)
	{
		INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, i);

		if (!listview_id)
			continue;

		HWND hlistview = GetDlgItem (hwnd, listview_id);

		if (!hlistview)
			continue;

		_app_listviewsetfont (hwnd, listview_id, FALSE);

		if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_RULES_CUSTOM)
		{
			_r_listview_setstyle (hwnd, listview_id, listview_ex_style, TRUE);

			if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
			{
				_r_listview_addcolumn (hwnd, listview_id, 0, app.LocaleString (IDS_NAME, NULL), 0, LVCFMT_LEFT);
				_r_listview_addcolumn (hwnd, listview_id, 1, app.LocaleString (IDS_ADDED, NULL), 0, LVCFMT_RIGHT);
			}
			else
			{
				_r_listview_addcolumn (hwnd, listview_id, 0, app.LocaleString (IDS_NAME, NULL), 0, LVCFMT_LEFT);
				_r_listview_addcolumn (hwnd, listview_id, 1, app.LocaleString (IDS_PROTOCOL, NULL), 0, LVCFMT_RIGHT);
				_r_listview_addcolumn (hwnd, listview_id, 2, app.LocaleString (IDS_DIRECTION, NULL), 0, LVCFMT_RIGHT);
			}

			_r_listview_addgroup (hwnd, listview_id, 0, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
			_r_listview_addgroup (hwnd, listview_id, 1, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
			_r_listview_addgroup (hwnd, listview_id, 2, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
		}
		else if (listview_id == IDC_NETWORK || listview_id == IDC_LOG)
		{
			_r_listview_setstyle (hwnd, listview_id, listview_ex_style & ~LVS_EX_CHECKBOXES, FALSE); // no checkboxes

			_r_listview_addcolumn (hwnd, listview_id, 0, app.LocaleString (IDS_NAME, NULL), 0, LVCFMT_LEFT);
			_r_listview_addcolumn (hwnd, listview_id, 1, app.LocaleString (IDS_ADDRESS, _r_fmt (L" (%s)", SZ_DIRECTION_LOCAL)), 0, LVCFMT_LEFT);
			_r_listview_addcolumn (hwnd, listview_id, 2, app.LocaleString (IDS_PORT, _r_fmt (L" (%s)", SZ_DIRECTION_LOCAL)), 0, LVCFMT_RIGHT);
			_r_listview_addcolumn (hwnd, listview_id, 3, app.LocaleString (IDS_ADDRESS, _r_fmt (L" (%s)", SZ_DIRECTION_REMOTE)), 0, LVCFMT_LEFT);
			_r_listview_addcolumn (hwnd, listview_id, 4, app.LocaleString (IDS_PORT, _r_fmt (L" (%s)", SZ_DIRECTION_REMOTE)), 0, LVCFMT_RIGHT);
			_r_listview_addcolumn (hwnd, listview_id, 5, app.LocaleString (IDS_PROTOCOL, NULL), 0, LVCFMT_RIGHT);

			if (listview_id == IDC_NETWORK)
			{
				_r_listview_addcolumn (hwnd, listview_id, 6, app.LocaleString (IDS_STATE, NULL), 0, LVCFMT_RIGHT);
			}
			else if (listview_id == IDC_LOG)
			{
				_r_listview_addcolumn (hwnd, listview_id, 6, app.LocaleString (IDS_FILTER, NULL), 0, LVCFMT_LEFT);
				_r_listview_addcolumn (hwnd, listview_id, 7, app.LocaleString (IDS_DIRECTION, NULL), 0, LVCFMT_RIGHT);
				_r_listview_addcolumn (hwnd, listview_id, 8, app.LocaleString (IDS_STATE, NULL), 0, LVCFMT_RIGHT);
			}
		}

		_r_tab_adjustchild (hwnd, IDC_TAB, hlistview);

		BringWindowToTop (hlistview); // HACK!!!
	}
}

LPVOID allocate_pugixml (size_t size)
{
	return _r_mem_allocatezero (size);
}

VOID _app_initialize ()
{
	pugi::set_memory_management_functions (&allocate_pugixml, &_r_mem_free); // set allocation routine

	// initialize spinlocks
	_r_fastlock_initialize (&lock_apply);
	_r_fastlock_initialize (&lock_checkbox);
	_r_fastlock_initialize (&lock_logbusy);
	_r_fastlock_initialize (&lock_logthread);
	_r_fastlock_initialize (&lock_transaction);

	// set privileges
	{
		DWORD privileges[] = {
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

	_r_str_printf (config.profile_path, RTL_NUMBER_OF (config.profile_path), L"%s\\" XML_PROFILE, app.GetProfileDirectory ());
	_r_str_printf (config.profile_internal_path, RTL_NUMBER_OF (config.profile_internal_path), L"%s\\" XML_PROFILE_INTERNAL, app.GetProfileDirectory ());

	_r_str_copy (config.profile_path_backup, RTL_NUMBER_OF (config.profile_path_backup), _r_fmt (L"%s.bak", config.profile_path));

	config.ntoskrnl_hash = _r_str_hash (PROC_SYSTEM_NAME);
	config.svchost_hash = _r_str_hash (_r_path_expand (PATH_SVCHOST));
	config.my_hash = _r_str_hash (app.GetBinaryPath ());

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
				rstring item_text = _r_listview_getitemtext (hwnd, listview_id, current_item, 0);

				if (item_text.IsEmpty ())
					continue;

				if (StrStrNIW (item_text, lpfr->lpstrFindWhat, (UINT)item_text.GetLength ()) != NULL)
				{
					_app_showitem (hwnd, listview_id, current_item, INVALID_INT);
					return FALSE;
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

			// init buffered paint
			BufferedPaintInit ();

			// allow drag&drop support
			DragAcceptFiles (hwnd, TRUE);

			// initialize settings
			app.SettingsAddPage (IDD_SETTINGS_GENERAL, IDS_SETTINGS_GENERAL);
			app.SettingsAddPage (IDD_SETTINGS_INTERFACE, IDS_TITLE_INTERFACE);
			app.SettingsAddPage (IDD_SETTINGS_HIGHLIGHTING, IDS_TITLE_HIGHLIGHTING);
			app.SettingsAddPage (IDD_SETTINGS_RULES, IDS_TRAY_RULES);

			if (!app.ConfigGetBoolean (L"IsInternalRulesDisabled", FALSE))
				app.SettingsAddPage (IDD_SETTINGS_BLOCKLIST, IDS_TRAY_BLOCKLIST_RULES);

			// dropped packets logging (win7+)
			app.SettingsAddPage (IDD_SETTINGS_NOTIFICATIONS, IDS_TITLE_NOTIFICATIONS);
			app.SettingsAddPage (IDD_SETTINGS_LOGGING, IDS_TITLE_LOGGING);

			// initialize colors
			addcolor (IDS_HIGHLIGHT_TIMER, L"IsHighlightTimer", TRUE, L"ColorTimer", LISTVIEW_COLOR_TIMER);
			addcolor (IDS_HIGHLIGHT_INVALID, L"IsHighlightInvalid", TRUE, L"ColorInvalid", LISTVIEW_COLOR_INVALID);
			addcolor (IDS_HIGHLIGHT_SPECIAL, L"IsHighlightSpecial", TRUE, L"ColorSpecial", LISTVIEW_COLOR_SPECIAL);
			addcolor (IDS_HIGHLIGHT_SILENT, L"IsHighlightSilent", TRUE, L"ColorSilent", LISTVIEW_COLOR_SILENT);
			addcolor (IDS_HIGHLIGHT_SIGNED, L"IsHighlightSigned", TRUE, L"ColorSigned", LISTVIEW_COLOR_SIGNED);
			addcolor (IDS_HIGHLIGHT_PICO, L"IsHighlightPico", TRUE, L"ColorPico", LISTVIEW_COLOR_PICO);
			addcolor (IDS_HIGHLIGHT_SYSTEM, L"IsHighlightSystem", TRUE, L"ColorSystem", LISTVIEW_COLOR_SYSTEM);
			addcolor (IDS_HIGHLIGHT_SERVICE, L"IsHighlightService", TRUE, L"ColorService", LISTVIEW_COLOR_SERVICE);
			addcolor (IDS_HIGHLIGHT_PACKAGE, L"IsHighlightPackage", TRUE, L"ColorPackage", LISTVIEW_COLOR_PACKAGE);
			addcolor (IDS_HIGHLIGHT_CONNECTION, L"IsHighlightConnection", TRUE, L"ColorConnection", LISTVIEW_COLOR_CONNECTION);

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
			_app_profile_load (hwnd, NULL);

			// add blocklist to update
			if (!app.ConfigGetBoolean (L"IsInternalRulesDisabled", FALSE))
				app.UpdateAddComponent (L"Internal rules", L"profile_internal", _r_fmt (L"%" TEXT (PR_LONG64), config.profile_internal_timestamp), config.profile_internal_path, FALSE);

			// initialize tab
			_app_settab_id (hwnd, app.ConfigGetInteger (L"CurrentTab", IDC_APPS_PROFILE));

			// initialize dropped packets log callback thread (win7+)
			RtlSecureZeroMemory (&log_stack, sizeof (log_stack));
			RtlInitializeSListHead (&log_stack.ListHead);

			// create notification window
			_app_notifycreatewindow ();

			// create network monitor thread
			_r_sys_createthreadex (&NetworkMonitorThread, (LPVOID)hwnd, FALSE, THREAD_PRIORITY_LOWEST);

			// install filters
			{
				ENUM_INSTALL_TYPE install_type = _wfp_isfiltersinstalled ();

				if (install_type != InstallDisabled)
				{
					if (install_type == InstallEnabledTemporary)
						config.is_filterstemporary = TRUE;

					//if (app.ConfigGetBoolean (L"IsDisableWindowsFirewallChecked", TRUE))
					//	_mps_changeconfig2 (FALSE);

					_app_changefilters (hwnd, TRUE, TRUE);
				}
				else
				{
					_r_status_settext (hwnd, IDC_STATUSBAR, 0, app.LocaleString (_app_getinterfacestatelocale (install_type), NULL));
				}
			}

			// set column size when "auto-size" option are disabled
			if (!app.ConfigGetBoolean (L"AutoSizeColumns", TRUE))
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
			_r_tray_create (hwnd, UID, WM_TRAYICON, app.GetSharedImage (app.GetHINSTANCE (), (_wfp_isfiltersinstalled () != InstallDisabled) ? IDI_ACTIVE : IDI_INACTIVE, _r_dc_getsystemmetrics (hwnd, SM_CXSMICON)), APP_NAME, FALSE);

			HMENU hmenu = GetMenu (hwnd);

			if (hmenu)
			{
				if (app.ConfigGetBoolean (L"IsInternalRulesDisabled", FALSE))
					_r_menu_enableitem (hmenu, 4, MF_BYPOSITION, FALSE);

				_r_menu_checkitem (hmenu, IDM_ALWAYSONTOP_CHK, 0, MF_BYCOMMAND, app.ConfigGetBoolean (L"AlwaysOnTop", _APP_ALWAYSONTOP));
				_r_menu_checkitem (hmenu, IDM_SHOWFILENAMESONLY_CHK, 0, MF_BYCOMMAND, app.ConfigGetBoolean (L"ShowFilenames", TRUE));
				_r_menu_checkitem (hmenu, IDM_AUTOSIZECOLUMNS_CHK, 0, MF_BYCOMMAND, app.ConfigGetBoolean (L"AutoSizeColumns", TRUE));
				_r_menu_checkitem (hmenu, IDM_ENABLESPECIALGROUP_CHK, 0, MF_BYCOMMAND, app.ConfigGetBoolean (L"IsEnableSpecialGroup", TRUE));

				{
					UINT menu_id;
					INT view_type = _r_calc_clamp (INT, app.ConfigGetInteger (L"ViewType", LV_VIEW_DETAILS), LV_VIEW_ICON, LV_VIEW_MAX);

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
					INT icon_size = _r_calc_clamp (INT, app.ConfigGetInteger (L"IconSize", SHIL_SYSSMALL), SHIL_LARGE, SHIL_LAST);

					if (icon_size == SHIL_EXTRALARGE)
						menu_id = IDM_SIZE_EXTRALARGE;

					else if (icon_size == SHIL_LARGE)
						menu_id = IDM_SIZE_LARGE;

					else
						menu_id = IDM_SIZE_SMALL;

					_r_menu_checkitem (hmenu, IDM_SIZE_SMALL, IDM_SIZE_EXTRALARGE, MF_BYCOMMAND, menu_id);
				}

				_r_menu_checkitem (hmenu, IDM_ICONSISHIDDEN, 0, MF_BYCOMMAND, app.ConfigGetBoolean (L"IsIconsHidden", FALSE));

				_r_menu_checkitem (hmenu, IDM_RULE_BLOCKOUTBOUND, 0, MF_BYCOMMAND, app.ConfigGetBoolean (L"BlockOutboundConnections", TRUE));
				_r_menu_checkitem (hmenu, IDM_RULE_BLOCKINBOUND, 0, MF_BYCOMMAND, app.ConfigGetBoolean (L"BlockInboundConnections", TRUE));
				_r_menu_checkitem (hmenu, IDM_RULE_ALLOWLOOPBACK, 0, MF_BYCOMMAND, app.ConfigGetBoolean (L"AllowLoopbackConnections", TRUE));
				_r_menu_checkitem (hmenu, IDM_RULE_ALLOW6TO4, 0, MF_BYCOMMAND, app.ConfigGetBoolean (L"AllowIPv6", TRUE));

				_r_menu_checkitem (hmenu, IDM_SECUREFILTERS_CHK, 0, MF_BYCOMMAND, app.ConfigGetBoolean (L"IsSecureFilters", TRUE));
				_r_menu_checkitem (hmenu, IDM_USESTEALTHMODE_CHK, 0, MF_BYCOMMAND, app.ConfigGetBoolean (L"UseStealthMode", TRUE));
				_r_menu_checkitem (hmenu, IDM_INSTALLBOOTTIMEFILTERS_CHK, 0, MF_BYCOMMAND, app.ConfigGetBoolean (L"InstallBoottimeFilters", TRUE));

				_r_menu_checkitem (hmenu, IDM_USECERTIFICATES_CHK, 0, MF_BYCOMMAND, app.ConfigGetBoolean (L"IsCertificatesEnabled", FALSE));
				_r_menu_checkitem (hmenu, IDM_USENETWORKRESOLUTION_CHK, 0, MF_BYCOMMAND, app.ConfigGetBoolean (L"IsNetworkResolutionsEnabled", FALSE));
				_r_menu_checkitem (hmenu, IDM_USEREFRESHDEVICES_CHK, 0, MF_BYCOMMAND, app.ConfigGetBoolean (L"IsRefreshDevices", TRUE));

				_r_menu_checkitem (hmenu, IDM_BLOCKLIST_SPY_DISABLE, IDM_BLOCKLIST_SPY_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_SPY_DISABLE + _r_calc_clamp (INT, app.ConfigGetInteger (L"BlocklistSpyState", 2), 0, 2));
				_r_menu_checkitem (hmenu, IDM_BLOCKLIST_UPDATE_DISABLE, IDM_BLOCKLIST_UPDATE_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_UPDATE_DISABLE + _r_calc_clamp (INT, app.ConfigGetInteger (L"BlocklistUpdateState", 0), 0, 2));
				_r_menu_checkitem (hmenu, IDM_BLOCKLIST_EXTRA_DISABLE, IDM_BLOCKLIST_EXTRA_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_EXTRA_DISABLE + _r_calc_clamp (INT, app.ConfigGetInteger (L"BlocklistExtraState", 0), 0, 2));
			}

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLENOTIFICATIONS_CHK, NULL, 0, app.ConfigGetBoolean (L"IsNotificationsEnabled", TRUE) ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED, I_IMAGENONE);
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLELOG_CHK, NULL, 0, app.ConfigGetBoolean (L"IsLogEnabled", FALSE) ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED, I_IMAGENONE);
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLEUILOG_CHK, NULL, 0, app.ConfigGetBoolean (L"IsLogUiEnabled", FALSE) ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED, I_IMAGENONE);

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
				app.LocaleMenu (hmenu, IDS_FILE, 0, TRUE, NULL);
				app.LocaleMenu (hmenu, IDS_SETTINGS, IDM_SETTINGS, FALSE, L"...\tF2");
				app.LocaleMenu (hmenu, IDS_ADD_FILE, IDM_ADD_FILE, FALSE, L"...");
				app.LocaleMenu (GetSubMenu (hmenu, 0), IDS_EXPORT, 5, TRUE, NULL);
				app.LocaleMenu (GetSubMenu (hmenu, 0), IDS_IMPORT, 6, TRUE, NULL);
				app.LocaleMenu (hmenu, IDS_IMPORT, IDM_IMPORT, FALSE, L"...\tCtrl+O");
				app.LocaleMenu (hmenu, IDS_EXPORT, IDM_EXPORT, FALSE, L"...\tCtrl+S");
				app.LocaleMenu (hmenu, IDS_EXIT, IDM_EXIT, FALSE, NULL);

				app.LocaleMenu (hmenu, IDS_EDIT, 1, TRUE, NULL);

				app.LocaleMenu (hmenu, IDS_PURGE_UNUSED, IDM_PURGE_UNUSED, FALSE, L"\tCtrl+Shift+X");
				app.LocaleMenu (hmenu, IDS_PURGE_TIMERS, IDM_PURGE_TIMERS, FALSE, L"\tCtrl+Shift+T");

				app.LocaleMenu (hmenu, IDS_FIND, IDM_FIND, FALSE, L"...\tCtrl+F");
				app.LocaleMenu (hmenu, IDS_FINDNEXT, IDM_FINDNEXT, FALSE, L"\tF3");

				app.LocaleMenu (hmenu, IDS_REFRESH, IDM_REFRESH, FALSE, L"\tF5");

				app.LocaleMenu (hmenu, IDS_VIEW, 2, TRUE, NULL);

				app.LocaleMenu (hmenu, IDS_ALWAYSONTOP_CHK, IDM_ALWAYSONTOP_CHK, FALSE, NULL);
				app.LocaleMenu (hmenu, IDS_SHOWFILENAMESONLY_CHK, IDM_SHOWFILENAMESONLY_CHK, FALSE, NULL);
				app.LocaleMenu (hmenu, IDS_AUTOSIZECOLUMNS_CHK, IDM_AUTOSIZECOLUMNS_CHK, FALSE, NULL);
				app.LocaleMenu (hmenu, IDS_ENABLESPECIALGROUP_CHK, IDM_ENABLESPECIALGROUP_CHK, FALSE, NULL);

				app.LocaleMenu (GetSubMenu (hmenu, 2), IDS_ICONS, 5, TRUE, NULL);
				app.LocaleMenu (hmenu, IDS_ICONSSMALL, IDM_SIZE_SMALL, FALSE, NULL);
				app.LocaleMenu (hmenu, IDS_ICONSLARGE, IDM_SIZE_LARGE, FALSE, NULL);
				app.LocaleMenu (hmenu, IDS_ICONSEXTRALARGE, IDM_SIZE_EXTRALARGE, FALSE, NULL);

				app.LocaleMenu (hmenu, IDS_VIEW_ICON, IDM_VIEW_ICON, FALSE, NULL);
				app.LocaleMenu (hmenu, IDS_VIEW_DETAILS, IDM_VIEW_DETAILS, FALSE, NULL);
				app.LocaleMenu (hmenu, IDS_VIEW_TILE, IDM_VIEW_TILE, FALSE, NULL);

				app.LocaleMenu (hmenu, IDS_ICONSISHIDDEN, IDM_ICONSISHIDDEN, FALSE, NULL);

				app.LocaleMenu (GetSubMenu (hmenu, 2), IDS_LANGUAGE, LANG_MENU, TRUE, L" (Language)");

				app.LocaleMenu (hmenu, IDS_FONT, IDM_FONT, FALSE, L"...");

				rstring recommended = app.LocaleString (IDS_RECOMMENDED, NULL);

				app.LocaleMenu (hmenu, IDS_TRAY_RULES, 3, TRUE, NULL);

				app.LocaleMenu (hmenu, IDS_TAB_NETWORK, IDM_CONNECTIONS_TITLE, FALSE, NULL);
				app.LocaleMenu (hmenu, IDS_TITLE_SECURITY, IDM_SECURITY_TITLE, FALSE, NULL);
				app.LocaleMenu (hmenu, IDS_TITLE_ADVANCED, IDM_ADVANCED_TITLE, FALSE, NULL);

				_r_menu_enableitem (hmenu, IDM_CONNECTIONS_TITLE, MF_BYCOMMAND, FALSE);
				_r_menu_enableitem (hmenu, IDM_SECURITY_TITLE, MF_BYCOMMAND, FALSE);
				_r_menu_enableitem (hmenu, IDM_ADVANCED_TITLE, MF_BYCOMMAND, FALSE);

				app.LocaleMenu (hmenu, IDS_RULE_BLOCKOUTBOUND, IDM_RULE_BLOCKOUTBOUND, FALSE, _r_fmt (L" (%s)", recommended.GetString ()));
				app.LocaleMenu (hmenu, IDS_RULE_BLOCKINBOUND, IDM_RULE_BLOCKINBOUND, FALSE, _r_fmt (L" (%s)", recommended.GetString ()));
				app.LocaleMenu (hmenu, IDS_RULE_ALLOWLOOPBACK, IDM_RULE_ALLOWLOOPBACK, FALSE, _r_fmt (L" (%s)", recommended.GetString ()));
				app.LocaleMenu (hmenu, IDS_RULE_ALLOW6TO4, IDM_RULE_ALLOW6TO4, FALSE, _r_fmt (L" (%s)", recommended.GetString ()));

				app.LocaleMenu (hmenu, IDS_SECUREFILTERS_CHK, IDM_SECUREFILTERS_CHK, FALSE, _r_fmt (L" (%s)", recommended.GetString ()));
				app.LocaleMenu (hmenu, IDS_USESTEALTHMODE_CHK, IDM_USESTEALTHMODE_CHK, FALSE, _r_fmt (L" (%s)", recommended.GetString ()));
				app.LocaleMenu (hmenu, IDS_INSTALLBOOTTIMEFILTERS_CHK, IDM_INSTALLBOOTTIMEFILTERS_CHK, FALSE, _r_fmt (L" (%s)", recommended.GetString ()));

				app.LocaleMenu (hmenu, IDS_USENETWORKRESOLUTION_CHK, IDM_USENETWORKRESOLUTION_CHK, FALSE, NULL);
				app.LocaleMenu (hmenu, IDS_USECERTIFICATES_CHK, IDM_USECERTIFICATES_CHK, FALSE, NULL);
				app.LocaleMenu (hmenu, IDS_USEREFRESHDEVICES_CHK, IDM_USEREFRESHDEVICES_CHK, FALSE, _r_fmt (L" (%s)", recommended.GetString ()));

				app.LocaleMenu (hmenu, IDS_TRAY_BLOCKLIST_RULES, 4, TRUE, NULL);

				app.LocaleMenu (hmenu, IDS_BLOCKLIST_SPY, IDM_BLOCKLIST_SPY_TITLE, FALSE, NULL);
				app.LocaleMenu (hmenu, IDS_BLOCKLIST_UPDATE, IDM_BLOCKLIST_UPDATE_TITLE, FALSE, NULL);
				app.LocaleMenu (hmenu, IDS_BLOCKLIST_EXTRA, IDM_BLOCKLIST_EXTRA_TITLE, FALSE, NULL);

				_r_menu_enableitem (hmenu, IDM_BLOCKLIST_SPY_TITLE, MF_BYCOMMAND, FALSE);
				_r_menu_enableitem (hmenu, IDM_BLOCKLIST_UPDATE_TITLE, MF_BYCOMMAND, FALSE);
				_r_menu_enableitem (hmenu, IDM_BLOCKLIST_EXTRA_TITLE, MF_BYCOMMAND, FALSE);

				app.LocaleMenu (hmenu, IDS_DISABLE, IDM_BLOCKLIST_SPY_DISABLE, FALSE, NULL);
				app.LocaleMenu (hmenu, IDS_ACTION_ALLOW, IDM_BLOCKLIST_SPY_ALLOW, FALSE, NULL);
				app.LocaleMenu (hmenu, IDS_ACTION_BLOCK, IDM_BLOCKLIST_SPY_BLOCK, FALSE, _r_fmt (L" (%s)", recommended.GetString ()));

				app.LocaleMenu (hmenu, IDS_DISABLE, IDM_BLOCKLIST_UPDATE_DISABLE, FALSE, _r_fmt (L" (%s)", recommended.GetString ()));
				app.LocaleMenu (hmenu, IDS_ACTION_ALLOW, IDM_BLOCKLIST_UPDATE_ALLOW, FALSE, NULL);
				app.LocaleMenu (hmenu, IDS_ACTION_BLOCK, IDM_BLOCKLIST_UPDATE_BLOCK, FALSE, NULL);

				app.LocaleMenu (hmenu, IDS_DISABLE, IDM_BLOCKLIST_EXTRA_DISABLE, FALSE, _r_fmt (L" (%s)", recommended.GetString ()));
				app.LocaleMenu (hmenu, IDS_ACTION_ALLOW, IDM_BLOCKLIST_EXTRA_ALLOW, FALSE, NULL);
				app.LocaleMenu (hmenu, IDS_ACTION_BLOCK, IDM_BLOCKLIST_EXTRA_BLOCK, FALSE, NULL);

				app.LocaleMenu (hmenu, IDS_HELP, 5, TRUE, NULL);
				app.LocaleMenu (hmenu, IDS_WEBSITE, IDM_WEBSITE, FALSE, NULL);
				app.LocaleMenu (hmenu, IDS_CHECKUPDATES, IDM_CHECKUPDATES, FALSE, NULL);
				app.LocaleMenu (hmenu, IDS_ABOUT, IDM_ABOUT, FALSE, L"\tF1");

				app.LocaleEnum ((HWND)GetSubMenu (hmenu, 2), LANG_MENU, TRUE, IDX_LANGUAGE); // enum localizations
			}

			// localize toolbar
			_app_setinterfacestate (hwnd);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_REFRESH, app.LocaleString (IDS_REFRESH, NULL), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, 0, I_IMAGENONE);
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_SETTINGS, app.LocaleString (IDS_SETTINGS, NULL), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, 0, I_IMAGENONE);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_OPENRULESEDITOR, app.LocaleString (IDS_OPENRULESEDITOR, L"..."), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLENOTIFICATIONS_CHK, app.LocaleString (IDS_ENABLENOTIFICATIONS_CHK, NULL), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLELOG_CHK, app.LocaleString (IDS_ENABLELOG_CHK, NULL), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLEUILOG_CHK, app.LocaleString (IDS_ENABLEUILOG_CHK, L" (session only)"), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_LOGSHOW, app.LocaleString (IDS_LOGSHOW, L" (Ctrl+I)"), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_LOGCLEAR, app.LocaleString (IDS_LOGCLEAR, L" (Ctrl+X)"), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_DONATE, app.LocaleString (IDS_DONATE, NULL), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

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

				_r_tab_setitem (hwnd, IDC_TAB, i, app.LocaleString (locale_id, NULL), I_IMAGENONE, 0);

				if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
				{
					_r_listview_setcolumn (hwnd, listview_id, 0, app.LocaleString (IDS_NAME, NULL), 0);
					_r_listview_setcolumn (hwnd, listview_id, 1, app.LocaleString (IDS_ADDED, NULL), 0);
				}
				else if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM)
				{
					_r_listview_setcolumn (hwnd, listview_id, 0, app.LocaleString (IDS_NAME, NULL), 0);
					_r_listview_setcolumn (hwnd, listview_id, 1, app.LocaleString (IDS_PROTOCOL, NULL), 0);
					_r_listview_setcolumn (hwnd, listview_id, 2, app.LocaleString (IDS_DIRECTION, NULL), 0);
				}
				else if (listview_id == IDC_NETWORK)
				{
					_r_listview_setcolumn (hwnd, listview_id, 0, app.LocaleString (IDS_NAME, NULL), 0);
					_r_listview_setcolumn (hwnd, listview_id, 1, app.LocaleString (IDS_ADDRESS, _r_fmt (L" (%s)", SZ_DIRECTION_LOCAL)), 0);
					_r_listview_setcolumn (hwnd, listview_id, 2, app.LocaleString (IDS_PORT, _r_fmt (L" (%s)", SZ_DIRECTION_LOCAL)), 0);
					_r_listview_setcolumn (hwnd, listview_id, 3, app.LocaleString (IDS_ADDRESS, _r_fmt (L" (%s)", SZ_DIRECTION_REMOTE)), 0);
					_r_listview_setcolumn (hwnd, listview_id, 4, app.LocaleString (IDS_PORT, _r_fmt (L" (%s)", SZ_DIRECTION_REMOTE)), 0);
					_r_listview_setcolumn (hwnd, listview_id, 5, app.LocaleString (IDS_PROTOCOL, NULL), 0);
					_r_listview_setcolumn (hwnd, listview_id, 6, app.LocaleString (IDS_STATE, NULL), 0);
				}
				else if (listview_id == IDC_LOG)
				{
					_r_listview_setcolumn (hwnd, listview_id, 0, app.LocaleString (IDS_NAME, NULL), 0);
					_r_listview_setcolumn (hwnd, listview_id, 1, app.LocaleString (IDS_ADDRESS, _r_fmt (L" (%s)", SZ_DIRECTION_LOCAL)), 0);
					_r_listview_setcolumn (hwnd, listview_id, 2, app.LocaleString (IDS_PORT, _r_fmt (L" (%s)", SZ_DIRECTION_LOCAL)), 0);
					_r_listview_setcolumn (hwnd, listview_id, 3, app.LocaleString (IDS_ADDRESS, _r_fmt (L" (%s)", SZ_DIRECTION_REMOTE)), 0);
					_r_listview_setcolumn (hwnd, listview_id, 4, app.LocaleString (IDS_PORT, _r_fmt (L" (%s)", SZ_DIRECTION_REMOTE)), 0);
					_r_listview_setcolumn (hwnd, listview_id, 5, app.LocaleString (IDS_PROTOCOL, NULL), 0);
					_r_listview_setcolumn (hwnd, listview_id, 6, app.LocaleString (IDS_FILTER, NULL), 0);
					_r_listview_setcolumn (hwnd, listview_id, 7, app.LocaleString (IDS_DIRECTION, NULL), 0);
					_r_listview_setcolumn (hwnd, listview_id, 8, app.LocaleString (IDS_STATE, NULL), 0);
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
			_r_wnd_addstyle (config.hnotification, IDC_RULES_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (config.hnotification, IDC_ALLOW_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (config.hnotification, IDC_BLOCK_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (config.hnotification, IDC_LATER_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

			_app_notifyrefresh (config.hnotification, FALSE);

			break;
		}

		case RM_TASKBARCREATED:
		{
			// refresh tray icon
			_r_tray_destroy (hwnd, UID);
			_r_tray_create (hwnd, UID, WM_TRAYICON, app.GetSharedImage (app.GetHINSTANCE (), (_wfp_isfiltersinstalled () != InstallDisabled) ? IDI_ACTIVE : IDI_INACTIVE, _r_dc_getsystemmetrics (hwnd, SM_CXSMICON)), APP_NAME, FALSE);

			break;
		}

		case RM_DPICHANGED:
		{
			_app_imagelist_init (hwnd);

			SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_SETIMAGELIST, 0, (LPARAM)config.himg_toolbar);

			// reset toolbar information
			_app_setinterfacestate (hwnd);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_REFRESH, app.LocaleString (IDS_REFRESH, NULL), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, 0, I_IMAGENONE);
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_SETTINGS, app.LocaleString (IDS_SETTINGS, NULL), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, 0, I_IMAGENONE);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_OPENRULESEDITOR, app.LocaleString (IDS_OPENRULESEDITOR, L"..."), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLENOTIFICATIONS_CHK, app.LocaleString (IDS_ENABLENOTIFICATIONS_CHK, NULL), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLELOG_CHK, app.LocaleString (IDS_ENABLELOG_CHK, NULL), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLEUILOG_CHK, app.LocaleString (IDS_ENABLEUILOG_CHK, NULL), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_LOGSHOW, app.LocaleString (IDS_LOGSHOW, L" (Ctrl+I)"), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);
			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_LOGCLEAR, app.LocaleString (IDS_LOGCLEAR, L" (Ctrl+X)"), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

			_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_DONATE, app.LocaleString (IDS_DONATE, NULL), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

			_app_toolbar_resize ();

			INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

			if (listview_id)
			{
				_app_listviewsetview (hwnd, listview_id);
				_app_listviewsetfont (hwnd, listview_id, TRUE);
				_app_listviewresize (hwnd, listview_id, FALSE);
			}

			_app_refreshstatus (hwnd, 0);

			break;
		}

		case RM_CONFIG_UPDATE:
		{
			_app_profile_save (NULL);
			_app_profile_load (hwnd, NULL);

			_app_refreshstatus (hwnd, INVALID_INT);

			_app_changefilters (hwnd, TRUE, FALSE);

			break;
		}

		case RM_CONFIG_RESET:
		{
			time_t current_timestamp = (time_t)lparam;

			_app_freerulesconfig_map (rules_config);

			_r_fs_makebackup (config.profile_path, current_timestamp);

			_r_fs_remove (config.profile_path, _R_FLAG_REMOVE_FORCE);
			_r_fs_remove (config.profile_path_backup, _R_FLAG_REMOVE_FORCE);

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
					!app.ShowConfirmMessage (hwnd, NULL, app.LocaleString (IDS_QUESTION_TIMER, NULL), L"ConfirmExitTimer") :
					!app.ShowConfirmMessage (hwnd, NULL, app.LocaleString (IDS_QUESTION_EXIT, NULL), L"ConfirmExit2"))
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
			app.ConfigSetInteger (L"CurrentTab", (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT));

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
			UINT numfiles = DragQueryFile ((HDROP)wparam, UINT32_MAX, NULL, 0);
			SIZE_T app_hash = 0;

			for (UINT i = 0; i < numfiles; i++)
			{
				UINT length = DragQueryFile ((HDROP)wparam, i, NULL, 0) + 1;

				LPWSTR file = (LPWSTR)_r_mem_allocatezero (length * sizeof (WCHAR));

				DragQueryFile ((HDROP)wparam, i, file, length);

				app_hash = _app_addapplication (hwnd, file, 0, 0, 0, FALSE, FALSE);

				_r_mem_free (file);
			}

			DragFinish ((HDROP)wparam);

			_app_profile_save (NULL);
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

					_app_listviewsort (hwnd, listview_id, INVALID_INT, FALSE);
					_app_listviewsetview (hwnd, listview_id);
					_app_listviewsetfont (hwnd, listview_id, FALSE);

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

					INT ctrl_id = PtrToInt ((LPVOID)lpnmcd->nmcd.hdr.idFrom);

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

					INT ctrl_id = PtrToInt ((LPVOID)lpnmlv->hdr.idFrom);

					_app_listviewsort (hwnd, ctrl_id, lpnmlv->iSubItem, TRUE);

					break;
				}

				case LVN_GETINFOTIP:
				{
					LPNMLVGETINFOTIP lpnmlv = (LPNMLVGETINFOTIP)lparam;

					_r_str_copy (lpnmlv->pszText, lpnmlv->cchTextMax, _app_gettooltip (hwnd, lpnmlv));

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

							INT listview_id = PtrToInt ((LPVOID)lpnmlv->hdr.idFrom);
							BOOLEAN is_changed = FALSE;

							BOOLEAN is_enabled = (lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (2);

							if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
							{
								SIZE_T app_hash = lpnmlv->lParam;
								PR_OBJECT ptr_app_object = _app_getappitem (app_hash);

								if (!ptr_app_object)
									break;

								PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

								OBJECTS_VEC rules;

								if (ptr_app)
								{
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

										rules.push_back (ptr_app_object);

										HANDLE hengine = _wfp_getenginehandle ();

										if (hengine)
											_wfp_create3filters (hengine, rules, __LINE__);

										is_changed = TRUE;
									}
								}

								_r_obj2_dereference (ptr_app_object);
							}
							else if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM)
							{
								OBJECTS_VEC rules;

								SIZE_T rule_idx = lpnmlv->lParam;
								PR_OBJECT ptr_rule_object = _app_getrulebyid (rule_idx);

								if (!ptr_rule_object)
									break;

								PITEM_RULE ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

								if (ptr_rule)
								{
									if (ptr_rule->is_enabled != is_enabled)
									{
										_r_fastlock_acquireshared (&lock_checkbox);

										_app_ruleenable (ptr_rule, is_enabled);
										_app_setruleiteminfo (hwnd, listview_id, lpnmlv->iItem, ptr_rule, TRUE);

										_r_fastlock_releaseshared (&lock_checkbox);

										rules.push_back (ptr_rule_object);

										HANDLE hengine = _wfp_getenginehandle ();

										if (hengine)
											_wfp_create4filters (hengine, rules, __LINE__);

										is_changed = TRUE;
									}
								}

								_r_obj2_dereference (ptr_rule_object);
							}

							if (is_changed)
							{
								_app_listviewsort (hwnd, listview_id, INVALID_INT, FALSE);
								_app_refreshstatus (hwnd, listview_id);

								_app_profile_save (NULL);
							}
						}
					}

					break;
				}

				case LVN_GETEMPTYMARKUP:
				{
					NMLVEMPTYMARKUP* lpnmlv = (NMLVEMPTYMARKUP*)lparam;

					lpnmlv->dwFlags = EMF_CENTERED;
					_r_str_copy (lpnmlv->szMarkup, RTL_NUMBER_OF (lpnmlv->szMarkup), app.LocaleString (IDS_STATUS_EMPTY, NULL));

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
					return TRUE;
				}

				case NM_DBLCLK:
				{
					LPNMITEMACTIVATE lpnmlv = (LPNMITEMACTIVATE)lparam;

					if (lpnmlv->iItem == INVALID_INT)
						break;

					INT command_id = 0;
					INT ctrl_id = PtrToInt ((LPVOID)lpnmlv->hdr.idFrom);

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

					INT listview_id = PtrToInt ((LPVOID)lpnmlv->hdr.idFrom);

					if (!(listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_LOG))
						break;

					HMENU hmenu = CreatePopupMenu ();

					if (!hmenu)
						break;

					HMENU hsubmenu1 = NULL;
					HMENU hsubmenu2 = NULL;

					SIZE_T hash_item = _r_listview_getitemlparam (hwnd, listview_id, lpnmlv->iItem);
					INT lv_column_current = lpnmlv->iSubItem;

					if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
					{
						hsubmenu1 = CreatePopupMenu ();
						hsubmenu2 = CreatePopupMenu ();

						// show rules
						AppendMenu (hsubmenu1, MF_STRING, IDM_DISABLENOTIFICATIONS, app.LocaleString (IDS_DISABLENOTIFICATIONS, NULL));

						_app_generate_rulesmenu (hsubmenu1, hash_item);

						// show timers
						AppendMenu (hsubmenu2, MF_STRING, IDM_DISABLETIMER, app.LocaleString (IDS_DISABLETIMER, NULL));
						AppendMenu (hsubmenu2, MF_SEPARATOR, 0, NULL);

						_app_generate_timermenu (hsubmenu2, hash_item);

						AppendMenu (hmenu, MF_STRING, IDM_PROPERTIES, app.LocaleString (IDS_EXPLORE, L"\tEnter"));
						AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);
						AppendMenu (hmenu, MF_POPUP, (UINT_PTR)hsubmenu1, app.LocaleString (IDS_TRAY_RULES, NULL));
						AppendMenu (hmenu, MF_POPUP, (UINT_PTR)hsubmenu2, app.LocaleString (IDS_TIMER, NULL));
						AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);
						AppendMenu (hmenu, MF_STRING, IDM_DELETE, app.LocaleString (IDS_DELETE, L"\tDel"));
						AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);
						AppendMenu (hmenu, MF_STRING, IDM_CHECK, app.LocaleString (IDS_CHECK, NULL));
						AppendMenu (hmenu, MF_STRING, IDM_UNCHECK, app.LocaleString (IDS_UNCHECK, NULL));
						AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);
						AppendMenu (hmenu, MF_STRING, IDM_SELECT_ALL, app.LocaleString (IDS_SELECT_ALL, L"\tCtrl+A"));
						AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);
						AppendMenu (hmenu, MF_STRING, IDM_COPY, app.LocaleString (IDS_COPY, L"\tCtrl+C"));
						AppendMenu (hmenu, MF_STRING, IDM_COPY2, app.LocaleString (IDS_COPY, _r_fmt (L" \"%s\"", _r_listview_getcolumntext (hwnd, listview_id, lv_column_current).GetString ())));

						SetMenuDefaultItem (hmenu, IDM_PROPERTIES, MF_BYCOMMAND);

						_r_menu_checkitem (hmenu, IDM_DISABLENOTIFICATIONS, 0, MF_BYCOMMAND, (BOOL)_app_getappinfo (hash_item, InfoIsSilent) != FALSE);

						if (listview_id != IDC_APPS_PROFILE)
							_r_menu_enableitem (hmenu, IDM_DELETE, MF_BYCOMMAND, FALSE);
					}
					else if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM)
					{
						if (listview_id == IDC_RULES_CUSTOM)
							AppendMenu (hmenu, MF_STRING, IDM_OPENRULESEDITOR, app.LocaleString (IDS_ADD, L"..."));

						AppendMenu (hmenu, MF_STRING, IDM_PROPERTIES, app.LocaleString (IDS_EDIT2, L"...\tEnter"));

						if (listview_id == IDC_RULES_CUSTOM)
						{
							AppendMenu (hmenu, MF_STRING, IDM_DELETE, app.LocaleString (IDS_DELETE, L"\tDel"));

							PR_OBJECT ptr_rule_object = _app_getrulebyid (hash_item);

							if (ptr_rule_object)
							{
								PITEM_RULE ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

								if (ptr_rule && ptr_rule->is_readonly)
									_r_menu_enableitem (hmenu, IDM_DELETE, MF_BYCOMMAND, FALSE);

								_r_obj2_dereference (ptr_rule_object);
							}
						}

						AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);
						AppendMenu (hmenu, MF_STRING, IDM_CHECK, app.LocaleString (IDS_CHECK, NULL));
						AppendMenu (hmenu, MF_STRING, IDM_UNCHECK, app.LocaleString (IDS_UNCHECK, NULL));
						AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);
						AppendMenu (hmenu, MF_STRING, IDM_SELECT_ALL, app.LocaleString (IDS_SELECT_ALL, L"\tCtrl+A"));
						AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);
						AppendMenu (hmenu, MF_STRING, IDM_COPY, app.LocaleString (IDS_COPY, L"\tCtrl+C"));
						AppendMenu (hmenu, MF_STRING, IDM_COPY2, app.LocaleString (IDS_COPY, _r_fmt (L" \"%s\"", _r_listview_getcolumntext (hwnd, listview_id, lv_column_current).GetString ())));

						SetMenuDefaultItem (hmenu, IDM_PROPERTIES, MF_BYCOMMAND);
					}
					else if (listview_id == IDC_NETWORK)
					{
						AppendMenu (hmenu, MF_STRING, IDM_PROPERTIES, app.LocaleString (IDS_SHOWINLIST, L"\tEnter"));
						AppendMenu (hmenu, MF_STRING, IDM_OPENRULESEDITOR, app.LocaleString (IDS_OPENRULESEDITOR, L"..."));
						AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);
						AppendMenu (hmenu, MF_STRING, IDM_NETWORK_CLOSE, app.LocaleString (IDS_NETWORK_CLOSE, NULL));
						AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);
						AppendMenu (hmenu, MF_STRING, IDM_SELECT_ALL, app.LocaleString (IDS_SELECT_ALL, L"\tCtrl+A"));
						AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);
						AppendMenu (hmenu, MF_STRING, IDM_COPY, app.LocaleString (IDS_COPY, L"\tCtrl+C"));
						AppendMenu (hmenu, MF_STRING, IDM_COPY2, app.LocaleString (IDS_COPY, _r_fmt (L" \"%s\"", _r_listview_getcolumntext (hwnd, listview_id, lv_column_current).GetString ())));

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
						AppendMenu (hmenu, MF_STRING, IDM_PROPERTIES, app.LocaleString (IDS_SHOWINLIST, L"\tEnter"));
						AppendMenu (hmenu, MF_STRING, IDM_OPENRULESEDITOR, app.LocaleString (IDS_OPENRULESEDITOR, L"..."));
						AppendMenu (hmenu, MF_STRING, IDM_TRAY_LOGCLEAR, app.LocaleString (IDS_LOGCLEAR, L"\tCtrl+X"));
						AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);
						AppendMenu (hmenu, MF_STRING, IDM_SELECT_ALL, app.LocaleString (IDS_SELECT_ALL, L"\tCtrl+A"));
						AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);
						AppendMenu (hmenu, MF_STRING, IDM_COPY, app.LocaleString (IDS_COPY, L"\tCtrl+C"));
						AppendMenu (hmenu, MF_STRING, IDM_COPY2, app.LocaleString (IDS_COPY, _r_fmt (L" \"%s\"", _r_listview_getcolumntext (hwnd, listview_id, lv_column_current).GetString ())));

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

					BOOLEAN is_filtersinstalled = (_wfp_isfiltersinstalled () != InstallDisabled);

					HMENU hmenu = LoadMenu (NULL, MAKEINTRESOURCE (IDM_TRAY));
					HMENU hsubmenu = GetSubMenu (hmenu, 0);

					{
						MENUITEMINFO mii = {0};

						mii.cbSize = sizeof (mii);
						mii.fMask = MIIM_BITMAP;

						mii.hbmpItem = is_filtersinstalled ? config.hbmp_disable : config.hbmp_enable;
						SetMenuItemInfo (hsubmenu, IDM_TRAY_START, FALSE, &mii);
					}

					// localize
					app.LocaleMenu (hsubmenu, IDS_TRAY_SHOW, IDM_TRAY_SHOW, FALSE, NULL);
					app.LocaleMenu (hsubmenu, is_filtersinstalled ? IDS_TRAY_STOP : IDS_TRAY_START, IDM_TRAY_START, FALSE, NULL);

					app.LocaleMenu (hsubmenu, IDS_TITLE_NOTIFICATIONS, NOTIFICATIONS_ID, TRUE, NULL);
					app.LocaleMenu (hsubmenu, IDS_TITLE_LOGGING, LOGGING_ID, TRUE, NULL);

					app.LocaleMenu (hsubmenu, IDS_ENABLENOTIFICATIONS_CHK, IDM_TRAY_ENABLENOTIFICATIONS_CHK, FALSE, NULL);
					app.LocaleMenu (hsubmenu, IDS_NOTIFICATIONSOUND_CHK, IDM_TRAY_ENABLENOTIFICATIONSSOUND_CHK, FALSE, NULL);
					app.LocaleMenu (hsubmenu, IDS_NOTIFICATIONONTRAY_CHK, IDM_TRAY_NOTIFICATIONONTRAY_CHK, FALSE, NULL);

					app.LocaleMenu (hsubmenu, IDS_ENABLELOG_CHK, IDM_TRAY_ENABLELOG_CHK, FALSE, NULL);
					app.LocaleMenu (hsubmenu, IDS_ENABLEUILOG_CHK, IDM_TRAY_ENABLEUILOG_CHK, FALSE, L" (session only)");
					app.LocaleMenu (hsubmenu, IDS_LOGSHOW, IDM_TRAY_LOGSHOW, FALSE, NULL);
					app.LocaleMenu (hsubmenu, IDS_LOGCLEAR, IDM_TRAY_LOGCLEAR, FALSE, NULL);

					if (_r_fs_exists (app.GetLogPath ()))
					{
						app.LocaleMenu (hsubmenu, IDS_TRAY_LOGERR, ERRLOG_ID, TRUE, NULL);

						app.LocaleMenu (hsubmenu, IDS_LOGSHOW, IDM_TRAY_LOGSHOW_ERR, FALSE, NULL);
						app.LocaleMenu (hsubmenu, IDS_LOGCLEAR, IDM_TRAY_LOGCLEAR_ERR, FALSE, NULL);
					}
					else
					{
						DeleteMenu (hsubmenu, ERRLOG_ID, MF_BYPOSITION);
					}

					app.LocaleMenu (hsubmenu, IDS_SETTINGS, IDM_TRAY_SETTINGS, FALSE, L"...");
					app.LocaleMenu (hsubmenu, IDS_WEBSITE, IDM_TRAY_WEBSITE, FALSE, NULL);
					app.LocaleMenu (hsubmenu, IDS_ABOUT, IDM_TRAY_ABOUT, FALSE, NULL);
					app.LocaleMenu (hsubmenu, IDS_EXIT, IDM_TRAY_EXIT, FALSE, NULL);

					_r_menu_checkitem (hsubmenu, IDM_TRAY_ENABLENOTIFICATIONS_CHK, 0, MF_BYCOMMAND, app.ConfigGetBoolean (L"IsNotificationsEnabled", TRUE));
					_r_menu_checkitem (hsubmenu, IDM_TRAY_ENABLENOTIFICATIONSSOUND_CHK, 0, MF_BYCOMMAND, app.ConfigGetBoolean (L"IsNotificationsSound", TRUE));
					_r_menu_checkitem (hsubmenu, IDM_TRAY_NOTIFICATIONONTRAY_CHK, 0, MF_BYCOMMAND, app.ConfigGetBoolean (L"IsNotificationsOnTray", FALSE));

					_r_menu_checkitem (hsubmenu, IDM_TRAY_ENABLELOG_CHK, 0, MF_BYCOMMAND, app.ConfigGetBoolean (L"IsLogEnabled", FALSE));
					_r_menu_checkitem (hsubmenu, IDM_TRAY_ENABLEUILOG_CHK, 0, MF_BYCOMMAND, app.ConfigGetBoolean (L"IsLogUiEnabled", FALSE));

					if (!app.ConfigGetBoolean (L"IsNotificationsEnabled", TRUE))
					{
						_r_menu_enableitem (hsubmenu, IDM_TRAY_ENABLENOTIFICATIONSSOUND_CHK, MF_BYCOMMAND, FALSE);
						_r_menu_enableitem (hsubmenu, IDM_TRAY_NOTIFICATIONONTRAY_CHK, MF_BYCOMMAND, FALSE);
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
					if (!app.ConfigGetBoolean (L"IsRefreshDevices", TRUE))
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

			if (HIWORD (wparam) == 0 && ctrl_id >= IDX_LANGUAGE && ctrl_id <= (IDX_LANGUAGE + (INT)app.LocaleGetCount ()))
			{
				app.LocaleApplyFromMenu (GetSubMenu (GetSubMenu (GetMenu (hwnd), 2), LANG_MENU), ctrl_id, IDX_LANGUAGE);
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
				PR_OBJECT ptr_rule_object = _app_getrulebyid (rule_idx);

				if (!ptr_rule_object)
					return FALSE;

				PITEM_RULE ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

				if (ptr_rule)
				{
					while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
					{
						SIZE_T app_hash = _r_listview_getitemlparam (hwnd, listview_id, item);

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
								is_remove = !!(ptr_rule->is_enabled && (ptr_rule->apps.find (app_hash) != ptr_rule->apps.end ()));

							if (is_remove)
							{
								ptr_rule->apps.erase (app_hash);

								if (ptr_rule->apps.empty ())
									_app_ruleenable (ptr_rule, FALSE);
							}
							else
							{
								ptr_rule->apps[app_hash] = TRUE;

								_app_ruleenable (ptr_rule, TRUE);
							}

							_r_fastlock_acquireshared (&lock_checkbox);
							_app_setappiteminfo (hwnd, listview_id, item, app_hash, ptr_app);
							_r_fastlock_releaseshared (&lock_checkbox);
						}

						_r_obj2_dereference (ptr_app_object);
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

					OBJECTS_VEC rules;
					rules.push_back (ptr_rule_object);

					HANDLE hengine = _wfp_getenginehandle ();

					if (hengine)
						_wfp_create4filters (hengine, rules, __LINE__);
				}

				_r_obj2_dereference (ptr_rule_object);

				_app_listviewsort (hwnd, listview_id, INVALID_INT, FALSE);
				_app_refreshstatus (hwnd, listview_id);

				_app_profile_save (NULL);

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
				OBJECTS_VEC rules;

				while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
				{
					SIZE_T app_hash = _r_listview_getitemlparam (hwnd, listview_id, item);
					PR_OBJECT ptr_app_object = _app_getappitem (app_hash);

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
						_r_obj2_dereference (ptr_app_object);
					}
				}

				HANDLE hengine = _wfp_getenginehandle ();

				if (hengine)
					_wfp_create3filters (hengine, rules, __LINE__);

				_app_freeobjects_vec (rules);

				_app_listviewsort (hwnd, listview_id, INVALID_INT, FALSE);
				_app_refreshstatus (hwnd, listview_id);

				_app_profile_save (NULL);

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
					ShellExecute (hwnd, NULL, _APP_WEBSITE_URL, NULL, NULL, SW_SHOWDEFAULT);
					break;
				}

				case IDM_CHECKUPDATES:
				{
					app.UpdateCheck (hwnd);
					break;
				}

				case IDM_DONATE:
				{
					ShellExecute (hwnd, NULL, _r_fmt (_APP_DONATE_URL, APP_NAME_SHORT), NULL, NULL, SW_SHOWDEFAULT);
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
					_r_str_copy (path, RTL_NUMBER_OF (path), XML_PROFILE);

					WCHAR title[MAX_PATH] = {0};
					_r_str_printf (title, RTL_NUMBER_OF (title), L"%s %s...", app.LocaleString (IDS_IMPORT, NULL).GetString (), path);

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
							app.ShowErrorMessage (hwnd, L"Profile loading error!", ERROR_INVALID_DATA, NULL);
						}
						else
						{
							_app_profile_save (config.profile_path_backup); // made backup
							_app_profile_load (hwnd, path); // load profile

							_app_refreshstatus (hwnd, INVALID_INT);

							_app_changefilters (hwnd, TRUE, FALSE);
						}
					}

					break;
				}

				case IDM_EXPORT:
				{
					WCHAR path[MAX_PATH] = {0};
					_r_str_copy (path, RTL_NUMBER_OF (path), XML_PROFILE);

					WCHAR title[MAX_PATH] = {0};
					_r_str_printf (title, RTL_NUMBER_OF (title), L"%s %s...", app.LocaleString (IDS_EXPORT, NULL).GetString (), path);

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
						_app_profile_save (path);
					}

					break;
				}

				case IDM_ALWAYSONTOP_CHK:
				{
					BOOLEAN new_val = !app.ConfigGetBoolean (L"AlwaysOnTop", _APP_ALWAYSONTOP);

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
					app.ConfigSetBoolean (L"AlwaysOnTop", new_val);

					_r_wnd_top (hwnd, new_val);

					break;
				}

				case IDM_AUTOSIZECOLUMNS_CHK:
				{
					BOOLEAN new_val = !app.ConfigGetBoolean (L"AutoSizeColumns", TRUE);

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
					app.ConfigSetBoolean (L"AutoSizeColumns", new_val);

					if (new_val)
						_app_listviewresize (hwnd, (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT), FALSE);

					break;
				}

				case IDM_SHOWFILENAMESONLY_CHK:
				{
					BOOLEAN new_val = !app.ConfigGetBoolean (L"ShowFilenames", TRUE);

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
					app.ConfigSetBoolean (L"ShowFilenames", new_val);

					// regroup apps
					for (auto &p : apps)
					{
						PR_OBJECT ptr_app_object = _r_obj2_reference (p.second);

						if (!ptr_app_object)
							continue;

						SIZE_T app_hash = p.first;
						PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

						if (ptr_app)
						{
							_app_getdisplayname (app_hash, ptr_app, &ptr_app->display_name);

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
						}

						_r_obj2_dereference (ptr_app_object);
					}

					_app_listviewsort (hwnd, (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT), INVALID_INT, FALSE);

					break;
				}

				case IDM_ENABLESPECIALGROUP_CHK:
				{
					BOOLEAN new_val = !app.ConfigGetBoolean (L"IsEnableSpecialGroup", TRUE);

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
					app.ConfigSetBoolean (L"IsEnableSpecialGroup", new_val);

					// regroup apps
					for (auto &p : apps)
					{
						PR_OBJECT ptr_app_object = _r_obj2_reference (p.second);

						if (!ptr_app_object)
							continue;

						SIZE_T app_hash = p.first;
						PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

						if (ptr_app)
						{
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
						}

						_r_obj2_dereference (ptr_app_object);
					}

					INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

					if (listview_id)
					{
						_app_listviewsort (hwnd, listview_id, INVALID_INT, FALSE);
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
					app.ConfigSetInteger (L"ViewType", view_type);

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
					app.ConfigSetInteger (L"IconSize", icon_size);

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
					BOOLEAN new_val = !app.ConfigGetBoolean (L"IsIconsHidden", FALSE);

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
					app.ConfigSetBoolean (L"IsIconsHidden", new_val);

					for (auto &p : apps)
					{
						PR_OBJECT ptr_app_object = _r_obj2_reference (p.second);

						if (!ptr_app_object)
							continue;

						SIZE_T app_hash = p.first;
						PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

						if (ptr_app)
						{
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
						}

						_r_obj2_dereference (ptr_app_object);
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

					_app_listviewinitfont (hwnd, &lf);

					if (ChooseFont (&cf))
					{
						app.ConfigSetString (L"Font", !_r_str_isempty (lf.lfFaceName) ? _r_fmt (L"%s;%d;%d", lf.lfFaceName, _r_dc_fontheighttosize (hwnd, lf.lfHeight), lf.lfWeight) : UI_FONT_DEFAULT);

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

					app.ConfigInit ();

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

						app.ConfigSetInteger (L"BlocklistSpyState", new_state);

						_app_ruleblocklistset (hwnd, new_state, INVALID_INT, INVALID_INT, TRUE);
					}
					else if (ctrl_id >= IDM_BLOCKLIST_UPDATE_DISABLE && ctrl_id <= IDM_BLOCKLIST_UPDATE_BLOCK)
					{
						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_UPDATE_DISABLE, IDM_BLOCKLIST_UPDATE_BLOCK, MF_BYCOMMAND, ctrl_id);

						INT new_state = _r_calc_clamp (INT, ctrl_id - IDM_BLOCKLIST_UPDATE_DISABLE, 0, 2);

						app.ConfigSetInteger (L"BlocklistUpdateState", new_state);

						_app_ruleblocklistset (hwnd, INVALID_INT, new_state, INVALID_INT, TRUE);
					}
					else if (ctrl_id >= IDM_BLOCKLIST_EXTRA_DISABLE && ctrl_id <= IDM_BLOCKLIST_EXTRA_BLOCK)
					{
						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_EXTRA_DISABLE, IDM_BLOCKLIST_EXTRA_BLOCK, MF_BYCOMMAND, ctrl_id);

						INT new_state = _r_calc_clamp (INT, ctrl_id - IDM_BLOCKLIST_EXTRA_DISABLE, 0, 2);

						app.ConfigSetInteger (L"BlocklistExtraState", new_state);

						_app_ruleblocklistset (hwnd, INVALID_INT, INVALID_INT, new_state, TRUE);
					}

					break;
				}

				case IDM_TRAY_ENABLELOG_CHK:
				{
					BOOLEAN new_val = !app.ConfigGetBoolean (L"IsLogEnabled", FALSE);

					_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, ctrl_id, NULL, 0, new_val ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED, I_IMAGENONE);
					app.ConfigSetBoolean (L"IsLogEnabled", new_val);

					_app_loginit (new_val);

					break;
				}

				case IDM_TRAY_ENABLEUILOG_CHK:
				{
					BOOLEAN new_val = !app.ConfigGetBoolean (L"IsLogUiEnabled", FALSE);

					_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, ctrl_id, NULL, 0, new_val ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED, I_IMAGENONE);
					app.ConfigSetBoolean (L"IsLogUiEnabled", new_val);

					break;
				}

				case IDM_TRAY_ENABLENOTIFICATIONS_CHK:
				{
					BOOLEAN new_val = !app.ConfigGetBoolean (L"IsNotificationsEnabled", TRUE);

					_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, ctrl_id, NULL, 0, new_val ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED, I_IMAGENONE);
					app.ConfigSetBoolean (L"IsNotificationsEnabled", new_val);

					_app_notifyrefresh (config.hnotification, TRUE);

					break;
				}

				case IDM_TRAY_ENABLENOTIFICATIONSSOUND_CHK:
				{
					BOOLEAN new_val = !app.ConfigGetBoolean (L"IsNotificationsSound", TRUE);

					app.ConfigSetBoolean (L"IsNotificationsSound", new_val);

					break;
				}

				case IDM_TRAY_NOTIFICATIONONTRAY_CHK:
				{
					BOOLEAN new_val = !app.ConfigGetBoolean (L"IsNotificationsOnTray", FALSE);

					app.ConfigSetBoolean (L"IsNotificationsOnTray", new_val);

					if (IsWindowVisible (config.hnotification))
						_app_notifysetpos (config.hnotification, TRUE);

					break;
				}

				case IDM_TRAY_LOGSHOW:
				{
					if (app.ConfigGetBoolean (L"IsLogUiEnabled", FALSE))
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
						rstring path = _r_path_expand (app.ConfigGetString (L"LogPath", LOG_PATH_DEFAULT));

						if (!_r_fs_exists (path))
							return FALSE;

						if (_r_fs_isvalidhandle (config.hlogfile))
							FlushFileBuffers (config.hlogfile);

						_r_sys_createprocess (NULL, _r_fmt (L"%s \"%s\"", _app_getlogviewer ().GetString (), path.GetString ()), NULL);
					}

					break;
				}

				case IDM_TRAY_LOGCLEAR:
				{
					rstring path = _r_path_expand (app.ConfigGetString (L"LogPath", LOG_PATH_DEFAULT));
					BOOLEAN is_validhandle = _r_fs_isvalidhandle (config.hlogfile);

					if (is_validhandle || _r_fs_exists (path) || !log_arr.empty ())
					{
						if (!app.ShowConfirmMessage (hwnd, NULL, app.LocaleString (IDS_QUESTION, NULL), L"ConfirmLogClear"))
							break;

						_app_freelogstack ();

						_app_logclear ();
						_app_logclear_ui (hwnd);
					}

					break;
				}

				case IDM_TRAY_LOGSHOW_ERR:
				{
					LPCWSTR path = app.GetLogPath ();

					if (_r_fs_exists (path))
						_r_sys_createprocess (NULL, _r_fmt (L"%s \"%s\"", _app_getlogviewer ().GetString (), path), NULL);

					break;
				}

				case IDM_TRAY_LOGCLEAR_ERR:
				{
					if (!app.ShowConfirmMessage (hwnd, NULL, app.LocaleString (IDS_QUESTION, NULL), L"ConfirmLogClear"))
						break;

					LPCWSTR path = app.GetLogPath ();

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
					WCHAR files[_R_BUFFER_LENGTH] = {0};
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
							GetCurrentDirectory (RTL_NUMBER_OF (dir), dir);

							while (*p)
							{
								p += _r_str_length (p) + 1;

								if (*p)
								{
									app_hash = _app_addapplication (hwnd, _r_fmt (L"%s\\%s", dir, p), 0, 0, 0, FALSE, FALSE);
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
						_app_profile_save (NULL);
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
						PR_OBJECT ptr_app_object = _app_getappitem (app_hash);

						if (!ptr_app_object)
							continue;

						PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

						if (ptr_app)
						{
							if (ctrl_id == IDM_DISABLENOTIFICATIONS)
							{
								if (new_val == INVALID_INT)
									new_val = !ptr_app->is_silent;

								if (ptr_app->is_silent != new_val)
									ptr_app->is_silent = new_val;

								if (new_val)
									_app_freenotify (app_hash, ptr_app);
							}
							else if (ctrl_id == IDM_DISABLETIMER)
							{
								_app_timer_reset (hwnd, ptr_app);
							}
						}

						_r_obj2_dereference (ptr_app_object);
					}

					_app_listviewsort (hwnd, listview_id, INVALID_INT, FALSE);
					_app_refreshstatus (hwnd, listview_id);

					_app_profile_save (NULL);

					break;
				}

				case IDM_COPY:
				case IDM_COPY2:
				{
					INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);
					INT item = INVALID_INT;

					INT column_count = _r_listview_getcolumncount (hwnd, listview_id);
					INT column_current = (INT)lparam;
					rstring divider = _r_fmt (L"%c ", DIVIDER_COPY);

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
					INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);
					BOOLEAN new_val = (ctrl_id == IDM_CHECK);

					BOOLEAN is_changed = FALSE;

					if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
					{
						OBJECTS_VEC rules;
						INT item = INVALID_INT;

						while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
						{
							SIZE_T app_hash = _r_listview_getitemlparam (hwnd, listview_id, item);
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

								is_changed = TRUE;

								// do not reset reference counter
							}
							else
							{
								_r_obj2_dereference (ptr_app_object);
							}
						}

						if (is_changed)
						{
							HANDLE hengine = _wfp_getenginehandle ();

							if (hengine)
								_wfp_create3filters (hengine, rules, __LINE__);

							_app_freeobjects_vec (rules);
						}
					}
					else if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM)
					{
						OBJECTS_VEC rules;
						INT item = INVALID_INT;

						while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
						{
							SIZE_T rule_idx = _r_listview_getitemlparam (hwnd, listview_id, item);
							PR_OBJECT ptr_rule_object = _app_getrulebyid (rule_idx);

							if (!ptr_rule_object)
								continue;

							PITEM_RULE ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

							if (ptr_rule && ptr_rule->is_enabled != new_val)
							{
								_app_ruleenable (ptr_rule, new_val);

								_r_fastlock_acquireshared (&lock_checkbox);
								_app_setruleiteminfo (hwnd, listview_id, item, ptr_rule, TRUE);
								_r_fastlock_releaseshared (&lock_checkbox);

								rules.push_back (ptr_rule_object);

								is_changed = TRUE;

								// do not reset reference counter
							}
							else
							{
								_r_obj2_dereference (ptr_rule_object);
							}
						}

						if (is_changed)
						{
							HANDLE hengine = _wfp_getenginehandle ();

							if (hengine)
								_wfp_create4filters (hengine, rules, __LINE__);

							_app_freeobjects_vec (rules);
						}
					}

					if (is_changed)
					{
						_app_listviewsort (hwnd, listview_id, INVALID_INT, FALSE);
						_app_refreshstatus (hwnd, listview_id);

						_app_profile_save (NULL);
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
					PR_OBJECT ptr_rule_object = _r_obj2_allocateex (ptr_rule, &_app_dereferencerule);

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
								ptr_rule->apps[app_hash] = TRUE;
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
								if (_r_str_isempty (ptr_rule->pname))
								{
									rstring item_text = _r_listview_getitemtext (hwnd, listview_id, item, 0);

									_r_str_alloc (&ptr_rule->pname, item_text.GetLength (), item_text);
								}

								if (ptr_network->app_hash && !_r_str_isempty (ptr_network->path))
								{
									if (!_app_isappfound (ptr_network->app_hash))
									{
										_app_addapplication (hwnd, ptr_network->path, 0, 0, 0, FALSE, FALSE);

										_app_refreshstatus (hwnd, listview_id);
										_app_profile_save (NULL);
									}

									ptr_rule->apps[ptr_network->app_hash] = TRUE;
								}

								ptr_rule->protocol = ptr_network->protocol;

								_app_formataddress (ptr_network->af, 0, &ptr_network->remote_addr, ptr_network->remote_port, &ptr_rule->prule_remote, FMTADDR_AS_RULE);
								_app_formataddress (ptr_network->af, 0, &ptr_network->local_addr, ptr_network->local_port, &ptr_rule->prule_local, FMTADDR_AS_RULE);

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
								if (_r_str_isempty (ptr_rule->pname))
								{
									rstring item_text = _r_listview_getitemtext (hwnd, listview_id, item, 0);

									_r_str_alloc (&ptr_rule->pname, item_text.GetLength (), item_text);
								}

								if (ptr_log->app_hash && !_r_str_isempty (ptr_log->path))
								{
									if (!_app_isappfound (ptr_log->app_hash))
									{
										_app_addapplication (hwnd, ptr_log->path, 0, 0, 0, FALSE, FALSE);

										_app_refreshstatus (hwnd, listview_id);
										_app_profile_save (NULL);
									}

									ptr_rule->apps[ptr_log->app_hash] = TRUE;
								}

								ptr_rule->protocol = ptr_log->protocol;
								ptr_rule->direction = ptr_log->direction;

								_app_formataddress (ptr_log->af, 0, &ptr_log->remote_addr, ptr_log->remote_port, &ptr_rule->prule_remote, FMTADDR_AS_RULE);
								_app_formataddress (ptr_log->af, 0, &ptr_log->local_addr, ptr_log->local_port, &ptr_rule->prule_local, FMTADDR_AS_RULE);

								_r_obj_dereference (ptr_log);
							}
						}
					}

					if (DialogBoxParam (NULL, MAKEINTRESOURCE (IDD_EDITOR), hwnd, &EditorProc, (LPARAM)ptr_rule_object))
					{
						SIZE_T rule_idx = rules_arr.size ();
						rules_arr.push_back (_r_obj2_reference (ptr_rule_object));

						INT listview_rules_id = _app_getlistview_id (DataRuleCustom);

						if (listview_rules_id)
						{
							INT item_id = _r_listview_getitemcount (hwnd, listview_rules_id, FALSE);

							_r_fastlock_acquireshared (&lock_checkbox);

							_r_listview_additem (hwnd, listview_rules_id, item_id, 0, ptr_rule->pname, _app_getruleicon (ptr_rule), _app_getrulegroup (ptr_rule), rule_idx);
							_app_setruleiteminfo (hwnd, listview_rules_id, item_id, ptr_rule, TRUE);

							_r_fastlock_releaseshared (&lock_checkbox);
						}

						_app_listviewsort (hwnd, listview_id, INVALID_INT, FALSE);
						_app_refreshstatus (hwnd, listview_id);

						_app_profile_save (NULL);
					}

					_r_obj2_dereference (ptr_rule_object);

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

							LPCWSTR path = (LPCWSTR)_app_getappinfo (app_hash, InfoPath);

							if (path)
								_r_path_explore (path);
						}
						while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT);
					}
					else if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM)
					{
						SIZE_T rule_idx = _r_listview_getitemlparam (hwnd, listview_id, item);
						PR_OBJECT ptr_rule_object = _app_getrulebyid (rule_idx);

						if (!ptr_rule_object)
							break;

						if (DialogBoxParam (NULL, MAKEINTRESOURCE (IDD_EDITOR), hwnd, &EditorProc, (LPARAM)ptr_rule_object))
						{
							PITEM_RULE ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

							if (ptr_rule)
								_app_setruleiteminfo (hwnd, listview_id, item, ptr_rule, TRUE);

							_app_listviewsort (hwnd, listview_id, INVALID_INT, FALSE);
							_app_refreshstatus (hwnd, listview_id);

							_app_profile_save (NULL);
						}

						_r_obj2_dereference (ptr_rule_object);
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
								_app_addapplication (hwnd, ptr_network->path, 0, 0, 0, FALSE, FALSE);

								_app_refreshstatus (hwnd, listview_id);
								_app_profile_save (NULL);
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
								_app_addapplication (hwnd, ptr_log->path, 0, 0, 0, FALSE, FALSE);

								_app_refreshstatus (hwnd, listview_id);
								_app_profile_save (NULL);
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
					INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

					if (listview_id != IDC_APPS_PROFILE && listview_id != IDC_RULES_CUSTOM)
						break;

					INT selected = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETSELECTEDCOUNT, 0, 0);

					if (!selected || app.ShowMessage (hwnd, MB_YESNO | MB_ICONEXCLAMATION, NULL, NULL, _r_fmt (app.LocaleString (IDS_QUESTION_DELETE, NULL), selected)) != IDYES)
						break;

					INT count = _r_listview_getitemcount (hwnd, listview_id, FALSE) - 1;

					GUIDS_VEC guids;

					for (INT i = count; i != INVALID_INT; i--)
					{
						if ((UINT)SendDlgItemMessage (hwnd, listview_id, LVM_GETITEMSTATE, (WPARAM)i, LVNI_SELECTED) == LVNI_SELECTED)
						{
							if (listview_id == IDC_APPS_PROFILE)
							{
								SIZE_T app_hash = _r_listview_getitemlparam (hwnd, listview_id, i);
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

									_app_freeapplication (app_hash);

									_r_obj2_dereferenceex (ptr_app_object, 2);
								}
								else
								{
									_r_obj2_dereference (ptr_app_object);
								}
							}
							else if (listview_id == IDC_RULES_CUSTOM)
							{
								SIZE_T rule_idx = _r_listview_getitemlparam (hwnd, listview_id, i);
								PR_OBJECT ptr_rule_object = _app_getrulebyid (rule_idx);

								if (!ptr_rule_object)
									continue;

								PITEM_RULE ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

								HASHER_MAP apps_checker;

								if (ptr_rule && !ptr_rule->is_readonly) // skip "read-only" rules
								{
									guids.insert (guids.end (), ptr_rule->guids.begin (), ptr_rule->guids.end ());

									for (auto &p : ptr_rule->apps)
										apps_checker[p.first] = TRUE;

									SendDlgItemMessage (hwnd, listview_id, LVM_DELETEITEM, (WPARAM)i, 0);

									rules_arr.at (rule_idx) = NULL;

									_r_obj2_dereferenceex (ptr_rule_object, 2);
								}
								else
								{
									_r_obj2_dereference (ptr_rule_object);
								}

								for (auto &p : apps_checker)
								{
									SIZE_T app_hash = p.first;
									PR_OBJECT ptr_app_object = _app_getappitem (app_hash);

									if (!ptr_app_object)
										continue;

									PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

									if (ptr_app)
									{
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
									}

									_r_obj2_dereference (ptr_app_object);
								}
							}
						}
					}

					HANDLE hengine = _wfp_getenginehandle ();

					if (hengine)
						_wfp_destroyfilters_array (hengine, guids, __LINE__);

					_app_refreshstatus (hwnd, INVALID_INT);
					_app_profile_save (NULL);

					break;
				}

				case IDM_PURGE_UNUSED:
				{
					BOOLEAN is_deleted = FALSE;

					GUIDS_VEC guids;
					std::vector<SIZE_T> apps_list;

					for (auto &p : apps)
					{
						PR_OBJECT ptr_app_object = _r_obj2_reference (p.second);

						if (!ptr_app_object)
							continue;

						SIZE_T app_hash = p.first;
						PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

						if (ptr_app && !ptr_app->is_undeletable && (!_app_isappexists (ptr_app) || ((ptr_app->type != DataAppService && ptr_app->type != DataAppUWP) && !_app_isappused (ptr_app, app_hash))))
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

							guids.insert (guids.end (), ptr_app->guids.begin (), ptr_app->guids.end ());

							apps_list.push_back (app_hash);

							is_deleted = TRUE;
						}

						_r_obj2_dereference (ptr_app_object);
					}

					for (auto &p : apps_list)
						_app_freeapplication (p);

					if (is_deleted)
					{
						HANDLE hengine = _wfp_getenginehandle ();

						if (hengine)
							_wfp_destroyfilters_array (hengine, guids, __LINE__);

						_app_refreshstatus (hwnd, INVALID_INT);
						_app_profile_save (NULL);
					}

					break;
				}

				case IDM_PURGE_TIMERS:
				{
					if (!_app_istimersactive () || app.ShowMessage (hwnd, MB_YESNO | MB_ICONEXCLAMATION, NULL, NULL, app.LocaleString (IDS_QUESTION_TIMERS, NULL)) != IDYES)
						break;

					OBJECTS_VEC rules;

					for (auto &p : apps)
					{
						PR_OBJECT ptr_app_object = _r_obj2_reference (p.second);

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
							_r_obj2_dereference (ptr_app_object);
						}
					}

					HANDLE hengine = _wfp_getenginehandle ();

					if (hengine)
						_wfp_create3filters (hengine, rules, __LINE__);

					_app_freeobjects_vec (rules);

					_app_profile_save (NULL);

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

					_r_str_alloc (&ptr_log->path, _r_str_length (app.GetBinaryPath ()), app.GetBinaryPath ());
					_r_str_alloc (&ptr_log->filter_name, _r_str_length (FN_AD), FN_AD);

					//_app_formataddress (ptr_log->af, ptr_log->protocol, &ptr_log->remote_addr, 0, &ptr_log->remote_fmt, FMTADDR_USE_PROTOCOL | FMTADDR_RESOLVE_HOST);
					//_app_formataddress (ptr_log->af, ptr_log->protocol, &ptr_log->local_addr, 0, &ptr_log->local_fmt, FMTADDR_USE_PROTOCOL | FMTADDR_RESOLVE_HOST);

					PR_OBJECT ptr_app_object = _app_getappitem (config.my_hash);

					if (ptr_app_object)
					{
						PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

						if (ptr_app)
						{
							ptr_app->last_notify = 0;

							_app_notifyadd (config.hnotification, ptr_log, ptr_app);
						}

						_r_obj2_dereference (ptr_app_object);
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

					rstring path = app.GetBinaryPath ();
					_r_path_ntpathfromdos (path);

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
							FwpmFreeMemory ((LPVOID*)&layer);

						if (filter)
							FwpmFreeMemory ((LPVOID*)&filter);
					}

					LPCWSTR terminator = NULL;

					for (UINT i = 0; i < 255; i++)
					{
						RtlIpv4StringToAddress (_r_fmt (RM_AD2, i + 1), TRUE, &terminator, &ipv4_remote);
						RtlIpv4StringToAddress (_r_fmt (LM_AD2, i + 1), TRUE, &terminator, &ipv4_local);

						UINT32 remote_addr = _r_byteswap_ulong (ipv4_remote.S_un.S_addr);
						UINT32 local_addr = _r_byteswap_ulong (ipv4_local.S_un.S_addr);

						_wfp_logcallback (flags, &ft, (UINT8*)path.GetString (), NULL, (SID*)config.pbuiltin_admins_sid, IPPROTO_TCP, FWP_IP_VERSION_V4, remote_addr, NULL, RP_AD, local_addr, NULL, LP_AD, layer_id, filter_id, FWP_DIRECTION_OUTBOUND, FALSE, FALSE);
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

INT APIENTRY wWinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, INT nCmdShow)
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
				BOOLEAN is_install = FALSE;
				BOOLEAN is_uninstall = FALSE;
				BOOLEAN is_silent = FALSE;
				BOOLEAN is_temporary = FALSE;

				for (INT i = 0; i < numargs; i++)
				{
					if (_r_str_compare_length (arga[i], L"/install", 8) == 0)
						is_install = TRUE;

					else if (_r_str_compare_length (arga[i], L"/uninstall", 10) == 0)
						is_uninstall = TRUE;

					else if (_r_str_compare_length (arga[i], L"/silent", 7) == 0)
						is_silent = TRUE;

					else if (_r_str_compare_length (arga[i], L"/temp", 5) == 0)
						is_temporary = TRUE;
				}

				SAFE_DELETE_LOCAL (arga);

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

		if (app.CreateMainWindow (IDD_MAIN, IDI_MAIN, &DlgProc))
		{
			HACCEL haccel = LoadAccelerators (app.GetHINSTANCE (), MAKEINTRESOURCE (IDA_MAIN));

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

	return (INT)msg.wParam;
}
