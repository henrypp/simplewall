// simplewall
// Copyright (c) 2020 Henry++

#include "global.h"

VOID _app_message_contextmenu (HWND hwnd, LPNMITEMACTIVATE lpnmlv)
{
	if (lpnmlv->iItem == -1)
		return;

	INT listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

	if (!(listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_LOG))
		return;

	HMENU hmenu = CreatePopupMenu ();

	if (!hmenu)
		return;

	PR_STRING localized_string = NULL;
	PR_STRING column_text = NULL;

	SIZE_T hash_item = _r_listview_getitemlparam (hwnd, listview_id, lpnmlv->iItem);
	INT lv_column_current = lpnmlv->iSubItem;

	if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
	{
		_r_obj_movereference (&localized_string, _r_format_string (L"%s...\tEnter", _r_locale_getstring (IDS_EDIT2)));
		AppendMenu (hmenu, MF_STRING, IDM_PROPERTIES, _r_obj_getstringorempty (localized_string));

		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

		_r_obj_movereference (&localized_string, _r_format_string (L"%s\tCtrl+E", _r_locale_getstring (IDS_EXPLORE)));
		AppendMenu (hmenu, MF_STRING, IDM_EXPLORE, _r_obj_getstringorempty (localized_string));

		_r_obj_movereference (&localized_string, _r_format_string (L"%s\tDel", _r_locale_getstring (IDS_DELETE)));
		AppendMenu (hmenu, MF_STRING, IDM_DELETE, _r_obj_getstringorempty (localized_string));

		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

		AppendMenu (hmenu, MF_STRING, IDM_DISABLENOTIFICATIONS, _r_locale_getstring (IDS_DISABLENOTIFICATIONS));
		AppendMenu (hmenu, MF_STRING, IDM_DISABLETIMER, _r_locale_getstring (IDS_DISABLETIMER));

		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);
		AppendMenu (hmenu, MF_STRING, IDM_CHECK, _r_locale_getstring (IDS_CHECK));
		AppendMenu (hmenu, MF_STRING, IDM_UNCHECK, _r_locale_getstring (IDS_UNCHECK));
		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

		_r_obj_movereference (&localized_string, _r_format_string (L"%s\tCtrl+A", _r_locale_getstring (IDS_SELECT_ALL)));
		AppendMenu (hmenu, MF_STRING, IDM_SELECT_ALL, _r_obj_getstringorempty (localized_string));

		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

		_r_obj_movereference (&localized_string, _r_format_string (L"%s\tCtrl+C", _r_locale_getstring (IDS_COPY)));
		AppendMenu (hmenu, MF_STRING, IDM_COPY, _r_obj_getstringorempty (localized_string));

		column_text = _r_listview_getcolumntext (hwnd, listview_id, lv_column_current);

		if (column_text)
		{
			_r_obj_movereference (&localized_string, _r_format_string (L"%s \"%s\"", _r_locale_getstring (IDS_COPY), _r_obj_getstringorempty (column_text)));
			AppendMenu (hmenu, MF_STRING, IDM_COPY2, _r_obj_getstringorempty (localized_string));

			_r_obj_dereference (column_text);
		}

		SetMenuDefaultItem (hmenu, IDM_PROPERTIES, MF_BYCOMMAND);

		PITEM_APP ptr_app = _r_obj_findhashtable (apps, hash_item);

		if (ptr_app)
		{
			_r_menu_checkitem (hmenu, IDM_DISABLENOTIFICATIONS, 0, MF_BYCOMMAND, PtrToInt (_app_getappinfo (ptr_app, InfoIsSilent)) != FALSE);

			if (PtrToInt (_app_getappinfo (ptr_app, InfoIsTimerSet)) != TRUE)
				_r_menu_enableitem (hmenu, IDM_DISABLETIMER, MF_BYCOMMAND, FALSE);
		}

		if (listview_id != IDC_APPS_PROFILE)
			_r_menu_enableitem (hmenu, IDM_DELETE, MF_BYCOMMAND, FALSE);
	}
	else if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM)
	{
		if (listview_id == IDC_RULES_CUSTOM)
		{
			_r_obj_movereference (&localized_string, _r_format_string (L"%s...", _r_locale_getstring (IDS_ADD)));
			AppendMenu (hmenu, MF_STRING, IDM_OPENRULESEDITOR, _r_obj_getstringorempty (localized_string));
		}

		_r_obj_movereference (&localized_string, _r_format_string (L"%s...\tEnter", _r_locale_getstring (IDS_EDIT2)));
		AppendMenu (hmenu, MF_STRING, IDM_PROPERTIES, _r_obj_getstringorempty (localized_string));

		if (listview_id == IDC_RULES_CUSTOM)
		{
			_r_obj_movereference (&localized_string, _r_format_string (L"%s\tDel", _r_locale_getstring (IDS_DELETE)));
			AppendMenu (hmenu, MF_STRING, IDM_DELETE, _r_obj_getstringorempty (localized_string));

			PITEM_RULE ptr_rule = _app_getrulebyid (hash_item);

			if (ptr_rule)
			{
				if (ptr_rule->is_readonly)
					_r_menu_enableitem (hmenu, IDM_DELETE, MF_BYCOMMAND, FALSE);
			}
		}

		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);
		AppendMenu (hmenu, MF_STRING, IDM_CHECK, _r_locale_getstring (IDS_CHECK));
		AppendMenu (hmenu, MF_STRING, IDM_UNCHECK, _r_locale_getstring (IDS_UNCHECK));
		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

		_r_obj_movereference (&localized_string, _r_format_string (L"%s\tCtrl+A", _r_locale_getstring (IDS_SELECT_ALL)));
		AppendMenu (hmenu, MF_STRING, IDM_SELECT_ALL, _r_obj_getstringorempty (localized_string));

		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

		_r_obj_movereference (&localized_string, _r_format_string (L"%s\tCtrl+C", _r_locale_getstring (IDS_COPY)));
		AppendMenu (hmenu, MF_STRING, IDM_COPY, _r_obj_getstringorempty (localized_string));

		column_text = _r_listview_getcolumntext (hwnd, listview_id, lv_column_current);

		if (column_text)
		{
			_r_obj_movereference (&localized_string, _r_format_string (L"%s \"%s\"", _r_locale_getstring (IDS_COPY), _r_obj_getstringorempty (column_text)));
			AppendMenu (hmenu, MF_STRING, IDM_COPY2, _r_obj_getstringorempty (localized_string));

			_r_obj_dereference (column_text);
		}

		SetMenuDefaultItem (hmenu, IDM_PROPERTIES, MF_BYCOMMAND);
	}
	else if (listview_id == IDC_NETWORK)
	{
		_r_obj_movereference (&localized_string, _r_format_string (L"%s\tEnter", _r_locale_getstring (IDS_SHOWINLIST)));
		AppendMenu (hmenu, MF_STRING, IDM_PROPERTIES, _r_obj_getstringorempty (localized_string));

		_r_obj_movereference (&localized_string, _r_format_string (L"%s...", _r_locale_getstring (IDS_OPENRULESEDITOR)));
		AppendMenu (hmenu, MF_STRING, IDM_OPENRULESEDITOR, _r_obj_getstringorempty (localized_string));

		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);
		AppendMenu (hmenu, MF_STRING, IDM_DELETE, _r_locale_getstring (IDS_NETWORK_CLOSE));
		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

		_r_obj_movereference (&localized_string, _r_format_string (L"%s\tCtrl+A", _r_locale_getstring (IDS_SELECT_ALL)));
		AppendMenu (hmenu, MF_STRING, IDM_SELECT_ALL, _r_obj_getstringorempty (localized_string));

		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

		_r_obj_movereference (&localized_string, _r_format_string (L"%s\tCtrl+C", _r_locale_getstring (IDS_COPY)));
		AppendMenu (hmenu, MF_STRING, IDM_COPY, _r_obj_getstringorempty (localized_string));

		column_text = _r_listview_getcolumntext (hwnd, listview_id, lv_column_current);

		if (column_text)
		{
			_r_obj_movereference (&localized_string, _r_format_string (L"%s \"%s\"", _r_locale_getstring (IDS_COPY), _r_obj_getstringorempty (column_text)));
			AppendMenu (hmenu, MF_STRING, IDM_COPY2, _r_obj_getstringorempty (localized_string));

			_r_obj_dereference (column_text);
		}

		SetMenuDefaultItem (hmenu, IDM_PROPERTIES, MF_BYCOMMAND);

		PITEM_NETWORK ptr_network = _r_obj_findhashtable (network_map, hash_item);

		if (ptr_network)
		{
			if (ptr_network->af != AF_INET || ptr_network->state != MIB_TCP_STATE_ESTAB)
				_r_menu_enableitem (hmenu, IDM_DELETE, MF_BYCOMMAND, FALSE);
		}
	}
	else if (listview_id == IDC_LOG)
	{
		_r_obj_movereference (&localized_string, _r_format_string (L"%s\tEnter", _r_locale_getstring (IDS_SHOWINLIST)));
		AppendMenu (hmenu, MF_STRING, IDM_PROPERTIES, _r_obj_getstringorempty (localized_string));

		_r_obj_movereference (&localized_string, _r_format_string (L"%s...", _r_locale_getstring (IDS_OPENRULESEDITOR)));
		AppendMenu (hmenu, MF_STRING, IDM_OPENRULESEDITOR, _r_obj_getstringorempty (localized_string));

		_r_obj_movereference (&localized_string, _r_format_string (L"%s\tCtrl+X", _r_locale_getstring (IDS_LOGCLEAR)));
		AppendMenu (hmenu, MF_STRING, IDM_TRAY_LOGCLEAR, _r_obj_getstringorempty (localized_string));

		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

		_r_obj_movereference (&localized_string, _r_format_string (L"%s\tCtrl+A", _r_locale_getstring (IDS_SELECT_ALL)));
		AppendMenu (hmenu, MF_STRING, IDM_SELECT_ALL, _r_obj_getstringorempty (localized_string));

		AppendMenu (hmenu, MF_SEPARATOR, 0, NULL);

		_r_obj_movereference (&localized_string, _r_format_string (L"%s\tCtrl+C", _r_locale_getstring (IDS_COPY)));
		AppendMenu (hmenu, MF_STRING, IDM_COPY, _r_obj_getstringorempty (localized_string));

		column_text = _r_listview_getcolumntext (hwnd, listview_id, lv_column_current);

		if (column_text)
		{
			_r_obj_movereference (&localized_string, _r_format_string (L"%s \"%s\"", _r_locale_getstring (IDS_COPY), _r_obj_getstringorempty (column_text)));
			AppendMenu (hmenu, MF_STRING, IDM_COPY2, _r_obj_getstringorempty (localized_string));

			_r_obj_dereference (column_text);
		}

		SetMenuDefaultItem (hmenu, IDM_PROPERTIES, MF_BYCOMMAND);
	}

	INT command_id = _r_menu_popup (hmenu, hwnd, NULL, FALSE);

	DestroyMenu (hmenu);

	if (command_id)
		PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (command_id, 0), (LPARAM)lv_column_current);

	SAFE_DELETE_REFERENCE (localized_string);
}


