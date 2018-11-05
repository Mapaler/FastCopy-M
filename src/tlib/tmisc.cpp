static char *tmisc_id = 
	"@(#)Copyright (C) 1996-2017 H.Shirouzu		tmisc.cpp	Ver0.99";
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Application Frame Class
	Create					: 1996-06-01(Sat)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "tlib.h"

#include <lm.h>
#include <list>
#include <map>
#include <process.h>  
#include <Shellapi.h>
#include <propkey.h>
#include <propvarutil.h>
#include <time.h>

using namespace std;

OSVERSIONINFOEX TOSVerInfo = []() {
	OSVERSIONINFOEX	ovi = { sizeof(OSVERSIONINFO) };
	::GetVersionEx((OSVERSIONINFO *)&ovi);
	return	ovi;
}();

HINSTANCE defaultStrInstance;

static CRITICAL_SECTION	gCs;
static BOOL gCsInit = []() {
	::InitializeCriticalSection(&gCs);
	return	TRUE;
}();

Condition::Event	*Condition::gEvents  = NULL;
volatile LONG		Condition::gEventMap = 0;

static LCID defaultLCID;

#pragma comment (lib, "Dbghelp.lib")
#pragma comment (lib, "Netapi32.lib")

/*=========================================================================
  クラス ： Condition
  概  要 ： 条件変数クラス
  説  明 ： 
  注  意 ： 
=========================================================================*/
Condition::Condition(void)
{
	static BOOL once = InitGlobalEvents();
	isInit = FALSE;
}

Condition::~Condition(void)
{
	UnInitialize();
}

BOOL Condition::InitGlobalEvents()
{
	gEvents = new Event[MaxThreads];	// プロセス終了まで解放しない
	gEventMap = 0xffffffff;

	// 事前に多少作っておく（おそらく十分すぎる）
	for (int i=0; i < 10; i++) {
		gEvents[i].hEvent = ::CreateEvent(0, FALSE, FALSE, NULL);
	}

	return	TRUE;
}

BOOL Condition::Initialize()
{
	if (!isInit) {
		::InitializeCriticalSection(&cs);
		waitBits = 0;
		isInit = TRUE;
	}
	return	TRUE;
}

void Condition::UnInitialize(void)
{
	if (isInit) {
		::DeleteCriticalSection(&cs);
		isInit = FALSE;
	}
}

BOOL Condition::Wait(DWORD timeout)
{
// 参考程度の空き開始位置調査
// （正確な確認は、INIT_EVENT <-> WAIT_EVENT の CAS で）
	u_int	idx = get_ntz(_InterlockedExchangeAdd(&gEventMap, 0));
	u_int	self_bit = 0;
	if (idx >= MaxThreads) idx = 0;

	int	count = 0;
	while (count < MaxThreads) {
		if (InterlockedCompareExchange(&gEvents[idx].kind, WAIT_EVENT, INIT_EVENT) == INIT_EVENT) {
			self_bit = 1 << idx;
			_InterlockedAnd(&gEventMap, ~self_bit);
			break;
		}
		if (++idx == MaxThreads) idx = 0;
		count++;
	}
	if (count >= MaxThreads) {	// 通常はありえない
		MessageBox(0, "Detect too many wait threads", "TLib", MB_OK);
		return	FALSE;
	}
	Event	&event = gEvents[idx];

	if (event.hEvent == NULL) {
		event.hEvent = ::CreateEvent(0, FALSE, FALSE, NULL);
	}
	waitBits |= self_bit;

	UnLock();

	DWORD	status = ::WaitForSingleObject(event.hEvent, timeout);

	Lock();
	waitBits &= ~self_bit;
	InterlockedExchange(&event.kind, INIT_EVENT);
	_InterlockedOr(&gEventMap, self_bit);

	return	status == WAIT_TIMEOUT ? FALSE : TRUE;
}

void Condition::Notify(void)	// 現状では、眠っているスレッド全員を起こす
{
	if (waitBits) {
		u_int	bits = waitBits;
		while (bits) {
			int		idx = get_ntz(bits);
			Event	&event = gEvents[idx];

			if (event.kind == WAIT_EVENT) {
				::SetEvent(event.hEvent);
				event.kind = DONE_EVENT;	// INIT <-> WAIT間以外では CASは無用
			}
			bits &= ~(1 << idx);
		}
	}
}

// Condtion test
//#include <process.h>
//
//struct Arg {
//	Condition	&cv;
//	int			&val;
//	bool		&done;
//	int			no;
//	Arg(Condition *_cv, int *_val, bool *_done, int _no)
//		: cv(*_cv), val(*_val), done(*_done), no(_no) {}
//};
//
//#define MULTI   5
//#define THREADS 6
//#define VAL 1000000
//
//void cond_func(void *_arg) {
//	Arg	&arg = *(Arg *)_arg;
//
//	arg.cv.Lock();
//	while (arg.val < VAL) {
//		if ((arg.val % THREADS) == arg.no) {
//			arg.val++;
//			arg.cv.Notify();
//		} else {
//			arg.cv.Wait();
//		}
//	}
//	arg.done = true;
//	arg.cv.Notify();
//	arg.cv.UnLock();
//}
//
//void cond_test()
//{
//	DWORD		tick = GetTickCount();
//	Condition	cv[MULTI];
//	int			val[MULTI] = {};
//	bool		done[MULTI][THREADS] = {};
//
//	for (int i=0; i < MULTI; i++) {
//		cv[i].Initialize();
//
//		for (int ii=0; ii < THREADS; ii++) {
//			_beginthread(cond_func, 0, new Arg(&cv[i], &val[i], &done[i][ii], ii));
//		}
//	}
//
//	for (int i=0; i < MULTI; i++) {
//		cv[i].Lock();
//		while (1) {
//			if (val[i] == VAL) {
//				for (int ii=0; ii < THREADS; ii++) {
//					while (1) {
//						if (done[i][ii]) break;
//						cv[i].Wait();
//					}
//				}
//				break;
//			}
//			cv[i].Wait();
//		}
//		cv[i].UnLock();
//	}
//
//	DBG(Fmt("%d\n", GetTickCount() - tick));
//}


/*=========================================================================
  クラス ： VBuf
  概  要 ： 仮想メモリ管理クラス
  説  明 ： 
  注  意 ： 
=========================================================================*/
VBuf::VBuf(size_t _size, size_t _max_size)
{
	dumpExcept = FALSE;
	Init();

	if (_size || _max_size) {
		AllocBuf(_size, _max_size);
	}
}

VBuf::~VBuf()
{
	if (buf) {
		FreeBuf();
	}
}

void VBuf::Init(void)
{
	buf = NULL;
	size = 0;
	usedSize = 0;
	maxSize = 0;
}

BOOL VBuf::AllocBuf(size_t _size, size_t _max_size)
{
	if (buf) FreeBuf();

	if (_max_size == 0) {
		_max_size = _size;
	}
	maxSize = _max_size;

// 1page 分だけ余計に確保（buffer over flow 検出用）
	if (!(buf = (BYTE *)::VirtualAlloc(NULL, maxSize + PAGE_SIZE, MEM_RESERVE, PAGE_READWRITE))) {
		Init();
		return	FALSE;
	}

	if (dumpExcept) {
		RegisterDumpExceptArea(buf, maxSize + PAGE_SIZE);
	}

	return	Grow(_size);
}

BOOL VBuf::LockBuf(void)
{
	return	::VirtualLock(buf, size);
}

void VBuf::FreeBuf(void)
{
	if (buf) {
		::VirtualFree(buf, 0, MEM_RELEASE);
		if (dumpExcept && size) {
			RemoveDumpExceptArea(buf);
		}
	}
	Init();
}

BOOL VBuf::Grow(size_t grow_size)
{
	if (size + grow_size > maxSize)
		return	FALSE;

	if (grow_size && !::VirtualAlloc(buf + size, grow_size, MEM_COMMIT, PAGE_READWRITE))
		return	FALSE;

	size += grow_size;

	return	TRUE;
}


// LoadStr
void InitInstanceForLoadStr(HINSTANCE hI)
{
	defaultStrInstance = hI;
}

LPSTR LoadStrA(UINT resId, HINSTANCE hI)
{
	static TResHash	*hash;
	static LCID lastLCID;

	if (lastLCID != defaultLCID || !hash) {
		if (hash) {
			hash->Reset();
		} else {
			hash = new TResHash(1000);
		}
		lastLCID = defaultLCID;
	}

	TResHashObj	*obj;

	if ((obj = hash->Search(resId)) == NULL) {
		char	buf[1024];
		if (::LoadStringA(hI ? hI : defaultStrInstance, resId, buf, sizeof(buf)) >= 0) {
			obj = new TResHashObj(resId, strdup(buf));
			hash->Register(obj);
		}
	}
	return	obj ? (char *)obj->val : NULL;
}

LPWSTR LoadStrW(UINT resId, HINSTANCE hI)
{
	static TResHash	*hash;
	static LCID lastLCID;

	if (lastLCID != defaultLCID || !hash) {
		if (hash) {
			hash->Reset();
		} else {
			hash = new TResHash(1000);
		}
		lastLCID = defaultLCID;
	}

	TResHashObj	*obj;

	if ((obj = hash->Search(resId)) == NULL) {
		WCHAR	buf[1024];
		if (::LoadStringW(hI ? hI : defaultStrInstance, resId, buf,
				sizeof(buf) / sizeof(WCHAR)) >= 0) {
			obj = new TResHashObj(resId, wcsdup(buf));
			hash->Register(obj);
		}
	}
	return	obj ? (LPWSTR)obj->val : NULL;
}

