// simplewall
// Copyright (c) 2016-2019 Henry++

#pragma once

void _app_timer_create (HWND hwnd, MFILTER_APPS* ptr_apps, time_t seconds);
size_t _app_timer_remove (HWND hwnd, MFILTER_APPS* ptr_apps);

bool _app_istimeractive (ITEM_APP const *ptr_app);
bool _app_istimersactive ();

void CALLBACK _app_timer_callback (PVOID lparam, BOOLEAN);
