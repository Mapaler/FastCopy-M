static char *setuplg_id = 
	"@(#)Copyright (C) 2015-2016 H.Shirouzu		setupdlg.cpp	ver3.25";
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Create					: 2015-07-17(Fri)
	Update					: 2016-10-19(Wed)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	Modify					: Mapaler 2015-08-13
	======================================================================== */

#include "mainwin.h"
#include <stdio.h>

#include "shellext/shelldef.h"

/*
	Setup用Sheet
*/
BOOL TSetupSheet::Create(int _resId, Cfg *_cfg, TSetupDlg *_parent)
{
	cfg    = _cfg;
	resId  = _resId;
	parent = setupDlg = _parent;

	if (resId == MAIN_SHEET) sv = new SheetDefSv;

	return	TDlg::Create();
}

BOOL TSetupSheet::EvCreate(LPARAM lParam)
{
	RECT	rc;
	POINT	pt;
	::GetWindowRect(parent->GetDlgItem(SETUP_LIST), &rc);
	pt.x = rc.right;
	pt.y = rc.top;
	ScreenToClient(parent->hWnd, &pt);
	SetWindowPos(0, pt.x + 10, pt.y - 2, 0, 0, SWP_NOSIZE|SWP_NOZORDER);

	SetWindowLong(GWL_EXSTYLE, GetWindowLong(GWL_EXSTYLE)|WS_EX_CONTROLPARENT);

	SetData();
	return	TRUE;
}

BOOL TSetupSheet::CheckData()
{
	if (resId == MAIN_SHEET) {
		if (GetDlgItemInt(BUFSIZE_EDIT) <
			setupDlg->GetSheet(IO_SHEET)->GetDlgItemInt(MAXTRANS_EDIT) * BUFIO_SIZERATIO) {
			MessageBox(LoadStr(IDS_SMALLBUF_SETERR));
			return	FALSE;
		}
		return	TRUE;
	}
	else if (resId == IO_SHEET) {
		if (GetDlgItemInt(MAXTRANS_EDIT) <= 0 || GetDlgItemInt(MAXOVL_EDIT) <= 0) {
			MessageBox(LoadStr(IDS_SMALLVAL_SETERR));
			return	FALSE;
		}
		if (GetDlgItemInt(MAXTRANS_EDIT) > 4095) { // under 4GB
			MessageBox(LoadStr(IDS_BIGVAL_SETERR));
			return	FALSE;
		}
		if (GetDlgItemInt(MAXTRANS_EDIT) % GetDlgItemInt(MAXOVL_EDIT)) {
			MessageBox(LoadStr(IDS_MAXOVL_SETERR));
			return	FALSE;
		}
		if (GetDlgItemInt(MAXTRANS_EDIT) * BUFIO_SIZERATIO >
			setupDlg->GetSheet(MAIN_SHEET)->GetDlgItemInt(BUFSIZE_EDIT)) {
			MessageBox(LoadStr(IDS_BIGIO_SETERR));
			return	FALSE;
		}
		return	TRUE;
	}
	else if (resId == PHYSDRV_SHEET) {
		char	c, last_c = 0, buf[sizeof(cfg->driveMap)];
		GetDlgItemText(DRIVEMAP_EDIT, buf, sizeof(buf));
		for (int i=0; i < sizeof(buf) && (c = buf[i]); i++) {
			if (c >= 'a' && c <= 'z') buf[i] = c = toupper(c);
			if ((c < 'A' || c > 'Z' || strchr(buf+i+1, c)) && c != ',' || c == last_c) {
				MessageBox(LoadStr(IDS_DRVGROUP_SETERR));
				return	FALSE;
			}
			last_c = c;
		}
		return	TRUE;
	}
	else if (resId == PARALLEL_SHEET) {
		if (GetDlgItemInt(MAXRUN_EDIT) <= 0) {
			MessageBox(LoadStr(IDS_SMALLVAL_SETERR));
			return	FALSE;
		}
		return	TRUE;
	}
	else if (resId == COPYOPT_SHEET) {
		return	TRUE;
	}
	else if (resId == DEL_SHEET) {
		return	TRUE;
	}
	else if (resId == LOG_SHEET) {
		return	TRUE;
	}
	else if (resId == MISC_SHEET) {
		return	TRUE;
	}

	return	TRUE;
}

