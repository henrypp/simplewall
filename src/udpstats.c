// simplewall
// Copyright (c) 2026 Henry++
// Copyright (c) 2026 Tarkiin
// UDP events contain metadata only; no packet payload is captured or stored
#include "global.h"

FORCEINLINE static ULONG udp_bucket (
	_In_ ULONG pid,
	_In_ USHORT port
)
{
	return (pid ^ port) % UDP_BUCKETS;
}

static VOID udp_initialize_properties (
	_Out_ PUDP_TRACE_PROPERTIES buffer
)
{
	RtlZeroMemory (buffer, sizeof (UDP_TRACE_PROPERTIES));

	buffer->properties.Wnode.BufferSize = sizeof (UDP_TRACE_PROPERTIES);
	buffer->properties.Wnode.Guid = udp_session;
	buffer->properties.Wnode.ClientContext = 1; // QPC; ProcessTrace converts to FILETIME
	buffer->properties.Wnode.Flags = WNODE_FLAG_TRACED_GUID;
	buffer->properties.BufferSize = 64;
	//buffer->properties.MinimumBuffers = 0; // already 0
	buffer->properties.MaximumBuffers = 128;
	buffer->properties.FlushTimer = 1;
	buffer->properties.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
	buffer->properties.LoggerNameOffset = FIELD_OFFSET (UDP_TRACE_PROPERTIES, name);
}

// select a bound socket, not a remote flow. UDP listeners may use wildcard addresses and IPv6 sockets may receive IPv4. never credit two rows for one event.
static INT udp_match (
	_In_ const PUDP_ENDPOINT endpoint,
	_In_ ADDRESS_FAMILY af,
	_In_ const PBYTE address
)
{
	ULONG_PTR length;
	const BYTE mapped[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF};
	const BYTE zero[16] = {0};

	if (endpoint->af == af)
	{
		length = (af == AF_INET) ? 4 : 16; // set correct sizes

		if (RtlEqualMemory (endpoint->address, address, length))
			return 3;

		if (RtlEqualMemory (endpoint->address, zero, length))
			return 2;
	}
	else if (af == AF_INET && endpoint->af == AF_INET6)
	{
		if (RtlEqualMemory (endpoint->address, mapped, 12) && !RtlEqualMemory (endpoint->address + 12, address, 4)) // set correct sizes
			return 2;

		if (RtlEqualMemory (endpoint->address, zero, 16)) // set correct sizes
			return 1;
	}

	return 0;
}

static VOID udp_account (
	_In_ PSW_UDP_STATS stats,
	_In_ ULONG pid,
	_In_ ULONG size,
	_In_ USHORT port,
	_In_ ADDRESS_FAMILY af,
	_In_ const PBYTE address,
	_In_ ULONG scope_id,
	_In_ BOOLEAN is_receive,
	_In_ ULONG64 timestamp,
	_In_ ULONG error_code
)
{
	UDP_ENTRY *best = NULL, *entry;
	INT candidate, score = 0;
	BOOLEAN is_ambiguous = FALSE;

	AcquireSRWLockExclusive (&stats->lock);

	for (entry = stats->buckets[udp_bucket (pid, port)]; entry; entry = entry->next)
	{
		if (entry->endpoint.pid != pid || entry->endpoint.port != port)
			continue;

		if (af == AF_INET6 && scope_id != MAXDWORD && entry->endpoint.scope_id != scope_id)
			continue;

		candidate = udp_match (&entry->endpoint, af, address);

		if (candidate > score)
		{
			best = entry;
			score = candidate;
			is_ambiguous = FALSE;
		}
		else if (candidate && candidate == score)
		{
			is_ambiguous = TRUE;
		}
	}

	if (best && !is_ambiguous && timestamp >= best->since)
	{
		if (error_code != ERROR_SUCCESS)
			best->error_code = error_code;

		best->is_observed = TRUE;

		if (is_receive)
		{
			best->received += size;
		}
		else
		{
			best->sent += size;
		}
	}
	else if (is_ambiguous)
	{
		// never silently attribute an event to one of several equally good rows
		for (entry = stats->buckets[udp_bucket (pid, port)]; entry; entry = entry->next)
		{
			if (entry->endpoint.pid == pid && entry->endpoint.port == port && udp_match (&entry->endpoint, af, address) == score)
				entry->error_code = ERROR_INVALID_DATA;
		}
	}

	ReleaseSRWLockExclusive (&stats->lock);
}

