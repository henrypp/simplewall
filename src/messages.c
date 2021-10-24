// simplewall
// Copyright (c) 2021 Henry++

#include "global.h"

VOID _app_message_contextmenu (_In_ HWND hwnd, _In_ LPNMITEMACTIVATE lpnmlv)
{
	HMENU hmenu;
	HMENU hsubmenu_rules;
	HMENU hsubmenu_timers;

	PITEM_APP ptr_app;
	PITEM_NETWORK ptr_network;

	PR_STRING localized_string;
	PR_STRING column_text;

	ULONG_PTR hash_code;

	INT listview_id;
	INT lv_column_current;
	INT command_id;

	BOOLEAN is_readonly;

	if (lpnmlv->iItem == -1)
		return;

	listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

	if (!(listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_LOG))
		return;

	hmenu = CreatePopupMenu ();

	if (!hmenu)
		return;

	hsubmenu_rules = NULL;
	hsubmenu_timers = NULL;

	localized_string = NULL;
	column_text = NULL;

	hash_code = _app_getlistviewitemcontext (hwnd, listview_id, lpnmlv->iItem);
	lv_column_current = lpnmlv->iSubItem;

	if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
	{
		hsubmenu_rules = CreatePopupMenu ();
		hsubmenu_timers = CreatePopupMenu ();

		ptr_app = _app_getappitem (hash_code);

		_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_EDIT2), L"...\tEnter"));
		AppendMenu (hmenu, MF_STRING, IDM_PROPERTIES, localized_string->buffer);

		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

		// show rules
		if (hsubmenu_rules)
		{
			AppendMenu (hmenu, MF_POPUP, (UINT_PTR)hsubmenu_rules, _r_locale_getstring (IDS_TRAY_RULES));

			AppendMenu (hsubmenu_rules, MF_STRING, IDM_DISABLENOTIFICATIONS, _r_locale_getstring (IDS_DISABLENOTIFICATIONS));

			_app_generate_rulescontrol (hsubmenu_rules, hash_code);
		}

		// show timers
		if (hsubmenu_timers)
		{
			AppendMenu (hmenu, MF_POPUP, (UINT_PTR)hsubmenu_timers, _r_locale_getstring (IDS_TIMER));

			AppendMenu (hsubmenu_timers, MF_STRING, IDM_DISABLETIMER, _r_locale_getstring (IDS_DISABLETIMER));
			AppendMenu (hsubmenu_timers, MF_SEPARATOR, 0, NULL);

			if (ptr_app)
				_app_generate_timerscontrol (hsubmenu_timers, ptr_app);
		}

		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

		_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_EXPLORE), L"\tCtrl+E"));
		AppendMenu (hmenu, MF_STRING, IDM_EXPLORE, localized_string->buffer);

		_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_DELETE), L"\tDel"));
		AppendMenu (hmenu, MF_STRING, IDM_DELETE, localized_string->buffer);

		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);
		AppendMenu (hmenu, MF_STRING, IDM_CHECK, _r_locale_getstring (IDS_CHECK));
		AppendMenu (hmenu, MF_STRING, IDM_UNCHECK, _r_locale_getstring (IDS_UNCHECK));
		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

		_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_SELECT_ALL), L"\tCtrl+A"));
		AppendMenu (hmenu, MF_STRING, IDM_SELECT_ALL, localized_string->buffer);

		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

		_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_COPY), L"\tCtrl+C"));
		AppendMenu (hmenu, MF_STRING, IDM_COPY, localized_string->buffer);

		column_text = _r_listview_getcolumntext (hwnd, listview_id, lv_column_current);

		if (column_text)
		{
			_r_obj_movereference (&localized_string, _r_obj_concatstrings (4, _r_locale_getstring (IDS_COPY), L" \"", column_text->buffer, L"\""));
			AppendMenu (hmenu, MF_STRING, IDM_COPY2, localized_string->buffer);

			_r_obj_dereference (column_text);
		}

		SetMenuDefaultItem (hmenu, IDM_PROPERTIES, MF_BYCOMMAND);

		if (ptr_app)
		{
			_r_menu_checkitem (hmenu, IDM_DISABLENOTIFICATIONS, 0, MF_BYCOMMAND, PtrToInt (_app_getappinfo (ptr_app, INFO_IS_SILENT)) != FALSE);

			_r_obj_dereference (ptr_app);
		}

		if (listview_id != IDC_APPS_PROFILE)
			_r_menu_enableitem (hmenu, IDM_DELETE, MF_BYCOMMAND, FALSE);
	}
	else if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM)
	{
		if (listview_id == IDC_RULES_CUSTOM)
		{
			_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_ADD), L"..."));
			AppendMenu (hmenu, MF_STRING, IDM_OPENRULESEDITOR, localized_string->buffer);
		}

		_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_EDIT2), L"...\tEnter"));
		AppendMenu (hmenu, MF_STRING, IDM_PROPERTIES, localized_string->buffer);

		if (listview_id == IDC_RULES_CUSTOM)
		{
			_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_DELETE), L"\tDel"));
			AppendMenu (hmenu, MF_STRING, IDM_DELETE, localized_string->buffer);

			is_readonly = !!(PtrToInt (_app_getruleinfobyid (hash_code, INFO_IS_READONLY)));

			if (is_readonly)
				_r_menu_enableitem (hmenu, IDM_DELETE, MF_BYCOMMAND, FALSE);
		}

		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);
		AppendMenu (hmenu, MF_STRING, IDM_CHECK, _r_locale_getstring (IDS_CHECK));
		AppendMenu (hmenu, MF_STRING, IDM_UNCHECK, _r_locale_getstring (IDS_UNCHECK));
		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

		_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_SELECT_ALL), L"\tCtrl+A"));
		AppendMenu (hmenu, MF_STRING, IDM_SELECT_ALL, localized_string->buffer);

		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

		_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_COPY), L"\tCtrl+C"));
		AppendMenu (hmenu, MF_STRING, IDM_COPY, localized_string->buffer);

		column_text = _r_listview_getcolumntext (hwnd, listview_id, lv_column_current);

		if (column_text)
		{
			_r_obj_movereference (&localized_string, _r_obj_concatstrings (4, _r_locale_getstring (IDS_COPY), L" \"", column_text->buffer, L"\""));
			AppendMenu (hmenu, MF_STRING, IDM_COPY2, localized_string->buffer);

			_r_obj_dereference (column_text);
		}

		SetMenuDefaultItem (hmenu, IDM_PROPERTIES, MF_BYCOMMAND);
	}
	else if (listview_id == IDC_NETWORK)
	{
		_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_SHOWINLIST), L"\tEnter"));
		AppendMenu (hmenu, MF_STRING, IDM_PROPERTIES, localized_string->buffer);

		_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_OPENRULESEDITOR), L"..."));
		AppendMenu (hmenu, MF_STRING, IDM_OPENRULESEDITOR, localized_string->buffer);

		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

		_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_EXPLORE), L"\tCtrl+E"));
		AppendMenu (hmenu, MF_STRING, IDM_EXPLORE, localized_string->buffer);

		AppendMenu (hmenu, MF_STRING, IDM_DELETE, _r_locale_getstring (IDS_NETWORK_CLOSE));
		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

		_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_SELECT_ALL), L"\tCtrl+A"));
		AppendMenu (hmenu, MF_STRING, IDM_SELECT_ALL, localized_string->buffer);

		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

		_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_COPY), L"\tCtrl+C"));
		AppendMenu (hmenu, MF_STRING, IDM_COPY, localized_string->buffer);

		column_text = _r_listview_getcolumntext (hwnd, listview_id, lv_column_current);

		if (column_text)
		{
			_r_obj_movereference (&localized_string, _r_obj_concatstrings (4, _r_locale_getstring (IDS_COPY), L" \"", column_text->buffer, L"\""));
			AppendMenu (hmenu, MF_STRING, IDM_COPY2, localized_string->buffer);

			_r_obj_dereference (column_text);
		}

		SetMenuDefaultItem (hmenu, IDM_PROPERTIES, MF_BYCOMMAND);

		ptr_network = _app_getnetworkitem (hash_code);

		if (ptr_network)
		{
			if (ptr_network->af != AF_INET || ptr_network->state != MIB_TCP_STATE_ESTAB)
			{
				_r_menu_enableitem (hmenu, IDM_DELETE, MF_BYCOMMAND, FALSE);
			}

			_r_obj_dereference (ptr_network);
		}
	}
	else if (listview_id == IDC_LOG)
	{
		_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_SHOWINLIST), L"\tEnter"));
		AppendMenu (hmenu, MF_STRING, IDM_PROPERTIES, localized_string->buffer);

		_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_OPENRULESEDITOR), L"..."));
		AppendMenu (hmenu, MF_STRING, IDM_OPENRULESEDITOR, localized_string->buffer);

		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

		_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_EXPLORE), L"\tCtrl+E"));
		AppendMenu (hmenu, MF_STRING, IDM_EXPLORE, localized_string->buffer);

		_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_LOGCLEAR), L"\tCtrl+X"));
		AppendMenu (hmenu, MF_STRING, IDM_TRAY_LOGCLEAR, localized_string->buffer);

		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

		_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_SELECT_ALL), L"\tCtrl+A"));
		AppendMenu (hmenu, MF_STRING, IDM_SELECT_ALL, localized_string->buffer);

		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

		_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_COPY), L"\tCtrl+C"));
		AppendMenu (hmenu, MF_STRING, IDM_COPY, localized_string->buffer);

		column_text = _r_listview_getcolumntext (hwnd, listview_id, lv_column_current);

		if (column_text)
		{
			_r_obj_movereference (&localized_string, _r_obj_concatstrings (4, _r_locale_getstring (IDS_COPY), L" \"", column_text->buffer, L"\""));
			AppendMenu (hmenu, MF_STRING, IDM_COPY2, localized_string->buffer);

			_r_obj_dereference (column_text);
		}

		SetMenuDefaultItem (hmenu, IDM_PROPERTIES, MF_BYCOMMAND);
	}

	command_id = _r_menu_popup (hmenu, hwnd, NULL, FALSE);

	if (command_id)
		PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (command_id, 0), (LPARAM)lv_column_current);

	if (hsubmenu_rules)
		DestroyMenu (hsubmenu_rules);

	if (hsubmenu_timers)
		DestroyMenu (hsubmenu_timers);

	DestroyMenu (hmenu);

	if (localized_string)
		_r_obj_dereference (localized_string);
}

