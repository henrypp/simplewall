// simplewall
// Copyright (c) 2016-2021 Henry++

#include "global.h"

BOOLEAN _wfp_isfiltersapplying ()
{
	return _r_queuedlock_islocked (&lock_apply) || _r_queuedlock_islocked (&lock_transaction);
}

ENUM_INSTALL_TYPE _wfp_isproviderinstalled (_In_ HANDLE engine_handle)
{
	ENUM_INSTALL_TYPE result;
	FWPM_PROVIDER *ptr_provider;

	result = INSTALL_DISABLED;

	if (FwpmProviderGetByKey (engine_handle, &GUID_WfpProvider, &ptr_provider) == ERROR_SUCCESS)
	{
		if (ptr_provider)
		{
			if (ptr_provider->flags & FWPM_PROVIDER_FLAG_DISABLED)
			{
				//result = INSTALL_DISABLED;
			}
			else if (ptr_provider->flags & FWPM_PROVIDER_FLAG_PERSISTENT)
			{
				result = INSTALL_ENABLED;
			}
			else
			{
				result = INSTALL_ENABLED_TEMPORARY;
			}

			FwpmFreeMemory ((PVOID_PTR)&ptr_provider);
		}
	}

	return result;
}

ENUM_INSTALL_TYPE _wfp_issublayerinstalled (_In_ HANDLE engine_handle)
{
	ENUM_INSTALL_TYPE result;
	FWPM_SUBLAYER *ptr_sublayer;

	result = INSTALL_DISABLED;

	if (FwpmSubLayerGetByKey (engine_handle, &GUID_WfpSublayer, &ptr_sublayer) == ERROR_SUCCESS)
	{
		if (ptr_sublayer)
		{
			if (ptr_sublayer->flags & FWPM_SUBLAYER_FLAG_PERSISTENT)
			{
				result = INSTALL_ENABLED;
			}
			else
			{
				result = INSTALL_ENABLED_TEMPORARY;
			}

			FwpmFreeMemory ((PVOID_PTR)&ptr_sublayer);
		}
	}

	return result;
}

ENUM_INSTALL_TYPE _wfp_isfiltersinstalled ()
{
	HANDLE engine_handle;

	engine_handle = _wfp_getenginehandle ();

	if (engine_handle)
		return _wfp_isproviderinstalled (engine_handle);

	return INSTALL_DISABLED;
}

HANDLE _wfp_getenginehandle ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static HANDLE engine_handle = NULL;

	if (_r_initonce_begin (&init_once))
	{
		FWPM_SESSION session;
		ULONG code;
		ULONG attempts;

		attempts = 6;

		do
		{
			RtlZeroMemory (&session, sizeof (session));

			session.displayData.name = APP_NAME;
			session.displayData.description = APP_NAME;

			session.txnWaitTimeoutInMSec = TRANSACTION_TIMEOUT;

			code = FwpmEngineOpen (NULL, RPC_C_AUTHN_WINNT, NULL, &session, &engine_handle);

			if (code == ERROR_SUCCESS)
			{
				break;
			}
			else
			{
				if (code == EPT_S_NOT_REGISTERED)
				{
					// The error say that BFE service is not in the running state, so we wait.
					if (attempts != 1)
					{
						_r_sys_sleep (500);
						continue;
					}
				}

				_r_log (LOG_LEVEL_CRITICAL, NULL, L"FwpmEngineOpen", code, NULL);

				_r_show_errormessage (_r_app_gethwnd (), L"WFP engine initialization failed! Try again later.", code, NULL);

				RtlExitUserProcess (code);

				break;
			}
		}
		while (--attempts);

		_r_initonce_end (&init_once);
	}

	return engine_handle;
}

PR_STRING _wfp_getlayername (_In_ LPCGUID layer_guid)
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

	for (SIZE_T i = 0; i < RTL_NUMBER_OF (layer_guids); i++)
	{
		if (IsEqualGUID (layer_guid, layer_guids[i]))
			return _r_obj_createstring3 (&layer_names[i]);
	}

	_r_str_fromguid (layer_guid, TRUE, &string);

	return string;
}

