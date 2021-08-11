// simplewall
// Copyright (c) 2016-2021 Henry++

#include "global.h"

BOOLEAN _wfp_isfiltersapplying ()
{
	return _r_queuedlock_islocked (&lock_apply) || _r_queuedlock_islocked (&lock_transaction);
}

ENUM_INSTALL_TYPE _wfp_isproviderinstalled (_In_ HANDLE hengine)
{
	ENUM_INSTALL_TYPE result;
	FWPM_PROVIDER *ptr_provider;

	result = InstallDisabled;

	if (FwpmProviderGetByKey (hengine, &GUID_WfpProvider, &ptr_provider) == ERROR_SUCCESS)
	{
		if (ptr_provider)
		{
			if ((ptr_provider->flags & FWPM_PROVIDER_FLAG_DISABLED))
			{
				result = InstallDisabled;
			}
			else if ((ptr_provider->flags & FWPM_PROVIDER_FLAG_PERSISTENT))
			{
				result = InstallEnabled;
			}
			else
			{
				result = InstallEnabledTemporary;
			}

			FwpmFreeMemory ((PVOID_PTR)&ptr_provider);
		}
	}

	return result;
}

ENUM_INSTALL_TYPE _wfp_issublayerinstalled (_In_ HANDLE hengine)
{
	ENUM_INSTALL_TYPE result;
	FWPM_SUBLAYER *ptr_sublayer;

	result = InstallDisabled;

	if (FwpmSubLayerGetByKey (hengine, &GUID_WfpSublayer, &ptr_sublayer) == ERROR_SUCCESS)
	{
		if (ptr_sublayer)
		{
			if ((ptr_sublayer->flags & FWPM_SUBLAYER_FLAG_PERSISTENT))
			{
				result = InstallEnabled;
			}
			else
			{
				result = InstallEnabledTemporary;
			}

			FwpmFreeMemory ((PVOID_PTR)&ptr_sublayer);
		}
	}

	return result;
}

ENUM_INSTALL_TYPE _wfp_isfiltersinstalled ()
{
	HANDLE hengine;

	hengine = _wfp_getenginehandle ();

	if (hengine)
		return _wfp_isproviderinstalled (hengine);

	return InstallDisabled;
}

HANDLE _wfp_getenginehandle ()
{
	static HANDLE engine_handle = NULL;
	HANDLE current_handle;

	current_handle = InterlockedCompareExchangePointer (&engine_handle, NULL, NULL);

	if (!current_handle)
	{
		FWPM_SESSION session;
		HANDLE new_handle;
		ULONG code;

		RtlZeroMemory (&session, sizeof (session));

		session.displayData.name = APP_NAME;
		session.displayData.description = APP_NAME;

		session.txnWaitTimeoutInMSec = TRANSACTION_TIMEOUT;

		code = FwpmEngineOpen (NULL, RPC_C_AUTHN_WINNT, NULL, &session, &new_handle);

		if (code != ERROR_SUCCESS)
		{
			_r_log (LOG_LEVEL_CRITICAL, &GUID_TrayIcon, L"FwpmEngineOpen", code, NULL);

			RtlRaiseStatus (code); // WIN32
		}
		else
		{
			current_handle = InterlockedCompareExchangePointer (&engine_handle, new_handle, NULL);

			if (!current_handle)
			{
				current_handle = new_handle;
			}
			else
			{
				FwpmEngineClose (new_handle);
			}
		}
	}

	return current_handle;
}