LPSTR LoadStrU8(UINT resId, HINSTANCE hI)
{
	static TResHash	*hash;
	static LCID lastLCID;

	if (lastLCID != defaultLCID || !hash) {
		if (hash) {
			hash->Reset();
		} else {
			hash = new TResHash(1000);
		}
		lastLCID = defaultLCID;
	}

	TResHashObj	*obj;

	if ((obj = hash->Search(resId)) == NULL) {
		WCHAR	buf[1024];
		if (::LoadStringW(hI ? hI : defaultStrInstance, resId, buf, sizeof(buf) / sizeof(WCHAR))
				>= 0) {
			U8str	buf_u8(buf);
			obj = new TResHashObj(resId, strdup(buf_u8.s()));
			hash->Register(obj);
		}
	}
	return	obj ? (char *)obj->val : NULL;
}

void TSetDefaultLCID(LCID lcid)
{
	defaultLCID = lcid ? lcid : ::GetUserDefaultLCID();

	TSetThreadLocale(defaultLCID);
}

LCID TGetDefaultLCID()
{
	return	defaultLCID;
}

HMODULE TLoadLibrary(LPSTR dllname)
{
	HMODULE	hModule = ::LoadLibrary(dllname);

	if (!hModule) {
		Debug("TLoadLibrary err=%d\n", GetLastError());
	}

	if (defaultLCID) {
		TSetThreadLocale(defaultLCID);
	}

	return	hModule;
}

HMODULE TLoadLibraryW(WCHAR *dllname)
{
	HMODULE	hModule = LoadLibraryW(dllname);

	if (!hModule) {
		Debug("TLoadLibraryW err=%d\n", GetLastError());
	}

	if (defaultLCID) {
		TSetThreadLocale(defaultLCID);
	}

	return	hModule;
}

/* Win64検出 */
BOOL TIsWow64()
{
	static BOOL	ret = []() {
		BOOL r = FALSE;
		BOOL (WINAPI *pIsWow64Process)(HANDLE, BOOL *) = (BOOL (WINAPI *)(HANDLE, BOOL *))
				GetProcAddress(::GetModuleHandle("kernel32"), "IsWow64Process");
		if (pIsWow64Process) {
			pIsWow64Process(::GetCurrentProcess(), &r);
		}
		return	r;
	}();

    return ret;
}

BOOL TOs64()
{
#ifdef _WIN64
	return	TRUE;
#else
	return	TIsWow64();
#endif
}

BOOL TRegDisableReflectionKey(HKEY hBase)
{
	static LONG (WINAPI *pRegDisableReflectionKey)(HKEY) = []() {
		return	(LONG (WINAPI *)(HKEY))
			GetProcAddress(::GetModuleHandle("advapi32"), "RegDisableReflectionKey");
	}();

	return	pRegDisableReflectionKey ? pRegDisableReflectionKey(hBase) == ERROR_SUCCESS : FALSE;
}

BOOL TRegEnableReflectionKey(HKEY hBase)
{
	static LONG (WINAPI *pRegEnableReflectionKey)(HKEY) = []() {
		return	(LONG (WINAPI *)(HKEY))
			GetProcAddress(::GetModuleHandle("advapi32"), "RegEnableReflectionKey");
	}();

	return	pRegEnableReflectionKey ? pRegEnableReflectionKey(hBase) == ERROR_SUCCESS : FALSE;
}

BOOL TWow64DisableWow64FsRedirection(void *oldval)
{
	static BOOL (WINAPI *pWow64DisableWow64FsRedirection)(void *) = []() {
		return	(BOOL (WINAPI *)(void *))
			GetProcAddress(::GetModuleHandle("kernel32"), "Wow64DisableWow64FsRedirection");
	}();

	return	pWow64DisableWow64FsRedirection ? pWow64DisableWow64FsRedirection(oldval) : FALSE;
}

BOOL TWow64RevertWow64FsRedirection(void *oldval)
{
	static BOOL (WINAPI *pWow64RevertWow64FsRedirection)(void *) = []() {
		return	(BOOL (WINAPI *)(void *))
			GetProcAddress(::GetModuleHandle("kernel32"), "Wow64RevertWow64FsRedirection");
	}();

	return	pWow64RevertWow64FsRedirection ? pWow64RevertWow64FsRedirection(oldval) : FALSE;
}

BOOL TIsEnableUAC()
{
	static BOOL ret = []() {
		if (!IsWinVista()) {
			return FALSE;
		}
		TRegistry reg(HKEY_LOCAL_MACHINE);
		if (reg.OpenKey("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System")) {
			int	val = 1;
			if (reg.GetInt("EnableLUA", &val) && val == 0) {
				return FALSE;
			}
		}
		return	TRUE;
	}();

	return	ret;
}

BOOL TIsVirtualizedDirW(WCHAR *path)
{
	if (!IsWinVista()) return FALSE;

	WCHAR	buf[MAX_PATH];
	DWORD	csidl[] = { CSIDL_WINDOWS, CSIDL_PROGRAM_FILES, CSIDL_PROGRAM_FILESX86,
						CSIDL_COMMON_APPDATA, 0xffffffff };

	for (int i=0; csidl[i] != 0xffffffff; i++) {
		if (SHGetSpecialFolderPathW(NULL, buf, csidl[i], FALSE)) {
			while (1) {
				int	len = (int)wcslen(buf);
				if (wcsnicmp(buf, path, len) == 0) {
					WCHAR	ch = path[len];
					if (ch == 0 || ch == '\\' || ch == '/') {
						return	TRUE;
					}
				}
#ifndef _WIN64
#define X86_SFX		L" (x86)"
#define X86_SFX_LEN	6
				if (csidl[i] == CSIDL_PROGRAM_FILES &&
					wcscmp(buf + len - X86_SFX_LEN, X86_SFX) == 0) {
					buf[len - X86_SFX_LEN] = 0;
					continue;
				}
#endif
				break;
			}
		}
	}

	return	FALSE;
}

BOOL TMakeVirtualStorePathW(WCHAR *org_path, WCHAR *buf)
{
	if (!IsWinVista()) return FALSE;

	if (!TIsVirtualizedDirW(org_path)
	|| !SHGetSpecialFolderPathW(NULL, buf, CSIDL_LOCAL_APPDATA, FALSE)
	||	org_path[1] != ':' || org_path[2] != '\\') {
		wcscpy(buf, org_path);
		return	FALSE;
	}

	swprintf(buf + wcslen(buf), L"\\VirtualStore%s", org_path + 2);
	return	TRUE;
}

BOOL TSetPrivilege(LPSTR privName, BOOL bEnable)
{
	HANDLE				hToken;
	TOKEN_PRIVILEGES	tp = {1};

	if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY, &hToken))
		return FALSE;

	BOOL ret = ::LookupPrivilegeValue(NULL, privName, &tp.Privileges[0].Luid);

	if (ret) {
		if (bEnable) tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		else		 tp.Privileges[0].Attributes = 0;

		ret = ::AdjustTokenPrivileges(hToken, FALSE, &tp, 0, 0, 0);
	}
	::CloseHandle(hToken);

	return	ret;
}

BOOL TSetThreadLocale(int lcid)
{
	static LANGID (WINAPI *pSetThreadUILanguage)(LANGID LangId) = []() {
		return	(LANGID (WINAPI *)(LANGID LangId))
			GetProcAddress(::GetModuleHandle("kernel32"), "SetThreadUILanguage");
	}();

	if (pSetThreadUILanguage) {
		pSetThreadUILanguage(LANGIDFROMLCID(lcid));
	}
	return ::SetThreadLocale(lcid);
}

BOOL TChangeWindowMessageFilter(UINT msg, DWORD flg)
{
	static BOOL	(WINAPI *pChangeWindowMessageFilter)(UINT, DWORD) = []() {
		return	(BOOL (WINAPI *)(UINT, DWORD))
			GetProcAddress(::GetModuleHandle("user32"), "ChangeWindowMessageFilter");
	}();

	return	pChangeWindowMessageFilter ? pChangeWindowMessageFilter(msg, flg) : FALSE;
}

/*
	ファイルダイアログ用汎用ルーチン
*/
BOOL OpenFileDlg::Exec(UINT editCtl, char *title, char *filter, char *defaultDir, char *defaultExt)
{
	char buf[MAX_PATH_U8];

	if (parent == NULL)
		return FALSE;

	parent->GetDlgItemTextU8(editCtl, buf, sizeof(buf));

	if (!Exec(buf, sizeof(buf), title, filter, defaultDir, defaultExt))
		return	FALSE;

	parent->SetDlgItemTextU8(editCtl, buf);
	return	TRUE;
}

BOOL OpenFileDlg::Exec(char *target, int targ_size, char *title, char *filter, char *defaultDir,
						char *defaultExt)
{
	if (targ_size <= 1) return FALSE;

	OPENFILENAME	ofn;
	U8str			fileName(targ_size);
	U8str			dirName(targ_size);
	char			*fname = NULL;

	// target が fullpath の場合は default dir は使わない
	if (*target && (*target == '\\' || *(target + 1) == ':')) {
	 	GetFullPathNameU8(target, targ_size, dirName.Buf(), &fname);
		if (fname) {
			*(fname -1) = 0;
			strncpyz(fileName.Buf(), fname, targ_size);
		}
	}
	else if (defaultDir) {
		strncpyz(dirName.Buf(), defaultDir, targ_size);
		if (target) {
			strncpyz(fileName.Buf(), target, targ_size);
		}
	}

	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = parent ? parent->hWnd : NULL;
	ofn.lpstrFilter = filter;
	ofn.nFilterIndex = filter ? 1 : 0;
	ofn.lpstrFile = fileName.Buf();
	ofn.lpstrDefExt	 = defaultExt;
	ofn.nMaxFile = targ_size;
	ofn.lpstrTitle = title;
	ofn.lpstrInitialDir = dirName.Buf();
	ofn.lpfnHook = hook;
	ofn.Flags = OFN_HIDEREADONLY|OFN_EXPLORER|(hook ? OFN_ENABLEHOOK : 0);
	if (mode == OPEN || mode == MULTI_OPEN)
		ofn.Flags |= OFN_FILEMUSTEXIST | (mode == MULTI_OPEN ? OFN_ALLOWMULTISELECT : 0);
	else
		ofn.Flags |= (mode == NODEREF_SAVE ? OFN_NODEREFERENCELINKS : 0);
	ofn.Flags |= flags;

	U8str	dirNameBak(targ_size);
	GetCurrentDirectoryU8(targ_size, dirNameBak.Buf());

	BOOL	ret = (mode == OPEN || mode == MULTI_OPEN) ?
					GetOpenFileNameU8(&ofn) : GetSaveFileNameU8(&ofn);

	SetCurrentDirectoryU8(dirNameBak.Buf());
	if (ret) {
		if (mode == MULTI_OPEN) {
			memcpy(target, fileName.Buf(), targ_size);
		} else {
			strncpyz(target, ofn.lpstrFile, targ_size);
		}

		if (defaultDir) strncpyz(defaultDir, ofn.lpstrFile, ofn.nFileOffset);
	}

	return	ret;
}