BOOL TSetupSheet::SetData()
{
	if (resId == MAIN_SHEET) {
		if (sv) {
			sv->bufSize			= cfg->bufSize;
			sv->estimateMode	= cfg->estimateMode;
			sv->ignoreErr		= cfg->ignoreErr;
			sv->enableVerify	= cfg->enableVerify;
			sv->enableAcl		= cfg->enableAcl;
			sv->enableStream	= cfg->enableStream;
			sv->speedLevel		= cfg->speedLevel;
			sv->isExtendFilter	= cfg->isExtendFilter;
			sv->enableOwdel		= cfg->enableOwdel;
		}
		SetDlgItemInt(BUFSIZE_EDIT, cfg->bufSize);
		CheckDlgButton(ESTIMATE_CHECK, cfg->estimateMode);
		CheckDlgButton(IGNORE_CHECK, cfg->ignoreErr);
		CheckDlgButton(VERIFY_CHECK, cfg->enableVerify);
		CheckDlgButton(ACL_CHECK, cfg->enableAcl);
		CheckDlgButton(STREAM_CHECK, cfg->enableStream);
		SendDlgItemMessage(SPEED_SLIDER, TBM_SETRANGE, 0, MAKELONG(SPEED_SUSPEND, SPEED_FULL));
		SetSpeedLevelLabel(this, cfg->speedLevel);
		CheckDlgButton(EXTENDFILTER_CHECK, cfg->isExtendFilter);
		CheckDlgButton(OWDEL_CHECK, cfg->enableOwdel);
	}
	else if (resId == IO_SHEET) {
		SetDlgItemInt(MAXTRANS_EDIT, cfg->maxTransSize);
		SetDlgItemInt(MAXOVL_EDIT, cfg->maxOvlNum);
		if (cfg->minSectorSize == 0 || cfg->minSectorSize == 4096) {
			CheckDlgButton(SECTOR4096_CHECK, cfg->minSectorSize == 4096);
		} else {
			::EnableWindow(GetDlgItem(SECTOR4096_CHECK), FALSE);
		}
		CheckDlgButton(READOSBUF_CHECK, cfg->isReadOsBuf);
		SetDlgItemInt(NONBUFMINNTFS_EDIT, cfg->nbMinSizeNtfs);
		SetDlgItemInt(NONBUFMINFAT_EDIT, cfg->nbMinSizeFat);
	}
	else if (resId == PHYSDRV_SHEET) {
		SetDlgItemText(DRIVEMAP_EDIT, cfg->driveMap);
		SendDlgItemMessage(NETDRVMODE_COMBO, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_NETDRV_UNC));
		SendDlgItemMessage(NETDRVMODE_COMBO, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_NETDRV_SVR));
		SendDlgItemMessage(NETDRVMODE_COMBO, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_NETDRV_ALL));
		SendDlgItemMessage(NETDRVMODE_COMBO, CB_SETCURSEL, cfg->netDrvMode, 0);
	}
	else if (resId == PARALLEL_SHEET) {
		SetDlgItemInt(MAXRUN_EDIT, cfg->maxRunNum);
		CheckDlgButton(FORCESTART_CHECK, cfg->forceStart);
	}
	else if (resId == COPYOPT_SHEET) {
		CheckDlgButton(SAMEDIR_RENAME_CHECK, cfg->isSameDirRename);
		CheckDlgButton(EMPTYDIR_CHECK, cfg->skipEmptyDir);
		CheckDlgButton(REPARSE_CHECK, cfg->isReparse);
		::EnableWindow(GetDlgItem(REPARSE_CHECK), TRUE);
		CheckDlgButton(MOVEATTR_CHECK, cfg->enableMoveAttr);
		CheckDlgButton(SERIALMOVE_CHECK, cfg->serialMove);
		CheckDlgButton(SERIALVERIFYMOVE_CHECK, cfg->serialVerifyMove);
		SendDlgItemMessage(HASH_COMBO, CB_ADDSTRING, 0, (LPARAM)"MD5");
		SendDlgItemMessage(HASH_COMBO, CB_ADDSTRING, 0, (LPARAM)"SHA-1");
		SendDlgItemMessage(HASH_COMBO, CB_ADDSTRING, 0, (LPARAM)"SHA-256");
		SendDlgItemMessage(HASH_COMBO, CB_SETCURSEL, cfg->hashMode, 0);
		SetDlgItemInt(TIMEGRACE_EDIT, cfg->timeDiffGrace);
	}
	else if (resId == DEL_SHEET) {
		CheckDlgButton(NSA_CHECK, cfg->enableNSA);
		CheckDlgButton(DELDIR_CHECK, cfg->delDirWithFilter);
	}
	else if (resId == LOG_SHEET) {
		SetDlgItemInt(HISTORY_EDIT, cfg->maxHistoryNext);
		CheckDlgButton(ERRLOG_CHECK, cfg->isErrLog);
		CheckDlgButton(UTF8LOG_CHECK, cfg->isUtf8Log);
		::EnableWindow(GetDlgItem(UTF8LOG_CHECK), TRUE);
		CheckDlgButton(FILELOG_CHECK, cfg->fileLogMode);
		CheckDlgButton(TIMESTAMP_CHECK, (cfg->fileLogFlags & FastCopy::FILELOG_TIMESTAMP) ?
			TRUE : FALSE);
		CheckDlgButton(FILESIZE_CHECK,  (cfg->fileLogFlags & FastCopy::FILELOG_FILESIZE)  ?
			TRUE : FALSE);
		CheckDlgButton(ACLERRLOG_CHECK, cfg->aclErrLog);
		::EnableWindow(GetDlgItem(ACLERRLOG_CHECK), TRUE);
		CheckDlgButton(STREAMERRLOG_CHECK, cfg->streamErrLog);
		::EnableWindow(GetDlgItem(STREAMERRLOG_CHECK), TRUE);
	}
	else if (resId == MISC_SHEET) {
		CheckDlgButton(EXECCONFIRM_CHECK, cfg->execConfirm);
		CheckDlgButton(TASKBAR_CHECK, cfg->taskbarMode);
		CheckDlgButton(FINISH_CHECK, (cfg->finishNotify & 1));
		CheckDlgButton(SPAN1_RADIO + cfg->infoSpan, 1);
		CheckDlgButton(PREVENTSLEEP_CHECK, cfg->preventSleep);

		if ((cfg->lcid != -1 || GetSystemDefaultLCID() != 0x409)) { // == 0x411 改成 != 0x409 让所有语言都可以切换到英文
			::ShowWindow(GetDlgItem(LCID_CHECK), SW_SHOW);
			::EnableWindow(GetDlgItem(LCID_CHECK), TRUE);
			CheckDlgButton(LCID_CHECK, cfg->lcid == -1 || cfg->lcid != 0x409 ? FALSE : TRUE);
		}
	}
	return	TRUE;
}

