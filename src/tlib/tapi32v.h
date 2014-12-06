/* @(#)Copyright (C) 1996-2010 H.Shirouzu		tapi32v.h	Ver0.99 */
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Main Header
	Create					: 2005-04-10(Sun)
	Update					: 2010-05-09(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef TAPI32V_H
#define TAPI32V_H

extern int (WINAPI *RegisterClassV)(const void *wc);
extern HWND (WINAPI *FindWindowV)(const void *class_name, const void *window_name);
extern HWND (WINAPI *CreateWindowExV)(DWORD exStyle, const void *className, const void *title,
	DWORD style, int x, int y, int nw, int nh, HWND hParent, HMENU hMenu, HINSTANCE hI,
	void *param);

extern long (WINAPI *RegCreateKeyExV)(HKEY hKey, const void *subkey, DWORD reserved,
	void *class_name, DWORD opt, REGSAM sam, LPSECURITY_ATTRIBUTES *sa, PHKEY pkey, DWORD *dp);
extern long (WINAPI *RegOpenKeyExV)(HKEY hKey, const void *subkey, DWORD reserved, REGSAM sam,
	PHKEY pkey);
extern long (WINAPI *RegQueryValueExV)(HKEY hKey, const void *subkey, DWORD *reserved,
	DWORD *type, BYTE *data, DWORD *size);
extern long (WINAPI *RegQueryValueV)(HKEY hKey, const void *subkey, void *value, LONG *size);
extern long (WINAPI *RegSetValueExV)(HKEY hKey, const void *name, DWORD *reserved,
	DWORD type, const BYTE *data, DWORD size);
extern long (WINAPI *RegDeleteKeyV)(HKEY hKey, const void *subkey);
extern long (WINAPI *RegDeleteValueV)(HKEY hKey, const void *subval);
extern long (WINAPI *RegEnumKeyExV)(HKEY hKey, DWORD ikey, void *key, DWORD *size,
	DWORD *reserved, BYTE *class_name, DWORD *class_size, PFILETIME ft);
extern long (WINAPI *RegEnumValueV)(HKEY hKey, DWORD iVal, void *val, DWORD *size,
	DWORD *reserved, DWORD *type, BYTE *data, DWORD *data_size);

extern int (WINAPI *GetWindowTextV)(HWND, void *, int);
extern int (WINAPI *SetWindowTextV)(HWND, const void *);
extern BOOL (WINAPI *GetWindowTextLengthV)(HWND);
extern UINT (WINAPI *GetDlgItemTextV)(HWND, int, void *, int);
extern BOOL (WINAPI *SetDlgItemTextV)(HWND, int, const void *);
extern BOOL (WINAPI *PostMessageV)(HWND, UINT, WPARAM, LPARAM);
extern BOOL (WINAPI *SendMessageV)(HWND, UINT, WPARAM, LPARAM);
extern BOOL (WINAPI *SendDlgItemMessageV)(HWND, int, UINT, WPARAM, LPARAM);
extern int (WINAPI *MessageBoxV)(HWND, const void *, const void *, UINT);
extern DWORD (WINAPI *FormatMessageV)(DWORD, const void *, DWORD, DWORD, void *, DWORD, va_list*);
extern HWND (WINAPI *CreateDialogParamV)(HANDLE, const void *, HWND, DLGPROC, LPARAM);
extern int (WINAPI *DialogBoxParamV)(HANDLE, const void *, HWND, DLGPROC, LPARAM);
extern LONG_PTR (WINAPI *GetWindowLongV)(HWND, int);
extern LONG_PTR (WINAPI *SetWindowLongV)(HWND, int, LONG_PTR);
extern LRESULT (WINAPI *CallWindowProcV)(WNDPROC, HWND, UINT, WPARAM, LPARAM);

#define CreateDialogV(hInstance, lpTemplate, hWndParent, lpDialogFunc) \
		CreateDialogParamV(hInstance, lpTemplate, hWndParent, lpDialogFunc, 0L)
#define DialogBoxV(hInstance, lpTemplate, hWndParent, lpDialogFunc) \
		DialogBoxParamV(hInstance, lpTemplate, hWndParent, lpDialogFunc, 0L)

extern DWORD (WINAPI *GetFileAttributesV)(const void *path);
extern BOOL (WINAPI *SetFileAttributesV)(const void *path, DWORD attr);
extern HANDLE (WINAPI *FindFirstFileV)(const void *path, WIN32_FIND_DATAW *fdat);
extern BOOL (WINAPI *FindNextFileV)(const void *path, WIN32_FIND_DATAW *fdat);
extern HANDLE (WINAPI *CreateFileV)(const void *path, DWORD access, DWORD share,
	LPSECURITY_ATTRIBUTES sa, DWORD create, DWORD attr, HANDLE ht);
extern BOOL (WINAPI *CreateHardLinkV)(const void *, const void *, LPSECURITY_ATTRIBUTES sa);
extern BOOL (WINAPI *MoveFileV)(const void *, const void *);
extern BOOL (WINAPI *CreateDirectoryV)(const void *path, LPSECURITY_ATTRIBUTES secattr);
extern BOOL (WINAPI *RemoveDirectoryV)(const void *path);
extern DWORD (WINAPI *GetCurrentDirectoryV)(DWORD size, void *path);
extern BOOL (WINAPI *SetCurrentDirectoryV)(const void *path);
extern BOOL (WINAPI *DeleteFileV)(const void *path);
extern DWORD (WINAPI *GetModuleFileNameV)(HMODULE hMod, void *path, DWORD len);
extern DWORD (WINAPI *GetFullPathNameV)(const void *path, DWORD len, void *buf, void **fname);
extern BOOL (WINAPI *GetDiskFreeSpaceV)(const void *path, DWORD *spc, DWORD *bps, DWORD *fc,
	DWORD *cl);
extern DWORD (WINAPI *GetDriveTypeV)(const void *path);
extern BOOL (WINAPI *GetVolumeInformationV)(const void *, void *, DWORD, DWORD *, DWORD *,
	DWORD *, void *, DWORD);
extern UINT (WINAPI *DragQueryFileV)(HDROP, UINT, void *, UINT);
extern BOOL (WINAPI *GetOpenFileNameV)(LPOPENFILENAMEW);
extern BOOL (WINAPI *GetSaveFileNameV)(LPOPENFILENAMEW);
extern BOOL (WINAPI *InsertMenuV)(HMENU, UINT, UINT, UINT, const void *);
extern BOOL (WINAPI *ModifyMenuV)(HMENU, UINT, UINT, UINT, const void *);

#if _MSC_VER <= 1200
#define MONITOR_DEFAULTTONULL       0x00000000
#define MONITOR_DEFAULTTOPRIMARY    0x00000001
#define MONITOR_DEFAULTTONEAREST    0x00000002

typedef HANDLE HMONITOR;
typedef struct {
	DWORD   cbSize;
	RECT    rcMonitor;
	RECT    rcWork;
	DWORD   dwFlags;
} MONITORINFO;
#endif

extern BOOL (WINAPI *GetMonitorInfoV)(HMONITOR hMonitor, MONITORINFO *lpmi);
extern HMONITOR (WINAPI *MonitorFromPointV)(POINT pt, DWORD dwFlags);


extern BOOL (WINAPI *SHGetPathFromIDListV)(LPCITEMIDLIST, void *);
extern LPITEMIDLIST (WINAPI *SHBrowseForFolderV)(BROWSEINFO *);
extern LPITEMIDLIST (WINAPI *ILCreateFromPathV)(void *);
extern void (WINAPI *ILFreeV)(LPITEMIDLIST);
extern GUID IID_IShellLinkV;
extern HINSTANCE (WINAPI *ShellExecuteV)(HWND hWnd, const void *, const void *, void *,
	const void *, int);
extern BOOL (WINAPI *ShellExecuteExV)(LPSHELLEXECUTEINFO);
extern HMODULE (WINAPI *LoadLibraryV)(void *);
extern BOOL (WINAPI *PlaySoundV)(const void *, HMODULE, DWORD);

extern void *(WINAPI *GetCommandLineV)(void);
extern void *(WINAPI *CharUpperV)(void *str);
extern void *(WINAPI *CharLowerV)(void *str);
extern int (WINAPI *lstrcmpiV)(const void *str1, const void *str2);
extern int (WINAPI *lstrlenV)(const void *str);
extern void *(*lstrchrV)(const void *str, int);
extern WCHAR (*lGetCharV)(const void *, int);
extern void (*lSetCharV)(void *, int, WCHAR);
extern WCHAR (*lGetCharIncV)(const void **);
extern int (*strcmpV)(const void *str1, const void *str2);	// static ... for qsort
extern int (*strnicmpV)(const void *str1, const void *str2, int len);
extern int (*strlenV)(const void *path);
extern void *(*strcpyV)(void *dst, const void *src);
extern void *(*strdupV)(const void *src);
extern void *(*strchrV)(const void *src, int);
extern void *(*strrchrV)(const void *src, int);
extern long (*strtolV)(const void *, const void **, int base);
extern u_long (*strtoulV)(const void *, const void **, int base);
extern int (*sprintfV)(void *buf, const void *format,...);
extern int (*MakePathV)(void *dest, const void *dir, const void *file);

extern void *ASTERISK_V;		// "*"
extern void *FMT_CAT_ASTER_V;	// "%s\\*"
extern void *FMT_STR_V;			// "%s"
extern void *FMT_QUOTE_STR_V;	// "\"%s\""
extern void *FMT_INT_STR_V;		// "%d"
extern void *BACK_SLASH_V;		// "\\"
extern void *SEMICOLON_V;		// ";"
extern void *SEMICLN_SPC_V;		// "; "
extern void *NEWLINE_STR_V;		// "\r\n"
extern void *EMPTY_STR_V;		// ""
extern void *DOT_V;				// "."
extern void *DOTDOT_V;			// ".."
extern void *QUOTE_V;			// "\""
extern void *TRUE_V;			// "true"
extern void *FALSE_V;			// "false"
extern int CHAR_LEN_V;			// 2(WCHAR) or 1(char)
extern int MAX_PATHLEN_V;
extern BOOL IS_WINNT_V;
extern DWORD SHCNF_PATHV;

inline void *MakeAddr(const void *addr, int len) { return (BYTE *)addr + len * CHAR_LEN_V; }
inline void SetChar(void *addr, int offset, int val) {
	IS_WINNT_V ? (*(WCHAR *)MakeAddr(addr, offset) = val)
			   : (*(char *)MakeAddr(addr, offset) = val);
}
inline WCHAR GetChar(const void *addr, int offset) {
	return IS_WINNT_V ? *(WCHAR *)MakeAddr(addr, offset) : *(char *)MakeAddr(addr, offset);
}
inline int DiffLen(const void *high, const void *low) {
	return (int)(IS_WINNT_V ? (WCHAR *)high - (WCHAR *)low : (char *)high - (char *)low);
}

void InitWin32API_V(void);
void *GetLoadStrV(UINT resId, HINSTANCE hI=NULL);

#ifdef UNICODE
#define GetLoadStr GetLoadStrW
#else
#define GetLoadStr GetLoadStrA
#endif

#if _MSC_VER < 1200
typedef struct _REPARSE_GUID_DATA_BUFFER {
	DWORD ReparseTag;
	WORD ReparseDataLength;
	WORD Reserved;
	GUID ReparseGuid;
	struct {
		BYTE DataBuffer[1];
	} GenericReparseBuffer;
} REPARSE_GUID_DATA_BUFFER,  *PREPARSE_GUID_DATA_BUFFER;

#define MAXIMUM_REPARSE_DATA_BUFFER_SIZE  (16 * 1024)
#define IsReparseTagMicrosoft(x) ((x) & 0x80000000)
#define FILE_ATTRIBUTE_REPARSE_POINT	0x00000400

#define IO_REPARSE_TAG_MOUNT_POINT		(0xA0000003L)
#define IO_REPARSE_TAG_HSM				(0xC0000004L)
#define IO_REPARSE_TAG_SIS				(0x80000007L)
#define IO_REPARSE_TAG_DFS				(0x8000000AL)
#define IO_REPARSE_TAG_SYMLINK			(0xA000000CL)
#define IO_REPARSE_TAG_DFSR				(0x80000012L)
#endif

#if _MSC_VER <= 1200
#define FSCTL_SET_REPARSE_POINT			0x000900A4
#define FSCTL_GET_REPARSE_POINT			0x000900A8
#define FSCTL_DELETE_REPARSE_POINT		0x000900AC
#define FILE_FLAG_OPEN_REPARSE_POINT	0x00200000
#endif

#if _MSC_VER != 1200
typedef struct _REPARSE_DATA_BUFFER {
	DWORD	ReparseTag;
	WORD	ReparseDataLength;
	WORD	Reserved;
	union {
		struct {
			WORD	SubstituteNameOffset;
			WORD	SubstituteNameLength;
			WORD	PrintNameOffset;
			WORD	PrintNameLength;
			ULONG	Flags;
			WCHAR	PathBuffer[1];
		} SymbolicLinkReparseBuffer;
		struct {
			WORD	SubstituteNameOffset;
			WORD	SubstituteNameLength;
			WORD	PrintNameOffset;
			WORD	PrintNameLength;
			WCHAR	PathBuffer[1];
		} MountPointReparseBuffer;
		struct {
			BYTE	DataBuffer[1];
		} GenericReparseBuffer;
	};
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;
#endif

#define REPARSE_DATA_BUFFER_HEADER_SIZE  FIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer)
#define REPARSE_GUID_DATA_BUFFER_HEADER_SIZE \
			FIELD_OFFSET(REPARSE_GUID_DATA_BUFFER, GenericReparseBuffer)

#define IsReparseTagJunction(r) \
		(((REPARSE_DATA_BUFFER *)r)->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT)
#define IsReparseTagSymlink(r) \
		(((REPARSE_DATA_BUFFER *)r)->ReparseTag == IO_REPARSE_TAG_SYMLINK)

#ifndef SE_CREATE_SYMBOLIC_LINK_NAME
#define SE_CREATE_SYMBOLIC_LINK_NAME  "SeCreateSymbolicLinkPrivilege"
#endif

#ifndef SE_MANAGE_VOLUME_NAME
#define SE_MANAGE_VOLUME_NAME  "SeManageVolumePrivilege"
#endif

BOOL TLibInit_Win32V();

#endif
