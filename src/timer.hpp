// simplewall
// Copyright (c) 2016-2019 Henry++

#pragma once

void _app_timer_set (HWND hwnd, PITEM_APP ptr_app, time_t seconds);
void _app_timer_reset (HWND hwnd, PITEM_APP ptr_app);

bool _app_istimeractive (const PITEM_APP ptr_app);
bool _app_istimersactive ();

void CALLBACK _app_timer_callback (PVOID lparam, BOOLEAN);
