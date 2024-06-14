// simplewall
// Copyright (c) 2019-2024 Henry++

#include "global.h"

_Ret_maybenull_
PITEM_NETWORK_CONTEXT _app_network_getcontext ()
{
	static R_INITONCE init_once = PR_INITONCE_INIT;
	static PITEM_NETWORK_CONTEXT network_context = NULL;

	if (_r_initonce_begin (&init_once))
	{
		network_context = _r_mem_allocate (sizeof (ITEM_NETWORK_CONTEXT));

		network_context->network_ptr = _r_obj_createhashtablepointer (256);
		network_context->checker_ptr = _r_obj_createhashtablepointer (256);

		_r_initonce_end (&init_once);
	}

	return network_context;
}

VOID _app_network_initialize (
	_In_ HWND hwnd
)
{
	PITEM_NETWORK_CONTEXT network_context;
	R_ENVIRONMENT environment;
	BOOLEAN is_enabled;

	is_enabled = _r_config_getboolean (L"IsNetworkMonitorEnabled", TRUE);

	if (!is_enabled)
		return;

	network_context = _app_network_getcontext ();

	if (!network_context)
		return;

	network_context->hwnd = hwnd;

	_r_queuedlock_acquireexclusive (&network_context->lock_network);
	_r_obj_clearhashtable (network_context->network_ptr);
	_r_queuedlock_releaseexclusive (&network_context->lock_network);

	_r_queuedlock_acquireexclusive (&network_context->lock_checker);
	_r_obj_clearhashtable (network_context->checker_ptr);
	_r_queuedlock_releaseexclusive (&network_context->lock_checker);

	// create network monitor thread
	_r_sys_setenvironment (&environment, THREAD_PRIORITY_ABOVE_NORMAL, IoPriorityNormal, MEMORY_PRIORITY_NORMAL);

	_r_sys_createthread (NULL, NtCurrentProcess (), &_app_network_threadproc, network_context, &environment, L"NetMonitor");
}

VOID _app_network_uninitialize (
	_In_ PITEM_NETWORK_CONTEXT context
)
{
	_r_queuedlock_acquireexclusive (&context->lock_network);
	_r_obj_clearhashtable (context->network_ptr);
	_r_queuedlock_releaseexclusive (&context->lock_network);

	_r_queuedlock_acquireexclusive (&context->lock_checker);
	_r_obj_clearhashtable (context->checker_ptr);
	_r_queuedlock_releaseexclusive (&context->lock_checker);
}

