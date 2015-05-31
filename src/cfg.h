/* static char *cfg_id = 
	"@(#)Copyright (C) 2005-2012 H.Shirouzu		cfg.h	Ver2.10"; */
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Create					: 2005-01-23(Sun)
	Update					: 2012-06-17(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef CFG_H
#define CFG_H
#include "tlib/tlib.h"
#include "resource.h"

struct Job {
	void	*title;
	void	*src;
	void	*dst;
	void	*cmd;
	int		bufSize;
	int		estimateMode;
	int		diskMode;
	BOOL	ignoreErr;
	BOOL	enableOwdel;
	BOOL	enableAcl;
	BOOL	enableStream;
	BOOL	enableVerify;
	BOOL	isFilter;
	void	*includeFilter;
	void	*excludeFilter;
	void	*fromDateFilter;
	void	*toDateFilter;
	void	*minSizeFilter;
	void	*maxSizeFilter;

	void Init() {
		memset(this, 0, sizeof(Job));
	}
	void SetString(void *_title, void *_src, void *_dst, void *_cmd,
					void *_includeFilter, void *_excludeFilter,
					void *_fromDateFilter, void *_toDateFilter,
					void *_minSizeFilter, void *_maxSizeFilter) {
		title = strdupV(_title);
		src = strdupV(_src);
		dst = strdupV(_dst);
		cmd = strdupV(_cmd);
		includeFilter = strdupV(_includeFilter);
		excludeFilter = strdupV(_excludeFilter);
		fromDateFilter = strdupV(_fromDateFilter);
		toDateFilter = strdupV(_toDateFilter);
		minSizeFilter = strdupV(_minSizeFilter);
		maxSizeFilter = strdupV(_maxSizeFilter);
	}
	void Set(const Job *job) {
		memcpy(this, job, sizeof(Job));
		SetString(job->title, job->src, job->dst, job->cmd,
					job->includeFilter, job->excludeFilter,
					job->fromDateFilter, job->toDateFilter,
					job->minSizeFilter, job->maxSizeFilter);
	}
	void UnSet() {
		free(excludeFilter);
		free(includeFilter);
		free(fromDateFilter);
		free(toDateFilter);
		free(minSizeFilter);
		free(maxSizeFilter);
		free(cmd);
		free(dst);
		free(src);
		free(title);
		Init();
	}

	Job() {
		Init();
	}
	Job(const Job& job) {
		Init();
		Set(&job);
	}
	~Job() { UnSet(); }
};

struct FinAct {
	void	*title;
	void	*sound;
	void	*command;
	int		shutdownTime;
	DWORD	flags;
	enum {	BUILTIN=0x1, ERR_SOUND=0x2, ERR_CMD=0x4, ERR_SHUTDOWN=0x8, WAIT_CMD=0x10,
			FORCE=0x20, SUSPEND=0x40, HIBERNATE=0x80, SHUTDOWN=0x100, NORMAL_CMD=0x200, };

	void Init() {
		memset(this, 0, sizeof(FinAct));
		shutdownTime = -1;
	}
	void SetString(void *_title, void *_sound, void *_command) {
		title = strdupV(_title);
		sound = strdupV(_sound);
		command = strdupV(_command);
	}
	void Set(const FinAct *finAct) {
		memcpy(this, finAct, sizeof(FinAct));
		SetString(finAct->title, finAct->sound, finAct->command);
	}
	void UnSet() {
		free(command);
		free(sound);
		free(title);
		Init();
	}
	FinAct() {
		Init();
	}
	FinAct(const FinAct& action) {
		Init();
		Set(&action);
	}
	~FinAct() { UnSet(); }
};

class Cfg {
protected:
	TInifile	ini;
	BOOL Init(void *user_dir, void *virtual_dir);
	BOOL IniStrToV(char *inipath, void *path);
	BOOL VtoIniStr(void *path, char *inipath);
	BOOL EntryHistory(void **path_array, void ****history_array, int max);

public:
	int		bufSize;
	int		maxTransSize;
	int		maxOpenFiles;
	int		maxAttrSize;
	int		maxDirSize;
	int		nbMinSizeNtfs;
	int		nbMinSizeFat;
	BOOL	isReadOsBuf;
	int		maxHistory;
	int		maxHistoryNext;
	int		copyMode;
	int		copyFlags;
	int		skipEmptyDir;	// 0:no, 1:filter-mode only, 2:always
	int		forceStart;		// 0:delete only, 1:always(copy+delete), 2:always wait
	BOOL	ignoreErr;
	int		estimateMode;
	int		diskMode;
	int		lcid;
	int		waitTick;
	int		speedLevel;
	BOOL	isAutoSlowIo;
	BOOL	alwaysLowIo;
	BOOL	enableOwdel;
	BOOL	enableAcl;
	BOOL	enableStream;
	BOOL	enableVerify;
	BOOL	usingMD5;
	BOOL	enableNSA;
	BOOL	delDirWithFilter;
	BOOL	enableMoveAttr;
	BOOL	serialMove;
	BOOL	serialVerifyMove;
	BOOL	isReparse;
	BOOL	isLinkDest;
	int		maxLinkHash;
	_int64	allowContFsize;
	BOOL	isReCreate;
	BOOL	isExtendFilter;
	int		taskbarMode; // 0: use tray, 1: use taskbar
	int		infoSpan;	// information update timing (0:250msec, 1:500msec, 2:1000sec)
	BOOL	isTopLevel;
	BOOL	isErrLog;
	BOOL	isUtf8Log;
	int		fileLogMode;
	BOOL	aclErrLog;
	BOOL	streamErrLog;
	BOOL	isRunasButton;
	BOOL	isSameDirRename;
	BOOL	shextAutoClose;
	BOOL	shextTaskTray;
	BOOL	shextNoConfirm;
	BOOL	shextNoConfirmDel;
	BOOL	execConfirm;
	void	**srcPathHistory;
	void	**dstPathHistory;
	void	**delPathHistory;
	void	**includeHistory;
	void	**excludeHistory;
	void	**fromDateHistory;
	void	**toDateHistory;
	void	**minSizeHistory;
	void	**maxSizeHistory;
	void	*execPathV;
	void	*execDirV;
	void	*userDirV;
	void	*virtualDirV;
	void	*errLogPathV;	// UNICODE
	Job		**jobArray;
	int		jobMax;
	FinAct  **finActArray;
	int		finActMax;
	POINT	winpos;
	SIZE	winsize;
	char	driveMap[64];

	Cfg();
	~Cfg();
	BOOL ReadIni(void *user_dir, void *virtual_dir);
	BOOL PostReadIni(void);
	BOOL WriteIni(void);
	BOOL EntryPathHistory(void *src, void *dst);
	BOOL EntryDelPathHistory(void *del);
	BOOL EntryFilterHistory(void *inc, void *exc, void *from, void *to, void *min, void *max);

	int SearchJobV(void *title);
	BOOL AddJobV(const Job *job);
	BOOL DelJobV(void *title);

	int SearchFinActV(void *title, BOOL cmd_line=FALSE);
	BOOL AddFinActV(const FinAct *job);
	BOOL DelFinActV(void *title);
};

#define INVALID_POINTVAL	-10000
#define INVALID_SIZEVAL		-10000

#define IS_INVALID_POINT(pt) (pt.x == INVALID_POINTVAL && pt.y == INVALID_POINTVAL)
#define IS_INVALID_SIZE(sz) (sz.cx == INVALID_SIZEVAL  && sz.cy == INVALID_SIZEVAL)


#endif
