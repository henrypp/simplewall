// simplewall
// Copyright (c) 2016-2019 Henry++

#include "global.hpp"

bool _wfp_isfiltersapplying ()
{
	return _r_fastlock_islocked (&lock_apply) || _r_fastlock_islocked (&lock_transaction);
}

bool _wfp_isfiltersinstalled ()
{
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

bool _wfp_initialize (bool is_full)
{
	bool result = true;
	DWORD rc;

	_r_fastlock_acquireshared (&lock_transaction);

	if (!config.hengine)
	{
		// generate unique session key
		if (!config.psession)
		{
			config.psession = new GUID;

			if (CoCreateGuid (config.psession) != S_OK)
				SAFE_DELETE (config.psession);
		}

		SecureZeroMemory (&session, sizeof (session));

		session.displayData.name = APP_NAME;
		session.displayData.description = APP_NAME;

		session.txnWaitTimeoutInMSec = TRANSACTION_TIMEOUT;

		if (config.psession)
			CopyMemory (&session.sessionKey, config.psession, sizeof (GUID));

		rc = FwpmEngineOpen (nullptr, RPC_C_AUTHN_WINNT, nullptr, &session, &config.hengine);

		if (rc != ERROR_SUCCESS || !config.hengine)
		{
			_app_logerror (L"FwpmEngineOpen", rc, nullptr, false);

			if (config.hengine)
				config.hengine = nullptr;
		}

		if (!config.hengine)
		{
			_r_fastlock_releaseshared (&lock_transaction);
			return false;
		}
	}

	// set security info
	if (config.pusersid && (!config.pacl_engine || !config.pacl_default || !config.pacl_secure))
	{
		SAFE_LOCAL_FREE (config.pacl_engine);
		SAFE_LOCAL_FREE (config.pacl_default);
		SAFE_LOCAL_FREE (config.pacl_secure);

		PSID pWorldSID = nullptr;
		SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;

		if (!AllocateAndInitializeSid (&SIDAuthWorld, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &pWorldSID))
		{
			_app_logerror (L"AllocateAndInitializeSid", GetLastError (), nullptr, true);
		}
		else
		{
			static const size_t count = 2;
			EXPLICIT_ACCESS access[count] = {0};

			SecureZeroMemory (access, count * sizeof (EXPLICIT_ACCESS));

			// create default (engine) acl
			access[0].grfAccessPermissions = FWPM_GENERIC_ALL | DELETE | WRITE_DAC | WRITE_OWNER;
			access[0].grfAccessMode = GRANT_ACCESS;
			access[0].grfInheritance = NO_INHERITANCE;
			BuildTrusteeWithSid (&(access[0].Trustee), config.pusersid);

			access[1].grfAccessPermissions = FWPM_GENERIC_ALL;
			access[1].grfAccessMode = GRANT_ACCESS;
			access[1].grfInheritance = CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE;
			BuildTrusteeWithSid (&(access[1].Trustee), pWorldSID);

			SetEntriesInAcl (_countof (access), access, nullptr, &config.pacl_engine);

			// create default (simplewall) acl
			access[0].grfAccessPermissions = FWPM_GENERIC_EXECUTE | FWPM_GENERIC_WRITE | DELETE | WRITE_DAC | WRITE_OWNER;
			access[0].grfAccessMode = GRANT_ACCESS;
			access[0].grfInheritance = NO_INHERITANCE;
			BuildTrusteeWithSid (&(access[0].Trustee), config.pusersid);

			access[1].grfAccessPermissions = FWPM_GENERIC_EXECUTE | FWPM_GENERIC_READ;
			access[1].grfAccessMode = GRANT_ACCESS;
			access[1].grfInheritance = CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE;
			BuildTrusteeWithSid (&(access[1].Trustee), pWorldSID);

			SetEntriesInAcl (_countof (access), access, nullptr, &config.pacl_default);

			// create secure (simplewall) acl
			access[0].grfAccessPermissions = FWPM_GENERIC_EXECUTE | FWPM_GENERIC_READ;
			access[0].grfAccessMode = GRANT_ACCESS;
			access[0].grfInheritance = NO_INHERITANCE;
			BuildTrusteeWithSid (&(access[0].Trustee), config.pusersid);

			access[1].grfAccessPermissions = FWPM_ACTRL_WRITE | DELETE | WRITE_DAC | WRITE_OWNER;
			access[1].grfAccessMode = DENY_ACCESS;
			access[1].grfInheritance = CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE;
			BuildTrusteeWithSid (&(access[1].Trustee), pWorldSID);

			SetEntriesInAcl (_countof (access), access, nullptr, &config.pacl_secure);

			FreeSid (pWorldSID);
		}
	}

	// set security information
	{
		if (config.pusersid)
		{
			FwpmEngineSetSecurityInfo (config.hengine, OWNER_SECURITY_INFORMATION, (const SID*)config.pusersid, nullptr, nullptr, nullptr);
			FwpmNetEventsSetSecurityInfo (config.hengine, OWNER_SECURITY_INFORMATION, (const SID*)config.pusersid, nullptr, nullptr, nullptr);
		}

		if (config.pacl_engine)
		{
			FwpmEngineSetSecurityInfo (config.hengine, DACL_SECURITY_INFORMATION, nullptr, nullptr, config.pacl_engine, nullptr);
			FwpmNetEventsSetSecurityInfo (config.hengine, DACL_SECURITY_INFORMATION, nullptr, nullptr, config.pacl_engine, nullptr);
		}
	}

	// set engine options
	{
		FWP_VALUE val;

		// dropped packets logging (win7+)
		if (is_full && !config.is_neteventset)
		{
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
						_app_logerror (L"FwpmEngineSetOption", rc, L"FWPM_ENGINE_NET_EVENT_MATCH_ANY_KEYWORDS", true);

					// enables the connection monitoring feature and starts logging creation and deletion events (and notifying any subscribers)
					val.type = FWP_UINT32;
					val.uint32 = 1;

					rc = FwpmEngineSetOption (config.hengine, FWPM_ENGINE_MONITOR_IPSEC_CONNECTIONS, &val);

					if (rc != ERROR_SUCCESS)
						_app_logerror (L"FwpmEngineSetOption", rc, L"FWPM_ENGINE_MONITOR_IPSEC_CONNECTIONS", true);
				}

				config.is_neteventset = true;
			}
		}

		// packet queuing (win8+)
		if (is_full && _r_sys_validversion (6, 2) && app.ConfigGet (L"IsPacketQueuingEnabled", true).AsBool ())
		{
			// enables inbound or forward packet queuing independently. when enabled, the system is able to evenly distribute cpu load to multiple cpus for site-to-site ipsec tunnel scenarios.
			val.type = FWP_UINT32;
			val.uint32 = FWPM_ENGINE_OPTION_PACKET_QUEUE_INBOUND | FWPM_ENGINE_OPTION_PACKET_QUEUE_FORWARD;

			rc = FwpmEngineSetOption (config.hengine, FWPM_ENGINE_PACKET_QUEUING, &val);

			if (rc != ERROR_SUCCESS)
				_app_logerror (L"FwpmEngineSetOption", rc, L"FWPM_ENGINE_PACKET_QUEUING", true);
		}
	}

	if (is_full)
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

			sublayer.providerKey = (LPGUID)& GUID_WfpProvider;
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
			}
		}
	}

	// set security information
	if (_wfp_isfiltersinstalled ())
	{
		const bool is_secure = app.ConfigGet (L"IsSecureFilters", false).AsBool ();

		if (config.pusersid)
		{
			FwpmProviderSetSecurityInfoByKey (config.hengine, &GUID_WfpProvider, OWNER_SECURITY_INFORMATION, (const SID*)config.pusersid, nullptr, nullptr, nullptr);
			FwpmSubLayerSetSecurityInfoByKey (config.hengine, &GUID_WfpSublayer, OWNER_SECURITY_INFORMATION, (const SID*)config.pusersid, nullptr, nullptr, nullptr);
		}

		if (is_secure ? config.pacl_secure : config.pacl_default)
		{
			FwpmProviderSetSecurityInfoByKey (config.hengine, &GUID_WfpProvider, DACL_SECURITY_INFORMATION, nullptr, nullptr, is_secure ? config.pacl_secure : config.pacl_default, nullptr);
			FwpmSubLayerSetSecurityInfoByKey (config.hengine, &GUID_WfpSublayer, DACL_SECURITY_INFORMATION, nullptr, nullptr, is_secure ? config.pacl_secure : config.pacl_default, nullptr);
		}
	}

	_r_fastlock_releaseshared (&lock_transaction);

	return result;
}

