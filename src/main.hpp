// simplewall
// Copyright (c) 2016-2020 Henry++

#pragma once

#include "routine.hpp"

#include "resource.hpp"
#include "app.hpp"
#include "global.hpp"

// libs
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "dnsapi.lib")
#pragma comment(lib, "fwpuclnt.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "rpcrt4.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "windowscodecs.lib")


// guids
EXTERN_C static const GUID GUID_WfpProvider = {0xb0d553e2, 0xc6a0, 0x4a9a, {0xae, 0xb8, 0xc7, 0x52, 0x48, 0x3e, 0xd6, 0x2f}};
EXTERN_C static const GUID GUID_WfpSublayer = {0x9fee6f59, 0xb951, 0x4f9a, {0xb5, 0x2f, 0x13, 0x3d, 0xcf, 0x7a, 0x42, 0x79}};

// deprecated and not used, but need for compatibility
EXTERN_C static GUID GUID_WfpOutboundCallout4_DEPRECATED = {0xf1251f1a, 0xab09, 0x4ce7, {0xba, 0xe3, 0x6c, 0xcc, 0xce, 0xf2, 0xc8, 0xca}};
EXTERN_C static GUID GUID_WfpOutboundCallout6_DEPRECATED = {0xfd497f2e, 0x46f5, 0x486d, {0xb0, 0xc, 0x3f, 0x7f, 0xe0, 0x7a, 0x94, 0xa6}};
EXTERN_C static GUID GUID_WfpInboundCallout4_DEPRECATED = {0xefc879ce, 0x3066, 0x45bb, {0x8a, 0x70, 0x17, 0xfe, 0x29, 0x78, 0x53, 0xc0}};
EXTERN_C static GUID GUID_WfpInboundCallout6_DEPRECATED = {0xd0420299, 0x52d8, 0x4f18, {0xbc, 0x80, 0x47, 0x3a, 0x24, 0x93, 0xf2, 0x69}};
EXTERN_C static GUID GUID_WfpListenCallout4_DEPRECATED = {0x51fa679d, 0x578b, 0x4835, {0xa6, 0x3e, 0xca, 0xd7, 0x68, 0x7f, 0x74, 0x95}};
EXTERN_C static GUID GUID_WfpListenCallout6_DEPRECATED = {0xa02187ca, 0xe655, 0x4adb, {0xa1, 0xf2, 0x47, 0xa2, 0xc9, 0x78, 0xf9, 0xce}};

// enums
typedef enum _ENUM_TYPE_DATA
{
	DataUnknown,
	DataAppRegular,
	DataAppDevice,
	DataAppNetwork,
	DataAppPico, // win10+
	DataAppService,
	DataAppUWP, // win8+
	DataRuleBlocklist,
	DataRuleSystem,
	DataRuleCustom,
	DataRulesConfig,
	DataTypePort,
	DataTypeIp,
	DataTypeHost,
} ENUM_TYPE_DATA;

typedef enum _ENUM_TYPE_XML
{
	XmlApps = 0,
	XmlRules = 1,
	XmlRulesConfig = 2,
	XmlProfileV3 = 3,
	XmlProfileInternalV3 = 4,
} ENUM_TYPE_XML;

typedef enum _ENUM_INFO_DATA
{
	InfoName = 0,
	InfoPath = 1,
	InfoTimestampPtr = 2,
	InfoTimerPtr = 3,
	InfoIconId = 4,
	InfoListviewId = 5,
	InfoIsSilent = 6,
	InfoIsUndeletable = 7,
} ENUM_INFO_DATA;

typedef enum _ENUM_INSTALL_TYPE
{
	InstallDisabled = 0,
	InstallEnabled = 1,
	InstallEnabledTemporary = 2,
} ENUM_INSTALL_TYPE;

// config
#define LANG_MENU 6
#define UID 1984 // if you want to keep a secret, you must also hide it from yourself.

#define XML_PROFILE_VER_3 3
#define XML_PROFILE_VER_CURRENT XML_PROFILE_VER_3

#define XML_PROFILE L"profile.xml"
#define XML_PROFILE_INTERNAL L"profile_internal.xml"

