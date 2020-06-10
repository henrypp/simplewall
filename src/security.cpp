// simplewall
// Copyright (c) 2020 Henry++

#include "global.hpp"

PSID _app_quyerybuiltinsid (WELL_KNOWN_SID_TYPE sid_type)
{
	DWORD sidLength = SECURITY_MAX_SID_SIZE;
	PSID pSid = _r_mem_allocatezero (sidLength);

	if (CreateWellKnownSid (sid_type, NULL, pSid, &sidLength))
		return pSid;

	_r_mem_free (pSid);

	return NULL;
}

PSID _app_queryservicesid (LPCWSTR name)
{
	UNICODE_STRING serviceName;
	RtlInitUnicodeString (&serviceName, (LPWSTR)name);

	ULONG sidLength = 0;
	PSID pSid = NULL;

	if (RtlCreateServiceSid (&serviceName, pSid, &sidLength) == STATUS_BUFFER_TOO_SMALL)
	{
		pSid = _r_mem_allocatezero (sidLength);

		if (NT_SUCCESS (RtlCreateServiceSid (&serviceName, pSid, &sidLength)))
			return pSid;

		_r_mem_free (pSid);
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
			DWORD token_length = 0;
			GetTokenInformation (htoken, TokenUser, NULL, 0, &token_length);

			if (GetLastError () == ERROR_INSUFFICIENT_BUFFER)
			{
				PTOKEN_USER ptoken_user = (PTOKEN_USER)_r_mem_allocatezero (token_length);

				if (GetTokenInformation (htoken, TokenUser, ptoken_user, token_length, &token_length))
				{
					if (RtlValidSid (ptoken_user->User.Sid))
					{
						DWORD sidLength = RtlLengthSid (ptoken_user->User.Sid);
						config.pbuiltin_current_sid = _r_mem_allocatezero (sidLength);

						RtlCopyMemory (config.pbuiltin_current_sid, ptoken_user->User.Sid, sidLength);
					}
				}

				_r_mem_free (ptoken_user);
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

VOID _app_setexplicitaccess (PEXPLICIT_ACCESS pExplicitAccess, ACCESS_MODE AccessMode, DWORD dwInheritance, DWORD dwAccessPermissionss, PSID pusersid)
{
	pExplicitAccess->grfAccessPermissions = dwAccessPermissionss;
	pExplicitAccess->grfAccessMode = AccessMode;
	pExplicitAccess->grfInheritance = dwInheritance;

	BuildTrusteeWithSid (&(pExplicitAccess->Trustee), pusersid);
}

PACL _app_createaccesscontrollist (PACL pAcl, BOOLEAN is_secure)
{
	BOOLEAN is_secured = FALSE;

	BOOLEAN is_currentuserhaverights = FALSE;
	BOOLEAN is_openforeveryone = FALSE;

	for (WORD cAce = 0; cAce < pAcl->AceCount; cAce++)
	{
		PACCESS_ALLOWED_ACE pAce = NULL;

		if (!GetAce (pAcl, cAce, (PVOID*)&pAce))
			continue;

		if (pAce->Header.AceType == ACCESS_ALLOWED_ACE_TYPE)
		{
			// versions of SW before v3.0.5 added Carte blanche for current user
			//
			// src: https://github.com/henrypp/simplewall/blob/v.2.3.12/src/main.cpp#L8273
			// src: https://github.com/henrypp/simplewall/blob/v.2.3.13/src/main.cpp#L8354
			// src: https://github.com/henrypp/simplewall/blob/v.2.4.6/src/main.cpp#L8828
			// src: https://github.com/henrypp/simplewall/blob/v.3.0.5/src/wfp.cpp#L109
			if (EqualSid (&pAce->SidStart, config.pbuiltin_current_sid) && (pAce->Mask & (FWPM_GENERIC_EXECUTE | FWPM_GENERIC_WRITE | DELETE | WRITE_DAC | WRITE_OWNER)) != 0)
				is_currentuserhaverights = TRUE;

			// versions of SW before v3.1.1 added Carte blanche for Everyone
			//
			// src: https://github.com/henrypp/simplewall/blob/v.2.4.6/src/main.cpp#L8833
			// src: https://github.com/henrypp/simplewall/blob/v.3.0.5/src/wfp.cpp#L114
			// src: https://github.com/henrypp/simplewall/blob/v.3.1.1/src/wfp.cpp#L150
			else if (EqualSid (&pAce->SidStart, config.pbuiltin_world_sid) && ((pAce->Mask & ~(FWPM_ACTRL_CLASSIFY | FWPM_ACTRL_OPEN)) & (FWPM_GENERIC_EXECUTE | FWPM_GENERIC_READ)) != 0)
				is_openforeveryone = TRUE;
		}
		else if (pAce->Header.AceType == ACCESS_DENIED_ACE_TYPE)
		{
			if (EqualSid (&pAce->SidStart, config.pbuiltin_world_sid) && (pAce->Mask == (FWPM_ACTRL_WRITE | DELETE | WRITE_DAC | WRITE_OWNER)))
				is_secured = TRUE;
		}
	}

	if (is_openforeveryone || is_currentuserhaverights || is_secured != is_secure)
	{
		PACL pNewDacl = NULL;

		DWORD count = 0;

		EXPLICIT_ACCESS ea[3];
		RtlSecureZeroMemory (&ea, sizeof (ea));

		// revoke current user access rights
		if (is_currentuserhaverights)
		{
			_app_setexplicitaccess (&ea[count++], REVOKE_ACCESS, NO_INHERITANCE, 0, config.pbuiltin_current_sid);
		}

		// revoke everyone access rights
		if (is_openforeveryone)
		{
			_app_setexplicitaccess (&ea[count++], REVOKE_ACCESS, NO_INHERITANCE, 0, config.pbuiltin_world_sid);
		}

		// secure filter from deletion
		_app_setexplicitaccess (&ea[count++],
								is_secure ? DENY_ACCESS : GRANT_ACCESS,
								is_secure ? CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE : NO_INHERITANCE,
								FWPM_ACTRL_WRITE | DELETE | WRITE_DAC | WRITE_OWNER,
								config.pbuiltin_world_sid
		);

		DWORD code = SetEntriesInAcl (count, ea, pAcl, &pNewDacl);

		if (code == ERROR_SUCCESS)
			return pNewDacl;

		app.LogError (L"SetEntriesInAcl", code, NULL, 0);
	}

	return NULL;
}

VOID _app_setsecurityinfoforengine (HANDLE hengine)
{
	PACL pDacl = NULL;
	PSECURITY_DESCRIPTOR psecurityDescriptor = NULL;

	DWORD code = FwpmEngineGetSecurityInfo (hengine, DACL_SECURITY_INFORMATION, NULL, NULL, &pDacl, NULL, &psecurityDescriptor);

	if (code != ERROR_SUCCESS)
	{
		app.LogError (L"FwpmEngineGetSecurityInfo", code, NULL, 0);
		return;
	}

	BOOLEAN is_currentuserhaverights = FALSE;
	BOOLEAN is_openforeveryone = FALSE;

	for (WORD cAce = 0; cAce < pDacl->AceCount; cAce++)
	{
		PACCESS_ALLOWED_ACE pAce = NULL;

		if (!GetAce (pDacl, cAce, (PVOID*)&pAce))
			continue;

		if (pAce->Header.AceType != ACCESS_ALLOWED_ACE_TYPE)
			continue;

		// versions of SW before v3.0.5 added Carte blanche for current user
		//
		// src: https://github.com/henrypp/simplewall/blob/v.2.3.12/src/main.cpp#L8273
		// src: https://github.com/henrypp/simplewall/blob/v.2.3.13/src/main.cpp#L8354
		// src: https://github.com/henrypp/simplewall/blob/v.2.4.6/src/main.cpp#L8815
		// src: https://github.com/henrypp/simplewall/blob/v.3.0.5/src/wfp.cpp#L96
		if (EqualSid (&pAce->SidStart, config.pbuiltin_current_sid) && (pAce->Mask == (FWPM_GENERIC_ALL | DELETE | WRITE_DAC | WRITE_OWNER)))
			is_currentuserhaverights = TRUE;

		// versions of SW before v3.1.1 added Carte blanche for Everyone
		//
		// src: https://github.com/henrypp/simplewall/blob/v.2.4.6/src/main.cpp#L8820
		// src: https://github.com/henrypp/simplewall/blob/v.3.0.5/src/wfp.cpp#L101
		// src: https://github.com/henrypp/simplewall/blob/v.3.1.1/src/wfp.cpp#L137
		if (EqualSid (&pAce->SidStart, config.pbuiltin_world_sid) && ((pAce->Mask & ~(FWPM_ACTRL_CLASSIFY | FWPM_ACTRL_OPEN)) & (FWPM_GENERIC_ALL)) != 0)
			is_openforeveryone = TRUE;
	}

	if (is_currentuserhaverights || is_openforeveryone)
	{
		FwpmEngineSetSecurityInfo (hengine, OWNER_SECURITY_INFORMATION, (const SID*)config.pbuiltin_localservice_sid, NULL, NULL, NULL);
		FwpmNetEventsSetSecurityInfo (hengine, OWNER_SECURITY_INFORMATION, (const SID*)config.pbuiltin_localservice_sid, NULL, NULL, NULL);

		PACL pNewDacl = NULL;
		DWORD count = 0;

		EXPLICIT_ACCESS ea[18];
		RtlSecureZeroMemory (&ea, sizeof (ea));

		// revoke current user access rights
		if (is_currentuserhaverights)
		{
			if (config.pbuiltin_current_sid)
				_app_setexplicitaccess (&ea[count++], REVOKE_ACCESS, NO_INHERITANCE, 0, config.pbuiltin_current_sid);
		}

		// reset default engine rights
		if (is_openforeveryone)
		{
			if (config.pbuiltin_admins_sid)
			{
				_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, NO_INHERITANCE, FWPM_GENERIC_ALL | DELETE | WRITE_DAC | WRITE_OWNER, config.pbuiltin_admins_sid);
				_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE, 0x10000000, config.pbuiltin_admins_sid);
			}

			if (config.pbuiltin_netops_sid)
			{
				_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, NO_INHERITANCE, FWPM_GENERIC_ALL | DELETE, config.pbuiltin_netops_sid);
				_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE, 0xE0000000, config.pbuiltin_netops_sid);
			}

			if (config.pservice_mpssvc_sid)
			{
				_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, NO_INHERITANCE, FWPM_GENERIC_ALL | DELETE, config.pservice_mpssvc_sid);
				_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE, 0xE0000000, config.pservice_mpssvc_sid);
			}

			if (config.pservice_nlasvc_sid)
			{
				_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, NO_INHERITANCE, FWPM_GENERIC_READ | FWPM_GENERIC_EXECUTE, config.pservice_nlasvc_sid);
				_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE, 0xA0000000, config.pservice_nlasvc_sid);
			}

			if (config.pservice_policyagent_sid)
			{
				_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, NO_INHERITANCE, FWPM_GENERIC_ALL | DELETE, config.pservice_policyagent_sid);
				_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE, 0xE0000000, config.pservice_policyagent_sid);
			}

			if (config.pservice_rpcss_sid)
			{
				_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, NO_INHERITANCE, FWPM_GENERIC_ALL | DELETE, config.pservice_rpcss_sid);
				_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE, 0xE0000000, config.pservice_rpcss_sid);
			}

			if (config.pservice_wdiservicehost_sid)
			{
				_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, NO_INHERITANCE, FWPM_GENERIC_READ | FWPM_GENERIC_EXECUTE, config.pservice_wdiservicehost_sid);
				_app_setexplicitaccess (&ea[count++], GRANT_ACCESS, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE, 0xA0000000, config.pservice_wdiservicehost_sid);
			}

			if (config.pbuiltin_world_sid)
				_app_setexplicitaccess (&ea[count++], SET_ACCESS, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE, FWPM_ACTRL_CLASSIFY | FWPM_ACTRL_OPEN, config.pbuiltin_world_sid);
		}

		code = SetEntriesInAcl (count, ea, pDacl, &pNewDacl);

		if (code != ERROR_SUCCESS)
		{
			app.LogError (L"SetEntriesInAcl", code, NULL, 0);
		}
		else
		{
			code = FwpmEngineSetSecurityInfo (hengine, DACL_SECURITY_INFORMATION, NULL, NULL, pNewDacl, NULL);

			if (code != ERROR_SUCCESS)
				app.LogError (L"FwpmEngineSetSecurityInfo", code, NULL, 0);

			code = FwpmNetEventsSetSecurityInfo (hengine, DACL_SECURITY_INFORMATION, NULL, NULL, pNewDacl, NULL);

			if (code != ERROR_SUCCESS)
				app.LogError (L"FwpmEngineSetSecurityInfo", code, NULL, 0);

			LocalFree (pNewDacl);
		}
	}

	if (psecurityDescriptor)
		FwpmFreeMemory ((PVOID*)&psecurityDescriptor);
}

