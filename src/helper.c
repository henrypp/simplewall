// simplewall
// Copyright (c) 2016-2022 Henry++

#include "global.h"

VOID NTAPI _app_dereferenceapp (
	_In_ PVOID entry
)
{
	PITEM_APP ptr_item;

	ptr_item = entry;

	if (ptr_item->display_name)
		_r_obj_dereference (ptr_item->display_name);

	if (ptr_item->real_path)
		_r_obj_dereference (ptr_item->real_path);

	if (ptr_item->short_name)
		_r_obj_dereference (ptr_item->short_name);

	if (ptr_item->original_path)
		_r_obj_dereference (ptr_item->original_path);

	if (ptr_item->notification)
		_r_obj_dereference (ptr_item->notification);

	if (ptr_item->guids)
		_r_obj_dereference (ptr_item->guids);
}

VOID NTAPI _app_dereferenceappinfo (
	_In_ PVOID entry
)
{
	PITEM_APP_INFO ptr_item;

	ptr_item = entry;

	if (ptr_item->path)
		_r_obj_dereference (ptr_item->path);

	if (ptr_item->signature_info)
		_r_obj_dereference (ptr_item->signature_info);

	if (ptr_item->version_info)
		_r_obj_dereference (ptr_item->version_info);
}

VOID NTAPI _app_dereferenceruleconfig (
	_In_ PVOID entry
)
{
	PITEM_RULE_CONFIG ptr_item;

	ptr_item = entry;

	if (ptr_item->name)
		_r_obj_dereference (ptr_item->name);

	if (ptr_item->apps)
		_r_obj_dereference (ptr_item->apps);
}

VOID NTAPI _app_dereferencelog (
	_In_ PVOID entry
)
{
	PITEM_LOG ptr_item;

	ptr_item = entry;

	if (ptr_item->path)
		_r_obj_dereference (ptr_item->path);

	if (ptr_item->filter_name)
		_r_obj_dereference (ptr_item->filter_name);

	if (ptr_item->layer_name)
		_r_obj_dereference (ptr_item->layer_name);

	if (ptr_item->username)
		_r_obj_dereference (ptr_item->username);

	if (ptr_item->protocol_str)
		_r_obj_dereference (ptr_item->protocol_str);

	if (ptr_item->local_addr_str)
		_r_obj_dereference (ptr_item->local_addr_str);

	if (ptr_item->remote_addr_str)
		_r_obj_dereference (ptr_item->remote_addr_str);

	if (ptr_item->local_host_str)
		_r_obj_dereference (ptr_item->local_host_str);

	if (ptr_item->remote_host_str)
		_r_obj_dereference (ptr_item->remote_host_str);
}

VOID NTAPI _app_dereferencenetwork (
	_In_ PVOID entry
)
{
	PITEM_NETWORK ptr_item;

	ptr_item = entry;

	if (ptr_item->path)
		_r_obj_dereference (ptr_item->path);

	if (ptr_item->protocol_str)
		_r_obj_dereference (ptr_item->protocol_str);

	if (ptr_item->local_addr_str)
		_r_obj_dereference (ptr_item->local_addr_str);

	if (ptr_item->remote_addr_str)
		_r_obj_dereference (ptr_item->remote_addr_str);

	if (ptr_item->local_host_str)
		_r_obj_dereference (ptr_item->local_host_str);

	if (ptr_item->remote_host_str)
		_r_obj_dereference (ptr_item->remote_host_str);
}

VOID NTAPI _app_dereferencerule (
	_In_ PVOID entry
)
{
	PITEM_RULE ptr_item;

	ptr_item = entry;

	if (ptr_item->apps)
		_r_obj_dereference (ptr_item->apps);

	if (ptr_item->name)
		_r_obj_dereference (ptr_item->name);

	if (ptr_item->rule_remote)
		_r_obj_dereference (ptr_item->rule_remote);

	if (ptr_item->rule_local)
		_r_obj_dereference (ptr_item->rule_local);

	if (ptr_item->protocol_str)
		_r_obj_dereference (ptr_item->protocol_str);

	if (ptr_item->guids)
		_r_obj_dereference (ptr_item->guids);
}