VOID _app_message_traycontextmenu (HWND hwnd)
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
}

VOID _app_message_dpichanged (HWND hwnd)
{
	PR_STRING localized_string = NULL;

	_app_imagelist_init (hwnd);

	SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_SETIMAGELIST, 0, (LPARAM)config.himg_toolbar);

	// reset toolbar information
	_app_setinterfacestate (hwnd);

	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_REFRESH, _r_locale_getstring (IDS_REFRESH), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, 0, I_IMAGENONE);
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_SETTINGS, _r_locale_getstring (IDS_SETTINGS), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, 0, I_IMAGENONE);

	_r_obj_movereference (&localized_string, _r_format_string (L"%s...", _r_locale_getstring (IDS_OPENRULESEDITOR)));
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_OPENRULESEDITOR, _r_obj_getstringorempty (localized_string), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLENOTIFICATIONS_CHK, _r_locale_getstring (IDS_ENABLENOTIFICATIONS_CHK), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLELOG_CHK, _r_locale_getstring (IDS_ENABLELOG_CHK), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLEUILOG_CHK, _r_locale_getstring (IDS_ENABLEUILOG_CHK), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_obj_movereference (&localized_string, _r_format_string (L"%s (Ctrl+I)", _r_locale_getstring (IDS_LOGSHOW)));
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_LOGSHOW, _r_obj_getstringorempty (localized_string), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_obj_movereference (&localized_string, _r_format_string (L"%s (Ctrl+X)", _r_locale_getstring (IDS_LOGCLEAR)));
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_LOGCLEAR, _r_obj_getstringorempty (localized_string), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_DONATE, _r_locale_getstring (IDS_DONATE), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_app_toolbar_resize ();

	INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, -1);

	if (listview_id)
	{
		_app_listviewsetview (hwnd, listview_id);
		_app_listviewsetfont (hwnd, listview_id, TRUE);
		_app_listviewresize (hwnd, listview_id, FALSE);
	}

	_app_refreshstatus (hwnd, 0);

	SAFE_DELETE_REFERENCE (localized_string);
}

