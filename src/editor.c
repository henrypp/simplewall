// simplewall
// Copyright (c) 2016-2025 Henry++

#include "global.h"

_Ret_maybenull_
PEDITOR_CONTEXT _app_editor_createwindow (
	_In_ HWND hwnd,
	_In_ PVOID lparam,
	_In_ INT page_id,
	_In_ BOOLEAN is_settorules
)
{
	PEDITOR_CONTEXT context;

	context = _r_mem_allocate (sizeof (EDITOR_CONTEXT));

	context->ptr_app = lparam;
	context->page_id = page_id;
	context->is_settorules = is_settorules;

	if (_r_wnd_createmodalwindow (_r_sys_getimagebase (), MAKEINTRESOURCE (IDD_EDITOR), hwnd, &EditorProc, context))
		return context;

	_app_editor_deletewindow (context);

	return NULL;
}

VOID _app_editor_deletewindow (
	_In_ PEDITOR_CONTEXT context
)
{
	_r_mem_free (context);
}

_Ret_maybenull_
PEDITOR_CONTEXT _app_editor_getcontext (
	_In_ HWND hwnd
)
{
	PEDITOR_CONTEXT context;

	context = _r_wnd_getcontext (hwnd, SHORT_MAX);

	return context;
}

VOID _app_editor_setcontext (
	_In_ HWND hwnd,
	_In_ PEDITOR_CONTEXT context
)
{
	_r_wnd_setcontext (hwnd, SHORT_MAX, context);
}

VOID _app_editor_addtabitem (
	_In_ HWND hwnd,
	_In_ UINT locale_id,
	_In_ INT dlg_id,
	_In_ PEDITOR_CONTEXT context
)
{
	HWND htab;

	htab = _r_wnd_createwindow (_r_sys_getimagebase (), MAKEINTRESOURCE (dlg_id), hwnd, &EditorPagesProc, context);

	if (!htab)
		return;

	_r_tab_additem (hwnd, IDC_TAB, INT_ERROR, _r_locale_getstring (locale_id), I_DEFAULT, (LPARAM)htab);

	_r_tab_adjustchild (hwnd, IDC_TAB, htab);

	BringWindowToTop (htab); // HACK!!!

	_r_wnd_sendmessage (htab, 0, RM_INITIALIZE, 0, 0);
}

VOID _app_editor_settabtitle (
	_In_ HWND hwnd,
	_In_ INT listview_id
)
{
	WCHAR buffer[128];
	HWND hparent;
	UINT locale_id;
	INT checked_count;
	INT tab_id;

	if (listview_id == IDC_RULE_APPS_ID)
	{
		locale_id = IDS_TAB_APPS;
		tab_id = 2;
	}
	else if (listview_id == IDC_APP_RULES_ID)
	{
		locale_id = IDS_TRAY_RULES;
		tab_id = 1;
	}
	else
	{
		return;
	}

	hparent = GetParent (hwnd);

	if (!hparent)
		return;

	checked_count = _r_listview_getitemcheckedcount (hwnd, listview_id);

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%s (%d)", _r_locale_getstring (locale_id), checked_count);

	_r_tab_setitem (hparent, IDC_TAB, tab_id, buffer, I_DEFAULT, I_DEFAULT);
}

_Ret_maybenull_
PR_STRING _app_editor_getrulesfromlistview (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ INT exclude_id
)
{
	R_STRINGREF divider_sr = PR_STRINGREF_INIT (DIVIDER_RULE);
	R_STRINGBUILDER sb;
	PR_STRING string;
	INT item_count;

	item_count = _r_listview_getitemcount (hwnd, ctrl_id);

	if (!item_count)
		return NULL;

	_r_obj_initializestringbuilder (&sb, 512);

	for (INT i = 0; i < item_count; i++)
	{
		if (i == exclude_id)
			continue;

		string = _r_listview_getitemtext (hwnd, ctrl_id, i, 0);

		if (string)
		{
			_r_str_trimstring2 (&string->sr, DIVIDER_RULE DIVIDER_TRIM, 0);

			if (!_r_obj_isstringempty2 (string))
			{
				// check maximum length of one rule
				if ((_r_str_getlength2 (&sb.string->sr) + _r_str_getlength2 (&string->sr)) > RULE_RULE_CCH_MAX)
				{
					_r_obj_dereference (string);

					break;
				}

				_r_obj_appendstringbuilder2 (&sb, &string->sr);
				_r_obj_appendstringbuilder2 (&sb, &divider_sr);
			}

			_r_obj_dereference (string);
		}
	}

	string = _r_obj_finalstringbuilder (&sb);

	_r_str_trimstring (&string->sr, &divider_sr, PR_TRIM_END_ONLY);

	if (_r_obj_isstringempty2 (string))
	{
		_r_obj_dereference (string);

		return NULL;
	}

	return string;
}

VOID _app_editor_setrulestolistview (
	_In_ HWND hwnd,
	_In_ INT ctrl_id,
	_In_ PR_STRING rule
)
{
	PR_STRING rule_string;
	R_STRINGREF first_part;
	R_STRINGREF remaining_part;
	INT item_id = 0;

	_r_obj_initializestringref2 (&remaining_part, &rule->sr);

	while (remaining_part.length != 0)
	{
		_r_str_splitatchar (&remaining_part, DIVIDER_RULE[0], &first_part, &remaining_part);

		rule_string = _r_obj_createstring2 (&first_part);

		_app_listview_lock (hwnd, ctrl_id, TRUE);
		_r_listview_additem (hwnd, ctrl_id, item_id, rule_string->buffer, I_DEFAULT, I_DEFAULT, I_DEFAULT);
		_app_listview_lock (hwnd, ctrl_id, FALSE);

		item_id += 1;

		_r_obj_dereference (rule_string);
	}

	_r_listview_setcolumn (hwnd, ctrl_id, 0, NULL, -100);
}

