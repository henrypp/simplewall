// simplewall
// Copyright (c) 2016-2019 Henry++

#include "global.hpp"

UINT _app_gettab_id (HWND hwnd, size_t page_id)
{
	TCITEM tci = {0};

	tci.mask = TCIF_PARAM;

	SendDlgItemMessage (hwnd, IDC_TAB, TCM_GETITEM, ((page_id == LAST_VALUE) ? SendDlgItemMessage (hwnd, IDC_TAB, TCM_GETCURSEL, 0, 0) : page_id), (LPARAM)& tci);

	return (UINT)tci.lParam;
}

void _app_settab_id (HWND hwnd, size_t page_id)
{
	for (size_t i = 0; i < (size_t)SendDlgItemMessage (hwnd, IDC_TAB, TCM_GETITEMCOUNT, 0, 0); i++)
	{
		const UINT listview_id = _app_gettab_id (hwnd, i);

		if (listview_id == page_id)
		{
			SendDlgItemMessage (hwnd, IDC_TAB, TCM_SETCURSEL, i, 0);

			NMHDR hdr = {0};

			hdr.code = TCN_SELCHANGE;
			hdr.idFrom = IDC_TAB;

			SendMessage (hwnd, WM_NOTIFY, 0, (LPARAM)& hdr);

			break;
		}
	}
}

void _app_applycasestyle (LPWSTR buffer, size_t length)
{
	if (buffer && length && wcschr (buffer, OBJ_NAME_PATH_SEPARATOR))
	{
		buffer[0] = _r_str_upper (buffer[0]);

		for (size_t i = 1; i < length; i++)
			buffer[i] = _r_str_lower (buffer[i]);
	}
}

bool _app_formataddress (PITEM_LOG const ptr_log, FWP_DIRECTION dir, UINT16 port, LPWSTR * ptr_dest, bool is_appenddns)
{
	if (!ptr_log || !ptr_dest)
		return false;

	bool result = false;

	WCHAR formatted_address[512] = {0};

	PIN_ADDR addrv4 = nullptr;
	PIN6_ADDR addrv6 = nullptr;

	if (ptr_log->af == AF_INET)
	{
		addrv4 = (dir == FWP_DIRECTION_OUTBOUND || dir == FWP_DIRECTION_OUT) ? &ptr_log->remote_addr : &ptr_log->local_addr;

		if (InetNtop (AF_INET, addrv4, formatted_address, _countof (formatted_address)))
			result = !IN4_IS_ADDR_UNSPECIFIED (addrv4);
	}
	else if (ptr_log->af == AF_INET6)
	{
		addrv6 = (dir == FWP_DIRECTION_OUTBOUND || dir == FWP_DIRECTION_OUT) ? &ptr_log->remote_addr6 : &ptr_log->local_addr6;

		if (InetNtop (AF_INET6, addrv6, formatted_address, _countof (formatted_address)))
			result = !IN6_IS_ADDR_UNSPECIFIED (addrv6);
	}

	if (port)
		StringCchCat (formatted_address, _countof (formatted_address), _r_fmt (L":%d", port));

	if (result && is_appenddns && app.ConfigGet (L"IsNetworkResolutionsEnabled", false).AsBool () && config.is_wsainit)
	{
		const size_t hash = _r_str_hash (formatted_address);

		if (hash)
		{
			_r_fastlock_acquireshared (&lock_cache);
			const bool is_exists = cache_hosts.find (hash) != cache_hosts.end ();
			_r_fastlock_releaseshared (&lock_cache);

			if (is_exists)
			{
				_r_fastlock_acquireshared (&lock_cache);

				if (cache_hosts[hash])
					StringCchCat (formatted_address, _countof (formatted_address), _r_fmt (L" (%s)", cache_hosts[hash]));

				_r_fastlock_releaseshared (&lock_cache);
			}
			else
			{
				WCHAR hostBuff[NI_MAXHOST] = {0};
				LPWSTR cache_ptr = nullptr;

				if (_app_resolveaddress (ptr_log->af, (ptr_log->af == AF_INET) ? (LPVOID)addrv4 : (LPVOID)addrv6, hostBuff, _countof (hostBuff)))
				{
					StringCchCat (formatted_address, _countof (formatted_address), _r_fmt (L" (%s)", hostBuff));

					_r_str_alloc (&cache_ptr, _r_str_length (hostBuff), hostBuff);
				}

				_r_fastlock_acquireexclusive (&lock_cache);

				_app_freecache (&cache_hosts);

				cache_hosts[hash] = cache_ptr;

				_r_fastlock_releaseexclusive (&lock_cache);
			}
		}
	}

	_r_str_alloc (ptr_dest, _r_str_length (formatted_address), formatted_address);

	return result;
}

void _app_freearray (std::vector<PITEM_ADD> * ptr)
{
	if (!ptr)
		return;

	for (size_t i = 0; i < ptr->size (); i++)
	{
		PITEM_ADD ptr_item = ptr->at (i);

		SAFE_DELETE (ptr_item);
	}

	ptr->clear ();
}

void _app_freecache (MCACHE_MAP * ptr_map)
{
	if (ptr_map->size () <= UMAP_CACHE_LIMIT)
		return;

	for (auto &p : *ptr_map)
	{
		LPWSTR ptr_buffer = p.second;

		SAFE_DELETE_ARRAY (ptr_buffer);
	}

	ptr_map->clear ();
}

void _app_freethreadpool (MTHREADPOOL * ptr_pool)
{
	if (!ptr_pool || ptr_pool->empty ())
		return;

	const size_t count = ptr_pool->size ();

	for (size_t i = (count - 1); i != LAST_VALUE; i--)
	{
		const HANDLE hthread = ptr_pool->at (i);

		if (WaitForSingleObjectEx (hthread, 0, FALSE) == WAIT_OBJECT_0)
		{
			CloseHandle (hthread);
			ptr_pool->erase (ptr_pool->begin () + i);
		}
	}
}

void _app_freelogstack ()
{
	while (true)
	{
		const PSLIST_ENTRY listEntry = RtlInterlockedPopEntrySList (&log_stack.ListHead);

		if (!listEntry)
			break;

		InterlockedDecrement (&log_stack.item_count);

		PITEM_LIST_ENTRY ptr_entry = CONTAINING_RECORD (listEntry, ITEM_LIST_ENTRY, ListEntry);

		SAFE_DELETE (ptr_entry->Body);

		_aligned_free (ptr_entry);
	}
}

