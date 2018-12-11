/* @(#)Copyright (C) 1996-2017 H.Shirouzu		tlib.h	Ver0.99 */
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Main Header
	Create					: 1996-06-01(Sat)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef TLIBMISC_H
#define TLIBMISC_H

typedef SSIZE_T	ssize_t;

#define ALIGN_SIZE(all_size, block_size) (((all_size) + (block_size) -1) \
										 / (block_size) * (block_size))
#define ALIGN_BLOCK(size, align_size) (((size) + (align_size) -1) / (align_size))

template<class T> class THashTblT;

template<class T>
class THashObjT {
public:
	THashObjT	*prevHash = NULL;
	THashObjT	*nextHash = NULL;
	T			hashId    = 0;

public:
	THashObjT() {
	}
	virtual ~THashObjT() {
		if (prevHash && prevHash != this) {
			UnlinkHash();
		}
	}
	virtual BOOL LinkHash(THashObjT *top) {
		if (prevHash) {
			return FALSE;
		}
		this->nextHash = top->nextHash;
		this->prevHash = top;
		top->nextHash->prevHash = this;
		top->nextHash = this;
		return TRUE;
	}
	virtual BOOL UnlinkHash() {
		if (!prevHash) {
			return FALSE;
		}
		prevHash->nextHash = nextHash;
		nextHash->prevHash = prevHash;
		prevHash = nextHash = NULL;
		return TRUE;
	}
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
		if ((hashNum = _hashNum) > 0) {
			Init(hashNum);
		}
	}
	virtual ~THashTblT() { UnInit(); }
	virtual BOOL Init(int _hashNum) {
		hashTbl = new THashObjT<T> [hashNum = _hashNum];
		if (hashTbl == NULL) {
			return	FALSE;	// VC4's new don't occur exception
		}
		for (int i=0; i < hashNum; i++) {
			auto obj = hashTbl + i;
			obj->prevHash = obj->nextHash = obj;
		}
		registerNum = 0;
		return	TRUE;
	}
	virtual void UnInit() {
		if (!hashTbl) {
			return;
		}
		if (isDeleteObj) {
			for (int i=0; i < hashNum && registerNum > 0; i++) {
				auto	start = hashTbl + i;
				for (auto obj=start->nextHash; obj != start; ) {
					auto next = obj->nextHash;
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
		if (obj->LinkHash(hashTbl + (hash_id % hashNum))) {
			registerNum++;
		}
	}
	virtual void UnRegister(THashObjT<T> *obj) {
		if (obj->UnlinkHash()) {
			registerNum--;
		}
	}
	virtual void Reset() {
		UnInit();
		Init(hashNum);
	}
	virtual THashObjT<T> *Search(const void *data, T hash_id) {
		auto top = hashTbl + (hash_id % hashNum);
		for (auto obj=top->nextHash; obj != top; obj=obj->nextHash) {
			if (obj->hashId == hash_id && IsSameVal(obj, data)) {
				return obj;
			}
		}
		return	NULL;
	}
	virtual int	GetRegisterNum() {
		return registerNum;
	}
//	virtual T		MakeHashId(const void *data) = 0;
};

typedef THashObjT<u_int>	THashObj;
typedef THashTblT<u_int>	THashTbl;
typedef THashObjT<uint64> THashObj64;
typedef THashTblT<uint64> THashTbl64;

/* for internal use start */
struct TResHashObj : THashObj {
	void	*val;

	TResHashObj(UINT _resId, void *_val) {
		hashId = _resId;
		val = _val;
	}

	~TResHashObj() {
		free(val);
	}
	
};

class TResHash : public THashTbl {
protected:
	virtual BOOL IsSameVal(THashObj *obj, const void *val) {
		return obj->hashId == *(u_int *)val;
	}

public:
	TResHash(int _hashNum) : THashTbl(_hashNum) {}
	TResHashObj	*Search(UINT resId) {
		return (TResHashObj *)THashTbl::Search(&resId, resId);
	}
	void	Register(TResHashObj *obj) {
		THashTbl::Register(obj, obj->hashId);
	}
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
	size_t	size;
	size_t	usedSize;
	size_t	maxSize;
	BOOL	dumpExcept;
	void	Init();

public:
	VBuf(size_t _size=0, size_t _max_size=0);
	~VBuf();
	BOOL	AllocBuf(size_t _size, size_t _max_size=0);
	BOOL	LockBuf();
	void	FreeBuf();
	BOOL	Grow(size_t grow_size);
	operator bool() { return buf ? true : false; }
	operator void *() { return buf; }
	operator BYTE *() { return buf; }
	operator char *() { return (char *)buf; }
	BYTE	*Buf() { return	buf; }
	WCHAR	*WBuf() { return (WCHAR *)buf; }
	size_t	Size() const { return size; }
	size_t	MaxSize() const { return maxSize; }
	size_t	UsedSize() const { return usedSize; }
	BYTE	*UsedEnd() { return	buf + usedSize; }
	void	SetUsedSize(size_t _used_size) { usedSize = _used_size; }
	size_t	AddUsedSize(size_t _used_size) { return usedSize += _used_size; }
	size_t	RemainSize(void) const { return size - usedSize; }
	VBuf& operator =(const VBuf&); // prohibit
	void	EnableDumpExcept(BOOL on=TRUE) { dumpExcept = on; }
};

template <class T>
class VBVec : public VBuf {
	size_t	growSize;
	size_t	usedNum;

public:
	VBVec() {
		growSize = 0;
		usedNum = 0;
	}
	BOOL Init(size_t min_num, size_t max_num, size_t grow_num=0) {
		size_t min_size = ALIGN_SIZE(min_num * sizeof(T), PAGE_SIZE);
		size_t max_size = ALIGN_SIZE(max_num * sizeof(T), PAGE_SIZE);
		growSize = grow_num ? ALIGN_SIZE(grow_num * sizeof(T), PAGE_SIZE) : min_size;
		if (growSize == 0) {
			growSize = ALIGN_SIZE(sizeof(T), PAGE_SIZE);
		}
		usedNum  = 0;
		return AllocBuf(min_size, max_size);
	}
	T& operator [](size_t idx) {
		Aquire(idx);
		return	Get(idx);
	}
	const T& operator [](size_t idx) const {
		return	Get(idx);
	}
	T& operator [](int idx) {
		Aquire(idx);
		return	Get(idx);
	}
	const T& operator [](int idx) const {
		return	Get(idx);
	}
	VBVec<T>& operator =(const VBVec<T> &);	// default definition is none
	bool Aquire(size_t idx) {
		size_t	need_num = idx + 1;
		if (need_num <= usedNum) {
			return true;
		}

		size_t	need_size = need_num * sizeof(T);
		if (need_size > size) {
			if (need_size > maxSize) {
				return false;
			}
			if (need_size > size && !Grow(growSize)) {
				return false;
			}
		}
		usedSize = need_size;
		usedNum  = need_num;
		return	true;
	}
	bool Set(size_t idx, const T& d) {
		if (!Aquire(idx)) {
			return false;
		}
		Get(idx) = d;
		return	true;
	}
	T& Get(size_t idx) {
		return *(T *)(buf + (sizeof(T) * idx));
	}
	const T& Get(size_t idx) const {
		return *(T *)(buf + (sizeof(T) * idx));
	}
	size_t UsedNum() const {
		return usedNum;
	}
	int	UsedNumInt() const {
		return (int)usedNum;
	}
	size_t SetUsedNum(size_t num) {
		usedNum  = num;
		usedSize = num * sizeof(T);
		return UsedNum();
	}
	bool Push(const T& d) {
		return	Set(UsedNum(), d); // usedNum will be increment in Aquire
	}
	bool Pop() {
		if (usedNum <= 0) {
			return false;
		}
		usedSize -= sizeof(T);
		usedNum--;
		return	true;
	}
	T& Top()  { return Get(0); }
	T& Last() { return Get(UsedNum()-1); }
	const T& Top() const { return Get(0); }
	const T& Last() const { return Get(UsedNum()-1); }
};

class GBuf {
protected:
	HGLOBAL	hGlobal = NULL;
	BYTE	*buf = NULL;
	size_t	size = 0;
	UINT	flags = 0;

public:
	GBuf(size_t _size=0, BOOL with_lock=TRUE, UINT _flags=GMEM_MOVEABLE) {
		Init(_size, with_lock, _flags);
	}
	GBuf(VBuf *vbuf, BOOL with_lock=TRUE, UINT _flags=GMEM_MOVEABLE) {
		Init(vbuf->Size(), with_lock, _flags);
		if (buf) {
			memcpy(buf, vbuf->Buf(), vbuf->Size());
		}
	}
	~GBuf() {
		UnInit();
	}
	BOOL Init(size_t _size=0, BOOL with_lock=TRUE, UINT _flags=GMEM_MOVEABLE) {
		UnInit();
		flags	= _flags;

		if ((size = _size) == 0) {
			return TRUE;
		}
		if (!(hGlobal = ::GlobalAlloc(flags, size))) {
			return FALSE;
		}
		if (!with_lock) {
			return	TRUE;
		}
		return	Lock() ? TRUE : FALSE;
	}
	void UnInit() {
		if (buf && (flags & GMEM_FIXED)) {
			::GlobalUnlock(buf);
		}
		if (hGlobal) {
			::GlobalFree(hGlobal);
		}
		buf		= NULL;
		hGlobal	= NULL;
		size	= 0;
	}
	HGLOBAL	Handle() {
		return hGlobal;
	}
	BYTE *Buf() {
		return	buf;
	}
	BYTE *Lock() {
		if ((flags & GMEM_FIXED)) {
			buf = (BYTE *)hGlobal;
		}
		else {
			buf = (BYTE *)::GlobalLock(hGlobal);
		}
		return	buf;
	}
	void Unlock() {
		if (!(flags & GMEM_FIXED)) {
			::GlobalUnlock(buf);
			buf = NULL;
		}
		
	}
	size_t	Size() const { return size; }
	const GBuf& operator =(const GBuf &) = delete;
};

//#define DYNBUF_DBG

class DynBuf {
protected:
	char	*buf = NULL;
	size_t	size = 0;
	size_t	usedSize = 0;
#ifdef DYNBUF_DBG
	VBuf	vbuf;
#endif

public:
	DynBuf(size_t _size=0, void *init_buf=NULL) {
		if (_size > 0) {
			if (init_buf) {
				Init(init_buf, _size);
			} else {
				Alloc(_size);
			}
		}
	}
	DynBuf(const DynBuf &org) {
		*this = org;
	}
	~DynBuf() {
#ifndef DYNBUF_DBG
		free(buf);
#endif
	}
	char *Alloc(size_t _size) {
#ifdef DYNBUF_DBG
		buf = NULL;
		size = 0;
		usedSize = 0;
		vbuf.FreeBuf();
		if (_size > 0) {
			vbuf.AllocBuf(ALIGN_SIZE(_size, PAGE_SIZE));
			size_t mod = _size % PAGE_SIZE;
			size_t diff = mod ? (PAGE_SIZE - mod) : 0;
			buf = (char *)vbuf + diff;
			size = _size;
		}
		return	buf;
#else
		free(buf);
		buf = NULL;
		size = 0;
		usedSize = 0;
		if (_size <= 0) {
			return NULL;
		}
		if ((buf = (char *)malloc(_size))) {
			size = _size;
			memset(buf, 0, min(sizeof(WCHAR), size));
		}
#endif
		return	buf;
	}
	void Init(void *init_buf, size_t _size) {
		Alloc(_size);
		if (init_buf) {
			memcpy(buf, init_buf, _size);
			usedSize = _size;
		}
	}
	void Free() 		{ Alloc(0); }
	BYTE *Buf()			{ return (BYTE *)buf; }
	const char *s() const { return (buf && size) ? (const char *)buf : ""; }
	char *S() { return (char *)buf; }
	operator char*()	{ return (char *)buf; }
	operator const char*() const { return (const char *)buf; }
	operator BYTE*()	{ return (BYTE *)buf; }
	operator const BYTE*() const { return (const BYTE *)buf; }
	operator WCHAR*()	{ return (WCHAR *)buf; }
	operator const WCHAR*() const { return (const WCHAR *)buf; }
	operator void*()	{ return (void *)buf; }
	operator bool() { return (buf && size) ? true : false; }
	size_t Size() const		{ return size; }
	size_t WSize() const	{ return size / sizeof(WCHAR); }
	size_t UsedSize() const	{ return usedSize; }
	size_t SetUsedSize(size_t _usedSize) { return usedSize = _usedSize; }
	size_t	RemainSize(void) const { return size - usedSize; }
	size_t	AddUsedSize(size_t _used_size) { return usedSize += _used_size; }

	size_t SetByStr(const char *s, int len=-1) {
		if (!s) s = "";
		if (len == -1) {
			len = (int)strlen(s);
		}
		Alloc(len + 1);
		memcpy(Buf(), s, len);
		Buf()[len] = 0;
		SetUsedSize(len);
		return	len;
	}

	DynBuf& operator =(const DynBuf &d) {
		if (Alloc(d.size) && d.size > 0) {
			memcpy(buf, d.buf, d.size);
		}
		usedSize = d.usedSize;
		return	*this;
	}
	bool operator ==(const DynBuf &d) const {
		if (usedSize != d.usedSize) return false;
		if (usedSize == 0) return true;
		return	memcmp(buf, d.buf, usedSize) == 0;
	}
	bool operator !=(const DynBuf &d) const {
		return	!(*this == d);
	}
};

void InitInstanceForLoadStr(HINSTANCE hI);

LPSTR LoadStrA(UINT resId, HINSTANCE hI=NULL);
LPSTR LoadStrU8(UINT resId, HINSTANCE hI=NULL);
LPWSTR LoadStrW(UINT resId, HINSTANCE hI=NULL);

void TSetDefaultLCID(LCID id=0);
LCID TGetDefaultLCID();
HMODULE TLoadLibrary(LPSTR dllname);
HMODULE TLoadLibraryW(WCHAR *dllname);

inline int get_ntz64(uint64 val) {
#ifdef _WIN64
	u_long	ret = 0;
	_BitScanForward64(&ret, val);
	return	ret;
#else
	u_long	ret = 0;
	if (_BitScanForward(&ret, (u_int)val)) {
		return ret;
	}
	else {
		ret = 0;
	}

	u_int sval = (u_int)(val >> 32);
	if (_BitScanForward(&ret, sval)) {
		ret += 32;
	}
	else {
		ret = 0;
	}
	return	ret;
#endif
}

inline int get_ntz(u_int val) {
	u_long	ret = 0;
	if (!_BitScanForward(&ret, val)) {
		ret = 0;
	}
	return	ret;
}

inline int get_nlz64(uint64 val) {
#ifdef _WIN64
	u_long	ret = 0;
	if (!_BitScanReverse64(&ret, val)) {
		ret = 0;
	}
	return	ret;
#else
	u_long	ret = 0;
	u_int	sval = (u_int)(val >> 32);
	if (_BitScanReverse(&ret, sval)) {
		ret += 32;
		return ret;
	} else {
		ret = 0;
	}

	u_long sv_ret = ret;
	if (!_BitScanReverse(&ret, (u_int)val)) {
		ret = sv_ret;
	}

	return	ret;
#endif
}

inline int get_nlz(u_int val) {
	u_long	ret = 0;
	_BitScanReverse(&ret, val);
	return	ret;
}

inline int len_to_hexlen(int64 len) {
	if (len == 0) return 1;

	int	bits = get_nlz64(len);
	int	bytes = bits / 4 + 1;

	return	bytes;
}

inline size_t b64_size(size_t len) {
	return ((len + 2) / 3) * 4;
}

inline size_t b64_to_len(size_t len) {
	return (len / 4) * 3;
}

DWORD64 GetTick64();
DWORD GetTick();

class TTick {
	DWORD	tick;
public:
	TTick() { start(); }
	DWORD start() { return (tick = ::GetTick()); }
	DWORD elaps(BOOL overwrite=TRUE) {
		DWORD	cur = ::GetTick();
		DWORD	diff = cur - tick;
		if (overwrite) {
			tick = cur;
		}
		return	diff;
	}
};

/* UNIX - Windows 文字コード変換 */
template<class T> int LocalNewLineToUnixT(const T *src, T *dst, int max_dstlen, int len=-1) {
	T		*sv_dst  = dst;
	T		*max_dst = dst + max_dstlen - 1;
	const T	*max_src = src + ((len >= 0) ? len : (max_dstlen * 2));

	if (!src || !dst || max_dstlen <= 0) return 0;

	while (src < max_src && *src && dst < max_dst) {
		if ((*dst = *src++) != '\r') {
			dst++;
		}
	}
	*dst = 0;

	return	int(dst - sv_dst);
}

template<class T> int UnixNewLineToLocalT(const T *src, T *dst, int max_dstlen, int len=-1) {
	T		*sv_dst  = dst;
	T		*max_dst = dst + max_dstlen - 1;
	const T	*max_src = src + ((len >= 0) ? len : max_dstlen);

	if (!src || !dst || max_dstlen <= 0) return 0;

	while (src < max_src && *src && dst < max_dst) {
		if ((*dst = *src++) == '\n' && dst + 1 < max_dst) {
			*dst++ = '\r';
			*dst++ = '\n';
		}
		else dst++;
	}
	*dst = 0;

	return	int(dst - sv_dst);
}

#define LocalNewLineToUnix  LocalNewLineToUnixT<char>
#define UnixNewLineToLocal  UnixNewLineToLocalT<char>
#define LocalNewLineToUnixW LocalNewLineToUnixT<WCHAR>
#define UnixNewLineToLocalW UnixNewLineToLocalT<WCHAR>

//int LocalNewLineToUnix(const char *src, char *dest, int max_dstlen);
//int UnixNewLineToLocal(const char *src, char *dest, int max_dstlen);

BOOL TIsWow64();
BOOL TOs64();
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
BOOL TGetTextWidth(HDC hDc, const WCHAR *s, int len, int width, int *rlen, int *rcx);
HBITMAP TDIBtoDDB(HBITMAP hDibBmp); // 8bit には非対応
BOOL TOpenExplorerSelOneW(const WCHAR *path);
BOOL TOpenExplorerSelW(const WCHAR *dir, WCHAR **path, int num);

BOOL TSetDefaultDllDirectories(DWORD flags);

const WCHAR *TGetSysDirW(WCHAR *buf=NULL);
const WCHAR *TGetExeDirW(WCHAR *buf=NULL);

enum TLoadType { TLT_SYSDIR, TLT_EXEDIR };
HMODULE TLoadLibraryExW(const WCHAR *dll, TLoadType t);

BOOL TGetIntegrityLevel(DWORD *ilv);
BOOL TIsLowIntegrity();

class TWin;
class OpenFileDlg {
public:
	enum			Mode { OPEN, MULTI_OPEN, SAVE, NODEREF_SAVE };

protected:
	TWin			*parent;
	LPOFNHOOKPROC	hook;
	Mode			mode;
	DWORD			flags;

public:
	OpenFileDlg(TWin *_parent, Mode _mode=OPEN, LPOFNHOOKPROC _hook=NULL, DWORD _flags=0) {
		parent = _parent; hook = _hook; mode = _mode; flags = _flags;
	}
	BOOL Exec(char *target, int size, char *title=NULL, char *filter=NULL, char *defaultDir=NULL,
				char *defaultExt=NULL);
	BOOL Exec(UINT editCtl, char *title=NULL, char *filter=NULL, char *defaultDir=NULL,
				char *defaultExt=NULL);
};

BOOL ShellLinkW(const WCHAR *target, const WCHAR *link, const WCHAR *arg=NULL,
	const WCHAR *desc=NULL, const WCHAR *app_id=NULL);
BOOL ShellLinkU8(const char *target, const char *link, const char *arg=NULL,
	const char *desc=NULL, const char *app_id=NULL);
BOOL ReadLinkW(const WCHAR *link, WCHAR *target, WCHAR *arg=NULL, WCHAR *desc=NULL);
BOOL ReadLinkU8(const char *link, char *target, char *arg=NULL, char *desc=NULL);
HRESULT UpdateLinkW(const WCHAR *link, const WCHAR *arg=NULL, const WCHAR *desc=NULL,
	DWORD flags=SLR_NO_UI|SLR_UPDATE|SLR_NOSEARCH, HWND hWnd=0);
HRESULT UpdateLinkU8(const char *link, const char *arg=NULL, const char *desc=NULL,
	DWORD flags=SLR_NO_UI|SLR_UPDATE|SLR_NOSEARCH, HWND hWnd=0);
BOOL DeleteLinkW(const WCHAR *link);
BOOL DeleteLinkU8(const char *link);
BOOL GetParentDirW(const WCHAR *srcfile, WCHAR *dir);
BOOL GetParentDirU8(const char *srcfile, char *dir);

BOOL MakeDirU8(char *dir);
BOOL MakeDirW(WCHAR *dir);
BOOL MakeDirAllU8(char *dir);
BOOL MakeDirAllW(WCHAR *dir);
HANDLE CreateFileWithDirU8(const char *path, DWORD flg, DWORD share, SECURITY_ATTRIBUTES *sa,
	DWORD create_flg, DWORD attr, HANDLE hTmpl);
HANDLE CreateFileWithDirW(const WCHAR *path, DWORD flg, DWORD share, SECURITY_ATTRIBUTES *sa,
	DWORD create_flg, DWORD attr, HANDLE hTmpl);

HWND ShowHelpW(HWND hOwner, const WCHAR *help_dir, const WCHAR *help_file, const WCHAR *section=0);
HWND ShowHelpU8(HWND hOwner, const char *help_dir, const char *help_file, const char *section=NULL);void UnInitShowHelp();

HWND TransMsgHelp(MSG *msg);
HWND CloseHelpAll();

BOOL TSetClipBoardTextW(HWND hWnd, const WCHAR *s, int len=-1);

HBITMAP TIconToBmp(HICON hIcon, int cx, int cy);

enum TrayMode { TIS_NONE, TIS_HIDE, TIS_SHOW, TIS_ALL };
BOOL ForceSetTrayIcon(HWND hWnd, UINT id, DWORD pref=2);
TrayMode GetTrayIconState();

BOOL SetWinAppId(HWND hWnd, const WCHAR *app_id);
BOOL IsWineEnvironment();

// ADS/domain
BOOL IsDomainEnviron();
BOOL GetDomainAndUid(WCHAR *domain, WCHAR *uid);
BOOL GetDomainFullName(const WCHAR *domain, const WCHAR *uid, WCHAR *full_name);
BOOL GetDomainGroup(const WCHAR *domain, const WCHAR *uid, WCHAR *group);

BOOL IsInstalledFont(HDC hDc, const WCHAR *face_name, BYTE charset=DEFAULT_CHARSET);

// Firewall
BOOL Is3rdPartyFwEnabled(BOOL use_except_list=TRUE, DWORD timeout=5000, BOOL *is_timeout=NULL);
int Get3rdPartyFwNum();
BOOL Get3rdPartyFwName(int idx, WCHAR *buf, int max_len);

BOOL IsLockedScreen();

struct FwStatus {
	BOOL	fwEnable;
	DWORD	entryCnt;
	DWORD	enableCnt;
	DWORD	disableCnt;
	DWORD	allowCnt;
	DWORD	blockCnt;
	DWORD	profTypes;	// NET_FW_PROFILE_TYPE2 bits

	FwStatus() {
		Init();
	}
	void Init() {
		fwEnable = FALSE;
		entryCnt = 0;
		enableCnt = 0;
		disableCnt = 0;
		allowCnt = 0;
		blockCnt = 0;
		profTypes = 0;
	}
	BOOL IsAllowed() {
		return	(blockCnt == 0 && allowCnt > 0) ? TRUE : FALSE;
	}
	BOOL IsBlocked() {
		return	(blockCnt > 0) ? TRUE : FALSE;
	}
};

BOOL GetFwStatus(const WCHAR *path, FwStatus *fs);
BOOL GetFwStatusEx(const WCHAR *path, FwStatus *fs);
BOOL SetFwStatus(const WCHAR *path=NULL, const WCHAR *label=NULL, BOOL enable=TRUE);
BOOL SetFwStatusEx(const WCHAR *path=NULL, const WCHAR *label=NULL, BOOL enable=TRUE);
BOOL DelFwStatus(const WCHAR *path=NULL);
CRITICAL_SECTION *TLibCs();

BOOL TIsAdminGroup();

// 1601年1月1日から1970年1月1日までの通算100ナノ秒
#define UNIXTIME_BASE	((int64)0x019db1ded53e8000)

inline time_t FileTime2UnixTime(FILETIME *ft) {
	return	(time_t)((*(int64 *)ft - UNIXTIME_BASE) / 10000000);
}
inline void UnixTime2FileTime(time_t ut, FILETIME *ft) {
	*(int64 *)ft = (int64)ut * 10000000 + UNIXTIME_BASE;
}
void time_to_SYSTEMTIME(time_t t, SYSTEMTIME *st, BOOL is_local=TRUE);
time_t SYSTEMTIME_to_time(const SYSTEMTIME &st, BOOL is_local=TRUE);

void U8Out(const char *fmt,...);

BOOL TGetUrlAssocAppW(const WCHAR *scheme, WCHAR *wbuf, int max_len);
time_t TGetBuildTimestamp();

#define BIT_SET(flg, targ, val) (flg ? (targ |= val) : (targ &= ~val))

#endif

