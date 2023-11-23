// simplewall
// Copyright (c) 2016-2023 Henry++

#include "global.h"

BOOLEAN _wfp_isfiltersapplying ()
{
	return _r_queuedlock_islocked (&lock_apply) || _r_queuedlock_islocked (&lock_transaction);
}

ENUM_INSTALL_TYPE _wfp_isproviderinstalled (
	_In_ HANDLE engine_handle
)
{
	FWPM_PROVIDER *ptr_provider;
	ENUM_INSTALL_TYPE install_type;
	ULONG status;

	status = FwpmProviderGetByKey (engine_handle, &GUID_WfpProvider, &ptr_provider);

	if (status != ERROR_SUCCESS || !ptr_provider)
		return INSTALL_DISABLED;

	if (ptr_provider->flags & FWPM_PROVIDER_FLAG_DISABLED)
	{
		install_type = INSTALL_DISABLED;
	}
	else if (ptr_provider->flags & FWPM_PROVIDER_FLAG_PERSISTENT)
	{
		install_type = INSTALL_ENABLED;
	}
	else
	{
		install_type = INSTALL_ENABLED_TEMPORARY;
	}

	FwpmFreeMemory ((PVOID_PTR)&ptr_provider);

	return install_type;
}

ENUM_INSTALL_TYPE _wfp_issublayerinstalled (
	_In_ HANDLE engine_handle
)
{
	FWPM_SUBLAYER *ptr_sublayer;
	ENUM_INSTALL_TYPE install_type;
	ULONG status;

	status = FwpmSubLayerGetByKey (engine_handle, &GUID_WfpSublayer, &ptr_sublayer);

	if (status != ERROR_SUCCESS || !ptr_sublayer)
		return INSTALL_DISABLED;

	if (ptr_sublayer->flags & FWPM_SUBLAYER_FLAG_PERSISTENT)
	{
		install_type = INSTALL_ENABLED;
	}
	else
	{
		install_type = INSTALL_ENABLED_TEMPORARY;
	}

	FwpmFreeMemory ((PVOID_PTR)&ptr_sublayer);

	return install_type;
}

BOOLEAN _wfp_isfiltersinstalled ()
{
	ENUM_INSTALL_TYPE install_type;

	install_type = _wfp_getinstalltype ();

	return (install_type != INSTALL_DISABLED);
}

HANDLE _wfp_getenginehandle ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static HANDLE engine_handle = NULL;

	FWPM_SESSION session;
	ULONG status;
	ULONG attempts = 6;

	if (_r_initonce_begin (&init_once))
	{
		do
		{
			RtlZeroMemory (&session, sizeof (session));

			session.displayData.name = _r_app_getname ();
			session.displayData.description = _r_app_getname ();

			session.txnWaitTimeoutInMSec = TRANSACTION_TIMEOUT;

			status = FwpmEngineOpen (NULL, RPC_C_AUTHN_WINNT, NULL, &session, &engine_handle);

			if (status == ERROR_SUCCESS)
			{
				break;
			}
			else
			{
				if (status == EPT_S_NOT_REGISTERED)
				{
					// The error say that BFE service is not in the running state, so we wait.
					if (attempts)
					{
						_r_sys_sleep (500);
						attempts -= 1;

						continue;
					}
				}

				_r_log (LOG_LEVEL_CRITICAL, NULL, L"FwpmEngineOpen", status, NULL);

				_r_show_errormessage (_r_app_gethwnd (), L"WFP engine initialization failed! Try again later.", status, NULL, NULL, NULL);

				RtlExitUserProcess (status);

				break;
			}
		}
		while (--attempts);

		_r_initonce_end (&init_once);
	}

	return engine_handle;
}

ENUM_INSTALL_TYPE _wfp_getinstalltype ()
{
	HANDLE engine_handle;
	ENUM_INSTALL_TYPE install_type;

	engine_handle = _wfp_getenginehandle ();

	if (engine_handle)
	{
		install_type = _wfp_isproviderinstalled (engine_handle);

		return install_type;
	}

	return INSTALL_DISABLED;
}

PR_STRING _wfp_getlayername (
	_In_ LPGUID layer_guid
)
{
	static LPCGUID layer_guids[] = {
		&FWPM_LAYER_ALE_AUTH_CONNECT_V4,
		&FWPM_LAYER_ALE_AUTH_CONNECT_V6,
		&FWPM_LAYER_ALE_CONNECT_REDIRECT_V4,
		&FWPM_LAYER_ALE_CONNECT_REDIRECT_V6,
		&FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4,
		&FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6,
		&FWPM_LAYER_ALE_AUTH_LISTEN_V4,
		&FWPM_LAYER_ALE_AUTH_LISTEN_V6,
		&FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V4,
		&FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V6,
		&FWPM_LAYER_OUTBOUND_TRANSPORT_V4,
		&FWPM_LAYER_OUTBOUND_TRANSPORT_V6,
		&FWPM_LAYER_INBOUND_TRANSPORT_V4,
		&FWPM_LAYER_INBOUND_TRANSPORT_V6,
		&FWPM_LAYER_IPFORWARD_V4,
		&FWPM_LAYER_IPFORWARD_V6,
		&FWPM_LAYER_INBOUND_ICMP_ERROR_V4,
		&FWPM_LAYER_INBOUND_ICMP_ERROR_V6,
		&FWPM_LAYER_OUTBOUND_ICMP_ERROR_V4,
		&FWPM_LAYER_OUTBOUND_ICMP_ERROR_V6,
	};

	static R_STRINGREF layer_names[] = {
		PR_STRINGREF_INIT (L"FWPM_LAYER_ALE_AUTH_CONNECT_V4"),
		PR_STRINGREF_INIT (L"FWPM_LAYER_ALE_AUTH_CONNECT_V6"),
		PR_STRINGREF_INIT (L"FWPM_LAYER_ALE_CONNECT_REDIRECT_V4"),
		PR_STRINGREF_INIT (L"FWPM_LAYER_ALE_CONNECT_REDIRECT_V6"),
		PR_STRINGREF_INIT (L"FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4"),
		PR_STRINGREF_INIT (L"FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6"),
		PR_STRINGREF_INIT (L"FWPM_LAYER_ALE_AUTH_LISTEN_V4"),
		PR_STRINGREF_INIT (L"FWPM_LAYER_ALE_AUTH_LISTEN_V6"),
		PR_STRINGREF_INIT (L"FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V4"),
		PR_STRINGREF_INIT (L"FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V6"),
		PR_STRINGREF_INIT (L"FWPM_LAYER_OUTBOUND_TRANSPORT_V4"),
		PR_STRINGREF_INIT (L"FWPM_LAYER_OUTBOUND_TRANSPORT_V6"),
		PR_STRINGREF_INIT (L"FWPM_LAYER_INBOUND_TRANSPORT_V4"),
		PR_STRINGREF_INIT (L"FWPM_LAYER_INBOUND_TRANSPORT_V6"),
		PR_STRINGREF_INIT (L"FWPM_LAYER_IPFORWARD_V4"),
		PR_STRINGREF_INIT (L"FWPM_LAYER_IPFORWARD_V6"),
		PR_STRINGREF_INIT (L"FWPM_LAYER_INBOUND_ICMP_ERROR_V4"),
		PR_STRINGREF_INIT (L"FWPM_LAYER_INBOUND_ICMP_ERROR_V6"),
		PR_STRINGREF_INIT (L"FWPM_LAYER_OUTBOUND_ICMP_ERROR_V4"),
		PR_STRINGREF_INIT (L"FWPM_LAYER_OUTBOUND_ICMP_ERROR_V6"),
	};

	C_ASSERT (RTL_NUMBER_OF (layer_guids) == RTL_NUMBER_OF (layer_names));

	PR_STRING string;

	for (ULONG_PTR i = 0; i < RTL_NUMBER_OF (layer_guids); i++)
	{
		if (IsEqualGUID (layer_guid, layer_guids[i]))
			return _r_obj_createstring3 (&layer_names[i]);
	}

	_r_str_fromguid (layer_guid, TRUE, &string);

	return string;
}

