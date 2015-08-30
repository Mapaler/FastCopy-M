/* @(#)Copyright (C) 1996-2015 H.Shirouzu		tlib.h	Ver0.99 */
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Main Header
	Create					: 1996-06-01(Sat)
	Update					: 2015-08-12(Wed)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef TLIBMISC_H
#define TLIBMISC_H

#define ALIGN_SIZE(all_size, block_size) (((all_size) + (block_size) -1) \
										 / (block_size) * (block_size))
#define ALIGN_BLOCK(size, align_size) (((size) + (align_size) -1) / (align_size))

template<class T> class THashTblT;

template<class T>
class THashObjT {
public:
	THashObjT	*prevHash;
	THashObjT	*nextHash;
	T				hashId;

public:
	THashObjT() { prevHash = nextHash = NULL; hashId = 0; }
	virtual ~THashObjT() { if (prevHash && prevHash != this) UnlinkHash(); }

	virtual BOOL LinkHash(THashObjT *top) {
		if (prevHash) return FALSE;
		this->nextHash = top->nextHash;
		this->prevHash = top;
		top->nextHash->prevHash = this;
		top->nextHash = this;
		return TRUE;
	}
	virtual BOOL UnlinkHash() {
		if (!prevHash) return FALSE;
		prevHash->nextHash = nextHash;
		nextHash->prevHash = prevHash;
		prevHash = nextHash = NULL;
		return TRUE;
	}
	friend THashTblT<T>;
};

template<class T>
class THashTblT {
protected:
	THashObjT<T>	*hashTbl;
	int				hashNum;
	int				registerNum;
	BOOL			isDeleteObj;
	virtual BOOL	IsSameVal(THashObjT<T> *, const void *val) = 0;

public:
	THashTblT(int _hashNum=0, BOOL _isDeleteObj=TRUE) {
		hashTbl = NULL;
		registerNum = 0;
		isDeleteObj = _isDeleteObj;
		if ((hashNum = _hashNum) > 0) Init(hashNum);
	}
	virtual ~THashTblT() { UnInit(); }
	virtual BOOL Init(int _hashNum) {
		if ((hashTbl = new THashObjT<T> [hashNum = _hashNum]) == NULL) {
			return	FALSE;	// VC4's new don't occur exception
		}
		for (int i=0; i < hashNum; i++) {
			THashObjT<T>	*obj = hashTbl + i;
			obj->prevHash = obj->nextHash = obj;
		}
		registerNum = 0;
		return	TRUE;
	}
	virtual void UnInit() {
		if (!hashTbl) return;
		if (isDeleteObj) {
			for (int i=0; i < hashNum && registerNum > 0; i++) {
				THashObjT<T>	*start = hashTbl + i;
				for (THashObjT<T> *obj=start->nextHash; obj != start; ) {
					THashObjT<T> *next = obj->nextHash;
					delete obj;
					obj = next;
					registerNum--;
				}
			}
		}
		delete [] hashTbl;
		hashTbl = NULL;
		registerNum = 0;
	}
	virtual void Register(THashObjT<T> *obj, T hash_id) {
		obj->hashId = hash_id;
		if (obj->LinkHash(hashTbl + (hash_id % hashNum))) registerNum++;
	}
	virtual void UnRegister(THashObjT<T> *obj) {
		if (obj->UnlinkHash()) registerNum--;
	}
	virtual THashObjT<T> *Search(const void *data, T hash_id) {
		THashObjT<T> *top = hashTbl + (hash_id % hashNum);
		for (THashObjT<T> *obj=top->nextHash; obj != top; obj=obj->nextHash) {
			if (obj->hashId == hash_id && IsSameVal(obj, data)) return obj;
		}
		return	NULL;
	}
	virtual int		GetRegisterNum() { return registerNum; }
//	virtual T		MakeHashId(const void *data) = 0;
};

typedef THashObjT<u_int>	THashObj;
typedef THashTblT<u_int>	THashTbl;
typedef THashObjT<uint64> THashObj64;
typedef THashTblT<uint64> THashTbl64;

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
	enum Kind { INIT_EVENT=0, WAIT_EVENT, DONE_EVENT };

	struct Event {
		HANDLE	hEvent;
		u_int	kind;	// Kind が入るが compare and swap用に u_int に。
		Event() { hEvent = 0; kind = INIT_EVENT; }
	};
	static Event			*gEvents;
	static volatile LONG	gEventMap;
	static BOOL				InitGlobalEvents();
	static const int		MaxThreads = 32;

	BOOL			isInit;
	CRITICAL_SECTION cs;
	u_int			waitBits;

public:
	Condition(void);
	~Condition();

	BOOL Initialize(void);
	void UnInitialize(void);

	void Lock(void)		{ ::EnterCriticalSection(&cs); }
	void UnLock(void)	{ ::LeaveCriticalSection(&cs); }

	// ロックを取得してから利用すること
	int  IsWait()	{ return waitBits ? TRUE : FALSE; }

	BOOL Wait(DWORD timeout=INFINITE);
	void Notify(void);
};

#define PAGE_SIZE	(4 * 1024)

