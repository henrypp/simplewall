// simplewall
// Copyright (c) 2016-2020 Henry++

#pragma once

VOID _app_notifycreatewindow ();
BOOLEAN _app_notifycommand (_In_ HWND hwnd, _In_ INT button_id, _In_ LONG64 seconds);

BOOLEAN _app_notifyadd (_In_ HWND hwnd, _In_ PITEM_LOG ptr_log, _In_ PITEM_APP ptr_app);
VOID _app_freenotify (_Inout_ PITEM_APP ptr_app);

SIZE_T _app_notifyget_id (_In_ HWND hwnd, _In_ BOOLEAN is_nearest);
PITEM_LOG _app_notifyget_obj (_In_ SIZE_T app_hash);

BOOLEAN _app_notifyshow (_In_ HWND hwnd, _In_ PITEM_LOG ptr_log, _In_ BOOLEAN is_forced, _In_ BOOLEAN is_safety);
VOID _app_notifyhide (_In_ HWND hwnd);

VOID _app_notifyplaysound ();

VOID _app_notifyrefresh (_In_ HWND hwnd, _In_ BOOLEAN is_safety);

VOID _app_notifysetpos (_In_ HWND hwnd, _In_ BOOLEAN is_forced);

HFONT _app_notifyfontinit (_In_ HWND hwnd, _In_ PLOGFONT plf, _In_ LONG height, _In_ LONG weight, _In_ BOOLEAN is_underline);
VOID _app_notifyfontset (_In_ HWND hwnd);

VOID _app_notifydrawgradient (_In_ HDC hdc, _In_ LPRECT lprc, _In_ COLORREF rgb1, _In_ COLORREF rgb2, _In_ ULONG mode);

INT_PTR CALLBACK NotificationProc (_In_ HWND hwnd, _In_ UINT msg, _In_ WPARAM wparam, _In_ LPARAM lparam);
