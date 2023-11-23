// simplewall
// Copyright (c) 2016-2023 Henry++

#pragma once

// guids
DEFINE_GUID (GUID_WfpProvider, 0xb0d553e2, 0xc6a0, 0x4a9a, 0xae, 0xb8, 0xc7, 0x52, 0x48, 0x3e, 0xd6, 0x2f);
DEFINE_GUID (GUID_WfpSublayer, 0x9fee6f59, 0xb951, 0x4f9a, 0xb5, 0x2f, 0x13, 0x3d, 0xcf, 0x7a, 0x42, 0x79);

// filter names
#define FW_NAME_BLOCK_CONNECTION L"BlockConnection"
#define FW_NAME_BLOCK_RECVACCEPT L"BlockRecvAccept"
#define FW_NAME_ICMP_ERROR L"BlockIcmpError"
#define FW_NAME_TCP_RST_ONCLOSE L"BlockTcpRstOnClose"
#define FW_NAME_BOOTTIME L"BlockBoottime"
#define FW_NAME_LOOPBACK L"AllowLoopback"

// sublayer weight
#define FW_SUBLAYER_WEIGHT 0xFFFF

// filter weights
#define FW_WEIGHT_HIGHEST_IMPORTANT 0x0F
#define FW_WEIGHT_HIGHEST 0x0E
#define FW_WEIGHT_RULE_BLOCKLIST 0x0D
#define FW_WEIGHT_RULE_USER_BLOCK 0x0C
#define FW_WEIGHT_RULE_USER 0x0B
#define FW_WEIGHT_RULE_SYSTEM 0x0A
#define FW_WEIGHT_APP 0x09
#define FW_WEIGHT_LOWEST 0x08

ENUM_INSTALL_TYPE _wfp_isproviderinstalled (
	_In_ HANDLE engine_handle
);

ENUM_INSTALL_TYPE _wfp_issublayerinstalled (
	_In_ HANDLE engine_handle
);

BOOLEAN _wfp_isfiltersapplying ();

BOOLEAN _wfp_isfiltersinstalled ();

HANDLE _wfp_getenginehandle ();

ENUM_INSTALL_TYPE _wfp_getinstalltype ();

PR_STRING _wfp_getlayername (
	_In_ LPGUID layer_guid
);

BOOLEAN _wfp_initialize (
	_In_opt_ HWND hwnd,
	_In_ HANDLE engine_handle
);

VOID _wfp_uninitialize (
	_In_ HANDLE engine_handle,
	_In_ BOOLEAN is_full
);

VOID _wfp_installfilters (
	_In_ HANDLE engine_handle
);

BOOLEAN _wfp_transact_start (
	_In_ HANDLE engine_handle,
	_In_ LPCWSTR file_name,
	_In_ UINT line
);

BOOLEAN _wfp_transact_commit (
	_In_ HANDLE engine_handle,
	_In_ LPCWSTR file_name,
	_In_ UINT line
);

ULONG _wfp_createcallout (
	_In_ HANDLE engine_handle,
	_In_ LPCGUID layer_key,
	_In_ LPCGUID callout_key
);

BOOLEAN _wfp_deletefilter (
	_In_ HANDLE engine_handle,
	_In_ LPGUID filter_id
);

ULONG _wfp_createfilter (
	_In_ HANDLE engine_handle,
	_In_ ENUM_TYPE_DATA type,
	_In_opt_ LPCWSTR filter_name,
	_In_reads_ (count) FWPM_FILTER_CONDITION *lpcond,
	_In_ UINT32 count,
	_In_ LPCGUID layer_id,
	_In_opt_ LPCGUID callout_id,
	_In_ UINT8 weight,
	_In_ FWP_ACTION_TYPE action,
	_In_ UINT32 flags,
	_Inout_opt_ PR_ARRAY guids
);

VOID _wfp_clearfilter_ids ();

VOID _wfp_destroyfilters (
	_In_ HANDLE engine_handle
);

VOID _wfp_destroyfilters_array (
	_In_ HANDLE engine_handle,
	_In_ PR_ARRAY guids,
	_In_ LPCWSTR file_name,
	_In_ UINT line
);

BOOLEAN _wfp_createrulefilter (
	_In_ HANDLE engine_handle,
	_In_ ENUM_TYPE_DATA filter_type,
	_In_opt_ LPCWSTR filter_name,
	_In_opt_ ULONG_PTR app_hash,
	_In_opt_ PITEM_FILTER_CONFIG filter_config,
	_In_opt_ PR_STRINGREF rule_remote,
	_In_opt_ PR_STRINGREF rule_local,
	_In_ UINT8 weight,
	_In_ FWP_ACTION_TYPE action,
	_In_ UINT32 flags,
	_Inout_opt_ PR_ARRAY guids
);

BOOLEAN _wfp_create4filters (
	_In_ HANDLE engine_handle,
	_In_  PR_LIST rules,
	_In_ LPCWSTR file_name,
	_In_ UINT line,
	_In_ BOOLEAN is_intransact
);

BOOLEAN _wfp_create3filters (
	_In_ HANDLE engine_handle,
	_In_ PR_LIST rules,
	_In_ LPCWSTR file_name,
	_In_ UINT line,
	_In_ BOOLEAN is_intransact
);

BOOLEAN _wfp_create2filters (
	_In_ HANDLE engine_handle,
	_In_ LPCWSTR file_name,
	_In_ UINT line,
	_In_ BOOLEAN is_intransact
);

_Success_ (return == ERROR_SUCCESS)
ULONG _wfp_dumpcallouts (
	_In_ HANDLE engine_handle,
	_In_ LPCGUID provider_id,
	_Outptr_ PR_ARRAY_PTR out_buffer
);

_Success_ (return == ERROR_SUCCESS)
ULONG _wfp_dumpfilters (
	_In_ HANDLE engine_handle,
	_In_ LPCGUID provider_id,
	_Outptr_ PR_ARRAY_PTR out_buffer
);

VOID NTAPI _wfp_applythread (
	_In_ PVOID arglist,
	_In_ ULONG busy_count
);

VOID _wfp_firewallenable (
	_In_ BOOLEAN is_enable
);

BOOLEAN _wfp_firewallisenabled ();

_Success_ (NT_SUCCESS (return))
NTSTATUS _FwpmGetAppIdFromFileName1 (
	_In_ PR_STRING path,
	_In_ ENUM_TYPE_DATA type,
	_Out_ PVOID_PTR byte_blob
);

VOID ByteBlobAlloc (
	_In_ LPCVOID data,
	_In_ ULONG_PTR bytes_count,
	_Out_ PVOID_PTR byte_blob
);

VOID ByteBlobFree (
	_Inout_ PVOID_PTR byte_blob
);
