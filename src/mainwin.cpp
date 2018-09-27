static char *mainwin_id = 
	"@(#)Copyright (C) 2004-2018 H.Shirouzu		mainwin.cpp	ver3.50";
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Create					: 2004-09-15(Wed)
	Update					: 2018-05-28(Mon)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	Modify					: Mapaler 2015-09-29
	======================================================================== */

#include "mainwin.h"
#include <time.h>
#include "shellext/shelldef.h"

#pragma warning (push)
#pragma warning (disable : 4091) // dbghelp.h(1540): warning C4091: 'typedef ' ignored...
#include <dbghelp.h>
#pragma warning (pop)

using namespace std;

#define FASTCOPY_TIMER_TICK 250 //基本信息更新时间，帧速

/*=========================================================================
  クラス ： TFastCopyApp
  概  要 ： アプリケーションクラス
  説  明 ： 
  注  意 ： 
=========================================================================*/
TFastCopyApp::TFastCopyApp(HINSTANCE _hI, LPSTR _cmdLine, int _nCmdShow)
	: TApp(_hI, _cmdLine, _nCmdShow)
{
	::SetDllDirectory("");

	if (!TSetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32)) {
		TLoadLibraryExW(L"iertutil.dll", TLT_SYSDIR);
		TLoadLibraryExW(L"cryptbase.dll", TLT_SYSDIR);
		TLoadLibraryExW(L"urlmon.dll", TLT_SYSDIR);
		TLoadLibraryExW(L"cscapi.dll", TLT_SYSDIR);
	}

	TLoadLibraryExW(L"riched20.dll", TLT_SYSDIR);
	TLoadLibraryExW(L"msftedit.dll", TLT_SYSDIR);

	DBG("MSC_VER=%d FileStat=%zd %d\n", _MSC_VER, offsetof(FileStat, cFileName));

//	LoadLibrary("SHELL32.DLL");
//	LoadLibrary("COMCTL32.DLL");
//	LoadLibrary("COMDLG32.dll");
//	TLibInit_AdvAPI32();
//	extern void tapi32_test();
//	tapi32_test();
//	extern void cond_test();
//	cond_test();
//	extern void makehash_test();
//	makehash_test();
//	extern void hash_speed();
//	hash_speed();
//	extern void hash_speed_mt2();
//	hash_speed_mt2();
}

TFastCopyApp::~TFastCopyApp()
{
}

void TFastCopyApp::InitWindow(void)
{
	TRegisterClass(FASTCOPY_CLASS);

	auto	dlg = new TMainDlg();
	mainWnd = dlg;
	dlg->Create();
}

extern HWND TransMsgHelp(MSG *msg);

BOOL TFastCopyApp::PreProcMsg(MSG *msg)
{
	if (msg->message == WM_KEYDOWN || msg->message == WM_KEYUP || msg->message == WM_ACTIVATEAPP) {
		mainWnd->PostMessage(WM_FASTCOPY_KEY, 0, 0);
	}

	if (TransMsgHelp(msg)) {
		return	TRUE;
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
TMainDlg::TMainDlg() : TDlg(MAIN_DIALOG),
	aboutDlg(this), setupDlg(&cfg, this), jobDlg(&cfg, this), finActDlg(&cfg, this),
	srcEdit(MAX_SRCEDITCR, this), pathEdit(this), errEdit(this), bufEdit(this), statEdit(this),
	speedSlider(this), speedStatic(this), listBtn(this), pauseOkBtn(this), pauseListBtn(this)
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
		TApp::Exit(-1);
		return;
	}
	SetUserAgent();

	if (cfg.lcid > 0) {
		TSetDefaultLCID(cfg.lcid);
	}
	cfg.PostReadIni();

	shellMode = SHELL_NONE;
	isErrLog = cfg.isErrLog;
	isUtf8Log = cfg.isUtf8Log;
	fileLogMode = (FileLogMode)cfg.fileLogMode;
	isListLog = FALSE;
	isReparse = cfg.isReparse;
	isLinkDest = cfg.isLinkDest;
	isReCreate = cfg.isReCreate;
	isExtendFilter = cfg.isExtendFilter;
	maxLinkHash = cfg.maxLinkHash;
	maxTempRunNum = 0;
	forceStart = cfg.forceStart;
	finishNotify = cfg.finishNotify;
	dlsvtMode = cfg.dlsvtMode;
	isPause = FALSE;

	showState = SS_HIDE;
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

	captureMode = FALSE;
	lastYPos = 0;
	dividYPos = 0;

	statusFont = NULL;
	memset(&statusLogFont, 0, sizeof(statusLogFont));

	curPriority = 0;

	if (IsWinVista()) {	// WinServer2012+ATOM で発生する問題への対処（暫定）
		::SetPriorityClass(::GetCurrentProcess(), PROCESS_MODE_BACKGROUND_BEGIN);
		::SetPriorityClass(::GetCurrentProcess(), PROCESS_MODE_BACKGROUND_END);
	}

	switch (cfg.priority) {
	case 1:
		SetPriority(IDLE_PRIORITY_CLASS);
		break;

	case 2:
		SetPriority(NORMAL_PRIORITY_CLASS);
		break;

	case 3:
		SetPriority(HIGH_PRIORITY_CLASS);
		break;

	default:
		SetPriority(IDLE_PRIORITY_CLASS);
		SetPriority(NORMAL_PRIORITY_CLASS);
		break;
	}

	autoCloseLevel = NO_CLOSE;
	curIconIdx = 0;
	pathLogBuf = NULL;
	isDelay = FALSE;
	isAbort = FALSE;
	endTick = 0;
	hErrLog = INVALID_HANDLE_VALUE;
	hErrLogMutex = ::CreateMutex(NULL, FALSE, FASTCOPYLOG_MUTEX);

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
	Wstr	wstr;

	// backup editbox
	if (mode == SETHIST_LIST || mode == SETHIST_CLEAR) {
		auto len = ::GetWindowTextLengthW(GetDlgItem(item));
		wstr.Init(len + 1);
		if (GetDlgItemTextW(item, wstr.Buf(), len + 1) != len)
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
		SetDlgItemTextW(item, wstr.s());
	}
}

BOOL TMainDlg::SetMiniWindow(void)
{
	GetWindowRect(&rect);
	int cur_height = rect.cy();
	int new_height = cur_height - (isErrEditHide ? 0 : normalHeight - miniHeight);
	int min_height = miniHeight - (isExtendFilter ? 0 : filterHeight);

	isErrEditHide = TRUE;
	if (new_height < min_height) new_height = min_height;

	MoveWindow(rect.x(), rect.y(), rect.cx(), new_height, IsWindowVisible());
	return	TRUE;
}

BOOL TMainDlg::SetNormalWindow()
{
	if (IsWindowVisible()) {
		GetWindowRect(&rect);
		int diff_size = normalHeight - miniHeight;
		int height = rect.cy() + (isErrEditHide ? diff_size : 0);
		isErrEditHide = FALSE;
		MoveWindow(rect.x(), rect.y(), rect.cx(), height, TRUE);
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
		sz.cx = orgRect.cx() + cfg.winsize.cx;
		sz.cy = cfg.winsize.cy + (isErrEditHide ? miniHeight : normalHeight);
	}
	TRect	src(0, 0, ::GetSystemMetrics(SM_CXFULLSCREEN), ::GetSystemMetrics(SM_CYFULLSCREEN));

	if (isFixPos) {
		isFixPos = FALSE;
		if (HMONITOR hMon = ::MonitorFromPoint(cfg.winpos, MONITOR_DEFAULTTONEAREST)) {
			MONITORINFO	mi = { sizeof(mi) };

			if (::GetMonitorInfoW(hMon, &mi)) {
				if (PtInRect(&mi.rcMonitor, cfg.winpos)) {
					pt = cfg.winpos;
					isFixPos = TRUE;
				}
			}
		}
	}
	if (!isFixPos) {
		if (HMONITOR hMon = ::MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST)) {
			MONITORINFO	mi = { sizeof(mi) };

			if (::GetMonitorInfoW(hMon, &mi)) {
				src = mi.rcMonitor;
			}
		}
		pt.x = src.x() + (src.cx() - rect.cx()) / 2;
		pt.y = src.y() + (src.cy() - rect.cy()) / 2;
	}

	return	SetWindowPos(NULL, pt.x, pt.y, sz.cx, sz.cy,
			(isFixSize ? 0 : SWP_NOSIZE)|SWP_NOZORDER|SWP_NOACTIVATE);
}

BOOL TMainDlg::SetupWindow()
{
	static BOOL once = FALSE;

	if (once) {
		return TRUE;
	}

	once = TRUE;
	EnableWindow(TRUE);

	if (cfg.isTopLevel)
		SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOMOVE);

	MoveCenter();
	SetItemEnable(GetCopyMode());

	SetMiniWindow();
	return	TRUE;
}

BOOL TMainDlg::IsForeground()
{
	HWND hForeWnd = GetForegroundWindow();

	return	hForeWnd && hWnd && (hForeWnd == hWnd || ::IsChild(hWnd, hForeWnd)) ? TRUE : FALSE;
}

void TMainDlg::StatusEditSetup(BOOL reset)
{
	static bool once = false;

	if (!reset) {
		if (statusFont || once) return;
	}
	if (statusFont) {
		::DeleteObject(statusFont);
		statusFont = NULL;
	}
	once = true;

	LOGFONTW	&lf = statusLogFont;
	memset(&lf, 0, sizeof(lf));
	int			font_size = 0;
	HDC			hDc			= ::GetDC(hWnd);
	int			logPixY		= ::GetDeviceCaps(hDc, LOGPIXELSY);
	DWORD		font_id		= IDS_STATUS_FONT;
	DWORD		fontsize_id	= IDS_STATUS_FONTSIZE;

	if (logPixY != 96) {	// enable display font scaling
		font_id		= IDS_STATUS_ALTFONT;
		fontsize_id	= IDS_STATUS_ALTFONTSIZE;
	}
	if (*cfg.statusFont) {
		wcscpy(lf.lfFaceName, cfg.statusFont);
	}
	else {
		if (WCHAR *s = LoadStrW(font_id)) {
			wcscpy(lf.lfFaceName, s);
		}
		if (!*lf.lfFaceName) return;
	}
	if ((font_size = cfg.statusFontSize) <= 0) {
		if (char *s = LoadStr(fontsize_id)) {
			font_size = atoi(s);
		}
		if (font_size <= 0) return;
	}

	lf.lfCharSet = DEFAULT_CHARSET;
	POINT	pt[2]={};
	pt[0].y = (int)((int64)logPixY * font_size / 720);
	::DPtoLP(hDc, pt,  2);
	lf.lfHeight = -abs(pt[0].y - pt[1].y);
	::ReleaseDC(hWnd, hDc);

	statusFont = ::CreateFontIndirectW(&lf);
	statEdit.SendMessage(WM_SETFONT, (WPARAM)statusFont, 0);

	SetStatMargin();

	InvalidateRect(0, 0);
}

void TMainDlg::ChooseFont()
{
	if (!*cfg.statusFont) {
		StatusEditSetup();
	}

	HDC		hDc = ::GetDC(hWnd);
	int		logPixY = ::GetDeviceCaps(hDc, LOGPIXELSY);

	LOGFONTW	tmpFont = statusLogFont;
	CHOOSEFONTW	cf = { sizeof(cf) };
	cf.hwndOwner	= hWnd;
	cf.lpLogFont	= &tmpFont;
	cf.Flags		= CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_SHOWHELP
		| CF_LIMITSIZE | CF_NOVERTFONTS;
	cf.nFontType	= SCREEN_FONTTYPE;
	cf.nSizeMax		= 15;

	if (::ChooseFontW(&cf)) {
		POINT	pt[2] = {};
		pt[0].y = abs(tmpFont.lfHeight);
		::LPtoDP(hDc, pt, 2);
		cfg.statusFontSize = int(ceil((double)abs(pt[0].y - pt[1].y) * 720 / logPixY));
		wcscpy(cfg.statusFont, tmpFont.lfFaceName);

		StatusEditSetup(TRUE);
		SetInfo(TRUE);
		cfg.WriteIni();
	}
	SetStatMargin();

	::ReleaseDC(hWnd, hDc);
}

BOOL TMainDlg::EvCreate(LPARAM lParam)
{
	BOOL is_elevated_admin = IsWinVista() && ::IsUserAnAdmin() && TIsEnableUAC();

	if (is_elevated_admin || isNoUI) {
		SetVersionStr(is_elevated_admin, isNoUI);
	}
	::DragAcceptFiles(hWnd, TRUE);

	char	title[100];
	sprintf(title, "%s %s%s", FASTCOPY_TITLE, GetVersionStr(), GetVerAdminStr());

	WCHAR	user_log[MAX_PATH];
	WCHAR	dump_log[MAX_PATH];
	MakePathW(user_log, cfg.userDir, L"fastcopy_exception.log");
	MakePathW(dump_log, cfg.userDir, L"fastcopy_exception.dmp");

	InstallExceptionFilter(title, LoadStr(IDS_EXCEPTIONLOG), WtoAs(user_log), WtoAs(dump_log),
		MiniDumpWithPrivateReadWriteMemory | 
		MiniDumpWithDataSegs | 
		MiniDumpWithHandleData |
		MiniDumpWithFullMemoryInfo | 
		MiniDumpWithThreadInfo | 
		MiniDumpWithUnloadedModules);

	if ((cfg.debugMainFlags & GSHACK_SKIP) == 0) {
		TGsFailureHack();
	}

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
				::GetMenuString(hMenu, 3, buf, sizeof(buf), MF_BYPOSITION|MF_STRING);
				if ((p = strchr(buf, '('))) {
					*p = 0;
					::ModifyMenu(hMenu, 3, MF_BYPOSITION|MF_STRING, 0, buf);
				}
				::InsertMenuW(hMenu, 4, MF_BYPOSITION|MF_STRING|MF_HELP, ADMIN_MENUITEM,
					LoadStrW(IDS_ELEVATE));
			}
		}
	}

	srcEdit.AttachWnd(GetDlgItem(SRC_EDIT));
	statEdit.AttachWnd(GetDlgItem(STATUS_EDIT));
	pathEdit.AttachWnd(GetDlgItem(PATH_EDIT));
	errEdit.AttachWnd(GetDlgItem(ERR_EDIT));
	bufEdit.AttachWnd(GetDlgItem(BUFSIZE_EDIT));
	bufEdit.CreateTipWnd(LoadStrW(IDS_BUFSIZETIP));

	SendDlgItemMessage(IDOK, WM_GETFONT, 0, 0L);

	int		i;
	for (i=0; i < MAX_FASTCOPY_ICON; i++) {
		hMainIcon[i] = ::LoadIcon(TApp::GetInstance(), (LPCSTR)(LONG_PTR)(FASTCOPY_ICON + i));
	}
	::SetClassLong(hWnd, GCL_HICON, LONG_RDC(hMainIcon[FCNORM_ICON_IDX]));
	SetIcon();

	hAccel = ::LoadAccelerators(TApp::GetInstance(), (LPCSTR)IDR_ACCEL);
	SetSize();

	taskbarList = NULL;

	if (IsWin7()) {
		SetWinAppId(hWnd, L"FastCopy");
		::CoCreateInstance(CLSID_TaskbarList, 0, CLSCTX_INPROC_SERVER, IID_ITaskbarList,
			(void **)&taskbarList);
		if (taskbarList) {
			taskbarList->SetProgressValue(hWnd, 0, 100);
			taskbarList->SetProgressState(hWnd, TBPF_NOPROGRESS);
		}
	}

	TaskBarCreateMsg = ::RegisterWindowMessage("TaskbarCreated");

	// メッセージセット
	SetDlgItemText(STATUS_EDIT, LoadStr(IDS_BEHAVIOR));
	SetDlgItemInt(BUFSIZE_EDIT, cfg.bufSize);

