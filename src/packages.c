// simplewall
// Copyright (c) 2016-2026 Henry++

#include "global.h"

BOOLEAN _app_package_isnotexists (
	_In_ PR_STRING package_sid
)
{
	ULONG app_hash;

	app_hash = _r_str_gethash (&package_sid->sr, TRUE);

	if (_app_isappfound (app_hash))
		return TRUE;

	// there we try to found new packages (HACK!!!)
	_app_package_getpackageslist (_r_app_gethwnd ());

	return _app_isappfound (app_hash);
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
	PR_STRING manifest_path = NULL, path_string, result_path = NULL;
	R_STRINGREF executable_sr;
	BOOLEAN is_success = FALSE;
	HRESULT status;

	path_string = *package_root_folder;

	for (ULONG_PTR i = 0; i < RTL_NUMBER_OF (appx_names); i++)
	{
		_r_obj_movereference ((PVOID_PTR)&manifest_path, _r_obj_concatstringrefs (3, &path_string->sr, &separator_sr, &appx_names[i]));

		if (_r_fs_isexists (&manifest_path->sr))
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

			_r_obj_movereference ((PVOID_PTR)&result_path, _r_obj_concatstringrefs (3, &path_string->sr, &separator_sr, &executable_sr));

			if (_r_fs_isexists (&result_path->sr))
			{
				_r_obj_swapreference ((PVOID_PTR)package_root_folder, result_path);
				break;
			}
		}
	}

CleanupExit:

	if (manifest_path)
		_r_obj_dereference (manifest_path);

	if (result_path)
		_r_obj_dereference (result_path);

	_r_xml_destroylibrary (&xml_library);
}

VOID _app_package_getpackagebyname (
	_In_ HANDLE hroot,
	_In_ LPCWSTR path,
	_In_ PR_STRING key_name
)
{
	PR_STRING display_name = NULL, package_sid_string = NULL, real_path = NULL;
	PR_BYTE package_sid = NULL;
	PITEM_APP ptr_app;
	WCHAR buffer[0x100];
	HANDLE hsubkey;
	LONG64 timestamp;
	ULONG app_hash;
	NTSTATUS status;

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%s\\%s", path, key_name->buffer);

	status = _r_reg_openkey (&hsubkey, hroot, buffer, 0, KEY_READ);

	if (!NT_SUCCESS (status))
	{
		_r_log (LOG_LEVEL_ERROR, NULL, TEXT (__FUNCTION__), status, buffer);

		goto CleanupExit;
	}

	status = _r_reg_querybinary (hsubkey, L"PackageSid", &package_sid);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	status = _r_str_fromsid (&package_sid_string, (PSID)(package_sid->buffer));

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	// already exists (skip)
	app_hash = _r_str_gethash (&package_sid_string->sr, TRUE);

	if (_app_isappfound (app_hash))
		goto CleanupExit;

	// parse package information
	if (!_app_uwp_getpackageinfo (key_name, &display_name, &real_path))
		goto CleanupExit;

	if (real_path)
		_app_package_parsepath (&real_path);

	//_r_queuedlock_acquireexclusive (&lock_apps);
	ptr_app = _app_addapplication (NULL, DATA_APP_UWP, package_sid_string, display_name, real_path);
	//_r_queuedlock_releaseexclusive (&lock_apps);

	if (ptr_app)
	{
		status = _r_reg_queryinfo (hsubkey, NULL, &timestamp);

		if (NT_SUCCESS (status))
			_app_setappinfo (ptr_app, INFO_TIMESTAMP, &timestamp);

		_app_setappinfo (ptr_app, INFO_BYTES_DATA, _r_obj_reference (package_sid));

		_r_obj_dereference (ptr_app);
	}

CleanupExit:

	if (package_sid_string)
		_r_obj_dereference (package_sid_string);

	if (display_name)
		_r_obj_dereference (display_name);

	if (package_sid)
		_r_obj_dereference (package_sid);

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
	PR_STRING display_name = NULL, moniker = NULL, real_path = NULL;
	PR_BYTE package_sid = NULL;
	PITEM_APP ptr_app;
	WCHAR buffer[0x100];
	HANDLE hsubkey;
	LONG64 timestamp = 0;
	ULONG app_hash;
	NTSTATUS status;

	// already exists (skip)
	app_hash = _r_str_gethash (&key_name->sr, TRUE);

	if (_app_isappfound (app_hash))
		return;

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%s\\%s", path, key_name->buffer);

	status = _r_reg_openkey (&hsubkey, hroot, buffer, 0, KEY_READ);

	if (!NT_SUCCESS (status))
	{
		_r_log (LOG_LEVEL_ERROR, NULL, TEXT (__FUNCTION__), status, buffer);

		goto CleanupExit;
	}

	package_sid = _r_str_tosid (key_name);

	if (!package_sid)
		goto CleanupExit;

	// query package moniker
	status = _r_reg_querystring (hsubkey, L"Moniker", &moniker, NULL);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	// parse package information
	if (!_app_uwp_getpackageinfo (moniker, &display_name, &real_path))
		goto CleanupExit;

	if (real_path)
		_app_package_parsepath (&real_path);

	//_r_queuedlock_acquireexclusive (&lock_apps);
	ptr_app = _app_addapplication (NULL, DATA_APP_UWP, key_name, display_name, NULL);
	//_r_queuedlock_releaseexclusive (&lock_apps);

	if (ptr_app)
	{
		status = _r_reg_queryinfo (hsubkey, NULL, &timestamp);

		if (NT_SUCCESS (status))
			_app_setappinfo (ptr_app, INFO_TIMESTAMP, &timestamp);

		_app_setappinfo (ptr_app, INFO_BYTES_DATA, _r_obj_reference (package_sid));

		_r_obj_dereference (ptr_app);
	}

CleanupExit:

	if (display_name)
		_r_obj_dereference (display_name);

	if (package_sid)
		_r_obj_dereference (package_sid);

	if (real_path)
		_r_obj_dereference (real_path);

	if (moniker)
		_r_obj_dereference (moniker);

	if (hsubkey)
		NtClose (hsubkey);
}

