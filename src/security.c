// simplewall
// Copyright (c) 2020-2021 Henry++

#include "global.h"

_Ret_maybenull_
PSID _app_quyerybuiltinsid (_In_ WELL_KNOWN_SID_TYPE sid_type)
{
	PSID sid;
	ULONG sid_length;

	sid_length = SECURITY_MAX_SID_SIZE;
	sid = _r_mem_allocatezero (sid_length);

	if (!CreateWellKnownSid (sid_type, NULL, sid, &sid_length))
	{
		_r_log_v (LOG_LEVEL_ERROR, NULL, L"CreateWellKnownSid", GetLastError (), L"%" TEXT (PRIu32), sid_type);
	}
	else
	{
		return sid;
	}

	_r_mem_free (sid);

	return NULL;
}

VOID _app_generate_credentials ()
{
	R_STRINGREF service_name;

	// For revoke current user (v3.0.5 Beta and lower)
	if (!config.pbuiltin_current_sid)
		config.pbuiltin_current_sid = _r_sys_getcurrenttoken ().token_sid;

	// S-1-5-32-544 (BUILTIN\Administrators)
	if (!config.pbuiltin_admins_sid)
		config.pbuiltin_admins_sid = _app_quyerybuiltinsid (WinBuiltinAdministratorsSid);

	// S-1-5-32-556 (BUILTIN\Network Configuration Operators)
	if (!config.pbuiltin_netops_sid)
		config.pbuiltin_netops_sid = _app_quyerybuiltinsid (WinBuiltinNetworkConfigurationOperatorsSid);

	if (!config.pservice_mpssvc_sid)
	{
		_r_obj_initializestringref (&service_name, L"mpssvc");

		config.pservice_mpssvc_sid = _r_sys_getservicesid (&service_name);
	}

	if (!config.pservice_nlasvc_sid)
	{
		_r_obj_initializestringref (&service_name, L"NlaSvc");

		config.pservice_nlasvc_sid = _r_sys_getservicesid (&service_name);
	}

	if (!config.pservice_policyagent_sid)
	{
		_r_obj_initializestringref (&service_name, L"PolicyAgent");

		config.pservice_policyagent_sid = _r_sys_getservicesid (&service_name);
	}

	if (!config.pservice_rpcss_sid)
	{
		_r_obj_initializestringref (&service_name, L"RpcSs");

		config.pservice_rpcss_sid = _r_sys_getservicesid (&service_name);
	}

	if (!config.pservice_wdiservicehost_sid)
	{
		_r_obj_initializestringref (&service_name, L"WdiServiceHost");

		config.pservice_wdiservicehost_sid = _r_sys_getservicesid (&service_name);
	}
}

