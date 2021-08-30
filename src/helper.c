// simplewall
// Copyright (c) 2016-2021 Henry++

#include "global.h"

VOID NTAPI _app_dereferenceapp (_In_ PVOID entry)
{
	PITEM_APP ptr_item;

	ptr_item = entry;

	SAFE_DELETE_REFERENCE (ptr_item->display_name);
	SAFE_DELETE_REFERENCE (ptr_item->real_path);
	SAFE_DELETE_REFERENCE (ptr_item->short_name);
	SAFE_DELETE_REFERENCE (ptr_item->original_path);
	SAFE_DELETE_REFERENCE (ptr_item->signature);
	SAFE_DELETE_REFERENCE (ptr_item->version_info);

	SAFE_DELETE_REFERENCE (ptr_item->pnotification);

	SAFE_DELETE_REFERENCE (ptr_item->guids);
}

VOID NTAPI _app_dereferenceruleconfig (_In_ PVOID entry)
{
	PITEM_RULE_CONFIG ptr_rule_config;

	ptr_rule_config = entry;

	SAFE_DELETE_REFERENCE (ptr_rule_config->name);
	SAFE_DELETE_REFERENCE (ptr_rule_config->apps);
}

VOID NTAPI _app_dereferencelog (_In_ PVOID entry)
{
	PITEM_LOG ptr_item;

	ptr_item = entry;

	SAFE_DELETE_REFERENCE (ptr_item->path);
	SAFE_DELETE_REFERENCE (ptr_item->provider_name);
	SAFE_DELETE_REFERENCE (ptr_item->filter_name);
	SAFE_DELETE_REFERENCE (ptr_item->username);

	SAFE_DELETE_REFERENCE (ptr_item->local_addr_str);
	SAFE_DELETE_REFERENCE (ptr_item->local_host_str);
	SAFE_DELETE_REFERENCE (ptr_item->remote_addr_str);
	SAFE_DELETE_REFERENCE (ptr_item->remote_host_str);

	SAFE_DELETE_ICON (ptr_item->hicon);
}

VOID NTAPI _app_dereferencenetwork (_In_ PVOID entry)
{
	PITEM_NETWORK ptr_network;

	ptr_network = entry;

	SAFE_DELETE_REFERENCE (ptr_network->path);

	SAFE_DELETE_REFERENCE (ptr_network->local_addr_str);
	SAFE_DELETE_REFERENCE (ptr_network->local_host_str);
	SAFE_DELETE_REFERENCE (ptr_network->remote_addr_str);
	SAFE_DELETE_REFERENCE (ptr_network->remote_host_str);
}

VOID NTAPI _app_dereferencerule (_In_ PVOID entry)
{
	PITEM_RULE ptr_item;

	ptr_item = entry;

	SAFE_DELETE_REFERENCE (ptr_item->apps);

	SAFE_DELETE_REFERENCE (ptr_item->name);
	SAFE_DELETE_REFERENCE (ptr_item->rule_remote);
	SAFE_DELETE_REFERENCE (ptr_item->rule_local);

	SAFE_DELETE_REFERENCE (ptr_item->guids);
}

BOOLEAN _app_ischeckboxlocked (_In_ HWND hwnd)
{
	PVOID context;
	ULONG hash_code;

	hash_code = _r_math_hashinteger_ptr ((ULONG_PTR)hwnd);

	_r_queuedlock_acquireshared (&lock_context);

	context = _r_obj_findhashtable (context_table, hash_code);

	_r_queuedlock_releaseshared (&lock_context);

	return (context != NULL);
}

VOID _app_setcheckboxlock (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ BOOLEAN is_lock)
{
	HWND hctrl;
	ULONG hash_code;

	hctrl = GetDlgItem (hwnd, ctrl_id);

	if (!hctrl)
		return;

	hash_code = _r_math_hashinteger_ptr ((ULONG_PTR)hctrl);

	_r_queuedlock_acquireexclusive (&lock_context);

	if (is_lock)
	{
		_r_obj_addhashtableitem (context_table, hash_code, NULL);
	}
	else
	{
		_r_obj_removehashtableitem (context_table, hash_code);
	}

	_r_queuedlock_releaseexclusive (&lock_context);
}

VOID _app_addcachetable (_Inout_ PR_HASHTABLE hashtable, _In_ ULONG_PTR hash_code, _In_ PR_QUEUED_LOCK spin_lock, _In_ PR_STRING string)
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

_Ret_maybenull_
PVOID _app_getcachetable (_Inout_ PR_HASHTABLE cache_table, _In_ ULONG_PTR hash_code, _In_ PR_QUEUED_LOCK spin_lock)
{
	PR_STRING string;

	_r_queuedlock_acquireshared (spin_lock);

	string = _r_obj_findhashtablepointer (cache_table, hash_code);

	_r_queuedlock_releaseshared (spin_lock);

	return string;
}

_Ret_maybenull_
PR_STRING _app_formataddress (_In_ ADDRESS_FAMILY af, _In_ UINT8 proto, _In_ LPCVOID address, _In_opt_ UINT16 port, _In_ ULONG flags)
{
	R_STRINGBUILDER formatted_address;
	BOOLEAN is_success = FALSE;

	_r_obj_initializestringbuilder (&formatted_address);

	if ((flags & FMTADDR_AS_ARPA) != 0)
	{
		if (af == AF_INET)
		{
			PIN_ADDR p4addr = (PIN_ADDR)address;

			_r_obj_appendstringbuilderformat (&formatted_address, L"%hhu.%hhu.%hhu.%hhu.%s", p4addr->s_impno, p4addr->s_lh, p4addr->s_host, p4addr->s_net, DNS_IP4_REVERSE_DOMAIN_STRING_W);
		}
		else if (af == AF_INET6)
		{
			PIN6_ADDR p6addr = (PIN6_ADDR)address;

			for (INT i = sizeof (IN6_ADDR) - 1; i >= 0; i--)
				_r_obj_appendstringbuilderformat (&formatted_address, L"%hhx.%hhx.", p6addr->s6_addr[i] & 0xF, (p6addr->s6_addr[i] >> 4) & 0xF);

			_r_obj_appendstringbuilder (&formatted_address, DNS_IP6_REVERSE_DOMAIN_STRING_W);
		}

		is_success = TRUE;
	}
	else
	{
		WCHAR addr_str[DNS_MAX_NAME_BUFFER_LENGTH] = {0};

		if ((flags & FMTADDR_USE_PROTOCOL))
		{
			_r_obj_appendstringbuilderformat (&formatted_address, L"%s://", _app_getprotoname (proto, AF_UNSPEC, SZ_UNKNOWN));
		}

		if (_app_formatip (af, address, addr_str, RTL_NUMBER_OF (addr_str), (flags & FMTADDR_AS_RULE) != 0))
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
			_r_obj_appendstringbuilderformat (&formatted_address, is_success ? L":%" TEXT (PRIu16) : L"%" TEXT (PRIu16), port);
			is_success = TRUE;
		}
	}

	if (is_success)
		return _r_obj_finalstringbuilder (&formatted_address);

	_r_obj_deletestringbuilder (&formatted_address);

	return NULL;
}

_Success_ (return)
BOOLEAN _app_formatip (_In_ ADDRESS_FAMILY af, _In_ LPCVOID address, _Out_writes_to_ (buffer_size, buffer_size) LPWSTR out_buffer, _In_ ULONG buffer_size, _In_ BOOLEAN is_checkempty)
{
	if (af == AF_INET)
	{
		PIN_ADDR p4addr = (PIN_ADDR)address;

		if (is_checkempty)
		{
			if (IN4_IS_ADDR_UNSPECIFIED (p4addr))
				return FALSE;
		}

		if (NT_SUCCESS (RtlIpv4AddressToStringEx (p4addr, 0, out_buffer, &buffer_size)))
			return TRUE;
	}
	else if (af == AF_INET6)
	{
		PIN6_ADDR p6addr = (PIN6_ADDR)address;

		if (is_checkempty)
		{
			if (IN6_IS_ADDR_UNSPECIFIED (p6addr))
				return FALSE;
		}

		if (NT_SUCCESS (RtlIpv6AddressToStringEx (p6addr, 0, 0, out_buffer, &buffer_size)))
			return TRUE;
	}

	return FALSE;
}

PR_STRING _app_formatport (_In_ UINT16 port, _In_ UINT8 proto)
{
	LPCWSTR service_string;

	service_string = _app_getservicename (port, proto, NULL);

	if (service_string)
	{
		return _r_format_string (L"%" TEXT (PRIu16) L" (%s)", port, service_string);
	}

	return _r_format_string (L"%" TEXT (PRIu16), port);
}