static ULONG udp_u32 (
	_In_ const PBYTE data
)
{
	ULONG value;

	RtlCopyMemory (&value, data, sizeof (ULONG));

	return value;
}

static VOID udp_afd_event (
	_In_ PSW_UDP_STATS stats,
	_In_ PEVENT_RECORD event
)
{
	UDP_AFD_SOCKET **link, *socket;
	ULONG64 handle = 0;
	ULONG pointer_size = (event->EventHeader.Flags & EVENT_HEADER_FLAG_32_BIT_HEADER) ? sizeof (ULONG) : sizeof (ULONG64), prefix = 8 + 2 * pointer_size, length, size_offset;
	USHORT af, port, id = event->EventHeader.EventDescriptor.Id;
	const PBYTE data = (const PBYTE)(event->UserData);
	BOOLEAN is_receive = id == 1004 || id == 1006 || id == 1009 || id == 1012 || id == 1015;
	BOOLEAN is_transfer = is_receive || id == 1003 || id == 1005 || id == 1007 || id == 1011 || id == 1013;

	// RIO uses a different completion path. mark just that socket unavailable.
	if (id == 4006)
	{
		if (event->EventHeader.EventDescriptor.Version != 0 || !data || event->UserDataLength < pointer_size)
			goto unsupported;

		RtlCopyMemory (&handle, data, pointer_size);

		for (socket = stats->sockets[(handle >> 4) % UDP_BUCKETS]; socket; socket = socket->next)
		{
			if (socket->handle == handle)
			{
				socket->is_unsupported = TRUE;

				if (socket->is_bound)
				{
					udp_account (
						stats,
						socket->endpoint.pid,
						0,
						socket->endpoint.port,
						socket->endpoint.af,
						socket->endpoint.address,
						socket->endpoint.scope_id,
						FALSE,
						event->EventHeader.TimeStamp.QuadPart,
						ERROR_NOT_SUPPORTED
					);
				}

				break;
			}
		}

		return;
	}

	if (id != 1000 && id != 1001 && id != 1002 && id != 1030 && !is_transfer)
		return;

	if (event->EventHeader.EventDescriptor.Version != 0 || !data || event->UserDataLength < prefix)
		goto unsupported;

	RtlCopyMemory (&handle, data + 8 + pointer_size, pointer_size);

	link = &stats->sockets[(handle >> 4) % UDP_BUCKETS];

	while (*link && (*link)->handle != handle)
	{
		link = &(*link)->next;
	}

	socket = *link;

	if ((id == 1001 || id == 1002) && udp_u32 (data) == 0)
	{
		if (socket)
		{
			*link = socket->next;

			stats->socket_count -= 1;

			_r_mem_free (socket);
		}

		return;
	}

	if (id == 1000 && udp_u32 (data) == 0)
	{
		if (event->UserDataLength < prefix + 12 + pointer_size + 4)
			goto unsupported;

		// kernel endpoint addresses are reused, including by TCP sockets
		if (socket)
		{
			*link = socket->next;

			stats->socket_count -= 1;

			_r_mem_free (socket);

			socket = NULL;
		}

		if (udp_u32 (data + prefix + 4) != SOCK_DGRAM || udp_u32 (data + prefix + 8) != IPPROTO_UDP)
			return;

		if (!socket)
		{
			if (stats->socket_count >= UDP_MAX_ENDPOINTS)
			{
				_InterlockedExchange (&stats->error_code, ERROR_NOT_ENOUGH_MEMORY);

				return;
			}

			socket = (UDP_AFD_SOCKET *)_r_mem_allocatesafe (sizeof (UDP_AFD_SOCKET));

			if (!socket)
			{
				_InterlockedExchange (&stats->error_code, ERROR_NOT_ENOUGH_MEMORY);

				return;
			}

			socket->next = *link;

			*link = socket;

			stats->socket_count += 1;
		}

		socket->handle = handle;
		socket->is_bound = FALSE;

		RtlZeroMemory (&socket->endpoint, sizeof (UDP_ENDPOINT));

		socket->endpoint.pid = udp_u32 (data + prefix + 12);

		return;
	}

	if (!socket || udp_u32 (data) != 1)
		return;

	if (id == 1030)
	{
		if (event->UserDataLength < prefix + 8)
			goto unsupported;

		if (udp_u32 (data + prefix) != ERROR_SUCCESS)
			return;

		length = udp_u32 (data + prefix + 4);

		if (length > event->UserDataLength - prefix - 8 || length < 4)
			goto unsupported;

		RtlCopyMemory (&af, data + prefix + 8, 2);
		RtlCopyMemory (&port, data + prefix + 10, 2);

		if ((af != AF_INET && af != AF_INET6) || length < (af == AF_INET ? 16U : 28U))
			goto unsupported;

		socket->endpoint.af = af;
		socket->endpoint.port = (USHORT)((port >> 8) | (port << 8));

		RtlCopyMemory (socket->endpoint.address, data + prefix + (af == AF_INET ? 12 : 16), af == AF_INET ? 4 : 16);

		socket->endpoint.scope_id = (af == AF_INET6) ? udp_u32 (data + prefix + 32) : 0;
		socket->is_bound = socket->endpoint.port != 0;

		if (socket->is_bound && socket->is_unsupported)
		{
			udp_account (
				stats,
				socket->endpoint.pid,
				0,
				socket->endpoint.port,
				af,
				socket->endpoint.address,
				socket->endpoint.scope_id,
				FALSE,
				event->EventHeader.TimeStamp.QuadPart,
				ERROR_NOT_SUPPORTED
			);
		}

		return;
	}

	if (is_transfer && socket->is_bound)
	{
		size_offset = prefix + 4 + pointer_size;

		if (event->UserDataLength < size_offset + 8)
			goto unsupported;

		if (udp_u32 (data + size_offset + 4) != ERROR_SUCCESS)
			return;

		udp_account (
			stats,
			socket->endpoint.pid,
			udp_u32 (data + size_offset),
			socket->endpoint.port,
			socket->endpoint.af,
			socket->endpoint.address,
			socket->endpoint.scope_id,
			is_receive,
			event->EventHeader.TimeStamp.QuadPart,
			socket->is_unsupported ? ERROR_NOT_SUPPORTED : ERROR_SUCCESS
		);
	}

	return;

unsupported:

	_InterlockedExchange (&stats->error_code, ERROR_NOT_SUPPORTED);
}

