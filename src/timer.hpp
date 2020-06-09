// simplewall
// Copyright (c) 2016-2020 Henry++

#pragma once

VOID _app_timer_set (HWND hwnd, PITEM_APP ptr_app, time_t seconds);
VOID _app_timer_reset (HWND hwnd, PITEM_APP ptr_app);

BOOLEAN _app_istimeractive (const PITEM_APP ptr_app);
BOOLEAN _app_istimersactive ();

VOID CALLBACK _app_timer_callback (PVOID lpParameter, BOOLEAN TimerOrWaitFired);
