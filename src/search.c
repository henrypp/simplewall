// simplewall
// Copyright (c) 2012-2021 dmex
// Copyright (c) 2021-2024 Henry++

#include "global.h"

VOID _app_search_initialize (
	_Inout_ PSEARCH_CONTEXT context
)
{
	RECT rect;
	HTHEME htheme;
	LONG cx_border;
	HRESULT status;

	GetWindowRect (context->hwnd, &rect);

	cx_border = _r_dc_getsystemmetrics (SM_CXBORDER, context->dpi_value);

	// initialize borders
	context->cx_width = _r_dc_getdpi (20, context->dpi_value);
	context->cx_border = cx_border;
	context->dc_brush = GetStockObject (DC_BRUSH);

	if (IsThemeActive ())
	{
		htheme = _r_dc_openthemedata (context->hwnd, VSCLASS_EDIT, context->dpi_value);

		if (htheme)
		{
			status = GetThemeInt (htheme, 0, 0, TMT_BORDERSIZE, &context->cx_border);

			if (FAILED (status))
				context->cx_border = cx_border;

			CloseThemeData (htheme);
		}
	}
}

VOID _app_search_create (
	_In_ HWND hwnd
)
{
	PSEARCH_CONTEXT context;
	WCHAR buffer[128];

	context = _r_mem_allocate (sizeof (SEARCH_CONTEXT));

	context->hwnd = hwnd;
	context->dpi_value = _r_dc_getwindowdpi (hwnd);

	_app_search_initialize (context);

	_r_wnd_setcontext (context->hwnd, SHORT_MAX, context);

	// Subclass the Edit control window procedure.
	context->wnd_proc = (WNDPROC)GetWindowLongPtrW (context->hwnd, GWLP_WNDPROC);
	SetWindowLongPtrW (context->hwnd, GWLP_WNDPROC, (LONG_PTR)_app_search_subclass_proc);

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%s...", _r_locale_getstring (IDS_FIND));

	_r_edit_setcuebanner (context->hwnd, 0, buffer);

	// Initialize the theme parameters.
	_app_search_themechanged (hwnd, context);
}

VOID _app_search_initializeimages (
	_In_ PSEARCH_CONTEXT context,
	_In_ HWND hwnd
)
{
	HICON hicon_prev;
	HBITMAP hbitmap;
	NTSTATUS status;

	// initialize icons
	context->image_width = _r_dc_getsystemmetrics (SM_CXSMICON, context->dpi_value) + _r_dc_getdpi (4, context->dpi_value);
	context->image_height = _r_dc_getsystemmetrics (SM_CYSMICON, context->dpi_value) + _r_dc_getdpi (4, context->dpi_value);

	status = _r_res_loadimage (
		_r_sys_getimagebase (),
		L"PNG",
		MAKEINTRESOURCEW (IDP_SEARCH_LIGHT),
		&GUID_ContainerFormatPng,
		context->image_width,
		context->image_height,
		&hbitmap
	);

	if (NT_SUCCESS (status))
	{
		hicon_prev = context->hicon_light;

		context->hicon_light = _r_dc_bitmaptoicon (hbitmap, context->image_width, context->image_height);

		if (hicon_prev)
			DestroyIcon (hicon_prev);

		DeleteObject (hbitmap);
	}

	status = _r_res_loadimage (
		_r_sys_getimagebase (),
		L"PNG",
		MAKEINTRESOURCEW (IDP_SEARCH_DARK),
		&GUID_ContainerFormatPng,
		context->image_width,
		context->image_height,
		&hbitmap
	);

	if (NT_SUCCESS (status))
	{
		hicon_prev = context->hicon_dark;

		context->hicon_dark = _r_dc_bitmaptoicon (hbitmap, context->image_width, context->image_height);

		if (hicon_prev)
			DestroyIcon (hicon_prev);

		DeleteObject (hbitmap);
	}
}