class VBuf {
protected:
	BYTE	*buf;
	VBuf	*borrowBuf;
	size_t	size;
	size_t	usedSize;
	size_t	maxSize;
	void	Init();

public:
	VBuf(size_t _size=0, size_t _max_size=0, VBuf *_borrowBuf=NULL);
	~VBuf();
	BOOL	AllocBuf(size_t _size, size_t _max_size=0, VBuf *_borrowBuf=NULL);
	BOOL	LockBuf();
	void	FreeBuf();
	BOOL	Grow(size_t grow_size);
	operator bool() { return buf ? true : false; }
	operator void *() { return buf; }
	operator BYTE *() { return buf; }
	operator char *() { return (char *)buf; }
	BYTE	*Buf() { return	buf; }
	WCHAR	*WBuf() { return (WCHAR *)buf; }
	size_t	Size() { return size; }
	size_t	MaxSize() { return maxSize; }
	size_t	UsedSize() { return usedSize; }
	void	SetUsedSize(size_t _used_size) { usedSize = _used_size; }
	size_t	AddUsedSize(size_t _used_size) { return usedSize += _used_size; }
	size_t	RemainSize(void) { return	size - usedSize; }
};

template <class T>
class VBVec : public VBuf {
	size_t	growSize;
	int		usedNum;

public:
	VBVec() {}
	BOOL Init(int min_num, int max_num, int grow_num=0) {
		size_t min_size = ALIGN_SIZE(min_num * sizeof(T), PAGE_SIZE);
		size_t max_size = ALIGN_SIZE(max_num * sizeof(T), PAGE_SIZE);
		growSize = grow_num ? ALIGN_SIZE(grow_num * sizeof(T), PAGE_SIZE) : min_size;
		usedNum  = 0;
		return AllocBuf(min_size, max_size);
	}
	T& operator [](int idx) {
		Aquire(idx);
		return	Get(idx);
	}
	bool Aquire(int idx) {
		int		need_num = idx + 1;
		if (need_num <= usedNum) return true;

		size_t	need_size = need_num * sizeof(T);
		if (need_size > size) {
			if (need_size > maxSize) return false;
			if (need_size > size && !Grow(growSize)) return false;
		}
		usedSize = need_size;
		usedNum  = need_num;
		return	true;
	}
	bool Set(int idx, const T& d) {
		if (!Aquire(idx)) return false;
		Get(idx) = d;
		return	true;
	}
	T& Get(int idx) {
		return *(T *)(buf + (sizeof(T) * idx));
	}
	int UsedNum() {
		return usedNum;
	}
	int SetUsedNum(int num) {
		usedNum  = num;
		usedSize = num * sizeof(T);
		return UsedNum();
	}
	bool Push(const T& d) {
		return	Set(UsedNum(), d); // usedNum will be increment in Aquire
	}
	bool Pop() {
		if (usedNum <= 0) return false;
		usedSize -= sizeof(T);
		usedNum--;
		return	true;
	}
	T& Top()  { return Get(0); }
	T& Last() { return Get(UsedNum()-1); }
};

class GBuf {
protected:
	HGLOBAL	hGlobal;
	BYTE	*buf;
	int		size;
	UINT	flags;

public:
	GBuf(int _size=0, BOOL with_lock=TRUE, UINT _flags=GMEM_MOVEABLE) {
		Init(_size, with_lock, _flags);
	}
	GBuf(VBuf *vbuf, BOOL with_lock=TRUE, UINT _flags=GMEM_MOVEABLE) {
		Init((int)vbuf->Size(), with_lock, _flags);
		if (buf) memcpy(buf, vbuf->Buf(), (int)vbuf->Size());
	}
	~GBuf() {
		UnInit();
	}
	BOOL Init(int _size=0, BOOL with_lock=TRUE, UINT _flags=GMEM_MOVEABLE) {
		hGlobal	= NULL;
		buf		= NULL;
		flags	= _flags;
		if ((size = _size) == 0) return TRUE;
		if (!(hGlobal = ::GlobalAlloc(flags, size))) return FALSE;
		if (!with_lock) return	TRUE;
		return	Lock() ? TRUE : FALSE;
	}
	void UnInit() {
		if (buf && (flags & GMEM_FIXED)) ::GlobalUnlock(buf);
		if (hGlobal) ::GlobalFree(hGlobal);
		buf		= NULL;
		hGlobal	= NULL;
	}
	HGLOBAL	Handle() { return hGlobal; }
	BYTE *Buf() {
		return	buf;
	}
	BYTE *Lock() {
		if ((flags & GMEM_FIXED))	buf = (BYTE *)hGlobal;
		else						buf = (BYTE *)::GlobalLock(hGlobal);
		return	buf;
	}
	void Unlock() {
		if (!(flags & GMEM_FIXED)) {
			::GlobalUnlock(buf);
			buf = NULL;
		}
		
	}
	int	Size() { return size; }
};