BOOLEAN _wfp_initialize (
	_In_opt_ HWND hwnd,
	_In_ HANDLE engine_handle
)
{
	FWPM_PROVIDER provider;
	FWPM_SUBLAYER sublayer;
	FWP_VALUE val;
	FWP_VALUE0* fwp_query = NULL;
	ULONG status;
	BOOLEAN is_providerexist;
	BOOLEAN is_sublayerexist;
	BOOLEAN is_intransact;
	BOOLEAN is_success = TRUE; // already initialized

	_r_queuedlock_acquireshared (&lock_transaction);

	_app_setenginesecurity (engine_handle);

	// install engine provider and it's sublayer
	is_providerexist = (_wfp_isproviderinstalled (engine_handle) != INSTALL_DISABLED);
	is_sublayerexist = (_wfp_issublayerinstalled (engine_handle) != INSTALL_DISABLED);

	if (!is_providerexist || !is_sublayerexist)
	{
		is_intransact = _wfp_transact_start (engine_handle, DBG_ARG);

		if (!is_providerexist)
		{
			// create provider
			RtlZeroMemory (&provider, sizeof (provider));

			provider.displayData.name = _r_app_getname ();
			provider.displayData.description = _r_app_getname ();

			provider.providerKey = GUID_WfpProvider;

			if (!config.is_filterstemporary)
				provider.flags = FWPM_PROVIDER_FLAG_PERSISTENT;

			status = FwpmProviderAdd (engine_handle, &provider, NULL);

			if (status != ERROR_SUCCESS && status != FWP_E_ALREADY_EXISTS)
			{
				if (is_intransact)
				{
					FwpmTransactionAbort (engine_handle);

					is_intransact = FALSE;
				}

				_r_log (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"FwpmProviderAdd", status, NULL);

				is_success = FALSE;

				goto CleanupExit;
			}
			else
			{
				is_providerexist = TRUE;
			}
		}

		if (!is_sublayerexist)
		{
			// create sublayer
			RtlZeroMemory (&sublayer, sizeof (sublayer));

			sublayer.displayData.name = _r_app_getname ();
			sublayer.displayData.description = _r_app_getname ();

			sublayer.providerKey = (LPGUID)&GUID_WfpProvider;
			sublayer.subLayerKey = GUID_WfpSublayer;

			// highest weight for UINT16
			sublayer.weight = (UINT16)_r_config_getlong (L"SublayerWeight", FW_SUBLAYER_WEIGHT);

			if (!config.is_filterstemporary)
				sublayer.flags = FWPM_SUBLAYER_FLAG_PERSISTENT;

			status = FwpmSubLayerAdd (engine_handle, &sublayer, NULL);

			if (status != ERROR_SUCCESS && status != FWP_E_ALREADY_EXISTS)
			{
				if (is_intransact)
				{
					FwpmTransactionAbort (engine_handle);

					is_intransact = FALSE;
				}

				_r_log (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"FwpmSubLayerAdd", status, NULL);

				is_success = FALSE;

				goto CleanupExit;
			}
			else
			{
				is_sublayerexist = TRUE;
			}
		}

		if (is_intransact)
		{
			if (!_wfp_transact_commit (engine_handle, DBG_ARG))
				is_success = FALSE;
		}
	}

	// set provider security information
	_app_setprovidersecurity (engine_handle, &GUID_WfpProvider, TRUE);

	// set sublayer security information
	_app_setsublayersecurity (engine_handle, &GUID_WfpSublayer, TRUE);

	// set engine options
	RtlZeroMemory (&val, sizeof (val));

	// dropped packets logging (win7+)
	if (!config.is_neteventset)
	{
		// query net events state
		config.is_neteventenabled = FALSE;

		status = FwpmEngineGetOption (engine_handle, FWPM_ENGINE_COLLECT_NET_EVENTS, &fwp_query);

		if (status == ERROR_SUCCESS && fwp_query)
		{
			config.is_neteventenabled = (fwp_query->type == FWP_UINT32) && (!!fwp_query->uint32);

			FwpmFreeMemory ((PVOID_PTR)&fwp_query);
		}

		// enable net events (if it is disabled)
		if (config.is_neteventenabled)
		{
			config.is_neteventset = TRUE;
		}
		else
		{
			val.type = FWP_UINT32;
			val.uint32 = TRUE;

			status = FwpmEngineSetOption (engine_handle, FWPM_ENGINE_COLLECT_NET_EVENTS, &val);

			if (status != ERROR_SUCCESS)
			{
				_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmEngineSetOption", status, L"FWPM_ENGINE_COLLECT_NET_EVENTS");
			}
			else
			{
				config.is_neteventset = TRUE;
			}
		}

		if (config.is_neteventset)
		{
			_wfp_logsetoption (engine_handle);

			_wfp_logsubscribe (hwnd, engine_handle);
		}
	}

	// packet queuing (win8+)
	if (_r_config_getboolean (L"IsPacketQueuingEnabled", TRUE))
	{
		// Enables inbound or forward packet queuing independently.
		// when enabled, the system is able to evenly distribute cpu load
		// to multiple cpus for site-to-site ipsec tunnel scenarios.

		val.type = FWP_UINT32;
		val.uint32 = FWPM_ENGINE_OPTION_PACKET_QUEUE_INBOUND | FWPM_ENGINE_OPTION_PACKET_QUEUE_FORWARD;

		status = FwpmEngineSetOption (engine_handle, FWPM_ENGINE_PACKET_QUEUING, &val);

		if (status != ERROR_SUCCESS)
			_r_log (LOG_LEVEL_WARNING, NULL, L"FwpmEngineSetOption", status, L"FWPM_ENGINE_PACKET_QUEUING");
	}

CleanupExit:

	_r_queuedlock_releaseshared (&lock_transaction);

	return is_success;
}

VOID _wfp_uninitialize (
	_In_ HANDLE engine_handle,
	_In_ BOOLEAN is_full
)
{
	PR_ARRAY callouts;
	PR_STRING string;
	LPGUID guid;
	FWP_VALUE val = {0};
	BOOLEAN is_intransact;
	ULONG status;

	_r_queuedlock_acquireshared (&lock_transaction);

	// dropped packets logging (win7+)
	if (config.is_neteventset)
		_wfp_logunsubscribe (engine_handle);

	if (!config.is_neteventenabled && config.is_neteventset)
	{
		val.type = FWP_UINT32;
		val.uint32 = FALSE;

		status = FwpmEngineSetOption (engine_handle, FWPM_ENGINE_COLLECT_NET_EVENTS, &val);

		if (status == ERROR_SUCCESS)
			config.is_neteventset = FALSE;
	}

	if (is_full)
	{
		_app_setprovidersecurity (engine_handle, &GUID_WfpProvider, FALSE);
		_app_setsublayersecurity (engine_handle, &GUID_WfpSublayer, FALSE);

		status = _wfp_dumpcallouts (engine_handle, &GUID_WfpProvider, &callouts);

		if (callouts)
		{
			for (ULONG_PTR i = 0; i < _r_obj_getarraysize (callouts); i++)
			{
				guid = _r_obj_getarrayitem (callouts, i);

				_app_setcalloutsecurity (engine_handle, guid, FALSE);
			}
		}

		is_intransact = _wfp_transact_start (engine_handle, DBG_ARG);

		// destroy callouts (deprecated)
		if (callouts)
		{
			for (ULONG_PTR i = 0; i < _r_obj_getarraysize (callouts); i++)
			{
				guid = _r_obj_getarrayitem (callouts, i);

				status = FwpmCalloutDeleteByKey (engine_handle, guid);

				if (status != ERROR_SUCCESS)
				{
					_r_str_fromguid (guid, TRUE, &string);

					_r_log (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"FwpmCalloutDeleteByKey", status, _r_obj_getstring (string));

					if (string)
						_r_obj_dereference (string);
				}
			}
		}

		// destroy sublayer
		status = FwpmSubLayerDeleteByKey (engine_handle, &GUID_WfpSublayer);

		if (status != ERROR_SUCCESS && status != FWP_E_SUBLAYER_NOT_FOUND)
			_r_log (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"FwpmSubLayerDeleteByKey", status, NULL);

		// destroy provider
		status = FwpmProviderDeleteByKey (engine_handle, &GUID_WfpProvider);

		if (status != ERROR_SUCCESS && status != FWP_E_PROVIDER_NOT_FOUND)
			_r_log (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"FwpmProviderDeleteByKey", status, NULL);

		if (is_intransact)
			_wfp_transact_commit (engine_handle, DBG_ARG);

		if (callouts)
			_r_obj_dereference (callouts);
	}

	_r_queuedlock_releaseshared (&lock_transaction);
}

VOID _wfp_installfilters (
	_In_ HANDLE engine_handle
)
{
	// dump all filters into array
	PR_ARRAY guids;
	PR_LIST rules;
	LPGUID guid;
	PITEM_APP ptr_app = NULL;
	PITEM_RULE ptr_rule;
	ULONG_PTR enum_key;
	BOOLEAN is_intransact;
	ULONG status;

	// set security information
	_app_setprovidersecurity (engine_handle, &GUID_WfpProvider, FALSE);
	_app_setsublayersecurity (engine_handle, &GUID_WfpSublayer, FALSE);

	_wfp_clearfilter_ids ();

	_r_queuedlock_acquireshared (&lock_transaction);

	status = _wfp_dumpfilters (engine_handle, &GUID_WfpProvider, &guids);

	// restore filters security
	if (status == ERROR_SUCCESS)
	{
		for (ULONG_PTR i = 0; i < _r_obj_getarraysize (guids); i++)
		{
			guid = _r_obj_getarrayitem (guids, i);

			_app_setfiltersecurity (engine_handle, guid, FALSE, DBG_ARG);
		}
	}

	is_intransact = _wfp_transact_start (engine_handle, DBG_ARG);

	// destroy all filters
	if (status == ERROR_SUCCESS)
	{
		for (ULONG_PTR i = 0; i < _r_obj_getarraysize (guids); i++)
		{
			guid = _r_obj_getarrayitem (guids, i);

			_wfp_deletefilter (engine_handle, guid);
		}

		_r_obj_dereference (guids);
	}

	rules = _r_obj_createlist (&_r_obj_dereference);

	// apply apps rules
	enum_key = 0;

	_r_queuedlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtablepointer (apps_table, &ptr_app, NULL, &enum_key))
	{
		if (ptr_app->is_enabled)
			_r_obj_addlistitem (rules, _r_obj_reference (ptr_app));
	}

	_r_queuedlock_releaseshared (&lock_apps);

	if (!_r_obj_isempty2 (rules))
	{
		_wfp_create3filters (engine_handle, rules, DBG_ARG, is_intransact);

		_r_obj_clearlist (rules);
	}

	// apply blocklist/system/user rules
	_r_queuedlock_acquireshared (&lock_rules);

	for (ULONG_PTR i = 0; i < _r_obj_getlistsize (rules_list); i++)
	{
		ptr_rule = _r_obj_getlistitem (rules_list, i);

		if (ptr_rule)
		{
			if (ptr_rule->is_enabled)
				_r_obj_addlistitem (rules, _r_obj_reference (ptr_rule));
		}
	}

	_r_queuedlock_releaseshared (&lock_rules);

	if (!_r_obj_isempty2 (rules))
	{
		_wfp_create4filters (engine_handle, rules, DBG_ARG, is_intransact);

		_r_obj_clearlist (rules);
	}

	// apply internal rules
	_wfp_create2filters (engine_handle, DBG_ARG, is_intransact);

	if (is_intransact)
		_wfp_transact_commit (engine_handle, DBG_ARG);

	// secure filters
	status = _wfp_dumpfilters (engine_handle, &GUID_WfpProvider, &guids);

	if (status == ERROR_SUCCESS)
	{
		for (ULONG_PTR i = 0; i < _r_obj_getarraysize (guids); i++)
		{
			guid = _r_obj_getarrayitem (guids, i);

			_app_setfiltersecurity (engine_handle, guid, TRUE, DBG_ARG);
		}

		_r_obj_dereference (guids);
	}

	_app_setprovidersecurity (engine_handle, &GUID_WfpProvider, TRUE);
	_app_setsublayersecurity (engine_handle, &GUID_WfpSublayer, TRUE);

	_r_queuedlock_releaseshared (&lock_transaction);

	_r_obj_dereference (rules);
}

BOOLEAN _wfp_transact_start (
	_In_ HANDLE engine_handle,
	_In_ LPCWSTR file_name,
	_In_ UINT line
)
{
	ULONG status;

	status = FwpmTransactionBegin (engine_handle, 0);

	if (status == FWP_E_TXN_IN_PROGRESS)
		return FALSE;

	if (status != ERROR_SUCCESS)
	{
		_r_log_v (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"FwpmTransactionBegin", status, L"%s:%" TEXT (PRIu32), DBG_ARG_VAR);

		return FALSE;
	}

	return TRUE;
}

