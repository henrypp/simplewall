// simplewall
// Copyright (c) 2016-2020 Henry++

#pragma once

VOID _app_notifycreatewindow ();
BOOLEAN _app_notifycommand (HWND hwnd, INT button_id, time_t seconds);

BOOLEAN _app_notifyadd (HWND hwnd, PITEM_LOG ptr_log, PITEM_APP ptr_app);
VOID _app_freenotify (SIZE_T app_hash, PITEM_APP ptr_app);

SIZE_T _app_notifyget_id (HWND hwnd, BOOLEAN is_nearest);
PITEM_LOG _app_notifyget_obj (SIZE_T app_hash);

BOOLEAN _app_notifyshow (HWND hwnd, PITEM_LOG ptr_log, BOOLEAN is_forced, BOOLEAN is_safety);
VOID _app_notifyhide (HWND hwnd);

VOID _app_notifyplaysound ();

VOID _app_notifyrefresh (HWND hwnd, BOOLEAN is_safety);

VOID _app_notifysetpos (HWND hwnd, BOOLEAN is_forced);

HFONT _app_notifyfontinit (HWND hwnd, PLOGFONT plf, LONG height, LONG weight, LPCWSTR name, BOOLEAN is_underline);
VOID _app_notifyfontset (HWND hwnd);

INT_PTR CALLBACK NotificationProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