VOID _app_search_themechanged (
	_In_ HWND hwnd,
	_In_ PSEARCH_CONTEXT context
)
{
	_app_search_initialize (context);
	_app_search_initializeimages (context, hwnd);

	// Reset the client area margins.
	_r_ctrl_settextmargin (hwnd, 0, 0, 0);

	// Refresh the non-client area.
	SetWindowPos (hwnd, NULL, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);

	// Force the edit control to update its non-client area.
	RedrawWindow (hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);
}

VOID _app_search_setvisible (
	_In_ HWND hwnd,
	_In_ HWND hsearch
)
{
	BOOLEAN is_visible;

	is_visible = _r_config_getboolean (L"IsShowSearchBar", TRUE);

	if (is_visible)
	{
		ShowWindow (hsearch, SW_SHOWNA);

		if (_r_wnd_isvisible (hwnd))
			SetFocus (hsearch);
	}
	else
	{
		_r_ctrl_setstring (hsearch, 0, L"");

		ShowWindow (hsearch, SW_HIDE);
	}
}

VOID _app_search_drawwindow (
	_Inout_ PSEARCH_CONTEXT context,
	_In_ LPCRECT wnd_rect
)
{
	if (!_r_theme_isenabled ())
		return;

	SetDCBrushColor (context->hdc, RGB (65, 65, 65));

	SelectObject (context->hdc, context->dc_brush);

	PatBlt (context->hdc, wnd_rect->left, wnd_rect->top, 1, wnd_rect->bottom - wnd_rect->top, PATCOPY);
	PatBlt (context->hdc, wnd_rect->right - 1, wnd_rect->top, 1, wnd_rect->bottom - wnd_rect->top, PATCOPY);
	PatBlt (context->hdc, wnd_rect->left, wnd_rect->top, wnd_rect->right - wnd_rect->left, 1, PATCOPY);
	PatBlt (context->hdc, wnd_rect->left, wnd_rect->bottom - 1, wnd_rect->right - wnd_rect->left, 1, PATCOPY);

	SetDCBrushColor (context->hdc, RGB (60, 60, 60));

	SelectObject (context->hdc, context->dc_brush);

	PatBlt (context->hdc, wnd_rect->left + 1, wnd_rect->top + 1, 1, wnd_rect->bottom - wnd_rect->top - 2, PATCOPY);
	PatBlt (context->hdc, wnd_rect->right - 2, wnd_rect->top + 1, 1, wnd_rect->bottom - wnd_rect->top - 2, PATCOPY);
	PatBlt (context->hdc, wnd_rect->left + 1, wnd_rect->top + 1, wnd_rect->right - wnd_rect->left - 2, 1, PATCOPY);
	PatBlt (context->hdc, wnd_rect->left + 1, wnd_rect->bottom - 2, wnd_rect->right - wnd_rect->left - 2, 1, PATCOPY);
}

VOID _app_search_drawbutton (
	_Inout_ PSEARCH_CONTEXT context,
	_In_ HWND hwnd,
	_In_ LPCRECT wnd_rect
)
{
	RECT btn_rect;

	_app_search_getbuttonrect (context, wnd_rect, &btn_rect);

	if (context->is_pushed)
	{
		_r_dc_fillrect (context->hdc, &btn_rect, _r_theme_isenabled () ? RGB (99, 99, 99) : RGB (153, 209, 255));
	}
	else if (context->is_hot)
	{
		_r_dc_fillrect (context->hdc, &btn_rect, _r_theme_isenabled () ? RGB (78, 78, 78) : RGB (205, 232, 255));
	}
	else
	{
		_r_dc_fillrect (context->hdc, &btn_rect, _r_theme_isenabled () ? RGB (60, 60, 60) : GetSysColor (COLOR_WINDOW));
	}

	DrawIconEx (
		context->hdc,
		btn_rect.left + 1,
		btn_rect.top,
		_r_theme_isenabled () ? context->hicon_light : context->hicon_dark,
		context->image_width,
		context->image_height,
		0,
		NULL,
		DI_NORMAL
	);
}

