static char *install_id = 
	"@(#)Copyright (C) 2005-2016 H.Shirouzu		install.cpp	Ver3.20";
/* ========================================================================
	Project  Name			: Installer for FastCopy
	Module Name				: Installer Application Class
	Create					: 2005-02-02(Wed)
	Update					: 2016-09-28(Wed)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	======================================================================== */

#include "../tlib/tlib.h"
#include "stdio.h"
#include "instrc.h"
#include "install.h"

char *current_shell = TIsWow64() ? CURRENT_SHEXTDLL_EX : CURRENT_SHEXTDLL;
char *SetupFiles [] = {
	FASTCOPY_EXE, INSTALL_EXE, CURRENT_SHEXTDLL, CURRENT_SHEXTDLL_EX,
	README_TXT, README_ENG_TXT, GPL_TXT, XXHASH_TXT, HELP_CHM, NULL
};

/*
	Vista以降
*/
#ifdef _WIN64
BOOL ConvertToX86Dir(WCHAR *target)
{
	WCHAR	buf[MAX_PATH];
	WCHAR	buf86[MAX_PATH];
	ssize_t	len;

	if (!::SHGetSpecialFolderPathW(NULL, buf, CSIDL_PROGRAM_FILES, FALSE)) return FALSE;
	len = wcslen(buf);
	buf[len++] = '\\';
	buf[len]   = 0;

	if (wcsnicmp(buf, target, len)) return FALSE;

	if (!::SHGetSpecialFolderPathW(NULL, buf86, CSIDL_PROGRAM_FILESX86, FALSE)) return FALSE;
	MakePathW(buf, buf86, target + len);
	wcscpy(target, buf);

	return	 TRUE;
}
#endif

