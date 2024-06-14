// simplewall
// Copyright (c) 2016-2024 Henry++

#pragma once

PICON_INFORMATION _app_icons_getdefault ();

_Ret_maybenull_
HICON _app_icons_getdefaultapp_hicon ();

_Ret_maybenull_
HICON _app_icons_getdefaulttype_hicon (
	_In_ ENUM_TYPE_DATA type,
	_In_ PICON_INFORMATION icon_info
);

LONG _app_icons_getdefaultapp_id (
	_In_ ENUM_TYPE_DATA type
);

_Ret_maybenull_
HICON _app_icons_getsafeapp_hicon (
	_In_ ULONG_PTR app_hash
);

VOID _app_icons_loaddefaults (
	_In_ ENUM_TYPE_DATA type,
	_Inout_opt_ PLONG icon_id_ptr,
	_Inout_opt_ HICON_PTR hicon_ptr
);

VOID _app_icons_loadfromfile (
	_In_opt_ PR_STRING path,
	_In_ ENUM_TYPE_DATA type,
	_Out_opt_ PLONG icon_id_ptr,
	_Out_opt_ HICON_PTR hicon_ptr,
	_In_ BOOLEAN is_loaddefaults
);