_Ret_maybenull_
PACL _app_createaccesscontrollist (_In_ PACL acl, _In_ BOOLEAN is_secure)
{
	BOOLEAN is_secured = FALSE;

	BOOLEAN is_currentuserhaverights = FALSE;
	BOOLEAN is_openforeveryone = FALSE;

	for (WORD ace_index = 0; ace_index < acl->AceCount; ace_index++)
	{
		PACCESS_ALLOWED_ACE ace = NULL;

		if (!GetAce (acl, ace_index, &ace))
			continue;

		if (ace->Header.AceType == ACCESS_ALLOWED_ACE_TYPE)
		{
			// versions of SW before v3.0.5 added Carte blanche for current user
			//
			// src: https://github.com/henrypp/simplewall/blob/v.2.3.12/src/main.cpp#L8273
			// src: https://github.com/henrypp/simplewall/blob/v.2.3.13/src/main.cpp#L8354
			// src: https://github.com/henrypp/simplewall/blob/v.2.4.6/src/main.cpp#L8828
			// src: https://github.com/henrypp/simplewall/blob/v.3.0.5/src/wfp.cpp#L109
			if (RtlEqualSid (&ace->SidStart, config.pbuiltin_current_sid))
			{
				if ((ace->Mask & (FWPM_GENERIC_EXECUTE | FWPM_GENERIC_WRITE | DELETE | WRITE_DAC | WRITE_OWNER)) != 0)
					is_currentuserhaverights = TRUE;
			}

			// versions of SW before v3.1.1 added Carte blanche for Everyone
			//
			// src: https://github.com/henrypp/simplewall/blob/v.2.4.6/src/main.cpp#L8833
			// src: https://github.com/henrypp/simplewall/blob/v.3.0.5/src/wfp.cpp#L114
			// src: https://github.com/henrypp/simplewall/blob/v.3.1.1/src/wfp.cpp#L150
			else if (RtlEqualSid (&ace->SidStart, &SeEveryoneSid))
			{
				if (((ace->Mask & ~(FWPM_ACTRL_CLASSIFY | FWPM_ACTRL_OPEN)) & (FWPM_GENERIC_EXECUTE | FWPM_GENERIC_READ)) != 0)
					is_openforeveryone = TRUE;
			}
		}
		else if (ace->Header.AceType == ACCESS_DENIED_ACE_TYPE)
		{
			if (RtlEqualSid (&ace->SidStart, &SeEveryoneSid))
			{
				if ((ace->Mask == (FWPM_ACTRL_WRITE | DELETE | WRITE_DAC | WRITE_OWNER)))
					is_secured = TRUE;
			}
		}
	}

	if (is_openforeveryone || is_currentuserhaverights || is_secured != is_secure)
	{
		EXPLICIT_ACCESS ea[3] = {0};
		PACL new_dacl = NULL;
		ULONG count = 0;

		// revoke current user access rights
		if (is_currentuserhaverights)
		{
			_app_setexplicitaccess (&ea[count++], REVOKE_ACCESS, 0, NO_INHERITANCE, config.pbuiltin_current_sid);
		}

		// revoke everyone access rights
		if (is_openforeveryone)
		{
			_app_setexplicitaccess (&ea[count++], REVOKE_ACCESS, 0, NO_INHERITANCE, &SeEveryoneSid);
		}

		// secure filter from deletion
		_app_setexplicitaccess (&ea[count++],
								is_secure ? DENY_ACCESS : GRANT_ACCESS,
								FWPM_ACTRL_WRITE | DELETE | WRITE_DAC | WRITE_OWNER,
								is_secure ? CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE : NO_INHERITANCE,
								&SeEveryoneSid
		);

		ULONG code = SetEntriesInAcl (count, ea, acl, &new_dacl);

		if (code == ERROR_SUCCESS)
			return new_dacl;

		_r_log (LOG_LEVEL_ERROR, NULL, L"SetEntriesInAcl", code, NULL);
	}

	return NULL;
}

VOID _app_setexplicitaccess (_Out_ PEXPLICIT_ACCESS ea, _In_ ACCESS_MODE mode, _In_ ULONG rights, _In_ ULONG inheritance, _In_opt_ PSID sid)
{
	ea->grfAccessMode = mode;
	ea->grfAccessPermissions = rights;
	ea->grfInheritance = inheritance;

	RtlZeroMemory (&(ea->Trustee), sizeof (ea->Trustee));

	BuildTrusteeWithSid (&(ea->Trustee), sid);
}

