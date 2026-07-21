// simplewall
// Copyright (c) 2016-2026 Henry++

#include "global.h"

VOID NTAPI _app_dereferenceapp (
	_In_ PVOID entry
)
{
	PITEM_APP ptr_item = (PITEM_APP)entry;

	if (ptr_item->display_name)
		_r_obj_dereference (ptr_item->display_name);

	if (ptr_item->original_path)
		_r_obj_dereference (ptr_item->original_path);

	if (ptr_item->notification)
		_r_obj_dereference (ptr_item->notification);

	if (ptr_item->short_name)
		_r_obj_dereference (ptr_item->short_name);

	if (ptr_item->real_path)
		_r_obj_dereference (ptr_item->real_path);

	if (ptr_item->guids)
		_r_obj_dereference (ptr_item->guids);
}

VOID NTAPI _app_dereferenceappinfo (
	_In_ PVOID entry
)
{
	PITEM_APP_INFO ptr_item = (PITEM_APP_INFO)entry;

	if (ptr_item->signature_info)
		_r_obj_dereference (ptr_item->signature_info);

	if (ptr_item->version_info)
		_r_obj_dereference (ptr_item->version_info);

	if (ptr_item->path)
		_r_obj_dereference (ptr_item->path);
}

VOID NTAPI _app_dereferenceruleconfig (
	_In_ PVOID entry
)
{
	PITEM_RULE_CONFIG ptr_item = (PITEM_RULE_CONFIG)entry;

	if (ptr_item->name)
		_r_obj_dereference (ptr_item->name);

	if (ptr_item->apps)
		_r_obj_dereference (ptr_item->apps);
}

VOID NTAPI _app_dereferencelog (
	_In_ PVOID entry
)
{
	PITEM_LOG ptr_item = (PITEM_LOG)entry;

	if (ptr_item->remote_host_str)
		_r_obj_dereference (ptr_item->remote_host_str);

	if (ptr_item->local_host_str)
		_r_obj_dereference (ptr_item->local_host_str);

	if (ptr_item->remote_addr_str)
		_r_obj_dereference (ptr_item->remote_addr_str);

	if (ptr_item->local_addr_str)
		_r_obj_dereference (ptr_item->local_addr_str);

	if (ptr_item->protocol_str)
		_r_obj_dereference (ptr_item->protocol_str);

	if (ptr_item->filter_name)
		_r_obj_dereference (ptr_item->filter_name);

	if (ptr_item->layer_name)

		_r_obj_dereference (ptr_item->layer_name);

	if (ptr_item->username)
		_r_obj_dereference (ptr_item->username);

	if (ptr_item->path)
		_r_obj_dereference (ptr_item->path);
}

VOID NTAPI _app_dereferencenetwork (
	_In_ PVOID entry
)
{
	PITEM_NETWORK ptr_item = (PITEM_NETWORK)entry;

	if (ptr_item->remote_host_str)
		_r_obj_dereference (ptr_item->remote_host_str);

	if (ptr_item->local_host_str)
		_r_obj_dereference (ptr_item->local_host_str);

	if (ptr_item->remote_addr_str)
		_r_obj_dereference (ptr_item->remote_addr_str);

	if (ptr_item->local_addr_str)
		_r_obj_dereference (ptr_item->local_addr_str);

	if (ptr_item->protocol_str)
		_r_obj_dereference (ptr_item->protocol_str);

	if (ptr_item->path)
		_r_obj_dereference (ptr_item->path);
}

VOID NTAPI _app_dereferencerule (
	_In_ PVOID entry
)
{
	PITEM_RULE ptr_item = (PITEM_RULE)entry;

	if (ptr_item->protocol_str)
		_r_obj_dereference (ptr_item->protocol_str);

	if (ptr_item->rule_remote)
		_r_obj_dereference (ptr_item->rule_remote);

	if (ptr_item->rule_local)
		_r_obj_dereference (ptr_item->rule_local);

	if (ptr_item->guids)
		_r_obj_dereference (ptr_item->guids);

	if (ptr_item->apps)
		_r_obj_dereference (ptr_item->apps);

	if (ptr_item->name)
		_r_obj_dereference (ptr_item->name);
}

VOID _app_addcachetable (
	_Inout_ PR_HASHTABLE hashtable,
	_In_ ULONG hash_code,
	_In_ PR_QUEUED_LOCK spin_lock,
	_In_opt_ PVOID string
)
{
	BOOLEAN is_exceed;

	// check overflow and do nothing
	_r_queuedlock_acquireshared (spin_lock);
	is_exceed = (_r_obj_gethashtablesize (hashtable) >= MAP_CACHE_MAX);
	_r_queuedlock_releaseshared (spin_lock);

	if (is_exceed)
		return;

	_r_queuedlock_acquireexclusive (spin_lock);
	_r_obj_addhashtablepointer (hashtable, hash_code, string);
	_r_queuedlock_releaseexclusive (spin_lock);
}

BOOLEAN _app_checkipisnotlocal (
	_In_ ADDRESS_FAMILY af,
	_In_ LPCVOID address
)
{
	PIN6_ADDR p6addr;
	PIN_ADDR p4addr;

	switch (af)
	{
		case AF_INET:
		{
			p4addr = (const PIN_ADDR)address;

			if (IN4_IS_ADDR_UNSPECIFIED (p4addr) || IN4_IS_ADDR_LOOPBACK (p4addr) || IN4_IS_ADDR_RFC1918 (p4addr))
				return FALSE;

			return TRUE;
		}

		case AF_INET6:
		{
			p6addr = (const PIN6_ADDR)address;

			if (IN6_IS_ADDR_UNSPECIFIED (p6addr) || IN6_IS_ADDR_LOOPBACK (p6addr) || IN6_IS_ADDR_LINKLOCAL (p6addr) || IN6_IS_ADDR_SITELOCAL (p6addr) || IN6_IS_ADDR_ULA (p6addr))
				return FALSE;

			return TRUE;
		}

		default:
		{
			return FALSE; // unknown address family
		}
	}
}

BOOLEAN _app_getcachetable (
	_Inout_ PR_HASHTABLE cache_table,
	_In_ ULONG hash_code,
	_In_ PR_QUEUED_LOCK spin_lock,
	_Out_ PR_STRING_PTR string
)
{
	PR_OBJECT_POINTER object_ptr;

	_r_queuedlock_acquireshared (spin_lock);
	object_ptr = (PR_OBJECT_POINTER)_r_obj_findhashtable (cache_table, hash_code);
	_r_queuedlock_releaseshared (spin_lock);

	if (object_ptr && object_ptr->object_body)
	{
		*string = _r_obj_reference (object_ptr->object_body);

		return TRUE;
	}

	*string = NULL;

	return FALSE;
}

PR_STRING _app_formatarpa (
	_In_ ADDRESS_FAMILY af,
	_In_ LPCVOID address
)
{
	R_STRINGBUILDER formatted_address;
	PIN6_ADDR p6addr;
	PIN_ADDR p4addr;

	_r_obj_initializestringbuilder (&formatted_address, 0);

	switch (af)
	{
		case AF_INET:
		{
			p4addr = (const PIN_ADDR)address;

			_r_obj_appendstringbuilderformat (&formatted_address, L"%hhu.%hhu.%hhu.%hhu.%s", p4addr->s_impno, p4addr->s_lh, p4addr->s_host, p4addr->s_net, DNS_IP4_REVERSE_DOMAIN_STRING_W);

			break;
		}

		case AF_INET6:
		{
			p6addr = (const PIN6_ADDR)address;

			for (INT i = sizeof (IN6_ADDR) - 1; i >= 0; i--)
			{
				_r_obj_appendstringbuilderformat (&formatted_address, L"%hhx.%hhx.", p6addr->s6_addr[i] & 0xF, (p6addr->s6_addr[i] >> 4) & 0xF);
			}

			_r_obj_appendstringbuilder (&formatted_address, DNS_IP6_REVERSE_DOMAIN_STRING_W);

			break;
		}

		default:
		{
			_r_obj_appendstringbuilderformat (&formatted_address, L"<unknown address family %d>", af);
			break;
		}
	}

	return _r_obj_finalstringbuilder (&formatted_address);
}

