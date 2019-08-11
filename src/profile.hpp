// simplewall
// Copyright (c) 2016-2019 Henry++

#pragma once

bool _app_getappinfo (size_t app_hash, EnumInfo info_key, LPVOID presult, size_t size);
bool _app_setappinfo (size_t app_hash, EnumInfo info_key, LONG_PTR info_value);

size_t _app_addapplication (HWND hwnd, rstring path, time_t timestamp, time_t timer, time_t last_notify, bool is_silent, bool is_enabled, bool is_fromdb);

PR_OBJECT _app_getappitem (size_t app_hash);
PR_OBJECT _app_getrulebyid (size_t idx);
PR_OBJECT _app_getrulebyhash (size_t rule_hash);
PR_OBJECT _app_getnetworkitem (size_t network_hash);
size_t _app_getnetworkapp (size_t network_hash);

void _app_freeapplication (size_t hash);

void _app_getcount (PITEM_STATUS ptr_status);

INT _app_getappgroup (size_t app_hash, PITEM_APP ptr_app);
INT _app_getrulegroup (PITEM_RULE ptr_rule);
INT _app_getruleicon (PITEM_RULE ptr_rule);

rstring _app_gettooltip (INT listview_id, size_t lparam);

void _app_setappiteminfo (HWND hwnd, INT listview_id, INT item, size_t app_hash, PITEM_APP ptr_app);
void _app_setruleiteminfo (HWND hwnd, INT listview_id, INT item, PITEM_RULE ptr_rule, bool include_apps);

void _app_ruleenable (PITEM_RULE ptr_rule, bool is_enable);
void _app_ruleenable2 (PITEM_RULE ptr_rule, bool is_enable);

bool _app_ruleblocklistsetchange (PITEM_RULE ptr_rule, INT new_state);
bool _app_ruleblocklistsetstate (PITEM_RULE ptr_rule, INT spy_state, INT update_state, INT extra_state);
void _app_ruleblocklistset (HWND hwnd, INT spy_state, INT update_state, INT extra_state, bool is_instantapply);

rstring _app_rulesexpand (PITEM_RULE ptr_rule, bool is_fordisplay, LPCWSTR delimeter);

bool _app_isappfound (size_t app_hash);
bool _app_isapphaveconnection (size_t app_hash);
bool _app_isapphaverule (size_t app_hash);
bool _app_isappused (ITEM_APP *ptr_app, size_t app_hash);
bool _app_isappexists (ITEM_APP *ptr_app);

//bool _app_isruleblocklist (LPCWSTR name);
bool _app_isrulehost (LPCWSTR rule);
bool _app_isruleip (LPCWSTR rule);
bool _app_isruleport (LPCWSTR rule);

bool _app_profile_load_check (LPCWSTR path, EnumXmlType type, bool is_strict);
void _app_profile_load_internal (LPCWSTR path, LPCWSTR path_backup, time_t* ptimestamp);
void _app_profile_load (HWND hwnd, LPCWSTR path_custom = nullptr);
void _app_profile_save (LPCWSTR path_custom = nullptr);