BOOLEAN _wfp_initialize (_In_ HANDLE hengine, _In_ BOOLEAN is_full)
{
	ULONG code;
	BOOLEAN is_success;
	BOOLEAN is_providerexist;
	BOOLEAN is_sublayerexist;
	BOOLEAN is_intransact;

	_r_queuedlock_acquireshared (&lock_transaction);

	if (hengine)
	{
		is_success = TRUE; // already initialized
	}
	else
	{
		hengine = _wfp_getenginehandle ();

		if (!hengine)
		{
			is_success = FALSE;

			goto CleanupExit;
		}

		is_success = TRUE;
	}

	_app_setsecurityinfoforengine (hengine);

	// install engine provider and it's sublayer
	is_providerexist = (_wfp_isproviderinstalled (hengine) != InstallDisabled);
	is_sublayerexist = (_wfp_issublayerinstalled (hengine) != InstallDisabled);

	if (is_full)
	{
		if (!is_providerexist || !is_sublayerexist)
		{
			is_intransact = _wfp_transact_start (hengine, __LINE__);

			if (!is_providerexist)
			{
				// create provider
				FWPM_PROVIDER provider = {0};

				provider.displayData.name = APP_NAME;
				provider.displayData.description = APP_NAME;

				provider.providerKey = GUID_WfpProvider;

				if (!config.is_filterstemporary)
					provider.flags = FWPM_PROVIDER_FLAG_PERSISTENT;

				code = FwpmProviderAdd (hengine, &provider, NULL);

				if (code != ERROR_SUCCESS && code != FWP_E_ALREADY_EXISTS)
				{
					if (is_intransact)
					{
						FwpmTransactionAbort (hengine);
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
				sublayer.weight = (UINT16)_r_config_getuinteger (L"SublayerWeight", SUBLAYER_WEIGHT_DEFAULT); // highest weight for UINT16

				if (!config.is_filterstemporary)
					sublayer.flags = FWPM_SUBLAYER_FLAG_PERSISTENT;

				code = FwpmSubLayerAdd (hengine, &sublayer, NULL);

				if (code != ERROR_SUCCESS && code != FWP_E_ALREADY_EXISTS)
				{
					if (is_intransact)
					{
						FwpmTransactionAbort (hengine);
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
				if (!_wfp_transact_commit (hengine, __LINE__))
					is_success = FALSE;
			}
		}
	}

	// set security information
	if (is_providerexist || is_sublayerexist)
	{
		BOOLEAN is_secure = _r_config_getboolean (L"IsSecureFilters", TRUE);

		_app_setsecurityinfoforprovider (hengine, &GUID_WfpProvider, is_secure);
		_app_setsecurityinfoforsublayer (hengine, &GUID_WfpSublayer, is_secure);
	}

	// set engine options
	if (is_full)
	{
		BOOLEAN is_win8 = _r_sys_isosversiongreaterorequal (WINDOWS_8);

		FWP_VALUE val;

		// dropped packets logging (win7+)
		if (!config.is_neteventset)
		{
			val.type = FWP_UINT32;
			val.uint32 = 1;

			code = FwpmEngineSetOption (hengine, FWPM_ENGINE_COLLECT_NET_EVENTS, &val);

			if (code != ERROR_SUCCESS)
			{
				_r_log (LOG_LEVEL_WARNING, NULL, L"FwpmEngineSetOption", code, L"FWPM_ENGINE_COLLECT_NET_EVENTS");
			}
			else
			{
				// configure dropped packets logging (win8+)
				if (is_win8)
				{
					// the filter engine will collect wfp network events that match any supplied key words
					val.type = FWP_UINT32;
					val.uint32 = FWPM_NET_EVENT_KEYWORD_CLASSIFY_ALLOW |
						FWPM_NET_EVENT_KEYWORD_INBOUND_MCAST |
						FWPM_NET_EVENT_KEYWORD_INBOUND_BCAST;

					// 1903+
					if (_r_sys_isosversiongreaterorequal (WINDOWS_10_1903))
						val.uint32 |= FWPM_NET_EVENT_KEYWORD_PORT_SCANNING_DROP;

					code = FwpmEngineSetOption (hengine, FWPM_ENGINE_NET_EVENT_MATCH_ANY_KEYWORDS, &val);

					if (code != ERROR_SUCCESS)
						_r_log (LOG_LEVEL_WARNING, NULL, L"FwpmEngineSetOption", code, L"FWPM_ENGINE_NET_EVENT_MATCH_ANY_KEYWORDS");

					// enables the connection monitoring feature and starts logging creation and deletion events (and notifying any subscribers)
					if (_r_config_getboolean (L"IsMonitorIPSecConnections", TRUE))
					{
						val.type = FWP_UINT32;
						val.uint32 = 1;

						code = FwpmEngineSetOption (hengine, FWPM_ENGINE_MONITOR_IPSEC_CONNECTIONS, &val);

						if (code != ERROR_SUCCESS)
							_r_log (LOG_LEVEL_WARNING, NULL, L"FwpmEngineSetOption", code, L"FWPM_ENGINE_MONITOR_IPSEC_CONNECTIONS");
					}
				}

				config.is_neteventset = TRUE;

				_wfp_logsubscribe (hengine);
			}
		}

		// packet queuing (win8+)
		if (is_win8 && _r_config_getboolean (L"IsPacketQueuingEnabled", TRUE))
		{
			// enables inbound or forward packet queuing independently. when enabled, the system is able to evenly distribute cpu load to multiple cpus for site-to-site ipsec tunnel scenarios.
			val.type = FWP_UINT32;
			val.uint32 = FWPM_ENGINE_OPTION_PACKET_QUEUE_INBOUND | FWPM_ENGINE_OPTION_PACKET_QUEUE_FORWARD;

			code = FwpmEngineSetOption (hengine, FWPM_ENGINE_PACKET_QUEUING, &val);

			if (code != ERROR_SUCCESS)
				_r_log (LOG_LEVEL_WARNING, NULL, L"FwpmEngineSetOption", code, L"FWPM_ENGINE_PACKET_QUEUING");
		}
	}

CleanupExit:

	_r_queuedlock_releaseshared (&lock_transaction);

	return is_success;
}

VOID _wfp_uninitialize (_In_ HANDLE hengine, _In_ BOOLEAN is_full)
{
	_r_queuedlock_acquireshared (&lock_transaction);

	ULONG code;

	// dropped packets logging (win7+)
	if (config.is_neteventset)
	{
		_wfp_logunsubscribe (hengine);

		//if (_r_sys_validversion (6, 2))
		//{
		//	// monitor ipsec connection (win8+)
		//	val.type = FWP_UINT32;
		//	val.uint32 = 0;

		//	FwpmEngineSetOption (hengine, FWPM_ENGINE_MONITOR_IPSEC_CONNECTIONS, &val);

		//	// packet queuing (win8+)
		//	val.type = FWP_UINT32;
		//	val.uint32 = FWPM_ENGINE_OPTION_PACKET_QUEUE_NONE;

		//	FwpmEngineSetOption (hengine, FWPM_ENGINE_PACKET_QUEUING, &val);
		//}

		//val.type = FWP_UINT32;
		//val.uint32 = 0;

		//code = FwpmEngineSetOption (hengine, FWPM_ENGINE_COLLECT_NET_EVENTS, &val);
	}

	if (is_full)
	{
		_app_setsecurityinfoforprovider (hengine, &GUID_WfpProvider, FALSE);
		_app_setsecurityinfoforsublayer (hengine, &GUID_WfpSublayer, FALSE);

		BOOLEAN is_intransact = _wfp_transact_start (hengine, __LINE__);

		// destroy callouts (deprecated)
		{
			LPCGUID callouts[] = {
				&GUID_WfpOutboundCallout4_DEPRECATED,
				&GUID_WfpOutboundCallout6_DEPRECATED,
				&GUID_WfpInboundCallout4_DEPRECATED,
				&GUID_WfpInboundCallout6_DEPRECATED,
				&GUID_WfpListenCallout4_DEPRECATED,
				&GUID_WfpListenCallout6_DEPRECATED
			};

			for (SIZE_T i = 0; i < RTL_NUMBER_OF (callouts); i++)
				FwpmCalloutDeleteByKey (hengine, callouts[i]);
		}

		// destroy sublayer
		code = FwpmSubLayerDeleteByKey (hengine, &GUID_WfpSublayer);

		if (code != ERROR_SUCCESS && code != FWP_E_SUBLAYER_NOT_FOUND)
			_r_log (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"FwpmSubLayerDeleteByKey", code, NULL);

		// destroy provider
		code = FwpmProviderDeleteByKey (hengine, &GUID_WfpProvider);

		if (code != ERROR_SUCCESS && code != FWP_E_PROVIDER_NOT_FOUND)
			_r_log (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"FwpmProviderDeleteByKey", code, NULL);

		if (is_intransact)
			_wfp_transact_commit (hengine, __LINE__);
	}

	_r_queuedlock_releaseshared (&lock_transaction);
}

VOID _wfp_installfilters (_In_ HANDLE hengine)
{
	// set security information
	_app_setsecurityinfoforprovider (hengine, &GUID_WfpProvider, FALSE);
	_app_setsecurityinfoforsublayer (hengine, &GUID_WfpSublayer, FALSE);

	_wfp_clearfilter_ids ();

	_r_queuedlock_acquireshared (&lock_transaction);

	// dump all filters into array
	PR_ARRAY guids;
	PR_LIST rules;
	LPCGUID guid;

	BOOLEAN is_intransact;
	BOOLEAN is_secure;

	guids = _wfp_dumpfilters (hengine, &GUID_WfpProvider);

	// restore filters security
	if (guids)
	{
		for (SIZE_T i = 0; i < _r_obj_getarraysize (guids); i++)
		{
			guid = _r_obj_getarrayitem (guids, i);

			if (guid)
				_app_setsecurityinfoforfilter (hengine, guid, FALSE, __LINE__);
		}
	}

	is_intransact = _wfp_transact_start (hengine, __LINE__);

	// destroy all filters
	if (guids)
	{
		for (SIZE_T i = 0; i < _r_obj_getarraysize (guids); i++)
		{
			guid = _r_obj_getarrayitem (guids, i);

			if (guid)
				_wfp_deletefilter (hengine, guid);
		}

		_r_obj_dereference (guids);
	}

	// apply internal rules
	_wfp_create2filters (hengine, __LINE__, is_intransact);

	rules = _r_obj_createlist (&_r_obj_dereference);

	// apply apps rules
	PITEM_APP ptr_app;
	SIZE_T enum_key;

	enum_key = 0;

	_r_queuedlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtablepointer (apps_table, &ptr_app, NULL, &enum_key))
	{
		if (ptr_app->is_enabled)
		{
			_r_obj_addlistitem (rules, _r_obj_reference (ptr_app));
		}
	}

	_r_queuedlock_releaseshared (&lock_apps);

	if (!_r_obj_islistempty (rules))
	{
		_wfp_create3filters (hengine, rules, __LINE__, is_intransact);

		_r_obj_clearlist (rules);
	}

	// apply blocklist/system/user rules
	PITEM_RULE ptr_rule;

	_r_queuedlock_acquireshared (&lock_rules);

	for (SIZE_T i = 0; i < _r_obj_getlistsize (rules_list); i++)
	{
		ptr_rule = _r_obj_getlistitem (rules_list, i);

		if (ptr_rule && ptr_rule->is_enabled)
			_r_obj_addlistitem (rules, _r_obj_reference (ptr_rule));
	}

	_r_queuedlock_releaseshared (&lock_rules);

	if (!_r_obj_islistempty (rules))
	{
		_wfp_create4filters (hengine, rules, __LINE__, is_intransact);

		//_r_obj_clearlist (rules);
	}

	if (is_intransact)
		_wfp_transact_commit (hengine, __LINE__);

	// secure filters
	is_secure = _r_config_getboolean (L"IsSecureFilters", TRUE);

	if (is_secure)
	{
		guids = _wfp_dumpfilters (hengine, &GUID_WfpProvider);

		if (guids)
		{
			for (SIZE_T i = 0; i < _r_obj_getarraysize (guids); i++)
			{
				guid = _r_obj_getarrayitem (guids, i);

				if (guid)
					_app_setsecurityinfoforfilter (hengine, guid, is_secure, __LINE__);
			}

			_r_obj_dereference (guids);
		}
	}

	_app_setsecurityinfoforprovider (hengine, &GUID_WfpProvider, is_secure);
	_app_setsecurityinfoforsublayer (hengine, &GUID_WfpSublayer, is_secure);

	_r_obj_dereference (rules);

	_r_queuedlock_releaseshared (&lock_transaction);
}

BOOLEAN _wfp_transact_start (_In_ HANDLE hengine, _In_ UINT line)
{
	ULONG code = FwpmTransactionBegin (hengine, 0);

	//if (code == FWP_E_TXN_IN_PROGRESS)
	//	return FALSE;

	if (code != ERROR_SUCCESS)
	{
		_r_log_v (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"FwpmTransactionBegin", code, L"#%" PRIu32, line);
		return FALSE;
	}

	return TRUE;
}

BOOLEAN _wfp_transact_commit (_In_ HANDLE hengine, _In_ UINT line)
{
	ULONG code = FwpmTransactionCommit (hengine);

	if (code != ERROR_SUCCESS)
	{
		FwpmTransactionAbort (hengine);

		_r_log_v (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"FwpmTransactionCommit", code, L"#%" PRIu32, line);
		return FALSE;

	}

	return TRUE;
}

BOOLEAN _wfp_deletefilter (_In_ HANDLE hengine, _In_ LPCGUID filter_id)
{
	ULONG code = FwpmFilterDeleteByKey (hengine, filter_id);

#if !defined(_DEBUG)
	if (code != ERROR_SUCCESS && code != FWP_E_FILTER_NOT_FOUND)
#else
	if (code != ERROR_SUCCESS)
#endif // !DEBUG
	{
		PR_STRING guid_string = _r_str_fromguid (filter_id);

		_r_log (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"FwpmFilterDeleteByKey", code, _r_obj_getstringordefault (guid_string, SZ_EMPTY));

		if (guid_string)
			_r_obj_dereference (guid_string);

		return FALSE;
	}

	return TRUE;
}

FORCEINLINE LPCWSTR _wfp_filtertypetostring (_In_ ENUM_TYPE_DATA filter_type)
{
	switch (filter_type)
	{
		case DataAppRegular:
		case DataAppDevice:
		case DataAppNetwork:
		case DataAppPico:
		case DataAppService:
		case DataAppUWP:
		{
			return L"App";
		}

		case DataRuleBlocklist:
		{
			return L"Blocklist";
		}

		case DataRuleSystem:
		{
			return L"System rule";
		}

		case DataRuleSystemUser:
		case DataRuleUser:
		{
			return L"User rule";
		}

		case DataFilterGeneral:
		{
			return L"Internal";
		}
	}

	return NULL;
}

ULONG _wfp_createfilter (_In_ HANDLE hengine, _In_ ENUM_TYPE_DATA type, _In_opt_ LPCWSTR filter_name, _In_reads_ (count) FWPM_FILTER_CONDITION *lpcond, _In_ UINT32 count, _In_opt_ LPCGUID layer_id, _In_opt_ LPCGUID callout_id, _In_ UINT8 weight, _In_ FWP_ACTION_TYPE action, _In_ UINT32 flags, _Inout_opt_ PR_ARRAY guids)
{
	FWPM_FILTER filter = {0};
	WCHAR filter_description[128];
	LPCWSTR filter_type_string;
	UINT64 filter_id;
	ULONG code;

	// create filter guid
	code = _r_math_createguid (&filter.filterKey);

	if (code != ERROR_SUCCESS)
	{
		return code;
	}

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

	if (layer_id)
		RtlCopyMemory (&filter.layerKey, layer_id, sizeof (GUID));

	if (callout_id)
		RtlCopyMemory (&filter.action.calloutKey, callout_id, sizeof (GUID));

	code = FwpmFilterAdd (hengine, &filter, NULL, &filter_id);

	if (code == ERROR_SUCCESS)
	{
		if (guids)
			_r_obj_addarrayitem (guids, &filter.filterKey);
	}
	else
	{
		_r_log (LOG_LEVEL_ERROR, &GUID_TrayIcon, L"FwpmFilterAdd", code, filter.displayData.description);
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

VOID _wfp_destroyfilters (_In_ HANDLE hengine)
{
	_wfp_clearfilter_ids ();

	// destroy all filters
	PR_ARRAY guids;

	_r_queuedlock_acquireshared (&lock_transaction);

	guids = _wfp_dumpfilters (hengine, &GUID_WfpProvider);

	if (guids)
	{
		_wfp_destroyfilters_array (hengine, guids, __LINE__);

		_r_obj_dereference (guids);
	}

	_r_queuedlock_releaseshared (&lock_transaction);
}

BOOLEAN _wfp_destroyfilters_array (_In_ HANDLE hengine, _In_ PR_ARRAY guids, _In_ UINT line)
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
			_app_setsecurityinfoforfilter (hengine, guid, FALSE, line);
	}

	is_intransact = _wfp_transact_start (hengine, line);

	for (SIZE_T i = 0; i < _r_obj_getarraysize (guids); i++)
	{
		guid = _r_obj_getarrayitem (guids, i);

		if (guid)
			_wfp_deletefilter (hengine, guid);
	}

	if (is_intransact)
		_wfp_transact_commit (hengine, line);

	_r_queuedlock_releaseshared (&lock_transaction);

	_app_restoreinterfacestate (_r_app_gethwnd (), is_enabled);

	return TRUE;
}

BOOLEAN _wfp_createrulefilter (_In_ HANDLE hengine, _In_ ENUM_TYPE_DATA filter_type, _In_opt_ LPCWSTR filter_name, _In_opt_ ULONG_PTR app_hash, _In_opt_ PITEM_FILTER_CONFIG filter_config, _In_opt_ PR_STRINGREF rule_remote, _In_opt_ PR_STRINGREF rule_local, _In_ UINT8 weight, _In_ FWP_ACTION_TYPE action, _In_ UINT32 flags, _Inout_opt_ PR_ARRAY guids)
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

		if (ptr_app->type == DataAppService) // windows service
		{
			if (ptr_app->pbytes)
			{
				ByteBlobAlloc (ptr_app->pbytes->buffer, RtlLengthSecurityDescriptor (ptr_app->pbytes->buffer), &byte_blob);

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
		else if (ptr_app->type == DataAppUWP) // windows store app (win8+)
		{
			if (ptr_app->pbytes)
			{
				fwfc[count].fieldKey = FWPM_CONDITION_ALE_PACKAGE_ID;
				fwfc[count].matchType = FWP_MATCH_EQUAL;
				fwfc[count].conditionValue.type = FWP_SID;
				fwfc[count].conditionValue.sid = (SID *)ptr_app->pbytes->buffer;

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

			RtlZeroMemory (&address, sizeof (address));

			if (!_app_parserulestring (rules[i], &address))
				goto CleanupExit;

			if (i == 0)
			{
				if (address.type == DataTypePort)
				{
					is_remoteport_set = TRUE;
				}
				else if (address.type == DataTypeIp || address.type == DataTypeHost)
				{
					is_remoteaddr_set = TRUE;
				}
			}

			if (address.is_range)
			{
				if (address.type == DataTypeIp)
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

				fwfc[count].fieldKey = (address.type == DataTypePort) ? ((i == 0) ? FWPM_CONDITION_IP_REMOTE_PORT : FWPM_CONDITION_IP_LOCAL_PORT) : ((i == 0) ? FWPM_CONDITION_IP_REMOTE_ADDRESS : FWPM_CONDITION_IP_LOCAL_ADDRESS);
				fwfc[count].matchType = FWP_MATCH_RANGE;
				fwfc[count].conditionValue.type = FWP_RANGE_TYPE;
				fwfc[count].conditionValue.rangeValue = &address.range;

				count += 1;
			}
			else
			{
				if (address.type == DataTypePort)
				{
					fwfc[count].fieldKey = ((i == 0) ? FWPM_CONDITION_IP_REMOTE_PORT : FWPM_CONDITION_IP_LOCAL_PORT);
					fwfc[count].matchType = FWP_MATCH_EQUAL;
					fwfc[count].conditionValue.type = FWP_UINT16;
					fwfc[count].conditionValue.uint16 = address.port;

					count += 1;
				}
				else if (address.type == DataTypeIp || address.type == DataTypeHost)
				{
					fwfc[count].fieldKey = ((i == 0) ? FWPM_CONDITION_IP_REMOTE_ADDRESS : FWPM_CONDITION_IP_LOCAL_ADDRESS);
					fwfc[count].matchType = FWP_MATCH_EQUAL;

					if (address.format == NET_ADDRESS_IPV4)
					{
						af = AF_INET;

						fwfc[count].conditionValue.type = FWP_V4_ADDR_MASK;
						fwfc[count].conditionValue.v4AddrMask = &address.addr4;

						count += 1;
					}
					else if (address.format == NET_ADDRESS_IPV6)
					{
						af = AF_INET6;

						fwfc[count].conditionValue.type = FWP_V6_ADDR_MASK;
						fwfc[count].conditionValue.v6AddrMask = &address.addr6;

						count += 1;
					}
					else if (address.format == NET_ADDRESS_DNS_NAME)
					{
						R_STRINGREF host_part;
						R_STRINGREF remaining_part;

						_r_obj_initializestringref (&remaining_part, address.host);

						while (remaining_part.length != 0)
						{
							_r_str_splitatchar (&remaining_part, DIVIDER_RULE[0], &host_part, &remaining_part);

							if (!_wfp_createrulefilter (hengine, filter_type, filter_name, app_hash, filter_config, &host_part, NULL, weight, action, flags, guids))
							{
								goto CleanupExit;
							}
						}

						is_success = TRUE;

						goto CleanupExit;
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
			if (address.type == DataTypeIp || address.type == DataTypeHost)
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
			_wfp_createfilter (hengine, filter_type, filter_name, fwfc, count, &FWPM_LAYER_ALE_AUTH_CONNECT_V4, NULL, weight, action, flags, guids);

			// win7+
			_wfp_createfilter (hengine, filter_type, filter_name, fwfc, count, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V4, NULL, weight, action, flags, guids);
		}

		if (af == AF_INET6 || af == AF_UNSPEC)
		{
			_wfp_createfilter (hengine, filter_type, filter_name, fwfc, count, &FWPM_LAYER_ALE_AUTH_CONNECT_V6, NULL, weight, action, flags, guids);

			// win7+
			_wfp_createfilter (hengine, filter_type, filter_name, fwfc, count, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V6, NULL, weight, action, flags, guids);
		}
	}

	// create inbound layer filter
	if (direction == FWP_DIRECTION_INBOUND || direction == FWP_DIRECTION_MAX)
	{
		if (af == AF_INET || af == AF_UNSPEC)
		{
			_wfp_createfilter (hengine, filter_type, filter_name, fwfc, count, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, NULL, weight, action, flags, guids);

			if (action == FWP_ACTION_BLOCK && (!is_remoteaddr_set && !is_remoteport_set))
				_wfp_createfilter (hengine, filter_type, filter_name, fwfc, count, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V4, NULL, weight, FWP_ACTION_PERMIT, flags, guids);
		}

		if (af == AF_INET6 || af == AF_UNSPEC)
		{
			_wfp_createfilter (hengine, filter_type, filter_name, fwfc, count, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, weight, action, flags, guids);

			if (action == FWP_ACTION_BLOCK && (!is_remoteaddr_set && !is_remoteport_set))
				_wfp_createfilter (hengine, filter_type, filter_name, fwfc, count, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V6, NULL, weight, FWP_ACTION_PERMIT, flags, guids);
		}
	}

	is_success = TRUE;

CleanupExit:

	if (ptr_app)
		_r_obj_dereference (ptr_app);

	ByteBlobFree (&byte_blob);

	return is_success;
}

BOOLEAN _wfp_create4filters (_In_ HANDLE hengine, _In_  PR_LIST rules, _In_ UINT line, _In_ BOOLEAN is_intransact)
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
				_app_setsecurityinfoforfilter (hengine, guid, FALSE, line);
		}

		_r_queuedlock_acquireshared (&lock_transaction);
		is_intransact = !_wfp_transact_start (hengine, line);
	}

	for (SIZE_T i = 0; i < _r_obj_getarraysize (guids); i++)
	{
		guid = _r_obj_getarrayitem (guids, i);

		if (guid)
			_wfp_deletefilter (hengine, guid);
	}

	R_STRINGREF remote_remaining_part;
	R_STRINGREF local_remaining_part;
	R_STRINGREF rule_remote_part;
	R_STRINGREF rule_local_part;

	for (SIZE_T i = 0; i < _r_obj_getlistsize (rules); i++)
	{
		ptr_rule = _r_obj_getlistitem (rules, i);

		if (!ptr_rule)
			continue;

		ptr_rule->is_haveerrors = FALSE;
		_r_obj_cleararray (ptr_rule->guids);

		if (!ptr_rule->is_enabled)
			continue;

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
			LPCWSTR rule_name;

			rule_name = _r_obj_getstring (ptr_rule->name);

			// apply rules for services hosts
			if (ptr_rule->is_forservices)
			{
				if (!_wfp_createrulefilter (hengine, ptr_rule->type, rule_name, config.ntoskrnl_hash, &ptr_rule->config, &rule_remote_part, &rule_local_part, ptr_rule->weight, ptr_rule->action, 0, ptr_rule->guids))
				{
					ptr_rule->is_haveerrors = TRUE;
				}

				if (!_wfp_createrulefilter (hengine, ptr_rule->type, rule_name, config.svchost_hash, &ptr_rule->config, &rule_remote_part, &rule_local_part, ptr_rule->weight, ptr_rule->action, 0, ptr_rule->guids))
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
					if (ptr_rule->is_forservices && (hash_code == config.ntoskrnl_hash || hash_code == config.svchost_hash))
						continue;

					if (!_wfp_createrulefilter (hengine, ptr_rule->type, rule_name, hash_code, &ptr_rule->config, &rule_remote_part, &rule_local_part, ptr_rule->weight, ptr_rule->action, 0, ptr_rule->guids))
					{
						ptr_rule->is_haveerrors = TRUE;
					}
				}
			}
			else
			{
				if (!_wfp_createrulefilter (hengine, ptr_rule->type, rule_name, 0, &ptr_rule->config, &rule_remote_part, &rule_local_part, ptr_rule->weight, ptr_rule->action, 0, ptr_rule->guids))
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

		_wfp_transact_commit (hengine, line);

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
							_app_setsecurityinfoforfilter (hengine, guid, is_secure, line);
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

BOOLEAN _wfp_create3filters (_In_ HANDLE hengine, _In_ PR_LIST rules, _In_ UINT line, _In_ BOOLEAN is_intransact)
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
				_app_setsecurityinfoforfilter (hengine, guid, FALSE, line);
		}

		_r_queuedlock_acquireshared (&lock_transaction);
		is_intransact = !_wfp_transact_start (hengine, line);
	}

	for (SIZE_T i = 0; i < _r_obj_getarraysize (guids); i++)
	{
		guid = _r_obj_getarrayitem (guids, i);

		if (guid)
			_wfp_deletefilter (hengine, guid);
	}

	for (SIZE_T i = 0; i < _r_obj_getlistsize (rules); i++)
	{
		ptr_app = _r_obj_getlistitem (rules, i);

		if (ptr_app && ptr_app->is_enabled)
		{
			if (!_wfp_createrulefilter (hengine, ptr_app->type, _app_getappdisplayname (ptr_app, TRUE), ptr_app->app_hash, NULL, NULL, NULL, FW_WEIGHT_APP, FWP_ACTION_PERMIT, 0, ptr_app->guids))
			{
				ptr_app->is_haveerrors = TRUE;
			}
		}
	}

	if (!is_intransact)
	{
		PITEM_APP ptr_app;
		BOOLEAN is_secure;

		_wfp_transact_commit (hengine, line);

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
							_app_setsecurityinfoforfilter (hengine, guid, is_secure, line);
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

BOOLEAN _wfp_create2filters (_In_ HANDLE hengine, _In_ UINT line, _In_ BOOLEAN is_intransact)
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
					_app_setsecurityinfoforfilter (hengine, guid, FALSE, line);
			}
		}

		_r_queuedlock_acquireshared (&lock_transaction);
		is_intransact = !_wfp_transact_start (hengine, line);
	}

	if (!_r_obj_isarrayempty (filter_ids))
	{
		for (SIZE_T i = 0; i < _r_obj_getarraysize (filter_ids); i++)
		{
			guid = _r_obj_getarrayitem (filter_ids, i);

			if (guid)
				_wfp_deletefilter (hengine, guid);
		}

		_r_obj_cleararray (filter_ids);
	}

	FWPM_FILTER_CONDITION fwfc[3] = {0};

	// add loopback connections permission
	if (_r_config_getboolean (L"AllowLoopbackConnections", TRUE))
	{
		ITEM_ADDRESS address;

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

		_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 1, &FWPM_LAYER_ALE_AUTH_CONNECT_V4, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, 0, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 1, &FWPM_LAYER_ALE_AUTH_CONNECT_V6, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, 0, filter_ids);

		// win7+
		_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 1, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V4, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, 0, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 1, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V6, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, 0, filter_ids);

		_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 1, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, 0, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 1, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, 0, filter_ids);

		_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 1, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V4, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, 0, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 1, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V6, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, 0, filter_ids);

		for (SIZE_T i = 0; i < RTL_NUMBER_OF (loopback_list); i++)
		{
			RtlZeroMemory (&address, sizeof (address));

			if (!_app_parserulestring (&loopback_list[i], &address))
			{
				continue;
			}

			fwfc[1].matchType = FWP_MATCH_EQUAL;

			if (address.format == NET_ADDRESS_IPV4)
			{
				fwfc[1].conditionValue.type = FWP_V4_ADDR_MASK;
				fwfc[1].conditionValue.v4AddrMask = &address.addr4;

				fwfc[1].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
				_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 2, &FWPM_LAYER_ALE_AUTH_CONNECT_V4, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, 0, filter_ids);

				// win7+
				_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 2, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V4, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, 0, filter_ids);

				fwfc[1].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;
				_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 2, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, 0, filter_ids);
				_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 2, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V4, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, 0, filter_ids);
			}
			else if (address.format == NET_ADDRESS_IPV6)
			{
				fwfc[1].conditionValue.type = FWP_V6_ADDR_MASK;
				fwfc[1].conditionValue.v6AddrMask = &address.addr6;

				fwfc[1].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
				_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 2, &FWPM_LAYER_ALE_AUTH_CONNECT_V6, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, 0, filter_ids);

				// win7+
				_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 2, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V6, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, 0, filter_ids);

				fwfc[1].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;
				_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 2, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, 0, filter_ids);
				_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 2, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V6, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, 0, filter_ids);
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
		_wfp_createfilter (hengine, DataFilterGeneral, L"Allow6to4", fwfc, 1, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, 0, filter_ids);

		// allows icmpv6 router solicitation messages, which are required for the ipv6 stack to work properly
		fwfc[0].fieldKey = FWPM_CONDITION_ICMP_TYPE;
		fwfc[0].matchType = FWP_MATCH_EQUAL;
		fwfc[0].conditionValue.type = FWP_UINT16;

		fwfc[0].conditionValue.uint16 = 0x85;
		_wfp_createfilter (hengine, DataFilterGeneral, L"AllowIcmpV6Type133", fwfc, 1, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, 0, filter_ids);

		// allows icmpv6 router advertise messages, which are required for the ipv6 stack to work properly
		fwfc[0].conditionValue.uint16 = 0x86;
		_wfp_createfilter (hengine, DataFilterGeneral, L"AllowIcmpV6Type134", fwfc, 1, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, 0, filter_ids);

		// allows icmpv6 neighbor solicitation messages, which are required for the ipv6 stack to work properly
		fwfc[0].conditionValue.uint16 = 0x87;
		_wfp_createfilter (hengine, DataFilterGeneral, L"AllowIcmpV6Type135", fwfc, 1, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, 0, filter_ids);

		// allows icmpv6 neighbor advertise messages, which are required for the ipv6 stack to work properly
		fwfc[0].conditionValue.uint16 = 0x88;
		_wfp_createfilter (hengine, DataFilterGeneral, L"AllowIcmpV6Type136", fwfc, 1, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, 0, filter_ids);
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

		_wfp_createfilter (hengine, DataFilterGeneral, FW_NAME_ICMP_ERROR, fwfc, 2, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V4, NULL, FW_WEIGHT_HIGHEST, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, FW_NAME_ICMP_ERROR, fwfc, 2, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V6, NULL, FW_WEIGHT_HIGHEST, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, filter_ids);

		// blocks tcp port scanners (exclude loopback)
		fwfc[0].conditionValue.uint32 |= FWP_CONDITION_FLAG_IS_IPSEC_SECURED;

		_wfp_createfilter (hengine, DataFilterGeneral, FW_NAME_TCP_RST_ONCLOSE, fwfc, 1, &FWPM_LAYER_INBOUND_TRANSPORT_V4_DISCARD, &FWPM_CALLOUT_WFP_TRANSPORT_LAYER_V4_SILENT_DROP, FW_WEIGHT_HIGHEST, FWP_ACTION_CALLOUT_TERMINATING, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, FW_NAME_TCP_RST_ONCLOSE, fwfc, 1, &FWPM_LAYER_INBOUND_TRANSPORT_V6_DISCARD, &FWPM_CALLOUT_WFP_TRANSPORT_LAYER_V6_SILENT_DROP, FW_WEIGHT_HIGHEST, FWP_ACTION_CALLOUT_TERMINATING, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, filter_ids);
	}

	// configure outbound layer
	{
		FWP_ACTION_TYPE action = _r_config_getboolean (L"BlockOutboundConnections", TRUE) ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT;

		_wfp_createfilter (hengine, DataFilterGeneral, FW_NAME_BLOCK_CONNECTION, NULL, 0, &FWPM_LAYER_ALE_AUTH_CONNECT_V4, NULL, FW_WEIGHT_LOWEST, action, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, FW_NAME_BLOCK_CONNECTION, NULL, 0, &FWPM_LAYER_ALE_AUTH_CONNECT_V6, NULL, FW_WEIGHT_LOWEST, action, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, filter_ids);

		// win7+
		_wfp_createfilter (hengine, DataFilterGeneral, FW_NAME_BLOCK_REDIRECT, NULL, 0, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V4, NULL, FW_WEIGHT_LOWEST, action, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, FW_NAME_BLOCK_REDIRECT, NULL, 0, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V6, NULL, FW_WEIGHT_LOWEST, action, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, filter_ids);
	}

	// configure inbound layer
	{
		FWP_ACTION_TYPE action = (_r_config_getboolean (L"UseStealthMode", TRUE) || _r_config_getboolean (L"BlockInboundConnections", TRUE)) ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT;

		_wfp_createfilter (hengine, DataFilterGeneral, FW_NAME_BLOCK_RECVACCEPT, NULL, 0, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, NULL, FW_WEIGHT_LOWEST, action, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, FW_NAME_BLOCK_RECVACCEPT, NULL, 0, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, FW_WEIGHT_LOWEST, action, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, filter_ids);
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

		_wfp_createfilter (hengine, DataFilterGeneral, FW_NAME_BOOTTIME, fwfc, 1, &FWPM_LAYER_IPFORWARD_V4, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, FW_NAME_BOOTTIME, fwfc, 1, &FWPM_LAYER_IPFORWARD_V6, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);

		_wfp_createfilter (hengine, DataFilterGeneral, FW_NAME_BOOTTIME, fwfc, 1, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, FW_NAME_BOOTTIME, fwfc, 1, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);

		_wfp_createfilter (hengine, DataFilterGeneral, FW_NAME_BOOTTIME, fwfc, 1, &FWPM_LAYER_INBOUND_ICMP_ERROR_V4, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, FW_NAME_BOOTTIME, fwfc, 1, &FWPM_LAYER_INBOUND_ICMP_ERROR_V6, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);

		_wfp_createfilter (hengine, DataFilterGeneral, FW_NAME_BOOTTIME, fwfc, 1, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V4, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, FW_NAME_BOOTTIME, fwfc, 1, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V6, NULL, FW_WEIGHT_HIGHEST_IMPORTANT, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);

		_wfp_createfilter (hengine, DataFilterGeneral, FW_NAME_BOOTTIME, NULL, 0, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, NULL, FW_WEIGHT_LOWEST, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, FW_NAME_BOOTTIME, NULL, 0, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, FW_WEIGHT_LOWEST, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);

		_wfp_createfilter (hengine, DataFilterGeneral, FW_NAME_BOOTTIME, NULL, 0, &FWPM_LAYER_INBOUND_ICMP_ERROR_V4, NULL, FW_WEIGHT_LOWEST, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, FW_NAME_BOOTTIME, NULL, 0, &FWPM_LAYER_INBOUND_ICMP_ERROR_V6, NULL, FW_WEIGHT_LOWEST, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);

		_wfp_createfilter (hengine, DataFilterGeneral, FW_NAME_BOOTTIME, NULL, 0, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V4, NULL, FW_WEIGHT_LOWEST, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, FW_NAME_BOOTTIME, NULL, 0, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V6, NULL, FW_WEIGHT_LOWEST, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);

		// win7+ boot-time features
		fwfc[0].fieldKey = FWPM_CONDITION_FLAGS;
		fwfc[0].matchType = FWP_MATCH_FLAGS_NONE_SET;
		fwfc[0].conditionValue.type = FWP_UINT32;
		fwfc[0].conditionValue.uint32 = FWP_CONDITION_FLAG_IS_OUTBOUND_PASS_THRU | FWP_CONDITION_FLAG_IS_INBOUND_PASS_THRU;

		_wfp_createfilter (hengine, DataFilterGeneral, FW_NAME_BOOTTIME, fwfc, 1, &FWPM_LAYER_IPFORWARD_V4, NULL, FW_WEIGHT_APP, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, FW_NAME_BOOTTIME, fwfc, 1, &FWPM_LAYER_IPFORWARD_V6, NULL, FW_WEIGHT_APP, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);
	}

	if (!is_intransact)
	{
		BOOLEAN is_secure;

		_wfp_transact_commit (hengine, line);

		is_secure = _r_config_getboolean (L"IsSecureFilters", TRUE);

		if (is_secure)
		{
			for (SIZE_T i = 0; i < _r_obj_getarraysize (filter_ids); i++)
			{
				guid = _r_obj_getarrayitem (filter_ids, i);

				if (guid)
					_app_setsecurityinfoforfilter (hengine, guid, is_secure, line);
			}
		}

		_r_queuedlock_releaseshared (&lock_transaction);
	}

	_app_restoreinterfacestate (_r_app_gethwnd (), is_enabled);

	return TRUE;
}