void TSwitchToThisWindow(HWND hWnd, BOOL flg)
{
	static void	(WINAPI *pSwitchToThisWindow)(HWND, BOOL) = []() {
		return	(void (WINAPI *)(HWND, BOOL))
			GetProcAddress(::GetModuleHandle("user32"), "SwitchToThisWindow");
	}();

	if (pSwitchToThisWindow) {
		pSwitchToThisWindow(hWnd, flg);
	}
}
/*
float GetMonitorScaleFactor()
{
	MONITORINFOEX mie = { sizeof(MONITORINFOEX) };

	GetMonitorInfo(hMonitor, &LogicalMonitorInfo);
	LogicalMonitorWidth = LogicalMonitorInfo.rcMonitor.right – LogicalMonitorInfo.rcMonitor.left;
	LogicalDesktopWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);

}*/


/*
	リンク
	あらかじめ、CoInitialize(NULL); を実行しておくこと
	target ... target_path
	link   ... new_link_path
*/
BOOL ShellLinkW(const WCHAR *target, const WCHAR *link, const WCHAR *arg, const WCHAR *desc,
	const WCHAR *app_id)
{
	IShellLinkW		*shellLink;
	IPersistFile	*persistFile;
	BOOL			ret = FALSE;

	if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
			(void **)&shellLink))) {
		shellLink->SetPath(target);
		if (arg) {
			shellLink->SetArguments(arg);
		}
		if (desc) {
			shellLink->SetDescription(desc);
		}
		WCHAR	buf[MAX_PATH];
		GetParentDirW(target, buf);
		shellLink->SetWorkingDirectory(buf);

		if (app_id && *app_id) {
			IPropertyStore *pps;
			HRESULT hr = shellLink->QueryInterface(IID_PPV_ARGS(&pps));

			if (SUCCEEDED(hr)) {
				PROPVARIANT pv;
				hr = InitPropVariantFromString(app_id, &pv);
				if (SUCCEEDED(hr)) {
					pps->SetValue(PKEY_AppUserModel_ID, pv);
					pps->Commit();
					PropVariantClear(&pv);
				}
			}
			pps->Release();
		}

		if (SUCCEEDED(shellLink->QueryInterface(IID_IPersistFile, (void **)&persistFile))) {
			if (SUCCEEDED(persistFile->Save(link, TRUE))) {
				ret = TRUE;
				GetParentDirW(link, buf);
				::SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATHW|SHCNF_FLUSH, buf, NULL);
			}
			persistFile->Release();
		}
		shellLink->Release();
	}
	return	ret;
}

BOOL ShellLinkU8(const char *target, const char *link, const char *arg, const char *desc,
	const char *app_id)
{
	Wstr	wtarg(target);
	Wstr	wlink(link);
	Wstr	warg(arg);
	Wstr	wdesc(desc);
	Wstr	wapp(app_id);

	return	ShellLinkW(wtarg.s(), wlink.s(), warg.s(), wdesc.s(), wapp.s());
}

BOOL ReadLinkW(const WCHAR *link, WCHAR *target, WCHAR *arg, WCHAR *desc)
{
	IShellLinkW		*shellLink;		// 実際は IShellLinkA or IShellLinkW
	IPersistFile	*persistFile;
	BOOL			ret = FALSE;

	if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
			(void **)&shellLink))) {
		if (SUCCEEDED(shellLink->QueryInterface(IID_IPersistFile, (void **)&persistFile))) {
			if (SUCCEEDED(persistFile->Load(link, STGM_READ))) {
				if (SUCCEEDED(shellLink->GetPath(target, MAX_PATH, NULL, 0))) {
					if (arg) {
						shellLink->GetArguments(arg, MAX_PATH);
					}
					if (desc) {
						shellLink->GetDescription(desc, INFOTIPSIZE);
					}
					ret = TRUE;
				}
			}
			persistFile->Release();
		}
		shellLink->Release();
	}
	return	ret;
}

BOOL ReadLinkU8(const char *link, char *targ, char *arg, char *desc)
{
	Wstr	wlink(link);
	Wstr	wtarg(MAX_PATH);
	Wstr	warg(INFOTIPSIZE);
	Wstr	wdesc(INFOTIPSIZE);

	if (!ReadLinkW(wlink.s(), wtarg.Buf(), arg ? warg.Buf() : NULL, desc ? wdesc.Buf() : NULL)) {
		return	FALSE;
	}
	WtoU8(wtarg.s(), targ, MAX_PATH_U8);
	if (arg) {
		WtoU8(warg.s(), arg, INFOTIPSIZE);
	}
	if (desc) {
		WtoU8(wdesc.s(), desc, INFOTIPSIZE);
	}

	return	TRUE;
}

HRESULT UpdateLinkW(const WCHAR *link, const WCHAR *arg, const WCHAR *desc, DWORD flags, HWND hWnd)
{
	IPersistFile	*persistFile;
	IShellLinkW		*shellLink;

	HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
		(void **)&shellLink);
	if (hr == S_OK) {
		// 事前に IPersistFile::Loadしないと、Distribute Link Tracking されない
		hr = shellLink->QueryInterface(IID_IPersistFile, (void **)&persistFile);
		if (hr == S_OK) {
			hr = persistFile->Load(link, STGM_READ);
			if (hr == S_OK) {
				hr = shellLink->Resolve(hWnd, flags);
				if (arg) {
					shellLink->SetArguments(arg);
				}
				if (desc) {
					shellLink->SetDescription(desc);
				}
				if (arg || desc) {
					persistFile->Save(link, TRUE);
				}
			}
			persistFile->Release();
		}
		shellLink->Release();
	}
	return	hr;
}

HRESULT UpdateLinkU8(const char *link, const char *arg, const char *desc, DWORD flags, HWND hWnd)
{
	Wstr	wlink(link);
	Wstr	warg(arg);
	Wstr	wdesc(desc);

	return	UpdateLinkW(wlink.s(), warg.s(), wdesc.s(), flags, hWnd);
}


/*
	リンクファイル削除
*/
BOOL DeleteLinkW(const WCHAR *link)
{
	WCHAR	dir[MAX_PATH];

	if (!DeleteFileW(link))
		return	FALSE;

	GetParentDirW(link, dir);
	::SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATHW|SHCNF_FLUSH, dir, NULL);

	return	TRUE;
}

BOOL DeleteLinkU8(const char *link)
{
	Wstr	wlink(link);

	return	DeleteLinkW(wlink.s());
}

/*
	親ディレクトリ取得（必ずフルパスであること。UNC対応）
*/
BOOL GetParentDirW(const WCHAR *srcfile, WCHAR *dir)
{
	WCHAR	path[MAX_PATH];
	WCHAR	*fname=NULL;

	if (GetFullPathNameW(srcfile, MAX_PATH, path, &fname) == 0 || fname == NULL)
		return	wcsncpyz(dir, srcfile, MAX_PATH), FALSE;

	if ((fname - path) > 3 || path[1] != ':')
		fname[-1] = 0;
	else
		fname[0] = 0;		// C:\ の場合

	wcsncpyz(dir, path, MAX_PATH);
	return	TRUE;
}

/*
	2byte文字系でもきちんと動作させるためのルーチン
	 (*strrchr(path, "\\")=0 だと '表'などで問題を起すため)
*/
BOOL GetParentDirU8(const char *org_path, char *target_dir)
{
	char	path[MAX_PATH_U8];
	char	*fname=NULL;

	if (GetFullPathNameU8(org_path, sizeof(path), path, &fname) == 0 || fname == NULL)
		return	strncpyz(target_dir, org_path, MAX_PATH_U8), FALSE;

	if (fname - path > 3 || path[1] != ':')
		*(fname - 1) = 0;
	else
		*fname = 0;		// C:\ の場合

	strncpyz(target_dir, path, MAX_PATH_U8);
	return	TRUE;
}


BOOL MakeDirU8(char *dir)
{
	if (GetFileAttributesU8(dir) != 0xffffffff) {
		return	TRUE;
	}
	return	CreateDirectoryU8(dir, 0);
}

BOOL MakeDirW(WCHAR *dir)
{
	if (::GetFileAttributesW(dir) != 0xffffffff) {
		return	TRUE;
	}
	return	::CreateDirectoryW(dir, 0);
}

BOOL MakeDirAllU8(char *dir)
{
	if (MakeDirU8(dir)) {
		return	TRUE;
	}

	char	parent[MAX_PATH_U8];
	if (!GetParentDirU8(dir, parent) || stricmp(dir, parent) == 0 || *parent == 0) {
		return	FALSE;
	}
	if (!MakeDirAllU8(parent)) {
		return	FALSE;
	}
	return	MakeDirU8(dir);
}

BOOL MakeDirAllW(WCHAR *dir)
{
	if (MakeDirW(dir)) {
		return	TRUE;
	}

	WCHAR	parent[MAX_PATH];
	if (!GetParentDirW(dir, parent) || wcsicmp(dir, parent) == 0 || *parent == 0) {
		return	FALSE;
	}
	if (!MakeDirAllW(parent)) {
		return	FALSE;
	}
	return	MakeDirW(dir);
}

