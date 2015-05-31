static char *tdlg_id = 
	"@(#)Copyright (C) 1996-2011 H.Shirouzu		tdlg.cpp	Ver0.97";
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Dialog Class
	Create					: 1996-06-01(Sat)
	Update					: 2011-04-20(Wed)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "tlib.h"

TDlg::TDlg(UINT _resId, TWin *_parent) : TWin(_parent)
{
	resId	= _resId;
	modalFlg = FALSE;
	maxItems = 0;
	dlgItems = NULL;
}

TDlg::~TDlg()
{
	if (hWnd) EndDialog(FALSE);
	delete [] dlgItems;
}

BOOL TDlg::Create(HINSTANCE hInstance)
{
	TApp::GetApp()->AddWin(this);

	hWnd = ::CreateDialogV(hInstance ? hInstance : TApp::GetInstance(), (void *)resId,
				parent ? parent->hWnd : NULL, (DLGPROC)TApp::WinProc);

	if (hWnd)
		return	TRUE;
	else
		return	TApp::GetApp()->DelWin(this), FALSE;
}

int TDlg::Exec(void)
{
	TApp::GetApp()->AddWin(this);
	modalFlg = TRUE;
	int result = ::DialogBoxV(TApp::GetInstance(), (void *)resId, parent ? parent->hWnd : NULL,
					(DLGPROC)TApp::WinProc);
	modalFlg = FALSE;
	return	result;
}

void TDlg::Destroy(void)
{
	EndDialog(FALSE);
}