VOID _app_message_contextmenu_columns (_In_ HWND hwnd, _In_ LPNMHDR nmlp)
{
	HWND hlistview;
	HMENU hmenu;
	PR_STRING column_text;
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
			AppendMenu (hmenu, MF_STRING, IDX_COLUMN + (UINT_PTR)i, column_text->buffer);

			_r_menu_checkitem (hmenu, IDX_COLUMN + i, 0, MF_BYCOMMAND, TRUE);

			_r_obj_dereference (column_text);
		}

	}

	_r_menu_popup (hmenu, hwnd, NULL, TRUE);

	DestroyMenu (hmenu);
}

VOID _app_message_traycontextmenu (_In_ HWND hwnd)
{
	HMENU hmenu;
	HMENU hsubmenu;
	BOOLEAN is_filtersinstalled;

	SetForegroundWindow (hwnd); // don't touch

#define NOTIFICATIONS_ID 4
#define LOGGING_ID 5
#define ERRLOG_ID 6

	hmenu = LoadMenu (NULL, MAKEINTRESOURCE (IDM_TRAY));
	hsubmenu = GetSubMenu (hmenu, 0);

	is_filtersinstalled = (_wfp_isfiltersinstalled () != INSTALL_DISABLED);

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
}

VOID _app_message_dpichanged (_In_ HWND hwnd)
{
	PR_STRING localized_string = NULL;

	_app_imagelist_init (hwnd);

	// reset toolbar information
	_app_setinterfacestate (hwnd);

	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_REFRESH, _r_locale_getstring (IDS_REFRESH), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, 0, I_IMAGENONE);
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_SETTINGS, _r_locale_getstring (IDS_SETTINGS), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, 0, I_IMAGENONE);

	_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_OPENRULESEDITOR), L"..."));
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_OPENRULESEDITOR, localized_string->buffer, BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLENOTIFICATIONS_CHK, _r_locale_getstring (IDS_ENABLENOTIFICATIONS_CHK), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLELOG_CHK, _r_locale_getstring (IDS_ENABLELOG_CHK), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLEUILOG_CHK, _r_locale_getstring (IDS_ENABLEUILOG_CHK), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_LOGSHOW), L" (Ctrl+I)"));
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_LOGSHOW, localized_string->buffer, BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_LOGCLEAR), L" (Ctrl+X)"));
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_LOGCLEAR, localized_string->buffer, BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_DONATE, _r_locale_getstring (IDS_DONATE), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_app_toolbar_resize ();

	_app_listviewloadfont (hwnd, TRUE);
	_app_updatelistviewbylparam (hwnd, DATA_LISTVIEW_CURRENT, PR_UPDATE_TYPE | PR_UPDATE_FORCE);

	if (localized_string)
		_r_obj_dereference (localized_string);
}

LONG_PTR _app_message_custdraw (_In_ HWND hwnd, _In_ LPNMLVCUSTOMDRAW lpnmlv)
{
	INT ctrl_id;

	switch (lpnmlv->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
		{
			ctrl_id = (INT)(INT_PTR)lpnmlv->nmcd.hdr.idFrom;

			if (ctrl_id == IDC_TOOLBAR)
			{
				return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
			}

			return CDRF_NOTIFYITEMDRAW;
		}

		case CDDS_ITEMPREPAINT:
		{
			if (lpnmlv->dwItemType != LVCDI_ITEM)
				return CDRF_DODEFAULT;

			ctrl_id = (INT)(INT_PTR)lpnmlv->nmcd.hdr.idFrom;

			if (ctrl_id == IDC_TOOLBAR)
			{
				TBBUTTONINFO tbi = {0};

				tbi.cbSize = sizeof (tbi);
				tbi.dwMask = TBIF_STYLE | TBIF_STATE | TBIF_IMAGE;

				if ((INT)SendMessage (lpnmlv->nmcd.hdr.hwndFrom, TB_GETBUTTONINFO, (WPARAM)lpnmlv->nmcd.dwItemSpec, (LPARAM)&tbi) == -1)
					break;

				if ((tbi.fsState & TBSTATE_ENABLED) == 0)
				{
					INT icon_size_x = 0;
					INT icon_size_y = 0;

					_r_dc_fixwindowfont (lpnmlv->nmcd.hdc, lpnmlv->nmcd.hdr.hwndFrom); // fix

					SetBkMode (lpnmlv->nmcd.hdc, TRANSPARENT);
					SetTextColor (lpnmlv->nmcd.hdc, GetSysColor (COLOR_GRAYTEXT));

					if (tbi.iImage != I_IMAGENONE)
					{
						HIMAGELIST himglist;
						ULONG padding;
						UINT rebar_height;

						himglist = (HIMAGELIST)SendMessage (lpnmlv->nmcd.hdr.hwndFrom, TB_GETIMAGELIST, 0, 0);

						if (himglist)
						{
							padding = (ULONG)SendMessage (lpnmlv->nmcd.hdr.hwndFrom, TB_GETPADDING, 0, 0);
							rebar_height = (UINT)SendMessage (GetParent (lpnmlv->nmcd.hdr.hwndFrom), RB_GETBARHEIGHT, 0, 0);

							ImageList_GetIconSize (himglist, &icon_size_x, &icon_size_y);

							_r_dc_drawimagelisticon (
								lpnmlv->nmcd.hdc,
								himglist,
								tbi.iImage,
								lpnmlv->nmcd.rc.left + (LOWORD (padding) / 2),
								(rebar_height / 2) - (icon_size_y / 2),
								ILS_SATURATE, // grayscale
								ILD_NORMAL | ILD_ASYNC
							);
						}
					}

					if ((tbi.fsStyle & BTNS_SHOWTEXT) == BTNS_SHOWTEXT)
					{
						WCHAR text[128] = {0};
						SendMessage (lpnmlv->nmcd.hdr.hwndFrom, TB_GETBUTTONTEXT, (WPARAM)lpnmlv->nmcd.dwItemSpec, (LPARAM)text);

						if (tbi.iImage != I_IMAGENONE)
							lpnmlv->nmcd.rc.left += icon_size_x;

						DrawTextEx (lpnmlv->nmcd.hdc, text, (INT)_r_str_getlength (text), &lpnmlv->nmcd.rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_HIDEPREFIX, NULL);
					}

					return CDRF_SKIPDEFAULT;
				}
			}
			else
			{
				if (!_r_config_getboolean (L"IsEnableHighlighting", TRUE))
					return CDRF_DODEFAULT;

				COLORREF new_clr = 0;
				ULONG_PTR index;
				ULONG view_type = _r_listview_getview (hwnd, ctrl_id);
				BOOLEAN is_tableview = (view_type == LV_VIEW_DETAILS || view_type == LV_VIEW_SMALLICON || view_type == LV_VIEW_TILE);
				BOOLEAN is_systemapp = FALSE;
				BOOLEAN is_validconnection = FALSE;

				index = _app_getlistviewitemcontext (hwnd, ctrl_id, (INT)(INT_PTR)lpnmlv->nmcd.dwItemSpec);

				if ((ctrl_id >= IDC_APPS_PROFILE && ctrl_id <= IDC_APPS_UWP) || ctrl_id == IDC_RULE_APPS_ID || ctrl_id == IDC_NETWORK || ctrl_id == IDC_LOG)
				{
					ULONG_PTR app_hash = 0;

					if (ctrl_id == IDC_NETWORK)
					{
						PITEM_NETWORK ptr_network = _app_getnetworkitem (index);

						if (ptr_network)
						{
							app_hash = ptr_network->app_hash;
							is_systemapp = _app_isappfromsystem (ptr_network->path, app_hash);
							is_validconnection = ptr_network->is_connection;

							_r_obj_dereference (ptr_network);
						}
					}
					else if (ctrl_id == IDC_LOG)
					{
						PITEM_LOG ptr_log = _app_getlogitem (index);

						if (ptr_log)
						{
							app_hash = _app_getlogapp (index);
							is_systemapp = _app_isappfromsystem (ptr_log->path, app_hash);

							_r_obj_dereference (ptr_log);
						}
					}
					else
					{
						PR_STRING real_path;

						app_hash = index;
						is_validconnection = _app_isapphaveconnection (app_hash);

						real_path = _app_getappinfobyhash (app_hash, INFO_PATH);

						if (real_path)
						{
							is_systemapp = _app_isappfromsystem (real_path, app_hash);

							_r_obj_dereference (real_path);
						}
					}

					if (app_hash)
					{
						new_clr = _app_getappcolor (ctrl_id, app_hash, is_systemapp, is_validconnection);
					}
				}
				else if (ctrl_id >= IDC_RULES_BLOCKLIST && ctrl_id <= IDC_RULES_CUSTOM || ctrl_id == IDC_APP_RULES_ID)
				{
					new_clr = _app_getrulecolor (ctrl_id, index);
				}
				else if (ctrl_id == IDC_COLORS)
				{
					PITEM_COLOR ptr_clr = (PITEM_COLOR)lpnmlv->nmcd.lItemlParam;

					if (ptr_clr)
					{
						new_clr = ptr_clr->new_clr ? ptr_clr->new_clr : ptr_clr->default_clr;
					}
				}
				else
				{
					break;
				}

				if (new_clr)
				{
					lpnmlv->clrText = _r_dc_getcolorbrightness (new_clr);
					lpnmlv->clrTextBk = new_clr;

					if (is_tableview)
						_r_dc_fillrect (lpnmlv->nmcd.hdc, &lpnmlv->nmcd.rc, new_clr);

					return CDRF_NEWFONT;
				}
			}

			break;
		}
	}

	return CDRF_DODEFAULT;
}