LONG_PTR _app_message_custdraw (LPNMLVCUSTOMDRAW lpnmlv)
{
	switch (lpnmlv->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
		{
			INT ctrl_id = (INT)(INT_PTR)lpnmlv->nmcd.hdr.idFrom;

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

			INT ctrl_id = (INT)(INT_PTR)lpnmlv->nmcd.hdr.idFrom;

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

					SelectObject (lpnmlv->nmcd.hdc, (HFONT)SendMessage (lpnmlv->nmcd.hdr.hwndFrom, WM_GETFONT, 0, 0)); // fix

					SetBkMode (lpnmlv->nmcd.hdc, TRANSPARENT);
					SetTextColor (lpnmlv->nmcd.hdc, GetSysColor (COLOR_GRAYTEXT));

					if (tbi.iImage != I_IMAGENONE)
					{
						HIMAGELIST himglist = (HIMAGELIST)SendMessage (lpnmlv->nmcd.hdr.hwndFrom, TB_GETIMAGELIST, 0, 0);

						if (himglist)
						{
							ULONG padding = (ULONG)SendMessage (lpnmlv->nmcd.hdr.hwndFrom, TB_GETPADDING, 0, 0);
							UINT rebar_height = (UINT)SendMessage (GetParent (lpnmlv->nmcd.hdr.hwndFrom), RB_GETBARHEIGHT, 0, 0);

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

						DrawTextEx (lpnmlv->nmcd.hdc, text, (INT)_r_str_length (text), &lpnmlv->nmcd.rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_HIDEPREFIX, NULL);
					}

					return CDRF_SKIPDEFAULT;
				}
			}
			else
			{
				if (!_r_config_getboolean (L"IsEnableHighlighting", TRUE))
					return CDRF_DODEFAULT;

				COLORREF new_clr = 0;
				INT view_type = (INT)SendMessage (lpnmlv->nmcd.hdr.hwndFrom, LVM_GETVIEW, 0, 0);
				BOOLEAN is_tableview = (view_type == LV_VIEW_DETAILS || view_type == LV_VIEW_SMALLICON || view_type == LV_VIEW_TILE);
				BOOLEAN is_systemapp = FALSE;
				BOOLEAN is_validconnection = FALSE;

				if ((ctrl_id >= IDC_APPS_PROFILE && ctrl_id <= IDC_APPS_UWP) || ctrl_id == IDC_RULE_APPS_ID || ctrl_id == IDC_NETWORK || ctrl_id == IDC_LOG)
				{
					SIZE_T app_hash = 0;

					if (ctrl_id == IDC_NETWORK)
					{
						PITEM_NETWORK ptr_network = _r_obj_findhashtable (network_map, lpnmlv->nmcd.lItemlParam);

						if (ptr_network)
						{
							app_hash = ptr_network->app_hash;
							is_systemapp = _app_isappfromsystem (_r_obj_getstring (ptr_network->path), app_hash);
							is_validconnection = ptr_network->is_connection;
						}
					}
					else if (ctrl_id == IDC_LOG)
					{
						PITEM_LOG ptr_log = _app_getlogitem (lpnmlv->nmcd.lItemlParam);

						if (ptr_log)
						{
							app_hash = _app_getlogapp (lpnmlv->nmcd.lItemlParam);
							is_systemapp = _app_isappfromsystem (_r_obj_getstring (ptr_log->path), app_hash);

							_r_obj_dereference (ptr_log);
						}
					}
					else
					{
						app_hash = lpnmlv->nmcd.lItemlParam;
						is_validconnection = _app_isapphaveconnection (app_hash);

						PR_STRING real_path = _app_getappinfobyhash (app_hash, InfoPath);

						if (real_path)
						{
							is_systemapp = _app_isappfromsystem (_r_obj_getstring (real_path), app_hash);

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
					new_clr = _app_getrulecolor (ctrl_id, lpnmlv->nmcd.lItemlParam);
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

VOID _app_message_find (HWND hwnd, LPFINDREPLACE lpfr)
{
	if ((lpfr->Flags & FR_DIALOGTERM) != 0)
	{
		config.hfind = NULL;
	}
	else if ((lpfr->Flags & FR_FINDNEXT) != 0)
	{
		INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, -1);

		if (!listview_id)
			return;

		INT total_count = _r_listview_getitemcount (hwnd, listview_id);

		if (!total_count)
			return;

		BOOLEAN is_wrap = TRUE;

		INT selected_item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);

		INT current_item = selected_item + 1;
		INT last_item = total_count;

find_wrap:

		for (; current_item < last_item; current_item++)
		{
			PR_STRING item_text = _r_listview_getitemtext (hwnd, listview_id, current_item, 0);

			if (item_text)
			{
				if (StrStrNIW (item_text->buffer, lpfr->lpstrFindWhat, (UINT)_r_obj_getstringlength (item_text)) != NULL)
				{
					_app_showitem (hwnd, listview_id, current_item, -1);

					_r_obj_dereference (item_text);

					return;
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
}

VOID _app_message_resizewindow (HWND hwnd, LPARAM lparam)
{
	SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_AUTOSIZE, 0, 0);
	SendDlgItemMessage (hwnd, IDC_STATUSBAR, WM_SIZE, 0, 0);

	RECT rc = {0};
	GetClientRect (GetDlgItem (hwnd, IDC_STATUSBAR), &rc);

	INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, -1);
	INT statusbar_height = _r_calc_rectheight (&rc);
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

VOID _app_message_initialize (HWND hwnd)
{
	_r_tray_create (hwnd, UID, WM_TRAYICON, _r_app_getsharedimage (_r_sys_getimagebase (), (_wfp_isfiltersinstalled () != InstallDisabled) ? IDI_ACTIVE : IDI_INACTIVE, _r_dc_getsystemmetrics (hwnd, SM_CXSMICON)), APP_NAME, FALSE);

	HMENU hmenu = GetMenu (hwnd);

	if (hmenu)
	{
		if (_r_config_getboolean (L"IsInternalRulesDisabled", FALSE))
			_r_menu_enableitem (hmenu, 4, MF_BYPOSITION, FALSE);

		_r_menu_checkitem (hmenu, IDM_ALWAYSONTOP_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"AlwaysOnTop", APP_ALWAYSONTOP));
		_r_menu_checkitem (hmenu, IDM_SHOWFILENAMESONLY_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"ShowFilenames", TRUE));
		_r_menu_checkitem (hmenu, IDM_AUTOSIZECOLUMNS_CHK, 0, MF_BYCOMMAND, _r_config_getboolean (L"AutoSizeColumns", TRUE));

		{
			UINT menu_id;
			INT view_type = _r_calc_clamp (_r_config_getinteger (L"ViewType", LV_VIEW_DETAILS), LV_VIEW_ICON, LV_VIEW_MAX);

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
			INT icon_size = _r_calc_clamp (_r_config_getinteger (L"IconSize", SHIL_SYSSMALL), SHIL_LARGE, SHIL_LAST);

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

		_r_menu_checkitem (hmenu, IDM_BLOCKLIST_SPY_DISABLE, IDM_BLOCKLIST_SPY_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_SPY_DISABLE + _r_calc_clamp (_r_config_getinteger (L"BlocklistSpyState", 2), 0, 2));
		_r_menu_checkitem (hmenu, IDM_BLOCKLIST_UPDATE_DISABLE, IDM_BLOCKLIST_UPDATE_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_UPDATE_DISABLE + _r_calc_clamp (_r_config_getinteger (L"BlocklistUpdateState", 0), 0, 2));
		_r_menu_checkitem (hmenu, IDM_BLOCKLIST_EXTRA_DISABLE, IDM_BLOCKLIST_EXTRA_BLOCK, MF_BYCOMMAND, IDM_BLOCKLIST_EXTRA_DISABLE + _r_calc_clamp (_r_config_getinteger (L"BlocklistExtraState", 0), 0, 2));
	}

	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLENOTIFICATIONS_CHK, NULL, 0, _r_config_getboolean (L"IsNotificationsEnabled", TRUE) ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED, I_IMAGENONE);
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLELOG_CHK, NULL, 0, _r_config_getboolean (L"IsLogEnabled", FALSE) ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED, I_IMAGENONE);
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLEUILOG_CHK, NULL, 0, _r_config_getboolean (L"IsLogUiEnabled", FALSE) ? TBSTATE_PRESSED | TBSTATE_ENABLED : TBSTATE_ENABLED, I_IMAGENONE);
}

VOID _app_message_localize (HWND hwnd)
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

		_r_menu_setitemtext (hmenu, IDM_VIEW_DETAILS, FALSE, _r_locale_getstring (IDS_VIEW_DETAILS));
		_r_menu_setitemtext (hmenu, IDM_VIEW_ICON, FALSE, _r_locale_getstring (IDS_VIEW_ICON));
		_r_menu_setitemtext (hmenu, IDM_VIEW_TILE, FALSE, _r_locale_getstring (IDS_VIEW_TILE));

		_r_menu_setitemtext (hmenu, IDM_ICONSISHIDDEN, FALSE, _r_locale_getstring (IDS_ICONSISHIDDEN));

		_r_menu_setitemtextformat (GetSubMenu (hmenu, 2), LANG_MENU, TRUE, L"%s (Language)", _r_locale_getstring (IDS_LANGUAGE));

		_r_menu_setitemtextformat (hmenu, IDM_FONT, FALSE, L"%s...", _r_locale_getstring (IDS_FONT));

		LPCWSTR recommended_string = _r_locale_getstring (IDS_RECOMMENDED);

		_r_menu_setitemtext (hmenu, IDM_CONNECTIONS_TITLE, FALSE, _r_locale_getstring (IDS_TAB_NETWORK));
		_r_menu_setitemtext (hmenu, IDM_SECURITY_TITLE, FALSE, _r_locale_getstring (IDS_TITLE_SECURITY));
		_r_menu_setitemtext (hmenu, IDM_ADVANCED_TITLE, FALSE, _r_locale_getstring (IDS_TITLE_ADVANCED));

		_r_menu_enableitem (hmenu, IDM_CONNECTIONS_TITLE, MF_BYCOMMAND, FALSE);
		_r_menu_enableitem (hmenu, IDM_SECURITY_TITLE, MF_BYCOMMAND, FALSE);
		_r_menu_enableitem (hmenu, IDM_ADVANCED_TITLE, MF_BYCOMMAND, FALSE);

		_r_menu_setitemtextformat (hmenu, IDM_RULE_BLOCKOUTBOUND, FALSE, L"%s (%s)", _r_locale_getstring (IDS_RULE_BLOCKOUTBOUND), recommended_string);
		_r_menu_setitemtextformat (hmenu, IDM_RULE_BLOCKINBOUND, FALSE, L"%s (%s)", _r_locale_getstring (IDS_RULE_BLOCKINBOUND), recommended_string);
		_r_menu_setitemtextformat (hmenu, IDM_RULE_ALLOWLOOPBACK, FALSE, L"%s (%s)", _r_locale_getstring (IDS_RULE_ALLOWLOOPBACK), recommended_string);
		_r_menu_setitemtextformat (hmenu, IDM_RULE_ALLOW6TO4, FALSE, L"%s (%s)", _r_locale_getstring (IDS_RULE_ALLOW6TO4), recommended_string);
		_r_menu_setitemtextformat (hmenu, IDM_SECUREFILTERS_CHK, FALSE, L"%s (%s)", _r_locale_getstring (IDS_SECUREFILTERS_CHK), recommended_string);
		_r_menu_setitemtextformat (hmenu, IDM_USESTEALTHMODE_CHK, FALSE, L"%s (%s)", _r_locale_getstring (IDS_USESTEALTHMODE_CHK), recommended_string);
		_r_menu_setitemtextformat (hmenu, IDM_INSTALLBOOTTIMEFILTERS_CHK, FALSE, L"%s (%s)", _r_locale_getstring (IDS_INSTALLBOOTTIMEFILTERS_CHK), recommended_string);

		_r_menu_setitemtext (hmenu, IDM_USENETWORKRESOLUTION_CHK, FALSE, _r_locale_getstring (IDS_USENETWORKRESOLUTION_CHK));
		_r_menu_setitemtext (hmenu, IDM_USECERTIFICATES_CHK, FALSE, _r_locale_getstring (IDS_USECERTIFICATES_CHK));
		_r_menu_setitemtextformat (hmenu, IDM_USEREFRESHDEVICES_CHK, FALSE, L"%s (%s)", _r_locale_getstring (IDS_USEREFRESHDEVICES_CHK), recommended_string);

		_r_menu_setitemtext (hmenu, IDM_BLOCKLIST_SPY_TITLE, FALSE, _r_locale_getstring (IDS_BLOCKLIST_SPY));
		_r_menu_setitemtext (hmenu, IDM_BLOCKLIST_UPDATE_TITLE, FALSE, _r_locale_getstring (IDS_BLOCKLIST_UPDATE));
		_r_menu_setitemtext (hmenu, IDM_BLOCKLIST_EXTRA_TITLE, FALSE, _r_locale_getstring (IDS_BLOCKLIST_EXTRA));

		_r_menu_enableitem (hmenu, IDM_BLOCKLIST_SPY_TITLE, MF_BYCOMMAND, FALSE);
		_r_menu_enableitem (hmenu, IDM_BLOCKLIST_UPDATE_TITLE, MF_BYCOMMAND, FALSE);
		_r_menu_enableitem (hmenu, IDM_BLOCKLIST_EXTRA_TITLE, MF_BYCOMMAND, FALSE);

		_r_menu_setitemtext (hmenu, IDM_BLOCKLIST_SPY_DISABLE, FALSE, _r_locale_getstring (IDS_DISABLE));
		_r_menu_setitemtext (hmenu, IDM_BLOCKLIST_SPY_ALLOW, FALSE, _r_locale_getstring (IDS_ACTION_ALLOW));
		_r_menu_setitemtextformat (hmenu, IDM_BLOCKLIST_SPY_BLOCK, FALSE, L"%s (%s)", _r_locale_getstring (IDS_ACTION_BLOCK), recommended_string);
		_r_menu_setitemtextformat (hmenu, IDM_BLOCKLIST_UPDATE_DISABLE, FALSE, L"%s (%s)", _r_locale_getstring (IDS_DISABLE), recommended_string);

		_r_menu_setitemtext (hmenu, IDM_BLOCKLIST_UPDATE_ALLOW, FALSE, _r_locale_getstring (IDS_ACTION_ALLOW));
		_r_menu_setitemtext (hmenu, IDM_BLOCKLIST_UPDATE_BLOCK, FALSE, _r_locale_getstring (IDS_ACTION_BLOCK));

		_r_menu_setitemtextformat (hmenu, IDM_BLOCKLIST_EXTRA_DISABLE, FALSE, L"%s (%s)", _r_locale_getstring (IDS_DISABLE), recommended_string);

		_r_menu_setitemtext (hmenu, IDM_BLOCKLIST_EXTRA_ALLOW, FALSE, _r_locale_getstring (IDS_ACTION_ALLOW));
		_r_menu_setitemtext (hmenu, IDM_BLOCKLIST_EXTRA_BLOCK, FALSE, _r_locale_getstring (IDS_ACTION_BLOCK));

		_r_menu_setitemtext (hmenu, IDM_WEBSITE, FALSE, _r_locale_getstring (IDS_WEBSITE));
		_r_menu_setitemtext (hmenu, IDM_CHECKUPDATES, FALSE, _r_locale_getstring (IDS_CHECKUPDATES));

		_r_menu_setitemtextformat (hmenu, IDM_ABOUT, FALSE, L"%s\tF1", _r_locale_getstring (IDS_ABOUT));

		_r_locale_enum (GetSubMenu (hmenu, 2), LANG_MENU, IDX_LANGUAGE); // enum localizations
	}

	PR_STRING localized_string = NULL;

	// localize toolbar
	_app_setinterfacestate (hwnd);

	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_REFRESH, _r_locale_getstring (IDS_REFRESH), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, 0, I_IMAGENONE);
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_SETTINGS, _r_locale_getstring (IDS_SETTINGS), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, 0, I_IMAGENONE);

	_r_obj_movereference (&localized_string, _r_format_string (L"%s...", _r_locale_getstring (IDS_OPENRULESEDITOR)));
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_OPENRULESEDITOR, _r_obj_getstringorempty (localized_string), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLENOTIFICATIONS_CHK, _r_locale_getstring (IDS_ENABLENOTIFICATIONS_CHK), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLELOG_CHK, _r_locale_getstring (IDS_ENABLELOG_CHK), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_obj_movereference (&localized_string, _r_format_string (L"%s (session only)", _r_locale_getstring (IDS_ENABLEUILOG_CHK)));
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_ENABLEUILOG_CHK, _r_obj_getstringorempty (localized_string), BTNS_CHECK | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_obj_movereference (&localized_string, _r_format_string (L"%s (Ctrl+I)", _r_locale_getstring (IDS_LOGSHOW)));
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_LOGSHOW, _r_obj_getstringorempty (localized_string), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_obj_movereference (&localized_string, _r_format_string (L"%s (Ctrl+X)", _r_locale_getstring (IDS_LOGCLEAR)));
	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_TRAY_LOGCLEAR, _r_obj_getstringorempty (localized_string), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	_r_toolbar_setbutton (config.hrebar, IDC_TOOLBAR, IDM_DONATE, _r_locale_getstring (IDS_DONATE), BTNS_BUTTON | BTNS_AUTOSIZE, 0, I_IMAGENONE);

	// set rebar size
	_app_toolbar_resize ();

	// localize tabs
	for (INT i = 0; i < (INT)SendDlgItemMessage (hwnd, IDC_TAB, TCM_GETITEMCOUNT, 0, 0); i++)
	{
		INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, i);

		UINT locale_id;

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
			_r_listview_setcolumn (hwnd, listview_id, 1, _r_obj_getstringorempty (localized_string), 0);

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_LOCAL L")", _r_locale_getstring (IDS_PORT)));
			_r_listview_setcolumn (hwnd, listview_id, 2, _r_obj_getstringorempty (localized_string), 0);

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_REMOTE L")", _r_locale_getstring (IDS_ADDRESS)));
			_r_listview_setcolumn (hwnd, listview_id, 3, _r_obj_getstringorempty (localized_string), 0);

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_REMOTE L")", _r_locale_getstring (IDS_PORT)));
			_r_listview_setcolumn (hwnd, listview_id, 4, _r_obj_getstringorempty (localized_string), 0);

			_r_listview_setcolumn (hwnd, listview_id, 5, _r_locale_getstring (IDS_PROTOCOL), 0);
			_r_listview_setcolumn (hwnd, listview_id, 6, _r_locale_getstring (IDS_STATE), 0);
		}
		else if (listview_id == IDC_LOG)
		{
			_r_listview_setcolumn (hwnd, listview_id, 0, _r_locale_getstring (IDS_NAME), 0);

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_LOCAL L")", _r_locale_getstring (IDS_ADDRESS)));
			_r_listview_setcolumn (hwnd, listview_id, 1, _r_obj_getstringorempty (localized_string), 0);

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_LOCAL L")", _r_locale_getstring (IDS_PORT)));
			_r_listview_setcolumn (hwnd, listview_id, 2, _r_obj_getstringorempty (localized_string), 0);

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_REMOTE L")", _r_locale_getstring (IDS_ADDRESS)));
			_r_listview_setcolumn (hwnd, listview_id, 3, _r_obj_getstringorempty (localized_string), 0);

			_r_obj_movereference (&localized_string, _r_format_string (L"%s (" SZ_DIRECTION_REMOTE L")", _r_locale_getstring (IDS_PORT)));
			_r_listview_setcolumn (hwnd, listview_id, 4, _r_obj_getstringorempty (localized_string), 0);

			_r_listview_setcolumn (hwnd, listview_id, 5, _r_locale_getstring (IDS_PROTOCOL), 0);
			_r_listview_setcolumn (hwnd, listview_id, 6, _r_locale_getstring (IDS_FILTER), 0);
			_r_listview_setcolumn (hwnd, listview_id, 7, _r_locale_getstring (IDS_DIRECTION), 0);
			_r_listview_setcolumn (hwnd, listview_id, 8, _r_locale_getstring (IDS_STATE), 0);
		}
	}

	INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, -1);

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

	SAFE_DELETE_REFERENCE (localized_string);
}