VOID _app_addcachetable (
	_Inout_ PR_HASHTABLE hashtable,
	_In_ ULONG_PTR hash_code,
	_In_ PR_QUEUED_LOCK spin_lock,
	_In_opt_ PR_STRING string
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

BOOLEAN _app_getcachetable (
	_Inout_ PR_HASHTABLE cache_table,
	_In_ ULONG_PTR hash_code,
	_In_ PR_QUEUED_LOCK spin_lock,
	_Out_ PR_STRING_PTR string
)
{
	PR_OBJECT_POINTER object_ptr;

	_r_queuedlock_acquireshared (spin_lock);

	object_ptr = _r_obj_findhashtable (cache_table, hash_code);

	_r_queuedlock_releaseshared (spin_lock);

	if (object_ptr)
	{
		*string = _r_obj_referencesafe (object_ptr->object_body);

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
	PIN_ADDR p4addr;
	PIN6_ADDR p6addr;

	_r_obj_initializestringbuilder (&formatted_address);

	if (af == AF_INET)
	{
		p4addr = (PIN_ADDR)address;

		_r_obj_appendstringbuilderformat (
			&formatted_address,
			L"%hhu.%hhu.%hhu.%hhu.%s",
			p4addr->s_impno,
			p4addr->s_lh,
			p4addr->s_host,
			p4addr->s_net,
			DNS_IP4_REVERSE_DOMAIN_STRING_W
		);
	}
	else if (af == AF_INET6)
	{
		p6addr = (PIN6_ADDR)address;

		for (INT i = sizeof (IN6_ADDR) - 1; i >= 0; i--)
		{
			_r_obj_appendstringbuilderformat (
				&formatted_address,
				L"%hhx.%hhx.",
				p6addr->s6_addr[i] & 0xF,
				(p6addr->s6_addr[i] >> 4) & 0xF
			);
		}

		_r_obj_appendstringbuilder (
			&formatted_address,
			DNS_IP6_REVERSE_DOMAIN_STRING_W
		);
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
	PR_STRING string;
	BOOLEAN is_success;

	_r_obj_initializestringbuilder (&formatted_address);

	is_success = FALSE;

	if ((flags & FMTADDR_USE_PROTOCOL))
	{
		string = _app_db_getprotoname (proto, af, FALSE);

		if (string)
		{
			_r_obj_appendstringbuilder2 (&formatted_address, string);
			_r_obj_appendstringbuilder (&formatted_address, L"://");

			_r_obj_dereference (string);
		}
	}

	if (_app_formatip (af, address, addr_str, RTL_NUMBER_OF (addr_str), !!(flags & FMTADDR_AS_RULE)))
	{
		if (af == AF_INET6 && port)
		{
			_r_obj_appendstringbuilderformat (&formatted_address, L"[%s]", addr_str);
		}
		else
		{
			_r_obj_appendstringbuilder (&formatted_address, addr_str);
		}

		is_success = TRUE;
	}

	if (port && !(flags & FMTADDR_USE_PROTOCOL))
	{
		_r_obj_appendstringbuilderformat (
			&formatted_address,
			is_success ? L":%" TEXT (PRIu16) : L"%" TEXT (PRIu16),
			port
		);

		is_success = TRUE;
	}

	if (is_success)
		return _r_obj_finalstringbuilder (&formatted_address);

	_r_obj_deletestringbuilder (&formatted_address);

	return NULL;
}

PR_STRING _app_formataddress_interlocked (
	_In_ PVOID volatile *string,
	_In_ ADDRESS_FAMILY af,
	_In_ LPCVOID address
)
{
	PR_STRING current_string;
	PR_STRING new_string;

	current_string = InterlockedCompareExchangePointer (
		string,
		NULL,
		NULL
	);

	if (current_string)
		return current_string;

	new_string = _app_formataddress (af, 0, address, 0, 0);

	current_string = InterlockedCompareExchangePointer (
		string,
		new_string,
		NULL
	);

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

_Success_ (return)
BOOLEAN _app_formatip (
	_In_ ADDRESS_FAMILY af,
	_In_ LPCVOID address,
	_Out_writes_to_ (buffer_size, buffer_size) LPWSTR buffer,
	_In_ ULONG buffer_size,
	_In_ BOOLEAN is_checkempty
)
{
	PIN_ADDR p4addr;
	PIN6_ADDR p6addr;
	NTSTATUS status;

	if (af == AF_INET)
	{
		p4addr = (PIN_ADDR)address;

		if (is_checkempty)
		{
			if (IN4_IS_ADDR_UNSPECIFIED (p4addr))
				return FALSE;
		}

		status = RtlIpv4AddressToStringEx (
			p4addr,
			0,
			buffer,
			&buffer_size
		);

		if (status == STATUS_SUCCESS)
			return TRUE;
	}
	else if (af == AF_INET6)
	{
		p6addr = (PIN6_ADDR)address;

		if (is_checkempty)
		{
			if (IN6_IS_ADDR_UNSPECIFIED (p6addr))
				return FALSE;
		}

		status = RtlIpv6AddressToStringEx (
			p6addr,
			0,
			0,
			buffer,
			&buffer_size
		);

		if (status == STATUS_SUCCESS)
			return TRUE;
	}

	return FALSE;
}

PR_STRING _app_formatport (
	_In_ UINT16 port,
	_In_ UINT8 proto
)
{
	LPCWSTR service_string;

	service_string = _app_db_getservicename (port, proto, NULL);

	if (service_string)
		return _r_format_string (L"%" TEXT (PRIu16) L" (%s)", port, service_string);

	return _r_format_string (L"%" TEXT (PRIu16), port);
}

_Ret_maybenull_
PITEM_APP_INFO _app_getappinfobyhash2 (
	_In_ ULONG_PTR app_hash
)
{
	PITEM_APP_INFO ptr_app_info;

	_r_queuedlock_acquireshared (&lock_cache_information);

	ptr_app_info = _r_obj_findhashtablepointer (cache_information, app_hash);

	_r_queuedlock_releaseshared (&lock_cache_information);

	return ptr_app_info;
}

_Success_ (return)
BOOLEAN _app_getappinfoparam2 (
	_In_ ULONG_PTR app_hash,
	_In_ ENUM_INFO_DATA2 info_data,
	_Out_ PVOID_PTR buffer_ptr
)
{
	PITEM_APP_INFO ptr_app_info;
	PITEM_APP ptr_app;
	LONG icon_id;
	BOOLEAN is_success;

	is_success = FALSE;

	*buffer_ptr = NULL;

	ptr_app_info = _app_getappinfobyhash2 (app_hash);

	if (!ptr_app_info)
	{
		// fallback
		switch (info_data)
		{
			case INFO_ICON_ID:
			{
				ptr_app = _app_getappitem (app_hash);

				if (ptr_app)
				{
					icon_id = _app_icons_getdefaultapp_id (ptr_app->type);

					_r_obj_dereference (ptr_app);
				}
				else
				{
					icon_id = _app_icons_getdefaultapp_id (DATA_APP_REGULAR);
				}

				*buffer_ptr = LongToPtr (icon_id);

				return TRUE;
			}
		}

		return FALSE;
	}

	switch (info_data)
	{
		case INFO_ICON_ID:
		{
			icon_id = InterlockedCompareExchange (
				&ptr_app_info->large_icon_id,
				0,
				0
			);

			if (icon_id)
			{
				*buffer_ptr = LongToPtr (icon_id);
				is_success = TRUE;
			}

			break;
		}

		case INFO_SIGNATURE_STRING:
		{
			if (ptr_app_info->signature_info)
			{
				*buffer_ptr = _r_obj_reference (ptr_app_info->signature_info);
				is_success = TRUE;
			}

			break;
		}

		case INFO_VERSION_STRING:
		{
			if (ptr_app_info->version_info)
			{
				*buffer_ptr = _r_obj_reference (ptr_app_info->version_info);
				is_success = TRUE;
			}

			break;
		}
	}

	_r_obj_dereference (ptr_app_info);

	return is_success;
}

BOOLEAN _app_isappsigned (
	_In_ ULONG_PTR app_hash
)
{
	PR_STRING string;
	BOOLEAN is_signed;

	if (_app_getappinfoparam2 (app_hash, INFO_SIGNATURE_STRING, &string))
	{
		is_signed = !_r_obj_isstringempty2 (string);

		_r_obj_dereference (string);

		return is_signed;
	}

	return FALSE;
}

BOOLEAN _app_isappvalidbinary (
	_In_ ENUM_TYPE_DATA type,
	_In_ PR_STRING path
)
{
	static R_STRINGREF valid_exts[] = {
		PR_STRINGREF_INIT (L".exe"),
		PR_STRINGREF_INIT (L".dll"),
	};

	if (type != DATA_APP_REGULAR && type != DATA_APP_SERVICE && type != DATA_APP_UWP)
		return FALSE;

	if (!_app_isappvalidpath (&path->sr))
		return FALSE;

	for (SIZE_T i = 0; i < RTL_NUMBER_OF (valid_exts); i++)
	{
		if (_r_str_isendsswith (&path->sr, &valid_exts[i], TRUE))
			return TRUE;
	}

	return FALSE;
}

BOOLEAN _app_isappvalidpath (
	_In_ PR_STRINGREF path
)
{
	if (path->length <= (3 * sizeof (WCHAR)))
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
		if (ptr_app->original_path)
			return _r_obj_reference (ptr_app->original_path);
	}

	if (ptr_app->type == DATA_APP_SERVICE)
	{
		if (ptr_app->original_path)
			return _r_obj_reference (ptr_app->original_path);
	}
	else if (ptr_app->type == DATA_APP_UWP)
	{
		if (ptr_app->display_name)
			return _r_obj_reference (ptr_app->display_name);

		if (ptr_app->real_path)
			return _r_obj_reference (ptr_app->real_path);

		if (ptr_app->original_path)
			return _r_obj_reference (ptr_app->original_path);
	}

	if (is_shortened || _r_config_getboolean (L"ShowFilenames", TRUE))
	{
		if (ptr_app->short_name)
			return _r_obj_reference (ptr_app->short_name);
	}

	if (ptr_app->real_path)
		return _r_obj_reference (ptr_app->real_path);

	return NULL;
}

_Ret_maybenull_
PR_STRING _app_getappname (
	_In_ PITEM_APP ptr_app
)
{
	if (ptr_app->type == DATA_APP_UWP || ptr_app->type == DATA_APP_SERVICE)
	{
		if (ptr_app->real_path)
			return _r_obj_reference (ptr_app->real_path);

		if (ptr_app->display_name)
			return _r_obj_reference (ptr_app->display_name);
	}

	if (ptr_app->original_path)
		return _r_obj_reference (ptr_app->original_path);

	if (ptr_app->real_path)
		return _r_obj_reference (ptr_app->real_path);

	return NULL;
}

VOID _app_getfileicon (
	_Inout_ PITEM_APP_INFO ptr_app_info
)
{
	LONG icon_id;
	BOOLEAN is_iconshidded;

	icon_id = 0;
	is_iconshidded = _r_config_getboolean (L"IsIconsHidden", FALSE);

	if (is_iconshidded || !_app_isappvalidbinary (ptr_app_info->type, ptr_app_info->path))
	{
		_app_icons_loadfromfile (
			NULL,
			ptr_app_info->type,
			&icon_id,
			NULL,
			TRUE
		);
	}
	else
	{
		_app_icons_loadfromfile (
			ptr_app_info->path,
			ptr_app_info->type,
			&icon_id,
			NULL,
			TRUE
		);
	}

	InterlockedCompareExchange (
		&ptr_app_info->large_icon_id,
		icon_id,
		ptr_app_info->large_icon_id
	);
}

_Success_ (return)
BOOLEAN _app_calculatefilehash (
	_In_ HANDLE hfile,
	_In_opt_ LPCWSTR algorithm_id,
	_Out_ PVOID_PTR file_hash_ptr,
	_Out_ PULONG file_hash_length_ptr,
	_Out_ HCATADMIN * hcat_admin_ptr
)
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static GUID DriverActionVerify = DRIVER_ACTION_VERIFY;

	static CCAAC2 _CryptCATAdminAcquireContext2 = NULL;
	static CCAHFFH2 _CryptCATAdminCalcHashFromFileHandle2 = NULL;

	HCATADMIN hcat_admin;
	HMODULE hwintrust;
	PBYTE file_hash;
	ULONG file_hash_length;

	if (_r_initonce_begin (&init_once))
	{
		hwintrust = _r_sys_loadlibrary (L"wintrust.dll");

		if (hwintrust)
		{
			_CryptCATAdminAcquireContext2 = (CCAAC2)GetProcAddress (
				hwintrust,
				"CryptCATAdminAcquireContext2"
			);

			_CryptCATAdminCalcHashFromFileHandle2 = (CCAHFFH2)GetProcAddress (
				hwintrust,
				"CryptCATAdminCalcHashFromFileHandle2"
			);

			//FreeLibrary (hwintrust);
		}

		_r_initonce_end (&init_once);
	}

	if (_CryptCATAdminAcquireContext2)
	{
		if (!_CryptCATAdminAcquireContext2 (
			&hcat_admin,
			&DriverActionVerify,
			algorithm_id,
			NULL,
			0))
		{
			return FALSE;
		}
	}
	else
	{
		if (!CryptCATAdminAcquireContext (&hcat_admin, &DriverActionVerify, 0))
			return FALSE;
	}

	file_hash_length = 32;
	file_hash = _r_mem_allocatezero (file_hash_length);

	if (_CryptCATAdminCalcHashFromFileHandle2)
	{
		if (!_CryptCATAdminCalcHashFromFileHandle2 (
			hcat_admin,
			hfile,
			&file_hash_length,
			file_hash,
			0))
		{
			file_hash = _r_mem_reallocatezero (file_hash, file_hash_length);

			if (!_CryptCATAdminCalcHashFromFileHandle2 (
				hcat_admin,
				hfile,
				&file_hash_length,
				file_hash,
				0))
			{
				CryptCATAdminReleaseContext (hcat_admin, 0);
				_r_mem_free (file_hash);

				return FALSE;
			}
		}
	}
	else
	{
		if (!CryptCATAdminCalcHashFromFileHandle (
			hfile,
			&file_hash_length,
			file_hash,
			0))
		{
			file_hash = _r_mem_reallocatezero (file_hash, file_hash_length);

			if (!CryptCATAdminCalcHashFromFileHandle (
				hfile,
				&file_hash_length,
				file_hash,
				0))
			{
				CryptCATAdminReleaseContext (hcat_admin, 0);
				_r_mem_free (file_hash);

				return FALSE;
			}
		}
	}

	*file_hash_ptr = file_hash;
	*file_hash_length_ptr = file_hash_length;
	*hcat_admin_ptr = hcat_admin;

	return TRUE;
}

_Ret_maybenull_
PR_STRING _app_verifygetstring (
	_In_ HANDLE state_data
)
{
	PCRYPT_PROVIDER_DATA prov_data;
	PCRYPT_PROVIDER_SGNR prov_signer;
	PCRYPT_PROVIDER_CERT prov_cert;

	PR_STRING string;
	ULONG length;

	ULONG idx;

	prov_data = WTHelperProvDataFromStateData (state_data);

	if (prov_data)
	{
		idx = 0;

		while (TRUE)
		{
			prov_signer = WTHelperGetProvSignerFromChain (prov_data, idx, FALSE, 0);

			if (!prov_signer)
				break;

			prov_cert = WTHelperGetProvCertFromChain (prov_signer, idx);

			if (!prov_cert)
				break;

			length = CertGetNameString (
				prov_cert->pCert,
				CERT_NAME_ATTR_TYPE,
				0,
				szOID_COMMON_NAME,
				NULL,
				0) - 1;

			if (length > 1)
			{
				string = _r_obj_createstring_ex (NULL, length * sizeof (WCHAR));

				CertGetNameString (
					prov_cert->pCert,
					CERT_NAME_ATTR_TYPE,
					0,
					szOID_COMMON_NAME,
					string->buffer,
					length + 1
				);

				_r_obj_trimstringtonullterminator (string);

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

	trust_data.cbStruct = sizeof (trust_data);
	trust_data.dwUIChoice = WTD_UI_NONE;
	trust_data.pPolicyCallbackData = policy_callback;
	trust_data.dwUnionChoice = union_choice;

	if (union_choice == WTD_CHOICE_CATALOG)
	{
		trust_data.pCatalog = union_data;
	}
	else
	{
		trust_data.pFile = union_data;
	}

	if (_r_config_getboolean (L"IsOCSPEnabled", FALSE))
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

	if (status == ERROR_SUCCESS && trust_data.hWVTStateData)
	{
		*signature_string = _app_verifygetstring (trust_data.hWVTStateData);
	}
	else
	{
		*signature_string = NULL;
	}

	// Close state data
	trust_data.dwStateAction = WTD_STATEACTION_CLOSE;
	WinVerifyTrust (INVALID_HANDLE_VALUE, action_id, &trust_data);

	return status;
}

LONG _app_verifyfilefromcatalog (
	_In_ HANDLE hfile,
	_In_ LPCWSTR file_path,
	_In_opt_ LPCWSTR algorithm_id,
	_Out_ PR_STRING_PTR signature_string
)
{
	static GUID DriverActionVerify = DRIVER_ACTION_VERIFY;

	WINTRUST_CATALOG_INFO catalog_info = {0};
	DRIVER_VER_INFO ver_info = {0};
	CATALOG_INFO ci = {0};
	HCATADMIN hcat_admin;
	HCATINFO hcat_info;
	LONG64 file_size;
	PR_STRING string;
	PR_STRING file_hash_tag;
	PVOID file_hash;
	ULONG file_hash_length;
	LONG status;

	file_size = _r_fs_getsize (hfile);

	if (!file_size || file_size > _r_calc_megabytes2bytes64 (32))
	{
		*signature_string = NULL;

		return TRUST_E_NOSIGNATURE;
	}

	string = NULL;
	status = TRUST_E_FAIL;

	if (_app_calculatefilehash (
		hfile,
		algorithm_id,
		&file_hash,
		&file_hash_length,
		&hcat_admin))
	{
		hcat_info = CryptCATAdminEnumCatalogFromHash (
			hcat_admin,
			file_hash,
			file_hash_length,
			0,
			NULL
		);

		if (hcat_info)
		{
			file_hash_tag = _r_str_fromhex (file_hash, file_hash_length, TRUE);

			if (CryptCATCatalogInfoFromContext (hcat_info, &ci, 0))
			{
				// Disable OS version checking by passing in a DRIVER_VER_INFO structure.
				ver_info.cbStruct = sizeof (DRIVER_VER_INFO);

				catalog_info.cbStruct = sizeof (catalog_info);
				catalog_info.pcwszCatalogFilePath = ci.wszCatalogFile;
				catalog_info.pcwszMemberFilePath = file_path;
				catalog_info.hMemberFile = hfile;
				catalog_info.pcwszMemberTag = file_hash_tag->buffer;
				catalog_info.pbCalculatedFileHash = file_hash;
				catalog_info.cbCalculatedFileHash = file_hash_length;
				catalog_info.hCatAdmin = hcat_admin;

				status = _app_verifyfromfile (
					WTD_CHOICE_CATALOG,
					&catalog_info,
					&DriverActionVerify,
					&ver_info,
					&string
				);

				if (ver_info.pcSignerCertContext)
					CertFreeCertificateContext (ver_info.pcSignerCertContext);
			}

			CryptCATAdminReleaseCatalogContext (hcat_admin, hcat_info, 0);

			_r_obj_dereference (file_hash_tag);
		}

		CryptCATAdminReleaseContext (hcat_admin, 0);

		_r_mem_free (file_hash);
	}

	*signature_string = string;

	return status;
}

VOID _app_getfilesignatureinfo (
	_Inout_ PITEM_APP_INFO ptr_app_info
)
{
	static GUID WinTrustActionGenericVerifyV2 = WINTRUST_ACTION_GENERIC_VERIFY_V2;

	WINTRUST_FILE_INFO file_info = {0};

	HANDLE hfile;
	PR_STRING string;
	LONG status;

	if (!_app_isappvalidbinary (ptr_app_info->type, ptr_app_info->path))
	{
		_r_obj_movereference (&ptr_app_info->signature_info, _r_obj_referenceemptystring ());
		return;
	}

	hfile = CreateFile (
		ptr_app_info->path->buffer,
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if (!_r_fs_isvalidhandle (hfile))
		return;

	file_info.cbStruct = sizeof (file_info);
	file_info.pcwszFilePath = ptr_app_info->path->buffer;
	file_info.hFile = hfile;

	status = _app_verifyfromfile (
		WTD_CHOICE_FILE,
		&file_info,
		&WinTrustActionGenericVerifyV2,
		NULL,
		&string
	);

	if (status == TRUST_E_NOSIGNATURE)
	{
		if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
		{
			status = _app_verifyfilefromcatalog (
				hfile,
				ptr_app_info->path->buffer,
				BCRYPT_SHA256_ALGORITHM,
				&string
			);
		}
		else
		{
			status = _app_verifyfilefromcatalog (
				hfile,
				ptr_app_info->path->buffer,
				NULL,
				&string
			);
		}
	}

	if (!string)
		string = _r_obj_referenceemptystring ();

	_r_obj_movereference (&ptr_app_info->signature_info, string);

	NtClose (hfile);
}

VOID _app_getfileversioninfo (
	_Inout_ PITEM_APP_INFO ptr_app_info
)
{
	R_STRINGBUILDER sb;
	PR_STRING version_string = NULL;
	HINSTANCE hlib = NULL;
	VS_FIXEDFILEINFO *ver_info;
	R_BYTEREF ver_block;
	PR_STRING string;
	ULONG lcid;

	if (!_app_isappvalidbinary (ptr_app_info->type, ptr_app_info->path))
		goto CleanupExit;

	hlib = LoadLibraryEx (
		ptr_app_info->path->buffer,
		NULL,
		LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE
	);

	if (!hlib)
		goto CleanupExit;

	if (!_r_res_loadresource (hlib, MAKEINTRESOURCE (VS_VERSION_INFO), RT_VERSION, &ver_block))
		goto CleanupExit;

	_r_obj_initializestringbuilder (&sb);

	lcid = _r_res_querytranslation (ver_block.buffer);

	// get file description
	string = _r_res_querystring (ver_block.buffer, L"FileDescription", lcid);

	if (string)
	{
		_r_obj_appendstringbuilder (&sb, SZ_TAB);
		_r_obj_appendstringbuilder2 (&sb, string);

		_r_obj_dereference (string);
	}

	// get file version
	if (_r_res_queryversion (ver_block.buffer, &ver_info))
	{
		if (_r_obj_isstringempty2 (sb.string))
		{
			_r_obj_appendstringbuilder (&sb, SZ_TAB);
		}
		else
		{
			_r_obj_appendstringbuilder (&sb, L" ");
		}

		_r_obj_appendstringbuilderformat (
			&sb,
			L"%d.%d",
			HIWORD (ver_info->dwFileVersionMS),
			LOWORD (ver_info->dwFileVersionMS)
		);

		if (HIWORD (ver_info->dwFileVersionLS) || LOWORD (ver_info->dwFileVersionLS))
		{
			_r_obj_appendstringbuilderformat (&sb, L".%d", HIWORD (ver_info->dwFileVersionLS));

			if (LOWORD (ver_info->dwFileVersionLS))
				_r_obj_appendstringbuilderformat (&sb, L".%d", LOWORD (ver_info->dwFileVersionLS));
		}
	}

	if (!_r_obj_isstringempty2 (sb.string))
		_r_obj_appendstringbuilder (&sb, L"\r\n");

	// get file company
	string = _r_res_querystring (ver_block.buffer, L"CompanyName", lcid);

	if (string)
	{
		_r_obj_appendstringbuilder (&sb, SZ_TAB);
		_r_obj_appendstringbuilder2 (&sb, string);
		_r_obj_appendstringbuilder (&sb, L"\r\n");

		_r_obj_dereference (string);
	}

	version_string = _r_obj_finalstringbuilder (&sb);

	_r_str_trimstring2 (version_string, DIVIDER_TRIM, 0);

	if (_r_obj_isstringempty2 (version_string))
		_r_obj_clearreference (&version_string);

CleanupExit:

	if (!version_string)
		version_string = _r_obj_referenceemptystring ();

	_r_obj_movereference (&ptr_app_info->version_info, version_string);

	if (hlib)
		FreeLibrary (hlib);
}

ULONG_PTR _app_addcolor (
	_In_ UINT locale_id,
	_In_ LPCWSTR config_name,
	_In_ BOOLEAN is_enabled,
	_In_ LPCWSTR config_value,
	_In_ COLORREF default_clr
)
{
	ITEM_COLOR ptr_clr = {0};
	ULONG hash_code;

	ptr_clr.config_name = _r_obj_createstring (config_name);
	ptr_clr.config_value = _r_obj_createstring (config_value);
	ptr_clr.new_clr = _r_config_getulong_ex (config_value, default_clr, L"colors");

	ptr_clr.default_clr = default_clr;
	ptr_clr.locale_id = locale_id;
	ptr_clr.is_enabled = is_enabled;

	hash_code = _r_str_gethash2 (ptr_clr.config_value, TRUE);

	_r_obj_addhashtableitem (colors_table, hash_code, &ptr_clr);

	return hash_code;
}

COLORREF _app_getcolorvalue (
	_In_ ULONG_PTR color_hash
)
{
	PITEM_COLOR ptr_clr;

	ptr_clr = _r_obj_findhashtable (colors_table, color_hash);

	if (ptr_clr)
		return ptr_clr->new_clr ? ptr_clr->new_clr : ptr_clr->default_clr;

	return 0;
}

VOID _app_generate_rulescontrol (
	_In_ HMENU hsubmenu,
	_In_ ULONG_PTR app_hash,
	_In_opt_ PITEM_LOG ptr_log
)
{
	ITEM_STATUS status;
	MENUITEMINFO mii;
	WCHAR buffer[128];
	PITEM_RULE ptr_rule;
	SIZE_T limit_group;
	SIZE_T i;
	BOOLEAN is_global;
	BOOLEAN is_enabled;

	_app_getcount (&status);

	if (!status.rules_count)
	{
		AppendMenu (hsubmenu, MF_STRING, IDX_RULES_SPECIAL, SZ_EMPTY);

		_r_menu_enableitem (hsubmenu, IDX_RULES_SPECIAL, MF_BYCOMMAND, FALSE);
	}
	else
	{
		for (UINT8 type = 0; type < 2; type++)
		{
			if (type == 0)
			{
				if (!status.rules_predefined_count)
					continue;
			}
			else
			{
				if (!status.rules_user_count)
					continue;
			}

			for (UINT8 loop = 0; loop < 2; loop++)
			{
				limit_group = 14; // limit rules

				_r_queuedlock_acquireshared (&lock_rules);

				for (i = 0; i < _r_obj_getlistsize (rules_list) && limit_group; i++)
				{
					ptr_rule = _r_obj_getlistitem (rules_list, i);

					if (!ptr_rule)
						continue;

					is_global = (ptr_rule->is_enabled && _r_obj_ishashtableempty (ptr_rule->apps));
					is_enabled = is_global ||
						(ptr_rule->is_enabled && (_r_obj_findhashtable (ptr_rule->apps, app_hash)));

					if (ptr_rule->type != DATA_RULE_USER)
						continue;

					if ((type == 0 && (!ptr_rule->is_readonly || is_global)) ||
						(type == 1 && (ptr_rule->is_readonly || is_global)))
					{
						continue;
					}

					if ((loop == 0 && !is_enabled) || (loop == 1 && is_enabled))
						continue;

					_r_str_printf (
						buffer,
						RTL_NUMBER_OF (buffer),
						_r_locale_getstring (IDS_RULE_APPLY_2),
						_r_obj_getstring (ptr_rule->name)
					);

					if (ptr_rule->is_readonly)
						_r_str_append (buffer, RTL_NUMBER_OF (buffer), SZ_RULE_INTERNAL_MENU);

					RtlZeroMemory (&mii, sizeof (mii));

					mii.cbSize = sizeof (mii);
					mii.fMask = MIIM_ID | MIIM_FTYPE | MIIM_STATE | MIIM_STRING;
					mii.fType = MFT_STRING;
					mii.dwTypeData = buffer;
					mii.fState = (is_enabled ? MF_CHECKED : MF_UNCHECKED);
					mii.wID = IDX_RULES_SPECIAL + (UINT)i;

					InsertMenuItem (hsubmenu, mii.wID, FALSE, &mii);

					limit_group -= 1;
				}

				_r_queuedlock_releaseshared (&lock_rules);
			}

			if (!type)
				AppendMenu (hsubmenu, MF_SEPARATOR, 0, NULL);
		}

		if (ptr_log)
		{
			AppendMenu (hsubmenu, MF_SEPARATOR, 0, NULL);

			_r_str_printf (
				buffer,
				RTL_NUMBER_OF (buffer),
				_r_locale_getstring (IDS_RULE_APPLY_2),
				_r_obj_getstring (ptr_log->remote_addr_str)
			);

			RtlZeroMemory (&mii, sizeof (mii));

			mii.cbSize = sizeof (mii);
			mii.fMask = MIIM_ID | MIIM_FTYPE | MIIM_STATE | MIIM_STRING;
			mii.fType = MFT_STRING;
			mii.dwTypeData = buffer;
			mii.fState = MF_UNCHECKED;
			mii.wID = IDX_RULES_SPECIAL + (UINT)i + 1;

			InsertMenuItem (hsubmenu, mii.wID, FALSE, &mii);
		}
	}

	AppendMenu (hsubmenu, MF_SEPARATOR, 0, NULL);
	AppendMenu (hsubmenu, MF_STRING, IDM_OPENRULESEDITOR, _r_locale_getstring (IDS_OPENRULESEDITOR));
}

VOID _app_generate_timerscontrol (
	_In_ HMENU hsubmenu,
	_In_ ULONG_PTR app_hash
)
{
	LONG64 current_time;
	LONG64 app_time;
	LONG64 timestamp;
	PR_STRING interval_string;
	UINT index;
	BOOLEAN is_checked;

	current_time = _r_unixtime_now ();

	is_checked = FALSE;

	_app_getappinfobyhash (app_hash, INFO_TIMER_PTR, (PVOID_PTR)&app_time);

	for (SIZE_T i = 0; i < RTL_NUMBER_OF (timer_array); i++)
	{
		timestamp = timer_array[i];

		interval_string = _r_format_interval (timestamp, 3);

		if (!interval_string)
			continue;

		index = IDX_TIMER + (UINT)i;

		AppendMenu (hsubmenu, MF_STRING, index, interval_string->buffer);

		if (!is_checked && (app_time > current_time) && (app_time <= (current_time + timestamp)))
		{
			_r_menu_checkitem (hsubmenu, IDX_TIMER, index, MF_BYCOMMAND, index);
			is_checked = TRUE;
		}

		_r_obj_dereference (interval_string);
	}

	if (!is_checked)
		_r_menu_checkitem (hsubmenu, IDM_DISABLETIMER, IDM_DISABLETIMER, MF_BYCOMMAND, IDM_DISABLETIMER);
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

	if (ptr_rule->is_forservices && _app_issystemhash (ptr_app->app_hash))
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

		if (_r_obj_ishashtableempty (ptr_rule->apps))
		{
			_app_ruleenable (ptr_rule, FALSE, TRUE);
		}
	}

	if (item_id != -1)
	{
		listview_id = _app_listview_getbytype (ptr_rule->type);

		_app_listview_updateitemby_param (
			hwnd,
			_app_listview_getitemcontext (hwnd, listview_id, item_id),
			FALSE
		);
	}

	_app_listview_updateitemby_param (hwnd, ptr_app->app_hash, TRUE);

	return TRUE;
}

_Success_ (return)
BOOLEAN _app_parsenetworkstring (
	_In_ LPCWSTR network_string,
	_Inout_ PITEM_ADDRESS address
)
{
	NET_ADDRESS_INFO ni;
	NET_ADDRESS_INFO ni_end;
	ULONG mask;
	ULONG types;
	ULONG code;
	USHORT range_port1;
	USHORT range_port2;
	USHORT port;
	BYTE prefix_length;

	types = NET_STRING_IP_ADDRESS_NO_SCOPE | NET_STRING_IP_SERVICE | NET_STRING_IP_NETWORK |
		NET_STRING_IP_ADDRESS_NO_SCOPE | NET_STRING_IP_ADDRESS;

	if (address->is_range)
	{
		range_port1 = 0;
		range_port2 = 0;

		code = ParseNetworkString (address->range_start, types, &ni, &range_port1, NULL);

		if (code != ERROR_SUCCESS)
			goto CleanupExit;

		code = ParseNetworkString (address->range_end, types, &ni_end, &range_port2, NULL);

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
		code = ParseNetworkString (network_string, types, &ni, &port, &prefix_length);
	}

	if (code != ERROR_SUCCESS)
		goto CleanupExit;

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
			address->range.valueLow.byteArray16 = (FWP_BYTE_ARRAY16 *)address->addr6_low;

			address->range.valueHigh.type = FWP_BYTE_ARRAY16_TYPE;
			RtlCopyMemory (address->addr6_high, ni_end.Ipv6Address.sin6_addr.u.Byte, FWP_V6_ADDR_SIZE);
			address->range.valueHigh.byteArray16 = (FWP_BYTE_ARRAY16 *)address->addr6_high;
		}
		else
		{
			RtlCopyMemory (address->addr6.addr, ni.Ipv6Address.sin6_addr.u.Byte, FWP_V6_ADDR_SIZE);
			address->addr6.prefixLength = min (prefix_length, 128);
		}

		return TRUE;
	}

CleanupExit:

	_r_log (LOG_LEVEL_INFO, NULL, L"ParseNetworkString", code, network_string);

	return FALSE;
}

_Success_ (return)
BOOLEAN _app_preparserulestring (
	_In_ PR_STRINGREF rule,
	_Out_ PITEM_ADDRESS address
)
{
	static WCHAR valid_chars[] = {
		L'.',
		L':',
		L'[',
		L']',
		L'/',
		L'-',
		L'_',
	};

	R_STRINGREF range_start_part;
	R_STRINGREF range_end_part;
	WCHAR rule_string[256];
	SIZE_T length;
	ULONG types;
	BOOLEAN is_valid;

	length = _r_str_getlength3 (rule);

	for (SIZE_T i = 0; i < length; i++)
	{
		if (IsCharAlphaNumeric (rule->buffer[i]))
			continue;

		is_valid = FALSE;

		for (SIZE_T j = 0; j < RTL_NUMBER_OF (valid_chars); j++)
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
	address->is_range = _r_str_splitatchar (
		rule,
		DIVIDER_RULE_RANGE,
		&range_start_part,
		&range_end_part
	);

	// extract start and end position of rule
	if (address->is_range)
	{
		// there is incorrect range syntax
		if (_r_obj_isstringempty2 (&range_start_part) || _r_obj_isstringempty2 (&range_end_part))
			return FALSE;

		_r_str_copystring (
			address->range_start,
			RTL_NUMBER_OF (address->range_start),
			&range_start_part
		);

		_r_str_copystring (
			address->range_end,
			RTL_NUMBER_OF (address->range_end),
			&range_end_part
		);
	}

	// check rule for port
	if (address->type == DATA_UNKNOWN)
	{
		address->type = DATA_TYPE_PORT;

		for (SIZE_T i = 0; i < length; i++)
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

	_r_str_copystring (rule_string, RTL_NUMBER_OF (rule_string), rule);

	types = NET_STRING_IP_ADDRESS | NET_STRING_IP_SERVICE | NET_STRING_IP_NETWORK |
		NET_STRING_IP_ADDRESS_NO_SCOPE | NET_STRING_IP_SERVICE_NO_SCOPE;

	// check rule for ip address
	if (address->is_range)
	{
		if (ParseNetworkString (address->range_start, types, NULL, NULL, NULL) == ERROR_SUCCESS &&
			ParseNetworkString (address->range_end, types, NULL, NULL, NULL) == ERROR_SUCCESS)
		{
			address->type = DATA_TYPE_IP;
			return TRUE;
		}
	}
	else
	{
		if (ParseNetworkString (rule_string, types, NULL, NULL, NULL) == ERROR_SUCCESS)
		{
			address->type = DATA_TYPE_IP;
			return TRUE;
		}
	}

	return FALSE;
}

_Success_ (return)
BOOLEAN _app_parserulestring (
	_In_opt_ PR_STRINGREF rule,
	_Out_ PITEM_ADDRESS address
)
{
	WCHAR rule_string[256];
	R_STRINGREF sr;

	RtlZeroMemory (address, sizeof (ITEM_ADDRESS));

	if (_r_obj_isstringempty (rule))
		return TRUE;

	// parse rule type
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

			if (address->range.valueLow.uint16 < 1)
				return FALSE;

			if (address->range.valueHigh.uint16 > 65535)
				return FALSE;

			if (address->range.valueLow.uint16 >= address->range.valueHigh.uint16)
				return FALSE;

			return TRUE;
		}
		else
		{
			// ...port
			address->port = (UINT16)_r_str_toulong (rule);

			if (address->port < 1)
				return FALSE;

			if (address->port > 65535)
				return FALSE;

			return TRUE;
		}
	}
	else if (address->type == DATA_TYPE_IP)
	{
		_r_str_copystring (rule_string, RTL_NUMBER_OF (rule_string), rule);

		if (!_app_parsenetworkstring (rule_string, address))
			return FALSE;
	}

	return TRUE;
}

_Ret_maybenull_
PR_STRING _app_resolveaddress (
	_In_ ADDRESS_FAMILY af,
	_In_ LPCVOID address
)
{
	PDNS_RECORD dns_records;
	PR_STRING arpa_string;
	PR_STRING string;
	ULONG_PTR arpa_hash;
	DNS_STATUS status;

	arpa_string = _app_formatarpa (af, address);
	arpa_hash = _r_str_gethash2 (arpa_string, TRUE);

	if (_app_getcachetable (
		cache_resolution,
		arpa_hash,
		&lock_cache_resolution,
		&string))
	{
		_r_obj_dereference (arpa_string);
		return string;
	}

	dns_records = NULL;
	string = NULL;

	status = DnsQuery (
		arpa_string->buffer,
		DNS_TYPE_PTR,
		DNS_QUERY_NO_HOSTS_FILE,
		NULL,
		&dns_records,
		NULL
	);

	if (status == NO_ERROR)
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

			DnsRecordListFree (dns_records, DnsFreeRecordList);
		}
	}

	if (!string)
		string = _r_obj_referenceemptystring ();

	_app_addcachetable (
		cache_resolution,
		arpa_hash,
		&lock_cache_resolution,
		_r_obj_reference (string)
	);

	_r_obj_dereference (arpa_string);

	return string;
}

PR_STRING _app_resolveaddress_interlocked (
	_In_ PVOID volatile *string,
	_In_ ADDRESS_FAMILY af,
	_In_ LPCVOID address,
	_In_ BOOLEAN is_resolutionenabled
)
{
	PR_STRING current_string;
	PR_STRING new_string;

	current_string = InterlockedCompareExchangePointer (
		string,
		NULL,
		NULL
	);

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

	current_string = InterlockedCompareExchangePointer (
		string,
		new_string,
		NULL
	);

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

VOID _app_queue_fileinformation (
	_In_ PR_STRING path,
	_In_ ULONG_PTR app_hash,
	_In_ ENUM_TYPE_DATA type,
	_In_ INT listview_id
)
{
	PITEM_APP_INFO ptr_app_info;

	ptr_app_info = _app_getappinfobyhash2 (app_hash);

	if (ptr_app_info)
	{
		if (InterlockedCompareExchange (&ptr_app_info->lock, 0, 0) != 0)
		{
			_r_obj_dereference (ptr_app_info);
			return;
		}

		// all information is already set
		if (ptr_app_info->signature_info &&
			ptr_app_info->version_info &&
			InterlockedCompareExchange (&ptr_app_info->large_icon_id, 0, 0) != 0)
		{
			_r_obj_dereference (ptr_app_info);
			return;
		}
	}
	else
	{
		ptr_app_info = _r_obj_allocate (sizeof (ITEM_APP_INFO), &_app_dereferenceappinfo);

		ptr_app_info->path = _r_obj_reference (path);
		ptr_app_info->app_hash = app_hash;
		ptr_app_info->type = type;
		ptr_app_info->listview_id = listview_id;

		_r_queuedlock_acquireexclusive (&lock_cache_information);
		_r_obj_addhashtablepointer (cache_information, app_hash, _r_obj_reference (ptr_app_info));
		_r_queuedlock_releaseexclusive (&lock_cache_information);
	}

	InterlockedIncrement (&ptr_app_info->lock);

	_r_workqueue_queueitem (
		&file_queue,
		&_app_queuefileinformation,
		ptr_app_info
	);
}

VOID _app_queue_resolver (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ ULONG_PTR hash_code,
	_In_ PVOID base_address
)
{
	PITEM_CONTEXT context;

	context = _r_freelist_allocateitem (&context_free_list);

	context->hwnd = hwnd;
	context->listview_id = listview_id;
	context->lparam = hash_code;
	context->base_address = _r_obj_reference (base_address);

	_r_workqueue_queueitem (
		&resolver_queue,
		&_app_queueresolveinformation,
		context
	);
}

VOID NTAPI _app_queuefileinformation (
	_In_ PVOID arglist,
	_In_ ULONG busy_count
)
{
	PITEM_APP_INFO ptr_app_info;
	HWND hwnd;

	ptr_app_info = arglist;
	hwnd = _r_app_gethwnd ();

	// query app icon
	if (InterlockedCompareExchange (&ptr_app_info->large_icon_id, 0, 0) == 0)
		_app_getfileicon (ptr_app_info);

	// query certificate information
	if (!ptr_app_info->signature_info)
	{
		if (_r_config_getboolean (L"IsCertificatesEnabled", TRUE))
			_app_getfilesignatureinfo (ptr_app_info);
	}

	// query version info
	if (!ptr_app_info->version_info)
	{
		_app_getfileversioninfo (ptr_app_info);
	}

	// redraw listview
	if (!(busy_count % 4)) // lol, hack!!!
	{
		if (_r_wnd_isvisible (hwnd))
		{
			if (ptr_app_info->listview_id == _app_listview_getcurrent (hwnd))
				_r_listview_redraw (hwnd, ptr_app_info->listview_id, -1);
		}
	}

	InterlockedDecrement (&ptr_app_info->lock);

	_r_obj_dereference (ptr_app_info);
}

VOID NTAPI _app_queuenotifyinformation (
	_In_ PVOID arglist,
	_In_ ULONG busy_count
)
{
	PITEM_CONTEXT context;
	PITEM_APP_INFO ptr_app_info;
	PR_STRING address_str;
	PR_STRING host_str;
	PR_STRING signature_str;
	PR_STRING localized_string;
	HICON hicon;
	HDWP hdefer;
	BOOLEAN is_iconset;

	context = arglist;

	signature_str = NULL;
	host_str = NULL;

	is_iconset = FALSE;

	// query address string
	address_str = _app_formataddress (
		context->ptr_log->af,
		context->ptr_log->protocol,
		&context->ptr_log->remote_addr,
		0,
		FMTADDR_USE_PROTOCOL
	);

	// query notification host name
	if (_r_config_getboolean (L"IsNetworkResolutionsEnabled", FALSE))
	{
		host_str = _app_resolveaddress_interlocked (
			&context->ptr_log->remote_host_str,
			context->ptr_log->af,
			&context->ptr_log->remote_addr,
			TRUE
		);

		if (host_str)
			host_str = _r_obj_reference (host_str);
	}

	// query signature
	if (_r_config_getboolean (L"IsCertificatesEnabled", TRUE))
	{
		ptr_app_info = _app_getappinfobyhash2 (context->ptr_log->app_hash);

		if (ptr_app_info)
		{
			if (InterlockedCompareExchange (&ptr_app_info->lock, 0, 0) == 0)
			{
				if (!ptr_app_info->signature_info)
					_app_getfilesignatureinfo (ptr_app_info);
			}

			_r_obj_dereference (ptr_app_info);
		}
	}

	// query file icon
	hicon = _app_icons_getsafeapp_hicon (context->ptr_log->app_hash);

	if (_r_wnd_isvisible (context->hwnd))
	{
		if (context->ptr_log->app_hash == _app_notify_getapp_id (context->hwnd))
		{
			// set file icon
			_app_notify_setapp_icon (context->hwnd, hicon, TRUE);
			is_iconset = TRUE;

			// set signature information
			localized_string = _r_obj_concatstrings (
				2,
				_r_locale_getstring (IDS_SIGNATURE),
				L":"
			);

			_app_getappinfoparam2 (
				context->ptr_log->app_hash,
				INFO_SIGNATURE_STRING,
				&signature_str
			);

			if (_r_obj_isstringempty (signature_str))
				_r_obj_movereference (&signature_str, _r_locale_getstring_ex (IDS_SIGN_UNSIGNED));

			hdefer = BeginDeferWindowPos (2);

			_r_ctrl_settablestring (
				context->hwnd,
				&hdefer,
				IDC_SIGNATURE_ID,
				&localized_string->sr,
				IDC_SIGNATURE_TEXT,
				&signature_str->sr
			);

			// set address string
			_r_obj_movereference (
				&localized_string,
				_r_obj_concatstrings (2, _r_locale_getstring (IDS_ADDRESS), L":")
			);

			if (_r_obj_isstringempty (address_str))
				_r_obj_movereference (&address_str, _r_obj_createstring (SZ_EMPTY));

			_r_ctrl_settablestring (
				context->hwnd,
				&hdefer,
				IDC_ADDRESS_ID,
				&localized_string->sr,
				IDC_ADDRESS_TEXT,
				&address_str->sr
			);

			// set host string
			_r_obj_movereference (
				&localized_string,
				_r_obj_concatstrings (2, _r_locale_getstring (IDS_HOST), L":")
			);

			if (_r_obj_isstringempty (host_str))
				_r_obj_movereference (&host_str, _r_obj_createstring (SZ_EMPTY));

			_r_ctrl_settablestring (
				context->hwnd,
				&hdefer,
				IDC_HOST_ID,
				&localized_string->sr,
				IDC_HOST_TEXT,
				&host_str->sr
			);

			_r_obj_dereference (localized_string);

			if (hdefer)
				EndDeferWindowPos (hdefer);
		}
	}

	if (!is_iconset && hicon)
		DestroyIcon (hicon);

	if (signature_str)
		_r_obj_dereference (signature_str);

	if (address_str)
		_r_obj_dereference (address_str);

	if (host_str)
		_r_obj_dereference (host_str);

	_r_obj_dereference (context->ptr_log);

	_r_freelist_deleteitem (&context_free_list, context);
}

VOID NTAPI _app_queueresolveinformation (
	_In_ PVOID arglist,
	_In_ ULONG busy_count
)
{
	PITEM_CONTEXT context;
	BOOLEAN is_resolutionenabled;

	context = arglist;

	is_resolutionenabled = _r_config_getboolean (L"IsNetworkResolutionsEnabled", FALSE);

	if (context->listview_id == IDC_LOG)
	{
		_app_resolveaddress_interlocked (
			&context->ptr_log->local_host_str,
			context->ptr_log->af,
			&context->ptr_log->local_addr,
			is_resolutionenabled
		);

		_app_resolveaddress_interlocked (
			&context->ptr_log->remote_host_str,
			context->ptr_log->af,
			&context->ptr_log->remote_addr,
			is_resolutionenabled
		);

		_r_obj_dereference (context->ptr_log);
	}
	else if (context->listview_id == IDC_NETWORK)
	{
		// query address information
		_app_formataddress_interlocked (
			&context->ptr_network->local_addr_str,
			context->ptr_network->af,
			&context->ptr_network->local_addr
		);

		_app_formataddress_interlocked (
			&context->ptr_network->remote_addr_str,
			context->ptr_network->af,
			&context->ptr_network->remote_addr
		);

		_app_resolveaddress_interlocked (
			&context->ptr_network->local_host_str,
			context->ptr_network->af,
			&context->ptr_network->local_addr,
			is_resolutionenabled
		);

		_app_resolveaddress_interlocked (
			&context->ptr_network->remote_host_str,
			context->ptr_network->af,
			&context->ptr_network->remote_addr,
			is_resolutionenabled
		);

		_r_obj_dereference (context->ptr_network);
	}

	// redraw listview
	if (!(busy_count % 4)) // lol, hack!!!
	{
		if (_r_wnd_isvisible (context->hwnd))
		{
			if (_app_listview_getcurrent (context->hwnd) == context->listview_id)
				_r_listview_redraw (context->hwnd, context->listview_id, -1);
		}
	}

	_r_freelist_deleteitem (&context_free_list, context);
}

_Ret_maybenull_
HBITMAP _app_bitmapfrompng (
	_In_opt_ HINSTANCE hinst,
	_In_ LPCWSTR name,
	_In_ LONG width
)
{
	R_BYTEREF buffer;

	if (!_r_res_loadresource (hinst, name, L"PNG", &buffer))
		return NULL;

	return _r_dc_imagetobitmap (
		&GUID_ContainerFormatPng,
		buffer.buffer,
		(ULONG)buffer.length,
		width,
		width
	);
}

VOID _app_wufixhelper (
	_In_ SC_HANDLE hsvcmgr,
	_In_ LPCWSTR service_name,
	_In_ LPCWSTR k_value,
	_In_ BOOLEAN is_enable
)
{
	WCHAR reg_key[128];
	WCHAR reg_value[128];
	SC_HANDLE hsvc;
	PR_STRING image_path;
	HKEY hkey;
	SERVICE_STATUS svc_status;
	LSTATUS status;
	BOOLEAN is_enabled;

	_r_str_printf (
		reg_key,
		RTL_NUMBER_OF (reg_key),
		L"SYSTEM\\CurrentControlSet\\Services\\%s",
		service_name
	);

	status = RegOpenKeyEx (
		HKEY_LOCAL_MACHINE,
		reg_key,
		0,
		KEY_READ | KEY_WRITE,
		&hkey
	);

	if (status != ERROR_SUCCESS)
		return;

	// query service path
	is_enabled = FALSE;

	image_path = _r_reg_querystring (hkey, NULL, L"ImagePath");

	if (image_path)
	{
		if (_r_str_isstartswith2 (
			&image_path->sr,
			is_enable ? L"%systemroot%\\system32\\wusvc.exe" : L"%systemroot%\\system32\\svchost.exe",
			TRUE))
		{
			is_enabled = TRUE;
		}

		_r_obj_dereference (image_path);
	}

	// set new image path
	_r_str_printf (
		reg_value,
		RTL_NUMBER_OF (reg_value),
		L"%%systemroot%%\\system32\\%s -k %s -p",
		is_enable ? L"wusvc.exe" : L"svchost.exe",
		k_value
	);

	status = RegSetValueEx (
		hkey,
		L"ImagePath",
		0,
		REG_EXPAND_SZ,
		(PBYTE)reg_value,
		(ULONG)(_r_str_getlength (reg_value) * sizeof (WCHAR) + sizeof (UNICODE_NULL))
	);

	// restart service
	if (is_enable != is_enabled)
	{
		hsvc = OpenService (
			hsvcmgr,
			service_name,
			SERVICE_START | SERVICE_STOP | SERVICE_QUERY_STATUS
		);

		if (hsvc)
		{
			if (QueryServiceStatus (hsvc, &svc_status))
			{
				if (svc_status.dwCurrentState != SERVICE_STOPPED)
					ControlService (hsvc, SERVICE_CONTROL_STOP, &svc_status);
			}

			CloseServiceHandle (hsvc);
		}
	}

	RegCloseKey (hkey);
}

VOID _app_wufixenable (
	_In_ HWND hwnd,
	_In_ BOOLEAN is_enable
)
{
	SC_HANDLE hsvcmgr;
	PR_STRING service_path;
	WCHAR buffer1[MAX_PATH];
	WCHAR buffer2[MAX_PATH];
	ULONG_PTR app_hash;

	_r_str_printf (
		buffer1,
		RTL_NUMBER_OF (buffer1),
		L"%s\\svchost.exe",
		_r_sys_getsystemdirectory ()->buffer
	);

	_r_str_printf (
		buffer2,
		RTL_NUMBER_OF (buffer2),
		L"%s\\wusvc.exe",
		_r_sys_getsystemdirectory ()->buffer
	);

	if (is_enable)
	{
		if (!_r_fs_exists (buffer2))
			_r_fs_copyfile (buffer1, buffer2, COPY_FILE_COPY_SYMLINK);

		service_path = _r_obj_createstring (buffer2);

		app_hash = _app_addapplication (hwnd, DATA_UNKNOWN, service_path, NULL, NULL);

		if (app_hash)
		{
			_app_setappinfobyhash (app_hash, INFO_IS_ENABLED, IntToPtr (TRUE));

			_app_setappinfobyhash (app_hash, INFO_IS_UNDELETABLE, IntToPtr (TRUE));
		}

		_r_obj_dereference (service_path);
	}
	else
	{
		if (_r_fs_exists (buffer2))
			_r_fs_deletefile (buffer2, TRUE);
	}

	_r_config_setboolean (L"IsWUFixEnabled", is_enable);

	hsvcmgr = OpenSCManager (
		NULL,
		NULL,
		SC_MANAGER_CONNECT | SERVICE_START | SERVICE_STOP | SERVICE_QUERY_STATUS
	);

	if (!hsvcmgr)
		return;

	_app_wufixhelper (hsvcmgr, L"wuauserv", L"netsvcs", is_enable);
	_app_wufixhelper (hsvcmgr, L"DoSvc", L"NetworkService", is_enable);
	_app_wufixhelper (hsvcmgr, L"UsoSvc", L"netsvcs", is_enable);

	CloseServiceHandle (hsvcmgr);
}