BOOLEAN _wfp_transact_commit (
	_In_ HANDLE engine_handle,
	_In_ LPCWSTR file_name,
	_In_ UINT line
)
{
	ULONG status;

	status = FwpmTransactionCommit (engine_handle);

	if (status != ERROR_SUCCESS)
	{
		FwpmTransactionAbort (engine_handle);

		_r_log_v (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"FwpmTransactionCommit", status, L"%s:%" TEXT (PRIu32), DBG_ARG_VAR);

		return FALSE;
	}

	return TRUE;
}

BOOLEAN _wfp_deletefilter (
	_In_ HANDLE engine_handle,
	_In_ LPGUID filter_id
)
{
	PR_STRING string;
	ULONG status;

	status = FwpmFilterDeleteByKey (engine_handle, filter_id);

	if (status != ERROR_SUCCESS && status != FWP_E_FILTER_NOT_FOUND)
	{
		_r_str_fromguid (filter_id, TRUE, &string);

		_r_log (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"FwpmFilterDeleteByKey", status, _r_obj_getstringordefault (string, SZ_EMPTY));

		if (string)
			_r_obj_dereference (string);

		return FALSE;
	}

	return TRUE;
}

FORCEINLINE LPCWSTR _wfp_filtertypetostring (
	_In_ ENUM_TYPE_DATA filter_type
)
{
	switch (filter_type)
	{
		case DATA_APP_REGULAR:
		case DATA_APP_DEVICE:
		case DATA_APP_NETWORK:
		case DATA_APP_PICO:
		case DATA_APP_SERVICE:
		case DATA_APP_UWP:
		{
			return L"App";
		}

		case DATA_RULE_BLOCKLIST:
		{
			return L"Blocklist";
		}

		case DATA_RULE_SYSTEM:
		{
			return L"System rule";
		}

		case DATA_RULE_SYSTEM_USER:
		case DATA_RULE_USER:
		{
			return L"User rule";
		}

		case DATA_FILTER_GENERAL:
		{
			return L"Internal";
		}
	}

	return NULL;
}

ULONG _wfp_createcallout (
	_In_ HANDLE engine_handle,
	_In_ LPCGUID layer_key,
	_In_ LPCGUID callout_key
)
{
	FWPM_CALLOUT0 callout = {0};
	ULONG status;

	callout.displayData.name = _r_app_getname ();
	callout.displayData.description = _r_app_getname ();

	if (!config.is_filterstemporary)
		callout.flags = FWPM_CALLOUT_FLAG_PERSISTENT;

	callout.providerKey = (LPGUID)&GUID_WfpProvider;

	RtlCopyMemory (&callout.calloutKey, callout_key, sizeof (GUID));
	RtlCopyMemory (&callout.applicableLayer, layer_key, sizeof (GUID));

	status = FwpmCalloutAdd (engine_handle, &callout, NULL, NULL);

	if (status != ERROR_SUCCESS && status != FWP_E_ALREADY_EXISTS)
		_r_log (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"FwpmCalloutAdd", status, NULL);

	return status;
}

ULONG _wfp_createfilter (
	_In_ HANDLE engine_handle,
	_In_ ENUM_TYPE_DATA type,
	_In_opt_ LPCWSTR filter_name,
	_In_reads_ (count) FWPM_FILTER_CONDITION *lpcond,
	_In_ UINT32 count,
	_In_ LPCGUID layer_id,
	_In_opt_ LPCGUID callout_id,
	_In_ UINT8 weight,
	_In_ FWP_ACTION_TYPE action,
	_In_ UINT32 flags,
	_Inout_opt_ PR_ARRAY guids
)
{
	FWPM_FILTER filter = {0};
	WCHAR filter_description[128];
	PR_STRING layer_name;
	LPCWSTR filter_type;
	UINT64 filter_id;
	ULONG status;

	// create filter guid
	status = _r_math_createguid (&filter.filterKey);

	if (status != ERROR_SUCCESS)
		return status;

	filter_type = _wfp_filtertypetostring (type);

	if (filter_type && filter_name)
	{
		_r_str_printf (filter_description, RTL_NUMBER_OF (filter_description), L"%s\\%s", filter_type, filter_name);
	}
	else if (filter_name)
	{
		_r_str_copy (filter_description, RTL_NUMBER_OF (filter_description), filter_name);
	}
	else
	{
		_r_str_copy (filter_description, RTL_NUMBER_OF (filter_description), SZ_EMPTY);
	}

	// reset action rights
	if (weight == FW_WEIGHT_HIGHEST_IMPORTANT || action == FWP_ACTION_BLOCK || action == FWP_ACTION_CALLOUT_TERMINATING)
		filter.flags = FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT;

	// set filter flags
	if ((flags & FWPM_FILTER_FLAG_BOOTTIME) == 0)
	{
		if (!config.is_filterstemporary)
			filter.flags |= FWPM_FILTER_FLAG_PERSISTENT;

		// filter is indexed to help enable faster lookup during classification (win8+)
		filter.flags |= FWPM_FILTER_FLAG_INDEXED;
	}

	if (flags)
		filter.flags |= flags;

	filter.displayData.name = _r_app_getname ();
	filter.displayData.description = filter_description;

	RtlCopyMemory (&filter.subLayerKey, &GUID_WfpSublayer, sizeof (GUID));
	RtlCopyMemory (&filter.layerKey, layer_id, sizeof (GUID));

	if (callout_id)
		RtlCopyMemory (&filter.action.calloutKey, callout_id, sizeof (GUID));

	filter.providerKey = (LPGUID)&GUID_WfpProvider;
	filter.weight.type = FWP_UINT8;
	filter.weight.uint8 = weight;
	filter.action.type = action;

	if (count)
	{
		filter.numFilterConditions = count;
		filter.filterCondition = lpcond;
	}

	status = FwpmFilterAdd (engine_handle, &filter, NULL, &filter_id);

	if (status == ERROR_SUCCESS)
	{
		if (guids)
			_r_obj_addarrayitem (guids, &filter.filterKey);
	}
	else
	{
		layer_name = _wfp_getlayername ((LPGUID)layer_id);

		_r_log_v (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"FwpmFilterAdd", status, L"%s\\%s", _r_obj_getstring (layer_name), filter.displayData.description);

		if (layer_name)
			_r_obj_dereference (layer_name);
	}

	return status;
}

VOID _wfp_clearfilter_ids ()
{
	PITEM_APP ptr_app = NULL;
	PITEM_RULE ptr_rule;
	ULONG_PTR enum_key;

	// clear common filters
	_r_obj_cleararray (filter_ids);

	// clear apps filters
	_r_queuedlock_acquireshared (&lock_apps);

	enum_key = 0;

	while (_r_obj_enumhashtablepointer (apps_table, &ptr_app, NULL, &enum_key))
	{
		ptr_app->is_haveerrors = FALSE;

		_r_obj_cleararray (ptr_app->guids);
	}

	_r_queuedlock_releaseshared (&lock_apps);

	// clear rules filters
	_r_queuedlock_acquireshared (&lock_rules);

	for (ULONG_PTR i = 0; i < _r_obj_getlistsize (rules_list); i++)
	{
		ptr_rule = _r_obj_getlistitem (rules_list, i);

		if (!ptr_rule)
			continue;

		ptr_rule->is_haveerrors = FALSE;

		_r_obj_cleararray (ptr_rule->guids);
	}

	_r_queuedlock_releaseshared (&lock_rules);
}

VOID _wfp_destroyfilters (
	_In_ HANDLE engine_handle
)
{
	PR_ARRAY guids;
	ULONG status;

	_wfp_clearfilter_ids ();

	// destroy all filters
	_r_queuedlock_acquireshared (&lock_transaction);

	status = _wfp_dumpfilters (engine_handle, &GUID_WfpProvider, &guids);

	_r_queuedlock_releaseshared (&lock_transaction);

	if (status != ERROR_SUCCESS)
		return;

	_wfp_destroyfilters_array (engine_handle, guids, DBG_ARG);

	_r_obj_dereference (guids);
}

VOID _wfp_destroyfilters_array (
	_In_ HANDLE engine_handle,
	_In_ PR_ARRAY guids,
	_In_ LPCWSTR file_name,
	_In_ UINT line
)
{
	LPGUID guid;
	BOOLEAN is_enabled;
	BOOLEAN is_intransact;

	is_enabled = _app_initinterfacestate (_r_app_gethwnd (), FALSE);

	_r_queuedlock_acquireshared (&lock_transaction);

	for (ULONG_PTR i = 0; i < _r_obj_getarraysize (guids); i++)
	{
		guid = _r_obj_getarrayitem (guids, i);

		_app_setfiltersecurity (engine_handle, guid, FALSE, DBG_ARG_VAR);
	}

	is_intransact = _wfp_transact_start (engine_handle, DBG_ARG_VAR);

	for (ULONG_PTR i = 0; i < _r_obj_getarraysize (guids); i++)
	{
		guid = _r_obj_getarrayitem (guids, i);

		_wfp_deletefilter (engine_handle, guid);
	}

	if (is_intransact)
		_wfp_transact_commit (engine_handle, DBG_ARG_VAR);

	_r_queuedlock_releaseshared (&lock_transaction);

	_app_restoreinterfacestate (_r_app_gethwnd (), is_enabled);
}