VOID _app_search_getbuttonrect (
	_In_ PSEARCH_CONTEXT context,
	_In_ LPCRECT wnd_rect,
	_Out_ PRECT btn_rect
)
{
	*btn_rect = *wnd_rect;

	btn_rect->left = ((btn_rect->right - context->cx_width) - context->cx_border - 1);
	btn_rect->top += context->cx_border;
	btn_rect->right -= context->cx_border;
	btn_rect->bottom -= context->cx_border;
}

BOOLEAN _app_search_isstringfound (
	_In_opt_ PR_STRINGREF string,
	_In_ PR_STRINGREF search_string,
	_Inout_ PITEM_LISTVIEW_CONTEXT context,
	_Inout_ PBOOLEAN is_changed
)
{
	if (!string)
	{
		if (context->is_hidden)
		{
			context->is_hidden = FALSE;

			*is_changed = TRUE;
		}

		return FALSE;
	}

	if (_r_str_findstring (string, search_string, TRUE) != SIZE_MAX)
	{
		if (context->is_hidden)
		{
			context->is_hidden = FALSE;

			*is_changed = TRUE;
		}

		return TRUE;
	}
	else
	{
		if (!context->is_hidden)
		{
			context->is_hidden = TRUE;

			*is_changed = TRUE;
		}
	}

	return FALSE;
}

BOOLEAN _app_search_applyfiltercallback (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_opt_ PR_STRING search_string
)
{
	PITEM_LISTVIEW_CONTEXT context;
	INT item_count;
	BOOLEAN is_changed = FALSE;

	item_count = _r_listview_getitemcount (hwnd, listview_id);

	if (!item_count)
		return FALSE;

	for (INT i = 0; i < item_count; i++)
	{
		context = (PITEM_LISTVIEW_CONTEXT)_r_listview_getitemlparam (hwnd, listview_id, i);

		if (!context)
			continue;

		if (_app_search_applyfilteritem (hwnd, listview_id, i, context, search_string))
			is_changed = TRUE;
	}

	if (is_changed)
		_app_listview_updateby_id (hwnd, listview_id, PR_UPDATE_NOSETVIEW | PR_UPDATE_FORCE);

	return is_changed;
}