void TSetupSheet::ReflectToMainWindow()
{
	TWin *win = parent ? parent->Parent() : NULL;

	if (!win) return;

	if (cfg->bufSize != sv->bufSize) {
		win->SetDlgItemInt(BUFSIZE_EDIT, cfg->bufSize);
	}
	if (cfg->estimateMode != sv->estimateMode) {
		win->CheckDlgButton(ESTIMATE_CHECK, cfg->estimateMode);
	}
	if (cfg->ignoreErr != sv->ignoreErr) {
		win->CheckDlgButton(IGNORE_CHECK, cfg->ignoreErr);
	}
	if (cfg->enableVerify != sv->enableVerify) {
		win->CheckDlgButton(VERIFY_CHECK, cfg->enableVerify);
	}
	if (cfg->enableAcl != sv->enableAcl) {
		win->CheckDlgButton(ACL_CHECK, cfg->enableAcl);
	}
	if (cfg->enableStream != sv->enableStream) {
		win->CheckDlgButton(STREAM_CHECK, cfg->enableStream);
	}
	if (cfg->speedLevel != sv->speedLevel) {
		SetSpeedLevelLabel(win, cfg->speedLevel);
		win->PostMessage(WM_HSCROLL, MAKEWPARAM(SB_THUMBTRACK, cfg->speedLevel),
			(LPARAM)win->GetDlgItem(SPEED_SLIDER));
	}
	if (cfg->isExtendFilter != sv->isExtendFilter) {
		win->CheckDlgButton(EXTENDFILTER_CHECK, cfg->isExtendFilter);
	}
	if (cfg->enableOwdel != sv->enableOwdel) {
		win->CheckDlgButton(OWDEL_CHECK, cfg->enableOwdel);
	}
}

