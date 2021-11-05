// simplewall
// Copyright (c) 2016-2021 Henry++

#pragma once

// profile types
typedef enum _ENUM_TYPE_XML
{
	XML_TYPE_PROFILE = 3,
	XML_TYPE_PROFILE_INTERNAL = 4,
} ENUM_TYPE_XML;

// profile versions
typedef enum _ENUM_VERSION_XML
{
	// v3.0.2: first major update, rule attribute "apps" now separated by "|"
	XML_VERSION_3 = 3,

	// v3.4: added "rules_custom" into internal profile and "os_version" for rule attributes
	XML_VERSION_4 = 4,

	XML_VERSION_5 = 5,
} ENUM_VERSION_XML;

#define XML_VERSION_CURRENT XML_VERSION_4

#define XML_PROFILE L"profile.xml"
#define XML_PROFILE_INTERNAL L"profile_internal.xml"

_Ret_maybenull_
PVOID _app_getappinfo (_In_ PITEM_APP ptr_app, _In_ ENUM_INFO_DATA info_data);

_Ret_maybenull_
PVOID _app_getappinfobyhash (_In_ ULONG_PTR app_hash, _In_ ENUM_INFO_DATA info_data);

VOID _app_setappinfo (_In_ PITEM_APP ptr_app, _In_ ENUM_INFO_DATA info_data, _In_ PVOID value);
VOID _app_setappinfobyhash (_In_ ULONG_PTR app_hash, _In_ ENUM_INFO_DATA info_data, _In_ PVOID value);

_Ret_maybenull_
PVOID _app_getruleinfo (_In_ PITEM_RULE ptr_rule, _In_ ENUM_INFO_DATA info_data);

_Ret_maybenull_
PVOID _app_getruleinfobyid (_In_ SIZE_T index, _In_ ENUM_INFO_DATA info_data);

ULONG_PTR _app_addapplication (_In_opt_ HWND hwnd, _In_ ENUM_TYPE_DATA type, _In_ PR_STRINGREF path, _In_opt_ PR_STRING display_name, _In_opt_ PR_STRING real_path);

PITEM_RULE _app_addrule (_In_opt_ PR_STRING name, _In_opt_ PR_STRING rule_remote, _In_opt_ PR_STRING rule_local, _In_ FWP_DIRECTION direction, _In_ UINT8 protocol, _In_ ADDRESS_FAMILY af);

_Ret_maybenull_
PITEM_RULE_CONFIG _app_addruleconfigtable (_In_ PR_HASHTABLE hashtable, _In_ ULONG_PTR rule_hash, _In_opt_ PR_STRING name, _In_ BOOLEAN is_enabled);

_Ret_maybenull_
PITEM_APP _app_getappitem (_In_ ULONG_PTR app_hash);

_Ret_maybenull_
PITEM_RULE _app_getrulebyid (_In_ SIZE_T index);

_Ret_maybenull_
PITEM_RULE _app_getrulebyhash (_In_ ULONG_PTR rule_hash);

_Ret_maybenull_
PITEM_RULE_CONFIG _app_getruleconfigitem (_In_ ULONG_PTR rule_hash);

ULONG_PTR _app_getnetworkapp (_In_ ULONG_PTR network_hash);

_Ret_maybenull_
PITEM_NETWORK _app_getnetworkitem (_In_ ULONG_PTR network_hash);

_Ret_maybenull_
PITEM_LOG _app_getlogitem (_In_ ULONG_PTR log_hash);

ULONG_PTR _app_getlogapp (_In_ SIZE_T index);
COLORREF _app_getappcolor (_In_ INT listview_id, _In_ ULONG_PTR app_hash, _In_ BOOLEAN is_systemapp, _In_ BOOLEAN is_validconnection);

VOID _app_freeapplication (_In_ ULONG_PTR app_hash);

BOOLEAN _app_isappfromsystem (_In_opt_ PR_STRING path, _In_ ULONG_PTR app_hash);
BOOLEAN _app_isapphaveconnection (_In_ ULONG_PTR app_hash);
BOOLEAN _app_isapphavedrive (_In_ INT letter);
BOOLEAN _app_isapphaverule (_In_ ULONG_PTR app_hash, _In_ BOOLEAN is_countdisabled);
BOOLEAN _app_isappexists (_In_ PITEM_APP ptr_app);

BOOLEAN _app_isappfound (_In_ ULONG_PTR app_hash);
BOOLEAN _app_isappunused (_In_ PITEM_APP ptr_app);

BOOLEAN _app_isnetworkfound (_In_ ULONG_PTR network_hash);

BOOLEAN _app_issystemhash (_In_ ULONG_PTR app_hash);

FORCEINLINE BOOLEAN _app_isappused (_In_ PITEM_APP ptr_app)
{
	if (ptr_app->is_enabled || ptr_app->is_silent || _app_isapphaverule (ptr_app->app_hash, TRUE))
		return TRUE;

	return FALSE;
}

VOID _app_getcount (_Out_ PITEM_STATUS status);

COLORREF _app_getrulecolor (_In_ INT listview_id, _In_ SIZE_T rule_idx);

VOID _app_setappiteminfo (_In_ HWND hwnd, _In_ INT listview_id, _In_ INT item_id, _In_ PITEM_APP ptr_app);
VOID _app_setruleiteminfo (_In_ HWND hwnd, _In_ INT listview_id, _In_ INT item_id, _In_ PITEM_RULE ptr_rule, _In_ BOOLEAN include_apps);

VOID _app_ruleenable (_Inout_ PITEM_RULE ptr_rule, _In_ BOOLEAN is_enable, _In_ BOOLEAN is_createconfig);

BOOLEAN _app_ruleblocklistsetchange (_Inout_ PITEM_RULE ptr_rule, _In_ LONG new_state);
BOOLEAN _app_ruleblocklistsetstate (_Inout_ PITEM_RULE ptr_rule, _In_ LONG spy_state, _In_ LONG update_state, _In_ LONG extra_state);
VOID _app_ruleblocklistset (_In_opt_ HWND hwnd, _In_ INT spy_state, _In_ INT update_state, _In_ INT extra_state, _In_ BOOLEAN is_instantapply);

_Ret_maybenull_
PR_STRING _app_appexpandrules (_In_ ULONG_PTR app_hash, _In_ LPCWSTR delimeter);

_Ret_maybenull_
PR_STRING _app_rulesexpandapps (_In_ PITEM_RULE ptr_rule, _In_ BOOLEAN is_fordisplay, _In_ LPCWSTR delimeter);

_Ret_maybenull_
PR_STRING _app_rulesexpandrules (_In_opt_ PR_STRING rule, _In_ LPCWSTR delimeter);

BOOLEAN _app_isprofilenodevalid (_Inout_ PR_XML_LIBRARY xml_library, _In_ ENUM_VERSION_XML min_version, _In_ ENUM_TYPE_XML type);
BOOLEAN _app_isprofilevalid (_In_opt_ HWND hwnd, _In_ PR_STRING path);

BOOLEAN _app_isrulesupportedbyos (_In_ PR_STRINGREF os_version);

VOID _app_profile_load_fallback ();
VOID _app_profile_load_helper (_Inout_ PR_XML_LIBRARY xml_library, _In_ ENUM_TYPE_DATA type, _In_ ENUM_VERSION_XML version);
VOID _app_profile_load_internal (_In_ PR_STRING path, _In_ LPCWSTR resource_name, _Inout_opt_ PLONG64 timestamp);
VOID _app_profile_load (_In_opt_ HWND hwnd, _In_opt_ LPCWSTR path_custom);
VOID _app_profile_save ();