VOID _app_command_idtotimers (HWND hwnd, INT ctrl_id)
{
	INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, -1);

	if (!listview_id || !SendDlgItemMessage (hwnd, listview_id, LVM_GETSELECTEDCOUNT, 0, 0))
		return;

	SIZE_T timer_idx = (SIZE_T)ctrl_id - IDX_TIMER;
	PLONG64 seconds = _r_obj_getarrayitem (timers, timer_idx);
	INT item = -1;

	if (_wfp_isfiltersinstalled ())
	{
		HANDLE hengine = _wfp_getenginehandle ();

		if (hengine)
		{
			PR_LIST rules = _r_obj_createlistex (0x20, NULL);

			while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != -1)
			{
				SIZE_T app_hash = _r_listview_getitemlparam (hwnd, listview_id, item);
				PITEM_APP ptr_app = _r_obj_findhashtable (apps, app_hash);

				if (!ptr_app)
					continue;

				if (seconds)
					_app_timer_set (hwnd, ptr_app, *seconds);

				_r_obj_addlistitem (rules, ptr_app);
			}

			_wfp_create3filters (hengine, rules, __LINE__, FALSE);

			_r_obj_dereference (rules);
		}
	}

	_app_listviewsort (hwnd, listview_id, -1, FALSE);
	_app_refreshstatus (hwnd, listview_id);

	_app_profile_save ();
}

