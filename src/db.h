// simplewall
// Copyright (c) 2019-2024 Henry++

#pragma once

// profile types
typedef enum _ENUM_TYPE_XML
{
	XML_TYPE_PROFILE = 3,
	XML_TYPE_PROFILE_INTERNAL = 4,
} ENUM_TYPE_XML;

// profile versions
typedef enum _ENUM_VERSION_XML
{
	// v3.0.2: first major update, rule attribute "apps" now separated by "|"
	XML_VERSION_3 = 3,

	// v3.4: added "rules_custom" into internal profile and "os_version" for rule attributes
	XML_VERSION_4 = 4,

	// v3.7: added hashes support, use encrypted file format and added comments for apps/rules
	XML_VERSION_5 = 5,
} ENUM_VERSION_XML;

typedef struct _DB_INFORMATION
{
	R_XML_LIBRARY xml_library;

	PR_BYTE hash;
	PR_BYTE bytes;

	LONG64 timestamp;

	ENUM_VERSION_XML version;
	ENUM_TYPE_XML type;
} DB_INFORMATION, *PDB_INFORMATION;

// Offset	Length	Description
// ------------------------------------------
// 0		3		FOURCC string 'swc'
// 4		1		Body information flag
// 36		32		SHA256 checksum of the decrypted bytes
// 68		whole	Body contained specified data

static const BYTE profile2_fourcc[] = {
	0x73, 0x77, 0x63, // 's', 'w', 'c'
};

#define XML_VERSION_MINIMAL XML_VERSION_3
#define XML_VERSION_CURRENT XML_VERSION_4
#define XML_VERSION_MAXIMUM XML_VERSION_5

#define XML_PROFILE_FILE L"profile.xml"
#define XML_PROFILE_INTERNAL L"profile_internal.xml"

#define PROFILE2_ID_PLAIN ((BYTE)(0x30))
#define PROFILE2_ID_COMPRESSED ((BYTE)(0x31))
#define PROFILE2_ID_ENCRYPTED ((BYTE)(0x32))

#define PROFILE2_KEY "Gc(j1EptodnKP?{ZT!SlMVip[fJIs&Ci3fOqjATfp@h,q0(]QVaGs5Iht3)/b:Ll"

#define PROFILE2_FOURCC_LENGTH sizeof (profile2_fourcc) + sizeof (BYTE)
#define PROFILE2_SHA256_LENGTH 32UL
#define PROFILE2_HEADER_LENGTH (PROFILE2_FOURCC_LENGTH + PROFILE2_SHA256_LENGTH)

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_db_initialize (
	_Out_ PDB_INFORMATION db_info,
	_In_ BOOLEAN is_reader
);

VOID _app_db_destroy (
	_Inout_ PDB_INFORMATION db_info
);

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_db_encrypt (
	_In_ PR_BYTEREF bytes,
	_Out_ PR_BYTE_PTR out_buffer
);

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_db_decrypt (
	_In_ PR_BYTEREF buffer,
	_Out_ PR_BYTE_PTR out_buffer
);

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_db_gethash (
	_In_ PR_BYTEREF bytes,
	_Out_ PR_BYTE_PTR out_buffer
);

BYTE _app_getprofiletype ();

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_db_ishashvalid (
	_In_ PR_BYTEREF buffer,
	_In_ PR_BYTEREF hash_bytes
);

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_db_openfrombuffer (
	_Inout_ PDB_INFORMATION db_info,
	_In_ PR_STORAGE buffer,
	_In_ ENUM_VERSION_XML min_version,
	_In_ ENUM_TYPE_XML type
);

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_db_openfromfile (
	_Inout_ PDB_INFORMATION db_info,
	_In_ PR_STRING path,
	_In_ ENUM_VERSION_XML min_version,
	_In_ ENUM_TYPE_XML type
);

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_db_decodebody (
	_Inout_ PDB_INFORMATION db_info
);

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_db_encodebody (
	_Inout_ PDB_INFORMATION db_info,
	_In_ BYTE profile_type,
	_Out_ PR_BYTE_PTR out_buffer
);

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_db_generatebody (
	_In_ BYTE profile_type,
	_In_ PR_BYTE hash_value,
	_In_ PR_BYTE buffer,
	_Out_ PR_BYTE_PTR out_buffer
);

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_db_decodebuffer (
	_Inout_ PDB_INFORMATION db_info,
	_In_ ENUM_TYPE_XML type,
	_In_ ENUM_VERSION_XML min_version
);

BOOLEAN _app_db_parse (
	_Inout_ PDB_INFORMATION db_info,
	_In_ ENUM_TYPE_XML type
);

VOID _app_db_parse_app (
	_Inout_ PDB_INFORMATION db_info
);

VOID _app_db_parse_rule (
	_Inout_ PDB_INFORMATION db_info,
	_In_ ENUM_TYPE_DATA type
);

VOID _app_db_parse_ruleconfig (
	_Inout_ PDB_INFORMATION db_info
);

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_db_savetofile (
	_Inout_ PDB_INFORMATION db_info,
	_In_ PR_STRING path,
	_In_ ENUM_VERSION_XML version,
	_In_ ENUM_TYPE_XML type,
	_In_ LONG64 timestamp
);

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_db_save_streamtofile (
	_Inout_ PDB_INFORMATION db_info,
	_In_ PR_STRING path
);

VOID _app_db_save_app (
	_Inout_ PDB_INFORMATION db_info
);

VOID _app_db_save_rule (
	_Inout_ PDB_INFORMATION db_info
);

VOID _app_db_save_ruleconfig (
	_Inout_ PDB_INFORMATION db_info
);

_Ret_maybenull_
LPCWSTR _app_db_getconnectionstatename (
	_In_ ULONG state
);

_Ret_maybenull_
PR_STRING _app_db_getdirectionname (
	_In_ FWP_DIRECTION direction,
	_In_ BOOLEAN is_loopback,
	_In_ BOOLEAN is_localized
);

_Ret_maybenull_
PR_STRING _app_db_getprotoname (
	_In_ ULONG proto,
	_In_ ADDRESS_FAMILY af,
	_In_ BOOLEAN is_notnull
);

_Ret_maybenull_
LPCWSTR _app_db_getservicename (
	_In_ UINT16 port,
	_In_ UINT8 proto
);
