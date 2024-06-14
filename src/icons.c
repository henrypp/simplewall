// simplewall
// Copyright (c) 2016-2024 Henry++

#include "global.h"

PICON_INFORMATION _app_icons_getdefault ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static ICON_INFORMATION icon_info = {0};

	PR_STRING path;

	if (_r_initonce_begin (&init_once))
	{
		// load default icons
		path = _r_obj_concatstrings (
			2,
			_r_sys_getsystemdirectory ()->buffer,
			PATH_SVCHOST
		);

		_app_icons_loadfromfile (path, 0, &icon_info.app_icon_id, &icon_info.app_hicon, FALSE);

		// load default service icons
		_r_obj_dereference (path);

		path = _r_obj_concatstrings (
			2,
			_r_sys_getsystemdirectory ()->buffer,
			L"\\shell32.dll"
		);

		_app_icons_loadfromfile (path, 0, &icon_info.service_icon_id, &icon_info.service_hicon, FALSE);

		_r_obj_dereference (path);

		// load uwp icons
		if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
		{
			path = _r_obj_concatstrings (
				2,
				_r_sys_getsystemdirectory ()->buffer,
				L"\\wsreset.exe"
			);

			_app_icons_loadfromfile (path, 0, &icon_info.uwp_icon_id, &icon_info.uwp_hicon, FALSE);

			_r_obj_dereference (path);
		}

		_r_initonce_end (&init_once);
	}

	return &icon_info;
}

_Ret_maybenull_
HICON _app_icons_getdefaultapp_hicon ()
{
	PICON_INFORMATION icon_info;

	icon_info = _app_icons_getdefault ();

	if (icon_info->app_hicon)
		return CopyIcon (icon_info->app_hicon);

	return NULL;
}

_Ret_maybenull_
HICON _app_icons_getdefaulttype_hicon (
	_In_ ENUM_TYPE_DATA type,
	_In_ PICON_INFORMATION icon_info
)
{
	if (type == DATA_APP_UWP)
	{
		if (icon_info->uwp_hicon)
			return CopyIcon (icon_info->uwp_hicon);
	}
	else if (type == DATA_APP_SERVICE)
	{
		if (icon_info->service_hicon)
			return CopyIcon (icon_info->service_hicon);
	}

	if (icon_info->app_hicon)
		return CopyIcon (icon_info->app_hicon);

	return NULL;
}

LONG _app_icons_getdefaultapp_id (
	_In_ ENUM_TYPE_DATA type
)
{
	PICON_INFORMATION icon_info;

	icon_info = _app_icons_getdefault ();

	if (type == DATA_APP_UWP)
	{
		return icon_info->uwp_icon_id;
	}
	else if (type == DATA_APP_SERVICE)
	{
		return icon_info->service_icon_id;
	}

	return icon_info->app_icon_id;
}

_Ret_maybenull_
HICON _app_icons_getsafeapp_hicon (
	_In_ ULONG_PTR app_hash
)
{
	PICON_INFORMATION icon_info;
	PITEM_APP ptr_app;
	HICON hicon;
	LONG icon_id;
	BOOLEAN is_iconshidded;

	icon_info = _app_icons_getdefault ();
	ptr_app = _app_getappitem (app_hash);

	if (!ptr_app)
	{
		if (icon_info->app_hicon)
			return CopyIcon (icon_info->app_hicon);

		return NULL;
	}

	is_iconshidded = _r_config_getboolean (L"IsIconsHidden", FALSE);

	if (is_iconshidded || !_app_isappvalidbinary (ptr_app->real_path))
	{
		hicon = _app_icons_getdefaulttype_hicon (ptr_app->type, icon_info);

		_r_obj_dereference (ptr_app);

		return hicon;
	}

	_app_icons_loadfromfile (ptr_app->real_path, ptr_app->type, &icon_id, &hicon, TRUE);

	if (!icon_id || ((ptr_app->type == DATA_APP_UWP || ptr_app->type == DATA_APP_SERVICE) && icon_id == icon_info->app_icon_id))
	{
		if (hicon)
			DestroyIcon (hicon);

		hicon = _app_icons_getdefaulttype_hicon (ptr_app->type, icon_info);
	}

	_r_obj_dereference (ptr_app);

	return hicon;
}

VOID _app_icons_loaddefaults (
	_In_ ENUM_TYPE_DATA type,
	_Inout_opt_ PLONG icon_id_ptr,
	_Inout_opt_ HICON_PTR hicon_ptr
)
{
	PICON_INFORMATION icon_info;

	icon_info = _app_icons_getdefault ();

	if (icon_id_ptr)
	{
		if (*icon_id_ptr == 0 || (type == DATA_APP_UWP && *icon_id_ptr == icon_info->app_icon_id) || (type == DATA_APP_SERVICE && *icon_id_ptr == icon_info->app_icon_id))
		{
			if (type == DATA_APP_UWP)
			{
				*icon_id_ptr = icon_info->uwp_icon_id;
			}
			else if (type == DATA_APP_SERVICE)
			{
				*icon_id_ptr = icon_info->service_icon_id;
			}
			else
			{
				*icon_id_ptr = icon_info->app_icon_id;
			}
		}
	}

	if (hicon_ptr)
	{
		if (*hicon_ptr == NULL || type == DATA_APP_UWP)
		{
			if (type == DATA_APP_UWP)
			{
				if (icon_info->uwp_hicon)
					*hicon_ptr = CopyIcon (icon_info->uwp_hicon);
			}
			else
			{
				if (icon_info->app_hicon)
					*hicon_ptr = CopyIcon (icon_info->app_hicon);
			}
		}
	}
}

VOID _app_icons_loadfromfile (
	_In_opt_ PR_STRING path,
	_In_ ENUM_TYPE_DATA type,
	_Out_opt_ PLONG icon_id_ptr,
	_Out_opt_ HICON_PTR hicon_ptr,
	_In_ BOOLEAN is_loaddefaults
)
{
	if (!icon_id_ptr && !hicon_ptr)
		return;

	if (icon_id_ptr)
		*icon_id_ptr = 0;

	if (hicon_ptr)
		*hicon_ptr = NULL;

	if (path)
		_r_path_geticon (path->buffer, icon_id_ptr, hicon_ptr);

	if (is_loaddefaults)
		_app_icons_loaddefaults (type, icon_id_ptr, hicon_ptr);
}
