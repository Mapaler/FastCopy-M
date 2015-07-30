/* @(#)Copyright (C) 1996-2015 H.Shirouzu		tlib.h	Ver0.99 */
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Main Header
	Create					: 1996-06-01(Sat)
	Update					: 2015-06-22(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef TLIB_H
#define TLIB_H

#ifndef STRICT
#define STRICT
#endif

// for debug allocator (like a efence)
//#define REPLACE_DEBUG_ALLOCATOR
#ifdef REPLACE_DEBUG_ALLOCATOR
#define malloc  valloc
#define calloc  vcalloc
#define realloc vrealloc
#define free    vfree
#define strdup  vstrdup
#define wcsdup  vwcsdup
extern "C" {
void *valloc(size_t size);
void *vcalloc(size_t num, size_t ele);
void *vrealloc(void *d, size_t size);
void vfree(void *d);
char *vstrdup(const char *s);
//unsigned short *vwcsdup(const unsigned short *s);
}
#endif

/* for crypto api */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

/* for new version VC */
#if _MSC_VER >= 1400
#pragma warning ( disable : 4996 )
#else
#define LONG_PTR LONG
#endif
#pragma warning ( disable : 4355 )

typedef signed _int64 int64;
typedef unsigned _int64 uint64;

#include <winsock2.h>
#include <ws2tcpip.h>

#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <stdlib.h>

#include "commctrl.h"
#include <regstr.h>
#include <shlobj.h>
#include <tchar.h>
#include "tmisc.h"
#include "tapi32ex.h"
//#include "tapi32u8.h"	 /* describe last line */

extern DWORD TWinVersion;	// define in tmisc.cpp

#define WINEXEC_ERR_MAX		31
#define TLIB_SLEEPTIMER		32000

#define TLIB_CRYPT		0x00000001
#define TLIB_PROTECT	0x00000002

#define IsNewShell()	(LOBYTE(LOWORD(TWinVersion)) >= 4)

#define IsWin31()		(LOBYTE(LOWORD(TWinVersion)) == 3 && HIBYTE(LOWORD(TWinVersion)) < 20)
#define IsWin95()		(LOBYTE(LOWORD(TWinVersion)) >= 4 && TWinVersion >= 0x80000000)

#define IsWinNT350()	(LOBYTE(LOWORD(TWinVersion)) == 3 && TWinVersion < 0x80000000 \
							&& HIBYTE(LOWORD(TWinVersion)) == 50)
#define IsWinNT()		(LOBYTE(LOWORD(TWinVersion)) >= 4 && TWinVersion < 0x80000000)
#define IsWin2K()		(LOBYTE(LOWORD(TWinVersion)) >= 5 && TWinVersion < 0x80000000)
#define IsWinXP()		((LOBYTE(LOWORD(TWinVersion)) >= 6 || LOBYTE(LOWORD(TWinVersion)) == 5 \
							&& HIBYTE(LOWORD(TWinVersion)) >= 1) && TWinVersion < 0x80000000)
#define IsWinVista()	(LOBYTE(LOWORD(TWinVersion)) >= 6 && TWinVersion < 0x80000000)
#define IsWin7()		((LOBYTE(LOWORD(TWinVersion)) >= 7 || LOBYTE(LOWORD(TWinVersion)) == 6 \
							&& HIBYTE(LOWORD(TWinVersion)) >= 1) && TWinVersion < 0x80000000)
#define IsWin8()		((LOBYTE(LOWORD(TWinVersion)) >= 7 || LOBYTE(LOWORD(TWinVersion)) == 6 \
							&& HIBYTE(LOWORD(TWinVersion)) >= 2) && TWinVersion < 0x80000000)
/* manifest に supported OS の記述が必須 */
#define IsWin81()		((LOBYTE(LOWORD(TWinVersion)) >= 7 || LOBYTE(LOWORD(TWinVersion)) == 6 \
							&& HIBYTE(LOWORD(TWinVersion)) >= 3) && TWinVersion < 0x80000000)
#define IsWin10()		(LOBYTE(LOWORD(TWinVersion)) >= 10 && TWinVersion < 0x80000000)

