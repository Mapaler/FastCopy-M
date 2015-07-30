static char *tap32u8_id = 
	"@(#)Copyright (C) 1996-2015 H.Shirouzu		tap32u8.cpp	Ver0.99";
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Application Frame Class
	Create					: 1996-06-01(Sat)
	Update					: 2015-06-22(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "tlib.h"
#include "tapi32u8.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

HWND CreateWindowU8(const char *class_name, const char *window_name, DWORD style,
	int x, int y, int width, int height, HWND hParent, HMENU hMenu, HINSTANCE hInst, void *param)
{
	Wstr	class_name_w(class_name), window_name_w(window_name);

	return	::CreateWindowW(class_name_w, window_name_w, style, x, y, width, height,
			hParent, hMenu, hInst, param);
}

HWND FindWindowU8(const char *class_name, const char *window_name)
{
	Wstr	class_name_w(class_name), window_name_w(window_name);

	return	::FindWindowW(class_name_w, window_name_w);
}

BOOL AppendMenuU8(HMENU hMenu, UINT flags, UINT idItem, const char *item_str)
{
	Wstr	item_str_w(item_str);
	return	::AppendMenuW(hMenu, flags, idItem, item_str_w);
}

BOOL InsertMenuU8(HMENU hMenu, UINT idItem, UINT flags, UINT idNewItem, const char *item_str)
{
	Wstr	item_str_w(item_str);
	return	::InsertMenuW(hMenu, idItem, flags, idNewItem, item_str_w);
}

BOOL ModifyMenuU8(HMENU hMenu, UINT idItem, UINT flags, UINT idNewItem, const char *item_str)
{
	Wstr	item_str_w(item_str);
	return	::ModifyMenuW(hMenu, idItem, flags, idNewItem, item_str_w);
}

UINT DragQueryFileU8(HDROP hDrop, UINT iFile, char *buf, UINT cb)
{
	Wstr	wbuf(cb);

	UINT	ret = ::DragQueryFileW(hDrop, iFile, wbuf.Buf(), cb);

	if (ret > 0 && buf) {
		ret = WtoU8(wbuf, buf, cb);
	}
	return	ret;
}

void WIN32_FIND_DATA_WtoU8(const WIN32_FIND_DATAW *fdat_w, WIN32_FIND_DATA_U8 *fdat_u8,
	BOOL include_fname)
{
	memcpy(fdat_u8, fdat_w, offsetof(WIN32_FIND_DATAW, cFileName));
	if (include_fname) {
		WtoU8(fdat_w->cFileName, fdat_u8->cFileName, sizeof(fdat_u8->cFileName));
		WtoU8(fdat_w->cAlternateFileName, fdat_u8->cAlternateFileName,
			sizeof(fdat_u8->cAlternateFileName));
	}
}

HANDLE FindFirstFileU8(const char *path, WIN32_FIND_DATA_U8 *fdat)
{
	Wstr				wpath(path);
	WIN32_FIND_DATAW	fdat_w;
	HANDLE				ret;

	if ((ret = ::FindFirstFileW(wpath, &fdat_w)) != INVALID_HANDLE_VALUE) {
		WIN32_FIND_DATA_WtoU8(&fdat_w, fdat);
	}

	return	ret;
}

BOOL FindNextFileU8(HANDLE hDir, WIN32_FIND_DATA_U8 *fdat)
{
	WIN32_FIND_DATAW	fdat_w;
	BOOL				ret;

	if ((ret = ::FindNextFileW(hDir, &fdat_w))) {
		WIN32_FIND_DATA_WtoU8(&fdat_w, fdat);
	}

	return	ret;
}

BOOL GetFileInfomationU8(const char *path, WIN32_FIND_DATA_U8 *fdata)
{
	HANDLE	fh;

	if ((fh = FindFirstFileU8(path, fdata)) != INVALID_HANDLE_VALUE)
	{
		::FindClose(fh);
		return	TRUE;
	}

	if ((fh = CreateFileU8(path, GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0)) != INVALID_HANDLE_VALUE)
	{
		BY_HANDLE_FILE_INFORMATION	info;
		BOOL	info_ret = ::GetFileInformationByHandle(fh, &info);
		::CloseHandle(fh);
		if (info_ret) {
			memcpy(fdata, &info, (char *)&info.dwVolumeSerialNumber - (char *)&info);
			return	TRUE;
		}
	}

	return	(fdata->dwFileAttributes = GetFileAttributesU8(path)) == 0xffffffff ? FALSE : TRUE;
}

HANDLE CreateFileU8(const char *path, DWORD access_flg, DWORD share_flg, SECURITY_ATTRIBUTES *sa,
	DWORD create_flg, DWORD attr_flg, HANDLE hTemplate)
{
	Wstr wpath(path);
	return ::CreateFileW(wpath, access_flg, share_flg, sa, create_flg, attr_flg, hTemplate);
}

BOOL DeleteFileU8(const char *path)
{
	Wstr wpath(path);
	return ::DeleteFileW(wpath);
}

BOOL CreateDirectoryU8(const char *path, SECURITY_ATTRIBUTES *lsa)
{
	Wstr wpath(path);
	return ::CreateDirectoryW(wpath, lsa);
}

BOOL RemoveDirectoryU8(const char *path)
{
	Wstr wpath(path);
	return ::RemoveDirectoryW(wpath);
}

DWORD GetFullPathNameU8(const char *path, DWORD size, char *buf, char **fname)
{
	Wstr	wpath(path), wbuf(size);
	WCHAR	*wfname=NULL;

	DWORD	ret = ::GetFullPathNameW(wpath, size, wbuf.Buf(), &wfname);

	if (ret == 0 || ret > size)
		return	ret;

	int fname_len = wfname ? WtoU8(wfname, buf, size) : 0;
	int path_len  = WtoU8(wbuf, buf, size);
	*fname = wfname ? (buf + path_len - fname_len) : NULL;

	return	ret;
}

DWORD GetFileAttributesU8(const char *path)
{
	Wstr wpath(path);
	return ::GetFileAttributesW(wpath);
}

BOOL SetFileAttributesU8(const char *path, DWORD attr)
{
	Wstr	wpath(path);
	return	::SetFileAttributesW(wpath, attr);
}

HINSTANCE ShellExecuteU8(HWND hWnd, LPCSTR op, LPCSTR file, LPSTR params, LPCSTR dir, int nShow)
{
	Wstr	op_w(op), file_w(file), params_w(params), dir_w(dir);
	return	::ShellExecuteW(hWnd, op_w, file_w, params_w, dir_w, nShow);
}

BOOL ShellExecuteExU8(SHELLEXECUTEINFO *info)
{
	SHELLEXECUTEINFOW	info_w;
	memcpy(&info_w, info, sizeof(SHELLEXECUTEINFO));
	Wstr	verb_w(info->lpVerb), file_w(info->lpFile), param_w(info->lpParameters),
			dir_w(info->lpDirectory), class_w(info->lpClass);

	info_w.lpVerb		= verb_w;
	info_w.lpFile		= file_w;
	info_w.lpParameters	= param_w;
	info_w.lpDirectory	= dir_w;
	info_w.lpClass		= class_w;

	BOOL	ret = ::ShellExecuteExW(&info_w);

	info_w.lpFile		= (WCHAR *)info->lpFile;
	info_w.lpParameters	= (WCHAR *)info->lpParameters;
	info_w.lpDirectory	= (WCHAR *)info->lpDirectory;
	info_w.lpClass		= (WCHAR *)info->lpClass;
	memcpy(info, &info_w, sizeof(SHELLEXECUTEINFO));

	return	ret;
}

DWORD GetCurrentDirectoryU8(DWORD size, char *dir)
{
	Wstr	dir_w(size);
	DWORD	ret = ::GetCurrentDirectoryW(size, dir_w.Buf());
	if (ret > 0) {
		ret = WtoU8(dir_w, dir, size);
	}
	return	ret;
}

DWORD GetWindowsDirectoryU8(char *dir, DWORD size)
{
	Wstr	dir_w(size);
	DWORD	ret = ::GetWindowsDirectoryW(dir_w.Buf(), size);
	if (ret > 0) {
		ret = WtoU8(dir_w, dir, size);
	}
	return	ret;
}

BOOL SetCurrentDirectoryU8(char *dir)
{
	Wstr	dir_w(dir);
	return	::SetCurrentDirectoryW(dir_w);
}

BOOL GetOpenFileNameU8Core(LPOPENFILENAME ofn, BOOL (WINAPI *ofn_func)(OPENFILENAMEW*))
{
	OPENFILENAMEW	ofn_w;
	Wstr	filter_w(MAX_PATH), cfilter_w(ofn->nMaxCustFilter),
			file_w(ofn->nMaxFile), ftitle_w(ofn->nMaxFileTitle),
			idir_w(MAX_PATH), title_w(MAX_PATH), defext_w(MAX_PATH), template_w(MAX_PATH);

	memcpy(&ofn_w, ofn, sizeof(OPENFILENAME));

	WCHAR *wp=filter_w.Buf();
	for (const char *p=ofn->lpstrFilter; p && *p; p+=strlen(p)+1) {
		wp += U8toW(p, wp, (int)(MAX_PATH - (wp - filter_w.Buf())));
	}
	*wp = 0;
	U8toW(ofn->lpstrCustomFilter, cfilter_w.Buf(), ofn->nMaxCustFilter);
	U8toW(ofn->lpstrFile, file_w.Buf(), ofn->nMaxFile);
	U8toW(ofn->lpstrFileTitle, ftitle_w.Buf(), ofn->nMaxFileTitle);
	U8toW(ofn->lpstrInitialDir, idir_w.Buf(), MAX_PATH);
	U8toW(ofn->lpstrTitle, title_w.Buf(), MAX_PATH);
	U8toW(ofn->lpstrDefExt, defext_w.Buf(), MAX_PATH);
	U8toW(ofn->lpTemplateName, template_w.Buf(), MAX_PATH);

	if (ofn->lpstrFilter)		ofn_w.lpstrFilter		= filter_w;
	if (ofn->lpstrCustomFilter)	ofn_w.lpstrCustomFilter	= cfilter_w.Buf();
	if (ofn->lpstrFile)			ofn_w.lpstrFile			= file_w.Buf();
	if (ofn->lpstrFileTitle)	ofn_w.lpstrFileTitle	= ftitle_w.Buf();
	if (ofn->lpstrInitialDir)	ofn_w.lpstrInitialDir	= idir_w;
	if (ofn->lpstrTitle)		ofn_w.lpstrTitle		= title_w;
	if (ofn->lpstrDefExt)		ofn_w.lpstrDefExt		= defext_w;
	if (ofn->lpTemplateName)	ofn_w.lpTemplateName	= template_w;

	BOOL	ret = ofn_func(&ofn_w);

	if (ofn->lpstrCustomFilter)	WtoU8(cfilter_w, ofn->lpstrCustomFilter, ofn->nMaxCustFilter);
	if (ofn->lpstrFileTitle)	WtoU8(ftitle_w, ofn->lpstrFileTitle, ofn->nMaxFileTitle);
	if (ofn->lpstrFile) {
		if (ofn_w.Flags & OFN_ALLOWMULTISELECT) {
			const WCHAR *wp=file_w;
			char *p;
			for (p=ofn->lpstrFile; wp && *wp; wp+=wcslen(wp)+1) {
				p += WtoU8(wp, p, (int)(ofn->nMaxFile - (p - ofn->lpstrFile)));
			}
			*p = 0;
		}
		else {
			WtoU8(file_w, ofn->lpstrFile, ofn->nMaxFile);
		}
	}
//	if (ofn_w.lpstrFile[ofn_w.nFileOffset])
		ofn->nFileOffset = ::WideCharToMultiByte(CP_UTF8, 0, ofn_w.lpstrFile, ofn_w.nFileOffset,
							0, 0, 0, 0);

	return	ret;
}

BOOL GetOpenFileNameU8(LPOPENFILENAME ofn)
{
	return	GetOpenFileNameU8Core(ofn, GetOpenFileNameW);
}

BOOL GetSaveFileNameU8(LPOPENFILENAME ofn)
{
	return	GetOpenFileNameU8Core(ofn, GetSaveFileNameW);
}

/*
	リンクの解決
	あらかじめ、CoInitialize(NULL); を実行しておくこと
*/
BOOL ReadLinkU8(LPCSTR src, LPSTR dest, LPSTR arg)
{
	IShellLink		*shellLink;
	IPersistFile	*persistFile;
	WCHAR			wbuf[MAX_PATH];
	BOOL			ret = FALSE;

	if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
		(void **)&shellLink)))
	{
		if (SUCCEEDED(shellLink->QueryInterface(IID_IPersistFile, (void **)&persistFile)))
		{
			U8toW(src, wbuf, MAX_PATH);
			if (SUCCEEDED(persistFile->Load(wbuf, STGM_READ)))
			{
				if (SUCCEEDED(shellLink->GetPath((char *)wbuf, MAX_PATH, NULL, SLGP_SHORTPATH)))
				{
					WtoU8(wbuf, dest, MAX_PATH_U8);
					shellLink->GetArguments((char *)wbuf, MAX_PATH);
					WtoU8(wbuf, arg, MAX_PATH_U8);
					ret = TRUE;
				}
			}
			persistFile->Release();
		}
		shellLink->Release();
	}
	return	ret;
}