VOID NTAPI _app_package_threadproc (
	_In_ PVOID arglist
)
{
	HANDLE hpackages_key = NULL, hservices_key = NULL, event_handle1 = NULL, event_handle2 = NULL, hevents[2] = {0};
	PITEM_APP ptr_app = NULL;
	IO_STATUS_BLOCK isb;
	HWND hwnd;
	LONG64 current_time;
	ULONG_PTR enum_key = 0;
	ULONG flags = REG_NOTIFY_CHANGE_NAME;
	NTSTATUS status;

	hwnd = (HWND)arglist;

	status = NtCreateEvent (&event_handle1, EVENT_ALL_ACCESS, NULL, NotificationEvent, FALSE);

	if (!NT_SUCCESS (status))
	{
		_r_show_errormessage (hwnd, NULL, status, L"NtCreateEvent", ET_NATIVE);
		return;
	}

	status = NtCreateEvent (&event_handle2, EVENT_ALL_ACCESS, NULL, NotificationEvent, FALSE);

	if (!NT_SUCCESS (status))
	{
		_r_show_errormessage (hwnd, NULL, status, L"NtCreateEvent", ET_NATIVE);

		NtClose (event_handle1); // do not left handle opened!

		return;
	}

	hevents[0] = event_handle1;
	hevents[1] = event_handle2;

	status = _r_reg_openkey (&hservices_key, HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services", 0, KEY_NOTIFY);

	if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
	{
		_r_reg_openkey (&hpackages_key, HKEY_CURRENT_USER, L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages", 0, KEY_NOTIFY);

		flags |= REG_NOTIFY_THREAD_AGNOSTIC;
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

				while (_r_obj_enumhashtablepointer (apps_table, (PVOID_PTR)&ptr_app, NULL, &enum_key))
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

				while (_r_obj_enumhashtablepointer (apps_table, (PVOID_PTR)&ptr_app, NULL, &enum_key))
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
}

VOID _app_package_getpackageslist (
	_In_opt_ HWND hwnd
)
{
	LPCWSTR reg_byname = L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages";
	LPCWSTR reg_bysid = L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppContainer\\Mappings";
	PR_STRING name;
	HANDLE hkey;
	ULONG index = 0;
	NTSTATUS status;

	// query packages by name
	status = _r_reg_openkey (&hkey, HKEY_CURRENT_USER, reg_byname, 0, KEY_READ);

	if (!NT_SUCCESS (status))
	{
		if (hwnd && status != STATUS_OBJECT_NAME_NOT_FOUND)
			_r_show_errormessage (hwnd, L"Could not query packages by name!", status, reg_byname, ET_NATIVE);
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
	status = _r_reg_openkey (&hkey, HKEY_CURRENT_USER, reg_bysid, 0, KEY_READ);

	if (!NT_SUCCESS (status))
	{
		if (hwnd && status != STATUS_OBJECT_NAME_NOT_FOUND)
			_r_show_errormessage (hwnd, L"Could not query packages by sid!", status, reg_bysid, ET_NATIVE);
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
	_In_opt_ HWND hwnd
)
{
	PR_STRING converted_path, name_string, service_name, service_path;
	LPENUM_SERVICE_STATUS_PROCESS service, services;
	PVOID buffer, service_sd = NULL;
	R_STRINGREF dummy_filename;
	WCHAR general_key[0x100];
	PR_BYTE service_sid;
	EXPLICIT_ACCESS ea;
	PITEM_APP ptr_app;
	SC_HANDLE hsvcmgr;
	HANDLE hkey;
	LONG64 service_timestamp;
	ULONG buffer_size, return_length, services_returned, service_type = SERVICE_WIN32_OWN_PROCESS | SERVICE_WIN32_SHARE_PROCESS, sd_length;
	NTSTATUS status;

	hsvcmgr = OpenSCManagerW (NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE);

	if (!hsvcmgr)
	{
		if (hwnd)
			_r_show_errormessage (hwnd, NULL, NtLastError (), L"OpenSCManagerW", ET_WINDOWS);

		return;
	}

	// win10+
	if (_r_sys_isosversiongreaterorequal (WINDOWS_10))
		service_type |= SERVICE_INTERACTIVE_PROCESS | SERVICE_USER_SERVICE | SERVICE_USERSERVICE_INSTANCE;

	buffer_size = PR_SIZE_BUFFER;
	buffer = _r_mem_allocate (buffer_size);

	if (!EnumServicesStatusExW (hsvcmgr, SC_ENUM_PROCESS_INFO, service_type, SERVICE_STATE_ALL, (PBYTE)buffer, buffer_size, &return_length, &services_returned, NULL, NULL))
	{
		if (NtLastError () == ERROR_MORE_DATA)
		{
			// reallocate new buffer
			buffer_size += return_length;
			buffer = _r_mem_reallocate (buffer, buffer_size);

			// now query again for services
			if (!EnumServicesStatusExW (hsvcmgr, SC_ENUM_PROCESS_INFO, service_type, SERVICE_STATE_ALL, (PBYTE)buffer, buffer_size, &return_length, &services_returned, NULL, NULL))
			{
				_r_mem_free (buffer); // cleanup!

				buffer = NULL;
			}
		}
		else
		{
			_r_mem_free (buffer); // cleanup!

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

		if (_app_isappfound (_r_str_gethash2 (service->lpServiceName, TRUE)))
			continue;

		_r_str_printf (general_key, RTL_NUMBER_OF (general_key), L"SYSTEM\\CurrentControlSet\\Services\\%s", service->lpServiceName);

		status = _r_reg_openkey (&hkey, HKEY_LOCAL_MACHINE, general_key, 0, KEY_READ);

		if (!NT_SUCCESS (status))
			continue;

		// query service path
		status = _r_reg_querystring (hkey, L"ImagePath", &service_path, NULL);

		if (NT_SUCCESS (status))
		{
			_r_path_parsecommandlinefuzzy (&service_path->sr, &dummy_filename, NULL, &converted_path);

			if (converted_path)
			{
				_r_obj_movereference ((PVOID_PTR)&service_path, converted_path);
			}
			else
			{
				_r_obj_movereference ((PVOID_PTR)&service_path, _r_path_dospathfromnt (&service_path->sr));
			}

			// query service sid
			status = _r_sys_getservicesid (&service_sid, service->lpServiceName);

			if (NT_SUCCESS (status))
			{
				// when evaluating SECURITY_DESCRIPTOR conditions, the filter engine checks for FWP_ACTRL_MATCH_FILTER access.
				// if the DACL grants access, it does not mean that the traffic is allowed; it just means that the condition
				// evaluates to true. likewise if it denies access, the condition evaluates to FALSE.
				_app_setexplicitaccess (&ea, NO_INHERITANCE, GRANT_ACCESS, FWP_ACTRL_MATCH_FILTER, (PSID)(service_sid->buffer));

				// security descriptors must be in self-relative form (i.e., contiguous). the security descriptor returned
				// by BuildSecurityDescriptorW is already self-relative, but if you're using another mechanism to build
				// the descriptor, you may have to convert it. see MakeSelfRelativeSD for details.
				status = BuildSecurityDescriptorW (NULL, NULL, 1, &ea, 0, NULL, NULL, &sd_length, &service_sd);

				if (status == ERROR_SUCCESS && service_sd)
				{
					service_name = _r_obj_createstring (service->lpServiceName);
					name_string = _r_obj_createstring (service->lpDisplayName);

					ptr_app = _app_addapplication (NULL, DATA_APP_SERVICE, service_name, name_string, service_path);

					if (ptr_app)
					{
						// query service timestamp
						status = _r_reg_queryinfo (hkey, NULL, &service_timestamp);

						if (NT_SUCCESS (status))
							_app_setappinfo (ptr_app, INFO_TIMESTAMP, &service_timestamp);

						_app_setappinfo (ptr_app, INFO_BYTES_DATA, _r_obj_createbyte_ex ((LPCSTR)service_sd, sd_length));

						_r_obj_dereference (ptr_app);
					}

					_r_obj_dereference (service_name);
					_r_obj_dereference (name_string);

					LocalFree (service_sd);
				}

				_r_obj_dereference (service_sid);
			}

			_r_obj_dereference (service_path);
		}

		NtClose (hkey);
	}

	CloseServiceHandle (hsvcmgr);

	_r_mem_free (buffer);
}
