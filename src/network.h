// simplewall
// Copyright (c) 2019-2021 Henry++

#pragma once

VOID _app_network_initialize (_In_ HWND hwnd);

ULONG_PTR _app_network_gethash (_In_ ADDRESS_FAMILY af, _In_ ULONG pid, _In_opt_ LPCVOID remote_addr, _In_opt_ ULONG remote_port, _In_opt_ LPCVOID local_addr, _In_opt_ ULONG local_port, _In_ UINT8 proto, _In_ ULONG state);
BOOLEAN _app_network_getpath (_In_ ULONG pid, _In_opt_ PULONG64 modules, _Inout_ PITEM_NETWORK ptr_network);

BOOLEAN _app_network_isvalidconnection (_In_ ADDRESS_FAMILY af, _In_ LPCVOID address);

NTSTATUS NTAPI _app_network_threadproc (_In_ PVOID arglist);
