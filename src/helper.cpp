// simplewall
// Copyright (c) 2016-2020 Henry++

#include "global.hpp"

VOID NTAPI _app_dereferenceapp (PVOID pdata)
{
	PITEM_APP ptr_item = (PITEM_APP)pdata;

	SAFE_DELETE_MEMORY (ptr_item->display_name);
	SAFE_DELETE_MEMORY (ptr_item->real_path);
	SAFE_DELETE_MEMORY (ptr_item->original_path);

	if (ptr_item->pnotification)
		_r_obj_dereference (ptr_item->pnotification);

	delete ptr_item;
}

VOID NTAPI _app_dereferenceappshelper (PVOID pdata)
{
	PITEM_APP_HELPER ptr_item = (PITEM_APP_HELPER)pdata;

	SAFE_DELETE_MEMORY (ptr_item->display_name);
	SAFE_DELETE_MEMORY (ptr_item->real_path);
	SAFE_DELETE_MEMORY (ptr_item->internal_name);

	SAFE_DELETE_MEMORY (ptr_item->pdata);
}

VOID NTAPI _app_dereferencelog (PVOID pdata)
{
	PITEM_LOG ptr_item = (PITEM_LOG)pdata;

	SAFE_DELETE_MEMORY (ptr_item->path);
	SAFE_DELETE_MEMORY (ptr_item->provider_name);
	SAFE_DELETE_MEMORY (ptr_item->filter_name);
	SAFE_DELETE_MEMORY (ptr_item->username);

	SAFE_DELETE_ICON (ptr_item->hicon);
}

VOID NTAPI _app_dereferencenetwork (PVOID pdata)
{
	PITEM_NETWORK ptr_item = (PITEM_NETWORK)pdata;

	SAFE_DELETE_MEMORY (ptr_item->path);
}

VOID NTAPI _app_dereferencerule (PVOID pdata)
{
	PITEM_RULE ptr_item = (PITEM_RULE)pdata;

	SAFE_DELETE_MEMORY (ptr_item->pname);
	SAFE_DELETE_MEMORY (ptr_item->prule_remote);
	SAFE_DELETE_MEMORY (ptr_item->prule_local);

	delete ptr_item;
}

VOID NTAPI _app_dereferenceruleconfig (PVOID pdata)
{
	PITEM_RULE_CONFIG ptr_item = (PITEM_RULE_CONFIG)pdata;

	SAFE_DELETE_MEMORY (ptr_item->pname);
	SAFE_DELETE_MEMORY (ptr_item->papps);
}

BOOLEAN _app_formataddress (ADDRESS_FAMILY af, UINT8 proto, const PVOID paddr, UINT16 port, LPWSTR* ptr_dest, DWORD flags)
{
	if (af != AF_INET && af != AF_INET6)
		return FALSE;

	BOOLEAN result = FALSE;

	WCHAR formatted_address[DNS_MAX_NAME_BUFFER_LENGTH] = {0};

	PIN_ADDR p4addr = (PIN_ADDR)paddr;
	PIN6_ADDR p6addr = (PIN6_ADDR)paddr;

	if ((flags & FMTADDR_AS_ARPA) != 0)
	{
		if (af == AF_INET)
		{
			_r_str_cat (formatted_address, RTL_NUMBER_OF (formatted_address), _r_fmt (L"%hhu.%hhu.%hhu.%hhu.%s", p4addr->s_impno, p4addr->s_lh, p4addr->s_host, p4addr->s_net, DNS_IP4_REVERSE_DOMAIN_STRING_W).GetString ());
		}
		else
		{
			for (INT i = sizeof (IN6_ADDR) - 1; i >= 0; i--)
				_r_str_cat (formatted_address, RTL_NUMBER_OF (formatted_address), _r_fmt (L"%hhx.%hhx.", p6addr->s6_addr[i] & 0xF, (p6addr->s6_addr[i] >> 4) & 0xF).GetString ());

			_r_str_cat (formatted_address, RTL_NUMBER_OF (formatted_address), DNS_IP6_REVERSE_DOMAIN_STRING_W);
		}

		result = TRUE;
	}
	else
	{
		if ((flags & FMTADDR_USE_PROTOCOL) != 0)
			_r_str_printf (formatted_address, RTL_NUMBER_OF (formatted_address), L"%s://", _app_getprotoname (proto, AF_UNSPEC, SZ_UNKNOWN).GetString ());

		WCHAR addr_str[DNS_MAX_NAME_BUFFER_LENGTH] = {0};

		if (_app_formatip (af, paddr, addr_str, RTL_NUMBER_OF (addr_str)))
		{
			if ((flags & FMTADDR_AS_RULE) != 0)
			{
				if (af == AF_INET)
					result = !IN4_IS_ADDR_UNSPECIFIED (p4addr);

				else
					result = !IN6_IS_ADDR_UNSPECIFIED (p6addr);

				if (result)
					_r_str_cat (formatted_address, RTL_NUMBER_OF (formatted_address), (af == AF_INET6) ? _r_fmt (L"[%s]", addr_str).GetString () : addr_str);
			}
			else
			{
				_r_str_cat (formatted_address, RTL_NUMBER_OF (formatted_address), addr_str);
				result = TRUE;
			}
		}

		if (port && (flags & FMTADDR_USE_PROTOCOL) == 0)
			_r_str_cat (formatted_address, RTL_NUMBER_OF (formatted_address), _r_fmt (!_r_str_isempty (formatted_address) ? L":%" TEXT (PRIu16) : L"%" TEXT (PRIu16), port).GetString ());
	}

	if ((flags & FMTADDR_RESOLVE_HOST) != 0)
	{
		if (result && app.ConfigGetBoolean (L"IsNetworkResolutionsEnabled", FALSE))
		{
			SIZE_T addr_hash = _r_str_hash (formatted_address);
			BOOLEAN is_exists = cache_hosts.find (addr_hash) != cache_hosts.end ();

			if (is_exists)
			{
				PR_OBJECT phash = cache_hosts[addr_hash];

				if (phash)
				{
					PR_OBJECT ptr_cache_object = _r_obj2_reference (phash);

					if (ptr_cache_object->pdata)
						_r_str_cat (formatted_address, RTL_NUMBER_OF (formatted_address), _r_fmt (L" (%s)", (LPCWSTR)ptr_cache_object->pdata).GetString ());

					_r_obj2_dereference (ptr_cache_object);
				}
			}
			else
			{
				cache_hosts[addr_hash] = NULL;

				LPWSTR ptr_cache = NULL;

				if (_app_resolveaddress (af, paddr, &ptr_cache))
				{
					_r_str_cat (formatted_address, RTL_NUMBER_OF (formatted_address), _r_fmt (L" (%s)", ptr_cache).GetString ());

					_app_freeobjects_map (cache_hosts, MAP_CACHE_MAX);

					cache_hosts[addr_hash] = _r_obj2_allocate (ptr_cache);
				}
			}
		}
	}

	_r_str_alloc (ptr_dest, _r_str_length (formatted_address), formatted_address);

	return result;
}

BOOLEAN _app_formatip (ADDRESS_FAMILY af, PVOID paddr, LPWSTR out_buffer, ULONG buffer_size)
{
	if (af == AF_INET)
	{
		if (NT_SUCCESS (RtlIpv4AddressToStringEx ((PIN_ADDR)paddr, 0, out_buffer, &buffer_size)))
			return TRUE;
	}
	else if (af == AF_INET6)
	{
		if (NT_SUCCESS (RtlIpv6AddressToStringEx ((PIN6_ADDR)paddr, 0, 0, out_buffer, &buffer_size)))
			return TRUE;
	}

	return FALSE;
}

rstring _app_formatport (UINT16 port, BOOLEAN is_noempty)
{
	rstring result;

	if (is_noempty)
	{
		result.Format (L"%" TEXT (PRIu16), port);

		rstring service_name = _app_getservicename (port, NULL);

		if (!service_name.IsEmpty ())
			result.AppendFormat (L" (%s)", service_name.GetString ());
	}
	else
	{
		result.Format (L"%" TEXT (PRIu16) L" (%s)", port, _app_getservicename (port, SZ_UNKNOWN).GetString ());
	}

	return result;
}

VOID _app_freeobjects_map (OBJECTS_MAP& ptr_map, SIZE_T max_size)
{
	if (max_size && ptr_map.size () <= max_size)
		return;

	for (auto &it = ptr_map.begin (); it != ptr_map.end ();)
	{
		if (it->second)
			_r_obj2_dereference (it->second);

		it = ptr_map.erase (it);

		if (max_size && ptr_map.size () <= max_size)
			break;
	}
}

VOID _app_freeappshelper_map (OBJECTS_APP_HELPER& ptr_map)
{
	if (ptr_map.empty ())
		return;

	for (auto &it = ptr_map.begin (); it != ptr_map.end ();)
	{
		if (it->second)
			_r_obj_dereference (it->second);

		it = ptr_map.erase (it);
	}
}

VOID _app_freerulesconfig_map (OBJECTS_RULE_CONFIG& ptr_map)
{
	if (ptr_map.empty ())
		return;

	for (auto &it = ptr_map.begin (); it != ptr_map.end ();)
	{
		if (it->second)
			_r_obj_dereference (it->second);

		it = ptr_map.erase (it);
	}
}

VOID _app_freeobjects_vec (OBJECTS_VEC& ptr_vec)
{
	for (auto &it = ptr_vec.begin (); it != ptr_vec.end ();)
	{
		PR_OBJECT ptr_object = *it;

		if (ptr_object)
			_r_obj2_dereference (ptr_object);

		it = ptr_vec.erase (it);
	}
}

