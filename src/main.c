// simplewall
// Copyright (c) 2016-2021 Henry++

#include "global.h"

UINT WM_FINDMSGSTRING = 0;

THREAD_API ApplyThread (_In_ PVOID lparam)
{
	PITEM_CONTEXT context = (PITEM_CONTEXT)lparam;

	_r_spinlock_acquireshared (&lock_apply);

	HANDLE hengine = _wfp_getenginehandle ();

	if (hengine)
	{
		// dropped packets logging (win7+)
		if (config.is_neteventset)
			_wfp_logunsubscribe (hengine);

		if (context->is_install)
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

	_app_restoreinterfacestate (context->hwnd, TRUE);
	_app_setinterfacestate (context->hwnd);

	_app_profile_save ();

	SetEvent (config.done_evt);

	_r_mem_free (context);

	_r_spinlock_releaseshared (&lock_apply);

	return ERROR_SUCCESS;
}

THREAD_API NetworkMonitorThread (_In_ PVOID lparam)
{
	ULONG network_timeout = _r_config_getulong (L"NetworkTimeout", NETWORK_TIMEOUT);

	if (network_timeout && network_timeout != INFINITE)
	{
		PR_HASHTABLE checker_map;
		PITEM_NETWORK ptr_network;
		PR_HASHSTORE hashstore;
		PR_STRING local_address_string;
		PR_STRING local_port_string;
		PR_STRING remote_address_string;
		PR_STRING remote_port_string;
		HWND hwnd;
		SIZE_T enum_key;
		INT network_listview_id;
		INT current_listview_id;
		INT item_id;
		BOOLEAN is_highlighting_enabled;
		BOOLEAN is_refresh;

		hwnd = (HWND)lparam;
		network_listview_id = IDC_NETWORK;

		network_timeout = _r_calc_clamp32 (network_timeout, 500, 60 * 1000); // set allowed range

		checker_map = _r_obj_createhashtableex (sizeof (R_HASHSTORE), 512, NULL);

		while (TRUE)
		{
			_app_generate_connections (checker_map);

			is_highlighting_enabled = _r_config_getboolean (L"IsEnableHighlighting", TRUE) && _r_config_getbooleanex (L"IsHighlightConnection", TRUE, L"colors");
			current_listview_id = (INT)_r_tab_getitemlparam (hwnd, IDC_TAB, -1);
			is_refresh = FALSE;
			enum_key = 0;

			// add new connections into list
			while (_r_obj_enumhashtable (network_table, &ptr_network, NULL, &enum_key))
			{
				hashstore = _r_obj_findhashtable (checker_map, ptr_network->network_hash);

				if (!hashstore || !hashstore->value_number)
					continue;

				local_address_string = _app_formataddress (ptr_network->af, 0, &ptr_network->local_addr, 0, 0);
				local_port_string = _app_formatport (ptr_network->local_port, ptr_network->protocol, TRUE);
				remote_address_string = _app_formataddress (ptr_network->af, 0, &ptr_network->remote_addr, 0, 0);
				remote_port_string = _app_formatport (ptr_network->remote_port, ptr_network->protocol, TRUE);

				item_id = _r_listview_getitemcount (hwnd, network_listview_id);

				_r_listview_additemex (hwnd, network_listview_id, item_id, 0, _r_obj_isstringempty (ptr_network->path) ? SZ_EMPTY : _r_path_getbasename (ptr_network->path->buffer), ptr_network->icon_id, _app_getnetworkgroup (ptr_network), ptr_network->network_hash);

				_r_listview_setitem (hwnd, network_listview_id, item_id, 1, _r_obj_getstringordefault (local_address_string, SZ_EMPTY));
				_r_listview_setitem (hwnd, network_listview_id, item_id, 3, _r_obj_getstringordefault (remote_address_string, SZ_EMPTY));
				_r_listview_setitem (hwnd, network_listview_id, item_id, 5, _app_getprotoname (ptr_network->protocol, ptr_network->af, SZ_EMPTY));
				_r_listview_setitem (hwnd, network_listview_id, item_id, 6, _app_getconnectionstatusname (ptr_network->state, NULL));

				if (ptr_network->local_port)
					_r_listview_setitem (hwnd, network_listview_id, item_id, 2, _r_obj_getstringorempty (local_port_string));

				if (ptr_network->remote_port)
					_r_listview_setitem (hwnd, network_listview_id, item_id, 4, _r_obj_getstringorempty (remote_port_string));

				SAFE_DELETE_REFERENCE (local_address_string);
				SAFE_DELETE_REFERENCE (local_port_string);
				SAFE_DELETE_REFERENCE (remote_address_string);
				SAFE_DELETE_REFERENCE (remote_port_string);

				// redraw listview item
				if (is_highlighting_enabled)
				{
					INT app_listview_id = PtrToInt (_app_getappinfobyhash (ptr_network->app_hash, InfoListviewId));

					if (app_listview_id && current_listview_id == app_listview_id)
					{
						INT item_pos = _app_getposition (hwnd, app_listview_id, ptr_network->app_hash);

						if (item_pos != -1)
							_r_listview_redraw (hwnd, app_listview_id, item_pos);
					}
				}

				is_refresh = TRUE;
			}

			// refresh network tab as well
			if (is_refresh)
			{
				if (current_listview_id == network_listview_id)
				{
					SendDlgItemMessage (hwnd, network_listview_id, WM_SETREDRAW, FALSE, 0);

					_app_refreshgroups (hwnd, network_listview_id);

					_app_listviewsort (hwnd, network_listview_id, -1, FALSE);
					_app_listviewresize (hwnd, network_listview_id, FALSE);

					SendDlgItemMessage (hwnd, network_listview_id, WM_SETREDRAW, TRUE, 0);
				}
			}

			// remove closed connections from list
			INT item_count = _r_listview_getitemcount (hwnd, network_listview_id);

			if (item_count)
			{
				ULONG_PTR network_hash;
				ULONG_PTR app_hash;

				for (INT i = item_count - 1; i != -1; i--)
				{
					network_hash = _r_listview_getitemlparam (hwnd, network_listview_id, i);

					if (_r_obj_findhashtable (checker_map, network_hash))
						continue;

					_r_listview_deleteitem (hwnd, network_listview_id, i);

					app_hash = _app_getnetworkapp (network_hash);

					_r_spinlock_acquireexclusive (&lock_network);

					_r_obj_removehashtableentry (network_table, network_hash);

					_r_spinlock_releaseexclusive (&lock_network);

					// redraw listview item
					if (app_hash)
					{
						if (is_highlighting_enabled)
						{
							INT app_listview_id = PtrToInt (_app_getappinfobyhash (app_hash, InfoListviewId));

							if (app_listview_id && current_listview_id == app_listview_id)
							{
								INT item_pos = _app_getposition (hwnd, app_listview_id, app_hash);

								if (item_pos != -1)
									_r_listview_redraw (hwnd, app_listview_id, item_pos);
							}
						}
					}
				}
			}

			WaitForSingleObjectEx (NtCurrentThread (), network_timeout, FALSE);
		}
	}

	return ERROR_SUCCESS;
}

BOOLEAN _app_changefilters (_In_ HWND hwnd, _In_ BOOLEAN is_install, _In_ BOOLEAN is_forced)
{
	if (_wfp_isfiltersapplying ())
		return FALSE;

	INT listview_id = (INT)_r_tab_getitemlparam (hwnd, IDC_TAB, -1);

	_app_listviewsort (hwnd, listview_id, -1, FALSE);

	if (is_forced || _wfp_isfiltersinstalled ())
	{
		_app_initinterfacestate (hwnd, TRUE);

		PITEM_CONTEXT context = _r_mem_allocatezero (sizeof (ITEM_CONTEXT));

		context->hwnd = hwnd;
		context->is_install = is_install;

		if (!NT_SUCCESS (_r_sys_createthreadex (&ApplyThread, context, NULL, THREAD_PRIORITY_HIGHEST)))
		{
			_r_mem_free (context);

			return FALSE;
		}


		return TRUE;
	}

	_app_profile_save ();

	_r_listview_redraw (hwnd, listview_id, -1);

	return FALSE;
}

VOID addcolor (_In_ UINT locale_id, _In_ LPCWSTR config_name, _In_ BOOLEAN is_enabled, _In_ LPCWSTR config_value, _In_ COLORREF default_clr)
{
	ITEM_COLOR ptr_clr = {0};

	ptr_clr.config_name = _r_obj_createstring (config_name);
	ptr_clr.config_value = _r_obj_createstring (config_value);
	ptr_clr.hash = _r_obj_getstringhash (ptr_clr.config_value);
	ptr_clr.new_clr = _r_config_getulongex (config_value, default_clr, L"colors");

	ptr_clr.default_clr = default_clr;
	ptr_clr.locale_id = locale_id;
	ptr_clr.is_enabled = is_enabled;

	_r_obj_addarrayitem (colors, &ptr_clr);
}

BOOLEAN _app_installmessage (_In_opt_ HWND hwnd, _In_ BOOLEAN is_install)
{
	WCHAR str_main[256];
	WCHAR radio_text_1[128];
	WCHAR radio_text_2[128];
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

	_r_str_copy (str_button_text_1, RTL_NUMBER_OF (str_button_text_1), _r_locale_getstring (is_install ? IDS_TRAY_START : IDS_TRAY_STOP));
	_r_str_copy (str_button_text_2, RTL_NUMBER_OF (str_button_text_2), _r_locale_getstring (IDS_CLOSE));

	td_buttons[0].nButtonID = IDYES;
	td_buttons[0].pszButtonText = str_button_text_1;

	td_buttons[1].nButtonID = IDNO;
	td_buttons[1].pszButtonText = str_button_text_2;

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

VOID _app_config_apply (_In_ HWND hwnd, _In_ INT ctrl_id)
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
					PR_ARRAY guids = _r_obj_createarrayex (sizeof (GUID), 0x1000, NULL);

					_app_setsecurityinfoforprovider (hengine, &GUID_WfpProvider, new_val);
					_app_setsecurityinfoforsublayer (hengine, &GUID_WfpSublayer, new_val);

					if (_wfp_dumpfilters (hengine, &GUID_WfpProvider, guids))
					{
						for (SIZE_T i = 0; i < _r_obj_getarraysize (guids); i++)
						{
							LPCGUID guid = _r_obj_getarrayitem (guids, i);

							if (guid)
								_app_setsecurityinfoforfilter (hengine, guid, new_val, __LINE__);
						}
					}

					_r_obj_dereference (guids);
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
				PR_STRING signature_string;
				PITEM_APP ptr_app;
				SIZE_T enum_key = 0;

				_r_spinlock_acquireshared (&lock_apps);

				while (_r_obj_enumhashtable (apps, &ptr_app, NULL, &enum_key))
				{
					signature_string = _app_getsignatureinfo (ptr_app);

					if (signature_string)
						_r_obj_dereference (signature_string);
				}

				_r_spinlock_releaseshared (&lock_apps);
			}

			_r_listview_redraw (_r_app_gethwnd (), (INT)_r_tab_getitemlparam (_r_app_gethwnd (), IDC_TAB, -1), -1);

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
			_wfp_create2filters (hengine, __LINE__, FALSE);
	}
}