VOID _app_network_generatetable (
	_Inout_ PITEM_NETWORK_CONTEXT network_context
)
{
	PITEM_NETWORK ptr_network;
	IN_ADDR remote_addr;
	IN_ADDR local_addr;
	PMIB_TCPTABLE_OWNER_MODULE tcp4_table;
	PMIB_TCP6TABLE_OWNER_MODULE tcp6_table;
	PMIB_UDPTABLE_OWNER_MODULE udp4_table;
	PMIB_UDP6TABLE_OWNER_MODULE udp6_table;
	ULONG_PTR network_hash;
	PVOID buffer;
	ULONG allocated_size;
	ULONG required_size;
	ULONG status;

	_r_queuedlock_acquireexclusive (&network_context->lock_checker);
	_r_obj_clearhashtable (network_context->checker_ptr);
	_r_queuedlock_releaseexclusive (&network_context->lock_checker);

	required_size = 0;
	GetExtendedTcpTable (NULL, &required_size, FALSE, AF_INET, TCP_TABLE_OWNER_MODULE_ALL, 0);

	allocated_size = required_size;
	buffer = _r_mem_allocate (allocated_size);

	if (required_size)
	{
		tcp4_table = buffer;
		status = GetExtendedTcpTable (tcp4_table, &required_size, FALSE, AF_INET, TCP_TABLE_OWNER_MODULE_ALL, 0);

		if (status == NO_ERROR)
		{
			for (ULONG i = 0; i < tcp4_table->dwNumEntries; i++)
			{
				RtlZeroMemory (&remote_addr, sizeof (remote_addr));
				RtlZeroMemory (&local_addr, sizeof (local_addr));

				remote_addr.S_un.S_addr = tcp4_table->table[i].dwRemoteAddr;
				local_addr.S_un.S_addr = tcp4_table->table[i].dwLocalAddr;

				network_hash = _app_network_gethash (
					AF_INET,
					tcp4_table->table[i].dwOwningPid,
					&remote_addr,
					tcp4_table->table[i].dwRemotePort,
					&local_addr,
					tcp4_table->table[i].dwLocalPort,
					IPPROTO_TCP,
					tcp4_table->table[i].dwState
				);

				if (_app_network_isitemfound (network_hash))
				{
					_r_queuedlock_acquireexclusive (&network_context->lock_checker);
					_r_obj_addhashtablepointer (network_context->checker_ptr, network_hash, NULL);
					_r_queuedlock_releaseexclusive (&network_context->lock_checker);

					continue;
				}

				ptr_network = _r_obj_allocate (sizeof (ITEM_NETWORK), &_app_dereferencenetwork);

				if (!_app_network_getpath (ptr_network, tcp4_table->table[i].dwOwningPid, tcp4_table->table[i].OwningModuleInfo))
				{
					_r_obj_dereference (ptr_network);

					continue;
				}

				ptr_network->af = AF_INET;
				ptr_network->protocol = IPPROTO_TCP;
				ptr_network->protocol_str = _app_db_getprotoname (ptr_network->protocol, ptr_network->af, FALSE);

				ptr_network->remote_addr.S_un.S_addr = tcp4_table->table[i].dwRemoteAddr;
				ptr_network->remote_port = _r_byteswap_ushort ((USHORT)tcp4_table->table[i].dwRemotePort);

				ptr_network->local_addr.S_un.S_addr = tcp4_table->table[i].dwLocalAddr;
				ptr_network->local_port = _r_byteswap_ushort ((USHORT)tcp4_table->table[i].dwLocalPort);

				ptr_network->state = tcp4_table->table[i].dwState;

				if (tcp4_table->table[i].dwState == MIB_TCP_STATE_ESTAB)
				{
					if (_app_network_isvalidconnection (ptr_network->af, &ptr_network->remote_addr) || _app_network_isvalidconnection (ptr_network->af, &ptr_network->local_addr))
						ptr_network->is_connection = TRUE;
				}

				_r_queuedlock_acquireexclusive (&network_context->lock_network);
				_r_obj_addhashtablepointer (network_context->network_ptr, network_hash, ptr_network);
				_r_queuedlock_releaseexclusive (&network_context->lock_network);

				_r_queuedlock_acquireexclusive (&network_context->lock_checker);
				_r_obj_addhashtablepointer (network_context->checker_ptr, network_hash, _r_obj_reference (ptr_network->path));
				_r_queuedlock_releaseexclusive (&network_context->lock_checker);
			}
		}
	}

	required_size = 0;
	GetExtendedTcpTable (NULL, &required_size, FALSE, AF_INET6, TCP_TABLE_OWNER_MODULE_ALL, 0);

	if (required_size)
	{
		if (allocated_size < required_size)
		{
			buffer = _r_mem_reallocate (buffer, required_size);

			allocated_size = required_size;
		}

		tcp6_table = buffer;
		status = GetExtendedTcpTable (tcp6_table, &required_size, FALSE, AF_INET6, TCP_TABLE_OWNER_MODULE_ALL, 0);

		if (status == NO_ERROR)
		{
			for (ULONG i = 0; i < tcp6_table->dwNumEntries; i++)
			{
				network_hash = _app_network_gethash (
					AF_INET6,
					tcp6_table->table[i].dwOwningPid,
					tcp6_table->table[i].ucRemoteAddr,
					tcp6_table->table[i].dwRemotePort,
					tcp6_table->table[i].ucLocalAddr,
					tcp6_table->table[i].dwLocalPort,
					IPPROTO_TCP,
					tcp6_table->table[i].dwState
				);

				if (_app_network_isitemfound (network_hash))
				{
					_r_queuedlock_acquireexclusive (&network_context->lock_checker);
					_r_obj_addhashtablepointer (network_context->checker_ptr, network_hash, NULL);
					_r_queuedlock_releaseexclusive (&network_context->lock_checker);

					continue;
				}

				ptr_network = _r_obj_allocate (sizeof (ITEM_NETWORK), &_app_dereferencenetwork);

				if (!_app_network_getpath (ptr_network, tcp6_table->table[i].dwOwningPid, tcp6_table->table[i].OwningModuleInfo))
				{
					_r_obj_dereference (ptr_network);

					continue;
				}

				ptr_network->af = AF_INET6;
				ptr_network->protocol = IPPROTO_TCP;
				ptr_network->protocol_str = _app_db_getprotoname (ptr_network->protocol, ptr_network->af, FALSE);

				RtlCopyMemory (ptr_network->remote_addr6.u.Byte, tcp6_table->table[i].ucRemoteAddr, FWP_V6_ADDR_SIZE);
				ptr_network->remote_port = _r_byteswap_ushort ((USHORT)tcp6_table->table[i].dwRemotePort);

				RtlCopyMemory (ptr_network->local_addr6.u.Byte, tcp6_table->table[i].ucLocalAddr, FWP_V6_ADDR_SIZE);
				ptr_network->local_port = _r_byteswap_ushort ((USHORT)tcp6_table->table[i].dwLocalPort);

				ptr_network->state = tcp6_table->table[i].dwState;

				if (tcp6_table->table[i].dwState == MIB_TCP_STATE_ESTAB)
				{
					if (_app_network_isvalidconnection (ptr_network->af, &ptr_network->remote_addr6) || _app_network_isvalidconnection (ptr_network->af, &ptr_network->local_addr6))
						ptr_network->is_connection = TRUE;
				}

				_r_queuedlock_acquireexclusive (&network_context->lock_network);
				_r_obj_addhashtablepointer (network_context->network_ptr, network_hash, ptr_network);
				_r_queuedlock_releaseexclusive (&network_context->lock_network);

				_r_queuedlock_acquireexclusive (&network_context->lock_checker);
				_r_obj_addhashtablepointer (network_context->checker_ptr, network_hash, _r_obj_reference (ptr_network->path));
				_r_queuedlock_releaseexclusive (&network_context->lock_checker);
			}
		}
	}

	required_size = 0;
	GetExtendedUdpTable (NULL, &required_size, FALSE, AF_INET, UDP_TABLE_OWNER_MODULE, 0);

	if (required_size)
	{
		if (allocated_size < required_size)
		{
			buffer = _r_mem_reallocate (buffer, required_size);
			allocated_size = required_size;
		}

		udp4_table = buffer;
		status = GetExtendedUdpTable (udp4_table, &required_size, FALSE, AF_INET, UDP_TABLE_OWNER_MODULE, 0);

		if (status == NO_ERROR)
		{
			for (ULONG i = 0; i < udp4_table->dwNumEntries; i++)
			{
				RtlZeroMemory (&local_addr, sizeof (local_addr));

				local_addr.S_un.S_addr = udp4_table->table[i].dwLocalAddr;

				network_hash = _app_network_gethash (
					AF_INET,
					udp4_table->table[i].dwOwningPid,
					NULL,
					0,
					&local_addr,
					udp4_table->table[i].dwLocalPort,
					IPPROTO_UDP,
					0
				);

				if (_app_network_isitemfound (network_hash))
				{
					_r_queuedlock_acquireexclusive (&network_context->lock_checker);
					_r_obj_addhashtablepointer (network_context->checker_ptr, network_hash, NULL);
					_r_queuedlock_releaseexclusive (&network_context->lock_checker);

					continue;
				}

				ptr_network = _r_obj_allocate (sizeof (ITEM_NETWORK), &_app_dereferencenetwork);

				if (!_app_network_getpath (ptr_network, udp4_table->table[i].dwOwningPid, udp4_table->table[i].OwningModuleInfo))
				{
					_r_obj_dereference (ptr_network);

					continue;
				}

				ptr_network->af = AF_INET;
				ptr_network->protocol = IPPROTO_UDP;
				ptr_network->protocol_str = _app_db_getprotoname (ptr_network->protocol, ptr_network->af, FALSE);

				ptr_network->local_addr.S_un.S_addr = udp4_table->table[i].dwLocalAddr;
				ptr_network->local_port = _r_byteswap_ushort ((USHORT)udp4_table->table[i].dwLocalPort);

				if (_app_network_isvalidconnection (ptr_network->af, &ptr_network->local_addr))
					ptr_network->is_connection = TRUE;

				_r_queuedlock_acquireexclusive (&network_context->lock_network);
				_r_obj_addhashtablepointer (network_context->network_ptr, network_hash, ptr_network);
				_r_queuedlock_releaseexclusive (&network_context->lock_network);

				_r_queuedlock_acquireexclusive (&network_context->lock_checker);
				_r_obj_addhashtablepointer (network_context->checker_ptr, network_hash, _r_obj_reference (ptr_network->path));
				_r_queuedlock_releaseexclusive (&network_context->lock_checker);
			}
		}
	}

	required_size = 0;
	GetExtendedUdpTable (NULL, &required_size, FALSE, AF_INET6, UDP_TABLE_OWNER_MODULE, 0);

	if (required_size)
	{
		if (allocated_size < required_size)
		{
			buffer = _r_mem_reallocate (buffer, required_size);
			allocated_size = required_size;
		}

		udp6_table = buffer;
		status = GetExtendedUdpTable (udp6_table, &required_size, FALSE, AF_INET6, UDP_TABLE_OWNER_MODULE, 0);

		if (status == NO_ERROR)
		{
			for (ULONG i = 0; i < udp6_table->dwNumEntries; i++)
			{
				network_hash = _app_network_gethash (
					AF_INET6,
					udp6_table->table[i].dwOwningPid,
					NULL,
					0,
					udp6_table->table[i].ucLocalAddr,
					udp6_table->table[i].dwLocalPort,
					IPPROTO_UDP,
					0
				);

				if (_app_network_isitemfound (network_hash))
				{
					_r_queuedlock_acquireexclusive (&network_context->lock_checker);
					_r_obj_addhashtablepointer (network_context->checker_ptr, network_hash, NULL);
					_r_queuedlock_releaseexclusive (&network_context->lock_checker);

					continue;
				}

				ptr_network = _r_obj_allocate (sizeof (ITEM_NETWORK), &_app_dereferencenetwork);

				if (!_app_network_getpath (ptr_network, udp6_table->table[i].dwOwningPid, udp6_table->table[i].OwningModuleInfo))
				{
					_r_obj_dereference (ptr_network);

					continue;
				}

				ptr_network->af = AF_INET6;
				ptr_network->protocol = IPPROTO_UDP;
				ptr_network->protocol_str = _app_db_getprotoname (ptr_network->protocol, ptr_network->af, FALSE);

				RtlCopyMemory (ptr_network->local_addr6.u.Byte, udp6_table->table[i].ucLocalAddr, FWP_V6_ADDR_SIZE);
				ptr_network->local_port = _r_byteswap_ushort ((USHORT)udp6_table->table[i].dwLocalPort);

				if (_app_network_isvalidconnection (ptr_network->af, &ptr_network->local_addr6))
					ptr_network->is_connection = TRUE;

				_r_queuedlock_acquireexclusive (&network_context->lock_network);
				_r_obj_addhashtablepointer (network_context->network_ptr, network_hash, ptr_network);
				_r_queuedlock_releaseexclusive (&network_context->lock_network);

				_r_queuedlock_acquireexclusive (&network_context->lock_checker);
				_r_obj_addhashtablepointer (network_context->checker_ptr, network_hash, _r_obj_reference (ptr_network->path));
				_r_queuedlock_releaseexclusive (&network_context->lock_checker);
			}
		}
	}

	if (buffer)
		_r_mem_free (buffer);
}

