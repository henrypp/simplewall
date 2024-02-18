// simplewall
// Copyright (c) 2016-2024 Henry++

#pragma once

typedef struct _ITEM_LISTVIEW_CONTEXT
{
	ULONG_PTR id_code;

	struct
	{
		ULONG is_hidden : 1;
		ULONG spare_bits : 31;
	} DUMMYSTRUCTNAME;
} ITEM_LISTVIEW_CONTEXT, *PITEM_LISTVIEW_CONTEXT;

#define PR_SETITEM_REDRAW 0x0001
#define PR_SETITEM_UPDATE 0x0002

#define PR_UPDATE_TYPE 0x0001
#define PR_UPDATE_FORCE 0x0002
#define PR_UPDATE_NOREFRESH 0x0004
#define PR_UPDATE_NOSORT 0x0008
#define PR_UPDATE_NORESIZE 0x0010
#define PR_UPDATE_NOREDRAW 0x0020
#define PR_UPDATE_NOSETVIEW 0x0040

INT _app_listview_getcurrent (
	_In_ HWND hwnd
);

INT _app_listview_getbytab (
	_In_ HWND hwnd,
	_In_ INT tab_id
);

_Success_ (return != 0)
INT _app_listview_getbytype (
	_In_ ENUM_TYPE_DATA type
);

VOID _app_listview_additems (
	_In_ HWND hwnd
);

VOID _app_listview_clearitems (
	_In_ HWND hwnd
);

VOID _app_listview_addappitem (
	_In_ HWND hwnd,
	_In_ PITEM_APP ptr_app
);

VOID _app_listview_addruleitem (
	_In_ HWND hwnd,
	_In_ PITEM_RULE ptr_rule,
	_In_ ULONG_PTR rule_idx,
	_In_ BOOLEAN is_forapp
);

VOID _app_listview_addnetworkitem (
	_In_ HWND hwnd,
	_In_ ULONG_PTR network_hash
);

VOID _app_listview_addlogitem (
	_In_ HWND hwnd,
	_In_ ULONG_PTR log_hash
);

BOOLEAN _app_listview_islocked (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
);

VOID _app_listview_lock (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ BOOLEAN is_lock
);

LPARAM _app_listview_createcontext (
	_In_ ULONG_PTR id_code
);

VOID _app_listview_destroycontext (
	_In_ PITEM_LISTVIEW_CONTEXT context
);

ULONG_PTR _app_listview_getcontextcode (
	_In_ LPARAM lparam
);

ULONG_PTR _app_listview_getappcontext (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ INT item_id
);

ULONG_PTR _app_listview_getitemcontext (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ INT item_id
);

BOOLEAN _app_listview_isitemhidden (
	_In_ LPARAM lparam
);

_Success_ (return != -1)
INT _app_listview_finditem (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ ULONG_PTR id_code
);

VOID _app_listview_removeitem (
	_In_ HWND hwnd,
	_In_ ULONG_PTR id_code,
	_In_ ENUM_TYPE_DATA type
);

VOID _app_listview_showitemby_id (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ INT item_id,
	_In_ INT scroll_pos
);

VOID _app_listview_showitemby_param (
	_In_ HWND hwnd,
	_In_ ULONG_PTR lparam,
	_In_ BOOLEAN is_app
);

VOID _app_listview_updateby_id (
	_In_ HWND hwnd,
	_In_ INT lparam,
	_In_ ULONG flags
);

VOID _app_listview_updateby_param (
	_In_ HWND hwnd,
	_In_ ULONG_PTR lparam,
	_In_ ULONG flags,
	_In_ BOOLEAN is_app
);

VOID _app_listview_updateitemby_param (
	_In_ HWND hwnd,
	_In_ ULONG_PTR lparam,
	_In_ BOOLEAN is_app
);

VOID _app_listview_updateitemby_id (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ INT item_id
);

VOID _app_listview_loadfont (
	_In_ LONG dpi_value,
	_In_ BOOLEAN is_forced
);

VOID _app_listview_refreshgroups (
	_In_ HWND hwnd,
	_In_ INT listview_id
);

VOID _app_listview_resize (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ BOOLEAN is_forced
);

VOID _app_listview_setfont (
	_In_ HWND hwnd,
	_In_ INT listview_id
);

VOID _app_listview_setview (
	_In_ HWND hwnd,
	_In_ INT listview_id
);

INT CALLBACK _app_listview_compare_callback (
	_In_ LPARAM lparam1,
	_In_ LPARAM lparam2,
	_In_ LPARAM lparam
);

VOID _app_listview_sort (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ LONG column_id,
	_In_ BOOLEAN is_notifycode
);
