// simplewall
// Copyright (c) 2022 Henry++

#include "routine.h"

#include <winrt/windows.foundation.h>
#include <winrt/windows.foundation.collections.h>
#include <winrt/windows.applicationmodel.h>
#include <winrt/windows.management.deployment.h>
#include <winrt/windows.storage.h>
#include <winrt/windows.storage.streams.h>

#include "uwp.h"

BOOLEAN _app_package_getpackage_info (
	_In_ PR_STRING package_name,
	_Out_ PR_STRING_PTR name_ptr,
	_Out_ PR_STRING_PTR path_ptr
)
{
	winrt::Windows::ApplicationModel::Package package = NULL;
	winrt::hstring display_name;
	winrt::hstring path;

	*name_ptr = NULL;
	*path_ptr = NULL;

	package = winrt::Windows::Management::Deployment::PackageManager{}.FindPackage (package_name->buffer);

	if (!package)
		return FALSE;

	display_name = package.DisplayName ();
	path = package.InstalledLocation ().Path ();

	if (display_name.empty ())
		display_name = package.Id ().Name ();

	if (display_name.empty ())
	{
		*name_ptr = _r_obj_createstring2 (package_name);
	}
	else
	{
		*name_ptr = _r_obj_createstring_ex (
			display_name.c_str (),
			display_name.size () * sizeof (WCHAR)
		);
	}

	if (!path.empty ())
	{
		*path_ptr = _r_obj_createstring_ex (
			path.c_str (),
			path.size () * sizeof (WCHAR)
		);
	}

	return TRUE;
}
