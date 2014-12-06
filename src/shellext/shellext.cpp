static char *shellext_id = 
	"@(#)Copyright (C) 2005-2012 H.Shirouzu		shellext.cpp	Ver2.10";
/* ========================================================================
	Project  Name			: Shell Extension for Fast Copy
	Create					: 2005-01-23(Sun)
	Update					: 2012-06-17(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "../tlib/tlib.h"
#include <olectl.h>
#include "resource.h"

#pragma data_seg(".text")
#define INITGUID
#include <initguid.h>
#include <shlguid.h>
#include "shelldef.h"
#include "shellext.h"
#pragma data_seg()

static ShellExtSystem	*SysObj = NULL;

// レジストリ登録キー（Ref: tortoise subversion）
static char	*DllRegKeys[] = {
	"*\\shellex\\ContextMenuHandlers",
	"*\\shellex\\DragDropHandlers",
	"Folder\\shellex\\ContextMenuHandlers",
	"Folder\\shellex\\DragDropHandlers",
	"Directory\\shellex\\ContextMenuHandlers",
	"Directory\\Background\\shellex\\ContextMenuHandlers",
	"Directory\\shellex\\DragDropHandlers",
	"Drive\\shellex\\ContextMenuHandlers",
	"Drive\\shellex\\DragDropHandlers",
	"InternetShortcut\\shellex\\ContextMenuHandlers",
	"lnkfile\\shellex\\ContextMenuHandlers",
	NULL
};

static void	*FMT_TOSTR_V;

//#define SHEXT_DEBUG_LOG

#ifdef SHEXT_DEBUG_LOG

static CRITICAL_SECTION cs;

DWORD DbgLog(char *fmt,...)
{
	static HANDLE hLogFile = INVALID_HANDLE_VALUE;
	DWORD	ret = 0;

	::EnterCriticalSection(&cs);

	if (hLogFile == INVALID_HANDLE_VALUE) {
		hLogFile = ::CreateFile("c:\\temp\\shellext.log", GENERIC_WRITE,
					FILE_SHARE_READ|FILE_SHARE_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	}
	if (hLogFile != INVALID_HANDLE_VALUE) {
		char	buf[16384];

		DWORD	len = wsprintf(buf, "%04x: ", ::GetCurrentThreadId());

		va_list	va;
		va_start(va, fmt);
		len += wvsprintf(buf + len, fmt, va);
		va_end(va);

		::SetFilePointer(hLogFile, ::GetFileSize(hLogFile, 0), 0, FILE_BEGIN);
		::WriteFile(hLogFile, buf, len, &len, 0);
	}

	::LeaveCriticalSection(&cs);

	return	ret;
}

DWORD DbgLogW(WCHAR *fmt,...)
{
	WCHAR	wbuf[8192];
	char	buf[sizeof(wbuf)];

	va_list	va;
	va_start(va, fmt);
	DWORD	len = wvsprintfW(wbuf, fmt, va);
	va_end(va);

	WtoA(wbuf, buf, sizeof(buf), len + 1);
	return	DbgLog("%s", buf);
}
#else
#define DbgLog
#define DbgLogW
#endif

/*=========================================================================
  クラス ： ShellExt
  概  要 ： シェル拡張クラス
  説  明 ： 
  注  意 ： 
=========================================================================*/
ShellExt::ShellExt(void)
{
	refCnt = 0;
	isCut = FALSE;
	dataObj = NULL;
	if (SysObj)
		SysObj->DllRefCnt++;
	::CoInitialize(0);
}

ShellExt::~ShellExt()
{
	if (dataObj)
		dataObj->Release();
	if (SysObj)
		SysObj->DllRefCnt--;

	::CoUninitialize();
}

