// simplewall
// Copyright (c) 2022 Henry++

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

BOOLEAN _app_package_getpackage_info (
	_In_ PR_STRING package_name,
	_Out_ PR_STRING_PTR name_ptr,
	_Out_ PR_STRING_PTR path_ptr
);

#ifdef __cplusplus
}
#endif