LRESULT TDlg::WinProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LRESULT	result = 0;

	switch (uMsg)
	{
	case WM_INITDIALOG:
		if (rect.left != CW_USEDEFAULT && !(::GetWindowLong(hWnd, GWL_STYLE) & WS_CHILD)) {
			MoveWindow(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,FALSE);
		}
		if (rect.left == CW_USEDEFAULT) {
			GetWindowRect(&orgRect);
		}
		return	EvCreate(lParam);

	case WM_CLOSE:
		EvClose();
		return	0;

	case WM_COMMAND:
		EvCommand(HIWORD(wParam), LOWORD(wParam), lParam);
		return	0;

	case WM_SYSCOMMAND:
		EvSysCommand(wParam, MAKEPOINTS(lParam));
		return	0;

	case WM_TIMER:
		EvTimer(wParam, (TIMERPROC)lParam);
		return	0;

	case WM_DESTROY:
		EvDestroy();
		return	0;

	case WM_NCDESTROY:
		if (!::IsIconic(hWnd)) GetWindowRect(&rect);
		EvNcDestroy();
		TApp::GetApp()->DelWin(this);
		hWnd = 0;
		return	0;

	case WM_QUERYENDSESSION:
		result = EvQueryEndSession((BOOL)wParam, (BOOL)lParam);
		SetWindowLong(DWL_MSGRESULT, result);
		return	0;

	case WM_ENDSESSION:
		EvEndSession((BOOL)wParam, (BOOL)lParam);
		return	0;

	case WM_QUERYOPEN:
		result = EvQueryOpen();
		SetWindowLong(DWL_MSGRESULT, result);
		return	result;

	case WM_PAINT:
		EvPaint();
		return	0;

	case WM_NCPAINT:
		EvNcPaint((HRGN)wParam);
		return	0;

	case WM_SIZE:
		EvSize((UINT)wParam, LOWORD(lParam), HIWORD(lParam));
		return	0;

	case WM_MOVE:
		EvMove(LOWORD(lParam), HIWORD(lParam));
		return	0;

	case WM_SHOWWINDOW:
		EvShowWindow((BOOL)wParam, (int)lParam);
		return	0;

	case WM_GETMINMAXINFO:
		EvGetMinMaxInfo((MINMAXINFO *)lParam);
		return	0;

	case WM_SETCURSOR:
		result = EvSetCursor((HWND)wParam, LOWORD(lParam), HIWORD(lParam));
		SetWindowLong(DWL_MSGRESULT, result);
		return	result;

	case WM_MOUSEMOVE:
		return	EvMouseMove((UINT)wParam, MAKEPOINTS(lParam));

	case WM_NCHITTEST:
		EvNcHitTest(MAKEPOINTS(lParam), &result);
		SetWindowLong(DWL_MSGRESULT, result);
		return	result;

	case WM_MEASUREITEM:
		result = EvMeasureItem((UINT)wParam, (LPMEASUREITEMSTRUCT)lParam);
		SetWindowLong(DWL_MSGRESULT, result);
		return	result;

	case WM_DRAWITEM:
		result = EvDrawItem((UINT)wParam, (LPDRAWITEMSTRUCT)lParam);
		SetWindowLong(DWL_MSGRESULT, result);
		return	result;

	case WM_NOTIFY:
		result = EvNotify((UINT)wParam, (LPNMHDR)lParam);
		SetWindowLong(DWL_MSGRESULT, result);
		return	result;

	case WM_CONTEXTMENU:
		result = EvContextMenu((HWND)wParam, MAKEPOINTS(lParam));
		SetWindowLong(DWL_MSGRESULT, result);
		return	result;

	case WM_HOTKEY:
		result = EvHotKey((int)wParam);
		SetWindowLong(DWL_MSGRESULT, result);
		return	result;

	case WM_ACTIVATEAPP:
		EvActivateApp((BOOL)wParam, (DWORD)lParam);
		break;

	case WM_ACTIVATE:
		EvActivate(LOWORD(wParam), HIWORD(wParam), (HWND)lParam);
		break;

	case WM_DROPFILES:
		EvDropFiles((HDROP)wParam);
		return	0;

	case WM_CHAR:
		EvChar((WCHAR)wParam, lParam);
		SetWindowLong(DWL_MSGRESULT, 0);
		return	0;

	case WM_WINDOWPOSCHANGING:
		EvWindowPosChanging((WINDOWPOS *)lParam);
		SetWindowLong(DWL_MSGRESULT, 0);
		return	0;

	case WM_WINDOWPOSCHANGED:
		EvWindowPosChanged((WINDOWPOS *)lParam);
		SetWindowLong(DWL_MSGRESULT, 0);
		return	0;

	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_NCLBUTTONUP:
	case WM_NCRBUTTONUP:
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_NCLBUTTONDOWN:
	case WM_NCRBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDBLCLK:
	case WM_NCLBUTTONDBLCLK:
	case WM_NCRBUTTONDBLCLK:
		EventButton(uMsg, (int)wParam, MAKEPOINTS(lParam));
		return	0;

	case WM_KEYUP:
	case WM_KEYDOWN:
		EventKey(uMsg, (int)wParam, (LONG)lParam);
		return	0;

	case WM_HSCROLL:
	case WM_VSCROLL:
		EventScroll(uMsg, LOWORD(wParam), HIWORD(wParam), (HWND)lParam);
		return	0;

	case WM_ENTERMENULOOP:
	case WM_EXITMENULOOP:
		EventMenuLoop(uMsg, (BOOL)wParam);
		break;

	case WM_INITMENU:
	case WM_INITMENUPOPUP:
		EventInitMenu(uMsg, (HMENU)wParam, LOWORD(lParam), (BOOL)HIWORD(lParam));
		return	0;

	case WM_MENUSELECT:
		EvMenuSelect(LOWORD(wParam), (BOOL)HIWORD(wParam), (HMENU)lParam);
		return	0;

	case WM_CTLCOLORBTN:
	case WM_CTLCOLORDLG:
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORLISTBOX:
	case WM_CTLCOLORMSGBOX:
	case WM_CTLCOLORSCROLLBAR:
	case WM_CTLCOLORSTATIC:
		EventCtlColor(uMsg, (HDC)wParam, (HWND)lParam, (HBRUSH *)&result);
		SetWindowLong(DWL_MSGRESULT, result);
		return	result;

	case WM_KILLFOCUS:
	case WM_SETFOCUS:
		EventFocus(uMsg, (HWND)wParam);
		return	0;

	default:
		if (uMsg >= WM_APP && uMsg <= 0xBFFF) {
			result = EventApp(uMsg, wParam, lParam);
		}
		else if (uMsg >= WM_USER && uMsg < WM_APP || uMsg >= 0xC000 && uMsg <= 0xFFFF) {
			result = EventUser(uMsg, wParam, lParam);
		}
		else {
			result = EventSystem(uMsg, wParam, lParam);
		}
		SetWindowLong(DWL_MSGRESULT, result);
		return	result;
	}

	return	FALSE;
}

