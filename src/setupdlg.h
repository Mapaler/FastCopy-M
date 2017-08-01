/* static char *setupdlg_id = 
	"@(#)Copyright (C) 2015-2016 H.Shirouzu		setupdlg.h	Ver3.25"; */
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Create					: 2015-07-17(Fri)
	Update					: 2016-10-19(Wed)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	======================================================================== */

#ifndef SETUPDLG_H
#define SETUPDLG_H
#include "tlib/tlib.h"
#include "resource.h"
#include "cfg.h"

class TSetupDlg;

struct SheetDefSv {
	int		bufSize;
	int		estimateMode;
	BOOL	ignoreErr;
	BOOL	enableVerify;
	BOOL	enableAcl;
	BOOL	enableStream;
	int		speedLevel;
	BOOL	isExtendFilter;
	BOOL	enableOwdel;
};

class TSetupSheet : public TDlg {
protected:
	Cfg			*cfg;
	TSetupDlg	*setupDlg;
	SheetDefSv	*sv;

public:
	TSetupSheet() { cfg = NULL; sv = NULL; }
	~TSetupSheet() { if (sv) free(sv); }
	BOOL Create(int resid, Cfg *_cfg, TSetupDlg *_parent);
	BOOL	CheckData();
	BOOL	SetData();
	BOOL	GetData();
	void	ReflectToMainWindow();
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EventScroll(UINT uMsg, int Code, int nPos, HWND hwndScrollBar);
	virtual	BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl);
};

#define MAIN_SHEET     SETUP_SHEET1
#define IO_SHEET       SETUP_SHEET2
#define PHYSDRV_SHEET  SETUP_SHEET3
#define PARALLEL_SHEET SETUP_SHEET4
#define COPYOPT_SHEET  SETUP_SHEET5
#define DEL_SHEET      SETUP_SHEET6
#define LOG_SHEET      SETUP_SHEET7
#define MISC_SHEET     SETUP_SHEET8
#define MAX_SETUP_SHEET	(SETUP_SHEET8 - SETUP_SHEET1 + 1)

class TSetupDlg : public TDlg {
	Cfg			*cfg;
	int			curIdx;
	TSubClass	setup_list;
	TSetupSheet	sheet[MAX_SETUP_SHEET];

public:
	TSetupDlg(Cfg *_cfg, TWin *_parent = NULL);
	void		SetSheet(int idx=-1);
	TSetupSheet	*GetSheet(int idx) { return &sheet[idx - SETUP_SHEET1]; }

	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvCreate(LPARAM lParam);
};

class ShellExt {
	HMODULE	hShDll;
	BOOL	isAdmin;

public:
	ShellExt(BOOL _isAdmin) { hShDll = NULL; isAdmin = _isAdmin; }
	~ShellExt() { if (hShDll) UnLoad(); }
	BOOL	Load(const WCHAR *dll_name);
	BOOL	UnLoad(void);
	BOOL	Status(void) { return	hShDll ? TRUE : FALSE; }
	HRESULT	(WINAPI *RegisterDllProc)(void);
	HRESULT	(WINAPI *UnRegisterDllProc)(void);
	HRESULT	(WINAPI *RegisterDllUserProc)(void);
	HRESULT	(WINAPI *UnRegisterDllUserProc)(void);
	BOOL	(WINAPI *IsRegisterDllProc)(BOOL);
	BOOL	(WINAPI *SetMenuFlagsProc)(BOOL, int);
	int		(WINAPI *GetMenuFlagsProc)(BOOL);
	BOOL	(WINAPI *SetAdminModeProc)(BOOL);
};

class TShellExtDlg : public TDlg {
	Cfg				*cfg;
	Cfg::ShExtCfg	*shCfg;
	ShellExt		shellExt;
	BOOL			isAdmin;

public:
	TShellExtDlg(Cfg *_cfg, BOOL _isAdmin, TWin *_parent = NULL);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvNcDestroy(void);

	BOOL	RegisterShellExt(BOOL is_register);
	BOOL	ReflectStatus(void);
};

#endif
