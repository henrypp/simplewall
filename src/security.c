// simplewall
// Copyright (c) 2020-2021 Henry++

#include "global.h"

_Ret_maybenull_
PSID _app_quyerybuiltinsid (_In_ WELL_KNOWN_SID_TYPE sid_type)
{
	ULONG sid_length = SECURITY_MAX_SID_SIZE;
	PSID psid = _r_mem_allocatezero (sid_length);

	if (CreateWellKnownSid (sid_type, NULL, psid, &sid_length))
		return psid;

	_r_mem_free (psid);

	return NULL;
}

_Ret_maybenull_
PSID _app_queryservicesid (_In_ LPCWSTR name)
{
	UNICODE_STRING service_name;
	RtlInitUnicodeString (&service_name, name);

	ULONG sid_length = 0;
	PSID psid = NULL;

	if (RtlCreateServiceSid (&service_name, psid, &sid_length) == STATUS_BUFFER_TOO_SMALL)
	{
		psid = _r_mem_allocatezero (sid_length);

		if (NT_SUCCESS (RtlCreateServiceSid (&service_name, psid, &sid_length)))
			return psid;

		_r_mem_free (psid);
	}

	return NULL;
}

VOID _app_generate_credentials ()
{
	// For revoke current user (v3.0.5 Beta and lower)
	if (!config.pbuiltin_current_sid)
	{
		// get user sid
		HANDLE htoken;

		if (OpenProcessToken (NtCurrentProcess (), TOKEN_QUERY, &htoken))
		{
			ULONG token_length = 0;
			GetTokenInformation (htoken, TokenUser, NULL, 0, &token_length);

			if (GetLastError () == ERROR_INSUFFICIENT_BUFFER)
			{
				PTOKEN_USER token_user = _r_mem_allocatezero (token_length);

				if (GetTokenInformation (htoken, TokenUser, token_user, token_length, &token_length))
				{
					if (RtlValidSid (token_user->User.Sid))
					{
						ULONG sid_length = RtlLengthSid (token_user->User.Sid);
						config.pbuiltin_current_sid = _r_mem_allocatezero (sid_length);

						memcpy (config.pbuiltin_current_sid, token_user->User.Sid, sid_length);
					}
				}

				_r_mem_free (token_user);
			}

			CloseHandle (htoken);
		}
	}

	// S-1-1-0 (Everyone)
	if (!config.pbuiltin_world_sid)
		config.pbuiltin_world_sid = _app_quyerybuiltinsid (WinWorldSid);

	// S-1-5-19 (NT AUTHORITY\LOCAL SERVICE)
	if (!config.pbuiltin_localservice_sid)
		config.pbuiltin_localservice_sid = _app_quyerybuiltinsid (WinLocalServiceSid);

	// S-1-5-32-544 (BUILTIN\Administrators)
	if (!config.pbuiltin_admins_sid)
		config.pbuiltin_admins_sid = _app_quyerybuiltinsid (WinBuiltinAdministratorsSid);

	// S-1-5-32-556 (BUILTIN\Network Configuration Operators)
	if (!config.pbuiltin_netops_sid)
		config.pbuiltin_netops_sid = _app_quyerybuiltinsid (WinBuiltinNetworkConfigurationOperatorsSid);

	// Query services security
	if (!config.pservice_mpssvc_sid)
		config.pservice_mpssvc_sid = _app_queryservicesid (L"mpssvc");

	if (!config.pservice_nlasvc_sid)
		config.pservice_nlasvc_sid = _app_queryservicesid (L"NlaSvc");

	if (!config.pservice_policyagent_sid)
		config.pservice_policyagent_sid = _app_queryservicesid (L"PolicyAgent");

	if (!config.pservice_rpcss_sid)
		config.pservice_rpcss_sid = _app_queryservicesid (L"RpcSs");

	if (!config.pservice_wdiservicehost_sid)
		config.pservice_wdiservicehost_sid = _app_queryservicesid (L"WdiServiceHost");
}