VOID _app_freelogobjects_vec (OBJECTS_LOG& ptr_vec)
{
	for (auto &it = ptr_vec.begin (); it != ptr_vec.end ();)
	{
		PVOID ptr_object = *it;

		if (ptr_object)
		{
			PITEM_LOG ptr_log = (PITEM_LOG)_r_obj_reference (ptr_object);

			if (ptr_log)
				_r_obj_dereferenceex (ptr_log, 2);
		}

		it = ptr_vec.erase (it);
	}
}

VOID _app_freethreadpool (THREADS_VEC& ptr_vec)
{
	for (auto &it = ptr_vec.begin (); it != ptr_vec.end ();)
	{
		HANDLE hthread = *it;

		if (_r_fs_isvalidhandle (hthread))
		{
			if (WaitForSingleObjectEx (hthread, 0, FALSE) != WAIT_OBJECT_0)
			{
				++it;
				continue;
			}

			CloseHandle (hthread);
		}

		it = ptr_vec.erase (it);
	}
}

VOID _app_freelogstack ()
{
	while (TRUE)
	{
		PSLIST_ENTRY listEntry = RtlInterlockedPopEntrySList (&log_stack.ListHead);

		if (!listEntry)
			break;

		InterlockedDecrement (&log_stack.item_count);

		PITEM_LOG_LISTENTRY ptr_entry = CONTAINING_RECORD (listEntry, ITEM_LOG_LISTENTRY, ListEntry);

		if (ptr_entry)
		{
			if (ptr_entry->Body)
				_r_obj_dereference (ptr_entry->Body);

			_aligned_free (ptr_entry);
		}
	}
}

VOID _app_getappicon (const PITEM_APP ptr_app, BOOLEAN is_small, PINT picon_id, HICON* picon)
{
	BOOLEAN is_iconshidden = app.ConfigGetBoolean (L"IsIconsHidden", FALSE);

	if (ptr_app->type == DataAppRegular || ptr_app->type == DataAppService)
	{
		if (is_iconshidden || !_app_getfileicon (ptr_app->real_path, is_small, picon_id, picon))
		{
			if (picon_id)
				*picon_id = (ptr_app->type == DataAppService) ? config.icon_service_id : config.icon_id;

			if (picon)
				*picon = CopyIcon (is_small ? config.hicon_small : config.hicon_large);
		}

		if (ptr_app->type == DataAppService)
		{
			if (picon_id && *picon_id == config.icon_id)
				*picon_id = config.icon_service_id;
		}
	}
	else if (ptr_app->type == DataAppUWP)
	{
		if (picon_id)
			*picon_id = config.icon_uwp_id;

		if (picon)
			*picon = CopyIcon (is_small ? config.hicon_package : config.hicon_large); // small-only!
	}
	else
	{
		if (picon_id)
			*picon_id = config.icon_id;

		if (picon)
			*picon = CopyIcon (is_small ? config.hicon_small : config.hicon_large);
	}
}

VOID _app_getdisplayname (SIZE_T app_hash, PITEM_APP ptr_app, LPWSTR* extracted_name)
{
	if (!extracted_name)
		return;

	if (ptr_app->type == DataAppService)
	{
		_r_str_alloc (extracted_name, _r_str_length (ptr_app->original_path), ptr_app->original_path);
	}
	else if (ptr_app->type == DataAppUWP)
	{
		rstring name;

		if (!_app_item_get (ptr_app->type, app_hash, &name, NULL, NULL, NULL))
			name = ptr_app->original_path;

		_r_str_alloc (extracted_name, name.GetLength (), name.GetString ());
	}
	else
	{
		LPCWSTR ptr_path = ((app_hash == config.ntoskrnl_hash) ? ptr_app->original_path : ptr_app->real_path);

		if (app.ConfigGetBoolean (L"ShowFilenames", TRUE))
		{
			_r_str_alloc (extracted_name, INVALID_SIZE_T, _r_path_getfilename (ptr_path));
		}
		else
		{
			_r_str_alloc (extracted_name, _r_str_length (ptr_path), ptr_path);
		}
	}
}

BOOLEAN _app_getfileicon (LPCWSTR path, BOOLEAN is_small, PINT picon_id, HICON* picon)
{
	if (_r_str_isempty (path) || (!picon_id && !picon))
		return FALSE;

	BOOLEAN result = FALSE;

	SHFILEINFO shfi = {0};

	DWORD flags = 0;

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

		result = TRUE;
	}

	if (!result)
	{
		if (picon_id)
			*picon_id = config.icon_id;

		if (picon)
			*picon = CopyIcon (is_small ? config.hicon_small : config.hicon_large);

		result = TRUE;
	}

	return result;
}

PR_OBJECT _app_getsignatureinfo (SIZE_T app_hash, const PITEM_APP ptr_app)
{
	if (_r_str_isempty (ptr_app->real_path) || (ptr_app->type != DataAppRegular && ptr_app->type != DataAppService && ptr_app->type != DataAppUWP))
		return NULL;

	BOOLEAN is_exists = cache_signatures.find (app_hash) != cache_signatures.end ();

	PR_OBJECT ptr_cache_object = NULL;

	if (is_exists)
	{
		ptr_cache_object = _r_obj2_reference (cache_signatures[app_hash]);
	}
	else
	{
		cache_signatures[app_hash] = NULL;

		HANDLE hfile = CreateFile (ptr_app->real_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);

		if (_r_fs_isvalidhandle (hfile))
		{
			GUID WinTrustActionGenericVerifyV2 = WINTRUST_ACTION_GENERIC_VERIFY_V2;

			WINTRUST_FILE_INFO fileInfo = {0};

			fileInfo.cbStruct = sizeof (fileInfo);
			fileInfo.pcwszFilePath = ptr_app->real_path;
			fileInfo.hFile = hfile;

			WINTRUST_DATA trustData = {0};

			trustData.cbStruct = sizeof (trustData);
			trustData.dwUIChoice = WTD_UI_NONE;
			trustData.fdwRevocationChecks = WTD_REVOKE_NONE;
			trustData.dwProvFlags = WTD_SAFER_FLAG | WTD_CACHE_ONLY_URL_RETRIEVAL;
			trustData.dwUnionChoice = WTD_CHOICE_FILE;
			trustData.pFile = &fileInfo;

			trustData.dwStateAction = WTD_STATEACTION_VERIFY;
			LONG status = WinVerifyTrust ((HWND)INVALID_HANDLE_VALUE, &WinTrustActionGenericVerifyV2, &trustData);

			if (status == S_OK)
			{
				PCRYPT_PROVIDER_DATA provData = WTHelperProvDataFromStateData (trustData.hWVTStateData);

				if (provData)
				{
					PCRYPT_PROVIDER_SGNR psProvSigner = WTHelperGetProvSignerFromChain (provData, 0, FALSE, 0);

					if (psProvSigner)
					{
						CRYPT_PROVIDER_CERT* psProvCert = WTHelperGetProvCertFromChain (psProvSigner, 0);

						if (psProvCert)
						{
							DWORD num_chars = CertGetNameString (psProvCert->pCert, CERT_NAME_ATTR_TYPE, 0, szOID_COMMON_NAME, NULL, 0);

							if (num_chars > 1)
							{
								LPWSTR ptr_cache = (LPWSTR)_r_mem_allocatezero (num_chars * sizeof (WCHAR));

								if (CertGetNameString (psProvCert->pCert, CERT_NAME_ATTR_TYPE, 0, szOID_COMMON_NAME, ptr_cache, num_chars) > 1)
								{
									_app_freeobjects_map (cache_signatures, MAP_CACHE_MAX);

									cache_signatures[app_hash] = _r_obj2_allocate (ptr_cache);

									ptr_cache_object = _r_obj2_reference (cache_signatures[app_hash]);
								}
								else
								{
									_r_mem_free (ptr_cache);
								}
							}
						}
					}
				}
			}

			trustData.dwStateAction = WTD_STATEACTION_CLOSE;
			WinVerifyTrust ((HWND)INVALID_HANDLE_VALUE, &WinTrustActionGenericVerifyV2, &trustData);

			CloseHandle (hfile);
		}
	}

	ptr_app->is_signed = (ptr_cache_object != NULL && ptr_cache_object->pdata != NULL);

	return ptr_cache_object;
}