// 123456789012345678901234567
//	SetDlgItemText(STATUS_EDIT, "1234567890123456789012345678901234567890\r\n2\r\n3\r\n4\r\n5\r\n6\r\n7\r\n8\r\n9\r\n10\r\n11\r\n12");

	SetCopyModeList();
	UpdateMenu();

	CheckDlgButton(IGNORE_CHECK, cfg.ignoreErr);
	CheckDlgButton(ESTIMATE_CHECK, cfg.estimateMode);
	CheckDlgButton(VERIFY_CHECK, cfg.enableVerify);
	CheckDlgButton(OWDEL_CHECK, cfg.enableOwdel);
	CheckDlgButton(ACL_CHECK, cfg.enableAcl);
	CheckDlgButton(STREAM_CHECK, cfg.enableStream);

	SendDlgItemMessage(PATH_EDIT,   EM_SETWORDBREAKPROC, 0, (LPARAM)EditWordBreakProcW);
	SendDlgItemMessage(ERR_EDIT,    EM_SETWORDBREAKPROC, 0, (LPARAM)EditWordBreakProcW);

	SendDlgItemMessage(PATH_EDIT,   EM_SETTARGETDEVICE, 0, 0);		// 折り返し
	SendDlgItemMessage(ERR_EDIT,    EM_SETTARGETDEVICE, 0, 0);		// 折り返し

	SendDlgItemMessage(PATH_EDIT,   EM_LIMITTEXT, 0, 0);
	SendDlgItemMessage(ERR_EDIT,    EM_LIMITTEXT, 0, 0);

	// スライダコントロール
	speedSlider.AttachWnd(GetDlgItem(SPEED_SLIDER));
	speedSlider.SendMessage(TBM_SETRANGE, FALSE, MAKELONG(SPEED_SUSPEND, SPEED_FULL));
	speedSlider.CreateTipWnd(LoadStrW(IDS_SPEEDTIP));
	speedStatic.AttachWnd(GetDlgItem(SPEED_STATIC));
	speedStatic.CreateTipWnd(LoadStrW(IDS_SPEEDTIP));
	SetSpeedLevelLabel(this, speedLevel = cfg.speedLevel);

	listBtn.AttachWnd(GetDlgItem(LIST_BUTTON));
	listBtn.CreateTipWnd(LoadStrW(IDS_LISTTIP));

	// Pause/Resume
	hPauseIcon = ::LoadIcon(TApp::hInst(), (LPCSTR)PAUSE_ICON);
	hPlayIcon = ::LoadIcon(TApp::hInst(), (LPCSTR)PLAY_ICON);

	pauseOkBtn.AttachWnd(GetDlgItem(PAUSEOK_BTN));
	pauseOkBtn.CreateTipWnd(LoadStrW(IDS_PAUSE));
	pauseOkBtn.SendMessage(BM_SETIMAGE, IMAGE_ICON, (LPARAM)hPauseIcon);

	pauseListBtn.AttachWnd(GetDlgItem(PAUSELIST_BTN));
	pauseListBtn.CreateTipWnd(LoadStrW(IDS_PAUSE));
	pauseListBtn.SendMessage(BM_SETIMAGE, IMAGE_ICON, (LPARAM)hPauseIcon);

	// Source hist
	SendDlgItemMessage(HIST_BTN, BM_SETIMAGE, IMAGE_ICON,
		(LPARAM)::LoadIcon(TApp::hInst(), (LPCSTR)HIST_ICON));

#ifdef USE_LISTVIEW
	// リストビュー関連
#endif

	// 履歴セット
	SetPathHistory(SETHIST_EDIT);
//	SetFilterHistory(SETHIST_LIST);
	if (cfg.maxHistory > 0) {
		SetHistPath(0);
	}

	RECT	path_rect, err_rect;
	GetWindowRect(&rect);
	::GetWindowRect(GetDlgItem(PATH_EDIT), &path_rect);
	::GetWindowRect(GetDlgItem(ERR_EDIT), &err_rect);
	SendDlgItemMessage(PATH_EDIT, EM_SETBKGNDCOLOR, 0, ::GetSysColor(COLOR_3DFACE));
	SendDlgItemMessage(ERR_EDIT, EM_SETBKGNDCOLOR, 0, ::GetSysColor(COLOR_3DFACE));
	normalHeight	= rect.cy();
	miniHeight		= normalHeight - (err_rect.bottom - path_rect.bottom);
	normalHeightOrg	= normalHeight;

	RECT	exc_rect, date_rect;
	::GetWindowRect(GetDlgItem(EXCLUDE_COMBO), &exc_rect);
	::GetWindowRect(GetDlgItem(FROMDATE_COMBO), &date_rect);
	filterHeight = date_rect.bottom - exc_rect.bottom;

	SetExtendFilter();

	if (*cfg.statusFont) {
		StatusEditSetup();
	}
	SetStatMargin();

	// command line mode
	if (orgArgc > 1) {
		// isRunAsParent の場合は、Child側からの CLOSE を待つため、自主終了しない
		if (!CommandLineExecW(orgArgc, orgArgv) &&
			(shellMode == SHELL_NONE || autoCloseLevel >= NOERR_CLOSE) && !isRunAsParent) {
			resultStatus = FALSE;
			if (IsForeground() && (::GetKeyState(VK_SHIFT) & 0x8000)) {
				autoCloseLevel = NO_CLOSE;
			}
			else {
				PostMessage(WM_CLOSE, 0, 0);
			}
		}
		if (cfg.taskbarMode) {
			EnableWindow(TRUE);
		}
	}
	else {
		Show();
		/* //去除自动检查更新
		if (cfg.updCheck & 0x1) {	// 最新版確認
			time_t	now = time(NULL);

			if (cfg.lastUpdCheck + (24 * 3600) < now || cfg.lastUpdCheck > now) {	// 1日以上経過
				cfg.lastUpdCheck = now;
				cfg.WriteIni();
				UpdateCheck(TRUE);
			}
		}
		*/
	}
	if (isInstaller) {
		TaskTrayTemp(TRUE);
		auto	mode = GetTrayIconState();
		TaskTrayTemp(FALSE);
		isInstaller = FALSE;

		if (mode == TIS_HIDE && !isNoUI && !cfg.taskbarMode) {
			PostMessage(WM_FASTCOPY_TRAYSETUP, 0, 0);
		}
	}

/*
	OpenDebugConsole();
						// EN     JP     DE     LUX     PRC      HK      TW      SG
	LCID	lcids[] = { 0x409, 0x411, 0x407, 0x1007, 0x0804, 0x0C04, 0x0404, 0x1004 };
	for (auto id: lcids) {
		TSetDefaultLCID(id);
		DebugW(L"id(%x): %s\n", id, LoadStrW(IDS_MKDIR));
	}
*/

	return	TRUE;
}

BOOL TMainDlg::EvDestroy(void)
{
	if (auto *app = TApp::GetApp()) {
		app->SetResult(resultStatus ? 0 : -1);
	}

	return	TRUE;
}

BOOL TMainDlg::EvNcDestroy(void)
{
	TaskTray(NIM_DELETE);
	UnInitShowHelp();

	return	TRUE;
}

BOOL TMainDlg::CancelCopy()
{
	::KillTimer(hWnd, FASTCOPY_TIMER);

	if (!isDelay) fastCopy.Suspend();

	int	ret = TRUE;

	if (isNoUI) {
		WriteErrLogNoUI("CancelCopy (aborted by system or etc.)");
	}
	else if (!isDelay) {
		ret = (TMsgBox(this).Exec(LoadStr(IsListing() ? IDS_LISTCONFIRM :
					info.mode == FastCopy::DELETE_MODE ? IDS_DELSTOPCONFIRM : IDS_STOPCONFIRM),
					FASTCOPY, MB_OKCANCEL) == IDOK) ? TRUE : FALSE;
	}

	if (isDelay) {
		if (ret == IDOK) {
			isDelay = FALSE;
			EndCopy();
		}
		else {
			SetTimer(FASTCOPY_TIMER, FASTCOPY_TIMER_TICK*2);
		}
	}
	else {
		fastCopy.Resume();

		if (ret == IDOK) {
			isAbort = TRUE;
			fastCopy.Aborting();
		}
		else {
			SetTimer(FASTCOPY_TIMER, FASTCOPY_TIMER_TICK);
		}
	}

	return	ret;
}

BOOL TMainDlg::SwapTargetCore(const WCHAR *s, const WCHAR *d, WCHAR *out_s, WCHAR *out_d)
{
	WCHAR	*src_fname = NULL;
	WCHAR	*dst_fname = NULL;
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
			|| (attr & FILE_ATTRIBUTE_DIRECTORY) && (!cfg.isReparse || !IsReparse(attr));

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
	srcArray.GetMultiPath(out_s, MAX_WPATH, CRLF, NULW, TRUE);
	return	TRUE;
}

BOOL TMainDlg::SwapTarget(BOOL check_only)
{
	DWORD	src_len = ::GetWindowTextLengthW(GetDlgItem(SRC_EDIT));
	DWORD	dst_len = ::GetWindowTextLengthW(GetDlgItem(DST_COMBO));

	if (src_len == 0 && dst_len == 0 || max(src_len, dst_len) >= MAX_WPATH) return FALSE;

	auto		_src = make_unique<WCHAR[]>(MAX_WPATH);
	auto		src = _src.get();
	auto		_dst = make_unique<WCHAR[]>(MAX_WPATH);
	auto		dst = _dst.get();
	PathArray	srcArray;
	PathArray	dstArray;
	BOOL		ret = FALSE;

	if (src && dst && srcEdit.GetWindowTextW(src, src_len+1) == src_len
		&& GetDlgItemTextW(DST_COMBO, dst, dst_len+1) == dst_len) {
		if (srcArray.RegisterMultiPath(src, CRLF) <= 1 && dstArray.RegisterPath(dst) <= 1
			&& !(srcArray.Num() == 0 && dstArray.Num() == 0)) {
			ret = TRUE;
			if (!check_only) {
				if (srcArray.Num() == 0) {
					dstArray.GetMultiPath(src, MAX_WPATH, CRLF, NULW, TRUE);
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
					srcEdit.SetWindowTextW(src);
					SetDlgItemTextW(DST_COMBO, dst);
				}
			}
		}
	}

	return	ret;
}

void TMainDlg::PostSetup()
{
	SetCopyModeList();
	isErrLog = cfg.isErrLog;
	isUtf8Log = cfg.isUtf8Log;
	fileLogMode = (FileLogMode)cfg.fileLogMode;
	isReparse = cfg.isReparse;
	skipEmptyDir = cfg.skipEmptyDir;
	forceStart = cfg.forceStart;
	dlsvtMode = cfg.dlsvtMode;
	isExtendFilter = cfg.isExtendFilter;
	finishNotify = cfg.finishNotify;

	SetDlgItemInt(BUFSIZE_EDIT, cfg.bufSize);

	SetExtendFilter();
}

