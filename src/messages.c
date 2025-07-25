// simplewall
// Copyright (c) 2021-2025 Henry++

#include "global.h"

VOID _app_message_initialize (
	_In_ HWND hwnd
)
{
	ENUM_INSTALL_TYPE install_type;
	HMENU hmenu;
	LONG icon_size;
	LONG view_type;
	UINT menu_id;
	BOOLEAN is_enabled;

	install_type = _wfp_getinstalltype ();

	_r_tray_create (hwnd, &GUID_TrayIcon, RM_TRAYICON, NULL, NULL, FALSE);

	_app_settrayicon (hwnd, install_type);

	hmenu = GetMenu (hwnd);

	if (hmenu)
	{
		if (_r_config_getboolean (L"IsInternalRulesDisabled", FALSE, NULL))
			_r_menu_enableitem (hmenu, 4, FALSE, FALSE);

		_r_menu_checkitem (hmenu, IDM_ALWAYSONTOP_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"AlwaysOnTop", FALSE, NULL));
		_r_menu_checkitem (hmenu, IDM_AUTOSIZECOLUMNS_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"AutoSizeColumns", TRUE, NULL));
		_r_menu_checkitem (hmenu, IDM_SHOWFILENAMESONLY_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"ShowFilenames", TRUE, NULL));
		_r_menu_checkitem (hmenu, IDM_SHOWSEARCHBAR_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsShowSearchBar", TRUE, NULL));

		view_type = _r_calc_clamp (_r_config_getlong (L"ViewType", LV_VIEW_DETAILS, NULL), LV_VIEW_ICON, LV_VIEW_MAX);

		if (view_type == LV_VIEW_ICON)
		{
			menu_id = IDM_VIEW_ICON;
		}
		else if (view_type == LV_VIEW_TILE)
		{
			menu_id = IDM_VIEW_TILE;
		}
		else
		{
			menu_id = IDM_VIEW_DETAILS;
		}

		_r_menu_checkitem (hmenu, IDM_VIEW_DETAILS, IDM_VIEW_TILE, MF_BYCOMMAND, menu_id);

		icon_size = _r_calc_clamp (_r_config_getlong (L"IconSize", SHIL_SMALL, NULL), SHIL_LARGE, SHIL_LAST);

		if (icon_size == SHIL_EXTRALARGE)
		{
			menu_id = IDM_SIZE_EXTRALARGE;
		}
		else if (icon_size == SHIL_LARGE)
		{
			menu_id = IDM_SIZE_LARGE;
		}
		else
		{
			menu_id = IDM_SIZE_SMALL;
		}

		_r_menu_checkitem (hmenu, IDM_SIZE_SMALL, IDM_SIZE_EXTRALARGE, MF_BYCOMMAND, menu_id);
		_r_menu_checkitem (hmenu, IDM_ICONSISHIDDEN, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsIconsHidden", FALSE, NULL));
		_r_menu_checkitem (hmenu, IDM_USEDARKTHEME_CHK, 0, MF_BYCOMMAND, _r_theme_isenabled ());
		_r_menu_checkitem (hmenu, IDM_LOADONSTARTUP_CHK, 0, MF_BYCOMMAND, _r_autorun_isenabled ());
		_r_menu_checkitem (hmenu, IDM_STARTMINIMIZED_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsStartMinimized", FALSE, NULL));
		_r_menu_checkitem (hmenu, IDM_SKIPUACWARNING_CHK, 0, MF_BYCOMMAND, _r_skipuac_isenabled ());
		_r_menu_checkitem (hmenu, IDM_CHECKUPDATES_CHK, 0, MF_BYCOMMAND, _r_update_isenabled (FALSE));
		_r_menu_checkitem (hmenu, IDM_RULE_BLOCKOUTBOUND, 0, MF_BYCOMMAND, _r_config_getboolean (L"BlockOutboundConnections", TRUE, NULL));
		_r_menu_checkitem (hmenu, IDM_RULE_BLOCKINBOUND, 0, MF_BYCOMMAND, _r_config_getboolean (L"BlockInboundConnections", TRUE, NULL));
		_r_menu_checkitem (hmenu, IDM_RULE_ALLOWLOOPBACK, 0, MF_BYCOMMAND, _r_config_getboolean (L"AllowLoopbackConnections", TRUE, NULL));
		_r_menu_checkitem (hmenu, IDM_RULE_ALLOW6TO4, 0, MF_BYCOMMAND, _r_config_getboolean (L"AllowIPv6", TRUE, NULL));
		_r_menu_checkitem (hmenu, IDM_RULE_ALLOWWINDOWSUPDATE, 0, MF_BYCOMMAND, _app_wufixenabled ());

		if (!_r_sys_isosversiongreaterorequal (WINDOWS_10))
			_r_menu_enableitem (hmenu, IDM_RULE_ALLOWWINDOWSUPDATE, FALSE, FALSE);

		_r_menu_checkitem (hmenu, IDM_PROFILETYPE_PLAIN, IDM_PROFILETYPE_ENCRYPTED, MF_BYCOMMAND, IDM_PROFILETYPE_PLAIN + _r_calc_clamp (_r_config_getlong (L"ProfileType", 0, NULL), 0, 2));

		is_enabled = _r_config_getboolean (L"IsHashesEnabled", FALSE, NULL);

		_r_menu_checkitem (hmenu, IDM_USECERTIFICATES_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsCertificatesEnabled", TRUE, NULL));
		_r_menu_checkitem (hmenu, IDM_KEEPUNUSED_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsKeepUnusedApps", TRUE, NULL));
		_r_menu_checkitem (hmenu, IDM_USEHASHES_CHK, 0, MF_BYCOMMAND, is_enabled);
		_r_menu_checkitem (hmenu, IDM_USENETWORKRESOLUTION_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsNetworkResolutionsEnabled", FALSE, NULL));
		_r_menu_checkitem (hmenu, IDM_USEAPPMONITOR_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsEnableAppMonitor", FALSE, NULL));

		_r_menu_enableitem (hmenu, IDM_USEAPPMONITOR_CHK, FALSE, is_enabled);

		_r_menu_checkitem (
			hmenu,
			IDM_BLOCKLIST_SPY_DISABLE,
			IDM_BLOCKLIST_SPY_BLOCK,
			MF_BYCOMMAND,
			IDM_BLOCKLIST_SPY_DISABLE + _r_calc_clamp (_r_config_getlong (L"BlocklistSpyState", 2, NULL), 0, 2)
		);

		_r_menu_checkitem (
			hmenu,
			IDM_BLOCKLIST_UPDATE_DISABLE,
			IDM_BLOCKLIST_UPDATE_BLOCK,
			MF_BYCOMMAND,
			IDM_BLOCKLIST_UPDATE_DISABLE + _r_calc_clamp (_r_config_getlong (L"BlocklistUpdateState", 0, NULL), 0, 2)
		);

		_r_menu_checkitem (
			hmenu,
			IDM_BLOCKLIST_EXTRA_DISABLE,
			IDM_BLOCKLIST_EXTRA_BLOCK,
			MF_BYCOMMAND,
			IDM_BLOCKLIST_EXTRA_DISABLE + _r_calc_clamp (_r_config_getlong (L"BlocklistExtraState", 0, NULL), 0, 2)
		);
	}

	_r_toolbar_setbutton (
		config.hrebar,
		IDC_TOOLBAR,
		IDM_TRAY_ENABLENOTIFICATIONS_CHK,
		NULL,
		0,
		_r_config_getboolean (L"IsNotificationsEnabled", TRUE, NULL) ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED,
		I_IMAGENONE
	);

	_r_toolbar_setbutton (
		config.hrebar,
		IDC_TOOLBAR,
		IDM_TRAY_ENABLELOG_CHK,
		NULL,
		0,
		_r_config_getboolean (L"IsLogEnabled", FALSE, NULL) ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED,
		I_IMAGENONE
	);

	_r_toolbar_setbutton (
		config.hrebar,
		IDC_TOOLBAR,
		IDM_TRAY_ENABLEUILOG_CHK,
		NULL,
		0,
		_r_config_getboolean (L"IsLogUiEnabled", FALSE, NULL) ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED,
		I_IMAGENONE
	);
}

VOID _app_message_localize (
	_In_ HWND hwnd
)
{
	PITEM_TAB_CONTEXT tab_context;
	PR_STRING localized_string = NULL;
	LPCWSTR recommended_string;
	HMENU hmenu;
	HMENU hsubmenu;
	LONG dpi_value;

	hmenu = GetMenu (hwnd);

	dpi_value = _r_dc_getwindowdpi (hwnd);

	if (hmenu)
	{
		_r_menu_setitemtext (hmenu, 0, TRUE, _r_locale_getstring (IDS_FILE));
		_r_menu_setitemtext (hmenu, 1, TRUE, _r_locale_getstring (IDS_EDIT));
		_r_menu_setitemtext (hmenu, 2, TRUE, _r_locale_getstring (IDS_VIEW));
		_r_menu_setitemtext (hmenu, 3, TRUE, _r_locale_getstring (IDS_SETTINGS));
		_r_menu_setitemtext (hmenu, 4, TRUE, _r_locale_getstring (IDS_TRAY_BLOCKLIST_RULES));
		_r_menu_setitemtext (hmenu, 5, TRUE, _r_locale_getstring (IDS_HELP));

		// file submenu
		_r_menu_setitemtextformat (
			hmenu,
			IDM_SETTINGS,
			FALSE,
			L"%s...\tF2",
			_r_locale_getstring (IDS_SETTINGS)
		);

		_r_menu_setitemtextformat (
			hmenu,
			IDM_ADD_FILE,
			FALSE,
			L"%s...",
			_r_locale_getstring (IDS_ADD_FILE)
		);

		_r_menu_setitemtextformat (
			hmenu,
			IDM_IMPORT,
			FALSE,
			L"%s...\tCtrl+O",
			_r_locale_getstring (IDS_IMPORT)
		);

		_r_menu_setitemtextformat (
			hmenu,
			IDM_EXPORT,
			FALSE,
			L"%s...\tCtrl+S",
			_r_locale_getstring (IDS_EXPORT)
		);

		_r_menu_setitemtextformat (
			hmenu,
			IDM_EXIT,
			FALSE,
			_r_locale_getstring (IDS_EXIT)
		);

		// edit submenu
		_r_menu_setitemtextformat (
			hmenu,
			IDM_PURGE_UNUSED,
			FALSE,
			L"%s\tCtrl+Shift+X",
			_r_locale_getstring (IDS_PURGE_UNUSED)
		);

		_r_menu_setitemtextformat (
			hmenu,
			IDM_PURGE_TIMERS,
			FALSE,
			L"%s\tCtrl+Shift+T",
			_r_locale_getstring (IDS_PURGE_TIMERS)
		);

		_r_menu_setitemtextformat (
			hmenu,
			IDM_LOGCLEAR,
			FALSE,
			L"%s\tCtrl+X",
			_r_locale_getstring (IDS_LOGCLEAR)
		);

		_r_menu_setitemtextformat (
			hmenu,
			IDM_REFRESH,
			FALSE,
			L"%s\tF5",
			_r_locale_getstring (IDS_REFRESH)
		);

		// view submenu
		_r_menu_setitemtext (hmenu, IDM_ALWAYSONTOP_CHK, FALSE, _r_locale_getstring (IDS_ALWAYSONTOP_CHK));
		_r_menu_setitemtext (hmenu, IDM_AUTOSIZECOLUMNS_CHK, FALSE, _r_locale_getstring (IDS_AUTOSIZECOLUMNS_CHK));
		_r_menu_setitemtext (hmenu, IDM_SHOWFILENAMESONLY_CHK, FALSE, _r_locale_getstring (IDS_SHOWFILENAMESONLY_CHK));
		_r_menu_setitemtext (hmenu, IDM_SHOWSEARCHBAR_CHK, FALSE, _r_locale_getstring (IDS_SHOWSEARCHBAR_CHK));

		_r_menu_setitemtext (hmenu, IDM_SIZE_SMALL, FALSE, _r_locale_getstring (IDS_ICONSSMALL));
		_r_menu_setitemtext (hmenu, IDM_SIZE_LARGE, FALSE, _r_locale_getstring (IDS_ICONSLARGE));
		_r_menu_setitemtext (hmenu, IDM_SIZE_EXTRALARGE, FALSE, _r_locale_getstring (IDS_ICONSEXTRALARGE));

		_r_menu_setitemtext (hmenu, IDM_VIEW_DETAILS, FALSE, _r_locale_getstring (IDS_VIEW_DETAILS));
		_r_menu_setitemtext (hmenu, IDM_VIEW_ICON, FALSE, _r_locale_getstring (IDS_VIEW_ICON));
		_r_menu_setitemtext (hmenu, IDM_VIEW_TILE, FALSE, _r_locale_getstring (IDS_VIEW_TILE));

		_r_menu_setitemtext (hmenu, IDM_ICONSISHIDDEN, FALSE, _r_locale_getstring (IDS_ICONSISHIDDEN));

		_r_menu_setitemtext (hmenu, IDM_USEDARKTHEME_CHK, FALSE, _r_locale_getstring (IDS_USEDARKTHEME));

		hsubmenu = GetSubMenu (hmenu, 2);

		if (hsubmenu)
		{
			_r_menu_setitemtext (hsubmenu, ICONS_MENU, TRUE, _r_locale_getstring (IDS_ICONS));

			_r_menu_setitemtextformat (
				hsubmenu,
				LANG_MENU,
				TRUE,
				L"%s (Language)",
				_r_locale_getstring (IDS_LANGUAGE)
			);

			_r_locale_enum (hsubmenu, LANG_MENU, IDX_LANGUAGE); // enum localizations
		}

		_r_menu_setitemtextformat (hmenu, IDM_FONT, FALSE, L"%s...", _r_locale_getstring (IDS_FONT));

		// settings submenu
		_r_menu_setitemtext (hmenu, IDM_LOADONSTARTUP_CHK, FALSE, _r_locale_getstring (IDS_LOADONSTARTUP_CHK));
		_r_menu_setitemtext (hmenu, IDM_STARTMINIMIZED_CHK, FALSE, _r_locale_getstring (IDS_STARTMINIMIZED_CHK));
		_r_menu_setitemtext (hmenu, IDM_SKIPUACWARNING_CHK, FALSE, _r_locale_getstring (IDS_SKIPUACWARNING_CHK));
		_r_menu_setitemtext (hmenu, IDM_CHECKUPDATES_CHK, FALSE, _r_locale_getstring (IDS_CHECKUPDATES_CHK));

		_r_menu_setitemtext (hmenu, IDM_RULE_BLOCKOUTBOUND, FALSE, _r_locale_getstring (IDS_RULE_BLOCKOUTBOUND));
		_r_menu_setitemtext (hmenu, IDM_RULE_BLOCKINBOUND, FALSE, _r_locale_getstring (IDS_RULE_BLOCKINBOUND));
		_r_menu_setitemtext (hmenu, IDM_RULE_ALLOWLOOPBACK, FALSE, _r_locale_getstring (IDS_RULE_ALLOWLOOPBACK));
		_r_menu_setitemtext (hmenu, IDM_RULE_ALLOW6TO4, FALSE, _r_locale_getstring (IDS_RULE_ALLOW6TO4));

		_r_menu_setitemtext (hmenu, IDM_RULE_ALLOWWINDOWSUPDATE, FALSE, _r_locale_getstring (IDS_RULE_ALLOWWINDOWSUPDATE));

		_r_menu_setitemtext (hmenu, IDM_PROFILETYPE_PLAIN, FALSE, _r_locale_getstring (IDS_PROFILE_TYPE_PLAIN));
		_r_menu_setitemtext (hmenu, IDM_PROFILETYPE_COMPRESSED, FALSE, _r_locale_getstring (IDS_PROFILE_TYPE_COMPRESSED));
		_r_menu_setitemtext (hmenu, IDM_PROFILETYPE_ENCRYPTED, FALSE, _r_locale_getstring (IDS_PROFILE_TYPE_ENCRYPTED));

		_r_menu_setitemtext (hmenu, IDM_USENETWORKRESOLUTION_CHK, FALSE, _r_locale_getstring (IDS_USENETWORKRESOLUTION_CHK));
		_r_menu_setitemtext (hmenu, IDM_USECERTIFICATES_CHK, FALSE, _r_locale_getstring (IDS_USECERTIFICATES_CHK));
		_r_menu_setitemtext (hmenu, IDM_KEEPUNUSED_CHK, FALSE, _r_locale_getstring (IDS_KEEPUNUSED_CHK));
		_r_menu_setitemtext (hmenu, IDM_USEHASHES_CHK, FALSE, _r_locale_getstring (IDS_USEHASHES_CHK));
		_r_menu_setitemtext (hmenu, IDM_USEAPPMONITOR_CHK, FALSE, _r_locale_getstring (IDS_USEAPPMONITOR_CHK));

		hsubmenu = GetSubMenu (hmenu, 3);

		if (hsubmenu)
		{
			_r_menu_setitemtext (hsubmenu, 5, TRUE, _r_locale_getstring (IDS_TRAY_RULES));
			_r_menu_setitemtext (hsubmenu, 6, TRUE, _r_locale_getstring (IDS_PROFILE_TYPE));
		}

		recommended_string = _r_locale_getstring (IDS_RECOMMENDED);

		// blocklist submenu
		_r_menu_setitemtext (hmenu, IDM_BLOCKLIST_SPY_DISABLE, FALSE, _r_locale_getstring (IDS_DISABLE));
		_r_menu_setitemtext (hmenu, IDM_BLOCKLIST_SPY_ALLOW, FALSE, _r_locale_getstring (IDS_ACTION_ALLOW));
		_r_menu_setitemtextformat (hmenu, IDM_BLOCKLIST_SPY_BLOCK, FALSE, L"%s (%s)", _r_locale_getstring (IDS_ACTION_BLOCK), recommended_string);

		_r_menu_setitemtextformat (hmenu, IDM_BLOCKLIST_UPDATE_DISABLE, FALSE, L"%s (%s)", _r_locale_getstring (IDS_DISABLE), recommended_string);
		_r_menu_setitemtext (hmenu, IDM_BLOCKLIST_UPDATE_ALLOW, FALSE, _r_locale_getstring (IDS_ACTION_ALLOW));
		_r_menu_setitemtext (hmenu, IDM_BLOCKLIST_UPDATE_BLOCK, FALSE, _r_locale_getstring (IDS_ACTION_BLOCK));

		_r_menu_setitemtextformat (hmenu, IDM_BLOCKLIST_EXTRA_DISABLE, FALSE, L"%s (%s)", _r_locale_getstring (IDS_DISABLE), recommended_string);
		_r_menu_setitemtext (hmenu, IDM_BLOCKLIST_EXTRA_ALLOW, FALSE, _r_locale_getstring (IDS_ACTION_ALLOW));
		_r_menu_setitemtext (hmenu, IDM_BLOCKLIST_EXTRA_BLOCK, FALSE, _r_locale_getstring (IDS_ACTION_BLOCK));

		hsubmenu = GetSubMenu (hmenu, 4);

		if (hsubmenu)
		{
			_r_menu_setitemtext (hsubmenu, 0, TRUE, _r_locale_getstring (IDS_BLOCKLIST_SPY));
			_r_menu_setitemtext (hsubmenu, 1, TRUE, _r_locale_getstring (IDS_BLOCKLIST_UPDATE));
			_r_menu_setitemtext (hsubmenu, 2, TRUE, _r_locale_getstring (IDS_BLOCKLIST_EXTRA));
		}

		// help submenu
		_r_menu_setitemtext (hmenu, IDM_WEBSITE, FALSE, _r_locale_getstring (IDS_WEBSITE));
		_r_menu_setitemtext (hmenu, IDM_CHECKUPDATES, FALSE, _r_locale_getstring (IDS_CHECKUPDATES));
		_r_menu_setitemtextformat (hmenu, IDM_ABOUT, FALSE, L"%s\tF1", _r_locale_getstring (IDS_ABOUT));
	}

	// localize toolbar
	_app_setinterfacestate (hwnd, dpi_value);

	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_REFRESH, _r_locale_getstring (IDS_REFRESH), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, 0, I_IMAGENONE);
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_SETTINGS, _r_locale_getstring (IDS_SETTINGS), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, 0, I_IMAGENONE);

	_r_obj_movereference ((PVOID_PTR)&localized_string, _r_format_string (L"%s...", _r_locale_getstring (IDS_OPENRULESEDITOR)));
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_OPENRULESEDITOR, localized_string->buffer, BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLENOTIFICATIONS_CHK, _r_locale_getstring (IDS_ENABLENOTIFICATIONS_CHK), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLELOG_CHK, _r_locale_getstring (IDS_ENABLELOG_CHK), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_obj_movereference ((PVOID_PTR)&localized_string, _r_format_string (L"%s (%s)", _r_locale_getstring (IDS_ENABLEUILOG_CHK), _r_locale_getstring (IDS_SESSION_ONLY)));
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLEUILOG_CHK, localized_string->buffer, BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_obj_movereference ((PVOID_PTR)&localized_string, _r_format_string (L"%s (Ctrl+I)", _r_locale_getstring (IDS_LOGSHOW)));
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_LOGSHOW, localized_string->buffer, BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_obj_movereference ((PVOID_PTR)&localized_string, _r_format_string (L"%s (Ctrl+X)", _r_locale_getstring (IDS_LOGCLEAR)));
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_LOGCLEAR, localized_string->buffer, BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_DONATE, _r_locale_getstring (IDS_DONATE), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_obj_movereference ((PVOID_PTR)&localized_string, _r_format_string (L"%s... (Ctrl+F)", _r_locale_getstring (IDS_FIND)));
	_r_edit_setcuebanner (config.hrebar, IDC_SEARCH, localized_string->buffer);

	// set rebar size
	_app_toolbar_resize (hwnd, dpi_value);

	// localize tabs
	for (INT i = 0; i < _r_tab_getitemcount (hwnd, IDC_TAB); i++)
	{
		tab_context = _app_listview_getcontext (hwnd, i);

		if (!tab_context)
			continue;

		_r_tab_setitem (hwnd, IDC_TAB, i, _r_locale_getstring (tab_context->locale_id), I_DEFAULT, I_DEFAULT);

		switch (tab_context->listview_id)
		{
			case IDC_APPS_PROFILE:
			case IDC_APPS_SERVICE:
			case IDC_APPS_UWP:
			{
				_r_listview_setcolumn (hwnd, tab_context->listview_id, 0, _r_locale_getstring (IDS_NAME), 0);
				_r_listview_setcolumn (hwnd, tab_context->listview_id, 1, _r_locale_getstring (IDS_ADDED), 0);

				break;
			}

			case IDC_RULES_BLOCKLIST:
			case IDC_RULES_SYSTEM:
			case IDC_RULES_CUSTOM:
			{
				_r_listview_setcolumn (hwnd, tab_context->listview_id, 0, _r_locale_getstring (IDS_NAME), 0);
				_r_listview_setcolumn (hwnd, tab_context->listview_id, 1, _r_locale_getstring (IDS_PROTOCOL), 0);
				_r_listview_setcolumn (hwnd, tab_context->listview_id, 2, _r_locale_getstring (IDS_DIRECTION), 0);

				break;
			}

			case IDC_NETWORK:
			{
				_r_listview_setcolumn (hwnd, tab_context->listview_id, 0, _r_locale_getstring (IDS_NAME), 0);

				_r_obj_movereference ((PVOID_PTR)&localized_string, _r_format_string (L"%s (%s)", _r_locale_getstring (IDS_ADDRESS), _r_locale_getstring (IDS_DIRECTION_LOCAL)));
				_r_listview_setcolumn (hwnd, tab_context->listview_id, 1, localized_string->buffer, 0);

				_r_obj_movereference ((PVOID_PTR)&localized_string, _r_format_string (L"%s (%s)", _r_locale_getstring (IDS_HOST), _r_locale_getstring (IDS_DIRECTION_LOCAL)));
				_r_listview_setcolumn (hwnd, tab_context->listview_id, 2, localized_string->buffer, 0);

				_r_obj_movereference ((PVOID_PTR)&localized_string, _r_format_string (L"%s (%s)", _r_locale_getstring (IDS_PORT), _r_locale_getstring (IDS_DIRECTION_LOCAL)));
				_r_listview_setcolumn (hwnd, tab_context->listview_id, 3, localized_string->buffer, 0);

				_r_obj_movereference ((PVOID_PTR)&localized_string, _r_format_string (L"%s (%s)", _r_locale_getstring (IDS_ADDRESS), _r_locale_getstring (IDS_DIRECTION_REMOTE)));
				_r_listview_setcolumn (hwnd, tab_context->listview_id, 4, localized_string->buffer, 0);

				_r_obj_movereference ((PVOID_PTR)&localized_string, _r_format_string (L"%s (%s)", _r_locale_getstring (IDS_HOST), _r_locale_getstring (IDS_DIRECTION_REMOTE)));
				_r_listview_setcolumn (hwnd, tab_context->listview_id, 5, localized_string->buffer, 0);

				_r_obj_movereference ((PVOID_PTR)&localized_string, _r_format_string (L"%s (%s)", _r_locale_getstring (IDS_PORT), _r_locale_getstring (IDS_DIRECTION_REMOTE)));
				_r_listview_setcolumn (hwnd, tab_context->listview_id, 6, localized_string->buffer, 0);

				_r_listview_setcolumn (hwnd, tab_context->listview_id, 7, _r_locale_getstring (IDS_PROTOCOL), 0);
				_r_listview_setcolumn (hwnd, tab_context->listview_id, 8, _r_locale_getstring (IDS_STATE), 0);

				break;
			}

			case IDC_LOG:
			{
				_r_listview_setcolumn (hwnd, tab_context->listview_id, 0, _r_locale_getstring (IDS_NAME), 0);
				_r_listview_setcolumn (hwnd, tab_context->listview_id, 1, _r_locale_getstring (IDS_DATE), 0);

				_r_obj_movereference ((PVOID_PTR)&localized_string, _r_format_string (L"%s (%s)", _r_locale_getstring (IDS_ADDRESS), _r_locale_getstring (IDS_DIRECTION_LOCAL)));
				_r_listview_setcolumn (hwnd, tab_context->listview_id, 2, localized_string->buffer, 0);

				_r_obj_movereference ((PVOID_PTR)&localized_string, _r_format_string (L"%s (%s)", _r_locale_getstring (IDS_HOST), _r_locale_getstring (IDS_DIRECTION_LOCAL)));
				_r_listview_setcolumn (hwnd, tab_context->listview_id, 3, localized_string->buffer, 0);

				_r_obj_movereference ((PVOID_PTR)&localized_string, _r_format_string (L"%s (%s)", _r_locale_getstring (IDS_PORT), _r_locale_getstring (IDS_DIRECTION_LOCAL)));
				_r_listview_setcolumn (hwnd, tab_context->listview_id, 4, localized_string->buffer, 0);

				_r_obj_movereference ((PVOID_PTR)&localized_string, _r_format_string (L"%s (%s)", _r_locale_getstring (IDS_ADDRESS), _r_locale_getstring (IDS_DIRECTION_REMOTE)));
				_r_listview_setcolumn (hwnd, tab_context->listview_id, 5, localized_string->buffer, 0);

				_r_obj_movereference ((PVOID_PTR)&localized_string, _r_format_string (L"%s (%s)", _r_locale_getstring (IDS_HOST), _r_locale_getstring (IDS_DIRECTION_REMOTE)));
				_r_listview_setcolumn (hwnd, tab_context->listview_id, 6, localized_string->buffer, 0);

				_r_obj_movereference ((PVOID_PTR)&localized_string, _r_format_string (L"%s (%s)", _r_locale_getstring (IDS_PORT), _r_locale_getstring (IDS_DIRECTION_REMOTE)));
				_r_listview_setcolumn (hwnd, tab_context->listview_id, 7, localized_string->buffer, 0);

				_r_listview_setcolumn (hwnd, tab_context->listview_id, 8, _r_locale_getstring (IDS_PROTOCOL), 0);
				_r_listview_setcolumn (hwnd, tab_context->listview_id, 9, _r_locale_getstring (IDS_DIRECTION), 0);
				_r_listview_setcolumn (hwnd, tab_context->listview_id, 10, _r_locale_getstring (IDS_FILTER), 0);

				break;
			}
		}
	}

	_app_listview_updateby_id (hwnd, DATA_LISTVIEW_CURRENT, PR_UPDATE_TYPE | PR_UPDATE_FORCE);

	if (localized_string)
		_r_obj_dereference (localized_string);
}

VOID _app_message_uninitialize (
	_In_ HWND hwnd
)
{
	_r_tray_destroy (hwnd, &GUID_TrayIcon);
}

VOID _app_generate_appmenu (
	_In_ HMENU hmenu,
	_In_ HMENU hsubmenu_rules,
	_In_ HMENU hsubmenu_timers,
	_In_ ULONG app_hash
)
{
	_r_menu_additem (hmenu, 0, NULL);

	// show rules
	if (hsubmenu_rules)
	{
		_r_menu_addsubmenu (hmenu, INT_ERROR, hsubmenu_rules, _r_locale_getstring (IDS_TRAY_RULES));

		_r_menu_additem (hsubmenu_rules, IDM_DISABLENOTIFICATIONS, _r_locale_getstring (IDS_DISABLENOTIFICATIONS));
		_r_menu_additem (hsubmenu_rules, IDM_DISABLEREMOVAL, _r_locale_getstring (IDS_DISABLEREMOVAL));

		if (_app_isdisabledremoval (app_hash))
			_r_menu_enableitem (hsubmenu_rules, IDM_DISABLEREMOVAL, FALSE, FALSE);

		_r_menu_additem (hsubmenu_rules, 0, NULL);

		_app_generate_rulescontrol (hsubmenu_rules, app_hash, NULL);
	}

	// show timers
	if (hsubmenu_timers)
	{
		_r_menu_addsubmenu (hmenu, INT_ERROR, hsubmenu_timers, _r_locale_getstring (IDS_TIMER));

		_r_menu_additem (hsubmenu_timers, IDM_DISABLETIMER, _r_locale_getstring (IDS_DISABLETIMER));
		_r_menu_additem (hsubmenu_timers, 0, NULL);

		_app_generate_timerscontrol (hsubmenu_timers, app_hash);
	}

	_r_menu_additem (hmenu, 0, NULL);
}

VOID _app_message_contextmenu (
	_In_ HWND hwnd,
	_In_ LPNMITEMACTIVATE lpnmlv
)
{
	PR_STRING localized_string = NULL;
	PR_STRING column_text = NULL;
	HMENU hsubmenu_timers = NULL;
	HMENU hsubmenu_rules = NULL;
	HMENU hmenu;
	PITEM_NETWORK ptr_network;
	PITEM_APP ptr_app = NULL;
	ULONG hash_code;
	ULONG app_hash;
	INT listview_id;
	INT command_id;
	BOOLEAN is_checked = FALSE;
	BOOLEAN is_readonly = FALSE;

	if (lpnmlv->iItem == INT_ERROR)
		return;

	listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

	if (!(listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_LOG))
		return;

	hmenu = CreatePopupMenu ();

	if (!hmenu)
		return;

	hash_code = (ULONG)_app_listview_getitemcontext (hwnd, listview_id, lpnmlv->iItem);
	app_hash = _app_listview_getappcontext (hwnd, listview_id, lpnmlv->iItem);

	switch (listview_id)
	{
		case IDC_APPS_PROFILE:
		case IDC_APPS_SERVICE:
		case IDC_APPS_UWP:
		{
			ptr_app = _app_getappitem (app_hash);

			_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_EDIT2), L"...\tEnter"));

			_r_menu_additem_ex (hmenu, IDM_PROPERTIES, localized_string->buffer, MF_DEFAULT);

			hsubmenu_rules = CreatePopupMenu ();
			hsubmenu_timers = CreatePopupMenu ();

			_app_generate_appmenu (hmenu, hsubmenu_rules, hsubmenu_timers, app_hash);

			_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_EXPLORE), L"\tCtrl+E"));
			_r_menu_additem (hmenu, IDM_EXPLORE, localized_string->buffer);

			_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_DELETE), L"\tDel"));
			_r_menu_additem (hmenu, IDM_DELETE, localized_string->buffer);

			_r_menu_additem (hmenu, 0, NULL);
			_r_menu_additem (hmenu, IDM_CHECK, _r_locale_getstring (IDS_CHECK));
			_r_menu_additem (hmenu, IDM_UNCHECK, _r_locale_getstring (IDS_UNCHECK));
			_r_menu_additem (hmenu, 0, NULL);

			_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_SELECT_ALL), L"\tCtrl+A"));
			_r_menu_additem (hmenu, IDM_SELECT_ALL, localized_string->buffer);

			_r_menu_additem (hmenu, 0, NULL);

			_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_COPY), L"\tCtrl+C"));
			_r_menu_additem (hmenu, IDM_COPY, localized_string->buffer);

			column_text = _r_listview_getcolumntext (hwnd, listview_id, lpnmlv->iSubItem);

			if (column_text)
			{
				_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (4, _r_locale_getstring (IDS_COPY), L" \"", column_text->buffer, L"\""));
				_r_menu_additem (hmenu, IDM_COPY_VALUE, localized_string->buffer);

				_r_obj_dereference (column_text);
			}

			if (ptr_app)
			{
				if (_app_getappinfo (ptr_app, INFO_IS_SILENT, &is_checked, sizeof (BOOLEAN)))
				{
					if (is_checked)
						_r_menu_checkitem (hmenu, IDM_DISABLENOTIFICATIONS, 0, MF_BYCOMMAND, is_checked);
				}

				if (_app_getappinfo (ptr_app, INFO_IS_UNDELETABLE, &is_checked, sizeof (BOOLEAN)))
				{
					if (is_checked)
					{
						_r_menu_checkitem (hmenu, IDM_DISABLEREMOVAL, 0, MF_BYCOMMAND, is_checked);

						_r_menu_enableitem (hmenu, IDM_DELETE, FALSE, FALSE);
					}
				}
			}

			break;
		}

		case IDC_RULES_BLOCKLIST:
		case IDC_RULES_SYSTEM:
		case IDC_RULES_CUSTOM:
		{
			if (listview_id == IDC_RULES_CUSTOM)
			{
				_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_ADD), L"..."));
				_r_menu_additem (hmenu, IDM_OPENRULESEDITOR, localized_string->buffer);
			}

			_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_EDIT2), L"...\tEnter"));
			_r_menu_additem_ex (hmenu, IDM_PROPERTIES, localized_string->buffer, MF_DEFAULT);

			if (listview_id == IDC_RULES_CUSTOM)
			{
				_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_DELETE), L"\tDel"));
				_r_menu_additem (hmenu, IDM_DELETE, localized_string->buffer);

				if (_app_getruleinfobyid (hash_code, INFO_IS_READONLY, &is_readonly, sizeof (BOOLEAN)))
				{
					if (is_readonly)
						_r_menu_enableitem (hmenu, IDM_DELETE, FALSE, FALSE);
				}
			}

			_r_menu_additem (hmenu, 0, NULL);
			_r_menu_additem (hmenu, IDM_CHECK, _r_locale_getstring (IDS_CHECK));
			_r_menu_additem (hmenu, IDM_UNCHECK, _r_locale_getstring (IDS_UNCHECK));
			_r_menu_additem (hmenu, 0, NULL);

			_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_SELECT_ALL), L"\tCtrl+A"));

			_r_menu_additem (hmenu, IDM_SELECT_ALL, localized_string->buffer);
			_r_menu_additem (hmenu, 0, NULL);

			_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_COPY), L"\tCtrl+C"));

			_r_menu_additem (hmenu, IDM_COPY, localized_string->buffer);

			column_text = _r_listview_getcolumntext (hwnd, listview_id, lpnmlv->iSubItem);

			if (column_text)
			{
				_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (4, _r_locale_getstring (IDS_COPY), L" \"", column_text->buffer, L"\""));

				_r_menu_additem (hmenu, IDM_COPY_VALUE, localized_string->buffer);

				_r_obj_dereference (column_text);
			}

			break;
		}

		case IDC_NETWORK:
		{
			_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_SHOWINLIST), L"\tEnter"));
			_r_menu_additem_ex (hmenu, IDM_PROPERTIES, localized_string->buffer, MF_DEFAULT);

			_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_OPENRULESEDITOR), L"..."));
			_r_menu_additem (hmenu, IDM_OPENRULESEDITOR, localized_string->buffer);

			_r_menu_additem (hmenu, 0, NULL);

			_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_EXPLORE), L"\tCtrl+E"));
			_r_menu_additem (hmenu, IDM_EXPLORE, localized_string->buffer);

			_r_menu_additem (hmenu, IDM_DELETE, _r_locale_getstring (IDS_NETWORK_CLOSE));
			_r_menu_additem (hmenu, 0, NULL);

			_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_SELECT_ALL), L"\tCtrl+A"));
			_r_menu_additem (hmenu, IDM_SELECT_ALL, localized_string->buffer);

			_r_menu_additem (hmenu, 0, NULL);

			_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_COPY), L"\tCtrl+C"));
			_r_menu_additem (hmenu, IDM_COPY, localized_string->buffer);

			column_text = _r_listview_getcolumntext (hwnd, listview_id, lpnmlv->iSubItem);

			if (column_text)
			{
				_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (4, _r_locale_getstring (IDS_COPY), L" \"", column_text->buffer, L"\""));

				_r_menu_additem (hmenu, IDM_COPY_VALUE, localized_string->buffer);

				_r_obj_dereference (column_text);
			}

			ptr_network = _app_network_getitem (hash_code);

			if (ptr_network)
			{
				if (ptr_network->af != AF_INET || ptr_network->state != MIB_TCP_STATE_ESTAB)
					_r_menu_enableitem (hmenu, IDM_DELETE, FALSE, FALSE);

				_r_obj_dereference (ptr_network);
			}

			break;
		}

		case IDC_LOG:
		{
			_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_SHOWINLIST), L"\tEnter"));
			_r_menu_additem_ex (hmenu, IDM_PROPERTIES, localized_string->buffer, MF_DEFAULT);

			_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_OPENRULESEDITOR), L"..."));
			_r_menu_additem (hmenu, IDM_OPENRULESEDITOR, localized_string->buffer);

			hsubmenu_rules = CreatePopupMenu ();
			hsubmenu_timers = CreatePopupMenu ();

			_app_generate_appmenu (hmenu, hsubmenu_rules, hsubmenu_timers, app_hash);

			_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_EXPLORE), L"\tCtrl+E"));
			_r_menu_additem (hmenu, IDM_EXPLORE, localized_string->buffer);

			_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_LOGCLEAR), L"\tCtrl+X"));
			_r_menu_additem (hmenu, IDM_TRAY_LOGCLEAR, localized_string->buffer);

			_r_menu_additem (hmenu, 0, NULL);

			_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_SELECT_ALL), L"\tCtrl+A"));
			_r_menu_additem (hmenu, IDM_SELECT_ALL, localized_string->buffer);

			_r_menu_additem (hmenu, 0, NULL);

			_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_COPY), L"\tCtrl+C"));
			_r_menu_additem (hmenu, IDM_COPY, localized_string->buffer);

			column_text = _r_listview_getcolumntext (hwnd, listview_id, lpnmlv->iSubItem);

			if (column_text)
			{
				_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (4, _r_locale_getstring (IDS_COPY), L" \"", column_text->buffer, L"\""));
				_r_menu_additem (hmenu, IDM_COPY_VALUE, localized_string->buffer);

				_r_obj_dereference (column_text);
			}

			ptr_app = _app_getappitem (app_hash);

			if (ptr_app)
			{
				if (_app_getappinfo (ptr_app, INFO_IS_SILENT, &is_checked, sizeof (BOOLEAN)))
				{
					if (is_checked)
						_r_menu_checkitem (hmenu, IDM_DISABLENOTIFICATIONS, 0, MF_BYCOMMAND, is_checked);
				}

				if (_app_getappinfo (ptr_app, INFO_IS_UNDELETABLE, &is_checked, sizeof (BOOLEAN)))
				{
					if (is_checked)
						_r_menu_checkitem (hmenu, IDM_DISABLEREMOVAL, 0, MF_BYCOMMAND, is_checked);
				}
			}

			break;
		}
	}

	command_id = _r_menu_popup (hmenu, hwnd, NULL, FALSE);

	if (command_id)
		_r_ctrl_sendcommand (hwnd, command_id, (LPARAM)lpnmlv->iSubItem);

	if (hsubmenu_rules)
		DestroyMenu (hsubmenu_rules);

	if (hsubmenu_timers)
		DestroyMenu (hsubmenu_timers);

	DestroyMenu (hmenu);

	if (localized_string)
		_r_obj_dereference (localized_string);

	if (ptr_app)
		_r_obj_dereference (ptr_app);
}