void _wfp_uninitialize (bool is_full)
{
	SAFE_DELETE (config.psession);

	if (!config.hengine)
		return;

	_r_fastlock_acquireshared (&lock_transaction);

	DWORD result;

	FWP_VALUE val;

	// dropped packets logging (win7+)
	if (config.is_neteventset)
	{
		_wfp_logunsubscribe ();

		// collect ipsec connection (win8+)
		if (_r_sys_validversion (6, 2))
		{
			val.type = FWP_UINT32;
			val.uint32 = 0;

			result = FwpmEngineSetOption (config.hengine, FWPM_ENGINE_MONITOR_IPSEC_CONNECTIONS, &val);

			if (result != ERROR_SUCCESS)
				_app_logerror (L"FwpmEngineSetOption", result, L"FWPM_ENGINE_MONITOR_IPSEC_CONNECTIONS", true);
		}

		val.type = FWP_UINT32;
		val.uint32 = 0;

		result = FwpmEngineSetOption (config.hengine, FWPM_ENGINE_COLLECT_NET_EVENTS, &val);

		if (result != ERROR_SUCCESS)
			_app_logerror (L"FwpmEngineSetOption", result, L"FWPM_ENGINE_COLLECT_NET_EVENTS", true);

		else
			config.is_neteventset = false;
	}

	// packet queuing (win8+)
	if (_r_sys_validversion (6, 2))
	{
		val.type = FWP_UINT32;
		val.uint32 = FWPM_ENGINE_OPTION_PACKET_QUEUE_NONE;

		result = FwpmEngineSetOption (config.hengine, FWPM_ENGINE_PACKET_QUEUING, &val);

		if (result != ERROR_SUCCESS)
			_app_logerror (L"FwpmEngineSetOption", result, L"FWPM_ENGINE_PACKET_QUEUING", true);
	}

	if (is_full)
	{
		// set security information
		if (config.pusersid)
		{
			FwpmProviderSetSecurityInfoByKey (config.hengine, &GUID_WfpProvider, OWNER_SECURITY_INFORMATION, (const SID*)config.pusersid, nullptr, nullptr, nullptr);
			FwpmSubLayerSetSecurityInfoByKey (config.hengine, &GUID_WfpSublayer, OWNER_SECURITY_INFORMATION, (const SID*)config.pusersid, nullptr, nullptr, nullptr);

		}

		if (config.pacl_default)
		{
			FwpmProviderSetSecurityInfoByKey (config.hengine, &GUID_WfpProvider, DACL_SECURITY_INFORMATION, nullptr, nullptr, config.pacl_default, nullptr);
			FwpmSubLayerSetSecurityInfoByKey (config.hengine, &GUID_WfpSublayer, DACL_SECURITY_INFORMATION, nullptr, nullptr, config.pacl_default, nullptr);
		}

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
	}

	FwpmEngineClose (config.hengine);
	config.hengine = nullptr;

	_r_fastlock_releaseshared (&lock_transaction);
}

