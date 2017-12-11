/* static char *utility_id = 
	"@(#)Copyright (C) 2004-2017 H.Shirouzu		utility.h	Ver3.30"; */
/* ========================================================================
	Project  Name			: Utility
	Create					: 2004-09-15(Wed)
	Update					: 2017-03-06(Mon)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	======================================================================== */

#ifndef UTILITY_H
#define UTILITY_H

#include "tlib/tlib.h"
#include "shareinfo.h"

#define CRLF			L"\r\n"
#define SEMICLN			L";"
#define SEMICLN_SPC		L"; "
#define NULW			L""

class Logging {
protected:
	char	*buf;
public:
	Logging();
};

class PathArray : public THashTbl {
protected:
	struct PathObj : THashObj {
		WCHAR	*path;
		int		len;
		PathObj(const WCHAR *_path, int len=-1) { Set(_path, len); }
		~PathObj() { if (path) free(path); }
		BOOL Set(const WCHAR *_path, int len=-1);
	};
	int		num;
	PathObj	**pathArray;
	DWORD	flags;
	BOOL	SetPath(int idx, const WCHAR *path, int len=-1);

	virtual BOOL IsSameVal(THashObj *obj, const void *val) {
		return wcsicmp(((PathObj *)obj)->path, (WCHAR *)val) == 0;
	}

public:
	enum { ALLOW_SAME=1, NO_REMOVE_QUOTE=2 };
	PathArray(void);
	PathArray(const PathArray &);
	virtual ~PathArray();
	void	Init(void);
	void	SetMode(DWORD _flags) { flags = _flags; }
	int		RegisterMultiPath(const WCHAR *multi_path, const WCHAR *separator=SEMICLN);
	int		GetMultiPath(WCHAR *multi_path, int max_len, const WCHAR *separator=SEMICLN_SPC,
				const WCHAR *escape_chars=SEMICLN_SPC, BOOL with_endsep=FALSE);
	int		GetMultiPathLen(const WCHAR *separator=SEMICLN_SPC,
				const WCHAR *escape_chars=SEMICLN_SPC, BOOL with_endsep=FALSE);

	PathArray& operator=(const PathArray& init);

	WCHAR	*Path(int idx) const { return idx < num ? pathArray[idx]->path : NULL; }
	int		PathLen(int idx) const { return idx < num ? pathArray[idx]->len : 0; }
	int		Num(void) const { return num; }
	BOOL	RegisterPath(const WCHAR *path);
	BOOL	ReplacePath(int idx, WCHAR *new_path);

	u_int	MakeHashId(const void *data, int len=-1) {
		return MakeHash(data, (len >= 0 ? len : wcslen((WCHAR *)data)) * sizeof(WCHAR));
	}
	u_int	MakeHashId(const PathObj *obj) { return MakeHash(obj->path, obj->len * sizeof(WCHAR));}
};

#define MAX_DRIVES			(64)	// A-Z + UNC_drives
#define MAX_LOCAL_DRIVES	(26)	// A-Z
inline int DrvLetterToIndex(int drvLetter) { return toupper(drvLetter) - 'A'; }

class ShareInfo;

class DriveMng {
public:
	// ネットワークドライブでの同一物理ドライブと見做す判定
	//  NET_UNC_FULLVAL: UNCボリューム名全体で判断
	//  NET_UNC_SVRONLY: UNCサーバ名部分で判断
	//  NET_UNC_COMMON:  サーバを問わず、ネットワーク経由は同じと見做す
	enum NetDrvMode { NET_UNC_FULLVAL=0, NET_UNC_SVRONLY=1, NET_UNC_COMMON=2 };

protected:
	ShareInfo	*shareInfo;
	NetDrvMode	netDrvMode;

	struct DriveID {
		BYTE	*data;
		int		len;
		uint64	sameDrives;
	} drvID[MAX_DRIVES];	// A-Z + UNC drives
	char		driveMap[64];

	BOOL	RegisterDriveID(int index, void *data, int len);
	void	ModifyNetRoot(WCHAR *root);

public:
	DriveMng();
	~DriveMng();
	void	SetShareInfo(ShareInfo *_shareInfo) { shareInfo = _shareInfo; }
	void	Init(NetDrvMode mode=NET_UNC_FULLVAL);
	int		SetDriveID(const WCHAR *root);
	BOOL	IsSameDrive(const WCHAR *root1, const WCHAR *root2);
	void	SetDriveMap(char *map);
	uint64	OccupancyDrives(uint64 use_drives);
};