VOID _app_command_logshow (HWND hwnd)
{
	if (_r_config_getboolean (L"IsLogUiEnabled", FALSE))
	{
		_r_wnd_toggle (hwnd, TRUE);

		if (!_r_obj_islistempty (log_arr))
		{
			_app_showitem (hwnd, IDC_LOG, _app_getposition (hwnd, IDC_LOG, (_r_obj_getlistsize (log_arr) - 1)), -1);
		}
		else
		{
			_app_settab_id (hwnd, IDC_LOG);
		}
	}
	else
	{
		PR_STRING log_path;
		PR_STRING viewer_path;
		PR_STRING process_path;

		log_path = _r_str_expandenvironmentstring (_r_config_getstring (L"LogPath", LOG_PATH_DEFAULT));

		if (!log_path)
			return;

		if (!_r_fs_exists (log_path->buffer))
		{
			_r_obj_dereference (log_path);
			return;
		}

		if (_r_fs_isvalidhandle (config.hlogfile))
			FlushFileBuffers (config.hlogfile);

		viewer_path = _app_getlogviewer ();

		if (viewer_path)
		{
			process_path = _r_format_string (L"%s \"%s\"", _r_obj_getstring (viewer_path), log_path->buffer);

			if (process_path)
			{
				if (!_r_sys_createprocess (NULL, process_path->buffer, NULL))
					_r_show_errormessage (hwnd, NULL, GetLastError (), viewer_path->buffer, NULL);

				_r_obj_dereference (process_path);
			}

			_r_obj_dereference (viewer_path);
		}

		_r_obj_dereference (log_path);
	}
}

VOID _app_command_logclear (HWND hwnd)
{
	PR_STRING path = _r_str_expandenvironmentstring (_r_config_getstring (L"LogPath", LOG_PATH_DEFAULT));

	if (_r_fs_isvalidhandle (config.hlogfile) || (path && _r_fs_exists (path->buffer)) || !_r_obj_islistempty (log_arr))
	{
		if (_r_show_confirmmessage (hwnd, NULL, _r_locale_getstring (IDS_QUESTION), L"ConfirmLogClear"))
		{
			_app_freelogstack ();

			_app_logclear ();
			_app_logclear_ui (hwnd);
		}
	}

	SAFE_DELETE_REFERENCE (path);
}

VOID _app_command_logerrshow (HWND hwnd)
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
			process_path = _r_format_string (L"%s \"%s\"", viewer_path->buffer, log_path);

			if (process_path)
			{
				if (!_r_sys_createprocess (NULL, process_path->buffer, NULL))
					_r_show_errormessage (hwnd, NULL, GetLastError (), viewer_path->buffer, NULL);

				_r_obj_dereference (process_path);
			}

			_r_obj_dereference (viewer_path);
		}
	}
}