BOOL TSetupSheet::GetData()
{
	if (resId == MAIN_SHEET) {
		cfg->bufSize        = GetDlgItemInt(BUFSIZE_EDIT);
		cfg->estimateMode   = IsDlgButtonChecked(ESTIMATE_CHECK);
		cfg->ignoreErr      = IsDlgButtonChecked(IGNORE_CHECK);
		cfg->enableVerify   = IsDlgButtonChecked(VERIFY_CHECK);
		cfg->enableAcl      = IsDlgButtonChecked(ACL_CHECK);
		cfg->enableStream   = IsDlgButtonChecked(STREAM_CHECK);
		cfg->speedLevel     = (int)SendDlgItemMessage(SPEED_SLIDER, TBM_GETPOS, 0, 0);
		cfg->isExtendFilter = IsDlgButtonChecked(EXTENDFILTER_CHECK);
		cfg->enableOwdel    = IsDlgButtonChecked(OWDEL_CHECK);

		ReflectToMainWindow();
	}
	else if (resId == IO_SHEET) {
		cfg->maxTransSize  = GetDlgItemInt(MAXTRANS_EDIT);
		cfg->maxOvlNum     = GetDlgItemInt(MAXOVL_EDIT);
		if (cfg->minSectorSize == 0 || cfg->minSectorSize == 4096) {
			cfg->minSectorSize = IsDlgButtonChecked(SECTOR4096_CHECK) ? 4096 : 0;
		}
		cfg->isReadOsBuf   = IsDlgButtonChecked(READOSBUF_CHECK);
		cfg->nbMinSizeNtfs = GetDlgItemInt(NONBUFMINNTFS_EDIT);
		cfg->nbMinSizeFat  = GetDlgItemInt(NONBUFMINFAT_EDIT);

		char	buf[sizeof(cfg->driveMap)];
		GetDlgItemText(DRIVEMAP_EDIT, buf, sizeof(buf));
		strcpy(cfg->driveMap, buf);
	}
	else if (resId == PHYSDRV_SHEET) {
		GetDlgItemText(DRIVEMAP_EDIT, cfg->driveMap, sizeof(cfg->driveMap));
		cfg->netDrvMode = (int)SendDlgItemMessage(NETDRVMODE_COMBO, CB_GETCURSEL, 0, 0);
	}
	else if (resId == PARALLEL_SHEET) {
		cfg->maxRunNum  = GetDlgItemInt(MAXRUN_EDIT);
		cfg->forceStart = IsDlgButtonChecked(FORCESTART_CHECK);
	}
	else if (resId == COPYOPT_SHEET) {
		cfg->isSameDirRename  = IsDlgButtonChecked(SAMEDIR_RENAME_CHECK);
		cfg->skipEmptyDir     = IsDlgButtonChecked(EMPTYDIR_CHECK);
		cfg->isReparse        = IsDlgButtonChecked(REPARSE_CHECK);
		cfg->enableMoveAttr   = IsDlgButtonChecked(MOVEATTR_CHECK);
		cfg->serialMove       = IsDlgButtonChecked(SERIALMOVE_CHECK);
		cfg->serialVerifyMove = IsDlgButtonChecked(SERIALVERIFYMOVE_CHECK);
		cfg->hashMode         = (int)SendDlgItemMessage(HASH_COMBO, CB_GETCURSEL, 0, 0);
		cfg->timeDiffGrace    = GetDlgItemInt(TIMEGRACE_EDIT);
	}
	else if (resId == DEL_SHEET) {
		cfg->enableNSA        = IsDlgButtonChecked(NSA_CHECK);
		cfg->delDirWithFilter = IsDlgButtonChecked(DELDIR_CHECK);
	}
	else if (resId == LOG_SHEET) {
		cfg->maxHistoryNext = GetDlgItemInt(HISTORY_EDIT);
		cfg->isErrLog       = IsDlgButtonChecked(ERRLOG_CHECK);
		cfg->isUtf8Log      = IsDlgButtonChecked(UTF8LOG_CHECK);
		cfg->fileLogMode    = IsDlgButtonChecked(FILELOG_CHECK);
		cfg->fileLogFlags   = (IsDlgButtonChecked(TIMESTAMP_CHECK) ? FastCopy::FILELOG_TIMESTAMP
			: 0) | (IsDlgButtonChecked(FILESIZE_CHECK)  ? FastCopy::FILELOG_FILESIZE : 0);
		cfg->aclErrLog      = IsDlgButtonChecked(ACLERRLOG_CHECK);
		cfg->streamErrLog   = IsDlgButtonChecked(STREAMERRLOG_CHECK);
	}
	else if (resId == MISC_SHEET) {
		cfg->execConfirm = IsDlgButtonChecked(EXECCONFIRM_CHECK);
		cfg->taskbarMode = IsDlgButtonChecked(TASKBAR_CHECK);
		if (IsDlgButtonChecked(FINISH_CHECK)) {
			cfg->finishNotify |= 1;
		}
		else {
			cfg->finishNotify &= ~1;
		}
		cfg->infoSpan    =	IsDlgButtonChecked(SPAN1_RADIO) ? 0 :
							IsDlgButtonChecked(SPAN2_RADIO) ? 1 : 2;

		cfg->preventSleep = IsDlgButtonChecked(PREVENTSLEEP_CHECK);

		if (::IsWindowEnabled(GetDlgItem(LCID_CHECK))) {
			cfg->lcid = IsDlgButtonChecked(LCID_CHECK) ? 0x409 : -1;
		}
	}
	return	TRUE;
}

