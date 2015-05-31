static char *mainwin_id = 
	"@(#)Copyright (C) 2004-2012 H.Shirouzu		mainwin.cpp	ver2.11";
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Create					: 2004-09-15(Wed)
	Update					: 2012-06-18(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "mainwin.h"
#include "shellext/shelldef.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <richedit.h>

//#define HASHQUALITY_CHECK
#define FASTCOPY_TIMER_TICK 250

/*=========================================================================
  クラス ： TFastCopyApp
  概  要 ： アプリケーションクラス
  説  明 ： 
  注  意 ： 
=========================================================================*/
TFastCopyApp::TFastCopyApp(HINSTANCE _hI, LPSTR _cmdLine, int _nCmdShow)
	: TApp(_hI, _cmdLine, _nCmdShow)
{
	LoadLibrary("RICHED20.DLL");
//	LoadLibrary("SHELL32.DLL");
//	LoadLibrary("COMCTL32.DLL");
//	LoadLibrary("COMDLG32.dll");
	TLibInit_AdvAPI32();
#ifdef HASHQUALITY_CHECK
	extern void CheckHashQuality();
	CheckHashQuality();
#endif
}

TFastCopyApp::~TFastCopyApp()
{
}

void TFastCopyApp::InitWindow(void)
{
	TRegisterClass(FASTCOPY_CLASS);

	TDlg *mainDlg = new TMainDlg();
	mainWnd = mainDlg;
	mainDlg->Create();
}

BOOL TFastCopyApp::PreProcMsg(MSG *msg)
{
	if (msg->message == WM_KEYDOWN || msg->message == WM_KEYUP || msg->message == WM_ACTIVATEAPP) {
		mainWnd->PostMessage(WM_FASTCOPY_KEY, 0, 0);
	}
	return	TApp::PreProcMsg(msg);
}

int WINAPI WinMain(HINSTANCE _hI, HINSTANCE, LPSTR arg, int show)
{
	return	TFastCopyApp(_hI, arg, show).Run();
}

/*=========================================================================
  クラス ： TMainDlg
  概  要 ： メインダイアログ
  説  明 ： 
  注  意 ： 
=========================================================================*/
TMainDlg::TMainDlg() : TDlg(MAIN_DIALOG), aboutDlg(this), setupDlg(&cfg, this),
	shellExtDlg(&cfg, this), jobDlg(&cfg, this), finActDlg(&cfg, this),
	pathEdit(this), errEdit(this)
#ifdef USE_LISTVIEW
	, listHead(this), listView(this)
#endif
{
	WCHAR	*user_dir = NULL;
	WCHAR	*virtual_dir = NULL;

	orgArgv = CommandLineToArgvV(::GetCommandLineV(), &orgArgc);
	if (orgArgc == 2 && lstrcmpiV(orgArgv[1], GetLoadStrV(IDS_INSTALL_OPT)) == 0) {
		isInstaller = TRUE; // インストーラ起動
		orgArgc = 1;
	}
	else isInstaller = FALSE;

	if (IsWinVista() && TIsUserAnAdmin() && TIsEnableUAC()) {
		GetRunasInfo(&user_dir, &virtual_dir);
		TChangeWindowMessageFilter(WM_DROPFILES, 1);
		TChangeWindowMessageFilter(WM_COPYDATA, 1);
		TChangeWindowMessageFilter(WM_COPYGLOBALDATA, 1);
	}
	if (!cfg.ReadIni(user_dir, virtual_dir)) {
		MessageBox("Can't initialize..", FASTCOPY, MB_OK);
		PostQuitMessage(0);
		return;
	}

	if (cfg.lcid > 0)
		TSetDefaultLCID(cfg.lcid);

	cfg.PostReadIni();

	isShellExt = FALSE;
	isErrLog = cfg.isErrLog;
	isUtf8Log = cfg.isUtf8Log && IS_WINNT_V;
	fileLogMode = (FileLogMode)cfg.fileLogMode;
	isReparse = cfg.isReparse;
	isLinkDest = cfg.isLinkDest;
	isReCreate = cfg.isReCreate;
	isExtendFilter = cfg.isExtendFilter;
	maxLinkHash = cfg.maxLinkHash;
	forceStart = cfg.forceStart;

	shextNoConfirm    = cfg.shextNoConfirm;
	shextNoConfirmDel = cfg.shextNoConfirmDel;
	shextTaskTray     = cfg.shextTaskTray;
	shextAutoClose    = cfg.shextAutoClose;

	isTaskTray = FALSE;
	noConfirmDel = noConfirmStop = FALSE;
	isNetPlaceSrc = FALSE;
	skipEmptyDir = cfg.skipEmptyDir;
	diskMode = cfg.diskMode;
	curForeWnd = NULL;
	isErrEditHide = FALSE;
	isRunAsStart = FALSE;
	isRunAsParent = FALSE;
	resultStatus = TRUE;
	strcpyV(errLogPathV, cfg.errLogPathV);
	*fileLogPathV = 0;
	hFileLog = INVALID_HANDLE_VALUE;
	copyInfo = NULL;
	finActIdx = 0;
	doneRatePercent = -1;
	lastTotalSec = 0;
	calcTimes = 0;
	errBufOffset = 0;
	listBufOffset = 0;
	timerCnt = 0;
	timerLast = 0;

	curPriority = ::GetPriorityClass(::GetCurrentProcess());

	autoCloseLevel = NO_CLOSE;
	curIconIndex = 0;
	pathLogBuf = NULL;
	isDelay = FALSE;
	isAbort = FALSE;
	endTick = 0;
	hErrLog = INVALID_HANDLE_VALUE;
	hErrLogMutex = NULL;
	memset(&ti, 0, sizeof(ti));
}

TMainDlg::~TMainDlg()
{
}

void TMainDlg::SetPathHistory(SetHistMode mode, UINT item)
{
	if (GetCopyMode() == FastCopy::DELETE_MODE) {
		if (!item || item == SRC_COMBO) SetComboBox(SRC_COMBO, cfg.delPathHistory, mode);
	}
	else {
		if (!item || item == SRC_COMBO) SetComboBox(SRC_COMBO, cfg.srcPathHistory, mode);
		if (!item || item == DST_COMBO) SetComboBox(DST_COMBO, cfg.dstPathHistory, mode);
	}
}

void TMainDlg::SetFilterHistory(SetHistMode mode, UINT item)
{
	if (!item || item == INCLUDE_COMBO)  SetComboBox(INCLUDE_COMBO,  cfg.includeHistory,  mode);
	if (!item || item == EXCLUDE_COMBO)  SetComboBox(EXCLUDE_COMBO,  cfg.excludeHistory,  mode);
	if (!item || item == FROMDATE_COMBO) SetComboBox(FROMDATE_COMBO, cfg.fromDateHistory, mode);
	if (!item || item == TODATE_COMBO)   SetComboBox(TODATE_COMBO,   cfg.toDateHistory,   mode);
	if (!item || item == MINSIZE_COMBO)  SetComboBox(MINSIZE_COMBO,  cfg.minSizeHistory,  mode);
	if (!item || item == MAXSIZE_COMBO)  SetComboBox(MAXSIZE_COMBO,  cfg.maxSizeHistory,  mode);
}

void TMainDlg::SetComboBox(UINT item, void **history, SetHistMode mode)
{
	DWORD	len = 0;
	WCHAR	*wbuf = NULL;

	// backup editbox
	if (mode == SETHIST_LIST || mode == SETHIST_CLEAR) {
		len = ::GetWindowTextLengthV(GetDlgItem(item));
		wbuf = new WCHAR [len + 1];
		if (GetDlgItemTextV(item, wbuf, len + 1) != len)
			return;
	}

	// clear listbox & editbox
	SendDlgItemMessage(item, CB_RESETCONTENT, 0, 0);

	// set listbox
	if (mode == SETHIST_LIST) {
		for (int i=0; i < cfg.maxHistory; i++) {
			if (GetChar(history[i], 0))
				SendDlgItemMessageV(item, CB_INSERTSTRING, i, (LPARAM)history[i]);
		}
	}

	// set editbox
	if (mode == SETHIST_EDIT) {
		if (cfg.maxHistory > 0 /* && ::IsWindowEnabled(GetDlgItem(item) */)
			SetDlgItemTextV(item, history[0]);
	}

	// restore editbox
	if (mode == SETHIST_LIST || mode == SETHIST_CLEAR) {
		SetDlgItemTextV(item, wbuf);
	}

	delete [] wbuf;
}

BOOL TMainDlg::SetMiniWindow(void)
{
	GetWindowRect(&rect);
	int height = rect.bottom - rect.top - (isErrEditHide ? 0 : normalHeight - miniHeight);
	isErrEditHide = TRUE;
	MoveWindow(rect.left, rect.top, rect.right - rect.left, height, IsWindowVisible());
	return	TRUE;
}

BOOL TMainDlg::SetNormalWindow(void)
{
	if (IsWindowVisible()) {
		GetWindowRect(&rect);
		int height = rect.bottom - rect.top + (isErrEditHide ? normalHeight - miniHeight : 0);
		isErrEditHide = FALSE;
		MoveWindow(rect.left, rect.top, rect.right - rect.left, height, TRUE);
	}
	return	TRUE;
}

BOOL TMainDlg::MoveCenter()
{
	POINT	pt = {0, 0};
	SIZE	sz = {0, 0};
	::GetCursorPos(&pt);

	BOOL isFixPos  = !IS_INVALID_POINT(cfg.winpos);
	BOOL isFixSize = !IS_INVALID_SIZE(cfg.winsize);

	if (isFixSize) {
		sz.cx = orgRect.right - orgRect.left + cfg.winsize.cx;
		sz.cy = cfg.winsize.cy + (isErrEditHide ? miniHeight : normalHeight);
	}

	RECT	screen_rect = { 0, 0, ::GetSystemMetrics(SM_CXFULLSCREEN),
			::GetSystemMetrics(SM_CYFULLSCREEN) };

	if (MonitorFromPointV && GetMonitorInfoV) {
		HMONITOR	hMon;

		if (isFixPos && !(hMon = ::MonitorFromPointV(cfg.winpos, MONITOR_DEFAULTTONEAREST))) {
			isFixPos = FALSE;
		}
		if (!isFixPos && (hMon = ::MonitorFromPointV(pt, MONITOR_DEFAULTTONEAREST)) != NULL) {
			MONITORINFO	minfo;
			minfo.cbSize = sizeof(minfo);

			if (::GetMonitorInfoV(hMon, &minfo) && (minfo.rcMonitor.right - minfo.rcMonitor.left)
					> 0 && (minfo.rcMonitor.bottom - minfo.rcMonitor.top) > 0) {
				screen_rect = minfo.rcMonitor;
			}
		}
	}

	if (isFixPos) {
		pt = cfg.winpos;
	}
	else {
		pt.x = screen_rect.left + ((screen_rect.right - screen_rect.left)
				- (rect.right - rect.left)) / 2;
		pt.y = screen_rect.top + ((screen_rect.bottom - screen_rect.top)
				- (rect.bottom - rect.top)) / 2;
	}

	return	SetWindowPos(NULL, pt.x, pt.y, sz.cx, sz.cy,
			(isFixSize ? 0 : SWP_NOSIZE)|SWP_NOZORDER|SWP_NOACTIVATE);
}

BOOL TMainDlg::SetupWindow()
{
	static BOOL once = FALSE;

	if (once)
		return TRUE;

	once = TRUE;
	EnableWindow(TRUE);

	if (cfg.isTopLevel)
		SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOMOVE);

	MoveCenter();
	SetItemEnable(GetCopyMode() == FastCopy::DELETE_MODE);

	SetMiniWindow();
	return	TRUE;
}

struct CopyInfo COPYINFO_LIST [] = {
	{ IDS_ALLSKIP,	0, IDS_CMD_NOEXIST_ONLY,0, FastCopy::DIFFCP_MODE,	FastCopy::BY_NAME },
	{ IDS_ATTRCMP,	0, IDS_CMD_DIFF,		0, FastCopy::DIFFCP_MODE,	FastCopy::BY_ATTR },
	{ IDS_UPDATECOPY,0,IDS_CMD_UPDATE,		0, FastCopy::DIFFCP_MODE,	FastCopy::BY_LASTEST },
	{ IDS_FORCECOPY,0, IDS_CMD_FORCE_COPY,	0, FastCopy::DIFFCP_MODE,	FastCopy::BY_ALWAYS },
	{ IDS_SYNCCOPY,	0, IDS_CMD_SYNC,		0, FastCopy::SYNCCP_MODE,	FastCopy::BY_ATTR },
//	{ IDS_MUTUAL,	0, IDS_CMD_MUTUAL,		0, FastCopy::MUTUAL_MODE,	FastCopy::BY_LASTEST },
	{ IDS_MOVEATTR,	0, IDS_CMD_MOVE,		0, FastCopy::MOVE_MODE,		FastCopy::BY_ATTR },
	{ IDS_MOVEFORCE,0, IDS_CMD_MOVE,		0, FastCopy::MOVE_MODE,		FastCopy::BY_ALWAYS },
	{ IDS_DELETE,	0, IDS_CMD_DELETE,		0, FastCopy::DELETE_MODE,	FastCopy::BY_ALWAYS },
	{ 0,			0, 0,					0, (FastCopy::Mode)0,		(FastCopy::OverWrite)0 }
};

BOOL TMainDlg::SetCopyModeList(void)
{
	int	idx = cfg.copyMode;

	if (copyInfo == NULL) {		// 初回コピーモードリスト作成
		for (int i=0; COPYINFO_LIST[i].resId; i++) {
			COPYINFO_LIST[i].list_str = GetLoadStr(COPYINFO_LIST[i].resId);
			COPYINFO_LIST[i].cmdline_name = GetLoadStrV(COPYINFO_LIST[i].cmdline_resId);
		}
		copyInfo = new CopyInfo[sizeof(COPYINFO_LIST) / sizeof(CopyInfo)];
	}
	else {
		idx = (int)SendDlgItemMessage(MODE_COMBO, CB_GETCURSEL, 0, 0);
		SendDlgItemMessage(MODE_COMBO, CB_RESETCONTENT, 0, 0);
	}

	CopyInfo *ci = copyInfo;
	for (int i=0; COPYINFO_LIST[i].resId; i++) {
		if (cfg.enableMoveAttr && COPYINFO_LIST[i].resId == IDS_MOVEFORCE
		|| !cfg.enableMoveAttr && COPYINFO_LIST[i].resId == IDS_MOVEATTR) {
			continue;
		}
		*ci = COPYINFO_LIST[i];
		SendDlgItemMessage(MODE_COMBO, CB_ADDSTRING, 0, (LPARAM)ci->list_str);
		ci++;
	}
	memset(ci, 0, sizeof(CopyInfo));	// terminator

	SendDlgItemMessage(MODE_COMBO, CB_SETCURSEL, idx, 0);
	return	TRUE;
}

BOOL TMainDlg::IsForeground()
{
	HWND hForeWnd = GetForegroundWindow();

	return	hForeWnd && hWnd && (hForeWnd == hWnd || ::IsChild(hWnd, hForeWnd)) ? TRUE : FALSE;
}

