static char *twin_id = 
	"@(#)Copyright (C) 1996-2017 H.Shirouzu		twin.cpp	Ver0.99";
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Window Class
	Create					: 1996-06-01(Sat)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "tlib.h"

TWin::TWin(TWin *_parent)
{
	hWnd		= NULL;
	hTipWnd		= NULL;
	hAccel		= NULL;
	rect.InitDefault();
	orgRect		= rect;
	pRect.InitDefault();
	parent		= _parent;
	sleepBusy	= FALSE;
	isUnicode	= TRUE;
	scrollHack	= TRUE;
	modalCount	= 0;
	twinId		= TApp::GenTWinID();
}

TWin::~TWin()
{
	Destroy();
}

BOOL TWin::Create(LPCSTR className, LPCSTR title, DWORD style, DWORD exStyle, HMENU hMenu)
{
	Wstr	className_w(className, BY_MBCS);
	Wstr	title_w(title, BY_MBCS);

	return	CreateW(className_w.s(), title_w.s(), style, exStyle, hMenu);
}

BOOL TWin::CreateW(const WCHAR *className, const WCHAR *title, DWORD style, DWORD exStyle,
	HMENU hMenu)
{
	if (className == NULL || !*className) {
		className = TApp::GetApp()->GetDefaultClassW();
	}

	TApp::GetApp()->AddWin(this);

	if (!(hWnd = ::CreateWindowExW(exStyle, className, title, style,
			rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
			parent ? parent->hWnd : NULL, hMenu, TApp::hInst(), NULL))) {
		TApp::GetApp()->DelWin(this);
		Debug("*** TWin::CreateW Failed(%d) ***\n", GetLastError());
		return	FALSE;
	}

	return	TRUE;
}

void TWin::Destroy(void)
{
	if (hTipWnd) {
		CloseTipWnd();
	}

	if (hWnd) {
		::DestroyWindow(hWnd);
	}
}

void TWin::Show(int mode)
{
	if (hWnd) {
		::ShowWindow(hWnd, mode);
		::UpdateWindow(hWnd);
	}
}

