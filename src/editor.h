// simplewall
// Copyright (c) 2016-2021 Henry++

#pragma once

typedef struct _EDITOR_CONTEXT
{
	R_LAYOUT_MANAGER layout_manager;

	HWND hwnd;
	HICON hicon;

	union
	{
		PITEM_APP ptr_app;
		PITEM_RULE ptr_rule;
	};

	struct
	{
		SIZE_T current_length;
		INT listview_id;
		INT item_id;
	};

	INT page_id;

	BOOLEAN is_settorules;
} EDITOR_CONTEXT, *PEDITOR_CONTEXT;

_Ret_maybenull_
PEDITOR_CONTEXT _app_editor_createwindow (
	_In_ HWND hwnd,
	_In_ PVOID lparam,
	_In_ INT page_id,
	_In_ BOOLEAN is_settorules
);

VOID _app_editor_deletewindow (
	_In_ PEDITOR_CONTEXT context
);

_Ret_maybenull_
PEDITOR_CONTEXT _app_editor_getcontext (
	_In_ HWND hwnd
);

VOID _app_editor_setcontext (
	_In_ HWND hwnd,
	_In_ PEDITOR_CONTEXT context
);

VOID _app_editor_addtabitem (
	_In_ HWND hwnd,
	_In_ UINT locale_id,
	_In_ INT dlg_id,
	_In_ PEDITOR_CONTEXT context,
	_Inout_ PINT tabs_count
);

VOID _app_editor_settabtitle (
	_In_ HWND hwnd,
	_In_ INT listview_id
);

_Ret_maybenull_
PR_STRING _app_editor_getrulesfromlistview (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT exclude_id
);

VOID _app_editor_setrulestolistview (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ PR_STRING rule
);

INT_PTR CALLBACK EditorRuleProc (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
);

INT_PTR CALLBACK EditorPagesProc (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
);

INT_PTR CALLBACK EditorProc (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
);
