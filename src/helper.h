// simplewall
// Copyright (c) 2016-2021 Henry++

#pragma once

#define FMTADDR_AS_ARPA 0x0001
#define FMTADDR_AS_RULE 0x0002
#define FMTADDR_USE_PROTOCOL 0x0004

VOID NTAPI _app_dereferenceapp (_In_ PVOID entry);
VOID NTAPI _app_dereferenceruleconfig (_In_ PVOID entry);
VOID NTAPI _app_dereferencenetwork (_In_ PVOID entry);

VOID NTAPI _app_dereferencelog (_In_ PVOID entry);
VOID NTAPI _app_dereferencerule (_In_ PVOID entry);

BOOLEAN _app_ischeckboxlocked (_In_ HWND hwnd);
VOID _app_setcheckboxlock (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ BOOLEAN is_lock);

VOID _app_addcachetable (_Inout_ PR_HASHTABLE hashtable, _In_ ULONG_PTR hash_code, _In_ PR_QUEUED_LOCK spin_lock, _In_ PR_STRING string);

_Ret_maybenull_
PVOID _app_getcachetable (_Inout_ PR_HASHTABLE cache_table, _In_ ULONG_PTR hash_code, _In_ PR_QUEUED_LOCK spin_lock);

_Ret_maybenull_
PR_STRING _app_formataddress (_In_ ADDRESS_FAMILY af, _In_ UINT8 proto, _In_ LPCVOID address, _In_opt_ UINT16 port, _In_ ULONG flags);

_Success_ (return)
BOOLEAN _app_formatip (_In_ ADDRESS_FAMILY af, _In_ LPCVOID address, _Out_writes_to_ (buffer_size, buffer_size) LPWSTR out_buffer, _In_ ULONG buffer_size, _In_ BOOLEAN is_checkempty);

PR_STRING _app_formatport (_In_ UINT16 port, _In_ UINT8 proto);

BOOLEAN _app_isappvalidbinary (_In_ PITEM_APP ptr_app);
BOOLEAN _app_isappvalidpath (_In_ PR_STRINGREF path);

BOOLEAN _app_getfileicon (_In_ LPCWSTR path, _In_ BOOLEAN is_small, _Out_opt_ PINT icon_id, _Out_opt_ HICON* hicon);
BOOLEAN _app_getappicon (_In_opt_ PITEM_APP ptr_app, _In_ BOOLEAN is_small, _Out_opt_ PINT icon_id, _Out_opt_ HICON* hicon);

LPCWSTR _app_getappdisplayname (_In_ PITEM_APP ptr_app, _In_ BOOLEAN is_shortened);
VOID _app_getsignatureinfo (_Inout_ PITEM_APP ptr_app);
VOID _app_getversioninfo (_Inout_ PITEM_APP ptr_app);

_Ret_maybenull_
LPCWSTR _app_getservicename (_In_ UINT16 port, _In_ UINT8 proto, _In_opt_ LPCWSTR default_value);

_Ret_maybenull_
LPCWSTR _app_getprotoname (_In_ ULONG proto, _In_ ADDRESS_FAMILY af, _In_opt_ LPCWSTR default_value);

_Ret_maybenull_
LPCWSTR _app_getconnectionstatusname (_In_ ULONG state, _In_opt_ LPCWSTR default_value);

_Ret_maybenull_
PR_STRING _app_getdirectionname (_In_ FWP_DIRECTION direction, _In_ BOOLEAN is_loopback, _In_ BOOLEAN is_localized);

ULONG_PTR _app_addcolor (_In_ UINT locale_id, _In_ LPCWSTR config_name, _In_ BOOLEAN is_enabled, _In_ LPCWSTR config_value, _In_ COLORREF default_clr);
COLORREF _app_getcolorvalue (_In_ ULONG_PTR color_hash);

VOID _app_generate_connections (_Inout_ PR_HASHTABLE network_ptr, _Inout_ PR_HASHTABLE);
VOID _app_generate_packages ();
VOID _app_generate_services ();

VOID _app_generate_rulescontrol (_In_ HMENU hsubmenu, _In_opt_ ULONG_PTR app_hash);
VOID _app_generate_timerscontrol (_In_ HMENU hsubmenu, _In_opt_ PITEM_APP ptr_app);

BOOLEAN _app_setruletoapp (_In_ HWND hwnd, _Inout_ PITEM_RULE ptr_rule, _In_ INT item_id, _In_ PITEM_APP ptr_app, _In_ BOOLEAN is_enable);

_Ret_maybenull_
PR_STRING _app_parsehoststring (_In_ LPCWSTR hostname, _In_opt_ USHORT port);

_Success_ (return)
BOOLEAN _app_parsenetworkstring (_In_ LPCWSTR network_string, _Inout_ PITEM_ADDRESS address);

BOOLEAN _app_preparserulestring (_In_ PR_STRINGREF rule, _In_ PITEM_ADDRESS address);
BOOLEAN _app_parserulestring (_In_opt_ PR_STRINGREF rule, _Inout_opt_ PITEM_ADDRESS address);

_Ret_maybenull_
PR_STRING _app_resolveaddress (_In_ ADDRESS_FAMILY af, _In_ LPCVOID address);

VOID NTAPI _app_queuefileinformation (_In_ PVOID arglist, _In_ ULONG busy_count);
VOID NTAPI _app_queuenotifyinformation (_In_ PVOID arglist, _In_ ULONG busy_count);
VOID NTAPI _app_queueresolveinformation (_In_ PVOID arglist, _In_ ULONG busy_count);

_Ret_maybenull_
HBITMAP _app_bitmapfromico (_In_ HICON hicon, _In_ INT icon_size);

_Ret_maybenull_
HBITMAP _app_bitmapfrompng (_In_opt_ HINSTANCE hinst, _In_ LPCWSTR name, _In_ INT icon_size);
