// simplewall
// Copyright (c) 2016-2021 Henry++

#include "global.h"

PR_STRING _app_getlogviewer ()
{
	LPCWSTR result = _r_config_getstring (L"LogViewer", LOG_VIEWER_DEFAULT);

	if (!_r_str_isempty (result))
		return _r_str_expandenvironmentstring (result);

	return _r_str_expandenvironmentstring (LOG_VIEWER_DEFAULT);
}

VOID _app_loginit (BOOLEAN is_install)
{
	HANDLE current_handle;
	HANDLE new_handle;
	PR_STRING log_path;

	current_handle = InterlockedCompareExchangePointer (&config.hlogfile, NULL, config.hlogfile);

	// reset log handle
	if (_r_fs_isvalidhandle (current_handle))
		CloseHandle (current_handle);

	if (!is_install || !_r_config_getboolean (L"IsLogEnabled", FALSE))
		return; // already closed or not enabled

	log_path = _r_str_expandenvironmentstring (_r_config_getstring (L"LogPath", LOG_PATH_DEFAULT));

	if (!log_path)
		return;

	new_handle = CreateFile (log_path->buffer, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (_r_fs_isvalidhandle (new_handle))
	{
		if (GetLastError () != ERROR_ALREADY_EXISTS)
		{
			ULONG written;
			BYTE bom[] = {0xFF, 0xFE};

			WriteFile (new_handle, bom, sizeof (bom), &written, NULL); // write utf-16 le byte order mask
		}
		else
		{
			_r_fs_setpos (new_handle, 0, FILE_END);
		}

		InterlockedCompareExchangePointer (&config.hlogfile, new_handle, NULL);
	}


	_r_obj_dereference (log_path);
}

VOID _app_logwrite (PITEM_LOG ptr_log)
{
	PR_STRING path = NULL;
	WCHAR date_string[256];
	PR_STRING local_address_string;
	PR_STRING local_port_string;
	PR_STRING remote_address_string;
	PR_STRING remote_port_string;
	PR_STRING direction_string;
	PR_STRING buffer;

	// parse path
	{
		PITEM_APP ptr_app = _r_obj_findhashtable (apps, ptr_log->app_hash);

		if (ptr_app)
		{
			if (ptr_app->type == DataAppUWP || ptr_app->type == DataAppService)
			{
				if (!_r_obj_isstringempty (ptr_app->real_path))
				{
					path = _r_obj_reference (ptr_app->real_path);
				}
				else if (!_r_obj_isstringempty (ptr_app->display_name))
				{
					path = _r_obj_reference (ptr_app->display_name);
				}
			}
			else if (!_r_obj_isstringempty (ptr_app->original_path))
			{
				path = _r_obj_reference (ptr_app->original_path);
			}
		}
	}

	_r_format_unixtimeex (date_string, RTL_NUMBER_OF (date_string), ptr_log->timestamp, FDTF_SHORTDATE | FDTF_SHORTTIME);

	local_address_string = _app_formataddress (ptr_log->af, 0, &ptr_log->local_addr, 0, FMTADDR_RESOLVE_HOST);
	remote_address_string = _app_formataddress (ptr_log->af, 0, &ptr_log->remote_addr, 0, FMTADDR_RESOLVE_HOST);

	local_port_string = _app_formatport (ptr_log->local_port, TRUE);
	remote_port_string = _app_formatport (ptr_log->remote_port, TRUE);

	direction_string = _app_getdirectionname (ptr_log->direction, ptr_log->is_loopback, FALSE);

	buffer = _r_format_string (SZ_LOG_BODY,
							   date_string,
							   _r_obj_getstringordefault (ptr_log->username, SZ_EMPTY),
							   _r_obj_getstringordefault (path, SZ_EMPTY),
							   _r_obj_getstringordefault (local_address_string, SZ_EMPTY),
							   _r_obj_getstringordefault (local_port_string, SZ_EMPTY),
							   _r_obj_getstringordefault (remote_address_string, SZ_EMPTY),
							   _r_obj_getstringordefault (remote_port_string, SZ_EMPTY),
							   _app_getprotoname (ptr_log->protocol, ptr_log->af, SZ_UNKNOWN),
							   _r_obj_getstringorempty (ptr_log->provider_name),
							   _r_obj_getstringorempty (ptr_log->filter_name),
							   ptr_log->filter_id,
							   _r_obj_getstringordefault (direction_string, SZ_EMPTY),
							   (ptr_log->is_allow ? SZ_STATE_ALLOW : SZ_STATE_BLOCK)
	);

	if (_app_logislimitreached ())
		_app_logclear ();

	ULONG written;

	if (_r_fs_getsize (config.hlogfile) == 2)
		WriteFile (config.hlogfile, SZ_LOG_TITLE, (ULONG)(_r_str_length (SZ_LOG_TITLE) * sizeof (WCHAR)), &written, NULL); // adds csv header

	if (buffer)
		WriteFile (config.hlogfile, buffer->buffer, (ULONG)buffer->length, &written, NULL);

	SAFE_DELETE_REFERENCE (local_address_string);
	SAFE_DELETE_REFERENCE (local_port_string);
	SAFE_DELETE_REFERENCE (remote_address_string);
	SAFE_DELETE_REFERENCE (remote_port_string);
	SAFE_DELETE_REFERENCE (direction_string);
	SAFE_DELETE_REFERENCE (path);
	SAFE_DELETE_REFERENCE (buffer);
}

BOOLEAN _app_logisexists (HWND hwnd, PITEM_LOG ptr_log_new)
{
	BOOLEAN is_duplicate_found = FALSE;

	for (SIZE_T i = 0; i < _r_obj_getlistsize (log_arr); i++)
	{
		PITEM_LOG ptr_log = _r_obj_referencesafe (_r_obj_getlistitem (log_arr, i));

		if (!ptr_log)
			continue;

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
					is_duplicate_found = TRUE;
				}
			}
			else if (ptr_log->af == AF_INET6)
			{
				if (
					RtlEqualMemory (ptr_log->remote_addr6.u.Byte, ptr_log_new->remote_addr6.u.Byte, FWP_V6_ADDR_SIZE) &&
					RtlEqualMemory (ptr_log->local_addr6.u.Byte, ptr_log_new->local_addr6.u.Byte, FWP_V6_ADDR_SIZE)
					)
				{
					is_duplicate_found = TRUE;
				}
			}

			if (is_duplicate_found)
			{
				ptr_log_new->timestamp = ptr_log->timestamp; // upd date

				_r_obj_dereference (ptr_log);

				return TRUE;
			}
		}

		_r_obj_dereference (ptr_log);
	}

	return FALSE;
}

