static char *setuplg_id = 
	"@(#)Copyright (C) 2015-2018 H.Shirouzu		setupdlg.cpp	ver3.50";
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Create					: 2015-07-17(Fri)
	Update					: 2018-05-28(Mon)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	Modify					: Mapaler 2015-08-13
	======================================================================== */

#include "mainwin.h"
#include <stdio.h>

#include "shellext/shelldef.h"

static HICON hPlayIcon;
static HICON hPauseIcon;

using namespace std;

/*
	Setup用Sheet
*/
BOOL TSetupSheet::Create(int _resId, Cfg *_cfg, TSetupDlg *_parent)
{
	cfg    = _cfg;
	resId  = _resId;
	parent = setupDlg = _parent;
	initDone = FALSE;

	if (resId == MAIN_SHEET) {
		sv = new SheetDefSv;
	}

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

	initDone = TRUE;

	return	TRUE;
}

BOOL TSetupSheet::CheckData()
{
	if (resId == MAIN_SHEET) {
		return	TRUE;
	}
	else if (resId == IO_SHEET) {
		SetDlgItemInt(MAXTRANS_EDIT, GetDlgItemInt(OVLSIZE_EDIT) * GetDlgItemInt(OVLNUM_EDIT));
		int64	need_mb = FASTCOPY_BUFMB(GetDlgItemInt(OVLSIZE_EDIT), GetDlgItemInt(OVLNUM_EDIT));
		if (GetDlgItemInt(BUFSIZE_EDIT) < need_mb) {
			SetDlgItemInt(BUFSIZE_EDIT, cfg->bufSize);
			MessageBoxW(FmtW(LoadStrW(IDS_SMALLBUF_SETERR), need_mb));
			return	FALSE;
		}

		if (GetDlgItemInt(OVLSIZE_EDIT) <= 0 || GetDlgItemInt(OVLNUM_EDIT) <= 0) {
			MessageBox(LoadStr(IDS_SMALLVAL_SETERR));
			return	FALSE;
		}
		if (GetDlgItemInt(OVLSIZE_EDIT) > 4095) { // under 4GB
			MessageBox(LoadStr(IDS_BIGVAL_SETERR));
			return	FALSE;
		}
		if (GetDlgItemInt(OVLSIZE_EDIT) * GetDlgItemInt(OVLNUM_EDIT) * BUFIO_SIZERATIO >
			GetDlgItemInt(BUFSIZE_EDIT)) {
			SetDlgItemInt(OVLSIZE_EDIT, cfg->maxOvlSize);
			SetDlgItemInt(OVLNUM_EDIT, cfg->maxOvlNum);
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
	else if (resId == SHELLEXT_SHEET) {
		return	TRUE;
	}
	else if (resId == TRAY_SHEET) {
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
			sv->estimateMode	= cfg->estimateMode;
			sv->ignoreErr		= cfg->ignoreErr;
			sv->enableVerify	= cfg->enableVerify;
			sv->enableAcl		= cfg->enableAcl;
			sv->enableStream	= cfg->enableStream;
			sv->speedLevel		= cfg->speedLevel;
			sv->isExtendFilter	= cfg->isExtendFilter;
			sv->enableOwdel		= cfg->enableOwdel;
		}
		CheckDlgButton(ESTIMATE_CHECK, cfg->estimateMode);
		CheckDlgButton(IGNORE_CHECK, cfg->ignoreErr);
		CheckDlgButton(VERIFY_CHECK, cfg->enableVerify);
		CheckDlgButton(ACL_CHECK, cfg->enableAcl);
		CheckDlgButton(STREAM_CHECK, cfg->enableStream);
		SendDlgItemMessage(SPEED_SLIDER, TBM_SETRANGE, 0, MAKELONG(SPEED_SUSPEND, SPEED_FULL));
		SetSpeedLevelLabel(this, cfg->speedLevel);
		CheckDlgButton(EXTENDFILTER_CHECK, cfg->isExtendFilter);
		CheckDlgButton(OWDEL_CHECK, cfg->enableOwdel);

		if ((cfg->lcid != -1 || GetSystemDefaultLCID() != 0x409)) { // == 0x411 改成 != 0x409 让所有语言都可以切换到英文
			//::ShowWindow(GetDlgItem(LCID_CHECK), SW_SHOW);
			::EnableWindow(GetDlgItem(LCID_CHECK), TRUE);
			CheckDlgButton(LCID_CHECK, cfg->lcid == -1 || cfg->lcid != 0x409 ? FALSE : TRUE);
		}
	}
	else if (resId == IO_SHEET) {
		SetDlgItemInt(BUFSIZE_EDIT, cfg->bufSize);
		SetDlgItemInt(OVLSIZE_EDIT, cfg->maxOvlSize);
		SetDlgItemInt(OVLNUM_EDIT, cfg->maxOvlNum);
		SetDlgItemInt(MAXTRANS_EDIT, cfg->maxOvlSize * cfg->maxOvlNum);
		if (cfg->minSectorSize == 0 || cfg->minSectorSize == 4096) {
			CheckDlgButton(SECTOR4096_CHECK, cfg->minSectorSize == 4096);
		} else {
			::EnableWindow(GetDlgItem(SECTOR4096_CHECK), FALSE);
		}
		CheckDlgButton(READOSBUF_CHECK, cfg->isReadOsBuf);
		SetDlgItemInt64(NONBUFMINNTFS_EDIT, cfg->nbMinSizeNtfs);
		SetDlgItemInt64(NONBUFMINFAT_EDIT, cfg->nbMinSizeFat);
		CheckDlgButton(LARGEFETCH_CHK, cfg->largeFetch);
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

		SendDlgItemMessage(DLSVT_CMB, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_DLSVT_NONE));
		SendDlgItemMessage(DLSVT_CMB, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_DLSVT_FAT));
		SendDlgItemMessage(DLSVT_CMB, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_DLSVT_ALWAYS));
		SendDlgItemMessage(DLSVT_CMB, CB_SETCURSEL, cfg->dlsvtMode, 0);
		SetDlgItemText(TIMEGRACE_EDIT, Fmt("%lld", cfg->timeDiffGrace));

		SendDlgItemMessage(HASH_COMBO, CB_ADDSTRING, 0, (LPARAM)"MD5");
		SendDlgItemMessage(HASH_COMBO, CB_ADDSTRING, 0, (LPARAM)"SHA-1");
		SendDlgItemMessage(HASH_COMBO, CB_ADDSTRING, 0, (LPARAM)"SHA-256");
		SendDlgItemMessage(HASH_COMBO, CB_ADDSTRING, 0, (LPARAM)"xxHash");
		SendDlgItemMessage(HASH_COMBO, CB_SETCURSEL,
			cfg->hashMode <= Cfg::SHA256 ? int(cfg->hashMode) : 3, 0);
		CheckDlgButton(VERIFYREMOVE_CHK, cfg->verifyRemove);
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
	else if (resId == SHELLEXT_SHEET) {
		shext = make_unique<ShellExt>(cfg, this);
	}
	else if (resId == TRAY_SHEET) {
		CheckDlgButton(TASKBAR_CHECK, cfg->taskbarMode);
		SendDlgItemMessage(PLAY_BTN, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hPlayIcon);

		gwin = make_unique<TGifWin>(this);
		gwin->SetGif(TRAY_GIF);

		TRect	rc;
		::GetWindowRect(GetDlgItem(IDC_COMMENT), &rc);
		rc.ScreenToClient(hWnd);
		gwin->Create(rc.x() + 10, rc.bottom + 5);
		gwin->Show();
	}
	else if (resId == MISC_SHEET) {
		if (IsWin7()) {
			::ShowWindow(GetDlgItem(DIRSEL_CHK), SW_HIDE);
		}

		CheckDlgButton(UPDATE_CHK, cfg->updCheck ? TRUE : FALSE);
		CheckDlgButton(EXECCONFIRM_CHECK, cfg->execConfirm);
		CheckDlgButton(DIRSEL_CHK, cfg->dirSel);
		CheckDlgButton(FINISH_CHECK, (cfg->finishNotify & 1));
		CheckDlgButton(SPAN1_RADIO + cfg->infoSpan, 1);
		CheckDlgButton(PREVENTSLEEP_CHECK, cfg->preventSleep);
		CheckDlgButton(TEST_CHECK, cfg->testMode);
	}
	return	TRUE;
}

