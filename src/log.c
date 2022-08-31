// simplewall
// Copyright (c) 2016-2022 Henry++

#include "global.h"

VOID _app_loginit (
	_In_ BOOLEAN is_install
)
{
	HANDLE current_handle;
	HANDLE new_handle;
	PR_STRING log_path;

	current_handle = InterlockedCompareExchangePointer (
		&config.hlogfile,
		NULL,
		config.hlogfile
	);

	// reset log handle
	if (current_handle)
		NtClose (current_handle);

	if (!is_install || !_r_config_getboolean (L"IsLogEnabled", FALSE))
		return; // already closed or not enabled

	log_path = _app_getlogpath ();

	if (!log_path)
		return;

	new_handle = CreateFile (
		log_path->buffer,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if (_r_fs_isvalidhandle (new_handle))
	{
		_app_loginitfile (new_handle);

		current_handle = InterlockedCompareExchangePointer (
			&config.hlogfile,
			new_handle,
			NULL
		);

		if (current_handle)
			NtClose (new_handle);
	}

	_r_obj_dereference (log_path);
}

VOID _app_loginitfile (
	_In_ HANDLE hfile
)
{
	BYTE bom[] = {0xFF, 0xFE};
	LONG64 file_size;
	ULONG unused;

	file_size = _r_fs_getsize (hfile);

	if (!file_size)
	{
		// write utf-16 le byte order mask
		WriteFile (hfile, bom, sizeof (bom), &unused, NULL);

		// write csv header
		WriteFile (
			hfile,
			SZ_LOG_TITLE,
			(ULONG)(_r_str_getlength (SZ_LOG_TITLE) * sizeof (WCHAR)),
			&unused,
			NULL
		);
	}
	else
	{
		// move to eof
		_r_fs_setpos (hfile, file_size);
	}
}

ULONG_PTR _app_getloghash (
	_In_ HWND hwnd,
	_In_ PITEM_LOG ptr_log
)
{
	PR_STRING log_string;
	ULONG_PTR log_hash;

	UNREFERENCED_PARAMETER (hwnd);

	log_string = _r_format_string (
		L"%" TEXT (PRIu8) L"-%" TEXT (PR_ULONG_PTR) L"-%" TEXT (PRIu8) L"-%" \
		TEXT (PRIu8) L"-%" TEXT (PRIu16) L"-%" TEXT (PRIu16) L"-%s-%s",
		ptr_log->af,
		ptr_log->app_hash,
		ptr_log->protocol,
		ptr_log->direction,
		ptr_log->local_port,
		ptr_log->remote_port,
		_r_obj_getstring (ptr_log->local_addr_str),
		_r_obj_getstring (ptr_log->remote_addr_str)
	);

	log_hash = _r_str_gethash2 (log_string, TRUE);

	_r_obj_dereference (log_string);

	return log_hash;
}

PR_STRING _app_getlogpath ()
{
	PR_STRING path;

	path = _r_config_getstringexpand (L"LogPath", LOG_PATH_DEFAULT);

	return path;
}

PR_STRING _app_getlogviewer ()
{
	PR_STRING path;

	path = _r_config_getstringexpand (L"LogViewer", LOG_VIEWER_DEFAULT);

	return path;
}

BOOLEAN _app_islogfound (
	_In_ ULONG_PTR log_hash
)
{
	BOOLEAN is_found;

	_r_queuedlock_acquireshared (&lock_loglist);

	is_found = (_r_obj_findhashtable (log_table, log_hash) != NULL);

	_r_queuedlock_releaseshared (&lock_loglist);

	return is_found;
}

BOOLEAN _app_logislimitreached (
	_In_ HANDLE hfile
)
{
	LONG64 limit;
	LONG64 file_size;

	limit = _r_config_getlong64 (L"LogSizeLimitKb", LOG_SIZE_LIMIT_DEFAULT);

	if (!limit)
		return FALSE;

	file_size = _r_fs_getsize (hfile);

	return (file_size >= (_r_calc_kilobytes2bytes64 (limit)));
}

VOID _app_logclear (
	_In_opt_ HANDLE hfile
)
{
	PR_STRING log_path;

	if (_r_fs_isvalidhandle (hfile))
	{
		_r_fs_clearfile (hfile);
		_app_loginitfile (hfile);
	}

	log_path = _app_getlogpath ();

	if (log_path)
	{
		_r_fs_deletefile (log_path->buffer, TRUE);

		_r_obj_dereference (log_path);
	}
}

VOID _app_logclear_ui (
	_In_ HWND hwnd
)
{
	SendDlgItemMessage (hwnd, IDC_LOG, LVM_DELETEALLITEMS, 0, 0);
	//SendDlgItemMessage (hwnd, IDC_LOG, LVM_SETITEMCOUNT, 0, 0);

	_app_listview_resize (hwnd, IDC_LOG);

	_r_queuedlock_acquireexclusive (&lock_loglist);

	_r_obj_clearhashtable (log_table);

	_r_queuedlock_releaseexclusive (&lock_loglist);
}

VOID _app_logwrite (
	_In_ PITEM_LOG ptr_log
)
{
	PR_STRING path;
	PR_STRING date_string;
	PR_STRING local_port_string;
	PR_STRING remote_port_string;
	PR_STRING direction_string;
	PR_STRING buffer;
	PITEM_APP ptr_app;
	HANDLE current_handle;
	ULONG unused;

	current_handle = InterlockedCompareExchangePointer (
		&config.hlogfile,
		NULL,
		NULL
	);

	if (!current_handle)
		return;

	// parse path
	ptr_app = _app_getappitem (ptr_log->app_hash);

	if (ptr_app)
	{
		path = _app_getappname (ptr_app);

		_r_obj_dereference (ptr_app);
	}
	else
	{
		path = NULL;
	}

	date_string = _r_format_unixtime_ex (ptr_log->timestamp, FDTF_SHORTDATE | FDTF_LONGTIME);

	local_port_string = _app_formatport (ptr_log->local_port, ptr_log->protocol);
	remote_port_string = _app_formatport (ptr_log->remote_port, ptr_log->protocol);

	direction_string = _app_db_getdirectionname (ptr_log->direction, ptr_log->is_loopback, FALSE);

	buffer = _r_format_string (
		SZ_LOG_BODY,
		_r_obj_getstringordefault (date_string, SZ_EMPTY),
		_r_obj_getstringordefault (ptr_log->username, SZ_EMPTY),
		_r_obj_getstringordefault (path, SZ_EMPTY),
		_r_obj_getstringordefault (ptr_log->local_addr_str, SZ_EMPTY),
		local_port_string->buffer,
		_r_obj_getstringordefault (ptr_log->remote_addr_str, SZ_EMPTY),
		remote_port_string->buffer,
		_r_obj_getstringordefault (ptr_log->protocol_str, SZ_DIRECTION_ANY),
		_r_obj_getstringordefault (ptr_log->layer_name, SZ_EMPTY),
		_r_obj_getstringordefault (ptr_log->filter_name, SZ_EMPTY),
		ptr_log->filter_id,
		_r_obj_getstringordefault (direction_string, SZ_EMPTY),
		(ptr_log->is_allow ? SZ_STATE_ALLOW : SZ_STATE_BLOCK)
	);

	if (_app_logislimitreached (current_handle))
		_app_logclear (current_handle);

	WriteFile (
		current_handle,
		buffer->buffer,
		(ULONG)buffer->length,
		&unused,
		NULL
	);

	if (date_string)
		_r_obj_dereference (date_string);

	if (direction_string)
		_r_obj_dereference (direction_string);

	if (path)
		_r_obj_dereference (path);

	_r_obj_dereference (local_port_string);
	_r_obj_dereference (remote_port_string);

	_r_obj_dereference (buffer);
}

VOID _app_logwrite_ui (
	_In_ HWND hwnd,
	_In_ PITEM_LOG ptr_log
)
{
	ULONG_PTR log_hash;
	ULONG_PTR hash_code;
	SIZE_T table_size;
	INT item_id;

	log_hash = _app_getloghash (hwnd, ptr_log);

	if (!log_hash)
		return;

	// log was already in list, so skip it...
	if (_app_islogfound (log_hash))
		return;

	_r_queuedlock_acquireshared (&lock_loglist);
	table_size = _r_obj_gethashtablesize (log_table);
	_r_queuedlock_releaseshared (&lock_loglist);

	if (table_size >= MAP_CACHE_MAX)
	{
		item_id = _r_listview_getitemcount (hwnd, IDC_LOG) - 1;

		hash_code = _app_listview_getitemcontext (hwnd, IDC_LOG, item_id);
		_r_listview_deleteitem (hwnd, IDC_LOG, item_id);

		_r_queuedlock_acquireexclusive (&lock_loglist);
		_r_obj_removehashtablepointer (log_table, hash_code);
		_r_queuedlock_releaseexclusive (&lock_loglist);
	}

	_r_queuedlock_acquireexclusive (&lock_loglist);
	_r_obj_addhashtablepointer (log_table, log_hash, _r_obj_reference (ptr_log));
	_r_queuedlock_releaseexclusive (&lock_loglist);

	_app_listview_addlogitem (hwnd, ptr_log, log_hash);

	_app_queue_resolver (hwnd, IDC_LOG, log_hash, ptr_log);

	_app_listview_updateby_id (hwnd, IDC_LOG, 0);
}

VOID _wfp_logsubscribe (
	_In_ HANDLE engine_handle
)
{
	FWPMNES4 _FwpmNetEventSubscribe4;
	FWPMNES3 _FwpmNetEventSubscribe3;
	FWPMNES2 _FwpmNetEventSubscribe2;
	FWPMNES1 _FwpmNetEventSubscribe1;
	FWPMNES0 _FwpmNetEventSubscribe0;
	FWPM_NET_EVENT_SUBSCRIPTION subscription;
	FWPM_NET_EVENT_ENUM_TEMPLATE enum_template;
	HANDLE current_handle;
	HANDLE new_handle;
	HMODULE hfwpuclnt;
	ULONG code;

	current_handle = InterlockedCompareExchangePointer (
		&config.hnetevent,
		NULL,
		NULL
	);

	if (current_handle)
		return; // already subscribed

	hfwpuclnt = _r_sys_loadlibrary (L"fwpuclnt.dll");

	if (!hfwpuclnt)
	{
		_r_log (LOG_LEVEL_WARNING, NULL, L"_r_sys_loadlibrary", GetLastError (), L"fwpuclnt.dll");

		return;
	}

	_FwpmNetEventSubscribe4 = (FWPMNES4)GetProcAddress (hfwpuclnt, "FwpmNetEventSubscribe4");
	_FwpmNetEventSubscribe3 = (FWPMNES3)GetProcAddress (hfwpuclnt, "FwpmNetEventSubscribe3");
	_FwpmNetEventSubscribe2 = (FWPMNES2)GetProcAddress (hfwpuclnt, "FwpmNetEventSubscribe2");
	_FwpmNetEventSubscribe1 = (FWPMNES1)GetProcAddress (hfwpuclnt, "FwpmNetEventSubscribe1");
	_FwpmNetEventSubscribe0 = (FWPMNES0)GetProcAddress (hfwpuclnt, "FwpmNetEventSubscribe0");

	if (!_FwpmNetEventSubscribe4 &&
		!_FwpmNetEventSubscribe3 &&
		!_FwpmNetEventSubscribe2 &&
		!_FwpmNetEventSubscribe1 &&
		!_FwpmNetEventSubscribe0)
	{
		_r_log (LOG_LEVEL_WARNING, NULL, L"GetProcAddress", GetLastError (), L"FwpmNetEventSubscribe");

		goto CleanupExit; // there is no function to call
	}

	RtlZeroMemory (&subscription, sizeof (subscription));
	RtlZeroMemory (&enum_template, sizeof (enum_template));

	subscription.enumTemplate = &enum_template;
	new_handle = NULL;

	if (_FwpmNetEventSubscribe4)
	{
		code = _FwpmNetEventSubscribe4 (
			engine_handle,
			&subscription,
			&_wfp_logcallback4,
			NULL,
			&new_handle); // win10rs5+
	}
	else if (_FwpmNetEventSubscribe3)
	{
		code = _FwpmNetEventSubscribe3 (
			engine_handle,
			&subscription,
			&_wfp_logcallback3,
			NULL,
			&new_handle); // win10rs4+
	}
	else if (_FwpmNetEventSubscribe2)
	{
		code = _FwpmNetEventSubscribe2 (
			engine_handle,
			&subscription,
			&_wfp_logcallback2,
			NULL,
			&new_handle); // win10rs1+
	}
	else if (_FwpmNetEventSubscribe1)
	{
		code = _FwpmNetEventSubscribe1 (
			engine_handle,
			&subscription,
			&_wfp_logcallback1,
			NULL,
			&new_handle); // win8+
	}
	else if (_FwpmNetEventSubscribe0)
	{
		code = _FwpmNetEventSubscribe0 (
			engine_handle,
			&subscription,
			&_wfp_logcallback0,
			NULL,
			&new_handle); // win7+
	}
	else
	{
		goto CleanupExit;
	}

	if (code != ERROR_SUCCESS)
		_r_log (LOG_LEVEL_WARNING, NULL, L"FwpmNetEventSubscribe", code, NULL);

	current_handle = InterlockedCompareExchangePointer (
		&config.hnetevent,
		new_handle,
		NULL
	);

	if (current_handle)
	{
		if (new_handle)
			FwpmNetEventUnsubscribe (engine_handle, new_handle);
	}

	// initialize log file
	if (_r_config_getboolean (L"IsLogEnabled", FALSE))
		_app_loginit (TRUE);

CleanupExit:

	FreeLibrary (hfwpuclnt);
}

VOID _wfp_logunsubscribe (
	_In_ HANDLE engine_handle
)
{
	HANDLE current_handle;
	ULONG code;

	current_handle = InterlockedCompareExchangePointer (
		&config.hnetevent,
		NULL,
		config.hnetevent
	);

	if (current_handle)
	{
		code = FwpmNetEventUnsubscribe (engine_handle, current_handle);

		if (code != ERROR_SUCCESS)
			_r_log (LOG_LEVEL_WARNING, NULL, L"FwpmNetEventUnsubscribe", code, NULL);
	}

	_app_loginit (FALSE); // destroy log file handle if present
}

VOID _wfp_logsetoption (
	_In_ HANDLE engine_handle
)
{
	FWP_VALUE val;
	UINT32 mask;
	ULONG code;

	if (!config.is_neteventset)
		return;

	// configure dropped packets logging (win8+)
	if (!_r_sys_isosversiongreaterorequal (WINDOWS_8))
		return;

	mask = 0;

	// add allowed connections monitor
	if (!_r_config_getboolean (L"IsExcludeClassifyAllow", TRUE))
		mask |= FWPM_NET_EVENT_KEYWORD_CLASSIFY_ALLOW;

	// add inbound multicast and broadcast connections monitor
	if (!_r_config_getboolean (L"IsExcludeInbound", FALSE))
		mask |= FWPM_NET_EVENT_KEYWORD_INBOUND_MCAST | FWPM_NET_EVENT_KEYWORD_INBOUND_BCAST;

	// add port scanning drop connections monitor (1903+)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_10_1903))
	{
		if (!_r_config_getboolean (L"IsExcludePortScanningDrop", FALSE))
			mask |= FWPM_NET_EVENT_KEYWORD_PORT_SCANNING_DROP;
	}

	// the filter engine will collect wfp network events that match any supplied key words
	val.type = FWP_UINT32;
	val.uint32 = mask;

	code = FwpmEngineSetOption (
		engine_handle,
		FWPM_ENGINE_NET_EVENT_MATCH_ANY_KEYWORDS,
		&val
	);

	if (code != ERROR_SUCCESS)
	{
		_r_log (
			LOG_LEVEL_WARNING,
			NULL,
			L"FwpmEngineSetOption",
			code,
			L"FWPM_ENGINE_NET_EVENT_MATCH_ANY_KEYWORDS"
		);
	}

	// enables the connection monitoring feature and starts logging creation and
	// deletion events (and notifying any subscribers)
	val.type = FWP_UINT32;
	val.uint32 = !_r_config_getboolean (L"IsExcludeIPSecConnections", FALSE);

	code = FwpmEngineSetOption (
		engine_handle,
		FWPM_ENGINE_MONITOR_IPSEC_CONNECTIONS,
		&val
	);

	if (code != ERROR_SUCCESS)
	{
		_r_log (
			LOG_LEVEL_WARNING,
			NULL,
			L"FwpmEngineSetOption",
			code,
			L"FWPM_ENGINE_MONITOR_IPSEC_CONNECTIONS"
		);
	}
}

VOID CALLBACK _wfp_logcallback (
	_In_ PITEM_LOG_CALLBACK log
)
{
	HANDLE engine_handle;
	PITEM_LOG ptr_log;
	PR_STRING path;
	PR_STRING resolved_path;
	PR_STRING filter_name;
	PR_STRING layer_name;
	PR_STRING sid_string;
	FWPM_LAYER *layer_ptr;
	FWPM_FILTER *filter_ptr;
	UINT8 filter_weight;
	BOOLEAN is_myprovider;
	NTSTATUS status;

	engine_handle = _wfp_getenginehandle ();

	if (!engine_handle || !log->filter_id || !log->layer_id)
		return;

	if (log->is_allow && _r_config_getboolean (L"IsExcludeClassifyAllow", TRUE))
		return;

	// do not parse when tcp connection has been established, or when non-tcp traffic has been authorized
	if (FwpmLayerGetById (engine_handle, log->layer_id, &layer_ptr) != ERROR_SUCCESS)
		return;

	if (!layer_ptr)
		return;

	if (IsEqualGUID (&layer_ptr->layerKey, &FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4) ||
		IsEqualGUID (&layer_ptr->layerKey, &FWPM_LAYER_ALE_FLOW_ESTABLISHED_V6))
	{
		FwpmFreeMemory ((PVOID_PTR)&layer_ptr);
		return;
	}

	if (IsEqualGUID (&layer_ptr->layerKey, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4) ||
		IsEqualGUID (&layer_ptr->layerKey, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6))
	{
		log->direction = FWP_DIRECTION_INBOUND; // HACK!!! (issue #581)
	}

	layer_name = _wfp_getlayername (&layer_ptr->layerKey);

	FwpmFreeMemory ((PVOID_PTR)&layer_ptr);

	// get filter information
	if (FwpmFilterGetById (engine_handle, log->filter_id, &filter_ptr) != ERROR_SUCCESS || !filter_ptr)
	{
		_r_obj_dereference (layer_name);
		return;
	}

	filter_name = NULL;
	is_myprovider = FALSE;
	filter_weight = 0;

	if (!_r_str_isempty (filter_ptr->displayData.description))
	{
		filter_name = _r_obj_createstring (filter_ptr->displayData.description);
	}
	else if (!_r_str_isempty (filter_ptr->displayData.name))
	{
		filter_name = _r_obj_createstring (filter_ptr->displayData.name);
	}

	if (filter_ptr->weight.type == FWP_UINT8)
		filter_weight = filter_ptr->weight.uint8;

	if (filter_ptr->providerKey)
	{
		if (IsEqualGUID (filter_ptr->providerKey, &GUID_WfpProvider))
			is_myprovider = TRUE;
	}

	FwpmFreeMemory ((PVOID_PTR)&filter_ptr);

	// allocate log object
	ptr_log = _r_obj_allocate (sizeof (ITEM_LOG), &_app_dereferencelog);

	// copy date and time
	if (log->timestamp)
		ptr_log->timestamp = _r_unixtime_from_filetime (log->timestamp);

	// check token for null (not an appcontainer) (HACK!!!)
	if (log->flags & FWPM_NET_EVENT_FLAG_PACKAGE_ID_SET && log->package_id)
	{
		if (RtlEqualSid (log->package_id, &SeNobodySid))
		{
			log->flags &= ~(FWPM_NET_EVENT_FLAG_PACKAGE_ID_SET);
			log->package_id = NULL;
		}
	}

	// get package id (win8+)
	if (log->flags & FWPM_NET_EVENT_FLAG_PACKAGE_ID_SET && log->package_id)
	{
		status = _r_str_fromsid (log->package_id, &sid_string);

		if (status == STATUS_SUCCESS)
		{
			if (!_app_package_isnotexists (sid_string, 0))
				_r_obj_clearreference (&sid_string);
		}
	}
	else
	{
		sid_string = NULL;
	}

	// copy converted nt device path into win32
	if (log->flags & FWPM_NET_EVENT_FLAG_PACKAGE_ID_SET && sid_string)
	{
		_r_obj_swapreference (&ptr_log->path, sid_string);
		ptr_log->app_hash = _r_str_gethash2 (ptr_log->path, TRUE);
	}
	else if (log->flags & FWPM_NET_EVENT_FLAG_APP_ID_SET && log->app_id)
	{
		path = _r_obj_createstring ((LPCWSTR)(log->app_id));

		resolved_path = _r_path_dospathfromnt (path);

		if (resolved_path)
			_r_obj_movereference (&path, resolved_path);

		if (path)
		{
			_r_obj_movereference (&ptr_log->path, path);
			ptr_log->app_hash = _r_str_gethash2 (ptr_log->path, TRUE);
		}
	}
	else
	{
		ptr_log->app_hash = 0;
	}

	// get username information
	if (log->flags & FWPM_NET_EVENT_FLAG_USER_ID_SET && log->user_id)
		_r_sys_getusernamefromsid (log->user_id, &ptr_log->username);

	// destination
	if (log->flags & FWPM_NET_EVENT_FLAG_IP_VERSION_SET)
	{
		if (log->version == FWP_IP_VERSION_V4)
		{
			ptr_log->af = AF_INET;

			// remote address
			if (log->flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET)
			{
				if (ULongLongToULong (log->remote_addr4, &ptr_log->remote_addr.S_un.S_addr) == S_OK)
				{
					ptr_log->remote_addr.S_un.S_addr = _r_byteswap_ulong (ptr_log->remote_addr.S_un.S_addr);
				}
			}

			// local address
			if (log->flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET)
			{
				if (ULongLongToULong (log->local_addr4, &ptr_log->local_addr.S_un.S_addr) == S_OK)
				{
					ptr_log->local_addr.S_un.S_addr = _r_byteswap_ulong (ptr_log->local_addr.S_un.S_addr);
				}
			}
		}
		else if (log->version == FWP_IP_VERSION_V6)
		{
			ptr_log->af = AF_INET6;

			// remote address
			if (log->flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET && log->remote_addr6)
			{
				RtlCopyMemory (
					ptr_log->remote_addr6.u.Byte,
					log->remote_addr6->byteArray16,
					FWP_V6_ADDR_SIZE
				);
			}

			// local address
			if (log->flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET && log->local_addr6)
			{
				RtlCopyMemory (
					ptr_log->local_addr6.u.Byte,
					log->local_addr6->byteArray16,
					FWP_V6_ADDR_SIZE
				);
			}
		}
	}

	// ports
	if (log->flags & FWPM_NET_EVENT_FLAG_LOCAL_PORT_SET)
		ptr_log->local_port = log->local_port;

	if (log->flags & FWPM_NET_EVENT_FLAG_REMOTE_PORT_SET)
		ptr_log->remote_port = log->remote_port;

	// protocol
	if (log->flags & FWPM_NET_EVENT_FLAG_IP_PROTOCOL_SET)
	{
		ptr_log->protocol = log->protocol;
		ptr_log->protocol_str = _app_db_getprotoname (ptr_log->protocol, ptr_log->af, FALSE);
	}

	// indicates FWPM_NET_EVENT_TYPE_CLASSIFY_ALLOW state
	ptr_log->is_allow = log->is_allow;

	// indicates whether the packet originated from (or was heading to) the loopback adapter
	ptr_log->is_loopback = log->is_loopback;

	// set filter information
	ptr_log->filter_id = log->filter_id;
	ptr_log->direction = log->direction;

	ptr_log->is_myprovider = is_myprovider;

	ptr_log->filter_name = filter_name;
	ptr_log->layer_name = layer_name;

	ptr_log->is_blocklist = (filter_weight == FW_WEIGHT_RULE_BLOCKLIST);
	ptr_log->is_system = (filter_weight == FW_WEIGHT_HIGHEST) || (filter_weight == FW_WEIGHT_HIGHEST_IMPORTANT);
	ptr_log->is_custom = (filter_weight == FW_WEIGHT_RULE_USER) || (filter_weight == FW_WEIGHT_RULE_USER_BLOCK);

	_r_workqueue_queueitem (&log_queue, &_app_logthread, ptr_log);

	if (sid_string)
		_r_obj_dereference (sid_string);
}

FORCEINLINE BOOLEAN log_struct_to_f (
	_Inout_ PITEM_LOG_CALLBACK log,
	_In_ PVOID event_data,
	_In_ ULONG version
)
{
	if (version == WINDOWS_7)
	{
		FWPM_NET_EVENT1 *evt = event_data;

		if (evt->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP && evt->classifyDrop)
		{
			log->layer_id = evt->classifyDrop->layerId;
			log->filter_id = evt->classifyDrop->filterId;
			log->direction = evt->classifyDrop->msFwpDirection;
			log->is_loopback = !!evt->classifyDrop->isLoopback;
		}
		else if (evt->type == FWPM_NET_EVENT_TYPE_IPSEC_KERNEL_DROP && evt->ipsecDrop)
		{
			log->layer_id = evt->ipsecDrop->layerId;
			log->filter_id = evt->ipsecDrop->filterId;
			log->direction = evt->ipsecDrop->direction;
		}
		else
		{
			return FALSE;
		}

		// indicates the direction of the packet transmission and set valid directions
		switch (log->direction)
		{
			case FWP_DIRECTION_IN:
			case FWP_DIRECTION_INBOUND:
			{
				log->direction = FWP_DIRECTION_INBOUND;
				break;
			}

			case FWP_DIRECTION_OUT:
			case FWP_DIRECTION_OUTBOUND:
			{
				log->direction = FWP_DIRECTION_OUTBOUND;
				break;
			}

			default:
			{
				return FALSE;
			}
		}

		log->flags = evt->header.flags;
		log->timestamp = &evt->header.timeStamp;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_APP_ID_SET)
			log->app_id = evt->header.appId.data;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_USER_ID_SET)
			log->user_id = evt->header.userId;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_IP_PROTOCOL_SET)
			log->protocol = evt->header.ipProtocol;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_LOCAL_PORT_SET)
			log->local_port = evt->header.localPort;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_REMOTE_PORT_SET)
			log->remote_port = evt->header.remotePort;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_IP_VERSION_SET)
		{
			log->version = evt->header.ipVersion;

			if (evt->header.ipVersion == FWP_IP_VERSION_V4)
			{
				if (evt->header.flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET)
					log->local_addr4 = evt->header.localAddrV4;

				if (evt->header.flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET)
					log->remote_addr4 = evt->header.remoteAddrV4;
			}
			else if (evt->header.ipVersion == FWP_IP_VERSION_V6)
			{
				if (evt->header.flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET)
					log->local_addr6 = &evt->header.localAddrV6;

				if (evt->header.flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET)
					log->remote_addr6 = &evt->header.remoteAddrV6;
			}
		}
		else
		{
			log->version = FWP_IP_VERSION_NONE;
		}
	}
	else if (version == WINDOWS_8)
	{
		FWPM_NET_EVENT2 *evt = event_data;

		if (evt->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP && evt->classifyDrop)
		{
			log->layer_id = evt->classifyDrop->layerId;
			log->filter_id = evt->classifyDrop->filterId;
			log->direction = evt->classifyDrop->msFwpDirection;
			log->is_loopback = !!evt->classifyDrop->isLoopback;
		}
		else if (evt->type == FWPM_NET_EVENT_TYPE_IPSEC_KERNEL_DROP && evt->ipsecDrop)
		{
			log->layer_id = evt->ipsecDrop->layerId;
			log->filter_id = evt->ipsecDrop->filterId;
			log->direction = evt->ipsecDrop->direction;
		}
		else if (evt->type == FWPM_NET_EVENT_TYPE_CLASSIFY_ALLOW && evt->classifyAllow)
		{
			log->layer_id = evt->classifyAllow->layerId;
			log->filter_id = evt->classifyAllow->filterId;
			log->direction = evt->classifyAllow->msFwpDirection;
			log->is_loopback = !!evt->classifyAllow->isLoopback;

			log->is_allow = TRUE;
		}
		else if (evt->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP_MAC && evt->classifyDropMac)
		{
			log->layer_id = evt->classifyDropMac->layerId;
			log->filter_id = evt->classifyDropMac->filterId;
			log->direction = evt->classifyDropMac->msFwpDirection;
			log->is_loopback = !!evt->classifyDropMac->isLoopback;
		}
		else
		{
			return FALSE;
		}

		// indicates the direction of the packet transmission and set valid directions
		switch (log->direction)
		{
			case FWP_DIRECTION_IN:
			case FWP_DIRECTION_INBOUND:
			{
				log->direction = FWP_DIRECTION_INBOUND;
				break;
			}

			case FWP_DIRECTION_OUT:
			case FWP_DIRECTION_OUTBOUND:
			{
				log->direction = FWP_DIRECTION_OUTBOUND;
				break;
			}

			default:
			{
				return FALSE;
			}
		}

		log->flags = evt->header.flags;
		log->timestamp = &evt->header.timeStamp;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_APP_ID_SET)
			log->app_id = evt->header.appId.data;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_PACKAGE_ID_SET)
			log->package_id = evt->header.packageSid;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_USER_ID_SET)
			log->user_id = evt->header.userId;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_IP_PROTOCOL_SET)
			log->protocol = evt->header.ipProtocol;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_LOCAL_PORT_SET)
			log->local_port = evt->header.localPort;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_REMOTE_PORT_SET)
			log->remote_port = evt->header.remotePort;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_IP_VERSION_SET)
		{
			log->version = evt->header.ipVersion;

			if (evt->header.ipVersion == FWP_IP_VERSION_V4)
			{
				if (evt->header.flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET)
					log->local_addr4 = evt->header.localAddrV4;

				if (evt->header.flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET)
					log->remote_addr4 = evt->header.remoteAddrV4;
			}
			else if (evt->header.ipVersion == FWP_IP_VERSION_V6)
			{
				if (evt->header.flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET)
					log->local_addr6 = &evt->header.localAddrV6;

				if (evt->header.flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET)
					log->remote_addr6 = &evt->header.remoteAddrV6;
			}
		}
		else
		{
			log->version = FWP_IP_VERSION_NONE;
		}
	}
	else if (version == WINDOWS_10_1607)
	{
		FWPM_NET_EVENT3 *evt = event_data;

		if (evt->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP && evt->classifyDrop)
		{
			log->layer_id = evt->classifyDrop->layerId;
			log->filter_id = evt->classifyDrop->filterId;
			log->direction = evt->classifyDrop->msFwpDirection;
			log->is_loopback = !!evt->classifyDrop->isLoopback;
		}
		else if (evt->type == FWPM_NET_EVENT_TYPE_IPSEC_KERNEL_DROP && evt->ipsecDrop)
		{
			log->layer_id = evt->ipsecDrop->layerId;
			log->filter_id = evt->ipsecDrop->filterId;
			log->direction = evt->ipsecDrop->direction;
		}
		else if (evt->type == FWPM_NET_EVENT_TYPE_CLASSIFY_ALLOW && evt->classifyAllow)
		{
			log->layer_id = evt->classifyAllow->layerId;
			log->filter_id = evt->classifyAllow->filterId;
			log->direction = evt->classifyAllow->msFwpDirection;
			log->is_loopback = !!evt->classifyAllow->isLoopback;

			log->is_allow = TRUE;
		}
		else if (evt->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP_MAC && evt->classifyDropMac)
		{
			log->layer_id = evt->classifyDropMac->layerId;
			log->filter_id = evt->classifyDropMac->filterId;
			log->direction = evt->classifyDropMac->msFwpDirection;
			log->is_loopback = !!evt->classifyDropMac->isLoopback;
		}
		else
		{
			return FALSE;
		}

		// indicates the direction of the packet transmission and set valid directions
		switch (log->direction)
		{
			case FWP_DIRECTION_IN:
			case FWP_DIRECTION_INBOUND:
			{
				log->direction = FWP_DIRECTION_INBOUND;
				break;
			}

			case FWP_DIRECTION_OUT:
			case FWP_DIRECTION_OUTBOUND:
			{
				log->direction = FWP_DIRECTION_OUTBOUND;
				break;
			}

			default:
			{
				return FALSE;
			}
		}

		log->flags = evt->header.flags;
		log->timestamp = &evt->header.timeStamp;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_APP_ID_SET)
			log->app_id = evt->header.appId.data;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_PACKAGE_ID_SET)
			log->package_id = evt->header.packageSid;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_USER_ID_SET)
			log->user_id = evt->header.userId;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_IP_PROTOCOL_SET)
			log->protocol = evt->header.ipProtocol;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_LOCAL_PORT_SET)
			log->local_port = evt->header.localPort;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_REMOTE_PORT_SET)
			log->remote_port = evt->header.remotePort;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_IP_VERSION_SET)
		{
			log->version = evt->header.ipVersion;

			if (evt->header.ipVersion == FWP_IP_VERSION_V4)
			{
				if (evt->header.flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET)
					log->local_addr4 = evt->header.localAddrV4;

				if (evt->header.flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET)
					log->remote_addr4 = evt->header.remoteAddrV4;
			}
			else if (evt->header.ipVersion == FWP_IP_VERSION_V6)
			{
				if (evt->header.flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET)
					log->local_addr6 = &evt->header.localAddrV6;

				if (evt->header.flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET)
					log->remote_addr6 = &evt->header.remoteAddrV6;
			}
		}
		else
		{
			log->version = FWP_IP_VERSION_NONE;
		}
	}
	else if (version == WINDOWS_10_1803)
	{
		FWPM_NET_EVENT4 *evt = event_data;

		if (evt->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP && evt->classifyDrop)
		{
			log->layer_id = evt->classifyDrop->layerId;
			log->filter_id = evt->classifyDrop->filterId;
			log->direction = evt->classifyDrop->msFwpDirection;
			log->is_loopback = !!evt->classifyDrop->isLoopback;
		}
		else if (evt->type == FWPM_NET_EVENT_TYPE_IPSEC_KERNEL_DROP && evt->ipsecDrop)
		{
			log->layer_id = evt->ipsecDrop->layerId;
			log->filter_id = evt->ipsecDrop->filterId;
			log->direction = evt->ipsecDrop->direction;
		}
		else if (evt->type == FWPM_NET_EVENT_TYPE_CLASSIFY_ALLOW && evt->classifyAllow)
		{
			log->layer_id = evt->classifyAllow->layerId;
			log->filter_id = evt->classifyAllow->filterId;
			log->direction = evt->classifyAllow->msFwpDirection;
			log->is_loopback = !!evt->classifyAllow->isLoopback;

			log->is_allow = TRUE;
		}
		else if (evt->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP_MAC && evt->classifyDropMac)
		{
			log->layer_id = evt->classifyDropMac->layerId;
			log->filter_id = evt->classifyDropMac->filterId;
			log->direction = evt->classifyDropMac->msFwpDirection;
			log->is_loopback = !!evt->classifyDropMac->isLoopback;
		}
		else
		{
			return FALSE;
		}

		// indicates the direction of the packet transmission and set valid directions
		switch (log->direction)
		{
			case FWP_DIRECTION_IN:
			case FWP_DIRECTION_INBOUND:
			{
				log->direction = FWP_DIRECTION_INBOUND;
				break;
			}

			case FWP_DIRECTION_OUT:
			case FWP_DIRECTION_OUTBOUND:
			{
				log->direction = FWP_DIRECTION_OUTBOUND;
				break;
			}

			default:
			{
				return FALSE;
			}
		}

		log->flags = evt->header.flags;
		log->timestamp = &evt->header.timeStamp;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_APP_ID_SET)
			log->app_id = evt->header.appId.data;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_PACKAGE_ID_SET)
			log->package_id = evt->header.packageSid;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_USER_ID_SET)
			log->user_id = evt->header.userId;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_IP_PROTOCOL_SET)
			log->protocol = evt->header.ipProtocol;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_LOCAL_PORT_SET)
			log->local_port = evt->header.localPort;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_REMOTE_PORT_SET)
			log->remote_port = evt->header.remotePort;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_IP_VERSION_SET)
		{
			log->version = evt->header.ipVersion;

			if (evt->header.ipVersion == FWP_IP_VERSION_V4)
			{
				if (evt->header.flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET)
					log->local_addr4 = evt->header.localAddrV4;

				if (evt->header.flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET)
					log->remote_addr4 = evt->header.remoteAddrV4;
			}
			else if (evt->header.ipVersion == FWP_IP_VERSION_V6)
			{
				if (evt->header.flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET)
					log->local_addr6 = &evt->header.localAddrV6;

				if (evt->header.flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET)
					log->remote_addr6 = &evt->header.remoteAddrV6;
			}
		}
		else
		{
			log->version = FWP_IP_VERSION_NONE;
		}
	}
	else if (version == WINDOWS_10_1809)
	{
		FWPM_NET_EVENT5 *evt = event_data;

		if (evt->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP && evt->classifyDrop)
		{
			log->layer_id = evt->classifyDrop->layerId;
			log->filter_id = evt->classifyDrop->filterId;
			log->direction = evt->classifyDrop->msFwpDirection;
			log->is_loopback = !!evt->classifyDrop->isLoopback;
		}
		else if (evt->type == FWPM_NET_EVENT_TYPE_IPSEC_KERNEL_DROP && evt->ipsecDrop)
		{
			log->layer_id = evt->ipsecDrop->layerId;
			log->filter_id = evt->ipsecDrop->filterId;
			log->direction = evt->ipsecDrop->direction;
		}
		else if (evt->type == FWPM_NET_EVENT_TYPE_CLASSIFY_ALLOW && evt->classifyAllow)
		{
			log->layer_id = evt->classifyAllow->layerId;
			log->filter_id = evt->classifyAllow->filterId;
			log->direction = evt->classifyAllow->msFwpDirection;
			log->is_loopback = !!evt->classifyAllow->isLoopback;

			log->is_allow = TRUE;
		}
		else if (evt->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP_MAC && evt->classifyDropMac)
		{
			log->layer_id = evt->classifyDropMac->layerId;
			log->filter_id = evt->classifyDropMac->filterId;
			log->direction = evt->classifyDropMac->msFwpDirection;
			log->is_loopback = !!evt->classifyDropMac->isLoopback;
		}
		else
		{
			return FALSE;
		}

		// indicates the direction of the packet transmission and set valid directions
		switch (log->direction)
		{
			case FWP_DIRECTION_IN:
			case FWP_DIRECTION_INBOUND:
			{
				log->direction = FWP_DIRECTION_INBOUND;
				break;
			}

			case FWP_DIRECTION_OUT:
			case FWP_DIRECTION_OUTBOUND:
			{
				log->direction = FWP_DIRECTION_OUTBOUND;
				break;
			}

			default:
			{
				return FALSE;
			}
		}

		log->flags = evt->header.flags;
		log->timestamp = &evt->header.timeStamp;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_APP_ID_SET)
			log->app_id = evt->header.appId.data;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_PACKAGE_ID_SET)
			log->package_id = evt->header.packageSid;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_USER_ID_SET)
			log->user_id = evt->header.userId;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_IP_PROTOCOL_SET)
			log->protocol = evt->header.ipProtocol;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_LOCAL_PORT_SET)
			log->local_port = evt->header.localPort;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_REMOTE_PORT_SET)
			log->remote_port = evt->header.remotePort;

		if (evt->header.flags & FWPM_NET_EVENT_FLAG_IP_VERSION_SET)
		{
			log->version = evt->header.ipVersion;

			if (evt->header.ipVersion == FWP_IP_VERSION_V4)
			{
				if (evt->header.flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET)
					log->local_addr4 = evt->header.localAddrV4;

				if (evt->header.flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET)
					log->remote_addr4 = evt->header.remoteAddrV4;
			}
			else if (evt->header.ipVersion == FWP_IP_VERSION_V6)
			{
				if (evt->header.flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET)
					log->local_addr6 = &evt->header.localAddrV6;

				if (evt->header.flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET)
					log->remote_addr6 = &evt->header.remoteAddrV6;
			}
		}
		else
		{
			log->version = FWP_IP_VERSION_NONE;
		}
	}

	return TRUE;
}