void _app_getappicon (ITEM_APP const *ptr_app, bool is_small, size_t * picon_id, HICON * picon)
{
	const bool is_iconshidden = app.ConfigGet (L"IsIconsHidden", false).AsBool ();

	if (ptr_app->type == AppRegular || ptr_app->type == AppService)
	{
		if (is_iconshidden || !_app_getfileicon (ptr_app->real_path, is_small, picon_id, picon))
		{
			if (picon_id)
				*picon_id = config.icon_id;

			if (picon)
				*picon = CopyIcon (is_small ? config.hicon_small : config.hicon_large);
		}
	}
	else if (ptr_app->type == AppPackage)
	{
		if (picon_id)
			*picon_id = config.icon_package_id;

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

void _app_getdisplayname (size_t hash, ITEM_APP const *ptr_app, LPWSTR * extracted_name)
{
	if (!extracted_name)
		return;

	if (ptr_app->type == AppService)
	{
		_r_str_alloc (extracted_name, _r_str_length (ptr_app->original_path), ptr_app->original_path);
	}
	else if (ptr_app->type == AppPackage)
	{
		rstring name;

		if (!_app_item_get (ptr_app->type, hash, &name, nullptr, nullptr, nullptr, nullptr))
			name = ptr_app->original_path;

		_r_str_alloc (extracted_name, name.GetLength (), name);
	}
	else
	{
		LPCWSTR ptr_path = ((hash == config.ntoskrnl_hash) ? ptr_app->original_path : ptr_app->real_path);

		if (app.ConfigGet (L"ShowFilenames", true).AsBool ())
		{
			const rstring path = _r_path_extractfile (ptr_path);

			_r_str_alloc (extracted_name, path.GetLength (), path);
		}
		else
		{
			_r_str_alloc (extracted_name, _r_str_length (ptr_path), ptr_path);
		}
	}
}

bool _app_getfileicon (LPCWSTR path, bool is_small, size_t * picon_id, HICON * picon)
{
	if (!path || !_r_fs_exists (path) || (!picon_id && !picon))
		return false;

	bool result = false;

	SHFILEINFO shfi = {0};
	DWORD flags = SHGFI_USEFILEATTRIBUTES;

	if (picon_id)
		flags |= SHGFI_SYSICONINDEX;

	if (picon)
		flags |= SHGFI_ICON;

	if (is_small)
		flags |= SHGFI_SMALLICON;

	const HRESULT hrComInit = CoInitialize (nullptr);

	if ((hrComInit == RPC_E_CHANGED_MODE) || SUCCEEDED (hrComInit))
	{
		if (SHGetFileInfo (path, FILE_ATTRIBUTE_NORMAL, &shfi, sizeof (shfi), flags))
		{
			if (picon_id)
				*picon_id = (size_t)shfi.iIcon;

			if (picon && shfi.hIcon)
			{
				*picon = CopyIcon (shfi.hIcon);
				DestroyIcon (shfi.hIcon);
			}

			result = true;
		}

		if (SUCCEEDED (hrComInit))
			CoUninitialize ();
	}

	return result;
}

rstring _app_getshortcutpath (HWND hwnd, LPCWSTR path)
{
	rstring result;

	IShellLink *psl = nullptr;

	const HRESULT hrComInit = CoInitializeEx (nullptr, COINIT_MULTITHREADED);

	if ((hrComInit == RPC_E_CHANGED_MODE) || SUCCEEDED (hrComInit))
	{
		if (SUCCEEDED (CoInitializeSecurity (nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, 0, nullptr)))
		{
			if (SUCCEEDED (CoCreateInstance (CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLink, (void **)& psl)))
			{
				IPersistFile *ppf = nullptr;

				if (SUCCEEDED (psl->QueryInterface (IID_IPersistFile, (void **)& ppf)))
				{
					if (SUCCEEDED (ppf->Load (path, STGM_READ)))
					{
						if (SUCCEEDED (psl->Resolve (hwnd, 0)))
						{
							WIN32_FIND_DATA wfd = {0};
							WCHAR buffer[MAX_PATH] = {0};

							if (SUCCEEDED (psl->GetPath (buffer, _countof (buffer), (LPWIN32_FIND_DATA)& wfd, SLGP_RAWPATH)))
								result = buffer;
						}
					}

					ppf->Release ();
				}

				psl->Release ();
			}
		}

		if (SUCCEEDED (hrComInit))
			CoUninitialize ();
	}

	return result;
}

bool _app_getsignatureinfo (size_t hash, LPCWSTR path, LPWSTR * psigner)
{
	if (!app.ConfigGet (L"IsCerificatesEnabled", false).AsBool ())
		return false;

	if (!psigner || !path || !path[0])
		return false;

	_r_fastlock_acquireshared (&lock_cache);
	const bool is_exists = cache_signatures.find (hash) != cache_signatures.end ();
	_r_fastlock_releaseshared (&lock_cache);

	if (is_exists)
	{
		_r_fastlock_acquireshared (&lock_cache);

		LPCWSTR ptr_text = cache_signatures[hash];

		_r_str_alloc (psigner, _r_str_length (ptr_text), ptr_text);

		_r_fastlock_releaseshared (&lock_cache);

		return (*psigner != nullptr);
	}

	bool result = false;

	cache_signatures[hash] = nullptr;

	const HANDLE hfile = CreateFile (path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);

	if (hfile != INVALID_HANDLE_VALUE)
	{
		static GUID WinTrustActionGenericVerifyV2 = WINTRUST_ACTION_GENERIC_VERIFY_V2;

		WINTRUST_FILE_INFO fileInfo = {0};

		fileInfo.cbStruct = sizeof (fileInfo);
		fileInfo.pcwszFilePath = path;
		fileInfo.hFile = hfile;

		WINTRUST_DATA trustData = {0};

		trustData.cbStruct = sizeof (trustData);
		trustData.dwUIChoice = WTD_UI_NONE;
		trustData.fdwRevocationChecks = WTD_REVOKE_NONE;
		trustData.dwProvFlags = WTD_SAFER_FLAG | WTD_CACHE_ONLY_URL_RETRIEVAL;
		trustData.dwUnionChoice = WTD_CHOICE_FILE;
		trustData.pFile = &fileInfo;

		trustData.dwStateAction = WTD_STATEACTION_VERIFY;
		const LONG status = WinVerifyTrust ((HWND)INVALID_HANDLE_VALUE, &WinTrustActionGenericVerifyV2, &trustData);

		if (status == S_OK)
		{
			PCRYPT_PROVIDER_DATA provData = WTHelperProvDataFromStateData (trustData.hWVTStateData);

			if (provData)
			{
				PCRYPT_PROVIDER_SGNR psProvSigner = WTHelperGetProvSignerFromChain (provData, 0, FALSE, 0);

				if (psProvSigner)
				{
					CRYPT_PROVIDER_CERT *psProvCert = WTHelperGetProvCertFromChain (psProvSigner, 0);

					if (psProvCert)
					{
						const DWORD num_chars = CertGetNameString (psProvCert->pCert, CERT_NAME_ATTR_TYPE, 0, szOID_COMMON_NAME, nullptr, 0) + 1;

						if (num_chars > 1)
						{
							LPWSTR ptr_text = new WCHAR[num_chars];

							if (ptr_text)
							{
								if (!CertGetNameString (psProvCert->pCert, CERT_NAME_ATTR_TYPE, 0, szOID_COMMON_NAME, ptr_text, num_chars))
									SAFE_DELETE_ARRAY (ptr_text);
							}

							_r_fastlock_acquireexclusive (&lock_cache);

							_app_freecache (&cache_signatures);

							_r_str_alloc (psigner, _r_str_length (ptr_text), ptr_text);
							cache_signatures[hash] = ptr_text;

							_r_fastlock_releaseexclusive (&lock_cache);
						}
					}
				}
			}

			result = true;
		}

		trustData.dwStateAction = WTD_STATEACTION_CLOSE;
		WinVerifyTrust ((HWND)INVALID_HANDLE_VALUE, &WinTrustActionGenericVerifyV2, &trustData);

		CloseHandle (hfile);
	}

	return result;
}

bool _app_getversioninfo (size_t hash, LPCWSTR path, LPWSTR * pinfo)
{
	if (!path || !pinfo)
		return false;

	_r_fastlock_acquireshared (&lock_cache);
	const bool is_exists = cache_versions.find (hash) != cache_versions.end ();
	_r_fastlock_releaseshared (&lock_cache);

	if (is_exists)
	{
		_r_fastlock_acquireshared (&lock_cache);

		LPCWSTR ptr_text = cache_versions[hash];

		_r_str_alloc (pinfo, _r_str_length (ptr_text), ptr_text);

		_r_fastlock_releaseshared (&lock_cache);

		return (*pinfo != nullptr);
	}

	bool result = false;
	rstring buffer;

	cache_versions[hash] = nullptr;

	const HINSTANCE hlib = LoadLibraryEx (path, nullptr, LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE);

	if (hlib)
	{
		const HRSRC hres = FindResource (hlib, MAKEINTRESOURCE (VS_VERSION_INFO), RT_VERSION);

		if (hres)
		{
			const HGLOBAL hglob = LoadResource (hlib, hres);

			if (hglob)
			{
				const LPVOID versionInfo = LockResource (hglob);

				if (versionInfo)
				{
					UINT vLen = 0, langD = 0;
					LPVOID retbuf = nullptr;

					WCHAR author_entry[128] = {0};
					WCHAR description_entry[128] = {0};

					if (VerQueryValue (versionInfo, L"\\VarFileInfo\\Translation", &retbuf, &vLen) && vLen == 4)
					{
						CopyMemory (&langD, retbuf, vLen);
						StringCchPrintf (author_entry, _countof (author_entry), L"\\StringFileInfo\\%02X%02X%02X%02X\\CompanyName", (langD & 0xff00) >> 8, langD & 0xff, (langD & 0xff000000) >> 24, (langD & 0xff0000) >> 16);
						StringCchPrintf (description_entry, _countof (description_entry), L"\\StringFileInfo\\%02X%02X%02X%02X\\FileDescription", (langD & 0xff00) >> 8, langD & 0xff, (langD & 0xff000000) >> 24, (langD & 0xff0000) >> 16);
					}
					else
					{
						StringCchPrintf (author_entry, _countof (author_entry), L"\\StringFileInfo\\%04X04B0\\CompanyName", GetUserDefaultLangID ());
						StringCchPrintf (description_entry, _countof (description_entry), L"\\StringFileInfo\\%04X04B0\\FileDescription", GetUserDefaultLangID ());
					}

					if (VerQueryValue (versionInfo, description_entry, &retbuf, &vLen))
					{
						buffer.Append (SZ_TAB);
						buffer.Append (LPCWSTR (retbuf));

						UINT length = 0;
						VS_FIXEDFILEINFO *verInfo = nullptr;

						if (VerQueryValue (versionInfo, L"\\", (void **)(&verInfo), &length))
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

					if (VerQueryValue (versionInfo, author_entry, &retbuf, &vLen))
					{
						buffer.Append (SZ_TAB);
						buffer.Append (static_cast<LPCWSTR>(retbuf));
						buffer.Append (L"\r\n");
					}

					buffer.Trim (L"\r\n ");

					// get signature information
					{
						LPWSTR ptr_text = nullptr;

						_r_str_alloc (&ptr_text, buffer.GetLength (), buffer);
						_r_str_alloc (pinfo, buffer.GetLength (), buffer);

						_r_fastlock_acquireexclusive (&lock_cache);

						_app_freecache (&cache_versions);

						cache_versions[hash] = ptr_text;

						_r_fastlock_releaseexclusive (&lock_cache);
					}

					result = true;
				}

				FreeResource (hglob);
			}
		}

		FreeLibrary (hlib);
	}

	return result;
}

size_t _app_getposition (HWND hwnd, size_t hash)
{
	const UINT listview_id = _app_gettab_id (hwnd);

	for (size_t i = 0; i < _r_listview_getitemcount (hwnd, listview_id); i++)
	{
		if ((size_t)_r_listview_getitemlparam (hwnd, listview_id, i) == hash)
			return i;
	}

	return LAST_VALUE;
}

rstring _app_getprotoname (UINT8 proto)
{
	for (size_t i = 0; i < protocols.size (); i++)
	{
		PITEM_PROTOCOL const ptr_proto = protocols.at (i);

		if (ptr_proto && proto == ptr_proto->id)
			return ptr_proto->pname;
	}

	return SZ_EMPTY;
}

void _app_generate_packages ()
{
	HKEY hkey = nullptr;
	HKEY hsubkey = nullptr;

	LONG result = RegOpenKeyEx (HKEY_CLASSES_ROOT, L"Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages", 0, KEY_READ, &hkey);

	if (result != ERROR_SUCCESS)
	{
		if (result != ERROR_FILE_NOT_FOUND)
			_app_logerror (L"RegOpenKeyEx", result, nullptr, true);
	}
	else
	{
		DWORD index = 0;

		while (true)
		{
			rstring package_sid_string;

			WCHAR key_name[MAX_PATH] = {0};

			PSID package_sid[SECURITY_MAX_SID_SIZE] = {0};

			DWORD size = _countof (key_name) * sizeof (key_name[0]);

			if (RegEnumKeyEx (hkey, index++, key_name, &size, 0, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
				break;

			result = RegOpenKeyEx (hkey, key_name, 0, KEY_READ, &hsubkey);

			if (result != ERROR_SUCCESS)
			{
				if (result != ERROR_FILE_NOT_FOUND)
					_app_logerror (L"RegOpenKeyEx", result, key_name, true);
			}
			else
			{
				size = _countof (package_sid) * sizeof (package_sid[0]);
				result = RegQueryValueEx (hsubkey, L"PackageSid", nullptr, nullptr, (LPBYTE)package_sid, &size);

				if (result != ERROR_SUCCESS)
				{
					if (result != ERROR_FILE_NOT_FOUND)
						_app_logerror (L"RegQueryValueEx", result, key_name, true);

					continue;
				}

				package_sid_string = _r_str_fromsid (package_sid);

				const size_t hash = _r_str_hash (package_sid_string);

				WCHAR display_name[MAX_PATH] = {0};
				WCHAR path[MAX_PATH] = {0};

				PITEM_ADD ptr_item = new ITEM_ADD;

				ptr_item->hash = hash;

				size = _countof (display_name) * sizeof (display_name[0]);
				result = RegQueryValueEx (hsubkey, L"DisplayName", nullptr, nullptr, (LPBYTE)display_name, &size);

				if (result == ERROR_SUCCESS)
				{
					if (display_name[0] == L'@')
					{
						if (!SUCCEEDED (SHLoadIndirectString (rstring (display_name), display_name, _countof (display_name), nullptr)) || !display_name[0])
							StringCchCopy (display_name, _countof (display_name), key_name[0] ? key_name : package_sid_string);
					}
				}

				size = _countof (path) * sizeof (path[0]);
				RegQueryValueEx (hsubkey, L"PackageRootFolder", nullptr, nullptr, (LPBYTE)path, &size);

				// query timestamp
				FILETIME ft = {0};

				if (RegQueryInfoKey (hsubkey, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &ft) == ERROR_SUCCESS)
					ptr_item->timestamp = _r_unixtime_from_filetime (&ft);

				ptr_item->type = AppPackage;

				_r_str_alloc (&ptr_item->sid, package_sid_string.GetLength (), package_sid_string);
				_r_str_alloc (&ptr_item->display_name, _r_str_length (display_name), display_name);
				_r_str_alloc (&ptr_item->real_path, _r_str_length (path), path);

				ConvertStringSidToSid (package_sid_string, &ptr_item->psid);

				_app_load_appxmanifest (ptr_item);

				items.push_back (ptr_item);

				RegCloseKey (hsubkey);
			}
		}

		RegCloseKey (hkey);
	}
}

//void _app_generate_processes ()
//{
//	_app_freearray (&processes);
//
//	NTSTATUS status = 0;
//
//	ULONG length = 0x4000;
//	PBYTE buffer = new BYTE[length];
//
//	while (true)
//	{
//		status = NtQuerySystemInformation (SystemProcessInformation, buffer, length, &length);
//
//		if (status == 0xC0000023L /*STATUS_BUFFER_TOO_SMALL*/ || status == 0xc0000004 /*STATUS_INFO_LENGTH_MISMATCH*/)
//		{
//			SAFE_DELETE_ARRAY (buffer);
//
//			buffer = new BYTE[length];
//		}
//		else
//		{
//			break;
//		}
//	}
//
//	if (NT_SUCCESS (status))
//	{
//		PSYSTEM_PROCESS_INFORMATION spi = (PSYSTEM_PROCESS_INFORMATION)buffer;
//
//		std::unordered_map<size_t, bool> checker;
//
//		do
//		{
//			const DWORD pid = (DWORD)(DWORD_PTR)spi->UniqueProcessId;
//
//			if (!pid) // skip "system idle process"
//				continue;
//
//			const HANDLE hprocess = OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
//
//			if (hprocess)
//			{
//				WCHAR display_name[MAX_PATH] = {0};
//				WCHAR real_path[MAX_PATH] = {0};
//
//				size_t hash = 0;
//
//				StringCchPrintf (display_name, _countof (display_name), L"%s (%lu)", spi->ImageName.Buffer, pid);
//
//				if (pid == PROC_SYSTEM_PID)
//				{
//					StringCchCopy (real_path, _countof (real_path), _r_path_expand (PATH_NTOSKRNL));
//					hash = _r_str_hash (spi->ImageName.Buffer);
//				}
//				else
//				{
//					DWORD size = _countof (real_path) - 1;
//
//					if (QueryFullProcessImageName (hprocess, 0, real_path, &size))
//					{
//						_app_applycasestyle (real_path, _r_str_length (real_path)); // apply case-style
//						hash = _r_str_hash (real_path);
//					}
//					else
//					{
//						// cannot get file path because it's not filesystem process (Pico maybe?)
//						if (GetLastError () == ERROR_GEN_FAILURE)
//						{
//							StringCchCopy (real_path, _countof (real_path), spi->ImageName.Buffer);
//							hash = _r_str_hash (spi->ImageName.Buffer);
//						}
//						else
//						{
//							CloseHandle (hprocess);
//							continue;
//						}
//					}
//				}
//
//				if (hash && apps.find (hash) == apps.end () && checker.find (hash) == checker.end ())
//				{
//					checker[hash] = true;
//
//					PITEM_ADD ptr_item = new ITEM_ADD;
//
//					ptr_item->hash = hash;
//
//					_r_str_alloc (&ptr_item->display_name, _r_str_length (display_name), display_name);
//					_r_str_alloc (&ptr_item->real_path, _r_str_length (((pid == PROC_SYSTEM_PID) ? PROC_SYSTEM_NAME : real_path)), ((pid == PROC_SYSTEM_PID) ? PROC_SYSTEM_NAME : real_path));
//
//					// get file icon
//					{
//						HICON hicon = nullptr;
//
//						if (!app.ConfigGet (L"IsIconsHidden", false).AsBool () && _app_getfileicon (real_path, true, nullptr, &hicon))
//						{
//							ptr_item->hbmp = _app_ico2bmp (hicon);
//							DestroyIcon (hicon);
//						}
//					}
//
//					processes.push_back (ptr_item);
//				}
//
//				CloseHandle (hprocess);
//			}
//		}
//		while ((spi = ((spi->NextEntryOffset ? (PSYSTEM_PROCESS_INFORMATION)((PCHAR)(spi)+(spi)->NextEntryOffset) : nullptr))) != nullptr);
//
//		std::sort (processes.begin (), processes.end (),
//			[](const PITEM_ADD& a, const PITEM_ADD& b)->bool {
//			return StrCmpLogicalW (a->display_name, b->display_name) == -1;
//		});
//	}
//
//	SAFE_DELETE_ARRAY (buffer); // free the allocated buffer
//}

void _app_generate_services ()
{
	const SC_HANDLE hsvcmgr = OpenSCManager (nullptr, nullptr, SC_MANAGER_ALL_ACCESS);

	if (hsvcmgr)
	{
		ENUM_SERVICE_STATUS service;

		DWORD dwBytesNeeded = 0;
		DWORD dwServicesReturned = 0;
		DWORD dwResumedHandle = 0;
		DWORD dwServiceType = SERVICE_WIN32_OWN_PROCESS | SERVICE_WIN32_SHARE_PROCESS;
		const DWORD dwServiceState = SERVICE_STATE_ALL;

		// win10+
		if (_r_sys_validversion (10, 0))
			dwServiceType |= SERVICE_INTERACTIVE_PROCESS | SERVICE_USER_SERVICE | SERVICE_USERSERVICE_INSTANCE;

		if (!EnumServicesStatus (hsvcmgr, dwServiceType, dwServiceState, &service, sizeof (ENUM_SERVICE_STATUS), &dwBytesNeeded, &dwServicesReturned, &dwResumedHandle))
		{
			if (GetLastError () == ERROR_MORE_DATA)
			{
				// Set the buffer
				const DWORD dwBytes = sizeof (ENUM_SERVICE_STATUS) + dwBytesNeeded;
				LPENUM_SERVICE_STATUS pServices = new ENUM_SERVICE_STATUS[dwBytes];

				// Now query again for services
				if (EnumServicesStatus (hsvcmgr, dwServiceType, dwServiceState, (LPENUM_SERVICE_STATUS)pServices, dwBytes, &dwBytesNeeded, &dwServicesReturned, &dwResumedHandle))
				{
					// now traverse each service to get information
					for (DWORD i = 0; i < dwServicesReturned; i++)
					{
						LPENUM_SERVICE_STATUS psvc = (pServices + i);

						LPCWSTR display_name = psvc->lpDisplayName;
						LPCWSTR service_name = psvc->lpServiceName;

						WCHAR buffer[MAX_PATH] = {0};
						WCHAR real_path[MAX_PATH] = {0};

						time_t timestamp = 0;

						// get binary path
						const SC_HANDLE hsvc = OpenService (hsvcmgr, service_name, SERVICE_QUERY_CONFIG);

						if (hsvc)
						{
							LPQUERY_SERVICE_CONFIG lpqsc = {0};
							DWORD bytes_needed = 0;

							if (!QueryServiceConfig (hsvc, nullptr, 0, &bytes_needed))
							{
								lpqsc = new QUERY_SERVICE_CONFIG[bytes_needed];

								if (QueryServiceConfig (hsvc, lpqsc, bytes_needed, &bytes_needed))
								{
									FILETIME ft = {0};

									// query "ServiceDll" path
									{
										HKEY hkey = nullptr;

										if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, _r_fmt (L"System\\CurrentControlSet\\Services\\%s\\Parameters", service_name), 0, KEY_READ, &hkey) == ERROR_SUCCESS)
										{
											DWORD size = _countof (real_path) * sizeof (real_path[0]);

											if (RegQueryInfoKey (hkey, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &ft) == ERROR_SUCCESS)
												timestamp = _r_unixtime_from_filetime (&ft);

											if (RegQueryValueEx (hkey, L"ServiceDll", nullptr, nullptr, (LPBYTE)real_path, &size) == ERROR_SUCCESS)
												StringCchCopy (real_path, _countof (real_path), _r_path_expand (real_path));

											RegCloseKey (hkey);
										}
									}

									// query "lpftLastWriteTime"
									if (!timestamp)
									{
										HKEY hkey = nullptr;

										if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, _r_fmt (L"System\\CurrentControlSet\\Services\\%s", service_name), 0, KEY_READ, &hkey) == ERROR_SUCCESS)
										{
											if (RegQueryInfoKey (hkey, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &ft) == ERROR_SUCCESS)
												timestamp = _r_unixtime_from_filetime (&ft);

											RegCloseKey (hkey);
										}
									}

									// set path
									if (!real_path[0])
									{
										StringCchCopy (real_path, _countof (real_path), lpqsc->lpBinaryPathName);

										PathRemoveArgs (real_path);
										PathUnquoteSpaces (real_path);
									}

									_app_applycasestyle (real_path, _r_str_length (real_path)); // apply case-style
								}
								else
								{
									SAFE_DELETE_ARRAY (lpqsc);
									continue;
								}

								SAFE_DELETE_ARRAY (lpqsc);
							}

							CloseServiceHandle (hsvc);
						}

						UNICODE_STRING serviceNameUs = {0};

						serviceNameUs.Buffer = buffer;
						serviceNameUs.Length = (USHORT)(_r_str_length (service_name) * sizeof (WCHAR));
						serviceNameUs.MaximumLength = serviceNameUs.Length;

						StringCchCopy (buffer, _countof (buffer), service_name);

						SID *serviceSid = nullptr;
						ULONG serviceSidLength = 0;

						rstring sidstring;

						// get service security identifier
						if (RtlCreateServiceSid (&serviceNameUs, serviceSid, &serviceSidLength) == 0xC0000023 /*STATUS_BUFFER_TOO_SMALL*/)
						{
							serviceSid = new SID[serviceSidLength];

							if (serviceSid)
							{
								if (NT_SUCCESS (RtlCreateServiceSid (&serviceNameUs, serviceSid, &serviceSidLength)))
								{
									sidstring = _r_str_fromsid (serviceSid);
								}
								else
								{
									SAFE_DELETE_ARRAY (serviceSid);
								}
							}
						}

						if (serviceSid && !sidstring.IsEmpty ())
						{
							PITEM_ADD ptr_item = new ITEM_ADD;

							ptr_item->hash = _r_str_hash (service_name);
							ptr_item->timestamp = timestamp;
							ptr_item->type = AppService;

							_r_str_alloc (&ptr_item->service_name, _r_str_length (service_name), service_name);
							_r_str_alloc (&ptr_item->display_name, _r_str_length (display_name), display_name);
							_r_str_alloc (&ptr_item->real_path, _r_str_length (real_path), real_path);
							_r_str_alloc (&ptr_item->sid, sidstring.GetLength (), sidstring);

							if (!ConvertStringSecurityDescriptorToSecurityDescriptor (_r_fmt (SERVICE_SECURITY_DESCRIPTOR, sidstring.GetString ()).ToUpper (), SDDL_REVISION_1, &ptr_item->psd, nullptr))
							{
								_app_logerror (L"ConvertStringSecurityDescriptorToSecurityDescriptor", GetLastError (), service_name, false);

								SAFE_DELETE (ptr_item);
							}
							else
							{
								items.push_back (ptr_item);
							}
						}

						SAFE_DELETE_ARRAY (serviceSid);
					}

					SAFE_DELETE_ARRAY (pServices);
				}
			}
		}

		CloseServiceHandle (hsvcmgr);
	}
}

void _app_generate_rulesmenu (HMENU hsubmenu, size_t app_hash)
{
	static HBITMAP hbmp_allow = nullptr;
	static HBITMAP hbmp_block = nullptr;

	static HBITMAP hbmp_checked = nullptr;
	static HBITMAP hbmp_unchecked = nullptr;

	if (!hbmp_allow)
		hbmp_allow = _app_ico2bmp (app.GetSharedIcon (app.GetHINSTANCE (), IDI_ALLOW, GetSystemMetrics (SM_CXSMICON)));

	if (!hbmp_block)
		hbmp_block = _app_ico2bmp (app.GetSharedIcon (app.GetHINSTANCE (), IDI_BLOCK, GetSystemMetrics (SM_CXSMICON)));

	if (!hbmp_checked)
		hbmp_checked = _app_ico2bmp (app.GetSharedIcon (app.GetHINSTANCE (), IDI_CHECKED, GetSystemMetrics (SM_CXSMICON)));

	if (!hbmp_unchecked)
		hbmp_unchecked = _app_ico2bmp (app.GetSharedIcon (app.GetHINSTANCE (), IDI_UNCHECKED, GetSystemMetrics (SM_CXSMICON)));

	if (!_app_isrulesexists (TypeCustom, -1, -1))
	{
		AppendMenu (hsubmenu, MF_SEPARATOR, 0, nullptr);
		AppendMenu (hsubmenu, MF_STRING, IDX_RULES_SPECIAL, app.LocaleString (IDS_STATUS_EMPTY, nullptr));

		EnableMenuItem (hsubmenu, IDX_RULES_SPECIAL, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
	}
	else
	{
		for (UINT8 type = 0; type < 3; type++)
		{
			if (type == 0)
			{
				if (!_app_isrulesexists (TypeCustom, true, false))
					continue;
			}
			else if (type == 1)
			{
				if (!_app_isrulesexists (TypeCustom, false, false))
					continue;
			}
			else if (type == 2)
			{
				if (!_app_isrulesexists (TypeCustom, false, true))
					continue;
			}

			AppendMenu (hsubmenu, MF_SEPARATOR, 0, nullptr);

			for (UINT8 loop = 0; loop < 2; loop++)
			{
				for (size_t i = 0; i < rules_arr.size (); i++)
				{
					PITEM_RULE const ptr_rule = rules_arr.at (i);

					if (ptr_rule)
					{
						const bool is_global = (ptr_rule->is_enabled && ptr_rule->apps.empty ());
						const bool is_checked = is_global || (ptr_rule->is_enabled && (ptr_rule->apps.find (app_hash) != ptr_rule->apps.end ()));

						if (ptr_rule->type != TypeCustom || (type == 0 && (!ptr_rule->is_readonly || is_global)) || (type == 1 && (ptr_rule->is_readonly || is_global)) || (type == 2 && !is_global))
							continue;

						if ((loop == 0 && !is_checked) || (loop == 1 && is_checked))
							continue;

						WCHAR buffer[128] = {0};
						StringCchPrintf (buffer, _countof (buffer), app.LocaleString (IDS_RULE_APPLY_2, ptr_rule->is_readonly ? L" [*]" : nullptr), ptr_rule->pname);

						MENUITEMINFO mii = {0};

						mii.cbSize = sizeof (mii);
						mii.fMask = MIIM_ID | MIIM_FTYPE | MIIM_STATE | MIIM_STRING | MIIM_BITMAP | MIIM_CHECKMARKS;
						mii.fType = MFT_STRING;
						mii.dwTypeData = buffer;
						mii.hbmpItem = ptr_rule->is_block ? hbmp_block : hbmp_allow;
						mii.hbmpChecked = hbmp_checked;
						mii.hbmpUnchecked = hbmp_unchecked;
						mii.fState = (is_checked ? MF_CHECKED : MF_UNCHECKED);
						mii.wID = IDX_RULES_SPECIAL + UINT (i);

						if (is_global)
							mii.fState |= MF_DISABLED | MF_GRAYED;

						InsertMenuItem (hsubmenu, mii.wID, FALSE, &mii);
					}
				}
			}
		}
	}

	AppendMenu (hsubmenu, MF_SEPARATOR, 0, nullptr);
	AppendMenu (hsubmenu, MF_STRING, IDM_EDITRULES, app.LocaleString (IDS_EDITRULES, L"..."));
	AppendMenu (hsubmenu, MF_STRING, IDM_OPENRULESEDITOR, app.LocaleString (IDS_OPENRULESEDITOR, L"..."));
}

bool _app_item_get (EnumAppType type, size_t hash, rstring * display_name, rstring * real_path, PSID * lpsid, PSECURITY_DESCRIPTOR * lpsd, rstring * /*description*/)
{
	for (size_t i = 0; i < items.size (); i++)
	{
		PITEM_ADD ptr_item = items.at (i);

		if (
			!ptr_item ||
			ptr_item->type != type ||
			ptr_item->hash != hash
			)
		{
			continue;
		}

		if (display_name)
		{
			if (ptr_item->display_name && ptr_item->display_name[0])
				*display_name = ptr_item->display_name;

			else if (ptr_item->real_path && ptr_item->real_path[0])
				*display_name = ptr_item->real_path;

			else if (ptr_item->sid && ptr_item->sid[0])
				*display_name = ptr_item->sid;
		}

		if (real_path)
		{
			if (ptr_item->real_path && ptr_item->real_path[0])
				*real_path = ptr_item->real_path;
		}

		if (lpsid)
		{
			SAFE_DELETE_ARRAY (*lpsid);

			if (ptr_item->psid)
			{
				static const DWORD length = SECURITY_MAX_SID_SIZE;

				*lpsid = new BYTE[length];

				CopyMemory (*lpsid, ptr_item->psid, length);
			}
		}

		if (lpsd)
		{
			SAFE_DELETE_ARRAY (*lpsd);

			if (ptr_item->psd)
			{
				const DWORD length = GetSecurityDescriptorLength (ptr_item->psd);

				if (length)
				{
					*lpsd = new BYTE[length];

					CopyMemory (*lpsd, ptr_item->psd, length);
				}
			}
		}

		//if (description)
		//	*description = pvec->at (i).pdesc;

		return true;
	}

	return false;
}

INT CALLBACK _app_listviewcompare_abc (LPARAM item1, LPARAM item2, LPARAM lparam)
{
	HWND hwnd = GetParent ((HWND)lparam);
	const UINT ctrl_id = GetDlgCtrlID ((HWND)lparam);

	const bool is_checked1 = _r_listview_isitemchecked (hwnd, ctrl_id, (size_t)item1);
	const bool is_checked2 = _r_listview_isitemchecked (hwnd, ctrl_id, (size_t)item2);

	if ((is_checked1 || is_checked2) && (is_checked1 != is_checked2))
	{
		if (is_checked1 && !is_checked2)
			return -1;

		if (!is_checked1 && is_checked2)
			return 1;
	}

	const rstring str1 = _r_listview_getitemtext (hwnd, ctrl_id, (size_t)item1, 0);
	const rstring str2 = _r_listview_getitemtext (hwnd, ctrl_id, (size_t)item2, 0);

	return StrCmpLogicalW (str1, str2);
}

INT CALLBACK _app_listviewcompare_apps (LPARAM lp1, LPARAM lp2, LPARAM lparam)
{
	const UINT column_id = LOWORD (lparam);
	const BOOL is_descend = HIWORD (lparam);

	const size_t hash1 = static_cast<size_t>(lp1);
	const size_t hash2 = static_cast<size_t>(lp2);

	INT result = 0;

	PITEM_APP const ptr_app1 = _app_getapplication (hash1);
	PITEM_APP const ptr_app2 = _app_getapplication (hash2);

	if (!ptr_app1 || !ptr_app2)
		return 0;

	if (column_id == 0)
	{
		// file
		result = StrCmpLogicalW (ptr_app1->display_name, ptr_app2->display_name);
	}
	else if (column_id == 1)
	{
		// timestamp
		if (ptr_app1->timestamp == ptr_app2->timestamp)
			result = 0;

		else if (ptr_app1->timestamp < ptr_app2->timestamp)
			result = -1;

		else if (ptr_app1->timestamp > ptr_app2->timestamp)
			result = 1;
	}

	return is_descend ? -result : result;
}

INT CALLBACK _app_listviewcompare_rules (LPARAM item1, LPARAM item2, LPARAM lparam)
{
	const UINT listview_id = (UINT)lparam;

	const UINT column_id = app.ConfigGet (L"SortColumnRules", 0).AsUint ();
	const bool is_descend = app.ConfigGet (L"IsSortDescendingRules", false).AsBool ();

	const rstring str1 = _r_listview_getitemtext (app.GetHWND (), listview_id, (size_t)item1, column_id);
	const rstring str2 = _r_listview_getitemtext (app.GetHWND (), listview_id, (size_t)item2, column_id);

	const INT result = StrCmpLogicalW (str1, str2);

	return is_descend ? -result : result;
}

void _app_listviewsort (HWND hwnd, UINT ctrl_id, INT subitem, bool is_notifycode)
{
	_r_fastlock_acquireshared (&lock_access);

	for (INT i = 0; i < _r_listview_getcolumncount (hwnd, ctrl_id); i++)
		_r_listview_setcolumnsortindex (hwnd, ctrl_id, i, 0);

	if (
		ctrl_id == IDC_APPS_PROFILE ||
		ctrl_id == IDC_APPS_SERVICE ||
		ctrl_id == IDC_APPS_PACKAGE
		)
	{
		bool is_descend = app.ConfigGet (L"IsSortDescending", true).AsBool ();

		if (is_notifycode)
			is_descend = !is_descend;

		if (subitem == -1)
			subitem = app.ConfigGet (L"SortColumn", 1).AsBool ();

		const WPARAM wparam = MAKEWPARAM (subitem, is_descend);

		if (is_notifycode)
		{
			app.ConfigSet (L"IsSortDescending", is_descend);
			app.ConfigSet (L"SortColumn", (DWORD)subitem);
		}

		_r_listview_setcolumnsortindex (hwnd, ctrl_id, subitem, is_descend ? -1 : 1);

		_r_fastlock_acquireshared (&lock_checkbox);

		for (size_t i = 0; i < _r_listview_getitemcount (hwnd, ctrl_id); i++)
		{
			const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, ctrl_id, i);
			PITEM_APP ptr_app = _app_getapplication (hash);

			if (ptr_app)
			{
				_r_listview_setitem (hwnd, ctrl_id, i, 0, nullptr, LAST_VALUE, _app_getappgroup (hash, ptr_app));
				_r_listview_setitemcheck (hwnd, ctrl_id, i, ptr_app->is_enabled);
			}
		}

		_r_fastlock_releaseshared (&lock_checkbox);

		SendDlgItemMessage (hwnd, ctrl_id, LVM_SORTITEMS, wparam, (LPARAM)& _app_listviewcompare_apps);

		_app_refreshstatus (hwnd);
	}
	else if (
		ctrl_id == IDC_RULES_BLOCKLIST ||
		ctrl_id == IDC_RULES_SYSTEM ||
		ctrl_id == IDC_RULES_CUSTOM
		)
	{
		bool is_descend = app.ConfigGet (L"IsSortDescendingRules", false).AsBool ();

		if (is_notifycode)
			is_descend = !is_descend;

		if (subitem == -1)
			subitem = app.ConfigGet (L"SortColumnRules", 0).AsUint ();

		if (is_notifycode)
		{
			app.ConfigSet (L"IsSortDescendingRules", is_descend);
			app.ConfigSet (L"SortColumnRules", (DWORD)subitem);
		}

		_r_listview_setcolumnsortindex (hwnd, ctrl_id, subitem, is_descend ? -1 : 1);

		SendDlgItemMessage (hwnd, ctrl_id, LVM_SORTITEMSEX, (WPARAM)ctrl_id, (LPARAM)& _app_listviewcompare_rules);
	}
	else
	{
		SendDlgItemMessage (hwnd, ctrl_id, LVM_SORTITEMSEX, (WPARAM)GetDlgItem (hwnd, ctrl_id), (LPARAM)& _app_listviewcompare_abc);
	}

	_r_fastlock_releaseshared (&lock_access);
}

void _app_refreshstatus (HWND hwnd)
{
	const HWND hstatus = GetDlgItem (hwnd, IDC_STATUSBAR);
	const HDC hdc = GetDC (hstatus);

	const UINT listview_id = _app_gettab_id (hwnd);

	// item count
	if (hdc)
	{
		SelectObject (hdc, (HFONT)SendMessage (hstatus, WM_GETFONT, 0, 0)); // fix

		static const UINT parts_count = 3;

		rstring text[parts_count];
		INT parts[parts_count] = {0};
		LONG size[parts_count] = {0};
		LONG lay = 0;

		_r_fastlock_acquireshared (&lock_access);

		ITEM_STATUS itemStat = {0};
		_app_getcount (&itemStat);

		_r_fastlock_releaseshared (&lock_access);

		for (UINT i = 0; i < parts_count; i++)
		{
			switch (i)
			{
				case 0:
				{
					text[i].Format (app.LocaleString (IDS_STATUS_TOTAL, nullptr), itemStat.total_count);
					break;
				}

				case 1:
				{
					text[i].Format (L"%s: %d", app.LocaleString (IDS_STATUS_UNUSED_APPS, nullptr).GetString (), itemStat.unused_count);
					break;
				}

				case 2:
				{
					text[i].Format (L"%s: %d", app.LocaleString (IDS_STATUS_TIMER_APPS, nullptr).GetString (), itemStat.timers_count);
					break;
				}
			}

			size[i] = _r_dc_fontwidth (hdc, text[i], text[i].GetLength ()) + 10;

			if (i)
				lay += size[i];
		}

		RECT rc = {0};
		GetClientRect (hstatus, &rc);

		parts[0] = _R_RECT_WIDTH (&rc) - lay - GetSystemMetrics (SM_CXSMICON);
		parts[1] = parts[0] + size[1];
		parts[2] = parts[1] + size[2];

		SendMessage (hstatus, SB_SETPARTS, parts_count, (LPARAM)parts);

		for (UINT i = 0; i < parts_count; i++)
			SendMessage (hstatus, SB_SETTEXT, MAKEWPARAM (i, 0), (LPARAM)text[i].GetString ());

		ReleaseDC (hstatus, hdc);
	}

	// group information
	{
		size_t group1_count = 0;
		size_t group2_count = 0;
		size_t group3_count = 0;

		if (
			listview_id == IDC_APPS_PROFILE ||
			listview_id == IDC_APPS_SERVICE ||
			listview_id == IDC_APPS_PACKAGE
			)
		{
			const size_t total_count = _r_listview_getitemcount (hwnd, listview_id);

			for (size_t i = 0; i < total_count; i++)
			{
				const size_t hash = _r_listview_getitemlparam (hwnd, listview_id, i);
				const PITEM_APP ptr_app = _app_getapplication (hash);

				if (ptr_app)
				{
					const size_t group_id = _app_getappgroup (hash, ptr_app);

					if (group_id == 0)
						group1_count += 1;

					else if (group_id == 1)
						group2_count += 1;

					else
						group3_count += 1;
				}
			}

			const bool is_whitelist = (app.ConfigGet (L"Mode", ModeWhitelist).AsUint () == ModeWhitelist);

			_r_listview_setgroup (hwnd, listview_id, 0, app.LocaleString (is_whitelist ? IDS_GROUP_ALLOWED : IDS_GROUP_BLOCKED, _r_fmt (L" (%d/%d)", group1_count, total_count)), 0, 0);
			_r_listview_setgroup (hwnd, listview_id, 1, app.LocaleString (IDS_GROUP_SPECIAL_APPS, _r_fmt (L" (%d/%d)", group2_count, total_count)), 0, 0);
			_r_listview_setgroup (hwnd, listview_id, 2, app.LocaleString (is_whitelist ? IDS_GROUP_BLOCKED : IDS_GROUP_ALLOWED, _r_fmt (L" (%d/%d)", group3_count, total_count)), 0, 0);
		}
		else if (
			listview_id == IDC_RULES_BLOCKLIST ||
			listview_id == IDC_RULES_SYSTEM ||
			listview_id == IDC_RULES_CUSTOM
			)
		{
			const size_t total_count = _r_listview_getitemcount (hwnd, listview_id);

			for (size_t i = 0; i < total_count; i++)
			{
				const size_t idx = _r_listview_getitemlparam (hwnd, listview_id, i);
				const PITEM_RULE ptr_rule = rules_arr.at (idx);

				if (ptr_rule)
				{
					const size_t group_id = _app_getrulegroup (ptr_rule);

					if (group_id == 0)
						group1_count += 1;

					else if (group_id == 1)
						group2_count += 1;

					else
						group3_count += 1;
				}
			}

			_r_listview_setgroup (hwnd, listview_id, 0, app.LocaleString (IDS_GROUP_ENABLED, _r_fmt (L" (%d/%d)", group1_count, total_count)), 0, 0);
			_r_listview_setgroup (hwnd, listview_id, 1, app.LocaleString (IDS_GROUP_SPECIAL, _r_fmt (L" (%d/%d)", group2_count, total_count)), 0, 0);
			_r_listview_setgroup (hwnd, listview_id, 2, app.LocaleString (IDS_GROUP_DISABLED, _r_fmt (L" (%d/%d)", group3_count, total_count)), 0, 0);
		}
	}
}

rstring _app_parsehostaddress_dns (LPCWSTR host, USHORT port)
{
	rstring result;

	PDNS_RECORD ppQueryResultsSet = nullptr;
	PIP4_ARRAY pSrvList = nullptr;

	// use custom dns-server (if present)
	WCHAR dnsServer[INET_ADDRSTRLEN] = {0};
	StringCchCopy (dnsServer, _countof (dnsServer), app.ConfigGet (L"DnsServerV4", nullptr)); // ipv4 dns-server address

	if (dnsServer[0])
	{
		pSrvList = new IP4_ARRAY;

		if (pSrvList)
		{
			if (InetPton (AF_INET, dnsServer, &(pSrvList->AddrArray[0])))
			{
				pSrvList->AddrCount = 1;
			}
			else
			{
				_app_logerror (L"InetPton", WSAGetLastError (), dnsServer, true);
				SAFE_DELETE (pSrvList);
			}
		}
	}

	const DNS_STATUS dnsStatus = DnsQuery (host, DNS_TYPE_ALL, DNS_QUERY_NO_HOSTS_FILE | DNS_QUERY_DNSSEC_CHECKING_DISABLED, pSrvList, &ppQueryResultsSet, nullptr);

	if (dnsStatus != ERROR_SUCCESS)
	{
		_app_logerror (L"DnsQuery", dnsStatus, host, true);
	}
	else
	{
		for (auto current = ppQueryResultsSet; current != nullptr; current = current->pNext)
		{
			if (current->wType == DNS_TYPE_A)
			{
				// ipv4 address
				WCHAR str[INET_ADDRSTRLEN] = {0};
				InetNtop (AF_INET, &(current->Data.A.IpAddress), str, _countof (str));

				result.Append (str);

				if (port)
					result.AppendFormat (L":%d", port);

				result.Append (RULE_DELIMETER);
			}
			else if (current->wType == DNS_TYPE_AAAA)
			{
				// ipv6 address
				WCHAR str[INET6_ADDRSTRLEN] = {0};
				InetNtop (AF_INET6, &(current->Data.AAAA.Ip6Address), str, _countof (str));

				result.Append (str);

				if (port)
					result.AppendFormat (L":%d", port);

				result.Append (RULE_DELIMETER);
			}
			else if (current->wType == DNS_TYPE_CNAME)
			{
				// canonical name
				if (current->Data.CNAME.pNameHost)
				{
					result = _app_parsehostaddress_dns (current->Data.CNAME.pNameHost, port);
					break;
				}
			}
		}

		result.Trim (RULE_DELIMETER);

		DnsRecordListFree (ppQueryResultsSet, DnsFreeRecordList);
	}

	SAFE_DELETE (pSrvList);

	return result;
}

rstring _app_parsehostaddress_wsa (LPCWSTR hostname, USHORT port)
{
	rstring result;

	if (!config.is_wsainit || !hostname || !hostname[0] || !app.ConfigGet (L"IsEnableWsaResolver", true).AsBool ())
		return L"";

	ADDRINFOEXW hints = {0};
	ADDRINFOEXW * ppQueryResultsSet = nullptr;

	hints.ai_family = AF_UNSPEC;
	//hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	LPGUID lpNspid = nullptr;
	const INT code = GetAddrInfoEx (hostname, L"domain", NS_DNS, lpNspid, &hints, &ppQueryResultsSet, nullptr, nullptr, nullptr, nullptr);

	if (code != ERROR_SUCCESS || !ppQueryResultsSet)
	{
		_app_logerror (L"GetAddrInfoEx", code, hostname, false);
	}
	else
	{
		for (auto current = ppQueryResultsSet; current != nullptr; current = current->ai_next)
		{
			WCHAR printableIP[INET6_ADDRSTRLEN] = {0};

			if (current->ai_family == AF_INET)
			{
				struct sockaddr_in *sock_in4 = (struct sockaddr_in *)current->ai_addr;
				PIN_ADDR addr4 = &(sock_in4->sin_addr);

				if (IN4_IS_ADDR_UNSPECIFIED (addr4) || IN4_IS_ADDR_LOOPBACK (addr4))
					continue;

				InetNtop (current->ai_family, addr4, printableIP, _countof (printableIP));
			}
			else if (current->ai_family == AF_INET6)
			{
				struct sockaddr_in6 *sock_in6 = (struct sockaddr_in6 *)current->ai_addr;
				PIN6_ADDR addr6 = &(sock_in6->sin6_addr);

				if (IN6_IS_ADDR_UNSPECIFIED (addr6) || IN6_IS_ADDR_LOOPBACK (addr6))
					continue;

				InetNtop (current->ai_family, addr6, printableIP, _countof (printableIP));
			}

			if (!printableIP[0])
				continue;

			result.Append (printableIP);

			if (port)
				result.AppendFormat (L":%d", port);

			result.Append (RULE_DELIMETER);
		}

		result.Trim (RULE_DELIMETER);
	}

	if (ppQueryResultsSet)
		FreeAddrInfoExW (ppQueryResultsSet);

	return result;
}

bool _app_parsenetworkstring (LPCWSTR network_string, NET_ADDRESS_FORMAT * format_ptr, USHORT * port_ptr, FWP_V4_ADDR_AND_MASK * paddr4, FWP_V6_ADDR_AND_MASK * paddr6, LPWSTR paddr_dns, size_t dns_length)
{
	NET_ADDRESS_INFO ni;
	SecureZeroMemory (&ni, sizeof (ni));

	USHORT port = 0;
	BYTE prefix_length = 0;

	static const DWORD types = NET_STRING_ANY_ADDRESS | NET_STRING_ANY_SERVICE | NET_STRING_IP_NETWORK | NET_STRING_ANY_ADDRESS_NO_SCOPE | NET_STRING_ANY_SERVICE_NO_SCOPE;
	const DWORD errcode = ParseNetworkString (network_string, types, &ni, &port, &prefix_length);

	if (errcode != ERROR_SUCCESS)
	{
		_app_logerror (L"ParseNetworkString", errcode, network_string, true);
		return false;
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

				paddr4->addr = ntohl (ni.Ipv4Address.sin_addr.S_un.S_addr);
				paddr4->mask = ntohl (mask);
			}

			return true;
		}
		else if (ni.Format == NET_ADDRESS_IPV6)
		{
			if (paddr6)
			{
				CopyMemory (paddr6->addr, ni.Ipv6Address.sin6_addr.u.Byte, FWP_V6_ADDR_SIZE);
				paddr6->prefixLength = min (prefix_length, 128);
			}

			return true;
		}
		else if (ni.Format == NET_ADDRESS_DNS_NAME)
		{
			if (paddr_dns)
			{
				const size_t hash = _r_str_hash (ni.NamedAddress.Address);

				_r_fastlock_acquireshared (&lock_cache);
				const bool is_exists = cache_dns.find (hash) != cache_dns.end ();
				_r_fastlock_releaseshared (&lock_cache);

				if (is_exists)
				{
					_r_fastlock_acquireshared (&lock_cache);
					StringCchCopy (paddr_dns, dns_length, cache_dns[hash]);
					_r_fastlock_releaseshared (&lock_cache);

					return *paddr_dns;
				}

				rstring host = _app_parsehostaddress_dns (ni.NamedAddress.Address, port);

				if (host.IsEmpty ())
					host = _app_parsehostaddress_wsa (ni.NamedAddress.Address, port);

				if (host.IsEmpty ())
				{
					return false;
				}
				else
				{
					_r_fastlock_acquireexclusive (&lock_cache);

					_app_freecache (&cache_dns);
					_r_str_alloc (&cache_dns[hash], host.GetLength (), host);

					StringCchCopy (paddr_dns, dns_length, host);

					_r_fastlock_releaseexclusive (&lock_cache);

					return true;
				}
			}

			return true;
		}
	}

	return false;
}

