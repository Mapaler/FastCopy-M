static char *tapp_id = 
	"@(#)Copyright (C) 1996-2011 H.Shirouzu		tapp.cpp	Ver0.99";
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Application Frame Class
	Create					: 1996-06-01(Sat)
	Update					: 2011-04-10(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "tlib.h"

TApp *TApp::tapp = NULL;
#define MAX_TAPPWIN_HASH	1009
#define ENGLISH_TEST		0

TApp::TApp(HINSTANCE _hI, LPSTR _cmdLine, int _nCmdShow)
{
	hI				= _hI;
	cmdLine			= _cmdLine;
	nCmdShow		= _nCmdShow;
	mainWnd			= NULL;
	defaultClass	= "tapp";
	defaultClassV	= IS_WINNT_V ? (void *)L"tapp" : (void *)"tapp";
	tapp			= this;
	hash			= new TWinHashTbl(MAX_TAPPWIN_HASH);

	InitInstanceForLoadStr(hI);
	TLibInit_Win32V();

#if ENGLISH_TEST
	TSetDefaultLCID(0x409); // for English Dialog Test
#else
	TSetDefaultLCID();
#endif
	::CoInitialize(NULL);
	::InitCommonControls();
}

TApp::~TApp()
{
	delete mainWnd;
	::CoUninitialize();
}

int TApp::Run(void)
{
	MSG		msg;

	InitApp();
	InitWindow();

	while (::GetMessage(&msg, NULL, 0, 0))
	{
		if (PreProcMsg(&msg))
			continue;

		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
	}

	return	(int)msg.wParam;
}

BOOL TApp::PreProcMsg(MSG *msg)	// for TranslateAccel & IsDialogMessage
{
	for (HWND hWnd=msg->hwnd; hWnd; hWnd=::GetParent(hWnd))
	{
		TWin	*win = SearchWnd(hWnd);

		if (win)
			return	win->PreProcMsg(msg);
	}

	return	FALSE;
}

LRESULT CALLBACK TApp::WinProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	TApp	*app = TApp::GetApp();
	TWin	*win = app->SearchWnd(hWnd);

	if (win)
		return	win->WinProc(uMsg, wParam, lParam);

	if ((win = app->preWnd))
	{
		app->preWnd = NULL;
		app->AddWinByWnd(win, hWnd);
		return	win->WinProc(uMsg, wParam, lParam);
	}

	return	::DefWindowProc(hWnd, uMsg, wParam, lParam);
}

BOOL TApp::InitApp(void)	// reference kwc
{
	WNDCLASSW wc;

	memset(&wc, 0, sizeof(wc));
	wc.style			= (CS_BYTEALIGNCLIENT | CS_BYTEALIGNWINDOW | CS_DBLCLKS);
	wc.lpfnWndProc		= WinProc;
	wc.cbClsExtra 		= 0;
	wc.cbWndExtra		= 0;
	wc.hInstance		= hI;
	wc.hIcon			= NULL;
	wc.hCursor			= LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground	= NULL;
	wc.lpszMenuName		= NULL;
	wc.lpszClassName	= (LPCWSTR)defaultClassV;

	if (::FindWindowV(defaultClassV, NULL) == NULL)
	{
		if (::RegisterClassV(&wc) == 0)
			return FALSE;
	}

	return	TRUE;
}

void TApp::Idle(DWORD timeout)
{
	TApp	*app = TApp::GetApp();
	DWORD	start = GetTickCount();
	MSG		msg;

	while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		if (app->PreProcMsg(&msg))
			continue;

		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
		if (GetTickCount() - start >= timeout) break;
	}
}

BOOL TRegisterClass(LPCSTR class_name, UINT style, HICON hIcon, HCURSOR hCursor,
	HBRUSH hbrBackground, int classExtra, int wndExtra, LPCSTR menu_str)
{
	WNDCLASS	wc;

	memset(&wc, 0, sizeof(wc));
	wc.style			= style;
	wc.lpfnWndProc		= TApp::WinProc;
	wc.cbClsExtra 		= classExtra;
	wc.cbWndExtra		= wndExtra;
	wc.hInstance		= TApp::GetInstance();
	wc.hIcon			= hIcon;
	wc.hCursor			= hCursor;
	wc.hbrBackground	= hbrBackground;
	wc.lpszMenuName		= menu_str;
	wc.lpszClassName	= class_name;

	return	::RegisterClass(&wc);
}

BOOL TRegisterClassV(const void *class_name, UINT style, HICON hIcon, HCURSOR hCursor,
	HBRUSH hbrBackground, int classExtra, int wndExtra, const void *menu_str)
{
	WNDCLASSW	wc;

	memset(&wc, 0, sizeof(wc));
	wc.style			= style;
	wc.lpfnWndProc		= TApp::WinProc;
	wc.cbClsExtra 		= classExtra;
	wc.cbWndExtra		= wndExtra;
	wc.hInstance		= TApp::GetInstance();
	wc.hIcon			= hIcon;
	wc.hCursor			= hCursor;
	wc.hbrBackground	= hbrBackground;
	wc.lpszMenuName		= (LPCWSTR)menu_str;
	wc.lpszClassName	= (LPCWSTR)class_name;

	return	::RegisterClassV(&wc);
}

BOOL TRegisterClassU8(LPCSTR class_name, UINT style, HICON hIcon, HCURSOR hCursor,
	HBRUSH hbrBackground, int classExtra, int wndExtra, LPCSTR menu_str)
{
	Wstr	class_name_w(class_name, BY_UTF8);
	Wstr	menu_str_w(menu_str, BY_UTF8);

	return	TRegisterClassV(class_name_w, style, hIcon, hCursor, hbrBackground, classExtra,
			wndExtra, menu_str_w);
}

