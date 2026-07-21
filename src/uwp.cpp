// simplewall
// Copyright (c) 2022-2026 Henry++

#include "routine.h"

#include <winrt/windows.foundation.h>
#include <winrt/windows.foundation.collections.h>
#include <winrt/windows.applicationmodel.h>
#include <winrt/windows.management.deployment.h>
#include <winrt/windows.storage.h>
#include <winrt/windows.storage.streams.h>

#include "uwp.h"

_Success_ (return)
BOOLEAN _app_uwp_loadpackageinfo (
	_In_ PR_STRING package_name,
	_Inout_ PR_STRING_PTR out_name,
	_Inout_ PR_STRING_PTR out_path
)
{
	winrt::Windows::ApplicationModel::PackageStatus package_status = 0;
	winrt::Windows::ApplicationModel::Package package = NULL;
	winrt::hstring display_name, path;

	package = winrt::Windows::Management::Deployment::PackageManager{}.FindPackage (package_name->buffer);

	if (!package)
		return FALSE;

	if (_r_sys_isosversiongreaterorequal (WINDOWS_10))
	{
		package_status = package.Status ();

		if (package_status.Disabled () || package_status.NotAvailable ())
			return FALSE;
	}

	display_name = package.DisplayName ();
	path = package.InstalledLocation ().Path ();

	if (display_name.empty ())
		display_name = package.Id ().Name ();

	if (display_name.empty ())
	{
		*out_name = _r_obj_createstring2 (&package_name->sr);
	}
	else
	{
		if (_r_str_compare (display_name.c_str (), L"1527C705-839A-4832-9118-54D4BD6A0C89", TRUE) == 0)
			display_name = L"File Picker"; // HACK!!!

		*out_name = _r_obj_createstring_ex (display_name.c_str (), display_name.size () * sizeof (WCHAR));
	}

	if (!path.empty ())
		*out_path = _r_obj_createstring_ex (path.c_str (), path.size () * sizeof (WCHAR));

	return TRUE;
}

_Success_ (return)
BOOLEAN _app_uwp_getpackageinfo (
	_In_ PR_STRING package_name,
	_Out_ PR_STRING_PTR out_name,
	_Out_ PR_STRING_PTR out_path
)
{
	*out_name = NULL;
	*out_path = NULL;

	__try
	{
		return _app_uwp_loadpackageinfo (package_name, out_name, out_path);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		*out_name = _r_obj_createstring2 (&package_name->sr);

		return TRUE;
	}
}