BOOLEAN _wfp_createrulefilter (
	_In_ HANDLE engine_handle,
	_In_ ENUM_TYPE_DATA filter_type,
	_In_opt_ LPCWSTR filter_name,
	_In_opt_ ULONG_PTR app_hash,
	_In_opt_ PITEM_FILTER_CONFIG filter_config,
	_In_opt_ PR_STRINGREF rule_remote,
	_In_opt_ PR_STRINGREF rule_local,
	_In_ UINT8 weight,
	_In_ FWP_ACTION_TYPE action,
	_In_ UINT32 flags,
	_Inout_opt_ PR_ARRAY guids
)
{
	FWPM_FILTER_CONDITION fwfc[8] = {0};
	UINT32 count = 0;
	ITEM_ADDRESS address;
	FWP_V4_ADDR_AND_MASK fwp_addr4_and_mask1;
	FWP_V4_ADDR_AND_MASK fwp_addr4_and_mask2;
	FWP_V6_ADDR_AND_MASK fwp_addr6_and_mask1;
	FWP_V6_ADDR_AND_MASK fwp_addr6_and_mask2;
	FWP_RANGE fwp_range1;
	FWP_RANGE fwp_range2;
	FWP_BYTE_BLOB *byte_blob = NULL;
	PITEM_APP ptr_app = NULL;
	BOOLEAN is_remoteaddr_set = FALSE;
	BOOLEAN is_remoteport_set = FALSE;
	BOOLEAN is_success = FALSE;
	FWP_DIRECTION direction;
	UINT8 protocol;
	ADDRESS_FAMILY af;
	PR_STRINGREF rules[] = {
		rule_remote,
		rule_local
	};
	NTSTATUS status;

	if (filter_config)
	{
		direction = filter_config->direction;
		protocol = filter_config->protocol;
		af = filter_config->af;
	}
	else
	{
		direction = FWP_DIRECTION_MAX;
		protocol = 0;
		af = AF_UNSPEC;
	}

	// set path condition
	if (app_hash)
	{
		ptr_app = _app_getappitem (app_hash);

		if (!ptr_app)
		{
			_r_log_v (LOG_LEVEL_WARNING, NULL, TEXT (__FUNCTION__), 0, L"App \"%" TEXT (PR_ULONG_PTR) L"\" not found!", app_hash);

			goto CleanupExit;
		}

		if (ptr_app->type == DATA_APP_SERVICE) // windows service
		{
			if (ptr_app->bytes)
			{
				ByteBlobAlloc (ptr_app->bytes->buffer, RtlLengthSecurityDescriptor (ptr_app->bytes->buffer), &byte_blob);

				fwfc[count].fieldKey = FWPM_CONDITION_ALE_USER_ID;
				fwfc[count].matchType = FWP_MATCH_EQUAL;
				fwfc[count].conditionValue.type = FWP_SECURITY_DESCRIPTOR_TYPE;
				fwfc[count].conditionValue.sd = byte_blob;

				count += 1;
			}
			else
			{
				_r_log (LOG_LEVEL_ERROR, NULL, TEXT (__FUNCTION__), 0, _r_obj_getstring (ptr_app->original_path));

				goto CleanupExit;
			}
		}
		else if (ptr_app->type == DATA_APP_UWP) // uwp app (win8+)
		{
			if (ptr_app->bytes)
			{
				fwfc[count].fieldKey = FWPM_CONDITION_ALE_PACKAGE_ID;
				fwfc[count].matchType = FWP_MATCH_EQUAL;
				fwfc[count].conditionValue.type = FWP_SID;
				fwfc[count].conditionValue.sid = (PSID)ptr_app->bytes->buffer;

				count += 1;
			}
			else
			{
				_r_log (LOG_LEVEL_ERROR, NULL, TEXT (__FUNCTION__), 0, _r_obj_getstring (ptr_app->original_path));

				goto CleanupExit;
			}
		}
		else
		{
			status = _FwpmGetAppIdFromFileName1 (ptr_app->original_path, ptr_app->type, &byte_blob);

			if (NT_SUCCESS (status))
			{
				fwfc[count].fieldKey = FWPM_CONDITION_ALE_APP_ID;
				fwfc[count].matchType = FWP_MATCH_EQUAL;
				fwfc[count].conditionValue.type = FWP_BYTE_BLOB_TYPE;
				fwfc[count].conditionValue.byteBlob = byte_blob;

				count += 1;
			}
			else
			{
				// do not log file not found to error log
				if (status != STATUS_OBJECT_NAME_NOT_FOUND && status != STATUS_OBJECT_PATH_NOT_FOUND)
					_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmGetAppIdFromFileName", status, _r_obj_getstring (ptr_app->original_path));

				goto CleanupExit;
			}
		}
	}

	// set ip/port condition
	for (ULONG_PTR i = 0; i < RTL_NUMBER_OF (rules); i++)
	{
		if (_r_obj_isstringempty (rules[i]))
			continue;

		if (!_app_parserulestring (rules[i], &address))
			goto CleanupExit;

		if (i == 0)
		{
			if (address.type == DATA_TYPE_PORT)
			{
				is_remoteport_set = TRUE;
			}
			else if (address.type == DATA_TYPE_IP)
			{
				is_remoteaddr_set = TRUE;
			}
		}

		if (address.is_range)
		{
			if (address.type == DATA_TYPE_IP)
			{
				if (address.format == NET_ADDRESS_IPV4)
				{
					af = AF_INET;
				}
				else if (address.format == NET_ADDRESS_IPV6)
				{
					af = AF_INET6;
				}
				else
				{
					goto CleanupExit;
				}
			}

			if (address.type == DATA_TYPE_PORT)
			{
				if (i == 0)
				{
					fwfc[count].fieldKey = FWPM_CONDITION_IP_REMOTE_PORT;
				}
				else
				{
					fwfc[count].fieldKey = FWPM_CONDITION_IP_LOCAL_PORT;
				}
			}
			else
			{
				if (i == 0)
				{
					fwfc[count].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
				}
				else
				{
					fwfc[count].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;
				}
			}

			fwfc[count].matchType = FWP_MATCH_RANGE;
			fwfc[count].conditionValue.type = FWP_RANGE_TYPE;

			if (i == 0)
			{
				fwp_range1 = address.range;
				fwfc[count].conditionValue.rangeValue = &fwp_range1;
			}
			else
			{
				fwp_range2 = address.range;
				fwfc[count].conditionValue.rangeValue = &fwp_range2;
			}

			count += 1;
		}
		else
		{
			if (address.type == DATA_TYPE_PORT)
			{
				if (i == 0)
				{
					fwfc[count].fieldKey = FWPM_CONDITION_IP_REMOTE_PORT;
				}
				else
				{
					fwfc[count].fieldKey = FWPM_CONDITION_IP_LOCAL_PORT;
				}

				fwfc[count].matchType = FWP_MATCH_EQUAL;
				fwfc[count].conditionValue.type = FWP_UINT16;
				fwfc[count].conditionValue.uint16 = address.port;

				count += 1;
			}
			else if (address.type == DATA_TYPE_IP)
			{
				if (i == 0)
				{
					fwfc[count].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
				}
				else
				{
					fwfc[count].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;
				}

				fwfc[count].matchType = FWP_MATCH_EQUAL;

				if (address.format == NET_ADDRESS_IPV4)
				{
					af = AF_INET;

					fwfc[count].conditionValue.type = FWP_V4_ADDR_MASK;
					//fwfc[count].conditionValue.v4AddrMask = &address.addr4;

					if (i == 0)
					{
						fwp_addr4_and_mask1 = address.addr4;
						fwfc[count].conditionValue.v4AddrMask = &fwp_addr4_and_mask1;
					}
					else
					{
						fwp_addr4_and_mask2 = address.addr4;
						fwfc[count].conditionValue.v4AddrMask = &fwp_addr4_and_mask2;
					}

					count += 1;
				}
				else if (address.format == NET_ADDRESS_IPV6)
				{
					af = AF_INET6;

					fwfc[count].conditionValue.type = FWP_V6_ADDR_MASK;
					//fwfc[count].conditionValue.v6AddrMask = &address.addr6;

					if (i == 0)
					{
						fwp_addr6_and_mask1 = address.addr6;
						fwfc[count].conditionValue.v6AddrMask = &fwp_addr6_and_mask1;
					}
					else
					{
						fwp_addr6_and_mask2 = address.addr6;
						fwfc[count].conditionValue.v6AddrMask = &fwp_addr6_and_mask2;
					}

					count += 1;
				}
				else
				{
					goto CleanupExit;
				}
			}
			else
			{
				goto CleanupExit;
			}
		}

		// set port if available
		if (address.type == DATA_TYPE_IP)
		{
			if (address.port)
			{
				if (i == 0)
				{
					fwfc[count].fieldKey = FWPM_CONDITION_IP_REMOTE_PORT;
				}
				else
				{
					fwfc[count].fieldKey = FWPM_CONDITION_IP_LOCAL_PORT;
				}

				fwfc[count].matchType = FWP_MATCH_EQUAL;
				fwfc[count].conditionValue.type = FWP_UINT16;
				fwfc[count].conditionValue.uint16 = address.port;

				count += 1;
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
	if (direction == FWP_DIRECTION_OUTBOUND || direction == FWP_DIRECTION_MAX)
	{
		if (af == AF_INET || af == AF_UNSPEC)
		{
			_wfp_createfilter (
				engine_handle,
				filter_type,
				filter_name,
				fwfc,
				count,
				&FWPM_LAYER_ALE_AUTH_CONNECT_V4,
				NULL,
				weight,
				action,
				flags,
				guids
			);
		}

		if (af == AF_INET6 || af == AF_UNSPEC)
		{
			_wfp_createfilter (
				engine_handle,
				filter_type,
				filter_name,
				fwfc,
				count,
				&FWPM_LAYER_ALE_AUTH_CONNECT_V6,
				NULL,
				weight,
				action,
				flags,
				guids
			);
		}
	}

	// create inbound layer filter
	if (direction == FWP_DIRECTION_INBOUND || direction == FWP_DIRECTION_MAX)
	{
		if (af == AF_INET || af == AF_UNSPEC)
		{
			_wfp_createfilter (
				engine_handle,
				filter_type,
				filter_name,
				fwfc,
				count,
				&FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4,
				NULL,
				weight,
				action,
				flags,
				guids
			);
		}

		if (af == AF_INET6 || af == AF_UNSPEC)
		{
			_wfp_createfilter (
				engine_handle,
				filter_type,
				filter_name,
				fwfc,
				count,
				&FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6,
				NULL,
				weight,
				action,
				flags,
				guids
			);
		}
	}

	is_success = TRUE;

CleanupExit:

	if (ptr_app)
		_r_obj_dereference (ptr_app);

	if (byte_blob)
		ByteBlobFree (&byte_blob);

	return is_success;
}

BOOLEAN _wfp_create4filters (
	_In_ HANDLE engine_handle,
	_In_  PR_LIST rules,
	_In_ LPCWSTR file_name,
	_In_ UINT line,
	_In_ BOOLEAN is_intransact
)
{
	R_STRINGREF remote_remaining_part;
	R_STRINGREF local_remaining_part;
	R_STRINGREF rule_remote_part;
	R_STRINGREF rule_local_part;
	LPCWSTR rule_name;
	PR_ARRAY guids;
	LPGUID guid;
	PITEM_RULE ptr_rule;
	ULONG_PTR hash_code;
	ULONG_PTR enum_key;
	BOOLEAN is_enabled;

	if (_r_obj_isempty (rules))
		return FALSE;

	is_enabled = _app_initinterfacestate (_r_app_gethwnd (), FALSE);

	if (!is_intransact && _wfp_isfiltersapplying ())
		is_intransact = TRUE;

	guids = _r_obj_createarray (sizeof (GUID), NULL);

	if (!is_intransact)
	{
		for (ULONG_PTR i = 0; i < _r_obj_getlistsize (rules); i++)
		{
			ptr_rule = _r_obj_getlistitem (rules, i);

			if (!ptr_rule)
				continue;

			if (!_r_obj_isempty (ptr_rule->guids))
			{
				_r_obj_addarrayitems (guids, ptr_rule->guids->items, ptr_rule->guids->count);

				_r_obj_cleararray (ptr_rule->guids);
			}
		}

		for (ULONG_PTR i = 0; i < _r_obj_getarraysize (guids); i++)
		{
			guid = _r_obj_getarrayitem (guids, i);

			_app_setfiltersecurity (engine_handle, guid, FALSE, DBG_ARG_VAR);
		}

		_r_queuedlock_acquireshared (&lock_transaction);

		is_intransact = !_wfp_transact_start (engine_handle, DBG_ARG_VAR);
	}

	for (ULONG_PTR i = 0; i < _r_obj_getarraysize (guids); i++)
	{
		guid = _r_obj_getarrayitem (guids, i);

		_wfp_deletefilter (engine_handle, guid);
	}

	for (ULONG_PTR i = 0; i < _r_obj_getlistsize (rules); i++)
	{
		ptr_rule = _r_obj_getlistitem (rules, i);

		if (!ptr_rule)
			continue;

		ptr_rule->is_haveerrors = FALSE;

		_r_obj_cleararray (ptr_rule->guids);

		if (!ptr_rule->is_enabled)
			continue;

		rule_name = _r_obj_getstring (ptr_rule->name);

		if (!_r_obj_isstringempty (ptr_rule->rule_remote))
		{
			_r_str_splitatchar (&ptr_rule->rule_remote->sr, DIVIDER_RULE[0], &rule_remote_part, &remote_remaining_part);
		}
		else
		{
			_r_obj_initializestringrefempty (&rule_remote_part);
			_r_obj_initializestringrefempty (&remote_remaining_part);
		}

		if (!_r_obj_isstringempty (ptr_rule->rule_local))
		{
			_r_str_splitatchar (&ptr_rule->rule_local->sr, DIVIDER_RULE[0], &rule_local_part, &local_remaining_part);
		}
		else
		{
			_r_obj_initializestringrefempty (&rule_local_part);
			_r_obj_initializestringrefempty (&local_remaining_part);
		}

		while (TRUE)
		{
			// apply rules for services hosts
			if (ptr_rule->is_forservices)
			{
				if (!_wfp_createrulefilter (
					engine_handle,
					ptr_rule->type,
					rule_name,
					config.ntoskrnl_hash,
					&ptr_rule->config,
					&rule_remote_part,
					&rule_local_part,
					ptr_rule->weight,
					ptr_rule->action,
					0,
					ptr_rule->guids))
				{
					ptr_rule->is_haveerrors = TRUE;
				}

				if (!_wfp_createrulefilter (
					engine_handle,
					ptr_rule->type,
					rule_name,
					config.svchost_hash,
					&ptr_rule->config,
					&rule_remote_part,
					&rule_local_part,
					ptr_rule->weight,
					ptr_rule->action,
					0,
					ptr_rule->guids))
				{
					ptr_rule->is_haveerrors = TRUE;
				}
			}

			if (!_r_obj_isempty (ptr_rule->apps))
			{
				enum_key = 0;

				while (_r_obj_enumhashtable (ptr_rule->apps, NULL, &hash_code, &enum_key))
				{
					if (ptr_rule->is_forservices && _app_issystemhash (hash_code))
						continue;

					if (!_wfp_createrulefilter (
						engine_handle,
						ptr_rule->type,
						rule_name,
						hash_code,
						&ptr_rule->config,
						&rule_remote_part,
						&rule_local_part,
						ptr_rule->weight,
						ptr_rule->action,
						0,
						ptr_rule->guids))
					{
						ptr_rule->is_haveerrors = TRUE;
					}
				}
			}
			else
			{
				if (!_wfp_createrulefilter (
					engine_handle,
					ptr_rule->type,
					rule_name,
					0,
					&ptr_rule->config,
					&rule_remote_part,
					&rule_local_part,
					ptr_rule->weight,
					ptr_rule->action,
					0,
					ptr_rule->guids))
				{
					ptr_rule->is_haveerrors = TRUE;
				}
			}

			if (remote_remaining_part.length == 0 && local_remaining_part.length == 0)
				break;

			if (remote_remaining_part.length != 0)
				_r_str_splitatchar (&remote_remaining_part, DIVIDER_RULE[0], &rule_remote_part, &remote_remaining_part);

			if (local_remaining_part.length != 0)
				_r_str_splitatchar (&local_remaining_part, DIVIDER_RULE[0], &rule_local_part, &local_remaining_part);
		}
	}

	if (!is_intransact)
	{
		_wfp_transact_commit (engine_handle, DBG_ARG_VAR);

		for (ULONG_PTR i = 0; i < _r_obj_getlistsize (rules); i++)
		{
			ptr_rule = _r_obj_getlistitem (rules, i);

			if (ptr_rule && ptr_rule->is_enabled)
			{
				for (ULONG_PTR j = 0; j < _r_obj_getarraysize (ptr_rule->guids); j++)
				{
					guid = _r_obj_getarrayitem (ptr_rule->guids, j);

					_app_setfiltersecurity (engine_handle, guid, TRUE, DBG_ARG_VAR);
				}
			}
		}

		_r_queuedlock_releaseshared (&lock_transaction);
	}

	_app_restoreinterfacestate (_r_app_gethwnd (), is_enabled);

	_r_obj_dereference (guids);

	return TRUE;
}

BOOLEAN _wfp_create3filters (
	_In_ HANDLE engine_handle,
	_In_ PR_LIST rules,
	_In_ LPCWSTR file_name,
	_In_ UINT line,
	_In_ BOOLEAN is_intransact
)
{

	PR_ARRAY guids;
	LPGUID guid;
	PITEM_APP ptr_app;
	PR_STRING string;
	BOOLEAN is_enabled;

	if (_r_obj_isempty (rules))
		return FALSE;

	if (!is_intransact && _wfp_isfiltersapplying ())
		is_intransact = TRUE;

	guids = _r_obj_createarray (sizeof (GUID), NULL);

	is_enabled = _app_initinterfacestate (_r_app_gethwnd (), FALSE);

	if (!is_intransact)
	{
		for (ULONG_PTR i = 0; i < _r_obj_getlistsize (rules); i++)
		{
			ptr_app = _r_obj_getlistitem (rules, i);

			if (ptr_app && !_r_obj_isempty (ptr_app->guids))
			{
				_r_obj_addarrayitems (guids, ptr_app->guids->items, ptr_app->guids->count);

				_r_obj_cleararray (ptr_app->guids);
			}
		}

		for (ULONG_PTR i = 0; i < _r_obj_getarraysize (guids); i++)
		{
			guid = _r_obj_getarrayitem (guids, i);

			_app_setfiltersecurity (engine_handle, guid, FALSE, DBG_ARG_VAR);
		}

		_r_queuedlock_acquireshared (&lock_transaction);

		is_intransact = !_wfp_transact_start (engine_handle, DBG_ARG_VAR);
	}

	for (ULONG_PTR i = 0; i < _r_obj_getarraysize (guids); i++)
	{
		guid = _r_obj_getarrayitem (guids, i);

		_wfp_deletefilter (engine_handle, guid);
	}

	for (ULONG_PTR i = 0; i < _r_obj_getlistsize (rules); i++)
	{
		ptr_app = _r_obj_getlistitem (rules, i);

		if (ptr_app && ptr_app->is_enabled)
		{
			string = _app_getappdisplayname (ptr_app, TRUE);

			if (!_wfp_createrulefilter (
				engine_handle,
				ptr_app->type,
				_r_obj_getstring (string),
				ptr_app->app_hash,
				NULL,
				NULL,
				NULL,
				FW_WEIGHT_APP,
				FWP_ACTION_PERMIT,
				0,
				ptr_app->guids))
			{
				ptr_app->is_haveerrors = TRUE;
			}

			if (string)
				_r_obj_dereference (string);
		}
	}

	if (!is_intransact)
	{
		_wfp_transact_commit (engine_handle, DBG_ARG_VAR);

		for (ULONG_PTR i = 0; i < _r_obj_getlistsize (rules); i++)
		{
			ptr_app = _r_obj_getlistitem (rules, i);

			if (ptr_app)
			{
				for (ULONG_PTR j = 0; j < _r_obj_getarraysize (ptr_app->guids); j++)
				{
					guid = _r_obj_getarrayitem (ptr_app->guids, j);

					_app_setfiltersecurity (engine_handle, guid, TRUE, DBG_ARG_VAR);
				}
			}
		}

		_r_queuedlock_releaseshared (&lock_transaction);
	}

	_app_restoreinterfacestate (_r_app_gethwnd (), is_enabled);

	_r_obj_dereference (guids);

	return TRUE;
}

BOOLEAN _wfp_create2filters (
	_In_ HANDLE engine_handle,
	_In_ LPCWSTR file_name,
	_In_ UINT line,
	_In_ BOOLEAN is_intransact
)
{
	// ipv4/ipv6 loopback
	static R_STRINGREF loopback_list[] = {
		PR_STRINGREF_INIT (L"0.0.0.0/8"),
		PR_STRINGREF_INIT (L"10.0.0.0/8"),
		PR_STRINGREF_INIT (L"100.64.0.0/10"),
		PR_STRINGREF_INIT (L"127.0.0.0/8"),
		PR_STRINGREF_INIT (L"169.254.0.0/16"),
		PR_STRINGREF_INIT (L"172.16.0.0/12"),
		PR_STRINGREF_INIT (L"192.0.0.0/24"),
		PR_STRINGREF_INIT (L"192.0.2.0/24"),
		PR_STRINGREF_INIT (L"192.88.99.0/24"),
		PR_STRINGREF_INIT (L"192.168.0.0/16"),
		PR_STRINGREF_INIT (L"198.18.0.0/15"),
		PR_STRINGREF_INIT (L"198.51.100.0/24"),
		PR_STRINGREF_INIT (L"203.0.113.0/24"),
		PR_STRINGREF_INIT (L"224.0.0.0/4"),
		PR_STRINGREF_INIT (L"240.0.0.0/4"),
		PR_STRINGREF_INIT (L"255.255.255.255/32"),
		PR_STRINGREF_INIT (L"[::]/0"),
		PR_STRINGREF_INIT (L"[::]/128"),
		PR_STRINGREF_INIT (L"[::1]/128"),
		PR_STRINGREF_INIT (L"[::ffff:0:0]/96"),
		PR_STRINGREF_INIT (L"[::ffff:0:0:0]/96"),
		PR_STRINGREF_INIT (L"[64:ff9b::]/96"),
		PR_STRINGREF_INIT (L"[100::]/64"),
		PR_STRINGREF_INIT (L"[2001::]/32"),
		PR_STRINGREF_INIT (L"[2001:20::]/28"),
		PR_STRINGREF_INIT (L"[2001:db8::]/32"),
		PR_STRINGREF_INIT (L"[2002::]/16"),
		PR_STRINGREF_INIT (L"[fc00::]/7"),
		PR_STRINGREF_INIT (L"[fe80::]/10"),
		PR_STRINGREF_INIT (L"[ff00::]/8")
	};

	FWPM_FILTER_CONDITION fwfc[3] = {0};
	ITEM_ADDRESS address;
	LPGUID guid;
	FWP_ACTION_TYPE action;
	BOOLEAN is_enabled;

	is_enabled = _app_initinterfacestate (_r_app_gethwnd (), FALSE);

	if (!is_intransact && _wfp_isfiltersapplying ())
		is_intransact = TRUE;

	if (!is_intransact)
	{
		if (!_r_obj_isempty (filter_ids))
		{
			for (ULONG_PTR i = 0; i < _r_obj_getarraysize (filter_ids); i++)
			{
				guid = _r_obj_getarrayitem (filter_ids, i);

				_app_setfiltersecurity (engine_handle, guid, FALSE, DBG_ARG_VAR);
			}
		}

		_r_queuedlock_acquireshared (&lock_transaction);

		is_intransact = !_wfp_transact_start (engine_handle, DBG_ARG_VAR);
	}

	if (!_r_obj_isempty (filter_ids))
	{
		for (ULONG_PTR i = 0; i < _r_obj_getarraysize (filter_ids); i++)
		{
			guid = _r_obj_getarrayitem (filter_ids, i);

			_wfp_deletefilter (engine_handle, guid);
		}

		_r_obj_cleararray (filter_ids);
	}

	// add loopback connections permission
	if (_r_config_getboolean (L"AllowLoopbackConnections", TRUE))
	{
		// match all loopback (localhost) data
		// tests if the network traffic is (non-)app container loopback traffic (win8+)

		fwfc[0].fieldKey = FWPM_CONDITION_FLAGS;
		fwfc[0].matchType = FWP_MATCH_FLAGS_ANY_SET;
		fwfc[0].conditionValue.type = FWP_UINT32;
		fwfc[0].conditionValue.uint32 = FWP_CONDITION_FLAG_IS_LOOPBACK | FWP_CONDITION_FLAG_IS_APPCONTAINER_LOOPBACK;

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_LOOPBACK,
			fwfc,
			1,
			&FWPM_LAYER_ALE_AUTH_CONNECT_V4,
			NULL,
			FW_WEIGHT_HIGHEST_IMPORTANT,
			FWP_ACTION_PERMIT,
			0,
			filter_ids
		);

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_LOOPBACK,
			fwfc,
			1,
			&FWPM_LAYER_ALE_AUTH_CONNECT_V6,
			NULL,
			FW_WEIGHT_HIGHEST_IMPORTANT,
			FWP_ACTION_PERMIT,
			0,
			filter_ids
		);

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_LOOPBACK,
			fwfc,
			1,
			&FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4,
			NULL,
			FW_WEIGHT_HIGHEST_IMPORTANT,
			FWP_ACTION_PERMIT,
			0,
			filter_ids
		);

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_LOOPBACK,
			fwfc,
			1,
			&FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6,
			NULL,
			FW_WEIGHT_HIGHEST_IMPORTANT,
			FWP_ACTION_PERMIT,
			0,
			filter_ids
		);

		for (ULONG_PTR i = 0; i < RTL_NUMBER_OF (loopback_list); i++)
		{
			if (!_app_parserulestring (&loopback_list[i], &address))
				continue;

			fwfc[1].matchType = FWP_MATCH_EQUAL;

			if (address.format == NET_ADDRESS_IPV4)
			{
				fwfc[1].conditionValue.type = FWP_V4_ADDR_MASK;
				fwfc[1].conditionValue.v4AddrMask = &address.addr4;

				fwfc[1].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;

				_wfp_createfilter (
					engine_handle,
					DATA_FILTER_GENERAL,
					FW_NAME_LOOPBACK,
					fwfc,
					2,
					&FWPM_LAYER_ALE_AUTH_CONNECT_V4,
					NULL,
					FW_WEIGHT_HIGHEST_IMPORTANT,
					FWP_ACTION_PERMIT,
					0,
					filter_ids
				);

				fwfc[1].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;

				_wfp_createfilter (
					engine_handle,
					DATA_FILTER_GENERAL,
					FW_NAME_LOOPBACK,
					fwfc,
					2,
					&FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4,
					NULL,
					FW_WEIGHT_HIGHEST_IMPORTANT,
					FWP_ACTION_PERMIT,
					0,
					filter_ids
				);
			}
			else if (address.format == NET_ADDRESS_IPV6)
			{
				fwfc[1].conditionValue.type = FWP_V6_ADDR_MASK;
				fwfc[1].conditionValue.v6AddrMask = &address.addr6;

				fwfc[1].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;

				_wfp_createfilter (
					engine_handle,
					DATA_FILTER_GENERAL,
					FW_NAME_LOOPBACK,
					fwfc,
					2,
					&FWPM_LAYER_ALE_AUTH_CONNECT_V6,
					NULL,
					FW_WEIGHT_HIGHEST_IMPORTANT,
					FWP_ACTION_PERMIT,
					0,
					filter_ids
				);

				fwfc[1].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;

				_wfp_createfilter (
					engine_handle,
					DATA_FILTER_GENERAL,
					FW_NAME_LOOPBACK,
					fwfc,
					2,
					&FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6,
					NULL,
					FW_WEIGHT_HIGHEST_IMPORTANT,
					FWP_ACTION_PERMIT,
					0,
					filter_ids
				);
			}
		}
	}

	// firewall service rules
	// https://msdn.microsoft.com/en-us/library/gg462153.aspx
	if (_r_config_getboolean (L"AllowIPv6", TRUE))
	{
		// allows 6to4 tunneling, which enables ipv6 to run over an ipv4 network
		fwfc[0].fieldKey = FWPM_CONDITION_IP_PROTOCOL;
		fwfc[0].matchType = FWP_MATCH_EQUAL;
		fwfc[0].conditionValue.type = FWP_UINT8;
		fwfc[0].conditionValue.uint8 = IPPROTO_IPV6; // ipv6 header

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			L"Allow6to4",
			fwfc,
			1,
			&FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4,
			NULL,
			FW_WEIGHT_HIGHEST_IMPORTANT,
			FWP_ACTION_PERMIT,
			0,
			filter_ids
		);

		// allows icmpv6 router solicitation messages, which are
		// required for the ipv6 stack to work properly
		fwfc[0].fieldKey = FWPM_CONDITION_ICMP_TYPE;
		fwfc[0].matchType = FWP_MATCH_EQUAL;
		fwfc[0].conditionValue.type = FWP_UINT16;

		fwfc[0].conditionValue.uint16 = 0x85;

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			L"AllowIcmpV6Type133",
			fwfc,
			1,
			&FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6,
			NULL,
			FW_WEIGHT_HIGHEST_IMPORTANT,
			FWP_ACTION_PERMIT,
			0,
			filter_ids
		);

		// allows icmpv6 router advertise messages, which are required
		// for the ipv6 stack to work properly
		fwfc[0].conditionValue.uint16 = 0x86;

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			L"AllowIcmpV6Type134",
			fwfc,
			1,
			&FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6,
			NULL,
			FW_WEIGHT_HIGHEST_IMPORTANT,
			FWP_ACTION_PERMIT,
			0,
			filter_ids
		);

		// allows icmpv6 neighbor solicitation messages, which
		// are required for the ipv6 stack to work properly
		fwfc[0].conditionValue.uint16 = 0x87;

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			L"AllowIcmpV6Type135",
			fwfc,
			1,
			&FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6,
			NULL,
			FW_WEIGHT_HIGHEST_IMPORTANT,
			FWP_ACTION_PERMIT,
			0,
			filter_ids
		);

		// allows icmpv6 neighbor advertise messages, which are
		// required for the ipv6 stack to work properly
		fwfc[0].conditionValue.uint16 = 0x88;

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			L"AllowIcmpV6Type136",
			fwfc,
			1,
			&FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6,
			NULL,
			FW_WEIGHT_HIGHEST_IMPORTANT,
			FWP_ACTION_PERMIT,
			0,
			filter_ids
		);
	}

	// prevent port scanning using stealth discards and silent drops
	// https://docs.microsoft.com/ru-ru/windows/desktop/FWP/preventing-port-scanning
	if (_r_config_getboolean (L"UseStealthMode", TRUE))
	{
		// blocks udp port scanners
		// tests if the network traffic is (non-)app container loopback traffic (win8+)
		fwfc[0].fieldKey = FWPM_CONDITION_FLAGS;
		fwfc[0].matchType = FWP_MATCH_FLAGS_NONE_SET;
		fwfc[0].conditionValue.type = FWP_UINT32;
		fwfc[0].conditionValue.uint32 = FWP_CONDITION_FLAG_IS_LOOPBACK | FWP_CONDITION_FLAG_IS_APPCONTAINER_LOOPBACK;

		fwfc[1].fieldKey = FWPM_CONDITION_ICMP_TYPE;
		fwfc[1].matchType = FWP_MATCH_EQUAL;
		fwfc[1].conditionValue.type = FWP_UINT16;
		fwfc[1].conditionValue.uint16 = 0x03; // destination unreachable

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_ICMP_ERROR,
			fwfc,
			2,
			&FWPM_LAYER_OUTBOUND_ICMP_ERROR_V4,
			NULL,
			FW_WEIGHT_HIGHEST,
			FWP_ACTION_BLOCK,
			0,
			filter_ids
		);

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_ICMP_ERROR,
			fwfc,
			2,
			&FWPM_LAYER_OUTBOUND_ICMP_ERROR_V6,
			NULL,
			FW_WEIGHT_HIGHEST,
			FWP_ACTION_BLOCK,
			0,
			filter_ids
		);

		// blocks tcp port scanners (exclude loopback)
		fwfc[0].conditionValue.uint32 |= FWP_CONDITION_FLAG_IS_IPSEC_SECURED;

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_TCP_RST_ONCLOSE,
			fwfc,
			1,
			&FWPM_LAYER_INBOUND_TRANSPORT_V4_DISCARD,
			&FWPM_CALLOUT_WFP_TRANSPORT_LAYER_V4_SILENT_DROP,
			FW_WEIGHT_HIGHEST,
			FWP_ACTION_CALLOUT_TERMINATING,
			0,
			filter_ids
		);

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_TCP_RST_ONCLOSE,
			fwfc,
			1,
			&FWPM_LAYER_INBOUND_TRANSPORT_V6_DISCARD,
			&FWPM_CALLOUT_WFP_TRANSPORT_LAYER_V6_SILENT_DROP,
			FW_WEIGHT_HIGHEST,
			FWP_ACTION_CALLOUT_TERMINATING,
			0,
			filter_ids
		);
	}

	// install boot-time filters (enforced at boot-time,
	// even before "base filtering engine" service starts)
	if (_r_config_getboolean (L"InstallBoottimeFilters", TRUE) && !config.is_filterstemporary)
	{
		fwfc[0].fieldKey = FWPM_CONDITION_FLAGS;
		fwfc[0].matchType = FWP_MATCH_FLAGS_ANY_SET;
		fwfc[0].conditionValue.type = FWP_UINT32;
		fwfc[0].conditionValue.uint32 = FWP_CONDITION_FLAG_IS_LOOPBACK | FWP_CONDITION_FLAG_IS_APPCONTAINER_LOOPBACK;

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_BOOTTIME,
			fwfc,
			1,
			&FWPM_LAYER_IPFORWARD_V4,
			NULL,
			FW_WEIGHT_HIGHEST_IMPORTANT,
			FWP_ACTION_PERMIT,
			FWPM_FILTER_FLAG_BOOTTIME,
			filter_ids
		);

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_BOOTTIME,
			fwfc,
			1,
			&FWPM_LAYER_IPFORWARD_V6,
			NULL,
			FW_WEIGHT_HIGHEST_IMPORTANT,
			FWP_ACTION_PERMIT,
			FWPM_FILTER_FLAG_BOOTTIME,
			filter_ids
		);

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_BOOTTIME,
			fwfc,
			1,
			&FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4,
			NULL,
			FW_WEIGHT_HIGHEST_IMPORTANT,
			FWP_ACTION_PERMIT,
			FWPM_FILTER_FLAG_BOOTTIME,
			filter_ids
		);

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_BOOTTIME,
			fwfc,
			1,
			&FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6,
			NULL,
			FW_WEIGHT_HIGHEST_IMPORTANT,
			FWP_ACTION_PERMIT,
			FWPM_FILTER_FLAG_BOOTTIME,
			filter_ids
		);

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_BOOTTIME,
			fwfc,
			1,
			&FWPM_LAYER_INBOUND_ICMP_ERROR_V4,
			NULL,
			FW_WEIGHT_HIGHEST_IMPORTANT,
			FWP_ACTION_PERMIT,
			FWPM_FILTER_FLAG_BOOTTIME,
			filter_ids
		);

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_BOOTTIME,
			fwfc,
			1,
			&FWPM_LAYER_INBOUND_ICMP_ERROR_V6,
			NULL,
			FW_WEIGHT_HIGHEST_IMPORTANT,
			FWP_ACTION_PERMIT,
			FWPM_FILTER_FLAG_BOOTTIME,
			filter_ids
		);

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_BOOTTIME,
			fwfc,
			1,
			&FWPM_LAYER_OUTBOUND_ICMP_ERROR_V4,
			NULL,
			FW_WEIGHT_HIGHEST_IMPORTANT,
			FWP_ACTION_PERMIT,
			FWPM_FILTER_FLAG_BOOTTIME,
			filter_ids
		);

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_BOOTTIME,
			fwfc,
			1,
			&FWPM_LAYER_OUTBOUND_ICMP_ERROR_V6,
			NULL,
			FW_WEIGHT_HIGHEST_IMPORTANT,
			FWP_ACTION_PERMIT,
			FWPM_FILTER_FLAG_BOOTTIME,
			filter_ids
		);

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_BOOTTIME,
			NULL,
			0,
			&FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4,
			NULL,
			FW_WEIGHT_LOWEST,
			FWP_ACTION_BLOCK,
			FWPM_FILTER_FLAG_BOOTTIME,
			filter_ids
		);

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_BOOTTIME,
			NULL,
			0,
			&FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6,
			NULL,
			FW_WEIGHT_LOWEST,
			FWP_ACTION_BLOCK,
			FWPM_FILTER_FLAG_BOOTTIME,
			filter_ids
		);

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_BOOTTIME,
			NULL,
			0,
			&FWPM_LAYER_INBOUND_ICMP_ERROR_V4,
			NULL,
			FW_WEIGHT_LOWEST,
			FWP_ACTION_BLOCK,
			FWPM_FILTER_FLAG_BOOTTIME,
			filter_ids
		);

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_BOOTTIME,
			NULL,
			0,
			&FWPM_LAYER_INBOUND_ICMP_ERROR_V6,
			NULL,
			FW_WEIGHT_LOWEST,
			FWP_ACTION_BLOCK,
			FWPM_FILTER_FLAG_BOOTTIME,
			filter_ids
		);

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_BOOTTIME,
			NULL,
			0,
			&FWPM_LAYER_OUTBOUND_ICMP_ERROR_V4,
			NULL,
			FW_WEIGHT_LOWEST,
			FWP_ACTION_BLOCK,
			FWPM_FILTER_FLAG_BOOTTIME,
			filter_ids
		);

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_BOOTTIME,
			NULL,
			0,
			&FWPM_LAYER_OUTBOUND_ICMP_ERROR_V6,
			NULL,
			FW_WEIGHT_LOWEST,
			FWP_ACTION_BLOCK,
			FWPM_FILTER_FLAG_BOOTTIME,
			filter_ids
		);

		// win7+ boot-time features
		fwfc[0].fieldKey = FWPM_CONDITION_FLAGS;
		fwfc[0].matchType = FWP_MATCH_FLAGS_NONE_SET;
		fwfc[0].conditionValue.type = FWP_UINT32;
		fwfc[0].conditionValue.uint32 = FWP_CONDITION_FLAG_IS_OUTBOUND_PASS_THRU | FWP_CONDITION_FLAG_IS_INBOUND_PASS_THRU;

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_BOOTTIME,
			fwfc,
			1,
			&FWPM_LAYER_IPFORWARD_V4,
			NULL,
			FW_WEIGHT_APP,
			FWP_ACTION_BLOCK,
			FWPM_FILTER_FLAG_BOOTTIME,
			filter_ids
		);

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_BOOTTIME,
			fwfc,
			1,
			&FWPM_LAYER_IPFORWARD_V6,
			NULL,
			FW_WEIGHT_APP,
			FWP_ACTION_BLOCK,
			FWPM_FILTER_FLAG_BOOTTIME,
			filter_ids
		);
	}

	action = _r_config_getboolean (L"BlockOutboundConnections", TRUE) ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT;

	// block outbound connection
	if (action == FWP_ACTION_BLOCK)
	{
		// workaround #689
		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_BLOCK_CONNECTION,
			NULL,
			0,
			&FWPM_LAYER_ALE_AUTH_CONNECT_V4,
			&FWPM_CALLOUT_TCP_TEMPLATES_CONNECT_LAYER_V4,
			FW_WEIGHT_LOWEST,
			FWP_ACTION_CALLOUT_TERMINATING,
			0,
			filter_ids
		);

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_BLOCK_CONNECTION,
			NULL,
			0,
			&FWPM_LAYER_ALE_AUTH_CONNECT_V6,
			&FWPM_CALLOUT_TCP_TEMPLATES_CONNECT_LAYER_V6,
			FW_WEIGHT_LOWEST,
			FWP_ACTION_CALLOUT_TERMINATING,
			0,
			filter_ids
		);
	}

	// fallback
	_wfp_createfilter (
		engine_handle,
		DATA_FILTER_GENERAL,
		FW_NAME_BLOCK_CONNECTION,
		NULL,
		0,
		&FWPM_LAYER_ALE_AUTH_CONNECT_V4,
		NULL,
		FW_WEIGHT_LOWEST,
		action,
		0,
		filter_ids
	);

	_wfp_createfilter (
		engine_handle,
		DATA_FILTER_GENERAL,
		FW_NAME_BLOCK_CONNECTION,
		NULL,
		0,
		&FWPM_LAYER_ALE_AUTH_CONNECT_V6,
		NULL,
		FW_WEIGHT_LOWEST,
		action,
		0,
		filter_ids
	);

	// block inbound connections
	if (_r_config_getboolean (L"UseStealthMode", TRUE) || _r_config_getboolean (L"BlockInboundConnections", TRUE))
	{
		action = FWP_ACTION_BLOCK;
	}
	else
	{
		action = FWP_ACTION_PERMIT;
	}

	_wfp_createfilter (
		engine_handle,
		DATA_FILTER_GENERAL,
		FW_NAME_BLOCK_RECVACCEPT,
		NULL,
		0,
		&FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4,
		NULL,
		FW_WEIGHT_LOWEST,
		action,
		0,
		filter_ids
	);

	_wfp_createfilter (
		engine_handle,
		DATA_FILTER_GENERAL,
		FW_NAME_BLOCK_RECVACCEPT,
		NULL,
		0,
		&FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6,
		NULL,
		FW_WEIGHT_LOWEST,
		action,
		0,
		filter_ids
	);

	if (!is_intransact)
	{
		_wfp_transact_commit (engine_handle, DBG_ARG_VAR);

		for (ULONG_PTR i = 0; i < _r_obj_getarraysize (filter_ids); i++)
		{
			guid = _r_obj_getarrayitem (filter_ids, i);

			_app_setfiltersecurity (engine_handle, guid, TRUE, DBG_ARG_VAR);
		}

		_r_queuedlock_releaseshared (&lock_transaction);
	}

	_app_restoreinterfacestate (_r_app_gethwnd (), is_enabled);

	return TRUE;
}