LRESULT TWin::WinProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	BOOL	done = FALSE;
	LRESULT	result = 0;

	switch(uMsg)
	{
	case WM_CREATE:
		GetWindowRect(&orgRect);
		done = EvCreate(lParam);
		break;

	case WM_CLOSE:
		done = EvClose();
		break;

	case WM_COMMAND:
		done = EvCommand(HIWORD(wParam), LOWORD(wParam), lParam);
		break;

	case WM_SYSCOMMAND:
		done = EvSysCommand(wParam, MAKEPOINTS(lParam));
		break;

	case WM_TIMER:
		done = EvTimer(wParam, (TIMERPROC)lParam);
		break;

	case WM_DESTROY:
		done = EvDestroy();
		break;

	case WM_NCDESTROY:
		if (!::IsIconic(hWnd)) {
			GetWindowRect(&rect);
		}
		if (parent && parent->hWnd) {
			parent->GetWindowRect(&pRect);
		}
		if (!EvNcDestroy()) {	// hWndを0にする前に呼び出す
			DefWindowProc(uMsg, wParam, lParam);
		}
		done = TRUE;
		TApp::GetApp()->DelWin(this);
		hWnd = 0;
		break;

	case WM_QUERYENDSESSION:
		result = EvQueryEndSession((BOOL)wParam, (BOOL)lParam);
		done = TRUE;
		break;

	case WM_ENDSESSION:
		done = EvEndSession((BOOL)wParam, (BOOL)lParam);
		break;

	case WM_POWERBROADCAST:
		done = EvPowerBroadcast(wParam, lParam);
		break;

	case WM_QUERYOPEN:
		result = EvQueryOpen();
		done = TRUE;
		break;

	case WM_PAINT:
		done = EvPaint();
		break;

	case WM_NCPAINT:
		done = EvNcPaint((HRGN)wParam);
		break;

	case WM_PRINT:
	case WM_PRINTCLIENT:
		done = EventPrint(uMsg, (HDC)wParam, (DWORD)lParam);
		return	0;

	case WM_SIZE:
		done = EvSize((UINT)wParam, LOWORD(lParam), HIWORD(lParam));
		break;

	case WM_MOVE:
		done = EvMove(LOWORD(lParam), HIWORD(lParam));
		break;

	case WM_SHOWWINDOW:
		done = EvShowWindow((BOOL)wParam, (int)lParam);
		break;

	case WM_GETMINMAXINFO:
		done = EvGetMinMaxInfo((MINMAXINFO *)lParam);
		break;

	case WM_SETCURSOR:
		result = done = EvSetCursor((HWND)wParam, LOWORD(lParam), HIWORD(lParam));
		break;

	case WM_MOUSEMOVE:
		done = EvMouseMove((UINT)wParam, MAKEPOINTS(lParam));
		break;

	case WM_NCHITTEST:
		done = EvNcHitTest(MAKEPOINTS(lParam), &result);
		break;

	case WM_MEASUREITEM:
		result = done = EvMeasureItem((UINT)wParam, (LPMEASUREITEMSTRUCT)lParam);
		break;

	case WM_DRAWITEM:
		result = done = EvDrawItem((UINT)wParam, (LPDRAWITEMSTRUCT)lParam);
		break;

	case WM_NOTIFY:
		result = done = EvNotify((UINT)wParam, (LPNMHDR)lParam);
		break;

	case WM_CONTEXTMENU:
		result = done = EvContextMenu((HWND)wParam, MAKEPOINTS(lParam));
		break;

	case WM_HOTKEY:
		result = done = EvHotKey((int)wParam);
		break;

	case WM_ACTIVATEAPP:
		done = EvActivateApp((BOOL)wParam, (DWORD)lParam);
		break;

	case WM_ACTIVATE:
		EvActivate(LOWORD(wParam), HIWORD(wParam), (HWND)lParam);
		break;

	case WM_DROPFILES:
		done = EvDropFiles((HDROP)wParam);
		break;

	case WM_CHAR:
		done = EvChar((WCHAR)wParam, lParam);
		break;

	case WM_WINDOWPOSCHANGING:
		done = EvWindowPosChanging((WINDOWPOS *)lParam);
		break;

	case WM_WINDOWPOSCHANGED:
		done = EvWindowPosChanged((WINDOWPOS *)lParam);
		break;

	case WM_MOUSEWHEEL:
		done = EvMouseWheel(GET_KEYSTATE_WPARAM(wParam), GET_WHEEL_DELTA_WPARAM(wParam),
			GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;

	case WM_COPY:
		done = EvCopy();
		break;

	case WM_PASTE:
		done = EvPaste();
		break;

	case WM_CLEAR:
		done = EvClear();
		break;

	case WM_CUT:
		done = EvCut();
		break;

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
		done = EventButton(uMsg, (int)wParam, MAKEPOINTS(lParam));
		break;

	case WM_KEYUP:
	case WM_KEYDOWN:
		done = EventKey(uMsg, (int)wParam, (LONG)lParam);
		break;

	case WM_HSCROLL:
	case WM_VSCROLL:
		done = EventScrollWrapper(uMsg, LOWORD(wParam), HIWORD(wParam), (HWND)lParam);
		break;

	case WM_ENTERMENULOOP:
	case WM_EXITMENULOOP:
		done = EventMenuLoop(uMsg, (BOOL)wParam);
		break;

	case WM_INITMENU:
	case WM_INITMENUPOPUP:
		done = EventInitMenu(uMsg, (HMENU)wParam, LOWORD(lParam), (BOOL)HIWORD(lParam));
		break;

	case WM_MENUSELECT:
		done = EvMenuSelect(LOWORD(wParam), HIWORD(wParam), (HMENU)lParam);
		break;

	case WM_CTLCOLORBTN:
	case WM_CTLCOLORDLG:
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORLISTBOX:
	case WM_CTLCOLORMSGBOX:
	case WM_CTLCOLORSCROLLBAR:
	case WM_CTLCOLORSTATIC:
		done = EventCtlColor(uMsg, (HDC)wParam, (HWND)lParam, (HBRUSH *)&result);
		break;

	case WM_KILLFOCUS:
	case WM_SETFOCUS:
		done = EventFocus(uMsg, (HWND)wParam);
		break;

	default:
		if (uMsg >= WM_APP && uMsg <= 0xBFFF) {
			result = done = EventApp(uMsg, wParam, lParam);
		}
		else if (uMsg >= WM_USER && uMsg < WM_APP || uMsg >= 0xC000 && uMsg <= 0xFFFF) {
			result = done = EventUser(uMsg, wParam, lParam);
		}
		else {
			result = done = EventSystem(uMsg, wParam, lParam);
		}
		break;
	}

	return	done ? result : DefWindowProc(uMsg, wParam, lParam);
}