STDMETHODIMP ShellExt::Initialize(LPCITEMIDLIST pIDFolder, IDataObject *pDataObj, HKEY hRegKey)
{
	WCHAR	path[MAX_PATH_EX];

	srcArray.Init();
	dstArray.Init();

	if (dataObj) {
		dataObj->Release();
		dataObj = NULL;
	}

	if ((dataObj = pDataObj)) {
		pDataObj->AddRef();

		STGMEDIUM	medium;
		FORMATETC	fe = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };

		if (pDataObj->GetData(&fe, &medium) < 0)
			return E_INVALIDARG;

		HDROP	hDrop = (HDROP)::GlobalLock(medium.hGlobal);
		int max = DragQueryFileV(hDrop, 0xffffffff, NULL, 0);

		for (int i=0; i < max; i++) {
			DragQueryFileV(hDrop, i, path, sizeof(path) / CHAR_LEN_V);
			srcArray.RegisterPath(path);
		}
		::GlobalUnlock(medium.hGlobal);
		::ReleaseStgMedium(&medium);
	}

	if (pIDFolder && SHGetPathFromIDListV(pIDFolder, path)) {
		dstArray.RegisterPath(path);
	}

	if (srcArray.Num() == 0 && dstArray.Num() == 0) {
		return	E_FAIL;
	}

	return	NOERROR;
}

STDMETHODIMP ShellExt::QueryInterface(REFIID riid, void **ppv)
{
	*ppv = NULL;

	if (IsEqualIID(riid, IID_IShellExtInit) || IsEqualIID(riid, IID_IUnknown)) {
		*ppv = (IShellExtInit *)this;
	}
	else if (IsEqualIID(riid, IID_IContextMenu)) {
		*ppv = (IContextMenu *)this;
	}

	if (*ppv) {
		AddRef();
		return NOERROR;
	}

	return E_NOINTERFACE;
}

BOOL ShellExt::GetClipBoardInfo(PathArray *pathArray, BOOL *is_cut)
{
	if (::OpenClipboard(NULL) == FALSE)
		return FALSE;

	HDROP	hDrop = (HDROP)::GetClipboardData(CF_HDROP);
	BOOL	ret = FALSE;
	int		max = 0;

	if (hDrop && (max = DragQueryFileV(hDrop, 0xffffffff, NULL, 0)) > 0) {
		if (pathArray) {
			for (int i=0; i < max; i++) {
				WCHAR	path[MAX_PATH_EX];
				DragQueryFileV(hDrop, i, path, sizeof(path) / CHAR_LEN_V);
				pathArray->RegisterPath(path);
			}
		}

		UINT uDropEffect = ::RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);

		if (::IsClipboardFormatAvailable(uDropEffect)) {
			HANDLE	hData = ::GetClipboardData(uDropEffect);
			DWORD	*flg;

			if (hData && (flg = (DWORD *)::GlobalLock(hData))) {
				*is_cut = (*flg == DROPEFFECT_MOVE) ? TRUE : FALSE;
				::GlobalUnlock(hData);
				ret = TRUE;
			}
		}
	}
	::CloseClipboard();
	return	ret;
}

BOOL ShellExt::IsDir(void *path, BOOL is_resolve)
{
	WCHAR	wbuf[MAX_PATH_EX];

	if (is_resolve && ReadLinkV(path, wbuf)) {
		path = wbuf;
	}

	DWORD	attr = GetFileAttributesV(path);

	return	(attr != 0xffffffff && (attr & FILE_ATTRIBUTE_DIRECTORY)) ? TRUE : FALSE;
}