_Ret_maybenull_
PITEM_NETWORK _app_network_getitem (
	_In_ ULONG_PTR network_hash
)
{
	PITEM_NETWORK_CONTEXT network_context;
	PITEM_NETWORK ptr_network;

	network_context = _app_network_getcontext ();

	if (!network_context)
		return NULL;

	_r_queuedlock_acquireshared (&network_context->lock_network);
	ptr_network = _r_obj_findhashtablepointer (network_context->network_ptr, network_hash);
	_r_queuedlock_releaseshared (&network_context->lock_network);

	return ptr_network;
}

_Success_ (return != 0)
ULONG_PTR _app_network_getappitem (
	_In_ ULONG_PTR network_hash
)
{
	PITEM_NETWORK ptr_network;
	ULONG_PTR hash_code;

	ptr_network = _app_network_getitem (network_hash);

	if (!ptr_network)
		return 0;

	hash_code = ptr_network->app_hash;

	_r_obj_dereference (ptr_network);

	return hash_code;
}

ULONG_PTR _app_network_gethash (
	_In_ ADDRESS_FAMILY af,
	_In_ ULONG pid,
	_In_opt_ LPCVOID remote_addr,
	_In_opt_ ULONG remote_port,
	_In_opt_ LPCVOID local_addr,
	_In_opt_ ULONG local_port,
	_In_ UINT8 proto,
	_In_opt_ ULONG state
)
{
	WCHAR remote_address[LEN_IP_MAX] = {0};
	WCHAR local_address[LEN_IP_MAX] = {0};
	PR_STRING network_string;
	ULONG_PTR network_hash;

	if (remote_addr)
		_app_formatip (af, remote_addr, remote_address, RTL_NUMBER_OF (remote_address), FALSE);

	if (local_addr)
		_app_formatip (af, local_addr, local_address, RTL_NUMBER_OF (local_address), FALSE);

	network_string = _r_format_string (
		L"%" TEXT (PRIu8) L"_%" TEXT (PR_ULONG) L"_%s_%" TEXT (PR_ULONG) \
		L"_%s_%" TEXT (PR_ULONG) L"_%" TEXT (PRIu8) L"_%" TEXT (PR_ULONG),
		af,
		pid,
		remote_address,
		remote_port,
		local_address,
		local_port,
		proto,
		state
	);

	network_hash = _r_str_gethash2 (&network_string->sr, TRUE);

	_r_obj_dereference (network_string);

	return network_hash;
}

