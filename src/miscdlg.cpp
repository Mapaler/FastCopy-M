static char *miscdlg_id = 
	"@(#)Copyright (C) 2005-2012 H.Shirouzu		miscdlg.cpp	ver2.10";
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Create					: 2005-01-23(Sun)
	Update					: 2012-06-17(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "mainwin.h"
#include <stdio.h>

#include "shellext/shelldef.h"

/*
	About Dialog初期化処理
*/
TAboutDlg::TAboutDlg(TWin *_parent) : TDlg(ABOUT_DIALOG, _parent)
{
}

/*
	Window 生成時の CallBack
*/
BOOL TAboutDlg::EvCreate(LPARAM lParam)
{
	char	org[MAX_PATH], buf[MAX_PATH];

	GetDlgItemText(ABOUT_STATIC, org, sizeof(org));
	sprintf(buf, org, FASTCOPY_TITLE, GetVersionStr(), GetVerAdminStr(), GetCopyrightStr());
	SetDlgItemText(ABOUT_STATIC, buf);

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

BOOL TAboutDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:
	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;

	case URL_BUTTON:
		::ShellExecuteV(NULL, NULL, GetLoadStrV(IDS_FASTCOPYURL), NULL, NULL, SW_SHOW);
		return	TRUE;
	}
	return	FALSE;
}

/*
	Setup用Sheet
*/
BOOL TSetupSheet::Create(int _resId, Cfg *_cfg, TWin *_parent)
{
	cfg    = _cfg;
	resId  = _resId;
	parent = _parent;
	return	TDlg::Create();
}

BOOL TSetupSheet::EvCreate(LPARAM lParam)
{
	if (resId == SETUP_SHEET1) {
		SetDlgItemInt(BUFSIZE_EDIT, cfg->bufSize);
		CheckDlgButton(IGNORE_CHECK, cfg->ignoreErr);
		CheckDlgButton(ESTIMATE_CHECK, cfg->estimateMode);
		CheckDlgButton(ACL_CHECK, cfg->enableAcl && IS_WINNT_V);
		::EnableWindow(GetDlgItem(ACL_CHECK), IS_WINNT_V);
		CheckDlgButton(STREAM_CHECK, cfg->enableStream && IS_WINNT_V);
		::EnableWindow(GetDlgItem(STREAM_CHECK), IS_WINNT_V);
		CheckDlgButton(VERIFY_CHECK, cfg->enableVerify);
		CheckDlgButton(OWDEL_CHECK, cfg->enableOwdel);
		CheckDlgButton(EXTENDFILTER_CHECK, cfg->isExtendFilter);
		SendDlgItemMessage(SPEED_SLIDER, TBM_SETRANGE, FALSE,
			MAKELONG(SPEED_SUSPEND, SPEED_FULL));
		SetSpeedLevelLabel(this, cfg->speedLevel);
	}
	else if (resId == SETUP_SHEET2) {
		SetDlgItemInt(MAXTRANS_EDIT, cfg->maxTransSize);
		CheckDlgButton(READOSBUF_CHECK, cfg->isReadOsBuf);
		SetDlgItemInt(NONBUFMINNTFS_EDIT, cfg->nbMinSizeNtfs);
		SetDlgItemInt(NONBUFMINFAT_EDIT, cfg->nbMinSizeFat);
		SetDlgItemText(DRIVEMAP_EDIT, cfg->driveMap);
	}
	else if (resId == SETUP_SHEET3) {
		CheckDlgButton(SAMEDIR_RENAME_CHECK, cfg->isSameDirRename);
		CheckDlgButton(EMPTYDIR_CHECK, cfg->skipEmptyDir);
		CheckDlgButton(REPARSE_CHECK, cfg->isReparse);
		::EnableWindow(GetDlgItem(REPARSE_CHECK), IS_WINNT_V);
		CheckDlgButton(MOVEATTR_CHECK, cfg->enableMoveAttr);
		CheckDlgButton(SERIALMOVE_CHECK, cfg->serialMove);
		CheckDlgButton(SERIALVERIFYMOVE_CHECK, cfg->serialVerifyMove);
	}
	else if (resId == SETUP_SHEET4) {
		CheckDlgButton(NSA_CHECK, cfg->enableNSA);
		CheckDlgButton(DELDIR_CHECK, cfg->delDirWithFilter);
	}
	else if (resId == SETUP_SHEET5) {
		SetDlgItemInt(HISTORY_EDIT, cfg->maxHistoryNext);
		CheckDlgButton(ERRLOG_CHECK, cfg->isErrLog);
		CheckDlgButton(UTF8LOG_CHECK, cfg->isUtf8Log && IS_WINNT_V);
		::EnableWindow(GetDlgItem(UTF8LOG_CHECK), IS_WINNT_V);
		CheckDlgButton(FILELOG_CHECK, cfg->fileLogMode);
		CheckDlgButton(ACLERRLOG_CHECK, cfg->aclErrLog && IS_WINNT_V);
		::EnableWindow(GetDlgItem(ACLERRLOG_CHECK), IS_WINNT_V);
		CheckDlgButton(STREAMERRLOG_CHECK, cfg->streamErrLog && IS_WINNT_V);
		::EnableWindow(GetDlgItem(STREAMERRLOG_CHECK), IS_WINNT_V);
	}
	else if (resId == SETUP_SHEET6) {
		CheckDlgButton(FORCESTART_CHECK, cfg->forceStart);
		CheckDlgButton(EXECCONFIRM_CHECK, cfg->execConfirm);
		CheckDlgButton(TASKBAR_CHECK, cfg->taskbarMode);
		CheckDlgButton(SPAN1_RADIO + cfg->infoSpan, 1);

		if ((cfg->lcid != -1 || GetSystemDefaultLCID() == 0x411) && IS_WINNT_V) {
			::ShowWindow(GetDlgItem(LCID_CHECK), SW_SHOW);
			::EnableWindow(GetDlgItem(LCID_CHECK), TRUE);
			CheckDlgButton(LCID_CHECK, cfg->lcid == -1 || cfg->lcid == 0x411 ? FALSE : TRUE);
		}
	}

	RECT	rc;
	POINT	pt;
	::GetWindowRect(parent->GetDlgItem(SETUP_LIST), &rc);
	pt.x = rc.right;
	pt.y = rc.top;
	ScreenToClient(parent->hWnd, &pt);
	SetWindowPos(0, pt.x + 10, pt.y - 2, 0, 0, SWP_NOSIZE|SWP_NOZORDER);

	SetWindowLong(GWL_EXSTYLE, GetWindowLong(GWL_EXSTYLE)|WS_EX_CONTROLPARENT);

	return	TRUE;
}

