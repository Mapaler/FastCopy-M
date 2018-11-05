/* static char *setupdlg_id = 
	"@(#)Copyright (C) 2015-2018 H.Shirouzu		setupdlg.h	Ver3.50"; */
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Create					: 2015-07-17(Fri)
	Update					: 2018-05-28(Mon)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	======================================================================== */

#ifndef SETUPDLG_H
#define SETUPDLG_H
#include "tlib/tlib.h"
#include "resource.h"
#include "cfg.h"
#include "TLib/tgdiplus.h"

class TSetupDlg;

struct SheetDefSv {
	int		estimateMode = 0;
	BOOL	ignoreErr = FALSE;
	BOOL	enableVerify = FALSE;
	BOOL	enableAcl = FALSE;
	BOOL	enableStream = FALSE;
	int		speedLevel = FALSE;
	BOOL	isExtendFilter = FALSE;
	BOOL	enableOwdel = FALSE;
};

class ShellExt;

class TSetupSheet : public TDlg {
protected:
	Cfg			*cfg;
	TSetupDlg	*setupDlg;
	SheetDefSv	*sv;
	std::unique_ptr<ShellExt> shext;
	std::unique_ptr<TGifWin>  gwin;
	BOOL		initDone;

public:
	TSetupSheet() { cfg = NULL; sv = NULL; }
	~TSetupSheet() { if (sv) free(sv); }
	BOOL Create(int resid, Cfg *_cfg, TSetupDlg *_parent);
	BOOL	CheckData();
	BOOL	SetData();
	BOOL	GetData();
	void	ReflectToMainWindow();
	void	EnableDlgItems(BOOL flg, const std::vector<int> &except_ids);

	virtual	void	Show(int mode);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvNcDestroy();
	virtual	BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl);
	virtual BOOL	EventScroll(UINT uMsg, int Code, int nPos, HWND hwndScrollBar);
	virtual BOOL	EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

#define MAIN_SHEET     SETUP_SHEET1
#define IO_SHEET       SETUP_SHEET2
#define PHYSDRV_SHEET  SETUP_SHEET3
#define PARALLEL_SHEET SETUP_SHEET4
#define COPYOPT_SHEET  SETUP_SHEET5
#define DEL_SHEET      SETUP_SHEET6
#define LOG_SHEET      SETUP_SHEET7
#define SHELLEXT_SHEET SETUP_SHEET8
#define TRAY_SHEET     SETUP_SHEET9
#define MISC_SHEET     SETUP_SHEET10
#define MAX_SETUP_SHEET	(MISC_SHEET - MAIN_SHEET + 1)

inline int SheetToIdx(int sheet) { return (sheet - SETUP_SHEET1); }

class TSetupDlg : public TDlg {
	Cfg			*cfg;
	int			curIdx;
	int			nextIdx;
	TSubClass	setup_list;
	TSetupSheet	sheet[MAX_SETUP_SHEET];
	TRect		pRect;

public:
	TSetupDlg(Cfg *_cfg, TWin *_parent = NULL);
	void		SetSheetIdx(int idx) { nextIdx = idx - SETUP_SHEET1; }
	void		SetSheet();
	TSetupSheet	*GetSheet(int idx) { return &sheet[idx - SETUP_SHEET1]; }

	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvNcDestroy();
	virtual BOOL	EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class ShellExt {
	HMODULE			hShDll = NULL;
	BOOL			isAdmin = FALSE;
	BOOL			isModify = FALSE;
	Cfg				*cfg = NULL;
	Cfg::ShExtCfg	*shCfg = NULL;
	TSetupSheet		*parent = NULL;

public:
	ShellExt(Cfg *_cfg, TSetupSheet *_parent);
	~ShellExt();

	BOOL	Init();
	BOOL	Load(const WCHAR *dll_name);
	BOOL	UnLoad(void);
	BOOL	Status(void) { return	hShDll ? TRUE : FALSE; }

	HRESULT	(WINAPI *RegisterDllProc)(void) = NULL;
	HRESULT	(WINAPI *UnRegisterDllProc)(void) = NULL;
	HRESULT	(WINAPI *RegisterDllUserProc)(void) = NULL;
	HRESULT	(WINAPI *UnRegisterDllUserProc)(void) = NULL;
	BOOL	(WINAPI *IsRegisterDllProc)(BOOL) = NULL;
	BOOL	(WINAPI *SetMenuFlagsProc)(BOOL, int) = NULL;
	int		(WINAPI *GetMenuFlagsProc)(BOOL) = NULL;
	BOOL	(WINAPI *SetAdminModeProc)(BOOL) = NULL;

	BOOL	Command(WORD wNotifyCode, WORD wID, LPARAM hWndCtl);
	BOOL	RegisterShellExt();
	BOOL	ReflectStatus(void);
};

#endif