BOOLEAN _app_network_getpath (
	_Inout_ PITEM_NETWORK ptr_network,
	_In_ ULONG pid,
	_In_opt_ PULONG64 modules
)
{
	PTOKEN_APPCONTAINER_INFORMATION app_container = NULL;
	PR_STRING process_name = NULL;
	HANDLE hprocess;
	HANDLE htoken;
	NTSTATUS status;

	if (pid == PROC_WAITING_PID)
	{
		ptr_network->app_hash = 0;
		ptr_network->type = DATA_APP_REGULAR;
		ptr_network->path = _r_obj_createstring (PROC_WAITING_NAME);

		return TRUE;
	}
	else if (pid == PROC_SYSTEM_PID)
	{
		ptr_network->app_hash = config.ntoskrnl_hash;
		ptr_network->type = DATA_APP_REGULAR;
		ptr_network->path = _r_obj_createstring (PROC_SYSTEM_NAME);

		return TRUE;
	}

	if (modules)
	{
		process_name = _r_sys_querytaginformation (UlongToHandle (pid), UlongToPtr (*(PULONG)modules));

		if (process_name)
			ptr_network->type = DATA_APP_SERVICE;
	}

	if (!process_name)
	{
		status = _r_sys_openprocess (ULongToHandle (pid), PROCESS_QUERY_LIMITED_INFORMATION, &hprocess);

		if (NT_SUCCESS (status))
		{
			if (_r_sys_isprocessimmersive (hprocess))
			{
				ptr_network->type = DATA_APP_UWP;
			}
			else
			{
				ptr_network->type = DATA_APP_REGULAR;
			}

			if (ptr_network->type == DATA_APP_UWP)
			{
				status = NtOpenProcessTokenEx (hprocess, TOKEN_QUERY, 0, &htoken);

				if (NT_SUCCESS (status))
				{
					status = _r_sys_querytokeninformation (htoken, TokenAppContainerSid, &app_container);

					if (NT_SUCCESS (status))
					{
						_r_str_fromsid (app_container->TokenAppContainer, &process_name);

						_r_mem_free (app_container);
					}

					NtClose (htoken);
				}
			}

			if (!process_name)
			{
				status = _r_sys_queryprocessstring (hprocess, ProcessImageFileNameWin32, &process_name);

				// fix for WSL processes (issue #606)
				if (status == STATUS_UNSUCCESSFUL)
					status = _r_sys_queryprocessstring (hprocess, ProcessImageFileName, &process_name);
			}

			NtClose (hprocess);
		}
	}

	if (process_name)
	{
		ptr_network->app_hash = _r_str_gethash2 (&process_name->sr, TRUE);
		ptr_network->path = process_name;

		return TRUE;
	}

	return FALSE;
}

