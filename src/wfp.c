// simplewall
// Copyright (c) 2016-2021 Henry++

#include "global.h"

BOOLEAN _wfp_isfiltersapplying ()
{
	return _r_spinlock_islocked (&lock_apply) || _r_spinlock_islocked (&lock_transaction);
}

ENUM_INSTALL_TYPE _wfp_isproviderinstalled (HANDLE hengine)
{
	ENUM_INSTALL_TYPE result = InstallDisabled;
	FWPM_PROVIDER *ptr_provider = NULL;

	if (FwpmProviderGetByKey (hengine, &GUID_WfpProvider, &ptr_provider) == ERROR_SUCCESS)
	{
		if (ptr_provider)
		{
			if ((ptr_provider->flags & FWPM_PROVIDER_FLAG_DISABLED) != 0)
			{
				result = InstallDisabled;
			}
			else if ((ptr_provider->flags & FWPM_PROVIDER_FLAG_PERSISTENT) != 0)
			{
				result = InstallEnabled;
			}
			else
			{
				result = InstallEnabledTemporary;
			}

			FwpmFreeMemory ((PVOID*)&ptr_provider);
		}
	}

	return result;
}

ENUM_INSTALL_TYPE _wfp_issublayerinstalled (HANDLE hengine)
{
	ENUM_INSTALL_TYPE result = InstallDisabled;
	FWPM_SUBLAYER *ptr_sublayer = NULL;

	if (FwpmSubLayerGetByKey (hengine, &GUID_WfpSublayer, &ptr_sublayer) == ERROR_SUCCESS)
	{
		if (ptr_sublayer)
		{
			if ((ptr_sublayer->flags & FWPM_SUBLAYER_FLAG_PERSISTENT) != 0)
			{
				result = InstallEnabled;
			}
			else
			{
				result = InstallEnabledTemporary;
			}

			FwpmFreeMemory ((PVOID*)&ptr_sublayer);
		}
	}

	return result;
}

ENUM_INSTALL_TYPE _wfp_isfiltersinstalled ()
{
	HANDLE hengine = _wfp_getenginehandle ();

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
		memset (&session, 0, sizeof (session));

		session.displayData.name = APP_NAME;
		session.displayData.description = APP_NAME;

		session.txnWaitTimeoutInMSec = TRANSACTION_TIMEOUT;

		HANDLE new_handle;
		ULONG code = FwpmEngineOpen (NULL, RPC_C_AUTHN_WINNT, NULL, &session, &new_handle);

		if (code != ERROR_SUCCESS)
		{
			_r_log (Critical, UID, L"FwpmEngineOpen", code, NULL);
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

BOOLEAN _wfp_initialize (HANDLE hengine, BOOLEAN is_full)
{
	ULONG code;
	BOOLEAN is_success;
	BOOLEAN is_providerexist;
	BOOLEAN is_sublayerexist;
	BOOLEAN is_intransact;

	_r_spinlock_acquireshared (&lock_transaction);

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

					_r_log (Error, UID, L"FwpmProviderAdd", code, NULL);
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

					_r_log (Error, UID, L"FwpmSubLayerAdd", code, NULL);
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
				_r_log (Warning, 0, L"FwpmEngineSetOption", code, L"FWPM_ENGINE_COLLECT_NET_EVENTS");
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
					if (_r_sys_isosversiongreaterorequal (WINDOWS_10_19H1))
						val.uint32 |= FWPM_NET_EVENT_KEYWORD_PORT_SCANNING_DROP;

					code = FwpmEngineSetOption (hengine, FWPM_ENGINE_NET_EVENT_MATCH_ANY_KEYWORDS, &val);

					if (code != ERROR_SUCCESS)
						_r_log (Warning, 0, L"FwpmEngineSetOption", code, L"FWPM_ENGINE_NET_EVENT_MATCH_ANY_KEYWORDS");

					// enables the connection monitoring feature and starts logging creation and deletion events (and notifying any subscribers)
					if (_r_config_getboolean (L"IsMonitorIPSecConnections", TRUE))
					{
						val.type = FWP_UINT32;
						val.uint32 = 1;

						code = FwpmEngineSetOption (hengine, FWPM_ENGINE_MONITOR_IPSEC_CONNECTIONS, &val);

						if (code != ERROR_SUCCESS)
							_r_log (Warning, 0, L"FwpmEngineSetOption", code, L"FWPM_ENGINE_MONITOR_IPSEC_CONNECTIONS");
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
				_r_log (Warning, 0, L"FwpmEngineSetOption", code, L"FWPM_ENGINE_PACKET_QUEUING");
		}
	}

CleanupExit:

	_r_spinlock_releaseshared (&lock_transaction);

	return is_success;
}

VOID _wfp_uninitialize (HANDLE hengine, BOOLEAN is_full)
{
	_r_spinlock_acquireshared (&lock_transaction);

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
			_r_log (Error, UID, L"FwpmSubLayerDeleteByKey", code, NULL);

		// destroy provider
		code = FwpmProviderDeleteByKey (hengine, &GUID_WfpProvider);

		if (code != ERROR_SUCCESS && code != FWP_E_PROVIDER_NOT_FOUND)
			_r_log (Error, UID, L"FwpmProviderDeleteByKey", code, NULL);

		if (is_intransact)
			_wfp_transact_commit (hengine, __LINE__);
	}

	_r_spinlock_releaseshared (&lock_transaction);
}

