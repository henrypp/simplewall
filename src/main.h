// simplewall
// Copyright (c) 2016-2020 Henry++

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
#pragma comment(lib, "windowscodecs.lib")

// guids
DEFINE_GUID (GUID_WfpProvider, 0xb0d553e2, 0xc6a0, 0x4a9a, 0xae, 0xb8, 0xc7, 0x52, 0x48, 0x3e, 0xd6, 0x2f);
DEFINE_GUID (GUID_WfpSublayer, 0x9fee6f59, 0xb951, 0x4f9a, 0xb5, 0x2f, 0x13, 0x3d, 0xcf, 0x7a, 0x42, 0x79);

// deprecated and not used, but need for compatibility
DEFINE_GUID (GUID_WfpOutboundCallout4_DEPRECATED, 0xf1251f1a, 0xab09, 0x4ce7, 0xba, 0xe3, 0x6c, 0xcc, 0xce, 0xf2, 0xc8, 0xca);
DEFINE_GUID (GUID_WfpOutboundCallout6_DEPRECATED, 0xfd497f2e, 0x46f5, 0x486d, 0xb0, 0xc, 0x3f, 0x7f, 0xe0, 0x7a, 0x94, 0xa6);
DEFINE_GUID (GUID_WfpInboundCallout4_DEPRECATED, 0xefc879ce, 0x3066, 0x45bb, 0x8a, 0x70, 0x17, 0xfe, 0x29, 0x78, 0x53, 0xc0);
DEFINE_GUID (GUID_WfpInboundCallout6_DEPRECATED, 0xd0420299, 0x52d8, 0x4f18, 0xbc, 0x80, 0x47, 0x3a, 0x24, 0x93, 0xf2, 0x69);
DEFINE_GUID (GUID_WfpListenCallout4_DEPRECATED, 0x51fa679d, 0x578b, 0x4835, 0xa6, 0x3e, 0xca, 0xd7, 0x68, 0x7f, 0x74, 0x95);
DEFINE_GUID (GUID_WfpListenCallout6_DEPRECATED, 0xa02187ca, 0xe655, 0x4adb, 0xa1, 0xf2, 0x47, 0xa2, 0xc9, 0x78, 0xf9, 0xce);

typedef ULONG (WINAPI *FWPMNES4)(HANDLE engineHandle, const FWPM_NET_EVENT_SUBSCRIPTION0* subscription, FWPM_NET_EVENT_CALLBACK4 callback, PVOID context, HANDLE* eventsHandle); // win10rs5+
typedef ULONG (WINAPI *FWPMNES3)(HANDLE engineHandle, const FWPM_NET_EVENT_SUBSCRIPTION0* subscription, FWPM_NET_EVENT_CALLBACK3 callback, PVOID context, HANDLE* eventsHandle); // win10rs4+
typedef ULONG (WINAPI *FWPMNES2)(HANDLE engineHandle, const FWPM_NET_EVENT_SUBSCRIPTION0* subscription, FWPM_NET_EVENT_CALLBACK2 callback, PVOID context, HANDLE* eventsHandle); // win10rs1+
typedef ULONG (WINAPI *FWPMNES1)(HANDLE engineHandle, const FWPM_NET_EVENT_SUBSCRIPTION0* subscription, FWPM_NET_EVENT_CALLBACK1 callback, PVOID context, HANDLE* eventsHandle); // win8+
typedef ULONG (WINAPI *FWPMNES0)(HANDLE engineHandle, const FWPM_NET_EVENT_SUBSCRIPTION0* subscription, FWPM_NET_EVENT_CALLBACK0 callback, PVOID context, HANDLE* eventsHandle); // win7+

// enums
typedef enum _ENUM_TYPE_DATA
{
	DataUnknown = 0,
	DataAppRegular,
	DataAppDevice,
	DataAppNetwork,
	DataAppPico, // win10+
	DataAppService,
	DataAppUWP, // win8+
	DataRuleBlocklist,
	DataRuleSystem,
	DataRuleUser,
	DataRulesConfig,
	DataTypePort,
	DataTypeIp,
	DataTypeHost,
	DataFilterGeneral,
} ENUM_TYPE_DATA;

typedef enum _ENUM_TYPE_XML
{
	XmlProfileV3 = 3,
	XmlProfileInternalV3,
} ENUM_TYPE_XML;

