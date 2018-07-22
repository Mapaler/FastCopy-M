static char *mainwinopt_id = 
	"@(#)Copyright (C) 2015-2017 H.Shirouzu			mainwinopt.cpp	ver3.30";
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Create					: 2015-05-27(Wed)
	Update					: 2017-03-06(Mon)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	======================================================================== */

#include "mainwin.h"
#include "shellext/shelldef.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <richedit.h>

int GetArgOpt(WCHAR *arg, int default_value);
BOOL MakeFileToPathArray(WCHAR *path_file, PathArray *path, BOOL is_ucs2);

struct CopyInfo COPYINFO_LIST [] = {
	{ IDS_COPY_NAME,	0, CMD_COPY_NAME_STR,	FastCopy::DIFFCP_MODE,	FastCopy::BY_NAME    },
	{ IDS_COPY_ATTR,	0, CMD_COPY_ATTR_STR,	FastCopy::DIFFCP_MODE,	FastCopy::BY_ATTR    },
	{ IDS_COPY_LASTEST,	0, CMD_COPY_LASTEST_STR,FastCopy::DIFFCP_MODE,	FastCopy::BY_LASTEST },
	{ IDS_COPY_FORCE,	0, CMD_COPY_FORCE_STR,	FastCopy::DIFFCP_MODE,	FastCopy::BY_ALWAYS  },

//	{ IDS_SYNC_NAME,	0, CMD_SYNC_NAME_STR,	FastCopy::SYNCCP_MODE,	FastCopy::BY_NAME    },

//	{ IDS_SYNC_ATTR,	0, CMD_SYNC_ATTR_STR,	FastCopy::SYNCCP_MODE,	FastCopy::BY_ATTR    },
	{ IDS_SYNC_ATTR,	0, CMD_SYNC_STR,		FastCopy::SYNCCP_MODE,	FastCopy::BY_ATTR    },

//	{ IDS_SYNC_LASTEST,	0, CMD_SYNC_LASTEST_STR,FastCopy::SYNCCP_MODE,	FastCopy::BY_LASTEST },
//	{ IDS_SYNC_FORCE,	0, CMD_SYNC_FORCE_STR,	FastCopy::SYNCCP_MODE,	FastCopy::BY_ALWAYS  },

//	{ IDS_MOVE_NAME,	0, CMD_MOVE_NAME_STR,	FastCopy::MOVE_MODE,	FastCopy::BY_NAME    },

//	{ IDS_MOVE_ATTR,	0, CMD_MOVE_ATTR_STR,	FastCopy::MOVE_MODE,	FastCopy::BY_ATTR    },
	{ IDS_MOVE_ATTR,	0, CMD_MOVE_STR,		FastCopy::MOVE_MODE,	FastCopy::BY_ATTR    },

//	{ IDS_MOVE_LASTEST,	0, CMD_MOVE_LASTEST_STR,FastCopy::MOVE_MODE,	FastCopy::BY_LASTEST },

//	{ IDS_MOVE_FORCE,	0, CMD_MOVE_FORCE_STR,	FastCopy::MOVE_MODE,	FastCopy::BY_ALWAYS  },
	{ IDS_MOVE_FORCE,	0, CMD_MOVE_STR,		FastCopy::MOVE_MODE,	FastCopy::BY_ALWAYS  },

//	{ IDS_MUTUAL,		0, CMD_MUTUAL_STR,		FastCopy::MUTUAL_MODE,	FastCopy::BY_LASTEST },

	{ IDS_DELETE,		0, CMD_DELETE_STR,		FastCopy::DELETE_MODE,	FastCopy::BY_ALWAYS  },

	{ IDS_TEST,			0, CMD_TEST_STR,		FastCopy::TEST_MODE,	FastCopy::BY_ALWAYS  },

	{ 0,				0, 0,					(FastCopy::Mode)0,	  (FastCopy::OverWrite)0 }
};

