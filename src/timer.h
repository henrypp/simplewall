// simplewall
// Copyright (c) 2016-2024 Henry++

#pragma once

BOOLEAN _app_istimersactive ();

BOOLEAN _app_istimerset (
	_In_ PITEM_APP ptr_app
);

VOID _app_timer_set (
	_In_opt_ HWND hwnd,
	_Inout_ PITEM_APP ptr_app,
	_In_ LONG64 seconds
);

VOID _app_timer_reset (
	_In_opt_ HWND hwnd,
	_Inout_ PITEM_APP ptr_app
);

VOID _app_timer_remove (
	_Inout_ PITEM_APP ptr_app
);

VOID CALLBACK _app_timer_callback (
	_Inout_ PTP_CALLBACK_INSTANCE instance,
	_Inout_opt_ PVOID context,
	_Inout_ PTP_TIMER timer
);
