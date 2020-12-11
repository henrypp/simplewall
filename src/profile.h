// simplewall
// Copyright (c) 2016-2021 Henry++

#pragma once

PVOID _app_getappinfo (PITEM_APP ptr_app, ENUM_INFO_DATA info_data);
PVOID _app_getappinfobyhash (SIZE_T app_hash, ENUM_INFO_DATA info_data);
VOID _app_setappinfo (PITEM_APP ptr_app, ENUM_INFO_DATA info_data, PVOID value);
VOID _app_setappinfobyhash (SIZE_T app_hash, ENUM_INFO_DATA info_data, PVOID value);

PITEM_APP _app_addapplication (HWND hwnd, ENUM_TYPE_DATA type, LPCWSTR path, PR_STRING display_name, PR_STRING real_path);
PITEM_RULE _app_addrule (PR_STRING name, PR_STRING rule_remote, PR_STRING rule_local, FWP_DIRECTION direction, UINT8 protocol, ADDRESS_FAMILY af);

PITEM_RULE _app_getrulebyid (SIZE_T idx);
PITEM_RULE _app_getrulebyhash (SIZE_T rule_hash);
SIZE_T _app_getnetworkapp (SIZE_T network_hash);
PITEM_LOG _app_getlogitem (SIZE_T idx);
SIZE_T _app_getlogapp (SIZE_T idx);
COLORREF _app_getappcolor (INT listview_id, SIZE_T app_hash, BOOLEAN is_systemapp, BOOLEAN is_validconnection);

VOID _app_freeapplication (SIZE_T app_hash);

VOID _app_getcount (PITEM_STATUS ptr_status);

INT _app_getappgroup (PITEM_APP ptr_app);
INT _app_getnetworkgroup (PITEM_NETWORK ptr_network);
INT _app_getrulegroup (PITEM_RULE ptr_rule);
INT _app_getruleicon (PITEM_RULE ptr_rule);
COLORREF _app_getrulecolor (INT listview_id, SIZE_T rule_idx);

PR_STRING _app_gettooltip (HWND hwnd, INT listview_id, INT item_id);

VOID _app_setappiteminfo (HWND hwnd, INT listview_id, INT item, PITEM_APP ptr_app);
VOID _app_setruleiteminfo (HWND hwnd, INT listview_id, INT item, PITEM_RULE ptr_rule, BOOLEAN include_apps);

VOID _app_ruleenable (PITEM_RULE ptr_rule, BOOLEAN is_enable, BOOLEAN is_createconfig);

BOOLEAN _app_ruleblocklistsetchange (PITEM_RULE ptr_rule, INT new_state);
BOOLEAN _app_ruleblocklistsetstate (PITEM_RULE ptr_rule, INT spy_state, INT update_state, INT extra_state);
VOID _app_ruleblocklistset (HWND hwnd, INT spy_state, INT update_state, INT extra_state, BOOLEAN is_instantapply);

PR_STRING _app_appexpandrules (SIZE_T app_hash, LPCWSTR delimeter);
PR_STRING _app_rulesexpandapps (const PITEM_RULE ptr_rule, BOOLEAN is_fordisplay, LPCWSTR delimeter);
PR_STRING _app_rulesexpandrules (PR_STRING rule, LPCWSTR delimeter);

BOOLEAN _app_isappfromsystem (LPCWSTR path, SIZE_T app_hash);
BOOLEAN _app_isapphaveconnection (SIZE_T app_hash);
BOOLEAN _app_isapphavedrive (INT letter);
BOOLEAN _app_isapphaverule (SIZE_T app_hash, BOOLEAN is_countdisabled);
BOOLEAN _app_isappused (const PITEM_APP ptr_app);
BOOLEAN _app_isappexists (const PITEM_APP ptr_app);

VOID _app_openappdirectory (const PITEM_APP ptr_app);

#define TULE_TYPE_HOST (NET_STRING_NAMED_ADDRESS | NET_STRING_NAMED_SERVICE)
#define TULE_TYPE_IP (NET_STRING_IP_ADDRESS | NET_STRING_IP_SERVICE | NET_STRING_IP_NETWORK | NET_STRING_IP_ADDRESS_NO_SCOPE | NET_STRING_IP_SERVICE_NO_SCOPE)

BOOLEAN _app_isruletype (LPCWSTR rule, ULONG types);
BOOLEAN _app_isruleport (LPCWSTR rule, SIZE_T length);
BOOLEAN _app_isrulevalid (LPCWSTR rule, SIZE_T length);

BOOLEAN _app_profile_load_check (LPCWSTR path, ENUM_TYPE_XML type);
VOID _app_profile_load_fallback ();
VOID _app_profile_load_internal (LPCWSTR path, LPCWSTR resource_name, PLONG64 ptimestamp);
VOID _app_profile_load (HWND hwnd, LPCWSTR path_custom);
VOID _app_profile_save ();
