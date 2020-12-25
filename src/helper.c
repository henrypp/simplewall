// simplewall
// Copyright (c) 2016-2021 Henry++

#include "global.h"

VOID NTAPI _app_dereferenceapp (PVOID entry)
{
	PITEM_APP ptr_item = entry;

	SAFE_DELETE_REFERENCE (ptr_item->display_name);
	SAFE_DELETE_REFERENCE (ptr_item->real_path);
	SAFE_DELETE_REFERENCE (ptr_item->short_name);
	SAFE_DELETE_REFERENCE (ptr_item->original_path);

	SAFE_DELETE_REFERENCE (ptr_item->pnotification);

	SAFE_DELETE_REFERENCE (ptr_item->guids);
}

VOID NTAPI _app_dereferenceruleconfig (PVOID entry)
{
	PITEM_RULE_CONFIG ptr_rule_config = entry;

	SAFE_DELETE_REFERENCE (ptr_rule_config->name);
	SAFE_DELETE_REFERENCE (ptr_rule_config->apps);
}

VOID NTAPI _app_dereferencelog (PVOID entry)
{
	PITEM_LOG ptr_item = entry;

	SAFE_DELETE_REFERENCE (ptr_item->path);
	SAFE_DELETE_REFERENCE (ptr_item->provider_name);
	SAFE_DELETE_REFERENCE (ptr_item->filter_name);
	SAFE_DELETE_REFERENCE (ptr_item->username);

	SAFE_DELETE_ICON (ptr_item->hicon);
}

VOID NTAPI _app_dereferencenetwork (PVOID entry)
{
	PITEM_NETWORK ptr_network = entry;

	SAFE_DELETE_REFERENCE (ptr_network->path);
}

VOID NTAPI _app_dereferencerule (PVOID entry)
{
	PITEM_RULE ptr_item = entry;

	SAFE_DELETE_REFERENCE (ptr_item->apps);

	SAFE_DELETE_REFERENCE (ptr_item->name);
	SAFE_DELETE_REFERENCE (ptr_item->rule_remote);
	SAFE_DELETE_REFERENCE (ptr_item->rule_local);

	SAFE_DELETE_REFERENCE (ptr_item->guids);
}

PR_HASHSTORE _app_addcachetable (PR_HASHTABLE table, SIZE_T hash_code, PR_STRING string, INT number)
{
	R_HASHSTORE hashstore = {0};

	_r_obj_initializehashstoreex (&hashstore, string, number);

	return _r_obj_addhashtableitem (table, hash_code, &hashstore);
}

PR_STRING _app_resolveaddress (ADDRESS_FAMILY af, LPCVOID paddr)
{
	PR_STRING string = NULL;
	PR_STRING arpa_string;
	PDNS_RECORD dns_records;
	DNS_STATUS code;

	arpa_string = _app_formataddress (af, 0, paddr, 0, FMTADDR_AS_ARPA);

	if (arpa_string)
	{
		code = DnsQuery (arpa_string->buffer, DNS_TYPE_PTR, DNS_QUERY_NO_HOSTS_FILE, NULL, &dns_records, NULL);

		if (code == DNS_ERROR_RCODE_NO_ERROR)
		{
			if (dns_records)
			{
				if (!_r_str_isempty (dns_records->Data.PTR.pNameHost))
					string = _r_obj_createstring (dns_records->Data.PTR.pNameHost);

				DnsRecordListFree (dns_records, DnsFreeRecordList);
			}
		}

		_r_obj_dereference (arpa_string);
	}

	return string;
}

PR_STRING _app_formataddress (ADDRESS_FAMILY af, UINT8 proto, LPCVOID paddr, UINT16 port, ULONG flags)
{
	WCHAR formatted_address[DNS_MAX_NAME_BUFFER_LENGTH] = {0};
	PIN_ADDR p4addr = (PIN_ADDR)paddr;
	PIN6_ADDR p6addr = (PIN6_ADDR)paddr;
	BOOLEAN is_success = FALSE;

	if ((flags & FMTADDR_AS_ARPA) != 0)
	{
		if (af == AF_INET)
		{
			_r_str_appendformat (formatted_address, RTL_NUMBER_OF (formatted_address), L"%hhu.%hhu.%hhu.%hhu.%s", p4addr->s_impno, p4addr->s_lh, p4addr->s_host, p4addr->s_net, DNS_IP4_REVERSE_DOMAIN_STRING_W);
		}
		else if (af == AF_INET6)
		{
			for (INT i = sizeof (IN6_ADDR) - 1; i >= 0; i--)
				_r_str_appendformat (formatted_address, RTL_NUMBER_OF (formatted_address), L"%hhx.%hhx.", p6addr->s6_addr[i] & 0xF, (p6addr->s6_addr[i] >> 4) & 0xF);

			_r_str_append (formatted_address, RTL_NUMBER_OF (formatted_address), DNS_IP6_REVERSE_DOMAIN_STRING_W);
		}

		is_success = TRUE;
	}
	else
	{
		if ((flags & FMTADDR_USE_PROTOCOL) != 0)
		{
			_r_str_printf (formatted_address, RTL_NUMBER_OF (formatted_address), L"%s://", _app_getprotoname (proto, AF_UNSPEC, SZ_UNKNOWN));
		}

		WCHAR addr_str[DNS_MAX_NAME_BUFFER_LENGTH] = {0};

		if (_app_formatip (af, paddr, addr_str, RTL_NUMBER_OF (addr_str), (flags & FMTADDR_AS_RULE) != 0))
		{
			if (af == AF_INET6 && port)
			{
				_r_str_appendformat (formatted_address, RTL_NUMBER_OF (formatted_address), L"[%s]", addr_str);
			}
			else
			{
				_r_str_append (formatted_address, RTL_NUMBER_OF (formatted_address), addr_str);
			}

			is_success = TRUE;
		}

		if (port && (flags & FMTADDR_USE_PROTOCOL) == 0)
		{
			_r_str_appendformat (formatted_address, RTL_NUMBER_OF (formatted_address), !_r_str_isempty (formatted_address) ? L":%" TEXT (PRIu16) : L"%" TEXT (PRIu16), port);
			is_success = TRUE;
		}
	}

	if ((flags & FMTADDR_RESOLVE_HOST) != 0)
	{
		if (is_success && _r_config_getboolean (L"IsNetworkResolutionsEnabled", FALSE))
		{
			PR_STRING domain_string = NULL;
			SIZE_T addr_hash = _r_str_hash (formatted_address, _r_str_length (formatted_address));
			PR_HASHSTORE hashstore = _r_obj_findhashtable (cache_hosts, addr_hash);

			if (hashstore)
			{
				domain_string = hashstore->value_string;
			}
			else
			{
				hashstore = _app_addcachetable (cache_hosts, addr_hash, NULL, 0);

				if (hashstore)
				{
					domain_string = _app_resolveaddress (af, paddr);

					if (domain_string)
					{
						hashstore->value_string = domain_string;
					}
				}
			}

			if (domain_string)
			{
				_r_str_appendformat (formatted_address, RTL_NUMBER_OF (formatted_address), L" (%s)", domain_string->buffer);
			}
		}
	}

	if (is_success)
		return _r_obj_createstring (formatted_address);

	return NULL;
}