_Ret_maybenull_
PR_STRING _app_formataddress (
	_In_ ADDRESS_FAMILY af,
	_In_ UINT8 proto,
	_In_ LPCVOID address,
	_In_opt_ UINT16 port,
	_In_ ULONG flags
)
{
	WCHAR addr_str[DNS_MAX_NAME_BUFFER_LENGTH];
	R_STRINGBUILDER formatted_address;
	LPCWSTR string;
	NTSTATUS status;

	_r_obj_initializestringbuilder (&formatted_address, 0);

	if (flags & FMTADDR_USE_PROTOCOL)
	{
		string = _app_db_getprotoname (proto, af, NULL);

		if (string)
		{
			_r_obj_appendstringbuilder (&formatted_address, string);
			_r_obj_appendstringbuilder (&formatted_address, L"://");
		}
	}

	status = _app_formatip (addr_str, RTL_NUMBER_OF (addr_str), af, address, !!(flags & FMTADDR_AS_RULE));

	if (NT_SUCCESS (status))
	{
		if (af == AF_INET6 && port)
		{
			_r_obj_appendstringbuilderformat (&formatted_address, L"[%s]", addr_str);
		}
		else
		{
			_r_obj_appendstringbuilder (&formatted_address, addr_str);
		}
	}

	if (port && !(flags & FMTADDR_USE_PROTOCOL))
	{
		_r_obj_appendstringbuilderformat (&formatted_address, NT_SUCCESS (status) ? L":%" TEXT (PRIu16) : L"%" TEXT (PRIu16), port);

		status = STATUS_SUCCESS;
	}

	if (NT_SUCCESS (status))
		return _r_obj_finalstringbuilder (&formatted_address);

	_r_obj_deletestringbuilder (&formatted_address);

	return NULL;
}