VOID _app_command_logerrclear (HWND hwnd)
{
	if (!_r_show_confirmmessage (hwnd, NULL, _r_locale_getstring (IDS_QUESTION), L"ConfirmLogClear"))
		return;

	LPCWSTR path = _r_app_getlogpath ();

	if (!_r_fs_exists (path))
		return;

	_r_fs_remove (path, PR_FLAG_REMOVE_FORCE);
}

VOID _app_command_copy (HWND hwnd, INT ctrl_id, INT column_id)
{
	R_STRINGBUILDER buffer;
	PR_STRING string;
	INT listview_id;
	INT column_count;
	INT item;

	listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, -1);
	item = -1;

	column_count = _r_listview_getcolumncount (hwnd, listview_id);

	_r_obj_initializestringbuilder (&buffer);

	while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != -1)
	{
		if (ctrl_id == IDM_COPY)
		{
			for (INT i = 0; i < column_count; i++)
			{
				string = _r_listview_getitemtext (hwnd, listview_id, item, i);

				if (string)
				{
					if (!_r_obj_isstringempty (string))
						_r_obj_appendstringbuilderformat (&buffer, L"%s%s", string->buffer, DIVIDER_COPY);

					_r_obj_dereference (string);
				}
			}

			string = _r_obj_finalstringbuilder (&buffer);

			_r_obj_trimstring (string, DIVIDER_COPY);
		}
		else
		{
			string = _r_listview_getitemtext (hwnd, listview_id, item, column_id);

			if (string)
			{
				if (!_r_obj_isstringempty (string))
					_r_obj_appendstringbuilder (&buffer, string->buffer);

				_r_obj_dereference (string);
			}
		}

		_r_obj_appendstringbuilder (&buffer, L"\r\n");
	}

	string = _r_obj_finalstringbuilder (&buffer);

	_r_obj_trimstring (string, DIVIDER_TRIM);

	if (!_r_obj_isstringempty (string))
		_r_clipboard_set (hwnd, string->buffer, _r_obj_getstringlength (string));

	_r_obj_deletestringbuilder (&buffer);
}

VOID _app_command_checkbox (HWND hwnd, INT ctrl_id)
{
	PR_LIST rules = _r_obj_createlistex (0x400, NULL);
	INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, -1);
	INT item = -1;
	BOOLEAN new_val = (ctrl_id == IDM_CHECK);
	BOOLEAN is_changed = FALSE;

	if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
	{
		while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != -1)
		{
			SIZE_T app_hash = _r_listview_getitemlparam (hwnd, listview_id, item);
			PITEM_APP ptr_app = _r_obj_findhashtable (apps, app_hash);

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
					_app_freenotify (ptr_app);
				}

				ptr_app->is_enabled = new_val;

				_r_spinlock_acquireshared (&lock_checkbox);
				_app_setappiteminfo (hwnd, listview_id, item, ptr_app);
				_r_spinlock_releaseshared (&lock_checkbox);

				_r_obj_addlistitem (rules, ptr_app);

				is_changed = TRUE;

				// do not reset reference counter
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
		while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != -1)
		{
			SIZE_T rule_idx = _r_listview_getitemlparam (hwnd, listview_id, item);
			PITEM_RULE ptr_rule = _app_getrulebyid (rule_idx);

			if (!ptr_rule)
				continue;

			if (ptr_rule->is_enabled != new_val)
			{
				_app_ruleenable (ptr_rule, new_val, TRUE);

				_r_spinlock_acquireshared (&lock_checkbox);
				_app_setruleiteminfo (hwnd, listview_id, item, ptr_rule, TRUE);
				_r_spinlock_releaseshared (&lock_checkbox);

				_r_obj_addlistitem (rules, ptr_rule);

				is_changed = TRUE;

				// do not reset reference counter
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
		_app_listviewsort (hwnd, listview_id, -1, FALSE);
		_app_refreshstatus (hwnd, listview_id);

		_app_profile_save ();
	}
}

VOID _app_command_delete (HWND hwnd)
{
	MIB_TCPROW tcprow;
	WCHAR message_text[512];
	PR_ARRAY guids = NULL;
	INT listview_id;
	INT selected;
	INT count;

	listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, -1);

	if (listview_id != IDC_APPS_PROFILE && listview_id != IDC_RULES_CUSTOM && listview_id != IDC_NETWORK)
		return;

	if (!(selected = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETSELECTEDCOUNT, 0, 0)))
		return;

	if (listview_id != IDC_NETWORK)
	{
		_r_str_printf (message_text, RTL_NUMBER_OF (message_text), _r_locale_getstring (IDS_QUESTION_DELETE), selected);

		if (_r_show_message (hwnd, MB_YESNO | MB_ICONEXCLAMATION, NULL, NULL, message_text) != IDYES)
			return;

		guids = _r_obj_createarrayex (sizeof (GUID), 0x100, NULL);
	}

	count = _r_listview_getitemcount (hwnd, listview_id) - 1;

	for (INT i = count; i != -1; i--)
	{
		if (!_r_listview_isitemselected (hwnd, listview_id, i))
			continue;

		if (listview_id == IDC_APPS_PROFILE)
		{
			SIZE_T app_hash = _r_listview_getitemlparam (hwnd, listview_id, i);
			PITEM_APP ptr_app = _r_obj_findhashtable (apps, app_hash);

			if (!ptr_app)
				continue;

			if (!ptr_app->is_undeletable) // skip "undeletable" apps
			{
				if (!_r_obj_isarrayempty (ptr_app->guids))
					_r_obj_addarrayitems (guids, ptr_app->guids->items, ptr_app->guids->count);

				SendDlgItemMessage (hwnd, listview_id, LVM_DELETEITEM, (WPARAM)i, 0);

				_app_timer_reset (hwnd, ptr_app);
				_app_freenotify (ptr_app);

				_app_freeapplication (app_hash);
			}
		}
		else if (listview_id == IDC_RULES_CUSTOM)
		{
			SIZE_T rule_idx = _r_listview_getitemlparam (hwnd, listview_id, i);
			PITEM_RULE ptr_rule = _app_getrulebyid (rule_idx);
			PR_HASHTABLE apps_checker;
			PR_HASHSTORE hashstore;
			SIZE_T hash_code;
			SIZE_T enum_key = 0;

			if (!ptr_rule)
				continue;

			if (!ptr_rule->is_readonly) // skip "read-only" rules
			{
				apps_checker = _r_obj_createhashtable (sizeof (R_HASHSTORE), NULL);

				if (!_r_obj_isarrayempty (ptr_rule->guids))
					_r_obj_addarrayitems (guids, ptr_rule->guids->items, ptr_rule->guids->count);

				while (_r_obj_enumhashtable (ptr_rule->apps, &hashstore, &hash_code, &enum_key))
					_app_addcachetable (apps_checker, hash_code, NULL, 0);

				SendDlgItemMessage (hwnd, listview_id, LVM_DELETEITEM, (WPARAM)i, 0);

				SecureZeroMemory (ptr_rule, sizeof (ITEM_RULE));

				//rules_arr.at (rule_idx) = NULL;

				enum_key = 0;

				while (_r_obj_enumhashtable (apps_checker, &hashstore, &hash_code, &enum_key))
				{
					PITEM_APP ptr_app = _r_obj_findhashtable (apps, hash_code);

					if (!ptr_app)
						continue;

					INT app_listview_id = _app_getlistview_id (ptr_app->type);

					if (app_listview_id)
					{
						INT item_pos = _app_getposition (hwnd, app_listview_id, hash_code);

						if (item_pos != -1)
						{
							_r_spinlock_acquireshared (&lock_checkbox);
							_app_setappiteminfo (hwnd, app_listview_id, item_pos, ptr_app);
							_r_spinlock_releaseshared (&lock_checkbox);
						}
					}
				}
			}
		}
		else if (listview_id == IDC_NETWORK)
		{
			SIZE_T network_hash = _r_listview_getitemlparam (hwnd, listview_id, i);
			PITEM_NETWORK ptr_network = _r_obj_findhashtable (network_map, network_hash);

			if (!ptr_network)
				continue;

			if (ptr_network->af == AF_INET && ptr_network->state == MIB_TCP_STATE_ESTAB)
			{
				memset (&tcprow, 0, sizeof (tcprow));

				tcprow.dwState = MIB_TCP_STATE_DELETE_TCB;
				tcprow.dwLocalAddr = ptr_network->local_addr.S_un.S_addr;
				tcprow.dwLocalPort = _r_byteswap_ushort ((USHORT)ptr_network->local_port);
				tcprow.dwRemoteAddr = ptr_network->remote_addr.S_un.S_addr;
				tcprow.dwRemotePort = _r_byteswap_ushort ((USHORT)ptr_network->remote_port);

				if (SetTcpEntry (&tcprow) == NO_ERROR)
				{
					SendDlgItemMessage (hwnd, listview_id, LVM_DELETEITEM, (WPARAM)i, 0);

					_r_obj_removehashtableentry (network_map, network_hash);

					continue;
				}
			}
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

	_app_refreshstatus (hwnd, -1);
	_app_profile_save ();
}

VOID _app_command_disable (HWND hwnd, INT ctrl_id)
{
	INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, -1);

	// note: these commands only for profile...
	if (!(listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP))
		return;

	INT item = -1;
	BOOL new_val = -1;

	_r_spinlock_acquireshared (&lock_apps);

	while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != -1)
	{
		SIZE_T app_hash = _r_listview_getitemlparam (hwnd, listview_id, item);
		PITEM_APP ptr_app = _r_obj_findhashtable (apps, app_hash);

		if (!ptr_app)
			continue;

		if (ctrl_id == IDM_DISABLENOTIFICATIONS)
		{
			if (new_val == -1)
				new_val = !ptr_app->is_silent;

			ptr_app->is_silent = !!new_val;

			if (new_val)
				_app_freenotify (ptr_app);
		}
		else if (ctrl_id == IDM_DISABLETIMER)
		{
			_app_timer_reset (hwnd, ptr_app);
		}
	}

	_r_spinlock_releaseshared (&lock_apps);

	_app_listviewsort (hwnd, listview_id, -1, FALSE);
	_app_refreshstatus (hwnd, listview_id);

	_app_profile_save ();
}