#define IsLang(lang)	(PRIMARYLANGID(LANGIDFROMLCID(GetThreadLocale())) == lang)
#define wsizeof(x)		(sizeof(x) / sizeof(WCHAR))

#define BUTTON_CLASS		"BUTTON"
#define COMBOBOX_CLASS		"COMBOBOX"
#define EDIT_CLASS			"EDIT"
#define LISTBOX_CLASS		"LISTBOX"
#define MDICLIENT_CLASS		"MDICLIENT"
#define SCROLLBAR_CLASS		"SCROLLBAR"
#define STATIC_CLASS		"STATIC"

#define MAX_WPATH		32000
#define MAX_PATH_EX		(MAX_PATH * 8)
#define MAX_PATH_U8		(MAX_PATH * 3)
#define MAX_FNAME_LEN	255

#define BITS_OF_BYTE	8
#define BYTE_NUM		256

#ifndef EM_SETTARGETDEVICE
#define COLOR_BTNFACE           15
#define COLOR_3DFACE            COLOR_BTNFACE
#define EM_SETBKGNDCOLOR		(WM_USER + 67)
#define EM_SETTARGETDEVICE		(WM_USER + 72)
#endif

#ifndef WM_COPYGLOBALDATA
#define WM_COPYGLOBALDATA	0x0049
#endif

#ifndef CSIDL_PROGRAM_FILES
#define CSIDL_LOCAL_APPDATA		0x001c
#define CSIDL_WINDOWS			0x0024
#define CSIDL_PROGRAM_FILES		0x0026
#endif
#ifndef CSIDL_PROGRAM_FILESX86
#define CSIDL_PROGRAM_FILESX86	0x002a 
#endif

#ifndef CSIDL_APPDATA
#define CSIDL_APPDATA  0x1a
#endif

#ifndef CSIDL_COMMON_APPDATA
#define CSIDL_COMMON_APPDATA  0x23
#endif

#ifndef SHCNF_PATHW
#define SHCNF_PATHW 0x0005
#endif

#ifndef BCM_FIRST
#define BCM_FIRST     0x1600
#define IDI_SHIELD    1022
#endif

#ifndef BCM_SETSHIELD
#define BCM_SETSHIELD (BCM_FIRST + 0x000C)
#endif

#ifndef PROCESS_MODE_BACKGROUND_BEGIN
#define PROCESS_MODE_BACKGROUND_BEGIN 0x00100000
#define PROCESS_MODE_BACKGROUND_END   0x00200000
#endif

#ifndef SM_XVIRTUALSCREEN
#define SM_XVIRTUALSCREEN	76
#define SM_YVIRTUALSCREEN	77
#define SM_CXVIRTUALSCREEN	78
#define SM_CYVIRTUALSCREEN	79
#endif

#ifndef SPI_GETFOREGROUNDLOCKTIMEOUT
#define SPI_GETFOREGROUNDLOCKTIMEOUT 0x2000
#define SPI_SETFOREGROUNDLOCKTIMEOUT 0x2001
#endif

#ifndef SPI_GETMESSAGEDURATION
#define SPI_GETMESSAGEDURATION	0x2016
#define SPI_SETMESSAGEDURATION	0x2017
#endif

#ifdef _WIN64
#define SetClassLongA	SetClassLongPtrA
#define SetClassLongW	SetClassLongPtrW
#define GetClassLongA	GetClassLongPtrA
#define GetClassLongW	GetClassLongPtrW
#define SetWindowLongA	SetWindowLongPtrA
#define SetWindowLongW	SetWindowLongPtrW
#define GetWindowLongA	GetWindowLongPtrA
#define GetWindowLongW	GetWindowLongPtrW
#define GCL_HICON		GCLP_HICON
#define GCL_HCURSOR		GCLP_HCURSOR
#define GWL_WNDPROC		GWLP_WNDPROC
#define DWL_MSGRESULT	DWLP_MSGRESULT
#else
#define DWORD_PTR		DWORD
#endif

struct WINPOS {
	int		x;
	int		y;
	int		cx;
	int		cy;
};

