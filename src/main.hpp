// simplewall
// Copyright (c) 2016, 2017 Henry++

#ifndef __MAIN_H__
#define __MAIN_H__

#include <windows.h>
#include <commctrl.h>
#include <unordered_map>
#include "resource.hpp"
#include "app.hpp"

// config
#define WM_TRAYICON WM_APP + 1
#define UID 1984 // if you want to keep a secret, you must also hide it from yourself.

#define PROC_SYSTEM_PID 4
#define PROC_SYSTEM_NAME L"System"

#define PATH_LOG L"%userprofile%\\" APP_NAME_SHORT L".log"

#define PATH_NTOSKRNL L"%systemroot%\\system32\\ntoskrnl.exe"
#define PATH_SVCHOST L"%systemroot%\\system32\\svchost.exe"
#define PATH_STORE L"%systemroot%\\system32\\wsreset.exe"

#define XML_APPS L"apps.xml"
#define XML_BLOCKLIST L"blocklist.xml"
#define XML_RULES_CONFIG L"rules_config.xml"
#define XML_RULES_CUSTOM L"rules_custom.xml"
#define XML_RULES_SYSTEM L"rules_system.xml"

#define LANG_MENU 5
#define NA_TEXT L"<empty>"
#define TAB_SPACE L"   "
#define ERR_FORMAT L"%s() failed with error code 0x%.8lx (%s)"
#define RULE_DELIMETER L";"
#define UI_FONT_DEFAULT L"Segoe UI Light;10;300"

#define LEN_IP_MAX 68
#define LEN_HOST_MAX 512

#define WIKI_URL L"https://github.com/henrypp/simplewall/wiki/Rules-editor#rule-syntax-format"
#define BLOCKLIST_URL L"https://github.com/henrypp/simplewall/blob/master/bin/blocklist.xml"
#define LISTENS_ISSUE_URL L"https://github.com/henrypp/simplewall/issues/9"

// notification timer
#define NOTIFY_WIDTH 368
#define NOTIFY_HEIGHT 234
#define NOTIFY_SPACER 6
#define NOTIFY_CLASS_DLG L"NotificationDlg"
#define NOTIFY_TIMER_DISPLAY_ID 1001
#define NOTIFY_TIMER_TIMEOUT_ID 2002
#define NOTIFY_TIMER_MOUSELEAVE_ID 3003
#define NOTIFY_TIMER_MOUSE 250
#define NOTIFY_TIMER_POPUP 350
#define NOTIFY_TIMER_DEFAULT 20 // sec.
#define NOTIFY_TIMEOUT 30 // sec.
#define NOTIFY_TIMEOUT_MINIMUM 6 // sec.
#define NOTIFY_LIMIT_SIZE 6 //limit vector size
#define NOTIFY_SOUND_DEFAULT L"MailBeep"

// pugixml document configuration
#define PUGIXML_LOAD_FLAGS (pugi::parse_escapes)
#define PUGIXML_LOAD_ENCODING (pugi::encoding_auto)

#define PUGIXML_SAVE_FLAGS (pugi::format_indent | pugi::format_write_bom)
#define PUGIXML_SAVE_ENCODING (pugi::encoding_wchar)

// default colors
#define LISTVIEW_COLOR_SPECIAL RGB (255, 255, 170)
#define LISTVIEW_COLOR_INVALID RGB (255, 125, 148)
#define LISTVIEW_COLOR_NETWORK RGB (255, 178, 255)
#define LISTVIEW_COLOR_PACKAGE RGB(189, 251, 240)
#define LISTVIEW_COLOR_PICO RGB (51, 153, 255)
#define LISTVIEW_COLOR_SIGNED RGB (175, 228, 163)
#define LISTVIEW_COLOR_SILENT RGB (181, 181, 181)
#define LISTVIEW_COLOR_SYSTEM RGB(170, 204, 255)

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
enum EnumRuleDirection
{
	DirOutbound,
	DirInbound,
	DirBoth,
};

enum EnumRuleType
{
	TypeUnknown = 0,
	TypeIp = 2,
	TypePort = 4,
	TypeHost = 1,
};

enum EnumMode
{
	ModeWhitelist = 0,
	ModeBlacklist = 1,
};

struct STATIC_DATA
{
	SID* psid = nullptr;
	GUID* psession = nullptr;

	bool is_securityinfoset = false;
	bool is_popuperrors = false;
	bool is_notifytimeout = false;
	bool is_notifymouse = false;
	bool is_nocheckboxnotify = false;

	HIMAGELIST himg = nullptr;

	size_t icon_id = 0;
	size_t icon_package_id = 0;
	HICON hicon_large = nullptr;
	HICON hicon_small = nullptr;
	HICON hicon_package = nullptr;
	HBITMAP hbitmap_package_small = nullptr;

	HFONT hfont = nullptr;

	HANDLE hthread = nullptr;
	HANDLE hengine = nullptr;
	HANDLE hevent = nullptr;
	HANDLE hlog = nullptr;
	HANDLE hpackages = nullptr;

	HANDLE stop_evt = nullptr;
	HANDLE finish_evt = nullptr;