class DynBuf {
protected:
	char	*buf;
	int		size;

public:
	DynBuf(int _size=0)	{
		buf = NULL;
		if ((size = _size) > 0) Alloc(size);
	}
	~DynBuf() {
		free(buf);
	}
	char *Alloc(int _size) {
		if (buf) free(buf);
		buf = NULL;
		if ((size = _size) <= 0) return NULL;
		return	(buf = (char *)malloc(size));
	}
	operator char*()	{ return (char *)buf; }
	operator BYTE*()	{ return (BYTE *)buf; }
	operator WCHAR*()	{ return (WCHAR *)buf; }
	operator void*()	{ return (void *)buf; }
	int	Size()			{ return size; }
};

void InitInstanceForLoadStr(HINSTANCE hI);
LPSTR GetLoadStrA(UINT resId, HINSTANCE hI=NULL);
LPSTR GetLoadStrU8(UINT resId, HINSTANCE hI=NULL);
LPWSTR GetLoadStrW(UINT resId, HINSTANCE hI=NULL);
void TSetDefaultLCID(LCID id=0);
HMODULE TLoadLibrary(LPSTR dllname);
HMODULE TLoadLibraryW(WCHAR *dllname);
int MakePath(char *dest, const char *dir, const char *file);
int MakePathW(WCHAR *dest, const WCHAR *dir, const WCHAR *file);

int64 hex2ll(char *buf);
int bin2hexstr(const BYTE *bindata, int len, char *buf);
int bin2hexstr_revendian(const BYTE *bin, int len, char *buf);
int bin2hexstrW(const BYTE *bindata, int len, WCHAR *buf);
BOOL hexstr2bin(const char *buf, BYTE *bindata, int maxlen, int *len);
BOOL hexstr2bin_revendian(const char *buf, BYTE *bindata, int maxlen, int *len);

int bin2b64str(const BYTE *bindata, int len, char *buf);
int bin2b64str_revendian(const BYTE *bin, int len, char *buf);
BOOL b64str2bin(const char *buf, BYTE *bindata, int maxlen, int *len);
BOOL b64str2bin_revendian(const char *buf, BYTE *bindata, int maxlen, int *len);

int bin2urlstr(const BYTE *bindata, int len, char *str);
BOOL urlstr2bin(const char *str, BYTE *bindata, int maxlen, int *len);

void rev_order(BYTE *data, int size);
void rev_order(const BYTE *src, BYTE *dst, int size);

char *strdupNew(const char *_s, int max_len=-1);
WCHAR *wcsdupNew(const WCHAR *_s, int max_len=-1);

int strcpyz(char *dest, const char *src);
int wcscpyz(WCHAR *dest, const WCHAR *src);
int strncpyz(char *dest, const char *src, int num);
int wcsncpyz(WCHAR *dest, const WCHAR *src, int num);

inline int get_ntz64(uint64 val) {
#ifdef _WIN64
	u_long	ret = 0;
	_BitScanForward64(&ret, val);
	return	ret;
#else
	u_long	ret = 0;
	if (_BitScanForward(&ret, (u_int)val)) return ret;

	u_int sval = (u_int)(val >> 32);
	_BitScanForward(&ret, sval);
	ret += 32;
	return	ret;
#endif
}

inline int get_ntz(u_int val) {
	u_long	ret = 0;
	_BitScanForward(&ret, val);
	return	ret;
}


int LocalNewLineToUnix(const char *src, char *dest, int maxlen);
int UnixNewLineToLocal(const char *src, char *dest, int maxlen);

BOOL TIsWow64();
BOOL TRegEnableReflectionKey(HKEY hBase);
BOOL TRegDisableReflectionKey(HKEY hBase);
BOOL TWow64DisableWow64FsRedirection(void *oldval);
BOOL TWow64RevertWow64FsRedirection(void *oldval);
BOOL TIsEnableUAC();
BOOL TIsVirtualizedDirW(WCHAR *path);
BOOL TMakeVirtualStorePathW(WCHAR *org_path, WCHAR *buf);
BOOL TSetPrivilege(LPSTR privName, BOOL bEnable);
BOOL TSetThreadLocale(int lcid);
BOOL TChangeWindowMessageFilter(UINT msg, DWORD flg);
void TSwitchToThisWindow(HWND hWnd, BOOL flg);

BOOL InstallExceptionFilter(const char *title, const char *info, const char *fname=NULL);
void Debug(const char *fmt,...);
void DebugW(const WCHAR *fmt,...);
void DebugU8(const char *fmt,...);
const char *Fmt(const char *fmt,...);
const WCHAR *FmtW(const WCHAR *fmt,...);

BOOL SymLinkW(WCHAR *src, WCHAR *dest, WCHAR *arg=L"");
BOOL ReadLinkW(WCHAR *src, WCHAR *dest, WCHAR *arg=NULL);
BOOL DeleteLinkW(WCHAR *path);
BOOL GetParentDirW(const WCHAR *srcfile, WCHAR *dir);
HWND ShowHelpW(HWND hOwner, WCHAR *help_dir, WCHAR *help_file, WCHAR *section=NULL);
HWND ShowHelpU8(HWND hOwner, const char *help_dir, const char *help_file, const char *section=NULL);
HWND CloseHelpAll();

BOOL ForceSetTrayIcon(HWND hWnd, UINT id, DWORD pref=2);

#endif