VOID _app_logwrite_ui (HWND hwnd, PITEM_LOG ptr_log)
{
	WCHAR index_string[128];
	PITEM_APP ptr_app;
	PR_STRING local_address_string;
	PR_STRING local_port_string;
	PR_STRING remote_address_string;
	PR_STRING remote_port_string;
	PR_STRING direction_string;
	SIZE_T index;
	INT listview_id;
	INT item_id;

	if (_app_logisexists (hwnd, ptr_log))
		return;

	ptr_app = _r_obj_findhashtable (apps, ptr_log->app_hash);

	listview_id = IDC_LOG;
	index = _r_obj_addlistitem (log_arr, _r_obj_reference (ptr_log));

	local_address_string = _app_formataddress (ptr_log->af, 0, &ptr_log->local_addr, 0, 0);
	remote_address_string = _app_formataddress (ptr_log->af, 0, &ptr_log->remote_addr, 0, 0);

	local_port_string = _app_formatport (ptr_log->local_port, TRUE);
	remote_port_string = _app_formatport (ptr_log->remote_port, TRUE);

	direction_string = _app_getdirectionname (ptr_log->direction, ptr_log->is_loopback, FALSE);

	item_id = _r_listview_getitemcount (hwnd, listview_id);

	_r_listview_additemex (hwnd, listview_id, item_id, 0, ptr_app ? _app_getdisplayname (ptr_app, TRUE) : SZ_EMPTY, ptr_app ? PtrToInt (_app_getappinfo (ptr_app, InfoIconId)) : config.icon_id, I_GROUPIDNONE, index);
	_r_listview_setitem (hwnd, listview_id, item_id, 1, _r_obj_getstringordefault (local_address_string, SZ_EMPTY));
	_r_listview_setitem (hwnd, listview_id, item_id, 3, _r_obj_getstringordefault (remote_address_string, SZ_EMPTY));
	_r_listview_setitem (hwnd, listview_id, item_id, 5, _app_getprotoname (ptr_log->protocol, ptr_log->af, SZ_EMPTY));

	if (local_port_string)
		_r_listview_setitem (hwnd, listview_id, item_id, 2, _r_obj_getstringorempty (local_port_string));

	if (remote_port_string)
		_r_listview_setitem (hwnd, listview_id, item_id, 4, _r_obj_getstringorempty (remote_port_string));

	_r_listview_setitem (hwnd, listview_id, item_id, 6, _r_obj_getstringordefault (ptr_log->filter_name, SZ_EMPTY));
	_r_listview_setitem (hwnd, listview_id, item_id, 7, _r_obj_getstringorempty (direction_string));
	_r_listview_setitem (hwnd, listview_id, item_id, 8, ptr_log->is_allow ? SZ_STATE_ALLOW : SZ_STATE_BLOCK);

	_r_str_printf (index_string, RTL_NUMBER_OF (index_string), L"%" PR_SIZE_T, index);
	_r_listview_setitem (hwnd, listview_id, item_id, 9, index_string);

	SAFE_DELETE_REFERENCE (local_address_string);
	SAFE_DELETE_REFERENCE (remote_address_string);
	SAFE_DELETE_REFERENCE (local_port_string);
	SAFE_DELETE_REFERENCE (remote_port_string);
	SAFE_DELETE_REFERENCE (direction_string);

	if (listview_id == (INT)_r_tab_getlparam (hwnd, IDC_TAB, -1))
	{
		_app_listviewresize (hwnd, listview_id, FALSE);
		_app_listviewsort (hwnd, listview_id, -1, FALSE);
	}
}

