// simplewall
// Copyright (c) 2016-2021 Henry++

#pragma once

ENUM_INSTALL_TYPE _wfp_isproviderinstalled (_In_ HANDLE hengine);
ENUM_INSTALL_TYPE _wfp_issublayerinstalled (_In_ HANDLE hengine);

BOOLEAN _wfp_isfiltersapplying ();
ENUM_INSTALL_TYPE _wfp_isfiltersinstalled ();

HANDLE _wfp_getenginehandle ();

BOOLEAN _wfp_initialize (_In_ HANDLE hengine, _In_ BOOLEAN is_full);
VOID _wfp_uninitialize (_In_ HANDLE hengine, _In_ BOOLEAN is_full);

VOID _wfp_installfilters (_In_ HANDLE hengine);

BOOLEAN _wfp_transact_start (_In_ HANDLE hengine, _In_ UINT line);
BOOLEAN _wfp_transact_commit (_In_ HANDLE hengine, _In_ UINT line);

BOOLEAN _wfp_deletefilter (_In_ HANDLE hengine, _In_ LPCGUID filter_id);
ULONG _wfp_createfilter (_In_ HANDLE hengine, _In_ ENUM_TYPE_DATA filter_type, _In_opt_ LPCWSTR name, _In_count_ (count) FWPM_FILTER_CONDITION* lpcond, _In_ UINT32 count, _In_ UINT8 weight, _In_opt_ LPCGUID layer_id, _In_opt_ LPCGUID callout_id, _In_ FWP_ACTION_TYPE action, _In_ UINT32 flags, _In_opt_ PR_ARRAY guids);

VOID _wfp_clearfilter_ids ();
VOID _wfp_destroyfilters (_In_ HANDLE hengine);
BOOLEAN _wfp_destroyfilters_array (_In_ HANDLE hengine, _In_ PR_ARRAY guids, _In_ UINT line);

BOOLEAN _wfp_createrulefilter (_In_ HANDLE hengine, _In_ ENUM_TYPE_DATA filter_type, _In_opt_ LPCWSTR name, _In_opt_ SIZE_T app_hash, _In_opt_ PR_STRING rule_remote, _In_opt_ PR_STRING rule_local, _In_opt_ UINT8 protocol, _In_opt_ ADDRESS_FAMILY af, _In_opt_ FWP_DIRECTION dir, _In_ UINT8 weight, _In_ FWP_ACTION_TYPE action, _In_opt_ UINT32 flag, _In_opt_ PR_ARRAY guids);

BOOLEAN _wfp_create4filters (_In_ HANDLE hengine, _In_  PR_LIST rules, _In_ UINT line, _In_ BOOLEAN is_intransact);
BOOLEAN _wfp_create3filters (_In_ HANDLE hengine, _In_ PR_LIST rules, _In_ UINT line, _In_ BOOLEAN is_intransact);
BOOLEAN _wfp_create2filters (_In_ HANDLE hengine, _In_ UINT line, _In_ BOOLEAN is_intransact);

SIZE_T _wfp_dumpfilters (_In_ HANDLE hengine, _In_ LPCGUID provider_id, _Inout_ PR_ARRAY guids);

BOOLEAN _mps_firewallapi (_Inout_opt_ PBOOLEAN pis_enabled, _In_opt_ PBOOLEAN pis_enable);
VOID _mps_changeconfig2 (_In_ BOOLEAN is_enable);

_Success_ (return == ERROR_SUCCESS)
ULONG _FwpmGetAppIdFromFileName1 (_In_ LPCWSTR path, _Outptr_ FWP_BYTE_BLOB** lpblob, _In_ ENUM_TYPE_DATA type);

VOID ByteBlobAlloc (_In_ LPCVOID data, _In_ SIZE_T bytes_count, _Outptr_ FWP_BYTE_BLOB** byte_blob);
VOID ByteBlobFree (_Inout_ FWP_BYTE_BLOB** byte_blob);