BOOL TMainDlg::EvCreate(LPARAM lParam)
{
//	if (IS_WINNT_V && TIsWow64()) {
//		DWORD	val = 0;
//		TWow64DisableWow64FsRedirection(&val);
//	}

	if (IsWinVista() && TIsUserAnAdmin() && TIsEnableUAC()) {
		SetVersionStr(TRUE);
	}

	char	title[100];
	sprintf(title, "%s %s%s", FASTCOPY_TITLE, GetVersionStr(), GetVerAdminStr());

	InstallExceptionFilter(title, GetLoadStr(IDS_EXCEPTIONLOG));

	if (IsWinVista()) {
		HMENU	hMenu = ::GetMenu(hWnd);

		if (!TIsUserAnAdmin()) {
			if (cfg.isRunasButton) {
				HWND	hRunas = GetDlgItem(RUNAS_BUTTON);
				::SetWindowLong(hRunas, GWL_STYLE, ::GetWindowLong(hRunas, GWL_STYLE)|WS_VISIBLE);
				::SendMessage(hRunas, BCM_SETSHIELD, 0, 1);
			}
			else {
				char buf[128], *p;
				int	 len = ::GetMenuString(hMenu, 3, buf, sizeof(buf), MF_BYPOSITION|MF_STRING);
				if ((p = strchr(buf, '('))) {
					*p = 0;
					::ModifyMenu(hMenu, 3, MF_BYPOSITION|MF_STRING, 0, buf);
				}
				::InsertMenuV(hMenu, 4, MF_BYPOSITION|MF_STRING|MF_HELP, ADMIN_MENUITEM,
					GetLoadStrV(IDS_ELEVATE));
			}
		}
		if (strcmpV(cfg.execDirV, cfg.userDirV)) {
			::InsertMenu(::GetSubMenu(hMenu, 0), 0, MF_STRING|MF_BYPOSITION,
				USERDIR_MENUITEM, GetLoadStr(IDS_USERDIR_MENU));
		}
	}

	int		i;
	for (i=0; i < MAX_FASTCOPY_ICON; i++) {
		hMainIcon[i] = ::LoadIcon(TApp::GetInstance(), (LPCSTR)(FASTCOPY_ICON + i));
	}
	::SetClassLong(hWnd, GCL_HICON, (LONG)hMainIcon[FCNORMAL_ICON_IDX]);

	hAccel = LoadAccelerators(TApp::GetInstance(), (LPCSTR)IDR_ACCEL);
	SetSize();

	TaskBarCreateMsg = ::RegisterWindowMessage("TaskbarCreated");

	// メッセージセット
	SetDlgItemText(STATUS_EDIT, GetLoadStr(IDS_BEHAVIOR));
	SetDlgItemInt(BUFSIZE_EDIT, cfg.bufSize);

// 123456789012345678901234567
//	SetDlgItemText(STATUS_EDIT, "1234567890123456789012345678901234567890\r\n2\r\n3\r\n4\r\n5\r\n6\r\n7\r\n8\r\n9\r\n10\r\n11\r\n12");

	SetCopyModeList();
	UpdateMenu();

	CheckDlgButton(IGNORE_CHECK, cfg.ignoreErr);
	CheckDlgButton(ESTIMATE_CHECK, cfg.estimateMode);
	CheckDlgButton(VERIFY_CHECK, cfg.enableVerify);
	CheckDlgButton(TOPLEVEL_CHECK, cfg.isTopLevel);
	CheckDlgButton(OWDEL_CHECK, cfg.enableOwdel);
	CheckDlgButton(ACL_CHECK, cfg.enableAcl && IS_WINNT_V);
	CheckDlgButton(STREAM_CHECK, cfg.enableStream && IS_WINNT_V);

//	SendDlgItemMessage(STATUS_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)EditWordBreakProc);
//	SendDlgItemMessage(ERR_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)EditWordBreakProc);
	SendDlgItemMessage(ERR_EDIT, EM_SETTARGETDEVICE, 0, 0);		// 折り返し
	SendDlgItemMessage(ERR_EDIT, EM_LIMITTEXT, 0, 0);
	SendDlgItemMessage(PATH_EDIT, EM_LIMITTEXT, 0, 0);

	// スライダコントロール
	SendDlgItemMessage(SPEED_SLIDER, TBM_SETRANGE, FALSE, MAKELONG(SPEED_SUSPEND, SPEED_FULL));
	SetSpeedLevelLabel(this, speedLevel = cfg.speedLevel);

#ifdef USE_LISTVIEW
	// リストビュー関連
	listView.AttachWnd(GetDlgItem(MAIN_LIST));
	listHead.AttachWnd((HWND)listView.SendMessage(LVM_GETHEADER, 0, 0));

	DWORD style = listView.SendMessage(LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0) | LVS_EX_GRIDLINES
		| LVS_EX_FULLROWSELECT;
	listView.SendMessage(LVM_SETEXTENDEDLISTVIEWSTYLE, 0, style);

	char	*column[] = { "", "size(MB)", "files (dirs)", NULL };
	LV_COLUMN	lvC;
	memset(&lvC, 0, sizeof(lvC));
	lvC.fmt = LVCFMT_RIGHT;
	lvC.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;

	for (i=0; column[i]; i++) {
		lvC.pszText = column[i];
		lvC.cx = i==0 ? 40 : i == 1 ? 65 : 80;
		listView.SendMessage(LVM_INSERTCOLUMN, i, (LPARAM)&lvC);
	}

	char	*item[] = { "Read", "Write", "Verify", "Skip", "Del"/*, "OW"*/, NULL };
	LV_ITEM	lvi;
	memset(&lvi, 0, sizeof(lvi));
	lvi.mask = LVIF_TEXT|LVIF_PARAM;

	for (i=0; item[i]; i++) {
		lvi.iItem = i;
		lvi.pszText = item[i];
		lvi.iSubItem = 0;
		listView.SendMessage(LVM_INSERTITEM, 0, (LPARAM)&lvi);

		for (int j=1; j < 3; j++) {
			lvi.pszText = j == 1 ? "8,888,888" : "8,888,888 (99,999)";
			lvi.iSubItem = j;
			listView.SendMessage(LVM_SETITEMTEXT, i, (LPARAM)&lvi);
		}
	}
	listView.SendMessage(LVM_SETBKCOLOR, 0, (LPARAM)::GetSysColor(COLOR_3DFACE));
	listView.SendMessage(LVM_SETTEXTBKCOLOR, 0, (LPARAM)::GetSysColor(COLOR_3DFACE));
#endif

	pathEdit.AttachWnd(GetDlgItem(PATH_EDIT));
	errEdit.AttachWnd(GetDlgItem(ERR_EDIT));

	// 履歴セット
	SetPathHistory(SETHIST_EDIT);
//	SetFilterHistory(SETHIST_LIST);

	RECT	path_rect, err_rect;
	GetWindowRect(&rect);
	::GetWindowRect(GetDlgItem(PATH_EDIT), &path_rect);
	::GetWindowRect(GetDlgItem(ERR_EDIT), &err_rect);
	SendDlgItemMessage(PATH_EDIT, EM_SETBKGNDCOLOR, 0, ::GetSysColor(COLOR_3DFACE));
	SendDlgItemMessage(ERR_EDIT, EM_SETBKGNDCOLOR, 0, ::GetSysColor(COLOR_3DFACE));
	normalHeight	= rect.bottom - rect.top;
	miniHeight		= normalHeight - (err_rect.bottom - path_rect.bottom);

	RECT	inc_rect, date_rect;
	::GetWindowRect(GetDlgItem(INCLUDE_COMBO), &inc_rect);
	::GetWindowRect(GetDlgItem(FROMDATE_COMBO), &date_rect);
	filterHeight = date_rect.bottom - inc_rect.bottom;

	SetExtendFilter();

	if (isTaskTray) {
		TaskTray(NIM_ADD, hMainIcon[FCNORMAL_ICON_IDX], FASTCOPY);
	}
	else if (isInstaller) {
		TaskTray(NIM_ADD, hMainIcon[FCNORMAL_ICON_IDX], FASTCOPY);
		TaskTray(NIM_DELETE);
		isInstaller = FALSE;
	}

	// command line mode
	if (orgArgc > 1) {
		// isRunAsParent の場合は、Child側からの CLOSE を待つため、自主終了しない
		if (!CommandLineExecV(orgArgc, orgArgv) && (!isShellExt || autoCloseLevel >= NOERR_CLOSE)
				&& !isRunAsParent) {
			resultStatus = FALSE;
			if (IsForeground() && (::GetAsyncKeyState(VK_SHIFT) & 0x8000))
				autoCloseLevel = NO_CLOSE;
			else
				PostMessage(WM_CLOSE, 0, 0);
		}
	}
	else
		Show();

	return	TRUE;
}

BOOL TMainDlg::EvNcDestroy(void)
{
	TaskTray(NIM_DELETE);
	PostQuitMessage(resultStatus ? 0 : -1);
	return	TRUE;
}

BOOL TMainDlg::CancelCopy()
{
	::KillTimer(hWnd, FASTCOPY_TIMER);

	if (!isDelay) fastCopy.Suspend();

	int	ret = TMsgBox(this).Exec(GetLoadStr(IsListing() ? IDS_LISTCONFIRM :
					info.mode == FastCopy::DELETE_MODE ? IDS_DELSTOPCONFIRM : IDS_STOPCONFIRM),
				FASTCOPY, MB_OKCANCEL);

	if (isDelay) {
		if (ret == IDOK) {
			isDelay = FALSE;
			EndCopy();
		}
		else {
			::SetTimer(hWnd, FASTCOPY_TIMER, FASTCOPY_TIMER_TICK*2, NULL);
		}
	}
	else {
		fastCopy.Resume();

		if (ret == IDOK) {
			isAbort = TRUE;
			fastCopy.Aborting();
		}
		else {
			::SetTimer(hWnd, FASTCOPY_TIMER, FASTCOPY_TIMER_TICK, NULL);
		}
	}

	return	ret == IDOK ? TRUE : FALSE;
}

BOOL TMainDlg::SwapTargetCore(const void *s, const void *d, void *out_s, void *out_d)
{
	void	*src_fname = NULL, *dst_fname = NULL;
	BOOL	isSrcLastBS = GetChar(s, strlenV(s) - 1) == '\\'; // 95系は無視...
	BOOL	isDstLastBS = GetChar(d, strlenV(d) - 1) == '\\';
	BOOL	isSrcRoot = FALSE;

	VBuf	buf(MAX_PATHLEN_V * CHAR_LEN_V);
	VBuf	srcBuf(MAX_PATHLEN_V * CHAR_LEN_V);

	if (!buf.Buf() || !srcBuf.Buf()) return	FALSE;

	if (isSrcLastBS) {
		GetRootDirV(s, buf.Buf());
		if (strcmpV(s, buf.Buf()) == 0) {
			isSrcRoot = TRUE;
		}
		else {
			strcpyV(srcBuf.Buf(), s);
			SetChar(srcBuf.Buf(), strlenV(srcBuf.Buf()) -1, 0);
			s = srcBuf.Buf();
		}
	}

	DWORD	attr;
	BOOL	isSrcDir = isSrcLastBS || (attr = GetFileAttributesV(s)) == 0xffffffff
			|| (attr & FILE_ATTRIBUTE_DIRECTORY)
				&& (!cfg.isReparse || (attr & FILE_ATTRIBUTE_REPARSE_POINT) == 0);

	if (isSrcDir && !isDstLastBS) {	// dst に '\\' がない場合
		strcpyV(out_d, s);
		strcpyV(out_s, d);
		goto END;
	}

	if (!isDstLastBS) {	// dst 末尾に '\\' を付与
		MakePathV(buf.Buf(), d, EMPTY_STR_V);
		d = buf.Buf();
	}

	if (GetFullPathNameV(s, MAX_PATHLEN_V, out_d, &src_fname) <= 0) return	FALSE; // sを out_d に
	if (GetFullPathNameV(d, MAX_PATHLEN_V, out_s, &dst_fname) <= 0) return	FALSE; // dを out_s に

	if (src_fname) {  // a:\aaa\bbb  d:\ccc\ddd\ --> d:\ccc\ddd\bbb a:\aaa\ にする
		strcpyV(MakeAddr(out_s, strlenV(out_s)), src_fname);
		SetChar(src_fname, 0, 0);
		goto END;
	}
	else if (isSrcRoot) {	// a:\  d:\xxx\  -> d:\xxx a:\ にする
		GetRootDirV(out_s, buf.Buf());
		if (strcmpV(out_s, buf.Buf()) && !isSrcRoot) {
			SetChar(out_s, strlenV(out_s) -1, 0);
		}
		goto END;
	} else {
		return	FALSE;
	}

END:
	PathArray	srcArray;
	if (!srcArray.RegisterPath(out_s)) {
		return	FALSE;
	}
	srcArray.GetMultiPath(out_s, MAX_PATHLEN_V);
	return	TRUE;
}

BOOL TMainDlg::SwapTarget(BOOL check_only)
{
	DWORD	src_len = ::GetWindowTextLengthV(GetDlgItem(SRC_COMBO));
	DWORD	dst_len = ::GetWindowTextLengthV(GetDlgItem(DST_COMBO));

	if (src_len == 0 && dst_len == 0) return FALSE;

	void		*src = new WCHAR [MAX_PATHLEN_V];
	void		*dst = new WCHAR [MAX_PATHLEN_V];
	PathArray	srcArray, dstArray;
	BOOL		ret = FALSE;

	if (src && dst && GetDlgItemTextV(SRC_COMBO, src, src_len+1) == src_len
		&& GetDlgItemTextV(DST_COMBO, dst, dst_len+1) == dst_len) {
		if (srcArray.RegisterMultiPath(src) <= 1 && dstArray.RegisterPath(dst) <= 1
			&& !(srcArray.Num() == 0 && dstArray.Num() == 0)) {
			ret = TRUE;
			if (!check_only) {
				if (srcArray.Num() == 0) {
					dstArray.GetMultiPath(src, MAX_PATHLEN_V);
					SetChar(dst, 0, 0);
				}
				else if (dstArray.Num() == 0) {
					SetChar(src, 0, 0);
					strcpyV(dst, srcArray.Path(0));
				}
				else {
					ret = SwapTargetCore(srcArray.Path(0), dstArray.Path(0), src, dst);
				}
				if (ret) {
					SetDlgItemTextV(SRC_COMBO, src);
					SetDlgItemTextV(DST_COMBO, dst);
				}
			}
		}
	}

	delete [] dst;
	delete [] src;
	return	ret;
}

BOOL TMainDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID) {
	case IDOK: case LIST_BUTTON:
		if (!fastCopy.IsStarting() && !isDelay) {
			if ((::GetTickCount() - endTick) > 1000)
				ExecCopy(wID == LIST_BUTTON ? LISTING_EXEC : NORMAL_EXEC);
		}
		else if (CancelCopy()) {
			autoCloseLevel = NO_CLOSE;
		}
		return	TRUE;

	case SRC_COMBO: case DST_COMBO:
		if (wNotifyCode == CBN_DROPDOWN) {
			SetPathHistory(SETHIST_LIST, wID);
		}
		else if (wNotifyCode == CBN_CLOSEUP) {
			PostMessage(WM_FASTCOPY_PATHHISTCLEAR, wID, 0);
		}
		return	TRUE;

	case INCLUDE_COMBO:  case EXCLUDE_COMBO:
	case FROMDATE_COMBO: case TODATE_COMBO:
	case MINSIZE_COMBO:  case MAXSIZE_COMBO:
		if (wNotifyCode == CBN_DROPDOWN) {
			SetFilterHistory(SETHIST_LIST, wID);
		}
		else if (wNotifyCode == CBN_CLOSEUP) {
			PostMessage(WM_FASTCOPY_FILTERHISTCLEAR, wID, 0);
		}
		return	TRUE;

	case IDCANCEL: case CLOSE_MENUITEM:
		if (!fastCopy.IsStarting()) {
			EndDialog(wID);
		}
		else if (fastCopy.IsAborting()) {
			if (TMsgBox(this).Exec(GetLoadStr(IDS_CONFIRMFORCEEND), FASTCOPY, MB_OKCANCEL) == IDOK)
				EndDialog(wID);
		}
		else {
			CancelCopy();
			autoCloseLevel = NOERR_CLOSE;
		}
		return	TRUE;

	case ATONCE_BUTTON:
		isDelay = FALSE;
		::KillTimer(hWnd, FASTCOPY_TIMER);
		ExecCopyCore();
		return	TRUE;

	case SRC_FILE_BUTTON:
		BrowseDirDlgV(this, SRC_COMBO, GetLoadStrV(IDS_SRC_SELECT),
						BRDIR_MULTIPATH|BRDIR_CTRLADD|BRDIR_FILESELECT);
		return	TRUE;

	case DST_FILE_BUTTON:
		BrowseDirDlgV(this, DST_COMBO, GetLoadStrV(IDS_DST_SELECT), BRDIR_BACKSLASH);
		return	TRUE;

	case TOPLEVEL_CHECK:
		cfg.isTopLevel = IsDlgButtonChecked(TOPLEVEL_CHECK);
		SetWindowPos(cfg.isTopLevel ? HWND_TOPMOST :
			HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOMOVE);
		cfg.WriteIni();
		break;

	case MODE_COMBO:
		if (wNotifyCode == CBN_SELCHANGE) {
			BOOL	is_delete = GetCopyMode() == FastCopy::DELETE_MODE;
			if (is_delete && isNetPlaceSrc) {
				SetDlgItemTextV(SRC_COMBO, EMPTY_STR_V);
				isNetPlaceSrc = FALSE;
			}
			SetItemEnable(is_delete);
		}
		return	TRUE;

	case FILTER_CHECK:
		ReflectFilterCheck();
		return	TRUE;

	case OPENDIR_MENUITEM:
		::ShellExecuteV(NULL, NULL, cfg.execDirV, 0, 0, SW_SHOW);
		return	TRUE;

	case OPENLOG_MENUITEM:
		{
			SHELLEXECUTEINFO	sei = {0};
			sei.cbSize = sizeof(SHELLEXECUTEINFO);
			sei.fMask = 0;
			sei.lpVerb = NULL;
			sei.lpFile = (char *)errLogPathV;
			sei.lpParameters = NULL;
			sei.nShow = SW_NORMAL;
			ShellExecuteExV(&sei);
		}
		return	TRUE;

	case USERDIR_MENUITEM:
		::ShellExecuteV(NULL, NULL, cfg.userDirV, 0, 0, SW_SHOW);
		return	TRUE;

	case FIXPOS_MENUITEM:
		if (IS_INVALID_POINT(cfg.winpos)) {
			if (TMsgBox(this).Exec(GetLoadStr(IDS_FIXPOS_MSG), FASTCOPY, MB_OKCANCEL) == IDOK) {
				GetWindowRect(&rect);
				cfg.winpos.x = rect.left;
				cfg.winpos.y = rect.top;
				cfg.WriteIni();
			}
		}
		else {
			cfg.winpos.x = cfg.winpos.y = INVALID_POINTVAL;
			cfg.WriteIni();
		}
		return	TRUE;

	case FIXSIZE_MENUITEM:
		if (IS_INVALID_SIZE(cfg.winsize)) {
			if (TMsgBox(this).Exec(GetLoadStr(IDS_FIXSIZE_MSG), FASTCOPY, MB_OKCANCEL) == IDOK) {
				GetWindowRect(&rect);
				cfg.winsize.cx = (rect.right - rect.left) - (orgRect.right - orgRect.left);
				cfg.winsize.cy = (rect.bottom - rect.top)
									- (isErrEditHide ? miniHeight : normalHeight);
				cfg.WriteIni();
			}
		}
		else {
			cfg.winsize.cx = cfg.winsize.cy = INVALID_SIZEVAL;
			cfg.WriteIni();
		}
		return	TRUE;

	case SETUP_MENUITEM:
		if (setupDlg.Exec() == IDOK) {
			SetCopyModeList();
			SetDlgItemInt(BUFSIZE_EDIT, cfg.bufSize);
			CheckDlgButton(IGNORE_CHECK, cfg.ignoreErr);
			CheckDlgButton(ESTIMATE_CHECK, cfg.estimateMode);
			CheckDlgButton(VERIFY_CHECK, cfg.enableVerify);
			SetSpeedLevelLabel(this, speedLevel = cfg.speedLevel);
			UpdateSpeedLevel();
			CheckDlgButton(OWDEL_CHECK, cfg.enableOwdel);
			CheckDlgButton(ACL_CHECK, cfg.enableAcl);
			CheckDlgButton(STREAM_CHECK, cfg.enableStream);
			isErrLog = cfg.isErrLog;
			isUtf8Log = cfg.isUtf8Log;
			fileLogMode = (FileLogMode)cfg.fileLogMode;
			isReparse = cfg.isReparse;
			skipEmptyDir = cfg.skipEmptyDir;
			forceStart = cfg.forceStart;
			isExtendFilter = cfg.isExtendFilter;
		}
		SetExtendFilter();
		return	TRUE;

	case EXTENDFILTER_MENUITEM:
		isExtendFilter = !isExtendFilter;
		SetExtendFilter();
		return	TRUE;

	case SHELLEXT_MENUITEM:
		shellExtDlg.Exec();
		shextNoConfirm    = cfg.shextNoConfirm;
		shextNoConfirmDel = cfg.shextNoConfirmDel;
		shextTaskTray     = cfg.shextTaskTray;
		shextAutoClose    = cfg.shextAutoClose;
		return	TRUE;

	case ABOUT_MENUITEM:
		aboutDlg.Exec();
		return	TRUE;

	case FASTCOPYURL_MENUITEM:
		::ShellExecuteV(NULL, NULL, GetLoadStrV(IDS_FASTCOPYURL), NULL, NULL, SW_SHOW);
		return	TRUE;

	case AUTODISK_MENUITEM: case SAMEDISK_MENUITEM: case DIFFDISK_MENUITEM:
		diskMode = wID - AUTODISK_MENUITEM;
		UpdateMenu();
		break;

	case IDR_DISKMODE:
		diskMode = (diskMode + 1) % 3;
		UpdateMenu();
		break;

	case JOB_MENUITEM:
		jobDlg.Exec();
		UpdateMenu();
		return	TRUE;

	case SWAPTARGET_MENUITEM:
	case IDR_SWAPTARGET:
		SwapTarget();
		return	TRUE;

	case RUNAS_BUTTON: case ADMIN_MENUITEM:
		if (!fastCopy.IsStarting())
			RunAsAdmin();
		break;

	case HELP_BUTTON: case HELP_MENUITEM:
		ShowHelpV(hWnd, cfg.execDirV, GetLoadStrV(IDS_FASTCOPYHELP),
			wID == HELP_BUTTON ? toV("#usage") : NULL);
		return	TRUE;

	case FINACT_MENUITEM:
		finActDlg.Exec();
		SetFinAct(finActIdx < cfg.finActMax ? finActIdx : 0);
		return	TRUE;

	default:
		if (wID >= JOBOBJ_MENUITEM_START && wID <= JOBOBJ_MENUITEM_END) {
			int idx = wID - JOBOBJ_MENUITEM_START;
			if (idx < cfg.jobMax) {
				SetJob(idx);
			}
			return TRUE;
		}
		if (wID >= FINACT_MENUITEM_START && wID <= FINACT_MENUITEM_END) {
			SetFinAct(wID - FINACT_MENUITEM_START);
			return TRUE;
		}
		break;
	}
	return	FALSE;
}

