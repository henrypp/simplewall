// simplewall
// Copyright (c) 2016-2019 Henry++

#pragma once

#include <windows.h>
#include <commctrl.h>

#include "routine.hpp"
#include "resource.hpp"
#include "app.hpp"

// libs
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "dnsapi.lib")
#pragma comment(lib, "fwpuclnt.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "rpcrt4.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "windowscodecs.lib")

// guid
extern "C" {
	static const GUID GUID_WfpProvider =
	{0xb0d553e2, 0xc6a0, 0x4a9a, {0xae, 0xb8, 0xc7, 0x52, 0x48, 0x3e, 0xd6, 0x2f}};

	static const GUID GUID_WfpSublayer =
	{0x9fee6f59, 0xb951, 0x4f9a, {0xb5, 0x2f, 0x13, 0x3d, 0xcf, 0x7a, 0x42, 0x79}};

	// deprecated and not used, but need for compatibility
	static const GUID GUID_WfpOutboundCallout4_DEPRECATED =
	{0xf1251f1a, 0xab09, 0x4ce7, {0xba, 0xe3, 0x6c, 0xcc, 0xce, 0xf2, 0xc8, 0xca}};

	static const GUID GUID_WfpOutboundCallout6_DEPRECATED =
	{0xfd497f2e, 0x46f5, 0x486d, {0xb0, 0xc, 0x3f, 0x7f, 0xe0, 0x7a, 0x94, 0xa6}};

	static const GUID GUID_WfpInboundCallout4_DEPRECATED =
	{0xefc879ce, 0x3066, 0x45bb, {0x8a, 0x70, 0x17, 0xfe, 0x29, 0x78, 0x53, 0xc0}};

	static const GUID GUID_WfpInboundCallout6_DEPRECATED =
	{0xd0420299, 0x52d8, 0x4f18, {0xbc, 0x80, 0x47, 0x3a, 0x24, 0x93, 0xf2, 0x69}};

	static const GUID GUID_WfpListenCallout4_DEPRECATED =
	{0x51fa679d, 0x578b, 0x4835, {0xa6, 0x3e, 0xca, 0xd7, 0x68, 0x7f, 0x74, 0x95}};

	static const GUID GUID_WfpListenCallout6_DEPRECATED =
	{0xa02187ca, 0xe655, 0x4adb, {0xa1, 0xf2, 0x47, 0xa2, 0xc9, 0x78, 0xf9, 0xce}};
};

// enums
enum EnumAppType
{
	AppRegular = 0,
	AppDevice = 1,
	AppNetwork = 2,
	AppService = 3,
	AppPackage = 4, // win8+
	AppPico = 5 // win10+
};

enum EnumRuleType
{
	TypeRuleUnknown = 0,
	TypeBlocklist = 1,
	TypeSystem = 2,
	TypeCustom = 3,
};

enum EnumRuleItemType
{
	TypeRuleItemUnknown = 0,
	TypeHost = 1,
	TypeIp = 2,
	TypePort = 4,
};

enum EnumXmlType
{
	XmlApps = 0,
	XmlRules = 1,
	XmlRulesConfig = 2,
};

INT_PTR CALLBACK EditorProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
INT_PTR CALLBACK SettingsProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

// config
#define WM_TRAYICON WM_APP + 1
#define LANG_MENU 6
#define UID 1984 // if you want to keep a secret, you must also hide it from yourself.

#define XML_APPS L"apps.xml"
#define XML_BLOCKLIST L"blocklist.xml"
#define XML_RULES_CONFIG L"rules_config.xml"
#define XML_RULES_CUSTOM L"rules_custom.xml"
#define XML_RULES_SYSTEM L"rules_system.xml"

#define LOG_DIV L','
#define LOG_PATH_EXT L"log"
#define LOG_PATH_DEFAULT L"%userprofile%\\" APP_NAME_SHORT L"." LOG_PATH_EXT

#define PROC_SYSTEM_PID 4
#define PROC_SYSTEM_NAME L"System"