BOOLEAN _app_search_applyfilteritem (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ INT item_id,
	_Inout_ PITEM_LISTVIEW_CONTEXT context,
	_In_opt_ PR_STRING search_string
)
{
	PITEM_APP ptr_app = NULL;
	PITEM_RULE ptr_rule = NULL;
	PITEM_NETWORK ptr_network = NULL;
	PITEM_LOG ptr_log = NULL;
	PR_STRING string;
	BOOLEAN is_changed = FALSE;

	// reset hidden state
	if (context->is_hidden)
	{
		context->is_hidden = FALSE;

		is_changed = TRUE;
	}

	if (!search_string)
		goto CleanupExit;

	switch (listview_id)
	{
		case IDC_APPS_PROFILE:
		case IDC_APPS_SERVICE:
		case IDC_APPS_UWP:
		case IDC_RULE_APPS_ID:
		{
			ptr_app = _app_getappitem (context->id_code);

			if (!ptr_app)
				goto CleanupExit;

			// path
			if (ptr_app->real_path)
			{
				if (_app_search_isstringfound (&ptr_app->real_path->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			// comment
			if (!_r_obj_isstringempty (ptr_app->comment))
			{
				if (_app_search_isstringfound (&ptr_app->comment->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			break;
		}

		case IDC_RULES_BLOCKLIST:
		case IDC_RULES_SYSTEM:
		case IDC_RULES_CUSTOM:
		case IDC_APP_RULES_ID:
		{
			ptr_rule = _app_getrulebyid (context->id_code);

			if (!ptr_rule)
				goto CleanupExit;

			if (ptr_rule->name)
			{
				if (_app_search_isstringfound (&ptr_rule->name->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			if (ptr_rule->rule_remote)
			{
				if (_app_search_isstringfound (&ptr_rule->rule_remote->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			if (ptr_rule->rule_local)
			{
				if (_app_search_isstringfound (&ptr_rule->rule_local->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			if (ptr_rule->protocol_str)
			{
				if (_app_search_isstringfound (&ptr_rule->protocol_str->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			// comment
			if (!_r_obj_isstringempty (ptr_rule->comment))
			{
				if (_app_search_isstringfound (&ptr_rule->comment->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			break;
		}

		case IDC_NETWORK:
		{
			ptr_network = _app_network_getitem (context->id_code);

			if (!ptr_network)
				goto CleanupExit;

			// path
			if (ptr_network->path)
			{
				if (_app_search_isstringfound (&ptr_network->path->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			// local address
			string = _InterlockedCompareExchangePointer (&ptr_network->local_addr_str, NULL, NULL);

			if (string)
			{
				if (_app_search_isstringfound (&string->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			// local host
			string = _InterlockedCompareExchangePointer (&ptr_network->local_host_str, NULL, NULL);

			if (string)
			{
				if (_app_search_isstringfound (&string->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			// remote address
			string = _InterlockedCompareExchangePointer (&ptr_network->remote_addr_str, NULL, NULL);

			if (string)
			{
				if (_app_search_isstringfound (&string->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			// remote host
			string = _InterlockedCompareExchangePointer (&ptr_network->remote_host_str, NULL, NULL);

			if (string)
			{
				if (_app_search_isstringfound (&string->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			// protocol
			if (ptr_network->protocol_str)
			{
				if (_app_search_isstringfound (&ptr_network->protocol_str->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			break;
		}

		case IDC_LOG:
		{
			ptr_log = _app_getlogitem (context->id_code);

			if (!ptr_log)
				goto CleanupExit;

			// path
			if (ptr_log->path)
			{
				if (_app_search_isstringfound (&ptr_log->path->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			// filter name
			if (ptr_log->filter_name)
			{
				if (_app_search_isstringfound (&ptr_log->filter_name->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			// layer name
			if (ptr_log->layer_name)
			{
				if (_app_search_isstringfound (&ptr_log->layer_name->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			// user name
			if (ptr_log->username)
			{
				if (_app_search_isstringfound (&ptr_log->username->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			// local address
			string = _InterlockedCompareExchangePointer (&ptr_log->local_addr_str, NULL, NULL);

			if (string)
			{
				if (_app_search_isstringfound (&string->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			// local host
			string = _InterlockedCompareExchangePointer (&ptr_log->local_host_str, NULL, NULL);

			if (string)
			{
				if (_app_search_isstringfound (&string->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			// remote address
			string = _InterlockedCompareExchangePointer (&ptr_log->remote_addr_str, NULL, NULL);

			if (string)
			{
				if (_app_search_isstringfound (&string->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			// remote host
			string = _InterlockedCompareExchangePointer (&ptr_log->remote_host_str, NULL, NULL);

			if (string)
			{
				if (_app_search_isstringfound (&string->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			// protocol
			if (ptr_log->protocol_str)
			{
				if (_app_search_isstringfound (&ptr_log->protocol_str->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			break;
		}
	}

CleanupExit:

	if (ptr_app)
		_r_obj_dereference (ptr_app);

	if (ptr_rule)
		_r_obj_dereference (ptr_rule);

	if (ptr_network)
		_r_obj_dereference (ptr_network);

	if (ptr_log)
		_r_obj_dereference (ptr_log);

	if (is_changed)
		_r_listview_setitem_ex (hwnd, listview_id, item_id, 0, NULL, I_IMAGECALLBACK, I_GROUPIDCALLBACK, 0);

	return is_changed;
}

VOID _app_search_applyfilter (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_opt_ PR_STRING search_string
)
{
	if (!((listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_LOG) || listview_id == IDC_RULE_APPS_ID || listview_id == IDC_APP_RULES_ID))
		return;

	_app_search_applyfiltercallback (hwnd, listview_id, search_string);
}

VOID _app_search_createbufferedcontext (
	_Inout_ PSEARCH_CONTEXT context,
	_In_ HDC hdc,
	_In_ LPCRECT buf_rect
)
{
	context->hdc = CreateCompatibleDC (hdc);

	if (!context->hdc)
		return;

	CopyRect (&context->rect, buf_rect);

	context->hbitmap = CreateCompatibleBitmap (hdc, context->rect.right, context->rect.bottom);

	context->old_bitmap = SelectObject (context->hdc, context->hbitmap);
}

VOID _app_search_destroybufferedcontext (
	_Inout_ PSEARCH_CONTEXT context
)
{
	if (context->hdc && context->old_bitmap)
		SelectObject (context->hdc, context->old_bitmap);

	SAFE_DELETE_OBJECT (context->hbitmap);

	SAFE_DELETE_DC (context->hdc);
}

LRESULT CALLBACK _app_search_subclass_proc (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
)
{
	PSEARCH_CONTEXT context;
	WNDPROC wnd_proc;

	context = _r_wnd_getcontext (hwnd, SHORT_MAX);

	if (!context)
		return FALSE;

	wnd_proc = context->wnd_proc;

	switch (msg)
	{
		case WM_NCDESTROY:
		{
			SAFE_DELETE_ICON (context->hicon_light);
			SAFE_DELETE_ICON (context->hicon_dark);

			_r_wnd_removecontext (context->hwnd, SHORT_MAX);

			SetWindowLongPtrW (hwnd, GWLP_WNDPROC, (LONG_PTR)wnd_proc);

			_app_search_destroybufferedcontext (context);

			_r_mem_free (context);

			break;
		}

		case WM_ERASEBKGND:
		{
			return TRUE;
		}

		case WM_NCCALCSIZE:
		{
			LPNCCALCSIZE_PARAMS calc_size;

			calc_size = (LPNCCALCSIZE_PARAMS)lparam;

			// Let Windows handle the non-client defaults.
			CallWindowProcW (wnd_proc, hwnd, msg, wparam, lparam);

			// Deflate the client area to accommodate the custom button.
			calc_size->rgrc[0].right -= context->cx_width;

			return 0;
		}

		case WM_NCPAINT:
		{
			RECT wnd_rect;
			RECT buf_rect;
			POINT pt = {0};
			HRGN hrgn;
			HDC hdc;
			ULONG flags = DCX_WINDOW | DCX_LOCKWINDOWUPDATE | DCX_USESTYLE;
			BOOLEAN is_hot;

			hrgn = (HRGN)wparam;

			if (hrgn == HRGN_FULL)
				hrgn = NULL;

			if (hrgn)
				flags |= DCX_INTERSECTRGN | DCX_NODELETERGN;

			hdc = GetDCEx (hwnd, hrgn, flags);

			if (hdc)
			{
				// Get the screen coordinates of the window.
				GetWindowRect (hwnd, &wnd_rect);

				// Adjust the coordinates (start from 0,0).
				OffsetRect (&wnd_rect, -wnd_rect.left, -wnd_rect.top);

				// Exclude client area.
				ExcludeClipRect (
					hdc,
					wnd_rect.left + context->cx_border + 1,
					wnd_rect.top + context->cx_border + 1,
					wnd_rect.right - context->cx_width - (context->cx_border + 1),
					wnd_rect.bottom - (context->cx_border + 1)
				);

				SetRect (&buf_rect, 0, 0, _r_calc_rectwidth (&wnd_rect), _r_calc_rectheight (&wnd_rect));

				if (context->hdc && (context->rect.right < buf_rect.right || context->rect.bottom < buf_rect.bottom))
					_app_search_destroybufferedcontext (context);

				if (!context->hdc)
					_app_search_createbufferedcontext (context, hdc, &buf_rect);

				if (!context->hdc)
				{
					ReleaseDC (hwnd, hdc);

					break;
				}

				GetCursorPos (&pt);
				ScreenToClient (hwnd, &pt);

				is_hot = PtInRect (&wnd_rect, pt);

				if ((context->is_mouseactive && is_hot) || GetFocus () == hwnd)
				{
					_r_dc_framerect (context->hdc, &wnd_rect, GetSysColor (COLOR_HOTLIGHT));

					InflateRect (&wnd_rect, -1, -1);

					_r_dc_framerect (context->hdc, &wnd_rect, GetSysColor (COLOR_WINDOW));
				}
				else if (context->is_hot)
				{
					_r_dc_framerect (context->hdc, &wnd_rect, _r_theme_isenabled () ? RGB (0x8F, 0x8F, 0x8F) : RGB (0x02A, 0x02A, 0x02A));

					InflateRect (&wnd_rect, -1, -1);

					_r_dc_framerect (context->hdc, &wnd_rect, GetSysColor (COLOR_WINDOW));
				}
				else
				{
					_r_dc_framerect (context->hdc, &wnd_rect, GetSysColor (COLOR_WINDOWFRAME));

					InflateRect (&wnd_rect, -1, -1);

					_r_dc_framerect (context->hdc, &wnd_rect, GetSysColor (COLOR_WINDOW));
				}

				_app_search_drawwindow (context, &wnd_rect);
				_app_search_drawbutton (context, hwnd, &wnd_rect);

				BitBlt (hdc, buf_rect.left, buf_rect.top, buf_rect.right, buf_rect.bottom, context->hdc, 0, 0, SRCCOPY);

				ReleaseDC (hwnd, hdc);
			}

			return 0;
		}

		case WM_NCHITTEST:
		{
			RECT wnd_rect;
			RECT btn_rect;
			POINT point;

			// Get the screen coordinates of the mouse.
			if (!GetCursorPos (&point))
				break;

			// Get the screen coordinates of the window.
			if (!GetWindowRect (hwnd, &wnd_rect))
				break;

			// Get the position of the inserted button.
			_app_search_getbuttonrect (context, &wnd_rect, &btn_rect);

			// Check that the mouse is within the inserted button.
			if (PtInRect (&btn_rect, point))
				return HTBORDER;

			break;
		}

		case WM_NCLBUTTONDOWN:
		{
			RECT wnd_rect;
			RECT btn_rect;
			POINT point;

			// Get the screen coordinates of the mouse.
			if (!GetCursorPos (&point))
				break;

			// Get the screen coordinates of the window.
			if (!GetWindowRect (hwnd, &wnd_rect))
				break;

			// Get the position of the inserted button.
			_app_search_getbuttonrect (context, &wnd_rect, &btn_rect);

			context->is_pushed = PtInRect (&btn_rect, point);

			SetCapture (hwnd);

			RedrawWindow (hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);

			break;
		}

		case WM_LBUTTONUP:
		{
			RECT wnd_rect;
			RECT btn_rect;
			POINT point;

			// Get the screen coordinates of the mouse.
			if (!GetCursorPos (&point))
				break;

			// Get the screen coordinates of the window.
			if (!GetWindowRect (hwnd, &wnd_rect))
				break;

			// Get the position of the inserted button.
			_app_search_getbuttonrect (context, &wnd_rect, &btn_rect);

			// Check that the mouse is within the inserted button.
			if (PtInRect (&btn_rect, point))
			{
				SetFocus (hwnd);

				_r_ctrl_setstring (hwnd, 0, L"");
			}

			if (GetCapture () == hwnd)
			{
				context->is_pushed = FALSE;

				ReleaseCapture ();
			}

			RedrawWindow (hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);

			break;
		}

		case WM_CUT:
		case WM_CLEAR:
		case WM_PASTE:
		case WM_UNDO:
		case WM_KEYUP:
		case WM_SETTEXT:
		case WM_KILLFOCUS:
		{
			RedrawWindow (hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);
			break;
		}

		case WM_SETTINGCHANGE:
		case WM_SYSCOLORCHANGE:
		case WM_THEMECHANGED:
		case WM_DPICHANGED:
		{
			if (msg == WM_DPICHANGED)
				context->dpi_value = _r_dc_getwindowdpi (context->hwnd);

			_app_search_themechanged (hwnd, context);

			break;
		}

		case WM_MOUSEMOVE:
		case WM_NCMOUSEMOVE:
		{
			TRACKMOUSEEVENT tme = {0};
			RECT wnd_rect;
			RECT btn_rect;
			POINT pt;

			// Get the screen coordinates of the mouse.
			if (!GetCursorPos (&pt))
				break;

			// Get the screen coordinates of the window.
			if (!GetWindowRect (hwnd, &wnd_rect))
				break;

			context->is_hot = PtInRect (&wnd_rect, pt);

			// Get the position of the inserted button.
			_app_search_getbuttonrect (context, &wnd_rect, &btn_rect);

			// Check that the mouse is within the inserted button.
			if (!context->is_mouseactive)
			{
				tme.cbSize = sizeof (tme);
				tme.dwFlags = TME_LEAVE;
				tme.hwndTrack = hwnd;
				tme.dwHoverTime = HOVER_DEFAULT;

				context->is_mouseactive = TRUE;

				TrackMouseEvent (&tme);
			}

			RedrawWindow (hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);

			break;
		}

		case WM_MOUSELEAVE:
		case WM_NCMOUSELEAVE:
		{
			TRACKMOUSEEVENT tme = {0};
			RECT wnd_rect;
			RECT btn_rect;
			POINT point;

			if (context->is_mouseactive)
			{
				tme.cbSize = sizeof (TRACKMOUSEEVENT);
				tme.dwFlags = TME_LEAVE | TME_CANCEL;
				tme.hwndTrack = hwnd;
				tme.dwHoverTime = HOVER_DEFAULT;

				TrackMouseEvent (&tme);

				context->is_mouseactive = FALSE;
			}

			// Get the screen coordinates of the mouse.
			if (!GetCursorPos (&point))
				break;

			// Get the screen coordinates of the window.
			if (!GetWindowRect (hwnd, &wnd_rect))
				break;

			_app_search_getbuttonrect (context, &wnd_rect, &btn_rect);

			context->is_hot = PtInRect (&btn_rect, point);

			RedrawWindow (hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);

			break;
		}

		case WM_PAINT:
		{
			HBITMAP holdbitmap;
			HBITMAP hbitmap;
			PR_STRING string;
			RECT rect;
			HDC buffer_dc;
			HDC hdc;

			string = _r_edit_getcuebanner (hwnd, 0);

			if (
				_r_obj_isstringempty (string) ||
				GetFocus () == hwnd ||
				CallWindowProcW (wnd_proc, hwnd, WM_GETTEXTLENGTH, 0, 0) > 0 // Edit_GetTextLength
				)
			{
				if (string)
					_r_obj_dereference (string);

				return CallWindowProcW (wnd_proc, hwnd, msg, wparam, lparam);
			}

			hdc = (HDC)wparam ? (HDC)wparam : GetDC (hwnd);

			if (hdc)
			{
				GetClientRect (hwnd, &rect);

				buffer_dc = CreateCompatibleDC (hdc);
				hbitmap = CreateCompatibleBitmap (hdc, rect.right, rect.bottom);

				holdbitmap = SelectObject (buffer_dc, hbitmap);

				SetBkMode (buffer_dc, TRANSPARENT);

				if (_r_theme_isenabled ())
				{
					SetTextColor (buffer_dc, RGB (170, 170, 170));

					_r_dc_fillrect (buffer_dc, &rect, WND_BACKGROUND2_CLR);
				}
				else
				{
					SetTextColor (buffer_dc, GetSysColor (COLOR_GRAYTEXT));

					_r_dc_fillrect (buffer_dc, &rect, GetSysColor (COLOR_WINDOW));
				}

				_r_dc_fixfont (buffer_dc, hwnd, 0);

				rect.left += 2;

				_r_dc_drawtext (NULL, buffer_dc, &string->sr, &rect, 0, 0, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP, 0);

				rect.left -= 2;

				BitBlt (hdc, rect.left, rect.top, rect.right, rect.bottom, buffer_dc, 0, 0, SRCCOPY);

				SelectObject (buffer_dc, holdbitmap);

				DeleteObject (hbitmap);
				DeleteDC (buffer_dc);

				if (!(HDC)wparam)
					ReleaseDC (hwnd, hdc);
			}

			if (string)
				_r_obj_dereference (string);

			return DefWindowProcW (hwnd, msg, wparam, lparam);
		}
	}

	return CallWindowProcW (wnd_proc, hwnd, msg, wparam, lparam);
}