PR_OBJECT _app_getversioninfo (SIZE_T app_hash, const PITEM_APP ptr_app)
{
	if (_r_str_isempty (ptr_app->real_path))
		return NULL;

	PR_OBJECT ptr_cache_object = NULL;
	BOOLEAN is_exists = cache_versions.find (app_hash) != cache_versions.end ();

	if (is_exists)
	{
		PR_OBJECT phash = cache_versions[app_hash];

		if (phash)
			ptr_cache_object = _r_obj2_reference (phash);
	}
	else
	{
		cache_versions[app_hash] = NULL;

		HINSTANCE hlib = LoadLibraryEx (ptr_app->real_path, NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE);

		if (hlib)
		{
			HRSRC hres = FindResource (hlib, MAKEINTRESOURCE (VS_VERSION_INFO), RT_VERSION);

			if (hres)
			{
				HGLOBAL hglob = LoadResource (hlib, hres);

				if (hglob)
				{
					PVOID versionInfo = LockResource (hglob);

					if (versionInfo)
					{
						rstring buffer;

						UINT vLen = 0, langD = 0;
						PVOID retbuf = NULL;

						WCHAR authorEntry[128];
						WCHAR descriptionEntry[128];

						if (VerQueryValue (versionInfo, L"\\VarFileInfo\\Translation", &retbuf, &vLen) && vLen == 4)
						{
							RtlCopyMemory (&langD, retbuf, vLen);

							_r_str_printf (authorEntry, RTL_NUMBER_OF (authorEntry), L"\\StringFileInfo\\%02X%02X%02X%02X\\CompanyName", (langD & 0xff00) >> 8, langD & 0xff, (langD & 0xff000000) >> 24, (langD & 0xff0000) >> 16);
							_r_str_printf (descriptionEntry, RTL_NUMBER_OF (descriptionEntry), L"\\StringFileInfo\\%02X%02X%02X%02X\\FileDescription", (langD & 0xff00) >> 8, langD & 0xff, (langD & 0xff000000) >> 24, (langD & 0xff0000) >> 16);
						}
						else
						{
							_r_str_printf (authorEntry, RTL_NUMBER_OF (authorEntry), L"\\StringFileInfo\\%04X04B0\\CompanyName", GetUserDefaultLangID ());
							_r_str_printf (descriptionEntry, RTL_NUMBER_OF (descriptionEntry), L"\\StringFileInfo\\%04X04B0\\FileDescription", GetUserDefaultLangID ());
						}

						if (VerQueryValue (versionInfo, descriptionEntry, &retbuf, &vLen))
						{
							buffer.Append (SZ_TAB);
							buffer.Append ((LPCWSTR)retbuf);

							UINT length = 0;
							VS_FIXEDFILEINFO* verInfo = NULL;

							if (VerQueryValue (versionInfo, L"\\", (PVOID*)&verInfo, &length))
							{
								buffer.Append (_r_fmt (L" %d.%d", HIWORD (verInfo->dwFileVersionMS), LOWORD (verInfo->dwFileVersionMS)));

								if (HIWORD (verInfo->dwFileVersionLS) || LOWORD (verInfo->dwFileVersionLS))
								{
									buffer.Append (_r_fmt (L".%d", HIWORD (verInfo->dwFileVersionLS)));

									if (LOWORD (verInfo->dwFileVersionLS))
										buffer.Append (_r_fmt (L".%d", LOWORD (verInfo->dwFileVersionLS)));
								}
							}

							buffer.Append (L"\r\n");
						}

						if (VerQueryValue (versionInfo, authorEntry, &retbuf, &vLen))
						{
							buffer.Append (SZ_TAB);
							buffer.Append ((LPCWSTR)retbuf);
							buffer.Append (L"\r\n");
						}

						_r_str_trim (buffer, DIVIDER_TRIM);

						// get signature information
						LPWSTR ptr_cache = NULL;

						if (_r_str_alloc (&ptr_cache, buffer.GetLength (), buffer.GetString ()))
						{
							_app_freeobjects_map (cache_versions, MAP_CACHE_MAX);

							cache_versions[app_hash] = _r_obj2_allocate (ptr_cache);

							ptr_cache_object = _r_obj2_reference (cache_versions[app_hash]);
						}
					}

					FreeResource (hglob);
				}
			}

			FreeLibrary (hlib);
		}
	}

	return ptr_cache_object;
}

rstring _app_getservicename (UINT16 port, LPCWSTR default_value)
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

rstring _app_getprotoname (UINT8 proto, ADDRESS_FAMILY af, LPCWSTR default_value)
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

rstring _app_getconnectionstatusname (DWORD state, LPCWSTR default_value)
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

rstring _app_getdirectionname (FWP_DIRECTION direction, BOOLEAN is_loopback, BOOLEAN is_localized)
{
	rstring result;

	if (is_localized)
	{
		if (direction == FWP_DIRECTION_OUTBOUND)
			result = app.LocaleString (IDS_DIRECTION_1, NULL);

		else if (direction == FWP_DIRECTION_INBOUND)
			result = app.LocaleString (IDS_DIRECTION_2, NULL);

		else if (direction == FWP_DIRECTION_MAX)
			result = app.LocaleString (IDS_ANY, NULL);

		if (is_loopback)
			result.AppendFormat (L" (%s)", SZ_DIRECTION_LOOPBACK);
	}
	else
	{
		if (direction == FWP_DIRECTION_OUTBOUND)
			result = SZ_DIRECTION_OUT;

		else if (direction == FWP_DIRECTION_INBOUND)
			result = SZ_DIRECTION_IN;

		else if (direction == FWP_DIRECTION_MAX)
			result = SZ_DIRECTION_ANY;

		if (is_loopback)
			result.Append (L"-" SZ_DIRECTION_LOOPBACK);
	}

	return result;
}

rstring _app_getfiltername (LPCWSTR provider_name, LPCWSTR filter_name, LPCWSTR default_value)
{
	if (!_r_str_isempty (provider_name) && !_r_str_isempty (filter_name))
		return _r_fmt (L"%s\\%s", provider_name, filter_name);

	else if (!_r_str_isempty (filter_name))
		return filter_name;

	return default_value;
}

COLORREF _app_getcolorvalue (SIZE_T color_hash)
{
	if (!color_hash)
		return 0;

	for (auto &ptr_clr : colors)
	{
		if (ptr_clr->clr_hash == color_hash)
			return ptr_clr->new_clr ? ptr_clr->new_clr : ptr_clr->default_clr;
	}

	return 0;
}

rstring _app_getservicenamefromtag (HANDLE pid, const PVOID ptag)
{
	rstring result;
	HMODULE hlib = GetModuleHandle (L"advapi32.dll");

	if (hlib)
	{
		typedef ULONG (NTAPI* IQTI) (PVOID, SC_SERVICE_TAG_QUERY_TYPE, PSC_SERVICE_TAG_QUERY); // I_QueryTagInformation
		const IQTI _I_QueryTagInformation = (IQTI)GetProcAddress (hlib, "I_QueryTagInformation");

		if (_I_QueryTagInformation)
		{
			PSC_SERVICE_TAG_QUERY pnameTag = (PSC_SERVICE_TAG_QUERY)_r_mem_allocatezero (sizeof (SC_SERVICE_TAG_QUERY));

			pnameTag->ProcessId = HandleToUlong (pid);
			pnameTag->ServiceTag = PtrToUlong (ptag);

			_I_QueryTagInformation (NULL, ServiceNameFromTagInformation, pnameTag);

			if (pnameTag->Buffer)
			{
				result = (LPCWSTR)(pnameTag->Buffer);
				LocalFree (pnameTag->Buffer);
			}

			_r_mem_free (pnameTag);
		}
	}

	return result;
}

rstring _app_getnetworkpath (DWORD pid, PULONG64 pmodules, PINT picon_id, PSIZE_T phash)
{
	if (pid == PROC_WAITING_PID)
	{
		*phash = 0;
		*picon_id = config.icon_id;

		return PROC_WAITING_NAME;
	}

	rstring proc_name;

	if (pmodules)
		proc_name = _app_getservicenamefromtag (UlongToHandle (pid), UlongToPtr (*(PULONG)pmodules));

	if (proc_name.IsEmpty ())
	{
		if (pid == PROC_SYSTEM_PID)
		{
			proc_name = PROC_SYSTEM_NAME;
		}
		else
		{
			HANDLE hprocess = OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);

			if (hprocess)
			{
				DWORD size = 1024;
				BOOL is_success = QueryFullProcessImageName (hprocess, 0, proc_name.GetBuffer (size), &size);

				// fix for WSL processes (issue #606)
				if (!is_success && GetLastError () == ERROR_GEN_FAILURE)
				{
					size = 1024;
					is_success = QueryFullProcessImageName (hprocess, PROCESS_NAME_NATIVE, proc_name.GetBuffer (size), &size);
				}

				if (!is_success)
				{
					proc_name.Release ();
				}
				else
				{
					proc_name.ReleaseBuffer ();
				}

				CloseHandle (hprocess);
			}
		}
	}

	*phash = _r_str_hash (proc_name.GetString ());

	if (!proc_name.IsEmpty ())
	{
		*picon_id = (INT)_app_getappinfo (*phash, InfoIconId);

		if (!*picon_id)
			_app_getfileicon (proc_name.GetString (), TRUE, picon_id, NULL);
	}
	else
	{
		*picon_id = config.icon_id;
	}

	return proc_name;
}

SIZE_T _app_getnetworkhash (ADDRESS_FAMILY af, DWORD pid, PVOID remote_addr, DWORD remote_port, PVOID local_addr, DWORD local_port, UINT8 proto, DWORD state)
{
	WCHAR remote_addr_str[LEN_IP_MAX] = {0};
	WCHAR local_addr_str[LEN_IP_MAX] = {0};

	if (remote_addr)
		_app_formatip (af, remote_addr, remote_addr_str, RTL_NUMBER_OF (remote_addr_str));

	if (local_addr)
		_app_formatip (af, local_addr, local_addr_str, RTL_NUMBER_OF (local_addr_str));

	return _r_str_hash (_r_fmt (L"%" TEXT (PRIu8) L"_%" TEXT (PR_ULONG) L"_%s_%" TEXT (PR_ULONG) L"_%s_%" TEXT (PR_ULONG) L"_%" TEXT (PRIu8) L"_%" TEXT (PR_ULONG), af, pid, remote_addr_str, remote_port, local_addr_str, local_port, proto, state).GetString ());
}

BOOLEAN _app_isvalidconnection (ADDRESS_FAMILY af, const PVOID paddr)
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