HANDLE CreateFileWithDirU8(const char *path, DWORD flg, DWORD share, SECURITY_ATTRIBUTES *sa,
	DWORD create_flg, DWORD attr, HANDLE hTmpl)
{
	HANDLE	fh = CreateFileU8(path, flg, share, sa, create_flg, attr, hTmpl);

	if (fh != INVALID_HANDLE_VALUE) {
		return	fh;
	}

	DWORD	err = GetLastError();
	char	parent[MAX_PATH_U8];

	if (GetParentDirU8(path, parent) && MakeDirAllU8(parent)) {
		fh = CreateFileU8(path, flg, share, sa, create_flg, attr, hTmpl);
	}

	if (fh == INVALID_HANDLE_VALUE) {
		SetLastError(err);
	}
	return	fh;
}

HANDLE CreateFileWithDirW(const WCHAR *path, DWORD flg, DWORD share, SECURITY_ATTRIBUTES *sa,
	DWORD create_flg, DWORD attr, HANDLE hTmpl)
{
	HANDLE	fh = ::CreateFileW(path, flg, share, sa, create_flg, attr, hTmpl);

	if (fh != INVALID_HANDLE_VALUE) {
		return	fh;
	}

	DWORD	err = GetLastError();
	WCHAR	parent[MAX_PATH];

	if (GetParentDirW(path, parent) && MakeDirAllW(parent)) {
		fh = ::CreateFileW(path, flg, share, sa, create_flg, attr, hTmpl);
	}

	if (fh == INVALID_HANDLE_VALUE) {
		SetLastError(err);
	}
	return	fh;
}


// HtmlHelp WorkShop をインストールして、htmlhelp.h を include path に
// 入れること。
#define ENABLE_HTML_HELP
#if defined(ENABLE_HTML_HELP)
#include <htmlhelp.h>

static HWND (WINAPI *pHtmlHelpW)(HWND, WCHAR *, UINT, DWORD_PTR) = NULL;
static DWORD	htmlCookie;
static HMODULE	hHtmlHelp;

BOOL InitHtmlHelp()
{
	if (!hHtmlHelp) {
		hHtmlHelp = TLoadLibraryExW(L"hhctrl.ocx", TLT_SYSDIR);
	}
	if (hHtmlHelp && !pHtmlHelpW) {
		pHtmlHelpW = (HWND (WINAPI *)(HWND, WCHAR *, UINT, DWORD_PTR))
			::GetProcAddress(hHtmlHelp, "HtmlHelpW");
	}
	if (pHtmlHelpW) {
		htmlCookie = 0;
		pHtmlHelpW(NULL, NULL, HH_INITIALIZE, (DWORD_PTR)&htmlCookie);
		return	TRUE;
	}
	return	FALSE;
}

void UnInitHtmlHelp()
{
	if (hHtmlHelp) {
		if (pHtmlHelpW) {
			pHtmlHelpW(NULL, NULL, HH_UNINITIALIZE, htmlCookie);
			pHtmlHelpW = NULL;
		}
		::FreeLibrary(hHtmlHelp);
		hHtmlHelp = NULL;
	}
}

#endif

HWND TransMsgHelp(MSG *msg)
{
#if defined(ENABLE_HTML_HELP)
	if (pHtmlHelpW) {
		return	pHtmlHelpW(0, 0, HH_PRETRANSLATEMESSAGE, (DWORD_PTR)msg);
	}
#endif
	return	NULL;
}

HWND CloseHelpAll()
{
#if defined(ENABLE_HTML_HELP)
	if (!pHtmlHelpW) {
		return NULL;
	}
	return	pHtmlHelpW(0, 0, HH_CLOSE_ALL, 0);
#else
	return NULL;
#endif
}

HWND ShowHelpW(HWND hOwner, const WCHAR *help_dir, const WCHAR *help_file, const WCHAR *section)
{
#if defined(ENABLE_HTML_HELP)
	if (!pHtmlHelpW) {
		InitHtmlHelp();
	}

	if (pHtmlHelpW) {
		WCHAR	path[MAX_PATH];

		MakePathW(path, help_dir, help_file);
		if (section) {
			wcscpy(path + wcslen(path), section);
		}
		return	pHtmlHelpW(hOwner, path, HH_HELP_FINDER, 0);
	}
#endif
	return	NULL;
}

HWND ShowHelpU8(HWND hOwner, const char *help_dir, const char *help_file, const char *section)
{
	Wstr	dir(help_dir);
	Wstr	file(help_file);
	Wstr	sec(section);

	return	ShowHelpW(hOwner, dir.Buf(), file.Buf(), sec.Buf());
}

void UnInitShowHelp()
{
#if defined(ENABLE_HTML_HELP)
	CloseHelpAll();
	UnInitHtmlHelp();
#endif
}

//#define MAGIC_NTZ 0x03F566ED27179461ULL
//static int *ntz64_init() {
//	static int ntz_tbl[64];
//	uint64 val = MAGIC_NTZ;
//	for (int i=0; i < 64; i++) {
//		ntz_tbl[val >> 58] = i;
//		val <<= 1;
//	}
//	return	ntz_tbl;
//}
//
//int get_ntz64(uint64 val) {
//	static int *ntz_tbl = ntz64_init();
//	return	ntz_tbl[((val & -(int64)val) * MAGIC_NTZ) >> 58];
//}

#ifdef REPLACE_DEBUG_ALLOCATOR

#define VALLOC_SIG 0x12345678
#define ALLOC_ALIGN 4
//#define NON_FREE
#undef malloc
#undef realloc
#undef free

extern "C" {
void *malloc(size_t);
void *realloc(void *, size_t);
void free(void *);
}

inline size_t align_size(size_t size, size_t grain) {
	return (size + grain -1) / grain * grain;
}

inline size_t alloc_size(size_t size) {
	return	align_size((align_size(size, ALLOC_ALIGN) + 16 + PAGE_SIZE), PAGE_SIZE);
}
inline void *valloc_base(void *d)
{
	DWORD	org  = (DWORD)d;
	DWORD	base = org & 0xfffff000;

	if (org - base < 16) base -= PAGE_SIZE;

	return	(void *)base;
}
inline size_t valloc_size(void *d)
{
	d = valloc_base(d);

	if (((DWORD *)d)[0] != VALLOC_SIG) {
		return	SIZE_MAX;
	}
	return	((size_t *)d)[1];
}


void *valloc(size_t size)
{
	size_t	s = alloc_size(size);
	void	*d = VirtualAlloc(0, s, MEM_RESERVE, PAGE_NOACCESS);

	if (!d || !VirtualAlloc(d, s - PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE)) {
		DBG("valloc error(%x %d %d)\n", d, s, size);
		return NULL;
	}

	((DWORD *)d)[0]  = VALLOC_SIG;
	((size_t *)d)[1] = size;

	DBG("valloc (%x %d %d)\n", d, s, size);

	return (void *)((u_char *)d + s - PAGE_SIZE - align_size(size, ALLOC_ALIGN));
}

void *vcalloc(size_t num, size_t ele)
{
	size_t	size = num * ele;
	void	*d = valloc(size);

	if (d) {
		memset(d, 0, size);
	}
	return	d;
}

void *vrealloc(void *d, size_t size)
{
	size_t	old_size = 0;

	if (d) {
		if ((old_size = valloc_size(d)) == SIZE_MAX) {
			DBG("non vrealloc (%x %d %d)\n", d, old_size, size);
			return realloc(d, size);
		}
		if (size == 0) {
			vfree(d);
			return NULL;
		}
	}

	void	*new_d = valloc(size);

	if (new_d && d) {
		memcpy(new_d, d, min(size, old_size));
	}
	return new_d;
}

void vfree(void *d)
{
	if (!d) return;

	size_t	size = valloc_size(d);

	if (size == SIZE_MAX) {
		DBG("vfree non vfree (%x)\n", d);
		free(d);
		return;
	}
	DBG(" vfree %x %d %d\n", valloc_base(d), alloc_size(size), size);

#ifdef NON_FREE
	VirtualFree(valloc_base(d), alloc_size(size), MEM_DECOMMIT);
#else
	VirtualFree(valloc_base(d), 0, MEM_RELEASE);
#endif
}

char *vstrdup(const char *s)
{
	size_t	size = strlen(s) + 1;
	void	*d = valloc(size);
	if (d) {
		memcpy(d, s, size);
	}
	return	(char *)d;
}

WCHAR *vwcsdup(const WCHAR *s)
{
	size_t	size = (wcslen(s) + 1) * sizeof(WCHAR);
	void	*d = valloc(size);
	if (d) {
		memcpy(d, s, size);
	}
	return	(WCHAR *)d;
}

void *operator new(size_t size)
{
	return	valloc(size);
}

void operator delete(void *d)
{
	vfree(d);
}

#if _MSC_VER >= 1200
void *operator new [](size_t size)
{
	return	valloc(size);
}

void operator delete [](void *d)
{
	vfree(d);
}
#endif

#endif


/*
	Explorer非公開COM I/F
*/
struct NOTIFYITEM {
	WCHAR	*exe;
	WCHAR	*tip;
	HICON	hIcon;
	HWND	hWnd;
	DWORD	pref;
	UINT	id;
	GUID	guid;
	void*	dummy[8];	// for Win10(x86)...why?
};

class __declspec(uuid("D782CCBA-AFB0-43F1-94DB-FDA3779EACCB")) INotificationCB : public IUnknown {
public:
	virtual HRESULT __stdcall Notify(u_long, NOTIFYITEM *) = 0;
};