BOOL ConvertVirtualStoreConf(WCHAR *execDir, WCHAR *userDir, WCHAR *virtualDir)
{
#define FASTCOPY_INI_W			L"FastCopy.ini"
	WCHAR	buf[MAX_PATH];
	WCHAR	org_ini[MAX_PATH], usr_ini[MAX_PATH], vs_ini[MAX_PATH];
	BOOL	is_admin = ::IsUserAnAdmin();
	BOOL	is_exists;

	MakePathW(usr_ini, userDir, FASTCOPY_INI_W);
	MakePathW(org_ini, execDir, FASTCOPY_INI_W);

#ifdef _WIN64
	ConvertToX86Dir(org_ini);
#endif

	is_exists = ::GetFileAttributesW(usr_ini) != 0xffffffff;
	 if (!is_exists) {
		::CreateDirectoryW(userDir, NULL);
	}

	if (virtualDir && virtualDir[0]) {
		MakePathW(vs_ini,  virtualDir, FASTCOPY_INI_W);
		if (::GetFileAttributesW(vs_ini) != 0xffffffff) {
			if (!is_exists) {
				is_exists = ::CopyFileW(vs_ini, usr_ini, TRUE);
			}
			MakePathW(buf, userDir, L"to_OldDir(VirtualStore).lnk");
			SymLinkW(virtualDir, buf);
			swprintf(buf, L"%s.obsolete", vs_ini);
			::MoveFileW(vs_ini, buf);
			if (::GetFileAttributesW(vs_ini) != 0xffffffff) {
				::DeleteFileW(vs_ini);
			}
		}
	}

	if ((is_admin || !is_exists) && ::GetFileAttributesW(org_ini) != 0xffffffff) {
		if (!is_exists) {
			is_exists = ::CopyFileW(org_ini, usr_ini, TRUE);
		}
		if (is_admin) {
			swprintf(buf, L"%s.obsolete", org_ini);
			::MoveFileW(org_ini, buf);
			if (::GetFileAttributesW(org_ini) != 0xffffffff) {
				::DeleteFileW(org_ini);
			}
		}
	}

	MakePathW(buf, userDir, L"to_ExeDir.lnk");
//	if (GetFileAttributesW(buf) == 0xffffffff) {
		SymLinkW(execDir, buf);
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

	if (::IsUserAnAdmin()) {
		WCHAR	*p = wcsstr((WCHAR *)GetCommandLineW(), L"/runas=");

		if (p) {
			if (p && (p = wcstok(p+7,  L",")))  cfg.hOrgWnd	= (HWND)(LONG_PTR)wcstoull(p, 0, 16);
			if (p && (p = wcstok(NULL, L",")))  cfg.mode	= (InstMode)wcstoul(p, 0, 10);
			if (p && (p = wcstok(NULL, L",")))  cfg.runImme	= wcstoul(p, 0, 10);
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
		cfg->startMenu = AtoWs(buf);
		reg.GetStr(REGSTR_DESKTOP,  buf, MAX_PATH);
		cfg->deskTop   = AtoWs(buf);
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
	char	buf[MAX_PATH] = "";
	char	setupDir[MAX_PATH] = "";

// タイトル設定
	if (IsWinVista() && ::IsUserAnAdmin()) {
		GetWindowText(buf, sizeof(buf));
		strcat(buf, " (Admin)");
		SetWindowText(buf);
	}

// 既にセットアップされている場合は、セットアップディレクトリを読み出す
	if (!*setupDir) {
		TRegistry	reg(HSTOOLS_STR, NULL, BY_MBCS);
		if (reg.OpenKey(FASTCOPY)) {
			reg.GetStr("Path", setupDir, sizeof(setupDir));
			reg.CloseKey();
		}
	}

// Program Filesのパス取り出し
	if (!*setupDir) {
		TRegistry	reg(HKEY_LOCAL_MACHINE, BY_MBCS);
		if (reg.OpenKey(REGSTR_PATH_SETUP)) {
			if (reg.GetStr(REGSTR_PROGRAMFILES, buf, sizeof(buf)))
				MakePath(setupDir, buf, FASTCOPY);
			reg.CloseKey();
		}
	}

	if (!cfg.startMenu || !cfg.deskTop) {
		GetShortcutPath(&cfg);
	}

	SetDlgItemText(FILE_EDIT, cfg.setupDir ? WtoAs(cfg.setupDir) : setupDir);

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
		(*(int64 *)&dst_dat.ftLastWriteTime == *(int64 *)&src_dat.ftLastWriteTime)
	||  ( (*(int64 *)&src_dat.ftLastWriteTime % 10000000) == 0 ||
		  (*(int64 *)&dst_dat.ftLastWriteTime % 10000000) == 0 )
	 &&	*(int64 *)&dst_dat.ftLastWriteTime + 20000000 >= *(int64 *)&src_dat.ftLastWriteTime
	 &&	*(int64 *)&dst_dat.ftLastWriteTime - 20000000 <= *(int64 *)&src_dat.ftLastWriteTime;
}

BOOL RotateFile(char *path, int max_cnt, BOOL with_delete)
{
	BOOL	ret = TRUE;

	for (int i=max_cnt-1; i >= 0; i--) {
		char	src[MAX_PATH];
		char	dst[MAX_PATH];

		_snprintf(src, sizeof(src), (i == 0) ? "%s" : "%s.%d", path, i);
		_snprintf(dst, sizeof(dst), "%s.%d", path, i+1);

		if (::GetFileAttributes(src) == 0xffffffff) {
			continue;
		}
		if (::MoveFileEx(src, dst, MOVEFILE_REPLACE_EXISTING)) {
			if (with_delete) {
				::MoveFileEx(dst, NULL, MOVEFILE_DELAY_UNTIL_REBOOT); // require admin priv...
			}
		}
		else {
			ret = FALSE;
		}
	}
	return	ret;
}

BOOL MiniCopy(char *src, char *dst, BOOL *is_rotate=NULL)
{
	HANDLE		hSrc, hDst, hMap;
	BOOL		ret = FALSE;
	DWORD		srcSize, dstSize;
	void		*view;
	FILETIME	ct, la, ft;
	BOOL		isRotate = FALSE;

	if ((hSrc = ::CreateFile(src, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0))
				== INVALID_HANDLE_VALUE) {
		Debug("src(%s) open err(%x)\n", src, GetLastError());
		return	FALSE;
	}
	srcSize = ::GetFileSize(hSrc, 0);

	hDst = ::CreateFile(dst, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (hDst == INVALID_HANDLE_VALUE) {
		RotateFile(dst, 10, TRUE);
		if (is_rotate) {
			*is_rotate = TRUE;
		}
		hDst = ::CreateFile(dst, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	}

	if (hDst != INVALID_HANDLE_VALUE) {
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
	else {
		Debug("dst(%s) open err(%x)\n", dst, GetLastError());
		return	FALSE;
	}
	::CloseHandle(hSrc);

	return	ret;
}

BOOL DelayCopy(char *src, char *dst)
{
	char	tmp_file[MAX_PATH];
	BOOL	ret = FALSE;

	sprintf(tmp_file, "%s.new", dst);

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
	WCHAR				buf[MAX_PATH * 2] = {};
	WCHAR				exe_path[MAX_PATH] = {};
	WCHAR				setup_path[MAX_PATH] = {};
	WCHAR				app_data[MAX_PATH] = {};
	WCHAR				virtual_store[MAX_PATH] = {};
	WCHAR				fastcopy_dir[MAX_PATH] = {};
	WCHAR				*fastcopy_dirname = NULL;
	int					len;

	::GetModuleFileNameW(NULL, exe_path, sizeof(exe_path));

	if (!(len = GetDlgItemTextW(FILE_EDIT, setup_path, MAX_PATH))) return FALSE;
	if (setup_path[len-1] == '\\') setup_path[len-1] = 0;
	if (!::GetFullPathNameW(setup_path, MAX_PATH, fastcopy_dir, &fastcopy_dirname)) return FALSE;

	if (!::SHGetSpecialFolderPathW(NULL, buf, CSIDL_APPDATA, FALSE)) return FALSE;
	MakePathW(app_data, buf, fastcopy_dirname);

	wcscpy(buf, fastcopy_dir);
	if (cfg.mode == SETUP_MODE) {
#ifdef _WIN64
		ConvertToX86Dir(buf);
#endif
		if (!TMakeVirtualStorePathW(buf, virtual_store)) return FALSE;
	}
	else {
		wcscpy(virtual_store, L"dummy");
	}

	swprintf(buf, L"/runas=%p,%d,%d,%d,%d,\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"",
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
	BOOL	is_rotate = FALSE;
	int		len;

// インストールパス設定
	len = GetDlgItemText(FILE_EDIT, setupDir, sizeof(setupDir));
	if (setupDir[len-1] == '\\') setupDir[len-1] = 0;
	Wstr	w_setup(setupDir);

	if (IsWinVista() && TIsVirtualizedDirW(w_setup.Buf())) {
		if (!::IsUserAnAdmin()) {
			return	RunAsAdmin(TRUE);
		}
		else if (cfg.runImme && cfg.setupDir && lstrcmpiW(w_setup.Buf(), cfg.setupDir)) {
			return	MessageBox(LoadStr(IDS_ADMINCHANGE)), FALSE;
		}
	}

	CreateDirectory(setupDir, NULL);
	DWORD	attr = GetFileAttributes(setupDir);

	if (attr == 0xffffffff || (attr & FILE_ATTRIBUTE_DIRECTORY) == 0)
		return	MessageBox(LoadStr(IDS_NOTCREATEDIR)), FALSE;
	MakePath(setupPath, setupDir, FASTCOPY_EXE);

	if (MessageBox(LoadStr(IDS_START), INSTALL_STR, MB_OKCANCEL|MB_ICONINFORMATION) != IDOK)
		return	FALSE;

// ファイルコピー
	if (cfg.mode == SETUP_MODE) {
		char	installPath[MAX_PATH], orgDir[MAX_PATH];

		::GetModuleFileName(NULL, orgDir, sizeof(orgDir));
		GetParentDir(orgDir, orgDir);

		for (int cnt=0; SetupFiles[cnt] != NULL; cnt++) {
			MakePath(buf, orgDir, SetupFiles[cnt]);
			MakePath(installPath, setupDir, SetupFiles[cnt]);

			if (MiniCopy(buf, installPath, &is_rotate) || IsSameFile(buf, installPath))
				continue;

			if ((strcmp(SetupFiles[cnt], CURRENT_SHEXTDLL_EX) == 0 ||
				 strcmp(SetupFiles[cnt], CURRENT_SHEXTDLL) == 0) && DelayCopy(buf, installPath)) {
				is_delay_copy = TRUE;
				continue;
			}
			else {
				Debug("mincopy fail path\n%s\n%s\n%s\n%d %d\n", SetupFiles[cnt], CURRENT_SHEXTDLL_EX, CURRENT_SHEXTDLL, strcmp(SetupFiles[cnt], CURRENT_SHEXTDLL_EX), strcmp(SetupFiles[cnt], CURRENT_SHEXTDLL));
			}
			return	MessageBox(installPath, LoadStr(IDS_NOTCREATEFILE)), FALSE;
		}
		TRegistry	reg(HSTOOLS_STR, 0, BY_MBCS);
		if (reg.CreateKey(FASTCOPY)) {
			reg.SetStr("Path", setupDir);
			reg.CloseKey();
		}
	}

// スタートメニュー＆デスクトップに登録
	char	*linkPath[]	= { WtoA(cfg.startMenu), WtoA(cfg.deskTop), NULL };
	BOOL	execFlg[]	= { cfg.programLink,     cfg.desktopLink,   NULL };

	for (int cnt=0; linkPath[cnt]; cnt++) {
		strcpy(buf, linkPath[cnt]);
		if (cnt != 0 || RemoveSameLink(linkPath[cnt], buf) == FALSE) {
			::sprintf(buf + strlen(buf), "\\%s", FASTCOPY_SHORTCUT);
		}
		if (execFlg[cnt]) {
			Wstr	w_setup(setupPath, BY_MBCS);
			Wstr	w_buf(buf, BY_MBCS);
			SymLinkW(w_setup.Buf(), w_buf.Buf());
		}
		else {
			Wstr	w_buf(buf, BY_MBCS);
			DeleteLinkW(w_buf.Buf());
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

	if (IsWinVista() && TIsVirtualizedDirW(w_setup.Buf())) {
		WCHAR	wbuf[MAX_PATH] = L"", old_path[MAX_PATH] = L"", usr_path[MAX_PATH] = L"";
		WCHAR	fastcopy_dir[MAX_PATH], *fastcopy_dirname = NULL;

		::GetFullPathNameW(w_setup.Buf(), MAX_PATH, fastcopy_dir, &fastcopy_dirname);

		if (cfg.appData) {
			wcscpy(usr_path, cfg.appData);
		}
		else {
			::SHGetSpecialFolderPathW(NULL, wbuf, CSIDL_APPDATA, FALSE);
			MakePathW(usr_path, wbuf, fastcopy_dirname);
		}

		ConvertVirtualStoreConf(w_setup.Buf(), usr_path, cfg.virtualDir);
	}

// コピーしたアプリケーションを起動
	const char *msg = LoadStr(is_delay_copy ? IDS_DELAYSETUPCOMPLETE :
							  is_rotate ? IDS_REPLACECOMPLETE : IDS_SETUPCOMPLETE);
	int			flg = MB_OKCANCEL|MB_ICONINFORMATION;

	TLaunchDlg	dlg(msg, this);

	if (dlg.Exec() == IDOK) {
		ShellExecute(NULL, "open", setupPath, "", setupDir, SW_SHOW);
	}

	::PostQuitMessage(0);
	return	TRUE;
}

// シェル拡張を解除
enum ShellExtOpe { CHECK_SHELLEXT, UNREGISTER_SHELLEXT };

int ShellExtFunc(char *setup_dir, ShellExtOpe kind, BOOL isAdmin)
{
	char	buf[MAX_PATH];
	int		ret = FALSE;

	MakePath(buf, setup_dir, CURRENT_SHEXTDLL);
	HMODULE		hShellExtDll = TLoadLibrary(buf);

	if (hShellExtDll) {
		BOOL (WINAPI *IsRegisterDll)(BOOL) = (BOOL (WINAPI *)(BOOL))
			GetProcAddress(hShellExtDll, "IsRegistServer");
		HRESULT (WINAPI *UnRegisterDll)(void) = (HRESULT (WINAPI *)(void))
			GetProcAddress(hShellExtDll, "DllUnregisterServer");
		BOOL (WINAPI *SetAdminMode)(BOOL) = (BOOL (WINAPI *)(BOOL))
			GetProcAddress(hShellExtDll, "SetAdminMode");

		if (IsRegisterDll && UnRegisterDll && SetAdminMode) {
			switch (kind) {
			case CHECK_SHELLEXT:
				ret = IsRegisterDll(isAdmin);
				break;
			case UNREGISTER_SHELLEXT:
				SetAdminMode(isAdmin);
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

	is_shext = ShellExtFunc(setupDir, CHECK_SHELLEXT, TRUE);

	if (is_shext && IsWinVista() && !::IsUserAnAdmin()) {
		RunAsAdmin(TRUE);
		return	TRUE;
	}

	if (MessageBox(LoadStr(IDS_START), UNINSTALL_STR, MB_OKCANCEL | MB_ICONINFORMATION) != IDOK)
		return	FALSE;

	// スタートメニュー＆デスクトップから削除
	TRegistry	reg(HKEY_CURRENT_USER, BY_MBCS);
	if (reg.OpenKey(REGSTR_SHELLFOLDERS)) {
		char	*regStr[] = { REGSTR_PROGRAMS, REGSTR_DESKTOP, NULL };

		for (int cnt = 0; regStr[cnt] != NULL; cnt++) {
			if (reg.GetStr(regStr[cnt], buf, sizeof(buf))) {
				if (cnt == 0)
					RemoveSameLink(buf);
				::sprintf(buf + strlen(buf), "\\%s", FASTCOPY_SHORTCUT);
				Wstr	w_buf(buf, BY_MBCS);
				DeleteLinkW(w_buf.Buf());
			}
		}
		reg.CloseKey();
	}

	ShellExtFunc(setupDir, UNREGISTER_SHELLEXT, FALSE);
	if (::IsUserAnAdmin()) {
		ShellExtFunc(setupDir, UNREGISTER_SHELLEXT, TRUE);
	}

#ifdef _WIN64
	if (1) {
#else
	if (TIsWow64()) {
#endif
		SHELLEXECUTEINFO	sei = { sizeof(sei) };
		char	arg[1024];

		sprintf(arg, "\"%s\\%s\",%s", setupDir, CURRENT_SHEXTDLL_EX, "DllUnregisterServer");
		sei.lpFile = "rundll32.exe";
		sei.lpParameters = arg;
		ShellExecuteEx(&sei);
	}

// レジストリからアンインストール情報を削除
	if (reg.OpenKey("Software")) {
		if (reg.OpenKey(HSTOOLS_STR)) {
			reg.DeleteKey(FASTCOPY);
			reg.CloseKey();
		}
		reg.CloseKey();
	}

#if 0
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
#endif

// 終了メッセージ
	MessageBox(is_shext ? LoadStr(IDS_UNINSTSHEXTFIN) : LoadStr(IDS_UNINSTFIN));

// インストールディレクトリを開く
	if (GetFileAttributes(setupDir) != 0xffffffff) {
		::ShellExecute(NULL, NULL, setupDir, 0, 0, SW_SHOW);
	}

// AppDataディレクトリを開く
	if (IsWinVista()) {
		WCHAR	wbuf[MAX_PATH] = L"", upath[MAX_PATH] = L"";
		WCHAR	fastcopy_dir[MAX_PATH] = L"", *fastcopy_dirname = NULL;
		Wstr	w_setup(setupDir);

		if (TIsVirtualizedDirW(w_setup.Buf())) {
			if (::SHGetSpecialFolderPathW(NULL, wbuf, CSIDL_APPDATA, FALSE)) {
				::GetFullPathNameW(w_setup.Buf(), MAX_PATH, fastcopy_dir, &fastcopy_dirname);
				MakePathW(upath, wbuf, fastcopy_dirname);

				if (::GetFileAttributesW(upath) != 0xffffffff) {
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
	IShellLink		*shellLink;
	IPersistFile	*persistFile;
	WCHAR			wbuf[MAX_PATH];
	BOOL			ret = FALSE;

	if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink,
			(void **)&shellLink))) {
		if (SUCCEEDED(shellLink->QueryInterface(IID_IPersistFile, (void **)&persistFile))) {
			AtoW(src, wbuf, wsizeof(wbuf));
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

	::sprintf(path, "%s\\*.*", dir);
	if ((fh = ::FindFirstFile(path, &data)) == INVALID_HANDLE_VALUE)
		return	FALSE;

	do {
		::sprintf(path, "%s\\%s", dir, data.cFileName);
		if (ReadLink(path, dest, arg) && *arg == 0) {
			int		dest_len = (int)strlen(dest);
			int		fastcopy_len = (int)strlen(FASTCOPY_EXE);
			if (dest_len > fastcopy_len
			&& strnicmp(dest + dest_len - fastcopy_len, FASTCOPY_EXE, fastcopy_len) == 0) {
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

	::CreateWindow(BUTTON_CLASS, LoadStr(IDS_MKDIR), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, 10,
		pt.y, cx, cy, hWnd, (HMENU)MKDIR_BUTTON, TApp::GetInstance(), NULL);
	::CreateWindow(BUTTON_CLASS, LoadStr(IDS_RMDIR), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
		18 + cx, pt.y, cx, cy, hWnd, (HMENU)RMDIR_BUTTON, TApp::GetInstance(), NULL);

	HFONT	hDlgFont = (HFONT)SendDlgItemMessage(IDOK, WM_GETFONT, 0, 0L);
	if (hDlgFont) {
		SendDlgItemMessage(MKDIR_BUTTON, WM_SETFONT, (LPARAM)hDlgFont, 0L);
		SendDlgItemMessage(RMDIR_BUTTON, WM_SETFONT, (LPARAM)hDlgFont, 0L);
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
	Show();
	return	TRUE;
}

BOOL TLaunchDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID)
	{
	case IDOK: case IDCANCEL:
		EndDialog(wID);
		return	TRUE;
	}
	return	FALSE;
}


