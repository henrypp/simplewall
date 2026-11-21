// simplewall
// Copyright (c) 2016-2026 Henry++
//
// Auto rules: one decision (allow or block) that applies to every
// application signed by a publisher, or living under a path pattern.

#include "global.h"

VOID NTAPI _app_dereferenceautorule (
	_In_ PVOID entry
)
{
	PITEM_AUTORULE ptr_item = (PITEM_AUTORULE)entry;

	if (ptr_item->publisher)
		_r_obj_dereference (ptr_item->publisher);

	if (ptr_item->path_pattern)
		_r_obj_dereference (ptr_item->path_pattern);

	if (ptr_item->path_expanded)
		_r_obj_dereference (ptr_item->path_expanded);
}

VOID _app_autorules_initialize ()
{
	if (!autorules_list)
		autorules_list = _r_obj_createlist (0x04, &_r_obj_dereference);
}

// forget the "already checked" mark on every app, so the next blocked
// packet re-evaluates it against the current set of auto rules
static VOID _app_autorules_resetchecked ()
{
	PITEM_APP ptr_app = NULL;
	ULONG_PTR enum_key = 0;

	_r_queuedlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtablepointer (apps_table, (PVOID_PTR)&ptr_app, NULL, &enum_key))
	{
		ptr_app->is_autorulechecked = FALSE;
	}

	_r_queuedlock_releaseshared (&lock_apps);
}

VOID _app_autorules_clear ()
{
	_r_queuedlock_acquireexclusive (&lock_autorules);
	_r_obj_clearlist (autorules_list);
	_r_queuedlock_releaseexclusive (&lock_autorules);
}

BOOLEAN _app_autorules_isempty ()
{
	BOOLEAN is_empty;

	_r_queuedlock_acquireshared (&lock_autorules);
	is_empty = (_r_obj_getlistsize (autorules_list) == 0);
	_r_queuedlock_releaseshared (&lock_autorules);

	return is_empty;
}

BOOLEAN _app_autorules_havepublisherrules ()
{
	PITEM_AUTORULE ptr_rule;
	BOOLEAN is_found = FALSE;

	_r_queuedlock_acquireshared (&lock_autorules);

	for (ULONG_PTR i = 0; i < _r_obj_getlistsize (autorules_list); i++)
	{
		ptr_rule = (PITEM_AUTORULE)_r_obj_getlistitem (autorules_list, i);

		if (ptr_rule && !_r_obj_isstringempty (ptr_rule->publisher))
		{
			is_found = TRUE;
			break;
		}
	}

	_r_queuedlock_releaseshared (&lock_autorules);

	return is_found;
}