VOID _app_message_contextmenu_columns (
	_In_ HWND hwnd,
	_In_ LPNMHDR nmlp
)
{
	PR_STRING column_text;
	HWND hlistview;
	HMENU hmenu;
	INT listview_id;

	hlistview = GetParent (nmlp->hwndFrom);

	if (!hlistview)
		return;

	listview_id = GetDlgCtrlID (hlistview);

	if (!(listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_LOG))
		return;

	hmenu = CreatePopupMenu ();

	if (!hmenu)
		return;

	for (INT i = 0; i < _r_listview_getcolumncount (hwnd, listview_id); i++)
	{
		column_text = _r_listview_getcolumntext (hwnd, listview_id, i);

		if (column_text)
		{
			_r_menu_additem (hmenu, IDX_COLUMN + i, column_text->buffer);

			_r_menu_checkitem (hmenu, IDX_COLUMN + i, 0, MF_BYCOMMAND, TRUE);

			_r_obj_dereference (column_text);
		}

	}

	_r_menu_popup (hmenu, hwnd, NULL, TRUE);

	DestroyMenu (hmenu);
}

VOID _app_message_traycontextmenu (
	_In_ HWND hwnd
)
{
	ENUM_INSTALL_TYPE install_type;
	HMENU hsubmenu;
	HMENU hmenu;

	hmenu = LoadMenuW (NULL, MAKEINTRESOURCE (IDM_TRAY));

	if (!hmenu)
		return;

	hsubmenu = GetSubMenu (hmenu, 0);

	if (!hsubmenu)
	{
		DestroyMenu (hmenu);

		return;
	}

	install_type = _wfp_getinstalltype ();

	_r_menu_setitembitmap (hsubmenu, IDM_TRAY_START, FALSE, _app_getstatebitmap (install_type));

	// localize
	_r_menu_setitemtext (hsubmenu, IDM_TRAY_SHOW, FALSE, _r_locale_getstring (IDS_TRAY_SHOW));
	_r_menu_setitemtext (hsubmenu, IDM_TRAY_START, FALSE, _app_getstateaction (install_type));
	_r_menu_setitemtext (hsubmenu, NOTIFICATIONS_ID, TRUE, _r_locale_getstring (IDS_TITLE_NOTIFICATIONS));
	_r_menu_setitemtext (hsubmenu, LOGGING_ID, TRUE, _r_locale_getstring (IDS_TITLE_LOGGING));
	_r_menu_setitemtext (hsubmenu, IDM_TRAY_ENABLENOTIFICATIONS_CHK, FALSE, _r_locale_getstring (IDS_ENABLENOTIFICATIONS_CHK));
	_r_menu_setitemtext (hsubmenu, IDM_TRAY_ENABLENOTIFICATIONSSOUND_CHK, FALSE, _r_locale_getstring (IDS_NOTIFICATIONSOUND_CHK));
	_r_menu_setitemtext (hsubmenu, IDM_TRAY_NOTIFICATIONFULLSCREENSILENTMODE_CHK, FALSE, _r_locale_getstring (IDS_NOTIFICATIONFULLSCREENSILENTMODE_CHK));
	_r_menu_setitemtext (hsubmenu, IDM_TRAY_NOTIFICATIONONTRAY_CHK, FALSE, _r_locale_getstring (IDS_NOTIFICATIONONTRAY_CHK));
	_r_menu_setitemtext (hsubmenu, IDM_TRAY_ENABLELOG_CHK, FALSE, _r_locale_getstring (IDS_ENABLELOG_CHK));
	_r_menu_setitemtextformat (hsubmenu, IDM_TRAY_ENABLEUILOG_CHK, FALSE, L"%s (%s)", _r_locale_getstring (IDS_ENABLEUILOG_CHK), _r_locale_getstring (IDS_SESSION_ONLY));

	_r_menu_setitemtext (hsubmenu, IDM_TRAY_LOGSHOW, FALSE, _r_locale_getstring (IDS_LOGSHOW));
	_r_menu_setitemtext (hsubmenu, IDM_TRAY_LOGCLEAR, FALSE, _r_locale_getstring (IDS_LOGCLEAR));

	if (_r_fs_exists (&_r_app_getlogpath ()->sr))
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
	_r_menu_checkitem (hsubmenu, IDM_TRAY_ENABLENOTIFICATIONS_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsNotificationsEnabled", TRUE, NULL));
	_r_menu_checkitem (hsubmenu, IDM_TRAY_ENABLENOTIFICATIONSSOUND_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsNotificationsSound", TRUE, NULL));
	_r_menu_checkitem (hsubmenu, IDM_TRAY_NOTIFICATIONFULLSCREENSILENTMODE_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsNotificationsFullscreenSilentMode", TRUE, NULL));
	_r_menu_checkitem (hsubmenu, IDM_TRAY_NOTIFICATIONONTRAY_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsNotificationsOnTray", FALSE, NULL));
	_r_menu_checkitem (hsubmenu, IDM_TRAY_ENABLELOG_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsLogEnabled", FALSE, NULL));
	_r_menu_checkitem (hsubmenu, IDM_TRAY_ENABLEUILOG_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsLogUiEnabled", FALSE, NULL));

	if (!_r_config_getboolean (L"IsNotificationsEnabled", TRUE, NULL))
	{
		_r_menu_enableitem (hsubmenu, IDM_TRAY_ENABLENOTIFICATIONSSOUND_CHK, FALSE, FALSE);
		_r_menu_enableitem (hsubmenu, IDM_TRAY_NOTIFICATIONFULLSCREENSILENTMODE_CHK, FALSE, FALSE);
		_r_menu_enableitem (hsubmenu, IDM_TRAY_NOTIFICATIONONTRAY_CHK, FALSE, FALSE);
	}
	else if (!_r_config_getboolean (L"IsNotificationsSound", TRUE, NULL))
	{
		_r_menu_enableitem (hsubmenu, IDM_TRAY_NOTIFICATIONFULLSCREENSILENTMODE_CHK, FALSE, FALSE);
	}

	if (_wfp_isfiltersapplying ())
		_r_menu_enableitem (hsubmenu, IDM_TRAY_START, FALSE, FALSE);

	SetForegroundWindow (hwnd); // don't touch

	_r_menu_popup (hsubmenu, hwnd, NULL, TRUE);

	DestroyMenu (hmenu);
}

VOID _app_message_dpichanged (
	_In_ HWND hwnd,
	_In_ LONG dpi_value
)
{
	PR_STRING localized_string = NULL;

	_app_windowloadfont (dpi_value);

	_app_imagelist_init (hwnd, dpi_value);

	// reset toolbar information
	_app_setinterfacestate (hwnd, dpi_value);

	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_REFRESH, _r_locale_getstring (IDS_REFRESH), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, 0, I_IMAGENONE);
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_SETTINGS, _r_locale_getstring (IDS_SETTINGS), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, 0, I_IMAGENONE);

	_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_OPENRULESEDITOR), L"..."));
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_OPENRULESEDITOR, localized_string->buffer, BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLENOTIFICATIONS_CHK, _r_locale_getstring (IDS_ENABLENOTIFICATIONS_CHK), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLELOG_CHK, _r_locale_getstring (IDS_ENABLELOG_CHK), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLEUILOG_CHK, _r_locale_getstring (IDS_ENABLEUILOG_CHK), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_LOGSHOW), L" (Ctrl+I)"));
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_LOGSHOW, localized_string->buffer, BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_LOGCLEAR), L" (Ctrl+X)"));
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_LOGCLEAR, localized_string->buffer, BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_DONATE, _r_locale_getstring (IDS_DONATE), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_app_listview_loadfont (dpi_value, TRUE);
	_app_listview_updateby_id (hwnd, DATA_LISTVIEW_CURRENT, PR_UPDATE_TYPE | PR_UPDATE_FORCE);

	_r_wnd_sendmessage (hwnd, 0, WM_SIZE, 0, 0);

	_r_obj_dereference (localized_string);
}

