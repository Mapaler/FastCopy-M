static char *install_id = 
	"@(#)Copyright (C) 2005-2012 H.Shirouzu		install.cpp	Ver2.11";
/* ========================================================================
	Project  Name			: Installer for FastCopy
	Module Name				: Installer Application Class
	Create					: 2005-02-02(Wed)
	Update					: 2012-06-18(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "../tlib/tlib.h"
#include "stdio.h"
#include "instrc.h"
#include "install.h"

char *current_shell = TIsWow64() ? CURRENT_SHEXTDLL_EX : CURRENT_SHEXTDLL;
char *SetupFiles [] = {
	FASTCOPY_EXE, INSTALL_EXE, CURRENT_SHEXTDLL, CURRENT_SHEXTDLL_EX, README_TXT, HELP_CHM, NULL
};

/*
	Vista以降
*/
#ifdef _WIN64
BOOL ConvertToX86Dir(void *target)
{
	WCHAR	buf[MAX_PATH];
	WCHAR	buf86[MAX_PATH];
	int		len;

	if (!TSHGetSpecialFolderPathV(NULL, buf, CSIDL_PROGRAM_FILES, FALSE)) return FALSE;
	len = strlenV(buf);
	SetChar(buf, len++, '\\');
	SetChar(buf, len, 0);

	if (strnicmpV(buf, target, len)) return FALSE;

	if (!TSHGetSpecialFolderPathV(NULL, buf86, CSIDL_PROGRAM_FILESX86, FALSE)) return FALSE;
	MakePathV(buf, buf86, MakeAddr(target, len));
	strcpyV(target, buf);

	return	 TRUE;
}
#endif

BOOL ConvertVirtualStoreConf(void *execDirV, void *userDirV, void *virtualDirV)
{
#define FASTCOPY_INI_W			L"FastCopy.ini"
	WCHAR	buf[MAX_PATH];
	WCHAR	org_ini[MAX_PATH], usr_ini[MAX_PATH], vs_ini[MAX_PATH];
	BOOL	is_admin = TIsUserAnAdmin();
	BOOL	is_exists;

	MakePathV(usr_ini, userDirV, FASTCOPY_INI_W);
	MakePathV(org_ini, execDirV, FASTCOPY_INI_W);

#ifdef _WIN64
	ConvertToX86Dir(org_ini);
#endif

	is_exists = GetFileAttributesV(usr_ini) != 0xffffffff;
	 if (!is_exists) {
		CreateDirectoryV(userDirV, NULL);
	}

	if (virtualDirV && GetChar(virtualDirV, 0)) {
		MakePathV(vs_ini,  virtualDirV, FASTCOPY_INI_W);
		if (GetFileAttributesV(vs_ini) != 0xffffffff) {
			if (!is_exists) {
				is_exists = ::CopyFileW(vs_ini, usr_ini, TRUE);
			}
			MakePathV(buf, userDirV, L"to_OldDir(VirtualStore).lnk");
			SymLinkV(virtualDirV, buf);
			sprintfV(buf, L"%s.obsolete", vs_ini);
			MoveFileW(vs_ini, buf);
			if (GetFileAttributesV(vs_ini) != 0xffffffff) {
				DeleteFileV(vs_ini);
			}
		}
	}

	if ((is_admin || !is_exists) && GetFileAttributesV(org_ini) != 0xffffffff) {
		if (!is_exists) {
			is_exists = ::CopyFileW(org_ini, usr_ini, TRUE);
		}
		if (is_admin) {
			sprintfV(buf, L"%s.obsolete", org_ini);
			MoveFileW(org_ini, buf);
			if (GetFileAttributesV(org_ini) != 0xffffffff) {
				DeleteFileV(org_ini);
			}
		}
	}

	MakePathV(buf, userDirV, L"to_ExeDir.lnk");
//	if (GetFileAttributesV(buf) == 0xffffffff) {
		SymLinkV(execDirV, buf);
//	}

	return	TRUE;
}

/*
	WinMain
*/
int WINAPI WinMain(HINSTANCE hI, HINSTANCE, LPSTR cmdLine, int nCmdShow)
{
	return	TInstApp(hI, cmdLine, nCmdShow).Run();
}

/*
	インストールアプリケーションクラス
*/
TInstApp::TInstApp(HINSTANCE _hI, LPSTR _cmdLine, int _nCmdShow) : TApp(_hI, _cmdLine, _nCmdShow)
{
}

TInstApp::~TInstApp()
{
}

void TInstApp::InitWindow(void)
{
/*	if (IsWinNT() && TIsWow64()) {
		DWORD	val = 0;
		TWow64DisableWow64FsRedirection(&val);
	}
*/
	TDlg *maindlg = new TInstDlg(cmdLine);
	mainWnd = maindlg;
	maindlg->Create();
}