BOOLEAN _app_network_isapphaveconnection (
	_In_ ULONG_PTR app_hash
)
{
	PITEM_NETWORK_CONTEXT network_context;
	PITEM_NETWORK ptr_network = NULL;
	ULONG_PTR enum_key = 0;

	network_context = _app_network_getcontext ();

	if (!network_context)
		return FALSE;

	_r_queuedlock_acquireshared (&network_context->lock_network);

	while (_r_obj_enumhashtablepointer (network_context->network_ptr, &ptr_network, NULL, &enum_key))
	{
		if (ptr_network->app_hash != app_hash)
			continue;

		if (ptr_network->is_connection)
		{
			_r_queuedlock_releaseshared (&network_context->lock_network);

			return TRUE;
		}
	}

	_r_queuedlock_releaseshared (&network_context->lock_network);

	return FALSE;
}

BOOLEAN _app_network_isitemfound (
	_In_ ULONG_PTR network_hash
)
{
	PITEM_NETWORK_CONTEXT network_context;
	BOOLEAN is_found;

	network_context = _app_network_getcontext ();

	if (!network_context)
		return FALSE;

	_r_queuedlock_acquireshared (&network_context->lock_network);
	is_found = (_r_obj_findhashtable (network_context->network_ptr, network_hash) != NULL);
	_r_queuedlock_releaseshared (&network_context->lock_network);

	return is_found;
}