#define PATH_NTOSKRNL L"%systemroot%\\system32\\ntoskrnl.exe"
#define PATH_SVCHOST L"%systemroot%\\system32\\svchost.exe"
#define PATH_WINSTORE L"%systemroot%\\system32\\wsreset.exe"

#define WIKI_URL L"https://github.com/henrypp/simplewall/wiki/Rules-editor#rule-syntax-format"

#define BOOTTIME_FILTER_NAME L"Boot-time filter"
#define SUBLAYER_WEIGHT_DEFAULT 65534

#define SERVICE_SECURITY_DESCRIPTOR L"O:SYG:SYD:(A;;CCRC;;;%s)"

#define SZ_TAB L"   "
#define SZ_EMPTY L"<empty>"

#define SZ_LOG_REMOTE_ADDRESS L"Remote"
#define SZ_LOG_LOCAL_ADDRESS L"Local"
#define SZ_LOG_BLOCK L"BLOCK"
#define SZ_LOG_ALLOW L"ALLOW"
#define SZ_LOG_DIRECTION_IN L"IN"
#define SZ_LOG_DIRECTION_OUT L"OUT"
#define SZ_LOG_DIRECTION_LOOPBACK L"-Loopback"

#define RULE_DELIMETER L";"
#define RULE_RANGE_CHAR L'-'
#define UI_FONT L"Segoe UI"
#define UI_FONT_NOTIFICATION L"Calibri"
#define UI_FONT_DEFAULT UI_FONT L";9;400"
#define BACKUP_HOURS_PERIOD 4 // make backup every X hour(s) (default)
#define LOG_SIZE_LIMIT 1024

#define LEN_IP_MAX 68
#define UMAP_CACHE_LIMIT 1024

#define FILTERS_TIMEOUT 9000
#define TRANSACTION_TIMEOUT 4000

// notifications
#define NOTIFY_CLASS_DLG L"NotificationDlg"

#define NOTIFY_WIDTH 334
#define NOTIFY_HEIGHT 372
#define NOTIFY_BTN_WIDTH 110

#define NOTIFY_PATH_COMPACT 36

#define NOTIFY_CLR_BG GetSysColor (COLOR_WINDOW)
#define NOTIFY_CLR_BG_BRUSH GetSysColorBrush (COLOR_WINDOW)
#define NOTIFY_CLR_TEXT GetSysColor (COLOR_WINDOWTEXT)
#define NOTIFY_CLR_BORDER RGB (255, 112, 0)
#define NOTIFY_CLR_TITLE_BG RGB (240, 240, 240)
#define NOTIFY_CLR_TITLE_TEXT RGB (102, 102, 102)

#define NOTIFY_TIMER_POPUP_ID 1001
#define NOTIFY_TIMER_TIMEOUT_ID 2002
#define NOTIFY_TIMER_SAFETY_ID 3003

#define NOTIFY_TIMER_POPUP 350
#define NOTIFY_TIMER_SAFETY 500

#define NOTIFY_TIMER_DEFAULT 30 // sec.
#define NOTIFY_TIMEOUT_DEFAULT 30 // sec.

#define NOTIFY_LIMIT_SIZE 16 // limit notifications pool size
#define NOTIFY_LIMIT_POOL_SIZE 128
#define NOTIFY_LIMIT_THREAD_COUNT 2

#define NOTIFY_SOUND_DEFAULT L"MailBeep"

// pugixml document configuration
#define PUGIXML_LOAD_FLAGS (pugi::parse_escapes)
#define PUGIXML_LOAD_ENCODING (pugi::encoding_auto)

#define PUGIXML_SAVE_FLAGS (pugi::format_indent | pugi::format_write_bom)
#define PUGIXML_SAVE_ENCODING (pugi::encoding_wchar)

// default colors
#define LISTVIEW_COLOR_INVALID RGB (255, 125, 148)
#define LISTVIEW_COLOR_PACKAGE RGB(134, 227, 227)
#define LISTVIEW_COLOR_PICO RGB (51, 153, 255)
#define LISTVIEW_COLOR_SERVICE RGB (207, 189, 255)
#define LISTVIEW_COLOR_SIGNED RGB (175, 228, 163)
#define LISTVIEW_COLOR_SILENT RGB (180, 193, 201)
#define LISTVIEW_COLOR_SPECIAL RGB (255, 255, 170)
#define LISTVIEW_COLOR_SYSTEM RGB(151, 196, 251)
#define LISTVIEW_COLOR_TIMER RGB(255, 190, 142)

