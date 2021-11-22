// simplewall
// Copyright (c) 2019-2021 Henry++

#include "global.h"

VOID _app_network_initialize (_In_ HWND hwnd)
{
	R_ENVIRONMENT environment;
	BOOLEAN is_enabled;

	is_enabled = _r_config_getboolean (L"IsNetworkMonitorEnabled", TRUE);

	if (!is_enabled)
		return;

	_r_sys_setenvironment (&environment, THREAD_PRIORITY_HIGHEST, IoPriorityNormal, MEMORY_PRIORITY_NORMAL);

	// create network monitor thread
	_r_sys_createthread (&_app_network_threadproc, hwnd, NULL, &environment);
}

VOID _app_network_generatetable (_Inout_ PR_HASHTABLE network_ptr, _Inout_ PR_HASHTABLE checker_map)
{
	PITEM_NETWORK ptr_network;
	ULONG_PTR network_hash;

	PVOID buffer;
	ULONG allocated_size;
	ULONG required_size;

	_r_obj_clearhashtable (checker_map);

	required_size = 0;
	GetExtendedTcpTable (NULL, &required_size, FALSE, AF_INET, TCP_TABLE_OWNER_MODULE_ALL, 0);

	allocated_size = required_size;
	buffer = _r_mem_allocatezero (allocated_size);

	if (required_size)
	{
		PMIB_TCPTABLE_OWNER_MODULE tcp4_table = buffer;

		if (GetExtendedTcpTable (tcp4_table, &required_size, FALSE, AF_INET, TCP_TABLE_OWNER_MODULE_ALL, 0) == NO_ERROR)
		{
			for (ULONG i = 0; i < tcp4_table->dwNumEntries; i++)
			{
				IN_ADDR remote_addr = {0};
				IN_ADDR local_addr = {0};

				remote_addr.S_un.S_addr = tcp4_table->table[i].dwRemoteAddr;
				local_addr.S_un.S_addr = tcp4_table->table[i].dwLocalAddr;

				network_hash = _app_network_gethash (AF_INET, tcp4_table->table[i].dwOwningPid, &remote_addr, tcp4_table->table[i].dwRemotePort, &local_addr, tcp4_table->table[i].dwLocalPort, IPPROTO_TCP, tcp4_table->table[i].dwState);

				if (_app_isnetworkfound (network_hash))
				{
					_r_obj_addhashtablepointer (checker_map, network_hash, NULL);
					continue;
				}

				ptr_network = _r_obj_allocate (sizeof (ITEM_NETWORK), &_app_dereferencenetwork);

				if (!_app_network_getpath (tcp4_table->table[i].dwOwningPid, tcp4_table->table[i].OwningModuleInfo, ptr_network))
				{
					_r_obj_dereference (ptr_network);
					continue;
				}

				ptr_network->af = AF_INET;
				ptr_network->protocol = IPPROTO_TCP;
				ptr_network->protocol_str = _app_getprotoname (ptr_network->protocol, ptr_network->af, FALSE);

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

				_r_queuedlock_acquireexclusive (&lock_network);

				_r_obj_addhashtablepointer (network_ptr, network_hash, ptr_network);

				_r_queuedlock_releaseexclusive (&lock_network);

				_r_obj_addhashtablepointer (checker_map, network_hash, _r_obj_reference (ptr_network->path));
			}
		}
	}

	required_size = 0;
	GetExtendedTcpTable (NULL, &required_size, FALSE, AF_INET6, TCP_TABLE_OWNER_MODULE_ALL, 0);

	if (required_size)
	{
		if (allocated_size < required_size)
		{
			buffer = _r_mem_reallocatezero (buffer, required_size);
			allocated_size = required_size;
		}

		PMIB_TCP6TABLE_OWNER_MODULE tcp6_table = buffer;

		if (GetExtendedTcpTable (tcp6_table, &required_size, FALSE, AF_INET6, TCP_TABLE_OWNER_MODULE_ALL, 0) == NO_ERROR)
		{
			for (ULONG i = 0; i < tcp6_table->dwNumEntries; i++)
			{
				network_hash = _app_network_gethash (AF_INET6, tcp6_table->table[i].dwOwningPid, tcp6_table->table[i].ucRemoteAddr, tcp6_table->table[i].dwRemotePort, tcp6_table->table[i].ucLocalAddr, tcp6_table->table[i].dwLocalPort, IPPROTO_TCP, tcp6_table->table[i].dwState);

				if (_app_isnetworkfound (network_hash))
				{
					_r_obj_addhashtablepointer (checker_map, network_hash, NULL);
					continue;
				}

				ptr_network = _r_obj_allocate (sizeof (ITEM_NETWORK), &_app_dereferencenetwork);

				if (!_app_network_getpath (tcp6_table->table[i].dwOwningPid, tcp6_table->table[i].OwningModuleInfo, ptr_network))
				{
					_r_obj_dereference (ptr_network);
					continue;
				}

				ptr_network->af = AF_INET6;
				ptr_network->protocol = IPPROTO_TCP;
				ptr_network->protocol_str = _app_getprotoname (ptr_network->protocol, ptr_network->af, FALSE);

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

				_r_queuedlock_acquireexclusive (&lock_network);

				_r_obj_addhashtablepointer (network_ptr, network_hash, ptr_network);

				_r_queuedlock_releaseexclusive (&lock_network);

				_r_obj_addhashtablepointer (checker_map, network_hash, _r_obj_reference (ptr_network->path));
			}
		}
	}

	required_size = 0;
	GetExtendedUdpTable (NULL, &required_size, FALSE, AF_INET, UDP_TABLE_OWNER_MODULE, 0);

	if (required_size)
	{
		if (allocated_size < required_size)
		{
			buffer = _r_mem_reallocatezero (buffer, required_size);
			allocated_size = required_size;
		}

		PMIB_UDPTABLE_OWNER_MODULE udp4_table = buffer;

		if (GetExtendedUdpTable (udp4_table, &required_size, FALSE, AF_INET, UDP_TABLE_OWNER_MODULE, 0) == NO_ERROR)
		{
			for (ULONG i = 0; i < udp4_table->dwNumEntries; i++)
			{
				IN_ADDR local_addr = {0};
				local_addr.S_un.S_addr = udp4_table->table[i].dwLocalAddr;

				network_hash = _app_network_gethash (AF_INET, udp4_table->table[i].dwOwningPid, NULL, 0, &local_addr, udp4_table->table[i].dwLocalPort, IPPROTO_UDP, 0);

				if (_app_isnetworkfound (network_hash))
				{
					_r_obj_addhashtablepointer (checker_map, network_hash, NULL);
					continue;
				}

				ptr_network = _r_obj_allocate (sizeof (ITEM_NETWORK), &_app_dereferencenetwork);

				if (!_app_network_getpath (udp4_table->table[i].dwOwningPid, udp4_table->table[i].OwningModuleInfo, ptr_network))
				{
					_r_obj_dereference (ptr_network);
					continue;
				}

				ptr_network->af = AF_INET;
				ptr_network->protocol = IPPROTO_UDP;
				ptr_network->protocol_str = _app_getprotoname (ptr_network->protocol, ptr_network->af, FALSE);

				ptr_network->local_addr.S_un.S_addr = udp4_table->table[i].dwLocalAddr;
				ptr_network->local_port = _r_byteswap_ushort ((USHORT)udp4_table->table[i].dwLocalPort);

				if (_app_network_isvalidconnection (ptr_network->af, &ptr_network->local_addr))
					ptr_network->is_connection = TRUE;

				_r_queuedlock_acquireexclusive (&lock_network);

				_r_obj_addhashtablepointer (network_ptr, network_hash, ptr_network);

				_r_queuedlock_releaseexclusive (&lock_network);

				_r_obj_addhashtablepointer (checker_map, network_hash, _r_obj_reference (ptr_network->path));
			}
		}
	}

	required_size = 0;
	GetExtendedUdpTable (NULL, &required_size, FALSE, AF_INET6, UDP_TABLE_OWNER_MODULE, 0);

	if (required_size)
	{
		if (allocated_size < required_size)
		{
			buffer = _r_mem_reallocatezero (buffer, required_size);
			allocated_size = required_size;
		}

		PMIB_UDP6TABLE_OWNER_MODULE udp6_table = buffer;

		if (GetExtendedUdpTable (udp6_table, &required_size, FALSE, AF_INET6, UDP_TABLE_OWNER_MODULE, 0) == NO_ERROR)
		{
			for (ULONG i = 0; i < udp6_table->dwNumEntries; i++)
			{
				network_hash = _app_network_gethash (AF_INET6, udp6_table->table[i].dwOwningPid, NULL, 0, udp6_table->table[i].ucLocalAddr, udp6_table->table[i].dwLocalPort, IPPROTO_UDP, 0);

				if (_app_isnetworkfound (network_hash))
				{
					_r_obj_addhashtablepointer (checker_map, network_hash, NULL);
					continue;
				}

				ptr_network = _r_obj_allocate (sizeof (ITEM_NETWORK), &_app_dereferencenetwork);

				if (!_app_network_getpath (udp6_table->table[i].dwOwningPid, udp6_table->table[i].OwningModuleInfo, ptr_network))
				{
					_r_obj_dereference (ptr_network);
					continue;
				}

				ptr_network->af = AF_INET6;
				ptr_network->protocol = IPPROTO_UDP;
				ptr_network->protocol_str = _app_getprotoname (ptr_network->protocol, ptr_network->af, FALSE);

				RtlCopyMemory (ptr_network->local_addr6.u.Byte, udp6_table->table[i].ucLocalAddr, FWP_V6_ADDR_SIZE);
				ptr_network->local_port = _r_byteswap_ushort ((USHORT)udp6_table->table[i].dwLocalPort);

				if (_app_network_isvalidconnection (ptr_network->af, &ptr_network->local_addr6))
					ptr_network->is_connection = TRUE;

				_r_queuedlock_acquireexclusive (&lock_network);

				_r_obj_addhashtablepointer (network_ptr, network_hash, ptr_network);

				_r_queuedlock_releaseexclusive (&lock_network);

				_r_obj_addhashtablepointer (checker_map, network_hash, _r_obj_reference (ptr_network->path));
			}
		}
	}

	if (buffer)
		_r_mem_free (buffer);
}

