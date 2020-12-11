// simplewall
// Copyright (c) 2020-2021 Henry++

#include "global.h"

PSID _app_quyerybuiltinsid (WELL_KNOWN_SID_TYPE sid_type)
{
	ULONG sid_length = SECURITY_MAX_SID_SIZE;
	PSID psid = _r_mem_allocatezero (sid_length);

	if (CreateWellKnownSid (sid_type, NULL, psid, &sid_length))
		return psid;

	_r_mem_free (psid);

	return NULL;
}

PSID _app_queryservicesid (LPCWSTR name)
{
	UNICODE_STRING service_name;
	RtlInitUnicodeString (&service_name, (LPWSTR)name);

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

PACL _app_createaccesscontrollist (PACL pacl, BOOLEAN is_secure)
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
			if (EqualSid (&pace->SidStart, config.pbuiltin_current_sid) && (pace->Mask & (FWPM_GENERIC_EXECUTE | FWPM_GENERIC_WRITE | DELETE | WRITE_DAC | WRITE_OWNER)) != 0)
				is_currentuserhaverights = TRUE;

			// versions of SW before v3.1.1 added Carte blanche for Everyone
			//
			// src: https://github.com/henrypp/simplewall/blob/v.2.4.6/src/main.cpp#L8833
			// src: https://github.com/henrypp/simplewall/blob/v.3.0.5/src/wfp.cpp#L114
			// src: https://github.com/henrypp/simplewall/blob/v.3.1.1/src/wfp.cpp#L150
			else if (EqualSid (&pace->SidStart, config.pbuiltin_world_sid) && ((pace->Mask & ~(FWPM_ACTRL_CLASSIFY | FWPM_ACTRL_OPEN)) & (FWPM_GENERIC_EXECUTE | FWPM_GENERIC_READ)) != 0)
				is_openforeveryone = TRUE;
		}
		else if (pace->Header.AceType == ACCESS_DENIED_ACE_TYPE)
		{
			if (EqualSid (&pace->SidStart, config.pbuiltin_world_sid) && (pace->Mask == (FWPM_ACTRL_WRITE | DELETE | WRITE_DAC | WRITE_OWNER)))
				is_secured = TRUE;
		}
	}

	if (is_openforeveryone || is_currentuserhaverights || is_secured != is_secure)
	{
		PACL pnewdacl = NULL;
		ULONG count = 0;
		EXPLICIT_ACCESS ea[3];

		RtlSecureZeroMemory (&ea, sizeof (ea));

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

		_r_log (Error, 0, L"SetEntriesInAcl", code, NULL);
	}

	return NULL;
}

VOID _app_setsecurityinfoforengine (HANDLE hengine)
{
	PACL pdacl = NULL;
	PSECURITY_DESCRIPTOR psecurity_descriptor = NULL;
	ULONG code;
	BOOLEAN is_currentuserhaverights = FALSE;
	BOOLEAN is_openforeveryone = FALSE;

	code = FwpmEngineGetSecurityInfo (hengine, DACL_SECURITY_INFORMATION, NULL, NULL, &pdacl, NULL, &psecurity_descriptor);

	if (code != ERROR_SUCCESS)
	{
		_r_log (Error, 0, L"FwpmEngineGetSecurityInfo", code, NULL);
		return;
	}

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
		if (EqualSid (&pace->SidStart, config.pbuiltin_current_sid) && (pace->Mask == (FWPM_GENERIC_ALL | DELETE | WRITE_DAC | WRITE_OWNER)))
			is_currentuserhaverights = TRUE;

		// versions of SW before v3.1.1 added Carte blanche for Everyone
		//
		// src: https://github.com/henrypp/simplewall/blob/v.2.4.6/src/main.cpp#L8820
		// src: https://github.com/henrypp/simplewall/blob/v.3.0.5/src/wfp.cpp#L101
		// src: https://github.com/henrypp/simplewall/blob/v.3.1.1/src/wfp.cpp#L137
		if (EqualSid (&pace->SidStart, config.pbuiltin_world_sid) && ((pace->Mask & ~(FWPM_ACTRL_CLASSIFY | FWPM_ACTRL_OPEN)) & (FWPM_GENERIC_ALL)) != 0)
			is_openforeveryone = TRUE;
	}

	if (is_currentuserhaverights || is_openforeveryone)
	{
		FwpmEngineSetSecurityInfo (hengine, OWNER_SECURITY_INFORMATION, (const SID*)config.pbuiltin_localservice_sid, NULL, NULL, NULL);
		FwpmNetEventsSetSecurityInfo (hengine, OWNER_SECURITY_INFORMATION, (const SID*)config.pbuiltin_localservice_sid, NULL, NULL, NULL);

		PACL pnewdacl = NULL;
		ULONG count = 0;

		EXPLICIT_ACCESS ea[18];
		RtlSecureZeroMemory (&ea, sizeof (ea));

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
			_r_log (Error, 0, L"SetEntriesInAcl", code, NULL);
		}
		else
		{
			code = FwpmEngineSetSecurityInfo (hengine, DACL_SECURITY_INFORMATION, NULL, NULL, pnewdacl, NULL);

			if (code != ERROR_SUCCESS)
				_r_log (Error, 0, L"FwpmEngineSetSecurityInfo", code, NULL);

			code = FwpmNetEventsSetSecurityInfo (hengine, DACL_SECURITY_INFORMATION, NULL, NULL, pnewdacl, NULL);

			if (code != ERROR_SUCCESS)
				_r_log (Error, 0, L"FwpmEngineSetSecurityInfo", code, NULL);

			LocalFree (pnewdacl);
		}
	}

	if (psecurity_descriptor)
		FwpmFreeMemory ((PVOID*)&psecurity_descriptor);
}

