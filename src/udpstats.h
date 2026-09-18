// simplewall
// Copyright (c) 2026 Henry++
// Copyright (c) 2026 Tarkiin
// UDP events contain metadata only; no packet payload is captured or stored
#pragma once

#include "global.h"

static const GUID afd_provider = {0xE53C6823, 0x7BB8, 0x44BB, {0x90, 0xDC, 0x3F, 0x86, 0x09, 0x0D, 0x48, 0xA6}};
static const GUID udp_session = {0x4BFA58D6, 0x5D5D, 0x46F2, {0x93, 0x93, 0xC9, 0xE0, 0x5E, 0x27, 0xB4, 0x71}};

_Ret_maybenull_
PSW_UDP_STATS udp_stats_create ();

NTSTATUS udp_stats_start (
	_Inout_opt_ PSW_UDP_STATS stats
);

VOID udp_stats_stop (
	_In_opt_ PSW_UDP_STATS stats
);

VOID udp_stats_destroy (
	_In_opt_ PSW_UDP_STATS stats
);

ULONG udp_stats_poll (
	_In_opt_ PSW_UDP_STATS stats
);

VOID udp_stats_begin_refresh (
	_Inout_opt_ PSW_UDP_STATS stats
);

VOID udp_stats_read (
	_Inout_ PSW_UDP_STATS stats,
	_In_ PUDP_ENDPOINT endpoint,
	_Out_ PUDP_SNAPSHOT snapshot
);

VOID udp_stats_end_refresh (
	_In_opt_ SW_UDP_STATS *stats,
	_In_ BOOLEAN ipv4_complete,
	_In_ BOOLEAN ipv6_complete
);