void TSetupSheet::ReflectToMainWindow()
{
	TWin *win = parent ? parent->Parent() : NULL;

	if (!win) return;

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

void TSetupSheet::EnableDlgItems(BOOL flg, const vector<int> &except_ids)
{
	auto	&ids = except_ids;

	for (HWND hw=::GetWindow(hWnd, GW_CHILD); hw; hw=::GetWindow(hw, GW_HWNDNEXT)) {
		if (int ctrId = ::GetDlgCtrlID(hw)) {
			if (find(begin(ids), end(ids), ctrId) == end(ids)) {
				::EnableWindow(hw, flg);
			}
		}
	}
}

BOOL TSetupSheet::GetData()
{
	if (resId == MAIN_SHEET) {
		cfg->estimateMode   = IsDlgButtonChecked(ESTIMATE_CHECK);
		cfg->ignoreErr      = IsDlgButtonChecked(IGNORE_CHECK);
		cfg->enableVerify   = IsDlgButtonChecked(VERIFY_CHECK);
		cfg->enableAcl      = IsDlgButtonChecked(ACL_CHECK);
		cfg->enableStream   = IsDlgButtonChecked(STREAM_CHECK);
		cfg->speedLevel     = (int)SendDlgItemMessage(SPEED_SLIDER, TBM_GETPOS, 0, 0);
		cfg->isExtendFilter = IsDlgButtonChecked(EXTENDFILTER_CHECK);
		cfg->enableOwdel    = IsDlgButtonChecked(OWDEL_CHECK);

		if (::IsWindowEnabled(GetDlgItem(LCID_CHECK))) {
			cfg->lcid = IsDlgButtonChecked(LCID_CHECK) ? 0x409 : -1;
		}

		ReflectToMainWindow();
	}
	else if (resId == IO_SHEET) {
		cfg->bufSize     = GetDlgItemInt(BUFSIZE_EDIT);
		cfg->maxOvlSize  = GetDlgItemInt(OVLSIZE_EDIT);
		cfg->maxOvlNum   = GetDlgItemInt(OVLNUM_EDIT);
		if (cfg->minSectorSize == 0 || cfg->minSectorSize == 4096) {
			cfg->minSectorSize = IsDlgButtonChecked(SECTOR4096_CHECK) ? 4096 : 0;
		}
		cfg->isReadOsBuf   = IsDlgButtonChecked(READOSBUF_CHECK);
		cfg->nbMinSizeNtfs = GetDlgItemInt64(NONBUFMINNTFS_EDIT);
		cfg->nbMinSizeFat  = GetDlgItemInt64(NONBUFMINFAT_EDIT);
		cfg->largeFetch    = IsDlgButtonChecked(LARGEFETCH_CHK);
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

		char buf[128];
		if (GetDlgItemText(TIMEGRACE_EDIT, buf, sizeof(buf)) > 0) {
			cfg->timeDiffGrace = strtoll(buf, 0, 10);
		}
		cfg->dlsvtMode = (int)SendDlgItemMessage(DLSVT_CMB, CB_GETCURSEL, 0, 0);

		int val = (int)SendDlgItemMessage(HASH_COMBO, CB_GETCURSEL, 0, 0);
		cfg->hashMode = (val <= 2) ? (Cfg::HashMode)val : Cfg::XXHASH;
		cfg->verifyRemove = IsDlgButtonChecked(VERIFYREMOVE_CHK);
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
	else if (resId == SHELLEXT_SHEET) {
		shext->RegisterShellExt();
	}
	else if (resId == TRAY_SHEET) {
		cfg->taskbarMode = IsDlgButtonChecked(TASKBAR_CHECK);
	}
	else if (resId == MISC_SHEET) {
		if (IsDlgButtonChecked(UPDATE_CHK)) {
			cfg->updCheck |= 0x1;
		} else {
			cfg->updCheck = 0;
		}

		cfg->execConfirm = IsDlgButtonChecked(EXECCONFIRM_CHECK);
		cfg->dirSel = IsDlgButtonChecked(DIRSEL_CHK);
		if (IsDlgButtonChecked(FINISH_CHECK)) {
			cfg->finishNotify |= 1;
		}
		else {
			cfg->finishNotify &= ~1;
		}
		cfg->infoSpan = IsDlgButtonChecked(SPAN1_RADIO) ? 0 :
			IsDlgButtonChecked(SPAN2_RADIO) ? 1 : 2;

		cfg->preventSleep = IsDlgButtonChecked(PREVENTSLEEP_CHECK);
		cfg->testMode = IsDlgButtonChecked(TEST_CHECK);
	}
	return	TRUE;
}

void TSetupSheet::Show(int mode)
{
	TDlg::Show(mode);

	if (resId == SHELLEXT_SHEET) {
	}
	else if (resId == TRAY_SHEET) {
		if (mode == SW_SHOW) {
			//EvCommand(0, PLAY_BTN, 0);
			GetParent()->GetParent()->PostMessage(WM_FASTCOPY_TRAYTEMP, 0, 1);
		}
		else {
			GetParent()->GetParent()->PostMessage(WM_FASTCOPY_TRAYTEMP, 0, 0);
			if (gwin->IsPlay()) {
				gwin->Stop();
			}
		}
	}
}

BOOL TSetupSheet::EvNcDestroy()
{
	if (resId == TRAY_SHEET) {
		GetParent()->GetParent()->PostMessage(WM_FASTCOPY_TRAYTEMP, 0, 0);
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
		else if (resId == SHELLEXT_SHEET) {
			section = L"#setting_shell";
		}
		else if (resId == TRAY_SHEET) {
			section = L"#setting_tray";
		}
		else if (resId == MISC_SHEET) {
			section = L"#setting_misc";
		}
		ShowHelpW(NULL, cfg->execDir, LoadStrW(IDS_FASTCOPYHELP), section);
		return	TRUE;
	}

	if (resId == IO_SHEET && initDone) {
		if (wNotifyCode == EN_CHANGE && (wID == OVLSIZE_EDIT || wID == OVLNUM_EDIT)) {
			int	os = GetDlgItemInt(OVLSIZE_EDIT);
			int	on = GetDlgItemInt(OVLNUM_EDIT);
			SetDlgItemInt(MAXTRANS_EDIT, os * on);
			DBG("set %d\n", os * on);
			return	TRUE;
		}
	}
	else if (resId == SHELLEXT_SHEET) {
		return	shext->Command(wNotifyCode, wID, hWndCtl);
	}
	else if (resId == TRAY_SHEET) {
		if (wID == PLAY_BTN) {
			if (gwin->IsPlay()) {
				if (gwin->IsPause()) {
					gwin->Resume();
					SendDlgItemMessage(PLAY_BTN, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hPauseIcon);
				}
				else {
					gwin->Pause();
					SendDlgItemMessage(PLAY_BTN, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hPlayIcon);
				}
			}
			else if (gwin->Play(WM_FASTCOPY_PLAYFIN)) {
				SendDlgItemMessage(PLAY_BTN, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hPauseIcon);
				if (::GetFocus() == GetDlgItem(PLAY_BTN)) {
					SetFocus();
				}
			}
			return	TRUE;
		}
	}

	return	FALSE;
}

BOOL TSetupSheet::EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_FASTCOPY_PLAYFIN) {
		SendDlgItemMessage(PLAY_BTN, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hPlayIcon);
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

	if (!hPlayIcon) {
		hPlayIcon = ::LoadIcon(TApp::hInst(), (LPCSTR)PLAY_ICON);
	}
	if (!hPauseIcon) {
		hPauseIcon = ::LoadIcon(TApp::hInst(), (LPCSTR)PAUSE_ICON);
	}
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

	if (rect.left == CW_USEDEFAULT) {
		GetWindowRect(&rect);
		MoveWindow(rect.x() + 30, rect.y() + 50, rect.cx(), rect.cy(), FALSE);
	}
	else {
		RestoreRectFromParent();
		MoveWindow(rect.x(), rect.y(), rect.cx(), rect.cy(), FALSE);
	}

	PostMessage(WM_FASTCOPY_FOCUS, 0, 0);

	return	TRUE;
}

BOOL TSetupDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK: case IDAPPLY:
		if (!sheet[curIdx].CheckData()) {
			return	TRUE;
		}
		for (int i=0; i < MAX_SETUP_SHEET; i++) {
			sheet[i].GetData();
		}
		cfg->WriteIni();
		GetParent()->PostMessage(WM_FASTCOPY_POSTSETUP, 0, 0);

		if (wID == IDOK) {
			EndDialog(wID);
		}
		return	TRUE;

	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;

	case SETUP_LIST:
		if (wNotifyCode == LBN_SELCHANGE) {
			SetSheet();
		}
		return	TRUE;
	}

	return	FALSE;
}