BOOL TSetupSheet::GetData()
{
	if (resId == SETUP_SHEET1) {
		cfg->bufSize        = GetDlgItemInt(BUFSIZE_EDIT);
		cfg->ignoreErr      = IsDlgButtonChecked(IGNORE_CHECK);
		cfg->estimateMode   = IsDlgButtonChecked(ESTIMATE_CHECK);
		cfg->enableAcl      = IsDlgButtonChecked(ACL_CHECK);
		cfg->enableStream   = IsDlgButtonChecked(STREAM_CHECK);
		cfg->enableVerify   = IsDlgButtonChecked(VERIFY_CHECK);
		cfg->enableOwdel    = IsDlgButtonChecked(OWDEL_CHECK);
		cfg->isExtendFilter = IsDlgButtonChecked(EXTENDFILTER_CHECK);
		cfg->speedLevel     = (int)SendDlgItemMessage(SPEED_SLIDER, TBM_GETPOS, 0, 0);
	}
	else if (resId == SETUP_SHEET2) {
		cfg->maxTransSize  = GetDlgItemInt(MAXTRANS_EDIT);
		cfg->isReadOsBuf   = IsDlgButtonChecked(READOSBUF_CHECK);
		cfg->nbMinSizeNtfs = GetDlgItemInt(NONBUFMINNTFS_EDIT);
		cfg->nbMinSizeFat  = GetDlgItemInt(NONBUFMINFAT_EDIT);

		char	c, last_c = 0, buf[sizeof(cfg->driveMap)];
		GetDlgItemText(DRIVEMAP_EDIT, buf, sizeof(buf));
		for (int i=0; i < sizeof(buf) && (c = buf[i]); i++) {
			if (c >= 'a' && c <= 'z') buf[i] = c = toupper(c);
			if ((c < 'A' || c > 'Z' || strchr(buf+i+1, c)) && c != ',' || c == last_c) {
				MessageBox(buf, "Invalid DriveGroup format");
				return	FALSE;
			}
			last_c = c;
		}
		strcpy(cfg->driveMap, buf);
	}
	else if (resId == SETUP_SHEET3) {
		cfg->isSameDirRename  = IsDlgButtonChecked(SAMEDIR_RENAME_CHECK);
		cfg->skipEmptyDir     = IsDlgButtonChecked(EMPTYDIR_CHECK);
		cfg->isReparse        = IsDlgButtonChecked(REPARSE_CHECK);
		cfg->enableMoveAttr   = IsDlgButtonChecked(MOVEATTR_CHECK);
		cfg->serialMove       = IsDlgButtonChecked(SERIALMOVE_CHECK);
		cfg->serialVerifyMove = IsDlgButtonChecked(SERIALVERIFYMOVE_CHECK);
	}
	else if (resId == SETUP_SHEET4) {
		cfg->enableNSA        = IsDlgButtonChecked(NSA_CHECK);
		cfg->delDirWithFilter = IsDlgButtonChecked(DELDIR_CHECK);
	}
	else if (resId == SETUP_SHEET5) {
		cfg->maxHistoryNext = GetDlgItemInt(HISTORY_EDIT);
		cfg->isErrLog       = IsDlgButtonChecked(ERRLOG_CHECK);
		cfg->isUtf8Log      = IsDlgButtonChecked(UTF8LOG_CHECK) && IS_WINNT_V;
		cfg->fileLogMode    = IsDlgButtonChecked(FILELOG_CHECK);
		cfg->aclErrLog      = IsDlgButtonChecked(ACLERRLOG_CHECK) && IS_WINNT_V;
		cfg->streamErrLog   = IsDlgButtonChecked(STREAMERRLOG_CHECK) && IS_WINNT_V;
	}
	else if (resId == SETUP_SHEET6) {
		cfg->execConfirm = IsDlgButtonChecked(EXECCONFIRM_CHECK);
		cfg->forceStart  = IsDlgButtonChecked(FORCESTART_CHECK);
		cfg->taskbarMode = IsDlgButtonChecked(TASKBAR_CHECK);
		cfg->infoSpan    =	IsDlgButtonChecked(SPAN1_RADIO) ? 0 :
							IsDlgButtonChecked(SPAN2_RADIO) ? 1 : 2;
			::KillTimer(hWnd, FASTCOPY_TIMER);


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


/*
	Setup Dialog初期化処理
*/
TSetupDlg::TSetupDlg(Cfg *_cfg, TWin *_parent) : TDlg(SETUP_DIALOG, _parent)
{
	cfg = _cfg;
}

/*
	Window 生成時の CallBack
*/
BOOL TSetupDlg::EvCreate(LPARAM lParam)
{
	setup_list.AttachWnd(GetDlgItem(SETUP_LIST));

	for (int i=0; i < MAX_SETUP_SHEET; i++) {
		sheet[i].Create(SETUP_SHEET1 + i, cfg, this);
		setup_list.SendMessage(LB_ADDSTRING, 0, (LPARAM)GetLoadStr(IDS_SETUP_SHEET1 + i));
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
	case IDOK:
		{
			for (int i=0; i < MAX_SETUP_SHEET; i++) {
				sheet[i].GetData();
			}
			cfg->WriteIni();
			EndDialog(wID);
		}
		return	TRUE;

	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;

	case SETUP_LIST:
		SetSheet();
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
	for (int i=0; i < MAX_SETUP_SHEET; i++) {
		sheet[i].Show(i == idx ? SW_SHOW : SW_HIDE);
	}
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
#define ISREGISTER_PROC		"IsRegistServer"
#define SETMENUFLAGS_PROC	"SetMenuFlags"
#define GETMENUFLAGS_PROC	"GetMenuFlags"
#define UPDATEDLL_PROC		"UpdateDll"

BOOL ShellExt::Load(void *parent_dir, void *dll_name)
{
	if (hShellExtDll) UnLoad();

	WCHAR	path[MAX_PATH];
	MakePathV(path, parent_dir, dll_name);
	if ((hShellExtDll = TLoadLibraryV(path)) == NULL)
		return	FALSE;

	RegisterDllProc		= (HRESULT (WINAPI *)(void))GetProcAddress(hShellExtDll, REGISTER_PROC);
	UnRegisterDllProc	= (HRESULT (WINAPI *)(void))GetProcAddress(hShellExtDll, UNREGISTER_PROC);
	IsRegisterDllProc	= (BOOL (WINAPI *)(void))GetProcAddress(hShellExtDll, ISREGISTER_PROC);
	SetMenuFlagsProc	= (BOOL (WINAPI *)(int))GetProcAddress(hShellExtDll, SETMENUFLAGS_PROC);
	GetMenuFlagsProc	= (int (WINAPI *)(void))GetProcAddress(hShellExtDll, GETMENUFLAGS_PROC);
	UpdateDllProc		= (BOOL (WINAPI *)(void))GetProcAddress(hShellExtDll, UPDATEDLL_PROC);

	if (!RegisterDllProc || !UnRegisterDllProc || !IsRegisterDllProc
	|| !SetMenuFlagsProc || !GetMenuFlagsProc || !UpdateDllProc) {
		::FreeLibrary(hShellExtDll);
		hShellExtDll = NULL;
		return	FALSE;
	}
	return	TRUE;
}

BOOL ShellExt::UnLoad(void)
{
	if (hShellExtDll) {
		::FreeLibrary(hShellExtDll);
		hShellExtDll = NULL;
	}
	return	TRUE;
}

/*
	ShellExt Dialog初期化処理
*/
TShellExtDlg::TShellExtDlg(Cfg *_cfg, TWin *_parent) : TDlg(SHELLEXT_DIALOG, _parent)
{
	cfg = _cfg;
}

/*
	Window 生成時の CallBack
*/
BOOL TShellExtDlg::EvCreate(LPARAM lParam)
{
	if (!shellExt.Load(cfg->execDirV,
			IS_WINNT_V ? (void *)AtoW(CURRENT_SHEXTDLL) : CURRENT_SHEXTDLL)) {
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
	else
		MoveWindow(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, FALSE);

	return	TRUE;
}

BOOL TShellExtDlg::ReflectStatus(void)
{
	BOOL	isRegister = shellExt.IsRegisterDllProc();
	int		flags;

	SetDlgItemText(IDSHELLEXT_OK, isRegister ?
		GetLoadStr(IDS_SHELLEXT_MODIFY) : GetLoadStr(IDS_SHELLEXT_EXEC));
	::EnableWindow(GetDlgItem(IDSHELLEXT_CANCEL), isRegister);

	if ((flags = shellExt.GetMenuFlagsProc()) == -1) {
		flags = (SHEXT_RIGHT_COPY|SHEXT_RIGHT_DELETE|SHEXT_DD_COPY|SHEXT_DD_MOVE);
		// for old shellext
	}
	if ((flags & SHEXT_MENUFLG_EX) == 0) {	// old shellext
		::EnableWindow(GetDlgItem(RIGHT_PASTE_CHECK), FALSE);
	}

	if (flags & SHEXT_ISSTOREOPT) {
		cfg->shextNoConfirm		= (flags & SHEXT_NOCONFIRM) ? TRUE : FALSE;
		cfg->shextNoConfirmDel	= (flags & SHEXT_NOCONFIRMDEL) ? TRUE : FALSE;
		cfg->shextTaskTray		= (flags & SHEXT_TASKTRAY) ? TRUE : FALSE;
		cfg->shextAutoClose		= (flags & SHEXT_AUTOCLOSE) ? TRUE : FALSE;
	}

	CheckDlgButton(RIGHT_COPY_CHECK, flags & SHEXT_RIGHT_COPY);
	CheckDlgButton(RIGHT_DELETE_CHECK, flags & SHEXT_RIGHT_DELETE);
	CheckDlgButton(RIGHT_PASTE_CHECK, flags & SHEXT_RIGHT_PASTE);
	CheckDlgButton(RIGHT_SUBMENU_CHECK, flags & SHEXT_SUBMENU_RIGHT);
	CheckDlgButton(DD_COPY_CHECK, flags & SHEXT_DD_COPY);
	CheckDlgButton(DD_MOVE_CHECK, flags & SHEXT_DD_MOVE);
	CheckDlgButton(DD_SUBMENU_CHECK, flags & SHEXT_SUBMENU_DD);

	CheckDlgButton(NOCONFIRM_CHECK, cfg->shextNoConfirm);
	CheckDlgButton(NOCONFIRMDEL_CHECK, cfg->shextNoConfirmDel);
	CheckDlgButton(TASKTRAY_CHECK, cfg->shextTaskTray);
	CheckDlgButton(AUTOCLOSE_CHECK, cfg->shextAutoClose);

	return	TRUE;
}

BOOL TShellExtDlg::EvNcDestroy(void)
{
	if (shellExt.Status())
		shellExt.UnLoad();
	return	TRUE;
}

BOOL TShellExtDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDSHELLEXT_OK: case IDSHELLEXT_CANCEL:
		if (IsWinXP() && !TIsUserAnAdmin()) {
			TMsgBox(this).Exec(GetLoadStr(IDS_REQUIRE_ADMIN));
			return	TRUE;
		}
		if (RegisterShellExt(wID == IDSHELLEXT_OK ? TRUE : FALSE) == FALSE) {
			TMsgBox(this).Exec("ShellExt Error");
		}
		ReflectStatus();
		if (wID == IDSHELLEXT_OK)
			EndDialog(wID);
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

	cfg->shextNoConfirm    = is_register && IsDlgButtonChecked(NOCONFIRM_CHECK);
	cfg->shextNoConfirmDel = is_register && IsDlgButtonChecked(NOCONFIRMDEL_CHECK);
	cfg->shextTaskTray     = is_register && IsDlgButtonChecked(TASKTRAY_CHECK);
	cfg->shextAutoClose    = IsDlgButtonChecked(AUTOCLOSE_CHECK);
	cfg->WriteIni();

#ifdef _WIN64
	if (IS_WINNT_V) {
#else
	if (IS_WINNT_V && TIsWow64()) {
#endif
		WCHAR	arg[1024];
		Wstr	cur_shell_ex(CURRENT_SHEXTDLL_EX);
		Wstr	reg_proc(REGISTER_PROC);
		Wstr	unreg_proc(UNREGISTER_PROC);

		sprintfV(arg, L"\"%s\\%s\",%s", cfg->execDirV, cur_shell_ex.Buf(),
			is_register ? reg_proc.Buf() : unreg_proc.Buf());

		SHELLEXECUTEINFOW	sei = { sizeof(sei) };

		sei.lpFile = L"rundll32.exe";
		sei.lpParameters = arg;
		::ShellExecuteExW(&sei);
	}

	if (!is_register)
		return	shellExt.UnRegisterDllProc() == S_OK ? TRUE : FALSE;

	int		flags = SHEXT_MENU_DEFAULT;

	if ((shellExt.GetMenuFlagsProc() & SHEXT_MENUFLG_EX)) {
		flags |= SHEXT_ISSTOREOPT;
		if (cfg->shextNoConfirm)	flags |=  SHEXT_NOCONFIRM;
		else						flags &= ~SHEXT_NOCONFIRM;
		if (cfg->shextNoConfirmDel)	flags |=  SHEXT_NOCONFIRMDEL;
		else						flags &= ~SHEXT_NOCONFIRMDEL;
		if (cfg->shextTaskTray)		flags |=  SHEXT_TASKTRAY;
		else						flags &= ~SHEXT_TASKTRAY;
		if (cfg->shextAutoClose)	flags |=  SHEXT_AUTOCLOSE;
		else						flags &= ~SHEXT_AUTOCLOSE;
	}
	else {	// for old shell ext
		flags &= ~SHEXT_MENUFLG_EX;
	}

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

	if (shellExt.RegisterDllProc() != S_OK)
		return	FALSE;

	return	shellExt.SetMenuFlagsProc(flags);
}

int TExecConfirmDlg::Exec(const void *_src, const void *_dst)
{
	src = _src;
	dst = _dst;

	return	TDlg::Exec();
}

BOOL TExecConfirmDlg::EvCreate(LPARAM lParam)
{
	if (title) SetWindowTextV(title);

	SendDlgItemMessage(MESSAGE_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)EditWordBreakProc);
	SetDlgItemTextV(SRC_EDIT, src);
	if (dst) SetDlgItemTextV(DST_EDIT, dst);

	if (rect.left == CW_USEDEFAULT) {
		GetWindowRect(&rect);
		rect.left += 30, rect.right += 30;
		rect.top += 30, rect.bottom += 30;
		MoveWindow(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, FALSE);
	}

	SetDlgItem(SRC_EDIT, XY_FIT);
	if (resId == COPYCONFIRM_DIALOG) SetDlgItem(DST_STATIC, LEFT_FIT|BOTTOM_FIT);
	if (resId == COPYCONFIRM_DIALOG) SetDlgItem(DST_EDIT, X_FIT|BOTTOM_FIT);
//	if (resId == COPYCONFIRM_DIALOG)
//		SetDlgItem(HELP_CONFIRM_BUTTON, LEFT_FIT|BOTTOM_FIT);
	if (resId == DELCONFIRM_DIALOG)  SetDlgItem(OWDEL_CHECK, LEFT_FIT|BOTTOM_FIT);
	SetDlgItem(IDOK, LEFT_FIT|BOTTOM_FIT);
	SetDlgItem(IDCANCEL, LEFT_FIT|BOTTOM_FIT);

	if (isShellExt && IsWinVista() && !TIsUserAnAdmin()) {
		HWND	hRunas = GetDlgItem(RUNAS_BUTTON);
		::SetWindowLong(hRunas, GWL_STYLE, ::GetWindowLong(hRunas, GWL_STYLE) | WS_VISIBLE);
		::SendMessage(hRunas, BCM_SETSHIELD, 0, 1);
		SetDlgItem(RUNAS_BUTTON, LEFT_FIT|BOTTOM_FIT);
	}

	Show();
	if (info->mode == FastCopy::DELETE_MODE) {
		CheckDlgButton(OWDEL_CHECK,
			(info->flags & (FastCopy::OVERWRITE_DELETE|FastCopy::OVERWRITE_DELETE_NSA)) != 0);
	}
	SetForceForegroundWindow();

	GetWindowRect(&orgRect);

	return	TRUE;
}

BOOL TExecConfirmDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case HELP_CONFIRM_BUTTON:
		ShowHelpV(hWnd, cfg->execDirV, GetLoadStrV(IDS_FASTCOPYHELP), toV("#shellcancel"));
		return	TRUE;

	case IDOK: case IDCANCEL: case RUNAS_BUTTON:
		EndDialog(wID);
		return	TRUE;

	case OWDEL_CHECK:
		{
			parent->CheckDlgButton(OWDEL_CHECK, IsDlgButtonChecked(OWDEL_CHECK));
			FastCopy::Flags	flags = cfg->enableNSA ?
				FastCopy::OVERWRITE_DELETE_NSA : FastCopy::OVERWRITE_DELETE;
			if (IsDlgButtonChecked(OWDEL_CHECK))
				info->flags |= flags;
			else
				info->flags &= ~(DWORD)flags;
		}
		return	TRUE;
	}
	return	FALSE;
}

BOOL TExecConfirmDlg::EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight)
{
	if (fwSizeType == SIZE_RESTORED || fwSizeType == SIZE_MAXIMIZED)
		return	FitDlgItems();
	return	FALSE;;
}


/*=========================================================================
  クラス ： BrowseDirDlgV
  概  要 ： ディレクトリブラウズ用コモンダイアログ拡張クラス
  説  明 ： SHBrowseForFolder のサブクラス化
  注  意 ： 
=========================================================================*/
BOOL BrowseDirDlgV(TWin *parentWin, UINT editCtl, void *title, int flg)
{
	if (SHBrowseForFolderV == NULL || SHGetPathFromIDListV == NULL)	// old version NT kernel.
		return	FALSE;

	WCHAR		fileBuf[MAX_PATH_EX] = L"", buf[MAX_PATH_EX] = L"";
	BOOL		ret = FALSE;
	void		*c_root_v = IS_WINNT_V ? (void *)L"C:\\" : (void *)"C:\\";
	PathArray	pathArray;

	parentWin->GetDlgItemTextV(editCtl, fileBuf, MAX_PATH_EX);
	pathArray.RegisterMultiPath(fileBuf);

	if (pathArray.Num() > 0) {
		strcpyV(fileBuf, pathArray.Path(0));
	}
	else {
		strcpyV(fileBuf, c_root_v);
	}

	DirFileMode mode = DIRSELECT;
	TBrowseDirDlgV	dirDlg(title, fileBuf, flg, parentWin);
	TOpenFileDlg	fileDlg(parentWin, TOpenFileDlg::MULTI_OPEN, OFDLG_DIRSELECT);

	while (mode != SELECT_EXIT) {
		switch (mode) {
		case DIRSELECT:
			if (dirDlg.Exec()) {
				if (flg & BRDIR_BACKSLASH) {
					MakePathV(buf, fileBuf, EMPTY_STR_V);
					strcpyV(fileBuf, buf);
				}
				if (flg & BRDIR_MULTIPATH) {
					if ((flg & BRDIR_CTRLADD) == 0 ||
						(::GetAsyncKeyState(VK_CONTROL) & 0x8000) == 0) {
						pathArray.Init();
					}
					pathArray.RegisterPath(fileBuf);
					pathArray.GetMultiPath(fileBuf, MAX_PATH_EX);
				}
				::SetDlgItemTextV(parentWin->hWnd, editCtl, fileBuf);
				ret = TRUE;
			}
			else if (dirDlg.GetMode() == FILESELECT) {
				mode = FILESELECT;
				continue;
			}
			mode = SELECT_EXIT;
			break;

		case FILESELECT:
			SetChar(buf, sprintfV(buf, FMT_STR_V, GetLoadStrV(IDS_ALLFILES_FILTER)) + 1, 0);
			fileDlg.Exec(editCtl, NULL, buf, fileBuf, fileBuf);
			if (fileDlg.GetMode() == DIRSELECT)
				mode = DIRSELECT;
			else
				mode = SELECT_EXIT;
			break;
		}
	}

	return	ret;
}

TBrowseDirDlgV::TBrowseDirDlgV(void *title, void *_fileBuf, int _flg, TWin *parentWin)
{
	fileBuf = _fileBuf;
	flg = _flg;

	iMalloc = NULL;
	SHGetMalloc(&iMalloc);

	brInfo.hwndOwner = parentWin->hWnd;
	brInfo.pidlRoot = 0;
	brInfo.pszDisplayName = (char *)fileBuf;
	brInfo.lpszTitle = (char *)title;
	brInfo.ulFlags = BIF_RETURNONLYFSDIRS|BIF_USENEWUI|BIF_UAHINT|BIF_RETURNFSANCESTORS
					 /*|BIF_SHAREABLE*/;
	brInfo.lpfn = TBrowseDirDlgV::BrowseDirDlg_Proc;
	brInfo.lParam = (LPARAM)this;
	brInfo.iImage = 0;
}

TBrowseDirDlgV::~TBrowseDirDlgV()
{
	if (iMalloc)
		iMalloc->Release();
}

BOOL TBrowseDirDlgV::Exec()
{
	LPITEMIDLIST	pidlBrowse;
	BOOL	ret = FALSE;

	do {
		mode = DIRSELECT;
		if ((pidlBrowse = ::SHBrowseForFolderV(&brInfo)) != NULL) {
			if (::SHGetPathFromIDListV(pidlBrowse, fileBuf))
				ret = TRUE;
			iMalloc->Free(pidlBrowse);
			return	ret;
		}
	} while (mode == RELOAD);

	return	ret;
}

/*
	BrowseDirDlg用コールバック
*/
int CALLBACK TBrowseDirDlgV::BrowseDirDlg_Proc(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM data)
{
	switch (uMsg)
	{
	case BFFM_INITIALIZED:
		((TBrowseDirDlgV *)data)->AttachWnd(hWnd);
		break;

	case BFFM_SELCHANGED:
		if (((TBrowseDirDlgV *)data)->hWnd)
			((TBrowseDirDlgV *)data)->SetFileBuf(lParam);
		break;
	}
	return 0;
}

/*
	BrowseDlg用サブクラス生成
*/
BOOL TBrowseDirDlgV::AttachWnd(HWND _hWnd)
{
	BOOL	ret = TSubClass::AttachWnd(_hWnd);

// ディレクトリ設定
	DWORD	attr = GetFileAttributesV(fileBuf);
	if (attr != 0xffffffff && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0)
		GetParentDirV(fileBuf, fileBuf);

	if (ILCreateFromPathV) {	// 2000/XP
		LPITEMIDLIST pidl = ILCreateFromPathV(fileBuf);
		SendMessageV(BFFM_SETSELECTION, FALSE, (LPARAM)pidl);
		ILFreeV(pidl);
	}
	else {
		char	buf[MAX_PATH_EX], *target;
		if (IS_WINNT_V)			// NT4.0
			WtoA((WCHAR *)fileBuf, target=buf, sizeof(buf));
		else					// Win98
			target = (char *)fileBuf;
		SendMessageV(BFFM_SETSELECTION, TRUE, (LPARAM)target);
	}

// ボタン作成
	RECT	tmp_rect;
	::GetWindowRect(GetDlgItem(IDOK), &tmp_rect);
	POINT	pt = { tmp_rect.left, tmp_rect.top };
	::ScreenToClient(hWnd, &pt);
	int		cx = (pt.x - 30) / 2, cy = tmp_rect.bottom - tmp_rect.top;

//	::CreateWindow(BUTTON_CLASS, GetLoadStr(IDS_MKDIR), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
//		10, pt.y, cx, cy, hWnd, (HMENU)MKDIR_BUTTON, TApp::GetInstance(), NULL);
//	::CreateWindow(BUTTON_CLASS, GetLoadStr(IDS_RMDIR), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
//		18 + cx, pt.y, cx, cy, hWnd, (HMENU)RMDIR_BUTTON, TApp::GetInstance(), NULL);

	if (flg & BRDIR_FILESELECT) {
		GetClientRect(hWnd, &rect);
		int		file_cx = cx * 3 / 2;
		::CreateWindow(BUTTON_CLASS, GetLoadStr(IDS_FILESELECT),
			WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, rect.right - file_cx - 18, 10, file_cx, cy,
			hWnd, (HMENU)FILESELECT_BUTTON, TApp::GetInstance(), NULL);
	}

	HFONT	hDlgFont = (HFONT)SendDlgItemMessage(IDOK, WM_GETFONT, 0, 0L);
	if (hDlgFont)
	{
//		SendDlgItemMessage(MKDIR_BUTTON, WM_SETFONT, (UINT)hDlgFont, 0L);
//		SendDlgItemMessage(RMDIR_BUTTON, WM_SETFONT, (UINT)hDlgFont, 0L);
		if (flg & BRDIR_FILESELECT) {
			SendDlgItemMessage(FILESELECT_BUTTON, WM_SETFONT, (UINT)hDlgFont, 0L);
		}
	}

	return	ret;
}

/*
	BrowseDlg用 WM_COMMAND 処理
*/
BOOL TBrowseDirDlgV::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID)
	{
	case MKDIR_BUTTON:
		{
			WCHAR	dirBuf[MAX_PATH_EX], path[MAX_PATH_EX];
			TInputDlgV	dlg(dirBuf, this);
			if (dlg.Exec() == FALSE)
				return	TRUE;
			MakePathV(path, fileBuf, dirBuf);
			if (::CreateDirectoryV(path, NULL))
			{
				strcpyV(fileBuf, path);
				mode = RELOAD;
				PostMessage(WM_CLOSE, 0, 0);
			}
		}
		return	TRUE;

	case RMDIR_BUTTON:
		if (::RemoveDirectoryV(fileBuf))
		{
			GetParentDirV(fileBuf, fileBuf);
			mode = RELOAD;
			PostMessage(WM_CLOSE, 0, 0);
		}
		return	TRUE;

	case FILESELECT_BUTTON:
		mode = FILESELECT;
		PostMessage(WM_CLOSE, 0, 0);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TBrowseDirDlgV::SetFileBuf(LPARAM list)
{
	return	::SHGetPathFromIDListV((LPITEMIDLIST)list, fileBuf);
}

/*
	親ディレクトリ取得（必ずフルパスであること。UNC対応）
*/
BOOL TBrowseDirDlgV::GetParentDirV(void *srcfile, void *dir)
{
	WCHAR	path[MAX_PATH_EX], *fname=NULL;

	if (::GetFullPathNameV(srcfile, MAX_PATH_EX, path, (void **)&fname) == 0 || fname == NULL)
		return	strcpyV(dir, srcfile), FALSE;

	if (((char *)fname - (char *)path) / CHAR_LEN_V > 3 || GetChar(path, 1) != ':')
		SetChar(fname, -1, 0);
	else
		SetChar(fname, 0, 0);		// C:\ の場合

	strcpyV(dir, path);
	return	TRUE;
}


/*=========================================================================
  クラス ： TInputDlgV
  概  要 ： １行入力ダイアログ
  説  明 ： 
  注  意 ： 
=========================================================================*/
BOOL TInputDlgV::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID)
	{
	case IDOK:
		GetDlgItemTextV(INPUT_EDIT, dirBuf, MAX_PATH_EX);
		EndDialog(wID);
		return	TRUE;

	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;
	}
	return	FALSE;
}