VOID _app_generate_connections (OBJECTS_NETWORK& ptr_map, HASHER_MAP& checker_map)
{
	checker_map.clear ();

	DWORD allocationSize = 0x4000;
	PVOID memoryAddress = _r_mem_allocatezero (allocationSize);

	DWORD requiredSize = 0;
	GetExtendedTcpTable (NULL, &requiredSize, FALSE, AF_INET, TCP_TABLE_OWNER_MODULE_ALL, 0);

	if (requiredSize)
	{
		if (allocationSize < requiredSize)
		{
			memoryAddress = _r_mem_reallocatezero (memoryAddress, requiredSize);
			allocationSize = requiredSize;
		}

		PMIB_TCPTABLE_OWNER_MODULE tcp4Table = (PMIB_TCPTABLE_OWNER_MODULE)memoryAddress;

		if (GetExtendedTcpTable (tcp4Table, &requiredSize, FALSE, AF_INET, TCP_TABLE_OWNER_MODULE_ALL, 0) == NO_ERROR)
		{
			for (DWORD i = 0; i < tcp4Table->dwNumEntries; i++)
			{
				IN_ADDR remote_addr = {0};
				IN_ADDR local_addr = {0};

				remote_addr.S_un.S_addr = tcp4Table->table[i].dwRemoteAddr;
				local_addr.S_un.S_addr = tcp4Table->table[i].dwLocalAddr;

				SIZE_T network_hash = _app_getnetworkhash (AF_INET, tcp4Table->table[i].dwOwningPid, &remote_addr, tcp4Table->table[i].dwRemotePort, &local_addr, tcp4Table->table[i].dwLocalPort, IPPROTO_TCP, tcp4Table->table[i].dwState);

				if (ptr_map.find (network_hash) != ptr_map.end ())
				{
					checker_map[network_hash] = FALSE;
					continue;
				}

				PITEM_NETWORK ptr_network = (PITEM_NETWORK)_r_obj_allocateex (sizeof (ITEM_NETWORK), &_app_dereferencenetwork);

				rstring path = _app_getnetworkpath (tcp4Table->table[i].dwOwningPid, tcp4Table->table[i].OwningModuleInfo, &ptr_network->icon_id, &ptr_network->app_hash);

				_r_str_alloc (&ptr_network->path, path.GetLength (), path.GetString ());

				ptr_network->af = AF_INET;
				ptr_network->protocol = IPPROTO_TCP;

				ptr_network->remote_addr.S_un.S_addr = tcp4Table->table[i].dwRemoteAddr;
				ptr_network->remote_port = _r_byteswap_ushort ((USHORT)tcp4Table->table[i].dwRemotePort);

				ptr_network->local_addr.S_un.S_addr = tcp4Table->table[i].dwLocalAddr;
				ptr_network->local_port = _r_byteswap_ushort ((USHORT)tcp4Table->table[i].dwLocalPort);

				ptr_network->state = tcp4Table->table[i].dwState;

				if (tcp4Table->table[i].dwState == MIB_TCP_STATE_ESTAB)
				{
					if (_app_isvalidconnection (ptr_network->af, &ptr_network->remote_addr) || _app_isvalidconnection (ptr_network->af, &ptr_network->local_addr))
						ptr_network->is_connection = TRUE;
				}

				ptr_map[network_hash] = ptr_network;
				checker_map[network_hash] = TRUE;
			}
		}
	}

	requiredSize = 0;
	GetExtendedTcpTable (NULL, &requiredSize, FALSE, AF_INET6, TCP_TABLE_OWNER_MODULE_ALL, 0);

	if (requiredSize)
	{
		if (allocationSize < requiredSize)
		{
			memoryAddress = _r_mem_reallocate (memoryAddress, requiredSize);
			allocationSize = requiredSize;
		}

		PMIB_TCP6TABLE_OWNER_MODULE tcp6Table = (PMIB_TCP6TABLE_OWNER_MODULE)memoryAddress;

		if (GetExtendedTcpTable (tcp6Table, &requiredSize, FALSE, AF_INET6, TCP_TABLE_OWNER_MODULE_ALL, 0) == NO_ERROR)
		{
			for (DWORD i = 0; i < tcp6Table->dwNumEntries; i++)
			{
				SIZE_T network_hash = _app_getnetworkhash (AF_INET6, tcp6Table->table[i].dwOwningPid, tcp6Table->table[i].ucRemoteAddr, tcp6Table->table[i].dwRemotePort, tcp6Table->table[i].ucLocalAddr, tcp6Table->table[i].dwLocalPort, IPPROTO_TCP, tcp6Table->table[i].dwState);

				if (ptr_map.find (network_hash) != ptr_map.end ())
				{
					checker_map[network_hash] = FALSE;
					continue;
				}

				PITEM_NETWORK ptr_network = (PITEM_NETWORK)_r_obj_allocateex (sizeof (ITEM_NETWORK), &_app_dereferencenetwork);

				rstring path = _app_getnetworkpath (tcp6Table->table[i].dwOwningPid, tcp6Table->table[i].OwningModuleInfo, &ptr_network->icon_id, &ptr_network->app_hash);

				_r_str_alloc (&ptr_network->path, path.GetLength (), path.GetString ());

				ptr_network->af = AF_INET6;
				ptr_network->protocol = IPPROTO_TCP;

				RtlCopyMemory (ptr_network->remote_addr6.u.Byte, tcp6Table->table[i].ucRemoteAddr, FWP_V6_ADDR_SIZE);
				ptr_network->remote_port = _r_byteswap_ushort ((USHORT)tcp6Table->table[i].dwRemotePort);

				RtlCopyMemory (ptr_network->local_addr6.u.Byte, tcp6Table->table[i].ucLocalAddr, FWP_V6_ADDR_SIZE);
				ptr_network->local_port = _r_byteswap_ushort ((USHORT)tcp6Table->table[i].dwLocalPort);

				ptr_network->state = tcp6Table->table[i].dwState;

				if (tcp6Table->table[i].dwState == MIB_TCP_STATE_ESTAB)
				{
					if (_app_isvalidconnection (ptr_network->af, &ptr_network->remote_addr6) || _app_isvalidconnection (ptr_network->af, &ptr_network->local_addr6))
						ptr_network->is_connection = TRUE;
				}

				ptr_map[network_hash] = ptr_network;
				checker_map[network_hash] = TRUE;
			}
		}
	}

	requiredSize = 0;
	GetExtendedUdpTable (NULL, &requiredSize, FALSE, AF_INET, UDP_TABLE_OWNER_MODULE, 0);

	if (requiredSize)
	{
		if (allocationSize < requiredSize)
		{
			memoryAddress = _r_mem_reallocate (memoryAddress, requiredSize);
			allocationSize = requiredSize;
		}

		PMIB_UDPTABLE_OWNER_MODULE udp4Table = (PMIB_UDPTABLE_OWNER_MODULE)memoryAddress;

		if (GetExtendedUdpTable (udp4Table, &requiredSize, FALSE, AF_INET, UDP_TABLE_OWNER_MODULE, 0) == NO_ERROR)
		{
			for (DWORD i = 0; i < udp4Table->dwNumEntries; i++)
			{
				IN_ADDR local_addr = {0};
				local_addr.S_un.S_addr = udp4Table->table[i].dwLocalAddr;

				SIZE_T network_hash = _app_getnetworkhash (AF_INET, udp4Table->table[i].dwOwningPid, NULL, 0, &local_addr, udp4Table->table[i].dwLocalPort, IPPROTO_UDP, 0);

				if (ptr_map.find (network_hash) != ptr_map.end ())
				{
					checker_map[network_hash] = FALSE;
					continue;
				}

				PITEM_NETWORK ptr_network = (PITEM_NETWORK)_r_obj_allocateex (sizeof (ITEM_NETWORK), &_app_dereferencenetwork);

				rstring path = _app_getnetworkpath (udp4Table->table[i].dwOwningPid, udp4Table->table[i].OwningModuleInfo, &ptr_network->icon_id, &ptr_network->app_hash);

				_r_str_alloc (&ptr_network->path, path.GetLength (), path.GetString ());

				ptr_network->af = AF_INET;
				ptr_network->protocol = IPPROTO_UDP;

				ptr_network->local_addr.S_un.S_addr = udp4Table->table[i].dwLocalAddr;
				ptr_network->local_port = _r_byteswap_ushort ((USHORT)udp4Table->table[i].dwLocalPort);

				ptr_network->state = 0;

				if (_app_isvalidconnection (ptr_network->af, &ptr_network->local_addr))
					ptr_network->is_connection = TRUE;

				ptr_map[network_hash] = ptr_network;
				checker_map[network_hash] = TRUE;
			}
		}

	}

	requiredSize = 0;
	GetExtendedUdpTable (NULL, &requiredSize, FALSE, AF_INET6, UDP_TABLE_OWNER_MODULE, 0);

	if (requiredSize)
	{
		if (allocationSize < requiredSize)
		{
			memoryAddress = _r_mem_reallocate (memoryAddress, requiredSize);
			allocationSize = requiredSize;
		}

		PMIB_UDP6TABLE_OWNER_MODULE udp6Table = (PMIB_UDP6TABLE_OWNER_MODULE)memoryAddress;

		if (GetExtendedUdpTable (udp6Table, &requiredSize, FALSE, AF_INET6, UDP_TABLE_OWNER_MODULE, 0) == NO_ERROR)
		{
			for (DWORD i = 0; i < udp6Table->dwNumEntries; i++)
			{
				SIZE_T network_hash = _app_getnetworkhash (AF_INET6, udp6Table->table[i].dwOwningPid, NULL, 0, udp6Table->table[i].ucLocalAddr, udp6Table->table[i].dwLocalPort, IPPROTO_UDP, 0);

				if (ptr_map.find (network_hash) != ptr_map.end ())
				{
					checker_map[network_hash] = FALSE;
					continue;
				}

				PITEM_NETWORK ptr_network = (PITEM_NETWORK)_r_obj_allocateex (sizeof (ITEM_NETWORK), &_app_dereferencenetwork);

				rstring path = _app_getnetworkpath (udp6Table->table[i].dwOwningPid, udp6Table->table[i].OwningModuleInfo, &ptr_network->icon_id, &ptr_network->app_hash);

				_r_str_alloc (&ptr_network->path, path.GetLength (), path.GetString ());

				ptr_network->af = AF_INET6;
				ptr_network->protocol = IPPROTO_UDP;

				RtlCopyMemory (ptr_network->local_addr6.u.Byte, udp6Table->table[i].ucLocalAddr, FWP_V6_ADDR_SIZE);
				ptr_network->local_port = _r_byteswap_ushort ((USHORT)udp6Table->table[i].dwLocalPort);

				ptr_network->state = 0;

				if (_app_isvalidconnection (ptr_network->af, &ptr_network->local_addr6))
					ptr_network->is_connection = TRUE;

				ptr_map[network_hash] = ptr_network;
				checker_map[network_hash] = TRUE;
			}
		}
	}

	if (memoryAddress)
		_r_mem_free (memoryAddress);
}

