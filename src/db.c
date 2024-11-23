// simplewall
// Copyright (c) 2019-2024 Henry++

#include "global.h"

_Success_ (NT_SUCCESS (return))
NTSTATUS _app_db_initialize (
	_Out_ PDB_INFORMATION db_info,
	_In_ BOOLEAN is_reader
)
{
	NTSTATUS status;

	RtlZeroMemory (db_info, sizeof (DB_INFORMATION));

	status = _r_xml_initializelibrary (&db_info->xml_library, is_reader);

	return status;
}

VOID _app_db_destroy (
	_Inout_ PDB_INFORMATION db_info
)
{
	if (db_info->bytes)
		_r_obj_clearreference (&db_info->bytes);

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

	profile_type = _r_config_getlong (L"ProfileType", 0);

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

	_r_obj_movereference (&db_info->bytes, _r_obj_createbyte4 (buffer));

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
		_r_obj_clearreference (&db_info->bytes);

	status = _r_fs_openfile (&path->sr, GENERIC_READ, FILE_SHARE_READ, 0, FALSE, &hfile);

	if (!NT_SUCCESS (status))
		return status;

	status = _r_fs_readfilebytes (hfile, &db_info->bytes);

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
	ULONG_PTR app_hash;
	PR_STRING string;
	PR_STRING path;
	PR_STRING dos_path;
	LONG64 timestamp;
	LONG64 timer;
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
		dos_path = _r_path_dospathfromnt (path);

		if (dos_path)
			_r_obj_movereference (&path, dos_path);
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
					_app_setappinfo (ptr_app, INFO_IS_SILENT, IntToPtr (is_silent));

				if (is_enabled)
					_app_setappinfo (ptr_app, INFO_IS_ENABLED, IntToPtr (is_enabled));

				if (is_undeletable)
					_app_setappinfo (ptr_app, INFO_IS_UNDELETABLE, IntToPtr (is_undeletable));

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
	ULONG_PTR app_hash;
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
	ULONG_PTR rule_hash;
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

	rule_hash = _r_str_gethash2 (&ptr_rule->name->sr, TRUE);

	if (!_r_obj_isstringempty (comment))
	{
		_r_obj_movereference (&ptr_rule->comment, comment);
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
		blocklist_spy_state = _r_calc_clamp (_r_config_getlong (L"BlocklistSpyState", 2), 0, 2);
		blocklist_update_state = _r_calc_clamp (_r_config_getlong (L"BlocklistUpdateState", 0), 0, 2);
		blocklist_extra_state = _r_calc_clamp (_r_config_getlong (L"BlocklistExtraState", 0), 0, 2);

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

			app_hash = _r_str_gethash2 (&path_string->sr, TRUE);

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
						_app_setappinfobyhash (app_hash, INFO_IS_UNDELETABLE, IntToPtr (TRUE));
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
	_r_obj_addlistitem (rules_list, ptr_rule);
	_r_queuedlock_releaseexclusive (&lock_rules);

	_r_obj_deletestringbuilder (&sb);
}

VOID _app_db_parse_ruleconfig (
	_Inout_ PDB_INFORMATION db_info
)
{
	PITEM_RULE_CONFIG ptr_config;
	PR_STRING rule_name;
	ULONG_PTR rule_hash;

	rule_name = _r_xml_getattribute_string (&db_info->xml_library, L"name");

	if (!rule_name)
		return;

	rule_hash = _r_str_gethash2 (&rule_name->sr, TRUE);

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
	static USHORT format[] = {COMPRESSION_FORMAT_LZNT1, COMPRESSION_FORMAT_XPRESS};

	PR_BYTE new_bytes;
	USHORT architecture;
	BYTE profile_type;
	NTSTATUS status;

	if (db_info->bytes->length < PROFILE2_HEADER_LENGTH)
		return STATUS_SUCCESS;

	if (!RtlEqualMemory (db_info->bytes->buffer, profile2_fourcc, sizeof (profile2_fourcc)))
		return STATUS_SUCCESS;

	profile_type = db_info->bytes->buffer[sizeof (profile2_fourcc)];

	// skip fourcc
	_r_obj_skipbytelength (&db_info->bytes->sr, PROFILE2_FOURCC_LENGTH);

	// read the hash
	_r_obj_movereference (&db_info->hash, _r_obj_createbyte_ex (db_info->bytes->buffer, PROFILE2_SHA256_LENGTH));

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
					_r_obj_movereference (&db_info->bytes, new_bytes);

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

			_r_obj_movereference (&db_info->bytes, new_bytes);

			break;
		}

		default:
		{
			return STATUS_FILE_NOT_SUPPORTED;
		}
	}

	if (!NT_SUCCESS (status))
		return status;

	if (RtlEqualMemory (db_info->bytes->buffer, profile2_fourcc, sizeof (profile2_fourcc)))
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

	_r_obj_movereference (&bytes, new_bytes);

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

	is_keepunusedapps = _r_config_getboolean (L"IsKeepUnusedApps", TRUE);

	_app_db_writeelementstart (db_info, L"apps");

	_r_queuedlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtablepointer (apps_table, &ptr_app, NULL, &enum_key))
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

		if (!_r_obj_isstringempty (ptr_app->hash) && _r_config_getboolean (L"IsHashesEnabled", FALSE))
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

	while (_r_obj_enumhashtable (rules_config, &ptr_config, NULL, &enum_key))
	{
		if (_r_obj_isstringempty (ptr_config->name))
			continue;

		is_enabled_default = ptr_config->is_enabled;
		rule_hash = _r_str_gethash2 (&ptr_config->name->sr, TRUE);

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

			_r_obj_clearreference (&apps_string);
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

		case 7:
			return L"echo";

		case 9:
			return L"discard";

		case 11:
			return L"systat";

		case 13:
			return L"daytime";

		case 20:
			return L"ftp-data";

		case 21:
			return L"ftp";

		case 22:
			return L"ssh";

		case 23:
			return L"telnet";

		case 25:
			return L"smtp";

		case 26:
			return L"rsftp";

		case 37:
			return L"time";

		case 39:
			return L"rlp";

		case 42:
			return L"nameserver";

		case 43:
			return L"nicname";

		case 48:
			return L"auditd";

		case 53:
			return L"domain";

		case 63:
			return L"whois++";

		case 67:
		case 68:
			return L"dhcp";

		case 69:
			return L"tftp";

		case 78:
			return L"vettcp";

		case 79:
		case 2003:
			return L"finger";

		case 80:
			return L"http";

		case 81:
			return L"hosts2-ns";

		case 84:
			return L"ctf";

		case 88:
			return L"kerberos-sec";

		case 90:
			return L"dnsix";

		case 92:
			return L"npp";

		case 93:
			return L"dcp";

		case 94:
			return L"objcall";

		case 95:
			return L"supdup";

		case 101:
			return L"hostname";

		case 105:
			return L"cso";

		case 106:
			return L"pop3pw";

		case 107:
			return L"rtelnet";

		case 109:
			return L"pop2";

		case 110:
			return L"pop3";

		case 111:
			return L"rpcbind";

		case 112:
			return L"mcidas";

		case 113:
			return L"auth";

		case 115:
			return L"sftp";

		case 118:
			return L"sqlserv";

		case 119:
			return L"nntp";

		case 123:
			return L"ntp";

		case 126:
			return L"nxedit";

		case 129:
			return L"pwdgen";

		case 133:
			return L"statsrv";

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

		case 143:
			return L"imap";

		case 144:
			return L"news";

		case 145:
			return L"uaac";

		case 150:
			return L"sql-net";

		case 152:
			return L"bftp";

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

		case 169:
			return L"send";

		case 170:
			return L"print-srv";

		case 174:
			return L"mailq";

		case 175:
			return L"vmnet";

		case 179:
			return L"bgp";

		case 182:
			return L"audit";

		case 185:
			return L"remote-kis";

		case 186:
			return L"kis";

		case 189:
			return L"qft";

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

		case 209:
			return L"qmtp";

		case 245:
			return L"link";

		case 280:
			return L"http-mgmt";

		case 322:
			return L"rtsps";

		case 349:
			return L"mftp";

		case 389:
			return L"ldap";

		case 427:
			return L"svrloc";

		case 443:
		{
			if (proto == IPPROTO_UDP)
				return L"quic";

			return L"https";
		}

		case 444:
			return L"snpp";

		case 445:
			return L"microsoft-ds";

		case 464:
			return L"kerberos";

		case 465:
			return L"smtps";

		case 500:
			return L"isakmp";

		case 513:
			return L"login";

		case 514:
			return L"shell";

		case 515:
			return L"printer";

		case 524:
			return L"ncp";

		case 530:
			return L"rpc";

		case 543:
			return L"klogin";

		case 544:
			return L"kshell";

		case 546:
			return L"dhcpv6-client";

		case 547:
			return L"dhcpv6-server";

		case 548:
			return L"afp";

		case 554:
			return L"rtsp";

		case 565:
			return L"whoami";

		case 558:
			return L"sdnskmp";

		case 585:
			return L"imap4-ssl";

		case 587:
			return L"submission";

		case 631:
			return L"ipp";

		case 636:
			return L"ldaps";

		case 646:
			return L"ldp";

		case 647:
			return L"dhcp-failover";

		case 666:
			return L"doom"; // khe-khe-khe!

		case 847:
			return L"dhcp-failover2";

		case 861:
			return L"owamp-control";

		case 862:
			return L"twamp-control";

		case 873:
			return L"rsync";

		case 853:
		{
			if (proto == IPPROTO_TCP)
				return L"domain-s";

			break;
		}

		case 989:
			return L"ftps-data";

		case 990:
			return L"ftps";

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
			return L"windows-icfw";

		case 1025:
			return L"NFS-or-IIS";

		case 1026:
		{
			if (proto == IPPROTO_UDP)
				return L"win-rpc";

			break;
		}

		case 1027:
			return L"IIS";

		case 1028:
		case 1029:
			return L"ms-lsa";

		case 1033:
		{
			if (proto == IPPROTO_UDP)
				return L"netinfo-local";

			return L"netinfo";
		}

		case 1080:
			return L"socks";

		case 1085:
			return L"webobjects";

		case 1100:
			return L"mctp";

		case 1110:
			return L"nfsd";

		case 1111:
			return L"lmsocialserver";

		case 1112:
		case 1114:
		case 4333:
			return L"mini-sql";

		case 1119:
			return L"bnetgame";

		case 1120:
			return L"bnetfile";

		case 1123:
			return L"murray";

		case 1138:
			return L"encrypted_admin";

		case 1155:
			return L"nfa";

		case 1194:
			return L"openvpn";

		case 1337:
			return L"menandmice-dns";

		case 1433:
			return L"ms-sql-s";

		case 1688:
			return L"nsjtp-data";

		case 1701:
			return L"l2tp";

		case 1720:
			return L"h323q931";

		case 1723:
			return L"pptp";

		case 1863:
			return L"msnp";

		case 1900:
		case 5000:
			return L"upnp";

		case 2000:
			return L"cisco-sccp";

		case 2054:
			return L"weblogin";

		case 2086:
			return L"gnunet";

		case 2001:
			return L"dc";

		case 2121:
			return L"ccproxy-ftp";

		case 2164:
			return L"ddns-v3";

		case 2167:
			return L"raw-serial";

		case 2171:
			return L"msfw-storage";

		case 2172:
			return L"msfw-s-storage";

		case 2173:
			return L"msfw-replica";

		case 2174:
			return L"msfw-array";

		case 2371:
			return L"worldwire";

		case 2717:
			return L"pn-requester";

		case 2869:
			return L"icslap";

		case 3000:
			return L"ppp";

		case 3074:
			return L"xbox";

		case 3128:
			return L"squid-http";

		case 3306:
			return L"mysql";

		case 3389:
			return L"ms-wbt-server";

		case 3407:
			return L"ldap-admin";

		case 3540:
			return L"pnrp-port";

		case 3558:
			return L"mcp-port";

		case 3587:
			return L"p2pgroup";

		case 3702:
			return L"ws-discovery";

		case 3713:
			return L"tftps";

		case 3724:
			return L"blizwow";

		case 4500:
			return L"ipsec-nat-t";

		case 4554:
			return L"msfrs";

		case 4687:
			return L"nst";

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

		case 5190:
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

		case 5362:
			return L"serverwsd2";

		case 5432:
			return L"postgresql";

		case 5631:
			return L"pcanywheredata";

		case 5666:
			return L"nrpe";

		case 5687:
			return L"gog-multiplayer";

		case 5800:
			return L"vnc-http";

		case 5900:
			return L"vnc";

		case 5938:
			return L"teamviewer";

		case 6000:
		case 6001:
		case 6002:
		case 6003:
			return L"x11";

		case 6222:
		case 6662: // deprecated!
			return L"radmind";

		case 6346:
			return L"gnutella";

		case 6347:
			return L"gnutella2";

		case 6622:
			return L"mcftp";

		case 6665:
		case 6666:
		case 6667:
		case 6668:
		case 6669:
			return L"ircu";

		case 6881:
			return L"bittorrent-tracker";

		case 7070:
			return L"realserver";

		case 7235:
			return L"aspcoordination";

		case 8443:
			return L"https-alt";

		case 8021:
			return L"ftp-proxy";

		case 8333:
		case 18333:
			return L"bitcoin";

		case 591:
		case 8000:
		case 8008:
		case 8080:
		case 8444:
			return L"http-alt";

		case 8999:
			return L"bctp";

		case 9418:
			return L"git";

		case 9800:
			return L"webdav";

		case 10107:
			return L"bctp-server";

		case 11371:
		{
			if (proto == IPPROTO_UDP)
				return L"hkp";

			return L"pksd";
		}

		case 25565:
			return L"minecraft";

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
			return L"mongod";

		case 27500:
			return L"quakeworld";

		case 27910:
			return L"quake2";

		case 27960:
			return L"quake3";

		case 28240:
			return L"siemensgsm";

		case 33434:
			return L"traceroute";
	}

	return NULL;
}
