static char *mainwinev_id = 
	"@(#)Copyright (C) 2004-2018 H.Shirouzu and FastCopy Lab, LLC.	mainwinev.cpp	ver3.50";
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Create					: 2018-10-08(Mon)
	Update					: 2018-10-08(Mon)
	Copyright				: H.Shirouzu / FastCopy Lab, LLC.
	License					: GNU General Public License version 3
	======================================================================== */

#include "mainwin.h"
#include <time.h>
#include "shellext/shelldef.h"

#pragma warning (push)
#pragma warning (disable : 4091) // dbghelp.h(1540): warning C4091: 'typedef ' ignored...
#include <dbghelp.h>
#pragma warning (pop)

using namespace std;

#define FASTCOPY_TIMER_TICK 250


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

		if (cfg.updCheck & 0x1) {	// 最新版確認
			time_t	now = time(NULL);

			if (cfg.lastUpdCheck + (24 * 3600) < now || cfg.lastUpdCheck > now) {	// 1日以上経過
				cfg.lastUpdCheck = now;
				cfg.WriteIni();
				UpdateCheck(TRUE);
			}
		}
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
		//ShowHelpW(0, cfg.execDir, LoadStrW(IDS_FASTCOPYHELP), L"#filter");
		ShowHelp(cfg.execDir, LoadStrW(IDS_FASTCOPYHELP), L"#filter");
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
		//ShowHelpW(0, cfg.execDir, LoadStrW(IDS_FASTCOPYHELP), L"#usage");
		ShowHelp(cfg.execDir, LoadStrW(IDS_FASTCOPYHELP), L"#usage");
		return	TRUE;

	case HELP_MENUITEM:
		//ShowHelpW(0, cfg.execDir, LoadStrW(IDS_FASTCOPYHELP), NULL);
		ShowHelp(cfg.execDir, LoadStrW(IDS_FASTCOPYHELP));
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

BOOL TMainDlg::EventScroll(UINT uMsg, int Code, int nPos, HWND hwndScrollBar)
{
	speedLevel = SetSpeedLevelLabel(this);
	UpdateSpeedLevel();

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


// Event内部関数

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