_Success_ (return == ERROR_SUCCESS)
ULONG _wfp_dumpcallouts (
	_In_ HANDLE engine_handle,
	_In_ LPCGUID provider_id,
	_Outptr_ PR_ARRAY_PTR out_buffer
)
{
	FWPM_CALLOUT **callouts_enum = NULL;
	FWPM_CALLOUT *callout;
	PR_ARRAY guids = NULL;
	HANDLE enum_handle = NULL;
	ULONG status;
	UINT32 return_count;

	*out_buffer = NULL;

	status = FwpmCalloutCreateEnumHandle (engine_handle, NULL, &enum_handle);

	if (status != ERROR_SUCCESS)
		return status;

	status = FwpmCalloutEnum (engine_handle, enum_handle, UINT32_MAX, &callouts_enum, &return_count);

	if (status != ERROR_SUCCESS)
		goto CleanupExit;

	if (!callouts_enum)
	{
		status = ERROR_NOT_FOUND;

		goto CleanupExit;
	}

	guids = _r_obj_createarray_ex (sizeof (GUID), return_count, NULL);

	for (UINT32 i = 0; i < return_count; i++)
	{
		callout = callouts_enum[i];

		if (!callout || !callout->providerKey)
			continue;

		if (IsEqualGUID (callout->providerKey, provider_id))
			_r_obj_addarrayitem (guids, &callout->calloutKey);
	}

	if (_r_obj_isempty2 (guids))
	{
		status = ERROR_NOT_FOUND;

		_r_obj_dereference (guids);
	}

CleanupExit:

	if (status == ERROR_SUCCESS)
		*out_buffer = guids;

	if (enum_handle)
		FwpmCalloutDestroyEnumHandle (engine_handle, enum_handle);

	if (callouts_enum)
		FwpmFreeMemory ((PVOID_PTR)&callouts_enum);

	return status;
}