#define LOG_PATH_EXT L"log"
#define LOG_PATH_DEFAULT L"%USERPROFILE%\\" APP_NAME_SHORT L"." LOG_PATH_EXT
#define LOG_VIEWER_DEFAULT L"%SystemRoot%\\notepad.exe"
#define LOG_SIZE_LIMIT_DEFAULT _r_calc_kilobytes2bytes (DWORD, 1)

#define PROC_SYSTEM_PID 4
#define PROC_SYSTEM_NAME L"System"

#define PROC_WAITING_PID 0
#define PROC_WAITING_NAME L"Waiting connections"

#define PATH_NTOSKRNL L"%SystemRoot%\\system32\\ntoskrnl.exe"
#define PATH_SVCHOST L"%SystemRoot%\\system32\\svchost.exe"
#define PATH_SHELL32 L"%SystemRoot%\\system32\\shell32.dll"
#define PATH_WINSTORE L"%SystemRoot%\\system32\\wsreset.exe"

#define WIKI_URL L"https://github.com/henrypp/simplewall/wiki/Rules-editor#rule-syntax-format"
#define WINDOWSSPYBLOCKER_URL L"https://github.com/crazy-max/WindowsSpyBlocker"

#define BOOTTIME_FILTER_NAME L"Boot-time filter"
#define SUBLAYER_WEIGHT_DEFAULT 0xFFFE

#define DIVIDER_COPY L','
#define DIVIDER_CSV L","
#define DIVIDER_APP L"|"
#define DIVIDER_RULE L";"
#define DIVIDER_RULE_RANGE L'-'
#define DIVIDER_TRIM L"\r\n "

#define SZ_TAB L"   "
#define SZ_TAB_CRLF L"\r\n" SZ_TAB
#define SZ_EMPTY L"<empty>"
#define SZ_RULE_INTERNAL_MENU L"*"
#define SZ_RULE_INTERNAL_TITLE L"Internal rule"
#define SZ_RULE_NEW_TITLE L"<new rule>"
#define SZ_UNKNOWN L"unknown"

#define SZ_DIRECTION_REMOTE L"Remote"
#define SZ_DIRECTION_LOCAL L"Local"
#define SZ_STATE_ALLOW L"Allowed"
#define SZ_STATE_BLOCK L"Blocked"
#define SZ_DIRECTION_IN L"Inbound"
#define SZ_DIRECTION_OUT L"Outbound"
#define SZ_DIRECTION_ANY L"Any"
#define SZ_DIRECTION_LOOPBACK L"Loopback"

#define SZ_LOG_TITLE L"Date" DIVIDER_CSV L"User" DIVIDER_CSV L"Path" DIVIDER_CSV L"Address (" SZ_DIRECTION_LOCAL L")" DIVIDER_CSV L"Port (" SZ_DIRECTION_LOCAL L")" DIVIDER_CSV L"Address (" SZ_DIRECTION_REMOTE L")" DIVIDER_CSV L"Port (" SZ_DIRECTION_REMOTE L")" DIVIDER_CSV L"Protocol" DIVIDER_CSV L"Filter name" DIVIDER_CSV L"Filter ID" DIVIDER_CSV L"Direction" DIVIDER_CSV L"State\r\n"
#define SZ_LOG_BODY L"\"%s\"" DIVIDER_CSV L"\"%s\"" DIVIDER_CSV L"\"%s\"" DIVIDER_CSV L"\"%s\"" DIVIDER_CSV L"\"%s\"" DIVIDER_CSV L"\"%s\"" DIVIDER_CSV L"\"%s\"" DIVIDER_CSV L"\"%s\"" DIVIDER_CSV L"\"%s\"" DIVIDER_CSV L"\"#%" TEXT (PRIu64) L"\"" DIVIDER_CSV L"\"%s\"" DIVIDER_CSV L"\"%s\"\r\n"

#define UI_FONT L"Segoe UI"
#define UI_FONT_NOTIFICATION L"Calibri"
#define UI_FONT_DEFAULT UI_FONT L";9;400"
#define BACKUP_HOURS_PERIOD _r_calc_hours2seconds (time_t, 4) // make backup every X hour(s) (default)

