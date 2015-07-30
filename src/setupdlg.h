/* static char *setupdlg_id = 
	"@(#)Copyright (C) 2015 H.Shirouzu		setupdlg.h	Ver3.00"; */
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Create					: 2015-07-17(Fri)
	Update					: 2015-07-17(Fri)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	======================================================================== */

#ifndef SETUPDLG_H
#define SETUPDLG_H
#include "tlib/tlib.h"
#include "resource.h"
#include "cfg.h"

class TSetupSheet : public TDlg {
protected:
	Cfg	*cfg;

public:
	TSetupSheet() { cfg = NULL; }
	BOOL Create(int resid, Cfg *_cfg, TWin *_parent);
	BOOL	CheckData();
	BOOL	SetData();
	BOOL	GetData();
//	void	ReflectDisp();
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
	void	SetSheet(int idx=-1);

	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvCreate(LPARAM lParam);
};

class ShellExt {
	HMODULE	hShellExtDll;

public:
	ShellExt() { hShellExtDll = NULL; }
	~ShellExt() { if (hShellExtDll) UnLoad(); }
	BOOL	Load(WCHAR *parent_dir, WCHAR *dll_name);
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

#endif