VOID _wfp_installfilters (HANDLE hengine)
{
	// set security information
	_app_setsecurityinfoforprovider (hengine, &GUID_WfpProvider, FALSE);
	_app_setsecurityinfoforsublayer (hengine, &GUID_WfpSublayer, FALSE);

	_wfp_clearfilter_ids ();

	_r_spinlock_acquireshared (&lock_transaction);

	// dump all filters into array
	PR_ARRAY guids = _r_obj_createarrayex (sizeof (GUID), 0x800, NULL);

	SIZE_T filters_count = _wfp_dumpfilters (hengine, &GUID_WfpProvider, guids);
	LPCGUID guid;

	// restore filters security
	if (filters_count)
	{
		for (SIZE_T i = 0; i < _r_obj_getarraysize (guids); i++)
		{
			guid = _r_obj_getarrayitem (guids, i);

			if (guid)
				_app_setsecurityinfoforfilter (hengine, guid, FALSE, __LINE__);
		}
	}

	BOOLEAN is_intransact = _wfp_transact_start (hengine, __LINE__);

	// destroy all filters
	if (filters_count)
	{
		for (SIZE_T i = 0; i < _r_obj_getarraysize (guids); i++)
		{
			guid = _r_obj_getarrayitem (guids, i);

			if (guid)
				_wfp_deletefilter (hengine, guid);
		}
	}

	// apply internal rules
	_wfp_create2filters (hengine, __LINE__, is_intransact);

	PR_LIST rules = _r_obj_createlistex (0x200, NULL);

	// apply apps rules
	PITEM_APP ptr_app;
	SIZE_T enum_key = 0;

	while (_r_obj_enumhashtable (apps, &ptr_app, NULL, &enum_key))
	{
		if (ptr_app->is_enabled)
		{
			_r_obj_addlistitem (rules, ptr_app);
		}
	}

	if (!_r_obj_islistempty (rules))
		_wfp_create3filters (hengine, rules, __LINE__, is_intransact);

	_r_obj_clearlist (rules);

	// apply blocklist/system/user rules
	for (SIZE_T i = 0; i < _r_obj_getarraysize (rules_arr); i++)
	{
		PITEM_RULE ptr_rule = _r_obj_getarrayitem (rules_arr, i);

		if (ptr_rule && ptr_rule->is_enabled)
		{
			_r_obj_addlistitem (rules, ptr_rule);
		}
	}

	if (!_r_obj_islistempty (rules))
		_wfp_create4filters (hengine, rules, __LINE__, is_intransact);

	_r_obj_clearlist (rules);

	if (is_intransact)
		_wfp_transact_commit (hengine, __LINE__);

	// secure filters
	BOOLEAN is_secure = _r_config_getboolean (L"IsSecureFilters", TRUE);

	if (is_secure)
	{
		if (_wfp_dumpfilters (hengine, &GUID_WfpProvider, guids))
		{
			for (SIZE_T i = 0; i < _r_obj_getarraysize (guids); i++)
			{
				guid = _r_obj_getarrayitem (guids, i);

				if (guid)
					_app_setsecurityinfoforfilter (hengine, guid, is_secure, __LINE__);
			}
		}
	}

	_app_setsecurityinfoforprovider (hengine, &GUID_WfpProvider, is_secure);
	_app_setsecurityinfoforsublayer (hengine, &GUID_WfpSublayer, is_secure);

	_r_obj_dereference (rules);
	_r_obj_dereference (guids);

	_r_spinlock_releaseshared (&lock_transaction);
}

BOOLEAN _wfp_transact_start (HANDLE hengine, UINT line)
{
	ULONG code = FwpmTransactionBegin (hengine, 0);

	//if (code == FWP_E_TXN_IN_PROGRESS)
	//	return FALSE;

	if (code != ERROR_SUCCESS)
	{
		_r_log_v (Error, UID, L"FwpmTransactionBegin", code, L"#%" TEXT (PRIu32), line);
		return FALSE;
	}

	return TRUE;
}

BOOLEAN _wfp_transact_commit (HANDLE hengine, UINT line)
{
	ULONG code = FwpmTransactionCommit (hengine);

	if (code != ERROR_SUCCESS)
	{
		FwpmTransactionAbort (hengine);

		_r_log_v (Error, UID, L"FwpmTransactionCommit", code, L"#%" TEXT (PRIu32), line);
		return FALSE;

	}

	return TRUE;
}

BOOLEAN _wfp_deletefilter (HANDLE hengine, LPCGUID filter_id)
{
	ULONG code = FwpmFilterDeleteByKey (hengine, filter_id);

#if !defined(_DEBUG)
	if (code != ERROR_SUCCESS && code != FWP_E_FILTER_NOT_FOUND)
#else
	if (code != ERROR_SUCCESS)
#endif // !DEBUG
	{
		PR_STRING guid_string = _r_str_fromguid (filter_id);

		_r_log (Error, UID, L"FwpmFilterDeleteByKey", code, _r_obj_getstringordefault (guid_string, SZ_EMPTY));

		if (guid_string)
			_r_obj_dereference (guid_string);

		return FALSE;
	}

	return TRUE;
}

FORCEINLINE LPCWSTR _wfp_filtertypetostring (ENUM_TYPE_DATA filter_type)
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
			return L"Apps";
		}

		case DataRuleBlocklist:
		{
			return L"Blocklist";
		}

		case DataRuleSystem:
		{
			return L"System rules";
		}

		case DataRuleUser:
		{
			return L"User rules";
		}

		case DataFilterGeneral:
		{
			return L"Internal";
		}
	}

	return NULL;
}

ULONG _wfp_createfilter (HANDLE hengine, ENUM_TYPE_DATA filter_type, LPCWSTR name, FWPM_FILTER_CONDITION* lpcond, UINT32 count, UINT8 weight, LPCGUID layer_id, LPCGUID callout_id, FWP_ACTION_TYPE action, UINT32 flags, PR_ARRAY guids)
{
	FWPM_FILTER filter = {0};

	WCHAR filter_description[128];
	WCHAR filter_name[64];
	LPCWSTR filter_type_string;
	UINT64 filter_id;
	ULONG code;

	// create filter guid
	HRESULT hr = CoCreateGuid (&filter.filterKey);

	if (FAILED (hr))
	{
		return hr;
	}

	_r_str_copy (filter_name, RTL_NUMBER_OF (filter_name), APP_NAME);

	filter_type_string = _wfp_filtertypetostring (filter_type);

	if (filter_type_string && name)
	{
		_r_str_printf (filter_description, RTL_NUMBER_OF (filter_description), L"%s\\%s", filter_type_string, name);
	}
	else if (name)
	{
		_r_str_copy (filter_description, RTL_NUMBER_OF (filter_description), name);
	}
	else
	{
		_r_str_copy (filter_description, RTL_NUMBER_OF (filter_description), SZ_EMPTY);
	}

	// set filter flags
	if ((flags & FWPM_FILTER_FLAG_BOOTTIME) == 0)
	{
		if (!config.is_filterstemporary)
			filter.flags = FWPM_FILTER_FLAG_PERSISTENT;

		// filter is indexed to help enable faster lookup during classification (win8+)
		if (_r_sys_isosversiongreaterorequal (WINDOWS_8))
			filter.flags |= FWPM_FILTER_FLAG_INDEXED;
	}

	if (flags)
		filter.flags |= flags;

	filter.displayData.name = filter_name;
	filter.displayData.description = filter_description;
	filter.providerKey = (GUID*)&GUID_WfpProvider;
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
		_r_log (Error, UID, L"FwpmFilterAdd", code, filter.displayData.description);
	}

	return code;
}

