// simplewall
// Copyright (c) 2016-2020 Henry++

#pragma once

void _app_notifycreatewindow (HWND hwnd);
bool _app_notifycommand (HWND hwnd, INT button_id, time_t seconds);

bool _app_notifyadd (HWND hwnd, PR_OBJECT ptr_log_object, PITEM_APP ptr_app);
void _app_freenotify (size_t app_hash, PITEM_APP ptr_app, bool is_refresh = true);

size_t _app_notifyget_id (HWND hwnd, size_t current_id);
PR_OBJECT _app_notifyget_obj (size_t app_hash);

bool _app_notifyshow (HWND hwnd, PR_OBJECT ptr_log_object, bool is_forced, bool is_safety);
void _app_notifyhide (HWND hwnd);

void _app_notifyplaysound ();

void _app_notifyrefresh (HWND hwnd, bool is_safety);

void _app_notifysetpos (HWND hwnd, bool is_forced);

HFONT _app_notifyfontinit (HWND hwnd, PLOGFONT plf, LONG height, LONG weight, LPCWSTR name, BYTE is_underline);
void _app_notifyfontset (HWND hwnd);

INT_PTR CALLBACK NotificationProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