LRESULT TWin::DefWindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (!hWnd) {
		return	0;
	}
	return	isUnicode ? ::DefWindowProcW(hWnd, uMsg, wParam, lParam) :
						::DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

BOOL TWin::PreProcMsg(MSG *msg)
{
	return	TranslateAccelerator(msg);
}

BOOL TWin::TranslateAccelerator(MSG *msg)
{
	if (hWnd && hAccel) {
		return	isUnicode ? ::TranslateAcceleratorW(hWnd, hAccel, msg) :
							::TranslateAcceleratorA(hWnd, hAccel, msg);
	}
	return	FALSE;
}

BOOL TWin::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	return	FALSE;
}

BOOL TWin::EvSysCommand(WPARAM uCmdType, POINTS pos)
{
	return	FALSE;
}

BOOL TWin::EvCreate(LPARAM lParam)
{
	return	FALSE;
}

BOOL TWin::EvClose(void)
{
	return	FALSE;
}


BOOL TWin::EvMeasureItem(UINT ctlID, MEASUREITEMSTRUCT *lpMis)
{
	return	FALSE;
}

BOOL TWin::EvDrawItem(UINT ctlID, DRAWITEMSTRUCT *lpDis)
{
	return	FALSE;
}

BOOL TWin::EvDestroy(void)
{
	return	FALSE;
}

BOOL TWin::EvNcDestroy(void)
{
	return	FALSE;
}

BOOL TWin::EvTimer(WPARAM timerID, TIMERPROC proc)
{
	return	FALSE;
}

BOOL TWin::Sleep(UINT mSec)
{
	if (mSec == 0 || sleepBusy)
		return	TRUE;

	if (!SetTimer(TLIB_SLEEPTIMER, mSec))
		return	FALSE;
	sleepBusy = TRUE;

	MSG		msg;
	while (isUnicode ?  ::GetMessageW(&msg, 0, 0, 0) :
						::GetMessageA(&msg, 0, 0, 0))
	{
		if (msg.hwnd == hWnd && msg.wParam == TLIB_SLEEPTIMER)
		{
			KillTimer(TLIB_SLEEPTIMER);
			break;
		}
		if (TApp::GetApp()->PreProcMsg(&msg))
			continue;

		::TranslateMessage(&msg);
		isUnicode ? ::DispatchMessageW(&msg) :
					::DispatchMessageA(&msg);
	}
	sleepBusy = FALSE;

	return	TRUE;
}

BOOL TWin::EvQueryEndSession(BOOL nSession, BOOL nLogOut)
{
	return	TRUE;
}

BOOL TWin::EvEndSession(BOOL nSession, BOOL nLogOut)
{
	return	TRUE;
}

BOOL TWin::EvPowerBroadcast(WPARAM pbtEvent, LPARAM pbtData)
{
	return	TRUE;
}

BOOL TWin::EvQueryOpen(void)
{
	return	TRUE;
}

BOOL TWin::EvPaint(void)
{
	return	FALSE;
}

BOOL TWin::EvNcPaint(HRGN hRgn)
{
	return	FALSE;
}

BOOL TWin::EventPrint(UINT uMsg, HDC hDc, DWORD opt)
{
	return	FALSE;
}

BOOL TWin::EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight)
{
	return	FALSE;
}

BOOL TWin::EvMove(int xpos, int ypos)
{
	return	FALSE;
}

BOOL TWin::EvShowWindow(BOOL fShow, int fnStatus)
{
	return	FALSE;
}

BOOL TWin::EvGetMinMaxInfo(MINMAXINFO *info)
{
	return	FALSE;
}