void _wfp_installfilters ()
{
	// set security information
	if (config.pusersid)
	{
		FwpmProviderSetSecurityInfoByKey (config.hengine, &GUID_WfpProvider, OWNER_SECURITY_INFORMATION, (const SID*)config.pusersid, nullptr, nullptr, nullptr);
		FwpmSubLayerSetSecurityInfoByKey (config.hengine, &GUID_WfpSublayer, OWNER_SECURITY_INFORMATION, (const SID*)config.pusersid, nullptr, nullptr, nullptr);
	}

	if (config.pacl_default)
	{
		FwpmProviderSetSecurityInfoByKey (config.hengine, &GUID_WfpProvider, DACL_SECURITY_INFORMATION, nullptr, nullptr, config.pacl_default, nullptr);
		FwpmSubLayerSetSecurityInfoByKey (config.hengine, &GUID_WfpSublayer, DACL_SECURITY_INFORMATION, nullptr, nullptr, config.pacl_default, nullptr);
	}

	_wfp_clearfilter_ids ();

	_r_fastlock_acquireshared (&lock_transaction);

	// dump all filters into array
	GUIDS_VEC filter_all;
	const bool filters_count = _wfp_dumpfilters (&GUID_WfpProvider, &filter_all);

	// restore filters security
	if (filters_count)
	{
		for (size_t i = 0; i < filter_all.size (); i++)
			_wfp_setfiltersecurity (config.hengine, &filter_all.at (i), config.pusersid, config.pacl_default, __LINE__);
	}

	const bool is_intransact = _wfp_transact_start (__LINE__);

	// destroy all filters
	if (filters_count)
	{
		for (size_t i = 0; i < filter_all.size (); i++)
			_wfp_deletefilter (config.hengine, &filter_all.at (i));
	}

	// apply internal rules
	_wfp_create2filters (__LINE__, is_intransact);

	// apply apps rules
	{
		OBJECTS_VEC rules;

		for (auto &p : apps)
		{
			PR_OBJECT ptr_app_object = _r_obj_reference (p.second);

			if (!ptr_app_object)
				continue;

			PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

			if (ptr_app && ptr_app->is_enabled)
			{
				rules.push_back (ptr_app_object);
			}
			else
			{
				_r_obj_dereference (ptr_app_object, &_app_dereferenceapp);
			}
		}

		_wfp_create3filters (rules, __LINE__, is_intransact);
		_app_freeobjects_vec (rules, &_app_dereferenceapp);
	}

	// apply system/custom/blocklist rules
	{
		OBJECTS_VEC rules;

		for (size_t i = 0; i < rules_arr.size (); i++)
		{
			PR_OBJECT ptr_rule_object = _r_obj_reference (rules_arr.at (i));

			if (!ptr_rule_object)
				continue;

			const PITEM_RULE ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

			if (ptr_rule && ptr_rule->is_enabled)
			{
				rules.push_back (ptr_rule_object);
			}
			else
			{
				_r_obj_dereference (ptr_rule_object, &_app_dereferencerule);
			}
		}

		_wfp_create4filters (rules, __LINE__, is_intransact);
		_app_freeobjects_vec (rules, &_app_dereferencerule);
	}

	if (is_intransact)
		_wfp_transact_commit (__LINE__);

	// secure filters
	{
		const bool is_secure = app.ConfigGet (L"IsSecureFilters", false).AsBool ();

		if (is_secure ? config.pacl_secure : config.pacl_default)
		{
			if (_wfp_dumpfilters (&GUID_WfpProvider, &filter_all))
			{
				for (size_t i = 0; i < filter_all.size (); i++)
					_wfp_setfiltersecurity (config.hengine, &filter_all.at (i), config.pusersid, is_secure ? config.pacl_secure : config.pacl_default, __LINE__);
			}
		}

		// set security information
		if (config.pusersid)
		{
			FwpmProviderSetSecurityInfoByKey (config.hengine, &GUID_WfpProvider, OWNER_SECURITY_INFORMATION, (const SID*)config.pusersid, nullptr, nullptr, nullptr);
			FwpmSubLayerSetSecurityInfoByKey (config.hengine, &GUID_WfpSublayer, OWNER_SECURITY_INFORMATION, (const SID*)config.pusersid, nullptr, nullptr, nullptr);
		}

		if (is_secure ? config.pacl_secure : config.pacl_default)
		{
			FwpmProviderSetSecurityInfoByKey (config.hengine, &GUID_WfpProvider, DACL_SECURITY_INFORMATION, nullptr, nullptr, is_secure ? config.pacl_secure : config.pacl_default, nullptr);
			FwpmSubLayerSetSecurityInfoByKey (config.hengine, &GUID_WfpSublayer, DACL_SECURITY_INFORMATION, nullptr, nullptr, is_secure ? config.pacl_secure : config.pacl_default, nullptr);
		}
	}

	_r_fastlock_releaseshared (&lock_transaction);
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

bool _wfp_deletefilter (HANDLE engineHandle, const GUID * ptr_filter_id)
{
	if (!engineHandle || !ptr_filter_id || *ptr_filter_id == GUID_NULL)
		return false;

	const DWORD result = FwpmFilterDeleteByKey (config.hengine, ptr_filter_id);

	if (result != ERROR_SUCCESS && result != FWP_E_FILTER_NOT_FOUND)
	{
		_app_logerror (L"FwpmFilterDeleteByKey", result, _r_str_fromguid (*ptr_filter_id).GetString (), false);
		return false;
	}

	return true;
}

DWORD _wfp_createfilter (LPCWSTR name, FWPM_FILTER_CONDITION * lpcond, UINT32 const count, UINT8 weight, const GUID * layer, const GUID * callout, FWP_ACTION_TYPE action, UINT32 flags, GUIDS_VEC * ptr_filters)
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

	filter.providerKey = (LPGUID)& GUID_WfpProvider;
	filter.subLayerKey = GUID_WfpSublayer;
	CoCreateGuid (&filter.filterKey); // set filter guid

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
		if (ptr_filters)
			ptr_filters->push_back (filter.filterKey);
	}
	else
	{
		_app_logerror (L"FwpmFilterAdd", result, name, false);
	}

	return result;
}

void _wfp_clearfilter_ids ()
{
	_r_fastlock_acquireexclusive (&lock_access);

	// clear common filters
	filter_ids.clear ();

	// clear apps filters
	for (auto &p : apps)
		_app_setappinfo (p.first, InfoClearIds, 0);

	// clear rules filters
	for (size_t i = 0; i < rules_arr.size (); i++)
	{
		PR_OBJECT ptr_rule_object = _r_obj_reference (rules_arr.at (i));

		if (!ptr_rule_object)
			continue;

		PITEM_RULE ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

		if (ptr_rule)
		{
			ptr_rule->is_haveerrors = false;
			ptr_rule->guids.clear ();
		}

		_r_obj_dereference (ptr_rule_object, &_app_dereferencerule);
	}

	_r_fastlock_releaseexclusive (&lock_access);
}

void _wfp_destroyfilters ()
{
	if (!config.hengine)
		return;

	_wfp_clearfilter_ids ();

	// destroy all filters
	GUIDS_VEC filter_all;

	if (_wfp_dumpfilters (&GUID_WfpProvider, &filter_all))
		_wfp_destroy2filters (filter_all, __LINE__);
}

bool _wfp_destroy2filters (GUIDS_VEC & ptr_filters, UINT line)
{
	if (!config.hengine || ptr_filters.empty ())
		return false;

	const bool is_enabled = _app_initinterfacestate (app.GetHWND ());

	_r_fastlock_acquireshared (&lock_transaction);

	for (size_t i = 0; i < ptr_filters.size (); i++)
		_wfp_setfiltersecurity (config.hengine, &ptr_filters.at (i), config.pusersid, config.pacl_default, line);

	const bool is_intransact = _wfp_transact_start (line);

	for (size_t i = 0; i < ptr_filters.size (); i++)
		_wfp_deletefilter (config.hengine, &ptr_filters.at (i));

	ptr_filters.clear ();

	if (is_intransact)
		_wfp_transact_commit (line);

	_r_fastlock_releaseshared (&lock_transaction);

	_app_restoreinterfacestate (app.GetHWND (), is_enabled);

	return true;
}

