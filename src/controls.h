// simplewall
// Copyright (c) 2016-2021 Henry++

#pragma once

EXTERN_C const IID IID_IImageList2;

INT _app_getcurrentlistview_id (_In_ HWND hwnd);
INT _app_getlistviewbytab_id (_In_ HWND hwnd, _In_ INT tab_id);
INT _app_getlistviewbytype_id (_In_ ENUM_TYPE_DATA type);

#define PR_SETITEM_REDRAW  0x0001
#define PR_SETITEM_UPDATE 0x0002

VOID _app_setlistviewbylparam (_In_ HWND hwnd, _In_ LPARAM lparam, _In_ ULONG flags, _In_ BOOLEAN is_app);

#define PR_UPDATE_TYPE 0x0001
#define PR_UPDATE_FORCE 0x0002
#define PR_UPDATE_NOREFRESH 0x0004
#define PR_UPDATE_NOSORT 0x0008
#define PR_UPDATE_NORESIZE 0x0010
#define PR_UPDATE_NOREDRAW 0x0020

VOID _app_updatelistviewbylparam (_In_ HWND hwnd, _In_ LPARAM lparam, _In_ ULONG flags);

VOID _app_addlistviewapp (_In_ HWND hwnd, _In_ PITEM_APP ptr_app);
VOID _app_addlistviewrule (_In_ HWND hwnd, _In_ PITEM_RULE ptr_rule, _In_ SIZE_T rule_idx, _In_ BOOLEAN is_forapp);

_Ret_maybenull_
PR_STRING _app_gettooltipbylparam (_In_ HWND hwnd, _In_ INT listview_id, _In_ LPARAM lparam);

VOID _app_settab_id (_In_ HWND hwnd, _In_ INT page_id);

UINT _app_getinterfacestatelocale (_In_ ENUM_INSTALL_TYPE install_type);
BOOLEAN _app_initinterfacestate (_In_ HWND hwnd, _In_ BOOLEAN is_forced);
VOID _app_restoreinterfacestate (_In_ HWND hwnd, _In_ BOOLEAN is_enabled);
VOID _app_setinterfacestate (_In_ HWND hwnd);

VOID _app_imagelist_init (_In_ HWND hwnd);

VOID _app_listviewresize (_In_ HWND hwnd, _In_ INT listview_id, _In_ BOOLEAN is_forced);
VOID _app_listviewsetview (_In_ HWND hwnd, _In_ INT listview_id);

VOID _app_listviewloadfont (_In_ HWND hwnd, _In_ BOOLEAN is_forced);
VOID _app_listviewsetfont (_In_ HWND hwnd, _In_ INT listview_id);

INT CALLBACK _app_listviewcompare_callback (_In_ LPARAM lparam1, _In_ LPARAM lparam2, _In_ LPARAM lparam);
VOID _app_listviewsort (_In_ HWND hwnd, _In_ INT listview_id, _In_ INT column_id, _In_ BOOLEAN is_notifycode);

VOID _app_toolbar_init (_In_ HWND hwnd);
VOID _app_toolbar_resize ();

VOID _app_refreshgroups (_In_ HWND hwnd, _In_ INT listview_id);
VOID _app_refreshstatus (_In_ HWND hwnd);

VOID _app_showitem (_In_ HWND hwnd, _In_ INT listview_id, _In_ INT item_id, _In_ INT scroll_pos);
VOID _app_showitembylparam (_In_ HWND hwnd, _In_ LPARAM lparam, _In_ BOOLEAN is_app);

VOID _app_updateitembyidx (_In_ HWND hwnd, _In_ INT listview_id, _In_ INT item_id);
VOID _app_updateitembylparam (_In_ HWND hwnd, _In_ LPARAM lparam, _In_ BOOLEAN is_app);

_Success_ (return != -1)
FORCEINLINE INT _app_getposition (_In_ HWND hwnd, _In_ INT listview_id, _In_ LPARAM lparam)
{
	return _r_listview_finditem (hwnd, listview_id, -1, lparam);
}