VOID _wfp_clearfilter_ids ()
{
	PITEM_APP ptr_app;
	PITEM_RULE ptr_rule;
	SIZE_T enum_key = 0;

	// clear common filters
	_r_obj_cleararray (filter_ids);

	// clear apps filters
	while (_r_obj_enumhashtable (apps, &ptr_app, NULL, &enum_key))
	{
		ptr_app->is_haveerrors = FALSE;

		_r_obj_cleararray (ptr_app->guids);
	}

	// clear rules filters
	for (SIZE_T i = 0; i < _r_obj_getarraysize (rules_arr); i++)
	{
		ptr_rule = _r_obj_getarrayitem (rules_arr, i);

		if (!ptr_rule)
			continue;

		ptr_rule->is_haveerrors = FALSE;

		_r_obj_cleararray (ptr_rule->guids);
	}
}

VOID _wfp_destroyfilters (HANDLE hengine)
{
	_wfp_clearfilter_ids ();

	// destroy all filters
	PR_ARRAY guids = _r_obj_createarrayex (sizeof (GUID), 0x1000, NULL);

	_r_spinlock_acquireshared (&lock_transaction);

	if (_wfp_dumpfilters (hengine, &GUID_WfpProvider, guids))
		_wfp_destroyfilters_array (hengine, guids, __LINE__);

	_r_obj_dereference (guids);

	_r_spinlock_releaseshared (&lock_transaction);
}

BOOLEAN _wfp_destroyfilters_array (HANDLE hengine, PR_ARRAY guids, UINT line)
{
	if (_r_obj_isarrayempty (guids))
		return FALSE;

	LPCGUID guid;
	BOOLEAN is_enabled = _app_initinterfacestate (_r_app_gethwnd (), FALSE);

	_r_spinlock_acquireshared (&lock_transaction);

	for (SIZE_T i = 0; i < _r_obj_getarraysize (guids); i++)
	{
		guid = _r_obj_getarrayitem (guids, i);

		if (guid)
			_app_setsecurityinfoforfilter (hengine, guid, FALSE, line);
	}

	BOOLEAN is_intransact = _wfp_transact_start (hengine, line);

	for (SIZE_T i = 0; i < _r_obj_getarraysize (guids); i++)
	{
		guid = _r_obj_getarrayitem (guids, i);

		if (guid)
			_wfp_deletefilter (hengine, guid);
	}

	if (is_intransact)
		_wfp_transact_commit (hengine, line);

	_r_spinlock_releaseshared (&lock_transaction);

	_app_restoreinterfacestate (_r_app_gethwnd (), is_enabled);

	return TRUE;
}

