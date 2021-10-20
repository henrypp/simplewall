// simplewall
// Copyright (c) 2016-2021 Henry++

#include "global.h"

static VOID _app_addeditortab (_In_ HWND hwnd, _In_ UINT locale_id, _In_ INT dlg_id, _In_ PITEM_CONTEXT context, _Inout_ PINT tabs_count)
{
	HWND htab;

	htab = CreateDialogParam (NULL, MAKEINTRESOURCE (dlg_id), hwnd, &PropertiesPagesProc, (LPARAM)context);

	if (!htab)
		return;

	_r_tab_additem (hwnd, IDC_TAB, *tabs_count, _r_locale_getstring (locale_id), I_IMAGENONE, (LPARAM)htab);

	*tabs_count += 1;

	_r_tab_adjustchild (hwnd, IDC_TAB, htab);

	SendMessage (htab, RM_INITIALIZE, 0, 0);
}

static VOID _app_settabcounttitle (_In_ HWND hwnd, _In_ INT listview_id)
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
	else // if (listview_id == IDC_APP_RULES_ID)
	{
		locale_id = IDS_TRAY_RULES;
		tab_id = 1;
	}

	hparent = GetParent (hwnd);

	if (!hparent)
		return;

	checked_count = _r_listview_getitemcheckedcount (hwnd, listview_id);

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%s (%d)", _r_locale_getstring (locale_id), checked_count);

	_r_tab_setitem (hparent, IDC_TAB, tab_id, buffer, I_IMAGENONE, 0);
}

static PR_STRING _app_getrulesfromlistview (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT exclude_id)
{
	static R_STRINGREF divider_sr = PR_STRINGREF_INIT (DIVIDER_RULE);
	R_STRINGBUILDER buffer;
	PR_STRING string;

	_r_obj_initializestringbuilder (&buffer);

	for (INT i = 0; i < _r_listview_getitemcount (hwnd, ctrl_id); i++)
	{
		if (i == exclude_id)
			continue;

		string = _r_listview_getitemtext (hwnd, ctrl_id, i, 0);

		if (string)
		{
			_r_str_trimstring2 (string, DIVIDER_TRIM DIVIDER_RULE, 0);

			if (!_r_obj_isstringempty2 (string))
			{
				// check maximum length of one rule
				if ((_r_obj_getstringlength (buffer.string) + _r_obj_getstringlength (string)) > RULE_RULE_CCH_MAX)
				{
					_r_obj_dereference (string);
					break;
				}

				_r_obj_appendstringbuilder2 (&buffer, string);
				_r_obj_appendstringbuilder3 (&buffer, &divider_sr);
			}

			_r_obj_dereference (string);
		}
	}

	string = _r_obj_finalstringbuilder (&buffer);

	_r_str_trimstring (string, &divider_sr, 0);

	return string;
}

static VOID _app_setrulestolistview (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ PR_STRING rule)
{
	PR_STRING rule_string;
	R_STRINGREF first_part;
	R_STRINGREF remaining_part;

	INT item_id = 0;

	_r_obj_initializestringref2 (&remaining_part, rule);

	while (remaining_part.length != 0)
	{
		_r_str_splitatchar (&remaining_part, DIVIDER_RULE[0], &first_part, &remaining_part);

		rule_string = _r_obj_createstring3 (&first_part);

		_app_setcheckboxlock (hwnd, ctrl_id, TRUE);
		_r_listview_additem (hwnd, ctrl_id, item_id, rule_string->buffer);
		_app_setcheckboxlock (hwnd, ctrl_id, FALSE);

		item_id += 1;

		_r_obj_dereference (rule_string);
	}

	_r_listview_setcolumn (hwnd, ctrl_id, 0, NULL, -100);
}

