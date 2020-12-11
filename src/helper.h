// simplewall
// Copyright (c) 2016-2021 Henry++

#pragma once

#define FMTADDR_AS_ARPA 0x0001
#define FMTADDR_AS_RULE 0x0002
#define FMTADDR_USE_PROTOCOL 0x0004
#define FMTADDR_RESOLVE_HOST 0x0008

VOID NTAPI _app_dereferenceapp (PVOID entry);
VOID NTAPI _app_dereferenceruleconfig (PVOID entry);
VOID NTAPI _app_dereferencenetwork (PVOID entry);

VOID NTAPI _app_dereferencelog (PVOID entry);
VOID NTAPI _app_dereferencerule (PVOID entry);

PR_HASHSTORE _app_addcachetable (PR_HASHTABLE table, SIZE_T hash_code, PR_STRING string, INT number);

PR_STRING _app_resolveaddress (ADDRESS_FAMILY af, LPCVOID paddr);
PR_STRING _app_formataddress (ADDRESS_FAMILY af, UINT8 proto, LPCVOID paddr, UINT16 port, ULONG flags);
BOOLEAN _app_formatip (ADDRESS_FAMILY af, LPCVOID paddr, LPWSTR out_buffer, ULONG buffer_size, BOOLEAN is_checkempty);
PR_STRING _app_formatport (UINT16 port, BOOLEAN is_noempty);

VOID _app_freelogstack ();

VOID _app_getappicon (const PITEM_APP ptr_app, BOOLEAN is_small, PINT picon_id, HICON* picon);
LPCWSTR _app_getdisplayname (PITEM_APP ptr_app, BOOLEAN is_shortened);
BOOLEAN _app_getfileicon (LPCWSTR path, BOOLEAN is_small, PINT picon_id, HICON* picon);
PR_STRING _app_getsignatureinfo (PITEM_APP ptr_app);
PR_STRING _app_getversioninfo (PITEM_APP ptr_app);
LPCWSTR _app_getservicename (UINT16 port, LPCWSTR default_value);
LPCWSTR _app_getprotoname (UINT8 proto, ADDRESS_FAMILY af, LPCWSTR default_value);
LPCWSTR _app_getconnectionstatusname (ULONG state, LPCWSTR default_value);
PR_STRING _app_getdirectionname (FWP_DIRECTION direction, BOOLEAN is_loopback, BOOLEAN is_localized);
COLORREF _app_getcolorvalue (SIZE_T color_hash);

VOID _app_generate_connections (PR_HASHTABLE checker_map);
VOID _app_generate_packages ();
VOID _app_generate_services ();

VOID _app_generate_timerscontrol (PVOID hwnd, INT ctrl_id, PITEM_APP ptr_app);

PR_STRING _app_parsehostaddress_dns (LPCWSTR hostname, USHORT port);

BOOLEAN _app_parsenetworkstring (LPCWSTR network_string, NET_ADDRESS_FORMAT* format_ptr, PUSHORT port_ptr, FWP_V4_ADDR_AND_MASK* paddr4, FWP_V6_ADDR_AND_MASK* paddr6, LPWSTR dnsString, SIZE_T dnsLength);
BOOLEAN _app_parserulestring (PR_STRING rule, PITEM_ADDRESS ptr_addr);

INT _app_getlistview_id (ENUM_TYPE_DATA type);

HBITMAP _app_bitmapfromico (HICON hicon, INT icon_size);
HBITMAP _app_bitmapfrompng (HINSTANCE hinst, LPCWSTR name, INT icon_size);
