// simplewall
// Copyright (c) 2020-2024 Henry++

#pragma once

_Ret_maybenull_
PSID _app_quyerybuiltinsid (
	_In_ WELL_KNOWN_SID_TYPE sid_type
);

VOID _app_generate_credentials ();

_Ret_maybenull_
PACL _app_createaccesscontrollist (
	_In_ PACL acl,
	_In_ BOOLEAN is_secure
);

VOID _app_setexplicitaccess (
	_Out_ PEXPLICIT_ACCESS ea,
	_In_ ACCESS_MODE mode,
	_In_ ULONG rights,
	_In_ ULONG inheritance,
	_In_opt_ PSID sid
);

VOID _app_setenginesecurity (
	_In_ HANDLE hengine
);

VOID _app_setprovidersecurity (
	_In_ HANDLE hengine,
	_In_ LPCGUID provider_guid,
	_In_ BOOLEAN is_secure
);

VOID _app_setsublayersecurity (
	_In_ HANDLE hengine,
	_In_ LPCGUID sublayer_guid,
	_In_ BOOLEAN is_secure
);

VOID _app_setcalloutsecurity (
	_In_ HANDLE hengine,
	_In_ LPCGUID callout_guid,
	_In_ BOOLEAN is_secure
);

VOID _app_setfiltersecurity (
	_In_ HANDLE hengine,
	_In_ LPCGUID filter_guid,
	_In_ BOOLEAN is_secure,
	_In_ LPCWSTR file_name,
	_In_ UINT line
);