BOOL PlaySoundU8(const char *path, HMODULE hmod, DWORD flg)
{
	Wstr	path_w(path);
	return	::PlaySoundW(path_w, hmod, flg);
}

/*=========================================================================
	Win32(W) API UTF8 wrapper
=========================================================================*/
BOOL GetMenuStringU8(HMENU hMenu, UINT uItem, char *buf, int bufsize, UINT flags)
{
	WCHAR	*wbuf = new WCHAR [bufsize];
	BOOL	ret = ::GetMenuStringW(hMenu, uItem, wbuf, bufsize, flags);
	if (ret) {
		WtoU8(wbuf, buf, bufsize);
	}
	delete wbuf;
	return	ret;
}

DWORD GetModuleFileNameU8(HMODULE hModule, char *buf, DWORD bufsize)
{
	WCHAR	*wbuf = new WCHAR [bufsize];
	DWORD	ret = ::GetModuleFileNameW(hModule, wbuf, bufsize);
	if (ret) {
		WtoU8(wbuf, buf, bufsize);
	}
	delete wbuf;
	return	ret;
}

UINT GetDriveTypeU8(const char *path)
{
	Wstr	wpath(path);

	return	::GetDriveTypeW(wpath);
}

LPSTR GetLoadStrU8(UINT resId, HINSTANCE hI)
{
	extern HINSTANCE defaultStrInstance;

	static TResHash	*hash;

	if (hash == NULL) {
		hash = new TResHash(100);
	}

	WCHAR		buf[1024];
	TResHashObj	*obj;

	if ((obj = hash->Search(resId)) == NULL) {
		if (::LoadStringW(hI ? hI : defaultStrInstance, resId, buf, sizeof(buf) / sizeof(WCHAR))
				>= 0) {
			U8str	buf_u8(buf);
			obj = new TResHashObj(resId, strdup(buf_u8));
			hash->Register(obj);
		}
	}
	return	obj ? (char *)obj->val : NULL;
}