BOOL TMainDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID) {
	case IDOK: case LIST_BUTTON:
	//	if (fastCopy.IsStarting()) {
	//		*(int *)0 = 0;
	//	}
		if (!fastCopy.IsStarting() && !isDelay) {
			if ((::GetTick() - endTick) > 1000)
				ExecCopy(wID == LIST_BUTTON ? LISTING_EXEC : NORMAL_EXEC);
		}
		else if (CancelCopy()) {
			autoCloseLevel = NO_CLOSE;
		}

//		extern void bo_test(void);
//		bo_test();
		return	TRUE;

	case PAUSEOK_BTN:
	case PAUSELIST_BTN:
		isPause = !isPause;
		UpdateSpeedLevel();
		if (auto *btn = (wID == PAUSEOK_BTN) ? &pauseOkBtn : &pauseListBtn) {
			btn->SendMessage(BM_SETIMAGE, IMAGE_ICON, (LPARAM)(isPause ? hPlayIcon : hPauseIcon));
			btn->SetTipTextW(LoadStrW(isPause ? IDS_RESUME : IDS_PAUSE));
		}
		return	TRUE;

	case BUFSIZE_EDIT:
		setupDlg.SetSheetIdx(IO_SHEET);
		setupDlg.Exec();
		return	TRUE;

	//case SRC_COMBO:
	case DST_COMBO:
		if (wNotifyCode == CBN_DROPDOWN) {
			SetPathHistory(SETHIST_LIST, wID);
		}
		else if (wNotifyCode == CBN_CLOSEUP) {
			PostMessage(WM_FASTCOPY_PATHHISTCLEAR, wID, 0);
		}
		return	TRUE;

	case SRC_FILE_BUTTON:
		if (BrowseDirDlgW(this, SRC_EDIT, LoadStrW(IDS_SRC_SELECT),
				BRDIR_MULTIPATH|BRDIR_CTRLADD|BRDIR_FILESELECT|BRDIR_TAILCR
				|BRDIR_BACKSLASH|(cfg.dirSel ? 0 : BRDIR_INITFILESEL))) {
			srcEdit.Fit(TRUE);
		}
		return	TRUE;

	case DST_FILE_BUTTON:
		BrowseDirDlgW(this, DST_COMBO, LoadStrW(IDS_DST_SELECT), BRDIR_BACKSLASH);
		return	TRUE;

	case HIST_BTN:
		ShowHistMenu();
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
					if (TMsgBox(this).Exec(LoadStr(IDS_CONFIRMFORCEEND),
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
		fastCopy.TakeExclusivePriv(1);
		ExecCopyCore();
		return	TRUE;

	case TOPLEVEL_MENUITEM:
		cfg.isTopLevel = !cfg.isTopLevel;
		SetWindowPos(cfg.isTopLevel ? HWND_TOPMOST :
			HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOMOVE);
		cfg.WriteIni();
		break;

	case MODE_COMBO:
		if (wNotifyCode == CBN_SELCHANGE) {
			if (GetCopyMode() == FastCopy::DELETE_MODE && isNetPlaceSrc) {
				srcEdit.SetWindowTextW(L"");
				isNetPlaceSrc = FALSE;
			}
			if (GetCopyMode() == FastCopy::TEST_MODE) {
				srcEdit.SetWindowTextW(LoadStrW(IDS_TESTSAMPLE));
			}
			else if (cfg.testMode) {
				WCHAR	w[MAX_PATH] = L"";
				if (srcEdit.GetWindowTextW(w, wsizeof(w)) > 0 && !wcsnicmp(w, L"w=", 2)) {
					srcEdit.SetWindowTextW(L"");
				}
			}

			SetItemEnable(GetCopyMode());
		}
		return	TRUE;

	case FILTER_CHECK:
		ReflectFilterCheck();
		return	TRUE;

	case FILTERHELP_BUTTON:
		ShowHelpW(0, cfg.execDir, LoadStrW(IDS_FASTCOPYHELP), L"#filter");
		return	TRUE;

	case OPENDIR_MENUITEM:
		{
			WCHAR	dir[MAX_PATH];
			MakePathW(dir, cfg.userDir, L"");	// 末尾に \\ がないと exeが開く環境あり…
			::ShellExecuteW(NULL, NULL, dir, 0, 0, SW_SHOW);
		}
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
			cfg.winsize.cx = rect.cx() - orgRect.cx();
			cfg.winsize.cy = rect.cy() - (isErrEditHide ? miniHeight : normalHeight);
			cfg.WriteIni();
		}
		else {
			cfg.winsize.cx = cfg.winsize.cy = INVALID_SIZEVAL;
			cfg.WriteIni();
		}
		return	TRUE;

	case SETUP_MENUITEM:
		setupDlg.Exec();
		SetUserAgent();
		return	TRUE;

	case FONT_MENUITEM:
		ChooseFont();
		return TRUE;

	case RESETFONT_MENUITEM:
		*cfg.statusFont = 0;
		cfg.statusFontSize = 0;
		StatusEditSetup(TRUE);
		cfg.WriteIni();
		return TRUE;

	case EXTENDFILTER_MENUITEM:
		isExtendFilter = !isExtendFilter;
		SetExtendFilter();
		return	TRUE;

	case ABOUT_MENUITEM:
		aboutDlg.Exec();
		return	TRUE;

	case FASTCOPYURL_MENUITEM:
		::ShellExecuteW(NULL, NULL, LoadStrW(IDS_FASTCOPYURL), NULL, NULL, SW_SHOW);
		return	TRUE;

	case FCSUPPORTURL_MENUITEM:
		::ShellExecuteW(NULL, NULL, LoadStrW(IDS_FCSUPPORTURL), NULL, NULL, SW_SHOW);
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
		SetUserAgent();
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
		ShowHelpW(0, cfg.execDir, LoadStrW(IDS_FASTCOPYHELP), L"#usage");
		return	TRUE;

	case HELP_MENUITEM:
		ShowHelpW(0, cfg.execDir, LoadStrW(IDS_FASTCOPYHELP), NULL);
		return	TRUE;

	case FINACT_MENUITEM:
		finActDlg.Exec();
		SetFinAct(finActIdx < cfg.finActMax ? finActIdx : 0);
		return	TRUE;

	case UPDCHECK_MENUITEM:
		UpdateCheck();
		return	TRUE;

	case SRC_EDIT:
		if (wNotifyCode == EN_UPDATE) {
	//		srcEdit.Fit();
		}
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
		if (wID >= SRCHIST_MENUITEM_START && wID <= SRCHIST_MENUITEM_END) {
			SetHistPath(wID - SRCHIST_MENUITEM_START);
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
		if (IsWindowVisible()) {
			TaskTray(NIM_DELETE);
		}
		return	FALSE;

	case SC_MINIMIZE:
		PostMessage(WM_FASTCOPY_HIDDEN, 0, 0);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TMainDlg::EvNotify(UINT ctlID, NMHDR *pNmHdr)
{
	switch (pNmHdr->code) {
	case EN_LINK:
		{
			ENLINK	*el = (ENLINK *)pNmHdr;
			switch (el->msg) {
			case WM_LBUTTONUP:
				if (el->chrg.cpMax - el->chrg.cpMin < 1024) {
					WCHAR	wbuf[1024];
					pathEdit.SendMessageW(EM_EXSETSEL, 0, (LPARAM)(&(el->chrg)));
					pathEdit.SendMessageW(EM_GETSELTEXT, 0, (LPARAM)wbuf);
					::ShellExecuteW(hWnd, NULL, wbuf, NULL, NULL, SW_SHOWNORMAL);
				}
				break;
			}
		}
		return	FALSE;
	}

	return FALSE;
}

BOOL TMainDlg::EventInitMenu(UINT uMsg, HMENU hMenu, UINT uPos, BOOL fSystemMenu)
{
	if (uMsg == WM_INITMENU) {
		::CheckMenuItem(GetSubMenu(hMenu, 0), FIXPOS_MENUITEM,
			MF_BYCOMMAND|(IS_INVALID_POINT(cfg.winpos) ? MF_UNCHECKED : MF_CHECKED));
		::CheckMenuItem(GetSubMenu(hMenu, 0), FIXSIZE_MENUITEM,
			MF_BYCOMMAND|(IS_INVALID_SIZE(cfg.winsize) ? MF_UNCHECKED : MF_CHECKED));
		::CheckMenuItem(GetSubMenu(hMenu, 0), TOPLEVEL_MENUITEM,
			MF_BYCOMMAND|(cfg.isTopLevel ? MF_CHECKED : MF_UNCHECKED));

		::EnableMenuItem(GetSubMenu(hMenu, 2), SWAPTARGET_MENUITEM,
			MF_BYCOMMAND|(SwapTarget(TRUE) ? MF_ENABLED : MF_GRAYED));

		return	TRUE;
	}
	return	FALSE;
}

void TMainDlg::ShowHistMenu()
{
	BOOL	is_delete = GetCopyMode() == FastCopy::DELETE_MODE;
	HMENU	hMenu = ::CreatePopupMenu();
	TRect	rc;

	::GetWindowRect(GetDlgItem(HIST_BTN), &rc);

	for (int i=0; i < cfg.maxHistory; i++) {
		auto	&path = is_delete ? cfg.delPathHistory[i] : cfg.srcPathHistory[i];
		if (*path) {
			PathArray	pathArray;
			pathArray.RegisterMultiPath(path);

			int		len = pathArray.GetMultiPathLen();
			Wstr	w(len + 100);

			if (len > 160) {
				swprintf(w.Buf(), L"%.150s ... (%d files)", path, pathArray.Num());
			}
			else {
				wcscpy(w.Buf(), path);
			}

			AppendMenuW(hMenu, MF_STRING, SRCHIST_MENUITEM_START + i, w.s());
		}
	}
	if (::GetMenuItemCount(hMenu) == 0) {
		AppendMenuU8(hMenu, MF_STRING|MF_DISABLED|MF_GRAYED, SRCHIST_MENUITEM_START, "(None)");
	}

	::TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, rc.right-3, rc.bottom-3, 0, hWnd, NULL);
	::DestroyMenu(hMenu);
}

void TMainDlg::SetHistPath(int idx)
{
	BOOL	is_delete = GetCopyMode() == FastCopy::DELETE_MODE;
	auto	&path = is_delete ? cfg.delPathHistory[idx] : cfg.srcPathHistory[idx];

	if (!path || !*path) {
		return;
	}

	PathArray	pathArray;

	// CTL が押されている場合、現在の内容を加算
	if (::GetKeyState(VK_CONTROL) & 0x8000) {
		int		max_len = srcEdit.GetWindowTextLengthW() + 1;
		Wstr	wstr(max_len);

		srcEdit.GetWindowTextW(wstr.Buf(), max_len);
		pathArray.RegisterMultiPath(wstr.s(), CRLF);
	}

	pathArray.RegisterMultiPath(path);

	if (pathArray.Num() > 0) {
		int		max_len = pathArray.GetMultiPathLen(CRLF, NULW, TRUE);
		Wstr	wstr(max_len);

		if (pathArray.GetMultiPath(wstr.Buf(), max_len, CRLF, NULW, TRUE)) {
			srcEdit.SetWindowTextW(wstr.s());
		}
	}
}

void TMainDlg::SetStatMargin()
{
	static auto lcid = ::GetUserDefaultLCID();

	if (lcid == 0x411) return;

#define STAT_MARGIN	(2)
	TRect	rc;
	statEdit.GetClientRect(&rc);
	rc.left = rc.top = STAT_MARGIN;
	statEdit.SendMessage(EM_SETRECT, 0, (LPARAM)&rc);
}

BOOL TMainDlg::EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight)
{
	if (fwSizeType != SIZE_RESTORED && fwSizeType != SIZE_MAXIMIZED) {
		return	FALSE;
	}

	if (IsWindowVisible()) {
		EnableWindow(TRUE);
	}
	if ((showState & SS_TRAY) && IsWindowVisible()) {
		TaskTray(NIM_DELETE);
	}

	RefreshWindow();
	SetTimer(FASTCOPY_PAINT_TIMER, 100);
	SetStatMargin();

	return	TRUE;
}

void TMainDlg::GetSeparateArea(RECT *sep_rc)
{
	TRect	src_rc;
	TRect	dst_rc;
	TPoint	src_pt;
	TPoint	dst_pt;
	int		src_height;
	int		dst_width;

	srcEdit.GetWindowRect(&src_rc);
	src_height = src_rc.cy();
	src_pt.x = src_rc.left;
	src_pt.y = src_rc.top;
	::ScreenToClient(hWnd, &src_pt);

	::GetWindowRect(GetDlgItem(DST_COMBO), &dst_rc);
	dst_width = dst_rc.cx();
	dst_pt.x = dst_rc.left;
	dst_pt.y = dst_rc.top;
	::ScreenToClient(hWnd, &dst_pt);

	sep_rc->left   = dst_pt.x;
	sep_rc->right  = dst_pt.x + dst_width;
	sep_rc->top    = src_pt.y + src_height;
	sep_rc->bottom = dst_pt.y;
}

BOOL TMainDlg::IsSeparateArea(int x, int y)
{
	POINT	pt = {x, y};
	RECT	sep_rc;

	GetSeparateArea(&sep_rc);
	return	PtInRect(&sep_rc, pt);
}

BOOL TMainDlg::EvSetCursor(HWND cursorWnd, WORD nHitTest, WORD wMouseMsg)
{
	BOOL	need_set = captureMode;

	if (!need_set) {
		POINT	pt;
		::GetCursorPos(&pt);
		::ScreenToClient(hWnd, &pt);
		if (IsSeparateArea(pt.x, pt.y)) need_set = TRUE;
	}

	if (need_set) {
		static HCURSOR hResizeCursor = ::LoadCursor(TApp::hInst(), (LPCSTR)SEP_CUR);
		::SetCursor(hResizeCursor);
		return	TRUE;
	}

	return FALSE;
}

void TMainDlg::ResizeForSrcEdit(int diff_y)
{
	dlgItems[srcedit_item].diffCy += diff_y;

	DBG("ResizeForSrcEdit=%d\n", diff_y);
	TRect	rc;
	GetWindowRect(&rc);
	MoveWindow(rc.x(), rc.y(), rc.cx(), rc.cy() + diff_y, TRUE);
}

BOOL TMainDlg::EvMouseMove(UINT fwKeys, POINTS pos)
{
	if ((fwKeys & MK_LBUTTON) && captureMode) {
		if (lastYPos == (int)pos.y)
			return	TRUE;
		lastYPos = (int)pos.y;

		int		min_y = 20;

		if (pos.y < min_y) {
			pos.y = min_y;
		}

//		EvSize(SIZE_RESTORED, 0, 0);
		ResizeForSrcEdit(pos.y - dividYPos);

		dividYPos = (int)pos.y;
		return	TRUE;
	}
	return	FALSE;
}

BOOL TMainDlg::EventButton(UINT uMsg, int nHitTest, POINTS pos)
{
	switch (uMsg)
	{
	case WM_LBUTTONDOWN:
		if (!captureMode && IsSeparateArea(pos.x, pos.y)) {
			POINT	pt;
			captureMode = TRUE;
			::SetCapture(hWnd);
			::GetCursorPos(&pt);
			::ScreenToClient(hWnd, &pt);
			dividYPos = pt.y;
			lastYPos = 0;
		}
		return	TRUE;

	case WM_LBUTTONUP:
		if (captureMode) {
			captureMode = FALSE;
			::ReleaseCapture();
			return	TRUE;
		}
		break;
	}
	return	FALSE;
}


void set_fitflag(DWORD *flags, DWORD f, BOOL on)
{
	if (on) {
		*flags |= f;
	}
	else {
		*flags &= ~f;
	}
}

void set_fitskip(DWORD *flags, BOOL on)
{
	set_fitflag(flags, FIT_SKIP, on);
}

void TMainDlg::RefreshWindow(BOOL is_start_stop)
{
	if (!copyInfo) return;	// 通常はありえない。

	BOOL	is_delete = GetCopyMode() == FastCopy::DELETE_MODE;
	BOOL	is_del_test = is_delete || GetCopyMode() == FastCopy::TEST_MODE;
	BOOL	is_exec = fastCopy.IsStarting() || isDelay;
	BOOL	is_filter = IsDlgButtonChecked(FILTER_CHECK);
	BOOL	is_err_grow = !(isErrEditHide || IsListing()); // == !err_grow
	int		ydiff = (!isErrEditHide ? 0 : normalHeight - miniHeight);
	int		ydiffex = isExtendFilter ? 0 : filterHeight;
	DlgItem	*item = NULL;

	int err_items[] = { errstatus_item, errstatic_item, erredit_item };
	for (auto &i: err_items) {
		item = dlgItems + i;
		item->diffY = is_err_grow ? -ydiffex : 0;
		if (i == erredit_item) {
			item->flags  = isErrEditHide ? FIT_SKIP : X_FIT|(is_err_grow ? Y_FIT : BOTTOM_FIT);
			item->diffCy = ydiff + (is_err_grow ? ydiffex : 0);
		} else {
			item->flags = isErrEditHide ? FIT_SKIP : LEFT_FIT|(is_err_grow ? TOP_FIT : BOTTOM_FIT);
		}
	}

	int extf_items[] = { todatestatic_item, todatecombo_item, fdatestatic_item, fdatecombo_item,
		minsizestatic_item, maxsizestatic_item, minsizecombo_item, maxsizecombo_item };
	for (auto &i: extf_items) {
		item = dlgItems + i;
		set_fitskip(&item->flags, !isExtendFilter);
	}
	item = dlgItems + filterhelp_item;
	set_fitskip(&item->flags, !is_filter);

	int slide_items[] = { todatestatic_item, todatecombo_item, fdatestatic_item, fdatecombo_item,
		minsizestatic_item, maxsizestatic_item, minsizecombo_item, maxsizecombo_item };
	for (auto &i: extf_items) {
		item = dlgItems + i;
		set_fitskip(&item->flags, !isExtendFilter);
	}

	dlgItems[status_item].diffCy = isDelay ? 0 : dlgItems[atonce_item].wpos.cy;

	set_fitskip(&dlgItems[verify_item].flags, is_del_test);
	set_fitskip(&dlgItems[estimate_item].flags, is_del_test);
	set_fitskip(&dlgItems[owdel_item].flags, !is_delete);

	set_fitskip(&dlgItems[acl_item].flags, is_del_test);
	set_fitskip(&dlgItems[stream_item].flags, is_del_test);

	set_fitskip(&dlgItems[ok_item].flags, is_exec && IsListing());
	set_fitskip(&dlgItems[list_item].flags, is_exec && !IsListing());

	set_fitskip(&dlgItems[pauseok_item].flags, !is_exec || !IsListing());
	set_fitskip(&dlgItems[pauselist_item].flags, !is_exec || IsListing());

	set_fitskip(&dlgItems[atonce_item].flags, !isDelay);

	item = dlgItems + path_item;
	item->flags = X_FIT|(is_err_grow ? TOP_FIT : Y_FIT);
	item->diffY = -ydiffex;
	item->diffCy = ydiff + (is_err_grow ? 0 : ydiffex);

	FitDlgItems();

	if (is_start_stop) {
		BOOL	flg = !fastCopy.IsStarting();
		::EnableWindow(dlgItems[estimate_item].hWnd, flg);
		::EnableWindow(dlgItems[verify_item].hWnd, flg);
		::EnableWindow(dlgItems[ignore_item].hWnd, flg);
		::EnableWindow(dlgItems[owdel_item].hWnd, flg);
		::EnableWindow(dlgItems[acl_item].hWnd, flg);
		::EnableWindow(dlgItems[stream_item].hWnd, flg);
	}
}

void TMainDlg::SetSize(void)
{
	SetDlgItem(SRC_FILE_BUTTON, LEFT_FIT|TOP_FIT);
	SetDlgItem(SRC_EDIT, X_FIT|TOP_FIT|DIFF_CASCADE);
	SetDlgItem(DST_FILE_BUTTON, LEFT_FIT|TOP_FIT);
	SetDlgItem(DST_COMBO, X_FIT|TOP_FIT);
	SetDlgItem(STATUS_EDIT, X_FIT|TOP_FIT);
	SetDlgItem(MODE_COMBO, RIGHT_FIT|TOP_FIT);
	SetDlgItem(BUF_STATIC, RIGHT_FIT|TOP_FIT);
	SetDlgItem(BUFSIZE_EDIT, RIGHT_FIT|TOP_FIT);
	SetDlgItem(IGNORE_CHECK, RIGHT_FIT|TOP_FIT);
	SetDlgItem(ESTIMATE_CHECK, RIGHT_FIT|TOP_FIT);
	SetDlgItem(VERIFY_CHECK, RIGHT_FIT|TOP_FIT);
	SetDlgItem(LIST_BUTTON, RIGHT_FIT|TOP_FIT);
	SetDlgItem(PAUSELIST_BTN, RIGHT_FIT|TOP_FIT);
	SetDlgItem(IDOK, RIGHT_FIT|TOP_FIT);
	SetDlgItem(PAUSEOK_BTN, RIGHT_FIT|TOP_FIT);
	SetDlgItem(ATONCE_BUTTON, X_FIT|TOP_FIT);
	SetDlgItem(OWDEL_CHECK, LEFT_FIT|TOP_FIT);
	SetDlgItem(ACL_CHECK, LEFT_FIT|TOP_FIT);
	SetDlgItem(STREAM_CHECK, LEFT_FIT|TOP_FIT);
	SetDlgItem(SPEED_SLIDER, RIGHT_FIT|TOP_FIT);
	SetDlgItem(SPEED_STATIC, RIGHT_FIT|TOP_FIT);
	SetDlgItem(SAMEDRV_STATIC, RIGHT_FIT|TOP_FIT);
	SetDlgItem(INC_STATIC, LEFT_FIT|TOP_FIT);
	SetDlgItem(FILTERHELP_BUTTON, RIGHT_FIT|TOP_FIT);
	SetDlgItem(EXC_STATIC, LEFT_FIT|TOP_FIT);
	SetDlgItem(INCLUDE_COMBO, X_FIT|TOP_FIT);
	SetDlgItem(EXCLUDE_COMBO, X_FIT|TOP_FIT);
	SetDlgItem(FILTER_CHECK, RIGHT_FIT|TOP_FIT);

	SetDlgItem(FROMDATE_STATIC, LEFT_FIT|TOP_FIT);
	SetDlgItem(TODATE_STATIC, MIDX_FIT|TOP_FIT);
	SetDlgItem(MINSIZE_STATIC, RIGHT_FIT|TOP_FIT);
	SetDlgItem(MAXSIZE_STATIC, RIGHT_FIT|TOP_FIT);
	SetDlgItem(FROMDATE_COMBO, LEFT_FIT|MIDCX_FIT|TOP_FIT);
	SetDlgItem(TODATE_COMBO, MIDX_FIT|MIDCX_FIT|TOP_FIT);
	SetDlgItem(MINSIZE_COMBO, RIGHT_FIT|TOP_FIT);
	SetDlgItem(MAXSIZE_COMBO, RIGHT_FIT|TOP_FIT);

	SetDlgItem(PATH_EDIT, X_FIT|Y_FIT);
	SetDlgItem(ERR_STATIC, LEFT_FIT|BOTTOM_FIT);
	SetDlgItem(ERRSTATUS_STATIC, LEFT_FIT|BOTTOM_FIT);
	SetDlgItem(ERR_EDIT, X_FIT|BOTTOM_FIT);

	GetWindowRect(&orgRect);
}

/*
	DropFiles Event CallBack
*/
BOOL TMainDlg::EvDropFiles(HDROP hDrop)
{
	PathArray	pathArray(PathArray::DIRFILE_REDUCE);
	WCHAR	path[MAX_PATH_EX];
	BOOL	isDstDrop = IsDestDropFiles(hDrop);
	BOOL	isDeleteMode = GetCopyMode() == FastCopy::DELETE_MODE;
	int		max = isDstDrop ? 1 : ::DragQueryFileW(hDrop, 0xffffffff, 0, 0), max_len;

	// CTL が押されている場合、現在の内容を加算
	if (!isDstDrop) {
		if (::GetKeyState(VK_CONTROL) & 0x8000) {
			max_len = srcEdit.GetWindowTextLengthW() + 1;
			auto buf = make_unique<WCHAR[]>(max_len);
			srcEdit.GetWindowTextW(buf.get(), max_len);
			pathArray.RegisterMultiPath(buf.get(), CRLF);
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

		DWORD	attr = ::GetFileAttributesW(path);
		if (attr & FILE_ATTRIBUTE_DIRECTORY) {	// 0xffffffff も認める(for 95系OSのroot対策)
			MakePathW(path, NULL, L"");
		}
		pathArray.RegisterPath(path);
		pathArray.Sort();
	}
	::DragFinish(hDrop);

	if (pathArray.Num() > 0) {
		if (isDstDrop) {
			SetDlgItemTextW(DST_COMBO, path);
		}
		else {
			// 文字列連結用領域
			auto buf = make_unique<WCHAR[]>(max_len = pathArray.GetMultiPathLen(CRLF, NULW, TRUE));
			if (pathArray.GetMultiPath(buf.get(), max_len, CRLF, NULW, TRUE)) {
				srcEdit.SetWindowTextW(buf.get());
			}
		}
	}
	return	TRUE;
}

void TMainDlg::SetPriority(DWORD new_class)
{
	if (curPriority != new_class) {
		if (IsWinVista() && curPriority == IDLE_PRIORITY_CLASS) {
//			::SetPriorityClass(::GetCurrentProcess(), PROCESS_MODE_BACKGROUND_END);
//			::SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
		}

		::SetPriorityClass(::GetCurrentProcess(), new_class);

		if (IsWinVista() && new_class == IDLE_PRIORITY_CLASS) {
//			::SetPriorityClass(::GetCurrentProcess(), PROCESS_MODE_BACKGROUND_BEGIN);
//			::SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
		}
		curPriority = new_class;
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

		::ShowWindow(dlgItems[fdatestatic_item].hWnd, mode);
		::ShowWindow(dlgItems[todatestatic_item].hWnd, mode);
		::ShowWindow(dlgItems[minsizestatic_item].hWnd, mode);
		::ShowWindow(dlgItems[maxsizestatic_item].hWnd, mode);

		::ShowWindow(dlgItems[fdatecombo_item].hWnd, mode);
		::ShowWindow(dlgItems[todatecombo_item].hWnd, mode);
		::ShowWindow(dlgItems[minsizecombo_item].hWnd, mode);
		::ShowWindow(dlgItems[maxsizecombo_item].hWnd, mode);

		GetWindowRect(&rect);
		int height = rect.cy() + (isExtendFilter ? filterHeight : -filterHeight);
		MoveWindow(rect.x(), rect.y(), rect.cx(), height, IsWindowVisible());
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

	::EnableWindow(dlgItems[filter_item].hWnd,     is_show_filter);
	::EnableWindow(dlgItems[inccombo_item].hWnd,   new_status);
	::EnableWindow(dlgItems[exccombo_item].hWnd,   new_status);
	::EnableWindow(dlgItems[incstatic_item].hWnd,  new_status);
	::EnableWindow(dlgItems[excstatic_item].hWnd,  new_status);
	::EnableWindow(dlgItems[fdatecombo_item].hWnd, new_status);
	::EnableWindow(dlgItems[todatecombo_item].hWnd,  new_status);
	::EnableWindow(dlgItems[minsizecombo_item].hWnd, new_status);
	::EnableWindow(dlgItems[maxsizecombo_item].hWnd, new_status);
	::ShowWindow(dlgItems[filterhelp_item].hWnd, is_checked ? SW_SHOW : SW_HIDE);
}

BOOL TMainDlg::IsDestDropFiles(HDROP hDrop)
{
	POINT	pt;
	RECT	dst_rect;

	if (!::DragQueryPoint(hDrop, &pt) || !::ClientToScreen(hWnd, &pt)) {
		if (!::GetCursorPos(&pt))
			return	FALSE;
	}

	if (!::GetWindowRect(dlgItems[dstcombo_item].hWnd, &dst_rect) || !PtInRect(&dst_rect, pt))
		return	FALSE;

	return	TRUE;
}

BOOL ParseDateTime(const WCHAR *s, int64 *val, BOOL *is_abs)
{
	WCHAR	tmp[MINI_BUF];

	*is_abs = (*s != '+' && *s != '-') ? TRUE : FALSE;

	if (*is_abs) {
		int		cont = 0;
		WCHAR	*d = tmp;

		for ( ; *s; s++) {
			if (*s == ' ' || *s == '/' || *s == ':') {
				if (cont % 2) {
					return	FALSE;
				}
				cont = 0;
			}
			else if (*s < '0' || *s > '9') {
				return	FALSE;
			}
			else {
				cont++;
				*d++ = *s;
			}
		}
		*d = 0;
	}
	else {
		wcsncpyz(tmp, s, MINI_BUF);
	}

	*val = wcstoll(tmp, 0, 10);
	return	TRUE;
}

int64 TMainDlg::GetDateInfo(WCHAR *buf, BOOL is_end)
{
	FILETIME	ft;
	SYSTEMTIME	st;
	int64		&t = *(int64 *)&ft;
	int64		val = 0;
	BOOL		is_abs = FALSE;
	WCHAR		*p;
	WCHAR		*tok = strtok_pathW(buf, L"", &p);

	if (!tok || !ParseDateTime(tok, &val, &is_abs)) {
		return	-1;
	}

	if (is_abs) {	// absolute
		int64 day = 0;

		memset(&st, 0, sizeof(st));
		if (is_end) {
			st.wHour			= 23;
			st.wMinute			= 59;
			st.wSecond			= 59;
			st.wMilliseconds	= 999;
		}
		if (val > 16010000000000) {
			day = val / 1000000;
			int64 tim = val % 1000000;
			st.wHour   = (WORD)(tim / 10000);
			st.wMinute = (WORD)((tim % 10000) / 100);
			st.wSecond = (WORD)(tim % 100);
		}
		else if (val > 160100000000) {
			day = val / 10000;
			int64 tim = val % 10000;
			st.wHour   = (WORD)(tim / 100);
			st.wMinute = (WORD)(tim % 100);
		}
		else if (val > 1601000000) {
			day = val / 100;
			int64 tim = val % 100;
			st.wHour  = (WORD)(tim % 100);
		}
		else if (val > 16010000) {
			day = val;
		} else {
			return -1;
		}
		st.wYear	= (WORD) (day / 10000);
		st.wMonth	= (WORD)((day / 100) % 100);
		st.wDay		= (WORD) (day % 100);

		SYSTEMTIME	sst;
		TzSpecificLocalTimeToSystemTime(NULL, &st, &sst);
		if (!::SystemTimeToFileTime(&sst, &ft)) return -1;
	}
	else {
		::GetSystemTime(&st);
		::SystemTimeToFileTime(&st, &ft);
		switch (tok[wcslen(tok)-1]) {
		case 'W': val *= 60 * 60 * 24 * 7;	break;
		case 'D': val *= 60 * 60 * 24;		break;
		case 'h': val *= 60 * 60;			break;
		case 'm': val *= 60;				break;
		case 's':							break;
		default: return	-1;
		}
		t += val * 10000000;
	}

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
	BOOL	is_move_mode = copyInfo[idx].mode == FastCopy::MOVE_MODE;
	BOOL	is_test_mode = copyInfo[idx].mode == FastCopy::TEST_MODE;
	BOOL	is_filter = IsDlgButtonChecked(FILTER_CHECK);
	BOOL	is_listing = (exec_flags & LISTING_EXEC) ? TRUE : FALSE;
	BOOL	is_initerr_logging = (noConfirmStop && !is_listing) ? TRUE : FALSE;
	BOOL	is_fore = IsForeground();
	BOOL	is_ctrkey   = (::GetKeyState(VK_CONTROL) & 0x8000) ? TRUE : FALSE;
	BOOL	is_shiftkey = (::GetKeyState(VK_SHIFT) & 0x8000) ? TRUE : FALSE;
	// 削除は原則として、排他から除外するが、shiftキー実行すると排他動作
	BOOL	is_del_force = (is_delete_mode && is_fore && is_shiftkey) ? FALSE : TRUE;

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
		| (!is_delete_mode && !is_test_mode && IsDlgButtonChecked(ESTIMATE_CHECK) && !is_listing ?
			FastCopy::PRE_SEARCH : 0)
		| (is_delete_mode && IsDlgButtonChecked(OWDEL_CHECK) ? cfg.enableNSA ?
			FastCopy::OVERWRITE_DELETE_NSA : FastCopy::OVERWRITE_DELETE : 0)
		| (is_delete_mode && cfg.delDirWithFilter ? FastCopy::DELDIR_WITH_FILTER : 0)
		| (cfg.isSameDirRename ? FastCopy::SAMEDIR_RENAME : 0)
		| (cfg.isAutoSlowIo ? FastCopy::AUTOSLOW_IOLIMIT : 0)
		| (cfg.isReadOsBuf ? FastCopy::USE_OSCACHE_READ : 0)
		| (cfg.isWShareOpen ? FastCopy::WRITESHARE_OPEN : 0)
		| (skipEmptyDir && is_filter ? FastCopy::SKIP_EMPTYDIR : 0)
		| (!isReparse && !is_move_mode && !is_delete_mode ? FastCopy::REPARSE_AS_NORMAL : 0)
		| (is_move_mode && cfg.serialMove ? FastCopy::SERIAL_MOVE : 0)
		| (is_move_mode && cfg.serialVerifyMove ? FastCopy::SERIAL_VERIFY_MOVE : 0)
		| (isLinkDest ? FastCopy::RESTORE_HARDLINK : 0)
		| (isReCreate ? FastCopy::DEL_BEFORE_CREATE : 0)
		| (cfg.largeFetch ? 0 : FastCopy::NO_LARGE_FETCH)
		| ((diskMode == 2 || is_test_mode) ? FastCopy::FIX_DIFFDISK : (diskMode == 1) ?
			FastCopy::FIX_SAMEDISK : 0);

	info.flags			&= ~cfg.copyUnFlags;
	info.verifyFlags	= 0;

	if (!is_test_mode && !is_delete_mode) {
		if ((!is_listing && IsDlgButtonChecked(VERIFY_CHECK)) ||
			(is_listing && is_fore && is_ctrkey)) {
			info.verifyFlags = FastCopy::VERIFY_FILE
				| (cfg.verifyRemove ? FastCopy::VERIFY_REMOVE : 0);

			switch (cfg.hashMode) {
			case Cfg::MD5:
				info.verifyFlags |= FastCopy::VERIFY_MD5;
				break;
			case Cfg::SHA1:
				info.verifyFlags |= FastCopy::VERIFY_SHA1;
				break;
			case Cfg::SHA256:
				info.verifyFlags |= FastCopy::VERIFY_SHA256;
				break;
			case Cfg::XXHASH:
				info.verifyFlags |= FastCopy::VERIFY_XXHASH;
				break;
			}
		}
	}

	info.timeDiffGrace	= (int64)cfg.timeDiffGrace * 10000; // 1ms -> 100ns

	info.fileLogFlags	= (!is_listing || is_ctrkey || is_shiftkey) ? cfg.fileLogFlags : 0;
	if (is_test_mode) {	// テスト作成時は強制で詳細ログ出力
		info.fileLogFlags |= FastCopy::FILELOG_FILESIZE;
	}
	info.debugFlags		= cfg.debugFlags;

	info.bufSize		= (size_t)GetDlgItemInt(BUFSIZE_EDIT) * 1024 * 1024;
	info.maxRunNum		= MaxRunNum();
	info.netDrvMode		= cfg.netDrvMode;
	info.aclReset		= cfg.aclReset;
	info.maxOvlSize		= (size_t)cfg.maxOvlSize * 1024 * 1024;
	info.maxOvlNum		= cfg.maxOvlNum;
	info.maxOpenFiles	= cfg.maxOpenFiles;
	info.maxAttrSize	= (size_t)cfg.maxAttrSize * 1024 * 1024;
	info.maxDirSize		= (size_t)cfg.maxDirSize * 1024 * 1024;
	info.maxMoveSize	= (size_t)cfg.maxMoveSize * 1024 * 1024;
	info.maxDigestSize	= (size_t)cfg.maxDigestSize * 1024 * 1024;
	info.minSectorSize	= cfg.minSectorSize;
	info.maxLinkHash	= maxLinkHash;
	info.maxRunNum		= MaxRunNum();
	info.allowContFsize	= cfg.allowContFsize;
	info.nbMinSizeNtfs	= cfg.nbMinSizeNtfs * 1024LL;
	info.nbMinSizeFat	= cfg.nbMinSizeFat * 1024LL;
	info.hNotifyWnd		= hWnd;
	info.uNotifyMsg		= WM_FASTCOPY_MSG;
	info.fromDateFilter	= 0;
	info.toDateFilter	= 0;
	info.dlsvtMode		= dlsvtMode;
	info.minSizeFilter	= -1;
	info.maxSizeFilter	= -1;
	strcpy(info.driveMap, cfg.driveMap);

	errBufOffset		= 0;
	listBufOffset		= 0;
	lastTotalSec		= 0;
	calcTimes			= 0;

	memset(&ti, 0, sizeof(ti));
	isAbort = FALSE;

	resultStatus = TRUE;
	int		src_len = srcEdit.GetWindowTextLengthW() + 1;
	int		dst_len = ::GetWindowTextLengthW(GetDlgItem(DST_COMBO)) + 1;
	if (src_len <= 1 || !is_delete_mode && dst_len <= 1) {
		return	FALSE;
	}

	auto	_src = make_unique<WCHAR[]>(src_len);
	auto	src = _src.get();
	WCHAR	dst[MAX_PATH_EX] = L"";
	BOOL	ret = TRUE;
	BOOL	exec_confirm = FALSE;

	if (!isNoUI) {
		if (cfg.execConfirm || (is_fore && (::GetKeyState(VK_CONTROL) & 0x8000))) {
			exec_confirm = TRUE;
		} else {
			if (shellMode != SHELL_NONE && (exec_flags & CMDLINE_EXEC)) {
				Cfg::ShExtCfg *shCfg = (shellMode == SHELL_ADMIN) ? &cfg.shAdmin : &cfg.shUser;
				exec_confirm = is_delete_mode ? !shCfg->noConfirmDel : !shCfg->noConfirm;
			} else {
				exec_confirm = is_delete_mode ? !noConfirmDel : cfg.execConfirm;
			}
		}
	}

	if (srcEdit.GetWindowTextW(src, src_len) == 0
	|| !is_delete_mode && GetDlgItemTextW(DST_COMBO, dst, MAX_PATH_EX) == 0) {
		const char	*msg = "Can't get src or dst field";
		if (isNoUI) {
			WriteErrLogNoUI(msg);
		}
		else {
			TMsgBox(this).Exec(msg);
		}
		ret = FALSE;
	}

	SendDlgItemMessage(PATH_EDIT,   EM_AUTOURLDETECT, 0, 0);
	LRESULT evMask = pathEdit.SendMessage(EM_GETEVENTMASK, 0, 0) & ~ENM_LINK;
	pathEdit.SendMessage(EM_SETEVENTMASK, 0, evMask); 

	SendDlgItemMessage(PATH_EDIT, WM_SETTEXT, 0, (LPARAM)"");
	SendDlgItemMessage(STATUS_EDIT, WM_SETTEXT, 0, (LPARAM)"");
	SendDlgItemMessage(ERRSTATUS_STATIC, WM_SETTEXT, 0, (LPARAM)"");
	if (!*cfg.statusFont) StatusEditSetup();

	PathArray	srcArray, dstArray, incArray, excArray;
	srcArray.RegisterMultiPath(src, CRLF);
	if (!is_delete_mode)
		dstArray.RegisterPath(dst);

	// フィルタ
	WCHAR	from_date[MINI_BUF]=L"", to_date[MINI_BUF]=L"";
	WCHAR	min_size[MINI_BUF]=L"", max_size[MINI_BUF]=L"";
	WCHAR	*inc = NULL;
	WCHAR	*exc = NULL;
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
				const char *msg = LoadStr(IDS_DATEFORMAT_MSG);
				if (isNoUI) {
					WriteErrLogNoUI(AtoU8(msg));
				}
				else {
					TMsgBox(this).Exec(msg);
				}
				ret = FALSE;
			}
			if (info.minSizeFilter  == -2 || info.maxSizeFilter == -2) {
				const char *msg = LoadStr(IDS_SIZEFORMAT_MSG);
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
		SetDlgItemText(STATUS_EDIT, LoadStr(IDS_Info_Error)); //Error
	}

	int		src_list_len = srcArray.GetMultiPathLen(CRLF, NULW);
	auto	_src_list = make_unique<WCHAR[]>(src_list_len);
	auto	src_list = _src_list.get();

	int		srcline_len = srcArray.GetMultiPathLen();
	Wstr	srcline(srcline_len);
	srcArray.GetMultiPath(srcline.Buf(), srcline_len);

	if (ret && exec_confirm && (exec_flags & LISTING_EXEC) == 0) {
		srcArray.GetMultiPath(src_list, src_list_len, CRLF, NULW);
		WCHAR	*title =
					info.mode == FastCopy::MOVE_MODE   ? LoadStrW(IDS_MOVECONFIRM) :
					info.mode == FastCopy::SYNCCP_MODE ? LoadStrW(IDS_SYNCCONFIRM) :
					info.isRenameMode ? LoadStrW(IDS_DUPCONFIRM): NULL;
		int64	sv_flags = info.flags;	// delete confirm で変化しないか確認

		switch (TExecConfirmDlg(&info, &cfg, this, title, shellMode != SHELL_NONE).Exec(src_list,
				is_delete_mode ? NULL : dst)) {
		case IDOK:
			break;

		case RUNAS_BUTTON:
			ret = FALSE;
			RunAsAdmin(RUNAS_IMMEDIATE);
			break;

		default:
			ret = FALSE;
			if (shellMode != SHELL_NONE && !is_delete_mode)
				autoCloseLevel = NO_CLOSE;
			break;
		}

		if (ret && is_delete_mode && info.flags != sv_flags) {
			// flag が変化していた場合は、再登録
			ret = fastCopy.RegisterInfo(&srcArray, &dstArray, &info, &incArray, &excArray);
		}
	}

	if (ret) {
		if (info.mode != FastCopy::TEST_MODE) {
			if (is_delete_mode ? cfg.EntryDelPathHistory(srcline.Buf()) :
								 cfg.EntryPathHistory(srcline.Buf(), dst)) {
			//	SetPathHistory(SETHIST_LIST);
			}
			if (is_filter) {
				cfg.EntryFilterHistory(inc, exc, from_date, to_date, min_size, max_size);
			//	SetFilterHistory(SETHIST_LIST);
			}
			cfg.WriteIni();
		}
	}

	int	pathLogMax = (src_len + wsizeof(dst)) * 4 + 1; // UTF8換算

	if ((ret || is_initerr_logging) && (pathLogBuf = new char [pathLogMax])) {
		int len = sprintf(pathLogBuf, "<%s> %s" //Source
					, isUtf8Log ? AtoU8s(LoadStr(IDS_Log_Source)) : LoadStr(IDS_Log_Source) //Source
					, isUtf8Log ? WtoU8s(srcline.s()) : WtoAs(srcline.s())
					);
		if (!is_delete_mode) {
			len += sprintf(pathLogBuf + len, "\r\n<%s> %s" //DestDir
						, isUtf8Log ? AtoU8s(LoadStr(IDS_Log_DestDir)) : LoadStr(IDS_Log_DestDir) //DestDir
						, isUtf8Log ? WtoU8s(dst) : WtoAs(dst)
						);
		}
		if (inc && inc[0]) {
			len += sprintf(pathLogBuf + len, "\r\n<%s> %s" //Includ
						, isUtf8Log ? AtoU8s(LoadStr(IDS_Log_Includ)) : LoadStr(IDS_Log_Includ) //Includ
						, isUtf8Log ? WtoU8s(inc) : WtoAs(inc)
						);
		}
		if (exc && exc[0]) {
			len += sprintf(pathLogBuf + len, "\r\n<%s> %s" //Exclude
						, isUtf8Log ? AtoU8s(LoadStr(IDS_Log_Exclude)) : LoadStr(IDS_Log_Exclude) //Exclude
						, isUtf8Log ? WtoU8s(exc) : WtoAs(exc)
						);
		}
	
		len += sprintf(pathLogBuf + len, "\r\n<%s> %s" //Command
					, isUtf8Log ? AtoU8s(LoadStr(IDS_Log_Command)) : LoadStr(IDS_Log_Command) //Command
					, isUtf8Log ? AtoU8s(copyInfo[idx].list_str) : copyInfo[idx].list_str
					);
		if ((info.flags & (FastCopy::WITH_ACL|FastCopy::WITH_ALTSTREAM|FastCopy::OVERWRITE_DELETE
						 |FastCopy::OVERWRITE_DELETE_NSA)) ||
			(info.verifyFlags & FastCopy::VERIFY_FILE)) {
			len += sprintf(pathLogBuf + len, " (%s"
						, isUtf8Log ? AtoU8s(LoadStr(IDS_Log_with)) : LoadStr(IDS_Log_with) //with
						);
			if (!is_delete_mode && (info.verifyFlags & FastCopy::VERIFY_FILE))
				len += sprintf(pathLogBuf + len, " %s"
							, isUtf8Log ? AtoU8s(LoadStr(IDS_Log_Verify)) : LoadStr(IDS_Log_Verify) //Verify
							);
			if (!is_delete_mode && (info.flags & FastCopy::WITH_ACL))
				len += sprintf(pathLogBuf + len, " %s"
							, isUtf8Log ? AtoU8s(LoadStr(IDS_Log_ACL)) : LoadStr(IDS_Log_ACL) //ACL
							);
			if (!is_delete_mode && (info.flags & FastCopy::WITH_ALTSTREAM))
				len += sprintf(pathLogBuf + len, " %s"
							, isUtf8Log ? AtoU8s(LoadStr(IDS_Log_AltStream)) : LoadStr(IDS_Log_AltStream) //AltStream
							);
			if (is_delete_mode && (info.flags & FastCopy::OVERWRITE_DELETE))
				len += sprintf(pathLogBuf + len, " %s"
							, isUtf8Log ? AtoU8s(LoadStr(IDS_Log_OverWrite)) : LoadStr(IDS_Log_OverWrite) //OverWrite
							);
			if (is_delete_mode && (info.flags & FastCopy::OVERWRITE_DELETE_NSA))
				len += sprintf(pathLogBuf + len, " %s"
							, isUtf8Log ? AtoU8s(LoadStr(IDS_Log_NSA)) : LoadStr(IDS_Log_NSA) //NSA
							);
			len += sprintf(pathLogBuf + len, ")");
		}
		if (is_listing && isListLog) len += sprintf(pathLogBuf + len, " (%s)"
					, isUtf8Log ? AtoU8s(LoadStr(IDS_Log_ListingOnly)) : LoadStr(IDS_Log_ListingOnly) //Listing only
					);

		WCHAR	job[MAX_PATH] = L"";
		GetDlgItemTextW(JOBTITLE_STATIC, job, MAX_PATH);
		if (*job) {
			len += sprintf(pathLogBuf + len, "\r\n<%s> %s"
				, isUtf8Log ? AtoU8s(LoadStr(IDS_Log_JobName)) : LoadStr(IDS_Log_JobName) //JobName
				, isUtf8Log ? WtoU8s(job) : WtoAs(job)
				);
		}
		len += sprintf(pathLogBuf + len , "\r\n");
	}

	delete [] exc;
	delete [] inc;

	if (ret) {
		SendDlgItemMessage(PATH_EDIT, EM_SETTARGETDEVICE, 0, IsListing() ? 1 : 0);	// 折り返し
		SendDlgItemMessage(ERR_EDIT,  EM_SETTARGETDEVICE, 0, IsListing() ? 0 : 1);	// 折り返し
		SetMiniWindow();
		normalHeight = normalHeightOrg;
		SetDlgItemText(ERR_EDIT, "");
		ShareInfo::CheckInfo	ci;

		if ((is_delete_mode && is_del_force)
			|| fastCopy.IsTakeExclusivePriv()
			|| fastCopy.TakeExclusivePriv(forceStart, &ci)
			|| forceStart == 1) { // forceStart==1の場合も、可能な限りTakeをしておく
			ret = ExecCopyCore();
		}
		else {
			isDelay = TRUE;
			SetDlgItemText(STATUS_EDIT, LoadStr(ci.all_running >= MaxRunNum() ?
				IDS_WAIT_MANYPROC : IDS_WAIT_ACQUIREDRV));
			SetTimer(FASTCOPY_TIMER, FASTCOPY_TIMER_TICK*2);
			SetIcon(FCWAIT_ICON_IDX);
			if (showState & SS_TRAY) {
				TaskTray(NIM_MODIFY, FCWAIT_ICON_IDX, FASTCOPY);
			}
		}

		if (ret) {
			SetDlgItemText(IsListing() ? LIST_BUTTON : IDOK, LoadStr(IDS_CANCEL));
			if (auto *btn = IsListing() ? &pauseOkBtn : &pauseListBtn) {
				btn->SendMessage(BM_SETIMAGE, IMAGE_ICON, (LPARAM)hPauseIcon);
				btn->SetTipTextW(LoadStrW(IDS_PAUSE));
			}

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
		if (cfg.preventSleep) {
			::SetThreadExecutionState(ES_SYSTEM_REQUIRED|ES_CONTINUOUS);
		}
		RefreshWindow(TRUE);
		timerCnt = timerLast = 0;
		::GetCursorPos(&curPt);
		SetTimer(FASTCOPY_TIMER, FASTCOPY_TIMER_TICK);
		UpdateSpeedLevel();
	}
	return	ret;
}

BOOL TMainDlg::EndCopy(void)
{
	if (cfg.priority <= 0) {
		SetPriority(NORMAL_PRIORITY_CLASS);
	}
	::KillTimer(hWnd, FASTCOPY_TIMER);
	isPause = FALSE;

	BOOL	is_starting = fastCopy.IsStarting();

	if (is_starting) {
		fastCopy.GetTransInfo(&ti, TRUE);
		if (ti.total.allErrFiles == 0 && ti.total.allErrDirs == 0 &&
			(ti.total.abortCnt + ti.preTotal.abortCnt) == 0) {
			resultStatus = TRUE;
		}
		else {
			resultStatus = FALSE;
		}
		SetInfo(TRUE);

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
		SendDlgItemMessage(STATUS_EDIT, WM_SETTEXT, 0, (LPARAM)LoadStr(IDS_EndCopy_Cancelled)); // ---- Cancelled. ----
	}

	::SetThreadExecutionState(ES_CONTINUOUS);

	delete [] pathLogBuf;
	pathLogBuf = NULL;

	SetDlgItemText(IsListing() ? LIST_BUTTON : IDOK, LoadStr(IsListing() ?
		IDS_LISTING : IDS_EXECUTE));
	SetDlgItemText(SAMEDRV_STATIC, "");

	BOOL	is_auto_close = autoCloseLevel == FORCE_CLOSE
			|| autoCloseLevel == NOERR_CLOSE &&
				(!is_starting ||
					(ti.total.allErrFiles == 0 && ti.total.allErrDirs == 0 && errBufOffset == 0));

	if (!IsListing()) {
		if (is_starting && !isAbort) {
			ExecFinalAction(is_auto_close);
		}
		if (is_auto_close || isNoUI) {
			PostMessage(WM_CLOSE, 0, 0);
		}
		autoCloseLevel = NO_CLOSE;
		shellMode = SHELL_NONE;
	}
	RefreshWindow(TRUE);
	UpdateMenu();
	CheckVerifyExtension();

	::SetFocus(GetDlgItem(IsListing() ? LIST_BUTTON : IDOK));

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
	BOOL	is_err = ti.total.allErrFiles || ti.total.allErrDirs ||
			(ti.total.abortCnt || ti.preTotal.abortCnt);

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
				_snprintf(buf, sizeof(buf), "%s\r\n%s", svbuf, LoadStr(IDS_FinalAction_Wait)); //Wait for finish action...
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
				// ::ExitWindowsEx(EWX_POWEROFF|(is_force ? EWX_FORCE : 0), 0);
				::InitiateSystemShutdownEx(NULL, NULL, 0, is_force, FALSE,
					SHTDN_REASON_MAJOR_APPLICATION |
					SHTDN_REASON_MINOR_OTHER       |
					SHTDN_REASON_FLAG_PLANNED
				);
			}
		}
	}
	return	TRUE;
}