enum DlgItemFlags {
	NONE_FIT	= 0x000,
	LEFT_FIT	= 0x001,
	HMID_FIT	= 0x002,
	RIGHT_FIT	= 0x004,
	TOP_FIT		= 0x008,
	VMID_FIT	= 0x010,
	BOTTOM_FIT	= 0x020,
	FIT_SKIP	= 0x800,
	X_FIT		= LEFT_FIT|RIGHT_FIT,
	Y_FIT		= TOP_FIT|BOTTOM_FIT,
	XY_FIT		= X_FIT|Y_FIT,
};

struct DlgItem {
	DWORD	flags;	// DlgItemFlags
	HWND	hWnd;
	UINT	id;
	WINPOS	wpos;
};

// UTF8 string class
enum StrMode { BY_UTF8, BY_MBCS };

/* for internal use end */

struct TRect : public RECT {
	TRect(long x=0, long y=0, long cx=0, long cy=0) { left=x, top=y, right=x+cx, bottom=y+cy; }
	TRect(const RECT &rc) { *this = rc; }
	TRect(const POINT &tl, const POINT &br) { left=tl.x, top=tl.y, right=br.x, bottom=br.y; }
	TRect(const POINTS &tl, const POINTS &br) { left=tl.x, top=tl.y, right=br.x, bottom=br.y; }
	TRect(const POINT &tl, long cx, long cy) { left=tl.x, top=tl.y, right=left+cx, bottom=top+cy; }
	TRect(const POINTS &tl, long cx, long cy) { left=tl.x, top=tl.y, right=left+cx, bottom=top+cy;}

	long&	x() { return left; }
	long&	y() { return top; }
	long	cx() { return right - left; }
	long	cy() { return bottom - top; }
	void	set_x(long  x) { left = x; }
	void	set_y(long  y) { top = y; }
	void	set_cx(long v) { right = left + v; }
	void	set_cy(long v) { bottom = top + v; }
	void	Regular() { if (left > right) { long t=left; left=right; right=t; }
	                    if (top > bottom) { long t=top; top=bottom; bottom=t; } }
	void	Infrate(long cx, long cy) { left-=cx; right+=cx; top-=cy; bottom+=cy; }
	void	Size(long cx, long cy) { right = left + cx; bottom = top + cy; }
};

class TWin : public THashObj {
protected:
	TRect			rect;
	TRect			orgRect;
	HACCEL			hAccel;
	TWin			*parent;
	BOOL			sleepBusy;	// for TWin::Sleep() only
	BOOL			isUnicode;
	BOOL			scrollHack;	// need 32bit scroll hack, if EventScroll is overridden

public:
	TWin(TWin *_parent = NULL);
	virtual	~TWin();

	HWND			hWnd;
	DWORD			modalCount;
	DWORD			twinId;

