// simplewall
// Copyright (c) 2020-2021 Henry++

#pragma once

_Ret_maybenull_
PSID _app_quyerybuiltinsid (_In_ WELL_KNOWN_SID_TYPE sid_type);

_Ret_maybenull_
PSID _app_queryservicesid (_In_ LPCWSTR name);

VOID _app_generate_credentials ();

_Ret_maybenull_
PACL _app_createaccesscontrollist (_In_ PACL pacl, _In_ BOOLEAN is_secure);

VOID _app_setexplicitaccess (_Out_ PEXPLICIT_ACCESS pea, _In_ ACCESS_MODE mode, _In_ ULONG rights, _In_ ULONG inheritance, _In_opt_ PSID psid);
VOID _app_setsecurityinfoforengine (_In_ HANDLE hengine);
VOID _app_setsecurityinfoforprovider (_In_ HANDLE hengine, _In_ LPCGUID lpguid, _In_ BOOLEAN is_secure);
VOID _app_setsecurityinfoforsublayer (_In_ HANDLE hengine, _In_ LPCGUID lpguid, _In_ BOOLEAN is_secure);
VOID _app_setsecurityinfoforfilter (_In_ HANDLE hengine, _In_ LPCGUID lpguid, _In_ BOOLEAN is_secure, _In_ UINT line);
