// simplewall
// Copyright (c) 2016-2018 Henry++

#ifndef __MAIN_H__
#define __MAIN_H__

#include <windows.h>
#include <commctrl.h>
#include <unordered_map>
#include "resource.hpp"
#include "app.hpp"

// config
#define WM_TRAYICON WM_APP + 1
#define LANG_MENU 5
#define UID 1984 // if you want to keep a secret, you must also hide it from yourself.

#define PROC_SYSTEM_PID 4
#define PROC_SYSTEM_NAME L"System"

#define PATH_LOG L"%userprofile%\\" APP_NAME_SHORT L".log"

#define PATH_NTOSKRNL L"%systemroot%\\system32\\ntoskrnl.exe"
#define PATH_SVCHOST L"%systemroot%\\system32\\svchost.exe"
#define PATH_STORE L"%systemroot%\\system32\\wsreset.exe"
#define PATH_SERVICES L"%systemroot%\\system32\\services.msc"

#define XML_APPS L"apps.xml"
#define XML_BLOCKLIST L"blocklist.xml"
#define XML_RULES_CONFIG L"rules_config.xml"
#define XML_RULES_CUSTOM L"rules_custom.xml"
#define XML_RULES_SYSTEM L"rules_system.xml"

#define WIKI_URL L"https://github.com/henrypp/simplewall/wiki/Rules-editor#rule-syntax-format"
#define BLOCKLIST_URL L"https://github.com/henrypp/simplewall/blob/master/bin/blocklist.xml"
#define LISTENS_ISSUE_URL L"https://github.com/henrypp/simplewall/issues/9"

#define SERVICE_SECURITY_DESCRIPTOR L"O:SYG:SYD:(A;; CCRC;;;%s)"

#define SZ_TAB L"   "
#define SZ_EMPTY L"<empty>"
#define SZ_REMOTE L"Remote"
#define SZ_LOCAL L"Local"
#define SZ_DIRECTION_IN L"IN"
#define SZ_DIRECTION_OUT L"OUT"
#define SZ_DIRECTION_LOOPBACK L"-Loopback"

#define RULE_DELIMETER L";"
#define UI_FONT L"Segoe UI"
#define UI_FONT_DEFAULT UI_FONT L";9;400"
#define BACKUP_HOURS_PERIOD 1 // make backup every 1 hour (default)

#define LEN_IP_MAX 68
#define LEN_HOST_MAX 512

#define TIMER_DEFAULT 1
#define TIMER_LOG_CALLBACK 4000

// notifications
#define NOTIFY_CLASS_DLG L"NotificationDlg"

#define NOTIFY_WIDTH 368
#define NOTIFY_HEIGHT 248
#define NOTIFY_BTN_WIDTH 110

#define NOTIFY_TIMER_POPUP_ID 1001
#define NOTIFY_TIMER_TIMEOUT_ID 2002

#define NOTIFY_TIMER_POPUP 350

#define NOTIFY_TIMER_DEFAULT 6 // sec.
#define NOTIFY_TIMEOUT 60 // sec.
#define NOTIFY_TIMEOUT_MINIMUM 8 // sec.
#define NOTIFY_LIMIT_SIZE 10 //limit notifications pool size

#define NOTIFY_SOUND_DEFAULT L"MailBeep"
#define NOTIFY_BORDER_COLOR RGB(255,98,98)

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
#define FILTER_WEIGHT_CUSTOM 0xC
#define FILTER_WEIGHT_SYSTEM 0xB
#define FILTER_WEIGHT_APPLICATION 0xA
#define FILTER_WEIGHT_LOWEST 0x9

// memory limitation
#define RULE_NAME_CCH_MAX 64
#define RULE_RULE_CCH_MAX 256

// libs
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "dnsapi.lib")
#pragma comment(lib, "fwpuclnt.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "wintrust.lib")

