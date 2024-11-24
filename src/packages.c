// simplewall
// Copyright (c) 2016-2024 Henry++

#include "global.h"

BOOLEAN _app_package_isnotexists (
	_In_ PR_STRING package_sid,
	_In_opt_ ULONG_PTR app_hash
)
{
	if (!app_hash)
		app_hash = _r_str_gethash2 (&package_sid->sr, TRUE);

	if (_app_isappfound (app_hash))
		return TRUE;

	// there we try to found new package (HACK!!!)
	_app_package_getpackageslist (_r_app_gethwnd ());

	if (_app_isappfound (app_hash))
		return TRUE;

	return FALSE;
}

VOID _app_package_parsepath (
	_Inout_ PR_STRING_PTR package_root_folder
)
{
	R_STRINGREF appx_names[] = {
		PR_STRINGREF_INIT (L"AppxManifest.xml"),
		PR_STRINGREF_INIT (L"VSAppxManifest.xml"),
	};

	R_STRINGREF separator_sr = PR_STRINGREF_INIT (L"\\");
	R_XML_LIBRARY xml_library = {0};
	PR_STRING manifest_path = NULL;
	PR_STRING result_path = NULL;
	PR_STRING path_string;
	R_STRINGREF executable_sr;
	HRESULT status;
	BOOLEAN is_success = FALSE;

	path_string = *package_root_folder;

	for (ULONG_PTR i = 0; i < RTL_NUMBER_OF (appx_names); i++)
	{
		_r_obj_movereference (&manifest_path, _r_obj_concatstringrefs (3, &path_string->sr, &separator_sr, &appx_names[i]));

		if (_r_fs_exists (&manifest_path->sr))
		{
			is_success = TRUE;

			break;
		}
	}

	if (!is_success)
		goto CleanupExit;

	status = _r_xml_initializelibrary (&xml_library, TRUE);

	if (FAILED (status))
		goto CleanupExit;

	status = _r_xml_parsefile (&xml_library, manifest_path->buffer);

	if (FAILED (status))
		goto CleanupExit;

	if (_r_xml_findchildbytagname (&xml_library, L"Applications"))
	{
		while (_r_xml_enumchilditemsbytagname (&xml_library, L"Application"))
		{
			if (FAILED (_r_xml_getattribute (&xml_library, L"Executable", &executable_sr)))
				continue;

			_r_obj_movereference (&result_path, _r_obj_concatstringrefs (3, &path_string->sr, &separator_sr, &executable_sr));

			if (_r_fs_exists (&result_path->sr))
			{
				_r_obj_swapreference (package_root_folder, result_path);

				break;
			}
		}
	}

CleanupExit:

	if (result_path)
		_r_obj_dereference (result_path);

	if (manifest_path)
		_r_obj_dereference (manifest_path);

	_r_xml_destroylibrary (&xml_library);
}

VOID _app_package_getpackagebyname (
	_In_ HANDLE hroot,
	_In_ LPCWSTR path,
	_In_ PR_STRING key_name
)
{
	WCHAR buffer[256];
	PR_STRING package_sid_string = NULL;
	PR_STRING display_name = NULL;
	PR_STRING real_path = NULL;
	PR_BYTE package_sid = NULL;
	PITEM_APP ptr_app;
	HANDLE hsubkey;
	ULONG_PTR app_hash;
	LONG64 timestamp;
	NTSTATUS status;

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%s\\%s", path, key_name->buffer);

	status = _r_reg_openkey (hroot, buffer, 0, KEY_READ, &hsubkey);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	status = _r_reg_querybinary (hsubkey, L"PackageSid", &package_sid);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	status = _r_str_fromsid (package_sid->buffer, &package_sid_string);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	// already exists (skip)
	app_hash = _r_str_gethash2 (&package_sid_string->sr, TRUE);

	if (_app_isappfound (app_hash))
		goto CleanupExit;

	// parse package information
	if (!_app_uwp_getpackageinfo (key_name, &display_name, &real_path))
		goto CleanupExit;

	if (real_path)
		_app_package_parsepath (&real_path);

	//_r_queuedlock_acquireexclusive (&lock_apps);
	app_hash = _app_addapplication (NULL, DATA_APP_UWP, package_sid_string, display_name, real_path);
	//_r_queuedlock_releaseexclusive (&lock_apps);

	if (app_hash)
	{
		ptr_app = _app_getappitem (app_hash);

		if (ptr_app)
		{
			status = _r_reg_queryinfo (hsubkey, NULL, &timestamp);

			if (NT_SUCCESS (status))
				_app_setappinfo (ptr_app, INFO_TIMESTAMP, &timestamp);

			_app_setappinfo (ptr_app, INFO_BYTES_DATA, _r_obj_reference (package_sid));

			_r_obj_dereference (ptr_app);
		}
	}

CleanupExit:

	if (display_name)
		_r_obj_dereference (display_name);

	if (package_sid)
		_r_obj_dereference (package_sid);

	if (package_sid_string)
		_r_obj_dereference (package_sid_string);

	if (real_path)
		_r_obj_dereference (real_path);

	if (hsubkey)
		NtClose (hsubkey);
}