static VOID WINAPI udp_event (
	_In_ PEVENT_RECORD event
)
{
	PSW_UDP_STATS stats = event->UserContext;
#if defined(UDP_STATS_DIAGNOSTIC)
	UDP_STATS_DIAGNOSTIC (event);
#endif // UDP_STATS_DIAGNOSTIC

	if (_InterlockedCompareExchange (&stats->stopping, 0, 0))
		return;

	if (IsEqualGUID (&event->EventHeader.ProviderId, &afd_provider))
		udp_afd_event (stats, event);
}

static ULONG WINAPI udp_buffer (
	_In_ PEVENT_TRACE_LOGFILEW buffer
)
{
	PSW_UDP_STATS stats = (PSW_UDP_STATS)buffer->Context;

	if (buffer->EventsLost)
		_InterlockedExchange (&stats->error_code, ERROR_DATA_NOT_ACCEPTED);

	return !_InterlockedCompareExchange (&stats->stopping, 0, 0);
}

static VOID NTAPI udp_thread (
	_In_ PVOID context
)
{
	PSW_UDP_STATS stats = (PSW_UDP_STATS)context;
	ULONG result;

	result = ProcessTrace (&stats->hconsumer, 1, NULL, NULL);

	if (!_InterlockedCompareExchange (&stats->stopping, 0, 0))
		_InterlockedExchange (&stats->error_code, result ? result : ERROR_OPERATION_ABORTED);
}

