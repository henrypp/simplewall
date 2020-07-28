// simplewall
// Copyright (c) 2016-2020 Henry++

#pragma once

LONG_PTR _app_getappinfo (SIZE_T app_hash, ENUM_INFO_DATA info_key);
BOOLEAN _app_setappinfo (SIZE_T app_hash, ENUM_INFO_DATA info_key, LONG_PTR info_value);

SIZE_T _app_addapplication (HWND hwnd, LPCWSTR path, time_t timestamp, time_t timer, time_t last_notify, BOOLEAN is_silent, BOOLEAN is_enabled);

PITEM_APP _app_getappitem (SIZE_T app_hash);
PITEM_APP_HELPER _app_getapphelperitem (SIZE_T app_hash);
PITEM_RULE _app_getrulebyid (SIZE_T idx);
PITEM_RULE _app_getrulebyhash (SIZE_T rule_hash);
PITEM_NETWORK _app_getnetworkitem (SIZE_T network_hash);
SIZE_T _app_getnetworkapp (SIZE_T network_hash);
PITEM_LOG _app_getlogitem (SIZE_T idx);
SIZE_T _app_getlogapp (SIZE_T idx);
COLORREF _app_getappcolor (INT listview_id, SIZE_T app_hash, BOOLEAN is_validconnection);

VOID _app_freeapplication (SIZE_T app_hash);

VOID _app_getcount (PITEM_STATUS ptr_status);

INT _app_getappgroup (SIZE_T app_hash, const PITEM_APP ptr_app);
INT _app_getnetworkgroup (PITEM_NETWORK ptr_network);
INT _app_getrulegroup (PITEM_RULE ptr_rule);
INT _app_getruleicon (PITEM_RULE ptr_rule);
COLORREF _app_getrulecolor (INT listview_id, SIZE_T rule_idx);

PR_STRING _app_gettooltip (HWND hwnd, LPNMLVGETINFOTIP lpnmlv);

VOID _app_setappiteminfo (HWND hwnd, INT listview_id, INT item, SIZE_T app_hash, PITEM_APP ptr_app);
VOID _app_setruleiteminfo (HWND hwnd, INT listview_id, INT item, PITEM_RULE ptr_rule, BOOLEAN include_apps);

VOID _app_ruleenable (PITEM_RULE ptr_rule, BOOLEAN is_enable);
VOID _app_ruleenable2 (PITEM_RULE ptr_rule, BOOLEAN is_enable);

BOOLEAN _app_ruleblocklistsetchange (PITEM_RULE ptr_rule, INT new_state);
BOOLEAN _app_ruleblocklistsetstate (PITEM_RULE ptr_rule, INT spy_state, INT update_state, INT extra_state);
VOID _app_ruleblocklistset (HWND hwnd, INT spy_state, INT update_state, INT extra_state, BOOLEAN is_instantapply);

PR_STRING _app_appexpandrules (SIZE_T app_hash, LPCWSTR delimeter);
PR_STRING _app_rulesexpandapps (const PITEM_RULE ptr_rule, BOOLEAN is_fordisplay, LPCWSTR delimeter);
PR_STRING _app_rulesexpandrules (PR_STRING rule, LPCWSTR delimeter);

BOOLEAN _app_isappfound (SIZE_T app_hash);
BOOLEAN _app_isapphelperfound (SIZE_T app_hash);
BOOLEAN _app_isapphaveconnection (SIZE_T app_hash);
BOOLEAN _app_isapphavedrive (INT letter);
BOOLEAN _app_isapphaverule (SIZE_T app_hash);
BOOLEAN _app_isappused (const PITEM_APP ptr_app, SIZE_T app_hash);
BOOLEAN _app_isappexists (const PITEM_APP ptr_app);

BOOLEAN _app_isrulehost (LPCWSTR rule);
BOOLEAN _app_isruleip (LPCWSTR rule);
BOOLEAN _app_isruleport (LPCWSTR rule);
BOOLEAN _app_isrulevalidchars (LPCWSTR rule);

BOOLEAN _app_profile_load_check (LPCWSTR path, ENUM_TYPE_XML type, BOOLEAN is_strict);
VOID _app_profile_load_internal (LPCWSTR path, LPCWSTR path_backup, time_t* ptimestamp);
VOID _app_profile_load (HWND hwnd, LPCWSTR path_custom);
VOID _app_profile_save ();
