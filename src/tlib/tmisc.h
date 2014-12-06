/* @(#)Copyright (C) 1996-2011 H.Shirouzu		tlib.h	Ver0.99 */
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Main Header
	Create					: 1996-06-01(Sat)
	Update					: 2011-05-23(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef TLIBMISC_H
#define TLIBMISC_H

class THashTbl;

class THashObj {
public:
	THashObj	*priorHash;
	THashObj	*nextHash;
	u_int		hashId;

public:
	THashObj() { priorHash = nextHash = NULL; hashId = 0; }
	virtual ~THashObj() { if (priorHash && priorHash != this) UnlinkHash(); }

	virtual BOOL LinkHash(THashObj *top);
	virtual BOOL UnlinkHash();
	friend THashTbl;
};

class THashTbl {
protected:
	THashObj	*hashTbl;
	int			hashNum;
	int			registerNum;
	BOOL		isDeleteObj;

	virtual BOOL	IsSameVal(THashObj *, const void *val) = 0;

public:
	THashTbl(int _hashNum=0, BOOL _isDeleteObj=TRUE);
	virtual ~THashTbl();
	virtual BOOL	Init(int _hashNum);
	virtual void	UnInit();
	virtual void	Register(THashObj *obj, u_int hash_id);
	virtual void	UnRegister(THashObj *obj);
	virtual THashObj *Search(const void *data, u_int hash_id);
	virtual int		GetRegisterNum() { return registerNum; }
//	virtual u_int	MakeHashId(const void *data) = 0;
};

/* for internal use start */
struct TResHashObj : THashObj {
	void	*val;
	TResHashObj(UINT _resId, void *_val) { hashId = _resId; val = _val; }
	~TResHashObj() { free(val); }
	
};

class TResHash : public THashTbl {
protected:
	virtual BOOL IsSameVal(THashObj *obj, const void *val) {
		return obj->hashId == *(u_int *)val;
	}

public:
	TResHash(int _hashNum) : THashTbl(_hashNum) {}
	TResHashObj	*Search(UINT resId) { return (TResHashObj *)THashTbl::Search(&resId, resId); }
	void		Register(TResHashObj *obj) { THashTbl::Register(obj, obj->hashId); }
};

class Condition {
protected:
	enum WaitEvent { CLEAR_EVENT=0, DONE_EVENT, WAIT_EVENT };
	CRITICAL_SECTION	cs;
	HANDLE				*hEvents;
	WaitEvent			*waitEvents;
	int					max_threads;
	int					waitCnt;

public:
	Condition(void);
	~Condition();

	BOOL Initialize(int _max_threads);
	void UnInitialize(void);

	void Lock(void)		{ ::EnterCriticalSection(&cs); }
	void UnLock(void)	{ ::LeaveCriticalSection(&cs); }

	// ロックを取得してから利用すること
	int  WaitThreads()	{ return waitCnt; }
	int  IsWait()		{ return waitCnt ? TRUE : FALSE; }
	void DetachThread() { max_threads--; }
	int  MaxThreads()   { return max_threads; }

	BOOL Wait(DWORD timeout=INFINITE);
	void Notify(void);
};

#define PAGE_SIZE	(4 * 1024)

class VBuf {
protected:
	BYTE	*buf;
	VBuf	*borrowBuf;
	int		size;
	int		usedSize;
	int		maxSize;
	void	Init();

public:
	VBuf(int _size=0, int _max_size=0, VBuf *_borrowBuf=NULL);
	~VBuf();
	BOOL	AllocBuf(int _size, int _max_size=0, VBuf *_borrowBuf=NULL);
	BOOL	LockBuf();
	void	FreeBuf();
	BOOL	Grow(int grow_size);
	operator BYTE *() { return buf; }
	BYTE	*Buf() { return	buf; }
	WCHAR	*WBuf() { return (WCHAR *)buf; }
	int		Size() { return size; }
	int		MaxSize() { return maxSize; }
	int		UsedSize() { return usedSize; }
	void	SetUsedSize(int _used_size) { usedSize = _used_size; }
	int		AddUsedSize(int _used_size) { return usedSize += _used_size; }
	int		RemainSize(void) { return	size - usedSize; }
};

void InitInstanceForLoadStr(HINSTANCE hI);
LPSTR GetLoadStrA(UINT resId, HINSTANCE hI=NULL);
LPSTR GetLoadStrU8(UINT resId, HINSTANCE hI=NULL);
LPWSTR GetLoadStrW(UINT resId, HINSTANCE hI=NULL);
void TSetDefaultLCID(LCID id=0);
HMODULE TLoadLibrary(LPSTR dllname);
HMODULE TLoadLibraryV(void *dllname);
int MakePath(char *dest, const char *dir, const char *file);
int MakePathW(WCHAR *dest, const WCHAR *dir, const WCHAR *file);
WCHAR lGetCharIncW(const WCHAR **str);
WCHAR lGetCharIncA(const char **str);
WCHAR lGetCharW(const WCHAR *str, int);
WCHAR lGetCharA(const char *str, int);
void lSetCharW(WCHAR *str, int offset, WCHAR ch);
void lSetCharA(char *str, int offset, WCHAR ch);

_int64 hex2ll(char *buf);
int bin2hexstr(const BYTE *bindata, int len, char *buf);
int bin2hexstr_revendian(const BYTE *bin, int len, char *buf);
int bin2hexstrW(const BYTE *bindata, int len, WCHAR *buf);
BOOL hexstr2bin(const char *buf, BYTE *bindata, int maxlen, int *len);
BOOL hexstr2bin_revendian(const char *buf, BYTE *bindata, int maxlen, int *len);

int bin2b64str(const BYTE *bindata, int len, char *buf);
int bin2b64str_revendian(const BYTE *bin, int len, char *buf);
BOOL b64str2bin(const char *buf, BYTE *bindata, int maxlen, int *len);
BOOL b64str2bin_revendian(const char *buf, BYTE *bindata, int maxlen, int *len);

void rev_order(BYTE *data, int size);
void rev_order(const BYTE *src, BYTE *dst, int size);

char *strdupNew(const char *_s);
WCHAR *wcsdupNew(const WCHAR *_s);

int strncmpi(const char *str1, const char *str2, size_t num);
char *strncpyz(char *dest, const char *src, size_t num);

int LocalNewLineToUnix(const char *src, char *dest, int maxlen);
int UnixNewLineToLocal(const char *src, char *dest, int maxlen);

BOOL TIsWow64();
BOOL TRegEnableReflectionKey(HKEY hBase);
BOOL TRegDisableReflectionKey(HKEY hBase);
BOOL TWow64DisableWow64FsRedirection(void *oldval);
BOOL TWow64RevertWow64FsRedirection(void *oldval);
BOOL TIsUserAnAdmin();
BOOL TIsEnableUAC();
BOOL TSHGetSpecialFolderPathV(HWND, void *, int, BOOL);
BOOL TIsVirtualizedDirV(void *path);
BOOL TMakeVirtualStorePathV(void *org_path, void *buf);
BOOL TSetPrivilege(LPSTR pszPrivilege, BOOL bEnable);
BOOL TSetThreadLocale(int lcid);
BOOL TChangeWindowMessageFilter(UINT msg, DWORD flg);
void TSwitchToThisWindow(HWND hWnd, BOOL flg);

BOOL InstallExceptionFilter(char *title, char *info);
void Debug(char *fmt,...);
void DebugW(WCHAR *fmt,...);
void DebugU8(char *fmt,...);
const char *Fmt(char *fmt,...);
const WCHAR *FmtW(WCHAR *fmt,...);

BOOL SymLinkV(void *src, void *dest, void *arg=L"");
BOOL ReadLinkV(void *src, void *dest, void *arg=NULL);
BOOL DeleteLinkV(void *path);
BOOL GetParentDirV(const void *srcfile, void *dir);
HWND ShowHelpV(HWND hOwner, void *help_dir, void *help_file, void *section=NULL);
HWND ShowHelpU8(HWND hOwner, const char *help_dir, const char *help_file, const char *section=NULL);

#endif

