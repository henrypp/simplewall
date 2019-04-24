// simplewall
// Copyright (c) 2016-2019 Henry++

#pragma once

UINT _app_gettab_id (HWND hwnd, size_t page_id = LAST_VALUE);
void _app_settab_id (HWND hwnd, size_t page_id);

bool _app_initinterfacestate ();
void _app_restoreinterfacestate (bool is_enabled);
void _app_setinterfacestate (HWND hwnd);

void _app_applycasestyle (LPWSTR buffer, size_t length);

bool _app_formataddress (ADDRESS_FAMILY af, PVOID const ptr_addr, UINT16 port, LPWSTR *ptr_dest, bool is_appenddns);

void _app_freearray (std::vector<PITEM_ADD>* ptr);
void _app_freecache (MCACHE_MAP* ptr_map);
void _app_freethreadpool (MTHREADPOOL* ptr_pool);
void _app_freelogstack ();
void _app_getappicon (ITEM_APP const *ptr_app, bool is_small, size_t* picon_id, HICON* picon);
void _app_getdisplayname (size_t hash, ITEM_APP const *ptr_app, LPWSTR* extracted_name);
bool _app_getfileicon (LPCWSTR path, bool is_small, size_t* picon_id, HICON* picon);
rstring _app_getshortcutpath (HWND hwnd, LPCWSTR path);
bool _app_getsignatureinfo (size_t hash, LPCWSTR path, LPWSTR* psigner);
bool _app_getversioninfo (size_t hash, LPCWSTR path, LPWSTR* pinfo);
size_t _app_getposition (HWND hwnd, size_t hash);
rstring _app_getprotoname (UINT8 proto);

void _app_generate_connections ();
void _app_generate_packages ();
void _app_generate_services ();

void _app_generate_rulesmenu (HMENU hsubmenu, size_t app_hash);

bool _app_item_get (EnumAppType type, size_t hash, rstring* display_name, rstring* real_path, PSID* lpsid, PSECURITY_DESCRIPTOR* lpsd, rstring* /*description*/);

INT CALLBACK _app_listviewcompare_abc (LPARAM item1, LPARAM item2, LPARAM lparam);
INT CALLBACK _app_listviewcompare_apps (LPARAM lp1, LPARAM lp2, LPARAM lparam);
INT CALLBACK _app_listviewcompare_rules (LPARAM item1, LPARAM item2, LPARAM lparam);
void _app_listviewsort (HWND hwnd, UINT ctrl_id, INT subitem = -1, bool is_notifycode = false);

void _app_refreshstatus (HWND hwnd);

rstring _app_parsehostaddress_dns (LPCWSTR host, USHORT port);
rstring _app_parsehostaddress_wsa (LPCWSTR hostname, USHORT port);

bool _app_parsenetworkstring (LPCWSTR network_string, NET_ADDRESS_FORMAT* format_ptr, USHORT* port_ptr, FWP_V4_ADDR_AND_MASK* paddr4, FWP_V6_ADDR_AND_MASK* paddr6, LPWSTR paddr_dns, size_t dns_length);
bool _app_parserulestring (rstring rule, PITEM_ADDRESS ptr_addr);

bool _app_resolveaddress (ADDRESS_FAMILY af, LPVOID paddr, LPWSTR buffer, DWORD length);
void _app_resolvefilename (rstring& path);

UINT _app_getapplistview_id (size_t hash);
UINT _app_getrulelistview_id (const PITEM_RULE ptr_rule);

void _app_showitem (HWND hwnd, UINT listview_id, LPARAM lparam, INT scroll_pos = -1);

HBITMAP _app_bitmapfromico (HICON hicon, INT icon_size);
HBITMAP _app_bitmapfrompng (HINSTANCE hinst, LPCWSTR name, INT icon_size);

void _app_load_appxmanifest (PITEM_ADD ptr_item);
LPVOID _app_loadresource (HINSTANCE hinst, LPCWSTR res, LPCWSTR type, PDWORD size);