ULONG_PTR _app_network_gethash (_In_ ADDRESS_FAMILY af, _In_ ULONG pid, _In_opt_ LPCVOID remote_addr, _In_opt_ ULONG remote_port, _In_opt_ LPCVOID local_addr, _In_opt_ ULONG local_port, _In_ UINT8 proto, _In_ ULONG state)
{
	WCHAR remote_address[LEN_IP_MAX] = {0};
	WCHAR local_address[LEN_IP_MAX] = {0};
	PR_STRING network_string;
	ULONG_PTR network_hash;

	if (remote_addr)
		_app_formatip (af, remote_addr, remote_address, RTL_NUMBER_OF (remote_address), FALSE);

	if (local_addr)
		_app_formatip (af, local_addr, local_address, RTL_NUMBER_OF (local_address), FALSE);

	network_string = _r_format_string (L"%" TEXT (PRIu8) L"_%" TEXT (PR_ULONG) L"_%s_%" TEXT (PR_ULONG) L"_%s_%" TEXT (PR_ULONG) L"_%" TEXT (PRIu8) L"_%" TEXT (PR_ULONG),
									   af,
									   pid,
									   remote_address,
									   remote_port,
									   local_address,
									   local_port,
									   proto,
									   state
	);

	if (!network_string)
		return 0;

	network_hash = _r_str_gethash2 (network_string, TRUE);

	_r_obj_dereference (network_string);

	return network_hash;
}

