/* @(#)Copyright (C) 2005-2017 H.Shirouzu		install.h	Ver3.31 */
/* ========================================================================
	Project  Name			: Installer for IPMSG32
	Module Name				: Main Header
	Create					: 2005-02-02(Wed)
	Update					: 2017-07-30(Sun)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	======================================================================== */
#ifndef __INST_H__
#define __INST_H__

enum InstMode { SETUP_MODE, UNINSTALL_MODE };

struct InstallCfg {
	InstMode	mode;
	BOOL		programLink;
	BOOL		desktopLink;
	BOOL		isAppReg;
	BOOL		runImme;
	BOOL		isAuto;
	BOOL		isSilent;
	BOOL		isExtract;
	BOOL		isExt64;
	BOOL		isSubDir;
	HWND		hOrgWnd;
	WCHAR		*setupDir;
	WCHAR		*appData;
	WCHAR		*virtualDir;
	WCHAR		*startMenu;
	WCHAR		*deskTop;
	Wstr		setuped;
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
	TSubClassCtl	extractBtn;
	TSubClassCtl	startBtn;
	TInstSheet		*propertySheet = NULL;
	InstallCfg		cfg;
	IPDict			ipDict;
	WCHAR			defaultDir[MAX_PATH] = {};

	U8str	ver;
	WCHAR	**orgArgv = NULL;
	int		orgArgc = 0;
	BOOL	VerifyDict();
	void	GetDefaultDir();

public:
	TInstDlg(char *cmdLine);
	virtual ~TInstDlg();

	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvNcDestroy(void);
#if 0
	virtual BOOL	EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif
	BOOL	Install(void);
	BOOL	UnInstall(void);
	BOOL	RunAsAdmin(BOOL is_imme);
	void	ChangeMode(void);
	void	Extract(void);
	void	Exit(DWORD exit_code);
	BOOL	RemoveSameLink(const WCHAR *dir, WCHAR *remove_path=NULL);
	void	GetDictName(const WCHAR *fname, WCHAR *dname);

	void	ErrMsg(const WCHAR *body, const WCHAR *title=L"FastCopy");
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
	WCHAR	*fileBuf = NULL;
	BOOL	dirtyFlg = FALSE;
	BOOL	*is_x64 = NULL;

public:
	TBrowseDirDlg(WCHAR *_fileBuf, BOOL *_is_x64) :
		fileBuf(_fileBuf), is_x64(_is_x64) {}
	virtual BOOL	AttachWnd(HWND _hWnd);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	SetFileBuf(LPARAM list);
	BOOL	IsDirty(void) { return dirtyFlg; };
};

class TInputDlg : public TDlg
{
protected:
	WCHAR	*dirBuf;

public:
	TInputDlg(WCHAR *_dirBuf, TWin *_win) : TDlg(INPUT_DIALOG, _win) { dirBuf = _dirBuf; }
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
};

class TLaunchDlg : public TDlg
{
protected:
	WCHAR *msg;

public:
	TLaunchDlg(const WCHAR *_msg, TWin *_win);
	virtual ~TLaunchDlg();

	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
};


#define FASTCOPY			L"FastCopy"
#define FASTCOPY_EXE		L"FastCopy.exe"
#define INSTALL_EXE			L"setup.exe"

#define README_TXT			L"readme.txt"
#define README_ENG_TXT		L"readme_eng.txt"
#define GPL_TXT				L"license-gpl3.txt"
#define XXHASH_TXT			L"xxhash-LICENSE.txt"
#define HELP_CHM			L"FastCopy.chm"

#define DOC_README_TXT		L"doc\\readme.txt"
#define DOC_README_ENG_TXT	L"doc\\readme_eng.txt"
#define DOC_HELP_JA			L"doc\\FastCopy.chm"
//#define DOC_HELP_EN		L"doc\\FastCopy_eng.htm"
#define DOC_XXHASH_TXT		L"doc\\xxhash-LICENSE.txt"
#define DOC_GPL_TXT			L"doc\\license-gpl3.txt"

#define SHELLEXT1_DLL		L"FastCopy_shext.dll"
#define SHELLEXT2_DLL		L"FastCopy_shext2.dll"
#define SHELLEXT3_DLL		L"FastCopy_shext3.dll"
#define SHELLEXT4_DLL		L"FastCopy_shext4.dll"
#define FCSHELLEXT1_DLL		L"FastExt1.dll"
#define FCSHELLEX64_DLL		L"FastEx64.dll"
#define FASTCOPY_INI		L"FastCopy2.ini"
#define FASTCOPY_LINK		L"to_ExeDir.lnk"
#define FASTCOPY_LOG		L"FastCopy.log"
#define FASTCOPY_LOGDIR		L"Log"
#define FASTCOPY_DOCDIR		L"Doc"

#ifdef _WIN64
#define CURRENT_SHEXTDLL	FCSHELLEX64_DLL
#define CURRENT_SHEXTDLL_EX	FCSHELLEXT1_DLL
#else
#define CURRENT_SHEXTDLL	FCSHELLEXT1_DLL
#define CURRENT_SHEXTDLL_EX	FCSHELLEX64_DLL
#endif

#define UNC_PREFIX			L"\\\\"

#define UNINSTALL_CMDLINE	L"/r"
#define FASTCOPY_SHORTCUT	L"FastCopy.lnk"
#define UNINST_BAT_W		L"fcuninst.bat"

#define REGSTR_SHELLFOLDERS	REGSTR_PATH_EXPLORER "\\Shell Folders"
#define REGSTR_STARTUP		"Startup"
#define REGSTR_DESKTOP		"Desktop"
#define REGSTR_PROGRAMS		"Programs"
#define REGSTR_PATH			"Path"
#define REGSTR_PROGRAMFILES	"ProgramFilesDir"

#define INSTALL_STR			L"Install"
#define UNINSTALL_STR		L"UnInstall"
#define UPDATE_STR			L"UPDATE"

#define HKCUSW_STR			"\\HKEY_CURRENT_USER\\Software"
#define HSTOOLS_STR			"HSTools"

// function prototype
BOOL BrowseDirDlg(TWin *parentWin, UINT editCtl, const WCHAR *title, BOOL *is_x64=NULL);
int CALLBACK BrowseDirDlg_Proc(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM data);

// inline function
inline BOOL IsUncFile(const WCHAR *path) { return wcsnicmp(path, UNC_PREFIX, 2) == 0; }

BOOL IsSameFileDict(IPDict *dict, const WCHAR *fname, const WCHAR *dst);
BOOL IPDictCopy(IPDict *dict, const WCHAR *fname, const WCHAR *dst, BOOL *is_rotate);

BOOL DeflateData(BYTE *buf, DWORD size, DynBuf *dbuf);
enum CreateStatus { CS_OK, CS_BROKEN, CS_ACCESS };
BOOL GetIPDictBySelf(IPDict *dict);

#define FDATA_KEY	"fdata"
#define MTIME_KEY	"mtime"
#define FSIZE_KEY	"fsize"
#define VER_KEY		"ver"
#define SHA256_KEY	"sha256"

#endif