BOOL TWin::EvSetCursor(HWND cursorWnd, WORD nHitTest, WORD wMouseMsg)
{
	return	FALSE;
}

BOOL TWin::EvMouseMove(UINT fwKeys, POINTS pos)
{
	return	FALSE;
}

BOOL TWin::EvNcHitTest(POINTS pos, LRESULT *result)
{
	return	FALSE;
}

BOOL TWin::EvNotify(UINT ctlID, NMHDR *pNmHdr)
{
	return	FALSE;
}

BOOL TWin::EvContextMenu(HWND childWnd, POINTS pos)
{
	return	FALSE;
}

BOOL TWin::EvHotKey(int hotKey)
{
	return	FALSE;
}

BOOL TWin::EvActivateApp(BOOL fActivate, DWORD dwThreadID)
{
	return	FALSE;
}

BOOL TWin::EvActivate(BOOL fActivate, DWORD fMinimized, HWND hActiveWnd)
{
	return	FALSE;
}

BOOL TWin::EvWindowPosChanging(WINDOWPOS *pos)
{
	return	FALSE;
}

BOOL TWin::EvWindowPosChanged(WINDOWPOS *pos)
{
	return	FALSE;
}

BOOL TWin::EvMouseWheel(WORD fwKeys, short zDelta, short xPos, short yPos)
{
	return	FALSE;
}

BOOL TWin::EvCopy()
{
	return	FALSE;
}

BOOL TWin::EvPaste()
{
	return	FALSE;
}

BOOL TWin::EvCut()
{
	return	FALSE;
}

BOOL TWin::EvClear()
{
	return	FALSE;
}

BOOL TWin::EvChar(WCHAR code, LPARAM keyData)
{
	return	FALSE;
}

BOOL TWin::EventScrollWrapper(UINT uMsg, int nCode, int nPos, HWND scrollBar)
{
	if (scrollHack) {
		// 32bit対応スクロール変換
		if (nCode == SB_THUMBTRACK || nCode == SB_THUMBPOSITION) {
			SCROLLINFO	si = { sizeof(si), SIF_TRACKPOS };
			if (::GetScrollInfo(hWnd, uMsg == WM_HSCROLL ? SB_HORZ : SB_VERT, &si)) {
				nPos = si.nTrackPos;
			}
		}
	}
	return	EventScroll(uMsg, nCode, nPos, scrollBar);
}

BOOL TWin::EventScroll(UINT uMsg, int nCode, int nPos, HWND scrollBar)
{
	scrollHack = FALSE;
	return	FALSE;
}

BOOL TWin::EventButton(UINT uMsg, int nHitTest, POINTS pos)
{
	return	FALSE;
}

BOOL TWin::EventKey(UINT uMsg, int nVirtKey, LONG lKeyData)
{
	return	FALSE;
}

BOOL TWin::EventMenuLoop(UINT uMsg, BOOL fIsTrackPopupMenu)
{
	return	FALSE;
}

BOOL TWin::EventInitMenu(UINT uMsg, HMENU hMenu, UINT uPos, BOOL fSystemMenu)
{
	return	FALSE;
}

BOOL TWin::EvMenuSelect(UINT uItem, UINT fuFlag, HMENU hMenu)
{
	return	FALSE;
}

BOOL TWin::EvDropFiles(HDROP hDrop)
{
	return	FALSE;
}

BOOL TWin::EventCtlColor(UINT uMsg, HDC hDcCtl, HWND hWndCtl, HBRUSH *result)
{
	return	FALSE;
}

BOOL TWin::EventFocus(UINT uMsg, HWND hFocusWnd)
{
	return	FALSE;
}

BOOL TWin::EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return	FALSE;
}

BOOL TWin::EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return	FALSE;
}

BOOL TWin::EventSystem(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return	FALSE;
}

UINT TWin::GetDlgItemText(int ctlId, LPSTR buf, int len)
{
	return	::GetDlgItemText(hWnd, ctlId, buf, len);
}

UINT TWin::GetDlgItemTextW(int ctlId, WCHAR *buf, int len)
{
	return	::GetDlgItemTextW(hWnd, ctlId, buf, len);
}