bool _wfp_createrulefilter (LPCWSTR name, size_t app_hash, LPCWSTR rule_remote, LPCWSTR rule_local, UINT8 protocol, ADDRESS_FAMILY af, FWP_DIRECTION dir, UINT8 weight, FWP_ACTION_TYPE action, UINT32 flag, GUIDS_VEC * pmfarr)
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
	if (app_hash)
	{
		PR_OBJECT ptr_app_object = _app_getappitem (app_hash);

		if (!ptr_app_object)
			return false;

		PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

		if (!ptr_app)
		{
			_r_obj_dereference (ptr_app_object, &_app_dereferenceapp);
			return false;
		}

		if (ptr_app->type == DataAppUWP) // windows store app (win8+)
		{
			if (ptr_app->pdata)
			{
				fwfc[count].fieldKey = FWPM_CONDITION_ALE_PACKAGE_ID;
				fwfc[count].matchType = FWP_MATCH_EQUAL;
				fwfc[count].conditionValue.type = FWP_SID;
				fwfc[count].conditionValue.sid = (SID*)ptr_app->pdata;

				count += 1;
			}
			else
			{
				_app_logerror (TEXT (__FUNCTION__), 0, ptr_app->display_name, true);
				_r_obj_dereference (ptr_app_object, &_app_dereferenceapp);

				return false;
			}
		}
		else if (ptr_app->type == DataAppService) // windows service
		{
			if (ptr_app->pdata && ByteBlobAlloc (ptr_app->pdata, GetSecurityDescriptorLength (ptr_app->pdata), &bSid))
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
				_r_obj_dereference (ptr_app_object, &_app_dereferenceapp);

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

				// do not log file not found to error log
				if (rc != ERROR_FILE_NOT_FOUND && rc != ERROR_PATH_NOT_FOUND)
					_app_logerror (L"FwpmGetAppIdFromFileName", rc, path, true);

				_r_obj_dereference (ptr_app_object, &_app_dereferenceapp);

				return false;
			}
		}

		_r_obj_dereference (ptr_app_object, &_app_dereferenceapp);
	}

	// set ip/port condition
	{
		LPCWSTR rules[] = {
			rule_remote,
			rule_local
		};

		for (size_t i = 0; i < _countof (rules); i++)
		{
			if (rules[i] && rules[i][0] && _wcsicmp (rules[i], L"*") != 0)
			{
				if (!_app_parserulestring (rules[i], &addr))
				{
					ByteBlobFree (&bSid);
					ByteBlobFree (&bPath);

					return false;
				}
				else
				{
					if (i == 0)
					{
						if (addr.type == DataTypeIp || addr.type == DataTypeHost)
							is_remoteaddr_set = true;

						else if (addr.type == DataTypePort)
							is_remoteport_set = true;
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
								ByteBlobFree (&bSid);
								ByteBlobFree (&bPath);

								return false;
							}
						}

						fwfc[count].fieldKey = (addr.type == DataTypePort) ? ((i == 0) ? FWPM_CONDITION_IP_REMOTE_PORT : FWPM_CONDITION_IP_LOCAL_PORT) : ((i == 0) ? FWPM_CONDITION_IP_REMOTE_ADDRESS : FWPM_CONDITION_IP_LOCAL_ADDRESS);
						fwfc[count].matchType = FWP_MATCH_RANGE;
						fwfc[count].conditionValue.type = FWP_RANGE_TYPE;
						fwfc[count].conditionValue.rangeValue = &range;

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
									if (!_wfp_createrulefilter (name, app_hash, arr2[j], nullptr, protocol, af, dir, weight, action, flag, pmfarr))
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
			_wfp_createfilter (name, fwfc, count, weight, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V4, nullptr, action, flag, pmfarr);
		}

		if (af == AF_INET6 || af == AF_UNSPEC)
		{
			_wfp_createfilter (name, fwfc, count, weight, &FWPM_LAYER_ALE_AUTH_CONNECT_V6, nullptr, action, flag, pmfarr);

			// win7+
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

bool _wfp_create4filters (OBJECTS_VEC & ptr_rules, UINT line, bool is_intransact)
{
	if (!config.hengine || ptr_rules.empty ())
		return false;

	const bool is_enabled = _app_initinterfacestate (app.GetHWND ());

	if (!is_intransact && _wfp_isfiltersapplying ())
		is_intransact = true;

	if (!is_intransact)
	{
		GUIDS_VEC guids;

		for (size_t i = 0; i < ptr_rules.size (); i++)
		{
			PR_OBJECT ptr_rule_object = _r_obj_reference (ptr_rules.at (i));

			if (!ptr_rule_object)
				continue;

			const PITEM_RULE ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

			if (ptr_rule)
				guids.insert (guids.end (), ptr_rule->guids.begin (), ptr_rule->guids.end ());

			_r_obj_dereference (ptr_rule_object, &_app_dereferencerule);
		}

		_wfp_destroy2filters (guids, line);

		_r_fastlock_acquireshared (&lock_transaction);
		is_intransact = !_wfp_transact_start (line);
	}

	for (size_t i = 0; i < ptr_rules.size (); i++)
	{
		PR_OBJECT ptr_rule_object = _r_obj_reference (ptr_rules.at (i));

		if (!ptr_rule_object)
			continue;

		const PITEM_RULE ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

		if (ptr_rule)
		{
			GUIDS_VEC guids;
			bool is_haveerrors = false;

			if (ptr_rule->is_enabled)
			{
				rstring::rvector rule_remote_arr = rstring (ptr_rule->prule_remote).AsVector (RULE_DELIMETER);
				rstring::rvector rule_local_arr = rstring (ptr_rule->prule_local).AsVector (RULE_DELIMETER);

				const size_t rules_remote_length = rule_remote_arr.size ();
				const size_t rules_local_length = rule_local_arr.size ();
				const size_t count = max (1, max (rules_remote_length, rules_local_length));

				for (size_t j = 0; j < count; j++)
				{
					rstring rule_remote = L"";
					rstring rule_local = L"";

					// sync remote rules and local rules
					if (!rule_remote_arr.empty () && rules_remote_length > j)
						rule_remote = rule_remote_arr.at (j).Trim (L"\r\n ");

					// sync local rules and remote rules
					if (!rule_local_arr.empty () && rules_local_length > j)
						rule_local = rule_local_arr.at (j).Trim (L"\r\n ");

					// apply rules for services hosts
					if (ptr_rule->is_forservices)
					{
						if (!_wfp_createrulefilter (ptr_rule->pname, config.ntoskrnl_hash, rule_remote, rule_local, ptr_rule->protocol, ptr_rule->af, ptr_rule->dir, ptr_rule->weight, ptr_rule->is_block ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT, 0, &guids))
							is_haveerrors = true;

						if (!_wfp_createrulefilter (ptr_rule->pname, config.svchost_hash, rule_remote, rule_local, ptr_rule->protocol, ptr_rule->af, ptr_rule->dir, ptr_rule->weight, ptr_rule->is_block ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT, 0, &guids))
							is_haveerrors = true;
					}

					if (!ptr_rule->apps.empty ())
					{
						for (auto const &p : ptr_rule->apps)
						{
							if (ptr_rule->is_forservices && (p.first == config.ntoskrnl_hash || p.first == config.svchost_hash))
								continue;

							if (!_wfp_createrulefilter (ptr_rule->pname, p.first, rule_remote, rule_local, ptr_rule->protocol, ptr_rule->af, ptr_rule->dir, ptr_rule->weight, ptr_rule->is_block ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT, 0, &guids))
								is_haveerrors = true;
						}
					}
					else
					{
						if (!_wfp_createrulefilter (ptr_rule->pname, 0, rule_remote, rule_local, ptr_rule->protocol, ptr_rule->af, ptr_rule->dir, ptr_rule->weight, ptr_rule->is_block ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT, 0, &guids))
							is_haveerrors = true;
					}
				}
			}

			ptr_rule->is_haveerrors = is_haveerrors;

			ptr_rule->guids.clear ();
			ptr_rule->guids.insert (ptr_rule->guids.end (), guids.begin (), guids.end ());
		}

		_r_obj_dereference (ptr_rule_object, &_app_dereferencerule);
	}

	if (!is_intransact)
	{
		_wfp_transact_commit (line);

		const bool is_secure = app.ConfigGet (L"IsSecureFilters", false).AsBool ();

		if (is_secure ? config.pacl_secure : config.pacl_default)
		{
			for (size_t i = 0; i < ptr_rules.size (); i++)
			{
				PR_OBJECT ptr_rule_object = _r_obj_reference (ptr_rules.at (i));

				if (!ptr_rule_object)
					continue;

				const PITEM_RULE ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

				if (ptr_rule && ptr_rule->is_enabled)
				{
					for (size_t j = 0; j < ptr_rule->guids.size (); j++)
						_wfp_setfiltersecurity (config.hengine, &ptr_rule->guids.at (j), config.pusersid, is_secure ? config.pacl_secure : config.pacl_default, line);
				}

				_r_obj_dereference (ptr_rule_object, &_app_dereferencerule);
			}
		}

		_r_fastlock_releaseshared (&lock_transaction);
	}

	_app_restoreinterfacestate (app.GetHWND (), is_enabled);

	return true;
}

bool _wfp_create3filters (OBJECTS_VEC & ptr_apps, UINT line, bool is_intransact)
{
	if (!config.hengine || ptr_apps.empty ())
		return false;

	const bool is_enabled = _app_initinterfacestate (app.GetHWND ());

	static const FWP_ACTION_TYPE action = FWP_ACTION_PERMIT;

	if (!is_intransact && _wfp_isfiltersapplying ())
		is_intransact = true;

	if (!is_intransact)
	{
		GUIDS_VEC ids;

		for (size_t i = 0; i < ptr_apps.size (); i++)
		{
			PR_OBJECT ptr_app_object = _r_obj_reference (ptr_apps.at (i));

			if (!ptr_app_object)
				continue;

			PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

			if (ptr_app)
				ids.insert (ids.end (), ptr_app->guids.begin (), ptr_app->guids.end ());

			_r_obj_dereference (ptr_app_object, &_app_dereferenceapp);
		}

		_wfp_destroy2filters (ids, line);

		_r_fastlock_acquireshared (&lock_transaction);
		is_intransact = !_wfp_transact_start (line);
	}

	for (size_t i = 0; i < ptr_apps.size (); i++)
	{
		PR_OBJECT ptr_app_object = _r_obj_reference (ptr_apps.at (i));

		if (!ptr_app_object)
			continue;

		PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

		if (ptr_app)
		{
			GUIDS_VEC guids;
			bool is_haveerrors = false;

			if (ptr_app->is_enabled)
			{
				if (!_wfp_createrulefilter (ptr_app->display_name, _r_str_hash (ptr_app->original_path), nullptr, nullptr, 0, AF_UNSPEC, FWP_DIRECTION_MAX, FILTER_WEIGHT_APPLICATION, action, 0, &guids))
					is_haveerrors = true;
			}

			ptr_app->is_haveerrors = is_haveerrors;

			ptr_app->guids.clear ();
			ptr_app->guids.insert (ptr_app->guids.end (), guids.begin (), guids.end ());
		}

		_r_obj_dereference (ptr_app_object, &_app_dereferenceapp);
	}

	if (!is_intransact)
	{
		_wfp_transact_commit (line);

		const bool is_secure = app.ConfigGet (L"IsSecureFilters", false).AsBool ();

		if (is_secure ? config.pacl_secure : config.pacl_default)
		{
			for (size_t i = 0; i < ptr_apps.size (); i++)
			{
				PR_OBJECT ptr_app_object = _r_obj_reference (ptr_apps.at (i));

				if (!ptr_app_object)
					continue;

				PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

				if (ptr_app)
				{
					for (size_t j = 0; j < ptr_app->guids.size (); j++)
						_wfp_setfiltersecurity (config.hengine, &ptr_app->guids.at (j), config.pusersid, is_secure ? config.pacl_secure : config.pacl_default, line);
				}

				_r_obj_dereference (ptr_app_object, &_app_dereferenceapp);
			}
		}

		_r_fastlock_releaseshared (&lock_transaction);
	}

	_app_restoreinterfacestate (app.GetHWND (), is_enabled);

	return true;
}

bool _wfp_create2filters (UINT line, bool is_intransact)
{
	if (!config.hengine)
		return false;

	const bool is_enabled = _app_initinterfacestate (app.GetHWND ());

	if (!is_intransact && _wfp_isfiltersapplying ())
		is_intransact = true;

	if (!is_intransact)
	{
		_wfp_destroy2filters (filter_ids, line);
		filter_ids.clear ();

		_r_fastlock_acquireshared (&lock_transaction);
		is_intransact = !_wfp_transact_start (line);
	}

	FWPM_FILTER_CONDITION fwfc[3] = {0};

	// add loopback connections permission
	if (app.ConfigGet (L"AllowLoopbackConnections", true).AsBool ())
	{
		// match all loopback (localhost) data
		fwfc[0].fieldKey = FWPM_CONDITION_FLAGS;
		fwfc[0].matchType = FWP_MATCH_FLAGS_ALL_SET;
		fwfc[0].conditionValue.type = FWP_UINT32;
		fwfc[0].conditionValue.uint32 = FWP_CONDITION_FLAG_IS_LOOPBACK;

		// tests if the network traffic is (non-)app container loopback traffic (win8+)
		if (_r_sys_validversion (6, 2))
		{
			fwfc[0].matchType = FWP_MATCH_FLAGS_ANY_SET;
			fwfc[0].conditionValue.uint32 |= FWP_CONDITION_FLAG_IS_APPCONTAINER_LOOPBACK;
		}

		_wfp_createfilter (nullptr, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_CONNECT_V4, nullptr, FWP_ACTION_PERMIT, 0, &filter_ids);
		_wfp_createfilter (nullptr, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_CONNECT_V6, nullptr, FWP_ACTION_PERMIT, 0, &filter_ids);

		// win7+
		_wfp_createfilter (nullptr, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V4, nullptr, FWP_ACTION_PERMIT, 0, &filter_ids);
		_wfp_createfilter (nullptr, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V6, nullptr, FWP_ACTION_PERMIT, 0, &filter_ids);

		_wfp_createfilter (nullptr, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, nullptr, FWP_ACTION_PERMIT, 0, &filter_ids);
		_wfp_createfilter (nullptr, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, FWP_ACTION_PERMIT, 0, &filter_ids);

		_wfp_createfilter (nullptr, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V4, nullptr, FWP_ACTION_PERMIT, 0, &filter_ids);
		_wfp_createfilter (nullptr, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V6, nullptr, FWP_ACTION_PERMIT, 0, &filter_ids);

		_wfp_createfilter (nullptr, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_LISTEN_V4, nullptr, FWP_ACTION_PERMIT, 0, &filter_ids);
		_wfp_createfilter (nullptr, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_LISTEN_V6, nullptr, FWP_ACTION_PERMIT, 0, &filter_ids);

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

			if (_app_parserulestring (ip_list[i], &addr))
			{
				fwfc[1].matchType = FWP_MATCH_EQUAL;

				if (addr.format == NET_ADDRESS_IPV4)
				{
					fwfc[1].conditionValue.type = FWP_V4_ADDR_MASK;
					fwfc[1].conditionValue.v4AddrMask = &addr4;

					fwfc[1].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
					_wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_CONNECT_V4, nullptr, FWP_ACTION_PERMIT, 0, &filter_ids);

					// win7+
					_wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V4, nullptr, FWP_ACTION_PERMIT, 0, &filter_ids);

					fwfc[1].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;
					_wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, nullptr, FWP_ACTION_PERMIT, 0, &filter_ids);
					_wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V4, nullptr, FWP_ACTION_PERMIT, 0, &filter_ids);
					_wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_LISTEN_V4, nullptr, FWP_ACTION_PERMIT, 0, &filter_ids);
				}
				else if (addr.format == NET_ADDRESS_IPV6)
				{
					fwfc[1].conditionValue.type = FWP_V6_ADDR_MASK;
					fwfc[1].conditionValue.v6AddrMask = &addr6;

					fwfc[1].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
					_wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_CONNECT_V6, nullptr, FWP_ACTION_PERMIT, 0, &filter_ids);

					// win7+
					_wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V6, nullptr, FWP_ACTION_PERMIT, 0, &filter_ids);

					fwfc[1].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;
					_wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, FWP_ACTION_PERMIT, 0, &filter_ids);
					_wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V6, nullptr, FWP_ACTION_PERMIT, 0, &filter_ids);
					_wfp_createfilter (nullptr, fwfc, 2, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_LISTEN_V6, nullptr, FWP_ACTION_PERMIT, 0, &filter_ids);
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

		_wfp_createfilter (L"Allow6to4", fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, nullptr, FWP_ACTION_PERMIT, 0, &filter_ids);

		// allows icmpv6 router solicitation messages, which are required for the ipv6 stack to work properly
		fwfc[0].fieldKey = FWPM_CONDITION_ICMP_TYPE;
		fwfc[0].matchType = FWP_MATCH_EQUAL;
		fwfc[0].conditionValue.type = FWP_UINT16;
		fwfc[0].conditionValue.uint16 = 0x85;

		_wfp_createfilter (L"AllowIcmpV6Type133", fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, FWP_ACTION_PERMIT, 0, &filter_ids);

		// allows icmpv6 router advertise messages, which are required for the ipv6 stack to work properly
		fwfc[0].conditionValue.uint16 = 0x86;
		_wfp_createfilter (L"AllowIcmpV6Type134", fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, FWP_ACTION_PERMIT, 0, &filter_ids);

		// allows icmpv6 neighbor solicitation messages, which are required for the ipv6 stack to work properly
		fwfc[0].conditionValue.uint16 = 0x87;
		_wfp_createfilter (L"AllowIcmpV6Type135", fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, FWP_ACTION_PERMIT, 0, &filter_ids);

		// allows icmpv6 neighbor advertise messages, which are required for the ipv6 stack to work properly
		fwfc[0].conditionValue.uint16 = 0x88;
		_wfp_createfilter (L"AllowIcmpV6Type136", fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, FWP_ACTION_PERMIT, 0, &filter_ids);
	}

	// prevent port scanning using stealth discards and silent drops
	// https://docs.microsoft.com/ru-ru/windows/desktop/FWP/preventing-port-scanning
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

		_wfp_createfilter (L"BlockIcmpErrorV4", fwfc, 2, FILTER_WEIGHT_HIGHEST, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V4, nullptr, FWP_ACTION_BLOCK, 0, &filter_ids);
		_wfp_createfilter (L"BlockIcmpErrorV6", fwfc, 2, FILTER_WEIGHT_HIGHEST, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V6, nullptr, FWP_ACTION_BLOCK, 0, &filter_ids);

		// blocks tcp port scanners (exclude loopback)
		fwfc[0].conditionValue.uint32 |= FWP_CONDITION_FLAG_IS_IPSEC_SECURED;

		_wfp_createfilter (L"BlockTcpRstOnCloseV4", fwfc, 1, FILTER_WEIGHT_HIGHEST, &FWPM_LAYER_INBOUND_TRANSPORT_V4_DISCARD, &FWPM_CALLOUT_WFP_TRANSPORT_LAYER_V4_SILENT_DROP, FWP_ACTION_CALLOUT_TERMINATING, 0, &filter_ids);
		_wfp_createfilter (L"BlockTcpRstOnCloseV6", fwfc, 1, FILTER_WEIGHT_HIGHEST, &FWPM_LAYER_INBOUND_TRANSPORT_V6_DISCARD, &FWPM_CALLOUT_WFP_TRANSPORT_LAYER_V6_SILENT_DROP, FWP_ACTION_CALLOUT_TERMINATING, 0, &filter_ids);
	}

	// block all outbound traffic
	_wfp_createfilter (L"BlockConnectionsV4", nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_CONNECT_V4, nullptr, FWP_ACTION_BLOCK, 0, &filter_ids);
	_wfp_createfilter (L"BlockConnectionsV6", nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_CONNECT_V6, nullptr, FWP_ACTION_BLOCK, 0, &filter_ids);

	// win7+
	_wfp_createfilter (L"BlockConnectionOutboundRedirectionV4", nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V4, nullptr, FWP_ACTION_BLOCK, 0, &filter_ids);
	_wfp_createfilter (L"BlockConnectionOutboundRedirectionV6", nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_CONNECT_REDIRECT_V6, nullptr, FWP_ACTION_BLOCK, 0, &filter_ids);

	// block all inbound traffic (only on "stealth" mode)
	if (app.ConfigGet (L"UseStealthMode", false).AsBool () || !app.ConfigGet (L"AllowInboundConnections", false).AsBool ())
	{
		_wfp_createfilter (L"BlockRecvAcceptConnectionsV4", nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, nullptr, FWP_ACTION_BLOCK, 0, &filter_ids);
		_wfp_createfilter (L"BlockRecvAcceptConnectionsV6", nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, FWP_ACTION_BLOCK, 0, &filter_ids);
	}

	// block all listen traffic (NOT RECOMMENDED!!!!)
	// issue: https://github.com/henrypp/simplewall/issues/9
	if (!app.ConfigGet (L"AllowListenConnections2", true).AsBool ())
	{
		_wfp_createfilter (L"BlockListenConnectionsV4", nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_LISTEN_V4, nullptr, FWP_ACTION_BLOCK, 0, &filter_ids);
		_wfp_createfilter (L"BlockListenConnectionsV6", nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_LISTEN_V6, nullptr, FWP_ACTION_BLOCK, 0, &filter_ids);
	}

	// install boot-time filters (enforced at boot-time, even before "base filtering engine" service starts)
	if (app.ConfigGet (L"InstallBoottimeFilters", false).AsBool ())
	{
		fwfc[0].fieldKey = FWPM_CONDITION_FLAGS;
		fwfc[0].matchType = FWP_MATCH_FLAGS_ALL_SET;
		fwfc[0].conditionValue.type = FWP_UINT32;
		fwfc[0].conditionValue.uint32 = FWP_CONDITION_FLAG_IS_LOOPBACK;

		// tests if the network traffic is (non-)app container loopback traffic (win8+)
		if (_r_sys_validversion (6, 2))
		{
			fwfc[0].matchType = FWP_MATCH_FLAGS_ANY_SET;
			fwfc[0].conditionValue.uint32 |= FWP_CONDITION_FLAG_IS_APPCONTAINER_LOOPBACK;
		}

		_wfp_createfilter (BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_IPFORWARD_V4, nullptr, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);
		_wfp_createfilter (BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_IPFORWARD_V6, nullptr, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);

		_wfp_createfilter (BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, nullptr, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);
		_wfp_createfilter (BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);

		_wfp_createfilter (BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_INBOUND_ICMP_ERROR_V4, nullptr, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);
		_wfp_createfilter (BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_INBOUND_ICMP_ERROR_V6, nullptr, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);

		_wfp_createfilter (BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V4, nullptr, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);
		_wfp_createfilter (BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_HIGHEST_IMPORTANT, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V6, nullptr, FWP_ACTION_PERMIT, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);

		_wfp_createfilter (BOOTTIME_FILTER_NAME, nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4, nullptr, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);
		_wfp_createfilter (BOOTTIME_FILTER_NAME, nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6, nullptr, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);

		_wfp_createfilter (BOOTTIME_FILTER_NAME, nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_INBOUND_ICMP_ERROR_V4, nullptr, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);
		_wfp_createfilter (BOOTTIME_FILTER_NAME, nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_INBOUND_ICMP_ERROR_V6, nullptr, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);

		_wfp_createfilter (BOOTTIME_FILTER_NAME, nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V4, nullptr, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);
		_wfp_createfilter (BOOTTIME_FILTER_NAME, nullptr, 0, FILTER_WEIGHT_LOWEST, &FWPM_LAYER_OUTBOUND_ICMP_ERROR_V6, nullptr, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);

		// win7+ boot-time features
		fwfc[0].fieldKey = FWPM_CONDITION_FLAGS;
		fwfc[0].matchType = FWP_MATCH_FLAGS_NONE_SET;
		fwfc[0].conditionValue.type = FWP_UINT32;
		fwfc[0].conditionValue.uint32 = FWP_CONDITION_FLAG_IS_OUTBOUND_PASS_THRU | FWP_CONDITION_FLAG_IS_INBOUND_PASS_THRU;

		_wfp_createfilter (BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_APPLICATION, &FWPM_LAYER_IPFORWARD_V4, nullptr, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);
		_wfp_createfilter (BOOTTIME_FILTER_NAME, fwfc, 1, FILTER_WEIGHT_APPLICATION, &FWPM_LAYER_IPFORWARD_V6, nullptr, FWP_ACTION_BLOCK, FWPM_FILTER_FLAG_BOOTTIME, &filter_ids);
	}

	if (!is_intransact)
	{
		_wfp_transact_commit (line);

		const bool is_secure = app.ConfigGet (L"IsSecureFilters", false).AsBool ();

		if (is_secure ? config.pacl_secure : config.pacl_default)
		{
			for (size_t i = 0; i < filter_ids.size (); i++)
				_wfp_setfiltersecurity (config.hengine, &filter_ids.at (i), config.pusersid, is_secure ? config.pacl_secure : config.pacl_default, line);
		}

		_r_fastlock_releaseshared (&lock_transaction);
	}

	_app_restoreinterfacestate (app.GetHWND (), is_enabled);

	return true;
}