/*
	メインダイアログクラス
*/
TInstDlg::TInstDlg(char *cmdLine) : TDlg(INSTALL_DIALOG), staticText(this)
{
	cfg.mode = strcmp(cmdLine, "/r") ? SETUP_MODE : UNINSTALL_MODE;
	cfg.programLink	= TRUE;
	cfg.desktopLink	= TRUE;

	cfg.hOrgWnd   = NULL;
	cfg.runImme   = FALSE;
	cfg.appData   = NULL;
	cfg.setupDir  = NULL;
	cfg.startMenu = NULL;
	cfg.deskTop   = NULL;
	cfg.virtualDir= NULL;

	if (IS_WINNT_V && TIsUserAnAdmin()) {
		WCHAR	*p = wcsstr((WCHAR *)GetCommandLineV(), L"/runas=");

		if (p) {
			if (p && (p = wcstok(p+7,  L",")))  cfg.hOrgWnd		= (HWND)wcstoul(p, 0, 16);
			if (p && (p = wcstok(NULL, L",")))  cfg.mode		= (InstMode)wcstoul(p, 0, 10);
			if (p && (p = wcstok(NULL, L",")))  cfg.runImme		= wcstoul(p, 0, 10);
			if (p && (p = wcstok(NULL, L",")))  cfg.programLink	= wcstoul(p, 0, 10);
			if (p && (p = wcstok(NULL, L",")))  cfg.desktopLink	= wcstoul(p, 0, 10);
			if (p && (p = wcstok(NULL, L"\""))) cfg.startMenu = p; if (p) p = wcstok(NULL, L"\"");
			if (p && (p = wcstok(NULL, L"\""))) cfg.deskTop   = p; if (p) p = wcstok(NULL, L"\"");
			if (p && (p = wcstok(NULL, L"\""))) cfg.setupDir  = p; if (p) p = wcstok(NULL, L"\"");
			if (p && (p = wcstok(NULL, L"\""))) cfg.appData   = p; if (p) p = wcstok(NULL, L"\"");
			if (p && (p = wcstok(NULL, L"\""))) cfg.virtualDir= p;
			if (p) {
				::PostMessage(cfg.hOrgWnd, WM_CLOSE, 0, 0);
			}
			else {
				cfg.runImme = FALSE;
				::PostQuitMessage(0);
			}
		}
	}
}

TInstDlg::~TInstDlg()
{
}

/*
	親ディレクトリ取得（必ずフルパスであること。UNC対応）
*/
BOOL GetParentDir(const char *srcfile, char *dir)
{
	char	path[MAX_PATH], *fname=NULL;

	if (GetFullPathName(srcfile, MAX_PATH, path, &fname) == 0 || fname == NULL)
		return	strcpy(dir, srcfile), FALSE;

	if (fname - path > 3 || path[1] != ':') fname--;
	*fname = 0;

	strcpy(dir, path);
	return	TRUE;
}

BOOL GetShortcutPath(InstallCfg *cfg)
{
// スタートメニュー＆デスクトップに登録
	TRegistry	reg(HKEY_CURRENT_USER, BY_MBCS);
	if (reg.OpenKey(REGSTR_SHELLFOLDERS)) {
		char	buf[MAX_PATH] = "";
		reg.GetStr(REGSTR_PROGRAMS, buf, MAX_PATH);
		cfg->startMenu = toV(buf, TRUE);
		reg.GetStr(REGSTR_DESKTOP,  buf, MAX_PATH);
		cfg->deskTop   = toV(buf, TRUE);
		reg.CloseKey();
		return	TRUE;
	}
	return	FALSE;
}

/*
	メインダイアログ用 WM_INITDIALOG 処理ルーチン
*/
BOOL TInstDlg::EvCreate(LPARAM lParam)
{
	GetWindowRect(&rect);
	int		cx = ::GetSystemMetrics(SM_CXFULLSCREEN), cy = ::GetSystemMetrics(SM_CYFULLSCREEN);
	int		xsize = rect.right - rect.left, ysize = rect.bottom - rect.top;

	::SetClassLong(hWnd, GCL_HICON,
					(LONG_PTR)::LoadIcon(TApp::GetInstance(), (LPCSTR)SETUP_ICON));
	MoveWindow((cx - xsize)/2, (cy - ysize)/2, xsize, ysize, TRUE);
	Show();

// プロパティシートの生成
	staticText.AttachWnd(GetDlgItem(INSTALL_STATIC));
	propertySheet = new TInstSheet(this, &cfg);

// 現在ディレクトリ設定
	char		buf[MAX_PATH], setupDir[MAX_PATH];
	TRegistry	reg(HKEY_LOCAL_MACHINE, BY_MBCS);

// タイトル設定
	if (IsWinVista() && TIsUserAnAdmin()) {
		GetWindowText(buf, sizeof(buf));
		strcat(buf, " (Admin)");
		SetWindowText(buf);
	}

// Program Filesのパス取り出し
	if (reg.OpenKey(REGSTR_PATH_SETUP)) {
		if (reg.GetStr(REGSTR_PROGRAMFILES, buf, sizeof(buf)))
			MakePath(setupDir, buf, FASTCOPY);
		reg.CloseKey();
	}

// 既にセットアップされている場合は、セットアップディレクトリを読み出す
	if (reg.OpenKey(REGSTR_PATH_UNINSTALL)) {
		if (reg.OpenKey(FASTCOPY)) {
			if (reg.GetStr(REGSTR_VAL_UNINSTALLER_COMMANDLINE, setupDir, sizeof(setupDir))) {
				GetParentDir(setupDir, setupDir);
			}
			reg.CloseKey();
		}
		reg.CloseKey();
	}

	if (!cfg.startMenu || !cfg.deskTop) {
		GetShortcutPath(&cfg);
	}

	SetDlgItemText(FILE_EDIT, cfg.setupDir ? toA(cfg.setupDir) : setupDir);

	CheckDlgButton(cfg.mode == SETUP_MODE ? SETUP_RADIO : UNINSTALL_RADIO, 1);
	ChangeMode();

	if (cfg.runImme) PostMessage(WM_COMMAND, MAKEWORD(IDOK, 0), 0);

	return	TRUE;
}