BOOL TSetupSheet::EventScroll(UINT uMsg, int Code, int nPos, HWND hwndScrollBar)
{
	SetSpeedLevelLabel(this);
	return	TRUE;
}

BOOL TSetupSheet::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	if (wID == HELP_BUTTON) {
		WCHAR	*section = L"";
		if (resId == MAIN_SHEET) {
			section = L"#setting_default";
		}
		else if (resId == IO_SHEET) {
			section = L"#setting_io";
		}
		else if (resId == PHYSDRV_SHEET) {
			section = L"#setting_physical";
		}
		else if (resId == PARALLEL_SHEET) {
			section = L"#setting_parallel";
		}
		else if (resId == COPYOPT_SHEET) {
			section = L"#setting_copyopt";
		}
		else if (resId == DEL_SHEET) {
			section = L"#setting_del";
		}
		else if (resId == LOG_SHEET) {
			section = L"#setting_log";
		}
		else if (resId == MISC_SHEET) {
			section = L"#setting_misc";
		}
		ShowHelpW(NULL, cfg->execDir, LoadStrW(IDS_FASTCOPYHELP), section);
		return	TRUE;
	}
	return	FALSE;
}

/*
	Setup Dialog初期化処理
*/
TSetupDlg::TSetupDlg(Cfg *_cfg, TWin *_parent) : TDlg(SETUP_DIALOG, _parent)
{
	cfg		= _cfg;
	curIdx	= -1;
}

/*
	Window 生成時の CallBack
*/
BOOL TSetupDlg::EvCreate(LPARAM lParam)
{
	setup_list.AttachWnd(GetDlgItem(SETUP_LIST));

	for (int i=0; i < MAX_SETUP_SHEET; i++) {
		sheet[i].Create(SETUP_SHEET1 + i, cfg, this);
		setup_list.SendMessage(LB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_SETUP_SHEET1 + i));
	}
	SetSheet();

	if (rect.left == CW_USEDEFAULT)
	{
		GetWindowRect(&rect);
		int xsize = rect.right - rect.left, ysize = rect.bottom - rect.top;
		int	cx = ::GetSystemMetrics(SM_CXFULLSCREEN), cy = ::GetSystemMetrics(SM_CYFULLSCREEN);
		int	x = (cx - xsize)/2;
		int y = (cy - ysize)/2;

		MoveWindow((x < 0) ? 0 : x % (cx - xsize), (y < 0) ? 0 : y % (cy - ysize),
			xsize, ysize, FALSE);
	}
	else
		MoveWindow(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, FALSE);

	return	TRUE;
}

BOOL TSetupDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK: case IDAPPLY:
		{
			if (!sheet[curIdx].CheckData()) {
				return	TRUE;
			}
			for (int i=0; i < MAX_SETUP_SHEET; i++) {
				sheet[i].GetData();
			}
			cfg->WriteIni();
			if (wID == IDOK) {
				EndDialog(wID);
			}
		}
		return	TRUE;

	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;

	case SETUP_LIST:
		if (wNotifyCode == LBN_SELCHANGE) SetSheet();
		return	TRUE;
	}
	return	FALSE;
}

