// simplewall
// Copyright (c) 2016-2019 Henry++

#pragma once

void _app_notifycreatewindow ();
bool _app_notifycommand (HWND hwnd, UINT ctrl_id, size_t timer_idx);

bool _app_notifyadd (HWND hwnd, PITEM_LOG const ptr_log, PITEM_APP const ptr_app);
void _app_freenotify (size_t idx_orhash, bool is_idx);
size_t _app_notifygetcurrent (HWND hwnd);

bool _app_notifyshow (HWND hwnd, size_t idx, bool is_forced, bool is_safety);
void _app_notifyhide (HWND hwnd);

void _app_notifyplaysound ();

bool _app_notifyrefresh (HWND hwnd, bool is_safety);

void _app_notifysetpos (HWND hwnd);
bool _app_notifysettimeout (HWND hwnd, UINT_PTR timer_id, bool is_create, UINT timeout);

void _app_notifysetnote (HWND hwnd, UINT ctrl_id, rstring str);
void _app_notifysettext (HDC hdc, HWND hwnd, UINT ctrl_id1, LPCWSTR text1, UINT ctrl_id2, LPCWSTR text2);

HFONT _app_notifyinitfont (PLOGFONT plf, LONG height, LONG weight, LPCWSTR name, BOOL is_underline);

void DrawFrameBorder (HDC hdc, HWND hwnd, COLORREF clr);
LRESULT CALLBACK NotificationProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