INT_PTR CALLBACK EditorRuleProc (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
)
{
	PEDITOR_CONTEXT context;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			PR_STRING string;

			context = (PEDITOR_CONTEXT)lparam;

			_app_editor_setcontext (hwnd, context);

			_r_wnd_center (hwnd, GetParent (hwnd));

			// localize window
			_r_ctrl_setstring (hwnd, 0, _r_locale_getstring (IDS_RULE));

			_r_edit_settextlimit (
				hwnd,
				IDC_RULE_ID,
				_r_calc_clamp (RULE_RULE_CCH_MAX - (LONG)(context->current_length) - 1, 1, RULE_RULE_CCH_MAX)
			);

			_r_edit_setcuebanner (hwnd, IDC_RULE_ID, L"Example: 192.168.0.1;192.168.0.17");

			if (context->item_id != INT_ERROR)
			{
				string = _r_listview_getitemtext (context->hwnd, context->listview_id, context->item_id, 0);

				if (string)
				{
					_r_ctrl_setstring (hwnd, IDC_RULE_ID, string->buffer);

					_r_obj_dereference (string);
				}
			}

			_r_ctrl_setstring (
				hwnd,
				IDC_RULE_HINT,
				L"eg. 192.168.0.1; 192.168.0.1; [fc00::]\r\neg. 192.168.0.1:80; 192.168.0.1:443; [fc00::]:443;\r\n"\
				L"eg. 192.168.0.1-192.168.0.255; 192.168.0.1-192.168.0.255;\r\neg. 192.168.0.1-192.168.0.255:80; 192.168.0.1-192.168.0.255:443;\r\n"\
				L"eg. 192.168.0.0/16; 192.168.0.0/24; fe80::/10;\r\n"\
				L"eg. 20-21; 49152-65534;\r\neg. 21; 80; 443\r\n"
			);

			_r_ctrl_setstring (
				hwnd,
				IDC_SAVE,
				_r_locale_getstring (context->item_id != INT_ERROR ? IDS_SAVE : IDS_ADD)
			);

			_r_ctrl_setstring (hwnd, IDC_CLOSE, _r_locale_getstring (IDS_CLOSE));

			// enable save button
			_r_ctrl_enable (hwnd, IDC_SAVE, _r_ctrl_getstringlength (hwnd, IDC_RULE_ID) != 0);

			_r_theme_initialize (hwnd);

			SetFocus (NULL);

			break;
		}

		case WM_CLOSE:
		{
			EndDialog (hwnd, FALSE);
			break;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc;

			hdc = BeginPaint (hwnd, &ps);

			if (!hdc)
				break;

			_r_dc_drawwindow (hdc, hwnd, TRUE);

			EndPaint (hwnd, &ps);

			break;
		}

		case WM_DPICHANGED:
		{
			_r_wnd_message_dpichanged (hwnd, wparam, lparam);
			break;
		}

		case WM_SETTINGCHANGE:
		{
			_r_wnd_message_settingchange (hwnd, wparam, lparam);
			break;
		}

		case WM_COMMAND:
		{
			INT ctrl_id = LOWORD (wparam);
			INT notify_code = HIWORD (wparam);

			if (notify_code == EN_CHANGE)
			{
				// enable save button
				_r_ctrl_enable (hwnd, IDC_SAVE, _r_ctrl_getstringlength (hwnd, IDC_RULE_ID) != 0);

				return FALSE;
			}
			else if (notify_code == EN_MAXTEXT)
			{
				_r_ctrl_showballoontip (hwnd, ctrl_id, 0, NULL, _r_locale_getstring (IDS_LIMIT_REACHED));

				return FALSE;
			}

			switch (ctrl_id)
			{
				case IDOK: // process Enter key
				case IDC_SAVE:
				{
					ITEM_ADDRESS address;
					R_STRINGREF remaining_part;
					R_STRINGREF first_part;
					WCHAR rule_string[256];
					PR_STRING string;
					INT item_id;

					if (!_r_ctrl_isenabled (hwnd, IDC_SAVE))
						return FALSE;

					string = _r_ctrl_getstring (hwnd, IDC_RULE_ID);

					if (!string)
						return FALSE;

					_r_str_trimstring2 (&string->sr, DIVIDER_TRIM DIVIDER_RULE, 0);

					if (_r_obj_isstringempty2 (string))
					{
						_r_ctrl_showballoontip (hwnd, IDC_RULE_ID, 0, NULL, _r_locale_getstring (IDS_STATUS_EMPTY));
						_r_ctrl_enable (hwnd, IDC_SAVE, FALSE);

						_r_obj_dereference (string);

						return FALSE;
					}

					context = _app_editor_getcontext (hwnd);

					if (!context)
						return FALSE;

					_r_obj_initializestringref2 (&remaining_part, &string->sr);

					item_id = _r_listview_getitemcount (context->hwnd, context->listview_id);

					while (remaining_part.length != 0)
					{
						_r_str_splitatchar (&remaining_part, DIVIDER_RULE[0], &first_part, &remaining_part);

						if (!_app_parserulestring (&first_part, &address))
						{
							_r_ctrl_showballoontip (hwnd, IDC_RULE_ID, 0, NULL, _r_locale_getstring (IDS_STATUS_SYNTAX_ERROR));

							_r_ctrl_enable (hwnd, IDC_SAVE, FALSE);

							return FALSE;
						}

						_r_str_copystring (rule_string, RTL_NUMBER_OF (rule_string), &first_part);

						_r_listview_additem (context->hwnd, context->listview_id, item_id, rule_string, I_DEFAULT, I_DEFAULT, I_DEFAULT);

						item_id += 1;
					}

					if (context->item_id != INT_ERROR)
						_r_listview_deleteitem (context->hwnd, context->listview_id, context->item_id);

					_r_listview_setcolumn (context->hwnd, context->listview_id, 0, NULL, -100);

					_r_obj_dereference (string);

					FALLTHROUGH;
				}

				case IDCANCEL: // process Esc key
				case IDC_CLOSE:
				{
					EndDialog (hwnd, FALSE);
					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

INT_PTR CALLBACK EditorPagesProc (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
)
{
	PEDITOR_CONTEXT context;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			WCHAR buffer[256];
			PITEM_APP ptr_app = NULL;
			PITEM_RULE ptr_rule;
			PR_STRING string;
			HWND hctrl;
			INT i;
			ULONG_PTR enum_key;
			BOOLEAN is_enabled;

			context = (PEDITOR_CONTEXT)lparam;

			_app_editor_setcontext (hwnd, context);

			EnableThemeDialogTexture (hwnd, ETDT_ENABLETAB);

			SetWindowPos (
				hwnd,
				HWND_TOP,
				0,
				0,
				0,
				0,
				SWP_NOSIZE | SWP_NOMOVE | SWP_FRAMECHANGED
			);

			// name
			if (GetDlgItem (hwnd, IDC_RULE_NAME_ID))
			{
				_r_ctrl_setstringformat (hwnd, IDC_RULE_NAME, L"%s:", _r_locale_getstring (IDS_NAME));

				if (!_r_obj_isstringempty (context->ptr_rule->name))
					_r_ctrl_setstring (hwnd, IDC_RULE_NAME_ID, context->ptr_rule->name->buffer);

				_r_ctrl_setreadonly (hwnd, IDC_RULE_NAME_ID, !!context->ptr_rule->is_readonly);
				_r_ctrl_settextlimit (hwnd, IDC_RULE_NAME_ID, RULE_NAME_CCH_MAX - 1);
			}

			// comment
			if (GetDlgItem (hwnd, IDC_RULE_COMMENT_ID))
			{
				_r_ctrl_setstringformat (hwnd, IDC_RULE_COMMENT, L"%s:", _r_locale_getstring (IDS_COMMENT));

				if (!_r_obj_isstringempty (context->ptr_rule->comment))
					_r_ctrl_setstring (hwnd, IDC_RULE_COMMENT_ID, context->ptr_rule->comment->buffer);

				_r_ctrl_enable (hwnd, IDC_RULE_COMMENT_ID, !context->ptr_rule->is_readonly);
				_r_ctrl_settextlimit (hwnd, IDC_RULE_COMMENT_ID, RULE_RULE_CCH_MAX - 1);
			}

			// direction
			if (GetDlgItem (hwnd, IDC_RULE_DIRECTION))
			{
				_r_ctrl_setstringformat (hwnd, IDC_RULE_DIRECTION, L"%s:", _r_locale_getstring (IDS_DIRECTION));

				_r_combobox_insertitem (hwnd, IDC_RULE_DIRECTION_ID, 0, _r_locale_getstring (IDS_DIRECTION_1), (LPARAM)FWP_DIRECTION_OUTBOUND);
				_r_combobox_insertitem (hwnd, IDC_RULE_DIRECTION_ID, 1, _r_locale_getstring (IDS_DIRECTION_2), (LPARAM)FWP_DIRECTION_INBOUND);
				_r_combobox_insertitem (hwnd, IDC_RULE_DIRECTION_ID, 2, _r_locale_getstring (IDS_ANY), (LPARAM)FWP_DIRECTION_MAX);

				_r_combobox_setcurrentitembylparam (hwnd, IDC_RULE_DIRECTION_ID, (LPARAM)context->ptr_rule->direction);

				_r_ctrl_enable (hwnd, IDC_RULE_DIRECTION_ID, !context->ptr_rule->is_readonly);
			}

			// action
			if (GetDlgItem (hwnd, IDC_RULE_ACTION))
			{
				_r_ctrl_setstringformat (hwnd, IDC_RULE_ACTION, L"%s:", _r_locale_getstring (IDS_ACTION));

				_r_combobox_insertitem (hwnd, IDC_RULE_ACTION_ID, 0, _r_locale_getstring (IDS_ACTION_BLOCK), (LPARAM)FWP_ACTION_BLOCK);
				_r_combobox_insertitem (hwnd, IDC_RULE_ACTION_ID, 1, _r_locale_getstring (IDS_ACTION_ALLOW), (LPARAM)FWP_ACTION_PERMIT);

				_r_combobox_setcurrentitembylparam (hwnd, IDC_RULE_ACTION_ID, (LPARAM)context->ptr_rule->action);

				_r_ctrl_enable (hwnd, IDC_RULE_ACTION_ID, !context->ptr_rule->is_readonly);
			}

			// protocols
			if (GetDlgItem (hwnd, IDC_RULE_PROTOCOL_ID))
			{
				UINT8 protos[] = {
					IPPROTO_ICMP,
					IPPROTO_IGMP,
					IPPROTO_IPV4,
					IPPROTO_TCP,
					IPPROTO_UDP,
					IPPROTO_RDP,
					IPPROTO_IPV6,
					IPPROTO_ICMPV6,
					IPPROTO_L2TP,
					IPPROTO_SCTP,
				};

				_r_ctrl_setstringformat (hwnd, IDC_RULE_PROTOCOL, L"%s:", _r_locale_getstring (IDS_PROTOCOL));

				_r_combobox_insertitem (hwnd, IDC_RULE_PROTOCOL_ID, 0, _r_locale_getstring (IDS_ANY), 0);

				for (i = 0; i < RTL_NUMBER_OF (protos); i++)
				{
					string = _app_db_getprotoname (protos[i], AF_UNSPEC, TRUE);

					_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%s (%" TEXT (PRIu8) L")", string->buffer, protos[i]);

					_r_combobox_insertitem (hwnd, IDC_RULE_PROTOCOL_ID, i + 1, buffer, (LPARAM)protos[i]);

					_r_obj_dereference (string);
				}

				// unknown protocol
				if (_r_combobox_getcurrentitem (hwnd, IDC_RULE_PROTOCOL_ID) == CB_ERR)
				{
					string = _app_db_getprotoname (context->ptr_rule->protocol, AF_UNSPEC, TRUE);

					_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%s (%" TEXT (PRIu8) L")", string->buffer, context->ptr_rule->protocol);

					_r_combobox_insertitem (hwnd, IDC_RULE_PROTOCOL_ID, i + 1, buffer, (LPARAM)context->ptr_rule->protocol);

					_r_obj_dereference (string);
				}

				_r_combobox_setcurrentitembylparam (hwnd, IDC_RULE_PROTOCOL_ID, (LPARAM)context->ptr_rule->protocol);

				_r_ctrl_enable (hwnd, IDC_RULE_PROTOCOL_ID, !context->ptr_rule->is_readonly);
			}

			// family (ports-only)
			if (GetDlgItem (hwnd, IDC_RULE_VERSION_ID))
			{
				_r_ctrl_setstringformat (hwnd, IDC_RULE_VERSION, L"%s:", _r_locale_getstring (IDS_PORTVERSION));

				_r_combobox_insertitem (hwnd, IDC_RULE_VERSION_ID, 0, _r_locale_getstring (IDS_ANY), (LPARAM)AF_UNSPEC);
				_r_combobox_insertitem (hwnd, IDC_RULE_VERSION_ID, 1, L"IPv4", (LPARAM)AF_INET);
				_r_combobox_insertitem (hwnd, IDC_RULE_VERSION_ID, 2, L"IPv6", (LPARAM)AF_INET6);

				_r_combobox_setcurrentitembylparam (hwnd, IDC_RULE_VERSION_ID, (LPARAM)context->ptr_rule->af);

				_r_ctrl_enable (hwnd, IDC_RULE_VERSION_ID, !context->ptr_rule->is_readonly);
			}

			// rule (remote)
			if (GetDlgItem (hwnd, IDC_RULE_REMOTE_ID))
			{
				_r_ctrl_setstringformat (hwnd, IDC_RULE_REMOTE, L"%s (%s):", _r_locale_getstring (IDS_RULE), _r_locale_getstring (IDS_DIRECTION_REMOTE));

				_r_listview_setstyle (
					hwnd,
					IDC_RULE_REMOTE_ID,
					LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP,
					FALSE
				);

				_r_listview_addcolumn (hwnd, IDC_RULE_REMOTE_ID, 0, NULL, -100, 0);

				if (!_r_obj_isstringempty (context->ptr_rule->rule_remote))
				{
					_app_editor_setrulestolistview (hwnd, IDC_RULE_REMOTE_ID, context->ptr_rule->rule_remote);

					_r_listview_setcolumn (hwnd, IDC_RULE_REMOTE_ID, 0, NULL, -100);
				}

				_r_ctrl_setstringformat (hwnd, IDC_RULE_REMOTE_ADD, L"%s...", _r_locale_getstring (IDS_ADD));
				_r_ctrl_setstringformat (hwnd, IDC_RULE_REMOTE_EDIT, L"%s...", _r_locale_getstring (IDS_EDIT2));
				_r_ctrl_setstringformat (hwnd, IDC_RULE_REMOTE_DELETE, L"%s...", _r_locale_getstring (IDS_DELETE));

				_r_ctrl_enable (hwnd, IDC_RULE_REMOTE_ADD, !context->ptr_rule->is_readonly);
				_r_ctrl_enable (hwnd, IDC_RULE_REMOTE_EDIT, FALSE);
				_r_ctrl_enable (hwnd, IDC_RULE_REMOTE_DELETE, FALSE);
			}

			// rule (local)
			if (GetDlgItem (hwnd, IDC_RULE_LOCAL_ID))
			{
				_r_ctrl_setstringformat (hwnd, IDC_RULE_LOCAL, L"%s (%s):", _r_locale_getstring (IDS_RULE), _r_locale_getstring (IDS_DIRECTION_LOCAL));

				_r_listview_setstyle (
					hwnd,
					IDC_RULE_LOCAL_ID,
					LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP,
					FALSE
				);

				_r_listview_addcolumn (hwnd, IDC_RULE_LOCAL_ID, 0, NULL, 100, 0);

				if (!_r_obj_isstringempty (context->ptr_rule->rule_local))
				{
					_app_editor_setrulestolistview (hwnd, IDC_RULE_LOCAL_ID, context->ptr_rule->rule_local);

					_r_listview_setcolumn (hwnd, IDC_RULE_LOCAL_ID, 0, NULL, -100);
				}

				_r_ctrl_setstringformat (hwnd, IDC_RULE_LOCAL_ADD, L"%s...", _r_locale_getstring (IDS_ADD));
				_r_ctrl_setstringformat (hwnd, IDC_RULE_LOCAL_EDIT, L"%s...", _r_locale_getstring (IDS_EDIT2));
				_r_ctrl_setstringformat (hwnd, IDC_RULE_LOCAL_DELETE, L"%s...", _r_locale_getstring (IDS_DELETE));

				_r_ctrl_enable (hwnd, IDC_RULE_LOCAL_ADD, !context->ptr_rule->is_readonly);
				_r_ctrl_enable (hwnd, IDC_RULE_LOCAL_EDIT, FALSE);
				_r_ctrl_enable (hwnd, IDC_RULE_LOCAL_DELETE, FALSE);
			}

			// search
			hctrl = GetDlgItem (hwnd, IDC_SEARCH);

			if (hctrl)
			{
				_app_search_create (hctrl);

				SetWindowPos (hctrl, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_FRAMECHANGED); // HACK!!!
			}

			// apps
			if (GetDlgItem (hwnd, IDC_RULE_APPS_ID))
			{
				_app_listview_setview (hwnd, IDC_RULE_APPS_ID);

				_r_listview_setstyle (
					hwnd,
					IDC_RULE_APPS_ID,
					LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_CHECKBOXES,
					TRUE
				);

				_r_listview_addcolumn (hwnd, IDC_RULE_APPS_ID, 0, _r_locale_getstring (IDS_NAME), 100, LVCFMT_LEFT);

				_r_listview_addgroup (hwnd, IDC_RULE_APPS_ID, 0, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
				_r_listview_addgroup (hwnd, IDC_RULE_APPS_ID, 1, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
				_r_listview_addgroup (hwnd, IDC_RULE_APPS_ID, 2, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);

				_r_listview_addgroup (hwnd, IDC_RULE_APPS_ID, LV_HIDDEN_GROUP_ID, L"", 3, LVGS_HIDDEN, LVGS_HIDDEN);

				// apps (apply to)
				enum_key = 0;

				_r_queuedlock_acquireshared (&lock_apps);

				while (_r_obj_enumhashtablepointer (apps_table, (PVOID_PTR)&ptr_app, NULL, &enum_key))
				{
					// check for services
					is_enabled = ((_r_obj_findhashtable (context->ptr_rule->apps, ptr_app->app_hash)) || (context->ptr_rule->is_forservices && _app_issystemhash (ptr_app->app_hash)));

					_app_listview_lock (hwnd, IDC_RULE_APPS_ID, TRUE);

					_r_listview_additem (hwnd, IDC_RULE_APPS_ID, 0, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK, I_GROUPIDCALLBACK, _app_listview_createcontext (ptr_app->app_hash));

					_r_listview_setitemcheck (hwnd, IDC_RULE_APPS_ID, 0, is_enabled);

					_app_listview_lock (hwnd, IDC_RULE_APPS_ID, FALSE);
				}

				_r_queuedlock_releaseshared (&lock_apps);

				// resize column
				_r_listview_setcolumn (hwnd, IDC_RULE_APPS_ID, 0, NULL, -100);

				// localize groups
				_app_listview_refreshgroups (hwnd, IDC_RULE_APPS_ID);

				// sort column
				_app_listview_sort (hwnd, IDC_RULE_APPS_ID, INT_ERROR, FALSE);
			}

			// app group
			if (GetDlgItem (hwnd, IDC_APP_NAME))
				_r_ctrl_setstringformat (hwnd, IDC_APP_NAME, L"%s:", _r_locale_getstring (IDS_SETTINGS_GENERAL));

			// app path
			if (GetDlgItem (hwnd, IDC_APP_PATH))
				_r_ctrl_setstringformat (hwnd, IDC_APP_PATH, L"%s:", _r_locale_getstring (IDS_FILEPATH));

			// app comment
			if (GetDlgItem (hwnd, IDC_APP_COMMENT))
				_r_ctrl_setstringformat (hwnd, IDC_APP_COMMENT, L"%s:", _r_locale_getstring (IDS_COMMENT));

			// app icon
			if (GetDlgItem (hwnd, IDC_APP_ICON_ID))
			{
				context->hicon = _app_icons_getsafeapp_hicon (context->ptr_app->app_hash);

				_r_wnd_sendmessage (hwnd, IDC_APP_ICON_ID, STM_SETICON, (WPARAM)context->hicon, 0);
			}

			// app display name
			if (GetDlgItem (hwnd, IDC_APP_NAME_ID))
			{
				string = _app_getappdisplayname (context->ptr_app, TRUE);

				if (string)
				{
					_r_ctrl_setstring (hwnd, IDC_APP_NAME_ID, string->buffer);

					_r_obj_dereference (string);
				}

				_r_ctrl_settextmargin (hwnd, IDC_APP_NAME_ID, 0, 0);
			}

			// app comment
			if (GetDlgItem (hwnd, IDC_APP_COMMENT_ID))
			{
				_r_ctrl_setstring (hwnd, IDC_APP_COMMENT_ID, _r_obj_getstring (context->ptr_app->comment));

				_r_ctrl_settextmargin (hwnd, IDC_APP_COMMENT_ID, 0, 0);
			}

			// app signature
			if (GetDlgItem (hwnd, IDC_APP_SIGNATURE_ID))
			{
				string = NULL;

				_app_getappinfoparam2 (context->ptr_app->app_hash, 0, INFO_SIGNATURE_STRING, &string, sizeof (PR_STRING));

				_r_ctrl_setstringformat (
					hwnd,
					IDC_APP_SIGNATURE_ID,
					L"%s: %s",
					_r_locale_getstring (IDS_SIGNATURE),
					_r_obj_getstringordefault (string, _r_locale_getstring (IDS_SIGN_UNSIGNED))
				);

				_r_ctrl_settextmargin (hwnd, IDC_APP_NAME_ID, 0, 0);

				_r_ctrl_settextmargin (hwnd, IDC_APP_SIGNATURE_ID, 0, 0);

				if (string)
					_r_obj_dereference (string);
			}

			// app path
			if (GetDlgItem (hwnd, IDC_APP_PATH_ID))
			{
				if (_app_isappvalidpath (context->ptr_app->real_path))
				{
					_r_ctrl_setstring (hwnd, IDC_APP_PATH_ID, context->ptr_app->real_path->buffer);
				}
				else
				{
					_r_ctrl_enable (hwnd, IDC_APP_EXPLORE_ID, FALSE);
				}
			}

			// app hash
			if (GetDlgItem (hwnd, IDC_APP_HASH_ID))
				_r_ctrl_setstring (hwnd, IDC_APP_HASH_ID, _r_obj_getstringordefault (context->ptr_app->hash, L"<empty>"));

			// app rules
			if (GetDlgItem (hwnd, IDC_APP_RULES_ID))
			{
				// configure listview
				_app_listview_setview (hwnd, IDC_APP_RULES_ID);

				_r_listview_setstyle (
					hwnd,
					IDC_APP_RULES_ID,
					LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_CHECKBOXES,
					TRUE
				);

				_r_listview_addcolumn (hwnd, IDC_APP_RULES_ID, 0, _r_locale_getstring (IDS_NAME), 100, LVCFMT_LEFT);

				_r_listview_addgroup (hwnd, IDC_APP_RULES_ID, 0, _r_locale_getstring (IDS_TRAY_SYSTEM_RULES), 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
				_r_listview_addgroup (hwnd, IDC_APP_RULES_ID, 1, _r_locale_getstring (IDS_TRAY_USER_RULES), 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
				_r_listview_addgroup (hwnd, IDC_APP_RULES_ID, LV_HIDDEN_GROUP_ID, L"", 2, LVGS_COLLAPSED, LVGS_COLLAPSED);

				// initialize
				_r_queuedlock_acquireshared (&lock_rules);

				for (ULONG_PTR i = 0; i < _r_obj_getlistsize (rules_list); i++)
				{
					ptr_rule = _r_obj_getlistitem (rules_list, i);

					if (!ptr_rule || ptr_rule->type != DATA_RULE_USER)
						continue;

					// check for services
					is_enabled = ((ptr_rule->is_forservices && _app_issystemhash (context->ptr_app->app_hash)) || _r_obj_findhashtable (ptr_rule->apps, context->ptr_app->app_hash));

					_app_listview_lock (hwnd, IDC_APP_RULES_ID, TRUE);

					_r_listview_additem (hwnd, IDC_APP_RULES_ID, 0, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK, I_GROUPIDCALLBACK, _app_listview_createcontext ((ULONG)i));

					_r_listview_setitemcheck (hwnd, IDC_APP_RULES_ID, 0, is_enabled);

					_app_listview_lock (hwnd, IDC_APP_RULES_ID, FALSE);
				}

				_r_queuedlock_releaseshared (&lock_rules);

				// resize column
				_r_listview_setcolumn (hwnd, IDC_APP_RULES_ID, 0, NULL, -100);

				// localize groups
				_app_listview_refreshgroups (hwnd, IDC_APP_RULES_ID);

				// sort column
				_app_listview_sort (hwnd, IDC_APP_RULES_ID, INT_ERROR, FALSE);
			}

			// hints
			if (GetDlgItem (hwnd, IDC_RULE_HINT))
				_r_ctrl_setstring (hwnd, IDC_RULE_HINT, _r_locale_getstring (IDS_RULE_HINT));

			if (GetDlgItem (hwnd, IDC_RULE_APPS_HINT))
				_r_ctrl_setstring (hwnd, IDC_RULE_APPS_HINT, _r_locale_getstring (IDS_RULE_APPS_HINT));

			if (GetDlgItem (hwnd, IDC_APP_HINT))
				_r_ctrl_setstring (hwnd, IDC_APP_HINT, _r_locale_getstring (IDS_APP_HINT));

			break;
		}

		case RM_INITIALIZE:
		{
			// apps
			if (GetDlgItem (hwnd, IDC_RULE_APPS_ID))
				_app_editor_settabtitle (hwnd, IDC_RULE_APPS_ID);

			// rules
			if (GetDlgItem (hwnd, IDC_APP_RULES_ID))
				_app_editor_settabtitle (hwnd, IDC_APP_RULES_ID);

			break;
		}

		case WM_DESTROY:
		{
			// apps
			if (GetDlgItem (hwnd, IDC_RULE_APPS_ID))
				_r_listview_deleteallitems (hwnd, IDC_RULE_APPS_ID);

			// app rules
			if (GetDlgItem (hwnd, IDC_APP_RULES_ID))
				_r_listview_deleteallitems (hwnd, IDC_APP_RULES_ID);

			context = _app_editor_getcontext (hwnd);

			if (!context)
				break;

			if (context->hicon)
				DestroyIcon (context->hicon);

			break;
		}

		case WM_SIZE:
		{
			INT listview_ids[] = {
				IDC_RULE_APPS_ID,
				IDC_APP_RULES_ID,
				IDC_RULE_REMOTE_ID,
				IDC_RULE_LOCAL_ID,
			};

			HWND hlistview;

			for (ULONG_PTR i = 0; i < RTL_NUMBER_OF (listview_ids); i++)
			{
				hlistview = GetDlgItem (hwnd, listview_ids[i]);

				if (hlistview)
					_r_listview_setcolumn (hwnd, listview_ids[i], 0, NULL, -100);
			}

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lparam;

			switch (nmlp->code)
			{
				case NM_CUSTOMDRAW:
				{
					LONG_PTR result;

					result = _app_message_custdraw (hwnd, (LPNMLVCUSTOMDRAW)lparam);

					SetWindowLongPtrW (hwnd, DWLP_MSGRESULT, result);

					return result;
				}

				case NM_DBLCLK:
				{
					LPNMITEMACTIVATE lpnmlv;
					ULONG command_id = 0;
					INT listview_id;

					lpnmlv = (LPNMITEMACTIVATE)lparam;

					if (lpnmlv->iItem == INT_ERROR)
						break;

					listview_id = (INT)(INT_PTR)(lpnmlv->hdr.idFrom);

					if (listview_id == IDC_RULE_REMOTE_ID)
					{
						command_id = IDC_RULE_REMOTE_EDIT;
					}
					else if (listview_id == IDC_RULE_LOCAL_ID)
					{
						command_id = IDC_RULE_LOCAL_EDIT;
					}
					else if (listview_id == IDC_RULE_APPS_ID || listview_id == IDC_APP_RULES_ID)
					{
						command_id = IDM_PROPERTIES;
					}

					if (command_id)
						_r_ctrl_sendcommand (hwnd, command_id, 0);

					break;
				}

				case NM_RCLICK:
				{
					LPNMITEMACTIVATE lpnmlv;
					HMENU hsubmenu;
					PR_STRING localized_string = NULL;
					UINT id_add;
					UINT id_edit;
					UINT id_delete;
					INT listview_id;
					BOOLEAN is_remote;
					BOOLEAN is_selected;

					lpnmlv = (LPNMITEMACTIVATE)lparam;
					listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					if (listview_id != IDC_RULE_REMOTE_ID && listview_id != IDC_RULE_LOCAL_ID && listview_id != IDC_RULE_APPS_ID && listview_id != IDC_APP_RULES_ID)
						break;

					context = _app_editor_getcontext (hwnd);

					if (!context)
						break;

					hsubmenu = CreatePopupMenu ();

					if (!hsubmenu)
						break;

					is_selected = _r_listview_getselectedcount (hwnd, listview_id) != 0;

					if (listview_id == IDC_RULE_REMOTE_ID || listview_id == IDC_RULE_LOCAL_ID)
					{
						is_remote = (listview_id == IDC_RULE_REMOTE_ID);

						id_add = is_remote ? IDC_RULE_REMOTE_ADD : IDC_RULE_LOCAL_ADD;
						id_edit = is_remote ? IDC_RULE_REMOTE_EDIT : IDC_RULE_LOCAL_EDIT;
						id_delete = is_remote ? IDC_RULE_REMOTE_DELETE : IDC_RULE_LOCAL_DELETE;

						_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_ADD), L"..."));
						_r_menu_additem (hsubmenu, id_add, localized_string->buffer);

						_r_obj_movereference ((PVOID_PTR)&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_EDIT2), L"..."));
						_r_menu_additem (hsubmenu, id_edit, localized_string->buffer);

						_r_menu_additem (hsubmenu, id_delete, _r_locale_getstring (IDS_DELETE));
						_r_menu_addseparator (hsubmenu);
						_r_menu_additem (hsubmenu, IDM_COPY, _r_locale_getstring (IDS_COPY));

						if (context->ptr_rule->is_readonly)
						{
							_r_menu_enableitem (hsubmenu, id_add, FALSE, FALSE);
							_r_menu_enableitem (hsubmenu, id_edit, FALSE, FALSE);
							_r_menu_enableitem (hsubmenu, id_delete, FALSE, FALSE);
						}

						if (!is_selected)
						{
							_r_menu_enableitem (hsubmenu, id_edit, FALSE, FALSE);
							_r_menu_enableitem (hsubmenu, id_delete, FALSE, FALSE);
							_r_menu_enableitem (hsubmenu, IDM_COPY, FALSE, FALSE);
						}

						_r_obj_dereference (localized_string);
					}
					else if (listview_id == IDC_RULE_APPS_ID || listview_id == IDC_APP_RULES_ID)
					{
						_r_menu_additem (hsubmenu, IDM_CHECK, _r_locale_getstring (IDS_CHECK));
						_r_menu_additem (hsubmenu, IDM_UNCHECK, _r_locale_getstring (IDS_UNCHECK));
						_r_menu_addseparator (hsubmenu);
						_r_menu_additem (hsubmenu, IDM_COPY, _r_locale_getstring (IDS_COPY));

						if (context->is_settorules && context->ptr_rule->type != DATA_RULE_USER)
						{
							_r_menu_enableitem (hsubmenu, IDM_CHECK, FALSE, FALSE);
							_r_menu_enableitem (hsubmenu, IDM_UNCHECK, FALSE, FALSE);
						}

						if (!is_selected)
						{
							_r_menu_enableitem (hsubmenu, IDM_CHECK, FALSE, FALSE);
							_r_menu_enableitem (hsubmenu, IDM_UNCHECK, FALSE, FALSE);
							_r_menu_enableitem (hsubmenu, IDM_COPY, FALSE, FALSE);
						}
					}

					_r_menu_popup (hsubmenu, hwnd, NULL, TRUE);

					DestroyMenu (hsubmenu);

					break;
				}

				case LVN_DELETEITEM:
				{
					PITEM_LISTVIEW_CONTEXT listview_context;
					LPNMLISTVIEW lpnmlv;
					INT listview_id;

					lpnmlv = (LPNMLISTVIEW)lparam;
					listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					if (!(listview_id == IDC_RULE_APPS_ID || listview_id == IDC_APP_RULES_ID))
						break;

					listview_context = (PITEM_LISTVIEW_CONTEXT)lpnmlv->lParam;

					if (!listview_context)
						break;

					_app_listview_destroycontext (listview_context);

					break;
				}

				case LVN_GETDISPINFO:
				{
					LPNMLVDISPINFOW lpnmlv;
					INT listview_id;

					lpnmlv = (LPNMLVDISPINFOW)lparam;
					listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					_app_message_displayinfo (hwnd, listview_id, lpnmlv);

					break;
				}

				case LVN_COLUMNCLICK:
				{
					LPNMLISTVIEW lpnmlv;
					INT listview_id;

					lpnmlv = (LPNMLISTVIEW)lparam;
					listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					_app_listview_sort (hwnd, listview_id, lpnmlv->iSubItem, TRUE);

					break;
				}

				case LVN_ITEMCHANGING:
				{
					LPNMLISTVIEW lpnmlv;
					INT listview_id;

					lpnmlv = (LPNMLISTVIEW)lparam;
					listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					context = _app_editor_getcontext (hwnd);

					if (!context)
						break;

					if ((lpnmlv->uChanged & LVIF_STATE) != 0)
					{
						if (listview_id == IDC_RULE_APPS_ID)
						{
							if ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (1) || ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (2)))
							{
								if (!_app_listview_islocked (hwnd, (INT)(INT_PTR)lpnmlv->hdr.idFrom) && context->ptr_rule->type != DATA_RULE_USER)
								{
									SetWindowLongPtrW (hwnd, DWLP_MSGRESULT, TRUE);

									return TRUE;
								}
							}
						}
					}

					break;
				}

				case LVN_ITEMCHANGED:
				{
					LPNMLISTVIEW lpnmlv;
					INT listview_id;
					BOOLEAN is_selected;

					lpnmlv = (LPNMLISTVIEW)lparam;
					listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					context = _app_editor_getcontext (hwnd);

					if (!context)
						break;

					if ((lpnmlv->uChanged & LVIF_STATE) != 0)
					{
						if (listview_id == IDC_RULE_APPS_ID || listview_id == IDC_APP_RULES_ID)
						{
							if ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (1) || ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (2)))
							{
								if (_app_listview_islocked (hwnd, (INT)(INT_PTR)lpnmlv->hdr.idFrom))
									break;

								_app_editor_settabtitle (hwnd, listview_id);

								_app_listview_refreshgroups (hwnd, listview_id);
								_app_listview_sort (hwnd, listview_id, INT_ERROR, FALSE);
							}
						}
						else if (listview_id == IDC_RULE_REMOTE_ID || listview_id == IDC_RULE_LOCAL_ID)
						{
							if ((lpnmlv->uOldState & LVIS_SELECTED) != 0 || (lpnmlv->uNewState & LVIS_SELECTED) != 0)
							{
								if (!context->ptr_rule->is_readonly)
								{
									is_selected = (lpnmlv->uNewState & LVIS_SELECTED) != 0;

									if (listview_id == IDC_RULE_REMOTE_ID)
									{
										_r_ctrl_enable (hwnd, IDC_RULE_REMOTE_EDIT, is_selected);
										_r_ctrl_enable (hwnd, IDC_RULE_REMOTE_DELETE, is_selected);
									}
									else if (listview_id == IDC_RULE_LOCAL_ID)
									{
										_r_ctrl_enable (hwnd, IDC_RULE_LOCAL_EDIT, is_selected);
										_r_ctrl_enable (hwnd, IDC_RULE_LOCAL_DELETE, is_selected);
									}
								}
							}
						}
					}

					break;
				}

				case LVN_GETINFOTIP:
				{
					LPNMLVGETINFOTIPW lpnmlv;
					PR_STRING string;
					ULONG_PTR context;
					INT listview_id;

					lpnmlv = (LPNMLVGETINFOTIPW)lparam;
					listview_id = (INT)lpnmlv->hdr.idFrom;

					if (listview_id != IDC_RULE_APPS_ID && listview_id != IDC_APP_RULES_ID)
						break;

					context = _app_listview_getitemcontext (hwnd, listview_id, lpnmlv->iItem);

					string = _app_gettooltipbylparam (hwnd, listview_id, context);

					if (!string)
						break;

					_r_str_copy (lpnmlv->pszText, lpnmlv->cchTextMax, string->buffer);

					_r_obj_dereference (string);

					break;
				}

				case LVN_GETEMPTYMARKUP:
				{
					NMLVEMPTYMARKUP* lpnmlv = (NMLVEMPTYMARKUP*)lparam;

					lpnmlv->dwFlags = EMF_CENTERED;

					_r_str_copy (lpnmlv->szMarkup, RTL_NUMBER_OF (lpnmlv->szMarkup), _r_locale_getstring (IDS_STATUS_EMPTY));

					SetWindowLongPtrW (hwnd, DWLP_MSGRESULT, TRUE);

					return TRUE;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			INT notify_code = HIWORD (wparam);
			INT ctrl_id = LOWORD (wparam);

			if (notify_code == EN_MAXTEXT)
			{
				_r_ctrl_showballoontip (hwnd, ctrl_id, 0, NULL, _r_locale_getstring (IDS_LIMIT_REACHED));

				return FALSE;
			}
			else if (notify_code == EN_CHANGE)
			{
				PR_STRING string;
				INT listview_id;

				if (ctrl_id == IDC_RULE_NAME_ID)
				{
					_r_ctrl_enable (GetParent (hwnd), IDC_SAVE, _r_ctrl_getstringlength (hwnd, ctrl_id) != 0); // enable apply button
					break;
				}

				if (ctrl_id != IDC_SEARCH)
					break;

				if (GetDlgItem (hwnd, IDC_RULE_APPS_ID))
				{
					listview_id = IDC_RULE_APPS_ID;
				}
				else if (GetDlgItem (hwnd, IDC_APP_RULES_ID))
				{
					listview_id = IDC_APP_RULES_ID;
				}
				else
				{
					return FALSE;
				}

				string = _r_ctrl_getstring (hwnd, IDC_SEARCH);

				_app_search_applyfilter (hwnd, listview_id, string);

				if (string)
					_r_obj_dereference (string);

				return FALSE;
			}

			switch (ctrl_id)
			{
				case IDC_RULE_REMOTE_ADD:
				case IDC_RULE_LOCAL_ADD:
				case IDC_RULE_REMOTE_EDIT:
				case IDC_RULE_LOCAL_EDIT:
				{
					PR_STRING string;
					ULONG_PTR current_length;
					INT listview_id;
					INT item_id;

					if (!_r_ctrl_isenabled (hwnd, ctrl_id))
						break;

					listview_id = (ctrl_id == IDC_RULE_REMOTE_ADD || ctrl_id == IDC_RULE_REMOTE_EDIT) ? IDC_RULE_REMOTE_ID : IDC_RULE_LOCAL_ID;

					if (ctrl_id == IDC_RULE_REMOTE_EDIT || ctrl_id == IDC_RULE_LOCAL_EDIT)
					{
						// edit rule
						item_id = _r_listview_getnextselected (hwnd, listview_id, INT_ERROR);

						if (item_id == INT_ERROR)
							break;
					}
					else
					{
						// create new rule
						item_id = INT_ERROR;
					}

					string = _app_editor_getrulesfromlistview (hwnd, listview_id, item_id);

					if (string)
					{
						current_length = _r_str_getlength2 (&string->sr);

						_r_obj_dereference (string);
					}
					else
					{
						current_length = 0;
					}

					if (ctrl_id == IDC_RULE_REMOTE_ADD || ctrl_id == IDC_RULE_LOCAL_ADD)
					{
						if (current_length >= RULE_RULE_CCH_MAX)
						{
							_r_show_errormessage (hwnd, NULL, STATUS_IMPLEMENTATION_LIMIT, NULL, ET_NATIVE);

							return FALSE;
						}
					}

					context = _app_editor_getcontext (hwnd);

					if (!context)
						break;

					context->hwnd = hwnd;
					context->listview_id = listview_id;
					context->item_id = item_id;
					context->current_length = current_length;

					_r_wnd_createmodalwindow (_r_sys_getimagebase (), MAKEINTRESOURCE (IDD_EDITOR_ADDRULE), hwnd, &EditorRuleProc, context);

					break;
				}

				case IDC_RULE_REMOTE_DELETE:
				case IDC_RULE_LOCAL_DELETE:
				{
					INT listview_id;
					INT selected_count;
					INT item_count;

					listview_id = ((ctrl_id == IDC_RULE_REMOTE_DELETE) ? IDC_RULE_REMOTE_ID : IDC_RULE_LOCAL_ID);

					selected_count = _r_listview_getselectedcount (hwnd, listview_id);

					if (!selected_count)
						break;

					if (_r_show_message (hwnd, MB_YESNO | MB_ICONEXCLAMATION, NULL, _r_locale_getstring (IDS_QUESTION_DELETE)) != IDYES)
						break;

					item_count = _r_listview_getitemcount (hwnd, listview_id) - 1;

					for (INT i = item_count; i != INT_ERROR; i--)
					{
						if (_r_listview_isitemselected (hwnd, listview_id, i))
							_r_listview_deleteitem (hwnd, listview_id, i);
					}

					_r_listview_setcolumn (hwnd, listview_id, 0, NULL, -100);

					break;
				}

				case IDM_CHECK:
				case IDM_UNCHECK:
				{
					INT listview_id;
					INT item_id = INT_ERROR;
					BOOLEAN new_val;

					if (GetDlgItem (hwnd, IDC_RULE_APPS_ID))
					{
						listview_id = IDC_RULE_APPS_ID;
					}
					else if (GetDlgItem (hwnd, IDC_APP_RULES_ID))
					{
						listview_id = IDC_APP_RULES_ID;
					}
					else
					{
						break;
					}

					new_val = (ctrl_id == IDM_CHECK);

					_app_listview_lock (hwnd, listview_id, TRUE);

					while ((item_id = _r_listview_getnextselected (hwnd, listview_id, item_id)) != INT_ERROR)
					{
						_r_listview_setitemcheck (hwnd, listview_id, item_id, new_val);
					}

					_app_listview_lock (hwnd, listview_id, FALSE);

					_app_listview_refreshgroups (hwnd, listview_id);
					_app_listview_sort (hwnd, listview_id, INT_ERROR, FALSE);

					break;
				}

				case IDC_APP_EXPLORE_ID:
				{
					context = _app_editor_getcontext (hwnd);

					if (!context)
						break;

					if (_app_isappvalidpath (context->ptr_app->real_path))
						_r_shell_showfile (&context->ptr_app->real_path->sr);

					break;
				}

				case IDC_APP_HASH_RECHECK_ID:
				{
					PR_STRING string;
					HANDLE hfile;
					NTSTATUS status;

					context = _app_editor_getcontext (hwnd);

					if (!context)
						break;

					if (!_app_isappvalidpath (context->ptr_app->real_path))
						break;

					status = _r_fs_openfile (
						&context->ptr_app->real_path->sr,
						GENERIC_READ,
						FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE,
						0,
						FALSE,
						&hfile
					);

					if (NT_SUCCESS (status))
					{
						string = _app_getfilehashinfo (hfile, context->ptr_app->app_hash);

						_r_ctrl_setstring (hwnd, IDC_APP_HASH_ID, _r_obj_getstringordefault (string, L"<empty>"));

						NtClose (hfile);
					}

					break;
				}

				case IDM_PROPERTIES:
				{
					ULONG_PTR index;
					INT listview_id;
					INT item_id;

					if (GetDlgItem (hwnd, IDC_RULE_APPS_ID))
					{
						listview_id = IDC_RULE_APPS_ID;
					}
					else if (GetDlgItem (hwnd, IDC_APP_RULES_ID))
					{
						listview_id = IDC_APP_RULES_ID;
					}
					else
					{
						break;
					}

					item_id = _r_listview_getnextselected (hwnd, listview_id, INT_ERROR);

					if (item_id != INT_ERROR)
					{
						index = _app_listview_getitemcontext (hwnd, listview_id, item_id);

						_app_listview_showitemby_param (_r_app_gethwnd (), index, (listview_id == IDC_RULE_APPS_ID));
					}

					break;
				}

				case IDM_COPY:
				{
					R_STRINGBUILDER sb;
					PR_STRING string;
					HWND hlistview;
					INT listview_id;
					INT item_id = INT_ERROR;

					hlistview = GetFocus ();

					if (!hlistview)
						break;

					listview_id = GetDlgCtrlID (hlistview);

					if (!listview_id || !_r_listview_getitemcount (hwnd, listview_id))
						break;

					_r_obj_initializestringbuilder (&sb, 512);

					while ((item_id = _r_listview_getnextselected (hwnd, listview_id, item_id)) != INT_ERROR)
					{
						string = _r_listview_getitemtext (hwnd, listview_id, item_id, 0);

						if (string)
						{
							_r_obj_appendstringbuilder2 (&sb, &string->sr);
							_r_obj_appendstringbuilder (&sb, SZ_CRLF);

							_r_obj_dereference (string);
						}
					}

					string = _r_obj_finalstringbuilder (&sb);

					_r_str_trimstring2 (&string->sr, DIVIDER_TRIM, 0);

					_r_clipboard_set (hwnd, &string->sr);

					_r_obj_deletestringbuilder (&sb);

					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

INT_PTR CALLBACK EditorProc (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
)
{
	PEDITOR_CONTEXT context;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			WCHAR title[128] = {0};
			PR_STRING string;

			context = (PEDITOR_CONTEXT)lparam;

			_app_editor_setcontext (hwnd, context);

			// configure tabs
			if (context->is_settorules)
			{
				_r_str_copy (title, RTL_NUMBER_OF (title), _r_obj_getstringorempty (context->ptr_rule->name));

				if (context->ptr_rule->is_readonly)
					_r_str_appendformat (title, RTL_NUMBER_OF (title), L" (%s)", _r_locale_getstring (IDS_INTERNAL_RULE));

				_app_editor_addtabitem (hwnd, IDS_SETTINGS_GENERAL, IDD_EDITOR_GENERAL, context);
				_app_editor_addtabitem (hwnd, IDS_RULE, IDD_EDITOR_RULE, context);
				_app_editor_addtabitem (hwnd, IDS_TAB_APPS, IDD_EDITOR_APPS, context);

				// set state
				_r_ctrl_setstring (hwnd, IDC_ENABLE_CHK, _r_locale_getstring (IDS_ENABLE_CHK));

				_r_ctrl_checkbutton (hwnd, IDC_ENABLE_CHK, !!(context->ptr_rule->is_enabled));
			}
			else
			{
				string = _app_getappdisplayname (context->ptr_app, TRUE);

				if (string)
				{
					_r_str_copy (title, RTL_NUMBER_OF (title), string->buffer);

					_r_obj_dereference (string);
				}

				_app_editor_addtabitem (hwnd, IDS_SETTINGS_GENERAL, IDD_EDITOR_APPINFO, context);
				_app_editor_addtabitem (hwnd, IDS_TRAY_RULES, IDD_EDITOR_APPRULES, context);

				// show state
				_r_ctrl_setstring (hwnd, IDC_ENABLE_CHK, _r_locale_getstring (IDS_ENABLE_APP_CHK));

				_r_ctrl_checkbutton (hwnd, IDC_ENABLE_CHK, !!(context->ptr_app->is_enabled));
			}

			// initialize layout
			_r_layout_initializemanager (&context->layout_manager, hwnd);

			_r_wnd_top (hwnd, TRUE);

			_r_wnd_center (hwnd, GetParent (hwnd));

			_r_window_restoreposition (hwnd, L"editor");

			// set window title
			_r_ctrl_setstring (hwnd, 0, title);

			_r_ctrl_setstring (hwnd, IDC_SAVE, _r_locale_getstring (IDS_SAVE));
			_r_ctrl_setstring (hwnd, IDC_CLOSE, _r_locale_getstring (IDS_CLOSE));

			_r_tab_selectitem (hwnd, IDC_TAB, _r_calc_clamp (context->page_id, 0, _r_tab_getitemcount (hwnd, IDC_TAB)));

			_r_theme_initialize (hwnd);

			break;
		}

		case WM_DESTROY:
		{
			_r_window_saveposition (hwnd, L"editor");

			context = _app_editor_getcontext (hwnd);

			if (context)
				_r_layout_destroymanager (&context->layout_manager);

			break;
		}

		case WM_CLOSE:
		{
			EndDialog (hwnd, FALSE);
			break;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc;

			hdc = BeginPaint (hwnd, &ps);

			if (!hdc)
				break;

			_r_dc_drawwindow (hdc, hwnd, FALSE);

			EndPaint (hwnd, &ps);

			break;
		}

		case WM_DPICHANGED:
		{
			_r_wnd_message_dpichanged (hwnd, wparam, lparam);
			break;
		}

		case WM_SETTINGCHANGE:
		{
			_r_wnd_message_settingchange (hwnd, wparam, lparam);
			break;
		}

		case WM_SIZE:
		{
			HWND hpage;
			INT item_id;

			context = _app_editor_getcontext (hwnd);

			if (!context)
				break;

			_r_layout_resize (&context->layout_manager, wparam);

			item_id = _r_tab_getcurrentitem (hwnd, IDC_TAB);
			hpage = (HWND)_r_tab_getitemlparam (hwnd, IDC_TAB, item_id);

			if (hpage)
				_r_tab_adjustchild (hwnd, IDC_TAB, hpage);

			break;
		}

		case WM_GETMINMAXINFO:
		{
			context = _app_editor_getcontext (hwnd);

			if (!context)
				break;

			_r_layout_resizeminimumsize (&context->layout_manager, lparam);

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lparam;

			switch (nmlp->code)
			{
				case TCN_SELCHANGING:
				{
					HWND hpage;
					INT item_id;

					item_id = _r_tab_getcurrentitem (hwnd, IDC_TAB);
					hpage = (HWND)_r_tab_getitemlparam (hwnd, IDC_TAB, item_id);

					if (hpage)
						ShowWindow (hpage, SW_HIDE);

					break;
				}

				case TCN_SELCHANGE:
				{
					HWND hpage;
					INT item_id;

					item_id = _r_tab_getcurrentitem (hwnd, IDC_TAB);
					hpage = (HWND)_r_tab_getitemlparam (hwnd, IDC_TAB, item_id);

					if (!hpage)
						break;

					_r_tab_adjustchild (hwnd, IDC_TAB, hpage);

					ShowWindow (hpage, SW_SHOWNA);
					SetFocus (NULL);

					break;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			INT ctrl_id = LOWORD (wparam);

			switch (ctrl_id)
			{
				case IDOK: // process Enter key
				case IDC_SAVE:
				{
					HWND hpage_general;
					HWND hpage_rule;
					HWND hpage_apps;
					PR_STRING string;
					PR_LIST rules = NULL;
					HANDLE hengine;
					PITEM_RULE ptr_rule;
					ULONG_PTR rule_idx;
					ULONG app_hash;
					INT listview_id;
					INT item_id;
					BOOLEAN is_enable;

					context = _app_editor_getcontext (hwnd);

					if (!context)
						break;

					hpage_general = (HWND)_r_tab_getitemlparam (hwnd, IDC_TAB, 0);
					hpage_rule = (HWND)_r_tab_getitemlparam (hwnd, IDC_TAB, 1);

					if (context->is_settorules)
					{
						hpage_apps = (HWND)_r_tab_getitemlparam (hwnd, IDC_TAB, 2);

						if (!hpage_general || !_r_ctrl_getstringlength (hpage_general, IDC_RULE_NAME_ID))
							return FALSE;

						context->ptr_rule->is_haveerrors = FALSE; // reset errors

						// do not change read-only rules
						if (!context->ptr_rule->is_readonly)
						{
							// name
							string = _r_ctrl_getstring (hpage_general, IDC_RULE_NAME_ID);

							if (!string)
								return FALSE;

							_r_str_trimstring2 (&string->sr, DIVIDER_TRIM DIVIDER_RULE, 0);

							if (_r_obj_isstringempty2 (string))
							{
								_r_ctrl_showballoontip (hpage_general, IDC_RULE_NAME_ID, 0, NULL, _r_locale_getstring (IDS_STATUS_EMPTY));

								_r_obj_dereference (string);

								return FALSE;
							}

							_r_obj_movereference ((PVOID_PTR)&context->ptr_rule->name, string);

							// comment
							string = _r_ctrl_getstring (hpage_general, IDC_RULE_COMMENT_ID);

							if (string)
								_r_str_trimstring2 (&string->sr, DIVIDER_TRIM DIVIDER_RULE, 0);

							_r_obj_movereference ((PVOID_PTR)&context->ptr_rule->comment, string);

							if (hpage_rule)
							{
								// rule (remote)
								string = _app_editor_getrulesfromlistview (hpage_rule, IDC_RULE_REMOTE_ID, INT_ERROR);

								_r_obj_movereference ((PVOID_PTR)&context->ptr_rule->rule_remote, string);

								// rule (local)
								string = _app_editor_getrulesfromlistview (hpage_rule, IDC_RULE_LOCAL_ID, INT_ERROR);

								_r_obj_movereference ((PVOID_PTR)&context->ptr_rule->rule_local, string);
							}

							item_id = _r_combobox_getcurrentitem (hpage_general, IDC_RULE_PROTOCOL_ID);

							context->ptr_rule->protocol = (UINT8)_r_combobox_getitemlparam (hpage_general, IDC_RULE_PROTOCOL_ID, item_id);

							item_id = _r_combobox_getcurrentitem (hpage_general, IDC_RULE_VERSION_ID);

							context->ptr_rule->af = (ADDRESS_FAMILY)_r_combobox_getitemlparam (hpage_general, IDC_RULE_VERSION_ID, item_id);

							string = _app_db_getprotoname (context->ptr_rule->protocol, context->ptr_rule->af, FALSE);

							_r_obj_movereference ((PVOID_PTR)&context->ptr_rule->protocol_str, string);

							item_id = _r_combobox_getcurrentitem (hpage_general, IDC_RULE_DIRECTION_ID);

							context->ptr_rule->direction = (FWP_DIRECTION)_r_combobox_getitemlparam (hpage_general, IDC_RULE_DIRECTION_ID, item_id);

							item_id = _r_combobox_getcurrentitem (hpage_general, IDC_RULE_ACTION_ID);

							context->ptr_rule->action = (FWP_ACTION_TYPE)_r_combobox_getitemlparam (hpage_general, IDC_RULE_ACTION_ID, item_id);

							if (context->ptr_rule->type == DATA_RULE_USER)
							{
								if (context->ptr_rule->action == FWP_ACTION_BLOCK)
								{
									context->ptr_rule->weight = FW_WEIGHT_RULE_USER_BLOCK;
								}
								else
								{
									context->ptr_rule->weight = FW_WEIGHT_RULE_USER;
								}
							}
						}

						// save rule apps
						if (context->ptr_rule->type == DATA_RULE_USER)
						{
							_r_obj_clearhashtable (context->ptr_rule->apps);

							for (INT i = 0; i < _r_listview_getitemcount (hpage_apps, IDC_RULE_APPS_ID); i++)
							{
								if (!_r_listview_isitemchecked (hpage_apps, IDC_RULE_APPS_ID, i))
									continue;

								app_hash = (ULONG)_app_listview_getitemcontext (hpage_apps, IDC_RULE_APPS_ID, i);

								if (context->ptr_rule->is_forservices && _app_issystemhash (app_hash))
									continue;

								if (_app_isappfound (app_hash))
									_r_obj_addhashtableitem (context->ptr_rule->apps, app_hash, NULL);
							}
						}

						// enable rule
						_app_ruleenable (context->ptr_rule, _r_ctrl_isbuttonchecked (hwnd, IDC_ENABLE_CHK), TRUE);

						rules = _r_obj_createlist (1, NULL);

						_r_obj_addlistitem (rules, context->ptr_rule, NULL);
					}
					else
					{
						context->ptr_app->is_haveerrors = FALSE; // reset errors
						context->ptr_app->is_enabled = _r_ctrl_isbuttonchecked (hwnd, IDC_ENABLE_CHK);

						rules = _r_obj_createlist (1, NULL);

						_r_obj_addlistitem (rules, context->ptr_app, NULL);

						// comment
						string = _r_ctrl_getstring (hpage_general, IDC_APP_COMMENT_ID);

						if (string)
							_r_str_trimstring2 (&string->sr, DIVIDER_TRIM DIVIDER_RULE, 0);

						_r_obj_movereference ((PVOID_PTR)&context->ptr_app->comment, string);

						if (hpage_rule)
						{
							_r_queuedlock_acquireshared (&lock_rules);

							for (INT i = 0; i < _r_listview_getitemcount (hpage_rule, IDC_APP_RULES_ID); i++)
							{
								rule_idx = _app_listview_getitemcontext (hpage_rule, IDC_APP_RULES_ID, i);
								ptr_rule = _r_obj_getlistitem (rules_list, rule_idx);

								if (!ptr_rule)
									continue;

								listview_id = _app_listview_getbytype (ptr_rule->type);
								item_id = _app_listview_finditem (_r_app_gethwnd (), listview_id, rule_idx);

								if (item_id != INT_ERROR)
								{
									is_enable = _r_listview_isitemchecked (hpage_rule, IDC_APP_RULES_ID, i);

									_app_setruletoapp (_r_app_gethwnd (), ptr_rule, item_id, context->ptr_app, is_enable);
								}
							}

							_r_queuedlock_releaseshared (&lock_rules);
						}
					}

					// apply filter
					if (rules)
					{
						if (!_r_obj_isempty2 (rules) && _wfp_isfiltersinstalled ())
						{
							hengine = _wfp_getenginehandle ();

							if (context->is_settorules)
							{
								_wfp_create4filters (hengine, rules, DBG_ARG, FALSE);
							}
							else
							{
								_wfp_create3filters (hengine, rules, DBG_ARG, FALSE);
							}
						}

						_r_obj_dereference (rules);
					}

					EndDialog (hwnd, TRUE);

					break;
				}

				case IDCANCEL: // process Esc key
				{
					HWND hpage;
					HWND hsearch;
					INT item_id;

					item_id = _r_tab_getcurrentitem (hwnd, IDC_TAB);
					hpage = (HWND)_r_tab_getitemlparam (hwnd, IDC_TAB, item_id);

					if (hpage)
					{
						hsearch = GetDlgItem (hpage, IDC_SEARCH);

						if (hsearch)
						{
							if (GetFocus () == hsearch)
							{
								_r_ctrl_setstring (hsearch, 0, L"");
								SetFocus (hwnd);

								break;
							}
						}
					}

					FALLTHROUGH;
				}

				case IDC_CLOSE:
				{
					EndDialog (hwnd, FALSE);
					break;
				}
			}

			break;
		}
	}

	return FALSE;
}