BOOLEAN _wfp_createrulefilter (HANDLE hengine, ENUM_TYPE_DATA filter_type, LPCWSTR name, SIZE_T app_hash, PR_STRING rule_remote, PR_STRING rule_local, UINT8 protocol, ADDRESS_FAMILY af, FWP_DIRECTION dir, UINT8 weight, FWP_ACTION_TYPE action, UINT32 flag, PR_ARRAY guids)
{
	UINT32 count = 0;
	FWPM_FILTER_CONDITION fwfc[8] = {0};

	ITEM_ADDRESS addr;

	FWP_BYTE_BLOB* byte_blob = NULL;
	PITEM_APP ptr_app = NULL;
	BOOLEAN is_remoteaddr_set = FALSE;
	BOOLEAN is_remoteport_set = FALSE;
	BOOLEAN is_success = FALSE;

	memset (&addr, 0, sizeof (addr));

	// set path condition
	if (app_hash)
	{
		ptr_app = _r_obj_findhashtable (apps, app_hash);

		if (!ptr_app)
		{
			_r_log_v (Error, 0, TEXT (__FUNCTION__), 0, L"App \"%" TEXT (PR_SIZE_T) L"\" not found!", app_hash);

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
				_r_log (Error, 0, TEXT (__FUNCTION__), 0, _app_getdisplayname (ptr_app, TRUE));

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
				fwfc[count].conditionValue.sid = (SID*)ptr_app->pbytes->buffer;

				count += 1;
			}
			else
			{
				_r_log (Error, 0, TEXT (__FUNCTION__), 0, _app_getdisplayname (ptr_app, TRUE));

				goto CleanupExit;
			}
		}
		else
		{
			LPCWSTR path = _r_obj_getstring (ptr_app->original_path);
			ULONG code = _FwpmGetAppIdFromFileName1 (path, &byte_blob, ptr_app->type);

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
					_r_log (Error, 0, L"FwpmGetAppIdFromFileName", code, path);

				goto CleanupExit;
			}
		}
	}

	// set ip/port condition
	{
		PR_STRING rules[] = {
			rule_remote,
			rule_local
		};

		for (SIZE_T i = 0; i < RTL_NUMBER_OF (rules); i++)
		{
			if (_r_obj_isstringempty (rules[i]))
				continue;

			if (!_app_parserulestring (rules[i], &addr))
			{
				goto CleanupExit;
			}
			else
			{
				if (i == 0)
				{
					if (addr.type == DataTypeIp || addr.type == DataTypeHost)
						is_remoteaddr_set = TRUE;

					else if (addr.type == DataTypePort)
						is_remoteport_set = TRUE;
				}

				if (addr.is_range && (addr.type == DataTypeIp || addr.type == DataTypePort))
				{
					if (addr.type == DataTypeIp)
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
							goto CleanupExit;
						}
					}

					fwfc[count].fieldKey = (addr.type == DataTypePort) ? ((i == 0) ? FWPM_CONDITION_IP_REMOTE_PORT : FWPM_CONDITION_IP_LOCAL_PORT) : ((i == 0) ? FWPM_CONDITION_IP_REMOTE_ADDRESS : FWPM_CONDITION_IP_LOCAL_ADDRESS);
					fwfc[count].matchType = FWP_MATCH_RANGE;
					fwfc[count].conditionValue.type = FWP_RANGE_TYPE;
					fwfc[count].conditionValue.rangeValue = &addr.range;

					count += 1;
				}
				else if (addr.type == DataTypePort)
				{
					fwfc[count].fieldKey = ((i == 0) ? FWPM_CONDITION_IP_REMOTE_PORT : FWPM_CONDITION_IP_LOCAL_PORT);
					fwfc[count].matchType = FWP_MATCH_EQUAL;
					fwfc[count].conditionValue.type = FWP_UINT16;
					fwfc[count].conditionValue.uint16 = addr.port;

					count += 1;
				}
				else if (addr.type == DataTypeHost || addr.type == DataTypeIp)
				{
					fwfc[count].fieldKey = ((i == 0) ? FWPM_CONDITION_IP_REMOTE_ADDRESS : FWPM_CONDITION_IP_LOCAL_ADDRESS);
					fwfc[count].matchType = FWP_MATCH_EQUAL;

					if (addr.format == NET_ADDRESS_IPV4)
					{
						af = AF_INET;

						fwfc[count].conditionValue.type = FWP_V4_ADDR_MASK;
						fwfc[count].conditionValue.v4AddrMask = &addr.addr4;

						count += 1;
					}
					else if (addr.format == NET_ADDRESS_IPV6)
					{
						af = AF_INET6;

						fwfc[count].conditionValue.type = FWP_V6_ADDR_MASK;
						fwfc[count].conditionValue.v6AddrMask = &addr.addr6;

						count += 1;
					}
					else if (addr.format == NET_ADDRESS_DNS_NAME)
					{
						PR_STRING host_part;
						R_STRINGREF remaining_part;

						_r_obj_initializestringref (&remaining_part, addr.host);

						while (remaining_part.length != 0)
						{
							host_part = _r_str_splitatchar (&remaining_part, &remaining_part, DIVIDER_RULE[0]);

							if (host_part)
							{
								if (!_wfp_createrulefilter (hengine, filter_type, name, app_hash, host_part, NULL, protocol, af, dir, weight, action, flag, guids))
								{
									_r_obj_dereference (host_part);

									goto CleanupExit;
								}

								_r_obj_dereference (host_part);
							}
						}

						is_success = TRUE;

						goto CleanupExit;
					}
					else
					{
						goto CleanupExit;
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
					goto CleanupExit;
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
			_wfp_createfilter (hengine, filter_type, name, fwfc, count, weight, &FWPM_LAYER_ALE_AUTH_CONNECT_V4, NULL, action, flag, guids);

			// win7+
			_wfp_createfilter (hengine, filter_type, name, fwfc, count, weight, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V4, NULL, action, flag, guids);
		}

		if (af == AF_INET6 || af == AF_UNSPEC)
		{
			_wfp_createfilter (hengine, filter_type, name, fwfc, count, weight, &FWPM_LAYER_ALE_AUTH_CONNECT_V6, NULL, action, flag, guids);

			// win7+
			_wfp_createfilter (hengine, filter_type, name, fwfc, count, weight, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V6, NULL, action, flag, guids);
		}
	}

	// create inbound layer filter
	if (dir == FWP_DIRECTION_INBOUND || dir == FWP_DIRECTION_MAX)
	{
		if (af == AF_INET || af == AF_UNSPEC)
		{
			_wfp_createfilter (hengine, filter_type, name, fwfc, count, weight, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, NULL, action, flag, guids);

			if (action == FWP_ACTION_BLOCK && (!is_remoteaddr_set && !is_remoteport_set))
				_wfp_createfilter (hengine, filter_type, name, fwfc, count, weight, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V4, NULL, FWP_ACTION_PERMIT, flag, guids);
		}

		if (af == AF_INET6 || af == AF_UNSPEC)
		{
			_wfp_createfilter (hengine, filter_type, name, fwfc, count, weight, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, action, flag, guids);

			if (action == FWP_ACTION_BLOCK && (!is_remoteaddr_set && !is_remoteport_set))
				_wfp_createfilter (hengine, filter_type, name, fwfc, count, weight, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V6, NULL, FWP_ACTION_PERMIT, flag, guids);
		}
	}

	is_success = TRUE;

CleanupExit:

	if (byte_blob)
		ByteBlobFree (&byte_blob);

	return is_success;
}