/*
	メインダイアログ用 WM_COMMAND 処理ルーチン
*/
BOOL TInstDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID) {
	case IDOK:
		propertySheet->GetData();
		if (cfg.mode == UNINSTALL_MODE)
			UnInstall();
		else
			Install();
		return	TRUE;

	case IDCANCEL:
		::PostQuitMessage(0);
		return	TRUE;

	case FILE_BUTTON:
		BrowseDirDlg(this, FILE_EDIT, "Select Install Directory");
		return	TRUE;

	case SETUP_RADIO:
	case UNINSTALL_RADIO:
		if (wNotifyCode == BN_CLICKED)
			ChangeMode();
		return	TRUE;
	}
	return	FALSE;
}

void TInstDlg::ChangeMode(void)
{
	cfg.mode = IsDlgButtonChecked(SETUP_RADIO) ? SETUP_MODE : UNINSTALL_MODE;
	::EnableWindow(GetDlgItem(FILE_EDIT), cfg.mode == SETUP_MODE);
	propertySheet->Paste();
}

BOOL IsSameFile(char *src, char *dst)
{
	WIN32_FIND_DATA	src_dat, dst_dat;
	HANDLE	hFind;

	if ((hFind = ::FindFirstFile(src, &src_dat)) == INVALID_HANDLE_VALUE)
		return	FALSE;
	::FindClose(hFind);

	if ((hFind = ::FindFirstFile(dst, &dst_dat)) == INVALID_HANDLE_VALUE)
		return	FALSE;
	::FindClose(hFind);

	if (src_dat.nFileSizeLow != dst_dat.nFileSizeLow
	||  src_dat.nFileSizeHigh != dst_dat.nFileSizeHigh)
		return	FALSE;

	return
		(*(_int64 *)&dst_dat.ftLastWriteTime == *(_int64 *)&src_dat.ftLastWriteTime)
	||  ( (*(_int64 *)&src_dat.ftLastWriteTime % 10000000) == 0 ||
		  (*(_int64 *)&dst_dat.ftLastWriteTime % 10000000) == 0 )
	 &&	*(_int64 *)&dst_dat.ftLastWriteTime + 20000000 >= *(_int64 *)&src_dat.ftLastWriteTime
	 &&	*(_int64 *)&dst_dat.ftLastWriteTime - 20000000 <= *(_int64 *)&src_dat.ftLastWriteTime;
}

BOOL MiniCopy(char *src, char *dst)
{
	HANDLE		hSrc, hDst, hMap;
	BOOL		ret = FALSE;
	DWORD		srcSize, dstSize;
	void		*view;
	FILETIME	ct, la, ft;

	if ((hSrc = ::CreateFile(src, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0))
				== INVALID_HANDLE_VALUE)
		return	FALSE;
	srcSize = ::GetFileSize(hSrc, 0);

	if ((hDst = ::CreateFile(dst, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0))
				!= INVALID_HANDLE_VALUE) {
		if ((hMap = ::CreateFileMapping(hSrc, 0, PAGE_READONLY, 0, 0, 0)) != NULL) {
			if ((view = ::MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0)) != NULL) {
				if (::WriteFile(hDst, view, srcSize, &dstSize, 0) && srcSize == dstSize) {
					if (::GetFileTime(hSrc, &ct, &la, &ft))
						::SetFileTime(hDst, &ct, &la, &ft);
					ret = TRUE;
				}
				::UnmapViewOfFile(view);
			}
			::CloseHandle(hMap);
		}
		::CloseHandle(hDst);
	}
	::CloseHandle(hSrc);

	return	ret;
}