VOID _app_package_getpackagebysid (
	_In_ HANDLE hroot,
	_In_ LPCWSTR path,
	_In_ PR_STRING key_name
)
{
	WCHAR buffer[256];
	PR_STRING display_name = NULL;
	PR_STRING real_path = NULL;
	PR_BYTE package_sid = NULL;
	PR_STRING moniker = NULL;
	PITEM_APP ptr_app;
	HANDLE hsubkey;
	ULONG_PTR app_hash;
	LONG64 timestamp = 0;
	NTSTATUS status;

	// already exists (skip)
	app_hash = _r_str_gethash2 (&key_name->sr, TRUE);

	if (_app_isappfound (app_hash))
		return;

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%s\\%s", path, key_name->buffer);

	status = _r_reg_openkey (hroot, buffer, 0, KEY_READ, &hsubkey);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	package_sid = _r_str_tosid (key_name);

	if (!package_sid)
		goto CleanupExit;

	// query package moniker
	status = _r_reg_querystring (hsubkey, L"Moniker", TRUE, &moniker);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	// parse package information
	if (!_app_uwp_getpackageinfo (moniker, &display_name, &real_path))
		goto CleanupExit;

	if (real_path)
		_app_package_parsepath (&real_path);

	//_r_queuedlock_acquireexclusive (&lock_apps);
	app_hash = _app_addapplication (NULL, DATA_APP_UWP, key_name, display_name, NULL);
	//_r_queuedlock_releaseexclusive (&lock_apps);

	if (app_hash)
	{
		ptr_app = _app_getappitem (app_hash);

		if (ptr_app)
		{
			status = _r_reg_queryinfo (hsubkey, NULL, &timestamp);

			if (NT_SUCCESS (status))
				_app_setappinfo (ptr_app, INFO_TIMESTAMP, &timestamp);

			_app_setappinfo (ptr_app, INFO_BYTES_DATA, _r_obj_reference (package_sid));

			_r_obj_dereference (ptr_app);
		}
	}

CleanupExit:

	if (moniker)
		_r_obj_dereference (moniker);

	if (display_name)
		_r_obj_dereference (display_name);

	if (real_path)
		_r_obj_dereference (real_path);

	if (package_sid)
		_r_obj_dereference (package_sid);

	if (hsubkey)
		NtClose (hsubkey);
}

