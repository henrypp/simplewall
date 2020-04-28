// simplewall
// Copyright (c) 2016-2020 Henry++

#pragma once

#define FMTADDR_AS_ARPA 0x000001
#define FMTADDR_AS_RULE 0x000002
#define FMTADDR_USE_PROTOCOL 0x000004
#define FMTADDR_RESOLVE_HOST 0x000008

void _app_dereferenceapp (PVOID pdata);
void _app_dereferenceappshelper (PVOID pdata);
void _app_dereferencelog (PVOID pdata);
void _app_dereferencenetwork (PVOID pdata);
void _app_dereferencerule (PVOID pdata);
void _app_dereferenceruleconfig (PVOID pdata);
void _app_dereferencestring (PVOID pdata);

bool _app_formataddress (ADDRESS_FAMILY af, UINT8 proto, const PVOID ptr_addr, UINT16 port, LPWSTR* ptr_dest, DWORD flags);
rstring _app_formatport (UINT16 port, bool is_noempty);

void _app_freeobjects_map (OBJECTS_MAP& ptr_map, bool is_forced);
void _app_freeobjects_vec (OBJECTS_VEC& ptr_vec);
void _app_freethreadpool (THREADS_VEC* ptr_pool);
void _app_freelogstack ();

void _app_getappicon (const PITEM_APP ptr_app, bool is_small, PINT picon_id, HICON* picon);
void _app_getdisplayname (size_t app_hash, ITEM_APP* ptr_app, LPWSTR* extracted_name);
bool _app_getfileicon (LPCWSTR path, bool is_small, PINT picon_id, HICON* picon);
PR_OBJECT _app_getsignatureinfo (size_t app_hash, const PITEM_APP ptr_app);
PR_OBJECT _app_getversioninfo (size_t app_hash, const PITEM_APP ptr_app);
rstring _app_getservicename (UINT16 port, LPCWSTR empty_text);
rstring _app_getprotoname (UINT8 proto, ADDRESS_FAMILY af);
rstring _app_getstatename (DWORD state);
COLORREF _app_getcolorvalue (size_t color_hash);

void _app_generate_connections (OBJECTS_MAP& ptr_map, HASHER_MAP& checker_map);
void _app_generate_packages ();
void _app_generate_services ();

void _app_generate_rulesmenu (HMENU hsubmenu, size_t app_hash);
void _app_generate_timermenu (HMENU hsubmenu, size_t app_hash);

bool _app_item_get (EnumDataType type, size_t app_hash, rstring* display_name, rstring* real_path, time_t* ptime, void** lpdata);

void _app_refreshstatus (HWND hwnd, INT listview_id = INVALID_INT);

rstring _app_parsehostaddress_dns (LPCWSTR hostname, USHORT port);
//rstring _app_parsehostaddress_wsa (LPCWSTR hostname, USHORT port);

bool _app_parsenetworkstring (LPCWSTR network_string, NET_ADDRESS_FORMAT* format_ptr, PUSHORT port_ptr, FWP_V4_ADDR_AND_MASK* paddr4, FWP_V6_ADDR_AND_MASK* paddr6, LPWSTR paddr_dns, size_t dns_length);
bool _app_parserulestring (rstring rule, PITEM_ADDRESS ptr_addr);

bool _app_resolveaddress (ADDRESS_FAMILY af, LPVOID paddr, LPWSTR* pbuffer);

INT _app_getlistview_id (EnumDataType type);

HBITMAP _app_bitmapfromico (HICON hicon, INT icon_size);
HBITMAP _app_bitmapfrompng (HINSTANCE hinst, LPCWSTR name, INT icon_size);

void _app_load_appxmanifest (PITEM_APP_HELPER ptr_app_item);
