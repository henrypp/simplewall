// simplewall
// Copyright (c) 2016, 2017 Henry++

#include <winsock2.h>
#include <ws2ipdef.h>
#include <windns.h>
#include <mstcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <subauth.h>
#include <fwpmu.h>
#include <dbt.h>
#include <aclapi.h>
#include <wtsapi32.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <sddl.h>
#include <ws2tcpip.h>
#include <wintrust.h>
#include <softpub.h>
#include <netfw.h>
#include <algorithm>

#include "main.hpp"
#include "rapp.hpp"
#include "routine.hpp"

#include "pugiconfig.hpp"
#include "..\..\pugixml\src\pugixml.hpp"

#include "resource.hpp"

CONST UINT WM_FINDMSGSTRING = RegisterWindowMessage (FINDMSGSTRING);

rapp app (APP_NAME, APP_NAME_SHORT, APP_VERSION, APP_COPYRIGHT);

std::unordered_map<size_t, ITEM_APPLICATION> apps;
std::unordered_map<size_t, __time64_t> notifications_last;

std::unordered_map<size_t, LPWSTR> cache_signatures;
std::unordered_map<size_t, LPWSTR> cache_versions;

std::unordered_map<rstring, bool, rstring::hash, rstring::is_equal> rules_config;

std::vector<ITEM_COLOR> colors;
std::vector<ITEM_PACKAGE> packages;
std::vector<ITEM_PROCESS> processes;
std::vector<ITEM_PROTOCOL> protocols;

std::vector<ITEM_RULE*> rules_blocklist;
std::vector<ITEM_RULE*> rules_system;
std::vector<ITEM_RULE*> rules_custom;

std::vector<ITEM_LOG*> notifications;

STATIC_DATA config;

FWPM_SESSION session;

EXTERN_C const IID IID_IImageList;

_R_FASTLOCK lock_apply;
_R_FASTLOCK lock_access;
_R_FASTLOCK lock_writelog;

bool _wfp_initialize (bool is_full);
void _wfp_uninitialize (bool is_full);

BOOL initializer_callback (HWND hwnd, DWORD msg, LPVOID, LPVOID);

UINT WINAPI ApplyThread (LPVOID lparam);
LRESULT CALLBACK NotificationProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

void _app_notifysettimeout (HWND hwnd, UINT_PTR id, bool is_create, UINT timeout);
bool _app_notifyrefresh ();

bool _wfp_logsubscribe ();
bool _wfp_logunsubscribe ();

bool messageFlag (HWND hwnd, LPCWSTR config_cfg, LPCWSTR text, LPCWSTR flag_text)
{
	if (!app.ConfigGet (config_cfg, true).AsBool ())
		return true;

	WCHAR main[512] = {0};
	WCHAR flag[64] = {0};

	INT result = 0;
	BOOL is_flagchecked = 0;

	TASKDIALOGCONFIG tdc = {0};

	tdc.cbSize = sizeof (tdc);
	tdc.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
	tdc.hwndParent = hwnd;
	tdc.pszWindowTitle = APP_NAME;
	tdc.pfCallback = &_r_msg_callback;
	tdc.pszMainIcon = TD_WARNING_ICON;
	tdc.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
	tdc.pszContent = main;
	tdc.pszVerificationText = flag;

	if (!app.ConfigGet (config_cfg, true).AsBool ())
		tdc.dwFlags |= TDF_VERIFICATION_FLAG_CHECKED;

	StringCchCopy (main, _countof (main), text);
	StringCchCopy (flag, _countof (flag), flag_text);

	if (_r_msg_taskdialog (&tdc, &result, nullptr, &is_flagchecked))
	{
		if (result == IDYES)
		{
			app.ConfigSet (config_cfg, is_flagchecked ? false : true);
			return true;
		}
	}

	return false;
}

void _app_logerror (LPCWSTR fn, DWORD result, LPCWSTR desc, bool is_nopopups = false)
{
	_r_dbg_write (APP_NAME_SHORT, APP_VERSION, fn, result, desc);

	if (!is_nopopups && app.ConfigGet (L"IsErrorNotificationsEnabled", true).AsBool ()) // check for timeout (sec.)
	{
		config.is_popuperrors = true;

		app.TrayPopup (UID, NIIF_USER | (app.ConfigGet (L"IsNotificationsSilent", false).AsBool () ? NIIF_NOSOUND : 0), APP_NAME, I18N (&app, IDS_STATUS_ERROR, 0));
	}
}

void _mps_changeconfig (bool is_stop)
{
	DWORD result = 0;
	bool is_started = false;

	SC_HANDLE scm = OpenSCManager (nullptr, nullptr, SC_MANAGER_ALL_ACCESS);

	if (!scm)
	{
		_app_logerror (L"OpenSCManager", GetLastError (), nullptr);
	}
	else
	{
		LPCWSTR arr[] = {
			L"mpssvc",
			L"mpsdrv",
		};

		for (INT i = 0; i < _countof (arr); i++)
		{
			SC_HANDLE sc = OpenService (scm, arr[i], SERVICE_CHANGE_CONFIG | SERVICE_QUERY_STATUS | SERVICE_STOP);

			if (!sc)
			{
				result = GetLastError ();

				if (result != ERROR_ACCESS_DENIED)
					_app_logerror (L"OpenService", GetLastError (), arr[i]);
			}
			else
			{
				if (!is_started)
				{
					SERVICE_STATUS status;

					if (QueryServiceStatus (sc, &status))
					{
						is_started = (status.dwCurrentState == SERVICE_RUNNING);
					}
				}

				if (!ChangeServiceConfig (sc, SERVICE_NO_CHANGE, is_stop ? SERVICE_DISABLED : SERVICE_AUTO_START, SERVICE_NO_CHANGE, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr))
				{
					_app_logerror (L"ChangeServiceConfig", GetLastError (), arr[i]);
				}

				CloseServiceHandle (sc);
			}
		}

		// start services
		if (is_stop)
		{
			_r_run (nullptr, L"netsh advfirewall set allprofiles state off", nullptr, SW_HIDE);
		}
		else
		{
			for (INT i = 0; i < _countof (arr); i++)
			{
				SC_HANDLE sc = OpenService (scm, arr[i], SERVICE_QUERY_STATUS | SERVICE_START);

				if (!sc)
				{
					_app_logerror (L"OpenService", GetLastError (), arr[i]);
				}
				else
				{
					DWORD dwBytesNeeded = 0;
					SERVICE_STATUS_PROCESS ssp = {0};

					if (!QueryServiceStatusEx (sc, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof (ssp), &dwBytesNeeded))
					{
						_app_logerror (L"QueryServiceStatusEx", GetLastError (), arr[i]);
					}
					else
					{
						if (ssp.dwCurrentState != SERVICE_RUNNING)
						{
							if (!StartService (sc, 0, nullptr))
							{
								_app_logerror (L"StartService", GetLastError (), arr[i]);
							}
						}

						CloseServiceHandle (sc);
					}
				}
			}

			_r_sleep (250);

			_r_run (nullptr, L"cmd /c netsh advfirewall set allprofiles state on & netsh advfirewall set allprofiles firewallpolicy blockinbound,allowoutbound", nullptr, SW_HIDE);
		}

		CloseServiceHandle (scm);
	}
}

void _app_listviewresize (HWND hwnd, UINT ctrl_id)
{
	if (!app.ConfigGet (L"AutoSizeColumns", true).AsBool ())
		return;

	RECT rc = {0};
	GetWindowRect (GetDlgItem (hwnd, ctrl_id), &rc);

	const INT width = (rc.right - rc.left) - GetSystemMetrics (SM_CXVSCROLL);

	const INT cx2 = (std::max) (app.GetDPI (110), (std::min) (app.GetDPI (190), _R_PERCENT_VAL (28, width)));
	const INT cx1 = width - cx2;

	_r_listview_setcolumn (hwnd, ctrl_id, 0, nullptr, cx1);
	_r_listview_setcolumn (hwnd, ctrl_id, 1, nullptr, cx2);
}

void _app_listviewsetimagelist (HWND hwnd, UINT ctrl_id)
{
	HIMAGELIST h = nullptr;

	const bool is_large = app.ConfigGet (L"IsLargeIcons", false).AsBool () && ctrl_id == IDC_LISTVIEW;
	const bool is_iconshidden = app.ConfigGet (L"IsIconsHidden", false).AsBool ();

	if (SUCCEEDED (SHGetImageList (is_large ? SHIL_LARGE : SHIL_SMALL, IID_IImageList, (LPVOID*)&h)))
	{
		SendDlgItemMessage (hwnd, ctrl_id, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)h);
		SendDlgItemMessage (hwnd, ctrl_id, LVM_SETIMAGELIST, LVSIL_NORMAL, (LPARAM)h);
	}

	if (ctrl_id != IDC_LISTVIEW)
		return;

	SendDlgItemMessage (hwnd, ctrl_id, LVM_SCROLL, 0, GetScrollPos (GetDlgItem (hwnd, ctrl_id), SB_VERT)); // scrollbar-hack!!!

	CheckMenuRadioItem (GetMenu (hwnd), IDM_ICONSSMALL, IDM_ICONSLARGE, (is_large ? IDM_ICONSLARGE : IDM_ICONSSMALL), MF_BYCOMMAND);
	CheckMenuItem (GetMenu (hwnd), IDM_ICONSISHIDDEN, MF_BYCOMMAND | (is_iconshidden ? MF_CHECKED : MF_UNCHECKED));
}

bool _app_listviewinitfont (PLOGFONT plf)
{
	if (!plf)
		return false;

	rstring buffer = app.ConfigGet (L"Font", UI_FONT_DEFAULT);

	if (buffer.IsEmpty ())
	{
		return false;
	}
	else
	{
		rstring::rvector vc = buffer.AsVector (L";");

		for (size_t i = 0; i < vc.size (); i++)
		{
			vc.at (i).Trim (L" \r\n");

			if (vc.at (i).IsEmpty ())
				continue;

			if (i == 0)
			{
				StringCchCopy (plf->lfFaceName, LF_FACESIZE, vc.at (i));
			}
			else if (i == 1)
			{
				plf->lfHeight = _r_dc_fontsizetoheight (vc.at (i).AsInt ());
			}
			else if (i == 2)
			{
				plf->lfWeight = vc.at (i).AsInt ();
			}
			else
			{
				break;
			}
		}
	}

	// fill missed font values
	{
		NONCLIENTMETRICS ncm = {0};
		ncm.cbSize = sizeof (ncm);

		if (SystemParametersInfo (SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0))
		{
			PLOGFONT pdeflf = &ncm.lfMessageFont;

			if (!plf->lfFaceName[0])
				StringCchCopy (plf->lfFaceName, LF_FACESIZE, pdeflf->lfFaceName);

			if (!plf->lfHeight)
				plf->lfHeight = pdeflf->lfHeight;

			if (!plf->lfWeight)
				plf->lfWeight = pdeflf->lfWeight;

			// set default values
			plf->lfCharSet = DEFAULT_CHARSET;
			plf->lfQuality = CLEARTYPE_QUALITY;
		}
	}

	return true;
}

void _app_listviewsetfont (HWND hwnd, UINT ctrl_id)
{
	LOGFONT lf = {0};

	if (!config.hfont)
	{
		if (_app_listviewinitfont (&lf))
		{
			config.hfont = CreateFontIndirect (&lf);

			if (config.hfont)
				SendDlgItemMessage (hwnd, ctrl_id, WM_SETFONT, (WPARAM)config.hfont, TRUE);

			return;
		}
		else
		{
			SendDlgItemMessage (hwnd, ctrl_id, WM_SETFONT, 0, TRUE);
		}
	}
	else
	{
		SendDlgItemMessage (hwnd, ctrl_id, WM_SETFONT, (WPARAM)config.hfont, TRUE);
	}

}

void ShowItem (HWND hwnd, UINT ctrl_id, size_t item, INT scroll_pos)
{
	if (item != LAST_VALUE)
	{
		ListView_SetItemState (GetDlgItem (hwnd, ctrl_id), -1, 0, LVIS_SELECTED); // deselect all
		ListView_SetItemState (GetDlgItem (hwnd, ctrl_id), item, LVIS_SELECTED, LVIS_SELECTED); // select item

		if (scroll_pos == -1)
			SendDlgItemMessage (hwnd, ctrl_id, LVM_ENSUREVISIBLE, item, TRUE); // ensure item visible
	}

	if (scroll_pos != -1)
		SendDlgItemMessage (hwnd, ctrl_id, LVM_SCROLL, 0, scroll_pos); // restore vscroll position
}

void _app_refreshstatus (HWND hwnd, bool first_part, bool second_part)
{
	if (first_part)
	{
		WCHAR buffer[128] = {0};
		StringCchPrintf (buffer, _countof (buffer), I18N (&app, IDS_STATUS_TOTAL, 0), apps.size ());

		const size_t selection_count = SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_GETSELECTEDCOUNT, 0, 0);

		if (selection_count)
		{
			StringCchCat (buffer, _countof (buffer), L" / ");
			StringCchCat (buffer, _countof (buffer), _r_fmt (I18N (&app, IDS_STATUS_SELECTED, 0), selection_count));
		}

		_r_status_settext (hwnd, IDC_STATUSBAR, 0, buffer);
	}

	if (second_part)
	{
		size_t group1_count = 0;
		size_t group2_count = 0;

		for (auto const &p : apps)
		{
			if (p.second.is_enabled)
				group1_count += 1;
		}

		group2_count = (_r_listview_getitemcount (hwnd, IDC_LISTVIEW) - group1_count);

		switch (app.ConfigGet (L"Mode", ModeWhitelist).AsUint ())
		{
			case ModeWhitelist:
			{
				_r_listview_setgroup (hwnd, IDC_LISTVIEW, 0, _r_fmt (L"%s (%d)", I18N (&app, IDS_GROUP_ALLOWED, 0), group1_count));
				_r_listview_setgroup (hwnd, IDC_LISTVIEW, 1, _r_fmt (L"%s (%d)", I18N (&app, IDS_GROUP_BLOCKED, 0), group2_count));

				break;
			}

			case ModeBlacklist:
			{
				_r_listview_setgroup (hwnd, IDC_LISTVIEW, 0, _r_fmt (L"%s (%d)", I18N (&app, IDS_GROUP_BLOCKED, 0), group1_count));
				_r_listview_setgroup (hwnd, IDC_LISTVIEW, 1, _r_fmt (L"%s (%d)", I18N (&app, IDS_GROUP_ALLOWED, 0), group2_count));

				break;
			}
		}
	}
}

bool _app_getversion (size_t hash, LPCWSTR path, LPCWSTR* pinfo)
{
	if (!pinfo)
		return false;

	if (cache_versions.find (hash) != cache_versions.end ())
	{
		*pinfo = cache_versions[hash];

		return (cache_versions[hash] != nullptr);
	}

	bool result = false;
	rstring buffer;

	cache_versions[hash] = nullptr;

	HINSTANCE h = LoadLibraryEx (path, nullptr, DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_DATAFILE);

	if (h)
	{
		HRSRC hv = FindResource (h, MAKEINTRESOURCE (VS_VERSION_INFO), RT_VERSION);

		if (hv)
		{
			HGLOBAL hg = LoadResource (h, hv);

			if (hg)
			{
				LPVOID versionInfo = LockResource (hg);

				if (versionInfo)
				{
					UINT vLen = 0, langD = 0;
					LPVOID retbuf = nullptr;

					WCHAR author_entry[64] = {0};
					WCHAR description_entry[64] = {0};
					WCHAR version_entry[64] = {0};

					if (VerQueryValue (versionInfo, L"\\VarFileInfo\\Translation", &retbuf, &vLen) && vLen == 4)
					{
						memcpy_s (&langD, 4, retbuf, 4);
						StringCchPrintf (author_entry, _countof (author_entry), L"\\StringFileInfo\\%02X%02X%02X%02X\\CompanyName", (langD & 0xff00) >> 8, langD & 0xff, (langD & 0xff000000) >> 24, (langD & 0xff0000) >> 16);
						StringCchPrintf (description_entry, _countof (description_entry), L"\\StringFileInfo\\%02X%02X%02X%02X\\FileDescription", (langD & 0xff00) >> 8, langD & 0xff, (langD & 0xff000000) >> 24, (langD & 0xff0000) >> 16);
						StringCchPrintf (version_entry, _countof (version_entry), L"\\StringFileInfo\\%02X%02X%02X%02X\\FileVersion", (langD & 0xff00) >> 8, langD & 0xff, (langD & 0xff000000) >> 24, (langD & 0xff0000) >> 16);
					}
					else
					{
						StringCchPrintf (author_entry, _countof (author_entry), L"\\StringFileInfo\\%04X04B0\\CompanyName", GetUserDefaultLangID ());
						StringCchPrintf (description_entry, _countof (description_entry), L"\\StringFileInfo\\%04X04B0\\FileDescription", GetUserDefaultLangID ());
						StringCchPrintf (version_entry, _countof (version_entry), L"\\StringFileInfo\\%04X04B0\\FileVersion", GetUserDefaultLangID ());
					}

					if (VerQueryValue (versionInfo, description_entry, &retbuf, &vLen))
					{
						buffer.Append (TAB_SPACE);
						buffer.Append (static_cast<LPCWSTR>(retbuf));

						UINT length = 0;
						VS_FIXEDFILEINFO* verInfo = nullptr;

						if (VerQueryValue (versionInfo, L"\\", reinterpret_cast<LPVOID*>(&verInfo), &length))
						{
							buffer.Append (_r_fmt (L" %d.%d", HIWORD (verInfo->dwProductVersionMS), LOWORD (verInfo->dwProductVersionMS)));

							if (HIWORD (verInfo->dwProductVersionLS) || LOWORD (verInfo->dwProductVersionLS))
							{
								buffer.Append (_r_fmt (L".%d", HIWORD (verInfo->dwProductVersionLS)));

								if (LOWORD (verInfo->dwProductVersionLS))
									buffer.Append (_r_fmt (L".%d", LOWORD (verInfo->dwProductVersionLS)));
							}
						}

						buffer.Append (L"\r\n");
					}

					if (VerQueryValue (versionInfo, author_entry, &retbuf, &vLen))
					{
						buffer.Append (TAB_SPACE);
						buffer.Append (static_cast<LPCWSTR>(retbuf));
						buffer.Append (L"\r\n");
					}

					buffer.Trim (L"\r\n ");

					// get signature information
					{
						LPWSTR ppointer = (LPWSTR)malloc ((buffer.GetLength () + 1) * sizeof (WCHAR));

						if (ppointer)
							StringCchCopy (ppointer, buffer.GetLength () + 1, buffer);

						*pinfo = ppointer;
						cache_versions[hash] = ppointer;
					}

					result = true;
				}
			}

			UnlockResource (hg);
			FreeResource (hg);
		}

		FreeLibrary (h);
	}

	return result;
}

bool _app_getfileicon (LPCWSTR path, bool is_small, size_t* picon_id, HICON* picon)
{
	if (!picon_id && !picon)
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

	CoInitialize (nullptr);

	if (SHGetFileInfo (path, 0, &shfi, sizeof (shfi), flags))
	{
		if (picon_id)
			*picon_id = shfi.iIcon;

		if (picon)
		{
			*picon = CopyIcon (shfi.hIcon);

			DestroyIcon (shfi.hIcon);
		}

		result = true;
	}

	CoUninitialize ();

	return result;
}

size_t _app_geticonid (LPCWSTR path)
{
	size_t result = config.icon_id;

	if (!app.ConfigGet (L"IsIconsHidden", false).AsBool ())
	{
		if (!_app_getfileicon (path, false, &result, nullptr))
			result = config.icon_id;
	}

	return result;
}

size_t _app_getposition (HWND hwnd, size_t hash)
{
	for (size_t i = 0; i < _r_listview_getitemcount (hwnd, IDC_LISTVIEW); i++)
	{
		if ((size_t)_r_listview_getitemlparam (hwnd, IDC_LISTVIEW, i) == hash)
			return i;
	}

	return LAST_VALUE;
}

rstring _app_getshortcutpath (HWND hwnd, LPCWSTR path)
{
	rstring result;

	IShellLink* psl = nullptr;

	CoInitializeEx (nullptr, COINIT_MULTITHREADED);
	CoInitializeSecurity (nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, 0, nullptr);

	if (SUCCEEDED (CoCreateInstance (CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl)))
	{
		IPersistFile* ppf = nullptr;

		if (SUCCEEDED (psl->QueryInterface (IID_IPersistFile, (LPVOID*)&ppf)))
		{
			if (SUCCEEDED (ppf->Load (path, STGM_READ)))
			{
				if (SUCCEEDED (psl->Resolve (hwnd, 0)))
				{
					WIN32_FIND_DATA wfd = {0};
					WCHAR buffer[MAX_PATH] = {0};

					if (SUCCEEDED (psl->GetPath (buffer, _countof (buffer), (WIN32_FIND_DATA*)&wfd, SLGP_RAWPATH)))
					{
						result = buffer;
					}
				}
			}

			ppf->Release ();
		}

		psl->Release ();
	}

	CoUninitialize ();

	return result;
}

bool _app_verifysignature (size_t hash, LPCWSTR path, LPCWSTR* psigner)
{
	if (!psigner)
		return false;

	if (cache_signatures.find (hash) != cache_signatures.end ())
	{
		*psigner = cache_signatures[hash];

		return (cache_signatures[hash] != nullptr);
	}

	bool result = false;

	DWORD cert_encoding = 0;
	HCERTSTORE hcertstore = nullptr;
	HCRYPTMSG hmessage = nullptr;

	cache_signatures[hash] = nullptr;

	if (CryptQueryObject (CERT_QUERY_OBJECT_FILE, path, CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED, CERT_QUERY_FORMAT_FLAG_BINARY, 0, &cert_encoding, nullptr, nullptr, &hcertstore, &hmessage, nullptr))
	{
		DWORD info_size = 0;

		if (CryptMsgGetParam (hmessage, CMSG_SIGNER_INFO_PARAM, 0, nullptr, &info_size))
		{
			PCMSG_SIGNER_INFO signer_info = (PCMSG_SIGNER_INFO)LocalAlloc (LPTR, info_size);

			if (signer_info)
			{
				if (CryptMsgGetParam (hmessage, CMSG_SIGNER_INFO_PARAM, 0, (PVOID)signer_info, &info_size))
				{
					CERT_INFO certinfo = {0};

					certinfo.Issuer = signer_info->Issuer;
					certinfo.SerialNumber = signer_info->SerialNumber;

					PCCERT_CONTEXT cert_context = CertFindCertificateInStore (hcertstore, cert_encoding, 0, CERT_FIND_SUBJECT_CERT, (PVOID)&certinfo, nullptr);

					if (cert_context)
					{
						LPSTR usages[] = {szOID_PKIX_KP_SERVER_AUTH};

						CERT_ENHKEY_USAGE enhkey_usage = {0};
						enhkey_usage.rgpszUsageIdentifier = usages;
						enhkey_usage.cUsageIdentifier = _countof (usages);

						CERT_USAGE_MATCH usage = {0};
						usage.dwType = USAGE_MATCH_TYPE_AND;
						usage.Usage = enhkey_usage;

						CERT_CHAIN_PARA params = {0};
						params.cbSize = sizeof (params);
						params.RequestedUsage = usage;

						PCCERT_CHAIN_CONTEXT chain_context = nullptr;

						if (CertGetCertificateChain (nullptr, cert_context, nullptr, nullptr, &params, CERT_CHAIN_DISABLE_PASS1_QUALITY_FILTERING | CERT_CHAIN_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT, nullptr, &chain_context))
						{
							AUTHENTICODE_EXTRA_CERT_CHAIN_POLICY_PARA extra_policy = {0};
							extra_policy.cbSize = sizeof (extra_policy);
							extra_policy.dwRegPolicySettings = 0;

							if (signer_info)
								extra_policy.pSignerInfo = signer_info;

							CERT_CHAIN_POLICY_PARA policy = {0};
							policy.cbSize = sizeof (policy);
							policy.pvExtraPolicyPara = &extra_policy;

							CERT_CHAIN_POLICY_STATUS status = {0};
							status.cbSize = sizeof (status);

							if (CertVerifyCertificateChainPolicy (CERT_CHAIN_POLICY_AUTHENTICODE, chain_context, &policy, &status))
							{
								if (
									status.dwError != CERT_E_UNTRUSTEDROOT &&
									status.dwError != CERT_E_EXPIRED &&
									status.dwError != CRYPT_E_REVOKED
									)
								{
									// get signature information
									{
										const DWORD num_chars = CertGetNameString (cert_context, CERT_NAME_ATTR_TYPE, 0, szOID_COMMON_NAME, nullptr, 0);

										if (num_chars > 1)
										{
											LPWSTR ppointer = (LPWSTR)malloc ((num_chars + 1) * sizeof (WCHAR));

											if (ppointer)
												CertGetNameString (cert_context, CERT_NAME_ATTR_TYPE, 0, szOID_COMMON_NAME, ppointer, num_chars);

											*psigner = ppointer;
											cache_signatures[hash] = ppointer;
										}
									}

									result = true;
								}
							}

							CertFreeCertificateChain (chain_context);
						}

						CertFreeCertificateContext (cert_context);
					}
				}

				LocalFree (signer_info);
			}
		}

		CryptMsgClose (hmessage);
		CertCloseStore (hcertstore, CERT_CLOSE_STORE_FORCE_FLAG);
	}

	return result;
}

HBITMAP _app_ico2bmp (HICON hico)
{
	const INT icon_size = GetSystemMetrics (SM_CXSMICON);

	RECT rc = {0};
	rc.right = icon_size;
	rc.bottom = icon_size;

	HDC hdc = GetDC (nullptr);
	HDC hmemdc = CreateCompatibleDC (hdc);
	HBITMAP hbitmap = CreateCompatibleBitmap (hdc, icon_size, icon_size);
	ReleaseDC (nullptr, hdc);

	HGDIOBJ old_bmp = SelectObject (hmemdc, hbitmap);
	_r_dc_fillrect (hmemdc, &rc, GetSysColor (COLOR_MENU));
	DrawIconEx (hmemdc, 0, 0, hico, icon_size, icon_size, 0, nullptr, DI_NORMAL);
	SelectObject (hmemdc, old_bmp);

	DeleteDC (hmemdc);

	return hbitmap;
}

bool _app_package_get (size_t hash, rstring* display_name, rstring* real_path)
{
	for (size_t i = 0; i < packages.size (); i++)
	{
		if (packages.at (i).hash == hash)
		{
			if (display_name && packages.at (i).display_name[0])
				*display_name = packages.at (i).display_name;

			if (real_path && packages.at (i).real_path[0])
				*real_path = packages.at (i).real_path;

			return true;
		}
	}

	return false;
}

void _app_package_generate ()
{
	UINT32 numAppContainers = 0;
	INET_FIREWALL_APP_CONTAINER* pAppContainers = 0;

	const HMODULE hlib = LoadLibrary (L"firewallapi.dll");

	if (hlib)
	{
		const NIEAC _NetworkIsolationEnumAppContainers = (NIEAC)GetProcAddress (hlib, "NetworkIsolationEnumAppContainers");

		if (_NetworkIsolationEnumAppContainers)
		{
			const DWORD status = _NetworkIsolationEnumAppContainers (NETISO_FLAG_FORCE_COMPUTE_BINARIES, (LPDWORD)&numAppContainers, &pAppContainers);

			if (status == ERROR_SUCCESS)
			{
				packages.clear ();

				std::unordered_map<size_t, bool> checker;

				for (UINT32 containerIndex = 0; containerIndex < numAppContainers; containerIndex++)
				{
					LPWSTR appsid = nullptr;

					if (pAppContainers[containerIndex].appContainerSid && ConvertSidToStringSid (pAppContainers[containerIndex].appContainerSid, &appsid))
					{
						const size_t hash = _r_str_hash (appsid);

						if (checker.find (hash) == checker.end ())
						{
							checker[hash] = true;

							ITEM_PACKAGE item = {0};

							item.hash = hash;
							StringCchCopy (item.sid, _countof (item.sid), appsid);

							// get package name
							{
								SHLoadIndirectString (pAppContainers[containerIndex].displayName, item.display_name, _countof (item.display_name), nullptr);

								if (!item.display_name[0])
									StringCchCopy (item.display_name, _countof (item.display_name), pAppContainers[containerIndex].appContainerName);
							}

							// get package path
							if (pAppContainers[containerIndex].binaries.count)
							{
								if (pAppContainers[containerIndex].binaries.binaries[0][0] == L'\\')
									StringCchCopy (item.real_path, _countof (item.real_path), pAppContainers[containerIndex].binaries.binaries[0] + 4);

								else
									StringCchCopy (item.real_path, _countof (item.real_path), pAppContainers[containerIndex].binaries.binaries[0]);
							}

							// get file icon
							item.hbmp = config.hbitmap_package_small;

							packages.push_back (item);
						}

						if (appsid)
							LocalFree (appsid);
					}
				}
			}
			else
			{
				_app_logerror (L"NetworkIsolationEnumAppContainers", status, nullptr, true);
			}
		}

		FreeLibrary (hlib);
	}
}

size_t _app_addapplication (HWND hwnd, rstring path, __time64_t timestamp, bool is_silent, bool is_checked, bool is_fromdb)
{
	if (path.IsEmpty ())
		return 0;

	// if file is shortcut - get location
	if (!is_fromdb)
	{
		if (_wcsnicmp (PathFindExtension (path), L".lnk", 4) == 0)
			path = _app_getshortcutpath (hwnd, path);
	}

	const size_t hash = path.Hash ();

	if (apps.find (hash) != apps.end ())
		return 0; // already exists

	ITEM_APPLICATION *ptr_app = &apps[hash]; // application pointer;

	const bool is_ntoskrnl = (hash == config.ntoskrnl_hash);

	rstring real_path;
	rstring display_name;

	if (is_ntoskrnl) // "system" process
	{
		real_path = _r_path_expand (PATH_NTOSKRNL);
		display_name = path;
	}
	else if (_wcsnicmp (path, L"S-1-", 4) == 0) // windows store (win8 and above)
	{
		ptr_app->is_storeapp = true;
		_app_package_get (hash, &display_name, &real_path);
	}
	else
	{
		real_path = path;

		if (app.ConfigGet (L"ShowFilenames", true).AsBool ())
			display_name = _r_path_extractfile (path);
		else
			display_name = path;
	}

	StringCchCopy (ptr_app->original_path, _countof (ptr_app->original_path), path);
	StringCchCopy (ptr_app->real_path, _countof (ptr_app->real_path), real_path);
	StringCchCopy (ptr_app->display_name, _countof (ptr_app->display_name), display_name);

	const DWORD dwAttr = GetFileAttributes (ptr_app->real_path);

	ptr_app->is_enabled = is_checked;
	ptr_app->is_silent = is_silent;

	if (!ptr_app->is_storeapp)
	{
		ptr_app->is_system = is_ntoskrnl || (((dwAttr != INVALID_FILE_ATTRIBUTES && dwAttr & FILE_ATTRIBUTE_SYSTEM) != 0)) || (_wcsnicmp (ptr_app->real_path, config.windows_dir, config.wd_length) == 0);
		ptr_app->is_picoapp = (wcsstr (ptr_app->real_path, L"\\") == nullptr);
	}

	ptr_app->timestamp = timestamp ? timestamp : _r_unixtime_now ();

	if (!ptr_app->is_storeapp && !ptr_app->is_picoapp)
		ptr_app->is_network = PathIsNetworkPath (ptr_app->real_path) ? true : false;

	if (!ptr_app->is_picoapp && !ptr_app->is_network && ptr_app->real_path[0] != L'\\')
	{
		ptr_app->is_signed = _app_verifysignature (hash, ptr_app->real_path, &ptr_app->signer);

		if (ptr_app->is_storeapp)
			ptr_app->icon_id = config.icon_package_id;

		else
			ptr_app->icon_id = _app_geticonid (ptr_app->real_path);
	}
	else
	{
		ptr_app->icon_id = config.icon_id;
	}

	const size_t item = _r_listview_getitemcount (hwnd, IDC_LISTVIEW);

	config.is_nocheckboxnotify = true;

	_r_listview_additem (hwnd, IDC_LISTVIEW, item, 0, ptr_app->display_name, ptr_app->icon_id, ptr_app->is_enabled ? 0 : 1, hash);
	_r_listview_setitem (hwnd, IDC_LISTVIEW, item, 1, _r_fmt_date (ptr_app->timestamp, FDTF_SHORTDATE | FDTF_SHORTTIME));

	_r_listview_setitemcheck (hwnd, IDC_LISTVIEW, item, is_checked);

	config.is_nocheckboxnotify = false;

	return hash;
}

ITEM_APPLICATION* _app_getapplication (size_t hash)
{
	ITEM_APPLICATION *ptr_app = nullptr;

	if (hash && apps.find (hash) != apps.end ())
		ptr_app = &apps.at (hash);

	return ptr_app;
}

void _app_freenotify (size_t idx, size_t hash)
{
	const size_t count = notifications.size ();

	if (!count)
		return;

	std::vector<size_t> idx_array;

	bool is_idxadded = false;

	if (hash)
	{
		for (size_t i = 0; i < notifications.size (); i++)
		{
			ITEM_LOG const* ptr_log = notifications.at (i);

			if (!ptr_log || (ptr_log && ptr_log->hash == hash))
			{
				idx_array.push_back (i);

				if (idx != LAST_VALUE && idx == i)
					is_idxadded = true;
			}
		}
	}

	if (idx != LAST_VALUE && !is_idxadded)
		idx_array.push_back (idx);

	if (idx_array.empty ())
		return;

	for (size_t i = (idx_array.size () - 1); i != LAST_VALUE; i--)
	{
		const size_t vc_idx = idx_array.at (i);

		ITEM_LOG* ptr_log = notifications.at (vc_idx);

		if (ptr_log)
		{
			if (ptr_log->hicon)
			{
				DestroyIcon (ptr_log->hicon);
				ptr_log->hicon = nullptr;
			}

			free (ptr_log);
			ptr_log = nullptr;
		}

		notifications.erase (notifications.begin () + vc_idx);
	}
}

bool _app_freeapplication (size_t hash)
{
	bool is_enabled = false;

	ITEM_APPLICATION* ptr_app = _app_getapplication (hash);

	if (ptr_app)
		is_enabled = ptr_app->is_enabled;

	if (hash)
	{
		if (cache_signatures.find (hash) != cache_signatures.end ())
		{
			free (cache_signatures[hash]);
			cache_signatures.erase (hash);
		}

		if (cache_versions.find (hash) != cache_versions.end ())
		{
			free (cache_versions[hash]);
			cache_versions.erase (hash);
		}

		if (!is_enabled && ptr_app && !ptr_app->rules.empty ())
			is_enabled = true;

		_app_freenotify (LAST_VALUE, hash);
		_app_notifyrefresh ();

		apps.erase (hash);
	}

	return is_enabled;
}

void _app_freerule (ITEM_RULE** ptr)
{
	if (ptr && *ptr)
	{
		ITEM_RULE *ptr_rule = *ptr;

		if (ptr_rule->pname)
		{
			free (ptr_rule->pname);
			ptr_rule->pname = nullptr;
		}

		if (ptr_rule->prule)
		{
			free (ptr_rule->prule);
			ptr_rule->prule = nullptr;
		}

		if (ptr_rule->papps)
		{
			free (ptr_rule->papps);
			ptr_rule->papps = nullptr;
		}

		free (ptr_rule);

		ptr_rule = nullptr;
		*ptr = nullptr;
	}
}

bool _app_checkrules (std::vector<size_t> const* ptr)
{
	if (!ptr || ptr->empty ())
		return false;

	for (size_t i = 0; i < ptr->size (); i++)
	{
		ITEM_RULE *ptr_rule = rules_custom.at (ptr->at (i));

		if (ptr_rule)
		{
			if (ptr_rule->is_enabled)
				return true;
		}
	}

	return false;
}

DWORD_PTR _app_getcolorvalue (size_t hash, bool is_brush)
{
	size_t idx = LAST_VALUE;

	for (size_t i = 0; i < colors.size (); i++)
	{
		if (colors.at (i).hash == hash)
		{
			idx = i;
			break;
		}
	}

	if (idx != LAST_VALUE)
	{
		if (is_brush)
			return (DWORD_PTR)colors.at (idx).hbr;

		else
			return colors.at (idx).clr;
	}

	return 0;
}

DWORD_PTR _app_getcolor (size_t hash, bool is_brush)
{
	_r_fastlock_acquireshared (&lock_access);

	rstring color_value;
	ITEM_APPLICATION const* ptr_app = _app_getapplication (hash);

	if (ptr_app)
	{
		if (app.ConfigGet (L"IsHighlightInvalid", true).AsBool () && ((ptr_app->is_enabled && ptr_app->error_count) || (ptr_app->is_storeapp && !_app_package_get (hash, nullptr, nullptr)) || (!ptr_app->is_storeapp && !ptr_app->is_picoapp && !ptr_app->is_network && ptr_app->real_path[0] != L'\\' && !_r_fs_exists (ptr_app->real_path))))
			color_value = L"ColorInvalid";

		else if (app.ConfigGet (L"IsHighlightSpecial", true).AsBool () && _app_checkrules (&ptr_app->rules))
			color_value = L"ColorSpecial";

		else if (ptr_app->is_silent && app.ConfigGet (L"IsHighlightSilent", true).AsBool ())
			color_value = L"ColorSilent";

		else if (ptr_app->is_storeapp && app.ConfigGet (L"IsHighlightPackage", true).AsBool ())
			color_value = L"ColorPackage";

		else if (ptr_app->is_signed && app.ConfigGet (L"IsHighlightSigned", true).AsBool ())
			color_value = L"ColorSigned";

		else if (ptr_app->is_picoapp && app.ConfigGet (L"IsHighlightPico", true).AsBool ())
			color_value = L"ColorPico";

		else if (ptr_app->is_network && app.ConfigGet (L"IsHighlightNetwork", true).AsBool ())
			color_value = L"ColorNetwork";

		else if (ptr_app->is_system && app.ConfigGet (L"IsHighlightSystem", true).AsBool ())
			color_value = L"ColorSystem";
	}

	_r_fastlock_releaseshared (&lock_access);

	return _app_getcolorvalue (color_value.Hash (), is_brush);
}

rstring _app_gettooltip (size_t hash)
{
	rstring result;

	_r_fastlock_acquireshared (&lock_access);

	ITEM_APPLICATION* ptr_app = _app_getapplication (hash);

	if (ptr_app)
	{
		result = ptr_app->real_path;

		// file information
		if (!ptr_app->is_storeapp && !ptr_app->is_network && !ptr_app->is_picoapp && ptr_app->real_path[0] != L'\\')
		{
			rstring buffer;

			_app_getversion (hash, ptr_app->real_path, &ptr_app->description);
			buffer = ptr_app->description;

			if (!buffer.IsEmpty ())
			{
				buffer.InsertFormat (0, L"\r\n%s:\r\n" TAB_SPACE, I18N (&app, IDS_FILE, 0));
				result.Append (buffer);
			}
		}

		// signature
		if (ptr_app->is_signed && ptr_app->signer)
			result.AppendFormat (L"\r\n%s:\r\n" TAB_SPACE L"%s", I18N (&app, IDS_SIGNATURE, 0), ptr_app->signer);

		// notes
		{
			rstring buffer;

			if ((ptr_app->is_enabled && ptr_app->error_count) || (ptr_app->is_storeapp && !_app_package_get (hash, nullptr, nullptr)) || ((!ptr_app->is_storeapp && !ptr_app->is_picoapp && !ptr_app->is_network && ptr_app->real_path[0] != L'\\' && !_r_fs_exists (ptr_app->real_path))))
				buffer.AppendFormat (TAB_SPACE L"%s\r\n", I18N (&app, IDS_HIGHLIGHT_INVALID, 0));

			if (ptr_app->is_network)
				buffer.AppendFormat (TAB_SPACE L"%s\r\n", I18N (&app, IDS_HIGHLIGHT_NETWORK, 0));

			if (ptr_app->is_picoapp)
				buffer.AppendFormat (TAB_SPACE L"%s\r\n", I18N (&app, IDS_HIGHLIGHT_PICO, 0));

			if (ptr_app->is_signed)
				buffer.AppendFormat (TAB_SPACE L"%s\r\n", I18N (&app, IDS_HIGHLIGHT_SIGNED, 0));

			if (ptr_app->is_silent)
				buffer.AppendFormat (TAB_SPACE L"%s\r\n", I18N (&app, IDS_HIGHLIGHT_SILENT, 0));

			if (_app_checkrules (&ptr_app->rules))
				buffer.AppendFormat (TAB_SPACE L"%s\r\n", I18N (&app, IDS_HIGHLIGHT_SPECIAL, 0));

			if (ptr_app->is_system)
				buffer.AppendFormat (TAB_SPACE L"%s\r\n", I18N (&app, IDS_HIGHLIGHT_SYSTEM, 0));

			if (ptr_app->is_storeapp)
				buffer.AppendFormat (TAB_SPACE L"%s\r\n", I18N (&app, IDS_HIGHLIGHT_PACKAGE, 0));

			if (!buffer.IsEmpty ())
			{
				buffer.InsertFormat (0, L"\r\n%s:\r\n", I18N (&app, IDS_NOTES, 0));
				result.Append (buffer);
			}
		}
	}

	_r_fastlock_releaseshared (&lock_access);

	return result;
}

UINT _wfp_destroyfilters (bool is_full)
{
	UINT error_count = 0;

	// dropped packets logging (win7 and above)
	if (_r_sys_validversion (6, 1))
		_wfp_logunsubscribe ();

	_r_fastlock_acquireexclusive (&lock_access);

	for (auto &p : apps)
		p.second.error_count = false;

	for (size_t i = 0; i < rules_blocklist.size (); i++)
	{
		ITEM_RULE *ptr_rule = rules_blocklist.at (i);

		if (ptr_rule)
			ptr_rule->error_count = false;
	}

	for (size_t i = 0; i < rules_system.size (); i++)
	{
		ITEM_RULE *ptr_rule = rules_system.at (i);

		if (ptr_rule)
			ptr_rule->error_count = false;
	}

	for (size_t i = 0; i < rules_custom.size (); i++)
	{
		ITEM_RULE *ptr_rule = rules_custom.at (i);

		if (ptr_rule)
			ptr_rule->error_count = false;
	}

	_r_fastlock_releaseexclusive (&lock_access);

	DWORD result = FwpmTransactionBegin (config.hengine, 0);

	if (result != ERROR_SUCCESS)
	{
		_app_logerror (L"FwpmTransactionBegin", result, nullptr);
		++error_count;
	}
	else
	{
		HANDLE henum = nullptr;

		result = FwpmFilterCreateEnumHandle (config.hengine, nullptr, &henum);

		if (result != ERROR_SUCCESS)
		{
			_app_logerror (L"FwpmFilterCreateEnumHandle", result, nullptr);
			++error_count;
		}
		else
		{
			UINT32 count = 0;
			FWPM_FILTER** matchingFwpFilter = nullptr;

			result = FwpmFilterEnum (config.hengine, henum, 0xFFFFFFFF, &matchingFwpFilter, &count);

			if (result != ERROR_SUCCESS)
			{
				_app_logerror (L"FwpmFilterEnum", result, nullptr);
				++error_count;
			}
			else
			{
				if (matchingFwpFilter)
				{
					for (UINT32 i = 0; i < count; i++)
					{
						if (matchingFwpFilter[i]->providerKey && memcmp (matchingFwpFilter[i]->providerKey, &GUID_WfpProvider, sizeof (GUID)) == 0)
						{
							result = FwpmFilterDeleteById (config.hengine, matchingFwpFilter[i]->filterId);

							if (result != ERROR_SUCCESS)
							{
								_app_logerror (L"FwpmFilterDeleteById", result, nullptr);
								++error_count;
							}
						}

						if (WaitForSingleObjectEx (config.stop_evt, 0, FALSE) == WAIT_OBJECT_0)
						{
							FwpmFreeMemory ((LPVOID*)&matchingFwpFilter);
							FwpmTransactionAbort (config.hengine);

							return 0;
						}
					}

					FwpmFreeMemory ((LPVOID*)&matchingFwpFilter);
				}
			}
		}

		if (henum)
			FwpmFilterDestroyEnumHandle (config.hengine, henum);

		FwpmTransactionCommit (config.hengine);
	}

	if (is_full)
	{
		// set icons
		app.SetIcon (IDI_INACTIVE);
		app.TraySetInfo (UID, _r_loadicon (app.GetHINSTANCE (), MAKEINTRESOURCE (IDI_INACTIVE), GetSystemMetrics (SM_CXSMICON)), nullptr);

		SetDlgItemText (app.GetHWND (), IDC_START_BTN, I18N (&app, IDS_TRAY_START, 0));
	}

	return error_count;
}

DWORD _wfp_createfilter (LPCWSTR name, FWPM_FILTER_CONDITION* lpcond, UINT32 const count, UINT8 weight, GUID layer, const GUID* callout, BOOL is_block, bool is_boottime)
{
	FWPM_FILTER filter = {0};

	WCHAR fltr_name[128] = {0};
	StringCchCopy (fltr_name, _countof (fltr_name), name ? name : APP_NAME);

	filter.displayData.name = fltr_name;
	filter.displayData.description = fltr_name;

	if (is_boottime)
		filter.flags = FWPM_FILTER_FLAG_BOOTTIME;
	else
		filter.flags = FWPM_FILTER_FLAG_PERSISTENT;

	// filter is indexed to help enable faster lookup during classification (win8 and above)
	if (!is_boottime && _r_sys_validversion (6, 2))
		filter.flags |= FWPM_FILTER_FLAG_INDEXED;

	filter.providerKey = (LPGUID)&GUID_WfpProvider;
	filter.layerKey = layer;
	filter.subLayerKey = GUID_WfpSublayer;

	if (count)
	{
		filter.numFilterConditions = count;
		filter.filterCondition = lpcond;
	}

	if (is_block == FWP_ACTION_CALLOUT_TERMINATING)
		filter.action.type = FWP_ACTION_CALLOUT_TERMINATING;
	else
		filter.action.type = ((is_block) ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT);

	if (callout)
		memcpy_s (&filter.action.calloutKey, sizeof (GUID), callout, sizeof (GUID));

	filter.weight.type = FWP_UINT8;
	filter.weight.uint8 = weight;

	UINT64 filter_id = 0;

	return FwpmFilterAdd (config.hengine, &filter, nullptr, &filter_id);
}

INT CALLBACK _app_listviewcmp_appsrules (LPARAM item1, LPARAM item2, LPARAM lparam)
{
	HWND hwnd = (HWND)lparam;

	const bool is_checked1 = _r_listview_isitemchecked (hwnd, IDC_FILES_LV, (size_t)item1);
	const bool is_checked2 = _r_listview_isitemchecked (hwnd, IDC_FILES_LV, (size_t)item2);

	if (is_checked1 < is_checked2)
	{
		return 1;
	}
	else if (is_checked1 > is_checked2)
	{
		return -1;
	}

	return _r_listview_getitemtext (hwnd, IDC_FILES_LV, (size_t)item1, 0).CompareNoCase (_r_listview_getitemtext (hwnd, IDC_FILES_LV, (size_t)item2, 0));
}

INT CALLBACK _app_listviewcmp_rules (LPARAM lp1, LPARAM lp2, LPARAM)
{
	if (lp1 > lp2)
	{
		return 1;
	}
	else if (lp1 < lp2)
	{
		return -1;
	}

	return 0;
}

INT CALLBACK _app_listviewcompare (LPARAM lp1, LPARAM lp2, LPARAM lparam)
{
	const UINT column_id = LOWORD (lparam);
	const BOOL is_descend = HIWORD (lparam);

	const size_t hash1 = static_cast<size_t>(lp1);
	const size_t hash2 = static_cast<size_t>(lp2);

	INT result = 0;

	ITEM_APPLICATION const *ptr_app1 = _app_getapplication (hash1);

	if (!ptr_app1)
		return 0;

	ITEM_APPLICATION const *ptr_app2 = _app_getapplication (hash2);

	if (!ptr_app1)
		return 0;

	if (ptr_app1->is_enabled && !ptr_app2->is_enabled)
	{
		result = -1;
	}
	else if (!ptr_app1->is_enabled && ptr_app2->is_enabled)
	{
		result = 1;
	}
	else
	{
		if (column_id == 0)
		{
			// file
			result = _wcsicmp (ptr_app1->display_name, ptr_app2->display_name);
		}
		else if (column_id == 1)
		{
			// timestamp
			if (ptr_app1->timestamp == ptr_app2->timestamp)
			{
				result = 0;
			}
			else if (ptr_app1->timestamp < ptr_app2->timestamp)
			{
				result = -1;
			}
			else if (ptr_app1->timestamp > ptr_app2->timestamp)
			{
				result = 1;
			}
		}
	}

	return is_descend ? -result : result;
}

void _app_listviewsort_appsrules (HWND hwnd, UINT ctrl_id)
{
	SendDlgItemMessage (hwnd, ctrl_id, LVM_SORTITEMSEX, (LPARAM)hwnd, (LPARAM)&_app_listviewcmp_appsrules);
}

void _app_listviewsort_rules (HWND hwnd, UINT ctrl_id)
{
	_r_fastlock_acquireshared (&lock_access);

	SendDlgItemMessage (hwnd, ctrl_id, LVM_SORTITEMS, 0, (LPARAM)&_app_listviewcmp_rules);

	_r_fastlock_releaseshared (&lock_access);
}

void _app_listviewsort (HWND hwnd, UINT ctrl_id, INT subitem, bool is_notifycode)
{
	bool is_descend = app.ConfigGet (L"IsSortDescending", true).AsBool ();

	if (is_notifycode)
		is_descend = !is_descend;

	if (subitem == -1)
		subitem = app.ConfigGet (L"SortColumn", 1).AsBool ();

	LPARAM lparam = MAKELPARAM (subitem, is_descend);

	if (is_notifycode)
	{
		app.ConfigSet (L"IsSortDescending", is_descend);
		app.ConfigSet (L"SortColumn", (DWORD)subitem);
	}

	_r_listview_setcolumnsortindex (hwnd, ctrl_id, 0, 0);
	_r_listview_setcolumnsortindex (hwnd, ctrl_id, 1, 0);

	_r_listview_setcolumnsortindex (hwnd, ctrl_id, subitem, is_descend ? -1 : 1);

	_r_fastlock_acquireshared (&lock_access);

	SendDlgItemMessage (hwnd, ctrl_id, LVM_SORTITEMS, lparam, (LPARAM)&_app_listviewcompare);

	_r_fastlock_releaseshared (&lock_access);

	_app_refreshstatus (hwnd, true, true);
}

bool _app_ruleisport (LPCWSTR rule)
{
	if (!rule)
		return false;

	const size_t length = wcslen (rule);

	for (size_t i = 0; i < length; i++)
	{
		if (iswdigit (rule[i]) == 0 && rule[i] != L'-')
			return false;
	}

	return true;
}

bool _app_ruleishost (LPCWSTR rule)
{
	if (!rule)
		return false;

	size_t dotcounter = 0;
	size_t semicoloncounter = 0;
	bool have_nonhex = false;

	for (size_t i = (wcslen (rule) - 1); i != LAST_VALUE; i--)
	{
		const WCHAR ch = _r_str_lower (rule[i]);

		if (ch == L'-')
		{
			continue;
		}
		else if (ch == L'/') // prefix length set
		{
			return false;
		}
		else if (ch == L':' && (++semicoloncounter > 1)) // ipv6
		{
			return false;
		}
		else if (ch == L'.' && (++dotcounter >= 4))
		{
			return false;
		}
		else if ((ch >= L'g' && ch <= L'z'))
		{
			return true;
		}
	}

	if (have_nonhex && dotcounter <= 3)
		return true;

	return false;
}

rstring _app_parsehostaddress (LPCWSTR host, USHORT port)
{
	rstring result;

	PDNS_RECORD ppQueryResultsSet = nullptr;
	PIP4_ARRAY pSrvList = nullptr;

	DWORD options = DNS_QUERY_NO_HOSTS_FILE | DNS_QUERY_NO_NETBT | DNS_QUERY_NO_MULTICAST | DNS_QUERY_NO_LOCAL_NAME;

	// use custom dns-server (if present)
	WCHAR dnsServer[INET_ADDRSTRLEN] = {0};
	StringCchCopy (dnsServer, _countof (dnsServer), app.ConfigGet (L"DnsServerV4", nullptr)); // ipv4 dns-server address

	if (dnsServer[0])
	{
		pSrvList = (PIP4_ARRAY)malloc (sizeof (IP4_ARRAY));

		if (pSrvList)
		{
			if (InetPton (AF_INET, dnsServer, &(pSrvList->AddrArray[0])))
			{
				pSrvList->AddrCount = 1;

				options = DNS_QUERY_WIRE_ONLY;
			}
			else
			{
				free (pSrvList);
				pSrvList = nullptr;
			}
		}
	}

	const DNS_STATUS dnsStatus = DnsQuery (host, DNS_TYPE_ALL, options, pSrvList, &ppQueryResultsSet, nullptr);

	if (dnsStatus != ERROR_SUCCESS)
	{
		_app_logerror (L"DnsQuery", dnsStatus, host, false);
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
					result.Append (_app_parsehostaddress (current->Data.CNAME.pNameHost, port));
			}
		}

		if (pSrvList)
		{
			free (pSrvList);
			pSrvList = nullptr;
		}

		DnsRecordListFree (ppQueryResultsSet, DnsFreeRecordList);
	}

	return result.Trim (RULE_DELIMETER);
}

bool _app_parsenetworkstring (rstring network_string, NET_ADDRESS_FORMAT* format_ptr, USHORT* port_ptr, FWP_V4_ADDR_AND_MASK* paddr4, FWP_V6_ADDR_AND_MASK* paddr6, LPWSTR paddr_dns)
{
	NET_ADDRESS_INFO ni;
	SecureZeroMemory (&ni, sizeof (ni));

	USHORT port = 0;
	BYTE prefix_length = 0;

	const DWORD errcode = ParseNetworkString (network_string, NET_STRING_IP_ADDRESS | NET_STRING_IP_SERVICE | NET_STRING_IP_NETWORK | NET_STRING_IP_ADDRESS_NO_SCOPE, &ni, &port, &prefix_length);

	if (errcode != ERROR_SUCCESS)
	{
		if (errcode == ERROR_INVALID_PARAMETER)
		{
			if (_app_ruleishost (network_string))
			{
				if (app.ConfigGet (L"IsHostsEnabled", true).AsBool ())
				{
					const size_t pos = network_string.Find (L':');

					if (pos != rstring::npos)
						port = (USHORT)network_string.Midded (pos + 1).AsUlong ();

					const rstring host = _app_parsehostaddress (network_string.Midded (0, pos), port);

					if (!host.IsEmpty ())
					{
						if (format_ptr)
							*format_ptr = NET_ADDRESS_DNS_NAME;

						if (port_ptr)
							*port_ptr = port;

						if (paddr_dns)
							StringCchCopy (paddr_dns, LEN_HOST_MAX, host);
					}

					return true;
				}

				return false;
			}
		}

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

				paddr4->mask = ntohl (mask);
				paddr4->addr = ntohl (ni.Ipv4Address.sin_addr.S_un.S_addr);
			}
		}
		else if (ni.Format == NET_ADDRESS_IPV6)
		{
			if (paddr6)
			{
				paddr6->prefixLength = (std::min) (prefix_length, (BYTE)128);
				memcpy_s (paddr6->addr, FWP_V6_ADDR_SIZE, ni.Ipv6Address.sin6_addr.u.Byte, FWP_V6_ADDR_SIZE);
			}
		}
		else
		{
			return false;
		}
	}

	return true;
}

bool _app_parserulestring (rstring rule, ITEM_ADDRESS *ptr_addr, EnumRuleType *ptype)
{
	rule.Trim (L"\r\n "); // trim whitespace

	if (rule.IsEmpty ())
		return false;

	if (rule.At (0) == L'*')
		return true;

	EnumRuleType type = TypeUnknown;
	size_t range_pos = rstring::npos;

	if (ptype)
		type = *ptype;

	// auto-parse rule type
	if (type == TypeUnknown)
	{
		if (_app_ruleisport (rule))
			type = TypePort;

		else
			type = _app_ruleishost (rule) ? TypeHost : TypeIp;
	}

	if (type == TypeUnknown)
		return false;

	if (type == TypePort || type == TypeIp)
	{
		range_pos = rule.Find (L'-');

		if (ptr_addr && range_pos != rstring::npos)
			ptr_addr->is_range = true;
	}

	WCHAR range_start[LEN_IP_MAX] = {0};
	WCHAR range_end[LEN_IP_MAX] = {0};

	if (range_pos != rstring::npos)
	{
		StringCchCopy (range_start, _countof (range_start), rule.Midded (0, range_pos));
		StringCchCopy (range_end, _countof (range_end), rule.Midded (range_pos + 1));
	}

	if (type == TypePort)
	{
		if (range_pos == rstring::npos)
		{
			// ...port
			if (ptr_addr)
			{
				ptr_addr->type = TypePort;
				ptr_addr->port = (UINT16)rule.AsUlong ();
			}

			return true;
		}
		else
		{
			// ...port range
			if (ptr_addr)
			{
				ptr_addr->type = TypePort;

				if (ptr_addr->prange)
				{
					ptr_addr->prange->valueLow.type = FWP_UINT16;
					ptr_addr->prange->valueLow.uint16 = (UINT16)wcstoul (range_start, nullptr, 10);

					ptr_addr->prange->valueHigh.type = FWP_UINT16;
					ptr_addr->prange->valueHigh.uint16 = (UINT16)wcstoul (range_end, nullptr, 10);
				}
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

		if (range_pos == rstring::npos)
		{
			// ...ip/host
			if (_app_parsenetworkstring (rule, &format, &port2, &addr4, &addr6, ptr_addr ? ptr_addr->host : nullptr))
			{
				if (format == NET_ADDRESS_IPV4)
				{
					if (ptr_addr && ptr_addr->paddr4)
					{
						ptr_addr->paddr4->mask = addr4.mask;
						ptr_addr->paddr4->addr = addr4.addr;
					}
				}
				else if (format == NET_ADDRESS_IPV6)
				{
					if (ptr_addr && ptr_addr->paddr6)
					{
						ptr_addr->paddr6->prefixLength = addr6.prefixLength;
						memcpy_s (ptr_addr->paddr6->addr, FWP_V6_ADDR_SIZE, addr6.addr, FWP_V6_ADDR_SIZE);
					}
				}
				else if (format == NET_ADDRESS_DNS_NAME)
				{
					if (ptr_addr)
					{
						ptr_addr->type = TypeHost;
						//ptr_addr->host = <hosts>;
					}
				}
				else
				{
					return false;
				}

				if (ptr_addr)
				{
					ptr_addr->format = format;
					ptr_addr->type = TypeIp;

					if (port2)
						ptr_addr->port = port2;
				}

				return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			// ...ip range (start)
			if (_app_parsenetworkstring (range_start, &format, &port2, &addr4, &addr6, nullptr))
			{
				if (format == NET_ADDRESS_IPV4)
				{
					if (ptr_addr && ptr_addr->prange)
					{
						ptr_addr->prange->valueLow.type = FWP_UINT32;
						ptr_addr->prange->valueLow.uint32 = addr4.addr;
					}
				}
				else if (format == NET_ADDRESS_IPV6)
				{
					if (ptr_addr && ptr_addr->prange)
					{
						ptr_addr->prange->valueLow.type = FWP_BYTE_ARRAY16_TYPE;
						memcpy_s (ptr_addr->prange->valueLow.byteArray16->byteArray16, FWP_V6_ADDR_SIZE, addr6.addr, FWP_V6_ADDR_SIZE);
					}
				}
				else
				{
					return false;
				}

				if (port2 && ptr_addr && !ptr_addr->port)
					ptr_addr->port = port2;
			}
			else
			{
				return false;
			}

			// ...ip range (end)
			if (_app_parsenetworkstring (range_end, &format, &port2, &addr4, &addr6, nullptr))
			{
				if (format == NET_ADDRESS_IPV4)
				{
					if (ptr_addr && ptr_addr->prange)
					{
						ptr_addr->prange->valueHigh.type = FWP_UINT32;
						ptr_addr->prange->valueHigh.uint32 = addr4.addr;
					}
				}
				else if (format == NET_ADDRESS_IPV6)
				{
					if (ptr_addr && ptr_addr->prange)
					{
						ptr_addr->prange->valueHigh.type = FWP_BYTE_ARRAY16_TYPE;
						memcpy_s (ptr_addr->prange->valueHigh.byteArray16->byteArray16, FWP_V6_ADDR_SIZE, addr6.addr, FWP_V6_ADDR_SIZE);
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

			if (ptr_addr)
			{
				ptr_addr->format = format;
				ptr_addr->type = TypeIp;
			}
		}
	}

	return true;
}

DWORD _FwpmGetAppIdFromFileName1 (LPCWSTR path, FWP_BYTE_BLOB** ptr)
{
	if (!path || !ptr)
		return ERROR_BAD_ARGUMENTS;

	rstring path_buff = path;

	DWORD result = ERROR_SUCCESS;

	// check for filename-only and device paths (do not touch them)
	if (path_buff.Find (L'\\', 0) != rstring::npos && path_buff.At (0) != L'\\')
	{
		result = _r_path_ntpathfromdos (path_buff);

		// file is inaccessible or not found, maybe low-level driver preventing file access?
		// try another way!
		if (result == ERROR_ACCESS_DENIED || result == ERROR_FILE_NOT_FOUND || result == ERROR_PATH_NOT_FOUND)
		{
			// file path (root)
			WCHAR path_root[16] = {0};
			StringCchCopy (path_root, _countof (path_root), path);
			PathStripToRoot (path_root);

			// file path (without root)
			WCHAR path_noroot[MAX_PATH] = {0};
			StringCchCopy (path_noroot, _countof (path_noroot), PathSkipRoot (path));

			path_buff = path_root;

			result = _r_path_ntpathfromdos (path_buff);

			if (result != ERROR_SUCCESS)
				return result;

			path_buff.Append (path_noroot);
			path_buff.ToLower (); // lower is important!
		}
		else if (result != ERROR_SUCCESS)
		{
			return result;
		}
		else
		{
			// successfully!
		}
	}

	// allocate buffer
	if (result == ERROR_SUCCESS)
	{
		*ptr = (FWP_BYTE_BLOB*)malloc (sizeof (FWP_BYTE_BLOB));

		if (!*ptr)
		{
			return ERROR_OUTOFMEMORY;
		}
		else
		{
			SecureZeroMemory (*ptr, sizeof (FWP_BYTE_BLOB));

			const size_t length = (path_buff.GetLength () + 1) * sizeof (WCHAR);
			const DWORD_PTR tmp_ptr = (DWORD_PTR)malloc (length);

			if (!tmp_ptr)
			{
				free (*ptr);

				return ERROR_OUTOFMEMORY;
			}
			else
			{
				SecureZeroMemory ((UINT8*)tmp_ptr, length);

				(*ptr)->data = (UINT8*)tmp_ptr;
				*((*ptr)->data) = (UINT8)tmp_ptr;

				(*ptr)->size = (UINT32)length;

				memcpy_s ((*ptr)->data, length, path_buff.GetString (), length);
			}
		}
	}

	return result;
}

void _FwpmFreeAppIdFromFileName1 (FWP_BYTE_BLOB** ptr)
{
	if (ptr && *ptr)
	{
		FWP_BYTE_BLOB* blob = *ptr;

		if (blob->data)
		{
			free (blob->data);
			blob->data = nullptr;
		}

		free (blob);
		blob = nullptr;
		*ptr = nullptr;
	}
}

bool _wfp_createrulefilter (LPCWSTR name, LPCWSTR rule, LPCWSTR path, EnumRuleDirection dir, EnumRuleType* ptype, UINT8 protocol, ADDRESS_FAMILY af, BOOL is_block, UINT8 weight, bool is_boottime)
{
	UINT32 count = 0;
	FWPM_FILTER_CONDITION fwfc[6] = {0};

	// rule without address
	if (rule && rule[0] == L'*')
		rule = nullptr;

	FWP_BYTE_BLOB* blob = nullptr;
	SID* psid = nullptr;

	FWP_V4_ADDR_AND_MASK addr4 = {0};
	FWP_V6_ADDR_AND_MASK addr6 = {0};

	FWP_RANGE range;
	SecureZeroMemory (&range, sizeof (range));

	ITEM_ADDRESS addr;
	SecureZeroMemory (&addr, sizeof (addr));

	addr.paddr4 = &addr4;
	addr.paddr6 = &addr6;
	addr.prange = &range;

	UINT32 ip_idx = UINT32 (-1);
	UINT32 port_idx = UINT32 (-1);

	if (path)
	{
		// windows store app (win8 and above)
		if (_wcsnicmp (path, L"S-1-", 4) == 0)
		{
			if (_r_sys_validversion (6, 2) && ConvertStringSidToSid (path, (PSID*)&psid))
			{
				fwfc[count].fieldKey = FWPM_CONDITION_ALE_PACKAGE_ID;
				fwfc[count].matchType = FWP_MATCH_EQUAL;
				fwfc[count].conditionValue.type = FWP_SID;
				fwfc[count].conditionValue.sid = psid;

				count += 1;
			}
			else
			{
				_app_logerror (L"ConvertStringSidToSid", GetLastError (), path, true);
				return false;
			}
		}
		else
		{
			const DWORD rc = _FwpmGetAppIdFromFileName1 (path, &blob);

			if (rc != ERROR_SUCCESS)
			{
				_FwpmFreeAppIdFromFileName1 (&blob);
				_app_logerror (L"FwpmGetAppIdFromFileName", rc, path, true);

				return false;
			}
			else
			{
				fwfc[count].fieldKey = FWPM_CONDITION_ALE_APP_ID;
				fwfc[count].matchType = FWP_MATCH_EQUAL;
				fwfc[count].conditionValue.type = FWP_BYTE_BLOB_TYPE;
				fwfc[count].conditionValue.byteBlob = blob;

				count += 1;
			}
		}
	}

	if (protocol)
	{
		fwfc[count].fieldKey = FWPM_CONDITION_IP_PROTOCOL;
		fwfc[count].matchType = FWP_MATCH_EQUAL;
		fwfc[count].conditionValue.type = FWP_UINT8;
		fwfc[count].conditionValue.uint8 = protocol;

		count += 1;
	}

	if (rule)
	{
		if (_app_parserulestring (rule, &addr, ptype))
		{
			if (addr.is_range && (addr.type == TypeIp || addr.type == TypePort))
			{
				if (addr.type == TypeIp)
				{
					if (addr.format == NET_ADDRESS_IPV4)
						af = AF_INET;

					else if (addr.format == NET_ADDRESS_IPV6)
						af = AF_INET6;

					else
						return false;
				}

				fwfc[count].matchType = FWP_MATCH_RANGE;
				fwfc[count].conditionValue.type = FWP_RANGE_TYPE;
				fwfc[count].conditionValue.rangeValue = &range;

				if (addr.type == TypePort)
					port_idx = count;

				else
					ip_idx = count;

				count += 1;
			}
			else if (addr.type == TypePort)
			{
				fwfc[count].matchType = FWP_MATCH_EQUAL;
				fwfc[count].conditionValue.type = FWP_UINT16;
				fwfc[count].conditionValue.uint16 = addr.port;

				port_idx = count;
				count += 1;
			}
			else if (addr.type == TypeHost || addr.type == TypeIp)
			{
				if (addr.format == NET_ADDRESS_IPV4)
				{
					af = AF_INET;

					fwfc[count].matchType = FWP_MATCH_EQUAL;
					fwfc[count].conditionValue.type = FWP_V4_ADDR_MASK;
					fwfc[count].conditionValue.v4AddrMask = &addr4;

					ip_idx = count;
					count += 1;
				}
				else if (addr.format == NET_ADDRESS_IPV6)
				{
					af = AF_INET6;

					fwfc[count].matchType = FWP_MATCH_EQUAL;
					fwfc[count].conditionValue.type = FWP_V6_ADDR_MASK;
					fwfc[count].conditionValue.v6AddrMask = &addr6;

					ip_idx = count;
					count += 1;
				}
				else if (addr.format == NET_ADDRESS_DNS_NAME)
				{
					_FwpmFreeAppIdFromFileName1 (&blob);
					LocalFree (psid);

					if (!app.ConfigGet (L"IsHostsEnabled", true).AsBool ())
						return true;

					rstring::rvector arr = rstring (addr.host).AsVector (RULE_DELIMETER);

					if (arr.empty ())
					{
						return false;
					}
					else
					{
						for (size_t i = 0; i < arr.size (); i++)
						{
							EnumRuleType type = TypeIp;

							if (!_wfp_createrulefilter (name, arr.at (i), path, dir, &type, protocol, af, is_block, weight, is_boottime))
								return false;
						}
					}

					return true;
				}
				else
				{
					_FwpmFreeAppIdFromFileName1 (&blob);
					LocalFree (psid);

					return false;
				}

				// set port if available
				if (addr.port)
				{
					fwfc[count].matchType = FWP_MATCH_EQUAL;
					fwfc[count].conditionValue.type = FWP_UINT16;
					fwfc[count].conditionValue.uint16 = addr.port;

					port_idx = count;
					count += 1;
				}
			}
			else
			{
				_FwpmFreeAppIdFromFileName1 (&blob);
				LocalFree (psid);

				return false;
			}
		}
		else
		{
			_FwpmFreeAppIdFromFileName1 (&blob);
			LocalFree (psid);

			return false;
		}
	}

	// create filters
	DWORD result = 0;

	if (dir == DirOutbound || dir == DirBoth)
	{
		if (ip_idx != UINT32 (-1))
			fwfc[ip_idx].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;

		if (port_idx != UINT32 (-1))
			fwfc[port_idx].fieldKey = FWPM_CONDITION_IP_REMOTE_PORT;

		if (af == AF_INET || af == AF_UNSPEC)
		{
			result = _wfp_createfilter (name, fwfc, count, weight, FWPM_LAYER_ALE_AUTH_CONNECT_V4, nullptr, is_block, is_boottime);

			if (result != ERROR_SUCCESS)
				_app_logerror (L"FwpmFilterAdd", result, rule, true);
		}

		if (af == AF_INET6 || af == AF_UNSPEC)
		{
			result = _wfp_createfilter (name, fwfc, count, weight, FWPM_LAYER_ALE_AUTH_CONNECT_V6, nullptr, is_block, is_boottime);

			if (result != ERROR_SUCCESS)
				_app_logerror (L"FwpmFilterAdd", result, rule, true);
		}
	}

	if (dir == DirInbound || dir == DirBoth)
	{
		if (ip_idx != UINT32 (-1))
			fwfc[ip_idx].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;

		if (port_idx != UINT32 (-1))
			fwfc[port_idx].fieldKey = FWPM_CONDITION_IP_LOCAL_PORT;

		if (af == AF_INET || af == AF_UNSPEC)
		{
			result = _wfp_createfilter (name, fwfc, count, weight, FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, nullptr, is_block, is_boottime);

			if (result != ERROR_SUCCESS)
				_app_logerror (L"FwpmFilterAdd", result, rule, true);
		}

		if (af == AF_INET6 || af == AF_UNSPEC)
		{
			result = _wfp_createfilter (name, fwfc, count, weight, FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, is_block, is_boottime);

			if (result != ERROR_SUCCESS)
				_app_logerror (L"FwpmFilterAdd", result, rule, true);
		}
	}

	// apply listen connections filter rule (apps/ports)
	if (!app.ConfigGet (L"AllowListenConnections2", true).AsBool () && !protocol && ip_idx == UINT32 (-1))
	{
		if (dir == DirInbound || dir == DirBoth)
		{
			if (af == AF_INET || af == AF_UNSPEC)
			{
				result = _wfp_createfilter (name, fwfc, count, weight, FWPM_LAYER_ALE_AUTH_LISTEN_V4, nullptr, is_block, is_boottime);

				if (result != ERROR_SUCCESS)
					_app_logerror (L"FwpmFilterAdd", result, rule, true);
			}

			if (af == AF_INET6 || af == AF_UNSPEC)
			{
				result = _wfp_createfilter (name, fwfc, count, weight, FWPM_LAYER_ALE_AUTH_LISTEN_V6, nullptr, is_block, is_boottime);

				if (result != ERROR_SUCCESS)
					_app_logerror (L"FwpmFilterAdd", result, rule, true);
			}
		}
	}

	// proxy connections (win8 and above)
	if (_r_sys_validversion (6, 2) && app.ConfigGet (L"EnableProxySupport", false).AsBool ())
	{
		if (path && dir == DirBoth && !rule)
		{
			fwfc[count].fieldKey = FWPM_CONDITION_FLAGS;
			fwfc[count].matchType = FWP_MATCH_FLAGS_ANY_SET;
			fwfc[count].conditionValue.type = FWP_UINT32;
			fwfc[count].conditionValue.uint32 = FWP_CONDITION_FLAG_IS_CONNECTION_REDIRECTED | FWP_CONDITION_FLAG_IS_PROXY_CONNECTION;

			count += 1;

			result = _wfp_createfilter (name, fwfc, count, FILTER_WEIGHT_HIGHEST_IMPORTANT, FWPM_LAYER_ALE_AUTH_CONNECT_V4, nullptr, FALSE, FALSE);

			if (result != ERROR_SUCCESS)
				_app_logerror (L"FwpmFilterAdd", result, rule, true);

			result = _wfp_createfilter (name, fwfc, count, FILTER_WEIGHT_HIGHEST_IMPORTANT, FWPM_LAYER_ALE_AUTH_CONNECT_V6, nullptr, FALSE, FALSE);

			if (result != ERROR_SUCCESS)
				_app_logerror (L"FwpmFilterAdd", result, rule, true);
		}
	}

	_FwpmFreeAppIdFromFileName1 (&blob);
	LocalFree (psid);

	return true;
}

LPVOID _app_loadresource (UINT rc, PDWORD size)
{
	const HINSTANCE hinst = app.GetHINSTANCE ();

	HRSRC hres = FindResource (hinst, MAKEINTRESOURCE (rc), RT_RCDATA);

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

void _app_loadrules (HWND hwnd, LPCWSTR path, UINT rc, bool is_internal, std::vector<ITEM_RULE*> *ptr_rules)
{
	if (!ptr_rules)
		return;

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file (path, PUGIXML_LOAD_FLAGS, PUGIXML_LOAD_ENCODING);

	if (!result)
	{
		// show only syntax, memory and i/o errors...
		if (result.status != pugi::status_file_not_found)
			_app_logerror (L"pugi::load_file", 0, _r_fmt (L"status: %d, offset: %d, text: %s, file: %s", result.status, result.offset, rstring (result.description ()).GetString (), path));

		// if file not found or parsing error, get internal rules (from resources)
		if (rc)
		{
			DWORD size = 0;
			LPVOID buffer = _app_loadresource (rc, &size);

			if (buffer)
				result = doc.load_buffer (buffer, size, PUGIXML_LOAD_FLAGS, PUGIXML_LOAD_ENCODING);
		}
	}

	if (result)
	{
		pugi::xml_node root = doc.child (L"root");

		if (root)
		{
			_r_fastlock_acquireexclusive (&lock_access);

			const bool is_loadextrarules = app.ConfigGet (L"IsExtraRulesEnabled", false).AsBool ();

			for (pugi::xml_node item = root.child (L"item"); item; item = item.next_sibling (L"item"))
			{
				if (!item.attribute (L"is_extra").empty () && item.attribute (L"is_extra").as_bool () && !is_loadextrarules)
					continue;

				const size_t idx = ptr_rules->size ();
				ITEM_RULE* rule_ptr = (ITEM_RULE*)malloc (sizeof (ITEM_RULE));

				if (rule_ptr)
				{
					SecureZeroMemory (rule_ptr, sizeof (ITEM_RULE));

					// allocate required memory
					{
						rstring attr_name = item.attribute (L"name").as_string ();
						rstring attr_rule = item.attribute (L"rule").as_string ();

						const size_t name_length = (std::min) (attr_name.GetLength (), (size_t)RULE_NAME_CCH_MAX) + 1;
						const size_t rule_length = (std::min) (attr_rule.GetLength (), (size_t)RULE_RULE_CCH_MAX) + 1;

						rule_ptr->pname = (LPWSTR)malloc ((name_length + 1) * sizeof (WCHAR));
						rule_ptr->prule = (LPWSTR)malloc ((rule_length + 1) * sizeof (WCHAR));

						if (rule_ptr->pname)
							StringCchCopy (rule_ptr->pname, name_length, attr_name);

						if (rule_ptr->prule)
							StringCchCopy (rule_ptr->prule, rule_length, attr_rule);
					}

					rule_ptr->dir = (EnumRuleDirection)item.attribute (L"dir").as_uint ();

					if (!item.attribute (L"type").empty ())
						rule_ptr->type = (EnumRuleType)item.attribute (L"type").as_uint ();

					rule_ptr->protocol = (UINT8)item.attribute (L"protocol").as_uint ();
					rule_ptr->version = (ADDRESS_FAMILY)item.attribute (L"version").as_uint ();

					if (!item.attribute (L"apps").empty ())
					{
						// allocate required memory
						rstring attr_apps = item.attribute (L"apps").as_string ();
						const size_t apps_length = (std::min) (attr_apps.GetLength (), (size_t)RULE_APPS_CCH_MAX) + 1;

						rule_ptr->papps = (LPWSTR)malloc ((apps_length + 1) * sizeof (WCHAR));

						if (rule_ptr->papps)
						{
							StringCchCopy (rule_ptr->papps, apps_length, attr_apps);

							if (!is_internal)
							{
								rstring::rvector arr = rstring (rule_ptr->papps).AsVector (RULE_DELIMETER);

								for (size_t i = 0; i < arr.size (); i++)
								{
									const rstring path_app = _r_path_expand (arr.at (i).Trim (L"\r\n "));

									if (!path_app.IsEmpty ())
									{
										const size_t hash = path_app.Hash ();

										if (hash)
										{
											if (apps.find (hash) == apps.end ())
												_app_addapplication (hwnd, path_app, 0, false, false, true);

											apps[hash].rules.push_back (idx);
										}
									}
								}
							}
						}
					}

					rule_ptr->is_block = item.attribute (L"is_block").as_bool ();
					rule_ptr->is_enabled = item.attribute (L"is_enabled").as_bool ();

					if (is_internal)
					{
						// internal rules
						if (rules_config.find (rule_ptr->pname) != rules_config.end ())
							rule_ptr->is_enabled = rules_config[rule_ptr->pname];
						else
							rules_config[rule_ptr->pname] = rule_ptr->is_enabled;
					}

					ptr_rules->push_back (rule_ptr);
				}
			}

			_r_fastlock_releaseexclusive (&lock_access);
		}
	}
}

void _app_profileload (HWND hwnd, LPCWSTR path_apps = nullptr, LPCWSTR path_rules = nullptr)
{
	// load applications
	{
		const size_t item_id = (size_t)SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
		const INT scroll_pos = GetScrollPos (GetDlgItem (hwnd, IDC_LISTVIEW), SB_VERT);

		_r_fastlock_acquireexclusive (&lock_access);

		// clear rules
		for (size_t i = 0; i < rules_custom.size (); i++)
		{
			if (rules_custom.at (i))
				_app_freerule (&rules_custom.at (i));
		}

		apps.clear ();
		rules_custom.clear ();
		rules_config.clear ();

		_r_listview_deleteallitems (hwnd, IDC_LISTVIEW);

		_r_fastlock_releaseexclusive (&lock_access);

		// generate package list (win8 and above)
		if (_r_sys_validversion (6, 2))
			_app_package_generate ();

		{
			pugi::xml_document doc;
			pugi::xml_parse_result result = doc.load_file (path_apps ? path_apps : config.apps_path, PUGIXML_LOAD_FLAGS, PUGIXML_LOAD_ENCODING);

			if (!result)
			{
				// show only syntax, memory and i/o errors...
				if (result.status != pugi::status_file_not_found)
					_app_logerror (L"pugi::load_file", 0, _r_fmt (L"status: %d, offset: %d, text: %s, file: %s", result.status, result.offset, rstring (result.description ()).GetString (), config.apps_path));
			}
			else
			{
				pugi::xml_node root = doc.child (L"root");

				if (root)
				{
					_r_fastlock_acquireexclusive (&lock_access);

					for (pugi::xml_node item = root.child (L"item"); item; item = item.next_sibling (L"item"))
						_app_addapplication (hwnd, item.attribute (L"path").as_string (), item.attribute (L"timestamp").as_ullong (), item.attribute (L"is_silent").as_bool (), item.attribute (L"is_enabled").as_bool (), true);

					_r_fastlock_releaseexclusive (&lock_access);
				}
			}
		}

		ShowItem (hwnd, IDC_LISTVIEW, item_id, scroll_pos);
	}

	// load rules config
	{
		pugi::xml_document doc;
		pugi::xml_parse_result result = doc.load_file (config.rules_config_path, PUGIXML_LOAD_FLAGS, PUGIXML_LOAD_ENCODING);

		if (result)
		{
			pugi::xml_node root = doc.child (L"root");

			if (root)
			{
				for (pugi::xml_node item = root.child (L"item"); item; item = item.next_sibling (L"item"))
					rules_config[item.attribute (L"name").as_string ()] = item.attribute (L"is_enabled").as_bool ();
			}
		}
	}

	// load internal rules
	if (rules_blocklist.empty ())
		_app_loadrules (hwnd, _r_fmt (L"%s\\" XML_BLOCKLIST, app.GetProfileDirectory ()), IDR_RULES_BLOCKLIST, true, &rules_blocklist);

	if (rules_system.empty ())
		_app_loadrules (hwnd, _r_fmt (L"%s\\" XML_RULES_SYSTEM, app.GetProfileDirectory ()), IDR_RULES_SYSTEM, true, &rules_system);

	// load custom rules
	_app_loadrules (hwnd, path_rules ? path_rules : config.rules_custom_path, 0, false, &rules_custom);

	_app_refreshstatus (hwnd, true, true);
}

void _app_profilesave (HWND hwnd, LPCWSTR path_apps = nullptr, LPCWSTR path_rules = nullptr)
{
	// apps list
	{
		pugi::xml_document doc;
		pugi::xml_node root = doc.append_child (L"root");

		if (root)
		{
			_r_fastlock_acquireshared (&lock_access);

			const size_t count = _r_listview_getitemcount (hwnd, IDC_LISTVIEW);

			if (count)
			{
				for (size_t i = 0; i < count; i++)
				{
					const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, IDC_LISTVIEW, i);

					if (hash)
					{
						pugi::xml_node item = root.append_child (L"item");

						if (item)
						{
							ITEM_APPLICATION const *ptr_app = _app_getapplication (hash);

							if (ptr_app)
							{
								item.append_attribute (L"path").set_value (ptr_app->original_path);
								item.append_attribute (L"timestamp").set_value (ptr_app->timestamp);
								item.append_attribute (L"is_silent").set_value (ptr_app->is_silent);
								item.append_attribute (L"is_enabled").set_value (ptr_app->is_enabled);
							}
						}
					}
				}
			}
			else
			{
				for (auto const &p : apps)
				{
					const size_t hash = p.first;

					if (hash)
					{
						pugi::xml_node item = root.append_child (L"item");

						if (item)
						{
							ITEM_APPLICATION const *ptr_app = &p.second;

							item.append_attribute (L"path").set_value (ptr_app->original_path);
							item.append_attribute (L"timestamp").set_value (ptr_app->timestamp);
							item.append_attribute (L"is_silent").set_value (ptr_app->is_silent);
							item.append_attribute (L"is_enabled").set_value (ptr_app->is_enabled);
						}
					}
				}
			}

			_r_fastlock_releaseshared (&lock_access);

			doc.save_file (path_apps ? path_apps : config.apps_path, L"\t", PUGIXML_SAVE_FLAGS, PUGIXML_SAVE_ENCODING);
		}
	}

	// internal rules config
	{
		pugi::xml_document doc;
		pugi::xml_node root = doc.append_child (L"root");

		if (root)
		{
			_r_fastlock_acquireshared (&lock_access);

			for (auto const &p : rules_config)
			{
				pugi::xml_node item = root.append_child (L"item");

				if (item)
				{
					item.append_attribute (L"name").set_value (p.first);
					item.append_attribute (L"is_enabled").set_value (p.second);
				}
			}

			_r_fastlock_releaseshared (&lock_access);

			doc.save_file (config.rules_config_path, L"\t", PUGIXML_SAVE_FLAGS, PUGIXML_SAVE_ENCODING);
		}
	}

	// custom rules
	{
		if (rules_custom.empty ())
		{
			if (_r_fs_exists (config.rules_custom_path))
				_r_fs_delete (config.rules_custom_path);
		}
		else
		{
			pugi::xml_document doc;
			pugi::xml_node root = doc.append_child (L"root");

			if (root)
			{
				_r_fastlock_acquireshared (&lock_access);

				for (size_t i = 0; i < rules_custom.size (); i++)
				{
					ITEM_RULE const* ptr_rule = rules_custom.at (i);

					if (ptr_rule)
					{
						pugi::xml_node item = root.append_child (L"item");

						if (item)
						{
							item.append_attribute (L"name").set_value (ptr_rule->pname);
							item.append_attribute (L"rule").set_value (ptr_rule->prule);
							item.append_attribute (L"dir").set_value (ptr_rule->dir);

							if (ptr_rule->type != TypeUnknown)
								item.append_attribute (L"type").set_value (ptr_rule->type);

							item.append_attribute (L"protocol").set_value (ptr_rule->protocol);
							item.append_attribute (L"version").set_value (ptr_rule->version);

							// add apps attribute
							{
								rstring arr;
								bool is_haveapps = false;

								for (auto const &p : apps)
								{
									ITEM_APPLICATION const* ptr_app = _app_getapplication (p.first);

									if (ptr_app)
									{
										for (size_t j = 0; j < p.second.rules.size (); j++)
										{
											if (p.second.rules.at (j) == i)
											{
												arr.Append (_r_path_unexpand (ptr_app->original_path));
												arr.Append (RULE_DELIMETER);

												is_haveapps = true;

												break;
											}
										}
									}
								}

								if (is_haveapps)
								{
									arr.Trim (RULE_DELIMETER);

									item.append_attribute (L"apps").set_value (arr);
								}
							}

							item.append_attribute (L"is_block").set_value (ptr_rule->is_block);
							item.append_attribute (L"is_enabled").set_value (ptr_rule->is_enabled);
						}
					}
				}

				_r_fastlock_releaseshared (&lock_access);

				doc.save_file (path_rules ? path_rules : config.rules_custom_path, L"\t", PUGIXML_SAVE_FLAGS, PUGIXML_SAVE_ENCODING);
			}
		}
	}
}

bool _wfp_isfiltersinstalled ()
{
	HKEY key = nullptr;
	bool result = false;

	if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\services\\BFE\\Parameters\\Policy\\Persistent\\Provider", 0, KEY_READ, &key) == ERROR_SUCCESS)
	{
		OLECHAR* guidString = nullptr;

		if (SUCCEEDED (StringFromCLSID (GUID_WfpProvider, &guidString)))
		{
			if (RegQueryValueEx (key, guidString, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
				result = true;

			CoTaskMemFree (guidString);
		}

		RegCloseKey (key);
	}

	return result;
}

UINT _wfp_installfilters ()
{
	UINT error_count = 0;

	_wfp_destroyfilters (false); // destroy all installed filters first

	DWORD result = FwpmTransactionBegin (config.hengine, 0);

	if (result != ERROR_SUCCESS)
	{
		_app_logerror (L"FwpmTransactionBegin", result, nullptr);
		++error_count;
	}
	else
	{
		const EnumMode mode = (EnumMode)app.ConfigGet (L"Mode", ModeWhitelist).AsUint ();

		FWPM_FILTER_CONDITION fwfc[6] = {0};

		// add loopback connections permission
		if (app.ConfigGet (L"AllowLoopbackConnections", true).AsBool ())
		{
			// match all loopback (localhost) data
			fwfc[0].fieldKey = FWPM_CONDITION_FLAGS;
			fwfc[0].matchType = FWP_MATCH_FLAGS_ANY_SET;
			fwfc[0].conditionValue.type = FWP_UINT32;
			fwfc[0].conditionValue.uint32 = FWP_CONDITION_FLAG_IS_LOOPBACK;

			// tests if the network traffic is (non-)app container loopback traffic (win8 and above)
			if (_r_sys_validversion (6, 2))
				fwfc[0].conditionValue.uint32 |= (FWP_CONDITION_FLAG_IS_APPCONTAINER_LOOPBACK | FWP_CONDITION_FLAG_IS_NON_APPCONTAINER_LOOPBACK);

			error_count += ERROR_SUCCESS != _wfp_createfilter (nullptr, fwfc, 1, FILTER_WEIGHT_HIGHEST, FWPM_LAYER_ALE_AUTH_CONNECT_V4, nullptr, FALSE, false);
			error_count += ERROR_SUCCESS != _wfp_createfilter (nullptr, fwfc, 1, FILTER_WEIGHT_HIGHEST, FWPM_LAYER_ALE_AUTH_CONNECT_V6, nullptr, FALSE, false);

			// boot-time filters loopback permission
			if (app.ConfigGet (L"InstallBoottimeFilters", false).AsBool ())
			{
				error_count += ERROR_SUCCESS != _wfp_createfilter (nullptr, fwfc, 1, FILTER_WEIGHT_HIGHEST, FWPM_LAYER_ALE_AUTH_CONNECT_V4, nullptr, FALSE, true);
				error_count += ERROR_SUCCESS != _wfp_createfilter (nullptr, fwfc, 1, FILTER_WEIGHT_HIGHEST, FWPM_LAYER_ALE_AUTH_CONNECT_V6, nullptr, FALSE, true);
			}

			// ipv4/ipv6 loopback
			LPCWSTR ip_list[] = {L"10.0.0.0/8", L"172.16.0.0/12", L"169.254.0.0/16", L"192.168.0.0/16", L"224.0.0.0/24", L"fd00::/8", L"fe80::/10"};

			for (size_t i = 0; i < _countof (ip_list); i++)
			{
				if (WaitForSingleObjectEx (config.stop_evt, 0, FALSE) == WAIT_OBJECT_0)
				{
					FwpmTransactionAbort (config.hengine);
					return 0;
				}

				FWP_V4_ADDR_AND_MASK addr4 = {0};
				FWP_V6_ADDR_AND_MASK addr6 = {0};

				ITEM_ADDRESS addr;
				SecureZeroMemory (&addr, sizeof (addr));

				addr.paddr4 = &addr4;
				addr.paddr6 = &addr6;

				EnumRuleType rule_type = TypeIp;

				if (_app_parserulestring (ip_list[i], &addr, &rule_type))
				{
					//fwfc[1].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;
					fwfc[1].matchType = FWP_MATCH_EQUAL;

					if (addr.format == NET_ADDRESS_IPV4)
					{
						fwfc[1].conditionValue.type = FWP_V4_ADDR_MASK;
						fwfc[1].conditionValue.v4AddrMask = &addr4;

						fwfc[1].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
						error_count += ERROR_SUCCESS != _wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST, FWPM_LAYER_ALE_AUTH_CONNECT_V4, nullptr, FALSE, false);

						if (app.ConfigGet (L"InstallBoottimeFilters", false).AsBool ())
							error_count += ERROR_SUCCESS != _wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST, FWPM_LAYER_ALE_AUTH_CONNECT_V4, nullptr, FALSE, true);

						fwfc[1].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;
						error_count += ERROR_SUCCESS != _wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST, FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, nullptr, FALSE, false);

						if (app.ConfigGet (L"InstallBoottimeFilters", false).AsBool ())
							error_count += ERROR_SUCCESS != _wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST, FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, nullptr, FALSE, true);
					}
					else if (addr.format == NET_ADDRESS_IPV6)
					{
						fwfc[1].conditionValue.type = FWP_V6_ADDR_MASK;
						fwfc[1].conditionValue.v6AddrMask = &addr6;

						fwfc[1].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
						error_count += ERROR_SUCCESS != _wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST, FWPM_LAYER_ALE_AUTH_CONNECT_V6, nullptr, FALSE, false);

						if (app.ConfigGet (L"InstallBoottimeFilters", false).AsBool ())
							error_count += ERROR_SUCCESS != _wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST, FWPM_LAYER_ALE_AUTH_CONNECT_V6, nullptr, FALSE, true);

						fwfc[1].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;
						error_count += ERROR_SUCCESS != _wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST, FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, FALSE, false);

						if (app.ConfigGet (L"InstallBoottimeFilters", false).AsBool ())
							error_count += ERROR_SUCCESS != _wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST, FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, FALSE, true);
					}
				}
			}
		}

		// apply apps rules
		{
			const bool is_block = (mode == ModeBlacklist) ? true : false;

			_r_fastlock_acquireshared (&lock_access);

			// unlock main app
			error_count += !_wfp_createrulefilter (nullptr, nullptr, app.GetBinaryPath (), DirBoth, nullptr, 0, AF_UNSPEC, false, FILTER_WEIGHT_APPLICATION, false);

			for (auto& p : apps)
			{
				if (WaitForSingleObjectEx (config.stop_evt, 0, FALSE) == WAIT_OBJECT_0)
				{
					_r_fastlock_releaseshared (&lock_access);

					FwpmTransactionAbort (config.hengine);
					return 0;
				}

				ITEM_APPLICATION *ptr_app = &p.second;

				if (ptr_app)
				{
					LPCWSTR path = ptr_app->original_path;

					if (ptr_app->is_enabled)
					{
						ptr_app->error_count = !_wfp_createrulefilter (nullptr, nullptr, path, DirBoth, nullptr, 0, AF_UNSPEC, is_block, FILTER_WEIGHT_APPLICATION, false);
						error_count += ptr_app->error_count;
					}
				}
			}

			_r_fastlock_releaseshared (&lock_access);
		}

		// apply system/custom/blocklist rules
		{
			std::vector<ITEM_RULE*> const* ptr_rules[] = {
				&rules_system,
				&rules_custom,
				&rules_blocklist,
			};

			_r_fastlock_acquireshared (&lock_access);

			for (size_t i = 0; i < _countof (ptr_rules); i++)
			{
				if (!ptr_rules[i])
					continue;

				UINT8 weight = FILTER_WEIGHT_CUSTOM;

				if (i == 0)
					weight = FILTER_WEIGHT_SYSTEM;

				else if (i == 1)
					weight = FILTER_WEIGHT_CUSTOM;

				else if (i == 2)
					weight = FILTER_WEIGHT_BLOCKLIST;

				for (size_t j = 0; j < ptr_rules[i]->size (); j++)
				{
					if (WaitForSingleObjectEx (config.stop_evt, 0, FALSE) == WAIT_OBJECT_0)
					{
						FwpmTransactionAbort (config.hengine);
						return 0;
					}

					ITEM_RULE *ptr_rule = ptr_rules[i]->at (j);

					if (ptr_rule && ptr_rule->is_enabled)
					{
						rstring::rvector rule_arr = rstring (ptr_rule->prule).AsVector (RULE_DELIMETER);

						for (size_t k = 0; k < rule_arr.size (); k++)
						{
							if (ptr_rule->papps)
							{
								// apply rule for specified apps (special)
								rstring::rvector path_arr = rstring (ptr_rule->papps).AsVector (RULE_DELIMETER);

								for (size_t l = 0; l < path_arr.size (); l++)
								{
									const rstring path = _r_path_expand (path_arr.at (l)).Trim (L"\r\n ");

									if (!path.IsEmpty ())
										ptr_rule->error_count = !_wfp_createrulefilter (ptr_rule->pname, rule_arr.at (k), path, ptr_rule->dir, &ptr_rule->type, ptr_rule->protocol, ptr_rule->version, ptr_rule->is_block, weight, false);
								}
							}
							else
							{
								// apply rule for all apps (global)
								ptr_rule->error_count = !_wfp_createrulefilter (ptr_rule->pname, rule_arr.at (k), nullptr, ptr_rule->dir, &ptr_rule->type, ptr_rule->protocol, ptr_rule->version, ptr_rule->is_block, weight, false);
							}

							error_count += ptr_rule->error_count;
						}
					}
				}
			}

			_r_fastlock_releaseshared (&lock_access);
		}

		// firewall service rules
		// https://msdn.microsoft.com/en-us/library/gg462153.aspx
		{
			// allows 6to4 tunneling, which enables ipv6 to run over an ipv4 network
			fwfc[0].fieldKey = FWPM_CONDITION_IP_PROTOCOL;
			fwfc[0].matchType = FWP_MATCH_EQUAL;
			fwfc[0].conditionValue.type = FWP_UINT8;
			fwfc[0].conditionValue.uint8 = IPPROTO_IPV6; // ipv6 header

			error_count += ERROR_SUCCESS != _wfp_createfilter (L"Allow6to4", fwfc, 1, FILTER_WEIGHT_HIGHEST, FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, nullptr, FALSE, false);

			// allows icmpv6 router solicitation messages, which are required for the ipv6 stack to work properly
			fwfc[0].fieldKey = FWPM_CONDITION_ICMP_TYPE;
			fwfc[0].matchType = FWP_MATCH_EQUAL;
			fwfc[0].conditionValue.type = FWP_UINT16;
			fwfc[0].conditionValue.uint16 = 0x85;

			error_count += ERROR_SUCCESS != _wfp_createfilter (L"AllowIcmpV6Type133", fwfc, 1, FILTER_WEIGHT_HIGHEST, FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, FALSE, false);

			// allows icmpv6 router advertise messages, which are required for the ipv6 stack to work properly
			fwfc[0].conditionValue.uint16 = 0x86;
			error_count += ERROR_SUCCESS != _wfp_createfilter (L"AllowIcmpV6Type134", fwfc, 1, FILTER_WEIGHT_HIGHEST, FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, FALSE, false);

			// allows icmpv6 neighbor solicitation messages, which are required for the ipv6 stack to work properly
			fwfc[0].conditionValue.uint16 = 0x87;
			error_count += ERROR_SUCCESS != _wfp_createfilter (L"AllowIcmpV6Type135", fwfc, 1, FILTER_WEIGHT_HIGHEST, FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, FALSE, false);

			// allows icmpv6 neighbor advertise messages, which are required for the ipv6 stack to work properly
			fwfc[0].conditionValue.uint16 = 0x88;
			error_count += ERROR_SUCCESS != _wfp_createfilter (L"AllowIcmpV6Type136", fwfc, 1, FILTER_WEIGHT_HIGHEST, FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, FALSE, false);
		}

		// prevent port scanning using stealth discards and silent drops
		if (app.ConfigGet (L"UseStealthMode", false).AsBool ())
		{
			// blocks udp port scanners
			fwfc[0].fieldKey = FWPM_CONDITION_ICMP_TYPE;
			fwfc[0].matchType = FWP_MATCH_EQUAL;
			fwfc[0].conditionValue.type = FWP_UINT16;
			fwfc[0].conditionValue.uint16 = 0x03; // destination unreachable

			error_count += ERROR_SUCCESS != _wfp_createfilter (L"BlockIcmpErrorV4", fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, FWPM_LAYER_OUTBOUND_ICMP_ERROR_V4, nullptr, TRUE, false);
			error_count += ERROR_SUCCESS != _wfp_createfilter (L"BlockIcmpErrorV6", fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, FWPM_LAYER_OUTBOUND_ICMP_ERROR_V6, nullptr, TRUE, false);

			// blocks tcp port scanners (exclude loopback)
			fwfc[0].fieldKey = FWPM_CONDITION_FLAGS;
			fwfc[0].matchType = FWP_MATCH_FLAGS_NONE_SET;
			fwfc[0].conditionValue.type = FWP_UINT32;
			fwfc[0].conditionValue.uint32 = FWP_CONDITION_FLAG_IS_LOOPBACK;

			// tests if the network traffic is (non-)app container loopback traffic (win8 and above)
			if (_r_sys_validversion (6, 2))
				fwfc[0].conditionValue.uint32 |= (FWP_CONDITION_FLAG_IS_APPCONTAINER_LOOPBACK | FWP_CONDITION_FLAG_IS_NON_APPCONTAINER_LOOPBACK);

			error_count += ERROR_SUCCESS != _wfp_createfilter (L"BlockTcpRstOnCloseV4", fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, FWPM_LAYER_INBOUND_TRANSPORT_V4_DISCARD, &FWPM_CALLOUT_WFP_TRANSPORT_LAYER_V4_SILENT_DROP, FWP_ACTION_CALLOUT_TERMINATING, false);
			error_count += ERROR_SUCCESS != _wfp_createfilter (L"BlockTcpRstOnCloseV6", fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, FWPM_LAYER_INBOUND_TRANSPORT_V6_DISCARD, &FWPM_CALLOUT_WFP_TRANSPORT_LAYER_V6_SILENT_DROP, FWP_ACTION_CALLOUT_TERMINATING, false);
		}

		// block all outbound traffic (only on "whitelist" mode)
		if (mode == ModeWhitelist)
		{
			error_count += ERROR_SUCCESS != _wfp_createfilter (L"Block all outbound connections", nullptr, 0, FILTER_WEIGHT_LOWEST, FWPM_LAYER_ALE_AUTH_CONNECT_V4, nullptr, TRUE, false);
			error_count += ERROR_SUCCESS != _wfp_createfilter (L"Block all outbound connections", nullptr, 0, FILTER_WEIGHT_LOWEST, FWPM_LAYER_ALE_AUTH_CONNECT_V6, nullptr, TRUE, false);

			// proxy connections (win8 and above)
			if (_r_sys_validversion (6, 2) && app.ConfigGet (L"EnableProxySupport", false).AsBool ())
			{
				fwfc[0].fieldKey = FWPM_CONDITION_FLAGS;
				fwfc[0].matchType = FWP_MATCH_FLAGS_ANY_SET;
				fwfc[0].conditionValue.type = FWP_UINT32;
				fwfc[0].conditionValue.uint32 = FWP_CONDITION_FLAG_IS_CONNECTION_REDIRECTED | FWP_CONDITION_FLAG_IS_PROXY_CONNECTION;

				error_count += ERROR_SUCCESS != _wfp_createfilter (L"Block all outbound connections (proxy)", fwfc, 1, FILTER_WEIGHT_HIGHEST, FWPM_LAYER_ALE_AUTH_CONNECT_V4, nullptr, TRUE, false);
				error_count += ERROR_SUCCESS != _wfp_createfilter (L"Block all outbound connections (proxy)", fwfc, 1, FILTER_WEIGHT_HIGHEST, FWPM_LAYER_ALE_AUTH_CONNECT_V6, nullptr, TRUE, false);
			}
		}

		// block all inbound traffic (only on "stealth" mode)
		if (app.ConfigGet (L"UseStealthMode", false).AsBool () || !app.ConfigGet (L"AllowInboundConnections", false).AsBool ())
		{
			error_count += ERROR_SUCCESS != _wfp_createfilter (L"Block all inbound connections", nullptr, 0, FILTER_WEIGHT_LOWEST, FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, nullptr, TRUE, false);
			error_count += ERROR_SUCCESS != _wfp_createfilter (L"Block all inbound connections", nullptr, 0, FILTER_WEIGHT_LOWEST, FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, TRUE, false);
		}

		// block all listen traffic (NOT RECOMMENDED!!!!)
		if (!app.ConfigGet (L"AllowListenConnections2", true).AsBool ())
		{
			error_count += ERROR_SUCCESS != _wfp_createfilter (L"Block all listen connections", nullptr, 0, FILTER_WEIGHT_LOWEST, FWPM_LAYER_ALE_AUTH_LISTEN_V4, nullptr, TRUE, false);
			error_count += ERROR_SUCCESS != _wfp_createfilter (L"Block all listen connections", nullptr, 0, FILTER_WEIGHT_LOWEST, FWPM_LAYER_ALE_AUTH_LISTEN_V6, nullptr, TRUE, false);
		}

		// install boot-time filters (enforced at boot-time, even before "base filtering engine" service starts)
		if (app.ConfigGet (L"InstallBoottimeFilters", false).AsBool ())
		{
			error_count += ERROR_SUCCESS != _wfp_createfilter (L"Block all outbound connections (boot-time)", nullptr, 0, FILTER_WEIGHT_LOWEST, FWPM_LAYER_ALE_AUTH_CONNECT_V4, nullptr, TRUE, true);
			error_count += ERROR_SUCCESS != _wfp_createfilter (L"Block all outbound connections (boot-time)", nullptr, 0, FILTER_WEIGHT_LOWEST, FWPM_LAYER_ALE_AUTH_CONNECT_V6, nullptr, TRUE, true);

			error_count += ERROR_SUCCESS != _wfp_createfilter (L"Block all inbound connections (boot-time)", nullptr, 0, FILTER_WEIGHT_LOWEST, FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, nullptr, TRUE, true);
			error_count += ERROR_SUCCESS != _wfp_createfilter (L"Block all inbound connections (boot-time)", nullptr, 0, FILTER_WEIGHT_LOWEST, FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, TRUE, true);
		}

		FwpmTransactionCommit (config.hengine);

		// set icons
		app.SetIcon (IDI_ACTIVE);
		app.TraySetInfo (UID, _r_loadicon (app.GetHINSTANCE (), MAKEINTRESOURCE (IDI_ACTIVE), GetSystemMetrics (SM_CXSMICON)), nullptr);

		SetDlgItemText (app.GetHWND (), IDC_START_BTN, I18N (&app, IDS_TRAY_STOP, 0));

		// dropped packets logging (win7 and above)
		if (_r_sys_validversion (6, 1))
			_wfp_logsubscribe ();
	}

	return error_count;
}

bool _app_installfilters (bool is_forced)
{
	if (is_forced || _wfp_isfiltersinstalled ())
	{
		_r_ctrl_enable (app.GetHWND (), IDC_START_BTN, false);

		if (_r_fastlock_islocked (&lock_apply) || config.hthread)
		{
			SetEvent (config.stop_evt);
			WaitForSingleObjectEx (config.finish_evt, 8000, FALSE);

			if (config.hthread)
			{
				CloseHandle (config.hthread);
				config.hthread = nullptr;
			}
		}

		config.hthread = (HANDLE)_beginthreadex (nullptr, 0, &ApplyThread, (LPVOID)true, 0, nullptr);

		return true;
	}
	else
	{
		_app_listviewsort (app.GetHWND (), IDC_LISTVIEW, -1, false);
		_app_profilesave (app.GetHWND ());

		_r_listview_redraw (app.GetHWND (), IDC_LISTVIEW);
	}

	return false;
}

bool _app_uninstallfilters ()
{
	_r_ctrl_enable (app.GetHWND (), IDC_START_BTN, false);

	if (_r_fastlock_islocked (&lock_apply) || config.hthread)
	{
		SetEvent (config.stop_evt);
		WaitForSingleObjectEx (config.finish_evt, 8000, FALSE);
	}

	config.hthread = (HANDLE)_beginthreadex (nullptr, 0, &ApplyThread, (LPVOID)false, 0, nullptr);

	return true;
}

bool _app_logchecklimit ()
{
	const size_t limit = app.ConfigGet (L"LogSizeLimitKb", 256).AsUint ();

	if (!limit || !config.hlog || config.hlog == INVALID_HANDLE_VALUE)
		return false;

	if (_r_fs_size (config.hlog) >= (limit * _R_BYTESIZE_KB))
	{
		// make backup before log truncate
		if (app.ConfigGet (L"IsLogBackup", true).AsBool ())
		{
			const rstring path = _r_path_expand (app.ConfigGet (L"LogPath", PATH_LOG));

			_r_fs_delete (path + L".bak");
			_r_fs_copy (path, path + L".bak");
		}

		SetFilePointer (config.hlog, 2, nullptr, FILE_BEGIN);
		SetEndOfFile (config.hlog);

		return true;
	}

	return false;
}

bool _app_loginit (bool is_install)
{
	// dropped packets logging (win7 and above)
	if (!_r_sys_validversion (6, 1))
		return false;

	// reset all handles
	_r_fastlock_acquireexclusive (&lock_writelog);

	if (config.hlog != nullptr && config.hlog != INVALID_HANDLE_VALUE)
	{
		CloseHandle (config.hlog);
		config.hlog = nullptr;
	}

	_r_fastlock_releaseexclusive (&lock_writelog);

	if (!is_install)
		return true; // already closed

	// check if log enabled
	if (!app.ConfigGet (L"IsLogEnabled", false).AsBool () || !_wfp_isfiltersinstalled ())
		return false;

	bool result = false;

	if (is_install)
	{
		const rstring path = _r_path_expand (app.ConfigGet (L"LogPath", PATH_LOG));

		_r_fastlock_acquireexclusive (&lock_writelog);

		config.hlog = CreateFile (path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);

		if (config.hlog == INVALID_HANDLE_VALUE)
		{
			_app_logerror (L"CreateFile", GetLastError (), path);
		}
		else
		{
			if (GetLastError () != ERROR_ALREADY_EXISTS)
			{
				DWORD written = 0;
				static const BYTE bom[] = {0xFF, 0xFE};

				WriteFile (config.hlog, bom, sizeof (bom), &written, nullptr); // write utf-16 le byte order mask
			}
			else
			{
				_app_logchecklimit ();

				SetFilePointer (config.hlog, 0, nullptr, FILE_END);
			}

			result = true;
		}

		_r_fastlock_releaseexclusive (&lock_writelog);
	}

	return result;
}

void _app_logwrite (ITEM_LOG const *ptr_log)
{
	if (!ptr_log)
		return;

	if (config.hlog != nullptr && config.hlog != INVALID_HANDLE_VALUE)
	{
		_r_fastlock_acquireexclusive (&lock_writelog);

		_app_logchecklimit ();

		// parse path
		rstring path;
		{
			ITEM_APPLICATION const *ptr_app = _app_getapplication (ptr_log->hash);

			if (ptr_app)
			{
				if (ptr_app->is_storeapp)
					path = ptr_app->real_path;

				else
					path = ptr_app->original_path;
			}
			else
			{
				path = NA_TEXT;
			}
		}

		// parse address
		rstring remote_addr;
		rstring local_addr;
		{
			if ((ptr_log->flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET) != 0)
			{
				remote_addr = ptr_log->remote_addr;

				if (ptr_log->remote_port)
					remote_addr.AppendFormat (L":%d", ptr_log->remote_port);
			}
			else
			{
				remote_addr = NA_TEXT;
			}

			if ((ptr_log->flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET) != 0)
			{
				local_addr = ptr_log->local_addr;

				if (ptr_log->local_port)
					local_addr.AppendFormat (L":%d", ptr_log->local_port);
			}
			else
			{
				local_addr = NA_TEXT;
			}
		}

		// parse filter name
		rstring filter;
		{
			if (ptr_log->provider_name[0])
				filter.Format (L"%s\\%s", ptr_log->provider_name, ptr_log->filter_name);

			else
				filter = ptr_log->filter_name;
		}

		// parse direction
		rstring direction;
		{
			if (ptr_log->direction == FWP_DIRECTION_IN)
				direction = L"In";

			else
				direction = L"Out";

			if (ptr_log->is_loopback)
				direction.Append (L"-Loopback");
		}

		rstring buffer;
		buffer.Format (L"[%s] %s (%s) [Remote: %s] [Local: %s] (%s) %s [%s]\r\n", ptr_log->date, path.GetString (), ptr_log->username, remote_addr.GetString (), local_addr.GetString (), ptr_log->protocol, filter.GetString (), direction.GetString ());

		DWORD written = 0;
		WriteFile (config.hlog, buffer.GetString (), DWORD (buffer.GetLength () * sizeof (WCHAR)), &written, nullptr);

		_r_fastlock_releaseexclusive (&lock_writelog);
	}
}

void _app_notifysetpos (HWND hwnd)
{
	RECT windowRect = {0};
	GetWindowRect (hwnd, &windowRect);

	RECT rc = {0};
	SystemParametersInfo (SPI_GETWORKAREA, 0, &rc, 0);

	windowRect.left = (rc.right - (windowRect.right - windowRect.left));
	windowRect.top = (rc.bottom - (windowRect.bottom - windowRect.top));

	SetWindowPos (hwnd, nullptr, windowRect.left - GetSystemMetrics (SM_CXBORDER), windowRect.top - GetSystemMetrics (SM_CYBORDER), 0, 0, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE | SWP_NOSIZE);
}

void _app_notifycreatewindow ()
{
	WNDCLASSEX wcex = {0};

	wcex.cbSize = sizeof (wcex);
	wcex.style = CS_VREDRAW | CS_HREDRAW;
	wcex.lpfnWndProc = &NotificationProc;
	wcex.hInstance = app.GetHINSTANCE ();
	wcex.hCursor = LoadCursor (nullptr, IDC_ARROW);
	wcex.lpszClassName = NOTIFY_CLASS_DLG;
	wcex.hbrBackground = GetSysColorBrush (COLOR_WINDOW);

	if (!RegisterClassEx (&wcex))
	{
		_app_logerror (L"RegisterClassEx", GetLastError (), nullptr);
	}
	else
	{
		const UINT width = app.GetDPI (NOTIFY_WIDTH);
		const UINT height = app.GetDPI (NOTIFY_HEIGHT);
		const UINT spacer = app.GetDPI (NOTIFY_SPACER);

		config.hnotification = CreateWindowEx (WS_EX_TOOLWINDOW | WS_EX_TOPMOST, NOTIFY_CLASS_DLG, nullptr, WS_POPUP, 0, 0, width, height, nullptr, nullptr, wcex.hInstance, nullptr);

		if (!config.hnotification)
		{
			_app_logerror (L"CreateWindowEx", GetLastError (), nullptr);
		}
		else
		{
			HFONT hfont_title = nullptr;
			HFONT hfont_text = nullptr;

			{
				NONCLIENTMETRICS ncm = {0};
				ncm.cbSize = sizeof (ncm);

				if (SystemParametersInfo (SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0))
				{
					LOGFONT* lf_title = &ncm.lfCaptionFont;
					LOGFONT* lf_text = &ncm.lfMessageFont;

					lf_title->lfHeight = _r_dc_fontsizetoheight (11);
					lf_text->lfHeight = _r_dc_fontsizetoheight (9);

					lf_title->lfWeight = FW_SEMIBOLD;
					lf_text->lfWeight = FW_NORMAL;

					// set default values
					lf_title->lfQuality = CLEARTYPE_QUALITY;
					lf_text->lfQuality = CLEARTYPE_QUALITY;

					StringCchCopy (lf_title->lfFaceName, LF_FACESIZE, L"Segoe UI");
					StringCchCopy (lf_text->lfFaceName, LF_FACESIZE, L"Segoe UI");

					hfont_title = CreateFontIndirect (lf_title);
					hfont_text = CreateFontIndirect (lf_text);
				}
			}

			HWND h = h = CreateWindowEx (0, WC_STATIC, APP_NAME, WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS | SS_CENTERIMAGE, app.GetDPI (64 + 8 + 10), app.GetDPI (10), width - app.GetDPI (64 + 8 + 10 + 24), app.GetDPI (20), config.hnotification, (HMENU)IDC_TITLE_ID, nullptr, nullptr);
			SendMessage (h, WM_SETFONT, (LPARAM)hfont_title, true);

			h = CreateWindowEx (0, WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_CENTER | SS_ICON | SS_NOTIFY, width - app.GetDPI (16) - (spacer * 2), app.GetDPI (10), GetSystemMetrics (SM_CYSMICON), GetSystemMetrics (SM_CXSMICON), config.hnotification, (HMENU)IDC_CLOSE_BTN, nullptr, nullptr);
			SendMessage (h, STM_SETIMAGE, IMAGE_ICON, (WPARAM)_r_loadicon (app.GetHINSTANCE (), MAKEINTRESOURCE (IDI_CLOSE), GetSystemMetrics (SM_CXSMICON)));

			h = CreateWindowEx (0, WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_CENTER | SS_ICON | SS_NOTIFY, app.GetDPI (8), app.GetDPI (10), app.GetDPI (62), app.GetDPI (98), config.hnotification, (HMENU)IDC_ICON_ID, nullptr, nullptr);

			h = CreateWindowEx (0, WC_EDIT, nullptr, WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL, app.GetDPI (64 + 8 + 10), app.GetDPI (36), width - app.GetDPI (64 + 8 + 10 + 8), app.GetDPI (16), config.hnotification, (HMENU)IDC_FILE_ID, nullptr, nullptr);
			SendMessage (h, WM_SETFONT, (LPARAM)hfont_text, true);

			h = CreateWindowEx (0, WC_EDIT, nullptr, WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL, app.GetDPI (64 + 8 + 10), app.GetDPI (54), width - app.GetDPI (64 + 8 + 10 + 8), app.GetDPI (16), config.hnotification, (HMENU)IDC_ADDRESS_ID, nullptr, nullptr);
			SendMessage (h, WM_SETFONT, (LPARAM)hfont_text, true);

			h = CreateWindowEx (0, WC_EDIT, nullptr, WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL, app.GetDPI (64 + 8 + 10), app.GetDPI (72), width - app.GetDPI (64 + 8 + 10 + 8), app.GetDPI (16), config.hnotification, (HMENU)IDC_FILTER_ID, nullptr, nullptr);
			SendMessage (h, WM_SETFONT, (LPARAM)hfont_text, true);

			h = CreateWindowEx (0, WC_EDIT, nullptr, WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL, app.GetDPI (64 + 8 + 10), app.GetDPI (90), width - app.GetDPI (64 + 8 + 10 + 8), app.GetDPI (16), config.hnotification, (HMENU)IDC_DATE_ID, nullptr, nullptr);
			SendMessage (h, WM_SETFONT, (LPARAM)hfont_text, true);

			h = CreateWindowEx (0, WC_BUTTON, nullptr, WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_CHECKBOX, app.GetDPI (12), app.GetDPI (120), width - app.GetDPI (20), app.GetDPI (16), config.hnotification, (HMENU)IDC_CREATERULE_ADDR_ID, nullptr, nullptr);
			SendMessage (h, WM_SETFONT, (LPARAM)hfont_text, true);

			h = CreateWindowEx (0, WC_BUTTON, nullptr, WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_CHECKBOX, app.GetDPI (12), app.GetDPI (138), width - app.GetDPI (20), app.GetDPI (16), config.hnotification, (HMENU)IDC_CREATERULE_PORT_ID, nullptr, nullptr);
			SendMessage (h, WM_SETFONT, (LPARAM)hfont_text, true);

			h = CreateWindowEx (0, WC_BUTTON, nullptr, WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_CHECKBOX, app.GetDPI (12), app.GetDPI (156), width - app.GetDPI (20), app.GetDPI (16), config.hnotification, (HMENU)IDC_DISABLENOTIFY_ID, nullptr, nullptr);
			SendMessage (h, WM_SETFONT, (LPARAM)hfont_text, true);

			h = CreateWindowEx (app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WC_BUTTON, L"<", WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, app.GetDPI (10), height - app.GetDPI (36), app.GetDPI (24), app.GetDPI (24), config.hnotification, (HMENU)IDC_PREV_ID, nullptr, nullptr);
			SendMessage (h, WM_SETFONT, (LPARAM)hfont_text, true);

			h = CreateWindowEx (app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WC_BUTTON, L">", WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, app.GetDPI (10 + 24 + 2), height - app.GetDPI (36), app.GetDPI (24), app.GetDPI (24), config.hnotification, (HMENU)IDC_NEXT_ID, nullptr, nullptr);
			SendMessage (h, WM_SETFONT, (LPARAM)hfont_text, true);

			h = CreateWindowEx (0, WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS | SS_CENTERIMAGE, app.GetDPI (10 + 24 + 2 + 24 + 6), height - app.GetDPI (36), app.GetDPI (44), app.GetDPI (24), config.hnotification, (HMENU)IDC_IDX_ID, nullptr, nullptr);
			SendMessage (h, WM_SETFONT, (LPARAM)hfont_text, true);

			h = CreateWindowEx (app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WC_BUTTON, nullptr, WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, width - ((app.GetDPI (120) + app.GetDPI (8)) * 2), height - app.GetDPI (36), app.GetDPI (120), app.GetDPI (24), config.hnotification, (HMENU)IDC_ALLOW_BTN, nullptr, nullptr);
			SendMessage (h, WM_SETFONT, (LPARAM)hfont_text, true);
			SendMessage (h, BM_SETIMAGE, IMAGE_ICON, (WPARAM)_r_loadicon (app.GetHINSTANCE (), MAKEINTRESOURCE (IDI_ALLOW), GetSystemMetrics (SM_CXSMICON)));

			h = CreateWindowEx (app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WC_BUTTON, nullptr, WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, width - app.GetDPI (120) - app.GetDPI (10), height - app.GetDPI (36), app.GetDPI (120), app.GetDPI (24), config.hnotification, (HMENU)IDC_BLOCK_BTN, nullptr, nullptr);
			SendMessage (h, WM_SETFONT, (LPARAM)hfont_text, true);
			SendMessage (h, BM_SETIMAGE, IMAGE_ICON, (WPARAM)_r_loadicon (app.GetHINSTANCE (), MAKEINTRESOURCE (IDI_BLOCK), GetSystemMetrics (SM_CXSMICON)));

			{
				RECT rc2 = {0};

				rc2.left = rc2.right = app.GetDPI (5);

				SendDlgItemMessage (config.hnotification, IDC_ALLOW_BTN, BCM_SETTEXTMARGIN, 0, (LPARAM)&rc2);
				SendDlgItemMessage (config.hnotification, IDC_BLOCK_BTN, BCM_SETTEXTMARGIN, 0, (LPARAM)&rc2);
			}

			_r_ctrl_settip (config.hnotification, IDC_ICON_ID, LPSTR_TEXTCALLBACK);
			_r_ctrl_settip (config.hnotification, IDC_FILE_ID, LPSTR_TEXTCALLBACK);
			_r_ctrl_settip (config.hnotification, IDC_ADDRESS_ID, LPSTR_TEXTCALLBACK);
			_r_ctrl_settip (config.hnotification, IDC_FILTER_ID, LPSTR_TEXTCALLBACK);
			_r_ctrl_settip (config.hnotification, IDC_DATE_ID, LPSTR_TEXTCALLBACK);

			_r_ctrl_settip (config.hnotification, IDC_CREATERULE_ADDR_ID, LPSTR_TEXTCALLBACK);
			_r_ctrl_settip (config.hnotification, IDC_CREATERULE_PORT_ID, LPSTR_TEXTCALLBACK);

			_app_notifysettimeout (config.hnotification, NOTIFY_TIMER_MOUSELEAVE_ID, true, 250);

			UpdateWindow (config.hnotification);
			ShowWindow (config.hnotification, SW_HIDE);
		}
	}
}

size_t _app_notifygetcurrent ()
{
	const size_t count = notifications.size ();
	size_t new_idx = LAST_VALUE;

	if (count)
	{
		if (count == 1)
		{
			new_idx = 0;
		}
		else
		{
			const size_t idx = (size_t)GetWindowLongPtr (config.hnotification, GWLP_USERDATA);
			new_idx = (std::max) ((size_t)0, (std::min) (idx, count - 1));
		}
	}

	SetWindowLongPtr (config.hnotification, GWLP_USERDATA, new_idx);

	return new_idx;
}

void _app_notifycommand (HWND hwnd, bool is_block)
{
	const size_t idx = _app_notifygetcurrent ();

	if (idx == LAST_VALUE)
		return;

	ITEM_LOG *ptr_log = notifications.at (idx);

	if (!ptr_log || (ptr_log && !ptr_log->hash))
		return;

	const size_t hash = ptr_log->hash;
	ITEM_APPLICATION *ptr_app = _app_getapplication (hash);

	if (ptr_app)
	{
		if (IsDlgButtonChecked (hwnd, IDC_DISABLENOTIFY_ID) == BST_CHECKED)
			ptr_app->is_silent = true;

		if (!is_block)
		{
			const bool is_createaddrrule = IsDlgButtonChecked (hwnd, IDC_CREATERULE_ADDR_ID) == BST_CHECKED;
			const bool is_createportrule = IsDlgButtonChecked (hwnd, IDC_CREATERULE_PORT_ID) == BST_CHECKED;

			if (is_createaddrrule || is_createportrule)
			{
				WCHAR rule[128] = {0};

				if (is_createaddrrule)
					StringCchCopy (rule, _countof (rule), ptr_log->remote_addr);

				if (is_createportrule)
				{
					if (is_createaddrrule)
						StringCchCat (rule, _countof (rule), L":");

					StringCchCat (rule, _countof (rule), _r_fmt (L"%d", ptr_log->remote_port));
				}

				size_t rule_id = LAST_VALUE;

				const size_t rule_length = (std::min) (wcslen (rule), (size_t)RULE_RULE_CCH_MAX) + 1;

				for (size_t i = 0; i < rules_custom.size (); i++)
				{
					ITEM_RULE const *rule_ptr = rules_custom.at (i);

					if (rule_ptr)
					{
						if (!rule_ptr->is_block && rule_ptr->prule && _wcsnicmp (rule_ptr->prule, rule, rule_length) == 0)
						{
							rule_id = i;
							break;
						}
					}
				}

				if (rule_id == LAST_VALUE)
				{
					// create rule
					ITEM_RULE *ptr_rule = (ITEM_RULE*)malloc (sizeof (ITEM_RULE));

					if (ptr_rule)
					{
						SecureZeroMemory (ptr_rule, sizeof (ITEM_RULE));

						const size_t name_length = (std::min) (wcslen (rule), (size_t)RULE_NAME_CCH_MAX) + 1;
						const size_t path_length = (std::min) (wcslen (ptr_app->original_path), (size_t)RULE_APPS_CCH_MAX) + 1;

						ptr_rule->pname = (LPWSTR)malloc ((name_length + 1) * sizeof (WCHAR));

						if (ptr_rule->pname)
							StringCchCopy (ptr_rule->pname, name_length + 1, rule);

						ptr_rule->prule = (LPWSTR)malloc ((rule_length + 1) * sizeof (WCHAR));

						if (ptr_rule->prule)
							StringCchCopy (ptr_rule->prule, rule_length + 1, rule);

						ptr_rule->dir = (ptr_log->direction == FWP_DIRECTION_IN) ? DirInbound : DirOutbound;
						ptr_rule->is_block = false;
						ptr_rule->is_enabled = true;

						rule_id = rules_custom.size ();
						rules_custom.push_back (ptr_rule);
					}
				}
				else
				{
					// modify rule
				}

				// add rule for app
				if (rule_id != LAST_VALUE)
					ptr_app->rules.push_back (rule_id);
			}
			else
			{
				// allow entire app
				ptr_app->is_enabled = true;

				config.is_nocheckboxnotify = true;

				const size_t item = _app_getposition (app.GetHWND (), hash);

				_r_listview_setitem (app.GetHWND (), IDC_LISTVIEW, item, 0, nullptr, LAST_VALUE, 0);
				_r_listview_setitemcheck (app.GetHWND (), IDC_LISTVIEW, item, true);

				config.is_nocheckboxnotify = false;
			}
		}

		notifications_last[hash] = _r_unixtime_now ();

		// clear notifications
		_app_freenotify (LAST_VALUE, hash);
		_app_notifyrefresh ();

		// apply rules
		{
			_app_profilesave (app.GetHWND ());
			_app_profileload (app.GetHWND ()); // important!

			if (!is_block)
				_app_installfilters (false);
			else
				_r_listview_redraw (app.GetHWND (), IDC_LISTVIEW);
		}
	}
}

void _app_notifysettimeout (HWND hwnd, UINT_PTR id, bool is_create, UINT timeout)
{
	if (is_create)
	{
		if (!id)
			return;

		if (id == NOTIFY_TIMER_TIMEOUT_ID)
			config.is_notifytimeout = true;

		SetTimer (hwnd, id, timeout, nullptr);
	}
	else
	{
		if (!id || id == NOTIFY_TIMER_TIMEOUT_ID)
			config.is_notifytimeout = false;

		if (id)
		{
			KillTimer (hwnd, id);
		}
		else
		{
			KillTimer (hwnd, NOTIFY_TIMER_DISPLAY_ID);
			KillTimer (hwnd, NOTIFY_TIMER_TIMEOUT_ID);
		}
	}
}

bool _app_notifyshow (size_t idx)
{
	if (!app.ConfigGet (L"IsNotificationsEnabled", true).AsBool ())
		return false;

	const size_t total_size = notifications.size ();

	if (!total_size)
		return false;

	if (total_size == 1)
	{
		idx = 0;
	}
	else if (idx == LAST_VALUE)
	{
		idx = _app_notifygetcurrent ();

		if (idx == LAST_VALUE)
			return false;
	}

	ITEM_LOG const* ptr_log = notifications.at ((std::max) ((size_t)0, (std::min) (idx, total_size - 1)));

	if (ptr_log)
	{
		ITEM_APPLICATION const* ptr_app = _app_getapplication (ptr_log->hash);

		if (ptr_app)
		{
			SendDlgItemMessage (config.hnotification, IDC_ICON_ID, STM_SETIMAGE, IMAGE_ICON, (WPARAM)(ptr_log->hicon ? ptr_log->hicon : config.hicon_large));

			_r_ctrl_settext (config.hnotification, IDC_FILE_ID, L"%s: %s [%s]", I18N (&app, IDS_FILE, 0), _r_path_extractfile (ptr_app->display_name).GetString (), ptr_app->is_signed ? I18N (&app, IDS_SIGN_SIGNED, 0) : I18N (&app, IDS_SIGN_UNSIGNED, 0));

			if (ptr_log->remote_port)
				_r_ctrl_settext (config.hnotification, IDC_ADDRESS_ID, L"%s: %s:%d (%s) [%s]", I18N (&app, IDS_ADDRESS, 0), ptr_log->remote_addr, ptr_log->remote_port, ptr_log->protocol, I18N (&app, IDS_DIRECTION_1 + ((ptr_log->direction == FWP_DIRECTION_IN) ? 1 : 0), _r_fmt (L"IDS_DIRECTION_%d", (ptr_log->direction == FWP_DIRECTION_IN) ? 2 : 1)));
			else
				_r_ctrl_settext (config.hnotification, IDC_ADDRESS_ID, L"%s: %s (%s) [%s]", I18N (&app, IDS_ADDRESS, 0), ptr_log->remote_addr, ptr_log->protocol, I18N (&app, IDS_DIRECTION_1 + ((ptr_log->direction == FWP_DIRECTION_IN) ? 1 : 0), _r_fmt (L"IDS_DIRECTION_%d", (ptr_log->direction == FWP_DIRECTION_IN) ? 2 : 1)));

			_r_ctrl_settext (config.hnotification, IDC_DATE_ID, L"%s: %s", I18N (&app, IDS_DATE, 0), ptr_log->date);
			_r_ctrl_settext (config.hnotification, IDC_FILTER_ID, L"%s: %s", I18N (&app, IDS_FILTER, 0), ptr_log->filter_name);

			_r_ctrl_settext (config.hnotification, IDC_CREATERULE_ADDR_ID, I18N (&app, IDS_NOTIFY_CREATERULE_ADDRESS, 0), ptr_log->remote_addr);
			_r_ctrl_settext (config.hnotification, IDC_CREATERULE_PORT_ID, I18N (&app, IDS_NOTIFY_CREATERULE_PORT, 0), ptr_log->remote_port);

			_r_ctrl_settext (config.hnotification, IDC_DISABLENOTIFY_ID, I18N (&app, IDS_NOTIFY_DISABLENOTIFICATIONS, 0), _r_path_extractfile (ptr_app->display_name).GetString ());

			_r_ctrl_settext (config.hnotification, IDC_IDX_ID, L"%d/%d", idx + 1, total_size);

			_r_ctrl_enable (config.hnotification, IDC_CREATERULE_PORT_ID, ptr_log->remote_port != 0);

			_r_ctrl_settext (config.hnotification, IDC_ALLOW_BTN, I18N (&app, IDS_ACTION_1, 0));
			_r_ctrl_settext (config.hnotification, IDC_BLOCK_BTN, I18N (&app, IDS_ACTION_3, 0));

			CheckDlgButton (config.hnotification, IDC_CREATERULE_ADDR_ID, BST_UNCHECKED);
			CheckDlgButton (config.hnotification, IDC_CREATERULE_PORT_ID, BST_UNCHECKED);
			CheckDlgButton (config.hnotification, IDC_DISABLENOTIFY_ID, BST_UNCHECKED);

			SetWindowLongPtr (config.hnotification, GWLP_USERDATA, idx);

			_app_notifysetpos (config.hnotification);

			if (total_size > 1)
			{
				_r_ctrl_enable (config.hnotification, IDC_NEXT_ID, true);
				_r_ctrl_enable (config.hnotification, IDC_PREV_ID, true);
			}
			else
			{
				_r_ctrl_enable (config.hnotification, IDC_NEXT_ID, false);
				_r_ctrl_enable (config.hnotification, IDC_PREV_ID, false);
			}

			// redraw icon
			{
				const HWND hctrl = GetDlgItem (config.hnotification, IDC_ICON_ID);

				RECT rect = {0};

				GetClientRect (hctrl, &rect);
				InvalidateRect (hctrl, &rect, TRUE);
				MapWindowPoints (hctrl, config.hnotification, (LPPOINT)&rect, 2);
				RedrawWindow (config.hnotification, &rect, nullptr, RDW_ERASE | RDW_INVALIDATE);
			}

			ShowWindow (config.hnotification, SW_SHOW);

			return true;
		}
	}

	return false;
}

bool _app_notifyrefresh ()
{
	const size_t idx = _app_notifygetcurrent ();

	if (notifications.empty () || idx == LAST_VALUE || !IsWindowVisible (config.hnotification))
	{
		_app_notifysettimeout (config.hnotification, 0, false, 0);
		ShowWindow (config.hnotification, SW_HIDE);

		return false;
	}

	return _app_notifyshow (idx);
}

// Play notification sound even if system have "nosound" mode
void _app_notifysound ()
{
	if (app.ConfigGet (L"IsNotificationsSilent", false).AsBool ())
		return;

	bool result = false;

	if (!config.notify_snd_path[0])
	{
		HKEY key = nullptr;

		if (RegOpenKeyEx (HKEY_CURRENT_USER, L"AppEvents\\Schemes\\Apps\\.Default\\" NOTIFY_SOUND_DEFAULT L"\\.Default", 0, KEY_READ, &key) == ERROR_SUCCESS)
		{
			DWORD size = _countof (config.notify_snd_path) * sizeof (WCHAR);

			if (RegQueryValueEx (key, nullptr, nullptr, nullptr, (LPBYTE)config.notify_snd_path, &size) == ERROR_SUCCESS)
			{
				StringCchCopy (config.notify_snd_path, _countof (config.notify_snd_path), _r_path_expand (config.notify_snd_path));
				result = true;
			}

			RegCloseKey (key);
		}
	}
	else
	{
		result = true;
	}

	if (result && _r_fs_exists (config.notify_snd_path))
		PlaySound (config.notify_snd_path, nullptr, SND_SENTRY | SND_SYSTEM | SND_FILENAME | SND_ASYNC);

	else
		PlaySound (NOTIFY_SOUND_DEFAULT, nullptr, SND_SENTRY | SND_SYSTEM | SND_ASYNC);
}

void _app_notifyadd (ITEM_LOG const *ptr_log)
{
	// do not show notifications if file is not present
	if (!ptr_log || !ptr_log->hash)
		return;

	ITEM_APPLICATION* ptr_app = _app_getapplication (ptr_log->hash);

	if (!ptr_app || (ptr_app && ptr_app->is_silent))
		return;

	// check for last display time
	{
		const UINT notification_timeout = (std::max) (app.ConfigGet (L"NotificationsTimeout", NOTIFY_TIMEOUT).AsUint (), (UINT)NOTIFY_TIMEOUT_MINIMUM);

		if ((_r_unixtime_now () - notifications_last[ptr_log->hash]) <= notification_timeout)
			return;
	}

	// clear same apps log items
	_app_freenotify (LAST_VALUE, ptr_log->hash);

	// check limit
	if (notifications.size () >= NOTIFY_LIMIT_SIZE)
		_app_freenotify (0, 0);

	_app_notifyrefresh ();

	ITEM_LOG* ptr_log2 = (ITEM_LOG*)malloc (sizeof (ITEM_LOG));

	if (ptr_log2)
	{
		SecureZeroMemory (ptr_log2, sizeof (ITEM_LOG));

		memcpy_s (ptr_log2, sizeof (ITEM_LOG), ptr_log, sizeof (ITEM_LOG));

		notifications_last[ptr_log->hash] = _r_unixtime_now ();
		notifications.push_back (ptr_log2);

		const size_t total_size = notifications.size ();
		const size_t idx = total_size - 1;

		if (_app_notifyshow (idx))
		{
			_app_notifysound ();

			const UINT display_timeout = app.ConfigGet (L"NotificationsDisplayTimeout", NOTIFY_TIMER_DEFAULT).AsUint ();

			if (display_timeout)
				_app_notifysettimeout (config.hnotification, NOTIFY_TIMER_TIMEOUT_ID, true, (display_timeout * _R_SECONDSCLOCK_MSEC));
		}
	}
}

void CALLBACK _app_logcallback (FILETIME const* pft, UINT8 const* app_id, SID* package_id, SID* user_id, UINT64 filter_id, UINT32 flags, UINT8 proto, FWP_IP_VERSION ipver, BOOL is_loopback, FWP_BYTE_ARRAY16 const* remoteaddr, UINT16 remoteport, FWP_BYTE_ARRAY16 const* localaddr, UINT16 localport, UINT32 direction)
{
	const bool is_logenabled = app.ConfigGet (L"IsLogEnabled", false).AsBool ();
	const bool is_notificationenabled = app.ConfigGet (L"IsNotificationsEnabled", true).AsBool ();
	bool is_myprovider = false;
	bool is_blocklist = false;

	LPWSTR sidstring = nullptr;
	WCHAR path[MAX_PATH] = {0};

	ITEM_LOG log;
	SecureZeroMemory (&log, sizeof (log));

	// copy date and time
	if (pft)
		StringCchCopy (log.date, _countof (log.date), _r_fmt_date (_r_unixtime_from_filetime (pft), FDTF_SHORTDATE | FDTF_LONGTIME));

	if (package_id)
	{
		if (ConvertSidToStringSid (package_id, &sidstring))
		{
			if (!_app_package_get (_r_str_hash (sidstring), nullptr, nullptr))
			{
				LocalFree (sidstring);
				sidstring = nullptr;
			}
		}
	}

	// copy converted nt device path into win32
	if (sidstring)
	{
		StringCchCopy (path, _countof (path), sidstring);
		log.hash = _r_str_hash (path);

		LocalFree (sidstring);
		sidstring = nullptr;
	}
	else if (app_id)
	{
		StringCchCopy (path, _countof (path), _r_path_dospathfromnt (LPCWSTR (app_id)));
		log.hash = _r_str_hash (path);
	}
	else
	{
		StringCchCopy (path, _countof (path), NA_TEXT);
		log.hash = 0;
	}

	// apps collector
	if (log.hash && apps.find (log.hash) == apps.end ())
	{
		const UINT notification_timeout = (std::max) (app.ConfigGet (L"NotificationsTimeout", NOTIFY_TIMEOUT).AsUint (), (UINT)NOTIFY_TIMEOUT_MINIMUM);

		if ((_r_unixtime_now () - notifications_last[log.hash]) > notification_timeout)
		{
			_app_addapplication (app.GetHWND (), path, 0, false, false, true);

			_app_listviewsort (app.GetHWND (), IDC_LISTVIEW, -1, false);
			_app_profilesave (app.GetHWND ());
		}
	}

	if (!is_logenabled && !is_notificationenabled)
		return;

	// get username (only if log to file enabled)
	if (is_logenabled)
	{
		if ((flags & FWPM_NET_EVENT_FLAG_USER_ID_SET) != 0 && user_id)
		{
			SID_NAME_USE sid_type;

			WCHAR username[MAX_PATH] = {0};
			WCHAR domain[MAX_PATH] = {0};

			DWORD length1 = _countof (username);
			DWORD length2 = _countof (domain);

			if (LookupAccountSid (nullptr, user_id, username, &length1, domain, &length2, &sid_type) && length1 && length2)
				StringCchPrintf (log.username, _countof (log.username), L"%s\\%s", domain, username);
		}

		if (!log.username[0])
			StringCchCopy (log.username, _countof (log.username), NA_TEXT);
	}

	// read filter information
	if (is_logenabled || is_notificationenabled)
	{
		if (filter_id)
		{
			FWPM_FILTER* filter = nullptr;
			FWPM_PROVIDER* provider = nullptr;

			if (FwpmFilterGetById (config.hengine, filter_id, &filter) == ERROR_SUCCESS)
			{
				StringCchCopy (log.filter_name, _countof (log.filter_name), (filter->displayData.description ? filter->displayData.description : filter->displayData.name));

				if (filter->providerKey)
				{
					if (memcmp (filter->providerKey, &GUID_WfpProvider, sizeof (GUID)) == 0)
						is_myprovider = true;

					if (filter->weight.type == FWP_UINT8 && filter->weight.uint8 == FILTER_WEIGHT_BLOCKLIST)
						is_blocklist = true;

					if (FwpmProviderGetByKey (config.hengine, filter->providerKey, &provider) == ERROR_SUCCESS)
						StringCchCopy (log.provider_name, _countof (log.provider_name), (provider->displayData.description ? provider->displayData.description : provider->displayData.name));
				}
			}

			if (filter)
				FwpmFreeMemory ((LPVOID*)&filter);

			if (provider)
				FwpmFreeMemory ((LPVOID*)&provider);

			if (!log.filter_name[0])
				StringCchPrintf (log.filter_name, _countof (log.filter_name), L"#%d", filter_id);
		}

		if (!log.filter_name[0])
			StringCchCopy (log.filter_name, _countof (log.filter_name), NA_TEXT);
	}

	// protocol
	{
		if (proto && (flags & FWPM_NET_EVENT_FLAG_IP_PROTOCOL_SET) != 0)
		{
			for (size_t i = 0; i < protocols.size (); i++)
			{
				if (protocols.at (i).id == proto)
					StringCchCopy (log.protocol, _countof (log.protocol), protocols.at (i).name);
			}

			log.protocol8 = proto;
		}

		if (!log.protocol[0])
			StringCchCopy (log.protocol, _countof (log.protocol), NA_TEXT);
	}

	if (is_logenabled || is_notificationenabled)
	{
		// ipv4 address
		if (ipver == FWP_IP_VERSION_V4)
		{
			if (remoteaddr && (flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET) != 0)
			{
				StringCchPrintf (log.remote_addr, _countof (log.remote_addr), L"%d.%d.%d.%d",
					remoteaddr->byteArray16[3],
					remoteaddr->byteArray16[2],
					remoteaddr->byteArray16[1],
					remoteaddr->byteArray16[0]
				);

				if (remoteport && (flags & FWPM_NET_EVENT_FLAG_REMOTE_PORT_SET) != 0)
					log.remote_port = remoteport;
			}

			if (localaddr && (flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET) != 0)
			{
				StringCchPrintf (log.local_addr, _countof (log.local_addr), L"%d.%d.%d.%d",
					localaddr->byteArray16[3],
					localaddr->byteArray16[2],
					localaddr->byteArray16[1],
					localaddr->byteArray16[0]
				);

				if (localport && (flags & FWPM_NET_EVENT_FLAG_LOCAL_PORT_SET) != 0)
					log.local_port = localport;
			}
		}
		else if (ipver == FWP_IP_VERSION_V6)
		{
			if (remoteaddr && (flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET) != 0)
			{
				InetNtop (AF_INET6, remoteaddr->byteArray16, log.remote_addr, _countof (log.remote_addr));

				if (remoteport && (flags & FWPM_NET_EVENT_FLAG_REMOTE_PORT_SET) != 0)
					log.remote_port = remoteport;
			}

			if (localaddr && (flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET) != 0)
			{
				InetNtop (AF_INET6, localaddr->byteArray16, log.local_addr, _countof (log.local_addr));

				if (localport && (flags & FWPM_NET_EVENT_FLAG_LOCAL_PORT_SET) != 0)
					log.local_port = localport;
			}
		}
	}

	// store flags
	log.flags = flags;

	// indicates whether the packet originated from (or was heading to) the loopback adapter
	log.is_loopback = is_loopback ? true : false;

	// indicates the direction of the packet transmission
	log.direction = direction;

	// write log to the file
	if (is_logenabled)
		_app_logwrite (&log);

	// show notification only for whitelist mode
	if (app.ConfigGet (L"Mode", ModeWhitelist).AsUint () == ModeWhitelist)
	{
		// show notification (only for my own provider and file is present)
		if (is_notificationenabled && is_myprovider && log.hash && (flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET) != 0)
		{
			if (is_blocklist && app.ConfigGet (L"IsNotificationsExcludeBlocklist", false).AsBool ())
				return;

			ITEM_APPLICATION const *ptr_app = _app_getapplication (log.hash);

			if (ptr_app)
			{
				if (!ptr_app->is_network && !ptr_app->is_picoapp && ptr_app->real_path[0] != L'\\')
				{
					if (ptr_app->is_storeapp)
						log.hicon = CopyIcon (config.hicon_package);

					else
						_app_getfileicon (path, false, nullptr, &log.hicon);
				}

				_app_notifyadd (&log);
			}
		}
	}
}

// win7 callback
void CALLBACK _app_logcallback0 (LPVOID, const FWPM_NET_EVENT1 *pEvent)
{
	if (pEvent && pEvent->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP && pEvent->classifyDrop)
	{
		_app_logcallback (&pEvent->header.timeStamp, pEvent->header.appId.data, nullptr, pEvent->header.userId, pEvent->classifyDrop->filterId, pEvent->header.flags, pEvent->header.ipProtocol, pEvent->header.ipVersion, pEvent->classifyDrop->isLoopback, &pEvent->header.remoteAddrV6, pEvent->header.remotePort, &pEvent->header.localAddrV6, pEvent->header.localPort, pEvent->classifyDrop->msFwpDirection);
	}
}

// win8 callback
void CALLBACK _app_logcallback1 (LPVOID, const FWPM_NET_EVENT2 *pEvent)
{
	if (pEvent && pEvent->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP && pEvent->classifyDrop)
	{
		_app_logcallback (&pEvent->header.timeStamp, pEvent->header.appId.data, pEvent->header.packageSid, pEvent->header.userId, pEvent->classifyDrop->filterId, pEvent->header.flags, pEvent->header.ipProtocol, pEvent->header.ipVersion, pEvent->classifyDrop->isLoopback, &pEvent->header.remoteAddrV6, pEvent->header.remotePort, &pEvent->header.localAddrV6, pEvent->header.localPort, pEvent->classifyDrop->msFwpDirection);
	}
}

// win10 callback
void CALLBACK _app_logcallback2 (LPVOID, const FWPM_NET_EVENT3 *pEvent)
{
	if (pEvent && pEvent->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP && pEvent->classifyDrop)
	{
		_app_logcallback (&pEvent->header.timeStamp, pEvent->header.appId.data, pEvent->header.packageSid, pEvent->header.userId, pEvent->classifyDrop->filterId, pEvent->header.flags, pEvent->header.ipProtocol, pEvent->header.ipVersion, pEvent->classifyDrop->isLoopback, &pEvent->header.remoteAddrV6, pEvent->header.remotePort, &pEvent->header.localAddrV6, pEvent->header.localPort, pEvent->classifyDrop->msFwpDirection);
	}
}

UINT WINAPI ApplyThread (LPVOID lparam)
{
#ifndef _WIN64
	if (_r_sys_iswow64 ())
		Wow64EnableWow64FsRedirection (FALSE);
#endif

	if (WaitForSingleObjectEx (config.stop_evt, 1000, FALSE) == WAIT_OBJECT_0)
	{
		SetEvent (config.finish_evt);
		config.hthread = nullptr;

		return ERROR_SUCCESS;
	}

	_r_fastlock_acquireexclusive (&lock_apply);

	const bool is_install = lparam ? true : false;

	_r_ctrl_enable (app.GetHWND (), IDC_START_BTN, false);

	if (is_install)
	{
		if (_wfp_initialize (true))
			_wfp_installfilters ();

		_app_listviewsort (app.GetHWND (), IDC_LISTVIEW, -1, false);
		_app_profilesave (app.GetHWND ());

		_r_listview_redraw (app.GetHWND (), IDC_LISTVIEW);
	}
	else
	{
		if (_wfp_initialize (false))
			_wfp_destroyfilters (true);

		_wfp_uninitialize (true);
	}

	//_app_freednscache ();

	_r_fastlock_releaseexclusive (&lock_apply);

	_r_ctrl_enable (app.GetHWND (), IDC_START_BTN, true);

	SetEvent (config.finish_evt);

	config.hthread = nullptr;

	return ERROR_SUCCESS;
}

void addcolor (LPCWSTR locale_sid, UINT locale_id, LPCWSTR config_name, bool is_enabled, LPCWSTR config_value, COLORREF default_clr)
{
	ITEM_COLOR color;
	SecureZeroMemory (&color, sizeof (color));

	size_t length = 0;

	if (config_name)
	{
		length = wcslen (config_name) + 1;
		color.config_name = (LPWSTR)malloc (length * sizeof (WCHAR));

		if (color.config_name)
			StringCchCopy (color.config_name, length, config_name);
	}

	if (config_value)
	{
		length = wcslen (config_value) + 1;
		color.config_value = (LPWSTR)malloc (length * sizeof (WCHAR));

		if (color.config_value)
			StringCchCopy (color.config_value, length, config_value);
	}

	if (locale_sid)
	{
		length = wcslen (locale_sid) + 1;
		color.locale_sid = (LPWSTR)malloc (length * sizeof (WCHAR));

		if (color.locale_sid)
			StringCchCopy (color.locale_sid, length, locale_sid);
	}

	color.hash = _r_str_hash (config_value);
	color.locale_id = locale_id;
	color.default_clr = default_clr;
	color.is_enabled = is_enabled;
	color.clr = app.ConfigGet (config_value, default_clr).AsUlong ();
	color.hbr = CreateSolidBrush (color.clr);

	colors.push_back (color);
}

void addprotocol (LPCWSTR name, UINT8 id)
{
	ITEM_PROTOCOL protocol;
	SecureZeroMemory (&protocol, sizeof (protocol));

	if (name)
	{
		size_t length = wcslen (name) + 1;
		protocol.name = (LPWSTR)malloc (length * sizeof (WCHAR));

		if (protocol.name)
			StringCchCopy (protocol.name, length, name);
	}

	protocol.id = id;

	protocols.push_back (protocol);
}

void _app_getprocesslist (std::vector<ITEM_PROCESS>* pvc)
{
	if (!pvc)
		return;

	// clear previous result
	{
		for (size_t i = 0; i < pvc->size (); i++)
		{
			if (pvc->at (i).hbmp)
				DeleteObject (pvc->at (i).hbmp); // free memory
		}

		pvc->clear ();
	}

	pvc->clear (); // clear previous result

	NTSTATUS status = 0;

	ULONG length = 0x4000;
	PVOID buffer = malloc (length);

	while (true)
	{
		status = NtQuerySystemInformation (SystemProcessInformation, buffer, length, &length);

		if (status == 0xC0000023L /*STATUS_BUFFER_TOO_SMALL*/ || status == 0xc0000004 /*STATUS_INFO_LENGTH_MISMATCH*/)
		{
			PVOID buffer_new = realloc (buffer, length);

			if (!buffer_new)
			{
				break;
			}
			else
			{
				buffer = buffer_new;
			}
		}
		else
		{
			break;
		}
	}

	if (NT_SUCCESS (status))
	{
		PSYSTEM_PROCESS_INFORMATION spi = (PSYSTEM_PROCESS_INFORMATION)buffer;

		std::unordered_map<size_t, bool> checker;

		do
		{
			const DWORD pid = (DWORD)(DWORD_PTR)spi->UniqueProcessId;

			if (!pid) // skip "system idle process"
				continue;

			const HANDLE hprocess = OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);

			if (hprocess)
			{
				WCHAR display_path[64] = {0};
				WCHAR real_path[MAX_PATH] = {0};

				size_t hash = 0;

				StringCchPrintf (display_path, _countof (display_path), L"%s (%d)", spi->ImageName.Buffer, pid);

				if (pid == PROC_SYSTEM_PID)
				{
					StringCchCopy (real_path, _countof (real_path), _r_path_expand (PATH_NTOSKRNL));
					hash = _r_str_hash (spi->ImageName.Buffer);
				}
				else
				{
					DWORD size = _countof (real_path) - 1;

					if (QueryFullProcessImageName (hprocess, 0, real_path, &size))
					{
						hash = _r_str_hash (real_path);
					}
					else
					{
						// cannot get file path because it's not filesystem process (Pico maybe?)
						if (GetLastError () == ERROR_GEN_FAILURE)
						{
							StringCchCopy (real_path, _countof (real_path), spi->ImageName.Buffer);
							hash = _r_str_hash (spi->ImageName.Buffer);
						}
						else
						{
							CloseHandle (hprocess);
							continue;
						}
					}
				}

				if (hash && apps.find (hash) == apps.end () && checker.find (hash) == checker.end ())
				{
					checker[hash] = true;

					ITEM_PROCESS item;
					SecureZeroMemory (&item, sizeof (item));

					StringCchCopy (item.display_path, _countof (item.display_path), display_path);
					StringCchCopy (item.real_path, _countof (item.real_path), ((pid == PROC_SYSTEM_PID) ? PROC_SYSTEM_NAME : real_path));

					// get file icon
					{
						HICON hicon = nullptr;

						if (_app_getfileicon (real_path, true, nullptr, &hicon))
						{
							item.hbmp = _app_ico2bmp (hicon);
							DestroyIcon (hicon);
						}
						else
						{
							item.hbmp = _app_ico2bmp (config.hicon_small);
						}
					}

					pvc->push_back (item);
				}

				CloseHandle (hprocess);
			}
		}
		while ((spi = ((spi->NextEntryOffset ? (PSYSTEM_PROCESS_INFORMATION)((PCHAR)(spi)+(spi)->NextEntryOffset) : nullptr))) != nullptr);
	}

	free (buffer); // free the allocated buffer
}

bool _app_installmessage (HWND hwnd, bool is_install)
{
	WCHAR main[256] = {0};

	WCHAR mode[128] = {0};
	WCHAR mode1[128] = {0};
	WCHAR mode2[128] = {0};

	WCHAR flag[64] = {0};

	INT result = 0;
	BOOL is_flagchecked = FALSE;
	INT radio_checked = 0;

	TASKDIALOGCONFIG tdc = {0};
	TASKDIALOG_BUTTON tdr[2] = {0};

	tdc.cbSize = sizeof (tdc);
	tdc.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
	tdc.hwndParent = hwnd;
	tdc.pszWindowTitle = APP_NAME;
	tdc.pfCallback = &_r_msg_callback;
	tdc.pszMainIcon = TD_WARNING_ICON;
	tdc.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
	tdc.pszMainInstruction = main;
	tdc.pszVerificationText = flag;

	if (is_install)
	{
		StringCchCopy (main, _countof (main), I18N (&app, IDS_QUESTION_START, 0));
		StringCchCopy (flag, _countof (flag), I18N (&app, IDS_DISABLEWINDOWSFIREWALL_CHK, 0));

		tdc.pszContent = mode;
		tdc.pRadioButtons = tdr;
		tdc.cRadioButtons = _countof (tdr);
		tdc.nDefaultRadioButton = IDM_TRAY_MODEWHITELIST + app.ConfigGet (L"Mode", ModeWhitelist).AsUint ();

		tdr[0].nButtonID = IDM_TRAY_MODEWHITELIST;
		tdr[0].pszButtonText = mode1;

		tdr[1].nButtonID = IDM_TRAY_MODEBLACKLIST;
		tdr[1].pszButtonText = mode2;

		StringCchPrintf (mode, _countof (mode), L"%s:", I18N (&app, IDS_TRAY_MODE, 0));
		StringCchCopy (mode1, _countof (mode1), I18N (&app, IDS_MODE_WHITELIST, 0));
		StringCchCopy (mode2, _countof (mode2), I18N (&app, IDS_MODE_BLACKLIST, 0));

		if (app.ConfigGet (L"IsDisableWindowsFirewallChecked", true).AsBool ())
			tdc.dwFlags |= TDF_VERIFICATION_FLAG_CHECKED;
	}
	else
	{
		StringCchCopy (main, _countof (main), I18N (&app, IDS_QUESTION_STOP, 0));
		StringCchCopy (flag, _countof (flag), I18N (&app, IDS_ENABLEWINDOWSFIREWALL_CHK, 0));

		if (app.ConfigGet (L"IsEnableWindowsFirewallChecked", true).AsBool ())
			tdc.dwFlags |= TDF_VERIFICATION_FLAG_CHECKED;
	}

	if (_r_msg_taskdialog (&tdc, &result, &radio_checked, &is_flagchecked))
	{
		if (result == IDYES)
		{
			if (is_install)
			{
				app.ConfigSet (L"IsDisableWindowsFirewallChecked", is_flagchecked ? true : false);

				app.ConfigSet (L"Mode", DWORD ((radio_checked == IDM_TRAY_MODEWHITELIST) ? ModeWhitelist : ModeBlacklist));
				CheckMenuRadioItem (GetMenu (hwnd), IDM_TRAY_MODEWHITELIST, IDM_TRAY_MODEBLACKLIST, IDM_TRAY_MODEWHITELIST + app.ConfigGet (L"Mode", ModeWhitelist).AsUint (), MF_BYCOMMAND);

				if (is_flagchecked)
					_mps_changeconfig (true);
			}
			else
			{
				app.ConfigSet (L"IsEnableWindowsFirewallChecked", is_flagchecked ? true : false);

				if (is_flagchecked)
					_mps_changeconfig (false);
			}

			return true;
		}
	}

	return false;
}

BOOL initializer_callback (HWND hwnd, DWORD msg, LPVOID, LPVOID)
{
	switch (msg)
	{
		case _RM_ARGUMENTS:
		{
			if (wcsstr (GetCommandLine (), L"/uninstall"))
			{
				const bool is_enabled = _wfp_isfiltersinstalled ();

				if (is_enabled)
				{
					if (_app_installmessage (hwnd, false))
					{
						if (_wfp_initialize (false))
							_wfp_destroyfilters (true);

						_wfp_uninitialize (true);
					}
				}

				return TRUE;
			}

			break;
		}

		case _RM_INITIALIZE:
		{
			if (app.ConfigGet (L"ShowTitleID", true).AsBool ())
				SetWindowText (hwnd, config.title);
			else
				SetWindowText (hwnd, APP_NAME);

			const bool state = _wfp_isfiltersinstalled ();

			// set icons
			app.SetIcon (state ? IDI_ACTIVE : IDI_INACTIVE);
			app.TrayCreate (hwnd, UID, WM_TRAYICON, _r_loadicon (app.GetHINSTANCE (), MAKEINTRESOURCE (state ? IDI_ACTIVE : IDI_INACTIVE), GetSystemMetrics (SM_CXSMICON)), false);
			SetDlgItemText (hwnd, IDC_START_BTN, I18N (&app, (state ? IDS_TRAY_STOP : IDS_TRAY_START), state ? L"IDS_TRAY_STOP" : L"IDS_TRAY_START"));

			CheckMenuItem (GetMenu (hwnd), IDM_ALWAYSONTOP_CHK, MF_BYCOMMAND | (app.ConfigGet (L"AlwaysOnTop", false).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (GetMenu (hwnd), IDM_SHOWFILENAMESONLY_CHK, MF_BYCOMMAND | (app.ConfigGet (L"ShowFilenames", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (GetMenu (hwnd), IDM_AUTOSIZECOLUMNS_CHK, MF_BYCOMMAND | (app.ConfigGet (L"AutoSizeColumns", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));

			CheckMenuRadioItem (GetMenu (hwnd), IDM_TRAY_MODEWHITELIST, IDM_TRAY_MODEBLACKLIST, IDM_TRAY_MODEWHITELIST + app.ConfigGet (L"Mode", ModeWhitelist).AsUint (), MF_BYCOMMAND);

			CheckMenuItem (GetMenu (hwnd), IDM_ENABLELOG_CHK, MF_BYCOMMAND | (app.ConfigGet (L"IsLogEnabled", false).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (GetMenu (hwnd), IDM_ENABLENOTIFICATIONS_CHK, MF_BYCOMMAND | (app.ConfigGet (L"IsNotificationsEnabled", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));

			// dropped packets logging (win7 and above)
			if (!_r_sys_validversion (6, 1))
			{
				EnableMenuItem (GetMenu (hwnd), IDM_ENABLELOG_CHK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
				EnableMenuItem (GetMenu (hwnd), IDM_ENABLENOTIFICATIONS_CHK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
			}

			break;
		}

		case _RM_LOCALIZE:
		{
			const HMENU menu = GetMenu (hwnd);

			app.LocaleMenu (menu, I18N (&app, IDS_FILE, 0), 0, true, nullptr);
			app.LocaleMenu (menu, I18N (&app, IDS_ADD_FILE, 0), IDM_ADD_FILE, false, nullptr);
			app.LocaleMenu (GetSubMenu (menu, 0), I18N (&app, IDS_EXPORT, 0), 2, true, nullptr);
			app.LocaleMenu (GetSubMenu (menu, 0), I18N (&app, IDS_IMPORT, 0), 3, true, nullptr);
			app.LocaleMenu (menu, _r_fmt (I18N (&app, IDS_EXPORT_APPS, 0), XML_APPS), IDM_EXPORT_APPS, false, nullptr);
			app.LocaleMenu (menu, _r_fmt (I18N (&app, IDS_EXPORT_RULES, 0), XML_RULES_CUSTOM), IDM_EXPORT_RULES, false, nullptr);
			app.LocaleMenu (menu, _r_fmt (I18N (&app, IDS_IMPORT_APPS, 0), XML_APPS), IDM_IMPORT_APPS, false, nullptr);
			app.LocaleMenu (menu, _r_fmt (I18N (&app, IDS_IMPORT_RULES, 0), XML_RULES_CUSTOM), IDM_IMPORT_RULES, false, nullptr);
			app.LocaleMenu (menu, I18N (&app, IDS_EXIT, 0), IDM_EXIT, false, nullptr);

			app.LocaleMenu (menu, I18N (&app, IDS_EDIT, 0), 1, true, nullptr);

			app.LocaleMenu (menu, I18N (&app, IDS_PURGEN, 0), IDM_PURGEN, false, L"\tCtrl+Del");

			app.LocaleMenu (menu, I18N (&app, IDS_FIND, 0), IDM_FIND, false, L"\tCtrl+F");
			app.LocaleMenu (menu, I18N (&app, IDS_FINDNEXT, 0), IDM_FINDNEXT, false, L"\tF3");

			app.LocaleMenu (menu, I18N (&app, IDS_REFRESH, 0), IDM_REFRESH, false, L"\tF5");

			app.LocaleMenu (menu, I18N (&app, IDS_VIEW, 0), 2, true, nullptr);

			app.LocaleMenu (menu, I18N (&app, IDS_ALWAYSONTOP_CHK, 0), IDM_ALWAYSONTOP_CHK, false, nullptr);
			app.LocaleMenu (menu, I18N (&app, IDS_SHOWFILENAMESONLY_CHK, 0), IDM_SHOWFILENAMESONLY_CHK, false, nullptr);
			app.LocaleMenu (menu, I18N (&app, IDS_AUTOSIZECOLUMNS_CHK, 0), IDM_AUTOSIZECOLUMNS_CHK, false, nullptr);

			app.LocaleMenu (GetSubMenu (menu, 2), I18N (&app, IDS_ICONS, 0), 4, true, nullptr);
			app.LocaleMenu (menu, I18N (&app, IDS_ICONSSMALL, 0), IDM_ICONSSMALL, false, nullptr);
			app.LocaleMenu (menu, I18N (&app, IDS_ICONSLARGE, 0), IDM_ICONSLARGE, false, nullptr);
			app.LocaleMenu (menu, I18N (&app, IDS_ICONSISHIDDEN, 0), IDM_ICONSISHIDDEN, false, nullptr);

			app.LocaleMenu (GetSubMenu (menu, 2), I18N (&app, IDS_LANGUAGE, 0), LANG_MENU, true, nullptr);

			app.LocaleMenu (menu, I18N (&app, IDS_FONT, 0), IDM_FONT, false, nullptr);

			app.LocaleMenu (menu, I18N (&app, IDS_SETTINGS, 0), 3, true, nullptr);

			app.LocaleMenu (GetSubMenu (menu, 3), I18N (&app, IDS_TRAY_MODE, 0), 0, true, nullptr);

			app.LocaleMenu (menu, I18N (&app, IDS_MODE_WHITELIST, 0), IDM_TRAY_MODEWHITELIST, false, nullptr);
			app.LocaleMenu (menu, I18N (&app, IDS_MODE_BLACKLIST, 0), IDM_TRAY_MODEBLACKLIST, false, nullptr);

			app.LocaleMenu (GetSubMenu (menu, 3), I18N (&app, IDS_TRAY_LOG, 0), 1, true, nullptr);

			app.LocaleMenu (menu, I18N (&app, IDS_ENABLELOG_CHK, 0), IDM_ENABLELOG_CHK, false, nullptr);
			app.LocaleMenu (menu, I18N (&app, IDS_ENABLENOTIFICATIONS_CHK, 0), IDM_ENABLENOTIFICATIONS_CHK, false, nullptr);
			app.LocaleMenu (menu, I18N (&app, IDS_LOGSHOW, 0), IDM_LOGSHOW, false, L"\tCtrl+I");
			app.LocaleMenu (menu, I18N (&app, IDS_LOGCLEAR, 0), IDM_LOGCLEAR, false, L"\tCtrl+X");

			app.LocaleMenu (menu, I18N (&app, IDS_SETTINGS, 0), IDM_SETTINGS, false, L"...\tCtrl+P");

			app.LocaleMenu (menu, I18N (&app, IDS_HELP, 0), 4, true, nullptr);
			app.LocaleMenu (menu, I18N (&app, IDS_WEBSITE, 0), IDM_WEBSITE, false, nullptr);
			app.LocaleMenu (menu, I18N (&app, IDS_DONATE, 0), IDM_DONATE, false, nullptr);
			app.LocaleMenu (menu, I18N (&app, IDS_CHECKUPDATES, 0), IDM_CHECKUPDATES, false, nullptr);
			app.LocaleMenu (menu, I18N (&app, IDS_ABOUT, 0), IDM_ABOUT, false, nullptr);

			app.LocaleEnum ((HWND)GetSubMenu (menu, 2), LANG_MENU, true, IDM_LANGUAGE); // enum localizations

			{
				const bool state = _wfp_isfiltersinstalled ();
				SetDlgItemText (hwnd, IDC_START_BTN, I18N (&app, (state ? IDS_TRAY_STOP : IDS_TRAY_START), state ? L"IDS_TRAY_STOP" : L"IDS_TRAY_START"));
			}

			SetDlgItemText (hwnd, IDC_SETTINGS_BTN, I18N (&app, IDS_SETTINGS, 0));
			SetDlgItemText (hwnd, IDC_EXIT_BTN, I18N (&app, IDS_EXIT, 0));

			_r_wnd_addstyle (hwnd, IDC_START_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (hwnd, IDC_SETTINGS_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (hwnd, IDC_EXIT_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

			_r_wnd_addstyle (config.hnotification, IDC_ALLOW_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (config.hnotification, IDC_BLOCK_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (config.hnotification, IDC_NEXT_ID, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (config.hnotification, IDC_PREV_ID, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

			_app_refreshstatus (hwnd, true, true);

			_r_listview_setcolumn (hwnd, IDC_LISTVIEW, 0, I18N (&app, IDS_FILEPATH, 0), 0);
			_r_listview_setcolumn (hwnd, IDC_LISTVIEW, 1, I18N (&app, IDS_ADDED, 0), 0);

			SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_RESETEMPTYTEXT, 0, 0);

			_app_notifyrefresh ();

			break;
		}

		case _RM_UNINITIALIZE:
		{
			app.TrayDestroy (UID);
			break;
		}
	}

	return FALSE;
}

LONG _app_wmcustdraw (LPNMLVCUSTOMDRAW lpnmlv, LPARAM lparam)
{
	LONG result = CDRF_DODEFAULT;

	if (!app.ConfigGet (L"UseHighlighting", true).AsBool ())
		return result;

	switch (lpnmlv->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
		{
			result = CDRF_NOTIFYITEMDRAW;
			break;
		}

		case CDDS_ITEMPREPAINT:
		{
			if (
				lpnmlv->nmcd.hdr.idFrom == IDC_LISTVIEW ||
				lpnmlv->nmcd.hdr.idFrom == IDC_FILES_LV
				)
			{
				const size_t hash = lpnmlv->nmcd.lItemlParam;

				if (hash)
				{
					const COLORREF new_clr = (COLORREF)_app_getcolor (hash, false);

					if (new_clr)
					{
						lpnmlv->clrTextBk = new_clr;
						lpnmlv->clrText = (_r_dc_getcolorbrightness (lpnmlv->clrTextBk) > 100) ? RGB (0x00, 0x00, 0x00) : RGB (0xff, 0xff, 0xff);

						_r_dc_fillrect (lpnmlv->nmcd.hdc, &lpnmlv->nmcd.rc, new_clr);

						result = CDRF_NEWFONT;
					}
				}
			}
			else if (lpnmlv->nmcd.hdr.idFrom == IDC_COLORS)
			{
				ITEM_COLOR const *ptr_clr = &colors.at (lpnmlv->nmcd.lItemlParam);

				if (ptr_clr)
				{
					lpnmlv->clrTextBk = ptr_clr->clr;
					lpnmlv->clrText = (_r_dc_getcolorbrightness (lpnmlv->clrTextBk) > 100) ? RGB (0x00, 0x00, 0x00) : RGB (0xff, 0xff, 0xff);

					_r_dc_fillrect (lpnmlv->nmcd.hdc, &lpnmlv->nmcd.rc, lpnmlv->clrTextBk);

					result = CDRF_NEWFONT;
				}
			}
			else if (lpnmlv->nmcd.hdr.idFrom == IDC_EDITOR)
			{
				COLORREF const custclr = (COLORREF)lparam;

				if (custclr)
				{
					lpnmlv->clrTextBk = custclr;
					lpnmlv->clrText = (_r_dc_getcolorbrightness (custclr) > 100) ? RGB (0, 0, 0) : RGB (255, 255, 255);

					_r_dc_fillrect (lpnmlv->nmcd.hdc, &lpnmlv->nmcd.rc, custclr);
				}
			}

			break;
		}
	}

	return result;
}

INT_PTR CALLBACK EditorProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static ITEM_RULE *ptr_rule = nullptr;
	static size_t idx = LAST_VALUE;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			ptr_rule = (ITEM_RULE*)lparam;
			idx = GetWindowLongPtr (app.GetHWND (), GWLP_USERDATA);

			// configure window
			_r_wnd_center (hwnd);

			SendMessage (hwnd, WM_SETICON, ICON_SMALL, (LPARAM)app.GetHICON (false));
			SendMessage (hwnd, WM_SETICON, ICON_BIG, (LPARAM)app.GetHICON (true));

			// localize window
			SetWindowText (hwnd, (ptr_rule && ptr_rule->pname ? _r_fmt (L"%s - \"%s\"", I18N (&app, IDS_EDITOR, 0), ptr_rule->pname) : I18N (&app, IDS_EDITOR, 0)));

			_r_ctrl_settext (hwnd, IDC_NAME, L"%s:", I18N (&app, IDS_NAME, 0));
			_r_ctrl_settext (hwnd, IDC_RULES, L"%s:", I18N (&app, IDS_RULE, 0));
			_r_ctrl_settext (hwnd, IDC_FILES, L"%s:", I18N (&app, IDS_APPLYTO, 0));
			_r_ctrl_settext (hwnd, IDC_DIRECTION, L"%s:", I18N (&app, IDS_DIRECTION, 0));
			_r_ctrl_settext (hwnd, IDC_PROTOCOL, L"%s:", I18N (&app, IDS_PROTOCOL, 0));
			_r_ctrl_settext (hwnd, IDC_PORTVERSION, L"%s:", I18N (&app, IDS_PORTVERSION, 0));
			_r_ctrl_settext (hwnd, IDC_ACTION, L"%s:", I18N (&app, IDS_ACTION, 0));

			_r_ctrl_settext (hwnd, IDC_RULES_WIKI, I18N (&app, IDS_RULES_WIKI, 0), WIKI_URL);
			SetDlgItemText (hwnd, IDC_ENABLED_CHK, I18N (&app, IDS_ENABLERULE_CHK, 0));

			SetDlgItemText (hwnd, IDC_SAVE, I18N (&app, IDS_SAVE, 0));
			SetDlgItemText (hwnd, IDC_CLOSE, I18N (&app, IDS_CLOSE, 0));

			// configure listview
			_r_listview_setstyle (hwnd, IDC_FILES_LV, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_CHECKBOXES);

			_r_listview_addcolumn (hwnd, IDC_FILES_LV, 0, nullptr, 95, LVCFMT_LEFT);

			_app_listviewsetimagelist (hwnd, IDC_FILES_LV);
			_app_listviewsetfont (hwnd, IDC_FILES_LV);

			// name
			if (ptr_rule && ptr_rule->pname)
				SetDlgItemText (hwnd, IDC_NAME_EDIT, ptr_rule->pname);

			// rule
			if (ptr_rule && ptr_rule->prule)
				SetDlgItemText (hwnd, IDC_RULES_EDIT, ptr_rule->prule);

			// apps (apply to)
			{
				size_t item = 0;

				_r_fastlock_acquireshared (&lock_access);

				for (auto const& p : apps)
				{
					ITEM_APPLICATION const *ptr_app = &p.second;

					if (ptr_app)
					{
						// windows store apps (win8 and above)
						if (ptr_app->is_storeapp && !_r_sys_validversion (6, 2))
							continue;

						config.is_nocheckboxnotify = true;

						_r_listview_additem (hwnd, IDC_FILES_LV, item, 0, _r_path_extractfile (ptr_app->display_name), ptr_app->icon_id, LAST_VALUE, p.first);

						if (idx != LAST_VALUE)
						{
							for (size_t i = 0; i < ptr_app->rules.size (); i++)
							{
								if (ptr_app->rules.at (i) == idx)
								{
									_r_listview_setitemcheck (hwnd, IDC_FILES_LV, item, true);
									break;
								}
							}
						}

						config.is_nocheckboxnotify = false;

						item += 1;
					}
				}

				_r_fastlock_releaseshared (&lock_access);

				// sort column
				_app_listviewsort_appsrules (hwnd, IDC_FILES_LV);

				// resize column
				RECT rc = {0};
				GetClientRect (GetDlgItem (hwnd, IDC_FILES_LV), &rc);

				_r_listview_setcolumn (hwnd, IDC_FILES_LV, 0, nullptr, (rc.right - rc.left));
			}

			// direction
			SendDlgItemMessage (hwnd, IDC_DIRECTION_EDIT, CB_INSERTSTRING, 0, (LPARAM)I18N (&app, IDS_DIRECTION_1, 0));
			SendDlgItemMessage (hwnd, IDC_DIRECTION_EDIT, CB_INSERTSTRING, 1, (LPARAM)I18N (&app, IDS_DIRECTION_2, 0));
			SendDlgItemMessage (hwnd, IDC_DIRECTION_EDIT, CB_INSERTSTRING, 2, (LPARAM)I18N (&app, IDS_DIRECTION_3, 0));

			if (ptr_rule)
				SendDlgItemMessage (hwnd, IDC_DIRECTION_EDIT, CB_SETCURSEL, (WPARAM)ptr_rule->dir, 0);

			// protocol
			SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_INSERTSTRING, 0, (LPARAM)I18N (&app, IDS_ALL, 0));
			SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_SETCURSEL, 0, 0);

			SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_SETEXTENDEDUI, 1, 0);

			for (size_t i = 0; i < protocols.size (); i++)
			{
				SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_INSERTSTRING, i + 1, (LPARAM)protocols.at (i).name);
				SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_SETITEMDATA, i + 1, (LPARAM)protocols.at (i).id);

				if (ptr_rule && ptr_rule->protocol == protocols.at (i).id)
					SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_SETCURSEL, (WPARAM)i + 1, 0);
			}

			// family (ports-only)
			SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_INSERTSTRING, 0, (LPARAM)I18N (&app, IDS_ALL, 0));
			SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_SETITEMDATA, 0, (LPARAM)AF_UNSPEC);

			SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_INSERTSTRING, 1, (LPARAM)L"IPv4");
			SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_SETITEMDATA, 1, (LPARAM)AF_INET);

			SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_INSERTSTRING, 2, (LPARAM)L"IPv6");
			SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_SETITEMDATA, 2, (LPARAM)AF_INET6);

			if (ptr_rule)
			{
				if (ptr_rule->version == AF_UNSPEC)
					SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_SETCURSEL, (WPARAM)0, 0);

				else if (ptr_rule->version == AF_INET)
					SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_SETCURSEL, (WPARAM)1, 0);

				else if (ptr_rule->version == AF_INET6)
					SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_SETCURSEL, (WPARAM)2, 0);
			}

			// action
			SendDlgItemMessage (hwnd, IDC_ACTION_EDIT, CB_INSERTSTRING, 0, (LPARAM)I18N (&app, IDS_ACTION_1, 0));
			SendDlgItemMessage (hwnd, IDC_ACTION_EDIT, CB_INSERTSTRING, 1, (LPARAM)I18N (&app, IDS_ACTION_2, 0));

			if (ptr_rule)
				SendDlgItemMessage (hwnd, IDC_ACTION_EDIT, CB_SETCURSEL, (WPARAM)ptr_rule->is_block, 0);

			// state
			CheckDlgButton (hwnd, IDC_ENABLED_CHK, ptr_rule && ptr_rule->is_enabled ? BST_CHECKED : BST_UNCHECKED);

			// set limitation
			SendDlgItemMessage (hwnd, IDC_NAME_EDIT, EM_LIMITTEXT, RULE_NAME_CCH_MAX - 1, 0);
			SendDlgItemMessage (hwnd, IDC_RULES_EDIT, EM_LIMITTEXT, RULE_RULE_CCH_MAX - 1, 0);

			_r_wnd_addstyle (hwnd, IDC_SAVE, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (hwnd, IDC_CLOSE, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

			_r_ctrl_enable (hwnd, IDC_SAVE, (SendDlgItemMessage (hwnd, IDC_NAME_EDIT, WM_GETTEXTLENGTH, 0, 0) > 0) && (SendDlgItemMessage (hwnd, IDC_RULES_EDIT, WM_GETTEXTLENGTH, 0, 0) > 0)); // enable apply button

			break;
		}

		case WM_CTLCOLORDLG:
		{
			return (INT_PTR)GetSysColorBrush (COLOR_WINDOW);
		}

		case WM_CTLCOLORBTN:
		case WM_CTLCOLORSTATIC:
		{
			const UINT ctrl_id = GetDlgCtrlID ((HWND)lparam);

			SetBkMode ((HDC)wparam, TRANSPARENT); // background-hack

			return (INT_PTR)GetSysColorBrush ((ctrl_id == IDC_ENABLED_CHK || ctrl_id == IDC_SAVE || ctrl_id == IDC_CLOSE) ? COLOR_BTNFACE : COLOR_WINDOW);
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps = {0};
			HDC dc = BeginPaint (hwnd, &ps);

			RECT rc = {0};
			GetClientRect (hwnd, &rc);

			static INT bottom = app.GetDPI (44);

			rc.top = rc.bottom - bottom;
			rc.bottom = rc.top + bottom;

			_r_dc_fillrect (dc, &rc, GetSysColor (COLOR_BTNFACE));

			for (INT i = 0; i < rc.right; i++)
				SetPixel (dc, i, rc.top, GetSysColor (COLOR_APPWORKSPACE));

			EndPaint (hwnd, &ps);

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lparam;

			switch (nmlp->code)
			{
				case NM_CLICK:
				case NM_RETURN:
				{
					if (nmlp->idFrom == IDC_RULES_WIKI)
					{
						PNMLINK nmlink = (PNMLINK)lparam;

						if (nmlink->item.szUrl)
							ShellExecute (hwnd, nullptr, nmlink->item.szUrl, nullptr, nullptr, SW_SHOWDEFAULT);
					}

					break;
				}

				case NM_CUSTOMDRAW:
				{
					if (nmlp->idFrom != IDC_FILES_LV)
						break;

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, _app_wmcustdraw ((LPNMLVCUSTOMDRAW)lparam, 0));
					return TRUE;
				}

				case LVN_ITEMCHANGED:
				{
					LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lparam;

					if (lpnmlv->uNewState == 8192 || lpnmlv->uNewState == 4096)
					{
						if (config.is_nocheckboxnotify)
							return FALSE;

						_app_listviewsort_appsrules (hwnd, IDC_FILES_LV);
					}

					break;
				}

				case LVN_GETINFOTIP:
				{
					LPNMLVGETINFOTIP lpnmlv = (LPNMLVGETINFOTIP)lparam;

					const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, (UINT)lpnmlv->hdr.idFrom, lpnmlv->iItem);

					if (hash)
						StringCchCopy (lpnmlv->pszText, lpnmlv->cchTextMax, _app_gettooltip (hash));

					break;
				}

				case LVN_GETEMPTYMARKUP:
				{
					NMLVEMPTYMARKUP* lpnmlv = (NMLVEMPTYMARKUP*)lparam;

					lpnmlv->dwFlags = EMF_CENTERED;
					StringCchCopy (lpnmlv->szMarkup, _countof (lpnmlv->szMarkup), I18N (&app, IDS_STATUS_EMPTY, 0));

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
					return TRUE;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			if (HIWORD (wparam) == EN_CHANGE)
			{
				_r_ctrl_enable (hwnd, IDC_SAVE, (SendDlgItemMessage (hwnd, IDC_NAME_EDIT, WM_GETTEXTLENGTH, 0, 0) > 0) && (SendDlgItemMessage (hwnd, IDC_RULES_EDIT, WM_GETTEXTLENGTH, 0, 0) > 0)); // enable apply button
				return FALSE;
			}

			switch (LOWORD (wparam))
			{
				case IDOK: // process Enter key
				case IDC_SAVE:
				{
					if (!SendDlgItemMessage (hwnd, IDC_NAME_EDIT, WM_GETTEXTLENGTH, 0, 0) || !SendDlgItemMessage (hwnd, IDC_RULES_EDIT, WM_GETTEXTLENGTH, 0, 0))
						return FALSE;

					// rule destination
					{
						rstring rule = _r_ctrl_gettext (hwnd, IDC_RULES_EDIT).Trim (L"\r\n " RULE_DELIMETER);

						const size_t rule_length = (std::min) (rule.GetLength (), (size_t)RULE_RULE_CCH_MAX) + 1;
						const size_t rule_size = (rule_length + 1) * sizeof (WCHAR);

						// here we parse and check rule syntax
						{
							rstring::rvector arr = rule.AsVector (RULE_DELIMETER);

							for (size_t i = 0; i < arr.size (); i++)
							{
								rstring rule_single = arr.at (i).Trim (L"\r\n ");

								if (rule_single.IsEmpty () || rule_single.At (0) == L'*')
									continue;

								if (!_app_parserulestring (rule_single, nullptr, nullptr))
								{
									_r_ctrl_showtip (hwnd, IDC_RULES_EDIT, TTI_ERROR, APP_NAME, _r_fmt (I18N (&app, IDS_STATUS_SYNTAX_ERROR, 0), rule_single.GetString ()));
									_r_ctrl_enable (hwnd, IDC_SAVE, false);

									return FALSE;
								}
							}
						}

						_r_fastlock_acquireexclusive (&lock_access);

						// save rule destination
						if (ptr_rule->prule)
						{
							free (ptr_rule->prule);
							ptr_rule->prule = nullptr;
						}

						ptr_rule->prule = (LPWSTR)malloc (rule_size);

						if (ptr_rule->prule)
							StringCchCopy (ptr_rule->prule, rule_length, rule);
					}

					// save rule name
					{
						rstring name = _r_ctrl_gettext (hwnd, IDC_NAME_EDIT).Trim (L"\r\n " RULE_DELIMETER);

						if (!name.IsEmpty ())
						{
							const size_t name_length = (std::min) (name.GetLength (), (size_t)RULE_NAME_CCH_MAX) + 1;
							const size_t new_sizeb = (name_length + 1) * sizeof (WCHAR);

							if (ptr_rule->pname)
							{
								free (ptr_rule->pname);
								ptr_rule->pname = nullptr;
							}

							ptr_rule->pname = (LPWSTR)malloc (new_sizeb);

							if (ptr_rule->pname)
								StringCchCopy (ptr_rule->pname, name_length, name);
						}
					}

					// rule apps
					for (size_t i = 0; i < _r_listview_getitemcount (hwnd, IDC_FILES_LV); i++)
					{
						const size_t hash = _r_listview_getitemlparam (hwnd, IDC_FILES_LV, i);
						ITEM_APPLICATION *ptr_app = _app_getapplication (hash);

						if (ptr_app)
						{
							const bool is_apply = _r_listview_isitemchecked (hwnd, IDC_FILES_LV, i);

							if (!is_apply)
							{
								if (idx != LAST_VALUE)
								{
									for (size_t j = ptr_app->rules.size () - 1; j != LAST_VALUE; j--)
									{
										if (ptr_app->rules.at (j) == idx)
											ptr_app->rules.erase (ptr_app->rules.begin () + j);
									}
								}
							}
							else
							{
								ptr_app->rules.push_back ((idx == LAST_VALUE) ? rules_custom.size () : idx);
							}
						}
					}

					ptr_rule->protocol = (UINT8)SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_GETITEMDATA, SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_GETCURSEL, 0, 0), 0);
					ptr_rule->version = (ADDRESS_FAMILY)SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_GETITEMDATA, SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_GETCURSEL, 0, 0), 0);

					ptr_rule->dir = (EnumRuleDirection)SendDlgItemMessage (hwnd, IDC_DIRECTION_EDIT, CB_GETCURSEL, 0, 0);
					ptr_rule->is_block = SendDlgItemMessage (hwnd, IDC_ACTION_EDIT, CB_GETCURSEL, 0, 0) ? true : false;
					ptr_rule->is_enabled = (IsDlgButtonChecked (hwnd, IDC_ENABLED_CHK) == BST_CHECKED) ? true : false;

					_r_fastlock_releaseexclusive (&lock_access);

					EndDialog (hwnd, 1);

					break;
				}

				case IDCANCEL: // process Esc key
				case IDC_CLOSE:
				{
					EndDialog (hwnd, 0);
					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

BOOL settings_callback (HWND hwnd, DWORD msg, LPVOID lpdata1, LPVOID lpdata2)
{
	PAPP_SETTINGS_PAGE const page = (PAPP_SETTINGS_PAGE)lpdata2;

	switch (msg)
	{
		case _RM_INITIALIZE:
		{
			switch (page->dlg_id)
			{
				case IDD_SETTINGS_GENERAL:
				{
					CheckDlgButton (hwnd, IDC_ALWAYSONTOP_CHK, app.ConfigGet (L"AlwaysOnTop", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

#ifdef _APP_HAVE_AUTORUN
					CheckDlgButton (hwnd, IDC_LOADONSTARTUP_CHK, app.AutorunIsEnabled () ? BST_CHECKED : BST_UNCHECKED);
#endif // _APP_HAVE_AUTORUN

#ifdef _APP_HAVE_SKIPUAC
					CheckDlgButton (hwnd, IDC_SKIPUACWARNING_CHK, app.SkipUacIsEnabled () ? BST_CHECKED : BST_UNCHECKED);
#endif // _APP_HAVE_SKIPUAC

					CheckDlgButton (hwnd, IDC_CHECKUPDATES_CHK, app.ConfigGet (L"CheckUpdates", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					app.LocaleEnum (hwnd, IDC_LANGUAGE, false, 0);

					break;
				}

				case IDD_SETTINGS_FILTERS:
				{
					CheckDlgButton (hwnd, IDC_RULE_ALLOWINBOUND, app.ConfigGet (L"AllowInboundConnections", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_RULE_ALLOWLISTEN, app.ConfigGet (L"AllowListenConnections2", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_RULE_ALLOWLOOPBACK, app.ConfigGet (L"AllowLoopbackConnections", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_USEFULLBLOCKLIST_CHK, app.ConfigGet (L"IsExtraRulesEnabled", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_USESTEALTHMODE_CHK, app.ConfigGet (L"UseStealthMode", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_INSTALLBOOTTIMEFILTERS_CHK, app.ConfigGet (L"InstallBoottimeFilters", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_PROXYSUPPORT_CHK, app.ConfigGet (L"EnableProxySupport", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_USEHOSTS_CHK, app.ConfigGet (L"IsHostsEnabled", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					_r_ctrl_settip (hwnd, IDC_USEFULLBLOCKLIST_CHK, LPSTR_TEXTCALLBACK);
					_r_ctrl_settip (hwnd, IDC_USESTEALTHMODE_CHK, LPSTR_TEXTCALLBACK);
					_r_ctrl_settip (hwnd, IDC_INSTALLBOOTTIMEFILTERS_CHK, LPSTR_TEXTCALLBACK);
					_r_ctrl_settip (hwnd, IDC_PROXYSUPPORT_CHK, LPSTR_TEXTCALLBACK);

					if (!_r_sys_validversion (6, 2))
						_r_ctrl_enable (hwnd, IDC_PROXYSUPPORT_CHK, false);

					break;
				}

				case IDD_SETTINGS_INTERFACE:
				{
					CheckDlgButton (hwnd, IDC_CONFIRMEXIT_CHK, app.ConfigGet (L"ConfirmExit", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_CONFIRMDELETE_CHK, app.ConfigGet (L"ConfirmDelete", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_CONFIRMLOGCLEAR_CHK, app.ConfigGet (L"ConfirmLogClear", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					// configure listview
					_r_listview_setstyle (hwnd, IDC_COLORS, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_CHECKBOXES);

					_r_listview_deleteallitems (hwnd, IDC_COLORS);
					_r_listview_deleteallcolumns (hwnd, IDC_COLORS);

					_r_listview_addcolumn (hwnd, IDC_COLORS, 0, nullptr, 95, LVCFMT_LEFT);

					{
						for (size_t i = 0; i < colors.size (); i++)
						{
							ITEM_COLOR *ptr_clr = &colors.at (i);

							if (ptr_clr)
							{
								ptr_clr->clr = app.ConfigGet (ptr_clr->config_value, ptr_clr->default_clr).AsUlong ();

								config.is_nocheckboxnotify = true;

								_r_listview_additem (hwnd, IDC_COLORS, i, 0, I18N (&app, ptr_clr->locale_id, ptr_clr->locale_sid), LAST_VALUE, LAST_VALUE, i);
								_r_listview_setitemcheck (hwnd, IDC_COLORS, i, app.ConfigGet (ptr_clr->config_name, ptr_clr->is_enabled).AsBool ());

								config.is_nocheckboxnotify = false;
							}
						}
					}

					break;
				}

				case IDD_SETTINGS_RULES_BLOCKLIST:
				case IDD_SETTINGS_RULES_SYSTEM:
				case IDD_SETTINGS_RULES_CUSTOM:
				{
					// configure listview
					_r_listview_setstyle (hwnd, IDC_EDITOR, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_CHECKBOXES);

					SendDlgItemMessage (hwnd, IDC_EDITOR, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)config.himg);
					SendDlgItemMessage (hwnd, IDC_EDITOR, LVM_SETIMAGELIST, LVSIL_NORMAL, (LPARAM)config.himg);

					_r_listview_deleteallitems (hwnd, IDC_EDITOR);
					_r_listview_deleteallgroups (hwnd, IDC_EDITOR);
					_r_listview_deleteallcolumns (hwnd, IDC_EDITOR);

					_r_listview_addcolumn (hwnd, IDC_EDITOR, 0, I18N (&app, IDS_NAME, 0), 49, LVCFMT_LEFT);
					_r_listview_addcolumn (hwnd, IDC_EDITOR, 1, I18N (&app, IDS_DIRECTION, 0), 26, LVCFMT_LEFT);
					_r_listview_addcolumn (hwnd, IDC_EDITOR, 2, I18N (&app, IDS_PROTOCOL, 0), 20, LVCFMT_LEFT);

					_r_listview_addgroup (hwnd, IDC_EDITOR, 0, nullptr, 0, LVGS_COLLAPSIBLE);
					_r_listview_addgroup (hwnd, IDC_EDITOR, 1, nullptr, 0, LVGS_COLLAPSIBLE);
					_r_listview_addgroup (hwnd, IDC_EDITOR, 2, nullptr, 0, LVGS_COLLAPSIBLE);

					_app_listviewsetfont (hwnd, IDC_EDITOR);

					std::vector<ITEM_RULE*> const* ptr_rules = nullptr;

					if (page->dlg_id == IDD_SETTINGS_RULES_BLOCKLIST)
						ptr_rules = &rules_blocklist;

					else if (page->dlg_id == IDD_SETTINGS_RULES_SYSTEM)
						ptr_rules = &rules_system;

					else if (page->dlg_id == IDD_SETTINGS_RULES_CUSTOM)
						ptr_rules = &rules_custom;

					if (ptr_rules && !ptr_rules->empty ())
					{
						for (size_t i = 0, item = 0; i < ptr_rules->size (); i++)
						{
							ITEM_RULE const *ptr_rule = ptr_rules->at (i);

							if (!ptr_rule)
								continue;

							size_t group_id = 2;

							if (ptr_rule->is_enabled && !ptr_rule->papps)
								group_id = 0;

							else if (ptr_rule->is_enabled && ptr_rule->papps)
								group_id = 1;

							config.is_nocheckboxnotify = true;

							_r_listview_additem (hwnd, IDC_EDITOR, item, 0, ptr_rule->pname, ptr_rule->is_block ? 1 : 0, group_id, i);
							_r_listview_setitemcheck (hwnd, IDC_EDITOR, item, ptr_rule->is_enabled);

							item += 1;

							config.is_nocheckboxnotify = false;
						}
					}

					break;
				}

				case IDD_SETTINGS_LOG:
				{
					CheckDlgButton (hwnd, IDC_ENABLELOG_CHK, app.ConfigGet (L"IsLogEnabled", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					SetDlgItemText (hwnd, IDC_LOGPATH, app.ConfigGet (L"LogPath", PATH_LOG));

					UDACCEL ud = {0};
					ud.nInc = 64; // set step to 64kb

					SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_SETACCEL, 1, (LPARAM)&ud);
					SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_SETRANGE32, 64, 4096);
					SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_SETPOS32, 0, app.ConfigGet (L"LogSizeLimitKb", 256).AsUint ());

					CheckDlgButton (hwnd, IDC_LOGBACKUP_CHK, app.ConfigGet (L"IsLogBackup", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_ENABLENOTIFICATIONS_CHK, app.ConfigGet (L"IsNotificationsEnabled", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_NOTIFICATIONSILENT_CHK, app.ConfigGet (L"IsNotificationsSilent", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					SendDlgItemMessage (hwnd, IDC_NOTIFICATIONDISPLAYTIMEOUT, UDM_SETRANGE32, 0, _R_SECONDSCLOCK_HOUR (1));
					SendDlgItemMessage (hwnd, IDC_NOTIFICATIONDISPLAYTIMEOUT, UDM_SETPOS32, 0, app.ConfigGet (L"NotificationsDisplayTimeout", NOTIFY_TIMER_DEFAULT).AsUint ());

					SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_SETRANGE32, NOTIFY_TIMEOUT_MINIMUM, _R_SECONDSCLOCK_HOUR (1));
					SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_SETPOS32, 0, app.ConfigGet (L"NotificationsTimeout", NOTIFY_TIMEOUT).AsUint ());

					CheckDlgButton (hwnd, IDC_NOTIFICATIONNOBLOCKLIST_CHK, app.ConfigGet (L"IsNotificationsExcludeBlocklist", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					// dropped packets logging (win7 and above)
					if (!_r_sys_validversion (6, 1))
					{
						_r_ctrl_enable (hwnd, IDC_ENABLELOG_CHK, false);
						_r_ctrl_enable (hwnd, IDC_ENABLENOTIFICATIONS_CHK, false);
					}
					else
					{
						PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDC_ENABLELOG_CHK, 0), 0);
						PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDC_ENABLENOTIFICATIONS_CHK, 0), 0);
					}

					break;
				}
			}

			break;
		}

		case _RM_LOCALIZE:
		{
			// localize titles
			SetDlgItemText (hwnd, IDC_TITLE_GENERAL, I18N (&app, IDS_TITLE_GENERAL, 0));
			SetDlgItemText (hwnd, IDC_TITLE_LANGUAGE, I18N (&app, IDS_TITLE_LANGUAGE, 0));
			SetDlgItemText (hwnd, IDC_TITLE_CONFIRMATIONS, I18N (&app, IDS_TITLE_CONFIRMATIONS, 0));
			SetDlgItemText (hwnd, IDC_TITLE_HIGHLIGHTING, I18N (&app, IDS_TITLE_HIGHLIGHTING, 0));
			SetDlgItemText (hwnd, IDC_TITLE_EXPERTS, I18N (&app, IDS_TITLE_EXPERTS, 0));
			SetDlgItemText (hwnd, IDC_TITLE_NOTIFICATIONS, I18N (&app, IDS_TITLE_NOTIFICATIONS, 0));
			SetDlgItemText (hwnd, IDC_TITLE_ADVANCED, I18N (&app, IDS_TITLE_ADVANCED, 0));

			switch (page->dlg_id)
			{
				case IDD_SETTINGS_GENERAL:
				{
					SetDlgItemText (hwnd, IDC_ALWAYSONTOP_CHK, I18N (&app, IDS_ALWAYSONTOP_CHK, 0));
					SetDlgItemText (hwnd, IDC_LOADONSTARTUP_CHK, I18N (&app, IDS_LOADONSTARTUP_CHK, 0));
					SetDlgItemText (hwnd, IDC_SKIPUACWARNING_CHK, I18N (&app, IDS_SKIPUACWARNING_CHK, 0));
					SetDlgItemText (hwnd, IDC_CHECKUPDATES_CHK, I18N (&app, IDS_CHECKUPDATES_CHK, 0));

					SetDlgItemText (hwnd, IDC_LANGUAGE_HINT, I18N (&app, IDS_LANGUAGE_HINT, 0));

					break;
				}

				case IDD_SETTINGS_INTERFACE:
				{
					SetDlgItemText (hwnd, IDC_CONFIRMEXIT_CHK, I18N (&app, IDS_CONFIRMEXIT_CHK, 0));
					SetDlgItemText (hwnd, IDC_CONFIRMDELETE_CHK, I18N (&app, IDS_CONFIRMDELETE_CHK, 0));
					SetDlgItemText (hwnd, IDC_CONFIRMLOGCLEAR_CHK, I18N (&app, IDS_CONFIRMLOGCLEAR_CHK, 0));

					for (size_t i = 0; i < _r_listview_getitemcount (hwnd, IDC_COLORS); i++)
					{
						const size_t idx = _r_listview_getitemlparam (hwnd, IDC_COLORS, i);

						ITEM_COLOR const* ptr_clr = &colors.at (idx);

						if (ptr_clr)
							_r_listview_setitem (hwnd, IDC_COLORS, i, 0, I18N (&app, ptr_clr->locale_id, ptr_clr->locale_sid));
					}

					SetDlgItemText (hwnd, IDC_COLORS_HINT, I18N (&app, IDS_COLORS_HINT, 0));

					_app_listviewsetfont (hwnd, IDC_COLORS);

					break;
				}

				case IDD_SETTINGS_FILTERS:
				{
					SetDlgItemText (hwnd, IDC_RULE_ALLOWINBOUND, I18N (&app, IDS_RULE_ALLOWINBOUND, 0));
					SetDlgItemText (hwnd, IDC_RULE_ALLOWLISTEN, I18N (&app, IDS_RULE_ALLOWLISTEN, 0));
					SetDlgItemText (hwnd, IDC_RULE_ALLOWLOOPBACK, I18N (&app, IDS_RULE_ALLOWLOOPBACK, 0));

					_r_ctrl_settext (hwnd, IDC_USEFULLBLOCKLIST_CHK, L"%s*", I18N (&app, IDS_USEFULLBLOCKLIST_CHK, 0));
					_r_ctrl_settext (hwnd, IDC_USESTEALTHMODE_CHK, L"%s*", I18N (&app, IDS_USESTEALTHMODE_CHK, 0));
					_r_ctrl_settext (hwnd, IDC_INSTALLBOOTTIMEFILTERS_CHK, L"%s*", I18N (&app, IDS_INSTALLBOOTTIMEFILTERS_CHK, 0));
					_r_ctrl_settext (hwnd, IDC_PROXYSUPPORT_CHK, L"%s* [BETA]", I18N (&app, IDS_PROXYSUPPORT_CHK, 0));

					SetDlgItemText (hwnd, IDC_USEHOSTS_CHK, I18N (&app, IDS_USEHOSTS_CHK, 0));

					break;
				}

				case IDD_SETTINGS_RULES_BLOCKLIST:
				case IDD_SETTINGS_RULES_SYSTEM:
				case IDD_SETTINGS_RULES_CUSTOM:
				{
					_r_listview_setcolumn (hwnd, IDC_EDITOR, 0, I18N (&app, IDS_NAME, 0), 0);
					_r_listview_setcolumn (hwnd, IDC_EDITOR, 1, I18N (&app, IDS_DIRECTION, 0), 0);
					_r_listview_setcolumn (hwnd, IDC_EDITOR, 2, I18N (&app, IDS_PROTOCOL, 0), 0);

					std::vector<ITEM_RULE*> const* ptr_rules = nullptr;

					_r_fastlock_acquireshared (&lock_access);

					if (page->dlg_id == IDD_SETTINGS_RULES_BLOCKLIST)
						ptr_rules = &rules_blocklist;

					else if (page->dlg_id == IDD_SETTINGS_RULES_SYSTEM)
						ptr_rules = &rules_system;

					else if (page->dlg_id == IDD_SETTINGS_RULES_CUSTOM)
						ptr_rules = &rules_custom;

					if (ptr_rules && !ptr_rules->empty ())
					{
						size_t group1_count = 0;
						size_t group2_count = 0;
						size_t group3_count = 0;

						for (size_t i = 0; i < _r_listview_getitemcount (hwnd, IDC_EDITOR); i++)
						{
							const size_t idx = _r_listview_getitemlparam (hwnd, IDC_EDITOR, i);
							ITEM_RULE const *ptr_rule = ptr_rules->at (idx);

							if (!ptr_rule)
								continue;

							rstring dir = I18N (&app, IDS_DIRECTION_1 + ptr_rule->dir, _r_fmt (L"IDS_DIRECTION_%d", ptr_rule->dir + 1));
							rstring protocol = I18N (&app, IDS_ALL, 0);

							// protocol
							if (ptr_rule->protocol)
							{
								for (size_t j = 0; j < protocols.size (); j++)
								{
									if (ptr_rule->protocol == protocols.at (j).id)
										protocol = protocols.at (j).name;
								}
							}

							size_t group_id = 2;

							if (ptr_rule->is_enabled && !ptr_rule->papps)
							{
								group_id = 0;
								group1_count += 1;
							}
							else if (ptr_rule->is_enabled && ptr_rule->papps)
							{
								group_id = 1;
								group2_count += 1;
							}
							else
							{
								group3_count += 1;
							}

							config.is_nocheckboxnotify = true;

							_r_listview_setitem (hwnd, IDC_EDITOR, i, 0, ptr_rule->pname, ptr_rule->is_block ? 1 : 0, group_id);
							_r_listview_setitem (hwnd, IDC_EDITOR, i, 1, dir);
							_r_listview_setitem (hwnd, IDC_EDITOR, i, 2, protocol);

							_r_listview_setitemcheck (hwnd, IDC_EDITOR, i, ptr_rule->is_enabled);

							config.is_nocheckboxnotify = false;
						}

						_r_listview_setgroup (hwnd, IDC_EDITOR, 0, _r_fmt (L"%s (%d)", I18N (&app, IDS_GROUP_GLOBAL, 0), group1_count));
						_r_listview_setgroup (hwnd, IDC_EDITOR, 1, _r_fmt (L"%s (%d)", I18N (&app, IDS_GROUP_SPECIAL, 0), group2_count));
						_r_listview_setgroup (hwnd, IDC_EDITOR, 2, _r_fmt (L"%s (%d)", I18N (&app, IDS_GROUP_DISABLED, 0), group3_count));
					}

					_r_fastlock_releaseshared (&lock_access);

					_app_listviewsort_rules (hwnd, IDC_EDITOR);
					_r_listview_redraw (hwnd, IDC_EDITOR);

					_r_ctrl_settext (hwnd, IDC_RULES_BLOCKLIST_HINT, I18N (&app, IDS_RULES_BLOCKLIST_HINT, 0), BLOCKLIST_URL);
					SetDlgItemText (hwnd, IDC_RULES_SYSTEM_HINT, I18N (&app, IDS_RULES_SYSTEM_HINT, 0));
					_r_ctrl_settext (hwnd, IDC_RULES_CUSTOM_HINT, I18N (&app, IDS_RULES_CUSTOM_HINT, 0), WIKI_URL);

					SendDlgItemMessage (hwnd, IDC_EDITOR, LVM_RESETEMPTYTEXT, 0, 0);

					break;
				}

				case IDD_SETTINGS_LOG:
				{
					SetDlgItemText (hwnd, IDC_ENABLELOG_CHK, I18N (&app, IDS_ENABLELOG_CHK, 0));

					SetDlgItemText (hwnd, IDC_LOGSIZELIMIT_HINT, I18N (&app, IDS_LOGSIZELIMIT_HINT, 0));
					SetDlgItemText (hwnd, IDC_LOGBACKUP_CHK, I18N (&app, IDS_LOGBACKUP_CHK, 0));

					SetDlgItemText (hwnd, IDC_ENABLENOTIFICATIONS_CHK, I18N (&app, IDS_ENABLENOTIFICATIONS_CHK, 0));
					SetDlgItemText (hwnd, IDC_NOTIFICATIONSILENT_CHK, I18N (&app, IDS_NOTIFICATIONSILENT_CHK, 0));
					SetDlgItemText (hwnd, IDC_NOTIFICATIONDISPLAYTIMEOUT_HINT, I18N (&app, IDS_NOTIFICATIONDISPLAYTIMEOUT_HINT, 0));
					SetDlgItemText (hwnd, IDC_NOTIFICATIONTIMEOUT_HINT, I18N (&app, IDS_NOTIFICATIONTIMEOUT_HINT, 0));
					SetDlgItemText (hwnd, IDC_NOTIFICATIONNOBLOCKLIST_CHK, I18N (&app, IDS_NOTIFICATIONNOBLOCKLIST_CHK, 0));

					_r_wnd_addstyle (hwnd, IDC_LOGPATH_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

					break;
				}
			}

			break;
		}

		case _RM_MESSAGE:
		{
			LPMSG pmsg = (LPMSG)lpdata1;

			switch (pmsg->message)
			{
				case WM_VSCROLL:
				case WM_HSCROLL:
				{
					if (page->dlg_id == IDD_SETTINGS_LOG)
					{
						const UINT ctrl_id = GetDlgCtrlID ((HWND)pmsg->lParam);

						if (ctrl_id == IDC_LOGSIZELIMIT)
							app.ConfigSet (L"LogSizeLimitKb", (DWORD)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));

						else if (ctrl_id == IDC_NOTIFICATIONDISPLAYTIMEOUT)
							app.ConfigSet (L"NotificationsDisplayTimeout", (DWORD)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));

						else if (ctrl_id == IDC_NOTIFICATIONTIMEOUT)
							app.ConfigSet (L"NotificationsTimeout", (DWORD)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));
					}

					break;
				}

				case WM_NOTIFY:
				{
					LPNMHDR nmlp = (LPNMHDR)pmsg->lParam;

					switch (nmlp->code)
					{
						case TTN_GETDISPINFO:
						{
							LPNMTTDISPINFO lpnmdi = (LPNMTTDISPINFO)pmsg->lParam;

							if ((lpnmdi->uFlags & TTF_IDISHWND) != 0)
							{
								WCHAR buffer[1024] = {0};
								const UINT ctrl_id = GetDlgCtrlID ((HWND)lpnmdi->hdr.idFrom);

								if (ctrl_id == IDC_USEFULLBLOCKLIST_CHK)
									StringCchCopy (buffer, _countof (buffer), I18N (&app, IDS_USEFULLBLOCKLIST_HINT, 0));

								else if (ctrl_id == IDC_USESTEALTHMODE_CHK)
									StringCchCopy (buffer, _countof (buffer), I18N (&app, IDS_USESTEALTHMODE_HINT, 0));

								else if (ctrl_id == IDC_INSTALLBOOTTIMEFILTERS_CHK)
									StringCchCopy (buffer, _countof (buffer), I18N (&app, IDS_INSTALLBOOTTIMEFILTERS_HINT, 0));

								else if (ctrl_id == IDC_PROXYSUPPORT_CHK)
									StringCchCopy (buffer, _countof (buffer), I18N (&app, IDS_PROXYSUPPORT_HINT, 0));

								if (buffer[0])
									lpnmdi->lpszText = buffer;
							}

							break;
						}

						case LVN_ITEMCHANGED:
						{
							if (config.is_nocheckboxnotify)
								break;

							LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)pmsg->lParam;

							if (lpnmlv->uNewState == 8192 || lpnmlv->uNewState == 4096)
							{
								const bool new_val = (lpnmlv->uNewState == 8192) ? true : false;
								const size_t idx = lpnmlv->lParam;

								if (nmlp->idFrom == IDC_COLORS)
								{
									ITEM_COLOR* ptr_clr = &colors.at (idx);

									if (ptr_clr)
									{
										app.ConfigSet (ptr_clr->config_name, new_val);

										_r_listview_redraw (app.GetHWND (), IDC_LISTVIEW);
									}
								}
								else if (nmlp->idFrom == IDC_EDITOR)
								{
									std::vector<ITEM_RULE*> const* ptr_rules = nullptr;

									if (page->dlg_id == IDD_SETTINGS_RULES_BLOCKLIST)
										ptr_rules = &rules_blocklist;

									else if (page->dlg_id == IDD_SETTINGS_RULES_SYSTEM)
										ptr_rules = &rules_system;

									else if (page->dlg_id == IDD_SETTINGS_RULES_CUSTOM)
										ptr_rules = &rules_custom;

									if (ptr_rules && !ptr_rules->empty ())
									{
										ITEM_RULE *ptr_rule = ptr_rules->at (idx);

										if (ptr_rule)
										{
											ptr_rule->is_enabled = new_val;

											if (
												page->dlg_id == IDD_SETTINGS_RULES_BLOCKLIST ||
												page->dlg_id == IDD_SETTINGS_RULES_SYSTEM
												)
											{
												if (ptr_rule->pname)
													rules_config[ptr_rule->pname] = new_val;
											}

											_app_profilesave (app.GetHWND ());

											settings_callback (hwnd, _RM_LOCALIZE, nullptr, page);

											_app_installfilters (false);
										}
									}
								}
							}

							break;
						}

						case LVN_GETINFOTIP:
						{
							const UINT ctrl_id = (UINT)nmlp->idFrom;

							if (ctrl_id != IDC_EDITOR)
								break;

							LPNMLVGETINFOTIP lpnmlv = (LPNMLVGETINFOTIP)pmsg->lParam;

							ITEM_RULE const *ptr_rule = nullptr;

							const size_t idx = _r_listview_getitemlparam (hwnd, ctrl_id, lpnmlv->iItem);

							if (page->dlg_id == IDD_SETTINGS_RULES_BLOCKLIST)
								ptr_rule = rules_blocklist.at (idx);

							else if (page->dlg_id == IDD_SETTINGS_RULES_SYSTEM)
								ptr_rule = rules_system.at (idx);

							else if (page->dlg_id == IDD_SETTINGS_RULES_CUSTOM)
								ptr_rule = rules_custom.at (idx);

							if (ptr_rule)
							{
								rstring rule = ptr_rule->prule;

								if (rule.IsEmpty ())
									rule = I18N (&app, IDS_STATUS_EMPTY, 0);

								else
									rule.Replace (RULE_DELIMETER, L"\r\n" TAB_SPACE);

								StringCchPrintf (lpnmlv->pszText, lpnmlv->cchTextMax, L"%s (#%d)\r\n%s:\r\n%s%s", ptr_rule->pname, idx, I18N (&app, IDS_RULE, 0), TAB_SPACE, rule.GetString ());

								if (ptr_rule->papps)
									StringCchCat (lpnmlv->pszText, lpnmlv->cchTextMax, _r_fmt (L"\r\n%s:\r\n%s%s", I18N (&app, IDS_FILEPATH, 0), TAB_SPACE, rstring (ptr_rule->papps).Replaced (RULE_DELIMETER, L"\r\n" TAB_SPACE).GetString ()));
							}

							break;
						}

						case NM_CUSTOMDRAW:
						{
							if (nmlp->idFrom == IDC_COLORS || nmlp->idFrom == IDC_EDITOR)
							{
								LPNMLVCUSTOMDRAW lpnmlv = (LPNMLVCUSTOMDRAW)pmsg->lParam;
								LPARAM lparam = 0;

								if (lpnmlv->nmcd.hdr.idFrom == IDC_EDITOR && lpnmlv->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
								{
									const size_t idx = lpnmlv->nmcd.lItemlParam;
									ITEM_RULE const* ptr_rule = nullptr;

									_r_fastlock_acquireshared (&lock_access);

									if (page->dlg_id == IDD_SETTINGS_RULES_BLOCKLIST)
										ptr_rule = rules_blocklist.at (idx);

									else if (page->dlg_id == IDD_SETTINGS_RULES_SYSTEM)
										ptr_rule = rules_system.at (idx);

									else if (page->dlg_id == IDD_SETTINGS_RULES_CUSTOM)
										ptr_rule = rules_custom.at (idx);

									if (ptr_rule)
									{
										if (ptr_rule->error_count)
											lparam = (LPARAM)_app_getcolorvalue (_r_str_hash (L"ColorInvalid"), false);

										else if (ptr_rule->papps)
											lparam = (LPARAM)_app_getcolorvalue (_r_str_hash (L"ColorSpecial"), false);
									}

									_r_fastlock_releaseshared (&lock_access);
								}

								SetWindowLongPtr (hwnd, DWLP_MSGRESULT, _app_wmcustdraw (lpnmlv, lparam));
								return TRUE;
							}

							break;
						}

						case LVN_GETEMPTYMARKUP:
						{
							NMLVEMPTYMARKUP* lpnmlv = (NMLVEMPTYMARKUP*)pmsg->lParam;

							lpnmlv->dwFlags = EMF_CENTERED;
							StringCchCopy (lpnmlv->szMarkup, _countof (lpnmlv->szMarkup), I18N (&app, IDS_STATUS_EMPTY, 0));

							SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
							return TRUE;
						}

						case NM_DBLCLK:
						{
							LPNMITEMACTIVATE lpnmlv = (LPNMITEMACTIVATE)pmsg->lParam;

							if (lpnmlv->iItem == -1)
								break;

							if (nmlp->idFrom == IDC_COLORS)
							{
								const size_t idx = _r_listview_getitemlparam (hwnd, IDC_COLORS, lpnmlv->iItem);

								CHOOSECOLOR cc = {0};

								COLORREF cust[16] = {
									LISTVIEW_COLOR_INVALID,
									LISTVIEW_COLOR_NETWORK,
									LISTVIEW_COLOR_PACKAGE,
									LISTVIEW_COLOR_PICO,
									LISTVIEW_COLOR_SIGNED,
									LISTVIEW_COLOR_SILENT,
									LISTVIEW_COLOR_SPECIAL,
									LISTVIEW_COLOR_SYSTEM,
								};

								cc.lStructSize = sizeof (cc);
								cc.Flags = CC_RGBINIT | CC_FULLOPEN;
								cc.hwndOwner = hwnd;
								cc.lpCustColors = cust;
								cc.rgbResult = colors.at (idx).clr;

								if (ChooseColor (&cc))
								{
									ITEM_COLOR *ptr_clr = &colors.at (idx);

									if (ptr_clr)
									{
										if (ptr_clr->hbr)
											DeleteObject (ptr_clr->hbr);

										ptr_clr->hbr = CreateSolidBrush (cc.rgbResult);
										ptr_clr->clr = cc.rgbResult;

										app.ConfigSet (ptr_clr->config_value, cc.rgbResult);

										_r_listview_redraw (hwnd, IDC_COLORS);
										_r_listview_redraw (app.GetHWND (), IDC_LISTVIEW);
									}
								}
							}
							else if (nmlp->idFrom == IDC_EDITOR)
							{
								PostMessage (hwnd, WM_COMMAND, MAKELPARAM (IDM_EDIT, 0), 0);
							}

							break;
						}

						case NM_CLICK:
						case NM_RETURN:
						{
							if (nmlp->idFrom == IDC_RULES_BLOCKLIST_HINT || nmlp->idFrom == IDC_RULES_CUSTOM_HINT)
							{
								PNMLINK nmlink = (PNMLINK)pmsg->lParam;

								if (nmlink->item.szUrl)
									ShellExecute (hwnd, nullptr, nmlink->item.szUrl, nullptr, nullptr, SW_SHOWDEFAULT);
							}

							break;
						}
					}

					break;
				}

				case WM_CONTEXTMENU:
				{
					const UINT ctrl_id = GetDlgCtrlID ((HWND)pmsg->wParam);

					if (ctrl_id != IDC_EDITOR)
						break;

					const HMENU menu = LoadMenu (nullptr, MAKEINTRESOURCE (IDM_EDITOR));
					const HMENU submenu = GetSubMenu (menu, 0);

					// localize
					app.LocaleMenu (submenu, I18N (&app, IDS_CHECKALL, 0), IDM_CHECKALL, false, nullptr);
					app.LocaleMenu (submenu, I18N (&app, IDS_UNCHECKALL, 0), IDM_UNCHECKALL, false, nullptr);
					app.LocaleMenu (submenu, I18N (&app, IDS_CHECK, 0), IDM_CHECK, false, nullptr);
					app.LocaleMenu (submenu, I18N (&app, IDS_UNCHECK, 0), IDM_UNCHECK, false, nullptr);

					if (!SendDlgItemMessage (hwnd, ctrl_id, LVM_GETITEMCOUNT, 0, 0))
					{
						EnableMenuItem (submenu, IDS_CHECKALL, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
						EnableMenuItem (submenu, IDS_UNCHECKALL, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					}

					if (!SendDlgItemMessage (hwnd, ctrl_id, LVM_GETSELECTEDCOUNT, 0, 0))
					{
						EnableMenuItem (submenu, IDM_EDIT, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
						EnableMenuItem (submenu, IDM_DELETE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
						EnableMenuItem (submenu, IDM_CHECK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
						EnableMenuItem (submenu, IDM_UNCHECK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					}

					if (page->dlg_id == IDD_SETTINGS_RULES_CUSTOM)
					{
						app.LocaleMenu (submenu, I18N (&app, IDS_ADD, 0), IDM_ADD, false, nullptr);
						app.LocaleMenu (submenu, I18N (&app, IDS_EDIT2, 0), IDM_EDIT, false, nullptr);
						app.LocaleMenu (submenu, I18N (&app, IDS_DELETE, 0), IDM_DELETE, false, nullptr);
					}
					else
					{
						DeleteMenu (submenu, IDM_ADD, MF_BYCOMMAND);
						DeleteMenu (submenu, IDM_EDIT, MF_BYCOMMAND);
						DeleteMenu (submenu, IDM_DELETE, MF_BYCOMMAND);
						DeleteMenu (submenu, 0, MF_BYPOSITION);
					}

					POINT pt = {0};
					GetCursorPos (&pt);

					TrackPopupMenuEx (submenu, TPM_RIGHTBUTTON | TPM_LEFTBUTTON, pt.x, pt.y, hwnd, nullptr);

					DestroyMenu (menu);

					break;
				}

				case WM_COMMAND:
				{
					switch (LOWORD (pmsg->wParam))
					{
						case IDC_ALWAYSONTOP_CHK:
						case IDC_LOADONSTARTUP_CHK:
						case IDC_SKIPUACWARNING_CHK:
						case IDC_CHECKUPDATES_CHK:
						case IDC_LANGUAGE:
						case IDC_USEFULLBLOCKLIST_CHK:
						case IDC_USESTEALTHMODE_CHK:
						case IDC_INSTALLBOOTTIMEFILTERS_CHK:
						case IDC_PROXYSUPPORT_CHK:
						case IDC_USEHOSTS_CHK:
						case IDC_RULE_ALLOWINBOUND:
						case IDC_RULE_ALLOWLISTEN:
						case IDC_RULE_ALLOWLOOPBACK:
						case IDC_CONFIRMEXIT_CHK:
						case IDC_CONFIRMDELETE_CHK:
						case IDC_CONFIRMLOGCLEAR_CHK:
						case IDC_ENABLELOG_CHK:
						case IDC_LOGPATH:
						case IDC_LOGPATH_BTN:
						case IDC_LOGSIZELIMIT_CTRL:
						case IDC_LOGBACKUP_CHK:
						case IDC_ENABLENOTIFICATIONS_CHK:
						case IDC_NOTIFICATIONSILENT_CHK:
						case IDC_NOTIFICATIONDISPLAYTIMEOUT_CTRL:
						case IDC_NOTIFICATIONTIMEOUT_CTRL:
						case IDC_NOTIFICATIONNOBLOCKLIST_CHK:
						{
							const UINT ctrl_id = LOWORD (pmsg->wParam);
							const UINT notify_code = HIWORD (pmsg->wParam);

							if (ctrl_id == IDC_ALWAYSONTOP_CHK)
							{
								app.ConfigSet (L"AlwaysOnTop", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
								CheckMenuItem (GetMenu (app.GetHWND ()), IDM_ALWAYSONTOP_CHK, MF_BYCOMMAND | ((IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? MF_CHECKED : MF_UNCHECKED));
							}
							else if (ctrl_id == IDC_LOADONSTARTUP_CHK)
							{
								app.AutorunEnable (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);
							}
							else if (ctrl_id == IDC_SKIPUACWARNING_CHK)
							{
								app.SkipUacEnable (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);
							}
							else if (ctrl_id == IDC_CHECKUPDATES_CHK)
							{
								app.ConfigSet (L"CheckUpdates", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
							}
							else if (ctrl_id == IDC_LANGUAGE && notify_code == CBN_SELCHANGE)
							{
								app.LocaleApplyFromControl (hwnd, ctrl_id);
							}
							else if (ctrl_id == IDC_USEFULLBLOCKLIST_CHK)
							{
								if (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED && !messageFlag (hwnd, L"ConfirmExpert", I18N (&app, IDS_QUESTION_EXPERT, 0), I18N (&app, IDS_QUESTION_FLAG_CHK, 0)))
								{
									CheckDlgButton (hwnd, ctrl_id, BST_UNCHECKED);
									return TRUE;
								}

								app.ConfigSet (L"IsExtraRulesEnabled", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);

								for (size_t i = 0; i < rules_blocklist.size (); i++)
								{
									if (rules_blocklist.at (i))
										_app_freerule (&rules_blocklist.at (i));
								}

								rules_blocklist.clear ();
								_app_loadrules (hwnd, _r_fmt (L"%s\\" XML_BLOCKLIST, app.GetProfileDirectory ()), IDR_RULES_BLOCKLIST, true, &rules_blocklist);

								app.SettingsPageInitialize (IDD_SETTINGS_RULES_BLOCKLIST, false); // re-inititalize page

								_app_installfilters (false);
							}
							else if (ctrl_id == IDC_USESTEALTHMODE_CHK)
							{
								if (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED && !messageFlag (hwnd, L"ConfirmExpert", I18N (&app, IDS_QUESTION_EXPERT, 0), I18N (&app, IDS_QUESTION_FLAG_CHK, 0)))
								{
									CheckDlgButton (hwnd, ctrl_id, BST_UNCHECKED);
									return TRUE;
								}

								app.ConfigSet (L"UseStealthMode", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);

								_app_installfilters (false);
							}
							else if (ctrl_id == IDC_INSTALLBOOTTIMEFILTERS_CHK)
							{
								if (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED && !messageFlag (hwnd, L"ConfirmExpert", I18N (&app, IDS_QUESTION_EXPERT, 0), I18N (&app, IDS_QUESTION_FLAG_CHK, 0)))
								{
									CheckDlgButton (hwnd, ctrl_id, BST_UNCHECKED);
									return TRUE;
								}

								app.ConfigSet (L"InstallBoottimeFilters", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);

								_app_installfilters (false);
							}
							else if (ctrl_id == IDC_PROXYSUPPORT_CHK)
							{
								if (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED && !messageFlag (hwnd, L"ConfirmExpert", I18N (&app, IDS_QUESTION_EXPERT, 0), I18N (&app, IDS_QUESTION_FLAG_CHK, 0)))
								{
									CheckDlgButton (hwnd, ctrl_id, BST_UNCHECKED);
									return TRUE;
								}

								app.ConfigSet (L"EnableProxySupport", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);

								_app_installfilters (false);
							}
							else if (ctrl_id == IDC_USEHOSTS_CHK)
							{
								app.ConfigSet (L"IsHostsEnabled", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);

								_app_installfilters (false);
							}
							else if (ctrl_id == IDC_RULE_ALLOWINBOUND)
							{
								app.ConfigSet (L"AllowInboundConnections", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);

								_app_installfilters (false);
							}
							else if (ctrl_id == IDC_RULE_ALLOWLISTEN)
							{
								if (IsDlgButtonChecked (hwnd, ctrl_id) == BST_UNCHECKED && !messageFlag (hwnd, L"ConfirmListen", _r_fmt (I18N (&app, IDS_QUESTION_LISTEN, 0), LISTENS_ISSUE_URL), I18N (&app, IDS_QUESTION_FLAG_CHK, 0)))
								{
									CheckDlgButton (hwnd, ctrl_id, BST_CHECKED);
									return TRUE;
								}

								app.ConfigSet (L"AllowListenConnections2", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);

								_app_installfilters (false);
							}
							else if (ctrl_id == IDC_RULE_ALLOWLOOPBACK)
							{
								app.ConfigSet (L"AllowLoopbackConnections", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);

								_app_installfilters (false);
							}
							else if (ctrl_id == IDC_CONFIRMEXIT_CHK)
							{
								app.ConfigSet (L"ConfirmExit", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
							}
							else if (ctrl_id == IDC_CONFIRMDELETE_CHK)
							{
								app.ConfigSet (L"ConfirmDelete", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
							}
							else if (ctrl_id == IDC_CONFIRMLOGCLEAR_CHK)
							{
								app.ConfigSet (L"ConfirmLogClear", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
							}
							else if (ctrl_id == IDC_ENABLELOG_CHK)
							{
								const bool is_enabled = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

								app.ConfigSet (L"IsLogEnabled", is_enabled);
								CheckMenuItem (GetMenu (app.GetHWND ()), IDM_ENABLELOG_CHK, MF_BYCOMMAND | (is_enabled ? MF_CHECKED : MF_UNCHECKED));

								_r_ctrl_enable (hwnd, IDC_LOGPATH, is_enabled); // input
								_r_ctrl_enable (hwnd, IDC_LOGPATH_BTN, is_enabled); // button

								EnableWindow ((HWND)SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_GETBUDDY, 0, 0), is_enabled);

								_r_ctrl_enable (hwnd, IDC_LOGBACKUP_CHK, is_enabled); // button

								_app_loginit (is_enabled);
							}
							else if (ctrl_id == IDC_LOGPATH && notify_code == EN_KILLFOCUS)
							{
								app.ConfigSet (L"LogPath", _r_ctrl_gettext (hwnd, ctrl_id));

								_app_loginit (app.ConfigGet (L"IsLogEnabled", false));
							}
							else if (ctrl_id == IDC_LOGPATH_BTN)
							{
								OPENFILENAME ofn = {0};

								WCHAR path[MAX_PATH] = {0};
								GetDlgItemText (hwnd, IDC_LOGPATH, path, _countof (path));
								StringCchCopy (path, _countof (path), _r_path_expand (path));

								ofn.lStructSize = sizeof (ofn);
								ofn.hwndOwner = hwnd;
								ofn.lpstrFile = path;
								ofn.nMaxFile = _countof (path);
								ofn.lpstrFileTitle = APP_NAME_SHORT;
								ofn.nMaxFile = _countof (path);
								ofn.lpstrFilter = L"*.log\0*.log\0\0";
								ofn.lpstrDefExt = L"log";
								ofn.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

								if (GetSaveFileName (&ofn))
								{
									StringCchCopy (path, _countof (path), _r_path_unexpand (path));

									app.ConfigSet (L"LogPath", path);
									SetDlgItemText (hwnd, IDC_LOGPATH, path);

									_app_loginit (app.ConfigGet (L"IsLogEnabled", false));

								}
							}
							else if (ctrl_id == IDC_LOGSIZELIMIT_CTRL && notify_code == EN_KILLFOCUS)
							{
								app.ConfigSet (L"LogSizeLimitKb", (DWORD)SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_GETPOS32, 0, 0));
							}
							else if (ctrl_id == IDC_LOGBACKUP_CHK)
							{
								app.ConfigSet (L"IsLogBackup", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
							}
							else if (ctrl_id == IDC_ENABLENOTIFICATIONS_CHK)
							{
								const bool is_enabled = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

								app.ConfigSet (L"IsNotificationsEnabled", is_enabled);
								CheckMenuItem (GetMenu (app.GetHWND ()), IDM_ENABLENOTIFICATIONS_CHK, MF_BYCOMMAND | (is_enabled ? MF_CHECKED : MF_UNCHECKED));

								_r_ctrl_enable (hwnd, IDC_NOTIFICATIONSILENT_CHK, is_enabled);
								_r_ctrl_enable (hwnd, IDC_NOTIFICATIONNOBLOCKLIST_CHK, is_enabled);

								EnableWindow ((HWND)SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_GETBUDDY, 0, 0), is_enabled);
								EnableWindow ((HWND)SendDlgItemMessage (hwnd, IDC_NOTIFICATIONDISPLAYTIMEOUT, UDM_GETBUDDY, 0, 0), is_enabled);
							}
							else if (ctrl_id == IDC_NOTIFICATIONSILENT_CHK)
							{
								app.ConfigSet (L"IsNotificationsSilent", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
							}
							else if (ctrl_id == IDC_NOTIFICATIONDISPLAYTIMEOUT_CTRL && notify_code == EN_KILLFOCUS)
							{
								app.ConfigSet (L"NotificationsDisplayTimeout", (DWORD)SendDlgItemMessage (hwnd, IDC_NOTIFICATIONDISPLAYTIMEOUT, UDM_GETPOS32, 0, 0));
							}
							else if (ctrl_id == IDC_NOTIFICATIONTIMEOUT_CTRL && notify_code == EN_KILLFOCUS)
							{
								app.ConfigSet (L"NotificationsTimeout", (DWORD)SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_GETPOS32, 0, 0));
							}
							else if (ctrl_id == IDC_NOTIFICATIONNOBLOCKLIST_CHK)
							{
								app.ConfigSet (L"IsNotificationsExcludeBlocklist", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
							}

							break;
						}

						case IDM_ADD:
						{
							if (page->dlg_id != IDD_SETTINGS_RULES_CUSTOM)
								break;

							ITEM_RULE *ptr_rule = (ITEM_RULE*)malloc (sizeof (ITEM_RULE));

							if (ptr_rule)
							{
								SecureZeroMemory (ptr_rule, sizeof (ITEM_RULE));

								ptr_rule->is_block = true; // block by default

								SetWindowLongPtr (app.GetHWND (), GWLP_USERDATA, LAST_VALUE);
								if (DialogBoxParam (nullptr, MAKEINTRESOURCE (IDD_EDITOR), hwnd, &EditorProc, (LPARAM)ptr_rule))
								{
									_r_fastlock_acquireexclusive (&lock_access);

									rules_custom.push_back (ptr_rule);

									_r_fastlock_releaseexclusive (&lock_access);

									_app_profilesave (app.GetHWND ());
									_app_profileload (app.GetHWND ()); // important!

									app.SettingsPageInitialize (page->dlg_id, true); // re-inititalize page

									_app_installfilters (false);
								}
								else
								{
									_app_freerule (&ptr_rule);
								}
							}

							break;
						}

						case IDM_EDIT:
						{
							if (page->dlg_id != IDD_SETTINGS_RULES_CUSTOM)
								break;

							const size_t item = (size_t)SendDlgItemMessage (hwnd, IDC_EDITOR, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);

							if (item == LAST_VALUE)
								break;

							const size_t idx = _r_listview_getitemlparam (hwnd, IDC_EDITOR, item);

							ITEM_RULE *ptr_rule = rules_custom.at (idx);

							if (ptr_rule)
							{
								SetWindowLongPtr (app.GetHWND (), GWLP_USERDATA, idx);
								if (DialogBoxParam (nullptr, MAKEINTRESOURCE (IDD_EDITOR), hwnd, &EditorProc, (LPARAM)ptr_rule))
								{
									_app_profilesave (app.GetHWND ());
									_app_profileload (app.GetHWND ()); // important!

									app.SettingsPageInitialize (page->dlg_id, true); // re-inititalize page

									_app_installfilters (false);
								}
							}

							break;
						}

						case IDM_DELETE:
						{
							if (page->dlg_id != IDD_SETTINGS_RULES_CUSTOM)
								break;

							const size_t total_count = (size_t)SendDlgItemMessage (hwnd, IDC_EDITOR, LVM_GETSELECTEDCOUNT, 0, 0);

							if (!total_count)
								break;

							if (_r_msg (hwnd, MB_YESNO | MB_ICONEXCLAMATION, APP_NAME, nullptr, I18N (&app, IDS_QUESTION_DELETE, 0), total_count) != IDYES)
								break;

							const size_t count = _r_listview_getitemcount (hwnd, IDC_EDITOR) - 1;
							bool is_enabled = false;

							_r_fastlock_acquireexclusive (&lock_access);

							for (size_t i = count; i != LAST_VALUE; i--)
							{
								if (ListView_GetItemState (GetDlgItem (hwnd, IDC_EDITOR), i, LVNI_SELECTED))
								{
									const size_t idx = _r_listview_getitemlparam (hwnd, IDC_EDITOR, i);

									// clear special rules
									for (auto &p : apps)
									{
										ITEM_APPLICATION *ptr_app = &p.second;

										if (ptr_app)
										{
											for (size_t j = 0; j < ptr_app->rules.size (); j++)
											{
												if (ptr_app->rules.at (j) == idx)
													ptr_app->rules.erase (ptr_app->rules.begin () + j);
											}
										}
									}

									if (!is_enabled && rules_custom.at (idx) && rules_custom.at (idx)->is_enabled)
										is_enabled = true;

									SendDlgItemMessage (hwnd, IDC_EDITOR, LVM_DELETEITEM, i, 0);

									_app_freerule (&rules_custom.at (idx));
								}
							}

							_r_fastlock_releaseexclusive (&lock_access);

							_app_profilesave (app.GetHWND ());
							_app_profileload (app.GetHWND ()); // important!

							app.SettingsPageInitialize (page->dlg_id, true); // re-inititalize page

							if (is_enabled)
								_app_installfilters (false);

							_r_listview_redraw (hwnd, IDC_EDITOR);

							break;
						}

						case IDM_CHECK:
						case IDM_UNCHECK:
						case IDM_CHECKALL:
						case IDM_UNCHECKALL:
						{
							std::vector<ITEM_RULE*> const* ptr_rules = nullptr;

							if (page->dlg_id == IDD_SETTINGS_RULES_BLOCKLIST)
								ptr_rules = &rules_blocklist;

							else if (page->dlg_id == IDD_SETTINGS_RULES_SYSTEM)
								ptr_rules = &rules_system;

							else if (page->dlg_id == IDD_SETTINGS_RULES_CUSTOM)
								ptr_rules = &rules_custom;

							if (ptr_rules)
							{
								const bool new_val = ((LOWORD (pmsg->wParam) == IDM_CHECK) || (LOWORD (pmsg->wParam) == IDM_CHECKALL)) ? true : false;
								const INT flag = ((LOWORD (pmsg->wParam) == IDM_CHECK) || (LOWORD (pmsg->wParam) == IDM_UNCHECK)) ? LVNI_SELECTED : LVNI_ALL;

								INT item = -1;

								_r_fastlock_acquireexclusive (&lock_access);

								while ((item = (INT)SendDlgItemMessage (hwnd, IDC_EDITOR, LVM_GETNEXTITEM, item, flag)) != -1)
								{
									const size_t idx = _r_listview_getitemlparam (hwnd, IDC_EDITOR, item);

									ITEM_RULE* ptr_rule = ptr_rules->at (idx);

									if (ptr_rule)
									{
										ptr_rule->is_enabled = new_val;

										config.is_nocheckboxnotify = true;

										_r_listview_setitemcheck (hwnd, IDC_EDITOR, item, new_val);

										if (page->dlg_id == IDD_SETTINGS_RULES_BLOCKLIST || page->dlg_id == IDD_SETTINGS_RULES_SYSTEM)
											rules_config[ptr_rule->pname] = new_val;

										config.is_nocheckboxnotify = false;
									}
								}

								_r_fastlock_releaseexclusive (&lock_access);

								_app_profilesave (app.GetHWND ());
								_app_installfilters (false);

								settings_callback (hwnd, _RM_LOCALIZE, nullptr, page); // re-inititalize page
							}

							break;
						}
					}

					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

void ResizeWindow (HWND hwnd, INT width, INT height)
{
	RECT rc = {0};

	GetClientRect (GetDlgItem (hwnd, IDC_EXIT_BTN), &rc);
	const INT button_width = rc.right;

	GetClientRect (GetDlgItem (hwnd, IDC_STATUSBAR), &rc);
	const INT statusbar_height = rc.bottom;

	const INT button_top = height - statusbar_height - app.GetDPI (1 + 34);

	SetWindowPos (GetDlgItem (hwnd, IDC_LISTVIEW), nullptr, 0, 0, width, height - statusbar_height - app.GetDPI (1 + 46), SWP_NOCOPYBITS | SWP_NOREDRAW | SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE);

	SetWindowPos (GetDlgItem (hwnd, IDC_START_BTN), nullptr, app.GetDPI (10), button_top, 0, 0, SWP_NOCOPYBITS | SWP_NOREDRAW | SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE | SWP_NOSIZE);
	SetWindowPos (GetDlgItem (hwnd, IDC_SETTINGS_BTN), nullptr, width - app.GetDPI (10) - button_width - button_width - app.GetDPI (6), button_top, 0, 0, SWP_NOCOPYBITS | SWP_NOREDRAW | SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE | SWP_NOSIZE);
	SetWindowPos (GetDlgItem (hwnd, IDC_EXIT_BTN), nullptr, width - app.GetDPI (10) - button_width, button_top, 0, 0, SWP_NOCOPYBITS | SWP_NOREDRAW | SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE | SWP_NOSIZE);

	// resize statusbar
	SendDlgItemMessage (hwnd, IDC_STATUSBAR, WM_SIZE, 0, 0);
}

bool _wfp_logunsubscribe ()
{
	bool result = false;

	_app_loginit (false); // destroy log file handle if present

	if (config.hevent)
	{
		FWPMNEU _FwpmNetEventUnsubscribe = (FWPMNEU)GetProcAddress (GetModuleHandle (L"fwpuclnt.dll"), "FwpmNetEventUnsubscribe0");

		if (_FwpmNetEventUnsubscribe)
		{
			DWORD rc = _FwpmNetEventUnsubscribe (config.hengine, config.hevent);

			if (rc == ERROR_SUCCESS)
			{
				config.hevent = nullptr;
				result = true;
			}
		}
	}

	return result;
}

bool _wfp_logsubscribe ()
{
	bool result = false;

	if (config.hevent)
	{
		result = true;
	}
	else
	{
		HINSTANCE hmodule = GetModuleHandle (L"fwpuclnt.dll");

		if (hmodule)
		{
			FWPMNES2 _FwpmNetEventSubscribe2 = nullptr;
			FWPMNES1 _FwpmNetEventSubscribe1 = nullptr;
			FWPMNES0 _FwpmNetEventSubscribe0 = nullptr;

			_FwpmNetEventSubscribe2 = (FWPMNES2)GetProcAddress (hmodule, "FwpmNetEventSubscribe2"); // win10

			if (!_FwpmNetEventSubscribe2)
			{
				_FwpmNetEventSubscribe1 = (FWPMNES1)GetProcAddress (hmodule, "FwpmNetEventSubscribe1"); // win8

				if (!_FwpmNetEventSubscribe1)
					_FwpmNetEventSubscribe0 = (FWPMNES0)GetProcAddress (hmodule, "FwpmNetEventSubscribe0"); // win7
			}

			if (!_FwpmNetEventSubscribe2 && !_FwpmNetEventSubscribe1 && !_FwpmNetEventSubscribe0)
			{
				_app_logerror (L"GetProcAddress", GetLastError (), L"FwpmNetEventSubscribe");
			}
			else
			{
				FWPM_NET_EVENT_SUBSCRIPTION subscription;
				FWPM_NET_EVENT_ENUM_TEMPLATE enum_template;

				SecureZeroMemory (&subscription, sizeof (subscription));
				SecureZeroMemory (&enum_template, sizeof (enum_template));

				if (config.psession)
					memcpy_s (&subscription.sessionKey, sizeof (GUID), config.psession, sizeof (GUID));

				subscription.enumTemplate = &enum_template;

				DWORD rc = 0;

				if (_FwpmNetEventSubscribe2)
					rc = _FwpmNetEventSubscribe2 (config.hengine, &subscription, _app_logcallback2, nullptr, &config.hevent); // win10

				else if (_FwpmNetEventSubscribe1)
					rc = _FwpmNetEventSubscribe1 (config.hengine, &subscription, _app_logcallback1, nullptr, &config.hevent); // win8

				else if (_FwpmNetEventSubscribe0)
					rc = _FwpmNetEventSubscribe0 (config.hengine, &subscription, _app_logcallback0, nullptr, &config.hevent); // win7

				if (rc != ERROR_SUCCESS)
				{
					_app_logerror (L"FwpmNetEventSubscribe", rc, nullptr);
				}
				else
				{
					_app_loginit (true); // create log file
					result = true;
				}
			}
		}
	}

	return result;
}

bool _wfp_initialize (bool is_full)
{
	bool result = true;
	DWORD rc = 0;

	if (!config.hengine)
	{
		// generate unique session key
		if (!config.psession)
		{
			config.psession = (GUID*)malloc (sizeof (GUID));

			if (config.psession)
			{
				if (CoCreateGuid (config.psession) != S_OK)
				{
					SecureZeroMemory (config.psession, sizeof (GUID));
					free (config.psession);

					config.psession = nullptr;
				}
			}
		}

		SecureZeroMemory (&session, sizeof (session));

		session.displayData.name = APP_NAME;
		session.displayData.description = APP_NAME;

		if (config.psession)
			memcpy_s (&session.sessionKey, sizeof (GUID), config.psession, sizeof (GUID));

		rc = FwpmEngineOpen (nullptr, RPC_C_AUTHN_WINNT, nullptr, &session, &config.hengine);

		if (rc != ERROR_SUCCESS)
		{
			_app_logerror (L"FwpmEngineOpen", rc, nullptr);
			config.hengine = nullptr;
			result = false;
		}
	}

	// set security info
	if (is_full && config.hengine && !config.is_securityinfoset)
	{
		if (config.hengine && config.psid)
		{
			// Add DACL for given user
			PACL pDacl = nullptr;
			PSECURITY_DESCRIPTOR securityDescriptor = nullptr;

			rc = FwpmEngineGetSecurityInfo (config.hengine, DACL_SECURITY_INFORMATION, nullptr, nullptr, &pDacl, nullptr, &securityDescriptor);

			if (rc != ERROR_SUCCESS)
			{
				_app_logerror (L"FwpmEngineGetSecurityInfo", rc, nullptr);
			}
			else
			{
				bool bExists = false;

				// Loop through the ACEs and search for user SID.
				for (WORD cAce = 0; !bExists && cAce < pDacl->AceCount; cAce++)
				{
					ACCESS_ALLOWED_ACE* pAce = nullptr;

					// Get ACE info
					if (!GetAce (pDacl, cAce, (LPVOID*)&pAce))
					{
						_app_logerror (L"GetAce", GetLastError (), nullptr);
						continue;
					}

					if (pAce->Header.AceType != ACCESS_ALLOWED_ACE_TYPE)
						continue;

					if (EqualSid (&pAce->SidStart, config.psid))
						bExists = true;
				}

				if (!bExists)
				{
					PACL pNewDacl = nullptr;
					EXPLICIT_ACCESS ea = {0};

					// Initialize an EXPLICIT_ACCESS structure for the new ACE.
					SecureZeroMemory (&ea, sizeof (ea));

					ea.grfAccessPermissions = FWPM_GENERIC_WRITE | FWPM_GENERIC_EXECUTE | FWPM_GENERIC_READ | DELETE | WRITE_DAC | WRITE_OWNER;
					ea.grfAccessMode = GRANT_ACCESS;
					ea.grfInheritance = NO_INHERITANCE;
					ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
					ea.Trustee.TrusteeType = TRUSTEE_IS_USER;
					ea.Trustee.ptstrName = (LPTSTR)config.psid;
					//ea.Trustee.ptstrName = user;

					// Create a new ACL that merges the new ACE
					// into the existing DACL.
					rc = SetEntriesInAcl (1, &ea, pDacl, &pNewDacl);

					if (rc != ERROR_SUCCESS)
					{
						_app_logerror (L"SetEntriesInAcl", rc, nullptr);
					}
					else
					{
						rc = FwpmEngineSetSecurityInfo (config.hengine, DACL_SECURITY_INFORMATION, nullptr, nullptr, pNewDacl, nullptr);

						if (rc != ERROR_SUCCESS)
						{
							_app_logerror (L"FwpmEngineSetSecurityInfo", rc, nullptr);
						}
						else
						{
							config.is_securityinfoset = true;
						}
					}

					if (pNewDacl)
						LocalFree (pNewDacl);
				}
			}
		}
	}

	// dropped packets logging (win7 and above)
	if (is_full && config.hengine && !config.hevent && _r_sys_validversion (6, 1))
	{
		FWP_VALUE val;
		SecureZeroMemory (&val, sizeof (val));

		val.type = FWP_UINT32;
		val.uint32 = 1;

		rc = FwpmEngineSetOption (config.hengine, FWPM_ENGINE_COLLECT_NET_EVENTS, &val);

		if (rc != ERROR_SUCCESS)
		{
			_app_logerror (L"FwpmEngineSetOption", rc, L"FWPM_ENGINE_COLLECT_NET_EVENTS");
		}
	}

	if (is_full && config.hengine && !_wfp_isfiltersinstalled ())
	{
		rc = FwpmTransactionBegin (config.hengine, 0);

		if (rc != ERROR_SUCCESS)
		{
			_app_logerror (L"FwpmTransactionBegin", rc, nullptr);
			result = false;
		}
		else
		{
			// create provider
			FWPM_PROVIDER provider = {0};

			provider.displayData.name = APP_NAME;
			provider.displayData.description = APP_NAME;

			provider.providerKey = GUID_WfpProvider;
			provider.flags = FWPM_PROVIDER_FLAG_PERSISTENT;

			rc = FwpmProviderAdd (config.hengine, &provider, nullptr);

			if (rc != ERROR_SUCCESS && rc != FWP_E_ALREADY_EXISTS)
			{
				_app_logerror (L"FwpmProviderAdd", rc, nullptr);
				FwpmTransactionAbort (config.hengine);
				result = false;
			}
			else
			{
				FWPM_SUBLAYER sublayer = {0};

				sublayer.displayData.name = APP_NAME;
				sublayer.displayData.description = APP_NAME;

				sublayer.providerKey = (LPGUID)&GUID_WfpProvider;
				sublayer.subLayerKey = GUID_WfpSublayer;
				sublayer.flags = FWPM_SUBLAYER_FLAG_PERSISTENT;
				sublayer.weight = (UINT16)app.ConfigGet (L"SublayerWeight", 0x0000ffff).AsUint (); // highest weight "65535"

				rc = FwpmSubLayerAdd (config.hengine, &sublayer, nullptr);

				if (rc != ERROR_SUCCESS && rc != FWP_E_ALREADY_EXISTS)
				{
					_app_logerror (L"FwpmSubLayerAdd", rc, nullptr);
					FwpmTransactionAbort (config.hengine);
					result = false;
				}
				else
				{
					FwpmTransactionCommit (config.hengine);
					result = true;
				}
			}
		}
	}

	return result;
}

void _wfp_uninitialize (bool is_force)
{
	DWORD result = 0;

	if (config.hengine)
	{
		if (is_force)
		{
			result = FwpmTransactionBegin (config.hengine, 0);

			if (result != ERROR_SUCCESS)
			{
				_app_logerror (L"FwpmTransactionBegin", result, nullptr);
			}
			else
			{
				// destroy callouts (deprecated)
				{
					const GUID callouts[] = {
						GUID_WfpOutboundCallout4_DEPRECATED,
						GUID_WfpOutboundCallout6_DEPRECATED,
						GUID_WfpInboundCallout4_DEPRECATED,
						GUID_WfpInboundCallout6_DEPRECATED,
						GUID_WfpListenCallout4_DEPRECATED,
						GUID_WfpListenCallout6_DEPRECATED
					};

					for (UINT i = 0; i < _countof (callouts); i++)
						FwpmCalloutDeleteByKey (config.hengine, &callouts[i]);
				}

				// destroy sublayer
				result = FwpmSubLayerDeleteByKey (config.hengine, &GUID_WfpSublayer);

				if (result != ERROR_SUCCESS && result != FWP_E_SUBLAYER_NOT_FOUND)
					_app_logerror (L"FwpmSubLayerDeleteByKey", result, nullptr);

				// destroy provider
				result = FwpmProviderDeleteByKey (config.hengine, &GUID_WfpProvider);

				if (result != ERROR_SUCCESS && result != FWP_E_PROVIDER_NOT_FOUND)
					_app_logerror (L"FwpmProviderDeleteByKey", result, nullptr);

				FwpmTransactionCommit (config.hengine);
			}
		}

		// dropped packets logging (win7 and above)
		if (_r_sys_validversion (6, 1))
		{
			_wfp_logunsubscribe ();

			FWP_VALUE val;
			SecureZeroMemory (&val, sizeof (val));

			val.type = FWP_UINT32;
			val.uint32 = 0;

			result = FwpmEngineSetOption (config.hengine, FWPM_ENGINE_COLLECT_NET_EVENTS, &val);

			if (result != ERROR_SUCCESS)
				_app_logerror (L"FwpmEngineSetOption", result, L"FWPM_ENGINE_COLLECT_NET_EVENTS");
		}

		FwpmEngineClose (config.hengine);
		config.hengine = nullptr;
	}

	if (config.psession)
	{
		free (config.psession);
		config.psession = nullptr;
	}

	config.is_securityinfoset = false;
}

void DrawFrameBorder (HDC dc, HWND hwnd, COLORREF clr)
{
	RECT rc = {0};
	GetWindowRect (hwnd, &rc);

	HPEN hpen = CreatePen (PS_INSIDEFRAME, GetSystemMetrics (SM_CXBORDER), clr);

	HPEN old_pen = (HPEN)SelectObject (dc, hpen);
	HBRUSH old_brush = (HBRUSH)SelectObject (dc, GetStockObject (NULL_BRUSH));

	Rectangle (dc, 0, 0, (rc.right - rc.left), (rc.bottom - rc.top));

	SelectObject (dc, old_pen);
	SelectObject (dc, old_brush);

	DeleteObject (hpen);
}

LRESULT CALLBACK NotificationProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
		case WM_MOUSEMOVE:
		{
			if (config.is_notifytimeout)
				break;

			config.is_notifymouse = true;
			_app_notifysettimeout (hwnd, 0, false, 0);

			break;
		}

		case WM_MOUSELEAVE:
		{
			if (!config.is_notifymouse || config.is_notifytimeout)
				break;

			config.is_notifymouse = false;

			_app_notifysettimeout (hwnd, 0, false, 0);
			_app_notifysettimeout (hwnd, NOTIFY_TIMER_DISPLAY_ID, true, NOTIFY_TIMER_MOUSE);

			break;
		}

		case WM_ACTIVATE:
		{
			switch (LOWORD (wparam))
			{
				case WA_ACTIVE:
				case WA_CLICKACTIVE:
				{
					_app_notifysettimeout (hwnd, NOTIFY_TIMER_DISPLAY_ID, false, 0);
					break;
				}

				case WA_INACTIVE:
				{
					_app_notifysettimeout (hwnd, NOTIFY_TIMER_DISPLAY_ID, true, 0);
					break;
				}
			}

			break;
		}

		case WM_SIZE:
		{
			InvalidateRect (hwnd, nullptr, FALSE);
			break;
		}

		case WM_TIMER:
		{
			if (config.is_notifytimeout && wparam != NOTIFY_TIMER_TIMEOUT_ID)
				break;

			if (wparam == NOTIFY_TIMER_MOUSELEAVE_ID)
			{
				if (config.is_notifymouse && !_r_wnd_undercursor (hwnd))
					PostMessage (hwnd, WM_MOUSELEAVE, 0, 0);

				break;
			}

			if (wparam == NOTIFY_TIMER_TIMEOUT_ID)
			{
				if (_r_wnd_undercursor (hwnd))
				{
					_app_notifysettimeout (hwnd, NOTIFY_TIMER_TIMEOUT_ID, false, 0);
					break;
				}
			}

			if (
				wparam == NOTIFY_TIMER_DISPLAY_ID ||
				wparam == NOTIFY_TIMER_TIMEOUT_ID
				)
			{
				_app_notifysettimeout (hwnd, 0, false, 0);
				ShowWindow (hwnd, SW_HIDE);
			}

			break;
		}

		case WM_CLOSE:
		{
			_app_notifysettimeout (hwnd, 0, false, 0);
			ShowWindow (hwnd, SW_HIDE);

			SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
			return TRUE;
		}

		case WM_KEYDOWN:
		{
			switch (wparam)
			{
				case VK_ESCAPE:
				{
					_app_notifysettimeout (hwnd, 0, false, 0);
					ShowWindow (hwnd, SW_HIDE);

					break;
				}
			}

			break;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps = {0};
			HDC dc = BeginPaint (hwnd, &ps);

			RECT rc = {0};
			GetClientRect (hwnd, &rc);

			static INT bottom = app.GetDPI (48);

			rc.left = GetSystemMetrics (SM_CXBORDER);
			rc.right -= (rc.left * 2);
			rc.top = rc.bottom - bottom;
			rc.bottom = rc.top + bottom;

			_r_dc_fillrect (dc, &rc, GetSysColor (COLOR_BTNFACE));

			for (INT i = 0; i < rc.right; i++)
				SetPixel (dc, i, rc.top, GetSysColor (COLOR_APPWORKSPACE));

			DrawFrameBorder (dc, hwnd, GetSysColor (COLOR_HIGHLIGHT));

			EndPaint (hwnd, &ps);

			break;
		}

		case WM_CTLCOLORSTATIC:
		{
			const UINT ctrl_id = GetDlgCtrlID ((HWND)lparam);

			if (
				ctrl_id == IDC_TITLE_ID ||
				ctrl_id == IDC_CLOSE_BTN ||
				ctrl_id == IDC_FILE_ID ||
				ctrl_id == IDC_ADDRESS_ID ||
				ctrl_id == IDC_FILTER_ID ||
				ctrl_id == IDC_DATE_ID ||
				ctrl_id == IDC_CREATERULE_ADDR_ID ||
				ctrl_id == IDC_CREATERULE_PORT_ID ||
				ctrl_id == IDC_DISABLENOTIFY_ID
				)
			{
				SetBkMode ((HDC)wparam, TRANSPARENT); // background-hack

				return (INT_PTR)GetSysColorBrush (COLOR_WINDOW);
			}
			else if (GetDlgCtrlID ((HWND)lparam) == IDC_ICON_ID)
			{
				HBRUSH hbrush = nullptr;
				const size_t idx = _app_notifygetcurrent ();

				if (idx != LAST_VALUE)
				{
					ITEM_LOG const* ptr_log = notifications.at (idx);

					if (ptr_log)
					{
						const HBRUSH new_brush = (HBRUSH)_app_getcolor (ptr_log->hash, true);

						if (new_brush)
							hbrush = new_brush;
					}
				}

				if (!hbrush)
					hbrush = GetSysColorBrush (COLOR_BTNSHADOW);

				return (INT_PTR)hbrush;
			}

			break;
		}

		case WM_SETCURSOR:
		{
			const UINT ctrl_id = GetDlgCtrlID ((HWND)wparam);

			if (ctrl_id == IDC_ICON_ID || ctrl_id == IDC_CLOSE_BTN)
			{
				SetCursor (LoadCursor (nullptr, IDC_HAND));

				SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
				return TRUE;
			}

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lparam;

			switch (nmlp->code)
			{
				case TTN_GETDISPINFO:
				{
					LPNMTTDISPINFO lpnmdi = (LPNMTTDISPINFO)lparam;

					if ((lpnmdi->uFlags & TTF_IDISHWND) != 0)
					{
						WCHAR buffer[1024] = {0};
						const UINT ctrl_id = GetDlgCtrlID ((HWND)lpnmdi->hdr.idFrom);

						if (
							ctrl_id == IDC_ICON_ID ||
							ctrl_id == IDC_FILE_ID ||
							ctrl_id == IDC_ADDRESS_ID ||
							ctrl_id == IDC_FILTER_ID ||
							ctrl_id == IDC_DATE_ID
							)
						{
							const size_t idx = _app_notifygetcurrent ();

							if (idx != LAST_VALUE)
							{
								ITEM_LOG const* ptr_log = notifications.at (idx);

								if (ptr_log)
								{
									ITEM_APPLICATION* ptr_app = _app_getapplication (ptr_log->hash);

									if (ptr_app)
									{
										if (ctrl_id == IDC_ICON_ID)
										{
											StringCchCopy (buffer, _countof (buffer), _app_gettooltip (ptr_log->hash));
										}
										else if (ctrl_id == IDC_FILE_ID)
										{
											StringCchPrintf (buffer, _countof (buffer), L"%s: %s [%s]", I18N (&app, IDS_FILE, 0), ptr_app->real_path, ptr_app->is_signed ? I18N (&app, IDS_SIGN_SIGNED, 0) : I18N (&app, IDS_SIGN_UNSIGNED, 0));
										}
										else if (ctrl_id == IDC_ADDRESS_ID)
										{
											WCHAR remote_addr[LEN_IP_MAX] = {0};
											WCHAR local_addr[LEN_IP_MAX] = {0};

											if ((ptr_log->flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET) != 0)
											{
												StringCchCopy (remote_addr, _countof (remote_addr), ptr_log->remote_addr);

												if (ptr_log->remote_port)
													StringCchCat (remote_addr, _countof (remote_addr), _r_fmt (L":%d", ptr_log->remote_port));
											}
											else
											{
												StringCchCopy (local_addr, _countof (local_addr), I18N (&app, IDS_STATUS_EMPTY, 0));
											}

											if ((ptr_log->flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET) != 0)
											{
												StringCchCopy (local_addr, _countof (local_addr), ptr_log->local_addr);

												if (ptr_log->local_port)
													StringCchCat (local_addr, _countof (local_addr), _r_fmt (L":%d", ptr_log->local_port));
											}
											else
											{
												StringCchCopy (local_addr, _countof (local_addr), I18N (&app, IDS_STATUS_EMPTY, 0));
											}

											StringCchPrintf (buffer, _countof (buffer), L"%s (Remote): %s\r\n%s (Local): %s\r\n", I18N (&app, IDS_ADDRESS, 0), remote_addr, I18N (&app, IDS_ADDRESS, 0), local_addr);
										}
										else
										{
											StringCchCopy (buffer, _countof (buffer), _r_ctrl_gettext (hwnd, ctrl_id));
										}

										lpnmdi->lpszText = buffer;
									}
								}
							}
						}
						else if (ctrl_id == IDC_CREATERULE_ADDR_ID || ctrl_id == IDC_CREATERULE_PORT_ID)
						{
							StringCchCopy (buffer, _countof (buffer), I18N (&app, IDS_NOTIFY_TOOLTIP, 0));

							lpnmdi->lpszText = buffer;
						}
					}

					break;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			switch (LOWORD (wparam))
			{
				case IDC_ICON_ID:
				{
					const size_t idx = _app_notifygetcurrent ();

					if (idx != LAST_VALUE)
					{
						ShowItem (app.GetHWND (), IDC_LISTVIEW, _app_getposition (app.GetHWND (), notifications.at (idx)->hash), -1);

						_r_wnd_toggle (app.GetHWND (), true);
					}

					_app_notifysettimeout (hwnd, 0, false, 0);
					ShowWindow (hwnd, SW_HIDE);

					break;
				}

				case IDC_PREV_ID:
				{
					const size_t current_idx = _app_notifygetcurrent ();

					if (current_idx != LAST_VALUE)
					{
						const size_t count = notifications.size ();

						if (!current_idx)
						{
							_app_notifyshow (count - 1);
						}
						else
						{
							const size_t idx = (size_t)current_idx - 1;

							if (idx >= 0 && idx <= (count - 1))
								_app_notifyshow (idx);
						}
					}

					break;
				}

				case IDC_NEXT_ID:
				{
					const size_t current_idx = _app_notifygetcurrent ();

					if (current_idx != LAST_VALUE)
					{
						const size_t count = notifications.size ();

						if (current_idx >= count - 1)
						{
							_app_notifyshow (0);
						}
						else
						{
							const size_t idx = (size_t)current_idx + 1;

							if (idx >= 0 && idx <= (count - 1))
								_app_notifyshow (idx);
						}
					}

					break;
				}

				case IDC_ALLOW_BTN:
				{
					_app_notifycommand (hwnd, false);
					break;
				}

				case IDC_BLOCK_BTN:
				{
					_app_notifycommand (hwnd, true);
					break;
				}

				case IDC_CLOSE_BTN:
				{
					_app_notifysettimeout (hwnd, 0, false, 0);
					ShowWindow (hwnd, SW_HIDE);
					break;
				}
			}

			break;
		}
	}

	return DefWindowProc (hwnd, msg, wparam, lparam);
}

INT_PTR CALLBACK DlgProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_FINDMSGSTRING)
	{
		LPFINDREPLACE const lpfr = (LPFINDREPLACE)lparam;

		if ((lpfr->Flags & FR_DIALOGTERM) != 0)
		{
			config.hfind = nullptr;
		}
		else if ((lpfr->Flags & FR_FINDNEXT) != 0)
		{
			const size_t total = _r_listview_getitemcount (hwnd, IDC_LISTVIEW);
			const INT start = (INT)SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_GETNEXTITEM, (WPARAM)total - 1, LVNI_SELECTED | LVNI_DIRECTIONMASK | LVNI_BELOW) + 1;

			for (size_t i = start; i < total; i++)
			{
				const rstring text = _r_listview_getitemtext (hwnd, IDC_LISTVIEW, i, 0);

				if (StrStrI (text, lpfr->lpstrFindWhat) != nullptr)
				{
					ShowItem (hwnd, IDC_LISTVIEW, i, 0);
					SetFocus (hwnd);
					break;
				}
			}
		}

		return FALSE;
	}

	switch (msg)
	{
		case WM_INITDIALOG:
		{

#ifndef _WIN64
			if (_r_sys_iswow64 ())
				Wow64EnableWow64FsRedirection (FALSE);
#endif

			// static initializer
			config.wd_length = GetWindowsDirectory (config.windows_dir, _countof (config.windows_dir));
			StringCchPrintf (config.apps_path, _countof (config.apps_path), L"%s\\" XML_APPS, app.GetProfileDirectory ());
			StringCchPrintf (config.rules_config_path, _countof (config.rules_config_path), L"%s\\" XML_RULES_CONFIG, app.GetProfileDirectory ());
			StringCchPrintf (config.rules_custom_path, _countof (config.rules_custom_path), L"%s\\" XML_RULES_CUSTOM, app.GetProfileDirectory ());

			config.ntoskrnl_hash = _r_str_hash (PROC_SYSTEM_NAME);

			// set privileges
			{
				LPCWSTR privileges[] = {
					SE_BACKUP_NAME,
					SE_DEBUG_NAME,
					SE_SECURITY_NAME,
					SE_TAKE_OWNERSHIP_NAME,
				};

				_r_sys_setprivilege (privileges, _countof (privileges), true);
			}

			// get current user security identifier
			if (!config.psid)
			{
				// get user sid
				HANDLE token = nullptr;
				DWORD token_length = 0;
				PTOKEN_USER token_user = nullptr;

				if (OpenProcessToken (GetCurrentProcess (), TOKEN_QUERY, &token))
				{
					GetTokenInformation (token, TokenUser, nullptr, token_length, &token_length);

					if (GetLastError () == ERROR_INSUFFICIENT_BUFFER)
					{
						token_user = (PTOKEN_USER)malloc (token_length);

						if (token_user)
						{
							if (GetTokenInformation (token, TokenUser, token_user, token_length, &token_length))
							{
								SID_NAME_USE sid_type;

								WCHAR username[MAX_PATH] = {0};
								WCHAR domain[MAX_PATH] = {0};

								DWORD length1 = _countof (username);
								DWORD length2 = _countof (domain);

								if (LookupAccountSid (nullptr, token_user->User.Sid, username, &length1, domain, &length2, &sid_type))
								{
									StringCchPrintf (config.title, _countof (config.title), L"%s [%s\\%s]", APP_NAME, domain, username);
								}

								config.psid = (SID*)malloc (SECURITY_MAX_SID_SIZE);

								if (config.psid)
								{
									SecureZeroMemory (config.psid, SECURITY_MAX_SID_SIZE);

									memcpy_s (config.psid, SECURITY_MAX_SID_SIZE, token_user->User.Sid, SECURITY_MAX_SID_SIZE);
								}
							}
						}

						free (token_user);
					}

					CloseHandle (token);
				}

				if (!config.title[0])
					StringCchCopy (config.title, _countof (config.title), APP_NAME); // fallback
			}

			// configure listview
			_r_listview_setstyle (hwnd, IDC_LISTVIEW, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_CHECKBOXES);

			_r_listview_addcolumn (hwnd, IDC_LISTVIEW, 0, I18N (&app, IDS_FILEPATH, 0), 70, LVCFMT_LEFT);
			_r_listview_addcolumn (hwnd, IDC_LISTVIEW, 1, I18N (&app, IDS_ADDED, 0), 26, LVCFMT_LEFT);

			_r_listview_addgroup (hwnd, IDC_LISTVIEW, 0, I18N (&app, IDS_GROUP_ALLOWED, 0), 0, LVGS_COLLAPSIBLE | (app.ConfigGet (L"Group1IsCollaped", false).AsBool () ? LVGS_COLLAPSED : 0));
			_r_listview_addgroup (hwnd, IDC_LISTVIEW, 1, I18N (&app, IDS_GROUP_BLOCKED, 0), 0, LVGS_COLLAPSIBLE | (app.ConfigGet (L"Group2IsCollaped", false).AsBool () ? LVGS_COLLAPSED : 0));

			_app_listviewsetimagelist (hwnd, IDC_LISTVIEW);
			_app_listviewsetfont (hwnd, IDC_LISTVIEW);

			// load settings imagelist
			{
				const INT cx = GetSystemMetrics (SM_CXSMICON);

				config.himg = ImageList_Create (cx, cx, ILC_COLOR32 | ILC_MASK, 0, 5);

				HICON hico = _r_loadicon (app.GetHINSTANCE (), MAKEINTRESOURCE (IDI_ALLOW), cx);
				ImageList_ReplaceIcon (config.himg, -1, hico);
				DestroyIcon (hico);

				hico = _r_loadicon (app.GetHINSTANCE (), MAKEINTRESOURCE (IDI_BLOCK), cx);
				ImageList_ReplaceIcon (config.himg, -1, hico);
				DestroyIcon (hico);
			}

			// get default icon for executable
			_app_getfileicon (_r_path_expand (PATH_NTOSKRNL), false, &config.icon_id, &config.hicon_large);
			_app_getfileicon (_r_path_expand (PATH_NTOSKRNL), true, &config.icon_id, &config.hicon_small);

			// get default icon for windows store package (win8 and above)
			if (_r_sys_validversion (6, 2))
			{
				if (!_app_getfileicon (_r_path_expand (PATH_STORE), false, nullptr, &config.hicon_package))
					config.hicon_package = config.hicon_large;

				HICON hicon = nullptr;

				if (_app_getfileicon (_r_path_expand (PATH_STORE), true, &config.icon_package_id, &hicon))
				{
					config.hbitmap_package_small = _app_ico2bmp (hicon);
					DestroyIcon (hicon);
				}
				else
				{
					config.hbitmap_package_small = _app_ico2bmp (config.hicon_small);
					config.icon_package_id = config.icon_id;
				}
			}

			// drag & drop support
			DragAcceptFiles (hwnd, TRUE);

			// settings
			app.AddSettingsPage (nullptr, IDD_SETTINGS_GENERAL, IDS_SETTINGS_1, L"IDS_SETTINGS_1", &settings_callback);
			app.AddSettingsPage (nullptr, IDD_SETTINGS_INTERFACE, IDS_SETTINGS_3, L"IDS_SETTINGS_3", &settings_callback);

			{
				const size_t page_id = app.AddSettingsPage (nullptr, IDD_SETTINGS_FILTERS, IDS_TRAY_FILTERS, L"IDS_TRAY_FILTERS", &settings_callback);

				app.AddSettingsPage (nullptr, IDD_SETTINGS_RULES_BLOCKLIST, IDS_TRAY_BLOCKLIST_RULES, L"IDS_TRAY_BLOCKLIST_RULES", &settings_callback, page_id);
				app.AddSettingsPage (nullptr, IDD_SETTINGS_RULES_SYSTEM, IDS_TRAY_SYSTEM_RULES, L"IDS_TRAY_SYSTEM_RULES", &settings_callback, page_id);
				app.AddSettingsPage (nullptr, IDD_SETTINGS_RULES_CUSTOM, IDS_TRAY_CUSTOM_RULES, L"IDS_TRAY_CUSTOM_RULES", &settings_callback, page_id);
			}

			app.AddSettingsPage (nullptr, IDD_SETTINGS_LOG, IDS_TRAY_LOG, L"IDS_TRAY_LOG", &settings_callback);

			// load colors
			{
				colors.clear ();

				addcolor (L"IDS_HIGHLIGHT_INVALID", IDS_HIGHLIGHT_INVALID, L"IsHighlightInvalid", true, L"ColorInvalid", LISTVIEW_COLOR_INVALID);
				addcolor (L"IDS_HIGHLIGHT_NETWORK", IDS_HIGHLIGHT_NETWORK, L"IsHighlightNetwork", true, L"ColorNetwork", LISTVIEW_COLOR_NETWORK);
				addcolor (L"IDS_HIGHLIGHT_PACKAGE", IDS_HIGHLIGHT_PACKAGE, L"IsHighlightPackage", true, L"ColorPackage", LISTVIEW_COLOR_PACKAGE);
				addcolor (L"IDS_HIGHLIGHT_PICO", IDS_HIGHLIGHT_PICO, L"IsHighlightPico", true, L"ColorPico", LISTVIEW_COLOR_PICO);
				addcolor (L"IDS_HIGHLIGHT_SIGNED", IDS_HIGHLIGHT_SIGNED, L"IsHighlightSigned", true, L"ColorSigned", LISTVIEW_COLOR_SIGNED);
				addcolor (L"IDS_HIGHLIGHT_SILENT", IDS_HIGHLIGHT_SILENT, L"IsHighlightSilent", true, L"ColorSilent", LISTVIEW_COLOR_SILENT);
				addcolor (L"IDS_HIGHLIGHT_SPECIAL", IDS_HIGHLIGHT_SPECIAL, L"IsHighlightSpecial", true, L"ColorSpecial", LISTVIEW_COLOR_SPECIAL);
				addcolor (L"IDS_HIGHLIGHT_SYSTEM", IDS_HIGHLIGHT_SYSTEM, L"IsHighlightSystem", true, L"ColorSystem", LISTVIEW_COLOR_SYSTEM);
			}

			// load protocols
			{
				protocols.clear ();

				addprotocol (L"ICMP", IPPROTO_ICMP);
				addprotocol (L"ICMPv6", IPPROTO_ICMPV6);
				addprotocol (L"IGMP", IPPROTO_IGMP);
				addprotocol (L"IPv4", IPPROTO_IPV4);
				addprotocol (L"IPv6", IPPROTO_IPV6);
				addprotocol (L"L2TP", IPPROTO_L2TP);
				addprotocol (L"RAW", IPPROTO_RAW);
				addprotocol (L"RDP", IPPROTO_RDP);
				addprotocol (L"SCTP", IPPROTO_SCTP);
				addprotocol (L"TCP", IPPROTO_TCP);
				addprotocol (L"UDP", IPPROTO_UDP);
			}

			// initialize thread objects
			config.stop_evt = CreateEvent (nullptr, FALSE, FALSE, nullptr);
			config.finish_evt = CreateEvent (nullptr, FALSE, FALSE, nullptr);

			WTSRegisterSessionNotification (hwnd, NOTIFY_FOR_THIS_SESSION);

			// create notification window (win7 and above)
			if (_r_sys_validversion (6, 1))
				_app_notifycreatewindow ();

			// load profile
			_app_profileload (hwnd);
			_app_listviewsort (hwnd, IDC_LISTVIEW, -1, false);

			// install filters
			if (_wfp_isfiltersinstalled ())
				_app_installfilters (true);

			break;
		}

		case WM_DROPFILES:
		{
			UINT numfiles = DragQueryFile ((HDROP)wparam, 0xFFFFFFFF, nullptr, 0);
			size_t item = 0;

			_r_fastlock_acquireexclusive (&lock_access);

			for (UINT i = 0; i < numfiles; i++)
			{
				const UINT length = DragQueryFile ((HDROP)wparam, i, nullptr, 0);

				LPWSTR file = (LPWSTR)malloc ((length + 1) * sizeof (WCHAR));

				if (file)
				{
					DragQueryFile ((HDROP)wparam, i, file, length + 1);

					item = _app_addapplication (hwnd, file, 0, false, false, false);

					free (file);
				}
			}

			_r_fastlock_releaseexclusive (&lock_access);

			_app_listviewsort (hwnd, IDC_LISTVIEW, -1, false);
			_app_profilesave (hwnd);

			ShowItem (hwnd, IDC_LISTVIEW, _app_getposition (hwnd, item), -1);

			DragFinish ((HDROP)wparam);

			break;
		}

		case WM_CLOSE:
		{
			if (!messageFlag (hwnd, L"ConfirmExit", I18N (&app, IDS_QUESTION_EXIT, 0), I18N (&app, IDS_QUESTION_FLAG_CHK, 0)))
				return true;

			DestroyWindow (hwnd);

			break;
		}

		case WM_DESTROY:
		{
			app.TrayDestroy (UID);

			DestroyWindow (config.hnotification);
			UnregisterClass (NOTIFY_CLASS_DLG, app.GetHINSTANCE ());

			ImageList_Destroy (config.himg);

			app.ConfigSet (L"Group1IsCollaped", ((SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_GETGROUPSTATE, 0, LVGS_COLLAPSED) & LVGS_COLLAPSED) != 0) ? true : false);
			app.ConfigSet (L"Group2IsCollaped", ((SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_GETGROUPSTATE, 1, LVGS_COLLAPSED) & LVGS_COLLAPSED) != 0) ? true : false);

			if (_r_fastlock_islocked (&lock_apply))
			{
				WaitForSingleObjectEx (config.finish_evt, 8000, FALSE);
			}

			_wfp_uninitialize (false);

			_app_profilesave (hwnd);

			PostQuitMessage (0);

			break;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps = {0};
			HDC dc = BeginPaint (hwnd, &ps);

			RECT rc = {0};
			GetWindowRect (GetDlgItem (hwnd, IDC_LISTVIEW), &rc);

			for (INT i = 0; i < rc.right; i++)
				SetPixel (dc, i, rc.bottom - rc.top, GetSysColor (COLOR_APPWORKSPACE));

			EndPaint (hwnd, &ps);

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lparam;

			switch (nmlp->code)
			{
				case NM_CUSTOMDRAW:
				{
					if (nmlp->idFrom != IDC_LISTVIEW)
						break;

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, _app_wmcustdraw ((LPNMLVCUSTOMDRAW)lparam, 0));
					return TRUE;
				}

				case LVN_COLUMNCLICK:
				{
					LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lparam;

					_app_listviewsort (hwnd, IDC_LISTVIEW, pnmv->iSubItem, true);

					break;
				}

				case LVN_GETINFOTIP:
				{
					LPNMLVGETINFOTIP lpnmlv = (LPNMLVGETINFOTIP)lparam;

					const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, (UINT)lpnmlv->hdr.idFrom, lpnmlv->iItem);

					if (hash)
					{
						StringCchCopy (lpnmlv->pszText, lpnmlv->cchTextMax, _app_gettooltip (hash));
					}

					break;
				}

				case LVN_ITEMCHANGED:
				{
					LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lparam;

					if (lpnmlv->uNewState == 8192 || lpnmlv->uNewState == 4096)
					{
						if (config.is_nocheckboxnotify)
							return FALSE;

						const size_t hash = lpnmlv->lParam;
						ITEM_APPLICATION* ptr_app = _app_getapplication (hash);

						if (!ptr_app)
							return FALSE;

						ptr_app->is_enabled = (lpnmlv->uNewState == 8192) ? true : false;

						_r_listview_setitem (hwnd, IDC_LISTVIEW, lpnmlv->iItem, 0, nullptr, LAST_VALUE, ptr_app->is_enabled ? 0 : 1);

						_app_freenotify (LAST_VALUE, hash);
						_app_notifyrefresh ();

						_app_installfilters (false);
					}
					else if (((lpnmlv->uNewState ^ lpnmlv->uOldState) & LVIS_SELECTED) != 0)
					{
						_app_refreshstatus (hwnd, true, false);
					}

					break;
				}

				case LVN_GETEMPTYMARKUP:
				{
					NMLVEMPTYMARKUP* lpnmlv = (NMLVEMPTYMARKUP*)lparam;

					lpnmlv->dwFlags = EMF_CENTERED;
					StringCchCopy (lpnmlv->szMarkup, _countof (lpnmlv->szMarkup), I18N (&app, IDS_STATUS_EMPTY, 0));

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
					return TRUE;
				}

				case NM_DBLCLK:
				{
					LPNMITEMACTIVATE lpnmlv = (LPNMITEMACTIVATE)lparam;

					if (lpnmlv->iItem != -1)
						PostMessage (hwnd, WM_COMMAND, MAKELPARAM (IDM_EXPLORE, 0), 0);

					break;
				}
			}

			break;
		}

		case WM_MENUSELECT:
		{
			// show process information in statusbar
			if ((LOWORD (wparam) >= IDM_PROCESS && LOWORD (wparam) <= IDM_PROCESS + processes.size ()) && (GetMenuState ((HMENU)lparam, LOWORD (wparam), MF_BYCOMMAND) != 0xFFFFFFFF))
			{
				if (((HIWORD (wparam) & MF_HILITE) != 0) || ((HIWORD (wparam) & MF_MOUSESELECT) != 0))
				{
					ITEM_PROCESS const *ptr_proc = &processes.at (LOWORD (wparam) - IDM_PROCESS);

					if (ptr_proc)
						_r_status_settext (hwnd, IDC_STATUSBAR, 0, ptr_proc->real_path);
				}
			}
			// show package information in statusbar
			else if ((LOWORD (wparam) >= IDM_PACKAGE && LOWORD (wparam) <= IDM_PACKAGE + packages.size ()) && (GetMenuState ((HMENU)lparam, LOWORD (wparam), MF_BYCOMMAND) != 0xFFFFFFFF))
			{
				if (((HIWORD (wparam) & MF_HILITE) != 0) || ((HIWORD (wparam) & MF_MOUSESELECT) != 0))
				{
					ITEM_PACKAGE const *ptr_package = &packages.at (LOWORD (wparam) - IDM_PACKAGE);

					if (ptr_package)
						_r_status_settext (hwnd, IDC_STATUSBAR, 0, ptr_package->real_path);
				}
			}
			else
			{
				_app_refreshstatus (hwnd, true, false);
			}

			break;
		}

		case WM_CONTEXTMENU:
		{
			if (GetDlgCtrlID ((HWND)wparam) == IDC_LISTVIEW)
			{
				const HMENU menu = LoadMenu (nullptr, MAKEINTRESOURCE (IDM_LISTVIEW));
				const HMENU submenu = GetSubMenu (menu, 0);
				const HMENU submenu1 = GetSubMenu (submenu, 1);
				const HMENU submenu2 = GetSubMenu (submenu, 2);
				const HMENU submenu3 = GetSubMenu (submenu, 4);

				// localize
				app.LocaleMenu (submenu, I18N (&app, IDS_ADD, 0), 0, true, nullptr);
				app.LocaleMenu (submenu, I18N (&app, IDS_ADD_FILE, 0), IDM_ADD_FILE, false, nullptr);
				app.LocaleMenu (submenu, I18N (&app, IDS_ADD_PROCESS, 0), 1, true, nullptr);
				app.LocaleMenu (submenu, I18N (&app, IDS_ADD_PACKAGE, 0), 2, true, nullptr);
				app.LocaleMenu (submenu, I18N (&app, IDS_SETTINGS, 0), 4, true, nullptr);
				app.LocaleMenu (submenu, I18N (&app, IDS_DISABLENOTIFICATIONS, 0), IDM_DISABLENOTIFICATIONS, false, nullptr);
				app.LocaleMenu (submenu, I18N (&app, IDS_ALL, 0), IDM_ALL_PROCESSES, false, nullptr);
				app.LocaleMenu (submenu, I18N (&app, IDS_ALL, 0), IDM_ALL_PACKAGES, false, nullptr);
				app.LocaleMenu (submenu, I18N (&app, IDS_REFRESH, 0), IDM_REFRESH2, false, L"\tF5");
				app.LocaleMenu (submenu, I18N (&app, IDS_EXPLORE, 0), IDM_EXPLORE, false, nullptr);
				app.LocaleMenu (submenu, I18N (&app, IDS_COPY, 0), IDM_COPY, false, L"\tCtrl+C");
				app.LocaleMenu (submenu, I18N (&app, IDS_DELETE, 0), IDM_DELETE, false, L"\tDel");
				app.LocaleMenu (submenu, I18N (&app, IDS_CHECK, 0), IDM_CHECK, false, nullptr);
				app.LocaleMenu (submenu, I18N (&app, IDS_UNCHECK, 0), IDM_UNCHECK, false, nullptr);

				if (!SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_GETSELECTEDCOUNT, 0, 0))
				{
					EnableMenuItem (submenu, 4, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);
					EnableMenuItem (submenu, IDM_EXPLORE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					EnableMenuItem (submenu, IDM_COPY, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					EnableMenuItem (submenu, IDM_DELETE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					EnableMenuItem (submenu, IDM_CHECK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					EnableMenuItem (submenu, IDM_UNCHECK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
				}

				// generate processes popup menu
				{
					_app_getprocesslist (&processes);

					if (processes.empty ())
					{
						MENUITEMINFO mii = {0};

						rstring buffer = I18N (&app, IDS_STATUS_EMPTY, 0);

						mii.cbSize = sizeof (mii);
						mii.fMask = MIIM_STATE | MIIM_STRING;
						mii.dwTypeData = buffer.GetBuffer ();
						mii.fState = MF_DISABLED | MF_GRAYED;

						SetMenuItemInfo (submenu1, IDM_ALL_PROCESSES, FALSE, &mii);
						buffer.Clear ();
					}
					else
					{
						AppendMenu (submenu1, MF_SEPARATOR, 0, nullptr);

						for (size_t i = 0; i < processes.size (); i++)
						{
							MENUITEMINFO mii = {0};

							mii.cbSize = sizeof (mii);
							mii.fMask = MIIM_ID | MIIM_CHECKMARKS | MIIM_STRING;
							mii.fType = MFT_STRING;
							mii.fState = MFS_DEFAULT;
							mii.dwTypeData = processes.at (i).display_path;
							mii.wID = IDM_PROCESS + UINT (i);
							mii.hbmpChecked = processes.at (i).hbmp;
							mii.hbmpUnchecked = processes.at (i).hbmp;

							InsertMenuItem (submenu1, IDM_PROCESS + UINT (i), FALSE, &mii);
						}
					}
				}

				// generate packages popup menu (win8 and above)
				if (_r_sys_validversion (6, 2))
				{
					size_t total_added = 0;

					if (!packages.empty ())
					{
						for (size_t i = 0; i < packages.size (); i++)
						{
							if (apps.find (packages.at (i).hash) != apps.end ())
								continue;

							if (!total_added)
								AppendMenu (submenu2, MF_SEPARATOR, 1, nullptr);

							MENUITEMINFO mii = {0};

							mii.cbSize = sizeof (mii);
							mii.fMask = MIIM_ID | MIIM_CHECKMARKS | MIIM_STRING;
							mii.fType = MFT_STRING;
							mii.fState = MFS_DEFAULT;
							mii.dwTypeData = packages.at (i).display_name;
							mii.wID = IDM_PACKAGE + UINT (i);
							mii.hbmpChecked = packages.at (i).hbmp;
							mii.hbmpUnchecked = packages.at (i).hbmp;

							InsertMenuItem (submenu2, IDM_PACKAGE + UINT (i), FALSE, &mii);
							total_added += 1;
						}
					}

					if (!total_added)
					{
						MENUITEMINFO mii = {0};

						WCHAR buffer[128] = {0};
						StringCchCopy (buffer, _countof (buffer), I18N (&app, IDS_STATUS_EMPTY, 0));

						mii.cbSize = sizeof (mii);
						mii.fMask = MIIM_STATE | MIIM_STRING;
						mii.dwTypeData = buffer;
						mii.fState = MF_DISABLED | MF_GRAYED;

						SetMenuItemInfo (submenu2, IDM_ALL_PACKAGES, FALSE, &mii);
					}
				}
				else
				{
					DeleteMenu (submenu, 2, MF_BYPOSITION);
				}

				// show configuration
				{
					const size_t item = (size_t)SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED); // get first item
					const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, IDC_LISTVIEW, item);

					_r_fastlock_acquireshared (&lock_access);

					ITEM_APPLICATION const* ptr_app = _app_getapplication (hash);

					if (ptr_app)
					{
						CheckMenuItem (submenu, IDM_DISABLENOTIFICATIONS, MF_BYCOMMAND | (ptr_app->is_silent ? MF_CHECKED : MF_UNCHECKED));

						AppendMenu (submenu3, MF_SEPARATOR, 0, nullptr);

						if (rules_custom.empty ())
						{
							AppendMenu (submenu3, MF_STRING, IDM_RULES_SPECIAL, I18N (&app, IDS_STATUS_EMPTY, 0));
							EnableMenuItem (submenu3, IDM_RULES_SPECIAL, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
						}
						else
						{
							for (size_t i = 0; i < rules_custom.size (); i++)
							{
								ITEM_RULE const* ptr_rule = rules_custom.at (i);

								if (ptr_rule)
								{
									MENUITEMINFO mii = {0};

									bool is_checked = false;

									if (ptr_rule->is_enabled)
									{
										for (size_t j = 0; j < ptr_app->rules.size (); j++)
										{
											if (ptr_app->rules.at (j) == i)
											{
												is_checked = true;
												break;
											}
										}
									}

									WCHAR buffer[128] = {0};
									StringCchPrintf (buffer, _countof (buffer), I18N (&app, IDS_RULE_TITLE_2, 0), ptr_rule->is_block ? I18N (&app, IDS_ACTION_2, 0) : I18N (&app, IDS_ACTION_1, 0), ptr_rule->pname);

									mii.cbSize = sizeof (mii);
									mii.fMask = MIIM_ID | MIIM_STATE | MIIM_STRING;
									mii.fType = MFT_STRING;
									mii.dwTypeData = buffer;
									mii.fState = (is_checked ? MF_CHECKED : MF_UNCHECKED);
									mii.wID = IDM_RULES_SPECIAL + UINT (i);

									InsertMenuItem (submenu3, mii.wID, FALSE, &mii);
								}
							}
						}

						AppendMenu (submenu3, MF_SEPARATOR, 0, nullptr);
						AppendMenu (submenu3, MF_STRING, IDM_OPENRULESEDITOR, I18N (&app, IDS_OPENRULESEDITOR, 0));
					}

					_r_fastlock_releaseshared (&lock_access);
				}

				POINT pt = {0};
				GetCursorPos (&pt);

				TrackPopupMenuEx (submenu, TPM_RIGHTBUTTON | TPM_LEFTBUTTON, pt.x, pt.y, hwnd, nullptr);

				DestroyMenu (menu);
			}

			break;
		}

		case WM_SIZE:
		{
			ResizeWindow (hwnd, LOWORD (lparam), HIWORD (lparam));
			RedrawWindow (hwnd, nullptr, nullptr, RDW_ALLCHILDREN | RDW_ERASE | RDW_INVALIDATE);

			_app_listviewresize (hwnd, IDC_LISTVIEW);

			break;
		}

		case WM_TRAYICON:
		{
			switch (LOWORD (lparam))
			{
				case NIN_POPUPOPEN:
				{
					if (_app_notifyshow (LAST_VALUE))
						_app_notifysettimeout (config.hnotification, NOTIFY_TIMER_DISPLAY_ID, false, 0);

					break;
				}

				case NIN_POPUPCLOSE:

					_app_notifysettimeout (config.hnotification, NOTIFY_TIMER_DISPLAY_ID, true, NOTIFY_TIMER_POPUP);
					break;


				case NIN_BALLOONUSERCLICK:
				{
					if (config.is_popuperrors)
					{
						config.is_popuperrors = false;
						PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDM_TRAY_LOGSHOW_ERR, 0), 0);
					}

					break;
				}

				case NIN_BALLOONHIDE:
				case NIN_BALLOONTIMEOUT:
				{
					config.is_popuperrors = false;
					break;
				}

				case WM_MBUTTONDOWN:
				{
					PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDM_LOGSHOW, 0), 0);
					break;
				}

				case WM_LBUTTONUP:
				{
					SetForegroundWindow (hwnd);
					break;
				}

				case WM_LBUTTONDBLCLK:
				{
					_r_wnd_toggle (hwnd, false);
					break;
				}

				case WM_RBUTTONUP:
				{
					SetForegroundWindow (hwnd); // don't touch

					const HMENU menu = LoadMenu (nullptr, MAKEINTRESOURCE (IDM_TRAY));
					const HMENU submenu = GetSubMenu (menu, 0);

					// localize
					app.LocaleMenu (submenu, I18N (&app, IDS_TRAY_SHOW, 0), IDM_TRAY_SHOW, false, nullptr);

					{
						const bool state = _wfp_isfiltersinstalled ();
						app.LocaleMenu (submenu, I18N (&app, (state ? IDS_TRAY_STOP : IDS_TRAY_START), state ? L"IDS_TRAY_STOP" : L"IDS_TRAY_START"), IDM_TRAY_START, false, nullptr);
					}

					app.LocaleMenu (submenu, I18N (&app, IDS_TRAY_MODE, 0), 3, true, nullptr);
					app.LocaleMenu (submenu, I18N (&app, IDS_MODE_WHITELIST, 0), IDM_TRAY_MODEWHITELIST, false, nullptr);
					app.LocaleMenu (submenu, I18N (&app, IDS_MODE_BLACKLIST, 0), IDM_TRAY_MODEBLACKLIST, false, nullptr);

					app.LocaleMenu (submenu, I18N (&app, IDS_TRAY_LOG, 0), 5, true, nullptr);
					app.LocaleMenu (submenu, I18N (&app, IDS_ENABLELOG_CHK, 0), IDM_TRAY_ENABLELOG_CHK, false, nullptr);
					app.LocaleMenu (submenu, I18N (&app, IDS_ENABLENOTIFICATIONS_CHK, 0), IDM_TRAY_ENABLENOTIFICATIONS_CHK, false, nullptr);
					app.LocaleMenu (submenu, I18N (&app, IDS_LOGSHOW, 0), IDM_TRAY_LOGSHOW, false, nullptr);
					app.LocaleMenu (submenu, I18N (&app, IDS_LOGCLEAR, 0), IDM_TRAY_LOGCLEAR, false, nullptr);

					app.LocaleMenu (submenu, I18N (&app, IDS_TRAY_LOGERR, 0), 6, true, nullptr);

					if (_r_fs_exists (_r_dbg_getpath (APP_NAME_SHORT)))
					{
						app.LocaleMenu (submenu, I18N (&app, IDS_LOGSHOW, 0), IDM_TRAY_LOGSHOW_ERR, false, nullptr);
						app.LocaleMenu (submenu, I18N (&app, IDS_LOGCLEAR, 0), IDM_TRAY_LOGCLEAR_ERR, false, nullptr);
					}
					else
					{
						EnableMenuItem (submenu, 6, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);
					}

					app.LocaleMenu (submenu, I18N (&app, IDS_SETTINGS, 0), IDM_TRAY_SETTINGS, false, L"...");
					app.LocaleMenu (submenu, I18N (&app, IDS_WEBSITE, 0), IDM_TRAY_WEBSITE, false, nullptr);
					app.LocaleMenu (submenu, I18N (&app, IDS_ABOUT, 0), IDM_TRAY_ABOUT, false, nullptr);
					app.LocaleMenu (submenu, I18N (&app, IDS_EXIT, 0), IDM_TRAY_EXIT, false, nullptr);

					CheckMenuItem (submenu, IDM_TRAY_ENABLELOG_CHK, MF_BYCOMMAND | (app.ConfigGet (L"IsLogEnabled", false).AsBool () ? MF_CHECKED : MF_UNCHECKED));
					CheckMenuItem (submenu, IDM_TRAY_ENABLENOTIFICATIONS_CHK, MF_BYCOMMAND | (app.ConfigGet (L"IsNotificationsEnabled", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));

					if (_r_fastlock_islocked (&lock_apply))
						EnableMenuItem (submenu, IDM_TRAY_START, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

					// dropped packets logging (win7 and above)
					if (!_r_sys_validversion (6, 1))
					{
						EnableMenuItem (submenu, IDM_TRAY_ENABLELOG_CHK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
						EnableMenuItem (submenu, IDM_TRAY_ENABLENOTIFICATIONS_CHK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					}

					CheckMenuRadioItem (submenu, IDM_TRAY_MODEWHITELIST, IDM_TRAY_MODEBLACKLIST, IDM_TRAY_MODEWHITELIST + app.ConfigGet (L"Mode", ModeWhitelist).AsUint (), MF_BYCOMMAND);

					POINT pt = {0};
					GetCursorPos (&pt);

					TrackPopupMenuEx (submenu, TPM_RIGHTBUTTON | TPM_LEFTBUTTON, pt.x, pt.y, hwnd, nullptr);

					DestroyMenu (menu);

					break;
				}
			}

			break;
		}

		case WM_WTSSESSION_CHANGE:
		{
			// check for session id
			DWORD session_id = 0;

			ProcessIdToSessionId (GetCurrentProcessId (), &session_id);

			if (session_id != (DWORD)lparam)
				break;

			switch (wparam)
			{
				//case WTS_SESSION_LOGON:
				case WTS_SESSION_UNLOCK:
				{
					app.ConfigInit ();

					_app_profileload (hwnd);

					if (_wfp_isfiltersinstalled ())
					{
						if (_wfp_initialize (true))
							_app_installfilters (true);
					}
					else
					{
						_r_listview_redraw (hwnd, IDC_LISTVIEW);
					}

					break;
				}

				//case WTS_SESSION_LOGOFF:
				case WTS_SESSION_LOCK:
				{
					_app_profilesave (hwnd);
					_wfp_uninitialize (false);

					break;
				}
			}

			break;
		}

		case WM_DEVICECHANGE:
		{
			if (wparam == DBT_DEVICEARRIVAL)
			{
				const PDEV_BROADCAST_HDR lbhdr = (PDEV_BROADCAST_HDR)lparam;

				if (lbhdr->dbch_devicetype == DBT_DEVTYP_VOLUME)
				{
					_app_profilesave (hwnd);
					_app_profileload (hwnd);

					_app_installfilters (false);
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			if (HIWORD (wparam) == 0 && LOWORD (wparam) >= IDM_LANGUAGE && LOWORD (wparam) <= IDM_LANGUAGE + app.LocaleGetCount ())
			{
				app.LocaleApplyFromMenu (GetSubMenu (GetSubMenu (GetMenu (hwnd), 2), LANG_MENU), LOWORD (wparam), IDM_LANGUAGE);

				return FALSE;
			}
			else if ((LOWORD (wparam) >= IDM_PROCESS && LOWORD (wparam) <= IDM_PROCESS + processes.size ()))
			{
				ITEM_PROCESS const* ptr_proc = &processes.at (LOWORD (wparam) - IDM_PROCESS);

				const size_t hash = _app_addapplication (hwnd, ptr_proc->real_path, 0, false, false, true);

				_app_listviewsort (hwnd, IDC_LISTVIEW, -1, false);
				_app_profilesave (hwnd);

				ShowItem (hwnd, IDC_LISTVIEW, _app_getposition (hwnd, hash), -1);

				return FALSE;
			}
			else if ((LOWORD (wparam) >= IDM_PACKAGE && LOWORD (wparam) <= IDM_PACKAGE + packages.size ()))
			{
				ITEM_PACKAGE const* ptr_package = &packages.at (LOWORD (wparam) - IDM_PACKAGE);

				const size_t hash = _app_addapplication (hwnd, ptr_package->sid, 0, false, false, true);

				_app_listviewsort (hwnd, IDC_LISTVIEW, -1, false);
				_app_profilesave (hwnd);

				ShowItem (hwnd, IDC_LISTVIEW, _app_getposition (hwnd, hash), -1);

				return FALSE;
			}
			else if ((LOWORD (wparam) >= IDM_RULES_SPECIAL && LOWORD (wparam) <= IDM_RULES_SPECIAL + rules_custom.size ()))
			{
				const size_t idx = (LOWORD (wparam) - IDM_RULES_SPECIAL);

				INT item = -1;
				BOOL is_remove = (BOOL)-1;
				size_t total_count = 0;

				_r_fastlock_acquireexclusive (&lock_access);

				ITEM_RULE *ptr_rule = rules_custom.at (idx);

				if (!ptr_rule)
					return FALSE;

				if (ptr_rule->papps)
				{
					for (size_t i = 0; i < wcslen (ptr_rule->papps); i++)
					{
						if (_r_str_lower (ptr_rule->papps[i]) == L';')
							total_count += 1;
					}
				}

				while ((item = (INT)SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_GETNEXTITEM, item, LVNI_SELECTED)) != -1)
				{
					const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, IDC_LISTVIEW, item);

					ITEM_APPLICATION *ptr_app = _app_getapplication (hash);

					if (!ptr_app)
						continue;

					if (is_remove == (BOOL)-1)
					{
						is_remove = false;

						if (ptr_rule->is_enabled)
						{
							for (size_t i = 0; i < ptr_app->rules.size (); i++)
							{
								if (ptr_app->rules.at (i) == idx)
								{
									is_remove = true;
									break;
								}
							}
						}
					}

					if (is_remove)
					{
						if (!ptr_app->rules.empty ())
						{
							for (size_t i = ptr_app->rules.size () - 1; i != LAST_VALUE; i--)
							{
								if (ptr_app->rules.at (i) == idx)
									ptr_app->rules.erase (ptr_app->rules.begin () + i);
							}
						}
					}
					else
					{
						ptr_app->rules.push_back (idx);
					}
				}

				if (is_remove)
				{
					if (!total_count)
						ptr_rule->is_enabled = false;
				}
				else
				{
					ptr_rule->is_enabled = true;
				}

				_r_fastlock_releaseexclusive (&lock_access);

				_app_profilesave (hwnd);
				_app_profileload (hwnd); // important!

				_app_installfilters (false);

				return FALSE;
			}

			switch (LOWORD (wparam))
			{
				case IDCANCEL: // process Esc key
				case IDM_TRAY_SHOW:
				{
					_r_wnd_toggle (hwnd, false);
					break;
				}

				case IDM_SETTINGS:
				case IDM_TRAY_SETTINGS:
				case IDC_SETTINGS_BTN:
				{
					app.CreateSettingsWindow ();
					break;
				}

				case IDM_EXIT:
				case IDM_TRAY_EXIT:
				case IDC_EXIT_BTN:
				{
					SendMessage (hwnd, WM_CLOSE, 0, 0);
					break;
				}

				case IDM_WEBSITE:
				case IDM_TRAY_WEBSITE:
				{
					ShellExecute (hwnd, nullptr, _APP_WEBSITE_URL, nullptr, nullptr, SW_SHOWDEFAULT);
					break;
				}

				case IDM_DONATE:
				{
					app.CreateDonateWindow ();
					break;
				}

				case IDM_CHECKUPDATES:
				{
					app.CheckForUpdates (false);
					break;
				}

				case IDM_ABOUT:
				case IDM_TRAY_ABOUT:
				{
					app.CreateAboutWindow ();
					break;
				}

				case IDM_EXPORT_APPS:
				case IDM_EXPORT_RULES:
				{
					WCHAR path[MAX_PATH] = {0};
					StringCchCopy (path, _countof (path), ((LOWORD (wparam) == IDM_EXPORT_APPS) ? XML_APPS : XML_RULES_CUSTOM));

					OPENFILENAME ofn = {0};

					ofn.lStructSize = sizeof (ofn);
					ofn.hwndOwner = hwnd;
					ofn.lpstrFile = path;
					ofn.nMaxFile = _countof (path);
					ofn.lpstrFilter = L"*.xml\0*.xml\0\0";
					ofn.lpstrDefExt = L"xml";
					ofn.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

					if (GetSaveFileName (&ofn))
					{
						_app_profilesave (hwnd, ((LOWORD (wparam) == IDM_EXPORT_APPS) ? path : nullptr), ((LOWORD (wparam) == IDM_EXPORT_RULES) ? path : nullptr));
					}

					break;
				}

				case IDM_IMPORT_APPS:
				case IDM_IMPORT_RULES:
				{
					WCHAR path[MAX_PATH] = {0};
					StringCchCopy (path, _countof (path), ((LOWORD (wparam) == IDM_IMPORT_APPS) ? XML_APPS : XML_RULES_CUSTOM));

					OPENFILENAME ofn = {0};

					ofn.lStructSize = sizeof (ofn);
					ofn.hwndOwner = hwnd;
					ofn.lpstrFile = path;
					ofn.nMaxFile = _countof (path);
					ofn.lpstrFilter = L"*.xml\0*.xml\0\0";
					ofn.lpstrDefExt = L"xml";
					ofn.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_PATHMUSTEXIST | OFN_FORCESHOWHIDDEN;

					if (GetOpenFileName (&ofn))
					{
						// make backup
						if (LOWORD (wparam) == IDM_IMPORT_APPS)
							_r_fs_copy (config.apps_path, _r_fmt (L"%s.bak", config.apps_path));

						else if (LOWORD (wparam) == IDM_IMPORT_RULES)
							_r_fs_copy (config.rules_custom_path, _r_fmt (L"%s.bak", config.rules_custom_path));

						_app_profileload (hwnd, ((LOWORD (wparam) == IDM_IMPORT_APPS) ? path : nullptr), ((LOWORD (wparam) == IDM_IMPORT_RULES) ? path : nullptr));
						_app_profilesave (hwnd);

						_app_installfilters (false);
					}

					break;
				}

				case IDM_ALWAYSONTOP_CHK:
				{
					const bool new_val = !app.ConfigGet (L"AlwaysOnTop", false).AsBool ();

					CheckMenuItem (GetMenu (hwnd), IDM_ALWAYSONTOP_CHK, MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));
					app.ConfigSet (L"AlwaysOnTop", new_val);

					_r_wnd_top (hwnd, new_val);

					break;
				}

				case IDM_SHOWFILENAMESONLY_CHK:
				{
					const bool new_val = !app.ConfigGet (L"ShowFilenames", true).AsBool ();

					CheckMenuItem (GetMenu (hwnd), IDM_SHOWFILENAMESONLY_CHK, MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));
					app.ConfigSet (L"ShowFilenames", new_val);

					_app_profileload (hwnd);
					_app_listviewsort (hwnd, IDC_LISTVIEW, -1, false);

					_r_listview_redraw (hwnd, IDC_LISTVIEW);

					break;
				}

				case IDM_AUTOSIZECOLUMNS_CHK:
				{
					const bool new_val = !app.ConfigGet (L"AutoSizeColumns", true).AsBool ();

					CheckMenuItem (GetMenu (hwnd), IDM_AUTOSIZECOLUMNS_CHK, MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));
					app.ConfigSet (L"AutoSizeColumns", new_val);

					_app_listviewresize (hwnd, IDC_LISTVIEW);

					break;
				}

				case IDM_ICONSSMALL:
				case IDM_ICONSLARGE:
				{
					app.ConfigSet (L"IsLargeIcons", (LOWORD (wparam) == IDM_ICONSLARGE) ? true : false);

					_app_listviewsetimagelist (hwnd, IDC_LISTVIEW);

					break;
				}

				case IDM_ICONSISHIDDEN:
				{
					app.ConfigSet (L"IsIconsHidden", !app.ConfigGet (L"IsIconsHidden", false).AsBool ());

					_app_listviewsetimagelist (hwnd, IDC_LISTVIEW);

					_app_profileload (hwnd);
					_r_listview_redraw (hwnd, IDC_LISTVIEW);

					break;
				}

				case IDM_FONT:
				{
					CHOOSEFONT cf = {0};

					LOGFONT lf = {0};

					cf.lStructSize = sizeof (cf);
					cf.hwndOwner = hwnd;
					cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_NOSCRIPTSEL | CF_LIMITSIZE | CF_NOVERTFONTS;
					cf.nSizeMax = 14;
					cf.nSizeMin = 8;
					cf.lpLogFont = &lf;

					_app_listviewinitfont (&lf);

					if (ChooseFont (&cf))
					{
						if (config.hfont)
						{
							DeleteObject (config.hfont);
							config.hfont = nullptr;
						}

						if (lf.lfFaceName[0])
						{
							app.ConfigSet (L"Font", _r_fmt (L"%s;%d;%d", lf.lfFaceName, _r_dc_fontheighttosize (lf.lfHeight), lf.lfWeight));
						}
						else
						{
							app.ConfigSet (L"Font", UI_FONT_DEFAULT);
						}

						_app_listviewsetfont (hwnd, IDC_LISTVIEW);
					}

					break;
				}

				case IDM_TRAY_MODEWHITELIST:
				case IDM_TRAY_MODEBLACKLIST:
				{
					if (_r_msg (hwnd, MB_YESNO | MB_ICONEXCLAMATION, APP_NAME, nullptr, I18N (&app, IDS_QUESTION, 0)) != IDYES)
						break;

					EnumMode curr = ModeWhitelist;

					if (LOWORD (wparam) == IDM_TRAY_MODEBLACKLIST)
						curr = ModeBlacklist;

					app.ConfigSet (L"Mode", (DWORD)curr);

					_app_refreshstatus (hwnd, false, true);

					CheckMenuRadioItem (GetMenu (hwnd), IDM_TRAY_MODEWHITELIST, IDM_TRAY_MODEBLACKLIST, IDM_TRAY_MODEWHITELIST + curr, MF_BYCOMMAND);

					_app_installfilters (false);

					break;
				}

				case IDM_FIND:
				{
					if (!config.hfind)
					{
						static FINDREPLACE fr = {0}; // "static" is required for WM_FINDMSGSTRING

						fr.lStructSize = sizeof (fr);
						fr.hwndOwner = hwnd;
						fr.lpstrFindWhat = config.search_string;
						fr.wFindWhatLen = _countof (config.search_string) - 1;
						fr.Flags = FR_HIDEWHOLEWORD | FR_HIDEMATCHCASE | FR_HIDEUPDOWN | FR_FINDNEXT;

						config.hfind = FindText (&fr);
					}
					else
					{
						SetFocus (config.hfind);
					}

					break;
				}

				case IDM_FINDNEXT:
				{
					if (!config.search_string[0])
					{
						PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDM_FIND, 0), 0);
					}
					else
					{
						FINDREPLACE fr = {0};

						fr.Flags = FR_FINDNEXT;
						fr.lpstrFindWhat = config.search_string;

						PostMessage (hwnd, WM_FINDMSGSTRING, 0, (LPARAM)&fr);
					}

					break;
				}

				case IDM_REFRESH:
				case IDM_REFRESH2:
				{
					_app_profilesave (hwnd);
					_app_profileload (hwnd);

					_app_installfilters (false);

					break;
				}

				case IDM_ENABLELOG_CHK:
				case IDM_TRAY_ENABLELOG_CHK:
				{
					const bool new_val = !app.ConfigGet (L"IsLogEnabled", false).AsBool ();

					CheckMenuItem (GetMenu (hwnd), IDM_ENABLELOG_CHK, MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));
					app.ConfigSet (L"IsLogEnabled", new_val);

					_app_loginit (new_val);

					break;
				}

				case IDM_ENABLENOTIFICATIONS_CHK:
				case IDM_TRAY_ENABLENOTIFICATIONS_CHK:
				{
					const bool new_val = !app.ConfigGet (L"IsNotificationsEnabled", true).AsBool ();

					CheckMenuItem (GetMenu (hwnd), IDM_ENABLENOTIFICATIONS_CHK, MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));
					app.ConfigSet (L"IsNotificationsEnabled", new_val);

					break;
				}

				case IDM_LOGSHOW:
				case IDM_TRAY_LOGSHOW:
				{
					rstring path = _r_path_expand (app.ConfigGet (L"LogPath", PATH_LOG));

					if (!_r_fs_exists (path))
						return FALSE;

					_r_run (nullptr, _r_fmt (L"%s \"%s\"", app.ConfigGet (L"LogViewer", L"notepad.exe").GetString (), path.GetString ()));

					break;
				}

				case IDM_LOGCLEAR:
				case IDM_TRAY_LOGCLEAR:
				{
					rstring path = _r_path_expand (app.ConfigGet (L"LogPath", PATH_LOG));

					if ((config.hlog != nullptr && config.hlog != INVALID_HANDLE_VALUE) || _r_fs_exists (path))
					{
						if (!messageFlag (hwnd, L"ConfirmLogClear", I18N (&app, IDS_QUESTION, 0), I18N (&app, IDS_QUESTION_FLAG_CHK, 0)))
							break;

						if (config.hlog != nullptr && config.hlog != INVALID_HANDLE_VALUE)
						{
							_r_fastlock_acquireexclusive (&lock_writelog);

							SetFilePointer (config.hlog, 2, nullptr, FILE_BEGIN);
							SetEndOfFile (config.hlog);

							_r_fastlock_releaseexclusive (&lock_writelog);
						}
						else
						{
							_r_fs_delete (path);
						}

						_r_fs_delete (_r_fmt (L"%s.bak", path.GetString ()));
					}

					break;
				}

				case IDM_TRAY_LOGSHOW_ERR:
				{
					rstring path = _r_dbg_getpath (APP_NAME_SHORT);

					if (!_r_fs_exists (path))
						return FALSE;

					_r_run (nullptr, _r_fmt (L"%s \"%s\"", app.ConfigGet (L"LogViewer", L"notepad.exe").GetString (), path.GetString ()));

					break;
				}

				case IDM_TRAY_LOGCLEAR_ERR:
				{
					rstring path = _r_dbg_getpath (APP_NAME_SHORT);

					if (!_r_fs_exists (path) || !messageFlag (hwnd, L"ConfirmLogClear", I18N (&app, IDS_QUESTION, 0), I18N (&app, IDS_QUESTION_FLAG_CHK, 0)))
						break;

					_r_fs_delete (path);

					break;
				}

				case IDM_TRAY_START:
				case IDC_START_BTN:
				{
					const bool state = !_wfp_isfiltersinstalled ();

					if (_app_installmessage (hwnd, state))
					{
						if (state)
							_app_installfilters (true);
						else
							_app_uninstallfilters ();
					}

					break;
				}

				case IDM_ADD_FILE:
				{
					WCHAR files[_R_BUFFER_LENGTH] = {0};
					OPENFILENAME ofn = {0};

					size_t item = 0;

					ofn.lStructSize = sizeof (ofn);
					ofn.hwndOwner = hwnd;
					ofn.lpstrFile = files;
					ofn.nMaxFile = _countof (files);
					ofn.lpstrFilter = L"*.exe\0*.exe\0*.*\0*.*\0\0";
					ofn.Flags = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_ENABLESIZING | OFN_PATHMUSTEXIST | OFN_FORCESHOWHIDDEN;

					if (GetOpenFileName (&ofn))
					{
						_r_fastlock_acquireexclusive (&lock_access);

						if (files[ofn.nFileOffset - 1] != 0)
						{
							item = _app_addapplication (hwnd, files, 0, false, false, false);
						}
						else
						{
							LPWSTR p = files;
							WCHAR dir[MAX_PATH] = {0};
							GetCurrentDirectory (_countof (dir), dir);

							while (*p)
							{
								p += wcslen (p) + 1;

								if (*p)
									item = _app_addapplication (hwnd, _r_fmt (L"%s\\%s", dir, p), 0, false, false, false);
							}
						}

						_r_fastlock_releaseexclusive (&lock_access);

						_app_listviewsort (hwnd, IDC_LISTVIEW, -1, false);
						_app_profilesave (hwnd);

						ShowItem (hwnd, IDC_LISTVIEW, _app_getposition (hwnd, item), -1);
					}

					break;
				}

				case IDM_ALL_PROCESSES:
				case IDM_ALL_PACKAGES:
				{
					if (LOWORD (wparam) == IDM_ALL_PROCESSES)
					{
						_app_getprocesslist (&processes);

						for (size_t i = 0; i < processes.size (); i++)
							_app_addapplication (hwnd, processes.at (i).real_path, 0, false, false, true);
					}
					else
					{
						for (size_t i = 0; i < packages.size (); i++)
							_app_addapplication (hwnd, packages.at (i).sid, 0, false, false, true);
					}

					_app_listviewsort (hwnd, IDC_LISTVIEW, -1, false);
					_app_profilesave (hwnd);

					break;
				}

				case IDM_DISABLENOTIFICATIONS:
				case IDM_EXPLORE:
				case IDM_COPY:
				case IDM_CHECK:
				case IDM_UNCHECK:
				{
					INT item = -1;
					BOOL new_val = BOOL (-1);

					rstring buffer;

					while ((item = (INT)SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_GETNEXTITEM, item, LVNI_SELECTED)) != -1)
					{
						const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, IDC_LISTVIEW, item);

						ITEM_APPLICATION* ptr_app = _app_getapplication (hash);

						if (!ptr_app)
							continue;

						if (LOWORD (wparam) == IDM_EXPLORE)
						{
							if (!ptr_app->is_picoapp)
							{
								if (_r_fs_exists (ptr_app->real_path))
									_r_run (nullptr, _r_fmt (L"\"explorer.exe\" /select,\"%s\"", ptr_app->real_path));

								else if (_r_fs_exists (_r_path_extractdir (ptr_app->real_path)))
									ShellExecute (hwnd, nullptr, _r_path_extractdir (ptr_app->real_path), nullptr, nullptr, SW_SHOWDEFAULT);
							}
						}
						else if (LOWORD (wparam) == IDM_COPY)
						{
							buffer.Append (ptr_app->display_name).Append (L"\r\n");
						}
						else if (LOWORD (wparam) == IDM_DISABLENOTIFICATIONS)
						{
							if (new_val == BOOL (-1))
								new_val = !ptr_app->is_silent;

							ptr_app->is_silent = new_val ? true : false;

							if (new_val)
								_app_freenotify (LAST_VALUE, hash);
						}
						else if (LOWORD (wparam) == IDM_CHECK || LOWORD (wparam) == IDM_UNCHECK)
						{
							config.is_nocheckboxnotify = true;

							ptr_app->is_enabled = (LOWORD (wparam) == IDM_CHECK) ? true : false;

							_r_listview_setitem (hwnd, IDC_LISTVIEW, item, 0, nullptr, LAST_VALUE, ptr_app->is_enabled ? 0 : 1);
							_r_listview_setitemcheck (hwnd, IDC_LISTVIEW, item, ptr_app->is_enabled);

							_app_freenotify (LAST_VALUE, hash);

							config.is_nocheckboxnotify = false;
						}
					}

					if (LOWORD (wparam) == IDM_CHECK || LOWORD (wparam) == IDM_UNCHECK)
					{
						_app_notifyrefresh ();
						_app_profilesave (hwnd);

						_app_installfilters (false);
					}
					else if (LOWORD (wparam) == IDM_DISABLENOTIFICATIONS)
					{
						_app_notifyrefresh ();
						_app_profilesave (hwnd);

						_r_listview_redraw (hwnd, IDC_LISTVIEW);
					}
					else if (LOWORD (wparam) == IDM_COPY)
					{
						buffer.Trim (L"\r\n");
						_r_clipboard_set (hwnd, buffer, buffer.GetLength ());
					}

					break;
				}

				case IDM_OPENRULESEDITOR:
				{
					ITEM_RULE *ptr_rule = (ITEM_RULE*)malloc (sizeof (ITEM_RULE));

					if (ptr_rule)
					{
						SecureZeroMemory (ptr_rule, sizeof (ITEM_RULE));

						ptr_rule->is_block = true; // block by default

						SetWindowLongPtr (hwnd, GWLP_USERDATA, LAST_VALUE);
						if (DialogBoxParam (nullptr, MAKEINTRESOURCE (IDD_EDITOR), hwnd, &EditorProc, (LPARAM)ptr_rule))
						{
							rules_custom.push_back (ptr_rule);

							_app_profilesave (hwnd);
							_app_profileload (hwnd); // important!

							_app_installfilters (false);
						}
						else
						{
							_app_freerule (&ptr_rule);
						}
					}

					break;
				}

				case IDM_DELETE:
				{
					const UINT selected = (UINT)SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_GETSELECTEDCOUNT, 0, 0);

					if (!selected || !messageFlag (hwnd, L"ConfirmDelete", _r_fmt (I18N (&app, IDS_QUESTION_DELETE, 0), selected), I18N (&app, IDS_QUESTION_FLAG_CHK, 0)))
						break;

					bool is_checked = false;
					const size_t count = _r_listview_getitemcount (hwnd, IDC_LISTVIEW) - 1;

					size_t item = LAST_VALUE;

					_r_fastlock_acquireexclusive (&lock_access);

					for (size_t i = count; i != LAST_VALUE; i--)
					{
						if (ListView_GetItemState (GetDlgItem (hwnd, IDC_LISTVIEW), i, LVNI_SELECTED))
						{
							const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, IDC_LISTVIEW, i);

							if (hash)
							{
								SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_DELETEITEM, i, 0);

								if (_app_freeapplication (hash) && !is_checked)
									is_checked = true;

								item = i;
							}
						}
					}

					_r_fastlock_releaseexclusive (&lock_access);

					if (item != LAST_VALUE)
						ShowItem (hwnd, IDC_LISTVIEW, (std::min) (item, _r_listview_getitemcount (hwnd, IDC_LISTVIEW) - 1), -1);

					_app_profilesave (hwnd);
					_app_profileload (hwnd); // important!

					if (is_checked)
					{
						_app_installfilters (false);
					}
					else
					{
						_r_listview_redraw (hwnd, IDC_LISTVIEW);
					}

					break;
				}

				case IDM_PURGEN:
				{
					bool is_deleted = false;
					bool is_checked = false;

					const size_t count = _r_listview_getitemcount (hwnd, IDC_LISTVIEW) - 1;

					_r_fastlock_acquireexclusive (&lock_access);

					for (size_t i = count; i != LAST_VALUE; i--)
					{
						const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, IDC_LISTVIEW, i);
						ITEM_APPLICATION* ptr_app = _app_getapplication (hash);

						if (ptr_app)
						{
							if (ptr_app->is_enabled && ptr_app->error_count || (ptr_app->is_storeapp && !_app_package_get (hash, nullptr, nullptr)) || (!ptr_app->is_storeapp && !ptr_app->is_picoapp && !ptr_app->is_network && ptr_app->real_path[0] != L'\\' && !_r_fs_exists (ptr_app->real_path)))
							{
								SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_DELETEITEM, i, 0);

								is_deleted = true;

								if (_app_freeapplication (hash) && !is_checked)
									is_checked = true;
							}
						}
					}

					_r_fastlock_releaseexclusive (&lock_access);

					if (is_deleted)
					{
						_app_profilesave (hwnd);
						_app_profileload (hwnd); // important!

						_app_notifyrefresh ();

						if (is_checked)
						{
							_app_installfilters (false);
						}
						else
						{
							_r_listview_redraw (hwnd, IDC_LISTVIEW);
						}
					}

					break;
				}

				case IDM_SELECT_ALL:
				{
					ListView_SetItemState (GetDlgItem (hwnd, IDC_LISTVIEW), -1, LVIS_SELECTED, LVIS_SELECTED);
					break;
				}

				case IDM_ZOOM:
				{
					ShowWindow (hwnd, IsZoomed (hwnd) ? SW_RESTORE : SW_MAXIMIZE);
					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

INT APIENTRY wWinMain (HINSTANCE, HINSTANCE, LPWSTR, INT)
{
	MSG msg = {0};

	if (app.CreateMainWindow (&DlgProc, &initializer_callback))
	{
		const HACCEL haccel = LoadAccelerators (app.GetHINSTANCE (), MAKEINTRESOURCE (IDA_MAIN));

		while (GetMessage (&msg, nullptr, 0, 0) > 0)
		{
			if (haccel)
				TranslateAccelerator (app.GetHWND (), haccel, &msg);

			if (!IsDialogMessage (app.GetHWND (), &msg))
			{
				TranslateMessage (&msg);
				DispatchMessage (&msg);
			}
		}

		if (haccel)
			DestroyAcceleratorTable (haccel);
	}

	return (INT)msg.wParam;
}