#define LEN_IP_MAX 68
#define MAP_CACHE_MAX 1024

#define FILTERS_TIMEOUT 9000
#define TRANSACTION_TIMEOUT 6000
#define NETWORK_TIMEOUT 3500

// notifications
#define NOTIFY_GRADIENT_1 RGB (0, 68, 112)
#define NOTIFY_GRADIENT_2 RGB (7, 111, 95)

#define NOTIFY_TIMER_SAFETY_ID 666
#define NOTIFY_TIMER_SAFETY_TIMEOUT 600

#define NOTIFY_TIMEOUT_DEFAULT 30LL // sec.

#define NOTIFY_LIMIT_POOL_SIZE 128
#define NOTIFY_LIMIT_THREAD_COUNT 2
#define NOTIFY_LIMIT_THREAD_MAX 4

// pugixml document configuration
#define PUGIXML_LOAD_FLAGS (pugi::parse_escapes)
#define PUGIXML_LOAD_ENCODING (pugi::encoding_auto)

#define PUGIXML_SAVE_FLAGS (pugi::format_indent | pugi::format_write_bom)
#define PUGIXML_SAVE_ENCODING (pugi::encoding_wchar)

// default colors
#define LISTVIEW_COLOR_TIMER RGB(255, 190, 142)
#define LISTVIEW_COLOR_INVALID RGB (255, 125, 148)
#define LISTVIEW_COLOR_SPECIAL RGB (255, 255, 170)
#define LISTVIEW_COLOR_SILENT RGB (201, 201, 201)
#define LISTVIEW_COLOR_SIGNED RGB (175, 228, 163)
#define LISTVIEW_COLOR_PICO RGB (51, 153, 255)
#define LISTVIEW_COLOR_SYSTEM RGB(151, 196, 251)
#define LISTVIEW_COLOR_SERVICE RGB (207, 189, 255)
#define LISTVIEW_COLOR_PACKAGE RGB(134, 227, 227)
#define LISTVIEW_COLOR_CONNECTION RGB(255, 168, 242)

// filter weights
#define FILTER_WEIGHT_HIGHEST_IMPORTANT 0x0F
#define FILTER_WEIGHT_HIGHEST 0x0E
#define FILTER_WEIGHT_BLOCKLIST 0x0D
#define FILTER_WEIGHT_CUSTOM_BLOCK 0x0C
#define FILTER_WEIGHT_CUSTOM 0x0B
#define FILTER_WEIGHT_SYSTEM 0x0A
#define FILTER_WEIGHT_APPLICATION 0x09
#define FILTER_WEIGHT_LOWEST 0x08

// memory limitation for 1 rule
#define RULE_NAME_CCH_MAX 64
#define RULE_RULE_CCH_MAX 256

typedef std::vector<PR_OBJECT> OBJECTS_VEC;
typedef std::vector<HANDLE> THREADS_VEC;
typedef std::vector<GUID> GUIDS_VEC;
typedef std::unordered_map<SIZE_T, PR_OBJECT> OBJECTS_MAP;
typedef std::unordered_map<SIZE_T, ENUM_TYPE_DATA> TYPES_MAP;
typedef std::unordered_map<SIZE_T, BOOLEAN> HASHER_MAP;