bool _app_parserulestring (rstring rule, PITEM_ADDRESS ptr_addr)
{
	rule.Trim (L"\r\n "); // trim whitespace

	if (rule.IsEmpty ())
		return true;

	if (rule.At (0) == L'*')
		return true;

	EnumRuleItemType type = TypeRuleItemUnknown;
	const size_t range_pos = rule.Find (RULE_RANGE_CHAR);
	bool is_range = (range_pos != rstring::npos);

	WCHAR range_start[LEN_IP_MAX] = {0};
	WCHAR range_end[LEN_IP_MAX] = {0};

	if (is_range)
	{
		StringCchCopy (range_start, _countof (range_start), rule.Midded (0, range_pos));
		StringCchCopy (range_end, _countof (range_end), rule.Midded (range_pos + 1));
	}

	// auto-parse rule type
	{
		const size_t hash = rule.Hash ();

		_r_fastlock_acquireshared (&lock_cache);
		const bool is_exists = cache_types.find (hash) != cache_types.end ();
		_r_fastlock_releaseshared (&lock_cache);

		if (is_exists)
		{
			_r_fastlock_acquireshared (&lock_cache);

			type = cache_types[hash];

			_r_fastlock_releaseshared (&lock_cache);
		}
		else
		{
			if (_app_isruleport (rule))
			{
				type = TypePort;
			}
			else if (is_range ? (_app_isruleip (range_start) && _app_isruleip (range_end)) : _app_isruleip (rule))
			{
				type = TypeIp;
			}
			else if (_app_isrulehost (rule))
			{
				type = TypeHost;
			}

			if (type != TypeRuleItemUnknown)
			{
				_r_fastlock_acquireexclusive (&lock_cache);

				if (cache_types.size () >= UMAP_CACHE_LIMIT)
					cache_types.clear ();

				cache_types[hash] = type;

				_r_fastlock_releaseexclusive (&lock_cache);
			}
		}
	}

	if (type == TypeRuleItemUnknown)
		return false;

	if (!ptr_addr)
		return true;

	if (type == TypeHost)
		is_range = false;

	ptr_addr->is_range = is_range;

	if (type == TypePort)
	{
		if (!is_range)
		{
			// ...port
			ptr_addr->type = TypePort;
			ptr_addr->port = (UINT16)rule.AsUint ();

			return true;
		}
		else
		{
			// ...port range
			ptr_addr->type = TypePort;

			if (ptr_addr->prange)
			{
				ptr_addr->prange->valueLow.type = FWP_UINT16;
				ptr_addr->prange->valueLow.uint16 = (UINT16)wcstoul (range_start, nullptr, 10);

				ptr_addr->prange->valueHigh.type = FWP_UINT16;
				ptr_addr->prange->valueHigh.uint16 = (UINT16)wcstoul (range_end, nullptr, 10);
			}

			return true;
		}
	}
	else
	{
		NET_ADDRESS_FORMAT format;

		FWP_V4_ADDR_AND_MASK addr4 = {0};
		FWP_V6_ADDR_AND_MASK addr6 = {0};

		USHORT port2 = 0;

		if (type == TypeIp && is_range)
		{
			// ...ip range (start)
			if (_app_parsenetworkstring (range_start, &format, &port2, &addr4, &addr6, nullptr, 0))
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
						CopyMemory (ptr_addr->prange->valueLow.byteArray16->byteArray16, addr6.addr, FWP_V6_ADDR_SIZE);
					}
				}
				else
				{
					return false;
				}

				if (port2 && !ptr_addr->port)
					ptr_addr->port = port2;
			}
			else
			{
				return false;
			}

			// ...ip range (end)
			if (_app_parsenetworkstring (range_end, &format, &port2, &addr4, &addr6, nullptr, 0))
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
						CopyMemory (ptr_addr->prange->valueHigh.byteArray16->byteArray16, addr6.addr, FWP_V6_ADDR_SIZE);
					}
				}
				else
				{
					return false;
				}
			}
			else
			{
				return false;
			}

			ptr_addr->format = format;
			ptr_addr->type = TypeIp;
		}
		else
		{
			// ...ip/host
			if (_app_parsenetworkstring (rule, &format, &port2, &addr4, &addr6, ptr_addr->host, _countof (ptr_addr->host)))
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
						CopyMemory (ptr_addr->paddr6->addr, addr6.addr, FWP_V6_ADDR_SIZE);
					}
				}
				else if (format == NET_ADDRESS_DNS_NAME)
				{
					// ptr_addr->host = <hosts>;
				}
				else
				{
					return false;
				}

				ptr_addr->type = TypeIp;
				ptr_addr->format = format;

				if (port2)
					ptr_addr->port = port2;

				return true;
			}
			else
			{
				return false;
			}
		}
	}

	return true;
}