VOID _app_setsecurityinfoforprovider (HANDLE hengine, const GUID* lpguid, BOOLEAN is_secure)
{
	PACL pDacl = NULL;
	PSECURITY_DESCRIPTOR psecurityDescriptor = NULL;

	DWORD code = FwpmProviderGetSecurityInfoByKey (hengine, lpguid, DACL_SECURITY_INFORMATION, NULL, NULL, &pDacl, NULL, &psecurityDescriptor);

	if (code != ERROR_SUCCESS)
	{
		app.LogError (L"FwpmProviderGetSecurityInfoByKey", code, NULL, 0);
		return;
	}

	PACL pNewDacl = _app_createaccesscontrollist (pDacl, is_secure);

	if (pNewDacl)
	{
		FwpmProviderSetSecurityInfoByKey (hengine, lpguid, OWNER_SECURITY_INFORMATION, (const SID*)config.pbuiltin_admins_sid, NULL, NULL, NULL);

		code = FwpmProviderSetSecurityInfoByKey (hengine, lpguid, DACL_SECURITY_INFORMATION, NULL, NULL, pNewDacl, NULL);

		if (code != ERROR_SUCCESS)
			app.LogError (L"FwpmProviderSetSecurityInfoByKey", code, L"DACL_SECURITY_INFORMATION", 0);

		LocalFree (pNewDacl);
	}

	if (psecurityDescriptor)
		FwpmFreeMemory ((PVOID*)&psecurityDescriptor);
}