PR_STRING _app_formataddress_interlocked (
	_In_ volatile PVOID_PTR string,
	_In_ ADDRESS_FAMILY af,
	_In_ LPCVOID address
)
{
	PR_STRING current_string, new_string;

	current_string = (PR_STRING)_InterlockedCompareExchangePointer (string, NULL, NULL);

	if (current_string)
		return current_string;

	new_string = _app_formataddress (af, 0, address, 0, 0);

	current_string = (PR_STRING)_InterlockedCompareExchangePointer (string, new_string, NULL);

	if (!current_string)
	{
		current_string = new_string;
	}
	else
	{
		if (new_string)
			_r_obj_dereference (new_string);
	}

	return current_string;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_formatip (
	_Out_writes_to_ (buffer_length, buffer_length) LPWSTR buffer,
	_In_ ULONG buffer_length,
	_In_ ADDRESS_FAMILY af,
	_In_ LPCVOID address,
	_In_ BOOLEAN is_checkempty
)
{
	PIN6_ADDR p6addr;
	PIN_ADDR p4addr;

	RtlZeroMemory (buffer, buffer_length * sizeof (WCHAR));

	switch (af)
	{
		case AF_INET:
		{
			p4addr = (const PIN_ADDR)address;

			if (is_checkempty)
			{
				if (IN4_IS_ADDR_UNSPECIFIED (p4addr))
					return STATUS_INVALID_ADDRESS_COMPONENT;
			}

			return RtlIpv4AddressToStringExW (p4addr, 0, buffer, &buffer_length);
		}

		case AF_INET6:
		{
			p6addr = (const PIN6_ADDR)address;

			if (is_checkempty)
			{
				if (IN6_IS_ADDR_UNSPECIFIED (p6addr))
					return STATUS_INVALID_ADDRESS_COMPONENT;
			}

			return RtlIpv6AddressToStringExW (p6addr, 0, 0, buffer, &buffer_length);
		}

		default:
		{
			return STATUS_INVALID_PARAMETER; // unknown address family
		}
	}
}

PR_STRING _app_formatport (
	_In_ UINT16 port,
	_In_ UINT8 proto
)
{
	LPCWSTR service_string;

	service_string = _app_db_getservicename (port, proto);

	if (service_string)
		return _r_format_string (L"%" TEXT (PRIu16) L" (%s)", port, service_string);

	return _r_format_string (L"%" TEXT (PRIu16), port);
}

_Ret_maybenull_
PITEM_APP_INFO _app_getappinfobyhash2 (
	_In_ ULONG app_hash
)
{
	PITEM_APP_INFO ptr_app_info;

	_r_queuedlock_acquireshared (&lock_cache_information);
	ptr_app_info = (PITEM_APP_INFO)_r_obj_findhashtablepointer (cache_information, app_hash);
	_r_queuedlock_releaseshared (&lock_cache_information);

	return ptr_app_info;
}

_Success_ (return)
BOOLEAN _app_getappinfoparam2 (
	_In_ ULONG app_hash,
	_In_opt_ INT listview_id,
	_In_ ENUM_INFO_DATA2 info_data,
	_Out_writes_bytes_all_ (length) PVOID buffer,
	_In_ ULONG_PTR length
)
{
	PITEM_APP_INFO ptr_app_info;

	ptr_app_info = _app_getappinfobyhash2 (app_hash);

	switch (info_data)
	{
		case INFO_ICON_ID:
		{
			LONG icon_id = 0;

			if (length != sizeof (LONG))
			{
				if (ptr_app_info)
					_r_obj_dereference (ptr_app_info);

				return FALSE;
			}

			if (ptr_app_info)
				icon_id = ptr_app_info->icon_id;

			if (!icon_id)
				icon_id = _app_icons_getdefaultapp_id ((listview_id == IDC_APPS_UWP) ? DATA_APP_UWP : DATA_APP_REGULAR);

			if (icon_id)
			{
				RtlCopyMemory (buffer, &icon_id, length);

				if (ptr_app_info)
					_r_obj_dereference (ptr_app_info);

				return TRUE;
			}

			break;
		}

		case INFO_SIGNATURE_STRING:
		{
			PVOID ptr;

			if (length != sizeof (PVOID))
			{
				if (ptr_app_info)
					_r_obj_dereference (ptr_app_info);

				return FALSE;
			}

			if (ptr_app_info && !_r_obj_isstringempty (ptr_app_info->signature_info))
			{
				ptr = _r_obj_reference (ptr_app_info->signature_info);

				RtlCopyMemory (buffer, &ptr, length);

				_r_obj_dereference (ptr_app_info);

				return TRUE;
			}

			break;
		}

		case INFO_VERSION_STRING:
		{
			PVOID ptr;

			if (length != sizeof (PVOID))
			{
				if (ptr_app_info)
					_r_obj_dereference (ptr_app_info);

				return FALSE;
			}

			if (ptr_app_info && !_r_obj_isstringempty (ptr_app_info->version_info))
			{
				ptr = _r_obj_reference (ptr_app_info->version_info);

				RtlCopyMemory (buffer, &ptr, length);

				_r_obj_dereference (ptr_app_info);

				return TRUE;
			}

			break;
		}
	}

	if (ptr_app_info)
		_r_obj_dereference (ptr_app_info);

	return FALSE;
}

BOOLEAN _app_isappsigned (
	_In_ ULONG app_hash
)
{
	PR_STRING string = NULL;
	BOOLEAN is_signed = FALSE;

	if (_app_getappinfoparam2 (app_hash, 0, INFO_SIGNATURE_STRING, &string, sizeof (PR_STRING)))
	{
		is_signed = !_r_obj_isstringempty2 (string);

		_r_obj_dereference (string);
	}

	return is_signed;
}

BOOLEAN _app_isappvalidbinary (
	_In_opt_ PR_STRING path
)
{
	if (!path)
		return FALSE;

	if (!_app_isappvalidpath (path))
		return FALSE;

	return _r_str_isendsswith2 (&path->sr, L".exe", TRUE);
}

BOOLEAN _app_isappvalidpath (
	_In_opt_ PR_STRING path
)
{
	if (_r_obj_isstringempty (path))
		return FALSE;

	if (path->length <= (4 * sizeof (WCHAR)))
		return FALSE;

	if (path->buffer[1] != L':' || path->buffer[2] != L'\\')
		return FALSE;

	return TRUE;
}

_Ret_maybenull_
PR_STRING _app_getappdisplayname (
	_In_ PITEM_APP ptr_app,
	_In_ BOOLEAN is_shortened
)
{
	if (ptr_app->app_hash == config.ntoskrnl_hash)
	{
		if (!_r_obj_isstringempty (ptr_app->original_path))
			return _r_obj_reference (ptr_app->original_path);
	}

	switch (ptr_app->type)
	{
		case DATA_APP_SERVICE:
		{
			if (!_r_obj_isstringempty (ptr_app->original_path))
				return _r_obj_reference (ptr_app->original_path);

			break;
		}

		case DATA_APP_UWP:
		{
			if (!_r_obj_isstringempty (ptr_app->display_name))
				return _r_obj_reference (ptr_app->display_name);

			if (!_r_obj_isstringempty (ptr_app->real_path))
				return _r_obj_reference (ptr_app->real_path);

			if (!_r_obj_isstringempty (ptr_app->original_path))
				return _r_obj_reference (ptr_app->original_path);

			break;
		}

		default:
		{
			break;
		}
	}

	if ((is_shortened || _r_config_getboolean (L"ShowFilenames", TRUE, NULL)) && !_r_obj_isstringempty (ptr_app->short_name))
		return _r_obj_reference (ptr_app->short_name);

	if (!_r_obj_isstringempty (ptr_app->real_path))
		return _r_obj_reference (ptr_app->real_path);

	return NULL;
}

_Ret_maybenull_
PR_STRING _app_getapppath (
	_In_ PITEM_APP ptr_app
)
{
	PR_STRING path = NULL, string;

	if (ptr_app->type == DATA_APP_SERVICE || ptr_app->type == DATA_APP_UWP)
	{
		if (ptr_app->real_path)
			_r_obj_movereference ((PVOID_PTR)&path, _r_obj_reference (ptr_app->real_path));

		if (ptr_app->display_name)
			_r_obj_movereference ((PVOID_PTR)&path, _r_obj_reference (ptr_app->display_name));
	}

	if (ptr_app->original_path)
		_r_obj_movereference ((PVOID_PTR)&path, _r_obj_reference (ptr_app->original_path));

	if (ptr_app->real_path)
		_r_obj_movereference ((PVOID_PTR)&path, _r_obj_reference (ptr_app->real_path));

	if (path && _r_config_getboolean (L"IsShortPath", TRUE, NULL))
	{
		string = _r_str_environmentunexpandstring (&path->sr);

		_r_obj_movereference ((PVOID_PTR)&path, _r_path_compact (&string->sr, 0x40));

		_r_obj_dereference (string);
	}

	return path;
}

VOID _app_getfileicon (
	_Inout_ PITEM_APP_INFO ptr_app_info
)
{
	LONG icon_id = 0;

	if (_r_config_getboolean (L"IsIconsHidden", FALSE, NULL) || !_app_isappvalidbinary (ptr_app_info->path))
	{
		_app_icons_loadfromfile (NULL, ptr_app_info->type, &icon_id, NULL, TRUE);
	}
	else
	{
		_app_icons_loadfromfile (ptr_app_info->path, ptr_app_info->type, &icon_id, NULL, TRUE);
	}

	ptr_app_info->icon_id = icon_id;
}

_Success_ (return)
BOOLEAN _app_calculatefilehash (
	_In_ HANDLE hfile,
	_In_opt_ LPCWSTR algorithm_id,
	_Out_ PVOID_PTR out_file_hash,
	_Out_ PULONG out_file_hash_length,
	_Out_ HCATADMIN_PTR out_hcat_admin
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static CCAHFFH2 _CryptCATAdminCalcHashFromFileHandle2 = NULL;
	static CCAAC2 _CryptCATAdminAcquireContext2 = NULL;

	GUID DriverActionVerify = DRIVER_ACTION_VERIFY;
	HCATADMIN hcat_admin;
	HINSTANCE hwintrust;
	PBYTE file_hash;
	ULONG file_hash_length = 0x20;
	NTSTATUS status;

	if (_r_initonce_begin (&init_once))
	{
		status = _r_sys_loadlibrary2 (&hwintrust, L"wintrust.dll", 0);

		if (NT_SUCCESS (status))
		{
			_CryptCATAdminCalcHashFromFileHandle2 = (CCAHFFH2)_r_sys_getprocaddress (hwintrust, "CryptCATAdminCalcHashFromFileHandle2", 0);
			_CryptCATAdminAcquireContext2 = (CCAAC2)_r_sys_getprocaddress (hwintrust, "CryptCATAdminAcquireContext2", 0);

			//_r_sys_freelibrary (hwintrust);
		}

		_r_initonce_end (&init_once);
	}

	// init as null
	*out_file_hash_length = 0;
	*out_hcat_admin = NULL;
	*out_file_hash = NULL;

	if (_CryptCATAdminAcquireContext2 && !_CryptCATAdminAcquireContext2 (&hcat_admin, &DriverActionVerify, algorithm_id, NULL, 0))
	{
		return FALSE;
	}
	else
	{
		if (!CryptCATAdminAcquireContext (&hcat_admin, &DriverActionVerify, 0))
			return FALSE;
	}

	file_hash = (PBYTE)_r_mem_allocate (file_hash_length);

	if (_CryptCATAdminCalcHashFromFileHandle2 && !_CryptCATAdminCalcHashFromFileHandle2 (hcat_admin, hfile, &file_hash_length, file_hash, 0))
	{
		file_hash = (PBYTE)_r_mem_reallocate (file_hash, file_hash_length);

		if (!_CryptCATAdminCalcHashFromFileHandle2 (hcat_admin, hfile, &file_hash_length, file_hash, 0))
		{
			CryptCATAdminReleaseContext (hcat_admin, 0);

			_r_mem_free (file_hash);

			return FALSE;
		}
	}
	else
	{
		if (!CryptCATAdminCalcHashFromFileHandle (hfile, &file_hash_length, file_hash, 0))
		{
			file_hash = (PBYTE)_r_mem_reallocate (file_hash, file_hash_length);

			if (!CryptCATAdminCalcHashFromFileHandle (hfile, &file_hash_length, file_hash, 0))
			{
				CryptCATAdminReleaseContext (hcat_admin, 0);

				_r_mem_free (file_hash);

				return FALSE;
			}
		}
	}

	*out_file_hash_length = file_hash_length;
	*out_hcat_admin = hcat_admin;
	*out_file_hash = file_hash;

	return TRUE;
}

_Ret_maybenull_
PR_STRING _app_verifygetstring (
	_In_ HANDLE state_data
)
{
	PCRYPT_PROVIDER_SGNR prov_signer;
	PCRYPT_PROVIDER_DATA prov_data;
	PCRYPT_PROVIDER_CERT prov_cert;
	PR_STRING string;
	ULONG idx = 0, length;

	prov_data = WTHelperProvDataFromStateData (state_data);

	if (prov_data)
	{
		while (TRUE)
		{
			prov_signer = WTHelperGetProvSignerFromChain (prov_data, idx, FALSE, 0);

			if (!prov_signer)
				break;

			prov_cert = WTHelperGetProvCertFromChain (prov_signer, idx);

			if (!prov_cert)
				break;

			length = CertGetNameStringW (prov_cert->pCert, CERT_NAME_ATTR_TYPE, 0, szOID_COMMON_NAME, NULL, 0) - 1;

			if (length > 1)
			{
				string = _r_obj_createstring_ex (NULL, length * sizeof (WCHAR));

				CertGetNameStringW (prov_cert->pCert, CERT_NAME_ATTR_TYPE, 0, szOID_COMMON_NAME, string->buffer, length + 1);

				_r_str_trimtonullterminator (&string->sr);

				return string;
			}

			idx += 1;
		}
	}

	return NULL;
}

LONG _app_verifyfromfile (
	_In_ ULONG union_choice,
	_In_ PVOID union_data,
	_In_ LPGUID action_id,
	_In_opt_ PVOID policy_callback,
	_Out_ PR_STRING_PTR signature_string
)
{
	WINTRUST_DATA trust_data = {0};
	LONG status;

	trust_data.cbStruct = sizeof (WINTRUST_DATA);
	trust_data.dwUIChoice = WTD_UI_NONE;
	trust_data.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;
	trust_data.pPolicyCallbackData = policy_callback;
	trust_data.dwUnionChoice = union_choice;
	trust_data.dwStateAction = WTD_STATEACTION_VERIFY;
	trust_data.dwProvFlags = WTD_SAFER_FLAG | WTD_DISABLE_MD2_MD4;

	if (union_choice == WTD_CHOICE_FILE)
	{
		trust_data.pFile = (PWINTRUST_FILE_INFO)union_data;
	}
	else if (union_choice == WTD_CHOICE_CATALOG)
	{
		trust_data.pCatalog = (PWINTRUST_CATALOG_INFO)union_data;
	}
	else
	{
		*signature_string = NULL;

		return STATUS_INVALID_PARAMETER;
	}

	if (_r_config_getboolean (L"IsOCSPEnabled", FALSE, NULL))
	{
		trust_data.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;
		trust_data.dwProvFlags = WTD_SAFER_FLAG;
	}
	else
	{
		trust_data.fdwRevocationChecks = WTD_REVOKE_NONE;
		trust_data.dwProvFlags = WTD_SAFER_FLAG | WTD_CACHE_ONLY_URL_RETRIEVAL;
	}

	trust_data.dwStateAction = WTD_STATEACTION_VERIFY;
	status = WinVerifyTrust (INVALID_HANDLE_VALUE, action_id, &trust_data);

	*signature_string = (status == ERROR_SUCCESS && trust_data.hWVTStateData) ? _app_verifygetstring (trust_data.hWVTStateData) : NULL;

	// close state data
	trust_data.dwStateAction = WTD_STATEACTION_CLOSE;
	WinVerifyTrust (INVALID_HANDLE_VALUE, action_id, &trust_data);

	return status;
}

NTSTATUS _app_verifyfilefromcatalog (
	_In_ HANDLE hfile,
	_In_ LPCWSTR path,
	_In_opt_ LPCWSTR algorithm_id,
	_Out_ PR_STRING_PTR signature_string
)
{
	GUID DriverActionVerify = DRIVER_ACTION_VERIFY;
	WINTRUST_CATALOG_INFO catalog_info = {0};
	DRIVER_VER_INFO ver_info = {0};
	CATALOG_INFO ci = {0};
	PR_STRING file_hash_tag, string = NULL;
	HCATADMIN hcat_admin;
	HCATINFO hcat_info;
	PVOID file_hash;
	LONG64 file_size;
	ULONG file_hash_length;
	NTSTATUS status;

	status = _r_fs_getsize (NULL, hfile, &file_size);

	if (!NT_SUCCESS (status))
	{
		*signature_string = NULL;

		return status;
	}

	if (!file_size || file_size > _r_calc_megabytes2bytes64 (32))
	{
		*signature_string = NULL;

		return STATUS_FILE_TOO_LARGE;
	}

	if (_app_calculatefilehash (hfile, algorithm_id, &file_hash, &file_hash_length, &hcat_admin))
	{
		hcat_info = CryptCATAdminEnumCatalogFromHash (hcat_admin, (PBYTE)file_hash, file_hash_length, 0, NULL);

		if (hcat_info)
		{
			file_hash_tag = _r_str_fromhex ((PUCHAR)file_hash, file_hash_length, TRUE);

			if (CryptCATCatalogInfoFromContext (hcat_info, &ci, 0))
			{
				// disable OS version checking by passing in a DRIVER_VER_INFO structure
				ver_info.cbStruct = sizeof (DRIVER_VER_INFO);

				catalog_info.cbStruct = sizeof (WINTRUST_CATALOG_INFO);
				catalog_info.pcwszCatalogFilePath = ci.wszCatalogFile;
				catalog_info.pcwszMemberFilePath = path;
				catalog_info.hMemberFile = hfile;
				catalog_info.pcwszMemberTag = file_hash_tag->buffer;
				catalog_info.pbCalculatedFileHash = (PBYTE)file_hash;
				catalog_info.cbCalculatedFileHash = file_hash_length;
				catalog_info.hCatAdmin = hcat_admin;

				status = _app_verifyfromfile (WTD_CHOICE_CATALOG, &catalog_info, &DriverActionVerify, &ver_info, &string);

				if (ver_info.pcSignerCertContext)
					CertFreeCertificateContext (ver_info.pcSignerCertContext);
			}

			CryptCATAdminReleaseCatalogContext (hcat_admin, hcat_info, 0);

			_r_obj_dereference (file_hash_tag);
		}

		*signature_string = string;

		CryptCATAdminReleaseContext (hcat_admin, 0);

		_r_mem_free (file_hash);
	}
	else
	{
		*signature_string = NULL;

		return TRUST_E_SUBJECT_FORM_UNKNOWN;
	}

	return status;
}

VOID _app_getfilesignatureinfo (
	_Inout_ PITEM_APP_INFO ptr_app_info,
	_In_ HANDLE hfile
)
{
	GUID WinTrustActionGenericVerifyV2 = WINTRUST_ACTION_GENERIC_VERIFY_V2;
	WINTRUST_FILE_INFO file_info = {0};
	PR_STRING string = NULL;
	LONG status;

	// clean value
	__try
	{
		if (ptr_app_info->signature_info)
			_r_obj_clearreference ((PVOID_PTR)&ptr_app_info->signature_info);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		NOTHING;
	}

	file_info.cbStruct = sizeof (WINTRUST_FILE_INFO);
	file_info.pcwszFilePath = ptr_app_info->path->buffer;
	file_info.hFile = hfile;

	status = _app_verifyfromfile (WTD_CHOICE_FILE, &file_info, &WinTrustActionGenericVerifyV2, NULL, &string);

	if (status == TRUST_E_NOSIGNATURE)
	{
		status = _app_verifyfilefromcatalog (hfile, ptr_app_info->path->buffer, _r_sys_isosversiongreaterorequal (WINDOWS_8) ? BCRYPT_SHA256_ALGORITHM : NULL, &string);

		if (status == TRUST_E_NOSIGNATURE)
			_app_verifyfilefromcatalog (hfile, ptr_app_info->path->buffer, BCRYPT_SHA1_ALGORITHM, &string);
	}

	__try
	{
		_r_obj_movereference ((PVOID_PTR)&ptr_app_info->signature_info, string);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		NOTHING;
	}
}

VOID _app_getfileversioninfo (
	_Inout_ PITEM_APP_INFO ptr_app_info
)
{
	VS_FIXEDFILEINFO *ver_info = NULL;
	PR_STRING version_string = NULL;
	R_STORAGE ver_block = {0};
	R_STRINGBUILDER sb;
	HINSTANCE hlibrary;
	PR_STRING string;
	LCID lcid;
	NTSTATUS status;

	// clean value
	__try
	{
		if (ptr_app_info->version_info)
			_r_obj_clearreference ((PVOID_PTR)&ptr_app_info->version_info);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		NOTHING;
	}

	status = _r_sys_loadlibraryasresource (&hlibrary, &ptr_app_info->path->sr);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	status = _r_res_loadresource (&ver_block, hlibrary, RT_VERSION, MAKEINTRESOURCE (VS_VERSION_INFO), 0);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	_r_obj_initializestringbuilder (&sb, 0);

	lcid = _r_res_querytranslation (ver_block.buffer);

	// get file description
	string = _r_res_querystring (ver_block.buffer, L"FileDescription", lcid);

	if (string)
	{
		_r_obj_appendstringbuilder (&sb, SZ_TAB);
		_r_obj_appendstringbuilder2 (&sb, &string->sr);

		_r_obj_dereference (string);
	}

	// get file version
	if (_r_res_queryversion ((PVOID_PTR)&ver_info, ver_block.buffer))
	{
		_r_obj_appendstringbuilder (&sb, _r_obj_isstringempty2 (sb.string) ? SZ_TAB : L" ");
		_r_obj_appendstringbuilderformat (&sb, L"%d.%d", HIWORD (ver_info->dwFileVersionMS), LOWORD (ver_info->dwFileVersionMS));

		if (HIWORD (ver_info->dwFileVersionLS) || LOWORD (ver_info->dwFileVersionLS))
		{
			_r_obj_appendstringbuilderformat (&sb, L".%d", HIWORD (ver_info->dwFileVersionLS));

			if (LOWORD (ver_info->dwFileVersionLS))
				_r_obj_appendstringbuilderformat (&sb, L".%d", LOWORD (ver_info->dwFileVersionLS));
		}
	}

	if (!_r_obj_isstringempty2 (sb.string))
		_r_obj_appendstringbuilder (&sb, SZ_CRLF);

	// get file company
	string = _r_res_querystring (ver_block.buffer, L"CompanyName", lcid);

	if (string)
	{
		_r_obj_appendstringbuilder (&sb, SZ_TAB);
		_r_obj_appendstringbuilder2 (&sb, &string->sr);
		_r_obj_appendstringbuilder (&sb, SZ_CRLF);

		_r_obj_dereference (string);
	}

	version_string = _r_obj_finalstringbuilder (&sb);

	_r_str_trimstring2 (&version_string->sr, DIVIDER_TRIM, 0);

CleanupExit:

	ptr_app_info->version_info = version_string;

	if (hlibrary)
		_r_sys_freelibrary (hlibrary);
}

_Ret_maybenull_
PR_STRING _app_getfilehashinfo (
	_In_ HANDLE hfile,
	_In_ ULONG app_hash
)
{
	PITEM_APP ptr_app;
	PR_STRING string;

	ptr_app = _app_getappitem (app_hash);

	if (!ptr_app)
		return NULL;

	_r_crypt_getfilehash (&string, BCRYPT_SHA256_ALGORITHM, NULL, hfile);

	_r_obj_movereference ((PVOID_PTR)&ptr_app->hash, string);

	return ptr_app->hash;
}

ULONG _app_addcolor (
	_In_ ULONG locale_id,
	_In_ LPCWSTR config_name,
	_In_ LPCWSTR config_value,
	_In_ COLORREF default_clr,
	_In_ BOOLEAN is_enabled
)
{
	ITEM_COLOR ptr_clr = {0};
	ULONG hash_code;

	ptr_clr.config_name = _r_obj_createstring (config_name);
	ptr_clr.config_value = _r_obj_createstring (config_value);
	ptr_clr.new_clr = _r_config_getulong (config_value, default_clr, L"colors");

	ptr_clr.default_clr = default_clr;
	ptr_clr.locale_id = locale_id;
	ptr_clr.is_enabled = is_enabled;

	hash_code = _r_str_gethash (&ptr_clr.config_name->sr, TRUE);

	_r_obj_addhashtableitem (colors_table, hash_code, &ptr_clr);

	return hash_code;
}

_Success_ (return != 0)
COLORREF _app_getcolorvalue (
	_In_ ULONG color_hash
)
{
	PITEM_COLOR ptr_clr;

	ptr_clr = (PITEM_COLOR)_r_obj_findhashtable (colors_table, color_hash);

	if (ptr_clr)
		return ptr_clr->new_clr ? ptr_clr->new_clr : ptr_clr->default_clr;

	return 0;
}

BOOLEAN _app_setruletoapp (
	_In_ HWND hwnd,
	_Inout_ PITEM_RULE ptr_rule,
	_In_ INT item_id,
	_In_ PITEM_APP ptr_app,
	_In_ BOOLEAN is_enable
)
{
	INT listview_id;

	if ((ptr_rule->is_fordriver || ptr_rule->is_forservice) && _app_issystemhash (ptr_app->app_hash))
		return FALSE;

	if (is_enable == (_r_obj_findhashtable (ptr_rule->apps, ptr_app->app_hash) != NULL))
		return FALSE;

	if (is_enable)
	{
		_r_obj_addhashtableitem (ptr_rule->apps, ptr_app->app_hash, NULL);

		_app_ruleenable (ptr_rule, TRUE, TRUE);
	}
	else
	{
		_r_obj_removehashtableitem (ptr_rule->apps, ptr_app->app_hash);

		if (_r_obj_isempty (ptr_rule->apps))
			_app_ruleenable (ptr_rule, FALSE, TRUE);
	}

	if (item_id != INT_ERROR)
	{
		listview_id = _app_listview_getbytype (ptr_rule->type);

		if (listview_id != 0)
			_app_listview_updateitemby_param (hwnd, _app_listview_getitemcontext (hwnd, listview_id, item_id), FALSE);
	}

	_app_listview_updateitemby_param (hwnd, ptr_app->app_hash, TRUE);

	return TRUE;
}

_Success_ (return)
BOOLEAN _app_parsenetworkstring (
	_In_ PCR_STRINGREF network_string,
	_Inout_ PITEM_ADDRESS address
)
{
	NET_ADDRESS_INFO ni, ni_end;
	WCHAR rule_string[0x100];
	ULONG mask, types;
	USHORT port = 0, range_port1 = 0, range_port2 = 0;
	BYTE prefix_length;
	ULONG status;

	types = NET_STRING_IP_ADDRESS | NET_STRING_IP_ADDRESS_NO_SCOPE | NET_STRING_IP_SERVICE | NET_STRING_IP_NETWORK;

	if (address->is_range)
	{
		status = ParseNetworkString (address->range_start, types, &ni, &range_port1, NULL);

		if (status != ERROR_SUCCESS)
		{
			_r_log (LOG_LEVEL_INFO, NULL, L"ParseNetworkString", status, address->range_start);

			return FALSE;
		}

		status = ParseNetworkString (address->range_end, types, &ni_end, &range_port2, NULL);

		if (status != ERROR_SUCCESS)
		{
			_r_log (LOG_LEVEL_INFO, NULL, L"ParseNetworkString", status, address->range_end);

			return FALSE;
		}

		if (range_port2)
		{
			port = range_port2;
		}
		else if (range_port1)
		{
			port = range_port1;
		}
		else
		{
			port = 0;
		}
	}
	else
	{
		_r_str_copystring (rule_string, RTL_NUMBER_OF (rule_string), network_string);

		status = ParseNetworkString (rule_string, types, &ni, &port, &prefix_length);

		if (status != ERROR_SUCCESS)
		{
			_r_log (LOG_LEVEL_INFO, NULL, L"ParseNetworkString", status, rule_string);

			return FALSE;
		}
	}

	address->format = ni.Format;
	address->port = port;

	if (ni.Format == NET_ADDRESS_IPV4)
	{
		if (address->is_range)
		{
			address->range.valueLow.type = FWP_UINT32;
			address->range.valueLow.uint32 = _r_byteswap_ulong (ni.Ipv4Address.sin_addr.S_un.S_addr);

			address->range.valueHigh.type = FWP_UINT32;
			address->range.valueHigh.uint32 = _r_byteswap_ulong (ni_end.Ipv4Address.sin_addr.S_un.S_addr);
		}
		else
		{
			mask = 0;

			if (ConvertLengthToIpv4Mask (prefix_length, &mask) == NOERROR)
				mask = _r_byteswap_ulong (mask);

			address->addr4.addr = _r_byteswap_ulong (ni.Ipv4Address.sin_addr.S_un.S_addr);
			address->addr4.mask = mask;
		}

		return TRUE;
	}
	else if (ni.Format == NET_ADDRESS_IPV6)
	{
		if (address->is_range)
		{
			address->range.valueLow.type = FWP_BYTE_ARRAY16_TYPE;

			RtlCopyMemory (address->addr6_low, ni.Ipv6Address.sin6_addr.u.Byte, FWP_V6_ADDR_SIZE);
			address->range.valueLow.byteArray16 = (FWP_BYTE_ARRAY16*)address->addr6_low;

			address->range.valueHigh.type = FWP_BYTE_ARRAY16_TYPE;

			RtlCopyMemory (address->addr6_high, ni_end.Ipv6Address.sin6_addr.u.Byte, FWP_V6_ADDR_SIZE);
			address->range.valueHigh.byteArray16 = (FWP_BYTE_ARRAY16*)address->addr6_high;
		}
		else
		{
			RtlCopyMemory (address->addr6.addr, ni.Ipv6Address.sin6_addr.u.Byte, FWP_V6_ADDR_SIZE);
			address->addr6.prefixLength = min (prefix_length, 128);
		}

		return TRUE;
	}
	else
	{
		// unknown address family
	}

	return FALSE;
}

_Success_ (return)
BOOLEAN _app_preparserulestring (
	_In_ PCR_STRINGREF rule,
	_Out_ PITEM_ADDRESS address
)
{
	WCHAR valid_chars[] = {
		L'.',
		L':',
		L'[',
		L']',
		L'-',
		L'_',
		L'/',
	};

	R_STRINGREF range_start_part, range_end_part;
	WCHAR rule_string[0x100];
	ULONG_PTR length;
	ULONG types;
	BOOLEAN is_valid;
	ULONG status;

	length = _r_str_getlength2 (rule);

	for (ULONG_PTR i = 0; i < length; i++)
	{
		if (IsCharAlphaNumericW (rule->buffer[i]))
			continue;

		is_valid = FALSE;

		for (ULONG_PTR j = 0; j < RTL_NUMBER_OF (valid_chars); j++)
		{
			if (rule->buffer[i] == valid_chars[j])
			{
				is_valid = TRUE;
				break;
			}
		}

		if (!is_valid)
			return FALSE;
	}

	// parse rule range
	address->is_range = _r_str_splitatchar (rule, DIVIDER_RULE_RANGE, &range_start_part, &range_end_part);

	// extract start and end position of rule
	if (address->is_range)
	{
		// there is incorrect range syntax
		if (_r_obj_isstringempty2 (&range_start_part) || _r_obj_isstringempty2 (&range_end_part))
			return FALSE;

		_r_str_copystring (address->range_start, RTL_NUMBER_OF (address->range_start), &range_start_part);
		_r_str_copystring (address->range_end, RTL_NUMBER_OF (address->range_end), &range_end_part);
	}

	// check rule for port
	if (address->type == DATA_UNKNOWN)
	{
		address->type = DATA_TYPE_PORT;

		for (ULONG_PTR i = 0; i < length; i++)
		{
			if (!_r_str_isdigit (rule->buffer[i]) && rule->buffer[i] != DIVIDER_RULE_RANGE)
			{
				address->type = DATA_UNKNOWN;
				break;
			}
		}
	}

	if (address->type != DATA_UNKNOWN)
		return TRUE;

	types = NET_STRING_IP_ADDRESS | NET_STRING_IP_SERVICE | NET_STRING_IP_NETWORK | NET_STRING_IP_ADDRESS_NO_SCOPE | NET_STRING_IP_SERVICE_NO_SCOPE;

	// check rule for ip address
	if (address->is_range)
	{
		status = ParseNetworkString (address->range_start, types, NULL, NULL, NULL);

		if (status != ERROR_SUCCESS)
		{
			_r_log (LOG_LEVEL_INFO, NULL, L"ParseNetworkString", status, address->range_start);

			return FALSE;
		}

		status = ParseNetworkString (address->range_end, types, NULL, NULL, NULL);

		if (status != ERROR_SUCCESS)
		{
			_r_log (LOG_LEVEL_INFO, NULL, L"ParseNetworkString", status, address->range_end);

			return FALSE;
		}

		address->type = DATA_TYPE_IP;

		return TRUE;
	}
	else
	{
		_r_str_copystring (rule_string, RTL_NUMBER_OF (rule_string), rule);

		status = ParseNetworkString (rule_string, types, NULL, NULL, NULL);

		if (status == ERROR_SUCCESS)
		{
			address->type = DATA_TYPE_IP;

			return TRUE;
		}
		else
		{
			_r_log (LOG_LEVEL_INFO, NULL, L"ParseNetworkString", status, rule_string);
		}
	}

	return FALSE;
}

_Success_ (return)
BOOLEAN _app_parserulestring (
	_In_opt_ PCR_STRINGREF rule,
	_Out_ PITEM_ADDRESS address
)
{
	R_STRINGREF sr;

	RtlZeroMemory (address, sizeof (ITEM_ADDRESS));

	if (_r_obj_isstringempty (rule))
		return TRUE;

	// parse rule string
	if (!_app_preparserulestring (rule, address))
		return FALSE;

	if (address->type == DATA_TYPE_PORT)
	{
		if (address->is_range)
		{
			// ...port range
			_r_obj_initializestringref (&sr, address->range_start);

			address->range.valueLow.type = FWP_UINT16;
			address->range.valueLow.uint16 = (UINT16)_r_str_toulong (&sr);

			_r_obj_initializestringref (&sr, address->range_end);

			address->range.valueHigh.type = FWP_UINT16;
			address->range.valueHigh.uint16 = (UINT16)_r_str_toulong (&sr);

			if (address->range.valueLow.uint16 < 1 || address->range.valueHigh.uint16 > 65535)
				return FALSE;

			if (address->range.valueLow.uint16 >= address->range.valueHigh.uint16)
				return FALSE;

			return TRUE;
		}
		else
		{
			// ...port
			address->port = (UINT16)_r_str_toulong (rule);

			if (address->port < 1 || address->port > 65535)
				return FALSE;

			return TRUE;
		}
	}
	else if (address->type == DATA_TYPE_IP)
	{
		return _app_parsenetworkstring (rule, address);
	}

	return TRUE;
}

_Ret_maybenull_
PR_STRING _app_resolveaddress (
	_In_ ADDRESS_FAMILY af,
	_In_ LPCVOID address
)
{
	PR_STRING arpa_string, string = NULL;
	PDNS_RECORD dns_records = NULL;
	ULONG arpa_hash;
	DNS_STATUS status;

	arpa_string = _app_formatarpa (af, address);
	arpa_hash = _r_str_gethash (&arpa_string->sr, TRUE);

	if (_app_getcachetable (cache_resolution, arpa_hash, &lock_cache_resolution, &string))
	{
		_r_obj_dereference (arpa_string);

		return string;
	}

	status = DnsQuery_W (arpa_string->buffer, DNS_TYPE_PTR, DNS_QUERY_BYPASS_CACHE | DNS_QUERY_NO_HOSTS_FILE | DNS_QUERY_NO_MULTICAST, NULL, &dns_records, NULL);

	if (status == ERROR_SUCCESS)
	{
		if (dns_records)
		{
			for (PDNS_RECORD dns_record = dns_records; dns_record; dns_record = dns_record->pNext)
			{
				if (dns_record->wType == DNS_TYPE_PTR)
				{
					string = _r_obj_createstring (dns_record->Data.PTR.pNameHost);
					break;
				}
			}

			DnsFree (dns_records, DnsFreeRecordList);
		}
	}

	_app_addcachetable (cache_resolution, arpa_hash, &lock_cache_resolution, _r_obj_referencesafe (string));

	_r_obj_dereference (arpa_string);

	return string;
}

PR_STRING _app_resolveaddress_interlocked (
	_In_ volatile PVOID_PTR string,
	_In_ ADDRESS_FAMILY af,
	_In_ LPCVOID address,
	_In_ BOOLEAN is_resolutionenabled
)
{
	PR_STRING current_string, new_string;

	current_string = (PR_STRING)_InterlockedCompareExchangePointer (string, NULL, NULL);

	if (current_string)
		return current_string;

	if (is_resolutionenabled)
	{
		new_string = _app_resolveaddress (af, address);

		if (!new_string)
			new_string = _r_obj_referenceemptystring ();
	}
	else
	{
		new_string = _r_obj_referenceemptystring ();
	}

	current_string = (PR_STRING)_InterlockedCompareExchangePointer (string, new_string, NULL);

	if (!current_string)
	{
		current_string = new_string;
	}
	else
	{
		_r_obj_dereference (new_string);
	}

	return current_string;
}

VOID _app_fileloggingenable ()
{
	if (_r_config_getboolean (L"IsEnableAppMonitor", FALSE, NULL))
	{
		// TODO! rewrite this and watch FS changes through NtNotifyChangeDirectoryFile, then read file hash!
		if (NT_SUCCESS (_r_sys_createthread (&config.hmonitor_thread, NtCurrentProcess (), &_app_timercallback, NULL, NULL, L"FileMonitor")))
			NtResumeThread (config.hmonitor_thread, NULL);
	}
	else
	{
		if (config.hmonitor_thread)
		{
			_r_sys_terminatethread (config.hmonitor_thread, STATUS_CANCELLED);
			NtClose (config.hmonitor_thread);

			config.hmonitor_thread = NULL;
		}
	}
}

VOID NTAPI _app_timercallback (
	_In_opt_ PVOID context
)
{
	PITEM_APP ptr_app = NULL;
	PR_STRING string;
	ULONG_PTR enum_key;
	NTSTATUS status;

	while (TRUE)
	{
		_r_queuedlock_acquireshared (&lock_apps);

		enum_key = 0;

		while (_r_obj_enumhashtablepointer (apps_table, (PVOID_PTR)&ptr_app, NULL, &enum_key))
		{
			if (!ptr_app->hash || !_app_isappvalidbinary (ptr_app->real_path) || !_app_isappused (ptr_app))
				continue;

			status = _r_crypt_getfilehash (&string, BCRYPT_SHA256_ALGORITHM, &ptr_app->real_path->sr, NULL);

			if (NT_SUCCESS (status))
			{
				if (!_r_str_isequal (&ptr_app->hash->sr, &string->sr, TRUE))
				{
					_r_obj_movereference ((PVOID_PTR)&ptr_app->hash, string);

					_app_setappinfo (ptr_app, INFO_DISABLE, NULL);
				}
				else
				{
					_r_obj_dereference (string);
				}
			}
		}

		_r_queuedlock_releaseshared (&lock_apps);

		_r_sys_waitforsingleobject (NtCurrentThread (), _r_calc_minutes2milliseconds (10));
	}
}

VOID _app_getfileinformation (
	_In_ PR_STRING path,
	_In_ ULONG app_hash,
	_In_ ENUM_TYPE_DATA type,
	_In_ INT listview_id
)
{
	PITEM_APP_INFO ptr_app_info;

	if (_r_obj_isstringempty (path))
		return;

	ptr_app_info = _app_getappinfobyhash2 (app_hash);

	if (ptr_app_info)
	{
		// all information are already set!
		if (ptr_app_info->is_loaded)
		{
			_r_obj_dereference (ptr_app_info);
			return;
		}
	}
	else
	{
		ptr_app_info = (PITEM_APP_INFO)_r_obj_allocate (sizeof (ITEM_APP_INFO), &_app_dereferenceappinfo);

		ptr_app_info->path = _r_obj_reference (path);
		ptr_app_info->app_hash = app_hash;
		ptr_app_info->type = type;
		ptr_app_info->listview_id = listview_id;

		_r_queuedlock_acquireexclusive (&lock_cache_information);
		_r_obj_addhashtablepointer (cache_information, app_hash, _r_obj_reference (ptr_app_info));
		_r_queuedlock_releaseexclusive (&lock_cache_information);
	}

	// check for binary path is valid
	if (_app_isappvalidbinary (path))
	{
		_r_workqueue_queueitem (&file_queue, &_app_queue_fileinformation, ptr_app_info);
	}
	else
	{
		_r_obj_dereference (ptr_app_info); // cleanup!
	}
}

VOID _app_queue_resolver (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ ULONG hash_code,
	_In_ PVOID base_address
)
{
	PITEM_CONTEXT context;

	context = (PITEM_CONTEXT)_r_freelist_allocateitem (&context_free_list);

	context->hwnd = hwnd;
	context->listview_id = listview_id;
	context->lparam = hash_code;
	context->base_address = _r_obj_reference (base_address);

	_r_workqueue_queueitem (&resolver_queue, &_app_queue_resolveinformation, context);
}

VOID NTAPI _app_queue_fileinformation (
	_In_ PVOID arglist
)
{
	PITEM_APP_INFO ptr_app_info;
	HANDLE hfile;
	HWND hwnd;
	NTSTATUS status;

	ptr_app_info = (PITEM_APP_INFO)arglist;

	if (ptr_app_info->is_loaded)
		return;

	hwnd = _r_app_gethwnd ();

	status = _r_fs_openfile (&hfile, &ptr_app_info->path->sr, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE, 0, FALSE);

	if (!NT_SUCCESS (status))
	{
		if (status != STATUS_OBJECT_NAME_NOT_FOUND && status != STATUS_OBJECT_PATH_NOT_FOUND && status != STATUS_ACCESS_DENIED)
			_r_log (LOG_LEVEL_ERROR, NULL, L"_r_fs_openfile", status, ptr_app_info->path->buffer);

		return;
	}

	// query app icon
	_app_getfileicon (ptr_app_info);

	// query certificate information
	if (_r_config_getboolean (L"IsCertificatesEnabled", TRUE, NULL))
		_app_getfilesignatureinfo (ptr_app_info, hfile);

	// query version info
	_app_getfileversioninfo (ptr_app_info);

	// query sha256 info
	if (_r_config_getboolean (L"IsHashesEnabled", FALSE, NULL))
		_app_getfilehashinfo (hfile, ptr_app_info->app_hash);

	// redraw listview
	if (hwnd && _r_wnd_isvisible (hwnd, FALSE) && ptr_app_info->listview_id != 0)
		_r_listview_redraw (hwnd, ptr_app_info->listview_id);

	ptr_app_info->is_loaded = TRUE;

	_r_obj_dereference (ptr_app_info);

	NtClose (hfile);
}

VOID NTAPI _app_queue_notifyinformation (
	_In_ PVOID arglist
)
{
	PR_STRING address_str, host_str = NULL, localized_string, signature_str = NULL;
	PITEM_APP_INFO ptr_app_info;
	PITEM_CONTEXT context;
	PITEM_LOG ptr_log;
	HANDLE hfile;
	HICON hicon;
	HDWP hdefer;
	ULONG attempts = 6;
	BOOLEAN is_iconset = FALSE;
	NTSTATUS status;

	context = (PITEM_CONTEXT)arglist;

	ptr_log = (PITEM_LOG)context->base_address;

	// query notification host name
	if (_r_config_getboolean (L"IsNetworkResolutionsEnabled", TRUE, NULL))
	{
		host_str = _app_resolveaddress_interlocked ((volatile PVOID_PTR)&ptr_log->remote_host_str, ptr_log->af, &ptr_log->remote_addr, TRUE);
		host_str = _r_obj_reference (host_str);
	}

	// query signature
	if (_r_config_getboolean (L"IsCertificatesEnabled", TRUE, NULL))
	{
		ptr_app_info = _app_getappinfobyhash2 (ptr_log->app_hash);

		if (ptr_app_info)
		{
			if (_app_isappvalidbinary (ptr_app_info->path))
			{
				status = _r_fs_openfile (&hfile, &ptr_app_info->path->sr, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE, 0, FALSE);

				if (NT_SUCCESS (status))
				{
					_app_getfilesignatureinfo (ptr_app_info, hfile);

					NtClose (hfile);
				}
			}

			_r_obj_dereference (ptr_app_info);
		}
	}

	// query file icon
	hicon = _app_icons_getsafeapp_hicon (ptr_log->app_hash);

	if (_r_wnd_isvisible (context->hwnd, FALSE))
	{
		if (ptr_log->app_hash == _app_notify_getapp_id (context->hwnd))
		{
			// set file icon
			_app_notify_setapp_icon (context->hwnd, hicon);

			is_iconset = TRUE;

			// set signature information
			localized_string = _r_obj_concatstrings (
				2,
				_r_locale_getstring (IDS_SIGNATURE),
				L":"
			);

			do
			{
				if (!_app_getappinfoparam2 (ptr_log->app_hash, 0, INFO_SIGNATURE_STRING, &signature_str, sizeof (PR_STRING)))
				{
					_r_sys_sleep (250); // wait
				}
				else
				{
					break;
				}
			}
			while (--attempts);

			if (!signature_str)
				_r_obj_movereference ((PVOID_PTR)&signature_str, _r_locale_getstring_ex (IDS_SIGN_UNSIGNED));

			hdefer = BeginDeferWindowPos (2);

			// set signature string
			_r_ctrl_settablestring (context->hwnd, &hdefer, IDC_SIGNATURE_ID, &localized_string->sr, IDC_SIGNATURE_TEXT, &signature_str->sr);

			// query address string
			address_str = _app_formataddress (ptr_log->af, ptr_log->protocol, &ptr_log->remote_addr, 0, FMTADDR_USE_PROTOCOL);

			// set address string
			_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_ADDRESS), L":"));

			if (_r_obj_isstringempty (address_str))
				_r_obj_movereference ((PVOID_PTR)&address_str, _r_locale_getstring_ex (IDS_STATUS_EMPTY));

			_r_ctrl_settablestring (context->hwnd, &hdefer, IDC_ADDRESS_ID, &localized_string->sr, IDC_ADDRESS_TEXT, &address_str->sr);

			// set host string
			_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_HOST), L":"));

			if (_r_obj_isstringempty (host_str))
				_r_obj_movereference ((PVOID_PTR)&host_str, _r_locale_getstring_ex (IDS_STATUS_EMPTY));

			_r_ctrl_settablestring (context->hwnd, &hdefer, IDC_HOST_ID, &localized_string->sr, IDC_HOST_TEXT, &host_str->sr);

			_r_obj_dereference (localized_string);

			if (address_str)
				_r_obj_dereference (address_str);

			if (hdefer)
				EndDeferWindowPos (hdefer);
		}
	}

	_r_freelist_deleteitem (&context_free_list, context);

	if (!is_iconset && hicon)
		DestroyIcon (hicon);

	if (signature_str)
		_r_obj_dereference (signature_str);

	if (host_str)
		_r_obj_dereference (host_str);

	_r_obj_dereference (ptr_log);
}

