// simplewall
// Copyright (c) 2016-2021 Henry++

#include "global.h"

PR_STRING _app_gettrulesfromlistview (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ INT exclude_id)
{
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
			_r_obj_trimstring (string, DIVIDER_TRIM DIVIDER_RULE);

			if (!_r_obj_isstringempty (string))
			{
				// check maximum length of one rule
				if ((_r_obj_getstringlength (buffer.string) + _r_obj_getstringlength (string)) > RULE_RULE_CCH_MAX)
				{
					_r_obj_dereference (string);
					break;
				}

				_r_obj_appendstringbuilderformat (&buffer, L"%s" DIVIDER_RULE, string->buffer);
			}

			_r_obj_dereference (string);
		}
	}

	string = _r_obj_finalstringbuilder (&buffer);

	_r_obj_trimstring (string, DIVIDER_TRIM DIVIDER_RULE);

	return string;
}

VOID _app_setrulestolistview (_In_ HWND hwnd, _In_ INT ctrl_id, _In_ PR_STRING rule)
{
	INT index = 0;

	PR_STRING rule_part;
	R_STRINGREF remaining_part;

	_r_obj_initializestringref2 (&remaining_part, rule);

	while (remaining_part.length != 0)
	{
		rule_part = _r_str_splitatchar (&remaining_part, &remaining_part, DIVIDER_RULE[0]);

		if (rule_part)
		{
			if (!_r_obj_isstringempty (rule_part))
			{
				_r_spinlock_acquireshared (&lock_checkbox);
				_r_listview_additem (hwnd, ctrl_id, index, 0, rule_part->buffer);
				_r_spinlock_releaseshared (&lock_checkbox);

				index += 1;
			}

			_r_obj_dereference (rule_part);
		}
	}

	_r_listview_setcolumn (hwnd, ctrl_id, 0, NULL, -100);
}