BOOLEAN _wfp_initialize (_In_ HANDLE engine_handle)
{
	FWP_VALUE val;

	ULONG code;
	BOOLEAN is_success;
	BOOLEAN is_providerexist;
	BOOLEAN is_sublayerexist;
	BOOLEAN is_intransact;

	BOOLEAN is_secure;

	is_success = TRUE; // already initialized

	is_secure = _r_config_getboolean (L"IsSecureFilters", TRUE);

	_r_queuedlock_acquireshared (&lock_transaction);

	_app_setsecurityinfoforengine (engine_handle);

	// install engine provider and it's sublayer
	is_providerexist = (_wfp_isproviderinstalled (engine_handle) != INSTALL_DISABLED);
	is_sublayerexist = (_wfp_issublayerinstalled (engine_handle) != INSTALL_DISABLED);

	if (!is_providerexist || !is_sublayerexist)
	{
		is_intransact = _wfp_transact_start (engine_handle, __LINE__);

		if (!is_providerexist)
		{
			// create provider
			FWPM_PROVIDER provider = {0};

			provider.displayData.name = APP_NAME;
			provider.displayData.description = APP_NAME;

			provider.providerKey = GUID_WfpProvider;

			if (!config.is_filterstemporary)
				provider.flags = FWPM_PROVIDER_FLAG_PERSISTENT;

			code = FwpmProviderAdd (engine_handle, &provider, NULL);

			if (code != ERROR_SUCCESS && code != FWP_E_ALREADY_EXISTS)
			{
				if (is_intransact)
				{
					FwpmTransactionAbort (engine_handle);
					is_intransact = FALSE;
				}

				_r_log (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"FwpmProviderAdd", code, NULL);
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
			FWPM_SUBLAYER sublayer = {0};

			sublayer.displayData.name = APP_NAME;
			sublayer.displayData.description = APP_NAME;

			sublayer.providerKey = (LPGUID)&GUID_WfpProvider;
			sublayer.subLayerKey = GUID_WfpSublayer;
			sublayer.weight = (UINT16)_r_config_getlong (L"SublayerWeight", FW_SUBLAYER_WEIGHT); // highest weight for UINT16

			if (!config.is_filterstemporary)
				sublayer.flags = FWPM_SUBLAYER_FLAG_PERSISTENT;

			code = FwpmSubLayerAdd (engine_handle, &sublayer, NULL);

			if (code != ERROR_SUCCESS && code != FWP_E_ALREADY_EXISTS)
			{
				if (is_intransact)
				{
					FwpmTransactionAbort (engine_handle);
					is_intransact = FALSE;
				}

				_r_log (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"FwpmSubLayerAdd", code, NULL);
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
			if (!_wfp_transact_commit (engine_handle, __LINE__))
				is_success = FALSE;
		}
	}

	// set provider security information
	_app_setsecurityinfoforprovider (engine_handle, &GUID_WfpProvider, is_secure);

	// set sublayer security information
	_app_setsecurityinfoforsublayer (engine_handle, &GUID_WfpSublayer, is_secure);

	// set engine options
	RtlZeroMemory (&val, sizeof (val));

	// dropped packets logging (win7+)
	if (!config.is_neteventset)
	{
		FWP_VALUE0* fwp_query = NULL;

		// query net events state
		config.is_neteventenabled = FALSE;

		code = FwpmEngineGetOption (engine_handle, FWPM_ENGINE_COLLECT_NET_EVENTS, &fwp_query);

		if (code == ERROR_SUCCESS)
		{
			if (fwp_query)
			{
				config.is_neteventenabled = (fwp_query->type == FWP_UINT32) && (!!fwp_query->uint32);

				FwpmFreeMemory ((PVOID_PTR)&fwp_query);
			}
		}

		// enable net events (if it is disabled)
		if (config.is_neteventenabled)
		{
			config.is_neteventset = TRUE;
		}
		else
		{
			val.type = FWP_UINT32;
			val.uint32 = 1;

			code = FwpmEngineSetOption (engine_handle, FWPM_ENGINE_COLLECT_NET_EVENTS, &val);

			if (code != ERROR_SUCCESS)
			{
				_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmEngineSetOption", code, L"FWPM_ENGINE_COLLECT_NET_EVENTS");
			}
			else
			{
				config.is_neteventset = TRUE;
			}
		}

		if (config.is_neteventset)
		{
			_wfp_logsetoption (engine_handle);

			_wfp_logsubscribe (engine_handle);
		}
	}

	// packet queuing (win8+)
	if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
	{
		if (_r_config_getboolean (L"IsPacketQueuingEnabled", TRUE))
		{
			// Enables inbound or forward packet queuing independently.
			// when enabled, the system is able to evenly distribute cpu load
			// to multiple cpus for site-to-site ipsec tunnel scenarios.

			val.type = FWP_UINT32;
			val.uint32 = FWPM_ENGINE_OPTION_PACKET_QUEUE_INBOUND | FWPM_ENGINE_OPTION_PACKET_QUEUE_FORWARD;

			code = FwpmEngineSetOption (engine_handle, FWPM_ENGINE_PACKET_QUEUING, &val);

			if (code != ERROR_SUCCESS)
				_r_log (LOG_LEVEL_WARNING, NULL, L"FwpmEngineSetOption", code, L"FWPM_ENGINE_PACKET_QUEUING");
		}
	}

CleanupExit:

	_r_queuedlock_releaseshared (&lock_transaction);

	return is_success;
}

VOID _wfp_uninitialize (_In_ HANDLE engine_handle, _In_ BOOLEAN is_full)
{
	FWP_VALUE val;
	ULONG code;
	BOOLEAN is_intransact;

	_r_queuedlock_acquireshared (&lock_transaction);

	// dropped packets logging (win7+)
	if (config.is_neteventset)
	{
		_wfp_logunsubscribe (engine_handle);

		//if (_r_sys_validversion (6, 2))
		//{
		//	// monitor ipsec connection (win8+)
		//	val.type = FWP_UINT32;
		//	val.uint32 = 0;

		//	FwpmEngineSetOption (engine_handle, FWPM_ENGINE_MONITOR_IPSEC_CONNECTIONS, &val);

		//	// packet queuing (win8+)
		//	val.type = FWP_UINT32;
		//	val.uint32 = FWPM_ENGINE_OPTION_PACKET_QUEUE_NONE;

		//	FwpmEngineSetOption (engine_handle, FWPM_ENGINE_PACKET_QUEUING, &val);
		//}
	}

	if (!config.is_neteventenabled && config.is_neteventset)
	{
		RtlZeroMemory (&val, sizeof (val));

		val.type = FWP_UINT32;
		val.uint32 = 0;

		code = FwpmEngineSetOption (engine_handle, FWPM_ENGINE_COLLECT_NET_EVENTS, &val);

		if (code == ERROR_SUCCESS)
		{
			config.is_neteventset = FALSE;
		}
	}

	if (is_full)
	{
		static LPCGUID callouts[] = {
			&GUID_WfpOutboundCallout4_DEPRECATED,
			&GUID_WfpOutboundCallout6_DEPRECATED,
			&GUID_WfpInboundCallout4_DEPRECATED,
			&GUID_WfpInboundCallout6_DEPRECATED,
			&GUID_WfpListenCallout4_DEPRECATED,
			&GUID_WfpListenCallout6_DEPRECATED,
		};

		_app_setsecurityinfoforprovider (engine_handle, &GUID_WfpProvider, FALSE);
		_app_setsecurityinfoforsublayer (engine_handle, &GUID_WfpSublayer, FALSE);

		for (SIZE_T i = 0; i < RTL_NUMBER_OF (callouts); i++)
			_app_setsecurityinfoforcallout (engine_handle, callouts[i], FALSE);

		is_intransact = _wfp_transact_start (engine_handle, __LINE__);

		// destroy callouts (deprecated)
		for (SIZE_T i = 0; i < RTL_NUMBER_OF (callouts); i++)
		{
			code = FwpmCalloutDeleteByKey (engine_handle, callouts[i]);

			if (code != ERROR_SUCCESS && code != FWP_E_CALLOUT_NOT_FOUND)
				_r_log (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"FwpmCalloutDeleteByKey", code, NULL);
		}

		// destroy sublayer
		code = FwpmSubLayerDeleteByKey (engine_handle, &GUID_WfpSublayer);

		if (code != ERROR_SUCCESS && code != FWP_E_SUBLAYER_NOT_FOUND)
			_r_log (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"FwpmSubLayerDeleteByKey", code, NULL);

		// destroy provider
		code = FwpmProviderDeleteByKey (engine_handle, &GUID_WfpProvider);

		if (code != ERROR_SUCCESS && code != FWP_E_PROVIDER_NOT_FOUND)
			_r_log (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"FwpmProviderDeleteByKey", code, NULL);

		if (is_intransact)
			_wfp_transact_commit (engine_handle, __LINE__);
	}

	_r_queuedlock_releaseshared (&lock_transaction);
}

VOID _wfp_installfilters (_In_ HANDLE engine_handle)
{
	// dump all filters into array
	PR_ARRAY guids;
	PR_LIST rules;
	LPCGUID guid;

	BOOLEAN is_intransact;
	BOOLEAN is_secure;

	// set security information
	_app_setsecurityinfoforprovider (engine_handle, &GUID_WfpProvider, FALSE);
	_app_setsecurityinfoforsublayer (engine_handle, &GUID_WfpSublayer, FALSE);

	_wfp_clearfilter_ids ();

	_r_queuedlock_acquireshared (&lock_transaction);

	guids = _wfp_dumpfilters (engine_handle, &GUID_WfpProvider);

	// restore filters security
	if (guids)
	{
		for (SIZE_T i = 0; i < _r_obj_getarraysize (guids); i++)
		{
			guid = _r_obj_getarrayitem (guids, i);

			if (guid)
				_app_setsecurityinfoforfilter (engine_handle, guid, FALSE, __LINE__);
		}
	}

	is_intransact = _wfp_transact_start (engine_handle, __LINE__);

	// destroy all filters
	if (guids)
	{
		for (SIZE_T i = 0; i < _r_obj_getarraysize (guids); i++)
		{
			guid = _r_obj_getarrayitem (guids, i);

			if (guid)
				_wfp_deletefilter (engine_handle, guid);
		}

		_r_obj_dereference (guids);
	}

	rules = _r_obj_createlist (&_r_obj_dereference);

	// apply apps rules
	PITEM_APP ptr_app;
	SIZE_T enum_key;

	enum_key = 0;

	_r_queuedlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtablepointer (apps_table, &ptr_app, NULL, &enum_key))
	{
		if (ptr_app->is_enabled)
			_r_obj_addlistitem (rules, _r_obj_reference (ptr_app));
	}

	_r_queuedlock_releaseshared (&lock_apps);

	if (!_r_obj_islistempty (rules))
	{
		_wfp_create3filters (engine_handle, rules, __LINE__, is_intransact);

		_r_obj_clearlist (rules);
	}

	// apply blocklist/system/user rules
	PITEM_RULE ptr_rule;

	_r_queuedlock_acquireshared (&lock_rules);

	for (SIZE_T i = 0; i < _r_obj_getlistsize (rules_list); i++)
	{
		ptr_rule = _r_obj_getlistitem (rules_list, i);

		if (ptr_rule)
		{
			if (ptr_rule->is_enabled)
				_r_obj_addlistitem (rules, _r_obj_reference (ptr_rule));
		}
	}

	_r_queuedlock_releaseshared (&lock_rules);

	if (!_r_obj_islistempty (rules))
	{
		_wfp_create4filters (engine_handle, rules, __LINE__, is_intransact);

		_r_obj_clearlist (rules);
	}

	// apply internal rules
	_wfp_create2filters (engine_handle, __LINE__, is_intransact);

	if (is_intransact)
		_wfp_transact_commit (engine_handle, __LINE__);

	// secure filters
	is_secure = _r_config_getboolean (L"IsSecureFilters", TRUE);

	if (is_secure)
	{
		guids = _wfp_dumpfilters (engine_handle, &GUID_WfpProvider);

		if (guids)
		{
			for (SIZE_T i = 0; i < _r_obj_getarraysize (guids); i++)
			{
				guid = _r_obj_getarrayitem (guids, i);

				if (guid)
					_app_setsecurityinfoforfilter (engine_handle, guid, is_secure, __LINE__);
			}

			_r_obj_dereference (guids);
		}
	}

	_app_setsecurityinfoforprovider (engine_handle, &GUID_WfpProvider, is_secure);
	_app_setsecurityinfoforsublayer (engine_handle, &GUID_WfpSublayer, is_secure);

	_r_obj_dereference (rules);

	_r_queuedlock_releaseshared (&lock_transaction);
}

BOOLEAN _wfp_transact_start (_In_ HANDLE engine_handle, _In_ UINT line)
{
	ULONG code = FwpmTransactionBegin (engine_handle, 0);

	//if (code == FWP_E_TXN_IN_PROGRESS)
	//	return FALSE;

	if (code != ERROR_SUCCESS)
	{
		_r_log_v (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"FwpmTransactionBegin", code, L"#%" PRIu32, line);
		return FALSE;
	}

	return TRUE;
}

BOOLEAN _wfp_transact_commit (_In_ HANDLE engine_handle, _In_ UINT line)
{
	ULONG code = FwpmTransactionCommit (engine_handle);

	if (code != ERROR_SUCCESS)
	{
		FwpmTransactionAbort (engine_handle);

		_r_log_v (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"FwpmTransactionCommit", code, L"#%" PRIu32, line);
		return FALSE;

	}

	return TRUE;
}

BOOLEAN _wfp_deletefilter (_In_ HANDLE engine_handle, _In_ LPCGUID filter_id)
{
	PR_STRING string;
	ULONG code;

	code = FwpmFilterDeleteByKey (engine_handle, filter_id);

#if !defined(_DEBUG)
	if (code != ERROR_SUCCESS && code != FWP_E_FILTER_NOT_FOUND)
#else
	if (code != ERROR_SUCCESS)
#endif // !DEBUG
	{
		_r_str_fromguid (filter_id, TRUE, &string);

		_r_log (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"FwpmFilterDeleteByKey", code, _r_obj_getstringordefault (string, SZ_EMPTY));

		if (string)
			_r_obj_dereference (string);

		return FALSE;
	}

	return TRUE;
}

FORCEINLINE LPCWSTR _wfp_filtertypetostring (_In_ ENUM_TYPE_DATA filter_type)
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

ULONG _wfp_createcallout (_In_ HANDLE engine_handle, _In_ LPCGUID layer_key, _In_ LPCGUID callout_key)
{
	FWPM_CALLOUT0 callout = {0};
	ULONG code;

	callout.displayData.name = APP_NAME;
	callout.displayData.description = APP_NAME;

	if (!config.is_filterstemporary)
		callout.flags = FWPM_CALLOUT_FLAG_PERSISTENT;

	callout.providerKey = (LPGUID)&GUID_WfpProvider;

	RtlCopyMemory (&callout.calloutKey, callout_key, sizeof (GUID));
	RtlCopyMemory (&callout.applicableLayer, layer_key, sizeof (GUID));

	code = FwpmCalloutAdd (engine_handle, &callout, NULL, NULL);

	if (code != ERROR_SUCCESS && code != FWP_E_ALREADY_EXISTS)
		_r_log (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"FwpmCalloutAdd", code, NULL);

	return code;
}

ULONG _wfp_createfilter (_In_ HANDLE engine_handle, _In_ ENUM_TYPE_DATA type, _In_opt_ LPCWSTR filter_name, _In_reads_ (count) FWPM_FILTER_CONDITION *lpcond, _In_ UINT32 count, _In_ LPCGUID layer_id, _In_opt_ LPCGUID callout_id, _In_ UINT8 weight, _In_ FWP_ACTION_TYPE action, _In_ UINT32 flags, _Inout_opt_ PR_ARRAY guids)
{
	FWPM_FILTER filter = {0};
	WCHAR filter_description[128];
	LPCWSTR filter_type_string;
	UINT64 filter_id;
	ULONG code;

	// create filter guid
	code = _r_math_createguid (&filter.filterKey);

	if (code != ERROR_SUCCESS)
		return code;

	filter_type_string = _wfp_filtertypetostring (type);

	if (filter_type_string && filter_name)
	{
		_r_str_printf (filter_description, RTL_NUMBER_OF (filter_description), L"%s\\%s", filter_type_string, filter_name);
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
		if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
			filter.flags |= FWPM_FILTER_FLAG_INDEXED;
	}

	if (flags)
		filter.flags |= flags;

	filter.displayData.name = APP_NAME;
	filter.displayData.description = filter_description;

	filter.providerKey = (LPGUID)&GUID_WfpProvider;
	filter.subLayerKey = GUID_WfpSublayer;
	filter.weight.type = FWP_UINT8;
	filter.weight.uint8 = weight;
	filter.action.type = action;

	if (count)
	{
		filter.numFilterConditions = count;
		filter.filterCondition = lpcond;
	}

	RtlCopyMemory (&filter.layerKey, layer_id, sizeof (GUID));

	if (callout_id)
		RtlCopyMemory (&filter.action.calloutKey, callout_id, sizeof (GUID));

	code = FwpmFilterAdd (engine_handle, &filter, NULL, &filter_id);

	if (code == ERROR_SUCCESS)
	{
		if (guids)
			_r_obj_addarrayitem (guids, &filter.filterKey);
	}
	else
	{
		PR_STRING layer_name;

		layer_name = _wfp_getlayername (layer_id);

		_r_log_v (
			LOG_LEVEL_ERROR,
			&GUID_TrayIcon,
			L"FwpmFilterAdd",
			code,
			L"%s\\%s",
			_r_obj_getstring (layer_name),
			filter.displayData.description
		);

		if (layer_name)
			_r_obj_dereference (layer_name);
	}

	return code;
}

VOID _wfp_clearfilter_ids ()
{
	PITEM_APP ptr_app;
	PITEM_RULE ptr_rule;
	SIZE_T enum_key;

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

	for (SIZE_T i = 0; i < _r_obj_getlistsize (rules_list); i++)
	{
		ptr_rule = _r_obj_getlistitem (rules_list, i);

		if (!ptr_rule)
			continue;

		ptr_rule->is_haveerrors = FALSE;

		_r_obj_cleararray (ptr_rule->guids);
	}

	_r_queuedlock_releaseshared (&lock_rules);
}

VOID _wfp_destroyfilters (_In_ HANDLE engine_handle)
{
	_wfp_clearfilter_ids ();

	// destroy all filters
	PR_ARRAY guids;

	_r_queuedlock_acquireshared (&lock_transaction);

	guids = _wfp_dumpfilters (engine_handle, &GUID_WfpProvider);

	if (guids)
	{
		_wfp_destroyfilters_array (engine_handle, guids, __LINE__);

		_r_obj_dereference (guids);
	}

	_r_queuedlock_releaseshared (&lock_transaction);
}

BOOLEAN _wfp_destroyfilters_array (_In_ HANDLE engine_handle, _In_ PR_ARRAY guids, _In_ UINT line)
{
	LPCGUID guid;
	BOOLEAN is_enabled;
	BOOLEAN is_intransact;

	is_enabled = _app_initinterfacestate (_r_app_gethwnd (), FALSE);

	_r_queuedlock_acquireshared (&lock_transaction);

	for (SIZE_T i = 0; i < _r_obj_getarraysize (guids); i++)
	{
		guid = _r_obj_getarrayitem (guids, i);

		if (guid)
			_app_setsecurityinfoforfilter (engine_handle, guid, FALSE, line);
	}

	is_intransact = _wfp_transact_start (engine_handle, line);

	for (SIZE_T i = 0; i < _r_obj_getarraysize (guids); i++)
	{
		guid = _r_obj_getarrayitem (guids, i);

		if (guid)
			_wfp_deletefilter (engine_handle, guid);
	}

	if (is_intransact)
		_wfp_transact_commit (engine_handle, line);

	_r_queuedlock_releaseshared (&lock_transaction);

	_app_restoreinterfacestate (_r_app_gethwnd (), is_enabled);

	return TRUE;
}

BOOLEAN _wfp_createrulefilter (_In_ HANDLE engine_handle, _In_ ENUM_TYPE_DATA filter_type, _In_opt_ LPCWSTR filter_name, _In_opt_ ULONG_PTR app_hash, _In_opt_ PITEM_FILTER_CONFIG filter_config, _In_opt_ PR_STRINGREF rule_remote, _In_opt_ PR_STRINGREF rule_local, _In_ UINT8 weight, _In_ FWP_ACTION_TYPE action, _In_ UINT32 flags, _Inout_opt_ PR_ARRAY guids)
{
	FWPM_FILTER_CONDITION fwfc[8] = {0};
	UINT32 count = 0;

	ITEM_ADDRESS address;

	FWP_BYTE_BLOB *byte_blob = NULL;
	PITEM_APP ptr_app = NULL;
	BOOLEAN is_remoteaddr_set = FALSE;
	BOOLEAN is_remoteport_set = FALSE;
	BOOLEAN is_success = FALSE;

	FWP_DIRECTION direction;
	UINT8 protocol;
	ADDRESS_FAMILY af;

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
				_r_log (LOG_LEVEL_ERROR, NULL, TEXT (__FUNCTION__), 0, _app_getappdisplayname (ptr_app, TRUE));

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
				_r_log (LOG_LEVEL_ERROR, NULL, TEXT (__FUNCTION__), 0, _app_getappdisplayname (ptr_app, TRUE));

				goto CleanupExit;
			}
		}
		else
		{
			ULONG code = _FwpmGetAppIdFromFileName1 (ptr_app->original_path, ptr_app->type, &byte_blob);

			if (code == ERROR_SUCCESS)
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
				if (code != ERROR_FILE_NOT_FOUND && code != ERROR_PATH_NOT_FOUND)
					_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmGetAppIdFromFileName", code, _r_obj_getstring (ptr_app->original_path));

				goto CleanupExit;
			}
		}
	}

	FWP_V4_ADDR_AND_MASK fwp_addr4_and_mask1;
	FWP_V4_ADDR_AND_MASK fwp_addr4_and_mask2;

	FWP_V6_ADDR_AND_MASK fwp_addr6_and_mask1;
	FWP_V6_ADDR_AND_MASK fwp_addr6_and_mask2;

	FWP_RANGE fwp_range1;
	FWP_RANGE fwp_range2;

	// set ip/port condition
	{
		PR_STRINGREF rules[] = {
			rule_remote,
			rule_local
		};

		for (SIZE_T i = 0; i < RTL_NUMBER_OF (rules); i++)
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
					fwfc[count].fieldKey = ((i == 0) ? FWPM_CONDITION_IP_REMOTE_PORT : FWPM_CONDITION_IP_LOCAL_PORT);
				}
				else
				{
					fwfc[count].fieldKey = ((i == 0) ? FWPM_CONDITION_IP_REMOTE_ADDRESS : FWPM_CONDITION_IP_LOCAL_ADDRESS);
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
					fwfc[count].fieldKey = ((i == 0) ? FWPM_CONDITION_IP_REMOTE_PORT : FWPM_CONDITION_IP_LOCAL_PORT);
					fwfc[count].matchType = FWP_MATCH_EQUAL;
					fwfc[count].conditionValue.type = FWP_UINT16;
					fwfc[count].conditionValue.uint16 = address.port;

					count += 1;
				}
				else if (address.type == DATA_TYPE_IP)
				{
					fwfc[count].fieldKey = ((i == 0) ? FWPM_CONDITION_IP_REMOTE_ADDRESS : FWPM_CONDITION_IP_LOCAL_ADDRESS);
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
					fwfc[count].fieldKey = ((i == 0) ? FWPM_CONDITION_IP_REMOTE_PORT : FWPM_CONDITION_IP_LOCAL_PORT);
					fwfc[count].matchType = FWP_MATCH_EQUAL;
					fwfc[count].conditionValue.type = FWP_UINT16;
					fwfc[count].conditionValue.uint16 = address.port;

					count += 1;
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

			// win7+
			_wfp_createfilter (
				engine_handle,
				filter_type,
				filter_name,
				fwfc, count,
				&FWPM_LAYER_ALE_CONNECT_REDIRECT_V4,
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

			// win7+
			_wfp_createfilter (
				engine_handle,
				filter_type,
				filter_name,
				fwfc,
				count,
				&FWPM_LAYER_ALE_CONNECT_REDIRECT_V6,
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

			if (action == FWP_ACTION_BLOCK && (!is_remoteaddr_set && !is_remoteport_set))
			{
				_wfp_createfilter (
					engine_handle,
					filter_type,
					filter_name,
					fwfc,
					count,
					&FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V4,
					NULL,
					weight,
					FWP_ACTION_PERMIT,
					flags,
					guids
				);
			}
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

			if (action == FWP_ACTION_BLOCK && (!is_remoteaddr_set && !is_remoteport_set))
			{
				_wfp_createfilter (
					engine_handle,
					filter_type,
					filter_name,
					fwfc,
					count,
					&FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V6,
					NULL,
					weight,
					FWP_ACTION_PERMIT,
					flags,
					guids
				);
			}
		}
	}

	is_success = TRUE;

CleanupExit:

	if (ptr_app)
		_r_obj_dereference (ptr_app);

	ByteBlobFree (&byte_blob);

	return is_success;
}