BOOLEAN _app_formatip (ADDRESS_FAMILY af, LPCVOID paddr, LPWSTR out_buffer, ULONG buffer_size, BOOLEAN is_checkempty)
{
	PIN_ADDR p4addr;
	PIN6_ADDR p6addr;

	if (af == AF_INET)
	{
		p4addr = (PIN_ADDR)paddr;

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
		p6addr = (PIN6_ADDR)paddr;

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

PR_STRING _app_formatport (UINT16 port, BOOLEAN is_noempty)
{
	if (!port)
		return NULL;

	if (is_noempty)
	{
		LPCWSTR sevice_string = _app_getservicename (port, NULL);

		if (sevice_string)
		{
			return _r_format_string (L"%" TEXT (PRIu16) L" (%s)", port, sevice_string);
		}
		else
		{
			return _r_format_string (L"%" TEXT (PRIu16), port);
		}
	}
	else
	{
		return _r_format_string (L"%" TEXT (PRIu16) L" (%s)", port, _app_getservicename (port, SZ_UNKNOWN));
	}

	return NULL;
}

VOID _app_freelogstack ()
{
	PSLIST_ENTRY list_item;
	PITEM_LOG_LISTENTRY ptr_entry;

	while (TRUE)
	{
		list_item = RtlInterlockedPopEntrySList (&log_list_stack);

		if (!list_item)
			break;

		ptr_entry = CONTAINING_RECORD (list_item, ITEM_LOG_LISTENTRY, list_entry);

		if (ptr_entry->body)
			_r_obj_dereference (ptr_entry->body);

		_aligned_free (ptr_entry);
	}
}

VOID _app_getappicon (const PITEM_APP ptr_app, BOOLEAN is_small, PINT picon_id, HICON* picon)
{
	BOOLEAN is_iconshidden = _r_config_getboolean (L"IsIconsHidden", FALSE);

	if (ptr_app->type == DataAppRegular || ptr_app->type == DataAppService)
	{
		if (is_iconshidden || (_r_obj_isstringempty (ptr_app->real_path) || !_app_getfileicon (ptr_app->real_path->buffer, is_small, picon_id, picon)))
		{
			if (picon_id)
				*picon_id = config.icon_id;

			if (picon)
				*picon = CopyIcon (is_small ? config.hicon_small : config.hicon_large);
		}
	}
	else if (ptr_app->type == DataAppUWP)
	{
		if (picon_id)
			*picon_id = config.icon_uwp_id;

		if (picon)
			*picon = CopyIcon (is_small ? config.hicon_uwp : config.hicon_large); // small-only!
	}
	else
	{
		if (picon_id)
			*picon_id = config.icon_id;

		if (picon)
			*picon = CopyIcon (is_small ? config.hicon_small : config.hicon_large);
	}
}

LPCWSTR _app_getdisplayname (PITEM_APP ptr_app, BOOLEAN is_shortened)
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

	return ptr_app->real_path->buffer;
}

BOOLEAN _app_getfileicon (LPCWSTR path, BOOLEAN is_small, PINT picon_id, HICON* picon)
{
	SHFILEINFO shfi = {0};
	ULONG flags = 0;

	if (picon_id)
		flags |= SHGFI_SYSICONINDEX;

	if (picon)
		flags |= SHGFI_ICON;

	if (is_small)
		flags |= SHGFI_SMALLICON;

	if (SHGetFileInfo (path, 0, &shfi, sizeof (shfi), flags))
	{
		if (picon_id)
			*picon_id = shfi.iIcon;

		if (picon && shfi.hIcon)
			*picon = shfi.hIcon;
	}
	else
	{
		if (picon_id)
			*picon_id = config.icon_id;

		if (picon)
			*picon = CopyIcon (is_small ? config.hicon_small : config.hicon_large);
	}

	return TRUE;
}

PR_STRING _app_getsignatureinfo (PITEM_APP ptr_app)
{
	if (_r_obj_isstringempty (ptr_app->real_path) || (ptr_app->type != DataAppRegular && ptr_app->type != DataAppService && ptr_app->type != DataAppUWP))
		return NULL;

	HANDLE hfile = NULL;
	PR_STRING signature_cache_string = NULL;
	PR_HASHSTORE hashstore = _r_obj_findhashtable (cache_signatures, ptr_app->app_hash);

	if (hashstore)
	{
		signature_cache_string = hashstore->value_string;

		goto CleanupExit;
	}
	else
	{
		hashstore = _app_addcachetable (cache_signatures, ptr_app->app_hash, NULL, 0);

		if (!hashstore)
			goto CleanupExit;

		hfile = CreateFile (ptr_app->real_path->buffer, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (!_r_fs_isvalidhandle (hfile))
			goto CleanupExit;

		WINTRUST_FILE_INFO file_info = {0};
		WINTRUST_DATA trust_data = {0};
		GUID WinTrustActionGenericVerifyV2 = WINTRUST_ACTION_GENERIC_VERIFY_V2;

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

		if (WinVerifyTrust ((HWND)INVALID_HANDLE_VALUE, &WinTrustActionGenericVerifyV2, &trust_data) == ERROR_SUCCESS)
		{
			PCRYPT_PROVIDER_DATA prov_data = WTHelperProvDataFromStateData (trust_data.hWVTStateData);

			if (prov_data)
			{
				PCRYPT_PROVIDER_SGNR prov_signer = WTHelperGetProvSignerFromChain (prov_data, 0, FALSE, 0);

				if (prov_signer)
				{
					CRYPT_PROVIDER_CERT* prov_cert = WTHelperGetProvCertFromChain (prov_signer, 0);

					if (prov_cert)
					{
						ULONG num_chars = CertGetNameString (prov_cert->pCert, CERT_NAME_ATTR_TYPE, 0, szOID_COMMON_NAME, NULL, 0) - 1;

						if (num_chars)
						{
							signature_cache_string = _r_obj_createstringex (NULL, num_chars * sizeof (WCHAR));

							if (CertGetNameString (prov_cert->pCert, CERT_NAME_ATTR_TYPE, 0, szOID_COMMON_NAME, signature_cache_string->buffer, num_chars + 1))
							{
								// _app_freestrings_map (&cache_signatures, MAP_CACHE_MAX);

								hashstore->value_string = signature_cache_string;
							}
							else
							{
								_r_obj_clearreference (&signature_cache_string);
							}
						}
					}
				}
			}
		}

		trust_data.dwStateAction = WTD_STATEACTION_CLOSE;
		WinVerifyTrust ((HWND)INVALID_HANDLE_VALUE, &WinTrustActionGenericVerifyV2, &trust_data);
	}

CleanupExit:

	if (_r_fs_isvalidhandle (hfile))
		CloseHandle (hfile);

	ptr_app->is_signed = !_r_obj_isstringempty (signature_cache_string);

	return _r_obj_referencesafe (signature_cache_string);
}

PR_STRING _app_getversioninfo (PITEM_APP ptr_app)
{
	if (_r_obj_isstringempty (ptr_app->real_path))
		return NULL;

	HINSTANCE hlib = NULL;
	HRSRC hres = NULL;
	HGLOBAL hglob = NULL;
	PVOID version_info = NULL;
	R_STRINGBUILDER version_cache = {0};
	PR_STRING version_cache_string = NULL;
	PR_HASHSTORE hashstore = _r_obj_findhashtable (cache_versions, ptr_app->app_hash);

	if (hashstore)
	{
		version_cache_string = hashstore->value_string;

		goto CleanupExit;
	}

	hashstore = _app_addcachetable (cache_versions, ptr_app->app_hash, NULL, 0);

	if (!hashstore)
		goto CleanupExit;

	hlib = LoadLibraryEx (ptr_app->real_path->buffer, NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE);

	if (!hlib)
		goto CleanupExit;

	hres = FindResource (hlib, MAKEINTRESOURCE (VS_VERSION_INFO), RT_VERSION);

	if (!hres)
		goto CleanupExit;

	hglob = LoadResource (hlib, hres);

	if (!hglob)
		goto CleanupExit;

	version_info = LockResource (hglob);

	if (version_info)
	{
		_r_obj_initializestringbuilder (&version_cache);

		PVOID buffer;
		ULONG lang_id;
		UINT length;

		WCHAR author_entry[128];
		WCHAR description_entry[128];

		if (VerQueryValue (version_info, L"\\VarFileInfo\\Translation", &buffer, &length) && length == 4)
		{
			memcpy (&lang_id, buffer, length);

			_r_str_printf (author_entry, RTL_NUMBER_OF (author_entry), L"\\StringFileInfo\\%02X%02X%02X%02X\\CompanyName", (lang_id & 0xff00) >> 8, lang_id & 0xff, (lang_id & 0xff000000) >> 24, (lang_id & 0xff0000) >> 16);
			_r_str_printf (description_entry, RTL_NUMBER_OF (description_entry), L"\\StringFileInfo\\%02X%02X%02X%02X\\FileDescription", (lang_id & 0xff00) >> 8, lang_id & 0xff, (lang_id & 0xff000000) >> 24, (lang_id & 0xff0000) >> 16);
		}
		else
		{
			_r_str_printf (author_entry, RTL_NUMBER_OF (author_entry), L"\\StringFileInfo\\%04X04B0\\CompanyName", GetUserDefaultLangID ());
			_r_str_printf (description_entry, RTL_NUMBER_OF (description_entry), L"\\StringFileInfo\\%04X04B0\\FileDescription", GetUserDefaultLangID ());
		}

		if (VerQueryValue (version_info, description_entry, &buffer, &length))
		{
			_r_obj_appendstringbuilderformat (&version_cache, SZ_TAB L"%s", buffer);

			VS_FIXEDFILEINFO* ver_info;

			if (VerQueryValue (version_info, L"\\", &ver_info, &length))
			{
				_r_obj_appendstringbuilderformat (&version_cache, L" %d.%d", HIWORD (ver_info->dwFileVersionMS), LOWORD (ver_info->dwFileVersionMS));

				if (HIWORD (ver_info->dwFileVersionLS) || LOWORD (ver_info->dwFileVersionLS))
				{
					_r_obj_appendstringbuilderformat (&version_cache, L".%d", HIWORD (ver_info->dwFileVersionLS));

					if (LOWORD (ver_info->dwFileVersionLS))
						_r_obj_appendstringbuilderformat (&version_cache, L".%d", LOWORD (ver_info->dwFileVersionLS));
				}
			}

			_r_obj_appendstringbuilder (&version_cache, L"\r\n");
		}

		if (VerQueryValue (version_info, author_entry, &buffer, &length))
		{
			_r_obj_appendstringbuilderformat (&version_cache, SZ_TAB L"%s\r\n", buffer);
		}

		version_cache_string = _r_obj_finalstringbuilder (&version_cache);

		_r_obj_trimstring (version_cache_string, DIVIDER_TRIM);

		if (_r_obj_isstringempty (version_cache_string))
		{
			_r_obj_deletestringbuilder (&version_cache);

			goto CleanupExit;
		}

		//_app_freestrings_map (&cache_versions, MAP_CACHE_MAX);

		hashstore->value_string = version_cache_string;
	}

CleanupExit:

	if (hglob)
		FreeResource (hglob);

	if (hlib)
		FreeLibrary (hlib);

	return _r_obj_referencesafe (version_cache_string);
}

LPCWSTR _app_getservicename (UINT16 port, LPCWSTR default_value)
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
			return L"https";

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

LPCWSTR _app_getprotoname (UINT8 proto, ADDRESS_FAMILY af, LPCWSTR default_value)
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

LPCWSTR _app_getconnectionstatusname (ULONG state, LPCWSTR default_value)
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

	return default_value;
}

PR_STRING _app_getdirectionname (FWP_DIRECTION direction, BOOLEAN is_loopback, BOOLEAN is_localized)
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
		return _r_format_string (L"%s (" SZ_DIRECTION_LOOPBACK L")", text);

	return _r_obj_createstring (text);
}