_Ret_maybenull_
PSW_UDP_STATS udp_stats_create ()
{
	PSW_UDP_STATS stats;

	stats = (PSW_UDP_STATS)_r_mem_allocatesafe (sizeof (SW_UDP_STATS));

	if (stats)
	{
		stats->hconsumer = INVALID_PROCESSTRACE_HANDLE;
		stats->error_code = ERROR_NOT_READY;
	}

	return stats;
}

NTSTATUS udp_stats_start (
	_Inout_opt_ PSW_UDP_STATS stats
)
{
	EVENT_TRACE_LOGFILEW logfile = {0};
	UDP_TRACE_PROPERTIES properties;
	OBJECT_ATTRIBUTES oa = {0};
	R_ENVIRONMENT environment;
	UNICODE_STRING us;
	WCHAR buffer[0x80];
	TRACEHANDLE hsession = 0;
	NTSTATUS status;

	if (!stats)
		return STATUS_MEMORY_NOT_ALLOCATED;

	if (stats->hguard || _InterlockedCompareExchange (&stats->stopping, 0, 0))
		return STATUS_ALREADY_INITIALIZED;

	// a global semaphore distinguishes an orphan left by a crash from an active simplewall instance. never stop another tracing application's session.
	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"\\Sessions\\%u\\BaseNamedObjects\\%s", NtCurrentPeb ()->SessionId, UDP_SESSION_SEMAPHORE);

	_r_obj_initializeunicodestring (&us, buffer);

	InitializeObjectAttributes (&oa, &us, OBJ_CASE_INSENSITIVE | OBJ_OPENIF, NULL, NULL);

	status = NtCreateSemaphore (&stats->hguard, SEMAPHORE_ALL_ACCESS, &oa, 1, 1);

	if (NT_SUCCESS (status) || status == STATUS_OBJECT_NAME_COLLISION)
	{
		status = _r_sys_waitforsingleobject (stats->hguard, 0);

		stats->is_owns_guard = (status == STATUS_WAIT_0);

		status = stats->is_owns_guard ? STATUS_SUCCESS : STATUS_ALREADY_INITIALIZED;
	}
	else
	{
		stats->hguard = NULL;

		goto fail;
	}

	udp_initialize_properties (&properties);

	status = StartTraceW (&hsession, UDP_SESSION_NAME, &properties.properties);

	if (status == ERROR_ALREADY_EXISTS)
	{
		udp_initialize_properties (&properties);

		status = ControlTraceW (0, UDP_SESSION_NAME, &properties.properties, EVENT_TRACE_CONTROL_QUERY);

		if (status == ERROR_SUCCESS && IsEqualGUID (&properties.properties.Wnode.Guid, &udp_session))
		{
			status = ControlTraceW (0, UDP_SESSION_NAME, &properties.properties, EVENT_TRACE_CONTROL_STOP);

			if (status == ERROR_SUCCESS)
			{
				udp_initialize_properties (&properties);

				status = StartTraceW (&hsession, UDP_SESSION_NAME, &properties.properties);
			}
		}
		else if (status == ERROR_SUCCESS)
		{
			status = STATUS_ALREADY_INITIALIZED;
		}
	}

	if (status != STATUS_SUCCESS)
		goto fail;

	stats->hsession = hsession;

	// DATAGRAM plus RIO lifecycle metadata; no verbose packet-buffer events
	status = EnableTraceEx2 (hsession, &afd_provider, EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_INFORMATION, 0x41, 0, 0, NULL);

	if (status != STATUS_SUCCESS)
	{
		status = _r_sys_doserrortontstatus (status);

		goto fail;
	}

	logfile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
	logfile.LoggerName = UDP_SESSION_NAME;
	logfile.EventRecordCallback = &udp_event;
	logfile.BufferCallback = &udp_buffer;
	logfile.Context = stats;

	stats->hconsumer = OpenTraceW (&logfile);

	if (stats->hconsumer == INVALID_PROCESSTRACE_HANDLE)
	{
		status = _r_sys_doserrortontstatus (NtLastError ());

		goto fail;
	}

	_InterlockedExchange (&stats->error_code, STATUS_SUCCESS);

	_r_sys_setenvironment (&environment, THREAD_PRIORITY_ABOVE_NORMAL, IoPriorityHigh, MEMORY_PRIORITY_NORMAL);

	status = _r_sys_createthread (&stats->hthread, NtCurrentProcess (), &udp_thread, stats, &environment, L"UdpMonitoring");

	if (!NT_SUCCESS (status))
		goto fail;

	NtResumeThread (stats->hthread, NULL);

	return STATUS_SUCCESS;