void TSetupDlg::SetSheet(int idx)
{
	if (idx < 0) {
		if ((idx = (int)setup_list.SendMessage(LB_GETCURSEL, 0, 0)) < 0) {
			idx = 0;
			setup_list.SendMessage(LB_SETCURSEL, idx, 0);
		}
	}
	if (curIdx >= 0 && curIdx != idx) {
		if (!sheet[curIdx].CheckData()) {
			setup_list.SendMessage(LB_SETCURSEL, curIdx, 0);
			return;
		}
	}
	for (int i=0; i < MAX_SETUP_SHEET; i++) {
		sheet[i].Show(i == idx ? SW_SHOW : SW_HIDE);
	}
	curIdx = idx;
}


/*
	ShellExt
*/
#ifdef _WIN64
#define CURRENT_SHEXTDLL	"FastEx64.dll"
#define CURRENT_SHEXTDLL_EX	"FastExt1.dll"
#else
#define CURRENT_SHEXTDLL	"FastExt1.dll"
#define CURRENT_SHEXTDLL_EX	"FastEx64.dll"
#endif
#define REGISTER_PROC		"DllRegisterServer"
#define UNREGISTER_PROC		"DllUnregisterServer"
#define REGISTERUSER_PROC	"DllRegisterServerUser"
#define UNREGISTERUSER_PROC	"DllUnregisterServerUser"
#define ISREGISTER_PROC		"IsRegistServer"
#define SETMENUFLAGS_PROC	"SetMenuFlags"
#define GETMENUFLAGS_PROC	"GetMenuFlags"
#define SETADMINMODE_PROC	"SetAdminMode"

BOOL ShellExt::Load(WCHAR *parent_dir, WCHAR *dll_name)
{
	if (hShDll) {
		UnLoad();
	}

	WCHAR	path[MAX_PATH];
	MakePathW(path, parent_dir, dll_name);
	if ((hShDll = TLoadLibraryW(path)) == NULL)
		return	FALSE;

	RegisterDllProc  = (HRESULT (WINAPI *)(void))GetProcAddress(hShDll, REGISTER_PROC);
	UnRegisterDllProc= (HRESULT (WINAPI *)(void))GetProcAddress(hShDll, UNREGISTER_PROC);
	RegisterDllUserProc  = (HRESULT (WINAPI *)(void))GetProcAddress(hShDll, REGISTERUSER_PROC);
	UnRegisterDllUserProc= (HRESULT (WINAPI *)(void))GetProcAddress(hShDll, UNREGISTERUSER_PROC);
	IsRegisterDllProc= (BOOL (WINAPI *)(BOOL))GetProcAddress(hShDll, ISREGISTER_PROC);
	SetMenuFlagsProc = (BOOL (WINAPI *)(BOOL, int))GetProcAddress(hShDll, SETMENUFLAGS_PROC);
	GetMenuFlagsProc = (int (WINAPI *)(BOOL))GetProcAddress(hShDll, GETMENUFLAGS_PROC);
	SetAdminModeProc = (BOOL (WINAPI *)(BOOL))GetProcAddress(hShDll, SETADMINMODE_PROC);

// ver違いで proc error になるが、
// install時に overwrite failを検出して、リネームする

	if (!RegisterDllProc || !UnRegisterDllProc || !IsRegisterDllProc
	|| !RegisterDllUserProc || !UnRegisterDllUserProc
	|| !SetMenuFlagsProc || !GetMenuFlagsProc || !SetAdminModeProc) {
		::FreeLibrary(hShDll);
		hShDll = NULL;
		return	FALSE;
	}

	SetAdminModeProc(isAdmin);

	return	TRUE;
}

BOOL ShellExt::UnLoad(void)
{
	if (hShDll) {
		::FreeLibrary(hShDll);
		hShDll = NULL;
	}
	return	TRUE;
}

/*
	ShellExt Dialog初期化処理
*/
TShellExtDlg::TShellExtDlg(Cfg *_cfg, BOOL _isAdmin, TWin *_parent)
	: shellExt(_isAdmin), TDlg(SHELLEXT_DIALOG, _parent)
{
	cfg     = _cfg;
	isAdmin = _isAdmin;
	shCfg   = isAdmin ? &cfg->shAdmin : &cfg->shUser;
}