BOOL DelayCopy(char *src, char *dst)
{
	char	tmp_file[MAX_PATH];
	BOOL	ret = FALSE;

	wsprintf(tmp_file, "%s.new", dst);

	if (MiniCopy(src, tmp_file) == FALSE)
		return	FALSE;

	if (IsWinNT()) {
		ret = ::MoveFileEx(tmp_file, dst, MOVEFILE_DELAY_UNTIL_REBOOT|MOVEFILE_REPLACE_EXISTING);
	}
	else {
		char	win_ini[MAX_PATH], short_tmp[MAX_PATH], short_dst[MAX_PATH];

		::GetShortPathName(tmp_file, short_tmp, sizeof(short_tmp));
		::GetShortPathName(dst, short_dst, sizeof(short_dst));
		::GetWindowsDirectory(win_ini, sizeof(win_ini));
		strcat(win_ini, "\\WININIT.INI");
		// WritePrivateProfileString("Rename", "NUL", short_dst, win_ini); 必要なさそ
		ret = WritePrivateProfileString("Rename", short_dst, short_tmp, win_ini);
	}
	return	ret;
}

BOOL TInstDlg::RunAsAdmin(BOOL is_imme)
{
	SHELLEXECUTEINFOW	sei = {0};
	WCHAR				buf[MAX_PATH * 2];
	WCHAR				exe_path[MAX_PATH];
	WCHAR				setup_path[MAX_PATH];
	WCHAR				app_data[MAX_PATH];
	WCHAR				virtual_store[MAX_PATH];
	WCHAR				fastcopy_dir[MAX_PATH];
	WCHAR				*fastcopy_dirname = NULL;
	int					len;

	::GetModuleFileNameW(NULL, exe_path, sizeof(exe_path));

	if (!(len = GetDlgItemTextV(FILE_EDIT, setup_path, MAX_PATH))) return FALSE;
	if (GetChar(setup_path, len-1) == '\\') SetChar(setup_path, len-1, 0);
	if (!GetFullPathNameW(setup_path, MAX_PATH, fastcopy_dir, &fastcopy_dirname)) return FALSE;

	if (!TSHGetSpecialFolderPathV(NULL, buf, CSIDL_APPDATA, FALSE)) return FALSE;
	MakePathV(app_data, buf, fastcopy_dirname);

	strcpyV(buf, fastcopy_dir);
#ifdef _WIN64
	ConvertToX86Dir(buf);
#endif
	if (!TMakeVirtualStorePathV(buf, virtual_store)) return FALSE;

	sprintfV(buf, L"/runas=%p,%d,%d,%d,%d,\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"",
		hWnd, cfg.mode, is_imme, cfg.programLink, cfg.desktopLink,
		cfg.startMenu, cfg.deskTop,
		setup_path, app_data, virtual_store);

	sei.cbSize = sizeof(SHELLEXECUTEINFO);
	sei.lpVerb = L"runas";
	sei.lpFile = exe_path;
	sei.lpDirectory = NULL;
	sei.nShow = SW_NORMAL;
	sei.lpParameters = buf;

	EnableWindow(FALSE);
	BOOL ret = ::ShellExecuteExW(&sei);
	EnableWindow(TRUE);

	return	ret;
}

