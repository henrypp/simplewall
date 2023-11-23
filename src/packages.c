// simplewall
// Copyright (c) 2016-2023 Henry++

#include "global.h"

BOOLEAN _app_package_isnotexists (
	_In_ PR_STRING package_sid,
	_In_opt_ ULONG_PTR app_hash
)
{
	if (!app_hash)
		app_hash = _r_str_gethash2 (package_sid, TRUE);

	if (_app_isappfound (app_hash))
		return TRUE;

	// there we try to found new package (HACK!!!)
	_app_package_getpackageslist ();

	if (_app_isappfound (app_hash))
		return TRUE;

	return FALSE;
}

VOID _app_package_parsepath (
	_Inout_ PR_STRING_PTR package_root_folder
)
{
	static R_STRINGREF appx_names[] = {
		PR_STRINGREF_INIT (L"AppxManifest.xml"),
		PR_STRINGREF_INIT (L"VSAppxManifest.xml"),
	};

	static R_STRINGREF separator_sr = PR_STRINGREF_INIT (L"\\");

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

		if (_r_fs_exists (manifest_path->buffer))
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

			if (_r_fs_exists (result_path->buffer))
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
	PR_STRING display_name = NULL;
	PR_STRING package_sid_string = NULL;
	PR_STRING real_path = NULL;
	PR_BYTE package_sid = NULL;
	PITEM_APP ptr_app;
	HANDLE hsubkey;
	ULONG_PTR app_hash;
	LONG64 timestamp;
	NTSTATUS status;

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%s\\%s", path, key_name->buffer);

	status = _r_reg_openkey (hroot, buffer, KEY_READ, &hsubkey);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	status = _r_reg_querybinary (hsubkey, L"PackageSid", &package_sid);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	status = _r_str_fromsid (package_sid->buffer, &package_sid_string);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	// already exists (skip)
	app_hash = _r_str_gethash2 (package_sid_string, TRUE);

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
	PR_STRING moniker = NULL;
	PR_BYTE package_sid = NULL;
	PITEM_APP ptr_app;
	HANDLE hsubkey;
	ULONG_PTR app_hash;
	LONG64 timestamp = 0;
	NTSTATUS status;

	// already exists (skip)
	app_hash = _r_str_gethash2 (key_name, TRUE);

	if (_app_isappfound (app_hash))
		return;

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%s\\%s", path, key_name->buffer);

	status = _r_reg_openkey (hroot, buffer, KEY_READ, &hsubkey);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	package_sid = _r_str_tosid (key_name);

	if (!package_sid)
		goto CleanupExit;

	// query package moniker
	status = _r_reg_querystring (hsubkey, L"Moniker", &moniker);

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

VOID _app_package_getpackageslist ()
{
	static LPWSTR reg_byname = L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages";
	static LPWSTR reg_bysid = L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppContainer\\Mappings";

	HANDLE hkey;
	PR_STRING key_name;
	ULONG key_index;
	NTSTATUS status;

	// query packages by name
	status = _r_reg_openkey (HKEY_CURRENT_USER, reg_byname, KEY_READ, &hkey);

	if (!NT_SUCCESS (status))
	{
		_r_log (LOG_LEVEL_WARNING, NULL, L"_r_reg_openkey", status, L"Repository\\Packages");
	}
	else
	{
		key_index = 0;

		while (TRUE)
		{
			status = _r_reg_enumkey (hkey, key_index++, &key_name, NULL);

			if (!NT_SUCCESS (status))
				break;

			_app_package_getpackagebyname (HKEY_CURRENT_USER, reg_byname, key_name);

			_r_obj_dereference (key_name);
		}

		NtClose (hkey);
	}

	// query packages by sid
	status = _r_reg_openkey (HKEY_CURRENT_USER, reg_bysid, KEY_READ, &hkey);

	if (!NT_SUCCESS (status))
	{
		_r_log (LOG_LEVEL_WARNING, NULL, L"_r_reg_openkey", status, L"AppContainer\\Mappings");
	}
	else
	{
		key_index = 0;

		while (TRUE)
		{
			status = _r_reg_enumkey (hkey, key_index++, &key_name, NULL);

			if (!NT_SUCCESS (status))
				break;

			_app_package_getpackagebysid (HKEY_CURRENT_USER, reg_bysid, key_name);

			_r_obj_dereference (key_name);
		}

		NtClose (hkey);
	}
}

VOID _app_package_getserviceslist ()
{
	static ULONG initial_buffer_size = 0x8000;

	SC_HANDLE hsvcmgr;
	WCHAR general_key[256];
	EXPLICIT_ACCESS ea;
	LPENUM_SERVICE_STATUS_PROCESS service;
	LPENUM_SERVICE_STATUS_PROCESS services;
	PVOID service_sd;
	PR_STRING service_name;
	PR_STRING service_path;
	PR_BYTE service_sid;
	LONG64 service_timestamp;
	ULONG_PTR app_hash;
	ULONG service_type;
	ULONG service_state;
	ULONG sd_length;
	PR_STRING name_string;
	PITEM_APP ptr_app;
	R_STRINGREF dummy_filename;
	R_STRINGREF dummy_argument;
	PR_STRING converted_path;
	PVOID buffer;
	HANDLE hkey;
	ULONG buffer_size;
	ULONG return_length;
	ULONG services_returned;
	NTSTATUS status;

	hsvcmgr = OpenSCManager (NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE);

	if (!hsvcmgr)
		return;

	service_type = SERVICE_WIN32_OWN_PROCESS | SERVICE_WIN32_SHARE_PROCESS;
	service_state = SERVICE_STATE_ALL;

	// win10+
	if (_r_sys_isosversiongreaterorequal (WINDOWS_10))
		service_type |= SERVICE_INTERACTIVE_PROCESS | SERVICE_USER_SERVICE;

	buffer_size = initial_buffer_size;
	buffer = _r_mem_allocate (buffer_size);

	if (!EnumServicesStatusEx (hsvcmgr, SC_ENUM_PROCESS_INFO, service_type, service_state, buffer, buffer_size, &return_length, &services_returned, NULL, NULL))
	{
		if (PebLastError () == ERROR_MORE_DATA)
		{
			// Set the buffer
			buffer_size += return_length;
			buffer = _r_mem_reallocate (buffer, buffer_size);

			// Now query again for services
			if (!EnumServicesStatusEx (hsvcmgr, SC_ENUM_PROCESS_INFO, service_type, service_state, buffer, buffer_size, &return_length, &services_returned, NULL, NULL))
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

		status = _r_reg_openkey (HKEY_LOCAL_MACHINE, general_key, KEY_READ, &hkey);

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
		status = _r_reg_querystring (hkey, L"ImagePath", &service_path);

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

				status = BuildSecurityDescriptor (NULL, NULL, 1, &ea, 0, NULL, NULL, &sd_length, &service_sd);

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