	WCHAR title[128] = {0};
	WCHAR apps_path[MAX_PATH] = {0};
	WCHAR rules_custom_path[MAX_PATH] = {0};
	WCHAR rules_config_path[MAX_PATH] = {0};
	WCHAR notify_snd_path[MAX_PATH] = {0};

	HWND hnotification = nullptr;

	HWND hfind = nullptr;
	WCHAR search_string[128] = {0};

	WCHAR windows_dir[MAX_PATH] = {0};
	size_t wd_length = 0;

	size_t ntoskrnl_hash = 0;
};

struct ITEM_APPLICATION
{
	__time64_t timestamp = 0;

	size_t icon_id = 0;

	bool error_count = false;

	bool is_enabled = false;
	bool is_network = false;
	bool is_system = false;
	bool is_silent = false;
	bool is_signed = false;
	bool is_devicepath = false;
	bool is_storeapp = false; // win8 and above
	bool is_picoapp = false; // win10 and above

	LPCWSTR description = nullptr;
	LPCWSTR signer = nullptr;

	WCHAR display_name[MAX_PATH] = {0};
	WCHAR original_path[MAX_PATH] = {0};
	WCHAR real_path[MAX_PATH] = {0};
};

struct ITEM_RULE
{
	bool error_count = false;

	bool is_enabled = false;
	bool is_block = false;

	EnumRuleDirection dir = DirOutbound;
	EnumRuleType type = TypeUnknown;

	UINT8 protocol = 0;
	ADDRESS_FAMILY version = AF_UNSPEC;

	LPWSTR pname = nullptr;
	LPWSTR prule = nullptr;

	std::unordered_map<size_t, bool> apps;
};

struct ITEM_LOG
{
	bool is_loopback = false;

	size_t hash = 0;

	HICON hicon = nullptr;

	UINT16 remote_port = 0;
	UINT16 local_port = 0;

	UINT32 flags = 0;
	UINT32 direction = 0;

	WCHAR protocol[16] = {0};

	WCHAR date[32] = {0};

	WCHAR remote_addr[LEN_IP_MAX] = {0};
	WCHAR local_addr[LEN_IP_MAX] = {0};

	WCHAR username[MAX_PATH] = {0};

	WCHAR provider_name[MAX_PATH] = {0};
	WCHAR filter_name[MAX_PATH] = {0};
};

struct ITEM_PACKAGE
{
	size_t hash = 0;

	WCHAR sid[MAX_PATH] = {0};
	WCHAR display_name[MAX_PATH] = {0};
	//WCHAR real_path[MAX_PATH] = {0};
};

struct ITEM_PROCESS
{
	HBITMAP hbmp = nullptr;

	WCHAR display_path[64] = {0};
	WCHAR real_path[MAX_PATH] = {0};
};

struct ITEM_COLOR
{
	bool is_enabled = false;

	size_t hash = 0;

	UINT locale_id = 0;

	COLORREF default_clr = 0;
	COLORREF clr = 0;

	HBRUSH hbr = nullptr;

	LPWSTR locale_sid = nullptr;
	LPWSTR config_name = nullptr;
	LPWSTR config_value = nullptr;
};

struct ITEM_PROTOCOL
{
	UINT8 id = 0;
	LPWSTR name = nullptr;
};

struct ITEM_ADDRESS
{
	bool is_range = false;

	EnumRuleType type = TypeUnknown;

	NET_ADDRESS_FORMAT format;

	UINT16 port = 0;

	FWP_V4_ADDR_AND_MASK* paddr4 = nullptr;
	FWP_V6_ADDR_AND_MASK* paddr6 = nullptr;

	FWP_RANGE* prange = nullptr;

	WCHAR host[LEN_HOST_MAX] = {0};
};

// dropped events callback subscription (win7 and above)
#ifndef FWP_DIRECTION_IN
#define FWP_DIRECTION_IN 0x00003900L
#endif

#ifndef FWP_DIRECTION_OUT
#define FWP_DIRECTION_OUT 0x00003901L
#endif

typedef void (CALLBACK *FWPM_NET_EVENT_CALLBACK2)(_Inout_ void* context, _In_ const FWPM_NET_EVENT3* event);

typedef DWORD (WINAPI *FWPMNES0) (HANDLE, const FWPM_NET_EVENT_SUBSCRIPTION0*, FWPM_NET_EVENT_CALLBACK0, LPVOID, HANDLE*); // subscribe (win7)
typedef DWORD (WINAPI *FWPMNES1) (HANDLE, const FWPM_NET_EVENT_SUBSCRIPTION0*, FWPM_NET_EVENT_CALLBACK1, LPVOID, HANDLE*); // subscribe (win8)
typedef DWORD (WINAPI *FWPMNES2) (HANDLE, const FWPM_NET_EVENT_SUBSCRIPTION0*, FWPM_NET_EVENT_CALLBACK2, LPVOID, HANDLE*); // subscribe (win10)

typedef DWORD (WINAPI *FWPMNEU) (HANDLE, HANDLE); // unsubcribe (all)

#endif // __MAIN_H__