/*
	Window 生成時の CallBack
*/
BOOL TShellExtDlg::EvCreate(LPARAM lParam)
{
	if (!shellExt.Load(cfg->execDir, AtoWs(CURRENT_SHEXTDLL))) {
		TMsgBox(this).Exec("Can't load " CURRENT_SHEXTDLL);
		PostMessage(WM_CLOSE, 0, 0);
		return	FALSE;
	}

	ReflectStatus();

	if (rect.left == CW_USEDEFAULT)
	{
		GetWindowRect(&rect);
		int xsize = rect.right - rect.left, ysize = rect.bottom - rect.top;
		int	cx = ::GetSystemMetrics(SM_CXFULLSCREEN), cy = ::GetSystemMetrics(SM_CYFULLSCREEN);
		int	x = (cx - xsize)/2;
		int y = (cy - ysize)/2;

		MoveWindow((x < 0) ? 0 : x % (cx - xsize), (y < 0) ? 0 : y % (cy - ysize),
			xsize, ysize, FALSE);
	}
	else {
		MoveWindow(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, FALSE);
	}

	return	TRUE;
}

BOOL TShellExtDlg::ReflectStatus(void)
{
	BOOL	isRegister = shellExt.IsRegisterDllProc(isAdmin);
	int		flags;

	SetDlgItemText(IDSHELLEXT_OK, isRegister ?
		LoadStr(IDS_SHELLEXT_MODIFY) : LoadStr(IDS_SHELLEXT_EXEC));
	::EnableWindow(GetDlgItem(IDSHELLEXT_CANCEL), isRegister);

	if ((flags = shellExt.GetMenuFlagsProc(isAdmin)) == -1) {
		flags = (SHEXT_RIGHT_COPY|SHEXT_RIGHT_DELETE|SHEXT_DD_COPY|SHEXT_DD_MOVE);
		// for old shellext
	}
//	if ((flags & SHEXT_MENUFLG_EX) == 0) {	// old shellext
//		::EnableWindow(GetDlgItem(RIGHT_PASTE_CHECK), FALSE);
//	}

	if (flags & SHEXT_ISSTOREOPT) {
		shCfg->noConfirm	= (flags & SHEXT_NOCONFIRM) ? TRUE : FALSE;
		shCfg->noConfirmDel	= (flags & SHEXT_NOCONFIRMDEL) ? TRUE : FALSE;
		shCfg->taskTray		= (flags & SHEXT_TASKTRAY) ? TRUE : FALSE;
		shCfg->autoClose	= (flags & SHEXT_AUTOCLOSE) ? TRUE : FALSE;
	}

	CheckDlgButton(RIGHT_COPY_CHECK, flags & SHEXT_RIGHT_COPY);
	CheckDlgButton(RIGHT_DELETE_CHECK, flags & SHEXT_RIGHT_DELETE);
	CheckDlgButton(RIGHT_PASTE_CHECK, flags & SHEXT_RIGHT_PASTE);
	CheckDlgButton(RIGHT_SUBMENU_CHECK, flags & SHEXT_SUBMENU_RIGHT);
	CheckDlgButton(DD_COPY_CHECK, flags & SHEXT_DD_COPY);
	CheckDlgButton(DD_MOVE_CHECK, flags & SHEXT_DD_MOVE);
	CheckDlgButton(DD_SUBMENU_CHECK, flags & SHEXT_SUBMENU_DD);

	CheckDlgButton(NOCONFIRM_CHECK, shCfg->noConfirm);
	CheckDlgButton(NOCONFIRMDEL_CHECK, shCfg->noConfirmDel);
	CheckDlgButton(TASKTRAY_CHECK, shCfg->taskTray);
	CheckDlgButton(AUTOCLOSE_CHECK, shCfg->autoClose);

	SetWindowText(LoadStr(isAdmin ? IDS_SHELLEXT_ADMIN : IDS_SHELLEXT_USER));

	if (isAdmin && !::IsUserAnAdmin()) {
		SetDlgItemText(SHELL_STATIC, LoadStr(IDS_SHELLEXT_NEEDADMIN));
		::EnableWindow(GetDlgItem(IDSHELLEXT_OK), FALSE);
		::EnableWindow(GetDlgItem(IDSHELLEXT_CANCEL), FALSE);
	}

	return	TRUE;
}

BOOL TShellExtDlg::EvNcDestroy(void)
{
	if (shellExt.Status()) {
		shellExt.UnLoad();
	}
	return	TRUE;
}

