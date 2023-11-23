// simplewall
// Copyright (c) 2016-2023 Henry++

#pragma once

EXTERN_C const IID IID_IImageList2;

_Ret_maybenull_
PR_STRING _app_gettooltipbylparam (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ ULONG_PTR lparam
);

VOID _app_settab_id (
	_In_ HWND hwnd,
	_In_ INT page_id
);

LPWSTR _app_getstateaction (
	_In_ ENUM_INSTALL_TYPE install_type
);

HBITMAP _app_getstatebitmap (
	_In_ ENUM_INSTALL_TYPE install_type
);

INT _app_getstateicon (
	_In_ ENUM_INSTALL_TYPE install_type
);

LPCWSTR _app_getstatelocale (
	_In_ ENUM_INSTALL_TYPE install_type
);

BOOLEAN _app_initinterfacestate (
	_In_ HWND hwnd,
	_In_ BOOLEAN is_forced
);

VOID _app_restoreinterfacestate (
	_In_ HWND hwnd,
	_In_ BOOLEAN is_enabled
);

VOID _app_setinterfacestate (
	_In_ HWND hwnd,
	_In_ LONG dpi_value
);

VOID _app_settrayicon (
	_In_ HWND hwnd,
	_In_ ENUM_INSTALL_TYPE install_type
);

VOID _app_imagelist_init (
	_In_opt_ HWND hwnd,
	_In_ LONG dpi_value
);

HFONT _app_createfont (
	_Inout_ PLOGFONT logfont,
	_In_ LONG size,
	_In_ BOOLEAN is_underline,
	_In_ LONG dpi_value
);

VOID _app_windowloadfont (
	_In_ LONG dpi_value
);

VOID _app_toolbar_init (
	_In_ HWND hwnd,
	_In_ LONG dpi_value
);

VOID _app_toolbar_resize (
	_In_ HWND hwnd,
	_In_ LONG dpi_value
);

VOID _app_toolbar_setfont ();

VOID _app_window_resize (
	_In_ HWND hwnd,
	_In_ LPCRECT rect,
	_In_ LONG dpi_value
);

VOID _app_refreshstatus (
	_In_ HWND hwnd
);