class __declspec(uuid("FB852B2C-6BAD-4605-9551-F15F87830935")) ITrayNotify : public IUnknown {
public:
	virtual HRESULT __stdcall RegisterCallback(INotificationCB *) = 0;
	virtual HRESULT __stdcall SetPreference(const NOTIFYITEM *) = 0;
	virtual HRESULT __stdcall EnableAutoTray(BOOL) = 0;
};
class __declspec(uuid("D133CE13-3537-48BA-93A7-AFCD5D2053B4")) ITrayNotify8 : public IUnknown {
public:
	virtual HRESULT __stdcall RegisterCallback(INotificationCB *, u_long *) = 0;
	virtual HRESULT __stdcall UnregisterCallback(u_long) = 0;
	virtual HRESULT __stdcall SetPreference(const NOTIFYITEM *) = 0;
	virtual HRESULT __stdcall EnableAutoTray(BOOL) = 0;
	virtual HRESULT __stdcall DoAction(BOOL) = 0;
};
const CLSID TrayNotifyId = {
	0x25DEAD04, 0x1EAC, 0x4911, {0x9E, 0x3A, 0xAD, 0x0A, 0x4A, 0xB5, 0x60, 0xFD}
};

class INotifyCb : public INotificationCB {
	DWORD	ref = 0;

public:
	INotifyCb() {
	}
	DWORD	pref = 0;

	HRESULT __stdcall QueryInterface(REFIID riid, void **ppv) {
		if (riid == __uuidof(INotificationCB)) {
			*ppv = (INotificationCB *)this;
			AddRef();
			return	S_OK;
		}
		else if (riid == __uuidof(IUnknown)) {
			*ppv = (IUnknown *)this;
			AddRef();
			return	S_OK;
		}
		return	E_NOINTERFACE;
	}
	ULONG __stdcall AddRef() {
		DBG("AddRef(%d)\n", ref+1);
		return ++ref;
	}
	ULONG __stdcall Release() {
		DBG("Release(%d)\n", ref-1);
		if (--ref == 0) {
			DBG("delete INotifyCb\n");
			delete this;
		}
		return	ref;
	}
	HRESULT __stdcall Notify(ULONG ev, NOTIFYITEM *ni) {
		DBGW(L"ev=%d pref=%d id=%d path=%s tip=%s\n", ev, ni->pref, ni->id, ni->exe, ni->tip);
		DWORD	procId = 0;
		::GetWindowThreadProcessId(ni->hWnd, &procId);
		if (procId == ::GetCurrentProcessId()) {
			pref = ni->pref;
			DBG("pref=%d\n", pref);
		}
		return S_OK;
	}
};

BOOL ForceSetTrayIcon(HWND hWnd, UINT id, DWORD pref)
{
	BOOL		ret = FALSE;
	NOTIFYITEM	ni = { 0, 0, 0, hWnd, pref, id, 0 };

	if (IsWin8()) {
		ITrayNotify8 *tn = NULL;

		CoCreateInstance(TrayNotifyId, NULL, CLSCTX_LOCAL_SERVER, __uuidof(ITrayNotify8),
			(void **)&tn);
		if (tn) {
			if (SUCCEEDED(tn->SetPreference(&ni))) {
				ret = TRUE;
			}
			tn->Release();
		}
	} else {
		ITrayNotify *tn = NULL;

		CoCreateInstance(TrayNotifyId, NULL, CLSCTX_LOCAL_SERVER, __uuidof(ITrayNotify),
			(void **)&tn);
		if (tn) {
			if (SUCCEEDED(tn->SetPreference(&ni))) {
				ret = TRUE;
			}
			tn->Release();
		}
	}

	return	ret;
}

TrayMode GetTrayIconState()
{
	TRegistry	reg(HKEY_CURRENT_USER);

	if (reg.OpenKey("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer")) {
		int		val = 0;
		if (reg.GetInt("EnableAutoTray", &val) && val == 0) {
			return	TIS_ALL;	// always show
		}
	}
	if (!IsWin8()) {
		return	TIS_SHOW;
	}

	DWORD		pref = 0;
	ITrayNotify8 *tn = NULL;

	CoCreateInstance(TrayNotifyId, NULL, CLSCTX_LOCAL_SERVER, __uuidof(ITrayNotify8),
		(void **)&tn);
	if (tn) {
		INotifyCb	*incb = new INotifyCb;
		u_long		incb_cookie;

		tn->RegisterCallback(incb, &incb_cookie);
		pref = incb->pref;
		tn->UnregisterCallback(incb_cookie);
		tn->Release();
	}

	return	pref == 2 ? TIS_SHOW : TIS_HIDE;
}

/* =======================================================================
	Application ID Functions
 ======================================================================= */
BOOL SetWinAppId(HWND hWnd, const WCHAR *app_id)
{
	static HRESULT (WINAPI *pSHGetPropertyStoreForWindow)(HWND, REFIID, void**) = []() {
		return	(HRESULT (WINAPI *)(HWND, REFIID, void**))
			::GetProcAddress(::GetModuleHandle("shell32"), "SHGetPropertyStoreForWindow");
	}();

	if (!pSHGetPropertyStoreForWindow) {
		return	FALSE;
	}

	IPropertyStore *pps;
	HRESULT hr = pSHGetPropertyStoreForWindow(hWnd, IID_PPV_ARGS(&pps));
	if (SUCCEEDED(hr)) {
		PROPVARIANT pv;
		hr = ::InitPropVariantFromString(app_id, &pv);
		if (SUCCEEDED(hr)) {
			hr = pps->SetValue(PKEY_AppUserModel_ID, pv);
			::PropVariantClear(&pv);
		}
		pps->Release();
	}
	return	SUCCEEDED(hr);
}

static int CALLBACK font_enum_proc(ENUMLOGFONTEXW *elf, NEWTEXTMETRICEX *ntm,
	DWORD fontType, LPARAM found_p)
{
	BOOL	&found = *(BOOL *)found_p;

	found = TRUE;

	return	0;
}

BOOL IsInstalledFont(HDC hDc, const WCHAR *face_name, BYTE charset)
{
	LOGFONTW	lf = {};
	BOOL		found = FALSE;

	lf.lfCharSet = charset;
	wcscpy(lf.lfFaceName, face_name);

	::EnumFontFamiliesExW(hDc, &lf, (FONTENUMPROCW)font_enum_proc, (LPARAM)&found, 0);

	return	found;
}

/* =======================================================================
	Firewall Functions (require CoInitialize())
 ======================================================================= */
#include <netfw.h>
#include <OleAuto.h>

// Windows Firewall 除外設定を参照する 3rd Party Firewallリスト
static const WCHAR *Fw3rdPartyExceptKeywords[] = {
	L"ESET ",
	NULL,
};

struct Is3rdPartyFwEnabledParam {
	BOOL	use_except_list;
	BOOL	ret;
};

unsigned __stdcall Is3rdPartyFwEnabledProc(void *_param)
{
	Is3rdPartyFwEnabledParam	*param = (Is3rdPartyFwEnabledParam *)_param;
	INetFwProducts				*fwProd = NULL;

	param->ret = FALSE;

	__try {
		::CoInitialize(NULL);

		CoCreateInstance(__uuidof(NetFwProducts), 0, CLSCTX_INPROC_SERVER,
			__uuidof(INetFwProducts), (void **)&fwProd);

		if (!fwProd) {
			goto END;
		}
		long	cnt = 0;
		fwProd->get_Count(&cnt);
		fwProd->Release();

		if (cnt == 0 || !param->use_except_list) {
			goto END;
		}
		for (int i=0; i < cnt; i++) {
			WCHAR	name[MAX_PATH];
			if (Get3rdPartyFwName(i, name, MAX_PATH)) {
				for (int j=0; Fw3rdPartyExceptKeywords[j]; j++) {
					if (wcsstr(name, Fw3rdPartyExceptKeywords[j])) {
						goto END;	// 1件でもマッチすれば WinFW有効と判断
					}
				}
			}
		}
		param->ret = TRUE;
	}
	__except(EXCEPTION_EXECUTE_HANDLER) {
		param->ret = TRUE;
		DBG("INetFwProducts exception\n");
	}

END:
	_endthreadex(0);
	return	0;
}

BOOL Is3rdPartyFwEnabled(BOOL use_except_list, DWORD timeout, BOOL *is_timeout)
{
	Is3rdPartyFwEnabledParam	param = { use_except_list, FALSE };

	HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, Is3rdPartyFwEnabledProc, 
		(void *)&param, 0, NULL);

	DWORD	ret = ::WaitForSingleObject(hThread, timeout);
	if (ret == WAIT_TIMEOUT) {
		if (is_timeout) {
			*is_timeout = TRUE;
		}
		DBG("FW check timeout\n");

		::TerminateThread(hThread, 0); // 強制終了
		::CloseHandle(hThread);
		return	FALSE;
	}
	if (is_timeout) {
		*is_timeout = FALSE;
	}
	::CloseHandle(hThread);
	return	param.ret;
}

int Get3rdPartyFwNum()
{
	INetFwProducts	*fwProd = NULL;

	CoCreateInstance(__uuidof(NetFwProducts), 0, CLSCTX_INPROC_SERVER,
		__uuidof(INetFwProducts), (void **)&fwProd);

	if (!fwProd) {
		return 0;
	}
	long	cnt = 0;
	fwProd->get_Count(&cnt);

	fwProd->Release();

	return	(int)cnt;
}

BOOL Get3rdPartyFwName(int idx, WCHAR *name, int max_len)
{
	BOOL			ret = FALSE;
	INetFwProducts	*fwProd = NULL;

	CoCreateInstance(__uuidof(NetFwProducts), 0, CLSCTX_INPROC_SERVER,
		__uuidof(INetFwProducts), (void **)&fwProd);

	if (fwProd) {
		INetFwProduct	*product = NULL;
		fwProd->Item(idx, &product);
		if (product) {
			BSTR	bName = NULL;
			if (product->get_DisplayName(&bName) == S_OK) {
				wcsncpyz(name, bName, max_len);
				ret = TRUE;
				::SysFreeString(bName);
				DBGW(L"dispname=<%s>\n", name);
			}
		}
		fwProd->Release();
	}

	return	ret;
}