VOID _app_setsecurityinfoforsublayer (HANDLE hengine, const GUID* lpguid, BOOLEAN is_secure)
{
	PACL pDacl = NULL;
	PSECURITY_DESCRIPTOR psecurityDescriptor = NULL;

	DWORD code = FwpmSubLayerGetSecurityInfoByKey (hengine, lpguid, DACL_SECURITY_INFORMATION, NULL, NULL, &pDacl, NULL, &psecurityDescriptor);

	if (code != ERROR_SUCCESS)
	{
		app.LogError (L"FwpmSubLayerGetSecurityInfoByKey", code, NULL, 0);
		return;
	}

	PACL pNewDacl = _app_createaccesscontrollist (pDacl, is_secure);

	if (pNewDacl)
	{
		FwpmSubLayerSetSecurityInfoByKey (hengine, lpguid, OWNER_SECURITY_INFORMATION, (const SID*)config.pbuiltin_admins_sid, NULL, NULL, NULL);

		code = FwpmSubLayerSetSecurityInfoByKey (hengine, lpguid, DACL_SECURITY_INFORMATION, NULL, NULL, pNewDacl, NULL);

		if (code != ERROR_SUCCESS)
			app.LogError (L"FwpmSubLayerSetSecurityInfoByKey", code, NULL, 0);

		LocalFree (pNewDacl);
	}

	if (psecurityDescriptor)
		FwpmFreeMemory ((PVOID*)&psecurityDescriptor);
}