BOOLEAN _wfp_create4filters (_In_ HANDLE engine_handle, _In_  PR_LIST rules, _In_ UINT line, _In_ BOOLEAN is_intransact)
{
	PR_ARRAY guids;
	LPCGUID guid;
	PITEM_RULE ptr_rule;
	BOOLEAN is_enabled;

	if (_r_obj_islistempty (rules))
		return FALSE;

	is_enabled = _app_initinterfacestate (_r_app_gethwnd (), FALSE);

	if (!is_intransact && _wfp_isfiltersapplying ())
		is_intransact = TRUE;

	guids = _r_obj_createarray (sizeof (GUID), NULL);

	if (!is_intransact)
	{
		for (SIZE_T i = 0; i < _r_obj_getlistsize (rules); i++)
		{
			ptr_rule = _r_obj_getlistitem (rules, i);

			if (!ptr_rule)
				continue;

			if (!_r_obj_isarrayempty (ptr_rule->guids))
			{
				_r_obj_addarrayitems (guids, ptr_rule->guids->items, ptr_rule->guids->count);
				_r_obj_cleararray (ptr_rule->guids);
			}
		}

		for (SIZE_T i = 0; i < _r_obj_getarraysize (guids); i++)
		{
			guid = _r_obj_getarrayitem (guids, i);

			if (guid)
				_app_setsecurityinfoforfilter (engine_handle, guid, FALSE, line);
		}

		_r_queuedlock_acquireshared (&lock_transaction);
		is_intransact = !_wfp_transact_start (engine_handle, line);
	}

	for (SIZE_T i = 0; i < _r_obj_getarraysize (guids); i++)
	{
		guid = _r_obj_getarrayitem (guids, i);

		if (guid)
			_wfp_deletefilter (engine_handle, guid);
	}

	R_STRINGREF remote_remaining_part;
	R_STRINGREF local_remaining_part;
	R_STRINGREF rule_remote_part;
	R_STRINGREF rule_local_part;
	LPCWSTR rule_name;

	for (SIZE_T i = 0; i < _r_obj_getlistsize (rules); i++)
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
					ptr_rule->guids
					))
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
					ptr_rule->guids
					))
				{
					ptr_rule->is_haveerrors = TRUE;
				}
			}

			if (!_r_obj_ishashtableempty (ptr_rule->apps))
			{
				ULONG_PTR hash_code;
				SIZE_T enum_key = 0;

				while (_r_obj_enumhashtable (ptr_rule->apps, NULL, &hash_code, &enum_key))
				{
					if (ptr_rule->is_forservices && _app_issystemhash (hash_code))
						continue;

					if (!_wfp_createrulefilter (engine_handle, ptr_rule->type, rule_name, hash_code, &ptr_rule->config, &rule_remote_part, &rule_local_part, ptr_rule->weight, ptr_rule->action, 0, ptr_rule->guids))
					{
						ptr_rule->is_haveerrors = TRUE;
					}
				}
			}
			else
			{
				if (!_wfp_createrulefilter (engine_handle, ptr_rule->type, rule_name, 0, &ptr_rule->config, &rule_remote_part, &rule_local_part, ptr_rule->weight, ptr_rule->action, 0, ptr_rule->guids))
				{
					ptr_rule->is_haveerrors = TRUE;
				}
			}

			if (remote_remaining_part.length == 0 && local_remaining_part.length == 0)
				break;

			if (remote_remaining_part.length != 0)
			{
				_r_str_splitatchar (&remote_remaining_part, DIVIDER_RULE[0], &rule_remote_part, &remote_remaining_part);
			}

			if (local_remaining_part.length != 0)
			{
				_r_str_splitatchar (&local_remaining_part, DIVIDER_RULE[0], &rule_local_part, &local_remaining_part);
			}
		}
	}

	if (!is_intransact)
	{
		BOOLEAN is_secure;

		_wfp_transact_commit (engine_handle, line);

		is_secure = _r_config_getboolean (L"IsSecureFilters", TRUE);

		if (is_secure)
		{
			for (SIZE_T i = 0; i < _r_obj_getlistsize (rules); i++)
			{
				ptr_rule = _r_obj_getlistitem (rules, i);

				if (ptr_rule && ptr_rule->is_enabled)
				{
					for (SIZE_T j = 0; j < _r_obj_getarraysize (ptr_rule->guids); j++)
					{
						guid = _r_obj_getarrayitem (ptr_rule->guids, j);

						if (guid)
							_app_setsecurityinfoforfilter (engine_handle, guid, is_secure, line);
					}
				}
			}
		}

		_r_queuedlock_releaseshared (&lock_transaction);
	}

	_app_restoreinterfacestate (_r_app_gethwnd (), is_enabled);

	_r_obj_dereference (guids);

	return TRUE;
}