static INetFwProfile* GetFwProfile()
{
	INetFwMgr		*fwMgr  = NULL;
	INetFwPolicy	*fwPlcy = NULL;
	INetFwProfile	*fwProf = NULL;

	::CoCreateInstance(__uuidof(NetFwMgr), NULL, CLSCTX_INPROC_SERVER,
		__uuidof(INetFwMgr), (void **)&fwMgr );

	if (fwMgr) {
		fwMgr->get_LocalPolicy(&fwPlcy);
		if (fwPlcy) {
			fwPlcy->get_CurrentProfile(&fwProf);
			fwPlcy->Release();
		}
		fwMgr->Release();
	}

    return fwProf;
}


static BSTR GetSysFileName(const WCHAR *path)
{
	WCHAR	wpath[MAX_PATH];

	if (!path) {
		::GetModuleFileNameW(0, wpath, wsizeof(wpath));
		path = wpath;
	}
	return	::SysAllocString(path);
}

BOOL GetFwStatus(const WCHAR *path, FwStatus *fs)
{
	fs->Init();

	INetFwProfile	*fwProf = GetFwProfile();
	if (!fwProf) {
		return FALSE;
	}
	VARIANT_BOOL	vfw_enable = VARIANT_FALSE;
	fwProf->get_FirewallEnabled(&vfw_enable);
	fs->fwEnable = (vfw_enable != VARIANT_FALSE);

	INetFwAuthorizedApplications	*fwApps = NULL;
	fwProf->get_AuthorizedApplications(&fwApps);

	if (fwApps) {
		INetFwAuthorizedApplication	*app = NULL;
		BSTR	bpath = GetSysFileName(path);

		fwApps->Item(bpath, &app);
		if (app) {
			fs->entryCnt  = 1;
			fs->enableCnt = 1;
			VARIANT_BOOL	vent_enable = VARIANT_FALSE;
			app->get_Enabled(&vent_enable);
			if (vent_enable != VARIANT_FALSE) {
				fs->allowCnt++;
			}
			else {
				fs->blockCnt++;
			}
			app->Release();
		}
		::SysFreeString(bpath);
		fwApps->Release();
	}

	fwProf->Release();

	return	TRUE;
}

BOOL SetFwStatus(const WCHAR *path, const WCHAR *label, BOOL enable)
{
	INetFwProfile	*fwProf = GetFwProfile();

	if (!fwProf) {
		return FALSE;
	}
	BOOL	ret = FALSE;

	INetFwAuthorizedApplications	*fwApps = NULL;
	fwProf->get_AuthorizedApplications(&fwApps);

	if (fwApps) {
		INetFwAuthorizedApplication	*app = NULL;

		::CoCreateInstance( __uuidof(NetFwAuthorizedApplication), NULL, CLSCTX_INPROC_SERVER,
			__uuidof(INetFwAuthorizedApplication), (void **)&app);
		if (app) {
			BSTR	bpath  = GetSysFileName(path);
			BSTR	blabel = GetSysFileName(label ? label : path);

			app->put_ProcessImageFileName(bpath);
			app->put_Name(blabel);
			if (!enable) {
				app->put_Enabled(VARIANT_FALSE);
			}
			app->put_Name(blabel);
			if (fwApps->Add(app) >= S_OK) {
				ret = TRUE;
			}
			::SysFreeString(blabel);
			::SysFreeString(bpath);
			app->Release();
		}
		fwApps->Release();
	}
	fwProf->Release();

	return	ret;
}

BOOL DelFwStatus(const WCHAR *path)
{
	INetFwProfile	*fwProf = GetFwProfile();

	if (!fwProf) {
		return FALSE;
	}
	BOOL	ret = FALSE;

	INetFwAuthorizedApplications	*fwApps = NULL;
	fwProf->get_AuthorizedApplications(&fwApps);

	if (fwApps) {
		BSTR	bpath = GetSysFileName(path);
		ret = fwApps->Remove(bpath) >= S_OK;
		::SysFreeString(bpath);
		fwApps->Release();
	}
	fwProf->Release();

	return	ret;
}

static BOOL CheckFwRule(BSTR bpath, INetFwRule *rule, FwStatus *fs)
{
	long	profTypes = 0;

	if (rule->get_Profiles(&profTypes) < S_OK) {
		return	FALSE;
	}
	if ((fs->profTypes & profTypes) == 0) {	// 現在のプロファイルに合致しない
		return	FALSE;
	}

	BSTR	bapp = NULL;
	WCHAR	app[MAX_PATH];

	if (rule->get_ApplicationName(&bapp) < S_OK) {
		return	FALSE;
	}

	if (::ExpandEnvironmentStringsW(bapp, app, wsizeof(app)) > 0) {
		if (wcsicmp(app, bpath) == 0) {
			VARIANT_BOOL	vb = VARIANT_FALSE;
			rule->get_Enabled(&vb);
			fs->entryCnt++;

			if (vb != VARIANT_FALSE) {
				fs->enableCnt++;
				NET_FW_ACTION nfa;
				if (rule->get_Action(&nfa) >= S_OK) {
					if (nfa == NET_FW_ACTION_ALLOW) {
						fs->allowCnt++;
					}
					else {
						fs->blockCnt++;
					}
				}
			}
			else {
				fs->disableCnt++;
			}
		}
	}
	::SysFreeString(bapp);
	return	TRUE;
}

BOOL GetFwStatusEx(const WCHAR *path, FwStatus *fs)
{
	fs->Init();

	INetFwPolicy2	*plcy = NULL;

	if (::CoCreateInstance(__uuidof(NetFwPolicy2), NULL, CLSCTX_INPROC_SERVER,
		__uuidof(INetFwPolicy2), (void **)&plcy) < S_OK) {
		return	FALSE;
	}

	long	ptype = 0;
	plcy->get_CurrentProfileTypes(&ptype);
	fs->profTypes = ptype;

	for (DWORD i=1; ptype; i <<= 1) {
		if ((ptype & i) == 0) {
			continue;
		}
		ptype &= ~i;

		VARIANT_BOOL vfw_enable = VARIANT_FALSE;
		plcy->get_FirewallEnabled((NET_FW_PROFILE_TYPE2)i, &vfw_enable);
		fs->fwEnable = (vfw_enable != VARIANT_FALSE) ? TRUE : FALSE;
	}

	if (fs->fwEnable) {
		INetFwRules	*rules = NULL;

		if (plcy->get_Rules(&rules) >= S_OK) {
			IUnknown *pUnk = NULL;

			if (rules->get__NewEnum(&pUnk) >= S_OK) {
				IEnumVARIANT	*pEnum = NULL;

				if (pUnk->QueryInterface(IID_IEnumVARIANT, (void **)&pEnum) >= S_OK) {
					long	count = 0;
					VARIANT	var;
					ULONG	lFetch = 0;
					::VariantInit(&var);
					rules->get_Count(&count);
					BSTR	bpath = GetSysFileName(path);

					for (int i=0; i < count && pEnum->Next(1, &var, &lFetch) >= S_OK; i++) {
						INetFwRule	*rule = NULL;
						if (V_DISPATCH(&var)->QueryInterface(IID_INetFwRule, (void **)&rule)
							>= S_OK) {
							CheckFwRule(bpath, rule, fs);
							rule->Release();
						}
						::VariantClear(&var);
					}
					::SysFreeString(bpath);
				}
			}
			rules->Release();
		}
	}
	plcy->Release();

	return	TRUE;
}

static BOOL SetFwRule(BSTR bpath, BSTR blabel, INetFwRule *rule, long prof_type)
{
//	long	local_ptype = 0;
//
//	if (rule->get_Profiles(&local_ptype) < S_OK) {
//		return	FALSE;
//	}
//	if ((prof_type & local_ptype) == 0) {	// 現在のプロファイルに合致しない
//		return	FALSE;
//	}

	BSTR	bapp = NULL;
	WCHAR	app[MAX_PATH];

	if (rule->get_ApplicationName(&bapp) < S_OK) {
		return	FALSE;
	}

	if (::ExpandEnvironmentStringsW(bapp, app, wsizeof(app)) > 0) {
		if (wcsicmp(app, bpath) == 0) {
			VARIANT_BOOL	vb = VARIANT_FALSE;
			rule->get_Enabled(&vb);

			if (vb != VARIANT_FALSE) {
				NET_FW_ACTION nfa;
				if (rule->get_Action(&nfa) >= S_OK) {
					if (nfa == NET_FW_ACTION_BLOCK) {
						rule->put_Action(NET_FW_ACTION_ALLOW);
					}
				}
			}
		}
	}
	::SysFreeString(bapp);
	return	TRUE;
}

BOOL SetFwStatusEx(const WCHAR *path, const WCHAR *label, BOOL enable)
{
	SetFwStatus(path, label, enable);

	INetFwPolicy2	*plcy = NULL;

	if (::CoCreateInstance(__uuidof(NetFwPolicy2), NULL, CLSCTX_INPROC_SERVER,
		__uuidof(INetFwPolicy2), (void **)&plcy) < S_OK) {
		return	FALSE;
	}

	INetFwRules	*rules = NULL;
	long		ptype = 0;
	plcy->get_CurrentProfileTypes(&ptype);

	if (plcy->get_Rules(&rules) >= S_OK) {
		IUnknown *pUnk = NULL;

		if (rules->get__NewEnum(&pUnk) >= S_OK) {
			IEnumVARIANT	*pEnum = NULL;

			if (pUnk->QueryInterface(IID_IEnumVARIANT, (void **)&pEnum) >= S_OK) {
				long	count = 0;
				VARIANT	var;
				ULONG	lFetch = 0;
				::VariantInit(&var);
				rules->get_Count(&count);
				BSTR	bpath  = GetSysFileName(path);
				BSTR	blabel = GetSysFileName(label ? label : path);

				for (int i=0; i < count && pEnum->Next(1, &var, &lFetch) >= S_OK; i++) {
					INetFwRule	*rule = NULL;
					if (V_DISPATCH(&var)->QueryInterface(IID_INetFwRule, (void **)&rule)
						>= S_OK) {
						SetFwRule(bpath, blabel, rule, ptype);
						rule->Release();
					}
					::VariantClear(&var);
				}
				::SysFreeString(blabel);
				::SysFreeString(bpath);
			}
		}
		rules->Release();
	}
	plcy->Release();

	return	TRUE;
}