LONG_PTR _app_message_custdraw (
	_In_ HWND hwnd,
	_In_ LPNMLVCUSTOMDRAW lpnmlv
)
{
	PR_STRING real_path = NULL;
	PITEM_NETWORK ptr_network;
	PITEM_COLOR ptr_clr;
	PITEM_LOG ptr_log;
	COLORREF new_clr = 0;
	ULONG app_hash = 0;
	ULONG index;
	INT listview_id;
	BOOLEAN is_validconnection = FALSE;
	BOOLEAN is_systemapp = FALSE;

	switch (lpnmlv->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
		{
			return CDRF_NOTIFYITEMDRAW;
		}

		case CDDS_ITEMPREPAINT:
		{
			listview_id = (INT)(INT_PTR)lpnmlv->nmcd.hdr.idFrom;

			if (lpnmlv->dwItemType != LVCDI_ITEM)
				return CDRF_DODEFAULT;

			if (!lpnmlv->nmcd.lItemlParam)
				return CDRF_DODEFAULT;

			if (!_r_config_getboolean (L"IsEnableHighlighting", TRUE, NULL))
				return CDRF_DODEFAULT;

			switch (listview_id)
			{
				case IDC_APPS_PROFILE:
				case IDC_APPS_SERVICE:
				case IDC_APPS_UWP:
				case IDC_RULE_APPS_ID:
				case IDC_NETWORK:
				case IDC_LOG:
				{
					index = (ULONG)_app_listview_getcontextcode (lpnmlv->nmcd.lItemlParam);

					if (listview_id == IDC_NETWORK)
					{
						ptr_network = _app_network_getitem (index);

						if (ptr_network)
						{
							app_hash = ptr_network->app_hash;
							is_systemapp = _app_isappfromsystem (ptr_network->path, app_hash);
							is_validconnection = ptr_network->is_connection;
							//is_validconnection = FALSE; // do not highlight connections

							_r_obj_dereference (ptr_network);
						}
					}
					else if (listview_id == IDC_LOG)
					{
						ptr_log = _app_getlogitem (index);

						if (ptr_log)
						{
							app_hash = _app_getlogapp (index);
							is_systemapp = _app_isappfromsystem (ptr_log->path, app_hash);

							_r_obj_dereference (ptr_log);
						}
					}
					else
					{
						app_hash = index;
						is_validconnection = _app_network_isapphaveconnection (app_hash);

						if (_app_getappinfobyhash (app_hash, INFO_PATH, &real_path, sizeof (PR_STRING)))
						{
							is_systemapp = _app_isappfromsystem (real_path, app_hash);

							_r_obj_dereference (real_path);
						}
					}

					if (app_hash)
						new_clr = _app_getappcolor (listview_id, app_hash, is_systemapp, is_validconnection);

					break;
				}

				case IDC_RULES_BLOCKLIST:
				case IDC_RULES_SYSTEM:
				case IDC_RULES_CUSTOM:
				case IDC_APP_RULES_ID:
				{
					index = (ULONG)_app_listview_getcontextcode (lpnmlv->nmcd.lItemlParam);

					new_clr = _app_getrulecolor (listview_id, index);

					break;
				}

				case IDC_COLORS:
				{
					ptr_clr = (PITEM_COLOR)lpnmlv->nmcd.lItemlParam;

					new_clr = ptr_clr->new_clr ? ptr_clr->new_clr : ptr_clr->default_clr;

					break;
				}

				default:
				{
					return CDRF_DODEFAULT;
				}
			}

			if (new_clr)
			{
				lpnmlv->clrText = _r_dc_getcolorbrightness (new_clr);
				lpnmlv->clrTextBk = new_clr;

				_r_dc_fillrect (lpnmlv->nmcd.hdc, &lpnmlv->nmcd.rc, new_clr);

				return CDRF_NEWFONT;
			}

			break;
		}
	}

	return CDRF_DODEFAULT;
}