INT_PTR CALLBACK AddRuleProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
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
					_r_ctrl_settext (hwnd, IDC_RULE_ID, string->buffer);

					_r_obj_dereference (string);
				}
			}

			_r_ctrl_settext (hwnd, IDC_RULE_HINT, L"eg. 192.168.0.1\r\neg. [fe80::]\r\neg. 192.168.0.1:443\r\neg. [fe80::]:443\r\neg. 192.168.0.1-192.168.0.255\r\neg. 192.168.0.1-192.168.0.255:443\r\neg. 192.168.0.0/16\r\neg. fe80::/10\r\neg. 80\r\neg. 443\r\neg. 20-21\r\neg. 49152-65534");

			_r_ctrl_settext (hwnd, IDC_SAVE, _r_locale_getstring (context->item_id != -1 ? IDS_SAVE : IDS_ADD));
			_r_ctrl_settext (hwnd, IDC_CLOSE, _r_locale_getstring (IDS_CLOSE));

			BOOLEAN is_classic = _r_app_isclassicui ();

			_r_wnd_addstyle (hwnd, IDC_SAVE, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (hwnd, IDC_CLOSE, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

			_r_ctrl_enable (hwnd, IDC_SAVE, _r_ctrl_gettextlength (hwnd, IDC_RULE_ID) != 0); // enable apply button

			break;
		}

		case WM_NCCREATE:
		{
			_r_wnd_enablenonclientscaling (hwnd);
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
				_r_ctrl_enable (hwnd, IDC_SAVE, _r_ctrl_gettextlength (hwnd, IDC_RULE_ID) != 0); // enable apply button
				return FALSE;
			}

			switch (ctrl_id)
			{
				case IDOK: // process Enter key
				case IDC_SAVE:
				{
					if (!_r_ctrl_isenabled (hwnd, IDC_SAVE))
						return FALSE;

					PR_STRING string = _r_ctrl_gettext (hwnd, IDC_RULE_ID);

					if (!string)
						return FALSE;

					_r_obj_trimstring (string, DIVIDER_TRIM DIVIDER_RULE);

					if (_r_obj_isstringempty (string))
					{
						_r_ctrl_showballoontip (hwnd, IDC_RULE_ID, 0, NULL, _r_locale_getstring (IDS_STATUS_EMPTY));
						_r_ctrl_enable (hwnd, IDC_SAVE, FALSE);

						_r_obj_dereference (string);

						return FALSE;
					}

					PR_STRING rule_single;
					R_STRINGREF remaining_part;

					_r_obj_initializestringref2 (&remaining_part, string);

					while (remaining_part.length != 0)
					{
						rule_single = _r_str_splitatchar (&remaining_part, &remaining_part, DIVIDER_RULE[0]);

						if (rule_single)
						{
							if (!_app_parserulestring (rule_single, NULL))
							{
								_r_ctrl_showballoontip (hwnd, IDC_RULE_ID, 0, NULL, _r_locale_getstring (IDS_STATUS_SYNTAX_ERROR));
								_r_ctrl_enable (hwnd, IDC_SAVE, FALSE);

								_r_obj_dereference (rule_single);

								return FALSE;
							}

							_r_listview_additem (context->hwnd, context->listview_id, -1, 0, rule_single->buffer);

							_r_obj_dereference (rule_single);
						}
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

INT_PTR CALLBACK PropertiesPagesProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static PITEM_CONTEXT context = NULL;
	static HICON hicon_large = NULL;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			BOOLEAN is_classic = _r_app_isclassicui ();

			context = (PITEM_CONTEXT)lparam;

			EnableThemeDialogTexture (hwnd, ETDT_ENABLETAB);

			SetWindowPos (hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_FRAMECHANGED);

			// name
			if (GetDlgItem (hwnd, IDC_RULE_NAME_ID))
			{
				_r_ctrl_settextformat (hwnd, IDC_RULE_NAME, L"%s:", _r_locale_getstring (IDS_NAME));

				if (!_r_obj_isstringempty (context->ptr_rule->name))
					_r_ctrl_settext (hwnd, IDC_RULE_NAME_ID, context->ptr_rule->name->buffer);

				SendDlgItemMessage (hwnd, IDC_RULE_NAME_ID, EM_LIMITTEXT, RULE_NAME_CCH_MAX - 1, 0);
				SendDlgItemMessage (hwnd, IDC_RULE_NAME_ID, EM_SETREADONLY, context->ptr_rule->is_readonly, 0);
			}

			// direction
			if (GetDlgItem (hwnd, IDC_RULE_DIRECTION))
			{
				_r_ctrl_settextformat (hwnd, IDC_RULE_DIRECTION, L"%s:", _r_locale_getstring (IDS_DIRECTION));

				_r_ctrl_settext (hwnd, IDC_RULE_DIRECTION_OUTBOUND, _r_locale_getstring (IDS_DIRECTION_1));
				_r_ctrl_settext (hwnd, IDC_RULE_DIRECTION_INBOUND, _r_locale_getstring (IDS_DIRECTION_2));
				_r_ctrl_settext (hwnd, IDC_RULE_DIRECTION_ANY, _r_locale_getstring (IDS_ANY));

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
				_r_ctrl_settextformat (hwnd, IDC_RULE_ACTION, L"%s:", _r_locale_getstring (IDS_ACTION));

				_r_ctrl_settext (hwnd, IDC_RULE_ACTION_BLOCK, _r_locale_getstring (IDS_ACTION_BLOCK));
				_r_ctrl_settext (hwnd, IDC_RULE_ACTION_ALLOW, _r_locale_getstring (IDS_ACTION_ALLOW));

				CheckRadioButton (hwnd, IDC_RULE_ACTION_BLOCK, IDC_RULE_ACTION_ALLOW, context->ptr_rule->is_block ? IDC_RULE_ACTION_BLOCK : IDC_RULE_ACTION_ALLOW);

				_r_ctrl_enable (hwnd, IDC_RULE_ACTION_ALLOW, !context->ptr_rule->is_readonly);
				_r_ctrl_enable (hwnd, IDC_RULE_ACTION_BLOCK, !context->ptr_rule->is_readonly);
			}

			// protocols
			if (GetDlgItem (hwnd, IDC_RULE_PROTOCOL_ID))
			{
				_r_ctrl_settextformat (hwnd, IDC_RULE_PROTOCOL, L"%s:", _r_locale_getstring (IDS_PROTOCOL));

				WCHAR format[256];

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

				INT index = 0;

				_r_combobox_insertitem (hwnd, IDC_RULE_PROTOCOL_ID, index, _r_locale_getstring (IDS_ANY));
				_r_combobox_setitemparam (hwnd, IDC_RULE_PROTOCOL_ID, index, 0);

				if (context->ptr_rule->protocol == 0)
					_r_combobox_setcurrentitem (hwnd, IDC_RULE_PROTOCOL_ID, index);

				index += 1;

				for (SIZE_T i = 0; i < RTL_NUMBER_OF (protos); i++)
				{
					_r_str_printf (format, RTL_NUMBER_OF (format), L"%s (%" PRIu8 L")", _app_getprotoname (protos[i], AF_UNSPEC, SZ_UNKNOWN), protos[i]);

					_r_combobox_insertitem (hwnd, IDC_RULE_PROTOCOL_ID, index, format);
					_r_combobox_setitemparam (hwnd, IDC_RULE_PROTOCOL_ID, index, (LPARAM)protos[i]);

					if (context->ptr_rule->protocol == protos[i])
						_r_combobox_setcurrentitem (hwnd, IDC_RULE_PROTOCOL_ID, index);

					index += 1;
				}

				// unknown protocol
				if (_r_combobox_getcurrentitem (hwnd, IDC_RULE_PROTOCOL_ID) == CB_ERR)
				{
					_r_str_printf (format, RTL_NUMBER_OF (format), L"%s (%" PRIu8 L")", _app_getprotoname (context->ptr_rule->protocol, AF_UNSPEC, SZ_UNKNOWN), context->ptr_rule->protocol);

					_r_combobox_insertitem (hwnd, IDC_RULE_PROTOCOL_ID, index, format);
					_r_combobox_setitemparam (hwnd, IDC_RULE_PROTOCOL_ID, index, (LPARAM)context->ptr_rule->protocol);

					_r_combobox_setcurrentitem (hwnd, IDC_RULE_PROTOCOL_ID, index);
				}

				_r_ctrl_enable (hwnd, IDC_RULE_PROTOCOL_ID, !context->ptr_rule->is_readonly);
			}

			// family (ports-only)
			if (GetDlgItem (hwnd, IDC_RULE_VERSION_ID))
			{
				_r_ctrl_settextformat (hwnd, IDC_RULE_VERSION, L"%s:", _r_locale_getstring (IDS_PORTVERSION));

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
				_r_ctrl_settextformat (hwnd, IDC_RULE_REMOTE, L"%s (" SZ_DIRECTION_REMOTE L"):", _r_locale_getstring (IDS_RULE));

				_r_listview_setstyle (hwnd, IDC_RULE_REMOTE_ID, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP, FALSE);
				_r_listview_addcolumn (hwnd, IDC_RULE_REMOTE_ID, 0, NULL, -100, 0);

				if (!_r_obj_isstringempty (context->ptr_rule->rule_remote))
				{
					_app_setrulestolistview (hwnd, IDC_RULE_REMOTE_ID, context->ptr_rule->rule_remote);

					_r_listview_setcolumn (hwnd, IDC_RULE_REMOTE_ID, 0, NULL, -100);
				}

				_r_ctrl_settextformat (hwnd, IDC_RULE_REMOTE_ADD, L"%s...", _r_locale_getstring (IDS_ADD));
				_r_ctrl_settextformat (hwnd, IDC_RULE_REMOTE_EDIT, L"%s...", _r_locale_getstring (IDS_EDIT2));
				_r_ctrl_settext (hwnd, IDC_RULE_REMOTE_DELETE, _r_locale_getstring (IDS_DELETE));

				_r_ctrl_enable (hwnd, IDC_RULE_REMOTE_ADD, !context->ptr_rule->is_readonly);
				_r_ctrl_enable (hwnd, IDC_RULE_REMOTE_EDIT, FALSE);
				_r_ctrl_enable (hwnd, IDC_RULE_REMOTE_DELETE, FALSE);

				_r_wnd_addstyle (hwnd, IDC_RULE_REMOTE_ADD, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
				_r_wnd_addstyle (hwnd, IDC_RULE_REMOTE_EDIT, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
				_r_wnd_addstyle (hwnd, IDC_RULE_REMOTE_DELETE, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			}

			// rule (local)
			if (GetDlgItem (hwnd, IDC_RULE_LOCAL_ID))
			{
				_r_ctrl_settextformat (hwnd, IDC_RULE_LOCAL, L"%s (" SZ_DIRECTION_LOCAL L"):", _r_locale_getstring (IDS_RULE));

				_r_listview_setstyle (hwnd, IDC_RULE_LOCAL_ID, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP, FALSE);
				_r_listview_addcolumn (hwnd, IDC_RULE_LOCAL_ID, 0, NULL, -100, 0);

				if (!_r_obj_isstringempty (context->ptr_rule->rule_local))
				{
					_app_setrulestolistview (hwnd, IDC_RULE_LOCAL_ID, context->ptr_rule->rule_local);

					_r_listview_setcolumn (hwnd, IDC_RULE_LOCAL_ID, 0, NULL, -100);
				}

				_r_ctrl_settextformat (hwnd, IDC_RULE_LOCAL_ADD, L"%s...", _r_locale_getstring (IDS_ADD));
				_r_ctrl_settextformat (hwnd, IDC_RULE_LOCAL_EDIT, L"%s...", _r_locale_getstring (IDS_EDIT2));
				_r_ctrl_settext (hwnd, IDC_RULE_LOCAL_DELETE, _r_locale_getstring (IDS_DELETE));

				_r_ctrl_enable (hwnd, IDC_RULE_LOCAL_ADD, !context->ptr_rule->is_readonly);
				_r_ctrl_enable (hwnd, IDC_RULE_LOCAL_EDIT, FALSE);
				_r_ctrl_enable (hwnd, IDC_RULE_LOCAL_DELETE, FALSE);

				_r_wnd_addstyle (hwnd, IDC_RULE_LOCAL_ADD, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
				_r_wnd_addstyle (hwnd, IDC_RULE_LOCAL_EDIT, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
				_r_wnd_addstyle (hwnd, IDC_RULE_LOCAL_DELETE, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
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
					INT group_id;
					BOOLEAN is_enabled;

					_r_spinlock_acquireshared (&lock_apps);

					while (_r_obj_enumhashtable (apps, &ptr_app, NULL, &enum_key))
					{
						if (ptr_app->type == DataAppUWP)
						{
							group_id = 2;
						}
						else if (ptr_app->type == DataAppService)
						{
							group_id = 1;
						}
						else
						{
							group_id = 0;
						}

						// check for services
						is_enabled = (_r_obj_findhashtable (context->ptr_rule->apps, ptr_app->app_hash)) || (context->ptr_rule->is_forservices && (ptr_app->app_hash == config.ntoskrnl_hash || ptr_app->app_hash == config.svchost_hash));

						_r_spinlock_acquireshared (&lock_checkbox);

						_r_listview_additemex (hwnd, IDC_RULE_APPS_ID, 0, 0, _app_getdisplayname (ptr_app, TRUE), ptr_app->icon_id, group_id, ptr_app->app_hash);
						_r_listview_setitemcheck (hwnd, IDC_RULE_APPS_ID, 0, is_enabled);

						_r_spinlock_releaseshared (&lock_checkbox);
					}

					_r_spinlock_releaseshared (&lock_apps);
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
				_r_ctrl_settextformat (hwnd, IDC_APP_NAME, L"%s:", _r_locale_getstring (IDS_SETTINGS_GENERAL));

			// app path
			if (GetDlgItem (hwnd, IDC_APP_PATH))
				_r_ctrl_settextformat (hwnd, IDC_APP_PATH, L"%s:", _r_locale_getstring (IDS_FILEPATH));

			// app icon
			if (GetDlgItem (hwnd, IDC_APP_ICON_ID))
			{
				_app_getappicon (context->ptr_app, FALSE, NULL, &hicon_large);

				SendDlgItemMessage (hwnd, IDC_APP_ICON_ID, STM_SETICON, (WPARAM)hicon_large, 0);
			}

			// app display name
			if (GetDlgItem (hwnd, IDC_APP_NAME_ID))
			{
				_r_ctrl_settext (hwnd, IDC_APP_NAME_ID, _app_getdisplayname (context->ptr_app, TRUE));

				SendDlgItemMessage (hwnd, IDC_APP_NAME_ID, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, 0);
			}

			// app signature
			if (GetDlgItem (hwnd, IDC_APP_SIGNATURE_ID))
			{
				PR_STRING signature_string = NULL;

				if (_r_config_getboolean (L"IsCertificatesEnabled", FALSE))
				{
					if (context->ptr_app->is_signed)
						signature_string = _app_getsignatureinfo (context->ptr_app);
				}

				_r_ctrl_settextformat (hwnd, IDC_APP_SIGNATURE_ID, L"%s: %s", _r_locale_getstring (IDS_SIGNATURE), _r_obj_getstringordefault (signature_string, _r_locale_getstring (IDS_SIGN_UNSIGNED)));

				SendDlgItemMessage (hwnd, IDC_APP_SIGNATURE_ID, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, 0);

				if (signature_string)
					_r_obj_dereference (signature_string);
			}

			// app path
			if (GetDlgItem (hwnd, IDC_APP_PATH_ID))
				_r_ctrl_settext (hwnd, IDC_APP_PATH_ID, _r_obj_getstring (context->ptr_app->real_path));

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
				WCHAR buffer[128] = {0};
				PITEM_RULE ptr_rule;
				BOOLEAN is_enabled;

				_r_spinlock_acquireshared (&lock_rules);

				for (SIZE_T i = 0; i < _r_obj_getarraysize (rules_arr); i++)
				{
					ptr_rule = _r_obj_getarrayitem (rules_arr, i);

					if (!ptr_rule || ptr_rule->type != DataRuleUser)
						continue;

					// check for services
					is_enabled = ((ptr_rule->is_forservices && (context->ptr_app->app_hash == config.ntoskrnl_hash || context->ptr_app->app_hash == config.svchost_hash)) || _r_obj_findhashtable (ptr_rule->apps, context->ptr_app->app_hash));

					if (ptr_rule->name)
						_r_str_copy (buffer, RTL_NUMBER_OF (buffer), ptr_rule->name->buffer);

					if (ptr_rule->is_readonly)
						_r_str_append (buffer, RTL_NUMBER_OF (buffer), SZ_RULE_INTERNAL_MENU);

					_r_spinlock_acquireshared (&lock_checkbox);

					_r_listview_additemex (hwnd, IDC_APP_RULES_ID, 0, 0, buffer, _app_getruleicon (ptr_rule), ptr_rule->is_readonly ? 0 : 1, i);
					_r_listview_setitemcheck (hwnd, IDC_APP_RULES_ID, 0, is_enabled);

					_r_spinlock_releaseshared (&lock_checkbox);
				}

				_r_spinlock_releaseshared (&lock_rules);

				// resize column
				_r_listview_setcolumn (hwnd, IDC_APP_RULES_ID, 0, NULL, -100);

				// localize groups
				_app_refreshgroups (hwnd, IDC_APP_RULES_ID);

				// sort column
				_app_listviewsort (hwnd, IDC_APP_RULES_ID, -1, FALSE);
			}

			// hints
			if (GetDlgItem (hwnd, IDC_RULE_HINT))
				_r_ctrl_settext (hwnd, IDC_RULE_HINT, _r_locale_getstring (IDS_RULE_HINT));

			if (GetDlgItem (hwnd, IDC_RULE_APPS_HINT))
				_r_ctrl_settext (hwnd, IDC_RULE_APPS_HINT, _r_locale_getstring (IDS_RULE_APPS_HINT));

			if (GetDlgItem (hwnd, IDC_APP_HINT))
				_r_ctrl_settext (hwnd, IDC_APP_HINT, _r_locale_getstring (IDS_APP_HINT));

			break;
		}

		case WM_DESTROY:
		{
			if (hicon_large)
			{
				DestroyIcon (hicon_large);
				hicon_large = NULL;
			}

			break;
		}

		case WM_SIZE:
		{
			HWND hlistview;
			INT listview_ids[] = {
				IDC_RULE_APPS_ID,
				IDC_APP_RULES_ID,
				IDC_RULE_REMOTE_ID,
				IDC_RULE_LOCAL_ID,
			};

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
					LONG_PTR result = _app_message_custdraw ((LPNMLVCUSTOMDRAW)lparam);

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
					LPNMITEMACTIVATE lpnmlv = (LPNMITEMACTIVATE)lparam;
					INT listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					if (
						listview_id != IDC_RULE_REMOTE_ID &&
						listview_id != IDC_RULE_LOCAL_ID &&
						listview_id != IDC_RULE_APPS_ID &&
						listview_id != IDC_APP_RULES_ID
						)
					{
						break;
					}

					HMENU hsubmenu = CreatePopupMenu ();

					if (!hsubmenu)
						break;

					BOOLEAN is_selected = _r_listview_getselectedcount (hwnd, listview_id) != 0;

					if (listview_id == IDC_RULE_REMOTE_ID || listview_id == IDC_RULE_LOCAL_ID)
					{
						PR_STRING localized_string = NULL;
						BOOLEAN is_remote = (listview_id == IDC_RULE_REMOTE_ID);

						UINT id_add = is_remote ? IDC_RULE_REMOTE_ADD : IDC_RULE_LOCAL_ADD;
						UINT id_edit = is_remote ? IDC_RULE_REMOTE_EDIT : IDC_RULE_LOCAL_EDIT;
						UINT id_delete = is_remote ? IDC_RULE_REMOTE_DELETE : IDC_RULE_LOCAL_DELETE;

						_r_obj_movereference (&localized_string, _r_format_string (L"%s...", _r_locale_getstring (IDS_ADD)));
						AppendMenu (hsubmenu, MF_STRING, id_add, _r_obj_getstringorempty (localized_string));

						_r_obj_movereference (&localized_string, _r_format_string (L"%s...", _r_locale_getstring (IDS_EDIT2)));
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
					else if (listview_id == IDC_RULE_APPS_ID || listview_id == IDC_APP_RULES_ID)
					{
						AppendMenu (hsubmenu, MF_STRING, IDM_CHECK, _r_locale_getstring (IDS_CHECK));
						AppendMenu (hsubmenu, MF_STRING, IDM_UNCHECK, _r_locale_getstring (IDS_UNCHECK));
						AppendMenu (hsubmenu, MF_SEPARATOR, 0, NULL);
						AppendMenu (hsubmenu, MF_STRING, IDM_COPY, _r_locale_getstring (IDS_COPY));

						if (context->is_settorules && context->ptr_rule->type != DataRuleUser)
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

				case LVN_COLUMNCLICK:
				{
					LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lparam;
					INT listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					_app_listviewsort (hwnd, listview_id, lpnmlv->iSubItem, TRUE);

					break;
				}

				case LVN_ITEMCHANGING:
				{
					LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lparam;
					INT listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					if ((lpnmlv->uChanged & LVIF_STATE) != 0)
					{
						if (listview_id == IDC_RULE_APPS_ID)
						{
							if ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (1) || ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (2)))
							{
								if (!_r_spinlock_islocked (&lock_checkbox) && context->ptr_rule->type != DataRuleUser)
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
					LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lparam;
					INT listview_id = (INT)(INT_PTR)lpnmlv->hdr.idFrom;

					if ((lpnmlv->uChanged & LVIF_STATE) != 0)
					{
						if (listview_id == IDC_RULE_APPS_ID || listview_id == IDC_APP_RULES_ID)
						{
							if ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (1) || ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (2)))
							{
								if (_r_spinlock_islocked (&lock_checkbox))
									break;

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
					LPNMLVGETINFOTIP lpnmlv = (LPNMLVGETINFOTIP)lparam;
					PR_STRING string = _app_gettooltip (hwnd, (INT)(INT_PTR)lpnmlv->hdr.idFrom, lpnmlv->iItem);

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
					INT listview_id = ((ctrl_id == IDC_RULE_REMOTE_ADD || ctrl_id == IDC_RULE_REMOTE_EDIT) ? IDC_RULE_REMOTE_ID : IDC_RULE_LOCAL_ID);
					INT item_id;
					INT current_length;

					if ((ctrl_id == IDC_RULE_REMOTE_EDIT || ctrl_id == IDC_RULE_LOCAL_EDIT))
					{
						// edit rule
						item_id = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);

						if (item_id == -1)
							break;
					}
					else
					{
						// create new rule
						item_id = -1;
					}

					string = _app_gettrulesfromlistview (hwnd, listview_id, item_id);

					current_length = (INT)_r_obj_getstringlength (string);

					_r_obj_dereference (string);

					if (ctrl_id == IDC_RULE_REMOTE_ADD || ctrl_id == IDC_RULE_LOCAL_ADD)
					{
						if (current_length >= RULE_RULE_CCH_MAX)
						{
							_r_show_errormessage (hwnd, NULL, ERROR_IMPLEMENTATION_LIMIT, NULL, NULL);

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

					_r_spinlock_acquireshared (&lock_checkbox);

					while ((item_id = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item_id, LVNI_SELECTED)) != -1)
						_r_listview_setitemcheck (hwnd, listview_id, item_id, new_val);

					_r_spinlock_releaseshared (&lock_checkbox);

					_app_refreshgroups (hwnd, listview_id);
					_app_listviewsort (hwnd, listview_id, -1, FALSE);

					break;
				}

				case IDC_APP_EXPLORE_ID:
				{
					_app_openappdirectory (context->ptr_app);
					break;
				}

				case IDM_PROPERTIES:
				{
					LPARAM item_param;
					INT listview_id;
					INT listview_id2;
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

					item_id = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);

					if (item_id != -1)
					{
						item_param = _r_listview_getitemlparam (hwnd, listview_id, item_id);

						if (listview_id == IDC_RULE_APPS_ID)
						{
							listview_id2 = PtrToInt (_app_getappinfobyhash (item_param, InfoListviewId));
						}
						else if (listview_id == IDC_APP_RULES_ID)
						{
							listview_id2 = PtrToInt (_app_getruleinfobyid (item_param, InfoListviewId));
						}
						else
						{
							break;
						}

						if (listview_id2)
							_app_showitem (_r_app_gethwnd (), listview_id2, _app_getposition (_r_app_gethwnd (), listview_id2, item_param), -1);
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

					while ((item_id = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item_id, LVNI_SELECTED)) != -1)
					{
						string = _r_listview_getitemtext (hwnd, listview_id, item_id, 0);

						if (string)
						{
							if (!_r_obj_isstringempty (string))
								_r_obj_appendstringbuilderformat (&buffer, L"%s\r\n", string->buffer);

							_r_obj_dereference (string);
						}
					}

					string = _r_obj_finalstringbuilder (&buffer);

					_r_obj_trimstring (string, DIVIDER_TRIM);

					if (!_r_obj_isstringempty (string))
						_r_clipboard_set (hwnd, string->buffer, _r_obj_getstringlength (string));

					_r_obj_deletestringbuilder (&buffer);

					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

FORCEINLINE VOID _app_addeditortab (_In_ HWND hwnd, _In_ UINT locale_id, _In_ INT dlg_id, _In_ PITEM_CONTEXT context, _Inout_ PINT tabs_count)
{
	HWND htab = CreateDialogParam (NULL, MAKEINTRESOURCE (dlg_id), hwnd, &PropertiesPagesProc, (LPARAM)context);

	if (htab)
	{
		_r_tab_additem (hwnd, IDC_TAB, *tabs_count, _r_locale_getstring (locale_id), I_IMAGENONE, (LPARAM)htab);

		*tabs_count += 1;

		_r_tab_adjustchild (hwnd, IDC_TAB, htab);
	}
}

INT_PTR CALLBACK PropertiesProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static R_LAYOUT_MANAGER layout_manager = {0};
	static PITEM_CONTEXT context = NULL;
	static HICON hicon_small = NULL;
	static HICON hicon_large = NULL;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			WCHAR title[128];

			context = (PITEM_CONTEXT)lparam;

			// configure tabs
			INT tabs_count = 0;

			if (context->is_settorules)
			{
				_r_str_copy (title, RTL_NUMBER_OF (title), _r_obj_getstringordefault (context->ptr_rule->name, SZ_RULE_NEW_TITLE));

				if (context->ptr_rule->is_readonly)
					_r_str_appendformat (title, RTL_NUMBER_OF (title), L" (%s)", SZ_RULE_INTERNAL_TITLE);

				_app_addeditortab (hwnd, IDS_SETTINGS_GENERAL, IDD_EDITOR_GENERAL, context, &tabs_count);
				_app_addeditortab (hwnd, IDS_RULE, IDD_EDITOR_RULE, context, &tabs_count);
				_app_addeditortab (hwnd, IDS_TAB_APPS, IDD_EDITOR_APPS, context, &tabs_count);

				// set state
				_r_ctrl_settext (hwnd, IDC_ENABLE_CHK, _r_locale_getstring (IDS_ENABLE_CHK));

				CheckDlgButton (hwnd, IDC_ENABLE_CHK, context->ptr_rule->is_enabled ? BST_CHECKED : BST_UNCHECKED);
			}
			else
			{
				_r_str_copy (title, RTL_NUMBER_OF (title), _app_getdisplayname (context->ptr_app, TRUE));

				_app_addeditortab (hwnd, IDS_SETTINGS_GENERAL, IDD_EDITOR_APPINFO, context, &tabs_count);
				_app_addeditortab (hwnd, IDS_TRAY_RULES, IDD_EDITOR_APPRULES, context, &tabs_count);

				// set icon
				_app_getappicon (context->ptr_app, TRUE, NULL, &hicon_small);
				_app_getappicon (context->ptr_app, FALSE, NULL, &hicon_large);

				_r_wnd_seticon (hwnd, hicon_small, hicon_large);

				// show state
				_r_ctrl_settext (hwnd, IDC_ENABLE_CHK, _r_locale_getstring (IDS_ENABLE_APP_CHK));

				CheckDlgButton (hwnd, IDC_ENABLE_CHK, context->ptr_app->is_enabled ? BST_CHECKED : BST_UNCHECKED);
			}

			// initialize layout
			_r_layout_initializemanager (&layout_manager, hwnd);

			_r_window_restoreposition (hwnd, L"editor");

			_r_wnd_center (hwnd, GetParent (hwnd));

			// set window title
			SetWindowText (hwnd, title);

			_r_ctrl_settext (hwnd, IDC_SAVE, _r_locale_getstring (IDS_SAVE));
			_r_ctrl_settext (hwnd, IDC_CLOSE, _r_locale_getstring (IDS_CLOSE));

			BOOLEAN is_classic = _r_app_isclassicui ();

			_r_wnd_addstyle (hwnd, IDC_SAVE, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (hwnd, IDC_CLOSE, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

			_r_tab_selectitem (hwnd, IDC_TAB, _r_calc_clamp (context->page_id, 0, _r_tab_getitemcount (hwnd, IDC_TAB)));

			break;
		}

		case WM_NCCREATE:
		{
			_r_wnd_enablenonclientscaling (hwnd);
			break;
		}

		case WM_DESTROY:
		{
			if (hicon_small)
			{
				DestroyIcon (hicon_small);
				hicon_small = NULL;
			}

			if (hicon_large)
			{
				DestroyIcon (hicon_large);
				hicon_large = NULL;
			}

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
					HWND hpage = (HWND)_r_tab_getitemlparam (hwnd, IDC_TAB, -1);

					if (hpage)
						ShowWindowAsync (hpage, SW_HIDE);

					break;
				}

				case TCN_SELCHANGE:
				{
					HWND hpage = (HWND)_r_tab_getitemlparam (hwnd, IDC_TAB, -1);

					if (hpage)
					{
						_r_tab_adjustchild (hwnd, IDC_TAB, hpage);

						ShowWindowAsync (hpage, SW_SHOWNA);
						SetFocus (NULL);
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

						if (!hpage_general || !_r_ctrl_gettextlength (hpage_general, IDC_RULE_NAME_ID))
							return FALSE;

						PR_STRING string;

						context->ptr_rule->is_haveerrors = FALSE; // reset errors

						// do not change read-only rules
						if (!context->ptr_rule->is_readonly)
						{
							// name
							{
								string = _r_ctrl_gettext (hpage_general, IDC_RULE_NAME_ID);

								if (!string)
									return FALSE;

								_r_obj_trimstring (string, DIVIDER_TRIM DIVIDER_RULE);

								if (_r_obj_isstringempty (string))
								{
									_r_obj_dereference (string);
									return FALSE;
								}

								_r_obj_movereference (&context->ptr_rule->name, string);
							}

							// rule (remote)
							if (hpage_rule)
							{
								if (context->ptr_rule->rule_remote)
									_r_obj_clearreference (&context->ptr_rule->rule_remote);

								string = _app_gettrulesfromlistview (hpage_rule, IDC_RULE_REMOTE_ID, -1);

								if (!_r_obj_isstringempty (string))
								{
									_r_obj_movereference (&context->ptr_rule->rule_remote, string);
								}
								else
								{
									_r_obj_dereference (string);
								}
							}

							// rule (local)
							if (hpage_rule)
							{
								if (context->ptr_rule->rule_local)
									_r_obj_clearreference (&context->ptr_rule->rule_local);

								string = _app_gettrulesfromlistview (hpage_rule, IDC_RULE_LOCAL_ID, -1);

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
							context->ptr_rule->is_block = _r_ctrl_isradiobuttonchecked (hpage_general, IDC_RULE_ACTION_BLOCK, IDC_RULE_ACTION_ALLOW) == IDC_RULE_ACTION_BLOCK;

							if (context->ptr_rule->type == DataRuleUser)
								context->ptr_rule->weight = (context->ptr_rule->is_block ? FILTER_WEIGHT_CUSTOM_BLOCK : FILTER_WEIGHT_CUSTOM);
						}

						// save rule apps
						if (context->ptr_rule->type == DataRuleUser)
						{
							_r_obj_clearhashtable (context->ptr_rule->apps);

							for (INT i = 0; i < _r_listview_getitemcount (hpage_apps, IDC_RULE_APPS_ID); i++)
							{
								if (!_r_listview_isitemchecked (hpage_apps, IDC_RULE_APPS_ID, i))
									continue;

								ULONG_PTR app_hash = _r_listview_getitemlparam (hpage_apps, IDC_RULE_APPS_ID, i);

								if (context->ptr_rule->is_forservices && (app_hash == config.ntoskrnl_hash || app_hash == config.svchost_hash))
									continue;

								if (_app_getappitem (app_hash))
								{
									_app_addcachetablevalue (context->ptr_rule->apps, app_hash, NULL, 0);
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

							_r_spinlock_acquireshared (&lock_rules);

							for (INT i = 0; i < _r_listview_getitemcount (hpage_rule, IDC_APP_RULES_ID); i++)
							{
								rule_idx = _r_listview_getitemlparam (hpage_rule, IDC_APP_RULES_ID, i);

								ptr_rule = _r_obj_getarrayitem (rules_arr, rule_idx);

								if (!ptr_rule)
									continue;

								listview_id = _app_getlistview_id (ptr_rule->type);

								item_id = _app_getposition (_r_app_gethwnd (), listview_id, rule_idx);

								is_enable = _r_listview_isitemchecked (hpage_rule, IDC_APP_RULES_ID, i);

								_app_setruletoapp (_r_app_gethwnd (), ptr_rule, item_id, context->ptr_app, is_enable);
							}

							_r_spinlock_releaseshared (&lock_rules);
						}
					}

					// apply filter
					if (rules)
					{
						if (!_r_obj_islistempty (rules) && _wfp_isfiltersinstalled ())
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