int TConfirmDlg::Exec(const void *_message, BOOL _allow_continue, TWin *_parent)
{
	message = _message;
	parent = _parent;
	allow_continue = _allow_continue;
	return	TDlg::Exec();
}

BOOL TConfirmDlg::EvCreate(LPARAM lParam)
{
	if (!allow_continue) {
		SetWindowText(GetLoadStr(IDS_ERRSTOP));
		::EnableWindow(GetDlgItem(IDOK), FALSE);
		::EnableWindow(GetDlgItem(IDIGNORE), FALSE);
		::SetFocus(GetDlgItem(IDCANCEL));
	}
	SendDlgItemMessage(MESSAGE_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)EditWordBreakProc);
	SetDlgItemTextV(MESSAGE_EDIT, message);

	if (rect.left == CW_USEDEFAULT) {
		GetWindowRect(&rect);
		rect.left += 30, rect.right += 30;
		rect.top += 30, rect.bottom += 30;
		MoveWindow(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, FALSE);
	}

	Show();
	SetForceForegroundWindow();
	return	TRUE;
}

/*
	ファイルダイアログ用汎用ルーチン
*/
BOOL TOpenFileDlg::Exec(UINT editCtl, void *title, void *filter, void *defaultDir,
	void *init_data)
{
#define	MAX_OFNBUF	(MAX_WPATH * 4)
	WCHAR		buf[MAX_OFNBUF];
	PathArray	pathArray;

	if (parent == NULL)
		return FALSE;

	parent->GetDlgItemTextV(editCtl, buf, MAX_OFNBUF);
	pathArray.RegisterMultiPath(buf);

	if (init_data) {
		strcpyV(buf, init_data);
	}
	else if (pathArray.Num() > 0) {
		strcpyV(buf, pathArray.Path(0));
	}

	if (Exec(buf, title, filter, defaultDir) == FALSE)
		return	FALSE;

	if ((::GetAsyncKeyState(VK_CONTROL) & 0x8000) == 0) {
		pathArray.Init();
	}

	if (defaultDir) {
		int			dir_len = strlenV(defaultDir), offset = dir_len + 1;
		if (GetChar(buf, offset)) {  // 複数ファイル
			if (GetChar(buf, strlenV(buf) -1) != '\\')	// ドライブルートのみ例外
				SetChar(buf, dir_len++, '\\');
			for (; GetChar(buf, offset); offset++) {
				offset += sprintfV(MakeAddr(buf, dir_len), FMT_STR_V, MakeAddr(buf, offset));
				pathArray.RegisterPath(buf);
			}
			if (pathArray.GetMultiPath(buf, MAX_OFNBUF) >= 0) {
				parent->SetDlgItemTextV(editCtl, buf);
				return	TRUE;
			}
			else return	MessageBox("Too Many files...\n"), FALSE;
		}
	}
	pathArray.RegisterPath(buf);
	if (flg & OFDLG_WITHQUOTE) {
		pathArray.GetMultiPath(buf, MAX_OFNBUF, SEMICOLON_V, L" ");
	}
	else {
		pathArray.GetMultiPath(buf, MAX_OFNBUF);
	}
	parent->SetDlgItemTextV(editCtl, buf);
	return	TRUE;
}