BOOL TInstDlg::Install(void)
{
	char	buf[MAX_PATH], setupDir[MAX_PATH], setupPath[MAX_PATH];
	BOOL	is_delay_copy = FALSE;
	int		len;

// インストールパス設定
	len = GetDlgItemText(FILE_EDIT, setupDir, sizeof(setupDir));
	if (GetChar(setupDir, len-1) == '\\') SetChar(setupDir, len-1, 0);
	Wstr	w_setup(setupDir);

	if (IsWinVista() && TIsVirtualizedDirV(w_setup.Buf())) {
		if (!TIsUserAnAdmin()) {
			return	RunAsAdmin(TRUE);
		}
		else if (cfg.runImme && cfg.setupDir && lstrcmpiV(w_setup.Buf(), cfg.setupDir)) {
			return	MessageBox(GetLoadStr(IDS_ADMINCHANGE)), FALSE;
		}
	}

	CreateDirectory(setupDir, NULL);
	DWORD	attr = GetFileAttributes(setupDir);

	if (attr == 0xffffffff || (attr & FILE_ATTRIBUTE_DIRECTORY) == 0)
		return	MessageBox(GetLoadStr(IDS_NOTCREATEDIR)), FALSE;
	MakePath(setupPath, setupDir, FASTCOPY_EXE);

	if (MessageBox(GetLoadStr(IDS_START), INSTALL_STR, MB_OKCANCEL|MB_ICONINFORMATION) != IDOK)
		return	FALSE;

// ファイルコピー
	if (cfg.mode == SETUP_MODE) {
		char	installPath[MAX_PATH], orgDir[MAX_PATH];

		::GetModuleFileName(NULL, orgDir, sizeof(orgDir));
		GetParentDir(orgDir, orgDir);

		for (int cnt=0; SetupFiles[cnt] != NULL; cnt++) {
			MakePath(buf, orgDir, SetupFiles[cnt]);
			MakePath(installPath, setupDir, SetupFiles[cnt]);

			if (MiniCopy(buf, installPath) || IsSameFile(buf, installPath))
				continue;

			if ((strcmp(SetupFiles[cnt], CURRENT_SHEXTDLL_EX) == 0 ||
				 strcmp(SetupFiles[cnt], CURRENT_SHEXTDLL) == 0) && DelayCopy(buf, installPath)) {
				is_delay_copy = TRUE;
				continue;
			}
			return	MessageBox(installPath, GetLoadStr(IDS_NOTCREATEFILE)), FALSE;
		}
	}

// スタートメニュー＆デスクトップに登録
	char	*linkPath[]	= { toA(cfg.startMenu, TRUE), toA(cfg.deskTop, TRUE), NULL };
	BOOL	execFlg[]	= { cfg.programLink,          cfg.desktopLink,        NULL };

	for (int cnt=0; linkPath[cnt]; cnt++) {
		strcpy(buf, linkPath[cnt]);
		if (cnt != 0 || RemoveSameLink(linkPath[cnt], buf) == FALSE) {
			::wsprintf(buf + strlen(buf), "\\%s", FASTCOPY_SHORTCUT);
		}
		if (execFlg[cnt]) {
			if (IS_WINNT_V) {
				Wstr	w_setup(setupPath, BY_MBCS);
				Wstr	w_buf(buf, BY_MBCS);
				SymLinkV(w_setup.Buf(), w_buf.Buf());
			}
			else {
				SymLinkV(setupPath, buf);
			}
		}
		else {
			if (IS_WINNT_V) {
				Wstr	w_buf(buf, BY_MBCS);
				DeleteLinkV(w_buf.Buf());
			}
			else {
				DeleteLinkV(buf);
			}
		}
	}

#if 0
// レジストリにアンインストール情報を登録
	if (reg.OpenKey(REGSTR_PATH_UNINSTALL)) {
		if (reg.CreateKey(FASTCOPY)) {
			MakePath(buf, setupDir, INSTALL_EXE);
			strcat(buf, " /r");
			reg.SetStr(REGSTR_VAL_UNINSTALLER_DISPLAYNAME, FASTCOPY);
			reg.SetStr(REGSTR_VAL_UNINSTALLER_COMMANDLINE, buf);
			reg.CloseKey();
		}
		reg.CloseKey();
	}
#endif

	if (IsWinVista() && TIsVirtualizedDirV(w_setup.Buf())) {
		WCHAR	wbuf[MAX_PATH] = L"", old_path[MAX_PATH] = L"", usr_path[MAX_PATH] = L"";
		WCHAR	fastcopy_dir[MAX_PATH], *fastcopy_dirname = NULL;

		GetFullPathNameW(w_setup.Buf(), MAX_PATH, fastcopy_dir, &fastcopy_dirname);

		if (cfg.appData) {
			strcpyV(usr_path, cfg.appData);
		}
		else {
			TSHGetSpecialFolderPathV(NULL, wbuf, CSIDL_APPDATA, FALSE);
			MakePathV(usr_path, wbuf, fastcopy_dirname);
		}

		ConvertVirtualStoreConf(w_setup.Buf(), usr_path, cfg.virtualDir);
	}

// コピーしたアプリケーションを起動
	const char *msg = GetLoadStr(is_delay_copy ? IDS_DELAYSETUPCOMPLETE : IDS_SETUPCOMPLETE);
	int			flg = MB_OKCANCEL|MB_ICONINFORMATION;

	if (IsWin7()) {
		msg = Fmt("%s%s", msg, GetLoadStr(IDS_COMPLETE_WIN7));
	}

	TLaunchDlg	dlg(msg, this);
	UINT		id;

	if ((id = dlg.Exec()) == IDOK || id == LAUNCH_BUTTON) {
		char	*arg = (id == LAUNCH_BUTTON) ? "/install" : "";
		ShellExecute(NULL, "open", setupPath, arg, setupDir, SW_SHOW);
	}

	::PostQuitMessage(0);
	return	TRUE;
}

// シェル拡張を解除
enum ShellExtOpe { CHECK_SHELLEXT, UNREGISTER_SHELLEXT };

int ShellExtFunc(char *setup_dir, ShellExtOpe kind)
{
	char	buf[MAX_PATH];
	int		ret = FALSE;

	MakePath(buf, setup_dir, CURRENT_SHEXTDLL);
	HMODULE		hShellExtDll = TLoadLibrary(buf);

	if (hShellExtDll) {
		BOOL (WINAPI *IsRegisterDll)(void) = (BOOL (WINAPI *)(void))
			GetProcAddress(hShellExtDll, "IsRegistServer");
		HRESULT (WINAPI *UnRegisterDll)(void) = (HRESULT (WINAPI *)(void))
			GetProcAddress(hShellExtDll, "DllUnregisterServer");

		if (IsRegisterDll && UnRegisterDll) {
			switch (kind) {
			case CHECK_SHELLEXT:
				ret = IsRegisterDll();
				break;
			case UNREGISTER_SHELLEXT:
				ret = UnRegisterDll();
				break;
			}
		::FreeLibrary(hShellExtDll);
		}
	}
	return	ret;
}