// win7+ callback
VOID CALLBACK _wfp_logcallback0 (
	_In_opt_ PVOID context,
	_In_ const FWPM_NET_EVENT1 *event_data
)
{
	ITEM_LOG_CALLBACK log = {0};

	UNREFERENCED_PARAMETER (context);

	if (log_struct_to_f (&log, (PVOID)event_data, WINDOWS_7))
		_wfp_logcallback (&log);
}

// win8+ callback
VOID CALLBACK _wfp_logcallback1 (
	_In_opt_ PVOID context,
	_In_ const FWPM_NET_EVENT2 *event_data
)
{
	ITEM_LOG_CALLBACK log = {0};

	UNREFERENCED_PARAMETER (context);

	if (log_struct_to_f (&log, (PVOID)event_data, WINDOWS_8))
		_wfp_logcallback (&log);
}

// win10rs1+ callback
VOID CALLBACK _wfp_logcallback2 (
	_In_opt_ PVOID context,
	_In_ const FWPM_NET_EVENT3 *event_data
)
{
	ITEM_LOG_CALLBACK log = {0};

	UNREFERENCED_PARAMETER (context);

	if (log_struct_to_f (&log, (PVOID)event_data, WINDOWS_10_1607))
		_wfp_logcallback (&log);
}

// win10rs4+ callback
VOID CALLBACK _wfp_logcallback3 (
	_In_opt_ PVOID context,
	_In_ const FWPM_NET_EVENT4 *event_data
)
{
	ITEM_LOG_CALLBACK log = {0};

	UNREFERENCED_PARAMETER (context);

	if (log_struct_to_f (&log, (PVOID)event_data, WINDOWS_10_1803))
		_wfp_logcallback (&log);
}

