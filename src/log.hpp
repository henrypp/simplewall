// simplewall
// Copyright (c) 2016-2020 Henry++

#pragma once

rstring _app_getlogviewer ();

void _app_loginit (bool is_install);
void _app_logwrite (PITEM_LOG ptr_log);
bool _app_logisexists (HWND hwnd, PITEM_LOG ptr_log_new);
void _app_logwrite_ui (HWND hwnd, PR_OBJECT ptr_log_object);
bool _app_logchecklimit ();
void _app_logclear ();
void _app_logclear_ui (HWND hwnd);

void _wfp_logsubscribe (HANDLE hengine);
void _wfp_logunsubscribe (HANDLE hengine);

void CALLBACK _wfp_logcallback (UINT32 flags, FILETIME const* pft, UINT8 const* app_id, SID* package_id, SID* user_id, UINT8 proto, FWP_IP_VERSION ipver, UINT32 remote_addr4, FWP_BYTE_ARRAY16 const* remote_addr6, UINT16 remote_port, UINT32 local_addr4, FWP_BYTE_ARRAY16 const* local_addr6, UINT16 local_port, UINT16 layer_id, UINT64 filter_id, UINT32 direction, bool is_allow, bool is_loopback);

void CALLBACK _wfp_logcallback0 (LPVOID, const FWPM_NET_EVENT1* pEvent);
void CALLBACK _wfp_logcallback1 (LPVOID, const FWPM_NET_EVENT2* pEvent);
void CALLBACK _wfp_logcallback2 (LPVOID, const FWPM_NET_EVENT3* pEvent);
void CALLBACK _wfp_logcallback3 (LPVOID, const FWPM_NET_EVENT4* pEvent);
void CALLBACK _wfp_logcallback4 (LPVOID, const FWPM_NET_EVENT5* pEvent);

THREAD_FN LogThread (LPVOID lparam);
