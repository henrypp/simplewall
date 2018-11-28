// simplewall
// Copyright (c) 2016-2018 Henry++

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
#include <shobjidl.h>
#include <shlguid.h>
#include <sddl.h>
#include <ws2tcpip.h>
#include <wintrust.h>
#include <softpub.h>
#include <algorithm>
#include <userenv.h>

#include "main.hpp"
#include "rapp.hpp"
#include "routine.hpp"

#include "pugiconfig.hpp"
#include "..\..\pugixml\src\pugixml.hpp"

#include "resource.hpp"

rapp app (APP_NAME, APP_NAME_SHORT, APP_VERSION, APP_COPYRIGHT);

std::unordered_map<size_t, ITEM_APP> apps;

std::unordered_map<size_t, LPWSTR> cache_signatures;
std::unordered_map<size_t, LPWSTR> cache_versions;
std::unordered_map<size_t, LPWSTR> cache_hosts;

std::vector<HANDLE> threads_pool;

std::unordered_map<rstring, bool, rstring::hash, rstring::is_equal> rules_config;

std::vector<ITEM_COLOR> colors;
std::vector<ITEM_PROTOCOL> protocols;
std::vector<ITEM_ADD> processes;
std::vector<ITEM_ADD> packages;
std::vector<ITEM_ADD> services;
std::vector<time_t> timers;

std::vector<PITEM_RULE> rules_blocklist;
std::vector<PITEM_RULE> rules_system;
std::vector<PITEM_RULE> rules_custom;

std::vector<PITEM_LOG> notifications;
MARRAY filters2;

STATIC_DATA config;

FWPM_SESSION session;

EXTERN_C const IID IID_IImageList;

_R_FASTLOCK lock_apply;
_R_FASTLOCK lock_access;
_R_FASTLOCK lock_writelog;
_R_FASTLOCK lock_notification;
_R_FASTLOCK lock_threadpool;
_R_FASTLOCK lock_eventcallback;

ITEM_LIST_HEAD log_stack = {0};

bool _wfp_initialize (bool is_full);
void _wfp_uninitialize (bool is_full);

void CALLBACK _app_timer_callback (PVOID lparam, BOOLEAN);
bool _app_timer_create (HWND hwnd, size_t hash, time_t seconds, bool is_transact);
bool _app_timer_remove (HWND hwnd, size_t hash, bool is_transact);

bool _app_istimeractive (ITEM_APP const *ptr_app);
bool _app_isunused (ITEM_APP const *ptr_app, size_t hash);
bool _app_isexists (ITEM_APP const *ptr_app);

UINT WINAPI ApplyThread (LPVOID lparam);
LRESULT CALLBACK NotificationProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

bool _app_notifysettimeout (HWND hwnd, UINT_PTR id, bool is_create, UINT timeout);
bool _app_notifyrefresh ();

bool _wfp_logsubscribe ();
bool _wfp_logunsubscribe ();

void _app_logerror (LPCWSTR fn, DWORD code, LPCWSTR desc, bool is_nopopups)
{
	_r_dbg_write (APP_NAME_SHORT, APP_VERSION, fn, code, desc);

	if (!is_nopopups && app.ConfigGet (L"IsErrorNotificationsEnabled", true).AsBool ()) // check for timeout (sec.)
	{
		config.is_popuperrors = true;

		app.TrayPopup (app.GetHWND (), UID, nullptr, NIIF_USER | (app.ConfigGet (L"IsNotificationsSound", true).AsBool () ? 0 : NIIF_NOSOUND), APP_NAME, app.LocaleString (IDS_STATUS_ERROR, nullptr));
	}
}

void _mps_changeconfig (bool is_stop)
{
	const SC_HANDLE scm = OpenSCManager (nullptr, nullptr, SC_MANAGER_ALL_ACCESS);

	if (!scm)
	{
		_app_logerror (L"OpenSCManager", GetLastError (), nullptr, false);
	}
	else
	{
		LPCWSTR arr[] = {
			L"mpssvc",
			L"mpsdrv",
		};

		bool is_started = false;

		for (INT i = 0; i < _countof (arr); i++)
		{
			const SC_HANDLE sc = OpenService (scm, arr[i], SERVICE_CHANGE_CONFIG | SERVICE_QUERY_STATUS | SERVICE_STOP);

			if (!sc)
			{
				const DWORD result = GetLastError ();

				if (result != ERROR_ACCESS_DENIED)
					_app_logerror (L"OpenService", GetLastError (), arr[i], false);
			}
			else
			{
				if (!is_started)
				{
					SERVICE_STATUS status;

					if (QueryServiceStatus (sc, &status))
						is_started = (status.dwCurrentState == SERVICE_RUNNING);
				}

				if (!ChangeServiceConfig (sc, SERVICE_NO_CHANGE, is_stop ? SERVICE_DISABLED : SERVICE_AUTO_START, SERVICE_NO_CHANGE, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr))
					_app_logerror (L"ChangeServiceConfig", GetLastError (), arr[i], false);

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
					_app_logerror (L"OpenService", GetLastError (), arr[i], false);
				}
				else
				{
					DWORD dwBytesNeeded = 0;
					SERVICE_STATUS_PROCESS ssp = {0};

					if (!QueryServiceStatusEx (sc, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof (ssp), &dwBytesNeeded))
					{
						_app_logerror (L"QueryServiceStatusEx", GetLastError (), arr[i], false);
					}
					else
					{
						if (ssp.dwCurrentState != SERVICE_RUNNING)
						{
							if (!StartService (sc, 0, nullptr))
							{
								_app_logerror (L"StartService", GetLastError (), arr[i], false);
							}
						}

						CloseServiceHandle (sc);
					}
				}
			}

			_r_sleep (250);

			_r_run (nullptr, L"netsh advfirewall set allprofiles state on", nullptr, SW_HIDE);
		}

		CloseServiceHandle (scm);
	}
}

void _app_listviewresize (HWND hwnd, UINT ctrl_id)
{
	if (!app.ConfigGet (L"AutoSizeColumns", true).AsBool ())
		return;

	RECT rect = {0};
	GetWindowRect (GetDlgItem (hwnd, ctrl_id), &rect);

	const INT width = (rect.right - rect.left) - GetSystemMetrics (SM_CXVSCROLL);

	const INT cx2 = max (app.GetDPI (110), min (app.GetDPI (190), _R_PERCENT_VAL (28, width)));
	const INT cx1 = width - cx2;

	_r_listview_setcolumn (hwnd, ctrl_id, 0, nullptr, cx1);
	_r_listview_setcolumn (hwnd, ctrl_id, 1, nullptr, cx2);
}

void _app_listviewsetimagelist (HWND hwnd, UINT ctrl_id)
{
	HIMAGELIST himg = nullptr;

	const bool is_large = app.ConfigGet (L"IsLargeIcons", false).AsBool () && ctrl_id == IDC_LISTVIEW;
	const bool is_bigview = app.ConfigGet (L"IsBigView", true).AsBool () && ctrl_id == IDC_LISTVIEW;
	const bool is_iconshidden = app.ConfigGet (L"IsIconsHidden", false).AsBool ();

	if (SUCCEEDED (SHGetImageList ((is_bigview ? SHIL_EXTRALARGE : (is_large ? SHIL_LARGE : SHIL_SMALL)), IID_IImageList, (void**)&himg)))
	{
		SendDlgItemMessage (hwnd, ctrl_id, LVM_SETIMAGELIST, LVSIL_NORMAL, (LPARAM)himg);
		SendDlgItemMessage (hwnd, ctrl_id, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)himg);
	}

	if (ctrl_id != IDC_LISTVIEW)
		return;

	SendDlgItemMessage (hwnd, ctrl_id, LVM_SCROLL, 0, GetScrollPos (GetDlgItem (hwnd, ctrl_id), SB_VERT)); // scrollbar-hack!!!

	SendDlgItemMessage (hwnd, ctrl_id, LVM_SETVIEW, is_bigview ? LV_VIEW_ICON : LV_VIEW_DETAILS, NULL);

	CheckMenuRadioItem (GetMenu (hwnd), IDM_ICONSSMALL, IDM_ICONSEXTRALARGE, (is_bigview ? IDM_ICONSEXTRALARGE : (is_large ? IDM_ICONSLARGE : IDM_ICONSSMALL)), MF_BYCOMMAND);
	CheckMenuItem (GetMenu (hwnd), IDM_ICONSISHIDDEN, MF_BYCOMMAND | (is_iconshidden ? MF_CHECKED : MF_UNCHECKED));
}

void _app_setbuttonmargins (HWND hwnd, UINT ctrl_id)
{
	// set icons margin
	{
		RECT rc = {0};
		rc.left = rc.right = app.GetDPI (4);

		SendDlgItemMessage (hwnd, ctrl_id, BCM_SETTEXTMARGIN, 0, (LPARAM)&rc);
	}

	// set split info
	{
		BUTTON_SPLITINFO bsi = {0};

		bsi.mask = BCSIF_SIZE | BCSIF_STYLE;
		bsi.uSplitStyle = BCSS_STRETCH;

		bsi.size.cx = app.GetDPI (18);
		bsi.size.cy = 0;

		SendDlgItemMessage (hwnd, ctrl_id, BCM_SETSPLITINFO, 0, (LPARAM)&bsi);
	}
}

bool _app_listviewinitfont (PLOGFONT plf)
{
	const rstring buffer = app.ConfigGet (L"Font", UI_FONT_DEFAULT);

	if (!buffer.IsEmpty ())
	{
		rstring::rvector vc = buffer.AsVector (L";");

		for (size_t i = 0; i < vc.size (); i++)
		{
			vc.at (i).Trim (L" \r\n");

			if (vc.at (i).IsEmpty ())
				continue;

			if (i == 0)
				StringCchCopy (plf->lfFaceName, LF_FACESIZE, vc.at (i));

			else if (i == 1)
				plf->lfHeight = _r_dc_fontsizetoheight (vc.at (i).AsInt ());

			else if (i == 2)
				plf->lfWeight = vc.at (i).AsInt ();

			else
				break;
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
			plf->lfCharSet = pdeflf->lfCharSet;
			plf->lfQuality = pdeflf->lfQuality;
		}
	}

	return true;
}

rstring _app_getlogviewer ()
{
	rstring result;

	LPCWSTR csvviewer[] = {
		L"CSVFileView.exe",
		L"CSVFileView\\CSVFileView.exe",
		L"..\\CSVFileView\\CSVFileView.exe"
	};

	for (size_t i = 0; i < _countof (csvviewer); i++)
	{
		result = _r_path_expand (csvviewer[i]);

		if (_r_fs_exists (result))
			return result;
	}

	result = app.ConfigGet (L"LogViewer", L"notepad.exe");

	return _r_path_expand (result);
}

void _app_listviewsetfont (HWND hwnd, UINT ctrl_id, bool is_redraw)
{
	LOGFONT lf = {0};

	if (is_redraw || !config.hfont)
	{
		if (config.hfont)
		{
			DeleteObject (config.hfont);
			config.hfont = nullptr;
		}

		_app_listviewinitfont (&lf);
		config.hfont = CreateFontIndirect (&lf);
	}

	if (config.hfont)
		SendDlgItemMessage (hwnd, ctrl_id, WM_SETFONT, (WPARAM)config.hfont, TRUE);
}

ITEM_APP* _app_getapplication (size_t hash)
{
	ITEM_APP *ptr_app = nullptr;

	if (hash && apps.find (hash) != apps.end ())
		ptr_app = &apps.at (hash);

	return ptr_app;
}

void _app_applycasestyle (LPWSTR buffer, size_t length)
{
	if (length && wcschr (buffer, OBJ_NAME_PATH_SEPARATOR))
	{
		buffer[0] = _r_str_upper (buffer[0]);

		for (size_t i = 1; i < length; i++)
			buffer[i] = _r_str_lower (buffer[i]);
	}
}

bool _app_isapphaverule (size_t hash)
{
	if (!hash)
		return false;

	for (size_t i = 0; i < rules_custom.size (); i++)
	{
		PITEM_RULE const ptr_rule = rules_custom.at (i);

		if (ptr_rule && ptr_rule->is_enabled && ptr_rule->apps.find (hash) != ptr_rule->apps.end ())
			return true;
	}

	return false;
}

size_t _app_getappgroup (size_t hash, PITEM_APP const ptr_app)
{
	if (!ptr_app)
		return 2;

	// apps with special rule
	if (app.ConfigGet (L"IsEnableSpecialGroup", false).AsBool () && _app_isapphaverule (hash))
		return 1;

	return ptr_app->is_enabled ? 0 : 2;
}

size_t _app_getrulegroup (PITEM_RULE const ptr_rule)
{
	if (!ptr_rule || !ptr_rule->is_enabled)
		return 2;

	if (app.ConfigGet (L"IsEnableSpecialGroup", false).AsBool () && !ptr_rule->apps.empty ())
		return 1;

	return 0;
}

void ShowItem (HWND hwnd, UINT ctrl_id, size_t item, INT scroll_pos)
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

void _app_getcount (PITEM_STATUS ptr_status)
{
	if (!ptr_status)
		return;

	for (auto const &p : apps)
	{
		const bool is_exists = _app_isexists (&p.second);

		if (!is_exists)
			ptr_status->invalid_count += 1;

		if (_app_isunused (&p.second, p.first))
			ptr_status->unused_count += 1;

		if (_app_istimeractive (&p.second))
			ptr_status->timers_count += 1;

		ptr_status->total_count += 1;
	}
}

LONG gettextwidth (HDC hdc, LPCWSTR text, size_t length)
{
	SIZE size = {0};

	if (!GetTextExtentPoint32 (hdc, text, (INT)length, &size))
		size.cx = 200;

	return size.cx;
}

void _app_refreshstatus (HWND hwnd)
{
	const HWND hstatus = GetDlgItem (hwnd, IDC_STATUSBAR);
	const HDC hdc = GetDC (hstatus);

	// item count
	if (hdc)
	{
		SelectObject (hdc, (HFONT)SendMessage (hstatus, WM_GETFONT, 0, 0)); // fix

		const UINT parts_count = 4;

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

					const size_t selection_count = SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_GETSELECTEDCOUNT, 0, 0);

					if (selection_count)
						text[i].AppendFormat (L" / %s", _r_fmt (app.LocaleString (IDS_STATUS_SELECTED, nullptr), selection_count).GetString ());

					break;
				}

				case 1:
				{
					text[i].Format (L"%s: %d", app.LocaleString (IDS_STATUS_UNUSED_APPS, nullptr).GetString (), itemStat.unused_count);
					break;
				}

				case 2:
				{
					text[i].Format (L"%s: %d", app.LocaleString (IDS_STATUS_INVALID_APPS, nullptr).GetString (), itemStat.invalid_count);
					break;
				}

				case 3:
				{
					text[i].Format (L"%s: %d", app.LocaleString (IDS_STATUS_TIMER_APPS, nullptr).GetString (), itemStat.timers_count);
					break;
				}
			}

			size[i] = gettextwidth (hdc, text[i], text[i].GetLength ()) + 10;

			if (i)
				lay += size[i];
		}

		RECT rc = {0};
		GetClientRect (hstatus, &rc);

		parts[0] = _R_RECT_WIDTH (&rc) - lay - GetSystemMetrics (SM_CXSMICON);
		parts[1] = parts[0] + size[1];
		parts[2] = parts[1] + size[2];
		parts[3] = parts[2] + size[3];

		SendMessage (hstatus, SB_SETPARTS, parts_count, (LPARAM)parts);

		for (UINT i = 0; i < parts_count; i++)
		{
			SendMessage (hstatus, SB_SETTEXT, MAKEWPARAM (i, 0), (LPARAM)text[i].GetString ());
		}

		ReleaseDC (hstatus, hdc);
	}

	// group information
	{
		const size_t total_count = _r_listview_getitemcount (hwnd, IDC_LISTVIEW);
		size_t group1_count = 0;
		size_t group2_count = 0;
		size_t group3_count = 0;

		for (auto &p : apps)
		{
			const size_t group_id = _app_getappgroup (p.first, &p.second);

			if (group_id == 0)
				group1_count += 1;

			else if (group_id == 1)
				group2_count += 1;

			else
				group3_count += 1;
		}

		const bool is_whitelist = (app.ConfigGet (L"Mode", ModeWhitelist).AsUint () == ModeWhitelist);

		_r_listview_setgroup (hwnd, IDC_LISTVIEW, 0, app.LocaleString (is_whitelist ? IDS_GROUP_ALLOWED : IDS_GROUP_BLOCKED, _r_fmt (L" (%zu)", group1_count)), 0, 0);
		_r_listview_setgroup (hwnd, IDC_LISTVIEW, 1, app.LocaleString (IDS_GROUP_SPECIAL_APPS, _r_fmt (L" (%zu)", group2_count)), 0, 0);
		_r_listview_setgroup (hwnd, IDC_LISTVIEW, 2, app.LocaleString (is_whitelist ? IDS_GROUP_BLOCKED : IDS_GROUP_ALLOWED, _r_fmt (L" (%zu)", group3_count)), 0, 0);
	}
}

bool _app_item_get (std::vector<ITEM_ADD>* pvec, size_t hash, rstring* display_name, rstring* real_path, PSID* lpsid, PSECURITY_DESCRIPTOR* lpsd, rstring* /*description*/)
{
	for (size_t i = 0; i < pvec->size (); i++)
	{
		if (pvec->at (i).hash == hash)
		{
			if (display_name)
			{
				if (pvec->at (i).display_name[0])
					*display_name = pvec->at (i).display_name;

				else if (pvec->at (i).real_path[0])
					*display_name = pvec->at (i).real_path;

				else if (pvec->at (i).sid[0])
					*display_name = pvec->at (i).sid;
			}

			if (real_path)
			{
				if (pvec->at (i).real_path[0])
					*real_path = pvec->at (i).real_path;
			}

			if (lpsid)
				*lpsid = pvec->at (i).psid;

			if (lpsd)
				*lpsd = pvec->at (i).psd;

			//if (description)
			//	*description = pvec->at (i).pdesc;

			return true;
		}
	}

	return false;
}

void _app_getdisplayname (size_t hash, ITEM_APP const *ptr_app, rstring& extracted_name)
{
	if (
		hash == config.ntoskrnl_hash ||
		ptr_app->type == AppService
		)
	{
		extracted_name = ptr_app->original_path;
		return;
	}
	else if (ptr_app->type == AppStore)
	{
		_app_item_get (&packages, hash, &extracted_name, nullptr, nullptr, nullptr, nullptr);
		return;
	}

	if (app.ConfigGet (L"ShowFilenames", true).AsBool ())
	{
		extracted_name = _r_path_extractfile (ptr_app->real_path);
		return;
	}

	extracted_name = ptr_app->real_path;
}

bool _app_getinformation (size_t hash, LPCWSTR path, LPCWSTR* pinfo)
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

					WCHAR author_entry[86] = {0};
					WCHAR description_entry[86] = {0};

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
					{
						const size_t length = buffer.GetLength () + 1;
						LPWSTR ppointer = new WCHAR[length];

						if (ppointer)
							StringCchCopy (ppointer, length, buffer);

						*pinfo = ppointer;
						cache_versions[hash] = ppointer;
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

	if (SUCCEEDED (CoInitialize (nullptr)))
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

		CoUninitialize ();
	}

	return result;
}

void _app_getappicon (ITEM_APP const *ptr_app, bool is_small, size_t* picon_id, HICON* picon)
{
	const bool is_iconshidden = app.ConfigGet (L"IsIconsHidden", false).AsBool ();

	if (ptr_app->type == AppRegular)
	{
		if (is_iconshidden || !_app_getfileicon (ptr_app->real_path, is_small, picon_id, picon))
		{
			if (picon_id)
				*picon_id = config.icon_id;

			if (picon)
				*picon = CopyIcon (is_small ? config.hicon_small : config.hicon_large);
		}
	}
	else if (ptr_app->type == AppStore)
	{
		if (picon_id)
			*picon_id = config.icon_package_id;

		if (picon)
			*picon = CopyIcon (is_small ? config.hicon_package : config.hicon_large); // small-only!
	}
	else if (ptr_app->type == AppService)
	{
		if (picon_id)
			*picon_id = config.icon_service_id;

		if (picon)
			*picon = CopyIcon (is_small ? config.hicon_service_small : config.hicon_large); // small-only!
	}
	else
	{
		if (picon_id)
			*picon_id = config.icon_id;

		if (picon)
			*picon = CopyIcon (is_small ? config.hicon_small : config.hicon_large);
	}
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

	if (SUCCEEDED (CoInitializeEx (nullptr, COINIT_MULTITHREADED)))
	{
		if (SUCCEEDED (CoInitializeSecurity (nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, 0, nullptr)))
		{
			if (SUCCEEDED (CoCreateInstance (CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&psl)))
			{
				IPersistFile* ppf = nullptr;

				if (SUCCEEDED (psl->QueryInterface (IID_IPersistFile, (void**)&ppf)))
				{
					if (SUCCEEDED (ppf->Load (path, STGM_READ)))
					{
						if (SUCCEEDED (psl->Resolve (hwnd, 0)))
						{
							WIN32_FIND_DATA wfd = {0};
							WCHAR buffer[MAX_PATH] = {0};

							if (SUCCEEDED (psl->GetPath (buffer, _countof (buffer), (LPWIN32_FIND_DATA)&wfd, SLGP_RAWPATH)))
								result = buffer;
						}
					}

					ppf->Release ();
				}

				psl->Release ();
			}
		}

		CoUninitialize ();
	}

	return result;
}

bool _app_verifysignature (size_t hash, LPCWSTR path, LPCWSTR* psigner)
{
	if (!app.ConfigGet (L"IsCerificatesEnabled", false).AsBool ())
		return false;

	if (!psigner)
		return false;

	if (cache_signatures.find (hash) != cache_signatures.end ())
	{
		*psigner = cache_signatures[hash];

		return (cache_signatures[hash] != nullptr);
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
							LPWSTR ppointer = new WCHAR[num_chars];

							if (ppointer)
							{
								CertGetNameString (psProvCert->pCert, CERT_NAME_ATTR_TYPE, 0, szOID_COMMON_NAME, ppointer, num_chars);
								*psigner = ppointer;
							}
							else
							{
								*psigner = nullptr;
							}

							cache_signatures[hash] = ppointer;
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

HBITMAP _app_ico2bmp (HICON hicon)
{
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

void _app_generate_packages ()
{
	for (size_t i = 0; i < packages.size (); i++)
	{
		if (packages.at (i).psid)
			LocalFree (packages.at (i).psid);
	}

	packages.clear ();

	HKEY hkey = nullptr;
	HKEY hsubkey = nullptr;
	LONG result = RegOpenKeyEx (HKEY_CLASSES_ROOT, L"Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppContainer\\Mappings", 0, KEY_READ, &hkey);

	if (result != ERROR_SUCCESS)
	{
		_app_logerror (L"RegOpenKeyEx", result, nullptr, true);
	}
	else
	{
		DWORD index = 0;

		while (true)
		{
			WCHAR package_sid[MAX_PATH] = {0};
			DWORD size = _countof (package_sid);

			if (RegEnumKeyEx (hkey, index++, package_sid, &size, 0, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
				break;

			result = RegOpenKeyEx (hkey, package_sid, 0, KEY_READ, &hsubkey);

			if (result != ERROR_SUCCESS)
			{
				if (result != ERROR_FILE_NOT_FOUND)
					_app_logerror (L"RegOpenKeyEx", result, package_sid, true);
			}
			else
			{
				const size_t hash = _r_str_hash (package_sid);

				ITEM_ADD item = {0};

				item.hash = hash;
				StringCchCopy (item.sid, _countof (item.sid), package_sid);

				size = _countof (item.display_name) * sizeof (WCHAR);
				result = RegQueryValueEx (hsubkey, L"DisplayName", nullptr, nullptr, (LPBYTE)item.display_name, &size);

				if (result == ERROR_SUCCESS)
				{
					if (item.display_name[0] == L'@')
					{
						if (!SUCCEEDED (SHLoadIndirectString (rstring (item.display_name), item.display_name, _countof (item.display_name), nullptr)))
							item.display_name[0] = 0;
					}
				}

				if (!item.display_name[0])
				{
					size = _countof (item.display_name) * sizeof (WCHAR);
					RegQueryValueEx (hsubkey, L"Moniker", nullptr, nullptr, (LPBYTE)item.display_name, &size);
				}

				if (!item.display_name[0])
					StringCchCopy (item.display_name, _countof (item.display_name), package_sid);

				ConvertStringSidToSid (package_sid, &item.psid);

				packages.push_back (item);

				RegCloseKey (hsubkey);
			}
		}

		std::sort (packages.begin (), packages.end (),
			[](const ITEM_ADD& a, const ITEM_ADD& b)->bool {
			return StrCmpLogicalW (a.display_name, b.display_name) == -1;
		});

		RegCloseKey (hkey);
	}
}

void _app_generate_services ()
{
	for (size_t i = 0; i < services.size (); i++)
	{
		if (services.at (i).psd)
			LocalFree (services.at (i).psd);
	}

	services.clear ();

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
						LPWSTR sidstring = nullptr;

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
									// query path
									StringCchCopy (real_path, _countof (real_path), lpqsc->lpBinaryPathName);
									PathRemoveArgs (real_path);
									PathUnquoteSpaces (real_path);

									_app_applycasestyle (real_path, wcslen (real_path)); // apply case-style
								}
								else
								{
									delete[] lpqsc;
									continue;
								}

								delete[] lpqsc;
							}

							CloseServiceHandle (hsvc);
						}

						UNICODE_STRING serviceNameUs = {0};

						serviceNameUs.Buffer = buffer;
						serviceNameUs.Length = (USHORT)(wcslen (service_name) * sizeof (WCHAR));
						serviceNameUs.MaximumLength = serviceNameUs.Length;

						StringCchCopy (buffer, _countof (buffer), service_name);

						SID* serviceSid = nullptr;
						ULONG serviceSidLength = 0;

						// get service security identifier
						if (RtlCreateServiceSid (&serviceNameUs, serviceSid, &serviceSidLength) == 0xC0000023 /*STATUS_BUFFER_TOO_SMALL*/)
						{
							serviceSid = new SID[serviceSidLength];

							if (serviceSid)
							{
								if (NT_SUCCESS (RtlCreateServiceSid (&serviceNameUs, serviceSid, &serviceSidLength)))
								{
									ConvertSidToStringSid (serviceSid, &sidstring);
								}
								else
								{
									delete[] serviceSid;
									serviceSid = nullptr;
								}
							}
						}

						if (serviceSid && sidstring)
						{
							ITEM_ADD item = {0};

							StringCchCopy (item.service_name, _countof (item.service_name), service_name);
							StringCchCopy (item.display_name, _countof (item.display_name), display_name);
							StringCchCopy (item.real_path, _countof (item.real_path), real_path);
							StringCchCopy (item.sid, _countof (item.sid), sidstring);
							item.hash = _r_str_hash (item.service_name);

							if (!ConvertStringSecurityDescriptorToSecurityDescriptor (_r_fmt (SERVICE_SECURITY_DESCRIPTOR, sidstring).ToUpper (), SDDL_REVISION_1, &item.psd, nullptr))
								_app_logerror (L"ConvertStringSecurityDescriptorToSecurityDescriptor", GetLastError (), service_name, false);

							else
								services.push_back (item);
						}

						if (sidstring)
							LocalFree (sidstring);

						delete[] serviceSid;
					}

					std::sort (services.begin (), services.end (),
						[](const ITEM_ADD& a, const ITEM_ADD& b)->bool {
						return StrCmpLogicalW (a.service_name, b.service_name) == -1;
					});

					delete[] pServices;
				}
			}
		}

		CloseServiceHandle (hsvcmgr);
	}
}

void _app_resolvefilename (rstring& path)
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
	else if (_wcsnicmp (path, L"system32\\", 8) == 0)
	{
		WCHAR systemRoot[MAX_PATH] = {0};
		GetSystemDirectory (systemRoot, _countof (systemRoot));

		path.Mid (8);
		path.Insert (0, systemRoot);
	}
}

size_t _app_addapplication (HWND hwnd, rstring path, time_t timestamp, time_t timer, time_t last_notify, bool is_silent, bool is_enabled, bool is_fromdb)
{
	if (path.IsEmpty ())
		return 0;

	// if file is shortcut - get location
	if (!is_fromdb)
	{
		if (_wcsnicmp (PathFindExtension (path), L".lnk", 4) == 0)
		{
			path = _app_getshortcutpath (hwnd, path);

			if (path.IsEmpty ())
				return 0;
		}
	}

	_app_resolvefilename (path);

	const size_t hash = path.Hash ();

	if (apps.find (hash) != apps.end ())
		return 0; // already exists

	ITEM_APP *ptr_app = &apps[hash]; // application pointer

	const bool is_ntoskrnl = (hash == config.ntoskrnl_hash);
	const time_t current_time = _r_unixtime_now ();

	rstring real_path;

	if (_wcsnicmp (path, L"\\device\\", 8) == 0) // device path
	{
		real_path = path;

		ptr_app->type = AppDevice;
		ptr_app->icon_id = config.icon_id;
	}
	else if (_wcsnicmp (path, L"S-1-", 4) == 0) // windows store (win8+)
	{
		ptr_app->type = AppStore;
		ptr_app->icon_id = config.icon_package_id;

		_app_item_get (&packages, hash, nullptr, &real_path, &ptr_app->psid, nullptr, nullptr);
	}
	else if (PathIsNetworkPath (path)) // network path
	{
		real_path = path;

		ptr_app->type = AppNetwork;
		ptr_app->icon_id = config.icon_id;
	}
	else
	{
		real_path = path;

		if (!is_ntoskrnl && real_path.Find (OBJ_NAME_PATH_SEPARATOR) == rstring::npos)
		{
			if (_app_item_get (&services, hash, nullptr, &real_path, nullptr, &ptr_app->psd, nullptr))
			{
				ptr_app->type = AppService;
				ptr_app->icon_id = config.icon_service_id;
			}
			else
			{
				ptr_app->type = AppPico;
				ptr_app->icon_id = config.icon_id;
			}
		}
		else
		{
			ptr_app->type = AppRegular;

			if (is_ntoskrnl) // "system" process
			{
				//display_name = path;
				real_path = _r_path_expand (PATH_NTOSKRNL);
			}

			const DWORD dwAttr = GetFileAttributes (real_path);
			ptr_app->is_temp = ((dwAttr != INVALID_FILE_ATTRIBUTES && (dwAttr & FILE_ATTRIBUTE_TEMPORARY) != 0)) || (_wcsnicmp (real_path, config.tmp1_dir, config.tmp1_length) == 0);
			ptr_app->is_system = !ptr_app->is_temp && (is_ntoskrnl || ((dwAttr != INVALID_FILE_ATTRIBUTES && (dwAttr & FILE_ATTRIBUTE_SYSTEM) != 0)) || (_wcsnicmp (real_path, config.windows_dir, config.wd_length) == 0));

			ptr_app->is_signed = _app_verifysignature (hash, real_path, &ptr_app->signer);
		}
	}

	_app_applycasestyle (real_path.GetBuffer (), real_path.GetLength ()); // apply case-style
	_app_applycasestyle (path.GetBuffer (), path.GetLength ()); // apply case-style

	StringCchCopy (ptr_app->original_path, _countof (ptr_app->original_path), path);
	StringCchCopy (ptr_app->real_path, _countof (ptr_app->real_path), real_path);

	rstring name;
	_app_getdisplayname (hash, ptr_app, name);

	StringCchCopy (ptr_app->display_name, _countof (ptr_app->display_name), name);

	ptr_app->is_enabled = is_enabled;
	ptr_app->is_silent = is_silent;

	ptr_app->timestamp = timestamp ? timestamp : current_time;
	ptr_app->last_notify = last_notify;

	// install timer
	if (timer)
	{
		if (timer > current_time)
		{
			if (!ptr_app->htimer && CreateTimerQueueTimer (&ptr_app->htimer, config.htimer, &_app_timer_callback, (PVOID)hash, DWORD ((timer - current_time) * _R_SECONDSCLOCK_MSEC), 0, WT_EXECUTEONLYONCE | WT_EXECUTEINTIMERTHREAD))
			{
				ptr_app->is_enabled = true;
				ptr_app->timer = timer;
			}
		}
		else
		{
			ptr_app->is_enabled = false;
		}
	}

	_app_getappicon (ptr_app, false, &ptr_app->icon_id, nullptr);

	const size_t item = _r_listview_getitemcount (hwnd, IDC_LISTVIEW);

	config.is_nocheckboxnotify = true;

	_r_listview_additem (hwnd, IDC_LISTVIEW, item, 0, ptr_app->display_name, ptr_app->icon_id, _app_getappgroup (hash, ptr_app), hash);
	_r_listview_setitem (hwnd, IDC_LISTVIEW, item, 1, _r_fmt_date (ptr_app->timestamp, FDTF_SHORTDATE | FDTF_SHORTTIME));

	_r_listview_setitemcheck (hwnd, IDC_LISTVIEW, item, is_enabled);

	config.is_nocheckboxnotify = false;

	return hash;
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

		psock = (PSOCKADDR)&ipv4Address;
		size = sizeof (ipv4Address);
	}
	else if (af == AF_INET6)
	{
		ipv6Address.sin6_family = af;
		ipv6Address.sin6_addr = *PIN6_ADDR (paddr);

		psock = (PSOCKADDR)&ipv6Address;
		size = sizeof (ipv6Address);
	}
	else
	{
		return false;
	}

	if (GetNameInfo (psock, size, buffer, length, nullptr, 0, NI_NAMEREQD) != ERROR_SUCCESS)
		return false;

	return true;
}

rstring _app_rulesexpand (PITEM_RULE const ptr_rule)
{
	rstring result;

	if (ptr_rule)
	{
		for (auto const &p : ptr_rule->apps)
		{
			ITEM_APP const *ptr_app = _app_getapplication (p.first);

			if (ptr_app)
			{
				if (ptr_app->type == AppStore || ptr_app->type == AppService)
					result.Append (ptr_app->display_name);

				else
					result.Append (ptr_app->original_path);

				result.Append (L"\r\n" SZ_TAB);
			}
		}

		result.Trim (L"\r\n" SZ_TAB);
	}

	return result;
}

void _app_freenotify (size_t idx_orhash, bool is_idx)
{
	const size_t count = notifications.size ();

	if (!count)
		return;

	if (is_idx)
	{
		PITEM_LOG ptr_log = notifications.at (idx_orhash);

		delete ptr_log;

		notifications.erase (notifications.begin () + idx_orhash);
	}
	else
	{
		for (size_t i = (count - 1); i != LAST_VALUE; i--)
		{
			PITEM_LOG const ptr_log = notifications.at (i);

			if (!ptr_log || ptr_log->hash == idx_orhash)
			{
				delete ptr_log;

				notifications.erase (notifications.begin () + i);
			}
		}
	}
}

bool _app_freeapplication (size_t hash)
{
	bool is_enabled = false;

	PITEM_APP ptr_app = _app_getapplication (hash);

	if (ptr_app)
		is_enabled = ptr_app->is_enabled;

	if (hash)
	{
		if (cache_signatures.find (hash) != cache_signatures.end ())
		{
			delete[] cache_signatures[hash];
			cache_signatures.erase (hash);
		}

		if (cache_versions.find (hash) != cache_versions.end ())
		{
			delete[] cache_versions[hash];
			cache_versions.erase (hash);
		}

		for (size_t i = 0; i < rules_custom.size (); i++)
		{
			PITEM_RULE ptr_rule = rules_custom.at (i);

			if (ptr_rule)
			{
				if (ptr_rule->apps.find (hash) != ptr_rule->apps.end ())
				{
					ptr_rule->apps.erase (hash);

					if (ptr_rule->is_enabled && !is_enabled)
						is_enabled = true;

					if (ptr_rule->is_enabled && ptr_rule->apps.empty ())
						ptr_rule->is_enabled = false;
				}
			}
		}

		_r_fastlock_acquireexclusive (&lock_notification);
		_app_freenotify (hash, false);
		_r_fastlock_releaseexclusive (&lock_notification);

		if (ptr_app->htimer)
			DeleteTimerQueueTimer (config.htimer, ptr_app->htimer, nullptr);

		_app_notifyrefresh ();

		apps.erase (hash);
	}

	return is_enabled;
}

void _app_freerule (PITEM_RULE* ptr)
{
	if (ptr)
	{
		PITEM_RULE ptr_rule = *ptr;

		if (ptr_rule)
		{
			if (ptr_rule->pname)
			{
				delete[] (ptr_rule->pname);
				ptr_rule->pname = nullptr;
			}

			if (ptr_rule->prule_remote)
			{
				delete[] (ptr_rule->prule_remote);
				ptr_rule->prule_remote = nullptr;
			}

			if (ptr_rule->prule_local)
			{
				delete[] (ptr_rule->prule_local);
				ptr_rule->prule_local = nullptr;
			}

			delete ptr_rule;

			ptr_rule = nullptr;
			*ptr = nullptr;
		}
	}
}

COLORREF _app_getcolorvalue (size_t hash)
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
		return colors.at (idx).clr;

	return 0;
}

bool _app_isunused (ITEM_APP const *ptr_app, size_t hash)
{
	if (ptr_app->is_undeletable || ptr_app->is_enabled || ptr_app->is_silent || _app_isapphaverule (hash))
		return false;

	return true;
}

bool _app_isexists (ITEM_APP const *ptr_app)
{
	if (ptr_app->is_enabled && ptr_app->is_haveerrors)
		return false;

	if (ptr_app->is_undeletable)
		return true;

	if (ptr_app->type == AppRegular)
		return _r_fs_exists (ptr_app->real_path);

	else if (ptr_app->type == AppStore)
		return _app_item_get (&packages, _r_str_hash (ptr_app->original_path), nullptr, nullptr, nullptr, nullptr, nullptr);

	else if (ptr_app->type == AppService)
		return _app_item_get (&services, _r_str_hash (ptr_app->original_path), nullptr, nullptr, nullptr, nullptr, nullptr);

	return true;
}

bool _app_istimeractive (ITEM_APP const *ptr_app)
{
	return ptr_app->timer && (ptr_app->timer > _r_unixtime_now ());
}

bool _app_istimersactive ()
{
	bool result = false;

	_r_fastlock_acquireshared (&lock_access);

	for (auto &p : apps)
	{
		if (p.second.htimer)
		{
			result = true;
			break;
		}
	}

	_r_fastlock_releaseshared (&lock_access);

	return result;
}

COLORREF _app_getcolor (size_t hash)
{
	_r_fastlock_acquireshared (&lock_access);

	rstring color_value;
	PITEM_APP const ptr_app = _app_getapplication (hash);

	if (ptr_app)
	{
		if (app.ConfigGet (L"IsHighlightTimer", true).AsBool () && _app_istimeractive (ptr_app))
			color_value = L"ColorTimer";

		else if (app.ConfigGet (L"IsHighlightInvalid", true).AsBool () && !_app_isexists (ptr_app))
			color_value = L"ColorInvalid";

		else if (app.ConfigGet (L"IsHighlightSpecial", true).AsBool () && _app_isapphaverule (hash))
			color_value = L"ColorSpecial";

		else if (ptr_app->is_silent && app.ConfigGet (L"IsHighlightSilent", true).AsBool ())
			color_value = L"ColorSilent";

		else if (ptr_app->is_signed && app.ConfigGet (L"IsHighlightSigned", true).AsBool () && app.ConfigGet (L"IsCerificatesEnabled", false).AsBool ())
			color_value = L"ColorSigned";

		else if ((ptr_app->type == AppService) && app.ConfigGet (L"IsHighlightService", true).AsBool ())
			color_value = L"ColorService";

		else if ((ptr_app->type == AppStore) && app.ConfigGet (L"IsHighlightPackage", true).AsBool ())
			color_value = L"ColorPackage";

		else if ((ptr_app->type == AppPico) && app.ConfigGet (L"IsHighlightPico", true).AsBool ())
			color_value = L"ColorPico";

		else if (ptr_app->is_system && app.ConfigGet (L"IsHighlightSystem", true).AsBool ())
			color_value = L"ColorSystem";
	}

	_r_fastlock_releaseshared (&lock_access);

	return _app_getcolorvalue (color_value.Hash ());
}

rstring _app_gettooltip (size_t hash)
{
	rstring result;

	_r_fastlock_acquireshared (&lock_access);

	PITEM_APP ptr_app = _app_getapplication (hash);

	if (ptr_app)
	{
		result = ptr_app->real_path[0] ? ptr_app->real_path : ptr_app->display_name;

		// file information
		if (ptr_app->type == AppRegular)
		{
			rstring display_name;

			_app_getinformation (hash, ptr_app->real_path, &ptr_app->description);
			display_name = ptr_app->description;

			if (!display_name.IsEmpty ())
				result.AppendFormat (L"\r\n%s:\r\n" SZ_TAB L"%s" SZ_TAB, app.LocaleString (IDS_FILE, nullptr).GetString (), display_name.GetString ());
		}
		else if (ptr_app->type == AppService)
		{
			rstring display_name;
			_app_item_get (&services, hash, &display_name, nullptr, nullptr, nullptr, nullptr);

			if (!display_name.IsEmpty ())
				result.AppendFormat (L"\r\n%s:\r\n" SZ_TAB L"%s\r\n" SZ_TAB L"%s" SZ_TAB, app.LocaleString (IDS_FILE, nullptr).GetString (), ptr_app->original_path, display_name.GetString ());
		}

		// signature information
		if (app.ConfigGet (L"IsCerificatesEnabled", false).AsBool () && ptr_app->is_signed && ptr_app->signer)
			result.AppendFormat (L"\r\n%s:\r\n" SZ_TAB L"%s", app.LocaleString (IDS_SIGNATURE, nullptr).GetString (), ptr_app->signer);

		// timer information
		if (_app_istimeractive (ptr_app))
			result.AppendFormat (L"\r\n%s:\r\n" SZ_TAB L"%s", app.LocaleString (IDS_TIMELEFT, nullptr).GetString (), _r_fmt_interval (ptr_app->timer - _r_unixtime_now (), 3).GetString ());

		// notes
		{
			rstring buffer;

			if (!_app_isexists (ptr_app))
				buffer.AppendFormat (SZ_TAB L"%s\r\n", app.LocaleString (IDS_HIGHLIGHT_INVALID, nullptr).GetString ());

			if (ptr_app->is_silent)
				buffer.AppendFormat (SZ_TAB L"%s\r\n", app.LocaleString (IDS_HIGHLIGHT_SILENT, nullptr).GetString ());

			if (ptr_app->is_system)
				buffer.AppendFormat (SZ_TAB L"%s\r\n", app.LocaleString (IDS_HIGHLIGHT_SYSTEM, nullptr).GetString ());

			// app type
			{
				if (ptr_app->type == AppNetwork)
					buffer.AppendFormat (SZ_TAB L"%s\r\n", app.LocaleString (IDS_HIGHLIGHT_NETWORK, nullptr).GetString ());

				else if (ptr_app->type == AppPico)
					buffer.AppendFormat (SZ_TAB L"%s\r\n", app.LocaleString (IDS_HIGHLIGHT_PICO, nullptr).GetString ());

				else if (ptr_app->type == AppStore)
					buffer.AppendFormat (SZ_TAB L"%s\r\n", app.LocaleString (IDS_HIGHLIGHT_PACKAGE, nullptr).GetString ());

				else if (ptr_app->type == AppService)
					buffer.AppendFormat (SZ_TAB L"%s\r\n", app.LocaleString (IDS_HIGHLIGHT_SERVICE, nullptr).GetString ());
			}

			if (!buffer.IsEmpty ())
			{
				buffer.InsertFormat (0, L"\r\n%s:\r\n", app.LocaleString (IDS_NOTES, nullptr).GetString ());
				result.Append (buffer);
			}
		}
	}

	_r_fastlock_releaseshared (&lock_access);

	return result;
}

bool _wfp_transact_start (UINT line)
{
	if (config.hengine)
	{
		const DWORD result = FwpmTransactionBegin (config.hengine, 0);

		if (result == ERROR_SUCCESS)
			return true;

		_app_logerror (L"FwpmTransactionBegin", result, _r_fmt (L"#%d", line), false);
	}

	return false;
}

bool _wfp_transact_commit (UINT line)
{
	if (config.hengine)
	{
		const DWORD result = FwpmTransactionCommit (config.hengine);

		if (result == ERROR_SUCCESS)
			return true;

		FwpmTransactionAbort (config.hengine);

		_app_logerror (L"FwpmTransactionCommit", result, _r_fmt (L"#%d", line), false);
	}

	return false;
}

void _wfp_destroyfilters (bool is_full)
{
	if (!config.hengine)
		return;

	// dropped packets logging (win7+)
	if (config.is_neteventset && _r_sys_validversion (6, 1))
		_wfp_logunsubscribe ();

	for (auto &p : apps)
	{
		p.second.is_haveerrors = false;
		p.second.mfarr.clear ();
	}

	std::vector<PITEM_RULE>* mrules[] = {
		&rules_blocklist,
		&rules_system,
		&rules_custom
	};

	for (size_t i = 0; i < _countof (mrules); i++)
	{
		std::vector<PITEM_RULE>* ptr_rules = mrules[i];

		for (size_t j = 0; j < ptr_rules->size (); j++)
		{
			PITEM_RULE ptr_rule = ptr_rules->at (j);

			if (ptr_rule)
			{
				ptr_rule->is_haveerrors = false;
				ptr_rule->mfarr.clear ();
			}
		}
	}

	_wfp_transact_start (__LINE__);

	HANDLE henum = nullptr;
	DWORD result = FwpmFilterCreateEnumHandle (config.hengine, nullptr, &henum);

	if (result != ERROR_SUCCESS)
	{
		_app_logerror (L"FwpmFilterCreateEnumHandle", result, nullptr, false);
	}
	else
	{
		UINT32 count = 0;
		FWPM_FILTER** matchingFwpFilter = nullptr;

		result = FwpmFilterEnum (config.hengine, henum, UINT32_MAX, &matchingFwpFilter, &count);

		if (result != ERROR_SUCCESS)
		{
			_app_logerror (L"FwpmFilterEnum", result, nullptr, false);
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
							_app_logerror (L"FwpmFilterDeleteById", result, nullptr, false);
					}
				}

				FwpmFreeMemory ((void**)&matchingFwpFilter);
			}
		}
	}

	if (henum)
		FwpmFilterDestroyEnumHandle (config.hengine, henum);

	_wfp_transact_commit (__LINE__);

	if (is_full)
	{
		// set icons
		SendMessage (app.GetHWND (), WM_SETICON, ICON_SMALL, (LPARAM)app.GetSharedIcon (app.GetHINSTANCE (), IDI_INACTIVE, GetSystemMetrics (SM_CXSMICON)));
		SendMessage (app.GetHWND (), WM_SETICON, ICON_BIG, (LPARAM)app.GetSharedIcon (app.GetHINSTANCE (), IDI_INACTIVE, GetSystemMetrics (SM_CXICON)));

		SendDlgItemMessage (app.GetHWND (), IDC_START_BTN, BM_SETIMAGE, IMAGE_ICON, (WPARAM)app.GetSharedIcon (app.GetHINSTANCE (), IDI_ACTIVE, GetSystemMetrics (SM_CXSMICON)));

		app.TraySetInfo (app.GetHWND (), UID, nullptr, app.GetSharedIcon (app.GetHINSTANCE (), IDI_INACTIVE, GetSystemMetrics (SM_CXSMICON)), nullptr);

		SetDlgItemText (app.GetHWND (), IDC_START_BTN, app.LocaleString (IDS_TRAY_START, nullptr));
	}
}

DWORD _wfp_createfilter (LPCWSTR name, FWPM_FILTER_CONDITION* lpcond, UINT32 const count, UINT8 weight, const GUID* layer, const GUID* callout, FWP_ACTION_TYPE action, UINT32 flags, MARRAY* pmar = nullptr)
{
	FWPM_FILTER filter = {0};

	WCHAR fltr_name[128] = {0};
	StringCchCopy (fltr_name, _countof (fltr_name), name ? name : APP_NAME);

	filter.displayData.name = fltr_name;
	filter.displayData.description = fltr_name;

	// set filter flags
	if ((flags & FWPM_FILTER_FLAG_BOOTTIME) == 0)
	{
		filter.flags = FWPM_FILTER_FLAG_PERSISTENT;

		// filter is indexed to help enable faster lookup during classification (win8+)
		if (_r_sys_validversion (6, 2))
			filter.flags |= FWPM_FILTER_FLAG_INDEXED;
	}

	if (flags)
		filter.flags |= flags;

	filter.providerKey = (LPGUID)&GUID_WfpProvider;
	filter.subLayerKey = GUID_WfpSublayer;

	if (count)
	{
		filter.numFilterConditions = count;
		filter.filterCondition = lpcond;
	}

	if (layer)
		CopyMemory (&filter.layerKey, layer, sizeof (GUID));

	if (callout)
		CopyMemory (&filter.action.calloutKey, callout, sizeof (GUID));

	filter.weight.type = FWP_UINT8;
	filter.weight.uint8 = weight;

	filter.action.type = action;

	UINT64 filter_id = 0;
	const DWORD result = FwpmFilterAdd (config.hengine, &filter, nullptr, &filter_id);

	// issue #229
	//if (result == FWP_E_PROVIDER_NOT_FOUND)
	//{
	//}

	if (result == ERROR_SUCCESS)
	{
		if (pmar)
			pmar->push_back (filter_id);
	}
	else
	{
		_app_logerror (L"FwpmFilterAdd", result, name, false);
	}

	return result;
}

INT CALLBACK _app_listviewcmp_ab (LPARAM item1, LPARAM item2, LPARAM lparam)
{
	HWND hwnd = GetParent ((HWND)lparam);
	const UINT ctrl_id = GetDlgCtrlID ((HWND)lparam);

	const bool is_checked1 = _r_listview_isitemchecked (hwnd, ctrl_id, (size_t)item1);
	const bool is_checked2 = _r_listview_isitemchecked (hwnd, ctrl_id, (size_t)item2);

	if (is_checked1 < is_checked2)
		return 1;

	else if (is_checked1 > is_checked2)
		return -1;

	const rstring str1 = _r_listview_getitemtext (hwnd, ctrl_id, (size_t)item1, 0);
	const rstring str2 = _r_listview_getitemtext (hwnd, ctrl_id, (size_t)item2, 0);

	return StrCmpLogicalW (str1, str2);
}

INT CALLBACK _app_listviewcompare (LPARAM lp1, LPARAM lp2, LPARAM lparam)
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

void _app_listviewsort_ab (HWND hwnd, UINT ctrl_id)
{
	SendDlgItemMessage (hwnd, ctrl_id, LVM_SORTITEMSEX, (WPARAM)GetDlgItem (hwnd, ctrl_id), (LPARAM)&_app_listviewcmp_ab);
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

	config.is_nocheckboxnotify = true;

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

	config.is_nocheckboxnotify = false;

	SendDlgItemMessage (hwnd, ctrl_id, LVM_SORTITEMS, lparam, (LPARAM)&_app_listviewcompare);

	_r_fastlock_releaseshared (&lock_access);

	_app_refreshstatus (hwnd);
}

//bool _app_canihaveaccess ()
//{
//	bool result = false;
//
//	_r_fastlock_acquireshared (&lock_access);
//
//	PITEM_APP const ptr_app = _app_getapplication (config.myhash);
//
//	if (ptr_app)
//	{
//		const EnumMode mode = (EnumMode)app.ConfigGet (L"Mode", ModeWhitelist).AsUint ();
//
//		result = (mode == ModeWhitelist && ptr_app->is_enabled || mode == ModeBlacklist && !ptr_app->is_enabled);
//	}
//
//	_r_fastlock_releaseshared (&lock_access);
//
//	return result;
//}

bool _app_ruleisport (LPCWSTR rule)
{
	if (!rule || !rule[0])
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
	if (!rule || !rule[0])
		return false;

	NET_ADDRESS_INFO ni;
	SecureZeroMemory (&ni, sizeof (ni));

	USHORT port = 0;
	BYTE prefix_length = 0;

	static const DWORD types = NET_STRING_NAMED_ADDRESS | NET_STRING_NAMED_SERVICE;
	const DWORD errcode = ParseNetworkString (rule, types, &ni, &port, &prefix_length);

	return (errcode == ERROR_SUCCESS);
}

bool _app_ruleisip (LPCWSTR rule)
{
	if (!rule || !rule[0])
		return false;

	NET_ADDRESS_INFO ni;
	SecureZeroMemory (&ni, sizeof (ni));

	USHORT port = 0;
	BYTE prefix_length = 0;

	static const DWORD types = NET_STRING_IP_ADDRESS | NET_STRING_IP_SERVICE | NET_STRING_IP_NETWORK | NET_STRING_IP_ADDRESS_NO_SCOPE | NET_STRING_IP_SERVICE_NO_SCOPE;
	const DWORD errcode = ParseNetworkString (rule, types, &ni, &port, &prefix_length);

	return (errcode == ERROR_SUCCESS);
}

rstring _app_parsehostaddress (LPCWSTR host, USHORT port)
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

				delete pSrvList;
				pSrvList = nullptr;
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
					result.Append (_app_parsehostaddress (current->Data.CNAME.pNameHost, port));
			}
		}

		if (pSrvList)
		{
			delete pSrvList;
			pSrvList = nullptr;
		}

		result.Trim (RULE_DELIMETER);

		DnsRecordListFree (ppQueryResultsSet, DnsFreeRecordList);
	}

	return result;
}

bool _app_parsenetworkstring (rstring network_string, NET_ADDRESS_FORMAT* format_ptr, USHORT* port_ptr, FWP_V4_ADDR_AND_MASK* paddr4, FWP_V6_ADDR_AND_MASK* paddr6, LPWSTR paddr_dns)
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

				paddr4->mask = ntohl (mask);
				paddr4->addr = ntohl (ni.Ipv4Address.sin_addr.S_un.S_addr);
			}

			return true;
		}
		else if (ni.Format == NET_ADDRESS_IPV6)
		{
			if (paddr6)
			{
				paddr6->prefixLength = min ((INT)prefix_length, 128);
				CopyMemory (paddr6->addr, ni.Ipv6Address.sin6_addr.u.Byte, FWP_V6_ADDR_SIZE);
			}

			return true;
		}
		else if (ni.Format == NET_ADDRESS_DNS_NAME)
		{
			if (paddr_dns)
			{
				const rstring host = _app_parsehostaddress (ni.NamedAddress.Address, port);

				if (host.IsEmpty ())
					return false;

				StringCchCopy (paddr_dns, NI_MAXHOST, host);
			}

			return true;
		}
	}

	return false;
}

bool _app_parserulestring (rstring rule, PITEM_ADDRESS ptr_addr, EnumRuleType *ptype)
{
	rule.Trim (L"\r\n "); // trim whitespace

	if (rule.IsEmpty ())
		return true;

	if (rule.At (0) == L'*')
		return true;

	EnumRuleType type = TypeUnknown;
	size_t range_pos = range_pos = rule.Find (L'-');

	if (ptype)
		type = *ptype;

	// auto-parse rule type
	if (type == TypeUnknown)
	{
		if (_app_ruleisport (rule))
			type = TypePort;

		else if ((range_pos == rstring::npos) ? _app_ruleisip (rule) : (_app_ruleisip (rule.Midded (0, range_pos)) && _app_ruleisip (rule.Midded (range_pos + 1))))
			type = TypeIp;

		else if (_app_ruleishost (rule))
			type = TypeHost;

		else
			return false;
	}

	if (type == TypeUnknown)
		return false;

	if (!ptr_addr)
		return true;

	if (type == TypePort || type == TypeIp)
	{
		if (ptr_addr && range_pos != rstring::npos)
			ptr_addr->is_range = true;
	}

	WCHAR range_start[LEN_IP_MAX] = {0};
	WCHAR range_end[LEN_IP_MAX] = {0};

	if (type == TypePort || type == TypeIp)
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

		if (type == TypeIp && range_pos != rstring::npos)
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
						CopyMemory (ptr_addr->prange->valueLow.byteArray16->byteArray16, addr6.addr, FWP_V6_ADDR_SIZE);
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

			if (ptr_addr)
			{
				ptr_addr->format = format;
				ptr_addr->type = TypeIp;
			}
		}
		else
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
						CopyMemory (ptr_addr->paddr6->addr, addr6.addr, FWP_V6_ADDR_SIZE);
					}
				}
				else if (format == NET_ADDRESS_DNS_NAME)
				{
					//if (ptr_addr)
					//{
					//	ptr_addr->type = TypeHost;
					//	ptr_addr->host = <hosts>;
					//}
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
	}

	return true;
}

bool ByteBlobAlloc (PVOID data, size_t length, FWP_BYTE_BLOB** lpblob)
{
	if (!data || !length || !lpblob)
		return false;

	*lpblob = new FWP_BYTE_BLOB;

	if (*lpblob)
	{
		const PUINT8 tmp_ptr = new UINT8[length];

		if (!tmp_ptr)
		{
			delete *lpblob;
			*lpblob = nullptr;

			return false;
		}
		else
		{
			(*lpblob)->data = tmp_ptr;
			(*lpblob)->size = (UINT32)length;

			CopyMemory ((*lpblob)->data, data, length);

			return true;
		}
	}

	return false;
}

void ByteBlobFree (FWP_BYTE_BLOB** lpblob)
{
	if (lpblob && *lpblob)
	{
		FWP_BYTE_BLOB* blob = *lpblob;

		if (blob)
		{
			if (blob->data)
			{
				delete[] blob->data;
				blob->data = nullptr;
			}

			delete blob;
			blob = nullptr;
			*lpblob = nullptr;
		}
	}
}

DWORD _FwpmGetAppIdFromFileName1 (LPCWSTR path, FWP_BYTE_BLOB** lpblob, EnumAppType type)
{
	if (!path || !path[0] || !lpblob)
		return ERROR_BAD_ARGUMENTS;

	rstring path_buff;

	DWORD result = (DWORD)-1;

	if (type == AppRegular || type == AppNetwork || type == AppService)
	{
		path_buff = path;

		if (_r_str_hash (path) == config.ntoskrnl_hash)
		{
			result = ERROR_SUCCESS;
		}
		else
		{
			result = _r_path_ntpathfromdos (path_buff);

			// file is inaccessible or not found, maybe low-level driver preventing file access?
			// try another way!
			if (
				result == ERROR_ACCESS_DENIED ||
				result == ERROR_FILE_NOT_FOUND ||
				result == ERROR_PATH_NOT_FOUND
				)
			{
				if (PathIsRelative (path))
				{
					return result;
				}
				else
				{
					// file path (root)
					WCHAR path_root[MAX_PATH] = {0};
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
			}
			else if (result != ERROR_SUCCESS)
			{
				return result;
			}
		}
	}
	else if (type == AppPico || type == AppDevice)
	{
		path_buff = path;

		if (type == AppDevice)
			path_buff.ToLower (); // lower is important!

		result = ERROR_SUCCESS;
	}
	else
	{
		return ERROR_FILE_NOT_FOUND;
	}

	// allocate buffer
	if (!path_buff.IsEmpty ())
	{
		if (!ByteBlobAlloc ((LPVOID)path_buff.GetString (), (path_buff.GetLength () + 1) * sizeof (WCHAR), lpblob))
			return ERROR_OUTOFMEMORY;
	}
	else
	{
		return ERROR_BAD_ARGUMENTS;
	}

	return ERROR_SUCCESS;
}

bool _wfp_createrulefilter (LPCWSTR name, PITEM_APP const ptr_app, LPCWSTR rule_remote, LPCWSTR rule_local, UINT8 protocol, ADDRESS_FAMILY af, FWP_DIRECTION dir, EnumRuleType* ptype, UINT8 weight, FWP_ACTION_TYPE action, UINT32 flag, MARRAY* pmfarr)
{
	UINT32 count = 0;
	FWPM_FILTER_CONDITION fwfc[8] = {0};

	FWP_BYTE_BLOB* bPath = nullptr;
	FWP_BYTE_BLOB* bSid = nullptr;

	FWP_V4_ADDR_AND_MASK addr4 = {0};
	FWP_V6_ADDR_AND_MASK addr6 = {0};

	FWP_RANGE range;
	SecureZeroMemory (&range, sizeof (range));

	ITEM_ADDRESS addr;
	SecureZeroMemory (&addr, sizeof (addr));

	addr.paddr4 = &addr4;
	addr.paddr6 = &addr6;
	addr.prange = &range;

	bool is_remoteaddr_set = false;
	bool is_remoteport_set = false;

	// set path condition
	if (ptr_app)
	{
		if (ptr_app->type == AppStore) // windows store app (win8+)
		{
			if (ptr_app->psid)
			{
				fwfc[count].fieldKey = FWPM_CONDITION_ALE_PACKAGE_ID;
				fwfc[count].matchType = FWP_MATCH_EQUAL;
				fwfc[count].conditionValue.type = FWP_SID;
				fwfc[count].conditionValue.sid = (SID*)ptr_app->psid;

				count += 1;
			}
			else
			{
				_app_logerror (TEXT (__FUNCTION__), 0, ptr_app->display_name, true);
				return false;
			}
		}
		else if (ptr_app->type == AppService) // windows service
		{
			LPCWSTR path = _r_path_expand (PATH_SVCHOST);

			if (path)
			{
				const DWORD rc = _FwpmGetAppIdFromFileName1 (path, &bPath, ptr_app->type);

				if (rc == ERROR_SUCCESS)
				{
					fwfc[count].fieldKey = FWPM_CONDITION_ALE_APP_ID;
					fwfc[count].matchType = FWP_MATCH_EQUAL;
					fwfc[count].conditionValue.type = FWP_BYTE_BLOB_TYPE;
					fwfc[count].conditionValue.byteBlob = bPath;

					count += 1;
				}
			}

			if (ptr_app->psd && ByteBlobAlloc (ptr_app->psd, GetSecurityDescriptorLength (ptr_app->psd), &bSid))
			{
				fwfc[count].fieldKey = FWPM_CONDITION_ALE_USER_ID;
				fwfc[count].matchType = FWP_MATCH_EQUAL;
				fwfc[count].conditionValue.type = FWP_SECURITY_DESCRIPTOR_TYPE;
				fwfc[count].conditionValue.sd = bSid;

				count += 1;
			}
			else
			{
				ByteBlobFree (&bPath);
				ByteBlobFree (&bSid);

				_app_logerror (TEXT (__FUNCTION__), 0, ptr_app->display_name, true);

				return false;
			}
		}
		else
		{
			LPCWSTR path = ptr_app->original_path;
			const DWORD rc = _FwpmGetAppIdFromFileName1 (path, &bPath, ptr_app->type);

			if (rc == ERROR_SUCCESS)
			{
				fwfc[count].fieldKey = FWPM_CONDITION_ALE_APP_ID;
				fwfc[count].matchType = FWP_MATCH_EQUAL;
				fwfc[count].conditionValue.type = FWP_BYTE_BLOB_TYPE;
				fwfc[count].conditionValue.byteBlob = bPath;

				count += 1;
			}
			else
			{
				ByteBlobFree (&bSid);
				ByteBlobFree (&bPath);

				_app_logerror (L"FwpmGetAppIdFromFileName", rc, path, true);

				return false;
			}
		}
	}

	// set ip/port condition
	{
		LPCWSTR rules[] = {rule_remote, rule_local};

		for (size_t i = 0; i < _countof (rules); i++)
		{
			if (rules[i] && rules[i][0] && rules[i][0] != L'*')
			{
				if (!_app_parserulestring (rules[i], &addr, ptype))
				{
					ByteBlobFree (&bSid);
					ByteBlobFree (&bPath);

					_app_logerror (L"_app_parserulestring", ERROR_INVALID_NETNAME, _r_fmt (L"[%s: %s]", name, rules[i]), false);

					return false;
				}
				else
				{
					if (i == 0)
					{
						if (addr.type == TypeIp || addr.type == TypeHost)
							is_remoteaddr_set = true;

						else if (addr.type == TypePort)
							is_remoteport_set = true;
					}

					if (addr.is_range && (addr.type == TypeIp || addr.type == TypePort))
					{
						if (addr.type == TypeIp)
						{
							if (addr.format == NET_ADDRESS_IPV4)
							{
								af = AF_INET;
							}
							else if (addr.format == NET_ADDRESS_IPV6)
							{
								af = AF_INET6;
							}
							else
							{
								ByteBlobFree (&bSid);
								ByteBlobFree (&bPath);

								return false;
							}
						}

						fwfc[count].fieldKey = (addr.type == TypePort) ? ((i == 0) ? FWPM_CONDITION_IP_REMOTE_PORT : FWPM_CONDITION_IP_LOCAL_PORT) : ((i == 0) ? FWPM_CONDITION_IP_REMOTE_ADDRESS : FWPM_CONDITION_IP_LOCAL_ADDRESS);
						fwfc[count].matchType = FWP_MATCH_RANGE;
						fwfc[count].conditionValue.type = FWP_RANGE_TYPE;
						fwfc[count].conditionValue.rangeValue = &range;

						count += 1;
					}
					else if (addr.type == TypePort)
					{
						fwfc[count].fieldKey = ((i == 0) ? FWPM_CONDITION_IP_REMOTE_PORT : FWPM_CONDITION_IP_LOCAL_PORT);
						fwfc[count].matchType = FWP_MATCH_EQUAL;
						fwfc[count].conditionValue.type = FWP_UINT16;
						fwfc[count].conditionValue.uint16 = addr.port;

						count += 1;
					}
					else if (addr.type == TypeHost || addr.type == TypeIp)
					{
						fwfc[count].fieldKey = ((i == 0) ? FWPM_CONDITION_IP_REMOTE_ADDRESS : FWPM_CONDITION_IP_LOCAL_ADDRESS);
						fwfc[count].matchType = FWP_MATCH_EQUAL;

						if (addr.format == NET_ADDRESS_IPV4)
						{
							af = AF_INET;

							fwfc[count].conditionValue.type = FWP_V4_ADDR_MASK;
							fwfc[count].conditionValue.v4AddrMask = &addr4;

							count += 1;
						}
						else if (addr.format == NET_ADDRESS_IPV6)
						{
							af = AF_INET6;

							fwfc[count].conditionValue.type = FWP_V6_ADDR_MASK;
							fwfc[count].conditionValue.v6AddrMask = &addr6;

							count += 1;
						}
						else if (addr.format == NET_ADDRESS_DNS_NAME)
						{
							ByteBlobFree (&bSid);
							ByteBlobFree (&bPath);

							const rstring::rvector arr2 = rstring (addr.host).AsVector (RULE_DELIMETER);

							if (arr2.empty ())
							{
								return false;
							}
							else
							{
								for (size_t j = 0; j < arr2.size (); j++)
								{
									EnumRuleType type = TypeIp;

									if (!_wfp_createrulefilter (name, ptr_app, arr2[j], nullptr, protocol, af, dir, &type, weight, action, flag, pmfarr))
										return false;
								}
							}

							return true;
						}
						else
						{
							ByteBlobFree (&bSid);
							ByteBlobFree (&bPath);

							return false;
						}

						// set port if available
						if (addr.port)
						{
							fwfc[count].fieldKey = ((i == 0) ? FWPM_CONDITION_IP_REMOTE_PORT : FWPM_CONDITION_IP_LOCAL_PORT);
							fwfc[count].matchType = FWP_MATCH_EQUAL;
							fwfc[count].conditionValue.type = FWP_UINT16;
							fwfc[count].conditionValue.uint16 = addr.port;

							count += 1;
						}
					}
					else
					{
						ByteBlobFree (&bSid);
						ByteBlobFree (&bPath);

						return false;
					}
				}
			}
		}
	}

	// set protocol condition
	if (protocol)
	{
		fwfc[count].fieldKey = FWPM_CONDITION_IP_PROTOCOL;
		fwfc[count].matchType = FWP_MATCH_EQUAL;
		fwfc[count].conditionValue.type = FWP_UINT8;
		fwfc[count].conditionValue.uint8 = protocol;

		count += 1;
	}

	// create outbound layer filter
	if (dir == FWP_DIRECTION_OUTBOUND || dir == FWP_DIRECTION_MAX)
	{
		if (af == AF_INET || af == AF_UNSPEC)
		{
			_wfp_createfilter (name, fwfc, count, weight, &FWPM_LAYER_ALE_AUTH_CONNECT_V4, nullptr, action, flag, pmfarr);

			// win7+
			if (_r_sys_validversion (6, 1))
				_wfp_createfilter (name, fwfc, count, weight, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V4, nullptr, action, flag, pmfarr);
		}

		if (af == AF_INET6 || af == AF_UNSPEC)
		{
			_wfp_createfilter (name, fwfc, count, weight, &FWPM_LAYER_ALE_AUTH_CONNECT_V6, nullptr, action, flag, pmfarr);

			// win7+
			if (_r_sys_validversion (6, 1))
				_wfp_createfilter (name, fwfc, count, weight, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V6, nullptr, action, flag, pmfarr);
		}
	}

	// create inbound layer filter
	if (dir == FWP_DIRECTION_INBOUND || dir == FWP_DIRECTION_MAX)
	{
		if (af == AF_INET || af == AF_UNSPEC)
		{
			_wfp_createfilter (name, fwfc, count, weight, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, nullptr, action, flag, pmfarr);

			if (action == FWP_ACTION_BLOCK && (!is_remoteaddr_set && !is_remoteport_set))
				_wfp_createfilter (name, fwfc, count, weight, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V4, nullptr, FWP_ACTION_PERMIT, flag, pmfarr);
		}

		if (af == AF_INET6 || af == AF_UNSPEC)
		{
			_wfp_createfilter (name, fwfc, count, weight, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, action, flag, pmfarr);

			if (action == FWP_ACTION_BLOCK && (!is_remoteaddr_set && !is_remoteport_set))
				_wfp_createfilter (name, fwfc, count, weight, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V6, nullptr, FWP_ACTION_PERMIT, flag, pmfarr);
		}
	}

	// create listen layer filter
	if (!app.ConfigGet (L"AllowListenConnections2", true).AsBool () && !protocol && dir != FWP_DIRECTION_OUTBOUND && (!is_remoteaddr_set && !is_remoteport_set))
	{
		if (af == AF_INET || af == AF_UNSPEC)
			_wfp_createfilter (name, fwfc, count, weight, &FWPM_LAYER_ALE_AUTH_LISTEN_V4, nullptr, action, flag, pmfarr);

		if (af == AF_INET6 || af == AF_UNSPEC)
			_wfp_createfilter (name, fwfc, count, weight, &FWPM_LAYER_ALE_AUTH_LISTEN_V6, nullptr, action, flag, pmfarr);
	}

	ByteBlobFree (&bSid);
	ByteBlobFree (&bPath);

	return true;
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

void str_set (LPWSTR* pwstr, size_t length, LPCWSTR text)
{
	if (pwstr)
	{
		if (*pwstr)
		{
			delete[] (*pwstr);
			(*pwstr) = nullptr;
		}

		LPWSTR new_ptr = new WCHAR[length];

		if (new_ptr)
		{
			StringCchCopy (new_ptr, length, text);
			*pwstr = new_ptr;
		}
	}
}

void _app_loadrules (HWND hwnd, LPCWSTR path, LPCWSTR path_backup, bool is_internal, std::vector<PITEM_RULE> *ptr_rules, UINT8 weight, time_t *ptimestamp)
{
	if (!ptr_rules)
		return;

	pugi::xml_document doc_original;
	pugi::xml_document doc_backup;

	pugi::xml_node root;
	pugi::xml_parse_result result_original = doc_original.load_file (path, PUGIXML_LOAD_FLAGS, PUGIXML_LOAD_ENCODING);

	//if (!result)
	{
		// if file not found or parsing error, load from backup
		if (path_backup)
		{
			if (is_internal)
			{
				DWORD size = 0;
				const LPVOID buffer = _app_loadresource (path_backup, &size);

				pugi::xml_parse_result result_backup;

				if (buffer)
					result_backup = doc_backup.load_buffer (buffer, size, PUGIXML_LOAD_FLAGS, PUGIXML_LOAD_ENCODING);

				if (result_backup && (!result_original || doc_backup.child (L"root").attribute (L"timestamp").as_ullong () > doc_original.child (L"root").attribute (L"timestamp").as_ullong ()))
				{
					root = doc_backup.child (L"root");
					result_original = result_backup;
				}
			}
			else
			{
				if (!result_original && _r_fs_exists (path_backup))
					result_original = doc_original.load_file (path_backup, PUGIXML_LOAD_FLAGS, PUGIXML_LOAD_ENCODING);
			}
		}

		// show only syntax, memory and i/o errors...
		if (!result_original && result_original.status != pugi::status_file_not_found)
			_app_logerror (L"pugi::load_file", 0, _r_fmt (L"status: %d,offset: %d,text: %s,file: %s", result_original.status, result_original.offset, rstring (result_original.description ()).GetString (), path), false);
	}

	if (result_original)
	{
		if (!root)
			root = doc_original.child (L"root");

		if (root)
		{
			if (root.attribute (L"type").empty () || root.attribute (L"type").as_uint () == XmlRules)
			{
				// clear old entries
				{
					for (size_t i = 0; i < ptr_rules->size (); i++)
						_app_freerule (&ptr_rules->at (i));

					ptr_rules->clear ();
				}

				if (ptimestamp)
					*ptimestamp = root.attribute (L"timestamp").as_ullong ();

				for (pugi::xml_node item = root.child (L"item"); item; item = item.next_sibling (L"item"))
				{
					PITEM_RULE rule_ptr = new ITEM_RULE;

					if (rule_ptr)
					{
						// allocate required memory
						{
							const rstring attr_name = item.attribute (L"name").as_string ();
							const rstring attr_rule_remote = item.attribute (L"rule").as_string ();
							const rstring attr_rule_local = item.attribute (L"rule_local").as_string ();

							const size_t name_length = min (attr_name.GetLength (), RULE_NAME_CCH_MAX) + 1;
							const size_t rule_remote_length = min (attr_rule_remote.GetLength (), RULE_RULE_CCH_MAX) + 1;
							const size_t rule_local_length = min (attr_rule_local.GetLength (), RULE_RULE_CCH_MAX) + 1;

							str_set (&rule_ptr->pname, name_length, attr_name);
							str_set (&rule_ptr->prule_remote, rule_remote_length, attr_rule_remote);
							str_set (&rule_ptr->prule_local, rule_local_length, attr_rule_local);
						}

						rule_ptr->dir = (FWP_DIRECTION)item.attribute (L"dir").as_uint ();

						if (!item.attribute (L"type").empty ())
							rule_ptr->type = (EnumRuleType)item.attribute (L"type").as_uint ();

						rule_ptr->protocol = (UINT8)item.attribute (L"protocol").as_uint ();
						rule_ptr->af = (ADDRESS_FAMILY)item.attribute (L"version").as_uint ();

						if (!item.attribute (L"apps").empty ())
						{
							rstring::rvector arr = rstring (item.attribute (L"apps").as_string ()).AsVector (RULE_DELIMETER);

							for (size_t i = 0; i < arr.size (); i++)
							{
								const rstring path_app = _r_path_expand (arr.at (i).Trim (L"\r\n "));
								size_t hash = path_app.Hash ();

								if (hash)
								{
									if (!_app_getapplication (hash))
										hash = _app_addapplication (hwnd, path_app, 0, 0, 0, false, false, true);

									if (is_internal)
										apps[hash].is_undeletable = true; // prevent deletion for internal rules apps

									rule_ptr->apps[hash] = true;
								}
							}
						}

						rule_ptr->is_block = item.attribute (L"is_block").as_bool ();
						rule_ptr->is_enabled = item.attribute (L"is_enabled").as_bool ();

						// calculate rule weight
						{
							if (weight == FILTER_WEIGHT_CUSTOM && rule_ptr->is_block)
								rule_ptr->weight = FILTER_WEIGHT_CUSTOM_BLOCK;

							else
								rule_ptr->weight = weight;
						}

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
			}

			// sort rules alphabeticaly
			std::sort (ptr_rules->begin (), ptr_rules->end (),
				[](const PITEM_RULE& a, const PITEM_RULE& b)->bool {
				return StrCmpLogicalW (a->pname, b->pname) == -1;
			});
		}
	}
}

void _app_profileload (HWND hwnd, LPCWSTR path_apps = nullptr, LPCWSTR path_rules = nullptr)
{
	// load applications
	{
		const size_t item_id = (size_t)SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
		const INT scroll_pos = GetScrollPos (GetDlgItem (hwnd, IDC_LISTVIEW), SB_VERT);
		bool is_meadded = false;

		// generate package list (win8+)
		if (_r_sys_validversion (6, 2))
			_app_generate_packages ();

		// generate services list
		_app_generate_services ();

		_r_fastlock_acquireexclusive (&lock_access);

		// load apps list
		{
			pugi::xml_document doc;
			pugi::xml_parse_result result = doc.load_file (path_apps ? path_apps : config.apps_path, PUGIXML_LOAD_FLAGS, PUGIXML_LOAD_ENCODING);

			// load backup
			if (!result)
			{
				if (_r_fs_exists (config.apps_path_backup))
					result = doc.load_file (config.apps_path_backup, PUGIXML_LOAD_FLAGS, PUGIXML_LOAD_ENCODING);
			}

			if (!result)
			{
				// show only syntax, memory and i/o errors...
				if (result.status != pugi::status_file_not_found)
					_app_logerror (L"pugi::load_file", 0, _r_fmt (L"status: %d,offset: %d,text: %s,file: %s", result.status, result.offset, rstring (result.description ()).GetString (), path_apps ? path_apps : config.apps_path), false);
			}
			else
			{
				pugi::xml_node root = doc.child (L"root");

				if (root)
				{
					if (root.attribute (L"type").empty () || root.attribute (L"type").as_uint () == XmlApps)
					{
						apps.clear ();
						_r_listview_deleteallitems (hwnd, IDC_LISTVIEW);

						for (pugi::xml_node item = root.child (L"item"); item; item = item.next_sibling (L"item"))
						{
							const size_t hash = _r_str_hash (item.attribute (L"path").as_string ());

							if (hash == config.myhash)
								is_meadded = true;

							_app_addapplication (hwnd, item.attribute (L"path").as_string (), item.attribute (L"timestamp").as_ullong (), item.attribute (L"timer").as_ullong (), item.attribute (L"last_notify").as_ullong (), item.attribute (L"is_silent").as_bool (), item.attribute (L"is_enabled").as_bool (), true);
						}
					}
				}
			}

			_app_listviewsort (hwnd, IDC_LISTVIEW, -1, false);
			_r_listview_redraw (hwnd, IDC_LISTVIEW);
		}

		if (!is_meadded)
			_app_addapplication (hwnd, app.GetBinaryPath (), 0, 0, 0, false, (app.ConfigGet (L"Mode", ModeWhitelist).AsUint () == ModeWhitelist) ? true : false, true);

		apps[config.myhash].is_undeletable = true; // disable deletion for me ;)

		ShowItem (hwnd, IDC_LISTVIEW, item_id, scroll_pos);
	}

	// load rules config
	{
		pugi::xml_document doc;
		pugi::xml_parse_result result = doc.load_file (config.rules_config_path, PUGIXML_LOAD_FLAGS, PUGIXML_LOAD_ENCODING);

		if (!result && _r_fs_exists (config.rules_config_path_backup))
			result = doc.load_file (config.rules_config_path_backup, PUGIXML_LOAD_FLAGS, PUGIXML_LOAD_ENCODING);

		if (!result)
		{
			// show only syntax, memory and i/o errors...
			if (result.status != pugi::status_file_not_found)
				_app_logerror (L"pugi::load_file", 0, _r_fmt (L"status: %d,offset: %d,text: %s,file: %s", result.status, result.offset, rstring (result.description ()).GetString (), config.rules_config_path), false);
		}
		else
		{
			pugi::xml_node root = doc.child (L"root");

			if (root)
			{
				if (root.attribute (L"type").empty () || root.attribute (L"type").as_uint () == XmlRulesConfig)
				{
					rules_config.clear ();

					for (pugi::xml_node item = root.child (L"item"); item; item = item.next_sibling (L"item"))
						rules_config[item.attribute (L"name").as_string ()] = item.attribute (L"is_enabled").as_bool ();
				}
			}
		}
	}

	// load blocklist rules (internal)
	_app_loadrules (hwnd, config.rules_blocklist_path, MAKEINTRESOURCE (IDR_RULES_BLOCKLIST), true, &rules_blocklist, FILTER_WEIGHT_BLOCKLIST, &config.blocklist_timestamp);

	// load system rules (internal)
	_app_loadrules (hwnd, config.rules_system_path, MAKEINTRESOURCE (IDR_RULES_SYSTEM), true, &rules_system, FILTER_WEIGHT_SYSTEM, &config.rule_system_timestamp);

	// load custom rules
	_app_loadrules (hwnd, path_rules ? path_rules : config.rules_custom_path, config.rules_custom_path_backup, false, &rules_custom, FILTER_WEIGHT_CUSTOM, nullptr);

	_r_fastlock_releaseexclusive (&lock_access);

	_app_refreshstatus (hwnd);
}

bool _app_isrulepresent (size_t hash)
{
	if (!hash)
		return false;

	for (size_t i = 0; i < rules_blocklist.size (); i++)
	{
		PITEM_RULE ptr_rule = rules_blocklist.at (i);

		if (ptr_rule && ptr_rule->pname && _r_str_hash (ptr_rule->pname) == hash)
			return true;
	}

	for (size_t i = 0; i < rules_system.size (); i++)
	{
		PITEM_RULE ptr_rule = rules_system.at (i);

		if (ptr_rule && ptr_rule->pname && _r_str_hash (ptr_rule->pname) == hash)
			return true;
	}

	return false;
}

void _app_profilesave (HWND hwnd, LPCWSTR path_apps = nullptr, LPCWSTR path_rules = nullptr)
{
	const time_t current_time = _r_unixtime_now ();
	const time_t notification_timeout = app.ConfigGet (L"NotificationsTimeout", NOTIFY_TIMEOUT_DEFAULT).AsLonglong ();
	const bool is_backuprequired = app.ConfigGet (L"IsBackupProfile", true).AsBool () && (((current_time - app.ConfigGet (L"BackupTimestamp", 0).AsLonglong ()) >= app.ConfigGet (L"BackupPeriod", _R_SECONDSCLOCK_HOUR (BACKUP_HOURS_PERIOD)).AsLonglong ()) || !_r_fs_exists (config.apps_path_backup) || !_r_fs_exists (config.rules_custom_path_backup) || !_r_fs_exists (config.rules_config_path_backup));
	bool is_backupcreated = false;

	_r_fastlock_acquireshared (&lock_access);

	// save apps
	{
		pugi::xml_document doc;
		pugi::xml_node root = doc.append_child (L"root");

		if (root)
		{
			const size_t count = _r_listview_getitemcount (hwnd, IDC_LISTVIEW);

			root.append_attribute (L"timestamp").set_value (current_time);
			root.append_attribute (L"type").set_value (XmlApps);

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
							PITEM_APP const ptr_app = _app_getapplication (hash);

							if (ptr_app)
							{
								item.append_attribute (L"path").set_value (ptr_app->original_path);
								item.append_attribute (L"timestamp").set_value (ptr_app->timestamp);

								// set timer (if presented)
								if (_app_istimeractive (ptr_app))
									item.append_attribute (L"timer").set_value (ptr_app->timer);

								// set last notification timestamp (if presented)
								if (notification_timeout && ptr_app->last_notify && ((current_time - ptr_app->last_notify) < notification_timeout))
									item.append_attribute (L"last_notify").set_value (ptr_app->last_notify);

								if (ptr_app->is_silent)
									item.append_attribute (L"is_silent").set_value (ptr_app->is_silent);

								if (ptr_app->is_enabled)
									item.append_attribute (L"is_enabled").set_value (ptr_app->is_enabled);
							}
						}
					}
				}
			}
			else
			{
				for (auto &p : apps)
				{
					const size_t hash = p.first;

					if (hash)
					{
						pugi::xml_node item = root.append_child (L"item");

						if (item)
						{
							PITEM_APP const ptr_app = &p.second;

							item.append_attribute (L"path").set_value (ptr_app->original_path);
							item.append_attribute (L"timestamp").set_value (ptr_app->timestamp);

							// set timer (if presented)
							if (_app_istimeractive (ptr_app))
								item.append_attribute (L"timer").set_value (ptr_app->timer);

							if (ptr_app->is_silent)
								item.append_attribute (L"is_silent").set_value (ptr_app->is_silent);

							if (ptr_app->is_enabled)
								item.append_attribute (L"is_enabled").set_value (ptr_app->is_enabled);
						}
					}
				}
			}

			doc.save_file (path_apps ? path_apps : config.apps_path, L"\t", PUGIXML_SAVE_FLAGS, PUGIXML_SAVE_ENCODING);

			// make backup
			if (!path_apps && !apps.empty () && is_backuprequired)
			{
				doc.save_file (config.apps_path_backup, L"\t", PUGIXML_SAVE_FLAGS, PUGIXML_SAVE_ENCODING);
				is_backupcreated = true;
			}
		}
	}

	// save internal rules config
	{
		pugi::xml_document doc;
		pugi::xml_node root = doc.append_child (L"root");

		if (root)
		{
			root.append_attribute (L"timestamp").set_value (current_time);
			root.append_attribute (L"type").set_value (XmlRulesConfig);

			for (auto const &p : rules_config)
			{
				pugi::xml_node item = root.append_child (L"item");

				if (item)
				{
					if (_app_isrulepresent (_r_str_hash (p.first)))
					{
						item.append_attribute (L"name").set_value (p.first);
						item.append_attribute (L"is_enabled").set_value (p.second);
					}
				}
			}

			doc.save_file (config.rules_config_path, L"\t", PUGIXML_SAVE_FLAGS, PUGIXML_SAVE_ENCODING);

			// make backup
			if (!rules_config.empty () && is_backuprequired)
			{
				doc.save_file (config.rules_config_path_backup, L"\t", PUGIXML_SAVE_FLAGS, PUGIXML_SAVE_ENCODING);
				is_backupcreated = true;
			}
		}
	}

	// save custom rules
	{
		pugi::xml_document doc;
		pugi::xml_node root = doc.append_child (L"root");

		if (root)
		{
			root.append_attribute (L"timestamp").set_value (current_time);
			root.append_attribute (L"type").set_value (XmlRules);

			for (size_t i = 0; i < rules_custom.size (); i++)
			{
				PITEM_RULE const ptr_rule = rules_custom.at (i);

				if (ptr_rule)
				{
					pugi::xml_node item = root.append_child (L"item");

					if (item)
					{
						item.append_attribute (L"name").set_value (ptr_rule->pname);

						if (ptr_rule->prule_remote && ptr_rule->prule_remote[0])
							item.append_attribute (L"rule").set_value (ptr_rule->prule_remote);

						if (ptr_rule->prule_local && ptr_rule->prule_local[0])
							item.append_attribute (L"rule_local").set_value (ptr_rule->prule_local);

						if (ptr_rule->dir != FWP_DIRECTION_OUTBOUND)
							item.append_attribute (L"dir").set_value (ptr_rule->dir);

						if (ptr_rule->type != TypeUnknown)
							item.append_attribute (L"type").set_value (ptr_rule->type);

						if (ptr_rule->protocol != 0)
							item.append_attribute (L"protocol").set_value (ptr_rule->protocol);

						if (ptr_rule->af != AF_UNSPEC)
							item.append_attribute (L"version").set_value (ptr_rule->af);

						// add apps attribute
						if (!ptr_rule->apps.empty ())
						{
							rstring arr;
							bool is_haveapps = false;

							for (auto const &p : ptr_rule->apps)
							{
								PITEM_APP const ptr_app = _app_getapplication (p.first);

								if (ptr_app)
								{
									arr.Append (_r_path_unexpand (ptr_app->original_path));
									arr.Append (RULE_DELIMETER);

									if (!is_haveapps)
										is_haveapps = true;
								}
							}

							if (is_haveapps)
								item.append_attribute (L"apps").set_value (arr.Trim (RULE_DELIMETER));
						}

						item.append_attribute (L"is_block").set_value (ptr_rule->is_block);
						item.append_attribute (L"is_enabled").set_value (ptr_rule->is_enabled);
					}
				}
			}

			doc.save_file (path_rules ? path_rules : config.rules_custom_path, L"\t", PUGIXML_SAVE_FLAGS, PUGIXML_SAVE_ENCODING);

			// make backup
			if (is_backupcreated || (!path_rules && !rules_custom.empty () && is_backuprequired))
			{
				doc.save_file (config.rules_custom_path_backup, L"\t", PUGIXML_SAVE_FLAGS, PUGIXML_SAVE_ENCODING);
				is_backupcreated = true;
			}
		}
	}

	_r_fastlock_releaseshared (&lock_access);

	if (is_backupcreated)
		app.ConfigSet (L"BackupTimestamp", current_time);
}

bool _wfp_isfiltersinstalled ()
{
	if (config.is_providerset)
		return true;

	HKEY hkey = nullptr;
	bool result = false;

	if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\services\\BFE\\Parameters\\Policy\\Persistent\\Provider", 0, KEY_READ, &hkey) == ERROR_SUCCESS)
	{
		static const rstring guidString = _r_str_fromguid (GUID_WfpProvider);

		if (RegQueryValueEx (hkey, guidString, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
			result = true;

		RegCloseKey (hkey);
	}

	return result;
}

bool _wfp_destroy2filters (MARRAY* pmar, bool is_transact)
{
	if (!pmar || !config.hengine)
		return false;

	if (!is_transact)
	{
		_r_fastlock_acquireexclusive (&lock_apply);

		if (!_wfp_transact_start (__LINE__))
		{
			_r_fastlock_releaseexclusive (&lock_apply);

			SetEvent (config.done_evt);

			return false;
		}

		_r_fastlock_acquireexclusive (&lock_access);
	}

	for (size_t i = 0; i < pmar->size (); i++)
	{
		const UINT64 filter_id = pmar->at (i);

		if (filter_id)
			FwpmFilterDeleteById (config.hengine, filter_id);
	}

	if (!is_transact)
	{
		bool result = false;

		_r_fastlock_releaseexclusive (&lock_access);

		if (_wfp_transact_commit (__LINE__))
			result = true;

		_r_fastlock_releaseexclusive (&lock_apply);

		SetEvent (config.done_evt);

		return result;
	}

	pmar->clear ();

	return true;
}

bool _wfp_create4filters (PITEM_RULE ptr_rule, bool is_transact)
{
	if (!ptr_rule || !config.hengine)
		return false;

	if (!is_transact)
	{
		_r_fastlock_acquireexclusive (&lock_apply);

		if (!_wfp_transact_start (__LINE__))
		{
			_r_fastlock_releaseexclusive (&lock_apply);

			SetEvent (config.done_evt);

			return false;
		}

		_r_fastlock_acquireexclusive (&lock_access);
	}

	_wfp_destroy2filters (&ptr_rule->mfarr, true);

	if (ptr_rule->is_enabled)
	{
		rstring::rvector rule_remote_arr = rstring (ptr_rule->prule_remote).AsVector (RULE_DELIMETER);
		rstring::rvector rule_local_arr = rstring (ptr_rule->prule_local).AsVector (RULE_DELIMETER);

		const size_t rules_remote_length = rule_remote_arr.size ();
		const size_t rules_local_length = rule_local_arr.size ();
		const size_t count = max (1, max (rules_remote_length, rules_local_length));

		for (size_t i = 0; i < count; i++)
		{
			rstring rule_remote = L"";
			rstring rule_local = L"";

			// sync remote rules and local rules
			if (!rule_remote_arr.empty () && rules_remote_length > i)
				rule_remote = rule_remote_arr.at (i).Trim (L"\r\n ");

			// sync local rules and remote rules
			if (!rule_local_arr.empty () && rules_local_length > i)
				rule_local = rule_local_arr.at (i).Trim (L"\r\n ");

			if (!ptr_rule->apps.empty ())
			{
				for (auto const& p : ptr_rule->apps)
					ptr_rule->is_haveerrors = !_wfp_createrulefilter (ptr_rule->pname, _app_getapplication (p.first), rule_remote, rule_local, ptr_rule->protocol, ptr_rule->af, ptr_rule->dir, &ptr_rule->type, ptr_rule->weight, ptr_rule->is_block ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT, 0, &ptr_rule->mfarr);
			}
			else
			{
				ptr_rule->is_haveerrors = !_wfp_createrulefilter (ptr_rule->pname, nullptr, rule_remote, rule_local, ptr_rule->protocol, ptr_rule->af, ptr_rule->dir, &ptr_rule->type, ptr_rule->weight, ptr_rule->is_block ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT, 0, &ptr_rule->mfarr);
			}
		}
	}

	if (!is_transact)
	{
		bool result = false;

		_r_fastlock_releaseexclusive (&lock_access);

		if (_wfp_transact_commit (__LINE__))
			result = true;

		_r_fastlock_releaseexclusive (&lock_apply);

		SetEvent (config.done_evt);

		return result;
	}

	return true;
}

bool _wfp_create3filters (PITEM_APP ptr_app, bool is_transact)
{
	if (!ptr_app || !config.hengine)
		return false;

	const EnumMode mode = (EnumMode)app.ConfigGet (L"Mode", ModeWhitelist).AsUint ();
	const FWP_ACTION_TYPE action = (mode == ModeBlacklist) ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT;

	if (!is_transact)
	{
		_r_fastlock_acquireexclusive (&lock_apply);

		if (!_wfp_transact_start (__LINE__))
		{
			_r_fastlock_releaseexclusive (&lock_apply);

			SetEvent (config.done_evt);

			return false;
		}

		_r_fastlock_acquireexclusive (&lock_access);
	}

	_wfp_destroy2filters (&ptr_app->mfarr, true);

	if (ptr_app->is_enabled)
		ptr_app->is_haveerrors = !_wfp_createrulefilter (ptr_app->display_name, ptr_app, nullptr, nullptr, 0, AF_UNSPEC, FWP_DIRECTION_MAX, nullptr, FILTER_WEIGHT_APPLICATION, action, 0, &ptr_app->mfarr);

	if (!is_transact)
	{
		bool result = false;

		_r_fastlock_releaseexclusive (&lock_access);

		if (_wfp_transact_commit (__LINE__))
			result = true;

		_r_fastlock_releaseexclusive (&lock_apply);

		SetEvent (config.done_evt);

		return result;
	}

	return true;
}

bool _wfp_create2filters (bool is_transact)
{
	if (!config.hengine)
		return false;

	const EnumMode mode = (EnumMode)app.ConfigGet (L"Mode", ModeWhitelist).AsUint ();

	if (!is_transact)
	{
		_r_fastlock_acquireexclusive (&lock_apply);

		if (!_wfp_transact_start (__LINE__))
		{
			_r_fastlock_releaseexclusive (&lock_apply);

			SetEvent (config.done_evt);

			return false;
		}

		_r_fastlock_acquireexclusive (&lock_access);
	}

	_wfp_destroy2filters (&filters2, true);

	FWPM_FILTER_CONDITION fwfc[3] = {0};

	// add loopback connections permission
	if (app.ConfigGet (L"AllowLoopbackConnections", false).AsBool ())
	{
		// match all loopback (localhost) data
		fwfc[0].fieldKey = FWPM_CONDITION_FLAGS;
		fwfc[0].matchType = FWP_MATCH_FLAGS_ANY_SET;
		fwfc[0].conditionValue.type = FWP_UINT32;
		fwfc[0].conditionValue.uint32 = FWP_CONDITION_FLAG_IS_LOOPBACK;

		// tests if the network traffic is (non-)app container loopback traffic (win8+)
		if (_r_sys_validversion (6, 2))
			fwfc[0].conditionValue.uint32 |= FWP_CONDITION_FLAG_IS_APPCONTAINER_LOOPBACK;

		_wfp_createfilter (nullptr, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_CONNECT_V4, nullptr, FWP_ACTION_PERMIT, 0, &filters2);
		_wfp_createfilter (nullptr, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_CONNECT_V6, nullptr, FWP_ACTION_PERMIT, 0, &filters2);

		// win7+
		if (_r_sys_validversion (6, 1))
		{
			_wfp_createfilter (nullptr, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V4, nullptr, FWP_ACTION_PERMIT, 0, &filters2);
			_wfp_createfilter (nullptr, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V6, nullptr, FWP_ACTION_PERMIT, 0, &filters2);
		}

		_wfp_createfilter (nullptr, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, nullptr, FWP_ACTION_PERMIT, 0, &filters2);
		_wfp_createfilter (nullptr, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, FWP_ACTION_PERMIT, 0, &filters2);

		_wfp_createfilter (nullptr, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V4, nullptr, FWP_ACTION_PERMIT, 0, &filters2);
		_wfp_createfilter (nullptr, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V6, nullptr, FWP_ACTION_PERMIT, 0, &filters2);

		_wfp_createfilter (nullptr, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_LISTEN_V4, nullptr, FWP_ACTION_PERMIT, 0, &filters2);
		_wfp_createfilter (nullptr, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_LISTEN_V6, nullptr, FWP_ACTION_PERMIT, 0, &filters2);

		// ipv4/ipv6 loopback
		static LPCWSTR ip_list[] = {
			L"0.0.0.0/8",
			L"10.0.0.0/8",
			L"100.64.0.0/10",
			L"127.0.0.0/8",
			L"169.254.0.0/16",
			L"172.16.0.0/12",
			L"192.0.0.0/24",
			L"192.0.2.0/24",
			L"192.88.99.0/24",
			L"192.168.0.0/16",
			L"198.18.0.0/15",
			L"198.51.100.0/24",
			L"203.0.113.0/24",
			L"224.0.0.0/4",
			L"240.0.0.0/4",
			L"255.255.255.255/32",
			L"::/0",
			L"::/128",
			L"::1/128",
			L"::ffff:0:0/96",
			L"::ffff:0:0:0/96",
			L"64:ff9b::/96",
			L"100::/64",
			L"2001::/32",
			L"2001:20::/28",
			L"2001:db8::/32",
			L"2002::/16",
			L"fc00::/7",
			L"fe80::/10",
			L"ff00::/8"
		};

		for (size_t i = 0; i < _countof (ip_list); i++)
		{
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
					_wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_CONNECT_V4, nullptr, FWP_ACTION_PERMIT, 0, &filters2);

					// win7+
					if (_r_sys_validversion (6, 1))
						_wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V4, nullptr, FWP_ACTION_PERMIT, 0, &filters2);

					fwfc[1].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;
					_wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, nullptr, FWP_ACTION_PERMIT, 0, &filters2);
					_wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V4, nullptr, FWP_ACTION_PERMIT, 0, &filters2);
					_wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_LISTEN_V4, nullptr, FWP_ACTION_PERMIT, 0, &filters2);
				}
				else if (addr.format == NET_ADDRESS_IPV6)
				{
					fwfc[1].conditionValue.type = FWP_V6_ADDR_MASK;
					fwfc[1].conditionValue.v6AddrMask = &addr6;

					fwfc[1].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
					_wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_CONNECT_V6, nullptr, FWP_ACTION_PERMIT, 0, &filters2);

					// win7+
					if (_r_sys_validversion (6, 1))
						_wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V6, nullptr, FWP_ACTION_PERMIT, 0, &filters2);

					fwfc[1].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;
					_wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, FWP_ACTION_PERMIT, 0, &filters2);
					_wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V6, nullptr, FWP_ACTION_PERMIT, 0, &filters2);
					_wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_LISTEN_V6, nullptr, FWP_ACTION_PERMIT, 0, &filters2);
				}
			}
		}
	}

	// firewall service rules
	// https://msdn.microsoft.com/en-us/library/gg462153.aspx
	if (app.ConfigGet (L"AllowIPv6", true).AsBool ())
	{
		// allows 6to4 tunneling, which enables ipv6 to run over an ipv4 network
		fwfc[0].fieldKey = FWPM_CONDITION_IP_PROTOCOL;
		fwfc[0].matchType = FWP_MATCH_EQUAL;
		fwfc[0].conditionValue.type = FWP_UINT8;
		fwfc[0].conditionValue.uint8 = IPPROTO_IPV6; // ipv6 header

		_wfp_createfilter (L"Allow6to4", fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, nullptr, FWP_ACTION_PERMIT, 0, &filters2);

		// allows icmpv6 router solicitation messages, which are required for the ipv6 stack to work properly
		fwfc[0].fieldKey = FWPM_CONDITION_ICMP_TYPE;
		fwfc[0].matchType = FWP_MATCH_EQUAL;
		fwfc[0].conditionValue.type = FWP_UINT16;
		fwfc[0].conditionValue.uint16 = 0x85;

		_wfp_createfilter (L"AllowIcmpV6Type133", fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, FWP_ACTION_PERMIT, 0, &filters2);

		// allows icmpv6 router advertise messages, which are required for the ipv6 stack to work properly
		fwfc[0].conditionValue.uint16 = 0x86;
		_wfp_createfilter (L"AllowIcmpV6Type134", fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, FWP_ACTION_PERMIT, 0, &filters2);

		// allows icmpv6 neighbor solicitation messages, which are required for the ipv6 stack to work properly
		fwfc[0].conditionValue.uint16 = 0x87;
		_wfp_createfilter (L"AllowIcmpV6Type135", fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, FWP_ACTION_PERMIT, 0, &filters2);

		// allows icmpv6 neighbor advertise messages, which are required for the ipv6 stack to work properly
		fwfc[0].conditionValue.uint16 = 0x88;
		_wfp_createfilter (L"AllowIcmpV6Type136", fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, FWP_ACTION_PERMIT, 0, &filters2);
	}

	// prevent port scanning using stealth discards and silent drops
	if (app.ConfigGet (L"UseStealthMode", false).AsBool ())
	{
		// blocks udp port scanners
		fwfc[0].fieldKey = FWPM_CONDITION_FLAGS;
		fwfc[0].matchType = FWP_MATCH_FLAGS_NONE_SET;
		fwfc[0].conditionValue.type = FWP_UINT32;
		fwfc[0].conditionValue.uint32 = FWP_CONDITION_FLAG_IS_LOOPBACK;

		// tests if the network traffic is (non-)app container loopback traffic (win8+)
		if (_r_sys_validversion (6, 2))
			fwfc[0].conditionValue.uint32 |= FWP_CONDITION_FLAG_IS_APPCONTAINER_LOOPBACK;

		fwfc[1].fieldKey = FWPM_CONDITION_ICMP_TYPE;
		fwfc[1].matchType = FWP_MATCH_EQUAL;
		fwfc[1].conditionValue.type = FWP_UINT16;
		fwfc[1].conditionValue.uint16 = 0x03; // destination unreachable

		_wfp_createfilter (L"BlockIcmpErrorV4", fwfc, 2, FILTER_WEIGHT_HIGHEST, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V4, nullptr, FWP_ACTION_BLOCK, 0, &filters2);
		_wfp_createfilter (L"BlockIcmpErrorV6", fwfc, 2, FILTER_WEIGHT_HIGHEST, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V6, nullptr, FWP_ACTION_BLOCK, 0, &filters2);

		// blocks tcp port scanners (exclude loopback)
		fwfc[0].conditionValue.uint32 |= FWP_CONDITION_FLAG_IS_IPSEC_SECURED;

		_wfp_createfilter (L"BlockTcpRstOnCloseV4", fwfc, 1, FILTER_WEIGHT_HIGHEST, &FWPM_LAYER_INBOUND_TRANSPORT_V4_DISCARD, &FWPM_CALLOUT_WFP_TRANSPORT_LAYER_V4_SILENT_DROP, FWP_ACTION_CALLOUT_TERMINATING, 0, &filters2);
		_wfp_createfilter (L"BlockTcpRstOnCloseV6", fwfc, 1, FILTER_WEIGHT_HIGHEST, &FWPM_LAYER_INBOUND_TRANSPORT_V6_DISCARD, &FWPM_CALLOUT_WFP_TRANSPORT_LAYER_V6_SILENT_DROP, FWP_ACTION_CALLOUT_TERMINATING, 0, &filters2);
	}

	// block all outbound traffic (only on "whitelist" mode)
	if (mode == ModeWhitelist)
	{
		_wfp_createfilter (L"BlockOutboundConnectionsV4", nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_CONNECT_V4, nullptr, FWP_ACTION_BLOCK, 0, &filters2);
		_wfp_createfilter (L"BlockOutboundConnectionsV6", nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_CONNECT_V6, nullptr, FWP_ACTION_BLOCK, 0, &filters2);

		// win7+
		if (_r_sys_validversion (6, 1))
		{
			_wfp_createfilter (L"BlockOutboundRedirectionV4", nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V4, nullptr, FWP_ACTION_BLOCK, 0, &filters2);
			_wfp_createfilter (L"BlockOutboundRedirectionV6", nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V6, nullptr, FWP_ACTION_BLOCK, 0, &filters2);
		}
	}
	else
	{
		_wfp_createfilter (L"AllowOutboundConnectionsV4", nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_CONNECT_V4, nullptr, FWP_ACTION_PERMIT, 0, &filters2);
		_wfp_createfilter (L"AllowOutboundConnectionsV6", nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_CONNECT_V6, nullptr, FWP_ACTION_PERMIT, 0, &filters2);

		// win7+
		if (_r_sys_validversion (6, 1))
		{
			_wfp_createfilter (L"AllowOutboundRedirectionV4", nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V4, nullptr, FWP_ACTION_PERMIT, 0, &filters2);
			_wfp_createfilter (L"AllowOutboundRedirectionV6", nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V6, nullptr, FWP_ACTION_PERMIT, 0, &filters2);
		}
	}

	// block all inbound traffic (only on "stealth" mode)
	if (mode == ModeWhitelist && (app.ConfigGet (L"UseStealthMode", false).AsBool () || !app.ConfigGet (L"AllowInboundConnections", false).AsBool ()))
	{
		_wfp_createfilter (L"BlockInboundConnectionsV4", nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, nullptr, FWP_ACTION_BLOCK, 0, &filters2);
		_wfp_createfilter (L"BlockInboundConnectionsV6", nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, FWP_ACTION_BLOCK, 0, &filters2);
	}

	// block all listen traffic (NOT RECOMMENDED!!!!)
	// issue: https://github.com/henrypp/simplewall/issues/9
	if (mode == ModeWhitelist && !app.ConfigGet (L"AllowListenConnections2", true).AsBool ())
	{
		_wfp_createfilter (L"BlockListenConnectionsV4", nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_LISTEN_V4, nullptr, FWP_ACTION_BLOCK, 0, &filters2);
		_wfp_createfilter (L"BlockListenConnectionsV6", nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_LISTEN_V6, nullptr, FWP_ACTION_BLOCK, 0, &filters2);
	}

	// install boot-time filters (enforced at boot-time, even before "base filtering engine" service starts)
	if (app.ConfigGet (L"InstallBoottimeFilters", false).AsBool ())
	{
		{
			fwfc[0].fieldKey = FWPM_CONDITION_FLAGS;
			fwfc[0].matchType = FWP_MATCH_FLAGS_ANY_SET;
			fwfc[0].conditionValue.type = FWP_UINT32;
			fwfc[0].conditionValue.uint32 = FWP_CONDITION_FLAG_IS_LOOPBACK;

			// tests if the network traffic is (non-)app container loopback traffic (win8+)
			if (_r_sys_validversion (6, 2))
				fwfc[0].conditionValue.uint32 |= FWP_CONDITION_FLAG_IS_APPCONTAINER_LOOPBACK;

			_wfp_createfilter (BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_IPFORWARD_V4, nullptr, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, &filters2);
			_wfp_createfilter (BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_IPFORWARD_V6, nullptr, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, &filters2);

			_wfp_createfilter (BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, nullptr, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, &filters2);
			_wfp_createfilter (BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, &filters2);

			_wfp_createfilter (BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_INBOUND_ICMP_ERROR_V4, nullptr, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, &filters2);
			_wfp_createfilter (BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_INBOUND_ICMP_ERROR_V6, nullptr, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, &filters2);

			_wfp_createfilter (BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V4, nullptr, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, &filters2);
			_wfp_createfilter (BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V6, nullptr, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, &filters2);

			_wfp_createfilter (BOOTTIME_FILTER_NAME, nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, nullptr, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, &filters2);
			_wfp_createfilter (BOOTTIME_FILTER_NAME, nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, &filters2);

			_wfp_createfilter (BOOTTIME_FILTER_NAME, nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_INBOUND_ICMP_ERROR_V4, nullptr, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, &filters2);
			_wfp_createfilter (BOOTTIME_FILTER_NAME, nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_INBOUND_ICMP_ERROR_V6, nullptr, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, &filters2);

			_wfp_createfilter (BOOTTIME_FILTER_NAME, nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V4, nullptr, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, &filters2);
			_wfp_createfilter (BOOTTIME_FILTER_NAME, nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V6, nullptr, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, &filters2);
		}

		// win7+ boot-time features
		if (_r_sys_validversion (6, 1))
		{
			fwfc[0].fieldKey = FWPM_CONDITION_FLAGS;
			fwfc[0].matchType = FWP_MATCH_FLAGS_NONE_SET;
			fwfc[0].conditionValue.type = FWP_UINT32;
			fwfc[0].conditionValue.uint32 = FWP_CONDITION_FLAG_IS_OUTBOUND_PASS_THRU | FWP_CONDITION_FLAG_IS_INBOUND_PASS_THRU;

			_wfp_createfilter (BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_APPLICATION, &FWPM_LAYER_IPFORWARD_V4, nullptr, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, &filters2);
			_wfp_createfilter (BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_APPLICATION, &FWPM_LAYER_IPFORWARD_V6, nullptr, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, &filters2);
		}
	}

	if (!is_transact)
	{
		bool result = false;

		_r_fastlock_releaseexclusive (&lock_access);

		if (_wfp_transact_commit (__LINE__))
			result = true;

		_r_fastlock_releaseexclusive (&lock_apply);

		SetEvent (config.done_evt);

		return result;
	}

	return true;
}

void _wfp_installfilters ()
{
	_wfp_destroyfilters (false); // destroy all installed filters first

	_wfp_transact_start (__LINE__);

	// apply apps rules
	{
		for (auto& p : apps)
		{
			PITEM_APP ptr_app = &p.second;

			if (ptr_app->is_enabled)
				_wfp_create3filters (ptr_app, true);
		}
	}

	// apply system/custom/blocklist rules
	{
		std::vector<PITEM_RULE> const* ptr_rules[] = {
			&rules_system,
			&rules_custom,
			&rules_blocklist,
		};

		for (size_t i = 0; i < _countof (ptr_rules); i++)
		{
			if (!ptr_rules[i])
				continue;

			for (size_t j = 0; j < ptr_rules[i]->size (); j++)
			{
				PITEM_RULE ptr_rule = ptr_rules[i]->at (j);

				if (ptr_rule && ptr_rule->is_enabled)
					_wfp_create4filters (ptr_rule, true);
			}
		}
	}

	// apply internal rules
	_wfp_create2filters (true);

	_wfp_transact_commit (__LINE__);

	// set icons
	SendMessage (app.GetHWND (), WM_SETICON, ICON_SMALL, (LPARAM)app.GetSharedIcon (app.GetHINSTANCE (), IDI_ACTIVE, GetSystemMetrics (SM_CXSMICON)));
	SendMessage (app.GetHWND (), WM_SETICON, ICON_BIG, (LPARAM)app.GetSharedIcon (app.GetHINSTANCE (), IDI_ACTIVE, GetSystemMetrics (SM_CXICON)));

	SendDlgItemMessage (app.GetHWND (), IDC_START_BTN, BM_SETIMAGE, IMAGE_ICON, (WPARAM)app.GetSharedIcon (app.GetHINSTANCE (), IDI_INACTIVE, GetSystemMetrics (SM_CXSMICON)));

	app.TraySetInfo (app.GetHWND (), UID, nullptr, app.GetSharedIcon (app.GetHINSTANCE (), IDI_ACTIVE, GetSystemMetrics (SM_CXSMICON)), nullptr);

	SetDlgItemText (app.GetHWND (), IDC_START_BTN, app.LocaleString (IDS_TRAY_STOP, nullptr));

	// dropped packets logging (win7+)
	if (config.is_neteventset && _r_sys_validversion (6, 1))
		_wfp_logsubscribe ();
}

void _app_clear_threadpool (std::vector<HANDLE>* ptr_pool)
{
	if (!ptr_pool || ptr_pool->empty ())
		return;

	std::vector<size_t> remove_idx;

	for (size_t i = 0; i < ptr_pool->size (); i++)
	{
		const HANDLE hndl = ptr_pool->at (i);

		if (WaitForSingleObjectEx (hndl, 0, FALSE) == WAIT_OBJECT_0)
		{
			CloseHandle (hndl);
			remove_idx.push_back (i);
		}
	}

	if (remove_idx.empty ())
		return;

	for (size_t i = remove_idx.size (); i != 0; i--)
	{
		ptr_pool->erase (ptr_pool->begin () + remove_idx.at (i - 1));
	}
}

bool _app_changefilters (HWND hwnd, bool is_install, bool is_forced)
{
	if (_r_fastlock_islocked (&lock_apply))
		return false;

	_app_listviewsort (hwnd, IDC_LISTVIEW, -1, false);

	if (!is_install || ((is_install && is_forced) || _wfp_isfiltersinstalled ()))
	{
		_r_ctrl_enable (hwnd, IDC_START_BTN, false);

		_r_fastlock_acquireexclusive (&lock_threadpool);

		_app_clear_threadpool (&threads_pool);

		const HANDLE hthread = _r_createthread (&ApplyThread, (LPVOID)is_install);

		if (hthread)
			threads_pool.push_back (hthread);

		_r_fastlock_releaseexclusive (&lock_threadpool);

		return true;
	}

	_app_profilesave (hwnd);

	_r_listview_redraw (hwnd, IDC_LISTVIEW);

	return false;
}

void _app_clear_logstack ()
{
	while (true)
	{
		PSLIST_ENTRY ptr_list = InterlockedPopEntrySList (&log_stack.ListHead);

		if (!ptr_list)
			break;

		InterlockedDecrement (&log_stack.Count);

		PITEM_LIST_ENTRY ptr_entry = CONTAINING_RECORD (ptr_list, ITEM_LIST_ENTRY, ListEntry);
		PITEM_LOG ptr_log = (PITEM_LOG)ptr_entry->Body;

		if (ptr_log)
			delete ptr_log;

		_aligned_free (ptr_entry);
	}

	InterlockedFlushSList (&log_stack.ListHead);
}

void _app_logclear ()
{
	_app_clear_logstack ();

	const rstring path = _r_path_expand (app.ConfigGet (L"LogPath", LOG_PATH_DEFAULT));

	_r_fastlock_acquireexclusive (&lock_writelog);

	if (config.hlogfile != nullptr && config.hlogfile != INVALID_HANDLE_VALUE)
	{
		SetFilePointer (config.hlogfile, 2, nullptr, FILE_BEGIN);
		SetEndOfFile (config.hlogfile);
	}
	else
	{
		_r_fs_delete (path, false);
	}

	_r_fs_delete (_r_fmt (L"%s.bak", path.GetString ()), false);

	_r_fastlock_releaseexclusive (&lock_writelog);
}

bool _app_logchecklimit ()
{
	const DWORD limit = app.ConfigGet (L"LogSizeLimitKb", 256).AsUlong ();

	if (!limit || !config.hlogfile || config.hlogfile == INVALID_HANDLE_VALUE)
		return false;

	if (_r_fs_size (config.hlogfile) >= (limit * _R_BYTESIZE_KB))
	{
		_app_logclear ();

		return true;
	}

	return false;
}

bool _app_loginit (bool is_install)
{
	// dropped packets logging (win7+)
	if (!config.hnetevent || !_r_sys_validversion (6, 1))
		return false;

	// reset all handles
	_r_fastlock_acquireexclusive (&lock_writelog);

	if (config.hlogfile != nullptr && config.hlogfile != INVALID_HANDLE_VALUE)
	{
		CloseHandle (config.hlogfile);
		config.hlogfile = nullptr;
	}

	_r_fastlock_releaseexclusive (&lock_writelog);

	if (!is_install)
		return true; // already closed

	// check if log enabled
	if (!app.ConfigGet (L"IsLogEnabled", false).AsBool ())
		return false;

	bool result = false;

	const rstring path = _r_path_expand (app.ConfigGet (L"LogPath", LOG_PATH_DEFAULT));

	_r_fastlock_acquireexclusive (&lock_writelog);

	config.hlogfile = CreateFile (path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);

	if (config.hlogfile == INVALID_HANDLE_VALUE)
	{
		_app_logerror (L"CreateFile", GetLastError (), path, false);
	}
	else
	{
		if (GetLastError () != ERROR_ALREADY_EXISTS)
		{
			DWORD written = 0;
			static const BYTE bom[] = {0xFF, 0xFE};

			WriteFile (config.hlogfile, bom, sizeof (bom), &written, nullptr); // write utf-16 le byte order mask
		}
		else
		{
			_app_logchecklimit ();

			SetFilePointer (config.hlogfile, 0, nullptr, FILE_END);
		}

		result = true;
	}

	_r_fastlock_releaseexclusive (&lock_writelog);

	return result;
}

bool _app_formataddress (LPWSTR dest, size_t cchDest, PITEM_LOG const ptr_log, FWP_DIRECTION dir, UINT16 port, bool is_appenddns)
{
	if (!dest || !cchDest || !ptr_log)
		return false;

	bool result = false;

	PIN_ADDR addrv4 = nullptr;
	PIN6_ADDR addrv6 = nullptr;

	if (ptr_log->af == AF_INET)
	{
		addrv4 = (dir == FWP_DIRECTION_OUTBOUND) ? &ptr_log->remote_addr : &ptr_log->local_addr;

		InetNtop (ptr_log->af, addrv4, dest, cchDest);

		result = !IN4_IS_ADDR_UNSPECIFIED (addrv4);
	}
	else if (ptr_log->af == AF_INET6)
	{
		addrv6 = (dir == FWP_DIRECTION_OUTBOUND) ? &ptr_log->remote_addr6 : &ptr_log->local_addr6;

		InetNtop (ptr_log->af, addrv6, dest, cchDest);

		result = !IN6_IS_ADDR_UNSPECIFIED (addrv6);
	}

	if (port)
		StringCchCat (dest, cchDest, _r_fmt (L":%d", port));


	if (is_appenddns && result && app.ConfigGet (L"IsNetworkResolutionsEnabled", false).AsBool () && config.is_wsainit)
	{
		const size_t hash = _r_str_hash (dest);

		WCHAR hostBuff[NI_MAXHOST] = {0};

		if (cache_hosts.find (hash) != cache_hosts.end ())
		{
			if (cache_hosts[hash])
				StringCchCat (dest, cchDest, _r_fmt (L" (%s)", cache_hosts[hash]));
		}
		else
		{
			LPWSTR cache_ptr = nullptr;

			if (_app_resolveaddress (ptr_log->af, (ptr_log->af == AF_INET) ? (LPVOID)addrv4 : (LPVOID)addrv6, hostBuff, _countof (hostBuff)))
			{
				StringCchCat (dest, cchDest, _r_fmt (L" (%s)", hostBuff));

				cache_ptr = new WCHAR[wcslen (hostBuff) + 1];

				StringCchCopy (cache_ptr, wcslen (hostBuff) + 1, hostBuff);
			}

			cache_hosts[hash] = cache_ptr;
		}
	}

	return result;
}

rstring _app_getprotoname (UINT8 proto)
{
	for (size_t i = 0; i < protocols.size (); i++)
	{
		PITEM_PROTOCOL const ptr_proto = &protocols.at (i);

		if (proto == ptr_proto->id)
			return ptr_proto->pname;
	}

	return SZ_EMPTY;
}

void _app_logwrite (PITEM_LOG const ptr_log)
{
	if (!ptr_log)
		return;

	if (config.hlogfile == nullptr || config.hlogfile == INVALID_HANDLE_VALUE)
		return;

	// parse path
	rstring path;
	{
		_r_fastlock_acquireshared (&lock_access);

		PITEM_APP const ptr_app = _app_getapplication (ptr_log->hash);

		if (ptr_app)
		{
			if (ptr_app->type == AppStore || ptr_app->type == AppService)
			{
				if (ptr_app->real_path[0])
					path = ptr_app->real_path;

				else
					path = ptr_app->display_name;
			}
			else
			{
				path = ptr_app->original_path;
			}
		}

		_r_fastlock_releaseshared (&lock_access);

		if (path.IsEmpty ())
			path = SZ_EMPTY;
	}

	// parse filter name
	rstring filter;
	{
		if (ptr_log->provider_name[0])
			filter.Format (L"%s\\%s", ptr_log->provider_name, ptr_log->filter_name);

		else
			filter = ptr_log->filter_name[0] ? ptr_log->filter_name : SZ_EMPTY;
	}

	// parse direction
	rstring direction;
	{
		if (ptr_log->direction == FWP_DIRECTION_INBOUND)
			direction = SZ_LOG_DIRECTION_IN;

		else if (ptr_log->direction == FWP_DIRECTION_OUTBOUND)
			direction = SZ_LOG_DIRECTION_OUT;

		else
			direction = SZ_EMPTY;

		if (ptr_log->is_loopback)
			direction.Append (SZ_LOG_DIRECTION_LOOPBACK);
	}

	rstring buffer;
	buffer.Format (L"%s" LOG_DIV L"%s" LOG_DIV L"%s" LOG_DIV L"%s (" SZ_LOG_REMOTE_ADDRESS L")" LOG_DIV L"%s (" SZ_LOG_LOCAL_ADDRESS L")" LOG_DIV L"%s" LOG_DIV L"%s" LOG_DIV L"#%" PRIu64 LOG_DIV L"%s" LOG_DIV L"%s\r\n",
		_r_fmt_date (ptr_log->date, FDTF_SHORTDATE | FDTF_LONGTIME).GetString (),
		ptr_log->username,
		path.GetString (),
		ptr_log->remote_fmt,
		ptr_log->local_fmt,
		_app_getprotoname (ptr_log->protocol).GetString (),
		filter.GetString (),
		ptr_log->filter_id,
		direction.GetString (),
		(ptr_log->is_allow ? SZ_LOG_ALLOW : SZ_LOG_BLOCK)
	);

	_r_fastlock_acquireexclusive (&lock_writelog);

	_app_logchecklimit ();

	DWORD written = 0;
	WriteFile (config.hlogfile, buffer.GetString (), DWORD (buffer.GetLength () * sizeof (WCHAR)), &written, nullptr);

	_r_fastlock_releaseexclusive (&lock_writelog);
}

void CALLBACK _app_timer_callback (PVOID lparam, BOOLEAN)
{
	_r_fastlock_acquireexclusive (&lock_apply);
	_r_fastlock_acquireexclusive (&lock_access);

	const size_t hash = (size_t)lparam;
	const PITEM_APP ptr_app = _app_getapplication (hash);
	const HWND hwnd = app.GetHWND ();
	const bool is_succcess = _app_timer_remove (hwnd, hash, true);

	_r_fastlock_releaseexclusive (&lock_apply);
	_r_fastlock_releaseexclusive (&lock_access);

	if (is_succcess)
	{
		_app_listviewsort (hwnd, IDC_LISTVIEW, -1, false);
		_app_profilesave (hwnd);

		_r_listview_redraw (hwnd, IDC_LISTVIEW);

		if (app.ConfigGet (L"IsNotificationsTimer", true).AsBool ())
			app.TrayPopup (hwnd, UID, nullptr, NIIF_USER | (app.ConfigGet (L"IsNotificationsSound", true).AsBool () ? 0 : NIIF_NOSOUND), APP_NAME, _r_fmt (app.LocaleString (IDS_STATUS_TIMER_DONE, nullptr), ptr_app->display_name));
	}
}

bool _app_timer_create (HWND hwnd, size_t hash, time_t seconds, bool is_transact)
{
	if (!config.hengine)
		return false;

	const time_t current_time = _r_unixtime_now ();

	PITEM_APP ptr_app = _app_getapplication (hash);

	if (ptr_app)
	{
		if (ptr_app->htimer)
		{
			DeleteTimerQueueTimer (config.htimer, ptr_app->htimer, nullptr);
			ptr_app->htimer = nullptr;
		}

		if (CreateTimerQueueTimer (&ptr_app->htimer, config.htimer, &_app_timer_callback, (PVOID)hash, DWORD (seconds * _R_SECONDSCLOCK_MSEC), 0, WT_EXECUTEONLYONCE | WT_EXECUTEINTIMERTHREAD))
		{
			ptr_app->is_enabled = true;
			ptr_app->timer = current_time + seconds;

			_wfp_create3filters (ptr_app, is_transact);

			const size_t item = _app_getposition (hwnd, hash);

			if (item != LAST_VALUE)
			{
				config.is_nocheckboxnotify = true;

				_r_listview_setitem (hwnd, IDC_LISTVIEW, item, 0, nullptr, LAST_VALUE, _app_getappgroup (hash, ptr_app));
				_r_listview_setitemcheck (hwnd, IDC_LISTVIEW, item, ptr_app->is_enabled);

				config.is_nocheckboxnotify = false;
			}

			return true;
		}
	}

	return false;
}

bool _app_timer_remove (HWND hwnd, size_t hash, bool is_transact)
{
	if (!config.hengine)
		return false;

	const time_t current_time = _r_unixtime_now ();

	std::vector<size_t> remove_idx;

	PITEM_APP ptr_app = _app_getapplication (hash);

	if (ptr_app)
	{
		if (ptr_app->htimer)
		{
			DeleteTimerQueueTimer (config.htimer, ptr_app->htimer, nullptr);
			ptr_app->htimer = nullptr;
			ptr_app->timer = 0;

			ptr_app->is_enabled = false;
			_wfp_destroy2filters (&ptr_app->mfarr, is_transact);

			const size_t item = _app_getposition (hwnd, hash);

			if (item != LAST_VALUE)
			{
				if (!_app_isexists (ptr_app) || ptr_app->is_temp)
				{
					SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_DELETEITEM, item, 0);
					_app_freeapplication (hash);
				}
				else
				{
					config.is_nocheckboxnotify = true;

					_r_listview_setitem (hwnd, IDC_LISTVIEW, item, 0, nullptr, LAST_VALUE, _app_getappgroup (hash, ptr_app));
					_r_listview_setitemcheck (hwnd, IDC_LISTVIEW, item, ptr_app->is_enabled);

					config.is_nocheckboxnotify = false;
				}
			}

			return true;
		}
	}

	return false;
}

void _app_notifysetpos (HWND hwnd)
{
	RECT windowRect = {0};
	GetWindowRect (hwnd, &windowRect);

	RECT desktopRect = {0};
	SystemParametersInfo (SPI_GETWORKAREA, 0, &desktopRect, 0);

	APPBARDATA appbar = {0};
	appbar.cbSize = sizeof (appbar);
	appbar.hWnd = FindWindow (L"Shell_TrayWnd", nullptr);

	SHAppBarMessage (ABM_GETTASKBARPOS, &appbar);

	const UINT border_x = GetSystemMetrics (SM_CXBORDER) * 2;
	const UINT border_y = GetSystemMetrics (SM_CYBORDER) * 2;

	if (appbar.uEdge == ABE_LEFT)
	{
		windowRect.left = appbar.rc.right + border_x;
		windowRect.top = (desktopRect.bottom - (windowRect.bottom - windowRect.top)) - border_y;
	}
	else if (appbar.uEdge == ABE_TOP)
	{
		windowRect.left = (desktopRect.right - (windowRect.right - windowRect.left)) - border_x;
		windowRect.top = appbar.rc.bottom + border_y;
	}
	else if (appbar.uEdge == ABE_RIGHT)
	{
		windowRect.left = (desktopRect.right - (windowRect.right - windowRect.left)) - border_x;
		windowRect.top = (desktopRect.bottom - (windowRect.bottom - windowRect.top)) - border_y;
	}
	else/* if (appbar.uEdge == ABE_BOTTOM)*/
	{
		windowRect.left = (desktopRect.right - (windowRect.right - windowRect.left)) - border_x;
		windowRect.top = (desktopRect.bottom - (windowRect.bottom - windowRect.top)) - border_y;
	}

	SetWindowPos (hwnd, nullptr, windowRect.left, windowRect.top, 0, 0, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_NOCOPYBITS);
}

void _app_notifyhide (HWND hwnd)
{
	_app_notifysettimeout (hwnd, 0, false, 0);

	ShowWindow (hwnd, SW_HIDE);
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

	if (RegisterClassEx (&wcex))
	{
		const UINT wnd_width = app.GetDPI (NOTIFY_WIDTH);
		const UINT wnd_height = app.GetDPI (NOTIFY_HEIGHT);
		const UINT btn_split_width = app.GetDPI (NOTIFY_BTN_WIDTH + 10);
		const UINT btn_width = app.GetDPI (NOTIFY_BTN_WIDTH);

		const INT cxsmIcon = GetSystemMetrics (SM_CXSMICON);
		const INT IconSize = cxsmIcon + (cxsmIcon / 2);
		const INT IconXXXX = app.GetDPI (20);

		config.hnotification = CreateWindowEx (WS_EX_TOPMOST, NOTIFY_CLASS_DLG, nullptr, WS_POPUP, 0, 0, wnd_width, wnd_height, nullptr, nullptr, wcex.hInstance, nullptr);

		if (config.hnotification)
		{
			HFONT hfont_title = nullptr;
			HFONT hfont_text = nullptr;

			{
				NONCLIENTMETRICS ncm = {0};
				ncm.cbSize = sizeof (ncm);

				if (SystemParametersInfo (SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0))
				{
					PLOGFONT lf_title = &ncm.lfCaptionFont;
					PLOGFONT lf_text = &ncm.lfMessageFont;

					lf_title->lfHeight = _r_dc_fontsizetoheight (11);
					lf_text->lfHeight = _r_dc_fontsizetoheight (9);

					lf_title->lfWeight = FW_SEMIBOLD;
					lf_text->lfWeight = ncm.lfMessageFont.lfWeight;

					lf_title->lfQuality = ncm.lfCaptionFont.lfQuality;
					lf_text->lfQuality = ncm.lfMessageFont.lfQuality;

					lf_title->lfCharSet = ncm.lfCaptionFont.lfCharSet;
					lf_text->lfCharSet = ncm.lfMessageFont.lfCharSet;

					StringCchCopy (lf_title->lfFaceName, LF_FACESIZE, UI_FONT);
					StringCchCopy (lf_text->lfFaceName, LF_FACESIZE, UI_FONT);

					hfont_title = CreateFontIndirect (lf_title);
					hfont_text = CreateFontIndirect (lf_text);
				}
			}

			HWND hctrl = CreateWindow (WC_STATIC, APP_NAME, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_WORDELLIPSIS, IconSize + app.GetDPI (10), app.GetDPI (8), wnd_width - app.GetDPI (64 + 12 + 10 + 24), IconSize, config.hnotification, (HMENU)IDC_TITLE_ID, nullptr, nullptr);
			SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_title, true);

			hctrl = CreateWindow (WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_CENTER | SS_ICON, app.GetDPI (8), app.GetDPI (8), IconSize, IconSize, config.hnotification, (HMENU)IDC_ICON_ID, nullptr, nullptr);
			SendMessage (hctrl, STM_SETIMAGE, IMAGE_ICON, (WPARAM)app.GetSharedIcon (app.GetHINSTANCE (), IDI_MAIN, cxsmIcon));

			hctrl = CreateWindow (WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_CENTER | SS_ICON | SS_NOTIFY, wnd_width - app.GetDPI (48) - app.GetDPI (18), app.GetDPI (8), IconSize, IconSize, config.hnotification, (HMENU)IDC_MUTE_BTN, nullptr, nullptr);
			SendMessage (hctrl, STM_SETIMAGE, IMAGE_ICON, (WPARAM)app.GetSharedIcon (app.GetHINSTANCE (), IDI_MUTE, IconXXXX));

			hctrl = CreateWindow (WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_CENTER | SS_ICON | SS_NOTIFY, wnd_width - app.GetDPI (20) - app.GetDPI (12), app.GetDPI (8), IconSize, IconSize, config.hnotification, (HMENU)IDC_CLOSE_BTN, nullptr, nullptr);
			SendMessage (hctrl, STM_SETIMAGE, IMAGE_ICON, (WPARAM)app.GetSharedIcon (app.GetHINSTANCE (), IDI_CLOSE, IconXXXX));

			hctrl = CreateWindow (WC_LINK, nullptr, WS_CHILD | WS_VISIBLE, app.GetDPI (12), app.GetDPI (38), wnd_width - app.GetDPI (24), app.GetDPI (16), config.hnotification, (HMENU)IDC_FILE_ID, nullptr, nullptr);
			SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_text, true);

			hctrl = CreateWindow (WC_EDIT, nullptr, WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL | ES_MULTILINE, app.GetDPI (12), app.GetDPI (56), wnd_width - app.GetDPI (24), app.GetDPI (16), config.hnotification, (HMENU)IDC_ADDRESS_REMOTE_ID, nullptr, nullptr);
			SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_text, true);
			SendMessage (hctrl, EM_SETMARGINS, EC_LEFTMARGIN, 0);
			SendMessage (hctrl, EM_SETMARGINS, EC_RIGHTMARGIN, 0);

			hctrl = CreateWindow (WC_EDIT, nullptr, WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL | ES_MULTILINE, app.GetDPI (12), app.GetDPI (74), wnd_width - app.GetDPI (24), app.GetDPI (16), config.hnotification, (HMENU)IDC_ADDRESS_LOCAL_ID, nullptr, nullptr);
			SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_text, true);
			SendMessage (hctrl, EM_SETMARGINS, EC_LEFTMARGIN, 0);
			SendMessage (hctrl, EM_SETMARGINS, EC_RIGHTMARGIN, 0);

			hctrl = CreateWindow (WC_EDIT, nullptr, WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL | ES_MULTILINE, app.GetDPI (12), app.GetDPI (92), wnd_width - app.GetDPI (24), app.GetDPI (16), config.hnotification, (HMENU)IDC_FILTER_ID, nullptr, nullptr);
			SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_text, true);
			SendMessage (hctrl, EM_SETMARGINS, EC_LEFTMARGIN, 0);
			SendMessage (hctrl, EM_SETMARGINS, EC_RIGHTMARGIN, 0);

			hctrl = CreateWindow (WC_EDIT, nullptr, WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL | ES_MULTILINE, app.GetDPI (12), app.GetDPI (110), wnd_width - app.GetDPI (24), app.GetDPI (16), config.hnotification, (HMENU)IDC_DATE_ID, nullptr, nullptr);
			SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_text, true);
			SendMessage (hctrl, EM_SETMARGINS, EC_LEFTMARGIN, 0);
			SendMessage (hctrl, EM_SETMARGINS, EC_RIGHTMARGIN, 0);

			hctrl = CreateWindow (WC_BUTTON, nullptr, WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_CHECKBOX | BS_NOTIFY, app.GetDPI (12), app.GetDPI (138), wnd_width - app.GetDPI (24), app.GetDPI (16), config.hnotification, (HMENU)IDC_CREATERULE_ADDR_ID, nullptr, nullptr);
			SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_text, true);

			hctrl = CreateWindow (WC_BUTTON, nullptr, WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_CHECKBOX | BS_NOTIFY, app.GetDPI (12), app.GetDPI (156), wnd_width - app.GetDPI (24), app.GetDPI (16), config.hnotification, (HMENU)IDC_CREATERULE_PORT_ID, nullptr, nullptr);
			SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_text, true);

			hctrl = CreateWindow (WC_BUTTON, nullptr, WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_CHECKBOX | BS_NOTIFY, app.GetDPI (12), app.GetDPI (174), wnd_width - app.GetDPI (24), app.GetDPI (16), config.hnotification, (HMENU)IDC_DISABLENOTIFICATION_ID, nullptr, nullptr);
			SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_text, true);

			hctrl = CreateWindowEx (app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WC_BUTTON, L"<", WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, app.GetDPI (10), wnd_height - app.GetDPI (36), app.GetDPI (24), app.GetDPI (24), config.hnotification, (HMENU)IDC_PREV_ID, nullptr, nullptr);
			SendMessage (hctrl, WM_SETFONT, (LPARAM)hfont_text, true);

			hctrl = CreateWindowEx (app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WC_BUTTON, L">", WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, app.GetDPI (10 + 24 + 2), wnd_height - app.GetDPI (36), app.GetDPI (24), app.GetDPI (24), config.hnotification, (HMENU)IDC_NEXT_ID, nullptr, nullptr);
			SendMessage (hctrl, WM_SETFONT, (LPARAM)hfont_text, true);

			hctrl = CreateWindowEx (0, WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS | SS_CENTERIMAGE, app.GetDPI (10 + 24 + 2 + 24 + 6), wnd_height - app.GetDPI (36), app.GetDPI (44), app.GetDPI (24), config.hnotification, (HMENU)IDC_IDX_ID, nullptr, nullptr);
			SendMessage (hctrl, WM_SETFONT, (LPARAM)hfont_text, true);

			hctrl = CreateWindow (WC_BUTTON, nullptr, WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_SPLITBUTTON, wnd_width - ((btn_width + btn_split_width + app.GetDPI (8 + 8))), wnd_height - app.GetDPI (36), btn_split_width, app.GetDPI (24), config.hnotification, (HMENU)IDC_ALLOW_BTN, nullptr, nullptr);
			SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_text, true);
			SendMessage (hctrl, BM_SETIMAGE, IMAGE_ICON, (WPARAM)app.GetSharedIcon (app.GetHINSTANCE (), IDI_ALLOW, cxsmIcon));

			hctrl = CreateWindow (WC_BUTTON, nullptr, WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, wnd_width - btn_width - app.GetDPI (10), wnd_height - app.GetDPI (36), btn_width, app.GetDPI (24), config.hnotification, (HMENU)IDC_BLOCK_BTN, nullptr, nullptr);
			SendMessage (hctrl, WM_SETFONT, (WPARAM)hfont_text, true);
			SendMessage (hctrl, BM_SETIMAGE, IMAGE_ICON, (WPARAM)app.GetSharedIcon (app.GetHINSTANCE (), IDI_BLOCK, cxsmIcon));

			_app_setbuttonmargins (config.hnotification, IDC_ALLOW_BTN);
			_app_setbuttonmargins (config.hnotification, IDC_BLOCK_BTN);

			_r_ctrl_settip (config.hnotification, IDC_MUTE_BTN, LPSTR_TEXTCALLBACK);
			_r_ctrl_settip (config.hnotification, IDC_CLOSE_BTN, LPSTR_TEXTCALLBACK);

			_r_ctrl_settip (config.hnotification, IDC_FILE_ID, LPSTR_TEXTCALLBACK);
			_r_ctrl_settip (config.hnotification, IDC_ADDRESS_LOCAL_ID, LPSTR_TEXTCALLBACK);
			_r_ctrl_settip (config.hnotification, IDC_ADDRESS_REMOTE_ID, LPSTR_TEXTCALLBACK);
			_r_ctrl_settip (config.hnotification, IDC_FILTER_ID, LPSTR_TEXTCALLBACK);
			_r_ctrl_settip (config.hnotification, IDC_DATE_ID, LPSTR_TEXTCALLBACK);

			_r_ctrl_settip (config.hnotification, IDC_CREATERULE_ADDR_ID, LPSTR_TEXTCALLBACK);
			_r_ctrl_settip (config.hnotification, IDC_CREATERULE_PORT_ID, LPSTR_TEXTCALLBACK);
			_r_ctrl_settip (config.hnotification, IDC_DISABLENOTIFICATION_ID, LPSTR_TEXTCALLBACK);

			_app_notifyhide (config.hnotification);
		}
	}
}

size_t _app_notifygetcurrent ()
{
	size_t new_idx = LAST_VALUE;
	const size_t count = notifications.size ();

	if (count)
	{
		if (count == 1)
		{
			new_idx = 0;
		}
		else
		{
			const size_t idx = (size_t)GetWindowLongPtr (config.hnotification, GWLP_USERDATA);
			new_idx = max (0, min (idx, count - 1));
		}
	}

	SetWindowLongPtr (config.hnotification, GWLP_USERDATA, new_idx);

	return new_idx;
}

bool _app_notifycommand (HWND hwnd, EnumNotifyCommand command, size_t timer_idx)
{
	_r_fastlock_acquireexclusive (&lock_notification);

	const size_t idx = _app_notifygetcurrent ();

	if (idx != LAST_VALUE)
	{
		PITEM_LOG ptr_log = notifications.at (idx);

		if (ptr_log)
		{
			const size_t hash = ptr_log->hash;
			const size_t item = _app_getposition (app.GetHWND (), hash);

			const bool is_createaddrrule = (IsDlgButtonChecked (hwnd, IDC_CREATERULE_ADDR_ID) == BST_CHECKED) && IsWindowEnabled (GetDlgItem (hwnd, IDC_CREATERULE_ADDR_ID));
			const bool is_createportrule = (IsDlgButtonChecked (hwnd, IDC_CREATERULE_PORT_ID) == BST_CHECKED) && IsWindowEnabled (GetDlgItem (hwnd, IDC_CREATERULE_PORT_ID));
			const bool is_disablenotification = (IsDlgButtonChecked (hwnd, IDC_DISABLENOTIFICATION_ID) == BST_CHECKED) && IsWindowEnabled (GetDlgItem (hwnd, IDC_DISABLENOTIFICATION_ID));

			_r_fastlock_acquireexclusive (&lock_access);

			PITEM_APP ptr_app = _app_getapplication (hash);

			if (ptr_app)
			{
				_r_fastlock_acquireexclusive (&lock_apply);

				_wfp_transact_start (__LINE__);

				if (command == CmdAllow || command == CmdBlock)
				{
					// just create rule
					if (is_createaddrrule || is_createportrule)
					{
						WCHAR rule[128] = {0};

						if (is_createaddrrule)
							_app_formataddress (rule, _countof (rule), ptr_log, FWP_DIRECTION_OUTBOUND, 0, false);

						if (is_createportrule)
						{
							if (is_createaddrrule)
								StringCchCat (rule, _countof (rule), L":");

							StringCchCat (rule, _countof (rule), _r_fmt (L"%d", ptr_log->remote_port));
						}

						size_t rule_id = LAST_VALUE;

						const size_t length = wcslen (rule);
						const size_t rule_length = min (length, RULE_RULE_CCH_MAX) + 1;

						for (size_t i = 0; i < rules_custom.size (); i++)
						{
							PITEM_RULE rule_ptr = rules_custom.at (i);

							if (rule_ptr)
							{
								if (rule_ptr->prule_remote && _wcsnicmp (rule_ptr->prule_remote, rule, rule_length) == 0 && ((!rule_ptr->is_block && command == CmdAllow) || (rule_ptr->is_block && command == CmdBlock)))
								{
									rule_id = i;
									break;
								}
							}
						}

						if (rule_id == LAST_VALUE)
						{
							// create rule
							PITEM_RULE ptr_rule = new ITEM_RULE;

							if (ptr_rule)
							{
								const size_t name_length = min (length, (size_t)RULE_NAME_CCH_MAX) + 1;

								str_set (&ptr_rule->pname, name_length, rule);
								str_set (&ptr_rule->prule_remote, name_length, rule);

								ptr_rule->is_enabled = true;
								ptr_rule->is_block = ((command == CmdBlock) ? true : false);
								ptr_rule->dir = ptr_log->direction;
								ptr_rule->weight = (ptr_rule->is_block ? FILTER_WEIGHT_CUSTOM_BLOCK : FILTER_WEIGHT_CUSTOM);

								rules_custom.push_back (ptr_rule);
								rule_id = rules_custom.size () - 1;
							}
						}
						else
						{
							// modify rule
							PITEM_RULE ptr_rule = rules_custom.at (rule_id);

							if (ptr_rule)
							{
								ptr_rule->is_enabled = true;
								ptr_rule->is_block = ((command == CmdBlock) ? true : false);
								ptr_rule->weight = (ptr_rule->is_block ? FILTER_WEIGHT_CUSTOM_BLOCK : FILTER_WEIGHT_CUSTOM);
							}
						}

						// add rule for app
						if (rule_id != LAST_VALUE)
						{
							rules_custom.at (rule_id)->apps[hash] = true;

							_wfp_create4filters (rules_custom.at (rule_id), true);
						}
					}
					else
					{
						if (command == CmdAllow)
							ptr_app->is_enabled = true;

						config.is_nocheckboxnotify = true;

						_r_listview_setitemcheck (app.GetHWND (), IDC_LISTVIEW, item, ptr_app->is_enabled);

						config.is_nocheckboxnotify = false;
					}

					// create rule timer
					if (timer_idx != LAST_VALUE)
					{
						_app_timer_create (app.GetHWND (), hash, timers.at (timer_idx), true);
					}
					else
					{
						_wfp_create3filters (ptr_app, true);
					}

					ptr_app->is_silent = is_disablenotification;
				}
				else if (command == CmdMute)
				{
					ptr_app->is_silent = true;
				}

				_wfp_transact_commit (__LINE__);

				ptr_app->last_notify = _r_unixtime_now ();

				_r_fastlock_releaseexclusive (&lock_apply);
				_r_fastlock_releaseexclusive (&lock_access);

				_app_freenotify (hash, false);

				_r_fastlock_releaseexclusive (&lock_notification);

				_app_notifyrefresh ();

				_app_listviewsort (app.GetHWND (), IDC_LISTVIEW, -1, false);

				_app_profilesave (app.GetHWND ());

				_r_listview_redraw (app.GetHWND (), IDC_LISTVIEW);

				return true;
			}

			_r_fastlock_releaseexclusive (&lock_access);
		}
	}

	_r_fastlock_releaseexclusive (&lock_notification);

	return false;
}

bool _app_notifysettimeout (HWND hwnd, UINT_PTR timer_id, bool is_create, UINT timeout)
{
	if (is_create)
	{
		if (!hwnd || !timer_id)
			return false;

		if (timer_id == NOTIFY_TIMER_TIMEOUT_ID)
			config.is_notifytimeout = true;

		SetTimer (hwnd, timer_id, timeout, nullptr);
	}
	else
	{
		if (!timer_id || timer_id == NOTIFY_TIMER_TIMEOUT_ID)
			config.is_notifytimeout = false;

		if (timer_id)
		{
			KillTimer (hwnd, timer_id);
		}
		else
		{
			KillTimer (hwnd, NOTIFY_TIMER_POPUP_ID);
			KillTimer (hwnd, NOTIFY_TIMER_TIMEOUT_ID);
		}
	}

	return true;
}

bool _app_notifyshow (size_t idx, bool is_forced)
{
	_r_fastlock_acquireshared (&lock_notification);

	if (!app.ConfigGet (L"IsNotificationsEnabled", true).AsBool () || notifications.empty () || idx == LAST_VALUE)
	{
		SetWindowLongPtr (config.hnotification, GWLP_USERDATA, LAST_VALUE);

		_r_fastlock_releaseshared (&lock_notification);

		return false;
	}

	// prevent fullscreen apps lose focus
	if (is_forced && _r_wnd_isfullscreenmode ())
		is_forced = false;

	const size_t total_size = notifications.size ();

	idx = (total_size == 1) ? 0 : max (0, min (idx, total_size - 1));

	PITEM_LOG const ptr_log = notifications.at (idx);

	if (ptr_log)
	{
		_r_fastlock_acquireshared (&lock_access);

		PITEM_APP const ptr_app = _app_getapplication (ptr_log->hash);

		if (ptr_app)
		{
			SetWindowLongPtr (config.hnotification, GWLP_USERDATA, idx);

			rstring name;
			rstring is_signed;

			if (app.ConfigGet (L"ShowFilenames", true).AsBool ())
				name = _r_path_extractfile (ptr_app->display_name);

			else
				name = _r_path_compact (ptr_app->display_name, 48);

			if (app.ConfigGet (L"IsCerificatesEnabled", false).AsBool ())
				is_signed.Format (L" [%s]", app.LocaleString (ptr_app->is_signed ? IDS_SIGN_SIGNED : IDS_SIGN_UNSIGNED, nullptr).GetString ());

			_r_ctrl_settext (config.hnotification, IDC_TITLE_ID, APP_NAME);

			_r_ctrl_settext (config.hnotification, IDC_FILE_ID, L"%s: <a href=\"#\">%s</a>%s [%s]", app.LocaleString (IDS_FILE, nullptr).GetString (), name.GetString (), is_signed.GetString (), _app_getprotoname (ptr_log->protocol).GetString ());

			_r_ctrl_settext (config.hnotification, IDC_ADDRESS_REMOTE_ID, L"%s: %s", app.LocaleString (IDS_ADDRESS_REMOTE, nullptr).GetString (), ptr_log->remote_fmt);
			_r_ctrl_settext (config.hnotification, IDC_ADDRESS_LOCAL_ID, L"%s: %s", app.LocaleString (IDS_ADDRESS_LOCAL, nullptr).GetString (), ptr_log->local_fmt);

			_r_ctrl_settext (config.hnotification, IDC_FILTER_ID, L"%s: %s (#%llu)", app.LocaleString (IDS_FILTER, nullptr).GetString (), ptr_log->filter_name, ptr_log->filter_id);
			_r_ctrl_settext (config.hnotification, IDC_DATE_ID, L"%s: %s", app.LocaleString (IDS_DATE, nullptr).GetString (), _r_fmt_date (ptr_log->date, FDTF_SHORTDATE | FDTF_LONGTIME).GetString ());

			WCHAR addr_format[LEN_IP_MAX] = {0};
			const bool is_addressset = _app_formataddress (addr_format, _countof (addr_format), ptr_log, FWP_DIRECTION_OUTBOUND, 0, false);

			_r_ctrl_settext (config.hnotification, IDC_CREATERULE_ADDR_ID, app.LocaleString (IDS_NOTIFY_CREATERULE_ADDRESS, nullptr), addr_format);
			_r_ctrl_settext (config.hnotification, IDC_CREATERULE_PORT_ID, app.LocaleString (IDS_NOTIFY_CREATERULE_PORT, nullptr), ptr_log->remote_port);
			_r_ctrl_settext (config.hnotification, IDC_DISABLENOTIFICATION_ID, app.LocaleString (IDS_NOTIFY_DISABLENOTIFICATIONS, nullptr));

			_r_ctrl_enable (config.hnotification, IDC_CREATERULE_ADDR_ID, is_addressset);
			_r_ctrl_enable (config.hnotification, IDC_CREATERULE_PORT_ID, ptr_log->remote_port != 0);

			_r_ctrl_settext (config.hnotification, IDC_IDX_ID, L"%zu/%zu", idx + 1, total_size);

			_r_ctrl_settext (config.hnotification, IDC_ALLOW_BTN, app.LocaleString (IDS_ACTION_ALLOW, nullptr));
			_r_ctrl_settext (config.hnotification, IDC_BLOCK_BTN, app.LocaleString (IDS_ACTION_BLOCK, nullptr));

			CheckDlgButton (config.hnotification, IDC_CREATERULE_ADDR_ID, BST_UNCHECKED);
			CheckDlgButton (config.hnotification, IDC_CREATERULE_PORT_ID, BST_UNCHECKED);
			CheckDlgButton (config.hnotification, IDC_DISABLENOTIFICATION_ID, BST_UNCHECKED);

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

			_r_fastlock_releaseshared (&lock_notification);
			_r_fastlock_releaseshared (&lock_access);

			SendMessage (config.hnotification, WM_COMMAND, MAKEWPARAM (IDC_CREATERULE_ADDR_ID, 0), 0);
			SendMessage (config.hnotification, WM_COMMAND, MAKEWPARAM (IDC_CREATERULE_PORT_ID, 0), 0);

			ShowWindow (config.hnotification, is_forced ? SW_SHOW : SW_SHOWNA);

			return true;
		}

		_r_fastlock_releaseshared (&lock_access);
	}

	_r_fastlock_releaseshared (&lock_notification);

	return false;
}

bool _app_notifyrefresh ()
{
	if (!app.ConfigGet (L"IsNotificationsEnabled", true).AsBool ())
	{
		_app_notifyhide (config.hnotification);
		return true;
	}

	_r_fastlock_acquireshared (&lock_notification);

	const size_t idx = _app_notifygetcurrent ();

	if (notifications.empty () || idx == LAST_VALUE || !IsWindowVisible (config.hnotification))
	{
		_app_notifyhide (config.hnotification);
		_r_fastlock_releaseshared (&lock_notification);

		return false;
	}

	_r_fastlock_releaseshared (&lock_notification);

	return _app_notifyshow (idx, false);
}

// Play notification sound even if system have "nosound" mode
void _app_notifyplaysound ()
{
	bool result = false;

	if (!config.notify_snd_path[0] || !_r_fs_exists (config.notify_snd_path))
	{
		HKEY hkey = nullptr;
		config.notify_snd_path[0] = 0;

		if (RegOpenKeyEx (HKEY_CURRENT_USER, L"AppEvents\\Schemes\\Apps\\.Default\\" NOTIFY_SOUND_DEFAULT L"\\.Default", 0, KEY_READ, &hkey) == ERROR_SUCCESS)
		{
			DWORD size = _countof (config.notify_snd_path) * sizeof (WCHAR);

			if (RegQueryValueEx (hkey, nullptr, nullptr, nullptr, (LPBYTE)config.notify_snd_path, &size) == ERROR_SUCCESS)
			{
				const rstring path = _r_path_expand (config.notify_snd_path);

				if (_r_fs_exists (path))
				{
					StringCchCopy (config.notify_snd_path, _countof (config.notify_snd_path), path);
					result = true;
				}
			}

			RegCloseKey (hkey);
		}
	}
	else
	{
		result = true;
	}

	if (!result || !_r_fs_exists (config.notify_snd_path) || !PlaySound (config.notify_snd_path, nullptr, SND_FILENAME | SND_ASYNC))
		PlaySound (NOTIFY_SOUND_DEFAULT, nullptr, SND_ASYNC);
}

void _app_notifyadd (PITEM_LOG const ptr_log)
{
	const time_t current_time = _r_unixtime_now ();
	const PITEM_APP ptr_app = _app_getapplication (ptr_log->hash);

	if (!ptr_app)
		return;

	// check for last display time
	{
		const time_t notification_timeout = app.ConfigGet (L"NotificationsTimeout", NOTIFY_TIMEOUT_DEFAULT).AsLonglong ();

		if (notification_timeout && ((current_time - ptr_app->last_notify) < notification_timeout))
			return;
	}

	_r_fastlock_acquireexclusive (&lock_notification);

	// check limit
	if ((notifications.size () >= NOTIFY_LIMIT_SIZE))
		_app_freenotify (0, true);

	// get existing pool id (if exists)
	size_t chk_idx = LAST_VALUE;

	for (size_t i = 0; i < notifications.size (); i++)
	{
		PITEM_LOG ptr_chk = notifications.at (i);

		if (ptr_chk && ptr_chk->hash == ptr_log->hash)
		{
			chk_idx = i;
			break;
		}
	}

	ptr_app->last_notify = current_time;

	PITEM_LOG ptr_log2 = new ITEM_LOG;
	size_t idx = LAST_VALUE;

	if (ptr_log2)
	{
		CopyMemory (ptr_log2, ptr_log, sizeof (ITEM_LOG));

		if (chk_idx != LAST_VALUE)
		{
			idx = chk_idx;

			delete notifications.at (chk_idx);
			notifications.at (chk_idx) = ptr_log2;
		}
		else
		{
			notifications.push_back (ptr_log2);
			idx = notifications.size () - 1;
		}

		SetWindowLongPtr (config.hnotification, GWLP_USERDATA, chk_idx);

		_r_fastlock_releaseexclusive (&lock_notification);

		_app_notifyrefresh ();

		if (app.ConfigGet (L"IsNotificationsSound", true).AsBool ())
			_app_notifyplaysound ();

		if (!_r_wnd_undercursor (config.hnotification) && _app_notifyshow (idx, true))
		{
			const UINT display_timeout = app.ConfigGet (L"NotificationsDisplayTimeout", NOTIFY_TIMER_DEFAULT).AsUint ();

			if (display_timeout)
				_app_notifysettimeout (config.hnotification, NOTIFY_TIMER_TIMEOUT_ID, true, (display_timeout * _R_SECONDSCLOCK_MSEC));
		}
	}
}

UINT WINAPI LogThread (LPVOID lparam)
{
	const HWND hwnd = (HWND)lparam;

	const bool is_logenabled = app.ConfigGet (L"IsLogEnabled", false).AsBool ();
	const bool is_notificationenabled = (app.ConfigGet (L"Mode", ModeWhitelist).AsUint () == ModeWhitelist) && app.ConfigGet (L"IsNotificationsEnabled", true).AsBool (); // only for whitelist mode

	_r_fastlock_acquireshared (&lock_eventcallback);

	while (true)
	{
		PSLIST_ENTRY ptr_list = InterlockedPopEntrySList (&log_stack.ListHead);

		if (!ptr_list)
			break;

		InterlockedDecrement (&log_stack.Count);

		PITEM_LIST_ENTRY ptr_entry = CONTAINING_RECORD (ptr_list, ITEM_LIST_ENTRY, ListEntry);
		PITEM_LOG ptr_log = (PITEM_LOG)ptr_entry->Body;

		if (ptr_log)
		{
			// apps collector
			if (ptr_log->hash && !ptr_log->is_allow && apps.find (ptr_log->hash) == apps.end ())
			{
				_r_fastlock_acquireexclusive (&lock_access);
				_app_addapplication (hwnd, ptr_log->path, 0, 0, 0, false, false, true);
				_r_fastlock_releaseexclusive (&lock_access);

				_app_listviewsort (hwnd, IDC_LISTVIEW, -1, false);
				_app_profilesave (hwnd);
			}

			if ((is_logenabled || is_notificationenabled) && (!(ptr_log->is_system && app.ConfigGet (L"IsExcludeStealth", true).AsBool ())))
			{
				_app_formataddress (ptr_log->remote_fmt, _countof (ptr_log->remote_fmt), ptr_log, FWP_DIRECTION_OUTBOUND, ptr_log->remote_port, true);
				_app_formataddress (ptr_log->local_fmt, _countof (ptr_log->local_fmt), ptr_log, FWP_DIRECTION_INBOUND, ptr_log->local_port, true);

				// write log to a file
				if (is_logenabled)
					_app_logwrite (ptr_log);

				// show notification (only for my own provider and file is present)
				if (is_notificationenabled && !ptr_log->is_allow && ptr_log->is_myprovider && ptr_log->hash)
				{
					if (!(ptr_log->is_blocklist && app.ConfigGet (L"IsExcludeBlocklist", true).AsBool ()) && !(ptr_log->is_custom && app.ConfigGet (L"IsExcludeCustomRules", true).AsBool ()))
					{
						bool is_silent = true;

						// read app config
						{
							_r_fastlock_acquireshared (&lock_access);

							PITEM_APP const ptr_app = _app_getapplication (ptr_log->hash);

							if (ptr_app)
								is_silent = ptr_app->is_silent;

							_r_fastlock_releaseshared (&lock_access);
						}

						if (!is_silent)
							_app_notifyadd (ptr_log);
					}
				}
			}

			delete ptr_log;
		}

		_aligned_free (ptr_entry);
	}

	InterlockedFlushSList (&log_stack.ListHead);

	_r_fastlock_releaseshared (&lock_eventcallback);

	_endthreadex (0);

	return ERROR_SUCCESS;
}

void CALLBACK _wfp_logcallback (UINT32 flags, FILETIME const* pft, UINT8 const* app_id, SID* package_id, SID* user_id, UINT8 proto, FWP_IP_VERSION ipver, UINT32 remote_addr, FWP_BYTE_ARRAY16 const* remote_addr6, UINT16 remoteport, UINT32 local_addr, FWP_BYTE_ARRAY16 const* local_addr6, UINT16 localport, UINT16 layer_id, UINT64 filter_id, UINT32 direction, bool is_allow, bool is_loopback)
{
	if (_r_fastlock_islocked (&lock_apply))
		return;

	if (is_allow && app.ConfigGet (L"IsExcludeClassifyAllow", true).AsBool ())
		return;

	const bool is_logenabled = app.ConfigGet (L"IsLogEnabled", false).AsBool ();
	const bool is_notificationenabled = (app.ConfigGet (L"Mode", ModeWhitelist).AsUint () == ModeWhitelist) && app.ConfigGet (L"IsNotificationsEnabled", true).AsBool (); // only for whitelist mode

	// do not parse when tcp connection has been established, or when non-tcp traffic has been authorized
	if (layer_id)
	{
		FWPM_LAYER* layer = nullptr;

		if (FwpmLayerGetById (config.hengine, layer_id, &layer) == ERROR_SUCCESS)
		{
			if (layer && (memcmp (&layer->layerKey, &FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4, sizeof (GUID)) == 0 || memcmp (&layer->layerKey, &FWPM_LAYER_ALE_FLOW_ESTABLISHED_V6, sizeof (GUID)) == 0))
			{
				FwpmFreeMemory ((void**)&layer);
				return;
			}

			FwpmFreeMemory ((void**)&layer);
		}
	}

	PITEM_LIST_ENTRY ptr_entry = (PITEM_LIST_ENTRY)_aligned_malloc (sizeof (ITEM_LIST_ENTRY), MEMORY_ALLOCATION_ALIGNMENT);

	if (ptr_entry)
	{
		PITEM_LOG ptr_log = new ITEM_LOG;

		if (!ptr_log)
		{
			_aligned_free (ptr_entry);
			return;
		}

		// get package id (win8+)
		LPWSTR sidstring = nullptr;

		if (package_id)
		{
			if (ConvertSidToStringSid (package_id, &sidstring))
			{
				if (!_app_item_get (&packages, _r_str_hash (sidstring), nullptr, nullptr, nullptr, nullptr, nullptr))
				{
					LocalFree (sidstring);
					sidstring = nullptr;
				}
			}
		}

		// copy converted nt device path into win32
		if (sidstring)
		{
			StringCchCopy (ptr_log->path, _countof (ptr_log->path), sidstring);
			ptr_log->hash = _r_str_hash (ptr_log->path);

			LocalFree (sidstring);
			sidstring = nullptr;
		}
		else if (app_id)
		{
			StringCchCopy (ptr_log->path, _countof (ptr_log->path), _r_path_dospathfromnt (LPCWSTR (app_id)));
			ptr_log->hash = _r_str_hash (ptr_log->path);

			_app_applycasestyle (ptr_log->path, wcslen (ptr_log->path)); // apply case-style
		}
		else
		{
			StringCchCopy (ptr_log->path, _countof (ptr_log->path), SZ_EMPTY);
			ptr_log->hash = 0;
		}

		if (is_logenabled || is_notificationenabled)
		{
			// copy date and time
			if (pft)
				ptr_log->date = _r_unixtime_from_filetime (pft);

			// get username (only if log enabled)
			if (is_logenabled)
			{
				if ((flags & FWPM_NET_EVENT_FLAG_USER_ID_SET) != 0 && user_id)
				{
					SID_NAME_USE sid_type = SidTypeInvalid;

					WCHAR username[MAX_PATH] = {0};
					WCHAR domain[MAX_PATH] = {0};

					DWORD length1 = _countof (username);
					DWORD length2 = _countof (domain);

					if (LookupAccountSid (nullptr, user_id, username, &length1, domain, &length2, &sid_type))
						StringCchPrintf (ptr_log->username, _countof (ptr_log->username), L"%s\\%s", domain, username);

					else
						StringCchCopy (ptr_log->username, _countof (ptr_log->username), SZ_EMPTY);
				}
				else
				{
					StringCchCopy (ptr_log->username, _countof (ptr_log->username), SZ_EMPTY);
				}
			}

			// read filter information
			if (filter_id)
			{
				FWPM_FILTER* filter = nullptr;
				FWPM_PROVIDER* provider = nullptr;

				ptr_log->filter_id = filter_id;

				if (FwpmFilterGetById (config.hengine, filter_id, &filter) == ERROR_SUCCESS)
				{
					StringCchCopy (ptr_log->filter_name, _countof (ptr_log->filter_name), (filter->displayData.description ? filter->displayData.description : filter->displayData.name));

					if (filter->providerKey)
					{
						if (memcmp (filter->providerKey, &GUID_WfpProvider, sizeof (GUID)) == 0)
							ptr_log->is_myprovider = true;

						if (filter->weight.type == FWP_UINT8)
						{
							ptr_log->is_system = (filter->weight.uint8 == FILTER_WEIGHT_HIGHEST);
							ptr_log->is_blocklist = (filter->weight.uint8 == FILTER_WEIGHT_BLOCKLIST);
							ptr_log->is_custom = (filter->weight.uint8 == FILTER_WEIGHT_CUSTOM);
						}

						if (FwpmProviderGetByKey (config.hengine, filter->providerKey, &provider) == ERROR_SUCCESS)
							StringCchCopy (ptr_log->provider_name, _countof (ptr_log->provider_name), (provider->displayData.description ? provider->displayData.description : provider->displayData.name));
					}
				}

				if (filter)
					FwpmFreeMemory ((void**)&filter);

				if (provider)
					FwpmFreeMemory ((void**)&provider);

				if (!ptr_log->filter_name[0])
					StringCchCopy (ptr_log->filter_name, _countof (ptr_log->filter_name), SZ_EMPTY);
			}

			// destination
			{
				// ipv4 address
				if (ipver == FWP_IP_VERSION_V4)
				{
					ptr_log->af = AF_INET;

					// remote address
					ptr_log->remote_addr.S_un.S_addr = ntohl (remote_addr);

					if (remoteport)
						ptr_log->remote_port = remoteport;

					// local address
					ptr_log->local_addr.S_un.S_addr = ntohl (local_addr);

					if (localport)
						ptr_log->local_port = localport;
				}
				else if (ipver == FWP_IP_VERSION_V6)
				{
					ptr_log->af = AF_INET6;

					// remote address
					CopyMemory (ptr_log->remote_addr6.u.Byte, remote_addr6->byteArray16, FWP_V6_ADDR_SIZE);

					if (remoteport)
						ptr_log->remote_port = remoteport;

					// local address
					CopyMemory (&ptr_log->local_addr6.u.Byte, local_addr6->byteArray16, FWP_V6_ADDR_SIZE);

					if (localport)
						ptr_log->local_port = localport;
				}
			}

			// protocol
			ptr_log->protocol = proto;

			// indicates whether the packet originated from (or was heading to) the loopback adapter
			ptr_log->is_loopback = is_loopback;

			// indicates FWPM_NET_EVENT_TYPE_CLASSIFY_ALLOW state
			ptr_log->is_allow = is_allow;

			// indicates the direction of the packet transmission
			if (direction == FWP_DIRECTION_OUTBOUND || direction == FWP_DIRECTION_OUT)
				ptr_log->direction = FWP_DIRECTION_OUTBOUND;

			else if (direction == FWP_DIRECTION_INBOUND || direction == FWP_DIRECTION_IN)
				ptr_log->direction = FWP_DIRECTION_INBOUND;
		}

		// push into a slist
		{
			// prevent pool overflow
			if (InterlockedCompareExchange (&log_stack.Count, 0, 0) >= NOTIFY_LIMIT_POOL_SIZE)
				_app_clear_logstack ();

			ptr_entry->Body = (ULONG_PTR)ptr_log;

			InterlockedPushEntrySList (&log_stack.ListHead, &ptr_entry->ListEntry);
			InterlockedIncrement (&log_stack.Count);

			// check if thread has been terminated
			if (!_r_fastlock_islocked (&lock_eventcallback))
			{
				_r_fastlock_acquireexclusive (&lock_threadpool);

				_app_clear_threadpool (&threads_pool);

				const HANDLE hthread = _r_createthread (&LogThread, app.GetHWND ());

				if (hthread)
				{
					threads_pool.push_back (hthread);
				}
				else
				{
					_app_clear_threadpool (&threads_pool);
				}

				_r_fastlock_releaseexclusive (&lock_threadpool);
			}
		}
	}
}

// win7+ callback
void CALLBACK _wfp_logcallback0 (LPVOID, const FWPM_NET_EVENT1 *pEvent)
{
	if (pEvent)
	{
		UINT16 layer_id = 0;
		UINT64 filter_id = 0;
		UINT32 direction = 0;
		bool is_loopback = false;

		if (pEvent->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP && pEvent->classifyDrop)
		{
			layer_id = pEvent->classifyDrop->layerId;
			filter_id = pEvent->classifyDrop->filterId;
			direction = pEvent->classifyDrop->msFwpDirection;
			is_loopback = pEvent->classifyDrop->isLoopback;
		}
		else if (pEvent->type == FWPM_NET_EVENT_TYPE_IPSEC_KERNEL_DROP && pEvent->ipsecDrop)
		{
			layer_id = pEvent->ipsecDrop->layerId;
			filter_id = pEvent->ipsecDrop->filterId;
			direction = pEvent->ipsecDrop->direction;
		}
		else
		{
			return;
		}

		_wfp_logcallback (pEvent->header.flags, &pEvent->header.timeStamp, pEvent->header.appId.data, nullptr, pEvent->header.userId, pEvent->header.ipProtocol, pEvent->header.ipVersion, pEvent->header.remoteAddrV4, &pEvent->header.remoteAddrV6, pEvent->header.remotePort, pEvent->header.localAddrV4, &pEvent->header.localAddrV6, pEvent->header.localPort, layer_id, filter_id, direction, false, is_loopback);
	}
}

// win8+ callback
void CALLBACK _wfp_logcallback1 (LPVOID, const FWPM_NET_EVENT2 *pEvent)
{
	if (pEvent)
	{
		UINT16 layer_id = 0;
		UINT64 filter_id = 0;
		UINT32 direction = 0;
		bool is_loopback = false;
		bool is_allow = false;

		if (pEvent->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP && pEvent->classifyDrop)
		{
			layer_id = pEvent->classifyDrop->layerId;
			filter_id = pEvent->classifyDrop->filterId;
			direction = pEvent->classifyDrop->msFwpDirection;
			is_loopback = pEvent->classifyDrop->isLoopback;
		}
		else if (pEvent->type == FWPM_NET_EVENT_TYPE_IPSEC_KERNEL_DROP && pEvent->ipsecDrop)
		{
			layer_id = pEvent->ipsecDrop->layerId;
			filter_id = pEvent->ipsecDrop->filterId;
			direction = pEvent->ipsecDrop->direction;
		}
		else if (pEvent->type == FWPM_NET_EVENT_TYPE_CLASSIFY_ALLOW && pEvent->classifyAllow)
		{
			layer_id = pEvent->classifyAllow->layerId;
			filter_id = pEvent->classifyAllow->filterId;
			direction = pEvent->classifyAllow->msFwpDirection;
			is_loopback = pEvent->classifyAllow->isLoopback;

			is_allow = true;
		}
		else if (pEvent->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP_MAC && pEvent->classifyDropMac)
		{
			layer_id = pEvent->classifyDropMac->layerId;
			filter_id = pEvent->classifyDropMac->filterId;
			direction = pEvent->classifyDropMac->msFwpDirection;
			is_loopback = pEvent->classifyDropMac->isLoopback;
		}
		else
		{
			return;
		}

		_wfp_logcallback (pEvent->header.flags, &pEvent->header.timeStamp, pEvent->header.appId.data, pEvent->header.packageSid, pEvent->header.userId, pEvent->header.ipProtocol, pEvent->header.ipVersion, pEvent->header.remoteAddrV4, &pEvent->header.remoteAddrV6, pEvent->header.remotePort, pEvent->header.localAddrV4, &pEvent->header.localAddrV6, pEvent->header.localPort, layer_id, filter_id, direction, is_allow, is_loopback);
	}
}

// win10+ callback
void CALLBACK _wfp_logcallback2 (LPVOID, const FWPM_NET_EVENT3 *pEvent)
{
	if (pEvent)
	{
		UINT16 layer_id = 0;
		UINT64 filter_id = 0;
		UINT32 direction = 0;
		bool is_loopback = false;
		bool is_allow = false;

		if (pEvent->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP && pEvent->classifyDrop)
		{
			layer_id = pEvent->classifyDrop->layerId;
			filter_id = pEvent->classifyDrop->filterId;
			direction = pEvent->classifyDrop->msFwpDirection;
			is_loopback = pEvent->classifyDrop->isLoopback;
		}
		else if (pEvent->type == FWPM_NET_EVENT_TYPE_IPSEC_KERNEL_DROP && pEvent->ipsecDrop)
		{
			layer_id = pEvent->ipsecDrop->layerId;
			filter_id = pEvent->ipsecDrop->filterId;
			direction = pEvent->ipsecDrop->direction;
		}
		else if (pEvent->type == FWPM_NET_EVENT_TYPE_CLASSIFY_ALLOW && pEvent->classifyAllow)
		{
			layer_id = pEvent->classifyAllow->layerId;
			filter_id = pEvent->classifyAllow->filterId;
			direction = pEvent->classifyAllow->msFwpDirection;
			is_loopback = pEvent->classifyAllow->isLoopback;

			is_allow = true;
		}
		else if (pEvent->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP_MAC && pEvent->classifyDropMac)
		{
			layer_id = pEvent->classifyDropMac->layerId;
			filter_id = pEvent->classifyDropMac->filterId;
			direction = pEvent->classifyDropMac->msFwpDirection;
			is_loopback = pEvent->classifyDropMac->isLoopback;
		}
		else
		{
			return;
		}

		_wfp_logcallback (pEvent->header.flags, &pEvent->header.timeStamp, pEvent->header.appId.data, pEvent->header.packageSid, pEvent->header.userId, pEvent->header.ipProtocol, pEvent->header.ipVersion, pEvent->header.remoteAddrV4, &pEvent->header.remoteAddrV6, pEvent->header.remotePort, pEvent->header.localAddrV4, &pEvent->header.localAddrV6, pEvent->header.localPort, layer_id, filter_id, direction, is_allow, is_loopback);
	}
}

// win10rs4+ callback
void CALLBACK _wfp_logcallback3 (LPVOID, const FWPM_NET_EVENT4 *pEvent)
{
	if (pEvent)
	{
		UINT16 layer_id = 0;
		UINT64 filter_id = 0;
		UINT32 direction = 0;
		bool is_loopback = false;
		bool is_allow = false;

		if (pEvent->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP && pEvent->classifyDrop)
		{
			layer_id = pEvent->classifyDrop->layerId;
			filter_id = pEvent->classifyDrop->filterId;
			direction = pEvent->classifyDrop->msFwpDirection;
			is_loopback = pEvent->classifyDrop->isLoopback;
		}
		else if (pEvent->type == FWPM_NET_EVENT_TYPE_IPSEC_KERNEL_DROP && pEvent->ipsecDrop)
		{
			layer_id = pEvent->ipsecDrop->layerId;
			filter_id = pEvent->ipsecDrop->filterId;
			direction = pEvent->ipsecDrop->direction;
		}
		else if (pEvent->type == FWPM_NET_EVENT_TYPE_CLASSIFY_ALLOW && pEvent->classifyAllow)
		{
			layer_id = pEvent->classifyAllow->layerId;
			filter_id = pEvent->classifyAllow->filterId;
			direction = pEvent->classifyAllow->msFwpDirection;
			is_loopback = pEvent->classifyAllow->isLoopback;

			is_allow = true;
		}
		else if (pEvent->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP_MAC && pEvent->classifyDropMac)
		{
			layer_id = pEvent->classifyDropMac->layerId;
			filter_id = pEvent->classifyDropMac->filterId;
			direction = pEvent->classifyDropMac->msFwpDirection;
			is_loopback = pEvent->classifyDropMac->isLoopback;
		}
		else
		{
			return;
		}

		_wfp_logcallback (pEvent->header.flags, &pEvent->header.timeStamp, pEvent->header.appId.data, pEvent->header.packageSid, pEvent->header.userId, pEvent->header.ipProtocol, pEvent->header.ipVersion, pEvent->header.remoteAddrV4, &pEvent->header.remoteAddrV6, pEvent->header.remotePort, pEvent->header.localAddrV4, &pEvent->header.localAddrV6, pEvent->header.localPort, layer_id, filter_id, direction, is_allow, is_loopback);
	}
}

// win10rs5+ callback
void CALLBACK _wfp_logcallback4 (LPVOID, const FWPM_NET_EVENT5 *pEvent)
{
	if (pEvent)
	{
		UINT16 layer_id = 0;
		UINT64 filter_id = 0;
		UINT32 direction = 0;
		bool is_loopback = false;
		bool is_allow = false;

		if (pEvent->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP && pEvent->classifyDrop)
		{
			layer_id = pEvent->classifyDrop->layerId;
			filter_id = pEvent->classifyDrop->filterId;
			direction = pEvent->classifyDrop->msFwpDirection;
			is_loopback = pEvent->classifyDrop->isLoopback;
		}
		else if (pEvent->type == FWPM_NET_EVENT_TYPE_IPSEC_KERNEL_DROP && pEvent->ipsecDrop)
		{
			layer_id = pEvent->ipsecDrop->layerId;
			filter_id = pEvent->ipsecDrop->filterId;
			direction = pEvent->ipsecDrop->direction;
		}
		else if (pEvent->type == FWPM_NET_EVENT_TYPE_CLASSIFY_ALLOW && pEvent->classifyAllow)
		{
			layer_id = pEvent->classifyAllow->layerId;
			filter_id = pEvent->classifyAllow->filterId;
			direction = pEvent->classifyAllow->msFwpDirection;
			is_loopback = pEvent->classifyAllow->isLoopback;

			is_allow = true;
		}
		else if (pEvent->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP_MAC && pEvent->classifyDropMac)
		{
			layer_id = pEvent->classifyDropMac->layerId;
			filter_id = pEvent->classifyDropMac->filterId;
			direction = pEvent->classifyDropMac->msFwpDirection;
			is_loopback = pEvent->classifyDropMac->isLoopback;
		}
		else
		{
			return;
		}

		_wfp_logcallback (pEvent->header.flags, &pEvent->header.timeStamp, pEvent->header.appId.data, pEvent->header.packageSid, pEvent->header.userId, pEvent->header.ipProtocol, pEvent->header.ipVersion, pEvent->header.remoteAddrV4, &pEvent->header.remoteAddrV6, pEvent->header.remotePort, pEvent->header.localAddrV4, &pEvent->header.localAddrV6, pEvent->header.localPort, layer_id, filter_id, direction, is_allow, is_loopback);
	}
}

UINT WINAPI ApplyThread (LPVOID lparam)
{
	const bool is_install = lparam ? true : false;

	_r_ctrl_enable (app.GetHWND (), IDC_START_BTN, false);

	_r_fastlock_acquireexclusive (&lock_access);
	_r_fastlock_acquireexclusive (&lock_apply);

	if (is_install)
	{
		if (_wfp_initialize (true))
			_wfp_installfilters ();
	}
	else
	{
		if (_wfp_initialize (false))
			_wfp_destroyfilters (true);

		_wfp_uninitialize (true);
	}

	_r_fastlock_releaseexclusive (&lock_access);
	_r_fastlock_releaseexclusive (&lock_apply);

	_app_profilesave (app.GetHWND ());

	_r_listview_redraw (app.GetHWND (), IDC_LISTVIEW);

	_r_ctrl_enable (app.GetHWND (), IDC_START_BTN, true);

	SetEvent (config.done_evt);

	_endthreadex (0);

	return ERROR_SUCCESS;
}

void addcolor (UINT locale_id, LPCWSTR config_name, bool is_enabled, LPCWSTR config_value, COLORREF default_clr)
{
	ITEM_COLOR color = {0};
	SecureZeroMemory (&color, sizeof (color));

	if (config_name)
		str_set (&color.pcfg_name, wcslen (config_name) + 1, config_name);

	if (config_value)
		str_set (&color.pcfg_value, wcslen (config_value) + 1, config_value);

	color.hash = _r_str_hash (config_value);
	color.is_enabled = is_enabled;
	color.locale_id = locale_id;
	color.default_clr = default_clr;
	color.clr = app.ConfigGet (config_value, default_clr).AsUlong ();

	colors.push_back (color);
}

void addprotocol (LPCWSTR name, UINT8 id)
{
	ITEM_PROTOCOL protocol = {0};
	SecureZeroMemory (&protocol, sizeof (protocol));

	if (name)
		str_set (&protocol.pname, wcslen (name) + 1, name);

	protocol.id = id;

	protocols.push_back (protocol);
}

void _app_generate_processes ()
{
	// clear previous result
	{
		for (size_t i = 0; i < processes.size (); i++)
		{
			if (processes.at (i).hbmp)
				DeleteObject (processes.at (i).hbmp); // free memory
		}
	}

	processes.clear (); // clear previous result

	NTSTATUS status = 0;

	ULONG length = 0x4000;
	PBYTE buffer = new BYTE[length];

	while (true)
	{
		status = NtQuerySystemInformation (SystemProcessInformation, buffer, length, &length);

		if (status == 0xC0000023L /*STATUS_BUFFER_TOO_SMALL*/ || status == 0xc0000004 /*STATUS_INFO_LENGTH_MISMATCH*/)
		{
			delete[] buffer;
			buffer = new BYTE[length];
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
				WCHAR display_name[128] = {0};
				WCHAR real_path[MAX_PATH] = {0};

				size_t hash = 0;

				StringCchPrintf (display_name, _countof (display_name), L"%s (%lu)", spi->ImageName.Buffer, pid);

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
						_app_applycasestyle (real_path, wcslen (real_path)); // apply case-style
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

					ITEM_ADD item;
					SecureZeroMemory (&item, sizeof (item));

					StringCchCopy (item.display_name, _countof (item.display_name), display_name);
					StringCchCopy (item.real_path, _countof (item.real_path), ((pid == PROC_SYSTEM_PID) ? PROC_SYSTEM_NAME : real_path));

					// get file icon
					{
						HICON hicon = nullptr;

						if (!app.ConfigGet (L"IsIconsHidden", false).AsBool () && _app_getfileicon (real_path, true, nullptr, &hicon))
						{
							item.hbmp = _app_ico2bmp (hicon);
							DestroyIcon (hicon);
						}
						else
						{
							item.hbmp = _app_ico2bmp (config.hicon_small);
						}
					}

					processes.push_back (item);
				}

				CloseHandle (hprocess);
			}
		}
		while ((spi = ((spi->NextEntryOffset ? (PSYSTEM_PROCESS_INFORMATION)((PCHAR)(spi)+(spi)->NextEntryOffset) : nullptr))) != nullptr);

		std::sort (processes.begin (), processes.end (),
			[](const ITEM_ADD& a, const ITEM_ADD& b)->bool {
			return StrCmpLogicalW (a.display_name, b.display_name) == -1;
		});
	}

	delete[] buffer; // free the allocated buffer
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
	tdc.pszMainIcon = TD_WARNING_ICON;
	tdc.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
	tdc.pszMainInstruction = main;
	tdc.pszVerificationText = flag;
	tdc.pfCallback = &_r_msg_callback;
	tdc.lpCallbackData = MAKELONG (0, 1);

	if (is_install)
	{
		StringCchCopy (main, _countof (main), app.LocaleString (IDS_QUESTION_START, nullptr));
		StringCchCopy (flag, _countof (flag), app.LocaleString (IDS_DISABLEWINDOWSFIREWALL_CHK, nullptr));

		tdc.pszContent = mode;
		tdc.pRadioButtons = tdr;
		tdc.cRadioButtons = _countof (tdr);
		tdc.nDefaultRadioButton = IDM_TRAY_MODEWHITELIST + app.ConfigGet (L"Mode", ModeWhitelist).AsUint ();

		tdr[0].nButtonID = IDM_TRAY_MODEWHITELIST;
		tdr[0].pszButtonText = mode1;

		tdr[1].nButtonID = IDM_TRAY_MODEBLACKLIST;
		tdr[1].pszButtonText = mode2;

		StringCchCopy (mode, _countof (mode), app.LocaleString (IDS_TRAY_MODE, L":"));
		StringCchCopy (mode1, _countof (mode1), app.LocaleString (IDS_MODE_WHITELIST, nullptr));
		StringCchCopy (mode2, _countof (mode2), app.LocaleString (IDS_MODE_BLACKLIST, nullptr));

		if (app.ConfigGet (L"IsDisableWindowsFirewallChecked", true).AsBool ())
			tdc.dwFlags |= TDF_VERIFICATION_FLAG_CHECKED;
	}
	else
	{
		StringCchCopy (main, _countof (main), app.LocaleString (IDS_QUESTION_STOP, nullptr));
		StringCchCopy (flag, _countof (flag), app.LocaleString (IDS_ENABLEWINDOWSFIREWALL_CHK, nullptr));

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
				lpnmlv->nmcd.hdr.idFrom == IDC_APPS_LV
				)
			{
				const size_t hash = lpnmlv->nmcd.lItemlParam;

				if (hash)
				{
					const COLORREF new_clr = (COLORREF)_app_getcolor (hash);

					if (new_clr)
					{
						if (app.ConfigGet (L"IsBigView", true).AsBool () && lpnmlv->nmcd.hdr.idFrom == IDC_LISTVIEW)
						{
							lpnmlv->clrText = _r_dc_getcolorbrightness (new_clr);
							lpnmlv->clrTextBk = new_clr;
						}
						else
						{
							lpnmlv->clrText = _r_dc_getcolorbrightness (new_clr);
							lpnmlv->clrTextBk = new_clr;

							_r_dc_fillrect (lpnmlv->nmcd.hdc, &lpnmlv->nmcd.rc, new_clr);
						}

						result = CDRF_NEWFONT;
					}
				}
			}
			else if (lpnmlv->nmcd.hdr.idFrom == IDC_COLORS)
			{
				PITEM_COLOR const ptr_clr = &colors.at (lpnmlv->nmcd.lItemlParam);

				lpnmlv->clrTextBk = ptr_clr->clr;
				lpnmlv->clrText = _r_dc_getcolorbrightness (ptr_clr->clr);

				_r_dc_fillrect (lpnmlv->nmcd.hdc, &lpnmlv->nmcd.rc, lpnmlv->clrTextBk);

				result = CDRF_NEWFONT;
			}
			else if (lpnmlv->nmcd.hdr.idFrom == IDC_EDITOR)
			{
				COLORREF const custclr = (COLORREF)lparam;

				if (custclr)
				{
					lpnmlv->clrTextBk = custclr;
					lpnmlv->clrText = _r_dc_getcolorbrightness (custclr);

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
	static PITEM_RULE ptr_rule = nullptr;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			ptr_rule = (PITEM_RULE)lparam;

			// configure window
			_r_wnd_center (hwnd, GetParent (hwnd));

			SendMessage (hwnd, WM_SETICON, ICON_SMALL, (LPARAM)app.GetSharedIcon (app.GetHINSTANCE (), IDI_MAIN, GetSystemMetrics (SM_CXSMICON)));
			SendMessage (hwnd, WM_SETICON, ICON_BIG, (LPARAM)app.GetSharedIcon (app.GetHINSTANCE (), IDI_MAIN, GetSystemMetrics (SM_CXICON)));

			// localize window
			SetWindowText (hwnd, (ptr_rule && ptr_rule->pname ? _r_fmt (L"%s - \"%s\"", app.LocaleString (IDS_EDITOR, nullptr).GetString (), ptr_rule->pname) : app.LocaleString (IDS_EDITOR, nullptr)));

			SetDlgItemText (hwnd, IDC_NAME, app.LocaleString (IDS_NAME, L":"));
			SetDlgItemText (hwnd, IDC_RULE_REMOTE, app.LocaleString (IDS_RULE, L" (" SZ_LOG_REMOTE_ADDRESS L"):"));
			SetDlgItemText (hwnd, IDC_RULE_LOCAL, app.LocaleString (IDS_RULE, L" (" SZ_LOG_LOCAL_ADDRESS L"):"));
			SetDlgItemText (hwnd, IDC_REGION, app.LocaleString (IDS_REGION, L":"));
			SetDlgItemText (hwnd, IDC_DIRECTION, app.LocaleString (IDS_DIRECTION, L":"));
			SetDlgItemText (hwnd, IDC_PROTOCOL, app.LocaleString (IDS_PROTOCOL, L":"));
			SetDlgItemText (hwnd, IDC_PORTVERSION, app.LocaleString (IDS_PORTVERSION, L":"));
			SetDlgItemText (hwnd, IDC_ACTION, app.LocaleString (IDS_ACTION, L":"));

			SetDlgItemText (hwnd, IDC_DISABLE_CHK, app.LocaleString (IDS_DISABLE_CHK, nullptr));
			SetDlgItemText (hwnd, IDC_ENABLE_CHK, app.LocaleString (IDS_ENABLE_CHK, nullptr));
			SetDlgItemText (hwnd, IDC_ENABLEFORAPPS_CHK, app.LocaleString (IDS_ENABLEFORAPPS_CHK, nullptr));

			SetDlgItemText (hwnd, IDC_WIKI, app.LocaleString (IDS_WIKI, nullptr));
			SetDlgItemText (hwnd, IDC_SAVE, app.LocaleString (IDS_SAVE, nullptr));
			SetDlgItemText (hwnd, IDC_CLOSE, app.LocaleString (IDS_CLOSE, nullptr));

			// configure listview
			_r_listview_setstyle (hwnd, IDC_APPS_LV, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_CHECKBOXES);

			_r_listview_addcolumn (hwnd, IDC_APPS_LV, 0, nullptr, 95, LVCFMT_LEFT);

			_app_listviewsetimagelist (hwnd, IDC_APPS_LV);
			_app_listviewsetfont (hwnd, IDC_APPS_LV, false);

			// name
			if (ptr_rule && ptr_rule->pname)
				SetDlgItemText (hwnd, IDC_NAME_EDIT, ptr_rule->pname);

			// rule (remote)
			if (ptr_rule && ptr_rule->prule_remote)
				SetDlgItemText (hwnd, IDC_RULE_REMOTE_EDIT, ptr_rule->prule_remote);

			// rule (local)
			if (ptr_rule && ptr_rule->prule_local)
				SetDlgItemText (hwnd, IDC_RULE_LOCAL_EDIT, ptr_rule->prule_local);

			// apps (apply to)
			{
				size_t item = 0;

				_r_fastlock_acquireshared (&lock_access);

				for (auto& p : apps)
				{
					PITEM_APP const ptr_app = &p.second;

					// windows store apps (win8+)
					if (ptr_app->type == AppStore && !_r_sys_validversion (6, 2))
						continue;

					config.is_nocheckboxnotify = true;

					_r_listview_additem (hwnd, IDC_APPS_LV, item, 0, _r_path_extractfile (ptr_app->display_name), ptr_app->icon_id, LAST_VALUE, p.first);
					_r_listview_setitemcheck (hwnd, IDC_APPS_LV, item, ptr_rule && !ptr_rule->apps.empty () && (ptr_rule->apps.find (p.first) != ptr_rule->apps.end ()));

					config.is_nocheckboxnotify = false;

					item += 1;
				}

				_r_fastlock_releaseshared (&lock_access);

				// sort column
				_app_listviewsort_ab (hwnd, IDC_APPS_LV);

				// resize column
				RECT rc = {0};
				GetClientRect (GetDlgItem (hwnd, IDC_APPS_LV), &rc);

				_r_listview_setcolumn (hwnd, IDC_APPS_LV, 0, nullptr, (rc.right - rc.left));
			}

			// direction
			SendDlgItemMessage (hwnd, IDC_DIRECTION_EDIT, CB_INSERTSTRING, 0, (LPARAM)app.LocaleString (IDS_DIRECTION_1, nullptr).GetString ());
			SendDlgItemMessage (hwnd, IDC_DIRECTION_EDIT, CB_INSERTSTRING, 1, (LPARAM)app.LocaleString (IDS_DIRECTION_2, nullptr).GetString ());
			SendDlgItemMessage (hwnd, IDC_DIRECTION_EDIT, CB_INSERTSTRING, 2, (LPARAM)app.LocaleString (IDS_DIRECTION_3, nullptr).GetString ());

			if (ptr_rule)
				SendDlgItemMessage (hwnd, IDC_DIRECTION_EDIT, CB_SETCURSEL, (WPARAM)ptr_rule->dir, 0);

			// protocol
			SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_INSERTSTRING, 0, (LPARAM)app.LocaleString (IDS_ALL, nullptr).GetString ());
			SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_SETCURSEL, 0, 0);

			SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_SETEXTENDEDUI, 1, 0);

			for (size_t i = 0; i < protocols.size (); i++)
			{
				PITEM_PROTOCOL ptr_protocol = &protocols.at (i);

				SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_INSERTSTRING, i + 1, (LPARAM)_r_fmt (L"#%03d - %s", ptr_protocol->id, ptr_protocol->pname).GetString ());
				SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_SETITEMDATA, i + 1, (LPARAM)ptr_protocol->id);

				if (ptr_rule && ptr_rule->protocol == ptr_protocol->id)
					SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_SETCURSEL, (WPARAM)i + 1, 0);
			}

			// family (ports-only)
			SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_INSERTSTRING, 0, (LPARAM)app.LocaleString (IDS_ALL, nullptr).GetString ());
			SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_SETITEMDATA, 0, (LPARAM)AF_UNSPEC);

			SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_INSERTSTRING, 1, (LPARAM)L"IPv4");
			SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_SETITEMDATA, 1, (LPARAM)AF_INET);

			SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_INSERTSTRING, 2, (LPARAM)L"IPv6");
			SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_SETITEMDATA, 2, (LPARAM)AF_INET6);

			if (ptr_rule)
			{
				if (ptr_rule->af == AF_UNSPEC)
					SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_SETCURSEL, (WPARAM)0, 0);

				else if (ptr_rule->af == AF_INET)
					SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_SETCURSEL, (WPARAM)1, 0);

				else if (ptr_rule->af == AF_INET6)
					SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_SETCURSEL, (WPARAM)2, 0);
			}

			// action
			SendDlgItemMessage (hwnd, IDC_ACTION_EDIT, CB_INSERTSTRING, 0, (LPARAM)app.LocaleString (IDS_ACTION_ALLOW, nullptr).GetString ());
			SendDlgItemMessage (hwnd, IDC_ACTION_EDIT, CB_INSERTSTRING, 1, (LPARAM)app.LocaleString (IDS_ACTION_BLOCK, nullptr).GetString ());

			if (ptr_rule)
				SendDlgItemMessage (hwnd, IDC_ACTION_EDIT, CB_SETCURSEL, (WPARAM)ptr_rule->is_block, 0);

			// state
			{
				UINT ctrl_id = IDC_DISABLE_CHK;

				if (ptr_rule && ptr_rule->is_enabled)
				{
					ctrl_id = ptr_rule->apps.empty () ? IDC_ENABLE_CHK : IDC_ENABLEFORAPPS_CHK;

					if (!ptr_rule->apps.empty ())
						ctrl_id = IDC_ENABLEFORAPPS_CHK;
				}

				CheckRadioButton (hwnd, IDC_DISABLE_CHK, IDC_ENABLEFORAPPS_CHK, ctrl_id);
				SendMessage (hwnd, WM_COMMAND, MAKEWPARAM (ctrl_id, 0), 0);
			}

			// set limitation
			SendDlgItemMessage (hwnd, IDC_NAME_EDIT, EM_LIMITTEXT, RULE_NAME_CCH_MAX - 1, 0);
			SendDlgItemMessage (hwnd, IDC_RULE_REMOTE_EDIT, EM_LIMITTEXT, RULE_RULE_CCH_MAX - 1, 0);
			SendDlgItemMessage (hwnd, IDC_RULE_LOCAL_EDIT, EM_LIMITTEXT, RULE_RULE_CCH_MAX - 1, 0);

			_r_wnd_addstyle (hwnd, IDC_WIKI, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (hwnd, IDC_SAVE, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (hwnd, IDC_CLOSE, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

			_r_ctrl_enable (hwnd, IDC_SAVE, (SendDlgItemMessage (hwnd, IDC_NAME_EDIT, WM_GETTEXTLENGTH, 0, 0) > 0) && ((SendDlgItemMessage (hwnd, IDC_RULE_REMOTE_EDIT, WM_GETTEXTLENGTH, 0, 0) > 0) || (SendDlgItemMessage (hwnd, IDC_RULE_LOCAL_EDIT, WM_GETTEXTLENGTH, 0, 0) > 0))); // enable apply button

			break;
		}

		case WM_CONTEXTMENU:
		{
			if (GetDlgCtrlID ((HWND)wparam) == IDC_APPS_LV)
			{
				const HMENU menu = LoadMenu (nullptr, MAKEINTRESOURCE (IDM_EDITOR));
				const HMENU submenu = GetSubMenu (menu, 0);

				app.LocaleMenu (submenu, IDS_CHECK, IDM_CHECK, false, nullptr);
				app.LocaleMenu (submenu, IDS_UNCHECK, IDM_UNCHECK, false, nullptr);

				if (!SendDlgItemMessage (hwnd, IDC_APPS_LV, LVM_GETSELECTEDCOUNT, 0, 0))
				{
					EnableMenuItem (submenu, IDM_CHECK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					EnableMenuItem (submenu, IDM_UNCHECK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
				}

				DeleteMenu (submenu, IDM_ADD, MF_BYCOMMAND);
				DeleteMenu (submenu, IDM_EDIT, MF_BYCOMMAND);
				DeleteMenu (submenu, IDM_DELETE, MF_BYCOMMAND);
				DeleteMenu (submenu, 0, MF_BYPOSITION);
				DeleteMenu (submenu, IDM_SELECT_ALL, MF_BYCOMMAND);
				DeleteMenu (submenu, 0, MF_BYPOSITION);

				POINT pt = {0};
				GetCursorPos (&pt);

				TrackPopupMenuEx (submenu, TPM_RIGHTBUTTON | TPM_LEFTBUTTON, pt.x, pt.y, hwnd, nullptr);

				DestroyMenu (menu);
			}

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lparam;

			switch (nmlp->code)
			{
				case NM_CUSTOMDRAW:
				{
					if (nmlp->idFrom != IDC_APPS_LV)
						break;

					const LONG result = _app_wmcustdraw ((LPNMLVCUSTOMDRAW)lparam, 0);

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, result);
					return result;
				}

				case LVN_ITEMCHANGED:
				{
					LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lparam;

					if (lpnmlv->uNewState == 8192 || lpnmlv->uNewState == 4096)
					{
						if (config.is_nocheckboxnotify)
							return FALSE;

						_app_listviewsort_ab (hwnd, IDC_APPS_LV);
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
					StringCchCopy (lpnmlv->szMarkup, _countof (lpnmlv->szMarkup), app.LocaleString (IDS_STATUS_EMPTY, nullptr));

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
				_r_ctrl_enable (hwnd, IDC_SAVE, (SendDlgItemMessage (hwnd, IDC_NAME_EDIT, WM_GETTEXTLENGTH, 0, 0) > 0) && ((SendDlgItemMessage (hwnd, IDC_RULE_REMOTE_EDIT, WM_GETTEXTLENGTH, 0, 0) > 0) || (SendDlgItemMessage (hwnd, IDC_RULE_LOCAL_EDIT, WM_GETTEXTLENGTH, 0, 0) > 0))); // enable apply button
				return FALSE;
			}

			switch (LOWORD (wparam))
			{
				case IDC_DISABLE_CHK:
				case IDC_ENABLE_CHK:
				case IDC_ENABLEFORAPPS_CHK:
				{
					_r_ctrl_enable (hwnd, IDC_APPS_LV, IsDlgButtonChecked (hwnd, IDC_ENABLEFORAPPS_CHK) == BST_CHECKED);
					break;
				}

				case IDM_CHECK:
				case IDM_UNCHECK:
				{
					const bool new_val = (LOWORD (wparam) == IDM_CHECK) ? true : false;
					size_t item = LAST_VALUE;

					while ((item = (size_t)SendDlgItemMessage (hwnd, IDC_APPS_LV, LVM_GETNEXTITEM, item, LVNI_SELECTED)) != LAST_VALUE)
					{
						config.is_nocheckboxnotify = true;

						_r_listview_setitemcheck (hwnd, IDC_APPS_LV, item, new_val);

						config.is_nocheckboxnotify = false;
					}

					_app_listviewsort_ab (hwnd, IDC_APPS_LV);

					break;
				}

				case IDC_WIKI:
				{
					ShellExecute (hwnd, nullptr, WIKI_URL, nullptr, nullptr, SW_SHOWDEFAULT);
					break;
				}

				case IDOK: // process Enter key
				case IDC_SAVE:
				{
					if (!SendDlgItemMessage (hwnd, IDC_NAME_EDIT, WM_GETTEXTLENGTH, 0, 0) || (!SendDlgItemMessage (hwnd, IDC_RULE_REMOTE_EDIT, WM_GETTEXTLENGTH, 0, 0) && !SendDlgItemMessage (hwnd, IDC_RULE_LOCAL_EDIT, WM_GETTEXTLENGTH, 0, 0)))
						return FALSE;

					// rule destination
					{
						rstring rule_remote = _r_ctrl_gettext (hwnd, IDC_RULE_REMOTE_EDIT).Trim (L"\r\n " RULE_DELIMETER);
						size_t rule_remote_length = min (rule_remote.GetLength (), RULE_RULE_CCH_MAX) + 1;

						rstring rule_local = _r_ctrl_gettext (hwnd, IDC_RULE_LOCAL_EDIT).Trim (L"\r\n " RULE_DELIMETER);
						size_t rule_local_length = min (rule_local.GetLength (), RULE_RULE_CCH_MAX) + 1;

						// here we parse and check rule syntax
						{
							rstring::rvector arr = rule_remote.AsVector (RULE_DELIMETER);
							rstring rule_remote_fixed;

							for (size_t i = 0; i < arr.size (); i++)
							{
								LPCWSTR rule_single = arr.at (i).Trim (L" " RULE_DELIMETER);

								if (!_app_parserulestring (rule_single, nullptr, nullptr))
								{
									_r_ctrl_showtip (hwnd, IDC_RULE_REMOTE_EDIT, TTI_ERROR, APP_NAME, _r_fmt (app.LocaleString (IDS_STATUS_SYNTAX_ERROR, nullptr), rule_single));
									_r_ctrl_enable (hwnd, IDC_SAVE, false);

									return FALSE;
								}

								rule_remote_fixed.AppendFormat (L"%s" RULE_DELIMETER, rule_single);
							}

							rule_remote = rule_remote_fixed.Trim (L" " RULE_DELIMETER);
							rule_remote_length = min (rule_remote.GetLength (), RULE_RULE_CCH_MAX) + 1;
						}

						// here we parse and check rule syntax
						{
							rstring::rvector arr = rule_local.AsVector (RULE_DELIMETER);
							rstring rule_local_fixed;

							for (size_t i = 0; i < arr.size (); i++)
							{
								LPCWSTR rule_single = arr.at (i).Trim (L" " RULE_DELIMETER);

								if (!_app_parserulestring (rule_single, nullptr, nullptr))
								{
									_r_ctrl_showtip (hwnd, IDC_RULE_LOCAL_EDIT, TTI_ERROR, APP_NAME, _r_fmt (app.LocaleString (IDS_STATUS_SYNTAX_ERROR, nullptr), rule_single));
									_r_ctrl_enable (hwnd, IDC_SAVE, false);

									return FALSE;
								}

								rule_local_fixed.AppendFormat (L"%s" RULE_DELIMETER, rule_single);
							}

							rule_local = rule_local_fixed.Trim (L" " RULE_DELIMETER);
							rule_local_length = min (rule_local.GetLength (), RULE_RULE_CCH_MAX) + 1;
						}

						_r_fastlock_acquireexclusive (&lock_access);

						// save rule (remote)
						str_set (&ptr_rule->prule_remote, rule_remote_length, rule_remote);
						str_set (&ptr_rule->prule_local, rule_local_length, rule_local);
					}

					// save rule name
					{
						const rstring name = _r_ctrl_gettext (hwnd, IDC_NAME_EDIT).Trim (L"\r\n " RULE_DELIMETER);

						if (!name.IsEmpty ())
						{
							const size_t name_length = min (name.GetLength (), RULE_NAME_CCH_MAX) + 1;

							str_set (&ptr_rule->pname, name_length, name);
						}
					}

					// save rule apps
					{
						ptr_rule->apps.clear ();

						const bool is_enable = (IsDlgButtonChecked (hwnd, IDC_ENABLE_CHK) != BST_CHECKED);

						for (size_t i = 0; i < _r_listview_getitemcount (hwnd, IDC_APPS_LV); i++)
						{
							const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, IDC_APPS_LV, i);

							if (hash)
							{
								const bool is_apply = is_enable && _r_listview_isitemchecked (hwnd, IDC_APPS_LV, i);

								if (is_apply)
									ptr_rule->apps[hash] = true;

								_r_listview_setitem (app.GetHWND (), IDC_LISTVIEW, _app_getposition (app.GetHWND (), hash), 0, nullptr, LAST_VALUE, _app_getappgroup (hash, _app_getapplication (hash)));
							}
						}
					}

					ptr_rule->protocol = (UINT8)SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_GETITEMDATA, SendDlgItemMessage (hwnd, IDC_PROTOCOL_EDIT, CB_GETCURSEL, 0, 0), 0);
					ptr_rule->af = (ADDRESS_FAMILY)SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_GETITEMDATA, SendDlgItemMessage (hwnd, IDC_PORTVERSION_EDIT, CB_GETCURSEL, 0, 0), 0);

					ptr_rule->dir = (FWP_DIRECTION)SendDlgItemMessage (hwnd, IDC_DIRECTION_EDIT, CB_GETCURSEL, 0, 0);
					ptr_rule->is_block = SendDlgItemMessage (hwnd, IDC_ACTION_EDIT, CB_GETCURSEL, 0, 0) ? true : false;
					ptr_rule->is_enabled = (IsDlgButtonChecked (hwnd, IDC_DISABLE_CHK) == BST_UNCHECKED) ? true : false;
					ptr_rule->weight = (ptr_rule->is_block ? FILTER_WEIGHT_CUSTOM_BLOCK : FILTER_WEIGHT_CUSTOM);

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

INT_PTR CALLBACK SettingsProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static PAPP_SETTINGS_PAGE ptr_page = nullptr;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			ptr_page = (PAPP_SETTINGS_PAGE)lparam;
			break;
		}

		case RM_INITIALIZE:
		{
			const UINT dialog_id = (UINT)wparam;

			switch (dialog_id)
			{
				case IDD_SETTINGS_GENERAL:
				{
					CheckDlgButton (hwnd, IDC_ALWAYSONTOP_CHK, app.ConfigGet (L"AlwaysOnTop", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_STARTMINIMIZED_CHK, app.ConfigGet (L"IsStartMinimized", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

#ifdef _APP_HAVE_AUTORUN
					CheckDlgButton (hwnd, IDC_LOADONSTARTUP_CHK, app.AutorunIsEnabled () ? BST_CHECKED : BST_UNCHECKED);
#endif // _APP_HAVE_AUTORUN

#ifdef _APP_HAVE_SKIPUAC
					CheckDlgButton (hwnd, IDC_SKIPUACWARNING_CHK, app.SkipUacIsEnabled () ? BST_CHECKED : BST_UNCHECKED);
#endif // _APP_HAVE_SKIPUAC

					CheckDlgButton (hwnd, IDC_CHECKUPDATES_CHK, app.ConfigGet (L"CheckUpdates", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

#if defined(_APP_BETA) || defined(_APP_BETA_RC)
					CheckDlgButton (hwnd, IDC_CHECKUPDATESBETA_CHK, BST_CHECKED);
					_r_ctrl_enable (hwnd, IDC_CHECKUPDATESBETA_CHK, false);
#else
					CheckDlgButton (hwnd, IDC_CHECKUPDATESBETA_CHK, app.ConfigGet (L"CheckUpdatesBeta", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					if (!app.ConfigGet (L"CheckUpdates", true).AsBool ())
						_r_ctrl_enable (hwnd, IDC_CHECKUPDATESBETA_CHK, false);
#endif

					app.LocaleEnum (hwnd, IDC_LANGUAGE, false, 0);

					break;
				}

				case IDD_SETTINGS_RULES:
				{
					CheckDlgButton (hwnd, IDC_USECERTIFICATES_CHK, app.ConfigGet (L"IsCerificatesEnabled", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_USENETWORKRESOLUTION_CHK, app.ConfigGet (L"IsNetworkResolutionsEnabled", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_RULE_ALLOWINBOUND, app.ConfigGet (L"AllowInboundConnections", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_RULE_ALLOWLISTEN, app.ConfigGet (L"AllowListenConnections2", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_RULE_ALLOWLOOPBACK, app.ConfigGet (L"AllowLoopbackConnections", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					CheckDlgButton (hwnd, IDC_USESTEALTHMODE_CHK, app.ConfigGet (L"UseStealthMode", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_INSTALLBOOTTIMEFILTERS_CHK, app.ConfigGet (L"InstallBoottimeFilters", false).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					_r_ctrl_settip (hwnd, IDC_RULE_ALLOWINBOUND, LPSTR_TEXTCALLBACK);
					_r_ctrl_settip (hwnd, IDC_RULE_ALLOWLISTEN, LPSTR_TEXTCALLBACK);
					_r_ctrl_settip (hwnd, IDC_RULE_ALLOWLOOPBACK, LPSTR_TEXTCALLBACK);

					_r_ctrl_settip (hwnd, IDC_USESTEALTHMODE_CHK, LPSTR_TEXTCALLBACK);
					_r_ctrl_settip (hwnd, IDC_INSTALLBOOTTIMEFILTERS_CHK, LPSTR_TEXTCALLBACK);
					_r_ctrl_settip (hwnd, IDC_PROXYSUPPORT_CHK, LPSTR_TEXTCALLBACK);

					if (!_r_sys_validversion (6, 2))
						_r_ctrl_enable (hwnd, IDC_PROXYSUPPORT_CHK, false);

					break;
				}

				case IDD_SETTINGS_INTERFACE:
				{
					CheckDlgButton (hwnd, IDC_CONFIRMEXIT_CHK, app.ConfigGet (L"ConfirmExit2", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_CONFIRMEXITTIMER_CHK, app.ConfigGet (L"ConfirmExitTimer", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_CONFIRMDELETE_CHK, app.ConfigGet (L"ConfirmDelete", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_CONFIRMLOGCLEAR_CHK, app.ConfigGet (L"ConfirmLogClear", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					break;
				}

				case IDD_SETTINGS_HIGHLIGHTING:
				{
					// configure listview
					_r_listview_setstyle (hwnd, IDC_COLORS, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_CHECKBOXES);

					_app_listviewsetimagelist (hwnd, IDC_COLORS);

					_r_listview_deleteallitems (hwnd, IDC_COLORS);
					_r_listview_deleteallcolumns (hwnd, IDC_COLORS);

					_r_listview_addcolumn (hwnd, IDC_COLORS, 0, app.LocaleString (IDS_NAME, nullptr), 100, LVCFMT_LEFT);

					{
						for (size_t i = 0; i < colors.size (); i++)
						{
							PITEM_COLOR ptr_clr = &colors.at (i);

							ptr_clr->clr = app.ConfigGet (ptr_clr->pcfg_value, ptr_clr->default_clr).AsUlong ();

							config.is_nocheckboxnotify = true;

							_r_listview_additem (hwnd, IDC_COLORS, i, 0, app.LocaleString (ptr_clr->locale_id, nullptr), config.icon_id, LAST_VALUE, i);
							_r_listview_setitemcheck (hwnd, IDC_COLORS, i, app.ConfigGet (ptr_clr->pcfg_name, ptr_clr->is_enabled).AsBool ());

							config.is_nocheckboxnotify = false;
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

					_r_listview_addcolumn (hwnd, IDC_EDITOR, 0, app.LocaleString (IDS_NAME, nullptr), 49, LVCFMT_LEFT);
					_r_listview_addcolumn (hwnd, IDC_EDITOR, 1, app.LocaleString (IDS_DIRECTION, nullptr), 26, LVCFMT_LEFT);
					_r_listview_addcolumn (hwnd, IDC_EDITOR, 2, app.LocaleString (IDS_PROTOCOL, nullptr), 20, LVCFMT_LEFT);

					_r_listview_addgroup (hwnd, IDC_EDITOR, 0, nullptr, 0, LVGS_COLLAPSIBLE);
					_r_listview_addgroup (hwnd, IDC_EDITOR, 1, nullptr, 0, LVGS_COLLAPSIBLE);
					_r_listview_addgroup (hwnd, IDC_EDITOR, 2, nullptr, 0, LVGS_COLLAPSIBLE);

					_app_listviewsetfont (hwnd, IDC_EDITOR, false);

					std::vector<PITEM_RULE> const* ptr_rules = nullptr;

					if (dialog_id == IDD_SETTINGS_RULES_BLOCKLIST)
						ptr_rules = &rules_blocklist;

					else if (dialog_id == IDD_SETTINGS_RULES_SYSTEM)
						ptr_rules = &rules_system;

					else if (dialog_id == IDD_SETTINGS_RULES_CUSTOM)
						ptr_rules = &rules_custom;

					if (ptr_rules && !ptr_rules->empty ())
					{
						for (size_t i = 0, item = 0; i < ptr_rules->size (); i++)
						{
							PITEM_RULE const ptr_rule = ptr_rules->at (i);

							if (!ptr_rule)
								continue;

							size_t group_id = 2;

							if (ptr_rule->is_enabled)
								group_id = ptr_rule->apps.empty () ? 0 : 1;

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

					SetDlgItemText (hwnd, IDC_LOGPATH, app.ConfigGet (L"LogPath", LOG_PATH_DEFAULT));

					UDACCEL ud = {0};
					ud.nInc = 64; // set step to 64kb

					SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_SETACCEL, 1, (LPARAM)&ud);
					SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_SETRANGE32, 64, 2048);
					SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_SETPOS32, 0, app.ConfigGet (L"LogSizeLimitKb", 256).AsUint ());

					CheckDlgButton (hwnd, IDC_ENABLENOTIFICATIONS_CHK, app.ConfigGet (L"IsNotificationsEnabled", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_NOTIFICATIONSOUND_CHK, app.ConfigGet (L"IsNotificationsSound", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					SendDlgItemMessage (hwnd, IDC_NOTIFICATIONDISPLAYTIMEOUT, UDM_SETRANGE32, 0, _R_SECONDSCLOCK_MIN (30));
					SendDlgItemMessage (hwnd, IDC_NOTIFICATIONDISPLAYTIMEOUT, UDM_SETPOS32, 0, app.ConfigGet (L"NotificationsDisplayTimeout", NOTIFY_TIMER_DEFAULT).AsUint ());

					SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_SETRANGE32, 0, _R_SECONDSCLOCK_DAY (7));
					SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_SETPOS32, 0, app.ConfigGet (L"NotificationsTimeout", NOTIFY_TIMEOUT_DEFAULT).AsUint ());

					CheckDlgButton (hwnd, IDC_EXCLUDESTEALTH_CHK, app.ConfigGet (L"IsExcludeStealth", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, app.ConfigGet (L"IsExcludeClassifyAllow", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_EXCLUDEBLOCKLIST_CHK, app.ConfigGet (L"IsExcludeBlocklist", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton (hwnd, IDC_EXCLUDECUSTOM_CHK, app.ConfigGet (L"IsExcludeCustomRules", true).AsBool () ? BST_CHECKED : BST_UNCHECKED);

					if (!_r_sys_validversion (6, 2))
						_r_ctrl_enable (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, false);

					// dropped packets logging (win7+)
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

		case RM_LOCALIZE:
		{
			const UINT dialog_id = (UINT)wparam;

			// localize titles
			SetDlgItemText (hwnd, IDC_TITLE_GENERAL, app.LocaleString (IDS_TITLE_GENERAL, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_LANGUAGE, app.LocaleString (IDS_TITLE_LANGUAGE, L": (Language)"));
			SetDlgItemText (hwnd, IDC_TITLE_CONFIRMATIONS, app.LocaleString (IDS_TITLE_CONFIRMATIONS, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_HIGHLIGHTING, app.LocaleString (IDS_TITLE_HIGHLIGHTING, L":"));
			SetDlgItemText (hwnd, IDC_TITLE_EXPERTS1, app.LocaleString (IDS_TITLE_EXPERTS, L" #1:"));
			SetDlgItemText (hwnd, IDC_TITLE_EXPERTS2, app.LocaleString (IDS_TITLE_EXPERTS, L" #2:"));
			SetDlgItemText (hwnd, IDC_TITLE_LOGGING, app.LocaleString (IDS_TITLE_LOGGING, _r_sys_validversion (6, 1) ? L":" : L": (win7+)"));
			SetDlgItemText (hwnd, IDC_TITLE_NOTIFICATIONS, app.LocaleString (IDS_TITLE_NOTIFICATIONS, _r_sys_validversion (6, 1) ? L":" : L": (win7+)"));

			switch (dialog_id)
			{
				case IDD_SETTINGS_GENERAL:
				{
					SetDlgItemText (hwnd, IDC_ALWAYSONTOP_CHK, app.LocaleString (IDS_ALWAYSONTOP_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_STARTMINIMIZED_CHK, app.LocaleString (IDS_STARTMINIMIZED_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_LOADONSTARTUP_CHK, app.LocaleString (IDS_LOADONSTARTUP_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_SKIPUACWARNING_CHK, app.LocaleString (IDS_SKIPUACWARNING_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_CHECKUPDATES_CHK, app.LocaleString (IDS_CHECKUPDATES_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_CHECKUPDATESBETA_CHK, app.LocaleString (IDS_CHECKUPDATESBETA_CHK, nullptr));

					SetDlgItemText (hwnd, IDC_LANGUAGE_HINT, app.LocaleString (IDS_LANGUAGE_HINT, nullptr));

					break;
				}

				case IDD_SETTINGS_INTERFACE:
				{
					SetDlgItemText (hwnd, IDC_CONFIRMEXIT_CHK, app.LocaleString (IDS_CONFIRMEXIT_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_CONFIRMEXITTIMER_CHK, app.LocaleString (IDS_CONFIRMEXITTIMER_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_CONFIRMDELETE_CHK, app.LocaleString (IDS_CONFIRMDELETE_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_CONFIRMLOGCLEAR_CHK, app.LocaleString (IDS_CONFIRMLOGCLEAR_CHK, nullptr));

					break;
				}

				case IDD_SETTINGS_HIGHLIGHTING:
				{
					SetDlgItemText (hwnd, IDC_COLORS_HINT, app.LocaleString (IDS_COLORS_HINT, nullptr));

					for (size_t i = 0; i < _r_listview_getitemcount (hwnd, IDC_COLORS); i++)
					{
						const size_t idx = (size_t)_r_listview_getitemlparam (hwnd, IDC_COLORS, i);

						PITEM_COLOR const ptr_clr = &colors.at (idx);
						_r_listview_setitem (hwnd, IDC_COLORS, i, 0, app.LocaleString (ptr_clr->locale_id, nullptr));
					}

					_app_listviewsetfont (hwnd, IDC_COLORS, false);

					break;
				}

				case IDD_SETTINGS_RULES:
				{
					SetDlgItemText (hwnd, IDC_USECERTIFICATES_CHK, app.LocaleString (IDS_USECERTIFICATES_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_USENETWORKRESOLUTION_CHK, app.LocaleString (IDS_USENETWORKRESOLUTION_CHK, nullptr));

					SetDlgItemText (hwnd, IDC_RULE_ALLOWINBOUND, app.LocaleString (IDS_RULE_ALLOWINBOUND, nullptr));
					SetDlgItemText (hwnd, IDC_RULE_ALLOWLISTEN, app.LocaleString (IDS_RULE_ALLOWLISTEN, nullptr));
					SetDlgItemText (hwnd, IDC_RULE_ALLOWLOOPBACK, app.LocaleString (IDS_RULE_ALLOWLOOPBACK, nullptr));

					SetDlgItemText (hwnd, IDC_USESTEALTHMODE_CHK, app.LocaleString (IDS_USESTEALTHMODE_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_INSTALLBOOTTIMEFILTERS_CHK, app.LocaleString (IDS_INSTALLBOOTTIMEFILTERS_CHK, nullptr));

					break;
				}

				case IDD_SETTINGS_RULES_BLOCKLIST:
				case IDD_SETTINGS_RULES_SYSTEM:
				case IDD_SETTINGS_RULES_CUSTOM:
				{
					_r_listview_setcolumn (hwnd, IDC_EDITOR, 0, app.LocaleString (IDS_NAME, nullptr), 0);
					_r_listview_setcolumn (hwnd, IDC_EDITOR, 1, app.LocaleString (IDS_DIRECTION, nullptr), 0);
					_r_listview_setcolumn (hwnd, IDC_EDITOR, 2, app.LocaleString (IDS_PROTOCOL, nullptr), 0);

					std::vector<PITEM_RULE> const* ptr_rules = nullptr;

					_r_fastlock_acquireshared (&lock_access);

					if (dialog_id == IDD_SETTINGS_RULES_BLOCKLIST)
						ptr_rules = &rules_blocklist;

					else if (dialog_id == IDD_SETTINGS_RULES_SYSTEM)
						ptr_rules = &rules_system;

					else if (dialog_id == IDD_SETTINGS_RULES_CUSTOM)
						ptr_rules = &rules_custom;

					if (ptr_rules && !ptr_rules->empty ())
					{
						const size_t total_count = _r_listview_getitemcount (hwnd, IDC_EDITOR);
						size_t group1_count = 0;
						size_t group2_count = 0;
						size_t group3_count = 0;

						for (size_t i = 0; i < total_count; i++)
						{
							const size_t idx = (size_t)_r_listview_getitemlparam (hwnd, IDC_EDITOR, i);
							PITEM_RULE const ptr_rule = ptr_rules->at (idx);

							if (!ptr_rule)
								continue;

							rstring protocol = app.LocaleString (IDS_ALL, nullptr);

							// protocol
							if (ptr_rule->protocol)
								protocol = _app_getprotoname (ptr_rule->protocol);

							const size_t group_id = _app_getrulegroup (ptr_rule);

							if (group_id == 0)
								group1_count += 1;

							else if (group_id == 1)
								group2_count += 1;

							else
								group3_count += 1;

							config.is_nocheckboxnotify = true;

							_r_listview_setitem (hwnd, IDC_EDITOR, i, 0, ptr_rule->pname, ptr_rule->is_block ? 1 : 0, _app_getrulegroup (ptr_rule));
							_r_listview_setitem (hwnd, IDC_EDITOR, i, 1, app.LocaleString (IDS_DIRECTION_1 + ptr_rule->dir, nullptr));
							_r_listview_setitem (hwnd, IDC_EDITOR, i, 2, protocol);

							_r_listview_setitemcheck (hwnd, IDC_EDITOR, i, ptr_rule->is_enabled);

							config.is_nocheckboxnotify = false;
						}

						_r_listview_setgroup (hwnd, IDC_EDITOR, 0, app.LocaleString (IDS_GROUP_ENABLED, _r_fmt (L" (%zu/%zu)", group1_count, total_count)), 0, 0);
						_r_listview_setgroup (hwnd, IDC_EDITOR, 1, app.LocaleString (IDS_GROUP_SPECIAL, _r_fmt (L" (%zu/%zu)", group2_count, total_count)), 0, 0);
						_r_listview_setgroup (hwnd, IDC_EDITOR, 2, app.LocaleString (IDS_GROUP_DISABLED, _r_fmt (L" (%zu/%zu)", group3_count, total_count)), 0, 0);
					}

					_r_fastlock_releaseshared (&lock_access);

					_app_listviewsort_ab (hwnd, IDC_EDITOR);
					_r_listview_redraw (hwnd, IDC_EDITOR);

					SetDlgItemText (hwnd, IDC_RULES_BLOCKLIST_HINT, app.LocaleString (IDS_RULES_BLOCKLIST_HINT, nullptr));
					SetDlgItemText (hwnd, IDC_RULES_SYSTEM_HINT, app.LocaleString (IDS_RULES_SYSTEM_HINT, nullptr));
					_r_ctrl_settext (hwnd, IDC_RULES_CUSTOM_HINT, app.LocaleString (IDS_RULES_USER_HINT, nullptr), WIKI_URL);

					SendDlgItemMessage (hwnd, IDC_EDITOR, LVM_RESETEMPTYTEXT, 0, 0);

					break;
				}

				case IDD_SETTINGS_LOG:
				{
					SetDlgItemText (hwnd, IDC_ENABLELOG_CHK, app.LocaleString (IDS_ENABLELOG_CHK, nullptr));

					SetDlgItemText (hwnd, IDC_LOGSIZELIMIT_HINT, app.LocaleString (IDS_LOGSIZELIMIT_HINT, nullptr));

					SetDlgItemText (hwnd, IDC_ENABLENOTIFICATIONS_CHK, app.LocaleString (IDS_ENABLENOTIFICATIONS_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_NOTIFICATIONSOUND_CHK, app.LocaleString (IDS_NOTIFICATIONSOUND_CHK, nullptr));

					SetDlgItemText (hwnd, IDC_NOTIFICATIONDISPLAYTIMEOUT_HINT, app.LocaleString (IDS_NOTIFICATIONDISPLAYTIMEOUT_HINT, nullptr));
					SetDlgItemText (hwnd, IDC_NOTIFICATIONTIMEOUT_HINT, app.LocaleString (IDS_NOTIFICATIONTIMEOUT_HINT, nullptr));

					SetDlgItemText (hwnd, IDC_EXCLUDE1, app.LocaleString (IDS_TITLE_EXCLUDE, L":"));
					SetDlgItemText (hwnd, IDC_EXCLUDE2, app.LocaleString (IDS_TITLE_EXCLUDE, L":"));

					SetDlgItemText (hwnd, IDC_EXCLUDESTEALTH_CHK, app.LocaleString (IDS_EXCLUDESTEALTH_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, app.LocaleString (IDS_EXCLUDECLASSIFYALLOW_CHK, (_r_sys_validversion (6, 2) ? nullptr : L" [win8+]")));
					SetDlgItemText (hwnd, IDC_EXCLUDEBLOCKLIST_CHK, app.LocaleString (IDS_EXCLUDEBLOCKLIST_CHK, nullptr));
					SetDlgItemText (hwnd, IDC_EXCLUDECUSTOM_CHK, app.LocaleString (IDS_EXCLUDECUSTOM_CHK, nullptr));

					_r_wnd_addstyle (hwnd, IDC_LOGPATH_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

					break;
				}
			}

			break;
		}

		case WM_VSCROLL:
		case WM_HSCROLL:
		{
			const UINT ctrl_id = GetDlgCtrlID ((HWND)lparam);

			if (ctrl_id == IDC_LOGSIZELIMIT)
				app.ConfigSet (L"LogSizeLimitKb", (DWORD)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));

			else if (ctrl_id == IDC_NOTIFICATIONDISPLAYTIMEOUT)
				app.ConfigSet (L"NotificationsDisplayTimeout", (DWORD)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));

			else if (ctrl_id == IDC_NOTIFICATIONTIMEOUT)
				app.ConfigSet (L"NotificationsTimeout", (DWORD)SendDlgItemMessage (hwnd, ctrl_id, UDM_GETPOS32, 0, 0));

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

						if (ctrl_id == IDC_RULE_ALLOWINBOUND)
							StringCchCopy (buffer, _countof (buffer), app.LocaleString (IDS_RULE_ALLOWINBOUND_HINT, nullptr));

						else if (ctrl_id == IDC_RULE_ALLOWLISTEN)
							StringCchCopy (buffer, _countof (buffer), app.LocaleString (IDS_RULE_ALLOWLISTEN_HINT, nullptr));

						else if (ctrl_id == IDC_RULE_ALLOWLOOPBACK)
							StringCchCopy (buffer, _countof (buffer), app.LocaleString (IDS_RULE_ALLOWLOOPBACK_HINT, nullptr));

						else if (ctrl_id == IDC_USESTEALTHMODE_CHK)
							StringCchCopy (buffer, _countof (buffer), app.LocaleString (IDS_USESTEALTHMODE_HINT, nullptr));

						else if (ctrl_id == IDC_INSTALLBOOTTIMEFILTERS_CHK)
							StringCchCopy (buffer, _countof (buffer), app.LocaleString (IDS_INSTALLBOOTTIMEFILTERS_HINT, nullptr));

						else if (ctrl_id == IDC_PROXYSUPPORT_CHK)
							StringCchCopy (buffer, _countof (buffer), app.LocaleString (IDS_PROXYSUPPORT_HINT, nullptr));

						if (buffer[0])
							lpnmdi->lpszText = buffer;
					}

					break;
				}

				case LVN_ITEMCHANGED:
				{
					if (config.is_nocheckboxnotify)
						break;

					LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lparam;

					if (lpnmlv->uNewState == 8192 || lpnmlv->uNewState == 4096)
					{
						const bool new_val = (lpnmlv->uNewState == 8192) ? true : false;
						const size_t idx = lpnmlv->lParam;

						if (nmlp->idFrom == IDC_COLORS)
						{
							PITEM_COLOR ptr_clr = &colors.at (idx);

							app.ConfigSet (ptr_clr->pcfg_name, new_val);

							_r_listview_redraw (app.GetHWND (), IDC_LISTVIEW);
						}
						else if (nmlp->idFrom == IDC_EDITOR)
						{
							std::vector<PITEM_RULE> const* ptr_rules = nullptr;
							const LONG_PTR dialog_id = GetWindowLongPtr (hwnd, GWLP_USERDATA);

							if (dialog_id == IDD_SETTINGS_RULES_BLOCKLIST)
								ptr_rules = &rules_blocklist;

							else if (dialog_id == IDD_SETTINGS_RULES_SYSTEM)
								ptr_rules = &rules_system;

							else if (dialog_id == IDD_SETTINGS_RULES_CUSTOM)
								ptr_rules = &rules_custom;

							if (ptr_rules)
							{
								PITEM_RULE ptr_rule = ptr_rules->at (idx);

								if (ptr_rule)
								{
									ptr_rule->is_enabled = new_val;
									ptr_rule->is_haveerrors = false;

									if (
										dialog_id == IDD_SETTINGS_RULES_BLOCKLIST ||
										dialog_id == IDD_SETTINGS_RULES_SYSTEM
										)
									{
										if (ptr_rule->pname)
											rules_config[ptr_rule->pname] = new_val;
									}

									{
										config.is_nocheckboxnotify = true;

										_r_listview_setitem (hwnd, IDC_EDITOR, lpnmlv->iItem, 0, nullptr, LAST_VALUE, _app_getrulegroup (ptr_rule));

										config.is_nocheckboxnotify = false;
									}

									SendMessage (hwnd, RM_LOCALIZE, dialog_id, (LPARAM)ptr_page);

									_app_listviewsort (app.GetHWND (), IDC_LISTVIEW, -1, false);

									_app_profilesave (app.GetHWND ());

									_wfp_create4filters (ptr_rule, false);

									_r_listview_redraw (app.GetHWND (), IDC_LISTVIEW);
								}
							}
						}
					}

					break;
				}

				case LVN_GETINFOTIP:
				{
					const UINT ctrl_id = (UINT)nmlp->idFrom;
					const LONG_PTR dialog_id = GetWindowLongPtr (hwnd, GWLP_USERDATA);

					if (ctrl_id != IDC_EDITOR)
						break;

					LPNMLVGETINFOTIP lpnmlv = (LPNMLVGETINFOTIP)lparam;

					_r_fastlock_acquireshared (&lock_access);

					const size_t idx = (size_t)_r_listview_getitemlparam (hwnd, ctrl_id, lpnmlv->iItem);
					PITEM_RULE ptr_rule = nullptr;

					if (dialog_id == IDD_SETTINGS_RULES_BLOCKLIST)
						ptr_rule = rules_blocklist.at (idx);

					else if (dialog_id == IDD_SETTINGS_RULES_SYSTEM)
						ptr_rule = rules_system.at (idx);

					else if (dialog_id == IDD_SETTINGS_RULES_CUSTOM)
						ptr_rule = rules_custom.at (idx);

					if (ptr_rule)
					{
						rstring rule_remote = ptr_rule->prule_remote;
						rstring rule_local = ptr_rule->prule_local;

						rule_remote = rule_remote.IsEmpty () ? app.LocaleString (IDS_STATUS_EMPTY, nullptr) : rule_remote.Replace (RULE_DELIMETER, L"\r\n" SZ_TAB);
						rule_local = rule_local.IsEmpty () ? app.LocaleString (IDS_STATUS_EMPTY, nullptr) : rule_local.Replace (RULE_DELIMETER, L"\r\n" SZ_TAB);

						StringCchPrintf (lpnmlv->pszText, lpnmlv->cchTextMax, L"%s (#%zu)\r\n%s:\r\n%s%s\r\n%s:\r\n%s%s", ptr_rule->pname, idx, app.LocaleString (IDS_RULE, L" (" SZ_LOG_REMOTE_ADDRESS L")").GetString (), SZ_TAB, rule_remote.GetString (), app.LocaleString (IDS_RULE, L" (" SZ_LOG_LOCAL_ADDRESS L")").GetString (), SZ_TAB, rule_local.GetString ());

						if (!ptr_rule->apps.empty ())
							StringCchCat (lpnmlv->pszText, lpnmlv->cchTextMax, _r_fmt (L"\r\n%s:\r\n%s%s", app.LocaleString (IDS_FILEPATH, nullptr).GetString (), SZ_TAB, _app_rulesexpand (ptr_rule).GetString ()));
					}

					_r_fastlock_releaseshared (&lock_access);

					break;
				}

				case NM_CUSTOMDRAW:
				{
					if (nmlp->idFrom != IDC_COLORS && nmlp->idFrom != IDC_EDITOR)
						break;

					LPNMLVCUSTOMDRAW lpnmlv = (LPNMLVCUSTOMDRAW)lparam;
					LPARAM code = 0;

					if (lpnmlv->nmcd.hdr.idFrom == IDC_EDITOR && lpnmlv->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
					{
						const LONG_PTR dialog_id = GetWindowLongPtr (hwnd, GWLP_USERDATA);
						const size_t idx = lpnmlv->nmcd.lItemlParam;
						PITEM_RULE ptr_rule = nullptr;

						_r_fastlock_acquireshared (&lock_access);

						{
							if (dialog_id == IDD_SETTINGS_RULES_BLOCKLIST)
								ptr_rule = rules_blocklist.at (idx);

							else if (dialog_id == IDD_SETTINGS_RULES_SYSTEM)
								ptr_rule = rules_system.at (idx);

							else if (dialog_id == IDD_SETTINGS_RULES_CUSTOM)
								ptr_rule = rules_custom.at (idx);
						}

						if (ptr_rule)
						{
							if (ptr_rule->is_haveerrors)
								code = (LPARAM)_app_getcolorvalue (_r_str_hash (L"ColorInvalid"));

							else if (!ptr_rule->apps.empty ())
								code = (LPARAM)_app_getcolorvalue (_r_str_hash (L"ColorSpecial"));
						}

						_r_fastlock_releaseshared (&lock_access);
					}

					const LONG result = _app_wmcustdraw (lpnmlv, code);

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, result);
					return result;
				}

				case NM_DBLCLK:
				{
					LPNMITEMACTIVATE lpnmlv = (LPNMITEMACTIVATE)lparam;

					if (lpnmlv->iItem == -1)
						break;

					if (nmlp->idFrom == IDC_COLORS)
					{
						const size_t idx = (size_t)_r_listview_getitemlparam (hwnd, IDC_COLORS, lpnmlv->iItem);

						CHOOSECOLOR cc = {0};
						COLORREF cust[16] = {0};

						for (size_t i = 0; i < min (_countof (cust), colors.size ()); i++)
							cust[i] = colors.at (i).default_clr;

						cc.lStructSize = sizeof (cc);
						cc.Flags = CC_RGBINIT | CC_FULLOPEN;
						cc.hwndOwner = hwnd;
						cc.lpCustColors = cust;
						cc.rgbResult = colors.at (idx).clr;

						if (ChooseColor (&cc))
						{
							PITEM_COLOR ptr_clr = &colors.at (idx);

							ptr_clr->clr = cc.rgbResult;

							app.ConfigSet (ptr_clr->pcfg_value, cc.rgbResult);

							_r_listview_redraw (hwnd, IDC_COLORS);
							_r_listview_redraw (app.GetHWND (), IDC_LISTVIEW);
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
						PNMLINK nmlink = (PNMLINK)lparam;

						if (nmlink->item.szUrl && nmlink->item.szUrl[0])
							ShellExecute (hwnd, nullptr, nmlink->item.szUrl, nullptr, nullptr, SW_SHOWDEFAULT);
					}

					break;
				}

				case LVN_GETEMPTYMARKUP:
				{
					NMLVEMPTYMARKUP* lpnmlv = (NMLVEMPTYMARKUP*)lparam;

					lpnmlv->dwFlags = EMF_CENTERED;
					StringCchCopy (lpnmlv->szMarkup, _countof (lpnmlv->szMarkup), app.LocaleString (IDS_STATUS_EMPTY, nullptr));

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
					return TRUE;
				}
			}

			break;
		}

		case WM_CONTEXTMENU:
		{
			const UINT ctrl_id = GetDlgCtrlID ((HWND)wparam);
			const LONG_PTR dialog_id = GetWindowLongPtr (hwnd, GWLP_USERDATA);

			if (ctrl_id != IDC_EDITOR && ctrl_id != IDC_COLORS)
				break;

			const HMENU menu = LoadMenu (nullptr, MAKEINTRESOURCE (IDM_EDITOR));
			const HMENU submenu = GetSubMenu (menu, 0);

			// localize
			app.LocaleMenu (submenu, IDS_SELECT_ALL, IDM_SELECT_ALL, false, nullptr);
			app.LocaleMenu (submenu, IDS_CHECK, IDM_CHECK, false, nullptr);
			app.LocaleMenu (submenu, IDS_UNCHECK, IDM_UNCHECK, false, nullptr);

			if (!SendDlgItemMessage (hwnd, ctrl_id, LVM_GETSELECTEDCOUNT, 0, 0))
			{
				EnableMenuItem (submenu, IDM_EDIT, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
				EnableMenuItem (submenu, IDM_DELETE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
				EnableMenuItem (submenu, IDM_CHECK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
				EnableMenuItem (submenu, IDM_UNCHECK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
			}

			if (ctrl_id == IDC_EDITOR && dialog_id == IDD_SETTINGS_RULES_CUSTOM)
			{
				app.LocaleMenu (submenu, IDS_ADD, IDM_ADD, false, L"...");
				app.LocaleMenu (submenu, IDS_EDIT2, IDM_EDIT, false, L"...");
				app.LocaleMenu (submenu, IDS_DELETE, IDM_DELETE, false, nullptr);
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
			switch (LOWORD (wparam))
			{
				case IDC_ALWAYSONTOP_CHK:
				case IDC_STARTMINIMIZED_CHK:
				case IDC_LOADONSTARTUP_CHK:
				case IDC_SKIPUACWARNING_CHK:
				case IDC_CHECKUPDATES_CHK:
				case IDC_CHECKUPDATESBETA_CHK:
				case IDC_LANGUAGE:
				case IDC_CONFIRMEXIT_CHK:
				case IDC_CONFIRMEXITTIMER_CHK:
				case IDC_CONFIRMDELETE_CHK:
				case IDC_CONFIRMLOGCLEAR_CHK:
				case IDC_USESTEALTHMODE_CHK:
				case IDC_INSTALLBOOTTIMEFILTERS_CHK:
				case IDC_PROXYSUPPORT_CHK:
				case IDC_USECERTIFICATES_CHK:
				case IDC_USENETWORKRESOLUTION_CHK:
				case IDC_RULE_ALLOWINBOUND:
				case IDC_RULE_ALLOWLISTEN:
				case IDC_RULE_ALLOWLOOPBACK:
				case IDC_ENABLELOG_CHK:
				case IDC_LOGPATH:
				case IDC_LOGPATH_BTN:
				case IDC_LOGSIZELIMIT_CTRL:
				case IDC_ENABLENOTIFICATIONS_CHK:
				case IDC_NOTIFICATIONSOUND_CHK:
				case IDC_NOTIFICATIONDISPLAYTIMEOUT_CTRL:
				case IDC_NOTIFICATIONTIMEOUT_CTRL:
				case IDC_EXCLUDESTEALTH_CHK:
				case IDC_EXCLUDECLASSIFYALLOW_CHK:
				case IDC_EXCLUDEBLOCKLIST_CHK:
				case IDC_EXCLUDECUSTOM_CHK:
				{
					const UINT ctrl_id = LOWORD (wparam);
					const UINT notify_code = HIWORD (wparam);

					if (ctrl_id == IDC_ALWAYSONTOP_CHK)
					{
						app.ConfigSet (L"AlwaysOnTop", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
						CheckMenuItem (GetMenu (app.GetHWND ()), IDM_ALWAYSONTOP_CHK, MF_BYCOMMAND | ((IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? MF_CHECKED : MF_UNCHECKED));
					}
					else if (ctrl_id == IDC_STARTMINIMIZED_CHK)
					{
						app.ConfigSet (L"IsStartMinimized", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
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

#if !defined(_APP_BETA) && !defined(_APP_BETA_RC)
						_r_ctrl_enable (hwnd, IDC_CHECKUPDATESBETA_CHK, (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
#endif
					}
					else if (ctrl_id == IDC_CHECKUPDATESBETA_CHK)
					{
						app.ConfigSet (L"CheckUpdatesBeta", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
					}
					else if (ctrl_id == IDC_LANGUAGE && notify_code == CBN_SELCHANGE)
					{
						app.LocaleApplyFromControl (hwnd, ctrl_id);
					}
					else if (ctrl_id == IDC_CONFIRMEXIT_CHK)
					{
						app.ConfigSet (L"ConfirmExit2", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
					}
					else if (ctrl_id == IDC_CONFIRMEXITTIMER_CHK)
					{
						app.ConfigSet (L"ConfirmExitTimer", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
					}
					else if (ctrl_id == IDC_CONFIRMDELETE_CHK)
					{
						app.ConfigSet (L"ConfirmDelete", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
					}
					else if (ctrl_id == IDC_CONFIRMLOGCLEAR_CHK)
					{
						app.ConfigSet (L"ConfirmLogClear", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
					}
					else if (ctrl_id == IDC_USECERTIFICATES_CHK)
					{
						app.ConfigSet (L"IsCerificatesEnabled", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);

						if (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED)
						{
							_r_fastlock_acquireexclusive (&lock_access);

							for (auto &p : apps)
							{
								PITEM_APP ptr_app = &p.second;

								if (ptr_app->type == AppRegular)
									ptr_app->is_signed = _app_verifysignature (p.first, ptr_app->real_path, &ptr_app->signer);
							}

							_r_fastlock_releaseexclusive (&lock_access);
						}

						_r_listview_redraw (app.GetHWND (), IDC_LISTVIEW);
					}
					else if (ctrl_id == IDC_USENETWORKRESOLUTION_CHK)
					{
						app.ConfigSet (L"IsNetworkResolutionsEnabled", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);
					}
					else if (ctrl_id == IDC_RULE_ALLOWINBOUND)
					{
						if (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED && !app.ConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION_EXPERT, nullptr), L"ConfirmExpert"))
						{
							CheckDlgButton (hwnd, ctrl_id, BST_UNCHECKED);
							return TRUE;
						}

						app.ConfigSet (L"AllowInboundConnections", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);

						_wfp_create2filters (false);
					}
					else if (ctrl_id == IDC_RULE_ALLOWLISTEN)
					{
						if (IsDlgButtonChecked (hwnd, ctrl_id) == BST_UNCHECKED && !app.ConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION_EXPERT, nullptr), L"ConfirmExpert"))
						{
							CheckDlgButton (hwnd, ctrl_id, BST_CHECKED);
							return TRUE;
						}

						app.ConfigSet (L"AllowListenConnections2", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);

						_wfp_create2filters (false);
					}
					else if (ctrl_id == IDC_RULE_ALLOWLOOPBACK)
					{
						if (IsDlgButtonChecked (hwnd, ctrl_id) == BST_UNCHECKED && !app.ConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION_EXPERT, nullptr), L"ConfirmExpert"))
						{
							CheckDlgButton (hwnd, ctrl_id, BST_CHECKED);
							return TRUE;
						}

						app.ConfigSet (L"AllowLoopbackConnections", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);

						_wfp_create2filters (false);
					}
					else if (ctrl_id == IDC_USESTEALTHMODE_CHK)
					{
						if (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED && !app.ConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION_EXPERT, nullptr), L"ConfirmExpert"))
						{
							CheckDlgButton (hwnd, ctrl_id, BST_UNCHECKED);
							return TRUE;
						}

						app.ConfigSet (L"UseStealthMode", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);

						_wfp_create2filters (false);
					}
					else if (ctrl_id == IDC_INSTALLBOOTTIMEFILTERS_CHK)
					{
						if (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED && !app.ConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION_EXPERT, nullptr), L"ConfirmExpert"))
						{
							CheckDlgButton (hwnd, ctrl_id, BST_UNCHECKED);
							return true;
						}

						app.ConfigSet (L"InstallBoottimeFilters", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED) ? true : false);

						_wfp_create2filters (false);
					}
					else if (ctrl_id == IDC_ENABLELOG_CHK)
					{
						const bool is_enabled = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

						app.ConfigSet (L"IsLogEnabled", is_enabled);
						CheckMenuItem (GetMenu (app.GetHWND ()), IDM_ENABLELOG_CHK, MF_BYCOMMAND | (is_enabled ? MF_CHECKED : MF_UNCHECKED));

						_r_ctrl_enable (hwnd, IDC_LOGPATH, is_enabled); // input
						_r_ctrl_enable (hwnd, IDC_LOGPATH_BTN, is_enabled); // button

						EnableWindow ((HWND)SendDlgItemMessage (hwnd, IDC_LOGSIZELIMIT, UDM_GETBUDDY, 0, 0), is_enabled);

						_r_ctrl_enable (hwnd, IDC_EXCLUDESTEALTH_CHK, is_enabled);

						if (_r_sys_validversion (6, 2))
							_r_ctrl_enable (hwnd, IDC_EXCLUDECLASSIFYALLOW_CHK, is_enabled);

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
						ofn.lpstrFilter = L"*." LOG_PATH_EXT L"\0*." LOG_PATH_EXT L"\0\0";
						ofn.lpstrDefExt = LOG_PATH_EXT;
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
					else if (ctrl_id == IDC_ENABLENOTIFICATIONS_CHK)
					{
						const bool is_enabled = (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED);

						app.ConfigSet (L"IsNotificationsEnabled", is_enabled);
						CheckMenuItem (GetMenu (app.GetHWND ()), IDM_ENABLENOTIFICATIONS_CHK, MF_BYCOMMAND | (is_enabled ? MF_CHECKED : MF_UNCHECKED));

						_r_ctrl_enable (hwnd, IDC_NOTIFICATIONSOUND_CHK, is_enabled);

						EnableWindow ((HWND)SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_GETBUDDY, 0, 0), is_enabled);
						EnableWindow ((HWND)SendDlgItemMessage (hwnd, IDC_NOTIFICATIONDISPLAYTIMEOUT, UDM_GETBUDDY, 0, 0), is_enabled);

						_r_ctrl_enable (hwnd, IDC_EXCLUDEBLOCKLIST_CHK, is_enabled);
						_r_ctrl_enable (hwnd, IDC_EXCLUDECUSTOM_CHK, is_enabled);

						_app_notifyrefresh ();
					}
					else if (ctrl_id == IDC_NOTIFICATIONSOUND_CHK)
					{
						app.ConfigSet (L"IsNotificationsSound", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					}
					else if (ctrl_id == IDC_NOTIFICATIONDISPLAYTIMEOUT_CTRL && notify_code == EN_KILLFOCUS)
					{
						app.ConfigSet (L"NotificationsDisplayTimeout", (DWORD)SendDlgItemMessage (hwnd, IDC_NOTIFICATIONDISPLAYTIMEOUT, UDM_GETPOS32, 0, 0));
					}
					else if (ctrl_id == IDC_NOTIFICATIONTIMEOUT_CTRL && notify_code == EN_KILLFOCUS)
					{
						app.ConfigSet (L"NotificationsTimeout", (DWORD)SendDlgItemMessage (hwnd, IDC_NOTIFICATIONTIMEOUT, UDM_GETPOS32, 0, 0));
					}
					else if (ctrl_id == IDC_EXCLUDESTEALTH_CHK)
					{
						app.ConfigSet (L"IsExcludeStealth", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					}
					else if (ctrl_id == IDC_EXCLUDECLASSIFYALLOW_CHK)
					{
						app.ConfigSet (L"IsExcludeClassifyAllow", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					}
					else if (ctrl_id == IDC_EXCLUDEBLOCKLIST_CHK)
					{
						app.ConfigSet (L"IsExcludeBlocklist", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					}
					else if (ctrl_id == IDC_EXCLUDECUSTOM_CHK)
					{
						app.ConfigSet (L"IsExcludeCustomRules", (IsDlgButtonChecked (hwnd, ctrl_id) == BST_CHECKED));
					}

					break;
				}

				case IDM_ADD:
				{
					const LONG_PTR dialog_id = GetWindowLongPtr (hwnd, GWLP_USERDATA);

					if (dialog_id != IDD_SETTINGS_RULES_CUSTOM)
						break;

					PITEM_RULE ptr_rule = new ITEM_RULE;

					if (ptr_rule)
					{
						ptr_rule->is_enabled = true;
						ptr_rule->is_block = ((app.ConfigGet (L"Mode", ModeWhitelist).AsUint () == ModeWhitelist) ? false : true);

						if (DialogBoxParam (nullptr, MAKEINTRESOURCE (IDD_EDITOR), hwnd, &EditorProc, (LPARAM)ptr_rule))
						{
							_r_fastlock_acquireexclusive (&lock_access);
							rules_custom.push_back (ptr_rule);
							_r_fastlock_releaseexclusive (&lock_access);

							const size_t item = _r_listview_getitemcount (hwnd, IDC_EDITOR);

							config.is_nocheckboxnotify = true;

							_r_listview_additem (hwnd, IDC_EDITOR, item, 0, ptr_rule->pname, ptr_rule->is_block ? 1 : 0, _app_getrulegroup (ptr_rule), rules_custom.size () - 1);
							_r_listview_setitemcheck (hwnd, IDC_EDITOR, item, ptr_rule->is_enabled);

							config.is_nocheckboxnotify = false;

							SendMessage (hwnd, RM_LOCALIZE, dialog_id, (LPARAM)ptr_page);

							_app_listviewsort (app.GetHWND (), IDC_LISTVIEW, -1, false);
							_app_listviewsort_ab (hwnd, IDC_EDITOR);

							_app_profilesave (app.GetHWND ());

							_r_listview_redraw (app.GetHWND (), IDC_LISTVIEW);

							_wfp_create4filters (ptr_rule, false);
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
					const LONG_PTR dialog_id = GetWindowLongPtr (hwnd, GWLP_USERDATA);

					if (dialog_id != IDD_SETTINGS_RULES_CUSTOM)
						break;

					const size_t item = (size_t)SendDlgItemMessage (hwnd, IDC_EDITOR, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);

					if (item == LAST_VALUE)
						break;

					const size_t idx = (size_t)_r_listview_getitemlparam (hwnd, IDC_EDITOR, item);

					PITEM_RULE ptr_rule = rules_custom.at (idx);

					if (ptr_rule)
					{
						if (DialogBoxParam (nullptr, MAKEINTRESOURCE (IDD_EDITOR), hwnd, &EditorProc, (LPARAM)ptr_rule))
						{
							SendMessage (hwnd, RM_LOCALIZE, dialog_id, (LPARAM)ptr_page);

							_app_listviewsort (app.GetHWND (), IDC_LISTVIEW, -1, false);
							_app_listviewsort_ab (hwnd, IDC_EDITOR);

							_app_profilesave (app.GetHWND ());

							_r_listview_redraw (app.GetHWND (), IDC_LISTVIEW);

							_wfp_create4filters (ptr_rule, false);

						}
					}

					break;
				}

				case IDM_DELETE:
				{
					const LONG_PTR dialog_id = GetWindowLongPtr (hwnd, GWLP_USERDATA);

					if (dialog_id != IDD_SETTINGS_RULES_CUSTOM)
						break;

					const size_t total_count = (size_t)SendDlgItemMessage (hwnd, IDC_EDITOR, LVM_GETSELECTEDCOUNT, 0, 0);

					if (!total_count)
						break;

					if (_r_msg (hwnd, MB_YESNO | MB_ICONEXCLAMATION | MB_TOPMOST, APP_NAME, nullptr, app.LocaleString (IDS_QUESTION_DELETE, nullptr), total_count) != IDYES)
						break;

					const size_t count = _r_listview_getitemcount (hwnd, IDC_EDITOR) - 1;

					_r_fastlock_acquireexclusive (&lock_apply);
					_r_fastlock_acquireexclusive (&lock_access);

					_wfp_transact_start (__LINE__);

					for (size_t i = count; i != LAST_VALUE; i--)
					{
						if (ListView_GetItemState (GetDlgItem (hwnd, IDC_EDITOR), i, LVNI_SELECTED))
						{
							const size_t idx = (size_t)_r_listview_getitemlparam (hwnd, IDC_EDITOR, i);

							PITEM_RULE ptr_rule = rules_custom.at (idx);

							if (ptr_rule)
								_wfp_destroy2filters (&ptr_rule->mfarr, true);

							SendDlgItemMessage (hwnd, IDC_EDITOR, LVM_DELETEITEM, i, 0);

							_app_freerule (&rules_custom.at (idx));
						}
					}


					_wfp_transact_commit (__LINE__);

					_r_fastlock_releaseexclusive (&lock_apply);
					_r_fastlock_releaseexclusive (&lock_access);

					_app_listviewsort (app.GetHWND (), IDC_LISTVIEW, -1, false);
					_app_profilesave (app.GetHWND ());

					_r_listview_redraw (app.GetHWND (), IDC_LISTVIEW);

					SendMessage (hwnd, RM_LOCALIZE, dialog_id, (LPARAM)ptr_page);

					break;
				}

				case IDM_SELECT_ALL:
				{
					ListView_SetItemState (GetDlgItem (hwnd, IDC_COLORS), -1, LVIS_SELECTED, LVIS_SELECTED);
					ListView_SetItemState (GetDlgItem (hwnd, IDC_EDITOR), -1, LVIS_SELECTED, LVIS_SELECTED);

					break;
				}

				case IDM_CHECK:
				case IDM_UNCHECK:
				{
					const LONG_PTR dialog_id = GetWindowLongPtr (hwnd, GWLP_USERDATA);

					if (dialog_id == IDD_SETTINGS_HIGHLIGHTING)
					{
						size_t item = LAST_VALUE;
						const bool new_val = (LOWORD (wparam) == IDM_CHECK) ? true : false;

						while ((item = (size_t)SendDlgItemMessage (hwnd, IDC_COLORS, LVM_GETNEXTITEM, item, LVNI_SELECTED)) != LAST_VALUE)
						{
							_r_listview_setitemcheck (hwnd, IDC_COLORS, item, new_val);
						}
					}
					else if (
						dialog_id == IDD_SETTINGS_RULES_BLOCKLIST ||
						dialog_id == IDD_SETTINGS_RULES_SYSTEM ||
						dialog_id == IDD_SETTINGS_RULES_CUSTOM
						)
					{
						std::vector<PITEM_RULE> const* ptr_rules = nullptr;

						if (dialog_id == IDD_SETTINGS_RULES_BLOCKLIST)
							ptr_rules = &rules_blocklist;

						else if (dialog_id == IDD_SETTINGS_RULES_SYSTEM)
							ptr_rules = &rules_system;

						else if (dialog_id == IDD_SETTINGS_RULES_CUSTOM)
							ptr_rules = &rules_custom;

						if (ptr_rules)
						{
							const bool new_val = (LOWORD (wparam) == IDM_CHECK) ? true : false;
							bool is_changed = false;

							size_t item = LAST_VALUE;

							_r_fastlock_acquireexclusive (&lock_apply);
							_r_fastlock_acquireexclusive (&lock_access);

							_wfp_transact_start (__LINE__);

							while ((item = (size_t)SendDlgItemMessage (hwnd, IDC_EDITOR, LVM_GETNEXTITEM, item, LVNI_SELECTED)) != LAST_VALUE)
							{
								const size_t idx = (size_t)_r_listview_getitemlparam (hwnd, IDC_EDITOR, item);

								PITEM_RULE ptr_rule = ptr_rules->at (idx);

								if (ptr_rule)
								{
									if (ptr_rule->is_enabled != new_val)
									{
										ptr_rule->is_enabled = new_val;
										ptr_rule->is_haveerrors = false;

										config.is_nocheckboxnotify = true;

										_r_listview_setitem (hwnd, IDC_EDITOR, item, 0, nullptr, LAST_VALUE, _app_getrulegroup (ptr_rule));
										_r_listview_setitemcheck (hwnd, IDC_EDITOR, item, new_val);

										config.is_nocheckboxnotify = false;

										if (dialog_id == IDD_SETTINGS_RULES_BLOCKLIST || dialog_id == IDD_SETTINGS_RULES_SYSTEM)
											rules_config[ptr_rule->pname] = new_val;

										_wfp_create4filters (ptr_rule, true);

										if (!is_changed)
											is_changed = true;
									}
								}
							}

							_wfp_transact_commit (__LINE__);

							_r_fastlock_releaseexclusive (&lock_apply);
							_r_fastlock_releaseexclusive (&lock_access);

							if (is_changed)
							{
								SendMessage (hwnd, RM_LOCALIZE, dialog_id, (LPARAM)ptr_page);

								_app_listviewsort (app.GetHWND (), IDC_LISTVIEW, -1, false);

								_app_profilesave (app.GetHWND ());
								_app_refreshstatus (app.GetHWND ());

								_r_listview_redraw (app.GetHWND (), IDC_LISTVIEW);
							}
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

	if (config.hnetevent)
	{
		const HMODULE hlib = LoadLibrary (L"fwpuclnt.dll");

		if (hlib)
		{
			typedef DWORD (WINAPI *FWPMNEU) (HANDLE, HANDLE); // FwpmNetEventUnsubscribe0

			const FWPMNEU _FwpmNetEventUnsubscribe = (FWPMNEU)GetProcAddress (hlib, "FwpmNetEventUnsubscribe0");

			if (_FwpmNetEventUnsubscribe)
			{
				const DWORD rc = _FwpmNetEventUnsubscribe (config.hengine, config.hnetevent);

				if (rc == ERROR_SUCCESS)
				{
					config.hnetevent = nullptr;
					result = true;
				}
			}

			FreeLibrary (hlib);
		}
	}

	return result;
}

bool _wfp_logsubscribe ()
{
	if (!config.hengine)
		return false;

	bool result = false;

	if (config.hnetevent)
	{
		result = true;
	}
	else
	{
		const HMODULE hlib = LoadLibrary (L"fwpuclnt.dll");

		if (!hlib)
		{
			_app_logerror (L"LoadLibrary", GetLastError (), L"fwpuclnt.dll", false);
		}
		else
		{
			typedef DWORD (WINAPI *FWPMNES5) (HANDLE, const FWPM_NET_EVENT_SUBSCRIPTION0*, FWPM_NET_EVENT_CALLBACK4, LPVOID, LPHANDLE); // win10new+
			typedef DWORD (WINAPI *FWPMNES4) (HANDLE, const FWPM_NET_EVENT_SUBSCRIPTION0*, FWPM_NET_EVENT_CALLBACK4, LPVOID, LPHANDLE); // win10rs5+
			typedef DWORD (WINAPI *FWPMNES3) (HANDLE, const FWPM_NET_EVENT_SUBSCRIPTION0*, FWPM_NET_EVENT_CALLBACK3, LPVOID, LPHANDLE); // win10rs4+
			typedef DWORD (WINAPI *FWPMNES2) (HANDLE, const FWPM_NET_EVENT_SUBSCRIPTION0*, FWPM_NET_EVENT_CALLBACK2, LPVOID, LPHANDLE); // win10+
			typedef DWORD (WINAPI *FWPMNES1) (HANDLE, const FWPM_NET_EVENT_SUBSCRIPTION0*, FWPM_NET_EVENT_CALLBACK1, LPVOID, LPHANDLE); // win8+
			typedef DWORD (WINAPI *FWPMNES0) (HANDLE, const FWPM_NET_EVENT_SUBSCRIPTION0*, FWPM_NET_EVENT_CALLBACK0, LPVOID, LPHANDLE); // win7+

			const FWPMNES5 _FwpmNetEventSubscribe5 = (FWPMNES5)GetProcAddress (hlib, "FwpmNetEventSubscribe5"); // win10new+
			const FWPMNES4 _FwpmNetEventSubscribe4 = (FWPMNES4)GetProcAddress (hlib, "FwpmNetEventSubscribe4"); // win10rs5+
			const FWPMNES3 _FwpmNetEventSubscribe3 = (FWPMNES3)GetProcAddress (hlib, "FwpmNetEventSubscribe3"); // win10rs4+
			const FWPMNES2 _FwpmNetEventSubscribe2 = (FWPMNES2)GetProcAddress (hlib, "FwpmNetEventSubscribe2"); // win10+
			const FWPMNES1 _FwpmNetEventSubscribe1 = (FWPMNES1)GetProcAddress (hlib, "FwpmNetEventSubscribe1"); // win8+
			const FWPMNES0 _FwpmNetEventSubscribe0 = (FWPMNES0)GetProcAddress (hlib, "FwpmNetEventSubscribe0"); // win7+

			if (!_FwpmNetEventSubscribe5 && !_FwpmNetEventSubscribe4 && !_FwpmNetEventSubscribe3 && !_FwpmNetEventSubscribe2 && !_FwpmNetEventSubscribe1 && !_FwpmNetEventSubscribe0)
			{
				_app_logerror (L"GetProcAddress", GetLastError (), L"FwpmNetEventSubscribe", false);
			}
			else
			{
				FWPM_NET_EVENT_SUBSCRIPTION subscription;
				FWPM_NET_EVENT_ENUM_TEMPLATE enum_template;

				SecureZeroMemory (&subscription, sizeof (subscription));
				SecureZeroMemory (&enum_template, sizeof (enum_template));

				if (config.psession)
					CopyMemory (&subscription.sessionKey, config.psession, sizeof (GUID));

				subscription.enumTemplate = &enum_template;

				DWORD rc = 0;

				if (_FwpmNetEventSubscribe5)
					rc = _FwpmNetEventSubscribe5 (config.hengine, &subscription, &_wfp_logcallback4, nullptr, &config.hnetevent); // win10new+

				else if (_FwpmNetEventSubscribe4)
					rc = _FwpmNetEventSubscribe4 (config.hengine, &subscription, &_wfp_logcallback4, nullptr, &config.hnetevent); // win10rs5+

				else if (_FwpmNetEventSubscribe3)
					rc = _FwpmNetEventSubscribe3 (config.hengine, &subscription, &_wfp_logcallback3, nullptr, &config.hnetevent); // win10rs4+

				else if (_FwpmNetEventSubscribe2)
					rc = _FwpmNetEventSubscribe2 (config.hengine, &subscription, &_wfp_logcallback2, nullptr, &config.hnetevent); // win10+

				else if (_FwpmNetEventSubscribe1)
					rc = _FwpmNetEventSubscribe1 (config.hengine, &subscription, &_wfp_logcallback1, nullptr, &config.hnetevent); // win8+

				else if (_FwpmNetEventSubscribe0)
					rc = _FwpmNetEventSubscribe0 (config.hengine, &subscription, &_wfp_logcallback0, nullptr, &config.hnetevent); // win7+

				if (rc != ERROR_SUCCESS)
				{
					_app_logerror (L"FwpmNetEventSubscribe", rc, nullptr, false);
				}
				else
				{
					_app_loginit (true); // create log file
					result = true;
				}
			}

			FreeLibrary (hlib);
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
			config.psession = new GUID;

			if (config.psession)
			{
				if (CoCreateGuid (config.psession) != S_OK)
				{
					delete config.psession;
					config.psession = nullptr;
				}
			}
		}

		SecureZeroMemory (&session, sizeof (session));

		session.displayData.name = APP_NAME;
		session.displayData.description = APP_NAME;

		if (config.psession)
			CopyMemory (&session.sessionKey, config.psession, sizeof (GUID));

		rc = FwpmEngineOpen (nullptr, RPC_C_AUTHN_WINNT, nullptr, &session, &config.hengine);

		if (rc != ERROR_SUCCESS)
		{
			_app_logerror (L"FwpmEngineOpen", rc, nullptr, false);
			config.hengine = nullptr;
			result = false;
		}
	}

	// set security info
	if (is_full && config.hengine && !config.is_securityinfoset)
	{
		if (config.psid)
		{
			// Add DACL for given user
			PACL pDacl = nullptr;
			PSECURITY_DESCRIPTOR securityDescriptor = nullptr;

			rc = FwpmEngineGetSecurityInfo (config.hengine, DACL_SECURITY_INFORMATION, nullptr, nullptr, &pDacl, nullptr, &securityDescriptor);

			if (rc != ERROR_SUCCESS)
			{
				_app_logerror (L"FwpmEngineGetSecurityInfo", rc, nullptr, false);
			}
			else
			{
				bool bExists = false;

				// Loop through the ACEs and search for user SID.
				for (WORD cAce = 0; !bExists && cAce < pDacl->AceCount; cAce++)
				{
					ACCESS_ALLOWED_ACE* pAce = nullptr;

					// Get ACE info
					if (!GetAce (pDacl, cAce, (void**)&pAce))
					{
						_app_logerror (L"GetAce", GetLastError (), nullptr, false);
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

					ea.grfAccessPermissions = FWPM_GENERIC_ALL | DELETE | WRITE_DAC | WRITE_OWNER;
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
						_app_logerror (L"SetEntriesInAcl", rc, nullptr, false);
					}
					else
					{
						// set engine security information
						rc = FwpmEngineSetSecurityInfo (config.hengine, DACL_SECURITY_INFORMATION, nullptr, nullptr, pNewDacl, nullptr);

						if (rc != ERROR_SUCCESS)
						{
							_app_logerror (L"FwpmEngineSetSecurityInfo", rc, nullptr, true);
						}
						else
						{
							config.is_securityinfoset = true;

							// set net events security information
							rc = FwpmNetEventsSetSecurityInfo (config.hengine, DACL_SECURITY_INFORMATION, nullptr, nullptr, pNewDacl, nullptr);

							if (rc != ERROR_SUCCESS)
								_app_logerror (L"FwpmNetEventsSetSecurityInfo", rc, nullptr, true);
						}
					}

					if (pNewDacl)
						LocalFree (pNewDacl);
				}
			}
		}
	}

	if (is_full && config.hengine && !config.is_providerset)
	{
		const bool is_intransact = _wfp_transact_start (__LINE__);

		// create provider
		FWPM_PROVIDER provider = {0};

		provider.displayData.name = APP_NAME;
		provider.displayData.description = APP_NAME;

		provider.providerKey = GUID_WfpProvider;
		provider.flags = FWPM_PROVIDER_FLAG_PERSISTENT;

		rc = FwpmProviderAdd (config.hengine, &provider, nullptr);

		if (rc != ERROR_SUCCESS && rc != FWP_E_ALREADY_EXISTS)
		{
			if (is_intransact)
				FwpmTransactionAbort (config.hengine);

			_app_logerror (L"FwpmProviderAdd", rc, nullptr, false);
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
			sublayer.weight = (UINT16)app.ConfigGet (L"SublayerWeight", SUBLAYER_WEIGHT_DEFAULT).AsUint (); // highest weight for UINT16

			rc = FwpmSubLayerAdd (config.hengine, &sublayer, nullptr);

			if (rc != ERROR_SUCCESS && rc != FWP_E_ALREADY_EXISTS)
			{
				if (is_intransact)
					FwpmTransactionAbort (config.hengine);

				_app_logerror (L"FwpmSubLayerAdd", rc, nullptr, false);
				result = false;
			}
			else
			{
				if (is_intransact)
				{
					if (_wfp_transact_commit (__LINE__))
						result = true;
				}
				else
				{
					result = true;
				}

				config.is_providerset = true;
			}
		}
	}

	// dropped packets logging (win7+)
	if (is_full && config.hengine && !config.is_neteventset && _r_sys_validversion (6, 1))
	{
		FWP_VALUE val;

		val.type = FWP_UINT32;
		val.uint32 = 1;

		rc = FwpmEngineSetOption (config.hengine, FWPM_ENGINE_COLLECT_NET_EVENTS, &val);

		if (rc != ERROR_SUCCESS)
		{
			_app_logerror (L"FwpmEngineSetOption", rc, L"FWPM_ENGINE_COLLECT_NET_EVENTS", false);
		}
		else
		{
			// configure dropped packets logging (win8+)
			if (_r_sys_validversion (6, 2))
			{
				// the filter engine will collect wfp network events that match any supplied key words
				val.type = FWP_UINT32;
				val.uint32 = FWPM_NET_EVENT_KEYWORD_CLASSIFY_ALLOW | FWPM_NET_EVENT_KEYWORD_INBOUND_MCAST | FWPM_NET_EVENT_KEYWORD_INBOUND_BCAST;

				rc = FwpmEngineSetOption (config.hengine, FWPM_ENGINE_NET_EVENT_MATCH_ANY_KEYWORDS, &val);

				if (rc != ERROR_SUCCESS)
					_app_logerror (L"FwpmEngineSetOption", rc, L"FWPM_ENGINE_NET_EVENT_MATCH_ANY_KEYWORDS", false);

				// enables the connection monitoring feature and starts logging creation and deletion events (and notifying any subscribers)
				val.type = FWP_UINT32;
				val.uint32 = 1;

				rc = FwpmEngineSetOption (config.hengine, FWPM_ENGINE_MONITOR_IPSEC_CONNECTIONS, &val);

				if (rc != ERROR_SUCCESS)
					_app_logerror (L"FwpmEngineSetOption", rc, L"FWPM_ENGINE_MONITOR_IPSEC_CONNECTIONS", false);
			}

			config.is_neteventset = true;
		}
	}

	return result;
}

void _wfp_uninitialize (bool is_full)
{
	if (config.psession)
	{
		delete config.psession;
		config.psession = nullptr;
	}

	config.is_securityinfoset = false;

	if (!config.hengine)
		return;

	DWORD result;

	// dropped packets logging (win7+)
	if (config.is_neteventset && _r_sys_validversion (6, 1))
	{
		_wfp_logunsubscribe ();

		FWP_VALUE val;

		// collect ipsec connection (win8+)
		if (_r_sys_validversion (6, 2))
		{
			val.type = FWP_UINT32;
			val.uint32 = 0;

			FwpmEngineSetOption (config.hengine, FWPM_ENGINE_MONITOR_IPSEC_CONNECTIONS, &val);
		}

		val.type = FWP_UINT32;
		val.uint32 = 0;

		result = FwpmEngineSetOption (config.hengine, FWPM_ENGINE_COLLECT_NET_EVENTS, &val);

		if (result != ERROR_SUCCESS)
			_app_logerror (L"FwpmEngineSetOption", result, L"FWPM_ENGINE_COLLECT_NET_EVENTS", false);

		config.is_neteventset = false;
	}

	if (is_full)
	{
		const bool is_intransact = _wfp_transact_start (__LINE__);

		// destroy callouts (deprecated)
		{
			static const GUID callouts[] = {
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
			_app_logerror (L"FwpmSubLayerDeleteByKey", result, nullptr, false);

		// destroy provider
		result = FwpmProviderDeleteByKey (config.hengine, &GUID_WfpProvider);

		if (result != ERROR_SUCCESS && result != FWP_E_PROVIDER_NOT_FOUND)
			_app_logerror (L"FwpmProviderDeleteByKey", result, nullptr, false);

		if (is_intransact)
			_wfp_transact_commit (__LINE__);

		config.is_providerset = false;
	}

	FwpmEngineClose (config.hengine);
	config.hengine = nullptr;
}

void DrawFrameBorder (HDC hdc, HWND hwnd, COLORREF clr)
{
	RECT rc = {0};
	GetWindowRect (hwnd, &rc);

	const HPEN hpen = CreatePen (PS_INSIDEFRAME, GetSystemMetrics (SM_CXBORDER), clr);

	const HPEN old_pen = (HPEN)SelectObject (hdc, hpen);
	const HBRUSH old_brush = (HBRUSH)SelectObject (hdc, GetStockObject (NULL_BRUSH));

	Rectangle (hdc, 0, 0, (rc.right - rc.left), (rc.bottom - rc.top));

	SelectObject (hdc, old_pen);
	SelectObject (hdc, old_brush);

	DeleteObject (hpen);
}

LRESULT CALLBACK NotificationProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
		case WM_CLOSE:
		{
			_app_notifyhide (hwnd);

			SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
			return TRUE;
		}

		case WM_MOUSEMOVE:
		{
			if (!config.is_notifytimeout)
				_app_notifysettimeout (hwnd, 0, false, 0);

			break;
		}

		case WM_ACTIVATE:
		{
			switch (LOWORD (wparam))
			{
				case WA_INACTIVE:
				{
					if (!config.is_notifytimeout && !_r_wnd_undercursor (hwnd))
						_app_notifyhide (hwnd);

					break;
				}
			}

			break;
		}

		case WM_TIMER:
		{
			if (config.is_notifytimeout && wparam != NOTIFY_TIMER_TIMEOUT_ID)
				return FALSE;

			if (wparam == NOTIFY_TIMER_TIMEOUT_ID)
			{
				if (_r_wnd_undercursor (hwnd))
				{
					_app_notifysettimeout (hwnd, wparam, false, 0);
					return FALSE;
				}
			}

			if (wparam == NOTIFY_TIMER_POPUP_ID || wparam == NOTIFY_TIMER_TIMEOUT_ID)
				_app_notifyhide (hwnd);

			break;
		}

		case WM_KEYDOWN:
		{
			switch (wparam)
			{
				case VK_ESCAPE:
				{
					_app_notifyhide (hwnd);
					break;
				}
			}

			break;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps = {0};
			const HDC hdc = BeginPaint (hwnd, &ps);

			RECT rc = {0};
			GetClientRect (hwnd, &rc);

			static const INT bottom = app.GetDPI (48);

			rc.left = GetSystemMetrics (SM_CXBORDER);
			rc.right -= (rc.left * 2);
			rc.top = rc.bottom - bottom;
			rc.bottom = rc.top + bottom;

			_r_dc_fillrect (hdc, &rc, GetSysColor (COLOR_BTNFACE));

			for (INT i = 0; i < rc.right; i++)
				SetPixel (hdc, i, rc.top, GetSysColor (COLOR_APPWORKSPACE));

			DrawFrameBorder (hdc, hwnd, NOTIFY_BORDER_COLOR);

			EndPaint (hwnd, &ps);

			break;
		}

		case WM_CTLCOLORBTN:
		case WM_CTLCOLOREDIT:
		case WM_CTLCOLORSTATIC:
		{
			const UINT ctrl_id = GetDlgCtrlID ((HWND)lparam);

			if (
				ctrl_id == IDC_ICON_ID ||
				ctrl_id == IDC_TITLE_ID ||
				ctrl_id == IDC_MUTE_BTN ||
				ctrl_id == IDC_CLOSE_BTN ||
				ctrl_id == IDC_FILE_ID ||
				ctrl_id == IDC_ADDRESS_REMOTE_ID ||
				ctrl_id == IDC_ADDRESS_LOCAL_ID ||
				ctrl_id == IDC_FILTER_ID ||
				ctrl_id == IDC_DATE_ID ||
				ctrl_id == IDC_CREATERULE_ADDR_ID ||
				ctrl_id == IDC_CREATERULE_PORT_ID ||
				ctrl_id == IDC_DISABLENOTIFICATION_ID
				)
			{
				SetBkMode ((HDC)wparam, TRANSPARENT); // background-hack
				SetTextColor ((HDC)wparam, _r_dc_getcolorbrightness (GetSysColor (COLOR_WINDOW)));

				return (INT_PTR)GetSysColorBrush (COLOR_WINDOW);
			}

			break;
		}

		case WM_SETCURSOR:
		{
			const UINT ctrl_id = GetDlgCtrlID ((HWND)wparam);

			if (ctrl_id == IDC_MUTE_BTN || ctrl_id == IDC_CLOSE_BTN)
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
				case BCN_DROPDOWN:
				{
					if (nmlp->idFrom != IDC_ALLOW_BTN)
						break;

					const HMENU menu = CreateMenu ();
					const HMENU submenu = CreateMenu ();

					AppendMenu (menu, MF_POPUP, (UINT_PTR)submenu, L" ");

					for (UINT i = 0; i < timers.size (); i++)
						AppendMenu (submenu, MF_BYPOSITION, IDX_TIMER_NOTIFY + i, _r_fmt_interval (timers.at (i) + 1, 1));

					RECT buttonRect = {0};

					GetClientRect (nmlp->hwndFrom, &buttonRect);
					ClientToScreen (nmlp->hwndFrom, (LPPOINT)&buttonRect);

					TrackPopupMenuEx (submenu, TPM_RIGHTBUTTON | TPM_LEFTBUTTON, buttonRect.left, buttonRect.top, hwnd, nullptr);

					DestroyMenu (submenu);
					DestroyMenu (menu);

					break;
				}

				case TTN_GETDISPINFO:
				{
					LPNMTTDISPINFO lpnmdi = (LPNMTTDISPINFO)lparam;

					if ((lpnmdi->uFlags & TTF_IDISHWND) != 0)
					{
						WCHAR buffer[1024] = {0};
						const UINT ctrl_id = GetDlgCtrlID ((HWND)lpnmdi->hdr.idFrom);

						if (
							ctrl_id == IDC_MUTE_BTN ||
							ctrl_id == IDC_CLOSE_BTN ||
							ctrl_id == IDC_FILE_ID ||
							ctrl_id == IDC_ADDRESS_REMOTE_ID ||
							ctrl_id == IDC_ADDRESS_LOCAL_ID ||
							ctrl_id == IDC_FILTER_ID ||
							ctrl_id == IDC_DATE_ID
							)
						{
							_r_fastlock_acquireshared (&lock_notification);

							const size_t idx = _app_notifygetcurrent ();

							if (idx != LAST_VALUE)
							{
								PITEM_LOG const ptr_log = notifications.at (idx);

								if (ptr_log)
								{
									if (ctrl_id == IDC_MUTE_BTN)
										StringCchCopy (buffer, _countof (buffer), app.LocaleString (IDS_NOTIFY_DISABLENOTIFICATIONS, nullptr));

									else if (ctrl_id == IDC_CLOSE_BTN)
										StringCchCopy (buffer, _countof (buffer), app.LocaleString (IDS_CLOSE, nullptr));

									else if (ctrl_id == IDC_FILE_ID)
										StringCchCopy (buffer, _countof (buffer), _app_gettooltip (ptr_log->hash));

									else
										StringCchCopy (buffer, _countof (buffer), _r_ctrl_gettext (hwnd, ctrl_id));

									lpnmdi->lpszText = buffer;
								}
							}

							_r_fastlock_releaseshared (&lock_notification);
						}
						else if (ctrl_id == IDC_CREATERULE_ADDR_ID || ctrl_id == IDC_CREATERULE_PORT_ID)
						{
							StringCchCopy (buffer, _countof (buffer), app.LocaleString (IDS_NOTIFY_TOOLTIP, nullptr));

							lpnmdi->lpszText = buffer;
						}
					}

					break;
				}

				case NM_CLICK:
				case NM_RETURN:
				{
					PNMLINK nmlink = (PNMLINK)lparam;

					if (nmlink->item.szUrl[0])
					{
						if (nmlp->idFrom == IDC_FILE_ID)
						{
							_r_fastlock_acquireshared (&lock_notification);

							const size_t idx = _app_notifygetcurrent ();

							if (idx != LAST_VALUE)
							{
								ShowItem (app.GetHWND (), IDC_LISTVIEW, _app_getposition (app.GetHWND (), notifications.at (idx)->hash), -1);

								_r_wnd_toggle (app.GetHWND (), true);
							}

							_r_fastlock_releaseshared (&lock_notification);

							_app_notifyhide (hwnd);
						}
					}

					break;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			if ((LOWORD (wparam) >= IDX_TIMER_NOTIFY && LOWORD (wparam) <= IDX_TIMER_NOTIFY + timers.size ()))
			{
				const size_t idx = (LOWORD (wparam) - IDX_TIMER_NOTIFY);

				_app_notifycommand (hwnd, CmdAllow, idx);

				return FALSE;
			}

			switch (LOWORD (wparam))
			{
				case IDC_ALLOW_BTN:
				case IDC_BLOCK_BTN:
				{
					_app_notifycommand (hwnd, LOWORD (wparam) == IDC_ALLOW_BTN ? CmdAllow : CmdBlock, LAST_VALUE);
					break;
				}

				case IDC_MUTE_BTN:
				{
					_app_notifycommand (hwnd, CmdMute, LAST_VALUE);
					break;
				}

				case IDC_PREV_ID:
				{
					_r_fastlock_acquireshared (&lock_notification);

					const size_t current_idx = _app_notifygetcurrent ();

					if (current_idx != LAST_VALUE)
					{
						const size_t total_size = notifications.size ();

						if (!current_idx)
						{
							_app_notifyshow (total_size - 1, true);
						}
						else
						{
							const size_t idx = (size_t)current_idx - 1;

							if (idx >= 0 && idx <= (total_size - 1))
								_app_notifyshow (idx, true);
						}
					}

					_r_fastlock_releaseshared (&lock_notification);

					break;
				}

				case IDC_NEXT_ID:
				{
					_r_fastlock_acquireshared (&lock_notification);

					const size_t current_idx = _app_notifygetcurrent ();

					if (current_idx != LAST_VALUE)
					{
						const size_t total_size = notifications.size ();

						if (current_idx >= (total_size - 1))
						{
							_app_notifyshow (0, true);
						}
						else
						{
							const size_t idx = (size_t)current_idx + 1;

							if (idx >= 0 && idx <= (total_size - 1))
								_app_notifyshow (idx, true);
						}
					}

					_r_fastlock_releaseshared (&lock_notification);

					break;
				}

				case IDC_CLOSE_BTN:
				{
					_app_notifyhide (hwnd);
					break;
				}
			}

			break;
		}
	}

	return DefWindowProc (hwnd, msg, wparam, lparam);
}

void _app_generate_addmenu (HMENU submenu)
{
	constexpr auto uproc_id = 2;
	constexpr auto  upckg_id = 3;
	constexpr auto  usvc_id = 4;

	const HMENU submenu_process = GetSubMenu (submenu, uproc_id);
	const HMENU submenu_package = GetSubMenu (submenu, upckg_id);
	const HMENU submenu_service = GetSubMenu (submenu, usvc_id);

	_app_generate_processes ();

	app.LocaleMenu (submenu, IDS_ADD_FILE, IDM_ADD_FILE, false, L"...");
	app.LocaleMenu (submenu, IDS_ADD_PROCESS, uproc_id, true, nullptr);
	app.LocaleMenu (submenu, IDS_ADD_PACKAGE, upckg_id, true, _r_sys_validversion (6, 2) ? nullptr : L" [win8+]");
	app.LocaleMenu (submenu, IDS_ADD_SERVICE, usvc_id, true, nullptr);
	app.LocaleMenu (submenu, IDS_ALL, IDM_ALL_PROCESSES, false, _r_fmt (L" (%zu)", processes.size ()));
	app.LocaleMenu (submenu, IDS_ALL, IDM_ALL_PACKAGES, false, _r_fmt (L" (%zu)", packages.size ()));
	app.LocaleMenu (submenu, IDS_ALL, IDM_ALL_SERVICES, false, _r_fmt (L" (%zu)", services.size ()));

	// generate processes popup menu
	{
		if (processes.empty ())
		{
			MENUITEMINFO mii = {0};

			WCHAR buffer[128] = {0};
			StringCchCopy (buffer, _countof (buffer), app.LocaleString (IDS_STATUS_EMPTY, nullptr));

			mii.cbSize = sizeof (mii);
			mii.fMask = MIIM_STATE | MIIM_FTYPE | MIIM_STRING;
			mii.fType = MFT_STRING;
			mii.dwTypeData = buffer;
			mii.fState = MF_DISABLED | MF_GRAYED;

			SetMenuItemInfo (submenu_process, IDM_ALL_PROCESSES, FALSE, &mii);
		}
		else
		{
			AppendMenu (submenu_process, MF_SEPARATOR, 0, nullptr);

			for (size_t i = 0; i < processes.size (); i++)
			{
				MENUITEMINFO mii = {0};

				mii.cbSize = sizeof (mii);
				mii.fMask = MIIM_ID | MIIM_FTYPE | MIIM_BITMAP | MIIM_STRING;
				mii.fType = MFT_STRING;
				mii.dwTypeData = processes.at (i).display_name;
				mii.hbmpItem = processes.at (i).hbmp;
				mii.wID = IDX_PROCESS + UINT (i);

				InsertMenuItem (submenu_process, mii.wID, FALSE, &mii);
			}
		}
	}

	// generate packages popup menu (win8+)
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
					AppendMenu (submenu_package, MF_SEPARATOR, 1, nullptr);

				MENUITEMINFO mii = {0};

				mii.cbSize = sizeof (mii);
				mii.fMask = MIIM_ID | MIIM_FTYPE | MIIM_BITMAP | MIIM_STRING;
				mii.fType = MFT_STRING;
				mii.dwTypeData = packages.at (i).display_name;
				mii.hbmpItem = config.hbitmap_package_small;
				mii.wID = IDX_PACKAGE + UINT (i);

				InsertMenuItem (submenu_package, mii.wID, FALSE, &mii);
				total_added += 1;
			}
		}

		if (!total_added)
		{
			MENUITEMINFO mii = {0};

			WCHAR buffer[128] = {0};
			StringCchCopy (buffer, _countof (buffer), app.LocaleString (IDS_STATUS_EMPTY, nullptr));

			mii.cbSize = sizeof (mii);
			mii.fMask = MIIM_STATE | MIIM_FTYPE | MIIM_STRING;
			mii.fType = MFT_STRING;
			mii.dwTypeData = buffer;
			mii.fState = MF_DISABLED | MF_GRAYED;

			SetMenuItemInfo (submenu_package, IDM_ALL_PACKAGES, FALSE, &mii);
		}
	}
	else
	{
		EnableMenuItem (submenu, upckg_id, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);
	}

	{
		size_t total_added = 0;

		if (!services.empty ())
		{
			for (size_t i = 0; i < services.size (); i++)
			{
				if (apps.find (services.at (i).hash) != apps.end ())
					continue;

				if (!total_added)
					AppendMenu (submenu_service, MF_SEPARATOR, 1, nullptr);

				MENUITEMINFO mii = {0};

				mii.cbSize = sizeof (mii);
				mii.fMask = MIIM_ID | MIIM_FTYPE | MIIM_BITMAP | MIIM_STRING;
				mii.fType = MFT_STRING;
				mii.dwTypeData = services.at (i).service_name;
				mii.hbmpItem = config.hbitmap_service_small;
				mii.wID = IDX_SERVICE + UINT (i);

				InsertMenuItem (submenu_service, mii.wID, FALSE, &mii);
				total_added += 1;
			}
		}

		if (!total_added)
		{
			MENUITEMINFO mii = {0};

			WCHAR buffer[128] = {0};
			StringCchCopy (buffer, _countof (buffer), app.LocaleString (IDS_STATUS_EMPTY, nullptr));

			mii.cbSize = sizeof (mii);
			mii.fMask = MIIM_STATE | MIIM_FTYPE | MIIM_STRING;
			mii.fType = MFT_STRING;
			mii.dwTypeData = buffer;
			mii.fState = MF_DISABLED | MF_GRAYED;

			SetMenuItemInfo (submenu_service, IDM_ALL_SERVICES, FALSE, &mii);
		}
	}
}

INT_PTR CALLBACK DlgProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
		case WM_INITDIALOG:
		{
			// static initializer
			config.wd_length = GetWindowsDirectory (config.windows_dir, _countof (config.windows_dir));
			config.tmp1_length = GetTempPath (_countof (config.tmp1_dir), config.tmp1_dir);
			GetLongPathName (rstring (config.tmp1_dir), config.tmp1_dir, _countof (config.tmp1_dir));

			StringCchPrintf (config.apps_path, _countof (config.apps_path), L"%s\\" XML_APPS, app.GetProfileDirectory ());
			StringCchPrintf (config.rules_blocklist_path, _countof (config.rules_blocklist_path), L"%s\\" XML_BLOCKLIST, app.GetProfileDirectory ());
			StringCchPrintf (config.rules_system_path, _countof (config.rules_system_path), L"%s\\" XML_RULES_SYSTEM, app.GetProfileDirectory ());
			StringCchPrintf (config.rules_custom_path, _countof (config.rules_custom_path), L"%s\\" XML_RULES_CUSTOM, app.GetProfileDirectory ());
			StringCchPrintf (config.rules_config_path, _countof (config.rules_config_path), L"%s\\" XML_RULES_CONFIG, app.GetProfileDirectory ());

			StringCchPrintf (config.apps_path_backup, _countof (config.apps_path_backup), L"%s\\" XML_APPS L".bak", app.GetProfileDirectory ());
			StringCchPrintf (config.rules_config_path_backup, _countof (config.rules_config_path_backup), L"%s\\" XML_RULES_CONFIG L".bak", app.GetProfileDirectory ());
			StringCchPrintf (config.rules_custom_path_backup, _countof (config.rules_custom_path_backup), L"%s\\" XML_RULES_CUSTOM L".bak", app.GetProfileDirectory ());

			config.ntoskrnl_hash = _r_str_hash (PROC_SYSTEM_NAME);
			config.myhash = _r_str_hash (app.GetBinaryPath ());

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

			// set process priority
			SetPriorityClass (GetCurrentProcess (), ABOVE_NORMAL_PRIORITY_CLASS);

			// initialize spinlocks
			_r_fastlock_initialize (&lock_apply);
			_r_fastlock_initialize (&lock_access);
			_r_fastlock_initialize (&lock_writelog);
			_r_fastlock_initialize (&lock_notification);
			_r_fastlock_initialize (&lock_threadpool);
			_r_fastlock_initialize (&lock_eventcallback);

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
						token_user = new TOKEN_USER[token_length];

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
									StringCchPrintf (config.title, _countof (config.title), L"%s [%s\\%s]", APP_NAME, domain, username);

								config.psid = new BYTE[SECURITY_MAX_SID_SIZE];

								if (config.psid)
									CopyMemory (config.psid, token_user->User.Sid, SECURITY_MAX_SID_SIZE);
							}

							delete[] token_user;
						}
					}

					CloseHandle (token);
				}

				if (!config.title[0])
					StringCchCopy (config.title, _countof (config.title), APP_NAME); // fallback
			}

			// init buffered paint
			BufferedPaintInit ();

			// allow drag&drop support
			DragAcceptFiles (hwnd, TRUE);

			// configure listview
			_r_listview_setstyle (hwnd, IDC_LISTVIEW, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_CHECKBOXES | LVS_EX_HEADERINALLVIEWS);

			_r_listview_addcolumn (hwnd, IDC_LISTVIEW, 0, app.LocaleString (IDS_FILEPATH, nullptr), 70, LVCFMT_LEFT);
			_r_listview_addcolumn (hwnd, IDC_LISTVIEW, 1, app.LocaleString (IDS_ADDED, nullptr), 26, LVCFMT_LEFT);

			_r_listview_addgroup (hwnd, IDC_LISTVIEW, 0, app.LocaleString (IDS_GROUP_ALLOWED, nullptr), 0, LVGS_COLLAPSIBLE | (app.ConfigGet (L"Group1IsCollaped", false).AsBool () ? LVGS_COLLAPSED : LVGS_NORMAL));
			_r_listview_addgroup (hwnd, IDC_LISTVIEW, 1, app.LocaleString (IDS_GROUP_SPECIAL_APPS, nullptr), 0, LVGS_COLLAPSIBLE | (app.ConfigGet (L"Group2IsCollaped", false).AsBool () ? LVGS_COLLAPSED : LVGS_NORMAL));
			_r_listview_addgroup (hwnd, IDC_LISTVIEW, 2, app.LocaleString (IDS_GROUP_BLOCKED, nullptr), 0, LVGS_COLLAPSIBLE | (app.ConfigGet (L"Group3IsCollaped", false).AsBool () ? LVGS_COLLAPSED : LVGS_NORMAL));

			_app_listviewsetimagelist (hwnd, IDC_LISTVIEW);
			_app_listviewsetfont (hwnd, IDC_LISTVIEW, true);

			_app_setbuttonmargins (hwnd, IDC_START_BTN);

			// load settings imagelist
			{
				const INT cx_width = GetSystemMetrics (SM_CXSMICON);

				config.himg = ImageList_Create (cx_width, cx_width, ILC_COLOR32 | ILC_MASK, 0, 5);

				ImageList_ReplaceIcon (config.himg, -1, app.GetSharedIcon (app.GetHINSTANCE (), IDI_ALLOW, cx_width));
				ImageList_ReplaceIcon (config.himg, -1, app.GetSharedIcon (app.GetHINSTANCE (), IDI_BLOCK, cx_width));
			}

			// get default icon for executable
			_app_getfileicon (_r_path_expand (PATH_NTOSKRNL), false, &config.icon_id, &config.hicon_large);
			_app_getfileicon (_r_path_expand (PATH_NTOSKRNL), true, &config.icon_id, &config.hicon_small);

			// get default icon for services
			if (_app_getfileicon (_r_path_expand (PATH_SERVICES), true, &config.icon_service_id, &config.hicon_service_small))
			{
				config.hbitmap_service_small = _app_ico2bmp (config.hicon_service_small);
			}
			else
			{
				config.hicon_service_small = config.hicon_small;
				config.hbitmap_service_small = _app_ico2bmp (config.hicon_small);
			}

			// get default icon for windows store package (win8+)
			if (_r_sys_validversion (6, 2))
			{
				if (_app_getfileicon (_r_path_expand (PATH_WINSTORE), true, &config.icon_package_id, &config.hicon_package))
				{
					config.hbitmap_package_small = _app_ico2bmp (config.hicon_package);
				}
				else
				{
					config.icon_package_id = config.icon_id;
					config.hicon_package = config.hicon_small;
					config.hbitmap_package_small = _app_ico2bmp (config.hicon_small);
				}
			}

			// initialize settings
			app.SettingsAddPage (IDD_SETTINGS_GENERAL, IDS_SETTINGS_GENERAL);
			app.SettingsAddPage (IDD_SETTINGS_INTERFACE, IDS_TITLE_INTERFACE);
			app.SettingsAddPage (IDD_SETTINGS_HIGHLIGHTING, IDS_TITLE_HIGHLIGHTING);

			{
				const size_t page_id = app.SettingsAddPage (IDD_SETTINGS_RULES, IDS_TRAY_RULES);

				app.SettingsAddPage (IDD_SETTINGS_RULES_BLOCKLIST, IDS_TRAY_BLOCKLIST_RULES, page_id);
				app.SettingsAddPage (IDD_SETTINGS_RULES_SYSTEM, IDS_TRAY_SYSTEM_RULES, page_id);
				app.SettingsAddPage (IDD_SETTINGS_RULES_CUSTOM, IDS_TRAY_USER_RULES, page_id);
			}

			// dropped packets logging (win7+)
			if (_r_sys_validversion (6, 1))
				app.SettingsAddPage (IDD_SETTINGS_LOG, IDS_TRAY_LOG);

			// initialize timers
			{
				config.htimer = CreateTimerQueue ();

				timers.push_back (_R_SECONDSCLOCK_MIN (10));
				timers.push_back (_R_SECONDSCLOCK_MIN (20));
				timers.push_back (_R_SECONDSCLOCK_MIN (30));
				timers.push_back (_R_SECONDSCLOCK_HOUR (1));
				timers.push_back (_R_SECONDSCLOCK_HOUR (2));
				timers.push_back (_R_SECONDSCLOCK_HOUR (4));
				timers.push_back (_R_SECONDSCLOCK_HOUR (6));
			}

			// initialize colors
			{
				addcolor (IDS_HIGHLIGHT_TIMER, L"IsHighlightTimer", true, L"ColorTimer", LISTVIEW_COLOR_TIMER);
				addcolor (IDS_HIGHLIGHT_INVALID, L"IsHighlightInvalid", true, L"ColorInvalid", LISTVIEW_COLOR_INVALID);
				addcolor (IDS_HIGHLIGHT_SPECIAL, L"IsHighlightSpecial", true, L"ColorSpecial", LISTVIEW_COLOR_SPECIAL);
				addcolor (IDS_HIGHLIGHT_SILENT, L"IsHighlightSilent", true, L"ColorSilent", LISTVIEW_COLOR_SILENT);
				addcolor (IDS_HIGHLIGHT_SIGNED, L"IsHighlightSigned", true, L"ColorSigned", LISTVIEW_COLOR_SIGNED);
				addcolor (IDS_HIGHLIGHT_SERVICE, L"IsHighlightService", true, L"ColorService", LISTVIEW_COLOR_SERVICE);
				addcolor (IDS_HIGHLIGHT_PACKAGE, L"IsHighlightPackage", true, L"ColorPackage", LISTVIEW_COLOR_PACKAGE);
				addcolor (IDS_HIGHLIGHT_PICO, L"IsHighlightPico", true, L"ColorPico", LISTVIEW_COLOR_PICO);
				addcolor (IDS_HIGHLIGHT_SYSTEM, L"IsHighlightSystem", true, L"ColorSystem", LISTVIEW_COLOR_SYSTEM);
			}

			// initialize protocols
			{
				addprotocol (L"tcp", IPPROTO_TCP);
				addprotocol (L"udp", IPPROTO_UDP);
				addprotocol (L"icmp", IPPROTO_ICMP);
				addprotocol (L"ipv6-icmp", IPPROTO_ICMPV6);
				addprotocol (L"ipv4", IPPROTO_IPV4);
				addprotocol (L"ipv6", IPPROTO_IPV6);
				addprotocol (L"igmp", IPPROTO_IGMP);
				addprotocol (L"l2tp", IPPROTO_L2TP);
				addprotocol (L"sctp", IPPROTO_SCTP);
				addprotocol (L"rdp", IPPROTO_RDP);
				addprotocol (L"raw", IPPROTO_RAW);
			}

			// initialize thread objects
			config.done_evt = CreateEventEx (nullptr, nullptr, 0, EVENT_ALL_ACCESS);

			// initialize dropped packets log callback thread (win7+)
			if (_r_sys_validversion (6, 1))
			{
				// create notification window
				_app_notifycreatewindow ();

				// initialize slist
				{
					log_stack.Count = 0;
					InitializeSListHead (&log_stack.ListHead);
				}
			}

			// initialize winsock (required by getnameinfo)
			{
				WSADATA wsaData = {0};

				if (WSAStartup (WINSOCK_VERSION, &wsaData) == ERROR_SUCCESS)
					config.is_wsainit = true;
			}

			// load profile
			_app_profileload (hwnd);
			_app_listviewsort (hwnd, IDC_LISTVIEW, -1, false);

			// add blocklist to update
			app.UpdateAddComponent (L"Blocklist", L"blocklist", _r_fmt (L"%I64u", config.blocklist_timestamp), config.rules_blocklist_path, false);
			app.UpdateAddComponent (L"System rules", L"rules_system", _r_fmt (L"%I64u", config.rule_system_timestamp), config.rules_system_path, false);

			// install filters
			if (_wfp_isfiltersinstalled ())
				_app_changefilters (hwnd, true, true);

			break;
		}

		case RM_INITIALIZE:
		{
			if (app.ConfigGet (L"ShowTitleID", true).AsBool ())
				SetWindowText (hwnd, config.title);

			else
				SetWindowText (hwnd, APP_NAME);

			const bool state = _wfp_isfiltersinstalled ();

			// set icons
			SendMessage (hwnd, WM_SETICON, ICON_SMALL, (LPARAM)app.GetSharedIcon (app.GetHINSTANCE (), state ? IDI_ACTIVE : IDI_INACTIVE, GetSystemMetrics (SM_CXSMICON)));
			SendMessage (hwnd, WM_SETICON, ICON_BIG, (LPARAM)app.GetSharedIcon (app.GetHINSTANCE (), state ? IDI_ACTIVE : IDI_INACTIVE, GetSystemMetrics (SM_CXICON)));

			SendDlgItemMessage (hwnd, IDC_START_BTN, BM_SETIMAGE, IMAGE_ICON, (WPARAM)app.GetSharedIcon (app.GetHINSTANCE (), state ? IDI_INACTIVE : IDI_ACTIVE, GetSystemMetrics (SM_CXSMICON)));

			app.TrayCreate (hwnd, UID, nullptr, WM_TRAYICON, app.GetSharedIcon (app.GetHINSTANCE (), state ? IDI_ACTIVE : IDI_INACTIVE, GetSystemMetrics (SM_CXSMICON)), false);
			SetDlgItemText (hwnd, IDC_START_BTN, app.LocaleString (state ? IDS_TRAY_STOP : IDS_TRAY_START, nullptr));

			CheckMenuItem (GetMenu (hwnd), IDM_ALWAYSONTOP_CHK, MF_BYCOMMAND | (app.ConfigGet (L"AlwaysOnTop", false).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (GetMenu (hwnd), IDM_SHOWFILENAMESONLY_CHK, MF_BYCOMMAND | (app.ConfigGet (L"ShowFilenames", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (GetMenu (hwnd), IDM_AUTOSIZECOLUMNS_CHK, MF_BYCOMMAND | (app.ConfigGet (L"AutoSizeColumns", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (GetMenu (hwnd), IDM_ENABLESPECIALGROUP_CHK, MF_BYCOMMAND | (app.ConfigGet (L"IsEnableSpecialGroup", false).AsBool () ? MF_CHECKED : MF_UNCHECKED));

			CheckMenuRadioItem (GetMenu (hwnd), IDM_TRAY_MODEWHITELIST, IDM_TRAY_MODEBLACKLIST, IDM_TRAY_MODEWHITELIST + app.ConfigGet (L"Mode", ModeWhitelist).AsUint (), MF_BYCOMMAND);

			CheckMenuItem (GetMenu (hwnd), IDM_ENABLELOG_CHK, MF_BYCOMMAND | (app.ConfigGet (L"IsLogEnabled", false).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (GetMenu (hwnd), IDM_ENABLENOTIFICATIONS_CHK, MF_BYCOMMAND | (app.ConfigGet (L"IsNotificationsEnabled", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));

			// dropped packets logging (win7+)
			if (!_r_sys_validversion (6, 1))
			{
				EnableMenuItem (GetMenu (hwnd), IDM_ENABLELOG_CHK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
				EnableMenuItem (GetMenu (hwnd), IDM_ENABLENOTIFICATIONS_CHK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
			}

			break;
		}

		case RM_LOCALIZE:
		{
			const HMENU menu = GetMenu (hwnd);

			app.LocaleMenu (menu, IDS_FILE, 0, true, nullptr);
			app.LocaleMenu (GetSubMenu (menu, 0), IDS_EXPORT, 0, true, nullptr);
			app.LocaleMenu (GetSubMenu (menu, 0), IDS_IMPORT, 1, true, nullptr);
			app.LocaleMenu (menu, IDS_EXPORT, IDM_EXPORT_APPS, false, L" " XML_APPS L"\tCtrl+S");
			app.LocaleMenu (menu, IDS_EXPORT, IDM_EXPORT_RULES, false, L" " XML_RULES_CUSTOM L"\tCtrl+Shift+S");
			app.LocaleMenu (menu, IDS_IMPORT, IDM_IMPORT_APPS, false, L" " XML_APPS L"\tCtrl+O");
			app.LocaleMenu (menu, IDS_IMPORT, IDM_IMPORT_RULES, false, L" " XML_RULES_CUSTOM L"\tCtrl+Shift+O");
			app.LocaleMenu (menu, IDS_EXIT, IDM_EXIT, false, L"\tAlt+F4");

			app.LocaleMenu (menu, IDS_EDIT, 1, true, nullptr);

			app.LocaleMenu (menu, IDS_PURGE_UNUSED, IDM_PURGE_UNUSED, false, L"\tCtrl+Shift+X");
			app.LocaleMenu (menu, IDS_PURGE_ERRORS, IDM_PURGE_ERRORS, false, L"\tCtrl+Shift+E");
			app.LocaleMenu (menu, IDS_PURGE_TIMERS, IDM_PURGE_TIMERS, false, L"\tCtrl+Shift+T");

			app.LocaleMenu (menu, IDS_REFRESH, IDM_REFRESH, false, L"\tF5");

			app.LocaleMenu (menu, IDS_VIEW, 2, true, nullptr);

			app.LocaleMenu (menu, IDS_ALWAYSONTOP_CHK, IDM_ALWAYSONTOP_CHK, false, nullptr);
			app.LocaleMenu (menu, IDS_SHOWFILENAMESONLY_CHK, IDM_SHOWFILENAMESONLY_CHK, false, nullptr);
			app.LocaleMenu (menu, IDS_AUTOSIZECOLUMNS_CHK, IDM_AUTOSIZECOLUMNS_CHK, false, nullptr);
			app.LocaleMenu (menu, IDS_ENABLESPECIALGROUP_CHK, IDM_ENABLESPECIALGROUP_CHK, false, nullptr);

			app.LocaleMenu (GetSubMenu (menu, 2), IDS_ICONS, 5, true, nullptr);
			app.LocaleMenu (menu, IDS_ICONSSMALL, IDM_ICONSSMALL, false, nullptr);
			app.LocaleMenu (menu, IDS_ICONSLARGE, IDM_ICONSLARGE, false, nullptr);
			app.LocaleMenu (menu, IDS_ICONSEXTRALARGE, IDM_ICONSEXTRALARGE, false, nullptr);
			app.LocaleMenu (menu, IDS_ICONSISHIDDEN, IDM_ICONSISHIDDEN, false, nullptr);

			app.LocaleMenu (GetSubMenu (menu, 2), IDS_LANGUAGE, LANG_MENU, true, L" (Language)");

			app.LocaleMenu (menu, IDS_FONT, IDM_FONT, false, L"...");

			app.LocaleMenu (menu, IDS_SETTINGS, 3, true, nullptr);

			app.LocaleMenu (GetSubMenu (menu, 3), IDS_TRAY_MODE, 0, true, nullptr);

			app.LocaleMenu (menu, IDS_MODE_WHITELIST, IDM_TRAY_MODEWHITELIST, false, nullptr);
			app.LocaleMenu (menu, IDS_MODE_BLACKLIST, IDM_TRAY_MODEBLACKLIST, false, nullptr);

			app.LocaleMenu (GetSubMenu (menu, 3), IDS_TRAY_LOG, 1, true, nullptr);

			app.LocaleMenu (menu, IDS_ENABLELOG_CHK, IDM_ENABLELOG_CHK, false, nullptr);
			app.LocaleMenu (menu, IDS_ENABLENOTIFICATIONS_CHK, IDM_ENABLENOTIFICATIONS_CHK, false, nullptr);
			app.LocaleMenu (menu, IDS_LOGSHOW, IDM_LOGSHOW, false, L"\tCtrl+I");
			app.LocaleMenu (menu, IDS_LOGCLEAR, IDM_LOGCLEAR, false, L"\tCtrl+X");

			app.LocaleMenu (menu, IDS_SETTINGS, IDM_SETTINGS, false, L"...\tF2");

			app.LocaleMenu (menu, IDS_HELP, 4, true, nullptr);
			app.LocaleMenu (menu, IDS_WEBSITE, IDM_WEBSITE, false, nullptr);
			app.LocaleMenu (menu, IDS_CHECKUPDATES, IDM_CHECKUPDATES, false, nullptr);
			app.LocaleMenu (menu, IDS_ABOUT, IDM_ABOUT, false, L"\tF1");

			app.LocaleEnum ((HWND)GetSubMenu (menu, 2), LANG_MENU, true, IDX_LANGUAGE); // enum localizations

			{
				const bool state = _wfp_isfiltersinstalled ();
				SetDlgItemText (hwnd, IDC_START_BTN, app.LocaleString (_wfp_isfiltersinstalled () ? IDS_TRAY_STOP : IDS_TRAY_START, nullptr));
			}

			SetDlgItemText (hwnd, IDC_SETTINGS_BTN, app.LocaleString (IDS_SETTINGS, nullptr));
			SetDlgItemText (hwnd, IDC_EXIT_BTN, app.LocaleString (IDS_EXIT, nullptr));

			_r_wnd_addstyle (hwnd, IDC_START_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (hwnd, IDC_SETTINGS_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (hwnd, IDC_EXIT_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

			_r_wnd_addstyle (config.hnotification, IDC_NEXT_ID, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (config.hnotification, IDC_PREV_ID, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

			_r_wnd_addstyle (config.hnotification, IDC_ALLOW_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (config.hnotification, IDC_BLOCK_BTN, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

			_r_listview_setcolumn (hwnd, IDC_LISTVIEW, 0, app.LocaleString (IDS_FILEPATH, nullptr), 0);
			_r_listview_setcolumn (hwnd, IDC_LISTVIEW, 1, app.LocaleString (IDS_ADDED, nullptr), 0);

			SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_RESETEMPTYTEXT, 0, 0);

			_app_notifyrefresh ();
			_app_refreshstatus (hwnd);

			break;
		}

		case RM_UNINITIALIZE:
		{
			app.TrayDestroy (hwnd, UID, nullptr);
			break;
		}

		case RM_UPDATE_DONE:
		{
			_r_fastlock_acquireexclusive (&lock_access);

			_app_loadrules (hwnd, config.rules_system_path, MAKEINTRESOURCE (IDR_RULES_SYSTEM), true, &rules_system, FILTER_WEIGHT_SYSTEM, &config.rule_system_timestamp); // reload system rules
			_app_loadrules (hwnd, config.rules_blocklist_path, MAKEINTRESOURCE (IDR_RULES_BLOCKLIST), true, &rules_blocklist, FILTER_WEIGHT_BLOCKLIST, &config.blocklist_timestamp); // reload blocklist

			_r_fastlock_releaseexclusive (&lock_access);

			_app_changefilters (hwnd, true, false);

			break;
		}

		case RM_RESET_DONE:
		{
			rules_config.clear ();

			_r_fs_delete (config.rules_config_path, false);
			_r_fs_delete (config.rules_config_path_backup, false);

			_r_fastlock_acquireexclusive (&lock_access);

			_app_loadrules (hwnd, config.rules_system_path, MAKEINTRESOURCE (IDR_RULES_SYSTEM), true, &rules_system, FILTER_WEIGHT_SYSTEM, &config.rule_system_timestamp); // reload system rules
			_app_loadrules (hwnd, config.rules_blocklist_path, MAKEINTRESOURCE (IDR_RULES_BLOCKLIST), true, &rules_blocklist, FILTER_WEIGHT_BLOCKLIST, &config.blocklist_timestamp); // reload blocklist

			_r_fastlock_releaseexclusive (&lock_access);

			_app_profilesave (hwnd);

			_app_changefilters (hwnd, true, false);

			break;
		}

		case WM_DROPFILES:
		{
			UINT numfiles = DragQueryFile ((HDROP)wparam, UINT32_MAX, nullptr, 0);
			size_t item = 0;

			_r_fastlock_acquireexclusive (&lock_access);

			for (UINT i = 0; i < numfiles; i++)
			{
				const UINT length = DragQueryFile ((HDROP)wparam, i, nullptr, 0) + 1;

				LPWSTR file = new WCHAR[length];

				if (file)
				{
					DragQueryFile ((HDROP)wparam, i, file, length);

					item = _app_addapplication (hwnd, file, 0, 0, 0, false, false, false);

					delete[] file;
				}
			}

			_r_fastlock_releaseexclusive (&lock_access);

			DragFinish ((HDROP)wparam);

			_app_listviewsort (hwnd, IDC_LISTVIEW, -1, false);
			_app_profilesave (hwnd);

			ShowItem (hwnd, IDC_LISTVIEW, _app_getposition (hwnd, item), -1);

			break;
		}

		case WM_CLOSE:
		{
			if (_wfp_isfiltersinstalled ())
			{
				if (_app_istimersactive () ?
					!app.ConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION_TIMER, nullptr), L"ConfirmExitTimer") :
					!app.ConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION_EXIT, nullptr), L"ConfirmExit2"))
				{
					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
					return TRUE;
				}
			}

			DestroyWindow (hwnd);

			break;
		}

		case WM_DESTROY:
		{
			app.TrayDestroy (hwnd, UID, nullptr);

			app.ConfigSet (L"Group1IsCollaped", ((SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_GETGROUPSTATE, 0, LVGS_COLLAPSED) & LVGS_COLLAPSED) != 0) ? true : false);
			app.ConfigSet (L"Group2IsCollaped", ((SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_GETGROUPSTATE, 1, LVGS_COLLAPSED) & LVGS_COLLAPSED) != 0) ? true : false);
			app.ConfigSet (L"Group3IsCollaped", ((SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_GETGROUPSTATE, 2, LVGS_COLLAPSED) & LVGS_COLLAPSED) != 0) ? true : false);

			_app_profilesave (hwnd);

			if (config.done_evt)
			{
				if (_r_fastlock_islocked (&lock_apply))
					WaitForSingleObjectEx (config.done_evt, FILTERS_TIMEOUT, FALSE);

				CloseHandle (config.done_evt);
			}

			if (_r_sys_validversion (6, 1))
				_app_clear_logstack ();

			DeleteTimerQueue (config.htimer);

			_wfp_uninitialize (false);

			DestroyWindow (config.hnotification);
			UnregisterClass (NOTIFY_CLASS_DLG, app.GetHINSTANCE ());

			BufferedPaintUnInit ();

			ImageList_Destroy (config.himg);

			PostQuitMessage (0);

			break;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps = {0};
			const HDC hdc = BeginPaint (hwnd, &ps);

			RECT rc = {0};
			GetWindowRect (GetDlgItem (hwnd, IDC_LISTVIEW), &rc);

			for (INT i = 0; i < rc.right; i++)
				SetPixel (hdc, i, rc.bottom - rc.top, GetSysColor (COLOR_APPWORKSPACE));

			EndPaint (hwnd, &ps);

			break;
		}

		case WM_SETTINGCHANGE:
		{
			_app_notifyrefresh ();
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

					const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, (INT)lpnmlv->hdr.idFrom, lpnmlv->iItem);

					if (hash)
						StringCchCopy (lpnmlv->pszText, lpnmlv->cchTextMax, _app_gettooltip (hash));

					break;
				}

				case LVN_ITEMCHANGED:
				{
					LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lparam;

					if (lpnmlv->uNewState == 8192 || lpnmlv->uNewState == 4096)
					{
						if (config.is_nocheckboxnotify)
							return FALSE;

						_r_fastlock_acquireexclusive (&lock_access);
						_r_fastlock_acquireexclusive (&lock_apply);

						_wfp_transact_start (__LINE__);

						const size_t hash = lpnmlv->lParam;
						PITEM_APP ptr_app = _app_getapplication (hash);

						if (ptr_app)
						{
							ptr_app->is_enabled = (lpnmlv->uNewState == 8192) ? true : false;

							_r_fastlock_acquireexclusive (&lock_notification);
							_app_freenotify (hash, false);
							_r_fastlock_releaseexclusive (&lock_notification);

							if ((lpnmlv->uNewState == 4096) && ptr_app->htimer)
								_app_timer_remove (hwnd, hash, false);

							_app_notifyrefresh ();
							_app_listviewsort (hwnd, IDC_LISTVIEW, -1, false);

							_app_profilesave (hwnd);

							_wfp_create3filters (ptr_app, true);
						}

						_wfp_transact_commit (__LINE__);

						_r_fastlock_releaseexclusive (&lock_access);
						_r_fastlock_releaseexclusive (&lock_apply);

						_r_listview_redraw (hwnd, IDC_LISTVIEW);
					}
					else if (((lpnmlv->uNewState ^ lpnmlv->uOldState) & LVIS_SELECTED) != 0)
					{
						_app_refreshstatus (hwnd);
					}

					break;
				}

				case LVN_GETEMPTYMARKUP:
				{
					NMLVEMPTYMARKUP* lpnmlv = (NMLVEMPTYMARKUP*)lparam;

					lpnmlv->dwFlags = EMF_CENTERED;
					StringCchCopy (lpnmlv->szMarkup, _countof (lpnmlv->szMarkup), app.LocaleString (IDS_STATUS_EMPTY, nullptr));

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

		case WM_CONTEXTMENU:
		{
			if (GetDlgCtrlID ((HWND)wparam) == IDC_LISTVIEW)
			{
				const bool is_filtersinstalled = _wfp_isfiltersinstalled ();

				const UINT uaddmenu_id = 0;
				const UINT usettings_id = 2;
				const UINT utimer_id = 3;
				const UINT selected_count = (UINT)SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_GETSELECTEDCOUNT, 0, 0);

				const HMENU menu = LoadMenu (nullptr, MAKEINTRESOURCE (IDM_LISTVIEW));
				const HMENU submenu = GetSubMenu (menu, 0);
				const HMENU submenu_add = GetSubMenu (submenu, uaddmenu_id);
				const HMENU submenu_settings = GetSubMenu (submenu, usettings_id);
				const HMENU submenu_timer = GetSubMenu (submenu, utimer_id);

				// localize
				app.LocaleMenu (submenu, IDS_ADD, uaddmenu_id, true, nullptr);
				app.LocaleMenu (submenu, IDS_TRAY_RULES, usettings_id, true, nullptr);
				app.LocaleMenu (submenu, IDS_DISABLENOTIFICATIONS, IDM_DISABLENOTIFICATIONS, false, nullptr);
				app.LocaleMenu (submenu, IDS_TIMER, utimer_id, true, nullptr);
				app.LocaleMenu (submenu, IDS_DISABLETIMER, IDM_DISABLETIMER, false, nullptr);
				app.LocaleMenu (submenu, IDS_REFRESH, IDM_REFRESH2, false, L"\tF5");
				app.LocaleMenu (submenu, IDS_EXPLORE, IDM_EXPLORE, false, L"\tCtrl+E");
				app.LocaleMenu (submenu, IDS_COPY, IDM_COPY, false, L"\tCtrl+C");
				app.LocaleMenu (submenu, IDS_DELETE, IDM_DELETE, false, L"\tDel");
				app.LocaleMenu (submenu, IDS_CHECK, IDM_CHECK, false, nullptr);
				app.LocaleMenu (submenu, IDS_UNCHECK, IDM_UNCHECK, false, nullptr);
				app.LocaleMenu (submenu, IDS_PROPERTIES, IDM_PROPERTIES, false, L"\tEnter");

				if (!selected_count)
				{
					EnableMenuItem (submenu, usettings_id, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);
					EnableMenuItem (submenu, utimer_id, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);
					EnableMenuItem (submenu, IDM_EXPLORE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					EnableMenuItem (submenu, IDM_COPY, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					EnableMenuItem (submenu, IDM_DELETE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					EnableMenuItem (submenu, IDM_CHECK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					EnableMenuItem (submenu, IDM_UNCHECK, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
					EnableMenuItem (submenu, IDM_PROPERTIES, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
				}

				_app_generate_addmenu (submenu_add);

				// show configuration
				if (selected_count)
				{
					const size_t item = (size_t)SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED); // get first item
					const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, IDC_LISTVIEW, item);

					_r_fastlock_acquireshared (&lock_access);

					PITEM_APP const ptr_app = _app_getapplication (hash);

					if (ptr_app)
					{
						CheckMenuItem (submenu, IDM_DISABLENOTIFICATIONS, MF_BYCOMMAND | (ptr_app->is_silent ? MF_CHECKED : MF_UNCHECKED));

						AppendMenu (submenu_settings, MF_SEPARATOR, 0, nullptr);

						if (rules_custom.empty ())
						{
							AppendMenu (submenu_settings, MF_STRING, IDX_RULES_SPECIAL, app.LocaleString (IDS_STATUS_EMPTY, nullptr));
							EnableMenuItem (submenu_settings, IDX_RULES_SPECIAL, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
						}
						else
						{
							for (UINT8 loop = 0; loop < 2; loop++)
							{
								for (size_t i = 0; i < rules_custom.size (); i++)
								{
									PITEM_RULE const ptr_rule = rules_custom.at (i);

									if (ptr_rule)
									{
										const bool is_checked = (ptr_rule->is_enabled && (ptr_rule->apps.find (hash) != ptr_rule->apps.end ()));

										if ((loop == 0 && !is_checked) || (loop == 1 && is_checked))
											continue;

										WCHAR buffer[128] = {0};
										StringCchPrintf (buffer, _countof (buffer), app.LocaleString (IDS_RULE_APPLY, nullptr), app.LocaleString (ptr_rule->is_block ? IDS_ACTION_BLOCK : IDS_ACTION_ALLOW, nullptr).GetString (), ptr_rule->pname);

										if (selected_count > 1)
											StringCchCat (buffer, _countof (buffer), _r_fmt (L" (%d)", selected_count));

										MENUITEMINFO mii = {0};

										mii.cbSize = sizeof (mii);
										mii.fMask = MIIM_ID | MIIM_FTYPE | MIIM_STATE | MIIM_STRING;
										mii.fType = MFT_STRING;
										mii.dwTypeData = buffer;
										mii.fState = (is_checked ? MF_CHECKED : MF_UNCHECKED);
										mii.wID = IDX_RULES_SPECIAL + UINT (i);

										if (!is_filtersinstalled)
											mii.fState |= MF_DISABLED | MF_GRAYED;

										InsertMenuItem (submenu_settings, mii.wID, FALSE, &mii);
									}
								}
							}
						}

						AppendMenu (submenu_settings, MF_SEPARATOR, 0, nullptr);
						AppendMenu (submenu_settings, MF_STRING, IDM_OPENRULESEDITOR, app.LocaleString (IDS_OPENRULESEDITOR, L"..."));
					}

					// show timers
					bool is_checked = false;
					const time_t current_time = _r_unixtime_now ();

					for (size_t i = 0; i < timers.size (); i++)
					{
						MENUITEMINFO mii = {0};

						WCHAR buffer[128] = {0};
						StringCchCopy (buffer, _countof (buffer), _r_fmt_interval (timers.at (i) + 1, 1));

						mii.cbSize = sizeof (mii);
						mii.fMask = MIIM_ID | MIIM_FTYPE | MIIM_STATE | MIIM_STRING;
						mii.fType = MFT_STRING;
						mii.dwTypeData = buffer;
						mii.fState = MF_ENABLED;
						mii.wID = IDX_TIMER + UINT (i);

						if (!is_filtersinstalled)
							mii.fState = MF_DISABLED | MF_GRAYED;

						InsertMenuItem (submenu_timer, mii.wID, FALSE, &mii);

						if (!is_checked && ptr_app->timer > current_time && ptr_app->timer <= (current_time + timers.at (i)))
						{
							CheckMenuRadioItem (submenu_timer, IDX_TIMER, IDX_TIMER + UINT (timers.size ()), mii.wID, MF_BYCOMMAND);
							is_checked = true;
						}
					}

					if (!is_checked)
						CheckMenuRadioItem (submenu_timer, IDM_DISABLETIMER, IDM_DISABLETIMER, IDM_DISABLETIMER, MF_BYCOMMAND);

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
			RedrawWindow (hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE);

			_app_listviewresize (hwnd, IDC_LISTVIEW);
			_app_refreshstatus (hwnd);

			break;
		}

		case WM_TRAYICON:
		{
			switch (LOWORD (lparam))
			{
				case NIN_POPUPOPEN:
				{
					if (_app_notifyshow (_app_notifygetcurrent (), true))
						_app_notifysettimeout (config.hnotification, NOTIFY_TIMER_POPUP_ID, false, 0);

					break;
				}

				case NIN_POPUPCLOSE:
				{
					_app_notifysettimeout (config.hnotification, NOTIFY_TIMER_POPUP_ID, true, NOTIFY_TIMER_POPUP);
					break;
				}

				case NIN_BALLOONUSERCLICK:
				{
					if (config.is_popuperrors)
					{
						PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDM_TRAY_LOGSHOW_ERR, 0), 0);
						config.is_popuperrors = false;
					}

					break;
				}

				case NIN_BALLOONHIDE:
				case NIN_BALLOONTIMEOUT:
				{
					config.is_popuperrors = false;
					break;
				}

				case WM_MBUTTONUP:
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

					constexpr auto mode_id = 3;
					constexpr auto add_id = 5;
					constexpr auto delete_id = 6;
					constexpr auto notifications_id = 8;
					constexpr auto errlog_id = 9;

					const HMENU menu = LoadMenu (nullptr, MAKEINTRESOURCE (IDM_TRAY));
					const HMENU submenu = GetSubMenu (menu, 0);

					// localize
					app.LocaleMenu (submenu, IDS_TRAY_SHOW, IDM_TRAY_SHOW, false, nullptr);
					app.LocaleMenu (submenu, _wfp_isfiltersinstalled () ? IDS_TRAY_STOP : IDS_TRAY_START, IDM_TRAY_START, false, L"...");
					app.LocaleMenu (submenu, IDS_TRAY_MODE, mode_id, true, nullptr);
					app.LocaleMenu (submenu, IDS_MODE_WHITELIST, IDM_TRAY_MODEWHITELIST, false, nullptr);
					app.LocaleMenu (submenu, IDS_MODE_BLACKLIST, IDM_TRAY_MODEBLACKLIST, false, nullptr);

					app.LocaleMenu (submenu, IDS_ADD, add_id, true, nullptr);

					_app_generate_addmenu (GetSubMenu (submenu, add_id));

					{
						_r_fastlock_acquireshared (&lock_access);

						ITEM_STATUS itemStat = {0};
						_app_getcount (&itemStat);

						_r_fastlock_releaseshared (&lock_access);

						const size_t total_count = itemStat.unused_count + itemStat.invalid_count + itemStat.timers_count;

						app.LocaleMenu (submenu, IDS_DELETE, delete_id, true, total_count ? _r_fmt (L" (%d)", total_count).GetString () : nullptr);

						if (!total_count)
						{
							EnableMenuItem (submenu, delete_id, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);
						}
						else
						{
							app.LocaleMenu (submenu, IDS_PURGE_UNUSED, IDM_PURGE_UNUSED, false, itemStat.unused_count ? _r_fmt (L" (%d)", itemStat.unused_count).GetString () : nullptr);
							app.LocaleMenu (submenu, IDS_PURGE_ERRORS, IDM_PURGE_ERRORS, false, itemStat.invalid_count ? _r_fmt (L" (%d)", itemStat.invalid_count).GetString () : nullptr);
							app.LocaleMenu (submenu, IDS_PURGE_TIMERS, IDM_PURGE_TIMERS, false, itemStat.timers_count ? _r_fmt (L" (%d)", itemStat.timers_count).GetString () : nullptr);

							if (!itemStat.unused_count)
								EnableMenuItem (submenu, IDM_PURGE_UNUSED, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

							if (!itemStat.invalid_count)
								EnableMenuItem (submenu, IDM_PURGE_ERRORS, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

							if (!itemStat.timers_count)
								EnableMenuItem (submenu, IDM_PURGE_TIMERS, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
						}
					}

					app.LocaleMenu (submenu, IDS_TRAY_LOG, notifications_id, true, nullptr);
					app.LocaleMenu (submenu, IDS_ENABLELOG_CHK, IDM_TRAY_ENABLELOG_CHK, false, nullptr);
					app.LocaleMenu (submenu, IDS_ENABLENOTIFICATIONS_CHK, IDM_TRAY_ENABLENOTIFICATIONS_CHK, false, nullptr);
					app.LocaleMenu (submenu, IDS_LOGSHOW, IDM_TRAY_LOGSHOW, false, nullptr);
					app.LocaleMenu (submenu, IDS_LOGCLEAR, IDM_TRAY_LOGCLEAR, false, nullptr);

					app.LocaleMenu (submenu, IDS_TRAY_LOGERR, errlog_id, true, nullptr);

					{
						const rstring path = _r_path_expand (app.ConfigGet (L"LogPath", LOG_PATH_DEFAULT));

						if (!_r_fs_exists (path))
						{
							EnableMenuItem (submenu, IDM_TRAY_LOGSHOW, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
							EnableMenuItem (submenu, IDM_TRAY_LOGCLEAR, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
						}
					}

					if (_r_fs_exists (_r_dbg_getpath (APP_NAME_SHORT)))
					{
						app.LocaleMenu (submenu, IDS_LOGSHOW, IDM_TRAY_LOGSHOW_ERR, false, nullptr);
						app.LocaleMenu (submenu, IDS_LOGCLEAR, IDM_TRAY_LOGCLEAR_ERR, false, nullptr);
					}
					else
					{
						EnableMenuItem (submenu, errlog_id, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);
					}

					app.LocaleMenu (submenu, IDS_SETTINGS, IDM_TRAY_SETTINGS, false, L"...");
					app.LocaleMenu (submenu, IDS_WEBSITE, IDM_TRAY_WEBSITE, false, nullptr);
					app.LocaleMenu (submenu, IDS_ABOUT, IDM_TRAY_ABOUT, false, nullptr);
					app.LocaleMenu (submenu, IDS_EXIT, IDM_TRAY_EXIT, false, nullptr);

					CheckMenuItem (submenu, IDM_TRAY_ENABLELOG_CHK, MF_BYCOMMAND | (app.ConfigGet (L"IsLogEnabled", false).AsBool () ? MF_CHECKED : MF_UNCHECKED));
					CheckMenuItem (submenu, IDM_TRAY_ENABLENOTIFICATIONS_CHK, MF_BYCOMMAND | (app.ConfigGet (L"IsNotificationsEnabled", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));

					if (_r_fastlock_islocked (&lock_apply))
						EnableMenuItem (submenu, IDM_TRAY_START, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

					// dropped packets logging (win7+)
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

		case WM_POWERBROADCAST:
		{
			switch (wparam)
			{
				case PBT_APMSUSPEND:
				{
					_r_fastlock_acquireexclusive (&lock_threadpool);

					_app_clear_logstack ();
					_app_clear_threadpool (&threads_pool);

					_r_fastlock_releaseexclusive (&lock_threadpool);

					_wfp_uninitialize (false);
					_app_profilesave (hwnd);

					break;
				}

				case PBT_APMRESUMECRITICAL:
				case PBT_APMRESUMESUSPEND:
				{
					app.ConfigInit ();

					_app_profileload (hwnd);

					if (_wfp_isfiltersinstalled ())
					{
						if (_wfp_initialize (true))
							_app_changefilters (hwnd, true, true);
					}
					else
					{
						_r_listview_redraw (hwnd, IDC_LISTVIEW);
					}

					break;
				}
			}

			SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
			return TRUE;
		}

		case WM_DEVICECHANGE:
		{
			if (!app.ConfigGet (L"IsRefreshDevices", false).AsBool ())
				return TRUE;

			switch (wparam)
			{
				case DBT_DEVICEARRIVAL:
				case DBT_DEVICEREMOVECOMPLETE:
				{
					const PDEV_BROADCAST_HDR lbhdr = (PDEV_BROADCAST_HDR)lparam;

					if (lbhdr && lbhdr->dbch_devicetype == DBT_DEVTYP_VOLUME)
					{
						if (wparam == DBT_DEVICEARRIVAL)
						{
							_app_profileload (hwnd);
							_app_changefilters (hwnd, true, false);
						}
						else if (wparam == DBT_DEVICEREMOVECOMPLETE)
						{
							if (IsWindowVisible (hwnd))
								_r_listview_redraw (hwnd, IDC_LISTVIEW);
						}
					}

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
					return TRUE;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			if (HIWORD (wparam) == 0 && LOWORD (wparam) >= IDX_LANGUAGE && LOWORD (wparam) <= IDX_LANGUAGE + app.LocaleGetCount ())
			{
				app.LocaleApplyFromMenu (GetSubMenu (GetSubMenu (GetMenu (hwnd), 2), LANG_MENU), LOWORD (wparam), IDX_LANGUAGE);
				return FALSE;
			}
			else if ((LOWORD (wparam) >= IDX_PROCESS && LOWORD (wparam) <= IDX_PROCESS + processes.size ()))
			{
				PITEM_ADD const ptr_proc = &processes.at (LOWORD (wparam) - IDX_PROCESS);

				_r_fastlock_acquireexclusive (&lock_access);

				const size_t hash = _app_addapplication (hwnd, ptr_proc->real_path, 0, 0, 0, false, false, true);

				_r_fastlock_releaseexclusive (&lock_access);

				_app_listviewsort (hwnd, IDC_LISTVIEW, -1, false);
				_app_profilesave (hwnd);

				ShowItem (hwnd, IDC_LISTVIEW, _app_getposition (hwnd, hash), -1);

				return FALSE;
			}
			else if ((LOWORD (wparam) >= IDX_PACKAGE && LOWORD (wparam) <= IDX_PACKAGE + packages.size ()))
			{
				PITEM_ADD const ptr_package = &packages.at (LOWORD (wparam) - IDX_PACKAGE);

				_r_fastlock_acquireexclusive (&lock_access);

				const size_t hash = _app_addapplication (hwnd, ptr_package->sid, 0, 0, 0, false, false, true);

				_r_fastlock_releaseexclusive (&lock_access);

				_app_listviewsort (hwnd, IDC_LISTVIEW, -1, false);
				_app_profilesave (hwnd);

				ShowItem (hwnd, IDC_LISTVIEW, _app_getposition (hwnd, hash), -1);

				return FALSE;
			}
			else if ((LOWORD (wparam) >= IDX_SERVICE && LOWORD (wparam) <= IDX_SERVICE + services.size ()))
			{
				PITEM_ADD const ptr_svc = &services.at (LOWORD (wparam) - IDX_SERVICE);

				_r_fastlock_acquireexclusive (&lock_access);

				const size_t hash = _app_addapplication (hwnd, ptr_svc->service_name, 0, 0, 0, false, false, true);

				_r_fastlock_releaseexclusive (&lock_access);

				_app_listviewsort (hwnd, IDC_LISTVIEW, -1, false);
				_app_profilesave (hwnd);

				ShowItem (hwnd, IDC_LISTVIEW, _app_getposition (hwnd, hash), -1);

				return FALSE;
			}
			else if ((LOWORD (wparam) >= IDX_RULES_SPECIAL && LOWORD (wparam) <= IDX_RULES_SPECIAL + rules_custom.size ()))
			{
				const size_t idx = (LOWORD (wparam) - IDX_RULES_SPECIAL);

				size_t item = LAST_VALUE;
				BOOL is_remove = (BOOL)-1;

				_r_fastlock_acquireexclusive (&lock_access);

				PITEM_RULE ptr_rule = rules_custom.at (idx);

				if (!ptr_rule)
					return FALSE;

				while ((item = (size_t)SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_GETNEXTITEM, item, LVNI_SELECTED)) != LAST_VALUE)
				{
					const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, IDC_LISTVIEW, item);

					PITEM_APP ptr_app = _app_getapplication (hash);

					if (ptr_app)
					{
						if (is_remove == (BOOL)-1)
							is_remove = (ptr_rule->is_enabled && !ptr_rule->apps.empty () && (ptr_rule->apps.find (hash) != ptr_rule->apps.end ()));

						if (is_remove)
						{
							ptr_rule->apps.erase (hash);

							if (ptr_rule->apps.empty ())
								ptr_rule->is_enabled = false;
						}
						else
						{
							ptr_rule->apps[hash] = true;
							ptr_rule->is_enabled = true;
						}
					}
				}

				_r_fastlock_releaseexclusive (&lock_access);

				_app_listviewsort (hwnd, IDC_LISTVIEW, -1, false);
				_app_profilesave (hwnd);

				_r_listview_redraw (hwnd, IDC_LISTVIEW);

				_wfp_create4filters (ptr_rule, false);

				return FALSE;
			}
			else if ((LOWORD (wparam) >= IDX_TIMER && LOWORD (wparam) <= IDX_TIMER + timers.size ()))
			{
				if (!SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_GETSELECTEDCOUNT, 0, 0))
					break;

				const size_t idx = (LOWORD (wparam) - IDX_TIMER);

				size_t item = LAST_VALUE;
				const time_t current_time = _r_unixtime_now ();

				_r_fastlock_acquireexclusive (&lock_access);
				_r_fastlock_acquireexclusive (&lock_apply);

				_wfp_transact_start (__LINE__);

				while ((item = (size_t)SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_GETNEXTITEM, item, LVNI_SELECTED)) != LAST_VALUE)
				{
					const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, IDC_LISTVIEW, item);
					_app_timer_create (hwnd, hash, timers.at (idx), true);
				}

				_wfp_transact_commit (__LINE__);

				_r_fastlock_releaseexclusive (&lock_apply);
				_r_fastlock_releaseexclusive (&lock_access);

				_app_listviewsort (hwnd, IDC_LISTVIEW, -1, false);
				_app_profilesave (hwnd);

				_r_listview_redraw (hwnd, IDC_LISTVIEW);

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
					app.CreateSettingsWindow (&SettingsProc);
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

				case IDM_CHECKUPDATES:
				{
					app.UpdateCheck (true);
					break;
				}

				case IDM_ABOUT:
				case IDM_TRAY_ABOUT:
				{
					app.CreateAboutWindow (hwnd);
					break;
				}

				case IDM_EXPORT_APPS:
				case IDM_EXPORT_RULES:
				{
					WCHAR path[MAX_PATH] = {0};
					StringCchCopy (path, _countof (path), ((LOWORD (wparam) == IDM_EXPORT_APPS) ? XML_APPS : XML_RULES_CUSTOM));

					WCHAR title[MAX_PATH] = {0};
					StringCchPrintf (title, _countof (title), L"%s %s...", app.LocaleString (IDS_EXPORT, nullptr).GetString (), path);

					OPENFILENAME ofn = {0};

					ofn.lStructSize = sizeof (ofn);
					ofn.hwndOwner = hwnd;
					ofn.lpstrFile = path;
					ofn.nMaxFile = _countof (path);
					ofn.lpstrFilter = L"*.xml\0*.xml\0\0";
					ofn.lpstrDefExt = L"xml";
					ofn.lpstrTitle = title;
					ofn.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

					if (GetSaveFileName (&ofn))
					{
						if (LOWORD (wparam) == IDM_EXPORT_APPS)
							_r_fs_copy (config.apps_path, path);

						else if (LOWORD (wparam) == IDM_EXPORT_RULES)
							_r_fs_copy (config.rules_custom_path, path);
					}

					break;
				}

				case IDM_IMPORT_APPS:
				case IDM_IMPORT_RULES:
				{
					WCHAR path[MAX_PATH] = {0};
					StringCchCopy (path, _countof (path), ((LOWORD (wparam) == IDM_IMPORT_APPS) ? XML_APPS : XML_RULES_CUSTOM));

					WCHAR title[MAX_PATH] = {0};
					StringCchPrintf (title, _countof (title), L"%s %s...", app.LocaleString (IDS_IMPORT, nullptr).GetString (), path);

					OPENFILENAME ofn = {0};

					ofn.lStructSize = sizeof (ofn);
					ofn.hwndOwner = hwnd;
					ofn.lpstrFile = path;
					ofn.nMaxFile = _countof (path);
					ofn.lpstrFilter = L"*.xml;*.xml.bak\0*.xml;*.xml.bak\0*.*\0*.*\0\0";
					ofn.lpstrDefExt = L"xml";
					ofn.lpstrTitle = title;
					ofn.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_PATHMUSTEXIST | OFN_FORCESHOWHIDDEN;

					if (GetOpenFileName (&ofn))
					{
						// make backup
						if (LOWORD (wparam) == IDM_IMPORT_APPS)
							_r_fs_copy (config.apps_path, config.apps_path_backup);

						else if (LOWORD (wparam) == IDM_IMPORT_RULES)
							_r_fs_copy (config.rules_custom_path, config.rules_custom_path_backup);

						_app_profileload (hwnd, ((LOWORD (wparam) == IDM_IMPORT_APPS) ? path : nullptr), ((LOWORD (wparam) == IDM_IMPORT_RULES) ? path : nullptr));

						_app_listviewsort (hwnd, IDC_LISTVIEW, -1, false);
						_app_profilesave (hwnd);
						_app_notifyrefresh ();

						_app_changefilters (hwnd, true, false);
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

					_r_fastlock_acquireexclusive (&lock_access);

					for (size_t i = 0; i < _r_listview_getitemcount (hwnd, IDC_LISTVIEW); i++)
					{
						const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, IDC_LISTVIEW, i);
						PITEM_APP ptr_app = _app_getapplication (hash);

						if (ptr_app)
						{
							rstring name;
							_app_getdisplayname (hash, ptr_app, name);

							StringCchCopy (ptr_app->display_name, _countof (ptr_app->display_name), name);

							_r_listview_setitem (hwnd, IDC_LISTVIEW, i, 0, ptr_app->display_name);
						}
					}

					_r_fastlock_releaseexclusive (&lock_access);

					_app_notifyrefresh ();
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

				case IDM_ENABLESPECIALGROUP_CHK:
				{
					const bool new_val = !app.ConfigGet (L"IsEnableSpecialGroup", false).AsBool ();

					CheckMenuItem (GetMenu (hwnd), IDM_ENABLESPECIALGROUP_CHK, MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));
					app.ConfigSet (L"IsEnableSpecialGroup", new_val);

					_app_listviewsort (hwnd, IDC_LISTVIEW, -1, false);

					break;
				}

				case IDM_ICONSSMALL:
				case IDM_ICONSLARGE:
				case IDM_ICONSEXTRALARGE:
				{
					app.ConfigSet (L"IsLargeIcons", (LOWORD (wparam) == IDM_ICONSLARGE) ? true : false);
					app.ConfigSet (L"IsBigView", (LOWORD (wparam) == IDM_ICONSEXTRALARGE) ? true : false);

					_app_listviewsetimagelist (hwnd, IDC_LISTVIEW);

					_r_listview_redraw (hwnd, IDC_LISTVIEW);

					break;
				}

				case IDM_ICONSISHIDDEN:
				{
					const bool new_val = !app.ConfigGet (L"IsIconsHidden", false).AsBool ();

					app.ConfigSet (L"IsIconsHidden", new_val);

					CheckMenuItem (GetMenu (hwnd), IDM_ICONSISHIDDEN, MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));

					_r_fastlock_acquireexclusive (&lock_access);

					for (size_t i = 0; i < _r_listview_getitemcount (hwnd, IDC_LISTVIEW); i++)
					{
						const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, IDC_LISTVIEW, i);
						PITEM_APP ptr_app = _app_getapplication (hash);

						if (ptr_app)
						{
							size_t icon_id;
							_app_getappicon (ptr_app, false, &icon_id, nullptr);

							_r_listview_setitem (hwnd, IDC_LISTVIEW, i, 0, nullptr, icon_id);
						}
					}

					_r_fastlock_releaseexclusive (&lock_access);

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
						app.ConfigSet (L"Font", lf.lfFaceName[0] ? _r_fmt (L"%s;%d;%d", lf.lfFaceName, _r_dc_fontheighttosize (lf.lfHeight), lf.lfWeight) : UI_FONT_DEFAULT);

						_app_listviewsetfont (hwnd, IDC_LISTVIEW, true);
					}

					break;
				}

				case IDM_TRAY_MODEWHITELIST:
				case IDM_TRAY_MODEBLACKLIST:
				{
					DWORD current_mode = ModeWhitelist;

					if (LOWORD (wparam) == IDM_TRAY_MODEBLACKLIST)
						current_mode = ModeBlacklist;

					if ((app.ConfigGet (L"Mode", ModeWhitelist).AsUint () == current_mode) || (_wfp_isfiltersinstalled () && _r_msg (hwnd, MB_YESNO | MB_ICONEXCLAMATION | MB_TOPMOST, APP_NAME, nullptr, app.LocaleString (IDS_QUESTION_MODE, nullptr), app.LocaleString ((current_mode == ModeWhitelist) ? IDS_MODE_WHITELIST : IDS_MODE_BLACKLIST, nullptr).GetString ()) != IDYES))
						break;

					app.ConfigSet (L"Mode", current_mode);

					CheckMenuRadioItem (GetMenu (hwnd), IDM_TRAY_MODEWHITELIST, IDM_TRAY_MODEBLACKLIST, IDM_TRAY_MODEWHITELIST + current_mode, MF_BYCOMMAND);

					_app_changefilters (hwnd, true, false);
					_app_refreshstatus (hwnd);

					break;
				}

				case IDM_REFRESH:
				case IDM_REFRESH2:
				{
					_app_profileload (hwnd);
					_app_profilesave (hwnd);

					_app_changefilters (hwnd, true, false);

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

					_app_notifyrefresh ();

					break;
				}

				case IDM_LOGSHOW:
				case IDM_TRAY_LOGSHOW:
				{
					rstring path = _r_path_expand (app.ConfigGet (L"LogPath", LOG_PATH_DEFAULT));

					if (!_r_fs_exists (path))
						return FALSE;

					_r_run (nullptr, _r_fmt (L"%s \"%s\"", _app_getlogviewer ().GetString (), path.GetString ()));

					break;
				}

				case IDM_LOGCLEAR:
				case IDM_TRAY_LOGCLEAR:
				{
					const rstring path = _r_path_expand (app.ConfigGet (L"LogPath", LOG_PATH_DEFAULT));

					if ((config.hlogfile != nullptr && config.hlogfile != INVALID_HANDLE_VALUE) || _r_fs_exists (path))
					{
						if (!app.ConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION, nullptr), L"ConfirmLogClear"))
							break;

						_app_logclear ();
					}

					break;
				}

				case IDM_TRAY_LOGSHOW_ERR:
				{
					const rstring path = _r_dbg_getpath (APP_NAME_SHORT);

					if (!_r_fs_exists (path))
						return FALSE;

					_r_run (nullptr, _r_fmt (L"%s \"%s\"", _app_getlogviewer ().GetString (), path.GetString ()));

					break;
				}

				case IDM_TRAY_LOGCLEAR_ERR:
				{
					const rstring path = _r_dbg_getpath (APP_NAME_SHORT);

					if (!_r_fs_exists (path) || !app.ConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION, nullptr), L"ConfirmLogClear"))
						break;

					_r_fs_delete (path, false);

					break;
				}

				case IDM_TRAY_START:
				case IDC_START_BTN:
				{
					const bool state = !_wfp_isfiltersinstalled ();

					if (_app_installmessage (hwnd, state))
						_app_changefilters (hwnd, state, true);

					break;
				}

				case IDM_ADD_FILE:
				{
					WCHAR files[_R_BUFFER_LENGTH] = {0};
					OPENFILENAME ofn = {0};

					ofn.lStructSize = sizeof (ofn);
					ofn.hwndOwner = hwnd;
					ofn.lpstrFile = files;
					ofn.nMaxFile = _countof (files);
					ofn.lpstrFilter = L"*.exe\0*.exe\0*.*\0*.*\0\0";
					ofn.Flags = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_ENABLESIZING | OFN_PATHMUSTEXIST | OFN_FORCESHOWHIDDEN;

					if (GetOpenFileName (&ofn))
					{
						_r_fastlock_acquireexclusive (&lock_access);

						size_t hash = 0;

						if (files[ofn.nFileOffset - 1] != 0)
						{
							hash = _app_addapplication (hwnd, files, 0, 0, 0, false, false, false);
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
									hash = _app_addapplication (hwnd, _r_fmt (L"%s\\%s", dir, p), 0, 0, 0, false, false, false);
							}
						}

						_r_fastlock_releaseexclusive (&lock_access);

						_app_listviewsort (hwnd, IDC_LISTVIEW, -1, false);
						_app_profilesave (hwnd);

						ShowItem (hwnd, IDC_LISTVIEW, _app_getposition (hwnd, hash), -1);
					}

					break;
				}

				case IDM_ALL_PROCESSES:
				case IDM_ALL_PACKAGES:
				case IDM_ALL_SERVICES:
				{
					_r_fastlock_acquireexclusive (&lock_access);

					if (LOWORD (wparam) == IDM_ALL_PROCESSES)
					{
						for (size_t i = 0; i < processes.size (); i++)
							_app_addapplication (hwnd, processes.at (i).real_path, 0, 0, 0, false, false, true);
					}
					else if (LOWORD (wparam) == IDM_ALL_PACKAGES)
					{
						for (size_t i = 0; i < packages.size (); i++)
							_app_addapplication (hwnd, packages.at (i).sid, 0, 0, 0, false, false, true);
					}
					else if (LOWORD (wparam) == IDM_ALL_SERVICES)
					{
						for (size_t i = 0; i < services.size (); i++)
							_app_addapplication (hwnd, services.at (i).service_name, 0, 0, 0, false, false, true);
					}

					_r_fastlock_releaseexclusive (&lock_access);

					_app_listviewsort (hwnd, IDC_LISTVIEW, -1, false);
					_app_profilesave (hwnd);

					break;
				}

				case IDM_DISABLENOTIFICATIONS:
				case IDM_DISABLETIMER:
				case IDM_EXPLORE:
				case IDM_COPY:
				case IDM_CHECK:
				case IDM_UNCHECK:
				case IDM_PROPERTIES:
				{
					const UINT ctrl_id = LOWORD (wparam);

					size_t item = LAST_VALUE;
					BOOL new_val = BOOL (-1);

					rstring buffer;

					_r_fastlock_acquireexclusive (&lock_access);

					if (
						ctrl_id == IDM_CHECK ||
						ctrl_id == IDM_UNCHECK ||
						ctrl_id == IDM_DISABLETIMER
						)
					{
						_r_fastlock_acquireexclusive (&lock_apply);

						_wfp_transact_start (__LINE__);
					}

					while ((item = (size_t)SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_GETNEXTITEM, item, LVNI_SELECTED)) != LAST_VALUE)
					{
						const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, IDC_LISTVIEW, item);

						PITEM_APP ptr_app = _app_getapplication (hash);

						if (!ptr_app)
							continue;

						if (ctrl_id == IDM_EXPLORE)
						{
							if (ptr_app->type != AppPico && ptr_app->type != AppDevice)
							{
								if (_r_fs_exists (ptr_app->real_path))
									_r_run (nullptr, _r_fmt (L"\"explorer.exe\" /select,\"%s\"", ptr_app->real_path));

								else if (_r_fs_exists (_r_path_extractdir (ptr_app->real_path)))
									ShellExecute (hwnd, nullptr, _r_path_extractdir (ptr_app->real_path), nullptr, nullptr, SW_SHOWDEFAULT);
							}
						}
						else if (ctrl_id == IDM_PROPERTIES)
						{
							if (ptr_app->type != AppPico && ptr_app->type != AppDevice)
							{
								if (_r_fs_exists (ptr_app->real_path))
								{
									SHELLEXECUTEINFO shex = {0};

									shex.cbSize = sizeof (shex);
									shex.fMask = SEE_MASK_UNICODE | SEE_MASK_NOZONECHECKS | SEE_MASK_INVOKEIDLIST;
									shex.hwnd = hwnd;
									shex.lpVerb = L"properties";
									shex.nShow = SW_NORMAL;
									shex.lpFile = ptr_app->real_path;

									ShellExecuteEx (&shex);
								}
							}
						}
						else if (ctrl_id == IDM_COPY)
						{
							buffer.Append (ptr_app->display_name).Append (L"\r\n");
						}
						else if (ctrl_id == IDM_DISABLENOTIFICATIONS)
						{
							if (new_val == BOOL (-1))
								new_val = !ptr_app->is_silent;

							ptr_app->is_silent = new_val ? true : false;

							if (new_val)
							{
								_r_fastlock_acquireexclusive (&lock_notification);
								_app_freenotify (hash, false);
								_r_fastlock_releaseexclusive (&lock_notification);
							}
						}
						else if (ctrl_id == IDM_DISABLETIMER)
						{
							_app_timer_remove (hwnd, hash, true);
						}
						else if (ctrl_id == IDM_CHECK || ctrl_id == IDM_UNCHECK)
						{
							ptr_app->is_enabled = (ctrl_id == IDM_CHECK) ? true : false;

							if (ctrl_id == IDM_UNCHECK)
								_app_timer_remove (hwnd, hash, true);

							config.is_nocheckboxnotify = true;

							_r_listview_setitemcheck (hwnd, IDC_LISTVIEW, item, ptr_app->is_enabled);

							config.is_nocheckboxnotify = false;

							_r_fastlock_acquireexclusive (&lock_notification);
							_app_freenotify (hash, false);
							_r_fastlock_releaseexclusive (&lock_notification);

							_wfp_create3filters (ptr_app, true);
						}
					}

					_r_fastlock_releaseexclusive (&lock_access);

					if (ctrl_id == IDM_CHECK ||
						ctrl_id == IDM_UNCHECK ||
						ctrl_id == IDM_DISABLETIMER)
					{
						_wfp_transact_commit (__LINE__);

						_r_fastlock_releaseexclusive (&lock_apply);

						_app_notifyrefresh ();

						_app_listviewsort (hwnd, IDC_LISTVIEW, -1, false);
						_app_profilesave (hwnd);

						_r_listview_redraw (hwnd, IDC_LISTVIEW);
					}
					else if (ctrl_id == IDM_DISABLENOTIFICATIONS)
					{
						_app_notifyrefresh ();
						_app_refreshstatus (hwnd);

						_app_profilesave (hwnd);

						_r_listview_redraw (hwnd, IDC_LISTVIEW);
					}
					else if (ctrl_id == IDM_COPY)
					{
						buffer.Trim (L"\r\n");
						_r_clipboard_set (hwnd, buffer, buffer.GetLength ());
					}

					break;
				}

				case IDM_OPENRULESEDITOR:
				{
					PITEM_RULE ptr_rule = new ITEM_RULE;

					if (ptr_rule)
					{
						ptr_rule->is_enabled = true;
						ptr_rule->is_block = ((app.ConfigGet (L"Mode", ModeWhitelist).AsUint () == ModeWhitelist) ? false : true);

						size_t item = LAST_VALUE;

						while ((item = (size_t)SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_GETNEXTITEM, item, LVNI_SELECTED)) != LAST_VALUE)
						{
							const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, IDC_LISTVIEW, item);

							if (hash)
								ptr_rule->apps[hash] = true;
						}

						if (DialogBoxParam (nullptr, MAKEINTRESOURCE (IDD_EDITOR), hwnd, &EditorProc, (LPARAM)ptr_rule))
						{
							_r_fastlock_acquireexclusive (&lock_access);
							rules_custom.push_back (ptr_rule);
							_r_fastlock_releaseexclusive (&lock_access);

							_app_listviewsort (hwnd, IDC_LISTVIEW, -1, false);
							_app_profilesave (hwnd);

							_r_listview_redraw (hwnd, IDC_LISTVIEW);

							_wfp_create4filters (ptr_rule, false);
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

					if (!selected || !app.ConfirmMessage (hwnd, nullptr, _r_fmt (app.LocaleString (IDS_QUESTION_DELETE, nullptr), selected), L"ConfirmDelete"))
						break;

					const size_t count = _r_listview_getitemcount (hwnd, IDC_LISTVIEW) - 1;

					size_t item = LAST_VALUE;

					_r_fastlock_acquireexclusive (&lock_apply);
					_r_fastlock_acquireexclusive (&lock_access);

					_wfp_transact_start (__LINE__);

					for (size_t i = count; i != LAST_VALUE; i--)
					{
						if (ListView_GetItemState (GetDlgItem (hwnd, IDC_LISTVIEW), i, LVNI_SELECTED))
						{
							const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, IDC_LISTVIEW, i);
							PITEM_APP ptr_app = _app_getapplication (hash);

							if (ptr_app && !ptr_app->is_undeletable) // skip "undeletable" apps
							{
								_wfp_destroy2filters (&ptr_app->mfarr, true);

								SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_DELETEITEM, i, 0);
								_app_freeapplication (hash);

								item = i;
							}
						}
					}

					_wfp_transact_commit (__LINE__);

					_r_fastlock_releaseexclusive (&lock_access);
					_r_fastlock_releaseexclusive (&lock_apply);

					if (item != LAST_VALUE)
						ShowItem (hwnd, IDC_LISTVIEW, min (item, _r_listview_getitemcount (hwnd, IDC_LISTVIEW) - 1), -1);

					_app_profilesave (hwnd);

					_app_notifyrefresh ();
					_app_refreshstatus (hwnd);

					_r_listview_redraw (hwnd, IDC_LISTVIEW);

					break;
				}

				case IDM_PURGE_UNUSED:
				case IDM_PURGE_ERRORS:
				{
					bool is_deleted = false;

					const size_t count = _r_listview_getitemcount (hwnd, IDC_LISTVIEW) - 1;

					_r_fastlock_acquireexclusive (&lock_apply);
					_r_fastlock_acquireexclusive (&lock_access);

					_wfp_transact_start (__LINE__);

					for (size_t i = count; i != LAST_VALUE; i--)
					{
						const size_t hash = (size_t)_r_listview_getitemlparam (hwnd, IDC_LISTVIEW, i);
						PITEM_APP ptr_app = _app_getapplication (hash);

						if (ptr_app)
						{
							if (!_app_isexists (ptr_app) || (LOWORD (wparam) == IDM_PURGE_UNUSED && _app_isunused (ptr_app, hash)))
							{
								_wfp_destroy2filters (&ptr_app->mfarr, true);

								SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_DELETEITEM, i, 0);
								_app_freeapplication (hash);

								is_deleted = true;
							}
						}
					}

					_wfp_transact_commit (__LINE__);

					_r_fastlock_releaseexclusive (&lock_access);
					_r_fastlock_releaseexclusive (&lock_apply);

					if (is_deleted)
					{
						_app_profilesave (hwnd);

						_app_notifyrefresh ();
						_app_refreshstatus (hwnd);

						_r_listview_redraw (hwnd, IDC_LISTVIEW);
					}

					break;
				}

				case IDM_PURGE_TIMERS:
				{
					if (!_app_istimersactive () || !app.ConfirmMessage (hwnd, nullptr, app.LocaleString (IDS_QUESTION_TIMERS, nullptr), L"ConfirmTimers"))
						break;

					_r_fastlock_acquireexclusive (&lock_apply);
					_r_fastlock_acquireexclusive (&lock_access);

					_wfp_transact_start (__LINE__);

					for (auto &p : apps)
					{
						if (p.second.htimer)
							_app_timer_remove (hwnd, p.first, true);
					}

					_wfp_transact_commit (__LINE__);

					_r_fastlock_releaseexclusive (&lock_access);
					_r_fastlock_releaseexclusive (&lock_apply);

					_app_listviewsort (hwnd, IDC_LISTVIEW, -1, false);
					_app_profilesave (hwnd);

					_r_listview_redraw (hwnd, IDC_LISTVIEW);

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

				case 999:
				{
					apps[config.myhash].last_notify = 0;

					ITEM_LOG log = {0};

					log.hash = config.myhash;
					log.date = _r_unixtime_now ();

					log.af = AF_INET;
					log.protocol = IPPROTO_TCP;

					InetPton (log.af, L"195.210.46.14", &log.remote_addr);
					log.remote_port = 443;

					InetPton (log.af, L"192.168.2.2", &log.local_addr);
					log.local_port = 80;

					_app_formataddress (log.remote_fmt, _countof (log.remote_fmt), &log, FWP_DIRECTION_OUTBOUND, log.remote_port, true);
					_app_formataddress (log.local_fmt, _countof (log.local_fmt), &log, FWP_DIRECTION_INBOUND, log.local_port, true);

					StringCchCopy (log.filter_name, _countof (log.filter_name), L"<test filter>");

					_app_notifyadd (&log);

					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

INT APIENTRY wWinMain (HINSTANCE, HINSTANCE, LPWSTR args, INT)
{
	MSG msg = {0};

	if (wcsstr (args, L"/uninstall"))
	{
		if (_wfp_isfiltersinstalled ())
		{
			if (_app_installmessage (nullptr, false))
			{
				if (_wfp_initialize (false))
					_wfp_destroyfilters (true);

				_wfp_uninitialize (true);
			}
		}

		return ERROR_SUCCESS;
	}

	if (app.CreateMainWindow (IDD_MAIN, IDI_MAIN, &DlgProc))
	{
		const HACCEL haccel = LoadAccelerators (app.GetHINSTANCE (), MAKEINTRESOURCE (IDA_MAIN));

		if (haccel)
		{
			while (GetMessage (&msg, nullptr, 0, 0) > 0)
			{
				TranslateAccelerator (app.GetHWND (), haccel, &msg);

				if (!IsDialogMessage (app.GetHWND (), &msg))
				{
					TranslateMessage (&msg);
					DispatchMessage (&msg);
				}
			}

			DestroyAcceleratorTable (haccel);
		}
	}

	return (INT)msg.wParam;
}
