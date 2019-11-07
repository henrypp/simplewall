// simplewall
// Copyright (c) 2016-2019 Henry++

#pragma once

void _app_logerror (LPCWSTR fn, DWORD errcode, LPCWSTR desc, bool is_nopopups);

rstring _app_getlogviewer ();

bool _app_loginit (bool is_install);
void _app_logwrite (PITEM_LOG ptr_log);
bool _app_logchecklimit ();
void _app_logclear ();

bool _wfp_logsubscribe (HANDLE hengine);
bool _wfp_logunsubscribe (HANDLE hengine);

void CALLBACK _wfp_logcallback (UINT32 flags, FILETIME const *pft, UINT8 const*app_id, SID * package_id, SID * user_id, UINT8 proto, FWP_IP_VERSION ipver, UINT32 remote_addr4, FWP_BYTE_ARRAY16 const* remote_addr6, UINT16 remote_port, UINT32 local_addr4, FWP_BYTE_ARRAY16 const* local_addr6, UINT16 local_port, UINT16 layer_id, UINT64 filter_id, UINT32 direction, bool is_allow, bool is_loopback);

void CALLBACK _wfp_logcallback0 (LPVOID, const FWPM_NET_EVENT1 *pEvent);
void CALLBACK _wfp_logcallback1 (LPVOID, const FWPM_NET_EVENT2 *pEvent);
void CALLBACK _wfp_logcallback2 (LPVOID, const FWPM_NET_EVENT3 *pEvent);
void CALLBACK _wfp_logcallback3 (LPVOID, const FWPM_NET_EVENT4 *pEvent);
void CALLBACK _wfp_logcallback4 (LPVOID, const FWPM_NET_EVENT5 *pEvent);

UINT WINAPI LogThread (LPVOID lparam);