BOOL TWin::SetDlgItemText(int ctlId, LPCSTR buf)
{
	return	::SetDlgItemText(hWnd, ctlId, buf);
}

BOOL TWin::SetDlgItemTextW(int ctlId, const WCHAR *buf)
{
	return	::SetDlgItemTextW(hWnd, ctlId, buf);
}

int TWin::GetDlgItemInt(int ctlId, BOOL *err, BOOL sign)
{
	return	(int)::GetDlgItemInt(hWnd, ctlId, err, sign);
}

int64 TWin::GetDlgItemInt64(int ctlId, BOOL *err, BOOL sign)
{
	WCHAR	wbuf[128];

	if (!::GetDlgItemTextW(hWnd, ctlId, wbuf, wsizeof(wbuf))) {
		if (err) {
			*err = TRUE;
		}
		return	0;
	}

	if (sign) {
		return	wcstoll(wbuf, 0, 10);
	} else {
		return	(int64)wcstoull(wbuf, 0, 10);
	}
}

BOOL TWin::SetDlgItemInt(int ctlId, int val, BOOL sign)
{
	return	::SetDlgItemInt(hWnd, ctlId, val, sign);
}

BOOL TWin::SetDlgItemInt64(int ctlId, int64 val, BOOL sign)
{
	WCHAR	wbuf[128];
	snwprintfz(wbuf, wsizeof(wbuf), sign ? L"%lld" : L"%llu", val);
	return	::SetDlgItemTextW(hWnd, ctlId, wbuf);
}

HWND TWin::GetDlgItem(int ctlId)
{
	return	::GetDlgItem(hWnd, ctlId);
}

BOOL TWin::CheckDlgButton(int ctlId, UINT check)
{
	return	::CheckDlgButton(hWnd, ctlId, check);
}

UINT TWin::IsDlgButtonChecked(int ctlId)
{
	return	::IsDlgButtonChecked(hWnd, ctlId);
}

BOOL TWin::IsWindowVisible(void)
{
	return	::IsWindowVisible(hWnd);
}

BOOL TWin::EnableWindow(BOOL is_enable)
{
	return	::EnableWindow(hWnd, is_enable);
}

int TWin::MessageBox(LPCSTR msg, LPCSTR title, UINT style)
{
	modalCount++;
	int ret = ::MessageBox(hWnd, msg, title, style);
	modalCount--;

	return	ret;
}

int TWin::MessageBoxW(LPCWSTR msg, LPCWSTR title, UINT style)
{
	modalCount++;
	int ret = ::MessageBoxW(hWnd, msg, title, style);
	modalCount--;

	return	ret;
}

BOOL TWin::BringWindowToTop(void)
{
	if (!hWnd) {
		return	FALSE;
	}
	return	::BringWindowToTop(hWnd);
}

BOOL TWin::PostMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (!hWnd) {
		return	FALSE;
	}
	return	::PostMessage(hWnd, uMsg, wParam, lParam);
}

BOOL TWin::PostMessageW(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (!hWnd) {
		return	FALSE;
	}
	return	::PostMessageW(hWnd, uMsg, wParam, lParam);
}

LRESULT TWin::SendMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (!hWnd) {
		return	FALSE;
	}
	return	::SendMessage(hWnd, uMsg, wParam, lParam);
}

LRESULT TWin::SendMessageW(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (!hWnd) {
		return	FALSE;
	}
	return	::SendMessageW(hWnd, uMsg, wParam, lParam);
}

LRESULT TWin::SendDlgItemMessage(int idCtl, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return	::SendDlgItemMessage(hWnd, idCtl, uMsg, wParam, lParam);
}

LRESULT TWin::SendDlgItemMessageW(int idCtl, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return	::SendDlgItemMessageW(hWnd, idCtl, uMsg, wParam, lParam);
}

BOOL TWin::GetWindowRect(RECT *_rect)
{
	return	::GetWindowRect(hWnd, _rect ? _rect : &rect);
}

BOOL TWin::GetClientRect(RECT *rc)
{
	return	::GetClientRect(hWnd, rc);
}

