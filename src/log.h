// simplewall
// Copyright (c) 2016-2022 Henry++

#pragma once

// FwpmNetEventSubscribe4 (win10rs5+)
typedef ULONG (WINAPI *FWPMNES4)(
	_In_ HANDLE engine_handle,
	_In_ const FWPM_NET_EVENT_SUBSCRIPTION0 *subscription,
	_In_ FWPM_NET_EVENT_CALLBACK4 callback,
	_In_opt_ PVOID context,
	_Out_ PHANDLE events_handle
	);

// FwpmNetEventSubscribe3 (win10rs4+)
typedef ULONG (WINAPI *FWPMNES3)(
	_In_ HANDLE engine_handle,
	_In_ const FWPM_NET_EVENT_SUBSCRIPTION0 *subscription,
	_In_ FWPM_NET_EVENT_CALLBACK3 callback,
	_In_opt_ PVOID context,
	_Out_ PHANDLE events_handle
	);

// FwpmNetEventSubscribe2 (win10rs1+)
typedef ULONG (WINAPI *FWPMNES2)(
	_In_ HANDLE engine_handle,
	_In_ const FWPM_NET_EVENT_SUBSCRIPTION0 *subscription,
	_In_ FWPM_NET_EVENT_CALLBACK2 callback,
	_In_opt_ PVOID context,
	_Out_ PHANDLE events_handle
	);

// FwpmNetEventSubscribe1 (win8+)
typedef ULONG (WINAPI *FWPMNES1)(
	_In_ HANDLE engine_handle,
	_In_ const FWPM_NET_EVENT_SUBSCRIPTION0 *subscription,
	_In_ FWPM_NET_EVENT_CALLBACK1 callback,
	_In_opt_ PVOID context,
	_Out_ PHANDLE events_handle
	);

// FwpmNetEventSubscribe0 (win7+)
typedef ULONG (WINAPI *FWPMNES0)(
	_In_ HANDLE engine_handle,
	_In_ const FWPM_NET_EVENT_SUBSCRIPTION0 *subscription,
	_In_ FWPM_NET_EVENT_CALLBACK0 callback,
	_In_opt_ PVOID context,
	_Out_ PHANDLE events_handle
	);

VOID _app_loginit (
	_In_ BOOLEAN is_install
);

VOID _app_loginitfile (
	_In_ HANDLE hfile
);

ULONG_PTR _app_getloghash (
	_In_ PITEM_LOG ptr_log
);

PR_STRING _app_getlogpath ();

PR_STRING _app_getlogviewer ();

BOOLEAN _app_islogfound (
	_In_ ULONG_PTR log_hash
);

BOOLEAN _app_logislimitreached (
	_In_ HANDLE hfile
);

VOID _app_logclear (
	_In_opt_ HANDLE hfile
);

VOID _app_logclear_ui (
	_In_ HWND hwnd
);

VOID _app_logwrite (
	_In_ PITEM_LOG ptr_log
);

VOID _app_logwrite_ui (
	_In_ HWND hwnd,
	_In_ PITEM_LOG ptr_log
);

VOID _wfp_logsubscribe (
	_In_ HANDLE engine_handle
);

VOID _wfp_logunsubscribe (
	_In_ HANDLE engine_handle
);

VOID _wfp_logsetoption (
	_In_ HANDLE engine_handle
);

VOID CALLBACK _wfp_logcallback (
	_In_ PITEM_LOG_CALLBACK log
);

// win7+ callback
VOID CALLBACK _wfp_logcallback0 (
	_In_opt_ PVOID context,
	_In_ const FWPM_NET_EVENT1* event_data
);

// win8+ callback
VOID CALLBACK _wfp_logcallback1 (
	_In_opt_ PVOID context,
	_In_ const FWPM_NET_EVENT2* event_data
);

// win10rs1+ callback
VOID CALLBACK _wfp_logcallback2 (
	_In_opt_ PVOID context,
	_In_ const FWPM_NET_EVENT3* event_data
);

// win10rs4+ callback
VOID CALLBACK _wfp_logcallback3 (
	_In_opt_ PVOID context,
	_In_ const FWPM_NET_EVENT4* event_data
);

// win10rs5+ callback
VOID CALLBACK _wfp_logcallback4 (
	_In_opt_ PVOID context,
	_In_ const FWPM_NET_EVENT5* event_data
);

VOID NTAPI _app_logthread (
	_In_ PVOID arglist,
	_In_ ULONG busy_count
);
