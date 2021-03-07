// simplewall
// Copyright (c) 2016-2021 Henry++

#pragma once

VOID _app_timer_set (_In_opt_ HWND hwnd, _Inout_ PITEM_APP ptr_app, _In_ LONG64 seconds);
VOID _app_timer_reset (_In_opt_ HWND hwnd, _Inout_ PITEM_APP ptr_app);
VOID _app_timer_remove (_Inout_ PTP_TIMER* timer);

FORCEINLINE BOOLEAN _app_istimerset (_In_ PTP_TIMER timer)
{
	return timer && IsThreadpoolTimerSet (timer);
}

BOOLEAN _app_istimersactive ();

VOID CALLBACK _app_timer_callback (_Inout_ PTP_CALLBACK_INSTANCE instance, _Inout_opt_ PVOID context, _Inout_ PTP_TIMER timer);