BOOLEAN _wfp_create4filters (HANDLE hengine, PR_LIST rules, UINT line, BOOLEAN is_intransact)
{
	if (_r_obj_islistempty (rules))
		return FALSE;

	BOOLEAN is_enabled = _app_initinterfacestate (_r_app_gethwnd (), FALSE);

	if (!is_intransact && _wfp_isfiltersapplying ())
		is_intransact = TRUE;

	PR_ARRAY guids = _r_obj_createarrayex (sizeof (GUID), 0x400, NULL);
	LPCGUID guid;

	if (!is_intransact)
	{
		for (SIZE_T i = 0; i < _r_obj_getlistsize (rules); i++)
		{
			PITEM_RULE ptr_rule = _r_obj_getlistitem (rules, i);

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

		_r_spinlock_acquireshared (&lock_transaction);
		is_intransact = !_wfp_transact_start (hengine, line);
	}

	for (SIZE_T i = 0; i < _r_obj_getarraysize (guids); i++)
	{
		guid = _r_obj_getarrayitem (guids, i);

		if (guid)
			_wfp_deletefilter (hengine, guid);
	}

	PITEM_RULE ptr_rule;
	R_STRINGREF remote_remaining_part;
	R_STRINGREF local_remaining_part;
	PR_STRING rule_remote_string = NULL;
	PR_STRING rule_local_string = NULL;
	LPCWSTR rule_name_ptr;

	for (SIZE_T i = 0; i < _r_obj_getlistsize (rules); i++)
	{
		ptr_rule = _r_obj_getlistitem (rules, i);

		if (!ptr_rule)
			continue;

		ptr_rule->is_haveerrors = FALSE;
		_r_obj_cleararray (ptr_rule->guids);

		if (!ptr_rule->is_enabled)
			continue;

		rule_name_ptr = _r_obj_getstring (ptr_rule->name);

		if (!_r_obj_isstringempty (ptr_rule->rule_remote))
		{
			rule_remote_string = _r_str_splitatchar (&ptr_rule->rule_remote->sr, &remote_remaining_part, DIVIDER_RULE[0]);
		}
		else
		{
			_r_obj_initializeemptystringref (&remote_remaining_part);
		}

		if (!_r_obj_isstringempty (ptr_rule->rule_local))
		{
			rule_local_string = _r_str_splitatchar (&ptr_rule->rule_local->sr, &local_remaining_part, DIVIDER_RULE[0]);
		}
		else
		{
			_r_obj_initializeemptystringref (&local_remaining_part);
		}

		while (TRUE)
		{
			// apply rules for services hosts
			if (ptr_rule->is_forservices)
			{
				if (!_wfp_createrulefilter (hengine, ptr_rule->type, rule_name_ptr, config.ntoskrnl_hash, rule_remote_string, rule_local_string, ptr_rule->protocol, ptr_rule->af, ptr_rule->direction, ptr_rule->weight, ptr_rule->is_block ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT, 0, ptr_rule->guids))
					ptr_rule->is_haveerrors = TRUE;

				if (!_wfp_createrulefilter (hengine, ptr_rule->type, rule_name_ptr, config.svchost_hash, rule_remote_string, rule_local_string, ptr_rule->protocol, ptr_rule->af, ptr_rule->direction, ptr_rule->weight, ptr_rule->is_block ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT, 0, ptr_rule->guids))
					ptr_rule->is_haveerrors = TRUE;
			}

			if (!_r_obj_ishashtableempty (ptr_rule->apps))
			{
				PR_HASHSTORE hashstore;
				SIZE_T hash_code;
				SIZE_T enum_key = 0;

				while (_r_obj_enumhashtable (ptr_rule->apps, &hashstore, &hash_code, &enum_key))
				{
					if (ptr_rule->is_forservices && (hash_code == config.ntoskrnl_hash || hash_code == config.svchost_hash))
						continue;

					if (!_wfp_createrulefilter (hengine, ptr_rule->type, rule_name_ptr, hash_code, rule_remote_string, rule_local_string, ptr_rule->protocol, ptr_rule->af, ptr_rule->direction, ptr_rule->weight, ptr_rule->is_block ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT, 0, ptr_rule->guids))
						ptr_rule->is_haveerrors = TRUE;
				}
			}
			else
			{
				if (!_wfp_createrulefilter (hengine, ptr_rule->type, rule_name_ptr, 0, rule_remote_string, rule_local_string, ptr_rule->protocol, ptr_rule->af, ptr_rule->direction, ptr_rule->weight, ptr_rule->is_block ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT, 0, ptr_rule->guids))
					ptr_rule->is_haveerrors = TRUE;
			}

			if (remote_remaining_part.length == 0 && local_remaining_part.length == 0)
				break;

			if (remote_remaining_part.length != 0)
			{
				_r_obj_movereference (&rule_remote_string, _r_str_splitatchar (&remote_remaining_part, &remote_remaining_part, DIVIDER_RULE[0]));
			}

			if (local_remaining_part.length != 0)
			{
				_r_obj_movereference (&rule_local_string, _r_str_splitatchar (&local_remaining_part, &local_remaining_part, DIVIDER_RULE[0]));
			}
		}

		SAFE_DELETE_REFERENCE (rule_remote_string);
		SAFE_DELETE_REFERENCE (rule_local_string);
	}

	if (!is_intransact)
	{
		_wfp_transact_commit (hengine, line);

		BOOLEAN is_secure = _r_config_getboolean (L"IsSecureFilters", TRUE);

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

		_r_spinlock_releaseshared (&lock_transaction);
	}

	_app_restoreinterfacestate (_r_app_gethwnd (), is_enabled);

	_r_obj_dereference (guids);

	return TRUE;
}

BOOLEAN _wfp_create3filters (HANDLE hengine, PR_LIST rules, UINT line, BOOLEAN is_intransact)
{
	if (_r_obj_islistempty (rules))
		return FALSE;

	BOOLEAN is_enabled = _app_initinterfacestate (_r_app_gethwnd (), FALSE);
	FWP_ACTION_TYPE action = FWP_ACTION_PERMIT;

	if (!is_intransact && _wfp_isfiltersapplying ())
		is_intransact = TRUE;

	PR_ARRAY guids = _r_obj_createarrayex (sizeof (GUID), 0x400, NULL);
	LPCGUID guid;

	if (!is_intransact)
	{
		for (SIZE_T i = 0; i < _r_obj_getlistsize (rules); i++)
		{
			PITEM_APP ptr_app = _r_obj_getlistitem (rules, i);

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

		_r_spinlock_acquireshared (&lock_transaction);
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
		PITEM_APP ptr_app = _r_obj_getlistitem (rules, i);

		if (ptr_app && ptr_app->is_enabled)
		{
			if (!_wfp_createrulefilter (hengine, ptr_app->type, _app_getdisplayname (ptr_app, TRUE), ptr_app->app_hash, NULL, NULL, 0, AF_UNSPEC, FWP_DIRECTION_MAX, FILTER_WEIGHT_APPLICATION, action, 0, ptr_app->guids))
				ptr_app->is_haveerrors = TRUE;
		}
	}

	if (!is_intransact)
	{
		_wfp_transact_commit (hengine, line);

		BOOLEAN is_secure = _r_config_getboolean (L"IsSecureFilters", TRUE);

		if (is_secure)
		{
			for (SIZE_T i = 0; i < _r_obj_getlistsize (rules); i++)
			{
				PITEM_APP ptr_app = _r_obj_getlistitem (rules, i);

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

		_r_spinlock_releaseshared (&lock_transaction);
	}

	_app_restoreinterfacestate (_r_app_gethwnd (), is_enabled);

	_r_obj_dereference (guids);

	return TRUE;
}

BOOLEAN _wfp_create2filters (HANDLE hengine, UINT line, BOOLEAN is_intransact)
{
	LPCGUID guid;
	BOOLEAN is_enabled = _app_initinterfacestate (_r_app_gethwnd (), FALSE);

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

		_r_spinlock_acquireshared (&lock_transaction);
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

		_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_CONNECT_V4, NULL, FWP_ACTION_PERMIT, 0, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_CONNECT_V6, NULL, FWP_ACTION_PERMIT, 0, filter_ids);

		// win7+
		_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V4, NULL, FWP_ACTION_PERMIT, 0, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V6, NULL, FWP_ACTION_PERMIT, 0, filter_ids);

		_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, NULL, FWP_ACTION_PERMIT, 0, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, FWP_ACTION_PERMIT, 0, filter_ids);

		_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V4, NULL, FWP_ACTION_PERMIT, 0, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V6, NULL, FWP_ACTION_PERMIT, 0, filter_ids);

		// ipv4/ipv6 loopback
		LPCWSTR ip_list[] = {
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

		ITEM_ADDRESS addr;
		PR_STRING rule_string;

		for (SIZE_T i = 0; i < RTL_NUMBER_OF (ip_list); i++)
		{
			memset (&addr, 0, sizeof (addr));

			rule_string = _r_obj_createstring (ip_list[i]);

			if (_app_parserulestring (rule_string, &addr))
			{
				fwfc[1].matchType = FWP_MATCH_EQUAL;

				if (addr.format == NET_ADDRESS_IPV4)
				{
					fwfc[1].conditionValue.type = FWP_V4_ADDR_MASK;
					fwfc[1].conditionValue.v4AddrMask = &addr.addr4;

					fwfc[1].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
					_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_CONNECT_V4, NULL, FWP_ACTION_PERMIT, 0, filter_ids);

					// win7+
					_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V4, NULL, FWP_ACTION_PERMIT, 0, filter_ids);

					fwfc[1].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;
					_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, NULL, FWP_ACTION_PERMIT, 0, filter_ids);
					_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V4, NULL, FWP_ACTION_PERMIT, 0, filter_ids);
				}
				else if (addr.format == NET_ADDRESS_IPV6)
				{
					fwfc[1].conditionValue.type = FWP_V6_ADDR_MASK;
					fwfc[1].conditionValue.v6AddrMask = &addr.addr6;

					fwfc[1].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
					_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_CONNECT_V6, NULL, FWP_ACTION_PERMIT, 0, filter_ids);

					// win7+
					_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V6, NULL, FWP_ACTION_PERMIT, 0, filter_ids);

					fwfc[1].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;
					_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, FWP_ACTION_PERMIT, 0, filter_ids);
					_wfp_createfilter (hengine, DataFilterGeneral, NULL, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V6, NULL, FWP_ACTION_PERMIT, 0, filter_ids);
				}
			}

			_r_obj_dereference (rule_string);
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

		_wfp_createfilter (hengine, DataFilterGeneral, L"Allow6to4", fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, NULL, FWP_ACTION_PERMIT, 0, filter_ids);

		// allows icmpv6 router solicitation messages, which are required for the ipv6 stack to work properly
		fwfc[0].fieldKey = FWPM_CONDITION_ICMP_TYPE;
		fwfc[0].matchType = FWP_MATCH_EQUAL;
		fwfc[0].conditionValue.type = FWP_UINT16;
		fwfc[0].conditionValue.uint16 = 0x85;

		_wfp_createfilter (hengine, DataFilterGeneral, L"AllowIcmpV6Type133", fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, FWP_ACTION_PERMIT, 0, filter_ids);

		// allows icmpv6 router advertise messages, which are required for the ipv6 stack to work properly
		fwfc[0].conditionValue.uint16 = 0x86;
		_wfp_createfilter (hengine, DataFilterGeneral, L"AllowIcmpV6Type134", fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, FWP_ACTION_PERMIT, 0, filter_ids);

		// allows icmpv6 neighbor solicitation messages, which are required for the ipv6 stack to work properly
		fwfc[0].conditionValue.uint16 = 0x87;
		_wfp_createfilter (hengine, DataFilterGeneral, L"AllowIcmpV6Type135", fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, FWP_ACTION_PERMIT, 0, filter_ids);

		// allows icmpv6 neighbor advertise messages, which are required for the ipv6 stack to work properly
		fwfc[0].conditionValue.uint16 = 0x88;
		_wfp_createfilter (hengine, DataFilterGeneral, L"AllowIcmpV6Type136", fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, FWP_ACTION_PERMIT, 0, filter_ids);
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

		_wfp_createfilter (hengine, DataFilterGeneral, FILTER_NAME_ICMP_ERROR, fwfc, 2, FILTER_WEIGHT_HIGHEST, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V4, NULL, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, FILTER_NAME_ICMP_ERROR, fwfc, 2, FILTER_WEIGHT_HIGHEST, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V6, NULL, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, filter_ids);

		// blocks tcp port scanners (exclude loopback)
		fwfc[0].conditionValue.uint32 |= FWP_CONDITION_FLAG_IS_IPSEC_SECURED;

		_wfp_createfilter (hengine, DataFilterGeneral, FILTER_NAME_TCP_RST_ONCLOSE, fwfc, 1, FILTER_WEIGHT_HIGHEST, &FWPM_LAYER_INBOUND_TRANSPORT_V4_DISCARD, &FWPM_CALLOUT_WFP_TRANSPORT_LAYER_V4_SILENT_DROP, FWP_ACTION_CALLOUT_TERMINATING, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, FILTER_NAME_TCP_RST_ONCLOSE, fwfc, 1, FILTER_WEIGHT_HIGHEST, &FWPM_LAYER_INBOUND_TRANSPORT_V6_DISCARD, &FWPM_CALLOUT_WFP_TRANSPORT_LAYER_V6_SILENT_DROP, FWP_ACTION_CALLOUT_TERMINATING, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, filter_ids);
	}

	// configure outbound layer
	{
		FWP_ACTION_TYPE action = _r_config_getboolean (L"BlockOutboundConnections", TRUE) ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT;

		_wfp_createfilter (hengine, DataFilterGeneral, FILTER_NAME_BLOCK_CONNECTION, NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_CONNECT_V4, NULL, action, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, FILTER_NAME_BLOCK_CONNECTION, NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_CONNECT_V6, NULL, action, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, filter_ids);

		// win7+
		_wfp_createfilter (hengine, DataFilterGeneral, FILTER_NAME_BLOCK_CONNECTION_REDIRECT, NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V4, NULL, action, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, FILTER_NAME_BLOCK_CONNECTION_REDIRECT, NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V6, NULL, action, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, filter_ids);
	}

	// configure inbound layer
	{
		FWP_ACTION_TYPE action = (_r_config_getboolean (L"UseStealthMode", TRUE) || _r_config_getboolean (L"BlockInboundConnections", TRUE)) ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT;

		_wfp_createfilter (hengine, DataFilterGeneral, FILTER_NAME_BLOCK_RECVACCEPT, NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, NULL, action, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, FILTER_NAME_BLOCK_RECVACCEPT, NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, action, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, filter_ids);

		//_wfp_createfilter (hengine, DataFilterGeneral, L"BlockResourceAssignmentV4", NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V4, NULL, action, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, filter_ids);
		//_wfp_createfilter (hengine, DataFilterGeneral, L"BlockResourceAssignmentV6", NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V6, NULL, action, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, filter_ids);
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

		_wfp_createfilter (hengine, DataFilterGeneral, FILTER_NAME_BOOTTIME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_IPFORWARD_V4, NULL, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, FILTER_NAME_BOOTTIME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_IPFORWARD_V6, NULL, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);

		_wfp_createfilter (hengine, DataFilterGeneral, FILTER_NAME_BOOTTIME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, NULL, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, FILTER_NAME_BOOTTIME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);

		_wfp_createfilter (hengine, DataFilterGeneral, FILTER_NAME_BOOTTIME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_INBOUND_ICMP_ERROR_V4, NULL, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, FILTER_NAME_BOOTTIME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_INBOUND_ICMP_ERROR_V6, NULL, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);

		_wfp_createfilter (hengine, DataFilterGeneral, FILTER_NAME_BOOTTIME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V4, NULL, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, FILTER_NAME_BOOTTIME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V6, NULL, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);

		_wfp_createfilter (hengine, DataFilterGeneral, FILTER_NAME_BOOTTIME, NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, NULL, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, FILTER_NAME_BOOTTIME, NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);

		_wfp_createfilter (hengine, DataFilterGeneral, FILTER_NAME_BOOTTIME, NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_INBOUND_ICMP_ERROR_V4, NULL, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, FILTER_NAME_BOOTTIME, NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_INBOUND_ICMP_ERROR_V6, NULL, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);

		_wfp_createfilter (hengine, DataFilterGeneral, FILTER_NAME_BOOTTIME, NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V4, NULL, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, FILTER_NAME_BOOTTIME, NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V6, NULL, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);

		// win7+ boot-time features
		fwfc[0].fieldKey = FWPM_CONDITION_FLAGS;
		fwfc[0].matchType = FWP_MATCH_FLAGS_NONE_SET;
		fwfc[0].conditionValue.type = FWP_UINT32;
		fwfc[0].conditionValue.uint32 = FWP_CONDITION_FLAG_IS_OUTBOUND_PASS_THRU | FWP_CONDITION_FLAG_IS_INBOUND_PASS_THRU;

		_wfp_createfilter (hengine, DataFilterGeneral, FILTER_NAME_BOOTTIME, fwfc, 1, FILTER_WEIGHT_APPLICATION, &FWPM_LAYER_IPFORWARD_V4, NULL, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);
		_wfp_createfilter (hengine, DataFilterGeneral, FILTER_NAME_BOOTTIME, fwfc, 1, FILTER_WEIGHT_APPLICATION, &FWPM_LAYER_IPFORWARD_V6, NULL, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, filter_ids);
	}

	if (!is_intransact)
	{
		_wfp_transact_commit (hengine, line);

		BOOLEAN is_secure = _r_config_getboolean (L"IsSecureFilters", TRUE);

		if (is_secure)
		{
			for (SIZE_T i = 0; i < _r_obj_getarraysize (filter_ids); i++)
			{
				guid = _r_obj_getarrayitem (filter_ids, i);

				if (guid)
					_app_setsecurityinfoforfilter (hengine, guid, is_secure, line);
			}
		}

		_r_spinlock_releaseshared (&lock_transaction);
	}

	_app_restoreinterfacestate (_r_app_gethwnd (), is_enabled);

	return TRUE;
}

