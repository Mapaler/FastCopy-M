/* static char *mainwin_id = 
	"@(#)Copyright (C) 2004-2015 H.Shirouzu		mainwin.h	Ver3.10"; */
/* ========================================================================
	Project  Name			: Fast Copy file and directory
	Create					: 2004-09-15(Wed)
	Update					: 2015-11-29(Sun)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	Modify					: Mapaler 2015-09-16
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
#define FASTCOPY_TITLE		"FastCopy-M(64bit)"
#else
#define FASTCOPY_TITLE		"FastCopy-M"
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
	WCHAR				*cmdline_name;
	FastCopy::Mode		mode;
	FastCopy::OverWrite	overWrite;
};

//#ifdef _WinXP
//#define MAX_NORMAL_FASTCOPY_ICON	6 //动态图标个数
//#endif
//因为define的意义是编译时替换代码中文字而非产生const常量，因此不需要修改也可以使用。
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
	WCHAR			**orgArgv;
	Cfg				cfg;
//#ifdef _WinXP
//	HICON			hMainIcon[MAX_FASTCOPY_ICON];
//#else
	int				MAX_NORMAL_FASTCOPY_ICON; //动态图标个数
	HICON			*hMainIcon;
//#endif
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
	BOOL		isShellExt;
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
	BOOL		isReCreate;
	BOOL		isExtendFilter;
	BOOL		resultStatus;
	BOOL		isNoUI;

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

	// Register to dlgItems in SetSize()
	enum {	srcbutton_item=0, dstbutton_item, srccombo_item, dstcombo_item,
			status_item, mode_item, bufstatic_item, bufedit_item, help_item,
			ignore_item, estimate_item, verify_item, top_item, list_item, ok_item, atonce_item,
			owdel_item, acl_item, stream_item, speed_item, speedstatic_item, samedrv_item,
			incstatic_item, filterhelp_item, excstatic_item, inccombo_item, exccombo_item,
			filter_item,
//			fromdate_static, todate_static, minsize_static, maxsize_static,
//			fromdate_combo, todate_combo, minsize_combo, maxsize_combo,
			path_item, errstatic_item, errstatus_item, erredit_item, max_dlgitem };

	int			listBufOffset;
	int			errBufOffset;
	UINT		TaskBarCreateMsg;
	int			miniHeight;
	int			normalHeight;
	int			normalHeightOrg;
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
	HFONT			statusFont;
	LOGFONTW		statusLogFont;

protected:
	BOOL	SetCopyModeList(void);
	BOOL	IsForeground();
	BOOL	MoveCenter();
	BOOL	SetupWindow();
	void	StatusEditSetup(BOOL reset=FALSE);
	void	ChooseFont();
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
	BOOL	SwapTargetCore(const WCHAR *s, const WCHAR *d, WCHAR *out_s, WCHAR *out_d);
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
//	virtual BOOL	EvNotify(UINT ctlID, NMHDR *pNmHdr);

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
	BOOL	TaskTray(int nimMode, HICON hSetIcon=NULL, LPCSTR tip=NULL, BOOL balloon=FALSE);
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

#endif