	virtual void	Show(int mode = SW_SHOWDEFAULT);
	virtual BOOL	Create(LPCSTR className=NULL, LPCSTR title="",
						DWORD style=(WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN),
						DWORD exStyle=0, HMENU hMenu=NULL);
	virtual BOOL	CreateU8(LPCSTR className=NULL, LPCSTR title="",
						DWORD style=(WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN),
						DWORD exStyle=0, HMENU hMenu=NULL);
	virtual BOOL	CreateW(const WCHAR *className=NULL, const WCHAR *title=L"",
						DWORD style=(WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN),
						DWORD exStyle=0, HMENU hMenu=NULL);
	virtual	void	Destroy(void);

	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvSysCommand(WPARAM uCmdType, POINTS pos);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvClose(void);
	virtual BOOL	EvDestroy(void);
	virtual BOOL	EvNcDestroy(void);
	virtual BOOL	EvQueryEndSession(BOOL nSession, BOOL nLogOut);
	virtual BOOL	EvEndSession(BOOL nSession, BOOL nLogOut);
	virtual BOOL	EvQueryOpen(void);
	virtual BOOL	EvPaint(void);
	virtual BOOL	EvNcPaint(HRGN hRgn);
	virtual BOOL	EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight);
	virtual BOOL	EvMove(int xpos, int ypos);
	virtual BOOL	EvShowWindow(BOOL fShow, int fnStatus);
	virtual BOOL	EvGetMinMaxInfo(MINMAXINFO *info);
	virtual BOOL	EvTimer(WPARAM timerID, TIMERPROC proc);
	virtual BOOL	EvSetCursor(HWND cursorWnd, WORD nHitTest, WORD wMouseMsg);
	virtual BOOL	EvMouseMove(UINT fwKeys, POINTS pos);
	virtual BOOL	EvNcHitTest(POINTS pos, LRESULT *result);
	virtual BOOL	EvMeasureItem(UINT ctlID, MEASUREITEMSTRUCT *lpMis);
	virtual BOOL	EvDrawItem(UINT ctlID, DRAWITEMSTRUCT *lpDis);
	virtual BOOL	EvMenuSelect(UINT uItem, UINT fuFlag, HMENU hMenu);
	virtual BOOL	EvDropFiles(HDROP hDrop);
	virtual BOOL	EvNotify(UINT ctlID, NMHDR *pNmHdr);
	virtual BOOL	EvContextMenu(HWND childWnd, POINTS pos);
	virtual BOOL	EvHotKey(int hotKey);
	virtual BOOL	EvActivateApp(BOOL fActivate, DWORD dwThreadID);
	virtual BOOL	EvActivate(BOOL fActivate, DWORD fMinimized, HWND hActiveWnd);
	virtual BOOL	EvChar(WCHAR code, LPARAM keyData);
	virtual BOOL	EvWindowPosChanged(WINDOWPOS *pos);
	virtual BOOL	EvWindowPosChanging(WINDOWPOS *pos);

	virtual BOOL	EventButton(UINT uMsg, int nHitTest, POINTS pos);
	virtual BOOL	EventKey(UINT uMsg, int nVirtKey, LONG lKeyData);
	virtual BOOL	EventMenuLoop(UINT uMsg, BOOL fIsTrackPopupMenu);
	virtual BOOL	EventInitMenu(UINT uMsg, HMENU hMenu, UINT uPos, BOOL fSystemMenu);
	virtual BOOL	EventCtlColor(UINT uMsg, HDC hDcCtl, HWND hWndCtl, HBRUSH *result);
	virtual BOOL	EventFocus(UINT uMsg, HWND focusWnd);
	virtual BOOL	EventScrollWrapper(UINT uMsg, int nScrollCode, int nPos, HWND hScroll);
	virtual BOOL	EventScroll(UINT uMsg, int nScrollCode, int nPos, HWND hScroll);

	virtual BOOL	EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual BOOL	EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual BOOL	EventSystem(UINT uMsg, WPARAM wParam, LPARAM lParam);

	virtual UINT	GetDlgItemText(int ctlId, LPSTR buf, int len);
	virtual UINT	GetDlgItemTextW(int ctlId, WCHAR *buf, int len);
	virtual UINT	GetDlgItemTextU8(int ctlId, char *buf, int len);
	virtual BOOL	SetDlgItemText(int ctlId, LPCSTR buf);
	virtual BOOL	SetDlgItemTextW(int ctlId, const WCHAR *buf);
	virtual BOOL	SetDlgItemTextU8(int ctlId, const char *buf);
	virtual int		GetDlgItemInt(int ctlId, BOOL *err=NULL, BOOL sign=TRUE);
	virtual BOOL	SetDlgItemInt(int ctlId, int val, BOOL sign=TRUE);
	virtual HWND	GetDlgItem(int ctlId);
	virtual BOOL	CheckDlgButton(int ctlId, UINT check);
	virtual UINT	IsDlgButtonChecked(int ctlId);
	virtual BOOL	IsWindowVisible(void);
	virtual BOOL	EnableWindow(BOOL is_enable);

	virtual	int		MessageBox(LPCSTR msg, LPCSTR title="msg", UINT style=MB_OK);
	virtual	int		MessageBoxW(LPCWSTR msg, LPCWSTR title=L"", UINT style=MB_OK);
	virtual	int		MessageBoxU8(LPCSTR msg, LPCSTR title="msg", UINT style=MB_OK);
	virtual BOOL	BringWindowToTop(void);
	virtual BOOL	SetForegroundWindow(void);
	virtual BOOL	SetForceForegroundWindow(void);
	virtual BOOL	ShowWindow(int mode);
	virtual BOOL	PostMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual BOOL	PostMessageW(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual LRESULT	SendMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual LRESULT	SendMessageW(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual LRESULT	SendDlgItemMessage(int ctlId, UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual LRESULT	SendDlgItemMessageW(int ctlId, UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual BOOL	GetWindowRect(RECT *_rect=NULL);
	virtual BOOL	GetClientRect(RECT *rc);
	virtual BOOL	SetWindowPos(HWND hInsAfter, int x, int y, int cx, int cy, UINT fuFlags);
	virtual HWND	SetActiveWindow(void);
	virtual int		GetWindowText(LPSTR text, int size);
	virtual BOOL	SetWindowText(LPCSTR text);
	virtual BOOL	GetWindowTextW(WCHAR *text, int size);
	virtual BOOL	GetWindowTextU8(char *text, int size);
	virtual BOOL	SetWindowTextW(const WCHAR *text);
	virtual BOOL	SetWindowTextU8(const char *text);
	virtual int		GetWindowTextLengthW(void);
	virtual int		GetWindowTextLengthU8(void);
	virtual BOOL	InvalidateRect(const RECT *rc, BOOL fErase);

	virtual LONG_PTR SetWindowLong(int index, LONG_PTR val);
	virtual WORD	SetWindowWord(int index, WORD val);
	virtual LONG_PTR GetWindowLong(int index);
	virtual WORD	GetWindowWord(int index);
	virtual TWin	*GetParent(void) { return parent; };
	virtual void	SetParent(TWin *_parent) { parent = _parent; };
	virtual BOOL	MoveWindow(int x, int y, int cx, int cy, int bRepaint);
	virtual BOOL	FitMoveWindow(int x, int y);
	virtual BOOL	Sleep(UINT mSec);
	virtual BOOL	Idle(void);

	virtual	BOOL	PreProcMsg(MSG *msg);
	virtual	LRESULT	WinProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual	LRESULT	DefWindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class TDlg : public TWin {
protected:
	UINT	resId;
	BOOL	modalFlg;
	int		maxItems;
	DlgItem	*dlgItems;

public:
	TDlg(UINT	resid=0, TWin *_parent = NULL);
	virtual ~TDlg();

	virtual BOOL	Create(HINSTANCE hI = NULL);
	virtual	void	Destroy(void);
	virtual int		Exec(void);
	virtual void	EndDialog(int);
	UINT			ResId(void) { return resId; }
	virtual int		SetDlgItem(UINT ctl_id, DWORD flags=0);
	virtual BOOL	FitDlgItems();

	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvSysCommand(WPARAM uCmdType, POINTS pos);
	virtual BOOL	EvQueryOpen(void);

	virtual	BOOL	PreProcMsg(MSG *msg);
	virtual LRESULT	WinProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class TSubClass : public TWin {
protected:
	WNDPROC		oldProc;

public:
	TSubClass(TWin *_parent = NULL);
	virtual ~TSubClass();

	virtual BOOL	AttachWnd(HWND _hWnd);
	virtual BOOL	DetachWnd();
	virtual	LRESULT	DefWindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class TSubClassCtl : public TSubClass {
protected:
public:
	TSubClassCtl(TWin *_parent);

	virtual	BOOL	PreProcMsg(MSG *msg);
	virtual LRESULT WinProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

BOOL TRegisterClass(LPCSTR class_name, UINT style=CS_DBLCLKS, HICON hIcon=0, HCURSOR hCursor=0,
		HBRUSH hbrBackground=0, int classExtra=0, int wndExtra=0, LPCSTR menu_str=0);
BOOL TRegisterClassU8(LPCSTR class_name, UINT style=CS_DBLCLKS, HICON hIcon=0, HCURSOR hCursor=0,
		HBRUSH hbrBackground=0, int classExtra=0, int wndExtra=0, LPCSTR menu_str=0);
BOOL TRegisterClassW(const WCHAR *class_name, UINT style=CS_DBLCLKS, HICON hIcon=0,
		HCURSOR hCursor=0, HBRUSH hbrBackground=0, int classExtra=0, int wndExtra=0,
		const WCHAR *menu_str=0);

class TWinHashTbl : public THashTbl {
protected:
	virtual BOOL	IsSameVal(THashObj *obj, const void *val) {
		return ((TWin *)obj)->hWnd == *(HWND *)val;
	}

public:
	TWinHashTbl(int _hashNum) : THashTbl(_hashNum) {}
	virtual ~TWinHashTbl() {}

	u_int	MakeHashId(HWND hWnd) { return (u_int)hWnd * 0xf3f77d13; }
};

class TApp {
protected:
	static TApp	*tapp;
	TWinHashTbl	*hash;
	LPCSTR		defaultClass;
	LPCWSTR		defaultClassW;

	LPSTR		cmdLine;
	int			nCmdShow;
	TWin		*mainWnd;
	TWin		*preWnd;
	HINSTANCE	hI;
	DWORD		twinId;

	TWin	*SearchWnd(HWND hWnd) { return (TWin *)hash->Search(&hWnd, hash->MakeHashId(hWnd)); }
	virtual BOOL	InitApp(void);

public:
	TApp(HINSTANCE _hI, LPSTR _cmdLine, int _nCmdShow);
	virtual ~TApp();
	virtual void	InitWindow() = 0;
	virtual int		Run();
	virtual BOOL	PreProcMsg(MSG *msg);

	LPCSTR	GetDefaultClass() { return defaultClass; }
	LPCWSTR	GetDefaultClassW() { return defaultClassW; }
	void	AddWin(TWin *win) { preWnd = win; }
	void	AddWinByWnd(TWin *win, HWND hWnd) {
			win->hWnd = hWnd; hash->Register(win, hash->MakeHashId(hWnd));
	}
	void	DelWin(TWin *win) { hash->UnRegister(win); }

	static TApp *GetApp() { return tapp; }
	static void Idle(DWORD timeout=0);
	static HINSTANCE GetInstance() { return tapp->hI; }
	static LRESULT CALLBACK WinProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static DWORD  GenTWinID() { return tapp ? tapp->twinId++ : 0; }
};

struct TListObj {
	TListObj	*prev, *next;
};

template <class T>
class TListEx {
protected:
	TListObj	top;
	int			num;

public:
	TListEx() { Init(); }
	void Init() { top.prev = top.next = &top; num = 0; }
	void AddObj(T *obj) { // add to last
		obj->prev = (T *)top.prev; obj->next = (T *)&top;
		top.prev->next = obj;      top.prev = obj;
		num++;
	}
	void DelObj(T *obj) {
		if (obj->next) obj->next->prev = obj->prev;
		if (obj->prev) obj->prev->next = obj->next;
		obj->next = obj->prev = NULL;
		num--;
	}
	void PushObj(T *obj) { // add to top
		obj->next = top.next;
		obj->prev = &top;
		top.next->prev = obj;
		top.next = obj;
		num++;
	}
	T *TopObj(void)     { return (T*)(top.next  == &top ? NULL : top.next);  }
	T *EndObj(void)     { return (T*)(top.next  == &top ? NULL : top.prev);  } 
	T *NextObj(T *obj)  { return (T*)(obj->next == &top ? NULL : obj->next); } 
	T *PrevObj(T *obj)  { return (T*)(obj->prev == &top ? NULL : obj->prev); } 

	void MoveList(TListEx<T> *from_list) {
		if (from_list->top.next == &from_list->top) return;	// from_list is empty
		if (top.next == &top) {	// empty
			top = from_list->top;
			top.next->prev = top.prev->next = &top;
		}
		else {
			top.prev->next = from_list->top.next;
			from_list->top.next->prev = top.prev;
			from_list->top.prev->next = &top;
			top.prev = from_list->top.prev;
		}
		num += from_list->num;
		from_list->Init();
	}
	int  Num() { return num; }
	BOOL IsEmpty() { return top.next == &top; }
};

#define FREE_LIST	0
#define USED_LIST	1
#define LOCK_LIST	2
#define	RLIST_MAX	3
template <class T>
class TRecycleListEx {
	TListEx<T>	list[RLIST_MAX];
	T *data;

public:
	TRecycleListEx(int init_cnt=0) { data = NULL; if (init_cnt) Init(init_cnt); }
	virtual ~TRecycleListEx()      { delete [] data; }
	BOOL Init(int init_cnt) {
		UnInit();
		data = new T[init_cnt];
		for (int i=0; i < init_cnt; i++) list[FREE_LIST].AddObj(&data[i]);
		return TRUE;
	}
	void UnInit() {
		if (data) delete [] data;
		data = NULL;
		for (int i=0; i < RLIST_MAX; i++) list[i].Init();
	}
	T *GetObj(int list_type) {
		T *d = list[list_type].TopObj();
		if (d) list[list_type].DelObj(d);
		return d;
	}
	void PutObj(int list_type, T *obj)  { list[list_type].AddObj(obj);         }
	void DelObj(int list_type, T *obj)  { list[list_type].DelObj(obj);         }
	T	*TopObj(int list_type)          { return list[list_type].TopObj();     }
	T	*EndObj(int list_type)          { return list[list_type].EndObj();     }
	T	*NextObj(int list_type, T *obj) { return list[list_type].NextObj(obj); }
	T	*PrevObj(int list_type, T *obj) { return list[list_type].PrevObj(obj); }
	BOOL IsEmpty(int list_type)         { return list[list_type].IsEmpty();    }
	TListEx<T> *List(int list_type)     { return &list[list_type];             }
};


#define MAX_KEYARRAY	30

class TRegistry {
protected:
	HKEY	topKey;
	int		openCnt;
	StrMode	strMode;
	HKEY	hKey[MAX_KEYARRAY];

public:
	TRegistry(LPCSTR company, LPSTR appName=NULL, StrMode mode=BY_UTF8);
	TRegistry(LPCWSTR company, LPCWSTR appName=NULL, StrMode mode=BY_UTF8);
	TRegistry(HKEY top_key, StrMode mode=BY_UTF8);
	~TRegistry();

	void	ChangeTopKey(HKEY topKey);
	void	SetStrMode(StrMode mode) { strMode = mode; }

	BOOL	ChangeApp(LPCSTR company, LPSTR appName=NULL);
	BOOL	ChangeAppW(const WCHAR *company, const WCHAR *appName=NULL);

	BOOL	OpenKey(LPCSTR subKey, BOOL createFlg=FALSE);
	BOOL	OpenKeyW(const WCHAR *subKey, BOOL createFlg=FALSE);

	BOOL	CreateKey(LPCSTR subKey) { return OpenKey(subKey, TRUE); }
	BOOL	CreateKeyW(const WCHAR *subKey) { return OpenKeyW(subKey, TRUE); }

	BOOL	CloseKey(void);

	BOOL	GetInt(LPCSTR key, int *val);
	BOOL	GetIntW(const WCHAR *key, int *val);

	BOOL	SetInt(LPCSTR key, int val);
	BOOL	SetIntW(const WCHAR *key, int val);

	BOOL	GetLong(LPCSTR key, long *val);
	BOOL	GetLongW(const WCHAR *key, long *val);

	BOOL	SetLong(LPCSTR key, long val);
	BOOL	SetLongW(const WCHAR *key, long val);

	BOOL	GetStr(LPCSTR key, LPSTR str, int size_byte);
	BOOL	GetStrA(LPCSTR key, LPSTR str, int size_byte);
	BOOL	GetStrW(const WCHAR *key, WCHAR *str, int size_byte);

	BOOL	SetStr(LPCSTR key, LPCSTR str);
	BOOL	SetStrA(LPCSTR key, LPCSTR str);
	BOOL	SetStrW(const WCHAR *key, const WCHAR *str);

	BOOL	GetByte(LPCSTR key, BYTE *data, int *size);
	BOOL	GetByteW(const WCHAR *key, BYTE *data, int *size);

	BOOL	SetByte(LPCSTR key, const BYTE *data, int size);
	BOOL	SetByteW(const WCHAR *key, const BYTE *data, int size);

	BOOL	DeleteKey(LPCSTR str);
	BOOL	DeleteKeyW(const WCHAR *str);

	BOOL	DeleteValue(LPCSTR str);
	BOOL	DeleteValueW(const WCHAR *str);

	BOOL	EnumKey(DWORD cnt, LPSTR buf, int size);
	BOOL	EnumKeyW(DWORD cnt, WCHAR *buf, int size);

	BOOL	EnumValue(DWORD cnt, LPSTR buf, int size, DWORD *type=NULL);
	BOOL	EnumValueW(DWORD cnt, WCHAR *buf, int size, DWORD *type=NULL);

	BOOL	DeleteChildTree(LPCSTR subkey=NULL);
	BOOL	DeleteChildTreeW(const WCHAR *subkey=NULL);
};

class TIniKey : public TListObj {
protected:
	char	*key;	// null means comment line
	char	*val;

public:
	TIniKey(const char *_key=NULL, const char *_val=NULL) {
		key = val = NULL; if (_key || _val) Set(_key, _val);
	}
	~TIniKey() { free(key); free(val); }

	void Set(const char *_key=NULL, const char *_val=NULL) {
		if (_key) { free(key); key=strdup(_key); }
		if (_val) { free(val); val=strdup(_val); }
	}
	const char *Key() { return key; }
	const char *Val() { return val; }
};

class TIniSection : public TListObj, public TListEx<TIniKey> {
protected:
	char	*name;

public:
	TIniSection() { name = NULL; }
	~TIniSection() {
		free(name);
		for (TIniKey *key; (key = TopObj()); ) {
			DelObj(key);
			delete key;
		}
	}

	void Set(const char *_name=NULL) {
		if (_name) { free(name); name=strdup(_name); }
	}
	TIniKey *SearchKey(const char *key_name) {
		for (TIniKey *key = TopObj(); key; key = NextObj(key)) {
			if (key->Key() && strcmpi(key->Key(), key_name) == 0) return key;
		}
		return	NULL;
	}
	BOOL AddKey(const char *key_name, const char *val) {
		TIniKey	*key = key_name ? SearchKey(key_name) : NULL;
		if (!key) {
			key = new TIniKey(key_name, val);
			AddObj(key);
		}
		else {
			key->Set(NULL, val);
		}
		return	TRUE;
	}
	BOOL KeyToTop(const char *key_name) {
		TIniKey	*key = key_name ? SearchKey(key_name) : NULL;
		if (!key) return FALSE;
		DelObj(key);
		PushObj(key);
		return	TRUE;
	}
	BOOL DelKey(const char *key_name) {
		TIniKey	*key = SearchKey(key_name);
		if (!key) return FALSE;
		DelObj(key);
		delete key;
		return	TRUE;
	}
	const char *Name() { return name; }
};

class TInifile: public TListEx<TIniSection> {
protected:
	WCHAR		*iniFile;
	TIniSection	*curSec;
	TIniSection	*rootSec;
//	FILETIME	iniFt;
//	DWORD		iniSize;
	HANDLE		hMutex;

	BOOL Strip(const char *s, char *d=NULL, const char *strip_chars=" \t\r\n",
		const char *quote_chars="\"\"");
	BOOL Parse(const char *buf, BOOL *is_section, char *name, char *val);
	BOOL GetFileInfo(const char *fname, FILETIME *ft, int *size);
	TIniSection *SearchSection(const char *section);
	BOOL WriteIni();
	BOOL Lock();
	void UnLock();
	void InitCore(WCHAR *_ini);

public:
	TInifile(const WCHAR *ini=NULL);
	~TInifile();
	void Init(const WCHAR *_ini);
	void Init(const char *_ini);
	void UnInit();
	void SetSection(const char *section);
	BOOL CurSection(char *section);
	BOOL StartUpdate();
	BOOL EndUpdate();
	BOOL SetStr(const char *key, const char *val);
	DWORD GetStr(const char *key, char *val, int max_size, const char *default_val="",
					BOOL *use_default=NULL);
	const char *GetRawVal(const char *key);
	BOOL SetInt(const char *key, int val);
	BOOL DelSection(const char *section);
	BOOL DelKey(const char *key);
	BOOL KeyToTop(const char *key);
	int GetInt(const char *key, int default_val=-1);
	int64 GetInt64(const char *key, int64 default_val=-1);
	void SetIniFileNameW(const WCHAR *ini) {
		if (iniFile) free(iniFile);
		iniFile = wcsdup(ini);
	}
	const WCHAR *GetIniFileNameW(void) { return	iniFile; }
};


#include "tapi32u8.h"

#endif