STDMETHODIMP ShellExt::QueryContextMenu(HMENU hMenu, UINT iMenu, UINT cmdFirst, UINT cmdLast,
	UINT flg)
{
	HMENU	hTargetMenu = hMenu;
	BOOL	is_dd = dstArray.Num() && srcArray.Num();
	int		menu_flags = GetMenuFlags();
	int		mask_menu_flags = (menu_flags & (is_dd ? SHEXT_DD_MENUS : SHEXT_RIGHT_MENUS));
	BOOL	is_submenu = (menu_flags & (is_dd ? SHEXT_SUBMENU_DD : SHEXT_SUBMENU_RIGHT));
	BOOL	is_separator = (menu_flags & SHEXT_SUBMENU_NOSEP) ? FALSE : TRUE;

	if (!is_dd && (flg == CMF_NORMAL) || (flg & (CMF_VERBSONLY|CMF_DEFAULTONLY))) {
		return	ResultFromScode(MAKE_SCODE(SEVERITY_SUCCESS, 0, 0));
	}

	if (SysObj->DllRefCnt >= 2 && hMenu == SysObj->lastMenu) {
		DbgLogW(L" skip cnt=%d self=%x menu=%x/%x\n",
			SysObj->DllRefCnt, hMenu, this, SysObj->lastMenu);
		return	ResultFromScode(MAKE_SCODE(SEVERITY_SUCCESS, 0, 0));
	}
	DbgLogW(L" add cnt=%d self=%x menu=%x/%x\n",
		SysObj->DllRefCnt, hMenu, this, SysObj->lastMenu);

	isCut = FALSE;

	if (!is_dd && mask_menu_flags) {
		if (srcArray.Num() == 1 && dstArray.Num() == 0
		||  srcArray.Num() == 0 && dstArray.Num() == 1) {	// check paste mode
			PathArray	*target = srcArray.Num() ? &srcArray : &dstArray;

			if (mask_menu_flags & SHEXT_RIGHT_PASTE) {
				if (!IsDir(target->Path(0), srcArray.Num() ? TRUE : FALSE)
				|| !GetClipBoardInfo(&clipArray, &isCut)) {
					mask_menu_flags &= ~SHEXT_RIGHT_PASTE;
				}
			}
			DbgLogW(L"flg=%x isCut=%d mask_menu_flags=%x src=%d dst=%d clip=%d path=%s\r\n",
				flg, isCut, mask_menu_flags, srcArray.Num(), dstArray.Num(), clipArray.Num(),
				target->Path(0));
		}
		else mask_menu_flags &= ~SHEXT_RIGHT_PASTE;
	}

	// メニューアイテムの追加
	if (mask_menu_flags && srcArray.Num() >= ((mask_menu_flags & SHEXT_RIGHT_PASTE) ? 0 : 1)) {
//		DbgLogW(L"flg=%x isCut=%d mask_menu_flags=%x src=%d dst=%d clip=%d\r\n", flg, isCut,
//			mask_menu_flags, srcArray.Num(), dstArray.Num(), clipArray.Num());
		if (!is_dd && is_separator)
			::InsertMenu(hMenu, iMenu++, MF_SEPARATOR|MF_BYPOSITION, 0, 0);

		if (is_separator)
			::InsertMenu(hMenu, iMenu, MF_SEPARATOR|MF_BYPOSITION, 0, 0);

		if (is_submenu) {
			hTargetMenu = ::CreatePopupMenu();
			::InsertMenu(hMenu, iMenu, MF_POPUP|MF_BYPOSITION, (LONG_PTR)hTargetMenu, FASTCOPY);
			iMenu = 0;
		}

		if (mask_menu_flags & SHEXT_RIGHT_PASTE) {
			::InsertMenu(hTargetMenu, iMenu++, MF_STRING|MF_BYPOSITION,
				cmdFirst + SHEXT_MENU_PASTE, GetLoadStr(IDS_RIGHTPASTE));
		}

		if ((mask_menu_flags & (SHEXT_RIGHT_COPY|SHEXT_DD_COPY)) && srcArray.Num() > 0) {
			::InsertMenu(hTargetMenu, iMenu++, MF_STRING|MF_BYPOSITION,
				cmdFirst + SHEXT_MENU_COPY,
				is_dd ? GetLoadStr(IDS_DDCOPY) : GetLoadStr(IDS_RIGHTCOPY));
		}

		if ((mask_menu_flags & (SHEXT_RIGHT_DELETE|SHEXT_DD_MOVE)) && srcArray.Num() > 0) {
			::InsertMenu(hTargetMenu, iMenu++, MF_STRING|MF_BYPOSITION,
				cmdFirst + SHEXT_MENU_DELETE,
				is_dd ? GetLoadStr(IDS_DDMOVE) : GetLoadStr(IDS_RIGHTDEL));
		}
		SysObj->lastMenu = hMenu;
		DbgLogW(L" added cnt=%d self=%x set menu=%x/%x\n",
			SysObj->DllRefCnt, this, hMenu, SysObj->lastMenu);
	}

	return	ResultFromScode(MAKE_SCODE(SEVERITY_SUCCESS, 0, SHEXT_MENU_MAX));
}