VOID _app_generate_packages ()
{
	HKEY hkey;

	LONG code = RegOpenKeyEx (HKEY_CLASSES_ROOT, L"Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages", 0, KEY_READ, &hkey);

	if (code == ERROR_SUCCESS)
	{
		DWORD index = 0;
		DWORD max_length = _r_reg_querysubkeylength (hkey);

		if (!max_length)
		{
			RegCloseKey (hkey);
			return;
		}

		HKEY hsubkey;
		LPWSTR key_name = (LPWSTR)_r_mem_allocatezero ((max_length + 1) * sizeof (WCHAR));

		while (TRUE)
		{
			DWORD size = max_length;

			if (RegEnumKeyEx (hkey, index++, key_name, &size, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
				break;

			code = RegOpenKeyEx (hkey, key_name, 0, KEY_READ, &hsubkey);

			if (code == ERROR_SUCCESS)
			{
				LPBYTE package_sid = _r_reg_querybinary (hsubkey, L"PackageSid");

				if (!package_sid || !RtlValidSid (package_sid))
				{
					RegCloseKey (hsubkey);
					continue;
				}

				rstring package_sid_string = _r_str_fromsid (package_sid);

				if (package_sid_string.IsEmpty ())
				{
					_r_mem_free (package_sid);
					RegCloseKey (hsubkey);

					continue;
				}

				SIZE_T app_hash = _r_str_hash (package_sid_string.GetString ());

				if (_app_isapphelperfound (app_hash))
				{
					_r_mem_free (package_sid);
					RegCloseKey (hsubkey);

					continue;
				}

				rstring display_name = _r_reg_querystring (hsubkey, L"DisplayName");

				if (!display_name.IsEmpty ())
				{
					if (display_name.At (0) == L'@')
					{
						WCHAR name[MAX_PATH];

						if (SUCCEEDED (SHLoadIndirectString (display_name.GetString (), name, RTL_NUMBER_OF (name), NULL)))
							display_name = name;

						else
							display_name.Release ();
					}
				}

				if (display_name.IsEmpty ())
				{
					_r_mem_free (package_sid);
					RegCloseKey (hsubkey);

					continue;
				}

				PITEM_APP_HELPER ptr_item = (PITEM_APP_HELPER)_r_obj_allocateex (sizeof (ITEM_APP_HELPER), &_app_dereferenceappshelper);

				ptr_item->type = DataAppUWP;
				ptr_item->pdata = package_sid;

				// query timestamp
				ptr_item->timestamp = _r_reg_querytimestamp (hsubkey);

				rstring path = _r_reg_querystring (hsubkey, L"PackageRootFolder");

				if (!path.IsEmpty ())
					_r_str_alloc (&ptr_item->real_path, path.GetLength (), path.GetString ());

				RegCloseKey (hsubkey);

				_r_str_alloc (&ptr_item->display_name, display_name.GetLength (), display_name.GetString ());
				_r_str_alloc (&ptr_item->internal_name, package_sid_string.GetLength (), package_sid_string.GetString ());

				// load additional info from appx manifest
				_app_load_appxmanifest (ptr_item);

				apps_helper[app_hash] = ptr_item;
			}
		}

		_r_mem_free (key_name);

		RegCloseKey (hkey);
	}
}

VOID _app_generate_services ()
{
	SC_HANDLE hsvcmgr = OpenSCManager (NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE);

	if (!hsvcmgr)
		return;

	static DWORD initialBufferSize = 0x8000;

	DWORD returnLength = 0;
	DWORD servicesReturned = 0;
	DWORD dwServiceType = SERVICE_WIN32_OWN_PROCESS | SERVICE_WIN32_SHARE_PROCESS;
	DWORD dwServiceState = SERVICE_STATE_ALL;

	// win10+
	if (_r_sys_validversion (10, 0))
		dwServiceType |= SERVICE_INTERACTIVE_PROCESS | SERVICE_USER_SERVICE | SERVICE_USERSERVICE_INSTANCE;

	DWORD bufferSize = initialBufferSize;
	PVOID pBuffer = _r_mem_allocatezero (bufferSize);

	if (!pBuffer)
		return;

	if (!EnumServicesStatusEx (hsvcmgr, SC_ENUM_PROCESS_INFO, dwServiceType, dwServiceState, (LPBYTE)pBuffer, bufferSize, &returnLength, &servicesReturned, NULL, NULL))
	{
		if (GetLastError () == ERROR_MORE_DATA)
		{
			// Set the buffer
			bufferSize += returnLength;
			pBuffer = _r_mem_reallocateex (pBuffer, bufferSize, HEAP_ZERO_MEMORY);

			if (pBuffer)
			{
				// Now query again for services
				if (!EnumServicesStatusEx (hsvcmgr, SC_ENUM_PROCESS_INFO, dwServiceType, dwServiceState, (LPBYTE)pBuffer, bufferSize, &returnLength, &servicesReturned, NULL, NULL))
				{
					_r_mem_free (pBuffer);
					pBuffer = NULL;
				}
			}
		}
		else
		{
			_r_mem_free (pBuffer);
			pBuffer = NULL;
		}
	}

	// now traverse each service to get information
	if (pBuffer)
	{
		for (DWORD i = 0; i < servicesReturned; i++)
		{
			LPENUM_SERVICE_STATUS_PROCESS psvc = ((LPENUM_SERVICE_STATUS_PROCESS)pBuffer + i);

			LPCWSTR display_name = psvc->lpDisplayName;
			LPCWSTR service_name = psvc->lpServiceName;

			rstring real_path;

			time_t timestamp = 0;

			SIZE_T app_hash = _r_str_hash (service_name);

			if (_app_isapphelperfound (app_hash))
				continue;

			// query "ServiceDll" path
			{
				HKEY hkey = NULL;

				if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, _r_fmt (L"System\\CurrentControlSet\\Services\\%s\\Parameters", service_name).GetString (), 0, KEY_READ, &hkey) == ERROR_SUCCESS)
				{
					// query path
					real_path = _r_reg_querystring (hkey, L"ServiceDll");

					// query timestamp
					timestamp = _r_reg_querytimestamp (hkey);

					RegCloseKey (hkey);
				}
			}

			// fallback
			if (real_path.IsEmpty () || !timestamp)
			{
				HKEY hkey = NULL;

				if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, _r_fmt (L"System\\CurrentControlSet\\Services\\%s", service_name).GetString (), 0, KEY_READ, &hkey) == ERROR_SUCCESS)
				{
					// query path
					if (real_path.IsEmpty ())
					{
						real_path = _r_reg_querystring (hkey, L"ImagePath");

						if (!real_path.IsEmpty ())
						{
							PathRemoveArgs (real_path.GetBuffer ());
							real_path.ReleaseBuffer ();

							PathUnquoteSpaces (real_path.GetBuffer ());
							real_path.ReleaseBuffer ();
						}
					}

					// query timestamp
					if (!timestamp)
						timestamp = _r_reg_querytimestamp (hkey);

					RegCloseKey (hkey);
				}
			}

			if (!real_path.IsEmpty ())
				real_path = _r_path_dospathfromnt (real_path.GetString ());

			PSID pserviceSid = _app_queryservicesid (service_name);

			if (pserviceSid)
			{
				PVOID pservice_sd = NULL;
				DWORD sd_length = 0;

				EXPLICIT_ACCESS ea;
				RtlSecureZeroMemory (&ea, sizeof (ea));

				// When evaluating SECURITY_DESCRIPTOR conditions, the filter engine
				// checks for FWP_ACTRL_MATCH_FILTER access. If the DACL grants access,
				// it does not mean that the traffic is allowed; it just means that the
				// condition evaluates to true. Likewise if it denies access, the
				// condition evaluates to false.
				_app_setexplicitaccess (&ea, GRANT_ACCESS, NO_INHERITANCE, FWP_ACTRL_MATCH_FILTER, pserviceSid);

				// Security descriptors must be in self-relative form (i.e., contiguous).
				// The security descriptor returned by BuildSecurityDescriptorW is
				// already self-relative, but if you're using another mechanism to build
				// the descriptor, you may have to convert it. See MakeSelfRelativeSD for
				// details.
				if (BuildSecurityDescriptor (NULL, NULL, 1, &ea, 0, NULL, NULL, &sd_length, &pservice_sd) != ERROR_SUCCESS)
					continue;

				PITEM_APP_HELPER ptr_item = (PITEM_APP_HELPER)_r_obj_allocateex (sizeof (ITEM_APP_HELPER), &_app_dereferenceappshelper);

				ptr_item->type = DataAppService;
				ptr_item->timestamp = timestamp;

				_r_str_alloc (&ptr_item->display_name, _r_str_length (display_name), display_name);
				_r_str_alloc (&ptr_item->real_path, real_path.GetLength (), real_path.GetString ());
				_r_str_alloc (&ptr_item->internal_name, _r_str_length (service_name), service_name);

				ptr_item->pdata = _r_mem_allocatezero (sd_length);
				RtlCopyMemory (ptr_item->pdata, pservice_sd, sd_length);

				SAFE_DELETE_LOCAL (pservice_sd);

				apps_helper[app_hash] = ptr_item;

				_r_mem_free (pserviceSid);
			}
		}

		_r_mem_free (pBuffer);
	}

	CloseServiceHandle (hsvcmgr);
}