_Ret_maybenull_
PACL _app_createaccesscontrollist (_In_ PACL pacl, _In_ BOOLEAN is_secure)
{
	BOOLEAN is_secured = FALSE;

	BOOLEAN is_currentuserhaverights = FALSE;
	BOOLEAN is_openforeveryone = FALSE;

	for (WORD ace_index = 0; ace_index < pacl->AceCount; ace_index++)
	{
		PACCESS_ALLOWED_ACE pace = NULL;

		if (!GetAce (pacl, ace_index, &pace))
			continue;

		if (pace->Header.AceType == ACCESS_ALLOWED_ACE_TYPE)
		{
			// versions of SW before v3.0.5 added Carte blanche for current user
			//
			// src: https://github.com/henrypp/simplewall/blob/v.2.3.12/src/main.cpp#L8273
			// src: https://github.com/henrypp/simplewall/blob/v.2.3.13/src/main.cpp#L8354
			// src: https://github.com/henrypp/simplewall/blob/v.2.4.6/src/main.cpp#L8828
			// src: https://github.com/henrypp/simplewall/blob/v.3.0.5/src/wfp.cpp#L109
			if (RtlEqualSid (&pace->SidStart, config.pbuiltin_current_sid) && (pace->Mask & (FWPM_GENERIC_EXECUTE | FWPM_GENERIC_WRITE | DELETE | WRITE_DAC | WRITE_OWNER)) != 0)
				is_currentuserhaverights = TRUE;

			// versions of SW before v3.1.1 added Carte blanche for Everyone
			//
			// src: https://github.com/henrypp/simplewall/blob/v.2.4.6/src/main.cpp#L8833
			// src: https://github.com/henrypp/simplewall/blob/v.3.0.5/src/wfp.cpp#L114
			// src: https://github.com/henrypp/simplewall/blob/v.3.1.1/src/wfp.cpp#L150
			else if (RtlEqualSid (&pace->SidStart, config.pbuiltin_world_sid) && ((pace->Mask & ~(FWPM_ACTRL_CLASSIFY | FWPM_ACTRL_OPEN)) & (FWPM_GENERIC_EXECUTE | FWPM_GENERIC_READ)) != 0)
				is_openforeveryone = TRUE;
		}
		else if (pace->Header.AceType == ACCESS_DENIED_ACE_TYPE)
		{
			if (RtlEqualSid (&pace->SidStart, config.pbuiltin_world_sid) && (pace->Mask == (FWPM_ACTRL_WRITE | DELETE | WRITE_DAC | WRITE_OWNER)))
				is_secured = TRUE;
		}
	}

	if (is_openforeveryone || is_currentuserhaverights || is_secured != is_secure)
	{
		PACL pnewdacl = NULL;
		ULONG count = 0;
		EXPLICIT_ACCESS ea[3];

		// revoke current user access rights
		if (is_currentuserhaverights)
		{
			_app_setexplicitaccess (&ea[count++], REVOKE_ACCESS, 0, NO_INHERITANCE, config.pbuiltin_current_sid);
		}

		// revoke everyone access rights
		if (is_openforeveryone)
		{
			_app_setexplicitaccess (&ea[count++], REVOKE_ACCESS, 0, NO_INHERITANCE, config.pbuiltin_world_sid);
		}

		// secure filter from deletion
		_app_setexplicitaccess (&ea[count++],
								is_secure ? DENY_ACCESS : GRANT_ACCESS,
								FWPM_ACTRL_WRITE | DELETE | WRITE_DAC | WRITE_OWNER,
								is_secure ? CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE : NO_INHERITANCE,
								config.pbuiltin_world_sid
		);

		ULONG code = SetEntriesInAcl (count, ea, pacl, &pnewdacl);

		if (code == ERROR_SUCCESS)
			return pnewdacl;

		_r_log (LOG_LEVEL_ERROR, NULL, L"SetEntriesInAcl", code, NULL);
	}

	return NULL;
}

VOID _app_setexplicitaccess (_Out_ PEXPLICIT_ACCESS pea, _In_ ACCESS_MODE mode, _In_ ULONG rights, _In_ ULONG inheritance, _In_opt_ PSID psid)
{
	pea->grfAccessMode = mode;
	pea->grfAccessPermissions = rights;
	pea->grfInheritance = inheritance;

	memset (&(pea->Trustee), 0, sizeof (pea->Trustee));

	BuildTrusteeWithSid (&(pea->Trustee), psid);
}