STDMETHODIMP ShellExt::InvokeCommand(CMINVOKECOMMANDINFO *info)
{
	int		cmd = LOWORD(info->lpVerb);

	if (cmd >= 0 && cmd <= 2 && srcArray.Num() >= 0) {
		HANDLE	hRead, hWrite;
		SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), 0, TRUE };

		SysObj->lastMenu   = 0;

		::CreatePipe(&hRead, &hWrite, &sa, 0);
		::DuplicateHandle(::GetCurrentProcess(), hWrite, ::GetCurrentProcess(), &hWrite,
			0, FALSE, DUPLICATE_CLOSE_SOURCE|DUPLICATE_SAME_ACCESS);

		STARTUPINFO			st_info;
		memset(&st_info, 0, sizeof(st_info));
		st_info.cb = sizeof(st_info);
		st_info.dwFlags = STARTF_USESTDHANDLES;
		st_info.hStdInput = hRead;
		st_info.hStdOutput = ::GetStdHandle(STD_OUTPUT_HANDLE);
		st_info.hStdError = ::GetStdHandle(STD_ERROR_HANDLE);

		BOOL	is_dd = dstArray.Num();
		char	arg[MAX_PATH_EX], *cmd_str = "";
		PROCESS_INFORMATION	pr_info;
		BOOL	isClip = cmd == 2;
		BOOL	is_del = FALSE;
		int		menu_flags = GetMenuFlags();

		if (isClip && isCut || cmd == 1 && is_dd) {
			cmd_str = "/cmd=move";
		}
		else if (cmd == 1 && !is_dd) {
			cmd_str = "/cmd=delete";
			is_del = TRUE;
		}

		wsprintf(arg, "\"%s\" %s %s=%x", SysObj->ExeName, cmd_str, SHELLEXT_OPT, menu_flags);

		DbgLog("CreateProcess(%s)\r\n", arg);

		BOOL ret = ::CreateProcess(SysObj->ExeName, arg, 0, 0, TRUE, CREATE_DEFAULT_ERROR_MODE,
					0, 0, &st_info, &pr_info);
		::CloseHandle(hRead);

		if (ret) {
			PathArray	&src = isClip ? clipArray : srcArray;
			PathArray	&dst = (isClip && dstArray.Num() == 0) ? srcArray : dstArray;
			DWORD		len = src.GetMultiPathLen();
			WCHAR		*buf = new WCHAR[max(len, MAX_PATH_EX)];
			// dstArray が無い場合は、\0 まで出力
			len = src.GetMultiPath(buf, len) + (!is_dd && !isClip ? 1 : 0);

			DbgLogW(L"send fastcopy src=%s\r\n", buf);

			::WriteFile(hWrite, buf, len * CHAR_LEN_V, &len, 0);

			if (is_dd || isClip) {
				WCHAR	dir[MAX_PATH_EX];
				WCHAR	path[MAX_PATH_EX];
				void	*dstPath = (isClip && ReadLinkV(dst.Path(0), path)) ? path : dst.Path(0);

				MakePathV(dir, dstPath, EMPTY_STR_V);	// 末尾に \\ を付与
				len = sprintfV(buf, FMT_TOSTR_V, dir) + 1;
				DbgLogW(L"send fastcopy dst=%s\r\n", buf);
				::WriteFile(hWrite, buf, len * CHAR_LEN_V, &len, 0);
			}
			delete [] buf;
			::CloseHandle(pr_info.hProcess);
			::CloseHandle(pr_info.hThread);

			if (isClip) {
				if (::OpenClipboard(NULL)) {
					::EmptyClipboard();
					::CloseClipboard();
				}
			}
		}
		::CloseHandle(hWrite);
		return NOERROR;
	}
	return	E_INVALIDARG;
}

STDMETHODIMP ShellExt::GetCommandString(UINT_PTR cmd, UINT flg, UINT *, char *name, UINT cchMax)
{
	BOOL	is_dd = dstArray.Num();

	if (flg == GCS_HELPTEXT) {
		switch (cmd) {
		case 0:
			strncpy(name, is_dd ? GetLoadStr(IDS_DDCOPY) : GetLoadStr(IDS_RIGHTCOPY), cchMax);
			return	S_OK;
		case 1:
			strncpy(name, is_dd ? GetLoadStr(IDS_DDMOVE) : GetLoadStr(IDS_RIGHTDEL), cchMax);
			return	S_OK;
		case 2:
			strncpy(name, GetLoadStr(IDS_RIGHTPASTE), cchMax);
			return	S_OK;
		}
	}
    return E_INVALIDARG;
}

STDMETHODIMP_(ULONG) ShellExt::AddRef()
{
	return ++refCnt;
}

STDMETHODIMP_(ULONG) ShellExt::Release()
{
	if (--refCnt)
		return refCnt;

	delete this;
	return	0;
}


