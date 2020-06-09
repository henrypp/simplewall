// simplewall
// Copyright (c) 2016-2020 Henry++

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

BOOLEAN _wfp_deletefilter (HANDLE hengine, const LPGUID pfilter_id);
DWORD _wfp_createfilter (HANDLE hengine, LPCWSTR name, FWPM_FILTER_CONDITION* lpcond, UINT32 count, UINT8 weight, const GUID* layer_id, const GUID* callout_id, FWP_ACTION_TYPE action, UINT32 flags, GUIDS_VEC* ptr_filters);

VOID _wfp_clearfilter_ids ();
VOID _wfp_destroyfilters (HANDLE hengine);
BOOLEAN _wfp_destroyfilters_array (HANDLE hengine, GUIDS_VEC& ptr_filters, UINT line);

BOOLEAN _wfp_createrulefilter (HANDLE hengine, LPCWSTR name, SIZE_T app_hash, LPCWSTR rule_remote, LPCWSTR rule_local, UINT8 protocol, ADDRESS_FAMILY af, FWP_DIRECTION dir, UINT8 weight, FWP_ACTION_TYPE action, UINT32 flag, GUIDS_VEC* pmfarr);

BOOLEAN _wfp_create4filters (HANDLE hengine, const OBJECTS_VEC& ptr_rules, UINT line, BOOLEAN is_intransact = FALSE);
BOOLEAN _wfp_create3filters (HANDLE hengine, const OBJECTS_VEC& ptr_apps, UINT line, BOOLEAN is_intransact = FALSE);
BOOLEAN _wfp_create2filters (HANDLE hengine, UINT line, BOOLEAN is_intransact = FALSE);

SIZE_T _wfp_dumpfilters (HANDLE hengine, const GUID* pprovider_id, GUIDS_VEC* ptr_filters);

BOOLEAN _mps_firewallapi (PBOOLEAN pis_enabled, const PBOOLEAN pis_enable);
VOID _mps_changeconfig2 (BOOLEAN is_enable);

DWORD _FwpmGetAppIdFromFileName1 (LPCWSTR path, FWP_BYTE_BLOB** lpblob, ENUM_TYPE_DATA type);

BOOLEAN ByteBlobAlloc (const PVOID pdata, SIZE_T length, FWP_BYTE_BLOB** lpblob);
VOID ByteBlobFree (FWP_BYTE_BLOB** lpblob);