bool _app_resolveaddress (ADDRESS_FAMILY af, LPVOID paddr, LPWSTR buffer, DWORD length)
{
	SOCKADDR_IN ipv4Address = {0};
	SOCKADDR_IN6 ipv6Address = {0};
	PSOCKADDR psock = {0};
	socklen_t size = 0;

	if (af == AF_INET)
	{
		ipv4Address.sin_family = af;
		ipv4Address.sin_addr = *PIN_ADDR (paddr);

		psock = (PSOCKADDR)& ipv4Address;
		size = sizeof (ipv4Address);
	}
	else if (af == AF_INET6)
	{
		ipv6Address.sin6_family = af;
		ipv6Address.sin6_addr = *PIN6_ADDR (paddr);

		psock = (PSOCKADDR)& ipv6Address;
		size = sizeof (ipv6Address);
	}
	else
	{
		return false;
	}

	if (GetNameInfo (psock, size, buffer, length, nullptr, 0, NI_NAMEREQD) != 0)
		return false;

	return true;
}

void _app_resolvefilename (rstring & path)
{
	// "\??\" refers to \GLOBAL??\. Just remove it.
	if (_wcsnicmp (path, L"\\??\\", 4) == 0)
	{
		path.Mid (4);
	}
	// "\SystemRoot" means "C:\Windows".
	else if (_wcsnicmp (path, L"\\SystemRoot", 11) == 0)
	{
		WCHAR systemRoot[MAX_PATH] = {0};
		GetSystemDirectory (systemRoot, _countof (systemRoot));

		path.Mid (11 + 9);
		path.Insert (0, systemRoot);
	}
	// "system32\" means "C:\Windows\system32\".
	else if (_wcsnicmp (path, L"system32\\", 9) == 0)
	{
		WCHAR systemRoot[MAX_PATH] = {0};
		GetSystemDirectory (systemRoot, _countof (systemRoot));

		path.Mid (8);
		path.Insert (0, systemRoot);
	}
}

