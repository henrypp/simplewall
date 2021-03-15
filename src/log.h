// simplewall
// Copyright (c) 2016-2021 Henry++

#pragma once

PR_STRING _app_getlogviewer ();

VOID _app_loginit (_In_ BOOLEAN is_install);
VOID _app_logwrite (_In_ PITEM_LOG ptr_log);
BOOLEAN _app_logisexists (_In_ HWND hwnd, _In_ PITEM_LOG ptr_log_new);
VOID _app_logwrite_ui (_In_ HWND hwnd, _In_ PITEM_LOG ptr_log);
BOOLEAN _app_logislimitreached ();
VOID _app_logclear ();
VOID _app_logclear_ui (_In_ HWND hwnd);

VOID _wfp_logsubscribe (_In_ HANDLE hengine);
VOID _wfp_logunsubscribe (_In_ HANDLE hengine);

VOID CALLBACK _wfp_logcallback (_In_ PITEM_LOG_CALLBACK log);

VOID CALLBACK _wfp_logcallback0 (_In_ PVOID context, _In_ const FWPM_NET_EVENT1* event);
VOID CALLBACK _wfp_logcallback1 (_In_ PVOID context, _In_ const FWPM_NET_EVENT2* event);
VOID CALLBACK _wfp_logcallback2 (_In_ PVOID context, _In_ const FWPM_NET_EVENT3* event);
VOID CALLBACK _wfp_logcallback3 (_In_ PVOID context, _In_ const FWPM_NET_EVENT4* event);
VOID CALLBACK _wfp_logcallback4 (_In_ PVOID context, _In_ const FWPM_NET_EVENT5* event);

THREAD_API LogThread (_In_ PVOID lparam);