_Ret_maybenull_
PITEM_AUTORULE _app_autorules_add (
	_In_opt_ PR_STRING publisher,
	_In_opt_ PR_STRING path_pattern,
	_In_ BOOLEAN is_allow
)
{
	PITEM_AUTORULE ptr_rule;
	PR_STRING string;
	NTSTATUS status;

	if (_r_obj_isstringempty (publisher) && _r_obj_isstringempty (path_pattern))
		return NULL;

	// a rule is either a publisher rule or a path rule, never both
	if (!_r_obj_isstringempty (publisher))
		path_pattern = NULL;

	// do not add duplicates, just update the action
	_r_queuedlock_acquireexclusive (&lock_autorules);

	for (ULONG_PTR i = 0; i < _r_obj_getlistsize (autorules_list); i++)
	{
		ptr_rule = (PITEM_AUTORULE)_r_obj_getlistitem (autorules_list, i);

		if (!ptr_rule)
			continue;

		if (publisher && ptr_rule->publisher && _r_str_isequal (&ptr_rule->publisher->sr, &publisher->sr, TRUE))
		{
			ptr_rule->is_allow = is_allow;
			_r_queuedlock_releaseexclusive (&lock_autorules);

			_app_autorules_resetchecked ();

			return ptr_rule;
		}

		if (path_pattern && ptr_rule->path_pattern && _r_str_isequal (&ptr_rule->path_pattern->sr, &path_pattern->sr, TRUE))
		{
			ptr_rule->is_allow = is_allow;
			_r_queuedlock_releaseexclusive (&lock_autorules);

			_app_autorules_resetchecked ();

			return ptr_rule;
		}
	}

	ptr_rule = (PITEM_AUTORULE)_r_obj_allocate (sizeof (ITEM_AUTORULE), &_app_dereferenceautorule);

	ptr_rule->is_allow = is_allow;

	if (publisher)
	{
		ptr_rule->publisher = _r_obj_createstring2 (&publisher->sr);
	}
	else
	{
		ptr_rule->path_pattern = _r_obj_createstring2 (&path_pattern->sr);

		// expand environment variables only for matching, keep the saved form portable
		if (_r_str_findchar (&path_pattern->sr, L'%', FALSE) != SIZE_MAX)
		{
			status = _r_str_environmentexpandstring (&string, NULL, &path_pattern->sr);

			if (NT_SUCCESS (status))
				ptr_rule->path_expanded = string;
		}

		if (!ptr_rule->path_expanded)
			ptr_rule->path_expanded = _r_obj_createstring2 (&path_pattern->sr);
	}

	_r_obj_addlistitem (autorules_list, ptr_rule, NULL);

	_r_queuedlock_releaseexclusive (&lock_autorules);

	_app_autorules_resetchecked ();

	return ptr_rule;
}

BOOLEAN _app_autorules_remove (
	_In_ ULONG_PTR index
)
{
	PITEM_AUTORULE ptr_rule;
	PR_LIST new_list;
	BOOLEAN is_removed = FALSE;

	new_list = _r_obj_createlist (0x04, &_r_obj_dereference);

	_r_queuedlock_acquireexclusive (&lock_autorules);

	for (ULONG_PTR i = 0; i < _r_obj_getlistsize (autorules_list); i++)
	{
		ptr_rule = (PITEM_AUTORULE)_r_obj_getlistitem (autorules_list, i);

		if (!ptr_rule)
			continue;

		if (i == index)
		{
			is_removed = TRUE;
			continue;
		}

		_r_obj_addlistitem (new_list, _r_obj_reference (ptr_rule), NULL);
	}

	if (is_removed)
	{
		_r_obj_movereference ((PVOID_PTR)&autorules_list, new_list);
	}
	else
	{
		_r_obj_dereference (new_list);
	}

	_r_queuedlock_releaseexclusive (&lock_autorules);

	if (is_removed)
		_app_autorules_resetchecked ();

	return is_removed;
}

_Ret_maybenull_
PR_STRING _app_autorules_getdescription (
	_In_ PITEM_AUTORULE ptr_rule
)
{
	LPCWSTR action;

	action = _r_locale_getstring (ptr_rule->is_allow ? IDS_ACTION_ALLOW : IDS_ACTION_BLOCK);

	if (!_r_obj_isstringempty (ptr_rule->publisher))
		return _r_format_string (L"%s: %s \"%s\"", action, _r_locale_getstring (IDS_AUTORULE_KIND_PUBLISHER), ptr_rule->publisher->buffer);

	if (!_r_obj_isstringempty (ptr_rule->path_pattern))
		return _r_format_string (L"%s: %s %s", action, _r_locale_getstring (IDS_AUTORULE_KIND_PATH), ptr_rule->path_pattern->buffer);

	return NULL;
}

static BOOLEAN _app_autorules_isversionchar (
	_In_ WCHAR chr
)
{
	return (chr >= L'0' && chr <= L'9') || chr == L'.';
}

static BOOLEAN _app_autorules_isalnum (
	_In_ WCHAR chr
)
{
	return (chr >= L'0' && chr <= L'9') || (chr >= L'a' && chr <= L'z') || (chr >= L'A' && chr <= L'Z');
}