BOOLEAN _wfp_create3filters (_In_ HANDLE engine_handle, _In_ PR_LIST rules, _In_ UINT line, _In_ BOOLEAN is_intransact)
{
	if (_r_obj_islistempty (rules))
		return FALSE;

	PR_ARRAY guids;
	LPCGUID guid;
	PITEM_APP ptr_app;
	BOOLEAN is_enabled;

	if (!is_intransact && _wfp_isfiltersapplying ())
		is_intransact = TRUE;

	guids = _r_obj_createarray (sizeof (GUID), NULL);

	is_enabled = _app_initinterfacestate (_r_app_gethwnd (), FALSE);

	if (!is_intransact)
	{
		for (SIZE_T i = 0; i < _r_obj_getlistsize (rules); i++)
		{
			ptr_app = _r_obj_getlistitem (rules, i);

			if (ptr_app && !_r_obj_isarrayempty (ptr_app->guids))
			{
				_r_obj_addarrayitems (guids, ptr_app->guids->items, ptr_app->guids->count);
				_r_obj_cleararray (ptr_app->guids);
			}
		}

		for (SIZE_T i = 0; i < _r_obj_getarraysize (guids); i++)
		{
			guid = _r_obj_getarrayitem (guids, i);

			if (guid)
				_app_setsecurityinfoforfilter (engine_handle, guid, FALSE, line);
		}

		_r_queuedlock_acquireshared (&lock_transaction);
		is_intransact = !_wfp_transact_start (engine_handle, line);
	}

	for (SIZE_T i = 0; i < _r_obj_getarraysize (guids); i++)
	{
		guid = _r_obj_getarrayitem (guids, i);

		if (guid)
			_wfp_deletefilter (engine_handle, guid);
	}

	for (SIZE_T i = 0; i < _r_obj_getlistsize (rules); i++)
	{
		ptr_app = _r_obj_getlistitem (rules, i);

		if (ptr_app && ptr_app->is_enabled)
		{
			if (!_wfp_createrulefilter (engine_handle, ptr_app->type, _app_getappdisplayname (ptr_app, TRUE), ptr_app->app_hash, NULL, NULL, NULL, FW_WEIGHT_APP, FWP_ACTION_PERMIT, 0, ptr_app->guids))
			{
				ptr_app->is_haveerrors = TRUE;
			}
		}
	}

	if (!is_intransact)
	{
		BOOLEAN is_secure;

		_wfp_transact_commit (engine_handle, line);

		is_secure = _r_config_getboolean (L"IsSecureFilters", TRUE);

		if (is_secure)
		{
			for (SIZE_T i = 0; i < _r_obj_getlistsize (rules); i++)
			{
				ptr_app = _r_obj_getlistitem (rules, i);

				if (ptr_app)
				{
					for (SIZE_T j = 0; j < _r_obj_getarraysize (ptr_app->guids); j++)
					{
						guid = _r_obj_getarrayitem (ptr_app->guids, j);

						if (guid)
							_app_setsecurityinfoforfilter (engine_handle, guid, is_secure, line);
					}
				}
			}
		}

		_r_queuedlock_releaseshared (&lock_transaction);
	}

	_app_restoreinterfacestate (_r_app_gethwnd (), is_enabled);

	_r_obj_dereference (guids);

	return TRUE;
}