INT_PTR CALLBACK SettingsProc (_In_ HWND hwnd, _In_ UINT msg, _In_ WPARAM wparam, _In_ LPARAM lparam)
{
	switch (msg)
	{
		case RM_INITIALIZE:
		{
			INT dialog_id = (INT)wparam;

			switch (dialog_id)
			{
				case IDD_SETTINGS_GENERAL:
				{
					CheckDlgButton (hwnd, IDC_ALWAYSONTOP_CHK, _r_config_getboolean (L"AlwaysOnTop", APP_ALWAYSONTOP) ? BST_CHECKED : BST_UNCHECKED);

#if defined(APP_HAVE_AUTORUN)
					CheckDlgButton (hwnd, IDC_LOADONSTARTUP_CHK, _r_autorun_isenabled () ? BST_CHECKED : BST_UNCHECKED);
#endif // APP_HAVE_AUTORUN

					CheckDlgButton (hwnd, IDC_STARTMINIMIZED_CHK, _r_config_getboolean (L"IsStartMinimized", FALSE) ? BST_CHECKED : BST_UNCHECKED);

#if defined(APP_HAVE_SKIPUAC)
					CheckDlgButton (hwnd, IDC_SKIPUACWARNING_CHK, _r_skipuac_isenabled () ? BST_CHECKED : BST_UNCHECKED);
#endif // APP_HAVE_SKIPUAC

					CheckDlgButton (hwnd, IDC_CHECKUPDATES_CHK, _r_config_getboolean (L"CheckUpdates", TRUE) ? BST_CHECKED : BST_UNCHECKED);

#if defined(_DEBUG) || defined(APP_BETA)
					CheckDlgButton (hwnd, IDC_CHECKUPDATESBETA_CHK, BST_CHECKED);
					_r_ctrl_enable (hwnd, IDC_CHECKUPDATESBETA_CHK, FALSE);
#else
					CheckDlgButton (hwnd, IDC_CHECKUPDATESBETA_CHK, _r_config_getboolean (L"CheckUpdatesBeta", FALSE) ? BST_CHECKED : BST_UNCHECKED);

					if (!_r_config_getboolean (L"CheckUpdates", TRUE))
						_r_ctrl_enable (hwnd, IDC_CHECKUPDATESBETA_CHK, FALSE);
#endif // _DEBUG || APP_BETA

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

					CheckDlgButton (hwnd, (IDC_BLOCKLIST_SPY_DISABLE + _r_calc_clamp (_r_config_getinteger (L"BlocklistSpyState", 2), 0, 2)), BST_CHECKED);
					CheckDlgButton (hwnd, (IDC_BLOCKLIST_UPDATE_DISABLE + _r_calc_clamp (_r_config_getinteger (L"BlocklistUpdateState", 0), 0, 2)), BST_CHECKED);
					CheckDlgButton (hwnd, (IDC_BLOCKLIST_EXTRA_DISABLE + _r_calc_clamp (_r_config_getinteger (L"BlocklistExtraState", 0), 0, 2)), BST_CHECKED);

					break;
				}

				case IDD_SETTINGS_INTERFACE:
				{
					CheckDlgButton (hwnd, IDC_CONFIRMEXIT_CHK, _r_config_getboolean (L"ConfirmExit2", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_CONFIRMEXITTIMER_CHK, _r_config_getboolean (L"ConfirmExitTimer", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_CONFIRMLOGCLEAR_CHK, _r_config_getboolean (L"ConfirmLogClear", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_TRAYICONSINGLECLICK_CHK, _r_config_getboolean (L"IsTrayIconSingleClick", TRUE) ? BST_CHECKED : BST_UNCHECKED);

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

					for (SIZE_T i = 0; i < _r_obj_getarraysize (colors); i++)
					{
						ptr_clr = _r_obj_getarrayitem (colors, i);

						if (!ptr_clr)
							continue;

						ptr_clr->new_clr = _r_config_getulongex (_r_obj_getstring (ptr_clr->config_value), ptr_clr->default_clr, L"colors");

						_r_spinlock_acquireshared (&lock_checkbox);

						_r_listview_additemex (hwnd, IDC_COLORS, item, 0, _r_locale_getstring (ptr_clr->locale_id), config.icon_id, I_GROUPIDNONE, (LPARAM)ptr_clr);
						_r_listview_setitemcheck (hwnd, IDC_COLORS, item, _r_config_getbooleanex (_r_obj_getstring (ptr_clr->config_name), ptr_clr->is_enabled, L"colors"));

						_r_spinlock_releaseshared (&lock_checkbox);

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

					SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_SETRANGE32, 0, (LPARAM)_r_calc_days2seconds (7));
					SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_SETPOS32, 0, (LPARAM)_r_config_getulong (L"NotificationsTimeout", NOTIFY_TIMEOUT_DEFAULT));

					CheckDlgButton (hwnd, IDC_EXCLUDEBLOCKLIST_CHK, _r_config_getboolean (L"IsExcludeBlocklist", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_EXCLUDECUSTOM_CHK, _r_config_getboolean (L"IsExcludeCustomRules", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDC_ENABLENOTIFICATIONS_CHK, 0), WM_APP);

					break;
				}

				case IDD_SETTINGS_LOGGING:
				{
					PR_STRING string = NULL;

					CheckDlgButton (hwnd, IDC_ENABLELOG_CHK, _r_config_getboolean (L"IsLogEnabled", FALSE) ? BST_CHECKED : BST_UNCHECKED);

					string = _app_getlogpath ();

					if (string)
					{
						_r_ctrl_settext (hwnd, IDC_LOGPATH, string->buffer);

						_r_obj_dereference (string);
					}

					string = _app_getlogviewer ();

					if (string)
					{
						_r_ctrl_settext (hwnd, IDC_LOGVIEWER, string->buffer);

						_r_obj_dereference (string);
					}
					UDACCEL ud = {0};
					ud.nInc = 64; // set step to 64kb

					SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_SETACCEL, 1, (LPARAM)&ud);
					SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_SETRANGE32, 64, (LPARAM)_r_calc_kilobytes2bytes (512));
					SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_SETPOS32, 0, (LPARAM)_r_config_getulong (L"LogSizeLimitKb", LOG_SIZE_LIMIT_DEFAULT));

					CheckDlgButton (hwnd, IDC_ENABLEUILOG_CHK, _r_config_getboolean (L"IsLogUiEnabled", FALSE) ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_EXCLUDESTEALTH_CHK, _r_config_getboolean (L"IsExcludeStealth", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, _r_config_getboolean (L"IsExcludeClassifyAllow", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					// win8+
					if (!_r_sys_isosversiongreaterorequal (WINDOWS_8))
						_r_ctrl_enable (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, FALSE);

					PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDC_ENABLELOG_CHK, 0), WM_APP);

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
			_r_ctrl_settextformat (hwnd, IDC_TITLE_TRAY, L"%s:", _r_locale_getstring (IDS_TITLE_TRAY));
			_r_ctrl_settextformat (hwnd, IDC_TITLE_HIGHLIGHTING, L"%s:", _r_locale_getstring (IDS_TITLE_HIGHLIGHTING));
			_r_ctrl_settextformat (hwnd, IDC_TITLE_NOTIFICATIONS, L"%s:", _r_locale_getstring (IDS_TITLE_NOTIFICATIONS));
			_r_ctrl_settextformat (hwnd, IDC_TITLE_LOGGING, L"%s:", _r_locale_getstring (IDS_TITLE_LOGGING));
			_r_ctrl_settextformat (hwnd, IDC_TITLE_LOGVIEWER, L"%s:", _r_locale_getstring (IDS_LOGVIEWER_HINT));
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

					_r_ctrl_settext (hwnd, IDC_TRAYICONSINGLECLICK_CHK, _r_locale_getstring (IDS_TRAYICONSINGLECLICK_CHK));

					break;
				}

				case IDD_SETTINGS_HIGHLIGHTING:
				{
					_r_listview_setcolumn (hwnd, IDC_COLORS, 0, NULL, -100);

					for (INT i = 0; i < _r_listview_getitemcount (hwnd, IDC_COLORS); i++)
					{
						PITEM_COLOR ptr_clr = (PITEM_COLOR)_r_listview_getitemlparam (hwnd, IDC_COLORS, i);

						if (ptr_clr)
						{
							_r_spinlock_acquireshared (&lock_checkbox);
							_r_listview_setitem (hwnd, IDC_COLORS, i, 0, _r_locale_getstring (ptr_clr->locale_id));
							_r_spinlock_releaseshared (&lock_checkbox);
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

		case WM_SIZE:
		{
			RECT rect;
			HWND hlistview;

			hlistview = GetDlgItem (hwnd, IDC_COLORS);

			if (hlistview)
			{
				if (GetClientRect (hlistview, &rect))
				{
					_r_listview_setcolumn (hwnd, IDC_COLORS, 0, NULL, rect.right);
				}
			}

			break;
		}

		case WM_VSCROLL:
		case WM_HSCROLL:
		{
			INT ctrl_id = GetDlgCtrlID ((HWND)lparam);

			if (ctrl_id == IDC_LOGSIZELIMIT)
				_r_config_setulong (L"LogSizeLimitKb", (ULONG)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));

			else if (ctrl_id == IDC_NOTIFICATIONTIMEOUT)
				_r_config_setulong (L"NotificationsTimeout", (ULONG)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));

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

					INT listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					if ((lpnmlv->uChanged & LVIF_STATE) != 0)
					{
						if (listview_id == IDC_COLORS)
						{
							if ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (1) || ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (2)))
							{
								if (_r_spinlock_islocked (&lock_checkbox))
									break;

								BOOLEAN is_enabled = (lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (2);

								PITEM_COLOR ptr_clr = (PITEM_COLOR)lpnmlv->lParam;

								if (ptr_clr)
								{
									_r_config_setbooleanex (_r_obj_getstring (ptr_clr->config_name), is_enabled, L"colors");

									_r_listview_redraw (_r_app_gethwnd (), (INT)_r_tab_getitemlparam (_r_app_gethwnd (), IDC_TAB, -1), -1);
								}
							}
						}
					}

					break;
				}

				case NM_CUSTOMDRAW:
				{
					LONG_PTR result = _app_message_custdraw ((LPNMLVCUSTOMDRAW)lparam);

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, result);
					return result;
				}

				case NM_DBLCLK:
				{
					LPNMITEMACTIVATE lpnmlv = (LPNMITEMACTIVATE)lparam;

					INT listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					if (lpnmlv->iItem == -1)
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

							for (SIZE_T i = 0; i < _r_obj_getarraysize (colors); i++)
							{
								ptr_clr = _r_obj_getarrayitem (colors, i);

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
								_r_config_setulongex (_r_obj_getstring (ptr_clr_current->config_value), cc.rgbResult, L"colors");

								_r_listview_redraw (hwnd, IDC_COLORS, -1);
								_r_listview_redraw (_r_app_gethwnd (), (INT)_r_tab_getitemlparam (_r_app_gethwnd (), IDC_TAB, -1), -1);
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
						_r_shell_opendefault (pnmlink->item.szUrl);

					break;
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

#if defined(APP_HAVE_AUTORUN)
				case IDC_LOADONSTARTUP_CHK:
				{
					_r_autorun_enable (hwnd, (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					CheckDlgButton (hwnd, ctrl_id, _r_autorun_isenabled () ? BST_CHECKED : BST_UNCHECKED);

					break;
				}
#endif // APP_HAVE_AUTORUN

				case IDC_STARTMINIMIZED_CHK:
				{
					_r_config_setboolean (L"IsStartMinimized", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}

#if defined(APP_HAVE_SKIPUAC)
				case IDC_SKIPUACWARNING_CHK:
				{
					_r_skipuac_enable (hwnd, (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					CheckDlgButton (hwnd, ctrl_id, _r_skipuac_isenabled () ? BST_CHECKED : BST_UNCHECKED);

					break;
				}
#endif // APP_HAVE_SKIPUAC

				case IDC_CHECKUPDATES_CHK:
				{
					BOOLEAN is_enabled = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

					_r_config_setboolean (L"CheckUpdates", is_enabled);

#if !defined(_DEBUG) && !defined(APP_BETA)
					_r_ctrl_enable (hwnd, IDC_CHECKUPDATESBETA_CHK, is_enabled);
#endif // !_DEBUG && !APP_BETA

					break;
				}

#if !defined(_DEBUG) && !defined(APP_BETA)
				case IDC_CHECKUPDATESBETA_CHK:
				{
					_r_config_setboolean (L"CheckUpdatesBeta", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					break;
				}
#endif // !_DEBUG && !APP_BETA

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

				case IDC_TRAYICONSINGLECLICK_CHK:
				{
					_r_config_setboolean (L"IsTrayIconSingleClick", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
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
						INT new_state = _r_calc_clamp (_r_ctrl_isradiobuttonchecked (hwnd, IDC_BLOCKLIST_SPY_DISABLE, IDC_BLOCKLIST_SPY_BLOCK) - IDC_BLOCKLIST_SPY_DISABLE, 0, 2);

						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_SPY_DISABLE, IDM_BLOCKLIST_SPY_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_SPY_DISABLE + new_state);

						_r_config_setinteger (L"BlocklistSpyState", new_state);

						_app_ruleblocklistset (_r_app_gethwnd (), new_state, -1, -1, TRUE);
					}
					else if (ctrl_id >= IDC_BLOCKLIST_UPDATE_DISABLE && ctrl_id <= IDC_BLOCKLIST_UPDATE_BLOCK)
					{
						INT new_state = _r_calc_clamp (_r_ctrl_isradiobuttonchecked (hwnd, IDC_BLOCKLIST_UPDATE_DISABLE, IDC_BLOCKLIST_UPDATE_BLOCK) - IDC_BLOCKLIST_UPDATE_DISABLE, 0, 2);

						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_UPDATE_DISABLE, IDM_BLOCKLIST_UPDATE_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_UPDATE_DISABLE + new_state);

						_r_config_setinteger (L"BlocklistUpdateState", new_state);

						_app_ruleblocklistset (_r_app_gethwnd (), -1, new_state, -1, TRUE);
					}
					else if (ctrl_id >= IDC_BLOCKLIST_EXTRA_DISABLE && ctrl_id <= IDC_BLOCKLIST_EXTRA_BLOCK)
					{
						INT new_state = _r_calc_clamp (_r_ctrl_isradiobuttonchecked (hwnd, IDC_BLOCKLIST_EXTRA_DISABLE, IDC_BLOCKLIST_EXTRA_BLOCK) - IDC_BLOCKLIST_EXTRA_DISABLE, 0, 2);

						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_EXTRA_DISABLE, IDM_BLOCKLIST_EXTRA_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_EXTRA_DISABLE + new_state);

						_r_config_setinteger (L"BlocklistExtraState", new_state);

						_app_ruleblocklistset (_r_app_gethwnd (), -1, -1, new_state, TRUE);
					}

					break;
				}

				case IDC_ENABLELOG_CHK:
				{
					BOOLEAN is_postmessage = ((INT)lparam == WM_APP);
					BOOLEAN is_enabled = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);
					BOOLEAN is_logging_enabled = is_enabled || (IsDlgButtonChecked (hwnd, IDC_ENABLEUILOG_CHK) == BST_CHECKED);

					if (!is_postmessage)
					{
						_r_config_setboolean (L"IsLogEnabled", is_enabled);

						_app_loginit (is_enabled);
					}

					SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_PRESSBUTTON, IDM_TRAY_ENABLELOG_CHK, MAKELPARAM (is_enabled, 0));

					_r_ctrl_enable (hwnd, IDC_LOGPATH, is_enabled); // input
					_r_ctrl_enable (hwnd, IDC_LOGPATH_BTN, is_enabled); // button

					EnableWindow ((HWND)SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_GETBUDDY, 0, 0), is_enabled);

					_r_ctrl_enable (hwnd, IDC_EXCLUDESTEALTH_CHK, is_logging_enabled);

					// win8+
					if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
						_r_ctrl_enable (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, is_logging_enabled);

					break;
				}

				case IDC_ENABLEUILOG_CHK:
				{
					BOOLEAN is_enabled = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);
					BOOLEAN is_logging_enabled = is_enabled || (IsDlgButtonChecked (hwnd, IDC_ENABLELOG_CHK) == BST_CHECKED);

					_r_config_setboolean (L"IsLogUiEnabled", is_enabled);
					SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_PRESSBUTTON, IDM_TRAY_ENABLEUILOG_CHK, MAKELPARAM (is_enabled, 0));

					_r_ctrl_enable (hwnd, IDC_EXCLUDESTEALTH_CHK, is_logging_enabled);

					// win8+
					if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
						_r_ctrl_enable (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, is_logging_enabled);

					break;
				}

				case IDC_LOGPATH:
				{
					if (notify_code == EN_KILLFOCUS)
					{
						PR_STRING log_path = _r_ctrl_gettext (hwnd, ctrl_id);

						if (log_path)
						{
							_r_config_setstringexpand (L"LogPath", log_path->buffer);

							if (_r_config_getboolean (L"IsLogEnabled", FALSE))
								_app_loginit (TRUE);

							_r_obj_dereference (log_path);
						}
					}

					break;
				}

				case IDC_LOGPATH_BTN:
				{
					R_FILE_DIALOG file_dialog;
					PR_STRING path;

					COMDLG_FILTERSPEC filters[] = {
						L"Log files (*.log, *.csv)", L"*.log;*.csv",
						L"All files (*.*)", L"*.*",
					};

					if (_r_filedialog_initialize (&file_dialog, PR_FILEDIALOG_SAVEFILE))
					{
						_r_filedialog_setfilter (&file_dialog, filters, RTL_NUMBER_OF (filters));

						path = _r_ctrl_gettext (hwnd, IDC_LOGPATH);

						if (path)
						{
							_r_filedialog_setpath (&file_dialog, path->buffer);
						}

						if (_r_filedialog_show (hwnd, &file_dialog))
						{
							_r_obj_movereference (&path, _r_filedialog_getpath (&file_dialog));

							if (path)
							{
								_r_config_setstringexpand (L"LogPath", path->buffer);
								_r_ctrl_settext (hwnd, IDC_LOGPATH, path->buffer);

								_app_loginit (_r_config_getboolean (L"IsLogEnabled", FALSE));
							}
						}

						if (path)
							_r_obj_dereference (path);

						_r_filedialog_destroy (&file_dialog);
					}

					break;
				}

				case IDC_LOGVIEWER:
				{
					if (notify_code == EN_KILLFOCUS)
					{
						PR_STRING log_viewer = _r_ctrl_gettext (hwnd, ctrl_id);

						if (log_viewer)
						{
							_r_config_setstringexpand (L"LogViewer", log_viewer->buffer);

							_r_obj_dereference (log_viewer);
						}
					}

					break;
				}

				case IDC_LOGVIEWER_BTN:
				{
					R_FILE_DIALOG file_dialog;
					PR_STRING path;

					COMDLG_FILTERSPEC filters[] = {
						L"Executable files (*.exe)", L"*.exe",
						L"All files (*.*)", L"*.*",
					};

					if (_r_filedialog_initialize (&file_dialog, PR_FILEDIALOG_OPENFILE))
					{
						_r_filedialog_setfilter (&file_dialog, filters, RTL_NUMBER_OF (filters));

						path = _r_ctrl_gettext (hwnd, IDC_LOGVIEWER);

						if (path)
						{
							_r_filedialog_setpath (&file_dialog, path->buffer);
						}

						if (_r_filedialog_show (hwnd, &file_dialog))
						{
							_r_obj_movereference (&path, _r_filedialog_getpath (&file_dialog));

							if (path)
							{
								_r_config_setstringexpand (L"LogViewer", path->buffer);
								_r_ctrl_settext (hwnd, IDC_LOGVIEWER, path->buffer);
							}
						}

						if (path)
							_r_obj_dereference (path);

						_r_filedialog_destroy (&file_dialog);
					}

					break;
				}

				case IDC_LOGSIZELIMIT_CTRL:
				{
					if (notify_code == EN_KILLFOCUS)
						_r_config_setulong (L"LogSizeLimitKb", (ULONG)SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_GETPOS32, 0, 0));

					break;
				}

				case IDC_ENABLENOTIFICATIONS_CHK:
				{
					BOOLEAN is_postmessage = ((INT)lparam == WM_APP);
					BOOLEAN is_enabled = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

					if (!is_postmessage)
						_r_config_setboolean (L"IsNotificationsEnabled", is_enabled);

					SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_PRESSBUTTON, IDM_TRAY_ENABLENOTIFICATIONS_CHK, MAKELPARAM (is_enabled, 0));

					_r_ctrl_enable (hwnd, IDC_NOTIFICATIONSOUND_CHK, is_enabled);
					_r_ctrl_enable (hwnd, IDC_NOTIFICATIONONTRAY_CHK, is_enabled);

					HWND hbuddy = (HWND)SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_GETBUDDY, 0, 0);

					if (hbuddy)
						EnableWindow (hbuddy, is_enabled);

					_r_ctrl_enable (hwnd, IDC_EXCLUDEBLOCKLIST_CHK, is_enabled);
					_r_ctrl_enable (hwnd, IDC_EXCLUDECUSTOM_CHK, is_enabled);

					PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDC_NOTIFICATIONSOUND_CHK, 0), WM_APP);

					if (!is_postmessage)
						_app_notifyrefresh (config.hnotification, FALSE);

					break;
				}

				case IDC_NOTIFICATIONSOUND_CHK:
				{
					BOOLEAN is_postmessage = ((INT)lparam == WM_APP);
					BOOLEAN is_checked = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

					CheckDlgButton (hwnd, IDC_NOTIFICATIONFULLSCREENSILENTMODE_CHK, _r_config_getboolean (L"IsNotificationsFullscreenSilentMode", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					if (!is_postmessage)
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
						_r_config_setulong (L"NotificationsTimeout", (ULONG)SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_GETPOS32, 0, 0));

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

FORCEINLINE VOID _app_addmainwindowtab (_In_ HWND hwnd, _In_ INT ctrl_id, _Inout_ PINT tabs_count)
{
	_r_tab_additem (hwnd, IDC_TAB, *tabs_count, L"", I_IMAGENONE, (LPARAM)ctrl_id);

	*tabs_count += 1;
}

VOID _app_tabs_init (_In_ HWND hwnd)
{
	RECT rect = {0};

	PR_STRING localized_string = NULL;
	HINSTANCE hinst = _r_sys_getimagebase ();
	ULONG listview_ex_style = LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_HEADERINALLVIEWS;
	INT tabs_count = 0;

	_app_addmainwindowtab (hwnd, IDC_APPS_PROFILE, &tabs_count);
	_app_addmainwindowtab (hwnd, IDC_APPS_SERVICE, &tabs_count);

	// uwp apps (win8+)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
	{
		_app_addmainwindowtab (hwnd, IDC_APPS_UWP, &tabs_count);
	}

	if (!_r_config_getboolean (L"IsInternalRulesDisabled", FALSE))
	{
		_app_addmainwindowtab (hwnd, IDC_RULES_BLOCKLIST, &tabs_count);
		_app_addmainwindowtab (hwnd, IDC_RULES_SYSTEM, &tabs_count);
	}

	_app_addmainwindowtab (hwnd, IDC_RULES_CUSTOM, &tabs_count);
	_app_addmainwindowtab (hwnd, IDC_NETWORK, &tabs_count);
	_app_addmainwindowtab (hwnd, IDC_LOG, &tabs_count);

	LONG rebar_height = _r_rebar_getheight (hwnd, IDC_REBAR);
	LONG statusbar_height = _r_status_getheight (hwnd, IDC_STATUSBAR);

	HWND hlistview;
	INT listview_id;

	GetClientRect (hwnd, &rect);

	SetWindowPos (config.hrebar, NULL, 0, 0, rect.right, rebar_height, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
	SetWindowPos (GetDlgItem (hwnd, IDC_TAB), NULL, 0, rebar_height, rect.right, rect.bottom - rebar_height - statusbar_height, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

	for (INT i = 0; i < tabs_count; i++)
	{
		if (!(listview_id = (INT)_r_tab_getitemlparam (hwnd, IDC_TAB, i)) || !(hlistview = GetDlgItem (hwnd, listview_id)))
			continue;

		_r_tab_adjustchild (hwnd, IDC_TAB, hlistview);

		_app_listviewsetfont (hwnd, listview_id, FALSE);

		if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_RULES_CUSTOM)
		{
			_r_listview_setstyle (hwnd, listview_id, listview_ex_style | LVS_EX_CHECKBOXES, TRUE); // with checkboxes

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
		else if (listview_id == IDC_NETWORK)
		{
			_r_listview_setstyle (hwnd, listview_id, listview_ex_style, TRUE);

			_r_listview_addcolumn (hwnd, listview_id, 0, _r_locale_getstring (IDS_NAME), 0, LVCFMT_LEFT);

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_LOCAL L")", _r_locale_getstring (IDS_ADDRESS)));
			_r_listview_addcolumn (hwnd, listview_id, 1, _r_obj_getstringorempty (localized_string), 0, LVCFMT_LEFT);

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_LOCAL L")", _r_locale_getstring (IDS_PORT)));
			_r_listview_addcolumn (hwnd, listview_id, 2, _r_obj_getstringorempty (localized_string), 0, LVCFMT_RIGHT);

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_REMOTE L")", _r_locale_getstring (IDS_ADDRESS)));
			_r_listview_addcolumn (hwnd, listview_id, 3, _r_obj_getstringorempty (localized_string), 0, LVCFMT_LEFT);

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_REMOTE L")", _r_locale_getstring (IDS_PORT)));
			_r_listview_addcolumn (hwnd, listview_id, 4, _r_obj_getstringorempty (localized_string), 0, LVCFMT_RIGHT);

			_r_listview_addcolumn (hwnd, listview_id, 5, _r_locale_getstring (IDS_PROTOCOL), 0, LVCFMT_RIGHT);

			_r_listview_addcolumn (hwnd, listview_id, 6, _r_locale_getstring (IDS_STATE), 0, LVCFMT_RIGHT);

			_r_listview_addgroup (hwnd, listview_id, 0, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
			_r_listview_addgroup (hwnd, listview_id, 1, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
			_r_listview_addgroup (hwnd, listview_id, 2, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
		}
		else if (listview_id == IDC_LOG)
		{
			_r_listview_setstyle (hwnd, listview_id, listview_ex_style, FALSE);

			_r_listview_addcolumn (hwnd, listview_id, 0, _r_locale_getstring (IDS_NAME), 0, LVCFMT_LEFT);

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_LOCAL L")", _r_locale_getstring (IDS_ADDRESS)));
			_r_listview_addcolumn (hwnd, listview_id, 1, _r_obj_getstringorempty (localized_string), 0, LVCFMT_LEFT);

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_LOCAL L")", _r_locale_getstring (IDS_PORT)));
			_r_listview_addcolumn (hwnd, listview_id, 2, _r_obj_getstringorempty (localized_string), 0, LVCFMT_RIGHT);

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_REMOTE L")", _r_locale_getstring (IDS_ADDRESS)));
			_r_listview_addcolumn (hwnd, listview_id, 3, _r_obj_getstringorempty (localized_string), 0, LVCFMT_LEFT);

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_REMOTE L")", _r_locale_getstring (IDS_PORT)));
			_r_listview_addcolumn (hwnd, listview_id, 4, _r_obj_getstringorempty (localized_string), 0, LVCFMT_RIGHT);

			_r_listview_addcolumn (hwnd, listview_id, 5, _r_locale_getstring (IDS_PROTOCOL), 0, LVCFMT_RIGHT);

			_r_listview_addcolumn (hwnd, listview_id, 6, _r_locale_getstring (IDS_FILTER), 0, LVCFMT_LEFT);
			_r_listview_addcolumn (hwnd, listview_id, 7, _r_locale_getstring (IDS_DIRECTION), 0, LVCFMT_RIGHT);
			_r_listview_addcolumn (hwnd, listview_id, 8, _r_locale_getstring (IDS_STATE), 0, LVCFMT_RIGHT);
			_r_listview_addcolumn (hwnd, listview_id, 9, L"#", 0, LVCFMT_RIGHT);
		}
	}

	if (localized_string)
		_r_obj_dereference (localized_string);
}

