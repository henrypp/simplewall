// simplewall
// Copyright (c) 2016-2019 Henry++

#pragma once

bool _wfp_isproviderinstalled ();
bool _wfp_issublayerinstalled ();

bool _wfp_isfiltersapplying ();
bool _wfp_isfiltersinstalled ();

HANDLE& _wfp_getenginehandle ();

bool _wfp_initialize (bool is_full);
void _wfp_uninitialize (bool is_full);

void _wfp_installfilters ();

bool _wfp_transact_start (HANDLE hengine, UINT line);
bool _wfp_transact_commit (HANDLE hengine, UINT line);

bool _wfp_deletefilter (HANDLE hengine, const GUID* ptr_filter_id);
DWORD _wfp_createfilter (HANDLE hengine, LPCWSTR name, FWPM_FILTER_CONDITION* lpcond, UINT32 count, UINT8 weight, const GUID* layer, const GUID* callout, FWP_ACTION_TYPE action, UINT32 flags, GUIDS_VEC* ptr_filters);

void _wfp_clearfilter_ids ();
void _wfp_destroyfilters (HANDLE hengine);
bool _wfp_destroyfilters_array (HANDLE hengine, GUIDS_VEC& ptr_filters, UINT line);

bool _wfp_createrulefilter (HANDLE hengine, LPCWSTR name, size_t app_hash, LPCWSTR rule_remote, LPCWSTR rule_local, UINT8 protocol, ADDRESS_FAMILY af, FWP_DIRECTION dir, UINT8 weight, FWP_ACTION_TYPE action, UINT32 flag, GUIDS_VEC* pmfarr);

bool _wfp_create4filters (HANDLE hengine, const OBJECTS_VEC& ptr_rules, UINT line, bool is_intransact = false);
bool _wfp_create3filters (HANDLE hengine, const OBJECTS_VEC& ptr_apps, UINT line, bool is_intransact = false);
bool _wfp_create2filters (HANDLE hengine, UINT line, bool is_intransact = false);

void _wfp_setfiltersecurity (HANDLE hengine, const GUID& filter_id, const PACL pacl, UINT line);

size_t _wfp_dumpfilters (HANDLE hengine, const GUID* pprovider, GUIDS_VEC* ptr_filters);

bool _mps_firewallapi (bool* pis_enabled, const bool* pis_enable);
void _mps_changeconfig2 (bool is_enable);

DWORD _FwpmGetAppIdFromFileName1 (LPCWSTR path, FWP_BYTE_BLOB** lpblob, EnumDataType type);

bool ByteBlobAlloc (const PVOID data, size_t length, FWP_BYTE_BLOB** lpblob);
void ByteBlobFree (FWP_BYTE_BLOB** lpblob);