BOOLEAN _app_network_isvalidconnection (
	_In_ ADDRESS_FAMILY af,
	_In_ LPCVOID address
)
{
	PIN_ADDR p4addr;
	PIN6_ADDR p6addr;

	switch (af)
	{
		case AF_INET:
		{
			p4addr = (PIN_ADDR)address;

			return (!IN4_IS_ADDR_UNSPECIFIED (p4addr) &&
					!IN4_IS_ADDR_LOOPBACK (p4addr) &&
					!IN4_IS_ADDR_LINKLOCAL (p4addr) &&
					!IN4_IS_ADDR_MULTICAST (p4addr) &&
					!IN4_IS_ADDR_MC_ADMINLOCAL (p4addr) &&
					!IN4_IS_ADDR_RFC1918 (p4addr));
		}

		case AF_INET6:
		{
			p6addr = (PIN6_ADDR)address;

			return (!IN6_IS_ADDR_UNSPECIFIED (p6addr) &&
					!IN6_IS_ADDR_LOOPBACK (p6addr) &&
					!IN6_IS_ADDR_LINKLOCAL (p6addr) &&
					!IN6_IS_ADDR_MULTICAST (p6addr) &&
					!IN6_IS_ADDR_SITELOCAL (p6addr) &&
					!IN6_IS_ADDR_ANYCAST (p6addr));
		}
	}

	return FALSE;
}

