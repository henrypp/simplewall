// simplewall
// Copyright (c) 2019-2025 Henry++

#include "global.h"

_Success_ (SUCCEEDED (return))
HRESULT _app_db_initialize (
	_Out_ PDB_INFORMATION db_info,
	_In_ BOOLEAN is_reader
)
{
	RtlZeroMemory (db_info, sizeof (DB_INFORMATION));

	return _r_xml_initializelibrary (&db_info->xml_library, is_reader);
}

VOID _app_db_destroy (
	_Inout_ PDB_INFORMATION db_info
)
{
	if (db_info->bytes)
		_r_obj_clearreference ((PVOID_PTR)&db_info->bytes);

	_r_xml_destroylibrary (&db_info->xml_library);
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_db_encrypt (
	_In_ PR_BYTEREF bytes,
	_Out_ PR_BYTE_PTR out_buffer
)
{
	R_CRYPT_CONTEXT crypt_context;
	R_BYTEREF key;
	NTSTATUS status;

	*out_buffer = NULL;

	status = _r_crypt_createcryptcontext (&crypt_context, BCRYPT_AES_ALGORITHM);

	if (!NT_SUCCESS (status))
		return status;

	_r_obj_initializebyteref (&key, PROFILE2_KEY);

	status = _r_crypt_generatekey (&crypt_context, &key);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	status = _r_crypt_encryptbuffer (&crypt_context, bytes->buffer, (ULONG)bytes->length, out_buffer);

CleanupExit:

	_r_crypt_destroycryptcontext (&crypt_context);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_db_decrypt (
	_In_ PR_BYTEREF bytes,
	_Out_ PR_BYTE_PTR out_buffer
)
{
	R_CRYPT_CONTEXT crypt_context;
	R_BYTEREF key;
	NTSTATUS status;

	*out_buffer = NULL;

	status = _r_crypt_createcryptcontext (&crypt_context, BCRYPT_AES_ALGORITHM);

	if (!NT_SUCCESS (status))
		return status;

	_r_obj_initializebyteref (&key, PROFILE2_KEY);

	status = _r_crypt_generatekey (&crypt_context, &key);

	if (!NT_SUCCESS (status))
		goto CleanupExit;

	status = _r_crypt_decryptbuffer (&crypt_context, bytes->buffer, (ULONG)bytes->length, out_buffer);

CleanupExit:

	_r_crypt_destroycryptcontext (&crypt_context);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_db_gethash (
	_In_ PR_BYTEREF bytes,
	_Out_ PR_BYTE_PTR out_buffer
)
{
	R_CRYPT_CONTEXT hash_context;
	NTSTATUS status;

	*out_buffer = NULL;

	status = _r_crypt_createhashcontext (&hash_context, BCRYPT_SHA256_ALGORITHM);

	if (!NT_SUCCESS (status))
		return status;

	status = _r_crypt_hashbuffer (&hash_context, bytes->buffer, (ULONG)bytes->length);

	if (NT_SUCCESS (status))
		status = _r_crypt_finalhashcontext (&hash_context, NULL, out_buffer);

	_r_crypt_destroycryptcontext (&hash_context);

	return status;
}

BYTE _app_getprofiletype ()
{
	LONG profile_type;

	profile_type = _r_config_getlong (L"ProfileType", 0, NULL);

	switch (profile_type)
	{
		case 1:
		{
			return PROFILE2_ID_COMPRESSED;
		}

		case 2:
		{
			return PROFILE2_ID_ENCRYPTED;
		}
	}

	return PROFILE2_ID_PLAIN;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_db_ishashvalid (
	_In_ PR_BYTEREF buffer,
	_In_ PR_BYTEREF hash_bytes
)
{
	PR_BYTE new_hash_bytes = NULL;
	NTSTATUS status;

	status = _app_db_gethash (buffer, &new_hash_bytes);

	if (!NT_SUCCESS (status))
		return status;

	if (RtlEqualMemory (hash_bytes->buffer, new_hash_bytes->buffer, new_hash_bytes->length))
	{
		status = STATUS_SUCCESS;
	}
	else
	{
		status = STATUS_INVALID_IMAGE_HASH;
	}

	_r_obj_dereference (new_hash_bytes);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_db_openfrombuffer (
	_Inout_ PDB_INFORMATION db_info,
	_In_ PR_STORAGE buffer,
	_In_ ENUM_VERSION_XML min_version,
	_In_ ENUM_TYPE_XML type
)
{
	NTSTATUS status;

	_r_obj_movereference ((PVOID_PTR)&db_info->bytes, _r_obj_createbyte4 (buffer));

	status = _app_db_decodebuffer (db_info, type, min_version);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_db_openfromfile (
	_Inout_ PDB_INFORMATION db_info,
	_In_ PR_STRING path,
	_In_ ENUM_VERSION_XML min_version,
	_In_ ENUM_TYPE_XML type
)
{
	HANDLE hfile;
	NTSTATUS status;

	if (db_info->bytes)
		_r_obj_clearreference ((PVOID_PTR)&db_info->bytes);

	status = _r_fs_openfile (&path->sr, GENERIC_READ, FILE_SHARE_READ, 0, FALSE, &hfile);

	if (!NT_SUCCESS (status))
		return status;

	status = _r_fs_readbytes (hfile, &db_info->bytes);

	if (!NT_SUCCESS (status))
	{
		NtClose (hfile);

		return status;
	}

	status = _app_db_decodebuffer (db_info, type, min_version);

	NtClose (hfile);

	return status;
}

VOID _app_db_parse_app (
	_Inout_ PDB_INFORMATION db_info
)
{
	PITEM_APP ptr_app;
	PR_STRING string;
	PR_STRING path;
	PR_STRING dos_path;
	LONG64 timestamp;
	LONG64 timer;
	ULONG app_hash;
	BOOLEAN is_undeletable;
	BOOLEAN is_enabled;
	BOOLEAN is_silent;

	path = _r_xml_getattribute_string (&db_info->xml_library, L"path");

	if (_r_obj_isstringempty (path))
		return;

	// workaround for native paths
	// https://github.com/henrypp/simplewall/issues/817
	if (_r_str_isstartswith2 (&path->sr, L"\\device\\", TRUE))
	{
		dos_path = _r_path_dospathfromnt (&path->sr);

		if (dos_path)
			_r_obj_movereference ((PVOID_PTR)&path, dos_path);
	}

	if (!_r_obj_isstringempty2 (path))
	{
		app_hash = _app_addapplication (NULL, DATA_UNKNOWN, path, NULL, NULL);

		if (app_hash)
		{
			ptr_app = _app_getappitem (app_hash);

			if (ptr_app)
			{
				is_enabled = _r_xml_getattribute_boolean (&db_info->xml_library, L"is_enabled");
				is_silent = _r_xml_getattribute_boolean (&db_info->xml_library, L"is_silent");
				is_undeletable = _r_xml_getattribute_boolean (&db_info->xml_library, L"is_undeletable");

				timestamp = _r_xml_getattribute_long64 (&db_info->xml_library, L"timestamp");
				timer = _r_xml_getattribute_long64 (&db_info->xml_library, L"timer");

				string = _r_xml_getattribute_string (&db_info->xml_library, L"hash");

				if (string)
				{
					if (!_r_obj_isstringempty2 (string))
					{
						_app_setappinfo (ptr_app, INFO_HASH, string);
					}
					else
					{
						_r_obj_dereference (string);
					}
				}

				string = _r_xml_getattribute_string (&db_info->xml_library, L"comment");

				if (string)
				{
					if (!_r_obj_isstringempty2 (string))
					{
						_app_setappinfo (ptr_app, INFO_COMMENT, string);
					}
					else
					{
						_r_obj_dereference (string);
					}
				}

				if (is_silent)
					_app_setappinfo (ptr_app, INFO_IS_SILENT, LongToPtr (is_silent));

				if (is_enabled)
					_app_setappinfo (ptr_app, INFO_IS_ENABLED, LongToPtr (is_enabled));

				if (is_undeletable)
					_app_setappinfo (ptr_app, INFO_IS_UNDELETABLE, LongToPtr (is_undeletable));

				if (timestamp)
					_app_setappinfo (ptr_app, INFO_TIMESTAMP, &timestamp);

				if (timer)
					_app_setappinfo (ptr_app, INFO_TIMER, &timer);

				_r_obj_dereference (ptr_app);
			}
		}
	}

	_r_obj_dereference (path);
}

VOID _app_db_parse_rule (
	_Inout_ PDB_INFORMATION db_info,
	_In_ ENUM_TYPE_DATA type
)
{
	PITEM_RULE_CONFIG ptr_config = NULL;
	R_STRINGBUILDER sb;
	R_STRINGREF first_part;
	R_STRINGREF sr;
	PR_STRING rule_name;
	PR_STRING rule_remote;
	PR_STRING rule_local;
	PR_STRING path_string;
	PR_STRING comment;
	PR_STRING string;
	LONG blocklist_spy_state;
	LONG blocklist_update_state;
	LONG blocklist_extra_state;
	FWP_DIRECTION direction;
	FWP_ACTION_TYPE action;
	UINT8 protocol;
	ADDRESS_FAMILY af;
	PITEM_RULE ptr_rule;
	ULONG rule_hash;
	ULONG app_hash;
	BOOLEAN is_internal;
	NTSTATUS status;

	// check support version
	status = _r_xml_getattribute (&db_info->xml_library, L"os_version", &sr);

	if (SUCCEEDED (status))
	{
		if (!_app_isrulesupportedbyos (&sr))
			return;
	}

	rule_name = _r_xml_getattribute_string (&db_info->xml_library, L"name");

	if (!rule_name)
		return;

	rule_remote = _r_xml_getattribute_string (&db_info->xml_library, L"rule");
	rule_local = _r_xml_getattribute_string (&db_info->xml_library, L"rule_local");
	comment = _r_xml_getattribute_string (&db_info->xml_library, L"comment");
	direction = (FWP_DIRECTION)_r_xml_getattribute_long (&db_info->xml_library, L"dir");
	action = _r_xml_getattribute_boolean (&db_info->xml_library, L"is_block") ? FWP_ACTION_BLOCK : FWP_ACTION_PERMIT;
	protocol = (UINT8)_r_xml_getattribute_long (&db_info->xml_library, L"protocol");
	af = (ADDRESS_FAMILY)_r_xml_getattribute_long (&db_info->xml_library, L"version");

	ptr_rule = _app_addrule (rule_name, rule_remote, rule_local, direction, action, protocol, af);

	_r_obj_dereference (rule_name);

	if (rule_remote)
		_r_obj_dereference (rule_remote);

	if (rule_local)
		_r_obj_dereference (rule_local);

	rule_hash = _r_str_gethash (&ptr_rule->name->sr, TRUE);

	if (!_r_obj_isstringempty (comment))
	{
		_r_obj_movereference ((PVOID_PTR)&ptr_rule->comment, comment);
	}
	else
	{
		if (comment)
			_r_obj_dereference (comment);
	}

	ptr_rule->type = (type == DATA_RULE_SYSTEM_USER) ? DATA_RULE_USER : type;
	ptr_rule->is_forservices = _r_xml_getattribute_boolean (&db_info->xml_library, L"is_services");
	ptr_rule->is_readonly = (type != DATA_RULE_USER);
	ptr_rule->is_enabled = _r_xml_getattribute_boolean (&db_info->xml_library, L"is_enabled");

	// calculate rule weight
	if (type == DATA_RULE_BLOCKLIST)
	{
		ptr_rule->weight = FW_WEIGHT_RULE_BLOCKLIST;
	}
	else if (type == DATA_RULE_SYSTEM || type == DATA_RULE_SYSTEM_USER)
	{
		ptr_rule->weight = FW_WEIGHT_RULE_SYSTEM;
	}
	else if (type == DATA_RULE_USER)
	{
		ptr_rule->weight = (ptr_rule->action == FWP_ACTION_BLOCK) ? FW_WEIGHT_RULE_USER_BLOCK : FW_WEIGHT_RULE_USER;
	}

	if (type == DATA_RULE_BLOCKLIST)
	{
		blocklist_spy_state = _r_calc_clamp (_r_config_getlong (L"BlocklistSpyState", 2, NULL), 0, 2);
		blocklist_update_state = _r_calc_clamp (_r_config_getlong (L"BlocklistUpdateState", 0, NULL), 0, 2);
		blocklist_extra_state = _r_calc_clamp (_r_config_getlong (L"BlocklistExtraState", 0, NULL), 0, 2);

		_app_ruleblocklistsetstate (ptr_rule, blocklist_spy_state, blocklist_update_state, blocklist_extra_state);
	}
	else
	{
		ptr_rule->is_enabled_default = ptr_rule->is_enabled; // set default value for rule
	}

	// load rules config
	is_internal = (type == DATA_RULE_BLOCKLIST || type == DATA_RULE_SYSTEM || type == DATA_RULE_SYSTEM_USER);

	if (is_internal)
	{
		// internal rules
		ptr_config = _app_getruleconfigitem (rule_hash);

		if (ptr_config)
			ptr_rule->is_enabled = ptr_config->is_enabled;
	}

	// load apps
	_r_obj_initializestringbuilder (&sb, 256);

	string = _r_xml_getattribute_string (&db_info->xml_library, L"apps");

	if (!_r_obj_isstringempty (string))
	{
		_r_obj_appendstringbuilder2 (&sb, &string->sr);

		_r_obj_dereference (string);
	}

	if (is_internal && ptr_config && !_r_obj_isstringempty (ptr_config->apps))
	{
		if (!_r_obj_isstringempty2 (sb.string))
			_r_obj_appendstringbuilder (&sb, DIVIDER_APP);

		_r_obj_appendstringbuilder2 (&sb, &ptr_config->apps->sr);
	}

	string = _r_obj_finalstringbuilder (&sb);

	if (!_r_obj_isstringempty2 (string))
	{
		if (db_info->version < XML_VERSION_3)
			_r_str_replacechar (&string->sr, DIVIDER_RULE[0], DIVIDER_APP[0]);

		_r_obj_initializestringref2 (&sr, &string->sr);

		while (sr.length != 0)
		{
			_r_str_splitatchar (&sr, DIVIDER_APP[0], &first_part, &sr);

			status = _r_str_environmentexpandstring (NULL, &first_part, &path_string);

			if (status != STATUS_SUCCESS)
				path_string = _r_obj_createstring2 (&first_part);

			app_hash = _r_str_gethash (&path_string->sr, TRUE);

			if (app_hash)
			{
				if (ptr_rule->is_forservices && _app_issystemhash (app_hash))
				{
					_r_obj_dereference (path_string);

					continue;
				}

				if (!_app_isappfound (app_hash))
					app_hash = _app_addapplication (NULL, DATA_UNKNOWN, path_string, NULL, NULL);

				if (app_hash)
				{
					_r_obj_addhashtableitem (ptr_rule->apps, app_hash, NULL);

					if (ptr_rule->type == DATA_RULE_SYSTEM)
						_app_setappinfobyhash (app_hash, INFO_IS_UNDELETABLE, LongToPtr (TRUE));
				}
			}

			_r_obj_dereference (path_string);
		}

		// check if no app is added into rule, then disable it!
		if (ptr_rule->is_enabled)
		{
			if (_r_obj_isempty (ptr_rule->apps))
				ptr_rule->is_enabled = FALSE;
		}
	}

	_r_queuedlock_acquireexclusive (&lock_rules);
	_r_obj_addlistitem (rules_list, ptr_rule, NULL);
	_r_queuedlock_releaseexclusive (&lock_rules);

	_r_obj_deletestringbuilder (&sb);
}

VOID _app_db_parse_ruleconfig (
	_Inout_ PDB_INFORMATION db_info
)
{
	PITEM_RULE_CONFIG ptr_config;
	PR_STRING rule_name;
	ULONG rule_hash;

	rule_name = _r_xml_getattribute_string (&db_info->xml_library, L"name");

	if (!rule_name)
		return;

	rule_hash = _r_str_gethash (&rule_name->sr, TRUE);

	if (!rule_hash)
	{
		_r_obj_dereference (rule_name);

		return;
	}

	ptr_config = _app_getruleconfigitem (rule_hash);

	if (!ptr_config)
	{
		_r_queuedlock_acquireexclusive (&lock_rules_config);
		ptr_config = _app_addruleconfigtable (rules_config, rule_hash, rule_name, _r_xml_getattribute_boolean (&db_info->xml_library, L"is_enabled"));
		_r_queuedlock_releaseexclusive (&lock_rules_config);

		if (ptr_config)
		{
			ptr_config->apps = _r_xml_getattribute_string (&db_info->xml_library, L"apps");

			if (ptr_config->apps && db_info->version < XML_VERSION_3)
				_r_str_replacechar (&ptr_config->apps->sr, DIVIDER_RULE[0], DIVIDER_APP[0]);
		}
	}

	_r_obj_dereference (rule_name);
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_db_decodebody (
	_Inout_ PDB_INFORMATION db_info
)
{
	USHORT format[] = {COMPRESSION_FORMAT_LZNT1, COMPRESSION_FORMAT_XPRESS};
	USHORT architecture;
	PR_BYTE new_bytes;
	BYTE profile_type;
	NTSTATUS status;

	if (db_info->bytes->length < PROFILE2_HEADER_LENGTH)
		return STATUS_SUCCESS;

	if (!RtlEqualMemory (db_info->bytes->buffer, profile2_fourcc, sizeof (profile2_fourcc)) || !RtlEqualMemory (db_info->bytes->buffer, profile2_fourcc, sizeof (profile2_fourcc_old)))
		return STATUS_SUCCESS;

	profile_type = db_info->bytes->buffer[sizeof (profile2_fourcc)];

	// skip fourcc
	_r_obj_skipbytelength (&db_info->bytes->sr, PROFILE2_FOURCC_LENGTH);

	// read the hash
	_r_obj_movereference ((PVOID_PTR)&db_info->hash, _r_obj_createbyte_ex (db_info->bytes->buffer, PROFILE2_SHA256_LENGTH));

	// skip hash
	_r_obj_skipbytelength (&db_info->bytes->sr, PROFILE2_SHA256_LENGTH);

	switch (profile_type)
	{
		case PROFILE2_ID_COMPRESSED:
		{
			// decompress bytes
			for (ULONG_PTR i = 0; i < RTL_NUMBER_OF (format); i++)
			{
				status = _r_sys_decompressbuffer (format[i], &db_info->bytes->sr, &new_bytes);

				if (NT_SUCCESS (status))
				{
					_r_obj_movereference ((PVOID_PTR)&db_info->bytes, new_bytes);

					break;
				}
			}

			break;
		}

		case PROFILE2_ID_ENCRYPTED:
		{
			// decrypt bytes
			status = _app_db_decrypt (&db_info->bytes->sr, &new_bytes);

			if (!NT_SUCCESS (status))
				return status;

			_r_obj_movereference ((PVOID_PTR)&db_info->bytes, new_bytes);

			break;
		}

		default:
		{
			return STATUS_FILE_NOT_SUPPORTED;
		}
	}

	if (!NT_SUCCESS (status))
		return status;

	if (RtlEqualMemory (db_info->bytes->buffer, profile2_fourcc, sizeof (profile2_fourcc)) || RtlEqualMemory (db_info->bytes->buffer, profile2_fourcc_old, sizeof (profile2_fourcc_old)))
		return STATUS_MORE_PROCESSING_REQUIRED;

	// fix arm64 crash that was introduced by Micro$oft (issue #1228)
	if (NT_SUCCESS (_r_sys_getprocessorinformation (&architecture, NULL, NULL)))
	{
		if (architecture == PROCESSOR_ARCHITECTURE_ARM || architecture == PROCESSOR_ARCHITECTURE_ARM64)
			return STATUS_SUCCESS;
	}

	// validate hash
	if (db_info->hash)
		status = _app_db_ishashvalid (&db_info->bytes->sr, &db_info->hash->sr);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_db_encodebody (
	_Inout_ PDB_INFORMATION db_info,
	_In_ BYTE profile_type,
	_Out_ PR_BYTE_PTR out_buffer
)
{
	PR_BYTE body_bytes;
	PR_BYTE hash_value;
	PR_BYTE new_bytes;
	PR_BYTE bytes;
	NTSTATUS status;

	*out_buffer = NULL;

	status = _r_xml_readstream (&db_info->xml_library, &bytes);

	if (FAILED (status))
		return status;

	// generate body hash
	status = _app_db_gethash (&bytes->sr, &hash_value);

	if (!NT_SUCCESS (status))
	{
		_r_obj_dereference (hash_value);
		_r_obj_dereference (bytes);

		return status;
	}

	switch (profile_type)
	{
		case PROFILE2_ID_PLAIN:
		{
			new_bytes = _r_obj_reference (bytes);
			break;
		}

		case PROFILE2_ID_COMPRESSED:
		{
			// compress body
			status = _r_sys_compressbuffer (COMPRESSION_FORMAT_LZNT1 | COMPRESSION_ENGINE_MAXIMUM, &bytes->sr, &new_bytes);

			if (!NT_SUCCESS (status))
			{
				_r_obj_dereference (hash_value);
				_r_obj_dereference (bytes);

				return status;
			}

			break;
		}

		case PROFILE2_ID_ENCRYPTED:
		{
			status = _app_db_encrypt (&bytes->sr, &new_bytes);

			if (!NT_SUCCESS (status))
			{
				_r_obj_dereference (hash_value);
				_r_obj_dereference (bytes);

				return status;
			}

			break;
		}

		default:
		{
			return STATUS_FILE_NOT_SUPPORTED;
		}
	}

	_r_obj_movereference ((PVOID_PTR)&bytes, new_bytes);

	status = _app_db_generatebody (profile_type, hash_value, bytes, &body_bytes);

	*out_buffer = body_bytes;

	_r_obj_dereference (hash_value);
	_r_obj_dereference (bytes);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_db_generatebody (
	_In_ BYTE profile_type,
	_In_ PR_BYTE hash_value,
	_In_ PR_BYTE buffer,
	_Out_ PR_BYTE_PTR out_buffer
)
{
	PR_BYTE bytes;
	PVOID ptr;

	switch (profile_type)
	{
		case PROFILE2_ID_PLAIN:
		{
			bytes = _r_obj_reference (buffer);
			break;
		}

		case PROFILE2_ID_COMPRESSED:
		case PROFILE2_ID_ENCRYPTED:
		{
			bytes = _r_obj_createbyte_ex (NULL, PROFILE2_HEADER_LENGTH + buffer->length);

			RtlCopyMemory (bytes->buffer, profile2_fourcc, sizeof (profile2_fourcc));

			ptr = PTR_ADD_OFFSET (bytes->buffer, sizeof (profile2_fourcc));
			RtlCopyMemory (ptr, &profile_type, sizeof (BYTE));

			ptr = PTR_ADD_OFFSET (bytes->buffer, PROFILE2_FOURCC_LENGTH);
			RtlCopyMemory (ptr, hash_value->buffer, hash_value->length);

			ptr = PTR_ADD_OFFSET (bytes->buffer, PROFILE2_HEADER_LENGTH);
			RtlCopyMemory (ptr, buffer->buffer, buffer->length);

			break;
		}

		default:
		{
			*out_buffer = NULL;

			return STATUS_FILE_NOT_SUPPORTED;
		}
	}

	*out_buffer = bytes;

	return STATUS_SUCCESS;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_db_decodebuffer (
	_Inout_ PDB_INFORMATION db_info,
	_In_ ENUM_TYPE_XML type,
	_In_ ENUM_VERSION_XML min_version
)
{
	ULONG attempts = 6;
	NTSTATUS status;

	if (!db_info->bytes)
		return STATUS_BUFFER_ALL_ZEROS;

	do
	{
		status = _app_db_decodebody (db_info);

		if (status != STATUS_MORE_PROCESSING_REQUIRED)
			break;
	}
	while (--attempts);

	if (!NT_SUCCESS (status))
		return status;

	status = _r_xml_parsestring (&db_info->xml_library, db_info->bytes->buffer, (ULONG)db_info->bytes->length);

	if (FAILED (status))
		return status;

	if (!_r_xml_findchildbytagname (&db_info->xml_library, L"root"))
		return STATUS_DATA_ERROR;

	db_info->timestamp = _r_xml_getattribute_long64 (&db_info->xml_library, L"timestamp");
	db_info->type = _r_xml_getattribute_long (&db_info->xml_library, L"type");
	db_info->version = _r_xml_getattribute_long (&db_info->xml_library, L"version");

	if (db_info->type != type)
		return STATUS_NDIS_INVALID_DATA;

	if (db_info->version < min_version)
		return STATUS_FILE_NOT_SUPPORTED;

	return STATUS_SUCCESS;
}

BOOLEAN _app_db_parse (
	_Inout_ PDB_INFORMATION db_info,
	_In_ ENUM_TYPE_XML type
)
{
	if (!_r_xml_findchildbytagname (&db_info->xml_library, L"root"))
		return FALSE;

	switch (type)
	{
		case XML_TYPE_PROFILE:
		{
			// load apps
			if (_r_xml_findchildbytagname (&db_info->xml_library, L"apps"))
			{
				while (_r_xml_enumchilditemsbytagname (&db_info->xml_library, L"item"))
				{
					_app_db_parse_app (db_info);
				}
			}

			// load rules config
			if (_r_xml_findchildbytagname (&db_info->xml_library, L"rules_config"))
			{
				while (_r_xml_enumchilditemsbytagname (&db_info->xml_library, L"item"))
				{
					_app_db_parse_ruleconfig (db_info);
				}
			}

			// load user rules
			if (_r_xml_findchildbytagname (&db_info->xml_library, L"rules_custom"))
			{
				while (_r_xml_enumchilditemsbytagname (&db_info->xml_library, L"item"))
				{
					_app_db_parse_rule (db_info, DATA_RULE_USER);
				}
			}

			break;
		}

		case XML_TYPE_PROFILE_INTERNAL:
		{
			// load system rules
			if (_r_xml_findchildbytagname (&db_info->xml_library, L"rules_system"))
			{
				while (_r_xml_enumchilditemsbytagname (&db_info->xml_library, L"item"))
				{
					_app_db_parse_rule (db_info, DATA_RULE_SYSTEM);
				}
			}

			// load internal custom rules
			if (_r_xml_findchildbytagname (&db_info->xml_library, L"rules_custom"))
			{
				while (_r_xml_enumchilditemsbytagname (&db_info->xml_library, L"item"))
				{
					_app_db_parse_rule (db_info, DATA_RULE_SYSTEM_USER);
				}
			}

			// load blocklist rules
			if (_r_xml_findchildbytagname (&db_info->xml_library, L"rules_blocklist"))
			{
				while (_r_xml_enumchilditemsbytagname (&db_info->xml_library, L"item"))
				{
					_app_db_parse_rule (db_info, DATA_RULE_BLOCKLIST);
				}
			}

			break;
		}
	}

	return TRUE;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_db_savetofile (
	_Inout_ PDB_INFORMATION db_info,
	_In_ PR_STRING path,
	_In_ ENUM_VERSION_XML version,
	_In_ ENUM_TYPE_XML type,
	_In_ LONG64 timestamp
)
{
	NTSTATUS status;

	status = _r_xml_createstream (&db_info->xml_library, NULL, 1024);

	if (FAILED (status))
		return status;

	_r_xml_writestartdocument (&db_info->xml_library);

	_r_xml_writestartelement (&db_info->xml_library, L"root");

	_r_xml_setattribute_long (&db_info->xml_library, L"version", version);
	_r_xml_setattribute_long (&db_info->xml_library, L"type", type);
	_r_xml_setattribute_long64 (&db_info->xml_library, L"timestamp", timestamp);

	_app_db_save_app (db_info);
	_app_db_save_rule (db_info);
	_app_db_save_ruleconfig (db_info);

	_r_xml_writewhitespace (&db_info->xml_library, L"\r\n");
	_r_xml_writeendelement (&db_info->xml_library);
	_r_xml_writewhitespace (&db_info->xml_library, L"\r\n");
	_r_xml_writeenddocument (&db_info->xml_library);

	status = _app_db_save_streamtofile (db_info, path);

	return status;
}

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_db_save_streamtofile (
	_Inout_ PDB_INFORMATION db_info,
	_In_ PR_STRING path
)
{
	PR_BYTE new_bytes;
	HANDLE hfile;
	BYTE profile_type;
	NTSTATUS status;

	status = _r_fs_createfile (
		&path->sr,
		FILE_OVERWRITE_IF,
		GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		FILE_ATTRIBUTE_NORMAL,
		0,
		FALSE,
		NULL,
		&hfile
	);

	if (!NT_SUCCESS (status))
		return status;

	profile_type = _app_getprofiletype ();

	status = _app_db_encodebody (db_info, profile_type, &new_bytes);

	if (NT_SUCCESS (status))
	{
		_r_fs_writefile (hfile, new_bytes->buffer, (ULONG)new_bytes->length);

		_r_obj_dereference (new_bytes);
	}

	NtClose (hfile);

	return status;
}

FORCEINLINE VOID _app_db_writeelementstart (
	_Inout_ PDB_INFORMATION db_info,
	_In_ LPCWSTR name
)
{
	_r_xml_writewhitespace (&db_info->xml_library, L"\r\n\t");

	_r_xml_writestartelement (&db_info->xml_library, name);
}

FORCEINLINE VOID _app_db_writeelementend (
	_Inout_ PDB_INFORMATION db_info
)
{
	_r_xml_writewhitespace (&db_info->xml_library, L"\r\n\t");

	_r_xml_writeendelement (&db_info->xml_library);
}

VOID _app_db_save_app (
	_Inout_ PDB_INFORMATION db_info
)
{
	PITEM_APP ptr_app = NULL;
	ULONG_PTR enum_key = 0;
	BOOLEAN is_keepunusedapps;
	BOOLEAN is_usedapp;

	is_keepunusedapps = _r_config_getboolean (L"IsKeepUnusedApps", TRUE, NULL);

	_app_db_writeelementstart (db_info, L"apps");

	_r_queuedlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtablepointer (apps_table, (PVOID_PTR)&ptr_app, NULL, &enum_key))
	{
		if (_r_obj_isstringempty (ptr_app->original_path))
			continue;

		is_usedapp = _app_isappused (ptr_app);

		// do not save unused apps/uwp apps...
		if (!is_usedapp && (!is_keepunusedapps || (ptr_app->type == DATA_APP_SERVICE || ptr_app->type == DATA_APP_UWP)))
		{
			//_app_deleteappitem (_r_app_gethwnd (), ptr_app->type, ptr_app->app_hash);

			continue;
		}

		_r_xml_writewhitespace (&db_info->xml_library, L"\r\n\t\t");
		_r_xml_writestartelement (&db_info->xml_library, L"item");

		_r_xml_setattribute (&db_info->xml_library, L"path", ptr_app->original_path->buffer);

		if (!_r_obj_isstringempty (ptr_app->hash) && _r_config_getboolean (L"IsHashesEnabled", FALSE, NULL))
			_r_xml_setattribute (&db_info->xml_library, L"hash", ptr_app->hash->buffer);

		if (!_r_obj_isstringempty (ptr_app->comment))
			_r_xml_setattribute (&db_info->xml_library, L"comment", ptr_app->comment->buffer);

		if (ptr_app->timestamp)
			_r_xml_setattribute_long64 (&db_info->xml_library, L"timestamp", ptr_app->timestamp);

		// set timer (if presented)
		if (ptr_app->timer && _app_istimerset (ptr_app))
			_r_xml_setattribute_long64 (&db_info->xml_library, L"timer", ptr_app->timer);

		// ffu!
		if (ptr_app->profile)
			_r_xml_setattribute_long (&db_info->xml_library, L"profile", ptr_app->profile);

		if (ptr_app->is_undeletable)
			_r_xml_setattribute_boolean (&db_info->xml_library, L"is_undeletable", !!ptr_app->is_undeletable);

		if (ptr_app->is_silent)
			_r_xml_setattribute_boolean (&db_info->xml_library, L"is_silent", !!ptr_app->is_silent);

		if (ptr_app->is_enabled)
			_r_xml_setattribute_boolean (&db_info->xml_library, L"is_enabled", !!ptr_app->is_enabled);

		_r_xml_writeendelement (&db_info->xml_library);
	}

	_r_queuedlock_releaseshared (&lock_apps);

	_app_db_writeelementend (db_info);
}

VOID _app_db_save_rule (
	_Inout_ PDB_INFORMATION db_info
)
{
	PITEM_RULE ptr_rule;
	PR_STRING apps_string;

	_app_db_writeelementstart (db_info, L"rules_custom");

	_r_queuedlock_acquireshared (&lock_rules);

	for (ULONG_PTR i = 0; i < _r_obj_getlistsize (rules_list); i++)
	{
		ptr_rule = _r_obj_getlistitem (rules_list, i);

		if (!ptr_rule || ptr_rule->is_readonly || _r_obj_isstringempty (ptr_rule->name))
			continue;

		_r_xml_writewhitespace (&db_info->xml_library, L"\r\n\t\t");

		_r_xml_writestartelement (&db_info->xml_library, L"item");

		_r_xml_setattribute (&db_info->xml_library, L"name", ptr_rule->name->buffer);

		if (!_r_obj_isstringempty (ptr_rule->rule_remote))
			_r_xml_setattribute (&db_info->xml_library, L"rule", ptr_rule->rule_remote->buffer);

		if (!_r_obj_isstringempty (ptr_rule->rule_local))
			_r_xml_setattribute (&db_info->xml_library, L"rule_local", ptr_rule->rule_local->buffer);

		if (!_r_obj_isstringempty (ptr_rule->comment))
			_r_xml_setattribute (&db_info->xml_library, L"comment", ptr_rule->comment->buffer);

		// ffu!
		if (ptr_rule->profile)
			_r_xml_setattribute_long (&db_info->xml_library, L"profile", ptr_rule->profile);

		if (ptr_rule->direction != FWP_DIRECTION_OUTBOUND)
			_r_xml_setattribute_long (&db_info->xml_library, L"dir", ptr_rule->direction);

		if (ptr_rule->protocol != 0)
			_r_xml_setattribute_long (&db_info->xml_library, L"protocol", ptr_rule->protocol);

		if (ptr_rule->af != AF_UNSPEC)
			_r_xml_setattribute_long (&db_info->xml_library, L"version", ptr_rule->af);

		// add apps attribute
		if (!_r_obj_isempty (ptr_rule->apps))
		{
			apps_string = _app_rulesexpandapps (ptr_rule, FALSE, DIVIDER_APP);

			if (apps_string)
			{
				_r_xml_setattribute (&db_info->xml_library, L"apps", apps_string->buffer);

				_r_obj_dereference (apps_string);
			}
		}

		if (ptr_rule->action == FWP_ACTION_BLOCK)
			_r_xml_setattribute_boolean (&db_info->xml_library, L"is_block", TRUE);

		if (ptr_rule->is_enabled)
			_r_xml_setattribute_boolean (&db_info->xml_library, L"is_enabled", !!ptr_rule->is_enabled);

		_r_xml_writeendelement (&db_info->xml_library);
	}

	_r_queuedlock_releaseshared (&lock_rules);

	_app_db_writeelementend (db_info);
}

VOID _app_db_save_ruleconfig (
	_Inout_ PDB_INFORMATION db_info
)
{
	PITEM_RULE_CONFIG ptr_config = NULL;
	PITEM_RULE ptr_rule;
	PR_STRING apps_string;
	ULONG_PTR enum_key = 0;
	ULONG_PTR rule_hash;
	BOOLEAN is_enabled_default;

	_app_db_writeelementstart (db_info, L"rules_config");

	_r_queuedlock_acquireshared (&lock_rules_config);

	while (_r_obj_enumhashtable (rules_config, (PVOID_PTR)&ptr_config, NULL, &enum_key))
	{
		if (_r_obj_isstringempty (ptr_config->name))
			continue;

		is_enabled_default = ptr_config->is_enabled;
		rule_hash = _r_str_gethash (&ptr_config->name->sr, TRUE);

		ptr_rule = _app_getrulebyhash (rule_hash);

		apps_string = NULL;

		if (ptr_rule)
		{
			is_enabled_default = !!ptr_rule->is_enabled_default;

			if (ptr_rule->type == DATA_RULE_USER && !_r_obj_isempty (ptr_rule->apps))
				apps_string = _app_rulesexpandapps (ptr_rule, FALSE, DIVIDER_APP);

			_r_obj_dereference (ptr_rule);
		}

		// skip saving untouched configuration
		if (ptr_config->is_enabled == is_enabled_default && !apps_string)
			continue;

		_r_xml_writewhitespace (&db_info->xml_library, L"\r\n\t\t");

		_r_xml_writestartelement (&db_info->xml_library, L"item");

		_r_xml_setattribute (&db_info->xml_library, L"name", ptr_config->name->buffer);

		if (apps_string)
		{
			_r_xml_setattribute (&db_info->xml_library, L"apps", apps_string->buffer);

			_r_obj_clearreference ((PVOID_PTR)&apps_string);
		}

		_r_xml_setattribute_boolean (&db_info->xml_library, L"is_enabled", ptr_config->is_enabled);

		_r_xml_writeendelement (&db_info->xml_library);
	}

	_r_queuedlock_releaseshared (&lock_rules_config);

	_app_db_writeelementend (db_info);
}

_Ret_maybenull_
LPCWSTR _app_db_getconnectionstatename (
	_In_ ULONG state
)
{
	switch (state)
	{
		case MIB_TCP_STATE_CLOSED:
			return L"Closed";

		case MIB_TCP_STATE_LISTEN:
			return L"Listen";

		case MIB_TCP_STATE_SYN_SENT:
			return L"SYN sent";

		case MIB_TCP_STATE_SYN_RCVD:
			return L"SYN received";

		case MIB_TCP_STATE_ESTAB:
			return L"Established";

		case MIB_TCP_STATE_FIN_WAIT1:
			return L"FIN wait 1";

		case MIB_TCP_STATE_FIN_WAIT2:
			return L"FIN wait 2";

		case MIB_TCP_STATE_CLOSE_WAIT:
			return L"Close wait";

		case MIB_TCP_STATE_CLOSING:
			return L"Closing";

		case MIB_TCP_STATE_LAST_ACK:
			return L"Last ACK";

		case MIB_TCP_STATE_TIME_WAIT:
			return L"Time wait";

		case MIB_TCP_STATE_DELETE_TCB:
			return L"Delete TCB";
	}

	return NULL;
}

_Ret_maybenull_
PR_STRING _app_db_getdirectionname (
	_In_ FWP_DIRECTION direction,
	_In_ BOOLEAN is_loopback,
	_In_ BOOLEAN is_localized
)
{
	LPCWSTR text = NULL;

	if (is_localized)
	{
		switch (direction)
		{
			case FWP_DIRECTION_OUTBOUND:
			{
				text = _r_locale_getstring (IDS_DIRECTION_1);

				break;
			}

			case FWP_DIRECTION_INBOUND:
			{
				text = _r_locale_getstring (IDS_DIRECTION_2);

				break;
			}

			case FWP_DIRECTION_MAX:
			{
				text = _r_locale_getstring (IDS_ANY);

				break;
			}
		}
	}
	else
	{
		switch (direction)
		{
			case FWP_DIRECTION_OUTBOUND:
			{
				text = SZ_DIRECTION_OUT;

				break;
			}

			case FWP_DIRECTION_INBOUND:
			{
				text = SZ_DIRECTION_IN;

				break;
			}

			case FWP_DIRECTION_MAX:
			{
				text = SZ_DIRECTION_ANY;

				break;
			}
		}
	}

	if (!text)
		return NULL;

	if (is_loopback)
		return _r_obj_concatstrings (2, text, L" (" SZ_DIRECTION_LOOPBACK L")");

	return _r_obj_createstring (text);
}

_Ret_maybenull_
PR_STRING _app_db_getprotoname (
	_In_ ULONG proto,
	_In_ ADDRESS_FAMILY af,
	_In_ BOOLEAN is_notnull
)
{
	switch (proto)
	{
		// NOTE: this is used for "any" protocol
		case IPPROTO_HOPOPTS:
			break;

		case IPPROTO_ICMP:
			return _r_obj_createstring (L"icmp");

		case IPPROTO_IGMP:
			return _r_obj_createstring (L"igmp");

		case IPPROTO_GGP:
			return _r_obj_createstring (L"ggp");

		case IPPROTO_IPV4:
			return _r_obj_createstring (L"ipv4");

		case IPPROTO_ST:
			return _r_obj_createstring (L"st");

		case IPPROTO_TCP:
			return _r_obj_createstring (((af == AF_INET6) ? L"tcp6" : L"tcp"));

		case IPPROTO_CBT:
			return _r_obj_createstring (L"cbt");

		case IPPROTO_EGP:
			return _r_obj_createstring (L"egp");

		case IPPROTO_IGP:
			return _r_obj_createstring (L"igp");

		case IPPROTO_PUP:
			return _r_obj_createstring (L"pup");

		case IPPROTO_UDP:
			return _r_obj_createstring (((af == AF_INET6) ? L"udp6" : L"udp"));

		case IPPROTO_IDP:
			return _r_obj_createstring (L"xns-idp");

		case IPPROTO_RDP:
			return _r_obj_createstring (L"rdp");

		case IPPROTO_IPV6:
			return _r_obj_createstring (L"ipv6");

		case IPPROTO_ROUTING:
			return _r_obj_createstring (L"ipv6-route");

		case IPPROTO_FRAGMENT:
			return _r_obj_createstring (L"ipv6-frag");

		case IPPROTO_ESP:
			return _r_obj_createstring (L"esp");

		case IPPROTO_AH:
			return _r_obj_createstring (L"ah");

		case IPPROTO_ICMPV6:
			return _r_obj_createstring (L"ipv6-icmp");

		case IPPROTO_DSTOPTS:
			return _r_obj_createstring (L"ipv6-opts");

		case IPPROTO_L2TP:
			return _r_obj_createstring (L"l2tp");

		case IPPROTO_SCTP:
			return _r_obj_createstring (L"sctp");
	}

	if (is_notnull)
		return _r_format_string (L"Protocol #%" TEXT (PR_ULONG), proto);

	return NULL;
}

_Ret_maybenull_
LPCWSTR _app_db_getservicename (
	_In_ UINT16 port,
	_In_ UINT8 proto
)
{
	switch (port)
	{
		case 1:
			return L"tcpmux";

		case 2:
		case 3:
			return L"compressnet";

		case 5:
			return L"rje";

		case 7:
			return L"echo";

		case 9:
			return L"discard";

		case 11:
			return L"systat";

		case 13:
			return L"daytime";

		case 15:
		{
			if (proto == IPPROTO_TCP)
				return L"netstat";

			break;
		}

		case 17:
			return L"qotd";

		case 18:
			return L"msp";

		case 20:
			return L"ftp-data";

		case 21:
			return L"ftp";

		case 22:
			return L"ssh";

		case 23:
			return L"telnet";

		case 24:
			return L"priv-mail";

		case 25:
			return L"smtp";

		case 26:
		{
			if (proto == IPPROTO_TCP)
				return L"rsftp";

			break;
		}

		case 27:
			return L"nsw-fe";

		case 29:
			return L"msg-icp";

		case 31:
			return L"msg-auth";

		case 33:
			return L"dsp";

		case 35:
			return L"priv-print";

		case 37:
			return L"time";

		case 38:
			return L"rap";

		case 39:
			return L"rlp";

		case 41:
			return L"graphics";

		case 42:
			return L"nameserver";

		case 43:
			return L"whois";

		case 47:
			return L"ni-ftp";

		case 48:
			return L"auditd";

		case 49:
			return L"tacacs";

		case 50:
			return L"re-mail-ck";

		case 52:
			return L"xns-time";

		case 53:
			return L"domain";

		case 54:
			return L"xns-ch";

		case 55:
			return L"isi-gl";

		case 57:
			return L"priv-term";

		case 58:
			return L"xns-mail";

		case 59:
			return L"priv-file";

		case 61:
			return L"ni-mail";

		case 62:
			return L"acas";

		case 63:
			return L"via-ftp"; // whoispp

		case 64:
			return L"covia";

		case 65:
			return L"tacacs-ds";

		case 66:
			return L"sqlnet";

		case 67:
			return L"dhcps";

		case 68:
			return L"dhcpc";

		case 69:
			return L"tftp";

		case 70:
			return L"gopher";

		case 75:
			return L"priv-dial";

		case 76:
			return L"deos";

		case 77:
			return L"priv-rje"; // netjrs

		case 78:
			return L"vettcp";

		case 79:
		case 2003:
			return L"finger";

		case 80:
			return L"http";

		case 81:
			return L"hosts2-ns";

		case 82:
			return L"xfer";

		case 83:
			return L"mit-ml-dev";

		case 84:
			return L"ctf";

		case 85:
			return L"mit-ml-dev";

		case 86:
			return L"mfcobol";

		case 87:
		{
			if (proto == IPPROTO_TCP)
				return L"priv-term-l";

			break;
		}

		case 88:
			return L"kerberos-sec";

		case 89:
			return L"su-mit-tg";

		case 90:
			return L"dnsix";

		case 91:
			return L"mit-dov";

		case 92:
			return L"npp";

		case 93:
			return L"dcp";

		case 94:
			return L"objcall";

		case 95:
			return L"supdup";

		case 96:
			return L"dixie";

		case 97:
			return L"swift-rvf";

		case 98:
			return L"metagram";

		case 100:
		{
			if (proto == IPPROTO_TCP)
				return L"newacct";

			break;
		}

		case 101:
			return L"hostname";

		case 105:
			return L"csnet-ns";

		case 106:
		{
			if (proto == IPPROTO_TCP)
				return L"pop3pw";

			break;
		}

		case 107:
			return L"rtelnet";

		case 108:
			return L"snagas";

		case 109:
			return L"pop2";

		case 110:
			return L"pop3";

		case 111:
			return L"rpcbind";

		case 112:
			return L"mcidas";

		case 113:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"ident";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"auth";
			}

			break;
		}

		case 114:
			return L"audionews";

		case 115:
			return L"sftp";

		case 116:
			return L"ansanotify";

		case 117:
			return L"uucp-path";

		case 118:
			return L"sqlserv";

		case 119:
			return L"nntp";

		case 121:
			return L"erpc";

		case 123:
			return L"ntp";

		case 125:
			return L"locus-map";

		case 126:
			return L"unitary";

		case 127:
			return L"locus-con";

		case 128:
			return L"gss-xlicen";

		case 129:
			return L"pwdgen";

		case 130:
			return L"cisco-fna";

		case 131:
			return L"cisco-tna";

		case 132:
			return L"cisco-sys";

		case 133:
			return L"statsrv";

		case 134:
			return L"ingres-net";

		case 135:
			return L"msrpc";

		case 136:
			return L"profile";

		case 137:
			return L"netbios-ns";

		case 138:
			return L"netbios-dgm";

		case 139:
			return L"netbios-ssn";

		case 140:
			return L"emfis-data";

		case 141:
			return L"emfis-cntl";

		case 142:
			return L"bl-idm";

		case 143:
			return L"imap";

		case 144:
			return L"news";

		case 145:
			return L"uaac";

		case 148:
			return L"cronus";

		case 149:
			return L"aed-512";

		case 150:
			return L"sql-net";

		case 152:
			return L"bftp";

		case 153:
			return L"sgmp";

		case 154:
			return L"netsc-prod";

		case 155:
			return L"netsc-dev";

		case 156:
			return L"sqlsrv";

		case 159:
			return L"nss-routing";

		case 160:
			return L"sgmp-traps";

		case 161:
			return L"snmp";

		case 162:
			return L"snmptrap";

		case 163:
			return L"cmip-man";

		case 164:
			return L"cmip-agent";

		case 165:
			return L"xns-courier";

		case 166:
			return L"s-net";

		case 167:
			return L"namp";

		case 168:
			return L"rsvd";

		case 169:
			return L"send";

		case 170:
			return L"print-srv";

		case 171:
			return L"multiplex";

		case 172:
			return L"cl-1";

		case 173:
			return L"xyplex-mux";

		case 174:
			return L"mailq";

		case 175:
			return L"vmnet";

		case 176:
			return L"genrad-mux";

		case 177:
			return L"xdmcp";

		case 178:
			return L"nextstep";

		case 179:
			return L"bgp";

		case 180:
			return L"ris";

		case 181:
			return L"unify";

		case 182:
			return L"audit";

		case 183:
			return L"ocbinder";

		case 184:
			return L"ocserver";

		case 185:
			return L"remote-kis";

		case 186:
			return L"kis";

		case 187:
			return L"aci";

		case 188:
			return L"mumps";

		case 189:
			return L"qft";

		case 190:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"gacp";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"cacp";
			}

			break;
		}

		case 191:
			return L"prospero";

		case 192:
			return L"osu-nms";

		case 193:
			return L"srmp";

		case 194:
		case 529:
			return L"irc";

		case 195:
			return L"dn6-nlm-aud";

		case 196:
			return L"dn6-smm-red";

		case 197:
			return L"dls";

		case 198:
			return L"dls-mon";

		case 199:
			return L"smux";

		case 200:
			return L"src";

		case 201:
			return L"at-rtmp";

		case 202:
			return L"at-nbp";

		case 203:
			return L"at-3";

		case 204:
			return L"at-echo";

		case 205:
			return L"at-5";

		case 206:
			return L"at-zis";

		case 207:
			return L"at-7";

		case 208:
			return L"at-8";

		case 209:
			return L"tam"; // qmtp

		case 212:
			return L"anet";

		case 213:
			return L"ipx";

		case 214:
			return L"vmpwscs";

		case 215:
			return L"softpc";

		case 216:
			return L"atls";

		case 217:
			return L"dbase";

		case 218:
			return L"mpp";

		case 219:
			return L"uarps";

		case 220:
			return L"imap3";

		case 221:
		case 222:
			return L"rsh-spx";

		case 223:
			return L"cdc";

		case 224:
			return L"masqdialer";

		case 242:
			return L"direct";

		case 243:
			return L"sur-meas";

		case 244:
			return L"dayna";

		case 245:
			return L"link";

		case 246:
			return L"dsp3270";

		case 247:
			return L"subntbcst_tftp";

		case 248:
			return L"bhfhs";

		case 256:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"fw1-secureremote";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"rap";
			}

			break;
		}

		case 257:
		{
			if (proto == IPPROTO_TCP)
				return L"fw1-mc-fwmodule";

			break;
		}

		case 258:
			return L"yak-chat";

		case 259:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"esro-gen";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"firewall1-rdp";
			}

			break;
		}

		case 260:
			return L"openport";

		case 261:
			return L"nsiiops";

		case 262:
			return L"arcisdms";

		case 264:
			return L"fw1-or-bgmp";

		case 265:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"maybe-fw1";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"x-bone-ctl";
			}

			break;
		}

		case 266:
			return L"sst";

		case 267:
			return L"td-service";

		case 268:
			return L"td-replica";

		case 269:
			return L"manet";

		case 270:
			return L"gist";

		case 271:
		{
			if (proto == IPPROTO_TCP)
				return L"pt-tls";

			break;
		}

		case 280:
			return L"http-mgmt";

		case 281:
			return L"personal-link";

		case 282:
			return L"cableport-ax";

		case 283:
			return L"rescap";

		case 284:
			return L"corerjd";

		case 286:
			return L"fxp";

		case 287:
			return L"k-block";

		case 308:
			return L"novastorbakcup";

		case 309:
			return L"entrusttime";

		case 310:
			return L"bhmds";

		case 311:
			return L"asip-webadmin";

		case 312:
			return L"vslmp";

		case 313:
			return L"magenta-logic";

		case 314:
			return L"opalis-robot";

		case 318:
			return L"pkix-timestamp";

		case 319:
			return L"ptp-event";

		case 320:
			return L"ptp-general";

		case 321:
			return L"pip";

		case 322:
			return L"rtsps";

		case 323:
		{
			if (proto == IPPROTO_TCP)
				return L"rpki-rtr";

			break;
		}

		case 324:
		{
			if (proto == IPPROTO_TCP)
				return L"rpki-rtr-tls";

			break;
		}

		case 333:
			return L"texar";

		case 344:
			return L"pdap";

		case 345:
			return L"pawserv";

		case 346:
			return L"zserv";

		case 347:
			return L"fatserv";

		case 348:
			return L"csi-sgwp";

		case 349:
			return L"mftp";

		case 353:
			return L"ndsauth";

		case 355:
			return L"datex-asn";

		case 358:
			return L"shrinkwrap";

		case 359:
			return L"tenebris_nts";

		case 362:
			return L"srssend";

		case 363:
			return L"rsvp_tunnel";

		case 364:
			return L"aurora-cmgr";

		case 365:
			return L"dtk";

		case 367:
			return L"mortgageware";

		case 369:
			return L"rpc2portmap";

		case 370:
			return L"codaauth2";

		case 375:
			return L"hassle";

		case 376:
			return L"nip";

		case 377:
			return L"tnETOS";

		case 378:
			return L"dsETOS";

		case 381:
			return L"hp-collector";

		case 382:
			return L"hp-managed-node";

		case 383:
			return L"hp-alarm-mgr";

		case 384:
			return L"arns";

		case 386:
			return L"asa";

		case 387:
			return L"aurp";

		case 388:
			return L"unidata-ldm";

		case 389:
			return L"ldap";

		case 391:
			return L"synotics-relay";

		case 392:
			return L"synotics-broker";

		case 393:
			return L"dis";

		case 398:
			return L"kryptolan";

		case 399:
			return L"iso-tsap-c2";

		case 401:
			return L"ups";

		case 402:
			return L"genie";

		case 403:
			return L"decap";

		case 404:
		{
			if (proto == IPPROTO_UDP)
				return L"nced";

			break;
		}

		case 405:
			return L"ncld";

		case 406:
			return L"imsp";

		case 407:
			return L"timbuktu";

		case 410:
			return L"decladebug";

		case 411:
			return L"rmt";

		case 412:
			return L"synoptics-trap";

		case 413:
			return L"smsp";

		case 414:
			return L"infoseek";

		case 415:
			return L"bnet";

		case 417:
			return L"onmux";

		case 418:
			return L"hyper-g";

		case 419:
			return L"ariel1";

		case 420:
			return L"smpte";

		case 421:
			return L"ariel2";

		case 422:
			return L"ariel3";

		case 423:
			return L"opc-job-start";

		case 424:
			return L"opc-job-track";

		case 427:
			return L"svrloc";

		case 428:
			return L"ocs_cmu";

		case 429:
			return L"ocs_amu";

		case 430:
			return L"utmpsd";

		case 431:
			return L"utmpcd";

		case 433:
			return L"nnsp";

		case 434:
			return L"mobileip-agent";

		case 435:
			return L"mobilip-mn";

		case 437:
			return L"comscm";

		case 442:
			return L"cvc_hostd";

		case 443:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"https";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"quic";
			}

			break;
		}

		case 444:
			return L"snpp";

		case 445:
			return L"microsoft-ds";

		case 450:
			return L"tserver";

		case 456:
			return L"macon";

		case 458:
			return L"appleqtc";

		case 464:
			return L"kpasswd5"; // kerberos

		case 465:
			return L"smtps";

		case 469:
			return L"rcp";

		case 470:
			return L"scx-proxy";

		case 471:
			return L"mondex";

		case 473:
			return L"hybrid-pop";

		case 475:
			return L"tcpnethaspsrv";

		case 482:
		{
			if (proto == IPPROTO_UDP)
				return L"xlog";

			break;
		}

		case 485:
			return L"powerburst";

		case 486:
		{
			if (proto == IPPROTO_UDP)
			{
				return L"sstats";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"avian";
			}

			break;
		}

		case 488:
			return L"gss-http";

		case 489:
			return L"nest-protocol";

		case 490:
			return L"micom-pfs";

		case 491:
			return L"go-login";

		case 494:
			return L"pov-ray";

		case 495:
			return L"intecourier";

		case 496:
			return L"pim-rp-disc";

		case 497:
			return L"retrospect";

		case 499:
			return L"iso-ill";

		case 500:
			return L"isakmp";

		case 501:
			return L"stmf";

		case 502:
			return L"mbap";

		case 503:
			return L"intrinsa";

		case 505:
			return L"mailbox-lm";

		case 509:
			return L"snare";

		case 510:
			return L"fcp";

		case 511:
			return L"passgo";

		case 512:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"exec";
			}
			else if (proto == IPPROTO_TCP)
			{
				return L"biff";
			}

			break;
		}

		case 513:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"login";
			}
			else if (proto == IPPROTO_TCP)
			{
				return L"who";
			}

			break;
		}

		case 514:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"shell";
			}
			else if (proto == IPPROTO_TCP)
			{
				return L"syslog";
			}

			break;
		}

		case 515:
			return L"printer";

		case 517:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"talk";
			}
			else if (proto == IPPROTO_TCP)
			{
				return L"talk";
			}

			break;
		}

		case 518:
			return L"ntalk";

		case 519:
			return L"utime";

		case 520:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"efs";
			}
			else if (proto == IPPROTO_TCP)
			{
				return L"route";
			}

			break;
		}

		case 521:
			return L"ripng";

		case 524:
			return L"ncp";

		case 525:
			return L"timed";

		case 526:
			return L"tempo";

		case 530:
			return L"courier"; // rpc

		case 531:
			return L"conference";

		case 532:
			return L"netnews";

		case 533:
			return L"netwall";

		case 537:
			return L"nmsp";

		case 540:
			return L"uucp";

		case 541:
			return L"uucp-rlogin";

		case 543:
			return L"klogin";

		case 544:
			return L"kshell";

		case 545:
		{
			if (proto == IPPROTO_TCP)
				return L"ekshell";

			break;
		}

		case 546:
			return L"dhcpv6-client";

		case 547:
			return L"dhcpv6-server";

		case 548:
			return L"afp";

		case 550:
			return L"new-rwho";

		case 554:
			return L"rtsp";

		case 555:
			return L"dsf";

		case 556:
			return L"remotefs";

		case 558:
			return L"sdnskmp";

		case 560:
			return L"rmonitor";

		case 561:
			return L"monitor";

		case 562:
			return L"chshell";

		case 563:
			return L"snews";

		case 565:
			return L"whoami";

		case 568:
			return L"ms-shuttle";

		case 569:
			return L"ms-rome";

		case 574:
			return L"ftp-agent";

		case 580:
			return L"sntp-heartbeat";

		case 582:
			return L"scc-security";

		case 584:
			return L"keyserver";

		case 585:
			return L"imap4-ssl";

		case 587:
			return L"submission";

		case 591:
		case 8000:
		case 8008:
		case 8080:
			return L"http-alt";

		case 593:
			return L"http-rpc-epmap";

		case 609:
			return L"npmp-trap";

		case 610:
			return L"npmp-local";

		case 611:
			return L"npmp-gui";

		case 614:
			return L"sshell";

		case 620:
			return L"sco-websrvrmgr";

		case 624:
			return L"cryptoadmin";

		case 625:
			return L"apple-xsrvr-admin";

		case 626:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"apple-imap-admin";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"serialnumberd";
			}

			break;
		}

		case 628:
			return L"qmqp";

		case 629:
			return L"3com-amp3";

		case 630:
			return L"rda";

		case 631:
			return L"ipp";

		case 633:
			return L"servstat";

		case 636:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"ldapssl";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"ldaps";
			}

			break;
		}

		case 639:
			return L"msdp";

		case 646:
			return L"ldp";

		case 647:
			return L"dhcp-failover";

		case 648:
			return L"rrp";

		case 651:
			return L"ieee-mms";

		case 652:
			return L"hello-port";

		case 653:
			return L"repscmd";

		case 656:
			return L"spmp";

		case 658:
			return L"tenfold";

		case 660:
			return L"mac-srvr-admin";

		case 662:
			return L"pftp";

		case 663:
			return L"purenoise";

		case 666:
			return L"doom"; // khe-khe-khe!

		case 667:
			return L"disclose";

		case 674:
			return L"acapc"; // stalker

		case 678:
			return L"ggf-ncp";

		case 687:
			return L"asipregistry";

		case 691:
		{
			if (proto == IPPROTO_UDP)
				return L"msexch-routing";

			break;
		}

		case 697:
			return L"msexch-routing";

		case 699:
			return L"accessnetwork";

		case 701:
			return L"lmp";

		case 707:
			return L"borland-dsj";

		case 709:
			return L"entrustmanager";

		case 710:
			return L"entrust-ash";

		case 711:
			return L"cisco-tdp";

		case 713:
			return L"iris-xpc";

		case 714:
			return L"iris-xpcs";

		case 716:
			return L"pana";

		case 740:
			return L"netcp";

		case 741:
			return L"netgw";

		case 742:
			return L"netrcs";

		case 747:
			return L"fujitsu-dev";

		case 751:
			return L"kadmin";

		case 754:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"krb_prop";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"tell";
			}

			break;
		}

		case 758:
			return L"nlogin";

		case 761:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"kpasswd";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"rxe";
			}

			break;
		}

		case 767:
			return L"phonebook";

		case 773:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"submit";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"notify";
			}

			break;
		}

		case 777:
			return L"multiling-http";

		case 783:
		{
			if (proto == IPPROTO_TCP)
				return L"spamassassin";

			break;
		}

		case 799:
		{
			if (proto == IPPROTO_TCP)
				return L"controlit";

			break;
		}

		case 800:
			return L"mdbs_daemon";

		case 802:
		{
			if (proto == IPPROTO_TCP)
				return L"mbap-s";

			break;
		}

		case 808:
		{
			if (proto == IPPROTO_TCP)
				return L"ccproxy-http";

			break;
		}

		case 810:
			return L"fcp-udp";

		case 828:
			return L"itm-mcell-s";

		case 829:
			return L"pkix-3-ca-ra";

		case 830:
			return L"netconf-ssh";

		case 831:
			return L"netconf-beep";

		case 832:
			return L"netconfsoaphttp";

		case 833:
			return L"netconfsoapbeep";

		case 847:
			return L"dhcp-failover2";

		case 853:
		{
			if (proto == IPPROTO_TCP)
				return L"domain-s";

			break;
		}

		case 854:
		{
			if (proto == IPPROTO_TCP)
				return L"dlep";

			break;
		}

		case 860:
			return L"iscsi";

		case 861:
			return L"owamp-control";

		case 862:
			return L"twamp-control";

		case 873:
			return L"rsync";

		case 888:
			return L"accessbuilder";

		case 901:
		{
			if (proto == IPPROTO_TCP)
				return L"samba-swat";

			break;
		}

		case 910:
			return L"kink";

		case 950:
		{
			if (proto == IPPROTO_TCP)
				return L"oftep-rpc";

			break;
		}

		case 989:
			return L"ftps-data";

		case 990:
			return L"ftps";

		case 991:
			return L"nas";

		case 992:
			return L"telnets";

		case 993:
			return L"imaps";

		case 994:
			return L"ircs";

		case 995:
			return L"pop3s";

		case 1001:
			return L"webpush";

		case 1002:
		{
			if (proto == IPPROTO_TCP)
				return L"windows-icfw";

			break;
		}

		case 1025:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"NFS-or-IIS";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"blackjack";
			}

			break;
		}

		case 1026:
		{
			if (proto == IPPROTO_UDP)
				return L"win-rpc";

			break;
		}

		case 1027:
		{
			if (proto == IPPROTO_TCP)
				return L"IIS";

			break;
		}

		case 1028:
		{
			if (proto == IPPROTO_UDP)
				return L"ms-lsa";

			break;
		}

		case 1029:
		{
			if (proto == IPPROTO_TCP)
				return L"ms-lsa";

			break;
		}

		case 1033:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"netinfo";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"netinfo-local";
			}

			break;
		}

		case 1035:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"multidropper";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"mxxrlogin";
			}

			break;
		}

		case 1050:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"java-or-OTGfileshare";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"cma";
			}

			break;
		}

		case 1080:
			return L"socks";

		case 1085:
			return L"webobjects";

		case 1096:
			return L"cnrprotocol";

		case 1098:
			return L"rmiactivation";

		case 1099:
			return L"rmiregistry";

		case 1100:
			return L"mctp";

		case 1109:
		{
			if (proto == IPPROTO_TCP)
				return L"kpop"; // kerberos

			break;
		}

		case 1110:
			return L"nfsd";

		case 1111:
			return L"lmsocialserver";

		case 1112:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"msql";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"icp";
			}

			break;
		}

		case 1114:
		case 4333:
			return L"mini-sql";

		case 1119:
			return L"bnetgame";

		case 1120:
			return L"bnetfile";

		case 1121:
			return L"rmpp";

		case 1123:
			return L"murray";

		case 1130:
			return L"casp";

		case 1131:
			return L"caspssl";

		case 1138:
			return L"encrypted_admin";

		case 1147:
			return L"capioverlan";

		case 1150:
			return L"blaze";

		case 1153:
			return L"c1222-acse";

		case 1155:
			return L"nfa";

		case 1159:
			return L"oracle-oms";

		case 1164:
			return L"qsm-proxy";

		case 1165:
			return L"qsm-gui";

		case 1166:
			return L"qsm-remote";

		case 1168:
			return L"vchat";

		case 1183:
			return L"llsurfup-http";

		case 1184:
			return L"llsurfup-https";

		case 1186:
			return L"mysql-cluster";

		case 1187:
			return L"alias";

		case 1188:
			return L"hp-webadmin";

		case 1194:
			return L"openvpn";

		case 1214:
			return L"fasttrack"; // kazaa

		case 1220:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"quicktime";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"qt-serveradmin";
			}

			break;
		}

		case 1234:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"hotline";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"search-agent";
			}

			break;
		}

		case 1257:
			return L"shockwave2";

		case 1258:
			return L"opennl";

		case 1259:
			return L"opennl-voice";

		case 1270:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"ssserver";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"opsmgr"; // Microsoft Operations Manager
			}

			break;
		}

		case 1273:
			return L"emc-gateway";

		case 1307:
			return L"pacmand";

		case 1311:
			return L"rxmon";

		case 1318:
			return L"krb5gatekeeper";

		case 1321:
			return L"pip";

		case 1333:
			return L"passwrd-policy";

		case 1336:
			return L"ischat";

		case 1337:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"waste";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"menandmice-dns";
			}

			break;
		}

		case 1368:
			return L"screencast";

		case 1380:
			return L"telesis-licman";

		case 1381:
			return L"apple-licman";

		case 1384:
			return L"os-licman";

		case 1433:
			return L"ms-sql-s";

		case 1434:
			return L"ms-sql-m";

		case 1462:
			return L"world-lm";

		case 1498:
			return L"watcom-sql";

		case 1525:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"orasrv";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"oracle";
			}

			break;
		}

		case 1529:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"support";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"coauthor";
			}

			break;
		}

		case 1590:
			return L"gemini-lm";

		case 1630:
			return L"oraclenet8cman";

		case 1687:
			return L"nsjtp-ctrl";

		case 1688:
			return L"nsjtp-data";

		case 1689:
			return L"firefox";

		case 1701:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"l2f";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"L2TP";
			}

			break;
		}

		case 1702:
			return L"deskshare";

		case 1720:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"h323q931";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"h323q931";
			}

			break;
		}

		case 1723:
			return L"pptp";

		case 1726:
			return L"iberiagames";

		case 1733:
			return L"siipat";

		case 1745:
			return L"remote-winsock";

		case 1748:
			return L"oracle-em1";

		case 1750:
			return L"sslp";

		case 1755:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"wms";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"ms-streaming";
			}

			break;
		}

		case 1758:
			return L"tftp-mcast";

		case 1761:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"landesk-rc";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"cft-0";
			}

			break;
		}

		case 1762:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"landesk-rc";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"cft-1";
			}

			break;
		}

		case 1763:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"landesk-rc";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"cft-2";
			}

			break;
		}

		case 1764:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"landesk-rc";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"cft-3";
			}

			break;
		}

		case 1789:
			return L"hello";

		case 1793:
			return L"rsc-robot";

		case 1795:
			return L"dpi-proxy";

		case 1801:
			return L"msmq"; // Microsoft Message Queuing

		case 1833:
			return L"udpradio";

		case 1850:
			return L"gsi";

		case 1862:
			return L"mysql-cm-agent";

		case 1863:
			return L"msnp"; // MSN Messenger

		case 1900:
		case 5000:
			return L"upnp";

		case 1931:
			return L"amdsched";

		case 1981:
			return L"p2pq";

		case 2000:
			return L"cisco-sccp";

		case 2001:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"dc";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"wizard";
			}

			break;
		}

		case 2009:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"news";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"whosockami";
			}

			break;
		}

		case 2019:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"whosockami";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"about";
			}

			break;
		}

		case 2054:
			return L"weblogin";

		case 2086:
			return L"gnunet";

		case 2105:
			return L"eklogin"; // kerberos

		case 2106:
			return L"ekshell"; // kerberos

		case 2115:
			return L"kdm"; // Key Distribution Manager

		case 2121:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"ccproxy-ftp";
			}
			else if (proto == IPPROTO_UDP)
			{
				return L"scientia-ssdb";
			}

			break;
		}

		case 2129:
			return L"cs-live";

		case 2137:
			return L"connect";

		case 2142:
			return L"tdmoip";

		case 2159:
			return L"gdbremote";

		case 2164:
			return L"ddns-v3"; // Dynamic DNS Version 3

		case 2167:
			return L"raw-serial"; // Raw Async Serial Link

		case 2171:
			return L"msfw-storage"; // MS Firewall Storage

		case 2172:
			return L"msfw-s-storage"; // MS Firewall SecureStorage

		case 2173:
			return L"msfw-replica"; // MS Firewall Replication

		case 2174:
			return L"msfw-array"; // MS Firewall Intra Array

		case 2175:
			return L"ms-airsync"; // Microsoft Desktop AirSync Protocol

		case 2179:
			return L"ms-vmrdp"; // Microsoft RDP for virtual machines

		case 2191:
			return L"tvbus";

		case 2193:
			return L"drwcs"; // Dr.Web Enterprise Management Service

		case 2213:
			return L"kali";

		case 2273:
			return L"mysql-im"; // MySQL Instance Manager

		case 2311:
			return L"messageservice"; // Message Service

		case 2169:
		{
			if (proto == IPPROTO_TCP)
				return L"bif-p2p";

			break;
		}

		case 2374:
			return L"hydra";

		case 2375:
		{
			if (proto == IPPROTO_TCP)
				return L"docker";

			break;
		}

		case 2376:
		{
			if (proto == IPPROTO_TCP)
				return L"docker";

			break;
		}

		case 2377:
		{
			if (proto == IPPROTO_TCP)
				return L"swarm";

			break;
		}

		case 2382:
			return L"ms-olap3"; // Microsoft OLAP

		case 2383:
			return L"ms-olap4"; // Microsoft OLAP

		case 2525:
			return L"ms-v-worlds"; // MS V-Worlds

		case 2679:
			return L"syncserverssl";

		case 2710:
			return L"sso-service";

		case 2711:
			return L"sso-control";

		case 2717:
			return L"pn-requester";

		case 2718:
			return L"pn-requester2";

		case 2723:
			return L"watchdog-nt";

		case 2725:
			return L"msolap-ptp2";

		case 2775:
			return L"smpp";

		case 2784:
			return L"www-dev";

		case 2869:
			return L"icslap";

		case 2948:
			return L"wap-push";

		case 2949:
			return L"wap-pushsecure";

		case 2947:
			return L"symantec-av";

		case 2979:
			return L"h263-video";

		case 3000:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"ppp";
			}
			if (proto == IPPROTO_UDP)
			{
				return L"hbci";
			}

			break;
		}

		case 3074:
			return L"xbox";

		case 3128:
		{
			if (proto == IPPROTO_TCP)
				return L"squid-http";

			break;
		}

		case 3224:
			return L"aes-discovery";

		case 3260:
			return L"iscsi";

		case 3295:
			return L"dyniplookup";

		case 3306:
			return L"mysql";

		case 3326:
			return L"sftu"; // ;)

		case 3343:
			return L"ms-cluster-net";

		case 3352:
			return L"ssql"; // Scalable SQL

		case 3389:
			return L"ms-wbt-server"; // Microsoft Remote Display Protocol (aka ms-term-serv, microsoft-rdp)

		case 3407:
			return L"ldap-admin";

		case 3476:
			return L"nppmp"; // NVIDIA Mgmt Protocol

		case 3500:
			return L"rtmp-port";

		case 3535:
			return L"ms-la";

		case 3540:
			return L"pnrp-port";

		case 3544:
			return L"teredo";

		case 3547:
			return L"symantec-sim";

		case 3550:
			return L"ssmpp"; // Secure SMPP

		case 3558:
			return L"mcp-port"; // MCP user port

		case 3559:
			return L"cctv-port"; // CCTV control port

		case 3563:
			return L"watcomdebug";

		case 3587:
			return L"p2pgroup";

		case 3702:
			return L"ws-discovery";

		case 3713:
			return L"tftps";

		case 3721:
			return L"xsync";

		case 3724:
			return L"blizwow";

		case 4041:
			return L"ltp"; // Location Tracking Protocol

		case 4180:
			return L"httpx";

		case 4317:
		{
			if (proto == IPPROTO_TCP)
				return L"opentelemetry";

			break;
		}

		case 4321:
			return L"rwhois"; // Remote Who Is

		case 4500:
		{
			if (proto == IPPROTO_UDP)
				return L"nat-t-ike";

			break;
		}

		case 4554:
			return L"msfrs"; // MS FRS Replication

		case 4687:
			return L"nst"; // Network Scanner Tool FTP

		case 4876:
			return L"tritium-can";

		case 4899:
			return L"radmin";

		case 5004:
			return L"rtp-data";

		case 5005:
			return L"rtp";

		case 5009:
			return L"airport-admin";

		case 5051:
			return L"ida-agent";

		case 5060:
			return L"sip";

		case 5101:
			return L"admdog";

		case 5145:
			return L"rmonitor_secure";

		case 5190:
		case 5191:
		case 5192:
		case 5193:
			return L"aol";

		case 5350:
			return L"nat-pmp-status";

		case 5351:
			return L"nat-pmp";

		case 5352:
			return L"dns-llq";

		case 5353:
			return L"mdns";

		case 5354:
			return L"mdnsresponder";

		case 5355:
			return L"llmnr";

		case 5357:
			return L"wsdapi";

		case 5358:
			return L"wsdapi-s";

		case 5359:
			return L"ms-alerter";

		case 5360:
			return L"ms-sideshow";

		case 5361:
			return L"ms-s-sideshow";

		case 5362:
			return L"serverwsd2"; // Microsoft Windows Server WSD2 Service

		case 5432:
			return L"postgresql";

		case 5631:
			return L"pcanywheredata";

		case 5632:
			return L"pcanywherestat";

		case 5666:
		{
			if (proto == IPPROTO_TCP)
				return L"nrpe"; // Nagios Remote Plugin Executor

			break;
		}

		case 5687:
		{
			if (proto == IPPROTO_TCP)
				return L"gog-multiplayer";

			break;
		}

		case 5741:
			return L"ida-discover1";

		case 5742:
			return L"ida-discover2";

		case 5800:
		case 5801:
		case 5802:
		case 5803:
		{
			if (proto == IPPROTO_TCP)
				return L"vnc-http";

			break;
		}

		case 5900:
		{
			if (proto == IPPROTO_TCP)
				return L"vnc";

			break;
		}

		case 5901:
		{
			if (proto == IPPROTO_TCP)
				return L"vnc-1";

			break;
		}

		case 5902:
		{
			if (proto == IPPROTO_TCP)
				return L"vnc-2";

			break;
		}

		case 5903:
		{
			if (proto == IPPROTO_TCP)
				return L"vnc-3";

			break;
		}

		case 5938:
		{
			if (proto == IPPROTO_TCP)
				return L"teamviewer";

			break;
		}

		case 6000:
		case 6001:
		case 6002:
		case 6003:
		case 6004:
		case 6005:
		case 6006:
		case 6007:
		case 6008:
		case 6009:
		case 6010:
		case 6011:
		case 6012:
		case 6013:
		case 6014:
		case 6015:
		case 6016:
		case 6017:
		case 6018:
		case 6019:
		case 6020:
		case 6021:
		case 6022:
		case 6023:
		case 6024:
		case 6025:
		case 6026:
		case 6027:
		case 6028:
		case 6029:
		case 6030:
		case 6031:
		case 6032:
		case 6033:
		case 6034:
		case 6035:
		case 6036:
		case 6037:
		case 6038:
		case 6039:
		case 6040:
		case 6041:
		case 6042:
		case 6043:
		case 6044:
		case 6045:
		case 6046:
		case 6047:
		case 6048:
		case 6049:
		case 6050:
		case 6051:
		case 6052:
		case 6053:
		case 6054:
		case 6055:
		case 6056:
		case 6057:
		case 6058:
		case 6059:
		case 6060:
		case 6061:
		case 6062:
		case 6063:
			return L"x11";

		case 6074:
			return L"max"; // Microsoft Max

		case 6076:
		{
			if (proto == IPPROTO_TCP)
				return L"msft-dpm-cert"; // Microsoft DPM WCF Certificates

			break;
		}

		case 6222:
		case 6662: // deprecated!
			return L"radmind";

		case 6346:
			return L"gnutella";

		case 6347:
			return L"gnutella2";

		case 6620:
			return L"kftp-data"; // Kerberos V5 FTP Data

		case 6621:
			return L"kftp"; // Kerberos V5 FTP Control

		case 6622:
			return L"mcftp";

		case 6665:
		case 6666:
		case 6667:
		case 6668:
		case 6669:
		case 6670:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"irc";
			}
			if (proto == IPPROTO_UDP)
			{
				return L"ircu";
			}

			break;
		}


		case 6881:
		{
			if (proto == IPPROTO_TCP)
				return L"bittorrent-tracker";

			break;
		}

		case 7070:
		{
			if (proto == IPPROTO_TCP)
				return L"realserver";

			break;
		}

		case 7235:
		{
			if (proto == IPPROTO_TCP)
				return L"aspcoordination";

			break;
		}

		case 8021:
		{
			if (proto == IPPROTO_TCP)
				return L"ftp-proxy";

			break;
		}

		case 8333:
		case 18333:
		{
			if (proto == IPPROTO_TCP)
				return L"bitcoin";

			break;
		}

		case 8443:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"https-alt";

			}
			else if (proto == IPPROTO_UDP)
			{
				return L"pcsync-https";
			}

			break;
		}

		case 8444:
			return L"pcsync-http";

		case 8999:
			return L"bctp";

		case 9418:
			return L"git";

		case 9800:
			return L"davsrc"; // WebDav Source Port

		case 10107:
			return L"bctp-server";

		case 11371:
		{
			if (proto == IPPROTO_TCP)
			{
				return L"pksd";

			}
			else if (proto == IPPROTO_UDP)
			{
				return L"hkp";
			}

			break;
		}

		case 25565:
		{
			if (proto == IPPROTO_TCP)
			return L"minecraft";

			break;
		}

		case 26000:
			return L"quake";

		case 27015:
		{
			if (proto == IPPROTO_UDP)
				return L"halflife";

			break;
		}

		case 27017:
		case 27018:
		case 27019:
		case 28017:
		{
			if (proto == IPPROTO_TCP)
			return L"mongod";

			break;
		}

		case 27500:
		{
			if (proto == IPPROTO_UDP)
			return L"quakeworld";

			break;
		}

		case 27910:
		{
			if (proto == IPPROTO_UDP)
				return L"quake2";

			break;
		}

		case 27960:
		{
			if (proto == IPPROTO_UDP)
				return L"quake3";

			break;
		}

		case 28240:
			return L"siemensgsm";

		case 33434:
			return L"traceroute";
	}

	return NULL;
}