#ifndef OFN_ENABLESIZING
#define OFN_ENABLESIZING 0x00800000
#endif

BOOL TOpenFileDlg::Exec(void *target, void *title, void *filter, void *defaultDir)
{
	OPENFILENAMEW	ofn;
	WCHAR	szDirName[MAX_PATH] = L"", szFile[MAX_WPATH] = L"";
	void	*fname = NULL;

	mode = FILESELECT;

	if (GetChar(target, 0)) {
		DWORD	attr = ::GetFileAttributesV(target);
		if (attr != 0xffffffff && (attr & FILE_ATTRIBUTE_DIRECTORY))
			strcpyV(szDirName, target);
		else if (::GetFullPathNameV(target, MAX_PATH, szDirName, &fname) && fname) {
			SetChar(fname, -1, 0);
		}
	}
	if (GetChar(szDirName, 0) == 0 && defaultDir)
		strcpyV(szDirName, defaultDir);
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = parent ? parent->hWnd : NULL;
	ofn.lpstrFilter = (WCHAR *)filter;
	ofn.nFilterIndex = filter ? 1 : 0;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = MAX_WPATH;
	ofn.lpstrTitle = (WCHAR *)title;
	ofn.lpstrInitialDir = szDirName;
	ofn.lCustData = (LPARAM)this;
	ofn.lpfnHook = hook ? hook : (LPOFNHOOKPROC)OpenFileDlgProc;
	ofn.Flags = OFN_HIDEREADONLY|OFN_EXPLORER|OFN_ENABLEHOOK|OFN_ENABLESIZING;
	if (openMode == OPEN || openMode == MULTI_OPEN)
		ofn.Flags |= OFN_FILEMUSTEXIST | (openMode == MULTI_OPEN ? OFN_ALLOWMULTISELECT : 0);
	else
		ofn.Flags |= (openMode == NODEREF_SAVE ? OFN_NODEREFERENCELINKS : 0);

	WCHAR	orgDir[MAX_PATH];
	::GetCurrentDirectoryV(MAX_PATH, orgDir);

	BOOL	ret = (openMode == OPEN || openMode == MULTI_OPEN) ?
				 ::GetOpenFileNameV(&ofn) : ::GetSaveFileNameV(&ofn);

	::SetCurrentDirectoryV(orgDir);
	if (ret) {
		if (openMode == MULTI_OPEN)
			memcpy(target, szFile, MAX_WPATH * CHAR_LEN_V);
		else
			strcpyV(target, ofn.lpstrFile);

		if (defaultDir)
			strcpyV(defaultDir, ofn.lpstrFile);
	}

	return	ret;
}