/*=========================================================================
	UCS2(W) - UTF-8(U8) - ANSI(A) 相互変換
=========================================================================*/
int WtoU8(const WCHAR *src, char *dst, int bufsize, int max_len)
{
	if (bufsize == 1) {
		*dst = 0;
		return	0;
	}
	else if (bufsize >= 2) { // clear for UTF8
		int len = min(bufsize, 3);
		memset(dst + bufsize - len, 0, len);
	}

	int affect_len = bufsize ? bufsize - 1 : 0;
	int len = ::WideCharToMultiByte(CP_UTF8, 0, src, max_len, dst, affect_len, 0, 0);
	if (len < bufsize && dst) dst[len] = 0;

	return	len;
}

int U8toW(const char *src, WCHAR *dst, int bufsize, int max_len)
{
	int len = ::MultiByteToWideChar(CP_UTF8, 0, src, max_len, dst, bufsize);

	if (len == 0) {
		if (dst && bufsize > 0) {
			if ((len = (int)wcsnlen(dst, bufsize)) == bufsize) dst[--len] = 0;
		}
	} else if (len < bufsize) {
		dst[len] = 0;
	}

	return	len;
}

int AtoW(const char *src, WCHAR *dst, int bufsize, int max_len)
{
	int len = ::MultiByteToWideChar(CP_ACP, 0, src, max_len, dst, bufsize);

	if (len == 0) {
		if (dst && bufsize > 0) {
			if ((len = (int)wcsnlen(dst, bufsize)) == bufsize) {
				dst[--len] = 0;
			}
		}
	} else if (len < bufsize) {
		dst[len] = 0;
	}

	return	len;
}