class DataList {
public:
	struct Head {
		Head	*prev;
		Head	*next;
		ssize_t	alloc_size;
		ssize_t	data_size;
		BYTE	data[1];	// opaque
	};

protected:
	VBuf		buf;
	Head		*top;
	Head		*end;
	ssize_t		num;
	ssize_t		grow_size;
	ssize_t		min_margin;
	Condition	cv;

public:
	DataList(ssize_t size=0, ssize_t max_size=0, ssize_t _grow_size=0, VBuf *_borrowBuf=NULL,
		ssize_t _min_margin=65536);
	~DataList();

	BOOL Init(ssize_t size, ssize_t max_size, ssize_t _grow_size, VBuf *_borrowBuf=NULL,
		ssize_t _min_margin=65536);
	void UnInit();

	void Lock() { cv.Lock(); }
	void UnLock() { cv.UnLock(); }
	BOOL Wait(DWORD timeout=INFINITE) { return cv.Wait(timeout); }
	BOOL IsWait() { return cv.IsWait(); }
	void Notify() { cv.Notify(); }

	Head *Alloc(void *data, ssize_t copy_size, ssize_t need_size);
	Head *Get();
	Head *Peek(Head *prev=NULL);
	void Clear();
	ssize_t Num() { return num; }
	ssize_t RemainSize();
	ssize_t MaxSize() { return	buf.MaxSize(); }
	ssize_t Size() { return buf.Size(); }
	ssize_t Grow(ssize_t grow_size) { return buf.Grow(grow_size); }
	ssize_t MinMargin() { return min_margin; }
};


// WinNT
#define MOUNTED_DEVICES		"SYSTEM\\MountedDevices"
#define FMT_DOSDEVICES		"\\DosDevices\\%c:"
// Win95
#define ENUM_DEVICES		"Enum"
#define DRIVE_LETTERS		"CurrentDriveLetterAssignment"
#define CONFIG_ENUM			"Config Manager\\Enum"
#define HARDWARE_KEY		"HardWareKey"
#ifndef HKEY_DYN_DATA
#define HKEY_DYN_DATA		((HKEY)0x80000006)
#endif

WCHAR *strtok_pathW(WCHAR *str, const WCHAR *sep, WCHAR **p, BOOL remove_quote=TRUE);
WCHAR **CommandLineToArgvExW(WCHAR *cmdLine, int *_argc);
int CALLBACK EditWordBreakProcW(WCHAR *str, int cur, int len, int action);
BOOL GetRootDirW(const WCHAR *path, WCHAR *root_dir);
BOOL NetPlaceConvertW(WCHAR *src, WCHAR *dst);

DWORD ReadReparsePoint(HANDLE hFile, void *buf, DWORD size);
BOOL WriteReparsePoint(HANDLE hFile, void *buf, DWORD size);
BOOL DeleteReparsePoint(HANDLE hFile, void *buf);
BOOL IsReparseDataSame(void *d1, void *d2);
BOOL ResetAcl(const WCHAR *path, BOOL myself_acl=FALSE);

enum { FMF_NONE=0, FMF_ACL=1, FMF_MYACL=2, FMF_ATTR=0x100 }; // flags
BOOL ForceRemoveDirectoryW(const WCHAR *path, DWORD flags=FMF_NONE);
BOOL ForceDeleteFileW(const WCHAR *path, DWORD flags=FMF_NONE);
HANDLE ForceCreateFileW(const WCHAR *path, DWORD mode, DWORD share, SECURITY_ATTRIBUTES *sa,
	DWORD cr_mode, DWORD cr_flg, HANDLE hTempl, DWORD flags=FMF_NONE);

void DBGWrite(char *fmt,...);
void DBGWriteW(WCHAR *fmt,...);

ssize_t comma_int64(WCHAR *s, int64);
ssize_t comma_double(WCHAR *s, double, int precision);
ssize_t comma_int64(char *s, int64);
ssize_t comma_double(char *s, double, int precision);

//#define TRACE_DBG
#ifdef TRACE_DBG
void trclog_init();
void trclog(const WCHAR *func, int lines);
WCHAR *trclog_get(DWORD idx);
struct Trl {
	const WCHAR *func;
	Trl(const WCHAR *_func, int line) {
		func = _func;
		trclog(func, line);
	}
	~Trl() {
		trclog(func, 0);
	}
};
#else
#define Trl(...)
#endif

#endif