// Turns the folder of an executable into a pattern that survives updates by
// replacing version numbers in folder names with a wildcard, e.g.
//   C:\Program Files (x86)\Google\GoogleUpdater\140.0.7339.0\updater.exe
//     -> C:\Program Files (x86)\Google\GoogleUpdater\*
//   C:\Program Files\WindowsApps\MSTeams_25290.205.4069.4894_x64__8wekyb3d8bbwe\ms-teams.exe
//     -> C:\Program Files\WindowsApps\MSTeams_*_x64__8wekyb3d8bbwe\*
// Returns NULL when the folder has no version number (the exact path is then
// already handled by the normal per-app decision) or lives under Windows.
_Ret_maybenull_
PR_STRING _app_autorules_makepathpattern (
	_In_ PR_STRING path
)
{
	WCHAR buffer[0x400];
	R_STRINGREF dir_sr;
	ULONG_PTR dir_length, out_length = 0, last_slash = 0, i, j, run_end, dots;
	BOOLEAN is_changed = FALSE, is_lastwildcard = FALSE;

	if (_r_obj_isstringempty (path))
		return NULL;

	i = _r_str_findlastchar (&path->sr, OBJ_NAME_PATH_SEPARATOR, FALSE);

	if (i == SIZE_MAX || i < 3)
		return NULL;

	dir_length = i;

	// never generalize system folders
	_r_obj_initializestringref_ex (&dir_sr, path->buffer, dir_length * sizeof (WCHAR));

	if (config.windows_dir.length && _r_str_isstartswith (&dir_sr, &config.windows_dir, TRUE))
		return NULL;

	if (dir_length + 3 >= RTL_NUMBER_OF (buffer))
		return NULL;

	i = 0;

	while (i < dir_length)
	{
		WCHAR chr = path->buffer[i];

		if (chr == OBJ_NAME_PATH_SEPARATOR)
			last_slash = out_length;

		// a version starts with a digit that does not continue a word
		if (chr >= L'0' && chr <= L'9' && (i == 0 || !_app_autorules_isalnum (path->buffer[i - 1])))
		{
			j = i;

			while (j < dir_length && _app_autorules_isversionchar (path->buffer[j]))
				j += 1;

			run_end = j;

			// do not eat a trailing dot
			while (run_end > i && path->buffer[run_end - 1] == L'.')
				run_end -= 1;

			dots = 0;

			for (ULONG_PTR k = i; k < run_end; k++)
			{
				if (path->buffer[k] == L'.')
					dots += 1;
			}

			// at least "1.2" and not glued to a following word
			if (dots >= 1 && (run_end >= dir_length || !_app_autorules_isalnum (path->buffer[run_end])))
			{
				buffer[out_length++] = L'*';
				is_changed = TRUE;
				i = run_end;

				continue;
			}

			// not a version, copy as is
			while (i < run_end)
				buffer[out_length++] = path->buffer[i++];

			continue;
		}

		buffer[out_length++] = chr;
		i += 1;
	}

	if (!is_changed)
		return NULL;

	// if the last folder became just "*", it already matches every file below
	is_lastwildcard = (out_length >= 2 && buffer[out_length - 1] == L'*' && last_slash == out_length - 2);

	if (!is_lastwildcard)
	{
		buffer[out_length++] = OBJ_NAME_PATH_SEPARATOR;
		buffer[out_length++] = L'*';
	}

	buffer[out_length] = UNICODE_NULL;

	return _r_obj_createstring_ex (buffer, out_length * sizeof (WCHAR));
}

