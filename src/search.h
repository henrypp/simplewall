// simplewall
// Copyright (c) 2012-2026 dmex
// Copyright (c) 2021-2026 Henry++

#pragma once

typedef struct _SEARCHCONTROL_BUTTON
{
	LPCWSTR settings_name;

	union
	{
		ULONG flags;

		struct
		{
			ULONG is_hot : 1;
			ULONG is_pushed : 1;
			ULONG is_active : 1;
			ULONG is_error : 1;
			ULONG spare_bits : 28;
		};
	};

	INT index;
} SEARCHCONTROL_BUTTON, *PSEARCHCONTROL_BUTTON;

typedef struct _SEARCH_CONTEXT
{
	RECT rect;
	HIMAGELIST himglist;
	HBITMAP old_bitmap;
	HBITMAP hbitmap;
	HBRUSH dc_brush;
	HWND hprev;
	HWND hwnd;
	HDC hdc;

	WNDPROC wnd_proc;

	SEARCHCONTROL_BUTTON search_button;
	SEARCHCONTROL_BUTTON regex_button;
	SEARCHCONTROL_BUTTON case_button;

	union
	{
		ULONG flags;

		struct
		{
			ULONG is_hot : 1;
			ULONG is_hottrack : 1;
			ULONG is_windowfocus : 1;
			ULONG is_usesearchpointer : 1;
			ULONG spare_bits : 28;
		};
	};

	LONG dpi_value;

	LONG image_width;
	LONG image_height;

	INT cx_width;
	INT cx_border;
} SEARCH_CONTEXT, *PSEARCH_CONTEXT;

#define SC_BUTTON_COUNT 1

#define SC_IMAGE_SEARCH 0
#define SC_IMAGE_REGEX 1
#define SC_IMAGE_CASE 1

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
	_Inout_ PSEARCH_CONTEXT context
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
	_In_ PSEARCH_CONTEXT context,
	_In_ PSEARCHCONTROL_BUTTON button,
	_In_ HDC hdc,
	_In_ LPCRECT wnd_rect
);

VOID _app_search_paintbuttons (
	_In_ PSEARCH_CONTEXT context,
	_In_ HDC hdc,
	_In_ LPCRECT wnd_rect
);

VOID _app_search_getbuttonrect (
	_In_ PSEARCH_CONTEXT context,
	_In_ PSEARCHCONTROL_BUTTON button,
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

VOID _app_search_restorefocus (
	_In_ PSEARCH_CONTEXT context
);

VOID _app_search_excludeclient (
	_In_ PSEARCH_CONTEXT context,
	_In_ HWND hwnd,
	_In_ HDC hdc,
	_In_ LPCRECT wnd_rect
);

COLORREF _app_search_getframecolor (
	_In_ PSEARCH_CONTEXT context,
	_In_ HWND hwnd
);

VOID _app_search_paintframe (
	_In_ PSEARCH_CONTEXT context,
	_In_ HWND hwnd,
	_In_ HDC hdc,
	_In_ LPCRECT wnd_rect
);

VOID _app_search_paintnonclient (
	_In_ PSEARCH_CONTEXT context,
	_In_ HWND hwnd,
	_In_ HDC hdc,
	_In_ LPCRECT wnd_rect,
	_In_ LPCRECT buf_rect
);

BOOLEAN _app_search_handlenonclientpaint (
	_In_ PSEARCH_CONTEXT context,
	_In_ HWND hwnd,
	_In_ WPARAM wparam
);

LRESULT CALLBACK _app_search_subclass_proc (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
);