BOOL TMainDlg::EvSysCommand(WPARAM uCmdType, POINTS pos)
{
	switch (uCmdType)
	{
	case SC_RESTORE: case SC_MAXIMIZE:
		if (cfg.taskbarMode) TaskTray(NIM_DELETE);
		return	TRUE;

	case SC_MINIMIZE:
		PostMessage(WM_FASTCOPY_HIDDEN, 0, 0);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TMainDlg::EventInitMenu(UINT uMsg, HMENU hMenu, UINT uPos, BOOL fSystemMenu)
{
	if (uMsg == WM_INITMENU) {
		::CheckMenuItem(GetSubMenu(hMenu, 0), FIXPOS_MENUITEM,
			MF_BYCOMMAND|(IS_INVALID_POINT(cfg.winpos) ? MF_UNCHECKED : MF_CHECKED));
		::CheckMenuItem(GetSubMenu(hMenu, 0), FIXSIZE_MENUITEM,
			MF_BYCOMMAND|(IS_INVALID_SIZE(cfg.winsize) ? MF_UNCHECKED : MF_CHECKED));
		::EnableMenuItem(GetSubMenu(hMenu, 2), SWAPTARGET_MENUITEM,
			MF_BYCOMMAND|(SwapTarget(TRUE) ? MF_ENABLED : MF_GRAYED));
		return	TRUE;
	}
	return	FALSE;
}

BOOL TMainDlg::EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight)
{
	if (fwSizeType != SIZE_RESTORED && fwSizeType != SIZE_MAXIMIZED)
		return	FALSE;

	RefreshWindow();
	return	TRUE;
}

void TMainDlg::RefreshWindow(BOOL is_start_stop)
{
	if (!copyInfo) return;	// 通常はありえない。

	GetWindowRect(&rect);
	int	xdiff = (rect.right - rect.left) - (orgRect.right - orgRect.left);
	int ydiff = (rect.bottom - rect.top) - (orgRect.bottom - orgRect.top)
		+ (isErrEditHide ? normalHeight - miniHeight : 0) + (isExtendFilter ? 0 : filterHeight);
	int ydiffex = isExtendFilter ? 0 : -filterHeight;

	HDWP	hdwp = ::BeginDeferWindowPos(max_dlgitem);	// MAX item number
	UINT	dwFlg		= SWP_SHOWWINDOW | SWP_NOZORDER;
	UINT	dwHideFlg	= SWP_HIDEWINDOW | SWP_NOZORDER;
	DlgItem	*item;
	int		*target;
	BOOL	is_delete = GetCopyMode() == FastCopy::DELETE_MODE;

	int	slide_y[] = { errstatus_item, errstatic_item, -1 };
	for (target=slide_y; *target != -1; target++) {
		item = dlgItems + *target;
		hdwp = ::DeferWindowPos(hdwp, item->hWnd, 0, item->wpos.x, item->wpos.y + ydiff + ydiffex,
			item->wpos.cx, item->wpos.cy, isErrEditHide ? dwHideFlg : dwFlg);
	}

	item = dlgItems + erredit_item;
	hdwp = ::DeferWindowPos(hdwp, item->hWnd, 0, item->wpos.x, item->wpos.y + ydiff + ydiffex,
		item->wpos.cx + xdiff, item->wpos.cy, isErrEditHide ? dwHideFlg : dwFlg);

	item = dlgItems + path_item;
	hdwp = ::DeferWindowPos(hdwp, item->hWnd, 0, item->wpos.x, item->wpos.y + ydiffex,
		item->wpos.cx + xdiff, item->wpos.cy + ydiff, dwFlg);

	item = dlgItems + filter_item;
	hdwp = ::DeferWindowPos(hdwp, item->hWnd, 0, item->wpos.x + xdiff, item->wpos.y,
		item->wpos.cx, item->wpos.cy, dwFlg);

	int	harf_growslide_x[] = { exccombo_item, excstatic_item, -1 };
	for (target=harf_growslide_x; *target != -1; target++) {
		item = dlgItems + *target;
		hdwp = ::DeferWindowPos(hdwp, item->hWnd, 0, item->wpos.x + xdiff/2, item->wpos.y,
			item->wpos.cx + xdiff/2, item->wpos.cy, dwFlg);
	}

	int	harf_glow_x[] = { inccombo_item, incstatic_item, -1 };
	for (target=harf_glow_x; *target != -1; target++) {
		item = dlgItems + *target;
		hdwp = ::DeferWindowPos(hdwp, item->hWnd, 0, item->wpos.x, item->wpos.y,
			item->wpos.cx + xdiff/2, item->wpos.cy, dwFlg);
	}

	BOOL	is_exec = fastCopy.IsStarting() || isDelay;

	int	slide_x[] = { ok_item, list_item, samedrv_item, top_item, estimate_item, verify_item,
					  speed_item, speedstatic_item, ignore_item, bufedit_item, bufstatic_item,
					  help_item, mode_item, -1 };
	for (target=slide_x; *target != -1; target++) {
		item = dlgItems + *target;
		hdwp = ::DeferWindowPos(hdwp, item->hWnd, 0, item->wpos.x + xdiff,
			item->wpos.y, item->wpos.cx, item->wpos.cy,
			is_exec && (*target == list_item && !IsListing()
			|| *target == ok_item && IsListing()) || is_delete
				&& (*target == estimate_item || *target == verify_item) ? dwHideFlg : dwFlg);
	}

	int	grow_x[] = { srccombo_item, dstcombo_item, -1 };
	for (target=grow_x; *target != -1; target++) {
		item = dlgItems + *target;
		hdwp = ::DeferWindowPos(hdwp, item->hWnd, 0, item->wpos.x, item->wpos.y,
			item->wpos.cx + xdiff, item->wpos.cy, dwFlg);
	}

	item = dlgItems + atonce_item;
	hdwp = ::DeferWindowPos(hdwp, item->hWnd, 0, item->wpos.x, item->wpos.y,
		item->wpos.cx + xdiff, item->wpos.cy, !isDelay ? dwHideFlg : dwFlg);

	item = dlgItems + status_item;
	hdwp = ::DeferWindowPos(hdwp, item->hWnd, 0, item->wpos.x, item->wpos.y,
			item->wpos.cx + xdiff,
			(!isDelay ? dlgItems[atonce_item].wpos.y + dlgItems[atonce_item].wpos.cy
				- item->wpos.y : item->wpos.cy),
			dwFlg);
//	hdwp = ::DeferWindowPos(hdwp, item->hWnd, 0, item->wpos.x, item->wpos.y,
//			item->wpos.cx + xdiff, item->wpos.cy, dwFlg);

	EndDeferWindowPos(hdwp);

	if (is_start_stop) {
		BOOL	flg = !fastCopy.IsStarting();
		::EnableWindow(dlgItems[estimate_item].hWnd, flg);
		::EnableWindow(dlgItems[verify_item].hWnd, flg);
		::EnableWindow(dlgItems[ignore_item].hWnd, flg);
		::EnableWindow(dlgItems[owdel_item].hWnd, flg);
		::EnableWindow(dlgItems[acl_item].hWnd, flg && IS_WINNT_V);
		::EnableWindow(dlgItems[stream_item].hWnd, flg && IS_WINNT_V);
	}
}

void TMainDlg::SetSize(void)
{
	SetDlgItem(SRC_FILE_BUTTON);
	SetDlgItem(DST_FILE_BUTTON);
	SetDlgItem(SRC_COMBO);
	SetDlgItem(DST_COMBO);
	SetDlgItem(STATUS_EDIT);
	SetDlgItem(MODE_COMBO);
	SetDlgItem(BUF_STATIC);
	SetDlgItem(BUFSIZE_EDIT);
	SetDlgItem(HELP_BUTTON);
	SetDlgItem(IGNORE_CHECK);
	SetDlgItem(ESTIMATE_CHECK);
	SetDlgItem(VERIFY_CHECK);
	SetDlgItem(TOPLEVEL_CHECK);
	SetDlgItem(LIST_BUTTON);
	SetDlgItem(IDOK);
	SetDlgItem(ATONCE_BUTTON);
	SetDlgItem(OWDEL_CHECK);
	SetDlgItem(ACL_CHECK);
	SetDlgItem(STREAM_CHECK);
	SetDlgItem(SPEED_SLIDER);
	SetDlgItem(SPEED_STATIC);
	SetDlgItem(SAMEDRV_STATIC);
	SetDlgItem(INC_STATIC);
	SetDlgItem(EXC_STATIC);
	SetDlgItem(INCLUDE_COMBO);
	SetDlgItem(EXCLUDE_COMBO);
	SetDlgItem(FILTER_CHECK);
	SetDlgItem(PATH_EDIT);
	SetDlgItem(ERR_STATIC);
	SetDlgItem(ERRSTATUS_STATIC);
	SetDlgItem(ERR_EDIT);

	GetWindowRect(&orgRect);
}

/*
	DropFiles Event CallBack
*/
BOOL TMainDlg::EvDropFiles(HDROP hDrop)
{
	PathArray	pathArray;
	WCHAR	path[MAX_PATH_EX];
	BOOL	isDstDrop = IsDestDropFiles(hDrop);
	BOOL	isDeleteMode = GetCopyMode() == FastCopy::DELETE_MODE;
	int		max = isDstDrop ? 1 : ::DragQueryFileV(hDrop, 0xffffffff, 0, 0), max_len;

	// CTL が押されている場合、現在の内容を加算
	if (!isDstDrop) {
		if (::GetAsyncKeyState(VK_CONTROL) & 0x8000) {
			max_len = ::GetWindowTextLengthV(GetDlgItem(SRC_COMBO)) + 1;
			WCHAR	*buf = new WCHAR [max_len];
			GetDlgItemTextV(SRC_COMBO, buf, max_len);
			pathArray.RegisterMultiPath(buf);
			delete [] buf;
		}
		else
			isNetPlaceSrc = FALSE;
	}

	for (int i=0; i < max; i++) {
		if (::DragQueryFileV(hDrop, i, path, sizeof(path) / CHAR_LEN_V) <= 0)
			break;

		if (isDstDrop || !isDeleteMode) {
			if (NetPlaceConvertV(path, path) && !isDstDrop)
				isNetPlaceSrc = TRUE;
		}
		pathArray.RegisterPath(path);
	}
	::DragFinish(hDrop);

	if (pathArray.Num() > 0) {
		if (isDstDrop) {
			DWORD	attr = ::GetFileAttributesV(pathArray.Path(0));
			if (attr & FILE_ATTRIBUTE_DIRECTORY) {	// 0xffffffff も認める(for 95系OSのroot対策)
				MakePathV(path, pathArray.Path(0), EMPTY_STR_V);
				SetDlgItemTextV(DST_COMBO, path);
			}
		}
		else {
			WCHAR	*buf = new WCHAR [max_len = pathArray.GetMultiPathLen()]; // 文字列連結用領域
			if (pathArray.GetMultiPath(buf, max_len)) {
				SetDlgItemTextV(SRC_COMBO, buf);
			}
			delete [] buf;
		}
	}
	return	TRUE;
}

void TMainDlg::SetPriority(DWORD new_class)
{
	if (curPriority != new_class) {
		::SetPriorityClass(::GetCurrentProcess(), curPriority = new_class);
		if (IsWinVista()) {
			::SetPriorityClass(::GetCurrentProcess(), curPriority == IDLE_PRIORITY_CLASS ?
								PROCESS_MODE_BACKGROUND_BEGIN : PROCESS_MODE_BACKGROUND_END);
		}
	}
}

BOOL TMainDlg::EventScroll(UINT uMsg, int Code, int nPos, HWND hwndScrollBar)
{
	speedLevel = SetSpeedLevelLabel(this);
	UpdateSpeedLevel();

	return	TRUE;
}

FastCopy::Mode TMainDlg::GetCopyMode(void)
{
	if (!copyInfo) return FastCopy::DIFFCP_MODE;	// 通常はありえない。

	return	copyInfo[SendDlgItemMessage(MODE_COMBO, CB_GETCURSEL, 0, 0)].mode;
}

void TMainDlg::SetExtendFilter()
{
	static BOOL lastExtendFilter = TRUE;

	if (lastExtendFilter != isExtendFilter) {
		int mode = isExtendFilter ? SW_SHOW : SW_HIDE;

		::ShowWindow(GetDlgItem(FROMDATE_STATIC), mode);
		::ShowWindow(GetDlgItem(TODATE_STATIC),   mode);
		::ShowWindow(GetDlgItem(MINSIZE_STATIC),  mode);
		::ShowWindow(GetDlgItem(MAXSIZE_STATIC),  mode);
		::ShowWindow(GetDlgItem(FROMDATE_COMBO),  mode);
		::ShowWindow(GetDlgItem(TODATE_COMBO),    mode);
		::ShowWindow(GetDlgItem(MINSIZE_COMBO),   mode);
		::ShowWindow(GetDlgItem(MAXSIZE_COMBO),   mode);

		GetWindowRect(&rect);
		int height = rect.bottom - rect.top + (isExtendFilter ? filterHeight : -filterHeight);
		MoveWindow(rect.left, rect.top, rect.right - rect.left, height, IsWindowVisible());
		if (IsWindowVisible()) RefreshWindow();

		lastExtendFilter = isExtendFilter;
		UpdateMenu();
	}
}

void TMainDlg::ReflectFilterCheck(BOOL is_invert)
{
	BOOL	is_show_filter = TRUE;
	BOOL	is_checked = IsDlgButtonChecked(FILTER_CHECK);
	BOOL	new_status = (is_invert ? !is_checked : is_checked) && is_show_filter;

//	::ShowWindow(GetDlgItem(FILTER_CHECK), is_show_filter ? SW_SHOW : SW_HIDE);
//	::ShowWindow(GetDlgItem(INCLUDE_COMBO), is_show_filter ? SW_SHOW : SW_HIDE);
//	::ShowWindow(GetDlgItem(EXCLUDE_COMBO), is_show_filter ? SW_SHOW : SW_HIDE);
//	if (is_show_filter)
		CheckDlgButton(FILTER_CHECK, new_status);

	::EnableWindow(GetDlgItem(FILTER_CHECK), is_show_filter);
	::EnableWindow(GetDlgItem(INCLUDE_COMBO),  new_status);
	::EnableWindow(GetDlgItem(EXCLUDE_COMBO),  new_status);
	::EnableWindow(GetDlgItem(FROMDATE_COMBO), new_status);
	::EnableWindow(GetDlgItem(TODATE_COMBO),   new_status);
	::EnableWindow(GetDlgItem(MINSIZE_COMBO),  new_status);
	::EnableWindow(GetDlgItem(MAXSIZE_COMBO),  new_status);
}

BOOL TMainDlg::IsDestDropFiles(HDROP hDrop)
{
	POINT	pt;
	RECT	dst_rect;

	if (!::DragQueryPoint(hDrop, &pt) || !::ClientToScreen(hWnd, &pt)) {
		if (!::GetCursorPos(&pt))
			return	FALSE;
	}

	if (!::GetWindowRect(GetDlgItem(DST_COMBO), &dst_rect) || !PtInRect(&dst_rect, pt))
		return	FALSE;

	return	TRUE;
}

_int64 TMainDlg::GetDateInfo(void *buf, BOOL is_end)
{
	const void	*p = NULL;
	FILETIME	ft;
	SYSTEMTIME	st;
	_int64		&t = *(_int64 *)&ft;
	_int64		val;

	val = strtolV(buf, &p, 10);

	if (val > 0 && !strchrV(buf, '+')) {	// absolute
		if (val < 16010102) return -1;
		memset(&st, 0, sizeof(st));
		st.wYear	= (WORD) (val / 10000);
		st.wMonth	= (WORD)((val / 100) % 100);
		st.wDay		= (WORD) (val % 100);
		if (is_end) {
			st.wHour			= 23;
			st.wMinute			= 59;
			st.wSecond			= 59;
			st.wMilliseconds	= 999;
		}
		FILETIME	lt;
		if (!::SystemTimeToFileTime(&st, &lt)) return -1;
		if (!::LocalFileTimeToFileTime(&lt, &ft)) return -1;
	}
	else if (p && GetChar(p, 0)) {
		::GetSystemTime(&st);
		::SystemTimeToFileTime(&st, &ft);
		switch (GetChar(p, 0)) {
		case 'W': val *= 60 * 60 * 24 * 7;	break;
		case 'D': val *= 60 * 60 * 24;		break;
		case 'h': val *= 60 * 60;			break;
		case 'm': val *= 60;				break;
		case 's':							break;
		default: return	-1;
		}
		t += val * 10000000;
	}
	else return -1;

	return	t;
}

_int64 TMainDlg::GetSizeInfo(void *buf)
{
	const void	*p=NULL;
	_int64		val;

	if ((val = strtolV(buf, &p, 0)) < 0) return -2;

	if (val == 0 && p == buf) {	// 変換すべき数字が無い
		for (int i=0; GetChar(p, i); i++) {
			if (!strchr(" \t\r\n", GetChar(p, i))) return -2;
		}
		return	-1;
	}
	int c = p ? GetChar(p, 0) : ' ';

	switch (toupper(c)) {
	case 'T': val *= (_int64)1024 * 1024 * 1024 * 1024;	break;
	case 'G': val *= (_int64)1024 * 1024 * 1024;		break;
	case 'M': val *= (_int64)1024 * 1024;				break;
	case 'K': val *= (_int64)1024;						break;
	case ' ': case 0:	break;
	default: return	-2;
	}

	return	val;
}

BOOL TMainDlg::ExecCopy(DWORD exec_flags)
{
	int		idx = (int)SendDlgItemMessage(MODE_COMBO, CB_GETCURSEL, 0, 0);
	BOOL	is_delete_mode = copyInfo[idx].mode == FastCopy::DELETE_MODE;
	BOOL	is_filter = IsDlgButtonChecked(FILTER_CHECK);
	BOOL	is_listing = (exec_flags & LISTING_EXEC) ? TRUE : FALSE;
	BOOL	is_initerr_logging = (noConfirmStop && !is_listing) ? TRUE : FALSE;
	BOOL	is_fore = IsForeground();

	// コピーと同じように、削除も排他実行する
	if (is_delete_mode && is_fore && (::GetAsyncKeyState(VK_SHIFT) & 0x8000)) forceStart = 2;

	info.ignoreEvent	= (IsDlgButtonChecked(IGNORE_CHECK) ? FASTCOPY_ERROR_EVENT : 0) |
							(noConfirmStop ? FASTCOPY_STOP_EVENT : 0);
	info.mode			= copyInfo[idx].mode;
	info.overWrite		= copyInfo[idx].overWrite;
	info.isPhysLock		= IS_WINNT_V ? TRUE : FALSE;
	info.lcid			= cfg.lcid > 0 && IS_WINNT_V ? ::GetThreadLocale() : 0;
	info.flags			= cfg.copyFlags
//		| (FastCopy::REPORT_ACL_ERROR | FastCopy::REPORT_STREAM_ERROR)
		| (is_listing ? FastCopy::LISTING_ONLY : 0)
		| (!is_listing && fileLogMode != NO_FILELOG ? FastCopy::LISTING : 0)
		| (IsDlgButtonChecked(ACL_CHECK) && IS_WINNT_V ? FastCopy::WITH_ACL : 0)
		| (IsDlgButtonChecked(STREAM_CHECK) && IS_WINNT_V ? FastCopy::WITH_ALTSTREAM : 0)
		| (cfg.aclErrLog && IS_WINNT_V ? FastCopy::REPORT_ACL_ERROR : 0)
		| (cfg.streamErrLog && IS_WINNT_V ? FastCopy::REPORT_STREAM_ERROR : 0)
		| (!is_delete_mode && IsDlgButtonChecked(ESTIMATE_CHECK) && !is_listing ?
			FastCopy::PRE_SEARCH : 0)
		| ((!is_listing && IsDlgButtonChecked(VERIFY_CHECK)) ? FastCopy::VERIFY_FILE : 0)
		| (is_delete_mode && IsDlgButtonChecked(OWDEL_CHECK) ? cfg.enableNSA ?
			FastCopy::OVERWRITE_DELETE_NSA : FastCopy::OVERWRITE_DELETE : 0)
		| (is_delete_mode && cfg.delDirWithFilter ? FastCopy::DELDIR_WITH_FILTER : 0)
		| (cfg.isSameDirRename ? FastCopy::SAMEDIR_RENAME : 0)
		| (cfg.isAutoSlowIo ? FastCopy::AUTOSLOW_IOLIMIT : 0)
		| (cfg.isReadOsBuf ? FastCopy::USE_OSCACHE_READ : 0)
		| (cfg.usingMD5 ? FastCopy::VERIFY_MD5 : 0) // default is SHA-1
		| (skipEmptyDir && is_filter ? FastCopy::SKIP_EMPTYDIR : 0)
		| ((!isReparse && IS_WINNT_V)
			&& info.mode != FastCopy::MOVE_MODE
			&& info.mode != FastCopy::DELETE_MODE ?
			(FastCopy::FILE_REPARSE|FastCopy::DIR_REPARSE) : 0)
		| ((is_listing && is_fore && (::GetAsyncKeyState(VK_CONTROL) & 0x8000))
			? FastCopy::VERIFY_FILE : 0)
		| (info.mode == FastCopy::MOVE_MODE && cfg.serialMove ? FastCopy::SERIAL_MOVE : 0)
		| (info.mode == FastCopy::MOVE_MODE && cfg.serialVerifyMove ?
			FastCopy::SERIAL_VERIFY_MOVE : 0)
		| (isLinkDest ? FastCopy::RESTORE_HARDLINK : 0)
		| (isReCreate ? FastCopy::DEL_BEFORE_CREATE : 0)
		| (diskMode == 0 ? 0 : diskMode == 1 ? FastCopy::FIX_SAMEDISK : FastCopy::FIX_DIFFDISK);
	info.bufSize		= GetDlgItemInt(BUFSIZE_EDIT) * 1024 * 1024;
	info.maxTransSize	= cfg.maxTransSize * 1024 * 1024;
	info.maxOpenFiles	= cfg.maxOpenFiles;
	info.maxAttrSize	= cfg.maxAttrSize;
	info.maxDirSize		= cfg.maxDirSize;
	info.maxLinkHash	= maxLinkHash;
	info.allowContFsize	= cfg.allowContFsize;
	info.nbMinSizeNtfs	= cfg.nbMinSizeNtfs * 1024;
	info.nbMinSizeFat	= cfg.nbMinSizeFat * 1024;
	info.hNotifyWnd		= hWnd;
	info.uNotifyMsg		= WM_FASTCOPY_MSG;
	info.fromDateFilter	= 0;
	info.toDateFilter	= 0;
	info.minSizeFilter	= -1;
	info.maxSizeFilter	= -1;
	strcpy(info.driveMap, cfg.driveMap);

	errBufOffset		= 0;
	listBufOffset		= 0;
	lastTotalSec		= 0;
	calcTimes			= 0;

	memset(&ti, 0, sizeof(ti));
	isAbort = FALSE;

	resultStatus = FALSE;
	int		src_len = ::GetWindowTextLengthV(GetDlgItem(SRC_COMBO)) + 1;
	int		dst_len = ::GetWindowTextLengthV(GetDlgItem(DST_COMBO)) + 1;
	if (src_len <= 1 || !is_delete_mode && dst_len <= 1) {
		return	FALSE;
	}

	WCHAR	*src = new WCHAR [src_len], dst[MAX_PATH_EX] = L"";
	BOOL	ret = TRUE;
	BOOL	exec_confirm = cfg.execConfirm ||
										is_fore && (::GetAsyncKeyState(VK_CONTROL) & 0x8000);

	if (!exec_confirm) {
		if (isShellExt && (exec_flags & CMDLINE_EXEC))
			exec_confirm = is_delete_mode ? !shextNoConfirmDel : !shextNoConfirm;
		else
			exec_confirm = is_delete_mode ? !noConfirmDel : cfg.execConfirm;
	}

	if (GetDlgItemTextV(SRC_COMBO, src, src_len) == 0
	|| !is_delete_mode && GetDlgItemTextV(DST_COMBO, dst, MAX_PATH_EX) == 0) {
		TMsgBox(this).Exec("Can't get src or dst field");
		ret = FALSE;
	}
	SendDlgItemMessage(PATH_EDIT, WM_SETTEXT, 0, (LPARAM)"");
	SendDlgItemMessage(STATUS_EDIT, WM_SETTEXT, 0, (LPARAM)"");
	SendDlgItemMessage(STATUS_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)EditWordBreakProc);
	SendDlgItemMessage(ERRSTATUS_STATIC, WM_SETTEXT, 0, (LPARAM)"");

	PathArray	srcArray, dstArray, incArray, excArray;
	srcArray.RegisterMultiPath(src);
	if (!is_delete_mode)
		dstArray.RegisterPath(dst);

	// フィルタ
	WCHAR	from_date[MINI_BUF]=L"", to_date[MINI_BUF]=L"";
	WCHAR	min_size[MINI_BUF]=L"", max_size[MINI_BUF]=L"";
	WCHAR	*inc = NULL, *exc = NULL;
	if (is_filter) {
		DWORD	inc_len = ::GetWindowTextLengthV(GetDlgItem(INCLUDE_COMBO));
		DWORD	exc_len = ::GetWindowTextLengthV(GetDlgItem(EXCLUDE_COMBO));
		inc = new WCHAR [inc_len + 1];
		exc = new WCHAR [exc_len + 1];
		if (GetDlgItemTextV(INCLUDE_COMBO, inc, inc_len +1) == inc_len &&
			GetDlgItemTextV(EXCLUDE_COMBO, exc, exc_len +1) == exc_len) {
			incArray.RegisterMultiPath(inc);
			excArray.RegisterMultiPath(exc);
		}
		else ret = FALSE;

		if (isExtendFilter) {
			if (GetDlgItemTextV(FROMDATE_COMBO, from_date, MINI_BUF) > 0)
				info.fromDateFilter = GetDateInfo(from_date, FALSE);
			if (GetDlgItemTextV(TODATE_COMBO,   to_date,   MINI_BUF) > 0)
				info.toDateFilter   = GetDateInfo(to_date, TRUE);
			if (GetDlgItemTextV(MINSIZE_COMBO,  min_size,  MINI_BUF) > 0)
				info.minSizeFilter  = GetSizeInfo(min_size);
			if (GetDlgItemTextV(MAXSIZE_COMBO,  max_size,  MINI_BUF) > 0)
				info.maxSizeFilter  = GetSizeInfo(max_size);

			if (info.fromDateFilter == -1 || info.toDateFilter  == -1) {
				TMsgBox(this).Exec(GetLoadStr(IDS_DATEFORMAT_MSG));
				ret = FALSE;
			}
			if (info.minSizeFilter  == -2 || info.maxSizeFilter == -2) {
				TMsgBox(this).Exec(GetLoadStr(IDS_SIZEFORMAT_MSG));
				ret = FALSE;
			}
		}
	}

	// 確認用ファイル一覧
	if (!ret
	|| !(ret = fastCopy.RegisterInfo(&srcArray, &dstArray, &info, &incArray, &excArray))) {
		SetDlgItemText(STATUS_EDIT, "Error");
	}

	int	src_list_len = srcArray.GetMultiPathLen();
	void *src_list = new WCHAR [src_list_len];

	if (ret && exec_confirm && (exec_flags & LISTING_EXEC) == 0) {
		srcArray.GetMultiPath(src_list, src_list_len, NEWLINE_STR_V, EMPTY_STR_V);
		void	*title =
					info.mode == FastCopy::MOVE_MODE   ? GetLoadStrV(IDS_MOVECONFIRM) :
					info.mode == FastCopy::SYNCCP_MODE ? GetLoadStrV(IDS_SYNCCONFIRM) :
					info.isRenameMode ? GetLoadStrV(IDS_DUPCONFIRM): NULL;
		int		sv_flags = info.flags;	// delete confirm で変化しないか確認

		switch (TExecConfirmDlg(&info, &cfg, this, title, isShellExt).Exec(src_list,
				is_delete_mode ? NULL : dst)) {
		case IDOK:
			break;

		case RUNAS_BUTTON:
			ret = FALSE;
			RunAsAdmin(RUNAS_IMMEDIATE);
			break;

		default:
			ret = FALSE;
			if (isShellExt && !is_delete_mode)
				autoCloseLevel = NO_CLOSE;
			break;
		}

		if (ret && is_delete_mode && info.flags != sv_flags) {
			// flag が変化していた場合は、再登録
			ret = fastCopy.RegisterInfo(&srcArray, &dstArray, &info, &incArray, &excArray);
		}
	}

	if (ret) {
		if (is_delete_mode ? cfg.EntryDelPathHistory(src) : cfg.EntryPathHistory(src, dst)) {
		//	SetPathHistory(SETHIST_LIST);
		}
		if (is_filter) {
			cfg.EntryFilterHistory(inc, exc, from_date, to_date, min_size, max_size);
		//	SetFilterHistory(SETHIST_LIST);
		}
		cfg.WriteIni();
	}

	int	pathLogMax = src_len * CHAR_LEN_V + sizeof(dst);

	if ((ret || is_initerr_logging) && (pathLogBuf = new char [pathLogMax])) {
		int len = sprintf(pathLogBuf, "<Source>  %s", isUtf8Log ? WtoU8(src) : IS_WINNT_V ?
					WtoA(src) : (char *)src);
		if (!is_delete_mode) {
			len += sprintf(pathLogBuf + len, "\r\n<DestDir> %s", isUtf8Log ?
					WtoU8(dst) : IS_WINNT_V ? WtoA(dst) : (char *)dst);
		}
		if (inc && GetChar(inc, 0)) {
			len += sprintf(pathLogBuf + len, "\r\n<Include> %s", isUtf8Log ?
					WtoU8(inc) : IS_WINNT_V ? WtoA(inc) : (char *)inc);
		}
		if (exc && GetChar(exc, 0)) {
			len += sprintf(pathLogBuf + len, "\r\n<Exclude> %s", isUtf8Log ?
					WtoU8(exc) : IS_WINNT_V ? WtoA(exc) : (char *)exc);
		}
		len += sprintf(pathLogBuf + len, "\r\n<Command> %s", isUtf8Log ?
				AtoU8(copyInfo[idx].list_str) : copyInfo[idx].list_str);
		if (info.flags & (FastCopy::WITH_ACL|FastCopy::WITH_ALTSTREAM|FastCopy::OVERWRITE_DELETE
						 |FastCopy::OVERWRITE_DELETE_NSA|FastCopy::VERIFY_FILE)) {
			len += sprintf(pathLogBuf + len, " (with");
			if (!is_delete_mode && (info.flags & FastCopy::VERIFY_FILE))
				len += sprintf(pathLogBuf + len, " Verify");
			if (!is_delete_mode && (info.flags & FastCopy::WITH_ACL))
				len += sprintf(pathLogBuf + len, " ACL");
			if (!is_delete_mode && (info.flags & FastCopy::WITH_ALTSTREAM))
				len += sprintf(pathLogBuf + len, " AltStream");
			if (is_delete_mode && (info.flags & FastCopy::OVERWRITE_DELETE))
				len += sprintf(pathLogBuf + len, " OverWrite");
			if (is_delete_mode && (info.flags & FastCopy::OVERWRITE_DELETE_NSA))
				len += sprintf(pathLogBuf + len, " NSA");
			len += sprintf(pathLogBuf + len, ")");
		}
		len += sprintf(pathLogBuf + len , "\r\n");
	}

	delete [] exc;
	delete [] inc;
	delete [] src_list;
	delete [] src;

	if (ret) {
		SendDlgItemMessage(PATH_EDIT, EM_SETTARGETDEVICE, 0, IsListing() ? 1 : 0);	// 折り返し
		SetMiniWindow();
		SetDlgItemText(ERR_EDIT, "");

		if (forceStart == 1 || forceStart == 0 && is_delete_mode || fastCopy.TakeExclusivePriv()){
			if (forceStart == 1 && !is_delete_mode) {
				fastCopy.TakeExclusivePriv();
			}
			ret = ExecCopyCore();
		}
		else {
			isDelay = TRUE;
			::SetTimer(hWnd, FASTCOPY_TIMER, FASTCOPY_TIMER_TICK*2, NULL);
			if (isTaskTray) {
				TaskTray(NIM_MODIFY, hMainIcon[FCWAIT_ICON_IDX], FASTCOPY);
			}
		}

		if (ret) {
			SetDlgItemText(IsListing() ? LIST_BUTTON : IDOK, GetLoadStr(IDS_CANCEL));
			RefreshWindow(TRUE);
			if (IsWinVista() && !TIsUserAnAdmin()) {
				::EnableMenuItem(GetMenu(hWnd), ADMIN_MENUITEM,
					MF_BYCOMMAND|MF_DISABLED|MF_GRAYED);
				::DrawMenuBar(hWnd);
			}
		}
	}

	if (!ret) {
		if (pathLogBuf) {
			if (is_initerr_logging) {
				WriteErrLog(TRUE);
			}
			delete [] pathLogBuf;
			pathLogBuf = NULL;
		}
		CheckVerifyExtension();
	}

	return	ret;
}

