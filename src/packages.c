// simplewall
// Copyright (c) 2016-2022 Henry++

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

VOID _app_package_parsedisplayname (
	_In_ PR_STRING_PTR display_name_ptr,
	_In_ PR_STRING fallback_name
)
{
	PR_STRING display_name;
	PR_STRING buffer_string;
	ULONG buffer_length;
	SIZE_T pos;
	HRESULT hr;

	display_name = *display_name_ptr;

	if (!_r_obj_isstringempty (display_name))
	{
		if (display_name->buffer[0] == L'@')
		{
			buffer_length = 512;
			buffer_string = _r_obj_createstring_ex (NULL, buffer_length * sizeof (WCHAR));

			hr = SHLoadIndirectString (display_name->buffer, buffer_string->buffer, buffer_length, NULL);

			if (SUCCEEDED (hr))
			{
				_r_obj_trimstringtonullterminator (buffer_string);

				_r_obj_movereference (display_name_ptr, buffer_string);
			}
			else
			{
				_r_obj_dereference (buffer_string);
			}
		}
		else if (_r_str_isstartswith2 (&display_name->sr, L"ms-resource:", TRUE))
		{
			_r_obj_movereference (display_name_ptr, _r_obj_createstring2 (fallback_name));

			display_name = *display_name_ptr;

			pos = _r_str_findchar (&display_name->sr, L'_', FALSE);

			if (pos != SIZE_MAX)
				_r_obj_setstringlength (display_name, pos * sizeof (WCHAR));
		}
	}

	if (_r_obj_isstringempty (*display_name_ptr))
		_r_obj_swapreference (display_name_ptr, fallback_name);
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
	PR_STRING manifest_path;
	PR_STRING result_path;
	PR_STRING path_string;
	R_STRINGREF executable_sr;
	HRESULT hr;
	BOOLEAN is_success;

	path_string = *package_root_folder;

	manifest_path = NULL;
	result_path = NULL;

	is_success = FALSE;

	for (SIZE_T i = 0; i < RTL_NUMBER_OF (appx_names); i++)
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

	hr = _r_xml_initializelibrary (&xml_library, TRUE);

	if (hr != S_OK)
		goto CleanupExit;

	hr = _r_xml_parsefile (&xml_library, manifest_path->buffer);

	if (hr != S_OK)
		goto CleanupExit;

	if (!_r_xml_findchildbytagname (&xml_library, L"Applications"))
		goto CleanupExit;

	while (_r_xml_enumchilditemsbytagname (&xml_library, L"Application"))
	{
		if (!_r_xml_getattribute (&xml_library, L"Executable", &executable_sr))
			continue;

		_r_obj_movereference (&result_path, _r_obj_concatstringrefs (3, &path_string->sr, &separator_sr, &executable_sr));

		if (_r_fs_exists (result_path->buffer))
		{
			_r_obj_swapreference (package_root_folder, result_path);
			break;
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
	_In_ HKEY hkey,
	_In_ PR_STRING key_name
)
{
	PR_STRING display_name;
	PR_BYTE package_sid;
	PR_STRING package_sid_string;
	PR_STRING real_path;
	PITEM_APP ptr_app;
	ULONG_PTR app_hash;
	LONG64 timestamp;
	HKEY hsubkey;
	LONG status;

	display_name = NULL;
	package_sid = NULL;
	package_sid_string = NULL;
	real_path = NULL;

	status = RegOpenKeyEx (hkey, key_name->buffer, 0, KEY_READ, &hsubkey);

	if (status != ERROR_SUCCESS)
		goto CleanupExit;

	package_sid = _r_reg_querybinary (hsubkey, NULL, L"PackageSid");

	if (!package_sid)
		goto CleanupExit;

	status = _r_str_fromsid (package_sid->buffer, &package_sid_string);

	if (status != STATUS_SUCCESS)
		goto CleanupExit;

	// already exists (skip)
	app_hash = _r_str_gethash2 (package_sid_string, TRUE);

	if (_app_isappfound (app_hash))
		goto CleanupExit;

	// parse package display name
	display_name = _r_reg_querystring (hsubkey, NULL, L"DisplayName");
	_app_package_parsedisplayname (&display_name, key_name);

	// parse package path
	real_path = _r_reg_querystring (hsubkey, NULL, L"PackageRootFolder");
	_app_package_parsepath (&real_path);

	//_r_queuedlock_acquireexclusive (&lock_apps);
	app_hash = _app_addapplication (NULL, DATA_APP_UWP, package_sid_string, display_name, real_path);
	//_r_queuedlock_releaseexclusive (&lock_apps);

	if (app_hash)
	{
		ptr_app = _app_getappitem (app_hash);

		if (ptr_app)
		{
			timestamp = _r_reg_querytimestamp (hsubkey);

			_app_setappinfo (ptr_app, INFO_TIMESTAMP_PTR, &timestamp);
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
		RegCloseKey (hsubkey);
}

VOID _app_package_getpackagebysid (
	_In_ HKEY hkey,
	_In_ PR_STRING key_name
)
{
	PR_STRING moniker;
	PR_STRING display_name;
	PR_BYTE package_sid;
	PITEM_APP ptr_app;
	ULONG_PTR app_hash;
	LONG64 timestamp;
	HKEY hsubkey;
	LONG status;

	moniker = NULL;
	display_name = NULL;
	package_sid = NULL;

	// already exists (skip)
	app_hash = _r_str_gethash2 (key_name, TRUE);

	if (_app_isappfound (app_hash))
		return;

	status = RegOpenKeyEx (hkey, key_name->buffer, 0, KEY_READ, &hsubkey);

	if (status != ERROR_SUCCESS)
		goto CleanupExit;

	package_sid = _r_str_tosid (key_name);

	if (!package_sid)
		goto CleanupExit;

	// query package moniker
	moniker = _r_reg_querystring (hsubkey, NULL, L"Moniker");

	if (!moniker)
		moniker = _r_obj_reference (key_name);

	// TODO: path is not required to filtering package via WFP, only for
	// signature checking, but i think there is another way to do this. Not needed by now.

	//if (moniker)
	//{
	//	// parse package path
	//	// TODO: there is package full name required.
	//	status = _r_sys_getpackagepath (moniker, &real_path);
	//
	//	if (status == ERROR_SUCCESS)
	//	{
	//		_app_package_parsepath (&real_path);
	//	}
	//}
	//else
	//{
	//	moniker = _r_obj_reference (key_name);
	//
	//	real_path = NULL;
	//}

	// parse package display name
	display_name = _r_reg_querystring (hsubkey, NULL, L"DisplayName");
	_app_package_parsedisplayname (&display_name, moniker);

	// TODO: since packages does not obtain paths, we need to query general
	// information, certificates and icons in another way.

	//_r_queuedlock_acquireexclusive (&lock_apps);
	app_hash = _app_addapplication (NULL, DATA_APP_UWP, key_name, display_name, NULL);
	//_r_queuedlock_releaseexclusive (&lock_apps);

	if (app_hash)
	{
		ptr_app = _app_getappitem (app_hash);

		if (ptr_app)
		{
			timestamp = _r_reg_querytimestamp (hsubkey);

			_app_setappinfo (ptr_app, INFO_TIMESTAMP_PTR, &timestamp);
			_app_setappinfo (ptr_app, INFO_BYTES_DATA, _r_obj_reference (package_sid));

			_r_obj_dereference (ptr_app);
		}
	}

CleanupExit:

	if (moniker)
		_r_obj_dereference (moniker);

	if (display_name)
		_r_obj_dereference (display_name);

	if (package_sid)
		_r_obj_dereference (package_sid);

	if (hsubkey)
		RegCloseKey (hsubkey);
}

VOID _app_package_getpackageslist ()
{
	HKEY hkey;
	PR_STRING key_name;
	ULONG key_index;
	ULONG max_length;
	ULONG size;
	LONG status;

	// query packages by name
	status = RegOpenKeyEx (
		HKEY_CURRENT_USER,
		L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages",
		0,
		KEY_READ,
		&hkey
	);

	if (status != ERROR_SUCCESS)
	{
		_r_log (LOG_LEVEL_WARNING, NULL, L"RegOpenKeyEx", status, L"Repository\\Packages");
	}
	else
	{
		max_length = _r_reg_querysubkeylength (hkey);

		if (max_length)
		{
			key_index = 0;
			key_name = _r_obj_createstring_ex (NULL, max_length * sizeof (WCHAR));

			while (TRUE)
			{
				size = max_length + 1;
				status = RegEnumKeyEx (hkey, key_index++, key_name->buffer, &size, NULL, NULL, NULL, NULL);

				if (status != ERROR_SUCCESS)
					break;

				_r_obj_setstringlength_ex (key_name, size * sizeof (WCHAR), max_length * sizeof (WCHAR));

				_app_package_getpackagebyname (hkey, key_name);
			}

			_r_obj_dereference (key_name);
		}

		RegCloseKey (hkey);
	}

	// query packages by sid
	status = RegOpenKeyEx (
		HKEY_CURRENT_USER,
		L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppContainer\\Mappings",
		0,
		KEY_READ,
		&hkey
	);

	if (status != ERROR_SUCCESS)
	{
		_r_log (LOG_LEVEL_WARNING, NULL, L"RegOpenKeyEx", status, L"AppContainer\\Mappings");
	}
	else
	{
		max_length = _r_reg_querysubkeylength (hkey);

		if (max_length)
		{
			key_index = 0;
			key_name = _r_obj_createstring_ex (NULL, max_length * sizeof (WCHAR));

			while (TRUE)
			{
				size = max_length + 1;
				status = RegEnumKeyEx (hkey, key_index++, key_name->buffer, &size, NULL, NULL, NULL, NULL);

				if (status != ERROR_SUCCESS)
					break;

				_r_obj_setstringlength_ex (key_name, size * sizeof (WCHAR), max_length * sizeof (WCHAR));

				_app_package_getpackagebysid (hkey, key_name);
			}

			_r_obj_dereference (key_name);
		}

		RegCloseKey (hkey);
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
	ULONG buffer_size;

	ULONG return_length;
	ULONG services_returned;

	HKEY hkey;

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
	buffer = _r_mem_allocatezero (buffer_size);

	if (!EnumServicesStatusEx (hsvcmgr, SC_ENUM_PROCESS_INFO, service_type, service_state, buffer, buffer_size, &return_length, &services_returned, NULL, NULL))
	{
		if (GetLastError () == ERROR_MORE_DATA)
		{
			// Set the buffer
			buffer_size += return_length;
			buffer = _r_mem_reallocatezero (buffer, buffer_size);

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

	// now traverse each service to get information
	if (buffer)
	{
		services = (LPENUM_SERVICE_STATUS_PROCESS)buffer;

		for (ULONG i = 0; i < services_returned; i++)
		{
			service = &services[i];

			app_hash = _r_str_gethash (service->lpServiceName, TRUE);

			if (_app_isappfound (app_hash))
				continue;

			_r_str_printf (general_key, RTL_NUMBER_OF (general_key), L"System\\CurrentControlSet\\Services\\%s", service->lpServiceName);

			service_name = _r_obj_createstring (service->lpServiceName);

			if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, general_key, 0, KEY_READ, &hkey) != ERROR_SUCCESS)
			{
				_r_obj_dereference (service_name);
				continue;
			}

			// skip userservice instances service types (win10+)
			if (_r_sys_isosversiongreaterorequal (WINDOWS_10))
			{
				service_type = _r_reg_queryulong (hkey, NULL, L"Type");

				if (!service_type || (service_type & SERVICE_USERSERVICE_INSTANCE) != 0)
				{
					_r_obj_dereference (service_name);
					RegCloseKey (hkey);

					continue;
				}
			}

			// query service path
			service_path = _r_reg_querystring (hkey, L"Parameters", L"ServiceDLL");

			if (!service_path)
			{
				// Windows 8 places the ServiceDll for some services in the root key. (dmex)
				if (_r_sys_isosversionequal (WINDOWS_8) || _r_sys_isosversionequal (WINDOWS_8_1))
					service_path = _r_reg_querystring (hkey, NULL, L"ServiceDLL");

				if (!service_path)
					service_path = _r_reg_querystring (hkey, NULL, L"ImagePath");
			}

			if (service_path)
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

				// query service timestamp
				service_timestamp = _r_reg_querytimestamp (hkey);

				// query service sid
				status = _r_sys_getservicesid (&service_name->sr, &service_sid);

				if (status == STATUS_SUCCESS)
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
					if (BuildSecurityDescriptor (NULL, NULL, 1, &ea, 0, NULL, NULL, &sd_length, &service_sd) == ERROR_SUCCESS && service_sd)
					{
						name_string = _r_obj_createstring (service->lpDisplayName);

						app_hash = _app_addapplication (NULL, DATA_APP_SERVICE, service_name, name_string, service_path);

						if (app_hash)
						{
							ptr_app = _app_getappitem (app_hash);

							if (ptr_app)
							{
								_app_setappinfo (ptr_app, INFO_TIMESTAMP_PTR, &service_timestamp);
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

			RegCloseKey (hkey);
		}

		_r_mem_free (buffer);
	}

	CloseServiceHandle (hsvcmgr);
}
