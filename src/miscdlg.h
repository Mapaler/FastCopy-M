/* static char *miscdlg_id = 
	"@(#)Copyright (C) 2005-2015 H.Shirouzu		miscdlg.h	Ver3.00"; */
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Create					: 2005-01-23(Sun)
	Update					: 2015-08-12(Wed)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	======================================================================== */

#ifndef MISCDLG_H
#define MISCDLG_H
#include "tlib/tlib.h"
#include "resource.h"
#include "cfg.h"

class TAboutDlg : public TDlg {
public:
	TAboutDlg(TWin *_parent = NULL);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvCreate(LPARAM lParam);
};

class TConfirmDlg : public TDlg {
	BOOL		allow_continue;
	const WCHAR	*message;
public:
	TConfirmDlg(UINT _resId=CONFIRM_DIALOG) : TDlg(_resId) {}
	int Exec(const WCHAR *_message, BOOL _allow_continue, TWin *_parent);
	virtual BOOL	EvCreate(LPARAM lParam);
};

class TExecConfirmDlg : public TDlg {
	const WCHAR		*src;
	const WCHAR		*dst;
	const WCHAR		*title;
	Cfg				*cfg;
	FastCopy::Info	*info;
	BOOL			isShellExt;

public:
	TExecConfirmDlg(FastCopy::Info *_info, Cfg *_cfg, TWin *_parent, const WCHAR *_title=NULL,
		BOOL _isShellExt=FALSE)
		: TDlg(_info->mode == FastCopy::DELETE_MODE ? DELCONFIRM_DIALOG : COPYCONFIRM_DIALOG,
		_parent), cfg(_cfg), info(_info), title(_title), isShellExt(_isShellExt) {}
	int Exec(const WCHAR *_src, const WCHAR *_dst=NULL);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl);
	virtual BOOL	EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight);
};

enum	DirFileMode { DIRSELECT, RELOAD, FILESELECT, SELECT_EXIT };

class TBrowseDirDlgW : public TSubClass
{
public:
	DirFileMode mode;

protected:
	WCHAR		*fileBuf;
	IMalloc		*iMalloc;
	BROWSEINFOW	brInfo;		// W version との差は、文字ポインタ系メンバの型の違いのみ
	int			flg;

public:
	TBrowseDirDlgW(WCHAR *title, WCHAR *_fileBuf, int _flg, TWin *parentWin);
	virtual ~TBrowseDirDlgW();
	virtual BOOL	AttachWnd(HWND _hWnd);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	SetFileBuf(LPARAM list);
	virtual BOOL	Exec();
	DirFileMode GetMode(void) { return mode; };
	BOOL	GetParentDirW(WCHAR *srcfile, WCHAR *dir);
	static int CALLBACK BrowseDirDlg_Proc(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM data);
};

#define BRDIR_MULTIPATH		0x0001
#define BRDIR_CTRLADD		0x0002
#define BRDIR_BACKSLASH		0x0004
#define BRDIR_FILESELECT	0x0008
BOOL BrowseDirDlgW(TWin *parentWin, UINT editCtl, WCHAR *title, int flg=0);

class TInputDlg : public TDlg
{
protected:
	WCHAR	*dirBuf;

public:
	TInputDlg(WCHAR *_dirBuf, TWin *_win) : TDlg(INPUT_DIALOG, _win) { dirBuf = _dirBuf; }
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
};

#define OFDLG_DIRSELECT		0x0001
#define OFDLG_WITHQUOTE		0x0002