BOOL TInstDlg::UnInstall(void)
{
	char	buf[MAX_PATH];
	char	setupDir[MAX_PATH] = "";

	::GetModuleFileName(NULL, setupDir, sizeof(setupDir));
	GetParentDir(setupDir, setupDir);
	BOOL	is_shext = FALSE;

	is_shext = ShellExtFunc(setupDir, CHECK_SHELLEXT);

	if (is_shext && IsWinVista() && !TIsUserAnAdmin()) {
		RunAsAdmin(TRUE);
		return	TRUE;
	}

	if (MessageBox(GetLoadStr(IDS_START), UNINSTALL_STR, MB_OKCANCEL|MB_ICONINFORMATION) != IDOK)
		return	FALSE;

// スタートメニュー＆デスクトップから削除
	TRegistry	reg(HKEY_CURRENT_USER, BY_MBCS);
	if (reg.OpenKey(REGSTR_SHELLFOLDERS)) {
		char	*regStr[]	= { REGSTR_PROGRAMS, REGSTR_DESKTOP, NULL };

		for (int cnt=0; regStr[cnt] != NULL; cnt++) {
			if (reg.GetStr(regStr[cnt], buf, sizeof(buf))) {
				if (cnt == 0)
					RemoveSameLink(buf);
				::wsprintf(buf + strlen(buf), "\\%s", FASTCOPY_SHORTCUT);
				if (IS_WINNT_V) {
					Wstr	w_buf(buf, BY_MBCS);
					DeleteLinkV(w_buf.Buf());
				}
				else {
					DeleteLinkV(buf);
				}
			}
		}
		reg.CloseKey();
	}

	ShellExtFunc(setupDir, UNREGISTER_SHELLEXT);

#ifdef _WIN64
	if (IS_WINNT_V) {
#else
	if (IS_WINNT_V && TIsWow64()) {
#endif
		SHELLEXECUTEINFO	sei = { sizeof(sei) };
		char	arg[1024];

		sprintf(arg, "\"%s\\%s\",%s", setupDir, CURRENT_SHEXTDLL_EX, "DllUnregisterServer");
		sei.lpFile = "rundll32.exe";
		sei.lpParameters = arg;
		ShellExecuteEx(&sei);
	}

// レジストリからアンインストール情報を削除
	if (reg.OpenKey(REGSTR_PATH_UNINSTALL)) {
		if (reg.OpenKey(FASTCOPY)) {
			if (reg.GetStr(REGSTR_VAL_UNINSTALLER_COMMANDLINE, setupDir, sizeof(setupDir)))
				GetParentDir(setupDir, setupDir);
			reg.CloseKey();
		}
		reg.DeleteKey(FASTCOPY);
		reg.CloseKey();
	}

// 終了メッセージ
	MessageBox(is_shext ? GetLoadStr(IDS_UNINSTSHEXTFIN) : GetLoadStr(IDS_UNINSTFIN));

// インストールディレクトリを開く
	if (GetFileAttributes(setupDir) != 0xffffffff) {
		::ShellExecute(NULL, NULL, setupDir, 0, 0, SW_SHOW);
	}

// AppDataディレクトリを開く
	if (IsWinVista()) {
		WCHAR	wbuf[MAX_PATH] = L"", upath[MAX_PATH] = L"";
		WCHAR	fastcopy_dir[MAX_PATH] = L"", *fastcopy_dirname = NULL;
		Wstr	w_setup(setupDir);

		if (TIsVirtualizedDirV(w_setup.Buf())) {
			if (TSHGetSpecialFolderPathV(NULL, wbuf, CSIDL_APPDATA, FALSE)) {
				GetFullPathNameW(w_setup.Buf(), MAX_PATH, fastcopy_dir, &fastcopy_dirname);
				MakePathV(upath, wbuf, fastcopy_dirname);

				if (GetFileAttributesV(upath) != 0xffffffff) {
					::ShellExecuteW(NULL, NULL, upath, 0, 0, SW_SHOW);
				}
			}
		}
	}

	::PostQuitMessage(0);
	return	TRUE;
}


BOOL ReadLink(char *src, char *dest, char *arg=NULL)
{
	IShellLink		*shellLink;		// 実際は IShellLinkA or IShellLinkW
	IPersistFile	*persistFile;
	WCHAR			wbuf[MAX_PATH];
	BOOL			ret = FALSE;

	if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink,
			(void **)&shellLink))) {
		if (SUCCEEDED(shellLink->QueryInterface(IID_IPersistFile, (void **)&persistFile))) {
			AtoW(src, wbuf, MAX_PATH);
			if (SUCCEEDED(persistFile->Load(wbuf, STGM_READ))) {
				if (SUCCEEDED(shellLink->GetPath(dest, MAX_PATH, NULL, 0))) {
					if (arg) {
						shellLink->GetArguments(arg, MAX_PATH);
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

/*
	同じ内容を持つショートカットを削除（スタートメニューへの重複登録よけ）
*/
BOOL TInstDlg::RemoveSameLink(const char *dir, char *remove_path)
{
	char			path[MAX_PATH], dest[MAX_PATH], arg[MAX_PATH];
	HANDLE			fh;
	WIN32_FIND_DATA	data;
	BOOL			ret = FALSE;

	::wsprintf(path, "%s\\*.*", dir);
	if ((fh = ::FindFirstFile(path, &data)) == INVALID_HANDLE_VALUE)
		return	FALSE;

	do {
		::wsprintf(path, "%s\\%s", dir, data.cFileName);
		if (ReadLink(path, dest, arg) && *arg == 0) {
			int		dest_len = (int)strlen(dest);
			int		fastcopy_len = (int)strlen(FASTCOPY_EXE);
			if (dest_len > fastcopy_len
			&& strncmpi(dest + dest_len - fastcopy_len, FASTCOPY_EXE, fastcopy_len) == 0) {
				ret = ::DeleteFile(path);
				if (remove_path != NULL)
					strcpy(remove_path, path);
			}
		}

	} while (::FindNextFile(fh, &data));

	::FindClose(fh);
	return	ret;
}

TInstSheet::TInstSheet(TWin *_parent, InstallCfg *_cfg) : TDlg(INSTALL_SHEET, _parent)
{
	cfg = _cfg;
}

void TInstSheet::GetData(void)
{
	if (resId == UNINSTALL_SHEET) {
	}
	else {
		cfg->programLink	= IsDlgButtonChecked(PROGRAM_CHECK);
		cfg->desktopLink	= IsDlgButtonChecked(DESKTOP_CHECK);
	}
}

void TInstSheet::PutData(void)
{
	if (resId == UNINSTALL_SHEET) {
	}
	else {
		CheckDlgButton(PROGRAM_CHECK, cfg->programLink);
		CheckDlgButton(DESKTOP_CHECK, cfg->desktopLink);
	}
}

void TInstSheet::Paste(void)
{
	if (hWnd) {
		if ((resId == UNINSTALL_SHEET) == (cfg->mode == UNINSTALL_MODE))
			return;
		GetData();
		Destroy();
	}
	resId = cfg->mode == UNINSTALL_MODE ? UNINSTALL_SHEET : INSTALL_SHEET;

	Create();
	PutData();
}

BOOL TInstSheet::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID)
	{
	case IDOK:	case IDCANCEL:
		{
			parent->EvCommand(wNotifyCode, wID, hwndCtl);
		}
		return	TRUE;
	}
	return	FALSE;
}

BOOL TInstSheet::EvCreate(LPARAM lParam)
{
	RECT	rc;
	POINT	pt;
	::GetWindowRect(parent->GetDlgItem(INSTALL_STATIC), &rc);
	pt.x = rc.left;
	pt.y = rc.top;
	ScreenToClient(parent->hWnd, &pt);
	SetWindowPos(0, pt.x, pt.y, 0, 0, SWP_NOSIZE|SWP_NOZORDER);

	SetWindowLong(GWL_EXSTYLE, GetWindowLong(GWL_EXSTYLE)|WS_EX_CONTROLPARENT);

	Show();
	return	TRUE;
}

/*
	ディレクトリダイアログ用汎用ルーチン
*/
void BrowseDirDlg(TWin *parentWin, UINT editCtl, char *title)
{ 
	IMalloc			*iMalloc = NULL;
	BROWSEINFO		brInfo;
	LPITEMIDLIST	pidlBrowse;
	char			fileBuf[MAX_PATH];

	parentWin->GetDlgItemText(editCtl, fileBuf, sizeof(fileBuf));
	if (!SUCCEEDED(SHGetMalloc(&iMalloc)))
		return;

	TBrowseDirDlg	dirDlg(fileBuf);
	brInfo.hwndOwner = parentWin->hWnd;
	brInfo.pidlRoot = 0;
	brInfo.pszDisplayName = fileBuf;
	brInfo.lpszTitle = title;
	brInfo.ulFlags = BIF_RETURNONLYFSDIRS;
	brInfo.lpfn = BrowseDirDlg_Proc;
	brInfo.lParam = (LPARAM)&dirDlg;
	brInfo.iImage = 0;

	do {
		if ((pidlBrowse = ::SHBrowseForFolder(&brInfo)) != NULL) {
			if (::SHGetPathFromIDList(pidlBrowse, fileBuf))
				::SetDlgItemText(parentWin->hWnd, editCtl, fileBuf);
			iMalloc->Free(pidlBrowse);
			break;
		}
	} while (dirDlg.IsDirty());

	iMalloc->Release();
}

/*
	BrowseDirDlg用コールバック
*/
int CALLBACK BrowseDirDlg_Proc(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM data)
{
	switch (uMsg) {
	case BFFM_INITIALIZED:
		((TBrowseDirDlg *)data)->AttachWnd(hWnd);
		break;

	case BFFM_SELCHANGED:
		if (((TBrowseDirDlg *)data)->hWnd)
			((TBrowseDirDlg *)data)->SetFileBuf(lParam);
		break;
	}
	return 0;
}

/*
	BrowseDlg用サブクラス生成
*/
BOOL TBrowseDirDlg::AttachWnd(HWND _hWnd)
{
	BOOL	ret = TSubClass::AttachWnd(_hWnd);
	dirtyFlg = FALSE;

// ディレクトリ設定
	DWORD	attr = GetFileAttributes(fileBuf);
	if (attr == 0xffffffff || (attr & FILE_ATTRIBUTE_DIRECTORY) == 0)
		GetParentDir(fileBuf, fileBuf);
	SendMessage(BFFM_SETSELECTION, TRUE, (LPARAM)fileBuf);
	SetWindowText(FASTCOPY);

// ボタン作成
	RECT	tmp_rect;
	::GetWindowRect(GetDlgItem(IDOK), &tmp_rect);
	POINT	pt = { tmp_rect.left, tmp_rect.top };
	::ScreenToClient(hWnd, &pt);
	int		cx = (pt.x - 30) / 2, cy = tmp_rect.bottom - tmp_rect.top;

	::CreateWindow(BUTTON_CLASS, GetLoadStr(IDS_MKDIR), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, 10,
		pt.y, cx, cy, hWnd, (HMENU)MKDIR_BUTTON, TApp::GetInstance(), NULL);
	::CreateWindow(BUTTON_CLASS, GetLoadStr(IDS_RMDIR), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
		18 + cx, pt.y, cx, cy, hWnd, (HMENU)RMDIR_BUTTON, TApp::GetInstance(), NULL);

	HFONT	hDlgFont = (HFONT)SendDlgItemMessage(IDOK, WM_GETFONT, 0, 0L);
	if (hDlgFont) {
		SendDlgItemMessage(MKDIR_BUTTON, WM_SETFONT, (UINT)hDlgFont, 0L);
		SendDlgItemMessage(RMDIR_BUTTON, WM_SETFONT, (UINT)hDlgFont, 0L);
	}

	return	ret;
}

/*
	BrowseDlg用 WM_COMMAND 処理
*/
BOOL TBrowseDirDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID) {
	case MKDIR_BUTTON:
		{
			char		dirBuf[MAX_PATH], path[MAX_PATH];
			TInputDlg	dlg(dirBuf, this);
			if (dlg.Exec() != IDOK)
				return	TRUE;

			MakePath(path, fileBuf, dirBuf);
			if (::CreateDirectory(path, NULL)) {
				strcpy(fileBuf, path);
				dirtyFlg = TRUE;
				PostMessage(WM_CLOSE, 0, 0);
			}
		}
		return	TRUE;

	case RMDIR_BUTTON:
		if (::RemoveDirectory(fileBuf)) {
			GetParentDir(fileBuf, fileBuf);
			dirtyFlg = TRUE;
			PostMessage(WM_CLOSE, 0, 0);
		}
		return	TRUE;
	}
	return	FALSE;
}

BOOL TBrowseDirDlg::SetFileBuf(LPARAM list)
{
	return	::SHGetPathFromIDList((LPITEMIDLIST)list, fileBuf);
}

/*
	一行入力
*/
BOOL TInputDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID) {
	case IDOK:
		GetDlgItemText(INPUT_EDIT, dirBuf, MAX_PATH);
		EndDialog(wID);
		return	TRUE;

	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;
	}
	return	FALSE;
}

