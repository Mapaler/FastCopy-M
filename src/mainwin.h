/* static char *mainwin_id = 
	"@(#)Copyright (C) 2004-2018 H.Shirouzu		mainwin.h	Ver3.50"; */
/* ========================================================================
	Project  Name			: Fast Copy file and directory
	Create					: 2004-09-15(Wed)
	Update					: 2018-05-28(Mon)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	======================================================================== */

#ifndef MAINWIN_H
#define MAINWIN_H

#pragma warning(disable:4355)

#include "tlib/tlib.h"
#include "fastcopy.h"
#include "cfg.h"
#include "setupdlg.h"
#include "miscdlg.h"
#include "version.h"
#include <stddef.h>

#include "mainwinopt.h"

#ifdef _WIN64
#define FASTCOPY_TITLE		"FastCopy(64bit)"
#else
#define FASTCOPY_TITLE		"FastCopy"
#endif
#define FASTCOPY_CLASS		"fastcopy_class"

#define WM_FASTCOPY_MSG				(WM_APP + 100)
#define WM_FASTCOPY_NOTIFY			(WM_APP + 101)
#define WM_FASTCOPY_HIDDEN			(WM_APP + 102)
#define WM_FASTCOPY_RUNAS			(WM_APP + 103)
#define WM_FASTCOPY_STATUS			(WM_APP + 104)
#define WM_FASTCOPY_KEY				(WM_APP + 105)
#define WM_FASTCOPY_PATHHISTCLEAR	(WM_APP + 106)
#define WM_FASTCOPY_FILTERHISTCLEAR	(WM_APP + 107)
#define WM_FASTCOPY_UPDINFORES		(WM_APP + 120)
#define WM_FASTCOPY_UPDDLRES		(WM_APP + 121)
#define WM_FASTCOPY_RESIZESRCEDIT	(WM_APP + 122)
#define WM_FASTCOPY_SRCEDITFIT		(WM_APP + 123)
#define WM_FASTCOPY_FOCUS			(WM_APP + 124) // for setupdlg
#define WM_FASTCOPY_TRAYTEMP		(WM_APP + 125) // for setupdlg - mainwin
#define WM_FASTCOPY_TRAYSETUP		(WM_APP + 126)
#define WM_FASTCOPY_PLAYFIN			(WM_APP + 127)
#define WM_FASTCOPY_POSTSETUP		(WM_APP + 128)

#define FASTCOPY_TIMER			100
#define FASTCOPY_FIN_TIMER		110
#define FASTCOPY_PAINT_TIMER	120

#define FASTCOPY_NIM_ID			100

#define FASTCOPYLOG_MUTEX	"FastCopyLogMutex"

#define MAX_HISTORY_BUF		8192
#define MAX_HISTORY_CHAR_BUF (MAX_HISTORY_BUF * 4)

#define MINI_BUF 128
#define MAX_SRCEDITCR 10

#define SHELLEXT_MIN_ALLOC		(16 * 1024)
#ifdef _WIN64
#define SHELLEXT_MAX_ALLOC		(1024 * 1024 * 1024)
#else
#define SHELLEXT_MAX_ALLOC		(64 * 1024 * 1024)
#endif

struct CopyInfo {
	UINT				resId;
	char				*list_str;
	WCHAR				*cmdline_name;
	FastCopy::Mode		mode;
	FastCopy::OverWrite	overWrite;
};

#define FCNORM_ICON_IDX				0
#define FCNORM2_ICON_IDX			1
#define FCNORM3_ICON_IDX			2
#define FCNORM4_ICON_IDX			3
#define FCDONE_ICON_IDX				4
#define FCWAIT_ICON_IDX				5
#define FCERR_ICON_IDX				6
#define MAX_FASTCOPY_ICON			(FCERR_ICON_IDX + 1)
#define MAX_FCNORM_ICON				(FCNORM4_ICON_IDX + 1)

#define SPEED_FULL		11
#define SPEED_AUTO		10
#define SPEED_SUSPEND	0

// SDK 7.0A用Richedit.h に存在しない。SDK10に完全移行したら削除
#ifndef AURL_ENABLEURL
#define AURL_ENABLEURL			1
#define AURL_ENABLEEMAILADDR	2
#define AURL_ENABLETELNO		4
#define AURL_ENABLEEAURLS		8
#define AURL_ENABLEDRIVELETTERS	16
#endif

struct UpdateData {
	U8str			ver;
	U8str			path;
	int64			size;
	DynBuf			hash;
	DynBuf			dlData;
	IPDictStrList	sites;

