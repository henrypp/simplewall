// simplewall
// Copyright (c) 2016-2021 Henry++

#pragma once

ENUM_INSTALL_TYPE _wfp_isproviderinstalled (_In_ HANDLE engine_handle);
ENUM_INSTALL_TYPE _wfp_issublayerinstalled (_In_ HANDLE engine_handle);

BOOLEAN _wfp_isfiltersapplying ();
ENUM_INSTALL_TYPE _wfp_isfiltersinstalled ();

_Ret_maybenull_
HANDLE _wfp_getenginehandle ();

BOOLEAN _wfp_initialize (_In_ HANDLE engine_handle);
VOID _wfp_uninitialize (_In_ HANDLE engine_handle, _In_ BOOLEAN is_full);

VOID _wfp_installfilters (_In_ HANDLE engine_handle);

BOOLEAN _wfp_transact_start (_In_ HANDLE engine_handle, _In_ UINT line);
BOOLEAN _wfp_transact_commit (_In_ HANDLE engine_handle, _In_ UINT line);

BOOLEAN _wfp_deletefilter (_In_ HANDLE engine_handle, _In_ LPCGUID filter_id);
ULONG _wfp_createfilter (_In_ HANDLE engine_handle, _In_ ENUM_TYPE_DATA type, _In_opt_ LPCWSTR filter_name, _In_reads_ (count) FWPM_FILTER_CONDITION *lpcond, _In_ UINT32 count, _In_opt_ LPCGUID layer_id, _In_opt_ LPCGUID callout_id, _In_ UINT8 weight, _In_ FWP_ACTION_TYPE action, _In_ UINT32 flags, _Inout_opt_ PR_ARRAY guids);

VOID _wfp_clearfilter_ids ();
VOID _wfp_destroyfilters (_In_ HANDLE engine_handle);
BOOLEAN _wfp_destroyfilters_array (_In_ HANDLE engine_handle, _In_ PR_ARRAY guids, _In_ UINT line);

BOOLEAN _wfp_createrulefilter (_In_ HANDLE engine_handle, _In_ ENUM_TYPE_DATA filter_type, _In_opt_ LPCWSTR filter_name, _In_opt_ ULONG_PTR app_hash, _In_opt_ PITEM_FILTER_CONFIG filter_config, _In_opt_ PR_STRINGREF rule_remote, _In_opt_ PR_STRINGREF rule_local, _In_ UINT8 weight, _In_ FWP_ACTION_TYPE action, _In_ UINT32 flags, _Inout_opt_ PR_ARRAY guids);

BOOLEAN _wfp_create4filters (_In_ HANDLE engine_handle, _In_  PR_LIST rules, _In_ UINT line, _In_ BOOLEAN is_intransact);
BOOLEAN _wfp_create3filters (_In_ HANDLE engine_handle, _In_ PR_LIST rules, _In_ UINT line, _In_ BOOLEAN is_intransact);
BOOLEAN _wfp_create2filters (_In_ HANDLE engine_handle, _In_ UINT line, _In_ BOOLEAN is_intransact);

_Ret_maybenull_
PR_ARRAY _wfp_dumpfilters (_In_ HANDLE engine_handle, _In_ LPCGUID provider_id);

VOID NTAPI _wfp_applythread (_In_ PVOID arglist, _In_ ULONG busy_count);

VOID _wfp_firewallenable (_In_ BOOLEAN is_enable);
BOOLEAN _wfp_firewallisenabled ();

ULONG _FwpmGetAppIdFromFileName1 (_In_ PR_STRING path, _In_ ENUM_TYPE_DATA type, _Out_ PVOID_PTR byte_blob);

VOID ByteBlobAlloc (_In_ LPCVOID data, _In_ SIZE_T bytes_count, _Out_ PVOID_PTR byte_blob);
VOID ByteBlobFree (_Inout_ PVOID_PTR byte_blob);