_Success_ (return == ERROR_SUCCESS)
ULONG _wfp_dumpfilters (
	_In_ HANDLE engine_handle,
	_In_ LPCGUID provider_id,
	_Outptr_ PR_ARRAY_PTR out_buffer
)
{
	FWPM_FILTER **filters_enum = NULL;
	FWPM_FILTER *filter;
	PR_ARRAY guids = NULL;
	HANDLE enum_handle = NULL;
	ULONG status;
	UINT32 return_count;

	*out_buffer = NULL;

	status = FwpmFilterCreateEnumHandle (engine_handle, NULL, &enum_handle);

	if (status != ERROR_SUCCESS)
		return status;

	status = FwpmFilterEnum (engine_handle, enum_handle, UINT32_MAX, &filters_enum, &return_count);

	if (status != ERROR_SUCCESS)
		goto CleanupExit;

	if (!filters_enum)
	{
		status = ERROR_NOT_FOUND;

		goto CleanupExit;
	}

	guids = _r_obj_createarray_ex (sizeof (GUID), return_count, NULL);

	for (UINT32 i = 0; i < return_count; i++)
	{
		filter = filters_enum[i];

		if (!filter || !filter->providerKey)
			continue;

		if (IsEqualGUID (filter->providerKey, provider_id))
			_r_obj_addarrayitem (guids, &filter->filterKey);
	}

	if (_r_obj_isempty2 (guids))
	{
		status = ERROR_NOT_FOUND;

		_r_obj_dereference (guids);
	}

CleanupExit:

	if (status == ERROR_SUCCESS)
		*out_buffer = guids;

	if (enum_handle)
		FwpmFilterDestroyEnumHandle (engine_handle, enum_handle);

	if (filters_enum)
		FwpmFreeMemory ((PVOID_PTR)&filters_enum);

	return status;
}

