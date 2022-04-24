// simplewall
// Copyright (c) 2016-2022 Henry++

#pragma once

_Ret_maybenull_
PVOID _app_getappinfo (
	_In_ PITEM_APP ptr_app,
	_In_ ENUM_INFO_DATA info_data
);

_Ret_maybenull_
PVOID _app_getappinfobyhash (
	_In_ ULONG_PTR app_hash,
	_In_ ENUM_INFO_DATA info_data
);

VOID _app_setappinfo (
	_In_ PITEM_APP ptr_app,
	_In_ ENUM_INFO_DATA info_data,
	_In_ PVOID value
);

VOID _app_setappinfobyhash (
	_In_ ULONG_PTR app_hash,
	_In_ ENUM_INFO_DATA info_data,
	_In_ PVOID value
);

_Ret_maybenull_
PVOID _app_getruleinfo (
	_In_ PITEM_RULE ptr_rule,
	_In_ ENUM_INFO_DATA info_data
);

_Ret_maybenull_
PVOID _app_getruleinfobyid (
	_In_ SIZE_T index,
	_In_ ENUM_INFO_DATA info_data
);

_Success_ (return != 0)
ULONG_PTR _app_addapplication (
	_In_opt_ HWND hwnd,
	_In_ ENUM_TYPE_DATA type,
	_In_ PR_STRING path,
	_In_opt_ PR_STRING display_name,
	_In_opt_ PR_STRING real_path
);

PITEM_RULE _app_addrule (
	_In_opt_ PR_STRING name,
	_In_opt_ PR_STRING rule_remote,
	_In_opt_ PR_STRING rule_local,
	_In_ FWP_DIRECTION direction,
	_In_ UINT8 protocol,
	_In_ ADDRESS_FAMILY af
);

_Ret_maybenull_
PITEM_RULE_CONFIG _app_addruleconfigtable (
	_In_ PR_HASHTABLE hashtable,
	_In_ ULONG_PTR rule_hash,
	_In_ PR_STRING rule_name,
	_In_ BOOLEAN is_enabled
);

_Ret_maybenull_
PITEM_APP _app_getappitem (
	_In_ ULONG_PTR app_hash
);

_Ret_maybenull_
PITEM_RULE _app_getrulebyid (
	_In_ SIZE_T index
);

_Ret_maybenull_
PITEM_RULE _app_getrulebyhash (
	_In_ ULONG_PTR rule_hash
);

_Ret_maybenull_
PITEM_RULE_CONFIG _app_getruleconfigitem (
	_In_ ULONG_PTR rule_hash
);

_Ret_maybenull_
PITEM_LOG _app_getlogitem (
	_In_ ULONG_PTR log_hash
);

ULONG_PTR _app_getlogapp (
	_In_ SIZE_T index
);

COLORREF _app_getappcolor (
	_In_ INT listview_id,
	_In_ ULONG_PTR app_hash,
	_In_ BOOLEAN is_systemapp,
	_In_ BOOLEAN is_validconnection
);

VOID _app_freeapplication (
	_In_ ULONG_PTR app_hash
);

BOOLEAN _app_isappfromsystem (
	_In_opt_ PR_STRING path,
	_In_ ULONG_PTR app_hash
);

BOOLEAN _app_isapphavedrive (
	_In_ INT letter
);

BOOLEAN _app_isapphaverule (
	_In_ ULONG_PTR app_hash,
	_In_ BOOLEAN is_countdisabled
);

BOOLEAN _app_isappexists (
	_In_ PITEM_APP ptr_app
);

BOOLEAN _app_isappfound (
	_In_ ULONG_PTR app_hash
);

BOOLEAN _app_isappunused (
	_In_ PITEM_APP ptr_app
);

BOOLEAN _app_isappused (
	_In_ PITEM_APP ptr_app
);

BOOLEAN _app_issystemhash (
	_In_ ULONG_PTR app_hash
);

VOID _app_getcount (
	_Out_ PITEM_STATUS status
);

COLORREF _app_getrulecolor (
	_In_ INT listview_id,
	_In_ SIZE_T rule_idx
);

VOID _app_setappiteminfo (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ INT item_id,
	_In_ PITEM_APP ptr_app
);

VOID _app_setruleiteminfo (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ INT item_id,
	_In_ PITEM_RULE ptr_rule,
	_In_ BOOLEAN include_apps
);

VOID _app_ruleenable (
	_Inout_ PITEM_RULE ptr_rule,
	_In_ BOOLEAN is_enable,
	_In_ BOOLEAN is_createconfig
);

//VOID _app_rulecleanapp (
//	_In_opt_ HWND hwnd,
//	_In_ SIZE_T item_id,
//	_In_ PITEM_RULE ptr_rule,
//	_In_opt_ ULONG_PTR app_hash
//);

VOID _app_ruleremoveapp (
	_In_opt_ HWND hwnd,
	_In_ SIZE_T item_id,
	_In_ PITEM_RULE ptr_rule,
	_In_ ULONG_PTR app_hash
);

BOOLEAN _app_ruleblocklistsetchange (
	_Inout_ PITEM_RULE ptr_rule,
	_In_ LONG new_state
);

BOOLEAN _app_ruleblocklistsetstate (
	_Inout_ PITEM_RULE ptr_rule,
	_In_ LONG spy_state,
	_In_ LONG update_state,
	_In_ LONG extra_state
);

VOID _app_ruleblocklistset (
	_In_opt_ HWND hwnd,
	_In_ LONG spy_state,
	_In_ LONG update_state,
	_In_ LONG extra_state,
	_In_ BOOLEAN is_instantapply
);

_Ret_maybenull_
PR_STRING _app_appexpandrules (
	_In_ ULONG_PTR app_hash,
	_In_ LPCWSTR delimeter
);

_Ret_maybenull_
PR_STRING _app_rulesexpandapps (
	_In_ PITEM_RULE ptr_rule,
	_In_ BOOLEAN is_fordisplay,
	_In_ LPCWSTR delimeter
);

_Ret_maybenull_
PR_STRING _app_rulesexpandrules (
	_In_opt_ PR_STRING rule,
	_In_ LPCWSTR delimeter
);

BOOLEAN _app_isrulesupportedbyos (
	_In_ PR_STRINGREF os_version
);

VOID _app_profile_initialize ();

PDB_INFORMATION _app_profile_load_fromresource (
	_In_ LPCWSTR resource_name
);

VOID _app_profile_load_fallback ();

VOID _app_profile_load_internal (
	_In_ PR_STRING path,
	_In_ LPCWSTR resource_name,
	_Out_opt_ PLONG64 timestamp
);

NTSTATUS _app_profile_load (
	_In_opt_ HWND hwnd,
	_In_opt_ PR_STRING path_custom
);

NTSTATUS _app_profile_save ();
