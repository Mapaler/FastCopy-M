/* @(#)Copyright (C) 2005-2012 H.Shirouzu		install.h	Ver2.10 */
/* ========================================================================
	Project  Name			: Installer for IPMSG32
	Module Name				: Main Header
	Create					: 2005-02-02(Wed)
	Update					: 2012-06-17(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

enum InstMode { SETUP_MODE, UNINSTALL_MODE };

struct InstallCfg {
	InstMode	mode;
	BOOL		programLink;
	BOOL		desktopLink;
	BOOL		runImme;
	HWND		hOrgWnd;
	WCHAR		*setupDir;
	WCHAR		*appData;
	WCHAR		*virtualDir;
	void		*startMenu;
	void		*deskTop;
};

class TInstSheet : public TDlg
{
	InstallCfg	*cfg;

public:
	TInstSheet(TWin *_parent, InstallCfg *_cfg);

	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);

	void	Paste(void);
	void	GetData(void);
	void	PutData(void);
};

class TInstDlg : public TDlg
{
protected:
	TSubClassCtl	staticText;
	TInstSheet		*propertySheet;
	InstallCfg		cfg;

public:
	TInstDlg(char *cmdLine);
	virtual ~TInstDlg();

	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
#if 0
	virtual BOOL	EvNcDestroy(void);
	virtual BOOL	EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif
	BOOL	Install(void);
	BOOL	UnInstall(void);
	BOOL	RunAsAdmin(BOOL is_imme);
	void	ChangeMode(void);
	BOOL	RemoveSameLink(const char *dir, char *remove_path=NULL);
};

class TInstApp : public TApp
{
public:
	TInstApp(HINSTANCE _hI, LPSTR _cmdLine, int _nCmdShow);
	virtual ~TInstApp();

	void InitWindow(void);
};

class TBrowseDirDlg : public TSubClass
{
protected:
	char	*fileBuf;
	BOOL	dirtyFlg;

public:
	TBrowseDirDlg(char *_fileBuf) { fileBuf = _fileBuf; }
	virtual BOOL	AttachWnd(HWND _hWnd);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	SetFileBuf(LPARAM list);
	BOOL	IsDirty(void) { return dirtyFlg; };
};

class TInputDlg : public TDlg
{
protected:
	char	*dirBuf;

public:
	TInputDlg(char *_dirBuf, TWin *_win) : TDlg(INPUT_DIALOG, _win) { dirBuf = _dirBuf; }
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
};

class TLaunchDlg : public TDlg
{
protected:
	char *msg;

public:
	TLaunchDlg(LPCSTR _msg, TWin *_win);
	virtual ~TLaunchDlg();

	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
};


#define FASTCOPY			"FastCopy"
#define FASTCOPY_EXE		"FastCopy.exe"
#define INSTALL_EXE			"setup.exe"
#define README_TXT			"readme.txt"
#define HELP_CHM			"FastCopy.chm"
#define SHELLEXT1_DLL		"FastCopy_shext.dll"
#define SHELLEXT2_DLL		"FastCopy_shext2.dll"
#define SHELLEXT3_DLL		"FastCopy_shext3.dll"
#define SHELLEXT4_DLL		"FastCopy_shext4.dll"
#define FCSHELLEXT1_DLL		"FastExt1.dll"
#define FCSHELLEX64_DLL		"FastEx64.dll"

#ifdef _WIN64
#define CURRENT_SHEXTDLL	FCSHELLEX64_DLL
#define CURRENT_SHEXTDLL_EX	FCSHELLEXT1_DLL
#else
#define CURRENT_SHEXTDLL	FCSHELLEXT1_DLL
#define CURRENT_SHEXTDLL_EX	FCSHELLEX64_DLL
#endif

#define UNC_PREFIX			"\\\\"

#define UNINSTALL_CMDLINE	"/r"
#define FASTCOPY_SHORTCUT	"FastCopy.lnk"

#define REGSTR_SHELLFOLDERS	REGSTR_PATH_EXPLORER "\\Shell Folders"
#define REGSTR_STARTUP		"Startup"
#define REGSTR_DESKTOP		"Desktop"
#define REGSTR_PROGRAMS		"Programs"
#define REGSTR_PATH			"Path"
#define REGSTR_PROGRAMFILES	"ProgramFilesDir"

#define INSTALL_STR			"Install"
#define UNINSTALL_STR		"UnInstall"

// function prototype
void BrowseDirDlg(TWin *parentWin, UINT editCtl, char *title);
int CALLBACK BrowseDirDlg_Proc(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM data);
int MakePath(char *dest, const char *dir, const char *file);
UINT GetDriveTypeEx(const char *file);

// inline function
inline BOOL IsUncFile(const char *path) { return strnicmp(path, UNC_PREFIX, 2) == 0; }