	UpdateData() {
		Init();
	}
	void Init() {
		DataInit();
	}
	void DataInit() {
		ver.Init();
		path.Init();
		size = 0;
		hash.Free();
		dlData.Free();
		sites.clear();
	}
};

class TMainDlg : public TDlg {
protected:
	enum AutoCloseLevel { NO_CLOSE, NOERR_CLOSE, FORCE_CLOSE };
	enum { NORMAL_EXEC=1, LISTING_EXEC=2, CMDLINE_EXEC=4 };
	enum { RUNAS_IMMEDIATE=1, RUNAS_SHELLEXT=2, RUNAS_AUTOCLOSE=4 };
	enum { READ_LVIDX, WRITE_LVIDX, VERIFY_LVIDX, SKIP_LVIDX, DEL_LVIDX, OVWR_LVIDX };
	enum FileLogMode { NO_FILELOG, AUTO_FILELOG, FIX_FILELOG };
	enum DebugMainFlags { GSHACK_SKIP=1 };

	FastCopy		fastCopy;
	FastCopy::Info	info;

	int			orgArgc;
	WCHAR		**orgArgv;
	Cfg			cfg;
	HICON		hMainIcon[MAX_FASTCOPY_ICON];
	HICON		hPauseIcon;
	HICON		hPlayIcon;
	CopyInfo	*copyInfo;
	int			finActIdx;
	int			doneRatePercent;
	int			lastTotalSec;
	int			calcTimes;
	BOOL		isAbort;

	BOOL		captureMode;
	int			lastYPos;
	int			dividYPos;

/* share to runas */
	AutoCloseLevel autoCloseLevel;
	enum { SS_HIDE=0, SS_NORMAL=1, SS_MINMIZE=2, SS_TRAY=4, SS_TRAYMIN=6, SS_TEMP=8 };
	int			showState;
	int			speedLevel;
	BOOL		isPause;
	int			finishNotify;

	BOOL		noConfirmDel;
	BOOL		noConfirmStop;
	UINT		diskMode;
	enum		{ SHELL_NONE, SHELL_ADMIN, SHELL_USER } shellMode;
	BOOL		isInstaller;
	BOOL		isNetPlaceSrc;
	int			skipEmptyDir;
	int			forceStart;
	WCHAR		errLogPath[MAX_PATH];
	WCHAR		fileLogPath[MAX_PATH];
	WCHAR		lastFileLog[MAX_PATH];
	BOOL		isErrLog;
	BOOL		isUtf8Log;
	FileLogMode	fileLogMode;
	BOOL		isListLog;
	BOOL		isReparse;
	BOOL		isLinkDest;
	int			maxLinkHash;
	int			maxTempRunNum;	// 0以外は /force_start=N での一時制限
	BOOL		isReCreate;
	BOOL		isExtendFilter;
	BOOL		resultStatus;
	BOOL		isNoUI;
	int			dlsvtMode; // 0: none, 1: fat, 2: always

	BOOL		shextNoConfirm;
	BOOL		shextNoConfirmDel;
	BOOL		shextTaskTray;
	BOOL		shextAutoClose;

/* end of share to runas */

	BOOL		isRunAsStart;
	BOOL		isRunAsParent;

	void *RunasShareData() { return (void*)&autoCloseLevel; }
	DWORD RunasShareSize() {
		return offsetof(TMainDlg, isRunAsStart) - offsetof(TMainDlg, autoCloseLevel);
	}
	int			MaxRunNum() { return maxTempRunNum ? maxTempRunNum : cfg.maxRunNum; }

	// Register to dlgItems in SetSize()
	enum {	srcbutton_item=0, srcedit_item, dstbutton_item, dstcombo_item,
			status_item, mode_item, bufstatic_item, bufedit_item,
			ignore_item, estimate_item, verify_item, list_item, pauselist_item,
			ok_item, pauseok_item, atonce_item,
			owdel_item, acl_item, stream_item, speed_item, speedstatic_item, samedrv_item,
			incstatic_item, filterhelp_item, excstatic_item, inccombo_item, exccombo_item,
			filter_item,
			fdatestatic_item, todatestatic_item, minsizestatic_item, maxsizestatic_item,
			fdatecombo_item,  todatecombo_item,  minsizecombo_item,  maxsizecombo_item,
			path_item, errstatic_item, errstatus_item, erredit_item, max_dlgitem };