VOID _app_setsecurityinfoforengine (_In_ HANDLE hengine)
{
	PSECURITY_DESCRIPTOR security_descriptor;
	PSID sid_owner;
	PSID sid_group;
	PACL dacl;
	PACL sacl;
	ULONG code;
	BOOLEAN is_currentuserhaverights = FALSE;
	BOOLEAN is_openforeveryone = FALSE;

	code = FwpmEngineGetSecurityInfo (hengine, DACL_SECURITY_INFORMATION, &sid_owner, &sid_group, &dacl, &sacl, &security_descriptor);

	if (code != ERROR_SUCCESS)
	{
		_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmEngineGetSecurityInfo", code, NULL);
		return;
	}

	if (dacl)
	{
		for (WORD ace_index = 0; ace_index < dacl->AceCount; ace_index++)
		{
			PACCESS_ALLOWED_ACE ace = NULL;

			if (!GetAce (dacl, ace_index, &ace))
				continue;

			if (ace->Header.AceType != ACCESS_ALLOWED_ACE_TYPE)
				continue;

			// versions of SW before v3.0.5 added Carte blanche for current user
			//
			// src: https://github.com/henrypp/simplewall/blob/v.2.3.12/src/main.cpp#L8273
			// src: https://github.com/henrypp/simplewall/blob/v.2.3.13/src/main.cpp#L8354
			// src: https://github.com/henrypp/simplewall/blob/v.2.4.6/src/main.cpp#L8815
			// src: https://github.com/henrypp/simplewall/blob/v.3.0.5/src/wfp.cpp#L96
			if (RtlEqualSid (&ace->SidStart, config.pbuiltin_current_sid))
			{
				if ((ace->Mask == (FWPM_GENERIC_ALL | DELETE | WRITE_DAC | WRITE_OWNER)))
					is_currentuserhaverights = TRUE;
			}

			// versions of SW before v3.1.1 added Carte blanche for Everyone
			//
			// src: https://github.com/henrypp/simplewall/blob/v.2.4.6/src/main.cpp#L8820
			// src: https://github.com/henrypp/simplewall/blob/v.3.0.5/src/wfp.cpp#L101
			// src: https://github.com/henrypp/simplewall/blob/v.3.1.1/src/wfp.cpp#L137
			else if (RtlEqualSid (&ace->SidStart, &SeEveryoneSid))
			{
				if (((ace->Mask & ~(FWPM_ACTRL_CLASSIFY | FWPM_ACTRL_OPEN)) & (FWPM_GENERIC_ALL)) != 0)
					is_openforeveryone = TRUE;
			}
		}

		if (is_currentuserhaverights || is_openforeveryone)
		{
			FwpmEngineSetSecurityInfo (hengine, OWNER_SECURITY_INFORMATION, &SeLocalServiceSid, NULL, NULL, NULL);
			FwpmNetEventsSetSecurityInfo (hengine, OWNER_SECURITY_INFORMATION, &SeLocalServiceSid, NULL, NULL, NULL);

			EXPLICIT_ACCESS ea[18] = {0};
			PACL new_dacl = NULL;
			ULONG count = 0;

			// revoke current user access rights
			if (is_currentuserhaverights)
			{
				if (config.pbuiltin_current_sid)
					_app_setexplicitaccess (&ea[count++], REVOKE_ACCESS, 0, NO_INHERITANCE, config.pbuiltin_current_sid);
			}

			// reset default engine rights
			if (is_openforeveryone)
			{
				if (config.pbuiltin_admins_sid)
				{
					_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, FWPM_GENERIC_ALL | DELETE | WRITE_DAC | WRITE_OWNER, NO_INHERITANCE, config.pbuiltin_admins_sid);
					_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, 0x10000000, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE, config.pbuiltin_admins_sid);
				}

				if (config.pbuiltin_netops_sid)
				{
					_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, FWPM_GENERIC_ALL | DELETE, NO_INHERITANCE, config.pbuiltin_netops_sid);
					_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, 0xE0000000, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE, config.pbuiltin_netops_sid);
				}

				if (config.pservice_mpssvc_sid)
				{
					_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, FWPM_GENERIC_ALL | DELETE, NO_INHERITANCE, config.pservice_mpssvc_sid);
					_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, 0xE0000000, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE, config.pservice_mpssvc_sid);
				}

				if (config.pservice_nlasvc_sid)
				{
					_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, FWPM_GENERIC_READ | FWPM_GENERIC_EXECUTE, NO_INHERITANCE, config.pservice_nlasvc_sid);
					_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, 0xA0000000, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE, config.pservice_nlasvc_sid);
				}

				if (config.pservice_policyagent_sid)
				{
					_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, FWPM_GENERIC_ALL | DELETE, NO_INHERITANCE, config.pservice_policyagent_sid);
					_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, 0xE0000000, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE, config.pservice_policyagent_sid);
				}

				if (config.pservice_rpcss_sid)
				{
					_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, FWPM_GENERIC_ALL | DELETE, NO_INHERITANCE, config.pservice_rpcss_sid);
					_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, 0xE0000000, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE, config.pservice_rpcss_sid);
				}

				if (config.pservice_wdiservicehost_sid)
				{
					_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, FWPM_GENERIC_READ | FWPM_GENERIC_EXECUTE, NO_INHERITANCE, config.pservice_wdiservicehost_sid);
					_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, 0xA0000000, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE, config.pservice_wdiservicehost_sid);
				}

				_app_setexplicitaccess (&ea[count++], SET_ACCESS, FWPM_ACTRL_CLASSIFY | FWPM_ACTRL_OPEN, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE, &SeEveryoneSid);
			}

			code = SetEntriesInAcl (count, ea, dacl, &new_dacl);

			if (code != ERROR_SUCCESS)
			{
				_r_log (LOG_LEVEL_ERROR, NULL, L"SetEntriesInAcl", code, NULL);
			}
			else
			{
				code = FwpmEngineSetSecurityInfo (hengine, DACL_SECURITY_INFORMATION, NULL, NULL, new_dacl, NULL);

				if (code != ERROR_SUCCESS)
					_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmEngineSetSecurityInfo", code, NULL);

				code = FwpmNetEventsSetSecurityInfo (hengine, DACL_SECURITY_INFORMATION, NULL, NULL, new_dacl, NULL);

				if (code != ERROR_SUCCESS)
					_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmNetEventsSetSecurityInfo", code, NULL);

				LocalFree (new_dacl);
			}
		}
	}

	if (security_descriptor)
		FwpmFreeMemory ((PVOID_PTR)&security_descriptor);
}

