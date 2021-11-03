// simplewall
// Copyright (c) 2016-2021 Henry++

#pragma once

#include "routine.h"

#include "resource.h"
#include "app.h"
#include "global.h"

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

// guids
DEFINE_GUID (GUID_TrayIcon, 0xdab4837e, 0xcb0f, 0x47da, 0x92, 0x22, 0x21, 0x20, 0x74, 0x9f, 0x5c, 0x41);

DEFINE_GUID (GUID_WfpProvider, 0xb0d553e2, 0xc6a0, 0x4a9a, 0xae, 0xb8, 0xc7, 0x52, 0x48, 0x3e, 0xd6, 0x2f);
DEFINE_GUID (GUID_WfpSublayer, 0x9fee6f59, 0xb951, 0x4f9a, 0xb5, 0x2f, 0x13, 0x3d, 0xcf, 0x7a, 0x42, 0x79);

// deprecated and not used, but need for compatibility
DEFINE_GUID (GUID_WfpOutboundCallout4_DEPRECATED, 0xf1251f1a, 0xab09, 0x4ce7, 0xba, 0xe3, 0x6c, 0xcc, 0xce, 0xf2, 0xc8, 0xca);
DEFINE_GUID (GUID_WfpOutboundCallout6_DEPRECATED, 0xfd497f2e, 0x46f5, 0x486d, 0xb0, 0xc, 0x3f, 0x7f, 0xe0, 0x7a, 0x94, 0xa6);
DEFINE_GUID (GUID_WfpInboundCallout4_DEPRECATED, 0xefc879ce, 0x3066, 0x45bb, 0x8a, 0x70, 0x17, 0xfe, 0x29, 0x78, 0x53, 0xc0);
DEFINE_GUID (GUID_WfpInboundCallout6_DEPRECATED, 0xd0420299, 0x52d8, 0x4f18, 0xbc, 0x80, 0x47, 0x3a, 0x24, 0x93, 0xf2, 0x69);
DEFINE_GUID (GUID_WfpListenCallout4_DEPRECATED, 0x51fa679d, 0x578b, 0x4835, 0xa6, 0x3e, 0xca, 0xd7, 0x68, 0x7f, 0x74, 0x95);
DEFINE_GUID (GUID_WfpListenCallout6_DEPRECATED, 0xa02187ca, 0xe655, 0x4adb, 0xa1, 0xf2, 0x47, 0xa2, 0xc9, 0x78, 0xf9, 0xce);

// enums
typedef enum _ENUM_TYPE_DATA
{
	DATA_UNKNOWN = 0,
	DATA_APP_REGULAR,
	DATA_APP_DEVICE,
	DATA_APP_NETWORK,
	DATA_APP_SERVICE,
	DATA_APP_UWP, // win8+
	DATA_APP_PICO, // win10+
	DATA_RULE_BLOCKLIST,
	DATA_RULE_SYSTEM,
	DATA_RULE_SYSTEM_USER,
	DATA_RULE_USER,
	DATA_RULE_CONFIG,
	DATA_TYPE_PORT,
	DATA_TYPE_IP,
	DATA_FILTER_GENERAL,
	DATA_LISTVIEW_CURRENT,
} ENUM_TYPE_DATA;

typedef enum _ENUM_TYPE_XML
{
	XML_PROFILE_V3 = 3,
	XML_PROFILE_INTERNAL_V3 = 4,
} ENUM_TYPE_XML;

typedef enum _ENUM_INSTALL_TYPE
{
	INSTALL_DISABLED = 0,
	INSTALL_ENABLED,
	INSTALL_ENABLED_TEMPORARY,
} ENUM_INSTALL_TYPE;

typedef enum _ENUM_INFO_DATA
{
	INFO_PATH = 1,
	INFO_BYTES_DATA,
	INFO_DISPLAY_NAME,
	INFO_TIMESTAMP_PTR,
	INFO_TIMER_PTR,
	INFO_LISTVIEW_ID,
	INFO_IS_ENABLED,
	INFO_IS_READONLY,
	INFO_IS_SILENT,
	INFO_IS_UNDELETABLE,
} ENUM_INFO_DATA;