VOID _app_setsecurityinfoforengine (_In_ HANDLE hengine)
{
	PSECURITY_DESCRIPTOR security_descriptor;
	PSID psid_owner;
	PSID psid_group;
	PACL pdacl;
	PACL psacl;
	ULONG code;
	BOOLEAN is_currentuserhaverights = FALSE;
	BOOLEAN is_openforeveryone = FALSE;

	code = FwpmEngineGetSecurityInfo (hengine, DACL_SECURITY_INFORMATION, &psid_owner, &psid_group, &pdacl, &psacl, &security_descriptor);

	if (code != ERROR_SUCCESS)
	{
		_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmEngineGetSecurityInfo", code, NULL);
		return;
	}

	if (!pdacl)
		return;

	for (WORD ace_index = 0; ace_index < pdacl->AceCount; ace_index++)
	{
		PACCESS_ALLOWED_ACE pace = NULL;

		if (!GetAce (pdacl, ace_index, &pace))
			continue;

		if (pace->Header.AceType != ACCESS_ALLOWED_ACE_TYPE)
			continue;

		// versions of SW before v3.0.5 added Carte blanche for current user
		//
		// src: https://github.com/henrypp/simplewall/blob/v.2.3.12/src/main.cpp#L8273
		// src: https://github.com/henrypp/simplewall/blob/v.2.3.13/src/main.cpp#L8354
		// src: https://github.com/henrypp/simplewall/blob/v.2.4.6/src/main.cpp#L8815
		// src: https://github.com/henrypp/simplewall/blob/v.3.0.5/src/wfp.cpp#L96
		if (RtlEqualSid (&pace->SidStart, config.pbuiltin_current_sid) && (pace->Mask == (FWPM_GENERIC_ALL | DELETE | WRITE_DAC | WRITE_OWNER)))
			is_currentuserhaverights = TRUE;

		// versions of SW before v3.1.1 added Carte blanche for Everyone
		//
		// src: https://github.com/henrypp/simplewall/blob/v.2.4.6/src/main.cpp#L8820
		// src: https://github.com/henrypp/simplewall/blob/v.3.0.5/src/wfp.cpp#L101
		// src: https://github.com/henrypp/simplewall/blob/v.3.1.1/src/wfp.cpp#L137
		if (RtlEqualSid (&pace->SidStart, config.pbuiltin_world_sid) && ((pace->Mask & ~(FWPM_ACTRL_CLASSIFY | FWPM_ACTRL_OPEN)) & (FWPM_GENERIC_ALL)) != 0)
			is_openforeveryone = TRUE;
	}

	if (is_currentuserhaverights || is_openforeveryone)
	{
		FwpmEngineSetSecurityInfo (hengine, OWNER_SECURITY_INFORMATION, (const SID*)config.pbuiltin_localservice_sid, NULL, NULL, NULL);
		FwpmNetEventsSetSecurityInfo (hengine, OWNER_SECURITY_INFORMATION, (const SID*)config.pbuiltin_localservice_sid, NULL, NULL, NULL);

		PACL pnewdacl = NULL;
		ULONG count = 0;

		EXPLICIT_ACCESS ea[18];

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

			if (config.pbuiltin_world_sid)
				_app_setexplicitaccess (&ea[count++], SET_ACCESS, FWPM_ACTRL_CLASSIFY | FWPM_ACTRL_OPEN, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE, config.pbuiltin_world_sid);
		}

		code = SetEntriesInAcl (count, ea, pdacl, &pnewdacl);

		if (code != ERROR_SUCCESS)
		{
			_r_log (LOG_LEVEL_ERROR, NULL, L"SetEntriesInAcl", code, NULL);
		}
		else
		{
			code = FwpmEngineSetSecurityInfo (hengine, DACL_SECURITY_INFORMATION, NULL, NULL, pnewdacl, NULL);

			if (code != ERROR_SUCCESS)
				_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmEngineSetSecurityInfo", code, NULL);

			code = FwpmNetEventsSetSecurityInfo (hengine, DACL_SECURITY_INFORMATION, NULL, NULL, pnewdacl, NULL);

			if (code != ERROR_SUCCESS)
				_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmEngineSetSecurityInfo", code, NULL);

			LocalFree (pnewdacl);
		}
	}

	if (security_descriptor)
		FwpmFreeMemory ((PVOID*)&security_descriptor);
}