void _wfp_setfiltersecurity (HANDLE engineHandle, const GUID * pfilter_id, const PSID psid, PACL pacl, UINT line)
{
	if (!engineHandle || !pfilter_id || *pfilter_id == GUID_NULL || (!psid && !pacl))
		return;

	if (psid)
	{
		const DWORD result = FwpmFilterSetSecurityInfoByKey (engineHandle, pfilter_id, OWNER_SECURITY_INFORMATION, (const SID*)psid, nullptr, nullptr, nullptr);

		if (result != ERROR_SUCCESS)
			_app_logerror (L"FwpmFilterSetSecurityInfoByKey", result, _r_fmt (L"#%d", line), true);
	}

	if (pacl)
	{
		const DWORD result = FwpmFilterSetSecurityInfoByKey (engineHandle, pfilter_id, DACL_SECURITY_INFORMATION, nullptr, nullptr, pacl, nullptr);

		if (result != ERROR_SUCCESS)
			_app_logerror (L"FwpmFilterSetSecurityInfoByKey", result, _r_fmt (L"#%d", line), true);
	}
}

size_t _wfp_dumpfilters (const GUID * pprovider, GUIDS_VEC * ptr_filters)
{
	if (!config.hengine || !pprovider || !ptr_filters)
		return 0;

	ptr_filters->clear ();

	UINT32 count = 0;
	HANDLE henum = nullptr;

	DWORD result = FwpmFilterCreateEnumHandle (config.hengine, nullptr, &henum);

	if (result != ERROR_SUCCESS)
	{
		_app_logerror (L"FwpmFilterCreateEnumHandle", result, nullptr, false);
		return 0;
	}
	else
	{
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
					if (matchingFwpFilter[i] && matchingFwpFilter[i]->providerKey && memcmp (matchingFwpFilter[i]->providerKey, pprovider, sizeof (GUID)) == 0)
						ptr_filters->push_back (matchingFwpFilter[i]->filterKey);
				}

				FwpmFreeMemory ((void**)& matchingFwpFilter);
			}
			else
			{
				count = 0;
			}
		}
	}

	if (henum)
		FwpmFilterDestroyEnumHandle (config.hengine, henum);

	return count;
}