typedef struct tagSTATIC_DATA
{
	WCHAR profile_path[MAX_PATH] = {0};
	WCHAR profile_path_backup[MAX_PATH] = {0};
	WCHAR profile_internal_path[MAX_PATH] = {0};

	WCHAR windows_dir[MAX_PATH] = {0};

	WCHAR search_string[128] = {0};

	PSID pbuiltin_current_sid = NULL;
	PSID pbuiltin_world_sid = NULL;
	PSID pbuiltin_localservice_sid = NULL;
	PSID pbuiltin_admins_sid = NULL;
	PSID pbuiltin_netops_sid = NULL;

	PSID pservice_mpssvc_sid = NULL;
	PSID pservice_nlasvc_sid = NULL;
	PSID pservice_policyagent_sid = NULL;
	PSID pservice_rpcss_sid = NULL;
	PSID pservice_wdiservicehost_sid = NULL;

	HIMAGELIST himg_toolbar = NULL;
	HIMAGELIST himg_rules_small = NULL;
	HIMAGELIST himg_rules_large = NULL;

	HBITMAP hbmp_enable = NULL;
	HBITMAP hbmp_disable = NULL;
	HBITMAP hbmp_allow = NULL;
	HBITMAP hbmp_block = NULL;
	HBITMAP hbmp_cross = NULL;
	HBITMAP hbmp_rules = NULL;
	HBITMAP hbmp_checked = NULL;
	HBITMAP hbmp_unchecked = NULL;

	HANDLE hlogfile = NULL;
	HANDLE hnetevent = NULL;
	HANDLE done_evt = NULL;
	HANDLE htimer = NULL;
	HFONT hfont = NULL;
	HICON hicon_large = NULL;
	HICON hicon_small = NULL;
	HICON hicon_package = NULL;
	HWND hnotification = NULL;
	HWND hrebar = NULL;
	HWND hfind = NULL;

	time_t profile_internal_timestamp = 0;

	SIZE_T ntoskrnl_hash = 0;
	SIZE_T svchost_hash = 0;
	SIZE_T my_hash = 0;

	SIZE_T wd_length = 0;

	INT icon_id = 0;
	INT icon_service_id = 0;
	INT icon_uwp_id = 0;

	BOOLEAN is_notifytimeout = FALSE;
	BOOLEAN is_notifymouse = FALSE;
	BOOLEAN is_neteventset = FALSE;
	BOOLEAN is_filterstemporary = FALSE;
} STATIC_DATA, *PSTATIC_DATA;

typedef struct tagITEM_LOG
{
	LPWSTR path;
	LPWSTR provider_name;
	LPWSTR filter_name;
	LPWSTR username;

	HICON hicon;

	time_t timestamp;

	SIZE_T app_hash;

	ADDRESS_FAMILY af;

	UINT64 filter_id;

	FWP_DIRECTION direction;

	union
	{
		IN_ADDR remote_addr;
		IN6_ADDR remote_addr6;
	};

	union
	{
		IN_ADDR local_addr;
		IN6_ADDR local_addr6;
	};

	UINT16 remote_port;
	UINT16 local_port;

	UINT8 protocol;

	BOOLEAN is_allow;
	BOOLEAN is_loopback;
	BOOLEAN is_blocklist;
	BOOLEAN is_custom;
	BOOLEAN is_system;
	BOOLEAN is_myprovider;
} ITEM_LOG, *PITEM_LOG;

typedef struct tagITEM_APP
{
	GUIDS_VEC guids;

	LPWSTR display_name = NULL;
	LPWSTR original_path = NULL;
	LPWSTR real_path = NULL;

	PITEM_LOG pnotification = NULL;

	HANDLE htimer = NULL;

	time_t timestamp = 0;
	time_t timer = 0;
	time_t last_notify = 0;

	INT icon_id = 0;

	ENUM_TYPE_DATA type = DataUnknown;

	UINT8 profile = 0; // ffu!

	BOOLEAN is_enabled = FALSE;
	BOOLEAN is_haveerrors = FALSE;
	BOOLEAN is_system = FALSE;
	BOOLEAN is_silent = FALSE;
	BOOLEAN is_signed = FALSE;
	BOOLEAN is_undeletable = FALSE;
} ITEM_APP, *PITEM_APP;

typedef struct tagITEM_APP_HELPER
{
	time_t timestamp;

	ENUM_TYPE_DATA type;

	LPWSTR display_name;
	LPWSTR real_path;
	LPWSTR internal_name;

	PVOID pdata; // service - PSECURITY_DESCRIPTOR / uwp - PSID (win8+)
} ITEM_APP_HELPER, *PITEM_APP_HELPER;