BOOLEAN _app_isappvalidbinary (_In_ PITEM_APP ptr_app)
{
	static R_STRINGREF valid_exts[] = {
		PR_STRINGREF_INIT (L".exe"),
		PR_STRINGREF_INIT (L".dll"),
	};

	if (ptr_app->type != DataAppRegular && ptr_app->type != DataAppService && ptr_app->type != DataAppUWP)
		return FALSE;

	if (!ptr_app->real_path || !_app_isappvalidpath (&ptr_app->real_path->sr))
		return FALSE;

	for (SIZE_T i = 0; i < RTL_NUMBER_OF (valid_exts); i++)
	{
		if (_r_str_isendsswith (&ptr_app->real_path->sr, &valid_exts[i], TRUE))
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOLEAN _app_isappvalidpath (_In_ PR_STRINGREF path)
{
	if (path->length <= (3 * sizeof (WCHAR)))
		return FALSE;

	if (path->buffer[1] != L':' || path->buffer[2] != L'\\')
		return FALSE;

	return TRUE;
}

BOOLEAN _app_getfileicon (_In_ LPCWSTR path, _In_ BOOLEAN is_small, _Out_opt_ PINT icon_id, _Out_opt_ HICON * hicon)
{
	SHFILEINFO shfi = {0};
	ULONG flags = 0;

	if (icon_id)
		flags |= SHGFI_SYSICONINDEX;

	if (hicon)
		flags |= SHGFI_ICON;

	if (is_small)
		flags |= SHGFI_SMALLICON;

	if (SHGetFileInfo (path, 0, &shfi, sizeof (shfi), flags))
	{
		if (icon_id)
			*icon_id = shfi.iIcon;

		if (hicon)
			*hicon = shfi.hIcon;

		return TRUE;
	}

	if (icon_id)
		*icon_id = 0;

	if (hicon)
		*hicon = NULL;

	return FALSE;
}

BOOLEAN _app_getappicon (_In_opt_ PITEM_APP ptr_app, _In_ BOOLEAN is_small, _Out_opt_ PINT icon_id, _Out_opt_ HICON* hicon)
{
	BOOLEAN is_success = FALSE;

	if (ptr_app && !_r_config_getboolean (L"IsIconsHidden", FALSE) && _app_isappvalidbinary (ptr_app))
	{
		if (ptr_app->icon_id && icon_id)
		{
			*icon_id = ptr_app->icon_id;
			is_success = TRUE;
		}

		if (hicon || !is_success)
			is_success = _app_getfileicon (ptr_app->real_path->buffer, is_small, icon_id, hicon);
	}

	if (!is_success)
	{
		if (ptr_app && ptr_app->type == DataAppUWP)
		{
			if (icon_id)
				*icon_id = config.icon_uwp_id;

			if (hicon)
				*hicon = CopyIcon (is_small ? config.hicon_uwp : config.hicon_large); // small-only!
		}
		else
		{
			if (icon_id)
				*icon_id = config.icon_id;

			if (hicon)
				*hicon = CopyIcon (is_small ? config.hicon_small : config.hicon_large);
		}

		is_success = TRUE;
	}

	return is_success;
}

LPCWSTR _app_getappdisplayname (_In_ PITEM_APP ptr_app, _In_ BOOLEAN is_shortened)
{
	if (ptr_app->app_hash == config.ntoskrnl_hash)
	{
		if (!_r_obj_isstringempty (ptr_app->original_path))
			return ptr_app->original_path->buffer;
	}

	if (ptr_app->type == DataAppService)
	{
		if (!_r_obj_isstringempty (ptr_app->original_path))
			return ptr_app->original_path->buffer;
	}
	else if (ptr_app->type == DataAppUWP)
	{
		if (!_r_obj_isstringempty (ptr_app->display_name))
			return ptr_app->display_name->buffer;

		else if (!_r_obj_isstringempty (ptr_app->real_path))
			return ptr_app->real_path->buffer;

		else if (!_r_obj_isstringempty (ptr_app->original_path))
			return ptr_app->original_path->buffer;
	}

	if (is_shortened || _r_config_getboolean (L"ShowFilenames", TRUE))
	{
		if (!_r_obj_isstringempty (ptr_app->short_name))
			return ptr_app->short_name->buffer;
	}

	return ptr_app->real_path ? ptr_app->real_path->buffer : NULL;
}

_Success_ (return != NULL)
HCATADMIN _app_calculatefilehash (_In_ HANDLE hfile, _Out_ PVOID_PTR file_hash_ptr, _Out_ PULONG file_hash_length_ptr, _Out_ PHANDLE hcat_info_ptr)
{
	typedef BOOL (WINAPI *CCAAC2)(PHANDLE phCatAdmin, const GUID *pgSubsystem, PCWSTR pwszHashAlgorithm, PCCERT_STRONG_SIGN_PARA pStrongHashPolicy, DWORD dwFlags); // CryptCATAdminAcquireContext2 (win8+)
	typedef BOOL (WINAPI *CCAHFFH2)(PHANDLE phCatAdmin, HANDLE hFile, PULONG pcbHash, BYTE *pbHash, DWORD dwFlags); // CryptCATAdminCalcHashFromFileHandle2 (win8+)

	static GUID DriverActionVerify = DRIVER_ACTION_VERIFY;
	static R_INITONCE init_once = PR_INITONCE_INIT;

	static CCAAC2 _CryptCATAdminAcquireContext2 = NULL;
	static CCAHFFH2 _CryptCATAdminCalcHashFromFileHandle2 = NULL;

	HCATADMIN hcat_admin;
	HCATINFO hcat_info;

	PBYTE file_hash;
	ULONG file_hash_length;

	if (_r_initonce_begin (&init_once))
	{
		HMODULE hwintrust = LoadLibraryEx (L"wintrust.dll", NULL, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);

		if (hwintrust)
		{
			_CryptCATAdminAcquireContext2 = (CCAAC2)GetProcAddress (hwintrust, "CryptCATAdminAcquireContext2");
			_CryptCATAdminCalcHashFromFileHandle2 = (CCAHFFH2)GetProcAddress (hwintrust, "CryptCATAdminCalcHashFromFileHandle2");

			FreeLibrary (hwintrust);
		}

		_r_initonce_end (&init_once);
	}

	if (_CryptCATAdminAcquireContext2)
	{
		if (!_CryptCATAdminAcquireContext2 (&hcat_admin, &DriverActionVerify, BCRYPT_SHA256_ALGORITHM, NULL, 0))
			return NULL;
	}
	else
	{
		if (!CryptCATAdminAcquireContext (&hcat_admin, &DriverActionVerify, 0))
			return NULL;
	}

	file_hash_length = 32;
	file_hash = _r_mem_allocatezero (file_hash_length);

	if (_CryptCATAdminCalcHashFromFileHandle2)
	{
		if (!_CryptCATAdminCalcHashFromFileHandle2 (hcat_admin, hfile, &file_hash_length, file_hash, 0))
		{
			file_hash = _r_mem_reallocatezero (file_hash, file_hash_length);

			if (!_CryptCATAdminCalcHashFromFileHandle2 (hcat_admin, hfile, &file_hash_length, file_hash, 0))
			{
				CryptCATAdminReleaseContext (hcat_admin, 0);
				_r_mem_free (file_hash);

				return NULL;
			}
		}
	}
	else
	{
		if (!CryptCATAdminCalcHashFromFileHandle (hfile, &file_hash_length, file_hash, 0))
		{
			file_hash = _r_mem_reallocatezero (file_hash, file_hash_length);

			if (!CryptCATAdminCalcHashFromFileHandle (hfile, &file_hash_length, file_hash, 0))
			{
				CryptCATAdminReleaseContext (hcat_admin, 0);
				_r_mem_free (file_hash);

				return NULL;
			}
		}
	}

	hcat_info = CryptCATAdminEnumCatalogFromHash (hcat_admin, file_hash, file_hash_length, 0, NULL);

	if (!hcat_info)
	{
		if (CryptCATAdminAcquireContext (&hcat_admin, &DriverActionVerify, 0))
		{
			if (CryptCATAdminCalcHashFromFileHandle (hfile, &file_hash_length, file_hash, 0))
			{
				hcat_info = CryptCATAdminEnumCatalogFromHash (hcat_admin, file_hash, file_hash_length, 0, NULL);
			}
		}
	}

	if (!hcat_info)
	{
		CryptCATAdminReleaseContext (hcat_admin, 0);
		_r_mem_free (file_hash);

		return NULL;
	}

	*file_hash_ptr = file_hash;
	*file_hash_length_ptr = file_hash_length;
	*hcat_info_ptr = hcat_info;

	return hcat_admin;
}

VOID _app_getsignatureinfo (_Inout_ PITEM_APP ptr_app)
{
	if (!_app_isappvalidbinary (ptr_app))
		return;

	if (!_r_obj_isstringempty (ptr_app->signature))
		return;

	HANDLE hfile;

	hfile = CreateFile (ptr_app->real_path->buffer, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (!_r_fs_isvalidhandle (hfile))
		return;

	WINTRUST_FILE_INFO file_info = {0};
	WINTRUST_DATA trust_data = {0};
	WINTRUST_CATALOG_INFO catalog_info = {0};

	GUID WinTrustActionGenericVerifyV2 = WINTRUST_ACTION_GENERIC_VERIFY_V2;

	PCRYPT_PROVIDER_DATA prov_data;
	PCRYPT_PROVIDER_SGNR prov_signer;
	PCRYPT_PROVIDER_CERT prov_cert;

	file_info.cbStruct = sizeof (file_info);
	file_info.pcwszFilePath = ptr_app->real_path->buffer;
	file_info.hFile = hfile;

	trust_data.cbStruct = sizeof (trust_data);
	trust_data.dwUIChoice = WTD_UI_NONE;
	trust_data.fdwRevocationChecks = WTD_REVOKE_NONE;
	trust_data.dwProvFlags = WTD_SAFER_FLAG | WTD_CACHE_ONLY_URL_RETRIEVAL;
	trust_data.dwUnionChoice = WTD_CHOICE_FILE;
	trust_data.pFile = &file_info;

	trust_data.dwStateAction = WTD_STATEACTION_VERIFY;

	PVOID file_hash;
	ULONG file_hash_length;
	HCATADMIN hcat_admin;
	HCATINFO hcat_info;
	PR_STRING member_tag = NULL;
	CATALOG_INFO catalog = { 0 };

	hcat_admin = _app_calculatefilehash (hfile, &file_hash, &file_hash_length, &hcat_info);

	if (hcat_admin)
	{
		member_tag = _r_str_fromhex (file_hash, file_hash_length, TRUE);

		if (CryptCATCatalogInfoFromContext (hcat_info, &catalog, 0))
		{
			catalog_info.cbStruct = sizeof (WINTRUST_CATALOG_INFO);
			catalog_info.pcwszCatalogFilePath = catalog.wszCatalogFile;
			catalog_info.pcwszMemberFilePath = ptr_app->real_path->buffer;
			catalog_info.pcwszMemberTag = member_tag->buffer;

			trust_data.dwUnionChoice = WTD_CHOICE_CATALOG;
			trust_data.pCatalog = &catalog_info;
		}

		CryptCATAdminReleaseCatalogContext (hcat_admin, hcat_info, 0);
		CryptCATAdminReleaseContext (hcat_admin, 0);

		_r_mem_free (file_hash);
	}

	if (WinVerifyTrust ((HWND)INVALID_HANDLE_VALUE, &WinTrustActionGenericVerifyV2, &trust_data) == ERROR_SUCCESS)
	{
		prov_data = WTHelperProvDataFromStateData (trust_data.hWVTStateData);

		if (prov_data)
		{
			prov_signer = WTHelperGetProvSignerFromChain (prov_data, 0, FALSE, 0);

			if (prov_signer)
			{
				prov_cert = WTHelperGetProvCertFromChain (prov_signer, 0);

				if (prov_cert)
				{
					PR_STRING string;
					ULONG length;

					length = CertGetNameString (prov_cert->pCert, CERT_NAME_ATTR_TYPE, 0, szOID_COMMON_NAME, NULL, 0) - 1;

					if (length)
					{
						string = _r_obj_createstringex (NULL, length * sizeof (WCHAR));

						if (CertGetNameString (prov_cert->pCert, CERT_NAME_ATTR_TYPE, 0, szOID_COMMON_NAME, string->buffer, length + 1) <= 1)
						{
							_r_obj_dereference (string);
						}
						else
						{
							_r_obj_movereference (&ptr_app->signature, string);
						}
					}
				}
			}
		}
	}

	if (hcat_admin)
	{
		_r_obj_dereference(member_tag);
	}

	trust_data.dwStateAction = WTD_STATEACTION_CLOSE;
	WinVerifyTrust ((HWND)INVALID_HANDLE_VALUE, &WinTrustActionGenericVerifyV2, &trust_data);

	CloseHandle (hfile);
}

VOID _app_getversioninfo (_Inout_ PITEM_APP ptr_app)
{
	if (!_app_isappvalidbinary (ptr_app))
		return;

	if (!_r_obj_isstringempty (ptr_app->version_info))
		return;

	HINSTANCE hlib;
	R_STRINGBUILDER version_builder;
	PR_STRING version_string;
	PVOID version_info;

	hlib = LoadLibraryEx (ptr_app->real_path->buffer, NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE);

	if (!hlib)
		return;

	version_info = _r_res_loadresource (hlib, MAKEINTRESOURCE (VS_VERSION_INFO), RT_VERSION, NULL);

	if (!version_info)
		goto CleanupExit;

	UINT lang_id[4] = {0};
	UINT code_page[4] = {0};

	C_ASSERT (RTL_NUMBER_OF (lang_id) == RTL_NUMBER_OF (code_page));

	// Load language and codepage from the file
	_r_res_querytranslation (version_info, &lang_id[0], &code_page[0]);

	// Use the default language and codepage from the file
	lang_id[1] = GetUserDefaultLangID ();
	code_page[1] = code_page[0];

	// Use the language from the DLL and Latin codepage (most common)
	lang_id[2] = lang_id[0];
	code_page[2] = 1252;

	// Use the default language and Latin codepage (most common)
	lang_id[3] = GetUserDefaultLangID ();
	code_page[3] = 1252;

	PR_STRING description;
	PR_STRING company;
	VS_FIXEDFILEINFO *ver_info = NULL;

	_r_obj_initializestringbuilder (&version_builder);

	for (SIZE_T i = 0; i < RTL_NUMBER_OF (lang_id); i++)
	{
		if (!lang_id[i] || !code_page[i])
			continue;

		// get file description
		description = _r_res_querystring (version_info, L"FileDescription", lang_id[i], code_page[i]);

		if (description)
		{
			_r_obj_appendstringbuilder (&version_builder, SZ_TAB);
			_r_obj_appendstringbuilder2 (&version_builder, description);

			// get file version
			if (_r_res_queryversion (version_info, &ver_info))
			{
				_r_obj_appendstringbuilderformat (&version_builder, L" %d.%d", HIWORD (ver_info->dwFileVersionMS), LOWORD (ver_info->dwFileVersionMS));

				if (HIWORD (ver_info->dwFileVersionLS) || LOWORD (ver_info->dwFileVersionLS))
				{
					_r_obj_appendstringbuilderformat (&version_builder, L".%d", HIWORD (ver_info->dwFileVersionLS));

					if (LOWORD (ver_info->dwFileVersionLS))
					{
						_r_obj_appendstringbuilderformat (&version_builder, L".%d", LOWORD (ver_info->dwFileVersionLS));
					}
				}
			}

			_r_obj_appendstringbuilder (&version_builder, L"\r\n");

			_r_obj_dereference (description);
		}

		// get file company
		company = _r_res_querystring (version_info, L"CompanyName", lang_id[i], code_page[i]);

		if (company)
		{
			_r_obj_appendstringbuilder (&version_builder, SZ_TAB);
			_r_obj_appendstringbuilder2 (&version_builder, company);
			_r_obj_appendstringbuilder (&version_builder, L"\r\n");

			_r_obj_dereference (company);
		}

		if (description || company) // locale was right
			break;
	}

	version_string = _r_obj_finalstringbuilder (&version_builder);

	_r_str_trimstring2 (version_string, DIVIDER_TRIM);

	if (_r_obj_isstringempty (version_string))
	{
		_r_obj_deletestringbuilder (&version_builder);

		goto CleanupExit;
	}

	_r_obj_movereference (&ptr_app->version_info, version_string);

CleanupExit:

	FreeLibrary (hlib);
}

_Ret_maybenull_
LPCWSTR _app_getservicename (_In_ UINT16 port, _In_ UINT8 proto, _In_opt_ LPCWSTR default_value)
{
	switch (port)
	{
		case 1:
			return L"tcpmux";

		case 7:
			return L"echo";

		case 9:
			return L"discard";

		case 11:
			return L"systat";

		case 13:
			return L"daytime";

		case 20:
			return L"ftp-data";

		case 21:
			return L"ftp";

		case 22:
			return L"ssh";

		case 23:
			return L"telnet";

		case 25:
			return L"smtp";

		case 26:
			return L"rsftp";

		case 37:
			return L"time";

		case 39:
			return L"rlp";

		case 42:
			return L"nameserver";

		case 43:
			return L"nicname";

		case 48:
			return L"auditd";

		case 53:
			return L"domain";

		case 63:
			return L"whois++";

		case 67:
		case 68:
			return L"dhcp";

		case 69:
			return L"tftp";

		case 78:
			return L"vettcp";

		case 79:
		case 2003:
			return L"finger";

		case 80:
			return L"http";

		case 81:
			return L"hosts2-ns";

		case 84:
			return L"ctf";

		case 88:
			return L"kerberos-sec";

		case 90:
			return L"dnsix";

		case 92:
			return L"npp";

		case 93:
			return L"dcp";

		case 94:
			return L"objcall";

		case 95:
			return L"supdup";

		case 101:
			return L"hostname";

		case 105:
			return L"cso";

		case 106:
			return L"pop3pw";

		case 107:
			return L"rtelnet";

		case 109:
			return L"pop2";

		case 110:
			return L"pop3";

		case 111:
			return L"rpcbind";

		case 112:
			return L"mcidas";

		case 113:
			return L"auth";

		case 115:
			return L"sftp";

		case 118:
			return L"sqlserv";

		case 119:
			return L"nntp";

		case 123:
			return L"ntp";

		case 126:
			return L"nxedit";

		case 129:
			return L"pwdgen";

		case 135:
			return L"msrpc";

		case 136:
			return L"profile";

		case 137:
			return L"netbios-ns";

		case 138:
			return L"netbios-dgm";

		case 139:
			return L"netbios-ssn";

		case 143:
			return L"imap";

		case 144:
			return L"news";

		case 145:
			return L"uaac";

		case 150:
			return L"sql-net";

		case 152:
			return L"bftp";

		case 156:
			return L"sqlsrv";

		case 159:
			return L"nss-routing";

		case 160:
			return L"sgmp-traps";

		case 161:
			return L"snmp";

		case 162:
			return L"snmptrap";

		case 169:
			return L"send";

		case 174:
			return L"mailq";

		case 175:
			return L"vmnet";

		case 179:
			return L"bgp";

		case 182:
			return L"audit";

		case 185:
			return L"remote-kis";

		case 186:
			return L"kis";

		case 194:
		case 529:
			return L"irc";

		case 195:
			return L"dn6-nlm-aud";

		case 196:
			return L"dn6-smm-red";

		case 197:
			return L"dls";

		case 199:
			return L"smux";

		case 209:
			return L"qmtp";

		case 245:
			return L"link";

		case 280:
			return L"http-mgmt";

		case 322:
			return L"rtsps";

		case 349:
			return L"mftp";

		case 389:
			return L"ldap";

		case 427:
			return L"svrloc";

		case 443:
		{
			if (proto == IPPROTO_UDP)
				return L"quic";

			return L"https";
		}

		case 444:
			return L"snpp";

		case 445:
			return L"microsoft-ds";

		case 464:
			return L"kerberos";

		case 465:
			return L"smtps";

		case 500:
			return L"isakmp";

		case 513:
			return L"login";

		case 514:
			return L"shell";

		case 515:
			return L"printer";

		case 524:
			return L"ncp";

		case 530:
			return L"rpc";

		case 543:
			return L"klogin";

		case 544:
			return L"kshell";

		case 546:
			return L"dhcpv6-client";

		case 547:
			return L"dhcpv6-server";

		case 548:
			return L"afp";

		case 554:
			return L"rtsp";

		case 565:
			return L"whoami";

		case 558:
			return L"sdnskmp";

		case 585:
			return L"imap4-ssl";

		case 587:
			return L"submission";

		case 631:
			return L"ipp";

		case 636:
			return L"ldaps";

		case 646:
			return L"ldp";

		case 647:
			return L"dhcp-failover";

		case 666:
			return L"doom"; // khe-khe-khe!

		case 847:
			return L"dhcp-failover2";

		case 861:
			return L"owamp-control";

		case 862:
			return L"twamp-control";

		case 873:
			return L"rsync";

		case 853:
			return L"domain-s";

		case 989:
			return L"ftps-data";

		case 990:
			return L"ftps";

		case 992:
			return L"telnets";

		case 993:
			return L"imaps";

		case 994:
			return L"ircs";

		case 995:
			return L"pop3s";

		case 1025:
			return L"NFS-or-IIS";

		case 1027:
			return L"IIS";

		case 1029:
			return L"ms-lsa";

		case 1110:
			return L"nfsd";

		case 1111:
			return L"lmsocialserver";

		case 1112:
		case 1114:
		case 4333:
			return L"mini-sql";

		case 1119:
			return L"bnetgame";

		case 1120:
			return L"bnetfile";

		case 1123:
			return L"murray";

		case 1194:
			return L"openvpn";

		case 1337:
			return L"menandmice-dns";

		case 1433:
			return L"ms-sql-s";

		case 1688:
			return L"nsjtp-data";

		case 1701:
			return L"l2tp";

		case 1720:
			return L"h323q931";

		case 1723:
			return L"pptp";

		case 1863:
			return L"msnp";

		case 1900:
		case 5000:
			return L"upnp";

		case 2000:
			return L"cisco-sccp";

		case 2054:
			return L"weblogin";

		case 2086:
			return L"gnunet";

		case 2001:
			return L"dc";

		case 2121:
			return L"ccproxy-ftp";

		case 2164:
			return L"ddns-v3";

		case 2167:
			return L"raw-serial";

		case 2171:
			return L"msfw-storage";

		case 2172:
			return L"msfw-s-storage";

		case 2173:
			return L"msfw-replica";

		case 2174:
			return L"msfw-array";

		case 2371:
			return L"worldwire";

		case 2717:
			return L"pn-requester";

		case 2869:
			return L"icslap";

		case 3000:
			return L"ppp";

		case 3074:
			return L"xbox";

		case 3128:
			return L"squid-http";

		case 3306:
			return L"mysql";

		case 3389:
			return L"ms-wbt-server";

		case 3407:
			return L"ldap-admin";

		case 3540:
			return L"pnrp-port";

		case 3558:
			return L"mcp-port";

		case 3587:
			return L"p2pgroup";

		case 3702:
			return L"ws-discovery";

		case 3713:
			return L"tftps";

		case 3724:
			return L"blizwow";

		case 4500:
			return L"ipsec-nat-t";

		case 4554:
			return L"msfrs";

		case 4687:
			return L"nst";

		case 4876:
			return L"tritium-can";

		case 4899:
			return L"radmin";

		case 5004:
			return L"rtp-data";

		case 5005:
			return L"rtp";

		case 5009:
			return L"airport-admin";

		case 5051:
			return L"ida-agent";

		case 5060:
			return L"sip";

		case 5101:
			return L"admdog";

		case 5190:
			return L"aol";

		case 5350:
			return L"nat-pmp-status";

		case 5351:
			return L"nat-pmp";

		case 5352:
			return L"dns-llq";

		case 5353:
			return L"mdns";

		case 5354:
			return L"mdnsresponder";

		case 5355:
			return L"llmnr";

		case 5357:
			return L"wsdapi";

		case 5358:
			return L"wsdapi-s";

		case 5362:
			return L"serverwsd2";

		case 5432:
			return L"postgresql";

		case 5631:
			return L"pcanywheredata";

		case 5666:
			return L"nrpe";

		case 5687:
			return L"gog-multiplayer";

		case 5800:
			return L"vnc-http";

		case 5900:
			return L"vnc";

		case 5938:
			return L"teamviewer";

		case 6000:
		case 6001:
		case 6002:
		case 6003:
			return L"x11";

		case 6222:
		case 6662: // deprecated!
			return L"radmind";

		case 6346:
			return L"gnutella";

		case 6347:
			return L"gnutella2";

		case 6622:
			return L"mcftp";

		case 6665:
		case 6666:
		case 6667:
		case 6668:
		case 6669:
			return L"ircu";

		case 6881:
			return L"bittorrent-tracker";

		case 7070:
			return L"realserver";

		case 7235:
			return L"aspcoordination";

		case 8443:
			return L"https-alt";

		case 8021:
			return L"ftp-proxy";

		case 8333:
		case 18333:
			return L"bitcoin";

		case 591:
		case 8000:
		case 8008:
		case 8080:
		case 8444:
			return L"http-alt";

		case 8999:
			return L"bctp";

		case 9418:
			return L"git";

		case 9800:
			return L"webdav";

		case 10107:
			return L"bctp-server";

		case 11371:
			return L"hkp";

		case 25565:
			return L"minecraft";

		case 26000:
			return L"quake";

		case 27015:
			return L"halflife";

		case 27017:
		case 27018:
		case 27019:
		case 28017:
			return L"mongod";

		case 27500:
			return L"quakeworld";

		case 27910:
			return L"quake2";

		case 27960:
			return L"quake3";

		case 28240:
			return L"siemensgsm";

		case 33434:
			return L"traceroute";
	}

	return default_value;
}

_Ret_maybenull_
LPCWSTR _app_getprotoname (_In_ ULONG proto, _In_ ADDRESS_FAMILY af, _In_opt_ LPCWSTR default_value)
{
	switch (proto)
	{
		case IPPROTO_HOPOPTS:
			return L"hopopt";

		case IPPROTO_ICMP:
			return L"icmp";

		case IPPROTO_IGMP:
			return L"igmp";

		case IPPROTO_GGP:
			return L"ggp";

		case IPPROTO_IPV4:
			return L"ipv4";

		case IPPROTO_ST:
			return L"st";

		case IPPROTO_TCP:
			return ((af == AF_INET6) ? L"tcp6" : L"tcp");

		case IPPROTO_CBT:
			return L"cbt";

		case IPPROTO_EGP:
			return L"egp";

		case IPPROTO_IGP:
			return L"igp";

		case IPPROTO_PUP:
			return L"pup";

		case IPPROTO_UDP:
			return ((af == AF_INET6) ? L"udp6" : L"udp");

		case IPPROTO_IDP:
			return L"xns-idp";

		case IPPROTO_RDP:
			return L"rdp";

		case IPPROTO_IPV6:
			return L"ipv6";

		case IPPROTO_ROUTING:
			return L"ipv6-route";

		case IPPROTO_FRAGMENT:
			return L"ipv6-frag";

		case IPPROTO_ESP:
			return L"esp";

		case IPPROTO_AH:
			return L"ah";

		case IPPROTO_ICMPV6:
			return L"ipv6-icmp";

		case IPPROTO_DSTOPTS:
			return L"ipv6-opts";

		case IPPROTO_L2TP:
			return L"l2tp";

		case IPPROTO_SCTP:
			return L"sctp";
	}

	return default_value;
}

_Ret_maybenull_
LPCWSTR _app_getconnectionstatusname (_In_ ULONG state)
{
	switch (state)
	{
		case MIB_TCP_STATE_CLOSED:
			return L"Closed";

		case MIB_TCP_STATE_LISTEN:
			return L"Listen";

		case MIB_TCP_STATE_SYN_SENT:
			return L"SYN sent";

		case MIB_TCP_STATE_SYN_RCVD:
			return L"SYN received";

		case MIB_TCP_STATE_ESTAB:
			return L"Established";

		case MIB_TCP_STATE_FIN_WAIT1:
			return L"FIN wait 1";

		case MIB_TCP_STATE_FIN_WAIT2:
			return L"FIN wait 2";

		case MIB_TCP_STATE_CLOSE_WAIT:
			return L"Close wait";

		case MIB_TCP_STATE_CLOSING:
			return L"Closing";

		case MIB_TCP_STATE_LAST_ACK:
			return L"Last ACK";

		case MIB_TCP_STATE_TIME_WAIT:
			return L"Time wait";

		case MIB_TCP_STATE_DELETE_TCB:
			return L"Delete TCB";
	}

	return NULL;
}

_Ret_maybenull_
PR_STRING _app_getdirectionname (_In_ FWP_DIRECTION direction, _In_ BOOLEAN is_loopback, _In_ BOOLEAN is_localized)
{
	LPCWSTR text = NULL;

	if (is_localized)
	{
		if (direction == FWP_DIRECTION_OUTBOUND)
		{
			text = _r_locale_getstring (IDS_DIRECTION_1);
		}
		else if (direction == FWP_DIRECTION_INBOUND)
		{
			text = _r_locale_getstring (IDS_DIRECTION_2);
		}
		else if (direction == FWP_DIRECTION_MAX)
		{
			text = _r_locale_getstring (IDS_ANY);
		}
	}
	else
	{
		if (direction == FWP_DIRECTION_OUTBOUND)
		{
			text = SZ_DIRECTION_OUT;
		}
		else if (direction == FWP_DIRECTION_INBOUND)
		{
			text = SZ_DIRECTION_IN;
		}
		else if (direction == FWP_DIRECTION_MAX)
		{
			text = SZ_DIRECTION_ANY;
		}
	}

	if (!text)
		return NULL;

	if (is_loopback)
		return _r_obj_concatstrings (2, text, L" (" SZ_DIRECTION_LOOPBACK L")");

	return _r_obj_createstring (text);
}

ULONG_PTR _app_addcolor (_In_ UINT locale_id, _In_ LPCWSTR config_name, _In_ BOOLEAN is_enabled, _In_ LPCWSTR config_value, _In_ COLORREF default_clr)
{
	ITEM_COLOR ptr_clr = {0};
	ULONG hash_code;

	ptr_clr.config_name = _r_obj_createstring (config_name);
	ptr_clr.config_value = _r_obj_createstring (config_value);
	ptr_clr.new_clr = _r_config_getulongex (config_value, default_clr, L"colors");

	ptr_clr.default_clr = default_clr;
	ptr_clr.locale_id = locale_id;
	ptr_clr.is_enabled = is_enabled;

	hash_code = _r_obj_getstringhash (ptr_clr.config_value);

	_r_obj_addhashtableitem (colors_table, hash_code, &ptr_clr);

	return hash_code;
}

COLORREF _app_getcolorvalue (_In_ ULONG_PTR color_hash)
{
	PITEM_COLOR ptr_clr;

	ptr_clr = _r_obj_findhashtable (colors_table, color_hash);

	if (ptr_clr)
		return ptr_clr->new_clr ? ptr_clr->new_clr : ptr_clr->default_clr;

	return 0;
}

BOOLEAN _app_getnetworkpath (_In_ ULONG pid, _In_opt_ PULONG64 modules, _Inout_ PITEM_NETWORK ptr_network)
{
	if (pid == PROC_WAITING_PID)
	{
		ptr_network->app_hash = 0;
		ptr_network->icon_id = config.icon_id;
		ptr_network->type = DataAppRegular;
		ptr_network->path = _r_obj_createstring (PROC_WAITING_NAME);

		return TRUE;
	}
	else if (pid == PROC_SYSTEM_PID)
	{
		ptr_network->app_hash = config.ntoskrnl_hash;
		ptr_network->icon_id = config.icon_id;
		ptr_network->type = DataAppRegular;
		ptr_network->path = _r_obj_createstring (PROC_SYSTEM_NAME);

		return TRUE;
	}

	PR_STRING process_name = NULL;

	if (modules)
	{
		process_name = _r_sys_querytaginformation (UlongToHandle (pid), UlongToPtr (*(PULONG)modules));

		if (process_name)
		{
			ptr_network->type = DataAppService;
		}
	}

	if (!process_name)
	{
		NTSTATUS status;
		HANDLE hprocess;

		status = _r_sys_openprocess (UlongToHandle (pid), PROCESS_QUERY_LIMITED_INFORMATION, &hprocess);

		if (NT_SUCCESS (status))
		{
			if (_r_sys_isosversiongreaterorequal (WINDOWS_8) && _r_sys_isprocessimmersive (hprocess))
			{
				ptr_network->type = DataAppUWP;
			}
			else
			{
				ptr_network->type = DataAppRegular;
			}

			status = _r_sys_queryprocessstring (hprocess, ProcessImageFileNameWin32, &process_name);

			// fix for WSL processes (issue #606)
			if (status == STATUS_UNSUCCESSFUL)
			{
				status = _r_sys_queryprocessstring (hprocess, ProcessImageFileName, &process_name);
			}

			NtClose (hprocess);
		}
	}

	if (process_name)
	{
		_app_getfileicon (process_name->buffer, TRUE, &ptr_network->icon_id, NULL);

		ptr_network->app_hash = _r_obj_getstringhash (process_name);
		ptr_network->path = process_name;

		return TRUE;
	}

	return FALSE;
}

ULONG_PTR _app_getnetworkhash (_In_ ADDRESS_FAMILY af, _In_ ULONG pid, _In_opt_ LPCVOID remote_addr, _In_opt_ ULONG remote_port, _In_opt_ LPCVOID local_addr, _In_opt_ ULONG local_port, _In_ UINT8 proto, _In_ ULONG state)
{
	WCHAR remote_address[LEN_IP_MAX] = {0};
	WCHAR local_address[LEN_IP_MAX] = {0};
	PR_STRING network_string;
	ULONG_PTR network_hash;

	if (remote_addr)
		_app_formatip (af, remote_addr, remote_address, RTL_NUMBER_OF (remote_address), FALSE);

	if (local_addr)
		_app_formatip (af, local_addr, local_address, RTL_NUMBER_OF (local_address), FALSE);

	network_string = _r_format_string (L"%" TEXT (PRIu8) L"_%" TEXT (PR_ULONG) L"_%s_%" TEXT (PR_ULONG) L"_%s_%" TEXT (PR_ULONG) L"_%" TEXT (PRIu8) L"_%" TEXT (PR_ULONG),
									   af,
									   pid,
									   remote_address,
									   remote_port,
									   local_address,
									   local_port,
									   proto,
									   state
	);

	if (!network_string)
		return 0;

	network_hash = _r_obj_getstringhash (network_string);

	_r_obj_dereference (network_string);

	return network_hash;
}

BOOLEAN _app_isvalidconnection (_In_ ADDRESS_FAMILY af, _In_ LPCVOID paddr)
{
	if (af == AF_INET)
	{
		PIN_ADDR p4addr = (PIN_ADDR)paddr;

		return (!IN4_IS_ADDR_UNSPECIFIED (p4addr) &&
				!IN4_IS_ADDR_LOOPBACK (p4addr) &&
				!IN4_IS_ADDR_LINKLOCAL (p4addr) &&
				!IN4_IS_ADDR_MULTICAST (p4addr) &&
				!IN4_IS_ADDR_MC_ADMINLOCAL (p4addr) &&
				!IN4_IS_ADDR_RFC1918 (p4addr)
				);
	}
	else if (af == AF_INET6)
	{
		PIN6_ADDR p6addr = (PIN6_ADDR)paddr;

		return  (!IN6_IS_ADDR_UNSPECIFIED (p6addr) &&
				 !IN6_IS_ADDR_LOOPBACK (p6addr) &&
				 !IN6_IS_ADDR_LINKLOCAL (p6addr) &&
				 !IN6_IS_ADDR_MULTICAST (p6addr) &&
				 !IN6_IS_ADDR_SITELOCAL (p6addr) &&
				 !IN6_IS_ADDR_ANYCAST (p6addr)
				 );
	}

	return FALSE;
}

VOID _app_generate_connections (_Inout_ PR_HASHTABLE network_ptr, _Inout_ PR_HASHTABLE checker_map)
{
	PITEM_NETWORK ptr_network;
	ULONG_PTR network_hash;

	PVOID buffer;
	ULONG allocated_size;
	ULONG required_size;

	_r_obj_clearhashtable (checker_map);

	required_size = 0;
	GetExtendedTcpTable (NULL, &required_size, FALSE, AF_INET, TCP_TABLE_OWNER_MODULE_ALL, 0);

	allocated_size = required_size;
	buffer = _r_mem_allocatezero (allocated_size);

	if (required_size)
	{
		PMIB_TCPTABLE_OWNER_MODULE tcp4_table = buffer;

		if (GetExtendedTcpTable (tcp4_table, &required_size, FALSE, AF_INET, TCP_TABLE_OWNER_MODULE_ALL, 0) == NO_ERROR)
		{
			for (ULONG i = 0; i < tcp4_table->dwNumEntries; i++)
			{
				IN_ADDR remote_addr = {0};
				IN_ADDR local_addr = {0};

				remote_addr.S_un.S_addr = tcp4_table->table[i].dwRemoteAddr;
				local_addr.S_un.S_addr = tcp4_table->table[i].dwLocalAddr;

				network_hash = _app_getnetworkhash (AF_INET, tcp4_table->table[i].dwOwningPid, &remote_addr, tcp4_table->table[i].dwRemotePort, &local_addr, tcp4_table->table[i].dwLocalPort, IPPROTO_TCP, tcp4_table->table[i].dwState);

				if (_app_isnetworkfound (network_hash))
				{
					_r_obj_addhashtablepointer (checker_map, network_hash, NULL);
					continue;
				}

				ptr_network = _r_obj_allocate (sizeof (ITEM_NETWORK), &_app_dereferencenetwork);

				if (!_app_getnetworkpath (tcp4_table->table[i].dwOwningPid, tcp4_table->table[i].OwningModuleInfo, ptr_network))
				{
					_r_obj_dereference (ptr_network);
					continue;
				}

				ptr_network->af = AF_INET;
				ptr_network->protocol = IPPROTO_TCP;

				ptr_network->remote_addr.S_un.S_addr = tcp4_table->table[i].dwRemoteAddr;
				ptr_network->remote_port = _r_byteswap_ushort ((USHORT)tcp4_table->table[i].dwRemotePort);

				ptr_network->local_addr.S_un.S_addr = tcp4_table->table[i].dwLocalAddr;
				ptr_network->local_port = _r_byteswap_ushort ((USHORT)tcp4_table->table[i].dwLocalPort);

				ptr_network->state = tcp4_table->table[i].dwState;

				if (tcp4_table->table[i].dwState == MIB_TCP_STATE_ESTAB)
				{
					if (_app_isvalidconnection (ptr_network->af, &ptr_network->remote_addr) || _app_isvalidconnection (ptr_network->af, &ptr_network->local_addr))
						ptr_network->is_connection = TRUE;
				}

				_r_queuedlock_acquireexclusive (&lock_network);

				_r_obj_addhashtablepointer (network_ptr, network_hash, ptr_network);

				_r_queuedlock_releaseexclusive (&lock_network);

				_r_obj_addhashtablepointer (checker_map, network_hash, _r_obj_reference (ptr_network->path));
			}
		}
	}

	required_size = 0;
	GetExtendedTcpTable (NULL, &required_size, FALSE, AF_INET6, TCP_TABLE_OWNER_MODULE_ALL, 0);

	if (required_size)
	{
		if (allocated_size < required_size)
		{
			buffer = _r_mem_reallocatezero (buffer, required_size);
			allocated_size = required_size;
		}

		PMIB_TCP6TABLE_OWNER_MODULE tcp6_table = buffer;

		if (GetExtendedTcpTable (tcp6_table, &required_size, FALSE, AF_INET6, TCP_TABLE_OWNER_MODULE_ALL, 0) == NO_ERROR)
		{
			for (ULONG i = 0; i < tcp6_table->dwNumEntries; i++)
			{
				network_hash = _app_getnetworkhash (AF_INET6, tcp6_table->table[i].dwOwningPid, tcp6_table->table[i].ucRemoteAddr, tcp6_table->table[i].dwRemotePort, tcp6_table->table[i].ucLocalAddr, tcp6_table->table[i].dwLocalPort, IPPROTO_TCP, tcp6_table->table[i].dwState);

				if (_app_isnetworkfound (network_hash))
				{
					_r_obj_addhashtablepointer (checker_map, network_hash, NULL);
					continue;
				}

				ptr_network = _r_obj_allocate (sizeof (ITEM_NETWORK), &_app_dereferencenetwork);

				if (!_app_getnetworkpath (tcp6_table->table[i].dwOwningPid, tcp6_table->table[i].OwningModuleInfo, ptr_network))
				{
					_r_obj_dereference (ptr_network);
					continue;
				}

				ptr_network->af = AF_INET6;
				ptr_network->protocol = IPPROTO_TCP;

				RtlCopyMemory (ptr_network->remote_addr6.u.Byte, tcp6_table->table[i].ucRemoteAddr, FWP_V6_ADDR_SIZE);
				ptr_network->remote_port = _r_byteswap_ushort ((USHORT)tcp6_table->table[i].dwRemotePort);

				RtlCopyMemory (ptr_network->local_addr6.u.Byte, tcp6_table->table[i].ucLocalAddr, FWP_V6_ADDR_SIZE);
				ptr_network->local_port = _r_byteswap_ushort ((USHORT)tcp6_table->table[i].dwLocalPort);

				ptr_network->state = tcp6_table->table[i].dwState;

				if (tcp6_table->table[i].dwState == MIB_TCP_STATE_ESTAB)
				{
					if (_app_isvalidconnection (ptr_network->af, &ptr_network->remote_addr6) || _app_isvalidconnection (ptr_network->af, &ptr_network->local_addr6))
						ptr_network->is_connection = TRUE;
				}

				_r_queuedlock_acquireexclusive (&lock_network);

				_r_obj_addhashtablepointer (network_ptr, network_hash, ptr_network);

				_r_queuedlock_releaseexclusive (&lock_network);

				_r_obj_addhashtablepointer (checker_map, network_hash, _r_obj_reference (ptr_network->path));
			}
		}
	}

	required_size = 0;
	GetExtendedUdpTable (NULL, &required_size, FALSE, AF_INET, UDP_TABLE_OWNER_MODULE, 0);

	if (required_size)
	{
		if (allocated_size < required_size)
		{
			buffer = _r_mem_reallocatezero (buffer, required_size);
			allocated_size = required_size;
		}

		PMIB_UDPTABLE_OWNER_MODULE udp4_table = buffer;

		if (GetExtendedUdpTable (udp4_table, &required_size, FALSE, AF_INET, UDP_TABLE_OWNER_MODULE, 0) == NO_ERROR)
		{
			for (ULONG i = 0; i < udp4_table->dwNumEntries; i++)
			{
				IN_ADDR local_addr = {0};
				local_addr.S_un.S_addr = udp4_table->table[i].dwLocalAddr;

				network_hash = _app_getnetworkhash (AF_INET, udp4_table->table[i].dwOwningPid, NULL, 0, &local_addr, udp4_table->table[i].dwLocalPort, IPPROTO_UDP, 0);

				if (_app_isnetworkfound (network_hash))
				{
					_r_obj_addhashtablepointer (checker_map, network_hash, NULL);
					continue;
				}

				ptr_network = _r_obj_allocate (sizeof (ITEM_NETWORK), &_app_dereferencenetwork);

				if (!_app_getnetworkpath (udp4_table->table[i].dwOwningPid, udp4_table->table[i].OwningModuleInfo, ptr_network))
				{
					_r_obj_dereference (ptr_network);
					continue;
				}

				ptr_network->af = AF_INET;
				ptr_network->protocol = IPPROTO_UDP;

				ptr_network->local_addr.S_un.S_addr = udp4_table->table[i].dwLocalAddr;
				ptr_network->local_port = _r_byteswap_ushort ((USHORT)udp4_table->table[i].dwLocalPort);

				if (_app_isvalidconnection (ptr_network->af, &ptr_network->local_addr))
					ptr_network->is_connection = TRUE;

				_r_queuedlock_acquireexclusive (&lock_network);

				_r_obj_addhashtablepointer (network_ptr, network_hash, ptr_network);

				_r_queuedlock_releaseexclusive (&lock_network);

				_r_obj_addhashtablepointer (checker_map, network_hash, _r_obj_reference (ptr_network->path));
			}
		}
	}

	required_size = 0;
	GetExtendedUdpTable (NULL, &required_size, FALSE, AF_INET6, UDP_TABLE_OWNER_MODULE, 0);

	if (required_size)
	{
		if (allocated_size < required_size)
		{
			buffer = _r_mem_reallocatezero (buffer, required_size);
			allocated_size = required_size;
		}

		PMIB_UDP6TABLE_OWNER_MODULE udp6_table = buffer;

		if (GetExtendedUdpTable (udp6_table, &required_size, FALSE, AF_INET6, UDP_TABLE_OWNER_MODULE, 0) == NO_ERROR)
		{
			for (ULONG i = 0; i < udp6_table->dwNumEntries; i++)
			{
				network_hash = _app_getnetworkhash (AF_INET6, udp6_table->table[i].dwOwningPid, NULL, 0, udp6_table->table[i].ucLocalAddr, udp6_table->table[i].dwLocalPort, IPPROTO_UDP, 0);

				if (_app_isnetworkfound (network_hash))
				{
					_r_obj_addhashtablepointer (checker_map, network_hash, NULL);
					continue;
				}

				ptr_network = _r_obj_allocate (sizeof (ITEM_NETWORK), &_app_dereferencenetwork);

				if (!_app_getnetworkpath (udp6_table->table[i].dwOwningPid, udp6_table->table[i].OwningModuleInfo, ptr_network))
				{
					_r_obj_dereference (ptr_network);
					continue;
				}

				ptr_network->af = AF_INET6;
				ptr_network->protocol = IPPROTO_UDP;

				RtlCopyMemory (ptr_network->local_addr6.u.Byte, udp6_table->table[i].ucLocalAddr, FWP_V6_ADDR_SIZE);
				ptr_network->local_port = _r_byteswap_ushort ((USHORT)udp6_table->table[i].dwLocalPort);

				if (_app_isvalidconnection (ptr_network->af, &ptr_network->local_addr6))
					ptr_network->is_connection = TRUE;

				_r_queuedlock_acquireexclusive (&lock_network);

				_r_obj_addhashtablepointer (network_ptr, network_hash, ptr_network);

				_r_queuedlock_releaseexclusive (&lock_network);

				_r_obj_addhashtablepointer (checker_map, network_hash, _r_obj_reference (ptr_network->path));
			}
		}
	}

	if (buffer)
		_r_mem_free (buffer);
}

VOID _app_generate_packages ()
{
	PR_BYTE package_sid;
	PR_STRING package_sid_string;
	PR_STRING key_name;
	PR_STRING display_name;
	PR_STRING real_path;
	ULONG_PTR app_hash;
	HKEY hkey;
	HKEY hsubkey;
	ULONG key_index;
	ULONG max_length;
	ULONG size;
	LSTATUS code;

	code = RegOpenKeyEx (HKEY_CURRENT_USER, L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages", 0, KEY_READ, &hkey);

	if (code != ERROR_SUCCESS)
		return;

	max_length = _r_reg_querysubkeylength (hkey);

	if (max_length)
	{
		key_name = _r_obj_createstringex (NULL, max_length * sizeof (WCHAR));
		key_index = 0;

		while (TRUE)
		{
			size = max_length + 1;

			if (RegEnumKeyEx (hkey, key_index++, key_name->buffer, &size, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
				break;

			_r_obj_trimstringtonullterminator (key_name);

			code = RegOpenKeyEx (hkey, key_name->buffer, 0, KEY_READ, &hsubkey);

			if (code == ERROR_SUCCESS)
			{
				package_sid = _r_reg_querybinary (hsubkey, NULL, L"PackageSid");

				if (package_sid)
				{
					if (RtlValidSid (package_sid->buffer))
					{
						package_sid_string = _r_str_fromsid (package_sid->buffer);

						if (package_sid_string)
						{
							if (!_app_isappfound (_r_obj_getstringhash (package_sid_string)))
							{
								display_name = _r_reg_querystring (hsubkey, NULL, L"DisplayName");

								if (display_name)
								{
									if (!_r_obj_isstringempty (display_name))
									{
										if (display_name->buffer[0] == L'@')
										{
											PR_STRING localized_name;
											UINT localized_length;

											localized_length = 512;
											localized_name = _r_obj_createstringex (NULL, localized_length * sizeof (WCHAR));

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
										_r_obj_movereference (&display_name, _r_obj_reference (key_name));

									real_path = _r_reg_querystring (hsubkey, NULL, L"PackageRootFolder");

									app_hash = _app_addapplication (NULL, DataAppUWP, &package_sid_string->sr, display_name, real_path);

									if (app_hash)
									{
										PITEM_APP ptr_app;

										ptr_app = _app_getappitem (app_hash);

										if (ptr_app)
										{
											LONG64 timestamp = _r_reg_querytimestamp (hsubkey);

											_app_setappinfo (ptr_app, InfoTimestampPtr, &timestamp);
											_app_setappinfo (ptr_app, InfoBytesData, _r_obj_reference (package_sid));

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

VOID _app_generate_services ()
{
	SC_HANDLE hsvcmgr;

	hsvcmgr = OpenSCManager (NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE);

	if (!hsvcmgr)
		return;

	static ULONG initial_buffer_size = 0x8000;

	ULONG return_length;
	ULONG services_returned;
	ULONG service_type = SERVICE_WIN32_OWN_PROCESS | SERVICE_WIN32_SHARE_PROCESS;
	ULONG service_state = SERVICE_STATE_ALL;

	// win10+
	if (_r_sys_isosversiongreaterorequal (WINDOWS_10))
		service_type |= SERVICE_INTERACTIVE_PROCESS | SERVICE_USER_SERVICE;

	ULONG buffer_size = initial_buffer_size;
	PVOID buffer = _r_mem_allocatezero (buffer_size);

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
		WCHAR general_key[256];
		EXPLICIT_ACCESS ea;
		R_STRINGREF service_name;
		LPENUM_SERVICE_STATUS_PROCESS service;
		LPENUM_SERVICE_STATUS_PROCESS services;
		PSID service_sid;
		PVOID service_sd;
		PR_STRING service_path;
		LONG64 service_timestamp;
		ULONG_PTR app_hash;
		ULONG sd_length;

		HKEY hkey;

		services = (LPENUM_SERVICE_STATUS_PROCESS)buffer;

		for (ULONG i = 0; i < services_returned; i++)
		{
			service = &services[i];

			_r_obj_initializestringref (&service_name, service->lpServiceName);

			app_hash = _r_obj_getstringrefhash (&service_name);

			if (_app_isappfound (app_hash))
				continue;

			_r_str_printf (general_key, RTL_NUMBER_OF (general_key), L"System\\CurrentControlSet\\Services\\%s", service->lpServiceName);

			if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, general_key, 0, KEY_READ, &hkey) != ERROR_SUCCESS)
				continue;

			// skip userservice instances service types (win10+)
			if (_r_sys_isosversiongreaterorequal (WINDOWS_10))
			{
				ULONG service_type = _r_reg_queryulong (hkey, NULL, L"Type");

				if (!service_type || (service_type & SERVICE_USERSERVICE_INSTANCE) != 0)
				{
					RegCloseKey (hkey);
					continue;
				}
			}

			// query service path
			service_path = _r_reg_querystring (hkey, L"Parameters", L"ServiceDLL");

			if (!service_path)
				service_path = _r_reg_querystring (hkey, NULL, L"ImagePath");

			if (service_path)
			{
				PathRemoveArgs (service_path->buffer);
				PathUnquoteSpaces (service_path->buffer);

				_r_obj_trimstringtonullterminator (service_path);

				PR_STRING converted_path = _r_path_dospathfromnt (service_path->buffer);

				if (converted_path)
					_r_obj_movereference (&service_path, converted_path);

				// query service timestamp
				service_timestamp = _r_reg_querytimestamp (hkey);

				// query service sid
				service_sid = _r_sys_getservicesid (&service_name);

				if (service_sid)
				{
					// When evaluating SECURITY_DESCRIPTOR conditions, the filter engine
					// checks for FWP_ACTRL_MATCH_FILTER access. If the DACL grants access,
					// it does not mean that the traffic is allowed; it just means that the
					// condition evaluates to true. Likewise if it denies access, the
					// condition evaluates to false.
					_app_setexplicitaccess (&ea, GRANT_ACCESS, FWP_ACTRL_MATCH_FILTER, NO_INHERITANCE, service_sid);

					// Security descriptors must be in self-relative form (i.e., contiguous).
					// The security descriptor returned by BuildSecurityDescriptorW is
					// already self-relative, but if you're using another mechanism to build
					// the descriptor, you may have to convert it. See MakeSelfRelativeSD for
					// details.
					if (BuildSecurityDescriptor (NULL, NULL, 1, &ea, 0, NULL, NULL, &sd_length, &service_sd) == ERROR_SUCCESS && service_sd)
					{
						PR_STRING name_string = _r_obj_createstring (service->lpDisplayName);

						app_hash = _app_addapplication (NULL, DataAppService, &service_name, name_string, service_path);

						if (app_hash)
						{
							PITEM_APP ptr_app;

							ptr_app = _app_getappitem (app_hash);

							if (ptr_app)
							{
								_app_setappinfo (ptr_app, InfoTimestampPtr, &service_timestamp);
								_app_setappinfo (ptr_app, InfoBytesData, _r_obj_createbyteex (service_sd, sd_length));

								_r_obj_dereference (ptr_app);
							}
						}

						LocalFree (service_sd);

						_r_obj_dereference (name_string);
					}

					_r_mem_free (service_sid);
				}

				_r_obj_dereference (service_path);
			}

			RegCloseKey (hkey);
		}

		_r_mem_free (buffer);
	}

	CloseServiceHandle (hsvcmgr);
}

VOID _app_generate_rulescontrol (_In_ HMENU hsubmenu, _In_opt_ ULONG_PTR app_hash)
{
	ITEM_STATUS status;

	_app_getcount (&status);

	if (!app_hash || !status.rules_count)
	{
		AppendMenu (hsubmenu, MF_SEPARATOR, 0, NULL);
		AppendMenu (hsubmenu, MF_STRING, IDX_RULES_SPECIAL, _r_locale_getstring (IDS_STATUS_EMPTY));

		_r_menu_enableitem (hsubmenu, IDX_RULES_SPECIAL, MF_BYCOMMAND, FALSE);
	}
	else
	{
		MENUITEMINFO mii;
		WCHAR buffer[128];
		PITEM_RULE ptr_rule;
		BOOLEAN is_global;
		BOOLEAN is_enabled;

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

			AppendMenu (hsubmenu, MF_SEPARATOR, 0, NULL);

			for (UINT8 loop = 0; loop < 2; loop++)
			{
				SIZE_T limit_group = 14; // limit rules

				_r_queuedlock_acquireshared (&lock_rules);

				for (SIZE_T i = 0; i < _r_obj_getlistsize (rules_list) && limit_group; i++)
				{
					ptr_rule = _r_obj_getlistitem (rules_list, i);

					if (!ptr_rule)
						continue;

					is_global = (ptr_rule->is_enabled && _r_obj_ishashtableempty (ptr_rule->apps));
					is_enabled = is_global || (ptr_rule->is_enabled && (_r_obj_findhashtable (ptr_rule->apps, app_hash)));

					if (ptr_rule->type != DataRuleUser || (type == 0 && (!ptr_rule->is_readonly || is_global)) || (type == 1 && (ptr_rule->is_readonly || is_global)))
						continue;

					if ((loop == 0 && !is_enabled) || (loop == 1 && is_enabled))
						continue;

					_r_str_printf (buffer, RTL_NUMBER_OF (buffer), _r_locale_getstring (IDS_RULE_APPLY_2), _r_obj_getstring (ptr_rule->name));

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
		}
	}

	AppendMenu (hsubmenu, MF_SEPARATOR, 0, NULL);
	AppendMenu (hsubmenu, MF_STRING, IDM_OPENRULESEDITOR, _r_locale_getstring (IDS_OPENRULESEDITOR));
}

VOID _app_generate_timerscontrol (_In_ HMENU hsubmenu, _In_opt_ PITEM_APP ptr_app)
{
	LONG64 current_time;
	LONG64 app_time;
	LONG64 timestamp;
	PR_STRING interval_string;
	UINT index;
	BOOLEAN is_checked = (ptr_app == NULL);

	current_time = _r_unixtime_now ();

	if (ptr_app)
	{
		PVOID timer_ptr = _app_getappinfo (ptr_app, InfoTimerPtr);
		app_time = timer_ptr ? *((PLONG64)timer_ptr) : 0;
	}
	else
	{
		app_time = 0;
	}

	for (SIZE_T i = 0; i < RTL_NUMBER_OF (timer_array); i++)
	{
		timestamp = timer_array[i];

		interval_string = _r_format_interval (timestamp + 1, 1);

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
	{
		_r_menu_checkitem (hsubmenu, IDM_DISABLETIMER, IDM_DISABLETIMER, MF_BYCOMMAND, IDM_DISABLETIMER);
	}
}

BOOLEAN _app_setruletoapp (_In_ HWND hwnd, _Inout_ PITEM_RULE ptr_rule, _In_ INT item_id, _In_ PITEM_APP ptr_app, _In_ BOOLEAN is_enable)
{
	INT listview_id;

	if (ptr_rule->is_forservices && (ptr_app->app_hash == config.ntoskrnl_hash || ptr_app->app_hash == config.svchost_hash))
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
		listview_id = _app_getlistviewbytype_id (ptr_rule->type);

		_app_updateitembylparam (hwnd, _r_listview_getitemlparam (hwnd, listview_id, item_id), FALSE);
	}

	_app_updateitembylparam (hwnd, ptr_app->app_hash, TRUE);

	return TRUE;
}

_Ret_maybenull_
PR_STRING _app_parsehoststring (_In_ LPCWSTR hostname, _In_opt_ USHORT port)
{
	static R_STRINGREF divider_sr = PR_STRINGREF_INIT (DIVIDER_RULE);
	R_STRINGBUILDER buffer;
	PR_STRING string;
	PDNS_RECORD dns_records = NULL;

	_r_obj_initializestringbuilder (&buffer);

	// ipv4 address
	DNS_STATUS code = DnsQuery (hostname, DNS_TYPE_A, DNS_QUERY_NO_HOSTS_FILE, NULL, &dns_records, NULL);

	if (code != DNS_ERROR_RCODE_NO_ERROR)
	{
		if (code != DNS_INFO_NO_RECORDS)
			_r_log (LOG_LEVEL_INFO, NULL, L"DnsQuery (DNS_TYPE_A)", code, hostname);
	}
	else
	{
		if (dns_records)
		{
			for (PDNS_RECORD current = dns_records; current != NULL; current = current->pNext)
			{
				// ipv4 address
				WCHAR str[INET_ADDRSTRLEN];

				if (_app_formatip (AF_INET, &(current->Data.A.IpAddress), str, RTL_NUMBER_OF (str), TRUE))
				{
					_r_obj_appendstringbuilder (&buffer, str);

					if (port)
						_r_obj_appendstringbuilderformat (&buffer, L":%d", port);

					_r_obj_appendstringbuilder3 (&buffer, &divider_sr);
				}
			}
		}
	}

	if (dns_records)
	{
		DnsRecordListFree (dns_records, DnsFreeRecordList);
		dns_records = NULL;
	}

	// ipv6 address
	code = DnsQuery (hostname, DNS_TYPE_AAAA, DNS_QUERY_NO_HOSTS_FILE, NULL, &dns_records, NULL);

	if (code != DNS_ERROR_RCODE_NO_ERROR)
	{
		if (code != DNS_INFO_NO_RECORDS)
			_r_log (LOG_LEVEL_INFO, NULL, L"DnsQuery (DNS_TYPE_AAAA)", code, hostname);
	}
	else
	{
		if (dns_records)
		{
			for (PDNS_RECORD current = dns_records; current != NULL; current = current->pNext)
			{
				WCHAR str[INET6_ADDRSTRLEN];

				if (_app_formatip (AF_INET6, &current->Data.AAAA.Ip6Address, str, RTL_NUMBER_OF (str), TRUE))
				{
					_r_obj_appendstringbuilderformat (&buffer, L"[%s]", str);

					if (port)
						_r_obj_appendstringbuilderformat (&buffer, L":%d", port);

					_r_obj_appendstringbuilder3 (&buffer, &divider_sr);
				}
			}
		}
	}

	if (dns_records)
	{
		DnsRecordListFree (dns_records, DnsFreeRecordList);
		dns_records = NULL;
	}

	string = _r_obj_finalstringbuilder (&buffer);

	_r_str_trimstring (string, &divider_sr);

	if (!_r_obj_isstringempty (string))
	{
		return string;
	}

	_r_obj_deletestringbuilder (&buffer);

	return NULL;
}

_Success_ (return)
BOOLEAN _app_parsenetworkstring (_In_ LPCWSTR network_string, _Inout_ PITEM_ADDRESS address)
{
	NET_ADDRESS_INFO ni;
	NET_ADDRESS_INFO ni_end;
	USHORT port;
	BYTE prefix_length;

	ULONG types;
	ULONG code;

	types = NET_STRING_ANY_ADDRESS | NET_STRING_ANY_SERVICE | NET_STRING_IP_NETWORK | NET_STRING_ANY_ADDRESS_NO_SCOPE | NET_STRING_ANY_SERVICE_NO_SCOPE;

	if (address->is_range)
	{
		code = ParseNetworkString (address->range_start, types, &ni, &port, NULL);

		if (code != ERROR_SUCCESS)
			goto CleanupExit;

		code = ParseNetworkString (address->range_end, types, &ni_end, NULL, NULL);
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
			ULONG mask = 0;

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
	else if (ni.Format == NET_ADDRESS_DNS_NAME)
	{
		R_STRINGREF dns_name;
		ULONG_PTR dns_hash;
		PR_STRING host_string;

		_r_obj_initializestringref (&dns_name, ni.NamedAddress.Address);

		dns_hash = _r_obj_getstringrefhash (&dns_name);
		host_string = _app_getcachetable (cache_dns, dns_hash, &lock_cache_dns);

		if (host_string)
		{
			_r_str_copy (address->host, RTL_NUMBER_OF (address->host), host_string->buffer);
			_r_obj_dereference (host_string);

			return TRUE;
		}

		host_string = _app_parsehoststring (ni.NamedAddress.Address, port);

		if (host_string)
		{
			_r_str_copy (address->host, RTL_NUMBER_OF (address->host), host_string->buffer);

			_app_addcachetable (cache_dns, dns_hash, &lock_cache_dns, host_string);

			return TRUE;
		}

		return FALSE;
	}

CleanupExit:

	_r_log (LOG_LEVEL_INFO, NULL, L"ParseNetworkString", code, network_string);

	return FALSE;
}

BOOLEAN _app_preparserulestring (_In_ PR_STRINGREF rule, _In_ PITEM_ADDRESS address)
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
	SIZE_T length;

	length = _r_obj_getstringreflength (rule);

	// validate rule symbols if rule is unknown/new...
	// "type" is maybe already set, so check it to prevent duplicate execution

	if (address->type == DataUnknown)
	{
		for (SIZE_T i = 0; i < length; i++)
		{
			if (IsCharAlphaNumeric (rule->buffer[i]))
				continue;

			BOOLEAN is_valid = FALSE;

			for (SIZE_T j = 0; j < RTL_NUMBER_OF (valid_chars); j++)
			{
				if (rule->buffer[i] == valid_chars[j])
				{
					is_valid = TRUE;
					break;
				}
			}

			if (is_valid)
				continue;

			return FALSE;
		}
	}

	// parse rule range
	address->is_range = _r_str_splitatchar (rule, DIVIDER_RULE_RANGE, &range_start_part, &range_end_part);

	// extract start and end position of rule
	if (address->is_range)
	{
		// there is incorrect range syntax
		if (_r_obj_isstringempty (&range_start_part) || _r_obj_isstringempty (&range_end_part))
		{
			address->is_range = FALSE;
			address->type = DataUnknown;

			return FALSE;
		}

		_r_str_copystring (address->range_start, RTL_NUMBER_OF (address->range_start), &range_start_part);
		_r_str_copystring (address->range_end, RTL_NUMBER_OF (address->range_end), &range_end_part);
	}

	// check rule for port
	if (address->type == DataUnknown)
	{
		address->type = DataTypePort;

		for (SIZE_T i = 0; i < length; i++)
		{
			if (!_r_str_isdigit (rule->buffer[i]) && rule->buffer[i] != DIVIDER_RULE_RANGE)
			{
				address->type = DataUnknown;
				break;
			}
		}
	}

	if (address->type != DataUnknown)
		return TRUE;

#define RULE_TYPE_HOST (NET_STRING_NAMED_ADDRESS | NET_STRING_NAMED_SERVICE)
#define RULE_TYPE_IP (NET_STRING_IP_ADDRESS | NET_STRING_IP_SERVICE | NET_STRING_IP_NETWORK | NET_STRING_IP_ADDRESS_NO_SCOPE | NET_STRING_IP_SERVICE_NO_SCOPE)

	WCHAR rule_string[256];
	_r_str_copystring (rule_string, RTL_NUMBER_OF (rule_string), rule);

	// check rule for ip address
	if (address->is_range)
	{
		if (ParseNetworkString (address->range_start, RULE_TYPE_IP, NULL, NULL, NULL) == ERROR_SUCCESS && ParseNetworkString (address->range_end, RULE_TYPE_IP, NULL, NULL, NULL) == ERROR_SUCCESS)
		{
			address->type = DataTypeIp;
			return TRUE;
		}
	}
	else
	{
		if (ParseNetworkString (rule_string, RULE_TYPE_IP, NULL, NULL, NULL) == ERROR_SUCCESS)
		{
			address->type = DataTypeIp;
			return TRUE;
		}
	}

	if (ParseNetworkString (rule_string, RULE_TYPE_HOST, NULL, NULL, NULL) == ERROR_SUCCESS)
	{
		address->type = DataTypeHost;
		address->is_range = FALSE; // reset range status

		return TRUE;
	}

	return FALSE;
}

BOOLEAN _app_parserulestring (_In_opt_ PR_STRINGREF rule, _Inout_opt_ PITEM_ADDRESS address)
{
	if (_r_obj_isstringempty (rule))
		return TRUE;

	ITEM_ADDRESS address_copy;
	ULONG_PTR rule_hash;
	BOOLEAN is_checkonly = FALSE;

	if (!address)
	{
		RtlZeroMemory (&address_copy, sizeof (address_copy));

		is_checkonly = TRUE;
		address = &address_copy;
	}

	// auto-parse rule type
	rule_hash = _r_obj_getstringrefhash (rule);

	if (!_app_preparserulestring (rule, address))
		return FALSE;

	if (is_checkonly)
		return TRUE;

	if (address->type == DataTypePort)
	{
		if (address->is_range)
		{
			R_STRINGREF sr;

			// ...port range
			_r_obj_initializestringref (&sr, address->range_start);

			address->range.valueLow.type = FWP_UINT16;
			address->range.valueLow.uint16 = (UINT16)_r_str_touinteger (&sr);

			_r_obj_initializestringref (&sr, address->range_end);

			address->range.valueHigh.type = FWP_UINT16;
			address->range.valueHigh.uint16 = (UINT16)_r_str_touinteger (&sr);

			return TRUE;
		}
		else
		{
			// ...port
			address->port = (UINT16)_r_str_touinteger (rule);

			return TRUE;
		}
	}
	else if (address->type == DataTypeIp || address->type == DataTypeHost)
	{
		address->type = DataTypeIp;

		WCHAR rule_string[256];
		_r_str_copystring (rule_string, RTL_NUMBER_OF (rule_string), rule);

		if (!_app_parsenetworkstring (rule_string, address))
		{
			return FALSE;
		}
	}

	return TRUE;
}

_Ret_maybenull_
PR_STRING _app_resolveaddress (_In_ ADDRESS_FAMILY af, _In_ LPCVOID address)
{
	PDNS_RECORD dns_records;
	PR_STRING arpa_string;
	PR_STRING string;
	DNS_STATUS code;

	arpa_string = _app_formataddress (af, 0, address, 0, FMTADDR_AS_ARPA);
	string = NULL;

	if (arpa_string)
	{
		code = DnsQuery (arpa_string->buffer, DNS_TYPE_PTR, DNS_QUERY_NO_HOSTS_FILE, NULL, &dns_records, NULL);

		if (code == DNS_ERROR_RCODE_NO_ERROR)
		{
			if (dns_records)
			{
				if (!_r_str_isempty (dns_records->Data.PTR.pNameHost))
				{
					string = _r_obj_createstring (dns_records->Data.PTR.pNameHost);
				}

				DnsRecordListFree (dns_records, DnsFreeRecordList);
			}
		}

		_r_obj_dereference (arpa_string);
	}

	return string;
}

VOID NTAPI _app_queuefileinformation (_In_ PVOID arglist, _In_ ULONG busy_count)
{
	PITEM_APP ptr_app;
	HWND hwnd;

	ptr_app = arglist;
	hwnd = _r_app_gethwnd ();

	// query app icon
	_app_getappicon (ptr_app, TRUE, &ptr_app->icon_id, NULL);

	// query file version info
	_app_getversioninfo (ptr_app);

	// query certificate information
	if (_r_config_getboolean (L"IsCertificatesEnabled", TRUE))
	{
		_app_getsignatureinfo (ptr_app);
	}

	// redraw listview
	if (!(busy_count % 4)) // lol, hack!!!
	{
		if (_r_wnd_isvisible (hwnd))
		{
			_r_listview_redraw (hwnd, _app_getcurrentlistview_id (hwnd), -1);
		}
	}

	_r_obj_dereference (ptr_app);
}

VOID NTAPI _app_queuenotifyinformation (_In_ PVOID arglist, _In_ ULONG busy_count)
{
	PITEM_CONTEXT context;
	PITEM_APP ptr_app;
	PR_STRING remote_host_str;
	PR_STRING localized_string;
	HWND hctrl;

	context = arglist;

	// query file icon
	if (!context->ptr_log->hicon)
	{
		ptr_app = _app_getappitem (context->ptr_log->app_hash);

		_app_getappicon (ptr_app, FALSE, NULL, &context->ptr_log->hicon);

		if (ptr_app)
			_r_obj_dereference (ptr_app);
	}

	if (_r_wnd_isvisible (context->hwnd) && context->ptr_log->app_hash == _app_notifyget_id (context->hwnd, FALSE))
	{
		hctrl = GetDlgItem (context->hwnd, IDC_HEADER_ID);

		// set icon
		if (hctrl)
		{
			SetWindowLongPtr (hctrl, GWLP_USERDATA, (LONG_PTR)context->ptr_log->hicon);
			InvalidateRect (hctrl, NULL, TRUE);
		}
	}

	// query notification host name
	if (_r_config_getboolean (L"IsNetworkResolutionsEnabled", FALSE))
	{
		if (!context->ptr_log->remote_host_str)
		{
			remote_host_str = _app_resolveaddress (context->ptr_log->af, &context->ptr_log->remote_addr);
		}
		else
		{
			remote_host_str = _r_obj_reference (context->ptr_log->remote_host_str);
		}

		if (remote_host_str)
		{
			if (_r_wnd_isvisible (context->hwnd) && context->ptr_log->app_hash == _app_notifyget_id (context->hwnd, FALSE))
			{
				localized_string = _r_obj_concatstrings (2, _r_locale_getstring (IDS_HOST), L":");
				_r_ctrl_settabletext (context->hwnd, IDC_HOST_ID, &localized_string->sr, IDC_HOST_TEXT, &remote_host_str->sr);

				_r_obj_dereference (localized_string);
			}

			_r_obj_dereference (remote_host_str);
		}
	}

	_r_obj_dereference (context->ptr_log);

	_r_freelist_deleteitem (&context_free_list, context);
}

PR_STRING _app_getemptystring ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static PR_STRING string = NULL;

	if (_r_initonce_begin (&init_once))
	{
		string = _r_obj_createstring (L"");

		_r_initonce_end (&init_once);
	}

	return _r_obj_referencesafe (string);
}

VOID NTAPI _app_queueresolveinformation (_In_ PVOID arglist, _In_ ULONG busy_count)
{
	PITEM_CONTEXT context;
	PITEM_APP ptr_app;
	HWND hwnd;
	INT listview_id;
	BOOLEAN is_log;
	BOOLEAN is_resolutionenabled;

	context = arglist;

	is_log = (context->listview_id == IDC_LOG);
	is_resolutionenabled = _r_config_getboolean (L"IsNetworkResolutionsEnabled", FALSE);

	if (is_log)
	{
		// query file icon
		if (!context->ptr_log->icon_id)
		{
			ptr_app = _app_getappitem (context->ptr_log->app_hash);

			_app_getappicon (ptr_app, TRUE, &context->ptr_log->icon_id, NULL);

			if (ptr_app)
				_r_obj_dereference (ptr_app);
		}

		// query address information
		if (is_resolutionenabled)
		{
			_r_obj_movereference (&context->ptr_log->local_host_str, _app_resolveaddress (context->ptr_log->af, &context->ptr_log->local_addr));
			_r_obj_movereference (&context->ptr_log->remote_host_str, _app_resolveaddress (context->ptr_log->af, &context->ptr_log->remote_addr));
		}

		if (!context->ptr_log->local_host_str)
			context->ptr_log->local_host_str = _app_getemptystring ();

		if (!context->ptr_log->remote_host_str)
			context->ptr_log->remote_host_str = _app_getemptystring ();

		listview_id = IDC_LOG;

		_r_obj_dereference (context->ptr_log);
	}
	else
	{
		// query file icon
		if (!context->ptr_network->icon_id)
		{
			ptr_app = _app_getappitem (context->ptr_network->app_hash);

			_app_getappicon (ptr_app, TRUE, &context->ptr_network->icon_id, NULL);

			if (ptr_app)
				_r_obj_dereference (ptr_app);
		}

		// query address information
		_r_obj_movereference (&context->ptr_network->local_addr_str, _app_formataddress (context->ptr_network->af, 0, &context->ptr_network->local_addr, 0, 0));
		_r_obj_movereference (&context->ptr_network->remote_addr_str, _app_formataddress (context->ptr_network->af, 0, &context->ptr_network->remote_addr, 0, 0));

		if (is_resolutionenabled)
		{
			_r_obj_movereference (&context->ptr_network->local_host_str, _app_resolveaddress (context->ptr_network->af, &context->ptr_network->local_addr));
			_r_obj_movereference (&context->ptr_network->remote_host_str, _app_resolveaddress (context->ptr_network->af, &context->ptr_network->remote_addr));
		}

		if (!context->ptr_network->local_host_str)
			context->ptr_network->local_host_str = _app_getemptystring ();

		if (!context->ptr_network->remote_host_str)
			context->ptr_network->remote_host_str = _app_getemptystring ();

		listview_id = IDC_NETWORK;

		_r_obj_dereference (context->ptr_network);
	}

	// redraw listview
	if (!(busy_count % 4)) // lol, hack!!!
	{
		hwnd = _r_app_gethwnd ();

		if (_r_wnd_isvisible (hwnd) && (_app_getcurrentlistview_id (hwnd) == listview_id))
		{
			_r_listview_redraw (hwnd, listview_id, -1);
		}
	}

	_r_freelist_deleteitem (&context_free_list, context);
}

_Ret_maybenull_
HBITMAP _app_bitmapfromico (_In_ HICON hicon, _In_ INT icon_size)
{
	RECT icon_rect;
	SetRect (&icon_rect, 0, 0, icon_size, icon_size);

	HBITMAP hbitmap = NULL;
	HDC screen_hdc = GetDC (NULL);

	if (screen_hdc)
	{
		HDC hdc = CreateCompatibleDC (screen_hdc);

		if (hdc)
		{
			BITMAPINFO bitmap_info = {0};
			PVOID pbits;

			bitmap_info.bmiHeader.biSize = sizeof (bitmap_info);
			bitmap_info.bmiHeader.biPlanes = 1;
			bitmap_info.bmiHeader.biCompression = BI_RGB;

			bitmap_info.bmiHeader.biWidth = icon_size;
			bitmap_info.bmiHeader.biHeight = icon_size;
			bitmap_info.bmiHeader.biBitCount = 32;

			hbitmap = CreateDIBSection (hdc, &bitmap_info, DIB_RGB_COLORS, &pbits, NULL, 0);

			if (hbitmap)
			{
				HGDIOBJ old_bitmap = SelectObject (hdc, hbitmap);

				BLENDFUNCTION blend_func = {0};
				//blend_func.BlendOp = AC_SRC_OVER;
				blend_func.AlphaFormat = AC_SRC_ALPHA;
				blend_func.SourceConstantAlpha = 255;

				BP_PAINTPARAMS paint_params = {0};
				paint_params.cbSize = sizeof (paint_params);
				paint_params.dwFlags = BPPF_ERASE;
				paint_params.pBlendFunction = &blend_func;

				HDC buffer_hdc = NULL;

				HPAINTBUFFER paint_buffer = BeginBufferedPaint (hdc, &icon_rect, BPBF_DIB, &paint_params, &buffer_hdc);

				if (paint_buffer)
				{
					DrawIconEx (buffer_hdc, 0, 0, hicon, icon_size, icon_size, 0, NULL, DI_NORMAL);
					EndBufferedPaint (paint_buffer, TRUE);
				}
				else
				{
					_r_dc_fillrect (hdc, &icon_rect, GetSysColor (COLOR_MENU));
					DrawIconEx (hdc, 0, 0, hicon, icon_size, icon_size, 0, NULL, DI_NORMAL);
				}

				SelectObject (hdc, old_bitmap);
			}

			SAFE_DELETE_DC (hdc);
		}

		ReleaseDC (NULL, screen_hdc);
	}

	return hbitmap;
}

_Ret_maybenull_
HBITMAP _app_bitmapfrompng (_In_opt_ HINSTANCE hinst, _In_ LPCWSTR name, _In_ INT icon_size)
{
	BOOLEAN is_success = FALSE;

	UINT frame_count = 0;
	ULONG resource_length = 0;
	HDC screen_hdc = NULL;
	HDC hdc = NULL;
	BITMAPINFO bi = {0};
	HBITMAP hbitmap = NULL;
	PVOID bitmap_buffer = NULL;
	WICInProcPointer resource_buffer = NULL;
	IWICStream *wicStream = NULL;
	IWICBitmapSource *wicBitmapSource = NULL;
	IWICBitmapDecoder *wicDecoder = NULL;
	IWICBitmapFrameDecode *wicFrame = NULL;
	IWICImagingFactory *wicFactory = NULL;
	IWICFormatConverter *wicFormatConverter = NULL;
	IWICBitmapScaler *wicScaler = NULL;
	WICPixelFormatGUID pixelFormat;
	WICRect rect = {0, 0, icon_size, icon_size};

	if (FAILED (CoCreateInstance (&CLSID_WICImagingFactory1, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, &wicFactory)))
		goto CleanupExit;

	resource_buffer = (WICInProcPointer)_r_res_loadresource (hinst, name, L"PNG", &resource_length);

	if (!resource_buffer)
		goto CleanupExit;

	if (FAILED (IWICImagingFactory_CreateStream (wicFactory, &wicStream)))
		goto CleanupExit;

	if (FAILED (IWICStream_InitializeFromMemory (wicStream, resource_buffer, resource_length)))
		goto CleanupExit;

	if (FAILED (IWICImagingFactory_CreateDecoder (wicFactory, &GUID_ContainerFormatPng, NULL, &wicDecoder)))
		goto CleanupExit;

	if (FAILED (IWICBitmapDecoder_Initialize (wicDecoder, (IStream *)wicStream, WICDecodeMetadataCacheOnLoad)))
		goto CleanupExit;

	// Get the Frame count
	if (FAILED (IWICBitmapDecoder_GetFrameCount (wicDecoder, &frame_count)) || frame_count < 1)
		goto CleanupExit;

	// Get the Frame
	if (FAILED (IWICBitmapDecoder_GetFrame (wicDecoder, 0, &wicFrame)))
		goto CleanupExit;

	// Get the WicFrame image format
	if (FAILED (IWICBitmapFrameDecode_GetPixelFormat (wicFrame, &pixelFormat)))
		goto CleanupExit;

	// Check if the image format is supported:
	if (IsEqualGUID (&pixelFormat, &GUID_WICPixelFormat32bppPRGBA))
	{
		wicBitmapSource = (IWICBitmapSource *)wicFrame;
	}
	else
	{
		if (FAILED (IWICImagingFactory_CreateFormatConverter (wicFactory, &wicFormatConverter)))
			goto CleanupExit;

		if (FAILED (IWICFormatConverter_Initialize (wicFormatConverter, (IWICBitmapSource *)wicFrame, &GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom)))
			goto CleanupExit;

		IWICFormatConverter_QueryInterface (wicFormatConverter, &IID_IWICBitmapSource, &wicBitmapSource);

		IWICFormatConverter_Release (wicFormatConverter);

		IWICBitmapFrameDecode_Release (wicFrame);
	}

	bi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = rect.Width;
	bi.bmiHeader.biHeight = -(rect.Height);
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;

	screen_hdc = GetDC (NULL);
	hdc = CreateCompatibleDC (screen_hdc);
	hbitmap = CreateDIBSection (screen_hdc, &bi, DIB_RGB_COLORS, &bitmap_buffer, NULL, 0);

	if (FAILED (IWICImagingFactory_CreateBitmapScaler (wicFactory, &wicScaler)))
		goto CleanupExit;

	if (FAILED (IWICBitmapScaler_Initialize (wicScaler, wicBitmapSource, rect.Width, rect.Height, WICBitmapInterpolationModeFant)))
		goto CleanupExit;

	if (FAILED (IWICBitmapScaler_CopyPixels (wicScaler, &rect, rect.Width * 4, rect.Width * rect.Height * 4, (PBYTE)bitmap_buffer)))
		goto CleanupExit;

	is_success = TRUE;

CleanupExit:

	if (hdc)
		DeleteDC (hdc);

	if (screen_hdc)
		ReleaseDC (NULL, screen_hdc);

	if (wicScaler)
		IWICBitmapScaler_Release (wicScaler);

	if (wicBitmapSource)
		IWICBitmapSource_Release (wicBitmapSource);

	if (wicStream)
		IWICStream_Release (wicStream);

	if (wicDecoder)
		IWICBitmapDecoder_Release (wicDecoder);

	if (wicFactory)
		IWICImagingFactory_Release (wicFactory);

	if (!is_success)
	{
		SAFE_DELETE_OBJECT (hbitmap);

		return NULL;
	}

	return hbitmap;
}