BOOLEAN _app_logislimitreached ()
{
	LONG64 limit = _r_config_getlong64 (L"LogSizeLimitKb", LOG_SIZE_LIMIT_DEFAULT);

	if (!limit || !_r_fs_isvalidhandle (config.hlogfile))
		return FALSE;

	return (_r_fs_getsize (config.hlogfile) >= (_r_calc_kilobytes2bytes64 (limit)));
}

VOID _app_logclear ()
{
	HANDLE current_handle = InterlockedCompareExchangePointer (&config.hlogfile, NULL, NULL);

	if (_r_fs_isvalidhandle (current_handle))
	{
		_r_fs_setpos (current_handle, 2, FILE_BEGIN);

		SetEndOfFile (current_handle);

		FlushFileBuffers (current_handle);
	}
	else
	{
		PR_STRING log_path = _r_str_expandenvironmentstring (_r_config_getstring (L"LogPath", LOG_PATH_DEFAULT));

		if (log_path)
		{
			_r_fs_remove (log_path->buffer, PR_FLAG_REMOVE_FORCE);

			_r_obj_dereference (log_path);
		}
	}
}

VOID _app_logclear_ui (HWND hwnd)
{
	SendDlgItemMessage (hwnd, IDC_LOG, LVM_DELETEALLITEMS, 0, 0);
	//SendDlgItemMessage (hwnd, IDC_LOG, LVM_SETITEMCOUNT, 0, 0);

	_r_obj_clearlist (log_arr);
}

VOID _wfp_logsubscribe (HANDLE hengine)
{
	FWPMNES4 _FwpmNetEventSubscribe4;
	FWPMNES3 _FwpmNetEventSubscribe3;
	FWPMNES2 _FwpmNetEventSubscribe2;
	FWPMNES1 _FwpmNetEventSubscribe1;
	FWPMNES0 _FwpmNetEventSubscribe0;
	FWPM_NET_EVENT_SUBSCRIPTION subscription;
	FWPM_NET_EVENT_ENUM_TEMPLATE enum_template;
	HANDLE current_handle;
	HMODULE hfwpuclnt;
	HANDLE hevent;
	ULONG code;

	current_handle = InterlockedCompareExchangePointer (&config.hnetevent, NULL, config.hnetevent);

	if (current_handle)
		return; // already subscribed

	hfwpuclnt = LoadLibraryEx (L"fwpuclnt.dll", NULL, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);

	if (!hfwpuclnt)
	{
		_r_log (Warning, 0, L"LoadLibraryEx", GetLastError (), L"fwpuclnt.dll");
		return;
	}

	_FwpmNetEventSubscribe4 = (FWPMNES4)GetProcAddress (hfwpuclnt, "FwpmNetEventSubscribe4");
	_FwpmNetEventSubscribe3 = (FWPMNES3)GetProcAddress (hfwpuclnt, "FwpmNetEventSubscribe3");
	_FwpmNetEventSubscribe2 = (FWPMNES2)GetProcAddress (hfwpuclnt, "FwpmNetEventSubscribe2");
	_FwpmNetEventSubscribe1 = (FWPMNES1)GetProcAddress (hfwpuclnt, "FwpmNetEventSubscribe1");
	_FwpmNetEventSubscribe0 = (FWPMNES0)GetProcAddress (hfwpuclnt, "FwpmNetEventSubscribe0");

	if (!_FwpmNetEventSubscribe4 && !_FwpmNetEventSubscribe3 && !_FwpmNetEventSubscribe2 && !_FwpmNetEventSubscribe1 && !_FwpmNetEventSubscribe0)
	{
		_r_log (Warning, 0, L"GetProcAddress", GetLastError (), L"FwpmNetEventSubscribe");

		goto CleanupExit; // there is no function to call
	}

	RtlSecureZeroMemory (&subscription, sizeof (subscription));
	RtlSecureZeroMemory (&enum_template, sizeof (enum_template));

	subscription.enumTemplate = &enum_template;
	hevent = NULL;

	if (_FwpmNetEventSubscribe4)
		code = _FwpmNetEventSubscribe4 (hengine, &subscription, &_wfp_logcallback4, NULL, &hevent); // win10rs5+

	else if (_FwpmNetEventSubscribe3)
		code = _FwpmNetEventSubscribe3 (hengine, &subscription, &_wfp_logcallback3, NULL, &hevent); // win10rs4+

	else if (_FwpmNetEventSubscribe2)
		code = _FwpmNetEventSubscribe2 (hengine, &subscription, &_wfp_logcallback2, NULL, &hevent); // win10rs1+

	else if (_FwpmNetEventSubscribe1)
		code = _FwpmNetEventSubscribe1 (hengine, &subscription, &_wfp_logcallback1, NULL, &hevent); // win8+

	else if (_FwpmNetEventSubscribe0)
		code = _FwpmNetEventSubscribe0 (hengine, &subscription, &_wfp_logcallback0, NULL, &hevent); // win7+

	if (code != ERROR_SUCCESS)
	{
		_r_log (Warning, 0, L"FwpmNetEventSubscribe", code, NULL);
	}
	else
	{
		InterlockedCompareExchangePointer (&config.hnetevent, hevent, NULL);

		// initialize log file
		if (_r_config_getboolean (L"IsLogEnabled", FALSE))
			_app_loginit (TRUE);
	}

CleanupExit:

	FreeLibrary (hfwpuclnt);
}