VOID _app_initialize ()
{
	// initialize spinlocks
	_r_spinlock_initialize (&lock_apps);
	_r_spinlock_initialize (&lock_rules);
	_r_spinlock_initialize (&lock_rules_config);
	_r_spinlock_initialize (&lock_apply);
	_r_spinlock_initialize (&lock_checkbox);
	_r_spinlock_initialize (&lock_logbusy);
	_r_spinlock_initialize (&lock_loglist);
	_r_spinlock_initialize (&lock_logthread);
	_r_spinlock_initialize (&lock_network);
	_r_spinlock_initialize (&lock_profile);
	_r_spinlock_initialize (&lock_transaction);

	_r_spinlock_initialize (&lock_cache_dns);
	_r_spinlock_initialize (&lock_cache_hosts);
	_r_spinlock_initialize (&lock_cache_signatures);
	_r_spinlock_initialize (&lock_cache_types);
	_r_spinlock_initialize (&lock_cache_versions);

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

	// register message
	if (!WM_FINDMSGSTRING)
		WM_FINDMSGSTRING = RegisterWindowMessage (FINDMSGSTRING);

	// set process priority
	SetPriorityClass (NtCurrentProcess (), ABOVE_NORMAL_PRIORITY_CLASS);

	// static initializer
	config.wd_length = GetWindowsDirectory (config.windows_dir, RTL_NUMBER_OF (config.windows_dir));

	_r_str_printf (config.profile_path, RTL_NUMBER_OF (config.profile_path), L"%s\\" XML_PROFILE, _r_app_getprofiledirectory ());
	_r_str_printf (config.profile_path_backup, RTL_NUMBER_OF (config.profile_path_backup), L"%s\\" XML_PROFILE L".bak", _r_app_getprofiledirectory ());
	_r_str_printf (config.profile_internal_path, RTL_NUMBER_OF (config.profile_internal_path), L"%s\\" XML_PROFILE_INTERNAL, _r_app_getprofiledirectory ());

	config.svchost_path = _r_str_expandenvironmentstring (PATH_SVCHOST);
	config.ntoskrnl_path = _r_str_expandenvironmentstring (PATH_NTOSKRNL);

	if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
		config.winstore_path = _r_str_expandenvironmentstring (PATH_WINSTORE);

	config.my_hash = _r_str_hash (_r_sys_getimagepathname ());
	config.ntoskrnl_hash = _r_str_hash (PROC_SYSTEM_NAME);
	config.svchost_hash = _r_obj_getstringhash (config.svchost_path);

	// initialize timers
	if (!timers)
	{
		LONG64 timer_array[] =
		{
			_r_calc_minutes2seconds (2),
			_r_calc_minutes2seconds (5),
			_r_calc_minutes2seconds (10),
			_r_calc_minutes2seconds (30),
			_r_calc_hours2seconds (1),
			_r_calc_hours2seconds (2),
			_r_calc_hours2seconds (4),
			_r_calc_hours2seconds (6)
		};

		timers = _r_obj_createarrayex (sizeof (LONG64), RTL_NUMBER_OF (timer_array) + 1, NULL);

		for (SIZE_T i = 0; i < RTL_NUMBER_OF (timer_array); i++)
			_r_obj_addarrayitem (timers, &timer_array[i]);
	}

	// initialize colors array
	if (!colors)
		colors = _r_obj_createarrayex (sizeof (ITEM_COLOR), 16, NULL);

	// initialize thread objects
	if (!config.done_evt)
		config.done_evt = CreateEventEx (NULL, NULL, 0, EVENT_ALL_ACCESS);

	_app_generate_credentials ();

	// initialize global filters array object
	if (!filter_ids)
		filter_ids = _r_obj_createarrayex (sizeof (GUID), 16, NULL);

	// initialize apps table
	if (!apps)
		apps = _r_obj_createhashtableex (sizeof (ITEM_APP), 16, &_app_dereferenceapp);

	// initialize rules array object
	if (!rules_arr)
		rules_arr = _r_obj_createarrayex (sizeof (ITEM_RULE), 16, &_app_dereferencerule);

	// initialize rules configuration table
	if (!rules_config)
		rules_config = _r_obj_createhashtableex (sizeof (ITEM_RULE_CONFIG), 16, &_app_dereferenceruleconfig);

	// initialize log list object
	if (!log_arr)
		log_arr = _r_obj_createlistex (1024, &_r_obj_dereference);

	// initialize network table
	if (!network_table)
		network_table = _r_obj_createhashtableex (sizeof (ITEM_NETWORK), 16, &_app_dereferencenetwork);

	// initialize hasher table
	PR_HASHTABLE* cache_array[] = {
		&cache_dns,
		&cache_hosts,
		&cache_signatures,
		&cache_types,
		&cache_versions
	};

	for (SIZE_T i = 0; i < RTL_NUMBER_OF (cache_array); i++)
		(*cache_array[i]) = _r_obj_createhashtableex (sizeof (R_HASHSTORE), MAP_CACHE_MAX, &_r_util_dereferencehashstoreprocedure);
}

