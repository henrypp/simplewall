// simplewall
// Copyright (c) 2016-2021 Henry++

#pragma once

FORCEINLINE PR_STRING _app_getlogpath ()
{
	return _r_config_getstringexpand (L"LogPath", LOG_PATH_DEFAULT);
}

FORCEINLINE PR_STRING _app_getlogviewer ()
{
	return _r_config_getstringexpand (L"LogViewer", LOG_VIEWER_DEFAULT);
}

VOID _app_loginit (_In_ BOOLEAN is_install);
ULONG_PTR _app_getloghash (_In_ HWND hwnd, _In_ PITEM_LOG ptr_log);
BOOLEAN _app_islogfound (_In_ ULONG_PTR log_hash);
VOID _app_setlogiteminfo (_In_ HWND hwnd, _In_ INT listview_id, _In_ INT item_id, _In_ PITEM_LOG ptr_log);
BOOLEAN _app_logislimitreached (_In_ HANDLE hfile);

VOID _app_logclear (_In_opt_ HANDLE hfile);
VOID _app_logclear_ui (_In_ HWND hwnd);

VOID _app_logwrite (_In_ PITEM_LOG ptr_log);
VOID _app_logwrite_ui (_In_ HWND hwnd, _In_ PITEM_LOG ptr_log);

VOID _wfp_logsubscribe (_In_ HANDLE hengine);
VOID _wfp_logunsubscribe (_In_ HANDLE hengine);

VOID CALLBACK _wfp_logcallback (_In_ PITEM_LOG_CALLBACK log);

VOID CALLBACK _wfp_logcallback0 (_In_ PVOID context, _In_ const FWPM_NET_EVENT1* event);
VOID CALLBACK _wfp_logcallback1 (_In_ PVOID context, _In_ const FWPM_NET_EVENT2* event);
VOID CALLBACK _wfp_logcallback2 (_In_ PVOID context, _In_ const FWPM_NET_EVENT3* event);
VOID CALLBACK _wfp_logcallback3 (_In_ PVOID context, _In_ const FWPM_NET_EVENT4* event);
VOID CALLBACK _wfp_logcallback4 (_In_ PVOID context, _In_ const FWPM_NET_EVENT5* event);

THREAD_API LogThread (_In_ PVOID arglist);