VOID _wfp_logunsubscribe (HANDLE hengine)
{
	HANDLE current_handle = InterlockedCompareExchangePointer (&config.hnetevent, NULL, config.hnetevent);

	if (!current_handle)
		return;

	_app_loginit (FALSE); // destroy log file handle if present

	ULONG code = FwpmNetEventUnsubscribe (hengine, current_handle);

	if (code != ERROR_SUCCESS)
		_r_log (Warning, 0, L"FwpmNetEventUnsubscribe", code, NULL);
}

VOID CALLBACK _wfp_logcallback (UINT32 flags, const FILETIME* pft, UINT8 const* app_id, SID* package_id, SID* user_id, UINT8 proto, FWP_IP_VERSION ipver, UINT32 remote_addr4, FWP_BYTE_ARRAY16 const* remote_addr6, UINT16 remote_port, UINT32 local_addr4, FWP_BYTE_ARRAY16 const* local_addr6, UINT16 local_port, UINT16 layer_id, UINT64 filter_id, UINT32 direction, BOOLEAN is_allow, BOOLEAN is_loopback)
{
	HANDLE hengine = _wfp_getenginehandle ();

	if (!hengine || !filter_id || !layer_id || _wfp_isfiltersapplying () || (is_allow && _r_config_getboolean (L"IsExcludeClassifyAllow", TRUE)))
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
		FWPM_LAYER *layer;

		if (FwpmLayerGetById (hengine, layer_id, &layer) == ERROR_SUCCESS)
		{
			if (layer)
			{
				if (RtlEqualMemory (&layer->layerKey, &FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4, sizeof (GUID)) || RtlEqualMemory (&layer->layerKey, &FWPM_LAYER_ALE_FLOW_ESTABLISHED_V6, sizeof (GUID)))
				{
					FwpmFreeMemory ((PVOID*)&layer);
					return;
				}
				else if (RtlEqualMemory (&layer->layerKey, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, sizeof (GUID)) || RtlEqualMemory (&layer->layerKey, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, sizeof (GUID)))
				{
					direction = FWP_DIRECTION_INBOUND; // HACK!!! (issue #581)
				}

				FwpmFreeMemory ((PVOID*)&layer);
			}
		}
	}

	// get filter information
	BOOLEAN is_myprovider = FALSE;

	PR_STRING filter_name = NULL;
	PR_STRING provider_name = NULL;

	UINT8 filter_weight = 0;

	{
		FWPM_FILTER *ptr_filter = NULL;
		FWPM_PROVIDER *ptr_provider = NULL;

		if (FwpmFilterGetById (hengine, filter_id, &ptr_filter) == ERROR_SUCCESS && ptr_filter)
		{
			if (!_r_str_isempty (ptr_filter->displayData.description))
				filter_name = _r_obj_createstring (ptr_filter->displayData.description);

			else if (!_r_str_isempty (ptr_filter->displayData.name))
				filter_name = _r_obj_createstring (ptr_filter->displayData.name);

			if (ptr_filter->weight.type == FWP_UINT8)
				filter_weight = ptr_filter->weight.uint8;

			if (ptr_filter->providerKey)
			{
				if (RtlEqualMemory (ptr_filter->providerKey, &GUID_WfpProvider, sizeof (GUID)))
					is_myprovider = TRUE;

				if (FwpmProviderGetByKey (hengine, ptr_filter->providerKey, &ptr_provider) == ERROR_SUCCESS && ptr_provider)
				{
					if (!_r_str_isempty (ptr_provider->displayData.name))
						provider_name = _r_obj_createstring (ptr_provider->displayData.name);

					else if (!_r_str_isempty (ptr_provider->displayData.description))
						provider_name = _r_obj_createstring (ptr_provider->displayData.description);
				}
			}
		}

		if (ptr_filter)
			FwpmFreeMemory ((PVOID*)&ptr_filter);

		if (ptr_provider)
			FwpmFreeMemory ((PVOID*)&ptr_provider);

		// prevent filter "not found" items
		if (!filter_name && !provider_name)
			return;
	}

	PITEM_LOG_LISTENTRY ptr_entry = _aligned_malloc (sizeof (ITEM_LOG_LISTENTRY), MEMORY_ALLOCATION_ALIGNMENT);

	if (!ptr_entry)
		return;

	PITEM_LOG ptr_log = _r_obj_allocateex (sizeof (ITEM_LOG), &_app_dereferencelog);

	// get package id (win8+)
	PR_STRING sid_string = NULL;

	if ((flags & FWPM_NET_EVENT_FLAG_PACKAGE_ID_SET) != 0 && package_id)
	{
		sid_string = _r_str_fromsid (package_id);

		if (sid_string)
		{
			if (!_r_obj_findhashtable (apps, _r_obj_getstringhash (sid_string)))
				_r_obj_clearreference (&sid_string);
		}
	}

	// copy converted nt device path into win32
	if ((flags & FWPM_NET_EVENT_FLAG_PACKAGE_ID_SET) != 0 && sid_string)
	{
		_r_obj_movereference (&ptr_log->path, sid_string);
		sid_string = NULL;

		ptr_log->app_hash = _r_obj_getstringhash (ptr_log->path);
	}
	else if ((flags & FWPM_NET_EVENT_FLAG_APP_ID_SET) != 0 && app_id)
	{
		PR_STRING path = _r_path_dospathfromnt ((LPCWSTR)app_id);

		if (path)
		{
			_r_obj_movereference (&ptr_log->path, path);
			ptr_log->app_hash = _r_obj_getstringhash (ptr_log->path);
		}
	}
	else
	{
		ptr_log->app_hash = 0;
	}

	if (sid_string)
		_r_obj_clearreference (&sid_string);

	// copy date and time
	if (pft)
		ptr_log->timestamp = _r_unixtime_from_filetime (pft);

	// get username information
	if ((flags & FWPM_NET_EVENT_FLAG_USER_ID_SET) != 0 && user_id)
		ptr_log->username = _r_sys_getusernamefromsid (user_id);

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
				ptr_log->remote_addr.S_un.S_addr = _r_byteswap_ulong (remote_addr4);

			// local address
			if ((flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET) != 0 && local_addr4)
				ptr_log->local_addr.S_un.S_addr = _r_byteswap_ulong (local_addr4);
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

	ptr_log->filter_name = filter_name;
	ptr_log->provider_name = provider_name;

	ptr_log->is_blocklist = (filter_weight == FILTER_WEIGHT_BLOCKLIST);
	ptr_log->is_system = (filter_weight == FILTER_WEIGHT_HIGHEST) || (filter_weight == FILTER_WEIGHT_HIGHEST_IMPORTANT);
	ptr_log->is_custom = (filter_weight == FILTER_WEIGHT_CUSTOM) || (filter_weight == FILTER_WEIGHT_CUSTOM_BLOCK);

	// push into a singly linked list
	ptr_entry->body = ptr_log;

	RtlInterlockedPushEntrySList (&log_list_stack, &ptr_entry->list_entry);

	// check if thread has not exists
	if (!_r_spinlock_islocked (&lock_logthread))
	{
		_r_sys_createthreadex (&LogThread, _r_app_gethwnd (), NULL, THREAD_PRIORITY_HIGHEST);
	}
}

