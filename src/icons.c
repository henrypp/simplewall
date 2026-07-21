// simplewall
// Copyright (c) 2016-2026 Henry++

#include "global.h"

PICON_INFORMATION _app_icons_getdefault ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static ICON_INFORMATION icon_info = {0};

	PR_STRING path;

	if (_r_initonce_begin (&init_once))
	{
		// load default app icon
		_app_icons_loadfromfile (config.svchost_path, DATA_UNKNOWN, &icon_info.app_icon_id, &icon_info.app_hicon, FALSE);

		// load service icon
		path = _r_obj_concatstrings (
			2,
			_r_sys_getsystemdirectory ()->buffer,
			L"\\shell32.dll"
		);

		_app_icons_loadfromfile (path, DATA_UNKNOWN, &icon_info.service_icon_id, &icon_info.service_hicon, FALSE);

		_r_obj_dereference (path);

		// load uwp icon
		if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
		{
			path = _r_obj_concatstrings (
				2,
				_r_sys_getsystemdirectory ()->buffer,
				L"\\wsreset.exe"
			);

			_app_icons_loadfromfile (path, DATA_UNKNOWN, &icon_info.uwp_icon_id, &icon_info.uwp_hicon, FALSE);

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

	return icon_info->app_hicon ? CopyIcon (icon_info->app_hicon) : NULL;
}

_Ret_maybenull_
HICON _app_icons_getdefaulttype_hicon (
	_In_ ENUM_TYPE_DATA type,
	_In_ PICON_INFORMATION icon_info
)
{
	if (type == DATA_APP_SERVICE)
	{
		if (icon_info->service_hicon)
			return CopyIcon (icon_info->service_hicon);
	}
	else if (type == DATA_APP_UWP)
	{
		if (icon_info->uwp_hicon)
			return CopyIcon (icon_info->uwp_hicon);
	}

	return icon_info->app_hicon ? CopyIcon (icon_info->app_hicon) : NULL;
}

LONG _app_icons_getdefaultapp_id (
	_In_ ENUM_TYPE_DATA type
)
{
	PICON_INFORMATION icon_info;

	icon_info = _app_icons_getdefault ();

	if (type == DATA_APP_SERVICE)
	{
		return icon_info->service_icon_id;
	}
	else if (type == DATA_APP_UWP)
	{
		return icon_info->uwp_icon_id;
	}

	return icon_info->app_icon_id;
}

_Ret_maybenull_
HICON _app_icons_getsafeapp_hicon (
	_In_ ULONG app_hash
)
{
	PICON_INFORMATION icon_info;
	PITEM_APP ptr_app;
	HICON hicon;
	LONG icon_id;

	icon_info = _app_icons_getdefault ();
	ptr_app = _app_getappitem (app_hash);

	if (!ptr_app)
		return icon_info->app_hicon ? CopyIcon (icon_info->app_hicon) : NULL;

	if (_r_obj_isstringempty (ptr_app->real_path))
		return NULL;

	if (_r_config_getboolean (L"IsIconsHidden", FALSE, NULL) || !_app_isappvalidbinary (ptr_app->real_path))
	{
		hicon = _app_icons_getdefaulttype_hicon (ptr_app->type, icon_info);

		_r_obj_dereference (ptr_app);

		return hicon;
	}

	_app_icons_loadfromfile (ptr_app->real_path, ptr_app->type, &icon_id, &hicon, TRUE);

	if (!icon_id || ((ptr_app->type == DATA_APP_SERVICE || ptr_app->type == DATA_APP_UWP) && icon_id == icon_info->app_icon_id))
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
	_Inout_opt_ HICON_PTR out_hicon,
	_Inout_opt_ PLONG out_icon_id
)
{
	PICON_INFORMATION icon_info;

	icon_info = _app_icons_getdefault ();

	if (out_hicon)
	{
		if (*out_hicon == NULL && type == DATA_APP_UWP)
		{
			if (type == DATA_APP_UWP)
			{
				if (icon_info->uwp_hicon)
					*out_hicon = CopyIcon (icon_info->uwp_hicon);
			}
			else
			{
				if (icon_info->app_hicon)
					*out_hicon = CopyIcon (icon_info->app_hicon);
			}
		}
	}

	if (out_icon_id)
	{
		if (*out_icon_id == 0 || ((type == DATA_APP_SERVICE && *out_icon_id == icon_info->app_icon_id) || (type == DATA_APP_UWP && *out_icon_id == icon_info->app_icon_id)))
		{
			if (type == DATA_APP_SERVICE)
			{
				*out_icon_id = icon_info->service_icon_id;
			}
			else if (type == DATA_APP_UWP)
			{
				*out_icon_id = icon_info->uwp_icon_id;
			}
			else
			{
				*out_icon_id = icon_info->app_icon_id;
			}
		}
	}
}

VOID _app_icons_loadfromfile (
	_In_opt_ PR_STRING path,
	_In_ ENUM_TYPE_DATA type,
	_Out_opt_ PLONG out_icon_id,
	_Out_opt_ HICON_PTR out_hicon,
	_In_ BOOLEAN is_loaddefaults
)
{
	if (!out_icon_id && !out_hicon)
		return;

	if (out_icon_id)
		*out_icon_id = 0;

	if (out_hicon)
		*out_hicon = NULL;

	if (!_r_obj_isstringempty (path))
		_r_path_geticon (&path->sr, out_hicon, out_icon_id);

	if (is_loaddefaults)
		_app_icons_loaddefaults (type, out_hicon, out_icon_id);
}