_Ret_maybenull_
PR_ARRAY _wfp_dumpfilters (_In_ HANDLE hengine, _In_ LPCGUID provider_id)
{
	PR_ARRAY guids = NULL;
	HANDLE enum_handle;
	ULONG code;
	UINT32 return_count;

	code = FwpmFilterCreateEnumHandle (hengine, NULL, &enum_handle);

	if (code == ERROR_SUCCESS)
	{
		FWPM_FILTER **filters_enum;

		code = FwpmFilterEnum (hengine, enum_handle, UINT32_MAX, &filters_enum, &return_count);

		if (code == ERROR_SUCCESS)
		{
			if (filters_enum)
			{
				FWPM_FILTER *filter;

				guids = _r_obj_createarrayex (sizeof (GUID), return_count, NULL);

				for (UINT32 i = 0; i < return_count; i++)
				{
					filter = filters_enum[i];

					if (filter && filter->providerKey && IsEqualGUID (filter->providerKey, provider_id))
					{
						_r_obj_addarrayitem (guids, &filter->filterKey);
					}
				}

				if (_r_obj_isarrayempty (guids))
					_r_obj_clearreference (&guids);

				FwpmFreeMemory ((PVOID_PTR)&filters_enum);
			}
		}

		FwpmFilterDestroyEnumHandle (hengine, enum_handle);
	}

	return guids;
}

