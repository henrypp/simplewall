// simplewall
// Copyright (c) 2021 Henry++

#pragma once

VOID _app_message_contextmenu (_In_ HWND hwnd, _In_ LPNMITEMACTIVATE lpnmlv);
VOID _app_message_contextmenu_columns (_In_ HWND hwnd, _In_ LPNMHDR nmlp);

VOID _app_message_traycontextmenu (_In_ HWND hwnd);

LONG_PTR _app_message_custdraw (_In_ HWND hwnd, _In_ LPNMLVCUSTOMDRAW lpnmlv);
VOID _app_message_dpichanged (_In_ HWND hwnd, _In_ LONG dpi_value);
BOOLEAN _app_message_displayinfo (_In_ HWND hwnd, _In_ INT listview_id, _Inout_ LPNMLVDISPINFOW lpnmlv);

VOID _app_message_initialize (_In_ HWND hwnd);
VOID _app_message_localize (_In_ HWND hwnd);

FORCEINLINE VOID _app_message_uninitialize (_In_ HWND hwnd)
{
	_r_tray_destroy (hwnd, &GUID_TrayIcon);
}

VOID _app_command_idtorules (_In_ HWND hwnd, _In_ INT ctrl_id);
VOID _app_command_idtotimers (_In_ HWND hwnd, _In_ INT ctrl_id);

VOID _app_command_logshow (_In_ HWND hwnd);
VOID _app_command_logclear (_In_ HWND hwnd);

VOID _app_command_logerrshow (_In_opt_ HWND hwnd);
VOID _app_command_logerrclear (_In_opt_ HWND hwnd);

VOID _app_command_copy (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT column_id);
VOID _app_command_checkbox (_In_ HWND hwnd, _In_ INT ctrl_id);
VOID _app_command_delete (_In_ HWND hwnd);
VOID _app_command_disable (_In_ HWND hwnd, _In_ INT ctrl_id);
VOID _app_command_openeditor (_In_ HWND hwnd);
VOID _app_command_properties (_In_ HWND hwnd);
VOID _app_command_purgeunused (_In_ HWND hwnd);
VOID _app_command_purgetimers (_In_ HWND hwnd);
VOID _app_command_selectfont (_In_ HWND hwnd);
