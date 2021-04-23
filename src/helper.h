// simplewall
// Copyright (c) 2016-2021 Henry++

#pragma once

#define FMTADDR_AS_ARPA 0x0001
#define FMTADDR_AS_RULE 0x0002
#define FMTADDR_USE_PROTOCOL 0x0004
#define FMTADDR_RESOLVE_HOST 0x0008

VOID NTAPI _app_dereferenceapp (_In_ PVOID entry);
VOID NTAPI _app_dereferenceruleconfig (_In_ PVOID entry);
VOID NTAPI _app_dereferencenetwork (_In_ PVOID entry);

VOID NTAPI _app_dereferencelog (_In_ PVOID entry);
VOID NTAPI _app_dereferencerule (_In_ PVOID entry);

PR_HASHSTORE _app_addcachetable (_Inout_ PR_HASHTABLE hashtable, _In_ SIZE_T hash_code, _In_opt_ PR_STRING string, _In_opt_ LONG number);

_Ret_maybenull_
PR_STRING _app_resolveaddress (_In_ ADDRESS_FAMILY af, _In_ LPCVOID address);

_Ret_maybenull_
PR_STRING _app_formataddress (_In_ ADDRESS_FAMILY af, _In_ UINT8 proto, _In_ LPCVOID address, _In_opt_ UINT16 port, _In_ ULONG flags);

BOOLEAN _app_formatip (_In_ ADDRESS_FAMILY af, _In_ LPCVOID address, _Out_writes_to_ (buffer_size, buffer_size) LPWSTR out_buffer, _In_ ULONG buffer_size, _In_ BOOLEAN is_checkempty);

_Ret_maybenull_
PR_STRING _app_formatport (_In_ UINT16 port, _In_ UINT8 proto, _In_ BOOLEAN is_noempty);

VOID _app_freelogstack ();

VOID _app_getappicon (_In_ const PITEM_APP ptr_app, _In_ BOOLEAN is_small, _Out_opt_ PINT picon_id, _Out_opt_ HICON* picon);
LPCWSTR _app_getdisplayname (_In_ PITEM_APP ptr_app, _In_ BOOLEAN is_shortened);

_Success_ (return)
BOOLEAN _app_getfileicon (_In_ LPCWSTR path, _In_ BOOLEAN is_small, _Out_opt_ PINT picon_id, _Out_opt_ HICON * picon);

_Ret_maybenull_
PR_STRING _app_getsignatureinfo (_Inout_ PITEM_APP ptr_app);

_Ret_maybenull_
PR_STRING _app_getversioninfo (_In_ PITEM_APP ptr_app);

_Ret_maybenull_
LPCWSTR _app_getservicename (_In_ UINT16 port, _In_ UINT8 proto, _In_opt_ LPCWSTR default_value);

_Ret_maybenull_
LPCWSTR _app_getprotoname (_In_ UINT8 proto, _In_ ADDRESS_FAMILY af, _In_opt_ LPCWSTR default_value);

_Ret_maybenull_
LPCWSTR _app_getconnectionstatusname (_In_ ULONG state, _In_opt_ LPCWSTR default_value);

_Ret_maybenull_
PR_STRING _app_getdirectionname (_In_ FWP_DIRECTION direction, _In_ BOOLEAN is_loopback, _In_ BOOLEAN is_localized);

COLORREF _app_getcolorvalue (_In_ SIZE_T color_hash);

VOID _app_generate_connections (_Inout_ PR_HASHTABLE checker_map);
VOID _app_generate_packages ();
VOID _app_generate_services ();

VOID _app_generate_rulescontrol (_In_ HMENU hsubmenu, _In_opt_ SIZE_T app_hash);
VOID _app_generate_timerscontrol (_In_ HMENU hsubmenu, _In_opt_ PITEM_APP ptr_app);

BOOLEAN _app_setruletoapp (_In_ HWND hwnd, _Inout_ PITEM_RULE ptr_rule, _In_ INT item_id, _In_ PITEM_APP ptr_app, _In_ BOOLEAN is_enable);

_Ret_maybenull_
PR_STRING _app_parsehoststring (_In_ LPCWSTR hostname, _In_opt_ USHORT port);

_Success_ (return)
BOOLEAN _app_parsenetworkstring (_In_ LPCWSTR network_string, _Inout_ PITEM_ADDRESS address);

BOOLEAN _app_preparserulestring (_In_ PR_STRING rule, _In_ PITEM_ADDRESS address);
BOOLEAN _app_parserulestring (_In_opt_ PR_STRING rule, _Inout_opt_ PITEM_ADDRESS address);

INT _app_getlistview_id (_In_ ENUM_TYPE_DATA type);

_Ret_maybenull_
HBITMAP _app_bitmapfromico (_In_ HICON hicon, _In_ INT icon_size);

_Ret_maybenull_
HBITMAP _app_bitmapfrompng (_In_opt_ HINSTANCE hinst, _In_ LPCWSTR name, _In_ INT icon_size);