BOOL TShellExtDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDSHELLEXT_OK: case IDSHELLEXT_CANCEL:
		if (isAdmin && !::IsUserAnAdmin()) {
			TMsgBox(this).Exec(LoadStr(IDS_REQUIRE_ADMIN));
			return	TRUE;
		}
		if (RegisterShellExt(wID == IDSHELLEXT_OK ? TRUE : FALSE) == FALSE) {
			TMsgBox(this).Exec("ShellExt Error");
		}
		ReflectStatus();
		if (wID == IDSHELLEXT_OK) {
			EndDialog(wID);
		}
		return	TRUE;

	case IDOK: case IDCANCEL:
		EndDialog(wID);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TShellExtDlg::RegisterShellExt(BOOL is_register)
{
	if (shellExt.Status() == FALSE)
		return	FALSE;

	shCfg->noConfirm    = is_register && IsDlgButtonChecked(NOCONFIRM_CHECK);
	shCfg->noConfirmDel = is_register && IsDlgButtonChecked(NOCONFIRMDEL_CHECK);
	shCfg->taskTray     = is_register && IsDlgButtonChecked(TASKTRAY_CHECK);
	shCfg->autoClose    = IsDlgButtonChecked(AUTOCLOSE_CHECK);
//	cfg->WriteIni();

#ifdef _WIN64
	if (1) {
#else
	if (TIsWow64()) {
#endif
		WCHAR	arg[1024];
		Wstr	cur_shell_ex(CURRENT_SHEXTDLL_EX);
		Wstr	reg_proc(isAdmin ? REGISTER_PROC : REGISTERUSER_PROC);
		Wstr	unreg_proc(isAdmin ? UNREGISTER_PROC : UNREGISTERUSER_PROC);

		swprintf(arg, L"\"%s\\%s\",%s %d", cfg->execDir, cur_shell_ex.Buf(),
			is_register ? reg_proc.Buf() : unreg_proc.Buf(), isAdmin);

		SHELLEXECUTEINFOW	sei = { sizeof(sei) };

		sei.lpFile = L"rundll32.exe";
		sei.lpParameters = arg;
		::ShellExecuteExW(&sei);
	}

	if (!is_register) {
		HRESULT	hr = isAdmin ? shellExt.UnRegisterDllProc() : shellExt.UnRegisterDllUserProc();
		return	(hr == S_OK) ? TRUE : FALSE;
	}

	int		flags = SHEXT_MENU_DEFAULT;

//	if ((shellExt.GetMenuFlagsProc(isAdmin) & SHEXT_MENUFLG_EX)) {
		flags |= SHEXT_ISSTOREOPT;
		if (shCfg->noConfirm)		flags |=  SHEXT_NOCONFIRM;
		else						flags &= ~SHEXT_NOCONFIRM;
		if (shCfg->noConfirmDel)	flags |=  SHEXT_NOCONFIRMDEL;
		else						flags &= ~SHEXT_NOCONFIRMDEL;
		if (shCfg->taskTray)		flags |=  SHEXT_TASKTRAY;
		else						flags &= ~SHEXT_TASKTRAY;
		if (shCfg->autoClose)		flags |=  SHEXT_AUTOCLOSE;
		else						flags &= ~SHEXT_AUTOCLOSE;
//	}
//	else {	// for old shell ext
//		flags &= ~SHEXT_MENUFLG_EX;
//	}

	if (!IsDlgButtonChecked(RIGHT_COPY_CHECK))
		flags &= ~SHEXT_RIGHT_COPY;

	if (!IsDlgButtonChecked(RIGHT_DELETE_CHECK))
		flags &= ~SHEXT_RIGHT_DELETE;

	if (!IsDlgButtonChecked(RIGHT_PASTE_CHECK))
		flags &= ~SHEXT_RIGHT_PASTE;

	if (!IsDlgButtonChecked(DD_COPY_CHECK))
		flags &= ~SHEXT_DD_COPY;

	if (!IsDlgButtonChecked(DD_MOVE_CHECK))
		flags &= ~SHEXT_DD_MOVE;

	if (IsDlgButtonChecked(RIGHT_SUBMENU_CHECK))
		flags |= SHEXT_SUBMENU_RIGHT;

	if (IsDlgButtonChecked(DD_SUBMENU_CHECK))
		flags |= SHEXT_SUBMENU_DD;

	HRESULT	hr = isAdmin ? shellExt.RegisterDllProc() : shellExt.RegisterDllUserProc();
	if (hr != S_OK)
		return	FALSE;

	return	shellExt.SetMenuFlagsProc(isAdmin, flags);
}