_Ret_maybenull_
PR_STRING _app_autorules_getappsignature (
	_In_ PITEM_APP ptr_app,
	_In_ BOOLEAN is_allowsync
)
{
	PITEM_APP_INFO ptr_app_info;
	PR_STRING string = NULL;
	HANDLE hfile;
	NTSTATUS status;

	if (ptr_app->type != DATA_APP_REGULAR || _r_obj_isstringempty (ptr_app->real_path))
		return NULL;

	// cached by the file information queue
	if (_app_getappinfoparam2 (ptr_app->app_hash, 0, INFO_SIGNATURE_STRING, &string, sizeof (PR_STRING)))
		return string;

	if (!is_allowsync || !_app_isappvalidbinary (ptr_app->real_path))
		return NULL;

	status = _r_fs_openfile (&hfile, &ptr_app->real_path->sr, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE, 0, FALSE);

	if (!NT_SUCCESS (status))
		return NULL;

	ptr_app_info = _app_getappinfobyhash2 (ptr_app->app_hash);

	if (ptr_app_info)
	{
		// store into cache, so the next lookup is free
		_app_getfilesignatureinfo (ptr_app_info, hfile);

		if (!_r_obj_isstringempty (ptr_app_info->signature_info))
			string = _r_obj_reference (ptr_app_info->signature_info);

		_r_obj_dereference (ptr_app_info);
	}
	else
	{
		string = _app_getfilesignaturestring (ptr_app->real_path->buffer, hfile);
	}

	NtClose (hfile);

	return string;
}

static BOOLEAN _app_autorules_matchrule (
	_In_ PITEM_AUTORULE ptr_rule,
	_In_ PR_STRING path,
	_In_opt_ PR_STRING signature
)
{
	if (!_r_obj_isstringempty (ptr_rule->publisher))
		return (signature && _r_str_isequal (&signature->sr, &ptr_rule->publisher->sr, TRUE));

	if (!_r_obj_isstringempty (ptr_rule->path_expanded))
		return _r_str_match (path->buffer, ptr_rule->path_expanded->buffer, TRUE);

	return FALSE;
}

_Success_ (return)
BOOLEAN _app_autorules_match (
	_In_ PITEM_APP ptr_app,
	_In_opt_ PR_STRING signature,
	_Out_ PBOOLEAN is_allow
)
{
	PITEM_AUTORULE ptr_rule;
	BOOLEAN is_found = FALSE;

	*is_allow = FALSE;

	if (_r_obj_isstringempty (ptr_app->real_path))
		return FALSE;

	_r_queuedlock_acquireshared (&lock_autorules);

	// path rules are more specific, so they win over publisher rules
	for (UINT pass = 0; pass < 2 && !is_found; pass++)
	{
		for (ULONG_PTR i = 0; i < _r_obj_getlistsize (autorules_list); i++)
		{
			ptr_rule = (PITEM_AUTORULE)_r_obj_getlistitem (autorules_list, i);

			if (!ptr_rule)
				continue;

			if ((pass == 0) != _r_obj_isstringempty (ptr_rule->publisher))
				continue;

			if (_app_autorules_matchrule (ptr_rule, ptr_app->real_path, signature))
			{
				*is_allow = ptr_rule->is_allow;
				is_found = TRUE;

				break;
			}
		}
	}

	_r_queuedlock_releaseshared (&lock_autorules);

	return is_found;
}