SIZE_T _wfp_dumpfilters (HANDLE hengine, LPCGUID provider_id, PR_ARRAY guids)
{
	_r_obj_cleararray (guids);

	UINT32 count = 0;
	SIZE_T result = 0;
	HANDLE henum = NULL;

	ULONG code = FwpmFilterCreateEnumHandle (hengine, NULL, &henum);

	if (code == ERROR_SUCCESS)
	{
		FWPM_FILTER** matching_filters_enum = NULL;

		code = FwpmFilterEnum (hengine, henum, UINT32_MAX, &matching_filters_enum, &count);

		if (code == ERROR_SUCCESS)
		{
			if (matching_filters_enum)
			{
				for (UINT32 i = 0; i < count; i++)
				{
					FWPM_FILTER* filter = matching_filters_enum[i];

					if (filter && filter->providerKey && memcmp (filter->providerKey, provider_id, sizeof (GUID)) == 0)
					{
						_r_obj_addarrayitem (guids, &filter->filterKey);
						result += 1;
					}
				}

				FwpmFreeMemory ((PVOID*)&matching_filters_enum);
			}
		}
	}

	if (henum)
		FwpmFilterDestroyEnumHandle (hengine, henum);

	return result;
}

BOOLEAN _mps_firewallapi (PBOOLEAN pis_enabled, PBOOLEAN pis_enable)
{
	if (!pis_enabled && !pis_enable)
		return FALSE;

	BOOLEAN result = FALSE;

	INetFwPolicy2* INetFwPolicy = NULL;
	HRESULT hr = CoCreateInstance (&IID_INetFwPolicy2, NULL, CLSCTX_INPROC_SERVER, &IID_INetFwPolicy2, &INetFwPolicy);

	if (FAILED (hr))
		goto CleanupExit;

	NET_FW_PROFILE_TYPE2 profile_types[] = {
		NET_FW_PROFILE2_DOMAIN,
		NET_FW_PROFILE2_PRIVATE,
		NET_FW_PROFILE2_PUBLIC
	};

	if (pis_enabled)
	{
		*pis_enabled = FALSE;

		for (SIZE_T i = 0; i < RTL_NUMBER_OF (profile_types); i++)
		{
			VARIANT_BOOL is_enabled;

			hr = INetFwPolicy2_get_FirewallEnabled (INetFwPolicy, profile_types[i], &is_enabled);

			if (SUCCEEDED (hr))
			{
				if (is_enabled == VARIANT_TRUE)
				{
					*pis_enabled = TRUE;
					break;
				}

				result = TRUE;
			}
		}
	}

	if (pis_enable)
	{
		for (SIZE_T i = 0; i < RTL_NUMBER_OF (profile_types); i++)
		{
			hr = INetFwPolicy2_put_FirewallEnabled (INetFwPolicy, profile_types[i], *pis_enable ? VARIANT_TRUE : VARIANT_FALSE);

			if (SUCCEEDED (hr))
			{
				result = TRUE;
			}
			else
			{
				_r_log_v (Information, 0, L"INetFwPolicy2_put_FirewallEnabled", hr, L"%d", profile_types[i]);
			}
		}
	}

CleanupExit:

	if (INetFwPolicy)
		INetFwPolicy2_Release (INetFwPolicy);

	return result;
}

