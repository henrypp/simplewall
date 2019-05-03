// simplewall
// Copyright (c) 2016-2019 Henry++

#pragma once

size_t _app_addapplication (HWND hwnd, rstring path, time_t timestamp, time_t timer, time_t last_notify, bool is_silent, bool is_enabled, bool is_fromdb);

PITEM_APP _app_getapplication (size_t hash);
PITEM_RULE _app_getrule (size_t hash, EnumDataType type, BOOL is_readonly);

bool _app_freeapplication (size_t hash);
void _app_freerule (PITEM_RULE* ptr);

void _app_getcount (PITEM_STATUS ptr_status);

size_t _app_getappgroup (size_t hash, PITEM_APP const ptr_app);
size_t _app_getrulegroup (PITEM_RULE const ptr_rule);
size_t _app_getruleicon (PITEM_RULE const ptr_rule);

rstring _app_gettooltip (UINT listview_id, size_t idx);

void _app_setappiteminfo (HWND hwnd, UINT listview_id, size_t item, size_t app_hash, PITEM_APP ptr_app);
void _app_setruleiteminfo (HWND hwnd, UINT listview_id, size_t item, PITEM_RULE ptr_rule, bool include_apps);

void _app_ruleenable (PITEM_RULE ptr_rule, bool is_enable);
rstring _app_rulesexpand (PITEM_RULE const ptr_rule, bool is_forservices, LPCWSTR delimeter);

bool _app_isapphaveconnection (size_t hash);
bool _app_isapphaverule (size_t hash);
bool _app_isappused (ITEM_APP const *ptr_app, size_t hash);
bool _app_isappexists (ITEM_APP const *ptr_app);

bool _app_isrulehost (LPCWSTR rule);
bool _app_isruleip (LPCWSTR rule);
bool _app_isruleport (LPCWSTR rule);

bool _app_isrulepresent (size_t hash);

void _app_profile_loadrules (MFILTER_RULES *ptr_rules, LPCWSTR path, LPCWSTR path_backup, bool is_internal, EnumDataType type, UINT8 weight, time_t *ptimestamp);

void _app_profile_load (HWND hwnd, LPCWSTR path_apps = nullptr, LPCWSTR path_rules = nullptr);
void _app_profile_save (LPCWSTR path_apps = nullptr, LPCWSTR path_rules = nullptr);
