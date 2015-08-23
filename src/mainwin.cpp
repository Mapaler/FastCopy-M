static char *mainwin_id = 
	"@(#)Copyright (C) 2004-2015 H.Shirouzu		mainwin.cpp	ver3.02";
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Create					: 2004-09-15(Wed)
	Update					: 2015-08-12(Wed)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	Modify					: Mapaler 2015-08-23
	======================================================================== */

#include "mainwin.h"
#include "shellext/shelldef.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>

#define FASTCOPY_TIMER_TICK 250 //基本信息更新时间，看看怎么调节

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
//	TLibInit_AdvAPI32();

//	extern void tapi32_test();
//	tapi32_test();
//	extern void cond_test();
//	cond_test();
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

	::GetLocalTime(&startTm);

	isNoUI = FALSE;
	if (IsWinVista()) {
		DWORD	sid;
		if (::ProcessIdToSessionId(::GetCurrentProcessId(), &sid) && sid == 0) {
			isNoUI = TRUE;
			noConfirmDel = noConfirmStop = TRUE;
		}
	}

	orgArgv = CommandLineToArgvExW(::GetCommandLineW(), &orgArgc);
	if (orgArgc == 2 && wcsicmp(orgArgv[1], INSTALL_STR) == 0) {
		isInstaller = TRUE; // インストーラ起動
		orgArgc = 1;
	}
	else isInstaller = FALSE;

	if (IsWinVista() && ::IsUserAnAdmin() && TIsEnableUAC()) {
		GetRunasInfo(&user_dir, &virtual_dir);
		TChangeWindowMessageFilter(WM_DROPFILES, 1);
		TChangeWindowMessageFilter(WM_COPYDATA, 1);
		TChangeWindowMessageFilter(WM_COPYGLOBALDATA, 1);
	}
	if (!cfg.ReadIni(user_dir, virtual_dir)) {
		MessageBox("Can't initialize...", FASTCOPY, MB_OK);
		PostQuitMessage(0);
		return;
	}

	if (cfg.lcid > 0)
		TSetDefaultLCID(cfg.lcid);

	cfg.PostReadIni();

	isShellExt = FALSE;
	isErrLog = cfg.isErrLog;
	isUtf8Log = cfg.isUtf8Log;
	fileLogMode = (FileLogMode)cfg.fileLogMode;
	isListLog = FALSE;
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
	wcscpy(errLogPath, cfg.errLogPath);
	*fileLogPath = 0;
	*lastFileLog = 0;
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
	statusFont = NULL;

	curPriority = ::GetPriorityClass(::GetCurrentProcess());

	if (IsWinVista()) {	// WinServer2012+ATOM で発生する問題への対処（暫定）
		::SetPriorityClass(::GetCurrentProcess(), PROCESS_MODE_BACKGROUND_BEGIN);
		::SetPriorityClass(::GetCurrentProcess(), PROCESS_MODE_BACKGROUND_END);
	}

	SetPriority(SPEED_AUTO);
	SetPriority(SPEED_FULL);

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

void TMainDlg::SetComboBox(UINT item, WCHAR **history, SetHistMode mode)
{
	DWORD	len = 0;
	WCHAR	*wbuf = NULL;

	// backup editbox
	if (mode == SETHIST_LIST || mode == SETHIST_CLEAR) {
		len = ::GetWindowTextLengthW(GetDlgItem(item));
		wbuf = new WCHAR [len + 1];
		if (GetDlgItemTextW(item, wbuf, len + 1) != len)
			return;
	}

	// clear listbox & editbox
	SendDlgItemMessage(item, CB_RESETCONTENT, 0, 0);

	// set listbox
	if (mode == SETHIST_LIST) {
		for (int i=0; i < cfg.maxHistory; i++) {
			if (history[i][0])
				SendDlgItemMessageW(item, CB_INSERTSTRING, i, (LPARAM)history[i]);
		}
	}

	// set editbox
	if (mode == SETHIST_EDIT) {
		if (cfg.maxHistory > 0 /* && ::IsWindowEnabled(GetDlgItem(item) */)
			SetDlgItemTextW(item, history[0]);
	}

	// restore editbox
	if (mode == SETHIST_LIST || mode == SETHIST_CLEAR) {
		SetDlgItemTextW(item, wbuf);
	}

	delete [] wbuf;
}

BOOL TMainDlg::SetMiniWindow(void)
{
	GetWindowRect(&rect);
	int cur_height = rect.bottom - rect.top;
	int new_height = cur_height - (isErrEditHide ? 0 : normalHeight - miniHeight);
	int min_height = miniHeight - (isExtendFilter ? 0 : filterHeight);

	isErrEditHide = TRUE;
	if (new_height < min_height) new_height = min_height;

	MoveWindow(rect.left, rect.top, rect.right - rect.left, new_height, IsWindowVisible());
	return	TRUE;
}

