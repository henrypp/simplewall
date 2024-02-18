// simplewall
// Copyright (c) 2016-2024 Henry++

#pragma once

typedef struct _ICON_INFORMATION
{
	HICON app_hicon;
	HICON service_hicon;
	HICON uwp_hicon;

	LONG app_icon_id;
	LONG service_icon_id;
	LONG uwp_icon_id;
} ICON_INFORMATION, *PICON_INFORMATION;

#define FMTADDR_AS_RULE 0x0001
#define FMTADDR_USE_PROTOCOL 0x0002

VOID NTAPI _app_dereferenceapp (
	_In_ PVOID entry
);

VOID NTAPI _app_dereferenceappinfo (
	_In_ PVOID entry
);

VOID NTAPI _app_dereferenceruleconfig (
	_In_ PVOID entry
);

VOID NTAPI _app_dereferencenetwork (
	_In_ PVOID entry
);

VOID NTAPI _app_dereferencelog (
	_In_ PVOID entry
);
VOID NTAPI _app_dereferencerule (
	_In_ PVOID entry
);

VOID _app_addcachetable (
	_Inout_ PR_HASHTABLE hashtable,
	_In_ ULONG_PTR hash_code,
	_In_ PR_QUEUED_LOCK spin_lock,
	_In_opt_ PR_STRING string
);

BOOLEAN _app_getcachetable (
	_Inout_ PR_HASHTABLE cache_table,
	_In_ ULONG_PTR hash_code,
	_In_ PR_QUEUED_LOCK spin_lock,
	_Out_ PR_STRING_PTR string
);

PR_STRING _app_formatarpa (
	_In_ ADDRESS_FAMILY af,
	_In_ LPCVOID address
);

_Ret_maybenull_
PR_STRING _app_formataddress (
	_In_ ADDRESS_FAMILY af,
	_In_ UINT8 proto,
	_In_ LPCVOID address,
	_In_opt_ UINT16 port,
	_In_ ULONG flags
);

PR_STRING _app_formataddress_interlocked (
	_In_ PVOID volatile *string,
	_In_ ADDRESS_FAMILY af,
	_In_ LPCVOID address
);

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_formatip (
	_In_ ADDRESS_FAMILY af,
	_In_ LPCVOID address,
	_Out_writes_to_ (buffer_length, buffer_length) LPWSTR buffer,
	_In_ ULONG buffer_length,
	_In_ BOOLEAN is_checkempty
);

PR_STRING _app_formatport (
	_In_ UINT16 port,
	_In_ UINT8 proto
);

_Ret_maybenull_
PITEM_APP_INFO _app_getappinfobyhash2 (
	_In_ ULONG_PTR app_hash
);

_Success_ (return)
BOOLEAN _app_getappinfoparam2 (
	_In_ ULONG_PTR app_hash,
	_In_opt_ INT listview_id,
	_In_ ENUM_INFO_DATA2 info_data,
	_Out_writes_bytes_all_ (size) PVOID buffer,
	_In_ ULONG_PTR size
);

BOOLEAN _app_isappsigned (
	_In_ ULONG_PTR app_hash
);

BOOLEAN _app_isappvalidbinary (
	_In_opt_ PR_STRING path
);

BOOLEAN _app_isappvalidpath (
	_In_opt_ PR_STRING path
);

_Ret_maybenull_
PR_STRING _app_getappdisplayname (
	_In_ PITEM_APP ptr_app,
	_In_ BOOLEAN is_shortened
);

_Ret_maybenull_
PR_STRING _app_getappname (
	_In_ PITEM_APP ptr_app
);

VOID _app_getfileicon (
	_Inout_ PITEM_APP_INFO ptr_app_info
);

VOID _app_getfilesignatureinfo (
	_In_ HANDLE hfile,
	_Inout_ PITEM_APP_INFO ptr_app_info
);

VOID _app_getfileversioninfo (
	_Inout_ PITEM_APP_INFO ptr_app_info
);

PR_STRING _app_getfilehashinfo (
	_In_ HANDLE hfile,
	_In_ ULONG_PTR app_hash
);

ULONG_PTR _app_addcolor (
	_In_ UINT locale_id,
	_In_ LPCWSTR config_name,
	_In_ BOOLEAN is_enabled,
	_In_ LPCWSTR config_value,
	_In_ COLORREF default_clr
);

COLORREF _app_getcolorvalue (
	_In_ ULONG_PTR color_hash
);

VOID _app_generate_rulescontrol (
	_In_ HMENU hsubmenu,
	_In_ ULONG_PTR app_hash,
	_In_opt_ PITEM_LOG ptr_log
);

VOID _app_generate_timerscontrol (
	_In_ HMENU hsubmenu,
	_In_ ULONG_PTR app_hash
);

BOOLEAN _app_setruletoapp (
	_In_ HWND hwnd,
	_Inout_ PITEM_RULE ptr_rule,
	_In_ INT item_id,
	_In_ PITEM_APP ptr_app,
	_In_ BOOLEAN is_enable
);

_Success_ (return)
BOOLEAN _app_parsenetworkstring (
	_In_ LPCWSTR network_string,
	_Inout_ PITEM_ADDRESS address
);

_Success_ (return)
BOOLEAN _app_preparserulestring (
	_In_ PR_STRINGREF rule,
	_Out_ PITEM_ADDRESS address
);

_Success_ (return)
BOOLEAN _app_parserulestring (
	_In_opt_ PR_STRINGREF rule,
	_Out_ PITEM_ADDRESS address
);

_Ret_maybenull_
PR_STRING _app_resolveaddress (
	_In_ ADDRESS_FAMILY af,
	_In_ LPCVOID address
);

PR_STRING _app_resolveaddress_interlocked (
	_In_ PVOID volatile *string,
	_In_ ADDRESS_FAMILY af,
	_In_ LPCVOID address,
	_In_ BOOLEAN is_resolutionenabled
);

VOID _app_fileloggingenable ();

NTSTATUS NTAPI _app_timercallback (
	_In_opt_ PVOID context
);

VOID _app_getfileinformation (
	_In_ PR_STRING path,
	_In_ ULONG_PTR app_hash,
	_In_ ENUM_TYPE_DATA type,
	_In_ INT listview_id
);

VOID _app_queue_resolver (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ ULONG_PTR hash_code,
	_In_ PVOID base_address
);

VOID NTAPI _app_queue_fileinformation (
	_In_ PVOID arglist,
	_In_ ULONG busy_count
);

VOID NTAPI _app_queue_notifyinformation (
	_In_ PVOID arglist,
	_In_ ULONG busy_count
);

VOID NTAPI _app_queue_resolveinformation (
	_In_ PVOID arglist,
	_In_ ULONG busy_count
);

BOOLEAN _app_wufixenabled ();

VOID _app_wufixhelper (
	_In_ SC_HANDLE hsvcmgr,
	_In_ LPCWSTR service_name,
	_In_ LPCWSTR k_value,
	_In_ BOOLEAN is_enable
);

VOID _app_wufixenable (
	_In_ HWND hwnd,
	_In_ BOOLEAN is_install
);