BOOL TMainDlg::SetCopyModeList(void)
{
	int	idx = cfg.copyMode;

	if (copyInfo == NULL) {		// 初回コピーモードリスト作成
		for (int i=0; COPYINFO_LIST[i].resId; i++) {
			COPYINFO_LIST[i].list_str = LoadStr(COPYINFO_LIST[i].resId);
		}
		copyInfo = new CopyInfo[sizeof(COPYINFO_LIST) / sizeof(CopyInfo)];
	}
	else {
		idx = (int)SendDlgItemMessage(MODE_COMBO, CB_GETCURSEL, 0, 0);
		SendDlgItemMessage(MODE_COMBO, CB_RESETCONTENT, 0, 0);
	}

	CopyInfo *ci = copyInfo;
	for (int i=0; COPYINFO_LIST[i].resId; i++) {
		if (cfg.enableMoveAttr && COPYINFO_LIST[i].resId == IDS_MOVE_FORCE
		|| !cfg.enableMoveAttr && COPYINFO_LIST[i].resId == IDS_MOVE_ATTR) {
			continue;
		}
		if (!cfg.testMode && COPYINFO_LIST[i].mode == FastCopy::TEST_MODE) {
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

BOOL TMainDlg::CommandLineExecW(int argc, WCHAR **argv)
{
	VBuf	shellExtBuf;
	int		len;
	int		job_idx = -1;

	BOOL	is_openwin			= FALSE;
	BOOL	is_noexec			= FALSE;
	BOOL	is_delete			= FALSE;
	BOOL	is_updated			= FALSE;
	int		estimate_flg		= -1;
	DWORD	runas_flg			= 0;
	int		filter_mode			= 0;
	enum	{ NORMAL_FILTER=0x01, EXTEND_FILTER=0x02 };

	WCHAR	*p;
	WCHAR	*dst_path	= NULL;
	PathArray pathArray;

	argc--, argv++;		// 実行ファイル名は skip

	for (int i = 0; i < argc && argv[i][0] == '/'; i++) {	// /no_ui だけは先に検査
		if (wcsicmpEx(argv[i], NOUI_STR, &len) == 0) {
			isNoUI = GetArgOpt(argv[i] + len, TRUE);
		}
	}

	while (*argv && (*argv)[0] == '/') {
		if (wcsicmpEx(*argv, CMD_STR, &len) == 0) {
			int idx = CmdNameToComboIndex(*argv + len);
			if (idx == -1) {
				MessageBoxW(FmtW(L"%s is not recognized.\r\n%s", *argv,
					isNoUI ? L"" : L"\r\n" USAGE_STR), L"Illegal Command");
				return	FALSE;
			}

			// コマンドモードを選択
			SendDlgItemMessage(MODE_COMBO, CB_SETCURSEL, idx, 0);
		}
		else if (wcsicmpEx(*argv, JOB_STR, &len) == 0) {
			if ((job_idx = cfg.SearchJobW(*argv + len)) == -1) {
				MessageBoxW(FmtW(L"%s: %s", LoadStrW(IDS_JOBNOTFOUND), *argv + len),
							L"Illegal Command");
				return	FALSE;
			}
			SetJob(job_idx);
		}
		else if (wcsicmpEx(*argv, BUFSIZE_STR, &len) == 0) {
			SetDlgItemTextW(BUFSIZE_EDIT, *argv + len);
		}
		else if (wcsicmpEx(*argv, FILELOG_STR, &len) == 0) {
			if ((*argv)[len] == '=') len++;
			p = *argv + len;
			if (p[0] == 0 || wcsicmp(p, L"TRUE") == 0) {
				fileLogMode = AUTO_FILELOG;
			}
			else if (wcsicmp(p, L"FALSE") == 0) {
				fileLogMode = NO_FILELOG;
			}
			else {
				wcscpy(fileLogPath, p);
				fileLogMode = FIX_FILELOG;
			}
		}
		else if (wcsicmpEx(*argv, LOGFILE_STR, &len) == 0 && (p = *argv + len)) {
			wcscpy(errLogPath, p);
		}
		else if (wcsicmpEx(*argv, LOG_STR, &len) == 0) {
			isErrLog = GetArgOpt(*argv + len, TRUE);
		}
		else if (wcsicmpEx(*argv, UTF8LOG_STR, &len) == 0) {
			isUtf8Log = GetArgOpt(*argv + len, TRUE);
		}
		else if (wcsicmpEx(*argv, REPARSE_STR, &len) == 0) {
			isReparse = GetArgOpt(*argv + len, TRUE);
		}
		else if (wcsicmpEx(*argv, FORCESTART_STR, &len) == 0) {
			forceStart = GetArgOpt(*argv + len, TRUE);
			if (forceStart >= 2) {
				maxTempRunNum = forceStart;
				forceStart = 2;
			}
		}
		else if (wcsicmpEx(*argv, SKIPEMPTYDIR_STR, &len) == 0) {
			skipEmptyDir = GetArgOpt(*argv + len, TRUE);
		}
		else if (wcsicmpEx(*argv, ERRSTOP_STR, &len) == 0) {
			CheckDlgButton(IGNORE_CHECK, GetArgOpt(*argv + len, TRUE) ? FALSE : TRUE);
		}
		else if (wcsicmpEx(*argv, ESTIMATE_STR, &len) == 0) {
			estimate_flg = GetArgOpt(*argv + len, TRUE);
		}
		else if (wcsicmpEx(*argv, VERIFY_STR, &len) == 0) {
			CheckDlgButton(VERIFY_CHECK, GetArgOpt(*argv + len, TRUE) ? TRUE : FALSE);
		}
		else if (wcsicmpEx(*argv, AUTOSLOW_STR, &len) == 0) {
			SetSpeedLevelLabel(this, speedLevel = GetArgOpt(*argv + len, TRUE) ?
				SPEED_AUTO : SPEED_FULL);
		}
		else if (wcsicmpEx(*argv, SPEED_STR, &len) == 0) {
			p = *argv + len;
			speedLevel = wcsicmp(p, SPEED_FULL_STR) == 0 ? SPEED_FULL :
						wcsicmp(p, SPEED_AUTOSLOW_STR) == 0 ? SPEED_AUTO :
						wcsicmp(p, SPEED_SUSPEND_STR) == 0 ?
						SPEED_SUSPEND : GetArgOpt(p, SPEED_FULL);
			SetSpeedLevelLabel(this, speedLevel = min(speedLevel, SPEED_FULL));
		}
		else if (wcsicmpEx(*argv, OWDEL_STR, &len) == 0) {
			CheckDlgButton(OWDEL_CHECK, GetArgOpt(*argv + len, TRUE));
		}
		else if (wcsicmpEx(*argv, WIPEDEL_STR, &len) == 0) {
			CheckDlgButton(OWDEL_CHECK, GetArgOpt(*argv + len, TRUE));
		}
		else if (wcsicmpEx(*argv, ACL_STR, &len) == 0) {
			CheckDlgButton(ACL_CHECK, GetArgOpt(*argv + len, TRUE));
		}
		else if (wcsicmpEx(*argv, STREAM_STR, &len) == 0) {
			CheckDlgButton(STREAM_CHECK, GetArgOpt(*argv + len, TRUE));
		}
		else if (wcsicmpEx(*argv, LINKDEST_STR, &len) == 0) {
			int	val = GetArgOpt(*argv + len, TRUE);
			if (val > 1 || val < 0) {
				if (val < 300000) {
					return MessageBoxW(L"Too small(<300000) hashtable for linkdest",
							L"Illegal Command"), FALSE;
				}
				maxLinkHash = val;
				isLinkDest = TRUE;
			} else {
				isLinkDest = val;	// TRUE or FALSE
			}
		}
		else if (wcsicmpEx(*argv, RECREATE_STR, &len) == 0) {
			isReCreate = GetArgOpt(*argv + len, TRUE);
		}
		else if (wcsicmpEx(*argv, SRCFILEW_STR, &len) == 0) {
			if (!MakeFileToPathArray(*argv + len, &pathArray, TRUE)) {
				MessageBoxW(FmtW(L"Can't open: %s", *argv + len), L"Option error");
				return	FALSE;
			}
		}
		else if (wcsicmpEx(*argv, SRCFILE_STR, &len) == 0) {
			if (!MakeFileToPathArray(*argv + len, &pathArray, FALSE)) {
				MessageBoxW(FmtW(L"Can't open: %s", *argv + len) , L"Option error");
				return	FALSE;
			}
		}
		else if (wcsicmpEx(*argv, OPENWIN_STR, &len) == 0) {
			is_openwin = GetArgOpt(*argv + len, TRUE);
		}
		else if (wcsicmpEx(*argv, AUTOCLOSE_STR, &len) == 0 && autoCloseLevel != FORCE_CLOSE) {
			autoCloseLevel = (AutoCloseLevel)GetArgOpt(*argv + len, NOERR_CLOSE);
		}
		else if (wcsicmpEx(*argv, FORCECLOSE_STR, &len) == 0) {
			autoCloseLevel = FORCE_CLOSE;
		}
		else if (wcsicmpEx(*argv, NOEXEC_STR, &len) == 0) {
			is_noexec = GetArgOpt(*argv + len, TRUE);
		}
		else if (wcsicmpEx(*argv, BALLOON_STR, &len) == 0) {
			if (GetArgOpt(*argv + len, TRUE)) {
				finishNotify |= 1;
			} else {
				finishNotify &= ~1;
			}
		}
		else if (wcsicmpEx(*argv, DISKMODE_STR, &len) == 0) {
			if (wcsicmp(*argv + len, DISKMODE_SAME_STR) == 0)
				diskMode = 1;
			else if (wcsicmp(*argv + len, DISKMODE_DIFF_STR) == 0)
				diskMode = 2;
			else
				diskMode = 0;
		}
		else if (wcsicmpEx(*argv, INCLUDE_STR, &len) == 0) {
			filter_mode |= NORMAL_FILTER;
			SetDlgItemTextW(INCLUDE_COMBO, strtok_pathW(*argv + len, L"", &p));
		}
		else if (wcsicmpEx(*argv, EXCLUDE_STR, &len) == 0) {
			filter_mode |= NORMAL_FILTER;
			SetDlgItemTextW(EXCLUDE_COMBO, strtok_pathW(*argv + len, L"", &p));
		}
		else if (wcsicmpEx(*argv, FROMDATE_STR, &len) == 0) {
			filter_mode |= EXTEND_FILTER;
			SetDlgItemTextW(FROMDATE_COMBO, strtok_pathW(*argv + len, L"", &p));
		}
		else if (wcsicmpEx(*argv, TODATE_STR, &len) == 0) {
			filter_mode |= EXTEND_FILTER;
			SetDlgItemTextW(TODATE_COMBO, strtok_pathW(*argv + len, L"", &p));
		}
		else if (wcsicmpEx(*argv, MINSIZE_STR, &len) == 0) {
			filter_mode |= EXTEND_FILTER;
			SetDlgItemTextW(MINSIZE_COMBO, strtok_pathW(*argv + len, L"", &p));
		}
		else if (wcsicmpEx(*argv, MAXSIZE_STR, &len) == 0) {
			filter_mode |= EXTEND_FILTER;
			SetDlgItemTextW(MAXSIZE_COMBO, strtok_pathW(*argv + len, L"", &p));
		}
		else if (wcsicmpEx(*argv, NOCONFIRMDEL_STR, &len) == 0) {
			noConfirmDel = GetArgOpt(*argv + len, TRUE);
		}
		else if (wcsicmpEx(*argv, NOCONFIRMSTOP_STR, &len) == 0) {
			noConfirmStop = GetArgOpt(*argv + len, TRUE);
		}
		else if (wcsicmpEx(*argv, FINACT_STR, &len) == 0) {
			if (wcsicmp(*argv + len, L"FALSE") == 0) {
				SetFinAct(-1);
			}
			else {
				int idx = cfg.SearchFinActW(*argv + len, TRUE);
				if (idx >= 0) SetFinAct(idx);
			}
		}
		else if (wcsicmpEx(*argv, DLSVT_STR, &len) == 0) {
			if (wcsicmp(*argv + len, L"none") == 0) {
				dlsvtMode = 0;
			}
			else if (wcsicmp(*argv + len, L"auto") == 0) {
				dlsvtMode = 1;
			}
			else if (wcsicmp(*argv + len, L"always") == 0) {
				dlsvtMode = 2;
			}
		}
		else if (wcsicmpEx(*argv, UPDATED_STR, &len) == 0) {
			is_updated = TRUE;
			is_noexec = TRUE;

			SendDlgItemMessage(PATH_EDIT,   EM_AUTOURLDETECT, AURL_ENABLEURL, 0);
			LRESULT evMask = pathEdit.SendMessage(EM_GETEVENTMASK, 0, 0) | ENM_LINK;
			pathEdit.SendMessage(EM_SETEVENTMASK, 0, evMask); 

			SetDlgItemText(PATH_EDIT,
				Fmt("Update done. (%s) \r\nFastCopy HomePage: %s",
				GetVersionStr(), LoadStr(IDS_FASTCOPYURL)));
		}
		else if (wcsicmpEx(*argv, TO_STR, &len) == 0) {
			SetDlgItemTextW(DST_COMBO, dst_path = *argv + len);
		}
		else if (wcsicmpEx(*argv, RUNAS_STR, &len) == 0) {
			WCHAR	*p = *argv + len;
			HWND	hRunasParent = (HWND)(LONG_PTR)wcstoull(p, 0, 16);
			runas_flg = wcstoul(wcschr(p, ',') + 1, 0, 16);
			if (!::IsUserAnAdmin() || RunasSync(hRunasParent) == FALSE) {
				MessageBoxW(L"Not Admin or Failed to read parent window info", L"Option error");
				return	FALSE;
			}
		}
		else if (wcsicmpEx(*argv, FCSHEXT1_STR, &len) == 0) {
			Cfg::ShExtCfg	*shCfg = &cfg.shAdmin;
			shellMode = SHELL_ADMIN;

			if ((*argv)[len] == '=') {
				DWORD	flags = wcstoul(*argv + len + 1, 0, 16);
//				if (flags & SHEXT_ISSTOREOPT) {
					if ((flags & SHEXT_ISADMIN) == 0) {
						shellMode = SHELL_USER;
						shCfg = &cfg.shUser;
					}
					shCfg->noConfirm    = (flags & SHEXT_NOCONFIRM)    != 0;
					shCfg->noConfirmDel = (flags & SHEXT_NOCONFIRMDEL) != 0;
					shCfg->taskTray     = (flags & SHEXT_TASKTRAY)     != 0;
					shCfg->autoClose    = (flags & SHEXT_AUTOCLOSE)    != 0;
//				}
			}

			is_openwin = !shCfg->taskTray;
			autoCloseLevel = shCfg->autoClose ? NOERR_CLOSE : NO_CLOSE;
			HANDLE	hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);
			DWORD	read_size;

			shellExtBuf.AllocBuf(SHELLEXT_MIN_ALLOC, SHELLEXT_MAX_ALLOC);
			while (::ReadFile(hStdInput, shellExtBuf.UsedEnd(),
					(DWORD)shellExtBuf.RemainSize(), &read_size, 0) && read_size > 0) {
				if (shellExtBuf.AddUsedSize(read_size) == shellExtBuf.Size()) {
					shellExtBuf.Grow(SHELLEXT_MIN_ALLOC);
				}
			}
			if (shellExtBuf.UsedSize() == shellExtBuf.MaxSize()) {
				MessageBoxW(L"Too long arguments");
				return	FALSE;
			}
			shellExtBuf.UsedEnd()[0] = 0;

			if ((argv = CommandLineToArgvExW(shellExtBuf.WBuf(), &argc)) == NULL)
				break;
			continue;	// 再 parse
		}
		else if (wcsicmpEx(*argv, NOUI_STR, &len) == 0) {
			// すでに確認済み
		}
		else {
			MessageBoxW(FmtW(L"%s is not recognized.\r\n%s", *argv,
						isNoUI ? L"" : L"\r\n" USAGE_STR), L"Illegal Command");
			return	FALSE;
		}
		argc--, argv++;
	}

	// /no_ui の場合、UI系の確認をキャンセル
	if (isNoUI) noConfirmDel = noConfirmStop = TRUE;

	is_delete = GetCopyMode() == FastCopy::DELETE_MODE;

	if (!isRunAsStart) {
		if (job_idx == -1) {
			if (!is_updated) {
				srcEdit.SetWindowText("");
				if (!dst_path)
					SetDlgItemText(DST_COMBO, "");
			}
		}
		WCHAR	wBuf[MAX_PATH_EX];

		while (*argv && (*argv)[0] != '/') {
			WCHAR	*path = *argv;
			if (shellMode != SHELL_NONE && !is_delete) {
				if (NetPlaceConvertW(path, wBuf)) {
					path = wBuf;
					isNetPlaceSrc = TRUE;
				}
			}
			pathArray.RegisterPath(path);
			argc--, argv++;
		}

		int		path_len = pathArray.GetMultiPathLen(CRLF, NULW) + 1;
		Wstr	wstr(path_len);
		if (pathArray.GetMultiPath(wstr.Buf(), path_len, CRLF, NULW)) {
			srcEdit.SetWindowTextW(wstr.s());
		}

		if (argc == 1 && wcsicmpEx(*argv, TO_STR, &len) == 0) {
			dst_path = *argv + len;
			if (shellMode != SHELL_NONE && NetPlaceConvertW(dst_path, wBuf))
				dst_path = wBuf;
			SetDlgItemTextW(DST_COMBO, dst_path);
		}
		else if (argc >= 1)
			return	MessageBoxW(isNoUI ? L"" : USAGE_STR, L"Too few/many argument"), FALSE;

		if ((filter_mode & EXTEND_FILTER) && !isExtendFilter) {
			isExtendFilter = TRUE;
			SetExtendFilter();
		}

		if (srcEdit.GetWindowTextLengthW() == 0
		|| (!is_delete && ::GetWindowTextLengthW(GetDlgItem(DST_COMBO)) == 0)) {
			is_noexec = TRUE;
			if (shellMode != SHELL_NONE)	// コピー先の無い shell起動時は、autoclose を無視する
				autoCloseLevel = NO_CLOSE;
		}

		if (filter_mode) {
			ReflectFilterCheck(!IsDlgButtonChecked(FILTER_CHECK));
		}

		SetItemEnable(GetCopyMode());
		if (diskMode)
			UpdateMenu();

		if (estimate_flg == -1) {
			estimate_flg = isNoUI ? 0 : cfg.estimateMode;
		}
		if (cfg.estimateMode != estimate_flg) {
			CheckDlgButton(ESTIMATE_CHECK, estimate_flg);
		}
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
			TaskTray(NIM_ADD, FCNORM_ICON_IDX, FASTCOPY);
		}
	}

	return	is_noexec || (isRunAsStart && !(runas_flg & RUNAS_IMMEDIATE)) ?
		TRUE : ExecCopy(CMDLINE_EXEC);
}

int TMainDlg::CmdNameToComboIndex(const WCHAR *cmd_name)
{
	for (int i=0; copyInfo[i].cmdline_name; i++) {
		if (wcsicmp(cmd_name, copyInfo[i].cmdline_name) == 0)
			return	i;
	}
	return	-1;
}

int GetArgOpt(WCHAR *arg, int default_value)
{
	if (arg[0] == '=') {
		arg++;
	}

	if (arg[0] == 0) {
		return	default_value;
	}

	if (wcsicmp(arg, L"TRUE") == 0)
		return	TRUE;

	if (wcsicmp(arg, L"FALSE") == 0)
		return	FALSE;

	if (arg[0] >= '0' && arg[0] <= '9' /* || arg[0] == '-' */)
		return	wcstol(arg, NULL, 0);

	return	default_value;
}

BOOL MakeFileToPathArray(WCHAR *path_file, PathArray *path, BOOL is_ucs2)
{
	HANDLE	hFile = ::CreateFileW(path_file, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 0,
					OPEN_EXISTING, 0, 0);
	if (hFile == INVALID_HANDLE_VALUE) return	FALSE;

	BOOL	ret = FALSE;
	DWORD	size = ::GetFileSize(hFile, 0), trans;
	DynBuf	buf(size + sizeof(WCHAR));

	if (::ReadFile(hFile, buf, size, &trans, 0) && size == trans) {
		BYTE	*top = (BYTE *)buf;
		*(WCHAR *)(top + size) = 0;
		if (is_ucs2) {
			if (size > 2 && top[0] == 0xff && top[1] == 0xfe) {
				top += 2;
			}
			if (path->RegisterMultiPath((WCHAR *)top, CRLF) > 0) {
				ret = TRUE;
			}
		}
		else {
			if (size > 3 && top[0] == 0xef && top[1] == 0xbb && top[2] == 0xbf) {
				top += 3;
			}
			Wstr	wstr((char *)top, IsUTF8((char *)top) ? BY_UTF8 : BY_MBCS);
			if (path->RegisterMultiPath(wstr.s(), CRLF) > 0) {
				ret = TRUE;
			}
		}
	}
	::CloseHandle(hFile);

	return	ret;
}