// filter weights
#define FILTER_WEIGHT_HIGHEST_IMPORTANT 0xF
#define FILTER_WEIGHT_HIGHEST 0xE
#define FILTER_WEIGHT_BLOCKLIST 0xD
#define FILTER_WEIGHT_CUSTOM_BLOCK 0xC
#define FILTER_WEIGHT_CUSTOM 0xB
#define FILTER_WEIGHT_SYSTEM 0xA
#define FILTER_WEIGHT_APPLICATION 0x9
#define FILTER_WEIGHT_LOWEST 0x8

// memory limitation for 1 rule
#define RULE_NAME_CCH_MAX 64
#define RULE_RULE_CCH_MAX 256

typedef std::vector<GUID> MARRAY;

struct STATIC_DATA
{
	WCHAR apps_path[MAX_PATH] = {0};
	WCHAR rules_blocklist_path[MAX_PATH] = {0};
	WCHAR rules_system_path[MAX_PATH] = {0};
	WCHAR rules_custom_path[MAX_PATH] = {0};
	WCHAR rules_config_path[MAX_PATH] = {0};

	WCHAR apps_path_backup[MAX_PATH] = {0};
	WCHAR rules_custom_path_backup[MAX_PATH] = {0};
	WCHAR rules_config_path_backup[MAX_PATH] = {0};

	WCHAR windows_dir[MAX_PATH] = {0};
	WCHAR tmp1_dir[MAX_PATH] = {0};

	WCHAR title[128] = {0};

	PACL pacl_default = nullptr;
	PACL pacl_engine = nullptr;
	PACL pacl_secure = nullptr;
	PSID pusersid = nullptr;
	LPGUID psession = nullptr;

	HIMAGELIST himg = nullptr;
	HANDLE hengine = nullptr;
	HANDLE hlogfile = nullptr;
	HANDLE hnetevent = nullptr;
	HANDLE done_evt = nullptr;
	HANDLE htimer = nullptr;
	HFONT hfont = nullptr;
	HICON hicon_large = nullptr;
	HICON hicon_small = nullptr;
	HICON hicon_package = nullptr;
	HWND hnotification = nullptr;
	HWND htoolbar = nullptr;

	time_t blocklist_timestamp = 0;
	time_t rule_system_timestamp = 0;

	size_t ntoskrnl_hash = 0;
	size_t svchost_hash = 0;
	size_t myhash = 0;

	size_t tmp1_length = 0;
	size_t wd_length = 0;

	size_t icon_id = 0;
	size_t icon_package_id = 0;

	bool is_notifytimeout = false;
	bool is_notifymouse = false;
	bool is_wsainit = false;
	bool is_neteventset = false;
};

typedef struct tagITEM_STATUS
{
	size_t apps_count = 0;
	size_t apps_timer_count = 0;
	size_t apps_unused_count = 0;

	size_t rules_count = 0;
	size_t rules_global_count = 0;
	size_t rules_predefined_count = 0;
	size_t rules_user_count = 0;
} ITEM_STATUS, *PITEM_STATUS;

typedef struct tagITEM_APP
{
	MARRAY mfarr;

	LPWSTR display_name = nullptr;
	LPWSTR original_path = nullptr;
	LPWSTR real_path = nullptr;

	LPWSTR description = nullptr;
	LPWSTR signer = nullptr;

	union
	{
		PSECURITY_DESCRIPTOR psd = nullptr; // service app
		PSID psid; // store app (win8+)
	};

	HANDLE htimer = nullptr;

	time_t timestamp = 0;
	time_t timer = 0;
	time_t last_notify = 0;

	size_t icon_id = 0;

	EnumAppType type = AppRegular;

	bool is_haveerrors = false;

	bool is_enabled = false;
	bool is_system = false;
	bool is_silent = false;
	bool is_signed = false;
	bool is_temp = false;
	bool is_undeletable = false;
} ITEM_APP, *PITEM_APP;

