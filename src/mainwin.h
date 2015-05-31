/* static char *fastcopy_id = 
	"@(#)Copyright (C) 2004-2012 H.Shirouzu		mainwin.h	Ver2.11"; */
/* ========================================================================
	Project  Name			: Fast Copy file and directory
	Create					: 2004-09-15(Wed)
	Update					: 2012-06-18(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef MAINWIN_H
#define MAINWIN_H

#pragma warning(disable:4355)

#include "tlib/tlib.h"
#include "fastcopy.h"
#include "cfg.h"
#include "miscdlg.h"
#include "version.h"
#include <stddef.h>


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

#define FASTCOPY_TIMER		100
#define FASTCOPY_NIM_ID		100
#define FASTCOPY_FIN_TIMER	110

#define FASTCOPYLOG_MUTEX	"FastCopyLogMutex"

#define MAX_HISTORY_BUF		8192
#define MAX_HISTORY_CHAR_BUF (MAX_HISTORY_BUF * 4)

#define MINI_BUF 128

#define SHELLEXT_MIN_ALLOC		(16 * 1024)
#define SHELLEXT_MAX_ALLOC		(4 * 1024 * 1024)

struct CopyInfo {
	UINT				resId;
	char				*list_str;
	UINT				cmdline_resId;
	void				*cmdline_name;
	FastCopy::Mode		mode;
	FastCopy::OverWrite	overWrite;
};

#define MAX_NORMAL_FASTCOPY_ICON	4
#define FCNORMAL_ICON_IDX			0
#define FCWAIT_ICON_IDX				MAX_NORMAL_FASTCOPY_ICON
#define MAX_FASTCOPY_ICON			MAX_NORMAL_FASTCOPY_ICON + 1

#define SPEED_FULL		11
#define SPEED_AUTO		10
#define SPEED_SUSPEND	0

class TMainDlg : public TDlg {
protected:
	enum AutoCloseLevel { NO_CLOSE, NOERR_CLOSE, FORCE_CLOSE };
	enum { NORMAL_EXEC=1, LISTING_EXEC=2, CMDLINE_EXEC=4 };
	enum { RUNAS_IMMEDIATE=1, RUNAS_SHELLEXT=2, RUNAS_AUTOCLOSE=4 };
	enum { READ_LVIDX, WRITE_LVIDX, VERIFY_LVIDX, SKIP_LVIDX, DEL_LVIDX, OVWR_LVIDX };
	enum FileLogMode { NO_FILELOG, AUTO_FILELOG, FIX_FILELOG };

	FastCopy		fastCopy;
	FastCopy::Info	info;

	int				orgArgc;
	void			**orgArgv;
	Cfg				cfg;
	HICON			hMainIcon[MAX_FASTCOPY_ICON];
	CopyInfo		*copyInfo;
	int				finActIdx;
	int				doneRatePercent;
	int				lastTotalSec;
	int				calcTimes;
	BOOL			isAbort;

/* share to runas */
	AutoCloseLevel autoCloseLevel;
	BOOL		isTaskTray;
	BOOL		speedLevel;

	BOOL		noConfirmDel;
	BOOL		noConfirmStop;
	UINT		diskMode;
	BOOL		isRegExp;
	BOOL		isShellExt;
	BOOL		isInstaller;
	BOOL		isNetPlaceSrc;
	int			skipEmptyDir;
	int			forceStart;
	WCHAR		errLogPathV[MAX_PATH];
	WCHAR		fileLogPathV[MAX_PATH];
	BOOL		isErrLog;
	BOOL		isUtf8Log;
	FileLogMode	fileLogMode;
	BOOL		isReparse;
	BOOL		isLinkDest;
	int			maxLinkHash;
	BOOL		isReCreate;
	BOOL		isExtendFilter;
	BOOL		resultStatus;

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

	enum {	srcbutton_item=0, dstbutton_item, srccombo_item, dstcombo_item,
			status_item, mode_item, bufstatic_item, bufedit_item, help_item,
			ignore_item, estimate_item, verify_item, top_item, list_item, ok_item, atonce_item,
			owdel_item, acl_item, stream_item, speed_item, speedstatic_item, samedrv_item,
			incstatic_item, excstatic_item, inccombo_item, exccombo_item, filter_item,
//			fromdate_static, todate_static, minsize_static, maxsize_static,
//			fromdate_combo, todate_combo, minsize_combo, maxsize_combo,
			path_item, errstatic_item, errstatus_item, erredit_item, max_dlgitem };

	int			listBufOffset;
	int			errBufOffset;
	UINT		TaskBarCreateMsg;
	int			miniHeight;
	int			normalHeight;
	int			filterHeight;
	BOOL		isErrEditHide;
	UINT		curIconIndex;
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
	TShellExtDlg	shellExtDlg;
	TJobDlg			jobDlg;
	TFinActDlg		finActDlg;
	TEditSub		pathEdit;
	TEditSub		errEdit;

