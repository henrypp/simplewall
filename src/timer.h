// simplewall
// Copyright (c) 2016-2020 Henry++

#pragma once

VOID _app_timer_set (HWND hwnd, PITEM_APP ptr_app, LONG64 seconds);
VOID _app_timer_reset (HWND hwnd, PITEM_APP ptr_app);
VOID _app_timer_remove (PTP_TIMER* ptptimer);

BOOLEAN _app_istimerset (PTP_TIMER tptimer);
BOOLEAN _app_istimersactive ();

VOID CALLBACK _app_timer_callback (PTP_CALLBACK_INSTANCE instance, PVOID context, PTP_TIMER timer);
