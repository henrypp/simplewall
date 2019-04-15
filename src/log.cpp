// simplewall
// Copyright (c) 2016-2019 Henry++

#include "global.hpp"

void _app_logerror (LPCWSTR fn, DWORD errcode, LPCWSTR desc, bool is_nopopups)
{
	const time_t current_time = _r_unixtime_now ();

	_r_dbg (fn, errcode, desc);

	if ((current_time - app.ConfigGet (L"ErrorNotificationsTimestamp", 0).AsLonglong ()) >= app.ConfigGet (L"ErrorNotificationsPeriod", 4).AsLonglong () && !is_nopopups && app.ConfigGet (L"IsErrorNotificationsEnabled", true).AsBool ()) // check for timeout (sec.)
	{
		app.TrayPopup (app.GetHWND (), UID, nullptr, NIIF_ERROR | (app.ConfigGet (L"IsNotificationsSound", true).AsBool () ? 0 : NIIF_NOSOUND), APP_NAME, app.LocaleString (IDS_STATUS_ERROR, nullptr));
		app.ConfigSet (L"ErrorNotificationsTimestamp", current_time);
	}
}

rstring _app_getlogviewer ()
{
	rstring result;

	static LPCWSTR csvviewer[] = {
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

bool _app_logchecklimit ()
{
	const DWORD limit = app.ConfigGet (L"LogSizeLimitKb", LOG_SIZE_LIMIT).AsUlong ();

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

	config.hlogfile = CreateFile (path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

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

			_r_fs_setpos (config.hlogfile, 0, FILE_END);
		}

		result = true;
	}

	_r_fastlock_releaseexclusive (&lock_writelog);

	return result;
}

void _app_logwrite (PITEM_LOG const ptr_log)
{
	if (!ptr_log || !config.hlogfile || config.hlogfile == INVALID_HANDLE_VALUE)
		return;

	// parse path
	rstring path;
	{
		_r_fastlock_acquireshared (&lock_access);

		PITEM_APP const ptr_app = _app_getapplication (ptr_log->hash);

		if (ptr_app)
		{
			if (ptr_app->type == AppPackage || ptr_app->type == AppService)
			{
				if (ptr_app->real_path && ptr_app->real_path[0])
					path = ptr_app->real_path;

				else if (ptr_app->display_name && ptr_app->display_name[0])
					path = ptr_app->display_name;
			}
			else if (ptr_app->original_path && ptr_app->original_path[0])
			{
				path = ptr_app->original_path;
			}
		}

		_r_fastlock_releaseshared (&lock_access);
	}

	// parse filter name
	rstring filter_name;
	{
		if ((ptr_log->provider_name && ptr_log->provider_name[0]) && (ptr_log->filter_name && ptr_log->filter_name[0]))
			filter_name.Format (L"%s\\%s", ptr_log->provider_name, ptr_log->filter_name);

		else if (ptr_log->filter_name && ptr_log->filter_name[0])
			filter_name = ptr_log->filter_name;
	}

	// parse direction
	rstring direction;
	{
		if (ptr_log->direction == FWP_DIRECTION_INBOUND)
			direction = SZ_LOG_DIRECTION_IN;

		else if (ptr_log->direction == FWP_DIRECTION_OUTBOUND)
			direction = SZ_LOG_DIRECTION_OUT;

		if (ptr_log->is_loopback)
			direction.Append (SZ_LOG_DIRECTION_LOOPBACK);
	}

	rstring buffer;
	buffer.Format (L"\"%s\"%c\"%s\"%c\"%s\"%c%s (" SZ_LOG_REMOTE_ADDRESS L")%c%s (" SZ_LOG_LOCAL_ADDRESS L")%c%s%c\"%s\"%c#%" PRIu64 L"%c%s%c%s\r\n",
				   _r_fmt_date (ptr_log->date, FDTF_SHORTDATE | FDTF_LONGTIME).GetString (),
				   LOG_DIV,
				   ptr_log->username ? ptr_log->username : SZ_EMPTY,
				   LOG_DIV,
				   path.IsEmpty () ? SZ_EMPTY : path.GetString (),
				   LOG_DIV,
				   ptr_log->remote_fmt ? ptr_log->remote_fmt : SZ_EMPTY,
				   LOG_DIV,
				   ptr_log->local_fmt ? ptr_log->local_fmt : SZ_EMPTY,
				   LOG_DIV,
				   _app_getprotoname (ptr_log->protocol).GetString (),
				   LOG_DIV,
				   filter_name.IsEmpty () ? SZ_EMPTY : filter_name.GetString (),
				   LOG_DIV,
				   ptr_log->filter_id,
				   LOG_DIV,
				   direction.IsEmpty () ? SZ_EMPTY : direction.GetString (),
				   LOG_DIV,
				   (ptr_log->is_allow ? SZ_LOG_ALLOW : SZ_LOG_BLOCK)
	);

	_r_fastlock_acquireexclusive (&lock_writelog);

	_app_logchecklimit ();

	DWORD written = 0;
	WriteFile (config.hlogfile, buffer.GetString (), DWORD (buffer.GetLength () * sizeof (WCHAR)), &written, nullptr);

	_r_fastlock_releaseexclusive (&lock_writelog);
}

void _app_logclear ()
{
	const rstring path = _r_path_expand (app.ConfigGet (L"LogPath", LOG_PATH_DEFAULT));

	_r_fastlock_acquireexclusive (&lock_writelog);

	if (config.hlogfile != nullptr && config.hlogfile != INVALID_HANDLE_VALUE)
	{
		_r_fs_setpos (config.hlogfile, 2, FILE_BEGIN);

		SetEndOfFile (config.hlogfile);
	}
	else
	{
		_r_fs_delete (path, false);
	}

	_r_fs_delete (_r_fmt (L"%s.bak", path.GetString ()), false);

	_r_fastlock_releaseexclusive (&lock_writelog);
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
			typedef DWORD (WINAPI * FWPMNES5) (HANDLE, const FWPM_NET_EVENT_SUBSCRIPTION0 *, FWPM_NET_EVENT_CALLBACK4, LPVOID, LPHANDLE); // win10new+
			typedef DWORD (WINAPI * FWPMNES4) (HANDLE, const FWPM_NET_EVENT_SUBSCRIPTION0 *, FWPM_NET_EVENT_CALLBACK4, LPVOID, LPHANDLE); // win10rs5+
			typedef DWORD (WINAPI * FWPMNES3) (HANDLE, const FWPM_NET_EVENT_SUBSCRIPTION0 *, FWPM_NET_EVENT_CALLBACK3, LPVOID, LPHANDLE); // win10rs4+
			typedef DWORD (WINAPI * FWPMNES2) (HANDLE, const FWPM_NET_EVENT_SUBSCRIPTION0 *, FWPM_NET_EVENT_CALLBACK2, LPVOID, LPHANDLE); // win10+
			typedef DWORD (WINAPI * FWPMNES1) (HANDLE, const FWPM_NET_EVENT_SUBSCRIPTION0 *, FWPM_NET_EVENT_CALLBACK1, LPVOID, LPHANDLE); // win8+
			typedef DWORD (WINAPI * FWPMNES0) (HANDLE, const FWPM_NET_EVENT_SUBSCRIPTION0 *, FWPM_NET_EVENT_CALLBACK0, LPVOID, LPHANDLE); // win7+

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

bool _wfp_logunsubscribe ()
{
	bool result = false;

	_app_loginit (false); // destroy log file handle if present

	if (config.hnetevent)
	{
		const HMODULE hlib = LoadLibrary (L"fwpuclnt.dll");

		if (hlib)
		{
			typedef DWORD (WINAPI * FWPMNEU) (HANDLE, HANDLE); // FwpmNetEventUnsubscribe0

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

void CALLBACK _wfp_logcallback (UINT32 flags, FILETIME const *pft, UINT8 const *app_id, SID * package_id, SID * user_id, UINT8 proto, FWP_IP_VERSION ipver, UINT32 remote_addr, FWP_BYTE_ARRAY16 const *remote_addr6, UINT16 remoteport, UINT32 local_addr, FWP_BYTE_ARRAY16 const *local_addr6, UINT16 localport, UINT16 layer_id, UINT64 filter_id, UINT32 direction, bool is_allow, bool is_loopback)
{
	if (_wfp_isfiltersapplying ())
		return;

	if (is_allow && app.ConfigGet (L"IsExcludeClassifyAllow", true).AsBool ())
		return;

	// do not parse when tcp connection has been established, or when non-tcp traffic has been authorized
	if (layer_id)
	{
		FWPM_LAYER *layer = nullptr;

		if (FwpmLayerGetById (config.hengine, layer_id, &layer) == ERROR_SUCCESS && layer)
		{
			if (memcmp (&layer->layerKey, &FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4, sizeof (GUID)) == 0 || memcmp (&layer->layerKey, &FWPM_LAYER_ALE_FLOW_ESTABLISHED_V6, sizeof (GUID)) == 0)
			{
				FwpmFreeMemory ((void **)& layer);
				return;
			}

			FwpmFreeMemory ((void **)& layer);
		}
	}

	const bool is_logenabled = app.ConfigGet (L"IsLogEnabled", false).AsBool ();
	const bool is_notificationenabled = app.ConfigGet (L"IsNotificationsEnabled", true).AsBool ();

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
		rstring sidstring;

		if ((flags & FWPM_NET_EVENT_FLAG_PACKAGE_ID_SET) != 0 && package_id)
		{
			sidstring = _r_str_fromsid (package_id);

			if (sidstring.IsEmpty () || !_app_item_get (AppPackage, sidstring.Hash (), nullptr, nullptr, nullptr, nullptr, nullptr))
				sidstring.Clear ();
		}

		// copy converted nt device path into win32
		if ((flags & FWPM_NET_EVENT_FLAG_PACKAGE_ID_SET) != 0 && !sidstring.IsEmpty ())
		{
			_r_str_alloc (&ptr_log->path, sidstring.GetLength (), sidstring);

			ptr_log->hash = sidstring.Hash ();
		}
		else if ((flags & FWPM_NET_EVENT_FLAG_APP_ID_SET) != 0 && app_id)
		{
			const rstring path = _r_path_dospathfromnt (LPCWSTR (app_id));

			_r_str_alloc (&ptr_log->path, path.GetLength (), path);

			ptr_log->hash = path.Hash ();

			_app_applycasestyle (ptr_log->path, _r_str_length (ptr_log->path)); // apply case-style
		}
		else
		{
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
					{
						rstring userstring;
						userstring.Format (L"%s\\%s", domain, username);

						_r_str_alloc (&ptr_log->username, userstring.GetLength (), userstring);
					}
				}
			}

			// read filter information
			if (filter_id)
			{
				FWPM_FILTER *ptr_filter = nullptr;
				FWPM_PROVIDER *ptr_provider = nullptr;

				ptr_log->filter_id = filter_id;

				if (FwpmFilterGetById (config.hengine, filter_id, &ptr_filter) == ERROR_SUCCESS && ptr_filter)
				{
					LPCWSTR filter_name = ptr_filter->displayData.name ? ptr_filter->displayData.name : ptr_filter->displayData.description;

					_r_str_alloc (&ptr_log->filter_name, _r_str_length (filter_name), filter_name);

					if (ptr_filter->providerKey)
					{
						if (memcmp (ptr_filter->providerKey, &GUID_WfpProvider, sizeof (GUID)) == 0)
							ptr_log->is_myprovider = true;

						if (FwpmProviderGetByKey (config.hengine, ptr_filter->providerKey, &ptr_provider) == ERROR_SUCCESS && ptr_provider)
						{
							LPCWSTR provider_name = ptr_provider->displayData.name ? ptr_provider->displayData.name : ptr_provider->displayData.description;

							_r_str_alloc (&ptr_log->provider_name, _r_str_length (provider_name), provider_name);
						}
					}

					if (ptr_filter->weight.type == FWP_UINT8)
					{
						ptr_log->is_blocklist = (ptr_filter->weight.uint8 == FILTER_WEIGHT_BLOCKLIST);
						ptr_log->is_system = (ptr_filter->weight.uint8 == FILTER_WEIGHT_HIGHEST) || (ptr_filter->weight.uint8 == FILTER_WEIGHT_HIGHEST_IMPORTANT);
						ptr_log->is_custom = (ptr_filter->weight.uint8 == FILTER_WEIGHT_CUSTOM) || (ptr_filter->weight.uint8 == FILTER_WEIGHT_CUSTOM_BLOCK);
					}
				}

				if (ptr_filter)
					FwpmFreeMemory ((void **)& ptr_filter);

				if (ptr_provider)
					FwpmFreeMemory ((void **)& ptr_provider);
			}

			// destination
			if ((flags & FWPM_NET_EVENT_FLAG_IP_VERSION_SET) != 0)
			{
				if (ipver == FWP_IP_VERSION_V4)
				{
					ptr_log->af = AF_INET;

					// remote address
					if ((flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET) != 0 && remote_addr)
						ptr_log->remote_addr.S_un.S_addr = ntohl (remote_addr);

					// local address
					if ((flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET) != 0 && local_addr)
						ptr_log->local_addr.S_un.S_addr = ntohl (local_addr);
				}
				else if (ipver == FWP_IP_VERSION_V6)
				{
					ptr_log->af = AF_INET6;

					// remote address
					if ((flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET) != 0 && remote_addr6)
						CopyMemory (ptr_log->remote_addr6.u.Byte, remote_addr6->byteArray16, FWP_V6_ADDR_SIZE);

					// local address
					if ((flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET) != 0 && local_addr6)
						CopyMemory (&ptr_log->local_addr6.u.Byte, local_addr6->byteArray16, FWP_V6_ADDR_SIZE);
				}

				if ((flags & FWPM_NET_EVENT_FLAG_REMOTE_PORT_SET) != 0 && remoteport)
					ptr_log->remote_port = remoteport;

				if ((flags & FWPM_NET_EVENT_FLAG_LOCAL_PORT_SET) != 0 && localport)
					ptr_log->local_port = localport;
			}

			// protocol
			if ((flags & FWPM_NET_EVENT_FLAG_IP_PROTOCOL_SET) != 0 && proto)
				ptr_log->protocol = proto;

			// indicates FWPM_NET_EVENT_TYPE_CLASSIFY_ALLOW state
			ptr_log->is_allow = is_allow;

			// indicates whether the packet originated from (or was heading to) the loopback adapter
			ptr_log->is_loopback = is_loopback;

			// indicates the direction of the packet transmission
			if (direction == FWP_DIRECTION_OUTBOUND || direction == FWP_DIRECTION_OUT)
				ptr_log->direction = FWP_DIRECTION_OUTBOUND;

			else if (direction == FWP_DIRECTION_INBOUND || direction == FWP_DIRECTION_IN)
				ptr_log->direction = FWP_DIRECTION_INBOUND;
		}

		// push into a slist
		{
			ptr_entry->Body = ptr_log;

			RtlInterlockedPushEntrySList (&log_stack.ListHead, &ptr_entry->ListEntry);
			InterlockedIncrement (&log_stack.item_count);

			// check if thread has been terminated
			const LONG thread_count = InterlockedCompareExchange (&log_stack.thread_count, 0, 0);

			if (!_r_fastlock_islocked (&lock_logthread) || ((_r_fastlock_islocked (&lock_logbusy) || InterlockedCompareExchange (&log_stack.item_count, 0, 0) >= NOTIFY_LIMIT_POOL_SIZE) && (thread_count >= 1 && thread_count < NOTIFY_LIMIT_THREAD_COUNT)))
			{
				_r_fastlock_acquireexclusive (&lock_threadpool);

				_app_freethreadpool (&threads_pool);

				const HANDLE hthread = _r_createthread (&LogThread, app.GetHWND (), true, THREAD_PRIORITY_BELOW_NORMAL);

				if (hthread)
				{
					InterlockedIncrement (&log_stack.thread_count);

					threads_pool.push_back (hthread);
					ResumeThread (hthread);
				}

				_r_fastlock_releaseexclusive (&lock_threadpool);
			}
		}
	}
}

// win7+ callback
void CALLBACK _wfp_logcallback0 (LPVOID, const FWPM_NET_EVENT1 * pEvent)
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
void CALLBACK _wfp_logcallback1 (LPVOID, const FWPM_NET_EVENT2 * pEvent)
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
void CALLBACK _wfp_logcallback2 (LPVOID, const FWPM_NET_EVENT3 * pEvent)
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
void CALLBACK _wfp_logcallback3 (LPVOID, const FWPM_NET_EVENT4 * pEvent)
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
void CALLBACK _wfp_logcallback4 (LPVOID, const FWPM_NET_EVENT5 * pEvent)
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

UINT WINAPI LogThread (LPVOID lparam)
{
	const HWND hwnd = (HWND)lparam;

	_r_fastlock_acquireshared (&lock_logthread);

	while (true)
	{
		const PSLIST_ENTRY listEntry = RtlInterlockedPopEntrySList (&log_stack.ListHead);

		if (!listEntry)
			break;

		InterlockedDecrement (&log_stack.item_count);

		PITEM_LIST_ENTRY ptr_entry = CONTAINING_RECORD (listEntry, ITEM_LIST_ENTRY, ListEntry);
		PITEM_LOG ptr_log = ptr_entry->Body;

		_aligned_free (ptr_entry);

		if (ptr_log)
		{
			const bool is_logenabled = app.ConfigGet (L"IsLogEnabled", false).AsBool ();
			const bool is_notificationenabled = app.ConfigGet (L"IsNotificationsEnabled", true).AsBool ();

			// apps collector
			_r_fastlock_acquireshared (&lock_access);
			const bool is_notexist = ptr_log->hash && ptr_log->path && !ptr_log->is_allow && apps.find (ptr_log->hash) == apps.end ();
			_r_fastlock_releaseshared (&lock_access);

			if (is_notexist)
			{
				_r_fastlock_acquireshared (&lock_logbusy);

				_r_fastlock_acquireexclusive (&lock_access);
				_app_addapplication (hwnd, ptr_log->path, 0, 0, 0, false, false, true);
				_r_fastlock_releaseexclusive (&lock_access);

				_r_fastlock_releaseshared (&lock_logbusy);

				_app_listviewsort (hwnd, IDC_APPS_PROFILE, -1, false);
				_app_profile_save (hwnd);
			}

			bool is_added = false;

			if ((is_logenabled || is_notificationenabled) && (!(ptr_log->is_system && app.ConfigGet (L"IsExcludeStealth", true).AsBool ())))
			{
				_r_fastlock_acquireshared (&lock_logbusy);

				_app_formataddress (ptr_log->af, &ptr_log->remote_addr, ptr_log->remote_port, &ptr_log->remote_fmt, true);
				_app_formataddress (ptr_log->af, &ptr_log->local_addr, ptr_log->local_port, &ptr_log->local_fmt, true);

				_r_fastlock_releaseshared (&lock_logbusy);

				// write log to a file
				if (is_logenabled)
					_app_logwrite (ptr_log);

				// show notification (only for my own provider and file is present)
				if (is_notificationenabled && ptr_log->hash && !ptr_log->is_allow && ptr_log->is_myprovider)
				{
					if (!(ptr_log->is_blocklist && app.ConfigGet (L"IsExcludeBlocklist", true).AsBool ()) && !(ptr_log->is_custom && app.ConfigGet (L"IsExcludeCustomRules", true).AsBool ()))
					{
						// read app config
						_r_fastlock_acquireexclusive (&lock_access);

						PITEM_APP const ptr_app = _app_getapplication (ptr_log->hash);

						if (ptr_app)
						{
							if (!ptr_app->is_silent)
								is_added = _app_notifyadd (config.hnotification, ptr_log, ptr_app);
						}

						_r_fastlock_releaseexclusive (&lock_access);
					}
				}
			}

			if (!is_added)
				SAFE_DELETE (ptr_log);
		}
	}

	_r_fastlock_releaseshared (&lock_logthread);

	InterlockedDecrement (&log_stack.thread_count);

	_endthreadex (0);

	return ERROR_SUCCESS;
}