BOOL TWin::SetWindowPos(HWND hInsAfter, int x, int y, int cx, int cy, UINT fuFlags)
{
	if (!hWnd) {
		return	FALSE;
	}
	return	::SetWindowPos(hWnd, hInsAfter, x, y, cx, cy, fuFlags);
}

BOOL TWin::ShowWindow(int mode)
{
	if (!hWnd) {
		return	FALSE;
	}
	return	::ShowWindow(hWnd, mode);
}

BOOL TWin::SetForegroundWindow(void)
{
	if (!hWnd) {
		return	FALSE;
	}
	return	::SetForegroundWindow(hWnd);
}

BOOL TWin::SetForceForegroundWindow(BOOL revert_thread_input)
{
	if (!hWnd) {
		return	FALSE;
	}
	DWORD	foreId, targId, svTmOut;

	if (IsWinVista()) {
		TSwitchToThisWindow(hWnd, TRUE);
	}

	foreId = ::GetWindowThreadProcessId(::GetForegroundWindow(), NULL);
	targId = ::GetWindowThreadProcessId(hWnd, NULL);
	if (foreId != targId) {
		::AttachThreadInput(targId, foreId, TRUE);
		BringWindowToTop();
	}
	::SystemParametersInfo(SPI_GETFOREGROUNDLOCKTIMEOUT, 0, (void *)&svTmOut, 0);
	::SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, 0, 0);
	BOOL	ret = ::SetForegroundWindow(hWnd);
	::SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, (void *)(DWORD_PTR)svTmOut, 0);
	if (foreId != targId && revert_thread_input) {
		::AttachThreadInput(targId, foreId, FALSE);
	}

	return	ret;
}

HWND TWin::SetActiveWindow(void)
{
	if (!hWnd) {
		return	FALSE;
	}
	return	::SetActiveWindow(hWnd);
}

int TWin::GetWindowText(LPSTR text, int size)
{
	return	::GetWindowText(hWnd, text, size);
}

int TWin::GetWindowTextW(WCHAR *text, int size)
{
	return	::GetWindowTextW(hWnd, text, size);
}

BOOL TWin::SetWindowTextW(const WCHAR *text)
{
	return	::SetWindowTextW(hWnd, text);
}

int TWin::GetWindowTextLengthW(void)
{
	return	::GetWindowTextLengthW(hWnd);
}

BOOL TWin::SetWindowText(LPCSTR text)
{
	return	::SetWindowText(hWnd, text);
}

LONG_PTR TWin::SetWindowLong(int index, LONG_PTR val)
{
	if (!hWnd) {
		return	0;
	}
	return	isUnicode ? ::SetWindowLongW(hWnd, index, val) :
						::SetWindowLongA(hWnd, index, val);
}

LONG_PTR TWin::GetWindowLong(int index)
{
	if (!hWnd) {
		return	0;
	}
	return	isUnicode ? ::GetWindowLongW(hWnd, index) :
						::GetWindowLongA(hWnd, index);
}

UINT_PTR TWin::SetTimer(UINT_PTR idTimer, UINT uTimeout, TIMERPROC proc)
{
	if (!hWnd) {
		return	0;
	}
	return	::SetTimer(hWnd, idTimer, uTimeout, proc);
}

BOOL TWin::KillTimer(UINT_PTR idTimer)
{
	if (!hWnd) {
		return	FALSE;
	}
	return	::KillTimer(hWnd, idTimer);
}

BOOL TWin::MoveWindow(int x, int y, int cx, int cy, int bRepaint)
{
	if (!hWnd) {
		return	FALSE;
	}
	return	::MoveWindow(hWnd, x, y, cx, cy, bRepaint);
}