INT FirstDriveFromMask (_In_ ULONG unitmask)
{
	INT i;

	for (i = 0; i < PR_DEVICE_COUNT; ++i)
	{
		if (unitmask & 0x1)
			break;

		unitmask = unitmask >> 1;
	}

	return i;
}

INT_PTR CALLBACK DlgProc (_In_ HWND hwnd, _In_ UINT msg, _In_ WPARAM wparam, _In_ LPARAM lparam)
{
	static R_LAYOUT_MANAGER layout_manager = {0};

	if (msg == WM_FINDMSGSTRING)
	{
		_app_message_find (hwnd, (LPFINDREPLACE)lparam);

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

			// initialize imagelist
			_app_imagelist_init (hwnd);

			// initialize toolbar
			_app_toolbar_init (hwnd);
			_app_toolbar_resize ();

			// initialize tabs
			_app_tabs_init (hwnd);

			// load profile
			_app_profile_load (hwnd, NULL);

			// add blocklist to update
			if (!_r_config_getboolean (L"IsInternalRulesDisabled", FALSE))
			{
				WCHAR internal_profile_version[64];
				_r_str_fromlong64 (internal_profile_version, RTL_NUMBER_OF (internal_profile_version), config.profile_internal_timestamp);

				_r_update_addcomponent (L"Internal rules", L"profile_internal", internal_profile_version, config.profile_internal_path, FALSE);
			}

			// initialize tab
			_app_settab_id (hwnd, _r_config_getinteger (L"CurrentTab", IDC_APPS_PROFILE));

			// initialize dropped packets log callback thread (win7+)
			RtlInitializeSListHead (&log_list_stack);

			// create notification window
			_app_notifycreatewindow ();

			// create network monitor thread
			_r_sys_createthreadex (&NetworkMonitorThread, hwnd, NULL, THREAD_PRIORITY_LOWEST);

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
				for (INT i = 0; i < _r_tab_getitemcount (hwnd, IDC_TAB); i++)
				{
					INT listview_id = (INT)_r_tab_getitemlparam (hwnd, IDC_TAB, i);

					if (listview_id)
						_app_listviewresize (hwnd, listview_id, TRUE);
				}
			}

			// initialize layout manager
			_r_layout_initializemanager (&layout_manager, hwnd);

			layout_manager.original_size.x = 300;
			layout_manager.original_size.y = 300;

			break;
		}

		case WM_NCCREATE:
		{
			_r_wnd_enablenonclientscaling (hwnd);
			break;
		}

		case RM_INITIALIZE:
		{
			_app_message_initialize (hwnd);
			break;
		}

		case RM_UNINITIALIZE:
		{
			_app_message_uninitialize (hwnd);
			break;
		}

		case RM_LOCALIZE:
		{
			_app_message_localize (hwnd);
			break;
		}

		case RM_TASKBARCREATED:
		{
			// refresh tray icon
			_r_tray_destroy (hwnd, UID);
			_r_tray_create (hwnd, UID, WM_TRAYICON, _r_app_getsharedimage (_r_sys_getimagebase (), (_wfp_isfiltersinstalled () != InstallDisabled) ? IDI_ACTIVE : IDI_INACTIVE, _r_dc_getsystemmetrics (hwnd, SM_CXSMICON)), APP_NAME, FALSE);

			break;
		}

		case RM_CONFIG_UPDATE:
		{
			_app_profile_save ();
			_app_profile_load (hwnd, NULL);

			_app_refreshstatus (hwnd, -1);

			_app_changefilters (hwnd, TRUE, FALSE);

			break;
		}

		case RM_CONFIG_RESET:
		{
			LONG64 current_timestamp = (LONG64)lparam;

			_r_spinlock_acquireexclusive (&lock_rules_config);

			_r_obj_clearhashtable (rules_config);

			_r_spinlock_releaseexclusive (&lock_rules_config);

			_r_fs_makebackup (config.profile_path, current_timestamp, TRUE);

			_app_profile_load (hwnd, NULL);

			_app_refreshstatus (hwnd, -1);

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
			_r_config_setinteger (L"CurrentTab", (INT)_r_tab_getitemlparam (hwnd, IDC_TAB, -1));

			if (config.hnotification)
				DestroyWindow (config.hnotification);

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
			PITEM_APP ptr_app = NULL;
			UINT numfiles;
			UINT length;

			numfiles = DragQueryFile ((HDROP)wparam, UINT32_MAX, NULL, 0);

			for (UINT i = 0; i < numfiles; i++)
			{
				length = DragQueryFile ((HDROP)wparam, i, NULL, 0);
				string = _r_obj_createstringex (NULL, length * sizeof (WCHAR));

				if (DragQueryFile ((HDROP)wparam, i, string->buffer, length + 1))
					ptr_app = _app_addapplication (hwnd, DataUnknown, string->buffer, NULL, NULL);

				_r_obj_dereference (string);
			}

			DragFinish ((HDROP)wparam);

			_app_profile_save ();
			_app_refreshstatus (hwnd, -1);

			if (ptr_app)
				_app_showappitem (hwnd, ptr_app);

			break;
		}

		case WM_DPICHANGED:
		{
			_app_message_dpichanged (hwnd);
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
					INT listview_id = (INT)_r_tab_getitemlparam (hwnd, IDC_TAB, -1);

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
					UINT swp_flags;
					INT listview_id;

					listview_id = (INT)_r_tab_getitemlparam (hwnd, IDC_TAB, -1);

					if (!listview_id)
						break;

					_app_listviewsetview (hwnd, listview_id);
					_app_listviewsetfont (hwnd, listview_id, FALSE);

					_app_listviewsort (hwnd, listview_id, -1, FALSE);

					_app_refreshstatus (hwnd, listview_id);

					_r_listview_redraw (hwnd, listview_id, -1);

					swp_flags = SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOOWNERZORDER;

					if (!IsWindowVisible (hwnd) || IsIconic (hwnd)) // HACK!!!
						swp_flags |= SWP_NOACTIVATE;

					SetWindowPos (GetDlgItem (hwnd, listview_id), NULL, 0, 0, 0, 0, swp_flags);

					_app_listviewresize (hwnd, listview_id, FALSE);

					break;
				}

				case NM_CUSTOMDRAW:
				{
					LONG_PTR result = _app_message_custdraw ((LPNMLVCUSTOMDRAW)lparam);

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, result);
					return result;
				}

				case LVN_COLUMNCLICK:
				{
					LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lparam;

					INT ctrl_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					_app_listviewsort (hwnd, ctrl_id, lpnmlv->iSubItem, TRUE);

					break;
				}

				case LVN_GETINFOTIP:
				{
					LPNMLVGETINFOTIP lpnmlv = (LPNMLVGETINFOTIP)lparam;
					PR_STRING string = _app_gettooltip (hwnd, (INT)lpnmlv->hdr.idFrom, lpnmlv->iItem);

					if (string)
					{
						_r_str_copy (lpnmlv->pszText, lpnmlv->cchTextMax, string->buffer);

						_r_obj_dereference (string);
					}

					break;
				}

				case LVN_ITEMCHANGING:
				{
					LPNMLISTVIEW lpnmlv;
					INT listview_id;

					if (_r_spinlock_islocked (&lock_checkbox))
						break;

					lpnmlv = (LPNMLISTVIEW)lparam;
					listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					if (listview_id == IDC_APPS_PROFILE)

						if ((lpnmlv->uChanged & LVIF_STATE) != 0)
						{
							SIZE_T app_hash = lpnmlv->lParam;

							if ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (1))
							{
								if (app_hash == config.my_hash)
								{
									if (!_r_show_confirmmessage (hwnd, L"Warning!", L"Do not disallow simplewall executable.", NULL))
									{
										SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
										return TRUE;
									}
								}
							}
							else if ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (2))
							{
								if (app_hash == config.svchost_hash)
								{
									if (!_r_show_confirmmessage (hwnd, L"Warning!", L"Through service host (svchost.exe) internet traffic can pass away in a thousand of different ways. Be careful by allowing this, you deserve all you gonna do.", NULL))
									{
										SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
										return TRUE;
									}
								}
							}
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
							if (_r_spinlock_islocked (&lock_checkbox))
								break;

							INT listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;
							BOOLEAN is_changed = FALSE;

							BOOLEAN is_enabled = (lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (2);

							if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
							{
								ULONG_PTR app_hash = lpnmlv->lParam;
								PITEM_APP ptr_app = _app_getappitem (app_hash);

								if (!ptr_app)
									break;

								if (ptr_app->is_enabled != is_enabled)
								{
									ptr_app->is_enabled = is_enabled;

									_r_spinlock_acquireshared (&lock_checkbox);
									_app_setappiteminfo (hwnd, listview_id, lpnmlv->iItem, ptr_app);
									_r_spinlock_releaseshared (&lock_checkbox);

									if (is_enabled)
										_app_freenotify (ptr_app);

									if (!is_enabled && _app_istimerset (ptr_app->htimer))
										_app_timer_reset (hwnd, ptr_app);

									if (_wfp_isfiltersinstalled ())
									{
										HANDLE hengine = _wfp_getenginehandle ();

										if (hengine)
										{
											PR_LIST rules = _r_obj_createlist (NULL);

											_r_obj_addlistitem (rules, ptr_app);

											_wfp_create3filters (hengine, rules, __LINE__, FALSE);

											_r_obj_dereference (rules);
										}
									}

									is_changed = TRUE;
								}
							}
							else if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM)
							{
								SIZE_T rule_idx = lpnmlv->lParam;
								PITEM_RULE ptr_rule = _app_getrulebyid (rule_idx);

								if (!ptr_rule)
									break;

								if (ptr_rule->is_enabled != is_enabled)
								{
									_r_spinlock_acquireshared (&lock_checkbox);

									_app_ruleenable (ptr_rule, is_enabled, TRUE);
									_app_setruleiteminfo (hwnd, listview_id, lpnmlv->iItem, ptr_rule, TRUE);

									_r_spinlock_releaseshared (&lock_checkbox);

									if (_wfp_isfiltersinstalled ())
									{
										HANDLE hengine = _wfp_getenginehandle ();

										if (hengine)
										{
											PR_LIST rules = _r_obj_createlist (NULL);

											_r_obj_addlistitem (rules, ptr_rule);

											_wfp_create4filters (hengine, rules, __LINE__, FALSE);

											_r_obj_dereference (rules);
										}
									}

									is_changed = TRUE;
								}
							}

							if (is_changed)
							{
								_app_listviewsort (hwnd, listview_id, -1, FALSE);
								_app_refreshstatus (hwnd, listview_id);

								_app_profile_save ();
							}
						}
					}

					break;
				}

				case NM_DBLCLK:
				{
					LPNMITEMACTIVATE lpnmlv = (LPNMITEMACTIVATE)lparam;

					if (lpnmlv->iItem == -1)
						break;

					INT command_id = 0;
					INT ctrl_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					if (ctrl_id >= IDC_APPS_PROFILE && ctrl_id <= IDC_LOG)
					{
						command_id = IDM_PROPERTIES;
					}
					else if (ctrl_id == IDC_STATUSBAR)
					{
						LPNMMOUSE lpmouse = (LPNMMOUSE)lparam;

						if (lpmouse->dwItemSpec == 0)
						{
							command_id = IDM_SELECT_ALL;
						}
						else if (lpmouse->dwItemSpec == 1)
						{
							command_id = IDM_PURGE_UNUSED;
						}
						else if (lpmouse->dwItemSpec == 2)
						{
							command_id = IDM_PURGE_TIMERS;
						}
					}

					if (command_id)
						PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (command_id, 0), 0);

					break;
				}

				case NM_RCLICK:
				{
					_app_message_contextmenu (hwnd, (LPNMITEMACTIVATE)lparam);
					break;
				}
			}

			break;
		}

		case WM_SIZE:
		{
			INT listview_id;

			if (!_r_layout_resize (&layout_manager, wparam))
				break;

			_r_toolbar_resize (config.hrebar, IDC_TOOLBAR);

			listview_id = (INT)_r_tab_getitemlparam (hwnd, IDC_TAB, -1);

			if (listview_id)
				_app_listviewresize (hwnd, listview_id, FALSE);

			_app_refreshstatus (hwnd, 0);

			break;
		}

		case WM_GETMINMAXINFO:
		{
			_r_layout_resizeminimumsize (&layout_manager, lparam);
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

				case NIN_KEYSELECT:
				{
					if (GetForegroundWindow () != hwnd)
					{
						_r_wnd_toggle (hwnd, FALSE);
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
					if (_r_config_getboolean (L"IsTrayIconSingleClick", TRUE))
					{
						_r_wnd_toggle (hwnd, FALSE);
					}
					else
					{
						SetForegroundWindow (hwnd);
					}

					break;
				}

				case WM_LBUTTONDBLCLK:
				{
					if (!_r_config_getboolean (L"IsTrayIconSingleClick", TRUE))
					{
						_r_wnd_toggle (hwnd, FALSE);
					}

					break;
				}

				case WM_CONTEXTMENU:
				{
					_app_message_traycontextmenu (hwnd);
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
									_r_listview_redraw (hwnd, (INT)_r_tab_getitemlparam (hwnd, IDC_TAB, -1), -1);
							}
						}
						else if (wparam == DBT_DEVICEREMOVECOMPLETE)
						{
							if (IsWindowVisible (hwnd))
								_r_listview_redraw (hwnd, (INT)_r_tab_getitemlparam (hwnd, IDC_TAB, -1), -1);
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
			INT notify_code = HIWORD (wparam);

			if (!notify_code && ctrl_id >= IDX_LANGUAGE && ctrl_id <= (IDX_LANGUAGE + (INT)(INT_PTR)_r_locale_getcount ()))
			{
				_r_locale_applyfrommenu (GetSubMenu (GetSubMenu (GetMenu (hwnd), 2), LANG_MENU), ctrl_id);
				return FALSE;
			}
			else if (!notify_code && ctrl_id >= IDX_RULES_SPECIAL && ctrl_id <= (IDX_RULES_SPECIAL + (INT)(INT_PTR)_r_obj_getarraysize (rules_arr)))
			{
				_app_command_idtorules (hwnd, ctrl_id);
				return FALSE;
			}
			else if (!notify_code && ctrl_id >= IDX_TIMER && ctrl_id <= (IDX_TIMER + (INT)(INT_PTR)_r_obj_getarraysize (timers)))
			{
				_app_command_idtotimers (hwnd, ctrl_id);
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
					_r_settings_createwindow (hwnd, &SettingsProc, 0);
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
					_r_shell_opendefault (_r_app_getwebsite_url ());
					break;
				}

				case IDM_CHECKUPDATES:
				{
					_r_update_check (hwnd);
					break;
				}

				case IDM_DONATE:
				{
					_r_shell_opendefault (_r_app_getdonate_url ());
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
					R_FILE_DIALOG file_dialog;
					PR_STRING path;

					COMDLG_FILTERSPEC filters[] = {
						L"Xml files (*.xml)", L"*.xml;*.xml.bak",
						L"All files (*.*)", L"*.*",
					};

					if (_r_filedialog_initialize (&file_dialog, PR_FILEDIALOG_OPENFILE))
					{
						_r_filedialog_setfilter (&file_dialog, filters, RTL_NUMBER_OF (filters));
						_r_filedialog_setpath (&file_dialog, XML_PROFILE);

						if (_r_filedialog_show (hwnd, &file_dialog))
						{
							path = _r_filedialog_getpath (&file_dialog);

							if (path)
							{
								if (!_app_profile_load_check (path->buffer, XmlProfileV3))
								{
									_r_show_errormessage (hwnd, L"Import failure!", ERROR_INVALID_DATA, path->buffer, NULL);
								}
								else
								{
									// made backup
									_r_fs_deletefile (config.profile_path_backup, TRUE);
									_app_profile_save ();

									_app_profile_load (hwnd, path->buffer); // load profile

									_app_refreshstatus (hwnd, -1);

									_app_changefilters (hwnd, TRUE, FALSE);
								}

								_r_obj_dereference (path);
							}
						}

						_r_filedialog_destroy (&file_dialog);
					}

					break;
				}

				case IDM_EXPORT:
				{
					R_FILE_DIALOG file_dialog;
					PR_STRING path;

					COMDLG_FILTERSPEC filters[] = {
						L"Xml files (*.xml)", L"*.xml;*.xml.bak",
						L"All files (*.*)", L"*.*",
					};

					if (_r_filedialog_initialize (&file_dialog, PR_FILEDIALOG_SAVEFILE))
					{
						_r_filedialog_setfilter (&file_dialog, filters, RTL_NUMBER_OF (filters));
						_r_filedialog_setpath (&file_dialog, XML_PROFILE);

						if (_r_filedialog_show (hwnd, &file_dialog))
						{
							path = _r_filedialog_getpath (&file_dialog);

							if (path)
							{
								_app_profile_save ();

								// added information for export profile failure (issue #707)
								if (!_r_fs_copyfile (config.profile_path, path->buffer, 0))
									_r_show_errormessage (hwnd, L"Export failure!", GetLastError (), path->buffer, NULL);

								_r_obj_dereference (path);
							}
						}

						_r_filedialog_destroy (&file_dialog);
					}

					break;
				}

				case IDM_ALWAYSONTOP_CHK:
				{
					BOOLEAN new_val = !_r_config_getboolean (L"AlwaysOnTop", APP_ALWAYSONTOP);

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
						_app_listviewresize (hwnd, (INT)_r_tab_getitemlparam (hwnd, IDC_TAB, -1), FALSE);

					break;
				}

				case IDM_SHOWFILENAMESONLY_CHK:
				{
					PITEM_APP ptr_app;
					SIZE_T enum_key = 0;
					BOOLEAN new_val = !_r_config_getboolean (L"ShowFilenames", TRUE);

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
					_r_config_setboolean (L"ShowFilenames", new_val);

					// regroup apps
					_r_spinlock_acquireshared (&lock_apps);

					while (_r_obj_enumhashtable (apps, &ptr_app, NULL, &enum_key))
					{
						if (_r_obj_isstringempty (ptr_app->short_name))
							continue;

						INT listview_id = _app_getlistview_id (ptr_app->type);

						if (listview_id)
						{
							INT item_pos = _app_getposition (hwnd, listview_id, ptr_app->app_hash);

							if (item_pos != -1)
							{
								_r_spinlock_acquireshared (&lock_checkbox);
								_app_setappiteminfo (hwnd, listview_id, item_pos, ptr_app);
								_r_spinlock_releaseshared (&lock_checkbox);
							}
						}
					}

					_r_spinlock_releaseshared (&lock_apps);

					_app_listviewsort (hwnd, (INT)_r_tab_getitemlparam (hwnd, IDC_TAB, -1), -1, FALSE);

					break;
				}

				case IDM_VIEW_DETAILS:
				case IDM_VIEW_ICON:
				case IDM_VIEW_TILE:
				{
					INT view_type;

					if (ctrl_id == IDM_VIEW_ICON)
					{
						view_type = LV_VIEW_ICON;
					}
					else if (ctrl_id == IDM_VIEW_TILE)
					{
						view_type = LV_VIEW_TILE;
					}
					else
					{
						view_type = LV_VIEW_DETAILS;
					}

					_r_menu_checkitem (GetMenu (hwnd), IDM_VIEW_DETAILS, IDM_VIEW_TILE, MF_BYCOMMAND, ctrl_id);
					_r_config_setinteger (L"ViewType", view_type);

					INT listview_id = (INT)_r_tab_getitemlparam (hwnd, IDC_TAB, -1);

					if (listview_id)
					{
						_app_listviewsetview (hwnd, listview_id);
						_app_listviewresize (hwnd, listview_id, FALSE);

						_r_listview_redraw (hwnd, listview_id, -1);
					}

					break;
				}

				case IDM_SIZE_SMALL:
				case IDM_SIZE_LARGE:
				case IDM_SIZE_EXTRALARGE:
				{
					INT icon_size;

					if (ctrl_id == IDM_SIZE_LARGE)
					{
						icon_size = SHIL_LARGE;
					}
					else if (ctrl_id == IDM_SIZE_EXTRALARGE)
					{
						icon_size = SHIL_EXTRALARGE;
					}
					else
					{
						icon_size = SHIL_SYSSMALL;
					}

					_r_menu_checkitem (GetMenu (hwnd), IDM_SIZE_SMALL, IDM_SIZE_EXTRALARGE, MF_BYCOMMAND, ctrl_id);
					_r_config_setinteger (L"IconSize", icon_size);

					INT listview_id = (INT)_r_tab_getitemlparam (hwnd, IDC_TAB, -1);

					if (listview_id)
					{
						_app_listviewsetview (hwnd, listview_id);
						_app_listviewresize (hwnd, listview_id, FALSE);

						_r_listview_redraw (hwnd, listview_id, -1);
					}

					break;
				}

				case IDM_ICONSISHIDDEN:
				{
					PITEM_APP ptr_app;
					SIZE_T enum_key = 0;
					BOOLEAN new_val = !_r_config_getboolean (L"IsIconsHidden", FALSE);

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
					_r_config_setboolean (L"IsIconsHidden", new_val);

					_r_spinlock_acquireshared (&lock_apps);

					while (_r_obj_enumhashtable (apps, &ptr_app, NULL, &enum_key))
					{
						INT listview_id = _app_getlistview_id (ptr_app->type);

						if (listview_id)
						{
							INT item_pos = _app_getposition (hwnd, listview_id, ptr_app->app_hash);

							if (item_pos != -1)
							{
								_r_spinlock_acquireshared (&lock_checkbox);
								_app_setappiteminfo (hwnd, listview_id, item_pos, ptr_app);
								_r_spinlock_releaseshared (&lock_checkbox);
							}
						}
					}

					_r_spinlock_releaseshared (&lock_apps);

					break;
				}

				case IDM_FONT:
				{
					_app_command_selectfont (hwnd);
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

					_app_profile_load (hwnd, NULL);
					_app_refreshstatus (hwnd, -1);

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

					if (!hmenu)
						break;

					if (ctrl_id >= IDM_BLOCKLIST_SPY_DISABLE && ctrl_id <= IDM_BLOCKLIST_SPY_BLOCK)
					{
						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_SPY_DISABLE, IDM_BLOCKLIST_SPY_BLOCK, MF_BYCOMMAND, ctrl_id);

						INT new_state = _r_calc_clamp (ctrl_id - IDM_BLOCKLIST_SPY_DISABLE, 0, 2);

						_r_config_setinteger (L"BlocklistSpyState", new_state);

						_app_ruleblocklistset (hwnd, new_state, -1, -1, TRUE);
					}
					else if (ctrl_id >= IDM_BLOCKLIST_UPDATE_DISABLE && ctrl_id <= IDM_BLOCKLIST_UPDATE_BLOCK)
					{
						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_UPDATE_DISABLE, IDM_BLOCKLIST_UPDATE_BLOCK, MF_BYCOMMAND, ctrl_id);

						INT new_state = _r_calc_clamp (ctrl_id - IDM_BLOCKLIST_UPDATE_DISABLE, 0, 2);

						_r_config_setinteger (L"BlocklistUpdateState", new_state);

						_app_ruleblocklistset (hwnd, -1, new_state, -1, TRUE);
					}
					else if (ctrl_id >= IDM_BLOCKLIST_EXTRA_DISABLE && ctrl_id <= IDM_BLOCKLIST_EXTRA_BLOCK)
					{
						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_EXTRA_DISABLE, IDM_BLOCKLIST_EXTRA_BLOCK, MF_BYCOMMAND, ctrl_id);

						INT new_state = _r_calc_clamp (ctrl_id - IDM_BLOCKLIST_EXTRA_DISABLE, 0, 2);

						_r_config_setinteger (L"BlocklistExtraState", new_state);

						_app_ruleblocklistset (hwnd, -1, -1, new_state, TRUE);
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
					_app_command_logshow (hwnd);
					break;
				}

				case IDM_TRAY_LOGCLEAR:
				{
					_app_command_logclear (hwnd);
					break;
				}

				case IDM_TRAY_LOGSHOW_ERR:
				{
					_app_command_logerrshow (hwnd);
					break;
				}

				case IDM_TRAY_LOGCLEAR_ERR:
				{
					_app_command_logerrclear (hwnd);
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
					R_FILE_DIALOG file_dialog;
					PR_STRING path;

					COMDLG_FILTERSPEC filters[] = {
						L"Executable files (*.exe)", L"*.exe",
						L"All files (*.*)", L"*.*",
					};

					if (_r_filedialog_initialize (&file_dialog, PR_FILEDIALOG_OPENFILE))
					{
						_r_filedialog_setfilter (&file_dialog, filters, RTL_NUMBER_OF (filters));

						if (_r_filedialog_show (hwnd, &file_dialog))
						{
							path = _r_filedialog_getpath (&file_dialog);

							if (path)
							{
								PITEM_APP ptr_app = _app_addapplication (hwnd, DataUnknown, path->buffer, NULL, NULL);

								if (ptr_app)
								{
									_app_listviewsort (hwnd, _app_getlistview_id (ptr_app->type), -1, FALSE);

									_app_showappitem (hwnd, ptr_app);
								}

								_app_refreshstatus (hwnd, -1);

								_app_profile_save ();
							}
						}

						_r_filedialog_destroy (&file_dialog);
					}

					break;
				}

				case IDM_DISABLENOTIFICATIONS:
				case IDM_DISABLETIMER:
				{
					_app_command_disable (hwnd, ctrl_id);
					break;
				}

				case IDM_COPY:
				case IDM_COPY2:
				{
					_app_command_copy (hwnd, ctrl_id, (INT)lparam);
					break;
				}

				case IDM_EXPLORE:
				{
					INT listview_id = (INT)_r_tab_getitemlparam (hwnd, IDC_TAB, -1);
					INT item = -1;

					if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
					{
						while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != -1)
						{
							ULONG_PTR app_hash = _r_listview_getitemlparam (hwnd, listview_id, item);
							PITEM_APP ptr_app = _app_getappitem (app_hash);

							if (ptr_app)
								_app_openappdirectory (ptr_app);
						}
					}

					break;
				}

				case IDM_OPENRULESEDITOR:
				{
					_app_command_openeditor (hwnd);
					break;
				}

				case IDM_CHECK:
				case IDM_UNCHECK:
				{
					_app_command_checkbox (hwnd, ctrl_id);
					break;
				}

				case IDM_DELETE:
				{
					_app_command_delete (hwnd);
					break;
				}

				case IDM_PROPERTIES:
				{
					_app_command_properties (hwnd);
					break;
				}

				case IDM_PURGE_UNUSED:
				{
					_app_command_purgeunused (hwnd);
					break;
				}

				case IDM_PURGE_TIMERS:
				{
					_app_command_purgetimers (hwnd);
					break;
				}

				case IDM_SELECT_ALL:
				{
					ListView_SetItemState (GetDlgItem (hwnd, (INT)_r_tab_getitemlparam (hwnd, IDC_TAB, -1)), -1, LVIS_SELECTED, LVIS_SELECTED);
					break;
				}

				case IDM_ZOOM:
				{
					ShowWindow (hwnd, IsZoomed (hwnd) ? SW_RESTORE : SW_MAXIMIZE);
					break;
				}

				case IDM_CTRL1:
				case IDM_CTRL2:
				{
					INT count;
					INT item;

					count = _r_tab_getitemcount (hwnd, IDC_TAB);

					if (!count)
						break;

					item = _r_tab_getcurrentitem (hwnd, IDC_TAB);

					if (item == -1)
						break;

					if (ctrl_id == IDM_CTRL1)
					{
						item += 1;

						if (item >= count)
							item = 0;
					}
					else
					{
						item -= 1;

						if (item == -1)
							item = count - 1;
					}

					_r_tab_selectitem (hwnd, IDC_TAB, item);

					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

INT APIENTRY wWinMain (_In_ HINSTANCE hinst, _In_opt_ HINSTANCE prev_hinst, _In_ LPWSTR cmdline, _In_ INT show_cmd)
{
	MSG msg;

	RtlSecureZeroMemory (&config, sizeof (config));

	if (_r_app_initialize ())
	{
		WCHAR arguments_mutex_name[16];
		HANDLE arguments_mutex = NULL;
		_r_str_printf (arguments_mutex_name, RTL_NUMBER_OF (arguments_mutex_name), L"%sCmd", _r_app_getnameshort ());

		// parse arguments
		if (!_r_mutex_isexists (arguments_mutex_name))
		{
			_r_mutex_create (arguments_mutex_name, &arguments_mutex);

			INT numargs;
			LPWSTR* arga = CommandLineToArgvW (cmdline, &numargs);

			if (arga)
			{
				BOOLEAN is_install = FALSE;
				BOOLEAN is_uninstall = FALSE;
				BOOLEAN is_silent = FALSE;
				BOOLEAN is_temporary = FALSE;
				LPWSTR argument;

				for (INT i = 0; i < numargs; i++)
				{
					argument = arga[i];

					if (!(*argument == L'/' || *argument == L'-'))
						continue;

					argument += 1;

					if (_r_str_compare (argument, L"install") == 0)
					{
						is_install = TRUE;
					}
					else if (_r_str_compare (argument, L"uninstall") == 0)
					{
						is_uninstall = TRUE;
					}
					else if (_r_str_compare (argument, L"silent") == 0)
					{
						is_silent = TRUE;
					}
					else if (_r_str_compare (argument, L"temp") == 0)
					{
						is_temporary = TRUE;
					}
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
								if (is_silent)
								{
									OutputDebugString (L"If you'd like to make a call, please hang up and try again!");
								}
								else
								{
									if (((_wfp_isfiltersinstalled () != InstallDisabled) && _app_installmessage (NULL, FALSE)))
									{
										_wfp_destroyfilters (hengine);
										_wfp_uninitialize (hengine, TRUE);
									}
								}
							}
						}
					}
					else
					{
						_r_mutex_destroy (&arguments_mutex);

						return ERROR_ACCESS_DENIED;
					}

					_r_mutex_destroy (&arguments_mutex);

					return ERROR_SUCCESS;
				}
			}

			_r_mutex_destroy (&arguments_mutex);
		}
		else
		{
			return ERROR_SUCCESS;
		}

		if (_r_app_createwindow (IDD_MAIN, IDI_MAIN, &DlgProc))
		{
			HACCEL haccel = LoadAccelerators (hinst, MAKEINTRESOURCE (IDA_MAIN));

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