BOOL TDlg::PreProcMsg(MSG *msg)
{
	if (hAccel && ::TranslateAccelerator(hWnd, hAccel, msg))
		return	TRUE;

	if (!modalFlg)
		return	::IsDialogMessage(hWnd, msg);

	return	FALSE;
}

BOOL TDlg::EvSysCommand(WPARAM uCmdType, POINTS pos)
{
	return	FALSE;
}

BOOL TDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID)
	{
	case IDOK: case IDCANCEL: case IDYES: case IDNO:
	case IDABORT: case IDIGNORE: case IDRETRY:
		EndDialog(wID);
		return	TRUE;
	}

	return	FALSE;
}

BOOL TDlg::EvQueryOpen(void)
{
	return	FALSE;
}

BOOL TDlg::EvCreate(LPARAM lParam)
{
	return	TRUE;
}

void TDlg::EndDialog(int result)
{
	if (::IsWindow(hWnd))
	{
		if (modalFlg)
			::EndDialog(hWnd, result);
		else
			::DestroyWindow(hWnd);
	}
}

int TDlg::SetDlgItem(UINT ctl_id, DWORD flags)
{
	WINDOWPLACEMENT wp;
	wp.length = sizeof(wp);

	for (int i=0; i < maxItems; i++) {
		DlgItem *item = dlgItems + i;
		if (item->id == ctl_id) {
			item->hWnd = GetDlgItem(ctl_id);
			item->flags = flags;
			return i;
		}
	}

#define BIG_ALLOC 50
	if ((maxItems % BIG_ALLOC) == 0) {
		DlgItem *p = (DlgItem *)realloc(dlgItems, (maxItems + BIG_ALLOC) * sizeof(DlgItem));
		if (!p) return -1;
		dlgItems = p;
	}
	DlgItem *item = dlgItems + maxItems;

	item->hWnd = GetDlgItem(ctl_id);
	::GetWindowPlacement(item->hWnd, &wp);
	item->wpos.x = wp.rcNormalPosition.left;
	item->wpos.y = wp.rcNormalPosition.top;
	item->wpos.cx = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
	item->wpos.cy = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
	item->flags = flags;
	item->id = ctl_id;

	return	maxItems++;
}

BOOL TDlg::FitDlgItems()
{
	GetWindowRect(&rect);
	int	xdiff = (rect.right - rect.left) - (orgRect.right - orgRect.left);
	int ydiff = (rect.bottom - rect.top) - (orgRect.bottom - orgRect.top);

	HDWP	hdwp = ::BeginDeferWindowPos(maxItems);	// MAX item number
	UINT	dwFlg = SWP_SHOWWINDOW | SWP_NOZORDER;

	for (int i=0; i < maxItems; i++) {
		DlgItem *item = dlgItems + i;
		DWORD	f = item->flags;

		if (f & FIT_SKIP) continue;
		int x = (f & LEFT_FIT) == LEFT_FIT ? item->wpos.x :
				(f & HMID_FIT) == HMID_FIT ? item->wpos.x + xdiff/2 : item->wpos.x + xdiff;
		int y = (f & TOP_FIT)  == TOP_FIT  ? item->wpos.y :
				(f & VMID_FIT) == VMID_FIT ? item->wpos.y + ydiff/2 : item->wpos.y + ydiff;
		int w = (f & X_FIT)    == X_FIT    ? item->wpos.cx + xdiff : item->wpos.cx;
		int h = (f & Y_FIT)    == Y_FIT    ? item->wpos.cy + ydiff : item->wpos.cy;

		if (!(hdwp = ::DeferWindowPos(hdwp, item->hWnd, 0, x, y, w, h, dwFlg))) return FALSE;
	}
	::EndDeferWindowPos(hdwp);

	return	TRUE;
}