// 正確な GetTextExtentExPointW
BOOL TGetTextWidth(HDC hDc, const WCHAR *s, int len, int width, int *rlen, int *rcx)
{
	TRect	rc;
	TSize	sz;

	if (!::GetTextExtentExPointW(hDc, s, len, width, rlen, NULL, &sz)) {
		return	FALSE;
	}
//	if (!::GetTextExtentExPointW(hDc, s, *rlen, width, rlen, NULL, &sz)) {
//		return	FALSE;
//	}
	::DrawTextW(hDc, s, *rlen, &rc, DT_CALCRECT|DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);

//	if (rc.cx() <= width) {
//		if (rc.cx() != sz.cx) {
//			DBGW(L" GetTextWidth: %d %d %.*s\n", rc.cx(), sz.cx, *rlen, s);
//		}
//	}
	*rcx = rc.cx();
	return	TRUE;
}

HBITMAP TDIBtoDDB(HBITMAP hDibBmp) // 8bit には非対応
{
	HBITMAP		hDdbBmp  = NULL;
	HWND		hDesktop = ::GetDesktopWindow();
	HDC			hDc = ::GetDC(hDesktop);
	DIBSECTION	ds;

	if (::GetObject(hDibBmp, sizeof(DIBSECTION), &ds)) {
		hDdbBmp = ::CreateDIBitmap(hDc, &ds.dsBmih, CBM_INIT, ds.dsBm.bmBits,
			(BITMAPINFO *)&ds.dsBmih, DIB_RGB_COLORS);
	}
	::ReleaseDC(hDesktop, hDc);

	return	hDdbBmp;
}

BOOL TOpenExplorerSelOneW(const WCHAR *path)
{
	WCHAR	buf[MAX_PATH + 20];
	snwprintfz(buf, wsizeof(buf), L"/select,%s", path);
	return	INT_RDC(::ShellExecuteW(0, 0, L"explorer", buf, 0, SW_SHOW)) > 32;
}

// CoInitialize系必須
BOOL TOpenExplorerSelW(const WCHAR *dir, WCHAR **path, int num)
{
	BOOL	ret = FALSE;
	size_t	len = wcslen(dir);

	if (PIDLIST_ABSOLUTE iDir = ::ILCreateFromPathW(dir)) {
		if (PIDLIST_ABSOLUTE *items = new ITEMIDLIST *[num]) {
			memset(items, 0, sizeof(ITEMIDLIST *) * num);
			for (int i=0; i < num; i++) {
				if (wcslen(path[i]) + len + 2 < MAX_PATH) {
					WCHAR	buf[MAX_PATH];
					MakePathW(buf, dir, path[i]);
					items[i] = ::ILCreateFromPathW(buf);
				}
			}

			HRESULT hr = ::SHOpenFolderAndSelectItems(iDir, num, (LPCITEMIDLIST *)items, 0);
			if (hr >= S_OK || hr == E_INVALIDARG) {
				ret = TRUE; // items のエラーの場合は、shell は開く
			}
			for (int i=0; i < num; i++) {
				if (items[i]) {
					::ILFree(items[i]);
				}
			}
			delete [] items;
		}
		::ILFree(iDir);
	}
	return	ret;
}

BOOL TSetClipBoardTextW(HWND hWnd, const WCHAR *s, int len)
{
	if (len < 0) {
		len = (int)wcslen(s);
	}
	HANDLE	hGlobal = ::GlobalAlloc(GHND, (len + 1) * sizeof(WCHAR));

	if (!hGlobal) {
		return FALSE;
	}

	BOOL	ret = FALSE;
	WCHAR	*p = (WCHAR *)::GlobalLock(hGlobal);

	if (p) {
		wcsncpyz(p, s, len+1);
		::GlobalUnlock(hGlobal);
		::OpenClipboard(hWnd);
		::EmptyClipboard();

		if (::SetClipboardData(CF_UNICODETEXT, hGlobal)) {
			ret = TRUE;
		}
		::CloseClipboard();
	}

	if (!ret) {
		::GlobalFree(hGlobal);
	}

	return ret;
}

// バッファオーバーフロー検出用テストルーチン
void bo_test_core(char *buf)
{
	static char *p;

	p = buf;
	memset(p, 0x33, 200);
}

void bo_test()
{
	static char *p;
	char buf[100];

	p = buf;
	bo_test_core(p);
}

#if !defined(_DEBUG) &&  _MSC_VER >= 1900 && _MSC_VER <= 1915
#define ENABLE_GS_FAILURE_HACK
extern "C" __declspec(noreturn) void __cdecl __raise_securityfailure(PEXCEPTION_POINTERS const exception_pointers);
#endif

// バッファオーバーフローをApp側例外ハンドラでキャッチするためのhack
// Prevent to avoid FastCopy's ExceptionFilter by __report_gsfailure/__report_securityfailure
//  like a _set_security_error_handler
void TGsFailureHack()
{
#ifdef ENABLE_GS_FAILURE_HACK
	DWORD	flag = 0;

//	__raise_securityfailure(NULL);

	BYTE	*p = (BYTE *)&__raise_securityfailure;
	if (::VirtualProtect(p, 32, PAGE_EXECUTE_READWRITE, &flag)) {
#ifdef _WIN64
		memcpy(p+11, "\x90\x90\x90\x90\x90\x90", 6); // nop (overwrite SetUnhandledFilter call)
#else
		memcpy(p+5, "\x90\x90\x90\x90\x90\x90", 6);	// nop (overwrite SetUnhandledFilter call)
#endif
		::VirtualProtect(p, 32, flag, &flag);

		p = (BYTE *)&__report_gsfailure;
		if (::VirtualProtect(p, 32, PAGE_EXECUTE_READWRITE, &flag)) {
#ifdef _WIN64
			memcpy(p+9, "\xeb\x13", 2);		// jump 0x13 (skip to select deubbger)
#else
			memcpy(p+9, "\x74\x10", 2);		// jump 0x10 (skip to select deubbger)
#endif
			::VirtualProtect(p, 32, flag, &flag);
		}
	}
#endif
}

/*
	マスク情報をアルファ値として引き継ぐ形でDIBSectionを作成
	主に、SetMenuItemBitmaps用（GDI+なら Bitmap::FromFile(icon) で簡単…）
	今のところ、意図的に GetIconInfo は使わず、cx/cyはユーザ指定させる
*/
HBITMAP TIconToBmp(HICON hIcon, int cx, int cy)
{
	BITMAPINFO	bmi = {};
	bmi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth    = cx;
	bmi.bmiHeader.biHeight   = cy;
	bmi.bmiHeader.biPlanes   = 1;
	bmi.bmiHeader.biBitCount = 32;

	HDC		hBmpDc = ::CreateCompatibleDC(NULL);
	HDC		hTmpDc = ::CreateCompatibleDC(NULL);
	void	*dat = NULL;
	void	*tmp = NULL;
	HBITMAP	hBmp = ::CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &dat, 0, 0);
	HBITMAP hTmp = ::CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &tmp, 0, 0);
	HGDIOBJ	hBmpSv = ::SelectObject(hBmpDc, hBmp);
	HGDIOBJ	hTmpSv = ::SelectObject(hTmpDc, hTmp);

	::DrawIconEx(hBmpDc, 0, 0, hIcon, cx, cy, 0, 0, DI_NORMAL);
	::DrawIconEx(hTmpDc, 0, 0, hIcon, cx, cy, 0, 0, DI_MASK);

	for (int i=0; i < cy; i++) {
		for (int j=0; j < cx; j++) {
			if (((BYTE *)tmp)[(i*cx + j) * 4] == 0) {
				((BYTE *)dat)[(i*cx + j) * 4 + 3] = 0xff;
			}
//			for (int k=0; k < 4; k++) {
//				DBG("%02x:", ((BYTE *)dat)[(i*cx + j) * 4 + k]);
//			}
//			DBG(" ");
		}
//		DBG("\n");
	}

	::SelectObject(hBmpDc, hBmpSv);
	::SelectObject(hTmpDc, hTmpSv);
	::DeleteDC(hBmpDc);
	::DeleteDC(hTmpDc);

	::DeleteObject(hTmp);
	return	hBmp;
}

BOOL IsWineEnvironment()
{
	HMODULE	ntdll = ::GetModuleHandle("ntdll.dll");

	return	(ntdll && ::GetProcAddress(ntdll, "wine_get_version")) ? TRUE : FALSE;
}

CRITICAL_SECTION *TLibCs()
{
	return	&gCs;
}

DWORD64 GetTick64()
{
	static DWORD64 (WINAPI *pGetTickCount64)(void) = []() {
		return	(DWORD64 (WINAPI *)(void))
			::GetProcAddress(::GetModuleHandle("kernel32"), "GetTickCount64");
	}();

	if (pGetTickCount64) {
		return	pGetTickCount64();
	}

	::EnterCriticalSection(&gCs);

	static DWORD64	lastUpper;
	static DWORD	lastLower;

	DWORD	cur = ::GetTickCount();
	if (cur < lastLower) {
		lastUpper += (DWORD64)1 << 32;
	}
	DWORD64	ret = lastUpper | lastLower;

	lastLower = cur;

	::LeaveCriticalSection(&gCs);

	return	ret;
}