VOID NTAPI _app_queue_resolveinformation (
	_In_ PVOID arglist
)
{
	PITEM_NETWORK ptr_network;
	PITEM_CONTEXT context;
	PITEM_LOG ptr_log;
	BOOLEAN is_resolutionenabled;

	context = (PITEM_CONTEXT)arglist;

	is_resolutionenabled = _r_config_getboolean (L"IsNetworkResolutionsEnabled", TRUE, NULL);

	switch (context->listview_id)
	{
		case IDC_LOG:
		{
			ptr_log = (PITEM_LOG)context->base_address;

			_app_resolveaddress_interlocked ((volatile PVOID_PTR)&ptr_log->remote_host_str, ptr_log->af, &ptr_log->remote_addr, is_resolutionenabled);
			_app_resolveaddress_interlocked ((volatile PVOID_PTR)&ptr_log->local_host_str, ptr_log->af, &ptr_log->local_addr, is_resolutionenabled);

			break;
		}

		case IDC_NETWORK:
		{
			ptr_network = (PITEM_NETWORK)context->base_address;

			_app_formataddress_interlocked ((volatile PVOID_PTR)&ptr_network->remote_addr_str, ptr_network->af, &ptr_network->remote_addr);
			_app_formataddress_interlocked ((volatile PVOID_PTR)&ptr_network->local_addr_str, ptr_network->af, &ptr_network->local_addr);
			_app_resolveaddress_interlocked ((volatile PVOID_PTR)&ptr_network->remote_host_str, ptr_network->af, &ptr_network->remote_addr, is_resolutionenabled);
			_app_resolveaddress_interlocked ((volatile PVOID_PTR)&ptr_network->local_host_str, ptr_network->af, &ptr_network->local_addr, is_resolutionenabled);

			break;
		}
	}

	// redraw listview
	if (_r_wnd_isvisible (context->hwnd, FALSE))
		_r_listview_redraw (context->hwnd, context->listview_id);

	_r_obj_dereference (context->base_address);

	_r_freelist_deleteitem (&context_free_list, context);
}