int WtoA(const WCHAR *src, char *dst, int bufsize, int max_len)
{
	if (bufsize == 1) {
		*dst = 0;
		return	0;
	}

	int affect_len = bufsize ? bufsize - 1 : 0;
	int len = ::WideCharToMultiByte(CP_ACP, 0, src, max_len, dst, affect_len, 0, 0);

	if (len == 0) {
		if (dst && bufsize > 0) {
			if ((len = (int)strnlen(dst, affect_len)) == affect_len) {
				dst[len] = 0;
			}
		}
	} else if (len < bufsize) {
		dst[len] = 0;
	}

	return	len;
}

WCHAR *U8toW(const char *src, int max_len) {
	WCHAR	*wbuf = NULL;
	int		len = U8toW(src, NULL, 0, max_len) + 1;

	if (len > 0) {
		wbuf = new WCHAR [len];
		U8toW(src, wbuf, len, max_len);
	}
	return	wbuf;
}

#define MAX_STATIC_ARRAY 8 // 2^n

WCHAR *U8toWs(const char *src, int max_len) {
	static	WCHAR	*wbuf[MAX_STATIC_ARRAY];
	static	u_long	idx;

	WCHAR	*&cur_buf = wbuf[idx++ % MAX_STATIC_ARRAY];
	if (cur_buf) delete [] cur_buf;
	return	(cur_buf = U8toW(src, max_len));
}