VOID _app_setsecurityinfoforprovider (_In_ HANDLE hengine, _In_ LPCGUID lpguid, _In_ BOOLEAN is_secure)
{
	PSECURITY_DESCRIPTOR security_descriptor;
	PSID psid_owner;
	PSID psid_group;
	PACL pdacl;
	PACL psacl;
	PACL pnewdacl;
	ULONG code;

	code = FwpmProviderGetSecurityInfoByKey (hengine, lpguid, DACL_SECURITY_INFORMATION, &psid_owner, &psid_group, &pdacl, &psacl, &security_descriptor);

	if (code != ERROR_SUCCESS)
	{
		_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmProviderGetSecurityInfoByKey", code, NULL);
		return;
	}

	if (pdacl)
	{
		pnewdacl = _app_createaccesscontrollist (pdacl, is_secure);

		if (pnewdacl)
		{
			code = FwpmProviderSetSecurityInfoByKey (hengine, lpguid, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, (const SID*)config.pbuiltin_admins_sid, NULL, pnewdacl, NULL);

			if (code != ERROR_SUCCESS)
				_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmProviderSetSecurityInfoByKey", code, NULL);

			LocalFree (pnewdacl);
		}
	}

	if (security_descriptor)
		FwpmFreeMemory ((PVOID*)&security_descriptor);
}

VOID _app_setsecurityinfoforsublayer (_In_ HANDLE hengine, _In_ LPCGUID lpguid, _In_ BOOLEAN is_secure)
{
	PSECURITY_DESCRIPTOR security_descriptor;
	PSID psid_owner;
	PSID psid_group;
	PACL pdacl;
	PACL psacl;
	PACL pnewdacl;
	ULONG code;

	code = FwpmSubLayerGetSecurityInfoByKey (hengine, lpguid, DACL_SECURITY_INFORMATION, &psid_owner, &psid_group, &pdacl, &psacl, &security_descriptor);

	if (code != ERROR_SUCCESS)
	{
		_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmSubLayerGetSecurityInfoByKey", code, NULL);
		return;
	}

	if (pdacl)
	{
		pnewdacl = _app_createaccesscontrollist (pdacl, is_secure);

		if (pnewdacl)
		{
			code = FwpmSubLayerSetSecurityInfoByKey (hengine, lpguid, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, (const SID*)config.pbuiltin_admins_sid, NULL, pnewdacl, NULL);

			if (code != ERROR_SUCCESS)
				_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmSubLayerSetSecurityInfoByKey", code, NULL);

			LocalFree (pnewdacl);
		}
	}

	if (security_descriptor)
		FwpmFreeMemory ((PVOID*)&security_descriptor);
}

VOID _app_setsecurityinfoforfilter (_In_ HANDLE hengine, _In_ LPCGUID lpguid, _In_ BOOLEAN is_secure, _In_ UINT line)
{
	PSECURITY_DESCRIPTOR security_descriptor;
	PSID psid_owner;
	PSID psid_group;
	PACL pdacl;
	PACL psacl;
	PACL pnewdacl;
	ULONG code;

	code = FwpmFilterGetSecurityInfoByKey (hengine, lpguid, DACL_SECURITY_INFORMATION, &psid_owner, &psid_group, &pdacl, &psacl, &security_descriptor);

	if (code != ERROR_SUCCESS)
	{
#if !defined(_DEBUG)
		if (code != FWP_E_FILTER_NOT_FOUND)
#endif // !DEBUG
		{
			_r_log_v (LOG_LEVEL_ERROR, NULL, L"FwpmFilterSetSecurityInfoByKey", code, L"#%" PRIu32, line);
		}

		return;
	}

	if (pdacl)
	{
		pnewdacl = _app_createaccesscontrollist (pdacl, is_secure);

		if (pnewdacl)
		{
			code = FwpmFilterSetSecurityInfoByKey (hengine, lpguid, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, (const SID*)config.pbuiltin_admins_sid, NULL, pnewdacl, NULL);

			if (code != ERROR_SUCCESS)
				_r_log_v (LOG_LEVEL_ERROR, NULL, L"FwpmFilterSetSecurityInfoByKey", code, L"#%" PRIu32, line);

			LocalFree (pnewdacl);
		}
	}

	if (security_descriptor)
		FwpmFreeMemory ((PVOID*)&security_descriptor);
}