void _app_showitem (HWND hwnd, UINT ctrl_id, size_t item, INT scroll_pos)
{
	if (item != LAST_VALUE)
	{
		if (scroll_pos == -1)
			SendDlgItemMessage (hwnd, ctrl_id, LVM_ENSUREVISIBLE, item, TRUE); // ensure item visible

		ListView_SetItemState (GetDlgItem (hwnd, ctrl_id), -1, 0, LVIS_SELECTED | LVIS_FOCUSED); // deselect all
		ListView_SetItemState (GetDlgItem (hwnd, ctrl_id), item, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED); // select item
	}

	if (scroll_pos != -1)
		SendDlgItemMessage (hwnd, ctrl_id, LVM_SCROLL, 0, scroll_pos); // restore vscroll position
}

HBITMAP _app_ico2bmp (HICON hicon)
{
	if (!hicon)
		return nullptr;

	const INT icon_size = GetSystemMetrics (SM_CXSMICON);

	RECT iconRectangle = {0};

	iconRectangle.right = icon_size;
	iconRectangle.bottom = icon_size;

	HBITMAP hbitmap = nullptr;
	const HDC screenHdc = GetDC (nullptr);

	if (screenHdc)
	{
		const HDC hdc = CreateCompatibleDC (screenHdc);

		if (hdc)
		{
			BITMAPINFO bitmapInfo = {0};
			bitmapInfo.bmiHeader.biSize = sizeof (bitmapInfo);
			bitmapInfo.bmiHeader.biPlanes = 1;
			bitmapInfo.bmiHeader.biCompression = BI_RGB;

			bitmapInfo.bmiHeader.biWidth = icon_size;
			bitmapInfo.bmiHeader.biHeight = icon_size;
			bitmapInfo.bmiHeader.biBitCount = 32;

			hbitmap = CreateDIBSection (hdc, &bitmapInfo, DIB_RGB_COLORS, nullptr, nullptr, 0);

			if (hbitmap)
			{
				const HBITMAP oldBitmap = (HBITMAP)SelectObject (hdc, hbitmap);

				BLENDFUNCTION blendFunction = {0};
				blendFunction.BlendOp = AC_SRC_OVER;
				blendFunction.AlphaFormat = AC_SRC_ALPHA;
				blendFunction.SourceConstantAlpha = 255;

				BP_PAINTPARAMS paintParams = {0};
				paintParams.cbSize = sizeof (paintParams);
				paintParams.dwFlags = BPPF_ERASE;
				paintParams.pBlendFunction = &blendFunction;

				HDC bufferHdc = nullptr;

				const HPAINTBUFFER paintBuffer = BeginBufferedPaint (hdc, &iconRectangle, BPBF_DIB, &paintParams, &bufferHdc);

				if (paintBuffer)
				{
					DrawIconEx (bufferHdc, 0, 0, hicon, icon_size, icon_size, 0, nullptr, DI_NORMAL);
					EndBufferedPaint (paintBuffer, TRUE);
				}
				else
				{
					_r_dc_fillrect (hdc, &iconRectangle, GetSysColor (COLOR_MENU));
					DrawIconEx (hdc, 0, 0, hicon, icon_size, icon_size, 0, nullptr, DI_NORMAL);
				}

				SelectObject (hdc, oldBitmap);
			}

			DeleteDC (hdc);
		}

		ReleaseDC (nullptr, screenHdc);
	}

	return hbitmap;
}