DWORD GetTick() {
	return	::GetTickCount();
}


#include <psapi.h>
#pragma comment (lib, "psapi.lib")

BOOL IsLockedScreen()
{
	static list<HWND>	hlist;
	static map<HWND, pair<BOOL, decltype(hlist.begin())>>	hmap;

	HWND	hWnd = ::GetForegroundWindow();

	if (!hWnd) {
		DBG("NULL\n");
		return	TRUE;
	}

	auto	itr = hmap.find(hWnd);
	if (itr != hmap.end()) {
		hlist.erase(itr->second.second);
		itr->second.second = hlist.insert(hlist.end(), hWnd);
		DBG("cached %p %d\n", hWnd, itr->second.first);
		return	itr->second.first;
	}

	while (hlist.size() > 10) {
		auto	itr = hlist.begin();
		DBG("erase cache %p %d\n", *itr, hmap[*itr].first);
		hmap.erase(*itr);
		hlist.erase(itr);
	}

	BOOL	ret = FALSE;
	DWORD	pid = 0;

	::GetWindowThreadProcessId(hWnd, &pid);
	if (pid) {
		HANDLE	hProc = NULL;
		if (!(hProc = ::OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid))) {
			hProc = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
		}
		if (hProc) {
			WCHAR	wbuf[MAX_PATH];
			if (::GetProcessImageFileNameW(hProc, wbuf, wsizeof(wbuf))) {
				size_t	len = wcslen(wbuf);

#define LOCKAPP_NAME	L"\\LockApp.exe"
#define LOCKAPP_LEN		12
				if (len > LOCKAPP_LEN && wcsicmp(wbuf+len-LOCKAPP_LEN, LOCKAPP_NAME) == 0) {
					ret = TRUE;
					DBG("Lock Detect %p\n", hWnd);
				}
				//else DBGW(L"Non Lock Detect(%s) %p\n", wbuf, hWnd);
				::CloseHandle(hProc);
			}
		}
	}
	hmap[hWnd] = pair<BOOL, decltype(hlist.begin())>(ret, hlist.insert(hlist.end(), hWnd));

	return	ret;
}

BOOL TSetDefaultDllDirectories(DWORD flags)
{
	static BOOL (WINAPI *pSetDefaultDllDirectories)(DWORD) = []() {
		return	(BOOL (WINAPI *)(DWORD))
			::GetProcAddress(::GetModuleHandle("kernel32"), "SetDefaultDllDirectories");
	}();

	if (pSetDefaultDllDirectories) {
		return	pSetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
	}
	return	FALSE;
}

const WCHAR *TGetSysDirW(WCHAR *buf)
{
	static WCHAR	*sysDir = []() {
		static WCHAR	dir[MAX_PATH];
		::GetSystemDirectoryW(dir, wsizeof(dir));
		return	dir;
	}();

	if (buf) {
		wcscpyz(buf, sysDir);
	}

	return	sysDir;
}

const WCHAR *TGetExeDirW(WCHAR *buf)
{
	static WCHAR	*exeDir = []() {
		static WCHAR	dir[MAX_PATH];
		WCHAR	tmp[MAX_PATH];

		::GetModuleFileNameW(NULL, tmp, wsizeof(tmp));
		GetParentDirW(tmp, dir);
		return	dir;
	}();

	if (buf) {
		wcscpyz(buf, exeDir);
	}

	return	exeDir;
}

HMODULE TLoadLibraryExW(const WCHAR *dll, TLoadType t)
{
	const WCHAR	*targDir = NULL;

	switch (t) {
	case TLT_SYSDIR:
		targDir = TGetSysDirW();
		break;

	case TLT_EXEDIR:
		targDir = TGetExeDirW();
		break;

	default:
		targDir = NULL;
		break;
	}

	if (!targDir || !*targDir) {
		return	NULL;
	}

	WCHAR	path[MAX_PATH];
	MakePathW(path, targDir, dll);

	return	TLoadLibraryW(path);
}

BOOL TIsAdminGroup()
{
	static BOOL	ret = []() {
		BOOL	is_admin = FALSE;
		WCHAR	user[MAX_PATH] = {};
		DWORD	size = wsizeof(user);

		if (::GetUserNameW(user, &size)) {
			USER_INFO_1	*ui = NULL;

			if (::NetUserGetInfo(0, user, 1, (BYTE **)&ui) == NERR_Success) {
				if (ui->usri1_priv == USER_PRIV_ADMIN) {
					is_admin = TRUE;
				}
				::NetApiBufferFree(ui);
			}
		}
		return	is_admin;
	}();

	return	ret;
}


void time_to_SYSTEMTIME(time_t t, SYSTEMTIME *st, BOOL is_local)
{
	FILETIME	ft;
	UnixTime2FileTime(t, &ft);

	if (is_local) {
		SYSTEMTIME	st_tmp;
		::FileTimeToSystemTime(&ft, &st_tmp);
		::SystemTimeToTzSpecificLocalTime(NULL, &st_tmp, st);
	}
	else {
		::FileTimeToSystemTime(&ft, st);
	}
}

time_t SYSTEMTIME_to_time(const SYSTEMTIME &st, BOOL is_local)
{
	FILETIME	ft;

	if (is_local) {
		SYSTEMTIME	st_tmp;
		::TzSpecificLocalTimeToSystemTime(NULL, &st, &st_tmp);
		::SystemTimeToFileTime(&st_tmp, &ft);
	}
	else {
		::SystemTimeToFileTime(&st, &ft);
	}

	return	FileTime2UnixTime(&ft);
}

BOOL TGetIntegrityLevel(DWORD *ilv)
{
	HANDLE hToken = NULL;

	if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &hToken)) return FALSE;
	scope_defer([&]() { if (hToken) ::CloseHandle(hToken); });

	DWORD	len = 0;
	::GetTokenInformation(hToken, TokenIntegrityLevel, NULL, 0, &len);
	if (len == 0) return FALSE;

	auto tml = scope_raii((PTOKEN_MANDATORY_LABEL)malloc(len), [](auto tml) { free(tml); });
	if (!tml) return FALSE;

	if (!GetTokenInformation(hToken, TokenIntegrityLevel, tml, len, &len)) return FALSE;

	*ilv = *GetSidSubAuthority(tml->Label.Sid,
			(DWORD)(UCHAR)(*GetSidSubAuthorityCount(tml->Label.Sid) - 1));

	return	TRUE;
}

BOOL TIsLowIntegrity()
{
	DWORD	ilv = SECURITY_MANDATORY_LOW_RID;

	if (!TGetIntegrityLevel(&ilv)) {
		return	FALSE;
	}

	// SECURITY_MANDATORY_LOW_RID
	// SECURITY_MANDATORY_MEDIUM_RID
	// SECURITY_MANDATORY_HIGH_RID
	// SECURITY_MANDATORY_SYSTEM_RID

	return	ilv == SECURITY_MANDATORY_LOW_RID;
}

void U8Out(const char *fmt,...)
{
	static HANDLE hStdOut = ::GetStdHandle(STD_OUTPUT_HANDLE);
	static BOOL isConsole = TRUE;

	va_list	ap;
	va_start(ap, fmt);

	int len = vsnprintfz(NULL, 0, fmt, ap) + 1;

	U8str	u8(len);

	len = vsnprintfz(u8.Buf(), len, fmt, ap);
	va_end(ap);

	if (isConsole) {
		Wstr	w(u8.s());
		DWORD	wlen = w.Len();

		if (!::WriteConsoleW(hStdOut, w.s(), wlen, &wlen, 0)) {
			isConsole = FALSE;
		}
	}
	if (!isConsole) {
		DWORD	crlen = len * 2 + 1;
		U8str	u8cr(crlen);
		crlen = UnixNewLineToLocal(u8.s(), u8cr.Buf(), crlen);
		::WriteFile(hStdOut, u8cr.s(), crlen, &crlen, 0);
	}
}

BOOL TGetUrlAssocAppW(const WCHAR *scheme, WCHAR *wbuf, int max_len)
{
	TRegistry	reg(HKEY_CURRENT_USER);

	WCHAR	reg_path[MAX_PATH];
	snwprintfz(reg_path, wsizeof(reg_path),
		L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\%s\\UserChoice",
		scheme);
	if (reg.OpenKeyW(reg_path)) {
		WCHAR	progId[MAX_PATH];
		if (reg.GetStrW(L"ProgId", progId, sizeof(progId))) { // サイズはbyte指定
			reg.ChangeTopKey(HKEY_CLASSES_ROOT);

			snwprintfz(reg_path, wsizeof(reg_path), L"%s\\shell\\open\\command", progId);
			if (reg.OpenKeyW(reg_path)) {
				if (reg.GetStrW(NULL, progId, sizeof(progId))) { // サイズはbyte指定
					WCHAR	*p = NULL;
					if (auto tok = strtok_pathW(progId, L" ", &p, FALSE)) {
						wcsncpyz(wbuf, tok, max_len);
						return	TRUE;
					}
				}
			}
		}
	}
	return	FALSE;
}

time_t TGetBuildTimestamp()
{
	static time_t	t = [](){
		char	mon[4];
		struct tm tm = {};
		// Fri 19 Aug 13:32:58 2016
		Debug(__TIMESTAMP__);
		if (sscanf(__TIMESTAMP__, "%*3c %3s %d %d:%d:%d %d", mon, &tm.tm_mday,
							&tm.tm_hour, &tm.tm_min, &tm.tm_sec, &tm.tm_year) != 6) {
			return	(time_t)0;
		}
		tm.tm_year -= 1900;
		tm.tm_mon = [&]() {	// Jan to 0
			const char *mon_def[] =
				{ "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", 0 };
			for (int i=0; mon_def[i]; i++) {
				if (strcmp(mon_def[i], mon) == 0) return i;
			}
			return	0;
		}();
		return	mktime(&tm);
	}();

	return	t;
}