// win7+ callback
VOID CALLBACK _wfp_logcallback0 (PVOID context, const FWPM_NET_EVENT1* event)
{
	UINT16 layer_id;
	UINT64 filter_id;
	UINT32 direction;
	BOOLEAN is_loopback;

	if (event->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP && event->classifyDrop)
	{
		layer_id = event->classifyDrop->layerId;
		filter_id = event->classifyDrop->filterId;
		direction = event->classifyDrop->msFwpDirection;
		is_loopback = !!event->classifyDrop->isLoopback;
	}
	else if (event->type == FWPM_NET_EVENT_TYPE_IPSEC_KERNEL_DROP && event->ipsecDrop)
	{
		layer_id = event->ipsecDrop->layerId;
		filter_id = event->ipsecDrop->filterId;
		direction = event->ipsecDrop->direction;
		is_loopback = FALSE;
	}
	else
	{
		return;
	}

	_wfp_logcallback (event->header.flags,
					  &event->header.timeStamp,
					  event->header.appId.data,
					  NULL,
					  event->header.userId,
					  event->header.ipProtocol,
					  event->header.ipVersion,
					  event->header.remoteAddrV4,
					  &event->header.remoteAddrV6,
					  event->header.remotePort,
					  event->header.localAddrV4,
					  &event->header.localAddrV6,
					  event->header.localPort,
					  layer_id,
					  filter_id,
					  direction,
					  FALSE,
					  is_loopback
	);
}

