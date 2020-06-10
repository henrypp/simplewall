// simplewall
// Copyright (c) 2016-2020 Henry++

#pragma once

rstring _app_getlogviewer ();

VOID _app_loginit (BOOLEAN is_install);
VOID _app_logwrite (PITEM_LOG ptr_log);
BOOLEAN _app_logisexists (HWND hwnd, PITEM_LOG ptr_log_new);
VOID _app_logwrite_ui (HWND hwnd, PITEM_LOG ptr_log);
BOOLEAN _app_logislimitreached ();
VOID _app_logclear ();
VOID _app_logclear_ui (HWND hwnd);

VOID _wfp_logsubscribe (HANDLE hengine);
VOID _wfp_logunsubscribe (HANDLE hengine);

VOID CALLBACK _wfp_logcallback (UINT32 flags, FILETIME const* pft, UINT8 const* app_id, SID* package_id, SID* user_id, UINT8 proto, FWP_IP_VERSION ipver, UINT32 remote_addr4, FWP_BYTE_ARRAY16 const* remote_addr6, UINT16 remote_port, UINT32 local_addr4, FWP_BYTE_ARRAY16 const* local_addr6, UINT16 local_port, UINT16 layer_id, UINT64 filter_id, UINT32 direction, BOOLEAN is_allow, BOOLEAN is_loopback);

VOID CALLBACK _wfp_logcallback0 (PVOID pContext, const FWPM_NET_EVENT1* pEvent);
VOID CALLBACK _wfp_logcallback1 (PVOID pContext, const FWPM_NET_EVENT2* pEvent);
VOID CALLBACK _wfp_logcallback2 (PVOID pContext, const FWPM_NET_EVENT3* pEvent);
VOID CALLBACK _wfp_logcallback3 (PVOID pContext, const FWPM_NET_EVENT4* pEvent);
VOID CALLBACK _wfp_logcallback4 (PVOID pContext, const FWPM_NET_EVENT5* pEvent);

THREAD_FN LogThread (PVOID lparam);