VOID _app_command_openeditor (HWND hwnd)
{
	PITEM_RULE ptr_rule = _app_addrule (NULL, NULL, NULL, FWP_DIRECTION_OUTBOUND, 0, 0);

	_app_ruleenable (ptr_rule, TRUE, FALSE);

	ptr_rule->type = DataRuleUser;
	ptr_rule->is_block = FALSE;

	INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, -1);

	if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
	{
		INT item = -1;

		while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != -1)
		{
			SIZE_T app_hash = _r_listview_getitemlparam (hwnd, listview_id, item);

			if (_r_obj_findhashtable (apps, app_hash))
			{
				_app_addcachetable (ptr_rule->apps, app_hash, NULL, 0);
			}
		}
	}
	else if (listview_id == IDC_NETWORK)
	{
		ptr_rule->is_block = TRUE;

		INT item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);

		if (item != -1)
		{
			SIZE_T network_hash = _r_listview_getitemlparam (hwnd, listview_id, item);
			PITEM_NETWORK ptr_network = _r_obj_findhashtable (network_map, network_hash);

			if (ptr_network)
			{
				if (!ptr_rule->name)
				{
					ptr_rule->name = _r_listview_getitemtext (hwnd, listview_id, item, 0);
				}

				if (ptr_network->app_hash && !_r_obj_isstringempty (ptr_network->path))
				{
					if (!_r_obj_findhashtable (apps, ptr_network->app_hash))
					{
						_app_addapplication (hwnd, DataUnknown, ptr_network->path->buffer, NULL, NULL);

						_app_refreshstatus (hwnd, listview_id);
						_app_profile_save ();
					}

					_app_addcachetable (ptr_rule->apps, ptr_network->app_hash, NULL, 0);
				}

				ptr_rule->protocol = ptr_network->protocol;

				_r_obj_movereference (&ptr_rule->rule_remote, _app_formataddress (ptr_network->af, 0, &ptr_network->remote_addr, ptr_network->remote_port, FMTADDR_AS_RULE));
				_r_obj_movereference (&ptr_rule->rule_local, _app_formataddress (ptr_network->af, 0, &ptr_network->local_addr, ptr_network->local_port, FMTADDR_AS_RULE));
			}
		}
	}
	else if (listview_id == IDC_LOG)
	{
		//ptr_rule->is_block = FALSE;

		INT item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);

		if (item != -1)
		{
			SIZE_T log_idx = _r_listview_getitemlparam (hwnd, listview_id, item);
			PITEM_LOG ptr_log = _app_getlogitem (log_idx);

			if (ptr_log)
			{
				if (!ptr_rule->name)
				{
					ptr_rule->name = _r_listview_getitemtext (hwnd, listview_id, item, 0);
				}

				if (ptr_log->app_hash && !_r_obj_isstringempty (ptr_log->path))
				{
					if (!_r_obj_findhashtable (apps, ptr_log->app_hash))
					{
						_app_addapplication (hwnd, DataUnknown, ptr_log->path->buffer, NULL, NULL);

						_app_refreshstatus (hwnd, listview_id);
						_app_profile_save ();
					}

					_app_addcachetable (ptr_rule->apps, ptr_log->app_hash, NULL, 0);
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
		SIZE_T rule_idx = _r_obj_addarrayitem (rules_arr, ptr_rule);
		INT listview_rules_id = _app_getlistview_id (DataRuleUser);

		if (listview_rules_id)
		{
			INT item_id = _r_listview_getitemcount (hwnd, listview_rules_id);

			_r_spinlock_acquireshared (&lock_checkbox);

			_r_listview_additemex (hwnd, listview_rules_id, item_id, 0, _r_obj_getstringordefault (ptr_rule->name, SZ_EMPTY), _app_getruleicon (ptr_rule), _app_getrulegroup (ptr_rule), rule_idx);
			_app_setruleiteminfo (hwnd, listview_rules_id, item_id, ptr_rule, TRUE);

			_r_spinlock_releaseshared (&lock_checkbox);
		}

		_app_listviewsort (hwnd, listview_id, -1, FALSE);
		_app_refreshstatus (hwnd, listview_id);

		_app_profile_save ();
	}

	_r_mem_free (ptr_rule);
}

VOID _app_command_properties (HWND hwnd)
{
	INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, -1);
	INT item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);

	if (item == -1)
		return;

	if (listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_APPS_UWP)
	{
		ITEM_CONTEXT context = {0};
		SIZE_T app_hash = _r_listview_getitemlparam (hwnd, listview_id, item);
		PITEM_APP ptr_app = _r_obj_findhashtable (apps, app_hash);

		if (!ptr_app)
			return;

		context.is_settorules = FALSE;
		context.ptr_app = ptr_app;

		if (DialogBoxParam (NULL, MAKEINTRESOURCE (IDD_EDITOR), hwnd, &PropertiesProc, (LPARAM)&context))
		{
			_r_spinlock_acquireshared (&lock_checkbox);
			_app_setappiteminfo (hwnd, listview_id, item, ptr_app);
			_r_spinlock_releaseshared (&lock_checkbox);

			_app_listviewsort (hwnd, listview_id, -1, FALSE);
			_app_refreshstatus (hwnd, listview_id);

			_app_profile_save ();
		}
	}
	else if (listview_id >= IDC_RULES_BLOCKLIST && listview_id <= IDC_RULES_CUSTOM)
	{
		ITEM_CONTEXT context = {0};
		SIZE_T rule_idx = _r_listview_getitemlparam (hwnd, listview_id, item);
		PITEM_RULE ptr_rule = _app_getrulebyid (rule_idx);

		if (!ptr_rule)
			return;

		context.is_settorules = TRUE;
		context.ptr_rule = ptr_rule;

		if (DialogBoxParam (NULL, MAKEINTRESOURCE (IDD_EDITOR), hwnd, &PropertiesProc, (LPARAM)&context))
		{
			_r_spinlock_acquireshared (&lock_checkbox);
			_app_setruleiteminfo (hwnd, listview_id, item, ptr_rule, TRUE);
			_r_spinlock_releaseshared (&lock_checkbox);

			_app_listviewsort (hwnd, listview_id, -1, FALSE);
			_app_refreshstatus (hwnd, listview_id);

			_app_profile_save ();
		}
	}
	else if (listview_id == IDC_NETWORK)
	{
		SIZE_T network_hash = _r_listview_getitemlparam (hwnd, listview_id, item);
		PITEM_NETWORK ptr_network = _r_obj_findhashtable (network_map, network_hash);

		if (!ptr_network)
			return;

		if (ptr_network->app_hash && !_r_obj_isstringempty (ptr_network->path))
		{
			if (!_r_obj_findhashtable (apps, ptr_network->app_hash))
			{
				_app_addapplication (hwnd, DataUnknown, ptr_network->path->buffer, NULL, NULL);

				_app_refreshstatus (hwnd, listview_id);
				_app_profile_save ();
			}

			INT app_listview_id = PtrToInt (_app_getappinfobyhash (ptr_network->app_hash, InfoListviewId));

			if (app_listview_id)
			{
				_app_listviewsort (hwnd, app_listview_id, -1, FALSE);
				_app_showitem (hwnd, app_listview_id, _app_getposition (hwnd, app_listview_id, ptr_network->app_hash), -1);
			}
		}
	}
	else if (listview_id == IDC_LOG)
	{
		SIZE_T log_idx = _r_listview_getitemlparam (hwnd, listview_id, item);
		PITEM_LOG ptr_log = _app_getlogitem (log_idx);

		if (!ptr_log)
			return;

		if (ptr_log->app_hash && !_r_obj_isstringempty (ptr_log->path))
		{
			if (!_r_obj_findhashtable (apps, ptr_log->app_hash))
			{
				_app_addapplication (hwnd, DataUnknown, ptr_log->path->buffer, NULL, NULL);

				_app_refreshstatus (hwnd, listview_id);
				_app_profile_save ();
			}

			INT app_listview_id = PtrToInt (_app_getappinfobyhash (ptr_log->app_hash, InfoListviewId));

			if (app_listview_id)
			{
				_app_listviewsort (hwnd, app_listview_id, -1, FALSE);
				_app_showitem (hwnd, app_listview_id, _app_getposition (hwnd, app_listview_id, ptr_log->app_hash), -1);
			}
		}

		_r_obj_dereference (ptr_log);
	}
}