VOID _app_displayinfoapp_callback (_In_ INT listview_id, _In_ PITEM_APP ptr_app, _Inout_ LPNMLVDISPINFOW lpnmlv)
{
	PR_STRING string;

	// set text
	if ((lpnmlv->item.mask & LVIF_TEXT))
	{
		switch (lpnmlv->item.iSubItem)
		{
			case 0:
			{
				_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, _app_getappdisplayname (ptr_app, FALSE));
				break;
			}

			case 1:
			{
				string = _r_format_unixtime_ex (ptr_app->timestamp, FDTF_SHORTDATE | FDTF_SHORTTIME);

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
	if ((lpnmlv->item.mask & LVIF_IMAGE))
	{
		PVOID ptr = _app_getappinfoparam2 (ptr_app->app_hash, INFO_ICON_ID);

		lpnmlv->item.iImage = PtrToInt (ptr);
	}

	// set group id
	if ((lpnmlv->item.mask & LVIF_GROUPID))
	{
		if (_app_islistviewitemhidden (lpnmlv->item.lParam))
		{
			lpnmlv->item.iGroupId = LV_HIDDEN_GROUP_ID;
		}
		else
		{
			if (listview_id == IDC_RULE_APPS_ID)
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
						lpnmlv->item.iGroupId = 3;
					}
					else
					{
						lpnmlv->item.iGroupId = 2;
					}
				}
			}
		}
	}
}

VOID _app_displayinforule_callback (_In_ INT listview_id, _In_ PITEM_RULE ptr_rule, _Inout_ LPNMLVDISPINFOW lpnmlv)
{
	PR_STRING string;
	LPCWSTR name;

	// set text
	if ((lpnmlv->item.mask & LVIF_TEXT))
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

				break;
			}

			case 1:
			{
				if (ptr_rule->protocol)
				{
					name = _app_getprotoname (ptr_rule->protocol, AF_UNSPEC, SZ_UNKNOWN);
				}
				else
				{
					name = _r_locale_getstring (IDS_ANY);
				}

				if (name)
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, name);

				break;
			}

			case 2:
			{
				string = _app_getdirectionname (ptr_rule->direction, FALSE, TRUE);

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
	if ((lpnmlv->item.mask & LVIF_IMAGE))
	{
		lpnmlv->item.iImage = (ptr_rule->action == FWP_ACTION_BLOCK) ? 1 : 0;
	}

	// set group id
	if ((lpnmlv->item.mask & LVIF_GROUPID))
	{
		if (_app_islistviewitemhidden (lpnmlv->item.lParam))
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

				lpnmlv->item.iGroupId = ptr_rule->is_enabled ? 0 : 2;
			}
		}
	}
}

VOID _app_displayinfonetwork_callback (_In_ PITEM_NETWORK ptr_network, _Inout_ LPNMLVDISPINFOW lpnmlv)
{
	PR_STRING string;
	LPCWSTR name;

	// set text
	if ((lpnmlv->item.mask & LVIF_TEXT))
	{
		switch (lpnmlv->item.iSubItem)
		{
			case 0:
			{
				_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, _r_path_getbasename (ptr_network->path->buffer));
				break;
			}

			case 1:
			{
				if (ptr_network->local_addr_str)
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, ptr_network->local_addr_str->buffer);
				}

				break;
			}

			case 2:
			{
				if (ptr_network->local_host_str)
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, ptr_network->local_host_str->buffer);
				}
				else
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, SZ_LOADING);
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
				if (ptr_network->remote_addr_str)
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, ptr_network->remote_addr_str->buffer);
				}

				break;
			}

			case 5:
			{
				if (ptr_network->remote_host_str)
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, ptr_network->remote_host_str->buffer);
				}
				else
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, SZ_LOADING);
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
				name = _app_getprotoname (ptr_network->protocol, ptr_network->af, NULL);

				if (name)
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, name);

				break;
			}

			case 8:
			{
				name = _app_getconnectionstatusname (ptr_network->state);

				if (name)
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, name);

				break;
			}
		}
	}

	// set image
	if ((lpnmlv->item.mask & LVIF_IMAGE))
	{
		PVOID ptr = _app_getappinfoparam2 (ptr_network->app_hash, INFO_ICON_ID);

		lpnmlv->item.iImage = PtrToInt (ptr);
	}

	// set group id
	if ((lpnmlv->item.mask & LVIF_GROUPID))
	{
		if (_app_islistviewitemhidden (lpnmlv->item.lParam))
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

VOID _app_displayinfolog_callback (_Inout_ LPNMLVDISPINFOW lpnmlv, _In_opt_ PITEM_APP ptr_app, _In_ PITEM_LOG ptr_log)
{
	PR_STRING string;
	LPCWSTR name;

	// set text
	if ((lpnmlv->item.mask & LVIF_TEXT))
	{
		switch (lpnmlv->item.iSubItem)
		{
			case 0:
			{
				if (ptr_app)
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, _app_getappdisplayname (ptr_app, TRUE));
				}
				else if (ptr_log->path)
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, _r_path_getbasename (ptr_log->path->buffer));
				}
				else
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, SZ_EMPTY);
				}

				break;
			}

			case 1:
			{
				if (ptr_log->local_addr_str)
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, ptr_log->local_addr_str->buffer);
				}

				break;
			}

			case 2:
			{
				if (ptr_log->local_host_str)
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, ptr_log->local_host_str->buffer);
				}
				else
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, SZ_LOADING);
				}

				break;
			}

			case 3:
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

			case 4:
			{
				if (ptr_log->remote_addr_str)
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, ptr_log->remote_addr_str->buffer);
				}

				break;
			}

			case 5:
			{
				if (ptr_log->remote_host_str)
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, ptr_log->remote_host_str->buffer);
				}
				else
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, SZ_LOADING);
				}

				break;
			}

			case 6:
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

			case 7:
			{
				name = _app_getprotoname (ptr_log->protocol, ptr_log->af, NULL);

				if (name)
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, name);

				break;
			}

			case 8:
			{
				string = _app_getdirectionname (ptr_log->direction, ptr_log->is_loopback, FALSE);

				if (string)
				{
					_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, string->buffer);
					_r_obj_dereference (string);
				}

				break;
			}

			case 9:
			{
				string = _r_obj_concatstrings (2, ptr_log->is_allow ? L"[A] " : L"[B] ", _r_obj_getstringorempty (ptr_log->filter_name));

				_r_str_copy (lpnmlv->item.pszText, lpnmlv->item.cchTextMax, string->buffer);
				_r_obj_dereference (string);

				break;
			}

			case 10:
			{
				string = _r_format_unixtime_ex (ptr_log->timestamp, FDTF_SHORTDATE | FDTF_SHORTTIME);

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
	if ((lpnmlv->item.mask & LVIF_IMAGE))
	{
		PVOID ptr = _app_getappinfoparam2 (ptr_log->app_hash, INFO_ICON_ID);

		lpnmlv->item.iImage = PtrToInt (ptr);
	}

	// set group id
	if ((lpnmlv->item.mask & LVIF_GROUPID))
	{
		if (_app_islistviewitemhidden (lpnmlv->item.lParam))
		{
			lpnmlv->item.iGroupId = LV_HIDDEN_GROUP_ID;
		}
		else
		{
			lpnmlv->item.iGroupId = 0;
		}
	}
}

BOOLEAN _app_message_displayinfo (_In_ HWND hwnd, _In_ INT listview_id, _Inout_ LPNMLVDISPINFOW lpnmlv)
{
	PITEM_NETWORK ptr_network;
	PITEM_RULE ptr_rule;
	PITEM_APP ptr_app;
	PITEM_LOG ptr_log;
	ULONG_PTR index;

	index = _app_getlistviewitemcontext (hwnd, listview_id, lpnmlv->item.iItem);

	if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP || listview_id == IDC_RULE_APPS_ID)
	{
		ptr_app = _app_getappitem (index);

		if (ptr_app)
		{
			_app_displayinfoapp_callback (listview_id, ptr_app, lpnmlv);
			_r_obj_dereference (ptr_app);

			return TRUE;
		}
	}
	else if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM || listview_id == IDC_APP_RULES_ID)
	{
		ptr_rule = _app_getrulebyid (index);

		if (ptr_rule)
		{
			_app_displayinforule_callback (listview_id, ptr_rule, lpnmlv);
			_r_obj_dereference (ptr_rule);

			return TRUE;
		}
	}
	else if (listview_id == IDC_NETWORK)
	{
		ptr_network = _app_getnetworkitem (index);

		if (ptr_network)
		{
			_app_displayinfonetwork_callback (ptr_network, lpnmlv);
			_r_obj_dereference (ptr_network);

			return TRUE;
		}
	}
	else if (listview_id == IDC_LOG)
	{
		ptr_log = _app_getlogitem (index);

		if (ptr_log)
		{
			ptr_app = _app_getappitem (ptr_log->app_hash);

			_app_displayinfolog_callback (lpnmlv, ptr_app, ptr_log);

			if (ptr_app)
				_r_obj_dereference (ptr_app);

			_r_obj_dereference (ptr_log);

			return TRUE;
		}
	}

	return FALSE;
}

