// simplewall
// Copyright (c) 2016-2019 Henry++

#include "global.hpp"

void _app_dereferenceapp (PVOID pdata)
{
	delete PITEM_APP (pdata);
}

void _app_dereferenceappshelper (PVOID pdata)
{
	delete PITEM_APP_HELPER (pdata);
}

void _app_dereferencecolor (PVOID pdata)
{
	delete PITEM_COLOR (pdata);
}

void _app_dereferencelog (PVOID pdata)
{
	delete PITEM_LOG (pdata);
}

void _app_dereferencenetwork (PVOID pdata)
{
	delete PITEM_NETWORK (pdata);
}

void _app_dereferencerule (PVOID pdata)
{
	delete PITEM_RULE (pdata);
}

void _app_dereferenceruleconfig (PVOID pdata)
{
	delete PITEM_RULE_CONFIG (pdata);
}

void _app_dereferencestring (PVOID pdata)
{
	delete[] LPWSTR (pdata);
}

UINT _app_gettab_id (HWND hwnd, UINT page_id)
{
	TCITEM tci = {0};

	tci.mask = TCIF_PARAM;

	if (page_id == UINT (-1))
	{
		page_id = (UINT)SendDlgItemMessage (hwnd, IDC_TAB, TCM_GETCURSEL, 0, 0);

		if (page_id == UINT (-1))
			page_id = 0;
	}

	SendDlgItemMessage (hwnd, IDC_TAB, TCM_GETITEM, page_id, (LPARAM)& tci);

	return (UINT)tci.lParam;
}

void _app_settab_id (HWND hwnd, UINT page_id)
{
	if (!page_id || (_app_gettab_id (hwnd) == page_id && IsWindowVisible (GetDlgItem (hwnd, page_id))))
		return;

	for (UINT i = 0; i < (UINT)SendDlgItemMessage (hwnd, IDC_TAB, TCM_GETITEMCOUNT, 0, 0); i++)
	{
		const UINT listview_id = _app_gettab_id (hwnd, i);

		if (listview_id == page_id)
		{
			SendDlgItemMessage (hwnd, IDC_TAB, TCM_SETCURSEL, i, 0);

			NMHDR hdr = {0};

			hdr.code = TCN_SELCHANGE;
			hdr.hwndFrom = hwnd;
			hdr.idFrom = IDC_TAB;

			SendMessage (hwnd, WM_NOTIFY, 0, (LPARAM)& hdr);

			return;
		}
	}

	_app_settab_id (hwnd, IDC_APPS_PROFILE);
}

bool _app_initinterfacestate (HWND hwnd)
{
	const bool is_enabled = SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_ISBUTTONENABLED, IDM_TRAY_START, 0);

	if (is_enabled)
	{
		SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_ENABLEBUTTON, IDM_TRAY_START, MAKELPARAM (FALSE, 0));
		SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_ENABLEBUTTON, IDM_REFRESH, MAKELPARAM (FALSE, 0));

		_r_status_settext (hwnd, IDC_STATUSBAR, 0, app.LocaleString (IDS_STATUS_FILTERS_PROCESSING, L"..."));
	}

	return is_enabled;
}

void _app_restoreinterfacestate (HWND hwnd, bool is_enabled)
{
	if (is_enabled)
	{
		SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_ENABLEBUTTON, IDM_TRAY_START, MAKELPARAM (TRUE, 0));
		SendDlgItemMessage (config.hrebar, IDC_TOOLBAR, TB_ENABLEBUTTON, IDM_REFRESH, MAKELPARAM (TRUE, 0));

		_r_status_settext (hwnd, IDC_STATUSBAR, 0, app.LocaleString (_wfp_isfiltersinstalled () ? IDS_STATUS_FILTERS_ACTIVE : IDS_STATUS_FILTERS_INACTIVE, nullptr));
	}
}

void _app_setinterfacestate (HWND hwnd)
{
	const bool is_filtersinstalled = _wfp_isfiltersinstalled ();

	const HICON hico_big = app.GetSharedImage (app.GetHINSTANCE (), is_filtersinstalled ? IDI_ACTIVE : IDI_INACTIVE, GetSystemMetrics (SM_CXICON));
	const HICON hico_sm = app.GetSharedImage (app.GetHINSTANCE (), is_filtersinstalled ? IDI_ACTIVE : IDI_INACTIVE, GetSystemMetrics (SM_CXSMICON));

	SendMessage (hwnd, WM_SETICON, ICON_BIG, (LPARAM)hico_big);
	SendMessage (hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hico_sm);

	//SendDlgItemMessage (hwnd, IDC_STATUSBAR, SB_SETICON, 0, (LPARAM)hico_sm);

	_r_toolbar_setbuttoninfo (config.hrebar, IDC_TOOLBAR, IDM_TRAY_START, app.LocaleString (is_filtersinstalled ? IDS_TRAY_STOP : IDS_TRAY_START, nullptr), BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, 0, is_filtersinstalled ? 3 : 2);

	app.TraySetInfo (hwnd, UID, nullptr, app.GetSharedImage (app.GetHINSTANCE (), is_filtersinstalled ? IDI_ACTIVE : IDI_INACTIVE, GetSystemMetrics (SM_CXSMICON)), nullptr);
	app.TrayToggle (hwnd, UID, nullptr, true);
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

void _app_explorefile (LPCWSTR path)
{
	if (!path || !path[0])
		return;

	if (_r_fs_exists (path))
	{
		_r_run (nullptr, _r_fmt (L"\"explorer.exe\" /select,\"%s\"", path));
	}
	else
	{
		LPCWSTR dir = _r_path_extractdir (path);

		if (_r_fs_exists (dir))
			ShellExecute (nullptr, nullptr, dir, nullptr, nullptr, SW_SHOWDEFAULT);
	}
}

bool _app_formataddress (ADDRESS_FAMILY af, UINT8 proto, const PVOID ptr_addr, UINT16 port, LPWSTR * ptr_dest, DWORD flags)
{
	if (!ptr_addr || !ptr_dest || (af != AF_INET && af != AF_INET6))
		return false;

	bool result = false;

	WCHAR formatted_address[DNS_MAX_NAME_BUFFER_LENGTH] = {0};

	if (proto && (flags & FMTADDR_USE_PROTOCOL) != 0)
		StringCchPrintf (formatted_address, _countof (formatted_address), L"%s://", _app_getprotoname (proto, AF_UNSPEC).GetString ());

	if ((flags & FMTADDR_AS_ARPA) != 0)
	{
		if (af == AF_INET)
		{
			StringCchCat (formatted_address, _countof (formatted_address), _r_fmt (L"%hhu.%hhu.%hhu.%hhu.%s", ((PIN_ADDR)ptr_addr)->s_impno, ((PIN_ADDR)ptr_addr)->s_lh, ((PIN_ADDR)ptr_addr)->s_host, ((PIN_ADDR)ptr_addr)->s_net, DNS_IP4_REVERSE_DOMAIN_STRING_W));
		}
		else
		{
			for (INT i = sizeof (IN6_ADDR) - 1; i >= 0; i--)
				StringCchCat (formatted_address, _countof (formatted_address), _r_fmt (L"%hhx.%hhx.", ((PIN6_ADDR)ptr_addr)->s6_addr[i] & 0xF, (((PIN6_ADDR)ptr_addr)->s6_addr[i] >> 4) & 0xF));

			StringCchCat (formatted_address, _countof (formatted_address), DNS_IP6_REVERSE_DOMAIN_STRING_W);
		}

		result = true;
	}
	else
	{
		WCHAR addr_str[DNS_MAX_NAME_BUFFER_LENGTH] = {0};

		if (InetNtop (af, ptr_addr, addr_str, _countof (addr_str)))
		{
			if ((flags & FMTADDR_AS_RULE) != 0)
			{
				if (af == AF_INET)
					result = !IN4_IS_ADDR_UNSPECIFIED ((PIN_ADDR)ptr_addr);

				else
					result = !IN6_IS_ADDR_UNSPECIFIED ((PIN6_ADDR)ptr_addr);

				if (result)
					StringCchCat (formatted_address, _countof (formatted_address), (af == AF_INET6 && port) ? _r_fmt (L"[%s]", addr_str) : addr_str);
			}
			else
			{
				result = true;
				StringCchCat (formatted_address, _countof (formatted_address), addr_str);
			}
		}
	}

	if (port)
		StringCchCat (formatted_address, _countof (formatted_address), _r_fmt (formatted_address[0] ? L":%d" : L"%d", port));

	if ((flags & FMTADDR_RESOLVE_HOST) != 0)
	{
		if (result && app.ConfigGet (L"IsNetworkResolutionsEnabled", false).AsBool ())
		{
			const size_t addr_hash = _r_str_hash (formatted_address);

			if (addr_hash)
			{
				_r_fastlock_acquireshared (&lock_cache);
				const bool is_exists = cache_hosts.find (addr_hash) != cache_hosts.end ();
				_r_fastlock_releaseshared (&lock_cache);

				if (is_exists)
				{
					_r_fastlock_acquireshared (&lock_cache);
					PR_OBJECT ptr_cache_object = _r_obj_reference (cache_hosts[addr_hash]);
					_r_fastlock_releaseshared (&lock_cache);

					if (ptr_cache_object)
					{
						if (ptr_cache_object->pdata)
							StringCchCat (formatted_address, _countof (formatted_address), _r_fmt (L" (%s)", (LPCWSTR)ptr_cache_object->pdata));

						_r_obj_dereference (ptr_cache_object, &_app_dereferencestring);
					}
				}
				else
				{
					cache_hosts[addr_hash] = nullptr;

					LPWSTR ptr_cache = nullptr;

					if (_app_resolveaddress (af, ptr_addr, &ptr_cache))
					{
						StringCchCat (formatted_address, _countof (formatted_address), _r_fmt (L" (%s)", ptr_cache));

						_r_fastlock_acquireexclusive (&lock_cache);

						_app_freeobjects_map (cache_hosts, &_app_dereferencestring, false);
						cache_hosts[addr_hash] = _r_obj_allocate (ptr_cache);

						_r_obj_reference (cache_hosts[addr_hash]);

						_r_fastlock_releaseexclusive (&lock_cache);
					}
				}
			}
		}
	}

	_r_str_alloc (ptr_dest, _r_str_length (formatted_address), formatted_address);

	return formatted_address[0] != 0;
}

void _app_freeobjects_map (OBJECTS_MAP& ptr_map, OBJECT_CLEANUP_CALLBACK cleanup_callback, bool is_forced)
{
	if (is_forced || ptr_map.size () >= UMAP_CACHE_LIMIT)
	{
		for (auto &p : ptr_map)
			_r_obj_dereference (p.second, cleanup_callback);

		ptr_map.clear ();
	}
}

void _app_freeobjects_vec (OBJECTS_VEC & ptr_vec, OBJECT_CLEANUP_CALLBACK cleanup_callback)
{
	for (size_t i = 0; i < ptr_vec.size (); i++)
		_r_obj_dereference (ptr_vec.at (i), cleanup_callback);

	ptr_vec.clear ();
}

void _app_freethreadpool (THREADS_VEC * ptr_pool)
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
		const PSLIST_ENTRY listEntry = InterlockedPopEntrySList (&log_stack.ListHead);

		if (!listEntry)
			break;

		InterlockedDecrement (&log_stack.item_count);

		PITEM_LIST_ENTRY ptr_entry = CONTAINING_RECORD (listEntry, ITEM_LIST_ENTRY, ListEntry);

		_r_obj_dereference (ptr_entry->Body, &_app_dereferencelog);

		_aligned_free (ptr_entry);
	}
}