/*=========================================================================
  クラス ： ShellExtClassFactory
  概  要 ： シェル拡張クラス
  説  明 ： 
  注  意 ： 
=========================================================================*/
ShellExtClassFactory::ShellExtClassFactory(void)
{
	refCnt = 0;
	if (SysObj)
		SysObj->DllRefCnt++;
}

ShellExtClassFactory::~ShellExtClassFactory()
{
	if (SysObj)
		SysObj->DllRefCnt--;
}

STDMETHODIMP ShellExtClassFactory::CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObj)
{
	*ppvObj = NULL;

	if (pUnkOuter)
		return CLASS_E_NOAGGREGATION;

	ShellExt *shellExt = new ShellExt;
	if (NULL == shellExt)
		return E_OUTOFMEMORY;

	return shellExt->QueryInterface(riid, ppvObj);
}

STDMETHODIMP ShellExtClassFactory::LockServer(BOOL fLock)
{
	return NOERROR;
}

STDMETHODIMP ShellExtClassFactory::QueryInterface(REFIID riid, void **ppv)
{
	*ppv = NULL;

	if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
		*ppv = (IClassFactory *)this;
		AddRef();
		return NOERROR;
	}
	return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) ShellExtClassFactory::AddRef()
{
	return ++refCnt;
}

STDMETHODIMP_(ULONG) ShellExtClassFactory::Release()
{
	if (--refCnt)
		return refCnt;
	delete this;
	return 0;
}

/*=========================================================================
  関  数 ： 
  概  要 ： DLL Export 関数群
  説  明 ： 
  注  意 ： 
=========================================================================*/
STDAPI DllCanUnloadNow(void)
{
	return SysObj && SysObj->DllRefCnt == 0 ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppvOut)
{
	*ppvOut = NULL;

	if (IsEqualIID(rclsid, CURRENT_SHEXT_CLSID) || IsEqualIID(rclsid, CURRENT_SHEXTLNK_CLSID)) {
		ShellExtClassFactory	*cf = new ShellExtClassFactory;
		return cf->QueryInterface(riid, ppvOut);
	}
	return CLASS_E_CLASSNOTAVAILABLE;
}

BOOL GetClsId(REFIID cls_name, char *cls_id, int size)
{
	WCHAR		*w_cls_id;
	IMalloc		*im = NULL;

	::StringFromIID(cls_name, &w_cls_id);

	if (w_cls_id)
		WtoA(w_cls_id, cls_id, size);

	::CoGetMalloc(1, &im);
	im->Free(w_cls_id);
	im->Release();
	return	TRUE;
}

STDAPI DllRegisterServer(void)
{
	if (SHGetPathFromIDListV == NULL)
		return	E_FAIL;

	TShellExtRegistry	reg;

// CLASSKEY 登録
	if (reg.CreateClsKey()) {
		reg.SetStr(NULL, FASTCOPY);
		if (reg.CreateKey("InProcServer32")) {
			reg.SetStr(NULL, SysObj->DllName);
			reg.SetStr("ThreadingModel", "Apartment");
			reg.CloseKey();
		}
	}

// 関連付け
	reg.ChangeTopKey(HKEY_CLASSES_ROOT);
	for (int i=0; DllRegKeys[i]; i++) {
		if (reg.CreateKey(DllRegKeys[i])) {
			if (reg.CreateKey(FASTCOPY)) {
				reg.SetStr(NULL, reg.clsId);
				reg.CloseKey();
			}
			reg.CloseKey();
		}
	}

// NT系の追加
	if (IsWinNT())  {
		reg.ChangeTopKey(HKEY_LOCAL_MACHINE);
		if (reg.OpenKey(REG_SHELL_APPROVED)) {
			reg.SetStr(reg.clsId, FASTCOPY);
			reg.CloseKey();
		}
	}
	return S_OK;
}

