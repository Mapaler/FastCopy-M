/* @(#)Copyright (C) 1996-2017 H.Shirouzu		tapi32u8.h	Ver0.99 */
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Main Header
	Create					: 2005-04-10(Sun)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef TAPI32U8_H
#define TAPI32U8_H

struct WIN32_FIND_DATA_U8 {
	DWORD		dwFileAttributes;
	FILETIME	ftCreationTime;
	FILETIME	ftLastAccessTime;
	FILETIME	ftLastWriteTime;
	DWORD		nFileSizeHigh;
	DWORD		nFileSizeLow;
	DWORD		dwReserved0;
	DWORD		dwReserved1;
	char		cFileName[ MAX_PATH_U8 ];
	char		cAlternateFileName[ 14 * 3 ];
};


#define CP_UTF8                   65001       // UTF-8 translation
int WtoU8(const WCHAR *src, char *dst, int bufsize, int max_len=-1);
int U8toW(const char *src, WCHAR *dst, int bufsize, int max_len=-1);
int AtoW(const char *src, WCHAR *dst, int bufsize, int max_len=-1);
int WtoA(const WCHAR *src, char *dst, int bufsize, int max_len=-1);

/* dynamic allocation */
WCHAR *U8toW(const char *src, int max_len=-1);
//WCHAR *WtoW(const WCHAR *src, int max_len=-1) { return wcsdupNew(src, max_len); }
char *WtoU8(const WCHAR *src, int max_len=-1);
char *WtoA(const WCHAR *src, int max_len=-1);
char *AtoU8(const char *src, int max_len=-1);
char *U8toA(const char *src, int max_len=-1);
WCHAR *AtoW(const char *src, int max_len=-1);

/* use static buffer */
WCHAR *U8toWs(const char *src, int max_len=-1);
WCHAR *WtoWs(const WCHAR *src, int max_len=-1);
char *WtoU8s(const WCHAR *src, int max_len=-1);
char *WtoAs(const WCHAR *src, int max_len=-1);
char *AtoU8s(const char *src, int max_len=-1);
char *U8toAs(const char *src, int max_len=-1);
WCHAR *AtoWs(const char *src, int max_len=-1);

inline int WtoS(LPCWSTR src, char *dst, int bufsize, StrMode mode, int max_len=-1) {
	return (mode == BY_UTF8) ? WtoU8(src, dst, bufsize, max_len)
							 : WtoA(src, dst, bufsize, max_len);
}

// Win32(W) API UTF8 wrapper
BOOL GetMenuStringU8(HMENU hMenu, UINT uItem, char *buf, int bufsize, UINT flags);
DWORD GetModuleFileNameU8(HMODULE hModule, char *buf, DWORD bufsize);
UINT GetDriveTypeU8(const char *path);

class Wstr {
	WCHAR		*str;
	mutable int	len;
	void FirstInit() {
		str = NULL;
		len = -1;
	}

public:
	Wstr(const char *_str=NULL, StrMode mode=BY_UTF8) {
		FirstInit();
		Init(_str, mode);
	}
	Wstr(const WCHAR *_str, int _len=-1) {
		FirstInit();
		Init(_str, _len);
	}
	Wstr(const Wstr& w) {
		FirstInit();
		Init(w);
	}
	Wstr(int _len) {
		FirstInit();
		Init(_len);
	}
	~Wstr() { UnInit(); }