bool _mps_firewallapi (bool* pis_enabled, const bool* pis_enable)
{
	if (!pis_enabled && !pis_enable)
		return false;

	bool result = false;

	const HRESULT hrComInit = CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED);

	if ((hrComInit == RPC_E_CHANGED_MODE) || SUCCEEDED (hrComInit))
	{
		INetFwPolicy2* pNetFwPolicy2 = nullptr;
		HRESULT hr = CoCreateInstance (__uuidof (NetFwPolicy2), nullptr, CLSCTX_INPROC_SERVER, __uuidof (INetFwPolicy2), (void**)& pNetFwPolicy2);

		if (SUCCEEDED (hr) && pNetFwPolicy2)
		{
			static const NET_FW_PROFILE_TYPE2 profileTypes[] = {
				NET_FW_PROFILE2_DOMAIN,
				NET_FW_PROFILE2_PRIVATE,
				NET_FW_PROFILE2_PUBLIC
			};

			if (pis_enabled)
			{
				*pis_enabled = false;

				for (size_t i = 0; i < _countof (profileTypes); i++)
				{
					VARIANT_BOOL bIsEnabled = FALSE;

					hr = pNetFwPolicy2->get_FirewallEnabled (profileTypes[i], &bIsEnabled);

					if (SUCCEEDED (hr))
					{
						result = true;

						if (bIsEnabled == VARIANT_TRUE)
						{
							*pis_enabled = true;
							break;
						}
					}
				}
			}

			if (pis_enable)
			{
				for (size_t i = 0; i < _countof (profileTypes); i++)
				{
					hr = pNetFwPolicy2->put_FirewallEnabled (profileTypes[i], *pis_enable ? VARIANT_TRUE : VARIANT_FALSE);

					if (SUCCEEDED (hr))
						result = true;

					else
						_app_logerror (L"put_FirewallEnabled", hr, _r_fmt (L"%d", profileTypes[i]), true);
				}
			}
		}
		else
		{
			_app_logerror (L"CoCreateInstance", hr, L"INetFwPolicy2", true);
		}

		if (pNetFwPolicy2)
			pNetFwPolicy2->Release ();

		if (SUCCEEDED (hrComInit))
			CoUninitialize ();
	}

	return result;
}