BOOL TWin::MoveCenter(BOOL use_cursor_screen)
{
	TRect	screen_rect(0, 0,
		::GetSystemMetrics(SM_CXFULLSCREEN),
		::GetSystemMetrics(SM_CYFULLSCREEN));

	if (use_cursor_screen) {
		POINT		pt = {0, 0};
		::GetCursorPos(&pt);
		HMONITOR	hMon = ::MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);

		if (hMon) {
			MONITORINFO	minfo;
			minfo.cbSize = sizeof(minfo);

			if (::GetMonitorInfoW(hMon, &minfo) &&
				(minfo.rcMonitor.right - minfo.rcMonitor.left) > 0 &&
				(minfo.rcMonitor.bottom - minfo.rcMonitor.top) > 0) {
				screen_rect = minfo.rcMonitor;
			}
		}
	}

	TRect	rc;
	GetWindowRect(&rc);
	long x = screen_rect.left + (screen_rect.cx() - rc.cx()) / 2;
	long y = screen_rect.top  + (screen_rect.cy() - rc.cy()) / 2;

	return	SetWindowPos(NULL, x, y, 0, 0, SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
}

BOOL TWin::FitMoveWindow(int x, int y)
{
	if (!hWnd) {
		return	FALSE;
	}
	RECT	rc;
	GetWindowRect(&rc);
	int		cx = rc.right - rc.left;
	int		cy = rc.bottom - rc.top;

	int	start_x = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
	int	start_y = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
	int	end_x = start_x + ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
	int	end_y = start_y + ::GetSystemMetrics(SM_CYVIRTUALSCREEN);

	if (x + cx > end_x) x = end_x - cx;
	if (x < start_x)    x = start_x;
	if (y + cy > end_y) y = end_y - cy;
	if (y < start_y)    y = start_y;

	return	SetWindowPos(NULL, x, y, 0, 0, SWP_NOSIZE|SWP_NOREDRAW|SWP_NOZORDER);
}

BOOL TWin::Idle(void)
{
	MSG		msg;

	if (isUnicode ? ::PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE) :
					::PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
	{
		if (TApp::GetApp()->PreProcMsg(&msg))
			return	TRUE;

		::TranslateMessage(&msg);
		isUnicode ? ::DispatchMessageW(&msg) :
					::DispatchMessageA(&msg);
		return	TRUE;
	}

	return	FALSE;
}

BOOL TWin::CreateU8(LPCSTR className, LPCSTR title, DWORD style, DWORD exStyle, HMENU hMenu)
{
	Wstr	className_w(className, BY_UTF8);
	Wstr	title_w(title, BY_UTF8);

	return	CreateW(className_w.s(), title_w.s(), style, exStyle, hMenu);
}

UINT TWin::GetDlgItemTextU8(int ctlId, char *buf, int len)
{
	Wstr	wbuf(len);

	*buf = 0;
	GetDlgItemTextW(ctlId, wbuf.Buf(), len);

	return	WtoU8(wbuf.s(), buf, len);
}

BOOL TWin::SetDlgItemTextU8(int ctlId, const char *buf)
{
	Wstr	wbuf(buf);

	return	::SetDlgItemTextW(hWnd, ctlId, wbuf.s());
}

int TWin::MessageBoxU8(LPCSTR msg, LPCSTR title, UINT style)
{
	Wstr	wmsg(msg);
	Wstr	wtitle(title);

	
	return	MessageBoxW(wmsg.s(), wtitle.s(), style);
}

int TWin::GetWindowTextU8(char *text, int len)
{
	Wstr	wbuf(len);

	wbuf[0] = 0;
	if (::GetWindowTextW(hWnd, wbuf.Buf(), len) < 0) return -1;

	return	WtoU8(wbuf.s(), text, len);
}

BOOL TWin::SetWindowTextU8(const char *text)
{
	Wstr	wbuf(text);

	return	::SetWindowTextW(hWnd, wbuf.s());
}

int TWin::GetWindowTextLengthU8(void)
{
	int		len = ::GetWindowTextLengthW(hWnd);
	Wstr	wbuf(len + 1);

	if (::GetWindowTextW(hWnd, wbuf.Buf(), len + 1) <= 0) return 0;

	return	WtoU8(wbuf.s(), NULL, 0);
}

BOOL TWin::InvalidateRect(const RECT *rc, BOOL fErase)
{
	if (!hWnd) {
		return	FALSE;
	}
	return	::InvalidateRect(hWnd, rc, fErase);
}

HWND TWin::SetFocus()
{
	if (!hWnd) {
		return	NULL;
	}
	return	::SetFocus(hWnd);
}

