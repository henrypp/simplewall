// simplewall
// Copyright (c) 2016-2021 Henry++

#include "global.h"

NTSTATUS NTAPI NetworkMonitorThread (_In_ PVOID arglist)
{
	PR_HASHTABLE checker_map;
	PITEM_NETWORK ptr_network;
	PITEM_CONTEXT context;
	ULONG_PTR network_hash;
	PR_STRING string;
	HWND hwnd;
	SIZE_T enum_key;
	ULONG network_timeout;
	INT item_id;
	BOOLEAN is_highlighting_enabled;
	BOOLEAN is_refresh;

	network_timeout = _r_config_getulong (L"NetworkTimeout", NETWORK_TIMEOUT);

	if (network_timeout == 0 || network_timeout == INFINITE)
		return STATUS_SUCCESS;

	hwnd = (HWND)arglist;
	network_timeout = _r_calc_clamp32 (network_timeout, 1000, 60 * 1000); // set allowed range
	checker_map = _r_obj_createhashtablepointer (8);

	while (TRUE)
	{
		_app_generate_connections (network_table, checker_map);

		is_highlighting_enabled = _r_config_getboolean (L"IsEnableHighlighting", TRUE) && _r_config_getboolean_ex (L"IsHighlightConnection", TRUE, L"colors");
		is_refresh = FALSE;
		enum_key = 0;

		// add new connections into list
		while (_r_obj_enumhashtablepointer (network_table, &ptr_network, &network_hash, &enum_key))
		{
			string = _r_obj_findhashtablepointer (checker_map, network_hash);

			if (!string)
				continue;

			_r_obj_dereference (string);

			item_id = _r_listview_getitemcount (hwnd, IDC_NETWORK);

			_r_listview_additem_ex (hwnd, IDC_NETWORK, item_id, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK, I_GROUPIDCALLBACK, _app_createlistviewcontext (network_hash));

			if (ptr_network->path && ptr_network->app_hash)
				_app_queryfileinformation (ptr_network->path, ptr_network->app_hash, ptr_network->type, IDC_NETWORK);

			// resolve network address
			context = _r_freelist_allocateitem (&context_free_list);

			context->hwnd = hwnd;
			context->listview_id = IDC_NETWORK;
			context->lparam = network_hash;
			context->ptr_network = _r_obj_reference (ptr_network);

			_r_workqueue_queueitem (&resolver_queue, &_app_queueresolveinformation, context);

			is_refresh = TRUE;
		}

		// refresh network tab as well
		if (is_refresh)
		{
			_app_updatelistviewbylparam (hwnd, IDC_NETWORK, PR_UPDATE_NORESIZE);
		}

		// remove closed connections from list
		INT item_count = _r_listview_getitemcount (hwnd, IDC_NETWORK);

		if (item_count)
		{
			ULONG_PTR app_hash;

			for (INT i = item_count - 1; i != -1; i--)
			{
				network_hash = _app_getlistviewitemcontext (hwnd, IDC_NETWORK, i);

				if (_r_obj_findhashtable (checker_map, network_hash))
					continue;

				_r_listview_deleteitem (hwnd, IDC_NETWORK, i);

				app_hash = _app_getnetworkapp (network_hash);

				_r_queuedlock_acquireexclusive (&lock_network);

				_r_obj_removehashtablepointer (network_table, network_hash);

				_r_queuedlock_releaseexclusive (&lock_network);

				// redraw listview item
				if (app_hash)
				{
					if (is_highlighting_enabled)
					{
						_app_setlistviewbylparam (hwnd, app_hash, PR_SETITEM_REDRAW, TRUE);
					}
				}
			}
		}

		WaitForSingleObjectEx (NtCurrentThread (), network_timeout, FALSE);
	}

	return STATUS_SUCCESS;
}

BOOLEAN _app_changefilters (_In_ HWND hwnd, _In_ BOOLEAN is_install, _In_ BOOLEAN is_forced)
{
	PITEM_CONTEXT context;
	INT listview_id;

	listview_id = _app_getcurrentlistview_id (hwnd);

	_app_listviewsort (hwnd, listview_id, -1, FALSE);

	if (is_forced || _wfp_isfiltersinstalled ())
	{
		_app_initinterfacestate (hwnd, TRUE);

		context = _r_freelist_allocateitem (&context_free_list);

		context->hwnd = hwnd;
		context->is_install = is_install;

		_r_workqueue_queueitem (&wfp_queue, &_wfp_applythread, context);

		return TRUE;
	}

	_r_listview_redraw (hwnd, listview_id, -1);

	_app_profile_save ();

	return FALSE;
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

	TASKDIALOG_BUTTON td_buttons[2] = {0};
	TASKDIALOG_BUTTON td_radios[2] = {0};

	tdc.cbSize = sizeof (tdc);
	tdc.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_NO_SET_FOREGROUND | TDF_VERIFICATION_FLAG_CHECKED;
	tdc.hwndParent = hwnd;
	tdc.pszWindowTitle = _r_app_getname ();
	tdc.pszMainIcon = is_install ? TD_INFORMATION_ICON : TD_WARNING_ICON;
	//tdc.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
	tdc.pszMainInstruction = _r_app_getname ();
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

		_r_str_copy (radio_text_1, RTL_NUMBER_OF (radio_text_1), _r_locale_getstring (IDS_INSTALL_PERMANENT));
		_r_str_copy (radio_text_2, RTL_NUMBER_OF (radio_text_2), _r_locale_getstring (IDS_INSTALL_TEMPORARY));

		td_radios[0].nButtonID = IDYES;
		td_radios[0].pszButtonText = radio_text_1;

		td_radios[1].nButtonID = IDNO;
		td_radios[1].pszButtonText = radio_text_2;
	}
	else
	{
		_r_str_copy (str_main, RTL_NUMBER_OF (str_main), _r_locale_getstring (IDS_QUESTION_STOP));
		_r_str_copy (str_flag, RTL_NUMBER_OF (str_flag), _r_locale_getstring (IDS_ENABLEWINDOWSFIREWALL_CHK));
	}

	INT command_id;
	INT radio_id;
	BOOL is_flagchecked;

	tdc.pszContent = str_main;
	tdc.pszVerificationText = str_flag;

	if (_r_msg_taskdialog (&tdc, &command_id, &radio_id, &is_flagchecked))
	{
		if (command_id == IDYES)
		{
			if (is_install)
			{
				config.is_filterstemporary = (radio_id == IDNO);

				if (is_flagchecked)
					_wfp_firewallenable (FALSE);
			}
			else
			{
				config.is_filterstemporary = FALSE;

				if (is_flagchecked)
					_wfp_firewallenable (TRUE);
			}

			return TRUE;
		}
	}

	return FALSE;
}