VOID _app_command_purgeunused (HWND hwnd)
{
	BOOLEAN is_deleted = FALSE;

	PITEM_APP ptr_app;
	SIZE_T enum_key = 0;

	PR_ARRAY guids;
	PR_ARRAY apps_list;

	guids = _r_obj_createarrayex (sizeof (GUID), 1024, NULL);
	apps_list = _r_obj_createarray (sizeof (SIZE_T), NULL);

	_r_spinlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtable (apps, &ptr_app, NULL, &enum_key))
	{
		if (!ptr_app->is_undeletable && (!_app_isappexists (ptr_app) || ((ptr_app->type != DataAppService && ptr_app->type != DataAppUWP) && !_app_isappused (ptr_app))))
		{
			INT app_listview_id = _app_getlistview_id (ptr_app->type);

			if (app_listview_id)
			{
				INT item_pos = _app_getposition (hwnd, app_listview_id, ptr_app->app_hash);

				if (item_pos != -1)
					SendDlgItemMessage (hwnd, app_listview_id, LVM_DELETEITEM, (WPARAM)item_pos, 0);
			}

			_app_timer_reset (hwnd, ptr_app);
			_app_freenotify (ptr_app);

			if (!_r_obj_isarrayempty (ptr_app->guids))
				_r_obj_addarrayitems (guids, ptr_app->guids->items, ptr_app->guids->count);

			_r_obj_addarrayitem (apps_list, &ptr_app->app_hash);

			is_deleted = TRUE;
		}
	}

	_r_spinlock_releaseshared (&lock_apps);

	_r_spinlock_acquireexclusive (&lock_apps);

	for (SIZE_T i = 0; i < _r_obj_getarraysize (apps_list); i++)
	{
		PSIZE_T hash = _r_obj_getarrayitem (apps_list, i);

		if (hash)
		{
			_app_freeapplication (*hash);
		}
	}

	_r_spinlock_releaseexclusive (&lock_apps);

	if (is_deleted)
	{
		if (_wfp_isfiltersinstalled ())
		{
			HANDLE hengine = _wfp_getenginehandle ();

			if (hengine)
				_wfp_destroyfilters_array (hengine, guids, __LINE__);
		}

		_app_refreshstatus (hwnd, -1);
		_app_profile_save ();
	}

	_r_obj_dereference (guids);
	_r_obj_dereference (apps_list);
}

VOID _app_command_purgetimers (HWND hwnd)
{
	if (!_app_istimersactive () || _r_show_message (hwnd, MB_YESNO | MB_ICONEXCLAMATION, NULL, NULL, _r_locale_getstring (IDS_QUESTION_TIMERS)) != IDYES)
		return;

	PR_LIST rules = _r_obj_createlist (NULL);
	PITEM_APP ptr_app;
	SIZE_T enum_key = 0;

	_r_spinlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtable (apps, &ptr_app, NULL, &enum_key))
	{
		if (_app_istimerset (ptr_app->htimer))
		{
			_app_timer_reset (hwnd, ptr_app);

			_r_obj_addlistitem (rules, ptr_app);
		}
	}

	_r_spinlock_releaseshared (&lock_apps);

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

	_app_profile_save ();

	INT listview_id = (INT)_r_tab_getlparam (hwnd, IDC_TAB, -1);

	if (listview_id)
	{
		_app_listviewsort (hwnd, listview_id, -1, FALSE);
		_app_refreshstatus (hwnd, listview_id);
	}
}

VOID _app_command_selectfont (HWND hwnd)
{
	CHOOSEFONT cf = {0};

	LOGFONT lf = {0};

	cf.lStructSize = sizeof (cf);
	cf.hwndOwner = hwnd;
	cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_NOSCRIPTSEL | CF_LIMITSIZE | CF_NOVERTFONTS;
	cf.nSizeMax = 16;
	cf.nSizeMin = 8;
	cf.lpLogFont = &lf;

	_r_config_getfont (L"Font", hwnd, &lf, NULL);

	if (ChooseFont (&cf))
	{
		_r_config_setfont (L"Font", hwnd, &lf, NULL);

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
}