VOID _mps_changeconfig2 (BOOLEAN is_enable)
{
	// check settings
	BOOLEAN is_wfenabled = FALSE;
	_mps_firewallapi (&is_wfenabled, NULL);

	if (is_wfenabled == is_enable)
		return;

	SC_HANDLE scm = OpenSCManager (NULL, NULL, SC_MANAGER_ALL_ACCESS);

	if (!scm)
	{
		_r_log (Information, 0, L"OpenSCManager", GetLastError (), NULL);
	}
	else
	{
		LPCWSTR service_names[] = {
			L"mpssvc",
			L"mpsdrv",
		};

		BOOLEAN is_started = FALSE;

		for (SIZE_T i = 0; i < RTL_NUMBER_OF (service_names); i++)
		{
			SC_HANDLE sc = OpenService (scm, service_names[i], SERVICE_CHANGE_CONFIG | SERVICE_QUERY_STATUS | SERVICE_STOP);

			if (!sc)
			{
				ULONG code = GetLastError ();

				if (code != ERROR_ACCESS_DENIED)
					_r_log (Information, 0, L"OpenService", code, service_names[i]);
			}
			else
			{
				if (!is_started)
				{
					SERVICE_STATUS status;

					if (QueryServiceStatus (sc, &status))
						is_started = (status.dwCurrentState == SERVICE_RUNNING);
				}

				if (!ChangeServiceConfig (sc, SERVICE_NO_CHANGE, is_enable ? SERVICE_AUTO_START : SERVICE_DISABLED, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL))
					_r_log (Information, 0, L"ChangeServiceConfig", GetLastError (), service_names[i]);

				CloseServiceHandle (sc);
			}
		}

		// start services
		if (is_enable)
		{
			for (SIZE_T i = 0; i < RTL_NUMBER_OF (service_names); i++)
			{
				SC_HANDLE sc = OpenService (scm, service_names[i], SERVICE_QUERY_STATUS | SERVICE_START);

				if (!sc)
				{
					_r_log (Information, 0, L"OpenService", GetLastError (), service_names[i]);
				}
				else
				{
					SERVICE_STATUS_PROCESS ssp = {0};
					ULONG bytes_required = 0;

					if (!QueryServiceStatusEx (sc, SC_STATUS_PROCESS_INFO, (PBYTE)&ssp, sizeof (ssp), &bytes_required))
					{
						_r_log (Information, 0, L"QueryServiceStatusEx", GetLastError (), service_names[i]);
					}
					else
					{
						if (ssp.dwCurrentState != SERVICE_RUNNING)
						{
							if (!StartService (sc, 0, NULL))
								_r_log (Information, 0, L"StartService", GetLastError (), service_names[i]);
						}

						CloseServiceHandle (sc);
					}
				}
			}

			_r_sleep (250);
		}

		_mps_firewallapi (NULL, &is_enable);

		CloseServiceHandle (scm);
	}
}