class TOpenFileDlg : public TSubClass {
public:
	enum	OpenMode { OPEN, MULTI_OPEN, SAVE, NODEREF_SAVE };
	int		flg;
	DirFileMode mode;

protected:
	TWin			*parent;
	LPOFNHOOKPROC	hook;
	OpenMode		openMode;

public:
	TOpenFileDlg(TWin *_parent, OpenMode _openMode=OPEN, int _flg=0, LPOFNHOOKPROC _hook=NULL) {
		parent = _parent; hook = _hook; openMode = _openMode; flg = _flg;
	}
	static UINT WINAPI OpenFileDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);
	virtual BOOL AttachWnd(HWND _hWnd);
	virtual BOOL EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL Exec(WCHAR *target, WCHAR *title=NULL, WCHAR *filter=NULL, WCHAR *defaultDir=NULL);
	virtual BOOL Exec(UINT editCtl, WCHAR *title=NULL, WCHAR *filter=NULL, WCHAR *defaultDir=NULL,
						WCHAR *init_data=NULL);

	DirFileMode GetMode(void) { return mode; };
};

class TMainDlg;

class TJobDlg : public TDlg {
	Cfg			*cfg;
	TMainDlg	*mainParent;

public:
	TJobDlg(Cfg *cfg, TMainDlg *_parent = NULL);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvCreate(LPARAM lParam);
	BOOL AddJob();
	BOOL DelJob();
};

class TFinActDlg : public TDlg {
protected:
	Cfg			*cfg;
	TMainDlg	*mainParent;

public:
	TFinActDlg(Cfg *cfg, TMainDlg *_parent = NULL);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvCreate(LPARAM lParam);
	BOOL	AddFinAct();
	BOOL	DelFinAct();
	BOOL	Reflect(int idx);
};

class TMsgBox : public TDlg {
protected:
	const WCHAR	*msg;
	const WCHAR	*title;
	UINT		style;
	BOOL		isExecW;
	enum { msg_item, ok_item, cancel_item, max_dlgitem };

public:
	TMsgBox(TWin *_parent);
	virtual ~TMsgBox();

	virtual BOOL EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL EvCreate(LPARAM lParam);
	virtual BOOL EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight);
	virtual UINT ExecW(const WCHAR *_msg, const WCHAR *_title=L"Error", UINT _style=MB_OK);
	virtual UINT Exec(const char *_msg, const char *_title="Error", UINT _style=MB_OK);
};

class TFinDlg : public TMsgBox {
protected:
	int			sec;
	int			orgSec;
	const char	*main_msg;
	const char	*proc_msg;
	const char	*time_fmt;
	TMainDlg	*mainDlg;

public:
	TFinDlg(TMainDlg *_parent);
	virtual ~TFinDlg();

	virtual UINT Exec(int sec, DWORD mainmsg_id);
	virtual BOOL EvCreate(LPARAM lParam);
	virtual BOOL EvTimer(WPARAM timerID, TIMERPROC proc);
	void Update();
};

class TListHeader : public TSubClassCtl {
protected:
	LOGFONT	logFont;

public:
	TListHeader(TWin *_parent);
	virtual BOOL	AttachWnd(HWND _hWnd);
	virtual BOOL	EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual BOOL	ChangeFontNotify(void);
};

class TListViewEx : public TSubClassCtl {
protected:
	int		focus_index;

public:
	TListViewEx(TWin *_parent);

	int		GetFocusIndex(void) { return focus_index; }
	void	SetFocusIndex(int index) { focus_index = index; }

	virtual BOOL	EventFocus(UINT uMsg, HWND hFocusWnd);
	virtual BOOL	EventButton(UINT uMsg, int nHitTest, POINTS pos);
	virtual BOOL	EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class TEditSub : public TSubClassCtl {
public:
	TEditSub(TWin *_parent);

	virtual BOOL	AttachWnd(HWND _hWnd);
	virtual int		ExGetText(WCHAR *buf, int max_len, DWORD flags=GT_USECRLF, UINT codepage=CP_UTF8);
	virtual BOOL	ExSetText(const WCHAR *buf, int max_len=-1, DWORD flags=ST_DEFAULT, UINT codepage=CP_UTF8);

	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl);
	virtual BOOL	EvContextMenu(HWND childWnd, POINTS pos);
};


int SetSpeedLevelLabel(TWin *win, int level=-1);

#endif
