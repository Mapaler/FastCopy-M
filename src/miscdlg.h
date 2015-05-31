/* static char *miscdlg_id = 
	"@(#)Copyright (C) 2005-2012 H.Shirouzu		miscdlg.h	Ver2.10"; */
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Create					: 2005-01-23(Sun)
	Update					: 2012-06-17(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
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
	const void	*message;
public:
	TConfirmDlg(UINT _resId=CONFIRM_DIALOG) : TDlg(_resId) {}
	int Exec(const void *_message, BOOL _allow_continue, TWin *_parent);
	virtual BOOL	EvCreate(LPARAM lParam);
};

class TExecConfirmDlg : public TDlg {
	const void		*src;
	const void		*dst;
	const void		*title;
	Cfg				*cfg;
	FastCopy::Info	*info;
	BOOL			isShellExt;

public:
	TExecConfirmDlg(FastCopy::Info *_info, Cfg *_cfg, TWin *_parent, const void *_title=NULL,
		BOOL _isShellExt=FALSE)
		: TDlg(_info->mode == FastCopy::DELETE_MODE ? DELCONFIRM_DIALOG : COPYCONFIRM_DIALOG,
		_parent), cfg(_cfg), info(_info), title(_title), isShellExt(_isShellExt) {}
	int Exec(const void *_src, const void *_dst=NULL);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl);
	virtual BOOL	EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight);
};

class TSetupSheet : public TDlg {
protected:
	Cfg	*cfg;

public:
	TSetupSheet() { cfg = NULL; }
	BOOL Create(int resid, Cfg *_cfg, TWin *_parent);
	BOOL GetData();
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EventScroll(UINT uMsg, int Code, int nPos, HWND hwndScrollBar);
};

#define MAX_SETUP_SHEET	6
class TSetupDlg : public TDlg {
	Cfg			*cfg;
	TSubClass	setup_list;
	TSetupSheet	sheet[MAX_SETUP_SHEET];

public:
	TSetupDlg(Cfg *_cfg, TWin *_parent = NULL);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvCreate(LPARAM lParam);
	void	SetSheet(int idx=-1);
};

class ShellExt {
	HMODULE	hShellExtDll;

public:
	ShellExt() { hShellExtDll = NULL; }
	~ShellExt() { if (hShellExtDll) UnLoad(); }
	BOOL	Load(void *parent_dir, void *dll_name);
	BOOL	UnLoad(void);
	BOOL	Status(void) { return	hShellExtDll ? TRUE : FALSE; }
	HRESULT	(WINAPI *RegisterDllProc)(void);
	HRESULT	(WINAPI *UnRegisterDllProc)(void);
	BOOL	(WINAPI *IsRegisterDllProc)(void);
	BOOL	(WINAPI *SetMenuFlagsProc)(int);
	int		(WINAPI *GetMenuFlagsProc)(void);
	BOOL	(WINAPI *UpdateDllProc)(void);
};

class TShellExtDlg : public TDlg {
	Cfg			*cfg;
	ShellExt	shellExt;

public:
	TShellExtDlg(Cfg *_cfg, TWin *_parent = NULL);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvNcDestroy(void);

	BOOL	RegisterShellExt(BOOL is_register);
	BOOL	ReflectStatus(void);
};

enum	DirFileMode { DIRSELECT, RELOAD, FILESELECT, SELECT_EXIT };

class TBrowseDirDlgV : public TSubClass
{
public:
	DirFileMode mode;

protected:
	void		*fileBuf;
	IMalloc		*iMalloc;
	BROWSEINFO	brInfo;		// W version との差は、文字ポインタ系メンバの型の違いのみ
	int			flg;

public:
	TBrowseDirDlgV(void *title, void *_fileBuf, int _flg, TWin *parentWin);
	virtual ~TBrowseDirDlgV();
	virtual BOOL	AttachWnd(HWND _hWnd);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	SetFileBuf(LPARAM list);
	virtual BOOL	Exec();
	DirFileMode GetMode(void) { return mode; };
	BOOL	GetParentDirV(void *srcfile, void *dir);
	static int CALLBACK BrowseDirDlg_Proc(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM data);
};

#define BRDIR_MULTIPATH		0x0001
#define BRDIR_CTRLADD		0x0002
#define BRDIR_BACKSLASH		0x0004
#define BRDIR_FILESELECT	0x0008
BOOL BrowseDirDlgV(TWin *parentWin, UINT editCtl, void *title, int flg=0);

class TInputDlgV : public TDlg
{
protected:
	void	*dirBuf;

public:
	TInputDlgV(void *_dirBuf, TWin *_win) : TDlg(INPUT_DIALOG, _win) { dirBuf = _dirBuf; }
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
	virtual BOOL Exec(void *target, void *title=NULL, void *filter=NULL, void *defaultDir=NULL);
	virtual BOOL Exec(UINT editCtl, void *title=NULL, void *filter=NULL, void *defaultDir=NULL,
					void *init_data=NULL);

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
	void	*msg;
	void 	*title;
	UINT	style;
	BOOL	isExecV;
	enum { msg_item, ok_item, cancel_item, max_dlgitem };

public:
	TMsgBox(TWin *_parent);
	~TMsgBox();

	virtual BOOL EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL EvCreate(LPARAM lParam);
	virtual BOOL EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight);
	virtual UINT ExecV(void *_msg, void *_title=L"Error", UINT _style=MB_OK);
	virtual UINT Exec(char *_msg, char *_title="Error", UINT _style=MB_OK);
};

class TFinDlg : public TMsgBox {
protected:
	int			sec;
	const char	*fmt;

public:
	TFinDlg(TWin *_parent);
	~TFinDlg();

	virtual UINT Exec(int sec, DWORD _fmtID);
	virtual BOOL EvCreate(LPARAM lParam);
	virtual BOOL EvTimer(WPARAM timerID, TIMERPROC proc);
	void UpdateText();
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
	virtual int		ExGetText(void *buf, int max_len, DWORD flags=GT_USECRLF, UINT codepage=CP_UTF8);
	virtual BOOL	ExSetText(const void *buf, int max_len=-1, DWORD flags=ST_DEFAULT, UINT codepage=CP_UTF8);

	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl);
	virtual BOOL	EvContextMenu(HWND childWnd, POINTS pos);
};


int SetSpeedLevelLabel(TDlg *dlg, int level=-1);

#endif
