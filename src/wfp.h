// simplewall
// Copyright (c) 2016-2021 Henry++

#pragma once

ENUM_INSTALL_TYPE _wfp_isproviderinstalled (HANDLE hengine);
ENUM_INSTALL_TYPE _wfp_issublayerinstalled (HANDLE hengine);

BOOLEAN _wfp_isfiltersapplying ();
ENUM_INSTALL_TYPE _wfp_isfiltersinstalled ();

HANDLE _wfp_getenginehandle ();

BOOLEAN _wfp_initialize (HANDLE hengine, BOOLEAN is_full);
VOID _wfp_uninitialize (HANDLE hengine, BOOLEAN is_full);

VOID _wfp_installfilters (HANDLE hengine);

BOOLEAN _wfp_transact_start (HANDLE hengine, UINT line);
BOOLEAN _wfp_transact_commit (HANDLE hengine, UINT line);

BOOLEAN _wfp_deletefilter (HANDLE hengine, LPCGUID filter_id);
ULONG _wfp_createfilter (HANDLE hengine, ENUM_TYPE_DATA filter_type, LPCWSTR name, FWPM_FILTER_CONDITION* lpcond, UINT32 count, UINT8 weight, LPCGUID layer_id, LPCGUID callout_id, FWP_ACTION_TYPE action, UINT32 flags, PR_ARRAY guids);

VOID _wfp_clearfilter_ids ();
VOID _wfp_destroyfilters (HANDLE hengine);
BOOLEAN _wfp_destroyfilters_array (HANDLE hengine, PR_ARRAY guids, UINT line);

BOOLEAN _wfp_createrulefilter (HANDLE hengine, ENUM_TYPE_DATA filter_type, LPCWSTR name, SIZE_T app_hash, PR_STRING rule_remote, PR_STRING rule_local, UINT8 protocol, ADDRESS_FAMILY af, FWP_DIRECTION dir, UINT8 weight, FWP_ACTION_TYPE action, UINT32 flag, PR_ARRAY guids);

BOOLEAN _wfp_create4filters (HANDLE hengine, PR_LIST rules, UINT line, BOOLEAN is_intransact);
BOOLEAN _wfp_create3filters (HANDLE hengine, PR_LIST rules, UINT line, BOOLEAN is_intransact);
BOOLEAN _wfp_create2filters (HANDLE hengine, UINT line, BOOLEAN is_intransact);

SIZE_T _wfp_dumpfilters (HANDLE hengine, LPCGUID provider_id, PR_ARRAY guids);

BOOLEAN _mps_firewallapi (PBOOLEAN pis_enabled, PBOOLEAN pis_enable);
VOID _mps_changeconfig2 (BOOLEAN is_enable);

ULONG _FwpmGetAppIdFromFileName1 (LPCWSTR path, FWP_BYTE_BLOB** lpblob, ENUM_TYPE_DATA type);

VOID ByteBlobAlloc (_In_ LPCVOID data, _In_ SIZE_T bytes_count, _Outptr_ FWP_BYTE_BLOB** byte_blob);
VOID ByteBlobFree (_Inout_ FWP_BYTE_BLOB** byte_blob);