UINT WINAPI TOpenFileDlg::OpenFileDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
		((TOpenFileDlg *)(((OPENFILENAMEW *)lParam)->lCustData))->AttachWnd(hdlg);
		return TRUE;
	}
	return FALSE;
}

/*
	TOpenFileDlg用サブクラス生成
*/
BOOL TOpenFileDlg::AttachWnd(HWND _hWnd)
{
	BOOL	ret = TSubClass::AttachWnd(::GetParent(_hWnd));

	if ((flg & OFDLG_DIRSELECT) == 0)
		return	ret;

// ボタン作成
	RECT	ok_rect;

	::GetWindowRect(GetDlgItem(IDOK), &ok_rect);
	GetWindowRect(&rect);

	int	cx = (ok_rect.right - ok_rect.left) * 2;
	int	cy = ok_rect.bottom - ok_rect.top;
	int	x = ::GetSystemMetrics(SM_CXDLGFRAME);
	int	y = ok_rect.top - rect.top - ::GetSystemMetrics(SM_CYDLGFRAME) + cy * 4 / 3;
	int parent_cy = cy + y + 45 + ::GetSystemMetrics(SM_CYDLGFRAME);

	if (parent_cy > rect.bottom - rect.top) {
		MoveWindow(rect.left, rect.top, rect.right - rect.left, parent_cy, TRUE);
	}

	::CreateWindow(BUTTON_CLASS, GetLoadStr(IDS_DIRSELECT), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
		x, y, cx, cy, hWnd, (HMENU)DIRSELECT_BUTTON, TApp::GetInstance(), NULL);

	HFONT	hDlgFont = (HFONT)SendDlgItemMessage(IDOK, WM_GETFONT, 0, 0L);
	if (hDlgFont)
		SendDlgItemMessage(DIRSELECT_BUTTON, WM_SETFONT, (UINT)hDlgFont, 0L);

	return	ret;
}

/*
	BrowseDlg用 WM_COMMAND 処理
*/
BOOL TOpenFileDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID)
	{
	case DIRSELECT_BUTTON:
		mode = DIRSELECT;
		PostMessage(WM_CLOSE, 0, 0);
		return	TRUE;
	}
	return	FALSE;
}


/*
	Job Dialog初期化処理
*/
TJobDlg::TJobDlg(Cfg *_cfg, TMainDlg *_parent) : TDlg(JOB_DIALOG, _parent)
{
	cfg = _cfg;
	mainParent = _parent;
}

/*
	Window 生成時の CallBack
*/
BOOL TJobDlg::EvCreate(LPARAM lParam)
{
	WCHAR	buf[MAX_PATH];

	if (rect.left == CW_USEDEFAULT)
	{
		GetWindowRect(&rect);
		int xsize = rect.right - rect.left;
		int	ysize = rect.bottom - rect.top;
		MoveWindow(rect.left + 30, rect.top + 50, xsize, ysize, FALSE);
	}

	for (int i=0; i < cfg->jobMax; i++)
		SendDlgItemMessageV(TITLE_COMBO, CB_ADDSTRING, 0, (LPARAM)cfg->jobArray[i]->title);

	if (mainParent->GetDlgItemTextV(JOBTITLE_STATIC, buf, MAX_PATH) > 0) {
		int idx = cfg->SearchJobV(buf);
		if (idx >= 0)
			SendDlgItemMessage(TITLE_COMBO, CB_SETCURSEL, idx, 0);
		else
			SetDlgItemTextV(TITLE_COMBO, buf);
	}
	::EnableWindow(GetDlgItem(JOBDEL_BUTTON), cfg->jobMax ? TRUE : FALSE);

	return	TRUE;
}