VOID _app_setsecurityinfoforfilter (HANDLE hengine, const GUID* lpguid, BOOLEAN is_secure, UINT line)
{
	PACL pDacl = NULL;
	PSECURITY_DESCRIPTOR psecurityDescriptor = NULL;

	DWORD code = FwpmFilterGetSecurityInfoByKey (hengine, lpguid, DACL_SECURITY_INFORMATION, NULL, NULL, &pDacl, NULL, &psecurityDescriptor);

	if (code != ERROR_SUCCESS)
	{
		app.LogError (L"FwpmFilterSetSecurityInfoByKey", code, _r_fmt (L"#%d", line).GetString (), 0);
		return;
	}

	PACL pNewDacl = _app_createaccesscontrollist (pDacl, is_secure);

	if (pNewDacl)
	{
		FwpmFilterSetSecurityInfoByKey (hengine, lpguid, OWNER_SECURITY_INFORMATION, (const SID*)config.pbuiltin_admins_sid, NULL, NULL, NULL);

		code = FwpmFilterSetSecurityInfoByKey (hengine, lpguid, DACL_SECURITY_INFORMATION, NULL, NULL, pNewDacl, NULL);

		if (code != ERROR_SUCCESS)
			app.LogError (L"FwpmFilterSetSecurityInfoByKey", code, _r_fmt (L"#%d", line).GetString (), 0);

		LocalFree (pNewDacl);
	}

	if (psecurityDescriptor)
		FwpmFreeMemory ((PVOID*)&psecurityDescriptor);
}
