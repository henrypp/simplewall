// simplewall
// Copyright (c) 2016-2019 Henry++

#pragma once

bool _wfp_isfiltersapplying ();
bool _wfp_isfiltersinstalled ();

bool _wfp_initialize (bool is_full);
void _wfp_uninitialize (bool is_full);

void _wfp_installfilters ();

bool _wfp_transact_start (UINT line);
bool _wfp_transact_commit (UINT line);

bool _wfp_deletefilter (HANDLE engineHandle, const GUID* pfilter_id);
DWORD _wfp_createfilter (LPCWSTR name, FWPM_FILTER_CONDITION* lpcond, UINT32 const count, UINT8 weight, const GUID* layer, const GUID* callout, FWP_ACTION_TYPE action, UINT32 flags, MARRAY* pmar = nullptr);

void _wfp_destroyfilters ();
bool _wfp_destroy2filters (const MARRAY* pmar, UINT line);

bool _wfp_createrulefilter (LPCWSTR name, size_t app_hash, LPCWSTR rule_remote, LPCWSTR rule_local, UINT8 protocol, ADDRESS_FAMILY af, FWP_DIRECTION dir, UINT8 weight, FWP_ACTION_TYPE action, UINT32 flag, MARRAY* pmfarr);

bool _wfp_create4filters (const MFILTER_RULES* ptr_rules, UINT line, bool is_intransact = false);
bool _wfp_create3filters (const MFILTER_APPS* ptr_apps, UINT line, bool is_intransact = false);
bool _wfp_create2filters (UINT line, bool is_intransact = false);

void _wfp_setfiltersecurity (HANDLE engineHandle, const GUID* pfilter_id, const PSID psid, PACL pacl, UINT line);

size_t _wfp_dumpfilters (const GUID* pprovider, MARRAY* pfilters);

bool _mps_firewallapi (bool* pis_enabled, const bool* pis_enable);
void _mps_changeconfig2 (bool is_enable);

DWORD _FwpmGetAppIdFromFileName1 (LPCWSTR path, FWP_BYTE_BLOB** lpblob, EnumDataType type);

bool ByteBlobAlloc (PVOID data, size_t length, FWP_BYTE_BLOB** lpblob);
void ByteBlobFree (FWP_BYTE_BLOB** lpblob);