BOOL TJobDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:
		if (AddJob())
			EndDialog(wID);
		return	TRUE;

	case JOBDEL_BUTTON:
		DelJob();
		return	TRUE;

	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TJobDlg::AddJob()
{
	WCHAR	title[MAX_PATH];

	if (GetDlgItemTextV(TITLE_COMBO, title, MAX_PATH) <= 0)
		return	FALSE;

	int		src_len = (int)mainParent->SendDlgItemMessage(SRC_COMBO, WM_GETTEXTLENGTH, 0, 0) + 1;
	if (src_len >= MAX_HISTORY_BUF) {
		TMsgBox(this).Exec("Source is too long (max.8192 chars)");
		return	FALSE;
	}

	Job			job;
	int			idx = (int)mainParent->SendDlgItemMessage(MODE_COMBO, CB_GETCURSEL, 0, 0);
	CopyInfo	*info = mainParent->GetCopyInfo();
	void		*src = new WCHAR [src_len];
	WCHAR		dst[MAX_PATH_EX]=L"";
	WCHAR		inc[MAX_PATH]=L"", exc[MAX_PATH]=L"";
	WCHAR		from_date[MINI_BUF]=L"", to_date[MINI_BUF]=L"";
	WCHAR		min_size[MINI_BUF]=L"", max_size[MINI_BUF]=L"";

	job.bufSize = mainParent->GetDlgItemInt(BUFSIZE_EDIT);
	job.estimateMode = mainParent->IsDlgButtonChecked(ESTIMATE_CHECK);
	job.ignoreErr = mainParent->IsDlgButtonChecked(IGNORE_CHECK);
	job.enableOwdel = mainParent->IsDlgButtonChecked(OWDEL_CHECK);
	job.enableAcl = mainParent->IsDlgButtonChecked(ACL_CHECK);
	job.enableStream = mainParent->IsDlgButtonChecked(STREAM_CHECK);
	job.enableVerify = mainParent->IsDlgButtonChecked(VERIFY_CHECK);
	job.isFilter = mainParent->IsDlgButtonChecked(FILTER_CHECK);
	mainParent->GetDlgItemTextV(SRC_COMBO, src, src_len);

	if (info[idx].mode != FastCopy::DELETE_MODE) {
		mainParent->GetDlgItemTextV(DST_COMBO, dst, MAX_PATH_EX);
		HMENU	hMenu = ::GetSubMenu(::GetMenu(mainParent->hWnd), 2);
		if (::GetMenuState(hMenu, SAMEDISK_MENUITEM, MF_BYCOMMAND) & MF_CHECKED)
			job.diskMode = 1;
		else if (::GetMenuState(hMenu, DIFFDISK_MENUITEM, MF_BYCOMMAND) & MF_CHECKED)
			job.diskMode = 2;
	}

	if (job.isFilter) {
		mainParent->GetDlgItemTextV(INCLUDE_COMBO, inc, MAX_PATH);
		mainParent->GetDlgItemTextV(EXCLUDE_COMBO, exc, MAX_PATH);
		mainParent->GetDlgItemTextV(FROMDATE_COMBO, from_date, MAX_PATH);
		mainParent->GetDlgItemTextV(TODATE_COMBO, to_date, MAX_PATH);
		mainParent->GetDlgItemTextV(MINSIZE_COMBO, min_size, MAX_PATH);
		mainParent->GetDlgItemTextV(MAXSIZE_COMBO, max_size, MAX_PATH);
	}

	job.SetString(title, src, dst, info[idx].cmdline_name, inc, exc,
		from_date, to_date, min_size, max_size);
	if (cfg->AddJobV(&job)) {
		cfg->WriteIni();
		mainParent->SetDlgItemTextV(JOBTITLE_STATIC, title);
	}
	else TMsgBox(this).Exec("Add Job Error");

	delete [] src;
	return	TRUE;
}

BOOL TJobDlg::DelJob()
{
	WCHAR	buf[MAX_PATH], msg[MAX_PATH];

	if (GetDlgItemTextV(TITLE_COMBO, buf, MAX_PATH) > 0) {
		int idx = cfg->SearchJobV(buf);
		sprintfV(msg, GetLoadStrV(IDS_JOBNAME), buf);
		if (idx >= 0
				&& TMsgBox(this).ExecV(msg, GetLoadStrV(IDS_DELCONFIRM), MB_OKCANCEL) == IDOK) {
			cfg->DelJobV(buf);
			cfg->WriteIni();
			SendDlgItemMessage(TITLE_COMBO, CB_DELETESTRING, idx, 0);
			SendDlgItemMessage(TITLE_COMBO, CB_SETCURSEL,
				idx == 0 ? 0 : idx < cfg->jobMax ? idx : idx -1, 0);
		}
	}
	::EnableWindow(GetDlgItem(JOBDEL_BUTTON), cfg->jobMax ? TRUE : FALSE);
	return	TRUE;
}

/*
	終了処理設定ダイアログ
*/
TFinActDlg::TFinActDlg(Cfg *_cfg, TMainDlg *_parent) : TDlg(FINACTION_DIALOG, _parent)
{
	cfg = _cfg;
	mainParent = _parent;
}

BOOL TFinActDlg::EvCreate(LPARAM lParam)
{
	if (rect.left == CW_USEDEFAULT)
	{
		GetWindowRect(&rect);
		int xsize = rect.right - rect.left;
		int	ysize = rect.bottom - rect.top;
		MoveWindow(rect.left + 30, rect.top + 50, xsize, ysize, FALSE);
	}

	for (int i=0; i < cfg->finActMax; i++) {
		SendDlgItemMessageV(TITLE_COMBO, CB_INSERTSTRING, i, (LPARAM)cfg->finActArray[i]->title);
	}

	for (int i=0; i < 3; i++) {
		SendDlgItemMessageV(FACMD_COMBO, CB_INSERTSTRING, i,
			(LPARAM)GetLoadStrV(IDS_FACMD_ALWAYS + i));
	}

	Reflect(max(mainParent->GetFinActIdx(), 0));

	return	TRUE;
}

BOOL TFinActDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case ADD_BUTTON:
		AddFinAct();
//		EndDialog(wID);
		return	TRUE;

	case DEL_BUTTON:
		DelFinAct();
		return	TRUE;

	case TITLE_COMBO:
		if (wNotifyCode == CBN_SELCHANGE) {
			Reflect((int)SendDlgItemMessage(TITLE_COMBO, CB_GETCURSEL, 0, 0));
		}
		return	TRUE;

	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;

	case SOUND_BUTTON:
		TOpenFileDlg(this).Exec(SOUND_EDIT);
		return	TRUE;

	case PLAY_BUTTON:
		{
			WCHAR	path[MAX_PATH];
			if (GetDlgItemTextV(SOUND_EDIT, path, MAX_PATH) > 0) {
				PlaySoundV(path, 0, SND_FILENAME|SND_ASYNC);
			}
		}
		return	TRUE;

	case CMD_BUTTON:
		TOpenFileDlg(this, TOpenFileDlg::OPEN, OFDLG_WITHQUOTE).Exec(CMD_EDIT);
		return	TRUE;

	case SUSPEND_CHECK:
	case HIBERNATE_CHECK:
	case SHUTDOWN_CHECK:
		CheckDlgButton(SUSPEND_CHECK,
			wID == SUSPEND_CHECK   && IsDlgButtonChecked(SUSPEND_CHECK)   ? 1 : 0);
		CheckDlgButton(HIBERNATE_CHECK,
			wID == HIBERNATE_CHECK && IsDlgButtonChecked(HIBERNATE_CHECK) ? 1 : 0);
		CheckDlgButton(SHUTDOWN_CHECK,
			wID == SHUTDOWN_CHECK  && IsDlgButtonChecked(SHUTDOWN_CHECK)  ? 1 : 0);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TFinActDlg::Reflect(int idx)
{
	if (idx >= cfg->finActMax) return FALSE;

	FinAct *finAct = cfg->finActArray[idx];

	SetDlgItemTextV(TITLE_COMBO, finAct->title);
	SetDlgItemTextV(SOUND_EDIT, finAct->sound);
	SetDlgItemTextV(CMD_EDIT, finAct->command);

	char	shutdown_time[100] = "60";
	BOOL	is_stop = finAct->shutdownTime >= 0;

	if (is_stop) {
		sprintf(shutdown_time, "%d", finAct->shutdownTime);
	}
	SetDlgItemText(SHUTDOWNTIME_EDIT, shutdown_time);

	CheckDlgButton(SUSPEND_CHECK,     is_stop && (finAct->flags & FinAct::SUSPEND)      ? 1 : 0);
	CheckDlgButton(HIBERNATE_CHECK,   is_stop && (finAct->flags & FinAct::HIBERNATE)    ? 1 : 0);
	CheckDlgButton(SHUTDOWN_CHECK,    is_stop && (finAct->flags & FinAct::SHUTDOWN)     ? 1 : 0);
	CheckDlgButton(FORCE_CHECK,       is_stop && (finAct->flags & FinAct::FORCE)        ? 1 : 0);
	CheckDlgButton(SHUTDOWNERR_CHECK, is_stop && (finAct->flags & FinAct::ERR_SHUTDOWN) ? 1 : 0);

	CheckDlgButton(SOUNDERR_CHECK, (finAct->flags & FinAct::ERR_SOUND) ? 1 : 0);
	CheckDlgButton(WAITCMD_CHECK,  (finAct->flags & FinAct::WAIT_CMD)  ? 1 : 0);

	int	fidx =  (finAct->flags & FinAct::ERR_CMD) ?    2 : 
				(finAct->flags & FinAct::NORMAL_CMD) ? 1 : 0;
	SendDlgItemMessage(FACMD_COMBO, CB_SETCURSEL, fidx, 0);

	::EnableWindow(GetDlgItem(DEL_BUTTON), (finAct->flags & FinAct::BUILTIN) ? 0 : 1);

	return	TRUE;}

