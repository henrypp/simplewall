// simplewall
// Copyright (c) 2016-2021 Henry++

#pragma once

VOID _app_notifycreatewindow ();
VOID _app_notifydestroywindow ();

_Ret_maybenull_
HWND _app_notifygetwindow ();

BOOLEAN _app_notifycommand (_In_ HWND hwnd, _In_ INT button_id, _In_ LONG64 seconds);

BOOLEAN _app_notifyadd (_In_ PITEM_LOG ptr_log, _In_ PITEM_APP ptr_app);
VOID _app_freenotify (_Inout_ PITEM_APP ptr_app);

ULONG_PTR _app_notifyget_id (_In_ HWND hwnd, _In_ BOOLEAN is_nearest);
PITEM_LOG _app_notifyget_obj (_In_ ULONG_PTR app_hash);

BOOLEAN _app_notifyshow (_In_ HWND hwnd, _In_ PITEM_LOG ptr_log, _In_ BOOLEAN is_forced, _In_ BOOLEAN is_safety);
VOID _app_notifyhide (_In_ HWND hwnd);

VOID _app_notifyplaysound ();

VOID _app_notifyrefresh (_In_ HWND hwnd, _In_ BOOLEAN is_safety);

VOID _app_notifyseticon (_In_ HWND hwnd, _In_opt_ HICON hicon, _In_ BOOLEAN is_redraw);
VOID _app_notifysetpos (_In_ HWND hwnd, _In_ BOOLEAN is_forced);

HFONT _app_notifyfontinit (_Inout_ PLOGFONT logfont, _In_ LONG dpi_value, _In_ LONG size, _In_ BOOLEAN is_underline);
VOID _app_notifyfontset (_In_ HWND hwnd);

VOID _app_notifydrawgradient (_In_ HDC hdc, _In_ LPCRECT rect);

INT_PTR CALLBACK NotificationProc (_In_ HWND hwnd, _In_ UINT msg, _In_ WPARAM wparam, _In_ LPARAM lparam);
