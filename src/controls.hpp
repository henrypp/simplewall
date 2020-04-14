// simplewall
// Copyright (c) 2016-2020 Henry++

#pragma once

EXTERN_C const IID IID_IImageList2;

void _app_settab_id (HWND hwnd, INT page_id);

bool _app_initinterfacestate (HWND hwnd, bool is_forced);
void _app_restoreinterfacestate (HWND hwnd, bool is_enabled);
void _app_setinterfacestate (HWND hwnd);

void _app_listviewresize (HWND hwnd, INT listview_id, bool is_forced);
void _app_listviewsetview (HWND hwnd, INT listview_id);

bool _app_listviewinitfont (HWND hwnd, PLOGFONT plf);
void _app_listviewsetfont (HWND hwnd, INT listview_id, bool is_redraw);

INT CALLBACK _app_listviewcompare_callback (LPARAM lparam1, LPARAM lparam2, LPARAM lparam);
void _app_listviewsort (HWND hwnd, INT listview_id, INT column_id = INVALID_INT, bool is_notifycode = false);

INT _app_getposition (HWND hwnd, INT listview_id, LPARAM lparam);
void _app_showitem (HWND hwnd, INT listview_id, INT item, INT scroll_pos = INVALID_INT);

LONG _app_nmcustdraw_listview (LPNMLVCUSTOMDRAW lpnmlv);
LONG _app_nmcustdraw_toolbar (LPNMLVCUSTOMDRAW lpnmlv);