char *WtoU8(const WCHAR *src, int max_len) {
	char	*buf = NULL;
	int		len = WtoU8(src, NULL, 0, max_len) + 1;

	if (len > 0) {
		buf = new char [len];
		WtoU8(src, buf, len, max_len);
	}
	return	buf;
}

char *WtoU8s(const WCHAR *src, int max_len) {
	static	char	*buf[MAX_STATIC_ARRAY];
	static	u_long	idx;

	char	*&cur_buf = buf[idx++ % MAX_STATIC_ARRAY];
	if (cur_buf) delete [] cur_buf;
	return	(cur_buf = WtoU8(src, max_len));
}

WCHAR *AtoW(const char *src, int max_len) {
	WCHAR	*wbuf = NULL;
	int		len	  = AtoW(src, NULL, 0, max_len) + 1;

	if (len > 0) {
		wbuf = new WCHAR [len];
		AtoW(src, wbuf, len, max_len);
	}
	return	wbuf;
}

WCHAR *AtoWs(const char *src, int max_len) {
	static	WCHAR	*wbuf[MAX_STATIC_ARRAY];
	static	u_long	idx;

	WCHAR	*&cur_buf = wbuf[idx++ % MAX_STATIC_ARRAY];
	if (cur_buf) delete [] cur_buf;
	return	(cur_buf = AtoW(src, max_len));
}