STDAPI DllUnregisterServer(void)
{
	TShellExtRegistry	reg;

// CLASS_KEY 削除
	reg.DeleteChildTree(reg.clsId);

// 関連付け 削除
	reg.ChangeTopKey(HKEY_CLASSES_ROOT);
	for (int i=0; DllRegKeys[i]; i++) {
		if (reg.OpenKey(DllRegKeys[i])) {
			reg.DeleteChildTree(FASTCOPY);
			reg.CloseKey();
		}
	}

// 旧バージョン用 (.lnk 専用)
	TShellExtRegistry	linkreg(CURRENT_SHEXTLNK_CLSID);
	linkreg.DeleteChildTree(linkreg.clsId);

// NT系の追加
	if (IsWinNT())  {
		reg.ChangeTopKey(HKEY_LOCAL_MACHINE);
		if (reg.OpenKey(REG_SHELL_APPROVED)) {
			reg.DeleteValue(reg.clsId);
			reg.DeleteValue(linkreg.clsId);	// 旧バージョン用 (.lnk 専用)
			reg.CloseKey();
		}
	}

	return S_OK;
}

/*=========================================================================
  関  数 ： FastCopy 用 export 関数
  概  要 ： 
  説  明 ： 
  注  意 ： 
=========================================================================*/
BOOL WINAPI SetMenuFlags(int flags)
{
	TRegistry	reg(HKEY_CLASSES_ROOT);

	BOOL	ret = FALSE;

	if (reg.OpenKey(SysObj->MenuFlgRegKey)) {
		ret = reg.SetInt((flags & SHEXT_MENUFLG_EX) ? MENU_FLAGS : MENU_FLAGS_OLD, flags);
	}

	return	ret;
}

int WINAPI GetMenuFlagsCore(BOOL *is_present)
{
	TRegistry	reg(HKEY_CLASSES_ROOT);
	int			val = SHEXT_MENU_DEFAULT;
	BOOL		_is_present;

	if (!is_present) is_present = &_is_present;

	*is_present = FALSE;

	if (reg.OpenKey(SysObj->MenuFlgRegKey)) {
		if (reg.GetInt(MENU_FLAGS, &val)) {
			*is_present = TRUE;
		}
		else if (reg.GetInt(MENU_FLAGS_OLD, &val)) {
			val |= SHEXT_MENU_NEWDEF;
			*is_present = TRUE;
		}
	}

	return	val | SHEXT_MENUFLG_EX;
}

int WINAPI GetMenuFlags(void)
{
	return	GetMenuFlagsCore(NULL);
}

BOOL WINAPI UpdateDll(void)
{
	BOOL	is_present;
	int		val = GetMenuFlagsCore(&is_present);

	const GUID *oldiids[] = {
		&CLSID_ShellExtID1, // ID1 には lnkid はない
		&CLSID_ShellExtID2, &CLSID_ShellExtLinkID2,
		&CLSID_ShellExtID3, &CLSID_ShellExtLinkID3,
		&CLSID_ShellExtID4, &CLSID_ShellExtLinkID4,
		NULL };

	for (int i=0; oldiids[i]; i++) {
		TShellExtRegistry	oldReg(*oldiids[i]);
		oldReg.DeleteChildTree(oldReg.clsId);
	}

	if (is_present) {
		DllRegisterServer();
		SetMenuFlags(val);
	}
	return	TRUE;
}

BOOL WINAPI IsRegistServer(void)
{
	return	TShellExtRegistry().OpenClsKey();
}

/*=========================================================================
	PathArray
=========================================================================*/
PathArray::PathArray(void)
{
	totalLen = num = 0;
	pathArray = NULL;
}

PathArray::~PathArray()
{
	Init();
}

void PathArray::Init(void)
{
	if (pathArray) {
		while (--num >= 0)
			free(pathArray[num]);
		free(pathArray);
		pathArray = NULL;
		totalLen = num = 0;
	}
}

int PathArray::GetMultiPath(void *multi_path, int max_len)
{
	int		total_len = 0;
	void	*FMT_STR	= IS_WINNT_V ? (void *)L"%s\"%s\"" : (void *)"%s\"%s\"";
	void	*SPACE_STR	= IS_WINNT_V ? (void *)L" " : (void *)" ";

	for (int i=0; i < num; i++) {
		if (total_len + strlenV(pathArray[i]) + 4 >= max_len)
			break;
		total_len += sprintfV((char *)multi_path + total_len * CHAR_LEN_V, FMT_STR,
					i ? SPACE_STR : "\0", pathArray[i]);
	}
	return	total_len;
}

int PathArray::GetMultiPathLen(void)
{
	return	totalLen + 3 * num + 10;
}