// Applies the matching auto rule to an app nobody decided about yet.
// Returns TRUE when a decision was made (the caller should save the profile).
BOOLEAN _app_autorules_applytoapp (
	_In_opt_ HWND hwnd,
	_In_ PITEM_APP ptr_app,
	_In_ BOOLEAN is_allowsync
)
{
	PR_STRING signature = NULL;
	PR_LIST rules;
	HWND hmain;
	BOOLEAN is_allow;

	UNREFERENCED_PARAMETER (hwnd);

	if (_app_autorules_isempty ())
		return FALSE;

	if (ptr_app->type != DATA_APP_REGULAR)
		return FALSE;

	// only apps without a decision: never override what the user chose
	if (ptr_app->is_enabled || ptr_app->is_silent || ptr_app->is_undeletable || _app_istimerset (ptr_app))
		return FALSE;

	// expensive signature checks happen once per app until the rules change
	if (ptr_app->is_autorulechecked)
		is_allowsync = FALSE;

	if (_app_autorules_havepublisherrules ())
		signature = _app_autorules_getappsignature (ptr_app, is_allowsync);

	if (is_allowsync)
		ptr_app->is_autorulechecked = TRUE;

	if (!_app_autorules_match (ptr_app, signature, &is_allow))
	{
		if (signature)
			_r_obj_dereference (signature);

		return FALSE;
	}

	if (signature)
		_r_obj_dereference (signature);

	ptr_app->is_enabled = is_allow;
	ptr_app->is_silent = !is_allow;
	ptr_app->last_notify = _r_unixtime_now ();

	if (_wfp_isfiltersinstalled ())
	{
		rules = _r_obj_createlist (0x02, NULL);

		_r_obj_addlistitem (rules, ptr_app, NULL);

		_wfp_createappfilters (_wfp_getenginehandle (), rules, DBG_ARG, FALSE);

		_r_obj_dereference (rules);
	}

	hmain = _r_app_gethwnd ();

	if (hmain)
	{
		_app_listview_updateitemby_param (hmain, ptr_app->app_hash, TRUE);
		_app_listview_updateby_id (hmain, ptr_app->type, PR_UPDATE_TYPE);
	}

	return TRUE;
}

// How many other apps are waiting in the notification queue and would be
// covered by this rule (uses cached signatures only, it runs on the UI thread).
ULONG _app_autorules_countpending (
	_In_ PITEM_AUTORULE ptr_rule,
	_In_ ULONG exclude_hash
)
{
	PITEM_APP ptr_app = NULL;
	PR_STRING signature;
	ULONG_PTR enum_key = 0;
	ULONG count = 0;

	_r_queuedlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtablepointer (apps_table, (PVOID_PTR)&ptr_app, NULL, &enum_key))
	{
		if (!ptr_app->notification || ptr_app->app_hash == exclude_hash)
			continue;

		if (ptr_app->type != DATA_APP_REGULAR || _r_obj_isstringempty (ptr_app->real_path))
			continue;

		signature = NULL;

		if (!_r_obj_isstringempty (ptr_rule->publisher))
			_app_getappinfoparam2 (ptr_app->app_hash, 0, INFO_SIGNATURE_STRING, &signature, sizeof (PR_STRING));

		if (_app_autorules_matchrule (ptr_rule, ptr_app->real_path, signature))
			count += 1;

		if (signature)
			_r_obj_dereference (signature);
	}

	_r_queuedlock_releaseshared (&lock_apps);

	return count;
}

static VOID _app_autorules_addmenuitem (
	_In_ HMENU hmenu,
	_In_ UINT item_id,
	_In_ PR_STRING text,
	_In_ ULONG pending
)
{
	PR_STRING string;

	if (pending)
	{
		string = _r_format_string (_r_locale_getstring (IDS_AUTORULE_PENDING), text->buffer, pending);

		_r_menu_additem (hmenu, item_id, string->buffer);

		_r_obj_dereference (string);
	}
	else
	{
		_r_menu_additem (hmenu, item_id, text->buffer);
	}
}