void _app_load_appxmanifest (PITEM_ADD ptr_item)
{
	if (!ptr_item || !ptr_item->real_path)
		return;

	rstring result;
	rstring path;

	static LPCWSTR appx_names[] = {
		L"AppxManifest.xml",
		L"VSAppxManifest.xml",
	};

	for (size_t i = 0; i < _countof (appx_names); i++)
	{
		path.Format (L"%s\\%s", ptr_item->real_path, appx_names[i]);

		if (_r_fs_exists (path))
			goto doopen;
	}

	return;

doopen:

	pugi::xml_document doc;
	pugi::xml_parse_result xml_result = doc.load_file (path, PUGIXML_LOAD_FLAGS, PUGIXML_LOAD_ENCODING);

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
					result.Format (L"%s\\%s", ptr_item->real_path, item.attribute (L"Executable").as_string ());

					if (_r_fs_exists (result))
						break;
				}
			}
		}
	}

	_r_str_alloc (&ptr_item->real_path, result.GetLength (), result.GetString ());
}

LPVOID _app_loadresource (LPCWSTR res, PDWORD size)
{
	const HINSTANCE hinst = app.GetHINSTANCE ();

	HRSRC hres = FindResource (hinst, res, RT_RCDATA);

	if (hres)
	{
		HGLOBAL hloaded = LoadResource (hinst, hres);

		if (hloaded)
		{
			LPVOID pLockedResource = LockResource (hloaded);

			if (pLockedResource)
			{
				DWORD dwResourceSize = SizeofResource (hinst, hres);

				if (dwResourceSize != 0)
				{
					if (size)
						*size = dwResourceSize;

					return pLockedResource;
				}
			}
		}
	}

	return nullptr;
}