BOOL TSetupDlg::EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_FASTCOPY_FOCUS) {
		setup_list.SetFocus();
		return	TRUE;
	}
	return	FALSE;
}

BOOL TSetupDlg::EvNcDestroy()
{
	parent->GetWindowRect(&pRect);
	GetWindowRect(&rect);
	return	FALSE;
}


void TSetupDlg::SetSheet()
{
	if (nextIdx >= 0) {
		setup_list.SendMessage(LB_SETCURSEL, nextIdx, 0);
		nextIdx = -1;
	}

	int	idx = (int)setup_list.SendMessage(LB_GETCURSEL, 0, 0);

	if (idx < 0) {
		idx = 0;
		setup_list.SendMessage(LB_SETCURSEL, idx, 0);
	}
	if (curIdx >= 0 && curIdx != idx) {
		if (!sheet[curIdx].CheckData()) {
			setup_list.SendMessage(LB_SETCURSEL, curIdx, 0);
			return;
		}
	}
	for (int i=0; i < MAX_SETUP_SHEET; i++) {
		if (i != idx) {
			sheet[i].Show(SW_HIDE);
		}
	}
	sheet[idx].Show(SW_SHOW);
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

/*
	ShellExt初期化処理
*/
ShellExt::ShellExt(Cfg *_cfg, TSetupSheet *_parent) : cfg(_cfg), parent(_parent)
{
	isAdmin = ::IsUserAnAdmin();
	shCfg   = isAdmin ? &cfg->shAdmin : &cfg->shUser;

	Init();
}

ShellExt::~ShellExt()
{
	UnLoad();
}

BOOL ShellExt::Init()
{
	if (!Load(AtoWs(CURRENT_SHEXTDLL))) {
		auto	p = parent;

		p->EnableDlgItems(FALSE, { HELP_BUTTON, DISABLE_STATIC, -1 });
		::ShowWindow(p->GetDlgItem(DISABLE_STATIC), SW_SHOW);
		::ShowWindow(p->GetDlgItem(USER_STATIC), SW_HIDE);
		return	FALSE;
	}

	ReflectStatus();

	return	TRUE;
}

BOOL ShellExt::Load(const WCHAR *dll_name)
{
	UnLoad();

	if ((hShDll = TLoadLibraryExW(dll_name, TLT_EXEDIR)) == NULL) {
		return	FALSE;
	}

	RegisterDllProc   = (HRESULT (WINAPI *)(void))GetProcAddress(hShDll, REGISTER_PROC);
	UnRegisterDllProc = (HRESULT (WINAPI *)(void))GetProcAddress(hShDll, UNREGISTER_PROC);
	RegisterDllUserProc   = (HRESULT (WINAPI *)(void))GetProcAddress(hShDll, REGISTERUSER_PROC);
	UnRegisterDllUserProc = (HRESULT (WINAPI *)(void))GetProcAddress(hShDll, UNREGISTERUSER_PROC);
	IsRegisterDllProc= (BOOL (WINAPI *)(BOOL))GetProcAddress(hShDll, ISREGISTER_PROC);
	SetMenuFlagsProc = (BOOL (WINAPI *)(BOOL, int))GetProcAddress(hShDll, SETMENUFLAGS_PROC);
	GetMenuFlagsProc = (int (WINAPI *)(BOOL))GetProcAddress(hShDll, GETMENUFLAGS_PROC);
	SetAdminModeProc = (BOOL (WINAPI *)(BOOL))GetProcAddress(hShDll, SETADMINMODE_PROC);

// ver違いで proc error になるが、
// install時に overwrite failを検出して、リネームする

	if (!RegisterDllProc || !UnRegisterDllProc || !IsRegisterDllProc
		|| !RegisterDllUserProc || !UnRegisterDllUserProc
		|| !SetMenuFlagsProc || !GetMenuFlagsProc || !SetAdminModeProc) {
		UnLoad();
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

BOOL ShellExt::ReflectStatus(void)
{
	auto	p = parent;
	BOOL	isRegister = IsRegisterDllProc(isAdmin);

	p->CheckDlgButton(SHEXT_CHK, isRegister);

	::ShowWindow(p->GetDlgItem(isAdmin ? ADMIN_STATIC : USER_STATIC),  SW_SHOW);
	::ShowWindow(p->GetDlgItem(isAdmin ? USER_STATIC  : ADMIN_STATIC), SW_HIDE);

	p->EnableDlgItems(isRegister, { HELP_BUTTON, SHEXT_CHK, ADMIN_STATIC, USER_STATIC, -1 });

	int	flags = GetMenuFlagsProc(isAdmin);
	if (flags == -1) {
		flags = (SHEXT_RIGHT_COPY|SHEXT_RIGHT_DELETE|SHEXT_DD_COPY|SHEXT_DD_MOVE
				|SHEXT_AUTOCLOSE);
		// for old shellext
	}

	shCfg->noConfirm	= (flags & SHEXT_NOCONFIRM) ? TRUE : FALSE;
	shCfg->noConfirmDel	= (flags & SHEXT_NOCONFIRMDEL) ? TRUE : FALSE;
	shCfg->taskTray		= (flags & SHEXT_TASKTRAY) ? TRUE : FALSE;
	shCfg->autoClose	= (flags & SHEXT_AUTOCLOSE) ? TRUE : FALSE;

	p->CheckDlgButton(SHICON_CHECK, flags & SHEXT_MENUICON);

	p->CheckDlgButton(RIGHT_COPY_CHECK, flags & SHEXT_RIGHT_COPY);
	p->CheckDlgButton(RIGHT_DELETE_CHECK, flags & SHEXT_RIGHT_DELETE);
	p->CheckDlgButton(RIGHT_PASTE_CHECK, flags & SHEXT_RIGHT_PASTE);
	p->CheckDlgButton(RIGHT_SUBMENU_CHECK, flags & SHEXT_SUBMENU_RIGHT);
	p->CheckDlgButton(DD_COPY_CHECK, flags & SHEXT_DD_COPY);
	p->CheckDlgButton(DD_MOVE_CHECK, flags & SHEXT_DD_MOVE);
	p->CheckDlgButton(DD_SUBMENU_CHECK, flags & SHEXT_SUBMENU_DD);

	p->CheckDlgButton(NOCONFIRM_CHECK, shCfg->noConfirm);
	p->CheckDlgButton(NOCONFIRMDEL_CHECK, shCfg->noConfirmDel);
	p->CheckDlgButton(TASKTRAY_CHECK, shCfg->taskTray);
	p->CheckDlgButton(AUTOCLOSE_CHECK, shCfg->autoClose);

	return	TRUE;
}

BOOL ShellExt::Command(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case SHEXT_CHK:
		{
			BOOL	on = parent->IsDlgButtonChecked(SHEXT_CHK);
			parent->EnableDlgItems(on, { HELP_BUTTON, SHEXT_CHK, ADMIN_STATIC, USER_STATIC, -1 });
		}
		return	TRUE;

	default:
		isModify = TRUE;
		return	TRUE;
	}
	return	FALSE;
}

BOOL ShellExt::RegisterShellExt()
{
	DBG("RegisterShellExt\n");
	if (!Status()) {
		return	FALSE;
	}

	auto	p = parent;
	BOOL	is_register = parent->IsDlgButtonChecked(SHEXT_CHK);
	isModify = FALSE;

	shCfg->noConfirm    = is_register && p->IsDlgButtonChecked(NOCONFIRM_CHECK);
	shCfg->noConfirmDel = is_register && p->IsDlgButtonChecked(NOCONFIRMDEL_CHECK);
	shCfg->taskTray     = is_register && p->IsDlgButtonChecked(TASKTRAY_CHECK);
	shCfg->autoClose    = is_register && p->IsDlgButtonChecked(AUTOCLOSE_CHECK);
//	cfg->WriteIni();

	if (TOs64()) {
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
		HRESULT	hr = isAdmin ? UnRegisterDllProc() : UnRegisterDllUserProc();
		ReflectStatus();
		return	(hr == S_OK) ? TRUE : FALSE;
	}

	int		flags = SHEXT_MENU_DEFAULT;

//	if ((GetMenuFlagsProc(isAdmin) & SHEXT_MENUFLG_EX)) {
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

	if (!p->IsDlgButtonChecked(SHICON_CHECK))
		flags &= ~SHEXT_MENUICON;

	if (!p->IsDlgButtonChecked(RIGHT_COPY_CHECK))
		flags &= ~SHEXT_RIGHT_COPY;

	if (!p->IsDlgButtonChecked(RIGHT_DELETE_CHECK))
		flags &= ~SHEXT_RIGHT_DELETE;

	if (!p->IsDlgButtonChecked(RIGHT_PASTE_CHECK))
		flags &= ~SHEXT_RIGHT_PASTE;

	if (!p->IsDlgButtonChecked(DD_COPY_CHECK))
		flags &= ~SHEXT_DD_COPY;

	if (!p->IsDlgButtonChecked(DD_MOVE_CHECK))
		flags &= ~SHEXT_DD_MOVE;

	if (p->IsDlgButtonChecked(RIGHT_SUBMENU_CHECK))
		flags |= SHEXT_SUBMENU_RIGHT;

	if (p->IsDlgButtonChecked(DD_SUBMENU_CHECK))
		flags |= SHEXT_SUBMENU_DD;

	HRESULT	hr = isAdmin ? RegisterDllProc() : RegisterDllUserProc();
	if (hr != S_OK)
		return	FALSE;

	return	SetMenuFlagsProc(isAdmin, flags);
}