VOID _app_displayinfoapp_callback (
	_In_ PITEM_APP ptr_app,
	_Inout_ LPNMLVDISPINFOW lpnmlv
)
{
	PR_STRING string;
	LONG icon_id = 0;

	// set text
	if (lpnmlv->item.mask & LVIF_TEXT)
	{
		switch (lpnmlv->item.iSubItem)
		{
			case 0:
			{
				string = _app_getappdisplayname (ptr_app, FALSE);

				if (string)
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, string->buffer);

					_r_obj_dereference (string);
				}

				break;
			}

			case 1:
			{
				string = _r_format_unixtime (ptr_app->timestamp, FDTF_SHORTDATE | FDTF_LONGTIME);

				if (string)
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, string->buffer);

					_r_obj_dereference (string);
				}

				break;
			}
		}
	}

	// set image
	if (lpnmlv->item.mask & LVIF_IMAGE)
	{
		if (_app_getappinfoparam2 (ptr_app->app_hash, (INT)lpnmlv->hdr.idFrom, INFO_ICON_ID, &icon_id, sizeof (LONG)))
			lpnmlv->item.iImage = icon_id;
	}

	// set group id
	if (lpnmlv->item.mask & LVIF_GROUPID)
	{
		if (_app_listview_isitemhidden (lpnmlv->item.lParam))
		{
			lpnmlv->item.iGroupId = LV_HIDDEN_GROUP_ID;
		}
		else
		{
			if (lpnmlv->hdr.idFrom == IDC_RULE_APPS_ID)
			{
				if (ptr_app->type == DATA_APP_UWP)
				{
					lpnmlv->item.iGroupId = 2;
				}
				else if (ptr_app->type == DATA_APP_SERVICE)
				{
					lpnmlv->item.iGroupId = 1;
				}
				else
				{
					lpnmlv->item.iGroupId = 0;
				}
			}
			else
			{
				// apps with special rule
				if (_app_isapphaverule (ptr_app->app_hash, FALSE))
				{
					lpnmlv->item.iGroupId = 2;
				}
				else if (_app_istimerset (ptr_app))
				{
					lpnmlv->item.iGroupId = 1;
				}
				else if (ptr_app->is_enabled)
				{
					lpnmlv->item.iGroupId = 0;
				}
				else
				{
					// silent apps without rules and not enabled added into silent group
					if (ptr_app->is_silent)
					{
						lpnmlv->item.iGroupId = 4;
					}
					else
					{
						lpnmlv->item.iGroupId = 3;
					}
				}
			}
		}
	}
}