VOID _app_message_initialize (_In_ HWND hwnd)
{
	HICON hicon;
	HMENU hmenu;

	LONG dpi_value;

	LONG icon_small_x;
	LONG icon_small_y;

	UINT icon_id;

	dpi_value = _r_dc_getsystemdpi ();

	icon_small_x = _r_dc_getsystemmetrics (SM_CXSMICON, dpi_value);
	icon_small_y = _r_dc_getsystemmetrics (SM_CYSMICON, dpi_value);

	icon_id = _wfp_isfiltersinstalled () ? IDI_ACTIVE : IDI_INACTIVE;

	hicon = _r_sys_loadsharedicon (_r_sys_getimagebase (), MAKEINTRESOURCE (icon_id), icon_small_x, icon_small_y);

	_r_tray_create (hwnd, &GUID_TrayIcon, WM_TRAYICON, hicon, _r_app_getname (), FALSE);

	hmenu = GetMenu (hwnd);

	if (hmenu)
	{
		if (_r_config_getboolean (L"IsInternalRulesDisabled", FALSE))
			_r_menu_enableitem (hmenu, 4, MF_BYPOSITION, FALSE);

		_r_menu_checkitem (hmenu, IDM_ALWAYSONTOP_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"AlwaysOnTop", FALSE));
		_r_menu_checkitem (hmenu, IDM_SHOWFILENAMESONLY_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"ShowFilenames", TRUE));
		_r_menu_checkitem (hmenu, IDM_AUTOSIZECOLUMNS_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"AutoSizeColumns", TRUE));

		{
			UINT menu_id;
			INT view_type;

			view_type = _r_calc_clamp (_r_config_getinteger (L"ViewType", LV_VIEW_DETAILS), LV_VIEW_ICON, LV_VIEW_MAX);

			if (view_type == LV_VIEW_ICON)
				menu_id = IDM_VIEW_ICON;

			else if (view_type == LV_VIEW_TILE)
				menu_id = IDM_VIEW_TILE;

			else
				menu_id = IDM_VIEW_DETAILS;

			_r_menu_checkitem (hmenu, IDM_VIEW_DETAILS, IDM_VIEW_TILE, MF_BYCOMMAND, menu_id);
		}

		{
			UINT menu_id;
			INT icon_size;

			icon_size = _r_calc_clamp (_r_config_getinteger (L"IconSize", SHIL_SMALL), SHIL_LARGE, SHIL_LAST);

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
		}

		_r_menu_checkitem (hmenu, IDM_ICONSISHIDDEN, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsIconsHidden", FALSE));

		_r_menu_checkitem (hmenu, IDM_LOADONSTARTUP_CHK, 0, MF_BYCOMMAND, _r_autorun_isenabled ());
		_r_menu_checkitem (hmenu, IDM_STARTMINIMIZED_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsStartMinimized", FALSE));
		_r_menu_checkitem (hmenu, IDM_SKIPUACWARNING_CHK, 0, MF_BYCOMMAND, _r_skipuac_isenabled ());
		_r_menu_checkitem (hmenu, IDM_CHECKUPDATES_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"CheckUpdates", TRUE));

		_r_menu_checkitem (hmenu, IDM_RULE_BLOCKOUTBOUND, 0, MF_BYCOMMAND, _r_config_getboolean (L"BlockOutboundConnections", TRUE));
		_r_menu_checkitem (hmenu, IDM_RULE_BLOCKINBOUND, 0, MF_BYCOMMAND, _r_config_getboolean (L"BlockInboundConnections", TRUE));
		_r_menu_checkitem (hmenu, IDM_RULE_ALLOWLOOPBACK, 0, MF_BYCOMMAND, _r_config_getboolean (L"AllowLoopbackConnections", TRUE));
		_r_menu_checkitem (hmenu, IDM_RULE_ALLOW6TO4, 0, MF_BYCOMMAND, _r_config_getboolean (L"AllowIPv6", TRUE));

		_r_menu_checkitem (hmenu, IDM_USECERTIFICATES_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsCertificatesEnabled", TRUE));
		_r_menu_checkitem (hmenu, IDM_USENETWORKRESOLUTION_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsNetworkResolutionsEnabled", FALSE));
		_r_menu_checkitem (hmenu, IDM_USEREFRESHDEVICES_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"IsRefreshDevices", TRUE));

		_r_menu_checkitem (hmenu, IDM_BLOCKLIST_SPY_DISABLE, IDM_BLOCKLIST_SPY_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_SPY_DISABLE + _r_calc_clamp (_r_config_getinteger (L"BlocklistSpyState", 2), 0, 2));
		_r_menu_checkitem (hmenu, IDM_BLOCKLIST_UPDATE_DISABLE, IDM_BLOCKLIST_UPDATE_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_UPDATE_DISABLE + _r_calc_clamp (_r_config_getinteger (L"BlocklistUpdateState", 0), 0, 2));
		_r_menu_checkitem (hmenu, IDM_BLOCKLIST_EXTRA_DISABLE, IDM_BLOCKLIST_EXTRA_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_EXTRA_DISABLE + _r_calc_clamp (_r_config_getinteger (L"BlocklistExtraState", 0), 0, 2));
	}

	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLENOTIFICATIONS_CHK, NULL, 0, _r_config_getboolean (L"IsNotificationsEnabled", TRUE) ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED, I_IMAGENONE);
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLELOG_CHK, NULL, 0, _r_config_getboolean (L"IsLogEnabled", FALSE) ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED, I_IMAGENONE);
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLEUILOG_CHK, NULL, 0, _r_config_getboolean (L"IsLogUiEnabled", FALSE) ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED, I_IMAGENONE);
}