typedef enum _ENUM_INFO_DATA
{
	InfoPath = 1,
	InfoName,
	InfoTimestampPtr,
	InfoTimerPtr,
	InfoIconId,
	InfoListviewId,
	InfoIsEnabled,
	InfoIsSilent,
	InfoIsTimerSet,
	InfoIsUndeletable,
} ENUM_INFO_DATA;

typedef enum _ENUM_INSTALL_TYPE
{
	InstallDisabled = 0,
	InstallEnabled,
	InstallEnabledTemporary,
} ENUM_INSTALL_TYPE;

// config
#define LANG_MENU 5
#define UID 1984 // if you want to keep a secret, you must also hide it from yourself.

#define XML_PROFILE_VER_3 3
#define XML_PROFILE_VER_CURRENT XML_PROFILE_VER_3

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

#define PATH_NTOSKRNL L"%SystemRoot%\\system32\\ntoskrnl.exe"
#define PATH_SVCHOST L"%SystemRoot%\\system32\\svchost.exe"
#define PATH_SHELL32 L"%SystemRoot%\\system32\\shell32.dll"
#define PATH_WINSTORE L"%SystemRoot%\\system32\\wsreset.exe"

#define WINDOWSSPYBLOCKER_URL L"https://github.com/crazy-max/WindowsSpyBlocker"

#define SUBLAYER_WEIGHT_DEFAULT 0xFFFE

#define FILTER_NAME_ICMP_ERROR L"BlockIcmpError"
#define FILTER_NAME_TCP_RST_ONCLOSE L"BlockTcpRstOnClose"
#define FILTER_NAME_BLOCK_CONNECTION L"BlockConnection"
#define FILTER_NAME_BLOCK_CONNECTION_REDIRECT L"BlockConnectionRedirect"
#define FILTER_NAME_BLOCK_RECVACCEPT L"BlockRecvAccept"
#define FILTER_NAME_BOOTTIME L"BlockBoottime"

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

#define UI_FONT L"Segoe UI"
#define BACKUP_HOURS_PERIOD _r_calc_hours2seconds (4) // make backup every X hour(s) (default)
#define UI_STATUSBAR_PARTS_COUNT 3

#define LEN_IP_MAX 68
#define MAP_CACHE_MAX 500 // half of thousand limit for unordered_map

#define FILTERS_TIMEOUT 9000
#define TRANSACTION_TIMEOUT 6000
#define NETWORK_TIMEOUT 3500

// notifications
#define NOTIFY_GRADIENT_1 RGB (0, 68, 112)
#define NOTIFY_GRADIENT_2 RGB (7, 111, 95)

#define NOTIFY_TIMER_SAFETY_ID 666
#define NOTIFY_TIMER_SAFETY_TIMEOUT 600

#define NOTIFY_TIMEOUT_DEFAULT 30 // sec.

// default colors
#define LISTVIEW_COLOR_TIMER RGB(255, 190, 142)
#define LISTVIEW_COLOR_INVALID RGB (255, 125, 148)
#define LISTVIEW_COLOR_SPECIAL RGB (255, 255, 170)
#define LISTVIEW_COLOR_SILENT RGB (201, 201, 201)
#define LISTVIEW_COLOR_SIGNED RGB (175, 228, 163)
#define LISTVIEW_COLOR_PICO RGB (51, 153, 255)
#define LISTVIEW_COLOR_SYSTEM RGB(151, 196, 251)
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

typedef struct tagSTATIC_DATA
{
	WCHAR profile_path[MAX_PATH];
	WCHAR profile_path_backup[MAX_PATH];
	WCHAR profile_internal_path[MAX_PATH];

	WCHAR windows_dir[MAX_PATH];

	WCHAR search_string[128];

	PR_STRING ntoskrnl_path;
	PR_STRING svchost_path;
	PR_STRING winstore_path;

	PSID pbuiltin_current_sid;
	PSID pbuiltin_world_sid;
	PSID pbuiltin_localservice_sid;
	PSID pbuiltin_admins_sid;
	PSID pbuiltin_netops_sid;

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
	HBITMAP hbmp_cross;
	HBITMAP hbmp_rules;

	volatile HANDLE hlogfile;
	volatile HANDLE hnetevent;
	HANDLE done_evt;
	HFONT hfont;
	HICON hicon_large;
	HICON hicon_small;
	HICON hicon_uwp;
	HWND hnotification;
	HWND hrebar;
	HWND hfind;

	LONG64 profile_internal_timestamp;

	SIZE_T ntoskrnl_hash;
	SIZE_T svchost_hash;
	SIZE_T my_hash;

	SIZE_T wd_length;

	INT icon_id;
	INT icon_uwp_id;

	BOOLEAN is_notifytimeout;
	BOOLEAN is_notifymouse;
	BOOLEAN is_neteventset;
	BOOLEAN is_filterstemporary;
} STATIC_DATA, *PSTATIC_DATA;