// win8+ callback
VOID CALLBACK _wfp_logcallback1 (PVOID context, const FWPM_NET_EVENT2* event)
{
	UINT16 layer_id;
	UINT64 filter_id;
	UINT32 direction;

	BOOLEAN is_loopback;
	BOOLEAN is_allow = FALSE;

	if (event->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP && event->classifyDrop)
	{
		layer_id = event->classifyDrop->layerId;
		filter_id = event->classifyDrop->filterId;
		direction = event->classifyDrop->msFwpDirection;
		is_loopback = !!event->classifyDrop->isLoopback;
	}
	else if (event->type == FWPM_NET_EVENT_TYPE_IPSEC_KERNEL_DROP && event->ipsecDrop)
	{
		layer_id = event->ipsecDrop->layerId;
		filter_id = event->ipsecDrop->filterId;
		direction = event->ipsecDrop->direction;
		is_loopback = FALSE;
	}
	else if (event->type == FWPM_NET_EVENT_TYPE_CLASSIFY_ALLOW && event->classifyAllow)
	{
		layer_id = event->classifyAllow->layerId;
		filter_id = event->classifyAllow->filterId;
		direction = event->classifyAllow->msFwpDirection;
		is_loopback = !!event->classifyAllow->isLoopback;

		is_allow = TRUE;
	}
	else if (event->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP_MAC && event->classifyDropMac)
	{
		layer_id = event->classifyDropMac->layerId;
		filter_id = event->classifyDropMac->filterId;
		direction = event->classifyDropMac->msFwpDirection;
		is_loopback = !!event->classifyDropMac->isLoopback;
	}
	else
	{
		return;
	}

	_wfp_logcallback (event->header.flags,
					  &event->header.timeStamp,
					  event->header.appId.data,
					  event->header.packageSid,
					  event->header.userId,
					  event->header.ipProtocol,
					  event->header.ipVersion,
					  event->header.remoteAddrV4,
					  &event->header.remoteAddrV6,
					  event->header.remotePort,
					  event->header.localAddrV4,
					  &event->header.localAddrV6,
					  event->header.localPort,
					  layer_id,
					  filter_id,
					  direction,
					  is_allow,
					  is_loopback
	);
}

// win10rs1+ callback
VOID CALLBACK _wfp_logcallback2 (PVOID context, const FWPM_NET_EVENT3* event)
{
	UINT16 layer_id;
	UINT64 filter_id;
	UINT32 direction;

	BOOLEAN is_loopback;
	BOOLEAN is_allow = FALSE;

	if (event->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP && event->classifyDrop)
	{
		layer_id = event->classifyDrop->layerId;
		filter_id = event->classifyDrop->filterId;
		direction = event->classifyDrop->msFwpDirection;
		is_loopback = !!event->classifyDrop->isLoopback;
	}
	else if (event->type == FWPM_NET_EVENT_TYPE_IPSEC_KERNEL_DROP && event->ipsecDrop)
	{
		layer_id = event->ipsecDrop->layerId;
		filter_id = event->ipsecDrop->filterId;
		direction = event->ipsecDrop->direction;
		is_loopback = FALSE;
	}
	else if (event->type == FWPM_NET_EVENT_TYPE_CLASSIFY_ALLOW && event->classifyAllow)
	{
		layer_id = event->classifyAllow->layerId;
		filter_id = event->classifyAllow->filterId;
		direction = event->classifyAllow->msFwpDirection;
		is_loopback = !!event->classifyAllow->isLoopback;

		is_allow = TRUE;
	}
	else if (event->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP_MAC && event->classifyDropMac)
	{
		layer_id = event->classifyDropMac->layerId;
		filter_id = event->classifyDropMac->filterId;
		direction = event->classifyDropMac->msFwpDirection;
		is_loopback = !!event->classifyDropMac->isLoopback;
	}
	else
	{
		return;
	}

	_wfp_logcallback (event->header.flags,
					  &event->header.timeStamp,
					  event->header.appId.data,
					  event->header.packageSid,
					  event->header.userId,
					  event->header.ipProtocol,
					  event->header.ipVersion,
					  event->header.remoteAddrV4,
					  &event->header.remoteAddrV6,
					  event->header.remotePort,
					  event->header.localAddrV4,
					  &event->header.localAddrV6,
					  event->header.localPort,
					  layer_id,
					  filter_id,
					  direction,
					  is_allow,
					  is_loopback
	);
}