void _mps_changeconfig2 (bool is_enable)
{
	// check settings
	bool is_wfenabled = false;
	_mps_firewallapi (&is_wfenabled, nullptr);

	if (is_wfenabled == is_enable)
		return;

	const SC_HANDLE scm = OpenSCManager (nullptr, nullptr, SC_MANAGER_ALL_ACCESS);

	if (!scm)
	{
		_app_logerror (L"OpenSCManager", GetLastError (), nullptr, true);
	}
	else
	{
		static LPCWSTR arr[] = {
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

				if (!ChangeServiceConfig (sc, SERVICE_NO_CHANGE, is_enable ? SERVICE_AUTO_START : SERVICE_DISABLED, SERVICE_NO_CHANGE, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr))
					_app_logerror (L"ChangeServiceConfig", GetLastError (), arr[i], true);

				CloseServiceHandle (sc);
			}
		}

		// start services
		if (is_enable)
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

					if (!QueryServiceStatusEx (sc, SC_STATUS_PROCESS_INFO, (LPBYTE)& ssp, sizeof (ssp), &dwBytesNeeded))
					{
						_app_logerror (L"QueryServiceStatusEx", GetLastError (), arr[i], false);
					}
					else
					{
						if (ssp.dwCurrentState != SERVICE_RUNNING)
						{
							if (!StartService (sc, 0, nullptr))
								_app_logerror (L"StartService", GetLastError (), arr[i], false);
						}

						CloseServiceHandle (sc);
					}
				}
			}

			_r_sleep (250);

		}

		_mps_firewallapi (nullptr, &is_enable);

		CloseServiceHandle (scm);
	}
}