typedef struct tagITEM_LOG
{
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

	PR_STRING path;
	PR_STRING provider_name;
	PR_STRING filter_name;
	PR_STRING username;

	HICON hicon;

	LONG64 timestamp;

	UINT64 filter_id;

	SIZE_T app_hash;

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

typedef struct tagITEM_APP
{
	PR_ARRAY guids;

	PR_STRING original_path;
	PR_STRING display_name;
	PR_STRING short_name;
	PR_STRING real_path;

	PITEM_LOG pnotification;
	PR_BYTE pbytes; // service - PSECURITY_DESCRIPTOR / uwp - PSID (win8+)

	PTP_TIMER htimer;

	LONG64 timestamp;
	LONG64 timer;
	LONG64 last_notify;

	SIZE_T app_hash;

	INT icon_id;

	ENUM_TYPE_DATA type;

	UINT8 profile; // reserved ffu!

	BOOLEAN is_enabled;
	BOOLEAN is_haveerrors;
	BOOLEAN is_silent;
	BOOLEAN is_signed;
	BOOLEAN is_undeletable;
} ITEM_APP, *PITEM_APP;

typedef struct tagITEM_RULE
{
	PR_HASHTABLE apps;
	PR_ARRAY guids;

	PR_STRING name;
	PR_STRING rule_remote;
	PR_STRING rule_local;

	ADDRESS_FAMILY af;

	FWP_DIRECTION direction;

	UINT8 profile; // reserved ffu!

	UINT8 weight;
	UINT8 protocol;

	ENUM_TYPE_DATA type;

	BOOLEAN is_haveerrors;
	BOOLEAN is_forservices;
	BOOLEAN is_readonly;
	BOOLEAN is_enabled;
	BOOLEAN is_enabled_default;
	BOOLEAN is_block;
} ITEM_RULE, *PITEM_RULE;

typedef struct tagITEM_RULE_CONFIG
{
	PR_STRING name;
	PR_STRING apps;

	BOOLEAN is_enabled;
} ITEM_RULE_CONFIG, *PITEM_RULE_CONFIG;

typedef struct tagITEM_NETWORK
{
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

	PR_STRING path;
	SIZE_T app_hash;
	SIZE_T network_hash;
	ULONG state;
	INT icon_id;
	FWP_DIRECTION direction;
	ENUM_TYPE_DATA type;
	ADDRESS_FAMILY af;
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

	union
	{
		PITEM_RULE ptr_rule;
		PITEM_APP ptr_app;
	};

	union
	{
		struct
		{
			INT listview_id;
			INT item_id;
		};
		BOOLEAN is_install;
	};

	BOOLEAN is_settorules;

} ITEM_CONTEXT, *PITEM_CONTEXT;

typedef struct tagITEM_COLOR
{
	PR_STRING config_name;
	PR_STRING config_value;
	SIZE_T hash;
	COLORREF default_clr;
	COLORREF new_clr;
	UINT locale_id;
	BOOLEAN is_enabled;
} ITEM_COLOR, *PITEM_COLOR;

typedef struct tagITEM_ADDRESS
{
	union
	{
		FWP_V4_ADDR_AND_MASK addr4;
		FWP_V6_ADDR_AND_MASK addr6;
		FWP_RANGE range;
	};

	WCHAR host[NI_MAXHOST];

	ENUM_TYPE_DATA type;

	NET_ADDRESS_FORMAT format;

	UINT16 port;

	BOOLEAN is_range;
} ITEM_ADDRESS, *PITEM_ADDRESS;

typedef struct tagITEM_LOG_LISTENTRY
{
	SLIST_ENTRY list_entry;

#ifndef _WIN64
	ULONG_PTR reserved;
#endif // _WIN64

	PITEM_LOG body;

	//SLIST_ENTRY ListEntry;
	//QUAD_PTR Body;
} ITEM_LOG_LISTENTRY, *PITEM_LOG_LISTENTRY;

C_ASSERT (FIELD_OFFSET (ITEM_LOG_LISTENTRY, list_entry) == 0x00);
C_ASSERT (FIELD_OFFSET (ITEM_LOG_LISTENTRY, body) == MEMORY_ALLOCATION_ALIGNMENT);
