// simplewall
// Copyright (c) 2016-2026 Henry++

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
	_In_ ULONG app_hash
);

VOID _app_icons_loaddefaults (
	_In_ ENUM_TYPE_DATA type,
	_Inout_opt_ HICON_PTR out_hicon,
	_Inout_opt_ PLONG out_icon_id
);

VOID _app_icons_loadfromfile (
	_In_opt_ PR_STRING path,
	_In_ ENUM_TYPE_DATA type,
	_Out_opt_ PLONG out_icon_id,
	_Out_opt_ HICON_PTR out_hicon,
	_In_ BOOLEAN is_loaddefaults
);
