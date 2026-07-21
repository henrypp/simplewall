// simplewall
// Copyright (c) 2016-2026 Henry++

#pragma once

// guids
DEFINE_GUID (GUID_WfpProvider, 0xB0D553E2, 0xC6A0, 0x4A9A, 0xAE, 0xB8, 0xC7, 0x52, 0x48, 0x3E, 0xD6, 0x2F);
DEFINE_GUID (GUID_WfpSublayer, 0x9FEE6F59, 0xB951, 0x4F9A, 0xB5, 0x2F, 0x13, 0x3D, 0xCF, 0x7A, 0x42, 0x79);

// filter names
#define FWN_BLOCK_CONNECTION L"BlockConnection"
#define FWN_BLOCK_RECVACCEPT L"BlockRecvAccept"
#define FWN_ICMP_ERROR L"BlockIcmpError"
#define FWN_TCP_RST_ONCLOSE L"BlockTcpRstOnClose"
#define FWN_BOOTTIME L"BlockBoottime"
#define FWN_LOOPBACK L"AllowLoopback"

// sublayer weight
#define FWW_SUBLAYER 0xFFFF // highest weight for UINT16

// filter weights
#define FWW_IMPORTANT 0x0F
#define FWW_HIGHEST 0x0E
#define FWW_RULE_BLOCKLIST 0x0D
#define FWW_RULE_USER_BLOCK 0x0C
#define FWW_RULE_USER 0x0B
#define FWW_RULE_SYSTEM 0x0A
#define FWW_APP 0x09
#define FWW_LOWEST 0x08

HANDLE _wfp_getenginehandle ();

BOOLEAN _wfp_isfiltersapplying ();

ENUM_INSTALL_TYPE _wfp_isproviderinstalled (
	_In_ HANDLE engine_handle
);

ENUM_INSTALL_TYPE _wfp_issublayerinstalled (
	_In_ HANDLE engine_handle
);

BOOLEAN _wfp_isfiltersinstalled ();

ENUM_INSTALL_TYPE _wfp_getinstalltype ();

_Ret_maybenull_
PR_STRING _wfp_getlayername (
	_In_ LPCGUID layer_guid
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
	_In_ ULONG line
);

BOOLEAN _wfp_transact_commit (
	_In_ HANDLE engine_handle,
	_In_ LPCWSTR file_name,
	_In_ ULONG line
);

BOOLEAN _wfp_deletefilter (
	_In_ HANDLE engine_handle,
	_In_ LPCGUID filter_id,
	_In_ LPCWSTR file_name,
	_In_ ULONG line
);

ULONG _wfp_createfilter (
	_In_ HANDLE engine_handle,
	_In_ ENUM_TYPE_DATA type,
	_In_opt_ LPCWSTR filter_name,
	_In_reads_opt_ (count) FWPM_FILTER_CONDITION0 *lpcond,
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
	_Inout_ PR_ARRAY guids,
	_In_ LPCWSTR file_name,
	_In_ ULONG line
);

BOOLEAN _wfp_createrulefilter (
	_In_ HANDLE engine_handle,
	_In_ ENUM_TYPE_DATA filter_type,
	_In_opt_ LPCWSTR filter_name,
	_In_opt_ ULONG app_hash,
	_In_opt_ PITEM_FILTER_CONFIG filter_config,
	_In_opt_ PCR_STRINGREF rule_remote,
	_In_opt_ PCR_STRINGREF rule_local,
	_In_ UINT8 weight,
	_In_ FWP_ACTION_TYPE action,
	_In_ UINT32 flags,
	_Inout_opt_ PR_ARRAY guids
);

BOOLEAN _wfp_createrulefilters (
	_In_ HANDLE engine_handle,
	_In_ PR_LIST rules,
	_In_ LPCWSTR file_name,
	_In_ ULONG line,
	_In_ BOOLEAN is_intransact
);

BOOLEAN _wfp_createappfilters (
	_In_ HANDLE engine_handle,
	_In_ PR_LIST rules,
	_In_ LPCWSTR file_name,
	_In_ ULONG line,
	_In_ BOOLEAN is_intransact
);

BOOLEAN _wfp_createglobalfilters (
	_In_ HANDLE engine_handle,
	_In_ LPCWSTR file_name,
	_In_ ULONG line,
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
	_In_ PVOID arglist
);

VOID _wfp_firewallenable (
	_In_ BOOLEAN is_enable
);

BOOLEAN _wfp_firewallisenabled ();

_Success_ (NT_SUCCESS (return))
NTSTATUS _FwpmGetAppIdFromFileName1 (
	_Out_ PVOID_PTR byte_blob,
	_In_ PR_STRING path,
	_In_ ENUM_TYPE_DATA type
);

VOID ByteBlobAlloc (
	_Out_ PVOID_PTR byte_blob,
	_In_ LPCVOID data,
	_In_ ULONG_PTR bytes_count
);

VOID ByteBlobFree (
	_Inout_ PVOID_PTR byte_blob
);