// win10rs5+ callback
VOID CALLBACK _wfp_logcallback4 (
	_In_opt_ PVOID context,
	_In_ const FWPM_NET_EVENT5 *event_data
)
{
	ITEM_LOG_CALLBACK log = {0};

	UNREFERENCED_PARAMETER (context);

	if (log_struct_to_f (&log, (PVOID)event_data, WINDOWS_10_1809))
		_wfp_logcallback (&log);
}

VOID NTAPI _app_logthread (
	_In_ PVOID arglist,
	_In_ ULONG busy_count
)
{
	HWND hwnd;

	PITEM_LOG ptr_log;
	PITEM_APP ptr_app;

	INT is_silent;

	BOOLEAN is_logenabled;
	BOOLEAN is_loguienabled;
	BOOLEAN is_notificationenabled;

	BOOLEAN is_exludeallow;
	BOOLEAN is_exludeblocklist;
	BOOLEAN is_exludestealth;
	BOOLEAN is_notexist;

	hwnd = _r_app_gethwnd ();

	ptr_log = arglist;
	ptr_app = NULL;

	// apps collector
	is_notexist = ptr_log->app_hash && !ptr_log->is_allow && !_app_isappfound (ptr_log->app_hash);

	if (is_notexist)
	{
		ptr_log->app_hash = _app_addapplication (hwnd, DATA_UNKNOWN, ptr_log->path, NULL, NULL);

		if (ptr_log->app_hash)
		{
			ptr_app = _app_getappitem (ptr_log->app_hash);

			if (ptr_app)
				_app_listview_updateby_id (hwnd, ptr_app->type, PR_UPDATE_TYPE);

			_app_profile_save ();
		}
	}

	if (!ptr_app)
		ptr_app = _app_getappitem (ptr_log->app_hash);

	is_logenabled = _r_config_getboolean (L"IsLogEnabled", FALSE);
	is_loguienabled = _r_config_getboolean (L"IsLogUiEnabled", FALSE);
	is_notificationenabled = _r_config_getboolean (L"IsNotificationsEnabled", TRUE);

	is_exludeallow = !(ptr_log->is_allow && _r_config_getboolean (L"IsExcludeClassifyAllow", TRUE));
	is_exludestealth = !(ptr_log->is_system && _r_config_getboolean (L"IsExcludeStealth", TRUE));

	is_exludeblocklist = !(ptr_log->is_blocklist && _r_config_getboolean (L"IsExcludeBlocklist", TRUE)) &&
		!(ptr_log->is_custom && _r_config_getboolean (L"IsExcludeCustomRules", TRUE));

	if ((is_logenabled || is_loguienabled || is_notificationenabled) && is_exludestealth && is_exludeallow)
	{
		// get network string
		ptr_log->remote_addr_str = _app_formataddress (
			ptr_log->af,
			ptr_log->protocol,
			&ptr_log->remote_addr,
			0,
			0
		);

		ptr_log->local_addr_str = _app_formataddress (
			ptr_log->af,
			ptr_log->protocol,
			&ptr_log->local_addr,
			0,
			0
		);

		// write log to a file
		if (is_logenabled)
			_app_logwrite (ptr_log);

		// only for my own provider
		if (ptr_log->is_myprovider)
		{
			// write log to a ui
			if (is_loguienabled)
				_app_logwrite_ui (hwnd, ptr_log);

			// display notification
			if (is_notificationenabled && ptr_app && !ptr_log->is_allow)
			{
				if (is_exludeblocklist)
				{
					if (_app_getappinfo (ptr_app, INFO_IS_SILENT, (PVOID_PTR)&is_silent) && !is_silent)
						_app_notify_addobject (ptr_log, ptr_app);
				}
			}
		}
	}

	if (ptr_app)
		_r_obj_dereference (ptr_app);

	_r_obj_dereference (ptr_log);
}
