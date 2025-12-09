// simplewall
// Copyright (c) 2020-2024 Henry++

#include "global.h"

_Ret_maybenull_
PSID _app_quyerybuiltinsid (
	_In_ WELL_KNOWN_SID_TYPE sid_type
)
{
	PSID sid;
	ULONG sid_length;

	sid_length = SECURITY_MAX_SID_SIZE;
	sid = _r_mem_allocate (sid_length);

	if (!CreateWellKnownSid (sid_type, NULL, sid, &sid_length))
	{
		_r_log_v (LOG_LEVEL_ERROR, NULL, L"CreateWellKnownSid", NtLastError (), L"%" TEXT (PRIu32), sid_type);

		_r_mem_free (sid);

		return NULL;

	}

	return sid;
}

VOID _app_generate_credentials ()
{
	// For revoke current user (v3.0.5 Beta and lower)
	config.builtin_current_sid = _r_sys_getcurrenttoken ()->token_sid;

	// S-1-5-32-544 (BUILTIN\Administrators)
	config.builtin_admins_sid = _app_quyerybuiltinsid (WinBuiltinAdministratorsSid);

	// S-1-5-32-556 (BUILTIN\Network Configuration Operators)
	config.builtin_netops_sid = _app_quyerybuiltinsid (WinBuiltinNetworkConfigurationOperatorsSid);

	_r_sys_getservicesid (L"mpssvc", &config.service_mpssvc_sid);
	_r_sys_getservicesid (L"NlaSvc", &config.service_nlasvc_sid);
	_r_sys_getservicesid (L"PolicyAgent", &config.service_policyagent_sid);
	_r_sys_getservicesid (L"RpcSs", &config.service_rpcss_sid);
	_r_sys_getservicesid (L"WdiServiceHost", &config.service_wdiservicehost_sid);
}

_Ret_maybenull_
PACL _app_createaccesscontrollist (
	_In_ PACL acl,
	_In_ BOOLEAN is_secure
)
{
	PACCESS_ALLOWED_ACE ace = NULL;
	EXPLICIT_ACCESS ea[3] = {0};
	PACL new_dacl;
	ULONG count = 0;
	BOOLEAN is_currentuserhaverights = FALSE;
	BOOLEAN is_openforeveryone = FALSE;
	BOOLEAN is_secured = FALSE;
	NTSTATUS status;

	for (WORD ace_index = 0; ace_index < acl->AceCount; ace_index++)
	{
		status = RtlGetAce (acl, ace_index, (PVOID_PTR)&ace);

		if (!NT_SUCCESS (status))
			continue;

		if (ace->Header.AceType == ACCESS_ALLOWED_ACE_TYPE)
		{
			// versions of SW before v3.0.5 added Carte blanche for current user
			//
			// src: https://github.com/henrypp/simplewall/blob/v.2.3.12/src/main.cpp#L8273
			// src: https://github.com/henrypp/simplewall/blob/v.2.3.13/src/main.cpp#L8354
			// src: https://github.com/henrypp/simplewall/blob/v.2.4.6/src/main.cpp#L8828
			// src: https://github.com/henrypp/simplewall/blob/v.3.0.5/src/wfp.cpp#L109
			if (RtlEqualSid (&ace->SidStart, config.builtin_current_sid))
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
				if (ace->Mask == (FWPM_ACTRL_WRITE | DELETE | WRITE_DAC | WRITE_OWNER))
					is_secured = TRUE;
			}
		}
	}

	if (is_openforeveryone || is_currentuserhaverights || is_secured != is_secure)
	{
		// revoke current user access rights
		if (is_currentuserhaverights)
			_app_setexplicitaccess (&ea[count++], REVOKE_ACCESS, 0, NO_INHERITANCE, config.builtin_current_sid);

		// revoke everyone access rights
		if (is_openforeveryone)
			_app_setexplicitaccess (&ea[count++], REVOKE_ACCESS, 0, NO_INHERITANCE, &SeEveryoneSid);

		// secure filter from deletion
		_app_setexplicitaccess (
			&ea[count++],
			is_secure ? DENY_ACCESS : GRANT_ACCESS,
			FWPM_ACTRL_WRITE | DELETE | WRITE_DAC | WRITE_OWNER,
			is_secure ? CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE : NO_INHERITANCE,
			&SeEveryoneSid
		);

		status = SetEntriesInAclW (count, ea, acl, &new_dacl);

		if (status != ERROR_SUCCESS)
			_r_log (LOG_LEVEL_ERROR, NULL, L"SetEntriesInAclW", status, NULL);

		return new_dacl;
	}

	return NULL;
}

