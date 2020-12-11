// simplewall
// Copyright (c) 2016-2021 Henry++

#pragma once

VOID _app_message_contextmenu (HWND hwnd, LPNMITEMACTIVATE lpnmlv);
VOID _app_message_traycontextmenu (HWND hwnd);

LONG_PTR _app_message_custdraw (LPNMLVCUSTOMDRAW lpnmlv);
VOID _app_message_dpichanged (HWND hwnd);
VOID _app_message_find (HWND hwnd, LPFINDREPLACE lpfr);
VOID _app_message_resizewindow (HWND hwnd, LPARAM lparam);

VOID _app_message_initialize (HWND hwnd);
VOID _app_message_localize (HWND hwnd);

FORCEINLINE VOID _app_message_uninitialize (HWND hwnd)
{
	_r_tray_destroy (hwnd, UID);
}

VOID _app_command_idtotimers (HWND hwnd, INT ctrl_id);

VOID _app_command_logshow (HWND hwnd);
VOID _app_command_logclear (HWND hwnd);

VOID _app_command_logerrshow (HWND hwnd);
VOID _app_command_logerrclear (HWND hwnd);

VOID _app_command_copy (HWND hwnd, INT ctrl_id, INT column_id);
VOID _app_command_checkbox (HWND hwnd, INT ctrl_id);
VOID _app_command_delete (HWND hwnd);
VOID _app_command_disable (HWND hwnd, INT ctrl_id);
VOID _app_command_openeditor (HWND hwnd);
VOID _app_command_properties (HWND hwnd);
VOID _app_command_purgeunused (HWND hwnd);
VOID _app_command_purgetimers (HWND hwnd);
VOID _app_command_selectfont (HWND hwnd);