BOOLEAN _app_network_getpath (_In_ ULONG pid, _In_opt_ PULONG64 modules, _Inout_ PITEM_NETWORK ptr_network)
{
	PR_STRING process_name;
	NTSTATUS status;
	HANDLE hprocess;

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

	process_name = NULL;

	if (modules)
	{
		process_name = _r_sys_querytaginformation (UlongToHandle (pid), UlongToPtr (*(PULONG)modules));

		if (process_name)
			ptr_network->type = DATA_APP_SERVICE;
	}

	if (!process_name)
	{
		status = _r_sys_openprocess (UlongToHandle (pid), PROCESS_QUERY_LIMITED_INFORMATION, &hprocess);

		if (NT_SUCCESS (status))
		{
			if (_r_sys_isosversiongreaterorequal (WINDOWS_8) && _r_sys_isprocessimmersive (hprocess))
			{
				ptr_network->type = DATA_APP_UWP;
			}
			else
			{
				ptr_network->type = DATA_APP_REGULAR;
			}

			status = _r_sys_queryprocessstring (hprocess, ProcessImageFileNameWin32, &process_name);

			// fix for WSL processes (issue #606)
			if (status == STATUS_UNSUCCESSFUL)
				status = _r_sys_queryprocessstring (hprocess, ProcessImageFileName, &process_name);

			NtClose (hprocess);
		}
	}

	if (process_name)
	{
		ptr_network->app_hash = _r_str_gethash2 (process_name, TRUE);
		ptr_network->path = process_name;

		return TRUE;
	}

	return FALSE;
}