typedef struct tagITEM_RULE
{
	tagITEM_RULE ()
	{
		pname = nullptr;
		prule_remote = nullptr;
		prule_local = nullptr;
	}

	~tagITEM_RULE ()
	{
		SAFE_DELETE_ARRAY (pname);
		SAFE_DELETE_ARRAY (prule_remote);
		SAFE_DELETE_ARRAY (prule_local);
	}

	std::unordered_map<size_t, bool> apps;

	MARRAY mfarr;

	LPWSTR pname = nullptr;
	LPWSTR prule_remote = nullptr;
	LPWSTR prule_local = nullptr;

	ADDRESS_FAMILY af = AF_UNSPEC;

	FWP_DIRECTION dir = FWP_DIRECTION_OUTBOUND;

	UINT8 weight = 0;
	UINT8 protocol = 0;

	EnumRuleType type = TypeRuleUnknown;

	bool is_haveerrors = false;
	bool is_forservices = false;
	bool is_readonly = false;
	bool is_enabled = false;
	bool is_block = false;
} ITEM_RULE, *PITEM_RULE;

typedef struct _ITEM_RULE_CONFIG
{
	_ITEM_RULE_CONFIG ()
	{
		pname = nullptr;
		papps = nullptr;
	}

	~_ITEM_RULE_CONFIG ()
	{
		SAFE_DELETE_ARRAY (pname);
		SAFE_DELETE_ARRAY (papps);
	}

	LPWSTR pname = nullptr;
	LPWSTR papps = nullptr;

	bool is_enabled = false;
} ITEM_RULE_CONFIG, *PITEM_RULE_CONFIG;

typedef struct tagITEM_LOG
{
	tagITEM_LOG ()
	{
		path = nullptr;
		provider_name = nullptr;
		filter_name = nullptr;
		remote_fmt = nullptr;
		local_fmt = nullptr;
		username = nullptr;
	}

	~tagITEM_LOG ()
	{
		SAFE_DELETE_ARRAY (path);
		SAFE_DELETE_ARRAY (provider_name);
		SAFE_DELETE_ARRAY (filter_name);
		SAFE_DELETE_ARRAY (username);
		SAFE_DELETE_ARRAY (remote_fmt);
		SAFE_DELETE_ARRAY (local_fmt);
	}

	LPWSTR path = nullptr;
	LPWSTR provider_name = nullptr;
	LPWSTR filter_name = nullptr;
	LPWSTR remote_fmt = nullptr;
	LPWSTR local_fmt = nullptr;
	LPWSTR username = nullptr;

	time_t date = 0;

	size_t hash = 0;

	ADDRESS_FAMILY af = 0;

	UINT64 filter_id = 0;

	FWP_DIRECTION direction = FWP_DIRECTION_OUTBOUND;

	union
	{
		IN_ADDR remote_addr = {0};
		IN6_ADDR remote_addr6;
	};

	union
	{
		IN_ADDR local_addr = {0};
		IN6_ADDR local_addr6;
	};

	UINT16 remote_port = 0;
	UINT16 local_port = 0;

	UINT8 protocol = 0;

	bool is_allow = false;
	bool is_loopback = false;
	bool is_blocklist = false;
	bool is_custom = false;
	bool is_system = false;
	bool is_myprovider = false;
} ITEM_LOG, *PITEM_LOG;

typedef struct tagITEM_NETWORK
{
	tagITEM_NETWORK ()
	{
		path = nullptr;
		remote_fmt = nullptr;
		local_fmt = nullptr;
	}

	~tagITEM_NETWORK ()
	{
		SAFE_DELETE_ARRAY (path);
		SAFE_DELETE_ARRAY (remote_fmt);
		SAFE_DELETE_ARRAY (local_fmt);
	}

	LPWSTR path = nullptr;
	LPWSTR remote_fmt = nullptr;
	LPWSTR local_fmt = nullptr;

	size_t hash = 0;

	ADDRESS_FAMILY af = 0;

	FWP_DIRECTION direction = FWP_DIRECTION_OUTBOUND;

	union
	{
		IN_ADDR remote_addr = {0};
		IN6_ADDR remote_addr6;
	};

	union
	{
		IN_ADDR local_addr = {0};
		IN6_ADDR local_addr6;
	};

	size_t icon_id=0;

	UINT16 remote_port = 0;
	UINT16 local_port = 0;

	UINT8 protocol = 0;
} ITEM_NETWORK, *PITEM_NETWORK;