VOID _app_config_apply (_In_ HWND hwnd, _In_opt_ HWND hsettings, _In_ INT ctrl_id)
{
	HMENU hmenu;
	HANDLE hengine;

	BOOLEAN new_val;

	switch (ctrl_id)
	{
		case IDC_LOADONSTARTUP_CHK:
		case IDM_LOADONSTARTUP_CHK:
		{
			new_val = !_r_autorun_isenabled ();
			break;
		}

		case IDC_STARTMINIMIZED_CHK:
		case IDM_STARTMINIMIZED_CHK:
		{
			new_val = !_r_config_getboolean (L"IsStartMinimized", FALSE);
			break;
		}

		case IDC_SKIPUACWARNING_CHK:
		case IDM_SKIPUACWARNING_CHK:
		{
			new_val = !_r_skipuac_isenabled ();
			break;
		}

		case IDC_CHECKUPDATES_CHK:
		case IDM_CHECKUPDATES_CHK:
		{
			new_val = !_r_config_getboolean (L"CheckUpdates", TRUE);
			break;
		}

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
		{
			new_val = !_r_config_getboolean (L"IsSecureFilters", TRUE);
			break;
		}

		case IDC_USESTEALTHMODE_CHK:
		{
			new_val = !_r_config_getboolean (L"UseStealthMode", TRUE);
			break;
		}

		case IDC_INSTALLBOOTTIMEFILTERS_CHK:
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
			new_val = !_r_config_getboolean (L"IsCertificatesEnabled", TRUE);
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

	hmenu = GetMenu (hwnd);

	switch (ctrl_id)
	{
		case IDC_LOADONSTARTUP_CHK:
		case IDM_LOADONSTARTUP_CHK:
		{
			BOOLEAN is_enabled;

			_r_autorun_enable (hwnd, new_val);
			is_enabled = _r_autorun_isenabled ();

			_r_menu_checkitem (hmenu, IDM_LOADONSTARTUP_CHK, 0, MF_BYCOMMAND, is_enabled);

			if (hsettings)
				CheckDlgButton (hsettings, IDC_LOADONSTARTUP_CHK, is_enabled ? BST_CHECKED : BST_UNCHECKED);

			break;
		}

		case IDC_STARTMINIMIZED_CHK:
		case IDM_STARTMINIMIZED_CHK:
		{
			_r_config_setboolean (L"IsStartMinimized", new_val);
			_r_menu_checkitem (hmenu, IDM_STARTMINIMIZED_CHK, 0, MF_BYCOMMAND, new_val);

			break;
		}

		case IDC_SKIPUACWARNING_CHK:
		case IDM_SKIPUACWARNING_CHK:
		{
			BOOLEAN is_enabled;

			_r_skipuac_enable (hwnd, new_val);
			is_enabled = _r_skipuac_isenabled ();

			_r_menu_checkitem (hmenu, IDM_SKIPUACWARNING_CHK, 0, MF_BYCOMMAND, is_enabled);

			if (hsettings)
				CheckDlgButton (hsettings, IDC_SKIPUACWARNING_CHK, is_enabled ? BST_CHECKED : BST_UNCHECKED);

			break;
		}

		case IDC_CHECKUPDATES_CHK:
		case IDM_CHECKUPDATES_CHK:
		{
			_r_config_setboolean (L"CheckUpdates", new_val);
			_r_menu_checkitem (hmenu, IDM_CHECKUPDATES_CHK, 0, MF_BYCOMMAND, new_val);

			break;
		}

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
		{
			_r_config_setboolean (L"UseStealthMode", new_val);
			break;
		}

		case IDC_SECUREFILTERS_CHK:
		{
			_r_config_setboolean (L"IsSecureFilters", new_val);

			if (_wfp_isfiltersinstalled ())
			{
				hengine = _wfp_getenginehandle ();

				if (hengine)
				{
					PR_ARRAY guids;

					_app_setsecurityinfoforprovider (hengine, &GUID_WfpProvider, new_val);
					_app_setsecurityinfoforsublayer (hengine, &GUID_WfpSublayer, new_val);

					guids = _wfp_dumpfilters (hengine, &GUID_WfpProvider);

					if (guids)
					{
						for (SIZE_T i = 0; i < _r_obj_getarraysize (guids); i++)
						{
							LPCGUID guid = _r_obj_getarrayitem (guids, i);

							if (guid)
								_app_setsecurityinfoforfilter (hengine, guid, new_val, __LINE__);
						}

						_r_obj_dereference (guids);
					}
				}
			}

			break;
		}

		case IDC_INSTALLBOOTTIMEFILTERS_CHK:
		{
			_r_config_setboolean (L"InstallBoottimeFilters", new_val);
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
				PITEM_APP ptr_app;
				SIZE_T enum_key = 0;

				_r_queuedlock_acquireshared (&lock_apps);

				while (_r_obj_enumhashtablepointer (apps_table, &ptr_app, NULL, &enum_key))
				{
					if (ptr_app->real_path)
						_app_queryfileinformation (ptr_app->real_path, ptr_app->app_hash, ptr_app->type, _app_getlistviewbytype_id (ptr_app->type));
				}

				_r_queuedlock_releaseshared (&lock_apps);
			}

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
		case IDM_LOADONSTARTUP_CHK:
		case IDC_LOADONSTARTUP_CHK:
		case IDM_STARTMINIMIZED_CHK:
		case IDC_STARTMINIMIZED_CHK:
		case IDM_SKIPUACWARNING_CHK:
		case IDC_SKIPUACWARNING_CHK:
		case IDM_CHECKUPDATES_CHK:
		case IDC_CHECKUPDATES_CHK:
		case IDC_SECUREFILTERS_CHK:
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
		hengine = _wfp_getenginehandle ();

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
					CheckDlgButton (hwnd, IDC_ALWAYSONTOP_CHK, _r_config_getboolean (L"AlwaysOnTop", FALSE) ? BST_CHECKED : BST_UNCHECKED);

#if defined(APP_HAVE_AUTORUN)
					CheckDlgButton (hwnd, IDC_LOADONSTARTUP_CHK, _r_autorun_isenabled () ? BST_CHECKED : BST_UNCHECKED);
#endif // APP_HAVE_AUTORUN

					CheckDlgButton (hwnd, IDC_STARTMINIMIZED_CHK, _r_config_getboolean (L"IsStartMinimized", FALSE) ? BST_CHECKED : BST_UNCHECKED);

#if defined(APP_HAVE_SKIPUAC)
					CheckDlgButton (hwnd, IDC_SKIPUACWARNING_CHK, _r_skipuac_isenabled () ? BST_CHECKED : BST_UNCHECKED);
#endif // APP_HAVE_SKIPUAC

					CheckDlgButton (hwnd, IDC_CHECKUPDATES_CHK, _r_config_getboolean (L"CheckUpdates", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					_r_locale_enum (hwnd, IDC_LANGUAGE, 0);

					break;
				}

				case IDD_SETTINGS_RULES:
				{
					HWND htip;

					CheckDlgButton (hwnd, IDC_RULE_BLOCKOUTBOUND, _r_config_getboolean (L"BlockOutboundConnections", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_RULE_BLOCKINBOUND, _r_config_getboolean (L"BlockInboundConnections", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_RULE_ALLOWLOOPBACK, _r_config_getboolean (L"AllowLoopbackConnections", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_RULE_ALLOW6TO4, _r_config_getboolean (L"AllowIPv6", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_SECUREFILTERS_CHK, _r_config_getboolean (L"IsSecureFilters", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_USESTEALTHMODE_CHK, _r_config_getboolean (L"UseStealthMode", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_INSTALLBOOTTIMEFILTERS_CHK, _r_config_getboolean (L"InstallBoottimeFilters", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_USECERTIFICATES_CHK, _r_config_getboolean (L"IsCertificatesEnabled", TRUE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_USENETWORKRESOLUTION_CHK, _r_config_getboolean (L"IsNetworkResolutionsEnabled", FALSE) ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_USEREFRESHDEVICES_CHK, _r_config_getboolean (L"IsRefreshDevices", TRUE) ? BST_CHECKED : BST_UNCHECKED);

					htip = _r_ctrl_createtip (hwnd);

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

					CheckDlgButton (hwnd, (IDC_BLOCKLIST_SPY_DISABLE + _r_calc_clamp32 (_r_config_getlong (L"BlocklistSpyState", 2), 0, 2)), BST_CHECKED);
					CheckDlgButton (hwnd, (IDC_BLOCKLIST_UPDATE_DISABLE + _r_calc_clamp32 (_r_config_getlong (L"BlocklistUpdateState", 0), 0, 2)), BST_CHECKED);
					CheckDlgButton (hwnd, (IDC_BLOCKLIST_EXTRA_DISABLE + _r_calc_clamp32 (_r_config_getlong (L"BlocklistExtraState", 0), 0, 2)), BST_CHECKED);

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

					PITEM_COLOR ptr_clr;
					SIZE_T enum_key;
					LONG icon_id;
					INT item_id;

					icon_id = _app_getdefaultappicon_id ();

					_app_setcheckboxlock (hwnd, IDC_COLORS, TRUE);

					enum_key = 0;
					item_id = 0;

					while (_r_obj_enumhashtable (colors_table, &ptr_clr, NULL, &enum_key))
					{
						ptr_clr->new_clr = _r_config_getulong_ex (_r_obj_getstring (ptr_clr->config_value), ptr_clr->default_clr, L"colors");

						_r_listview_additem_ex (hwnd, IDC_COLORS, item_id, _r_locale_getstring (ptr_clr->locale_id), icon_id, I_GROUPIDNONE, (LPARAM)ptr_clr);
						_r_listview_setitemcheck (hwnd, IDC_COLORS, item_id, _r_config_getboolean_ex (_r_obj_getstring (ptr_clr->config_name), ptr_clr->is_enabled, L"colors"));

						item_id += 1;
					}

					_app_setcheckboxlock (hwnd, IDC_COLORS, FALSE);

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
					PR_STRING string;

					CheckDlgButton (hwnd, IDC_ENABLELOG_CHK, _r_config_getboolean (L"IsLogEnabled", FALSE) ? BST_CHECKED : BST_UNCHECKED);

					string = _app_getlogpath ();

					if (string)
					{
						_r_ctrl_setstring (hwnd, IDC_LOGPATH, string->buffer);

						_r_obj_dereference (string);
					}

					string = _app_getlogviewer ();

					if (string)
					{
						_r_ctrl_setstring (hwnd, IDC_LOGVIEWER, string->buffer);

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
			_r_ctrl_setstringformat (hwnd, IDC_TITLE_GENERAL, L"%s:", _r_locale_getstring (IDS_TITLE_GENERAL));
			_r_ctrl_setstringformat (hwnd, IDC_TITLE_LANGUAGE, L"%s: (Language)", _r_locale_getstring (IDS_TITLE_LANGUAGE));
			_r_ctrl_setstringformat (hwnd, IDC_TITLE_BLOCKLIST_SPY, L"%s:", _r_locale_getstring (IDS_BLOCKLIST_SPY));
			_r_ctrl_setstringformat (hwnd, IDC_TITLE_BLOCKLIST_UPDATE, L"%s:", _r_locale_getstring (IDS_BLOCKLIST_UPDATE));
			_r_ctrl_setstringformat (hwnd, IDC_TITLE_BLOCKLIST_EXTRA, L"%s: (Skype, Bing, Live, Outlook, etc.)", _r_locale_getstring (IDS_BLOCKLIST_EXTRA));
			_r_ctrl_setstringformat (hwnd, IDC_TITLE_CONNECTIONS, L"%s:", _r_locale_getstring (IDS_TAB_NETWORK));
			_r_ctrl_setstringformat (hwnd, IDC_TITLE_SECURITY, L"%s:", _r_locale_getstring (IDS_TITLE_SECURITY));
			_r_ctrl_setstringformat (hwnd, IDC_TITLE_CONFIRMATIONS, L"%s:", _r_locale_getstring (IDS_TITLE_CONFIRMATIONS));
			_r_ctrl_setstringformat (hwnd, IDC_TITLE_TRAY, L"%s:", _r_locale_getstring (IDS_TITLE_TRAY));
			_r_ctrl_setstringformat (hwnd, IDC_TITLE_HIGHLIGHTING, L"%s:", _r_locale_getstring (IDS_TITLE_HIGHLIGHTING));
			_r_ctrl_setstringformat (hwnd, IDC_TITLE_LOGGING, L"%s:", _r_locale_getstring (IDS_TITLE_LOGGING));
			_r_ctrl_setstringformat (hwnd, IDC_TITLE_LOGVIEWER, L"%s:", _r_locale_getstring (IDS_LOGVIEWER_HINT));
			_r_ctrl_setstringformat (hwnd, IDC_TITLE_INTERFACE, L"%s:", _r_locale_getstring (IDS_TITLE_INTERFACE));
			_r_ctrl_setstringformat (hwnd, IDC_TITLE_EXCLUDE, L"%s:", _r_locale_getstring (IDS_TITLE_EXCLUDE));
			_r_ctrl_setstringformat (hwnd, IDC_TITLE_ADVANCED, L"%s:", _r_locale_getstring (IDS_TITLE_ADVANCED));

			switch (dialog_id)
			{
				case IDD_SETTINGS_GENERAL:
				{
					_r_ctrl_setstring (hwnd, IDC_ALWAYSONTOP_CHK, _r_locale_getstring (IDS_ALWAYSONTOP_CHK));
					_r_ctrl_setstring (hwnd, IDC_LOADONSTARTUP_CHK, _r_locale_getstring (IDS_LOADONSTARTUP_CHK));
					_r_ctrl_setstring (hwnd, IDC_STARTMINIMIZED_CHK, _r_locale_getstring (IDS_STARTMINIMIZED_CHK));
					_r_ctrl_setstring (hwnd, IDC_SKIPUACWARNING_CHK, _r_locale_getstring (IDS_SKIPUACWARNING_CHK));
					_r_ctrl_setstring (hwnd, IDC_CHECKUPDATES_CHK, _r_locale_getstring (IDS_CHECKUPDATES_CHK));

					_r_ctrl_setstring (hwnd, IDC_LANGUAGE_HINT, _r_locale_getstring (IDS_LANGUAGE_HINT));

					break;
				}

				case IDD_SETTINGS_RULES:
				{
					LPCWSTR recommended_string;

					recommended_string = _r_locale_getstring (IDS_RECOMMENDED);

					_r_ctrl_setstringformat (hwnd, IDC_RULE_BLOCKOUTBOUND, L"%s (%s)", _r_locale_getstring (IDS_RULE_BLOCKOUTBOUND), recommended_string);
					_r_ctrl_setstringformat (hwnd, IDC_RULE_BLOCKINBOUND, L"%s (%s)", _r_locale_getstring (IDS_RULE_BLOCKINBOUND), recommended_string);

					_r_ctrl_setstringformat (hwnd, IDC_RULE_ALLOWLOOPBACK, L"%s (%s)", _r_locale_getstring (IDS_RULE_ALLOWLOOPBACK), recommended_string);
					_r_ctrl_setstringformat (hwnd, IDC_RULE_ALLOW6TO4, L"%s (%s)", _r_locale_getstring (IDS_RULE_ALLOW6TO4), recommended_string);

					_r_ctrl_setstringformat (hwnd, IDC_SECUREFILTERS_CHK, L"%s (%s)", _r_locale_getstring (IDS_SECUREFILTERS_CHK), recommended_string);
					_r_ctrl_setstringformat (hwnd, IDC_USESTEALTHMODE_CHK, L"%s (%s)", _r_locale_getstring (IDS_USESTEALTHMODE_CHK), recommended_string);
					_r_ctrl_setstringformat (hwnd, IDC_INSTALLBOOTTIMEFILTERS_CHK, L"%s (%s)", _r_locale_getstring (IDS_INSTALLBOOTTIMEFILTERS_CHK), recommended_string);

					_r_ctrl_setstring (hwnd, IDC_USENETWORKRESOLUTION_CHK, _r_locale_getstring (IDS_USENETWORKRESOLUTION_CHK));
					_r_ctrl_setstring (hwnd, IDC_USECERTIFICATES_CHK, _r_locale_getstring (IDS_USECERTIFICATES_CHK));
					_r_ctrl_setstringformat (hwnd, IDC_USEREFRESHDEVICES_CHK, L"%s (%s)", _r_locale_getstring (IDS_USEREFRESHDEVICES_CHK), recommended_string);

					break;
				}

				case IDD_SETTINGS_BLOCKLIST:
				{
					LPCWSTR recommended_string;
					LPCWSTR disable_string;
					LPCWSTR allow_string;
					LPCWSTR block_string;

					recommended_string = _r_locale_getstring (IDS_RECOMMENDED);
					disable_string = _r_locale_getstring (IDS_DISABLE);
					allow_string = _r_locale_getstring (IDS_ACTION_ALLOW);
					block_string = _r_locale_getstring (IDS_ACTION_BLOCK);

					_r_ctrl_setstring (hwnd, IDC_BLOCKLIST_SPY_DISABLE, disable_string);
					_r_ctrl_setstring (hwnd, IDC_BLOCKLIST_SPY_ALLOW, allow_string);
					_r_ctrl_setstringformat (hwnd, IDC_BLOCKLIST_SPY_BLOCK, L"%s (%s)", block_string, recommended_string);

					_r_ctrl_setstringformat (hwnd, IDC_BLOCKLIST_UPDATE_DISABLE, L"%s (%s)", disable_string, recommended_string);
					_r_ctrl_setstring (hwnd, IDC_BLOCKLIST_UPDATE_ALLOW, allow_string);
					_r_ctrl_setstring (hwnd, IDC_BLOCKLIST_UPDATE_BLOCK, block_string);

					_r_ctrl_setstringformat (hwnd, IDC_BLOCKLIST_EXTRA_DISABLE, L"%s (%s)", disable_string, recommended_string);
					_r_ctrl_setstring (hwnd, IDC_BLOCKLIST_EXTRA_ALLOW, allow_string);
					_r_ctrl_setstring (hwnd, IDC_BLOCKLIST_EXTRA_BLOCK, block_string);

					_r_ctrl_setstringformat (hwnd, IDC_BLOCKLIST_INFO, L"Author: <a href=\"%s\">WindowsSpyBlocker</a> - block spying and tracking on Windows systems.", WINDOWSSPYBLOCKER_URL);

					break;
				}

				case IDD_SETTINGS_INTERFACE:
				{
					_r_ctrl_setstring (hwnd, IDC_CONFIRMEXIT_CHK, _r_locale_getstring (IDS_CONFIRMEXIT_CHK));
					_r_ctrl_setstring (hwnd, IDC_CONFIRMEXITTIMER_CHK, _r_locale_getstring (IDS_CONFIRMEXITTIMER_CHK));
					_r_ctrl_setstring (hwnd, IDC_CONFIRMLOGCLEAR_CHK, _r_locale_getstring (IDS_CONFIRMLOGCLEAR_CHK));

					_r_ctrl_setstring (hwnd, IDC_TRAYICONSINGLECLICK_CHK, _r_locale_getstring (IDS_TRAYICONSINGLECLICK_CHK));

					break;
				}

				case IDD_SETTINGS_HIGHLIGHTING:
				{
					PITEM_COLOR ptr_clr;

					_r_listview_setcolumn (hwnd, IDC_COLORS, 0, NULL, -100);

					for (INT i = 0; i < _r_listview_getitemcount (hwnd, IDC_COLORS); i++)
					{
						ptr_clr = (PITEM_COLOR)_r_listview_getitemlparam (hwnd, IDC_COLORS, i);

						if (ptr_clr)
						{
							_r_listview_setitem (hwnd, IDC_COLORS, i, 0, _r_locale_getstring (ptr_clr->locale_id));
						}
					}

					_r_ctrl_setstring (hwnd, IDC_COLORS_HINT, _r_locale_getstring (IDS_COLORS_HINT));

					break;
				}

				case IDD_SETTINGS_NOTIFICATIONS:
				{
					_r_ctrl_setstring (hwnd, IDC_ENABLENOTIFICATIONS_CHK, _r_locale_getstring (IDS_ENABLENOTIFICATIONS_CHK));
					_r_ctrl_setstring (hwnd, IDC_NOTIFICATIONSOUND_CHK, _r_locale_getstring (IDS_NOTIFICATIONSOUND_CHK));
					_r_ctrl_setstring (hwnd, IDC_NOTIFICATIONFULLSCREENSILENTMODE_CHK, _r_locale_getstring (IDS_NOTIFICATIONFULLSCREENSILENTMODE_CHK));
					_r_ctrl_setstring (hwnd, IDC_NOTIFICATIONONTRAY_CHK, _r_locale_getstring (IDS_NOTIFICATIONONTRAY_CHK));

					_r_ctrl_setstring (hwnd, IDC_NOTIFICATIONTIMEOUT_HINT, _r_locale_getstring (IDS_NOTIFICATIONTIMEOUT_HINT));

					_r_ctrl_setstringformat (hwnd, IDC_EXCLUDEBLOCKLIST_CHK, L"%s %s", _r_locale_getstring (IDS_TITLE_EXCLUDE), _r_locale_getstring (IDS_EXCLUDEBLOCKLIST_CHK));
					_r_ctrl_setstringformat (hwnd, IDC_EXCLUDECUSTOM_CHK, L"%s %s", _r_locale_getstring (IDS_TITLE_EXCLUDE), _r_locale_getstring (IDS_EXCLUDECUSTOM_CHK));

					break;
				}

				case IDD_SETTINGS_LOGGING:
				{
					_r_ctrl_setstring (hwnd, IDC_ENABLELOG_CHK, _r_locale_getstring (IDS_ENABLELOG_CHK));

					_r_ctrl_setstring (hwnd, IDC_LOGSIZELIMIT_HINT, _r_locale_getstring (IDS_LOGSIZELIMIT_HINT));

					_r_ctrl_setstringformat (hwnd, IDC_ENABLEUILOG_CHK, L"%s (session only)", _r_locale_getstring (IDS_ENABLEUILOG_CHK));

					_r_ctrl_setstringformat (hwnd, IDC_EXCLUDESTEALTH_CHK, L"%s %s", _r_locale_getstring (IDS_TITLE_EXCLUDE), _r_locale_getstring (IDS_EXCLUDESTEALTH_CHK));
					_r_ctrl_setstringformat (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, L"%s %s [win8+]", _r_locale_getstring (IDS_TITLE_EXCLUDE), _r_locale_getstring (IDS_EXCLUDECLASSIFYALLOW_CHK));

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
			INT ctrl_id;

			ctrl_id = GetDlgCtrlID ((HWND)lparam);

			if (ctrl_id == IDC_LOGSIZELIMIT)
				_r_config_setulong (L"LogSizeLimitKb", (ULONG)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));

			else if (ctrl_id == IDC_NOTIFICATIONTIMEOUT)
				_r_config_setulong (L"NotificationsTimeout", (ULONG)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp;

			nmlp = (LPNMHDR)lparam;

			switch (nmlp->code)
			{
				case TTN_GETDISPINFO:
				{
					LPNMTTDISPINFO lpnmdi = (LPNMTTDISPINFO)lparam;

					if ((lpnmdi->uFlags & TTF_IDISHWND) != TTF_IDISHWND)
						break;

					WCHAR buffer[1024] = {0};
					INT ctrl_id;

					ctrl_id = GetDlgCtrlID ((HWND)(lpnmdi->hdr.idFrom));

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

					break;
				}

				case LVN_ITEMCHANGED:
				{
					LPNMLISTVIEW lpnmlv;
					INT listview_id;

					lpnmlv = (LPNMLISTVIEW)lparam;
					listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					if ((lpnmlv->uChanged & LVIF_STATE) != 0)
					{
						if (listview_id != IDC_COLORS)
							break;

						if ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (1) || ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (2)))
						{
							if (_app_ischeckboxlocked (lpnmlv->hdr.hwndFrom))
								break;

							PITEM_COLOR ptr_clr;
							BOOLEAN is_enabled;

							ptr_clr = (PITEM_COLOR)lpnmlv->lParam;

							if (ptr_clr)
							{
								is_enabled = (lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (2);

								_r_config_setboolean_ex (_r_obj_getstring (ptr_clr->config_name), is_enabled, L"colors");

								_r_listview_redraw (_r_app_gethwnd (), _app_getcurrentlistview_id (_r_app_gethwnd ()), -1);
							}
						}

					}

					break;
				}

				case NM_CUSTOMDRAW:
				{
					LONG_PTR result;

					result = _app_message_custdraw (hwnd, (LPNMLVCUSTOMDRAW)lparam);

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, result);
					return result;
				}

				case NM_DBLCLK:
				{
					LPNMITEMACTIVATE lpnmlv;
					INT listview_id;

					lpnmlv = (LPNMITEMACTIVATE)lparam;
					listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					if (lpnmlv->iItem == -1)
						break;

					if (listview_id == IDC_COLORS)
					{
						PITEM_COLOR ptr_clr_current;

						ptr_clr_current = (PITEM_COLOR)_r_listview_getitemlparam (hwnd, listview_id, lpnmlv->iItem);

						if (ptr_clr_current)
						{
							CHOOSECOLOR cc = {0};
							COLORREF cust[16] = {0};

							PITEM_COLOR ptr_clr;
							SIZE_T index;
							SIZE_T enum_key;

							index = 0;
							enum_key = 0;

							while (_r_obj_enumhashtable (colors_table, &ptr_clr, NULL, &enum_key))
							{
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
								_r_config_setulong_ex (_r_obj_getstring (ptr_clr_current->config_value), cc.rgbResult, L"colors");

								_r_listview_redraw (hwnd, IDC_COLORS, -1);
								_r_listview_redraw (_r_app_gethwnd (), _app_getcurrentlistview_id (_r_app_gethwnd ()), -1);
							}
						}
					}

					break;
				}

				case NM_CLICK:
				case NM_RETURN:
				{
					PNMLINK pnmlink;

					pnmlink = (PNMLINK)lparam;

					if (!_r_str_isempty (pnmlink->item.szUrl))
					{
						_r_shell_opendefault (pnmlink->item.szUrl);
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

			switch (ctrl_id)
			{
				case IDC_ALWAYSONTOP_CHK:
				{
					BOOLEAN is_enabled = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

					_r_config_setboolean (L"AlwaysOnTop", is_enabled);
					_r_menu_checkitem (GetMenu (_r_app_gethwnd ()), IDM_ALWAYSONTOP_CHK, 0, MF_BYCOMMAND, is_enabled);

					break;
				}

				case IDC_LANGUAGE:
				{
					if (notify_code == CBN_SELCHANGE)
						_r_locale_apply (hwnd, ctrl_id, 0);

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

				case IDC_LOADONSTARTUP_CHK:
				case IDC_STARTMINIMIZED_CHK:
				case IDC_SKIPUACWARNING_CHK:
				case IDC_CHECKUPDATES_CHK:
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
					_app_config_apply (_r_app_gethwnd (), hwnd, ctrl_id);
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
					HMENU hmenu;
					LONG new_state;

					hmenu = GetMenu (_r_app_gethwnd ());

					if (ctrl_id >= IDC_BLOCKLIST_SPY_DISABLE && ctrl_id <= IDC_BLOCKLIST_SPY_BLOCK)
					{
						new_state = _r_calc_clamp32 (_r_ctrl_isradiobuttonchecked (hwnd, IDC_BLOCKLIST_SPY_DISABLE, IDC_BLOCKLIST_SPY_BLOCK) - IDC_BLOCKLIST_SPY_DISABLE, 0, 2);

						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_SPY_DISABLE, IDM_BLOCKLIST_SPY_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_SPY_DISABLE + new_state);

						_r_config_setlong (L"BlocklistSpyState", new_state);

						_app_ruleblocklistset (_r_app_gethwnd (), new_state, -1, -1, TRUE);
					}
					else if (ctrl_id >= IDC_BLOCKLIST_UPDATE_DISABLE && ctrl_id <= IDC_BLOCKLIST_UPDATE_BLOCK)
					{
						new_state = _r_calc_clamp32 (_r_ctrl_isradiobuttonchecked (hwnd, IDC_BLOCKLIST_UPDATE_DISABLE, IDC_BLOCKLIST_UPDATE_BLOCK) - IDC_BLOCKLIST_UPDATE_DISABLE, 0, 2);

						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_UPDATE_DISABLE, IDM_BLOCKLIST_UPDATE_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_UPDATE_DISABLE + new_state);

						_r_config_setlong (L"BlocklistUpdateState", new_state);

						_app_ruleblocklistset (_r_app_gethwnd (), -1, new_state, -1, TRUE);
					}
					else if (ctrl_id >= IDC_BLOCKLIST_EXTRA_DISABLE && ctrl_id <= IDC_BLOCKLIST_EXTRA_BLOCK)
					{
						new_state = _r_calc_clamp32 (_r_ctrl_isradiobuttonchecked (hwnd, IDC_BLOCKLIST_EXTRA_DISABLE, IDC_BLOCKLIST_EXTRA_BLOCK) - IDC_BLOCKLIST_EXTRA_DISABLE, 0, 2);

						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_EXTRA_DISABLE, IDM_BLOCKLIST_EXTRA_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_EXTRA_DISABLE + new_state);

						_r_config_setlong (L"BlocklistExtraState", new_state);

						_app_ruleblocklistset (_r_app_gethwnd (), -1, -1, new_state, TRUE);
					}

					break;
				}

				case IDC_ENABLELOG_CHK:
				{
					HWND hctrl;
					BOOLEAN is_postmessage;
					BOOLEAN is_enabled;
					BOOLEAN is_logging_enabled;

					is_postmessage = ((INT)lparam == WM_APP);
					is_enabled = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);
					is_logging_enabled = is_enabled || (IsDlgButtonChecked (hwnd, IDC_ENABLEUILOG_CHK) == BST_CHECKED);

					if (!is_postmessage)
					{
						_r_config_setboolean (L"IsLogEnabled", is_enabled);

						_app_loginit (is_enabled);
					}

					SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_PRESSBUTTON, IDM_TRAY_ENABLELOG_CHK, MAKELPARAM (is_enabled, 0));

					_r_ctrl_enable (hwnd, IDC_LOGPATH, is_enabled); // input
					_r_ctrl_enable (hwnd, IDC_LOGPATH_BTN, is_enabled); // button

					hctrl = (HWND)SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_GETBUDDY, 0, 0);

					if (hctrl)
						_r_ctrl_enable (hctrl, 0, is_enabled);

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
					PR_STRING log_path;

					if (notify_code == EN_KILLFOCUS)
					{
						log_path = _r_ctrl_getstring (hwnd, ctrl_id);

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
					static COMDLG_FILTERSPEC filters[] = {
						L"Log files (*.log, *.csv)", L"*.log;*.csv",
						L"All files (*.*)", L"*.*",
					};

					R_FILE_DIALOG file_dialog;
					PR_STRING path;

					if (_r_filedialog_initialize (&file_dialog, PR_FILEDIALOG_SAVEFILE))
					{
						_r_filedialog_setfilter (&file_dialog, filters, RTL_NUMBER_OF (filters));

						path = _r_ctrl_getstring (hwnd, IDC_LOGPATH);

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
								_r_ctrl_setstring (hwnd, IDC_LOGPATH, path->buffer);

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
					PR_STRING log_viewer;

					if (notify_code == EN_KILLFOCUS)
					{
						log_viewer = _r_ctrl_getstring (hwnd, ctrl_id);

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
					static COMDLG_FILTERSPEC filters[] = {
						L"Executable files (*.exe)", L"*.exe",
						L"All files (*.*)", L"*.*",
					};

					R_FILE_DIALOG file_dialog;
					PR_STRING path;

					if (_r_filedialog_initialize (&file_dialog, PR_FILEDIALOG_OPENFILE))
					{
						_r_filedialog_setfilter (&file_dialog, filters, RTL_NUMBER_OF (filters));

						path = _r_ctrl_getstring (hwnd, IDC_LOGVIEWER);

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
								_r_ctrl_setstring (hwnd, IDC_LOGVIEWER, path->buffer);
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
					HWND hctrl;
					BOOLEAN is_postmessage;
					BOOLEAN is_enabled;

					is_postmessage = ((INT)lparam == WM_APP);
					is_enabled = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

					if (!is_postmessage)
						_r_config_setboolean (L"IsNotificationsEnabled", is_enabled);

					SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_PRESSBUTTON, IDM_TRAY_ENABLENOTIFICATIONS_CHK, MAKELPARAM (is_enabled, 0));

					_r_ctrl_enable (hwnd, IDC_NOTIFICATIONSOUND_CHK, is_enabled);
					_r_ctrl_enable (hwnd, IDC_NOTIFICATIONONTRAY_CHK, is_enabled);

					hctrl = (HWND)SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_GETBUDDY, 0, 0);

					if (hctrl)
						_r_ctrl_enable (hctrl, 0, is_enabled);

					_r_ctrl_enable (hwnd, IDC_EXCLUDEBLOCKLIST_CHK, is_enabled);
					_r_ctrl_enable (hwnd, IDC_EXCLUDECUSTOM_CHK, is_enabled);

					PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDC_NOTIFICATIONSOUND_CHK, 0), WM_APP);

					if (!is_postmessage)
					{
						hctrl = _app_notify_getwindow ();

						if (hctrl)
							_app_notify_refresh (hctrl, FALSE);
					}

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
					HWND hnotify;

					_r_config_setboolean (L"IsNotificationsOnTray", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));

					hnotify = _app_notify_getwindow ();

					if (hnotify)
						_app_notify_setposition (hnotify, TRUE);

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

INT _app_addwindowtabs (_In_ HWND hwnd)
{
	INT tabs_count = 0;

	_r_tab_additem (hwnd, IDC_TAB, tabs_count++, L"", I_IMAGENONE, (LPARAM)IDC_APPS_PROFILE);
	_r_tab_additem (hwnd, IDC_TAB, tabs_count++, L"", I_IMAGENONE, (LPARAM)IDC_APPS_SERVICE);

	// uwp apps (win8+)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
	{
		_r_tab_additem (hwnd, IDC_TAB, tabs_count++, L"", I_IMAGENONE, (LPARAM)IDC_APPS_UWP);
	}

	if (!_r_config_getboolean (L"IsInternalRulesDisabled", FALSE))
	{
		_r_tab_additem (hwnd, IDC_TAB, tabs_count++, L"", I_IMAGENONE, (LPARAM)IDC_RULES_BLOCKLIST);
		_r_tab_additem (hwnd, IDC_TAB, tabs_count++, L"", I_IMAGENONE, (LPARAM)IDC_RULES_SYSTEM);
	}

	_r_tab_additem (hwnd, IDC_TAB, tabs_count++, L"", I_IMAGENONE, (LPARAM)IDC_RULES_CUSTOM);
	_r_tab_additem (hwnd, IDC_TAB, tabs_count++, L"", I_IMAGENONE, (LPARAM)IDC_NETWORK);
	_r_tab_additem (hwnd, IDC_TAB, tabs_count++, L"", I_IMAGENONE, (LPARAM)IDC_LOG);

	return tabs_count;
}

VOID _app_tabs_init (_In_ HWND hwnd)
{
	RECT rect = {0};

	LONG statusbar_height;
	LONG rebar_height;
	ULONG style;

	HWND hlistview;
	INT listview_id;
	INT tabs_count;

	statusbar_height = _r_status_getheight (hwnd, IDC_STATUSBAR);
	rebar_height = _r_rebar_getheight (hwnd, IDC_REBAR);

	GetClientRect (hwnd, &rect);

	SetWindowPos (config.hrebar, NULL, 0, 0, rect.right, rebar_height, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
	SetWindowPos (GetDlgItem (hwnd, IDC_TAB), NULL, 0, rebar_height, rect.right, rect.bottom - rebar_height - statusbar_height, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

	_app_listviewloadfont (hwnd, TRUE);

	style = LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_HEADERINALLVIEWS | LVS_EX_HEADERDRAGDROP;
	tabs_count = _app_addwindowtabs (hwnd);

	for (INT i = 0; i < tabs_count; i++)
	{
		listview_id = _app_getlistviewbytab_id (hwnd, i);
		hlistview = GetDlgItem (hwnd, listview_id);

		if (!hlistview)
			continue;

		_r_tab_adjustchild (hwnd, IDC_TAB, hlistview);

		if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_RULES_CUSTOM)
		{
			_r_listview_setstyle (hwnd, listview_id, style | LVS_EX_CHECKBOXES, TRUE); // with checkboxes

			if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
			{
				_r_listview_addcolumn (hwnd, listview_id, 0, L"", -80, LVCFMT_LEFT);
				_r_listview_addcolumn (hwnd, listview_id, 1, L"", -20, LVCFMT_RIGHT);
			}
			else
			{
				_r_listview_addcolumn (hwnd, listview_id, 0, L"", -80, LVCFMT_LEFT);
				_r_listview_addcolumn (hwnd, listview_id, 1, L"", -10, LVCFMT_RIGHT);
				_r_listview_addcolumn (hwnd, listview_id, 2, L"", -10, LVCFMT_RIGHT);
			}

			_r_listview_addgroup (hwnd, listview_id, 0, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
			_r_listview_addgroup (hwnd, listview_id, 1, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
			_r_listview_addgroup (hwnd, listview_id, 2, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
			_r_listview_addgroup (hwnd, listview_id, 3, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
		}
		else if (listview_id == IDC_NETWORK)
		{
			_r_listview_setstyle (hwnd, listview_id, style, TRUE);

			_r_listview_addcolumn (hwnd, listview_id, 0, L"", -20, LVCFMT_LEFT);
			_r_listview_addcolumn (hwnd, listview_id, 1, L"", -10, LVCFMT_LEFT);
			_r_listview_addcolumn (hwnd, listview_id, 2, L"", -10, LVCFMT_LEFT);
			_r_listview_addcolumn (hwnd, listview_id, 3, L"", -10, LVCFMT_RIGHT);
			_r_listview_addcolumn (hwnd, listview_id, 4, L"", -10, LVCFMT_LEFT);
			_r_listview_addcolumn (hwnd, listview_id, 5, L"", -10, LVCFMT_LEFT);
			_r_listview_addcolumn (hwnd, listview_id, 6, L"", -10, LVCFMT_RIGHT);
			_r_listview_addcolumn (hwnd, listview_id, 7, L"", -10, LVCFMT_RIGHT);
			_r_listview_addcolumn (hwnd, listview_id, 8, L"", -10, LVCFMT_RIGHT);

			_r_listview_addgroup (hwnd, listview_id, 0, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
			_r_listview_addgroup (hwnd, listview_id, 1, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
			_r_listview_addgroup (hwnd, listview_id, 2, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
		}
		else if (listview_id == IDC_LOG)
		{
			_r_listview_setstyle (hwnd, listview_id, style, TRUE);

			_r_listview_addcolumn (hwnd, listview_id, 0, L"", -10, LVCFMT_LEFT);
			_r_listview_addcolumn (hwnd, listview_id, 1, L"", -10, LVCFMT_LEFT);
			_r_listview_addcolumn (hwnd, listview_id, 2, L"", -10, LVCFMT_LEFT);
			_r_listview_addcolumn (hwnd, listview_id, 3, L"", -10, LVCFMT_RIGHT);
			_r_listview_addcolumn (hwnd, listview_id, 4, L"", -10, LVCFMT_LEFT);
			_r_listview_addcolumn (hwnd, listview_id, 5, L"", -10, LVCFMT_LEFT);
			_r_listview_addcolumn (hwnd, listview_id, 6, L"", -10, LVCFMT_RIGHT);
			_r_listview_addcolumn (hwnd, listview_id, 7, L"", -10, LVCFMT_LEFT);
			_r_listview_addcolumn (hwnd, listview_id, 8, L"", -10, LVCFMT_RIGHT);
			_r_listview_addcolumn (hwnd, listview_id, 9, L"", -10, LVCFMT_LEFT);
			_r_listview_addcolumn (hwnd, listview_id, 10, L"", -10, LVCFMT_RIGHT);

			_r_listview_addgroup (hwnd, listview_id, 0, L"", 0, LVGS_NOHEADER, LVGS_NOHEADER);
		}

		// add filter group
		_r_listview_addgroup (hwnd, listview_id, LV_HIDDEN_GROUP_ID, L"", 0, LVGS_HIDDEN | LVGS_NOHEADER | LVGS_COLLAPSED, LVGS_HIDDEN | LVGS_NOHEADER | LVGS_COLLAPSED);

		_app_listviewsetfont (hwnd, listview_id);

		BringWindowToTop (hlistview); // HACK!!!
	}
}

VOID _app_initialize ()
{
	static ULONG privileges[] = {
		SE_SECURITY_PRIVILEGE,
		SE_TAKE_OWNERSHIP_PRIVILEGE,
		SE_BACKUP_PRIVILEGE,
		SE_RESTORE_PRIVILEGE,
		SE_DEBUG_PRIVILEGE,
	};

	R_ENVIRONMENT environment;

	// set privileges
	_r_sys_setprocessprivilege (NtCurrentProcess (), privileges, RTL_NUMBER_OF (privileges), TRUE);

	// set process priority
	_r_sys_setenvironment (&environment, PROCESS_PRIORITY_CLASS_HIGH, IoPriorityNormal, MEMORY_PRIORITY_NORMAL);

	_r_sys_setprocessenvironment (NtCurrentProcess (), &environment);

	// initialize workqueue
	_r_sys_setenvironment (&environment, THREAD_PRIORITY_LOWEST, IoPriorityVeryLow, MEMORY_PRIORITY_NORMAL);

	_r_workqueue_initialize (&file_queue, 0, 12, 500, &environment);
	_r_workqueue_initialize (&resolver_queue, 0, 6, 500, &environment);
	_r_workqueue_initialize (&resolve_notify_queue, 0, 2, 500, &environment);

	_r_sys_setenvironment (&environment, THREAD_PRIORITY_ABOVE_NORMAL, IoPriorityNormal, MEMORY_PRIORITY_NORMAL);

	_r_workqueue_initialize (&log_queue, 0, 3, 500, &environment);

	_r_sys_setenvironment (&environment, THREAD_PRIORITY_HIGHEST, IoPriorityNormal, MEMORY_PRIORITY_NORMAL);

	_r_workqueue_initialize (&wfp_queue, 0, 1, 500, &environment);

	// static initializer
	{
		SIZE_T length;

		length = GetWindowsDirectory (config.windows_dir_buffer, RTL_NUMBER_OF (config.windows_dir_buffer));

		_r_obj_initializestringref_ex (&config.windows_dir, config.windows_dir_buffer, length * sizeof (WCHAR));
	}

	_r_str_printf (config.profile_path, RTL_NUMBER_OF (config.profile_path), L"%s\\" XML_PROFILE, _r_app_getprofiledirectory ());
	_r_str_printf (config.profile_path_backup, RTL_NUMBER_OF (config.profile_path_backup), L"%s\\" XML_PROFILE L".bak", _r_app_getprofiledirectory ());
	_r_str_printf (config.profile_internal_path, RTL_NUMBER_OF (config.profile_internal_path), L"%s\\" XML_PROFILE_INTERNAL, _r_app_getprofiledirectory ());

	config.my_path = _r_obj_createstring (_r_sys_getimagepath ());
	config.svchost_path = _r_obj_concatstrings (2, _r_sys_getsystemdirectory (), PATH_SVCHOST);
	config.system_path = _r_obj_createstring (PROC_SYSTEM_NAME);
	config.ntoskrnl_path = _r_obj_concatstrings (2, _r_sys_getsystemdirectory (), PATH_NTOSKRNL);

	config.my_hash = _r_obj_getstringhash (config.my_path);
	config.ntoskrnl_hash = _r_obj_getstringhash (config.system_path);
	config.svchost_hash = _r_obj_getstringhash (config.svchost_path);

	// initialize free list
	_r_freelist_initialize (&context_free_list, sizeof (ITEM_CONTEXT), 32);
	_r_freelist_initialize (&listview_free_list, sizeof (ITEM_LISTVIEW_CONTEXT), 512);

	// initialize colors array
	if (!colors_table)
	{
		colors_table = _r_obj_createhashtable (sizeof (ITEM_COLOR), NULL);

		// initialize colors
		config.color_timer = _app_addcolor (IDS_HIGHLIGHT_TIMER, L"IsHighlightTimer", TRUE, L"ColorTimer", LV_COLOR_TIMER);
		config.color_invalid = _app_addcolor (IDS_HIGHLIGHT_INVALID, L"IsHighlightInvalid", TRUE, L"ColorInvalid", LV_COLOR_INVALID);
		config.color_special = _app_addcolor (IDS_HIGHLIGHT_SPECIAL, L"IsHighlightSpecial", TRUE, L"ColorSpecial", LV_COLOR_SPECIAL);
		config.color_signed = _app_addcolor (IDS_HIGHLIGHT_SIGNED, L"IsHighlightSigned", TRUE, L"ColorSigned", LV_COLOR_SIGNED);
		config.color_pico = _app_addcolor (IDS_HIGHLIGHT_PICO, L"IsHighlightPico", TRUE, L"ColorPico", LV_COLOR_PICO);
		config.color_system = _app_addcolor (IDS_HIGHLIGHT_SYSTEM, L"IsHighlightSystem", TRUE, L"ColorSystem", LV_COLOR_SYSTEM);
		config.color_network = _app_addcolor (IDS_HIGHLIGHT_CONNECTION, L"IsHighlightConnection", TRUE, L"ColorConnection", LV_COLOR_CONNECTION);
	}

	_app_generate_credentials ();

	// load default icons
	_app_getdefaulticons ();

	// initialize global filters array object
	if (!filter_ids)
		filter_ids = _r_obj_createarray (sizeof (GUID), NULL);

	// initialize apps table
	if (!apps_table)
		apps_table = _r_obj_createhashtablepointer (32);

	// initialize context table
	if (!context_table)
		context_table = _r_obj_createhashtable (sizeof (SHORT), NULL);

	// initialize rules array object
	if (!rules_list)
		rules_list = _r_obj_createlist (&_r_obj_dereference);

	// initialize rules configuration table
	if (!rules_config)
		rules_config = _r_obj_createhashtable (sizeof (ITEM_RULE_CONFIG), &_app_dereferenceruleconfig);

	// initialize log hashtable object
	if (!log_table)
		log_table = _r_obj_createhashtablepointer (32);

	// initialize network table
	if (!network_table)
		network_table = _r_obj_createhashtablepointer (32);

	// initialize cache table
	if (!cache_information)
		cache_information = _r_obj_createhashtablepointer (32);

	if (!cache_resolution)
		cache_resolution = _r_obj_createhashtablepointer (32);
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

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			ENUM_INSTALL_TYPE install_type;
			R_ENVIRONMENT environment;

			_app_initialize ();

			app_global.main.hwnd = hwnd; // HACK!!!

			// init buffered paint
			BufferedPaintInit ();

			// allow drag&drop support
			DragAcceptFiles (hwnd, TRUE);

			// restore window position (required!)
			_r_window_restoreposition (hwnd, L"window");

			// initialize imagelist
			_app_imagelist_init (hwnd);

			// initialize toolbar
			_app_toolbar_init (hwnd);
			_app_toolbar_resize ();

			// initialize tabs
			_app_tabs_init (hwnd);

			// create notification window
			_app_notify_createwindow ();

			// load profile
			_app_profile_load (hwnd, NULL);

			// install filters
			install_type = _wfp_isfiltersinstalled ();

			if (install_type != INSTALL_DISABLED)
			{
				if (install_type == INSTALL_ENABLED_TEMPORARY)
					config.is_filterstemporary = TRUE;

				_app_changefilters (hwnd, TRUE, TRUE);
			}
			else
			{
				_r_status_settext (hwnd, IDC_STATUSBAR, 0, _r_locale_getstring (_app_getinterfacestatelocale (install_type)));
			}

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

			// add blocklist to update
			if (!_r_config_getboolean (L"IsInternalRulesDisabled", FALSE))
			{
				WCHAR internal_profile_version[64];
				_r_str_fromlong64 (internal_profile_version, RTL_NUMBER_OF (internal_profile_version), config.profile_internal_timestamp);

				_r_update_addcomponent (L"Internal rules", L"rules", internal_profile_version, config.profile_internal_path, FALSE);
			}

			// initialize layout manager
			_r_layout_initializemanager (&layout_manager, hwnd);

			_r_layout_setoriginalsize (&layout_manager, 500, 220);

			// create network monitor thread
			_r_sys_setenvironment (&environment, THREAD_PRIORITY_BELOW_NORMAL, IoPriorityNormal, MEMORY_PRIORITY_NORMAL);

			_r_sys_createthread (&NetworkMonitorThread, hwnd, NULL, &environment);

			// initialize tab
			_app_settab_id (hwnd, _r_config_getlong (L"CurrentTab", IDC_APPS_PROFILE));

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
			HICON hicon;

			LONG dpi_value;

			LONG icon_small_x;
			LONG icon_small_y;

			UINT icon_id;

			dpi_value = _r_dc_getsystemdpi ();

			icon_small_x = _r_dc_getsystemmetrics (SM_CXSMICON, dpi_value);
			icon_small_y = _r_dc_getsystemmetrics (SM_CYSMICON, dpi_value);

			icon_id = _wfp_isfiltersinstalled () ? IDI_ACTIVE : IDI_INACTIVE;

			hicon = _r_sys_loadsharedicon (_r_sys_getimagebase (), MAKEINTRESOURCE (icon_id), icon_small_x, icon_small_y);

			// refresh tray icon
			_r_tray_create (hwnd, &GUID_TrayIcon, WM_TRAYICON, hicon, _r_app_getname (), FALSE);

			break;
		}

		case RM_CONFIG_UPDATE:
		{
			_app_profile_save ();
			_app_profile_load (hwnd, NULL);

			_app_changefilters (hwnd, TRUE, FALSE);

			break;
		}

		case RM_CONFIG_RESET:
		{
			_r_queuedlock_acquireexclusive (&lock_rules_config);

			_r_obj_clearhashtable (rules_config);

			_r_queuedlock_releaseexclusive (&lock_rules_config);

			_r_fs_makebackup (config.profile_path, TRUE);

			_app_profile_load (hwnd, NULL);

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
			HANDLE hengine;

			_r_config_setlong (L"CurrentTab", _app_getcurrentlistview_id (hwnd));

			_r_tray_destroy (hwnd, &GUID_TrayIcon);

			_app_loginit (FALSE);

			if (_r_queuedlock_islocked (&lock_apply))
			{
				_r_workqueue_waitforfinish (&wfp_queue);
				_r_workqueue_destroy (&wfp_queue);
			}

			hengine = _wfp_getenginehandle ();

			if (hengine)
				_wfp_uninitialize (hengine, FALSE);

			_app_notify_destroywindow ();

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
			HDROP hdrop;
			ULONG_PTR app_hash;
			ULONG numfiles;
			ULONG length;

			hdrop = (HDROP)wparam;
			numfiles = DragQueryFile (hdrop, UINT32_MAX, NULL, 0);
			app_hash = 0;

			for (ULONG i = 0; i < numfiles; i++)
			{
				length = DragQueryFile (hdrop, i, NULL, 0);
				string = _r_obj_createstring_ex (NULL, length * sizeof (WCHAR));

				if (DragQueryFile (hdrop, i, string->buffer, length + 1))
				{
					app_hash = _app_addapplication (hwnd, DATA_UNKNOWN, &string->sr, NULL, NULL);
				}

				_r_obj_dereference (string);
			}

			DragFinish (hdrop);

			_app_profile_save ();

			if (app_hash)
			{
				_app_setlistviewbylparam (hwnd, app_hash, PR_SETITEM_UPDATE, TRUE);
				_app_showitembylparam (hwnd, app_hash, TRUE);
			}

			break;
		}

		case WM_DPICHANGED:
		{
			_app_message_dpichanged (hwnd);
			break;
		}

		case WM_SETTINGCHANGE:
		{
			_r_wnd_changesettings (hwnd, wparam, lparam);
			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp;

			nmlp = (LPNMHDR)lparam;

			switch (nmlp->code)
			{
				case TCN_SELCHANGING:
				{
					HWND hlistview;
					INT listview_id;

					listview_id = _app_getcurrentlistview_id (hwnd);

					if (!listview_id)
						break;

					hlistview = GetDlgItem (hwnd, listview_id);

					if (!hlistview)
						break;

					ShowWindow (hlistview, SW_HIDE);

					break;
				}

				case TCN_SELCHANGE:
				{
					HWND hlistview;
					INT listview_id;

					listview_id = _app_getcurrentlistview_id (hwnd);

					if (!listview_id)
						break;

					hlistview = GetDlgItem (hwnd, listview_id);

					if (!hlistview)
						break;

					_app_search_applyfilter (hwnd, listview_id, config.search_string);

					_app_updatelistviewbylparam (hwnd, listview_id, PR_UPDATE_FORCE | PR_UPDATE_NORESIZE);

					ShowWindow (hlistview, SW_SHOWNA);

					if (_r_wnd_isvisiblefull (hwnd)) // HACK!!!
						SetFocus (hlistview);

					_app_listviewresize (hwnd, listview_id, FALSE);

					break;
				}

				case NM_CUSTOMDRAW:
				{
					LONG_PTR result;

					result = _app_message_custdraw (hwnd, (LPNMLVCUSTOMDRAW)lparam);

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, result);
					return result;
				}

				case LVN_INSERTITEM:
				{
					LPNMLISTVIEW lpnmlv;
					PITEM_LISTVIEW_CONTEXT context;
					INT listview_id;

					lpnmlv = (LPNMLISTVIEW)lparam;
					listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					if (!(listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_LOG))
						break;

					if (_r_obj_isstringempty (config.search_string))
						break;

					context = (PITEM_LISTVIEW_CONTEXT)_r_listview_getitemlparam (hwnd, listview_id, lpnmlv->iItem);

					if (!context)
						break;

					_app_search_applyfilteritem (hwnd, listview_id, lpnmlv->iItem, context, config.search_string);

					break;
				}

				case LVN_DELETEITEM:
				{
					LPNMLISTVIEW lpnmlv;
					PITEM_LISTVIEW_CONTEXT context;
					INT listview_id;

					lpnmlv = (LPNMLISTVIEW)lparam;
					listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					if (!(listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_LOG))
						break;

					context = (PITEM_LISTVIEW_CONTEXT)lpnmlv->lParam;

					if (!context)
						break;

					_app_destroylistviewcontext (context);

					break;
				}

				case LVN_COLUMNCLICK:
				{
					LPNMLISTVIEW lpnmlv;
					INT ctrl_id;

					lpnmlv = (LPNMLISTVIEW)lparam;
					ctrl_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					_app_listviewsort (hwnd, ctrl_id, lpnmlv->iSubItem, TRUE);

					break;
				}

				case LVN_GETINFOTIP:
				{
					LPNMLVGETINFOTIP lpnmlv;
					PR_STRING string;
					INT listview_id;

					lpnmlv = (LPNMLVGETINFOTIP)lparam;
					listview_id = (INT)lpnmlv->hdr.idFrom;

					string = _app_gettooltipbylparam (hwnd, listview_id, _app_getlistviewitemcontext (hwnd, listview_id, lpnmlv->iItem));

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

					lpnmlv = (LPNMLISTVIEW)lparam;

					if (_app_ischeckboxlocked (lpnmlv->hdr.hwndFrom))
						break;

					listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					if (listview_id != IDC_APPS_PROFILE)
						break;

					if ((lpnmlv->uChanged & LVIF_STATE) != 0)
					{
						ULONG_PTR app_hash;

						app_hash = _app_getlistviewitemcontext (hwnd, listview_id, lpnmlv->iItem);

						if ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (1))
						{
							if (app_hash == config.my_hash)
							{
								if (!_r_show_confirmmessage (hwnd, L"WARNING!", L"If you disallow this, you cannot use resolve network addresses option. Continue?", NULL))
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
								if (!_r_show_confirmmessage (hwnd, L"WARNING!", L"Be careful, through service host (svchost.exe) internet traffic can let out through unexpected ways. Continue?", NULL))
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
					LPNMLISTVIEW lpnmlv;
					INT listview_id;

					lpnmlv = (LPNMLISTVIEW)lparam;
					listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					if ((lpnmlv->uChanged & LVIF_STATE) != 0)
					{
						if ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (1) || ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (2)))
						{
							if (_app_ischeckboxlocked (lpnmlv->hdr.hwndFrom))
								break;

							ULONG_PTR app_hash;
							PITEM_APP ptr_app;

							BOOLEAN is_changed;
							BOOLEAN is_enabled;

							is_changed = FALSE;
							is_enabled = (lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (2);

							if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
							{
								app_hash = _app_getlistviewparam_id (lpnmlv->lParam);
								ptr_app = _app_getappitem (app_hash);

								if (!ptr_app)
									break;

								if (ptr_app->is_enabled != is_enabled)
								{
									ptr_app->is_enabled = is_enabled;

									_app_setcheckboxlock (hwnd, listview_id, TRUE);
									_app_setappiteminfo (hwnd, listview_id, lpnmlv->iItem, ptr_app);
									_app_setcheckboxlock (hwnd, listview_id, FALSE);

									if (is_enabled)
										_app_notify_freeobject (ptr_app);

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

								_r_obj_dereference (ptr_app);
							}
							else if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM)
							{
								SIZE_T rule_idx;
								PITEM_RULE ptr_rule;

								rule_idx = _app_getlistviewparam_id (lpnmlv->lParam);
								ptr_rule = _app_getrulebyid (rule_idx);

								if (!ptr_rule)
									break;

								if (ptr_rule->is_enabled != is_enabled)
								{
									_app_setcheckboxlock (hwnd, listview_id, TRUE);

									_app_ruleenable (ptr_rule, is_enabled, TRUE);
									_app_setruleiteminfo (hwnd, listview_id, lpnmlv->iItem, ptr_rule, TRUE);

									_app_setcheckboxlock (hwnd, listview_id, FALSE);

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

								_r_obj_dereference (ptr_rule);
							}

							if (is_changed)
							{
								_app_updatelistviewbylparam (hwnd, listview_id, 0);

								_app_profile_save ();
							}
						}
					}

					break;
				}

				case LVN_GETDISPINFO:
				{
					LPNMLVDISPINFOW lpnmlv;
					INT listview_id;

					lpnmlv = (LPNMLVDISPINFOW)lparam;
					listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					_app_message_displayinfo (hwnd, listview_id, lpnmlv);

					break;
				}

				case NM_DBLCLK:
				{
					LPNMITEMACTIVATE lpnmlv;

					lpnmlv = (LPNMITEMACTIVATE)lparam;

					if (lpnmlv->iItem == -1)
						break;

					INT ctrl_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;
					INT command_id = 0;

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
					LPNMITEMACTIVATE lpnmlv;

					if (nmlp->idFrom)
					{
						lpnmlv = (LPNMITEMACTIVATE)lparam;

						_app_message_contextmenu (hwnd, lpnmlv);
					}
					else
					{
						_app_message_contextmenu_columns (hwnd, nmlp);
					}

					break;
				}
			}

			break;
		}

		case WM_SIZE:
		{
			INT listview_id;

			_r_toolbar_resize (config.hrebar, IDC_TOOLBAR);

			if (!_r_layout_resize (&layout_manager, wparam))
				break;

			_app_toolbar_resize ();

			listview_id = _app_getcurrentlistview_id (hwnd);

			if (listview_id)
				_app_listviewresize (hwnd, listview_id, FALSE);

			_app_refreshstatus (hwnd);

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
					HANDLE hengine;

					_app_logclear_ui (hwnd);

					if (config.is_neteventset)
					{
						hengine = _wfp_getenginehandle ();

						if (hengine)
							_wfp_logunsubscribe (hengine);
					}

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
					return TRUE;
				}

				case PBT_APMRESUMECRITICAL:
				case PBT_APMRESUMESUSPEND:
				{
					HANDLE hengine;

					if (config.is_neteventset)
					{
						hengine = _wfp_getenginehandle ();

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
				{
					if (!_r_config_getboolean (L"IsRefreshDevices", TRUE) || !_wfp_isfiltersinstalled () || _wfp_isfiltersapplying ())
						break;

					PDEV_BROADCAST_HDR lbhdr;
					PDEV_BROADCAST_VOLUME lpdbv;
					BOOLEAN is_appexist;

					lbhdr = (PDEV_BROADCAST_HDR)lparam;

					if (lbhdr && lbhdr->dbch_devicetype == DBT_DEVTYP_VOLUME)
					{
						lpdbv = (PDEV_BROADCAST_VOLUME)lparam;

						is_appexist = _app_isapphavedrive (FirstDriveFromMask (lpdbv->dbcv_unitmask));

						if (is_appexist)
							_app_changefilters (hwnd, TRUE, FALSE);
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

			if (notify_code == EN_CHANGE)
			{
				PR_STRING string;
				INT listview_id;

				if (ctrl_id != IDC_SEARCH)
					break;

				listview_id = _app_getcurrentlistview_id (hwnd);
				string = _r_ctrl_getstring (config.hrebar, IDC_SEARCH);

				_r_obj_swapreference (&config.search_string, string);

				_app_search_applyfilter (hwnd, listview_id, string);

				if (string)
					_r_obj_dereference (string);

				return FALSE;
			}
			else if (notify_code == 0)
			{
				if (ctrl_id >= IDX_LANGUAGE && ctrl_id <= IDX_LANGUAGE + (INT)(INT_PTR)_r_locale_getcount () + 1)
				{
					HMENU hmenu;
					HMENU hsubmenu;

					hmenu = GetMenu (hwnd);

					if (hmenu)
					{
						hsubmenu = GetSubMenu (GetSubMenu (hmenu, 2), LANG_MENU);

						if (hsubmenu)
							_r_locale_apply (hsubmenu, ctrl_id, IDX_LANGUAGE);
					}

					return FALSE;
				}
				else if (ctrl_id >= IDX_RULES_SPECIAL && ctrl_id <= (IDX_RULES_SPECIAL + (INT)(INT_PTR)_r_obj_getlistsize (rules_list)))
				{
					_app_command_idtorules (hwnd, ctrl_id);
					return FALSE;
				}
				else if (ctrl_id >= IDX_TIMER && ctrl_id <= (IDX_TIMER + (RTL_NUMBER_OF (timer_array) - 1)))
				{
					_app_command_idtotimers (hwnd, ctrl_id);
					return FALSE;
				}
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
					static COMDLG_FILTERSPEC filters[] = {
						L"Xml files (*.xml)", L"*.xml;*.xml.bak",
						L"All files (*.*)", L"*.*",
					};

					R_FILE_DIALOG file_dialog;
					PR_STRING path;

					if (_r_filedialog_initialize (&file_dialog, PR_FILEDIALOG_OPENFILE))
					{
						_r_filedialog_setfilter (&file_dialog, filters, RTL_NUMBER_OF (filters));
						_r_filedialog_setpath (&file_dialog, XML_PROFILE);

						if (_r_filedialog_show (hwnd, &file_dialog))
						{
							path = _r_filedialog_getpath (&file_dialog);

							if (path)
							{
								if (!_app_profile_load_check (path->buffer))
								{
									R_ERROR_INFO error_info = {0};
									error_info.description = path->buffer;

									_r_show_errormessage (hwnd, NULL, ERROR_INVALID_DATA, &error_info);
								}
								else
								{
									// made backup
									_r_fs_deletefile (config.profile_path_backup, TRUE);
									_app_profile_save ();

									_app_profile_load (hwnd, path->buffer); // load profile

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
					static COMDLG_FILTERSPEC filters[] = {
						L"Xml files (*.xml)", L"*.xml;*.xml.bak",
						L"All files (*.*)", L"*.*",
					};

					R_FILE_DIALOG file_dialog;
					PR_STRING path;

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
								{
									R_ERROR_INFO error_info = {0};
									error_info.description = path->buffer;

									_r_show_errormessage (hwnd, NULL, GetLastError (), &error_info);
								}

								_r_obj_dereference (path);
							}
						}

						_r_filedialog_destroy (&file_dialog);
					}

					break;
				}

				case IDM_ALWAYSONTOP_CHK:
				{
					BOOLEAN new_val = !_r_config_getboolean (L"AlwaysOnTop", FALSE);

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
						_app_listviewresize (hwnd, _app_getcurrentlistview_id (hwnd), FALSE);

					break;
				}

				case IDM_SHOWFILENAMESONLY_CHK:
				{
					INT listview_id;
					BOOLEAN new_val;

					new_val = !_r_config_getboolean (L"ShowFilenames", TRUE);

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
					_r_config_setboolean (L"ShowFilenames", new_val);

					listview_id = _app_getcurrentlistview_id (hwnd);

					_r_listview_redraw (hwnd, listview_id, -1);
					_app_listviewsort (hwnd, listview_id, -1, FALSE);

					break;
				}

				case IDM_VIEW_DETAILS:
				case IDM_VIEW_ICON:
				case IDM_VIEW_TILE:
				{
					LONG view_type;

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
					_r_config_setlong (L"ViewType", view_type);

					_app_updatelistviewbylparam (hwnd, DATA_LISTVIEW_CURRENT, PR_UPDATE_TYPE);

					break;
				}

				case IDM_SIZE_SMALL:
				case IDM_SIZE_LARGE:
				case IDM_SIZE_EXTRALARGE:
				{
					LONG icon_size;

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
						icon_size = SHIL_SMALL;
					}

					_r_menu_checkitem (GetMenu (hwnd), IDM_SIZE_SMALL, IDM_SIZE_EXTRALARGE, MF_BYCOMMAND, ctrl_id);
					_r_config_setlong (L"IconSize", icon_size);

					_app_updatelistviewbylparam (hwnd, DATA_LISTVIEW_CURRENT, PR_UPDATE_TYPE);

					break;
				}

				case IDM_ICONSISHIDDEN:
				{
					PITEM_APP ptr_app;
					SIZE_T enum_key;
					BOOLEAN new_val;

					new_val = !_r_config_getboolean (L"IsIconsHidden", FALSE);

					_r_menu_checkitem (GetMenu (hwnd), ctrl_id, 0, MF_BYCOMMAND, new_val);
					_r_config_setboolean (L"IsIconsHidden", new_val);

					if (!new_val)
					{
						_r_queuedlock_acquireshared (&lock_apps);

						enum_key = 0;

						while (_r_obj_enumhashtablepointer (apps_table, &ptr_app, NULL, &enum_key))
						{
							if (ptr_app->real_path)
								_app_queryfileinformation (ptr_app->real_path, ptr_app->app_hash, ptr_app->type, _app_getlistviewbytype_id (ptr_app->type));
						}

						_r_queuedlock_releaseshared (&lock_apps);
					}

					break;
				}

				case IDM_FONT:
				{
					_app_command_selectfont (hwnd);
					break;
				}

				case IDM_FIND:
				{
					HWND hctrl;

					hctrl = GetDlgItem (config.hrebar, IDC_SEARCH);

					if (hctrl)
					{
						SetFocus (hctrl);

						_r_edit_setselection (config.hrebar, IDC_SEARCH, -1);
					}

					break;
				}

				case IDM_REFRESH:
				{
					if (_wfp_isfiltersapplying ())
						break;

					_app_profile_load (hwnd, NULL);

					_app_changefilters (hwnd, TRUE, FALSE);

					break;
				}

				case IDM_LOADONSTARTUP_CHK:
				case IDM_STARTMINIMIZED_CHK:
				case IDM_SKIPUACWARNING_CHK:
				case IDM_CHECKUPDATES_CHK:
				case IDM_RULE_BLOCKOUTBOUND:
				case IDM_RULE_BLOCKINBOUND:
				case IDM_RULE_ALLOWLOOPBACK:
				case IDM_RULE_ALLOW6TO4:
				case IDM_USENETWORKRESOLUTION_CHK:
				case IDM_USECERTIFICATES_CHK:
				case IDM_USEREFRESHDEVICES_CHK:
				{
					_app_config_apply (hwnd, NULL, ctrl_id);
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
					HMENU hmenu;
					LONG new_state;

					hmenu = GetMenu (hwnd);

					if (!hmenu)
						break;

					if (ctrl_id >= IDM_BLOCKLIST_SPY_DISABLE && ctrl_id <= IDM_BLOCKLIST_SPY_BLOCK)
					{
						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_SPY_DISABLE, IDM_BLOCKLIST_SPY_BLOCK, MF_BYCOMMAND, ctrl_id);

						new_state = _r_calc_clamp32 (ctrl_id - IDM_BLOCKLIST_SPY_DISABLE, 0, 2);

						_r_config_setlong (L"BlocklistSpyState", new_state);

						_app_ruleblocklistset (hwnd, new_state, -1, -1, TRUE);
					}
					else if (ctrl_id >= IDM_BLOCKLIST_UPDATE_DISABLE && ctrl_id <= IDM_BLOCKLIST_UPDATE_BLOCK)
					{
						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_UPDATE_DISABLE, IDM_BLOCKLIST_UPDATE_BLOCK, MF_BYCOMMAND, ctrl_id);

						new_state = _r_calc_clamp32 (ctrl_id - IDM_BLOCKLIST_UPDATE_DISABLE, 0, 2);

						_r_config_setlong (L"BlocklistUpdateState", new_state);

						_app_ruleblocklistset (hwnd, -1, new_state, -1, TRUE);
					}
					else if (ctrl_id >= IDM_BLOCKLIST_EXTRA_DISABLE && ctrl_id <= IDM_BLOCKLIST_EXTRA_BLOCK)
					{
						_r_menu_checkitem (hmenu, IDM_BLOCKLIST_EXTRA_DISABLE, IDM_BLOCKLIST_EXTRA_BLOCK, MF_BYCOMMAND, ctrl_id);

						new_state = _r_calc_clamp32 (ctrl_id - IDM_BLOCKLIST_EXTRA_DISABLE, 0, 2);

						_r_config_setlong (L"BlocklistExtraState", new_state);

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
					HWND hnotify;
					BOOLEAN new_val;

					new_val = !_r_config_getboolean (L"IsNotificationsEnabled", TRUE);

					_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, ctrl_id, NULL, 0, new_val ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED, I_IMAGENONE);
					_r_config_setboolean (L"IsNotificationsEnabled", new_val);

					hnotify = _app_notify_getwindow ();

					if (hnotify)
						_app_notify_refresh (hnotify, TRUE);

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
					HWND hnotify;
					BOOLEAN new_val;

					new_val = !_r_config_getboolean (L"IsNotificationsOnTray", FALSE);

					_r_config_setboolean (L"IsNotificationsOnTray", new_val);

					hnotify = _app_notify_getwindow ();

					if (hnotify)
						_app_notify_setposition (hnotify, TRUE);

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
					BOOLEAN is_filtersinstalled;

					if (_wfp_isfiltersapplying ())
						break;

					is_filtersinstalled = !(_wfp_isfiltersinstalled () != INSTALL_DISABLED);

					if (_app_installmessage (hwnd, is_filtersinstalled))
						_app_changefilters (hwnd, is_filtersinstalled, TRUE);

					break;
				}

				case IDM_ADD_FILE:
				{
					static COMDLG_FILTERSPEC filters[] = {
						L"Executable files (*.exe)", L"*.exe",
						L"All files (*.*)", L"*.*",
					};

					R_FILE_DIALOG file_dialog;
					PR_STRING path;
					ULONG_PTR app_hash;

					if (_r_filedialog_initialize (&file_dialog, PR_FILEDIALOG_OPENFILE))
					{
						_r_filedialog_setfilter (&file_dialog, filters, RTL_NUMBER_OF (filters));

						if (_r_filedialog_show (hwnd, &file_dialog))
						{
							path = _r_filedialog_getpath (&file_dialog);

							if (path)
							{
								app_hash = _app_addapplication (hwnd, DATA_UNKNOWN, &path->sr, NULL, NULL);

								if (app_hash)
								{
									_app_setlistviewbylparam (hwnd, app_hash, PR_SETITEM_UPDATE, TRUE);
									_app_showitembylparam (hwnd, app_hash, TRUE);

									_app_profile_save ();
								}
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
					ULONG_PTR hash_code;

					INT listview_id = _app_getcurrentlistview_id (hwnd);
					INT item_id = -1;

					if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
					{
						PITEM_APP ptr_app;

						while ((item_id = _r_listview_getnextselected (hwnd, listview_id, item_id)) != -1)
						{
							hash_code = _app_getlistviewitemcontext (hwnd, listview_id, item_id);
							ptr_app = _app_getappitem (hash_code);

							if (ptr_app)
							{
								if (ptr_app->real_path)
								{
									if (_app_isappvalidpath (&ptr_app->real_path->sr))
										_r_shell_showfile (ptr_app->real_path->buffer);
								}

								_r_obj_dereference (ptr_app);
							}
						}
					}
					else if (listview_id == IDC_NETWORK)
					{
						PITEM_NETWORK ptr_network;

						while ((item_id = _r_listview_getnextselected (hwnd, listview_id, item_id)) != -1)
						{
							hash_code = _app_getlistviewitemcontext (hwnd, listview_id, item_id);
							ptr_network = _app_getnetworkitem (hash_code);

							if (ptr_network)
							{
								if (ptr_network->path)
								{
									if (_app_isappvalidpath (&ptr_network->path->sr))
										_r_shell_showfile (ptr_network->path->buffer);
								}

								_r_obj_dereference (ptr_network);
							}
						}
					}
					else if (listview_id == IDC_LOG)
					{
						PITEM_LOG ptr_log;

						while ((item_id = _r_listview_getnextselected (hwnd, listview_id, item_id)) != -1)
						{
							hash_code = _app_getlistviewitemcontext (hwnd, listview_id, item_id);
							ptr_log = _app_getlogitem (hash_code);

							if (ptr_log)
							{
								if (ptr_log->path)
								{
									if (_app_isappvalidpath (&ptr_log->path->sr))
										_r_shell_showfile (ptr_log->path->buffer);
								}

								_r_obj_dereference (ptr_log);
							}
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
					INT listview_id;

					listview_id = _app_getcurrentlistview_id (hwnd);

					if (listview_id)
					{
						if (GetFocus () == GetDlgItem (hwnd, listview_id))
							_r_listview_setitemstate (hwnd, listview_id, -1, LVIS_SELECTED, LVIS_SELECTED);
					}

					break;
				}

				case IDM_ZOOM:
				{
					ShowWindow (hwnd, IsZoomed (hwnd) ? SW_RESTORE : SW_MAXIMIZE);
					break;
				}

				case IDM_TAB_NEXT:
				case IDM_TAB_PREV:
				{
					INT tabs_count;
					INT item_id;

					tabs_count = _r_tab_getitemcount (hwnd, IDC_TAB);

					if (!tabs_count)
						break;

					item_id = _r_tab_getcurrentitem (hwnd, IDC_TAB);

					if (item_id == -1)
						break;

					if (ctrl_id == IDM_TAB_NEXT)
					{
						item_id += 1;

						if (item_id >= tabs_count)
							item_id = 0;
					}
					else
					{
						item_id -= 1;

						if (item_id == -1)
							item_id = tabs_count - 1;
					}

					_r_tab_selectitem (hwnd, IDC_TAB, item_id);

					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

BOOLEAN _app_parseargs (_In_ LPCWSTR cmdline)
{
	WCHAR arguments_mutex_name[32];
	HANDLE arguments_mutex;

	_r_str_printf (arguments_mutex_name, RTL_NUMBER_OF (arguments_mutex_name), L"%sCmd", _r_app_getnameshort ());

	// parse arguments
	if (_r_mutex_isexists (arguments_mutex_name))
		return TRUE;

	BOOLEAN is_install = FALSE;
	BOOLEAN is_uninstall = FALSE;
	BOOLEAN is_silent = FALSE;
	BOOLEAN is_temporary = FALSE;
	BOOLEAN result = FALSE;

	_r_mutex_create (arguments_mutex_name, &arguments_mutex);

	if (_r_sys_getopt (cmdline, L"help", NULL))
	{
		_r_show_message (NULL, MB_OK | MB_ICONINFORMATION, _r_app_getname (), L"Available options:", L"\"simplewall.exe -install\" - enable filtering.\r\n\"simplewall.exe -install -temp\" - enable filtering until reboot.\r\n\"simplewall.exe -install -silent\" - enable filtering without prompt.\r\n\"simplewall.exe -uninstall\" - remove all installed filters.\r\n\"simplewall.exe -help\" - show this message.");

		result = TRUE;

		goto CleanupExit;
	}
	else if (_r_sys_getopt (cmdline, L"install", NULL))
	{
		is_install = TRUE;
	}
	else if (_r_sys_getopt (cmdline, L"uninstall", NULL))
	{
		is_uninstall = TRUE;
	}

	if (is_install)
	{
		if (_r_sys_getopt (cmdline, L"silent", NULL))
		{
			is_silent = TRUE;
		}

		if (_r_sys_getopt (cmdline, L"temp", NULL))
		{
			is_temporary = TRUE;
		}
	}

	if (is_install || is_uninstall)
	{

		// already elevated
		//if (_r_sys_iselevated ())
		{
			HANDLE hengine;

			_app_initialize ();

			hengine = _wfp_getenginehandle ();

			if (hengine)
			{
				if (is_install)
				{
					if (is_silent || _app_installmessage (NULL, TRUE))
					{
						if (is_temporary)
							config.is_filterstemporary = TRUE;

						_app_profile_load (NULL, NULL);

						if (_wfp_initialize (hengine))
							_wfp_installfilters (hengine);

						_wfp_uninitialize (hengine, FALSE);
					}
				}
				else if (is_uninstall)
				{
					if (((_wfp_isfiltersinstalled () != INSTALL_DISABLED) && _app_installmessage (NULL, FALSE)))
					{
						_wfp_destroyfilters (hengine);
						_wfp_uninitialize (hengine, TRUE);
					}
				}
			}
		}

		result = TRUE;
	}

CleanupExit:

	_r_mutex_destroy (&arguments_mutex);

	return result;
}

INT APIENTRY wWinMain (_In_ HINSTANCE hinst, _In_opt_ HINSTANCE prev_hinst, _In_ LPWSTR cmdline, _In_ INT show_cmd)
{
	MSG msg;
	HWND hwnd;
	HACCEL haccel;

	if (_r_app_initialize ())
	{
		if (_app_parseargs (cmdline))
			return ERROR_SUCCESS;

		hwnd = _r_app_createwindow (IDD_MAIN, IDI_MAIN, &DlgProc);

		if (hwnd)
		{
			haccel = LoadAccelerators (_r_sys_getimagebase (), MAKEINTRESOURCE (IDA_MAIN));

			if (haccel)
			{
				BOOL result;
				BOOL is_proceed;

				while (TRUE)
				{
					is_proceed = FALSE;
					result = GetMessage (&msg, NULL, 0, 0);

					if (!result || result == -1)
						break;

					hwnd = GetActiveWindow ();

					if (msg.hwnd == hwnd || IsChild (hwnd, msg.hwnd))
					{
						if (TranslateAccelerator (hwnd, haccel, &msg))
							is_proceed = TRUE;
					}

					if (IsDialogMessage (hwnd, &msg))
					{
						is_proceed = TRUE;
					}

					if (!is_proceed)
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
