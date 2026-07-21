// simplewall
// Copyright (c) 2022-2026 Henry++

#pragma once

EXTERN_C_START

_Success_ (return)
BOOLEAN _app_uwp_loadpackageinfo (
	_In_ PR_STRING package_name,
	_Inout_ PR_STRING_PTR out_name,
	_Inout_ PR_STRING_PTR out_path
);

_Success_ (return)
BOOLEAN _app_uwp_getpackageinfo (
	_In_ PR_STRING package_name,
	_Out_ PR_STRING_PTR out_name,
	_Out_ PR_STRING_PTR out_path
);

EXTERN_C_END