// guid
extern "C" {
	static const GUID GUID_WfpProvider =
	{0xb0d553e2, 0xc6a0, 0x4a9a, {0xae, 0xb8, 0xc7, 0x52, 0x48, 0x3e, 0xd6, 0x2f}};

	static const GUID GUID_WfpSublayer =
	{0x9fee6f59, 0xb951, 0x4f9a, {0xb5, 0x2f, 0x13, 0x3d, 0xcf, 0x7a, 0x42, 0x79}};

	// deprecated and not used, but need for remove
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
enum EnumRuleType
{
	TypeUnknown = 0,
	TypeIp = 2,
	TypeHost = 1,
	TypePort = 4,
};

enum EnumMode
{
	ModeWhitelist = 0,
	ModeBlacklist = 1,
};

enum EnumAppType
{
	AppRegular = 0,
	AppDevice = 1,
	AppNetwork = 2,
	AppService = 3,
	AppStore = 4, // win8+
	AppPico = 5 // win10+
};

enum EnumNotifyCommand
{
	CmdAllow = 0,
	CmdBlock = 1,
};

struct STATIC_DATA
{
	PSID psid = nullptr;
	LPGUID psession = nullptr;

	bool is_securityinfoset = false;
	bool is_popuperrors = false;
	bool is_notifytimeout = false;
	bool is_notifymouse = false;
	bool is_nocheckboxnotify = false;
	bool is_wsainit = false;
	bool is_ihaveinternetaccess = false;

	HIMAGELIST himg = nullptr;

	size_t icon_id = 0;
	size_t icon_package_id = 0;
	size_t icon_service_id = 0;
	HICON hicon_large = nullptr;
	HICON hicon_small = nullptr;
	HICON hicon_package = nullptr;
	HICON hicon_service_small = nullptr;
	HBITMAP hbitmap_package_small = nullptr;
	HBITMAP hbitmap_service_small = nullptr;

	HFONT hfont = nullptr;

	HANDLE hthread = nullptr;
	HANDLE hengine = nullptr;
	HANDLE hevent = nullptr;
	HANDLE hlog = nullptr;
	HANDLE hpackages = nullptr;

	HANDLE stop_evt = nullptr;
	HANDLE finish_evt = nullptr;
	HANDLE log_evt = nullptr;

	HANDLE htimer = nullptr;
	time_t timer_low = 0;

	time_t blocklist_timestamp = 0;

	WCHAR title[128] = {0};
	WCHAR notify_snd_path[MAX_PATH] = {0};

	WCHAR apps_path[MAX_PATH] = {0};
	WCHAR rules_custom_path[MAX_PATH] = {0};
	WCHAR rules_config_path[MAX_PATH] = {0};

	WCHAR apps_path_backup[MAX_PATH] = {0};
	WCHAR rules_custom_path_backup[MAX_PATH] = {0};
	WCHAR rules_config_path_backup[MAX_PATH] = {0};

	HWND hnotification = nullptr;

	HWND hfind = nullptr;
	WCHAR search_string[128] = {0};

	WCHAR windows_dir[MAX_PATH] = {0};
	size_t wd_length = 0;

	WCHAR tmp1_dir[MAX_PATH] = {0};
	size_t tmp1_length = 0;

	size_t ntoskrnl_hash = 0;
	size_t myhash = 0;
};

typedef struct _ITEM_APP
{
	__time64_t timestamp = 0;

	size_t icon_id = 0;

	bool is_haveerrors = false;

	bool is_enabled = false;
	bool is_system = false;
	bool is_silent = false;
	bool is_signed = false;
	bool is_temp = false;

	EnumAppType type = AppRegular;

	LPCWSTR description = nullptr;
	LPCWSTR signer = nullptr;

	WCHAR display_name[MAX_PATH] = {0};
	WCHAR original_path[MAX_PATH] = {0};
	WCHAR real_path[MAX_PATH] = {0};

	union
	{
		PSECURITY_DESCRIPTOR psd = nullptr; // service app
		PSID psid; // store app (win8+)
	};
} ITEM_APP, *PITEM_APP;

typedef struct _ITEM_RULE
{
	bool is_enabled = false;
	bool is_block = false;
	bool is_haveerrors = false;

	FWP_DIRECTION dir = FWP_DIRECTION_OUTBOUND;
	EnumRuleType type = TypeUnknown;

	UINT8 protocol = 0;
	ADDRESS_FAMILY version = AF_UNSPEC;

	LPWSTR pname = nullptr;
	LPWSTR prule = nullptr;

	std::unordered_map<size_t, bool> apps;
} ITEM_RULE, *PITEM_RULE;

typedef struct _ITEM_LOG
{
	bool is_loopback = false;
	bool is_blocklist = false;
	bool is_myprovider = false;

	size_t hash = 0;

	time_t date = 0;

	ADDRESS_FAMILY af = 0;

	UINT8 protocol = 0;

	UINT16 remote_port = 0;
	UINT16 local_port = 0;

	UINT64 filter_id = 0;

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

	FWP_DIRECTION direction = FWP_DIRECTION_OUTBOUND;

	WCHAR remote_fmt[192] = {0};
	WCHAR local_fmt[192] = {0};

	WCHAR username[192] = {0};

	WCHAR provider_name[MAX_PATH] = {0};
	WCHAR filter_name[MAX_PATH] = {0};

	WCHAR path[MAX_PATH] = {0};
} ITEM_LOG, *PITEM_LOG;

typedef struct _ITEM_LIST_ENTRY
{
	SLIST_ENTRY ListEntry;
	ULONG_PTR Body;
} ITEM_LIST_ENTRY, *PITEM_LIST_ENTRY;

typedef struct _ITEM_ADD
{
	size_t hash = 0;

	HBITMAP hbmp = nullptr;

	WCHAR sid[MAX_PATH] = {0};
	WCHAR display_name[MAX_PATH] = {0};
	WCHAR service_name[MAX_PATH] = {0};
	WCHAR real_path[MAX_PATH] = {0};

	union
	{
		PSID psid = nullptr;
		PSECURITY_DESCRIPTOR psd;
	};
} ITEM_ADD, *PITEM_ADD;

typedef struct _ITEM_COLOR
{
	bool is_enabled = false;

	size_t hash = 0;

	UINT locale_id = 0;

	COLORREF default_clr = 0;
	COLORREF clr = 0;

	LPWSTR config_name = nullptr;
	LPWSTR config_value = nullptr;
} ITEM_COLOR, *PITEM_COLOR;

typedef struct _ITEM_PROTOCOL
{
	UINT8 id = 0;
	LPWSTR name = nullptr;
} ITEM_PROTOCOL, *PITEM_PROTOCOL;

typedef struct _ITEM_ADDRESS
{
	bool is_range = false;

	EnumRuleType type = TypeUnknown;

	NET_ADDRESS_FORMAT format;

	UINT16 port = 0;

	FWP_V4_ADDR_AND_MASK* paddr4 = nullptr;
	FWP_V6_ADDR_AND_MASK* paddr6 = nullptr;

	FWP_RANGE* prange = nullptr;

	WCHAR host[LEN_HOST_MAX] = {0};
} ITEM_ADDRESS, *PITEM_ADDRESS;

// dropped events callback subscription (win7+)
#ifndef FWP_DIRECTION_IN
#define FWP_DIRECTION_IN 0x00003900L
#endif

#ifndef FWP_DIRECTION_OUT
#define FWP_DIRECTION_OUT 0x00003901L
#endif

typedef DWORD (WINAPI *FWPMNES2) (HANDLE, const FWPM_NET_EVENT_SUBSCRIPTION0*, FWPM_NET_EVENT_CALLBACK2, LPVOID, LPHANDLE); // subscribe (win10+)
typedef DWORD (WINAPI *FWPMNES1) (HANDLE, const FWPM_NET_EVENT_SUBSCRIPTION0*, FWPM_NET_EVENT_CALLBACK1, LPVOID, LPHANDLE); // subscribe (win8+)
typedef DWORD (WINAPI *FWPMNES0) (HANDLE, const FWPM_NET_EVENT_SUBSCRIPTION0*, FWPM_NET_EVENT_CALLBACK0, LPVOID, LPHANDLE); // subscribe (win7+)

typedef DWORD (WINAPI *FWPMNEU) (HANDLE, HANDLE); // unsubcribe (all)

#endif // __MAIN_H__