BOOL TMainDlg::ExecCopyCore(void)
{
	::GetLocalTime(&startTm);
	StartFileLog();

	BOOL ret = fastCopy.Start(&ti);
	if (ret) {
		RefreshWindow();
		timerCnt = timerLast = 0;
		::GetCursorPos(&curPt);
		::SetTimer(hWnd, FASTCOPY_TIMER, FASTCOPY_TIMER_TICK, NULL);
		UpdateSpeedLevel();
	}
	return	ret;
}

BOOL TMainDlg::EndCopy(void)
{
	SetPriority(NORMAL_PRIORITY_CLASS);
	::KillTimer(hWnd, FASTCOPY_TIMER);

	BOOL	is_starting = fastCopy.IsStarting();

	if (is_starting) {
		SetInfo(TRUE);

		if (ti.total.errFiles == 0 && ti.total.errDirs == 0) {
			resultStatus = TRUE;
		}

		if (!IsListing()) {
			if (isErrLog) WriteErrLog();
			if (fileLogMode != NO_FILELOG) EndFileLog();
		}
		::EnableWindow(GetDlgItem(IsListing() ? LIST_BUTTON : IDOK), FALSE);
		fastCopy.End();
		::EnableWindow(GetDlgItem(IsListing() ? LIST_BUTTON : IDOK), TRUE);
	}
	else {
		SendDlgItemMessage(STATUS_EDIT, WM_SETTEXT, 0, (LPARAM)" ---- Cancelled. ----");
	}

	delete [] pathLogBuf;
	pathLogBuf = NULL;

	SetDlgItemText(IsListing() ? LIST_BUTTON : IDOK, GetLoadStr(IsListing() ?
		IDS_LISTING : IDS_EXECUTE));
	SetDlgItemText(SAMEDRV_STATIC, "");

	BOOL	is_auto_close = autoCloseLevel == FORCE_CLOSE
							|| autoCloseLevel == NOERR_CLOSE
								&& (!is_starting || (ti.total.errFiles == 0 &&
									ti.total.errDirs == 0 && errBufOffset == 0));

	if (!IsListing()) {
		if (is_starting && !isAbort) {
			ExecFinalAction(is_auto_close);
		}
		if (is_auto_close) {
			PostMessage(WM_CLOSE, 0, 0);
		}
		autoCloseLevel = NO_CLOSE;
		isShellExt = FALSE;
	}
	RefreshWindow(TRUE);
	UpdateMenu();
	CheckVerifyExtension();

	SetFocus(GetDlgItem(IsListing() ? LIST_BUTTON : IDOK));

/*	if (isTaskTray) {
		TaskTray(NIM_MODIFY, hMainIcon[curIconIndex = FCNORMAL_ICON_IDX], FASTCOPY);
	}
*/
	if (IsWinVista() && !TIsUserAnAdmin()) {
		::EnableMenuItem(GetMenu(hWnd), ADMIN_MENUITEM, MF_BYCOMMAND|MF_ENABLED);
		::DrawMenuBar(hWnd);
	}

	return	TRUE;
}

