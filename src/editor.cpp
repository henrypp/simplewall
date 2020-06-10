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
			SetWindowText (hwnd, app.LocaleString (IDS_RULE, NULL).GetString ());

			if (pcontext->item_id != INVALID_INT)
				SetDlgItemText (hwnd, IDC_RULE_ID, _r_listview_getitemtext (pcontext->hwnd, pcontext->listview_id, pcontext->item_id, 0).GetString ());

			SetDlgItemText (hwnd, IDC_RULE_HINT, L"eg. 192.168.0.1\r\neg. [fe80::]\r\neg. 192.168.0.1:443\r\neg. [fe80::]:443\r\neg. 192.168.0.1-192.168.0.255\r\neg. 192.168.0.1-192.168.0.255:443\r\neg. 192.168.0.0/16\r\neg. fe80::/10\r\neg. 80\r\neg. 443\r\neg. 20-21\r\neg. 49152-65534");

			SetDlgItemText (hwnd, IDC_SAVE, app.LocaleString (pcontext->item_id != INVALID_INT ? IDS_SAVE : IDS_ADD, NULL).GetString ());
			SetDlgItemText (hwnd, IDC_CLOSE, app.LocaleString (IDS_CLOSE, NULL).GetString ());

			_r_wnd_addstyle (hwnd, IDC_SAVE, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (hwnd, IDC_CLOSE, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

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

					rstring rule_single = _r_ctrl_gettext (hwnd, IDC_RULE_ID);
					_r_str_trim (rule_single, DIVIDER_TRIM DIVIDER_RULE);

					if (rule_single.IsEmpty ())
						return FALSE;

					if (!_app_parserulestring (rule_single, NULL))
					{
						_r_ctrl_showtip (hwnd, IDC_RULE_ID, 0, NULL, app.LocaleString (IDS_STATUS_SYNTAX_ERROR, NULL).GetString ());
						_r_ctrl_enable (hwnd, IDC_SAVE, FALSE);

						return FALSE;
					}

					if (pcontext->item_id == INVALID_INT)
					{
						_r_listview_additem (pcontext->hwnd, pcontext->listview_id, INVALID_INT, 0, rule_single.GetString ());
					}
					else
					{
						_r_listview_setitem (pcontext->hwnd, pcontext->listview_id, pcontext->item_id, 0, rule_single.GetString ());
					}

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
			ptr_rule = (PITEM_RULE)lparam;

#ifndef _APP_NO_DARKTHEME
			_r_wnd_setdarktheme (hwnd);
#endif // _APP_NO_DARKTHEME

			EnableThemeDialogTexture (hwnd, ETDT_ENABLETAB);

			SetWindowPos (hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_FRAMECHANGED);

			// name
			if (GetDlgItem (hwnd, IDC_RULE_NAME_ID))
			{
				SetDlgItemText (hwnd, IDC_RULE_NAME, app.LocaleString (IDS_NAME, L":").GetString ());

				if (!_r_str_isempty (ptr_rule->pname))
					SetDlgItemText (hwnd, IDC_RULE_NAME_ID, ptr_rule->pname);

				SendDlgItemMessage (hwnd, IDC_RULE_NAME_ID, EM_LIMITTEXT, RULE_NAME_CCH_MAX - 1, 0);
				SendDlgItemMessage (hwnd, IDC_RULE_NAME_ID, EM_SETREADONLY, ptr_rule->is_readonly, 0);
			}

			rstring text_any = app.LocaleString (IDS_ANY, NULL);

			// direction
			if (GetDlgItem (hwnd, IDC_RULE_DIRECTION))
			{
				SetDlgItemText (hwnd, IDC_RULE_DIRECTION, app.LocaleString (IDS_DIRECTION, L":").GetString ());

				SetDlgItemText (hwnd, IDC_RULE_DIRECTION_OUTBOUND, app.LocaleString (IDS_DIRECTION_1, NULL).GetString ());
				SetDlgItemText (hwnd, IDC_RULE_DIRECTION_INBOUND, app.LocaleString (IDS_DIRECTION_2, NULL).GetString ());
				SetDlgItemText (hwnd, IDC_RULE_DIRECTION_ANY, text_any.GetString ());

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
				SetDlgItemText (hwnd, IDC_RULE_ACTION, app.LocaleString (IDS_ACTION, L":").GetString ());

				SetDlgItemText (hwnd, IDC_RULE_ACTION_BLOCK, app.LocaleString (IDS_ACTION_BLOCK, NULL).GetString ());
				SetDlgItemText (hwnd, IDC_RULE_ACTION_ALLOW, app.LocaleString (IDS_ACTION_ALLOW, NULL).GetString ());

				CheckRadioButton (hwnd, IDC_RULE_ACTION_BLOCK, IDC_RULE_ACTION_ALLOW, ptr_rule->is_block ? IDC_RULE_ACTION_BLOCK : IDC_RULE_ACTION_ALLOW);

				_r_ctrl_enable (hwnd, IDC_RULE_ACTION_ALLOW, !ptr_rule->is_readonly);
				_r_ctrl_enable (hwnd, IDC_RULE_ACTION_BLOCK, !ptr_rule->is_readonly);
			}

			// protocols
			if (GetDlgItem (hwnd, IDC_RULE_PROTOCOL_ID))
			{
				SetDlgItemText (hwnd, IDC_RULE_PROTOCOL, app.LocaleString (IDS_PROTOCOL, L":").GetString ());

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

				SendDlgItemMessage (hwnd, IDC_RULE_PROTOCOL_ID, CB_INSERTSTRING, (WPARAM)index, (LPARAM)text_any.GetString ());
				SendDlgItemMessage (hwnd, IDC_RULE_PROTOCOL_ID, CB_SETITEMDATA, (WPARAM)index, 0);

				if (ptr_rule->protocol == 0)
					SendDlgItemMessage (hwnd, IDC_RULE_PROTOCOL_ID, CB_SETCURSEL, (WPARAM)index, 0);

				index += 1;

				for (auto &proto : protos)
				{
					SendDlgItemMessage (hwnd, IDC_RULE_PROTOCOL_ID, CB_INSERTSTRING, (WPARAM)index, (LPARAM)_r_fmt (L"%s (%" TEXT (PRIu8) L")", _app_getprotoname (proto, AF_UNSPEC, SZ_UNKNOWN).GetString (), proto).GetString ());
					SendDlgItemMessage (hwnd, IDC_RULE_PROTOCOL_ID, CB_SETITEMDATA, (WPARAM)index, (LPARAM)proto);

					if (ptr_rule->protocol == proto)
						SendDlgItemMessage (hwnd, IDC_RULE_PROTOCOL_ID, CB_SETCURSEL, (WPARAM)index, 0);

					index += 1;
				}

				// unknown protocol
				if (SendDlgItemMessage (hwnd, IDC_RULE_PROTOCOL_ID, CB_GETCURSEL, 0, 0) == CB_ERR)
				{
					SendDlgItemMessage (hwnd, IDC_RULE_PROTOCOL_ID, CB_INSERTSTRING, (WPARAM)index, (LPARAM)_r_fmt (L"%s (%" TEXT (PRIu8) L")", _app_getprotoname (ptr_rule->protocol, AF_UNSPEC, SZ_UNKNOWN).GetString (), ptr_rule->protocol).GetString ());
					SendDlgItemMessage (hwnd, IDC_RULE_PROTOCOL_ID, CB_SETITEMDATA, (WPARAM)index, (LPARAM)ptr_rule->protocol);

					SendDlgItemMessage (hwnd, IDC_RULE_PROTOCOL_ID, CB_SETCURSEL, (WPARAM)index, 0);
				}

				_r_ctrl_enable (hwnd, IDC_RULE_PROTOCOL_ID, !ptr_rule->is_readonly);
			}

			// rule (remote)
			if (GetDlgItem (hwnd, IDC_RULE_REMOTE_ID))
			{
				SetDlgItemText (hwnd, IDC_RULE_REMOTE, app.LocaleString (IDS_RULE, L" (" SZ_DIRECTION_REMOTE L"):").GetString ());

				_r_listview_setstyle (hwnd, IDC_RULE_REMOTE_ID, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP, FALSE);
				_r_listview_addcolumn (hwnd, IDC_RULE_REMOTE_ID, 0, NULL, -100, 0);

				if (!_r_str_isempty (ptr_rule->prule_remote))
				{
					rstringvec rvc;
					_r_str_split (ptr_rule->prule_remote, INVALID_SIZE_T, DIVIDER_RULE[0], rvc);

					INT index = 0;

					for (auto &rule_single : rvc)
					{
						_r_str_trim (rule_single, DIVIDER_TRIM DIVIDER_RULE);

						_r_fastlock_acquireshared (&lock_checkbox);
						_r_listview_additem (hwnd, IDC_RULE_REMOTE_ID, index, 0, rule_single.GetString ());
						_r_fastlock_releaseshared (&lock_checkbox);

						index += 1;
					}

					_r_listview_setcolumn (hwnd, IDC_RULE_REMOTE_ID, 0, NULL, -100);
				}

				SetDlgItemText (hwnd, IDC_RULE_REMOTE_ADD, app.LocaleString (IDS_ADD, L"...").GetString ());
				SetDlgItemText (hwnd, IDC_RULE_REMOTE_EDIT, app.LocaleString (IDS_EDIT2, L"...").GetString ());
				SetDlgItemText (hwnd, IDC_RULE_REMOTE_DELETE, app.LocaleString (IDS_DELETE, NULL).GetString ());

				_r_ctrl_enable (hwnd, IDC_RULE_REMOTE_ADD, !ptr_rule->is_readonly);
				_r_ctrl_enable (hwnd, IDC_RULE_REMOTE_EDIT, FALSE);
				_r_ctrl_enable (hwnd, IDC_RULE_REMOTE_DELETE, FALSE);

				_r_wnd_addstyle (hwnd, IDC_RULE_REMOTE_ADD, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
				_r_wnd_addstyle (hwnd, IDC_RULE_REMOTE_EDIT, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
				_r_wnd_addstyle (hwnd, IDC_RULE_REMOTE_DELETE, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			}

			// rule (local)
			if (GetDlgItem (hwnd, IDC_RULE_LOCAL_ID))
			{
				SetDlgItemText (hwnd, IDC_RULE_LOCAL, app.LocaleString (IDS_RULE, L" (" SZ_DIRECTION_LOCAL L"):").GetString ());

				_r_listview_setstyle (hwnd, IDC_RULE_LOCAL_ID, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP, FALSE);
				_r_listview_addcolumn (hwnd, IDC_RULE_LOCAL_ID, 0, NULL, -100, 0);

				if (!_r_str_isempty (ptr_rule->prule_local))
				{
					rstringvec rvc;
					_r_str_split (ptr_rule->prule_local, INVALID_SIZE_T, DIVIDER_RULE[0], rvc);

					INT index = 0;

					for (auto &rule_single : rvc)
					{
						_r_str_trim (rule_single, DIVIDER_TRIM DIVIDER_RULE);

						_r_fastlock_acquireshared (&lock_checkbox);
						_r_listview_additem (hwnd, IDC_RULE_LOCAL_ID, index, 0, rule_single.GetString ());
						_r_fastlock_releaseshared (&lock_checkbox);

						index += 1;
					}

					_r_listview_setcolumn (hwnd, IDC_RULE_LOCAL_ID, 0, NULL, -100);
				}

				SetDlgItemText (hwnd, IDC_RULE_LOCAL_ADD, app.LocaleString (IDS_ADD, L"...").GetString ());
				SetDlgItemText (hwnd, IDC_RULE_LOCAL_EDIT, app.LocaleString (IDS_EDIT2, L"...").GetString ());
				SetDlgItemText (hwnd, IDC_RULE_LOCAL_DELETE, app.LocaleString (IDS_DELETE, NULL).GetString ());

				_r_ctrl_enable (hwnd, IDC_RULE_LOCAL_ADD, !ptr_rule->is_readonly);
				_r_ctrl_enable (hwnd, IDC_RULE_LOCAL_EDIT, FALSE);
				_r_ctrl_enable (hwnd, IDC_RULE_LOCAL_DELETE, FALSE);

				_r_wnd_addstyle (hwnd, IDC_RULE_LOCAL_ADD, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
				_r_wnd_addstyle (hwnd, IDC_RULE_LOCAL_EDIT, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
				_r_wnd_addstyle (hwnd, IDC_RULE_LOCAL_DELETE, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			}

			// apps
			if (GetDlgItem (hwnd, IDC_RULE_APPS_ID))
			{
				_app_listviewsetview (hwnd, IDC_RULE_APPS_ID);

				_r_listview_setstyle (hwnd, IDC_RULE_APPS_ID, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_CHECKBOXES, TRUE);

				_r_listview_addcolumn (hwnd, IDC_RULE_APPS_ID, 0, app.LocaleString (IDS_NAME, NULL).GetString (), 0, LVCFMT_LEFT);

				_r_listview_addgroup (hwnd, IDC_RULE_APPS_ID, 0, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
				_r_listview_addgroup (hwnd, IDC_RULE_APPS_ID, 1, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);
				_r_listview_addgroup (hwnd, IDC_RULE_APPS_ID, 2, L"", 0, LVGS_COLLAPSIBLE, LVGS_COLLAPSIBLE);

				// apps (apply to)
				for (auto &p : apps)
				{
					PR_OBJECT ptr_app_object = _r_obj2_reference (p.second);

					if (!ptr_app_object)
						continue;

					PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

					if (ptr_app)
					{
						INT group_id;

						if (ptr_app->type == DataAppUWP)
							group_id = 2;

						else if (ptr_app->type == DataAppService)
							group_id = 1;

						else
							group_id = 0;

						// check for services
						BOOLEAN is_enabled = (ptr_rule->apps.find (p.first) != ptr_rule->apps.end ()) || (ptr_rule->is_forservices && (p.first == config.ntoskrnl_hash || p.first == config.svchost_hash));

						_r_fastlock_acquireshared (&lock_checkbox);

						_r_listview_additem (hwnd, IDC_RULE_APPS_ID, 0, 0, _r_path_getfilename (ptr_app->display_name), ptr_app->icon_id, group_id, p.first);
						_r_listview_setitemcheck (hwnd, IDC_RULE_APPS_ID, 0, is_enabled);

						_r_fastlock_releaseshared (&lock_checkbox);
					}

					_r_obj2_dereference (ptr_app_object);
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
				SetDlgItemText (hwnd, IDC_RULE_HINT, app.LocaleString (IDS_RULE_HINT, NULL).GetString ());

			if (GetDlgItem (hwnd, IDC_RULE_APPS_HINT))
				SetDlgItemText (hwnd, IDC_RULE_APPS_HINT, app.LocaleString (IDS_RULE_APPS_HINT, NULL).GetString ());

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
						BOOLEAN is_remote = listview_id == IDC_RULE_REMOTE_ID;

						UINT id_add = is_remote ? IDC_RULE_REMOTE_ADD : IDC_RULE_LOCAL_ADD;
						UINT id_edit = is_remote ? IDC_RULE_REMOTE_EDIT : IDC_RULE_LOCAL_EDIT;
						UINT id_delete = is_remote ? IDC_RULE_REMOTE_DELETE : IDC_RULE_LOCAL_DELETE;

						AppendMenu (hsubmenu, MF_STRING, id_add, app.LocaleString (IDS_ADD, L"...").GetString ());
						AppendMenu (hsubmenu, MF_STRING, id_edit, app.LocaleString (IDS_EDIT2, L"...").GetString ());
						AppendMenu (hsubmenu, MF_STRING, id_delete, app.LocaleString (IDS_DELETE, NULL).GetString ());
						AppendMenu (hsubmenu, MF_SEPARATOR, 0, NULL);
						AppendMenu (hsubmenu, MF_STRING, IDM_COPY, app.LocaleString (IDS_COPY, NULL).GetString ());

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
					}
					else if (listview_id == IDC_RULE_APPS_ID)
					{
						AppendMenu (hsubmenu, MF_STRING, IDM_CHECK, app.LocaleString (IDS_CHECK, NULL).GetString ());
						AppendMenu (hsubmenu, MF_STRING, IDM_UNCHECK, app.LocaleString (IDS_UNCHECK, NULL).GetString ());
						AppendMenu (hsubmenu, MF_SEPARATOR, 0, NULL);
						AppendMenu (hsubmenu, MF_STRING, IDM_PROPERTIES, app.LocaleString (IDS_SHOWINLIST, NULL).GetString ());
						AppendMenu (hsubmenu, MF_SEPARATOR, 0, NULL);
						AppendMenu (hsubmenu, MF_STRING, IDM_COPY, app.LocaleString (IDS_COPY, NULL).GetString ());

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

					_r_str_copy (lpnmlv->pszText, lpnmlv->cchTextMax, _app_gettooltip (hwnd, lpnmlv).GetString ());

					break;
				}

				case LVN_GETEMPTYMARKUP:
				{
					NMLVEMPTYMARKUP* lpnmlv = (NMLVEMPTYMARKUP*)lparam;

					lpnmlv->dwFlags = EMF_CENTERED;
					_r_str_copy (lpnmlv->szMarkup, RTL_NUMBER_OF (lpnmlv->szMarkup), app.LocaleString (IDS_STATUS_EMPTY, NULL).GetString ());

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

					if (!selected || app.ShowMessage (hwnd, MB_YESNO | MB_ICONEXCLAMATION, NULL, NULL, _r_fmt (app.LocaleString (IDS_QUESTION_DELETE, NULL).GetString (), selected).GetString ()) != IDYES)
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
							_app_showitem (app.GetHWND (), app_listview_id, _app_getposition (app.GetHWND (), app_listview_id, app_hash), INVALID_INT);
					}

					break;
				}

				case IDM_COPY:
				{
					HWND hlistview = GetFocus ();

					if (!hlistview)
						break;

					INT listview_id = GetDlgCtrlID (hlistview);

					if (!listview_id || !_r_listview_getitemcount (hwnd, listview_id, FALSE))
						break;

					INT item = INVALID_INT;

					rstring buffer;

					while ((item = (INT)SendDlgItemMessage (hwnd, listview_id, LVM_GETNEXTITEM, (WPARAM)item, LVNI_SELECTED)) != INVALID_INT)
					{
						buffer.AppendFormat (L"%s\r\n", _r_listview_getitemtext (hwnd, listview_id, item, 0).GetString ());
					}

					_r_str_trim (buffer, DIVIDER_TRIM);

					_r_clipboard_set (hwnd, buffer.GetString (), buffer.GetLength ());

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
	static PR_OBJECT ptr_rule_object = NULL;
	static PITEM_RULE ptr_rule = NULL;

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			ptr_rule_object = (PR_OBJECT)lparam;

			if (!ptr_rule_object)
			{
				EndDialog (hwnd, FALSE);
				return FALSE;
			}

			ptr_rule = (PITEM_RULE)ptr_rule_object->pdata;

			if (!ptr_rule)
			{
				EndDialog (hwnd, FALSE);
				return FALSE;
			}

			//// configure window
			_r_wnd_center (hwnd, GetParent (hwnd));

			//// configure tabs
			_r_tab_additem (hwnd, IDC_TAB, 0, app.LocaleString (IDS_SETTINGS_GENERAL, NULL).GetString (), I_IMAGENONE, (LPARAM)CreateDialogParam (app.GetHINSTANCE (), MAKEINTRESOURCE (IDD_EDITOR_GENERAL), hwnd, &EditorPagesProc, (LPARAM)ptr_rule));
			_r_tab_additem (hwnd, IDC_TAB, 1, app.LocaleString (IDS_RULE, NULL).GetString (), I_IMAGENONE, (LPARAM)CreateDialogParam (app.GetHINSTANCE (), MAKEINTRESOURCE (IDD_EDITOR_RULE), hwnd, &EditorPagesProc, (LPARAM)ptr_rule));
			_r_tab_additem (hwnd, IDC_TAB, 2, app.LocaleString (IDS_TAB_APPS, NULL).GetString (), I_IMAGENONE, (LPARAM)CreateDialogParam (app.GetHINSTANCE (), MAKEINTRESOURCE (IDD_EDITOR_APPS), hwnd, &EditorPagesProc, (LPARAM)ptr_rule));

#if !defined(_APP_NO_DARKTHEME)
			_r_wnd_setdarktheme (hwnd);
#endif // !_APP_NO_DARKTHEME

			// localize window
			{
				rstring title = app.LocaleString (IDS_EDITOR, NULL);

				if (!_r_str_isempty (ptr_rule->pname))
					title.AppendFormat (L" \"%s\"", !_r_str_isempty (ptr_rule->pname) ? ptr_rule->pname : SZ_RULE_NEW_TITLE);

				if (ptr_rule->is_readonly)
					title.AppendFormat (L" (%s)", SZ_RULE_INTERNAL_TITLE);

				SetWindowText (hwnd, title.GetString ());
			}

			SetDlgItemText (hwnd, IDC_ENABLE_CHK, app.LocaleString (IDS_ENABLE_CHK, NULL).GetString ());

			SetDlgItemText (hwnd, IDC_SAVE, app.LocaleString (IDS_SAVE, NULL).GetString ());
			SetDlgItemText (hwnd, IDC_CLOSE, app.LocaleString (IDS_CLOSE, NULL).GetString ());

			_r_wnd_addstyle (hwnd, IDC_SAVE, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);
			_r_wnd_addstyle (hwnd, IDC_CLOSE, app.IsClassicUI () ? WS_EX_STATICEDGE : 0, WS_EX_STATICEDGE, GWL_EXSTYLE);

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

						ShowWindow (hpage, SW_SHOW);
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

					ptr_rule->is_haveerrors = FALSE; // reset errors

					// do not change read-only rules
					if (!ptr_rule->is_readonly)
					{
						// name
						{
							rstring name = _r_ctrl_gettext (hpage_general, IDC_RULE_NAME_ID);

							_r_str_trim (name, DIVIDER_TRIM DIVIDER_RULE);
							_r_str_alloc (&ptr_rule->pname, min (name.GetLength (), RULE_NAME_CCH_MAX), name.GetString ());
						}

						// rule (remote)
						{
							rstring buffer;

							for (INT i = 0; i < _r_listview_getitemcount (hpage_rule, IDC_RULE_REMOTE_ID, FALSE); i++)
							{
								rstring rule = _r_listview_getitemtext (hpage_rule, IDC_RULE_REMOTE_ID, i, 0);
								_r_str_trim (rule, DIVIDER_TRIM DIVIDER_RULE);

								if (!rule.IsEmpty ())
								{
									// check maximum length of one rule
									if ((buffer.GetLength () + rule.GetLength ()) > RULE_RULE_CCH_MAX)
										break;

									buffer.AppendFormat (L"%s" DIVIDER_RULE, rule.GetString ());
								}
							}

							_r_str_trim (buffer, DIVIDER_TRIM DIVIDER_RULE);
							_r_str_alloc (&ptr_rule->prule_remote, buffer.GetLength (), buffer.GetString ());
						}

						// rule (local)
						{
							rstring buffer;

							for (INT i = 0; i < _r_listview_getitemcount (hpage_rule, IDC_RULE_LOCAL_ID, FALSE); i++)
							{
								rstring rule = _r_listview_getitemtext (hpage_rule, IDC_RULE_LOCAL_ID, i, 0);
								_r_str_trim (rule, DIVIDER_TRIM DIVIDER_RULE);

								if (!rule.IsEmpty ())
								{
									// check maximum length of one rule
									if ((buffer.GetLength () + rule.GetLength ()) > RULE_RULE_CCH_MAX)
										break;

									buffer.AppendFormat (L"%s" DIVIDER_RULE, rule.GetString ());
								}
							}

							_r_str_trim (buffer, DIVIDER_TRIM DIVIDER_RULE);
							_r_str_alloc (&ptr_rule->prule_local, buffer.GetLength (), buffer.GetString ());
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
						ptr_rule->apps.clear ();

						for (INT i = 0; i < _r_listview_getitemcount (hpage_apps, IDC_RULE_APPS_ID, FALSE); i++)
						{
							SIZE_T app_hash = _r_listview_getitemlparam (hpage_apps, IDC_RULE_APPS_ID, i);

							if (ptr_rule->is_forservices && (app_hash == config.ntoskrnl_hash || app_hash == config.svchost_hash))
								continue;

							if (_app_isappfound (app_hash))
							{
								if (_r_listview_isitemchecked (hpage_apps, IDC_RULE_APPS_ID, i))
									ptr_rule->apps[app_hash] = TRUE;
							}
						}
					}

					// enable rule
					_app_ruleenable (ptr_rule, (IsDlgButtonChecked (hwnd, IDC_ENABLE_CHK) == BST_CHECKED));

					// apply filter
					{
						OBJECTS_VEC rules;
						rules.push_back (ptr_rule_object);

						_wfp_create4filters (_wfp_getenginehandle (), rules, __LINE__);

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