NTSTATUS NTAPI _app_package_threadproc (
	_In_ PVOID arglist
)
{
	IO_STATUS_BLOCK isb;
	HANDLE hservices_key = NULL;
	HANDLE hpackages_key = NULL;
	HANDLE event_handle1 = NULL;
	HANDLE event_handle2 = NULL;
	HANDLE hevents[2] = {0};
	PITEM_APP ptr_app = NULL;
	HWND hwnd;
	LONG64 current_time;
	ULONG_PTR enum_key = 0;
	ULONG flags = REG_NOTIFY_CHANGE_NAME;
	NTSTATUS status;

	hwnd = arglist;

	status = NtCreateEvent (&event_handle1, EVENT_ALL_ACCESS, NULL, NotificationEvent, FALSE);

	if (!NT_SUCCESS (status))
	{
		_r_show_errormessage (hwnd, NULL, status, L"NtCreateEvent", ET_NATIVE);

		return status;
	}

	status = NtCreateEvent (&event_handle2, EVENT_ALL_ACCESS, NULL, NotificationEvent, FALSE);

	if (!NT_SUCCESS (status))
	{
		_r_show_errormessage (hwnd, NULL, status, L"NtCreateEvent", ET_NATIVE);

		return status;
	}

	hevents[0] = event_handle1;
	hevents[1] = event_handle2;

	status = _r_reg_openkey (HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services", 0, KEY_NOTIFY, &hservices_key);

	if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
	{
		flags |= REG_NOTIFY_THREAD_AGNOSTIC;

		_r_reg_openkey (HKEY_CURRENT_USER, L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages", 0, KEY_NOTIFY, &hpackages_key);
	}

	if (NT_SUCCESS (status))
	{
		NtNotifyChangeKey (hservices_key, event_handle1, NULL, NULL, &isb, flags, TRUE, NULL, 0, TRUE);

		if (hpackages_key)
			NtNotifyChangeKey (hpackages_key, event_handle2, NULL, NULL, &isb, flags, TRUE, NULL, 0, TRUE);

		while (TRUE)
		{
			status = _r_sys_waitformultipleobjects (RTL_NUMBER_OF (hevents), hevents, INFINITE, FALSE);

			if (status == STATUS_WAIT_0)
			{
				_r_listview_deleteallitems (hwnd, IDC_APPS_SERVICE);

				_app_package_getserviceslist (hwnd);

				// add apps
				current_time = _r_unixtime_now ();

				enum_key = 0;

				_r_queuedlock_acquireshared (&lock_apps);

				while (_r_obj_enumhashtablepointer (apps_table, &ptr_app, NULL, &enum_key))
				{
					if (ptr_app->type == DATA_APP_SERVICE)
					{
						_app_listview_addappitem (hwnd, ptr_app);

						// install timer
						if (ptr_app->timer)
							_app_timer_set (hwnd, ptr_app, ptr_app->timer - current_time);
					}
				}

				_r_queuedlock_releaseshared (&lock_apps);

				NtNotifyChangeKey (hservices_key, event_handle1, NULL, NULL, &isb, flags, TRUE, NULL, 0, TRUE); // reset
			}
			else if (status == STATUS_WAIT_1)
			{
				_r_listview_deleteallitems (hwnd, IDC_APPS_UWP);

				_app_package_getpackageslist (hwnd);

				// add apps
				current_time = _r_unixtime_now ();

				enum_key = 0;

				_r_queuedlock_acquireshared (&lock_apps);

				while (_r_obj_enumhashtablepointer (apps_table, &ptr_app, NULL, &enum_key))
				{
					if (ptr_app->type == DATA_APP_UWP)
					{
						_app_listview_addappitem (hwnd, ptr_app);

						// install timer
						if (ptr_app->timer)
							_app_timer_set (hwnd, ptr_app, ptr_app->timer - current_time);
					}
				}

				_r_queuedlock_releaseshared (&lock_apps);

				if (hpackages_key)
					NtNotifyChangeKey (hpackages_key, event_handle2, NULL, NULL, &isb, flags, TRUE, NULL, 0, TRUE); // reset
			}
			else
			{
				break;
			}
		}
	}

	if (hservices_key)
		NtClose (hservices_key);

	if (hpackages_key)
		NtClose (hpackages_key);

	NtClose (event_handle1);
	NtClose (event_handle2);

	return STATUS_SUCCESS;
}

VOID _app_package_getpackageslist (
	_In_ HWND hwnd
)
{
	LPWSTR reg_byname = L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages";
	LPWSTR reg_bysid = L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppContainer\\Mappings";
	PR_STRING name;
	HANDLE hkey;
	ULONG index = 0;
	NTSTATUS status;

	// query packages by name
	status = _r_reg_openkey (HKEY_CURRENT_USER, reg_byname, 0, KEY_READ, &hkey);

	if (!NT_SUCCESS (status))
	{
		if (status != STATUS_OBJECT_NAME_NOT_FOUND)
			_r_show_errormessage (hwnd, NULL, status, reg_byname, ET_NATIVE);
	}
	else
	{
		while (TRUE)
		{
			status = _r_reg_enumkey (hkey, index, &name, NULL);

			if (!NT_SUCCESS (status))
				break;

			_app_package_getpackagebyname (HKEY_CURRENT_USER, reg_byname, name);

			_r_obj_dereference (name);

			index += 1;
		}

		NtClose (hkey);
	}

	// query packages by sid
	status = _r_reg_openkey (HKEY_CURRENT_USER, reg_bysid, 0, KEY_READ, &hkey);

	if (!NT_SUCCESS (status))
	{
		if (status != STATUS_OBJECT_NAME_NOT_FOUND)
			_r_show_errormessage (hwnd, NULL, status, reg_bysid, ET_NATIVE);
	}
	else
	{
		index = 0;

		while (TRUE)
		{
			status = _r_reg_enumkey (hkey, index, &name, NULL);

			if (!NT_SUCCESS (status))
				break;

			_app_package_getpackagebysid (HKEY_CURRENT_USER, reg_bysid, name);

			_r_obj_dereference (name);

			index += 1;
		}

		NtClose (hkey);
	}
}

VOID _app_package_getserviceslist (
	_In_ HWND hwnd
)
{
	WCHAR general_key[256];
	LPENUM_SERVICE_STATUS_PROCESS services;
	LPENUM_SERVICE_STATUS_PROCESS service;
	SERVICE_NOTIFY notify_context = {0};
	EXPLICIT_ACCESS ea;
	PR_STRING name_string;
	PR_STRING service_name;
	PR_STRING service_path;
	PR_BYTE service_sid;
	SC_HANDLE hsvcmgr;
	PITEM_APP ptr_app;
	PVOID service_sd;
	PVOID buffer;
	HANDLE hkey;
	LONG64 service_timestamp;
	ULONG_PTR app_hash;
	ULONG service_type = SERVICE_WIN32_OWN_PROCESS | SERVICE_WIN32_SHARE_PROCESS;
	ULONG service_state = SERVICE_STATE_ALL;
	ULONG sd_length;
	R_STRINGREF dummy_filename;
	R_STRINGREF dummy_argument;
	PR_STRING converted_path;
	ULONG services_returned;
	ULONG return_length;
	ULONG buffer_size;
	NTSTATUS status;

	hsvcmgr = OpenSCManagerW (NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE);

	if (!hsvcmgr)
	{
		_r_show_errormessage (hwnd, NULL, NtLastError (), L"OpenSCManagerW", ET_WINDOWS);

		return;
	}

	// win10+
	if (_r_sys_isosversiongreaterorequal (WINDOWS_10))
		service_type |= SERVICE_INTERACTIVE_PROCESS | SERVICE_USER_SERVICE;

	buffer_size = PR_SIZE_BUFFER;
	buffer = _r_mem_allocate (buffer_size);

	if (!EnumServicesStatusExW (hsvcmgr, SC_ENUM_PROCESS_INFO, service_type, service_state, buffer, buffer_size, &return_length, &services_returned, NULL, NULL))
	{
		if (NtLastError () == ERROR_MORE_DATA)
		{
			// set the buffer
			buffer_size += return_length;
			buffer = _r_mem_reallocate (buffer, buffer_size);

			// now query again for services
			if (!EnumServicesStatusExW (hsvcmgr, SC_ENUM_PROCESS_INFO, service_type, service_state, buffer, buffer_size, &return_length, &services_returned, NULL, NULL))
			{
				_r_mem_free (buffer);

				buffer = NULL;
			}
		}
		else
		{
			_r_mem_free (buffer);

			buffer = NULL;
		}
	}

	if (!buffer)
	{
		CloseServiceHandle (hsvcmgr);

		return;
	}

	// now traverse each service to get information
	services = (LPENUM_SERVICE_STATUS_PROCESS)buffer;

	for (ULONG i = 0; i < services_returned; i++)
	{
		service = &services[i];

		app_hash = _r_str_gethash (service->lpServiceName, TRUE);

		if (_app_isappfound (app_hash))
			continue;

		_r_str_printf (general_key, RTL_NUMBER_OF (general_key), L"System\\CurrentControlSet\\Services\\%s", service->lpServiceName);

		service_name = _r_obj_createstring (service->lpServiceName);

		status = _r_reg_openkey (HKEY_LOCAL_MACHINE, general_key, 0, KEY_READ, &hkey);

		if (!NT_SUCCESS (status))
		{
			_r_obj_dereference (service_name);

			continue;
		}

		// skip userservice instances service types (win10+)
		if (_r_sys_isosversiongreaterorequal (WINDOWS_10))
		{
			status = _r_reg_queryulong (hkey, L"Type", &service_type);

			if (!NT_SUCCESS (status) || (service_type & SERVICE_USERSERVICE_INSTANCE) != 0)
			{
				_r_obj_dereference (service_name);

				NtClose (hkey);

				continue;
			}
		}

		// query service path
		status = _r_reg_querystring (hkey, L"ImagePath", TRUE, &service_path);

		if (NT_SUCCESS (status))
		{
			_r_path_parsecommandlinefuzzy (&service_path->sr, &dummy_filename, &dummy_argument, &converted_path);

			if (converted_path)
			{
				_r_obj_movereference (&service_path, converted_path);
			}
			else
			{
				converted_path = _r_path_dospathfromnt (service_path);

				if (converted_path)
					_r_obj_movereference (&service_path, converted_path);
			}

			// query service sid
			status = _r_sys_getservicesid (service->lpServiceName, &service_sid);

			if (NT_SUCCESS (status))
			{
				// When evaluating SECURITY_DESCRIPTOR conditions, the filter engine
				// checks for FWP_ACTRL_MATCH_FILTER access. If the DACL grants access,
				// it does not mean that the traffic is allowed; it just means that the
				// condition evaluates to true. Likewise if it denies access, the
				// condition evaluates to false.

				_app_setexplicitaccess (&ea, GRANT_ACCESS, FWP_ACTRL_MATCH_FILTER, NO_INHERITANCE, service_sid->buffer);

				// Security descriptors must be in self-relative form (i.e., contiguous).
				// The security descriptor returned by BuildSecurityDescriptorW is
				// already self-relative, but if you're using another mechanism to build
				// the descriptor, you may have to convert it. See MakeSelfRelativeSD for
				// details.

				status = BuildSecurityDescriptorW (NULL, NULL, 1, &ea, 0, NULL, NULL, &sd_length, &service_sd);

				if (status == ERROR_SUCCESS && service_sd)
				{
					name_string = _r_obj_createstring (service->lpDisplayName);

					app_hash = _app_addapplication (NULL, DATA_APP_SERVICE, service_name, name_string, service_path);

					if (app_hash)
					{
						ptr_app = _app_getappitem (app_hash);

						if (ptr_app)
						{
							// query service timestamp
							status = _r_reg_queryinfo (hkey, NULL, &service_timestamp);

							if (NT_SUCCESS (status))
								_app_setappinfo (ptr_app, INFO_TIMESTAMP, &service_timestamp);

							_app_setappinfo (ptr_app, INFO_BYTES_DATA, _r_obj_createbyte_ex (service_sd, sd_length));

							_r_obj_dereference (ptr_app);
						}
					}

					LocalFree (service_sd);

					_r_obj_dereference (name_string);
				}

				_r_obj_dereference (service_sid);
			}

			_r_obj_dereference (service_path);
		}

		_r_obj_dereference (service_name);

		NtClose (hkey);
	}

	_r_mem_free (buffer);

	CloseServiceHandle (hsvcmgr);
}
