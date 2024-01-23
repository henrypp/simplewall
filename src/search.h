// simplewall
// Copyright (c) 2012-2021 dmex
// Copyright (c) 2021-2024 Henry++

#pragma once

typedef struct _SEARCH_CONTEXT
{
	HWND hwnd;
	HICON hicon;

	WNDPROC def_window_proc;

	union
	{
		ULONG flags;
		struct
		{
			ULONG is_hot : 1;
			ULONG is_pushed : 1;
			ULONG spare_bits : 30;
		};
	};

	LONG image_width;
	LONG image_height;

	INT cx_width;
	INT cx_border;
} SEARCH_CONTEXT, *PSEARCH_CONTEXT;

VOID _app_search_initializetheme (
	_Inout_ PSEARCH_CONTEXT context
);

VOID _app_search_destroytheme (
	_Inout_ PSEARCH_CONTEXT context
);

VOID _app_search_initialize (
	_In_ HWND hwnd
);

VOID _app_search_setvisible (
	_In_ HWND hwnd,
	_In_ HWND hsearch
);

VOID _app_search_drawbutton (
	_Inout_ PSEARCH_CONTEXT context,
	_In_ LPCRECT button_rect
);

VOID _app_search_getbuttonrect (
	_In_ PSEARCH_CONTEXT context,
	_Inout_ PRECT rect
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