VOID _app_setexplicitaccess (
	_Out_ PEXPLICIT_ACCESS ea,
	_In_ ACCESS_MODE mode,
	_In_ ULONG rights,
	_In_ ULONG inheritance,
	_In_opt_ PSID sid
)
{
	ea->grfAccessMode = mode;
	ea->grfAccessPermissions = rights;
	ea->grfInheritance = inheritance;

	RtlZeroMemory (&(ea->Trustee), sizeof (ea->Trustee));

	BuildTrusteeWithSidW (&(ea->Trustee), sid);
}

VOID _app_setenginesecurity (
	_In_ HANDLE hengine
)
{
	PSECURITY_DESCRIPTOR security_descriptor;
	PACCESS_ALLOWED_ACE ace = NULL;
	EXPLICIT_ACCESS ea[18] = {0};
	PSID sid_owner;
	PSID sid_group;
	PACL new_dacl;
	PACL dacl;
	PACL sacl;
	ULONG count = 0;
	BOOLEAN is_currentuserhaverights = FALSE;
	BOOLEAN is_openforeveryone = FALSE;
	NTSTATUS status;

	status = FwpmEngineGetSecurityInfo0 (hengine, DACL_SECURITY_INFORMATION, &sid_owner, &sid_group, &dacl, &sacl, &security_descriptor);

	if (status != ERROR_SUCCESS)
	{
		_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmEngineGetSecurityInfo0", status, NULL);

		return;
	}

	if (dacl)
	{
		for (WORD ace_index = 0; ace_index < dacl->AceCount; ace_index++)
		{
			status = RtlGetAce (dacl, ace_index, (PVOID_PTR)&ace);

			if (!NT_SUCCESS (status))
				continue;

			if (ace->Header.AceType != ACCESS_ALLOWED_ACE_TYPE)
				continue;

			// versions of SW before v3.0.5 added Carte blanche for current user
			//
			// src: https://github.com/henrypp/simplewall/blob/v.2.3.12/src/main.cpp#L8273
			// src: https://github.com/henrypp/simplewall/blob/v.2.3.13/src/main.cpp#L8354
			// src: https://github.com/henrypp/simplewall/blob/v.2.4.6/src/main.cpp#L8815
			// src: https://github.com/henrypp/simplewall/blob/v.3.0.5/src/wfp.cpp#L96
			if (RtlEqualSid (&ace->SidStart, config.builtin_current_sid))
			{
				if (ace->Mask == (FWPM_GENERIC_ALL | DELETE | WRITE_DAC | WRITE_OWNER))
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
			FwpmEngineSetSecurityInfo0 (hengine, OWNER_SECURITY_INFORMATION, &SeLocalServiceSid, NULL, NULL, NULL);

			FwpmNetEventsSetSecurityInfo0 (hengine, OWNER_SECURITY_INFORMATION, &SeLocalServiceSid, NULL, NULL, NULL);

			// revoke current user access rights
			if (is_currentuserhaverights)
			{
				if (config.builtin_current_sid)
					_app_setexplicitaccess (&ea[count++], REVOKE_ACCESS, 0, NO_INHERITANCE, config.builtin_current_sid);
			}

			// reset default engine rights
			if (is_openforeveryone)
			{
				if (config.builtin_admins_sid)
				{
					_app_setexplicitaccess (
						&ea[count++],
						GRANT_ACCESS,
						FWPM_GENERIC_ALL | DELETE | WRITE_DAC | WRITE_OWNER, NO_INHERITANCE,
						config.builtin_admins_sid
					);

					_app_setexplicitaccess (
						&ea[count++],
						GRANT_ACCESS,
						0x10000000,
						OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE,
						config.builtin_admins_sid
					);
				}

				if (config.builtin_netops_sid)
				{
					_app_setexplicitaccess (
						&ea[count++],
						GRANT_ACCESS,
						FWPM_GENERIC_ALL | DELETE,
						NO_INHERITANCE,
						config.builtin_netops_sid
					);

					_app_setexplicitaccess (
						&ea[count++],
						GRANT_ACCESS,
						0xE0000000,
						OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE,
						config.builtin_netops_sid
					);
				}

				if (config.service_mpssvc_sid)
				{
					_app_setexplicitaccess (
						&ea[count++],
						GRANT_ACCESS,
						FWPM_GENERIC_ALL | DELETE,
						NO_INHERITANCE,
						config.service_mpssvc_sid->buffer
					);

					_app_setexplicitaccess (
						&ea[count++],
						GRANT_ACCESS,
						0xE0000000,
						OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE,
						config.service_mpssvc_sid->buffer
					);
				}

				if (config.service_nlasvc_sid)
				{
					_app_setexplicitaccess (
						&ea[count++],
						GRANT_ACCESS,
						FWPM_GENERIC_READ | FWPM_GENERIC_EXECUTE,
						NO_INHERITANCE,
						config.service_nlasvc_sid->buffer
					);

					_app_setexplicitaccess (
						&ea[count++],
						GRANT_ACCESS,
						0xA0000000,
						OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE,
						config.service_nlasvc_sid->buffer
					);
				}

				if (config.service_policyagent_sid)
				{
					_app_setexplicitaccess (
						&ea[count++],
						GRANT_ACCESS,
						FWPM_GENERIC_ALL | DELETE,
						NO_INHERITANCE,
						config.service_policyagent_sid->buffer
					);

					_app_setexplicitaccess (
						&ea[count++],
						GRANT_ACCESS,
						0xE0000000,
						OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE,
						config.service_policyagent_sid->buffer
					);
				}

				if (config.service_rpcss_sid)
				{
					_app_setexplicitaccess (
						&ea[count++],
						GRANT_ACCESS,
						FWPM_GENERIC_ALL | DELETE,
						NO_INHERITANCE,
						config.service_rpcss_sid->buffer
					);

					_app_setexplicitaccess (
						&ea[count++],
						GRANT_ACCESS,
						0xE0000000,
						OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE,
						config.service_rpcss_sid->buffer
					);
				}

				if (config.service_wdiservicehost_sid)
				{
					_app_setexplicitaccess (
						&ea[count++],
						GRANT_ACCESS,
						FWPM_GENERIC_READ | FWPM_GENERIC_EXECUTE,
						NO_INHERITANCE,
						config.service_wdiservicehost_sid->buffer
					);

					_app_setexplicitaccess (
						&ea[count++],
						GRANT_ACCESS,
						0xA0000000,
						OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE,
						config.service_wdiservicehost_sid->buffer
					);
				}

				_app_setexplicitaccess (
					&ea[count++],
					SET_ACCESS,
					FWPM_ACTRL_CLASSIFY | FWPM_ACTRL_OPEN,
					OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE,
					&SeEveryoneSid
				);
			}

			status = SetEntriesInAclW (count, ea, dacl, &new_dacl);

			if (status != ERROR_SUCCESS)
			{
				_r_log (LOG_LEVEL_ERROR, NULL, L"SetEntriesInAclW", status, NULL);
			}
			else
			{
				status = FwpmEngineSetSecurityInfo0 (hengine, DACL_SECURITY_INFORMATION, NULL, NULL, new_dacl, NULL);

				if (status != ERROR_SUCCESS)
					_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmEngineSetSecurityInfo0", status, NULL);

				status = FwpmNetEventsSetSecurityInfo0 (hengine, DACL_SECURITY_INFORMATION, NULL, NULL, new_dacl, NULL);

				if (status != ERROR_SUCCESS)
					_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmNetEventsSetSecurityInfo0", status, NULL);

				LocalFree (new_dacl);
			}
		}
	}

	if (security_descriptor)
		FwpmFreeMemory0 ((PVOID_PTR)&security_descriptor);
}

VOID _app_setprovidersecurity (
	_In_ HANDLE hengine,
	_In_ LPCGUID provider_guid,
	_In_ BOOLEAN is_secure
)
{
	PSECURITY_DESCRIPTOR security_descriptor;
	PSID sid_owner;
	PSID sid_group;
	PACL dacl;
	PACL sacl;
	PACL new_dacl;
	ULONG status;

	status = FwpmProviderGetSecurityInfoByKey0 (
		hengine,
		provider_guid,
		DACL_SECURITY_INFORMATION,
		&sid_owner,
		&sid_group,
		&dacl,
		&sacl,
		&security_descriptor
	);

	if (status != ERROR_SUCCESS)
	{
		_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmProviderGetSecurityInfoByKey0", status, NULL);

		return;
	}

	if (dacl)
	{
		new_dacl = _app_createaccesscontrollist (dacl, is_secure);

		if (new_dacl)
		{
			status = FwpmProviderSetSecurityInfoByKey0 (
				hengine,
				provider_guid,
				OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
				(PCSID)config.builtin_admins_sid,
				NULL,
				new_dacl,
				NULL
			);

			if (status != ERROR_SUCCESS)
				_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmProviderSetSecurityInfoByKey0", status, NULL);

			LocalFree (new_dacl);
		}
	}

	if (security_descriptor)
		FwpmFreeMemory0 ((PVOID_PTR)&security_descriptor);
}

VOID _app_setsublayersecurity (
	_In_ HANDLE hengine,
	_In_ LPCGUID sublayer_guid,
	_In_ BOOLEAN is_secure
)
{
	PSECURITY_DESCRIPTOR security_descriptor;
	PSID sid_owner;
	PSID sid_group;
	PACL dacl;
	PACL sacl;
	PACL new_dacl;
	ULONG status;

	status = FwpmSubLayerGetSecurityInfoByKey0 (
		hengine,
		sublayer_guid,
		DACL_SECURITY_INFORMATION,
		&sid_owner,
		&sid_group,
		&dacl,
		&sacl,
		&security_descriptor
	);

	if (status != ERROR_SUCCESS)
	{
		_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmSubLayerGetSecurityInfoByKey0", status, NULL);

		return;
	}

	if (dacl)
	{
		new_dacl = _app_createaccesscontrollist (dacl, is_secure);

		if (new_dacl)
		{
			status = FwpmSubLayerSetSecurityInfoByKey0 (
				hengine,
				sublayer_guid,
				OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
				(PCSID)config.builtin_admins_sid,
				NULL,
				new_dacl,
				NULL
			);

			if (status != ERROR_SUCCESS)
				_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmSubLayerSetSecurityInfoByKey0", status, NULL);

			LocalFree (new_dacl);
		}
	}

	if (security_descriptor)
		FwpmFreeMemory0 ((PVOID_PTR)&security_descriptor);
}

VOID _app_setcalloutsecurity (
	_In_ HANDLE hengine,
	_In_ LPCGUID callout_guid,
	_In_ BOOLEAN is_secure
)
{
	PSECURITY_DESCRIPTOR security_descriptor;
	PSID sid_owner;
	PSID sid_group;
	PACL dacl;
	PACL sacl;
	PACL new_dacl;
	ULONG status;

	status = FwpmCalloutGetSecurityInfoByKey0 (
		hengine,
		callout_guid,
		DACL_SECURITY_INFORMATION,
		&sid_owner,
		&sid_group,
		&dacl,
		&sacl,
		&security_descriptor
	);

	if (status != ERROR_SUCCESS)
	{
		if (status != FWP_E_CALLOUT_NOT_FOUND)
			_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmCalloutGetSecurityInfoByKey0", status, NULL);

		return;
	}

	if (dacl)
	{
		new_dacl = _app_createaccesscontrollist (dacl, is_secure);

		if (new_dacl)
		{
			status = FwpmCalloutSetSecurityInfoByKey0 (
				hengine,
				callout_guid,
				OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
				(PCSID)config.builtin_admins_sid,
				NULL,
				new_dacl,
				NULL
			);

			if (status != ERROR_SUCCESS)
				_r_log (LOG_LEVEL_ERROR, NULL, L"FwpmCalloutSetSecurityInfoByKey0", status, NULL);

			LocalFree (new_dacl);
		}
	}

	if (security_descriptor)
		FwpmFreeMemory0 ((PVOID_PTR)&security_descriptor);
}

VOID _app_setfiltersecurity (
	_In_ HANDLE hengine,
	_In_ LPCGUID filter_guid,
	_In_ BOOLEAN is_secure,
	_In_ LPCWSTR file_name,
	_In_ ULONG line
)
{
	PSECURITY_DESCRIPTOR security_descriptor;
	PSID sid_owner;
	PSID sid_group;
	PACL dacl;
	PACL sacl;
	PACL new_dacl;
	ULONG status;

	status = FwpmFilterGetSecurityInfoByKey0 (hengine, filter_guid, DACL_SECURITY_INFORMATION, &sid_owner, &sid_group, &dacl, &sacl, &security_descriptor);

	if (status == ERROR_SUCCESS)
	{
		if (dacl)
		{
			new_dacl = _app_createaccesscontrollist (dacl, is_secure);

			if (new_dacl)
			{
				status = FwpmFilterSetSecurityInfoByKey0 (hengine, filter_guid, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, (PCSID)config.builtin_admins_sid, NULL, new_dacl, NULL);

				if (status != ERROR_SUCCESS)
					_r_log_v (LOG_LEVEL_ERROR, NULL, L"FwpmFilterSetSecurityInfoByKey0", status, L"#%d", line);

				LocalFree (new_dacl);
			}
		}
	}
	else
	{
		//if (status != FWP_E_FILTER_NOT_FOUND)
		_r_log_v (LOG_LEVEL_ERROR, NULL, L"FwpmFilterGetSecurityInfoByKey0", status, L"%s:%d", DBG_ARG_VAR);
	}

	if (security_descriptor)
		FwpmFreeMemory0 ((PVOID_PTR)&security_descriptor);
}
