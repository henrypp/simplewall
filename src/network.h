// simplewall
// Copyright (c) 2019-2021 Henry++

#pragma once

#define NETWORK_TIMEOUT 3500

typedef struct _ITEM_NETWORK_CONTEXT
{
	HWND hwnd;

	volatile HANDLE hconnections;

	R_QUEUED_LOCK lock_network;
	R_QUEUED_LOCK lock_checker;

	PR_HASHTABLE network_ptr;
	PR_HASHTABLE checker_ptr;

} ITEM_NETWORK_CONTEXT, *PITEM_NETWORK_CONTEXT;

VOID _app_network_initialize (_In_ HWND hwnd);
VOID _app_network_uninitialize ();

VOID _app_network_generatetable (_Inout_ PITEM_NETWORK_CONTEXT context);

_Ret_maybenull_
PITEM_NETWORK _app_network_getitem (_In_ ULONG_PTR network_hash);

ULONG_PTR _app_network_getappitem (_In_ ULONG_PTR network_hash);

ULONG_PTR _app_network_gethash (_In_ ADDRESS_FAMILY af, _In_ ULONG pid, _In_opt_ LPCVOID remote_addr, _In_opt_ ULONG remote_port, _In_opt_ LPCVOID local_addr, _In_opt_ ULONG local_port, _In_ UINT8 proto, _In_ ULONG state);
BOOLEAN _app_network_getpath (_In_ ULONG pid, _In_opt_ PULONG64 modules, _Inout_ PITEM_NETWORK ptr_network);

BOOLEAN _app_network_isapphaveconnection (_In_ ULONG_PTR app_hash);
BOOLEAN _app_network_isitemfound (_In_ ULONG_PTR network_hash);
BOOLEAN _app_network_isvalidconnection (_In_ ADDRESS_FAMILY af, _In_ LPCVOID address);

VOID _app_network_printlistviewtable (_Inout_ PITEM_NETWORK_CONTEXT context);

VOID _app_network_removeitem (_In_ ULONG_PTR network_hash);

_Ret_maybenull_
HANDLE _app_network_subscribe (_In_ HANDLE engine_handle);
VOID _app_network_unsubscribe (_In_ HANDLE engine_handle);

VOID CALLBACK _app_network_subscribe_callback (_Inout_opt_ PVOID context, _In_ FWPM_CONNECTION_EVENT_TYPE event_type, _In_ const FWPM_CONNECTION0* connection);

NTSTATUS NTAPI _app_network_threadproc (_In_ PVOID arglist);
