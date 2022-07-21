// simplewall
// Copyright (c) 2016-2022 Henry++

#pragma once

BOOLEAN _app_package_isnotexists (
	_In_ PR_STRING package_sid,
	_In_opt_ ULONG_PTR app_hash
);

VOID _app_package_parsepath (
	_Inout_ PR_STRING_PTR package_root_folder
);

VOID _app_package_getpackagebyname (
	_In_ HKEY hkey,
	_In_ PR_STRING key_name
);

VOID _app_package_getpackagebysid (
	_In_ HKEY hkey,
	_In_ PR_STRING key_name
);

VOID _app_package_getpackageslist ();
VOID _app_package_getserviceslist ();