DWORD _FwpmGetAppIdFromFileName1 (LPCWSTR path, FWP_BYTE_BLOB * *lpblob, EnumDataType type)
{
	if (!path || !path[0] || !lpblob)
		return ERROR_BAD_ARGUMENTS;

	rstring path_buff;

	if (type == DataAppRegular || type == DataAppNetwork || type == DataAppService)
	{
		path_buff = path;

		if (_r_str_hash (path) == config.ntoskrnl_hash)
		{
			if (ByteBlobAlloc ((LPVOID)path_buff.GetString (), (path_buff.GetLength () + 1) * sizeof (WCHAR), lpblob))
				return ERROR_SUCCESS;
		}
		else
		{
			DWORD result = _r_path_ntpathfromdos (path_buff);

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

			if (ByteBlobAlloc ((LPVOID)path_buff.GetString (), (path_buff.GetLength () + 1) * sizeof (WCHAR), lpblob))
				return ERROR_SUCCESS;
		}
	}
	else if (type == DataAppPico || type == DataAppDevice)
	{
		path_buff = path;

		if (type == DataAppDevice)
			path_buff.ToLower (); // lower is important!

		if (ByteBlobAlloc ((LPVOID)path_buff.GetString (), (path_buff.GetLength () + 1) * sizeof (WCHAR), lpblob))
			return ERROR_SUCCESS;
	}

	return ERROR_FILE_NOT_FOUND;
}

bool ByteBlobAlloc (PVOID data, size_t length, FWP_BYTE_BLOB * *lpblob)
{
	if (!data || !length || !lpblob)
		return false;

	*lpblob = new FWP_BYTE_BLOB;

	if (*lpblob)
	{
		const PUINT8 tmp_ptr = new UINT8[length];

		(*lpblob)->data = tmp_ptr;
		(*lpblob)->size = (UINT32)length;

		CopyMemory ((*lpblob)->data, data, length);

		return true;
	}

	return false;
}

void ByteBlobFree (FWP_BYTE_BLOB * *lpblob)
{
	if (lpblob && *lpblob)
	{
		FWP_BYTE_BLOB* blob = *lpblob;

		if (blob)
		{
			SAFE_DELETE_ARRAY (blob->data);
			SAFE_DELETE (blob);

			*lpblob = nullptr;
		}
	}
}
