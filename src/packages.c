// simplewall
// Copyright (c) 2016-2021 Henry++

#include "global.h"

VOID _app_package_getpackageinfo (_Inout_ PR_STRING_PTR package_root_folder)
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

VOID _app_package_getpackageslist ()
{
	PR_BYTE package_sid;
	PR_STRING package_sid_string;
	PR_STRING key_name;
	PR_STRING display_name;
	PR_STRING localized_name;
	PR_STRING real_path;
	PITEM_APP ptr_app;
	LONG64 timestamp;
	ULONG_PTR app_hash;
	UINT localized_length;
	HKEY hkey;
	HKEY hsubkey;
	ULONG key_index;
	ULONG max_length;
	ULONG size;
	NTSTATUS status;

	status = RegOpenKeyEx (
		HKEY_CURRENT_USER,
		L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages",
		0,
		KEY_READ,
		&hkey
	);

	if (status != ERROR_SUCCESS)
		return;

	max_length = _r_reg_querysubkeylength (hkey);

	if (max_length)
	{
		key_index = 0;
		key_name = _r_obj_createstring_ex (NULL, max_length * sizeof (WCHAR));

		while (TRUE)
		{
			size = max_length + 1;

			if (RegEnumKeyEx (hkey, key_index++, key_name->buffer, &size, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
				break;

			_r_obj_trimstringtonullterminator (key_name);

			status = RegOpenKeyEx (hkey, key_name->buffer, 0, KEY_READ, &hsubkey);

			if (status == ERROR_SUCCESS)
			{
				package_sid = _r_reg_querybinary (hsubkey, NULL, L"PackageSid");

				if (package_sid)
				{
					if (RtlValidSid (package_sid->buffer))
					{
						status = _r_str_fromsid (package_sid->buffer, &package_sid_string);

						if (status == STATUS_SUCCESS)
						{
							if (!_app_isappfound (_r_str_gethash2 (package_sid_string, TRUE)))
							{
								display_name = _r_reg_querystring (hsubkey, NULL, L"DisplayName");

								if (display_name)
								{
									if (!_r_obj_isstringempty2 (display_name))
									{
										if (display_name->buffer[0] == L'@')
										{
											localized_length = 512;
											localized_name = _r_obj_createstring_ex (NULL, localized_length * sizeof (WCHAR));

											if (SUCCEEDED (SHLoadIndirectString (display_name->buffer, localized_name->buffer, localized_length, NULL)))
											{
												_r_obj_trimstringtonullterminator (localized_name);

												_r_obj_movereference (&display_name, localized_name);
											}
											else
											{
												_r_obj_dereference (localized_name);
											}
										}
									}

									// use registry key name as fallback package name
									if (_r_obj_isstringempty (display_name))
										_r_obj_swapreference (&display_name, key_name);

									real_path = _r_reg_querystring (hsubkey, NULL, L"PackageRootFolder");

									// load additional info from appx manifest
									_app_package_getpackageinfo (&real_path);

									app_hash = _app_addapplication (NULL, DATA_APP_UWP, &package_sid_string->sr, display_name, real_path);

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

									if (real_path)
										_r_obj_dereference (real_path);

									_r_obj_dereference (display_name);
								}
							}

							_r_obj_dereference (package_sid_string);
						}
					}

					_r_obj_dereference (package_sid);
				}

				RegCloseKey (hsubkey);
			}
		}

		_r_obj_dereference (key_name);
	}

	RegCloseKey (hkey);
}

VOID _app_package_getserviceslist ()
{
	static ULONG initial_buffer_size = 0x8000;

	SC_HANDLE hsvcmgr;

	WCHAR general_key[256];
	EXPLICIT_ACCESS ea;
	R_STRINGREF service_name;
	LPENUM_SERVICE_STATUS_PROCESS service;
	LPENUM_SERVICE_STATUS_PROCESS services;
	PVOID service_sd;
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

			_r_obj_initializestringref (&service_name, service->lpServiceName);

			app_hash = _r_str_gethash3 (&service_name, TRUE);

			if (_app_isappfound (app_hash))
				continue;

			_r_str_printf (general_key, RTL_NUMBER_OF (general_key), L"System\\CurrentControlSet\\Services\\%s", service->lpServiceName);

			if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, general_key, 0, KEY_READ, &hkey) != ERROR_SUCCESS)
				continue;

			// skip userservice instances service types (win10+)
			if (_r_sys_isosversiongreaterorequal (WINDOWS_10))
			{
				service_type = _r_reg_queryulong (hkey, NULL, L"Type");

				if (!service_type || (service_type & SERVICE_USERSERVICE_INSTANCE) != 0)
				{
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
				status = _r_sys_getservicesid (&service_name, &service_sid);

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

						app_hash = _app_addapplication (NULL, DATA_APP_SERVICE, &service_name, name_string, service_path);

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

			RegCloseKey (hkey);
		}

		_r_mem_free (buffer);
	}

	CloseServiceHandle (hsvcmgr);
}
