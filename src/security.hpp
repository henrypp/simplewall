// simplewall
// Copyright (c) 2020 Henry++

#pragma once

PSID _app_quyerybuiltinsid (WELL_KNOWN_SID_TYPE sid_type);
PSID _app_queryservicesid (LPCWSTR name);

VOID _app_generate_credentials ();

VOID _app_setexplicitaccess (PEXPLICIT_ACCESS pExplicitAccess, ACCESS_MODE AccessMode, DWORD dwInheritance, DWORD dwAccessPermissionss, PSID pusersid);
PACL _app_createaccesscontrollist (PACL pAcl, BOOLEAN is_secure);

VOID _app_setsecurityinfoforengine (HANDLE hengine);
VOID _app_setsecurityinfoforprovider (HANDLE hengine, const GUID* lpguid, BOOLEAN is_secure);
VOID _app_setsecurityinfoforsublayer (HANDLE hengine, const GUID* lpguid, BOOLEAN is_secure);
VOID _app_setsecurityinfoforfilter (HANDLE hengine, const GUID* lpguid, BOOLEAN is_secure, UINT line);