BOOL TMainDlg::CheckVerifyExtension()
{
	if (fastCopy.IsStarting()) return FALSE;

	WCHAR	buf[128];
	DWORD	len = GetDlgItemTextW(LIST_BUTTON, buf, wsizeof(buf));
	BOOL	is_set = GetCopyMode() != FastCopy::DELETE_MODE && IsForeground()
					&& (::GetKeyState(VK_CONTROL) & 0x8000) ? TRUE : FALSE;

	if (len <= 0) return FALSE;

	WCHAR	end_ch = buf[len-1];
	if ( is_set && end_ch != 'V') {
		wcscpy(buf + len, L"+V");
		listBtn.UnSetTipText();
		listBtn.SetTipTextW(LoadStrW(IDS_LISTVTIP));
	}
	else if (!is_set && end_ch == 'V') {
		buf[len-2] = 0;
		listBtn.UnSetTipText();
		listBtn.SetTipTextW(LoadStrW(IDS_LISTTIP));
	}
	else return FALSE;

	SetDlgItemTextW(LIST_BUTTON, buf);
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

				::SetThreadExecutionState(ES_CONTINUOUS);
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
				if (cfg.preventSleep) {
					::SetThreadExecutionState(ES_SYSTEM_REQUIRED|ES_CONTINUOUS);
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
		if (showState & SS_TEMP) {
			return TRUE;
		}
		switch (lParam) {
		case WM_LBUTTONDOWN: case WM_RBUTTONDOWN:
			SetForceForegroundWindow();
			Show();
			break;

		case WM_LBUTTONUP: case WM_RBUTTONUP: case NIN_BALLOONUSERCLICK:
			if (lParam == NIN_BALLOONUSERCLICK) {
				SetForceForegroundWindow();
			}
			Show();
			if (isErrEditHide && (ti.errBuf && ti.errBuf->UsedSize() || errBufOffset)) {
				SetNormalWindow();
			}
			if (showState & SS_TRAY) {
				TaskTray(NIM_DELETE);
			}
			break;
		}
		return	TRUE;

	case WM_FASTCOPY_HIDDEN:
		TaskTray(NIM_ADD, curIconIdx, FASTCOPY);
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

	case WM_FASTCOPY_UPDINFORES:
		if (TInetReqReply *irr = (TInetReqReply *)lParam) {
			UpdateCheckRes(irr);
		}
		return	TRUE;

	case WM_FASTCOPY_UPDDLRES:
		if (TInetReqReply *irr = (TInetReqReply *)lParam) {
			UpdateDlRes(irr);
		}
		return	TRUE;

	case WM_FASTCOPY_RESIZESRCEDIT:
		ResizeForSrcEdit((int)lParam - dlgItems[srcedit_item].diffCy);
		return	TRUE;

	case WM_FASTCOPY_SRCEDITFIT:
		srcEdit.Fit((BOOL)lParam);
		return	TRUE;

	case WM_FASTCOPY_TRAYTEMP:
		TaskTrayTemp((BOOL)lParam);
		return	TRUE;

	case WM_FASTCOPY_TRAYSETUP:
		setupDlg.SetSheetIdx(TRAY_SHEET);
		setupDlg.Exec();
		return	TRUE;

	case WM_FASTCOPY_POSTSETUP:
		PostSetup();
		return	TRUE;
	}
	return	FALSE;
}