VOID _app_displayinforule_callback (
	_In_ INT listview_id,
	_In_ PITEM_RULE ptr_rule,
	_Inout_ LPNMLVDISPINFOW lpnmlv
)
{
	PR_STRING string;

	// set text
	if (lpnmlv->item.mask & LVIF_TEXT)
	{
		switch (lpnmlv->item.iSubItem)
		{
			case 0:
			{
				if (ptr_rule->name)
				{
					if (ptr_rule->is_readonly && ptr_rule->type == DATA_RULE_USER)
					{
						_r_str_printf (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, L"%s" SZ_RULE_INTERNAL_MENU, ptr_rule->name->buffer);
					}
					else
					{
						_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, ptr_rule->name->buffer);
					}
				}
				else
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, _r_locale_getstring (IDS_STATUS_EMPTY));
				}

				break;
			}

			case 1:
			{
				if (ptr_rule->protocol_str)
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, ptr_rule->protocol_str->buffer);
				}
				else
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, _r_locale_getstring (IDS_ANY));
				}

				break;
			}

			case 2:
			{
				string = _app_db_getdirectionname (ptr_rule->direction, FALSE, TRUE);

				if (string)
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, string->buffer);

					_r_obj_dereference (string);
				}

				break;
			}
		}
	}

	// set image
	if (lpnmlv->item.mask & LVIF_IMAGE)
		lpnmlv->item.iImage = (ptr_rule->action == FWP_ACTION_BLOCK) ? 1 : 0;

	// set group id
	if (lpnmlv->item.mask & LVIF_GROUPID)
	{
		if (_app_listview_isitemhidden (lpnmlv->item.lParam))
		{
			lpnmlv->item.iGroupId = LV_HIDDEN_GROUP_ID;
		}
		else
		{
			if (listview_id == IDC_APP_RULES_ID)
			{
				lpnmlv->item.iGroupId = ptr_rule->is_readonly ? 0 : 1;
			}
			else
			{
				if (ptr_rule->is_enabled)
				{
					if (ptr_rule->is_forservices || !_r_obj_isempty (ptr_rule->apps))
					{
						lpnmlv->item.iGroupId = 1;
					}
					else
					{
						lpnmlv->item.iGroupId = 0;
					}
				}
				else
				{
					lpnmlv->item.iGroupId = 2;
				}
			}
		}
	}
}

VOID _app_displayinfonetwork_callback (
	_In_ PITEM_NETWORK ptr_network,
	_Inout_ LPNMLVDISPINFOW lpnmlv
)
{
	PITEM_APP ptr_app;
	PR_STRING string;
	LPCWSTR name;
	LONG icon_id = 0;

	// set text
	if (lpnmlv->item.mask & LVIF_TEXT)
	{
		switch (lpnmlv->item.iSubItem)
		{
			case 0:
			{
				ptr_app = _app_getappitem (ptr_network->app_hash);

				if (ptr_app)
				{
					string = _app_getappdisplayname (ptr_app, TRUE);

					if (string)
					{
						_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, string->buffer);

						_r_obj_dereference (string);
					}

					_r_obj_dereference (ptr_app);
				}
				else if (ptr_network->path)
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, _r_path_getbasename (&ptr_network->path->sr));
				}

				break;
			}

			case 1:
			{
				string = _InterlockedCompareExchangePointer ((volatile PVOID_PTR)&ptr_network->local_addr_str, NULL, NULL);

				if (string)
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, string->buffer);

				break;
			}

			case 2:
			{
				string = _InterlockedCompareExchangePointer ((volatile PVOID_PTR)&ptr_network->local_host_str, NULL, NULL);

				if (string)
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, string->buffer);
				}
				else
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, _r_locale_getstring (IDS_LOADING));
				}

				break;
			}

			case 3:
			{
				if (ptr_network->local_port)
				{
					string = _app_formatport (ptr_network->local_port, ptr_network->protocol);

					if (string)
					{
						_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, string->buffer);

						_r_obj_dereference (string);
					}
				}

				break;
			}

			case 4:
			{
				string = _InterlockedCompareExchangePointer ((volatile PVOID_PTR)&ptr_network->remote_addr_str, NULL, NULL);

				if (string)
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, string->buffer);

				break;
			}

			case 5:
			{
				string = _InterlockedCompareExchangePointer ((volatile PVOID_PTR)&ptr_network->remote_host_str, NULL, NULL);

				if (string)
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, string->buffer);
				}
				else
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, _r_locale_getstring (IDS_LOADING));
				}

				break;
			}

			case 6:
			{
				if (ptr_network->remote_port)
				{
					string = _app_formatport (ptr_network->remote_port, ptr_network->protocol);

					if (string)
					{
						_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, string->buffer);

						_r_obj_dereference (string);
					}
				}

				break;
			}

			case 7:
			{
				if (ptr_network->protocol_str)
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, ptr_network->protocol_str->buffer);
				}
				else
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, _r_locale_getstring (IDS_ANY));
				}

				break;
			}

			case 8:
			{
				name = _app_db_getconnectionstatename (ptr_network->state);

				if (name)
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, name);

				break;
			}
		}
	}

	// set image
	if (lpnmlv->item.mask & LVIF_IMAGE)
	{
		if (_app_getappinfoparam2 (ptr_network->app_hash, (INT)lpnmlv->hdr.idFrom, INFO_ICON_ID, &icon_id, sizeof (LONG)))
			lpnmlv->item.iImage = icon_id;
	}

	// set group id
	if (lpnmlv->item.mask & LVIF_GROUPID)
	{
		if (_app_listview_isitemhidden (lpnmlv->item.lParam))
		{
			lpnmlv->item.iGroupId = LV_HIDDEN_GROUP_ID;
		}
		else
		{
			if (ptr_network->type == DATA_APP_SERVICE)
			{
				lpnmlv->item.iGroupId = 1;
			}
			else if (ptr_network->type == DATA_APP_UWP)
			{
				lpnmlv->item.iGroupId = 2;
			}
			else
			{
				lpnmlv->item.iGroupId = 0;
			}
		}
	}
}

VOID _app_displayinfolog_callback (
	_Inout_ LPNMLVDISPINFOW lpnmlv,
	_In_opt_ PITEM_APP ptr_app,
	_In_ PITEM_LOG ptr_log
)
{
	PR_STRING string;
	LONG icon_id = 0;

	// set text
	if (lpnmlv->item.mask & LVIF_TEXT)
	{
		switch (lpnmlv->item.iSubItem)
		{
			case 0:
			{
				if (ptr_app)
				{
					string = _app_getappdisplayname (ptr_app, TRUE);

					if (string)
					{
						_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, string->buffer);

						_r_obj_dereference (string);
					}
				}
				else if (ptr_log->path)
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, _r_path_getbasename (&ptr_log->path->sr));
				}
				else
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, _r_locale_getstring (IDS_STATUS_EMPTY));
				}

				break;
			}

			case 1:
			{
				string = _r_format_unixtime (ptr_log->timestamp, FDTF_SHORTDATE | FDTF_LONGTIME);

				if (string)
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, string->buffer);

					_r_obj_dereference (string);
				}

				break;
			}

			case 2:
			{
				string = _InterlockedCompareExchangePointer ((volatile PVOID_PTR)&ptr_log->local_addr_str, NULL, NULL);

				if (string)
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, string->buffer);

				break;
			}

			case 3:
			{
				string = _InterlockedCompareExchangePointer ((volatile PVOID_PTR)&ptr_log->local_host_str, NULL, NULL);

				if (string)
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, string->buffer);
				}
				else
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, _r_locale_getstring (IDS_LOADING));
				}

				break;
			}

			case 4:
			{
				if (ptr_log->local_port)
				{
					string = _app_formatport (ptr_log->local_port, ptr_log->protocol);

					if (string)
					{
						_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, string->buffer);

						_r_obj_dereference (string);
					}
				}

				break;
			}

			case 5:
			{
				string = _InterlockedCompareExchangePointer ((volatile PVOID_PTR)&ptr_log->remote_addr_str, NULL, NULL);

				if (string)
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, string->buffer);

				break;
			}

			case 6:
			{
				string = _InterlockedCompareExchangePointer ((volatile PVOID_PTR)&ptr_log->remote_host_str, NULL, NULL);

				if (string)
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, string->buffer);
				}
				else
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, _r_locale_getstring (IDS_LOADING));
				}

				break;
			}

			case 7:
			{
				if (ptr_log->remote_port)
				{
					string = _app_formatport (ptr_log->remote_port, ptr_log->protocol);

					if (string)
					{
						_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, string->buffer);

						_r_obj_dereference (string);
					}
				}

				break;
			}

			case 8:
			{
				if (ptr_log->protocol_str)
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, ptr_log->protocol_str->buffer);
				}
				else
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, _r_locale_getstring (IDS_ANY));
				}

				break;
			}

			case 9:
			{
				string = _app_db_getdirectionname (ptr_log->direction, ptr_log->is_loopback, FALSE);

				if (string)
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, string->buffer);

					_r_obj_dereference (string);
				}

				break;
			}

			case 10:
			{
				string = _r_obj_concatstrings (
					2,
					ptr_log->is_allow ? L"[A] " : L"[B] ",
					_r_obj_getstringordefault (ptr_log->filter_name, _r_locale_getstring (IDS_STATUS_EMPTY))
				);

				_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, string->buffer);

				_r_obj_dereference (string);

				break;
			}
		}
	}

	// set image
	if (lpnmlv->item.mask & LVIF_IMAGE)
	{
		if (_app_getappinfoparam2 (ptr_log->app_hash, (INT)lpnmlv->hdr.idFrom, INFO_ICON_ID, &icon_id, sizeof (LONG)))
			lpnmlv->item.iImage = icon_id;
	}

	// set group id
	if (lpnmlv->item.mask & LVIF_GROUPID)
	{
		if (_app_listview_isitemhidden (lpnmlv->item.lParam))
		{
			lpnmlv->item.iGroupId = LV_HIDDEN_GROUP_ID;
		}
		else
		{
			lpnmlv->item.iGroupId = 0;
		}
	}
}