	void Init(const Wstr &w) {
		Init(w.str);
		len = w.len;
	}
	void Init(const WCHAR *_str, int _len=-1) {
		UnInit();
		str = (_str ? wcsdupNew(_str, _len) : NULL);
		len = -1;
	}
	void Init(const char *_str, StrMode mode=BY_UTF8) {
		UnInit();
		str = _str ? mode == BY_UTF8 ? U8toW(_str) : AtoW(_str) : NULL;
		len = -1;
	}
	void Init(int _len=0) {
		UnInit();
		if (_len) {
			str = new WCHAR [_len + 1];
			*str=0;
		} else {
			str=NULL;
		}
		len = -1;
	}
	void UnInit() {
		if (str) {
			delete [] str;
			str = NULL;
		}
		len = -1;
	}
	Wstr &operator =(const WCHAR *_str) {
		Init(_str);
		return *this;
	}
	Wstr &operator =(const Wstr &w) {
		Init(w);
		return *this;
	}
	Wstr &operator +=(const WCHAR *_str) {
		if (_str && *_str) {
			int		len_l = Len();
			int		len_s = int(wcslen(_str));
			WCHAR	*buf  = new WCHAR [len_l + len_s + 1];
			if (str) {
				memcpy(buf, str, len_l * sizeof(WCHAR));
				delete [] str;
			}
			str = buf;
			memcpy(str + len_l, _str, (len_s + 1) * sizeof(WCHAR));
			len = len_l + len_s;
		}
		return	*this;
	}
	bool operator ==(const WCHAR *_str) const {
		if (!_str) {
			return IsEmpty();
		}
		return	wcscmp(s(), _str) == 0;
	}
	operator bool() {
		return !IsEmpty();
	}
	bool operator ==(const Wstr &_w) const {
		return _w == s();
	}
	bool operator !=(const WCHAR *_str) const {
		return !(*this == _str);
	}
	bool operator !=(const Wstr &_w) const {
		return !(_w == str);
	}
	bool operator <(const Wstr &_w) const {
		return wcscmp(s(), _w.s()) < 0;
	}
	WCHAR &operator [](int idx) {
		return str[idx];
	}
	WCHAR	*Buf() {
		len = -1;
		return str;
	}
	void UnBuf() {
		len = -1;
	}
	const WCHAR *s() const {
		return str ? str : L"";
	}
	bool IsEmpty() const {
		return !str || *str == 0;
	}
	int	Len() const {
		if (len >= 0) {
			return len;
		}
		if (!str) {
			return 0;
		}
		return (len = (int)wcslen(str));
	}
	int StripLen() const {
		if (len < 0) {
			Len();
		}
		int	strip_len = len;
		for ( ; strip_len > 0; strip_len--) {
			WCHAR	ch = str[strip_len-1];
			if (ch != '\n' && ch != '\r') {
				break;
			}
		}
		return	strip_len;
	}
	BOOL GetLine(int idx, Wstr *ln, BOOL is_strip=TRUE) {
		ln->Init();
		if (!str || !*str) return FALSE;

		int	cnt=0;
		const WCHAR *p = str;
		while (p && *p) {
			const WCHAR *next = wcschr(p, '\n');
			if (next) next++;
			if (cnt == idx) {
				if (next) {
					ln->Init(p, int(next - p));
				} else {
					*ln = p;
				}
				if (is_strip) ln->Strip();
				return TRUE;
			}
			cnt++;
			p = next;
		}
		return	FALSE;
	}
	void Strip(const WCHAR *strips=L"\r\n") {
		int len = Len();
		while (wcschr(strips, *str)) {
			memmove(str, str+1, (len--) * sizeof(WCHAR));
		}
		while (wcschr(strips, str[len-1])) {
			len--;
		}
		str[len] = 0;
	}
	int LineNum() {
		int	lines = 0;

		const WCHAR *p = str;
		while (p && *p) {
			lines++;
			if ((p = wcschr(p, '\n'))) {
				p++;
			}
		}
		if (p && *p) lines++;
		return	lines;
	}
};

class U8str {
	char	*str;
	mutable int	len;
	void FirstInit() {
		str = NULL;
		len = -1;
	}

public:
	U8str(const char *_str, StrMode mode=BY_UTF8, int _len=-1) {
		FirstInit();
		Init(_str, mode, _len);
	}
	U8str() {
		FirstInit();
		Init(NULL, BY_UTF8, -1);
	}
	U8str(const WCHAR *_str, int _len=-1) {
		FirstInit();
		Init(_str, _len);
	}
	U8str(const U8str& u) {
		FirstInit();
		Init(u.str);
	}
	U8str(int _len) {
		FirstInit();
		Init(_len);
	}
	~U8str() {
		UnInit();
	}