VOID NTAPI _wfp_applythread (
	_In_ PVOID arglist,
	_In_ ULONG busy_count
)
{
	PITEM_CONTEXT context;
	HANDLE engine_handle;
	LONG dpi_value;

	_r_queuedlock_acquireshared (&lock_apply);

	context = (PITEM_CONTEXT)arglist;
	engine_handle = _wfp_getenginehandle ();

	if (engine_handle)
	{
		// dropped packets logging (win7+)
		if (config.is_neteventset)
			_wfp_logunsubscribe (engine_handle);

		if (context->is_install)
		{
			if (_r_sys_isosversiongreaterorequal (WINDOWS_10))
				_app_wufixenable (context->hwnd, _app_wufixenabled ());

			if (_wfp_initialize (context->hwnd, engine_handle))
				_wfp_installfilters (engine_handle);
		}
		else
		{
			if (_r_sys_isosversiongreaterorequal (WINDOWS_10))
				_app_wufixenable (context->hwnd, FALSE);

			_wfp_destroyfilters (engine_handle);
			_wfp_uninitialize (engine_handle, TRUE);
		}

		// dropped packets logging (win7+)
		if (config.is_neteventset)
			_wfp_logsubscribe (context->hwnd, engine_handle);
	}

	dpi_value = _r_dc_getwindowdpi (context->hwnd);

	_app_restoreinterfacestate (context->hwnd, TRUE);
	_app_setinterfacestate (context->hwnd, dpi_value);

	_r_freelist_deleteitem (&context_free_list, context);

	_app_profile_save (context->hwnd);

	_r_queuedlock_releaseshared (&lock_apply);
}