BOOL PathArray::RegisterPath(const void *path)
{
#define MAX_ALLOC	100
	if ((num % MAX_ALLOC) == 0)
		pathArray = (void **)realloc(pathArray, (num + MAX_ALLOC) * sizeof(void *));

	int		len = strlenV(path) + 1;
	pathArray[num] = malloc(len * CHAR_LEN_V);
	memcpy(pathArray[num++], path, len * CHAR_LEN_V);
	totalLen += len;

	return	TRUE;
}

TShellExtRegistry::TShellExtRegistry(REFIID cls_name) : TRegistry(HKEY_CLASSES_ROOT)
{
	SetStrMode(BY_MBCS);
	GetClsId(cls_name, clsId, sizeof(clsId));
	OpenKey("CLSID");
}

DEFINE_GUID(IID_IShellLinkW, 0x000214F9, 0x0000, 0000, 0xC0, 0x00, 0x00, 0x00, \
	0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(IID_IShellLinkA, 0x000214EE, 0x0000, 0000, 0xC0, 0x00, 0x00, 0x00, \
	0x00, 0x00, 0x00, 0x46);

ShellExtSystem::ShellExtSystem(HINSTANCE hI)
{
#ifdef SHEXT_DEBUG_LOG
	::InitializeCriticalSection(&cs);
#endif
	TLibInit_Win32V();

	if (IS_WINNT_V) {
		FMT_TOSTR_V = L" /to=\"%s\"";
	}
	else {
		FMT_TOSTR_V = " /to=\"%s\"";
	}

/*	if (IS_WINNT_V && TIsWow64()) {
		DWORD	val = 0;
		TWow64DisableWow64FsRedirection(&val);
		TRegDisableReflectionKey(HKEY_CURRENT_USER);
		TRegDisableReflectionKey(HKEY_LOCAL_MACHINE);
		TRegDisableReflectionKey(HKEY_CLASSES_ROOT);
	}
*/
	HInstance = hI;
	DllRefCnt = 0;
	lastMenu  = 0;

// GetSystemDefaultLCID() に基づいたリソース文字列を事前にロードしておく
	LCID	curLcid = ::GetThreadLocale();
	LCID	newLcid = ::GetSystemDefaultLCID();

	if (curLcid != newLcid) {
		TSetThreadLocale(newLcid);
	}

	InitInstanceForLoadStr(hI);
	for (UINT id=IDS_RIGHTCOPY; id <= IDS_DDMOVE; id++) {
		GetLoadStr(id);
	}
	if (curLcid != newLcid) {
		TSetThreadLocale(curLcid);
	}

	char	path[MAX_PATH], buf[MAX_PATH], *fname = NULL;
	::GetModuleFileName(hI, buf, MAX_PATH);

	::GetFullPathName(buf, MAX_PATH, path, &fname);
/*	if (TIsWow64() && fname) {
		strcpy(fname, FCSHELLEX64_DLL);
	}
*/
	DllName = strdup(path);

	if (fname)	strcpy(fname, FASTCOPY_EXE);

	ExeName = strdup(path);

	wsprintf(path, "%s\\%s", DllRegKeys[0], FASTCOPY);
	MenuFlgRegKey = strdup(path);
}

ShellExtSystem::~ShellExtSystem()
{
#ifdef SHEXT_DEBUG_LOG
	::DeleteCriticalSection(&cs);
#endif
}

/*=========================================================================
  関  数 ： DllMain
  概  要 ： 
  説  明 ： 
  注  意 ： 
=========================================================================*/
int APIENTRY DllMain(HINSTANCE hI, DWORD reason, PVOID)
{
	switch (reason) {
	case DLL_PROCESS_ATTACH:
		if (SysObj == NULL)
			SysObj = new ShellExtSystem(hI);
		DbgLog("DLL_PROCESS_ATTACH\r\n");
		return	TRUE;

	case DLL_THREAD_ATTACH:
		DbgLog("DLL_THREAD_ATTACH\r\n");
		return	TRUE;

	case DLL_THREAD_DETACH:
		DbgLog("DLL_THREAD_DETACH\r\n");
		return	TRUE;

	case DLL_PROCESS_DETACH:
		if (SysObj) {
			delete [] SysObj->ExeName;
			delete SysObj;
			SysObj = NULL;
		}
		DbgLog("DLL_PROCESS_DETACH\r\n");
		return	TRUE;
	}
	return	TRUE;
}