COLORREF _app_getcolorvalue (SIZE_T color_hash)
{
	if (!color_hash)
		return 0;

	for (SIZE_T i = 0; i < _r_obj_getarraysize (colors); i++)
	{
		PITEM_COLOR ptr_clr = _r_obj_getarrayitem (colors, i);

		if (ptr_clr && ptr_clr->hash == color_hash)
			return ptr_clr->new_clr ? ptr_clr->new_clr : ptr_clr->default_clr;
	}

	return 0;
}

PR_STRING _app_getservicenamefromtag (HANDLE hprocess, LPCVOID service_tag)
{
	PR_STRING service_name_string = NULL;
	HMODULE hlib = GetModuleHandle (L"advapi32.dll");

	if (hlib)
	{
		typedef ULONG (NTAPI* IQTI) (PVOID, SC_SERVICE_TAG_QUERY_TYPE, PSC_SERVICE_TAG_QUERY); // I_QueryTagInformation
		const IQTI _I_QueryTagInformation = (IQTI)GetProcAddress (hlib, "I_QueryTagInformation");

		if (_I_QueryTagInformation)
		{
			PSC_SERVICE_TAG_QUERY service_tag_query = _r_mem_allocatezero (sizeof (SC_SERVICE_TAG_QUERY));

			service_tag_query->ProcessId = HandleToUlong (hprocess);
			service_tag_query->ServiceTag = PtrToUlong (service_tag);

			_I_QueryTagInformation (NULL, ServiceNameFromTagInformation, service_tag_query);

			if (service_tag_query->Buffer)
			{
				service_name_string = _r_obj_createstring ((LPCWSTR)service_tag_query->Buffer);

				LocalFree (service_tag_query->Buffer);
			}

			_r_mem_free (service_tag_query);
		}
	}

	return service_name_string;
}

BOOLEAN _app_isimmersiveprocess (HANDLE hprocess)
{
	HMODULE huser32 = GetModuleHandle (L"user32.dll");

	if (huser32)
	{
		typedef BOOL (WINAPI* IIP) (HANDLE); // IsImmersiveProcess
		const IIP _IsImmersiveProcess = (IIP)GetProcAddress (huser32, "IsImmersiveProcess");

		if (_IsImmersiveProcess)
			return !!_IsImmersiveProcess (hprocess);
	}

	return FALSE;
}