VOID _app_generate_rulesmenu (HMENU hsubmenu, SIZE_T app_hash)
{
	PITEM_STATUS pstatus = (PITEM_STATUS)_r_mem_allocatezero (sizeof (ITEM_STATUS));

	_app_getcount (pstatus);

	if (!app_hash || !pstatus->rules_count)
	{
		AppendMenu (hsubmenu, MF_SEPARATOR, 0, NULL);
		AppendMenu (hsubmenu, MF_STRING, IDX_RULES_SPECIAL, app.LocaleString (IDS_STATUS_EMPTY, NULL).GetString ());

		_r_menu_enableitem (hsubmenu, IDX_RULES_SPECIAL, MF_BYCOMMAND, FALSE);
	}
	else
	{
		for (UINT8 type = 0; type < 2; type++)
		{
			if (type == 0)
			{
				if (!pstatus->rules_predefined_count)
					continue;
			}
			else
			{
				if (!pstatus->rules_user_count)
					continue;
			}

			AppendMenu (hsubmenu, MF_SEPARATOR, 0, NULL);

			for (UINT8 loop = 0; loop < 2; loop++)
			{
				for (SIZE_T i = 0; i < rules_arr.size (); i++)
				{
					PR_OBJECT ptr_rule_object = rules_arr.at (i);

					if (!ptr_rule_object)
						continue;

					ptr_rule_object = _r_obj2_reference (ptr_rule_object);
					PITEM_RULE ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

					if (ptr_rule)
					{
						BOOLEAN is_global = (ptr_rule->is_enabled && ptr_rule->apps.empty ());
						BOOLEAN is_enabled = is_global || (ptr_rule->is_enabled && (ptr_rule->apps.find (app_hash) != ptr_rule->apps.end ()));

						if (ptr_rule->type != DataRuleCustom || (type == 0 && (!ptr_rule->is_readonly || is_global)) || (type == 1 && (ptr_rule->is_readonly || is_global)))
						{
							_r_obj2_dereference (ptr_rule_object);
							continue;
						}

						if ((loop == 0 && !is_enabled) || (loop == 1 && is_enabled))
						{
							_r_obj2_dereference (ptr_rule_object);
							continue;
						}

						WCHAR buffer[128];
						_r_str_printf (buffer, RTL_NUMBER_OF (buffer), app.LocaleString (IDS_RULE_APPLY_2, ptr_rule->is_readonly ? SZ_RULE_INTERNAL_MENU : NULL).GetString (), ptr_rule->pname);

						MENUITEMINFO mii = {0};

						mii.cbSize = sizeof (mii);
						mii.fMask = MIIM_ID | MIIM_FTYPE | MIIM_STATE | MIIM_STRING | MIIM_BITMAP | MIIM_CHECKMARKS;
						mii.fType = MFT_STRING;
						mii.dwTypeData = buffer;
						mii.hbmpItem = ptr_rule->is_block ? config.hbmp_block : config.hbmp_allow;
						mii.hbmpChecked = config.hbmp_checked;
						mii.hbmpUnchecked = config.hbmp_unchecked;
						mii.fState = (is_enabled ? MF_CHECKED : MF_UNCHECKED);
						mii.wID = IDX_RULES_SPECIAL + (UINT)i;

						InsertMenuItem (hsubmenu, mii.wID, FALSE, &mii);
					}

					_r_obj2_dereference (ptr_rule_object);
				}
			}
		}
	}

	AppendMenu (hsubmenu, MF_SEPARATOR, 0, NULL);
	AppendMenu (hsubmenu, MF_STRING, IDM_EDITRULES, app.LocaleString (IDS_EDITRULES, NULL).GetString ());
	AppendMenu (hsubmenu, MF_STRING, IDM_OPENRULESEDITOR, app.LocaleString (IDS_OPENRULESEDITOR, L"...").GetString ());

	_r_mem_free (pstatus);
}

VOID _app_generate_timermenu (HMENU hsubmenu, SIZE_T app_hash)
{
	BOOLEAN is_checked = (app_hash == 0);

	time_t current_time = _r_unixtime_now ();
	time_t* ptime = (time_t*)_app_getappinfo (app_hash, InfoTimerPtr);
	time_t app_time = ptime ? *ptime : 0;

	for (SIZE_T i = 0; i < timers.size (); i++)
	{
		UINT menu_id = IDX_TIMER + (UINT)i;
		time_t timer = timers.at (i);

		AppendMenu (hsubmenu, MF_STRING, menu_id, _r_fmt_interval (timer + 1, 1).GetString ());

		if (!is_checked && (app_time > current_time) && (app_time <= (current_time + timer)))
		{
			_r_menu_checkitem (hsubmenu, IDX_TIMER, menu_id, MF_BYCOMMAND, menu_id);
			is_checked = TRUE;
		}
	}

	if (!is_checked)
		_r_menu_checkitem (hsubmenu, IDM_DISABLETIMER, IDM_DISABLETIMER, MF_BYCOMMAND, IDM_DISABLETIMER);
}

BOOLEAN _app_item_get (ENUM_TYPE_DATA type, SIZE_T app_hash, rstring* display_name, rstring* real_path, time_t* ptime, PVOID* lpdata)
{
	PITEM_APP_HELPER ptr_app_item = _app_getapphelperitem (app_hash);

	if (!ptr_app_item)
		return FALSE;

	if (ptr_app_item->type != type)
	{
		_r_obj_dereference (ptr_app_item);
		return FALSE;
	}

	if (display_name)
	{
		if (!_r_str_isempty (ptr_app_item->display_name))
			*display_name = ptr_app_item->display_name;

		else if (!_r_str_isempty (ptr_app_item->real_path))
			*display_name = ptr_app_item->real_path;

		else if (!_r_str_isempty (ptr_app_item->internal_name))
			*display_name = ptr_app_item->internal_name;
	}

	if (real_path)
	{
		if (!_r_str_isempty (ptr_app_item->real_path))
			*real_path = ptr_app_item->real_path;
	}

	if (lpdata)
		*lpdata = ptr_app_item->pdata;

	if (ptime)
		*ptime = ptr_app_item->timestamp;

	_r_obj_dereference (ptr_app_item);

	return TRUE;
}

rstring _app_parsehostaddress_dns (LPCWSTR hostname, USHORT port)
{
	if (_r_str_isempty (hostname))
		return NULL;

	rstring result;

	PDNS_RECORD ppQueryResultsSet = NULL;

	// ipv4 address
	DNS_STATUS dnsStatus = DnsQuery (hostname, DNS_TYPE_A, DNS_QUERY_NO_HOSTS_FILE, NULL, &ppQueryResultsSet, NULL);

	if (dnsStatus != DNS_ERROR_RCODE_NO_ERROR && dnsStatus != DNS_INFO_NO_RECORDS)
	{
		app.LogError (L"DnsQuery (DNS_TYPE_A)", dnsStatus, hostname, 0);
	}
	else
	{
		if (ppQueryResultsSet)
		{
			for (auto &current = ppQueryResultsSet; current != NULL; current = current->pNext)
			{
				// ipv4 address
				WCHAR str[INET_ADDRSTRLEN];

				if (_app_formatip (AF_INET, &(current->Data.A.IpAddress), str, RTL_NUMBER_OF (str)))
				{
					result.Append (str);

					if (port)
						result.AppendFormat (L":%d", port);

					result.Append (DIVIDER_RULE);
				}
			}

			DnsRecordListFree (ppQueryResultsSet, DnsFreeRecordList);
			ppQueryResultsSet = NULL;
		}
	}

	// ipv6 address
	dnsStatus = DnsQuery (hostname, DNS_TYPE_AAAA, DNS_QUERY_NO_HOSTS_FILE, NULL, &ppQueryResultsSet, NULL);

	if (dnsStatus != DNS_ERROR_RCODE_NO_ERROR && dnsStatus != DNS_INFO_NO_RECORDS)
	{
		app.LogError (L"DnsQuery (DNS_TYPE_AAAA)", dnsStatus, hostname, 0);
	}
	else
	{
		if (ppQueryResultsSet)
		{
			for (auto &current = ppQueryResultsSet; current != NULL; current = current->pNext)
			{
				WCHAR str[INET6_ADDRSTRLEN];

				if (_app_formatip (AF_INET6, &(current->Data.AAAA.Ip6Address), str, RTL_NUMBER_OF (str)))
				{
					result.Append (str);

					if (port)
						result.AppendFormat (L":%d", port);

					result.Append (DIVIDER_RULE);
				}
			}

			DnsRecordListFree (ppQueryResultsSet, DnsFreeRecordList);
			ppQueryResultsSet = NULL;
		}
	}

	_r_str_trim (result, DIVIDER_RULE);

	return result;
}

