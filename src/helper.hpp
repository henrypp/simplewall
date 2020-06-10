// simplewall
// Copyright (c) 2016-2020 Henry++

#pragma once

#define FMTADDR_AS_ARPA 0x000001
#define FMTADDR_AS_RULE 0x000002
#define FMTADDR_USE_PROTOCOL 0x000004
#define FMTADDR_RESOLVE_HOST 0x000008

VOID NTAPI _app_dereferenceapp (PVOID pdata);
VOID NTAPI _app_dereferenceappshelper (PVOID pdata);
VOID NTAPI _app_dereferencelog (PVOID pdata);
VOID NTAPI _app_dereferencenetwork (PVOID pdata);
VOID NTAPI _app_dereferencerule (PVOID pdata);
VOID NTAPI _app_dereferenceruleconfig (PVOID pdata);

BOOLEAN _app_formataddress (ADDRESS_FAMILY af, UINT8 proto, const PVOID paddr, UINT16 port, LPWSTR* ptr_dest, DWORD flags);
BOOLEAN _app_formatip (ADDRESS_FAMILY af, PVOID paddr, LPWSTR out_buffer, ULONG buffer_size);
rstring _app_formatport (UINT16 port, BOOLEAN is_noempty);

VOID _app_freeobjects_map (OBJECTS_MAP& ptr_map, SIZE_T max_size);
VOID _app_freeappshelper_map (OBJECTS_APP_HELPER& ptr_map);
VOID _app_freerulesconfig_map (OBJECTS_RULE_CONFIG& ptr_map);
VOID _app_freeobjects_vec (OBJECTS_VEC& ptr_vec);
VOID _app_freelogobjects_vec (OBJECTS_LOG& ptr_vec);
VOID _app_freethreadpool (THREADS_VEC& ptr_vec);
VOID _app_freelogstack ();

VOID _app_getappicon (const PITEM_APP ptr_app, BOOLEAN is_small, PINT picon_id, HICON* picon);
VOID _app_getdisplayname (SIZE_T app_hash, PITEM_APP ptr_app, LPWSTR* extracted_name);
BOOLEAN _app_getfileicon (LPCWSTR path, BOOLEAN is_small, PINT picon_id, HICON* picon);
PR_OBJECT _app_getsignatureinfo (SIZE_T app_hash, const PITEM_APP ptr_app);
PR_OBJECT _app_getversioninfo (SIZE_T app_hash, const PITEM_APP ptr_app);
rstring _app_getservicename (UINT16 port, LPCWSTR default_value);
rstring _app_getprotoname (UINT8 proto, ADDRESS_FAMILY af, LPCWSTR default_value);
rstring _app_getconnectionstatusname (DWORD state, LPCWSTR default_value);
rstring _app_getdirectionname (FWP_DIRECTION direction, BOOLEAN is_loopback, BOOLEAN is_localized);
rstring _app_getfiltername (LPCWSTR provider_name, LPCWSTR filter_name, LPCWSTR default_value);
COLORREF _app_getcolorvalue (SIZE_T color_hash);

VOID _app_generate_connections (OBJECTS_NETWORK& ptr_map, HASHER_MAP& checker_map);
VOID _app_generate_packages ();
VOID _app_generate_services ();

VOID _app_generate_rulesmenu (HMENU hsubmenu, SIZE_T app_hash);
VOID _app_generate_timermenu (HMENU hsubmenu, SIZE_T app_hash);

BOOLEAN _app_item_get (ENUM_TYPE_DATA type, SIZE_T app_hash, rstring* display_name, rstring* real_path, time_t* ptime, PVOID* lpdata);

rstring _app_parsehostaddress_dns (LPCWSTR hostname, USHORT port);

BOOLEAN _app_parsenetworkstring (LPCWSTR network_string, NET_ADDRESS_FORMAT* format_ptr, PUSHORT port_ptr, FWP_V4_ADDR_AND_MASK* paddr4, FWP_V6_ADDR_AND_MASK* paddr6, LPWSTR paddr_dns, SIZE_T dns_length);
BOOLEAN _app_parserulestring (rstring rule, PITEM_ADDRESS ptr_addr);

BOOLEAN _app_resolveaddress (ADDRESS_FAMILY af, PVOID paddr, LPWSTR* pbuffer);

INT _app_getlistview_id (ENUM_TYPE_DATA type);

HBITMAP _app_bitmapfromico (HICON hicon, INT icon_size);
HBITMAP _app_bitmapfrompng (HINSTANCE hinst, LPCWSTR name, INT icon_size);

VOID _app_load_appxmanifest (PITEM_APP_HELPER ptr_app_item);
