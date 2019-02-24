/* static char *cfg_id = 
	"@(#)Copyright (C) 2005-2018 H.Shirouzu		cfg.h	Ver3.41"; */
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Create					: 2005-01-23(Sun)
	Update					: 2018-01-27(Sat)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	======================================================================== */

#ifndef CFG_H
#define CFG_H
#include "tlib/tlib.h"
#include "resource.h"

struct Job {
	WCHAR	*title;
	WCHAR	*src;
	WCHAR	*dst;
	WCHAR	*cmd;
	int		estimateMode;
	int		diskMode;
	BOOL	ignoreErr;
	BOOL	enableOwdel;
	BOOL	enableAcl;
	BOOL	enableStream;
	BOOL	enableVerify;
	BOOL	isFilter;
	WCHAR	*includeFilter;
	WCHAR	*excludeFilter;
	WCHAR	*fromDateFilter;
	WCHAR	*toDateFilter;
	WCHAR	*minSizeFilter;
	WCHAR	*maxSizeFilter;

	void Init() {
		memset(this, 0, sizeof(Job));
	}
	void SetString(WCHAR *_title, WCHAR *_src, WCHAR *_dst, WCHAR *_cmd,
					WCHAR *_includeFilter, WCHAR *_excludeFilter,
					WCHAR *_fromDateFilter, WCHAR *_toDateFilter,
					WCHAR *_minSizeFilter, WCHAR *_maxSizeFilter) {
		title = wcsdup(_title);
		src = wcsdup(_src);
		dst = wcsdup(_dst);
		cmd = wcsdup(_cmd);
		includeFilter = wcsdup(_includeFilter);
		excludeFilter = wcsdup(_excludeFilter);
		fromDateFilter = wcsdup(_fromDateFilter);
		toDateFilter = wcsdup(_toDateFilter);
		minSizeFilter = wcsdup(_minSizeFilter);
		maxSizeFilter = wcsdup(_maxSizeFilter);
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
	Job(const Job& org) {
		*this = org;
	}
	~Job() { UnSet(); }
	Job& operator=(const Job &org) {
		Init();
		Set(&org);
		return *this;
	}
};

struct FinAct {
	WCHAR	*title;
	WCHAR	*sound;
	WCHAR	*command;
	int		shutdownTime;
	DWORD	flags;
	enum {	BUILTIN=0x1, ERR_SOUND=0x2, ERR_CMD=0x4, ERR_SHUTDOWN=0x8, WAIT_CMD=0x10,
			FORCE=0x20, SUSPEND=0x40, HIBERNATE=0x80, SHUTDOWN=0x100, NORMAL_CMD=0x200, };

	void Init() {
		memset(this, 0, sizeof(FinAct));
		shutdownTime = -1;
	}
	void SetString(WCHAR *_title, WCHAR *_sound, WCHAR *_command) {
		title = wcsdup(_title);
		sound = wcsdup(_sound);
		command = wcsdup(_command);
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
	FinAct(const FinAct& org) {
		*this = org;
	}
	~FinAct() { UnSet(); }
	FinAct& operator=(const FinAct& org) {
		Init();
		Set(&org);
		return *this;
	}
};

class Cfg {
protected:
	TInifile	ini;
	BOOL Init(WCHAR *user_dir, WCHAR *virtual_dir);
	BOOL IniStrToW(char *str, WCHAR *wstr);
	BOOL EntryHistory(WCHAR **path_array, WCHAR ****history_array, int max);
	BOOL GetFilterStr(const char *key, char *tmpbuf, WCHAR *wbuf);

public:
	int		iniVersion;
	int		bufSize;		// MB
	int		maxRunNum;
	int		maxOvlSize;		// MB
	int		maxOvlNum;
	int		maxOpenFiles;
	int		maxAttrSize;	// MB
	int		maxDirSize;		// MB
	int		maxMoveSize;	// MB
	int		maxDigestSize;	// MB
	int		minSectorSize;
	int64	nbMinSizeNtfs;
	int64	nbMinSizeFat;
	int64	timeDiffGrace;
	BOOL	isReadOsBuf;
	BOOL	isWShareOpen;
	int		maxHistory;
	int		maxHistoryNext;
	int		copyMode;
	int64	copyFlags;
	int64	copyUnFlags;
	int		skipEmptyDir;	// 0:no, 1:filter-mode only, 2:always
	int		forceStart;		// 0:delete only, 1:always(copy+delete), 2:always wait
	BOOL	ignoreErr;
	int		estimateMode;
	int		diskMode;
	int		netDrvMode;
	int		aclReset;
	int		lcid;
	int		waitLv;
	int		speedLevel;
	BOOL	isAutoSlowIo;
	int		priority;
	BOOL	enableOwdel;
	BOOL	enableAcl;
	BOOL	enableStream;
	BOOL	enableVerify;
	BOOL	noSacl;