ULONG _FwpmGetAppIdFromFileName1 (_In_ PR_STRING path, _In_ ENUM_TYPE_DATA type, _Out_ PVOID_PTR byte_blob)
{
	PR_STRING original_path;
	ULONG code;

	*byte_blob = NULL;

	if (type == DataAppRegular || type == DataAppNetwork || type == DataAppService)
	{
		if (_r_obj_getstringhash (path) == config.ntoskrnl_hash)
		{
			ByteBlobAlloc (path->buffer, path->length + sizeof (UNICODE_NULL), byte_blob);

			return ERROR_SUCCESS;
		}
		else
		{
			original_path = _r_path_ntpathfromdos (path->buffer, &code);

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
						WCHAR path_root[512];
						R_STRINGREF path_skip_root;

						// file path (without root)
						_r_str_copystring (path_root, RTL_NUMBER_OF (path_root), &path->sr);
						PathStripToRoot (path_root);

						// file path (without root)
						_r_obj_initializestringref (&path_skip_root, PathSkipRoot (path->buffer));
						_r_str_tolower (&path_skip_root); // lower is important!

						original_path = _r_path_ntpathfromdos (path_root, &code);

						if (!original_path)
							return code;

						_r_obj_movereference (&original_path, _r_obj_concatstringrefs (2, &original_path->sr, &path_skip_root));
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
	else if (type == DataAppPico || type == DataAppDevice)
	{
		original_path = _r_obj_createstring2 (path);

		if (type == DataAppDevice)
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

	blob->size = (UINT32)bytes_count;
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
