static char *miscdlg_id = 
	"@(#)Copyright (C) 2005-2015 H.Shirouzu		miscdlg.cpp	ver3.03";
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Create					: 2005-01-23(Sun)
	Update					: 2015-08-30(Sun)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
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
		::ShellExecuteW(NULL, NULL, GetLoadStrW(IDS_FASTCOPYURL), NULL, NULL, SW_SHOW);
		return	TRUE;
	}
	return	FALSE;
}

int TExecConfirmDlg::Exec(const WCHAR *_src, const WCHAR *_dst)
{
	src = _src;
	dst = _dst;

	return	TDlg::Exec();
}

BOOL TExecConfirmDlg::EvCreate(LPARAM lParam)
{
	if (title) SetWindowTextW(title);

	SendDlgItemMessageW(MESSAGE_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)EditWordBreakProcW);
	SetDlgItemTextW(SRC_EDIT, src);
	if (dst) SetDlgItemTextW(DST_EDIT, dst);

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

	if (isShellExt && IsWinVista() && !::IsUserAnAdmin()) {
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
		ShowHelpW(0, cfg->execDir, GetLoadStrW(IDS_FASTCOPYHELP), L"#shellcancel");
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
  クラス ： BrowseDirDlgW
  概  要 ： ディレクトリブラウズ用コモンダイアログ拡張クラス
  説  明 ： SHBrowseForFolder のサブクラス化
  注  意 ： 
=========================================================================*/
BOOL BrowseDirDlgW(TWin *parentWin, UINT editCtl, WCHAR *title, int flg)
{
	WCHAR		fileBuf[MAX_PATH_EX] = L"";
	WCHAR		buf[MAX_PATH_EX] = L"";
	BOOL		ret = FALSE;
	WCHAR		*c_root_v = L"C:\\";
	PathArray	pathArray;

	parentWin->GetDlgItemTextW(editCtl, fileBuf, MAX_PATH_EX);
	pathArray.RegisterMultiPath(fileBuf);

	if (pathArray.Num() > 0) {
		wcscpy(fileBuf, pathArray.Path(0));
	}
	else {
		wcscpy(fileBuf, c_root_v);
	}

	DirFileMode mode = DIRSELECT;
	TBrowseDirDlgW	dirDlg(title, fileBuf, flg, parentWin);
	TOpenFileDlg	fileDlg(parentWin, TOpenFileDlg::MULTI_OPEN, OFDLG_DIRSELECT);

	while (mode != SELECT_EXIT) {
		switch (mode) {
		case DIRSELECT:
			if (dirDlg.Exec()) {
				if (flg & BRDIR_BACKSLASH) {
					MakePathW(buf, fileBuf, L"");
					wcscpy(fileBuf, buf);
				}
				if (flg & BRDIR_MULTIPATH) {
					if ((flg & BRDIR_CTRLADD) == 0 ||
						(::GetAsyncKeyState(VK_CONTROL) & 0x8000) == 0) {
						pathArray.Init();
					}
					pathArray.RegisterPath(fileBuf);
					pathArray.GetMultiPath(fileBuf, MAX_PATH_EX);
				}
				parentWin->SetDlgItemTextW(editCtl, fileBuf);
				ret = TRUE;
			}
			else if (dirDlg.GetMode() == FILESELECT) {
				mode = FILESELECT;
				continue;
			}
			mode = SELECT_EXIT;
			break;

		case FILESELECT:
			buf[wcscpyz(buf, GetLoadStrW(IDS_ALLFILES_FILTER)) + 1] =  0;
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

TBrowseDirDlgW::TBrowseDirDlgW(WCHAR *title, WCHAR *_fileBuf, int _flg, TWin *parentWin)
{
	fileBuf = _fileBuf;
	flg = _flg;

	iMalloc = NULL;
	SHGetMalloc(&iMalloc);

	brInfo.hwndOwner = parentWin->hWnd;
	brInfo.pidlRoot = 0;
	brInfo.pszDisplayName = fileBuf;
	brInfo.lpszTitle = title;
	brInfo.ulFlags = BIF_RETURNONLYFSDIRS|BIF_USENEWUI|BIF_UAHINT|BIF_RETURNFSANCESTORS
					 /*|BIF_SHAREABLE*/;
	brInfo.lpfn = TBrowseDirDlgW::BrowseDirDlg_Proc;
	brInfo.lParam = (LPARAM)this;
	brInfo.iImage = 0;
}

TBrowseDirDlgW::~TBrowseDirDlgW()
{
	if (iMalloc)
		iMalloc->Release();
}

BOOL TBrowseDirDlgW::Exec()
{
	LPITEMIDLIST	pidlBrowse;
	BOOL			ret = FALSE;

	do {
		mode = DIRSELECT;
		if ((pidlBrowse = ::SHBrowseForFolderW(&brInfo)) != NULL) {
			if (::SHGetPathFromIDListW(pidlBrowse, fileBuf))
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
int CALLBACK TBrowseDirDlgW::BrowseDirDlg_Proc(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM data)
{
	switch (uMsg)
	{
	case BFFM_INITIALIZED:
		((TBrowseDirDlgW *)data)->AttachWnd(hWnd);
		break;

	case BFFM_SELCHANGED:
		if (((TBrowseDirDlgW *)data)->hWnd)
			((TBrowseDirDlgW *)data)->SetFileBuf(lParam);
		break;
	}
	return 0;
}

/*
	BrowseDlg用サブクラス生成
*/
BOOL TBrowseDirDlgW::AttachWnd(HWND _hWnd)
{
	BOOL	ret = TSubClass::AttachWnd(_hWnd);

// ディレクトリ設定
	DWORD	attr = ::GetFileAttributesW(fileBuf);
	if (attr != 0xffffffff && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0)
		GetParentDirW(fileBuf, fileBuf);

	LPITEMIDLIST pidl = ::ILCreateFromPathW(fileBuf);
	SendMessageW(BFFM_SETSELECTION, FALSE, (LPARAM)pidl);
	ILFree(pidl);

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
		GetClientRect(&rect);
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
BOOL TBrowseDirDlgW::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID)
	{
	case MKDIR_BUTTON:
		{
			WCHAR	dirBuf[MAX_PATH_EX], path[MAX_PATH_EX];
			TInputDlg	dlg(dirBuf, this);
			if (dlg.Exec() == FALSE)
				return	TRUE;
			MakePathW(path, fileBuf, dirBuf);
			if (::CreateDirectoryW(path, NULL))
			{
				wcscpy(fileBuf, path);
				mode = RELOAD;
				PostMessage(WM_CLOSE, 0, 0);
			}
		}
		return	TRUE;

	case RMDIR_BUTTON:
		if (::RemoveDirectoryW(fileBuf))
		{
			GetParentDirW(fileBuf, fileBuf);
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

BOOL TBrowseDirDlgW::SetFileBuf(LPARAM list)
{
	return	::SHGetPathFromIDListW((LPITEMIDLIST)list, fileBuf);
}

/*
	親ディレクトリ取得（必ずフルパスであること。UNC対応）
*/
BOOL TBrowseDirDlgW::GetParentDirW(WCHAR *srcfile, WCHAR *dir)
{
	WCHAR	path[MAX_PATH_EX], *fname=NULL;

	if (::GetFullPathNameW(srcfile, MAX_PATH_EX, path, &fname) == 0 || fname == NULL)
		return	wcscpy(dir, srcfile), FALSE;

	if (fname - path > 3 || path[1] != ':')
		fname[-1] = 0;
	else
		fname[0] = 0;		// C:\ の場合

	wcscpy(dir, path);
	return	TRUE;
}


/*=========================================================================
  クラス ： TInputDlg
  概  要 ： １行入力ダイアログ
  説  明 ： 
  注  意 ： 
=========================================================================*/
BOOL TInputDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID)
	{
	case IDOK:
		GetDlgItemTextW(INPUT_EDIT, dirBuf, MAX_PATH_EX);
		EndDialog(wID);
		return	TRUE;

	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;
	}
	return	FALSE;
}

int TConfirmDlg::Exec(const WCHAR *_message, BOOL _allow_continue, TWin *_parent)
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
	SendDlgItemMessageW(MESSAGE_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)EditWordBreakProcW);
	SetDlgItemTextW(MESSAGE_EDIT, message);

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
BOOL TOpenFileDlg::Exec(UINT editCtl, WCHAR *title, WCHAR *filter, WCHAR *defaultDir,
	WCHAR *init_data)
{
#define	MAX_OFNBUF	(MAX_WPATH * 4)
	VBuf		vbuf(MAX_OFNBUF * sizeof(WCHAR));
	WCHAR		*buf = vbuf.WBuf();
	PathArray	pathArray;

	if (parent == NULL)
		return FALSE;

	parent->GetDlgItemTextW(editCtl, buf, MAX_OFNBUF);
	pathArray.RegisterMultiPath(buf);

	if (init_data) {
		wcscpy(buf, init_data);
	}
	else if (pathArray.Num() > 0) {
		wcscpy(buf, pathArray.Path(0));
	}

	if (Exec(buf, title, filter, defaultDir) == FALSE)
		return	FALSE;

	if ((::GetAsyncKeyState(VK_CONTROL) & 0x8000) == 0) {
		pathArray.Init();
	}

	if (defaultDir) {
		int		dir_len = (int)wcslen(defaultDir);
		int		offset = dir_len + 1;
		if (buf[offset]) {  // 複数ファイル
			if (buf[wcslen(buf) -1] != '\\')	// ドライブルートのみ例外
				buf[dir_len++] = '\\';
			for (; buf[offset]; offset++) {
				offset += wcscpyz(buf + dir_len, buf + offset);
				pathArray.RegisterPath(buf);
			}
			if (pathArray.GetMultiPath(buf, MAX_OFNBUF) >= 0) {
				parent->SetDlgItemTextW(editCtl, buf);
				return	TRUE;
			}
			else return	MessageBox("Too Many files...\n"), FALSE;
		}
	}
	pathArray.RegisterPath(buf);
	if (flg & OFDLG_WITHQUOTE) {
		pathArray.GetMultiPath(buf, MAX_OFNBUF, L";", L" ");
	}
	else {
		pathArray.GetMultiPath(buf, MAX_OFNBUF);
	}
	parent->SetDlgItemTextW(editCtl, buf);
	return	TRUE;
}

#ifndef OFN_ENABLESIZING
#define OFN_ENABLESIZING 0x00800000
#endif

BOOL TOpenFileDlg::Exec(WCHAR *target, WCHAR *title, WCHAR *filter, WCHAR *defaultDir)
{
	OPENFILENAMEW	ofn;
	VBuf	vbuf(MAX_WPATH * sizeof(WCHAR));
	WCHAR	szDirName[MAX_PATH] = L"";
	WCHAR	*szFile = vbuf.WBuf();
	WCHAR	*fname = NULL;

	mode = FILESELECT;

	if (target[0]) {
		DWORD	attr = ::GetFileAttributesW(target);
		if (attr != 0xffffffff && (attr & FILE_ATTRIBUTE_DIRECTORY))
			wcscpy(szDirName, target);
		else if (::GetFullPathNameW(target, MAX_PATH, szDirName, &fname) && fname) {
			fname[-1] = 0;
		}
	}
	if (szDirName[0] == 0 && defaultDir)
		wcscpy(szDirName, defaultDir);
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
	::GetCurrentDirectoryW(MAX_PATH, orgDir);

	BOOL	ret = (openMode == OPEN || openMode == MULTI_OPEN) ?
				 ::GetOpenFileNameW(&ofn) : ::GetSaveFileNameW(&ofn);

	::SetCurrentDirectoryW(orgDir);
	if (ret) {
		if (openMode == MULTI_OPEN)
			memcpy(target, szFile, MAX_WPATH * sizeof(WCHAR));
		else
			wcscpy(target, ofn.lpstrFile);

		if (defaultDir)
			wcscpy(defaultDir, ofn.lpstrFile);
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
		SendDlgItemMessageW(TITLE_COMBO, CB_ADDSTRING, 0, (LPARAM)cfg->jobArray[i]->title);

	if (mainParent->GetDlgItemTextW(JOBTITLE_STATIC, buf, MAX_PATH) > 0) {
		int idx = cfg->SearchJobW(buf);
		if (idx >= 0)
			SendDlgItemMessage(TITLE_COMBO, CB_SETCURSEL, idx, 0);
		else
			SetDlgItemTextW(TITLE_COMBO, buf);
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

	case HELP_BUTTON:
		ShowHelpW(NULL, cfg->execDir, GetLoadStrW(IDS_FASTCOPYHELP), L"#job");
		return	TRUE;
	}
	return	FALSE;
}

BOOL TJobDlg::AddJob()
{
	WCHAR	title[MAX_PATH];

	if (GetDlgItemTextW(TITLE_COMBO, title, MAX_PATH) <= 0)
		return	FALSE;

	int		src_len = (int)mainParent->SendDlgItemMessage(SRC_COMBO, WM_GETTEXTLENGTH, 0, 0) + 1;
	if (src_len >= MAX_HISTORY_BUF) {
		TMsgBox(this).Exec("Source is too long (max.8192 chars)");
		return	FALSE;
	}

	Job			job;
	int			idx = (int)mainParent->SendDlgItemMessage(MODE_COMBO, CB_GETCURSEL, 0, 0);
	CopyInfo	*info = mainParent->GetCopyInfo();
	WCHAR		*src = new WCHAR [src_len];
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
	mainParent->GetDlgItemTextW(SRC_COMBO, src, src_len);

	if (info[idx].mode != FastCopy::DELETE_MODE) {
		mainParent->GetDlgItemTextW(DST_COMBO, dst, MAX_PATH_EX);
		HMENU	hMenu = ::GetSubMenu(::GetMenu(mainParent->hWnd), 2);
		if (::GetMenuState(hMenu, SAMEDISK_MENUITEM, MF_BYCOMMAND) & MF_CHECKED)
			job.diskMode = 1;
		else if (::GetMenuState(hMenu, DIFFDISK_MENUITEM, MF_BYCOMMAND) & MF_CHECKED)
			job.diskMode = 2;
	}

	if (job.isFilter) {
		mainParent->GetDlgItemTextW(INCLUDE_COMBO, inc, MAX_PATH);
		mainParent->GetDlgItemTextW(EXCLUDE_COMBO, exc, MAX_PATH);
		mainParent->GetDlgItemTextW(FROMDATE_COMBO, from_date, MAX_PATH);
		mainParent->GetDlgItemTextW(TODATE_COMBO, to_date, MAX_PATH);
		mainParent->GetDlgItemTextW(MINSIZE_COMBO, min_size, MAX_PATH);
		mainParent->GetDlgItemTextW(MAXSIZE_COMBO, max_size, MAX_PATH);
	}

	job.SetString(title, src, dst, info[idx].cmdline_name, inc, exc,
		from_date, to_date, min_size, max_size);
	if (cfg->AddJobW(&job)) {
		cfg->WriteIni();
		mainParent->SetDlgItemTextW(JOBTITLE_STATIC, title);
	}
	else TMsgBox(this).Exec("Add Job Error");

	delete [] src;
	return	TRUE;
}

BOOL TJobDlg::DelJob()
{
	WCHAR	buf[MAX_PATH], msg[MAX_PATH];

	if (GetDlgItemTextW(TITLE_COMBO, buf, MAX_PATH) > 0) {
		int idx = cfg->SearchJobW(buf);
		swprintf(msg, GetLoadStrW(IDS_JOBNAME), buf);
		if (idx >= 0
				&& TMsgBox(this).ExecW(msg, GetLoadStrW(IDS_DELCONFIRM), MB_OKCANCEL) == IDOK) {
			cfg->DelJobW(buf);
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
		SendDlgItemMessageW(TITLE_COMBO, CB_INSERTSTRING, i, (LPARAM)cfg->finActArray[i]->title);
	}

	for (int i=0; i < 3; i++) {
		SendDlgItemMessageW(FACMD_COMBO, CB_INSERTSTRING, i,
			(LPARAM)GetLoadStrW(IDS_FACMD_ALWAYS + i));
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
			if (GetDlgItemTextW(SOUND_EDIT, path, MAX_PATH) > 0) {
				PlaySoundW(path, 0, SND_FILENAME|SND_ASYNC);
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

	case HELP_BUTTON:
		ShowHelpW(NULL, cfg->execDir, GetLoadStrW(IDS_FASTCOPYHELP), L"#finact");
		return	TRUE;
	}
	return	FALSE;
}

BOOL TFinActDlg::Reflect(int idx)
{
	if (idx >= cfg->finActMax) return FALSE;

	FinAct *finAct = cfg->finActArray[idx];

	SetDlgItemTextW(TITLE_COMBO, finAct->title);
	SetDlgItemTextW(SOUND_EDIT, finAct->sound);
	SetDlgItemTextW(CMD_EDIT, finAct->command);

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

	if (GetDlgItemTextW(TITLE_COMBO, title, MAX_PATH) <= 0 || wcsicmp(title, L"FALSE") == 0)
		return	FALSE;

	int idx = cfg->SearchFinActW(title);

	GetDlgItemTextW(SOUND_EDIT, sound, MAX_PATH);
	GetDlgItemTextW(CMD_EDIT, command, MAX_PATH);
	finAct.SetString(title, sound, command);

	if (sound[0]) {
		finAct.flags |= IsDlgButtonChecked(SOUNDERR_CHECK) ? FinAct::ERR_SOUND : 0;
	}
	if (command[0]) {
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
		if (GetDlgItemTextW(SHUTDOWNTIME_EDIT, buf, MAX_PATH) >= 0) {
			finAct.shutdownTime = wcstoul(buf, 0, 10);
		}
	}

	if (cfg->AddFinActW(&finAct)) {
		cfg->WriteIni();
		if (SendDlgItemMessage(TITLE_COMBO, CB_GETCOUNT, 0, 0) < cfg->finActMax) {
			SendDlgItemMessageW(TITLE_COMBO, CB_INSERTSTRING, cfg->finActMax-1, (LPARAM)title);
		}
		Reflect(cfg->SearchFinActW(title));
	}
	else TMsgBox(this).Exec("Add FinAct Error");

	return	TRUE;
}

BOOL TFinActDlg::DelFinAct()
{
	WCHAR	buf[MAX_PATH], msg[MAX_PATH];

	if (GetDlgItemTextW(TITLE_COMBO, buf, MAX_PATH) > 0) {
		int idx = cfg->SearchFinActW(buf);
		swprintf(msg, GetLoadStrW(IDS_FINACTNAME), buf);
		if (cfg->finActArray[idx]->flags & FinAct::BUILTIN) {
			MessageBox("Can't delete buit-in Action", "Error");
		}
		else if (TMsgBox(this).ExecW(msg, GetLoadStrW(IDS_DELCONFIRM), MB_OKCANCEL) == IDOK) {
			cfg->DelFinActW(buf);
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
	isExecW = FALSE;
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

	if (isExecW) {
		SetWindowTextW(title);
		SetDlgItemTextW(MESSAGE_EDIT, msg);
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

UINT TMsgBox::ExecW(const WCHAR *_msg, const WCHAR *_title, UINT _style)
{
	msg = _msg;
	title = _title;
	style = _style;
	isExecW = TRUE;

	return	TDlg::Exec();
}

UINT TMsgBox::Exec(const char *_msg, const char *_title, UINT _style)
{
	msg   = (WCHAR *)_msg;
	title = (WCHAR *)_title;
	style = _style;
	isExecW = FALSE;

	return	TDlg::Exec();
}


TFinDlg::TFinDlg(TMainDlg *_parent) : TMsgBox(_parent), mainDlg(_parent)
{
}

TFinDlg::~TFinDlg()
{
}

UINT TFinDlg::Exec(int _sec, DWORD mainmsg_id)
{
	orgSec = sec = _sec;
	main_msg = GetLoadStr(mainmsg_id);
	proc_msg = GetLoadStr(IDS_WAITPROC_MSG);
	time_fmt = GetLoadStr(IDS_WAITTIME_MSG);
	return	TMsgBox::Exec("", "FastCopy", MB_OKCANCEL);
}


/*
	Window 生成時の CallBack
*/
BOOL TFinDlg::EvCreate(LPARAM lParam)
{
	TMsgBox::EvCreate(lParam);
	Update();
	SetWindowPos(HWND_TOPMOST, 0, 0 ,0, 0, SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE);
	::SetTimer(hWnd, FASTCOPY_FIN_TIMER, 1000, NULL);
	return	TRUE;
}

void TFinDlg::Update()
{
	int		running = 0, waiting = 0;
	char	buf[100];
	int		len = strcpyz(buf, main_msg);
	ShareInfo::CheckInfo ci;

	if (mainDlg->GetRunningCount(&ci) && (ci.all_running > 0 || ci.all_waiting > 0)) {
		sec = orgSec;
		strcpyz(buf+len, proc_msg);
	}
	else {
		sprintf(buf+len, time_fmt, sec--);
	}
	SetDlgItemText(MESSAGE_EDIT, buf);
}

BOOL TFinDlg::EvTimer(WPARAM timerID, TIMERPROC proc)
{
	if (sec > 0) {
		Update();
	}
	else {
		::KillTimer(hWnd, timerID);
		EndDialog(IDOK);
	}
	return	TRUE;
}

int SetSpeedLevelLabel(TWin *win, int level)
{
	if (level == -1)
		level = (int)win->SendDlgItemMessage(SPEED_SLIDER, TBM_GETPOS, 0, 0);
	else
		win->SendDlgItemMessage(SPEED_SLIDER, TBM_SETPOS, TRUE, level);

	char	buf[64];
	sprintf(buf,
			level == SPEED_FULL ?		GetLoadStr(IDS_FULLSPEED_DISP) :
			level == SPEED_AUTO ?		GetLoadStr(IDS_AUTOSLOW_DISP) :
			level == SPEED_SUSPEND ?	GetLoadStr(IDS_SUSPEND_DISP) :
			 							GetLoadStr(IDS_RATE_DISP),
			level * 10);
	win->SetDlgItemText(SPEED_STATIC, buf);
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

	// Protection for Visual C++ Resource editor problem...
	// RICHEDIT20W is correct, but VC++ changes to RICHEDIT20A, sometimes.
#ifdef _DEBUG
#define RICHED20A_TEST
#ifdef RICHED20A_TEST
	char	cname[64];
	if (::GetClassName(_hWnd, cname, sizeof(cname)) && stricmp(cname, "RICHEDIT20A") == 0) {
		static bool once = false;
		if (!once) {
			once = true;
			MessageBox("Change RichEdit20A to RichEdit20W in fastcopy.rc", "FastCopy Resource file problem");
		}
	}
#endif
#endif

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
	UINT	flg = 0;

	flg = (is_readonly || !SendMessage(EM_CANUNDO, 0, 0)) ? MF_DISABLED|MF_GRAYED : 0;
	::AppendMenu(hMenu, MF_STRING|flg, WM_UNDO, GetLoadStr(IDS_UNDO));
	::AppendMenu(hMenu, MF_SEPARATOR, 0, 0);

	flg = is_readonly ? MF_DISABLED|MF_GRAYED : 0;
	::AppendMenu(hMenu, MF_STRING|flg, WM_CUT, GetLoadStr(IDS_CUT));
	::AppendMenu(hMenu, MF_STRING, WM_COPY, GetLoadStr(IDS_COPY));

	flg = (is_readonly || !SendMessage(EM_CANPASTE, 0, 0)) ? MF_DISABLED|MF_GRAYED : 0;
	::AppendMenu(hMenu, MF_STRING|flg, WM_PASTE, GetLoadStr(IDS_PASTE));

	flg = is_readonly ? MF_DISABLED|MF_GRAYED : 0;
	::AppendMenu(hMenu, MF_STRING|flg, WM_CLEAR, GetLoadStr(IDS_DELETE));
	::AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
	::AppendMenu(hMenu, MF_STRING, EM_SETSEL, GetLoadStr(IDS_SELECTALL));

	::TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pos.x, pos.y, 0, hWnd, NULL);
	::DestroyMenu(hMenu);

	return	TRUE;
}

int TEditSub::ExGetText(WCHAR *buf, int max_len, DWORD flags, UINT codepage)
{
	GETTEXTEX	ge;

	memset(&ge, 0, sizeof(ge));
	ge.cb = max_len;
	ge.flags = flags;
	ge.codepage = codepage;

	return	(int)SendMessageW(EM_GETTEXTEX, (WPARAM)&ge, (LPARAM)buf);
}

int TEditSub::ExSetText(const WCHAR *buf, int max_len, DWORD flags, UINT codepage)
{
	SETTEXTEX	se;

	se.flags = flags;
	se.codepage = codepage;

	return	(int)SendMessageW(EM_SETTEXTEX, (WPARAM)&se, (LPARAM)buf);
}

