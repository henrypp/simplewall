// simplewall
// Copyright (c) 2016-2020 Henry++

#include "global.hpp"

BOOLEAN _wfp_isfiltersapplying ()
{
	return _r_fastlock_islocked (&lock_apply) || _r_fastlock_islocked (&lock_transaction);
}

ENUM_INSTALL_TYPE _wfp_isproviderinstalled (HANDLE hengine)
{
	// check for persistent provider
	HKEY hkey;

	if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\services\\BFE\\Parameters\\Policy\\Persistent\\Provider", 0, KEY_READ, &hkey) == ERROR_SUCCESS)
	{
		static PR_STRING guidString = _r_str_fromguid ((LPGUID)&GUID_WfpProvider);

		if (guidString)
		{
			if (RegQueryValueEx (hkey, guidString->Buffer, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
			{
				RegCloseKey (hkey);
				return InstallEnabled;
			}
		}

		RegCloseKey (hkey);
	}

	// check for temporary provider
	FWPM_PROVIDER *ptr_provider;

	if (FwpmProviderGetByKey (hengine, &GUID_WfpProvider, &ptr_provider) == ERROR_SUCCESS)
	{
		FwpmFreeMemory ((PVOID*)&ptr_provider);

		return InstallEnabledTemporary;
	}

	return InstallDisabled;
}

ENUM_INSTALL_TYPE _wfp_issublayerinstalled (HANDLE hengine)
{
	// check for persistent sublayer
	HKEY hkey;

	if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\services\\BFE\\Parameters\\Policy\\Persistent\\SubLayer", 0, KEY_READ, &hkey) == ERROR_SUCCESS)
	{
		static PR_STRING guidString = _r_str_fromguid ((LPGUID)&GUID_WfpSublayer);

		if (guidString)
		{
			if (RegQueryValueEx (hkey, guidString->Buffer, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
			{
				RegCloseKey (hkey);
				return InstallEnabled;
			}
		}

		RegCloseKey (hkey);
	}

	// check for temporary sublayer
	FWPM_SUBLAYER *ptr_sublayer;

	if (FwpmSubLayerGetByKey (hengine, &GUID_WfpSublayer, &ptr_sublayer) == ERROR_SUCCESS)
	{
		FwpmFreeMemory ((PVOID*)&ptr_sublayer);

		return InstallEnabledTemporary;
	}

	return InstallDisabled;
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
	static HANDLE hengine = NULL;

	HANDLE currentHandle = InterlockedCompareExchangePointer (&hengine, NULL, NULL);

	if (!currentHandle)
	{
		FWPM_SESSION session;
		RtlSecureZeroMemory (&session, sizeof (session));

		session.displayData.name = APP_NAME;
		session.displayData.description = APP_NAME;

		session.txnWaitTimeoutInMSec = TRANSACTION_TIMEOUT;

		HANDLE newHandle;
		DWORD code = FwpmEngineOpen (NULL, RPC_C_AUTHN_WINNT, NULL, &session, &newHandle);

		if (code != ERROR_SUCCESS)
		{
			_r_logerror (UID, L"FwpmEngineOpen", code, NULL);
		}
		else
		{
			currentHandle = InterlockedCompareExchangePointer (&hengine, newHandle, NULL);

			if (!currentHandle)
			{
				currentHandle = newHandle;
			}
			else
			{
				FwpmEngineClose (newHandle);
			}
		}
	}

	return currentHandle;
}

BOOLEAN _wfp_initialize (HANDLE hengine, BOOLEAN is_full)
{
	BOOLEAN result;
	DWORD code;

	_r_fastlock_acquireshared (&lock_transaction);

	if (hengine)
	{
		result = TRUE; // already initialized
	}
	else
	{
		hengine = _wfp_getenginehandle ();

		if (!hengine)
		{
			result = FALSE;

			goto CleanupExit;
		}

		result = TRUE;
	}

	_app_setsecurityinfoforengine (hengine);

	// install engine provider and it's sublayer
	BOOLEAN is_providerexist = (_wfp_isproviderinstalled (hengine) != InstallDisabled);
	BOOLEAN is_sublayerexist = (_wfp_issublayerinstalled (hengine) != InstallDisabled);

	if (is_full)
	{
		if (!is_providerexist || !is_sublayerexist)
		{
			BOOLEAN is_intransact = _wfp_transact_start (hengine, __LINE__);

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

					_r_logerror (UID, L"FwpmProviderAdd", code, NULL);
					result = FALSE;

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

					_r_logerror (UID, L"FwpmSubLayerAdd", code, NULL);
					result = FALSE;

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
					result = FALSE;
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
				_r_logerror (0, L"FwpmEngineSetOption", code, L"FWPM_ENGINE_COLLECT_NET_EVENTS");
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
						_r_logerror (0, L"FwpmEngineSetOption", code, L"FWPM_ENGINE_NET_EVENT_MATCH_ANY_KEYWORDS");

					// enables the connection monitoring feature and starts logging creation and deletion events (and notifying any subscribers)
					if (_r_config_getboolean (L"IsMonitorIPSecConnections", TRUE))
					{
						val.type = FWP_UINT32;
						val.uint32 = 1;

						code = FwpmEngineSetOption (hengine, FWPM_ENGINE_MONITOR_IPSEC_CONNECTIONS, &val);

						if (code != ERROR_SUCCESS)
							_r_logerror (0, L"FwpmEngineSetOption", code, L"FWPM_ENGINE_MONITOR_IPSEC_CONNECTIONS");
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
				_r_logerror (0, L"FwpmEngineSetOption", code, L"FWPM_ENGINE_PACKET_QUEUING");
		}
	}

CleanupExit:

	_r_fastlock_releaseshared (&lock_transaction);

	return result;
}

VOID _wfp_uninitialize (HANDLE hengine, BOOLEAN is_full)
{
	_r_fastlock_acquireshared (&lock_transaction);

	DWORD code;

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
			_r_logerror (UID, L"FwpmSubLayerDeleteByKey", code, NULL);

		// destroy provider
		code = FwpmProviderDeleteByKey (hengine, &GUID_WfpProvider);

		if (code != ERROR_SUCCESS && code != FWP_E_PROVIDER_NOT_FOUND)
			_r_logerror (UID, L"FwpmProviderDeleteByKey", code, NULL);

		if (is_intransact)
			_wfp_transact_commit (hengine, __LINE__);
	}

	_r_fastlock_releaseshared (&lock_transaction);
}

VOID _wfp_installfilters (HANDLE hengine)
{
	// set security information
	_app_setsecurityinfoforprovider (hengine, &GUID_WfpProvider, FALSE);
	_app_setsecurityinfoforsublayer (hengine, &GUID_WfpSublayer, FALSE);

	_wfp_clearfilter_ids ();

	_r_fastlock_acquireshared (&lock_transaction);

	// dump all filters into array
	GUIDS_VEC filter_all;
	SIZE_T filters_count = _wfp_dumpfilters (hengine, &GUID_WfpProvider, &filter_all);

	// restore filters security
	if (filters_count)
	{
		for (auto it = filter_all.begin (); it != filter_all.end (); ++it)
			_app_setsecurityinfoforfilter (hengine, &(*it), FALSE, __LINE__);
	}

	BOOLEAN is_intransact = _wfp_transact_start (hengine, __LINE__);

	// destroy all filters
	if (filters_count)
	{
		for (auto it = filter_all.begin (); it != filter_all.end (); ++it)
			_wfp_deletefilter (hengine, &(*it));
	}

	// apply internal rules
	_wfp_create2filters (hengine, __LINE__, is_intransact);

	// apply apps rules
	{
		OBJECTS_APP_VECTOR rules;

		for (auto it = apps.begin (); it != apps.end (); ++it)
		{
			if (!it->second)
				continue;

			PITEM_APP ptr_app = (PITEM_APP)_r_obj_reference (it->second);

			if (ptr_app->is_enabled)
			{
				rules.push_back (ptr_app);
			}
			else
			{
				_r_obj_dereference (ptr_app);
			}
		}

		if (!rules.empty ())
		{
			_wfp_create3filters (hengine, &rules, __LINE__, is_intransact);
			_app_freeapps_vec (&rules);
		}
	}

	// apply blocklist/system/user rules
	{
		OBJECTS_RULE_VECTOR rules;

		for (auto it = rules_arr.begin (); it != rules_arr.end (); ++it)
		{
			if (!*it)
				continue;

			PITEM_RULE ptr_rule = (PITEM_RULE)_r_obj_reference (*it);

			if (ptr_rule->is_enabled)
			{
				rules.push_back (ptr_rule);
			}
			else
			{
				_r_obj_dereference (ptr_rule);
			}
		}

		if (!rules.empty ())
		{
			_wfp_create4filters (hengine, &rules, __LINE__, is_intransact);
			_app_freerules_vec (&rules);
		}
	}

	if (is_intransact)
		_wfp_transact_commit (hengine, __LINE__);

	// secure filters
	BOOLEAN is_secure = _r_config_getboolean (L"IsSecureFilters", TRUE);

	if (is_secure)
	{
		if (_wfp_dumpfilters (hengine, &GUID_WfpProvider, &filter_all))
		{
			for (auto it = filter_all.begin (); it != filter_all.end (); ++it)
				_app_setsecurityinfoforfilter (hengine, &(*it), is_secure, __LINE__);
		}
	}

	_app_setsecurityinfoforprovider (hengine, &GUID_WfpProvider, is_secure);
	_app_setsecurityinfoforsublayer (hengine, &GUID_WfpSublayer, is_secure);

	_r_fastlock_releaseshared (&lock_transaction);
}

BOOLEAN _wfp_transact_start (HANDLE hengine, UINT line)
{
	DWORD code = FwpmTransactionBegin (hengine, 0);

	//if (code == FWP_E_TXN_IN_PROGRESS)
	//	return FALSE;

	if (code != ERROR_SUCCESS)
	{
		_r_logerror_v (UID, L"FwpmTransactionBegin", code, L"#%" TEXT (PRIu32), line);
		return FALSE;
	}

	return TRUE;
}

BOOLEAN _wfp_transact_commit (HANDLE hengine, UINT line)
{
	DWORD code = FwpmTransactionCommit (hengine);

	if (code != ERROR_SUCCESS)
	{
		FwpmTransactionAbort (hengine);

		_r_logerror_v (UID, L"FwpmTransactionCommit", code, L"#%" TEXT (PRIu32), line);
		return FALSE;

	}

	return TRUE;
}

BOOLEAN _wfp_deletefilter (HANDLE hengine, LPCGUID pfilter_id)
{
	DWORD code = FwpmFilterDeleteByKey (hengine, pfilter_id);

	if (code != ERROR_SUCCESS && code != FWP_E_FILTER_NOT_FOUND)
	{
		PR_STRING guidString = _r_str_fromguid ((LPGUID)pfilter_id);

		_r_logerror (UID, L"FwpmFilterDeleteByKey", code, _r_obj_getstringordefault (guidString, SZ_EMPTY));

		if (guidString)
			_r_obj_dereference (guidString);

		return FALSE;
	}

	return TRUE;
}

DWORD _wfp_createfilter (HANDLE hengine, LPCWSTR name, FWPM_FILTER_CONDITION* lpcond, UINT32 count, UINT8 weight, LPCGUID layer_id, LPCGUID callout_id, FWP_ACTION_TYPE action, UINT32 flags, GUIDS_VEC* guids)
{
	FWPM_FILTER filter = {0};

	ULONG code;
	UINT64 filter_id;

	filter.displayData.name = APP_NAME;
	filter.displayData.description = name ? name : SZ_EMPTY;

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

	filter.providerKey = (GUID*)&GUID_WfpProvider;
	filter.subLayerKey = GUID_WfpSublayer;
	CoCreateGuid (&filter.filterKey); // set filter guid

	if (count)
	{
		filter.numFilterConditions = count;
		filter.filterCondition = lpcond;
	}

	if (layer_id)
		RtlCopyMemory (&filter.layerKey, layer_id, sizeof (GUID));

	if (callout_id)
		RtlCopyMemory (&filter.action.calloutKey, callout_id, sizeof (GUID));

	filter.weight.type = FWP_UINT8;
	filter.weight.uint8 = weight;

	filter.action.type = action;

	code = FwpmFilterAdd (hengine, &filter, NULL, &filter_id);

	if (code == ERROR_SUCCESS)
	{
		if (guids)
			guids->push_back (filter.filterKey);
	}
	else
	{
		_r_logerror (UID, L"FwpmFilterAdd", code, filter.displayData.description);
	}

	return code;
}

VOID _wfp_clearfilter_ids ()
{
	// clear common filters
	filter_ids.clear ();

	// clear apps filters
	for (auto it = apps.begin (); it != apps.end (); ++it)
	{
		if (!it->second)
			continue;

		PITEM_APP ptr_app = (PITEM_APP)_r_obj_reference (it->second);

		ptr_app->is_haveerrors = FALSE;
		ptr_app->guids->clear ();

		_r_obj_dereference (ptr_app);
	}

	// clear rules filters
	for (auto it = rules_arr.begin (); it != rules_arr.end (); ++it)
	{
		if (!*it)
			continue;

		PITEM_RULE ptr_rule = (PITEM_RULE)_r_obj_reference (*it);

		ptr_rule->is_haveerrors = FALSE;
		ptr_rule->guids->clear ();

		_r_obj_dereference (ptr_rule);
	}
}

VOID _wfp_destroyfilters (HANDLE hengine)
{
	_wfp_clearfilter_ids ();

	// destroy all filters
	GUIDS_VEC filter_all;

	_r_fastlock_acquireshared (&lock_transaction);

	if (_wfp_dumpfilters (hengine, &GUID_WfpProvider, &filter_all))
		_wfp_destroyfilters_array (hengine, &filter_all, __LINE__);

	_r_fastlock_releaseshared (&lock_transaction);
}

BOOLEAN _wfp_destroyfilters_array (HANDLE hengine, GUIDS_VEC* ptr_filters, UINT line)
{
	if (ptr_filters->empty ())
		return FALSE;

	BOOLEAN is_enabled = _app_initinterfacestate (_r_app_gethwnd (), FALSE);

	_r_fastlock_acquireshared (&lock_transaction);

	for (auto it = ptr_filters->begin (); it != ptr_filters->end (); ++it)
		_app_setsecurityinfoforfilter (hengine, &(*it), FALSE, line);

	BOOLEAN is_intransact = _wfp_transact_start (hengine, line);

	for (auto it = ptr_filters->begin (); it != ptr_filters->end (); ++it)
		_wfp_deletefilter (hengine, &(*it));

	ptr_filters->clear ();

	if (is_intransact)
		_wfp_transact_commit (hengine, line);

	_r_fastlock_releaseshared (&lock_transaction);

	_app_restoreinterfacestate (_r_app_gethwnd (), is_enabled);

	return TRUE;
}

BOOLEAN _wfp_createrulefilter (HANDLE hengine, LPCWSTR name, SIZE_T app_hash, PR_STRING rule_remote, PR_STRING rule_local, UINT8 protocol, ADDRESS_FAMILY af, FWP_DIRECTION dir, UINT8 weight, FWP_ACTION_TYPE action, UINT32 flag, GUIDS_VEC* guids)
{
	UINT32 count = 0;
	FWPM_FILTER_CONDITION fwfc[8] = {0};

	ITEM_ADDRESS addr;

	FWP_BYTE_BLOB* pblob = NULL;
	PITEM_APP ptr_app = NULL;
	BOOLEAN is_remoteaddr_set = FALSE;
	BOOLEAN is_remoteport_set = FALSE;
	BOOLEAN result = FALSE;

	RtlSecureZeroMemory (&addr, sizeof (addr));

	// set path condition
	if (app_hash)
	{
		ptr_app = _app_getappitem (app_hash);

		if (!ptr_app)
		{
			_r_logerror_v (0, TEXT (__FUNCTION__), 0, L"App \"%" TEXT (PR_SIZE_T) L"\" not found!", app_hash);

			goto CleanupExit;
		}

		if (ptr_app->type == DataAppService) // windows service
		{
			if (ptr_app->pbytes)
			{
				ByteBlobAlloc (ptr_app->pbytes->Buffer, RtlLengthSecurityDescriptor (ptr_app->pbytes->Buffer), &pblob);

				fwfc[count].fieldKey = FWPM_CONDITION_ALE_USER_ID;
				fwfc[count].matchType = FWP_MATCH_EQUAL;
				fwfc[count].conditionValue.type = FWP_SECURITY_DESCRIPTOR_TYPE;
				fwfc[count].conditionValue.sd = pblob;

				count += 1;
			}
			else
			{
				_r_logerror (0, TEXT (__FUNCTION__), 0, _app_getdisplayname (app_hash, ptr_app, TRUE));

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
				fwfc[count].conditionValue.sid = (SID*)ptr_app->pbytes->Buffer;

				count += 1;
			}
			else
			{
				_r_logerror (0, TEXT (__FUNCTION__), 0, _app_getdisplayname (app_hash, ptr_app, TRUE));

				goto CleanupExit;
			}
		}
		else
		{
			LPCWSTR path = _r_obj_getstring (ptr_app->original_path);
			DWORD code = _FwpmGetAppIdFromFileName1 (path, &pblob, ptr_app->type);

			if (code == ERROR_SUCCESS)
			{
				fwfc[count].fieldKey = FWPM_CONDITION_ALE_APP_ID;
				fwfc[count].matchType = FWP_MATCH_EQUAL;
				fwfc[count].conditionValue.type = FWP_BYTE_BLOB_TYPE;
				fwfc[count].conditionValue.byteBlob = pblob;

				count += 1;
			}
			else
			{
				// do not log file not found to error log
				if (code != ERROR_FILE_NOT_FOUND && code != ERROR_PATH_NOT_FOUND)
					_r_logerror (0, L"FwpmGetAppIdFromFileName", code, path);

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
			if (!_r_str_isempty (rules[i]))
			{
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
							PR_STRING hostPart;
							R_STRINGREF remainingPart;

							_r_stringref_initialize (&remainingPart, addr.host);

							while (remainingPart.Length != 0)
							{
								hostPart = _r_str_splitatchar (&remainingPart, &remainingPart, DIVIDER_RULE[0]);

								if (hostPart)
								{
									if (!_wfp_createrulefilter (hengine, name, app_hash, hostPart, NULL, protocol, af, dir, weight, action, flag, guids))
									{
										_r_obj_dereference (hostPart);

										goto CleanupExit;
									}

									_r_obj_dereference (hostPart);
								}
							}

							result = TRUE;

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
			_wfp_createfilter (hengine, name, fwfc, count, weight, &FWPM_LAYER_ALE_AUTH_CONNECT_V4, NULL, action, flag, guids);

			// win7+
			_wfp_createfilter (hengine, name, fwfc, count, weight, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V4, NULL, action, flag, guids);
		}

		if (af == AF_INET6 || af == AF_UNSPEC)
		{
			_wfp_createfilter (hengine, name, fwfc, count, weight, &FWPM_LAYER_ALE_AUTH_CONNECT_V6, NULL, action, flag, guids);

			// win7+
			_wfp_createfilter (hengine, name, fwfc, count, weight, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V6, NULL, action, flag, guids);
		}
	}

	// create inbound layer filter
	if (dir == FWP_DIRECTION_INBOUND || dir == FWP_DIRECTION_MAX)
	{
		if (af == AF_INET || af == AF_UNSPEC)
		{
			_wfp_createfilter (hengine, name, fwfc, count, weight, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, NULL, action, flag, guids);

			if (action == FWP_ACTION_BLOCK && (!is_remoteaddr_set && !is_remoteport_set))
				_wfp_createfilter (hengine, name, fwfc, count, weight, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V4, NULL, FWP_ACTION_PERMIT, flag, guids);
		}

		if (af == AF_INET6 || af == AF_UNSPEC)
		{
			_wfp_createfilter (hengine, name, fwfc, count, weight, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, action, flag, guids);

			if (action == FWP_ACTION_BLOCK && (!is_remoteaddr_set && !is_remoteport_set))
				_wfp_createfilter (hengine, name, fwfc, count, weight, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V6, NULL, FWP_ACTION_PERMIT, flag, guids);
		}
	}

	result = TRUE;

CleanupExit:

	if (ptr_app)
		_r_obj_dereference (ptr_app);

	if (pblob)
		ByteBlobFree (&pblob);

	return TRUE;
}

BOOLEAN _wfp_create4filters (HANDLE hengine, OBJECTS_RULE_VECTOR* ptr_rules, UINT line, BOOLEAN is_intransact)
{
	if (ptr_rules->empty ())
		return FALSE;

	BOOLEAN is_enabled = _app_initinterfacestate (_r_app_gethwnd (), FALSE);

	if (!is_intransact && _wfp_isfiltersapplying ())
		is_intransact = TRUE;

	GUIDS_VEC guids;

	if (!is_intransact)
	{
		for (auto it = ptr_rules->begin (); it != ptr_rules->end (); ++it)
		{
			if (!*it)
				continue;

			PITEM_RULE ptr_rule = (PITEM_RULE)_r_obj_reference (*it);

			if (!ptr_rule->guids->empty ())
			{
				guids.insert (guids.end (), ptr_rule->guids->begin (), ptr_rule->guids->end ());
				ptr_rule->guids->clear ();
			}

			_r_obj_dereference (ptr_rule);
		}

		for (auto it = guids.begin (); it != guids.end (); ++it)
			_app_setsecurityinfoforfilter (hengine, &(*it), FALSE, line);

		_r_fastlock_acquireshared (&lock_transaction);
		is_intransact = !_wfp_transact_start (hengine, line);
	}

	for (auto it = guids.begin (); it != guids.end (); ++it)
		_wfp_deletefilter (hengine, &(*it));

	PITEM_RULE ptr_rule;
	R_STRINGREF remoteRemainingPart;
	R_STRINGREF localRemainingPart;
	PR_STRING ruleRemoteString;
	PR_STRING ruleLocalString;
	LPCWSTR ruleNamePtr;

	for (auto it = ptr_rules->begin (); it != ptr_rules->end (); ++it)
	{
		if (!*it)
			continue;

		ptr_rule = (PITEM_RULE)_r_obj_reference (*it);

		ptr_rule->is_haveerrors = FALSE;
		ptr_rule->guids->clear ();

		if (!ptr_rule->is_enabled)
		{
			_r_obj_dereference (ptr_rule);
			continue;
		}

		ruleNamePtr = _r_obj_getstring (ptr_rule->name);

		ruleRemoteString = NULL;
		ruleLocalString = NULL;

		if (!_r_str_isempty (ptr_rule->rule_remote))
			ruleRemoteString = _r_str_splitatchar (&ptr_rule->rule_remote->sr, &remoteRemainingPart, DIVIDER_RULE[0]);
		else
			_r_obj_initializeemptystringref (&remoteRemainingPart);

		if (!_r_str_isempty (ptr_rule->rule_local))
			ruleLocalString = _r_str_splitatchar (&ptr_rule->rule_local->sr, &localRemainingPart, DIVIDER_RULE[0]);
		else
			_r_obj_initializeemptystringref (&localRemainingPart);

		while (TRUE)
		{
			// apply rules for services hosts
			if (ptr_rule->is_forservices)
			{
				if (!_wfp_createrulefilter (hengine, ruleNamePtr, config.ntoskrnl_hash, ruleRemoteString, ruleLocalString, ptr_rule->protocol, ptr_rule->af, ptr_rule->direction, ptr_rule->weight, ptr_rule->is_block ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT, 0, ptr_rule->guids))
					ptr_rule->is_haveerrors = TRUE;

				if (!_wfp_createrulefilter (hengine, ruleNamePtr, config.svchost_hash, ruleRemoteString, ruleLocalString, ptr_rule->protocol, ptr_rule->af, ptr_rule->direction, ptr_rule->weight, ptr_rule->is_block ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT, 0, ptr_rule->guids))
					ptr_rule->is_haveerrors = TRUE;
			}

			if (!ptr_rule->apps->empty ())
			{
				for (auto it2 = ptr_rule->apps->begin (); it2 != ptr_rule->apps->end (); ++it2)
				{
					if (ptr_rule->is_forservices && (it2->first == config.ntoskrnl_hash || it2->first == config.svchost_hash))
						continue;

					if (!_wfp_createrulefilter (hengine, ruleNamePtr, it2->first, ruleRemoteString, ruleLocalString, ptr_rule->protocol, ptr_rule->af, ptr_rule->direction, ptr_rule->weight, ptr_rule->is_block ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT, 0, ptr_rule->guids))
						ptr_rule->is_haveerrors = TRUE;
				}
			}
			else
			{
				if (!_wfp_createrulefilter (hengine, ruleNamePtr, 0, ruleRemoteString, ruleLocalString, ptr_rule->protocol, ptr_rule->af, ptr_rule->direction, ptr_rule->weight, ptr_rule->is_block ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT, 0, ptr_rule->guids))
					ptr_rule->is_haveerrors = TRUE;
			}

			if (remoteRemainingPart.Length == 0 && localRemainingPart.Length == 0)
				break;

			if (remoteRemainingPart.Length != 0)
			{
				_r_obj_movereference (&ruleRemoteString, _r_str_splitatchar (&remoteRemainingPart, &remoteRemainingPart, DIVIDER_RULE[0]));
			}

			if (localRemainingPart.Length != 0)
			{
				_r_obj_movereference (&ruleLocalString, _r_str_splitatchar (&localRemainingPart, &localRemainingPart, DIVIDER_RULE[0]));
			}
		}

		SAFE_DELETE_REFERENCE (ruleRemoteString);
		SAFE_DELETE_REFERENCE (ruleLocalString);

		_r_obj_dereference (ptr_rule);
	}

	if (!is_intransact)
	{
		_wfp_transact_commit (hengine, line);

		BOOLEAN is_secure = _r_config_getboolean (L"IsSecureFilters", TRUE);

		if (is_secure)
		{
			for (auto it = ptr_rules->begin (); it != ptr_rules->end (); ++it)
			{
				if (!*it)
					continue;

				ptr_rule = (PITEM_RULE)_r_obj_reference (*it);

				if (ptr_rule->is_enabled)
				{
					for (auto it2 = ptr_rule->guids->begin (); it2 != ptr_rule->guids->end (); ++it2)
						_app_setsecurityinfoforfilter (hengine, &(*it2), is_secure, line);
				}

				_r_obj_dereference (ptr_rule);
			}
		}

		_r_fastlock_releaseshared (&lock_transaction);
	}

	_app_restoreinterfacestate (_r_app_gethwnd (), is_enabled);

	return TRUE;
}

BOOLEAN _wfp_create3filters (HANDLE hengine, OBJECTS_APP_VECTOR* ptr_apps, UINT line, BOOLEAN is_intransact)
{
	if (ptr_apps->empty ())
		return FALSE;

	BOOLEAN is_enabled = _app_initinterfacestate (_r_app_gethwnd (), FALSE);
	FWP_ACTION_TYPE action = FWP_ACTION_PERMIT;

	if (!is_intransact && _wfp_isfiltersapplying ())
		is_intransact = TRUE;

	GUIDS_VEC guids;

	if (!is_intransact)
	{
		for (auto it = ptr_apps->begin (); it != ptr_apps->end (); ++it)
		{
			if (!*it)
				continue;

			PITEM_APP ptr_app = (PITEM_APP)_r_obj_reference (*it);

			if (!ptr_app->guids->empty ())
			{
				guids.insert (guids.end (), ptr_app->guids->begin (), ptr_app->guids->end ());
				ptr_app->guids->clear ();
			}

			_r_obj_dereference (ptr_app);
		}

		for (auto it = guids.begin (); it != guids.end (); ++it)
			_app_setsecurityinfoforfilter (hengine, &(*it), FALSE, line);

		_r_fastlock_acquireshared (&lock_transaction);
		is_intransact = !_wfp_transact_start (hengine, line);
	}

	for (auto it = guids.begin (); it != guids.end (); ++it)
		_wfp_deletefilter (hengine, &(*it));

	for (auto it = ptr_apps->begin (); it != ptr_apps->end (); ++it)
	{
		if (!*it)
			continue;

		PITEM_APP ptr_app = (PITEM_APP)_r_obj_reference (*it);

		if (ptr_app->is_enabled)
		{
			if (!_wfp_createrulefilter (hengine, _r_obj_getstring (ptr_app->display_name), _r_str_hash (ptr_app->original_path), NULL, NULL, 0, AF_UNSPEC, FWP_DIRECTION_MAX, FILTER_WEIGHT_APPLICATION, action, 0, ptr_app->guids))
				ptr_app->is_haveerrors = TRUE;
		}

		_r_obj_dereference (ptr_app);
	}

	if (!is_intransact)
	{
		_wfp_transact_commit (hengine, line);

		BOOLEAN is_secure = _r_config_getboolean (L"IsSecureFilters", TRUE);

		if (is_secure)
		{
			for (auto it = ptr_apps->begin (); it != ptr_apps->end (); ++it)
			{
				if (!*it)
					continue;

				PITEM_APP ptr_app = (PITEM_APP)_r_obj_reference (*it);

				for (auto it2 = ptr_app->guids->begin (); it2 != ptr_app->guids->end (); ++it2)
					_app_setsecurityinfoforfilter (hengine, &(*it2), is_secure, line);

				_r_obj_dereference (ptr_app);
			}
		}

		_r_fastlock_releaseshared (&lock_transaction);
	}

	_app_restoreinterfacestate (_r_app_gethwnd (), is_enabled);

	return TRUE;
}

BOOLEAN _wfp_create2filters (HANDLE hengine, UINT line, BOOLEAN is_intransact)
{
	BOOLEAN is_enabled = _app_initinterfacestate (_r_app_gethwnd (), FALSE);

	if (!is_intransact && _wfp_isfiltersapplying ())
		is_intransact = TRUE;

	if (!is_intransact)
	{
		if (!filter_ids.empty ())
		{
			for (auto it = filter_ids.begin (); it != filter_ids.end (); ++it)
				_app_setsecurityinfoforfilter (hengine, &(*it), FALSE, line);
		}

		_r_fastlock_acquireshared (&lock_transaction);
		is_intransact = !_wfp_transact_start (hengine, line);
	}

	if (!filter_ids.empty ())
	{
		for (auto it = filter_ids.begin (); it != filter_ids.end (); ++it)
			_wfp_deletefilter (hengine, &(*it));

		filter_ids.clear ();
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

		_wfp_createfilter (hengine, NULL, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_CONNECT_V4, NULL, FWP_ACTION_PERMIT, 0, &filter_ids);
		_wfp_createfilter (hengine, NULL, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_CONNECT_V6, NULL, FWP_ACTION_PERMIT, 0, &filter_ids);

		// win7+
		_wfp_createfilter (hengine, NULL, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V4, NULL, FWP_ACTION_PERMIT, 0, &filter_ids);
		_wfp_createfilter (hengine, NULL, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V6, NULL, FWP_ACTION_PERMIT, 0, &filter_ids);

		_wfp_createfilter (hengine, NULL, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, NULL, FWP_ACTION_PERMIT, 0, &filter_ids);
		_wfp_createfilter (hengine, NULL, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, FWP_ACTION_PERMIT, 0, &filter_ids);

		_wfp_createfilter (hengine, NULL, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V4, NULL, FWP_ACTION_PERMIT, 0, &filter_ids);
		_wfp_createfilter (hengine, NULL, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V6, NULL, FWP_ACTION_PERMIT, 0, &filter_ids);

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
		PR_STRING ruleString;

		for (SIZE_T i = 0; i < RTL_NUMBER_OF (ip_list); i++)
		{
			RtlSecureZeroMemory (&addr, sizeof (addr));

			ruleString = _r_obj_createstring (ip_list[i]);

			if (_app_parserulestring (ruleString, &addr))
			{
				fwfc[1].matchType = FWP_MATCH_EQUAL;

				if (addr.format == NET_ADDRESS_IPV4)
				{
					fwfc[1].conditionValue.type = FWP_V4_ADDR_MASK;
					fwfc[1].conditionValue.v4AddrMask = &addr.addr4;

					fwfc[1].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
					_wfp_createfilter (hengine, NULL, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_CONNECT_V4, NULL, FWP_ACTION_PERMIT, 0, &filter_ids);

					// win7+
					_wfp_createfilter (hengine, NULL, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V4, NULL, FWP_ACTION_PERMIT, 0, &filter_ids);

					fwfc[1].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;
					_wfp_createfilter (hengine, NULL, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, NULL, FWP_ACTION_PERMIT, 0, &filter_ids);
					_wfp_createfilter (hengine, NULL, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V4, NULL, FWP_ACTION_PERMIT, 0, &filter_ids);
				}
				else if (addr.format == NET_ADDRESS_IPV6)
				{
					fwfc[1].conditionValue.type = FWP_V6_ADDR_MASK;
					fwfc[1].conditionValue.v6AddrMask = &addr.addr6;

					fwfc[1].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
					_wfp_createfilter (hengine, NULL, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_CONNECT_V6, NULL, FWP_ACTION_PERMIT, 0, &filter_ids);

					// win7+
					_wfp_createfilter (hengine, NULL, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V6, NULL, FWP_ACTION_PERMIT, 0, &filter_ids);

					fwfc[1].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;
					_wfp_createfilter (hengine, NULL, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, FWP_ACTION_PERMIT, 0, &filter_ids);
					_wfp_createfilter (hengine, NULL, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V6, NULL, FWP_ACTION_PERMIT, 0, &filter_ids);
				}
			}

			_r_obj_dereference (ruleString);
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

		_wfp_createfilter (hengine, L"Allow6to4", fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, NULL, FWP_ACTION_PERMIT, 0, &filter_ids);

		// allows icmpv6 router solicitation messages, which are required for the ipv6 stack to work properly
		fwfc[0].fieldKey = FWPM_CONDITION_ICMP_TYPE;
		fwfc[0].matchType = FWP_MATCH_EQUAL;
		fwfc[0].conditionValue.type = FWP_UINT16;
		fwfc[0].conditionValue.uint16 = 0x85;

		_wfp_createfilter (hengine, L"AllowIcmpV6Type133", fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, FWP_ACTION_PERMIT, 0, &filter_ids);

		// allows icmpv6 router advertise messages, which are required for the ipv6 stack to work properly
		fwfc[0].conditionValue.uint16 = 0x86;
		_wfp_createfilter (hengine, L"AllowIcmpV6Type134", fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, FWP_ACTION_PERMIT, 0, &filter_ids);

		// allows icmpv6 neighbor solicitation messages, which are required for the ipv6 stack to work properly
		fwfc[0].conditionValue.uint16 = 0x87;
		_wfp_createfilter (hengine, L"AllowIcmpV6Type135", fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, FWP_ACTION_PERMIT, 0, &filter_ids);

		// allows icmpv6 neighbor advertise messages, which are required for the ipv6 stack to work properly
		fwfc[0].conditionValue.uint16 = 0x88;
		_wfp_createfilter (hengine, L"AllowIcmpV6Type136", fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, FWP_ACTION_PERMIT, 0, &filter_ids);
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

		_wfp_createfilter (hengine, L"BlockIcmpErrorV4", fwfc, 2, FILTER_WEIGHT_HIGHEST, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V4, NULL, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, &filter_ids);
		_wfp_createfilter (hengine, L"BlockIcmpErrorV6", fwfc, 2, FILTER_WEIGHT_HIGHEST, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V6, NULL, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, &filter_ids);

		// blocks tcp port scanners (exclude loopback)
		fwfc[0].conditionValue.uint32 |= FWP_CONDITION_FLAG_IS_IPSEC_SECURED;

		_wfp_createfilter (hengine, L"BlockTcpRstOnCloseV4", fwfc, 1, FILTER_WEIGHT_HIGHEST, &FWPM_LAYER_INBOUND_TRANSPORT_V4_DISCARD, &FWPM_CALLOUT_WFP_TRANSPORT_LAYER_V4_SILENT_DROP, FWP_ACTION_CALLOUT_TERMINATING, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, &filter_ids);
		_wfp_createfilter (hengine, L"BlockTcpRstOnCloseV6", fwfc, 1, FILTER_WEIGHT_HIGHEST, &FWPM_LAYER_INBOUND_TRANSPORT_V6_DISCARD, &FWPM_CALLOUT_WFP_TRANSPORT_LAYER_V6_SILENT_DROP, FWP_ACTION_CALLOUT_TERMINATING, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, &filter_ids);
	}

	// configure outbound layer
	{
		FWP_ACTION_TYPE action = _r_config_getboolean (L"BlockOutboundConnections", TRUE) ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT;

		_wfp_createfilter (hengine, L"BlockConnectionsV4", NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_CONNECT_V4, NULL, action, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, &filter_ids);
		_wfp_createfilter (hengine, L"BlockConnectionsV6", NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_CONNECT_V6, NULL, action, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, &filter_ids);

		// win7+
		_wfp_createfilter (hengine, L"BlockConnectionsRedirectionV4", NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V4, NULL, action, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, &filter_ids);
		_wfp_createfilter (hengine, L"BlockConnectionsRedirectionV6", NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V6, NULL, action, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, &filter_ids);
	}

	// configure inbound layer
	{
		FWP_ACTION_TYPE action = (_r_config_getboolean (L"UseStealthMode", TRUE) || _r_config_getboolean (L"BlockInboundConnections", TRUE)) ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT;

		_wfp_createfilter (hengine, L"BlockRecvAcceptConnectionsV4", NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, NULL, action, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, &filter_ids);
		_wfp_createfilter (hengine, L"BlockRecvAcceptConnectionsV6", NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, action, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, &filter_ids);

		//_wfp_createfilter (L"BlockResourceAssignmentV4", NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V4, NULL, action, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, &filter_ids);
		//_wfp_createfilter (L"BlockResourceAssignmentV6", NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V6, NULL, action, FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT, &filter_ids);
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

		_wfp_createfilter (hengine, BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_IPFORWARD_V4, NULL, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);
		_wfp_createfilter (hengine, BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_IPFORWARD_V6, NULL, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);

		_wfp_createfilter (hengine, BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, NULL, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);
		_wfp_createfilter (hengine, BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);

		_wfp_createfilter (hengine, BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_INBOUND_ICMP_ERROR_V4, NULL, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);
		_wfp_createfilter (hengine, BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_INBOUND_ICMP_ERROR_V6, NULL, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);

		_wfp_createfilter (hengine, BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V4, NULL, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);
		_wfp_createfilter (hengine, BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V6, NULL, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);

		_wfp_createfilter (hengine, BOOTTIME_FILTER_NAME, NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, NULL, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);
		_wfp_createfilter (hengine, BOOTTIME_FILTER_NAME, NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, NULL, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);

		_wfp_createfilter (hengine, BOOTTIME_FILTER_NAME, NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_INBOUND_ICMP_ERROR_V4, NULL, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);
		_wfp_createfilter (hengine, BOOTTIME_FILTER_NAME, NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_INBOUND_ICMP_ERROR_V6, NULL, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);

		_wfp_createfilter (hengine, BOOTTIME_FILTER_NAME, NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V4, NULL, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);
		_wfp_createfilter (hengine, BOOTTIME_FILTER_NAME, NULL, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V6, NULL, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);

		// win7+ boot-time features
		fwfc[0].fieldKey = FWPM_CONDITION_FLAGS;
		fwfc[0].matchType = FWP_MATCH_FLAGS_NONE_SET;
		fwfc[0].conditionValue.type = FWP_UINT32;
		fwfc[0].conditionValue.uint32 = FWP_CONDITION_FLAG_IS_OUTBOUND_PASS_THRU | FWP_CONDITION_FLAG_IS_INBOUND_PASS_THRU;

		_wfp_createfilter (hengine, BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_APPLICATION, &FWPM_LAYER_IPFORWARD_V4, NULL, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);
		_wfp_createfilter (hengine, BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_APPLICATION, &FWPM_LAYER_IPFORWARD_V6, NULL, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);
	}

	if (!is_intransact)
	{
		_wfp_transact_commit (hengine, line);

		BOOLEAN is_secure = _r_config_getboolean (L"IsSecureFilters", TRUE);

		if (is_secure)
		{
			for (auto it = filter_ids.begin (); it != filter_ids.end (); ++it)
				_app_setsecurityinfoforfilter (hengine, &(*it), is_secure, line);
		}

		_r_fastlock_releaseshared (&lock_transaction);
	}

	_app_restoreinterfacestate (_r_app_gethwnd (), is_enabled);

	return TRUE;
}

SIZE_T _wfp_dumpfilters (HANDLE hengine, LPCGUID pprovider_id, GUIDS_VEC* ptr_filters)
{
	ptr_filters->clear ();

	UINT32 count = 0;
	HANDLE henum = NULL;

	DWORD code = FwpmFilterCreateEnumHandle (hengine, NULL, &henum);

	if (code != ERROR_SUCCESS)
	{
		_r_logerror (0, L"FwpmFilterCreateEnumHandle", code, NULL);
		return 0;
	}
	else
	{
		FWPM_FILTER** matchingFwpFilter = NULL;

		code = FwpmFilterEnum (hengine, henum, UINT32_MAX, &matchingFwpFilter, &count);

		if (code != ERROR_SUCCESS)
		{
			_r_logerror (0, L"FwpmFilterEnum", code, NULL);
		}
		else
		{
			if (matchingFwpFilter)
			{
				for (UINT32 i = 0; i < count; i++)
				{
					FWPM_FILTER* pfilter = matchingFwpFilter[i];

					if (pfilter && pfilter->providerKey && RtlEqualMemory (pfilter->providerKey, pprovider_id, sizeof (GUID)))
						ptr_filters->push_back (pfilter->filterKey);
				}

				FwpmFreeMemory ((PVOID*)&matchingFwpFilter);
			}
			else
			{
				count = 0;
			}
		}
	}

	if (henum)
		FwpmFilterDestroyEnumHandle (hengine, henum);

	return count;
}

BOOLEAN _mps_firewallapi (PBOOLEAN pis_enabled, PBOOLEAN pis_enable)
{
	if (!pis_enabled && !pis_enable)
		return FALSE;

	BOOLEAN result = FALSE;

	INetFwPolicy2* pNetFwPolicy2 = NULL;
	HRESULT hr = CoCreateInstance (__uuidof (NetFwPolicy2), NULL, CLSCTX_INPROC_SERVER, __uuidof (INetFwPolicy2), (PVOID*)&pNetFwPolicy2);

	if (SUCCEEDED (hr) && pNetFwPolicy2)
	{
		NET_FW_PROFILE_TYPE2 profileTypes[] = {
			NET_FW_PROFILE2_DOMAIN,
			NET_FW_PROFILE2_PRIVATE,
			NET_FW_PROFILE2_PUBLIC
		};

		if (pis_enabled)
		{
			*pis_enabled = FALSE;

			for (auto type : profileTypes)
			{
				VARIANT_BOOL bIsEnabled = FALSE;

				hr = pNetFwPolicy2->get_FirewallEnabled (type, &bIsEnabled);

				if (SUCCEEDED (hr))
				{
					result = TRUE;

					if (bIsEnabled == VARIANT_TRUE)
					{
						*pis_enabled = TRUE;
						break;
					}
				}
			}
		}

		if (pis_enable)
		{
			for (auto type : profileTypes)
			{
				hr = pNetFwPolicy2->put_FirewallEnabled (type, *pis_enable ? VARIANT_TRUE : VARIANT_FALSE);

				if (SUCCEEDED (hr))
				{
					result = TRUE;
				}
				else
				{
					_r_logerror_v (0, L"put_FirewallEnabled", hr, L"%d", type);
				}
			}
		}
	}
	else
	{
		_r_logerror (0, L"CoCreateInstance", hr, L"INetFwPolicy2");
	}

	if (pNetFwPolicy2)
		pNetFwPolicy2->Release ();

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
		_r_logerror (0, L"OpenSCManager", GetLastError (), NULL);
	}
	else
	{
		LPCWSTR ServiceNames[] = {
			L"mpssvc",
			L"mpsdrv",
		};

		BOOLEAN is_started = FALSE;

		for (auto name : ServiceNames)
		{
			SC_HANDLE sc = OpenService (scm, name, SERVICE_CHANGE_CONFIG | SERVICE_QUERY_STATUS | SERVICE_STOP);

			if (!sc)
			{
				DWORD code = GetLastError ();

				if (code != ERROR_ACCESS_DENIED)
					_r_logerror (0, L"OpenService", code, name);
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
					_r_logerror (0, L"ChangeServiceConfig", GetLastError (), name);

				CloseServiceHandle (sc);
			}
		}

		// start services
		if (is_enable)
		{
			for (auto name : ServiceNames)
			{
				SC_HANDLE sc = OpenService (scm, name, SERVICE_QUERY_STATUS | SERVICE_START);

				if (!sc)
				{
					_r_logerror (0, L"OpenService", GetLastError (), name);
				}
				else
				{
					DWORD dwBytesNeeded = 0;
					SERVICE_STATUS_PROCESS ssp = {0};

					if (!QueryServiceStatusEx (sc, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof (ssp), &dwBytesNeeded))
					{
						_r_logerror (0, L"QueryServiceStatusEx", GetLastError (), name);
					}
					else
					{
						if (ssp.dwCurrentState != SERVICE_RUNNING)
						{
							if (!StartService (sc, 0, NULL))
								_r_logerror (0, L"StartService", GetLastError (), name);
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

DWORD _FwpmGetAppIdFromFileName1 (LPCWSTR path, FWP_BYTE_BLOB** lpblob, ENUM_TYPE_DATA type)
{
	DWORD code = ERROR_FILE_NOT_FOUND;

	PR_STRING originalPath = _r_obj_createstring (path);
	PR_STRING ntPath = NULL;

	if (type == DataAppRegular || type == DataAppNetwork || type == DataAppService)
	{
		if (_r_str_hash (originalPath) == config.ntoskrnl_hash)
		{
			ByteBlobAlloc (originalPath->Buffer, originalPath->Length + sizeof (UNICODE_NULL), lpblob);
			code = ERROR_SUCCESS;

			goto CleanupExit;
		}
		else
		{
			code = _r_path_ntpathfromdos (path, &ntPath);

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
					WCHAR pathRoot[128];
					WCHAR pathSkipRoot[512];

					// file path (root)
					_r_str_copy (pathRoot, RTL_NUMBER_OF (pathRoot), path);
					PathStripToRoot (pathRoot);

					// file path (without root)
					_r_str_copy (pathSkipRoot, RTL_NUMBER_OF (pathSkipRoot), PathSkipRoot (path));
					_r_str_tolower (pathSkipRoot); // lower is important!

					code = _r_path_ntpathfromdos (pathRoot, &ntPath);

					if (code != ERROR_SUCCESS)
						goto CleanupExit;

					_r_obj_movereference (&originalPath, _r_format_string (L"%s%s", _r_obj_getstring (ntPath), pathSkipRoot));
				}
			}
			else if (code == ERROR_SUCCESS)
			{
				_r_obj_movereference (&originalPath, ntPath);
				ntPath = NULL;
			}
			else
			{
				goto CleanupExit;

			}

			ByteBlobAlloc (_r_obj_getstring (originalPath), _r_obj_getstringsize (originalPath) + sizeof (UNICODE_NULL), lpblob);
		}
	}
	else if (type == DataAppPico || type == DataAppDevice)
	{
		if (type == DataAppDevice)
			_r_str_tolower (originalPath); // lower is important!

		ByteBlobAlloc (originalPath->Buffer, originalPath->Length + sizeof (UNICODE_NULL), lpblob);
		code = ERROR_SUCCESS;

		goto CleanupExit;
	}

CleanupExit:

	if (originalPath)
		_r_obj_dereference (originalPath);

	if (ntPath)
		_r_obj_dereference (ntPath);

	return code;
}

VOID ByteBlobAlloc (LPCVOID pdata, SIZE_T length, FWP_BYTE_BLOB** lpblob)
{
	FWP_BYTE_BLOB* pblob = (FWP_BYTE_BLOB*)_r_mem_allocatezero (sizeof (FWP_BYTE_BLOB));

	pblob->data = (UINT8*)_r_mem_allocatezero (length);
	pblob->size = (UINT32)length;

	RtlCopyMemory (pblob->data, pdata, length);

	*lpblob = pblob;
}

VOID ByteBlobFree (FWP_BYTE_BLOB** lpblob)
{
	if (lpblob && *lpblob)
	{
		FWP_BYTE_BLOB* pblob = *lpblob;

		if (pblob)
		{
			_r_mem_free (pblob->data);
			_r_mem_free (pblob);

			*lpblob = NULL;
		}
	}
}
