// simplewall
// Copyright (c) 2020 Henry++

#pragma once

PSID _app_quyerybuiltinsid (WELL_KNOWN_SID_TYPE sid_type);
PSID _app_queryservicesid (LPCWSTR name);

VOID _app_generate_credentials ();

FORCEINLINE VOID _app_setexplicitaccess (PEXPLICIT_ACCESS pea, ACCESS_MODE mode, ULONG rights, ULONG inheritance, PSID psid)
{
	pea->grfAccessMode = mode;
	pea->grfAccessPermissions = rights;
	pea->grfInheritance = inheritance;

	BuildTrusteeWithSid (&(pea->Trustee), psid);
}

PACL _app_createaccesscontrollist (PACL pacl, BOOLEAN is_secure);

VOID _app_setsecurityinfoforengine (HANDLE hengine);
VOID _app_setsecurityinfoforprovider (HANDLE hengine, LPCGUID lpguid, BOOLEAN is_secure);
VOID _app_setsecurityinfoforsublayer (HANDLE hengine, LPCGUID lpguid, BOOLEAN is_secure);
VOID _app_setsecurityinfoforfilter (HANDLE hengine, LPCGUID lpguid, BOOLEAN is_secure, UINT line);