BOOLEAN _app_parsenetworkstring (LPCWSTR network_string, NET_ADDRESS_FORMAT* format_ptr, PUSHORT port_ptr, FWP_V4_ADDR_AND_MASK* paddr4, FWP_V6_ADDR_AND_MASK* paddr6, LPWSTR paddr_dns, SIZE_T dns_length)
{
	NET_ADDRESS_INFO ni;
	RtlSecureZeroMemory (&ni, sizeof (ni));

	USHORT port;
	BYTE prefix_length;

	DWORD types = NET_STRING_ANY_ADDRESS | NET_STRING_ANY_SERVICE | NET_STRING_IP_NETWORK | NET_STRING_ANY_ADDRESS_NO_SCOPE | NET_STRING_ANY_SERVICE_NO_SCOPE;
	DWORD code = ParseNetworkString (network_string, types, &ni, &port, &prefix_length);

	if (code != ERROR_SUCCESS)
	{
		app.LogError (L"ParseNetworkString", code, network_string, 0);
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
				RtlCopyMemory (paddr6->addr, ni.Ipv6Address.sin6_addr.u.Byte, FWP_V6_ADDR_SIZE);
				paddr6->prefixLength = min (prefix_length, 128);
			}

			return TRUE;
		}
		else if (ni.Format == NET_ADDRESS_DNS_NAME)
		{
			if (paddr_dns)
			{
				SIZE_T dns_hash = _r_str_hash (ni.NamedAddress.Address);
				BOOLEAN is_exists = cache_dns.find (dns_hash) != cache_dns.end ();

				if (is_exists)
				{
					PR_OBJECT ptr_cache_object = _r_obj2_reference (cache_dns[dns_hash]);

					if (ptr_cache_object)
					{
						if (ptr_cache_object->pdata)
							_r_str_copy (paddr_dns, dns_length, (LPCWSTR)ptr_cache_object->pdata);

						_r_obj2_dereference (ptr_cache_object);
					}

					return !_r_str_isempty (paddr_dns);
				}

				rstring host = _app_parsehostaddress_dns (ni.NamedAddress.Address, port);

				//if (host.IsEmpty ())
				//	host = _app_parsehostaddress_wsa (ni.NamedAddress.Address, port);

				if (host.IsEmpty ())
				{
					return FALSE;
				}
				else
				{
					_r_str_copy (paddr_dns, dns_length, host.GetString ());

					LPWSTR ptr_cache = NULL;

					if (_r_str_alloc (&ptr_cache, host.GetLength (), host.GetString ()))
					{
						_app_freeobjects_map (cache_dns, MAP_CACHE_MAX);

						cache_dns[dns_hash] = _r_obj2_allocate (ptr_cache);
					}

					return TRUE;
				}
			}

			return TRUE;
		}
	}

	return FALSE;
}