	void Init(const Wstr &w) {
		Init(w.s());
		len = w.Len();
	}
	void Init(const WCHAR *_str, int _len=-1) {
		UnInit();
		str = _str ? WtoU8(_str, _len) : NULL;
		len = -1;
	}
	void Init(const char *_str, StrMode mode=BY_UTF8, int _len=-1) {
		UnInit();
		str = _str ? (mode == BY_UTF8) ? strdupNew(_str, _len) : AtoU8(_str) : NULL;
		len = -1;
	}
	void Init(int _len=0) {
		UnInit();
		if (_len) {
			str = new char [_len + 1];
			*str=0;
		} else {
			str=NULL;
		}
		len = -1;
	}
	void UnInit() {
		if (str) {
			delete [] str;
			str = NULL;
		}
		len = -1;
	}
	U8str &operator =(const char *_str) {
		Init(_str);
		return *this;
	}
	U8str &operator =(const U8str &u) {
		Init(u.str);
		return *this;
	}
	U8str &operator =(const WCHAR *w) {
		Init(w);
		return *this;
	}
	U8str &operator +=(const char *_str) {
		if (_str && *_str) {
			int		len_l = Len();
			int		len_s = int(strlen(_str));
			char	*buf  = new char [len_l + len_s + 1];
			if (str) {
				memcpy(buf, str, len_l * sizeof(char));
				delete [] str;
			}
			str = buf;
			memcpy(str + len_l, _str, (len_s + 1) * sizeof(char));
			len = len_l + len_s;
		}
		return	*this;
	}
	bool operator ==(const char *_str) const {
		if (!str || !_str) {
			return str == _str;
		}
		return	strcmp(str, _str) == 0;
	}
	operator bool() {
		return !IsEmpty();
	}
	bool operator ==(const U8str &_u) const {
		return _u == str;
	}
	bool operator !=(const char *_str) const {
		return !(*this == _str);
	}
	bool operator !=(const U8str &_u) const {
		return !(_u == str);
	}
	bool operator <(const U8str &_u) const {
		return strcmp(s(), _u.s()) < 0;
	}
	char &operator [](int idx) {
		return str[idx];
	}
	char *Buf() {
		len = -1;
		return str;
	}
	void UnBuf() {
		len = -1;
	}
	const char *s() const {
		return str ? str : "";
	}
	bool IsEmpty() const {
		return !str || *str == 0;
	}
	int	Len() const {
		if (len >= 0) {
			return len;
		}
		if (!str) {
			return 0;
		}
		return (len = (int)strlen(str));
	}
	BOOL GetLine(int idx, U8str *ln, BOOL is_strip=TRUE) {
		ln->Init();
		if (!str || !*str) return FALSE;

		int	cnt=0;
		const char *p = str;
		while (p && *p) {
			const char *next = strchr(p, '\n');
			if (next) next++;
			if (cnt == idx) {
				if (next) {
					ln->Init(p, BY_UTF8, int(next - p));
				} else {
					*ln = p;
				}
				if (is_strip) ln->Strip();
				return TRUE;
			}
			cnt++;
			p = next;
		}
		return	FALSE;
	}
	int LineNum() {
		int	lines = 0;

		const char *p = str;
		while (p && *p) {
			lines++;
			if ((p = strchr(p, '\n'))) {
				p++;
			}
		}
		if (p && *p) lines++;
		return	lines;
	}
	void Strip(const char *strips="\r\n") {
		int len = Len();
		while (strchr(strips, *str)) {
			memmove(str, str+1, len--);
		}
		while (strchr(strips, str[len-1])) {
			len--;
		}
		str[len] = 0;
	}
};

class MBCSstr {
	char	*str;
	mutable int	len;
	void FirstInit() {
		str = NULL;
		len = -1;
	}

public:
	MBCSstr(const char *_str=NULL, StrMode mode=BY_MBCS) {
		FirstInit();
		Init(_str, mode);
	}
	MBCSstr(const WCHAR *_str, int _len=-1) {
		FirstInit();
		Init(_str, _len);
	}
	MBCSstr(const MBCSstr& m) {
		FirstInit();
		Init(m.str);
	}
	MBCSstr(int _len) {
		FirstInit();
		Init(_len);
	}
	~MBCSstr() { UnInit(); }

