// simplewall
// Copyright (c) 2019-2021 Henry++

#pragma once

VOID _app_network_initialize (_In_ HWND hwnd);

_Ret_maybenull_
PITEM_NETWORK _app_network_getitem (_In_ ULONG_PTR network_hash);

ULONG_PTR _app_network_getappitem (_In_ ULONG_PTR network_hash);

ULONG_PTR _app_network_gethash (_In_ ADDRESS_FAMILY af, _In_ ULONG pid, _In_opt_ LPCVOID remote_addr, _In_opt_ ULONG remote_port, _In_opt_ LPCVOID local_addr, _In_opt_ ULONG local_port, _In_ UINT8 proto, _In_ ULONG state);
BOOLEAN _app_network_getpath (_In_ ULONG pid, _In_opt_ PULONG64 modules, _Inout_ PITEM_NETWORK ptr_network);

BOOLEAN _app_network_isapphaveconnection (_In_ ULONG_PTR app_hash);
BOOLEAN _app_network_isitemfound (_In_ ULONG_PTR network_hash);
BOOLEAN _app_network_isvalidconnection (_In_ ADDRESS_FAMILY af, _In_ LPCVOID address);

VOID _app_network_printlistviewtable (_In_ HWND hwnd, _In_ PR_HASHTABLE network_ptr, _In_ PR_HASHTABLE checker_map);

VOID _app_network_removeitem (_In_ ULONG_PTR network_hash);

_Ret_maybenull_
HANDLE _app_network_subscribe ();

VOID CALLBACK _app_network_subscribe_callback (_Inout_opt_ PVOID context, _In_ FWPM_CONNECTION_EVENT_TYPE event_type, _In_ const FWPM_CONNECTION0* connection);

NTSTATUS NTAPI _app_network_threadproc (_In_ PVOID arglist);