VOID _app_setsecurityinfoforprovider (_In_ HANDLE hengine, _In_ LPCGUID provider_guid, _In_ BOOLEAN is_secure)
{
	PSECURITY_DESCRIPTOR security_descriptor;
	PSID sid_owner;
	PSID sid_group;
	PACL dacl;
	PACL sacl;
	PACL new_dacl;
	ULONG code;

	code = FwpmProviderGetSecurityInfoByKey (hengine, provider_guid, DACL_SECURITY_INFORMATION, &sid_owner, &sid_group, &dacl, &sacl, &security_descriptor);

	if (code != ERROR_SUCCESS)
	{
		_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmProviderGetSecurityInfoByKey", code, NULL);
		return;
	}

	if (dacl)
	{
		new_dacl = _app_createaccesscontrollist (dacl, is_secure);

		if (new_dacl)
		{
			code = FwpmProviderSetSecurityInfoByKey (hengine, provider_guid, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, (const SID *)config.pbuiltin_admins_sid, NULL, new_dacl, NULL);

			if (code != ERROR_SUCCESS)
				_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmProviderSetSecurityInfoByKey", code, NULL);

			LocalFree (new_dacl);
		}
	}

	if (security_descriptor)
		FwpmFreeMemory ((PVOID_PTR)&security_descriptor);
}