	void Init(const Wstr &w) {
		Init(w.s());
		len = w.Len();
	}
	void Init(const WCHAR *_str, int _len=-1) {
		UnInit();
		str = _str ? WtoA(_str, _len) : NULL;
		len = -1;
	}
	void Init(const char *_str, StrMode mode=BY_MBCS) {
		UnInit();
		str = _str ? (mode == BY_MBCS) ? strdupNew(_str) : U8toA(_str) : NULL;
		len = -1;
	}
	void Init(int _len=0) {
		UnInit();
		if (_len) {
			str = new char [_len + 1];
			*str=0;
		} else {
			str=NULL;
		}
		len = -1;
	}
	void UnInit() {
		if (str) {
			delete [] str;
			str = NULL;
		}
		len = -1;
	}
	MBCSstr &operator =(const char *_str) {
		Init(_str);
		return *this;
	}
	MBCSstr &operator =(const MBCSstr &m) {
		Init(m.str);
		return *this;
	}
	MBCSstr &operator =(const WCHAR *w) {
		Init(w);
		return *this;
	}
	bool operator ==(const char *_str) const {
		if (!str || !_str) {
			return str == _str;
		}
		return	strcmp(str, _str) == 0;
	}
	operator bool() {
		return !IsEmpty();
	}
	bool operator ==(const MBCSstr &_m) const {
		return _m == str;
	}
	bool operator !=(const char *_str) const {
		return !(*this == _str);
	}
	bool operator !=(const MBCSstr &_m) const {
		return !(_m == str);
	}
	bool operator <(const MBCSstr &_m) const {
		return strcmp(s(), _m.s()) < 0;
	}
	char &operator [](int idx) {
		return str[idx];
	}
	char *Buf() {
		len = -1;
		return str;
	}
	void UnBuf() {
		len = -1;
	}
	const char *s() const {
		return str ? str : "";
	}
	bool IsEmpty() const {
		return !str || *str == 0;
	}
	int	Len() const {
		if (len >= 0) {
			return len;
		}
		if (!str) {
			return 0;
		}
		return (len = (int)strlen(str));
	}
};

inline int U8toA(const char *src, char *dst, int bufsize) {
	MBCSstr	ms(src, BY_UTF8);
	strncpyz(dst, ms.s(), bufsize);
	return	(int)strlen(dst);
}

inline int AtoU8(const char *src, char *dst, int bufsize) {
	U8str	u8(src, BY_MBCS);
	strncpyz(dst, u8.s(), bufsize);
	return	(int)strlen(dst);
}

BOOL IsUTF8(const char *s, BOOL *is_ascii=NULL, char **invalid_point=NULL, int max_len=INT_MAX);
BOOL StrictUTF8(char *s);
int u8cpyz(char *d, const char *s, int max_len);

BOOL StrictUTF8(char *s);
int U8Len(const char *s, int max_size=-1);

inline BOOL IsSurrogate(WCHAR ch) {
	return (ch >= 0xd800 && ch < 0xe000);
}

inline BOOL IsSurrogateL(WCHAR ch) {
	return (ch >= 0xd800 && ch < 0xdc00);
}

inline BOOL IsSurrogateR(WCHAR ch) {
	return (ch >= 0xdc00 && ch < 0xe000);
}

inline BOOL IsSurrogatePair(const WCHAR *s) {
	return IsSurrogateL(*s) && IsSurrogateR(*(s+1));
}

inline BOOL IsIVSL(const WCHAR ch) {
	return ch == 0xdb40;
}

inline BOOL IsIVSR(const WCHAR ch) {
	return (ch >= 0xdd00 && ch < 0xddff);
}

inline BOOL IsIVSOne(WCHAR ch) {
	return	(ch >= 0x180b && ch < 0x180e || ch >= 0xfe00 && ch < 0xfe10);
}

