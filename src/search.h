// simplewall
// Copyright (c) 2012-2021 dmex
// Copyright (c) 2021-2025 Henry++

#pragma once

typedef struct _SEARCH_CONTEXT
{
	RECT rect;
	HICON hicon_light;
	HICON hicon_dark;
	HBITMAP old_bitmap;
	HBITMAP hbitmap;
	HBRUSH dc_brush;
	HWND hwnd;
	HDC hdc;

	WNDPROC wnd_proc;

	union
	{
		ULONG flags;

		struct
		{
			ULONG is_hot : 1;
			ULONG is_pushed : 1;
			ULONG is_mouseactive : 1;
			ULONG spare_bits : 29;
		};
	};

	LONG dpi_value;

	LONG image_width;
	LONG image_height;

	INT cx_width;
	INT cx_border;
} SEARCH_CONTEXT, *PSEARCH_CONTEXT;

VOID _app_search_initialize (
	_Inout_ PSEARCH_CONTEXT context
);

VOID _app_search_create (
	_In_ HWND hwnd
);

VOID _app_search_initializeimages (
	_In_ PSEARCH_CONTEXT context,
	_In_ HWND hwnd
);

VOID _app_search_themechanged (
	_In_ HWND hwnd,
	_In_ PSEARCH_CONTEXT context
);

VOID _app_search_setvisible (
	_In_ HWND hwnd,
	_In_ HWND hsearch,
	_In_ LONG dpi_value
);

VOID _app_search_drawwindow (
	_Inout_ PSEARCH_CONTEXT context,
	_In_ LPCRECT wnd_rect
);

VOID _app_search_drawbutton (
	_Inout_ PSEARCH_CONTEXT context,
	_In_ HWND hwnd,
	_In_ LPCRECT wnd_rect
);

VOID _app_search_getbuttonrect (
	_In_ PSEARCH_CONTEXT context,
	_In_ LPCRECT wnd_rect,
	_Out_ PRECT btn_rect
);

BOOLEAN _app_search_applyfiltercallback (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_opt_ PR_STRING search_string
);

BOOLEAN _app_search_applyfilteritem (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ INT item_id,
	_Inout_ PITEM_LISTVIEW_CONTEXT context,
	_In_opt_ PR_STRING search_string
);

VOID _app_search_applyfilter (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_opt_ PR_STRING search_string
);

LRESULT CALLBACK _app_search_subclass_proc (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
);