VOID _app_message_localize (_In_ HWND hwnd)
{
	HMENU hmenu;
	HMENU hsubmenu;
	LPCWSTR recommended_string;

	hmenu = GetMenu (hwnd);

	if (hmenu)
	{
		recommended_string = _r_locale_getstring (IDS_RECOMMENDED);

		_r_menu_setitemtext (hmenu, 0, TRUE, _r_locale_getstring (IDS_FILE));
		_r_menu_setitemtext (hmenu, 1, TRUE, _r_locale_getstring (IDS_EDIT));
		_r_menu_setitemtext (hmenu, 2, TRUE, _r_locale_getstring (IDS_VIEW));
		_r_menu_setitemtext (hmenu, 3, TRUE, _r_locale_getstring (IDS_SETTINGS));
		_r_menu_setitemtext (hmenu, 4, TRUE, _r_locale_getstring (IDS_TRAY_BLOCKLIST_RULES));
		_r_menu_setitemtext (hmenu, 5, TRUE, _r_locale_getstring (IDS_HELP));

		// file submenu
		_r_menu_setitemtextformat (hmenu, IDM_SETTINGS, FALSE, L"%s...\tF2", _r_locale_getstring (IDS_SETTINGS));
		_r_menu_setitemtextformat (hmenu, IDM_ADD_FILE, FALSE, L"%s...", _r_locale_getstring (IDS_ADD_FILE));
		_r_menu_setitemtextformat (hmenu, IDM_IMPORT, FALSE, L"%s...\tCtrl+O", _r_locale_getstring (IDS_IMPORT));
		_r_menu_setitemtextformat (hmenu, IDM_EXPORT, FALSE, L"%s...\tCtrl+S", _r_locale_getstring (IDS_EXPORT));
		_r_menu_setitemtextformat (hmenu, IDM_EXIT, FALSE, _r_locale_getstring (IDS_EXIT));

		// edit submenu
		_r_menu_setitemtextformat (hmenu, IDM_PURGE_UNUSED, FALSE, L"%s\tCtrl+Shift+X", _r_locale_getstring (IDS_PURGE_UNUSED));
		_r_menu_setitemtextformat (hmenu, IDM_PURGE_TIMERS, FALSE, L"%s\tCtrl+Shift+T", _r_locale_getstring (IDS_PURGE_TIMERS));
		_r_menu_setitemtextformat (hmenu, IDM_REFRESH, FALSE, L"%s\tF5", _r_locale_getstring (IDS_REFRESH));

		// view submenu
		_r_menu_setitemtext (hmenu, IDM_ALWAYSONTOP_CHK, FALSE, _r_locale_getstring (IDS_ALWAYSONTOP_CHK));
		_r_menu_setitemtext (hmenu, IDM_SHOWFILENAMESONLY_CHK, FALSE, _r_locale_getstring (IDS_SHOWFILENAMESONLY_CHK));
		_r_menu_setitemtext (hmenu, IDM_AUTOSIZECOLUMNS_CHK, FALSE, _r_locale_getstring (IDS_AUTOSIZECOLUMNS_CHK));

		_r_menu_setitemtext (hmenu, IDM_SIZE_SMALL, FALSE, _r_locale_getstring (IDS_ICONSSMALL));
		_r_menu_setitemtext (hmenu, IDM_SIZE_LARGE, FALSE, _r_locale_getstring (IDS_ICONSLARGE));
		_r_menu_setitemtext (hmenu, IDM_SIZE_EXTRALARGE, FALSE, _r_locale_getstring (IDS_ICONSEXTRALARGE));

		_r_menu_setitemtext (hmenu, IDM_VIEW_DETAILS, FALSE, _r_locale_getstring (IDS_VIEW_DETAILS));
		_r_menu_setitemtext (hmenu, IDM_VIEW_ICON, FALSE, _r_locale_getstring (IDS_VIEW_ICON));
		_r_menu_setitemtext (hmenu, IDM_VIEW_TILE, FALSE, _r_locale_getstring (IDS_VIEW_TILE));

		_r_menu_setitemtext (hmenu, IDM_ICONSISHIDDEN, FALSE, _r_locale_getstring (IDS_ICONSISHIDDEN));

		hsubmenu = GetSubMenu (hmenu, 2);

		if (hsubmenu)
		{
			_r_menu_setitemtext (hsubmenu, 4, TRUE, _r_locale_getstring (IDS_ICONS));

			_r_menu_setitemtextformat (hsubmenu, LANG_MENU, TRUE, L"%s (Language)", _r_locale_getstring (IDS_LANGUAGE));
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

		_r_menu_setitemtext (hmenu, IDM_USENETWORKRESOLUTION_CHK, FALSE, _r_locale_getstring (IDS_USENETWORKRESOLUTION_CHK));
		_r_menu_setitemtext (hmenu, IDM_USECERTIFICATES_CHK, FALSE, _r_locale_getstring (IDS_USECERTIFICATES_CHK));
		_r_menu_setitemtext (hmenu, IDM_USEREFRESHDEVICES_CHK, FALSE, _r_locale_getstring (IDS_USEREFRESHDEVICES_CHK));

		hsubmenu = GetSubMenu (hmenu, 3);

		if (hsubmenu)
		{
			_r_menu_setitemtext (hsubmenu, 5, TRUE, _r_locale_getstring (IDS_TRAY_RULES));
		}

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

	PR_STRING localized_string = NULL;

	// localize toolbar
	_app_setinterfacestate (hwnd);

	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_REFRESH, _r_locale_getstring (IDS_REFRESH), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, 0, I_IMAGENONE);
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_SETTINGS, _r_locale_getstring (IDS_SETTINGS), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, 0, I_IMAGENONE);

	_r_obj_movereference (&localized_string, _r_format_string (L"%s...", _r_locale_getstring (IDS_OPENRULESEDITOR)));
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_OPENRULESEDITOR, localized_string->buffer, BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLENOTIFICATIONS_CHK, _r_locale_getstring (IDS_ENABLENOTIFICATIONS_CHK), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLELOG_CHK, _r_locale_getstring (IDS_ENABLELOG_CHK), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_obj_movereference (&localized_string, _r_format_string (L"%s (session only)", _r_locale_getstring (IDS_ENABLEUILOG_CHK)));
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLEUILOG_CHK, localized_string->buffer, BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_obj_movereference (&localized_string, _r_format_string (L"%s (Ctrl+I)", _r_locale_getstring (IDS_LOGSHOW)));
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_LOGSHOW, localized_string->buffer, BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_obj_movereference (&localized_string, _r_format_string (L"%s (Ctrl+X)", _r_locale_getstring (IDS_LOGCLEAR)));
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_LOGCLEAR, localized_string->buffer, BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_DONATE, _r_locale_getstring (IDS_DONATE), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_obj_movereference (&localized_string, _r_format_string (L"%s... (Ctrl+F)", _r_locale_getstring (IDS_FIND)));
	_r_edit_setcuebanner (config.hrebar, IDC_SEARCH, localized_string->buffer);

	// set rebar size
	_app_toolbar_resize ();

	// localize tabs
	INT listview_id;
	UINT locale_id;

	for (INT i = 0; i < _r_tab_getitemcount (hwnd, IDC_TAB); i++)
	{
		listview_id = _app_getlistviewbytab_id (hwnd, i);

		if (listview_id == IDC_APPS_PROFILE)
		{
			locale_id = IDS_TAB_APPS;
		}
		else if (listview_id == IDC_APPS_SERVICE)
		{
			locale_id = IDS_TAB_SERVICES;
		}
		else if (listview_id == IDC_APPS_UWP)
		{
			locale_id = IDS_TAB_PACKAGES;
		}
		else if (listview_id == IDC_RULES_BLOCKLIST)
		{
			locale_id = IDS_TRAY_BLOCKLIST_RULES;
		}
		else if (listview_id == IDC_RULES_SYSTEM)
		{
			locale_id = IDS_TRAY_SYSTEM_RULES;
		}
		else if (listview_id == IDC_RULES_CUSTOM)
		{
			locale_id = IDS_TRAY_USER_RULES;
		}
		else if (listview_id == IDC_NETWORK)
		{
			locale_id = IDS_TAB_NETWORK;
		}
		else if (listview_id == IDC_LOG)
		{
			locale_id = IDS_TITLE_LOGGING;
		}
		else
		{
			continue;
		}

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

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_LOCAL L")", _r_locale_getstring (IDS_ADDRESS)));
			_r_listview_setcolumn (hwnd, listview_id, 1, localized_string->buffer, 0);

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_LOCAL L")", _r_locale_getstring (IDS_HOST)));
			_r_listview_setcolumn (hwnd, listview_id, 2, localized_string->buffer, 0);

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_LOCAL L")", _r_locale_getstring (IDS_PORT)));
			_r_listview_setcolumn (hwnd, listview_id, 3, localized_string->buffer, 0);

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_REMOTE L")", _r_locale_getstring (IDS_ADDRESS)));
			_r_listview_setcolumn (hwnd, listview_id, 4, localized_string->buffer, 0);

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_REMOTE L")", _r_locale_getstring (IDS_HOST)));
			_r_listview_setcolumn (hwnd, listview_id, 5, localized_string->buffer, 0);

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_REMOTE L")", _r_locale_getstring (IDS_PORT)));
			_r_listview_setcolumn (hwnd, listview_id, 6, localized_string->buffer, 0);

			_r_listview_setcolumn (hwnd, listview_id, 7, _r_locale_getstring (IDS_PROTOCOL), 0);
			_r_listview_setcolumn (hwnd, listview_id, 8, _r_locale_getstring (IDS_STATE), 0);
		}
		else if (listview_id == IDC_LOG)
		{
			_r_listview_setcolumn (hwnd, listview_id, 0, _r_locale_getstring (IDS_NAME), 0);

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_LOCAL L")", _r_locale_getstring (IDS_ADDRESS)));
			_r_listview_setcolumn (hwnd, listview_id, 1, localized_string->buffer, 0);

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_LOCAL L")", _r_locale_getstring (IDS_HOST)));
			_r_listview_setcolumn (hwnd, listview_id, 2, localized_string->buffer, 0);

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_LOCAL L")", _r_locale_getstring (IDS_PORT)));
			_r_listview_setcolumn (hwnd, listview_id, 3, localized_string->buffer, 0);

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_REMOTE L")", _r_locale_getstring (IDS_ADDRESS)));
			_r_listview_setcolumn (hwnd, listview_id, 4, localized_string->buffer, 0);

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_REMOTE L")", _r_locale_getstring (IDS_HOST)));
			_r_listview_setcolumn (hwnd, listview_id, 5, localized_string->buffer, 0);

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_REMOTE L")", _r_locale_getstring (IDS_PORT)));
			_r_listview_setcolumn (hwnd, listview_id, 6, localized_string->buffer, 0);

			_r_listview_setcolumn (hwnd, listview_id, 7, _r_locale_getstring (IDS_PROTOCOL), 0);
			_r_listview_setcolumn (hwnd, listview_id, 8, _r_locale_getstring (IDS_DIRECTION), 0);
			_r_listview_setcolumn (hwnd, listview_id, 9, _r_locale_getstring (IDS_FILTER), 0);
			_r_listview_setcolumn (hwnd, listview_id, 10, _r_locale_getstring (IDS_DATE), 0);
		}
	}

	_app_updatelistviewbylparam (hwnd, DATA_LISTVIEW_CURRENT, PR_UPDATE_TYPE | PR_UPDATE_FORCE);

	if (localized_string)
		_r_obj_dereference (localized_string);
}

VOID _app_command_idtorules (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	SIZE_T rule_idx;
	PITEM_RULE ptr_rule;
	PITEM_APP ptr_app;
	PR_LIST rules;
	ULONG_PTR app_hash;
	INT listview_id;
	INT item_id;
	BOOL is_remove;

	listview_id = _app_getcurrentlistview_id (hwnd);

	if (!_r_listview_getselectedcount (hwnd, listview_id))
		return;

	rule_idx = (SIZE_T)ctrl_id - IDX_RULES_SPECIAL;
	ptr_rule = _app_getrulebyid (rule_idx);

	if (!ptr_rule)
		return;

	item_id = -1;
	is_remove = -1;

	while ((item_id = _r_listview_getnextselected (hwnd, listview_id, item_id)) != -1)
	{
		app_hash = _app_getlistviewitemcontext (hwnd, listview_id, item_id);

		if (ptr_rule->is_forservices && (app_hash == config.ntoskrnl_hash || app_hash == config.svchost_hash))
			continue;

		ptr_app = _app_getappitem (app_hash);

		if (!ptr_app)
			continue;

		_app_notify_freeobject (ptr_app);

		if (is_remove == -1)
			is_remove = !!(ptr_rule->is_enabled && _r_obj_findhashtable (ptr_rule->apps, app_hash));

		_app_setruletoapp (hwnd, ptr_rule, item_id, ptr_app, !is_remove);

		_r_obj_dereference (ptr_app);
	}

	rules = _r_obj_createlist (NULL);

	if (_wfp_isfiltersinstalled ())
	{
		HANDLE hengine = _wfp_getenginehandle ();

		if (hengine)
		{
			_r_obj_addlistitem (rules, ptr_rule);

			_wfp_create4filters (hengine, rules, __LINE__, FALSE);
		}
	}

	_app_updatelistviewbylparam (hwnd, listview_id, 0);
	_app_updateitembylparam (hwnd, rule_idx, FALSE);

	_r_obj_dereference (rules);
	_r_obj_dereference (ptr_rule);

	_app_profile_save ();
}