void _app_getappicon (ITEM_APP* ptr_app, bool is_small, size_t * picon_id, HICON * picon)
{
	const bool is_iconshidden = app.ConfigGet (L"IsIconsHidden", false).AsBool ();

	if (ptr_app->type == DataAppRegular || ptr_app->type == DataAppService)
	{
		if (is_iconshidden || !_app_getfileicon (ptr_app->real_path, is_small, picon_id, picon))
		{
			if (picon_id)
				*picon_id = (ptr_app->type == DataAppService) ? config.icon_service_id : config.icon_id;

			if (picon)
				* picon = CopyIcon (is_small ? config.hicon_small : config.hicon_large);
		}

		if (ptr_app->type == DataAppService && *picon_id == config.icon_id)
			* picon_id = config.icon_service_id;
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

void _app_getdisplayname (size_t app_hash, ITEM_APP* ptr_app, LPWSTR * extracted_name)
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

		if (!_app_item_get (ptr_app->type, app_hash, &name, nullptr, nullptr))
			name = ptr_app->original_path;

		_r_str_alloc (extracted_name, name.GetLength (), name);
	}
	else
	{
		LPCWSTR ptr_path = ((app_hash == config.ntoskrnl_hash) ? ptr_app->original_path : ptr_app->real_path);

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
	if (!path || !path[0] || (!picon_id && !picon))
		return false;

	bool result = false;

	SHFILEINFO shfi = {0};
	DWORD flags = 0;

	if (picon_id)
		flags |= SHGFI_SYSICONINDEX;

	if (picon)
		flags |= SHGFI_ICON;

	if (is_small)
		flags |= SHGFI_SMALLICON;

	const HRESULT hrComInit = CoInitialize (nullptr);

	if ((hrComInit == RPC_E_CHANGED_MODE) || SUCCEEDED (hrComInit))
	{
		if (SHGetFileInfo (path, 0, &shfi, sizeof (shfi), flags))
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

	if (!result)
	{
		if (picon_id)
			*picon_id = config.icon_id;

		if (picon)
			*picon = CopyIcon (is_small ? config.hicon_small : config.hicon_large);

		result = true;
	}

	return result;
}

rstring _app_getshortcutpath (HWND hwnd, LPCWSTR path)
{
	rstring result;

	IShellLink* psl = nullptr;

	const HRESULT hrComInit = CoInitializeEx (nullptr, COINIT_MULTITHREADED);

	if ((hrComInit == RPC_E_CHANGED_MODE) || SUCCEEDED (hrComInit))
	{
		if (SUCCEEDED (CoInitializeSecurity (nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, 0, nullptr)))
		{
			if (SUCCEEDED (CoCreateInstance (CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)& psl)))
			{
				IPersistFile* ppf = nullptr;

				if (SUCCEEDED (psl->QueryInterface (IID_IPersistFile, (void**)& ppf)))
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

PR_OBJECT _app_getsignatureinfo (size_t app_hash, PITEM_APP ptr_app)
{
	if (!app.ConfigGet (L"IsCertificatesEnabled", false).AsBool ())
		return nullptr;

	if (!app_hash || !ptr_app || !ptr_app->real_path || !ptr_app->real_path[0])
		return nullptr;

	_r_fastlock_acquireshared (&lock_cache);
	const bool is_exists = cache_signatures.find (app_hash) != cache_signatures.end ();
	_r_fastlock_releaseshared (&lock_cache);

	PR_OBJECT ptr_cache_object = nullptr;

	if (is_exists)
	{
		_r_fastlock_acquireshared (&lock_cache);
		ptr_cache_object = _r_obj_reference (cache_signatures[app_hash]);
		_r_fastlock_releaseshared (&lock_cache);
	}
	else
	{
		cache_signatures[app_hash] = nullptr;

		const HANDLE hfile = CreateFile (ptr_app->real_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);

		if (hfile != INVALID_HANDLE_VALUE)
		{
			static GUID WinTrustActionGenericVerifyV2 = WINTRUST_ACTION_GENERIC_VERIFY_V2;

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
			const LONG status = WinVerifyTrust ((HWND)INVALID_HANDLE_VALUE, &WinTrustActionGenericVerifyV2, &trustData);

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
							const DWORD num_chars = CertGetNameString (psProvCert->pCert, CERT_NAME_ATTR_TYPE, 0, szOID_COMMON_NAME, nullptr, 0) + 1;

							if (num_chars > 1)
							{
								LPWSTR ptr_cache = new WCHAR[num_chars];

								if (CertGetNameString (psProvCert->pCert, CERT_NAME_ATTR_TYPE, 0, szOID_COMMON_NAME, ptr_cache, num_chars) > 1)
								{
									_r_fastlock_acquireexclusive (&lock_cache);

									_app_freeobjects_map (cache_signatures, &_app_dereferencestring, false);
									cache_signatures[app_hash] = _r_obj_allocate (ptr_cache);

									ptr_cache_object = _r_obj_reference (cache_signatures[app_hash]);

									_r_fastlock_releaseexclusive (&lock_cache);
								}
								else
								{
									SAFE_DELETE_ARRAY (ptr_cache);
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

	ptr_app->is_signed = (ptr_cache_object != nullptr && ptr_cache_object->pdata != nullptr);

	return ptr_cache_object;
}

PR_OBJECT _app_getversioninfo (size_t app_hash, PITEM_APP ptr_app)
{
	if (!app_hash || !ptr_app || !ptr_app->real_path || !ptr_app->real_path[0])
		return nullptr;

	PR_OBJECT ptr_cache_object = nullptr;

	_r_fastlock_acquireshared (&lock_cache);
	const bool is_exists = cache_versions.find (app_hash) != cache_versions.end ();
	_r_fastlock_releaseshared (&lock_cache);

	if (is_exists)
	{
		_r_fastlock_acquireshared (&lock_cache);
		ptr_cache_object = _r_obj_reference (cache_versions[app_hash]);
		_r_fastlock_releaseshared (&lock_cache);
	}
	else
	{
		cache_versions[app_hash] = nullptr;

		const HINSTANCE hlib = LoadLibraryEx (ptr_app->real_path, nullptr, LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE);

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
						rstring buffer;

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
							VS_FIXEDFILEINFO* verInfo = nullptr;

							if (VerQueryValue (versionInfo, L"\\", (void**)(&verInfo), &length))
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
						LPWSTR ptr_cache = nullptr;

						if (_r_str_alloc (&ptr_cache, buffer.GetLength (), buffer))
						{
							_r_fastlock_acquireexclusive (&lock_cache);

							_app_freeobjects_map (cache_versions, &_app_dereferencestring, false);
							cache_versions[app_hash] = _r_obj_allocate (ptr_cache);

							ptr_cache_object = _r_obj_reference (cache_versions[app_hash]);

							_r_fastlock_releaseexclusive (&lock_cache);
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

rstring _app_getservicename (UINT16 port)
{
	if (!port)
		return SZ_EMPTY;

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
			return L"finger";

		case 80:
		case 8008:
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

		case 513:
			return L"login";

		case 500:
			return L"isakmp";

		case 514:
			return L"shell";

		case 515:
			return L"printer";

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

		case 587:
			return L"submission";

		case 631:
			return L"ipp";

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

		case 989:
			return L"ftps-data";

		case 990:
			return L"ftps";

		case 992:
			return L"telnets";

		case 993:
			return L"imaps";

		case 995:
			return L"pop3s";

		case 1029:
			return L"ms-lsa";

		case 1110:
			return L"nfsd-status";

		case 1119:
			return L"bnetgame";

		case 1120:
			return L"bnetfile";

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

		case 3540:
			return L"pnrp-port";

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

		case 6000:
		case 6001:
		case 6002:
		case 6003:
			return L"x11";

		case 6222:
		case 6662: // deprecated!
			return L"radmind";

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

		case 8000:
		case 8443:
			return L"https-alt";

		case 8021:
			return L"ftp-proxy";

		case 8080:
			return L"http-proxy";

		case 8333:
		case 18333:
			return L"bitcoin";

		case 8444:
			return L"http-alt";

		case 8999:
			return L"bctp";

		case 9800:
			return L"webdav";

		case 10107:
			return L"bctp-server";

		case 25565:
			return L"minecraft";

		case 26000:
			return L"quake";

		case 27015:
			return L"halflife";

		case 27017:
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

	return SZ_UNKNOWN;
}

rstring _app_getprotoname (UINT8 proto, ADDRESS_FAMILY af)
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

	return SZ_UNKNOWN;
}

rstring _app_getstatename (DWORD state)
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

	return L"";
}

rstring _app_getservicenamefromtag (HANDLE pid, PVOID ptag)
{
	rstring result;
	const HMODULE hlib = GetModuleHandle (L"advapi32.dll");

	if (hlib)
	{
		typedef ULONG (NTAPI * IQTI) (PVOID, SC_SERVICE_TAG_QUERY_TYPE, PSC_SERVICE_TAG_QUERY); // I_QueryTagInformation
		const IQTI _I_QueryTagInformation = (IQTI)GetProcAddress (hlib, "I_QueryTagInformation");

		if (_I_QueryTagInformation)
		{
			SC_SERVICE_TAG_QUERY nameFromTag;
			SecureZeroMemory (&nameFromTag, sizeof (nameFromTag));

			nameFromTag.ProcessId = HandleToUlong (pid);
			nameFromTag.ServiceTag = PtrToUlong (ptag);

			_I_QueryTagInformation (nullptr, ServiceNameFromTagInformation, &nameFromTag);

			if (nameFromTag.Buffer)
			{
				result = (LPCWSTR)nameFromTag.Buffer;
				LocalFree (nameFromTag.Buffer);
			}
		}
	}

	return result;
}

rstring _app_getnetworkpath (DWORD pid, ULONGLONG * pmodules, size_t * picon_id, size_t * phash)
{
	if (!pid)
	{
		*phash = 0;
		*picon_id = config.icon_id;

		return L"Waiting connections";
	}

	rstring proc_name;

	if (pmodules)
		proc_name = _app_getservicenamefromtag (UlongToHandle (pid), UlongToPtr (*(PULONG)pmodules));

	if (proc_name.IsEmpty ())
	{
		if (pid == PROC_SYSTEM_PID)
		{
			proc_name = PROC_SYSTEM_NAME;
			*picon_id = config.icon_id;
		}
		else
		{
			const HANDLE hprocess = OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);

			if (hprocess)
			{
				DWORD size = 1024;

				const BOOL res = QueryFullProcessImageName (hprocess, 0, proc_name.GetBuffer (size), &size);
				proc_name.ReleaseBuffer ();

				if (res)
				{
					_app_applycasestyle (proc_name.GetBuffer (), proc_name.GetLength ()); // apply case-style
				}
				else
				{
					*phash = 0;
					*picon_id = config.icon_id;

					proc_name.Clear ();
				}

				CloseHandle (hprocess);
			}
		}
	}

	*phash = proc_name.Hash ();

	if (!proc_name.IsEmpty ())
	{
		if (!_app_getappinfo (*phash, InfoIconId, picon_id, sizeof (size_t)))
			_app_getfileicon (proc_name, true, picon_id, nullptr);
	}

	return proc_name;
}

size_t _app_getnetworkhash (ADDRESS_FAMILY af, DWORD pid, PVOID remote_addr, DWORD remote_port, PVOID local_addr, DWORD local_port, UINT8 proto, DWORD state)
{
	WCHAR remote_addr_str[LEN_IP_MAX] = {0};
	WCHAR local_addr_str[LEN_IP_MAX] = {0};

	if (af == AF_INET)
	{
		if (remote_addr)
			RtlIpv4AddressToString ((PIN_ADDR)remote_addr, remote_addr_str);

		if (local_addr)
			RtlIpv4AddressToString ((PIN_ADDR)local_addr, local_addr_str);
	}
	else if (af == AF_INET6)
	{
		if (remote_addr)
			RtlIpv6AddressToString ((PIN6_ADDR)remote_addr, remote_addr_str);

		if (local_addr)
			RtlIpv6AddressToString ((PIN6_ADDR)local_addr, local_addr_str);
	}
	else
	{
		return 0;
	}

	LPCWSTR hash_format = L"%d_%d_%s_%d_%s_%d_%d_%d";

	return _r_str_hash (_r_fmt (hash_format, af, pid, remote_addr_str, remote_port, local_addr_str, local_port, proto, state));
}

bool _app_isvalidconnection (ADDRESS_FAMILY af, PVOID paddr)
{
	if (af == AF_INET)
	{
		return (!IN4_IS_ADDR_UNSPECIFIED (PIN_ADDR (paddr)) &&
				!IN4_IS_ADDR_LOOPBACK (PIN_ADDR (paddr)) &&
				!IN4_IS_ADDR_LINKLOCAL (PIN_ADDR (paddr)) &&
				!IN4_IS_ADDR_MC_ADMINLOCAL (PIN_ADDR (paddr)) &&
				!IN4_IS_ADDR_MC_SITELOCAL (PIN_ADDR (paddr))
				);
	}
	else if (af == AF_INET6)
	{
		return (!IN6_IS_ADDR_UNSPECIFIED (PIN6_ADDR (paddr)) &&
				!IN6_IS_ADDR_LOOPBACK (PIN6_ADDR (paddr)) &&
				!IN6_IS_ADDR_LINKLOCAL (PIN6_ADDR (paddr)) &&
				!IN6_IS_ADDR_SITELOCAL (PIN6_ADDR (paddr)) &&
				!IN6_IS_ADDR_MC_NODELOCAL (PIN6_ADDR (paddr)) &&
				!IN6_IS_ADDR_MC_LINKLOCAL (PIN6_ADDR (paddr)) &&
				!IN6_IS_ADDR_MC_SITELOCAL (PIN6_ADDR (paddr))
				);
	}

	return false;
}

void _app_generate_connections (OBJECTS_MAP& ptr_map, HASHER_MAP& checker_map)
{
	checker_map.clear ();

	DWORD tableSize = 0;
	GetExtendedTcpTable (nullptr, &tableSize, FALSE, AF_INET, TCP_TABLE_OWNER_MODULE_ALL, 0);

	if (tableSize)
	{
		PMIB_TCPTABLE_OWNER_MODULE tcp4Table = new MIB_TCPTABLE_OWNER_MODULE[tableSize];

		if (GetExtendedTcpTable (tcp4Table, &tableSize, FALSE, AF_INET, TCP_TABLE_OWNER_MODULE_ALL, 0) == NO_ERROR)
		{
			for (ULONG i = 0; i < tcp4Table->dwNumEntries; i++)
			{
				IN_ADDR remote_addr = {0};
				IN_ADDR local_addr = {0};

				remote_addr.S_un.S_addr = tcp4Table->table[i].dwRemoteAddr;
				local_addr.S_un.S_addr = tcp4Table->table[i].dwLocalAddr;

				const size_t net_hash = _app_getnetworkhash (AF_INET, tcp4Table->table[i].dwOwningPid, &remote_addr, tcp4Table->table[i].dwRemotePort, &local_addr, tcp4Table->table[i].dwLocalPort, IPPROTO_TCP, tcp4Table->table[i].dwState);

				if (!net_hash || ptr_map.find (net_hash) != ptr_map.end ())
				{
					checker_map[net_hash] = false;
					continue;
				}

				PITEM_NETWORK ptr_network = new ITEM_NETWORK;
				SecureZeroMemory (ptr_network, sizeof (ITEM_NETWORK));

				WCHAR path_name[MAX_PATH] = {0};
				StringCchCopy (path_name, _countof (path_name), _app_getnetworkpath (tcp4Table->table[i].dwOwningPid, tcp4Table->table[i].OwningModuleInfo, &ptr_network->icon_id, &ptr_network->app_hash));

				_r_str_alloc (&ptr_network->path, _r_str_length (path_name), path_name);

				ptr_network->af = AF_INET;
				ptr_network->protocol = IPPROTO_TCP;

				ptr_network->remote_addr.S_un.S_addr = tcp4Table->table[i].dwRemoteAddr;
				ptr_network->remote_port = _byteswap_ushort ((USHORT)tcp4Table->table[i].dwRemotePort);

				ptr_network->local_addr.S_un.S_addr = tcp4Table->table[i].dwLocalAddr;
				ptr_network->local_port = _byteswap_ushort ((USHORT)tcp4Table->table[i].dwLocalPort);

				ptr_network->state = tcp4Table->table[i].dwState;

				if (tcp4Table->table[i].dwState == MIB_TCP_STATE_ESTAB)
				{
					if (_app_isvalidconnection (ptr_network->af, &ptr_network->remote_addr) || _app_isvalidconnection (ptr_network->af, &ptr_network->local_addr))
						ptr_network->is_connection = true;
				}

				_app_formataddress (ptr_network->af, 0, &ptr_network->local_addr, ptr_network->local_port, &ptr_network->local_fmt, FMTADDR_RESOLVE_HOST);
				_app_formataddress (ptr_network->af, 0, &ptr_network->remote_addr, ptr_network->remote_port, &ptr_network->remote_fmt, FMTADDR_RESOLVE_HOST);

				ptr_map[net_hash] = _r_obj_allocate (ptr_network);
				checker_map[net_hash] = true;
			}
		}

		SAFE_DELETE_ARRAY (tcp4Table);
	}

	tableSize = 0;
	GetExtendedTcpTable (nullptr, &tableSize, FALSE, AF_INET6, TCP_TABLE_OWNER_MODULE_ALL, 0);

	if (tableSize)
	{
		PMIB_TCP6TABLE_OWNER_MODULE tcp6Table = new MIB_TCP6TABLE_OWNER_MODULE[tableSize];

		if (GetExtendedTcpTable (tcp6Table, &tableSize, FALSE, AF_INET6, TCP_TABLE_OWNER_MODULE_ALL, 0) == NO_ERROR)
		{
			for (ULONG i = 0; i < tcp6Table->dwNumEntries; i++)
			{
				const size_t net_hash = _app_getnetworkhash (AF_INET6, tcp6Table->table[i].dwOwningPid, tcp6Table->table[i].ucRemoteAddr, tcp6Table->table[i].dwRemotePort, tcp6Table->table[i].ucLocalAddr, tcp6Table->table[i].dwLocalPort, IPPROTO_TCP, tcp6Table->table[i].dwState);

				if (!net_hash || ptr_map.find (net_hash) != ptr_map.end ())
				{
					checker_map[net_hash] = false;
					continue;
				}

				PITEM_NETWORK ptr_network = new ITEM_NETWORK;
				SecureZeroMemory (ptr_network, sizeof (ITEM_NETWORK));

				WCHAR path_name[MAX_PATH] = {0};
				StringCchCopy (path_name, _countof (path_name), _app_getnetworkpath (tcp6Table->table[i].dwOwningPid, tcp6Table->table[i].OwningModuleInfo, &ptr_network->icon_id, &ptr_network->app_hash));

				_r_str_alloc (&ptr_network->path, _r_str_length (path_name), path_name);

				ptr_network->af = AF_INET6;
				ptr_network->protocol = IPPROTO_TCP;

				CopyMemory (ptr_network->remote_addr6.u.Byte, tcp6Table->table[i].ucRemoteAddr, FWP_V6_ADDR_SIZE);
				ptr_network->remote_port = _byteswap_ushort ((USHORT)tcp6Table->table[i].dwRemotePort);

				CopyMemory (ptr_network->local_addr6.u.Byte, tcp6Table->table[i].ucLocalAddr, FWP_V6_ADDR_SIZE);
				ptr_network->local_port = _byteswap_ushort ((USHORT)tcp6Table->table[i].dwLocalPort);

				ptr_network->state = tcp6Table->table[i].dwState;

				if (tcp6Table->table[i].dwState == MIB_TCP_STATE_ESTAB)
				{
					if (_app_isvalidconnection (ptr_network->af, &ptr_network->remote_addr6) || _app_isvalidconnection (ptr_network->af, &ptr_network->local_addr6))
						ptr_network->is_connection = true;
				}

				_app_formataddress (ptr_network->af, 0, &ptr_network->local_addr6, ptr_network->local_port, &ptr_network->local_fmt, FMTADDR_RESOLVE_HOST);
				_app_formataddress (ptr_network->af, 0, &ptr_network->remote_addr6, ptr_network->remote_port, &ptr_network->remote_fmt, FMTADDR_RESOLVE_HOST);

				ptr_map[net_hash] = _r_obj_allocate (ptr_network);
				checker_map[net_hash] = true;
			}
		}

		SAFE_DELETE_ARRAY (tcp6Table);
	}

	tableSize = 0;
	GetExtendedUdpTable (nullptr, &tableSize, FALSE, AF_INET, UDP_TABLE_OWNER_MODULE, 0);

	if (tableSize)
	{
		PMIB_UDPTABLE_OWNER_MODULE udp4Table = new MIB_UDPTABLE_OWNER_MODULE[tableSize];

		if (GetExtendedUdpTable (udp4Table, &tableSize, FALSE, AF_INET, UDP_TABLE_OWNER_MODULE, 0) == NO_ERROR)
		{
			for (ULONG i = 0; i < udp4Table->dwNumEntries; i++)
			{
				IN_ADDR local_addr = {0};
				local_addr.S_un.S_addr = udp4Table->table[i].dwLocalAddr;

				const size_t net_hash = _app_getnetworkhash (AF_INET, udp4Table->table[i].dwOwningPid, nullptr, 0, &local_addr, udp4Table->table[i].dwLocalPort, IPPROTO_UDP, 0);

				if (!net_hash || ptr_map.find (net_hash) != ptr_map.end ())
				{
					checker_map[net_hash] = false;
					continue;
				}

				PITEM_NETWORK ptr_network = new ITEM_NETWORK;
				SecureZeroMemory (ptr_network, sizeof (ITEM_NETWORK));

				WCHAR path_name[MAX_PATH] = {0};
				StringCchCopy (path_name, _countof (path_name), _app_getnetworkpath (udp4Table->table[i].dwOwningPid, udp4Table->table[i].OwningModuleInfo, &ptr_network->icon_id, &ptr_network->app_hash));

				_r_str_alloc (&ptr_network->path, _r_str_length (path_name), path_name);

				ptr_network->af = AF_INET;
				ptr_network->protocol = IPPROTO_UDP;

				ptr_network->local_addr.S_un.S_addr = udp4Table->table[i].dwLocalAddr;
				ptr_network->local_port = _byteswap_ushort ((USHORT)udp4Table->table[i].dwLocalPort);

				ptr_network->state = 0;

				if (_app_isvalidconnection (ptr_network->af, &ptr_network->local_addr))
					ptr_network->is_connection = true;

				_app_formataddress (ptr_network->af, 0, &ptr_network->local_addr, ptr_network->local_port, &ptr_network->local_fmt, FMTADDR_RESOLVE_HOST);

				ptr_map[net_hash] = _r_obj_allocate (ptr_network);
				checker_map[net_hash] = true;
			}
		}

		SAFE_DELETE_ARRAY (udp4Table);
	}

	tableSize = 0;
	GetExtendedUdpTable (nullptr, &tableSize, FALSE, AF_INET6, UDP_TABLE_OWNER_MODULE, 0);

	if (tableSize)
	{
		PMIB_UDP6TABLE_OWNER_MODULE udp6Table = new MIB_UDP6TABLE_OWNER_MODULE[tableSize];

		if (GetExtendedUdpTable (udp6Table, &tableSize, FALSE, AF_INET6, UDP_TABLE_OWNER_MODULE, 0) == NO_ERROR)
		{
			for (ULONG i = 0; i < udp6Table->dwNumEntries; i++)
			{
				const size_t net_hash = _app_getnetworkhash (AF_INET6, udp6Table->table[i].dwOwningPid, nullptr, 0, udp6Table->table[i].ucLocalAddr, udp6Table->table[i].dwLocalPort, IPPROTO_UDP, 0);

				if (!net_hash || ptr_map.find (net_hash) != ptr_map.end ())
				{
					checker_map[net_hash] = false;
					continue;
				}

				PITEM_NETWORK ptr_network = new ITEM_NETWORK;
				SecureZeroMemory (ptr_network, sizeof (ITEM_NETWORK));

				WCHAR path_name[MAX_PATH] = {0};
				StringCchCopy (path_name, _countof (path_name), _app_getnetworkpath (udp6Table->table[i].dwOwningPid, udp6Table->table[i].OwningModuleInfo, &ptr_network->icon_id, &ptr_network->app_hash));

				_r_str_alloc (&ptr_network->path, _r_str_length (path_name), path_name);

				ptr_network->af = AF_INET6;
				ptr_network->protocol = IPPROTO_UDP;

				CopyMemory (ptr_network->local_addr6.u.Byte, udp6Table->table[i].ucLocalAddr, FWP_V6_ADDR_SIZE);
				ptr_network->local_port = _byteswap_ushort ((USHORT)udp6Table->table[i].dwLocalPort);

				ptr_network->state = 0;

				if (_app_isvalidconnection (ptr_network->af, &ptr_network->local_addr6))
					ptr_network->is_connection = true;

				_app_formataddress (ptr_network->af, 0, &ptr_network->local_addr6, ptr_network->local_port, &ptr_network->local_fmt, FMTADDR_RESOLVE_HOST);

				ptr_map[net_hash] = _r_obj_allocate (ptr_network);
				checker_map[net_hash] = true;
			}
		}

		SAFE_DELETE_ARRAY (udp6Table);
	}
}

void _app_generate_packages ()
{
	HKEY hkey = nullptr;
	HKEY hsubkey = nullptr;

	LONG result = RegOpenKeyEx (HKEY_CLASSES_ROOT, L"Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages", 0, KEY_READ, &hkey);

	if (result == ERROR_SUCCESS)
	{
		DWORD index = 0;

		while (true)
		{
			WCHAR key_name[MAX_PATH] = {0};
			DWORD size = _countof (key_name);

			if (RegEnumKeyEx (hkey, index++, key_name, &size, 0, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
				break;

			result = RegOpenKeyEx (hkey, key_name, 0, KEY_READ, &hsubkey);

			if (result == ERROR_SUCCESS)
			{
				PSID package_sid[SECURITY_MAX_SID_SIZE] = {0};

				size = _countof (package_sid) * sizeof (package_sid[0]);
				result = RegQueryValueEx (hsubkey, L"PackageSid", nullptr, nullptr, (LPBYTE)package_sid, &size);

				if (result != ERROR_SUCCESS)
					continue;

				rstring package_sid_string = _r_str_fromsid (package_sid);
				const size_t app_hash = _r_str_hash (package_sid_string);

				if (apps_helper.find (app_hash) != apps_helper.end ())
					continue;

				PITEM_APP_HELPER ptr_item = new ITEM_APP_HELPER;

				ptr_item->type = DataAppUWP;

				WCHAR display_name[MAX_PATH] = {0};
				size = _countof (display_name) * sizeof (display_name[0]);
				result = RegQueryValueEx (hsubkey, L"DisplayName", nullptr, nullptr, (LPBYTE)display_name, &size);

				if (result == ERROR_SUCCESS)
				{
					if (display_name[0] == L'@')
					{
						const HRESULT hrComInit = CoInitialize (nullptr);

						if ((hrComInit == RPC_E_CHANGED_MODE) || SUCCEEDED (hrComInit))
						{
							WCHAR name[MAX_PATH] = {0};

							if (SUCCEEDED (SHLoadIndirectString (display_name, name, _countof (name), nullptr)))
								StringCchCopy (display_name, _countof (display_name), name);

							if (SUCCEEDED (hrComInit))
								CoUninitialize ();
						}
					}
				}

				WCHAR path[MAX_PATH] = {0};
				size = _countof (path) * sizeof (path[0]);
				RegQueryValueEx (hsubkey, L"PackageRootFolder", nullptr, nullptr, (LPBYTE)path, &size);

				// query timestamp
				FILETIME ft = {0};

				if (RegQueryInfoKey (hsubkey, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &ft) == ERROR_SUCCESS)
					ptr_item->timestamp = _r_unixtime_from_filetime (&ft);

				_r_str_alloc (&ptr_item->sid, package_sid_string.GetLength (), package_sid_string);
				_r_str_alloc (&ptr_item->display_name, _r_str_length (display_name), display_name);
				_r_str_alloc (&ptr_item->real_path, _r_str_length (path), path);

				if (!ConvertStringSidToSid (package_sid_string, &ptr_item->pdata))
				{
					SAFE_DELETE (ptr_item);
				}
				else
				{
					_app_load_appxmanifest (ptr_item);

					apps_helper[app_hash] = _r_obj_allocate (ptr_item);
				}

				RegCloseKey (hsubkey);
			}
		}

		RegCloseKey (hkey);
	}
}

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
						RtlInitUnicodeString (&serviceNameUs, (PWSTR)service_name);

						rstring sidstring;

						SID* serviceSid = nullptr;
						ULONG serviceSidLength = 0;

						// get service security identifier
						if (RtlCreateServiceSid (&serviceNameUs, serviceSid, &serviceSidLength) == 0xC0000023 /*STATUS_BUFFER_TOO_SMALL*/)
						{
							serviceSid = new SID[serviceSidLength];

							if (NT_SUCCESS (RtlCreateServiceSid (&serviceNameUs, serviceSid, &serviceSidLength)))
							{
								sidstring = _r_str_fromsid (serviceSid);
							}
							else
							{
								SAFE_DELETE_ARRAY (serviceSid);
							}
						}

						if (serviceSid && !sidstring.IsEmpty ())
						{
							const size_t app_hash = _r_str_hash (service_name);

							if (apps_helper.find (app_hash) != apps_helper.end ())
								continue;

							PITEM_APP_HELPER ptr_item = new ITEM_APP_HELPER;

							ptr_item->type = DataAppService;
							ptr_item->timestamp = timestamp;

							_r_str_alloc (&ptr_item->service_name, _r_str_length (service_name), service_name);
							_r_str_alloc (&ptr_item->display_name, _r_str_length (display_name), display_name);
							_r_str_alloc (&ptr_item->real_path, _r_str_length (real_path), real_path);
							_r_str_alloc (&ptr_item->sid, sidstring.GetLength (), sidstring);

							if (
								!ConvertStringSecurityDescriptorToSecurityDescriptor (_r_fmt (SERVICE_SECURITY_DESCRIPTOR, sidstring.GetString ()).ToUpper (), SDDL_REVISION_1, &ptr_item->pdata, nullptr) ||
								!IsValidSecurityDescriptor (ptr_item->pdata)
								)
							{
								SAFE_DELETE (ptr_item);
							}
							else
							{
								apps_helper[app_hash] = _r_obj_allocate (ptr_item);
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
	static const INT icon_size = GetSystemMetrics (SM_CXSMICON);

	static const HBITMAP hbmp_allow = _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_ALLOW), icon_size);
	static const HBITMAP hbmp_block = _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_BLOCK), icon_size);

	static const HBITMAP hbmp_checked = _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_CHECKED), icon_size);
	static const HBITMAP hbmp_unchecked = _app_bitmapfrompng (app.GetHINSTANCE (), MAKEINTRESOURCE (IDP_UNCHECKED), icon_size);

	ITEM_COUNT stat = {0};
	SecureZeroMemory (&stat, sizeof (stat));

	_app_getcount (&stat);

	if (!app_hash || !stat.rules_count)
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
				if (!stat.rules_predefined_count)
					continue;
			}
			else if (type == 1)
			{
				if (!stat.rules_user_count)
					continue;
			}
			else
			{
				if (!stat.rules_global_count)
					continue;
			}

			AppendMenu (hsubmenu, MF_SEPARATOR, 0, nullptr);

			for (UINT8 loop = 0; loop < 2; loop++)
			{
				for (size_t i = 0; i < rules_arr.size (); i++)
				{
					PR_OBJECT ptr_rule_object = _r_obj_reference (rules_arr.at (i));

					if (!ptr_rule_object)
						continue;

					PITEM_RULE ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

					if (ptr_rule)
					{
						const bool is_global = (ptr_rule->is_enabled && ptr_rule->apps.empty ());
						const bool is_enabled = is_global || (ptr_rule->is_enabled && (ptr_rule->apps.find (app_hash) != ptr_rule->apps.end ()));

						if (ptr_rule->type != DataRuleCustom || (type == 0 && (!ptr_rule->is_readonly || is_global)) || (type == 1 && (ptr_rule->is_readonly || is_global)) || (type == 2 && !is_global))
						{
							_r_obj_dereference (ptr_rule_object, &_app_dereferencerule);
							continue;
						}

						if ((loop == 0 && !is_enabled) || (loop == 1 && is_enabled))
						{
							_r_obj_dereference (ptr_rule_object, &_app_dereferencerule);
							continue;
						}

						WCHAR buffer[128] = {0};
						StringCchPrintf (buffer, _countof (buffer), app.LocaleString (IDS_RULE_APPLY_2, ptr_rule->is_readonly ? SZ_READONLY_RULE : nullptr), ptr_rule->pname);

						MENUITEMINFO mii = {0};

						mii.cbSize = sizeof (mii);
						mii.fMask = MIIM_ID | MIIM_FTYPE | MIIM_STATE | MIIM_STRING | MIIM_BITMAP | MIIM_CHECKMARKS;
						mii.fType = MFT_STRING;
						mii.dwTypeData = buffer;
						mii.hbmpItem = ptr_rule->is_block ? hbmp_block : hbmp_allow;
						mii.hbmpChecked = hbmp_checked;
						mii.hbmpUnchecked = hbmp_unchecked;
						mii.fState = (is_enabled ? MF_CHECKED : MF_UNCHECKED);
						mii.wID = IDX_RULES_SPECIAL + UINT (i);

						if (is_global)
							mii.fState |= MF_DISABLED | MF_GRAYED;

						InsertMenuItem (hsubmenu, mii.wID, FALSE, &mii);
					}

					_r_obj_dereference (ptr_rule_object, &_app_dereferencerule);
				}
			}
		}
	}

	AppendMenu (hsubmenu, MF_SEPARATOR, 0, nullptr);
	AppendMenu (hsubmenu, MF_STRING, IDM_EDITRULES, app.LocaleString (IDS_EDITRULES, nullptr));
	AppendMenu (hsubmenu, MF_STRING, IDM_OPENRULESEDITOR, app.LocaleString (IDS_OPENRULESEDITOR, L"..."));
}

bool _app_item_get (EnumDataType type, size_t app_hash, rstring* display_name, rstring* real_path, PBYTE* lpdata)
{
	if (apps_helper.find (app_hash) == apps_helper.end ())
		return false;

	PR_OBJECT ptr_app_object = _r_obj_reference (apps_helper[app_hash]);

	if (ptr_app_object)
	{
		PITEM_APP_HELPER ptr_app_item = (PITEM_APP_HELPER)ptr_app_object->pdata;

		if (ptr_app_item)
		{
			if (ptr_app_item->type != type)
			{
				_r_obj_dereference (ptr_app_object, &_app_dereferenceappshelper);
				return false;
			}

			if (display_name)
			{
				if (ptr_app_item->display_name && ptr_app_item->display_name[0])
					*display_name = ptr_app_item->display_name;

				else if (ptr_app_item->real_path && ptr_app_item->real_path[0])
					*display_name = ptr_app_item->real_path;

				else if (ptr_app_item->sid && ptr_app_item->sid[0])
					*display_name = ptr_app_item->sid;
			}

			if (real_path)
			{
				if (ptr_app_item->real_path && ptr_app_item->real_path[0])
					*real_path = ptr_app_item->real_path;
			}

			if (lpdata)
			{
				SAFE_DELETE_ARRAY (*lpdata);

				if (ptr_app_item->pdata)
				{
					DWORD length = 0;

					if (type == DataAppService)
						length = GetSecurityDescriptorLength (ptr_app_item->pdata);

					else if (type == DataAppUWP)
						length = SECURITY_MAX_SID_SIZE;

					if (length)
					{
						*lpdata = new BYTE[length];

						CopyMemory (*lpdata, ptr_app_item->pdata, length);
					}
				}
			}

			_r_obj_dereference (ptr_app_object, &_app_dereferenceappshelper);
			return true;
		}

		_r_obj_dereference (ptr_app_object, &_app_dereferenceappshelper);
	}

	return false;
}

INT CALLBACK _app_listviewcompare_callback (LPARAM lparam1, LPARAM lparam2, LPARAM lparam)
{
	const HWND hlistview = (HWND)lparam;
	const HWND hparent = GetParent (hlistview);
	const UINT listview_id = GetDlgCtrlID (hlistview);

	const size_t item1 = _app_getposition (hparent, listview_id, lparam1);
	const size_t item2 = _app_getposition (hparent, listview_id, lparam2);

	if (item1 == LAST_VALUE || item2 == LAST_VALUE)
		return 0;

	const rstring cfg_name = _r_fmt (L"listview\\%04x", listview_id);

	const INT column_id = app.ConfigGet (L"SortColumn", 0, cfg_name).AsInt ();
	const bool is_descend = app.ConfigGet (L"SortIsDescending", false, cfg_name).AsBool ();

	INT result = 0;

	if (listview_id != IDC_NETWORK)
	{
		const bool is_checked1 = _r_listview_isitemchecked (hparent, listview_id, item1);
		const bool is_checked2 = _r_listview_isitemchecked (hparent, listview_id, item2);

		if (is_checked1 != is_checked2)
		{
			if (is_checked1 && !is_checked2)
				result = is_descend ? 1 : -1;

			else if (!is_checked1 && is_checked2)
				result = is_descend ? -1 : 1;
		}
	}

	if (!result)
	{
		// timestamp sorting
		if (column_id == 1 && (listview_id == IDC_APPS_PROFILE || listview_id == IDC_APPS_SERVICE || listview_id == IDC_APPS_UWP))
		{
			time_t timestamp1 = 0;
			time_t timestamp2 = 0;

			_app_getappinfo (lparam1, InfoTimestamp, &timestamp1, sizeof (timestamp1));
			_app_getappinfo (lparam2, InfoTimestamp, &timestamp2, sizeof (timestamp2));

			if (timestamp1 < timestamp2)
				result = -1;

			else if (timestamp1 > timestamp2)
				result = 1;
		}
	}

	if (!result)
	{
		result = StrCmpLogicalW (
			_r_listview_getitemtext (hparent, listview_id, item1, column_id),
			_r_listview_getitemtext (hparent, listview_id, item2, column_id)
		);
	}

	return is_descend ? -result : result;
}

void _app_listviewsort (HWND hwnd, UINT listview_id, INT column, bool is_notifycode)
{
	if (!listview_id)
		return;

	const rstring cfg_name = _r_fmt (L"listview\\%04x", listview_id);

	bool is_descend = app.ConfigGet (L"SortIsDescending", false, cfg_name).AsBool ();

	if (is_notifycode)
		is_descend = !is_descend;

	if (column == -1)
		column = app.ConfigGet (L"SortColumn", 0, cfg_name).AsInt ();

	if (is_notifycode)
	{
		app.ConfigSet (L"SortIsDescending", is_descend, cfg_name);
		app.ConfigSet (L"SortColumn", column, cfg_name);
	}

	for (UINT i = 0; i < _r_listview_getcolumncount (hwnd, listview_id); i++)
		_r_listview_setcolumnsortindex (hwnd, listview_id, i, 0);

	_r_listview_setcolumnsortindex (hwnd, listview_id, column, is_descend ? -1 : 1);

	SendDlgItemMessage (hwnd, listview_id, LVM_SORTITEMS, (WPARAM)GetDlgItem (hwnd, listview_id), (LPARAM)& _app_listviewcompare_callback);
}

void _app_refreshstatus (HWND hwnd)
{
	ITEM_COUNT stat = {0};
	SecureZeroMemory (&stat, sizeof (stat));

	_app_getcount (&stat);

	const HWND hstatus = GetDlgItem (hwnd, IDC_STATUSBAR);
	const HDC hdc = GetDC (hstatus);

	// item count
	if (hdc)
	{
		SelectObject (hdc, (HFONT)SendMessage (hstatus, WM_GETFONT, 0, 0)); // fix

		static const UINT parts_count = 3;
		static const UINT padding = app.GetDPI (8);

		rstring text[parts_count];
		INT parts[parts_count] = {0};
		LONG size[parts_count] = {0};
		LONG lay = 0;

		for (UINT i = 0; i < parts_count; i++)
		{
			switch (i)
			{
				case 0:
				{
					text[i] = app.LocaleString (_wfp_isfiltersinstalled () ? IDS_STATUS_FILTERS_ACTIVE : IDS_STATUS_FILTERS_INACTIVE, nullptr);
					break;
				}

				case 1:
				{
					text[i].Format (L"%s: %d", app.LocaleString (IDS_STATUS_UNUSED_APPS, nullptr).GetString (), stat.apps_unused_count);
					break;
				}

				case 2:
				{
					text[i].Format (L"%s: %d", app.LocaleString (IDS_STATUS_TIMER_APPS, nullptr).GetString (), stat.apps_timer_count);
					break;
				}
			}

			size[i] = _r_dc_fontwidth (hdc, text[i], text[i].GetLength ()) + padding;

			if (i)
				lay += size[i];
		}

		RECT rc = {0};
		GetClientRect (hstatus, &rc);

		parts[0] = _R_RECT_WIDTH (&rc) - lay - GetSystemMetrics (SM_CXVSCROLL);
		parts[1] = parts[0] + size[1];
		parts[2] = parts[1] + size[2];

		SendMessage (hstatus, SB_SETPARTS, parts_count, (LPARAM)parts);

		for (UINT i = 0; i < parts_count; i++)
		{
			SendMessage (hstatus, SB_SETTEXT, MAKEWPARAM (i, 0), (LPARAM)text[i].GetString ());
			SendMessage (hstatus, SB_SETTIPTEXT, i, (LPARAM)text[i].GetString ());
		}

		ReleaseDC (hstatus, hdc);
	}

	// group information
	{
		const UINT listview_id = _app_gettab_id (hwnd);

		if (listview_id)
		{
			if ((SendDlgItemMessage (hwnd, listview_id, LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0) & LVS_EX_CHECKBOXES) != 0)
			{
				const bool is_rules_lv = (listview_id == IDC_RULES_BLOCKLIST || listview_id == IDC_RULES_SYSTEM || listview_id == IDC_RULES_CUSTOM);
				const UINT enabled_group_title = is_rules_lv ? IDS_GROUP_ENABLED : IDS_GROUP_ALLOWED;
				const UINT special_group_title = is_rules_lv ? IDS_GROUP_SPECIAL : IDS_GROUP_SPECIAL_APPS;
				const UINT disabled_group_title = is_rules_lv ? IDS_GROUP_DISABLED : IDS_GROUP_BLOCKED;

				const size_t total_count = _r_listview_getitemcount (hwnd, listview_id);

				size_t group1_count = 0;
				size_t group2_count = 0;
				size_t group3_count = 0;

				for (size_t i = 0; i < total_count; i++)
				{
					LVITEM lvi = {0};

					lvi.mask = LVIF_GROUPID;
					lvi.iItem = (INT)i;

					if (SendDlgItemMessage (hwnd, listview_id, LVM_GETITEM, 0, (LPARAM)& lvi))
					{
						if (lvi.iGroupId == 0)
							group1_count += 1;

						else if (lvi.iGroupId == 1)
							group2_count += 1;

						else
							group3_count += 1;
					}
				}

				_r_listview_setgroup (hwnd, listview_id, 0, app.LocaleString (enabled_group_title, total_count ? _r_fmt (L" (%d/%d)", group1_count, total_count).GetString () : nullptr), 0, 0);
				_r_listview_setgroup (hwnd, listview_id, 1, app.LocaleString (special_group_title, total_count ? _r_fmt (L" (%d/%d)", group2_count, total_count).GetString () : nullptr), 0, 0);
				_r_listview_setgroup (hwnd, listview_id, 2, app.LocaleString (disabled_group_title, total_count ? _r_fmt (L" (%d/%d)", group3_count, total_count).GetString () : nullptr), 0, 0);
			}
		}
	}
}

rstring _app_parsehostaddress_dns (LPCWSTR host, USHORT port)
{
	rstring result;

	PDNS_RECORD ppQueryResultsSet = nullptr;

	// ipv4 address
	DNS_STATUS dnsStatus = DnsQuery (host, DNS_TYPE_A, DNS_QUERY_NO_HOSTS_FILE, nullptr, &ppQueryResultsSet, nullptr);

	if (dnsStatus != DNS_ERROR_RCODE_NO_ERROR)
	{
		if (dnsStatus != DNS_INFO_NO_RECORDS)
			_app_logerror (L"DnsQuery (DNS_TYPE_A)", dnsStatus, host, true);
	}
	else
	{
		if (ppQueryResultsSet)
		{
			for (auto current = ppQueryResultsSet; current != nullptr; current = current->pNext)
			{
				// ipv4 address
				WCHAR str[INET_ADDRSTRLEN] = {0};

				InetNtop (AF_INET, &(current->Data.A.IpAddress), str, _countof (str));

				result.Append (str);

				if (port)
					result.AppendFormat (L":%d", port);

				result.Append (DIVIDER_RULE);
			}

			DnsRecordListFree (ppQueryResultsSet, DnsFreeRecordList);
		}
	}

	// ipv6 address
	dnsStatus = DnsQuery (host, DNS_TYPE_AAAA, DNS_QUERY_NO_HOSTS_FILE, nullptr, &ppQueryResultsSet, nullptr);

	if (dnsStatus != DNS_ERROR_RCODE_NO_ERROR)
	{
		if (dnsStatus != DNS_INFO_NO_RECORDS)
			_app_logerror (L"DnsQuery (DNS_TYPE_AAAA)", dnsStatus, host, true);
	}
	else
	{
		if (ppQueryResultsSet)
		{
			for (auto current = ppQueryResultsSet; current != nullptr; current = current->pNext)
			{
				WCHAR str[INET6_ADDRSTRLEN] = {0};
				InetNtop (AF_INET6, &(current->Data.AAAA.Ip6Address), str, _countof (str));

				result.Append (str);

				if (port)
					result.AppendFormat (L":%d", port);

				result.Append (DIVIDER_RULE);
			}

			DnsRecordListFree (ppQueryResultsSet, DnsFreeRecordList);
		}
	}

	return result.Trim (DIVIDER_RULE);
}

rstring _app_parsehostaddress_wsa (LPCWSTR hostname, USHORT port)
{
	if (!hostname || !hostname[0] || !app.ConfigGet (L"IsEnableWsaResolver", true).AsBool ())
		return L"";

	// initialize winsock (required by getnameinfo)
	WSADATA wsaData = {0};
	INT ret_code = WSAStartup (WINSOCK_VERSION, &wsaData);

	if (ret_code != ERROR_SUCCESS)
	{
		_app_logerror (L"WSAStartup", ret_code, nullptr, true);
		return L"";
	}

	rstring result;

	ADDRINFOEXW hints = {0};
	ADDRINFOEXW * ppQueryResultsSet = nullptr;

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	LPGUID lpNspid = nullptr;
	ret_code = GetAddrInfoEx (hostname, L"domain", NS_DNS, lpNspid, &hints, &ppQueryResultsSet, nullptr, nullptr, nullptr, nullptr);

	if (ret_code != ERROR_SUCCESS || !ppQueryResultsSet)
	{
		_app_logerror (L"GetAddrInfoEx", ret_code, hostname, true);
		return L"";
	}
	else
	{
		for (auto current = ppQueryResultsSet; current != nullptr; current = current->ai_next)
		{
			WCHAR printableIP[INET6_ADDRSTRLEN] = {0};

			if (current->ai_family == AF_INET)
			{
				struct sockaddr_in* sock_in4 = (struct sockaddr_in*)current->ai_addr;
				PIN_ADDR addr4 = &(sock_in4->sin_addr);

				if (IN4_IS_ADDR_UNSPECIFIED (addr4))
					continue;

				InetNtop (current->ai_family, addr4, printableIP, _countof (printableIP));
			}
			else if (current->ai_family == AF_INET6)
			{
				struct sockaddr_in6* sock_in6 = (struct sockaddr_in6*)current->ai_addr;
				PIN6_ADDR addr6 = &(sock_in6->sin6_addr);

				if (IN6_IS_ADDR_UNSPECIFIED (addr6))
					continue;

				InetNtop (current->ai_family, addr6, printableIP, _countof (printableIP));
			}

			if (!printableIP[0])
				continue;

			result.Append (printableIP);

			if (port)
				result.AppendFormat (L":%d", port);

			result.Append (DIVIDER_RULE);
		}

		result.Trim (DIVIDER_RULE);
	}

	FreeAddrInfoEx (ppQueryResultsSet);

	WSACleanup ();

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
				const size_t dns_hash = _r_str_hash (ni.NamedAddress.Address);

				_r_fastlock_acquireshared (&lock_cache);
				const bool is_exists = cache_dns.find (dns_hash) != cache_dns.end ();
				_r_fastlock_releaseshared (&lock_cache);

				if (is_exists)
				{
					_r_fastlock_acquireshared (&lock_cache);
					PR_OBJECT ptr_cache_object = _r_obj_reference (cache_dns[dns_hash]);
					_r_fastlock_releaseshared (&lock_cache);

					if (ptr_cache_object)
					{
						if (ptr_cache_object->pdata)
							StringCchCopy (paddr_dns, dns_length, (LPCWSTR)ptr_cache_object->pdata);

						_r_obj_dereference (ptr_cache_object, &_app_dereferencestring);
					}

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
					StringCchCopy (paddr_dns, dns_length, host);

					LPWSTR ptr_cache = nullptr;

					if (_r_str_alloc (&ptr_cache, host.GetLength (), host))
					{
						_r_fastlock_acquireexclusive (&lock_cache);

						_app_freeobjects_map (cache_dns, &_app_dereferencestring, false);
						cache_dns[dns_hash] = _r_obj_allocate (ptr_cache);

						_r_obj_reference (cache_dns[dns_hash]);

						_r_fastlock_releaseexclusive (&lock_cache);
					}

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

	if (rule.CompareNoCase (L"*") == 0)
		return true;

	EnumDataType type = DataUnknown;
	const size_t range_pos = rule.Find (DIVIDER_RULE_RANGE);
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
				type = DataTypePort;
			}
			else if (is_range ? (_app_isruleip (range_start) && _app_isruleip (range_end)) : _app_isruleip (rule))
			{
				type = DataTypeIp;
			}
			else if (_app_isrulehost (rule))
			{
				type = DataTypeHost;
			}

			if (type != DataUnknown)
			{
				_r_fastlock_acquireexclusive (&lock_cache);

				if (cache_types.size () >= UMAP_CACHE_LIMIT)
					cache_types.clear ();

				cache_types[hash] = type;

				_r_fastlock_releaseexclusive (&lock_cache);
			}
		}
	}

	if (type == DataUnknown)
		return false;

	if (!ptr_addr)
		return true;

	if (type == DataTypeHost)
		is_range = false;

	ptr_addr->is_range = is_range;

	if (type == DataTypePort)
	{
		if (!is_range)
		{
			// ...port
			ptr_addr->type = DataTypePort;
			ptr_addr->port = (UINT16)rule.AsUint ();

			return true;
		}
		else
		{
			// ...port range
			ptr_addr->type = DataTypePort;

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

		if (type == DataTypeIp && is_range)
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
			ptr_addr->type = DataTypeIp;
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

				ptr_addr->type = DataTypeIp;
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

bool _app_resolveaddress (ADDRESS_FAMILY af, LPVOID paddr, LPWSTR * pbuffer)
{
	if (!pbuffer || (af != AF_INET && af != AF_INET6))
		return false;

	bool result = false;

	LPWSTR pstraddr = nullptr;
	_app_formataddress (af, 0, paddr, 0, &pstraddr, FMTADDR_AS_ARPA);

	if (pstraddr)
	{
		PDNS_RECORD ppQueryResultsSet = nullptr;
		const DNS_STATUS dnsStatus = DnsQuery (pstraddr, DNS_TYPE_PTR, DNS_QUERY_NO_HOSTS_FILE, nullptr, &ppQueryResultsSet, nullptr);

		if (dnsStatus == DNS_ERROR_RCODE_NO_ERROR)
		{
			if (ppQueryResultsSet)
				result = _r_str_alloc (pbuffer, _r_str_length (ppQueryResultsSet->Data.PTR.pNameHost), ppQueryResultsSet->Data.PTR.pNameHost);
		}

		DnsRecordListFree (ppQueryResultsSet, DnsFreeRecordList);
	}

	SAFE_DELETE_ARRAY (pstraddr);

	return result;
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

UINT _app_getlistview_id (EnumDataType type)
{
	if (type == DataAppService)
		return IDC_APPS_SERVICE;

	else if (type == DataAppUWP)
		return IDC_APPS_UWP;

	else if (type == DataAppRegular || type == DataAppDevice || type == DataAppNetwork || type == DataAppPico)
		return IDC_APPS_PROFILE;

	else if (type == DataRuleBlocklist)
		return IDC_RULES_BLOCKLIST;

	else if (type == DataRuleSystem)
		return IDC_RULES_SYSTEM;

	else if (type == DataRuleCustom)
		return IDC_RULES_CUSTOM;

	return 0;
}

size_t _app_getposition (HWND hwnd, UINT listview_id, size_t lparam)
{
	LVFINDINFO lvfi = {0};

	lvfi.flags = LVFI_PARAM;
	lvfi.lParam = lparam;

	INT pos = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_FINDITEM, (WPARAM)-1, (LPARAM)& lvfi);

	if (pos == -1)
		return LAST_VALUE;

	return (size_t)pos;
}

void _app_showitem (HWND hwnd, UINT listview_id, size_t item, INT scroll_pos)
{
	if (!listview_id)
		return;

	_app_settab_id (hwnd, listview_id);

	const size_t total_count = _r_listview_getitemcount (hwnd, listview_id);

	if (!total_count)
		return;

	const HWND hlistview = GetDlgItem (hwnd, listview_id);

	if (item != LAST_VALUE)
	{
		item = std::clamp (item, size_t (0), total_count - 1);

		SendMessage (hlistview, LVM_ENSUREVISIBLE, (WPARAM)item, TRUE); // ensure item visible

		ListView_SetItemState (hlistview, -1, 0, LVIS_SELECTED | LVIS_FOCUSED); // deselect all
		ListView_SetItemState (hlistview, (WPARAM)item, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED); // select item
	}

	if (scroll_pos > 0)
		SendMessage (hlistview, LVM_SCROLL, 0, scroll_pos); // restore scroll position
}

HBITMAP _app_bitmapfromico (HICON hicon, INT icon_size)
{
	if (!hicon)
		return nullptr;

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

HBITMAP _app_bitmapfrompng (HINSTANCE hinst, LPCWSTR name, INT icon_size)
{
	bool success = false;

	UINT frameCount = 0;
	ULONG resourceLength = 0;
	HDC screenHdc = nullptr;
	HDC hdc = nullptr;
	BITMAPINFO bi = {0};
	HBITMAP hbitmap = nullptr;
	PVOID bitmapBuffer = nullptr;
	IWICStream* wicStream = nullptr;
	IWICBitmapSource* wicBitmapSource = nullptr;
	IWICBitmapDecoder* wicDecoder = nullptr;
	IWICBitmapFrameDecode* wicFrame = nullptr;
	IWICImagingFactory* wicFactory = nullptr;
	IWICBitmapScaler* wicScaler = nullptr;
	WICPixelFormatGUID pixelFormat;
	WICRect rect = {0, 0, icon_size, icon_size};

	const HRESULT hrComInit = CoInitializeEx (nullptr, COINIT_MULTITHREADED);

	if (FAILED (hrComInit) && (hrComInit != RPC_E_CHANGED_MODE))
		goto DoExit;

	// Create the ImagingFactory
	if (FAILED (CoCreateInstance (CLSID_WICImagingFactory1, nullptr, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, (LPVOID*)& wicFactory)))
		goto DoExit;

	// Load the resource
	WICInProcPointer resourceBuffer = (WICInProcPointer)_app_loadresource (hinst, name, L"PNG", &resourceLength);

	if (!resourceBuffer)
		goto DoExit;

	// Create the Stream
	if (FAILED (wicFactory->CreateStream (&wicStream)))
		goto DoExit;

	// Initialize the Stream from Memory
	if (FAILED (wicStream->InitializeFromMemory (resourceBuffer, resourceLength)))
		goto DoExit;

	if (FAILED (wicFactory->CreateDecoder (GUID_ContainerFormatPng, nullptr, &wicDecoder)))
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
	if (memcmp (&pixelFormat, &GUID_WICPixelFormat32bppPRGBA, sizeof (GUID)) == 0)
	{
		wicBitmapSource = (IWICBitmapSource*)wicFrame;
	}
	else
	{
		IWICFormatConverter* wicFormatConverter = nullptr;

		if (FAILED (wicFactory->CreateFormatConverter (&wicFormatConverter)))
			goto DoExit;

		if (FAILED (wicFormatConverter->Initialize (
			(IWICBitmapSource*)wicFrame,
			GUID_WICPixelFormat32bppPBGRA,
			WICBitmapDitherTypeNone,
			nullptr,
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

	screenHdc = GetDC (nullptr);
	hdc = CreateCompatibleDC (screenHdc);
	hbitmap = CreateDIBSection (screenHdc, &bi, DIB_RGB_COLORS, &bitmapBuffer, nullptr, 0);

	if (FAILED (wicFactory->CreateBitmapScaler (&wicScaler)))
		goto DoExit;

	if (FAILED (wicScaler->Initialize (wicBitmapSource, rect.Width, rect.Height, WICBitmapInterpolationModeFant)))
		goto DoExit;

	if (FAILED (wicScaler->CopyPixels (&rect, rect.Width * 4, rect.Width * rect.Height * 4, (PBYTE)bitmapBuffer)))
		goto DoExit;

	success = true;

DoExit:

	if (wicScaler)
		wicScaler->Release ();

	if (hdc)
		DeleteDC (hdc);

	if (screenHdc)
		ReleaseDC (nullptr, screenHdc);

	if (wicBitmapSource)
		wicBitmapSource->Release ();

	if (wicStream)
		wicStream->Release ();

	if (wicDecoder)
		wicDecoder->Release ();

	if (wicFactory)
		wicFactory->Release ();

	if (SUCCEEDED (hrComInit))
		CoUninitialize ();

	if (!success)
	{
		DeleteObject (hbitmap);
		return nullptr;
	}

	return hbitmap;
}

void _app_load_appxmanifest (PITEM_APP_HELPER ptr_app_item)
{
	if (!ptr_app_item || !ptr_app_item->real_path || !ptr_app_item->real_path[0])
		return;

	rstring result;
	rstring path;

	static LPCWSTR appx_names[] = {
		L"AppxManifest.xml",
		L"VSAppxManifest.xml",
	};

	for (size_t i = 0; i < _countof (appx_names); i++)
	{
		path.Format (L"%s\\%s", ptr_app_item->real_path, appx_names[i]);

		if (_r_fs_exists (path))
			goto DoOpen;
	}

	return;

DoOpen:

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
					result.Format (L"%s\\%s", ptr_app_item->real_path, item.attribute (L"Executable").as_string ());

					if (_r_fs_exists (result))
						break;
				}
			}
		}
	}

	_r_str_alloc (&ptr_app_item->real_path, result.GetLength (), result.GetString ());
}

LPVOID _app_loadresource (HINSTANCE hinst, LPCWSTR res, LPCWSTR type, PDWORD psize)
{
	HRSRC hres = FindResource (hinst, res, type);

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
					if (psize)
						*psize = dwResourceSize;

					return pLockedResource;
				}
			}
		}
	}

	return nullptr;
}