INT_PTR CALLBACK AddRuleProc (_In_ HWND hwnd, _In_ UINT msg, _In_ WPARAM wparam, _In_ LPARAM lparam)
{
	static PITEM_CONTEXT context = NULL;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			PR_STRING string;

			context = (PITEM_CONTEXT)lparam;

			_r_wnd_center (hwnd, GetParent (hwnd));

			// localize window
			SetWindowText (hwnd, _r_locale_getstring (IDS_RULE));

			_r_edit_settextlimit (hwnd, IDC_RULE_ID, _r_calc_clamp32 (RULE_RULE_CCH_MAX - context->current_length - 1, 1, RULE_RULE_CCH_MAX));

			if (context->item_id != -1)
			{
				string = _r_listview_getitemtext (context->hwnd, context->listview_id, context->item_id, 0);

				if (string)
				{
					_r_ctrl_setstring (hwnd, IDC_RULE_ID, string->buffer);

					_r_obj_dereference (string);
				}
			}

			_r_ctrl_setstring (hwnd, IDC_RULE_HINT, L"eg. 192.168.0.1\r\neg. [fe80::]\r\neg. 192.168.0.1:443\r\neg. [fe80::]:443\r\neg. 192.168.0.1-192.168.0.255\r\neg. 192.168.0.1-192.168.0.255:443\r\neg. 192.168.0.0/16\r\neg. fe80::/10\r\neg. 80\r\neg. 443\r\neg. 20-21\r\neg. 49152-65534");

			_r_ctrl_setstring (hwnd, IDC_SAVE, _r_locale_getstring (context->item_id != -1 ? IDS_SAVE : IDS_ADD));
			_r_ctrl_setstring (hwnd, IDC_CLOSE, _r_locale_getstring (IDS_CLOSE));

			_r_ctrl_enable (hwnd, IDC_SAVE, _r_ctrl_getstringlength (hwnd, IDC_RULE_ID) != 0); // enable apply button

			break;
		}

		case WM_CLOSE:
		{
			EndDialog (hwnd, FALSE);
			break;
		}

		case WM_SETTINGCHANGE:
		{
			_r_wnd_changesettings (hwnd, wparam, lparam);
			break;
		}

		case WM_COMMAND:
		{
			INT ctrl_id = LOWORD (wparam);
			INT notify_code = HIWORD (wparam);

			if (notify_code == EN_CHANGE)
			{
				_r_ctrl_enable (hwnd, IDC_SAVE, _r_ctrl_getstringlength (hwnd, IDC_RULE_ID) != 0); // enable apply button
				return FALSE;
			}
			else if (notify_code == EN_MAXTEXT)
			{
				_r_ctrl_showballoontip (hwnd, ctrl_id, 0, NULL, SZ_MAXTEXT);
				return FALSE;
			}

			switch (ctrl_id)
			{
				case IDOK: // process Enter key
				case IDC_SAVE:
				{
					PR_STRING string;

					if (!_r_ctrl_isenabled (hwnd, IDC_SAVE))
						return FALSE;

					string = _r_ctrl_getstring (hwnd, IDC_RULE_ID);

					if (!string)
						return FALSE;

					_r_str_trimstring2 (string, DIVIDER_TRIM DIVIDER_RULE, 0);

					if (_r_obj_isstringempty2 (string))
					{
						_r_ctrl_showballoontip (hwnd, IDC_RULE_ID, 0, NULL, _r_locale_getstring (IDS_STATUS_EMPTY));
						_r_ctrl_enable (hwnd, IDC_SAVE, FALSE);

						_r_obj_dereference (string);

						return FALSE;
					}

					WCHAR rule_string[256];
					R_STRINGREF remaining_part;
					R_STRINGREF first_part;

					_r_obj_initializestringref2 (&remaining_part, string);

					INT item_id = _r_listview_getitemcount (context->hwnd, context->listview_id);

					while (remaining_part.length != 0)
					{
						_r_str_splitatchar (&remaining_part, DIVIDER_RULE[0], &first_part, &remaining_part);

						if (!_app_parserulestring (&first_part, NULL))
						{
							_r_ctrl_showballoontip (hwnd, IDC_RULE_ID, 0, NULL, _r_locale_getstring (IDS_STATUS_SYNTAX_ERROR));
							_r_ctrl_enable (hwnd, IDC_SAVE, FALSE);

							return FALSE;
						}

						_r_str_copystring (rule_string, RTL_NUMBER_OF (rule_string), &first_part);

						_r_listview_additem (context->hwnd, context->listview_id, item_id, rule_string);

						item_id += 1;
					}

					if (context->item_id != -1)
						_r_listview_deleteitem (context->hwnd, context->listview_id, context->item_id);

					_r_obj_dereference (string);

					_r_listview_setcolumn (context->hwnd, context->listview_id, 0, NULL, -100);

					// no break;
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

INT_PTR CALLBACK PropertiesPagesProc (_In_ HWND hwnd, _In_ UINT msg, _In_ WPARAM wparam, _In_ LPARAM lparam)
{
	static PITEM_CONTEXT context = NULL;
	static HICON hicon_large = NULL;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			context = (PITEM_CONTEXT)lparam;

			EnableThemeDialogTexture (hwnd, ETDT_ENABLETAB);

			SetWindowPos (hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_FRAMECHANGED);

			// name
			if (GetDlgItem (hwnd, IDC_RULE_NAME_ID))
			{
				_r_ctrl_setstringformat (hwnd, IDC_RULE_NAME, L"%s:", _r_locale_getstring (IDS_NAME));

				if (!_r_obj_isstringempty (context->ptr_rule->name))
					_r_ctrl_setstring (hwnd, IDC_RULE_NAME_ID, context->ptr_rule->name->buffer);

				SendDlgItemMessage (hwnd, IDC_RULE_NAME_ID, EM_LIMITTEXT, RULE_NAME_CCH_MAX - 1, 0);
				SendDlgItemMessage (hwnd, IDC_RULE_NAME_ID, EM_SETREADONLY, (WPARAM)context->ptr_rule->is_readonly, 0);
			}

			// direction
			if (GetDlgItem (hwnd, IDC_RULE_DIRECTION))
			{
				_r_ctrl_setstringformat (hwnd, IDC_RULE_DIRECTION, L"%s:", _r_locale_getstring (IDS_DIRECTION));

				_r_ctrl_setstring (hwnd, IDC_RULE_DIRECTION_OUTBOUND, _r_locale_getstring (IDS_DIRECTION_1));
				_r_ctrl_setstring (hwnd, IDC_RULE_DIRECTION_INBOUND, _r_locale_getstring (IDS_DIRECTION_2));
				_r_ctrl_setstring (hwnd, IDC_RULE_DIRECTION_ANY, _r_locale_getstring (IDS_ANY));

				if (context->ptr_rule->direction == FWP_DIRECTION_OUTBOUND)
				{
					CheckRadioButton (hwnd, IDC_RULE_DIRECTION_OUTBOUND, IDC_RULE_DIRECTION_ANY, IDC_RULE_DIRECTION_OUTBOUND);
				}
				else if (context->ptr_rule->direction == FWP_DIRECTION_INBOUND)
				{
					CheckRadioButton (hwnd, IDC_RULE_DIRECTION_OUTBOUND, IDC_RULE_DIRECTION_ANY, IDC_RULE_DIRECTION_INBOUND);
				}
				else if (context->ptr_rule->direction == FWP_DIRECTION_MAX)
				{
					CheckRadioButton (hwnd, IDC_RULE_DIRECTION_OUTBOUND, IDC_RULE_DIRECTION_ANY, IDC_RULE_DIRECTION_ANY);
				}

				_r_ctrl_enable (hwnd, IDC_RULE_DIRECTION_OUTBOUND, !context->ptr_rule->is_readonly);
				_r_ctrl_enable (hwnd, IDC_RULE_DIRECTION_INBOUND, !context->ptr_rule->is_readonly);
				_r_ctrl_enable (hwnd, IDC_RULE_DIRECTION_ANY, !context->ptr_rule->is_readonly);
			}

			// action
			if (GetDlgItem (hwnd, IDC_RULE_ACTION))
			{
				_r_ctrl_setstringformat (hwnd, IDC_RULE_ACTION, L"%s:", _r_locale_getstring (IDS_ACTION));

				_r_ctrl_setstring (hwnd, IDC_RULE_ACTION_BLOCK, _r_locale_getstring (IDS_ACTION_BLOCK));
				_r_ctrl_setstring (hwnd, IDC_RULE_ACTION_ALLOW, _r_locale_getstring (IDS_ACTION_ALLOW));

				CheckRadioButton (hwnd, IDC_RULE_ACTION_BLOCK, IDC_RULE_ACTION_ALLOW, (context->ptr_rule->action == FWP_ACTION_BLOCK) ? IDC_RULE_ACTION_BLOCK : IDC_RULE_ACTION_ALLOW);

				_r_ctrl_enable (hwnd, IDC_RULE_ACTION_ALLOW, !context->ptr_rule->is_readonly);
				_r_ctrl_enable (hwnd, IDC_RULE_ACTION_BLOCK, !context->ptr_rule->is_readonly);
			}

			// protocols
			if (GetDlgItem (hwnd, IDC_RULE_PROTOCOL_ID))
			{
				static UINT8 protos[] = {
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

				WCHAR format[256];
				INT index = 0;

				_r_ctrl_setstringformat (hwnd, IDC_RULE_PROTOCOL, L"%s:", _r_locale_getstring (IDS_PROTOCOL));

				_r_combobox_insertitem (hwnd, IDC_RULE_PROTOCOL_ID, index, _r_locale_getstring (IDS_ANY));
				_r_combobox_setitemparam (hwnd, IDC_RULE_PROTOCOL_ID, index, 0);

				if (context->ptr_rule->protocol == 0)
					_r_combobox_setcurrentitem (hwnd, IDC_RULE_PROTOCOL_ID, index);

				for (SIZE_T i = 0; i < RTL_NUMBER_OF (protos); i++)
				{
					index += 1;

					_r_str_printf (format, RTL_NUMBER_OF (format), L"%s (%" TEXT (PRIu8) L")", _app_getprotoname (protos[i], AF_UNSPEC, SZ_UNKNOWN), protos[i]);

					_r_combobox_insertitem (hwnd, IDC_RULE_PROTOCOL_ID, index, format);
					_r_combobox_setitemparam (hwnd, IDC_RULE_PROTOCOL_ID, index, (LPARAM)protos[i]);

					if (context->ptr_rule->protocol == protos[i])
						_r_combobox_setcurrentitem (hwnd, IDC_RULE_PROTOCOL_ID, index);
				}

				// unknown protocol
				if (_r_combobox_getcurrentitem (hwnd, IDC_RULE_PROTOCOL_ID) == CB_ERR)
				{
					_r_str_printf (format, RTL_NUMBER_OF (format), L"%s (%" TEXT (PR_ULONG) L")", _app_getprotoname (context->ptr_rule->protocol, AF_UNSPEC, SZ_UNKNOWN), context->ptr_rule->protocol);

					_r_combobox_insertitem (hwnd, IDC_RULE_PROTOCOL_ID, index, format);
					_r_combobox_setitemparam (hwnd, IDC_RULE_PROTOCOL_ID, index, (LPARAM)context->ptr_rule->protocol);

					_r_combobox_setcurrentitem (hwnd, IDC_RULE_PROTOCOL_ID, index);
				}

				_r_ctrl_enable (hwnd, IDC_RULE_PROTOCOL_ID, !context->ptr_rule->is_readonly);
			}

			// family (ports-only)
			if (GetDlgItem (hwnd, IDC_RULE_VERSION_ID))
			{
				_r_ctrl_setstringformat (hwnd, IDC_RULE_VERSION, L"%s:", _r_locale_getstring (IDS_PORTVERSION));

				_r_combobox_insertitem (hwnd, IDC_RULE_VERSION_ID, 0, _r_locale_getstring (IDS_ANY));
				_r_combobox_setitemparam (hwnd, IDC_RULE_VERSION_ID, 0, (LPARAM)AF_UNSPEC);

				_r_combobox_insertitem (hwnd, IDC_RULE_VERSION_ID, 1, L"IPv4");
				_r_combobox_setitemparam (hwnd, IDC_RULE_VERSION_ID, 1, (LPARAM)AF_INET);

				_r_combobox_insertitem (hwnd, IDC_RULE_VERSION_ID, 2, L"IPv6");
				_r_combobox_setitemparam (hwnd, IDC_RULE_VERSION_ID, 2, (LPARAM)AF_INET6);

				if (context->ptr_rule->af == AF_INET)
				{
					_r_combobox_setcurrentitem (hwnd, IDC_RULE_VERSION_ID, 1);
				}
				else if (context->ptr_rule->af == AF_INET6)
				{
					_r_combobox_setcurrentitem (hwnd, IDC_RULE_VERSION_ID, 2);
				}
				else
				{
					_r_combobox_setcurrentitem (hwnd, IDC_RULE_VERSION_ID, 0);
				}

				_r_ctrl_enable (hwnd, IDC_RULE_VERSION_ID, !context->ptr_rule->is_readonly);
			}

			// rule (remote)
			if (GetDlgItem (hwnd, IDC_RULE_REMOTE_ID))
			{
				_r_ctrl_setstringformat (hwnd, IDC_RULE_REMOTE, L"%s (" SZ_DIRECTION_REMOTE L"):", _r_locale_getstring (IDS_RULE));

				_r_listview_setstyle (hwnd, IDC_RULE_REMOTE_ID, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP, FALSE);
				_r_listview_addcolumn (hwnd, IDC_RULE_REMOTE_ID, 0, NULL, -100, 0);

				if (!_r_obj_isstringempty (context->ptr_rule->rule_remote))
				{
					_app_setrulestolistview (hwnd, IDC_RULE_REMOTE_ID, context->ptr_rule->rule_remote);

					_r_listview_setcolumn (hwnd, IDC_RULE_REMOTE_ID, 0, NULL, -100);
				}

				_r_ctrl_setstringformat (hwnd, IDC_RULE_REMOTE_ADD, L"%s...", _r_locale_getstring (IDS_ADD));
				_r_ctrl_setstringformat (hwnd, IDC_RULE_REMOTE_EDIT, L"%s...", _r_locale_getstring (IDS_EDIT2));
				_r_ctrl_setstring (hwnd, IDC_RULE_REMOTE_DELETE, _r_locale_getstring (IDS_DELETE));

				_r_ctrl_enable (hwnd, IDC_RULE_REMOTE_ADD, !context->ptr_rule->is_readonly);
				_r_ctrl_enable (hwnd, IDC_RULE_REMOTE_EDIT, FALSE);
				_r_ctrl_enable (hwnd, IDC_RULE_REMOTE_DELETE, FALSE);
			}

			// rule (local)
			if (GetDlgItem (hwnd, IDC_RULE_LOCAL_ID))
			{
				_r_ctrl_setstringformat (hwnd, IDC_RULE_LOCAL, L"%s (" SZ_DIRECTION_LOCAL L"):", _r_locale_getstring (IDS_RULE));

				_r_listview_setstyle (hwnd, IDC_RULE_LOCAL_ID, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP, FALSE);
				_r_listview_addcolumn (hwnd, IDC_RULE_LOCAL_ID, 0, NULL, -100, 0);

				if (!_r_obj_isstringempty (context->ptr_rule->rule_local))
				{
					_app_setrulestolistview (hwnd, IDC_RULE_LOCAL_ID, context->ptr_rule->rule_local);

					_r_listview_setcolumn (hwnd, IDC_RULE_LOCAL_ID, 0, NULL, -100);
				}

				_r_ctrl_setstringformat (hwnd, IDC_RULE_LOCAL_ADD, L"%s...", _r_locale_getstring (IDS_ADD));
				_r_ctrl_setstringformat (hwnd, IDC_RULE_LOCAL_EDIT, L"%s...", _r_locale_getstring (IDS_EDIT2));
				_r_ctrl_setstring (hwnd, IDC_RULE_LOCAL_DELETE, _r_locale_getstring (IDS_DELETE));

				_r_ctrl_enable (hwnd, IDC_RULE_LOCAL_ADD, !context->ptr_rule->is_readonly);
				_r_ctrl_enable (hwnd, IDC_RULE_LOCAL_EDIT, FALSE);
				_r_ctrl_enable (hwnd, IDC_RULE_LOCAL_DELETE, FALSE);
			}

			// apps
			if (GetDlgItem (hwnd, IDC_RULE_APPS_ID))
			{
				_app_listviewsetview (hwnd, IDC_RULE_APPS_ID);

				_r_listview_setstyle (hwnd, IDC_RULE_APPS_ID, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_CHECKBOXES, TRUE);

				_r_listview_addcolumn (hwnd, IDC_RULE_APPS_ID, 0, _r_locale_getstring (IDS_NAME), 0, LVCFMT_LEFT);

				_r_listview_addgroup (hwnd, IDC_RULE_APPS_ID, 0, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
				_r_listview_addgroup (hwnd, IDC_RULE_APPS_ID, 1, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
				_r_listview_addgroup (hwnd, IDC_RULE_APPS_ID, 2, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);

				// apps (apply to)
				{
					PITEM_APP ptr_app;
					SIZE_T enum_key = 0;
					BOOLEAN is_enabled;

					_r_queuedlock_acquireshared (&lock_apps);

					while (_r_obj_enumhashtablepointer (apps_table, &ptr_app, NULL, &enum_key))
					{
						// check for services
						is_enabled = (_r_obj_findhashtable (context->ptr_rule->apps, ptr_app->app_hash)) || (context->ptr_rule->is_forservices && (ptr_app->app_hash == config.ntoskrnl_hash || ptr_app->app_hash == config.svchost_hash));

						_app_setcheckboxlock (hwnd, IDC_RULE_APPS_ID, TRUE);

						_r_listview_additem_ex (hwnd, IDC_RULE_APPS_ID, 0, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK, I_GROUPIDCALLBACK, _app_createlistviewparam (ptr_app->app_hash));
						_r_listview_setitemcheck (hwnd, IDC_RULE_APPS_ID, 0, is_enabled);

						_app_setcheckboxlock (hwnd, IDC_RULE_APPS_ID, FALSE);
					}

					_r_queuedlock_releaseshared (&lock_apps);

				}

				// resize column
				_r_listview_setcolumn (hwnd, IDC_RULE_APPS_ID, 0, NULL, -100);

				// localize groups
				_app_refreshgroups (hwnd, IDC_RULE_APPS_ID);

				// sort column
				_app_listviewsort (hwnd, IDC_RULE_APPS_ID, -1, FALSE);
			}

			// app group
			if (GetDlgItem (hwnd, IDC_APP_NAME))
			{
				_r_ctrl_setstringformat (hwnd, IDC_APP_NAME, L"%s:", _r_locale_getstring (IDS_SETTINGS_GENERAL));
			}

			// app path
			if (GetDlgItem (hwnd, IDC_APP_PATH))
			{
				_r_ctrl_setstringformat (hwnd, IDC_APP_PATH, L"%s:", _r_locale_getstring (IDS_FILEPATH));
			}

			// app icon
			if (GetDlgItem (hwnd, IDC_APP_ICON_ID))
			{
				hicon_large = _app_getfileiconsafe (context->ptr_app->app_hash);

				SendDlgItemMessage (hwnd, IDC_APP_ICON_ID, STM_SETICON, (WPARAM)hicon_large, 0);
			}

			// app display name
			if (GetDlgItem (hwnd, IDC_APP_NAME_ID))
			{
				_r_ctrl_setstring (hwnd, IDC_APP_NAME_ID, _app_getappdisplayname (context->ptr_app, TRUE));

				SendDlgItemMessage (hwnd, IDC_APP_NAME_ID, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, 0);
			}

			// app signature
			if (GetDlgItem (hwnd, IDC_APP_SIGNATURE_ID))
			{
				PR_STRING string;

				string = _app_getappinfoparam2 (context->ptr_app->app_hash, INFO_SIGNATURE_STRING);

				_r_ctrl_setstringformat (hwnd, IDC_APP_SIGNATURE_ID, L"%s: %s", _r_locale_getstring (IDS_SIGNATURE), _r_obj_getstringordefault (string, _r_locale_getstring (IDS_SIGN_UNSIGNED)));

				SendDlgItemMessage (hwnd, IDC_APP_SIGNATURE_ID, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, 0);

				if (string)
					_r_obj_dereference (string);
			}

			// app path
			if (GetDlgItem (hwnd, IDC_APP_PATH_ID))
			{
				_r_ctrl_setstring (hwnd, IDC_APP_PATH_ID, _r_obj_getstring (context->ptr_app->real_path));
			}

			// app rules
			if (GetDlgItem (hwnd, IDC_APP_RULES_ID))
			{
				// configure listview
				_app_listviewsetview (hwnd, IDC_APP_RULES_ID);

				_r_listview_setstyle (hwnd, IDC_APP_RULES_ID, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_CHECKBOXES, TRUE);

				_r_listview_addcolumn (hwnd, IDC_APP_RULES_ID, 0, _r_locale_getstring (IDS_NAME), 0, LVCFMT_LEFT);

				_r_listview_addgroup (hwnd, IDC_APP_RULES_ID, 0, _r_locale_getstring (IDS_TRAY_SYSTEM_RULES), 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
				_r_listview_addgroup (hwnd, IDC_APP_RULES_ID, 1, _r_locale_getstring (IDS_TRAY_USER_RULES), 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);

				// initialize
				PITEM_RULE ptr_rule;
				BOOLEAN is_enabled;

				_r_queuedlock_acquireshared (&lock_rules);

				for (SIZE_T i = 0; i < _r_obj_getlistsize (rules_list); i++)
				{
					ptr_rule = _r_obj_getlistitem (rules_list, i);

					if (!ptr_rule || ptr_rule->type != DATA_RULE_USER)
						continue;

					// check for services
					is_enabled = ((ptr_rule->is_forservices && (context->ptr_app->app_hash == config.ntoskrnl_hash || context->ptr_app->app_hash == config.svchost_hash)) || _r_obj_findhashtable (ptr_rule->apps, context->ptr_app->app_hash));

					_app_setcheckboxlock (hwnd, IDC_APP_RULES_ID, TRUE);

					_r_listview_additem_ex (hwnd, IDC_APP_RULES_ID, 0, LPSTR_TEXTCALLBACK, I_IMAGECALLBACK, I_GROUPIDCALLBACK, _app_createlistviewparam (i));
					_r_listview_setitemcheck (hwnd, IDC_APP_RULES_ID, 0, is_enabled);

					_app_setcheckboxlock (hwnd, IDC_APP_RULES_ID, FALSE);
				}

				_r_queuedlock_releaseshared (&lock_rules);

				// resize column
				_r_listview_setcolumn (hwnd, IDC_APP_RULES_ID, 0, NULL, -100);

				// localize groups
				_app_refreshgroups (hwnd, IDC_APP_RULES_ID);

				// sort column
				_app_listviewsort (hwnd, IDC_APP_RULES_ID, -1, FALSE);
			}

			// hints
			if (GetDlgItem (hwnd, IDC_RULE_HINT))
			{
				_r_ctrl_setstring (hwnd, IDC_RULE_HINT, _r_locale_getstring (IDS_RULE_HINT));
			}

			if (GetDlgItem (hwnd, IDC_RULE_APPS_HINT))
			{
				_r_ctrl_setstring (hwnd, IDC_RULE_APPS_HINT, _r_locale_getstring (IDS_RULE_APPS_HINT));
			}

			if (GetDlgItem (hwnd, IDC_APP_HINT))
			{
				_r_ctrl_setstring (hwnd, IDC_APP_HINT, _r_locale_getstring (IDS_APP_HINT));
			}

			break;
		}

		case RM_INITIALIZE:
		{
			// apps
			if (GetDlgItem (hwnd, IDC_RULE_APPS_ID))
				_app_settabcounttitle (hwnd, IDC_RULE_APPS_ID);

			// rules
			if (GetDlgItem (hwnd, IDC_APP_RULES_ID))
				_app_settabcounttitle (hwnd, IDC_APP_RULES_ID);

			break;
		}

		case WM_DESTROY:
		{
			SAFE_DELETE_ICON (hicon_large);
			break;
		}

		case WM_SIZE:
		{
			static INT listview_ids[] = {
				IDC_RULE_APPS_ID,
				IDC_APP_RULES_ID,
				IDC_RULE_REMOTE_ID,
				IDC_RULE_LOCAL_ID,
			};

			HWND hlistview;

			for (SIZE_T i = 0; i < RTL_NUMBER_OF (listview_ids); i++)
			{
				hlistview = GetDlgItem (hwnd, listview_ids[i]);

				if (hlistview)
				{
					_r_listview_setcolumn (hwnd, listview_ids[i], 0, NULL, -100);
				}
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

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, result);
					return result;
				}

				case NM_DBLCLK:
				{
					LPNMITEMACTIVATE lpnmlv = (LPNMITEMACTIVATE)lparam;

					if (lpnmlv->iItem == -1)
						break;

					INT command_id = 0;
					INT ctrl_id = (INT)(INT_PTR)(lpnmlv->hdr.idFrom);

					if (ctrl_id == IDC_RULE_REMOTE_ID)
					{
						command_id = IDC_RULE_REMOTE_EDIT;
					}
					else if (ctrl_id == IDC_RULE_LOCAL_ID)
					{
						command_id = IDC_RULE_LOCAL_EDIT;
					}
					else if (ctrl_id == IDC_RULE_APPS_ID || ctrl_id == IDC_APP_RULES_ID)
					{
						command_id = IDM_PROPERTIES;
					}

					if (command_id)
						PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (command_id, 0), 0);

					break;
				}

				case NM_RCLICK:
				{
					LPNMITEMACTIVATE lpnmlv;
					HMENU hsubmenu;
					BOOLEAN is_selected;
					INT listview_id;

					lpnmlv = (LPNMITEMACTIVATE)lparam;
					listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					if (
						listview_id != IDC_RULE_REMOTE_ID &&
						listview_id != IDC_RULE_LOCAL_ID &&
						listview_id != IDC_RULE_APPS_ID &&
						listview_id != IDC_APP_RULES_ID
						)
					{
						break;
					}

					hsubmenu = CreatePopupMenu ();

					if (!hsubmenu)
						break;

					is_selected = _r_listview_getselectedcount (hwnd, listview_id) != 0;

					if (listview_id == IDC_RULE_REMOTE_ID || listview_id == IDC_RULE_LOCAL_ID)
					{
						PR_STRING localized_string = NULL;
						BOOLEAN is_remote = (listview_id == IDC_RULE_REMOTE_ID);

						UINT id_add = is_remote ? IDC_RULE_REMOTE_ADD : IDC_RULE_LOCAL_ADD;
						UINT id_edit = is_remote ? IDC_RULE_REMOTE_EDIT : IDC_RULE_LOCAL_EDIT;
						UINT id_delete = is_remote ? IDC_RULE_REMOTE_DELETE : IDC_RULE_LOCAL_DELETE;

						_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_ADD), L"..."));
						AppendMenu (hsubmenu, MF_STRING, id_add, _r_obj_getstringorempty (localized_string));

						_r_obj_movereference (&localized_string, _r_obj_concatstrings (2, _r_locale_getstring (IDS_EDIT2), L"..."));
						AppendMenu (hsubmenu, MF_STRING, id_edit, _r_obj_getstringorempty (localized_string));

						AppendMenu (hsubmenu, MF_STRING, id_delete, _r_locale_getstring (IDS_DELETE));
						AppendMenu (hsubmenu, MF_SEPARATOR, 0, NULL);
						AppendMenu (hsubmenu, MF_STRING, IDM_COPY, _r_locale_getstring (IDS_COPY));

						if (context->ptr_rule->is_readonly)
						{
							_r_menu_enableitem (hsubmenu, id_add, MF_BYCOMMAND, FALSE);
							_r_menu_enableitem (hsubmenu, id_edit, MF_BYCOMMAND, FALSE);
							_r_menu_enableitem (hsubmenu, id_delete, MF_BYCOMMAND, FALSE);
						}

						if (!is_selected)
						{
							_r_menu_enableitem (hsubmenu, id_edit, MF_BYCOMMAND, FALSE);
							_r_menu_enableitem (hsubmenu, id_delete, MF_BYCOMMAND, FALSE);
							_r_menu_enableitem (hsubmenu, IDM_COPY, MF_BYCOMMAND, FALSE);
						}

						SAFE_DELETE_REFERENCE (localized_string);
					}
					else //if (listview_id == IDC_RULE_APPS_ID || listview_id == IDC_APP_RULES_ID)
					{
						AppendMenu (hsubmenu, MF_STRING, IDM_CHECK, _r_locale_getstring (IDS_CHECK));
						AppendMenu (hsubmenu, MF_STRING, IDM_UNCHECK, _r_locale_getstring (IDS_UNCHECK));
						AppendMenu (hsubmenu, MF_SEPARATOR, 0, NULL);
						AppendMenu (hsubmenu, MF_STRING, IDM_COPY, _r_locale_getstring (IDS_COPY));

						if (context->is_settorules && context->ptr_rule->type != DATA_RULE_USER)
						{
							_r_menu_enableitem (hsubmenu, IDM_CHECK, MF_BYCOMMAND, FALSE);
							_r_menu_enableitem (hsubmenu, IDM_UNCHECK, MF_BYCOMMAND, FALSE);
						}

						if (!is_selected)
						{
							_r_menu_enableitem (hsubmenu, IDM_CHECK, MF_BYCOMMAND, FALSE);
							_r_menu_enableitem (hsubmenu, IDM_UNCHECK, MF_BYCOMMAND, FALSE);
							_r_menu_enableitem (hsubmenu, IDM_COPY, MF_BYCOMMAND, FALSE);
						}
					}

					_r_menu_popup (hsubmenu, hwnd, NULL, TRUE);

					DestroyMenu (hsubmenu);

					break;
				}

				case LVN_DELETEITEM:
				{
					LPNMLISTVIEW lpnmlv;
					INT listview_id;

					lpnmlv = (LPNMLISTVIEW)lparam;
					listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					if (listview_id == IDC_RULE_APPS_ID || listview_id == IDC_APP_RULES_ID)
						_app_destroylistviewparam (lpnmlv->lParam);

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

					_app_listviewsort (hwnd, listview_id, lpnmlv->iSubItem, TRUE);

					break;
				}

				case LVN_ITEMCHANGING:
				{
					LPNMLISTVIEW lpnmlv;
					INT listview_id;

					lpnmlv = (LPNMLISTVIEW)lparam;
					listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					if ((lpnmlv->uChanged & LVIF_STATE) != 0)
					{
						if (listview_id == IDC_RULE_APPS_ID)
						{
							if ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (1) || ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (2)))
							{
								if (!_app_ischeckboxlocked (lpnmlv->hdr.hwndFrom) && context->ptr_rule->type != DATA_RULE_USER)
								{
									SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
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

					lpnmlv = (LPNMLISTVIEW)lparam;
					listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					if ((lpnmlv->uChanged & LVIF_STATE) != 0)
					{
						if (listview_id == IDC_RULE_APPS_ID || listview_id == IDC_APP_RULES_ID)
						{
							if ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (1) || ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (2)))
							{
								if (_app_ischeckboxlocked (lpnmlv->hdr.hwndFrom))
									break;

								_app_settabcounttitle (hwnd, listview_id);

								_app_refreshgroups (hwnd, listview_id);
								_app_listviewsort (hwnd, listview_id, -1, FALSE);
							}
						}
						else if (listview_id == IDC_RULE_REMOTE_ID || listview_id == IDC_RULE_LOCAL_ID)
						{
							if ((lpnmlv->uOldState & LVIS_SELECTED) != 0 || (lpnmlv->uNewState & LVIS_SELECTED) != 0)
							{
								if (!context->ptr_rule->is_readonly)
								{
									BOOLEAN is_selected = (lpnmlv->uNewState & LVIS_SELECTED) != 0;

									if (listview_id == IDC_RULE_REMOTE_ID)
									{
										_r_ctrl_enable (hwnd, IDC_RULE_REMOTE_EDIT, is_selected);
										_r_ctrl_enable (hwnd, IDC_RULE_REMOTE_DELETE, is_selected);
									}
									else// if (listview_id == IDC_RULE_LOCAL_ID)
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
					LPNMLVGETINFOTIP lpnmlv;
					PR_STRING string;
					INT listview_id;

					lpnmlv = (LPNMLVGETINFOTIP)lparam;
					listview_id = (INT)lpnmlv->hdr.idFrom;

					if (listview_id != IDC_RULE_APPS_ID && listview_id != IDC_APP_RULES_ID)
						break;

					string = _app_gettooltipbylparam (hwnd, listview_id, _app_getlistviewlparamvalue (hwnd, listview_id, lpnmlv->iItem));

					if (string)
					{
						_r_str_copy (lpnmlv->pszText, lpnmlv->cchTextMax, string->buffer);

						_r_obj_dereference (string);
					}

					break;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			INT ctrl_id = LOWORD (wparam);
			INT notify_code = HIWORD (wparam);

			if (notify_code == EN_MAXTEXT)
			{
				_r_ctrl_showballoontip (hwnd, ctrl_id, 0, NULL, SZ_MAXTEXT);
				return FALSE;
			}

			switch (ctrl_id)
			{
				case IDC_RULE_REMOTE_ADD:
				case IDC_RULE_LOCAL_ADD:
				case IDC_RULE_REMOTE_EDIT:
				case IDC_RULE_LOCAL_EDIT:
				{
					if (!_r_ctrl_isenabled (hwnd, ctrl_id))
						break;

					PR_STRING string;
					INT listview_id;
					INT item_id;
					INT current_length;

					listview_id = ((ctrl_id == IDC_RULE_REMOTE_ADD || ctrl_id == IDC_RULE_REMOTE_EDIT) ? IDC_RULE_REMOTE_ID : IDC_RULE_LOCAL_ID);

					if ((ctrl_id == IDC_RULE_REMOTE_EDIT || ctrl_id == IDC_RULE_LOCAL_EDIT))
					{
						// edit rule
						item_id = _r_listview_getnextselected (hwnd, listview_id, -1);

						if (item_id == -1)
							break;
					}
					else
					{
						// create new rule
						item_id = -1;
					}

					string = _app_getrulesfromlistview (hwnd, listview_id, item_id);

					current_length = (INT)_r_obj_getstringlength (string);

					_r_obj_dereference (string);

					if (ctrl_id == IDC_RULE_REMOTE_ADD || ctrl_id == IDC_RULE_LOCAL_ADD)
					{
						if (current_length >= RULE_RULE_CCH_MAX)
						{
							_r_show_errormessage (hwnd, NULL, ERROR_IMPLEMENTATION_LIMIT, NULL);

							return FALSE;
						}
					}

					ITEM_CONTEXT create_rule_context = {0};

					create_rule_context.hwnd = hwnd;
					create_rule_context.listview_id = listview_id;
					create_rule_context.item_id = item_id;
					create_rule_context.current_length = current_length;

					DialogBoxParam (NULL, MAKEINTRESOURCE (IDD_EDITOR_ADDRULE), hwnd, &AddRuleProc, (LPARAM)&create_rule_context);

					break;
				}

				case IDC_RULE_REMOTE_DELETE:
				case IDC_RULE_LOCAL_DELETE:
				{
					WCHAR message_text[256];
					INT listview_id = ((ctrl_id == IDC_RULE_REMOTE_DELETE) ? IDC_RULE_REMOTE_ID : IDC_RULE_LOCAL_ID);
					INT selected_count = _r_listview_getselectedcount (hwnd, listview_id);
					INT count;

					_r_str_printf (message_text, RTL_NUMBER_OF (message_text), _r_locale_getstring (IDS_QUESTION_DELETE), selected_count);

					if (!selected_count || _r_show_message (hwnd, MB_YESNO | MB_ICONEXCLAMATION, NULL, NULL, message_text) != IDYES)
						break;

					count = _r_listview_getitemcount (hwnd, listview_id) - 1;

					for (INT i = count; i != -1; i--)
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
					BOOLEAN new_val = (ctrl_id == IDM_CHECK);
					INT listview_id;
					INT item_id = -1;

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

					_app_setcheckboxlock (hwnd, listview_id, TRUE);

					while ((item_id = _r_listview_getnextselected (hwnd, listview_id, item_id)) != -1)
					{
						_r_listview_setitemcheck (hwnd, listview_id, item_id, new_val);
					}

					_app_setcheckboxlock (hwnd, listview_id, FALSE);

					_app_refreshgroups (hwnd, listview_id);
					_app_listviewsort (hwnd, listview_id, -1, FALSE);

					break;
				}

				case IDC_APP_EXPLORE_ID:
				{
					if (context->ptr_app->real_path)
					{
						if (_app_isappvalidpath (&context->ptr_app->real_path->sr))
							_r_shell_showfile (context->ptr_app->real_path->buffer);
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

					item_id = _r_listview_getnextselected (hwnd, listview_id, -1);

					if (item_id != -1)
					{
						index = _app_getlistviewlparamvalue (hwnd, listview_id, item_id);

						_app_showitembylparam (_r_app_gethwnd (), index, (listview_id == IDC_RULE_APPS_ID));
					}

					break;
				}

				case IDM_COPY:
				{
					HWND hlistview;
					R_STRINGBUILDER buffer;
					PR_STRING string;
					INT listview_id;
					INT item_id;

					hlistview = GetFocus ();

					if (!hlistview)
						break;

					listview_id = GetDlgCtrlID (hlistview);

					if (!listview_id || !_r_listview_getitemcount (hwnd, listview_id))
						break;

					_r_obj_initializestringbuilder (&buffer);

					item_id = -1;

					while ((item_id = _r_listview_getnextselected (hwnd, listview_id, item_id)) != -1)
					{
						string = _r_listview_getitemtext (hwnd, listview_id, item_id, 0);

						if (string)
						{
							_r_obj_appendstringbuilder2 (&buffer, string);
							_r_obj_appendstringbuilder (&buffer, L"\r\n");

							_r_obj_dereference (string);
						}
					}

					string = _r_obj_finalstringbuilder (&buffer);

					_r_str_trimstring2 (string, DIVIDER_TRIM, 0);

					_r_clipboard_set (hwnd, &string->sr);

					_r_obj_deletestringbuilder (&buffer);

					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

INT_PTR CALLBACK PropertiesProc (_In_ HWND hwnd, _In_ UINT msg, _In_ WPARAM wparam, _In_ LPARAM lparam)
{
	static R_LAYOUT_MANAGER layout_manager = {0};
	static PITEM_CONTEXT context = NULL;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			WCHAR title[128];
			INT tabs_count;

			context = (PITEM_CONTEXT)lparam;

			// configure tabs
			tabs_count = 0;

			if (context->is_settorules)
			{
				_r_str_copy (title, RTL_NUMBER_OF (title), _r_obj_getstringordefault (context->ptr_rule->name, SZ_RULE_NEW_TITLE));

				if (context->ptr_rule->is_readonly)
					_r_str_appendformat (title, RTL_NUMBER_OF (title), L" (%s)", SZ_RULE_INTERNAL_TITLE);

				_app_addeditortab (hwnd, IDS_SETTINGS_GENERAL, IDD_EDITOR_GENERAL, context, &tabs_count);
				_app_addeditortab (hwnd, IDS_RULE, IDD_EDITOR_RULE, context, &tabs_count);
				_app_addeditortab (hwnd, IDS_TAB_APPS, IDD_EDITOR_APPS, context, &tabs_count);

				// set state
				_r_ctrl_setstring (hwnd, IDC_ENABLE_CHK, _r_locale_getstring (IDS_ENABLE_CHK));

				CheckDlgButton (hwnd, IDC_ENABLE_CHK, context->ptr_rule->is_enabled ? BST_CHECKED : BST_UNCHECKED);
			}
			else
			{
				_r_str_copy (title, RTL_NUMBER_OF (title), _app_getappdisplayname (context->ptr_app, TRUE));

				_app_addeditortab (hwnd, IDS_SETTINGS_GENERAL, IDD_EDITOR_APPINFO, context, &tabs_count);
				_app_addeditortab (hwnd, IDS_TRAY_RULES, IDD_EDITOR_APPRULES, context, &tabs_count);

				// show state
				_r_ctrl_setstring (hwnd, IDC_ENABLE_CHK, _r_locale_getstring (IDS_ENABLE_APP_CHK));

				CheckDlgButton (hwnd, IDC_ENABLE_CHK, context->ptr_app->is_enabled ? BST_CHECKED : BST_UNCHECKED);
			}

			// initialize layout
			_r_layout_initializemanager (&layout_manager, hwnd);

			_r_window_restoreposition (hwnd, L"editor");

			_r_wnd_center (hwnd, GetParent (hwnd));

			// set window title
			SetWindowText (hwnd, title);

			_r_ctrl_setstring (hwnd, IDC_SAVE, _r_locale_getstring (IDS_SAVE));
			_r_ctrl_setstring (hwnd, IDC_CLOSE, _r_locale_getstring (IDS_CLOSE));

			_r_tab_selectitem (hwnd, IDC_TAB, _r_calc_clamp (context->page_id, 0, _r_tab_getitemcount (hwnd, IDC_TAB)));

			break;
		}

		case WM_DESTROY:
		{
			_r_window_saveposition (hwnd, L"editor");

			_r_layout_destroymanager (&layout_manager);

			break;
		}

		case WM_CLOSE:
		{
			EndDialog (hwnd, FALSE);
			break;
		}

		case WM_SIZE:
		{
			_r_layout_resize (&layout_manager, wparam);
			break;
		}

		case WM_GETMINMAXINFO:
		{
			_r_layout_resizeminimumsize (&layout_manager, lparam);
			break;
		}

		case WM_SETTINGCHANGE:
		{
			_r_wnd_changesettings (hwnd, wparam, lparam);
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

					if (hpage)
					{
						_r_tab_adjustchild (hwnd, IDC_TAB, hpage);

						ShowWindow (hpage, SW_SHOWNA);

						if (_r_wnd_isvisiblefull (hwnd)) // HACK!!!
							SetFocus (hpage);
					}

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
					PR_LIST rules = NULL;

					if (context->is_settorules)
					{
						HWND hpage_general = (HWND)_r_tab_getitemlparam (hwnd, IDC_TAB, 0);
						HWND hpage_rule = (HWND)_r_tab_getitemlparam (hwnd, IDC_TAB, 1);
						HWND hpage_apps = (HWND)_r_tab_getitemlparam (hwnd, IDC_TAB, 2);

						if (!hpage_general || !_r_ctrl_getstringlength (hpage_general, IDC_RULE_NAME_ID))
							return FALSE;

						PR_STRING string;

						context->ptr_rule->is_haveerrors = FALSE; // reset errors

						// do not change read-only rules
						if (!context->ptr_rule->is_readonly)
						{
							// name
							{
								string = _r_ctrl_getstring (hpage_general, IDC_RULE_NAME_ID);

								if (!string)
									return FALSE;

								_r_str_trimstring2 (string, DIVIDER_TRIM DIVIDER_RULE, 0);

								if (_r_obj_isstringempty2 (string))
								{
									_r_ctrl_showballoontip (hpage_general, IDC_RULE_NAME_ID, 0, NULL, _r_locale_getstring (IDS_STATUS_EMPTY));
									_r_obj_dereference (string);

									return FALSE;
								}

								_r_obj_movereference (&context->ptr_rule->name, string);
							}

							if (hpage_rule)
							{
								// rule (remote)
								if (context->ptr_rule->rule_remote)
									_r_obj_clearreference (&context->ptr_rule->rule_remote);

								string = _app_getrulesfromlistview (hpage_rule, IDC_RULE_REMOTE_ID, -1);

								if (!_r_obj_isstringempty (string))
								{
									_r_obj_movereference (&context->ptr_rule->rule_remote, string);
								}
								else
								{
									_r_obj_dereference (string);
								}

								// rule (local)
								if (context->ptr_rule->rule_local)
									_r_obj_clearreference (&context->ptr_rule->rule_local);

								string = _app_getrulesfromlistview (hpage_rule, IDC_RULE_LOCAL_ID, -1);

								if (!_r_obj_isstringempty (string))
								{
									_r_obj_movereference (&context->ptr_rule->rule_local, string);
								}
								else
								{
									_r_obj_dereference (string);
								}
							}

							context->ptr_rule->protocol = (UINT8)_r_combobox_getitemparam (hpage_general, IDC_RULE_PROTOCOL_ID, _r_combobox_getcurrentitem (hpage_general, IDC_RULE_PROTOCOL_ID));
							context->ptr_rule->af = (ADDRESS_FAMILY)_r_combobox_getitemparam (hpage_general, IDC_RULE_VERSION_ID, _r_combobox_getcurrentitem (hpage_general, IDC_RULE_VERSION_ID));

							context->ptr_rule->direction = (FWP_DIRECTION)_r_calc_clamp (_r_ctrl_isradiobuttonchecked (hpage_general, IDC_RULE_DIRECTION_OUTBOUND, IDC_RULE_DIRECTION_ANY) - IDC_RULE_DIRECTION_OUTBOUND, FWP_DIRECTION_OUTBOUND, FWP_DIRECTION_MAX);

							if (_r_ctrl_isradiobuttonchecked (hpage_general, IDC_RULE_ACTION_BLOCK, IDC_RULE_ACTION_ALLOW) == IDC_RULE_ACTION_BLOCK)
							{
								context->ptr_rule->action = FWP_ACTION_BLOCK;
							}
							else
							{
								context->ptr_rule->action = FWP_ACTION_PERMIT;
							}

							if (context->ptr_rule->type == DATA_RULE_USER)
								context->ptr_rule->weight = ((context->ptr_rule->action == FWP_ACTION_BLOCK) ? FW_WEIGHT_RULE_USER_BLOCK : FW_WEIGHT_RULE_USER);
						}

						// save rule apps
						if (context->ptr_rule->type == DATA_RULE_USER)
						{
							ULONG_PTR app_hash;

							_r_obj_clearhashtable (context->ptr_rule->apps);

							for (INT i = 0; i < _r_listview_getitemcount (hpage_apps, IDC_RULE_APPS_ID); i++)
							{
								if (!_r_listview_isitemchecked (hpage_apps, IDC_RULE_APPS_ID, i))
									continue;

								app_hash = _app_getlistviewlparamvalue (hpage_apps, IDC_RULE_APPS_ID, i);

								if (context->ptr_rule->is_forservices && (app_hash == config.ntoskrnl_hash || app_hash == config.svchost_hash))
									continue;

								if (_app_isappfound (app_hash))
								{
									_r_obj_addhashtableitem (context->ptr_rule->apps, app_hash, NULL);
								}
							}
						}

						// enable rule
						_app_ruleenable (context->ptr_rule, (IsDlgButtonChecked (hwnd, IDC_ENABLE_CHK) == BST_CHECKED), TRUE);

						rules = _r_obj_createlist (NULL);

						_r_obj_addlistitem (rules, context->ptr_rule);
					}
					else
					{
						HWND hpage_rule = (HWND)_r_tab_getitemlparam (hwnd, IDC_TAB, 1);

						context->ptr_app->is_haveerrors = FALSE; // reset errors
						context->ptr_app->is_enabled = !!(IsDlgButtonChecked (hwnd, IDC_ENABLE_CHK) == BST_CHECKED);

						rules = _r_obj_createlist (NULL);

						_r_obj_addlistitem (rules, context->ptr_app);

						if (hpage_rule)
						{
							PITEM_RULE ptr_rule;
							SIZE_T rule_idx;
							INT listview_id;
							INT item_id;
							BOOLEAN is_enable;

							_r_queuedlock_acquireshared (&lock_rules);

							for (INT i = 0; i < _r_listview_getitemcount (hpage_rule, IDC_APP_RULES_ID); i++)
							{
								rule_idx = _app_getlistviewlparamvalue (hpage_rule, IDC_APP_RULES_ID, i);
								ptr_rule = _r_obj_getlistitem (rules_list, rule_idx);

								if (!ptr_rule)
									continue;

								listview_id = _app_getlistviewbytype_id (ptr_rule->type);
								item_id = _app_getposition (_r_app_gethwnd (), listview_id, rule_idx);

								if (item_id != -1)
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
						if (!_r_obj_islistempty2 (rules) && _wfp_isfiltersinstalled ())
						{
							HANDLE hengine = _wfp_getenginehandle ();

							if (hengine)
							{
								if (context->is_settorules)
								{
									_wfp_create4filters (hengine, rules, __LINE__, FALSE);
								}
								else
								{
									_wfp_create3filters (hengine, rules, __LINE__, FALSE);
								}
							}
						}

						_r_obj_dereference (rules);
					}

					EndDialog (hwnd, TRUE);

					break;
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