BOOL TFinActDlg::AddFinAct()
{
	FinAct	finAct;

	WCHAR	title[MAX_PATH];
	WCHAR	sound[MAX_PATH];
	WCHAR	command[MAX_PATH_EX];
	WCHAR	buf[MAX_PATH];

	if (GetDlgItemTextV(TITLE_COMBO, title, MAX_PATH) <= 0 || lstrcmpiV(title, FALSE_V) == 0)
		return	FALSE;

	int idx = cfg->SearchFinActV(title);

	GetDlgItemTextV(SOUND_EDIT, sound, MAX_PATH);
	GetDlgItemTextV(CMD_EDIT, command, MAX_PATH);
	finAct.SetString(title, sound, command);

	if (GetChar(sound, 0)) {
		finAct.flags |= IsDlgButtonChecked(SOUNDERR_CHECK) ? FinAct::ERR_SOUND : 0;
	}
	if (GetChar(command, 0)) {
		int	fidx = (int)SendDlgItemMessage(FACMD_COMBO, CB_GETCURSEL, 0, 0);
		finAct.flags |= (fidx == 1 ? FinAct::NORMAL_CMD : fidx == 2 ? FinAct::ERR_CMD : 0);
		finAct.flags |= IsDlgButtonChecked(WAITCMD_CHECK) ? FinAct::WAIT_CMD : 0;
	}

	if (idx >= 1 && (cfg->finActArray[idx]->flags & FinAct::BUILTIN)) {
		finAct.flags |= cfg->finActArray[idx]->flags &
			(FinAct::SUSPEND|FinAct::HIBERNATE|FinAct::SHUTDOWN);
	}
	else {
		finAct.flags |= IsDlgButtonChecked(SUSPEND_CHECK)   ? FinAct::SUSPEND   : 0;
		finAct.flags |= IsDlgButtonChecked(HIBERNATE_CHECK) ? FinAct::HIBERNATE : 0;
		finAct.flags |= IsDlgButtonChecked(SHUTDOWN_CHECK)  ? FinAct::SHUTDOWN  : 0;
	}

	if (finAct.flags & (FinAct::SUSPEND|FinAct::HIBERNATE|FinAct::SHUTDOWN)) {
		finAct.flags |= (IsDlgButtonChecked(FORCE_CHECK) ? FinAct::FORCE : 0);
		finAct.flags |= (IsDlgButtonChecked(SHUTDOWNERR_CHECK) ? FinAct::ERR_SHUTDOWN : 0);
		finAct.shutdownTime = 60;
		if (GetDlgItemTextV(SHUTDOWNTIME_EDIT, buf, MAX_PATH) >= 0) {
			finAct.shutdownTime = strtoulV(buf, 0, 10);
		}
	}

	if (cfg->AddFinActV(&finAct)) {
		cfg->WriteIni();
		if (SendDlgItemMessage(TITLE_COMBO, CB_GETCOUNT, 0, 0) < cfg->finActMax) {
			SendDlgItemMessageV(TITLE_COMBO, CB_INSERTSTRING, cfg->finActMax-1, (LPARAM)title);
		}
		Reflect(cfg->SearchFinActV(title));
	}
	else TMsgBox(this).Exec("Add FinAct Error");

	return	TRUE;
}

BOOL TFinActDlg::DelFinAct()
{
	WCHAR	buf[MAX_PATH], msg[MAX_PATH];

	if (GetDlgItemTextV(TITLE_COMBO, buf, MAX_PATH) > 0) {
		int idx = cfg->SearchFinActV(buf);
		sprintfV(msg, GetLoadStrV(IDS_FINACTNAME), buf);
		if (cfg->finActArray[idx]->flags & FinAct::BUILTIN) {
			MessageBox("Can't delete buit-in Action", "Error");
		}
		else if (TMsgBox(this).ExecV(msg, GetLoadStrV(IDS_DELCONFIRM), MB_OKCANCEL) == IDOK) {
			cfg->DelFinActV(buf);
			cfg->WriteIni();
			SendDlgItemMessage(TITLE_COMBO, CB_DELETESTRING, idx, 0);
			idx = idx == cfg->finActMax ? idx -1 : idx;
			SendDlgItemMessage(TITLE_COMBO, CB_SETCURSEL, idx, 0);
			Reflect(idx);
		}
	}
	return	TRUE;
}


/*
	Message Box
*/
TMsgBox::TMsgBox(TWin *_parent) : TDlg(MESSAGE_DIALOG, _parent)
{
	msg = title = NULL;
	style = 0;
	isExecV = FALSE;
}

TMsgBox::~TMsgBox()
{
}

/*
	Window 生成時の CallBack
*/
BOOL TMsgBox::EvCreate(LPARAM lParam)
{
	if (rect.left == CW_USEDEFAULT)
	{
		GetWindowRect(&rect);
		int xsize = rect.right - rect.left;
		int	ysize = rect.bottom - rect.top;
		MoveWindow(rect.left + 30, rect.top + 50, xsize, ysize, FALSE);
	}

	if (isExecV) {
		SetWindowTextV(title);
		SetDlgItemTextV(MESSAGE_EDIT, msg);
	}
	else {
		SetWindowText((const char *)title);
		SetDlgItemText(MESSAGE_EDIT, (const char *)msg);
	}

	if ((style & MB_OKCANCEL) == 0) {
		::ShowWindow(GetDlgItem(IDCANCEL), SW_HIDE);
		RECT	ok_rect;
		HWND	ok_wnd = GetDlgItem(IDOK);
		::GetWindowRect(ok_wnd, &ok_rect);
		POINT	pt = { ok_rect.left, ok_rect.top };
		::ScreenToClient(hWnd, &pt);
		::SetWindowPos(ok_wnd, 0, pt.x + 50, pt.y, 0, 0, SWP_NOSIZE|SWP_NOZORDER);
	}

	GetWindowRect(&orgRect);

	SetDlgItem(MESSAGE_EDIT,	XY_FIT);
	SetDlgItem(IDOK,			LEFT_FIT|BOTTOM_FIT);
	SetDlgItem(IDCANCEL,		LEFT_FIT|BOTTOM_FIT);

	return	TRUE;
}

BOOL TMsgBox::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TMsgBox::EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight)
{
	if (fwSizeType == SIZE_RESTORED || fwSizeType == SIZE_MAXIMIZED)
		return	FitDlgItems();
	return	FALSE;;
}

UINT TMsgBox::ExecV(void *_msg, void *_title, UINT _style)
{
	msg = _msg;
	title = _title;
	style = _style;
	isExecV = TRUE;

	return	TDlg::Exec();
}

UINT TMsgBox::Exec(char *_msg, char *_title, UINT _style)
{
	msg = _msg;
	title = _title;
	style = _style;
	isExecV = FALSE;

	return	TDlg::Exec();
}


TFinDlg::TFinDlg(TWin *_parent) : TMsgBox(_parent)
{
}

TFinDlg::~TFinDlg()
{
}

UINT TFinDlg::Exec(int _sec, DWORD fmtID)
{
	sec = _sec;
	fmt = GetLoadStr(fmtID);
	return	TMsgBox::Exec("", "FastCopy", MB_OKCANCEL);
}


/*
	Window 生成時の CallBack
*/
BOOL TFinDlg::EvCreate(LPARAM lParam)
{
	TMsgBox::EvCreate(lParam);
	UpdateText();
//	SetForceForegroundWindow();
	SetWindowPos(HWND_TOPMOST, 0, 0 ,0, 0, SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE);
	::SetTimer(hWnd, FASTCOPY_FIN_TIMER, 1000, NULL);
	return	TRUE;
}

