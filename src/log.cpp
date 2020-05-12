// simplewall
// Copyright (c) 2016-2020 Henry++

#include "global.hpp"

rstring _app_getlogviewer ()
{
	rstring result = app.ConfigGet (L"LogViewer", LOG_VIEWER_DEFAULT);

	if (result.IsEmpty ())
		return _r_path_expand (LOG_VIEWER_DEFAULT);

	return _r_path_expand (result);
}

void _app_loginit (bool is_install)
{
	// dropped packets logging (win7+)
	if (!config.hnetevent)
		return;

	// reset all handles
	SAFE_DELETE_HANDLE (config.hlogfile);

	if (!is_install || !app.ConfigGet (L"IsLogEnabled", false).AsBool ())
		return; // already closed or not enabled

	const rstring path = _r_path_expand (app.ConfigGet (L"LogPath", LOG_PATH_DEFAULT));

	config.hlogfile = CreateFile (path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (!_r_fs_isvalidhandle (config.hlogfile))
	{
		app.LogError (L"CreateFile", GetLastError (), path, 0);
	}
	else
	{
		if (GetLastError () != ERROR_ALREADY_EXISTS)
		{
			DWORD written = 0;
			const BYTE bom[] = {0xFF, 0xFE};

			WriteFile (config.hlogfile, bom, sizeof (bom), &written, nullptr); // write utf-16 le byte order mask

			WriteFile (config.hlogfile, SZ_LOG_TITLE, DWORD (_r_str_length (SZ_LOG_TITLE) * sizeof (WCHAR)), &written, nullptr); // adds csv header
		}
		else
		{
			if (_app_logchecklimit ())
				_app_logclear ();

			_r_fs_setpos (config.hlogfile, 0, FILE_END);
		}
	}
}

void _app_logwrite (PITEM_LOG ptr_log)
{
	if (!_r_fs_isvalidhandle (config.hlogfile))
		return;

	// parse path
	rstring path;
	{
		PR_OBJECT ptr_app_object = _app_getappitem (ptr_log->app_hash);

		if (ptr_app_object)
		{
			PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

			if (ptr_app)
			{
				if (ptr_app->type == DataAppUWP || ptr_app->type == DataAppService)
				{
					if (!_r_str_isempty (ptr_app->real_path))
						path = ptr_app->real_path;

					else if (!_r_str_isempty (ptr_app->display_name))
						path = ptr_app->display_name;
				}
				else if (!_r_str_isempty (ptr_app->original_path))
				{
					path = ptr_app->original_path;
				}
			}

			_r_obj_dereference (ptr_app_object);
		}
	}

	// parse filter name
	rstring filter_name;
	{
		if (!_r_str_isempty (ptr_log->provider_name) && !_r_str_isempty (ptr_log->filter_name))
			filter_name.Format (L"%s\\%s", ptr_log->provider_name, ptr_log->filter_name);

		else if (!_r_str_isempty (ptr_log->filter_name))
			filter_name = ptr_log->filter_name;
	}

	// parse direction
	rstring direction;
	{
		if (ptr_log->direction == FWP_DIRECTION_INBOUND)
			direction = SZ_DIRECTION_IN;

		else if (ptr_log->direction == FWP_DIRECTION_OUTBOUND)
			direction = SZ_DIRECTION_OUT;

		if (ptr_log->is_loopback)
			direction.Append (SZ_DIRECTION_LOOPBACK);
	}

	LPWSTR local_fmt = nullptr;
	LPWSTR remote_fmt = nullptr;

	_app_formataddress (ptr_log->af, 0, &ptr_log->local_addr, 0, &local_fmt, FMTADDR_RESOLVE_HOST);
	_app_formataddress (ptr_log->af, 0, &ptr_log->remote_addr, 0, &remote_fmt, FMTADDR_RESOLVE_HOST);

	rstring buffer;
	buffer.Format (SZ_LOG_BODY,
				   _r_fmt_date (ptr_log->timestamp, FDTF_SHORTDATE | FDTF_LONGTIME).GetString (),
				   !_r_str_isempty (ptr_log->username) ? ptr_log->username : SZ_EMPTY,
				   !path.IsEmpty () ? path.GetString () : SZ_EMPTY,
				   !_r_str_isempty (local_fmt) ? local_fmt : SZ_EMPTY,
				   ptr_log->local_port ? _app_formatport (ptr_log->local_port, true).GetString () : SZ_EMPTY,
				   !_r_str_isempty (remote_fmt) ? remote_fmt : SZ_EMPTY,
				   ptr_log->remote_port ? _app_formatport (ptr_log->remote_port, true).GetString () : SZ_EMPTY,
				   _app_getprotoname (ptr_log->protocol, ptr_log->af).GetString (),
				   !filter_name.IsEmpty () ? filter_name.GetString () : SZ_EMPTY,
				   ptr_log->filter_id,
				   !direction.IsEmpty () ? direction.GetString () : SZ_EMPTY,
				   (ptr_log->is_allow ? SZ_STATE_ALLOW : SZ_STATE_BLOCK)
	);

	SAFE_DELETE_ARRAY (local_fmt);
	SAFE_DELETE_ARRAY (remote_fmt);

	if (_app_logchecklimit ())
		_app_logclear ();

	DWORD written = 0;
	WriteFile (config.hlogfile, buffer.GetString (), DWORD (buffer.GetLength () * sizeof (WCHAR)), &written, nullptr);
}

bool _app_logisexists (HWND hwnd, PITEM_LOG ptr_log_new)
{
	bool is_hitcount = false;

	for (auto &p : log_arr)
	{
		PR_OBJECT ptr_log_object = _r_obj_reference (p);

		if (!ptr_log_object)
			continue;

		PITEM_LOG ptr_log = (PITEM_LOG)ptr_log_object->pdata;

		if (ptr_log)
		{
			if (
				ptr_log->is_allow == ptr_log_new->is_allow &&
				ptr_log->af == ptr_log_new->af &&
				ptr_log->direction == ptr_log_new->direction &&
				ptr_log->protocol == ptr_log_new->protocol &&
				ptr_log->remote_port == ptr_log_new->remote_port &&
				ptr_log->local_port == ptr_log_new->local_port
				)
			{
				if (ptr_log->af == AF_INET)
				{
					if (
						ptr_log->remote_addr.S_un.S_addr == ptr_log_new->remote_addr.S_un.S_addr &&
						ptr_log->local_addr.S_un.S_addr == ptr_log_new->local_addr.S_un.S_addr
						)
					{
						is_hitcount = true;
					}
				}
				else if (ptr_log->af == AF_INET6)
				{
					if (
						RtlEqualMemory (ptr_log->remote_addr6.u.Byte, ptr_log_new->remote_addr6.u.Byte, FWP_V6_ADDR_SIZE) &&
						RtlEqualMemory (ptr_log->local_addr6.u.Byte, ptr_log_new->local_addr6.u.Byte, FWP_V6_ADDR_SIZE)
						)
					{
						is_hitcount = true;
					}
				}

				if (is_hitcount)
				{
					ptr_log_new->timestamp = ptr_log->timestamp; // upd date
					_r_obj_dereference (ptr_log_object);

					break;
				}
			}
		}

		_r_obj_dereference (ptr_log_object);
	}

	return is_hitcount;
}

void _app_logwrite_ui (HWND hwnd, PR_OBJECT ptr_log_object)
{
	PITEM_LOG ptr_log = (PITEM_LOG)ptr_log_object->pdata;

	if (_app_logisexists (hwnd, ptr_log))
		return;

	const INT log_listview_id = IDC_LOG;

	size_t index = log_arr.size ();
	log_arr.push_back (_r_obj_reference (ptr_log_object));

	INT icon_id = (INT)_app_getappinfo (ptr_log->app_hash, InfoIconId);

	if (!icon_id)
		icon_id = config.icon_id;

	LPWSTR display_name = (LPWSTR)_app_getappinfo (ptr_log->app_hash, InfoName);

	if (_r_str_isempty (display_name))
		display_name = ptr_log->path;

	LPWSTR local_fmt = nullptr;
	LPWSTR remote_fmt = nullptr;

	_app_formataddress (ptr_log->af, 0, &ptr_log->local_addr, 0, &local_fmt, 0);
	_app_formataddress (ptr_log->af, 0, &ptr_log->remote_addr, 0, &remote_fmt, 0);

	const INT item_id = _r_listview_getitemcount (hwnd, log_listview_id);

	_r_listview_additem (hwnd, log_listview_id, item_id, 0, !_r_str_isempty (display_name) ? display_name : SZ_EMPTY, icon_id, I_GROUPIDNONE, index);
	_r_listview_setitem (hwnd, log_listview_id, item_id, 1, !_r_str_isempty (local_fmt) ? local_fmt : SZ_EMPTY);
	_r_listview_setitem (hwnd, log_listview_id, item_id, 3, !_r_str_isempty (remote_fmt) ? remote_fmt : SZ_EMPTY);
	_r_listview_setitem (hwnd, log_listview_id, item_id, 5, _app_getprotoname (ptr_log->protocol, ptr_log->af));

	if (ptr_log->local_port)
		_r_listview_setitem (hwnd, log_listview_id, item_id, 2, _app_formatport (ptr_log->local_port, true));

	if (ptr_log->remote_port)
		_r_listview_setitem (hwnd, log_listview_id, item_id, 4, _app_formatport (ptr_log->remote_port, true));

	{
		rstring direction;

		if (ptr_log->direction == FWP_DIRECTION_INBOUND)
			direction = SZ_DIRECTION_IN;

		else if (ptr_log->direction == FWP_DIRECTION_OUTBOUND)
			direction = SZ_DIRECTION_OUT;

		if (ptr_log->is_loopback)
			direction.Append (SZ_DIRECTION_LOOPBACK);

		_r_listview_setitem (hwnd, log_listview_id, item_id, 6, direction);
	}

	_r_listview_setitem (hwnd, log_listview_id, item_id, 7, ptr_log->is_allow ? SZ_STATE_ALLOW : SZ_STATE_BLOCK);

	SAFE_DELETE_ARRAY (local_fmt);
	SAFE_DELETE_ARRAY (remote_fmt);

	if (log_listview_id == (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT))
	{
		_app_listviewresize (hwnd, log_listview_id, false);
		_app_listviewsort (hwnd, log_listview_id);
	}
}

bool _app_logchecklimit ()
{
	const DWORD limit = app.ConfigGet (L"LogSizeLimitKb", LOG_SIZE_LIMIT_DEFAULT).AsUlong ();

	if (!limit || !_r_fs_isvalidhandle (config.hlogfile))
		return false;

	return (_r_fs_size (config.hlogfile) >= (limit * _R_BYTESIZE_KB));
}

void _app_logclear ()
{
	if (_r_fs_isvalidhandle (config.hlogfile))
	{
		_r_fs_setpos (config.hlogfile, 2, FILE_BEGIN);

		SetEndOfFile (config.hlogfile);

		DWORD written = 0;
		WriteFile (config.hlogfile, SZ_LOG_TITLE, DWORD (_r_str_length (SZ_LOG_TITLE) * sizeof (WCHAR)), &written, nullptr); // adds csv header

		FlushFileBuffers (config.hlogfile);
	}
	else
	{
		const rstring path = _r_path_expand (app.ConfigGet (L"LogPath", LOG_PATH_DEFAULT));

		_r_fs_remove (path, RFS_FORCEREMOVE);
		_r_fs_remove (_r_fmt (L"%s.bak", path.GetString ()), RFS_FORCEREMOVE);
	}
}

void _app_logclear_ui (HWND hwnd)
{
	SendDlgItemMessage (hwnd, IDC_LOG, LVM_DELETEALLITEMS, 0, 0);
	//SendDlgItemMessage (hwnd, IDC_LOG, LVM_SETITEMCOUNT, 0, 0);

	_app_freeobjects_vec (log_arr);

	if (IsWindowVisible (GetDlgItem (hwnd, IDC_LOG)))
		_app_listviewresize (hwnd, IDC_LOG, false);

	//SendDlgItemMessage (hwnd, IDC_LOG, LVM_SETITEMCOUNT, (WPARAM)log_arr.size (), LVSICF_NOSCROLL);
}

void _wfp_logsubscribe (HANDLE hengine)
{
	if (!hengine)
		return;

	if (config.hnetevent)
		return; // already subscribed

	HMODULE hlib = LoadLibraryEx (L"fwpuclnt.dll", nullptr, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);

	if (hlib)
	{
		using FWPMNES4 = decltype (&FwpmNetEventSubscribe4); // win10rs5+
		using FWPMNES3 = decltype (&FwpmNetEventSubscribe3); // win10rs4+
		using FWPMNES2 = decltype (&FwpmNetEventSubscribe2); // win10rs1+
		using FWPMNES1 = decltype (&FwpmNetEventSubscribe1); // win8+
		using FWPMNES0 = decltype (&FwpmNetEventSubscribe0); // win7+

		const FWPMNES4 _FwpmNetEventSubscribe4 = (FWPMNES4)GetProcAddress (hlib, "FwpmNetEventSubscribe4");
		const FWPMNES3 _FwpmNetEventSubscribe3 = (FWPMNES3)GetProcAddress (hlib, "FwpmNetEventSubscribe3");
		const FWPMNES2 _FwpmNetEventSubscribe2 = (FWPMNES2)GetProcAddress (hlib, "FwpmNetEventSubscribe2");
		const FWPMNES1 _FwpmNetEventSubscribe1 = (FWPMNES1)GetProcAddress (hlib, "FwpmNetEventSubscribe1");
		const FWPMNES0 _FwpmNetEventSubscribe0 = (FWPMNES0)GetProcAddress (hlib, "FwpmNetEventSubscribe0");

		if (_FwpmNetEventSubscribe4 || _FwpmNetEventSubscribe3 || _FwpmNetEventSubscribe2 || _FwpmNetEventSubscribe1 || _FwpmNetEventSubscribe0)
		{
			FWPM_NET_EVENT_SUBSCRIPTION subscription;
			FWPM_NET_EVENT_ENUM_TEMPLATE enum_template;

			RtlSecureZeroMemory (&subscription, sizeof (subscription));
			RtlSecureZeroMemory (&enum_template, sizeof (enum_template));

			enum_template.numFilterConditions = 0; // get events for all conditions

			subscription.enumTemplate = &enum_template;

			DWORD rc;

			if (_FwpmNetEventSubscribe4)
				rc = _FwpmNetEventSubscribe4 (hengine, &subscription, &_wfp_logcallback4, nullptr, &config.hnetevent); // win10rs5+

			else if (_FwpmNetEventSubscribe3)
				rc = _FwpmNetEventSubscribe3 (hengine, &subscription, &_wfp_logcallback3, nullptr, &config.hnetevent); // win10rs4+

			else if (_FwpmNetEventSubscribe2)
				rc = _FwpmNetEventSubscribe2 (hengine, &subscription, &_wfp_logcallback2, nullptr, &config.hnetevent); // win10rs1+

			else if (_FwpmNetEventSubscribe1)
				rc = _FwpmNetEventSubscribe1 (hengine, &subscription, &_wfp_logcallback1, nullptr, &config.hnetevent); // win8+

			else if (_FwpmNetEventSubscribe0)
				rc = _FwpmNetEventSubscribe0 (hengine, &subscription, &_wfp_logcallback0, nullptr, &config.hnetevent); // win7+

			else
				rc = ERROR_INVALID_FUNCTION;

			if (rc != ERROR_SUCCESS)
			{
				app.LogError (L"FwpmNetEventSubscribe", rc, nullptr, 0);
			}
			else
			{
				_app_loginit (true); // create log file
			}
		}

		FreeLibrary (hlib);
	}
}

void _wfp_logunsubscribe (HANDLE hengine)
{
	if (!hengine)
		return;

	if (config.hnetevent)
	{
		_app_loginit (false); // destroy log file handle if present

		const DWORD rc = FwpmNetEventUnsubscribe (hengine, config.hnetevent);

		if (rc == ERROR_SUCCESS)
			config.hnetevent = nullptr;
	}
}

void CALLBACK _wfp_logcallback (UINT32 flags, FILETIME const* pft, UINT8 const* app_id, SID* package_id, SID* user_id, UINT8 proto, FWP_IP_VERSION ipver, UINT32 remote_addr4, FWP_BYTE_ARRAY16 const* remote_addr6, UINT16 remote_port, UINT32 local_addr4, FWP_BYTE_ARRAY16 const* local_addr6, UINT16 local_port, UINT16 layer_id, UINT64 filter_id, UINT32 direction, bool is_allow, bool is_loopback)
{
	const HANDLE hengine = _wfp_getenginehandle ();

	if (!hengine || !filter_id || !layer_id || _wfp_isfiltersapplying () || (is_allow && app.ConfigGet (L"IsExcludeClassifyAllow", true).AsBool ()))
		return;

	// set allowed directions
	switch (direction)
	{
		case FWP_DIRECTION_IN:
		case FWP_DIRECTION_INBOUND:
		case FWP_DIRECTION_OUT:
		case FWP_DIRECTION_OUTBOUND:
		{
			break;
		}

		default:
		{
			return;
		}
	}

	// do not parse when tcp connection has been established, or when non-tcp traffic has been authorized
	{
		FWPM_LAYER *layer = nullptr;

		if (FwpmLayerGetById (hengine, layer_id, &layer) == ERROR_SUCCESS && layer)
		{
			if (RtlEqualMemory (&layer->layerKey, &FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4, sizeof (GUID)) || RtlEqualMemory (&layer->layerKey, &FWPM_LAYER_ALE_FLOW_ESTABLISHED_V6, sizeof (GUID)))
			{
				FwpmFreeMemory ((void **)&layer);
				return;
			}
			else if (RtlEqualMemory (&layer->layerKey, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, sizeof (GUID)) || RtlEqualMemory (&layer->layerKey, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, sizeof (GUID)))
			{
				direction = FWP_DIRECTION_INBOUND; // HACK!!! (issue #581)
			}

			FwpmFreeMemory ((void**)&layer);
		}
	}

	// get filter information
	bool is_myprovider = false;

	rstring filter_name;
	rstring provider_name;

	UINT8 filter_weight = 0;

	{
		FWPM_FILTER *ptr_filter = nullptr;
		FWPM_PROVIDER *ptr_provider = nullptr;

		if (FwpmFilterGetById (hengine, filter_id, &ptr_filter) == ERROR_SUCCESS && ptr_filter)
		{
			filter_name = !_r_str_isempty (ptr_filter->displayData.description) ? ptr_filter->displayData.description : ptr_filter->displayData.name;

			if (ptr_filter->weight.type == FWP_UINT8)
				filter_weight = ptr_filter->weight.uint8;

			if (ptr_filter->providerKey)
			{
				if (RtlEqualMemory (ptr_filter->providerKey, &GUID_WfpProvider, sizeof (GUID)))
					is_myprovider = true;

				if (FwpmProviderGetByKey (hengine, ptr_filter->providerKey, &ptr_provider) == ERROR_SUCCESS && ptr_provider)
					provider_name = !_r_str_isempty (ptr_provider->displayData.name) ? ptr_provider->displayData.name : ptr_provider->displayData.description;
			}
		}

		if (ptr_filter)
			FwpmFreeMemory ((void**)&ptr_filter);

		if (ptr_provider)
			FwpmFreeMemory ((void**)&ptr_provider);

		// prevent filter "not found" items
		if (filter_name.IsEmpty () && provider_name.IsEmpty ())
			return;
	}

	PITEM_LIST_ENTRY ptr_entry = (PITEM_LIST_ENTRY)_aligned_malloc (sizeof (ITEM_LIST_ENTRY), MEMORY_ALLOCATION_ALIGNMENT);

	if (ptr_entry)
	{
		PITEM_LOG ptr_log = new ITEM_LOG;

		// get package id (win8+)
		rstring sidstring;

		if ((flags & FWPM_NET_EVENT_FLAG_PACKAGE_ID_SET) != 0 && package_id)
		{
			sidstring = _r_str_fromsid (package_id);

			if (sidstring.IsEmpty () || !_app_item_get (DataAppUWP, _r_str_hash (sidstring), nullptr, nullptr, nullptr, nullptr))
				sidstring.Release ();
		}

		// copy converted nt device path into win32
		if ((flags & FWPM_NET_EVENT_FLAG_PACKAGE_ID_SET) != 0 && !sidstring.IsEmpty ())
		{
			_r_str_alloc (&ptr_log->path, sidstring.GetLength (), sidstring);
			ptr_log->app_hash = _r_str_hash (sidstring);
		}
		else if ((flags & FWPM_NET_EVENT_FLAG_APP_ID_SET) != 0 && app_id)
		{
			const rstring path = _r_path_dospathfromnt (LPCWSTR (app_id));

			if (!path.IsEmpty ())
			{
				_r_str_alloc (&ptr_log->path, path.GetLength (), path);
				ptr_log->app_hash = _r_str_hash (path);
			}
		}
		else
		{
			ptr_log->app_hash = 0;
		}

		// copy date and time
		if (pft)
			ptr_log->timestamp = _r_unixtime_from_filetime (pft);

		// get username information
		if ((flags & FWPM_NET_EVENT_FLAG_USER_ID_SET) != 0 && user_id)
		{
			SID_NAME_USE sid_type;

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

		// indicates the direction of the packet transmission
		switch (direction)
		{
			case FWP_DIRECTION_IN:
			case FWP_DIRECTION_INBOUND:
			{
				ptr_log->direction = FWP_DIRECTION_INBOUND;
				break;
			}

			case FWP_DIRECTION_OUT:
			case FWP_DIRECTION_OUTBOUND:
			{
				ptr_log->direction = FWP_DIRECTION_OUTBOUND;
				break;
			}
		}

		// destination
		if ((flags & FWPM_NET_EVENT_FLAG_IP_VERSION_SET) != 0)
		{
			if (ipver == FWP_IP_VERSION_V4)
			{
				ptr_log->af = AF_INET;

				// remote address
				if ((flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET) != 0 && remote_addr4)
					ptr_log->remote_addr.S_un.S_addr = _byteswap_ulong (remote_addr4);

				// local address
				if ((flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET) != 0 && local_addr4)
					ptr_log->local_addr.S_un.S_addr = _byteswap_ulong (local_addr4);
			}
			else if (ipver == FWP_IP_VERSION_V6)
			{
				ptr_log->af = AF_INET6;

				// remote address
				if ((flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET) != 0 && remote_addr6)
					RtlCopyMemory (ptr_log->remote_addr6.u.Byte, remote_addr6->byteArray16, FWP_V6_ADDR_SIZE);

				// local address
				if ((flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET) != 0 && local_addr6)
					RtlCopyMemory (ptr_log->local_addr6.u.Byte, local_addr6->byteArray16, FWP_V6_ADDR_SIZE);
			}

			if ((flags & FWPM_NET_EVENT_FLAG_REMOTE_PORT_SET) != 0 && remote_port)
				ptr_log->remote_port = remote_port;

			if ((flags & FWPM_NET_EVENT_FLAG_LOCAL_PORT_SET) != 0 && local_port)
				ptr_log->local_port = local_port;
		}

		// protocol
		if ((flags & FWPM_NET_EVENT_FLAG_IP_PROTOCOL_SET) != 0)
			ptr_log->protocol = proto;

		// indicates FWPM_NET_EVENT_TYPE_CLASSIFY_ALLOW state
		ptr_log->is_allow = is_allow;

		// indicates whether the packet originated from (or was heading to) the loopback adapter
		ptr_log->is_loopback = is_loopback;

		// set filter information
		ptr_log->filter_id = filter_id;

		ptr_log->is_myprovider = is_myprovider;

		_r_str_alloc (&ptr_log->filter_name, filter_name.GetLength (), filter_name);
		_r_str_alloc (&ptr_log->provider_name, provider_name.GetLength (), provider_name);

		ptr_log->is_blocklist = (filter_weight == FILTER_WEIGHT_BLOCKLIST);
		ptr_log->is_system = (filter_weight == FILTER_WEIGHT_HIGHEST) || (filter_weight == FILTER_WEIGHT_HIGHEST_IMPORTANT);
		ptr_log->is_custom = (filter_weight == FILTER_WEIGHT_CUSTOM) || (filter_weight == FILTER_WEIGHT_CUSTOM_BLOCK);

		// push into a slist
		{
			ptr_entry->Body = _r_obj_allocate (ptr_log, &_app_dereferencelog);

			RtlInterlockedPushEntrySList (&log_stack.ListHead, &ptr_entry->ListEntry);
			InterlockedIncrement (&log_stack.item_count);

			// check if thread has been terminated
			const LONG thread_count = InterlockedCompareExchange (&log_stack.thread_count, 0, 0);

			if (!thread_count || !_r_fastlock_islocked (&lock_logthread))
			{
				_app_freethreadpool (threads_pool);

				const HANDLE hthread = _r_sys_createthread (&LogThread, app.GetHWND (), true, THREAD_PRIORITY_HIGHEST);

				if (hthread)
				{
					InterlockedIncrement (&log_stack.thread_count);

					threads_pool.push_back (hthread);

					NtResumeThread (hthread, nullptr);
				}
			}
		}
	}
}

// win7+ callback
void CALLBACK _wfp_logcallback0 (LPVOID, const FWPM_NET_EVENT1* pEvent)
{
	UINT16 layer_id;
	UINT64 filter_id;
	UINT32 direction;
	bool is_loopback;

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
		is_loopback = false;
	}
	else
	{
		return;
	}

	_wfp_logcallback (pEvent->header.flags, &pEvent->header.timeStamp, pEvent->header.appId.data, nullptr, pEvent->header.userId, pEvent->header.ipProtocol, pEvent->header.ipVersion, pEvent->header.remoteAddrV4, &pEvent->header.remoteAddrV6, pEvent->header.remotePort, pEvent->header.localAddrV4, &pEvent->header.localAddrV6, pEvent->header.localPort, layer_id, filter_id, direction, false, is_loopback);
}

// win8+ callback
void CALLBACK _wfp_logcallback1 (LPVOID, const FWPM_NET_EVENT2* pEvent)
{
	UINT16 layer_id;
	UINT64 filter_id;
	UINT32 direction;

	bool is_loopback;
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
		is_loopback = false;
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

// win10rs1+ callback
void CALLBACK _wfp_logcallback2 (LPVOID, const FWPM_NET_EVENT3* pEvent)
{
	UINT16 layer_id;
	UINT64 filter_id;
	UINT32 direction;

	bool is_loopback;
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
		is_loopback = false;
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

// win10rs4+ callback
void CALLBACK _wfp_logcallback3 (LPVOID, const FWPM_NET_EVENT4* pEvent)
{
	UINT16 layer_id;
	UINT64 filter_id;
	UINT32 direction;

	bool is_loopback;
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
		is_loopback = false;
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

// win10rs5+ callback
void CALLBACK _wfp_logcallback4 (LPVOID, const FWPM_NET_EVENT5* pEvent)
{
	UINT16 layer_id;
	UINT64 filter_id;
	UINT32 direction;

	bool is_loopback;
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
		is_loopback = false;
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

NTSTATUS WINAPI LogThread (LPVOID lparam)
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
		PR_OBJECT ptr_log_object = ptr_entry->Body;

		_aligned_free (ptr_entry);

		if (!ptr_log_object)
			continue;

		PITEM_LOG ptr_log = (PITEM_LOG)ptr_log_object->pdata;

		if (!ptr_log)
		{
			_r_obj_dereference (ptr_log_object);
			continue;
		}

		const bool is_logenabled = app.ConfigGet (L"IsLogEnabled", false).AsBool ();
		const bool is_loguienabled = app.ConfigGet (L"IsLogUiEnabled", true).AsBool ();
		const bool is_notificationenabled = app.ConfigGet (L"IsNotificationsEnabled", true).AsBool ();

		const bool is_exludestealth = (!(ptr_log->is_system && app.ConfigGet (L"IsExcludeStealth", true).AsBool ()));

		// apps collector
		const bool is_notexist = ptr_log->app_hash && !_r_str_isempty (ptr_log->path) && !ptr_log->is_allow && !_app_isappfound (ptr_log->app_hash);

		if (is_notexist)
		{
			_r_fastlock_acquireshared (&lock_logbusy);
			ptr_log->app_hash = _app_addapplication (hwnd, ptr_log->path, 0, 0, 0, false, false);
			_r_fastlock_releaseshared (&lock_logbusy);

			const INT app_listview_id = (INT)_app_getappinfo (ptr_log->app_hash, InfoListviewId);

			if (app_listview_id && app_listview_id == (INT)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT))
			{
				_app_listviewsort (hwnd, app_listview_id);
				_app_refreshstatus (hwnd, app_listview_id);
			}

			_app_profile_save ();
		}

		// made network name resolution
		{
			_r_fastlock_acquireshared (&lock_logbusy);

			LPWSTR buffer = nullptr;

			_app_formataddress (ptr_log->af, ptr_log->protocol, &ptr_log->remote_addr, 0, &buffer, FMTADDR_RESOLVE_HOST);
			_app_formataddress (ptr_log->af, ptr_log->protocol, &ptr_log->local_addr, 0, &buffer, FMTADDR_RESOLVE_HOST);

			SAFE_DELETE_ARRAY (buffer);

			_r_fastlock_releaseshared (&lock_logbusy);
		}

		if ((is_logenabled || is_loguienabled || is_notificationenabled) && is_exludestealth)
		{
			// write log to a file
			if (is_logenabled)
				_app_logwrite (ptr_log);

			// only for my own provider
			if (ptr_log->is_myprovider)
			{
				// write log to a ui
				if (is_loguienabled)
					_app_logwrite_ui (hwnd, ptr_log_object);

				// display notification
				if (is_notificationenabled && ptr_log->app_hash && !ptr_log->is_allow)
				{
					if (!(ptr_log->is_blocklist && app.ConfigGet (L"IsExcludeBlocklist", true).AsBool ()) && !(ptr_log->is_custom && app.ConfigGet (L"IsExcludeCustomRules", true).AsBool ()))
					{
						PR_OBJECT ptr_app_object = _app_getappitem (ptr_log->app_hash);

						if (ptr_app_object)
						{
							PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

							if (ptr_app)
							{
								if (!ptr_app->is_silent)
									_app_notifyadd (config.hnotification, _r_obj_reference (ptr_log_object), ptr_app);
							}

							_r_obj_dereference (ptr_app_object);
						}
					}
				}
			}
		}

		_r_obj_dereference (ptr_log_object);
	}

	_r_fastlock_releaseshared (&lock_logthread);

	InterlockedDecrement (&log_stack.thread_count);

	return _r_sys_endthread (ERROR_SUCCESS);
}