// Adds "everything from this publisher" / "everything in this folder" items to
// the Allow or Block dropdown of the notification window.
VOID _app_autorules_generatemenu (
	_In_ HMENU hmenu,
	_In_ ULONG app_hash,
	_In_ BOOLEAN is_allow
)
{
	ITEM_AUTORULE temp_rule = {0};
	PITEM_APP ptr_app;
	PR_STRING signature, pattern, text;
	UINT id_publisher, id_path;

	ptr_app = _app_getappitem (app_hash);

	if (!ptr_app)
		return;

	if (ptr_app->type != DATA_APP_REGULAR || _r_obj_isstringempty (ptr_app->real_path))
	{
		_r_obj_dereference (ptr_app);
		return;
	}

	id_publisher = is_allow ? IDM_AUTORULE_ALLOW_PUBLISHER : IDM_AUTORULE_BLOCK_PUBLISHER;
	id_path = is_allow ? IDM_AUTORULE_ALLOW_PATH : IDM_AUTORULE_BLOCK_PATH;

	if (is_allow && GetMenuItemCount (hmenu) > 0)
		_r_menu_addseparator (hmenu);

	// publisher
	signature = _app_autorules_getappsignature (ptr_app, TRUE);

	if (signature)
	{
		text = _r_format_string (_r_locale_getstring (is_allow ? IDS_AUTORULE_ALLOW_PUBLISHER : IDS_AUTORULE_BLOCK_PUBLISHER), signature->buffer);

		temp_rule.publisher = signature;

		_app_autorules_addmenuitem (hmenu, id_publisher, text, _app_autorules_countpending (&temp_rule, app_hash));

		temp_rule.publisher = NULL;

		_r_obj_dereference (text);
		_r_obj_dereference (signature);
	}
	else
	{
		_r_menu_additem (hmenu, id_publisher, _r_locale_getstring (IDS_AUTORULE_NOPUBLISHER));
		_r_menu_enableitem (hmenu, id_publisher, FALSE, FALSE);
	}

	// versioned folder
	pattern = _app_autorules_makepathpattern (ptr_app->real_path);

	if (pattern)
	{
		text = _r_format_string (_r_locale_getstring (is_allow ? IDS_AUTORULE_ALLOW_PATH : IDS_AUTORULE_BLOCK_PATH), pattern->buffer);

		temp_rule.path_expanded = pattern;

		_app_autorules_addmenuitem (hmenu, id_path, text, _app_autorules_countpending (&temp_rule, app_hash));

		temp_rule.path_expanded = NULL;

		_r_obj_dereference (text);
		_r_obj_dereference (pattern);
	}
	else
	{
		_r_menu_additem (hmenu, id_path, _r_locale_getstring (IDS_AUTORULE_NOPATH));
		_r_menu_enableitem (hmenu, id_path, FALSE, FALSE);
	}

	_r_obj_dereference (ptr_app);
}

// Creates an auto rule from the app shown in the notification window and
// applies it right away to that app, to every other pending notification it
// covers, and to every undecided app already in the list.
BOOLEAN _app_autorules_createfromnotify (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	PITEM_AUTORULE ptr_rule = NULL;
	PITEM_APP ptr_app, ptr_item = NULL;
	PR_STRING signature, pattern, unexpanded;
	PR_LIST apps;
	ULONG_PTR enum_key = 0;
	ULONG app_hash;
	BOOLEAN is_allow, is_publisher, is_changed = FALSE;

	is_allow = (ctrl_id == IDM_AUTORULE_ALLOW_PUBLISHER || ctrl_id == IDM_AUTORULE_ALLOW_PATH);
	is_publisher = (ctrl_id == IDM_AUTORULE_ALLOW_PUBLISHER || ctrl_id == IDM_AUTORULE_BLOCK_PUBLISHER);

	app_hash = _app_notify_getapp_id (hwnd);
	ptr_app = _app_getappitem (app_hash);

	if (!ptr_app)
		return FALSE;

	if (is_publisher)
	{
		signature = _app_autorules_getappsignature (ptr_app, TRUE);

		if (signature)
		{
			ptr_rule = _app_autorules_add (signature, NULL, is_allow);

			_r_obj_dereference (signature);
		}
	}
	else
	{
		pattern = _app_autorules_makepathpattern (ptr_app->real_path);

		if (pattern)
		{
			unexpanded = _r_str_environmentunexpandstring (&pattern->sr);

			ptr_rule = _app_autorules_add (NULL, unexpanded ? unexpanded : pattern, is_allow);

			if (unexpanded)
				_r_obj_dereference (unexpanded);

			_r_obj_dereference (pattern);
		}
	}

	_r_obj_dereference (ptr_app);

	if (!ptr_rule)
		return FALSE;

	// collect apps first, filters and listview updates must not run under the apps lock
	apps = _r_obj_createlist (0x10, &_r_obj_dereference);

	_r_queuedlock_acquireshared (&lock_apps);

	while (_r_obj_enumhashtablepointer (apps_table, (PVOID_PTR)&ptr_item, NULL, &enum_key))
	{
		if (ptr_item->type != DATA_APP_REGULAR)
			continue;

		if (ptr_item->is_enabled || ptr_item->is_silent || ptr_item->is_undeletable)
			continue;

		_r_obj_addlistitem (apps, _r_obj_reference (ptr_item), NULL);
	}

	_r_queuedlock_releaseshared (&lock_apps);

	for (ULONG_PTR i = 0; i < _r_obj_getlistsize (apps); i++)
	{
		ptr_item = (PITEM_APP)_r_obj_getlistitem (apps, i);

		if (!ptr_item)
			continue;

		// apps waiting for an answer get a real signature check, the rest use the cache
		if (_app_autorules_applytoapp (hwnd, ptr_item, (ptr_item->notification != NULL) || (ptr_item->app_hash == app_hash)))
		{
			if (ptr_item->notification)
				_r_obj_clearreference ((PVOID_PTR)&ptr_item->notification);

			is_changed = TRUE;
		}
	}

	_r_obj_dereference (apps);

	// show the next pending notification, or close the window
	if (_app_notify_getnextapp_id (hwnd) != 0)
	{
		_app_notify_refresh (hwnd);
	}
	else
	{
		DestroyWindow (hwnd);
	}

	_app_profile_save (NULL);

	return is_changed;
}