BOOLEAN _wfp_create2filters (_In_ HANDLE engine_handle, _In_ UINT line, _In_ BOOLEAN is_intransact)
{
	LPCGUID guid;
	BOOLEAN is_enabled;

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

	is_enabled = _app_initinterfacestate (_r_app_gethwnd (), FALSE);

	if (!is_intransact && _wfp_isfiltersapplying ())
		is_intransact = TRUE;

	if (!is_intransact)
	{
		if (!_r_obj_isarrayempty (filter_ids))
		{
			for (SIZE_T i = 0; i < _r_obj_getarraysize (filter_ids); i++)
			{
				guid = _r_obj_getarrayitem (filter_ids, i);

				if (guid)
					_app_setsecurityinfoforfilter (engine_handle, guid, FALSE, line);
			}
		}

		_r_queuedlock_acquireshared (&lock_transaction);
		is_intransact = !_wfp_transact_start (engine_handle, line);
	}

	if (!_r_obj_isarrayempty (filter_ids))
	{
		for (SIZE_T i = 0; i < _r_obj_getarraysize (filter_ids); i++)
		{
			guid = _r_obj_getarrayitem (filter_ids, i);

			if (guid)
				_wfp_deletefilter (engine_handle, guid);
		}

		_r_obj_cleararray (filter_ids);
	}

	FWPM_FILTER_CONDITION fwfc[3] = {0};
	ITEM_ADDRESS address;

	// add loopback connections permission
	if (_r_config_getboolean (L"AllowLoopbackConnections", TRUE))
	{
		// match all loopback (localhost) data
		fwfc[0].fieldKey = FWPM_CONDITION_FLAGS;
		fwfc[0].matchType = FWP_MATCH_FLAGS_ALL_SET;
		fwfc[0].conditionValue.type = FWP_UINT32;
		fwfc[0].conditionValue.uint32 = FWP_CONDITION_FLAG_IS_LOOPBACK;

		// tests if the network traffic is (non-)app container loopback traffic (win8+)
		if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
		{
			fwfc[0].matchType = FWP_MATCH_FLAGS_ANY_SET;
			fwfc[0].conditionValue.uint32 |= FWP_CONDITION_FLAG_IS_APPCONTAINER_LOOPBACK;
		}

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

		// win7+
		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_LOOPBACK,
			fwfc,
			1,
			&FWPM_LAYER_ALE_CONNECT_REDIRECT_V4,
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
			&FWPM_LAYER_ALE_CONNECT_REDIRECT_V6,
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

		_wfp_createfilter (
			engine_handle,
			DATA_FILTER_GENERAL,
			FW_NAME_LOOPBACK,
			fwfc,
			1,
			&FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V4,
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
			&FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V6,
			NULL,
			FW_WEIGHT_HIGHEST_IMPORTANT,
			FWP_ACTION_PERMIT,
			0,
			filter_ids
		);

		for (SIZE_T i = 0; i < RTL_NUMBER_OF (loopback_list); i++)
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

				// win7+
				_wfp_createfilter (
					engine_handle,
					DATA_FILTER_GENERAL,
					FW_NAME_LOOPBACK,
					fwfc,
					2,
					&FWPM_LAYER_ALE_CONNECT_REDIRECT_V4,
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

				_wfp_createfilter (
					engine_handle,
					DATA_FILTER_GENERAL,
					FW_NAME_LOOPBACK,
					fwfc,
					2,
					&FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V4,
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

				// win7+
				_wfp_createfilter (
					engine_handle,
					DATA_FILTER_GENERAL,
					FW_NAME_LOOPBACK,
					fwfc,
					2,
					&FWPM_LAYER_ALE_CONNECT_REDIRECT_V6,
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

				_wfp_createfilter (
					engine_handle,
					DATA_FILTER_GENERAL,
					FW_NAME_LOOPBACK,
					fwfc,
					2,
					&FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V6,
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

		// allows icmpv6 router solicitation messages, which are required for the ipv6 stack to work properly
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

		// allows icmpv6 router advertise messages, which are required for the ipv6 stack to work properly
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

		// allows icmpv6 neighbor solicitation messages, which are required for the ipv6 stack to work properly
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

		// allows icmpv6 neighbor advertise messages, which are required for the ipv6 stack to work properly
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
		fwfc[0].fieldKey = FWPM_CONDITION_FLAGS;
		fwfc[0].matchType = FWP_MATCH_FLAGS_NONE_SET;
		fwfc[0].conditionValue.type = FWP_UINT32;
		fwfc[0].conditionValue.uint32 = FWP_CONDITION_FLAG_IS_LOOPBACK;

		// tests if the network traffic is (non-)app container loopback traffic (win8+)
		if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
			fwfc[0].conditionValue.uint32 |= FWP_CONDITION_FLAG_IS_APPCONTAINER_LOOPBACK;

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

	// install boot-time filters (enforced at boot-time, even before "base filtering engine" service starts)
	if (_r_config_getboolean (L"InstallBoottimeFilters", TRUE) && !config.is_filterstemporary)
	{
		fwfc[0].fieldKey = FWPM_CONDITION_FLAGS;
		fwfc[0].matchType = FWP_MATCH_FLAGS_ALL_SET;
		fwfc[0].conditionValue.type = FWP_UINT32;
		fwfc[0].conditionValue.uint32 = FWP_CONDITION_FLAG_IS_LOOPBACK;

		// tests if the network traffic is (non-)app container loopback traffic (win8+)
		if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
		{
			fwfc[0].matchType = FWP_MATCH_FLAGS_ANY_SET;
			fwfc[0].conditionValue.uint32 |= FWP_CONDITION_FLAG_IS_APPCONTAINER_LOOPBACK;
		}

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
			filter_ids);

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
			filter_ids);

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

	FWP_ACTION_TYPE action;

	action = _r_config_getboolean (L"BlockOutboundConnections", TRUE) ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT;

	// block outbound connection
	if (action == FWP_ACTION_BLOCK)
	{
		// workaround #689
		if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
		{
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

	// block connection redirect
	_wfp_createfilter (
		engine_handle,
		DATA_FILTER_GENERAL,
		FW_NAME_BLOCK_REDIRECT,
		NULL,
		0,
		&FWPM_LAYER_ALE_CONNECT_REDIRECT_V4,
		NULL,
		FW_WEIGHT_LOWEST,
		action,
		0,
		filter_ids
	);

	_wfp_createfilter (
		engine_handle,
		DATA_FILTER_GENERAL,
		FW_NAME_BLOCK_REDIRECT,
		NULL,
		0,
		&FWPM_LAYER_ALE_CONNECT_REDIRECT_V6,
		NULL,
		FW_WEIGHT_LOWEST,
		action,
		0,
		filter_ids
	);

	// block inbound connections
	action = (_r_config_getboolean (L"UseStealthMode", TRUE) || _r_config_getboolean (L"BlockInboundConnections", TRUE)) ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT;

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
		BOOLEAN is_secure;

		_wfp_transact_commit (engine_handle, line);

		is_secure = _r_config_getboolean (L"IsSecureFilters", TRUE);

		if (is_secure)
		{
			for (SIZE_T i = 0; i < _r_obj_getarraysize (filter_ids); i++)
			{
				guid = _r_obj_getarrayitem (filter_ids, i);

				if (guid)
					_app_setsecurityinfoforfilter (engine_handle, guid, is_secure, line);
			}
		}

		_r_queuedlock_releaseshared (&lock_transaction);
	}

	_app_restoreinterfacestate (_r_app_gethwnd (), is_enabled);

	return TRUE;
}

_Ret_maybenull_
PR_ARRAY _wfp_dumpfilters (_In_ HANDLE engine_handle, _In_ LPCGUID provider_id)
{
	PR_ARRAY guids = NULL;
	HANDLE enum_handle;
	ULONG code;
	UINT32 return_count;

	code = FwpmFilterCreateEnumHandle (engine_handle, NULL, &enum_handle);

	if (code == ERROR_SUCCESS)
	{
		FWPM_FILTER **filters_enum;

		code = FwpmFilterEnum (engine_handle, enum_handle, UINT32_MAX, &filters_enum, &return_count);

		if (code == ERROR_SUCCESS)
		{
			if (filters_enum)
			{
				FWPM_FILTER *filter;

				guids = _r_obj_createarray_ex (sizeof (GUID), return_count, NULL);

				for (UINT32 i = 0; i < return_count; i++)
				{
					filter = filters_enum[i];

					if (filter)
					{
						if (filter->providerKey && IsEqualGUID (filter->providerKey, provider_id))
							_r_obj_addarrayitem (guids, &filter->filterKey);
					}
				}

				if (_r_obj_isarrayempty (guids))
					_r_obj_clearreference (&guids);

				FwpmFreeMemory ((PVOID_PTR)&filters_enum);
			}
		}

		FwpmFilterDestroyEnumHandle (engine_handle, enum_handle);
	}

	return guids;
}

VOID NTAPI _wfp_applythread (_In_ PVOID arglist, _In_ ULONG busy_count)
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
			if (_wfp_initialize (engine_handle))
				_wfp_installfilters (engine_handle);
		}
		else
		{
			_wfp_destroyfilters (engine_handle);
			_wfp_uninitialize (engine_handle, TRUE);
		}

		// dropped packets logging (win7+)
		if (config.is_neteventset)
			_wfp_logsubscribe (engine_handle);
	}

	dpi_value = _r_dc_getwindowdpi (context->hwnd);

	_app_restoreinterfacestate (context->hwnd, TRUE);
	_app_setinterfacestate (context->hwnd, dpi_value);

	_r_freelist_deleteitem (&context_free_list, context);

	_app_profile_save ();

	_r_queuedlock_releaseshared (&lock_apply);
}

VOID _wfp_firewallenable (_In_ BOOLEAN is_enable)
{
	static NET_FW_PROFILE_TYPE2 profile_types[] = {
		NET_FW_PROFILE2_DOMAIN,
		NET_FW_PROFILE2_PRIVATE,
		NET_FW_PROFILE2_PUBLIC
	};

	INetFwPolicy2 *INetFwPolicy = NULL;
	HRESULT hr;

	hr = CoCreateInstance (&CLSID_NetFwPolicy2, NULL, CLSCTX_INPROC_SERVER, &IID_INetFwPolicy2, &INetFwPolicy);

	if (FAILED (hr))
		return;

	for (SIZE_T i = 0; i < RTL_NUMBER_OF (profile_types); i++)
	{
		hr = INetFwPolicy2_put_FirewallEnabled (INetFwPolicy, profile_types[i], is_enable ? VARIANT_TRUE : VARIANT_FALSE);

		if (FAILED (hr))
			_r_log_v (LOG_LEVEL_INFO, NULL, L"INetFwPolicy2_put_FirewallEnabled", hr, L"%d", profile_types[i]);
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

	INetFwPolicy2 *INetFwPolicy;
	VARIANT_BOOL status;
	HRESULT hr;

	status = VARIANT_FALSE;

	hr = CoCreateInstance (&CLSID_NetFwPolicy2, NULL, CLSCTX_INPROC_SERVER, &IID_INetFwPolicy2, &INetFwPolicy);

	if (SUCCEEDED (hr))
	{
		for (SIZE_T i = 0; i < RTL_NUMBER_OF (profile_types); i++)
		{
			hr = INetFwPolicy2_get_FirewallEnabled (INetFwPolicy, profile_types[i], &status);

			if (SUCCEEDED (hr))
			{
				if (status == VARIANT_TRUE)
					break;
			}
		}

		INetFwPolicy2_Release (INetFwPolicy);
	}

	if (status == VARIANT_TRUE)
		return TRUE;

	return FALSE;
}

ULONG _FwpmGetAppIdFromFileName1 (_In_ PR_STRING path, _In_ ENUM_TYPE_DATA type, _Out_ PVOID_PTR byte_blob)
{
	PR_STRING original_path;
	ULONG code;

	*byte_blob = NULL;

	if (type == DATA_APP_REGULAR || type == DATA_APP_NETWORK || type == DATA_APP_SERVICE)
	{
		if (_r_str_gethash2 (path, TRUE) == config.ntoskrnl_hash)
		{
			ByteBlobAlloc (path->buffer, path->length + sizeof (UNICODE_NULL), byte_blob);

			return ERROR_SUCCESS;
		}
		else
		{
			original_path = _r_path_ntpathfromdos (path, &code);

			if (!original_path)
			{
				// file is inaccessible or not found, maybe low-level driver preventing file access?
				// try another way!
				if (
					code == ERROR_ACCESS_DENIED ||
					code == ERROR_FILE_NOT_FOUND ||
					code == ERROR_PATH_NOT_FOUND
					)
				{
					if (!_app_isappvalidpath (&path->sr))
					{
						return code;
					}
					else
					{
						PR_STRING path_root;
						R_STRINGREF path_skip_root;

						// file path (without root)
						path_root = _r_obj_createstring2 (path);
						PathStripToRoot (path_root->buffer);

						_r_obj_trimstringtonullterminator (path_root);

						// file path (without root)
						_r_obj_initializestringref (&path_skip_root, PathSkipRoot (path->buffer));

						original_path = _r_path_ntpathfromdos (path_root, &code);

						if (!original_path)
						{
							_r_obj_dereference (path_root);
							return code;
						}

						_r_obj_movereference (&original_path, _r_obj_concatstringrefs (2, &original_path->sr, &path_skip_root));

						_r_str_tolower (&original_path->sr); // lower is important!

						_r_obj_dereference (path_root);
					}
				}
				else
				{
					return code;
				}
			}

			ByteBlobAlloc (original_path->buffer, original_path->length + sizeof (UNICODE_NULL), byte_blob);

			_r_obj_dereference (original_path);

			return ERROR_SUCCESS;
		}
	}
	else if (type == DATA_APP_PICO || type == DATA_APP_DEVICE)
	{
		original_path = _r_obj_createstring2 (path);

		if (type == DATA_APP_DEVICE)
			_r_str_tolower (&original_path->sr); // lower is important!

		ByteBlobAlloc (original_path->buffer, original_path->length + sizeof (UNICODE_NULL), byte_blob);

		_r_obj_dereference (original_path);

		return ERROR_SUCCESS;
	}

	return ERROR_UNIDENTIFIED_ERROR;
}

VOID ByteBlobAlloc (_In_ LPCVOID data, _In_ SIZE_T bytes_count, _Out_ PVOID_PTR byte_blob)
{
	FWP_BYTE_BLOB *blob;

	blob = _r_mem_allocatezero (sizeof (FWP_BYTE_BLOB) + bytes_count);

	blob->size = (UINT)(UINT_PTR)bytes_count;
	blob->data = PTR_ADD_OFFSET (blob, sizeof (FWP_BYTE_BLOB));

	RtlCopyMemory (blob->data, data, bytes_count);

	*byte_blob = blob;
}

VOID ByteBlobFree (_Inout_ PVOID_PTR byte_blob)
{
	FWP_BYTE_BLOB *original_blob;

	original_blob = *byte_blob;
	*byte_blob = NULL;

	if (original_blob)
		_r_mem_free (original_blob);
}
