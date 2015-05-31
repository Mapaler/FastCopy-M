static char *cfg_id = 
	"@(#)Copyright (C) 2004-2012 H.Shirouzu		cfg.cpp	ver2.10";
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Create					: 2004-09-15(Wed)
	Update					: 2012-06-17(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "mainwin.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>

#define FASTCOPY_INI			"FastCopy.ini"
#define MAIN_SECTION			"main"
#define SRC_HISTORY				"src_history"
#define DST_HISTORY				"dst_history"
#define DEL_HISTORY				"del_history"
#define INC_HISTORY				"include_history"
#define EXC_HISTORY				"exclude_history"
#define FROMDATE_HISTORY		"fromdate_history"
#define TODATE_HISTORY			"todate_history"
#define MINSIZE_HISTORY			"minsize_history"
#define MAXSIZE_HISTORY			"maxsize_history"

#define MAX_HISTORY_KEY			"max_history"
#define COPYMODE_KEY			"default_copy_mode"
#define COPYFLAGS_KEY			"default_copy_flags"
#define SKIPEMPTYDIR_KEY		"skip_empty_dir"
#define FORCESTART_KEY			"force_start"
#define IGNORE_ERR_KEY			"ignore_error"
#define ESTIMATE_KEY			"estimate_mode"
#define DISKMODE_KEY			"disk_mode"
#define ISTOPLEVEL_KEY			"is_toplevel"
#define ISERRLOG_KEY			"is_errlog"
#define ISUTF8LOG_KEY			"is_utf8log"
#define FILELOGMODE_KEY			"filelog_mode"
#define ACLERRLOG_KEY			"aclerr_log"
#define STREAMERRLOG_KEY		"streamerr_log"
#define ISRUNASBUTTON_KEY		"is_runas_button"
#define ISSAMEDIRRENAME_KEY		"is_samedir_rename"
#define BUFSIZE_KEY				"bufsize"
#define MAXTRANSSIZE_KEY		"max_transize"
#define MAXOPENFILES_KEY		"max_openfiles"
#define MAXATTRSIZE_KEY			"max_attrsize"
#define MAXDIRSIZE_KEY			"max_dirsize"
#define SHEXTAUTOCLOSE_KEY		"shext_autoclose"
#define SHEXTTASKTRAY_KEY		"shext_tasktray"
#define SHEXTNOCONFIRM_KEY		"shext_dd_noconfirm"
#define SHEXTNOCONFIRMDEL_KEY	"shext_right_noconfirm"
#define EXECCONRIM_KEY			"exec_confirm"
#define FORCESTART_KEY			"force_start"
#define LCID_KEY				"lcid"
#define LOGFILE_KEY				"logfile"
#define WAITTICK_KEY			"wait_tick"
#define ISAUTOSLOWIO_KEY		"is_autoslow_io2"
#define SPEEDLEVEL_KEY			"speed_level"
#define ALWAYSLOWIO_KEY			"alwaysLowIo"
#define DRIVEMAP_KEY			"driveMap"
#define OWDEL_KEY				"overwrite_del"
#define ACL_KEY					"acl"
#define STREAM_KEY				"stream"
#define VERIFY_KEY				"verify"
#define USEMD5_KEY				"using_md5"
#define NSA_KEY					"nsa_del"
#define DELDIR_KEY				"deldir_with_filter"
#define MOVEATTR_KEY			"move_attr"
#define SERIALMOVE_KEY			"serial_move"
#define SERIALVERIFYMOVE_KEY	"serial_verify_move"
#define REPARSE_KEY				"reparse2"
#define LINKDEST_KEY			"linkdest"
#define MAXLINKHASH_KEY			"max_linkhash"
#define ALLOWCONTFSIZE_KEY		"allow_cont_fsize"
#define RECREATE_KEY			"recreate"
#define EXTENDFILTER_KEY		"extend_filter"
#define WINPOS_KEY				"win_pos"
#define TASKBARMODE_KEY			"taskbarMode"
#define INFOSPAN_KEY			"infoSpan"

#define NONBUFMINSIZENTFS_KEY	"nonbuf_minsize_ntfs2"
#define NONBUFMINSIZEFAT_KEY	"nonbuf_minsize_fat"
#define ISREADOSBUF_KEY			"is_readosbuf"

#define FMT_JOB_KEY				"job_%d"
#define TITLE_KEY				"title"
#define CMD_KEY					"cmd"
#define SRC_KEY					"src"
#define DST_KEY					"dst"
#define FILTER_KEY				"filter"
#define INCLUDE_KEY				"include_filter"
#define EXCLUDE_KEY				"exclude_filter"
#define FROMDATE_KEY			"fromdate_filter"
#define TODATE_KEY				"todate_filter"
#define MINSIZE_KEY				"minsize_filter"
#define MAXSIZE_KEY				"maxsize_filter"
#define FMT_FINACT_KEY			"finaction_%d"
#define SOUND_KEY				"sound"
#define SHUTDOWNTIME_KEY		"shutdown_time"
#define FLAGS_KEY				"flags"

#define DEFAULT_MAX_HISTORY		10
#define DEFAULT_COPYMODE		1
#define DEFAULT_COPYFLAGS		0
#define DEFAULT_EMPTYDIR		1
#define DEFAULT_FORCESTART		0
#ifdef _WIN64
#define DEFAULT_BUFSIZE			128
#define DEFAULT_MAXTRANSSIZE	16
#define DEFAULT_MAXATTRSIZE		(1024 * 1024 * 1024)
#define DEFAULT_MAXDIRSIZE		(1024 * 1024 * 1024)
#else
#define DEFAULT_BUFSIZE			32
#define DEFAULT_MAXTRANSSIZE	8
#define DEFAULT_MAXATTRSIZE		(128 * 1024 * 1024)
#define DEFAULT_MAXDIRSIZE		(128 * 1024 * 1024)
#endif
#define DEFAULT_MAXOPENFILES	256
#define DEFAULT_NBMINSIZE_NTFS	64		// nbMinSize 参照
#define DEFAULT_NBMINSIZE_FAT	128		// nbMinSize 参照
#define DEFAULT_LINKHASH		300000
#define DEFAULT_ALLOWCONTFSIZE	(1024 * 1024 * 1024)
#define DEFAULT_WAITTICK		10
#define JOB_MAX					1000
#define FINACT_MAX				1000
#define DEFAULT_FASTCOPYLOG		"FastCopy.log"
#define DEFAULT_INFOSPAN		2


/*
	Vista以降
*/
#ifdef _WIN64
BOOL ConvertToX86Dir(void *target)
{
	WCHAR	buf[MAX_PATH];
	WCHAR	buf86[MAX_PATH];
	int		len;

	if (!TSHGetSpecialFolderPathV(NULL, buf, CSIDL_PROGRAM_FILES, FALSE)) return FALSE;
	len = strlenV(buf);
	SetChar(buf, len++, '\\');
	SetChar(buf, len, 0);

	if (strnicmpV(buf, target, len)) return FALSE;

	if (!TSHGetSpecialFolderPathV(NULL, buf86, CSIDL_PROGRAM_FILESX86, FALSE)) return FALSE;
	MakePathV(buf, buf86, MakeAddr(target, len));
	strcpyV(target, buf);

	return	 TRUE;
}
#endif

BOOL ConvertVirtualStoreConf(void *execDirV, void *userDirV, void *virtualDirV)
{
#define FASTCOPY_INI_W			L"FastCopy.ini"
	WCHAR	buf[MAX_PATH];
	WCHAR	org_ini[MAX_PATH], usr_ini[MAX_PATH], vs_ini[MAX_PATH];
	BOOL	is_admin = TIsUserAnAdmin();
	BOOL	is_exists;

	MakePathV(usr_ini, userDirV, FASTCOPY_INI_W);
	MakePathV(org_ini, execDirV, FASTCOPY_INI_W);

#ifdef _WIN64
	ConvertToX86Dir(org_ini);
#endif

	is_exists = GetFileAttributesV(usr_ini) != 0xffffffff;
	 if (!is_exists) {
		CreateDirectoryV(userDirV, NULL);
	}

	if (virtualDirV && GetChar(virtualDirV, 0)) {
		MakePathV(vs_ini,  virtualDirV, FASTCOPY_INI_W);
		if (GetFileAttributesV(vs_ini) != 0xffffffff) {
			if (!is_exists) {
				is_exists = ::CopyFileW(vs_ini, usr_ini, TRUE);
			}
			MakePathV(buf, userDirV, L"to_OldDir(VirtualStore).lnk");
			SymLinkV(virtualDirV, buf);
			sprintfV(buf, L"%s.obsolete", vs_ini);
			MoveFileW(vs_ini, buf);
			if (GetFileAttributesV(vs_ini) != 0xffffffff) {
				DeleteFileV(vs_ini);
			}
		}
	}

	if ((is_admin || !is_exists) && GetFileAttributesV(org_ini) != 0xffffffff) {
		if (!is_exists) {
			is_exists = ::CopyFileW(org_ini, usr_ini, TRUE);
		}
		if (is_admin) {
			sprintfV(buf, L"%s.obsolete", org_ini);
			MoveFileW(org_ini, buf);
			if (GetFileAttributesV(org_ini) != 0xffffffff) {
				DeleteFileV(org_ini);
			}
		}
	}

	MakePathV(buf, userDirV, L"to_ExeDir.lnk");
	if (GetFileAttributesV(buf) == 0xffffffff) {
		SymLinkV(execDirV, buf);
	}

	return	TRUE;
}


/*=========================================================================
  クラス ： Cfg
  概  要 ： コンフィグクラス
  説  明 ： 
  注  意 ： 
=========================================================================*/
Cfg::Cfg()
{
}

Cfg::~Cfg()
{
	free(virtualDirV);
	free(userDirV);
	free(errLogPathV);
	free(execDirV);
	free(execPathV);
}

BOOL Cfg::Init(void *user_dir, void *virtual_dir)
{
	WCHAR	buf[MAX_PATH], path[MAX_PATH], *fname = NULL;

	GetModuleFileNameV(NULL, buf, MAX_PATH);
	GetFullPathNameV(buf, MAX_PATH, path, (void **)&fname);
	if (!fname) return FALSE;

	execPathV = strdupV(path);
	SetChar(fname, -1, 0); // remove '\\'
	execDirV = strdupV(path);

	errLogPathV = NULL;
	userDirV = NULL;
	virtualDirV = NULL;

	if (IsWinVista() && TIsVirtualizedDirV(execDirV)) {
		if (user_dir) {
			userDirV = strdupV(user_dir);
			if (virtual_dir) virtualDirV = strdupV(virtual_dir);
		}
		else {
			WCHAR	virtual_store[MAX_PATH];
			WCHAR	fastcopy_dir[MAX_PATH];
			WCHAR	*fastcopy_dirname = NULL;

			GetFullPathNameW(path, MAX_PATH, fastcopy_dir, &fastcopy_dirname);

			TSHGetSpecialFolderPathV(NULL, buf, CSIDL_APPDATA, FALSE);
			MakePathV(path, buf, fastcopy_dirname);
			userDirV = strdupV(path);

			strcpyV(buf, execDirV);
#ifdef _WIN64
			ConvertToX86Dir(buf);
#endif
			if (!TMakeVirtualStorePathV(buf, virtual_store)) return FALSE;
			virtualDirV = strdupV(virtual_store);
		}
		ConvertVirtualStoreConf(execDirV, userDirV, virtualDirV);
	}
	if (!userDirV) userDirV = strdupV(execDirV);

	char	ini_path[MAX_PATH];
	MakePath(ini_path, toA(userDirV), FASTCOPY_INI);
	ini.Init(ini_path);

	return	TRUE;
}

BOOL Cfg::ReadIni(void *user_dir, void *virtual_dir)
{
	if (!Init(user_dir, virtual_dir)) return FALSE;

	int		i, j;
	char	key[100], *p;
	char	*buf = new char [MAX_HISTORY_CHAR_BUF];
	WCHAR	*wbuf = new WCHAR [MAX_HISTORY_BUF];
	char	*section_array[] = {	SRC_HISTORY, DST_HISTORY, DEL_HISTORY,
									INC_HISTORY, EXC_HISTORY,
									FROMDATE_HISTORY, TODATE_HISTORY,
									MINSIZE_HISTORY, MAXSIZE_HISTORY };
	void	***history_array[] = {	&srcPathHistory, &dstPathHistory, &delPathHistory,
									&includeHistory, &excludeHistory,
									&fromDateHistory, &toDateHistory,
									&minSizeHistory, &maxSizeHistory };

	srcPathHistory	= NULL;
	dstPathHistory	= NULL;
	delPathHistory	= NULL;
	includeHistory	= NULL;
	excludeHistory	= NULL;

	jobArray = NULL;
	jobMax = 0;

	finActArray = NULL;
	finActMax = 0;

	ini.SetSection(MAIN_SECTION);
	bufSize			= ini.GetInt(BUFSIZE_KEY, DEFAULT_BUFSIZE);
	maxTransSize	= ini.GetInt(MAXTRANSSIZE_KEY, DEFAULT_MAXTRANSSIZE);
	maxOpenFiles	= ini.GetInt(MAXOPENFILES_KEY, DEFAULT_MAXOPENFILES);
	maxAttrSize		= ini.GetInt(MAXATTRSIZE_KEY, DEFAULT_MAXATTRSIZE);
	maxDirSize		= ini.GetInt(MAXDIRSIZE_KEY, DEFAULT_MAXDIRSIZE);
	nbMinSizeNtfs	= ini.GetInt(NONBUFMINSIZENTFS_KEY, DEFAULT_NBMINSIZE_NTFS);
	nbMinSizeFat	= ini.GetInt(NONBUFMINSIZEFAT_KEY, DEFAULT_NBMINSIZE_FAT);
	isReadOsBuf		= ini.GetInt(ISREADOSBUF_KEY, FALSE);
	maxHistoryNext	= maxHistory = ini.GetInt(MAX_HISTORY_KEY, DEFAULT_MAX_HISTORY);
	copyMode		= ini.GetInt(COPYMODE_KEY, DEFAULT_COPYMODE);
	copyFlags		= ini.GetInt(COPYFLAGS_KEY, DEFAULT_COPYFLAGS);
	skipEmptyDir	= ini.GetInt(SKIPEMPTYDIR_KEY, DEFAULT_EMPTYDIR);
	forceStart		= ini.GetInt(FORCESTART_KEY, DEFAULT_FORCESTART);
	ignoreErr		= ini.GetInt(IGNORE_ERR_KEY, TRUE);
	estimateMode	= ini.GetInt(ESTIMATE_KEY, 0);
	diskMode		= ini.GetInt(DISKMODE_KEY, 0);
	isTopLevel		= ini.GetInt(ISTOPLEVEL_KEY, FALSE);
	isErrLog		= ini.GetInt(ISERRLOG_KEY, TRUE);
	isUtf8Log		= ini.GetInt(ISUTF8LOG_KEY, TRUE);
	fileLogMode		= ini.GetInt(FILELOGMODE_KEY, 0);
	aclErrLog		= ini.GetInt(ACLERRLOG_KEY, FALSE);
	streamErrLog	= ini.GetInt(STREAMERRLOG_KEY, FALSE);
	isRunasButton	= ini.GetInt(ISRUNASBUTTON_KEY, FALSE);
	isSameDirRename	= ini.GetInt(ISSAMEDIRRENAME_KEY, TRUE);
	shextAutoClose	= ini.GetInt(SHEXTAUTOCLOSE_KEY, TRUE);
	shextTaskTray	= ini.GetInt(SHEXTTASKTRAY_KEY, FALSE);
	shextNoConfirm	= ini.GetInt(SHEXTNOCONFIRM_KEY, FALSE);
	shextNoConfirmDel = ini.GetInt(SHEXTNOCONFIRMDEL_KEY, FALSE);
	execConfirm		= ini.GetInt(EXECCONRIM_KEY, FALSE);
	lcid			= ini.GetInt(LCID_KEY, -1);
	waitTick		= ini.GetInt(WAITTICK_KEY, DEFAULT_WAITTICK);
	isAutoSlowIo	= ini.GetInt(ISAUTOSLOWIO_KEY, TRUE);
	speedLevel		= ini.GetInt(SPEEDLEVEL_KEY, SPEED_FULL);
	alwaysLowIo		= ini.GetInt(ALWAYSLOWIO_KEY, FALSE);
	enableOwdel		= ini.GetInt(OWDEL_KEY, FALSE);
	enableAcl		= ini.GetInt(ACL_KEY, FALSE);
	enableStream	= ini.GetInt(STREAM_KEY, FALSE);
	enableVerify	= ini.GetInt(VERIFY_KEY, FALSE);
	usingMD5		= ini.GetInt(USEMD5_KEY, TRUE);
	enableNSA		= ini.GetInt(NSA_KEY, FALSE);
	delDirWithFilter= ini.GetInt(DELDIR_KEY, FALSE);
	enableMoveAttr	= ini.GetInt(MOVEATTR_KEY, FALSE);
	serialMove		= ini.GetInt(SERIALMOVE_KEY, FALSE);
	serialVerifyMove = ini.GetInt(SERIALVERIFYMOVE_KEY, FALSE);
	isReparse		= ini.GetInt(REPARSE_KEY, TRUE);
	isLinkDest		= ini.GetInt(LINKDEST_KEY, FALSE);
	maxLinkHash		= ini.GetInt(MAXLINKHASH_KEY, DEFAULT_LINKHASH);
	allowContFsize	= ini.GetInt(ALLOWCONTFSIZE_KEY, DEFAULT_ALLOWCONTFSIZE);
	isReCreate		= ini.GetInt(RECREATE_KEY, FALSE);
	isExtendFilter	= ini.GetInt(EXTENDFILTER_KEY, FALSE);
	taskbarMode		= ini.GetInt(TASKBARMODE_KEY, 0);
	infoSpan		= ini.GetInt(INFOSPAN_KEY, DEFAULT_INFOSPAN);
	if (infoSpan < 0 || infoSpan > 2) infoSpan = DEFAULT_INFOSPAN;

	ini.GetStr(WINPOS_KEY, buf, MAX_PATH, "");
	winpos.x   =      (p = strtok(buf,  ", \t")) ? atoi(p) : INVALID_POINTVAL;
	winpos.y   = p && (p = strtok(NULL, ", \t")) ? atoi(p) : INVALID_POINTVAL;
	winsize.cx = p && (p = strtok(NULL, ", \t")) ? atoi(p) : INVALID_SIZEVAL;
	winsize.cy = p && (p = strtok(NULL, ", \t")) ? atoi(p) : INVALID_SIZEVAL;

	ini.GetStr(DRIVEMAP_KEY, driveMap, sizeof(driveMap), "");

/* logfile */
	ini.GetStr(LOGFILE_KEY, buf, MAX_PATH, DEFAULT_FASTCOPYLOG);
	IniStrToV(buf, wbuf);
	if (lstrchrV(wbuf, '\\') == NULL) {
		MakePathV(buf, userDirV, wbuf);
		errLogPathV = strdupV(buf);
	}
	else {
		errLogPathV = strdupV(wbuf);
	}

/* History */
	for (i=0; i < sizeof(section_array) / sizeof(char *); i++) {
		char	*&section = section_array[i];
		void	**&history = *history_array[i];

		ini.SetSection(section);
		history = (void **)calloc(maxHistory, sizeof(WCHAR *));
		for (j=0; j < maxHistory + 30; j++) {
			wsprintf(key, "%d", j);
			if (j < maxHistory) {
				ini.GetStr(key, buf, MAX_HISTORY_CHAR_BUF, "");
				IniStrToV(buf, wbuf);
				history[j] = strdupV(wbuf);
			}
			else if (!ini.DelKey(key))
				break;
		}
	}

/* Job */
	for (i=0; i < JOB_MAX; i++) {
		Job	job;

		wsprintf(buf, FMT_JOB_KEY, i);
		ini.SetSection(buf);

		if (ini.GetStr(TITLE_KEY, buf, MAX_HISTORY_CHAR_BUF) <= 0)
			break;
		IniStrToV(buf, wbuf);
		job.title = strdupV(wbuf);

		ini.GetStr(SRC_KEY, buf, MAX_HISTORY_CHAR_BUF);
		IniStrToV(buf, wbuf);
		job.src = strdupV(wbuf);

		ini.GetStr(DST_KEY, buf, MAX_HISTORY_CHAR_BUF);
		IniStrToV(buf, wbuf);
		job.dst = strdupV(wbuf);

		ini.GetStr(CMD_KEY, buf, MAX_HISTORY_CHAR_BUF);
		IniStrToV(buf, wbuf);
		job.cmd = strdupV(wbuf);

		ini.GetStr(INCLUDE_KEY, buf, MAX_HISTORY_CHAR_BUF);
		IniStrToV(buf, wbuf);
		job.includeFilter = strdupV(wbuf);
		ini.GetStr(EXCLUDE_KEY, buf, MAX_HISTORY_CHAR_BUF);
		IniStrToV(buf, wbuf);
		job.excludeFilter = strdupV(wbuf);

		ini.GetStr(FROMDATE_KEY, buf, MAX_HISTORY_CHAR_BUF);
		IniStrToV(buf, wbuf);
		job.fromDateFilter = strdupV(wbuf);
		ini.GetStr(TODATE_KEY, buf, MAX_HISTORY_CHAR_BUF);
		IniStrToV(buf, wbuf);
		job.toDateFilter = strdupV(wbuf);

		ini.GetStr(MINSIZE_KEY, buf, MAX_HISTORY_CHAR_BUF);
		IniStrToV(buf, wbuf);
		job.minSizeFilter = strdupV(wbuf);
		ini.GetStr(MAXSIZE_KEY, buf, MAX_HISTORY_CHAR_BUF);
		IniStrToV(buf, wbuf);
		job.maxSizeFilter = strdupV(wbuf);

		job.estimateMode = ini.GetInt(ESTIMATE_KEY, 0);
		job.diskMode = ini.GetInt(DISKMODE_KEY, 0);
		job.ignoreErr = ini.GetInt(IGNORE_ERR_KEY, TRUE);
		job.enableOwdel = ini.GetInt(OWDEL_KEY, FALSE);
		job.enableAcl = ini.GetInt(ACL_KEY, FALSE);
		job.enableStream = ini.GetInt(STREAM_KEY, FALSE);
		job.enableVerify = ini.GetInt(VERIFY_KEY, FALSE);
		job.isFilter = ini.GetInt(FILTER_KEY, FALSE);
		job.bufSize = ini.GetInt(BUFSIZE_KEY, DEFAULT_BUFSIZE);

		AddJobV(&job);
	}

/* FinAct */
	for (i=0; i < FINACT_MAX; i++) {
		FinAct	act;

		wsprintf(buf, FMT_FINACT_KEY, i);
		ini.SetSection(buf);

		if (ini.GetStr(TITLE_KEY, buf, MAX_HISTORY_CHAR_BUF) <= 0)
			break;
		IniStrToV(buf, wbuf);
		act.title = strdupV(wbuf);

		ini.GetStr(SOUND_KEY, buf, MAX_HISTORY_CHAR_BUF);
		IniStrToV(buf, wbuf);
		act.sound = strdupV(wbuf);

		ini.GetStr(CMD_KEY, buf, MAX_HISTORY_CHAR_BUF);
		IniStrToV(buf, wbuf);
		act.command = strdupV(wbuf);

		act.flags = ini.GetInt(FLAGS_KEY, 0);

		if (ini.GetStr(SHUTDOWNTIME_KEY, buf, MAX_HISTORY_CHAR_BUF) > 0) {
			act.shutdownTime = strtol(buf, 0, 10);
		}
		AddFinActV(&act);
	}

	if (::GetFileAttributes(ini.GetIniFileName()) == 0xffffffff) {
		WriteIni();
	}
	delete [] wbuf;
	delete [] buf;

	return	TRUE;
}

BOOL Cfg::PostReadIni(void)
{
	DWORD id_list[] = {
		IDS_FINACT_NORMAL, IDS_FINACT_SUSPEND, IDS_FINACT_HIBERNATE, IDS_FINACT_SHUTDOWN, 0
	};
	DWORD flags_list[] = {
		0,				   FinAct::SUSPEND,	   FinAct::HIBERNATE,	 FinAct::SHUTDOWN,    0
	};

	for (int i=0; id_list[i]; i++) {
		if (i >= finActMax) {
			FinAct	act;
			act.flags = flags_list[i] | FinAct::BUILTIN;
			if (i >= 1) act.shutdownTime = 60;
			act.SetString(GetLoadStrV(id_list[i]), L"", L"");
			AddFinActV(&act);
		}
		if (finActArray[i]->flags & FinAct::BUILTIN) {
			free(finActArray[i]->title);
			finActArray[i]->title = strdupV(GetLoadStrV(id_list[i]));
		}
	}

	return	TRUE;
}

BOOL Cfg::WriteIni(void)
{
	ini.StartUpdate();

	ini.SetSection(MAIN_SECTION);
	ini.SetInt(BUFSIZE_KEY, bufSize);
	ini.SetInt(MAXTRANSSIZE_KEY, maxTransSize);
//	ini.SetInt(MAXOPENFILES_KEY, maxOpenFiles);
	ini.SetInt(NONBUFMINSIZENTFS_KEY, nbMinSizeNtfs);
	ini.SetInt(NONBUFMINSIZEFAT_KEY, nbMinSizeFat);
	ini.SetInt(ISREADOSBUF_KEY, isReadOsBuf);
	ini.SetInt(MAX_HISTORY_KEY, maxHistoryNext);
	ini.SetInt(COPYMODE_KEY, copyMode);
//	ini.SetInt(COPYFLAGS_KEY, copyFlags);
	ini.SetInt(SKIPEMPTYDIR_KEY, skipEmptyDir);
	ini.SetInt(IGNORE_ERR_KEY, ignoreErr);
	ini.SetInt(ESTIMATE_KEY, estimateMode);
	ini.SetInt(DISKMODE_KEY, diskMode);
	ini.SetInt(ISTOPLEVEL_KEY, isTopLevel);
	ini.SetInt(ISERRLOG_KEY, isErrLog);
	ini.SetInt(ISUTF8LOG_KEY, isUtf8Log);
	ini.SetInt(FILELOGMODE_KEY, fileLogMode);
	ini.SetInt(ACLERRLOG_KEY, aclErrLog);
	ini.SetInt(STREAMERRLOG_KEY, streamErrLog);
//	ini.SetInt(ISRUNASBUTTON_KEY, isRunasButton);
	ini.SetInt(ISSAMEDIRRENAME_KEY, isSameDirRename);
	ini.SetInt(SHEXTAUTOCLOSE_KEY, shextAutoClose);
	ini.SetInt(SHEXTTASKTRAY_KEY, shextTaskTray);
	ini.SetInt(SHEXTNOCONFIRM_KEY, shextNoConfirm);
	ini.SetInt(SHEXTNOCONFIRMDEL_KEY, shextNoConfirmDel);
	ini.SetInt(EXECCONRIM_KEY, execConfirm);
	ini.SetInt(FORCESTART_KEY, forceStart);
	ini.SetInt(LCID_KEY, lcid);
//	ini.SetInt(WAITTICK_KEY, waitTick);
//	ini.SetInt(ISAUTOSLOWIO_KEY, isAutoSlowIo);
	ini.SetInt(SPEEDLEVEL_KEY, speedLevel);
//	ini.SetInt(ALWAYSLOWIO_KEY, alwaysLowIo);
	ini.SetInt(OWDEL_KEY, enableOwdel);
	ini.SetInt(ACL_KEY, enableAcl);
	ini.SetInt(STREAM_KEY, enableStream);
	ini.SetInt(VERIFY_KEY, enableVerify);
//	ini.SetInt(USEMD5_KEY, usingMD5);
	ini.SetInt(NSA_KEY, enableNSA);
	ini.SetInt(DELDIR_KEY, delDirWithFilter);
	ini.SetInt(MOVEATTR_KEY, enableMoveAttr);
	ini.SetInt(SERIALMOVE_KEY, serialMove);
	ini.SetInt(SERIALVERIFYMOVE_KEY, serialVerifyMove);
	ini.SetInt(REPARSE_KEY, isReparse);
//	ini.SetInt(LINKDEST_KEY, isLinkDest);
//	int.SetInt(MAXLINKHASH_KEY, maxLinkHash);
//	int.SetInt(ALLOWCONTFSIZE_KEY, allowContFsize);
//	ini.SetInt(RECREATE_KEY, isReCreate);
	ini.SetInt(EXTENDFILTER_KEY, isExtendFilter);
	ini.SetInt(TASKBARMODE_KEY, taskbarMode);
	ini.SetInt(INFOSPAN_KEY, infoSpan);

	char	val[256];
	sprintf(val, "%d,%d,%d,%d", winpos.x, winpos.y, winsize.cx, winsize.cy);
	ini.SetStr(WINPOS_KEY, val);
	ini.SetStr(DRIVEMAP_KEY, driveMap);

	char	*section_array[] = {	SRC_HISTORY, DST_HISTORY, DEL_HISTORY,
									INC_HISTORY, EXC_HISTORY,
									FROMDATE_HISTORY, TODATE_HISTORY,
									MINSIZE_HISTORY, MAXSIZE_HISTORY };
	void	***history_array[] = {	&srcPathHistory, &dstPathHistory, &delPathHistory,
									&includeHistory, &excludeHistory,
									&fromDateHistory, &toDateHistory,
									&minSizeHistory, &maxSizeHistory };
	int		i, j;
	char	key[100];
	char	*buf = new char [MAX_HISTORY_CHAR_BUF];

	for (i=0; i < sizeof(section_array) / sizeof(char *); i++) {
		char	*&section = section_array[i];
		void	**&history = *history_array[i];

		ini.SetSection(section);
		for (j=0; j < maxHistory; j++) {
			wsprintf(key, "%d", j);
			VtoIniStr(history[j], buf);
			if (j < maxHistoryNext)
				ini.SetStr(key, buf);
			else
				ini.DelKey(key);
		}
	}

	for (i=0; i < jobMax; i++) {
		wsprintf(buf, FMT_JOB_KEY, i);
		Job *job = jobArray[i];

		ini.SetSection(buf);

		VtoIniStr(job->title, buf);
		ini.SetStr(TITLE_KEY,		buf);

		VtoIniStr(job->src, buf);
		ini.SetStr(SRC_KEY,			buf);

		VtoIniStr(job->dst, buf);
		ini.SetStr(DST_KEY,			buf);

		VtoIniStr(job->cmd, buf);
		ini.SetStr(CMD_KEY,			buf);

		VtoIniStr(job->includeFilter, buf);
		ini.SetStr(INCLUDE_KEY,		buf);
		VtoIniStr(job->excludeFilter, buf);
		ini.SetStr(EXCLUDE_KEY,		buf);

		VtoIniStr(job->fromDateFilter, buf);
		ini.SetStr(FROMDATE_KEY,	buf);
		VtoIniStr(job->toDateFilter, buf);
		ini.SetStr(TODATE_KEY,	buf);

		VtoIniStr(job->minSizeFilter, buf);
		ini.SetStr(MINSIZE_KEY,	buf);
		VtoIniStr(job->maxSizeFilter, buf);
		ini.SetStr(MAXSIZE_KEY,	buf);

		ini.SetInt(ESTIMATE_KEY,	job->estimateMode);
		ini.SetInt(DISKMODE_KEY,	job->diskMode);
		ini.SetInt(IGNORE_ERR_KEY,	job->ignoreErr);
		ini.SetInt(OWDEL_KEY,		job->enableOwdel);
		ini.SetInt(ACL_KEY,			job->enableAcl);
		ini.SetInt(STREAM_KEY,		job->enableStream);
		ini.SetInt(VERIFY_KEY,		job->enableVerify);
		ini.SetInt(FILTER_KEY,		job->isFilter);
		ini.SetInt(BUFSIZE_KEY,		job->bufSize);
	}
	wsprintf(buf, FMT_JOB_KEY, i);
	ini.DelSection(buf);

	for (i=0; i < finActMax; i++) {
		wsprintf(buf, FMT_FINACT_KEY, i);
		FinAct *finAct = finActArray[i];

		ini.SetSection(buf);

		VtoIniStr(finAct->title, buf);
		ini.SetStr(TITLE_KEY,		buf);

		VtoIniStr(finAct->sound, buf);
		ini.SetStr(SOUND_KEY,		buf);

		VtoIniStr(finAct->command, buf);
		ini.SetStr(CMD_KEY,			buf);

		ini.SetInt(SHUTDOWNTIME_KEY,	finAct->shutdownTime);
		ini.SetInt(FLAGS_KEY,			finAct->flags);
	}
	wsprintf(buf, FMT_FINACT_KEY, i);
	ini.DelSection(buf);

	delete [] buf;

	return	ini.EndUpdate();
}

BOOL Cfg::EntryPathHistory(void *src, void *dst)
{
	void	*path_array[] = { src, dst };
	void	***history_array[] = { &srcPathHistory, &dstPathHistory };

	return	EntryHistory(path_array, history_array, 2);
}

BOOL Cfg::EntryDelPathHistory(void *del)
{
	void	*path_array[] = { del };
	void	***history_array[] = { &delPathHistory };

	return	EntryHistory(path_array, history_array, 1);
}

BOOL Cfg::EntryFilterHistory(void *inc, void *exc, void *from, void *to, void *min, void *max)
{
	void	*path_array[] = { inc, exc, from, to, min, max };
	void	***history_array[] = {
		&includeHistory, &excludeHistory, &fromDateHistory, &toDateHistory,
		&minSizeHistory, &maxSizeHistory
	};

	return	EntryHistory(path_array, history_array, 6);
}

BOOL Cfg::EntryHistory(void **path_array, void ****history_array, int max)
{
	BOOL	ret = TRUE;
	void	*target_path;

	for (int i=0; i < max; i++) {
		int		idx;
		void	*&path = path_array[i];
		void	**&history = *history_array[i];

		if (!path || strlenV(path) >= MAX_HISTORY_BUF || GetChar(path, 0) == 0) {
			ret = FALSE;
			continue;
		}
		for (idx=0; idx < maxHistory; idx++) {
			if (lstrcmpiV(path, history[idx]) == 0)
				break;
		}
		if (idx) {
			if (idx == maxHistory) {
				target_path = strdupV(path);
				free(history[--idx]);
			}
			else {
				target_path = history[idx];
			}
			memmove(history + 1, history, idx * sizeof(void *));
			history[0] = target_path;
		}
	}
	return	ret;
}

BOOL Cfg::IniStrToV(char *inipath, void *path)
{
	if (IS_WINNT_V) {
		int		len = (int)strlen(inipath) + 1;
		if (*inipath == '|') {
			hexstr2bin(inipath + 1, (BYTE *)path, len, &len);
		}
		else {
			AtoW(inipath, (WCHAR *)path, len);
		}
	}
	else
		strcpyV(path, inipath);

	return	TRUE;
}

BOOL Cfg::VtoIniStr(void *path, char *inipath)
{
	if (IS_WINNT_V) {
		int		len = (strlenV(path) + 1) * CHAR_LEN_V;
		int		err_cnt = 0;

		*inipath = 0;
		if (!::WideCharToMultiByte(CP_ACP, 0, (WCHAR *)path, -1, inipath, len, 0, &err_cnt))
			return	FALSE;

		if (err_cnt) {
			*inipath = '|';
			bin2hexstr((BYTE *)path, len, inipath + 1);
		}
	}
	else
		strcpyV(inipath, path);

	return	TRUE;
}

int Cfg::SearchJobV(void *title)
{
	for (int i=0; i < jobMax; i++) {
		if (lstrcmpiV(jobArray[i]->title, title) == 0)
			return	i;
	}
	return	-1;
}

BOOL Cfg::AddJobV(const Job *job)
{
	if (GetChar(job->title, 0) == 0
	|| strlenV(job->src) >= MAX_HISTORY_BUF || GetChar(job->src, 0) == 0)
		return	FALSE;

	int idx = SearchJobV(job->title);

	if (idx >= 0) {
		delete jobArray[idx];
		jobArray[idx] = new Job(*job);
		return TRUE;
	}

#define ALLOC_JOB	100
	if ((jobMax % ALLOC_JOB) == 0)
		jobArray = (Job **)realloc(jobArray, sizeof(Job *) * (jobMax + ALLOC_JOB));

	for (idx=0; idx < jobMax; idx++) {
		if (lstrcmpiV(jobArray[idx]->title, job->title) > 0)
			break;
	}
	memmove(jobArray + idx + 1, jobArray + idx, sizeof(Job *) * (jobMax++ - idx));
	jobArray[idx] = new Job(*job);
	return	TRUE;
}

BOOL Cfg::DelJobV(void *title)
{
	int idx = SearchJobV(title);
	if (idx == -1)
		return	FALSE;

	delete jobArray[idx];
	memmove(jobArray + idx, jobArray + idx + 1, sizeof(Job *) * (--jobMax - idx));
	return	TRUE;
}

int Cfg::SearchFinActV(void *title, BOOL is_cmdline)
{
	for (int i=0; i < finActMax; i++) {
		if (lstrcmpiV(finActArray[i]->title, title) == 0)
			return	i;
	}

	if (is_cmdline) {
		int	id_list[] = { IDS_STANDBY, IDS_HIBERNATE, IDS_SHUTDOWN };
		for (int i=0; i < 3; i++) {
			if (lstrcmpiV(GetLoadStrV(id_list[i]), title) == 0) return i + 1;
		}
	}

	return	-1;
}

BOOL Cfg::AddFinActV(const FinAct *finAct)
{
	if (GetChar(finAct->title, 0) == 0)
		return	FALSE;

	int		idx = SearchFinActV(finAct->title);

	if (idx >= 0) {
		DWORD builtin_flag = finActArray[idx]->flags & FinAct::BUILTIN;
		delete finActArray[idx];
		finActArray[idx] = new FinAct(*finAct);
		finActArray[idx]->flags |= builtin_flag;
		return TRUE;
	}

#define ALLOC_FINACT	100
	if ((finActMax % ALLOC_FINACT) == 0)
		finActArray =
			(FinAct **)realloc(finActArray, sizeof(FinAct *) * (finActMax + ALLOC_FINACT));

	finActArray[finActMax++] = new FinAct(*finAct);
	return	TRUE;
}

BOOL Cfg::DelFinActV(void *title)
{
	int idx = SearchFinActV(title);
	if (idx == -1)
		return	FALSE;

	delete finActArray[idx];
	memmove(finActArray + idx, finActArray + idx + 1, sizeof(FinAct *) * (--finActMax - idx));
	return	TRUE;
}