	int			listBufOffset;
	int			errBufOffset;
	UINT		TaskBarCreateMsg;
	int			miniHeight;
	int			normalHeight;
	int			normalHeightOrg;
	int			filterHeight;
	BOOL		isErrEditHide;
	UINT		curIconIdx;
	BOOL		isDelay;

	SYSTEMTIME	startTm;
	DWORD		endTick;

	char		*pathLogBuf;
	HANDLE		hErrLog;
	HANDLE		hErrLogMutex;
	HANDLE		hFileLog;
	TransInfo	ti;

// for detect autoslow status
	DWORD		timerCnt;
	DWORD		timerLast;
	POINT		curPt;
	HWND		curForeWnd;
	DWORD		curPriority;

#ifdef USE_LISTVIEW
//	TListHeader		listHead;
//	TListViewEx		listView;
#endif

	TAboutDlg		aboutDlg;
	TSetupDlg		setupDlg;
	TJobDlg			jobDlg;
	TFinActDlg		finActDlg;
	TSrcEdit		srcEdit;
	TEditSub		pathEdit;
	TEditSub		errEdit;
	TSubClassCtl	bufEdit;
	TSubClassCtl	statEdit;
	TSubClassCtl	speedSlider;
	TSubClassCtl	speedStatic;
	TSubClassCtl	listBtn;
	TSubClassCtl	pauseOkBtn;
	TSubClassCtl	pauseListBtn;
	HFONT			statusFont;
	LOGFONTW		statusLogFont;
	ITaskbarList3	*taskbarList;

	UpdateData		updData;

protected:
	BOOL	SetCopyModeList(void);
	BOOL	IsForeground();
	BOOL	MoveCenter();
	BOOL	SetupWindow();
	void	StatusEditSetup(BOOL reset=FALSE);
	void	ChooseFont();
	void	PostSetup();
	BOOL	ExecCopy(DWORD exec_flags);
	BOOL	ExecCopyCore(void);
	BOOL	EndCopy(void);
	BOOL	ExecFinalAction(BOOL is_sound_wait);
	BOOL	CancelCopy(void);
	void	SetItemEnable(FastCopy::Mode mode);
	FastCopy::Mode	GetCopyMode(void);
	void	SetExtendFilter();
	void	ReflectFilterCheck(BOOL is_invert=FALSE);
	BOOL	SwapTarget(BOOL check_only=FALSE);
	BOOL	SwapTargetCore(const WCHAR *s, const WCHAR *d, WCHAR *out_s, WCHAR *out_d);
	void	SetSize(void);
	void	UpdateMenu();
	void	SetJob(int idx);
	BOOL	IsListing() { return (info.flags & FastCopy::LISTING_ONLY) ? TRUE : FALSE; }
	void	SetPriority(DWORD new_class);
	DWORD	UpdateSpeedLevel(BOOL is_timer=FALSE);
	void	SetUserAgent();
#ifdef USE_LISTVIEW
	int		ListViewIdx(int idx);
	BOOL	SetListViewItem(int idx, int subIdx, char *txt);
#endif
	void	SetListInfo();
	void	SetFileLogInfo();
	BOOL	SetTaskTrayInfo(BOOL is_finish_status, double doneRate, int remain_sec);
	BOOL	CalcInfo(double *doneRate, int *remain_sec, int *total_sec);

	BOOL	WriteErrLogCore();
	void	WriteErrLogAtStart();
	void	WriteErrLogNormal();
	void	WriteErrLogNoUI(const char *msg);

	void	WriteLogHeader(HANDLE hFile, BOOL add_filelog=TRUE);
	BOOL	WriteLogFooter(HANDLE hFile);
	BOOL	StartFileLog();
	void	EndFileLog();
	BOOL	CheckVerifyExtension();
	void	UpdateCheck(BOOL is_silent=FALSE);
	void	UpdateCheckRes(TInetReqReply *_irr);
	void	UpdateDlRes(TInetReqReply *_irr);
	void	UpdateExec();

	void	GetSeparateArea(RECT *sep_rc);
	BOOL	IsSeparateArea(int x, int y);
	void	ShowHistMenu();
	void	SetHistPath(int idx);
	void	ResizeForSrcEdit(int diff_y);
	void	SetStatMargin();

public:
	TMainDlg();
	virtual ~TMainDlg();

	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvDestroy(void);
	virtual BOOL	EvNcDestroy(void);
	virtual BOOL	EvTimer(WPARAM timerID, TIMERPROC proc);
	virtual BOOL	EvSysCommand(WPARAM uCmdType, POINTS pos);