BOOL TWin::RestoreRectFromParent()
{
	if (pRect.left == CW_USEDEFAULT || !parent || !parent->hWnd) {
		return FALSE;
	}
	TRect	pCurRect;
	if (!parent->GetWindowRect(&pCurRect)) return FALSE;

	rect.left	= rect.left   + (pCurRect.left   - pRect.left);
	rect.right	= rect.right  + (pCurRect.right  - pRect.right);
	rect.top	= rect.top    + (pCurRect.top    - pRect.top);
	rect.bottom	= rect.bottom + (pCurRect.bottom - pRect.bottom);

	return	TRUE;
}

BOOL TWin::CreateTipWnd(const WCHAR *tip, int width, int tout)
{
	if (hTipWnd) {
		CloseTipWnd();
	}
	if (hWnd) {
		hTipWnd = ::CreateWindowExW(0, TOOLTIPS_CLASSW, NULL, TTS_ALWAYSTIP,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			hWnd, NULL, TApp::hInst(), NULL);

		SetTipTextW(tip, width, tout);
	}
	return	hTipWnd ? TRUE : FALSE;
}

BOOL TWin::SetTipTextW(const WCHAR *tip, int width, int tout)
{
	if (!hWnd || !hTipWnd) {
		return	FALSE;
	}

	if (width) {
		::SendMessageW(hTipWnd, TTM_SETMAXTIPWIDTH, 0, width);
	}
	if (tout) {
		::SendMessageW(hTipWnd, TTM_SETDELAYTIME, TTDT_AUTOPOP, tout);
	}

	if (tip) {
		TOOLINFOW ti = { sizeof(TOOLINFOW) };
		GetClientRect(&ti.rect);

		ti.uFlags = TTF_SUBCLASS;
		ti.hwnd = hWnd;
		ti.lpszText = (WCHAR *)tip;

		::SendMessageW(hTipWnd, TTM_ADDTOOLW, 0, (LPARAM)&ti);
	}

	return	TRUE;
}

BOOL TWin::UnSetTipText()
{
	if (!hWnd || !hTipWnd) {
		return	FALSE;
	}

	TOOLINFOW ti = { sizeof(TOOLINFOW) };
	GetClientRect(&ti.rect);

	ti.uFlags = TTF_SUBCLASS;
	ti.hwnd = hWnd;

	::SendMessageW(hTipWnd, TTM_DELTOOLW, 0, (LPARAM)&ti);

	return	TRUE;
}

BOOL TWin::CloseTipWnd()
{
	if (!hTipWnd) {
		return	FALSE;
	}

	::DestroyWindow(hTipWnd);
	hTipWnd = NULL;

	return	TRUE;
}

TSubClass::TSubClass(TWin *_parent) : TWin(_parent)
{
}

TSubClass::~TSubClass()
{
	if (oldProc && hWnd) DetachWnd();
}

BOOL TSubClass::AttachWnd(HWND _hWnd)
{
	TApp::GetApp()->AddWinByWnd(this, _hWnd);
	oldProc = (WNDPROC)SetWindowLong(GWL_WNDPROC, (LONG_PTR)TApp::WinProc);
	return	oldProc ? TRUE : FALSE;
}

BOOL TSubClass::DetachWnd()
{
	if (!oldProc || !hWnd) return FALSE;

	SetWindowLong(GWL_WNDPROC, (LONG_PTR)oldProc);
	oldProc = NULL;
	return	TRUE;
}

LRESULT TSubClass::DefWindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return	isUnicode ? ::CallWindowProcW((WNDPROC)oldProc, hWnd, uMsg, wParam, lParam) :
						::CallWindowProcA((WNDPROC)oldProc, hWnd, uMsg, wParam, lParam);
}

TSubClassCtl::TSubClassCtl(TWin *_parent) : TSubClass(_parent)
{
}

BOOL TSubClassCtl::PreProcMsg(MSG *msg)
{
	if (parent) {
		return	parent->PreProcMsg(msg);
	}

	return	FALSE;
}

TCtl::TCtl(TWin *_parent) : TWin(_parent)
{
}

BOOL TCtl::PreProcMsg(MSG *msg)
{
	if (parent) {
		return	parent->PreProcMsg(msg);
	}

	return	FALSE;
}