// win10rs4+ callback
VOID CALLBACK _wfp_logcallback3 (PVOID context, const FWPM_NET_EVENT4* event)
{
	UINT16 layer_id;
	UINT64 filter_id;
	UINT32 direction;

	BOOLEAN is_loopback;
	BOOLEAN is_allow = FALSE;

	if (event->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP && event->classifyDrop)
	{
		layer_id = event->classifyDrop->layerId;
		filter_id = event->classifyDrop->filterId;
		direction = event->classifyDrop->msFwpDirection;
		is_loopback = !!event->classifyDrop->isLoopback;
	}
	else if (event->type == FWPM_NET_EVENT_TYPE_IPSEC_KERNEL_DROP && event->ipsecDrop)
	{
		layer_id = event->ipsecDrop->layerId;
		filter_id = event->ipsecDrop->filterId;
		direction = event->ipsecDrop->direction;
		is_loopback = FALSE;
	}
	else if (event->type == FWPM_NET_EVENT_TYPE_CLASSIFY_ALLOW && event->classifyAllow)
	{
		layer_id = event->classifyAllow->layerId;
		filter_id = event->classifyAllow->filterId;
		direction = event->classifyAllow->msFwpDirection;
		is_loopback = !!event->classifyAllow->isLoopback;

		is_allow = TRUE;
	}
	else if (event->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP_MAC && event->classifyDropMac)
	{
		layer_id = event->classifyDropMac->layerId;
		filter_id = event->classifyDropMac->filterId;
		direction = event->classifyDropMac->msFwpDirection;
		is_loopback = !!event->classifyDropMac->isLoopback;
	}
	else
	{
		return;
	}

	_wfp_logcallback (event->header.flags,
					  &event->header.timeStamp,
					  event->header.appId.data,
					  event->header.packageSid,
					  event->header.userId,
					  event->header.ipProtocol,
					  event->header.ipVersion,
					  event->header.remoteAddrV4,
					  &event->header.remoteAddrV6,
					  event->header.remotePort,
					  event->header.localAddrV4,
					  &event->header.localAddrV6,
					  event->header.localPort,
					  layer_id,
					  filter_id,
					  direction,
					  is_allow,
					  is_loopback
	);
}

// win10rs5+ callback
VOID CALLBACK _wfp_logcallback4 (PVOID context, const FWPM_NET_EVENT5* event)
{
	UINT16 layer_id;
	UINT64 filter_id;
	UINT32 direction;

	BOOLEAN is_loopback;
	BOOLEAN is_allow = FALSE;

	if (event->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP && event->classifyDrop)
	{
		layer_id = event->classifyDrop->layerId;
		filter_id = event->classifyDrop->filterId;
		direction = event->classifyDrop->msFwpDirection;
		is_loopback = !!event->classifyDrop->isLoopback;
	}
	else if (event->type == FWPM_NET_EVENT_TYPE_IPSEC_KERNEL_DROP && event->ipsecDrop)
	{
		layer_id = event->ipsecDrop->layerId;
		filter_id = event->ipsecDrop->filterId;
		direction = event->ipsecDrop->direction;
		is_loopback = FALSE;
	}
	else if (event->type == FWPM_NET_EVENT_TYPE_CLASSIFY_ALLOW && event->classifyAllow)
	{
		layer_id = event->classifyAllow->layerId;
		filter_id = event->classifyAllow->filterId;
		direction = event->classifyAllow->msFwpDirection;
		is_loopback = !!event->classifyAllow->isLoopback;

		is_allow = TRUE;
	}
	else if (event->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP_MAC && event->classifyDropMac)
	{
		layer_id = event->classifyDropMac->layerId;
		filter_id = event->classifyDropMac->filterId;
		direction = event->classifyDropMac->msFwpDirection;
		is_loopback = !!event->classifyDropMac->isLoopback;
	}
	else
	{
		return;
	}

	_wfp_logcallback (event->header.flags,
					  &event->header.timeStamp,
					  event->header.appId.data,
					  event->header.packageSid,
					  event->header.userId,
					  event->header.ipProtocol,
					  event->header.ipVersion,
					  event->header.remoteAddrV4,
					  &event->header.remoteAddrV6,
					  event->header.remotePort,
					  event->header.localAddrV4,
					  &event->header.localAddrV6,
					  event->header.localPort,
					  layer_id,
					  filter_id,
					  direction,
					  is_allow,
					  is_loopback
	);
}