VOID _app_network_printlistviewtable (
	_Inout_ PITEM_NETWORK_CONTEXT network_context
)
{
	PITEM_NETWORK ptr_network = NULL;
	PR_STRING string;
	ULONG_PTR app_hash;
	ULONG_PTR network_hash;
	ULONG_PTR enum_key = 0;
	INT item_count;
	BOOLEAN is_highlight = FALSE;
	BOOLEAN is_refresh = FALSE;

	if (_r_config_getboolean (L"IsEnableHighlighting", TRUE) && _r_config_getboolean_ex (L"IsHighlightConnection", TRUE, L"colors"))
		is_highlight = TRUE;

	// add new connections into listview
	_r_queuedlock_acquireshared (&network_context->lock_network);

	while (_r_obj_enumhashtablepointer (network_context->network_ptr, &ptr_network, &network_hash, &enum_key))
	{
		string = _r_obj_findhashtablepointer (network_context->checker_ptr, network_hash);

		if (!string)
			continue;

		_app_listview_addnetworkitem (network_context->hwnd, network_hash);

		if (ptr_network->path && ptr_network->app_hash)
			_app_getfileinformation (ptr_network->path, ptr_network->app_hash, ptr_network->type, IDC_NETWORK);

		// resolve network address
		_app_queue_resolver (network_context->hwnd, IDC_NETWORK, network_hash, ptr_network);

		_r_obj_dereference (string);

		is_refresh = TRUE;
	}

	_r_queuedlock_releaseshared (&network_context->lock_network);

	// refresh network tab
	if (is_refresh)
		_app_listview_updateby_id (network_context->hwnd, IDC_NETWORK, PR_UPDATE_NORESIZE);

	// remove closed connections from list
	item_count = _r_listview_getitemcount (network_context->hwnd, IDC_NETWORK);

	if (!item_count)
		return;

	for (INT i = item_count - 1; i != -1; i--)
	{
		network_hash = _app_listview_getitemcontext (network_context->hwnd, IDC_NETWORK, i);

		if (_r_obj_findhashtable (network_context->checker_ptr, network_hash))
			continue;

		_r_listview_deleteitem (network_context->hwnd, IDC_NETWORK, i);

		app_hash = _app_network_getappitem (network_hash);

		_app_network_removeitem (network_hash);

		// redraw listview item
		if (!is_highlight || !app_hash)
			continue;

		_app_listview_updateby_param (network_context->hwnd, app_hash, PR_SETITEM_REDRAW, TRUE);
	}
}

VOID _app_network_removeitem (
	_In_ ULONG_PTR network_hash
)
{
	PITEM_NETWORK_CONTEXT network_context;

	network_context = _app_network_getcontext ();

	if (!network_context)
		return;

	_r_queuedlock_acquireexclusive (&network_context->lock_network);
	_r_obj_removehashtableitem (network_context->network_ptr, network_hash);
	_r_queuedlock_releaseexclusive (&network_context->lock_network);
}

NTSTATUS NTAPI _app_network_threadproc (
	_In_ PVOID arglist
)
{
	PITEM_NETWORK_CONTEXT network_context;

	network_context = (PITEM_NETWORK_CONTEXT)arglist;

	while (TRUE)
	{
		// update network table
		_app_network_generatetable (network_context);
		_app_network_printlistviewtable (network_context);

		_r_sys_waitforsingleobject (NtCurrentThread (), NETWORK_TIMEOUT);
	}

	_app_network_uninitialize (network_context);

	return STATUS_SUCCESS;
}