typedef enum _ENUM_INFO_DATA2
{
	INFO_ICON_ID,
	INFO_SIGNATURE_STRING, // dereference required
	INFO_VERSION_STRING, // dereference required
} ENUM_INFO_DATA2;

// config
#define LANG_MENU 5
#define UID 1984 // if you want to keep a secret, you must also hide it from yourself.

// v3.0.2: first major update, rule attribute "apps" now separated by "|"
#define XML_PROFILE_VER_3 0x03

// v3.4: added "rules_custom" into internal profile and "os_version" for rule attributes
#define XML_PROFILE_VER_4 0x04

#define XML_PROFILE_VER_CURRENT XML_PROFILE_VER_4

#define XML_PROFILE L"profile.xml"
#define XML_PROFILE_INTERNAL L"profile_internal.xml"

#define LOG_PATH_EXT L"log"
#define LOG_PATH_DEFAULT L"%USERPROFILE%\\" APP_NAME_SHORT L"." LOG_PATH_EXT
#define LOG_VIEWER_DEFAULT L"%SystemRoot%\\notepad.exe"
#define LOG_SIZE_LIMIT_DEFAULT _r_calc_kilobytes2bytes (1)

#define PROC_WAITING_PID 0
#define PROC_WAITING_NAME L"Waiting connections"

#define PROC_SYSTEM_PID 4
#define PROC_SYSTEM_NAME L"System"

#define PATH_NTOSKRNL L"\\ntoskrnl.exe"
#define PATH_SVCHOST L"\\svchost.exe"

#define WINDOWSSPYBLOCKER_URL L"https://github.com/crazy-max/WindowsSpyBlocker"

#define SUBLAYER_WEIGHT_DEFAULT 0xFFFE

#define DIVIDER_COPY L", "
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
#define SZ_LOADING L"Loading..."
#define SZ_MAXTEXT L"Limit reached."

#define SZ_DIRECTION_REMOTE L"Remote"
#define SZ_DIRECTION_LOCAL L"Local"
#define SZ_STATE_ALLOW L"Allowed"
#define SZ_STATE_BLOCK L"Blocked"
#define SZ_DIRECTION_IN L"Inbound"
#define SZ_DIRECTION_OUT L"Outbound"
#define SZ_DIRECTION_ANY L"Any"
#define SZ_DIRECTION_LOOPBACK L"Loopback"

#define SZ_LOG_TITLE L"Date" DIVIDER_CSV L"User" DIVIDER_CSV L"Path" DIVIDER_CSV L"Address (" SZ_DIRECTION_LOCAL L")" DIVIDER_CSV L"Port (" SZ_DIRECTION_LOCAL L")" DIVIDER_CSV L"Address (" SZ_DIRECTION_REMOTE L")" DIVIDER_CSV L"Port (" SZ_DIRECTION_REMOTE L")" DIVIDER_CSV L"Protocol" DIVIDER_CSV L"Filter name" DIVIDER_CSV L"Filter ID" DIVIDER_CSV L"Direction" DIVIDER_CSV L"State\r\n"
#define SZ_LOG_BODY L"\"%s\"" DIVIDER_CSV L"\"%s\"" DIVIDER_CSV L"\"%s\"" DIVIDER_CSV L"\"%s\"" DIVIDER_CSV L"\"%s\"" DIVIDER_CSV L"\"%s\"" DIVIDER_CSV L"\"%s\"" DIVIDER_CSV L"\"%s\"" DIVIDER_CSV L"\"%s\\%s\"" DIVIDER_CSV L"\"#%" TEXT (PRIu64) L"\"" DIVIDER_CSV L"\"%s\"" DIVIDER_CSV L"\"%s\"\r\n"

#define BACKUP_HOURS_PERIOD _r_calc_hours2seconds (4) // make backup every X hour(s) (default)