BOOLEAN _app_wufixenabled ()
{
	if (!_r_config_getboolean (L"IsWUFixEnabled", FALSE, NULL))
		return FALSE;

	return _r_fs_isexists (&config.wusvc_path->sr);
}

VOID _app_wufixhelper (
	_In_ SC_HANDLE hsvcmgr,
	_In_ LPCWSTR service_name,
	_In_ LPCWSTR k_value,
	_In_ BOOLEAN is_enable
)
{
	SERVICE_STATUS svc_status;
	WCHAR reg_key[0x80], reg_value[0x80];
	PR_STRING image_path;
	SC_HANDLE hsvc;
	HANDLE hkey;
	BOOLEAN is_enabled = FALSE;
	NTSTATUS status;

	_r_str_printf (reg_key, RTL_NUMBER_OF (reg_key), L"SYSTEM\\CurrentControlSet\\Services\\%s", service_name);

	status = _r_reg_openkey (&hkey, HKEY_LOCAL_MACHINE, reg_key, 0, KEY_READ | KEY_WRITE);

	if (!NT_SUCCESS (status))
	{
		_r_log (LOG_LEVEL_INFO, NULL, L"_r_reg_openkey", status, reg_key);
		return;
	}

	// query service path
	status = _r_reg_querystring (hkey, L"ImagePath", &image_path, NULL);

	if (NT_SUCCESS (status))
	{
		if (_r_str_isstartswith2 (&image_path->sr, is_enable ? L"%systemroot%\\system32\\wusvc.exe" : L"%systemroot%\\system32\\svchost.exe", TRUE))
			is_enabled = TRUE;

		_r_obj_dereference (image_path);
	}

	// set new image path
	_r_str_printf (reg_value, RTL_NUMBER_OF (reg_value), L"%%systemroot%%\\system32%s -k %s -p", is_enable ? PATH_WUSVC : PATH_SVCHOST, k_value);

	_r_reg_setvalue (hkey, L"ImagePath", REG_EXPAND_SZ, reg_value, (ULONG)(_r_str_getlength (reg_value) * sizeof (WCHAR) + sizeof (UNICODE_NULL)));

	// restart service
	if (is_enable != is_enabled)
	{
		hsvc = OpenServiceW (hsvcmgr, service_name, SERVICE_START | SERVICE_STOP | SERVICE_QUERY_STATUS);

		if (hsvc)
		{
			if (QueryServiceStatus (hsvc, &svc_status))
			{
				if (svc_status.dwCurrentState != SERVICE_STOPPED && svc_status.dwCurrentState != SERVICE_STOP_PENDING)
					ControlService (hsvc, SERVICE_CONTROL_STOP, &svc_status);
			}

			CloseServiceHandle (hsvc);
		}
	}

	NtClose (hkey);
}