VOID _app_command_idtotimers (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	SIZE_T timer_idx;
	LONG64 seconds;
	INT listview_id;
	INT item_id;

	listview_id = _app_getcurrentlistview_id (hwnd);

	if (!listview_id || !_r_listview_getselectedcount (hwnd, listview_id))
		return;

	timer_idx = (SIZE_T)ctrl_id - IDX_TIMER;
	seconds = timer_array[timer_idx];
	item_id = -1;

	if (_wfp_isfiltersinstalled ())
	{
		HANDLE hengine = _wfp_getenginehandle ();

		if (hengine)
		{
			PR_LIST rules;
			PITEM_APP ptr_app;
			ULONG_PTR app_hash;

			rules = _r_obj_createlist_ex (8, &_r_obj_dereference);

			while ((item_id = _r_listview_getnextselected (hwnd, listview_id, item_id)) != -1)
			{
				app_hash = _app_getlistviewitemcontext (hwnd, listview_id, item_id);
				ptr_app = _app_getappitem (app_hash);

				if (ptr_app)
				{
					_app_timer_set (hwnd, ptr_app, seconds);

					_r_obj_addlistitem (rules, ptr_app);
				}
			}

			_wfp_create3filters (hengine, rules, __LINE__, FALSE);

			_r_obj_dereference (rules);
		}
	}

	_app_updatelistviewbylparam (hwnd, listview_id, PR_UPDATE_FORCE);

	_app_profile_save ();
}

VOID _app_command_logshow (_In_ HWND hwnd)
{
	if (_r_config_getboolean (L"IsLogUiEnabled", FALSE))
	{
		INT item_count;

		item_count = _r_listview_getitemcount (hwnd, IDC_LOG);

		_r_wnd_toggle (hwnd, TRUE);

		_app_settab_id (hwnd, IDC_LOG);

		if (item_count)
		{
			_r_listview_ensurevisible (hwnd, IDC_LOG, item_count - 1);
		}
	}
	else
	{
		PR_STRING log_path;
		PR_STRING viewer_path;
		PR_STRING process_path;
		HANDLE current_handle;

		log_path = _r_config_getstringexpand (L"LogPath", LOG_PATH_DEFAULT);

		if (!log_path)
			return;

		if (!_r_fs_exists (log_path->buffer))
		{
			_r_obj_dereference (log_path);
			return;
		}

		current_handle = InterlockedCompareExchangePointer (&config.hlogfile, NULL, NULL);

		if (_r_fs_isvalidhandle (current_handle))
			FlushFileBuffers (current_handle);

		viewer_path = _app_getlogviewer ();

		if (viewer_path)
		{
			process_path = _r_obj_concatstrings (5, L"\"", viewer_path->buffer, L"\" \"", log_path->buffer, L"\"");

			if (!_r_sys_createprocess (viewer_path->buffer, process_path->buffer, NULL))
			{
				R_ERROR_INFO error_info = {0};
				error_info.description = viewer_path->buffer;

				_r_show_errormessage (hwnd, NULL, GetLastError (), &error_info);
			}

			_r_obj_dereference (process_path);
			_r_obj_dereference (viewer_path);
		}

		_r_obj_dereference (log_path);
	}
}

VOID _app_command_logclear (_In_ HWND hwnd)
{
	PR_STRING log_path;
	HANDLE current_handle;
	BOOLEAN is_valid;

	log_path = _r_config_getstringexpand (L"LogPath", LOG_PATH_DEFAULT);

	current_handle = InterlockedCompareExchangePointer (&config.hlogfile, NULL, NULL);

	_r_queuedlock_acquireshared (&lock_loglist);

	is_valid = _r_fs_isvalidhandle (current_handle) || (log_path && _r_fs_exists (log_path->buffer)) || !_r_obj_ishashtableempty (log_table);

	_r_queuedlock_releaseshared (&lock_loglist);

	if (is_valid)
	{
		if (_r_show_confirmmessage (hwnd, NULL, _r_locale_getstring (IDS_QUESTION), L"ConfirmLogClear"))
		{
			_app_logclear (current_handle);
			_app_logclear_ui (hwnd);
		}
	}

	if (log_path)
		_r_obj_dereference (log_path);
}

VOID _app_command_logerrshow (_In_opt_ HWND hwnd)
{
	PR_STRING viewer_path;
	PR_STRING process_path;
	LPCWSTR log_path;

	log_path = _r_app_getlogpath ();

	if (_r_fs_exists (log_path))
	{
		viewer_path = _app_getlogviewer ();

		if (viewer_path)
		{
			process_path = _r_format_string (L"\"%s\" \"%s\"", viewer_path->buffer, log_path);

			if (!_r_sys_createprocess (viewer_path->buffer, process_path->buffer, NULL))
			{
				R_ERROR_INFO error_info = {0};
				error_info.description = viewer_path->buffer;

				_r_show_errormessage (hwnd, NULL, GetLastError (), &error_info);
			}

			_r_obj_dereference (process_path);
			_r_obj_dereference (viewer_path);
		}
	}
}

VOID _app_command_logerrclear (_In_opt_ HWND hwnd)
{
	LPCWSTR path;

	if (!_r_show_confirmmessage (hwnd, NULL, _r_locale_getstring (IDS_QUESTION), L"ConfirmLogClear"))
		return;

	path = _r_app_getlogpath ();

	if (!_r_fs_exists (path))
		return;

	_r_fs_deletefile (path, TRUE);
}

VOID _app_command_copy (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT column_id)
{
	static R_STRINGREF divider_sr = PR_STRINGREF_INIT (DIVIDER_COPY);
	R_STRINGBUILDER buffer;
	PR_STRING string;
	INT listview_id;
	INT column_count;
	INT item_id;

	listview_id = _app_getcurrentlistview_id (hwnd);
	item_id = -1;

	column_count = _r_listview_getcolumncount (hwnd, listview_id);

	_r_obj_initializestringbuilder (&buffer);

	while ((item_id = _r_listview_getnextselected (hwnd, listview_id, item_id)) != -1)
	{
		if (ctrl_id == IDM_COPY)
		{
			for (INT i = 0; i < column_count; i++)
			{
				string = _r_listview_getitemtext (hwnd, listview_id, item_id, i);

				if (string)
				{
					_r_obj_appendstringbuilder2 (&buffer, string);
					_r_obj_appendstringbuilder3 (&buffer, &divider_sr);

					_r_obj_dereference (string);
				}
			}

			string = _r_obj_finalstringbuilder (&buffer);

			_r_str_trimstring (string, &divider_sr, 0);
		}
		else
		{
			string = _r_listview_getitemtext (hwnd, listview_id, item_id, column_id);

			if (string)
			{
				_r_obj_appendstringbuilder2 (&buffer, string);

				_r_obj_dereference (string);
			}
		}

		_r_obj_appendstringbuilder (&buffer, L"\r\n");
	}

	string = _r_obj_finalstringbuilder (&buffer);

	_r_str_trimstring2 (string, DIVIDER_TRIM, 0);

	_r_clipboard_set (hwnd, &string->sr);

	_r_obj_deletestringbuilder (&buffer);
}

VOID _app_command_checkbox (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	PR_LIST rules;
	INT listview_id;
	INT item_id;
	BOOLEAN new_val;
	BOOLEAN is_changed;

	rules = _r_obj_createlist_ex (8, &_r_obj_dereference);

	listview_id = _app_getcurrentlistview_id (hwnd);
	item_id = -1;

	new_val = (ctrl_id == IDM_CHECK);
	is_changed = FALSE;

	if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
	{
		ULONG_PTR app_hash;
		PITEM_APP ptr_app;

		while ((item_id = _r_listview_getnextselected (hwnd, listview_id, item_id)) != -1)
		{
			app_hash = _app_getlistviewitemcontext (hwnd, listview_id, item_id);
			ptr_app = _app_getappitem (app_hash);

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
					_app_notify_freeobject (ptr_app);
				}

				ptr_app->is_enabled = new_val;

				_app_setcheckboxlock (hwnd, listview_id, TRUE);
				_app_setappiteminfo (hwnd, listview_id, item_id, ptr_app);
				_app_setcheckboxlock (hwnd, listview_id, FALSE);

				_r_obj_addlistitem (rules, ptr_app);

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
					_wfp_create3filters (hengine, rules, __LINE__, FALSE);
			}
		}
	}
	else if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM)
	{
		PITEM_RULE ptr_rule;
		SIZE_T rule_idx;

		while ((item_id = _r_listview_getnextselected (hwnd, listview_id, item_id)) != -1)
		{
			rule_idx = _app_getlistviewitemcontext (hwnd, listview_id, item_id);
			ptr_rule = _app_getrulebyid (rule_idx);

			if (!ptr_rule)
				continue;

			if (ptr_rule->is_enabled != new_val)
			{
				_app_ruleenable (ptr_rule, new_val, TRUE);

				_app_setcheckboxlock (hwnd, listview_id, TRUE);
				_app_setruleiteminfo (hwnd, listview_id, item_id, ptr_rule, TRUE);
				_app_setcheckboxlock (hwnd, listview_id, FALSE);

				_r_obj_addlistitem (rules, ptr_rule);

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
					_wfp_create4filters (hengine, rules, __LINE__, FALSE);
			}
		}
	}

	_r_obj_dereference (rules);

	if (is_changed)
	{
		_app_updatelistviewbylparam (hwnd, listview_id, PR_UPDATE_FORCE);

		_app_profile_save ();
	}
}