#define LEN_IP_MAX 68
#define MAP_CACHE_MAX 900 // limit for caching hashtable

#define TRANSACTION_TIMEOUT 9000
#define NETWORK_TIMEOUT 3500

// controls
#define LV_HIDDEN_GROUP_ID 17
#define STATUSBAR_PARTS_COUNT 3

#define REBAR_TOOLBAR_ID 0
#define REBAR_SEARCH_ID 1

// notifications
#define NOTIFY_TIMER_SAFETY_ID 666
#define NOTIFY_TIMER_SAFETY_TIMEOUT 900

#define NOTIFY_TIMEOUT_DEFAULT 60 // sec.

// default colors
#define LV_COLOR_TIMER RGB(255, 190, 142)
#define LV_COLOR_INVALID RGB (255, 125, 148)
#define LV_COLOR_SPECIAL RGB (255, 255, 170)
#define LV_COLOR_SIGNED RGB (175, 228, 163)
#define LV_COLOR_PICO RGB (51, 153, 255)
#define LV_COLOR_SYSTEM RGB(151, 196, 251)
#define LV_COLOR_CONNECTION RGB(255, 168, 242)

// filter names
#define FW_NAME_BLOCK_CONNECTION L"BlockConnection"
#define FW_NAME_BLOCK_REDIRECT L"BlockRedirect"
#define FW_NAME_BLOCK_RECVACCEPT L"BlockRecvAccept"
#define FW_NAME_ICMP_ERROR L"BlockIcmpError"
#define FW_NAME_TCP_RST_ONCLOSE L"BlockTcpRstOnClose"
#define FW_NAME_BOOTTIME L"BlockBoottime"

// filter weights
#define FW_WEIGHT_HIGHEST_IMPORTANT 0x0f
#define FW_WEIGHT_HIGHEST 0x0e
#define FW_WEIGHT_RULE_BLOCKLIST 0x0d
#define FW_WEIGHT_RULE_USER_BLOCK 0x0c
#define FW_WEIGHT_RULE_USER 0x0b
#define FW_WEIGHT_RULE_SYSTEM 0x0a
#define FW_WEIGHT_APP 0x09
#define FW_WEIGHT_LOWEST 0x08

// memory limitation for 1 rule
#define RULE_NAME_CCH_MAX 64
#define RULE_RULE_CCH_MAX 256

typedef struct _STATIC_DATA
{
	WCHAR windows_dir_buffer[MAX_PATH];
	R_STRINGREF windows_dir;

	PR_STRING search_string;

	PSID pbuiltin_current_sid;
	PSID pbuiltin_netops_sid;
	PSID pbuiltin_admins_sid;

	PSID pservice_mpssvc_sid;
	PSID pservice_nlasvc_sid;
	PSID pservice_policyagent_sid;
	PSID pservice_rpcss_sid;
	PSID pservice_wdiservicehost_sid;

	HIMAGELIST himg_toolbar;
	HIMAGELIST himg_rules_small;
	HIMAGELIST himg_rules_large;

	HBITMAP hbmp_enable;
	HBITMAP hbmp_disable;
	HBITMAP hbmp_allow;
	HBITMAP hbmp_block;

	volatile HANDLE hlogfile;
	volatile HANDLE hnetevent;
	volatile HWND hnotification;

	HFONT hfont;
	HWND hrebar;
	HWND htoolbar;
	HWND hsearchbar;

	ULONG_PTR color_timer;
	ULONG_PTR color_invalid;
	ULONG_PTR color_special;
	ULONG_PTR color_signed;
	ULONG_PTR color_pico;
	ULONG_PTR color_system;
	ULONG_PTR color_network;

	BOOLEAN is_notifytimeout;
	BOOLEAN is_notifymouse;
	BOOLEAN is_neteventenabled;
	BOOLEAN is_neteventset;
	BOOLEAN is_filterstemporary;
} STATIC_DATA, *PSTATIC_DATA;