VOID _app_setsecurityinfoforprovider (HANDLE hengine, LPCGUID lpguid, BOOLEAN is_secure)
{
	PACL pdacl = NULL;
	PACL pnewdacl = NULL;
	PSECURITY_DESCRIPTOR psecurity_descriptor = NULL;

	ULONG code = FwpmProviderGetSecurityInfoByKey (hengine, lpguid, DACL_SECURITY_INFORMATION, NULL, NULL, &pdacl, NULL, &psecurity_descriptor);

	if (code != ERROR_SUCCESS)
	{
		_r_log (Error, 0, L"FwpmProviderGetSecurityInfoByKey", code, NULL);
		return;
	}

	pnewdacl = _app_createaccesscontrollist (pdacl, is_secure);

	if (pnewdacl)
	{
		FwpmProviderSetSecurityInfoByKey (hengine, lpguid, OWNER_SECURITY_INFORMATION, (const SID*)config.pbuiltin_admins_sid, NULL, NULL, NULL);

		code = FwpmProviderSetSecurityInfoByKey (hengine, lpguid, DACL_SECURITY_INFORMATION, NULL, NULL, pnewdacl, NULL);

		if (code != ERROR_SUCCESS)
			_r_log (Error, 0, L"FwpmProviderSetSecurityInfoByKey", code, L"DACL_SECURITY_INFORMATION");

		LocalFree (pnewdacl);
	}

	if (psecurity_descriptor)
		FwpmFreeMemory ((PVOID*)&psecurity_descriptor);
}

VOID _app_setsecurityinfoforsublayer (HANDLE hengine, LPCGUID lpguid, BOOLEAN is_secure)
{
	PACL pdacl = NULL;
	PACL pnewdacl = NULL;
	PSECURITY_DESCRIPTOR psecurity_descriptor = NULL;

	ULONG code = FwpmSubLayerGetSecurityInfoByKey (hengine, lpguid, DACL_SECURITY_INFORMATION, NULL, NULL, &pdacl, NULL, &psecurity_descriptor);

	if (code != ERROR_SUCCESS)
	{
		_r_log (Error, 0, L"FwpmSubLayerGetSecurityInfoByKey", code, NULL);
		return;
	}

	pnewdacl = _app_createaccesscontrollist (pdacl, is_secure);

	if (pnewdacl)
	{
		FwpmSubLayerSetSecurityInfoByKey (hengine, lpguid, OWNER_SECURITY_INFORMATION, (const SID*)config.pbuiltin_admins_sid, NULL, NULL, NULL);

		code = FwpmSubLayerSetSecurityInfoByKey (hengine, lpguid, DACL_SECURITY_INFORMATION, NULL, NULL, pnewdacl, NULL);

		if (code != ERROR_SUCCESS)
			_r_log (Error, 0, L"FwpmSubLayerSetSecurityInfoByKey", code, NULL);

		LocalFree (pnewdacl);
	}

	if (psecurity_descriptor)
		FwpmFreeMemory ((PVOID*)&psecurity_descriptor);
}

VOID _app_setsecurityinfoforfilter (HANDLE hengine, LPCGUID lpguid, BOOLEAN is_secure, UINT line)
{
	PACL pdacl = NULL;
	PACL pnewdacl = NULL;
	PSECURITY_DESCRIPTOR psecurity_descriptor = NULL;

	ULONG code = FwpmFilterGetSecurityInfoByKey (hengine, lpguid, DACL_SECURITY_INFORMATION, NULL, NULL, &pdacl, NULL, &psecurity_descriptor);

	if (code != ERROR_SUCCESS)
	{
#if !defined(_DEBUG)
		if (code != FWP_E_FILTER_NOT_FOUND)
#endif // !DEBUG
		{
			_r_log_v (Error, 0, L"FwpmFilterSetSecurityInfoByKey", code, L"#%" TEXT (PRIu32), line);
		}

		return;
	}

	pnewdacl = _app_createaccesscontrollist (pdacl, is_secure);

	if (pnewdacl)
	{
		FwpmFilterSetSecurityInfoByKey (hengine, lpguid, OWNER_SECURITY_INFORMATION, (const SID*)config.pbuiltin_admins_sid, NULL, NULL, NULL);

		code = FwpmFilterSetSecurityInfoByKey (hengine, lpguid, DACL_SECURITY_INFORMATION, NULL, NULL, pnewdacl, NULL);

		if (code != ERROR_SUCCESS)
			_r_log_v (Error, 0, L"FwpmFilterSetSecurityInfoByKey", code, L"#%" TEXT (PRIu32), line);

		LocalFree (pnewdacl);
	}

	if (psecurity_descriptor)
		FwpmFreeMemory ((PVOID*)&psecurity_descriptor);
}