VOID _app_wufixenable (
	_In_ HWND hwnd,
	_In_ BOOLEAN is_enable
)
{
	SC_HANDLE hsvcmgr;
	PITEM_APP ptr_app;
	PR_LIST rules;
	NTSTATUS status;

	hsvcmgr = OpenSCManagerW (NULL, NULL, SC_MANAGER_CONNECT | SERVICE_QUERY_STATUS | SERVICE_START | SERVICE_STOP);

	if (!hsvcmgr)
		return;

	if (is_enable)
	{
		if (_r_fs_isexists (&config.wusvc_path->sr))
			_r_fs_deletefile (&config.wusvc_path->sr, NULL);

		// TODO: windows cannot create symbolic link for another symlink, rewrite this
		status = _r_fs_createhardlink (&config.svchost_path->sr, &config.wusvc_path->sr);

		if (!NT_SUCCESS (status))
			_r_fs_copyfile (&config.svchost_path->sr, &config.wusvc_path->sr, FALSE);

		ptr_app = _app_addapplication (hwnd, DATA_UNKNOWN, config.wusvc_path, NULL, NULL);

		if (ptr_app)
		{
			_app_setappinfo (ptr_app, INFO_IS_ENABLED, LongToPtr (TRUE));
			_app_setappinfo (ptr_app, INFO_IS_UNDELETABLE, LongToPtr (TRUE));

			rules = _r_obj_createlist (0x02, NULL);

			_r_obj_addlistitem (rules, ptr_app, NULL);

			_wfp_createappfilters (_wfp_getenginehandle (), rules, DBG_ARG, FALSE);

			_r_obj_dereference (ptr_app);
			_r_obj_dereference (rules);
		}
	}
	else
	{
		if (_r_fs_isexists (&config.wusvc_path->sr))
		{
			ptr_app = _app_getappitem (_r_str_gethash (&config.wusvc_path->sr, TRUE));

			if (ptr_app)
			{
				_app_setappinfo (ptr_app, INFO_DISABLE, LongToPtr (FALSE));
				_app_setappinfo (ptr_app, INFO_IS_UNDELETABLE, LongToPtr (FALSE));

				_r_obj_dereference (ptr_app);
			}

			_r_fs_deletefile (&config.wusvc_path->sr, NULL);
		}
	}

	_app_wufixhelper (hsvcmgr, L"wuauserv", L"netsvcs", is_enable);
	_app_wufixhelper (hsvcmgr, L"DoSvc", L"NetworkService", is_enable);
	_app_wufixhelper (hsvcmgr, L"UsoSvc", L"netsvcs", is_enable);

	_r_config_setboolean (L"IsWUFixEnabled", is_enable, NULL);

	CloseServiceHandle (hsvcmgr);
}