fail:

	udp_stats_stop (stats);

	_InterlockedExchange (&stats->error_code, status);

	return status;
}

VOID udp_stats_stop (
	_In_opt_ PSW_UDP_STATS stats
)
{
	UDP_TRACE_PROPERTIES properties;

	if (!stats)
		return;

	AcquireSRWLockExclusive (&stats->control_lock);

	_InterlockedExchange (&stats->error_code, ERROR_OPERATION_ABORTED);
	_InterlockedExchange (&stats->stopping, TRUE);

	if (stats->hsession)
	{
		udp_initialize_properties (&properties);

		ControlTraceW (stats->hsession, NULL, &properties.properties, EVENT_TRACE_CONTROL_STOP);

		stats->hsession = 0;
	}

	if (stats->hconsumer != INVALID_PROCESSTRACE_HANDLE)
		CloseTrace (stats->hconsumer);

	if (stats->hthread)
	{
		_r_sys_waitforsingleobject (stats->hthread, INFINITE);

		NtClose (stats->hthread);

		stats->hthread = NULL;
	}

	stats->hconsumer = INVALID_PROCESSTRACE_HANDLE;

	if (stats->hguard)
	{
		if (stats->is_owns_guard)
			NtReleaseSemaphore (stats->hguard, 1, NULL);

		NtClose (stats->hguard);
		stats->hguard = NULL;

		stats->is_owns_guard = FALSE;
	}

	ReleaseSRWLockExclusive (&stats->control_lock);
}

VOID udp_stats_destroy (
	_In_opt_ PSW_UDP_STATS stats
)
{
	UDP_AFD_SOCKET *next, *socket;
	UDP_ENTRY *entry, *next_entry;

	if (!stats)
		return;

	udp_stats_stop (stats);

	for (ULONG i = 0; i < UDP_BUCKETS; i++)
	{
		socket = stats->sockets[i];

		while (socket)
		{
			next = socket->next;

			_r_mem_free (socket);

			socket = next;
		}

		entry = stats->buckets[i];

		while (entry)
		{
			next_entry = entry->next;

			_r_mem_free (entry);

			entry = next_entry;
		}
	}

	_r_mem_free (stats);
}

ULONG udp_stats_poll (
	_In_opt_ PSW_UDP_STATS stats
)
{
	UDP_TRACE_PROPERTIES properties;
	ULONG status;

	if (!stats)
		return ERROR_NOT_READY;

	AcquireSRWLockExclusive (&stats->control_lock);

	status = _InterlockedCompareExchange (&stats->error_code, 0, 0);

	if (status == ERROR_SUCCESS && stats->hsession)
	{
		udp_initialize_properties (&properties);

		status = ControlTraceW (stats->hsession, NULL, &properties.properties, EVENT_TRACE_CONTROL_QUERY);

		if (status == ERROR_SUCCESS && (properties.properties.EventsLost || properties.properties.RealTimeBuffersLost || properties.properties.LogBuffersLost))
			status = ERROR_DATA_NOT_ACCEPTED;

		if (status != ERROR_SUCCESS)
			_InterlockedExchange (&stats->error_code, status);
	}

	ReleaseSRWLockExclusive (&stats->control_lock);

	return status;
}