THREAD_API LogThread (PVOID lparam)
{
	HWND hwnd = (HWND)lparam;

	_r_spinlock_acquireshared (&lock_logthread);

	while (TRUE)
	{
		PSLIST_ENTRY list_item = RtlInterlockedPopEntrySList (&log_list_stack);

		if (!list_item)
			break;

		PITEM_LOG_LISTENTRY ptr_entry = CONTAINING_RECORD (list_item, ITEM_LOG_LISTENTRY, list_entry);
		PITEM_LOG ptr_log = ptr_entry->body;

		_aligned_free (ptr_entry);

		if (!ptr_log)
			continue;

		BOOLEAN is_logenabled = _r_config_getboolean (L"IsLogEnabled", FALSE);
		BOOLEAN is_loguienabled = _r_config_getboolean (L"IsLogUiEnabled", FALSE);
		BOOLEAN is_notificationenabled = _r_config_getboolean (L"IsNotificationsEnabled", TRUE);

		BOOLEAN is_exludestealth = !(ptr_log->is_system && _r_config_getboolean (L"IsExcludeStealth", TRUE));

		// apps collector
		BOOLEAN is_notexist = ptr_log->app_hash && !_r_obj_isstringempty (ptr_log->path) && !ptr_log->is_allow && !_r_obj_findhashtable (apps, ptr_log->app_hash);

		if (is_notexist)
		{
			_r_spinlock_acquireshared (&lock_logbusy);
			PITEM_APP ptr_app = _app_addapplication (hwnd, DataUnknown, ptr_log->path->buffer, NULL, NULL);
			_r_spinlock_releaseshared (&lock_logbusy);

			if (ptr_app)
				ptr_log->app_hash = ptr_app->app_hash;

			INT app_listview_id = PtrToInt (_app_getappinfobyhash (ptr_log->app_hash, InfoListviewId));

			if (app_listview_id && app_listview_id == (INT)_r_tab_getlparam (hwnd, IDC_TAB, -1))
			{
				_app_listviewsort (hwnd, app_listview_id, -1, FALSE);
				_app_refreshstatus (hwnd, app_listview_id);
			}

			_app_profile_save ();
		}

		// made network name resolution
		{
			_r_spinlock_acquireshared (&lock_logbusy);

			PR_STRING address_string;

			address_string = _app_formataddress (ptr_log->af, ptr_log->protocol, &ptr_log->remote_addr, 0, FMTADDR_RESOLVE_HOST);
			SAFE_DELETE_REFERENCE (address_string);

			address_string = _app_formataddress (ptr_log->af, ptr_log->protocol, &ptr_log->local_addr, 0, FMTADDR_RESOLVE_HOST);
			SAFE_DELETE_REFERENCE (address_string);

			_r_spinlock_releaseshared (&lock_logbusy);
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
					_app_logwrite_ui (hwnd, ptr_log);

				// display notification
				if (is_notificationenabled && ptr_log->app_hash && !ptr_log->is_allow)
				{
					if (!(ptr_log->is_blocklist && _r_config_getboolean (L"IsExcludeBlocklist", TRUE)) && !(ptr_log->is_custom && _r_config_getboolean (L"IsExcludeCustomRules", TRUE)))
					{
						PITEM_APP ptr_app = _r_obj_findhashtable (apps, ptr_log->app_hash);

						if (ptr_app)
						{
							if (!PtrToInt (_app_getappinfo (ptr_app, InfoIsSilent)))
								_app_notifyadd (config.hnotification, _r_obj_reference (ptr_log), ptr_app);
						}
					}
				}
			}
		}

		_r_obj_dereference (ptr_log);
	}

	_r_spinlock_releaseshared (&lock_logthread);

	return ERROR_SUCCESS;
}
