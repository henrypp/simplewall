// simplewall
// Copyright (c) 2022-2024 Henry++

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

_Success_ (return)
BOOLEAN _app_uwp_loadpackageinfo (
	_In_ PR_STRING package_name,
	_Inout_ PR_STRING_PTR name_ptr,
	_Inout_ PR_STRING_PTR path_ptr
);

_Success_ (return)
BOOLEAN _app_uwp_getpackageinfo (
	_In_ PR_STRING package_name,
	_Out_ PR_STRING_PTR name_ptr,
	_Out_ PR_STRING_PTR path_ptr
);

#ifdef __cplusplus
}
#endif
