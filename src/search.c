// simplewall
// Copyright (c) 2012-2026 dmex
// Copyright (c) 2021-2026 Henry++

#include "global.h"

VOID _app_search_initialize (
	_Inout_ PSEARCH_CONTEXT context
)
{
	HTHEME htheme;
	LONG cx_border;
	HRESULT status;

	cx_border = _r_dc_getsystemmetrics (SM_CXBORDER, context->dpi_value);

	// initialize borders
	context->cx_width = _r_dc_getdpi (20, context->dpi_value);
	context->cx_border = cx_border;
	context->dc_brush = (HBRUSH)GetStockObject (DC_BRUSH);

	context->search_button.index = SC_IMAGE_SEARCH;
	context->regex_button.index = SC_IMAGE_REGEX;
	context->case_button.index = SC_IMAGE_CASE;

	if (IsThemeActive ())
	{
		htheme = _r_dc_openthemedata (context->hwnd, VSCLASS_EDIT, context->dpi_value);

		if (htheme)
		{
			status = GetThemeInt (htheme, 0, 0, TMT_BORDERSIZE, &cx_border);

			if (SUCCEEDED (status))
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
	WCHAR buffer[0x80];

	context = (PSEARCH_CONTEXT)_r_mem_allocate (sizeof (SEARCH_CONTEXT));

	BringWindowToTop (hwnd); // HACK!!!

	context->hwnd = hwnd;
	context->dpi_value = _r_dc_getwindowdpi (hwnd);

	_app_search_initialize (context);

	_r_wnd_setcontext (context->hwnd, SHORT_MAX, context);

	// subclass the edit control window procedure
	context->wnd_proc = (WNDPROC)GetWindowLongPtrW (context->hwnd, GWLP_WNDPROC);
	SetWindowLongPtrW (context->hwnd, GWLP_WNDPROC, (LONG_PTR)&_app_search_subclass_proc);

	// set cue banner
	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%s...", _r_locale_getstring (IDS_FIND));

	_r_edit_setcuebanner (context->hwnd, 0, buffer);

	// initialize the theme parameters
	_app_search_themechanged (context->hwnd, context);
}

VOID _app_search_initializeimages (
	_In_ PSEARCH_CONTEXT context,
	_In_ HWND hwnd
)
{
	LPCWSTR name;
	HBITMAP hbitmap;
	NTSTATUS status;

	// initialize icons
	context->image_width = _r_dc_getsystemmetrics (SM_CXSMICON, context->dpi_value) + _r_dc_getdpi (4, context->dpi_value);
	context->image_height = _r_dc_getsystemmetrics (SM_CYSMICON, context->dpi_value) + _r_dc_getdpi (4, context->dpi_value);

	if (context->himglist)
	{
		_r_imagelist_setsize (context->himglist, context->image_width, context->image_height);
	}
	else
	{
		_r_imagelist_create (&context->himglist, context->image_width, context->image_height, ILC_MASK | ILC_COLOR32, 5, 5);
	}

	_r_imagelist_setimagecount (context->himglist, 5);

	// search button
	name = _r_theme_isenabled () ? MAKEINTRESOURCE (IDP_SEARCH_LIGHT) : MAKEINTRESOURCE (IDP_SEARCH_DARK);

	status = _r_res_loadimage (&hbitmap, _r_sys_getimagebase (), L"PNG", name, &GUID_ContainerFormatPng, context->image_width, context->image_height);

	if (NT_SUCCESS (status))
	{
		_r_imagelist_replace (context->himglist, hbitmap, NULL, SC_IMAGE_SEARCH);

		DeleteObject (hbitmap);
	}

	// regex button
	name = _r_theme_isenabled () ? MAKEINTRESOURCE (IDP_REGEX_LIGHT) : MAKEINTRESOURCE (IDP_REGEX_DARK);

	status = _r_res_loadimage (&hbitmap, _r_sys_getimagebase (), L"PNG", name, &GUID_ContainerFormatPng, context->image_width, context->image_height);

	if (NT_SUCCESS (status))
	{
		_r_imagelist_replace (context->himglist, hbitmap, NULL, SC_IMAGE_REGEX);

		DeleteObject (hbitmap);
	}

	// case button
	name = _r_theme_isenabled () ? MAKEINTRESOURCE (IDP_CASE_LIGHT) : MAKEINTRESOURCE (IDP_CASE_DARK);

	status = _r_res_loadimage (&hbitmap, _r_sys_getimagebase (), L"PNG", name, &GUID_ContainerFormatPng, context->image_width, context->image_height);

	if (NT_SUCCESS (status))
	{
		_r_imagelist_replace (context->himglist, hbitmap, NULL, SC_IMAGE_CASE);

		DeleteObject (hbitmap);
	}
}

VOID _app_search_themechanged (
	_In_ HWND hwnd,
	_Inout_ PSEARCH_CONTEXT context
)
{
	_app_search_initialize (context);
	_app_search_initializeimages (context, hwnd);

	// reset the client area margins
	CallWindowProcW (context->wnd_proc, hwnd, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM (0, 0));

	// refresh the non-client area
	SetWindowPos (hwnd, NULL, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

	// force the edit control to update its non-client area
	//RedrawWindow (hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);
}

VOID _app_search_setvisible (
	_In_ HWND hwnd,
	_In_ HWND hsearch,
	_In_ LONG dpi_value
)
{
	if (_r_config_getboolean (L"IsShowSearchBar", TRUE, NULL))
	{
		if (!_r_rebar_isbandexists (hwnd, IDC_REBAR, REBAR_SEARCH_ID))
			_r_rebar_insertband (hwnd, IDC_REBAR, REBAR_SEARCH_ID, config.hsearchbar, RBBS_VARIABLEHEIGHT | RBBS_NOGRIPPER | RBBS_USECHEVRON, _r_dc_getdpi (180, dpi_value), 20);

		ShowWindow (hsearch, SW_SHOWNA);

		_r_wnd_sendmessage (hsearch, 0, WM_SIZE, 0, 0);

		if (_r_wnd_isvisible (hwnd, FALSE))
			SetFocus (hsearch);
	}
	else
	{
		if (_r_rebar_isbandexists (hwnd, IDC_REBAR, REBAR_SEARCH_ID))
			_r_rebar_deleteband (hwnd, IDC_REBAR, REBAR_SEARCH_ID);

		_r_ctrl_setstring (hsearch, 0, L"");

		ShowWindow (hsearch, SW_HIDE);
	}

	UpdateWindow (hsearch);
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
	_In_ PSEARCH_CONTEXT context,
	_In_ PSEARCHCONTROL_BUTTON button,
	_In_ HDC hdc,
	_In_ LPCRECT wnd_rect
)
{
	RECT btn_rect, img_rect;

	_app_search_getbuttonrect (context, button, wnd_rect, &btn_rect);

	if (button->is_pushed)
	{
		_r_dc_fillrect (context->hdc, &btn_rect, _r_theme_isenabled () ? RGB (99, 99, 99) : RGB (153, 209, 255));
	}
	else if (button->is_hot)
	{
		_r_dc_fillrect (context->hdc, &btn_rect, _r_theme_isenabled () ? RGB (78, 78, 78) : RGB (205, 232, 255));
	}
	else
	{
		_r_dc_fillrect (context->hdc, &btn_rect, _r_theme_isenabled () ? RGB (60, 60, 60) : GetSysColor (COLOR_WINDOW));
	}

	if (button->index != INT_ERROR)
	{
		SetRect (
			&img_rect,
			btn_rect.left,
			btn_rect.top,
			btn_rect.left + 1 /*offset*/ + (_r_calc_rectwidth (&btn_rect) - context->cx_width) / 2,
			btn_rect.top + (_r_calc_rectheight (&btn_rect) - context->cx_width) / 2
		);

		_r_imagelist_draw (context->himglist, button->index, hdc, img_rect.left, img_rect.top, img_rect.right, img_rect.bottom, CLR_DEFAULT, CLR_DEFAULT, ILD_TRANSPARENT, TRUE);
	}
}

VOID _app_search_paintbuttons (
	_In_ PSEARCH_CONTEXT context,
	_In_ HDC hdc,
	_In_ LPCRECT wnd_rect
)
{
	_app_search_drawbutton (context, &context->search_button, hdc, wnd_rect);
	//_app_search_drawbutton (context, &context->regex_button, hdc, wnd_rect);
	//_app_search_drawbutton (context, &context->case_button, hdc, wnd_rect);
}

VOID _app_search_getbuttonrect (
	_In_ PSEARCH_CONTEXT context,
	_In_ PSEARCHCONTROL_BUTTON button,
	_In_ LPCRECT wnd_rect,
	_Out_ PRECT btn_rect
)
{
	CopyRect (btn_rect, wnd_rect);

	btn_rect->left = ((btn_rect->right - context->cx_width) - context->cx_border - 1);
	btn_rect->top += context->cx_border;
	btn_rect->right -= context->cx_border;
	btn_rect->bottom -= context->cx_border;

	// shift the button rect to the left based on the button index
	btn_rect->left -= ((context->cx_width + context->cx_border - 1) * (SC_BUTTON_COUNT - 1 - button->index));
	btn_rect->right -= ((context->cx_width + context->cx_border - 1) * (SC_BUTTON_COUNT - 1 - button->index));
}

BOOLEAN _app_search_isstringfound (
	_In_opt_ PR_STRINGREF string,
	_In_ PR_STRINGREF search_string,
	_Inout_ PITEM_LISTVIEW_CONTEXT context,
	_Inout_ PBOOLEAN is_changed
)
{
	if (_r_obj_isstringempty (string))
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
	PITEM_NETWORK ptr_network = NULL;
	PITEM_RULE ptr_rule = NULL;
	PITEM_APP ptr_app = NULL;
	PITEM_LOG ptr_log = NULL;
	PR_STRING string;
	BOOLEAN is_changed = FALSE;

	// reset hidden state
	if (context->is_hidden)
	{
		context->is_hidden = FALSE;

		is_changed = TRUE;
	}

	if (_r_obj_isstringempty (search_string))
		goto CleanupExit;

	switch (listview_id)
	{
		case IDC_APPS_PROFILE:
		case IDC_APPS_SERVICE:
		case IDC_APPS_UWP:
		case IDC_RULE_APPS_ID:
		{
			ptr_app = _app_getappitem ((ULONG)context->id_code);

			if (!ptr_app)
				goto CleanupExit;

			// path
			if (ptr_app->real_path && _app_search_isstringfound (&ptr_app->real_path->sr, &search_string->sr, context, &is_changed))
				goto CleanupExit;

			// comment
			if (!_r_obj_isstringempty (ptr_app->comment) && _app_search_isstringfound (&ptr_app->comment->sr, &search_string->sr, context, &is_changed))
				goto CleanupExit;

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

			if (ptr_rule->name && _app_search_isstringfound (&ptr_rule->name->sr, &search_string->sr, context, &is_changed))
				goto CleanupExit;

			if (ptr_rule->rule_remote && _app_search_isstringfound (&ptr_rule->rule_remote->sr, &search_string->sr, context, &is_changed))
				goto CleanupExit;

			if (ptr_rule->rule_local && _app_search_isstringfound (&ptr_rule->rule_local->sr, &search_string->sr, context, &is_changed))
				goto CleanupExit;

			if (ptr_rule->protocol_str && _app_search_isstringfound (&ptr_rule->protocol_str->sr, &search_string->sr, context, &is_changed))
				goto CleanupExit;

			// comment
			if (!_r_obj_isstringempty (ptr_rule->comment) && _app_search_isstringfound (&ptr_rule->comment->sr, &search_string->sr, context, &is_changed))
				goto CleanupExit;

			break;
		}

		case IDC_NETWORK:
		{
			ptr_network = _app_network_getitem ((ULONG)context->id_code);

			if (!ptr_network)
				goto CleanupExit;

			// path
			if (ptr_network->path && _app_search_isstringfound (&ptr_network->path->sr, &search_string->sr, context, &is_changed))
				goto CleanupExit;

			// local address
			string = (PR_STRING)_InterlockedCompareExchangePointer ((volatile PVOID_PTR)&ptr_network->local_addr_str, NULL, NULL);

			if (string)
			{
				if (_app_search_isstringfound (&string->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			// local host
			string = (PR_STRING)_InterlockedCompareExchangePointer ((volatile PVOID_PTR)&ptr_network->local_host_str, NULL, NULL);

			if (string)
			{
				if (_app_search_isstringfound (&string->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			// remote address
			string = (PR_STRING)_InterlockedCompareExchangePointer ((volatile PVOID_PTR)&ptr_network->remote_addr_str, NULL, NULL);

			if (string)
			{
				if (_app_search_isstringfound (&string->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			// remote host
			string = (PR_STRING)_InterlockedCompareExchangePointer ((volatile PVOID_PTR)&ptr_network->remote_host_str, NULL, NULL);

			if (string)
			{
				if (_app_search_isstringfound (&string->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			// protocol
			if (ptr_network->protocol_str && _app_search_isstringfound (&ptr_network->protocol_str->sr, &search_string->sr, context, &is_changed))
				goto CleanupExit;

			break;
		}

		case IDC_LOG:
		{
			ptr_log = _app_getlogitem ((ULONG)context->id_code);

			if (!ptr_log)
				goto CleanupExit;

			// path
			if (ptr_log->path && _app_search_isstringfound (&ptr_log->path->sr, &search_string->sr, context, &is_changed))
				goto CleanupExit;

			// filter name
			if (ptr_log->filter_name && _app_search_isstringfound (&ptr_log->filter_name->sr, &search_string->sr, context, &is_changed))
				goto CleanupExit;

			// layer name
			if (ptr_log->layer_name && _app_search_isstringfound (&ptr_log->layer_name->sr, &search_string->sr, context, &is_changed))
				goto CleanupExit;

			// user name
			if (ptr_log->username && _app_search_isstringfound (&ptr_log->username->sr, &search_string->sr, context, &is_changed))
				goto CleanupExit;

			// local address
			string = (PR_STRING)_InterlockedCompareExchangePointer ((volatile PVOID_PTR)&ptr_log->local_addr_str, NULL, NULL);

			if (string)
			{
				if (_app_search_isstringfound (&string->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			// local host
			string = (PR_STRING)_InterlockedCompareExchangePointer ((volatile PVOID_PTR)&ptr_log->local_host_str, NULL, NULL);

			if (string)
			{
				if (_app_search_isstringfound (&string->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			// remote address
			string = (PR_STRING)_InterlockedCompareExchangePointer ((volatile PVOID_PTR)&ptr_log->remote_addr_str, NULL, NULL);

			if (string)
			{
				if (_app_search_isstringfound (&string->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			// remote host
			string = (PR_STRING)_InterlockedCompareExchangePointer ((volatile PVOID_PTR)&ptr_log->remote_host_str, NULL, NULL);

			if (string)
			{
				if (_app_search_isstringfound (&string->sr, &search_string->sr, context, &is_changed))
					goto CleanupExit;
			}

			// protocol
			if (ptr_log->protocol_str && _app_search_isstringfound (&ptr_log->protocol_str->sr, &search_string->sr, context, &is_changed))
				goto CleanupExit;

			break;
		}
	}

CleanupExit:

	if (is_changed)
		_r_listview_setitem (hwnd, listview_id, item_id, 0, NULL, I_IMAGECALLBACK, I_GROUPIDCALLBACK, I_DEFAULT);

	if (ptr_network)
		_r_obj_dereference (ptr_network);

	if (ptr_rule)
		_r_obj_dereference (ptr_rule);

	if (ptr_app)
		_r_obj_dereference (ptr_app);

	if (ptr_log)
		_r_obj_dereference (ptr_log);

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

	context->old_bitmap = (HBITMAP)SelectObject (context->hdc, context->hbitmap);
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

VOID _app_search_restorefocus (
	_In_ PSEARCH_CONTEXT context
)
{
	if (context->hprev)
	{
		SetFocus (context->hprev);

		context->hprev = NULL;
	}
}

VOID _app_search_excludeclient (
	_In_ PSEARCH_CONTEXT context,
	_In_ HWND hwnd,
	_In_ HDC hdc,
	_In_ LPCRECT wnd_rect
)
{
	WINDOWINFO window_info = {0};

	window_info.cbSize = sizeof (WINDOWINFO);

	if (GetWindowInfo (hwnd, &window_info))
	{
		OffsetRect (&window_info.rcClient, -window_info.rcWindow.left, -window_info.rcWindow.top);

		ExcludeClipRect (hdc, window_info.rcClient.left, window_info.rcClient.top, window_info.rcClient.right, window_info.rcClient.bottom);
	}
	else
	{
		ExcludeClipRect (
			hdc,
			wnd_rect->left + (context->cx_border + 1),
			wnd_rect->top + (context->cx_border + 1),
			wnd_rect->right - (context->cx_width * SC_BUTTON_COUNT) - (context->cx_border + 1),
			wnd_rect->bottom - (context->cx_border + 1)
		);
	}
}

COLORREF _app_search_getframecolor (
	_In_ PSEARCH_CONTEXT context,
	_In_ HWND hwnd
)
{
	if (GetFocus () == hwnd)
		return WND_HIGHLIGHT_CLR;

	if (context->is_hot)
		return _r_theme_isenabled () ? WND_HIGHLIGHT_CLR : RGB (43, 43, 43);

	return _r_theme_isenabled () ? WND_BACKGROUND2_CLR : GetSysColor (COLOR_WINDOWFRAME);
}

VOID _app_search_paintframe (
	_In_ PSEARCH_CONTEXT context,
	_In_ HWND hwnd,
	_In_ HDC hdc,
	_In_ LPCRECT wnd_rect
)
{
	RECT frame_rect;

	CopyRect (&frame_rect, wnd_rect);

	_r_dc_framerect (hdc, &frame_rect, _app_search_getframecolor (context, hwnd));

	InflateRect (&frame_rect, -1, -1);

	_r_dc_framerect (hdc, &frame_rect, WND_BACKGROUND_CLR);
}

VOID _app_search_paintnonclient (
	_In_ PSEARCH_CONTEXT context,
	_In_ HWND hwnd,
	_In_ HDC hdc,
	_In_ LPCRECT wnd_rect,
	_In_ LPCRECT buf_rect
)
{
	_app_search_excludeclient (context, hwnd, hdc, wnd_rect);

	_r_dc_fillrect (hdc, buf_rect, _r_theme_isenabled () ? WND_BACKGROUND_CLR : GetSysColor (COLOR_WINDOW));

	_app_search_paintframe (context, hwnd, hdc, wnd_rect);
	_app_search_paintbuttons (context, hdc, wnd_rect);
}

BOOLEAN _app_search_handlenonclientpaint (
	_In_ PSEARCH_CONTEXT context,
	_In_ HWND hwnd,
	_In_ WPARAM wparam
)
{
	BP_PAINTPARAMS buf_params = {0};
	HPAINTBUFFER hpaintbuffer;
	RECT buf_rect, wnd_rect;
	HRGN hrgn;
	HDC hdc, hdc_buffer;
	ULONG flags = DCX_WINDOW | DCX_CACHE | DCX_USESTYLE;
	LONG height, width;

	// get the screen coordinates of the window
	if (!GetWindowRect (hwnd, &wnd_rect))
		return FALSE;

	width = _r_calc_rectwidth (&wnd_rect);
	height = _r_calc_rectheight (&wnd_rect);

	if (height <= 0 || width <= 0)
		return FALSE;

	hrgn = (HRGN)wparam;

	if (hrgn != HRGN_FULL && hrgn != RGN_ERROR)
	{
		if (!RectInRegion (hrgn, &wnd_rect))
			return FALSE; // frame area isn't dirty at all — skip GetDCEx entirely
	}

	if (hrgn == HRGN_FULL)
		hrgn = NULL;

	if (hrgn)
		flags |= DCX_INTERSECTRGN | DCX_NODELETERGN;

	hdc = GetDCEx (hwnd, hrgn, flags);

	if (hdc)
	{
		// normalize window coordinates (start from 0,0)
		OffsetRect (&wnd_rect, -wnd_rect.left, -wnd_rect.top);

		SetRect (&buf_rect, 0, 0, width, height);

		_app_search_excludeclient (context, hwnd, hdc, &wnd_rect);

		buf_params.cbSize = sizeof (BP_PAINTPARAMS);
		buf_params.dwFlags = BPPF_NONCLIENT;

		hpaintbuffer = BeginBufferedPaint (hdc, &buf_rect, BPBF_COMPATIBLEBITMAP, &buf_params, &hdc_buffer);

		if (hpaintbuffer)
		{
			_app_search_paintnonclient (context, hwnd, hdc_buffer, &wnd_rect, &buf_rect);

			EndBufferedPaint (hpaintbuffer, TRUE);
		}
		else
		{
			_app_search_paintnonclient (context, hwnd, hdc, &wnd_rect, &buf_rect);
		}

		ReleaseDC (hwnd, hdc);

		return TRUE;
	}

	return FALSE;
}

VOID _app_search_controlpaintcuebanner (
	_In_ PSEARCH_CONTEXT context,
	_In_ HDC hdc,
	_In_ LPCRECT rect,
	_In_ BOOLEAN is_erase
)
{
	PR_STRING string;
	RECT text_rect;

	if (is_erase)
		_r_dc_fillrect (hdc, rect, GetSysColor (COLOR_WINDOW));

	SetBkMode (hdc, TRANSPARENT);
	SetTextColor (hdc, _r_theme_isenabled () ? RGB (170, 170, 170) : GetSysColor (COLOR_GRAYTEXT));

	_r_dc_fillrect (hdc, rect, _r_theme_isenabled () ? RGB (60, 60, 60) : GetSysColor (COLOR_WINDOW));

	string = _r_edit_getcuebanner (context->hwnd, 0);

	if (!string)
		return;

	CopyRect (&text_rect, rect);

	text_rect.left += 2;

	_r_dc_drawtext (NULL, hdc, &string->sr, &text_rect, 0, 0, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP, 0);

	_r_obj_dereference (string);
}

FORCEINLINE _app_search_ishot (
	_In_ PSEARCH_CONTEXT context,
	_In_ BOOLEAN is_washot,
	_In_ BOOLEAN is_oldsearchhot,
	_In_ BOOLEAN is_oldregexhot,
	_In_ BOOLEAN is_oldcasehot

)
{
	if (is_washot != !!context->is_hot || is_oldsearchhot != !!context->search_button.is_hot || is_oldregexhot != !!context->regex_button.is_hot || is_oldcasehot != !!context->case_button.is_hot)
		return TRUE;

	return FALSE;
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

	context = (PSEARCH_CONTEXT)_r_wnd_getcontext (hwnd, SHORT_MAX);

	if (!context)
		return DefWindowProcW (hwnd, msg, wparam, lparam);

	wnd_proc = context->wnd_proc;

	switch (msg)
	{
		case WM_NCDESTROY:
		{
			//SAFE_DELETE_ICON (context->hicon_light);
			//SAFE_DELETE_ICON (context->hicon_dark);

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
			LPNCCALCSIZE_PARAMS calc_size = (LPNCCALCSIZE_PARAMS)lparam;

			// let Windows handle the non-client defaults
			CallWindowProcW (wnd_proc, hwnd, msg, wparam, lparam);

			// deflate the client area to accommodate the custom buttons, plus the same border inset reserved on the other three sides below - otherwise the text
			// area butts directly against the button row with no frame between them, while the other sides show a proper 2px border (dmex)
			calc_size->rgrc[0].right -= (context->cx_width * SC_BUTTON_COUNT);// +(context->cx_border + 1);

			// Note: also reserve the border drawn by "_app_search_paintframe" (a 2px-deep frame: the outer edge plus an inset edge). without this, the client edit control's
			// own rect still overlaps those pixels, so its redraws can leave stale fragments of the border behind whenever the non-client area isn't repainted alongside it. (dmex)
			//calc_size->rgrc[0].left += (context->cx_border + 1);
			//calc_size->rgrc[0].top += (context->cx_border + 1);
			//calc_size->rgrc[0].bottom -= (context->cx_border + 1);

			return 0;
		}

		case WM_NCPAINT:
		{

			if (_app_search_handlenonclientpaint (context, hwnd, wparam))
				return 0;

			break;
		}

		case WM_NCHITTEST:
		{
			RECT btn_rect, wnd_rect;
			POINT point = {0};

			// get the screen coordinates of the mouse
			point.x = GET_X_LPARAM (lparam);
			point.y = GET_Y_LPARAM (lparam);

			// get the screen coordinates of the window
			if (!GetWindowRect (hwnd, &wnd_rect))
				break;

			// get the position of the inserted button
			_app_search_getbuttonrect (context, &context->search_button, &wnd_rect, &btn_rect);

			if (PtInRect (&btn_rect, point))
				return HTBORDER;

			_app_search_getbuttonrect (context, &context->regex_button, &wnd_rect, &btn_rect);

			if (PtInRect (&btn_rect, point))
				return HTBORDER;

			_app_search_getbuttonrect (context, &context->case_button, &wnd_rect, &btn_rect);

			if (PtInRect (&btn_rect, point))
				return HTBORDER;

			break;
		}

		case WM_NCLBUTTONDOWN:
		case WM_NCLBUTTONDBLCLK:
		{
			// Note: the edit class has CS_DBLCLKS, so a second click shortly after the first arrives as WM_NCLBUTTONDBLCLK instead of WM_NCLBUTTONDOWN. treat it as a normal
			// press, otherwise the click is dropped. (dmex)
			RECT btn_rect, wnd_rect;
			POINT point = {0};

			// get the screen coordinates of the mouse
			point.x = GET_X_LPARAM (lparam);
			point.y = GET_Y_LPARAM (lparam);

			// get the screen coordinates of the window
			if (!GetWindowRect (hwnd, &wnd_rect))
				break;

			_app_search_getbuttonrect (context, &context->search_button, &wnd_rect, &btn_rect);
			context->search_button.is_pushed = PtInRect (&btn_rect, point);

			_app_search_getbuttonrect (context, &context->regex_button, &wnd_rect, &btn_rect);
			context->regex_button.is_pushed = PtInRect (&btn_rect, point);

			_app_search_getbuttonrect (context, &context->case_button, &wnd_rect, &btn_rect);
			context->case_button.is_pushed = PtInRect (&btn_rect, point);

			SetCapture (hwnd);

			RedrawWindow (hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);

			break;
		}

		case WM_LBUTTONUP:
		{
			RECT btn_rect, wnd_rect;
			POINT point = {0};
			ULONG position;

			// get the screen coordinates of the mouse
			position = GetMessagePos ();

			point.x = GET_X_LPARAM (position);
			point.y = GET_Y_LPARAM (position);

			// get the screen coordinates of the window
			if (!GetWindowRect (hwnd, &wnd_rect))
				break;

			_app_search_getbuttonrect (context, &context->search_button, &wnd_rect, &btn_rect);

			if (PtInRect (&btn_rect, point))
			{
				SetFocus (hwnd);

				_r_ctrl_setstring (hwnd, 0, L"");
			}

			//_app_search_getbuttonrect (context, &context->regex_button, &wnd_rect, &btn_rect);
			//
			//if (PtInRect (&btn_rect, point))
			//{
			//	context->regex_button.is_active = !context->regex_button.is_active;
			//
			//	_r_config_setboolean (context->regex_button.settings_name, !!context->regex_button.is_active, NULL);
			//}

			//_app_search_getbuttonrect (context, &context->case_button, &wnd_rect, &btn_rect);
			//
			//if (PtInRect (&btn_rect, point))
			//{
			//	context->case_button.is_active = !context->case_button.is_active;
			//
			//	_r_config_setboolean (context->case_button.settings_name, !!context->case_button.is_active, NULL);
			//}

			if (GetCapture () == hwnd)
			{
				context->search_button.is_pushed = FALSE;
				context->regex_button.is_pushed = FALSE;
				context->case_button.is_pushed = FALSE;

				ReleaseCapture ();
			}

			RedrawWindow (hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);

			break;
		}

		case WM_CAPTURECHANGED:
		{
			// capture can be lost without a button-up (menus, modal dialogs); clear the pushed state so the buttons don't stay painted down
			context->search_button.is_pushed = FALSE;
			context->regex_button.is_pushed = FALSE;
			context->case_button.is_pushed = FALSE;

			RedrawWindow (hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);

			break;
		}

		case WM_CUT:
		case WM_CLEAR:
		case WM_PASTE:
		case WM_UNDO:
		case WM_KEYUP:
		case WM_SETTEXT:
		{
			LRESULT result = CallWindowProcW (wnd_proc, hwnd, msg, wparam, lparam);

			RedrawWindow (hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);

			return result;
		}

		case WM_SETFOCUS:
		{
			context->is_windowfocus = TRUE;
			context->hprev = (HWND)wparam;

			//RedrawWindow (hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);
			InvalidateRect (hwnd, NULL, FALSE);

			break;
		}

		case WM_KILLFOCUS:
		{
			context->is_windowfocus = FALSE;

			RedrawWindow (hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);

			break;
		}

		case WM_SETTINGCHANGE:
		case WM_SYSCOLORCHANGE:
		case WM_THEMECHANGED:
		case WM_DPICHANGED_AFTERPARENT:
		{
			if (msg == WM_DPICHANGED_AFTERPARENT)
				context->dpi_value = _r_dc_getwindowdpi (context->hwnd);

			_app_search_themechanged (hwnd, context);

			break;
		}

		case WM_MOUSEMOVE:
		case WM_NCMOUSEMOVE:
		{
			TRACKMOUSEEVENT tme = {0};
			RECT wnd_rect;
			POINT point = {0};
			ULONG position;
			BOOLEAN is_oldcasehot, is_oldregexhot, is_oldsearchhot, is_washot;

			// get the screen coordinates of the mouse
			position = GetMessagePos ();

			point.x = GET_X_LPARAM (position);
			point.y = GET_Y_LPARAM (position);

			// get the screen coordinates of the window
			if (!GetWindowRect (hwnd, &wnd_rect))
				break;

			is_washot = !!context->is_hot;
			is_oldsearchhot = !!context->search_button.is_hot;
			is_oldregexhot = !!context->regex_button.is_hot;
			is_oldcasehot = !!context->case_button.is_hot;

			context->is_hot = PtInRect (&wnd_rect, point);

			// get the position of the inserted button
			//_app_search_getbuttonrect (context, &context->regex_button, &wnd_rect, &btn_rect);
			//
			//is_washot = !!context->regex_button.is_hot;
			//context->regex_button.is_hot = PtInRect (&btn_rect, point);

			// check that the mouse is within the inserted button
			if (!context->is_hottrack)
			{
				tme.cbSize = sizeof (TRACKMOUSEEVENT);
				tme.dwFlags = TME_LEAVE | TME_NONCLIENT;
				tme.hwndTrack = hwnd;
				tme.dwHoverTime = 0;

				context->is_hottrack = TRUE;

				TrackMouseEvent (&tme);
			}

			// Note: the buttons are painted in the non-client area, so the frame must be invalidated for the hot state to become visible. only redraw
			// on the hot-state transition since WM_MOUSEMOVE is generated for every mouse movement. (dmex)
			if (_app_search_ishot (context, is_washot, is_oldsearchhot, is_oldregexhot, is_oldcasehot))
				RedrawWindow (hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);

			break;
		}

		case WM_MOUSELEAVE:
		case WM_NCMOUSELEAVE:
		{
			TRACKMOUSEEVENT tme = {0};
			RECT btn_rect, wnd_rect;
			POINT point;

			context->is_hottrack = FALSE;

			if (!GetCursorPos (&point))
				break;

			if (!GetWindowRect (hwnd, &wnd_rect))
				break;

			context->is_hot = PtInRect (&wnd_rect, point);

			_app_search_getbuttonrect (context, &context->search_button, &wnd_rect, &btn_rect);
			context->search_button.is_hot = PtInRect (&btn_rect, point);

			_app_search_getbuttonrect (context, &context->regex_button, &wnd_rect, &btn_rect);
			context->regex_button.is_hot = PtInRect (&btn_rect, point);

			_app_search_getbuttonrect (context, &context->case_button, &wnd_rect, &btn_rect);
			context->case_button.is_hot = PtInRect (&btn_rect, point);

			RedrawWindow (hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);

			break;
		}

		case WM_CHAR:
		{
			// delete previous word for ctrl+backspace (dmex)
			if (wparam == VK_F16 && GetAsyncKeyState (VK_CONTROL) < 0)
				return 1;

			break;
		}

		case WM_GETDLGCODE:
		{
			// intercept esc key only when there is text to clear or focus to restore, otherwise let the dialog manager translate it to IDCANCEL (dmex)
			if (wparam == VK_ESCAPE && lparam && ((PMSG)lparam)->message == WM_KEYDOWN)
			{
				if (GetWindowTextLengthW (hwnd) != 0 || context->hprev)
					return DLGC_WANTMESSAGE;
			}

			break;
		}
	}

	return CallWindowProcW (wnd_proc, hwnd, msg, wparam, lparam);
}