protected:
	BOOL	SetCopyModeList(void);
	BOOL	IsForeground();
	BOOL	MoveCenter();
	BOOL	SetupWindow();
	BOOL	ExecCopy(DWORD exec_flags);
	BOOL	ExecCopyCore(void);
	BOOL	EndCopy(void);
	BOOL	ExecFinalAction(BOOL is_sound_wait);
	BOOL	CancelCopy(void);
	void	SetItemEnable(BOOL is_delete);
	FastCopy::Mode	GetCopyMode(void);
	void	SetExtendFilter();
	void	ReflectFilterCheck(BOOL is_invert=FALSE);
	BOOL	SwapTarget(BOOL check_only=FALSE);
	BOOL	SwapTargetCore(const void *s, const void *d, void *out_s, void *out_d);
	void	SetSize(void);
	void	UpdateMenu();
	void	SetJob(int idx);
	BOOL	IsListing() { return (info.flags & FastCopy::LISTING_ONLY) ? TRUE : FALSE; }
	void	SetPriority(DWORD new_class);
	DWORD	UpdateSpeedLevel(BOOL is_timer=FALSE);
#ifdef USE_LISTVIEW
	int		ListViewIdx(int idx);
	BOOL	SetListViewItem(int idx, int subIdx, char *txt);
#endif
	void	SetListInfo();
	void	SetFileLogInfo();
	BOOL	SetTaskTrayInfo(BOOL is_finish_status, double doneRate, int remain_h, int remain_m,
			int remain_s);
	BOOL	CalcInfo(double *doneRate, int *remain_sec, int *total_sec);
	void	WriteLogHeader(HANDLE hFile, BOOL add_filelog=TRUE);
	BOOL	WriteLogFooter(HANDLE hFile);
	BOOL	WriteErrLog(BOOL is_initerr=FALSE);
	BOOL	CheckVerifyExtension();
	BOOL	StartFileLog();
	void	EndFileLog();

public:
	TMainDlg();
	virtual ~TMainDlg();

	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvNcDestroy(void);
	virtual BOOL	EvTimer(WPARAM timerID, TIMERPROC proc);
	virtual BOOL	EvSysCommand(WPARAM uCmdType, POINTS pos);
	virtual BOOL	EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight);
	virtual BOOL	EvDropFiles(HDROP hDrop);
	virtual BOOL	EvActivateApp(BOOL fActivate, DWORD dwThreadID);
	virtual BOOL	EventScroll(UINT uMsg, int nCode, int nPos, HWND scrollBar);
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
	virtual void	Show(int mode = SW_SHOWDEFAULT);

	BOOL	RunAsAdmin(DWORD flg = 0);
	BOOL	EnableErrLogFile(BOOL on);
	int		CmdNameToComboIndex(void *cmd_name);

	BOOL	GetRunasInfo(WCHAR **user_dir, WCHAR **virtual_dir);
	BOOL	CommandLineExecV(int argc, void **argv);
	BOOL	RunasSync(HWND hOrg);

	BOOL	SetMiniWindow(void);
	BOOL	SetNormalWindow(void);
	void	RefreshWindow(BOOL is_start_stop=FALSE);
	BOOL	SetWindowTitle();
	BOOL	IsDestDropFiles(HDROP hDrop);
	_int64	GetDateInfo(void *buf, BOOL is_end);
	_int64	GetSizeInfo(void *buf);
	BOOL	SetInfo(BOOL is_finish_status=FALSE);
	enum	SetHistMode { SETHIST_LIST, SETHIST_EDIT, SETHIST_CLEAR };
	void	SetComboBox(UINT item, void **history, SetHistMode mode);
	void	SetPathHistory(SetHistMode mode, UINT item=0);
	void	SetFilterHistory(SetHistMode mode, UINT item=0);
	BOOL	TaskTray(int nimMode, HICON hSetIcon=NULL, LPCSTR tip=NULL);
	CopyInfo *GetCopyInfo() { return copyInfo; }
	void	SetFinAct(int idx);
	int		GetFinActIdx() { return finActIdx; }

};

class TFastCopyApp : public TApp {
public:
	TFastCopyApp(HINSTANCE _hI, LPSTR _cmdLine, int _nCmdShow);
	virtual ~TFastCopyApp();

	virtual void	InitWindow(void);
	virtual BOOL	PreProcMsg(MSG *msg);
};

#endif