typedef struct tagITEM_ADD
{
	tagITEM_ADD ()
	{
		hash = 0;

		sid = nullptr;
		display_name = nullptr;
		service_name = nullptr;
		real_path = nullptr;

		psid = nullptr;
		hbmp = nullptr;
	}

	~tagITEM_ADD ()
	{
		SAFE_DELETE_ARRAY (sid);
		SAFE_DELETE_ARRAY (display_name);
		SAFE_DELETE_ARRAY (service_name);
		SAFE_DELETE_ARRAY (real_path);

		if (hbmp)
		{
			DeleteObject (hbmp);
			hbmp = nullptr;
		}
	}

	size_t hash = 0;
	time_t timestamp = 0;

	EnumAppType type = AppRegular;

	LPWSTR display_name = nullptr;
	LPWSTR real_path = nullptr;
	LPWSTR sid = nullptr;
	LPWSTR service_name = nullptr;

	union
	{
		PSID psid = nullptr;
		PSECURITY_DESCRIPTOR psd;
	};

	HBITMAP hbmp = nullptr;

} ITEM_ADD, *PITEM_ADD;

typedef struct tagITEM_COLOR
{
	tagITEM_COLOR ()
	{
		pcfg_name = nullptr;
		pcfg_value = nullptr;
	}

	~tagITEM_COLOR ()
	{
		SAFE_DELETE_ARRAY (pcfg_name);
		SAFE_DELETE_ARRAY (pcfg_value);
	}

	LPWSTR pcfg_name = nullptr;
	LPWSTR pcfg_value = nullptr;

	size_t hash = 0;

	UINT locale_id = 0;

	COLORREF default_clr = 0;
	COLORREF clr = 0;

	bool is_enabled = false;
} ITEM_COLOR, *PITEM_COLOR;

typedef struct tagITEM_PROTOCOL
{
	tagITEM_PROTOCOL ()
	{
		pname = nullptr;
	}

	~tagITEM_PROTOCOL ()
	{
		SAFE_DELETE_ARRAY (pname);
	}

	LPWSTR pname = nullptr;
	UINT8 id = 0;
} ITEM_PROTOCOL, *PITEM_PROTOCOL;

typedef struct tagITEM_ADDRESS
{
	WCHAR host[NI_MAXHOST] = {0};

	FWP_V4_ADDR_AND_MASK* paddr4 = nullptr;
	FWP_V6_ADDR_AND_MASK* paddr6 = nullptr;

	FWP_RANGE* prange = nullptr;

	EnumRuleItemType type = TypeRuleItemUnknown;

	NET_ADDRESS_FORMAT format = NET_ADDRESS_FORMAT_UNSPECIFIED;

	UINT16 port = 0;

	bool is_range = false;
} ITEM_ADDRESS, *PITEM_ADDRESS;

typedef struct tagITEM_LIST_HEAD
{
	SLIST_HEADER ListHead;

	volatile LONG item_count;
	volatile LONG thread_count;
} ITEM_LIST_HEAD, *PITEM_LIST_HEAD;

typedef struct tagITEM_LIST_ENTRY
{
	SLIST_ENTRY ListEntry;

#ifndef _WIN64
	ULONG_PTR Reserved = 0;
#endif // _WIN64

	PITEM_LOG Body = nullptr;
} ITEM_LIST_ENTRY, *PITEM_LIST_ENTRY;

C_ASSERT (FIELD_OFFSET (ITEM_LIST_ENTRY, ListEntry) == 0);
C_ASSERT (FIELD_OFFSET (ITEM_LIST_ENTRY, Body) == MEMORY_ALLOCATION_ALIGNMENT);