BOOL TMainDlg::ExecFinalAction(BOOL is_sound_wait)
{
	if (finActIdx < 0) return FALSE;

	FinAct	*act = cfg.finActArray[finActIdx];
	BOOL	is_err = ti.total.errFiles || ti.total.errDirs;

	if (GetChar(act->sound, 0) && (!(act->flags & FinAct::ERR_SOUND) || is_err)) {
		PlaySoundV(act->sound, 0, SND_FILENAME|(is_sound_wait ? 0 : SND_ASYNC));
	}

	int	flg = (act->flags & FinAct::ERR_CMD)    ? FinAct::ERR_CMD    :
			  (act->flags & FinAct::NORMAL_CMD) ? FinAct::NORMAL_CMD : 0;

	if (GetChar(act->command, 0) &&
		(flg == 0 || flg == FinAct::NORMAL_CMD && !is_err || flg == FinAct::ERR_CMD && is_err)) {
		PathArray	pathArray;
		BOOL		is_wait = (act->flags & FinAct::WAIT_CMD) ? TRUE : FALSE;

		pathArray.SetMode(PathArray::ALLOW_SAME | PathArray::NO_REMOVE_QUOTE);
		pathArray.RegisterMultiPath(act->command);

		for (int i=0; i < pathArray.Num(); i++) {
			PathArray	path;
			PathArray	param;

			path.SetMode(PathArray::ALLOW_SAME | PathArray::NO_REMOVE_QUOTE);
			path.RegisterMultiPath(pathArray.Path(i), L" ");

			param.SetMode(PathArray::ALLOW_SAME);
			for (int j=1; j < path.Num(); j++) param.RegisterPath(path.Path(j));

			WCHAR *param_str = new WCHAR [param.GetMultiPathLen()];
			param.GetMultiPath(param_str, param.GetMultiPathLen(), L" ");

			SHELLEXECUTEINFO	sei = {0};
			sei.cbSize = sizeof(SHELLEXECUTEINFO);
			sei.fMask = is_wait ? SEE_MASK_NOCLOSEPROCESS : 0;
			sei.lpVerb = NULL;
			sei.lpFile = (char *)path.Path(0);
			sei.lpParameters = (char *)param_str;
			sei.lpDirectory = NULL;
			sei.nShow = SW_NORMAL;
			if (ShellExecuteExV(&sei) && is_wait && sei.hProcess) {
				char	buf[512], svbuf[512];
				GetDlgItemText(PATH_EDIT, svbuf, 512);
				_snprintf(buf, sizeof(buf), "%s\r\nWait for finish action...", svbuf);
				SetDlgItemText(PATH_EDIT, buf);
				EnableWindow(FALSE);
				while (::WaitForSingleObject(sei.hProcess, 100) == WAIT_TIMEOUT) {
					while (Idle())
						;
				}
				::CloseHandle(sei.hProcess);
				EnableWindow(TRUE);
				SetDlgItemText(PATH_EDIT, svbuf);
			//	SetForegroundWindow();
			}
			delete [] param_str;
		}
	}

	if ((act->flags & (FinAct::SUSPEND|FinAct::HIBERNATE|FinAct::SHUTDOWN))
	&& act->shutdownTime >= 0 && (!is_err || (act->flags & FinAct::ERR_CMD) == 0)) {
		TFinDlg	finDlg(this);
		BOOL	is_force = (act->flags & FinAct::FORCE) ? TRUE : FALSE;
		DWORD	msg_id = (act->flags & FinAct::SUSPEND) ? IDS_SUSPEND_MSG :
						(act->flags & FinAct::HIBERNATE) ? IDS_HIBERNATE_MSG : IDS_SHUTDOWN_MSG;

		if (act->shutdownTime == 0 || finDlg.Exec(act->shutdownTime, msg_id) == IDOK) {
			TSetPrivilege(SE_SHUTDOWN_NAME, TRUE);

			if (act->flags & FinAct::SUSPEND) {
				::SetSystemPowerState(TRUE, is_force);
			}
			else if (act->flags & FinAct::HIBERNATE) {
				::SetSystemPowerState(FALSE, is_force);
			}
			else if (act->flags & FinAct::SHUTDOWN) {
				::ExitWindowsEx(EWX_POWEROFF|(is_force ? EWX_FORCE : 0), 0);
			}
		}
	}
	return	TRUE;
}

void TMainDlg::WriteLogHeader(HANDLE hFile, BOOL add_filelog)
{
	char	buf[1024];

	::SetFilePointer(hFile, 0, 0, FILE_END);

	DWORD len = sprintf(buf, "-------------------------------------------------\r\n"
		"FastCopy(%s%s) start at %d/%02d/%02d %02d:%02d:%02d\r\n\r\n",
		GetVersionStr(), GetVerAdminStr(),
		startTm.wYear, startTm.wMonth, startTm.wDay,
		startTm.wHour, startTm.wMinute, startTm.wSecond);

	::WriteFile(hFile, buf, len, &len, 0);
	::WriteFile(hFile, pathLogBuf, (DWORD)strlen(pathLogBuf), &len, 0);

	if (add_filelog && *fileLogPathV) {
		len = sprintf(buf, "<FileLog> %s\r\n", IS_WINNT_V ? cfg.isUtf8Log ?
				WtoU8(fileLogPathV) : WtoA(fileLogPathV) : (char *)fileLogPathV);
		::WriteFile(hFile, buf, len, &len, 0);
	}

	if (finActIdx >= 1) {
		WCHAR	*title = (WCHAR *)cfg.finActArray[finActIdx]->title;
		len = sprintf(buf, "<PostPrc> %s\r\n\r\n", IS_WINNT_V ? cfg.isUtf8Log ?
				WtoU8(title) : WtoA(title) : (char *)title);
		::WriteFile(hFile, buf, len, &len, 0);
	}
	else {
		::WriteFile(hFile, "\r\n", 2, &len, 0);
	}
}

BOOL TMainDlg::WriteLogFooter(HANDLE hFile)
{
	DWORD	len = 0;
	char	buf[1024];

	::SetFilePointer(hFile, 0, 0, FILE_END);

	if (errBufOffset == 0 && ti.total.errFiles == 0 && ti.total.errDirs == 0)
		len += sprintf(buf + len, "%s No Errors\r\n", hFile == hFileLog ? "\r\n" : "");

	len += sprintf(buf + len, "\r\n");
	len += GetDlgItemText(STATUS_EDIT, buf + len, sizeof(buf) - len);
	len += sprintf(buf + len, "\r\n\r\nResult : ");
	len += GetDlgItemText(ERRSTATUS_STATIC, buf + len, sizeof(buf) - len);
	len += sprintf(buf + len, "%s", "\r\n\r\n");
	::WriteFile(hFile, buf, len, &len, 0);

	return	TRUE;
}

BOOL TMainDlg::WriteErrLog(BOOL is_initerr)
{
	EnableErrLogFile(TRUE);

	if (is_initerr) ::GetLocalTime(&startTm);

	WriteLogHeader(hErrLog);

	DWORD	len;

	if (errBufOffset) {
		if (IS_WINNT_V) {
			WCHAR	*werr = (WCHAR *)ti.errBuf->Buf();
			char	*err  = cfg.isUtf8Log ? WtoU8(werr) : WtoA(werr);
			::WriteFile(hErrLog, err, (DWORD)strlen(err), &len, 0);
		}
		else {
			::WriteFile(hErrLog, ti.errBuf->Buf(), errBufOffset, &len, 0);
		}
	}
	else if (is_initerr) {
		char *msg = "Initialize Error (Can't alloc memory or create/access DestDir)\r\n";
		::WriteFile(hErrLog, msg, (DWORD)strlen(msg), &len, 0);
	}

	if (!is_initerr) WriteLogFooter(hErrLog);

	EnableErrLogFile(FALSE);
	return	TRUE;
}

BOOL TMainDlg::CheckVerifyExtension()
{
	if (fastCopy.IsStarting()) return FALSE;

	char	buf[128];
	DWORD	len = GetDlgItemText(LIST_BUTTON, buf, sizeof(buf));
	BOOL	is_set = GetCopyMode() != FastCopy::DELETE_MODE && IsForeground()
						&& (::GetAsyncKeyState(VK_CONTROL) & 0x8000) ? TRUE : FALSE;

	if (len <= 0) return FALSE;

	char	end_ch = buf[len-1];
	if      ( is_set && end_ch != 'V') strcpy(buf + len, "+V");
	else if (!is_set && end_ch == 'V') buf[len-2] = 0;
	else return FALSE;

	SetDlgItemText(LIST_BUTTON, buf);
	return	TRUE;
}

BOOL TMainDlg::EvActivateApp(BOOL fActivate, DWORD dwThreadID)
{
	CheckVerifyExtension();
	return	FALSE;
}

BOOL TMainDlg::EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_FASTCOPY_MSG:
		switch (wParam) {
		case FastCopy::END_NOTIFY:
			EndCopy();
			break;

		case FastCopy::CONFIRM_NOTIFY:
			{
				FastCopy::Confirm	*confirm = (FastCopy::Confirm *)lParam;
				switch (TConfirmDlg().Exec(confirm->message, confirm->allow_continue, this)) {
				case IDIGNORE:	confirm->result = FastCopy::Confirm::IGNORE_RESULT;		break;
				case IDCANCEL:	confirm->result = FastCopy::Confirm::CANCEL_RESULT;		break;
				default:		confirm->result = FastCopy::Confirm::CONTINUE_RESULT;	break;
				}
			}
			break;

		case FastCopy::LISTING_NOTIFY:
			if (IsListing()) SetListInfo();
			else			 SetFileLogInfo();
			break;
		}
		return	TRUE;

	case WM_FASTCOPY_NOTIFY:
		switch (lParam) {
		case WM_LBUTTONDOWN: case WM_RBUTTONDOWN:
			SetForceForegroundWindow();
			Show();
			break;

		case WM_LBUTTONUP: case WM_RBUTTONUP:
			Show();
			if (isErrEditHide && (ti.errBuf && ti.errBuf->UsedSize() || errBufOffset)) {
				SetNormalWindow();
			}
			if (isTaskTray) {
				TaskTray(NIM_DELETE);
			}
			break;
		}
		return	TRUE;

	case WM_FASTCOPY_HIDDEN:
		Show(cfg.taskbarMode ? SW_MINIMIZE : SW_HIDE);
		TaskTray(NIM_ADD, hMainIcon[isDelay ? FCWAIT_ICON_IDX : FCNORMAL_ICON_IDX], FASTCOPY);
		return	TRUE;

	case WM_FASTCOPY_RUNAS:
		{
			HANDLE	hWrite = (HANDLE)lParam;
			DWORD	size = RunasShareSize();
			::WriteFile(hWrite, RunasShareData(), size, &size, NULL);
			::CloseHandle(hWrite);
		}
		return	TRUE;

	case WM_FASTCOPY_STATUS:
		return	fastCopy.IsStarting() ? 1 : isDelay ? 2 : 0;

	case WM_FASTCOPY_KEY:
		CheckVerifyExtension();
		return	TRUE;

	case WM_FASTCOPY_PATHHISTCLEAR:
		SetPathHistory(SETHIST_CLEAR, (UINT)wParam);
		return	TRUE;

	case WM_FASTCOPY_FILTERHISTCLEAR:
		SetFilterHistory(SETHIST_CLEAR, (UINT)wParam);
		return	TRUE;

	default:
		if (uMsg == TaskBarCreateMsg) {
			if (isTaskTray) {
				TaskTray(NIM_ADD, hMainIcon[isDelay ? FCWAIT_ICON_IDX : FCNORMAL_ICON_IDX], FASTCOPY);
			}
			return	TRUE;
		}
	}
	return	FALSE;
}

BOOL TMainDlg::EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return	FALSE;
}

BOOL TMainDlg::EventSystem(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return	FALSE;
}

BOOL TMainDlg::EvTimer(WPARAM timerID, TIMERPROC proc)
{
	timerCnt++;

	if (isDelay) {
		if (fastCopy.TakeExclusivePriv()) {
			::KillTimer(hWnd, FASTCOPY_TIMER);
			isDelay = FALSE;
			ExecCopyCore();
		}
	}
	else {
		if (timerCnt & 0x1) UpdateSpeedLevel(TRUE); // check 500msec

		if (timerCnt == 1 || ((timerCnt >> cfg.infoSpan) << cfg.infoSpan) == timerCnt) {
			SetInfo();
		}
	}

	return	TRUE;
}

DWORD TMainDlg::UpdateSpeedLevel(BOOL is_timer)
{
	DWORD	waitCnt = fastCopy.GetWaitTick();
	DWORD	newWaitCnt = waitCnt;

	if (speedLevel == SPEED_FULL) {
		newWaitCnt = 0;
	}
	else if (speedLevel == SPEED_AUTO) {
		POINT	pt;
		::GetCursorPos(&pt);
		HWND	foreWnd = ::GetForegroundWindow();

		if (foreWnd == hWnd) {
			newWaitCnt = 0;
		}
		else if (pt.x != curPt.x || pt.y != curPt.y || foreWnd != curForeWnd) {
			curPt = pt;
			curForeWnd = foreWnd;
			newWaitCnt = cfg.waitTick;
			timerLast = timerCnt;
		}
		else if (is_timer && newWaitCnt > 0 && (timerCnt - timerLast) >= 10) {
			newWaitCnt -= 1;
			timerLast = timerCnt;
		}
	}
	else if (speedLevel == SPEED_SUSPEND) {
		newWaitCnt = FastCopy::SUSPEND_WAITTICK;
	}
	else {
		static DWORD	waitArray[]    = { 2560, 1280, 640, 320, 160, 80, 40, 20, 10 };
		newWaitCnt = waitArray[speedLevel - 1];
	}

	fastCopy.SetWaitTick(newWaitCnt);

	if (!is_timer && fastCopy.IsStarting()) {
		SetPriority((speedLevel != SPEED_FULL || cfg.alwaysLowIo) ?
						IDLE_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS);
		if (speedLevel == SPEED_SUSPEND) {
			fastCopy.Suspend();
		}
		else {
			fastCopy.Resume();
		}
	}

	return	newWaitCnt;
}

#ifdef USE_LISTVIEW
int TMainDlg::ListViewIdx(int idx)
{
	return	idx;
}

BOOL TMainDlg::SetListViewItem(int idx, int subIdx, char *txt)
{
	LV_ITEM	lvi;
	lvi.mask = LVIF_TEXT|LVIF_PARAM;
	lvi.iItem = ListViewIdx(idx);
	lvi.pszText = txt;
	lvi.iSubItem = subIdx;

	return	listView.SendMessage(LVM_SETITEMTEXT, lvi.iItem, (LPARAM)&lvi);
}
#endif

BOOL TMainDlg::RunAsAdmin(DWORD flg)
{
	SHELLEXECUTEINFOW	sei = {0};
	WCHAR				buf[MAX_PATH];
	DWORD				size = MAX_PATH;

	swprintf(buf, L"/runas=%p,%x,\"%s\",\"%s\"",
		hWnd, flg, cfg.userDirV, cfg.virtualDirV ? cfg.virtualDirV : L"");
	sei.cbSize = sizeof(SHELLEXECUTEINFOW);
	sei.lpVerb = L"runas";
	sei.lpFile = (WCHAR *)cfg.execPathV;
	sei.lpDirectory = (WCHAR *)cfg.execDirV;
	sei.nShow = /*SW_MAXIMIZE*/ SW_NORMAL;
	sei.lpParameters = buf;

	EnableWindow(FALSE);
	isRunAsParent = ::ShellExecuteExW(&sei);
	EnableWindow(TRUE);

	return	isRunAsParent;
}

BOOL TMainDlg::EnableErrLogFile(BOOL on)
{
	if (on && hErrLog == INVALID_HANDLE_VALUE) {
		hErrLogMutex = ::CreateMutex(NULL, FALSE, FASTCOPYLOG_MUTEX);
		::WaitForSingleObject(hErrLogMutex, INFINITE);
		hErrLog = ::CreateFileV(errLogPathV, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, 0,
					OPEN_ALWAYS, 0, 0);
	}
	else if (!on && hErrLog != INVALID_HANDLE_VALUE) {
		::CloseHandle(hErrLog);
		hErrLog = INVALID_HANDLE_VALUE;

		::CloseHandle(hErrLogMutex);
		hErrLogMutex = NULL;
	}
	return	TRUE;
}

int GetArgOpt(void *arg, int default_value)
{
	if (GetChar(arg, 0) == '=') {
		arg = MakeAddr(arg, 1);
	}

	if (GetChar(arg, 0) == 0) {
		return	default_value;
	}

	if (lstrcmpiV(arg, TRUE_V) == 0)
		return	TRUE;

	if (lstrcmpiV(arg, FALSE_V) == 0)
		return	FALSE;

	if (GetChar(arg, 0) >= '0' && GetChar(arg, 0) <= '9' /* || GetChar(arg, 0) == '-' */)
		return	strtolV(arg, NULL, 0);

	return	default_value;
}

int TMainDlg::CmdNameToComboIndex(void *cmd_name)
{
	for (int i=0; copyInfo[i].cmdline_name; i++) {
		if (lstrcmpiV(cmd_name, copyInfo[i].cmdline_name) == 0)
			return	i;
	}
	return	-1;
}

BOOL MakeFileToPathArray(void *path_file, PathArray *path, BOOL is_ucs2)
{
	HANDLE	hFile = ::CreateFileV(path_file, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 0,
					OPEN_EXISTING, 0, 0);
	DWORD	size = ::GetFileSize(hFile, 0), trans;
	char	*buf = NULL;
	BOOL	ret = FALSE;

	if (hFile != INVALID_HANDLE_VALUE) {
		if ((buf = (char *)malloc(size + 2))) {
			if (::ReadFile(hFile, buf, size, &trans, 0) && size == trans) {
				buf[size] = buf[size+1] = 0;
				if (IS_WINNT_V && !is_ucs2) {
					Wstr	*wstr = new Wstr(buf, IsUTF8(buf) ? BY_UTF8 : BY_MBCS);
					if (wstr && wstr->Buf()) {
						if (path->RegisterMultiPath(wstr->Buf(), NEWLINE_STR_V) > 0) ret = TRUE;
					}
					delete wstr;
				}
				else {
					if (path->RegisterMultiPath(buf, NEWLINE_STR_V) > 0) ret = TRUE;
				}
			}
			free(buf);
		}
		::CloseHandle(hFile);
	}

	return	ret;
}

int inline strcmpiEx(void *s1, void *s2, int *len)
{
	*len = strlenV(s2);
	return	strnicmpV(s1, s2, *len);
}

BOOL TMainDlg::GetRunasInfo(WCHAR **user_dir, WCHAR **virtual_dir)
{
	WCHAR	**argv		= (WCHAR **)orgArgv;
	WCHAR	*RUNAS_STR	= (WCHAR *)GetLoadStrV(IDS_RUNAS_OPT);
	int		len;

	while (*argv && **argv == '/') {
		if (!strcmpiEx(*argv, RUNAS_STR, &len) == 0) {
			WCHAR	*p = *argv + len;

			if (p && (p = wcschr(p, ','))) p++;	//skip parent handle
			if (p && (p = wcschr(p, ','))) p++;	//skip runas flg

			// これ以降は、破壊取り出し
			if (p && (p = wcstok(NULL, L"\""))) {
				*user_dir = p;
				p = wcstok(NULL, L",");
				if (p && (p = wcstok(NULL, L"\""))) {
					*virtual_dir = p; // VirtualStoreを使わない場合は通らない
				}
				return	TRUE;
			}
			break;
		}
		argv++;
	}
	return	FALSE;
}