BOOLEAN _app_parserulestring (rstring rule, PITEM_ADDRESS ptr_addr)
{
	_r_str_trim (rule, DIVIDER_TRIM); // trim whitespace

	if (rule.IsEmpty ())
		return TRUE;

	if (!_app_isrulevalidchars (rule.GetString ()))
		return FALSE;

	ENUM_TYPE_DATA type = DataUnknown;
	SIZE_T range_pos = _r_str_find (rule.GetString (), rule.GetLength (), DIVIDER_RULE_RANGE, 0);
	BOOLEAN is_range = (range_pos != INVALID_SIZE_T);

	WCHAR range_start[LEN_IP_MAX] = {0};
	WCHAR range_end[LEN_IP_MAX] = {0};

	if (is_range)
	{
		_r_str_copy (range_start, RTL_NUMBER_OF (range_start), _r_str_extract (rule.GetString (), rule.GetLength (), 0, range_pos).GetString ());
		_r_str_copy (range_end, RTL_NUMBER_OF (range_end), _r_str_extract (rule.GetString (), rule.GetLength (), range_pos + 1).GetString ());
	}

	// auto-parse rule type
	{
		SIZE_T rule_hash = _r_str_hash (rule.GetString ());
		BOOLEAN is_exists = cache_types.find (rule_hash) != cache_types.end ();

		if (is_exists)
		{
			type = cache_types[rule_hash];
		}
		else
		{
			if (_app_isruleport (rule.GetString ()))
			{
				type = DataTypePort;
			}
			else if (is_range ? (_app_isruleip (range_start) && _app_isruleip (range_end)) : _app_isruleip (rule.GetString ()))
			{
				type = DataTypeIp;
			}
			else if (_app_isrulehost (rule.GetString ()))
			{
				type = DataTypeHost;
			}

			if (type != DataUnknown)
			{
				if (cache_types.size () >= MAP_CACHE_MAX)
					cache_types.clear ();

				cache_types[rule_hash] = type;
			}
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
			ptr_addr->port = (UINT16)_r_str_touinteger (rule.GetString ());

			return TRUE;
		}
		else
		{
			// ...port range
			ptr_addr->type = DataTypePort;

			if (ptr_addr->prange)
			{
				ptr_addr->prange->valueLow.type = FWP_UINT16;
				ptr_addr->prange->valueLow.uint16 = (UINT16)wcstoul (range_start, NULL, 10);

				ptr_addr->prange->valueHigh.type = FWP_UINT16;
				ptr_addr->prange->valueHigh.uint16 = (UINT16)wcstoul (range_end, NULL, 10);
			}

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
					if (ptr_addr->prange)
					{
						ptr_addr->prange->valueLow.type = FWP_UINT32;
						ptr_addr->prange->valueLow.uint32 = addr4.addr;
					}
				}
				else if (format == NET_ADDRESS_IPV6)
				{
					if (ptr_addr->prange)
					{
						ptr_addr->prange->valueLow.type = FWP_BYTE_ARRAY16_TYPE;
						RtlCopyMemory (ptr_addr->prange->valueLow.byteArray16->byteArray16, addr6.addr, FWP_V6_ADDR_SIZE);
					}
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
					if (ptr_addr->prange)
					{
						ptr_addr->prange->valueHigh.type = FWP_UINT32;
						ptr_addr->prange->valueHigh.uint32 = addr4.addr;
					}
				}
				else if (format == NET_ADDRESS_IPV6)
				{
					if (ptr_addr->prange)
					{
						ptr_addr->prange->valueHigh.type = FWP_BYTE_ARRAY16_TYPE;
						RtlCopyMemory (ptr_addr->prange->valueHigh.byteArray16->byteArray16, addr6.addr, FWP_V6_ADDR_SIZE);
					}
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
			if (_app_parsenetworkstring (rule.GetString (), &format, &port2, &addr4, &addr6, ptr_addr->host, RTL_NUMBER_OF (ptr_addr->host)))
			{
				if (format == NET_ADDRESS_IPV4)
				{
					if (ptr_addr->paddr4)
					{
						ptr_addr->paddr4->mask = addr4.mask;
						ptr_addr->paddr4->addr = addr4.addr;
					}
				}
				else if (format == NET_ADDRESS_IPV6)
				{
					if (ptr_addr->paddr6)
					{
						ptr_addr->paddr6->prefixLength = addr6.prefixLength;
						RtlCopyMemory (ptr_addr->paddr6->addr, addr6.addr, FWP_V6_ADDR_SIZE);
					}
				}
				else if (format == NET_ADDRESS_DNS_NAME)
				{
					// ptr_addr->host = <hosts>;
				}
				else
				{
					return FALSE;
				}

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

BOOLEAN _app_resolveaddress (ADDRESS_FAMILY af, PVOID paddr, LPWSTR* pbuffer)
{
	if (af != AF_INET && af != AF_INET6)
		return FALSE;

	BOOLEAN result = FALSE;

	LPWSTR pstraddr = NULL;

	if (_app_formataddress (af, 0, paddr, 0, &pstraddr, FMTADDR_AS_ARPA))
	{
		SIZE_T arpa_hash = _r_str_hash (pstraddr);
		BOOLEAN is_exists = cache_arpa.find (arpa_hash) != cache_arpa.end ();

		if (is_exists)
		{
			PR_OBJECT ptr_cache_object = _r_obj2_reference (cache_arpa[arpa_hash]);

			if (ptr_cache_object)
			{
				if (ptr_cache_object->pdata)
					result = _r_str_alloc (pbuffer, _r_str_length ((LPCWSTR)ptr_cache_object->pdata), (LPCWSTR)ptr_cache_object->pdata);

				_r_obj2_dereference (ptr_cache_object);
			}
		}
		else
		{
			cache_arpa[arpa_hash] = NULL;

			PDNS_RECORD ppQueryResultsSet = NULL;
			DNS_STATUS dnsStatus = DnsQuery (pstraddr, DNS_TYPE_PTR, DNS_QUERY_NO_HOSTS_FILE, NULL, &ppQueryResultsSet, NULL);

			if (dnsStatus == DNS_ERROR_RCODE_NO_ERROR)
			{
				if (ppQueryResultsSet)
				{
					SIZE_T len = _r_str_length (ppQueryResultsSet->Data.PTR.pNameHost);
					result = _r_str_alloc (pbuffer, len, ppQueryResultsSet->Data.PTR.pNameHost);

					if (result)
					{
						LPWSTR ptr_cache = NULL;

						if (_r_str_alloc (&ptr_cache, len, ppQueryResultsSet->Data.PTR.pNameHost))
						{
							_app_freeobjects_map (cache_arpa, MAP_CACHE_MAX);

							cache_arpa[arpa_hash] = _r_obj2_allocate (ptr_cache);
						}
					}
				}
			}

			DnsRecordListFree (ppQueryResultsSet, DnsFreeRecordList);
		}
	}

	SAFE_DELETE_MEMORY (pstraddr);

	return result;
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

	else if (type == DataRuleCustom)
		return IDC_RULES_CUSTOM;

	return 0;
}

HBITMAP _app_bitmapfromico (HICON hicon, INT icon_size)
{
	if (!hicon)
		return NULL;

	RECT iconRectangle;
	SetRect (&iconRectangle, 0, 0, icon_size, icon_size);

	HBITMAP hbitmap = NULL;
	HDC screenHdc = GetDC (NULL);

	if (screenHdc)
	{
		HDC hdc = CreateCompatibleDC (screenHdc);

		if (hdc)
		{
			BITMAPINFO bitmapInfo = {0};
			bitmapInfo.bmiHeader.biSize = sizeof (bitmapInfo);
			bitmapInfo.bmiHeader.biPlanes = 1;
			bitmapInfo.bmiHeader.biCompression = BI_RGB;

			bitmapInfo.bmiHeader.biWidth = icon_size;
			bitmapInfo.bmiHeader.biHeight = icon_size;
			bitmapInfo.bmiHeader.biBitCount = 32;

			hbitmap = CreateDIBSection (hdc, &bitmapInfo, DIB_RGB_COLORS, NULL, NULL, 0);

			if (hbitmap)
			{
				HGDIOBJ oldBitmap = SelectObject (hdc, hbitmap);

				BLENDFUNCTION blendFunction = {0};
				blendFunction.BlendOp = AC_SRC_OVER;
				blendFunction.AlphaFormat = AC_SRC_ALPHA;
				blendFunction.SourceConstantAlpha = 255;

				BP_PAINTPARAMS paintParams = {0};
				paintParams.cbSize = sizeof (paintParams);
				paintParams.dwFlags = BPPF_ERASE;
				paintParams.pBlendFunction = &blendFunction;

				HDC bufferHdc = NULL;

				HPAINTBUFFER paintBuffer = BeginBufferedPaint (hdc, &iconRectangle, BPBF_DIB, &paintParams, &bufferHdc);

				if (paintBuffer)
				{
					DrawIconEx (bufferHdc, 0, 0, hicon, icon_size, icon_size, 0, NULL, DI_NORMAL);
					EndBufferedPaint (paintBuffer, TRUE);
				}
				else
				{
					_r_dc_fillrect (hdc, &iconRectangle, GetSysColor (COLOR_MENU));
					DrawIconEx (hdc, 0, 0, hicon, icon_size, icon_size, 0, NULL, DI_NORMAL);
				}

				SelectObject (hdc, oldBitmap);
			}

			SAFE_DELETE_DC (hdc);
		}

		ReleaseDC (NULL, screenHdc);
	}

	return hbitmap;
}

HBITMAP _app_bitmapfrompng (HINSTANCE hinst, LPCWSTR name, INT icon_size)
{
	BOOLEAN success = FALSE;

	UINT frameCount = 0;
	ULONG resourceLength = 0;
	HDC screenHdc = NULL;
	HDC hdc = NULL;
	BITMAPINFO bi = {0};
	HBITMAP hbitmap = NULL;
	PVOID bitmapBuffer = NULL;
	IWICStream* wicStream = NULL;
	IWICBitmapSource* wicBitmapSource = NULL;
	IWICBitmapDecoder* wicDecoder = NULL;
	IWICBitmapFrameDecode* wicFrame = NULL;
	IWICImagingFactory* wicFactory = NULL;
	IWICBitmapScaler* wicScaler = NULL;
	WICPixelFormatGUID pixelFormat;
	WICRect rect = {0, 0, icon_size, icon_size};

	// Create the ImagingFactory
	if (FAILED (CoCreateInstance (CLSID_WICImagingFactory1, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, (PVOID*)&wicFactory)))
		goto DoExit;

	// Load the resource
	WICInProcPointer resourceBuffer = (WICInProcPointer)_r_loadresource (hinst, name, L"PNG", &resourceLength);

	if (!resourceBuffer)
		goto DoExit;

	// Create the Stream
	if (FAILED (wicFactory->CreateStream (&wicStream)))
		goto DoExit;

	// Initialize the Stream from Memory
	if (FAILED (wicStream->InitializeFromMemory (resourceBuffer, resourceLength)))
		goto DoExit;

	if (FAILED (wicFactory->CreateDecoder (GUID_ContainerFormatPng, NULL, &wicDecoder)))
		goto DoExit;

	if (FAILED (wicDecoder->Initialize ((IStream*)wicStream, WICDecodeMetadataCacheOnLoad)))
		goto DoExit;

	// Get the Frame count
	if (FAILED (wicDecoder->GetFrameCount (&frameCount)) || frameCount < 1)
		goto DoExit;

	// Get the Frame
	if (FAILED (wicDecoder->GetFrame (0, &wicFrame)))
		goto DoExit;

	// Get the WicFrame image format
	if (FAILED (wicFrame->GetPixelFormat (&pixelFormat)))
		goto DoExit;

	// Check if the image format is supported:
	if (RtlEqualMemory (&pixelFormat, &GUID_WICPixelFormat32bppPRGBA, sizeof (GUID)))
	{
		wicBitmapSource = (IWICBitmapSource*)wicFrame;
	}
	else
	{
		IWICFormatConverter* wicFormatConverter = NULL;

		if (FAILED (wicFactory->CreateFormatConverter (&wicFormatConverter)))
			goto DoExit;

		if (FAILED (wicFormatConverter->Initialize (
			(IWICBitmapSource*)wicFrame,
			GUID_WICPixelFormat32bppPBGRA,
			WICBitmapDitherTypeNone,
			NULL,
			0.0,
			WICBitmapPaletteTypeCustom
			)))
		{
			wicFormatConverter->Release ();
			goto DoExit;
		}

		// Convert the image to the correct format:
		wicFormatConverter->QueryInterface (&wicBitmapSource);

		// Cleanup the converter.
		wicFormatConverter->Release ();

		// Dispose the old frame now that the converted frame is in wicBitmapSource.
		wicFrame->Release ();
	}

	bi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = rect.Width;
	bi.bmiHeader.biHeight = -((LONG)rect.Height);
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;

	screenHdc = GetDC (NULL);
	hdc = CreateCompatibleDC (screenHdc);
	hbitmap = CreateDIBSection (screenHdc, &bi, DIB_RGB_COLORS, &bitmapBuffer, NULL, 0);

	if (FAILED (wicFactory->CreateBitmapScaler (&wicScaler)))
		goto DoExit;

	if (FAILED (wicScaler->Initialize (wicBitmapSource, rect.Width, rect.Height, WICBitmapInterpolationModeFant)))
		goto DoExit;

	if (FAILED (wicScaler->CopyPixels (&rect, rect.Width * 4, rect.Width * rect.Height * 4, (LPBYTE)bitmapBuffer)))
		goto DoExit;

	success = TRUE;

DoExit:

	if (wicScaler)
		wicScaler->Release ();

	SAFE_DELETE_DC (hdc);

	if (screenHdc)
		ReleaseDC (NULL, screenHdc);

	if (wicBitmapSource)
		wicBitmapSource->Release ();

	if (wicStream)
		wicStream->Release ();

	if (wicDecoder)
		wicDecoder->Release ();

	if (wicFactory)
		wicFactory->Release ();

	if (!success)
	{
		SAFE_DELETE_OBJECT (hbitmap);

		return NULL;
	}

	return hbitmap;
}

VOID _app_load_appxmanifest (PITEM_APP_HELPER ptr_app_item)
{
	if (_r_str_isempty (ptr_app_item->real_path))
		return;

	rstring result;
	rstring path;

	LPCWSTR appx_names[] = {
		L"AppxManifest.xml",
		L"VSAppxManifest.xml",
	};

	for (SIZE_T i = 0; i < RTL_NUMBER_OF (appx_names); i++)
	{
		path.Format (L"%s\\%s", ptr_app_item->real_path, appx_names[i]);

		if (_r_fs_exists (path.GetString ()))
			goto DoOpen;
	}

	return;

DoOpen:

	pugi::xml_document doc;
	pugi::xml_parse_result xml_result = doc.load_file (path.GetString (), PUGIXML_LOAD_FLAGS, PUGIXML_LOAD_ENCODING);

	if (xml_result)
	{
		pugi::xml_node root = doc.child (L"Package");

		if (root)
		{
			pugi::xml_node xml_applications = root.child (L"Applications");

			for (pugi::xml_node item = xml_applications.child (L"Application"); item; item = item.next_sibling (L"Application"))
			{
				if (!item.attribute (L"Executable").empty ())
				{
					result.Format (L"%s\\%s", ptr_app_item->real_path, item.attribute (L"Executable").as_string ());

					if (_r_fs_exists (result.GetString ()))
						break;
				}
			}
		}
	}

	_r_str_alloc (&ptr_app_item->real_path, result.GetLength (), result.GetString ());
}