BOOLEAN _app_message_displayinfo (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_Inout_ LPNMLVDISPINFOW lpnmlv
)
{
	PITEM_NETWORK ptr_network;
	PITEM_RULE ptr_rule;
	PITEM_APP ptr_app;
	PITEM_LOG ptr_log;
	ULONG_PTR index;

	index = _app_listview_getitemcontext (hwnd, listview_id, lpnmlv->item.iItem);

	switch (listview_id)
	{
		case IDC_APPS_PROFILE:
		case IDC_APPS_SERVICE:
		case IDC_APPS_UWP:
		case IDC_RULE_APPS_ID:
		{
			ptr_app = _app_getappitem ((ULONG)index);

			if (ptr_app)
			{
				_app_displayinfoapp_callback (ptr_app, lpnmlv);

				_r_obj_dereference (ptr_app);

				return TRUE;
			}

			break;
		}

		case IDC_RULES_BLOCKLIST:
		case IDC_RULES_SYSTEM:
		case IDC_RULES_CUSTOM:
		case IDC_APP_RULES_ID:
		{
			ptr_rule = _app_getrulebyid (index);

			if (ptr_rule)
			{
				_app_displayinforule_callback (listview_id, ptr_rule, lpnmlv);

				_r_obj_dereference (ptr_rule);

				return TRUE;
			}

			break;
		}

		case IDC_NETWORK:
		{
			ptr_network = _app_network_getitem ((ULONG)index);

			if (ptr_network)
			{
				_app_displayinfonetwork_callback (ptr_network, lpnmlv);

				_r_obj_dereference (ptr_network);

				return TRUE;
			}

			break;
		}

		case IDC_LOG:
		{
			ptr_log = _app_getlogitem ((ULONG)index);

			if (ptr_log)
			{
				ptr_app = _app_getappitem (ptr_log->app_hash);

				_app_displayinfolog_callback (lpnmlv, ptr_app, ptr_log);

				if (ptr_app)
					_r_obj_dereference (ptr_app);

				_r_obj_dereference (ptr_log);

				return TRUE;
			}

			break;
		}
	}

	return FALSE;
}

VOID _app_command_idtorules (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	PITEM_TAB_CONTEXT tab_context;
	PITEM_RULE ptr_rule;
	PITEM_APP ptr_app;
	PR_LIST rules;
	HANDLE hengine;
	ULONG_PTR rule_idx;
	ULONG app_hash;
	INT item_id = INT_ERROR;
	BOOL is_remove = INT_ERROR;

	tab_context = _app_listview_getcontext (hwnd, INT_ERROR);

	if (!tab_context || !_r_listview_getselectedcount (hwnd, tab_context->listview_id))
		return;

	rule_idx = (ULONG_PTR)ctrl_id - IDX_RULES_SPECIAL;
	ptr_rule = _app_getrulebyid (rule_idx);

	if (!ptr_rule)
		return;

	while ((item_id = _r_listview_getnextselected (hwnd, tab_context->listview_id, item_id)) != INT_ERROR)
	{
		app_hash = _app_listview_getappcontext (hwnd, tab_context->listview_id, item_id);

		if (ptr_rule->is_forservices && _app_issystemhash (app_hash))
			continue;

		ptr_app = _app_getappitem (app_hash);

		if (!ptr_app)
			continue;

		_app_notify_freeobject (NULL, ptr_app);

		if (is_remove == INT_ERROR)
			is_remove = !!(ptr_rule->is_enabled && _r_obj_findhashtable (ptr_rule->apps, app_hash));

		_app_setruletoapp (hwnd, ptr_rule, item_id, ptr_app, !is_remove);

		_r_obj_dereference (ptr_app);
	}

	if (_wfp_isfiltersinstalled ())
	{
		hengine = _wfp_getenginehandle ();

		rules = _r_obj_createlist (10, NULL);

		_r_obj_addlistitem (rules, ptr_rule, NULL);

		_wfp_create4filters (hengine, rules, DBG_ARG, FALSE);

		_r_obj_dereference (rules);
	}

	_app_listview_updateby_id (hwnd, tab_context->listview_id, 0);
	_app_listview_updateitemby_param (hwnd, rule_idx, FALSE);

	_r_obj_dereference (ptr_rule);

	_app_profile_save (hwnd);
}

VOID _app_command_idtotimers (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	PITEM_TAB_CONTEXT tab_context;
	PITEM_APP ptr_app;
	PR_LIST rules;
	HANDLE hengine;
	ULONG_PTR timer_idx;
	ULONG app_hash;
	LONG64 seconds;
	INT item_id = INT_ERROR;

	tab_context = _app_listview_getcontext (hwnd, INT_ERROR);

	if (!tab_context || !_r_listview_getselectedcount (hwnd, tab_context->listview_id))
		return;

	timer_idx = (ULONG_PTR)ctrl_id - IDX_TIMER;
	seconds = timer_array[timer_idx];

	if (_wfp_isfiltersinstalled ())
	{
		hengine = _wfp_getenginehandle ();

		rules = _r_obj_createlist (8, &_r_obj_dereference);

		while ((item_id = _r_listview_getnextselected (hwnd, tab_context->listview_id, item_id)) != INT_ERROR)
		{
			app_hash = _app_listview_getappcontext (hwnd, tab_context->listview_id, item_id);
			ptr_app = _app_getappitem (app_hash);

			if (ptr_app)
			{
				_app_timer_set (hwnd, ptr_app, seconds);

				_r_obj_addlistitem (rules, ptr_app, NULL);
			}
		}

		_wfp_create3filters (hengine, rules, TEXT (__FILE__), __LINE__, FALSE);

		_r_obj_dereference (rules);
	}

	_app_listview_updateby_id (hwnd, tab_context->listview_id, PR_UPDATE_FORCE);

	_app_profile_save (hwnd);
}

VOID _app_command_logshow (
	_In_ HWND hwnd
)
{
	HANDLE current_handle;
	PR_STRING viewer_path;
	PR_STRING log_path;
	PR_STRING cmdline;
	INT item_count;
	NTSTATUS status;

	if (_r_config_getboolean (L"IsLogUiEnabled", FALSE, NULL))
	{
		item_count = _r_listview_getitemcount (hwnd, IDC_LOG);

		_r_wnd_toggle (hwnd, TRUE);

		_app_settab_id (hwnd, IDC_LOG);

		if (item_count)
			_r_listview_ensurevisible (hwnd, IDC_LOG, item_count - 1);
	}
	else
	{
		log_path = _r_config_getstringexpand (L"LogPath", LOG_PATH_DEFAULT, NULL);

		viewer_path = _app_getlogviewer ();

		if (!log_path || !_r_fs_exists (&log_path->sr) || !viewer_path || !_r_fs_exists (&viewer_path->sr))
		{
			if (log_path)
				_r_obj_dereference (log_path);

			if (viewer_path)
				_r_obj_dereference (viewer_path);

			_r_wnd_toggle (hwnd, TRUE);

			return;
		}

		current_handle = _InterlockedCompareExchangePointer (&config.hlogfile, NULL, NULL);

		if (current_handle)
			_r_fs_flushfile (current_handle);

		cmdline = _r_obj_concatstrings (
			5,
			L"\"",
			viewer_path->buffer,
			L"\" \"",
			log_path->buffer,
			L"\""
		);

		status = _r_sys_createprocess (viewer_path->buffer, cmdline->buffer, NULL, FALSE);

		if (status != STATUS_SUCCESS)
			_r_show_errormessage (hwnd, NULL, status, cmdline->buffer, ET_NATIVE);

		_r_obj_dereference (viewer_path);
		_r_obj_dereference (log_path);
		_r_obj_dereference (cmdline);
	}
}

VOID _app_command_logclear (
	_In_ HWND hwnd
)
{
	HANDLE current_handle;
	PR_STRING log_path;
	LONG64 file_size;
	BOOLEAN is_valid;

	log_path = _r_config_getstringexpand (L"LogPath", LOG_PATH_DEFAULT, NULL);

	current_handle = _InterlockedCompareExchangePointer (&config.hlogfile, NULL, NULL);

	if (current_handle)
		_r_fs_getsize2 (NULL, current_handle, &file_size);

	is_valid = (current_handle && file_size > 2) || (log_path && _r_fs_exists (&log_path->sr));

	if (!is_valid)
	{
		_r_queuedlock_acquireshared (&lock_loglist);
		is_valid = !_r_obj_isempty (log_table);
		_r_queuedlock_releaseshared (&lock_loglist);
	}

	if (is_valid)
	{
		if (_r_show_confirmmessage (hwnd, _r_locale_getstring (IDS_QUESTION), NULL, L"ConfirmLogClear", FALSE))
		{
			_app_logclear (current_handle);

			_app_logclear_ui (hwnd);
		}
	}

	if (log_path)
		_r_obj_dereference (log_path);
}

VOID _app_command_logerrshow (
	_In_opt_ HWND hwnd
)
{
	PR_STRING process_path;
	PR_STRING viewer_path;
	PR_STRING path;
	NTSTATUS status;

	path = _r_app_getlogpath ();

	if (!_r_fs_exists (&path->sr))
		return;

	viewer_path = _app_getlogviewer ();

	if (!viewer_path)
		return;

	process_path = _r_obj_concatstrings (
		5,
		L"\"",
		viewer_path->buffer,
		L"\" \"",
		path->buffer,
		L"\""
	);

	status = _r_sys_createprocess (viewer_path->buffer, process_path->buffer, NULL, FALSE);

	if (status != STATUS_SUCCESS)
		_r_show_errormessage (hwnd, NULL, status, viewer_path->buffer, ET_NATIVE);

	_r_obj_dereference (process_path);
	_r_obj_dereference (viewer_path);
}

VOID _app_command_logerrclear (
	_In_opt_ HWND hwnd
)
{
	PR_STRING path;

	path = _r_app_getlogpath ();

	if (!_r_fs_exists (&path->sr))
		return;

	if (!_r_show_confirmmessage (hwnd, _r_locale_getstring (IDS_QUESTION), NULL, L"ConfirmLogClear", FALSE))
		return;

	_r_fs_deletefile (&path->sr, NULL);
}

VOID _app_command_copy (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT column_id
)
{
	R_STRINGREF divider_sr = PR_STRINGREF_INIT (DIVIDER_COPY);
	PITEM_TAB_CONTEXT tab_context;
	R_STRINGBUILDER sb;
	PR_STRING string;
	INT column_count;
	INT item_id = INT_ERROR;

	tab_context = _app_listview_getcontext (hwnd, INT_ERROR);

	if (!tab_context)
		return;

	column_count = _r_listview_getcolumncount (hwnd, tab_context->listview_id);

	_r_obj_initializestringbuilder (&sb, 512);

	while ((item_id = _r_listview_getnextselected (hwnd, tab_context->listview_id, item_id)) != INT_ERROR)
	{
		if (ctrl_id == IDM_COPY)
		{
			for (INT i = 0; i < column_count; i++)
			{
				string = _r_listview_getitemtext (hwnd, tab_context->listview_id, item_id, i);

				if (string)
				{
					_r_obj_appendstringbuilder2 (&sb, &string->sr);
					_r_obj_appendstringbuilder2 (&sb, &divider_sr);

					_r_obj_dereference (string);
				}
			}

			string = _r_obj_finalstringbuilder (&sb);

			_r_str_trimstring (&string->sr, &divider_sr, 0);
		}
		else
		{
			string = _r_listview_getitemtext (hwnd, tab_context->listview_id, item_id, column_id);

			if (string)
			{
				_r_obj_appendstringbuilder2 (&sb, &string->sr);

				_r_obj_dereference (string);
			}
		}

		_r_obj_appendstringbuilder (&sb, SZ_CRLF);
	}

	string = _r_obj_finalstringbuilder (&sb);

	_r_str_trimstring2 (&string->sr, DIVIDER_TRIM, 0);

	_r_clipboard_set (hwnd, &string->sr);

	_r_obj_deletestringbuilder (&sb);
}

VOID _app_command_checkbox (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	PITEM_TAB_CONTEXT tab_context;
	PITEM_APP ptr_app;
	PITEM_RULE ptr_rule;
	PR_LIST rules;
	HANDLE hengine;
	ULONG hash_code;
	INT item_id = INT_ERROR;
	BOOLEAN new_val = (ctrl_id == IDM_CHECK);
	BOOLEAN is_changed = FALSE;

	tab_context = _app_listview_getcontext (hwnd, INT_ERROR);

	if (!tab_context)
		return;

	rules = _r_obj_createlist (8, &_r_obj_dereference);

	if (tab_context->listview_id >= IDC_APPS_PROFILE && tab_context->listview_id <= IDC_APPS_UWP)
	{
		while ((item_id = _r_listview_getnextselected (hwnd, tab_context->listview_id, item_id)) != INT_ERROR)
		{
			hash_code = (ULONG)_app_listview_getitemcontext (hwnd, tab_context->listview_id, item_id);
			ptr_app = _app_getappitem (hash_code);

			if (!ptr_app)
				continue;

			if (ptr_app->is_enabled != new_val)
			{
				if (!new_val)
				{
					_app_timer_reset (hwnd, ptr_app);
				}
				else
				{
					_app_notify_freeobject (NULL, ptr_app);
				}

				ptr_app->is_enabled = new_val;

				_app_listview_lock (hwnd, tab_context->listview_id, TRUE);
				_app_setappiteminfo (hwnd, tab_context->listview_id, item_id, ptr_app);
				_app_listview_lock (hwnd, tab_context->listview_id, FALSE);

				_r_obj_addlistitem (rules, ptr_app, NULL);

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
				hengine = _wfp_getenginehandle ();

				_wfp_create3filters (hengine, rules, DBG_ARG, FALSE);
			}
		}
	}
	else if (tab_context->listview_id >= IDC_RULES_BLOCKLIST && tab_context->listview_id <= IDC_RULES_CUSTOM)
	{
		while ((item_id = _r_listview_getnextselected (hwnd, tab_context->listview_id, item_id)) != INT_ERROR)
		{
			hash_code = (ULONG)_app_listview_getitemcontext (hwnd, tab_context->listview_id, item_id);
			ptr_rule = _app_getrulebyid (hash_code);

			if (!ptr_rule)
				continue;

			if (ptr_rule->is_enabled != new_val)
			{
				_app_ruleenable (ptr_rule, new_val, TRUE);

				_app_listview_lock (hwnd, tab_context->listview_id, TRUE);
				_app_setruleiteminfo (hwnd, tab_context->listview_id, item_id, ptr_rule, TRUE);
				_app_listview_lock (hwnd, tab_context->listview_id, FALSE);

				_r_obj_addlistitem (rules, ptr_rule, NULL);

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
				hengine = _wfp_getenginehandle ();

				_wfp_create4filters (hengine, rules, DBG_ARG, FALSE);
			}
		}
	}

	_r_obj_dereference (rules);

	if (is_changed)
	{
		_app_listview_updateby_id (hwnd, tab_context->listview_id, PR_UPDATE_FORCE);

		_app_profile_save (hwnd);
	}
}