VOID _wfp_firewallenable (
	_In_ BOOLEAN is_enable
)
{
	static NET_FW_PROFILE_TYPE2 profile_types[] = {
		NET_FW_PROFILE2_DOMAIN,
		NET_FW_PROFILE2_PRIVATE,
		NET_FW_PROFILE2_PUBLIC
	};

	INetFwPolicy2 *INetFwPolicy = NULL;
	HRESULT status;

	status = CoCreateInstance (&CLSID_NetFwPolicy2, NULL, CLSCTX_INPROC_SERVER, &IID_INetFwPolicy2, &INetFwPolicy);

	if (FAILED (status))
		return;

	for (ULONG_PTR i = 0; i < RTL_NUMBER_OF (profile_types); i++)
	{
		status = INetFwPolicy2_put_FirewallEnabled (INetFwPolicy, profile_types[i], is_enable ? VARIANT_TRUE : VARIANT_FALSE);

		if (FAILED (status))
			_r_log_v (LOG_LEVEL_INFO, NULL, L"INetFwPolicy2_put_FirewallEnabled", status, L"%d", profile_types[i]);
	}

	INetFwPolicy2_Release (INetFwPolicy);
}

BOOLEAN _wfp_firewallisenabled ()
{
	static NET_FW_PROFILE_TYPE2 profile_types[] = {
		NET_FW_PROFILE2_DOMAIN,
		NET_FW_PROFILE2_PRIVATE,
		NET_FW_PROFILE2_PUBLIC
	};

	INetFwPolicy2 *INetFwPolicy = NULL;
	VARIANT_BOOL result = VARIANT_FALSE;
	HRESULT status;

	status = CoCreateInstance (&CLSID_NetFwPolicy2, NULL, CLSCTX_INPROC_SERVER, &IID_INetFwPolicy2, &INetFwPolicy);

	if (SUCCEEDED (status))
	{
		for (ULONG_PTR i = 0; i < RTL_NUMBER_OF (profile_types); i++)
		{
			status = INetFwPolicy2_get_FirewallEnabled (INetFwPolicy, profile_types[i], &result);

			if (SUCCEEDED (status))
			{
				if (result == VARIANT_TRUE)
					break;
			}
		}

		INetFwPolicy2_Release (INetFwPolicy);
	}

	if (result == VARIANT_TRUE)
		return TRUE;

	return FALSE;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _FwpmGetAppIdFromFileName1 (
	_In_ PR_STRING path,
	_In_ ENUM_TYPE_DATA type,
	_Out_ PVOID_PTR byte_blob
)
{
	PR_STRING original_path;
	PR_STRING path_root;
	R_STRINGREF path_skip_root;
	NTSTATUS status;

	*byte_blob = NULL;

	if (type == DATA_APP_REGULAR || type == DATA_APP_NETWORK || type == DATA_APP_SERVICE)
	{
		if (_r_str_gethash2 (path, TRUE) == config.ntoskrnl_hash)
		{
			ByteBlobAlloc (path->buffer, path->length + sizeof (UNICODE_NULL), byte_blob);

			return STATUS_SUCCESS;
		}
		else
		{
			status = _r_path_ntpathfromdos (path, &original_path);

			if (!NT_SUCCESS (status))
			{
				// file is inaccessible or not found, maybe low-level
				// driver preventing file access? try another way!
				if (status == STATUS_OBJECT_NAME_NOT_FOUND || status == STATUS_FILE_IS_A_DIRECTORY || status == STATUS_ACCESS_DENIED)
				{
					if (!_app_isappvalidpath (path))
					{
						return status;
					}
					else
					{
						// file path (without root)
						path_root = _r_obj_createstring2 (path);

						PathStripToRoot (path_root->buffer);

						_r_obj_trimstringtonullterminator (path_root);

						status = _r_path_ntpathfromdos (path_root, &original_path);

						if (!NT_SUCCESS (status))
						{
							_r_obj_dereference (path_root);

							return status;
						}

						// file path (without root)
						_r_obj_initializestringref (&path_skip_root, PathSkipRoot (path->buffer));

						_r_obj_movereference (&original_path, _r_obj_concatstringrefs (2, &original_path->sr, &path_skip_root));

						_r_str_tolower (&original_path->sr); // lower is important!

						_r_obj_dereference (path_root);
					}
				}
				else
				{
					return status;
				}
			}

			ByteBlobAlloc (original_path->buffer, original_path->length + sizeof (UNICODE_NULL), byte_blob);

			_r_obj_dereference (original_path);

			return STATUS_SUCCESS;
		}
	}
	else if (type == DATA_APP_PICO || type == DATA_APP_DEVICE)
	{
		original_path = _r_obj_createstring2 (path);

		if (type == DATA_APP_DEVICE)
			_r_str_tolower (&original_path->sr); // lower is important!

		ByteBlobAlloc (original_path->buffer, original_path->length + sizeof (UNICODE_NULL), byte_blob);

		_r_obj_dereference (original_path);

		return STATUS_SUCCESS;
	}

	return STATUS_FILE_NOT_AVAILABLE;
}

VOID ByteBlobAlloc (
	_In_ LPCVOID data,
	_In_ ULONG_PTR bytes_count,
	_Out_ PVOID_PTR byte_blob
)
{
	FWP_BYTE_BLOB *blob;

	blob = _r_mem_allocate (sizeof (FWP_BYTE_BLOB) + bytes_count);

	blob->size = (UINT)(UINT_PTR)bytes_count;
	blob->data = PTR_ADD_OFFSET (blob, sizeof (FWP_BYTE_BLOB));

	RtlCopyMemory (blob->data, data, bytes_count);

	*byte_blob = blob;
}

VOID ByteBlobFree (
	_Inout_ PVOID_PTR byte_blob
)
{
	FWP_BYTE_BLOB *original_blob;

	original_blob = *byte_blob;
	*byte_blob = NULL;

	if (original_blob)
		_r_mem_free (original_blob);
}