	virtual BOOL	EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight);
	virtual BOOL	EvSetCursor(HWND cursorWnd, WORD nHitTest, WORD wMouseMsg);
	virtual BOOL	EventButton(UINT uMsg, int nHitTest, POINTS pos);
	virtual BOOL	EvMouseMove(UINT fwKeys, POINTS pos);

	virtual BOOL	EvDropFiles(HDROP hDrop);
	virtual BOOL	EvActivateApp(BOOL fActivate, DWORD dwThreadID);
	virtual BOOL	EventScroll(UINT uMsg, int nCode, int nPos, HWND scrollBar);
	virtual BOOL	EvNotify(UINT ctlID, NMHDR *pNmHdr);

/*
	virtual BOOL	EvEndSession(BOOL nSession, BOOL nLogOut);
	virtual BOOL	EvQueryOpen(void);
	virtual BOOL	EvHotKey(int hotKey);
	virtual BOOL	EventButton(UINT uMsg, int nHitTest, POINTS pos);
*/
	virtual BOOL	EventInitMenu(UINT uMsg, HMENU hMenu, UINT uPos, BOOL fSystemMenu);
	virtual BOOL	EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual BOOL	EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual BOOL	EventSystem(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual BOOL	EventCtlColor(UINT uMsg, HDC hDcCtl, HWND hWndCtl, HBRUSH *result);

	virtual void	Show(int mode = SW_SHOWDEFAULT);
	virtual	int		MessageBox(LPCSTR msg, LPCSTR title="msg", UINT style=MB_OK);
	virtual	int		MessageBoxW(LPCWSTR msg, LPCWSTR title=L"", UINT style=MB_OK);
	virtual	int		MessageBoxU8(LPCSTR msg, LPCSTR title="msg", UINT style=MB_OK);

	BOOL	RunAsAdmin(DWORD flg = 0);
	BOOL	EnableErrLogFile(BOOL on);
	int		CmdNameToComboIndex(const WCHAR *cmd_name);

	BOOL	GetRunasInfo(WCHAR **user_dir, WCHAR **virtual_dir);
	BOOL	CommandLineExecW(int argc, WCHAR **argv);
	BOOL	RunasSync(HWND hOrg);

	BOOL	SetMiniWindow(void);
	BOOL	SetNormalWindow();
	void	RefreshWindow(BOOL is_start_stop=FALSE);
	BOOL	SetWindowTitle();
	BOOL	IsDestDropFiles(HDROP hDrop);
	int64	GetDateInfo(WCHAR *buf, BOOL is_end);
	int64	GetSizeInfo(WCHAR *buf);
	BOOL	SetInfo(BOOL is_finish_status=FALSE);
	enum	SetHistMode { SETHIST_LIST, SETHIST_EDIT, SETHIST_CLEAR };
	void	SetComboBox(UINT item, WCHAR **history, SetHistMode mode);
	void	SetPathHistory(SetHistMode mode, UINT item=0);
	void	SetFilterHistory(SetHistMode mode, UINT item=0);
	void	SetIcon(int idx=0);
	BOOL	TaskTray(int nimMode, int idx=0, LPCSTR tip=NULL, BOOL balloon=FALSE);
	void	TaskTrayTemp(BOOL on);
	CopyInfo *GetCopyInfo() { return copyInfo; }
	void	SetFinAct(int idx);
	int		GetFinActIdx() { return finActIdx; }
	BOOL	GetRunningCount(ShareInfo::CheckInfo *ci, uint64 use_drives=0) {
				return	fastCopy.GetRunningCount(ci, use_drives);
			}
};

class TFastCopyApp : public TApp {
public:
	TFastCopyApp(HINSTANCE _hI, LPSTR _cmdLine, int _nCmdShow);
	virtual ~TFastCopyApp();

	virtual void	InitWindow(void);
	virtual BOOL	PreProcMsg(MSG *msg);
};

int inline wcsicmpEx(WCHAR *s1, WCHAR *s2, int *len)
{
	*len = (int)wcslen(s2);
	return	wcsnicmp(s1, s2, *len);
}

#define FASTCOPY_SITE			"fastcopy.jp"
#define FASTCOPY_UPDATEINFO		"fastcopy-update.dat"
#ifdef _WIN64
#define UPDATE_FILENAME			UPDATE64_FILENAME
#else
#define UPDATE_FILENAME			UPDATE32_FILENAME
#endif
#define UPDATE32_FILENAME		"fastcopy_upd32.exe"
#define UPDATE64_FILENAME		"fastcopy_upd64.exe"

#endif