VOID _app_command_delete (_In_ HWND hwnd)
{
	MIB_TCPROW tcprow;
	PR_ARRAY guids = NULL;
	INT listview_id;
	INT selected_count;
	INT count;

	listview_id = _app_getcurrentlistview_id (hwnd);

	if (listview_id != IDC_APPS_PROFILE && listview_id != IDC_RULES_CUSTOM && listview_id != IDC_NETWORK)
		return;

	selected_count = _r_listview_getselectedcount (hwnd, listview_id);

	if (!selected_count)
		return;

	if (listview_id != IDC_NETWORK)
	{
		WCHAR message_text[512];
		_r_str_printf (message_text, RTL_NUMBER_OF (message_text), _r_locale_getstring (IDS_QUESTION_DELETE), selected_count);

		if (_r_show_message (hwnd, MB_YESNO | MB_ICONEXCLAMATION, NULL, NULL, message_text) != IDYES)
			return;

		guids = _r_obj_createarray (sizeof (GUID), NULL);
	}

	count = _r_listview_getitemcount (hwnd, listview_id) - 1;

	for (INT i = count; i != -1; i--)
	{
		if (!_r_listview_isitemselected (hwnd, listview_id, i))
			continue;

		if (listview_id == IDC_APPS_PROFILE)
		{
			ULONG_PTR app_hash;
			PITEM_APP ptr_app;

			app_hash = _app_getlistviewitemcontext (hwnd, listview_id, i);
			ptr_app = _app_getappitem (app_hash);

			if (!ptr_app)
				continue;

			if (!ptr_app->is_undeletable) // skip "undeletable" apps
			{
				if (!_r_obj_isarrayempty (ptr_app->guids))
				{
					_r_obj_addarrayitems (guids, ptr_app->guids->items, ptr_app->guids->count);
				}

				_r_listview_deleteitem (hwnd, listview_id, i);

				_app_timer_reset (hwnd, ptr_app);
				_app_notify_freeobject (ptr_app);

				_r_queuedlock_acquireexclusive (&lock_apps);

				_app_freeapplication (app_hash);

				_r_queuedlock_releaseexclusive (&lock_apps);
			}

			_r_obj_dereference (ptr_app);
		}
		else if (listview_id == IDC_RULES_CUSTOM)
		{
			SIZE_T rule_idx;
			PITEM_RULE ptr_rule;
			PR_HASHTABLE apps_checker;
			ULONG_PTR hash_code;
			SIZE_T enum_key;

			rule_idx = _app_getlistviewitemcontext (hwnd, listview_id, i);
			ptr_rule = _app_getrulebyid (rule_idx);

			if (!ptr_rule)
				continue;

			if (!ptr_rule->is_readonly) // skip "read-only" rules
			{
				apps_checker = _r_obj_createhashtable (sizeof (SHORT), NULL);

				if (!_r_obj_isarrayempty (ptr_rule->guids))
				{
					_r_obj_addarrayitems (guids, ptr_rule->guids->items, ptr_rule->guids->count);
				}

				enum_key = 0;

				while (_r_obj_enumhashtable (ptr_rule->apps, NULL, &hash_code, &enum_key))
				{
					_r_obj_addhashtableitem (apps_checker, hash_code, NULL);
				}

				_r_listview_deleteitem (hwnd, listview_id, i);

				_r_obj_reference (ptr_rule); // required to dereference later

				_r_queuedlock_acquireexclusive (&lock_rules);

				_r_obj_setlistitem (rules_list, rule_idx, NULL);

				_r_queuedlock_releaseexclusive (&lock_rules);

				enum_key = 0;

				while (_r_obj_enumhashtable (apps_checker, NULL, &hash_code, &enum_key))
				{
					_app_updateitembylparam (hwnd, hash_code, TRUE);
				}
			}

			_r_obj_dereference (ptr_rule);
		}
		else if (listview_id == IDC_NETWORK)
		{
			ULONG_PTR network_hash;
			PITEM_NETWORK ptr_network;

			network_hash = _app_getlistviewitemcontext (hwnd, listview_id, i);
			ptr_network = _app_getnetworkitem (network_hash);

			if (!ptr_network)
				continue;

			if (ptr_network->af == AF_INET && ptr_network->state == MIB_TCP_STATE_ESTAB)
			{
				RtlZeroMemory (&tcprow, sizeof (tcprow));

				tcprow.dwState = MIB_TCP_STATE_DELETE_TCB;
				tcprow.dwLocalAddr = ptr_network->local_addr.S_un.S_addr;
				tcprow.dwLocalPort = _r_byteswap_ushort ((USHORT)ptr_network->local_port);
				tcprow.dwRemoteAddr = ptr_network->remote_addr.S_un.S_addr;
				tcprow.dwRemotePort = _r_byteswap_ushort ((USHORT)ptr_network->remote_port);

				if (SetTcpEntry (&tcprow) == NO_ERROR)
				{
					_r_listview_deleteitem (hwnd, listview_id, i);

					_r_queuedlock_acquireexclusive (&lock_network);

					_r_obj_removehashtablepointer (network_table, network_hash);

					_r_queuedlock_releaseexclusive (&lock_network);
				}
			}

			_r_obj_dereference (ptr_network);
		}
	}

	if (guids)
	{
		if (!_r_obj_isarrayempty (guids))
		{
			if (_wfp_isfiltersinstalled ())
			{
				HANDLE hengine = _wfp_getenginehandle ();

				if (hengine)
					_wfp_destroyfilters_array (hengine, guids, __LINE__);
			}
		}

		_r_obj_dereference (guids);
	}

	_app_updatelistviewbylparam (hwnd, listview_id, 0);

	_app_profile_save ();
}

VOID _app_command_disable (_In_ HWND hwnd, _In_ INT ctrl_id)
{
	PITEM_APP ptr_app;
	ULONG_PTR app_hash;
	INT listview_id;
	INT item_id;
	BOOL new_val;

	listview_id = _app_getcurrentlistview_id (hwnd);

	// note: these commands only for apps...
	if (!(listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP))
		return;

	item_id = -1;
	new_val = -1;

	while ((item_id = _r_listview_getnextselected (hwnd, listview_id, item_id)) != -1)
	{
		app_hash = _app_getlistviewitemcontext (hwnd, listview_id, item_id);
		ptr_app = _app_getappitem (app_hash);

		if (!ptr_app)
			continue;

		if (ctrl_id == IDM_DISABLENOTIFICATIONS)
		{
			if (new_val == -1)
				new_val = !ptr_app->is_silent;

			_app_setappinfo (ptr_app, INFO_IS_SILENT, IntToPtr (new_val));
		}
		else if (ctrl_id == IDM_DISABLETIMER)
		{
			_app_timer_reset (hwnd, ptr_app);
		}

		_app_updateitembyidx (hwnd, listview_id, item_id);

		_r_obj_dereference (ptr_app);
	}

	_app_updatelistviewbylparam (hwnd, listview_id, 0);

	_app_profile_save ();
}

