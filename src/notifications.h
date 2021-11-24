// simplewall
// Copyright (c) 2016-2021 Henry++

#pragma once

typedef struct _NOTIFY_CONTEXT
{
	HBITMAP hbmp_allow;
	HBITMAP hbmp_block;
	HBITMAP hbmp_cross;
	HBITMAP hbmp_rules;

	HFONT hfont_title;
	HFONT hfont_link;
	HFONT hfont_text;

	HICON hicon;

	ULONG_PTR app_hash;
} NOTIFY_CONTEXT, *PNOTIFY_CONTEXT;

VOID _app_notify_createwindow ();
VOID _app_notify_destroywindow ();

_Ret_maybenull_
HWND _app_notify_getwindow ();

_Ret_maybenull_
PNOTIFY_CONTEXT _app_notify_getcontext (_In_ HWND hwnd);

VOID _app_notify_setcontext (_In_ HWND hwnd, _In_opt_ PNOTIFY_CONTEXT context);

BOOLEAN _app_notify_command (_In_ HWND hwnd, _In_ INT button_id, _In_ LONG64 seconds);

BOOLEAN _app_notify_addobject (_In_ PITEM_LOG ptr_log, _In_ PITEM_APP ptr_app);
VOID _app_notify_freeobject (_Inout_ PITEM_APP ptr_app);

_Ret_maybenull_
HICON _app_notify_getapp_icon (_In_ HWND hwnd);

_Ret_maybenull_
PITEM_LOG _app_notify_getobject (_In_ ULONG_PTR app_hash);

ULONG_PTR _app_notify_getapp_id (_In_ HWND hwnd);
ULONG_PTR _app_notify_getnextapp_id (_In_ HWND hwnd);

VOID _app_notify_setapp_icon (_In_ HWND hwnd, _In_opt_ HICON hicon, _In_ BOOLEAN is_redraw);
VOID _app_notify_setapp_id (_In_ HWND hwnd, _In_opt_ ULONG_PTR app_hash);

VOID _app_notify_show (_In_ HWND hwnd, _In_ PITEM_LOG ptr_log, _In_ BOOLEAN is_forced, _In_ BOOLEAN is_safety);
VOID _app_notify_hide (_In_ HWND hwnd);

VOID _app_notify_playsound ();

VOID _app_notify_refresh (_In_ HWND hwnd, _In_ BOOLEAN is_safety);

VOID _app_notify_setposition (_In_ HWND hwnd, _In_ BOOLEAN is_forced);

VOID _app_notify_initializefont (_In_ HWND hwnd, _Inout_ PNOTIFY_CONTEXT context, _In_ LONG dpi_value);
VOID _app_notify_initialize (_In_ HWND hwnd, _In_ LONG dpi_value);
VOID _app_notify_destroy (_In_ HWND hwnd);

VOID _app_notify_drawgradient (_In_ HDC hdc, _In_ LPCRECT rect);

INT_PTR CALLBACK NotificationProc (_In_ HWND hwnd, _In_ UINT msg, _In_ WPARAM wparam, _In_ LPARAM lparam);