PR_STRING _app_getnetworkpath (ULONG pid, PULONG64 pmodules, PITEM_NETWORK ptr_network)
{
	if (pid == PROC_WAITING_PID)
	{
		ptr_network->app_hash = 0;
		ptr_network->icon_id = config.icon_id;
		ptr_network->type = DataAppRegular;

		return _r_obj_createstring (PROC_WAITING_NAME);
	}
	else if (pid == PROC_SYSTEM_PID)
	{
		ptr_network->app_hash = config.ntoskrnl_hash;
		ptr_network->icon_id = config.icon_id;
		ptr_network->type = DataAppRegular;

		return _r_obj_createstring (PROC_SYSTEM_NAME);
	}

	PR_STRING process_name = NULL;

	if (pmodules)
	{
		PR_STRING service_name = _app_getservicenamefromtag (UlongToHandle (pid), UlongToPtr (*(PULONG)pmodules));

		if (service_name)
		{
			_r_obj_movereference (&process_name, service_name);

			ptr_network->type = DataAppService;
		}
	}

	if (!process_name)
	{
		HANDLE hprocess = OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);

		if (hprocess)
		{
			if (_r_sys_isosversiongreaterorequal (WINDOWS_8) && _app_isimmersiveprocess (hprocess))
			{
				ptr_network->type = DataAppUWP;
			}
			else
			{
				ptr_network->type = DataAppRegular;
			}

			ULONG size = 0x400;
			process_name = _r_obj_createstringex (NULL, size * sizeof (WCHAR));

			BOOL is_success = QueryFullProcessImageName (hprocess, 0, process_name->buffer, &size);

			// fix for WSL processes (issue #606)
			if (!is_success)
			{
				if (GetLastError () == ERROR_GEN_FAILURE)
				{
					size = 0x400;
					QueryFullProcessImageName (hprocess, PROCESS_NAME_NATIVE, process_name->buffer, &size);

					is_success = TRUE;
				}
			}

			if (!is_success)
			{
				SAFE_DELETE_REFERENCE (process_name);
			}
			else
			{
				_r_obj_trimstringtonullterminator (process_name);
			}

			CloseHandle (hprocess);
		}
	}

	if (!_r_obj_isstringempty (process_name))
	{
		SIZE_T app_hash = _r_obj_getstringhash (process_name);

		ptr_network->app_hash = app_hash;
		ptr_network->icon_id = PtrToInt (_app_getappinfobyhash (app_hash, InfoIconId));

		if (!ptr_network->icon_id)
			_app_getfileicon (process_name->buffer, TRUE, &ptr_network->icon_id, NULL);
	}
	else
	{
		ptr_network->app_hash = 0;
		ptr_network->icon_id = config.icon_id;
	}

	return process_name;
}