typedef struct _PROFILE_DATA
{
	PR_STRING profile_path;
	PR_STRING profile_path_backup;
	PR_STRING profile_internal_path;

	PR_STRING my_path;
	PR_STRING ntoskrnl_path;
	PR_STRING svchost_path;
	PR_STRING system_path;

	LONG64 profile_internal_timestamp;

	ULONG_PTR ntoskrnl_hash;
	ULONG_PTR svchost_hash;
	ULONG_PTR my_hash;
} PROFILE_DATA, *PPROFILE_DATA;

typedef struct _ITEM_LOG
{
	union
	{
		IN_ADDR remote_addr;
		IN6_ADDR remote_addr6;
	} DUMMYUNIONNAME;

	union
	{
		IN_ADDR local_addr;
		IN6_ADDR local_addr6;
	} DUMMYUNIONNAME2;

	PR_STRING path;
	PR_STRING provider_name;
	PR_STRING filter_name;
	PR_STRING username;
	PR_STRING local_addr_str;
	PR_STRING local_host_str;
	PR_STRING remote_addr_str;
	PR_STRING remote_host_str;

	LONG64 timestamp;

	UINT64 filter_id;

	ULONG_PTR app_hash;

	FWP_DIRECTION direction;

	ADDRESS_FAMILY af;

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

typedef struct _ITEM_APP
{
	PR_ARRAY guids;

	PR_STRING original_path;
	PR_STRING display_name;
	PR_STRING short_name;
	PR_STRING real_path;

	PITEM_LOG notification;
	PR_BYTE bytes; // service - PSECURITY_DESCRIPTOR / uwp - PSID (win8+)

	PTP_TIMER htimer;

	LONG64 timestamp;
	LONG64 timer;
	LONG64 last_notify;

	ULONG_PTR app_hash;

	ENUM_TYPE_DATA type;

	UINT8 profile; // reserved ffu!

	struct
	{
		ULONG is_enabled : 1;
		ULONG is_haveerrors : 1;
		ULONG is_silent : 1;
		ULONG is_undeletable : 1;
		ULONG spare_bits : 28;
	} DUMMYSTRUCTNAME;
} ITEM_APP, *PITEM_APP;

typedef struct _ITEM_APP_INFO
{
	PR_STRING path;

	PR_STRING signature_info;
	PR_STRING version_info;

	volatile LONG large_icon_id;
	volatile LONG lock;

	ULONG_PTR app_hash;

	ENUM_TYPE_DATA type;

	INT listview_id;
} ITEM_APP_INFO, *PITEM_APP_INFO;

typedef struct _ITEM_FILTER_CONFIG
{
	FWP_DIRECTION direction;
	UINT8 protocol;
	ADDRESS_FAMILY af;
} ITEM_FILTER_CONFIG, *PITEM_FILTER_CONFIG;

typedef struct _ITEM_RULE
{
	PR_HASHTABLE apps;
	PR_ARRAY guids;

	PR_STRING name;
	PR_STRING rule_remote;
	PR_STRING rule_local;

	struct
	{
		ULONG is_enabled : 1;
		ULONG is_haveerrors : 1;
		ULONG is_forservices : 1;
		ULONG is_readonly : 1;
		ULONG is_enabled_default : 1;
		ULONG spare_bits : 27;
	} DUMMYSTRUCTNAME;

	union
	{
		ITEM_FILTER_CONFIG config;

		struct
		{
			FWP_DIRECTION direction;
			UINT8 protocol;
			ADDRESS_FAMILY af;
		} DUMMYSTRUCTNAME2;
	} DUMMYUNIONNAME;

	ENUM_TYPE_DATA type;
	FWP_ACTION_TYPE action;
	UINT8 profile; // reserved ffu!
	UINT8 weight;
} ITEM_RULE, *PITEM_RULE;

typedef struct _ITEM_RULE_CONFIG
{
	PR_STRING name;
	PR_STRING apps;

	BOOLEAN is_enabled;
} ITEM_RULE_CONFIG, *PITEM_RULE_CONFIG;

typedef struct _ITEM_NETWORK
{
	union
	{
		IN_ADDR remote_addr;
		IN6_ADDR remote_addr6;
	} DUMMYUNIONNAME;

	union
	{
		IN_ADDR local_addr;
		IN6_ADDR local_addr6;
	} DUMMYUNIONNAME2;

	PR_STRING path;
	PR_STRING local_addr_str;
	PR_STRING local_host_str;
	PR_STRING remote_addr_str;
	PR_STRING remote_host_str;
	ULONG_PTR app_hash;
	ULONG state;
	FWP_DIRECTION direction;
	ENUM_TYPE_DATA type;
	ADDRESS_FAMILY af;
	UINT16 remote_port;
	UINT16 local_port;
	UINT8 protocol;
	BOOLEAN is_connection;
} ITEM_NETWORK, *PITEM_NETWORK;

typedef struct _ITEM_STATUS
{
	SIZE_T apps_count;
	SIZE_T apps_timer_count;
	SIZE_T apps_unused_count;
	SIZE_T rules_count;
	SIZE_T rules_global_count;
	SIZE_T rules_predefined_count;
	SIZE_T rules_user_count;
} ITEM_STATUS, *PITEM_STATUS;

typedef struct _ITEM_CONTEXT
{
	HWND hwnd;
	INT listview_id;

	union
	{
		struct
		{
			union
			{
				PITEM_LOG ptr_log;
				PITEM_NETWORK ptr_network;
			} DUMMYUNIONNAME2;

			LPARAM lparam;
		} DUMMYSTRUCTNAME3;

		BOOLEAN is_install;
	} DUMMYUNIONNAME;
} ITEM_CONTEXT, *PITEM_CONTEXT;

typedef struct _ITEM_LISTVIEW_CONTEXT
{
	ULONG_PTR id_code;

	BOOLEAN is_hidden;
} ITEM_LISTVIEW_CONTEXT, *PITEM_LISTVIEW_CONTEXT;

typedef struct _ITEM_COLOR
{
	PR_STRING config_name;
	PR_STRING config_value;
	COLORREF default_clr;
	COLORREF new_clr;
	UINT locale_id;
	BOOLEAN is_enabled;
} ITEM_COLOR, *PITEM_COLOR;

typedef struct _ITEM_ADDRESS
{
	WCHAR range_start[LEN_IP_MAX];
	WCHAR range_end[LEN_IP_MAX];

	union
	{
		FWP_V4_ADDR_AND_MASK addr4;
		FWP_V6_ADDR_AND_MASK addr6;

		struct
		{
			FWP_RANGE range;

			UINT8 addr6_low[FWP_V6_ADDR_SIZE];
			UINT8 addr6_high[FWP_V6_ADDR_SIZE];
		} DUMMYSTRUCTNAME;
	} DUMMYUNIONNAME;

	UINT16 port;

	ENUM_TYPE_DATA type;
	NET_ADDRESS_FORMAT format;

	BOOLEAN is_range;
} ITEM_ADDRESS, *PITEM_ADDRESS;

typedef struct _ITEM_LOG_CALLBACK
{
	union
	{
		ULONG_PTR remote_addr4;
		const FWP_BYTE_ARRAY16 *remote_addr6;
	} DUMMYUNIONNAME;

	union
	{
		ULONG_PTR local_addr4;
		const FWP_BYTE_ARRAY16 *local_addr6;
	} DUMMYUNIONNAME2;

	const FILETIME *timestamp;
	PUINT8 app_id;
	PSID package_id;
	PSID user_id;

	UINT64 filter_id;

	FWP_IP_VERSION version;

	UINT32 flags;
	UINT32 direction;

	UINT16 remote_port;
	UINT16 local_port;
	UINT16 layer_id;
	UINT8 protocol;
	BOOLEAN is_allow;
	BOOLEAN is_loopback;
} ITEM_LOG_CALLBACK, *PITEM_LOG_CALLBACK;