BOOL TMainDlg::SetNormalWindow()
{
	if (IsWindowVisible()) {
		GetWindowRect(&rect);
		int diff_size = normalHeight - miniHeight;
		int height = rect.bottom - rect.top + (isErrEditHide ? diff_size : 0);
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

	RECT	screen_rect = { 0, 0,
				::GetSystemMetrics(SM_CXFULLSCREEN), ::GetSystemMetrics(SM_CYFULLSCREEN) };

	HMONITOR	hMon;

	if (isFixPos && !(hMon = ::MonitorFromPoint(cfg.winpos, MONITOR_DEFAULTTONEAREST))) {
		isFixPos = FALSE;
	}
	if (!isFixPos && (hMon = ::MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST)) != NULL) {
		MONITORINFO	minfo;
		minfo.cbSize = sizeof(minfo);

		if (::GetMonitorInfoW(hMon, &minfo) && (minfo.rcMonitor.right - minfo.rcMonitor.left)
				> 0 && (minfo.rcMonitor.bottom - minfo.rcMonitor.top) > 0) {
			screen_rect = minfo.rcMonitor;
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

BOOL TMainDlg::IsForeground()
{
	HWND hForeWnd = GetForegroundWindow();

	return	hForeWnd && hWnd && (hForeWnd == hWnd || ::IsChild(hWnd, hForeWnd)) ? TRUE : FALSE;
}

void TMainDlg::SetFont()
{
	if (statusFont || !*cfg.statusFont) return;

	LOGFONTW lf = {};
	HDC		hDc = ::GetDC(hWnd);

	wcscpy(lf.lfFaceName, cfg.statusFont);
	lf.lfCharSet = DEFAULT_CHARSET;
	POINT pt={}, pt2={};
	pt.y = (int)((int64)::GetDeviceCaps(hDc, LOGPIXELSY) * cfg.statusFontSize / 720);
	::DPtoLP(hDc, &pt,  1);
	::DPtoLP(hDc, &pt2, 1);
	lf.lfHeight = -abs(pt.y - pt2.y);
	::ReleaseDC(hWnd, hDc);

	statusFont = ::CreateFontIndirectW(&lf);
	SendDlgItemMessage(STATUS_EDIT, WM_SETFONT, (WPARAM)statusFont, 0);
}

BOOL TMainDlg::EvCreate(LPARAM lParam)
{
	BOOL is_elevated_admin = IsWinVista() && ::IsUserAnAdmin() && TIsEnableUAC();

	if (is_elevated_admin || isNoUI) {
		SetVersionStr(is_elevated_admin, isNoUI);
	}

	char	title[100];
	sprintf(title, "%s %s%s", FASTCOPY_TITLE, GetVersionStr(), GetVerAdminStr());

	WCHAR	user_log[MAX_PATH];
	MakePathW(user_log, cfg.userDir, L"fastcopy_exception.log");

	InstallExceptionFilter(title, GetLoadStr(IDS_EXCEPTIONLOG), WtoAs(user_log));

	if (IsWinVista()) {
		HMENU	hMenu = ::GetMenu(hWnd);

		if (!::IsUserAnAdmin()) {
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
				::InsertMenuW(hMenu, 4, MF_BYPOSITION|MF_STRING|MF_HELP, ADMIN_MENUITEM,
					GetLoadStrW(IDS_ELEVATE));
			}
		}
	}

	pathEdit.AttachWnd(GetDlgItem(PATH_EDIT));
	errEdit.AttachWnd(GetDlgItem(ERR_EDIT));

	int		i;
	for (i=0; i < MAX_FASTCOPY_ICON; i++) {
		hMainIcon[i] = ::LoadIcon(TApp::GetInstance(), (LPCSTR)(FASTCOPY_ICON + i));
	}
	::SetClassLong(hWnd, GCL_HICON, (LONG)hMainIcon[FCNORMAL_ICON_IDX]);

	hAccel = LoadAccelerators(TApp::GetInstance(), (LPCSTR)IDR_ACCEL);
	SetSize();

	TaskBarCreateMsg = ::RegisterWindowMessage("TaskbarCreated");

	// メッセージセット
	//SetFont();	// STATUS_EDIT を Terminal Font に 去除设置字体
	if (cfg.lcid == 0x409) SetFont(); //仅英文改字体
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
	CheckDlgButton(ACL_CHECK, cfg.enableAcl);
	CheckDlgButton(STREAM_CHECK, cfg.enableStream);

	SendDlgItemMessage(STATUS_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)EditWordBreakProcW);
	SendDlgItemMessage(PATH_EDIT,   EM_SETWORDBREAKPROC, 0, (LPARAM)EditWordBreakProcW);
	SendDlgItemMessage(ERR_EDIT,    EM_SETWORDBREAKPROC, 0, (LPARAM)EditWordBreakProcW);

	SendDlgItemMessage(STATUS_EDIT, EM_SETTARGETDEVICE, 0, 0);		// 折り返し
	SendDlgItemMessage(PATH_EDIT,   EM_SETTARGETDEVICE, 0, 0);		// 折り返し
	SendDlgItemMessage(ERR_EDIT,    EM_SETTARGETDEVICE, 0, 0);		// 折り返し

	SendDlgItemMessage(STATUS_EDIT, EM_LIMITTEXT, 0, 0);
	SendDlgItemMessage(PATH_EDIT,   EM_LIMITTEXT, 0, 0);
	SendDlgItemMessage(ERR_EDIT,    EM_LIMITTEXT, 0, 0);

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
	normalHeightOrg	= normalHeight;

	RECT	exc_rect, date_rect;
	::GetWindowRect(GetDlgItem(EXCLUDE_COMBO), &exc_rect);
	::GetWindowRect(GetDlgItem(FROMDATE_COMBO), &date_rect);
	filterHeight = date_rect.bottom - exc_rect.bottom;

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
		if (!CommandLineExecW(orgArgc, orgArgv) && (!isShellExt || autoCloseLevel >= NOERR_CLOSE)
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

	int	ret = TRUE;

	if (isNoUI) {
		WriteErrLogNoUI("CanceCopy (aborted by system or etc)");
	}
	else if (!isDelay) {
		ret = (TMsgBox(this).Exec(GetLoadStr(IsListing() ? IDS_LISTCONFIRM :
					info.mode == FastCopy::DELETE_MODE ? IDS_DELSTOPCONFIRM : IDS_STOPCONFIRM),
					FASTCOPY, MB_OKCANCEL) == IDOK) ? TRUE : FALSE;
	}

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

	return	ret;
}

BOOL TMainDlg::SwapTargetCore(const WCHAR *s, const WCHAR *d, WCHAR *out_s, WCHAR *out_d)
{
	WCHAR	*src_fname = NULL, *dst_fname = NULL;
	BOOL	isSrcLastBS = s[wcslen(s) - 1] == '\\'; // 95系は無視...
	BOOL	isDstLastBS = d[wcslen(d) - 1] == '\\';
	BOOL	isSrcRoot = FALSE;

	VBuf	buf(MAX_WPATH * sizeof(WCHAR));
	VBuf	srcBuf(MAX_WPATH * sizeof(WCHAR));

	if (!buf.Buf() || !srcBuf.Buf()) return	FALSE;

	if (isSrcLastBS) {
		GetRootDirW(s, buf.WBuf());
		if (wcscmp(s, buf.WBuf()) == 0) {
			isSrcRoot = TRUE;
		}
		else {
			wcscpy(srcBuf.WBuf(), s);
			srcBuf.WBuf()[wcslen(srcBuf.WBuf()) -1] = 0;
			s = srcBuf.WBuf();
		}
	}

	DWORD	attr;
	BOOL	isSrcDir = isSrcLastBS || (attr = ::GetFileAttributesW(s)) == 0xffffffff
			|| (attr & FILE_ATTRIBUTE_DIRECTORY)
				&& (!cfg.isReparse || (attr & FILE_ATTRIBUTE_REPARSE_POINT) == 0);

	if (isSrcDir && !isDstLastBS) {	// dst に '\\' がない場合
		wcscpy(out_d, s);
		wcscpy(out_s, d);
		goto END;
	}

	if (!isDstLastBS) {	// dst 末尾に '\\' を付与
		MakePathW(buf.WBuf(), d, L"");
		d = buf.WBuf();
	}

	if (::GetFullPathNameW(s, MAX_WPATH, out_d, &src_fname) <= 0) return	FALSE; // sを out_d に
	if (::GetFullPathNameW(d, MAX_WPATH, out_s, &dst_fname) <= 0) return	FALSE; // dを out_s に

	if (src_fname) {  // a:\aaa\bbb  d:\ccc\ddd\ --> d:\ccc\ddd\bbb a:\aaa\ にする
		wcscpy(out_s + wcslen(out_s), src_fname);
		src_fname[0] = 0;
		goto END;
	}
	else if (isSrcRoot) {	// a:\  d:\xxx\  -> d:\xxx a:\ にする
		GetRootDirW(out_s, buf.WBuf());
		if (wcscmp(out_s, buf.WBuf()) && !isSrcRoot) {
			out_s[wcslen(out_s) -1] = 0;
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
	srcArray.GetMultiPath(out_s, MAX_WPATH);
	return	TRUE;
}

BOOL TMainDlg::SwapTarget(BOOL check_only)
{
	DWORD	src_len = ::GetWindowTextLengthW(GetDlgItem(SRC_COMBO));
	DWORD	dst_len = ::GetWindowTextLengthW(GetDlgItem(DST_COMBO));

	if (src_len == 0 && dst_len == 0) return FALSE;

	WCHAR		*src = new WCHAR [MAX_WPATH];
	WCHAR		*dst = new WCHAR [MAX_WPATH];
	PathArray	srcArray, dstArray;
	BOOL		ret = FALSE;

	if (src && dst && GetDlgItemTextW(SRC_COMBO, src, src_len+1) == src_len
		&& GetDlgItemTextW(DST_COMBO, dst, dst_len+1) == dst_len) {
		if (srcArray.RegisterMultiPath(src) <= 1 && dstArray.RegisterPath(dst) <= 1
			&& !(srcArray.Num() == 0 && dstArray.Num() == 0)) {
			ret = TRUE;
			if (!check_only) {
				if (srcArray.Num() == 0) {
					dstArray.GetMultiPath(src, MAX_WPATH);
					dst[0] = 0;
				}
				else if (dstArray.Num() == 0) {
					src[0] = 0;
					wcscpy(dst, srcArray.Path(0));
				}
				else {
					ret = SwapTargetCore(srcArray.Path(0), dstArray.Path(0), src, dst);
				}
				if (ret) {
					SetDlgItemTextW(SRC_COMBO, src);
					SetDlgItemTextW(DST_COMBO, dst);
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
		if (modalCount == 0) {	// protect from task scheduler
			if (!fastCopy.IsStarting()) {
				EndDialog(wID);
			}
			else if (fastCopy.IsAborting()) {
				if (isNoUI) {
					WriteErrLogNoUI("force terminated");
					EndDialog(IDCANCEL);
				}
				else {
					if (TMsgBox(this).Exec(GetLoadStr(IDS_CONFIRMFORCEEND),
											FASTCOPY, MB_OKCANCEL) == IDOK) {
						EndDialog(wID);
					}
				}
			}
			else {
				CancelCopy();
				autoCloseLevel = NOERR_CLOSE;
			}
		}
		return	TRUE;

	case ATONCE_BUTTON:
		isDelay = FALSE;
		::KillTimer(hWnd, FASTCOPY_TIMER);
		fastCopy.TakeExclusivePriv(true);
		ExecCopyCore();
		return	TRUE;

	case SRC_FILE_BUTTON:
		BrowseDirDlgW(this, SRC_COMBO, GetLoadStrW(IDS_SRC_SELECT),
						BRDIR_MULTIPATH|BRDIR_CTRLADD|BRDIR_FILESELECT);
		return	TRUE;

	case DST_FILE_BUTTON:
		BrowseDirDlgW(this, DST_COMBO, GetLoadStrW(IDS_DST_SELECT), BRDIR_BACKSLASH);
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
				SetDlgItemTextW(SRC_COMBO, L"");
				isNetPlaceSrc = FALSE;
			}
			SetItemEnable(is_delete);
		}
		return	TRUE;

	case FILTER_CHECK:
		ReflectFilterCheck();
		return	TRUE;

	case FILTERHELP_BUTTON:
		ShowHelpW(0, cfg.execDir, GetLoadStrW(IDS_FASTCOPYHELP), L"#filter");
		return	TRUE;

	case OPENDIR_MENUITEM:
		::ShellExecuteW(NULL, NULL, cfg.userDir, 0, 0, SW_SHOW);
		return	TRUE;

	case OPENLOG_MENUITEM:
		{
			SHELLEXECUTEINFOW	sei = {};
			sei.cbSize = sizeof(SHELLEXECUTEINFO);
			sei.fMask = 0;
			sei.lpVerb = NULL;
			sei.lpFile = errLogPath;
			sei.lpParameters = NULL;
			sei.nShow = SW_NORMAL;
			::ShellExecuteExW(&sei);
		}
		return	TRUE;

	case FILELOG_MENUITEM:
		{
			SHELLEXECUTEINFOW	sei = {};
			sei.cbSize = sizeof(SHELLEXECUTEINFO);
			sei.fMask = 0;
			sei.lpVerb = NULL;
			sei.lpFile = lastFileLog;
			sei.lpParameters = NULL;
			sei.nShow = SW_NORMAL;
			::ShellExecuteExW(&sei);
		}
		return	TRUE;

	case FIXPOS_MENUITEM:
		if (IS_INVALID_POINT(cfg.winpos)) {
			GetWindowRect(&rect);
			cfg.winpos.x = rect.left;
			cfg.winpos.y = rect.top;
			cfg.WriteIni();
		}
		else {
			cfg.winpos.x = cfg.winpos.y = INVALID_POINTVAL;
			cfg.WriteIni();
		}
		return	TRUE;

	case FIXSIZE_MENUITEM:
		if (IS_INVALID_SIZE(cfg.winsize)) {
			GetWindowRect(&rect);
			cfg.winsize.cx = (rect.right - rect.left) - (orgRect.right - orgRect.left);
			cfg.winsize.cy = (rect.bottom - rect.top)
								- (isErrEditHide ? miniHeight : normalHeight);
			cfg.WriteIni();
		}
		else {
			cfg.winsize.cx = cfg.winsize.cy = INVALID_SIZEVAL;
			cfg.WriteIni();
		}
		return	TRUE;

	case SETUP_MENUITEM:
		if (setupDlg.Exec() == IDOK) {
			SetCopyModeList();
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
		::ShellExecuteW(NULL, NULL, GetLoadStrW(IDS_FASTCOPYURL), NULL, NULL, SW_SHOW);
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

	case HELP_BUTTON:
		ShowHelpW(0, cfg.execDir, GetLoadStrW(IDS_FASTCOPYHELP), L"#usage");
		return	TRUE;

	case HELP_MENUITEM:
		ShowHelpW(0, cfg.execDir, GetLoadStrW(IDS_FASTCOPYHELP), NULL);
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

//BOOL TMainDlg::EvNotify(UINT ctlID, NMHDR *pNmHdr)
//{
//	return FALSE;
//}

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
	BOOL	is_exec = fastCopy.IsStarting() || isDelay;
	BOOL	is_filter = IsDlgButtonChecked(FILTER_CHECK);
	BOOL	is_path_grow = (isErrEditHide || IsListing()); // == !err_grow
	int		erredit_diff = is_path_grow ? (normalHeightOrg - normalHeight) : 0;

	int	slide_y[] = { errstatus_item, errstatic_item, -1 };
	for (target=slide_y; *target != -1; target++) {
		item = dlgItems + *target;
		hdwp = ::DeferWindowPos(hdwp, item->hWnd, 0, item->wpos.x,
			item->wpos.y + (is_path_grow ? ydiff + erredit_diff : 0) + ydiffex,
			item->wpos.cx, item->wpos.cy, isErrEditHide ? dwHideFlg : dwFlg);
	}

	item = dlgItems + erredit_item;
	hdwp = ::DeferWindowPos(hdwp, item->hWnd, 0, item->wpos.x,
		item->wpos.y + (is_path_grow ? ydiff + erredit_diff : 0) + ydiffex,
		item->wpos.cx + xdiff, item->wpos.cy + (is_path_grow ? -erredit_diff : ydiff),
		isErrEditHide ? dwHideFlg : dwFlg);

	item = dlgItems + path_item;
	hdwp = ::DeferWindowPos(hdwp, item->hWnd, 0, item->wpos.x, item->wpos.y + ydiffex,
		item->wpos.cx + xdiff, item->wpos.cy + (is_path_grow ? ydiff + erredit_diff : 0), dwFlg);

/*
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
*/
	int	slide_x[] = { ok_item, list_item, samedrv_item, top_item, estimate_item, verify_item,
					  speed_item, speedstatic_item, ignore_item, bufedit_item, bufstatic_item,
					  help_item, mode_item, filter_item, filterhelp_item, -1 };
	for (target=slide_x; *target != -1; target++) {
		item = dlgItems + *target;
		hdwp = ::DeferWindowPos(hdwp, item->hWnd, 0, item->wpos.x + xdiff,
			item->wpos.y, item->wpos.cx, item->wpos.cy,
			is_exec && (*target == list_item && !IsListing()
			|| *target == ok_item && IsListing()) || is_delete
				&& (*target == estimate_item || *target == verify_item)
			|| (*target == filterhelp_item && !is_filter) ? dwHideFlg : dwFlg);
	}

	int	grow_x[] = { inccombo_item, exccombo_item, srccombo_item, dstcombo_item, -1 };
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
		::EnableWindow(dlgItems[acl_item].hWnd, flg);
		::EnableWindow(dlgItems[stream_item].hWnd, flg);
	//	::EnableWindow(dlgItems[filterhelp_item].hWnd, flg);
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
	SetDlgItem(FILTERHELP_BUTTON);
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
	int		max = isDstDrop ? 1 : ::DragQueryFileW(hDrop, 0xffffffff, 0, 0), max_len;

	// CTL が押されている場合、現在の内容を加算
	if (!isDstDrop) {
		if (::GetAsyncKeyState(VK_CONTROL) & 0x8000) {
			max_len = ::GetWindowTextLengthW(GetDlgItem(SRC_COMBO)) + 1;
			WCHAR	*buf = new WCHAR [max_len];
			GetDlgItemTextW(SRC_COMBO, buf, max_len);
			pathArray.RegisterMultiPath(buf);
			delete [] buf;
		}
		else
			isNetPlaceSrc = FALSE;
	}

	for (int i=0; i < max; i++) {
		if (::DragQueryFileW(hDrop, i, path, wsizeof(path)) <= 0)
			break;

		if (isDstDrop || !isDeleteMode) {
			if (NetPlaceConvertW(path, path) && !isDstDrop)
				isNetPlaceSrc = TRUE;
		}
		pathArray.RegisterPath(path);
	}
	::DragFinish(hDrop);

	if (pathArray.Num() > 0) {
		if (isDstDrop) {
			DWORD	attr = ::GetFileAttributesW(pathArray.Path(0));
			if (attr & FILE_ATTRIBUTE_DIRECTORY) {	// 0xffffffff も認める(for 95系OSのroot対策)
				MakePathW(path, pathArray.Path(0), L"");
				SetDlgItemTextW(DST_COMBO, path);
			}
		}
		else {
			WCHAR	*buf = new WCHAR [max_len = pathArray.GetMultiPathLen()]; // 文字列連結用領域
			if (pathArray.GetMultiPath(buf, max_len)) {
				SetDlgItemTextW(SRC_COMBO, buf);
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
	::EnableWindow(GetDlgItem(INC_STATIC),     new_status);
	::EnableWindow(GetDlgItem(EXC_STATIC),     new_status);
	::EnableWindow(GetDlgItem(FROMDATE_COMBO), new_status);
	::EnableWindow(GetDlgItem(TODATE_COMBO),   new_status);
	::EnableWindow(GetDlgItem(MINSIZE_COMBO),  new_status);
	::EnableWindow(GetDlgItem(MAXSIZE_COMBO),  new_status);
	::ShowWindow(GetDlgItem(FILTERHELP_BUTTON), is_checked ? SW_SHOW : SW_HIDE);
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

int64 TMainDlg::GetDateInfo(WCHAR *buf, BOOL is_end)
{
	WCHAR		*p = NULL, tmp[MINI_BUF];
	FILETIME	ft;
	SYSTEMTIME	st;
	int64		&t = *(int64 *)&ft;
	int64		val;

	for (WCHAR *s=buf, *d=tmp; *d = *s; s++) {
		if (*s != '/') d++;
	}
	val = wcstol(tmp, &p, 10);

	if (val > 0 && !wcschr(buf, '+')) {	// absolute
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
	else if (p && p[0]) {
		::GetSystemTime(&st);
		::SystemTimeToFileTime(&st, &ft);
		switch (p[0]) {
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

int64 TMainDlg::GetSizeInfo(WCHAR *buf)
{
	WCHAR	*p=NULL;
	int64	val;

	if ((val = wcstol(buf, &p, 0)) < 0) return -2;

	if (val == 0 && p == buf) {	// 変換すべき数字が無い
		for (int i=0; p[i]; i++) {
			if (!strchr(" \t\r\n", p[i])) return -2;
		}
		return	-1;
	}
	int c = p ? p[0] : ' ';

	switch (toupper(c)) {
	case 'T': val *= (int64)1024 * 1024 * 1024 * 1024;	break;
	case 'G': val *= (int64)1024 * 1024 * 1024;		break;
	case 'M': val *= (int64)1024 * 1024;				break;
	case 'K': val *= (int64)1024;						break;
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
	BOOL	is_ctrkey   = (::GetAsyncKeyState(VK_CONTROL) & 0x8000) ? TRUE : FALSE;
	BOOL	is_shiftkey = (::GetAsyncKeyState(VK_SHIFT) & 0x8000) ? TRUE : FALSE;

	// コピーと同じように、削除も排他実行する
	if (is_delete_mode && is_fore && is_shiftkey) forceStart = 2;
	isListLog = (is_listing && is_shiftkey) ? TRUE : FALSE;

	info.ignoreEvent	= (IsDlgButtonChecked(IGNORE_CHECK) ? FASTCOPY_ERROR_EVENT : 0) |
							(noConfirmStop ? FASTCOPY_STOP_EVENT : 0);
	info.mode			= copyInfo[idx].mode;
	info.overWrite		= copyInfo[idx].overWrite;
	info.lcid			= cfg.lcid > 0 ? ::GetThreadLocale() : 0;
	info.flags			= cfg.copyFlags
//		| (FastCopy::REPORT_ACL_ERROR | FastCopy::REPORT_STREAM_ERROR)
		| (is_listing ? FastCopy::LISTING_ONLY : 0)
		| (!is_listing && fileLogMode != NO_FILELOG ? FastCopy::LISTING : 0)
		| (IsDlgButtonChecked(ACL_CHECK) ? FastCopy::WITH_ACL : 0)
		| (IsDlgButtonChecked(STREAM_CHECK) ? FastCopy::WITH_ALTSTREAM : 0)
		| (cfg.aclErrLog ? FastCopy::REPORT_ACL_ERROR : 0)
		| (cfg.streamErrLog ? FastCopy::REPORT_STREAM_ERROR : 0)
		| (!is_delete_mode && IsDlgButtonChecked(ESTIMATE_CHECK) && !is_listing ?
			FastCopy::PRE_SEARCH : 0)
		| ((!is_listing && IsDlgButtonChecked(VERIFY_CHECK)) ? FastCopy::VERIFY_FILE : 0)
		| (is_delete_mode && IsDlgButtonChecked(OWDEL_CHECK) ? cfg.enableNSA ?
			FastCopy::OVERWRITE_DELETE_NSA : FastCopy::OVERWRITE_DELETE : 0)
		| (is_delete_mode && cfg.delDirWithFilter ? FastCopy::DELDIR_WITH_FILTER : 0)
		| (cfg.isSameDirRename ? FastCopy::SAMEDIR_RENAME : 0)
		| (cfg.isAutoSlowIo ? FastCopy::AUTOSLOW_IOLIMIT : 0)
		| (cfg.isReadOsBuf ? FastCopy::USE_OSCACHE_READ : 0)
		| (cfg.isWShareOpen ? FastCopy::WRITESHARE_OPEN : 0)
		| (cfg.usingMD5 ? FastCopy::VERIFY_MD5 : 0)
		| (skipEmptyDir && is_filter ? FastCopy::SKIP_EMPTYDIR : 0)
		| (!isReparse && info.mode != FastCopy::MOVE_MODE && info.mode != FastCopy::DELETE_MODE ?
			FastCopy::USE_REPARSE : 0)
		| ((is_listing && is_fore && is_ctrkey) ? FastCopy::VERIFY_FILE : 0)
		| (info.mode == FastCopy::MOVE_MODE && cfg.serialMove ? FastCopy::SERIAL_MOVE : 0)
		| (info.mode == FastCopy::MOVE_MODE && cfg.serialVerifyMove ?
			FastCopy::SERIAL_VERIFY_MOVE : 0)
		| (isLinkDest ? FastCopy::RESTORE_HARDLINK : 0)
		| (isReCreate ? FastCopy::DEL_BEFORE_CREATE : 0)
		| ((diskMode == 2 || info.mode == FastCopy::TESTWRITE_MODE) ? FastCopy::FIX_DIFFDISK :
			diskMode == 1 ? FastCopy::FIX_SAMEDISK : 0);
	info.flags			&= ~cfg.copyUnFlags;
	info.timeDiffGrace	= (int64)cfg.timeDiffGrace * 10000; // 1ms -> 100ns

	info.fileLogFlags	= (!is_listing || is_ctrkey || is_shiftkey) ? cfg.fileLogFlags : 0;
	info.debugFlags		= cfg.debugFlags;

	info.bufSize		= (int64)GetDlgItemInt(BUFSIZE_EDIT) * 1024 * 1024;
	info.maxRunNum		= cfg.maxRunNum;
	info.maxTransSize	= cfg.maxTransSize * 1024 * 1024;
	info.netDrvMode		= cfg.netDrvMode;
	info.aclReset		= cfg.aclReset;
	info.maxOvlSize		= cfg.maxOvlSize > 0 ? (cfg.maxOvlSize * 1024 * 1024) : -1;
	info.maxOvlNum		= cfg.maxOvlNum;
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
	int		src_len = ::GetWindowTextLengthW(GetDlgItem(SRC_COMBO)) + 1;
	int		dst_len = ::GetWindowTextLengthW(GetDlgItem(DST_COMBO)) + 1;
	if (src_len <= 1 || !is_delete_mode && dst_len <= 1) {
		return	FALSE;
	}

	WCHAR	*src = new WCHAR [src_len], dst[MAX_PATH_EX] = L"";
	BOOL	ret = TRUE;
	BOOL	exec_confirm = (!isNoUI && (cfg.execConfirm ||
										(is_fore && (::GetAsyncKeyState(VK_CONTROL) & 0x8000))));

	if (!exec_confirm) {
		if (isShellExt && (exec_flags & CMDLINE_EXEC))
			exec_confirm = is_delete_mode ? !shextNoConfirmDel : !shextNoConfirm;
		else
			exec_confirm = is_delete_mode ? !noConfirmDel : cfg.execConfirm;
	}

	if (GetDlgItemTextW(SRC_COMBO, src, src_len) == 0
	|| !is_delete_mode && GetDlgItemTextW(DST_COMBO, dst, MAX_PATH_EX) == 0) {
		const char	*msg = "Can't get source or destination directory field";
		if (isNoUI) {
			WriteErrLogNoUI(msg);
		}
		else {
			TMsgBox(this).Exec(msg);
		}
		ret = FALSE;
	}
	SendDlgItemMessage(PATH_EDIT, WM_SETTEXT, 0, (LPARAM)"");
	SendDlgItemMessage(STATUS_EDIT, WM_SETTEXT, 0, (LPARAM)"");
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
		DWORD	inc_len = ::GetWindowTextLengthW(GetDlgItem(INCLUDE_COMBO));
		DWORD	exc_len = ::GetWindowTextLengthW(GetDlgItem(EXCLUDE_COMBO));
		inc = new WCHAR [inc_len + 1];
		exc = new WCHAR [exc_len + 1];
		if (GetDlgItemTextW(INCLUDE_COMBO, inc, inc_len +1) == inc_len &&
			GetDlgItemTextW(EXCLUDE_COMBO, exc, exc_len +1) == exc_len) {
			incArray.RegisterMultiPath(inc);
			excArray.RegisterMultiPath(exc);
		}
		else ret = FALSE;

		if (isExtendFilter) {
			if (GetDlgItemTextW(FROMDATE_COMBO, from_date, MINI_BUF) > 0)
				info.fromDateFilter = GetDateInfo(from_date, FALSE);
			if (GetDlgItemTextW(TODATE_COMBO,   to_date,   MINI_BUF) > 0)
				info.toDateFilter   = GetDateInfo(to_date, TRUE);
			if (GetDlgItemTextW(MINSIZE_COMBO,  min_size,  MINI_BUF) > 0)
				info.minSizeFilter  = GetSizeInfo(min_size);
			if (GetDlgItemTextW(MAXSIZE_COMBO,  max_size,  MINI_BUF) > 0)
				info.maxSizeFilter  = GetSizeInfo(max_size);

			if (info.fromDateFilter == -1 || info.toDateFilter  == -1) {
				const char *msg = GetLoadStr(IDS_DATEFORMAT_MSG);
				if (isNoUI) {
					WriteErrLogNoUI(AtoU8(msg));
				}
				else {
					TMsgBox(this).Exec(msg);
				}
				ret = FALSE;
			}
			if (info.minSizeFilter  == -2 || info.maxSizeFilter == -2) {
				const char *msg = GetLoadStr(IDS_SIZEFORMAT_MSG);
				if (isNoUI) {
					WriteErrLogNoUI(msg);
				}
				else {
					TMsgBox(this).Exec(AtoU8(msg));
				}
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
	WCHAR *src_list = new WCHAR [src_list_len];

	if (ret && exec_confirm && (exec_flags & LISTING_EXEC) == 0) {
		srcArray.GetMultiPath(src_list, src_list_len, NEWLINE_STR, L"");
		WCHAR	*title =
					info.mode == FastCopy::MOVE_MODE   ? GetLoadStrW(IDS_MOVECONFIRM) :
					info.mode == FastCopy::SYNCCP_MODE ? GetLoadStrW(IDS_SYNCCONFIRM) :
					info.isRenameMode ? GetLoadStrW(IDS_DUPCONFIRM): NULL;
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

	int	pathLogMax = src_len * sizeof(WCHAR) + sizeof(dst);

	if ((ret || is_initerr_logging) && (pathLogBuf = new char [pathLogMax])) {
		int len = sprintf(pathLogBuf, "<%s> %s" //Source
					, isUtf8Log ? AtoU8s(GetLoadStr(IDS_Log_Source)) : GetLoadStr(IDS_Log_Source) //Source
					, isUtf8Log ? WtoU8s(src) : WtoAs(src)
					);
		if (!is_delete_mode) {
			len += sprintf(pathLogBuf + len, "\r\n<%s> %s" //DestDir
						, isUtf8Log ? AtoU8s(GetLoadStr(IDS_Log_DestDir)) : GetLoadStr(IDS_Log_DestDir) //DestDir
						, isUtf8Log ? WtoU8s(dst) : WtoAs(dst)
						);
		}
		if (inc && inc[0]) {
			len += sprintf(pathLogBuf + len, "\r\n<%s> %s" //Includ
						, isUtf8Log ? AtoU8s(GetLoadStr(IDS_Log_Includ)) : GetLoadStr(IDS_Log_Includ) //Includ
						, isUtf8Log ? WtoU8s(inc) : WtoAs(inc)
						);
		}
		if (exc && exc[0]) {
			len += sprintf(pathLogBuf + len, "\r\n<%s> %s" //Exclude
						, isUtf8Log ? AtoU8s(GetLoadStr(IDS_Log_Exclude)) : GetLoadStr(IDS_Log_Exclude) //Exclude
						, isUtf8Log ? WtoU8s(exc) : WtoAs(exc)
						);
		}
	
		len += sprintf(pathLogBuf + len, "\r\n<%s> %s" //Command
					, isUtf8Log ? AtoU8s(GetLoadStr(IDS_Log_Command)) : GetLoadStr(IDS_Log_Command) //Command
					, isUtf8Log ? AtoU8s(copyInfo[idx].list_str) : copyInfo[idx].list_str
					);
		if (info.flags & (FastCopy::WITH_ACL|FastCopy::WITH_ALTSTREAM|FastCopy::OVERWRITE_DELETE
						 |FastCopy::OVERWRITE_DELETE_NSA|FastCopy::VERIFY_FILE)) {
			len += sprintf(pathLogBuf + len, " (%s"
						, isUtf8Log ? AtoU8s(GetLoadStr(IDS_Log_with)) : GetLoadStr(IDS_Log_with) //with
						);
			if (!is_delete_mode && (info.flags & FastCopy::VERIFY_FILE))
				len += sprintf(pathLogBuf + len, " %s"
							, isUtf8Log ? AtoU8s(GetLoadStr(IDS_Log_Verify)) : GetLoadStr(IDS_Log_Verify) //Verify
							);
			if (!is_delete_mode && (info.flags & FastCopy::WITH_ACL))
				len += sprintf(pathLogBuf + len, " %s"
							, isUtf8Log ? AtoU8s(GetLoadStr(IDS_Log_ACL)) : GetLoadStr(IDS_Log_ACL) //ACL
							);
			if (!is_delete_mode && (info.flags & FastCopy::WITH_ALTSTREAM))
				len += sprintf(pathLogBuf + len, " %s"
							, isUtf8Log ? AtoU8s(GetLoadStr(IDS_Log_AltStream)) : GetLoadStr(IDS_Log_AltStream) //AltStream
							);
			if (is_delete_mode && (info.flags & FastCopy::OVERWRITE_DELETE))
				len += sprintf(pathLogBuf + len, " %s"
							, isUtf8Log ? AtoU8s(GetLoadStr(IDS_Log_OverWrite)) : GetLoadStr(IDS_Log_OverWrite) //OverWrite
							);
			if (is_delete_mode && (info.flags & FastCopy::OVERWRITE_DELETE_NSA))
				len += sprintf(pathLogBuf + len, " %s"
							, isUtf8Log ? AtoU8s(GetLoadStr(IDS_Log_NSA)) : GetLoadStr(IDS_Log_NSA) //NSA
							);
			len += sprintf(pathLogBuf + len, ")");
		}
		if (is_listing && isListLog) len += sprintf(pathLogBuf + len, " (%s)"
											, isUtf8Log ? AtoU8s(GetLoadStr(IDS_Log_ListingOnly)) : GetLoadStr(IDS_Log_ListingOnly) //Listing only
											);

		WCHAR	job[MAX_PATH] = L"";
		GetDlgItemTextW(JOBTITLE_STATIC, job, MAX_PATH);
		if (*job) {
			len += sprintf(pathLogBuf + len, "\r\n<%s> %s"
				, isUtf8Log ? AtoU8s(GetLoadStr(IDS_Log_JobName)) : GetLoadStr(IDS_Log_JobName) //JobName
				, isUtf8Log ? WtoU8s(job) : WtoAs(job)
				);
		}
		len += sprintf(pathLogBuf + len , "\r\n");
	}

	delete [] exc;
	delete [] inc;
	delete [] src_list;
	delete [] src;

	if (ret) {
		SendDlgItemMessage(PATH_EDIT, EM_SETTARGETDEVICE, 0, IsListing() ? 1 : 0);	// 折り返し
		SendDlgItemMessage(ERR_EDIT,  EM_SETTARGETDEVICE, 0, IsListing() ? 0 : 1);	// 折り返し
		SetMiniWindow();
		normalHeight = normalHeightOrg;
		SetDlgItemText(ERR_EDIT, "");
		ShareInfo::CheckInfo	ci;

		if ((is_delete_mode && forceStart != 2) || fastCopy.IsTakeExclusivePriv()
		|| fastCopy.TakeExclusivePriv(forceStart ? true : false, &ci) || forceStart) {
			ret = ExecCopyCore();
		}
		else {
			isDelay = TRUE;
			SetDlgItemText(STATUS_EDIT, GetLoadStr((ci.all_running >= cfg.maxRunNum) ?
				IDS_WAIT_MANYPROC : IDS_WAIT_ACQUIREDRV));
			::SetTimer(hWnd, FASTCOPY_TIMER, FASTCOPY_TIMER_TICK*2, NULL);
			if (isTaskTray) {
				TaskTray(NIM_MODIFY, hMainIcon[FCWAIT_ICON_IDX], FASTCOPY);
			}
			else {
				::SetClassLong(hWnd, GCL_HICON, (LONG)hMainIcon[FCWAIT_ICON_IDX]);//变为等待图标
			}
		}

		if (ret) {
			SetDlgItemText(IsListing() ? LIST_BUTTON : IDOK, GetLoadStr(IDS_CANCEL));
			RefreshWindow(TRUE);
			if (IsWinVista() && !::IsUserAnAdmin()) {
				::EnableMenuItem(GetMenu(hWnd), ADMIN_MENUITEM,
					MF_BYCOMMAND|MF_DISABLED|MF_GRAYED);
				::DrawMenuBar(hWnd);
			}
		}
	}

	if (!ret) {
		if (pathLogBuf) {
			if (is_initerr_logging) {
				WriteErrLogAtStart();
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
		RefreshWindow(TRUE);
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

		if (!IsListing() && isErrLog) {
			WriteErrLogNormal();
		}
		if (fileLogMode != NO_FILELOG && (isListLog || !IsListing())) {
			EndFileLog();
		}
		::EnableWindow(GetDlgItem(IsListing() ? LIST_BUTTON : IDOK), FALSE);
		fastCopy.End();
		::EnableWindow(GetDlgItem(IsListing() ? LIST_BUTTON : IDOK), TRUE);
	}
	else {
		fastCopy.ReleaseExclusivePriv();	// start後は fastCopy.End() で自動発行
		SendDlgItemMessage(STATUS_EDIT, WM_SETTEXT, 0, (LPARAM)GetLoadStr(IDS_EndCopy_Cancelled)); // ---- Cancelled. ----
		::SetClassLong(hWnd, GCL_HICON, (LONG)hMainIcon[FCNORMAL_ICON_IDX]);//图标回到正常
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
		if (is_auto_close || isNoUI) {
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
	if (IsWinVista() && !::IsUserAnAdmin()) {
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

	if (act->sound[0] && (!(act->flags & FinAct::ERR_SOUND) || is_err)) {
		::PlaySoundW(act->sound, 0, SND_FILENAME|(is_sound_wait ? 0 : SND_ASYNC));
	}

	int	flg = (act->flags & FinAct::ERR_CMD)    ? FinAct::ERR_CMD    :
			  (act->flags & FinAct::NORMAL_CMD) ? FinAct::NORMAL_CMD : 0;

	if (act->command[0] &&
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

			Wstr	param_str(param.GetMultiPathLen());
			param.GetMultiPath(param_str.Buf(), param.GetMultiPathLen(), L" ");

			SHELLEXECUTEINFOW	sei = {};
			sei.cbSize = sizeof(SHELLEXECUTEINFO);
			sei.fMask = is_wait ? SEE_MASK_NOCLOSEPROCESS : 0;
			sei.lpVerb = NULL;
			sei.lpFile = path.Path(0);
			sei.lpParameters = param_str.Buf();
			sei.lpDirectory = NULL;
			sei.nShow = SW_NORMAL;
			if (::ShellExecuteExW(&sei) && is_wait && sei.hProcess) {
				char	buf[512], svbuf[512];
				GetDlgItemText(PATH_EDIT, svbuf, 512);
				_snprintf(buf, sizeof(buf), "%s\r\n%s", svbuf, GetLoadStr(IDS_FinalAction_Wait)); //Wait for finish action...
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

				if (isNoUI) { // 本来ここには到達しないはずだが…
					WriteErrLogNoUI(WtoU8(confirm->message));
					confirm->result = FastCopy::Confirm::CANCEL_RESULT;
				}
				else {
					switch (TConfirmDlg().Exec(confirm->message, confirm->allow_continue, this)) {
					case IDIGNORE:	confirm->result = FastCopy::Confirm::IGNORE_RESULT;		break;
					case IDCANCEL:	confirm->result = FastCopy::Confirm::CANCEL_RESULT;		break;
					default:		confirm->result = FastCopy::Confirm::CONTINUE_RESULT;	break;
					}
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
				TaskTray(NIM_ADD, hMainIcon[isDelay ? FCWAIT_ICON_IDX : FCNORMAL_ICON_IDX],
					FASTCOPY);
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
		ShareInfo::CheckInfo	ci;
		if (fastCopy.TakeExclusivePriv(false, &ci)) {
			::KillTimer(hWnd, FASTCOPY_TIMER);
			isDelay = FALSE;
			ExecCopyCore();
		}
		else {
			SetDlgItemText(STATUS_EDIT, GetLoadStr((ci.all_running >= cfg.maxRunNum) ?
				IDS_WAIT_MANYPROC : IDS_WAIT_ACQUIREDRV));
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

/* モーダルダイアログを出しているかどうかを判定可能に（NoUI用） */
int TMainDlg::MessageBoxW(LPCWSTR msg, LPCWSTR title, UINT style)
{
	if (isNoUI) {
		WCHAR	wbuf[2048];
		if (title && *title) {
			swprintf_s(wbuf, wsizeof(wbuf), L"%s: %s", title, msg);
		} else {
			wcscpy(wbuf, msg);
		}
		WriteErrLogNoUI(WtoU8s(wbuf));
		return	IDCANCEL;
	}
	return TWin::MessageBoxW(msg, title, style);
}

int TMainDlg::MessageBoxU8(LPCSTR msg, LPCSTR title, UINT style)
{
	Wstr	wmsg(msg), wtitle(title);
	return MessageBoxW(wmsg, wtitle, style);
}

int TMainDlg::MessageBox(LPCSTR msg, LPCSTR title, UINT style)
{
	Wstr	wmsg(msg, BY_MBCS), wtitle(title, BY_MBCS);
	return MessageBoxW(wmsg, wtitle, style);
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
	SHELLEXECUTEINFOW	sei = {};
	WCHAR				buf[MAX_PATH];
	DWORD				size = MAX_PATH;

	swprintf(buf, L"/runas=%p,%x,\"%s\",\"%s\"",
		hWnd, flg, cfg.userDir, cfg.virtualDir ? cfg.virtualDir : L"");
	sei.cbSize = sizeof(SHELLEXECUTEINFOW);
	sei.lpVerb = L"runas";
	sei.lpFile = (WCHAR *)cfg.execPath;
	sei.lpDirectory = (WCHAR *)cfg.execDir;
	sei.nShow = /*SW_MAXIMIZE*/ SW_NORMAL;
	sei.lpParameters = buf;

	EnableWindow(FALSE);
	isRunAsParent = ::ShellExecuteExW(&sei);
	EnableWindow(TRUE);

	return	isRunAsParent;
}

BOOL CopyItem(HWND hSrc, HWND hDst)
{
	if (!hSrc || !hDst) return	FALSE;

	DWORD	len = (DWORD)::SendMessageW(hSrc, WM_GETTEXTLENGTH, 0, 0) + 1;
	WCHAR	*wbuf = new WCHAR [len];

	if (!wbuf) return	FALSE;

	::SendMessageW(hSrc, WM_GETTEXT, len, (LPARAM)wbuf);
	::SendMessageW(hDst, WM_SETTEXT, 0, (LPARAM)wbuf);
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

BOOL TMainDlg::TaskTray(int nimMode, HICON hSetIcon, LPCSTR tip, BOOL balloon)
{
	isTaskTray = (nimMode == NIM_DELETE) ? FALSE : TRUE;
	BOOL	ret = FALSE;

	if (cfg.taskbarMode) {
		if (!hSetIcon) hSetIcon = hMainIcon[FCNORMAL_ICON_IDX];
		ret = (BOOL)SendMessage(WM_SETICON, ICON_BIG, (LPARAM)hSetIcon);
	}
	else {
		NOTIFYICONDATA	tn = { IsWinVista() ? sizeof(tn) : NOTIFYICONDATA_V2_SIZE };
		tn.hWnd = hWnd;
		tn.uID = FASTCOPY_NIM_ID;		// test
		tn.uFlags = NIF_MESSAGE|(hSetIcon ? NIF_ICON : 0)|(tip ? NIF_TIP : 0);
		tn.uCallbackMessage = WM_FASTCOPY_NOTIFY;
		tn.hIcon = hSetIcon;
		if (tip) sprintf(tn.szTip, "%.127s", tip);

		if (balloon && tip) {
			tn.uFlags |= NIF_INFO;
			strncpyz(tn.szInfo, tip, sizeof(tn.szInfo));
			strncpyz(tn.szInfoTitle, FASTCOPY, sizeof(tn.szInfoTitle));
		}

		ret = ::Shell_NotifyIcon(nimMode, &tn);

		if (isTaskTray) {
			static BOOL once_result = ForceSetTrayIcon(hWnd, FASTCOPY_NIM_ID);
		}
	}
	return	ret;
}

#ifndef SF_UNICODE
#define SF_UNICODE                                  0x00000010
#endif

DWORD CALLBACK RichEditStreamCallBack(DWORD_PTR dwCookie, BYTE *buf, LONG cb, LONG *pcb)
{
	WCHAR	*&text = *(WCHAR **)dwCookie;
	int		max_len = cb / sizeof(WCHAR);
	int		len = wcsncpyz((WCHAR *)buf, text, max_len);

	text += len;
	*pcb = len * sizeof(WCHAR);

	return	0;
}

BOOL RichEditSetText(HWND hWnd, WCHAR *text, int start=-1, int end=-1)
{
	SendMessageW(hWnd, EM_SETSEL, (WPARAM)start, (LPARAM)end);

	EDITSTREAM es = { (DWORD_PTR)&text, 0, RichEditStreamCallBack };
	return	(BOOL)SendMessageW(hWnd, EM_STREAMIN, (SF_TEXT|SFF_SELECTION|SF_UNICODE), (LPARAM)&es);
}


void TMainDlg::SetListInfo()
{
	RichEditSetText(GetDlgItem(PATH_EDIT), ti.listBuf->WBuf());
	listBufOffset += (int)ti.listBuf->UsedSize();

	if (info.fileLogFlags) SetFileLogInfo();

	ti.listBuf->SetUsedSize(0);
}

int mega_str(char *buf, int64 val)
{
	if (val >= (10 * 1024 * 1024)) {
		return	comma_int64(buf, val / 1024 / 1024);
	}
	return	comma_double(buf, (double)val / 1024 / 1024, 1);
}

int double_str(char *buf, double val)
{
	return	comma_double(buf, val, val < 10.0 ? 2 : val < 1000.0 ? 1 : 0);
}

int time_str(char *buf, DWORD sec)
{
	DWORD	h = sec / 3600;
	DWORD	m = (sec % 3600) / 60;
	DWORD	s = (sec % 60);

	if (h == 0) return sprintf(buf, "%02u:%02u", m, s);

	return	sprintf(buf, "%02u:%02u:%02u", h, m, s);
}

int ticktime_str(char *buf, DWORD tick)
{
	if (tick < 60000) return sprintf(buf, "%.1f %s"
		, (double)tick / 1000
		, GetLoadStr(IDS_Info_sec) //sec
		);
	return	time_str(buf, tick/1000);
}

BOOL TMainDlg::SetTaskTrayInfo(BOOL is_finish_status, double doneRate, int remain_sec)
{
	int		len = 0;
	char	buf[1024], s1[64], s2[64], s3[64], s4[64];

	*buf = 0;
	if (!cfg.taskbarMode) {
		if (info.mode == FastCopy::DELETE_MODE) {
			mega_str(s1, ti.total.deleteTrans);
			comma_int64(s2, ti.total.deleteFiles);
			double_str(s3, (double)ti.total.deleteFiles * 1000 / ti.tickCount);
			ticktime_str(s4, ti.tickCount);

			len += sprintf(buf + len, IsListing() ?
				"FastCopy (%s%s %s%s %s)" :
				"FastCopy (%s%s %s%s %s%s %s)"
				, s1
				, GetLoadStr(IDS_Info_MB) //MB
				, s2
				, GetLoadStr(IDS_Info_files) //files
				, IsListing() ? s4 : s3
				, GetLoadStr(IDS_Info_MB_s) //MB/s
				, s4
				);
		}
		else if (ti.total.isPreSearch) {
			mega_str(s1, ti.total.preTrans);
			comma_int64(s2, ti.total.preFiles);
			ticktime_str(s3, ti.tickCount);

			len += sprintf(buf + len, "%s (%s %s %s/%s %s/%s)"
				, GetLoadStr(IDS_TaskTrayInfo_Estimating) //Estimating
				, GetLoadStr(IDS_TaskTrayInfo_Total) //Total
				, s1
				, GetLoadStr(IDS_Info_MB) //MB
				, s2
				, GetLoadStr(IDS_Info_files) //files
				, s3
				);
		}
		else {
			if ((info.flags & FastCopy::PRE_SEARCH) && !ti.total.isPreSearch && !is_finish_status
			&& doneRate >= 0.0001) {
				time_str(s1, remain_sec);
				len += sprintf(buf + len, "%d%% (%s %s) "
					, doneRatePercent
					, GetLoadStr(IDS_TaskTrayInfo_Remain) //Remain
					, s1
					);
			}
			mega_str(s1, ti.total.writeTrans);
			comma_int64(s2, ti.total.writeFiles);
			double_str(s3, (double)ti.total.writeTrans / ti.tickCount / 1024 * 1000 / 1024);
			ticktime_str(s4, ti.tickCount);

			len += sprintf(buf + len, IsListing() ?
				"FastCopy (%s %s %s %s %s)" :
				"FastCopy (%s %s %s %s %s %s %s)"
				, s1
				, GetLoadStr(IDS_Info_MB) //MB
				, s2
				, GetLoadStr(IDS_Info_files) //files
				, IsListing() ? s4 : s3
				, GetLoadStr(IDS_Info_MB_s) //MB/s
				, s4
				);
		}
		if (is_finish_status) {
			strcpy(buf + len, GetLoadStr(IDS_TaskTrayInfo_Finished)); //Finished
		}
	}

	curIconIndex = is_finish_status ? 0 : (curIconIndex + 1) % MAX_NORMAL_FASTCOPY_ICON;
	TaskTray(NIM_MODIFY, hMainIcon[curIconIndex], buf/*, is_finish_status*/);
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
	int64	preFiles, preTrans, doneFiles, doneTrans;
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
	char	buf[1024], s1[64], s2[64], s3[64], s4[64], s5[64], s6[64];
	int		len = 0;
	double	doneRate;
	int		remain_sec, total_sec;

	doneRatePercent = -1;

	fastCopy.GetTransInfo(&ti, is_finish_status || !isTaskTray);
	if (ti.tickCount == 0) {
		ti.tickCount++;
	}

#define FILELOG_MINSIZE	512 * 1024

	if (IsListing() && ti.listBuf->UsedSize() > 0
	|| (info.flags & FastCopy::LISTING) && ti.listBuf->UsedSize() >= FILELOG_MINSIZE) {
		::EnterCriticalSection(ti.listCs);
		if (IsListing()) SetListInfo();
		else			 SetFileLogInfo();
		::LeaveCriticalSection(ti.listCs);
	}

	if ((info.flags & FastCopy::PRE_SEARCH) && !ti.total.isPreSearch) {
		CalcInfo(&doneRate, &remain_sec, &total_sec);
		doneRatePercent = (int)(doneRate * 100);
		SetWindowTitle();
	}

	if (isTaskTray) {
		SetTaskTrayInfo(is_finish_status, doneRate, remain_sec);
		if (isTaskTray && !is_finish_status) return TRUE;
	}

	if ((info.flags & FastCopy::PRE_SEARCH) && !ti.total.isPreSearch && !is_finish_status
	&& doneRate >= 0.0001) {
		time_str(s1, remain_sec);
		len += sprintf(buf + len, "--%s %s (%d%%)--\r\n"
			, GetLoadStr(IDS_Info_Remain) //Remain
			, s1
			, doneRatePercent);
	}
	if (ti.total.isPreSearch) {
		mega_str(s1, ti.total.preTrans);
		comma_int64(s2, ti.total.preFiles);
		comma_int64(s3, ti.total.preDirs);
		ticktime_str(s4, ti.tickCount);
		double_str(s5, (double)ti.total.preFiles * 1000 / ti.tickCount);

		len += sprintf(buf + len,
			"---- %s ----\r\n"
			"%s = %s %s\r\n"
			"%s = %s (%s)\r\n"
			"%s = %s\r\n"
			"%s = %s %s"
			, GetLoadStr(IDS_Info_Estimating) //Estimating ...
			, GetLoadStr(IDS_Info_PreTrans) //PreTrans
			, s1
			, GetLoadStr(IDS_Info_MB) //MB
			, GetLoadStr(IDS_Info_PreFiles) //PreFiles
			, s2
			, s3
			, GetLoadStr(IDS_Info_PreTime) //PreTime
			, s4
			, GetLoadStr(IDS_Info_PreRate) //PreRate
			, s5
			, GetLoadStr(IDS_Info_files_s) //files/s
			);
	}
	else if (info.mode == FastCopy::DELETE_MODE) {
		mega_str(s1, ti.total.deleteTrans);
		comma_int64(s2, ti.total.deleteFiles);
		comma_int64(s3, ti.total.deleteDirs);
		ticktime_str(s4, ti.tickCount);
		double_str(s5, (double)ti.total.deleteFiles * 1000 / ti.tickCount);
		double_str(s6, (double)ti.total.writeTrans / ((double)ti.tickCount/1000)/(1024*1024));

		len += sprintf(buf + len,
			IsListing() ?
			"%s = %s %s\r\n"
			"%s = %s (%s)\r\n"
			"%s = %s\r\n" :
			(info.flags & (FastCopy::OVERWRITE_DELETE|FastCopy::OVERWRITE_DELETE_NSA)) ?
			"%s = %s %s\r\n"
			"%s = %s (%s)\r\n"
			"%s = %s\r\n"
			"%s = %s %s\r\n"
			"%s = %s %s" :
			"%s = %s %s\r\n"
			"%s = %s (%s)\r\n"
			"%s = %s\r\n"
			"%s = %s %s"
			, GetLoadStr(IDS_Info_TotalDel) //TotalDel
			, s1
			, GetLoadStr(IDS_Info_MB) //MB
			, GetLoadStr(IDS_Info_DelFiles) //DelFiles
			, s2
			, s3
			, GetLoadStr(IDS_Info_TotalTime) //TotalTime
			, s4
			, GetLoadStr(IDS_Info_FileRate) //FileRate
			, s5
			, GetLoadStr(IDS_Info_files_s) //files/s
			, GetLoadStr(IDS_Info_OverWrite) //OverWrite
			, s6
			, GetLoadStr(IDS_Info_MB_s) //MB/s
			);
	}
	else {
		if (IsListing()) {
			mega_str(s1, ti.total.writeTrans);
			comma_int64(s2, ti.total.writeFiles);
			comma_int64(s3, (info.flags & FastCopy::RESTORE_HARDLINK) ? ti.total.linkFiles :
				ti.total.writeDirs);
			comma_int64(s4, ti.total.writeDirs);

			len += sprintf(buf + len,
				(info.flags & FastCopy::RESTORE_HARDLINK) ? 
				"%s = %s %s\r\n"
				"%s = %s/%s (%s)\r\n" :
				"%s = %s %s\r\n"
				"%s = %s (%s)\r\n"
				, GetLoadStr(IDS_Info_TotalSize) //TotalSize
				, s1
				, GetLoadStr(IDS_Info_MB) //MB
				, GetLoadStr(IDS_Info_TotalFiles) //TotalFiles
				, s2
				, s3
				, s4
				);
		}
		else {
			mega_str(s1, ti.total.readTrans);
			mega_str(s2, ti.total.writeTrans);
			comma_int64(s3, ti.total.writeFiles);
			comma_int64(s4, (info.flags & FastCopy::RESTORE_HARDLINK) ? ti.total.linkFiles :
				  ti.total.writeDirs);
			comma_int64(s5, ti.total.writeDirs);

			len += sprintf(buf + len,
				(info.flags & FastCopy::RESTORE_HARDLINK) ? 
				"%s = %s %s\r\n"
				"%s = %s %s\r\n"
				"%s = %s/%s (%s)\r\n" :
				"%s = %s %s\r\n"
				"%s = %s %s\r\n"
				"%s = %s (%s)\r\n"
				, GetLoadStr(IDS_Info_TotalRead) //TotalRead
				, s1
				, GetLoadStr(IDS_Info_MB) //MB
				, GetLoadStr(IDS_Info_TotalWrite) //TotalWrite
				, s2
				, GetLoadStr(IDS_Info_MB) //MB
				, GetLoadStr(IDS_Info_TotalFiles) //TotalFiles
				, s3
				, s4
				, s5
				);
		}

		if (ti.total.skipFiles || ti.total.skipDirs) {
			mega_str(s1, ti.total.skipTrans);
			comma_int64(s2, ti.total.skipFiles);
			comma_int64(s3, ti.total.skipDirs);

			len += sprintf(buf + len,
				"%s = %s %s\r\n"
				"%s = %s (%s)\r\n"
				, GetLoadStr(IDS_Info_TotalSkip) //TotalSkip
				, s1
				, GetLoadStr(IDS_Info_MB) //MB
				, GetLoadStr(IDS_Info_SkipFiles) //SkipFiles
				, s2
				, s3
				);
		}
		if (ti.total.deleteFiles || ti.total.deleteDirs) {
			mega_str(s1, ti.total.deleteTrans);
			comma_int64(s2, ti.total.deleteFiles);
			comma_int64(s3, ti.total.deleteDirs);

			len += sprintf(buf + len,
				"%s = %s %s\r\n"
				"%s = %s (%s)\r\n"
				, GetLoadStr(IDS_Info_TotalDel) //TotalDel
				, s1
				, GetLoadStr(IDS_Info_MB) //MB
				, GetLoadStr(IDS_Info_DelFiles) //DelFiles
				, s2
				, s3
				);
		}
		ticktime_str(s1, ti.tickCount);
		double_str(s2, (double)ti.total.writeTrans / ti.tickCount / 1024 * 1000 / 1024);
		double_str(s3, (double)ti.total.writeFiles * 1000 / ti.tickCount);

		len += sprintf(buf + len, IsListing() ?
			"%s = %s\r\n" :
			"%s = %s\r\n"
			"%s = %s %s\r\n"
			"%s = %s %s"
			, GetLoadStr(IDS_Info_TotalTime) //TotalTime
			, s1
			, GetLoadStr(IDS_Info_TransRate) //TransRate
			, s2
			, GetLoadStr(IDS_Info_MB_s) //MB/s
			, GetLoadStr(IDS_Info_FileRate) //FileRate
			, s3
			, GetLoadStr(IDS_Info_files_s) //files/s
			);

		if (info.flags & FastCopy::VERIFY_FILE) {
			mega_str(s1, ti.total.verifyTrans);
			comma_int64(s2, ti.total.verifyFiles);

			len += sprintf(buf + len, 
				"\r\n"
				"%s = %s %s\r\n"
				"%s = %s"
				, GetLoadStr(IDS_Info_VerifyRead) //VerifyRead
				, s1
				, GetLoadStr(IDS_Info_MB) //MB
				, GetLoadStr(IDS_Info_VerifyFiles) //VerifyFiles
				, s2
				);
		}
	}

	if (IsListing() && is_finish_status) {
		len += sprintf(buf + len, "\r\n---- %s%s %s ----"
			, GetLoadStr(IDS_Info_Listing) //Listing
			, (info.flags & FastCopy::VERIFY_FILE) ? GetLoadStr(IDS_Info_verify) : ""
			, GetLoadStr(IDS_Info_Done) //Done
			);
	}
	curIconIndex = is_finish_status ? 0 : (curIconIndex + 1) % MAX_NORMAL_FASTCOPY_ICON;
	::SetClassLong(hWnd, GCL_HICON, (LONG)hMainIcon[curIconIndex]);//图标动起来
	SetDlgItemText(STATUS_EDIT, buf);
	if (IsListing()) {
		if (is_finish_status) {
			int offset_v = listBufOffset / sizeof(WCHAR);
			int pathedit_len = (int)SendDlgItemMessageW(PATH_EDIT, WM_GETTEXTLENGTH, 0, 0);

			comma_int64(s1, ti.total.errFiles);
			comma_int64(s2, ti.total.errDirs);

			sprintf(buf, "%s (%s : %s  %s : %s)"
				, GetLoadStr(IDS_Info_Finished) //Finished
				, GetLoadStr(IDS_Info_ErrorFiles) //ErrorFiles
				, s1
				, GetLoadStr(IDS_Info_ErrorDirs) //ErrorDirs
				, s2
				);
			SendDlgItemMessageW(PATH_EDIT, EM_SETSEL, offset_v, offset_v);
			SendDlgItemMessage(PATH_EDIT, EM_REPLACESEL, 0, (LPARAM)buf);
//			int line = SendDlgItemMessage(PATH_EDIT, EM_GETLINECOUNT, 0, 0);
//			SendDlgItemMessage(PATH_EDIT, EM_LINESCROLL, 0, line > 2 ? line -2 : 0);
		}
	}
	else if (is_finish_status) {
		comma_int64(s1, ti.total.errFiles);
		comma_int64(s2, ti.total.errDirs);

		sprintf(buf, ti.total.errFiles || ti.total.errDirs ?
			"%s (%s : %s  %s : %s)" : "%s"
			, GetLoadStr(IDS_Info_Finished) //Finished
			, GetLoadStr(IDS_Info_ErrorFiles) //ErrorFiles
			, s1
			, GetLoadStr(IDS_Info_ErrorDirs) //ErrorDirs
			, s2
			);
		SetDlgItemText(PATH_EDIT, buf);
		SetWindowTitle();
	}
	else
		SetDlgItemTextW(PATH_EDIT, ti.total.isPreSearch ? L"" : ti.curPath);

	SetDlgItemText(SAMEDRV_STATIC, info.mode == FastCopy::DELETE_MODE ? "" : ti.isSameDrv ?
		GetLoadStr(IDS_SAMEDISK) : GetLoadStr(IDS_DIFFDISK));

//	SetDlgItemText(SPEED_STATIC, buf);

	if ((info.ignoreEvent ^ ti.ignoreEvent) & FASTCOPY_ERROR_EVENT)
		CheckDlgButton(IGNORE_CHECK,
			((info.ignoreEvent = ti.ignoreEvent) & FASTCOPY_ERROR_EVENT) ? 1 : 0);

	if (isErrEditHide && ti.errBuf->UsedSize() > 0) {
		if (ti.errBuf->UsedSize() < 40) {	// abort only の場合は小さく
			normalHeight = normalHeightOrg - (normalHeight - miniHeight) / 3;
		}
		SetNormalWindow();
	}
	if (ti.errBuf->UsedSize() > errBufOffset || errBufOffset == MAX_ERR_BUF || is_finish_status) {
		if (ti.errBuf->UsedSize() > errBufOffset && errBufOffset != MAX_ERR_BUF) {
			::EnterCriticalSection(ti.errCs);
			RichEditSetText(GetDlgItem(ERR_EDIT), (WCHAR *)(ti.errBuf->Buf() + errBufOffset));
			errBufOffset = (int)ti.errBuf->UsedSize();
			::LeaveCriticalSection(ti.errCs);
		}
		comma_int64(s1, ti.total.errFiles);
		comma_int64(s2, ti.total.errDirs);

		sprintf(buf, "(%s : %s / %s : %s)"
			, GetLoadStr(IDS_Info_ErrFiles) //ErrFiles
			, s1
			, GetLoadStr(IDS_Info_ErrDirs) //ErrDirs
			, s2);
		SetDlgItemText(ERRSTATUS_STATIC, buf);
	}
	return	TRUE;
}

BOOL TMainDlg::SetWindowTitle()
{
	static WCHAR	*title;
	static WCHAR	*version;
	static WCHAR	*admin;

	if (!title) {
		title	= FASTCOPYW;
		version	= AtoW(GetVersionStr());
		admin	= AtoW(GetVerAdminStr());
	}

#define FMT_HYPHEN	L" -%s"
#define FMT_PAREN	L" [%s]"
#define FMT_PERCENT	L"(%d%%) "

	WCHAR	buf[1024];
	WCHAR	*p = buf;

	if ((info.flags & FastCopy::PRE_SEARCH) && !ti.total.isPreSearch && doneRatePercent >= 0
	&& fastCopy.IsStarting()) {
		p += swprintf(p, FMT_PERCENT, doneRatePercent);
	}

	WCHAR	_job[MAX_PATH] = L"";
	WCHAR	*job = _job;
	WCHAR	*fin = finActIdx > 0 ? cfg.finActArray[finActIdx]->title : L"";

	GetDlgItemTextW(JOBTITLE_STATIC, job, MAX_PATH);

	p += wcscpyz(p, title);

	if (job[0]) {
		p += swprintf(p, FMT_PAREN, job);
	}

	if (fin[0]) {
		p += swprintf(p, FMT_HYPHEN, fin);
	}

	if (!job[0] && !fin[0]) {
		*p++ = ' ';
		p += wcscpyz(p, version);
	}
	p += wcscpyz(p, admin);

	return	SetWindowTextW(buf);
}

void TMainDlg::SetItemEnable(BOOL is_delete)
{
	::EnableWindow(GetDlgItem(DST_FILE_BUTTON), !is_delete);
	::EnableWindow(GetDlgItem(DST_COMBO), !is_delete);
	::ShowWindow(GetDlgItem(ESTIMATE_CHECK), is_delete ? SW_HIDE : SW_SHOW);
	::ShowWindow(GetDlgItem(VERIFY_CHECK), is_delete ? SW_HIDE : SW_SHOW);

	::EnableWindow(GetDlgItem(ACL_CHECK), !is_delete);
	::EnableWindow(GetDlgItem(STREAM_CHECK), !is_delete);
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

//	if (!IsWinVista() || ::IsUserAnAdmin()) {
//		::DeleteMenu(hMenu, ADMIN_MENUITEM, MF_BYCOMMAND);
//	}

// トップレベルメニューアイテムにアイコンを付与するのは無理かも？
//	HICON hIcon = ::LoadImage(GetModuleHandle("user32.dll"), 106, IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
//	::SetMenuItemBitmaps(hMenu, 0, MF_BYPOSITION, LoadBitmap(NULL, (char *)32754), LoadBitmap(NULL, (char *)32754));

	HMENU	hSubMenu = ::GetSubMenu(hMenu, 0);

	::EnableMenuItem(hSubMenu, FILELOG_MENUITEM,
		MF_BYCOMMAND|((*lastFileLog) ? MF_ENABLED : MF_GRAYED));

	hSubMenu = ::GetSubMenu(hMenu, 1);
	while (::GetMenuItemCount(hSubMenu) > 2 && ::DeleteMenu(hSubMenu, 2, MF_BYPOSITION))
		;

	GetDlgItemTextW(JOBTITLE_STATIC, buf, MAX_PATH_EX);

	for (i=0; i < cfg.jobMax; i++) {
		::InsertMenuW(hSubMenu, i + 2, MF_STRING|MF_BYPOSITION, JOBOBJ_MENUITEM_START + i,
			cfg.jobArray[i]->title);
		::CheckMenuItem(hSubMenu, i + 2,
			MF_BYPOSITION|(wcsicmp(buf, cfg.jobArray[i]->title) ? MF_UNCHECKED : MF_CHECKED));
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

	swprintf(buf, GetLoadStrW(IDS_FINACTMENU),
		finActIdx >= 0 ? cfg.finActArray[finActIdx]->title : L"");
	::ModifyMenuW(hSubMenu, 7, MF_BYPOSITION|MF_STRING, 0, buf);

	hSubMenu = ::GetSubMenu(hSubMenu, 7);
	while (::GetMenuItemCount(hSubMenu) > 2 && ::DeleteMenu(hSubMenu, 0, MF_BYPOSITION))
		;
	for (i=0; i < cfg.finActMax; i++) {
		::InsertMenuW(hSubMenu, i, MF_STRING|MF_BYPOSITION, FINACT_MENUITEM_START + i,
			cfg.finActArray[i]->title);
		::CheckMenuItem(hSubMenu, i, MF_BYPOSITION|(i == finActIdx ? MF_CHECKED : MF_UNCHECKED));
	}

	if (!fastCopy.IsStarting() && !isDelay) {
		SetDlgItemText(SAMEDRV_STATIC, diskMode == 0 ? "" : diskMode == 1 ?
			GetLoadStr(IDS_FIX_SAMEDISK) : GetLoadStr(IDS_FIX_DIFFDISK));
	}
	SetWindowTitle();
}

BOOL TMainDlg::GetRunasInfo(WCHAR **user_dir, WCHAR **virtual_dir)
{
	WCHAR	**argv		= (WCHAR **)orgArgv;
	int		len;

	while (*argv && **argv == '/') {
		if (!wcsicmpEx(*argv, RUNAS_STR, &len) == 0) {
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

void TMainDlg::SetJob(int idx)
{
	WCHAR	wbuf[MAX_PATH] = L"";
	WCHAR	*title = wbuf;
	Job		*job = cfg.jobArray[idx];

	idx = CmdNameToComboIndex(job->cmd);

	if (idx == -1)	return;

	if (GetDlgItemTextW(JOBTITLE_STATIC, title, MAX_PATH) > 0 && wcscmp(job->title, title) == 0) {
		SetDlgItemTextW(JOBTITLE_STATIC, L"");
		UpdateMenu();
		return;
	}

	SetDlgItemTextW(JOBTITLE_STATIC, job->title);
	SendDlgItemMessage(MODE_COMBO, CB_SETCURSEL, idx, 0);
	SetDlgItemInt(BUFSIZE_EDIT, job->bufSize);
	CheckDlgButton(ESTIMATE_CHECK, job->estimateMode);
	CheckDlgButton(VERIFY_CHECK, job->enableVerify);
	CheckDlgButton(IGNORE_CHECK, job->ignoreErr);
	CheckDlgButton(OWDEL_CHECK, job->enableOwdel);
	CheckDlgButton(ACL_CHECK, job->enableAcl);
	CheckDlgButton(STREAM_CHECK, job->enableStream);
	CheckDlgButton(FILTER_CHECK, job->isFilter);
	SetDlgItemTextW(SRC_COMBO, job->src);
	SetDlgItemTextW(DST_COMBO, job->dst);
	SetDlgItemTextW(INCLUDE_COMBO, job->includeFilter);
	SetDlgItemTextW(EXCLUDE_COMBO, job->excludeFilter);
	SetDlgItemTextW(FROMDATE_COMBO, job->fromDateFilter);
	SetDlgItemTextW(TODATE_COMBO, job->toDateFilter);
	SetDlgItemTextW(MINSIZE_COMBO, job->minSizeFilter);
	SetDlgItemTextW(MAXSIZE_COMBO, job->maxSizeFilter);
	SetItemEnable(copyInfo[idx].mode == FastCopy::DELETE_MODE);
	SetDlgItemText(PATH_EDIT, "");
	SetDlgItemText(ERR_EDIT, "");
	diskMode = job->diskMode;
	UpdateMenu();
	SetMiniWindow();

	if (job->isFilter &&
		(job->fromDateFilter[0] || job->toDateFilter[0] ||
		 job->minSizeFilter[0] || job->maxSizeFilter[0])) {
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