BOOL TMainDlg::CommandLineExecV(int argc, void **argv)
{
	VBuf	shellExtBuf;
	int		len;
	int		job_idx = -1;

	BOOL	is_openwin			= FALSE;
	BOOL	is_noexec			= FALSE;
	BOOL	is_delete			= FALSE;
	BOOL	is_estimate			= cfg.estimateMode;
	DWORD	runas_flg			= 0;
	int		filter_mode			= 0;
	enum	{ NORMAL_FILTER=0x01, EXTEND_FILTER=0x02 };

	void	*CMD_STR			= GetLoadStrV(IDS_CMD_OPT);
	void	*BUFSIZE_STR		= GetLoadStrV(IDS_BUFSIZE_OPT);
	void	*LOG_STR			= GetLoadStrV(IDS_LOG_OPT);
	void	*LOGFILE_STR		= GetLoadStrV(IDS_LOGFILE_OPT);
	void	*UTF8LOG_STR		= GetLoadStrV(IDS_UTF8LOG_OPT);
	void	*FILELOG_STR		= GetLoadStrV(IDS_FILELOG_OPT);
	void	*REPARSE_STR		= GetLoadStrV(IDS_REPARSE_OPT);
	void	*FORCESTART_STR		= GetLoadStrV(IDS_FORCESTART_OPT);
	void	*SKIPEMPTYDIR_STR	= GetLoadStrV(IDS_SKIPEMPTYDIR_OPT);
	void	*ERRSTOP_STR		= GetLoadStrV(IDS_ERRSTOP_OPT);
	void	*ESTIMATE_STR		= GetLoadStrV(IDS_ESTIMATE_OPT);
	void	*VERIFY_STR			= GetLoadStrV(IDS_VERIFY_OPT);
	void	*AUTOSLOW_STR		= GetLoadStrV(IDS_AUTOSLOW_OPT);
	void	*SPEED_STR			= GetLoadStrV(IDS_SPEED_OPT);
	void	*SPEED_FULL_STR		= GetLoadStrV(IDS_SPEED_FULL);
	void	*SPEED_AUTOSLOW_STR	= GetLoadStrV(IDS_SPEED_AUTOSLOW);
	void	*SPEED_SUSPEND_STR	= GetLoadStrV(IDS_SPEED_SUSPEND);
	void	*DISKMODE_STR		= GetLoadStrV(IDS_DISKMODE_OPT);
	void	*DISKMODE_SAME_STR	= GetLoadStrV(IDS_DISKMODE_SAME);
	void	*DISKMODE_DIFF_STR	= GetLoadStrV(IDS_DISKMODE_DIFF);
	void	*DISKMODE_AUTO_STR	= GetLoadStrV(IDS_DISKMODE_AUTO);
	void	*OPENWIN_STR		= GetLoadStrV(IDS_OPENWIN_OPT);
	void	*AUTOCLOSE_STR		= GetLoadStrV(IDS_AUTOCLOSE_OPT);
	void	*FORCECLOSE_STR		= GetLoadStrV(IDS_FORCECLOSE_OPT);
	void	*NOEXEC_STR			= GetLoadStrV(IDS_NOEXEC_OPT);
	void	*FCSHEXT1_STR		= GetLoadStrV(IDS_FCSHEXT1_OPT);
	void	*NOCONFIRMDEL_STR	= GetLoadStrV(IDS_NOCONFIRMDEL_OPT);
	void	*NOCONFIRMSTOP_STR	= GetLoadStrV(IDS_NOCONFIRMSTOP_OPT);
	void	*INCLUDE_STR		= GetLoadStrV(IDS_INCLUDE_OPT);
	void	*EXCLUDE_STR		= GetLoadStrV(IDS_EXCLUDE_OPT);
	void	*FROMDATE_STR		= GetLoadStrV(IDS_FROMDATE_OPT);
	void	*TODATE_STR			= GetLoadStrV(IDS_TODATE_OPT);
	void	*MINSIZE_STR		= GetLoadStrV(IDS_MINSIZE_OPT);
	void	*MAXSIZE_STR		= GetLoadStrV(IDS_MAXSIZE_OPT);
	void	*REGEXP_STR			= GetLoadStrV(IDS_REGEXP_OPT);
	void	*JOB_STR			= GetLoadStrV(IDS_JOB_OPT);
	void	*OWDEL_STR			= GetLoadStrV(IDS_OWDEL_OPT);
	void	*WIPEDEL_STR		= GetLoadStrV(IDS_WIPEDEL_OPT);
	void	*ACL_STR			= GetLoadStrV(IDS_ACL_OPT);
	void	*STREAM_STR			= GetLoadStrV(IDS_STREAM_OPT);
	void	*LINKDEST_STR		= GetLoadStrV(IDS_LINKDEST_OPT);
	void	*RECREATE_STR		= GetLoadStrV(IDS_RECREATE_OPT);
	void	*SRCFILEW_STR		= GetLoadStrV(IDS_SRCFILEW_OPT);
	void	*SRCFILE_STR		= GetLoadStrV(IDS_SRCFILE_OPT);
	void	*FINACT_STR			= GetLoadStrV(IDS_FINACT_OPT);
	void	*TO_STR				= GetLoadStrV(IDS_TO_OPT);
	void	*RUNAS_STR			= GetLoadStrV(IDS_RUNAS_OPT);
	void	*p;
	void	*dst_path	= NULL;
	PathArray pathArray;

	argc--, argv++;		// 実行ファイル名は skip

	while (*argv && GetChar(*argv, 0) == '/') {
		if (strcmpiEx(*argv, CMD_STR, &len) == 0) {
			int idx = CmdNameToComboIndex(MakeAddr(*argv, len));
			if (idx == -1)
				return	MessageBox(GetLoadStr(IDS_USAGE), "Illegal Command"), FALSE;

			// コマンドモードを選択
			SendDlgItemMessage(MODE_COMBO, CB_SETCURSEL, idx, 0);
		}
		else if (strcmpiEx(*argv, JOB_STR, &len) == 0) {
			if ((job_idx = cfg.SearchJobV(MakeAddr(*argv, len))) == -1) {
				MessageBoxW(FmtW(L"%s:%s %s", L"Illegal Command", GetLoadStrW(IDS_JOBNOTFOUND),
							MakeAddr(*argv, len)), FASTCOPYW);
				return	FALSE;
			}
			SetJob(job_idx);
		}
		else if (strcmpiEx(*argv, BUFSIZE_STR, &len) == 0) {
			SetDlgItemTextV(BUFSIZE_EDIT, MakeAddr(*argv, len));
		}
		else if (strcmpiEx(*argv, FILELOG_STR, &len) == 0) {
			if (GetChar(*argv, len) == '=') len++;
			p = MakeAddr(*argv, len);
			if (GetChar(p, 0) == 0 || lstrcmpiV(p, GetLoadStrV(IDS_TRUE)) == 0) {
				fileLogMode = AUTO_FILELOG;
			}
			else if (lstrcmpiV(p, GetLoadStrV(IDS_FALSE)) == 0) {
				fileLogMode = NO_FILELOG;
			}
			else {
				strcpyV(fileLogPathV, p);
				fileLogMode = FIX_FILELOG;
			}
		}
		else if (strcmpiEx(*argv, LOGFILE_STR, &len) == 0 && (p = MakeAddr(*argv, len))) {
			strcpyV(errLogPathV, p);
		}
		else if (strcmpiEx(*argv, LOG_STR, &len) == 0) {
			isErrLog = GetArgOpt(MakeAddr(*argv, len), TRUE);
		}
		else if (strcmpiEx(*argv, UTF8LOG_STR, &len) == 0) {
			isUtf8Log = GetArgOpt(MakeAddr(*argv, len), TRUE) && IS_WINNT_V;
		}
		else if (strcmpiEx(*argv, REPARSE_STR, &len) == 0) {
			isReparse = GetArgOpt(MakeAddr(*argv, len), TRUE);
		}
		else if (strcmpiEx(*argv, FORCESTART_STR, &len) == 0) {
			forceStart = GetArgOpt(MakeAddr(*argv, len), TRUE);
		}
		else if (strcmpiEx(*argv, SKIPEMPTYDIR_STR, &len) == 0) {
			skipEmptyDir = GetArgOpt(MakeAddr(*argv, len), TRUE);
		}
		else if (strcmpiEx(*argv, ERRSTOP_STR, &len) == 0) {
			CheckDlgButton(IGNORE_CHECK, GetArgOpt(MakeAddr(*argv, len), TRUE) ? FALSE : TRUE);
		}
		else if (strcmpiEx(*argv, ESTIMATE_STR, &len) == 0) {
			is_estimate = GetArgOpt(MakeAddr(*argv, len), TRUE);
		}
		else if (strcmpiEx(*argv, VERIFY_STR, &len) == 0) {
			CheckDlgButton(VERIFY_CHECK, GetArgOpt(MakeAddr(*argv, len), TRUE) ? TRUE : FALSE);
		}
		else if (strcmpiEx(*argv, AUTOSLOW_STR, &len) == 0) {
			SetSpeedLevelLabel(this, speedLevel = GetArgOpt(MakeAddr(*argv, len), TRUE) ?
				SPEED_AUTO : SPEED_FULL);
		}
		else if (strcmpiEx(*argv, SPEED_STR, &len) == 0) {
			p = MakeAddr(*argv, len);
			speedLevel = lstrcmpiV(p, SPEED_FULL_STR) == 0 ? SPEED_FULL :
						lstrcmpiV(p, SPEED_AUTOSLOW_STR) == 0 ? SPEED_AUTO :
						lstrcmpiV(p, SPEED_SUSPEND_STR) == 0 ?
						SPEED_SUSPEND : GetArgOpt(p, SPEED_FULL);
			SetSpeedLevelLabel(this, speedLevel = min(speedLevel, SPEED_FULL));
		}
		else if (strcmpiEx(*argv, OWDEL_STR, &len) == 0) {
			CheckDlgButton(OWDEL_CHECK, GetArgOpt(MakeAddr(*argv, len), TRUE));
		}
		else if (strcmpiEx(*argv, WIPEDEL_STR, &len) == 0) {
			CheckDlgButton(OWDEL_CHECK, GetArgOpt(MakeAddr(*argv, len), TRUE));
		}
		else if (strcmpiEx(*argv, ACL_STR, &len) == 0) {
			CheckDlgButton(ACL_CHECK, GetArgOpt(MakeAddr(*argv, len), TRUE) && IS_WINNT_V);
		}
		else if (strcmpiEx(*argv, STREAM_STR, &len) == 0) {
			CheckDlgButton(STREAM_CHECK, GetArgOpt(MakeAddr(*argv, len), TRUE) && IS_WINNT_V);
		}
		else if (strcmpiEx(*argv, LINKDEST_STR, &len) == 0 && CreateHardLinkV) {
			int	val = GetArgOpt(MakeAddr(*argv, len), TRUE);
			if (val > 1 || val < 0) {
				if (val < 300000) {
					return MessageBox("Too small(<300000) hashtable for linkdest",
							"Illegal Command"), FALSE;
				}
				maxLinkHash = val;
				isLinkDest = TRUE;
			} else {
				isLinkDest = val;	// TRUE or FALSE
			}
		}
		else if (strcmpiEx(*argv, RECREATE_STR, &len) == 0) {
			isReCreate = GetArgOpt(MakeAddr(*argv, len), TRUE);
		}
		else if (strcmpiEx(*argv, SRCFILEW_STR, &len) == 0) {
			if (!MakeFileToPathArray(MakeAddr(*argv, len), &pathArray, TRUE)) {
				MessageBoxW(FmtW(L"Can't open: %s", MakeAddr(*argv, len)), FASTCOPYW);
				return	FALSE;
			}
		}
		else if (strcmpiEx(*argv, SRCFILE_STR, &len) == 0) {
			if (!MakeFileToPathArray(MakeAddr(*argv, len), &pathArray, FALSE)) {
				MessageBoxW(FmtW(L"Can't open: %s", MakeAddr(*argv, len)), FASTCOPYW);
				return	FALSE;
			}
		}
		else if (strcmpiEx(*argv, OPENWIN_STR, &len) == 0) {
			is_openwin = GetArgOpt(MakeAddr(*argv, len), TRUE);
		}
		else if (strcmpiEx(*argv, AUTOCLOSE_STR, &len) == 0 && autoCloseLevel != FORCE_CLOSE) {
			autoCloseLevel = (AutoCloseLevel)GetArgOpt(MakeAddr(*argv, len), NOERR_CLOSE);
		}
		else if (strcmpiEx(*argv, FORCECLOSE_STR, &len) == 0) {
			autoCloseLevel = FORCE_CLOSE;
		}
		else if (strcmpiEx(*argv, NOEXEC_STR, &len) == 0) {
			is_noexec = GetArgOpt(MakeAddr(*argv, len), TRUE);
		}
		else if (strcmpiEx(*argv, DISKMODE_STR, &len) == 0) {
			if (lstrcmpiV(MakeAddr(*argv, len), DISKMODE_SAME_STR) == 0)
				diskMode = 1;
			else if (lstrcmpiV(MakeAddr(*argv, len), DISKMODE_DIFF_STR) == 0)
				diskMode = 2;
			else
				diskMode = 0;
		}
		else if (strcmpiEx(*argv, INCLUDE_STR, &len) == 0) {
			filter_mode |= NORMAL_FILTER;
			SetDlgItemTextV(INCLUDE_COMBO, strtok_pathV(MakeAddr(*argv, len), EMPTY_STR_V, &p));
		}
		else if (strcmpiEx(*argv, EXCLUDE_STR, &len) == 0) {
			filter_mode |= NORMAL_FILTER;
			SetDlgItemTextV(EXCLUDE_COMBO, strtok_pathV(MakeAddr(*argv, len), EMPTY_STR_V, &p));
		}
		else if (strcmpiEx(*argv, FROMDATE_STR, &len) == 0) {
			filter_mode |= EXTEND_FILTER;
			SetDlgItemTextV(FROMDATE_COMBO, strtok_pathV(MakeAddr(*argv, len), EMPTY_STR_V, &p));
		}
		else if (strcmpiEx(*argv, TODATE_STR, &len) == 0) {
			filter_mode |= EXTEND_FILTER;
			SetDlgItemTextV(TODATE_COMBO, strtok_pathV(MakeAddr(*argv, len), EMPTY_STR_V, &p));
		}
		else if (strcmpiEx(*argv, MINSIZE_STR, &len) == 0) {
			filter_mode |= EXTEND_FILTER;
			SetDlgItemTextV(MINSIZE_COMBO, strtok_pathV(MakeAddr(*argv, len), EMPTY_STR_V, &p));
		}
		else if (strcmpiEx(*argv, MAXSIZE_STR, &len) == 0) {
			filter_mode |= EXTEND_FILTER;
			SetDlgItemTextV(MAXSIZE_COMBO, strtok_pathV(MakeAddr(*argv, len), EMPTY_STR_V, &p));
		}
		else if (strcmpiEx(*argv, REGEXP_STR, &len) == 0) {
			isRegExp = GetArgOpt(MakeAddr(*argv, len), TRUE);
		}
		else if (strcmpiEx(*argv, NOCONFIRMDEL_STR, &len) == 0) {
			noConfirmDel = GetArgOpt(MakeAddr(*argv, len), TRUE);
		}
		else if (strcmpiEx(*argv, NOCONFIRMSTOP_STR, &len) == 0) {
			noConfirmStop = GetArgOpt(MakeAddr(*argv, len), TRUE);
		}
		else if (strcmpiEx(*argv, FINACT_STR, &len) == 0) {
			if (lstrcmpiV(MakeAddr(*argv, len), FALSE_V) == 0) {
				SetFinAct(-1);
			}
			else {
				int idx = cfg.SearchFinActV(MakeAddr(*argv, len), TRUE);
				if (idx >= 0) SetFinAct(idx);
			}
		}
		else if (strcmpiEx(*argv, TO_STR, &len) == 0) {
			SetDlgItemTextV(DST_COMBO, dst_path = MakeAddr(*argv, len));
		}
		else if (strcmpiEx(*argv, RUNAS_STR, &len) == 0 && IS_WINNT_V) {
			void	*p = MakeAddr(*argv, len);
			HWND	hRunasParent = (HWND)strtoulV(p, 0, 16);
			runas_flg = strtoulV(MakeAddr(strchrV(p, ','), 1), 0, 16);
			if (!TIsUserAnAdmin() || RunasSync(hRunasParent) == FALSE) {
				MessageBox("Not Admin or Failed to read parent window info", FASTCOPY);
				return	FALSE;
			}
		}
		else if (strcmpiEx(*argv, FCSHEXT1_STR, &len) == 0) {
			if (GetChar(*argv, len) == '=') {
				DWORD	flags = strtoulV(MakeAddr(*argv, len + 1), 0, 16);
				if (flags & SHEXT_ISSTOREOPT) {
					shextNoConfirm    = cfg.shextNoConfirm    = (flags & SHEXT_NOCONFIRM) != 0;
					shextNoConfirmDel = cfg.shextNoConfirmDel = (flags & SHEXT_NOCONFIRMDEL) != 0;
					shextTaskTray     = cfg.shextTaskTray     = (flags & SHEXT_TASKTRAY) != 0;
					shextAutoClose    = cfg.shextAutoClose    = (flags & SHEXT_AUTOCLOSE) != 0;
				}
			}

			isShellExt = TRUE;
			is_openwin = !shextTaskTray;
			autoCloseLevel = shextAutoClose ? NOERR_CLOSE : NO_CLOSE;
			HANDLE	hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);
			DWORD	read_size;
			BOOL	convertErr = FALSE;

			shellExtBuf.AllocBuf(SHELLEXT_MIN_ALLOC, SHELLEXT_MAX_ALLOC);
			while (::ReadFile(hStdInput, shellExtBuf.Buf() + shellExtBuf.UsedSize(),
					shellExtBuf.RemainSize(), &read_size, 0) && read_size > 0) {
				if (shellExtBuf.AddUsedSize(read_size) == shellExtBuf.Size())
					shellExtBuf.Grow(SHELLEXT_MIN_ALLOC);
			}
			shellExtBuf.Buf()[shellExtBuf.UsedSize()] = 0;

			if (convertErr)
				return	FALSE;
			if ((argv = CommandLineToArgvV(shellExtBuf.Buf(), &argc)) == NULL)
				break;
			continue;	// 再 parse
		}
		else
			return	MessageBoxV(GetLoadStrV(IDS_USAGE), *argv), FALSE;
		argc--, argv++;
	}

	is_delete = GetCopyMode() == FastCopy::DELETE_MODE;

	if (!isRunAsStart) {
		if (job_idx == -1) {
			SetDlgItemText(SRC_COMBO, "");
			if (!dst_path)
				SetDlgItemText(DST_COMBO, "");
		}
		WCHAR	wBuf[MAX_PATH_EX];

		while (*argv && GetChar(*argv, 0) != '/') {
			void	*path = *argv;
			if (isShellExt && !is_delete) {
				if (NetPlaceConvertV(path, wBuf)) {
					path = wBuf;
					isNetPlaceSrc = TRUE;
				}
			}
			pathArray.RegisterPath(path);
			argc--, argv++;
		}

		int		path_len = pathArray.GetMultiPathLen() + 1;
		void	*path = new WCHAR [path_len];
		if (pathArray.GetMultiPath(path, path_len))
			SetDlgItemTextV(SRC_COMBO, path);
		delete [] path;

		if (argc == 1 && strcmpiEx(*argv, TO_STR, &len) == 0) {
			dst_path = MakeAddr(*argv, len);
			if (isShellExt && NetPlaceConvertV(dst_path, wBuf))
				dst_path = wBuf;
			SetDlgItemTextV(DST_COMBO, dst_path);
		}
		else if (argc >= 1)
			return	MessageBox(GetLoadStr(IDS_USAGE), "Too few/many argument"), FALSE;

		if ((filter_mode & EXTEND_FILTER) && !isExtendFilter) {
			isExtendFilter = TRUE;
			SetExtendFilter();
		}

		if (::GetWindowTextLengthV(GetDlgItem(SRC_COMBO)) == 0
		|| (!is_delete && ::GetWindowTextLengthV(GetDlgItem(DST_COMBO)) == 0)) {
			is_noexec = TRUE;
			if (isShellExt)			// コピー先の無い shell起動時は、autoclose を無視する
				autoCloseLevel = NO_CLOSE;
		}

		if (filter_mode) ReflectFilterCheck(!IsDlgButtonChecked(FILTER_CHECK));

		SetItemEnable(is_delete);
		if (diskMode)
			UpdateMenu();

		if (!is_delete && (is_estimate ||
				!isShellExt && !is_noexec && cfg.estimateMode != is_estimate))
			CheckDlgButton(ESTIMATE_CHECK, is_estimate);
	}

	if (is_openwin || is_noexec || isRunAsStart) {
		Show();
	}
	else {
		MoveCenter();
		if (cfg.taskbarMode) {
			Show(SW_MINIMIZE);
		}
		else {
			TaskTray(NIM_ADD, hMainIcon[FCNORMAL_ICON_IDX], FASTCOPY);
		}
	}

	return	is_noexec || (isRunAsStart && !(runas_flg & RUNAS_IMMEDIATE)) ?
		TRUE : ExecCopy(CMDLINE_EXEC);
}