void TFinDlg::UpdateText()
{
	char	buf[100];
	sprintf(buf, fmt, sec);
	SetDlgItemText(MESSAGE_EDIT, buf);
}

BOOL TFinDlg::EvTimer(WPARAM timerID, TIMERPROC proc)
{
	if (--sec >= 0) {
		UpdateText();
	}
	else {
		::KillTimer(hWnd, timerID);
		EndDialog(IDOK);
	}
	return	TRUE;
}

int SetSpeedLevelLabel(TDlg *dlg, int level)
{
	if (level == -1)
		level = (int)dlg->SendDlgItemMessage(SPEED_SLIDER, TBM_GETPOS, 0, 0);
	else
		dlg->SendDlgItemMessage(SPEED_SLIDER, TBM_SETPOS, TRUE, level);

	char	buf[64];
	sprintf(buf,
			level == SPEED_FULL ?		GetLoadStr(IDS_FULLSPEED_DISP) :
			level == SPEED_AUTO ?		GetLoadStr(IDS_AUTOSLOW_DISP) :
			level == SPEED_SUSPEND ?	GetLoadStr(IDS_SUSPEND_DISP) :
			 							GetLoadStr(IDS_RATE_DISP),
			level * 10);
	dlg->SetDlgItemText(SPEED_STATIC, buf);
	return	level;
}


TListHeader::TListHeader(TWin *_parent) : TSubClassCtl(_parent)
{
	memset(&logFont, 0, sizeof(logFont));
}

BOOL TListHeader::AttachWnd(HWND _hWnd)
{
	BOOL ret = TSubClassCtl::AttachWnd(_hWnd);
	ChangeFontNotify();
	return	ret;
}

BOOL TListHeader::EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == HDM_LAYOUT) {
		HD_LAYOUT *hl = (HD_LAYOUT *)lParam;
		::CallWindowProcW((WNDPROC)oldProc, hWnd, uMsg, wParam, lParam);
//		Debug("HDM_LAYOUT(USER)2 top:%d/bottom:%d diff:%d cy:%d y:%d\n",
//			hl->prc->top, hl->prc->bottom, hl->prc->bottom - hl->prc->top,
//			hl->pwpos->cy, hl->pwpos->y);

		if (logFont.lfHeight) {
			int	height = abs(logFont.lfHeight) + 4;
			hl->prc->top = height;
			hl->pwpos->cy = height;
		}
		return	TRUE;
	}
	return	FALSE;
}

BOOL TListHeader::ChangeFontNotify()
{
	HFONT	hFont;

	if ((hFont = (HFONT)SendMessage(WM_GETFONT, 0, 0)) == NULL)
		return	FALSE;

	if (::GetObject(hFont, sizeof(LOGFONT), (void *)&logFont) == 0)
		return	FALSE;

//	Debug("lfHeight=%d\n", logFont.lfHeight);
	return	TRUE;
}

/*
	listview control の subclass化
	Focus を失ったときにも、選択色を変化させないための小細工
*/
#define INVALID_INDEX	-1
TListViewEx::TListViewEx(TWin *_parent) : TSubClassCtl(_parent)
{
	focus_index = INVALID_INDEX;
}

BOOL TListViewEx::EventFocus(UINT uMsg, HWND hFocusWnd)
{
	LV_ITEM	lvi;

	memset(&lvi, 0, sizeof(lvi));
	lvi.mask = LVIF_STATE;
	lvi.stateMask = LVIS_FOCUSED;
	int	maxItem = (int)SendMessage(LVM_GETITEMCOUNT, 0, 0);
	int itemNo;

	for (itemNo=0; itemNo < maxItem; itemNo++) {
		if (SendMessage(LVM_GETITEMSTATE, itemNo, (LPARAM)LVIS_FOCUSED) & LVIS_FOCUSED)
			break;
	}

	if (uMsg == WM_SETFOCUS)
	{
		if (itemNo == maxItem) {
			lvi.state = LVIS_FOCUSED;
			if (focus_index == INVALID_INDEX)
				focus_index = 0;
			SendMessage(LVM_SETITEMSTATE, focus_index, (LPARAM)&lvi);
			SendMessage(LVM_SETSELECTIONMARK, 0, focus_index);
		}
		return	FALSE;
	}
	else {	// WM_KILLFOCUS
		if (itemNo != maxItem) {
			SendMessage(LVM_SETITEMSTATE, itemNo, (LPARAM)&lvi);
			focus_index = itemNo;
		}
		return	TRUE;	// WM_KILLFOCUS は伝えない
	}
}

BOOL TListViewEx::EventButton(UINT uMsg, int nHitTest, POINTS pos)
{
	switch (uMsg)
	{
	case WM_RBUTTONDOWN:
		return	TRUE;
	case WM_RBUTTONUP:
		::PostMessage(parent->hWnd, uMsg, nHitTest, *(LPARAM *)&pos);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TListViewEx::EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (focus_index == INVALID_INDEX)
		return	FALSE;

	switch (uMsg) {
	case LVM_INSERTITEM:
		if (((LV_ITEM *)lParam)->iItem <= focus_index)
			focus_index++;
		break;
	case LVM_DELETEITEM:
		if ((int)wParam == focus_index)
			focus_index = INVALID_INDEX;
		else if ((int)wParam < focus_index)
			focus_index--;
		break;
	case LVM_DELETEALLITEMS:
		focus_index = INVALID_INDEX;
		break;
	}
	return	FALSE;
}

/*
	edit control の subclass化
*/
TEditSub::TEditSub(TWin *_parent) : TSubClassCtl(_parent)
{
}

#define EM_SETEVENTMASK			(WM_USER + 69)
#define EM_GETEVENTMASK			(WM_USER + 59)
#define ENM_LINK				0x04000000

BOOL TEditSub::AttachWnd(HWND _hWnd)
{
	if (!TSubClassCtl::AttachWnd(_hWnd))
		return	FALSE;

//	DWORD	evMask = SendMessage(EM_GETEVENTMASK, 0, 0) | ENM_LINK;
//	SendMessage(EM_SETEVENTMASK, 0, evMask); 
//	dblClicked = FALSE;

	return	TRUE;
}

BOOL TEditSub::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case WM_UNDO:
	case WM_CUT:
	case WM_COPY:
	case WM_PASTE:
	case WM_CLEAR:
		SendMessage(wID, 0, 0);
		return	TRUE;

	case EM_SETSEL:
		SendMessage(wID, 0, -1);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TEditSub::EvContextMenu(HWND childWnd, POINTS pos)
{
	HMENU	hMenu = ::CreatePopupMenu();
	BOOL	is_readonly = (int)(GetWindowLong(GWL_STYLE) & ES_READONLY);

	AppendMenu(hMenu, MF_STRING|((is_readonly || !SendMessage(EM_CANUNDO, 0, 0)) ? MF_DISABLED|MF_GRAYED : 0), WM_UNDO, GetLoadStr(IDS_UNDO));
	AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
	AppendMenu(hMenu, MF_STRING|(is_readonly ? MF_DISABLED|MF_GRAYED : 0), WM_CUT, GetLoadStr(IDS_CUT));
	AppendMenu(hMenu, MF_STRING, WM_COPY, GetLoadStr(IDS_COPY));
	AppendMenu(hMenu, MF_STRING|((is_readonly || !SendMessage(EM_CANPASTE, 0, 0)) ? MF_DISABLED|MF_GRAYED : 0), WM_PASTE, GetLoadStr(IDS_PASTE));
	AppendMenu(hMenu, MF_STRING|(is_readonly ? MF_DISABLED|MF_GRAYED : 0), WM_CLEAR, GetLoadStr(IDS_DELETE));
	AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
	AppendMenu(hMenu, MF_STRING, EM_SETSEL, GetLoadStr(IDS_SELECTALL));

	::TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pos.x, pos.y, 0, hWnd, NULL);
	::DestroyMenu(hMenu);

	return	TRUE;
}

int TEditSub::ExGetText(void *buf, int max_len, DWORD flags, UINT codepage)
{
	GETTEXTEX	ge;

	memset(&ge, 0, sizeof(ge));
	ge.cb = max_len;
	ge.flags = flags;
	ge.codepage = codepage;

	return	(int)::SendMessageW(hWnd, EM_GETTEXTEX, (WPARAM)&ge, (LPARAM)buf);
}

int TEditSub::ExSetText(const void *buf, int max_len, DWORD flags, UINT codepage)
{
	SETTEXTEX	se;

	se.flags = flags;
	se.codepage = codepage;

	return	(int)::SendMessageW(hWnd, EM_SETTEXTEX, (WPARAM)&se, (LPARAM)buf);
}