inline BOOL IsIVSAny(WCHAR ch) {
	return	IsIVSL(ch) || IsIVSR(ch) || IsIVSOne(ch);
}

inline BOOL IsIVS(const WCHAR *s, int *len=NULL) {
	if (IsIVSOne(*s)) {
		if (len) *len = 1;
		return	TRUE;
	}
	else if (IsIVSL(*s) && IsIVSR(*(s+1))) {
		if (len) *len = 2;
		return	TRUE;
	}
	return	FALSE;
}

HWND CreateWindowU8(const char *class_name, const char *window_name, DWORD style,
	int x, int y, int width, int height, HWND hParent, HMENU hMenu, HINSTANCE hInst, void *param);
HWND FindWindowU8(const char *class_name, const char *window_name=NULL);
BOOL AppendMenuU8(HMENU hMenu, UINT flags, UINT_PTR idItem, const char *item_str);
BOOL InsertMenuU8(HMENU hMenu, UINT idItem, UINT flags, UINT_PTR idNewItem, const char *item_str);
BOOL ModifyMenuU8(HMENU hMenu, UINT idItem, UINT flags, UINT_PTR idNewItem, const char *item_str);
DWORD GetFileAttributesU8(const char *path);
BOOL SetFileAttributesU8(const char *path, DWORD attr);
BOOL MoveFileExU8(const char *src, const char *dst, DWORD flags);

UINT DragQueryFileU8(HDROP hDrop, UINT iFile, char *buf, UINT cb);
void WIN32_FIND_DATA_WtoU8(const WIN32_FIND_DATAW *fdat_w, WIN32_FIND_DATA_U8 *fdat_u8,
	BOOL include_fname=TRUE);
HANDLE FindFirstFileU8(const char *path, WIN32_FIND_DATA_U8 *fdat);
BOOL FindNextFileU8(HANDLE hDir, WIN32_FIND_DATA_U8 *fdat);
DWORD GetFullPathNameU8(const char *path, DWORD size, char *buf, char **fname);
BOOL GetFileInfomationU8(const char *path, WIN32_FIND_DATA_U8 *fdata);
HANDLE CreateFileU8(const char *path, DWORD access_flg, DWORD share_flg,
	SECURITY_ATTRIBUTES *sa, DWORD create_flg, DWORD attr_flg, HANDLE hTemplate);
BOOL CreateDirectoryU8(const char *path, SECURITY_ATTRIBUTES *lsa);
BOOL DeleteFileU8(const char *path);
BOOL RemoveDirectoryU8(const char *path);

HINSTANCE ShellExecuteU8(HWND hWnd, LPCSTR op, LPCSTR file, LPSTR params, LPCSTR dir, int nShow);
BOOL ShellExecuteExU8(SHELLEXECUTEINFO *info);
DWORD GetCurrentDirectoryU8(DWORD size, char *dir);
DWORD GetWindowsDirectoryU8(char *dir, DWORD size);
BOOL SetCurrentDirectoryU8(char *dir);
BOOL GetOpenFileNameU8(LPOPENFILENAME ofn);
BOOL GetSaveFileNameU8(LPOPENFILENAME ofn);
BOOL PlaySoundU8(const char *path, HMODULE hmod, DWORD flg);
BOOL SHGetSpecialFolderPathU8(HWND hWnd, char *path, int csidl, BOOL fCreate=FALSE);

inline int TMessageBox(LPCSTR msg, LPCSTR title="msg", UINT style=MB_OK) {
	return	::MessageBox(0, msg, title, style);
}
inline int TMessageBoxW(LPCWSTR msg, LPCWSTR title=L"msg", UINT style=MB_OK) {
	return	::MessageBoxW(0, msg, title, style);
}
inline int TMessageBoxU8(LPCSTR msg, LPCSTR title="msg", UINT style=MB_OK) {
	Wstr	wmsg(msg);
	Wstr	wtitle(title);
	return	::MessageBoxW(0, wmsg.s(), wtitle.s(), style);
}

#endif
