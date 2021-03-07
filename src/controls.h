// simplewall
// Copyright (c) 2016-2021 Henry++

#pragma once

EXTERN_C const IID IID_IImageList2;

VOID _app_settab_id (_In_ HWND hwnd, _In_ INT page_id);

UINT _app_getinterfacestatelocale (_In_ ENUM_INSTALL_TYPE install_type);
BOOLEAN _app_initinterfacestate (_In_ HWND hwnd, _In_ BOOLEAN is_forced);
VOID _app_restoreinterfacestate (_In_ HWND hwnd, _In_ BOOLEAN is_enabled);
VOID _app_setinterfacestate (_In_ HWND hwnd);

VOID _app_imagelist_init (_In_opt_ HWND hwnd);

VOID _app_listviewresize (_In_ HWND hwnd, _In_ INT listview_id, _In_ BOOLEAN is_forced);
VOID _app_listviewsetview (_In_ HWND hwnd, _In_ INT listview_id);

VOID _app_listviewsetfont (_In_ HWND hwnd, _In_ INT listview_id, _In_ BOOLEAN is_forced);

INT CALLBACK _app_listviewcompare_callback (_In_ LPARAM lparam1, _In_ LPARAM lparam2, _In_ LPARAM lparam);
VOID _app_listviewsort (_In_ HWND hwnd, _In_ INT listview_id, _In_ INT column_id, _In_ BOOLEAN is_notifycode);

VOID _app_toolbar_init (_In_ HWND hwnd);
VOID _app_toolbar_resize ();

VOID _app_refreshgroups (_In_ HWND hwnd, _In_ INT listview_id);
VOID _app_refreshstatus (_In_ HWND hwnd, _In_ INT listview_id);

VOID _app_showitem (_In_ HWND hwnd, _In_ INT listview_id, _In_ INT item, _In_ INT scroll_pos);
BOOLEAN _app_showappitem (_In_ HWND hwnd, _In_ PITEM_APP ptr_app);

FORCEINLINE INT _app_getposition (_In_ HWND hwnd, _In_ INT listview_id, _In_ LPARAM lparam)
{
	return _r_listview_finditem (hwnd, listview_id, -1, lparam);
}
