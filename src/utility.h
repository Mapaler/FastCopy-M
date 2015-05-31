/* static char *utility_id = 
	"@(#)Copyright (C) 2004-2012 H.Shirouzu		utility.h	Ver2.10"; */
/* ========================================================================
	Project  Name			: Utility
	Create					: 2004-09-15(Wed)
	Update					: 2012-06-17(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef UTILITY_H
#define UTILITY_H

#include "tlib/tlib.h"

class Logging {
protected:
	char	*buf;
public:
	Logging();
};

class PathArray : public THashTbl {
protected:
	struct PathObj : THashObj {
		void	*path;
		int		len;
		PathObj(const void *_path, int len=-1) { Set(_path, len); }
		~PathObj() { if (path) free(path); }
		BOOL Set(const void *_path, int len=-1);
	};
	int		num;
	PathObj	**pathArray;
	DWORD	flags;
	BOOL	SetPath(int idx, const void *path, int len=-1);

	virtual BOOL	IsSameVal(THashObj *obj, const void *val) {
		return lstrcmpiV(((PathObj *)obj)->path, val) == 0;
	}

public:
	enum { ALLOW_SAME=1, NO_REMOVE_QUOTE=2 };
	PathArray(void);
	PathArray(const PathArray &);
	~PathArray();
	void	Init(void);
	void	SetMode(DWORD _flags) { flags = _flags; }
	int		RegisterMultiPath(const void *multi_path, const void *separator=SEMICOLON_V);
	int		GetMultiPath(void *multi_path, int max_len, const void *separator=SEMICLN_SPC_V,
				const void *escape_char=SEMICOLON_V);
	int		GetMultiPathLen(const void *separator=SEMICLN_SPC_V,
				const void *escape_char=SEMICOLON_V);

	PathArray& operator=(const PathArray& init);

	void	*Path(int idx) const { return idx < num ? pathArray[idx]->path : NULL; }
	int		Num(void) const { return	num; }
	BOOL	RegisterPath(const void *path);
	BOOL	ReplacePath(int idx, void *new_path);

	u_int	MakeHashId(const void *data, int len=-1) {
		return MakeHash(data, (len >= 0 ? len : strlenV(data)) * CHAR_LEN_V);
	}
	u_int	MakeHashId(const PathObj *obj) { return MakeHash(obj->path, obj->len * CHAR_LEN_V); }
};

#define MAX_DRIVE_LETTER	26

class DriveMng {
protected:
	struct DriveID {
		BYTE	*data;
		int		len;
	} drvID[MAX_DRIVE_LETTER];	// A-Z drive
	int		noIdCnt;	// for Win95 family
	char	driveMap[64];

	BOOL	RegisterDriveID(int index, void *data, int len);
	BOOL	SetDriveID(int drvLetter);
	int		LetterToIndex(int drvLetter) { return toupper(drvLetter) - 'A'; }

public:
	DriveMng();
	~DriveMng();
	void	Init();
	BOOL	IsSameDrive(int drvLetter1, int drvLetter2);
	void	SetDriveMap(char *map);
};

class DataList {
public:
	struct Head {
		Head	*prior;
		Head	*next;
		int		alloc_size;
		int		data_size;
		BYTE	data[1];	// opaque
	};

protected:
	VBuf		buf;
	Head		*top;
	Head		*end;
	int			num;
	int			grow_size;
	int			min_margin;
	Condition	cv;

public:
	DataList(int size=0, int max_size=0, int _grow_size=0, VBuf *_borrowBuf=NULL, int _min_margin=65536);
	~DataList();

	BOOL Init(int size, int max_size, int _grow_size, VBuf *_borrowBuf=NULL, int _min_margin=65536);
	void UnInit();

	void Lock() { cv.Lock(); }
	void UnLock() { cv.UnLock(); }
	BOOL Wait(DWORD timeout=INFINITE) { return cv.Wait(timeout); }
	BOOL IsWait() { return cv.WaitThreads() ? TRUE : FALSE; }
	void Notify() { cv.Notify(); }

	Head *Alloc(void *data, int copy_size, int need_size);
	Head *Get();
	Head *Fetch(Head *prior=NULL);
	void Clear();
	int Num() { return num; }
	int RemainSize();
	int MaxSize() { return buf.MaxSize(); }
	int Size() { return buf.Size(); }
	int Grow(int grow_size) { return buf.Grow(grow_size); }
	int MinMargin() { return min_margin; }
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

void *strtok_pathV(void *str, const void *sep, void **p, BOOL remove_quote=TRUE);
void **CommandLineToArgvV(void *cmdLine, int *_argc);
int CALLBACK EditWordBreakProc(LPTSTR str, int cur, int len, int action);
BOOL GetRootDirV(const void *path, void *root_dir);
BOOL NetPlaceConvertV(void *src, void *dst);

DWORD ReadReparsePoint(HANDLE hFile, void *buf, DWORD size);
BOOL WriteReparsePoint(HANDLE hFile, void *buf, DWORD size);
BOOL DeleteReparsePoint(HANDLE hFile, void *buf);
BOOL IsReparseDataSame(void *d1, void *d2);

void DBGWrite(char *fmt,...);
void DBGWriteW(WCHAR *fmt,...);

#endif

