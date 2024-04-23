// simplewall
// Copyright (c) 2022-2024 Henry++

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
	_Inout_ PR_STRING_PTR name_ptr,
	_Inout_ PR_STRING_PTR path_ptr
)
{
	winrt::Windows::ApplicationModel::Package package = NULL;
	winrt::Windows::ApplicationModel::PackageStatus status = 0;
	winrt::hstring display_name;
	winrt::hstring path;

	package = winrt::Windows::Management::Deployment::PackageManager{}.FindPackage (package_name->buffer);

	if (!package)
		return FALSE;

	if (_r_sys_isosversiongreaterorequal (WINDOWS_10))
	{
		status = package.Status ();

		if (status.Disabled () || status.NotAvailable ())
			return FALSE;
	}

	display_name = package.DisplayName ();
	path = package.InstalledLocation ().Path ();

	if (display_name.empty ())
		display_name = package.Id ().Name ();

	if (display_name.empty ())
	{
		*name_ptr = _r_obj_createstring2 (&package_name->sr);
	}
	else
	{
		if (display_name == L"1527c705-839a-4832-9118-54d4Bd6a0c89")
			display_name = L"File Picker"; // HACK!!!

		*name_ptr = _r_obj_createstring_ex (display_name.c_str (), display_name.size () * sizeof (WCHAR));
	}

	if (!path.empty ())
		*path_ptr = _r_obj_createstring_ex (path.c_str (), path.size () * sizeof (WCHAR));

	return TRUE;
}

_Success_ (return)
BOOLEAN _app_uwp_getpackageinfo (
	_In_ PR_STRING package_name,
	_Out_ PR_STRING_PTR name_ptr,
	_Out_ PR_STRING_PTR path_ptr
)
{
	BOOLEAN status = FALSE;

	*name_ptr = NULL;
	*path_ptr = NULL;

	__try
	{
		status = _app_uwp_loadpackageinfo (package_name, name_ptr, path_ptr);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		*name_ptr = _r_obj_createstring2 (&package_name->sr);

		return TRUE;
	}

	return status;
}