VOID _app_command_delete (
	_In_ HWND hwnd
)
{
	R_STRINGREF crlf = PR_STRINGREF_INIT (SZ_CRLF);
	PITEM_TAB_CONTEXT tab_context;
	R_STRINGBUILDER sb;
	PR_ARRAY guids = NULL;
	PITEM_NETWORK ptr_network;
	PR_HASHTABLE apps_checker;
	MIB_TCPROW tcprow;
	PR_STRING string;
	HANDLE hengine;
	PITEM_APP ptr_app;
	PITEM_RULE ptr_rule;
	ULONG_PTR rule_idx;
	ULONG_PTR enum_key;
	ULONG hash_code;
	INT selected_count;
	LPARAM lparam;
	INT item_count;

	tab_context = _app_listview_getcontext (hwnd, INT_ERROR);

	if (!tab_context || (tab_context->listview_id != IDC_APPS_PROFILE && tab_context->listview_id != IDC_RULES_CUSTOM && tab_context->listview_id != IDC_NETWORK))
		return;

	selected_count = _r_listview_getselectedcount (hwnd, tab_context->listview_id);

	if (!selected_count)
		return;

	item_count = _r_listview_getitemcount (hwnd, tab_context->listview_id);

	if (tab_context->listview_id != IDC_NETWORK)
	{
		_r_obj_initializestringbuilder (&sb, 256);

		string = _r_locale_getstring_ex (IDS_QUESTION_DELETE);

		if (string)
		{
			_r_obj_appendstringbuilder2 (&sb, &string->sr);
			_r_obj_appendstringbuilder2 (&sb, &crlf);
			_r_obj_appendstringbuilder2 (&sb, &crlf);

			_r_obj_dereference (string);
		}

		for (INT i = 0, j = 1; i < item_count; i++)
		{
			if (!_r_listview_isitemselected (hwnd, tab_context->listview_id, i))
				continue;

			lparam = _r_listview_getitemlparam (hwnd, tab_context->listview_id, i);

			if (_app_listview_isitemhidden (lparam))
				continue;

			string = _r_listview_getitemtext (hwnd, tab_context->listview_id, i, 0);

			if (string)
			{
				_r_obj_appendstringbuilderformat (&sb, L"%" TEXT (PRId32) ") ", j);
				_r_obj_appendstringbuilder2 (&sb, &string->sr);
				_r_obj_appendstringbuilder2 (&sb, &crlf);

				_r_obj_dereference (string);
			}

			j += 1;
		}

		string = _r_obj_finalstringbuilder (&sb);

		_r_str_trimstring (&string->sr, &crlf, PR_TRIM_END_ONLY);

		if (_r_show_message (hwnd, MB_YESNO | MB_ICONEXCLAMATION, NULL, string->buffer) != IDYES)
		{
			_r_obj_dereference (string);

			return;
		}

		guids = _r_obj_createarray (sizeof (GUID), 10, NULL);

		_r_obj_dereference (string);
	}

	for (INT i = item_count - 1; i != INT_ERROR; i--)
	{
		if (!_r_listview_isitemselected (hwnd, tab_context->listview_id, i))
			continue;

		if (tab_context->listview_id == IDC_APPS_PROFILE)
		{
			hash_code = (ULONG)_app_listview_getitemcontext (hwnd, tab_context->listview_id, i);
			ptr_app = _app_getappitem (hash_code);

			if (!ptr_app)
				continue;

			if (!ptr_app->is_undeletable) // skip "undeletable" apps
			{
				if (!_r_obj_isempty (ptr_app->guids))
					_r_obj_addarrayitems (guids, ptr_app->guids->items, ptr_app->guids->count);

				_r_listview_deleteitem (hwnd, tab_context->listview_id, i);

				_app_timer_reset (hwnd, ptr_app);
				_app_notify_freeobject (NULL, ptr_app);

				_r_queuedlock_acquireexclusive (&lock_apps);
				_app_freeapplication (hwnd, hash_code);
				_r_queuedlock_releaseexclusive (&lock_apps);
			}

			_r_obj_dereference (ptr_app);
		}
		else if (tab_context->listview_id == IDC_RULES_CUSTOM)
		{
			rule_idx = _app_listview_getitemcontext (hwnd, tab_context->listview_id, i);
			ptr_rule = _app_getrulebyid (rule_idx);

			if (!ptr_rule)
				continue;

			if (!ptr_rule->is_readonly) // skip "read-only" rules
			{
				apps_checker = _r_obj_createhashtable (sizeof (SHORT), 6, NULL);

				if (!_r_obj_isempty (ptr_rule->guids))
					_r_obj_addarrayitems (guids, ptr_rule->guids->items, ptr_rule->guids->count);

				enum_key = 0;

				while (_r_obj_enumhashtable (ptr_rule->apps, NULL, &hash_code, &enum_key))
				{
					_r_obj_addhashtableitem (apps_checker, hash_code, NULL);
				}

				_r_listview_deleteitem (hwnd, tab_context->listview_id, i);

				_r_obj_reference (ptr_rule); // required to dereference later

				_r_queuedlock_acquireexclusive (&lock_rules);
				_r_obj_setlistitem (rules_list, rule_idx, NULL);
				_r_queuedlock_releaseexclusive (&lock_rules);

				enum_key = 0;

				while (_r_obj_enumhashtable (apps_checker, NULL, &hash_code, &enum_key))
				{
					_app_listview_updateitemby_param (hwnd, hash_code, TRUE);
				}
			}

			_r_obj_dereference (ptr_rule);
		}
		else if (tab_context->listview_id == IDC_NETWORK)
		{
			hash_code = (ULONG)_app_listview_getitemcontext (hwnd, tab_context->listview_id, i);
			ptr_network = _app_network_getitem (hash_code);

			if (!ptr_network)
				continue;

			if (ptr_network->af == AF_INET && ptr_network->state == MIB_TCP_STATE_ESTAB)
			{
				RtlZeroMemory (&tcprow, sizeof (MIB_TCPROW));

				tcprow.dwState = MIB_TCP_STATE_DELETE_TCB;
				tcprow.dwLocalAddr = ptr_network->local_addr.S_un.S_addr;
				tcprow.dwLocalPort = _r_byteswap_ushort ((USHORT)ptr_network->local_port);
				tcprow.dwRemoteAddr = ptr_network->remote_addr.S_un.S_addr;
				tcprow.dwRemotePort = _r_byteswap_ushort ((USHORT)ptr_network->remote_port);

				if (SetTcpEntry (&tcprow) == NO_ERROR)
				{
					_r_listview_deleteitem (hwnd, tab_context->listview_id, i);

					_app_network_removeitem (hash_code);
				}
			}

			_r_obj_dereference (ptr_network);
		}
	}

	if (guids)
	{
		if (!_r_obj_isempty (guids))
		{
			if (_wfp_isfiltersinstalled ())
			{
				hengine = _wfp_getenginehandle ();

				_wfp_destroyfilters_array (hengine, guids, DBG_ARG);
			}
		}

		_r_obj_dereference (guids);
	}

	_app_listview_updateby_id (hwnd, tab_context->listview_id, 0);

	_app_profile_save (hwnd);
}

VOID _app_command_disable (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	PITEM_TAB_CONTEXT tab_context;
	PITEM_APP ptr_app;
	ULONG app_hash;
	INT item_id = INT_ERROR;
	BOOL new_val = INT_ERROR;

	tab_context = _app_listview_getcontext (hwnd, INT_ERROR);

	// note: these commands only for apps...
	if (!tab_context || (!(tab_context->listview_id >= IDC_APPS_PROFILE && tab_context->listview_id <= IDC_APPS_UWP) && tab_context->listview_id != IDC_LOG))
		return;

	while ((item_id = _r_listview_getnextselected (hwnd, tab_context->listview_id, item_id)) != INT_ERROR)
	{
		app_hash = _app_listview_getappcontext (hwnd, tab_context->listview_id, item_id);

		ptr_app = _app_getappitem (app_hash);

		if (!ptr_app)
			continue;

		if (ctrl_id == IDM_DISABLENOTIFICATIONS)
		{
			if (new_val == INT_ERROR)
				new_val = !ptr_app->is_silent;

			_app_setappinfo (ptr_app, INFO_IS_SILENT, IntToPtr (new_val));
		}
		else if (ctrl_id == IDM_DISABLEREMOVAL)
		{
			if (_app_isdisabledremoval (app_hash))
				continue;

			if (new_val == INT_ERROR)
				new_val = !ptr_app->is_undeletable;

			_app_setappinfo (ptr_app, INFO_IS_UNDELETABLE, IntToPtr (new_val));
		}
		else if (ctrl_id == IDM_DISABLETIMER)
		{
			_app_timer_reset (hwnd, ptr_app);
		}

		_app_listview_updateitemby_id (hwnd, tab_context->listview_id, item_id);

		_r_obj_dereference (ptr_app);
	}

	_app_listview_updateby_id (hwnd, tab_context->listview_id, 0);

	_app_profile_save (hwnd);
}

VOID _app_command_openeditor (
	_In_ HWND hwnd
)
{
	PITEM_TAB_CONTEXT tab_context;
	PITEM_NETWORK ptr_network;
	PEDITOR_CONTEXT context;
	PITEM_RULE ptr_rule;
	PITEM_LOG ptr_log;
	PR_STRING string;
	ULONG_PTR id_code;
	ULONG hash_code;
	INT item_id = INT_ERROR;

	ptr_rule = _app_addrule (NULL, NULL, NULL, FWP_DIRECTION_OUTBOUND, FWP_ACTION_PERMIT, 0, 0);

	_app_ruleenable (ptr_rule, TRUE, FALSE);

	tab_context = _app_listview_getcontext (hwnd, INT_ERROR);

	if (!tab_context)
		return;

	switch (tab_context->listview_id)
	{
		case IDC_APPS_PROFILE:
		case IDC_APPS_SERVICE:
		case IDC_APPS_UWP:
		{
			while ((item_id = _r_listview_getnextselected (hwnd, tab_context->listview_id, item_id)) != INT_ERROR)
			{
				hash_code = (ULONG)_app_listview_getitemcontext (hwnd, tab_context->listview_id, item_id);

				if (_app_isappfound (hash_code))
					_r_obj_addhashtableitem (ptr_rule->apps, hash_code, NULL);
			}

			break;
		}

		case IDC_NETWORK:
		{
			ptr_rule->action = FWP_ACTION_BLOCK;

			item_id = _r_listview_getnextselected (hwnd, tab_context->listview_id, INT_ERROR);

			if (item_id != INT_ERROR)
			{
				hash_code = (ULONG)_app_listview_getitemcontext (hwnd, tab_context->listview_id, item_id);
				ptr_network = _app_network_getitem (hash_code);

				if (ptr_network)
				{
					if (!ptr_rule->name)
						ptr_rule->name = _r_listview_getitemtext (hwnd, tab_context->listview_id, item_id, 0);

					if (ptr_network->app_hash && !_r_obj_isstringempty (ptr_network->path))
					{
						if (!_app_isappfound (ptr_network->app_hash))
						{
							ptr_network->app_hash = _app_addapplication (hwnd, DATA_UNKNOWN, ptr_network->path, NULL, NULL);

							if (ptr_network->app_hash)
							{
								_app_listview_updateby_param (hwnd, ptr_network->app_hash, PR_SETITEM_UPDATE, TRUE);

								_app_profile_save (hwnd);
							}
						}

						_r_obj_addhashtableitem (ptr_rule->apps, ptr_network->app_hash, NULL);
					}

					ptr_rule->protocol = ptr_network->protocol;

					string = _app_formataddress (ptr_network->af, 0, &ptr_network->remote_addr, ptr_network->remote_port, FMTADDR_AS_RULE);
					_r_obj_movereference ((PVOID_PTR)&ptr_rule->rule_remote, string);

					string = _app_formataddress (ptr_network->af, 0, &ptr_network->local_addr, ptr_network->local_port, FMTADDR_AS_RULE);
					_r_obj_movereference ((PVOID_PTR)&ptr_rule->rule_local, string);

					_r_obj_dereference (ptr_network);
				}
			}

			break;
		}

		case IDC_LOG:
		{
			item_id = _r_listview_getnextselected (hwnd, tab_context->listview_id, INT_ERROR);

			if (item_id != INT_ERROR)
			{
				hash_code = (ULONG)_app_listview_getitemcontext (hwnd, tab_context->listview_id, item_id);
				ptr_log = _app_getlogitem (hash_code);

				if (ptr_log)
				{
					if (!ptr_rule->name)
						ptr_rule->name = _r_listview_getitemtext (hwnd, tab_context->listview_id, item_id, 0);

					if (ptr_log->app_hash && !_r_obj_isstringempty (ptr_log->path))
					{
						if (!_app_isappfound (ptr_log->app_hash))
						{
							ptr_log->app_hash = _app_addapplication (hwnd, DATA_UNKNOWN, ptr_log->path, NULL, NULL);

							if (ptr_log->app_hash)
							{
								_app_listview_updateby_param (hwnd, ptr_log->app_hash, PR_SETITEM_UPDATE, TRUE);

								_app_profile_save (hwnd);
							}
						}

						_r_obj_addhashtableitem (ptr_rule->apps, ptr_log->app_hash, NULL);
					}

					ptr_rule->protocol = ptr_log->protocol;
					ptr_rule->direction = ptr_log->direction;

					string = _app_formataddress (ptr_log->af, 0, &ptr_log->remote_addr, ptr_log->remote_port, FMTADDR_AS_RULE);
					_r_obj_movereference ((PVOID_PTR)&ptr_rule->rule_remote, string);

					string = _app_formataddress (ptr_log->af, 0, &ptr_log->local_addr, ptr_log->local_port, FMTADDR_AS_RULE);
					_r_obj_movereference ((PVOID_PTR)&ptr_rule->rule_local, string);

					_r_obj_dereference (ptr_log);
				}
			}

			break;
		}
	}

	context = _app_editor_createwindow (hwnd, ptr_rule, 0, TRUE);

	if (context)
	{
		_r_queuedlock_acquireexclusive (&lock_rules);
		_r_obj_addlistitem (rules_list, _r_obj_reference (ptr_rule), &id_code);
		_r_queuedlock_releaseexclusive (&lock_rules);

		if (id_code != SIZE_MAX)
		{
			_app_listview_addruleitem (hwnd, ptr_rule, id_code, TRUE);
			_app_listview_updateby_id (hwnd, DATA_LISTVIEW_CURRENT, PR_UPDATE_TYPE);

			_app_profile_save (hwnd);
		}

		_app_editor_deletewindow (context);
	}

	_r_obj_dereference (ptr_rule);
}