VOID _app_command_openeditor (_In_ HWND hwnd)
{
	PITEM_RULE ptr_rule;
	ULONG_PTR app_hash;
	INT listview_id;
	INT item_id;

	ptr_rule = _app_addrule (NULL, NULL, NULL, FWP_DIRECTION_OUTBOUND, 0, 0);

	_app_ruleenable (ptr_rule, TRUE, FALSE);

	listview_id = _app_getcurrentlistview_id (hwnd);

	if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
	{
		item_id = -1;

		while ((item_id = _r_listview_getnextselected (hwnd, listview_id, item_id)) != -1)
		{
			app_hash = _app_getlistviewitemcontext (hwnd, listview_id, item_id);

			if (_app_isappfound (app_hash))
			{
				_r_obj_addhashtableitem (ptr_rule->apps, app_hash, NULL);
			}
		}
	}
	else if (listview_id == IDC_NETWORK)
	{
		ptr_rule->action = FWP_ACTION_BLOCK;

		item_id = _r_listview_getnextselected (hwnd, listview_id, -1);

		if (item_id != -1)
		{
			ULONG_PTR network_hash;
			PITEM_NETWORK ptr_network;

			network_hash = _app_getlistviewitemcontext (hwnd, listview_id, item_id);
			ptr_network = _app_getnetworkitem (network_hash);

			if (ptr_network)
			{
				if (!ptr_rule->name)
				{
					ptr_rule->name = _r_listview_getitemtext (hwnd, listview_id, item_id, 0);
				}

				if (ptr_network->app_hash && ptr_network->path)
				{
					if (!_app_isappfound (ptr_network->app_hash))
					{
						ptr_network->app_hash = _app_addapplication (hwnd, DATA_UNKNOWN, &ptr_network->path->sr, NULL, NULL);

						if (ptr_network->app_hash)
						{
							_app_setlistviewbylparam (hwnd, ptr_network->app_hash, PR_SETITEM_UPDATE, TRUE);

							_app_profile_save ();
						}
					}

					_r_obj_addhashtableitem (ptr_rule->apps, ptr_network->app_hash, NULL);
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
		item_id = _r_listview_getnextselected (hwnd, listview_id, -1);

		if (item_id != -1)
		{
			SIZE_T log_idx;
			PITEM_LOG ptr_log;

			log_idx = _app_getlistviewitemcontext (hwnd, listview_id, item_id);
			ptr_log = _app_getlogitem (log_idx);

			if (ptr_log)
			{
				if (!ptr_rule->name)
				{
					ptr_rule->name = _r_listview_getitemtext (hwnd, listview_id, item_id, 0);
				}

				if (ptr_log->app_hash && ptr_log->path)
				{
					if (!_app_isappfound (ptr_log->app_hash))
					{
						ptr_log->app_hash = _app_addapplication (hwnd, DATA_UNKNOWN, &ptr_log->path->sr, NULL, NULL);

						if (ptr_log->app_hash)
						{
							_app_setlistviewbylparam (hwnd, ptr_log->app_hash, PR_SETITEM_UPDATE, TRUE);

							_app_profile_save ();
						}
					}

					_r_obj_addhashtableitem (ptr_rule->apps, ptr_log->app_hash, NULL);
				}

				ptr_rule->protocol = ptr_log->protocol;
				ptr_rule->direction = ptr_log->direction;

				_r_obj_movereference (&ptr_rule->rule_remote, _app_formataddress (ptr_log->af, 0, &ptr_log->remote_addr, ptr_log->remote_port, FMTADDR_AS_RULE));
				_r_obj_movereference (&ptr_rule->rule_local, _app_formataddress (ptr_log->af, 0, &ptr_log->local_addr, ptr_log->local_port, FMTADDR_AS_RULE));

				_r_obj_dereference (ptr_log);
			}
		}
	}

	ITEM_CONTEXT context = {0};

	context.is_settorules = TRUE;
	context.ptr_rule = ptr_rule;

	if (DialogBoxParam (NULL, MAKEINTRESOURCE (IDD_EDITOR), hwnd, &PropertiesProc, (LPARAM)&context))
	{
		SIZE_T rule_idx;

		_r_queuedlock_acquireexclusive (&lock_rules);

		_r_obj_addlistitem_ex (rules_list, _r_obj_reference (ptr_rule), &rule_idx);

		_r_queuedlock_releaseexclusive (&lock_rules);

		if (rule_idx != SIZE_MAX)
		{
			_app_addlistviewrule (hwnd, ptr_rule, rule_idx, TRUE);
			_app_updatelistviewbylparam (hwnd, DATA_LISTVIEW_CURRENT, PR_UPDATE_TYPE);

			_app_profile_save ();
		}
	}

	_r_obj_dereference (ptr_rule);
}

VOID _app_command_properties (_In_ HWND hwnd)
{
	INT listview_id;
	INT item_id;

	listview_id = _app_getcurrentlistview_id (hwnd);
	item_id = _r_listview_getnextselected (hwnd, listview_id, -1);

	if (item_id == -1)
		return;

	if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
	{
		ITEM_CONTEXT context = {0};
		ULONG_PTR app_hash;
		PITEM_APP ptr_app;

		app_hash = _app_getlistviewitemcontext (hwnd, listview_id, item_id);
		ptr_app = _app_getappitem (app_hash);

		if (!ptr_app)
			return;

		context.is_settorules = FALSE;
		context.ptr_app = ptr_app;

		if (DialogBoxParam (NULL, MAKEINTRESOURCE (IDD_EDITOR), hwnd, &PropertiesProc, (LPARAM)&context))
		{
			_app_setcheckboxlock (hwnd, listview_id, TRUE);
			_app_setappiteminfo (hwnd, listview_id, item_id, ptr_app);
			_app_setcheckboxlock (hwnd, listview_id, FALSE);

			_app_updatelistviewbylparam (hwnd, listview_id, 0);

			_app_profile_save ();
		}

		_r_obj_dereference (ptr_app);
	}
	else if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM)
	{
		ITEM_CONTEXT context = {0};
		SIZE_T rule_idx;
		PITEM_RULE ptr_rule;

		rule_idx = _app_getlistviewitemcontext (hwnd, listview_id, item_id);
		ptr_rule = _app_getrulebyid (rule_idx);

		if (!ptr_rule)
			return;

		context.is_settorules = TRUE;
		context.ptr_rule = ptr_rule;

		if (DialogBoxParam (NULL, MAKEINTRESOURCE (IDD_EDITOR), hwnd, &PropertiesProc, (LPARAM)&context))
		{
			_app_setcheckboxlock (hwnd, listview_id, TRUE);
			_app_setruleiteminfo (hwnd, listview_id, item_id, ptr_rule, TRUE);
			_app_setcheckboxlock (hwnd, listview_id, FALSE);

			_app_updatelistviewbylparam (hwnd, listview_id, 0);

			_app_profile_save ();
		}

		_r_obj_dereference (ptr_rule);
	}
	else if (listview_id == IDC_NETWORK)
	{
		PITEM_NETWORK ptr_network;
		ULONG_PTR network_hash;

		network_hash = _app_getlistviewitemcontext (hwnd, listview_id, item_id);
		ptr_network = _app_getnetworkitem (network_hash);

		if (!ptr_network)
			return;

		if (ptr_network->app_hash && ptr_network->path)
		{
			if (!_app_isappfound (ptr_network->app_hash))
			{
				ptr_network->app_hash = _app_addapplication (hwnd, DATA_UNKNOWN, &ptr_network->path->sr, NULL, NULL);

				if (ptr_network->app_hash)
				{
					_app_setlistviewbylparam (hwnd, ptr_network->app_hash, PR_SETITEM_UPDATE, TRUE);

					_app_profile_save ();
				}
			}

			if (ptr_network->app_hash)
			{
				_app_showitembylparam (hwnd, ptr_network->app_hash, TRUE);
			}
		}

		_r_obj_dereference (ptr_network);
	}
	else if (listview_id == IDC_LOG)
	{
		SIZE_T log_idx;
		PITEM_LOG ptr_log;

		log_idx = _app_getlistviewitemcontext (hwnd, listview_id, item_id);
		ptr_log = _app_getlogitem (log_idx);

		if (!ptr_log)
			return;

		if (ptr_log->app_hash && ptr_log->path)
		{
			if (!_app_isappfound (ptr_log->app_hash))
			{
				ptr_log->app_hash = _app_addapplication (hwnd, DATA_UNKNOWN, &ptr_log->path->sr, NULL, NULL);

				if (ptr_log->app_hash)
				{
					_app_setlistviewbylparam (hwnd, ptr_log->app_hash, PR_SETITEM_UPDATE, TRUE);

					_app_profile_save ();
				}
			}

			if (ptr_log->app_hash)
			{
				_app_showitembylparam (hwnd, ptr_log->app_hash, TRUE);
			}
		}

		_r_obj_dereference (ptr_log);
	}
}

VOID _app_command_purgeunused (_In_ HWND hwnd)
{
	PITEM_APP ptr_app;
	ULONG_PTR hash_code;
	SIZE_T enum_key;
	PR_HASHTABLE apps_list;
	PR_ARRAY guids;
	INT listview_id;
	INT item_id;
	BOOLEAN is_deleted;

	apps_list = _r_obj_createhashtable (sizeof (SHORT), NULL);
	guids = _r_obj_createarray (sizeof (GUID), NULL);

	_r_queuedlock_acquireshared (&lock_apps);

	is_deleted = FALSE;

	enum_key = 0;

	while (_r_obj_enumhashtablepointer (apps_table, &ptr_app, &hash_code, &enum_key))
	{
		if (!_app_isappunused (ptr_app))
			continue;

		listview_id = _app_getlistviewbytype_id (ptr_app->type);

		if (listview_id)
		{
			item_id = _app_getposition (hwnd, listview_id, hash_code);

			if (item_id != -1)
			{
				_r_listview_deleteitem (hwnd, listview_id, item_id);
			}
		}

		_app_timer_reset (NULL, ptr_app);
		_app_notify_freeobject (ptr_app);

		if (!_r_obj_isarrayempty (ptr_app->guids))
			_r_obj_addarrayitems (guids, ptr_app->guids->items, ptr_app->guids->count);

		_r_obj_addhashtableitem (apps_list, hash_code, NULL);

		is_deleted = TRUE;
	}

	_r_queuedlock_releaseshared (&lock_apps);

	if (is_deleted)
	{
		enum_key = 0;

		while (_r_obj_enumhashtable (apps_list, NULL, &hash_code, &enum_key))
		{
			_r_queuedlock_acquireexclusive (&lock_apps);

			_app_freeapplication (hash_code);

			_r_queuedlock_releaseexclusive (&lock_apps);
		}

		if (!_r_obj_isarrayempty (guids) && _wfp_isfiltersinstalled ())
		{
			HANDLE hengine = _wfp_getenginehandle ();

			if (hengine)
				_wfp_destroyfilters_array (hengine, guids, __LINE__);
		}

		_app_updatelistviewbylparam (hwnd, DATA_LISTVIEW_CURRENT, PR_UPDATE_TYPE | PR_UPDATE_FORCE);
		_app_profile_save ();
	}

	_r_obj_dereference (guids);
	_r_obj_dereference (apps_list);
}

VOID _app_command_purgetimers (_In_ HWND hwnd)
{
	PR_LIST rules;
	PITEM_APP ptr_app;
	SIZE_T enum_key;

	if (!_app_istimersactive () || _r_show_message (hwnd, MB_YESNO | MB_ICONEXCLAMATION, NULL, NULL, _r_locale_getstring (IDS_QUESTION_TIMERS)) != IDYES)
		return;

	rules = _r_obj_createlist (NULL);

	_r_queuedlock_acquireshared (&lock_apps);

	enum_key = 0;

	while (_r_obj_enumhashtablepointer (apps_table, &ptr_app, NULL, &enum_key))
	{
		if (_app_istimerset (ptr_app->htimer))
		{
			_app_timer_reset (hwnd, ptr_app);

			_r_obj_addlistitem (rules, ptr_app);
		}
	}

	_r_queuedlock_releaseshared (&lock_apps);

	if (rules)
	{
		if (!_r_obj_islistempty (rules))
		{
			if (_wfp_isfiltersinstalled ())
			{
				HANDLE hengine = _wfp_getenginehandle ();

				if (hengine)
					_wfp_create3filters (hengine, rules, __LINE__, FALSE);
			}
		}

		_r_obj_dereference (rules);
	}

	_app_updatelistviewbylparam (hwnd, DATA_LISTVIEW_CURRENT, PR_UPDATE_TYPE | PR_UPDATE_FORCE);

	_app_profile_save ();
}

VOID _app_command_selectfont (_In_ HWND hwnd)
{
	CHOOSEFONT cf = {0};
	LOGFONT lf = {0};
	LONG dpi_value;

	cf.lStructSize = sizeof (cf);
	cf.hwndOwner = hwnd;
	cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_NOSCRIPTSEL | CF_LIMITSIZE | CF_NOVERTFONTS;
	cf.nSizeMax = 16;
	cf.nSizeMin = 8;
	cf.lpLogFont = &lf;

	dpi_value = _r_dc_getwindowdpi (hwnd);

	_r_config_getfont (L"Font", &lf, dpi_value);

	if (ChooseFont (&cf))
	{
		_r_config_setfont (L"Font", &lf, dpi_value);

		_app_listviewloadfont (hwnd, TRUE);
		_app_updatelistviewbylparam (hwnd, DATA_LISTVIEW_CURRENT, PR_UPDATE_TYPE | PR_UPDATE_FORCE);

		RedrawWindow (hwnd, NULL, NULL, RDW_NOFRAME | RDW_NOINTERNALPAINT | RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
	}
}