typedef struct tagITEM_RULE
{
	HASHER_MAP apps;

	GUIDS_VEC guids;

	LPWSTR pname = NULL;
	LPWSTR prule_remote = NULL;
	LPWSTR prule_local = NULL;

	ADDRESS_FAMILY af = AF_UNSPEC;

	FWP_DIRECTION direction = FWP_DIRECTION_OUTBOUND;

	UINT8 profile = 0; // ffu!

	UINT8 weight = 0;
	UINT8 protocol = 0;

	ENUM_TYPE_DATA type = DataUnknown;

	BOOLEAN is_haveerrors = FALSE;
	BOOLEAN is_forservices = FALSE;
	BOOLEAN is_readonly = FALSE;
	BOOLEAN is_enabled = FALSE;
	BOOLEAN is_enabled_default = FALSE;
	BOOLEAN is_block = FALSE;
} ITEM_RULE, *PITEM_RULE;

typedef struct tagITEM_RULE_CONFIG
{
	LPWSTR pname;
	LPWSTR papps;

	BOOLEAN is_enabled;
} ITEM_RULE_CONFIG, *PITEM_RULE_CONFIG;

typedef struct tagITEM_NETWORK
{
	LPWSTR path;
	SIZE_T app_hash;
	ADDRESS_FAMILY af;
	FWP_DIRECTION direction;

	union
	{
		IN_ADDR remote_addr;
		IN6_ADDR remote_addr6;
	};

	union
	{
		IN_ADDR local_addr;
		IN6_ADDR local_addr6;
	};

	DWORD state;

	INT icon_id;

	UINT16 remote_port;
	UINT16 local_port;

	UINT8 protocol;

	BOOLEAN is_connection;
} ITEM_NETWORK, *PITEM_NETWORK;

typedef struct tagITEM_STATUS
{
	SIZE_T apps_count;
	SIZE_T apps_timer_count;
	SIZE_T apps_unused_count;
	SIZE_T rules_count;
	SIZE_T rules_global_count;
	SIZE_T rules_predefined_count;
	SIZE_T rules_user_count;
} ITEM_STATUS, *PITEM_STATUS;

typedef struct tagITEM_CONTEXT
{
	HWND hwnd;
	INT listview_id;
	INT item_id;
	BOOLEAN is_install;
} ITEM_CONTEXT, *PITEM_CONTEXT;

typedef struct tagITEM_COLOR
{
	LPWSTR pcfg_name;
	LPWSTR pcfg_value;
	SIZE_T clr_hash;
	UINT locale_id;
	COLORREF default_clr;
	COLORREF new_clr;
	BOOLEAN is_enabled;
} ITEM_COLOR, *PITEM_COLOR;

typedef struct tagITEM_ADDRESS
{
	WCHAR host[NI_MAXHOST];

	FWP_V4_ADDR_AND_MASK* paddr4;
	FWP_V6_ADDR_AND_MASK* paddr6;

	FWP_RANGE* prange;

	ENUM_TYPE_DATA type;

	NET_ADDRESS_FORMAT format;

	UINT16 port;

	BOOLEAN is_range;
} ITEM_ADDRESS, *PITEM_ADDRESS;

typedef struct tagITEM_LIST_HEAD
{
	SLIST_HEADER ListHead;

	volatile LONG item_count;
	volatile LONG thread_count;
} ITEM_LIST_HEAD, *PITEM_LIST_HEAD;

typedef struct _ITEM_LOG_LISTENTRY
{
	SLIST_ENTRY ListEntry;

#ifndef _WIN64
	ULONG_PTR Reserved = 0;
#endif // _WIN64

	PITEM_LOG Body = NULL;

	//SLIST_ENTRY ListEntry;
	//QUAD_PTR Body;
} ITEM_LOG_LISTENTRY, *PITEM_LOG_LISTENTRY;

C_ASSERT (FIELD_OFFSET (ITEM_LOG_LISTENTRY, ListEntry) == 0x00);
C_ASSERT (FIELD_OFFSET (ITEM_LOG_LISTENTRY, Body) == MEMORY_ALLOCATION_ALIGNMENT);

typedef std::unordered_map<SIZE_T, PITEM_APP_HELPER> OBJECTS_APP_HELPER;
typedef std::unordered_map<SIZE_T, PITEM_RULE_CONFIG> OBJECTS_RULE_CONFIG;
typedef std::unordered_map<SIZE_T, PITEM_NETWORK> OBJECTS_NETWORK;
typedef std::vector<PITEM_LOG> OBJECTS_LOG;