VOID _app_command_properties (
	_In_ HWND hwnd
)
{
	PITEM_TAB_CONTEXT tab_context;
	PEDITOR_CONTEXT context;
	PITEM_NETWORK ptr_network;
	PITEM_RULE ptr_rule;
	PITEM_APP ptr_app;
	PITEM_LOG ptr_log;
	ULONG hash_code;
	INT item_id;

	tab_context = _app_listview_getcontext (hwnd, INT_ERROR);

	if (!tab_context)
		return;

	item_id = _r_listview_getnextselected (hwnd, tab_context->listview_id, INT_ERROR);

	if (item_id == INT_ERROR)
		return;

	switch (tab_context->listview_id)
	{
		case IDC_APPS_PROFILE:
		case IDC_APPS_SERVICE:
		case IDC_APPS_UWP:
		{
			hash_code = (ULONG)_app_listview_getitemcontext (hwnd, tab_context->listview_id, item_id);
			ptr_app = _app_getappitem (hash_code);

			if (!ptr_app)
				return;

			context = _app_editor_createwindow (hwnd, ptr_app, 0, FALSE);

			if (context)
			{
				_app_listview_lock (hwnd, tab_context->listview_id, TRUE);
				_app_setappiteminfo (hwnd, tab_context->listview_id, item_id, ptr_app);
				_app_listview_lock (hwnd, tab_context->listview_id, FALSE);

				_app_listview_updateby_id (hwnd, tab_context->listview_id, 0);

				_app_profile_save (hwnd);

				_app_editor_deletewindow (context);
			}

			_r_obj_dereference (ptr_app);

			break;
		}
		case IDC_RULES_BLOCKLIST:
		case IDC_RULES_SYSTEM:
		case IDC_RULES_CUSTOM:
		{
			hash_code = (ULONG)_app_listview_getitemcontext (hwnd, tab_context->listview_id, item_id);
			ptr_rule = _app_getrulebyid (hash_code);

			if (!ptr_rule)
				return;

			context = _app_editor_createwindow (hwnd, ptr_rule, 0, TRUE);

			if (context)
			{
				_app_listview_lock (hwnd, tab_context->listview_id, TRUE);
				_app_setruleiteminfo (hwnd, tab_context->listview_id, item_id, ptr_rule, TRUE);
				_app_listview_lock (hwnd, tab_context->listview_id, FALSE);

				_app_listview_updateby_id (hwnd, tab_context->listview_id, 0);

				_app_profile_save (hwnd);

				_app_editor_deletewindow (context);
			}

			_r_obj_dereference (ptr_rule);

			break;
		}

		case IDC_NETWORK:
		{
			hash_code = (ULONG)_app_listview_getitemcontext (hwnd, tab_context->listview_id, item_id);
			ptr_network = _app_network_getitem (hash_code);

			if (!ptr_network)
				return;

			if (ptr_network->app_hash && !_r_obj_isstringempty (ptr_network->path))
			{
				if (!_app_isappfound (ptr_network->app_hash))
				{
					ptr_network->app_hash = _app_addapplication (hwnd, DATA_UNKNOWN, ptr_network->path, NULL, NULL);

					if (ptr_network->app_hash)
					{
						_app_listview_updateby_param (hwnd, ptr_network->app_hash, PR_SETITEM_UPDATE, TRUE);

						_app_profile_save (hwnd);
					}
				}

				if (ptr_network->app_hash)
					_app_listview_showitemby_param (hwnd, ptr_network->app_hash, TRUE);
			}

			_r_obj_dereference (ptr_network);

			break;
		}

		case IDC_LOG:
		{
			hash_code = (ULONG)_app_listview_getitemcontext (hwnd, tab_context->listview_id, item_id);
			ptr_log = _app_getlogitem (hash_code);

			if (!ptr_log)
				return;

			if (ptr_log->app_hash && !_r_obj_isstringempty (ptr_log->path))
			{
				if (!_app_isappfound (ptr_log->app_hash))
				{
					ptr_log->app_hash = _app_addapplication (hwnd, DATA_UNKNOWN, ptr_log->path, NULL, NULL);

					if (ptr_log->app_hash)
					{
						_app_listview_updateby_param (hwnd, ptr_log->app_hash, PR_SETITEM_UPDATE, TRUE);

						_app_profile_save (hwnd);
					}
				}

				if (ptr_log->app_hash)
					_app_listview_showitemby_param (hwnd, ptr_log->app_hash, TRUE);
			}

			_r_obj_dereference (ptr_log);

			break;
		}
	}
}

VOID _app_command_purgeunused (
	_In_ HWND hwnd
)
{
	R_STRINGBUILDER sb;
	PITEM_APP ptr_app = NULL;
	PR_STRING string = NULL;
	PR_HASHTABLE apps_list;
	PR_ARRAY guids;
	HANDLE hengine;
	ULONG_PTR enum_key = 0;
	ULONG hash_code;

	apps_list = _r_obj_createhashtable (sizeof (ULONG_PTR), 8, NULL);
	guids = _r_obj_createarray (sizeof (GUID), 10, NULL);

	_r_obj_initializestringbuilder (&sb, 256);

	_r_queuedlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtablepointer (apps_table, (PVOID_PTR)&ptr_app, &hash_code, &enum_key))
	{
		if (!_app_isappunused (ptr_app) || ptr_app->type == DATA_APP_SERVICE || ptr_app->type == DATA_APP_UWP)
			continue;

		_app_deleteappitem (hwnd, ptr_app->type, hash_code);

		_app_timer_reset (NULL, ptr_app);

		_app_notify_freeobject (NULL, ptr_app);

		if (!_r_obj_isempty (ptr_app->guids))
			_r_obj_addarrayitems (guids, ptr_app->guids->items, ptr_app->guids->count);

		_r_obj_addhashtableitem (apps_list, hash_code, NULL);

		string = _app_getappdisplayname (ptr_app, FALSE);

		if (string)
		{
			_r_obj_appendstringbuilder (&sb, L"- ");

			_r_obj_appendstringbuilder2 (&sb, &string->sr);
			_r_obj_appendstringbuilder (&sb, SZ_CRLF);

			_r_obj_dereference (string);
		}
	}

	_r_queuedlock_releaseshared (&lock_apps);

	if (_r_obj_gethashtablesize (apps_list))
	{
		enum_key = 0;

		string = _r_obj_finalstringbuilder (&sb);

		_r_str_trimstring2 (&string->sr, SZ_CRLF, PR_TRIM_END_ONLY);

		if (_r_show_confirmmessage (hwnd, _r_locale_getstring (IDS_PURGE_UNUSED), string->buffer, L"ConfirmUnused", FALSE))
		{
			_r_queuedlock_acquireexclusive (&lock_apps);

			while (_r_obj_enumhashtable (apps_list, NULL, &hash_code, &enum_key))
			{
				_app_freeapplication (hwnd, hash_code);
			}

			_r_queuedlock_releaseexclusive (&lock_apps);

			if (!_r_obj_isempty2 (guids) && _wfp_isfiltersinstalled ())
			{
				hengine = _wfp_getenginehandle ();

				_wfp_destroyfilters_array (hengine, guids, DBG_ARG);
			}

			_app_listview_updateby_id (hwnd, DATA_LISTVIEW_CURRENT, PR_UPDATE_TYPE | PR_UPDATE_FORCE);

			_app_profile_save (hwnd);
		}
	}

	_r_obj_deletestringbuilder (&sb);

	_r_obj_dereference (apps_list);
	_r_obj_dereference (guids);
}

VOID _app_command_purgetimers (
	_In_ HWND hwnd
)
{
	PITEM_APP ptr_app = NULL;
	R_STRINGBUILDER sb;
	PR_STRING string;
	PR_STRING path;
	HANDLE hengine;
	PR_LIST rules;
	ULONG_PTR enum_key = 0;

	if (!_app_istimersactive ())
		return;

	rules = _r_obj_createlist (10, NULL);

	_r_obj_initializestringbuilder (&sb, 0x100);

	_r_queuedlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtablepointer (apps_table, (PVOID_PTR)&ptr_app, NULL, &enum_key))
	{
		if (_app_istimerset (ptr_app))
		{
			_r_obj_addlistitem (rules, ptr_app, NULL);

			path = _app_getappdisplayname (ptr_app, FALSE);

			if (path)
			{
				_r_obj_appendstringbuilder (&sb, L"- ");

				_r_obj_appendstringbuilder2 (&sb, &path->sr);

				string = _r_format_interval (ptr_app->timer - _r_unixtime_now (), TRUE);

				if (string)
				{
					_r_obj_appendstringbuilder (&sb, L" (");
					_r_obj_appendstringbuilder2 (&sb, &string->sr);
					_r_obj_appendstringbuilder (&sb, L")");

					_r_obj_dereference (string);
				}

				_r_obj_appendstringbuilder (&sb, SZ_CRLF);

				_r_obj_dereference (path);
			}
		}
	}

	_r_queuedlock_releaseshared (&lock_apps);

	if (!_r_obj_isempty2 (rules))
	{
		string = _r_obj_finalstringbuilder (&sb);

		_r_str_trimstring2 (&string->sr, SZ_CRLF, PR_TRIM_END_ONLY);

		if (_r_show_confirmmessage (hwnd, _r_locale_getstring (IDS_QUESTION_TIMERS), string->buffer, L"ConfirmTimers", FALSE))
		{
			if (_wfp_isfiltersinstalled ())
			{
				hengine = _wfp_getenginehandle ();

				_wfp_create3filters (hengine, rules, DBG_ARG, FALSE);
			}

			for (ULONG_PTR i = 0; i < _r_obj_getlistsize (rules); i++)
			{
				ptr_app = _r_obj_getlistitem (rules, i);

				if (ptr_app)
					_app_timer_reset (hwnd, ptr_app);
			}

			_app_listview_updateby_id (hwnd, DATA_LISTVIEW_CURRENT, PR_UPDATE_TYPE | PR_UPDATE_FORCE);

			_app_profile_save (hwnd);
		}
	}

	_r_obj_deletestringbuilder (&sb);

	_r_obj_dereference (rules);
}

VOID _app_command_selectfont (
	_In_ HWND hwnd
)
{
	CHOOSEFONT cf = {0};
	LOGFONT lf = {0};
	LONG dpi_value;
	UINT flags;

	cf.lStructSize = sizeof (CHOOSEFONT);
	cf.hwndOwner = hwnd;
	cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_NOSCRIPTSEL | CF_LIMITSIZE | CF_NOVERTFONTS;
	cf.nSizeMax = 16;
	cf.nSizeMin = 8;
	cf.lpLogFont = &lf;

	dpi_value = _r_dc_getwindowdpi (hwnd);

	_r_config_getfont (L"Font", &lf, dpi_value, NULL);

	if (!ChooseFontW (&cf))
		return;

	_r_config_setfont (L"Font", &lf, dpi_value, NULL);

	_app_listview_loadfont (dpi_value, TRUE);
	_app_listview_updateby_id (hwnd, DATA_LISTVIEW_CURRENT, PR_UPDATE_TYPE | PR_UPDATE_FORCE);

	flags = RDW_NOFRAME | RDW_NOINTERNALPAINT | RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN;

	RedrawWindow (hwnd, NULL, NULL, flags);
}
