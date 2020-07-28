// simplewall
// Copyright (c) 2016-2020 Henry++

#include "global.hpp"

INT_PTR CALLBACK AddRuleProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static PITEM_CONTEXT pcontext;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			pcontext = (PITEM_CONTEXT)lparam;

#if !defined(_APP_NO_DARKTHEME)
			_r_wnd_setdarktheme (hwnd);
#endif // !_APP_NO_DARKTHEME

			_r_wnd_center (hwnd, GetParent (hwnd));

			// localize window
			SetWindowText (hwnd, _r_locale_getstring (IDS_RULE));

			if (pcontext->item_id != INVALID_INT)
			{
				PR_STRING itemText = _r_listview_getitemtext (pcontext->hwnd, pcontext->listview_id, pcontext->item_id, 0);

				if (itemText)
				{
					_r_ctrl_settext (hwnd, IDC_RULE_ID, itemText->Buffer);
					_r_obj_dereference (itemText);
				}
			}

			_r_ctrl_settext (hwnd, IDC_RULE_HINT, L"eg. 192.168.0.1\r\neg. [fe80::]\r\neg. 192.168.0.1:443\r\neg. [fe80::]:443\r\neg. 192.168.0.1-192.168.0.255\r\neg. 192.168.0.1-192.168.0.255:443\r\neg. 192.168.0.0/16\r\neg. fe80::/10\r\neg. 80\r\neg. 443\r\neg. 20-21\r\neg. 49152-65534");

			_r_ctrl_settext (hwnd, IDC_SAVE, _r_locale_getstring (pcontext->item_id != INVALID_INT ? IDS_SAVE : IDS_ADD));
			_r_ctrl_settext (hwnd, IDC_CLOSE, _r_locale_getstring (IDS_CLOSE));

			BOOLEAN is_classic = _r_app_isclassicui ();

			_r_wnd_addstyle (hwnd, IDC_SAVE, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (hwnd, IDC_CLOSE, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

			_r_ctrl_enable (hwnd, IDC_SAVE, (INT)SendDlgItemMessage (hwnd, IDC_RULE_ID, WM_GETTEXTLENGTH, 0, 0) > 0); // enable apply button

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
				_r_ctrl_enable (hwnd, IDC_SAVE, SendDlgItemMessage (hwnd, IDC_RULE_ID, WM_GETTEXTLENGTH, 0, 0) > 0); // enable apply button
				return FALSE;
			}

			switch (ctrl_id)
			{
				case IDOK: // process Enter key
				case IDC_SAVE:
				{
					if (!_r_ctrl_isenabled (hwnd, IDC_SAVE))
						return FALSE;

					PR_STRING ruleString = _r_ctrl_gettext (hwnd, IDC_RULE_ID);

					if (!ruleString)
						return FALSE;

					_r_str_trim (ruleString, DIVIDER_TRIM DIVIDER_RULE);

					if (_r_str_isempty (ruleString))
					{
						_r_obj_dereference (ruleString);
						return FALSE;
					}

					if (!_app_parserulestring (ruleString, NULL))
					{
						_r_ctrl_showballoontip (hwnd, IDC_RULE_ID, 0, NULL, _r_locale_getstring (IDS_STATUS_SYNTAX_ERROR));
						_r_ctrl_enable (hwnd, IDC_SAVE, FALSE);

						_r_obj_dereference (ruleString);

						return FALSE;
					}

					if (pcontext->item_id == INVALID_INT)
					{
						_r_listview_additem (pcontext->hwnd, pcontext->listview_id, INVALID_INT, 0, ruleString->Buffer);
					}
					else
					{
						_r_listview_setitem (pcontext->hwnd, pcontext->listview_id, pcontext->item_id, 0, ruleString->Buffer);
					}

					_r_obj_dereference (ruleString);

					[[fallthrough]];
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

INT_PTR CALLBACK EditorPagesProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static PITEM_RULE ptr_rule = NULL;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			BOOLEAN is_classic = _r_app_isclassicui ();

			ptr_rule = (PITEM_RULE)lparam;

#ifndef _APP_NO_DARKTHEME
			_r_wnd_setdarktheme (hwnd);
#endif // _APP_NO_DARKTHEME

			EnableThemeDialogTexture (hwnd, ETDT_ENABLETAB);

			SetWindowPos (hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_FRAMECHANGED);

			// name
			if (GetDlgItem (hwnd, IDC_RULE_NAME_ID))
			{
				_r_ctrl_settextformat (hwnd, IDC_RULE_NAME, L"%s:", _r_locale_getstring (IDS_NAME));

				if (!_r_str_isempty (ptr_rule->name))
					_r_ctrl_settext (hwnd, IDC_RULE_NAME_ID, ptr_rule->name->Buffer);

				SendDlgItemMessage (hwnd, IDC_RULE_NAME_ID, EM_LIMITTEXT, RULE_NAME_CCH_MAX - 1, 0);
				SendDlgItemMessage (hwnd, IDC_RULE_NAME_ID, EM_SETREADONLY, ptr_rule->is_readonly, 0);
			}

			// direction
			if (GetDlgItem (hwnd, IDC_RULE_DIRECTION))
			{
				_r_ctrl_settextformat (hwnd, IDC_RULE_DIRECTION, L"%s:", _r_locale_getstring (IDS_DIRECTION));

				_r_ctrl_settext (hwnd, IDC_RULE_DIRECTION_OUTBOUND, _r_locale_getstring (IDS_DIRECTION_1));
				_r_ctrl_settext (hwnd, IDC_RULE_DIRECTION_INBOUND, _r_locale_getstring (IDS_DIRECTION_2));
				_r_ctrl_settext (hwnd, IDC_RULE_DIRECTION_ANY, _r_locale_getstring (IDS_ANY));

				if (ptr_rule->direction == FWP_DIRECTION_OUTBOUND)
					CheckRadioButton (hwnd, IDC_RULE_DIRECTION_OUTBOUND, IDC_RULE_DIRECTION_ANY, IDC_RULE_DIRECTION_OUTBOUND);

				else if (ptr_rule->direction == FWP_DIRECTION_INBOUND)
					CheckRadioButton (hwnd, IDC_RULE_DIRECTION_OUTBOUND, IDC_RULE_DIRECTION_ANY, IDC_RULE_DIRECTION_INBOUND);

				else if (ptr_rule->direction == FWP_DIRECTION_MAX)
					CheckRadioButton (hwnd, IDC_RULE_DIRECTION_OUTBOUND, IDC_RULE_DIRECTION_ANY, IDC_RULE_DIRECTION_ANY);

				_r_ctrl_enable (hwnd, IDC_RULE_DIRECTION_OUTBOUND, !ptr_rule->is_readonly);
				_r_ctrl_enable (hwnd, IDC_RULE_DIRECTION_INBOUND, !ptr_rule->is_readonly);
				_r_ctrl_enable (hwnd, IDC_RULE_DIRECTION_ANY, !ptr_rule->is_readonly);
			}

			// action
			if (GetDlgItem (hwnd, IDC_RULE_ACTION))
			{
				_r_ctrl_settextformat (hwnd, IDC_RULE_ACTION, L"%s:", _r_locale_getstring (IDS_ACTION));

				_r_ctrl_settext (hwnd, IDC_RULE_ACTION_BLOCK, _r_locale_getstring (IDS_ACTION_BLOCK));
				_r_ctrl_settext (hwnd, IDC_RULE_ACTION_ALLOW, _r_locale_getstring (IDS_ACTION_ALLOW));

				CheckRadioButton (hwnd, IDC_RULE_ACTION_BLOCK, IDC_RULE_ACTION_ALLOW, ptr_rule->is_block ? IDC_RULE_ACTION_BLOCK : IDC_RULE_ACTION_ALLOW);

				_r_ctrl_enable (hwnd, IDC_RULE_ACTION_ALLOW, !ptr_rule->is_readonly);
				_r_ctrl_enable (hwnd, IDC_RULE_ACTION_BLOCK, !ptr_rule->is_readonly);
			}

			// protocols
			if (GetDlgItem (hwnd, IDC_RULE_PROTOCOL_ID))
			{
				_r_ctrl_settextformat (hwnd, IDC_RULE_PROTOCOL, L"%s:", _r_locale_getstring (IDS_PROTOCOL));

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
				WCHAR format[256];
				LPCWSTR protocolString;

				SendDlgItemMessage (hwnd, IDC_RULE_PROTOCOL_ID, CB_INSERTSTRING, (WPARAM)index, (LPARAM)_r_locale_getstring (IDS_ANY));
				SendDlgItemMessage (hwnd, IDC_RULE_PROTOCOL_ID, CB_SETITEMDATA, (WPARAM)index, 0);

				if (ptr_rule->protocol == 0)
					SendDlgItemMessage (hwnd, IDC_RULE_PROTOCOL_ID, CB_SETCURSEL, (WPARAM)index, 0);

				index += 1;

				for (auto protocolId : protos)
				{
					protocolString = _app_getprotoname (protocolId, AF_UNSPEC, SZ_UNKNOWN);

					_r_str_printf (format, RTL_NUMBER_OF (format), L"%s (%" TEXT (PRIu8) L")", protocolString, protocolId);

					SendDlgItemMessage (hwnd, IDC_RULE_PROTOCOL_ID, CB_INSERTSTRING, (WPARAM)index, (LPARAM)format);
					SendDlgItemMessage (hwnd, IDC_RULE_PROTOCOL_ID, CB_SETITEMDATA, (WPARAM)index, (LPARAM)protocolId);

					if (ptr_rule->protocol == protocolId)
						SendDlgItemMessage (hwnd, IDC_RULE_PROTOCOL_ID, CB_SETCURSEL, (WPARAM)index, 0);

					index += 1;
				}

				// unknown protocol
				if (SendDlgItemMessage (hwnd, IDC_RULE_PROTOCOL_ID, CB_GETCURSEL, 0, 0) == CB_ERR)
				{
					protocolString = _app_getprotoname (ptr_rule->protocol, AF_UNSPEC, SZ_UNKNOWN);

					_r_str_printf (format, RTL_NUMBER_OF (format), L"%s (%" TEXT (PRIu8) L")", protocolString, ptr_rule->protocol);

					SendDlgItemMessage (hwnd, IDC_RULE_PROTOCOL_ID, CB_INSERTSTRING, (WPARAM)index, (LPARAM)format);
					SendDlgItemMessage (hwnd, IDC_RULE_PROTOCOL_ID, CB_SETITEMDATA, (WPARAM)index, (LPARAM)ptr_rule->protocol);

					SendDlgItemMessage (hwnd, IDC_RULE_PROTOCOL_ID, CB_SETCURSEL, (WPARAM)index, 0);
				}

				_r_ctrl_enable (hwnd, IDC_RULE_PROTOCOL_ID, !ptr_rule->is_readonly);
			}

			// rule (remote)
			if (GetDlgItem (hwnd, IDC_RULE_REMOTE_ID))
			{
				_r_ctrl_settextformat (hwnd, IDC_RULE_REMOTE, L"%s (" SZ_DIRECTION_REMOTE L"):", _r_locale_getstring (IDS_RULE));

				_r_listview_setstyle (hwnd, IDC_RULE_REMOTE_ID, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP, FALSE);
				_r_listview_addcolumn (hwnd, IDC_RULE_REMOTE_ID, 0, NULL, -100, 0);

				if (!_r_str_isempty (ptr_rule->rule_remote))
				{
					INT index = 0;

					PR_STRING rulePart;
					R_STRINGREF remainingPart;

					_r_stringref_initializeex (&remainingPart, ptr_rule->rule_remote->Buffer, ptr_rule->rule_remote->Length);

					while (remainingPart.Length != 0)
					{
						rulePart = _r_str_splitatchar (&remainingPart, &remainingPart, DIVIDER_RULE[0], TRUE);

						if (rulePart)
						{
							if (!_r_str_isempty (rulePart))
							{
								_r_fastlock_acquireshared (&lock_checkbox);
								_r_listview_additem (hwnd, IDC_RULE_REMOTE_ID, index, 0, rulePart->Buffer);
								_r_fastlock_releaseshared (&lock_checkbox);

								index += 1;
							}

							_r_obj_dereference (rulePart);
						}
					}

					_r_listview_setcolumn (hwnd, IDC_RULE_REMOTE_ID, 0, NULL, -100);
				}

				_r_ctrl_settextformat (hwnd, IDC_RULE_REMOTE_ADD, L"%s...", _r_locale_getstring (IDS_ADD));
				_r_ctrl_settextformat (hwnd, IDC_RULE_REMOTE_EDIT, L"%s...", _r_locale_getstring (IDS_EDIT2));
				_r_ctrl_settext (hwnd, IDC_RULE_REMOTE_DELETE, _r_locale_getstring (IDS_DELETE));

				_r_ctrl_enable (hwnd, IDC_RULE_REMOTE_ADD, !ptr_rule->is_readonly);
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

				if (!_r_str_isempty (ptr_rule->rule_local))
				{
					INT index = 0;

					PR_STRING rulePart;
					R_STRINGREF remainingPart;

					_r_stringref_initializeex (&remainingPart, ptr_rule->rule_local->Buffer, ptr_rule->rule_local->Length);

					while (remainingPart.Length != 0)
					{
						rulePart = _r_str_splitatchar (&remainingPart, &remainingPart, DIVIDER_RULE[0], TRUE);

						if (rulePart)
						{
							if (!_r_str_isempty (rulePart))
							{
								_r_fastlock_acquireshared (&lock_checkbox);
								_r_listview_additem (hwnd, IDC_RULE_LOCAL_ID, index, 0, rulePart->Buffer);
								_r_fastlock_releaseshared (&lock_checkbox);

								index += 1;
							}

							_r_obj_dereference (rulePart);
						}
					}

					_r_listview_setcolumn (hwnd, IDC_RULE_LOCAL_ID, 0, NULL, -100);
				}

				_r_ctrl_settextformat (hwnd, IDC_RULE_LOCAL_ADD, L"%s...", _r_locale_getstring (IDS_ADD));
				_r_ctrl_settextformat (hwnd, IDC_RULE_LOCAL_EDIT, L"%s...", _r_locale_getstring (IDS_EDIT2));
				_r_ctrl_settext (hwnd, IDC_RULE_LOCAL_DELETE, _r_locale_getstring (IDS_DELETE));

				_r_ctrl_enable (hwnd, IDC_RULE_LOCAL_ADD, !ptr_rule->is_readonly);
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
					SIZE_T app_hash;
					INT group_id;
					BOOLEAN is_enabled;

					for (auto it = apps.begin (); it != apps.end (); ++it)
					{
						if (!it->second)
							continue;

						app_hash = it->first;
						ptr_app = (PITEM_APP)_r_obj_reference (it->second);

						if (ptr_app->type == DataAppUWP)
							group_id = 2;

						else if (ptr_app->type == DataAppService)
							group_id = 1;

						else
							group_id = 0;

						// check for services
						is_enabled = (ptr_rule->apps->find (app_hash) != ptr_rule->apps->end ()) || (ptr_rule->is_forservices && (app_hash == config.ntoskrnl_hash || app_hash == config.svchost_hash));

						_r_fastlock_acquireshared (&lock_checkbox);

						_r_listview_additemex (hwnd, IDC_RULE_APPS_ID, 0, 0, _r_obj_getstringordefault (ptr_app->display_name, SZ_EMPTY), ptr_app->icon_id, group_id, app_hash);
						_r_listview_setitemcheck (hwnd, IDC_RULE_APPS_ID, 0, is_enabled);

						_r_fastlock_releaseshared (&lock_checkbox);

						_r_obj_dereference (ptr_app);
					}
				}

				// resize column
				_r_listview_setcolumn (hwnd, IDC_RULE_APPS_ID, 0, NULL, -100);

				// localize groups
				_app_refreshgroups (hwnd, IDC_RULE_APPS_ID);

				// sort column
				_app_listviewsort (hwnd, IDC_RULE_APPS_ID, INVALID_INT, FALSE);
			}

			// hints
			if (GetDlgItem (hwnd, IDC_RULE_HINT))
				_r_ctrl_settext (hwnd, IDC_RULE_HINT, _r_locale_getstring (IDS_RULE_HINT));

			if (GetDlgItem (hwnd, IDC_RULE_APPS_HINT))
				_r_ctrl_settext (hwnd, IDC_RULE_APPS_HINT, _r_locale_getstring (IDS_RULE_APPS_HINT));

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lparam;

			switch (nmlp->code)
			{
				case NM_CUSTOMDRAW:
				{
					LONG_PTR result = _app_nmcustdraw_listview ((LPNMLVCUSTOMDRAW)lparam);

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, result);
					return result;
				}

				case NM_DBLCLK:
				{
					LPNMITEMACTIVATE lpnmlv = (LPNMITEMACTIVATE)lparam;

					if (lpnmlv->iItem == INVALID_INT)
						break;

					INT command_id = 0;
					INT ctrl_id = PtrToInt ((PVOID)lpnmlv->hdr.idFrom);

					if (ctrl_id == IDC_RULE_REMOTE_ID)
					{
						command_id = IDC_RULE_REMOTE_EDIT;
					}
					else if (ctrl_id == IDC_RULE_LOCAL_ID)
					{
						command_id = IDC_RULE_LOCAL_EDIT;
					}

					if (command_id)
						PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (command_id, 0), 0);

					break;
				}

				case NM_RCLICK:
				{
					LPNMITEMACTIVATE lpnmlv = (LPNMITEMACTIVATE)lparam;

					INT listview_id = PtrToInt ((PVOID)lpnmlv->hdr.idFrom);

					if (
						listview_id != IDC_RULE_REMOTE_ID &&
						listview_id != IDC_RULE_LOCAL_ID &&
						listview_id != IDC_RULE_APPS_ID
						)
					{
						break;
					}

					HMENU hsubmenu = CreatePopupMenu ();

					if (!hsubmenu)
						break;

					BOOLEAN is_selected = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETSELECTEDCOUNT, 0, 0) != 0;

					if (listview_id == IDC_RULE_REMOTE_ID || listview_id == IDC_RULE_LOCAL_ID)
					{
						PR_STRING localizedString = NULL;
						BOOLEAN is_remote = listview_id == IDC_RULE_REMOTE_ID;

						UINT id_add = is_remote ? IDC_RULE_REMOTE_ADD : IDC_RULE_LOCAL_ADD;
						UINT id_edit = is_remote ? IDC_RULE_REMOTE_EDIT : IDC_RULE_LOCAL_EDIT;
						UINT id_delete = is_remote ? IDC_RULE_REMOTE_DELETE : IDC_RULE_LOCAL_DELETE;

						_r_obj_movereference (&localizedString, _r_format_string (L"%s...", _r_locale_getstring (IDS_ADD)));
						AppendMenu (hsubmenu, MF_STRING, id_add, _r_obj_getstringorempty (localizedString));

						_r_obj_movereference (&localizedString, _r_format_string (L"%s...", _r_locale_getstring (IDS_EDIT2)));
						AppendMenu (hsubmenu, MF_STRING, id_edit, _r_obj_getstringorempty (localizedString));

						AppendMenu (hsubmenu, MF_STRING, id_delete, _r_locale_getstring (IDS_DELETE));
						AppendMenu (hsubmenu, MF_SEPARATOR, 0, NULL);
						AppendMenu (hsubmenu, MF_STRING, IDM_COPY, _r_locale_getstring (IDS_COPY));

						if (ptr_rule->is_readonly)
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

						SAFE_DELETE_REFERENCE (localizedString);
					}
					else if (listview_id == IDC_RULE_APPS_ID)
					{
						AppendMenu (hsubmenu, MF_STRING, IDM_CHECK, _r_locale_getstring (IDS_CHECK));
						AppendMenu (hsubmenu, MF_STRING, IDM_UNCHECK, _r_locale_getstring (IDS_UNCHECK));
						AppendMenu (hsubmenu, MF_SEPARATOR, 0, NULL);
						AppendMenu (hsubmenu, MF_STRING, IDM_PROPERTIES, _r_locale_getstring (IDS_SHOWINLIST));
						AppendMenu (hsubmenu, MF_SEPARATOR, 0, NULL);
						AppendMenu (hsubmenu, MF_STRING, IDM_COPY, _r_locale_getstring (IDS_COPY));

						if (ptr_rule->type != DataRuleCustom)
						{
							_r_menu_enableitem (hsubmenu, IDM_CHECK, MF_BYCOMMAND, FALSE);
							_r_menu_enableitem (hsubmenu, IDM_UNCHECK, MF_BYCOMMAND, FALSE);
						}

						if (!is_selected)
						{
							_r_menu_enableitem (hsubmenu, IDM_CHECK, MF_BYCOMMAND, FALSE);
							_r_menu_enableitem (hsubmenu, IDM_UNCHECK, MF_BYCOMMAND, FALSE);
							_r_menu_enableitem (hsubmenu, IDM_PROPERTIES, MF_BYCOMMAND, FALSE);
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

					INT listview_id = PtrToInt ((PVOID)lpnmlv->hdr.idFrom);

					_app_listviewsort (hwnd, listview_id, lpnmlv->iSubItem, TRUE);

					break;
				}

				case LVN_ITEMCHANGING:
				{
					LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)lparam;

					INT listview_id = PtrToInt ((PVOID)lpnmlv->hdr.idFrom);

					if ((lpnmlv->uChanged & LVIF_STATE) != 0)
					{
						if (listview_id == IDC_RULE_APPS_ID)
						{
							if ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (1) || ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (2)))
							{
								if (!_r_fastlock_islocked (&lock_checkbox) && ptr_rule->type != DataRuleCustom)
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

					INT listview_id = PtrToInt ((PVOID)lpnmlv->hdr.idFrom);

					if ((lpnmlv->uChanged & LVIF_STATE) != 0)
					{
						if (listview_id == IDC_RULE_APPS_ID)
						{
							if ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (1) || ((lpnmlv->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK (2)))
							{
								if (_r_fastlock_islocked (&lock_checkbox))
									break;

								_app_refreshgroups (hwnd, listview_id);
								_app_listviewsort (hwnd, listview_id, INVALID_INT, FALSE);
							}
						}
						else if (listview_id == IDC_RULE_REMOTE_ID || listview_id == IDC_RULE_LOCAL_ID)
						{
							if ((lpnmlv->uOldState & LVIS_SELECTED) != 0 || (lpnmlv->uNewState & LVIS_SELECTED) != 0)
							{
								if (!ptr_rule->is_readonly)
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
					PR_STRING string = _app_gettooltip (hwnd, lpnmlv);

					if (string)
					{
						_r_str_copy (lpnmlv->pszText, lpnmlv->cchTextMax, string->Buffer);
						_r_obj_dereference (string);
					}

					break;
				}

				case LVN_GETEMPTYMARKUP:
				{
					NMLVEMPTYMARKUP* lpnmlv = (NMLVEMPTYMARKUP*)lparam;

					lpnmlv->dwFlags = EMF_CENTERED;
					_r_str_copy (lpnmlv->szMarkup, RTL_NUMBER_OF (lpnmlv->szMarkup), _r_locale_getstring (IDS_STATUS_EMPTY));

					SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
					return TRUE;
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

					INT listview_id = ((ctrl_id == IDC_RULE_REMOTE_ADD || ctrl_id == IDC_RULE_REMOTE_EDIT) ? IDC_RULE_REMOTE_ID : IDC_RULE_LOCAL_ID);
					INT item_id;

					if ((ctrl_id == IDC_RULE_REMOTE_EDIT || ctrl_id == IDC_RULE_LOCAL_EDIT))
					{
						// edit rule
						item_id = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)INVALID_INT, LVNI_SELECTED);

						if (item_id == INVALID_INT)
							break;
					}
					else
					{
						// create new rule
						item_id = INVALID_INT;
					}

					PITEM_CONTEXT pcontext = (PITEM_CONTEXT)_r_mem_allocatezero (sizeof (ITEM_CONTEXT));

					pcontext->hwnd = hwnd;
					pcontext->listview_id = listview_id;
					pcontext->item_id = item_id;

					DialogBoxParam (NULL, MAKEINTRESOURCE (IDD_EDITOR_ADDRULE), hwnd, &AddRuleProc, (LPARAM)pcontext);

					_r_mem_free (pcontext);

					break;
				}

				case IDC_RULE_REMOTE_DELETE:
				case IDC_RULE_LOCAL_DELETE:
				{
					INT listview_id = ((ctrl_id == IDC_RULE_REMOTE_DELETE) ? IDC_RULE_REMOTE_ID : IDC_RULE_LOCAL_ID);
					INT selected = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETSELECTEDCOUNT, 0, 0);

					WCHAR messageText[256];
					_r_str_printf (messageText, RTL_NUMBER_OF (messageText), _r_locale_getstring (IDS_QUESTION_DELETE), selected);

					if (!selected || _r_show_message (hwnd, MB_YESNO | MB_ICONEXCLAMATION, NULL, NULL, messageText) != IDYES)
						break;

					INT count = _r_listview_getitemcount (hwnd, listview_id, FALSE) - 1;

					for (INT i = count; i != INVALID_INT; i--)
					{
						if ((UINT)SendDlgItemMessage (hwnd, listview_id, LVM_GETITEMSTATE, (WPARAM)i, LVNI_SELECTED) == LVNI_SELECTED)
							SendDlgItemMessage (hwnd, listview_id, LVM_DELETEITEM, (WPARAM)i, 0);
					}

					break;
				}

				case IDM_CHECK:
				case IDM_UNCHECK:
				{
					BOOLEAN new_val = (ctrl_id == IDM_CHECK);
					INT item = INVALID_INT;

					_r_fastlock_acquireshared (&lock_checkbox);

					while ((item = (INT)SendDlgItemMessage (hwnd, IDC_RULE_APPS_ID, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
						_r_listview_setitemcheck (hwnd, IDC_RULE_APPS_ID, item, new_val);

					_r_fastlock_releaseshared (&lock_checkbox);

					_app_refreshgroups (hwnd, IDC_RULE_APPS_ID);
					_app_listviewsort (hwnd, IDC_RULE_APPS_ID, INVALID_INT, FALSE);

					break;
				}

				case IDM_PROPERTIES:
				{
					INT item = INVALID_INT;

					while ((item = (INT)SendDlgItemMessage (hwnd, IDC_RULE_APPS_ID, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
					{
						SIZE_T app_hash = _r_listview_getitemlparam (hwnd, IDC_RULE_APPS_ID, item);
						INT app_listview_id = (INT)_app_getappinfo (app_hash, InfoListviewId);

						if (app_listview_id)
							_app_showitem (_r_app_gethwnd (), app_listview_id, _app_getposition (_r_app_gethwnd (), app_listview_id, app_hash), INVALID_INT);
					}

					break;
				}

				case IDM_COPY:
				{
					HWND hlistview;
					PR_STRING buffer;
					PR_STRING string;
					INT listview_id;
					INT item;

					hlistview = GetFocus ();

					if (!hlistview)
						break;

					listview_id = GetDlgCtrlID (hlistview);

					if (!listview_id || !_r_listview_getitemcount (hwnd, listview_id, FALSE))
						break;

					buffer = _r_obj_createstringbuilder (512 * sizeof (WCHAR));

					item = INVALID_INT;

					while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
					{
						string = _r_listview_getitemtext (hwnd, listview_id, item, 0);

						if (string)
						{
							if (!_r_str_isempty (string))
								_r_string_appendformat (&buffer, L"%s\r\n", string->Buffer);

							_r_obj_dereference (string);
						}
					}

					_r_str_trim (buffer, DIVIDER_TRIM);

					if (!_r_str_isempty (buffer))
						_r_clipboard_set (hwnd, buffer->Buffer, _r_obj_getstringlength (buffer));

					_r_obj_dereference (buffer);

					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

INT_PTR CALLBACK EditorProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static PITEM_RULE ptr_rule = NULL;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			ptr_rule = (PITEM_RULE)lparam;

			if (!ptr_rule)
			{
				EndDialog (hwnd, FALSE);
				return FALSE;
			}

			// configure window
			_r_wnd_center (hwnd, GetParent (hwnd));

			// configure tabs
			_r_tab_additem (hwnd, IDC_TAB, 0, _r_locale_getstring (IDS_SETTINGS_GENERAL), I_IMAGENONE, (LPARAM)CreateDialogParam (NULL, MAKEINTRESOURCE (IDD_EDITOR_GENERAL), hwnd, &EditorPagesProc, (LPARAM)ptr_rule));
			_r_tab_additem (hwnd, IDC_TAB, 1, _r_locale_getstring (IDS_RULE), I_IMAGENONE, (LPARAM)CreateDialogParam (NULL, MAKEINTRESOURCE (IDD_EDITOR_RULE), hwnd, &EditorPagesProc, (LPARAM)ptr_rule));
			_r_tab_additem (hwnd, IDC_TAB, 2, _r_locale_getstring (IDS_TAB_APPS), I_IMAGENONE, (LPARAM)CreateDialogParam (NULL, MAKEINTRESOURCE (IDD_EDITOR_APPS), hwnd, &EditorPagesProc, (LPARAM)ptr_rule));

#if !defined(_APP_NO_DARKTHEME)
			_r_wnd_setdarktheme (hwnd);
#endif // !_APP_NO_DARKTHEME

			// localize window
			{
				WCHAR title[128];
				_r_str_printf (title, RTL_NUMBER_OF (title), L"%s \"%s\"", _r_locale_getstring (IDS_EDITOR), _r_obj_getstringordefault (ptr_rule->name, SZ_RULE_NEW_TITLE));

				if (ptr_rule->is_readonly)
					_r_str_appendformat (title, RTL_NUMBER_OF (title), L" (%s)", SZ_RULE_INTERNAL_TITLE);

				SetWindowText (hwnd, title);
			}

			_r_ctrl_settext (hwnd, IDC_ENABLE_CHK, _r_locale_getstring (IDS_ENABLE_CHK));

			_r_ctrl_settext (hwnd, IDC_SAVE, _r_locale_getstring (IDS_SAVE));
			_r_ctrl_settext (hwnd, IDC_CLOSE, _r_locale_getstring (IDS_CLOSE));

			BOOLEAN is_classic = _r_app_isclassicui ();

			_r_wnd_addstyle (hwnd, IDC_SAVE, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (hwnd, IDC_CLOSE, is_classic ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

			// state
			CheckDlgButton (hwnd, IDC_ENABLE_CHK, ptr_rule->is_enabled ? BST_CHECKED : BST_UNCHECKED);

			_r_tab_selectitem (hwnd, IDC_TAB, 0);

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

		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lparam;

			switch (nmlp->code)
			{
				case TCN_SELCHANGING:
				{
					HWND hpage = (HWND)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

					if (hpage)
						ShowWindow (hpage, SW_HIDE);

					break;
				}

				case TCN_SELCHANGE:
				{
					HWND hpage = (HWND)_r_tab_getlparam (hwnd, IDC_TAB, INVALID_INT);

					if (hpage)
					{
						_r_tab_adjustchild (hwnd, IDC_TAB, hpage);

						ShowWindow (hpage, SW_SHOWNA);
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
					HWND hpage_general = (HWND)_r_tab_getlparam (hwnd, IDC_TAB, 0);
					HWND hpage_rule = (HWND)_r_tab_getlparam (hwnd, IDC_TAB, 1);
					HWND hpage_apps = (HWND)_r_tab_getlparam (hwnd, IDC_TAB, 2);

					if (!SendDlgItemMessage (hpage_general, IDC_RULE_NAME_ID, WM_GETTEXTLENGTH, 0, 0))
						return FALSE;

					PR_STRING buffer;
					PR_STRING string;

					ptr_rule->is_haveerrors = FALSE; // reset errors

					// do not change read-only rules
					if (!ptr_rule->is_readonly)
					{
						// name
						{
							PR_STRING name = _r_ctrl_gettext (hpage_general, IDC_RULE_NAME_ID);

							if (_r_str_isempty (name))
							{
								if (name)
									_r_obj_dereference (name);

								return FALSE;
							}

							_r_str_trim (name, DIVIDER_TRIM DIVIDER_RULE);
							_r_obj_movereference (&ptr_rule->name, name);
						}

						// rule (remote)
						{
							if (ptr_rule->rule_remote)
								_r_obj_clearreference (&ptr_rule->rule_remote);

							buffer = _r_obj_createstringbuilder (RULE_RULE_CCH_MAX * sizeof (WCHAR));

							for (INT i = 0; i < _r_listview_getitemcount (hpage_rule, IDC_RULE_REMOTE_ID, FALSE); i++)
							{
								string = _r_listview_getitemtext (hpage_rule, IDC_RULE_REMOTE_ID, i, 0);

								if (string)
								{
									_r_str_trim (string, DIVIDER_TRIM DIVIDER_RULE);

									if (!_r_str_isempty (string))
									{
										// check maximum length of one rule
										if ((_r_obj_getstringlength (buffer) + _r_obj_getstringlength (string)) > RULE_RULE_CCH_MAX)
											break;

										_r_string_appendformat (&buffer, L"%s" DIVIDER_RULE, _r_obj_getstring (string));
									}

									_r_obj_dereference (string);
								}
							}

							_r_str_trim (buffer, DIVIDER_TRIM DIVIDER_RULE);

							if (!_r_str_isempty (buffer))
								_r_obj_movereference (&ptr_rule->rule_remote, buffer);
							else
								_r_obj_clearreference (&buffer);
						}

						// rule (local)
						{
							if (ptr_rule->rule_local)
								_r_obj_clearreference (&ptr_rule->rule_local);

							buffer = _r_obj_createstringbuilder (RULE_RULE_CCH_MAX * sizeof (WCHAR));

							for (INT i = 0; i < _r_listview_getitemcount (hpage_rule, IDC_RULE_LOCAL_ID, FALSE); i++)
							{
								string = _r_listview_getitemtext (hpage_rule, IDC_RULE_LOCAL_ID, i, 0);

								if (string)
								{
									_r_str_trim (string, DIVIDER_TRIM DIVIDER_RULE);

									if (!_r_str_isempty (string))
									{
										// check maximum length of one rule
										if ((_r_obj_getstringlength (buffer) + _r_obj_getstringlength (string)) > RULE_RULE_CCH_MAX)
											break;

										_r_string_appendformat (&buffer, L"%s" DIVIDER_RULE, _r_obj_getstring (string));
									}

									_r_obj_dereference (string);
								}
							}

							_r_str_trim (buffer, DIVIDER_TRIM DIVIDER_RULE);

							if (!_r_str_isempty (buffer))
								_r_obj_movereference (&ptr_rule->rule_local, buffer);
							else
								_r_obj_clearreference (&buffer);
						}

						ptr_rule->protocol = (UINT8)SendDlgItemMessage (hpage_general, IDC_RULE_PROTOCOL_ID, CB_GETITEMDATA, SendDlgItemMessage (hpage_general, IDC_RULE_PROTOCOL_ID, CB_GETCURSEL, 0, 0), 0);

						ptr_rule->direction = _r_calc_clamp (FWP_DIRECTION, _r_ctrl_isradiobuttonchecked (hpage_general, IDC_RULE_DIRECTION_OUTBOUND, IDC_RULE_DIRECTION_ANY) - IDC_RULE_DIRECTION_OUTBOUND, (INT)FWP_DIRECTION_OUTBOUND, (INT)FWP_DIRECTION_MAX);
						ptr_rule->is_block = _r_ctrl_isradiobuttonchecked (hpage_general, IDC_RULE_ACTION_BLOCK, IDC_RULE_ACTION_ALLOW) == IDC_RULE_ACTION_BLOCK;

						if (ptr_rule->type == DataRuleCustom)
							ptr_rule->weight = (ptr_rule->is_block ? FILTER_WEIGHT_CUSTOM_BLOCK : FILTER_WEIGHT_CUSTOM);
					}

					// save rule apps
					if (ptr_rule->type == DataRuleCustom)
					{
						ptr_rule->apps->clear ();

						for (INT i = 0; i < _r_listview_getitemcount (hpage_apps, IDC_RULE_APPS_ID, FALSE); i++)
						{
							SIZE_T app_hash = _r_listview_getitemlparam (hpage_apps, IDC_RULE_APPS_ID, i);

							if (ptr_rule->is_forservices && (app_hash == config.ntoskrnl_hash || app_hash == config.svchost_hash))
								continue;

							if (_app_isappfound (app_hash))
							{
								if (_r_listview_isitemchecked (hpage_apps, IDC_RULE_APPS_ID, i))
									ptr_rule->apps->emplace (app_hash, TRUE);
							}
						}
					}

					// enable rule
					_app_ruleenable (ptr_rule, (IsDlgButtonChecked (hwnd, IDC_ENABLE_CHK) == BST_CHECKED));

					// apply filter
					HANDLE hengine = _wfp_getenginehandle ();

					if (hengine)
					{
						OBJECTS_RULE_VECTOR rules;
						rules.emplace_back (ptr_rule);

						_wfp_create4filters (hengine, &rules, __LINE__);

						// note: do not needed!
						//_app_dereferenceobjects (rules, &_app_dereferencerule);
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