/*
	ファイルの保存されているドライブ識別
*/
UINT GetDriveTypeEx(const char *file)
{
	if (file == NULL)
		return	GetDriveType(NULL);

	if (IsUncFile(file))
		return	DRIVE_REMOTE;

	char	buf[MAX_PATH];
	int		len = (int)strlen(file), len2;

	strcpy(buf, file);
	do {
		len2 = len;
		GetParentDir(buf, buf);
		len = (int)strlen(buf);
	} while (len != len2);

	return	GetDriveType(buf);
}

/*
	起動ダイアログ
*/
TLaunchDlg::TLaunchDlg(LPCSTR _msg, TWin *_win) : TDlg(LAUNCH_DIALOG, _win)
{
	msg = strdup(_msg);
}

TLaunchDlg::~TLaunchDlg()
{
	free(msg);
}

/*
	メインダイアログ用 WM_INITDIALOG 処理ルーチン
*/
BOOL TLaunchDlg::EvCreate(LPARAM lParam)
{
	SetDlgItemText(MESSAGE_STATIC, msg);
	if (IsWin7()) {
		::ShowWindow(GetDlgItem(LAUNCH_BUTTON), SW_SHOW);
	}

	Show();
	return	TRUE;
}

#define NOTIFY_SETTINGS	"shell32.dll,Options_RunDLL 5"

BOOL TLaunchDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID)
	{
	case IDOK: case IDCANCEL:
		EndDialog(wID);
		return	TRUE;

	case LAUNCH_BUTTON:
		//ShellExecuteU8(NULL, NULL, GetLoadStr(IDS_TRAYURL), 0, 0, SW_SHOW);
		ShellExecuteU8(NULL, "open", "rundll32.exe", NOTIFY_SETTINGS, 0, SW_SHOW);
		EndDialog(wID);
		return	TRUE;
	}
	return	FALSE;
}