BOOL TMainDlg::EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == TaskBarCreateMsg) {
		if (showState & SS_TRAY) {
			TaskTray(NIM_ADD, curIconIdx, FASTCOPY);
		}
		return	TRUE;
	}
	return	FALSE;
}

BOOL TMainDlg::EventSystem(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return	FALSE;
}

BOOL TMainDlg::EventCtlColor(UINT uMsg, HDC hDcCtl, HWND hWndCtl, HBRUSH *result)
{
	if (uMsg == WM_CTLCOLORSTATIC) {
		if (hWndCtl == bufEdit.hWnd) {
			static HBRUSH hBrush = ::CreateSolidBrush(::GetSysColor(COLOR_WINDOW));
			::SetBkColor(hDcCtl, ::GetSysColor(COLOR_WINDOW));
			*result = hBrush;
			return	TRUE;
		}
	}

	return	FALSE;
}

BOOL TMainDlg::EvTimer(WPARAM timerID, TIMERPROC proc)
{
	if (timerID == FASTCOPY_TIMER) {
		timerCnt++;

		if (isDelay) {
			ShareInfo::CheckInfo	ci;
			if (fastCopy.TakeExclusivePriv(forceStart, &ci)) {
				::KillTimer(hWnd, FASTCOPY_TIMER);
				isDelay = FALSE;
				ExecCopyCore();
			}
			else {
				SetDlgItemText(STATUS_EDIT, LoadStr(ci.all_running >= MaxRunNum() ?
					IDS_WAIT_MANYPROC : IDS_WAIT_ACQUIREDRV));
			}
		}
		else {
			if (timerCnt & 0x1) {
				UpdateSpeedLevel(TRUE); // check 500msec
			}

			if (timerCnt == 1 || ((timerCnt >> cfg.infoSpan) << cfg.infoSpan) == timerCnt) {
				SetInfo();
			}
		}
	}
	else if (timerID == FASTCOPY_PAINT_TIMER) {
		DBG("GetTrayIconState=%d\n", GetTrayIconState());
		KillTimer(timerID);
		InvalidateRect(NULL, TRUE);
		DBG("InvalidateRect timer\n");
	}
	else {
		DBG("Illegal timer(%d)\n", timerID);
		KillTimer(timerID);
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
	return MessageBoxW(wmsg.s(), wtitle.s(), style);
}

int TMainDlg::MessageBox(LPCSTR msg, LPCSTR title, UINT style)
{
	Wstr	wmsg(msg, BY_MBCS), wtitle(title, BY_MBCS);
	return MessageBoxW(wmsg.s(), wtitle.s(), style);
}

DWORD TMainDlg::UpdateSpeedLevel(BOOL is_timer)
{
	DWORD	waitLv = fastCopy.GetWaitLv();
	DWORD	newWaitLv = waitLv;

	if (speedLevel == SPEED_FULL) {
		newWaitLv = 0;
	}
	else if (speedLevel == SPEED_AUTO) {
		POINT	pt;
		::GetCursorPos(&pt);
		HWND	foreWnd = ::GetForegroundWindow();

		if (foreWnd == hWnd) {
			newWaitLv = FastCopy::AUTOSLOW_WAITLV;
		}
		else if (pt.x != curPt.x || pt.y != curPt.y || foreWnd != curForeWnd) {
			curPt = pt;
			curForeWnd = foreWnd;
			newWaitLv = cfg.waitLv;
			timerLast = timerCnt;
		}
		else if (is_timer
				 && newWaitLv > FastCopy::AUTOSLOW_WAITLV
				 && (timerCnt - timerLast) >= 10) {
			newWaitLv -= 1;
			timerLast = timerCnt;
		}
	}
	else if (speedLevel == SPEED_SUSPEND) {
		newWaitLv = FastCopy::SUSPEND_WAITLV;
	}
	else {
		static DWORD	waitArray[]    = { 95, 90, 80, 70, 55, 40, 30, 20, 10 };
		newWaitLv = waitArray[speedLevel - 1];
	}

	fastCopy.SetWaitLv(newWaitLv);

	if (!is_timer && fastCopy.IsStarting()) {
		if (cfg.priority <= 0) {
			SetPriority((speedLevel != SPEED_FULL || cfg.priority == 1) ?
							IDLE_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS);
		}
		if (speedLevel == SPEED_SUSPEND || isPause) {
			fastCopy.Suspend();
		}
		else {
			fastCopy.Resume();
		}
	}

	return	newWaitLv;
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
	auto	wbuf = make_unique<WCHAR[]>(len);

	if (!wbuf) return	FALSE;

	::SendMessageW(hSrc, WM_GETTEXT, len, (LPARAM)wbuf.get());
	::SendMessageW(hDst, WM_SETTEXT, 0, (LPARAM)wbuf.get());
	::EnableWindow(hDst, ::IsWindowEnabled(hSrc));

	return	TRUE;
}

BOOL TMainDlg::RunasSync(HWND hOrg)
{
	int		i;

/* GUI情報をコピー */
	DWORD	copy_items[] = { SRC_EDIT, DST_COMBO, BUFSIZE_EDIT, INCLUDE_COMBO, EXCLUDE_COMBO,
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
	SetItemEnable(GetCopyMode());
	SetExtendFilter();

	isRunAsStart = TRUE;

	return	TRUE;
}

void TMainDlg::Show(int mode)
{
	if (mode != SW_HIDE && mode != SW_MINIMIZE) {
		SetupWindow();
	}
	if (mode == SW_SHOWDEFAULT) {
		isNoUI = FALSE;
	}
	TDlg::Show(mode);
}

BOOL TMainDlg::TaskTray(int nimMode, int idx, LPCSTR tip, BOOL balloon)
{
	if (nimMode != NIM_DELETE) {
		SetIcon(idx);
	}

	if (cfg.taskbarMode && (showState & SS_TEMP) == 0) {
		Show(SW_MINIMIZE);
		showState = SS_MINMIZE;
		DBG("TaskTray min\n");
		return	TRUE;
	}

	BOOL	ret = FALSE;

	if (nimMode == NIM_DELETE) {
		showState &= ~SS_TRAY;
	} else {
		showState |= SS_TRAY;
	}
	HICON	hIcon = (nimMode == NIM_DELETE) ? NULL : hMainIcon[idx];

	NOTIFYICONDATA	tn = { IsWinVista() ? sizeof(tn) : NOTIFYICONDATA_V2_SIZE };
	tn.hWnd = hWnd;
	tn.uID = FASTCOPY_NIM_ID;
	tn.uFlags = NIF_MESSAGE|(hIcon ? NIF_ICON : 0)|(tip ? NIF_TIP : 0);
	tn.uCallbackMessage = WM_FASTCOPY_NOTIFY;
	tn.hIcon = hIcon;
	if (tip) {
		sprintf(tn.szTip, "%.127s", tip);
	}
	DWORD	sv_tout = 0;

	if (balloon && tip && !isNoUI) {
		tn.uFlags |= NIF_INFO;
		strncpyz(tn.szInfo, tip, sizeof(tn.szInfo));
		strncpyz(tn.szInfoTitle, FASTCOPY, sizeof(tn.szInfoTitle));
		tn.uTimeout		= cfg.finishNotifyTout * 1000;
		tn.dwInfoFlags	= NIIF_INFO | NIIF_NOSOUND;

		// Vista以降では uTimeout ではなく SPI_SETMESSAGEDURATION
		if (IsWinVista() && !IsWin10()) {
			::SystemParametersInfo(SPI_GETMESSAGEDURATION, 0, (void *)&sv_tout, 0);
			::SystemParametersInfo(SPI_SETMESSAGEDURATION, 0, (void *)(INT_PTR)tn.uTimeout, 0);
		}
	}

	ret = ::Shell_NotifyIcon(nimMode, &tn);

	if (balloon && tip && !isNoUI) {
		if (IsWinVista() && !IsWin10() && sv_tout) {
			::SystemParametersInfo(SPI_SETMESSAGEDURATION, 0, (void *)(INT_PTR)sv_tout, 0);
		}
	}
	if (nimMode == NIM_ADD) {
		ForceSetTrayIcon(hWnd, FASTCOPY_NIM_ID);
		auto	mode = GetTrayIconState();
		if (mode == TIS_ALL || mode == TIS_SHOW || isNoUI) {
			showState &= ~SS_MINMIZE;
		} else {
			showState |= SS_MINMIZE;
		}
	}
	if (showState & SS_MINMIZE) {
//		SetIcon(idx);
	}
	if (nimMode != NIM_DELETE && (showState & SS_TEMP) == 0) {
		Show((showState & SS_MINMIZE) ? SW_MINIMIZE : SW_HIDE);
		DBG("TaskTray showState=%x\n", showState);
	}

	return	ret;
}

void TMainDlg::TaskTrayTemp(BOOL on)
{
	if (on) {
		if ((showState & SS_TEMP) == 0) {
			showState |= SS_TEMP;
			TaskTray(NIM_ADD, FCNORM_ICON_IDX, FASTCOPY);
		}
	}
	else {
		if (showState & SS_TEMP) {
			TaskTray(NIM_DELETE);
			showState &= ~SS_TEMP;
		}
	}
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

ssize_t mega_str(char *buf, int64 val)
{
	if (val >= (10 * 1024 * 1024)) {
		return	comma_int64(buf, val / 1024 / 1024);
	}
	return	comma_double(buf, (double)val / 1024 / 1024, 1);
}

ssize_t double_str(char *buf, double val)
{
	return	comma_double(buf, val, val < 10.0 ? 2 : val < 1000.0 ? 1 : 0);
}

ssize_t time_str(char *buf, DWORD sec)
{
	DWORD	h = sec / 3600;
	DWORD	m = (sec % 3600) / 60;
	DWORD	s = (sec % 60);

	if (h == 0) return sprintf(buf, "%02u:%02u", m, s);

	return	sprintf(buf, "%02u:%02u:%02u", h, m, s);
}

ssize_t ticktime_str(char *buf, DWORD tick)
{
	if (tick < 60000) return sprintf(buf, "%.1f %s"
		, (double)tick / 1000
		, LoadStr(IDS_Info_sec) //sec
		);
	return	time_str(buf, tick/1000);
}


void TMainDlg::SetIcon(int idx)
{
//	DBG("SetIcon=%d\n", idx);

	curIconIdx = idx;

	SendMessage(WM_SETICON, ICON_BIG, (LPARAM)hMainIcon[idx]);
}

void TMainDlg::SetUserAgent()
{
	static auto icld = ::GetUserDefaultLCID();

	TInetSetUserAgent(Fmt("FastCopy %s%s %x/%d%d%d/%x %s", GetVersionStr(),
#ifdef _WIN64
		"(x64)",
#else
		"",
#endif
		icld,
		cfg.ignoreErr?1:0,
		cfg.enableVerify?1:0,
		cfg.estimateMode?1:0,
		cfg.jobMax,
		TGetHashedMachineIdStr()
	));
}

BOOL TMainDlg::SetTaskTrayInfo(BOOL is_finish, double doneRate, int remain_sec)
{
	char	buf[1024];

	*buf = 0;
	if (!cfg.taskbarMode) {
		char	s1[64], s2[64], s3[64], s4[64];
		int		len = 0;

		if (info.mode == FastCopy::DELETE_MODE) {
			mega_str(s1, ti.total.deleteTrans);
			comma_int64(s2, ti.total.deleteFiles);
			double_str(s3, (double)ti.total.deleteFiles * 1000 / ti.execTickCount);
			ticktime_str(s4, ti.execTickCount);

			len += sprintf(buf + len, IsListing() ?
				"FastCopy (%s%s %s%s %s)" :
				"FastCopy (%s%s %s%s %s%s %s)"
				, s1
				, LoadStr(IDS_Info_MiB) //MiB
				, s2
				, LoadStr(IDS_Info_files) //files
				, IsListing() ? s4 : s3
				, LoadStr(IDS_Info_MiB_s) //MiB/s
				, s4
				);
		}
		else if (ti.isPreSearch) {
			mega_str(s1, ti.preTotal.readTrans);
			comma_int64(s2, ti.preTotal.readFiles);
			ticktime_str(s3, ti.fullTickCount);

			len += sprintf(buf + len, "%s (%s %s %s/%s %s/%s)"
				, LoadStr(IDS_TaskTrayInfo_Estimating) //Estimating
				, LoadStr(IDS_TaskTrayInfo_Total) //Total
				, s1
				, LoadStr(IDS_Info_MiB) //MiB
				, s2
				, LoadStr(IDS_Info_files) //files
				, s3
				);
		}
		else {
			if ((info.flags & FastCopy::PRE_SEARCH) && !ti.isPreSearch && !is_finish
			&& doneRate >= 0.0001) {
				time_str(s1, remain_sec);
				len += sprintf(buf + len, "%d%% (%s %s) "
					, doneRatePercent
					, LoadStr(IDS_TaskTrayInfo_Remain) //Remain
					, s1
					);
			}
			mega_str(s1, ti.total.writeTrans);
			comma_int64(s2, ti.total.writeFiles);
			double_str(s3, (double)ti.total.writeTrans / ti.execTickCount / 1024 * 1000 / 1024);
			ticktime_str(s4, ti.fullTickCount);

			len += sprintf(buf + len, IsListing() ?
				"FastCopy (%s %s %s %s %s)" :
				"FastCopy (%s %s %s %s %s %s %s)"
				, s1
				, LoadStr(IDS_Info_MiB) //MiB
				, s2
				, LoadStr(IDS_Info_files) //files
				, IsListing() ? s4 : s3
				, LoadStr(IDS_Info_MiB_s) //MiB/s
				, s4
				);
		}
		if (is_finish) {
			strcpy(buf + len, LoadStr(IDS_TaskTrayInfo_Finished)); //Finished
		}
	}

	TaskTray(NIM_MODIFY, curIconIdx, buf, (finishNotify & 1) && is_finish);

	return	TRUE;
}

inline double SizeToCoef(double ave_size, int64 files)
{
	if (files <= 2) {
		return	0.0;
	}
#define AVE_WATERMARK_MIN (4.0   * 1024)
#define AVE_WATERMARK_MID (64.0  * 1024)

	if (ave_size < AVE_WATERMARK_MIN) return 2.0;
	double ret = AVE_WATERMARK_MID / ave_size;
	if (ret >= 2.0) return 2.0;
	if (ret <= 0.001) return 0.001;
	return ret;
}

BOOL TMainDlg::CalcInfo(double *doneRate, int *remain_sec, int *total_sec)
{
	int64	preFiles;
	int64	doneFiles;
	double	realDoneRate;
	auto	&pre = ti.preTotal;
	auto	&cur = ti.total;

	calcTimes++;

	preFiles  = pre.readFiles   + pre.readDirs   +
				pre.writeFiles  + pre.writeDirs  +
				pre.deleteFiles + pre.deleteDirs +
				pre.verifyFiles +
				(pre.skipFiles  + pre.skipDirs) / 2000 +
				pre.allErrFiles + pre.allErrDirs;

	doneFiles = cur.readFiles   + cur.readDirs   +
				cur.writeFiles  + cur.writeDirs  +
				cur.deleteFiles + cur.deleteDirs +
				cur.verifyFiles +
				(cur.skipFiles + cur.skipDirs) / 2000 +
				cur.allErrFiles + cur.allErrDirs;

	if (info.mode == FastCopy::DELETE_MODE) {
		realDoneRate = *doneRate = doneFiles / (preFiles + 0.01);
		lastTotalSec = (int)(ti.execTickCount / realDoneRate / 1000);
	}
	else {
		double verifyCoef = 0.9;
		int64 preTrans =
					(ti.isSameDrv ? pre.readTrans : 0) +
					pre.writeTrans +
//					pre.deleteTrans + 
					int64(pre.verifyTrans * verifyCoef) +
//					pre.skipTrans + 
					pre.allErrTrans;

		int64 doneTrans =
					(ti.isSameDrv ? cur.readTrans : 0) +
					cur.writeTrans +
//					cur.deleteTrans + 
					int64(cur.verifyTrans * verifyCoef) +
//					cur.skipTrans + 
					cur.allErrTrans;

		if ((info.verifyFlags & FastCopy::VERIFY_FILE)) {
//			preFiles  += pre.writeFiles;
//			doneFiles += cur.verifyFiles;

//			preTrans  += pre.writeTrans;
//			doneTrans += cur.verifyTrans;
		}

		DBG("F: %5d(%4d/%4d) / %5d  T: %7d / %7d   \n", int(doneFiles), int(cur.writeFiles),
			int(cur.readFiles), int(preFiles), int(doneTrans/1024), int(preTrans/1024));

		if (doneFiles > preFiles) preFiles = doneFiles;
		if (doneTrans > preTrans) preTrans = doneTrans;

		double doneTransRate = doneTrans / (preTrans  + 0.01);
		double doneFileRate  = doneFiles / (preFiles  + 0.01);
		double aveSize       = preTrans  / (preFiles  + 0.01);
		double coef          = SizeToCoef(aveSize, preFiles);

		*doneRate    = (doneFileRate * coef + doneTransRate) / (coef + 1);
		realDoneRate = *doneRate;

		if (realDoneRate < 0.0001) {
			realDoneRate = 0.0001;
		}

		lastTotalSec = (int)(ti.execTickCount / realDoneRate / 1000);

		static int prevTotalSec;
		if (calcTimes >= 5) {
			int diff = lastTotalSec - prevTotalSec;
			int remain = lastTotalSec - (ti.execTickCount / 1000);
			int diff_ratio = (remain >= 30) ? 8 : (remain >= 10) ? 4 : (remain >= 5) ? 2 : 1;
			lastTotalSec = prevTotalSec + (diff / diff_ratio);
		}
		prevTotalSec = lastTotalSec;

		DBG("recalc(%d) %.2f sec=%d coef=%.2f pre=%lld\n",
			(realDoneRate < 0.70 ? 10 : (11 - int(realDoneRate * 10))),
			doneTransRate, lastTotalSec, coef, preFiles);
	}

	*total_sec = lastTotalSec;
	if ((*remain_sec = *total_sec - (ti.execTickCount / 1000)) < 0) {
		*remain_sec = 0;
	}

	return	TRUE;
}

BOOL TMainDlg::SetInfo(BOOL is_finish)
{
	char	buf[1024], s1[64], s2[64], s3[64], s4[64], s5[64], s6[64];
	int		len = 0;
	double	doneRate = 0.0;
	int		remain_sec = 0;
	int		total_sec = 0;

	doneRatePercent = -1;

	fastCopy.GetTransInfo(&ti, is_finish || (showState & SS_TRAY) == 0);
	if (ti.fullTickCount == 0) {
		ti.fullTickCount++;
	}
	if (ti.execTickCount == 0) {
		ti.execTickCount++;
	}

#define FILELOG_MINSIZE	512 * 1024

	if (IsListing() && ti.listBuf->UsedSize() > 0
	|| (info.flags & FastCopy::LISTING) && ti.listBuf->UsedSize() >= FILELOG_MINSIZE) {
		::EnterCriticalSection(ti.listCs);
		if (IsListing()) {
			SetListInfo();
		}
		else {
			SetFileLogInfo();
		}
		::LeaveCriticalSection(ti.listCs);
	}

	if ((info.flags & FastCopy::PRE_SEARCH) && !ti.isPreSearch) {
		CalcInfo(&doneRate, &remain_sec, &total_sec);
		doneRatePercent = (int)(doneRate * 100);
		SetWindowTitle();
	}

	auto idx =	isPause   ? FCNORM2_ICON_IDX :
				is_finish ? ((IsListing() || isAbort) ? FCNORM_ICON_IDX : 
							 resultStatus ? FCDONE_ICON_IDX : FCERR_ICON_IDX) :
				(curIconIdx + 1) % MAX_FCNORM_ICON;

	SetIcon(idx);

	if (showState & SS_TRAY) {
		SetTaskTrayInfo(is_finish, doneRate, remain_sec);
		if ((showState & SS_TRAY) && !is_finish) {
			return TRUE;
		}
	}

	if ((info.flags & FastCopy::PRE_SEARCH) && !ti.isPreSearch && !is_finish
	&& doneRate >= 0.0001) {
		time_str(s1, remain_sec);
		len += sprintf(buf + len, "--%s %s (%d%%)--\r\n"
			, LoadStr(IDS_Info_Remain) //Remain
			, s1
			, doneRatePercent);
	}
	BOOL	is_del = (info.mode == FastCopy::DELETE_MODE) ? TRUE : FALSE;
	auto	&cur = ti.isPreSearch ? ti.preTotal : ti.total;

	if (ti.isPreSearch) {
		auto	&files = is_del ? cur.deleteFiles : cur.readFiles;
		mega_str(s1,    is_del ? cur.deleteTrans : cur.readTrans);
		comma_int64(s2, files);
		comma_int64(s3, is_del ? cur.deleteDirs  : cur.readDirs);
		ticktime_str(s4, ti.fullTickCount);
		double_str(s5, (double)files * 1000 / ti.fullTickCount);

		len += sprintf(buf + len,
			"---- %s ----\r\n"
			"%s = %s %s\r\n"
			"%s = %s (%s)\r\n"
			"%s = %s\r\n"
			"%s = %s %s"
			, LoadStr(IDS_Info_Estimating) //Estimating ...
			, LoadStr(IDS_Info_PreTrans) //PreTrans
			, s1
			, LoadStr(IDS_Info_MiB) //MiB
			, LoadStr(IDS_Info_PreFiles) //PreFiles
			, s2
			, s3
			, LoadStr(IDS_Info_PreTime) //PreTime
			, s4
			, LoadStr(IDS_Info_PreRate) //PreRate
			, s5
			, LoadStr(IDS_Info_files_s) //files/s
			);
	}
	else if (info.mode == FastCopy::DELETE_MODE) {
		mega_str(s1, cur.deleteTrans);
		comma_int64(s2, cur.deleteFiles);
		comma_int64(s3, cur.deleteDirs);
		ticktime_str(s4, ti.execTickCount);
		double_str(s5, (double)cur.deleteFiles * 1000 / ti.execTickCount);
		double_str(s6, (double)cur.writeTrans / ((double)ti.execTickCount/1000)/(1024*1024));

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
			, LoadStr(IDS_Info_TotalDel) //TotalDel
			, s1
			, LoadStr(IDS_Info_MiB) //MiB
			, LoadStr(IDS_Info_DelFiles) //DelFiles
			, s2
			, s3
			, LoadStr(IDS_Info_TotalTime) //TotalTime
			, s4
			, LoadStr(IDS_Info_FileRate) //FileRate
			, s5
			, LoadStr(IDS_Info_files_s) //files/s
			, LoadStr(IDS_Info_OverWrite) //OverWrite
			, s6
			, LoadStr(IDS_Info_MiB_s) //MiB/s
			);
	}
	else {
		if (IsListing()) {
			mega_str(s1, cur.writeTrans);
			comma_int64(s2, cur.writeFiles);
			comma_int64(s3, (info.flags & FastCopy::RESTORE_HARDLINK) ?
				cur.linkFiles : cur.writeDirs);
			comma_int64(s4, cur.writeDirs);

			len += sprintf(buf + len,
				(info.flags & FastCopy::RESTORE_HARDLINK) ? 
				"%s = %s %s\r\n"
				"%s = %s/%s (%s)\r\n" :
				"%s = %s %s\r\n"
				"%s = %s (%s)\r\n"
				, LoadStr(IDS_Info_TotalSize) //TotalSize
				, s1
				, LoadStr(IDS_Info_MiB) //MiB
				, LoadStr(IDS_Info_TotalFiles) //TotalFiles
				, s2
				, s3
				, s4
				);
		}
		else {
			mega_str(s1, cur.readTrans);
			mega_str(s2, cur.writeTrans);
			comma_int64(s3, cur.writeFiles);
			comma_int64(s4, (info.flags & FastCopy::RESTORE_HARDLINK) ?
				cur.linkFiles : cur.writeDirs);
			comma_int64(s5, cur.writeDirs);

			len += sprintf(buf + len,
				(info.flags & FastCopy::RESTORE_HARDLINK) ? 
				"%s = %s %s\r\n"
				"%s = %s %s\r\n"
				"%s = %s/%s (%s)\r\n" :
				"%s = %s %s\r\n"
				"%s = %s %s\r\n"
				"%s = %s (%s)\r\n"
				, LoadStr(IDS_Info_TotalRead) //TotalRead
				, s1
				, LoadStr(IDS_Info_MiB) //MiB
				, LoadStr(IDS_Info_TotalWrite) //TotalWrite
				, s2
				, LoadStr(IDS_Info_MiB) //MiB
				, LoadStr(IDS_Info_TotalFiles) //TotalFiles
				, s3
				, s4
				, s5
				);
		}

		if (cur.skipFiles || cur.skipDirs) {
			mega_str(s1, cur.skipTrans);
			comma_int64(s2, cur.skipFiles);
			comma_int64(s3, cur.skipDirs);

			len += sprintf(buf + len,
				"%s = %s %s\r\n"
				"%s = %s (%s)\r\n"
				, LoadStr(IDS_Info_TotalSkip) //TotalSkip
				, s1
				, LoadStr(IDS_Info_MiB) //MiB
				, LoadStr(IDS_Info_SkipFiles) //SkipFiles
				, s2
				, s3
				);
		}
		if (cur.deleteFiles || cur.deleteDirs) {
			mega_str(s1, cur.deleteTrans);
			comma_int64(s2, cur.deleteFiles);
			comma_int64(s3, cur.deleteDirs);

			len += sprintf(buf + len,
				"%s = %s %s\r\n"
				"%s = %s (%s)\r\n"
				, LoadStr(IDS_Info_TotalDel) //TotalDel
				, s1
				, LoadStr(IDS_Info_MiB) //MiB
				, LoadStr(IDS_Info_DelFiles) //DelFiles
				, s2
				, s3
				);
		}
		ticktime_str(s1, ti.fullTickCount);
		double_str(s2, (double)cur.writeTrans / ti.execTickCount / 1024 * 1000 / 1024);
		double_str(s3, (double)cur.writeFiles * 1000 / ti.execTickCount);

		len += sprintf(buf + len, IsListing() ?
			"%s = %s\r\n" :
			"%s = %s\r\n"
			"%s = %s %s\r\n"
			"%s = %s %s"
			, LoadStr(IDS_Info_TotalTime) //TotalTime
			, s1
			, LoadStr(IDS_Info_TransRate) //TransRate
			, s2
			, LoadStr(IDS_Info_MiB_s) //MiB/s
			, LoadStr(IDS_Info_FileRate) //FileRate
			, s3
			, LoadStr(IDS_Info_files_s) //files/s
			);

		if (info.verifyFlags & FastCopy::VERIFY_FILE) {
			mega_str(s1, cur.verifyTrans);
			comma_int64(s2, cur.verifyFiles);

			len += sprintf(buf + len, 
				"\r\n"
				"%s = %s %s\r\n"
				"%s = %s"
				, LoadStr(IDS_Info_VerifyRead) //VerifyRead
				, s1
				, LoadStr(IDS_Info_MiB) //MiB
				, LoadStr(IDS_Info_VerifyFiles) //VerifyFiles
				, s2
				);
		}
	}

	if (IsListing() && is_finish) {
		len += sprintf(buf + len, "\r\n---- %s%s %s ----"
			, LoadStr(IDS_Info_Listing) //Listing
			, (info.verifyFlags & FastCopy::VERIFY_FILE) ? LoadStr(IDS_Info_verify) : ""
			, LoadStr(IDS_Info_Done) //Done
			);
	}
	SetDlgItemText(STATUS_EDIT, buf);

	if (IsListing()) {
		if (is_finish) {
			int offset_v = listBufOffset / sizeof(WCHAR);
			SendDlgItemMessageW(PATH_EDIT, WM_GETTEXTLENGTH, 0, 0);

			comma_int64(s1, ti.total.allErrFiles);
			comma_int64(s2, ti.total.allErrDirs);

			sprintf(buf, "%s (%s : %s  %s : %s)"
				, LoadStr(IDS_Info_Finished) //Finished
				, LoadStr(IDS_Info_ErrorFiles) //ErrorFiles
				, s1
				, LoadStr(IDS_Info_ErrorDirs) //ErrorDirs
				, s2
				);
			SendDlgItemMessageW(PATH_EDIT, EM_SETSEL, offset_v, offset_v);
			SendDlgItemMessage(PATH_EDIT, EM_REPLACESEL, 0, (LPARAM)buf);
//			int line = SendDlgItemMessage(PATH_EDIT, EM_GETLINECOUNT, 0, 0);
//			SendDlgItemMessage(PATH_EDIT, EM_LINESCROLL, 0, line > 2 ? line -2 : 0);
		}
	}
	else if (is_finish) {
		comma_int64(s1, ti.total.allErrFiles);
		comma_int64(s2, ti.total.allErrDirs);

		sprintf(buf, ti.total.allErrFiles || ti.total.allErrDirs ?
			"%s (%s : %s  %s : %s)" : "%s"
			, LoadStr(IDS_Info_Finished) //Finished
			, LoadStr(IDS_Info_ErrorFiles) //ErrorFiles
			, s1
			, LoadStr(IDS_Info_ErrorDirs) //ErrorDirs
			, s2
			);
		SetDlgItemText(PATH_EDIT, buf);
		SetWindowTitle();
	}
	else
		SetDlgItemTextW(PATH_EDIT, ti.isPreSearch ? L"" : ti.curPath);

	SetDlgItemText(SAMEDRV_STATIC, info.mode == FastCopy::DELETE_MODE ? "" : ti.isSameDrv ?
		LoadStr(IDS_SAMEDISK) : LoadStr(IDS_DIFFDISK));

//	SetDlgItemText(SPEED_STATIC, buf);

	if ((info.ignoreEvent ^ ti.ignoreEvent) & FASTCOPY_ERROR_EVENT)
		CheckDlgButton(IGNORE_CHECK,
			((info.ignoreEvent = ti.ignoreEvent) & FASTCOPY_ERROR_EVENT) ? 1 : 0);

	if (isErrEditHide && ti.errBuf->UsedSize() > 0) {
		if (ti.errBuf->UsedSize() < 40) {	// abort only の場合は小さく
//			normalHeight = normalHeightOrg - (normalHeight - miniHeight) / 3;
		}
		SetNormalWindow();
	}
	if (ti.errBuf->UsedSize() > errBufOffset || errBufOffset == MAX_ERR_BUF || is_finish) {
		if (ti.errBuf->UsedSize() > errBufOffset && errBufOffset != MAX_ERR_BUF) {
			::EnterCriticalSection(ti.errCs);
			RichEditSetText(GetDlgItem(ERR_EDIT), (WCHAR *)(ti.errBuf->Buf() + errBufOffset));
			errBufOffset = (int)ti.errBuf->UsedSize();
			::LeaveCriticalSection(ti.errCs);
		}
		comma_int64(s1, ti.total.allErrFiles);
		comma_int64(s2, ti.total.allErrDirs);

		sprintf(buf, "(%s : %s / %s : %s)"
			, LoadStr(IDS_Info_ErrFiles) //ErrFiles
			, s1
			, LoadStr(IDS_Info_ErrDirs) //ErrDirs
			, s2);
		SetDlgItemText(ERRSTATUS_STATIC, buf);
	}

	if ((showState & SS_TRAY) == 0 && is_finish && (finishNotify & 2)) {
		if (::GetForegroundWindow() != hWnd) {
			::FlashWindow(hWnd, TRUE);
		}
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

	if ((info.flags & FastCopy::PRE_SEARCH) && !ti.isPreSearch && doneRatePercent >= 0
	&& fastCopy.IsStarting()) {
		p += swprintf(p, FMT_PERCENT, doneRatePercent);
	}
	if (taskbarList) {
		taskbarList->SetProgressState(hWnd, fastCopy.IsStarting() ? TBPF_NORMAL : TBPF_NOPROGRESS);
		if (fastCopy.IsStarting()) {
			taskbarList->SetProgressValue(hWnd, doneRatePercent, 100);
		}
	}

	WCHAR		_job[MAX_PATH] = L"";
	WCHAR		*job = _job;
	const WCHAR	*fin = finActIdx > 0 ? cfg.finActArray[finActIdx]->title : L"";

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

void TMainDlg::SetItemEnable(FastCopy::Mode mode)
{
	BOOL	is_delete = (mode == FastCopy::DELETE_MODE);
	BOOL	is_test   = (mode == FastCopy::TEST_MODE);

	::EnableWindow(GetDlgItem(DST_FILE_BUTTON), !is_delete);
	::EnableWindow(GetDlgItem(DST_COMBO), !is_delete);
	::ShowWindow(GetDlgItem(ESTIMATE_CHECK), (is_delete || is_test) ? SW_HIDE : SW_SHOW);
	::ShowWindow(GetDlgItem(VERIFY_CHECK), (is_delete || is_test) ? SW_HIDE : SW_SHOW);

	::EnableWindow(GetDlgItem(ACL_CHECK), !(is_delete || is_test));
	::EnableWindow(GetDlgItem(STREAM_CHECK), !(is_delete || is_test));
	::EnableWindow(GetDlgItem(OWDEL_CHECK), is_delete);

	::ShowWindow(GetDlgItem(ACL_CHECK), (is_delete || is_test) ? SW_HIDE : SW_SHOW);
	::ShowWindow(GetDlgItem(STREAM_CHECK), (is_delete || is_test) ? SW_HIDE : SW_SHOW);
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

	swprintf(buf, LoadStrW(IDS_FINACTMENU),
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
			LoadStr(IDS_FIX_SAMEDISK) : LoadStr(IDS_FIX_DIFFDISK));
	}
	SetWindowTitle();
}

BOOL TMainDlg::GetRunasInfo(WCHAR **user_dir, WCHAR **virtual_dir)
{
	WCHAR	**argv		= (WCHAR **)orgArgv;
	int		len;

	while (*argv && **argv == '/') {
		if (wcsicmpEx(*argv, RUNAS_STR, &len) == 0) {
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
	CheckDlgButton(ESTIMATE_CHECK, job->estimateMode);
	CheckDlgButton(VERIFY_CHECK, job->enableVerify);
	CheckDlgButton(IGNORE_CHECK, job->ignoreErr);
	CheckDlgButton(OWDEL_CHECK, job->enableOwdel);
	CheckDlgButton(ACL_CHECK, job->enableAcl);
	CheckDlgButton(STREAM_CHECK, job->enableStream);
	CheckDlgButton(FILTER_CHECK, job->isFilter);

	PathArray	pathArray;
	pathArray.RegisterMultiPath(job->src);
	int		len = pathArray.GetMultiPathLen(CRLF, NULW, TRUE);
	Wstr	wstr(len);
	pathArray.GetMultiPath(wstr.Buf(), len, CRLF, NULW, TRUE);
	srcEdit.SetWindowTextW(wstr.s());

	SetDlgItemTextW(DST_COMBO, job->dst);
	SetDlgItemTextW(INCLUDE_COMBO, job->includeFilter);
	SetDlgItemTextW(EXCLUDE_COMBO, job->excludeFilter);
	SetDlgItemTextW(FROMDATE_COMBO, job->fromDateFilter);
	SetDlgItemTextW(TODATE_COMBO, job->toDateFilter);
	SetDlgItemTextW(MINSIZE_COMBO, job->minSizeFilter);
	SetDlgItemTextW(MAXSIZE_COMBO, job->maxSizeFilter);
	SetItemEnable(copyInfo[idx].mode);
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

