// simplewall
// Copyright (c) 2016-2026 Henry++
//
// Auto rules: one decision (allow or block) that applies to every
// application signed by a publisher, or living under a path pattern.
// This keeps a decision alive when an application moves to a new
// versioned folder after an update (Google Update, Teams, etc).

#pragma once

#include "global.h"

VOID NTAPI _app_dereferenceautorule (
	_In_ PVOID entry
);

VOID _app_autorules_initialize ();

VOID _app_autorules_clear ();

BOOLEAN _app_autorules_isempty ();

BOOLEAN _app_autorules_havepublisherrules ();

_Ret_maybenull_
PITEM_AUTORULE _app_autorules_add (
	_In_opt_ PR_STRING publisher,
	_In_opt_ PR_STRING path_pattern,
	_In_ BOOLEAN is_allow
);

BOOLEAN _app_autorules_remove (
	_In_ ULONG_PTR index
);

_Ret_maybenull_
PR_STRING _app_autorules_getdescription (
	_In_ PITEM_AUTORULE ptr_rule
);

_Ret_maybenull_
PR_STRING _app_autorules_makepathpattern (
	_In_ PR_STRING path
);

_Ret_maybenull_
PR_STRING _app_autorules_getappsignature (
	_In_ PITEM_APP ptr_app,
	_In_ BOOLEAN is_allowsync
);

_Success_ (return)
BOOLEAN _app_autorules_match (
	_In_ PITEM_APP ptr_app,
	_In_opt_ PR_STRING signature,
	_Out_ PBOOLEAN is_allow
);

BOOLEAN _app_autorules_applytoapp (
	_In_opt_ HWND hwnd,
	_In_ PITEM_APP ptr_app,
	_In_ BOOLEAN is_allowsync
);

ULONG _app_autorules_countpending (
	_In_ PITEM_AUTORULE ptr_rule,
	_In_ ULONG exclude_hash
);

VOID _app_autorules_generatemenu (
	_In_ HMENU hmenu,
	_In_ ULONG app_hash,
	_In_ BOOLEAN is_allow
);

BOOLEAN _app_autorules_createfromnotify (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
);

VOID _app_autorules_showmanager (
	_In_ HWND hwnd
);

VOID _app_autorules_removefrommenu (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
);
