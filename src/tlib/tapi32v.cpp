/* @(#)Copyright (C) 1996-2010 H.Shirouzu		tapi32v.cpp	Ver0.99 */
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Main Header
	Create					: 2005-04-10(Sun)
	Update					: 2010-05-09(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "tlib.h"
#include <stdio.h>
#include <mbstring.h>
#include "tapi32ex.h"
#include "tapi32v.h"

int (WINAPI *GetWindowTextV)(HWND, void *, int);
int (WINAPI *SetWindowTextV)(HWND, const void *);
BOOL (WINAPI *GetWindowTextLengthV)(HWND);
UINT (WINAPI *GetDlgItemTextV)(HWND, int, void *, int);
BOOL (WINAPI *SetDlgItemTextV)(HWND, int, const void *);
BOOL (WINAPI *PostMessageV)(HWND, UINT, WPARAM, LPARAM);
BOOL (WINAPI *SendMessageV)(HWND, UINT, WPARAM, LPARAM);
BOOL (WINAPI *SendDlgItemMessageV)(HWND, int, UINT, WPARAM, LPARAM);
int (WINAPI *MessageBoxV)(HWND, const void *, const void *, UINT);
DWORD (WINAPI *FormatMessageV)(DWORD, const void *, DWORD, DWORD, void *, DWORD, va_list *);
HWND (WINAPI *CreateDialogParamV)(HANDLE, const void *, HWND, DLGPROC, LPARAM);
int (WINAPI *DialogBoxParamV)(HANDLE, const void *, HWND, DLGPROC, LPARAM);
LONG_PTR (WINAPI *GetWindowLongV)(HWND, int);
LONG_PTR (WINAPI *SetWindowLongV)(HWND, int, LONG_PTR);
LRESULT (WINAPI *CallWindowProcV)(WNDPROC, HWND, UINT, WPARAM, LPARAM);

int (WINAPI *RegisterClassV)(const void *wc);
HWND (WINAPI *FindWindowV)(const void *class_name, const void *window_name);
HWND (WINAPI *CreateWindowExV)(DWORD exStyle, const void *className, const void *title,
	DWORD style, int x, int y, int nw, int nh, HWND hParent, HMENU hMenu, HINSTANCE hI,
	void *param);

long (WINAPI *RegCreateKeyExV)(HKEY hKey, const void *subkey, DWORD reserved,
	void *class_name, DWORD opt, REGSAM sam, LPSECURITY_ATTRIBUTES *sa, PHKEY pkey, DWORD *dp);
long (WINAPI *RegOpenKeyExV)(HKEY hKey, const void *subkey, DWORD reserved,
	REGSAM sam, PHKEY pkey);
long (WINAPI *RegQueryValueExV)(HKEY hKey, const void *subkey, DWORD *reserved,
	DWORD *type, BYTE *data, DWORD *size);
long (WINAPI *RegQueryValueV)(HKEY hKey, const void *subkey, void *value, LONG *size);
long (WINAPI *RegSetValueExV)(HKEY hKey, const void *name, DWORD *reserved,
	DWORD type, const BYTE *data, DWORD size);
long (WINAPI *RegDeleteKeyV)(HKEY hKey, const void *subkey);
long (WINAPI *RegDeleteValueV)(HKEY hKey, const void *subval);
long (WINAPI *RegEnumKeyExV)(HKEY hKey, DWORD ikey, void *key, DWORD *size,
	DWORD *reserved, BYTE *class_name, DWORD *class_size, PFILETIME ft);
long (WINAPI *RegEnumValueV)(HKEY hKey, DWORD iVal, void *val, DWORD *size,
	DWORD *reserved, DWORD *type, BYTE *data, DWORD *data_size);

DWORD	(WINAPI *GetFileAttributesV)(const void *path);
BOOL	(WINAPI *SetFileAttributesV)(const void *path, DWORD attr);
HANDLE	(WINAPI *FindFirstFileV)(const void *path, WIN32_FIND_DATAW *fdat);
BOOL	(WINAPI *FindNextFileV)(const void *path, WIN32_FIND_DATAW *fdat);
HANDLE	(WINAPI *CreateFileV)(const void *path, DWORD access, DWORD share,
	LPSECURITY_ATTRIBUTES sa, DWORD create, DWORD attr, HANDLE ht);
BOOL	(WINAPI *MoveFileV)(const void *, const void *);
BOOL	(WINAPI *CreateHardLinkV)(const void *, const void *, LPSECURITY_ATTRIBUTES sa);
BOOL	(WINAPI *CreateDirectoryV)(const void *path, LPSECURITY_ATTRIBUTES secattr);
BOOL	(WINAPI *RemoveDirectoryV)(const void *path);
DWORD	(WINAPI *GetCurrentDirectoryV)(DWORD size, void *path);
BOOL	(WINAPI *SetCurrentDirectoryV)(const void *path);
BOOL	(WINAPI *DeleteFileV)(const void *path);
DWORD	(WINAPI *GetModuleFileNameV)(HMODULE hMod, void *path, DWORD len);
DWORD	(WINAPI *GetFullPathNameV)(const void *path, DWORD len, void *buf, void **fname);
BOOL	(WINAPI *GetDiskFreeSpaceV)(const void *path, DWORD *spc, DWORD *bps, DWORD *fc,
	DWORD *cl);
DWORD	(WINAPI *GetDriveTypeV)(const void *path);
BOOL	(WINAPI *GetVolumeInformationV)(const void *, void *, DWORD, DWORD *, DWORD *, DWORD *,
	void *, DWORD);
UINT	(WINAPI *DragQueryFileV)(HDROP, UINT, void *, UINT);
BOOL	(WINAPI *GetOpenFileNameV)(LPOPENFILENAMEW);
BOOL	(WINAPI *GetSaveFileNameV)(LPOPENFILENAMEW);
BOOL	(WINAPI *InsertMenuV)(HMENU, UINT, UINT, UINT, const void *);
BOOL	(WINAPI *ModifyMenuV)(HMENU, UINT, UINT, UINT, const void *);

BOOL	(WINAPI *GetMonitorInfoV)(HMONITOR hMonitor, MONITORINFO *lpmi);
HMONITOR (WINAPI *MonitorFromPointV)(POINT pt, DWORD dwFlags);

BOOL	(WINAPI *SHGetPathFromIDListV)(LPCITEMIDLIST, void *);
LPITEMIDLIST (WINAPI *SHBrowseForFolderV)(BROWSEINFO *);
LPITEMIDLIST (WINAPI *ILCreateFromPathV)(void *);
void (WINAPI *ILFreeV)(LPITEMIDLIST);
GUID	IID_IShellLinkV;
HINSTANCE (WINAPI *ShellExecuteV)(HWND hWnd, const void*, const void*, void*, const void*, int);
BOOL	(WINAPI *ShellExecuteExV)(LPSHELLEXECUTEINFO);
HMODULE	(WINAPI *LoadLibraryV)(void *);
BOOL	(WINAPI *PlaySoundV)(const void *, HMODULE, DWORD);

void	*(WINAPI *GetCommandLineV)(void);
void	*(WINAPI *CharUpperV)(void *str);
void	*(WINAPI *CharLowerV)(void *str);
int		(WINAPI *lstrcmpiV)(const void *str1, const void *str2);
int		(WINAPI *lstrlenV)(const void *str);
void	*(*lstrchrV)(const void *str, int);
WCHAR	(*lGetCharV)(const void *, int);
void	(*lSetCharV)(void *, int, WCHAR ch);
WCHAR	(*lGetCharIncV)(const void **);
int		(*strcmpV)(const void *str1, const void *str2);	// static ... for qsort
int		(*strnicmpV)(const void *str1, const void *str2, int len);
int		(*strlenV)(const void *path);
void	*(*strcpyV)(void *dst, const void *src);
void	*(*strdupV)(const void *src);
void	*(*strchrV)(const void *src, int);
void	*(*strrchrV)(const void *src, int);
long	(*strtolV)(const void *, const void **, int base);
u_long	(*strtoulV)(const void *, const void **, int base);
int		(*sprintfV)(void *buf, const void *format,...);
int		(*MakePathV)(void *dest, const void *dir, const void *file);

void *ASTERISK_V;		// "*"
void *FMT_CAT_ASTER_V;	// "%s\\*"
void *FMT_STR_V;		// "%s"
void *FMT_QUOTE_STR_V;	// "\"%s\""
void *FMT_INT_STR_V;	// "%d"
void *BACK_SLASH_V;		// "\\"
void *SEMICOLON_V;		// ";"
void *SEMICLN_SPC_V;	// "; "
void *NEWLINE_STR_V;	// "\r\n"
void *EMPTY_STR_V;		// ""
void *DOT_V;			// "."
void *DOTDOT_V;			// ".."
void *QUOTE_V;			// "\""
void *TRUE_V;			// "true"
void *FALSE_V;			// "false"
int CHAR_LEN_V;			// 2(WCHAR) or 1(char)
int MAX_PATHLEN_V;		// MAX_WCHAR or 
BOOL IS_WINNT_V;		// MAX_WCHAR or 
DWORD SHCNF_PATHV = SHCNF_PATHW;

DEFINE_GUID(IID_IShellLinkW, 0x000214F9, 0x0000, 0000, 0xC0, 0x00, 0x00, 0x00, 0x00, \
	0x00, 0x00, 0x46);
DEFINE_GUID(IID_IShellLinkA, 0x000214EE, 0x0000, 0000, 0xC0, 0x00, 0x00, 0x00, 0x00, \
	0x00, 0x00, 0x46);

void *GetLoadStrV(UINT resId, HINSTANCE hI)
{
	if (IS_WINNT_V)
		return	GetLoadStrW(resId, hI);
	else
		return	GetLoadStrA(resId, hI);
}

BOOL TLibInit_Win32V()
{
	if (IS_WINNT_V = IsWinNT()) {
		// Win32API(UNICODE)
		GetWindowTextV = (int (WINAPI *)(HWND, void *, int))GetWindowTextW;
		SetWindowTextV = (int (WINAPI *)(HWND, const void *))SetWindowTextW;
		GetWindowTextLengthV = (BOOL (WINAPI *)(HWND))GetWindowTextLengthW;

		GetDlgItemTextV = (UINT (WINAPI *)(HWND, int, void *, int))GetDlgItemTextW;
		SetDlgItemTextV = (BOOL (WINAPI *)(HWND, int, const void *))SetDlgItemTextW;
		PostMessageV = (BOOL (WINAPI *)(HWND, UINT, WPARAM, LPARAM))PostMessageW;
		SendMessageV = (BOOL (WINAPI *)(HWND, UINT, WPARAM, LPARAM))SendMessageW;
		SendDlgItemMessageV = (BOOL (WINAPI *)(HWND, int, UINT, WPARAM, LPARAM))
			SendDlgItemMessageW;
		MessageBoxV = (int (WINAPI *)(HWND, const void *, const void *, UINT))MessageBoxW;
		FormatMessageV = (DWORD (WINAPI *)(DWORD, const void *, DWORD, DWORD, void *, DWORD,
			va_list *))FormatMessageW;
		CreateDialogParamV = (HWND (WINAPI *)(HANDLE, const void *, HWND, DLGPROC, LPARAM))
			CreateDialogParamW;
		DialogBoxParamV = (int (WINAPI *)(HANDLE, const void *, HWND, DLGPROC, LPARAM))
			DialogBoxParamW;
		GetWindowLongV = (LONG_PTR (WINAPI *)(HWND, int))GetWindowLongW;
		SetWindowLongV = (LONG_PTR (WINAPI *)(HWND, int, LONG_PTR))SetWindowLongW;
		CallWindowProcV = (LRESULT (WINAPI *)(WNDPROC, HWND, UINT, WPARAM, LPARAM))
			CallWindowProcW;
		RegisterClassV = (int (WINAPI *)(const void *))RegisterClassW;
		FindWindowV = (HWND (WINAPI *)(const void *, const void *))FindWindowW;
		CreateWindowExV = (HWND (WINAPI *)(DWORD, const void *, const void *, DWORD,
			int, int, int, int, HWND, HMENU, HINSTANCE, void *))CreateWindowExW;

		RegCreateKeyExV = (long (WINAPI *)(HKEY, const void *, DWORD, void *, DWORD, REGSAM,
			LPSECURITY_ATTRIBUTES *, PHKEY, DWORD *))RegCreateKeyExW;
		RegOpenKeyExV = (long (WINAPI *)(HKEY, const void *, DWORD, REGSAM, PHKEY))RegOpenKeyExW;
		RegQueryValueExV = (long (WINAPI *)(HKEY, const void *, DWORD *, DWORD *, BYTE *, DWORD*))
			RegQueryValueExW;
		RegQueryValueV = (long (WINAPI *)(HKEY, const void *, void *, LONG *))RegQueryValueW;
		RegSetValueExV = (long (WINAPI *)(HKEY, const void *, DWORD *, DWORD, const BYTE *,DWORD))
			RegSetValueExW;
		RegDeleteKeyV = (long (WINAPI *)(HKEY, const void *))RegDeleteKeyW;
		RegDeleteValueV = (long (WINAPI *)(HKEY, const void *))RegDeleteValueW;
		RegEnumKeyExV = (long (WINAPI *)(HKEY, DWORD, void *, DWORD *, DWORD *, BYTE *, DWORD *,
			PFILETIME))RegEnumKeyExW;
		RegEnumValueV = (long (WINAPI *)(HKEY, DWORD, void *, DWORD *, DWORD *, DWORD *, BYTE *,
			DWORD *))RegEnumValueW;

		GetFileAttributesV = (DWORD (WINAPI *)(const void *))::GetFileAttributesW;
		SetFileAttributesV = (BOOL (WINAPI *)(const void *, DWORD))::SetFileAttributesW;
		FindFirstFileV = (HANDLE (WINAPI *)(const void *, WIN32_FIND_DATAW *))::FindFirstFileW;
		FindNextFileV = (BOOL (WINAPI *)(const void *, WIN32_FIND_DATAW *))::FindNextFileW;
		CreateFileV = (HANDLE (WINAPI *)(const void *, DWORD, DWORD, LPSECURITY_ATTRIBUTES,
			DWORD, DWORD, HANDLE))::CreateFileW;
		MoveFileV = (BOOL (WINAPI *)(const void *, const void *))::MoveFileW;

		HMODULE	hKernel32 = ::GetModuleHandle("kernel32.dll");
		CreateHardLinkV = (BOOL (WINAPI *)(const void *, const void *, LPSECURITY_ATTRIBUTES sa))
			::GetProcAddress(hKernel32, "CreateHardLinkW");

		CreateDirectoryV = (BOOL (WINAPI *)(const void *, LPSECURITY_ATTRIBUTES))
			::CreateDirectoryW;
		RemoveDirectoryV = (BOOL (WINAPI *)(const void *))::RemoveDirectoryW;
		DeleteFileV = (BOOL (WINAPI *)(const void *))::DeleteFileW;
		GetCurrentDirectoryV = (DWORD (WINAPI *)(DWORD, void *))::GetCurrentDirectoryW;
		SetCurrentDirectoryV = (BOOL (WINAPI *)(const void *))::SetCurrentDirectoryW;
		GetModuleFileNameV = (DWORD (WINAPI *)(HMODULE, void *, DWORD))
			GetModuleFileNameW;
		GetFullPathNameV = (DWORD (WINAPI *)(const void *, DWORD, void *, void **))
			GetFullPathNameW;
		GetDiskFreeSpaceV = (BOOL (WINAPI *)(const void *, DWORD *, DWORD *, DWORD *, DWORD *))
			::GetDiskFreeSpaceW;
		GetDriveTypeV = (DWORD (WINAPI *)(const void *))::GetDriveTypeW;
		GetVolumeInformationV = (BOOL (WINAPI *)(const void *, void *, DWORD, DWORD *, DWORD *,
			DWORD *, void *, DWORD))::GetVolumeInformationW;
		DragQueryFileV = (UINT (WINAPI *)(HDROP, UINT, void *, UINT))DragQueryFileW;
		GetOpenFileNameV = (BOOL (WINAPI *)(LPOPENFILENAMEW))GetOpenFileNameW;
		GetSaveFileNameV = (BOOL (WINAPI *)(LPOPENFILENAMEW))GetSaveFileNameW;
		InsertMenuV = (BOOL (WINAPI *)(HMENU, UINT, UINT, UINT, const void *))InsertMenuW;
		ModifyMenuV = (BOOL (WINAPI *)(HMENU, UINT, UINT, UINT, const void *))ModifyMenuW;
		GetCommandLineV = (void *(WINAPI *)(void))GetCommandLineW;
		ShellExecuteV = (HINSTANCE (WINAPI *)(HWND hWnd, const void *, const void *, void *,
			const void *, int))ShellExecuteW;
		ShellExecuteExV = (BOOL (WINAPI *)(LPSHELLEXECUTEINFO))ShellExecuteExW;
		LoadLibraryV = (HMODULE	(WINAPI *)(void *))LoadLibraryW;
		PlaySoundV = (BOOL (WINAPI *)(const void *, HMODULE, DWORD))PlaySoundW;

		CharUpperV = (void *(WINAPI *)(void *))::CharUpperW;
		CharLowerV = (void *(WINAPI *)(void *))::CharLowerW;
		lstrcmpiV = (int (WINAPI *)(const void *, const void *))::lstrcmpiW;
		lstrlenV = (int (WINAPI *)(const void *))::lstrlenW;
		lstrchrV = (void *(*)(const void *, int))(const wchar_t *(*)(const wchar_t *, wchar_t))
			::wcschr;
		lGetCharV = (WCHAR (*)(const void *, int))::lGetCharW;
		lSetCharV = (void (*)(void *, int, WCHAR))::lSetCharW;
		lGetCharIncV = (WCHAR (*)(const void **))::lGetCharIncW;
		strcmpV = (int (*)(const void *, const void *))::wcscmp;
		strnicmpV = (int (*)(const void *, const void *, int))::_wcsnicmp;
		strlenV = (int (*)(const void *))::wcslen;
		strcpyV = (void *(*)(void *, const void *))::wcscpy;
		strdupV = (void *(*)(const void *))::wcsdup;
		strchrV = (void *(*)(const void *, int))(const wchar_t *(*)(const wchar_t *, wchar_t))
			::wcschr;
		strrchrV = (void *(*)(const void *, int))(const wchar_t *(*)(const wchar_t *, wchar_t))
			::wcsrchr;
		strtolV = (long (*)(const void *, const void **, int base))::wcstol;
		strtoulV = (u_long (*)(const void *, const void **, int base))::wcstoul;
		sprintfV = (int (*)(void *, const void *,...))(int (*)(wchar_t *, const wchar_t *,...))
			::swprintf;
		MakePathV = (int (*)(void *, const void *, const void *))::MakePathW;

		HMODULE	hShell32 = ::GetModuleHandle("shell32.dll");
		SHGetPathFromIDListV = (BOOL (WINAPI *)(LPCITEMIDLIST, void *))
			::GetProcAddress(hShell32, "SHGetPathFromIDListW");
		SHBrowseForFolderV = (LPITEMIDLIST (WINAPI *)(BROWSEINFO *))
			::GetProcAddress(hShell32, "SHBrowseForFolderW");
		ILCreateFromPathV = (LPITEMIDLIST (WINAPI *)(void *))
			::GetProcAddress(hShell32, "ILCreateFromPathW");
		ILFreeV = (void (WINAPI *)(LPITEMIDLIST))::GetProcAddress(hShell32, "ILFree");
		IID_IShellLinkV = IID_IShellLinkW;

		HMODULE	hUser32 = ::GetModuleHandle("user32.dll");
		GetMonitorInfoV = (BOOL (WINAPI *)(HMONITOR, MONITORINFO *))
			::GetProcAddress(hUser32, "GetMonitorInfoW");
		MonitorFromPointV = (HMONITOR (WINAPI *)(POINT, DWORD))
			::GetProcAddress(hUser32, "MonitorFromPoint");

		CHAR_LEN_V = sizeof(WCHAR);
		MAX_PATHLEN_V = MAX_WPATH;
		SHCNF_PATHV = SHCNF_PATHW;

		ASTERISK_V = L"*";
		FMT_CAT_ASTER_V = L"%s\\*";
		FMT_STR_V = L"%s";
		FMT_QUOTE_STR_V = L"\"%s\"";
		BACK_SLASH_V = L"\\";
		SEMICOLON_V = L";";
		SEMICLN_SPC_V = L"; ";
		NEWLINE_STR_V = L"\r\n";
		EMPTY_STR_V = L"";
		DOT_V = L".";
		DOTDOT_V = L"..";
		QUOTE_V = L"\"";
		TRUE_V = L"true";
		FALSE_V = L"false";
	}
	else {
		// Win32API(ANSI)
		GetWindowTextV = (int (WINAPI *)(HWND, void *, int))GetWindowTextA;
		SetWindowTextV = (int (WINAPI *)(HWND, const void *))SetWindowTextA;
		GetWindowTextLengthV = (BOOL (WINAPI *)(HWND))GetWindowTextLengthA;

		GetDlgItemTextV = (UINT (WINAPI *)(HWND, int, void *, int))GetDlgItemTextA;
		SetDlgItemTextV = (BOOL (WINAPI *)(HWND, int, const void *))SetDlgItemTextA;
		PostMessageV = (BOOL (WINAPI *)(HWND, UINT, WPARAM, LPARAM))PostMessageA;
		SendMessageV = (BOOL (WINAPI *)(HWND, UINT, WPARAM, LPARAM))SendMessageA;
		SendDlgItemMessageV = (BOOL (WINAPI *)(HWND, int, UINT, WPARAM, LPARAM))
			SendDlgItemMessageA;
		MessageBoxV = (int (WINAPI *)(HWND, const void *, const void *, UINT))MessageBoxA;
		FormatMessageV = (DWORD (WINAPI *)(DWORD, const void *, DWORD, DWORD, void *, DWORD,
			va_list *))FormatMessageA;
		CreateDialogParamV = (HWND (WINAPI *)(HANDLE, const void *, HWND, DLGPROC, LPARAM))
			CreateDialogParamA;
		DialogBoxParamV = (int (WINAPI *)(HANDLE, const void *, HWND, DLGPROC, LPARAM))
			DialogBoxParamA;
		GetWindowLongV = (LONG_PTR (WINAPI *)(HWND, int))GetWindowLongA;
		SetWindowLongV = (LONG_PTR (WINAPI *)(HWND, int, LONG_PTR))SetWindowLongA;
		CallWindowProcV = (LRESULT (WINAPI *)(WNDPROC, HWND, UINT, WPARAM, LPARAM))
			CallWindowProcA;

		RegisterClassV = (int (WINAPI *)(const void *))RegisterClassA;
		FindWindowV = (HWND (WINAPI *)(const void *, const void *))FindWindowA;
		CreateWindowExV = (HWND (WINAPI *)(DWORD, const void *, const void *, DWORD,
			int, int, int, int, HWND, HMENU, HINSTANCE, void *))CreateWindowExA;

		RegCreateKeyExV = (long (WINAPI *)(HKEY, const void *, DWORD, void *, DWORD,
			REGSAM, LPSECURITY_ATTRIBUTES *, PHKEY, DWORD *))RegCreateKeyExA;
		RegOpenKeyExV = (long (WINAPI *)(HKEY, const void *, DWORD, REGSAM, PHKEY))
			RegOpenKeyExA;
		RegQueryValueExV = (long (WINAPI *)(HKEY, const void *, DWORD *, DWORD *, BYTE *, DWORD*))
			RegQueryValueExA;
		RegQueryValueV = (long (WINAPI *)(HKEY, const void *, void *, LONG *))RegQueryValueA;
		RegSetValueExV = (long (WINAPI *)(HKEY, const void *, DWORD *, DWORD, const BYTE*, DWORD))
			RegSetValueExA;
		RegDeleteKeyV = (long (WINAPI *)(HKEY, const void *))RegDeleteKeyA;
		RegDeleteValueV = (long (WINAPI *)(HKEY, const void *))RegDeleteValueA;
		RegEnumKeyExV = (long (WINAPI *)(HKEY, DWORD, void *, DWORD *, DWORD *, BYTE *, DWORD *,
			PFILETIME))RegEnumKeyExA;
		RegEnumValueV = (long (WINAPI *)(HKEY, DWORD, void *, DWORD *, DWORD *, DWORD *, BYTE *,
			DWORD *))RegEnumValueA;

		GetFileAttributesV = (DWORD (WINAPI *)(const void *))::GetFileAttributesA;
		SetFileAttributesV = (BOOL (WINAPI *)(const void *, DWORD))::SetFileAttributesA;
		FindFirstFileV = (HANDLE (WINAPI *)(const void *, WIN32_FIND_DATAW *))::FindFirstFileA;
		FindNextFileV = (BOOL (WINAPI *)(const void *, WIN32_FIND_DATAW *))::FindNextFileA;
		CreateFileV = (HANDLE (WINAPI *)(const void *, DWORD, DWORD, LPSECURITY_ATTRIBUTES,
			DWORD, DWORD, HANDLE))::CreateFileA;
		MoveFileV = (BOOL (WINAPI *)(const void *, const void *))::MoveFileA;

		HMODULE	hKernel32 = ::GetModuleHandle("kernel32.dll");
		CreateHardLinkV = (BOOL (WINAPI *)(const void *, const void *, LPSECURITY_ATTRIBUTES sa))
			::GetProcAddress(hKernel32, "CreateHardLinkA");

		CreateDirectoryV = (BOOL (WINAPI *)(const void *, LPSECURITY_ATTRIBUTES))
			::CreateDirectoryA;
		RemoveDirectoryV = (BOOL (WINAPI *)(const void *))::RemoveDirectoryA;
		DeleteFileV = (BOOL (WINAPI *)(const void *))::DeleteFileA;
		GetCurrentDirectoryV = (DWORD (WINAPI *)(DWORD, void *))::GetCurrentDirectoryA;
		SetCurrentDirectoryV = (BOOL (WINAPI *)(const void *))::SetCurrentDirectoryA;
		GetModuleFileNameV = (DWORD (WINAPI *)(HMODULE, void *, DWORD))
			GetModuleFileNameA;
		GetFullPathNameV = (DWORD (WINAPI *)(const void *, DWORD, void *, void **))
			::GetFullPathNameA;
		GetDiskFreeSpaceV = (BOOL (WINAPI *)(const void *, DWORD *, DWORD *, DWORD *, DWORD *))
			::GetDiskFreeSpaceA;
		GetDriveTypeV = (DWORD (WINAPI *)(const void *))::GetDriveTypeA;
		GetVolumeInformationV = (BOOL (WINAPI *)(const void *, void *, DWORD, DWORD *, DWORD *,
			DWORD *, void *, DWORD))::GetVolumeInformationA;
		DragQueryFileV = (UINT (WINAPI *)(HDROP, UINT, void *, UINT))DragQueryFileA;
		GetOpenFileNameV = (BOOL (WINAPI *)(LPOPENFILENAMEW))GetOpenFileNameA;
		GetSaveFileNameV = (BOOL (WINAPI *)(LPOPENFILENAMEW))GetSaveFileNameA;
		InsertMenuV = (BOOL (WINAPI *)(HMENU, UINT, UINT, UINT, const void *))InsertMenuA;
		ModifyMenuV = (BOOL (WINAPI *)(HMENU, UINT, UINT, UINT, const void *))ModifyMenuA;
		GetCommandLineV = (void *(WINAPI *)(void))GetCommandLineA;
		ShellExecuteV = (HINSTANCE (WINAPI *)(HWND hWnd, const void *, const void *, void *,
			const void *, int))ShellExecuteA;
		ShellExecuteExV = (BOOL (WINAPI *)(LPSHELLEXECUTEINFO))ShellExecuteExA;
		LoadLibraryV = (HMODULE	(WINAPI *)(void *))LoadLibraryA;
		PlaySoundV = (BOOL (WINAPI *)(const void *, HMODULE, DWORD))PlaySoundA;

		CharUpperV = (void *(WINAPI *)(void *))::CharUpperA;
		CharLowerV = (void *(WINAPI *)(void *))::CharLowerA;
		lstrcmpiV = (int (WINAPI *)(const void *, const void *))::lstrcmpiA;
		lstrlenV = (int (WINAPI *)(const void *))::lstrlenA;
		lstrchrV = (void *(*)(const void *, int))(u_char *(*)(u_char *, u_int))::_mbschr;
		lGetCharV = (WCHAR (*)(const void *, int))::lGetCharA;
		lSetCharV = (void (*)(void *, int, WCHAR))::lSetCharA;
		lGetCharIncV = (WCHAR (*)(const void **))::lGetCharIncA;
		strcmpV = (int (*)(const void *, const void *))::strcmp;
		strnicmpV = (int (*)(const void *, const void *, int))::strnicmp;
		strlenV = (int (*)(const void *))::strlen;
		strcpyV = (void *(*)(void *, const void *))::strcpy;
		strdupV = (void *(*)(const void *))::strdup;
		strchrV = (void *(*)(const void *, int))(char *(*)(char *, int))::strchr;
		strrchrV = (void *(*)(const void *, int))(char *(*)(char *, int))::strrchr;
		strtolV = (long (*)(const void *, const void **, int base))::strtol;
		strtoulV = (u_long (*)(const void *, const void **, int base))::strtoul;
		sprintfV = (int (*)(void *, const void *,...))::sprintf;
		MakePathV = (int (*)(void *, const void *, const void *))::MakePath;

		SHGetPathFromIDListV = (BOOL (WINAPI *)(LPCITEMIDLIST, void *))SHGetPathFromIDList;
		SHBrowseForFolderV = (LPITEMIDLIST (WINAPI *)(BROWSEINFO *))SHBrowseForFolder;
		ILCreateFromPathV = (LPITEMIDLIST (WINAPI *)(void *))NULL;
		ILFreeV = (void (WINAPI *)(LPITEMIDLIST))NULL;
		IID_IShellLinkV = IID_IShellLinkA;

		HMODULE	hUser32 = ::GetModuleHandle("user32.dll");
		GetMonitorInfoV = (BOOL (WINAPI *)(HMONITOR, MONITORINFO *))
			::GetProcAddress(hUser32, "GetMonitorInfoA");
		MonitorFromPointV = (HMONITOR (WINAPI *)(POINT, DWORD))
			::GetProcAddress(hUser32, "MonitorFromPoint");

		MAX_PATHLEN_V = MAX_PATH_EX;
		CHAR_LEN_V = sizeof(char);
		SHCNF_PATHV = SHCNF_PATH;

		ASTERISK_V = "*";
		FMT_CAT_ASTER_V = "%s\\*";
		FMT_STR_V = "%s";
		FMT_QUOTE_STR_V = "\"%s\"";
		BACK_SLASH_V = "\\";
		SEMICOLON_V = ";";
		SEMICLN_SPC_V = "; ";
		NEWLINE_STR_V = "\r\n";
		EMPTY_STR_V = "";
		DOT_V = ".";
		DOTDOT_V = "..";
		QUOTE_V = "\"";
		TRUE_V = "true";
		FALSE_V = "false";
	}
	return	TRUE;
}