char *WtoA(const WCHAR *src, int max_len) {
	char	*buf = NULL;
	int		len	  = WtoA(src, NULL, 0, max_len) + 1;

	if (len > 0) {
		buf = new char [len];
		WtoA(src, buf, len, max_len);
	}
	return	buf;
}

char *WtoAs(const WCHAR *src, int max_len) {
	static	char	*buf[MAX_STATIC_ARRAY];
	static	u_long	idx;

	char	*&cur_buf = buf[idx++ % MAX_STATIC_ARRAY];
	if (cur_buf) delete [] cur_buf;
	return	(cur_buf = WtoA(src, max_len));
}

char *AtoU8(const char *src, int max_len) {
	WCHAR	*wsrc = AtoW(src, max_len);
	char	*buf;

	if (wsrc) buf = WtoU8(wsrc, max_len);
	delete [] wsrc;
	return	buf;
}

char *AtoU8s(const char *src, int max_len) {
	static	char	*buf[MAX_STATIC_ARRAY];
	static	u_long	idx;

	char	*&cur_buf = buf[idx++ % MAX_STATIC_ARRAY];
	if (cur_buf) delete [] cur_buf;
	return	(cur_buf = AtoU8(src, max_len));
}

char *U8toA(const char *src, int max_len) {
	char	*buf = NULL;

	WCHAR	*wsrc = U8toW(src, max_len);
	if (wsrc) buf = WtoA(wsrc, max_len);
	delete [] wsrc;
	return	buf;
}

char *U8toAs(const char *src, int max_len) {
	static	char	*buf[MAX_STATIC_ARRAY];
	static	u_long	idx;

	char	*&cur_buf = buf[idx++ % MAX_STATIC_ARRAY];
	if (cur_buf) delete [] cur_buf;
	return	(cur_buf = U8toA(src, max_len));
}


BOOL IsUTF8(const char *_s, BOOL *is_ascii, char **invalid_point)
{
	const u_char	*s = (const u_char *)_s;
	char 			*_invalid_point;
	BOOL			tmp;

	if (!is_ascii)      is_ascii = &tmp;
	if (!invalid_point) invalid_point = &_invalid_point;

	*is_ascii = TRUE;

	while (*s) {
		if (*s <= 0x7f) {
		}
		else {
			*is_ascii = FALSE;
			*invalid_point = (char *)s;

			if (*s <= 0xdf) {
				if ((*++s & 0xc0) != 0x80) return FALSE;
			}
			else if (*s <= 0xef) {
				if ((*++s & 0xc0) != 0x80) return FALSE;
				if ((*++s & 0xc0) != 0x80) return FALSE;
			}
			else if (*s <= 0xf7) {
				if ((*++s & 0xc0) != 0x80) return FALSE;
				if ((*++s & 0xc0) != 0x80) return FALSE;
				if ((*++s & 0xc0) != 0x80) return FALSE;
			}
			else if (*s <= 0xfb) {
				if ((*++s & 0xc0) != 0x80) return FALSE;
				if ((*++s & 0xc0) != 0x80) return FALSE;
				if ((*++s & 0xc0) != 0x80) return FALSE;
				if ((*++s & 0xc0) != 0x80) return FALSE;
			}
			else if (*s <= 0xfd) {
				if ((*++s & 0xc0) != 0x80) return FALSE;
				if ((*++s & 0xc0) != 0x80) return FALSE;
				if ((*++s & 0xc0) != 0x80) return FALSE;
				if ((*++s & 0xc0) != 0x80) return FALSE;
				if ((*++s & 0xc0) != 0x80) return FALSE;
			}
			else return FALSE;
		}
		s++;
	}
	return	TRUE;
}

BOOL StrictUTF8(char *s)
{
	char	*invalid_point = NULL;
	if (!IsUTF8(s, NULL, &invalid_point) && invalid_point) {
		*invalid_point = 0;
		return	TRUE;
	}
	return	FALSE;
}