BOOL CopyItem(HWND hSrc, HWND hDst)
{
	if (!hSrc || !hDst) return	FALSE;

	DWORD	len = ::SendMessageV(hSrc, WM_GETTEXTLENGTH, 0, 0) + 1;
	WCHAR	*wbuf = new WCHAR [len];

	if (!wbuf) return	FALSE;

	::SendMessageV(hSrc, WM_GETTEXT, len, (LPARAM)wbuf);
	::SendMessageV(hDst, WM_SETTEXT, 0, (LPARAM)wbuf);
	::EnableWindow(hDst, ::IsWindowEnabled(hSrc));

	delete [] wbuf;
	return	TRUE;
}

BOOL TMainDlg::RunasSync(HWND hOrg)
{
	int		i;

/* GUI情報をコピー */
	DWORD	copy_items[] = { SRC_COMBO, DST_COMBO, BUFSIZE_EDIT, INCLUDE_COMBO, EXCLUDE_COMBO,
							 FROMDATE_COMBO, TODATE_COMBO, MINSIZE_COMBO, MAXSIZE_COMBO,
							 JOBTITLE_STATIC, 0 };
	for (i=0; copy_items[i]; i++)
		CopyItem(::GetDlgItem(hOrg, copy_items[i]), GetDlgItem(copy_items[i]));

	DWORD	check_items[] = { IGNORE_CHECK, ESTIMATE_CHECK, VERIFY_CHECK, ACL_CHECK, STREAM_CHECK,
								OWDEL_CHECK, FILTER_CHECK, 0 };
	for (i=0; check_items[i]; i++)
		CheckDlgButton(check_items[i], ::IsDlgButtonChecked(hOrg, check_items[i]));

	SendDlgItemMessage(MODE_COMBO, CB_SETCURSEL,
		::SendDlgItemMessage(hOrg, MODE_COMBO, CB_GETCURSEL, 0, 0), 0);

/* Pipe で内部情報を受け渡し */
	DWORD	pid;
	::GetWindowThreadProcessId(hOrg, &pid);
	HANDLE	hProc = ::OpenProcess(PROCESS_DUP_HANDLE, FALSE, pid);

	HANDLE	hRead, hWrite, hDupWrite;
	::CreatePipe(&hRead, &hWrite, NULL, 8192);
	::DuplicateHandle(::GetCurrentProcess(), hWrite, hProc, &hDupWrite, 0, FALSE,
		DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE);
	::PostMessage(hOrg, WM_FASTCOPY_RUNAS, 0, (LPARAM)hDupWrite);
	DWORD	size = RunasShareSize();
	::ReadFile(hRead, RunasShareData(), size, &size, 0);

	::CloseHandle(hProc);
	::CloseHandle(hRead);
	::SendMessage(hOrg, WM_CLOSE, 0, 0);

/* 内部情報とGUIを無矛盾にする */
	SetSpeedLevelLabel(this, speedLevel);
	UpdateMenu();
	SetItemEnable(GetCopyMode() == FastCopy::DELETE_MODE);

	isRunAsStart = TRUE;

	return	TRUE;
}

void TMainDlg::Show(int mode)
{
	if (mode != SW_HIDE && mode != SW_MINIMIZE) {
		SetupWindow();
	}
	TDlg::Show(mode);
}

BOOL TMainDlg::TaskTray(int nimMode, HICON hSetIcon, LPCSTR tip)
{
	isTaskTray = nimMode == NIM_DELETE ? FALSE : TRUE;

	if (cfg.taskbarMode) {
		if (!hSetIcon) hSetIcon = hMainIcon[FCNORMAL_ICON_IDX];
		return	(BOOL)SendMessage(WM_SETICON, ICON_BIG, (LPARAM)hSetIcon);
	}
	else {
		NOTIFYICONDATA	tn;

		memset(&tn, 0, sizeof(tn));
		tn.cbSize = sizeof(tn);
		tn.hWnd = hWnd;
		tn.uID = FASTCOPY_NIM_ID;		// test
		tn.uFlags = NIF_MESSAGE|(hSetIcon ? NIF_ICON : 0)|(tip ? NIF_TIP : 0);
		tn.uCallbackMessage = WM_FASTCOPY_NOTIFY;
		tn.hIcon = hSetIcon;
		if (tip) sprintf(tn.szTip, "%.63s", tip);

		return	::Shell_NotifyIcon(nimMode, &tn);
	}
}

#ifndef SF_UNICODE
#define SF_UNICODE                                  0x00000010
#endif

DWORD CALLBACK RichEditStreamCallBack(DWORD_PTR dwCookie, BYTE *buf, LONG cb, LONG *pcb)
{
	WCHAR **text = (WCHAR **)dwCookie;

	int	len = (int)wcslen(*text);
	if (len * sizeof(WCHAR) > (DWORD)cb) {
		len = cb / sizeof(WCHAR);
	}
	if (len <= 0)
		return	1;

	memcpy(buf, *text, len * sizeof(WCHAR));
	*text = *text + len;
	*pcb = len * sizeof(WCHAR);

	return	0;
}

BOOL RichEditAppendText(HWND hWnd, void *text)
{
	SendMessageV(hWnd, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);

	if (IS_WINNT_V) {
		EDITSTREAM es = { (DWORD_PTR)&text, 0, RichEditStreamCallBack };
		return	SendMessageV(hWnd, EM_STREAMIN, (SF_TEXT|SFF_SELECTION|SF_UNICODE), (LPARAM)&es);
	}
	return	SendMessageV(hWnd, EM_REPLACESEL, 0, (LPARAM)text);
}


BOOL TMainDlg::StartFileLog()
{
	WCHAR	logDir[MAX_PATH] = L"";
	WCHAR	wbuf[MAX_PATH]   = L"";

	if (fileLogMode == NO_FILELOG || IsListing()) return FALSE;

	if (fileLogMode == AUTO_FILELOG || strchrV(fileLogPathV, '\\') == 0) {
		MakePathV(logDir, cfg.userDirV, GetLoadStrV(IDS_FILELOG_SUBDIR));
		if (GetFileAttributesV(logDir) == 0xffffffff)
			CreateDirectoryV(logDir, NULL);

		if (fileLogMode == FIX_FILELOG && *fileLogPathV) {
			strcpyV(wbuf, fileLogPathV);
			MakePathV(fileLogPathV, logDir, wbuf);
		}
	}

	if (fileLogMode == AUTO_FILELOG || *fileLogPathV == 0) {
		SYSTEMTIME	st = startTm;
		for (int i=0; i < 100; i++) {
			sprintfV(wbuf, GetLoadStrV(IDS_FILELOGNAME),
				st.wYear, st.wMonth, st.wDay, st.wHour,
				st.wMinute, st.wSecond, i);
			MakePathV(fileLogPathV, logDir, wbuf);

			hFileLog = ::CreateFileV(fileLogPathV, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
						0, CREATE_NEW, 0, 0);
			if (hFileLog != INVALID_HANDLE_VALUE) break;
		}
	}
	else {
		hFileLog = ::CreateFileV(fileLogPathV, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
					0, OPEN_ALWAYS, 0, 0);
	}

	if (hFileLog == INVALID_HANDLE_VALUE) return FALSE;

	WriteLogHeader(hFileLog, FALSE);
	return	TRUE;
}

void TMainDlg::EndFileLog()
{
	if (hFileLog != INVALID_HANDLE_VALUE) {
		SetFileLogInfo();
		WriteLogFooter(hFileLog);
		::CloseHandle(hFileLog);
	}

	if (fileLogMode != FIX_FILELOG) {
		*fileLogPathV = 0;
	}
	hFileLog = INVALID_HANDLE_VALUE;
}

#define FILELOG_MINSIZE	512 * 1024

void TMainDlg::SetFileLogInfo()
{
	DWORD	size = ti.listBuf->UsedSize();
//	DWORD	low, high;
//
//	if ((low = ::GetFileSize(hFileLog, &high)) == 0xffffffff && GetLastError() != NO_ERROR) return;
//	::LockFile(hFile, low, high, size, 0);
	::SetFilePointer(hFileLog, 0, 0, FILE_END);

	if (IS_WINNT_V) {
		if (isUtf8Log) {
			U8str	str((WCHAR *)ti.listBuf->Buf());
			::WriteFile(hFileLog, str, (int)strlen(str), &size, NULL);
		}
		else {
			MBCSstr	str((WCHAR *)ti.listBuf->Buf());
			::WriteFile(hFileLog, str, (int)strlen(str), &size, NULL);
		}
	}
	else {
		::WriteFile(hFileLog, ti.listBuf->Buf(), size, &size, NULL);
	}
//	::UnlockFile(hFile, low, high, size, 0);
	ti.listBuf->SetUsedSize(0);
}

void TMainDlg::SetListInfo()
{
	RichEditAppendText(GetDlgItem(PATH_EDIT), ti.listBuf->Buf());
	listBufOffset += ti.listBuf->UsedSize();
	ti.listBuf->SetUsedSize(0);
}

BOOL TMainDlg::SetTaskTrayInfo(BOOL is_finish_status, double doneRate,
				int remain_h, int remain_m, int remain_s)
{
	int		len = 0;
	char	buf[1024] = "";

	if (!cfg.taskbarMode) {
		if (info.mode == FastCopy::DELETE_MODE) {
			len += sprintf(buf + len, IsListing() ?
				"FastCopy (%.1fMB %dfiles %dsec)" :
				"FastCopy (%.1fMB %dfiles %dsec %.2fMB/s)",
				(double)ti.total.deleteTrans / (1024 * 1024),
				ti.total.deleteFiles,
				ti.tickCount / 1000,
				(double)ti.total.deleteFiles * 1000 / ti.tickCount);
		}
		else if (ti.total.isPreSearch) {
			len += sprintf(buf + len, " Estimating (Total %.1f MB/%d files/%d sec)",
				(double)ti.total.preTrans / (1024 * 1024),
				ti.total.preFiles,
				(int)(ti.tickCount / 1000));
		}
		else {
			if ((info.flags & FastCopy::PRE_SEARCH) && !ti.total.isPreSearch && !is_finish_status
			&& doneRate >= 0.0001) {
				len += sprintf(buf + len, "%d%% (Remain %02d:%02d:%02d) ",
					doneRatePercent, remain_h, remain_m, remain_s);
			}
			len += sprintf(buf + len, IsListing() ?
				"FastCopy (%.1fMB %dfiles %dsec)" :
				"FastCopy (%.1fMB %dfiles %dsec %.2fMB/s)",
				(double)ti.total.writeTrans / (1024 * 1024),
				ti.total.writeFiles,
				ti.tickCount / 1000,
				(double)ti.total.writeTrans / ti.tickCount / 1024 * 1000 / 1024
				);
		}
		if (is_finish_status) {
			strcpy(buf + len, " Finished");
		}
	}

	curIconIndex = is_finish_status ? 0 : (curIconIndex + 1) % MAX_NORMAL_FASTCOPY_ICON;
	TaskTray(NIM_MODIFY, hMainIcon[curIconIndex], buf);
	return	TRUE;
}

inline double SizeToCoef(const double &ave_size)
{
#define AVE_WATERMARK_MIN (4.0   * 1024)
#define AVE_WATERMARK_MID (64.0  * 1024)

	if (ave_size < AVE_WATERMARK_MIN) return 2.0;
	double ret = AVE_WATERMARK_MID / ave_size;
	if (ret >= 2.0) return 2.0;
	if (ret <= 0.1) return 0.1;
	return ret;
}

BOOL TMainDlg::CalcInfo(double *doneRate, int *remain_sec, int *total_sec)
{
	_int64	preFiles, preTrans, doneFiles, doneTrans;
	double	realDoneRate, coef, real_coef;

	calcTimes++;

	if (info.mode == FastCopy::DELETE_MODE) {
		preFiles  = ti.total.preFiles    + ti.total.preDirs;
		doneFiles = ti.total.deleteFiles + ti.total.deleteDirs + ti.total.skipDirs
					 + ti.total.errDirs + ti.total.skipFiles + ti.total.errFiles;
		realDoneRate = *doneRate = doneFiles / (preFiles + 0.01);
		lastTotalSec = (int)(ti.tickCount / realDoneRate / 1000);
	}
	else {
		preFiles = (ti.total.preFiles /* + ti.total.preDirs*/) * 2;
		preTrans = (ti.total.preTrans * 2);

		doneFiles = (ti.total.writeFiles + ti.total.readFiles /*+ ti.total.readDirs*/
					/*+ ti.total.skipDirs*/ + (ti.total.skipFiles + ti.total.errFiles) * 2);

		doneTrans = ti.total.writeTrans + ti.total.readTrans
					+ (ti.total.skipTrans + ti.total.errTrans) * 2;

		if ((info.flags & FastCopy::VERIFY_FILE)) {
			int vcoef =  ti.isSameDrv ? 1 : 2;
			preFiles  += ti.total.preFiles * vcoef;
			preTrans  += ti.total.preTrans * vcoef;
			doneFiles += (ti.total.verifyFiles + ti.total.skipFiles + ti.total.errFiles) * vcoef;
			doneTrans += (ti.total.verifyTrans + ti.total.skipTrans + ti.total.errTrans) * vcoef;
		}

//		Debug("F: %5d(%4d/%4d) / %5d  T: %7d / %7d   ", int(doneFiles), int(ti.total.writeFiles),
//			int(ti.total.readFiles), int(preFiles), int(doneTrans/1024), int(preTrans/1024));

		if (doneFiles > preFiles) preFiles = doneFiles;
		if (doneTrans > preTrans) preTrans = doneTrans;

		double doneTransRate = doneTrans / (preTrans  + 0.01);
		double doneFileRate  = doneFiles / (preFiles  + 0.01);
		double aveSize       = preTrans  / (preFiles  + 0.01);
//		double doneAveSize   = doneTrans / (doneFiles + 0.01);

		coef      = SizeToCoef(aveSize);
//		real_coef = coef * coef / SizeToCoef(doneAveSize);
		real_coef = coef;

		if (coef      > 100.0) coef      = 100.0;
		if (real_coef > 100.0) real_coef = 100.0;
		*doneRate    = (doneFileRate *      coef + doneTransRate) / (     coef + 1);
//		realDoneRate = (doneFileRate * real_coef + doneTransRate) / (real_coef + 1);
		realDoneRate = *doneRate;

		if (realDoneRate < 0.01) realDoneRate = 0.001;

		if (calcTimes < 10
		|| !(calcTimes % (realDoneRate < 0.70 ? 10 : (11 - int(realDoneRate * 10))))) {
			lastTotalSec = (int)(ti.tickCount / realDoneRate / 1000);
//			Debug("recalc(%d) ", (realDoneRate < 0.70 ? 10 : (11 - int(realDoneRate * 10))));
		}
//		else	Debug("nocalc(%d) ", (realDoneRate < 0.70 ? 10 : (11 - int(realDoneRate * 10))));
	}

	*total_sec = lastTotalSec;
	if ((*remain_sec = *total_sec - (ti.tickCount / 1000)) < 0) *remain_sec = 0;

//	Debug("rate=%2d(%2d) coef=%2.1f(%2.1f) rmain=%5d total=%5d\n", int(*doneRate * 100),
//		int(realDoneRate*100), coef, real_coef, *remain_sec, *total_sec);

	return	TRUE;
}

