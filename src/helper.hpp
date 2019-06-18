// simplewall
// Copyright (c) 2016-2019 Henry++

#pragma once

#define FMTADDR_AS_ARPA 0x000001
#define FMTADDR_AS_RULE 0x000002
#define FMTADDR_USE_PROTOCOL 0x000004
#define FMTADDR_RESOLVE_HOST 0x000008

void _app_dereferenceapp (PVOID pdata);
void _app_dereferenceappshelper (PVOID pdata);
void _app_dereferencecolor (PVOID pdata);
void _app_dereferencelog (PVOID pdata);
void _app_dereferencenetwork (PVOID pdata);
void _app_dereferencerule (PVOID pdata);
void _app_dereferenceruleconfig (PVOID pdata);
void _app_dereferencestring (PVOID pdata);

UINT _app_gettab_id (HWND hwnd, size_t page_id = LAST_VALUE);
void _app_settab_id (HWND hwnd, size_t page_id);

bool _app_initinterfacestate (HWND hwnd);
void _app_restoreinterfacestate (HWND hwnd, bool is_enabled);
void _app_setinterfacestate (HWND hwnd);

void _app_applycasestyle (LPWSTR buffer, size_t length);

bool _app_formataddress (ADDRESS_FAMILY af, UINT8 proto, const PVOID ptr_addr, UINT16 port, LPWSTR* ptr_dest, DWORD flags);

void _app_freeobjects_map (OBJECTS_MAP& ptr_map, OBJECT_CLEANUP_CALLBACK cleanup_callback, bool is_forced);
void _app_freeobjects_vec (OBJECTS_VEC& ptr_vec, OBJECT_CLEANUP_CALLBACK cleanup_callback);
void _app_freethreadpool (THREADS_VEC* ptr_pool);
void _app_freelogstack ();

void _app_getappicon (ITEM_APP* ptr_app, bool is_small, size_t* picon_id, HICON* picon);
void _app_getdisplayname (size_t app_hash, ITEM_APP* ptr_app, LPWSTR* extracted_name);
bool _app_getfileicon (LPCWSTR path, bool is_small, size_t* picon_id, HICON* picon);
rstring _app_getshortcutpath (HWND hwnd, LPCWSTR path);
PR_OBJECT _app_getsignatureinfo (size_t app_hash, PITEM_APP ptr_app);
PR_OBJECT _app_getversioninfo (size_t app_hash, PITEM_APP ptr_app);
rstring _app_getportname (UINT16 port);
rstring _app_getprotoname (UINT8 proto);
rstring _app_getstatename (DWORD state);

void _app_generate_connections (OBJECTS_MAP& ptr_map, CHECKER_MAP& checker_map);
void _app_generate_packages ();
void _app_generate_services ();

void _app_generate_rulesmenu (HMENU hsubmenu, size_t app_hash);

bool _app_item_get (EnumDataType type, size_t app_hash, rstring* display_name, rstring* real_path, PBYTE* lpdata);

INT CALLBACK _app_listviewcompare_callback (LPARAM lp1, LPARAM lp2, LPARAM lparam);
void _app_listviewsort (HWND hwnd, UINT listview_id, INT subitem = -1, bool is_notifycode = false);

void _app_refreshstatus (HWND hwnd);

rstring _app_parsehostaddress_dns (LPCWSTR host, USHORT port);
rstring _app_parsehostaddress_wsa (LPCWSTR hostname, USHORT port);

bool _app_parsenetworkstring (LPCWSTR network_string, NET_ADDRESS_FORMAT* format_ptr, USHORT* port_ptr, FWP_V4_ADDR_AND_MASK* paddr4, FWP_V6_ADDR_AND_MASK* paddr6, LPWSTR paddr_dns, size_t dns_length);
bool _app_parserulestring (rstring rule, PITEM_ADDRESS ptr_addr);

bool _app_resolveaddress (ADDRESS_FAMILY af, LPVOID paddr, LPWSTR* pbuffer);
void _app_resolvefilename (rstring& path);

UINT _app_getlistview_id (EnumDataType type);

size_t _app_getposition (HWND hwnd, UINT listview_id, size_t idx);
void _app_showitem (HWND hwnd, UINT listview_id, size_t item, INT scroll_pos = -1);

HBITMAP _app_bitmapfromico (HICON hicon, INT icon_size);
HBITMAP _app_bitmapfrompng (HINSTANCE hinst, LPCWSTR name, INT icon_size);

void _app_load_appxmanifest (PITEM_APP_HELPER ptr_app_item);
LPVOID _app_loadresource (HINSTANCE hinst, LPCWSTR res, LPCWSTR type, PDWORD size);