VOID _app_setsecurityinfoforsublayer (_In_ HANDLE hengine, _In_ LPCGUID sublayer_guid, _In_ BOOLEAN is_secure)
{
	PSECURITY_DESCRIPTOR security_descriptor;
	PSID sid_owner;
	PSID sid_group;
	PACL dacl;
	PACL sacl;
	PACL new_dacl;
	ULONG code;

	code = FwpmSubLayerGetSecurityInfoByKey (hengine, sublayer_guid, DACL_SECURITY_INFORMATION, &sid_owner, &sid_group, &dacl, &sacl, &security_descriptor);

	if (code != ERROR_SUCCESS)
	{
		_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmSubLayerGetSecurityInfoByKey", code, NULL);
		return;
	}

	if (dacl)
	{
		new_dacl = _app_createaccesscontrollist (dacl, is_secure);

		if (new_dacl)
		{
			code = FwpmSubLayerSetSecurityInfoByKey (hengine, sublayer_guid, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, (const SID *)config.pbuiltin_admins_sid, NULL, new_dacl, NULL);

			if (code != ERROR_SUCCESS)
				_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmSubLayerSetSecurityInfoByKey", code, NULL);

			LocalFree (new_dacl);
		}
	}

	if (security_descriptor)
		FwpmFreeMemory ((PVOID_PTR)&security_descriptor);
}

VOID _app_setsecurityinfoforcallout (_In_ HANDLE hengine, _In_ LPCGUID callout_guid, _In_ BOOLEAN is_secure)
{
	PSECURITY_DESCRIPTOR security_descriptor;
	PSID sid_owner;
	PSID sid_group;
	PACL dacl;
	PACL sacl;
	PACL new_dacl;
	ULONG code;

	code = FwpmCalloutGetSecurityInfoByKey (hengine, callout_guid, DACL_SECURITY_INFORMATION, &sid_owner, &sid_group, &dacl, &sacl, &security_descriptor);

	if (code != ERROR_SUCCESS)
	{
		if (code != FWP_E_CALLOUT_NOT_FOUND)
			_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmCalloutGetSecurityInfoByKey", code, NULL);

		return;
	}

	if (dacl)
	{
		new_dacl = _app_createaccesscontrollist (dacl, is_secure);

		if (new_dacl)
		{
			code = FwpmCalloutSetSecurityInfoByKey (hengine, callout_guid, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, (const SID *)config.pbuiltin_admins_sid, NULL, new_dacl, NULL);

			if (code != ERROR_SUCCESS)
				_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmCalloutSetSecurityInfoByKey", code, NULL);

			LocalFree (new_dacl);
		}
	}

	if (security_descriptor)
		FwpmFreeMemory ((PVOID_PTR)&security_descriptor);
}

VOID _app_setsecurityinfoforfilter (_In_ HANDLE hengine, _In_ LPCGUID filter_guid, _In_ BOOLEAN is_secure, _In_ UINT line)
{
	PSECURITY_DESCRIPTOR security_descriptor;
	PSID sid_owner;
	PSID sid_group;
	PACL dacl;
	PACL sacl;
	PACL new_dacl;
	ULONG code;

	code = FwpmFilterGetSecurityInfoByKey (hengine, filter_guid, DACL_SECURITY_INFORMATION, &sid_owner, &sid_group, &dacl, &sacl, &security_descriptor);

	if (code != ERROR_SUCCESS)
	{
#if !defined(_DEBUG)
		if (code != FWP_E_FILTER_NOT_FOUND)
#endif // !DEBUG
		{
			_r_log_v (LOG_LEVEL_ERROR, NULL, L"FwpmFilterSetSecurityInfoByKey", code, L"#%" TEXT (PRIu32), line);
		}

		return;
	}

	if (dacl)
	{
		new_dacl = _app_createaccesscontrollist (dacl, is_secure);

		if (new_dacl)
		{
			code = FwpmFilterSetSecurityInfoByKey (hengine, filter_guid, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, (const SID *)config.pbuiltin_admins_sid, NULL, new_dacl, NULL);

			if (code != ERROR_SUCCESS)
				_r_log_v (LOG_LEVEL_ERROR, NULL, L"FwpmFilterSetSecurityInfoByKey", code, L"#%" TEXT (PRIu32), line);

			LocalFree (new_dacl);
		}
	}

	if (security_descriptor)
		FwpmFreeMemory ((PVOID_PTR)&security_descriptor);
}