BOOL TMainDlg::SetInfo(BOOL is_finish_status)
{
	char	buf[1024];
	int		len = 0;
	double	doneRate;
	int		remain_sec, total_sec, remain_h, remain_m, remain_s;

	doneRatePercent = -1;

	fastCopy.GetTransInfo(&ti, is_finish_status || !isTaskTray);
	if (ti.tickCount == 0) {
		ti.tickCount++;
	}

	if (IsListing() && ti.listBuf->UsedSize() > 0
	|| (info.flags & FastCopy::LISTING) && ti.listBuf->UsedSize() >= FILELOG_MINSIZE) {
		::EnterCriticalSection(ti.listCs);
		if (IsListing()) SetListInfo();
		else			 SetFileLogInfo();
		::LeaveCriticalSection(ti.listCs);
	}

	if ((info.flags & FastCopy::PRE_SEARCH) && !ti.total.isPreSearch) {
		CalcInfo(&doneRate, &remain_sec, &total_sec);
		remain_h = remain_sec / 3600;
		remain_m = (remain_sec % 3600) / 60;
		remain_s = remain_sec % 60;
		doneRatePercent = (int)(doneRate * 100);
		SetWindowTitle();
	}

	if (isTaskTray) {
		SetTaskTrayInfo(is_finish_status, doneRate, remain_h, remain_m, remain_s);
		if (isTaskTray && !is_finish_status) return TRUE;
	}

	if ((info.flags & FastCopy::PRE_SEARCH) && !ti.total.isPreSearch && !is_finish_status
	&& doneRate >= 0.0001) {
		len += sprintf(buf + len, " -- Remain %02d:%02d:%02d (%d%%) --\r\n",
				remain_h, remain_m, remain_s, doneRatePercent);
	}
	if (ti.total.isPreSearch) {
		len += sprintf(buf + len,
			" ---- Estimating ... ----\r\n"
			"PreTrans = %.1fMB\r\nPreFiles  = %d (%d)\r\n"
			"PreTime  = %.2fsec\r\nPreRate  = %.2ffiles/s",
			(double)ti.total.preTrans / (1024 * 1024),
			ti.total.preFiles, ti.total.preDirs,
			(double)ti.tickCount / 1000,
			(double)ti.total.preFiles * 1000 / ti.tickCount);
	}
	else if (info.mode == FastCopy::DELETE_MODE) {
		len += sprintf(buf + len,
			IsListing() ?
			"TotalDel   = %.1f MB\r\n"
			"DelFiles   = %d (%d)\r\n"
			"TotalTime = %.2f sec\r\n" :
			(info.flags & (FastCopy::OVERWRITE_DELETE|FastCopy::OVERWRITE_DELETE_NSA)) ?
			"TotalDel   = %.1f MB\r\n"
			"DelFiles   = %d (%d)\r\n"
			"TotalTime = %.2f sec\r\n"
			"FileRate   = %.2f files/s\r\n"
			"OverWrite = %.1f MB/s" :
			"TotalDel   = %.1f MB\r\n"
			"DelFiles   = %d (%d)\r\n"
			"TotalTime = %.2f sec\r\n"
			"FileRate   = %.2f files/s",
			(double)ti.total.deleteTrans / (1024 * 1024),
			ti.total.deleteFiles, ti.total.deleteDirs,
			(double)ti.tickCount / 1000,
			(double)ti.total.deleteFiles * 1000 / ti.tickCount,
			(double)ti.total.writeTrans / ((double)ti.tickCount / 1000) / (1024 * 1024));
	}
	else {
		if (IsListing()) {
			len += sprintf(buf + len,
				(info.flags & FastCopy::RESTORE_HARDLINK) ? 
				"TotalSize = %.1f MB\r\n"
				"TotalFiles = %d/%d (%d)\r\n" :
				"TotalSize = %.1f MB\r\n"
				"TotalFiles = %d (%d)\r\n"
				, (double)ti.total.writeTrans / (1024 * 1024)
				, ti.total.writeFiles
				, (info.flags & FastCopy::RESTORE_HARDLINK) ?
				  ti.total.linkFiles :
				  ti.total.writeDirs
				, ti.total.writeDirs);
		}
		else {
			len += sprintf(buf + len,
				(info.flags & FastCopy::RESTORE_HARDLINK) ? 
				"TotalRead = %.1f MB\r\n"
				"TotalWrite = %.1f MB\r\n"
				"TotalFiles = %d/%d (%d)\r\n" :
				"TotalRead = %.1f MB\r\n"
				"TotalWrite = %.1f MB\r\n"
				"TotalFiles = %d (%d)\r\n"
				, (double)ti.total.readTrans / (1024 * 1024)
				, (double)ti.total.writeTrans / (1024 * 1024)
				, ti.total.writeFiles
				, (info.flags & FastCopy::RESTORE_HARDLINK) ?
				  ti.total.linkFiles :
				  ti.total.writeDirs
				, ti.total.writeDirs);
		}

		if (ti.total.skipFiles || ti.total.skipDirs) {
			len += sprintf(buf + len,
				"TotalSkip = %.1f MB\r\n"
				"SkipFiles = %d (%d)\r\n"
				, (double)ti.total.skipTrans / (1024 * 1024)
				, ti.total.skipFiles, ti.total.skipDirs);
		}
		if (ti.total.deleteFiles || ti.total.deleteDirs) {
			len += sprintf(buf + len,
				"TotalDel  = %.1f MB\r\n"
				"DelFiles  = %d (%d)\r\n"
				, (double)ti.total.deleteTrans / (1024 * 1024)
				, ti.total.deleteFiles, ti.total.deleteDirs);
		}
		len += sprintf(buf + len, IsListing() ?
			"TotalTime= %.2f sec\r\n" :
			"TotalTime= %.2f sec\r\n"
			"TransRate= %.2f MB/s\r\n"
			"FileRate  = %.2f files/s"
			, (double)ti.tickCount / 1000
			, (double)ti.total.writeTrans / ti.tickCount / 1024 * 1000 / 1024
			, (double)ti.total.writeFiles * 1000 / ti.tickCount);

		if (info.flags & FastCopy::VERIFY_FILE) {
			len += sprintf(buf + len,
				"\r\nVerifyRead= %.1f MB\r\nVerifyFiles= %d"
				, (double)ti.total.verifyTrans / (1024 * 1024)
				, ti.total.verifyFiles);
		}
	}

	if (IsListing() && is_finish_status) {
		len += sprintf(buf + len, "\r\n -- Listing%s Done --",
				(info.flags & FastCopy::VERIFY_FILE) ? " (verify)" : "");
	}
	SetDlgItemText(STATUS_EDIT, buf);

	if (IsListing()) {
		if (is_finish_status) {
			int offset_v = listBufOffset / CHAR_LEN_V;
			int pathedit_len = (int)SendDlgItemMessageV(PATH_EDIT, WM_GETTEXTLENGTH, 0, 0);
			sprintf(buf, "Finished. (ErrorFiles : %d  ErrorDirs : %d)",
				ti.total.errFiles, ti.total.errDirs);
			SendDlgItemMessageV(PATH_EDIT, EM_SETSEL, offset_v, offset_v);
			SendDlgItemMessage(PATH_EDIT, EM_REPLACESEL, 0, (LPARAM)buf);
//			int line = SendDlgItemMessage(PATH_EDIT, EM_GETLINECOUNT, 0, 0);
//			SendDlgItemMessage(PATH_EDIT, EM_LINESCROLL, 0, line > 2 ? line -2 : 0);
		}
	}
	else if (is_finish_status) {
		sprintf(buf, ti.total.errFiles || ti.total.errDirs ?
			"Finished. (ErrorFiles : %d  ErrorDirs : %d)" : "Finished.",
			ti.total.errFiles, ti.total.errDirs);
		SetDlgItemText(PATH_EDIT, buf);
		SetWindowTitle();
	}
	else
		SetDlgItemTextV(PATH_EDIT, ti.total.isPreSearch ? EMPTY_STR_V : ti.curPath);

	SetDlgItemText(SAMEDRV_STATIC, info.mode == FastCopy::DELETE_MODE ? "" : ti.isSameDrv ?
		GetLoadStr(IDS_SAMEDISK) : GetLoadStr(IDS_DIFFDISK));

//	SetDlgItemText(SPEED_STATIC, buf);

	if ((info.ignoreEvent ^ ti.ignoreEvent) & FASTCOPY_ERROR_EVENT)
		CheckDlgButton(IGNORE_CHECK,
			((info.ignoreEvent = ti.ignoreEvent) & FASTCOPY_ERROR_EVENT) ? 1 : 0);

	if (isErrEditHide && ti.errBuf->UsedSize() > 0) {
		SetNormalWindow();
	}
	if (ti.errBuf->UsedSize() > errBufOffset || errBufOffset == MAX_ERR_BUF || is_finish_status) {
		if (ti.errBuf->UsedSize() > errBufOffset && errBufOffset != MAX_ERR_BUF) {
			::EnterCriticalSection(ti.errCs);
			RichEditAppendText(GetDlgItem(ERR_EDIT), ti.errBuf->Buf() + errBufOffset);
			errBufOffset = ti.errBuf->UsedSize();
			::LeaveCriticalSection(ti.errCs);
		}
		sprintf(buf, "(ErrFiles : %d / ErrDirs : %d)", ti.total.errFiles, ti.total.errDirs);
		SetDlgItemText(ERRSTATUS_STATIC, buf);
	}
	return	TRUE;
}

BOOL TMainDlg::SetWindowTitle()
{
	static void	*title;
	static void	*version;
	static void	*admin;

	if (!title) {
		title	= IS_WINNT_V ? (void *)FASTCOPYW                    : (void *)FASTCOPY;
		version	= IS_WINNT_V ? (void *)AtoW(GetVersionStr(), TRUE)  : (void *)GetVersionStr();
		admin	= IS_WINNT_V ? (void *)AtoW(GetVerAdminStr(), TRUE) : (void *)GetVerAdminStr();
	}

	WCHAR	_buf[1024];
	void	*buf = _buf;
	int		len = 0;
	void	*FMT_HYPHEN  = IS_WINNT_V ? (void *)L" -%s"    : (void *)" -%s";
	void	*FMT_PAREN   = IS_WINNT_V ? (void *)L" [%s]"   : (void *)" [%s]";
	void	*FMT_PERCENT = IS_WINNT_V ? (void *)L"(%d%%) " : (void *)"(%d%%) ";
	void	*FMT_STR     = IS_WINNT_V ? (void *)L"%s"      : (void *)"%s";
	void	*FMT_STRSTR  = IS_WINNT_V ? (void *)L" %s%s"   : (void *)" %s%s";

	if ((info.flags & FastCopy::PRE_SEARCH) && !ti.total.isPreSearch && doneRatePercent >= 0
	&& fastCopy.IsStarting()) {
		len += sprintfV(MakeAddr(buf, len), FMT_PERCENT, doneRatePercent);
	}

	WCHAR	_job[MAX_PATH] = L"";
	void	*job = _job;
	void	*fin = finActIdx > 0 ? cfg.finActArray[finActIdx]->title : (void *)L"";

	GetDlgItemTextV(JOBTITLE_STATIC, job, MAX_PATH);

	len += sprintfV(MakeAddr(buf, len), FMT_STR, title);

	if (GetChar(job, 0)) {
		len += sprintfV(MakeAddr(buf, len), FMT_PAREN, job);
	}

	if (GetChar(fin, 0)) {
		len += sprintfV(MakeAddr(buf, len), FMT_HYPHEN, fin);
	}

	len += sprintfV(MakeAddr(buf, len), FMT_STRSTR,
			GetChar(job, 0) || GetChar(fin, 0) ? L"" : version, admin);

	return	SetWindowTextV(buf);
}

void TMainDlg::SetItemEnable(BOOL is_delete)
{
	::EnableWindow(GetDlgItem(DST_FILE_BUTTON), !is_delete);
	::EnableWindow(GetDlgItem(DST_COMBO), !is_delete);
	::ShowWindow(GetDlgItem(ESTIMATE_CHECK), is_delete ? SW_HIDE : SW_SHOW);
	::ShowWindow(GetDlgItem(VERIFY_CHECK), is_delete ? SW_HIDE : SW_SHOW);

	::EnableWindow(GetDlgItem(ACL_CHECK), !is_delete && IS_WINNT_V);
	::EnableWindow(GetDlgItem(STREAM_CHECK), !is_delete && IS_WINNT_V);
	::EnableWindow(GetDlgItem(OWDEL_CHECK), is_delete);

	::ShowWindow(GetDlgItem(ACL_CHECK), is_delete ? SW_HIDE : SW_SHOW);
	::ShowWindow(GetDlgItem(STREAM_CHECK), is_delete ? SW_HIDE : SW_SHOW);
	::ShowWindow(GetDlgItem(OWDEL_CHECK), is_delete ? SW_SHOW : SW_HIDE);

//	SetPathHistory(SETHIST_LIST);

	ReflectFilterCheck();
}

void TMainDlg::UpdateMenu(void)
{
	WCHAR	buf[MAX_PATH_EX];
	int		i;
	HMENU	hMenu = ::GetMenu(hWnd);

//	if (!IsWinVista() || TIsUserAnAdmin()) {
//		::DeleteMenu(hMenu, ADMIN_MENUITEM, MF_BYCOMMAND);
//	}

// トップレベルメニューアイテムにアイコンを付与するのは無理かも？
//	HICON hIcon = ::LoadImage(GetModuleHandle("user32.dll"), 106, IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
//	::SetMenuItemBitmaps(hMenu, 0, MF_BYPOSITION, LoadBitmap(NULL, (char *)32754), LoadBitmap(NULL, (char *)32754));

	HMENU	hSubMenu = ::GetSubMenu(hMenu, 1);
	while (::GetMenuItemCount(hSubMenu) > 2 && ::DeleteMenu(hSubMenu, 2, MF_BYPOSITION))
		;

	GetDlgItemTextV(JOBTITLE_STATIC, buf, MAX_PATH_EX);

	for (i=0; i < cfg.jobMax; i++) {
		InsertMenuV(hSubMenu, i + 2, MF_STRING|MF_BYPOSITION, JOBOBJ_MENUITEM_START + i,
			cfg.jobArray[i]->title);
		::CheckMenuItem(hSubMenu, i + 2,
			MF_BYPOSITION|(lstrcmpiV(buf, cfg.jobArray[i]->title) ? MF_UNCHECKED : MF_CHECKED));
	}

	hSubMenu = ::GetSubMenu(hMenu, 2);
	::CheckMenuItem(hSubMenu, AUTODISK_MENUITEM,
		MF_BYCOMMAND|((diskMode == 0) ? MF_CHECKED : MF_UNCHECKED));
	::CheckMenuItem(hSubMenu, SAMEDISK_MENUITEM,
		MF_BYCOMMAND|((diskMode == 1) ? MF_CHECKED : MF_UNCHECKED));
	::CheckMenuItem(hSubMenu, DIFFDISK_MENUITEM,
		MF_BYCOMMAND|((diskMode == 2) ? MF_CHECKED : MF_UNCHECKED));

	::CheckMenuItem(hSubMenu, EXTENDFILTER_MENUITEM,
		MF_BYCOMMAND|(isExtendFilter ? MF_CHECKED : MF_UNCHECKED));

	sprintfV(buf, GetLoadStrV(IDS_FINACTMENU),
		finActIdx >= 0 ? cfg.finActArray[finActIdx]->title : EMPTY_STR_V);
	ModifyMenuV(hSubMenu, 7, MF_BYPOSITION|MF_STRING, 0, buf);

	hSubMenu = ::GetSubMenu(hSubMenu, 7);
	while (::GetMenuItemCount(hSubMenu) > 2 && ::DeleteMenu(hSubMenu, 0, MF_BYPOSITION))
		;
	for (i=0; i < cfg.finActMax; i++) {
		InsertMenuV(hSubMenu, i, MF_STRING|MF_BYPOSITION, FINACT_MENUITEM_START + i,
			cfg.finActArray[i]->title);
		::CheckMenuItem(hSubMenu, i, MF_BYPOSITION|(i == finActIdx ? MF_CHECKED : MF_UNCHECKED));
	}

	if (!fastCopy.IsStarting() && !isDelay) {
		SetDlgItemText(SAMEDRV_STATIC, diskMode == 0 ? "" : diskMode == 1 ?
			GetLoadStr(IDS_FIX_SAMEDISK) : GetLoadStr(IDS_FIX_DIFFDISK));
	}
	SetWindowTitle();
}

void TMainDlg::SetJob(int idx)
{
	WCHAR	wbuf[MAX_PATH] = L"";
	void	*title = wbuf;
	Job		*job = cfg.jobArray[idx];

	idx = CmdNameToComboIndex(job->cmd);

	if (idx == -1)	return;

	if (GetDlgItemTextV(JOBTITLE_STATIC, title, MAX_PATH) > 0 && strcmpV(job->title, title) == 0) {
		SetDlgItemTextV(JOBTITLE_STATIC, L"");
		UpdateMenu();
		return;
	}

	SetDlgItemTextV(JOBTITLE_STATIC, job->title);
	SendDlgItemMessage(MODE_COMBO, CB_SETCURSEL, idx, 0);
	SetDlgItemInt(BUFSIZE_EDIT, job->bufSize);
	CheckDlgButton(ESTIMATE_CHECK, job->estimateMode);
	CheckDlgButton(VERIFY_CHECK, job->enableVerify);
	CheckDlgButton(IGNORE_CHECK, job->ignoreErr);
	CheckDlgButton(OWDEL_CHECK, job->enableOwdel);
	CheckDlgButton(ACL_CHECK, job->enableAcl);
	CheckDlgButton(STREAM_CHECK, job->enableStream);
	CheckDlgButton(FILTER_CHECK, job->isFilter);
	SetDlgItemTextV(SRC_COMBO, job->src);
	SetDlgItemTextV(DST_COMBO, job->dst);
	SetDlgItemTextV(INCLUDE_COMBO, job->includeFilter);
	SetDlgItemTextV(EXCLUDE_COMBO, job->excludeFilter);
	SetDlgItemTextV(FROMDATE_COMBO, job->fromDateFilter);
	SetDlgItemTextV(TODATE_COMBO, job->toDateFilter);
	SetDlgItemTextV(MINSIZE_COMBO, job->minSizeFilter);
	SetDlgItemTextV(MAXSIZE_COMBO, job->maxSizeFilter);
	SetItemEnable(copyInfo[idx].mode == FastCopy::DELETE_MODE);
	SetDlgItemText(PATH_EDIT, "");
	SetDlgItemText(ERR_EDIT, "");
	diskMode = job->diskMode;
	UpdateMenu();
	SetMiniWindow();

	if (job->isFilter &&
		(GetChar(job->fromDateFilter,0) || GetChar(job->toDateFilter,  0) ||
		 GetChar(job->minSizeFilter, 0) || GetChar(job->maxSizeFilter, 0))) {
		isExtendFilter = TRUE;
	}
	else isExtendFilter = cfg.isExtendFilter;

	SetExtendFilter();
}

void TMainDlg::SetFinAct(int idx)
{
	if (idx < cfg.finActMax) {
		finActIdx = idx;
		UpdateMenu();
	}
}


#ifdef HASHQUALITY_CHECK
//	ハッシュ品質確認（コリジョン検査）
#define MAX_HASH	5000000

#define THASH_RAND_NUM1 757
#define THASH_RAND_NUM2 769

extern int rand_data1[THASH_RAND_NUM1];
extern int rand_data2[THASH_RAND_NUM2];

class THtest : public THashTbl {
public:
	virtual BOOL IsSameVal(THashObj *obj, const void *_data) { return TRUE; }
	THtest() : THashTbl(MAX_HASH, FALSE) {}
};

void CheckHashQuality()
{
	THtest	ht;
	int		col=0;

	Debug("start hash test(%d)\n", MAX_HASH);
	THashObj	*obj = new THashObj[MAX_HASH];
	char	buf[100];
	memset(buf, 0, sizeof(buf));
	int		len = 4;
	DWORD	&val = *(DWORD *)buf;
	DWORD	t = GetTickCount();

	for (int i=0; i < MAX_HASH; i++) {
#if 0
		len = sprintf(buf, "str______________%I64d\n", (_int64)i) + 1;
#elif 1
		val = i;
#else
		val = (rand_data1[i % THASH_RAND_NUM1] ^ rand_data2[i % THASH_RAND_NUM2]);
#endif
		u_int	hash_id = MakeHash(buf, len);
		if (ht.Search(NULL, hash_id)) {
			col++;
		}
		else {
			ht.Register(obj + i, hash_id);
		}
	}
	Debug("col=%d of %d (%.2f sec)\n", col, MAX_HASH, (GetTickCount() - t) / (float)1000);
	delete [] obj;
}
#endif