// "Edit > Auto rules": a popup with every rule, clicking one deletes it.
VOID _app_autorules_showmanager (
	_In_ HWND hwnd
)
{
	PITEM_AUTORULE ptr_rule;
	PR_STRING string;
	HMENU hmenu;
	ULONG_PTR count;

	hmenu = CreatePopupMenu ();

	if (!hmenu)
		return;

	_r_queuedlock_acquireshared (&lock_autorules);

	count = _r_obj_getlistsize (autorules_list);

	if (count == 0)
	{
		_r_menu_additem (hmenu, IDX_AUTORULES, _r_locale_getstring (IDS_AUTORULES_EMPTY));
		_r_menu_enableitem (hmenu, IDX_AUTORULES, FALSE, FALSE);
	}
	else
	{
		_r_menu_additem (hmenu, 0, _r_locale_getstring (IDS_AUTORULES_HINT));
		_r_menu_enableitem (hmenu, 0, TRUE, FALSE);
		_r_menu_addseparator (hmenu);

		for (ULONG_PTR i = 0; i < count && i < AUTORULES_MAX; i++)
		{
			ptr_rule = (PITEM_AUTORULE)_r_obj_getlistitem (autorules_list, i);

			if (!ptr_rule)
				continue;

			string = _app_autorules_getdescription (ptr_rule);

			if (string)
			{
				_r_menu_additem (hmenu, IDX_AUTORULES + (UINT)i, string->buffer);

				_r_obj_dereference (string);
			}
		}
	}

	_r_queuedlock_releaseshared (&lock_autorules);

	_r_menu_popup (hmenu, hwnd, NULL, 0);

	DestroyMenu (hmenu);
}

VOID _app_autorules_removefrommenu (
	_In_ HWND hwnd,
	_In_ INT ctrl_id
)
{
	ULONG_PTR index;

	if (ctrl_id < IDX_AUTORULES || ctrl_id >= IDX_AUTORULES + AUTORULES_MAX)
		return;

	index = (ULONG_PTR)(ctrl_id - IDX_AUTORULES);

	if (_r_show_message (hwnd, MB_YESNO | MB_ICONEXCLAMATION, NULL, _r_locale_getstring (IDS_QUESTION_DELETE)) != IDYES)
		return;

	if (_app_autorules_remove (index))
		_app_profile_save (hwnd);
}