ULONG _FwpmGetAppIdFromFileName1 (LPCWSTR path, FWP_BYTE_BLOB** lpblob, ENUM_TYPE_DATA type)
{
	ULONG code = ERROR_FILE_NOT_FOUND;

	PR_STRING original_path = _r_obj_createstring (path);
	PR_STRING nt_path = NULL;

	if (type == DataAppRegular || type == DataAppNetwork || type == DataAppService)
	{
		if (_r_obj_getstringhash (original_path) == config.ntoskrnl_hash)
		{
			ByteBlobAlloc (original_path->buffer, original_path->length + sizeof (UNICODE_NULL), lpblob);
			code = ERROR_SUCCESS;

			goto CleanupExit;
		}
		else
		{
			code = _r_path_ntpathfromdos (path, &nt_path);

			// file is inaccessible or not found, maybe low-level driver preventing file access?
			// try another way!
			if (
				code == ERROR_ACCESS_DENIED ||
				code == ERROR_FILE_NOT_FOUND ||
				code == ERROR_PATH_NOT_FOUND
				)
			{
				if (PathIsRelative (path))
				{
					goto CleanupExit;
				}
				else
				{
					WCHAR path_root[128];
					WCHAR path_skip_root[512];

					// file path (root)
					_r_str_copy (path_root, RTL_NUMBER_OF (path_root), path);
					PathStripToRoot (path_root);

					// file path (without root)
					_r_str_copy (path_skip_root, RTL_NUMBER_OF (path_skip_root), PathSkipRoot (path));
					_r_str_tolower (path_skip_root, _r_str_length (path_skip_root)); // lower is important!

					code = _r_path_ntpathfromdos (path_root, &nt_path);

					if (code != ERROR_SUCCESS)
						goto CleanupExit;

					_r_obj_movereference (&original_path, _r_format_string (L"%s%s", nt_path->buffer, path_skip_root));
				}
			}
			else if (code == ERROR_SUCCESS)
			{
				_r_obj_movereference (&original_path, nt_path);
				nt_path = NULL;
			}
			else
			{
				goto CleanupExit;

			}

			ByteBlobAlloc (original_path->buffer, original_path->length + sizeof (UNICODE_NULL), lpblob);
		}
	}
	else if (type == DataAppPico || type == DataAppDevice)
	{
		if (type == DataAppDevice)
			_r_str_tolower (original_path->buffer, _r_obj_getstringlength (original_path)); // lower is important!

		ByteBlobAlloc (original_path->buffer, original_path->length + sizeof (UNICODE_NULL), lpblob);
		code = ERROR_SUCCESS;

		goto CleanupExit;
	}

CleanupExit:

	if (original_path)
		_r_obj_dereference (original_path);

	if (nt_path)
		_r_obj_dereference (nt_path);

	return code;
}

VOID ByteBlobAlloc (_In_ LPCVOID data, _In_ SIZE_T bytes_count, _Outptr_ FWP_BYTE_BLOB** byte_blob)
{
	FWP_BYTE_BLOB* blob = _r_mem_allocatezero (sizeof (FWP_BYTE_BLOB) + bytes_count);

	blob->size = (UINT32)bytes_count;
	blob->data = PTR_ADD_OFFSET (blob, sizeof (FWP_BYTE_BLOB));

	RtlCopyMemory (blob->data, data, bytes_count);

	*byte_blob = blob;
}

VOID ByteBlobFree (_Inout_ FWP_BYTE_BLOB** byte_blob)
{
	FWP_BYTE_BLOB* original_blob = *byte_blob;

	if (original_blob)
	{
		*byte_blob = NULL;

		_r_mem_free (original_blob);
	}
}