BOOLEAN _app_network_isvalidconnection (_In_ ADDRESS_FAMILY af, _In_ LPCVOID address)
{
	PIN_ADDR p4addr;
	PIN6_ADDR p6addr;

	if (af == AF_INET)
	{
		p4addr = (PIN_ADDR)address;

		return (!IN4_IS_ADDR_UNSPECIFIED (p4addr) &&
				!IN4_IS_ADDR_LOOPBACK (p4addr) &&
				!IN4_IS_ADDR_LINKLOCAL (p4addr) &&
				!IN4_IS_ADDR_MULTICAST (p4addr) &&
				!IN4_IS_ADDR_MC_ADMINLOCAL (p4addr) &&
				!IN4_IS_ADDR_RFC1918 (p4addr)
				);
	}
	else if (af == AF_INET6)
	{
		p6addr = (PIN6_ADDR)address;

		return  (!IN6_IS_ADDR_UNSPECIFIED (p6addr) &&
				 !IN6_IS_ADDR_LOOPBACK (p6addr) &&
				 !IN6_IS_ADDR_LINKLOCAL (p6addr) &&
				 !IN6_IS_ADDR_MULTICAST (p6addr) &&
				 !IN6_IS_ADDR_SITELOCAL (p6addr) &&
				 !IN6_IS_ADDR_ANYCAST (p6addr)
				 );
	}

	return FALSE;
}

NTSTATUS NTAPI _app_network_threadproc (_In_ PVOID arglist)
{
	PR_HASHTABLE checker_map;
	PITEM_NETWORK ptr_network;
	PITEM_CONTEXT context;
	ULONG_PTR network_hash;
	PR_STRING string;
	HWND hwnd;
	ULONG_PTR app_hash;
	SIZE_T enum_key;
	INT item_count;
	INT item_id;
	BOOLEAN is_highlighting_enabled;
	BOOLEAN is_refresh;

	hwnd = (HWND)arglist;
	checker_map = _r_obj_createhashtablepointer (8);

	while (TRUE)
	{
		_app_network_generatetable (network_table, checker_map);

		is_highlighting_enabled = _r_config_getboolean (L"IsEnableHighlighting", TRUE) && _r_config_getboolean_ex (L"IsHighlightConnection", TRUE, L"colors");
		is_refresh = FALSE;
		enum_key = 0;

		// add new connections into list
		while (_r_obj_enumhashtablepointer (network_table, &ptr_network, &network_hash, &enum_key))
		{
			string = _r_obj_findhashtablepointer (checker_map, network_hash);

			if (!string)
				continue;

			_r_obj_dereference (string);

			item_id = _r_listview_getitemcount (hwnd, IDC_NETWORK);

			_r_listview_additem_ex (hwnd, IDC_NETWORK, item_id, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK, I_GROUPIDCALLBACK, _app_createlistviewcontext (network_hash));

			if (ptr_network->path && ptr_network->app_hash)
				_app_queryfileinformation (ptr_network->path, ptr_network->app_hash, ptr_network->type, IDC_NETWORK);

			// resolve network address
			context = _r_freelist_allocateitem (&context_free_list);

			context->hwnd = hwnd;
			context->listview_id = IDC_NETWORK;
			context->lparam = network_hash;
			context->ptr_network = _r_obj_reference (ptr_network);

			_r_workqueue_queueitem (&resolver_queue, &_app_queueresolveinformation, context);

			is_refresh = TRUE;
		}

		// refresh network tab as well
		if (is_refresh)
			_app_updatelistviewbylparam (hwnd, IDC_NETWORK, PR_UPDATE_NORESIZE);

		// remove closed connections from list
		item_count = _r_listview_getitemcount (hwnd, IDC_NETWORK);

		if (item_count)
		{
			for (INT i = item_count - 1; i != -1; i--)
			{
				network_hash = _app_getlistviewitemcontext (hwnd, IDC_NETWORK, i);

				if (_r_obj_findhashtable (checker_map, network_hash))
					continue;

				_r_listview_deleteitem (hwnd, IDC_NETWORK, i);

				app_hash = _app_getnetworkapp (network_hash);

				_r_queuedlock_acquireexclusive (&lock_network);

				_r_obj_removehashtablepointer (network_table, network_hash);

				_r_queuedlock_releaseexclusive (&lock_network);

				// redraw listview item
				if (app_hash)
				{
					if (is_highlighting_enabled)
						_app_setlistviewbylparam (hwnd, app_hash, PR_SETITEM_REDRAW, TRUE);
				}
			}
		}

		WaitForSingleObjectEx (NtCurrentThread (), NETWORK_TIMEOUT, FALSE);
	}

	return STATUS_SUCCESS;
}