VOID udp_stats_begin_refresh (
	_Inout_opt_ PSW_UDP_STATS stats
)
{
	if (!stats)
		return;

	AcquireSRWLockExclusive (&stats->lock);
	stats->epoch += 1;
	ReleaseSRWLockExclusive (&stats->lock);
}

VOID udp_stats_read (
	_Inout_ PSW_UDP_STATS stats,
	_In_ PUDP_ENDPOINT endpoint,
	_Out_ PUDP_SNAPSHOT snapshot
)
{
	LARGE_INTEGER li;
	UDP_ENTRY *entry;
	ULONG bucket;

	RtlZeroMemory (snapshot, sizeof (UDP_SNAPSHOT));

	snapshot->error_code = stats ? _InterlockedCompareExchange (&stats->error_code, 0, 0) : ERROR_NOT_READY;

	if (snapshot->error_code != ERROR_SUCCESS)
		return;

	bucket = udp_bucket (endpoint->pid, endpoint->port);

	AcquireSRWLockExclusive (&stats->lock);

	for (entry = stats->buckets[bucket]; entry; entry = entry->next)
	{
		if (entry->endpoint.pid == endpoint->pid && entry->endpoint.port == endpoint->port && entry->endpoint.af == endpoint->af &&
			entry->endpoint.scope_id == endpoint->scope_id && RtlEqualMemory (entry->endpoint.address, endpoint->address, endpoint->af == AF_INET ? 4 : 16))
		{
			break;
		}
	}

	if (!entry)
	{
		if (stats->count < UDP_MAX_ENDPOINTS)
			entry = (PUDP_ENTRY)_r_mem_allocatesafe (sizeof (UDP_ENTRY));

		if (!entry)
		{
			snapshot->error_code = ERROR_NOT_ENOUGH_MEMORY;

			ReleaseSRWLockExclusive (&stats->lock);

			return;
		}

		entry->next = stats->buckets[bucket];
		stats->buckets[bucket] = entry;
		stats->count += 1;
	}

	if (!entry->since || entry->endpoint.created != endpoint->created)
	{
		entry->endpoint = *endpoint;
		entry->since = _r_sys_gettimestamp (&li)->QuadPart;

		if (endpoint->created > entry->since)
			entry->since = endpoint->created;

		entry->received = entry->sent = 0;
		entry->error_code = ERROR_SUCCESS;
		entry->is_observed = FALSE;
	}

	snapshot->error_code = entry->error_code ? entry->error_code : (entry->is_observed ? ERROR_SUCCESS : ERROR_NOT_READY);
	snapshot->received = entry->received;
	snapshot->sent = entry->sent;
	entry->epoch = stats->epoch;

	ReleaseSRWLockExclusive (&stats->lock);
}

VOID udp_stats_end_refresh (
	_In_opt_ SW_UDP_STATS *stats,
	_In_ BOOLEAN ipv4_complete,
	_In_ BOOLEAN ipv6_complete
)
{
	UDP_ENTRY *entry, *next, *previous;

	if (!stats)
		return;

	AcquireSRWLockExclusive (&stats->lock);

	for (ULONG i = 0; i < UDP_BUCKETS; i++)
	{
		entry = stats->buckets[i];
		previous = NULL;

		while (entry)
		{
			next = entry->next;

			if (entry->epoch != stats->epoch && (entry->endpoint.af == AF_INET ? ipv4_complete : ipv6_complete))
			{
				if (previous)
				{
					previous->next = next;
				}
				else
				{
					stats->buckets[i] = next;
				}

				_r_mem_free (entry);

				stats->count -= 1;
			}
			else
			{
				previous = entry;
			}

			entry = next;
		}
	}

	ReleaseSRWLockExclusive (&stats->lock);
}