SIZE_T _app_getnetworkhash (ADDRESS_FAMILY af, ULONG pid, LPCVOID remote_addr, ULONG remote_port, LPCVOID local_addr, ULONG local_port, UINT8 proto, ULONG state)
{
	WCHAR remote_address[LEN_IP_MAX] = {0};
	WCHAR local_address[LEN_IP_MAX] = {0};
	PR_STRING network_string;
	SIZE_T network_hash;

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

BOOLEAN _app_isvalidconnection (ADDRESS_FAMILY af, LPCVOID paddr)
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

VOID _app_generate_connections (PR_HASHTABLE checker_map)
{
	_r_obj_clearhashtable (checker_map);

	ITEM_NETWORK network;

	ULONG allocated_size = 0x4000;
	PVOID network_table = _r_mem_allocatezero (allocated_size);

	ULONG required_size = 0;
	GetExtendedTcpTable (NULL, &required_size, FALSE, AF_INET, TCP_TABLE_OWNER_MODULE_ALL, 0);

	if (required_size)
	{
		if (allocated_size < required_size)
		{
			network_table = _r_mem_reallocatezero (network_table, required_size);
			allocated_size = required_size;
		}

		PMIB_TCPTABLE_OWNER_MODULE tcp4_table = network_table;

		if (GetExtendedTcpTable (tcp4_table, &required_size, FALSE, AF_INET, TCP_TABLE_OWNER_MODULE_ALL, 0) == NO_ERROR)
		{
			for (ULONG i = 0; i < tcp4_table->dwNumEntries; i++)
			{
				IN_ADDR remote_addr = {0};
				IN_ADDR local_addr = {0};

				remote_addr.S_un.S_addr = tcp4_table->table[i].dwRemoteAddr;
				local_addr.S_un.S_addr = tcp4_table->table[i].dwLocalAddr;

				memset (&network, 0, sizeof (network));

				network.network_hash = _app_getnetworkhash (AF_INET, tcp4_table->table[i].dwOwningPid, &remote_addr, tcp4_table->table[i].dwRemotePort, &local_addr, tcp4_table->table[i].dwLocalPort, IPPROTO_TCP, tcp4_table->table[i].dwState);

				if (_r_obj_findhashtable (network_map, network.network_hash))
				{
					_app_addcachetable (checker_map, network.network_hash, NULL, 0);

					continue;
				}

				network.path = _app_getnetworkpath (tcp4_table->table[i].dwOwningPid, tcp4_table->table[i].OwningModuleInfo, &network);

				if (!network.path)
					continue;

				network.af = AF_INET;
				network.protocol = IPPROTO_TCP;

				network.remote_addr.S_un.S_addr = tcp4_table->table[i].dwRemoteAddr;
				network.remote_port = _r_byteswap_ushort ((USHORT)tcp4_table->table[i].dwRemotePort);

				network.local_addr.S_un.S_addr = tcp4_table->table[i].dwLocalAddr;
				network.local_port = _r_byteswap_ushort ((USHORT)tcp4_table->table[i].dwLocalPort);

				network.state = tcp4_table->table[i].dwState;

				if (tcp4_table->table[i].dwState == MIB_TCP_STATE_ESTAB)
				{
					if (_app_isvalidconnection (network.af, &network.remote_addr) || _app_isvalidconnection (network.af, &network.local_addr))
						network.is_connection = TRUE;
				}

				_r_obj_addhashtableitem (network_map, network.network_hash, &network);

				_app_addcachetable (checker_map, network.network_hash, NULL, 1);
			}
		}
	}

	required_size = 0;
	GetExtendedTcpTable (NULL, &required_size, FALSE, AF_INET6, TCP_TABLE_OWNER_MODULE_ALL, 0);

	if (required_size)
	{
		if (allocated_size < required_size)
		{
			network_table = _r_mem_reallocatezero (network_table, required_size);
			allocated_size = required_size;
		}

		PMIB_TCP6TABLE_OWNER_MODULE tcp6_table = network_table;

		if (GetExtendedTcpTable (tcp6_table, &required_size, FALSE, AF_INET6, TCP_TABLE_OWNER_MODULE_ALL, 0) == NO_ERROR)
		{
			for (ULONG i = 0; i < tcp6_table->dwNumEntries; i++)
			{
				memset (&network, 0, sizeof (network));

				network.network_hash = _app_getnetworkhash (AF_INET6, tcp6_table->table[i].dwOwningPid, tcp6_table->table[i].ucRemoteAddr, tcp6_table->table[i].dwRemotePort, tcp6_table->table[i].ucLocalAddr, tcp6_table->table[i].dwLocalPort, IPPROTO_TCP, tcp6_table->table[i].dwState);

				if (_r_obj_findhashtable (network_map, network.network_hash))
				{
					_app_addcachetable (checker_map, network.network_hash, NULL, 0);

					continue;
				}

				network.path = _app_getnetworkpath (tcp6_table->table[i].dwOwningPid, tcp6_table->table[i].OwningModuleInfo, &network);

				if (!network.path)
					continue;

				network.af = AF_INET6;
				network.protocol = IPPROTO_TCP;

				memcpy (network.remote_addr6.u.Byte, tcp6_table->table[i].ucRemoteAddr, FWP_V6_ADDR_SIZE);
				network.remote_port = _r_byteswap_ushort ((USHORT)tcp6_table->table[i].dwRemotePort);

				memcpy (network.local_addr6.u.Byte, tcp6_table->table[i].ucLocalAddr, FWP_V6_ADDR_SIZE);
				network.local_port = _r_byteswap_ushort ((USHORT)tcp6_table->table[i].dwLocalPort);

				network.state = tcp6_table->table[i].dwState;

				if (tcp6_table->table[i].dwState == MIB_TCP_STATE_ESTAB)
				{
					if (_app_isvalidconnection (network.af, &network.remote_addr6) || _app_isvalidconnection (network.af, &network.local_addr6))
						network.is_connection = TRUE;
				}

				_r_obj_addhashtableitem (network_map, network.network_hash, &network);

				_app_addcachetable (checker_map, network.network_hash, NULL, 1);
			}
		}
	}

	required_size = 0;
	GetExtendedUdpTable (NULL, &required_size, FALSE, AF_INET, UDP_TABLE_OWNER_MODULE, 0);

	if (required_size)
	{
		if (allocated_size < required_size)
		{
			network_table = _r_mem_reallocatezero (network_table, required_size);
			allocated_size = required_size;
		}

		PMIB_UDPTABLE_OWNER_MODULE udp4_table = network_table;

		if (GetExtendedUdpTable (udp4_table, &required_size, FALSE, AF_INET, UDP_TABLE_OWNER_MODULE, 0) == NO_ERROR)
		{
			for (ULONG i = 0; i < udp4_table->dwNumEntries; i++)
			{
				IN_ADDR local_addr = {0};
				local_addr.S_un.S_addr = udp4_table->table[i].dwLocalAddr;

				memset (&network, 0, sizeof (network));

				network.network_hash = _app_getnetworkhash (AF_INET, udp4_table->table[i].dwOwningPid, NULL, 0, &local_addr, udp4_table->table[i].dwLocalPort, IPPROTO_UDP, 0);

				if (_r_obj_findhashtable (network_map, network.network_hash))
				{
					_app_addcachetable (checker_map, network.network_hash, NULL, 0);

					continue;
				}

				network.path = _app_getnetworkpath (udp4_table->table[i].dwOwningPid, udp4_table->table[i].OwningModuleInfo, &network);

				if (!network.path)
					continue;

				network.af = AF_INET;
				network.protocol = IPPROTO_UDP;

				network.local_addr.S_un.S_addr = udp4_table->table[i].dwLocalAddr;
				network.local_port = _r_byteswap_ushort ((USHORT)udp4_table->table[i].dwLocalPort);

				if (_app_isvalidconnection (network.af, &network.local_addr))
					network.is_connection = TRUE;

				_r_obj_addhashtableitem (network_map, network.network_hash, &network);

				_app_addcachetable (checker_map, network.network_hash, NULL, 1);
			}
		}
	}

	required_size = 0;
	GetExtendedUdpTable (NULL, &required_size, FALSE, AF_INET6, UDP_TABLE_OWNER_MODULE, 0);

	if (required_size)
	{
		if (allocated_size < required_size)
		{
			network_table = _r_mem_reallocatezero (network_table, required_size);
			allocated_size = required_size;
		}

		PMIB_UDP6TABLE_OWNER_MODULE udp6_table = network_table;

		if (GetExtendedUdpTable (udp6_table, &required_size, FALSE, AF_INET6, UDP_TABLE_OWNER_MODULE, 0) == NO_ERROR)
		{
			for (ULONG i = 0; i < udp6_table->dwNumEntries; i++)
			{
				memset (&network, 0, sizeof (network));

				network.network_hash = _app_getnetworkhash (AF_INET6, udp6_table->table[i].dwOwningPid, NULL, 0, udp6_table->table[i].ucLocalAddr, udp6_table->table[i].dwLocalPort, IPPROTO_UDP, 0);

				if (_r_obj_findhashtable (network_map, network.network_hash))
				{
					_app_addcachetable (checker_map, network.network_hash, NULL, 0);

					continue;
				}

				network.path = _app_getnetworkpath (udp6_table->table[i].dwOwningPid, udp6_table->table[i].OwningModuleInfo, &network);

				if (!network.path)
					continue;

				network.af = AF_INET6;
				network.protocol = IPPROTO_UDP;

				memcpy (network.local_addr6.u.Byte, udp6_table->table[i].ucLocalAddr, FWP_V6_ADDR_SIZE);
				network.local_port = _r_byteswap_ushort ((USHORT)udp6_table->table[i].dwLocalPort);

				if (_app_isvalidconnection (network.af, &network.local_addr6))
					network.is_connection = TRUE;

				_r_obj_addhashtableitem (network_map, network.network_hash, &network);

				_app_addcachetable (checker_map, network.network_hash, NULL, 1);
			}
		}
	}

	if (network_table)
		_r_mem_free (network_table);
}

VOID _app_generate_packages ()
{
	PR_BYTE package_sid;
	PR_STRING package_sid_string;
	PR_STRING key_name;
	PR_STRING display_name;
	PR_STRING real_path;
	PITEM_APP ptr_app;
	SIZE_T app_hash;
	HKEY hkey;
	HKEY hsubkey;
	ULONG key_index;
	ULONG max_length;
	ULONG size;
	LSTATUS code;

	code = RegOpenKeyEx (HKEY_CURRENT_USER, L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages", 0, KEY_READ, &hkey);

	if (code == ERROR_SUCCESS)
	{
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
					package_sid = _r_reg_querybinary (hsubkey, L"PackageSid");

					if (package_sid)
					{
						if (RtlValidSid (package_sid->buffer))
						{
							package_sid_string = _r_str_fromsid (package_sid->buffer);

							if (package_sid_string)
							{
								app_hash = _r_obj_getstringhash (package_sid_string);

								if (!_r_obj_findhashtable (apps, app_hash))
								{
									display_name = _r_reg_querystring (hsubkey, L"DisplayName");

									if (display_name)
									{
										if (display_name->buffer[0] == L'@')
										{
											UINT localized_length = 0x200;
											PR_STRING localized_name = _r_obj_createstringex (NULL, localized_length * sizeof (WCHAR));

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

										// use registry key name as fallback package name
										if (_r_obj_isstringempty (display_name))
											_r_obj_movereference (&display_name, _r_obj_reference (key_name));

										real_path = _r_reg_querystring (hsubkey, L"PackageRootFolder");

										ptr_app = _app_addapplication (NULL, DataAppUWP, package_sid_string->buffer, display_name, real_path);

										if (ptr_app)
										{
											LONG64 timestamp = _r_reg_querytimestamp (hsubkey);

											_app_setappinfo (ptr_app, InfoTimestampPtr, &timestamp);

											_r_obj_movereference (&ptr_app->pbytes, _r_obj_reference (package_sid));
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
}

VOID _app_generate_services ()
{
	SC_HANDLE hsvcmgr = OpenSCManager (NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE);

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

	if (!EnumServicesStatusEx (hsvcmgr, SC_ENUM_PROCESS_INFO, service_type, service_state, (PBYTE)buffer, buffer_size, &return_length, &services_returned, NULL, NULL))
	{
		if (GetLastError () == ERROR_MORE_DATA)
		{
			// Set the buffer
			buffer_size += return_length;
			buffer = _r_mem_reallocatezero (buffer, buffer_size);

			// Now query again for services
			if (!EnumServicesStatusEx (hsvcmgr, SC_ENUM_PROCESS_INFO, service_type, service_state, (PBYTE)buffer, buffer_size, &return_length, &services_returned, NULL, NULL))
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
		LPQUERY_SERVICE_CONFIG svconfig = NULL;
		LPENUM_SERVICE_STATUS_PROCESS service;
		LPENUM_SERVICE_STATUS_PROCESS services;
		PITEM_APP ptr_app;
		SIZE_T app_hash;
		ULONG required_length;

		services = (LPENUM_SERVICE_STATUS_PROCESS)buffer;

		if (_r_sys_isosversiongreaterorequal (WINDOWS_10))
			svconfig = _r_mem_allocatezero (0x1000);

		for (ULONG i = 0; i < services_returned; i++)
		{
			service = &services[i];

			LPCWSTR service_name = service->lpServiceName;
			LPCWSTR display_name = service->lpDisplayName;

			if (svconfig && _r_sys_isosversiongreaterorequal (WINDOWS_10))
			{
				SC_HANDLE svc = OpenService (hsvcmgr, service_name, SERVICE_QUERY_CONFIG);

				if (svc)
				{
					if (QueryServiceConfig (svc, svconfig, 0x1000, &required_length))
					{
						if ((svconfig->dwServiceType & SERVICE_USERSERVICE_INSTANCE) != 0)
						{
							CloseServiceHandle (svc);
							continue;
						}
					}

					CloseServiceHandle (svc);
				}
			}

			app_hash = _r_str_hash (service_name, _r_str_length (service_name));

			if (_r_obj_findhashtable (apps, app_hash))
				continue;

			PR_STRING service_path = NULL;
			LONG64 service_timestamp = 0;

			HKEY hkey;

			WCHAR general_key[256];
			WCHAR parameters_key[256];

			_r_str_printf (general_key, RTL_NUMBER_OF (general_key), L"System\\CurrentControlSet\\Services\\%s", service_name);
			_r_str_printf (parameters_key, RTL_NUMBER_OF (parameters_key), L"System\\CurrentControlSet\\Services\\%s\\Parameters", service_name);

			// query "ServiceDll" path
			if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, parameters_key, 0, KEY_READ, &hkey) == ERROR_SUCCESS)
			{
				// query path
				service_path = _r_reg_querystring (hkey, L"ServiceDll");

				// query timestamp
				service_timestamp = _r_reg_querytimestamp (hkey);

				RegCloseKey (hkey);
			}

			// fallback
			if (!service_path || !service_timestamp)
			{
				if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, general_key, 0, KEY_READ, &hkey) == ERROR_SUCCESS)
				{
					// query path
					if (!service_path)
						service_path = _r_reg_querystring (hkey, L"ImagePath");

					// query timestamp
					if (!service_timestamp)
						service_timestamp = _r_reg_querytimestamp (hkey);

					RegCloseKey (hkey);
				}
			}

			if (!_r_obj_isstringempty (service_path))
			{
				PathRemoveArgs (service_path->buffer);
				PathUnquoteSpaces (service_path->buffer);

				_r_obj_trimstringtonullterminator (service_path);

				PR_STRING converted_path = _r_path_dospathfromnt (service_path->buffer);

				if (converted_path)
					_r_obj_movereference (&service_path, converted_path);
			}

			PSID service_sid = _app_queryservicesid (service_name);

			if (service_sid)
			{
				PVOID service_sd = NULL;
				ULONG sd_length = 0;

				EXPLICIT_ACCESS ea;
				memset (&ea, 0, sizeof (ea));

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
				if (BuildSecurityDescriptor (NULL, NULL, 1, &ea, 0, NULL, NULL, &sd_length, &service_sd) != ERROR_SUCCESS)
				{
					SAFE_DELETE_REFERENCE (service_path);
					continue;
				}

				PR_STRING name_string = _r_obj_createstring (display_name);

				ptr_app = _app_addapplication (NULL, DataAppService, service_name, name_string, service_path);

				if (ptr_app)
				{
					_app_setappinfo (ptr_app, InfoTimestampPtr, &service_timestamp);

					_r_obj_movereference (&ptr_app->pbytes, _r_obj_createbyteex (NULL, sd_length));

					memcpy (ptr_app->pbytes->buffer, service_sd, sd_length);
				}

				_r_obj_dereference (name_string);

				SAFE_DELETE_LOCAL (service_sd);

				_r_mem_free (service_sid);
			}

			SAFE_DELETE_REFERENCE (service_path);
		}

		if (svconfig)
			_r_mem_free (svconfig);

		_r_mem_free (buffer);
	}

	CloseServiceHandle (hsvcmgr);
}

VOID _app_generate_timerscontrol (PVOID hwnd, INT ctrl_id, PITEM_APP ptr_app)
{
	WCHAR interval_string[128];
	LONG64 current_time;
	PVOID timer_ptr;
	LONG64 app_time = 0;
	UINT index;
	BOOLEAN is_checked = (ptr_app == NULL);
	BOOLEAN is_menu = (ctrl_id == 0);

	current_time = _r_unixtime_now ();

	if (ptr_app)
	{
		timer_ptr = _app_getappinfo (ptr_app, InfoTimerPtr);
		app_time = timer_ptr ? *((PLONG64)timer_ptr) : 0;
	}

	if (!is_menu)
	{
		SendDlgItemMessage (hwnd, ctrl_id, CB_INSERTSTRING, 0, (LPARAM)_r_locale_getstring (IDS_DISABLETIMER));
		SendDlgItemMessage (hwnd, ctrl_id, CB_SETITEMDATA, 0, (LPARAM)-1);
	}

	for (SIZE_T i = 0; i < _r_obj_getarraysize (timers); i++)
	{
		timer_ptr = _r_obj_getarrayitem (timers, i);

		if (!timer_ptr)
			continue;

		LONG64 timestamp = *((PLONG64)timer_ptr);

		_r_format_interval (interval_string, RTL_NUMBER_OF (interval_string), timestamp + 1, 1);

		if (is_menu)
		{
			index = IDX_TIMER + (UINT)i;

			AppendMenu (hwnd, MF_STRING, index, interval_string);

			if (!is_checked && (app_time > current_time) && (app_time <= (current_time + timestamp)))
			{
				_r_menu_checkitem (hwnd, IDX_TIMER, index, MF_BYCOMMAND, index);
				is_checked = TRUE;
			}
		}
		else
		{
			index = (UINT)i + 1;

			SendDlgItemMessage (hwnd, ctrl_id, CB_INSERTSTRING, (WPARAM)index, (LPARAM)interval_string);
			SendDlgItemMessage (hwnd, ctrl_id, CB_SETITEMDATA, (WPARAM)index, (LPARAM)i);

			if (!is_checked && (app_time > current_time) && (app_time <= (current_time + timestamp)))
			{
				SendDlgItemMessage (hwnd, IDC_RULE_PROTOCOL_ID, CB_SETCURSEL, (WPARAM)index, 0);
				is_checked = TRUE;
			}
		}
	}

	if (!is_checked)
	{
		if (is_menu)
		{
			_r_menu_checkitem (hwnd, IDM_DISABLETIMER, IDM_DISABLETIMER, MF_BYCOMMAND, IDM_DISABLETIMER);
		}
		else
		{
			SendDlgItemMessage (hwnd, ctrl_id, CB_SETCURSEL, 0, 0);
		}
	}
}

PR_STRING _app_parsehostaddress_dns (LPCWSTR hostname, USHORT port)
{
	if (_r_str_isempty (hostname))
		return NULL;

	R_STRINGBUILDER buffer;
	PR_STRING string;
	PDNS_RECORD dns_records = NULL;

	_r_obj_initializestringbuilder (&buffer);

	// ipv4 address
	DNS_STATUS code = DnsQuery (hostname, DNS_TYPE_A, DNS_QUERY_NO_HOSTS_FILE, NULL, &dns_records, NULL);

	if (code != DNS_ERROR_RCODE_NO_ERROR)
	{
		if (code != DNS_INFO_NO_RECORDS)
			_r_log (Information, 0, L"DnsQuery (DNS_TYPE_A)", code, hostname);
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

					_r_obj_appendstringbuilder (&buffer, DIVIDER_RULE);
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
			_r_log (Information, 0, L"DnsQuery (DNS_TYPE_AAAA)", code, hostname);
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

					_r_obj_appendstringbuilder (&buffer, DIVIDER_RULE);
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

	_r_obj_trimstring (string, DIVIDER_RULE);

	if (!_r_obj_isstringempty (string))
		return string;

	_r_obj_deletestringbuilder (&buffer);

	return NULL;
}

BOOLEAN _app_parsenetworkstring (LPCWSTR network_string, NET_ADDRESS_FORMAT* format_ptr, PUSHORT port_ptr, FWP_V4_ADDR_AND_MASK* paddr4, FWP_V6_ADDR_AND_MASK* paddr6, LPWSTR dns_string, SIZE_T dns_length)
{
	NET_ADDRESS_INFO ni;
	memset (&ni, 0, sizeof (ni));

	USHORT port;
	BYTE prefix_length;

	ULONG types = NET_STRING_ANY_ADDRESS | NET_STRING_ANY_SERVICE | NET_STRING_IP_NETWORK | NET_STRING_ANY_ADDRESS_NO_SCOPE | NET_STRING_ANY_SERVICE_NO_SCOPE;
	ULONG code = ParseNetworkString (network_string, types, &ni, &port, &prefix_length);

	if (code != ERROR_SUCCESS)
	{
		_r_log (Warning, 0, L"ParseNetworkString", code, network_string);
		return FALSE;
	}
	else
	{
		if (format_ptr)
			*format_ptr = ni.Format;

		if (port_ptr)
			*port_ptr = port;

		if (ni.Format == NET_ADDRESS_IPV4)
		{
			if (paddr4)
			{
				ULONG mask = 0;
				ConvertLengthToIpv4Mask (prefix_length, &mask);

				paddr4->addr = _r_byteswap_ulong (ni.Ipv4Address.sin_addr.S_un.S_addr);
				paddr4->mask = _r_byteswap_ulong (mask);
			}

			return TRUE;
		}
		else if (ni.Format == NET_ADDRESS_IPV6)
		{
			if (paddr6)
			{
				memcpy (paddr6->addr, ni.Ipv6Address.sin6_addr.u.Byte, FWP_V6_ADDR_SIZE);
				paddr6->prefixLength = min (prefix_length, 128);
			}

			return TRUE;
		}
		else if (ni.Format == NET_ADDRESS_DNS_NAME)
		{
			if (dns_string)
			{
				SIZE_T dns_hash = _r_str_hash (ni.NamedAddress.Address, _r_str_length (ni.NamedAddress.Address));
				PR_HASHSTORE hashstore = _r_obj_findhashtable (cache_dns, dns_hash);

				if (hashstore)
				{
					if (hashstore->value_string)
					{
						_r_str_copy (dns_string, dns_length, hashstore->value_string->buffer);

						return TRUE;
					}
				}

				PR_STRING host_string = _app_parsehostaddress_dns (ni.NamedAddress.Address, port);

				if (host_string)
				{
					_r_str_copy (dns_string, dns_length, host_string->buffer);

					//_app_freestrings_map (&cache_dns, MAP_CACHE_MAX);

					_app_addcachetable (cache_dns, dns_hash, host_string, 0);

					return TRUE;
				}

				return FALSE;
			}

			return TRUE;
		}
	}

	return FALSE;
}

BOOLEAN _app_parserulestring (PR_STRING rule, PITEM_ADDRESS ptr_addr)
{
	if (_r_obj_isstringempty (rule))
		return TRUE;

	ENUM_TYPE_DATA type = DataUnknown;

	SIZE_T rule_length = _r_obj_getstringlength (rule);
	SIZE_T range_pos = _r_str_findchar (rule->buffer, rule_length, DIVIDER_RULE_RANGE);
	BOOLEAN is_range = (range_pos != SIZE_MAX);

	WCHAR range_start[LEN_IP_MAX] = {0};
	WCHAR range_end[LEN_IP_MAX] = {0};

	if (is_range)
	{
		PR_STRING range_start_string = _r_str_extract (rule, 0, range_pos);
		PR_STRING range_end_string = _r_str_extract (rule, range_pos + 1, rule_length - range_pos - 1);

		if (range_start_string)
		{
			_r_str_copy (range_start, RTL_NUMBER_OF (range_start), range_start_string->buffer);
			_r_obj_dereference (range_start_string);
		}

		if (range_end_string)
		{
			_r_str_copy (range_end, RTL_NUMBER_OF (range_end), range_end_string->buffer);
			_r_obj_dereference (range_end_string);
		}

		if (_r_str_isempty (range_start) || _r_str_isempty (range_end))
			return FALSE;
	}

	// auto-parse rule type
	{
		SIZE_T rule_hash = _r_obj_getstringhash (rule);
		PR_HASHSTORE hashstore = _r_obj_findhashtable (cache_types, rule_hash);

		if (hashstore)
		{
			type = hashstore->value_number;
		}
		else
		{
			if (_app_isrulevalid (rule->buffer, rule_length))
			{
				if (_app_isruleport (rule->buffer, rule_length))
				{
					type = DataTypePort;
				}
				else if (!is_range && _app_isruletype (rule->buffer, TULE_TYPE_IP))
				{
					type = DataTypeIp;
				}
				else if (is_range && _app_isruletype (range_start, TULE_TYPE_IP) && _app_isruletype (range_end, TULE_TYPE_IP))
				{
					type = DataTypeIp;
				}
				else if (_app_isruletype (rule->buffer, TULE_TYPE_HOST))
				{
					type = DataTypeHost;
				}
			}

			if (_r_obj_gethashtablecount (cache_types) >= MAP_CACHE_MAX)
				_r_obj_clearhashtable (cache_types);

			_app_addcachetable (cache_types, rule_hash, NULL, type);
		}
	}

	if (type == DataUnknown)
		return FALSE;

	if (!ptr_addr)
		return TRUE;

	if (type == DataTypeHost)
		is_range = FALSE;

	ptr_addr->is_range = is_range;

	if (type == DataTypePort)
	{
		if (!is_range)
		{
			// ...port
			ptr_addr->type = DataTypePort;
			ptr_addr->port = (UINT16)_r_str_touinteger (rule->buffer);

			return TRUE;
		}
		else
		{
			// ...port range
			ptr_addr->type = DataTypePort;

			ptr_addr->range.valueLow.type = FWP_UINT16;
			ptr_addr->range.valueLow.uint16 = (UINT16)wcstoul (range_start, NULL, 10);

			ptr_addr->range.valueHigh.type = FWP_UINT16;
			ptr_addr->range.valueHigh.uint16 = (UINT16)wcstoul (range_end, NULL, 10);

			return TRUE;
		}
	}
	else
	{
		NET_ADDRESS_FORMAT format;

		FWP_V4_ADDR_AND_MASK addr4 = {0};
		FWP_V6_ADDR_AND_MASK addr6 = {0};

		USHORT port2 = 0;

		if (type == DataTypeIp && is_range)
		{
			// ...ip range (start)
			if (_app_parsenetworkstring (range_start, &format, &port2, &addr4, &addr6, NULL, 0))
			{
				if (format == NET_ADDRESS_IPV4)
				{
					ptr_addr->range.valueLow.type = FWP_UINT32;
					ptr_addr->range.valueLow.uint32 = addr4.addr;
				}
				else if (format == NET_ADDRESS_IPV6)
				{
					ptr_addr->range.valueLow.type = FWP_BYTE_ARRAY16_TYPE;
					memcpy (ptr_addr->range.valueLow.byteArray16->byteArray16, addr6.addr, FWP_V6_ADDR_SIZE);
				}
				else
				{
					return FALSE;
				}

				if (port2 && !ptr_addr->port)
					ptr_addr->port = port2;
			}
			else
			{
				return FALSE;
			}

			// ...ip range (end)
			if (_app_parsenetworkstring (range_end, &format, &port2, &addr4, &addr6, NULL, 0))
			{
				if (format == NET_ADDRESS_IPV4)
				{
					ptr_addr->range.valueHigh.type = FWP_UINT32;
					ptr_addr->range.valueHigh.uint32 = addr4.addr;
				}
				else if (format == NET_ADDRESS_IPV6)
				{
					ptr_addr->range.valueHigh.type = FWP_BYTE_ARRAY16_TYPE;
					memcpy (ptr_addr->range.valueHigh.byteArray16->byteArray16, addr6.addr, FWP_V6_ADDR_SIZE);
				}
				else
				{
					return FALSE;
				}
			}
			else
			{
				return FALSE;
			}

			ptr_addr->format = format;
			ptr_addr->type = DataTypeIp;
		}
		else
		{
			// ...ip/host
			if (_app_parsenetworkstring (rule->buffer, &format, &port2, &ptr_addr->addr4, &ptr_addr->addr6, ptr_addr->host, RTL_NUMBER_OF (ptr_addr->host)))
			{
				ptr_addr->type = DataTypeIp;
				ptr_addr->format = format;

				if (port2)
					ptr_addr->port = port2;

				return TRUE;
			}
			else
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

INT _app_getlistview_id (ENUM_TYPE_DATA type)
{
	if (type == DataAppRegular || type == DataAppDevice || type == DataAppNetwork || type == DataAppPico)
		return IDC_APPS_PROFILE;

	else if (type == DataAppService)
		return IDC_APPS_SERVICE;

	else if (type == DataAppUWP)
		return IDC_APPS_UWP;

	else if (type == DataRuleBlocklist)
		return IDC_RULES_BLOCKLIST;

	else if (type == DataRuleSystem)
		return IDC_RULES_SYSTEM;

	else if (type == DataRuleUser)
		return IDC_RULES_CUSTOM;

	return 0;
}

HBITMAP _app_bitmapfromico (HICON hicon, INT icon_size)
{
	if (!hicon)
		return NULL;

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

			bitmap_info.bmiHeader.biSize = sizeof (bitmap_info);
			bitmap_info.bmiHeader.biPlanes = 1;
			bitmap_info.bmiHeader.biCompression = BI_RGB;

			bitmap_info.bmiHeader.biWidth = icon_size;
			bitmap_info.bmiHeader.biHeight = icon_size;
			bitmap_info.bmiHeader.biBitCount = 32;

			hbitmap = CreateDIBSection (hdc, &bitmap_info, DIB_RGB_COLORS, NULL, NULL, 0);

			if (hbitmap)
			{
				HGDIOBJ old_bitmap = SelectObject (hdc, hbitmap);

				BLENDFUNCTION blend_func = {0};
				blend_func.BlendOp = AC_SRC_OVER;
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

HBITMAP _app_bitmapfrompng (HINSTANCE hinst, LPCWSTR name, INT icon_size)
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
	IWICStream* wicStream = NULL;
	IWICBitmapSource* wicBitmapSource = NULL;
	IWICBitmapDecoder* wicDecoder = NULL;
	IWICBitmapFrameDecode* wicFrame = NULL;
	IWICImagingFactory* wicFactory = NULL;
	IWICFormatConverter* wicFormatConverter = NULL;
	IWICBitmapScaler* wicScaler = NULL;
	WICPixelFormatGUID pixelFormat;
	WICRect rect = {0, 0, icon_size, icon_size};

	if (FAILED (CoCreateInstance (&CLSID_WICImagingFactory1, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, &wicFactory)))
		goto CleanupExit;

	resource_buffer = (WICInProcPointer)_r_loadresource (hinst, name, L"PNG", &resource_length);

	if (!resource_buffer)
		goto CleanupExit;

	if (FAILED (IWICImagingFactory_CreateStream (wicFactory, &wicStream)))
		goto CleanupExit;

	if (FAILED (IWICStream_InitializeFromMemory (wicStream, resource_buffer, resource_length)))
		goto CleanupExit;

	if (FAILED (IWICImagingFactory_CreateDecoder (wicFactory, &GUID_ContainerFormatPng, NULL, &wicDecoder)))
		goto CleanupExit;

	if (FAILED (IWICBitmapDecoder_Initialize (wicDecoder, (IStream*)wicStream, WICDecodeMetadataCacheOnLoad)))
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
	if (memcmp (&pixelFormat, &GUID_WICPixelFormat32bppPRGBA, sizeof (GUID)) == 0)
	{
		wicBitmapSource = (IWICBitmapSource*)wicFrame;
	}
	else
	{
		if (FAILED (IWICImagingFactory_CreateFormatConverter (wicFactory, &wicFormatConverter)))
			goto CleanupExit;

		if (FAILED (IWICFormatConverter_Initialize (wicFormatConverter, (IWICBitmapSource*)wicFrame, &GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom)))
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