	BOOL	useOverlapIo;
	BOOL	usingMD5;
	enum HashMode { MD5, SHA1, SHA256, SHA512, XXHASH32, XXHASH } hashMode;
	BOOL	enableNSA;
	BOOL	delDirWithFilter;
	BOOL	enableMoveAttr;
	BOOL	serialMove;
	BOOL	serialVerifyMove;
	BOOL	isReparse;
	BOOL	isLinkDest;
	int		maxLinkHash;
	int64	allowContFsize;
	BOOL	isReCreate;
	BOOL	isExtendFilter;
	int		taskbarMode; // 0: use tray, 1: use taskbar
	int		finishNotify;
	int		finishNotifyTout;
	BOOL	preventSleep;

	int		infoSpan;	// information update timing (0:250msec, 1:500msec, 2:1000sec)
	BOOL	isTopLevel;
	BOOL	isErrLog;
	BOOL	isUtf8Log;
	int		fileLogMode;
	int		fileLogFlags;	// 0: None, 1: with timestamp, 2: with size, 3 both
	BOOL	aclErrLog;
	BOOL	streamErrLog;
	int		debugFlags;
	int		debugMainFlags;
	int		testMode;
	BOOL	isRunasButton;
	BOOL	isSameDirRename;
	BOOL	dlsvtMode; // 0: none, 1: fat, 2: always
	BOOL	largeFetch;
	BOOL	dirSel;
	BOOL	updCheck;
	time_t	lastUpdCheck;
	BOOL	verifyRemove;
	BOOL	verifyInfo;

	struct ShExtCfg {
		BOOL	autoClose;
		BOOL	taskTray;
		BOOL	noConfirm;
		BOOL	noConfirmDel;
	};
	ShExtCfg	shAdmin;
	ShExtCfg	shUser;

	BOOL	execConfirm;
	WCHAR	**srcPathHistory;
	WCHAR	**dstPathHistory;
	WCHAR	**delPathHistory;
	WCHAR	**includeHistory;
	WCHAR	**excludeHistory;
	WCHAR	**fromDateHistory;
	WCHAR	**toDateHistory;
	WCHAR	**minSizeHistory;
	WCHAR	**maxSizeHistory;
	WCHAR	*execPath;
	WCHAR	*execDir;
	WCHAR	*userDir;
	WCHAR	*virtualDir;
	WCHAR	*errLogPath;	// UNICODE
	Job		**jobArray;
	int		jobMax;
	FinAct  **finActArray;
	int		finActMax;
	POINT	winpos;
	SIZE	winsize;
	char	driveMap[64];
	WCHAR	statusFont[LF_FACESIZE];
	int		statusFontSize;
	DynBuf	officialPub;

	BOOL	needIniConvert;

	Cfg();
	~Cfg();
	BOOL ReadIni(WCHAR *user_dir, WCHAR *virtual_dir);
	BOOL PostReadIni(void);
	BOOL WriteIni(void);
	BOOL EntryPathHistory(WCHAR *src, WCHAR *dst);
	BOOL EntryDelPathHistory(WCHAR *del);
	BOOL EntryFilterHistory(WCHAR *inc, WCHAR *exc, WCHAR *from, WCHAR *to, WCHAR *min, WCHAR *max);

	int SearchJobW(const WCHAR *title);
	BOOL AddJobW(const Job *job);
	BOOL DelJobW(const WCHAR *title);

	int SearchFinActW(const WCHAR *title, BOOL cmd_line=FALSE);
	BOOL AddFinActW(const FinAct *job);
	BOOL DelFinActW(const WCHAR *title);
};

#define INVALID_POINTVAL	-10000
#define INVALID_SIZEVAL		-10000

#define IS_INVALID_POINT(pt) (pt.x == INVALID_POINTVAL && pt.y == INVALID_POINTVAL)
#define IS_INVALID_SIZE(sz) (sz.cx == INVALID_SIZEVAL  && sz.cy == INVALID_SIZEVAL)


// default param for cfg.cpp and setup.cpp
#define FINISH_NOTIFY_DEFAULT	20

#endif
