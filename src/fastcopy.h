/* static char *fastcopy_id = 
	"@(#)Copyright (C) 2004-2019 H.Shirouzu		fastcopy.h	Ver3.62"; */
/* ========================================================================
	Project  Name			: Fast Copy file and directory
	Create					: 2004-09-15(Wed)
	Update					: 2019-01-28(Mon)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	Modify					: Mapaler 2015-09-09
	======================================================================== */

#include "tlib/tlib.h"
#include "resource.h"
#include "utility.h"
#include "regexp.h"
#include "shareinfo.h"
#include <vector>

#ifdef WIN32
#pragma warning (disable: 4018)
#endif

#define MIN_SECTOR_SIZE		(512)
#define BIG_SECTOR_SIZE		(4096)
#define MAX_BUF				(1024 * 1024 * 1024)
#define MINREMAIN_BUF		(1024 * 1024)
#define MIN_BUF				(256 * 1024)
#define WAITMIN_BUF			(256 * 1024)
#define WAITMID_BUF			(1024 * 1024)
#define BIGTRANS_ALIGN		(256 * 1024)
#define APP_MEMSIZE			(6 * 1024 * 1024)
//#define NETDIRECT_SIZE	(64 * 1024 * 1024)
#define PATH_LOCAL_PREFIX	L"\\\\?\\"
#define PATH_UNC_PREFIX		L"\\\\?\\UNC"
#define PATH_LOCAL_PREFIX_LEN	4
#define PATH_UNC_PREFIX_LEN		7

#define BUFIO_SIZERATIO		2

#define IsDir(attr) ((attr) & FILE_ATTRIBUTE_DIRECTORY)
#define IsReparse(attr) ((attr) & FILE_ATTRIBUTE_REPARSE_POINT)
#define IsNoReparseDir(attr) (IsDir(attr) && !IsReparse(attr))
#define Attr(d) ((d)->dwFileAttributes)
#define IsSymlink(d) (IsReparse((d)->dwFileAttributes) && (d)->dwReserved0==IO_REPARSE_TAG_SYMLINK)

#define FASTCOPY			"FastCopy-M"
#define FASTCOPYW			L"FastCopy-M"

#define NTFS_STR			L"NTFS"
#define REFS_STR			L"REFS"
#define FMT_RENAME			L"%.*s(%d)%s"
#define FMT_PUTLIST			L"%c %s%s%s%s\r\n"
#define FMT_REDUCEMSG		L"Reduce MaxIO(%c) size (%dMB -> %dMB)"

#define PLSTR_LINK			L" =>"
#define PLSTR_REPARSE		L" ->"
#define PLSTR_REPDIR		L"\\ ->"
#define PLSTR_CASECHANGE	L" (CaseChanged)"
#define PLSTR_COMPARE		L" !!"

#define MAX_BASE_BUF		( 512 * 1024 * 1024)
#define MIN_BASE_BUF		(  64 * 1024 * 1024)
#define RESERVE_BUF			( 128 * 1024 * 1024)

#define MIN_ATTR_BUF		(256 * 1024)
//#define MAX_ATTR_BUF		(128 * 1024 * 1024)
#define MIN_ATTRIDX_BUF		ALIGN_SIZE((MIN_ATTR_BUF / 4), PAGE_SIZE)
#define MAX_ATTRIDX_BUF(x)	ALIGN_SIZE(((x) / 32), PAGE_SIZE)
#define MIN_MKDIRQUEVEC_NUM	((8 * 1024) / 16)
#define MAX_MKDIRQUEVEC_NUM	((256 * 1024) / 16)
#define MIN_DSTDIREXT_BUF	(64 * 1024)
#define MAX_DSTDIREXT_BUF	(2 * 1024 * 1024)
#define MIN_ERR_BUF			(64 * 1024)
#define MAX_ERR_BUF			(4 * 1024 * 1024)
#define MAX_LIST_BUF		(128 * 1024)
#define MIN_PUTLIST_BUF		(1 * 1024 * 1024)
#define MAX_PUTLIST_BUF		(4 * 1024 * 1024)
#define MIN_DEPTH_NUM		(PAGE_SIZE / sizeof(int))
#define MAX_DEPTH_NUM		(16 * 1024)

#define MIN_DIGEST_LIST		(1 * 1024 * 1024)
#define MIN_MOVEPATH_LIST	(1 * 1024 * 1024)

#define MAX_NTQUERY_BUF		(512 * 1024)

#define CV_WAIT_TICK		1000
#define FASTCOPY_ERROR_EVENT	0x01
#define FASTCOPY_STOP_EVENT		0x02

#define KB (1024)
#define MB (1024 * 1024)

#define STRMID_OFFSET	offsetof(WIN32_STREAM_ID, cStreamName)
#define MAX_ALTSTREAM	1000
#define REDUCE_SIZE		(1024 * 1024)
#define MAX_OPENTHREADS	16

inline int64 FASTCOPY_BUFSIZE(int64 ovl_size, int64 ovl_num) {
	int64	need_size = (int64)(ovl_size + 16*KB) * ovl_num * BUFIO_SIZERATIO;
	return	ALIGN_SIZE(need_size, MB);
}

inline int FASTCOPY_BUFMB(int ovl_mb, int ovl_num) {
	return	int(FASTCOPY_BUFSIZE(ovl_mb * MB, ovl_num) / MB);
}

struct TotalTrans {
	int		readDirs;
	int		readFiles;
	int		readAclStream;
	int64	readTrans;

	int		writeDirs;
	int		writeFiles;
	int		linkFiles;
	int		writeAclStream;
	int64	writeTrans;

	int		verifyFiles;
	int64	verifyTrans;

	int		deleteDirs;
	int		deleteFiles;
	int64	deleteTrans;

	int		skipDirs;
	int		skipFiles;
	int64	skipTrans;

	int		filterSrcSkips;
	int		filterDelSkips;
	int		filterDstSkips;

	int		allErrFiles;
	int		allErrDirs;
	int64	allErrTrans;

	int		rErrDirs;
	int		rErrFiles;
	int64	rErrTrans;

	int		wErrDirs;
	int		wErrFiles;
	int64	wErrTrans;

	int		dErrDirs;
	int		dErrFiles;
	int64	dErrTrans;

	int64	rErrStreamTrans;
	int		rErrAclStream;

	int64	wErrStreamTrans;
	int		wErrAclStream;

	int		openRetry;
	int		abortCnt;
};

struct TransInfo {
	BOOL				isPreSearch;
	TotalTrans			preTotal;
	TotalTrans			total;
	DWORD				fullTickCount;
	DWORD				execTickCount;
	BOOL				isSameDrv;
	DWORD				ignoreEvent;
	DWORD				waitLv;

	VBuf				*errBuf;
	CRITICAL_SECTION	*errCs;

	VBuf				*listBuf;
	CRITICAL_SECTION	*listCs;

	WCHAR				curPath[MAX_PATH_EX];
};

enum	FilterRes { FR_NONE=0, FR_UNMATCH, FR_MATCH, FR_CONT };

struct FileStat {
	int64		fileID;
	HANDLE		hFile;
	WCHAR		*upperName;		// cFileName 終端+1を指す
	FILETIME	ftCreationTime;
	FILETIME	ftLastAccessTime;
	FILETIME	ftLastWriteTime;
	DWORD		nFileSizeLow;	// WIN32_FIND_DATA の nFileSizeLow/High
	DWORD		nFileSizeHigh;	// とは逆順（int64 用）
	DWORD		dwFileAttributes;	// 0 == ALTSTREAM
	DWORD		dwReserved0; 		// WIN32_FIND_DATA の dwReserved0 (reparse tag)
	DWORD		lastError;
	bool		isExists;
	bool		isCaseChanged;
	bool		isWriteShare;
	bool		isNeedDel;
	FilterRes	filterRes;
	int			renameCount;
	int			size;
	int			minSize;		// upperName 分を含めない
	DWORD		hashVal;		// upperName の hash値
	HANDLE		hOvlFile;		// Overlap I/O用（BackupReadしない場合は、hFile と同じ値）

	// for hashTable
	FileStat	*next;

	// extraData
	BYTE		*acl;
	BYTE		*ead;
	BYTE		*rep;	// reparse data
	int			aclSize;
	int			eadSize;
	int			repSize;

	// md5/sha1/sha256 digest
	BYTE		digest[SHA256_SIZE];
	WCHAR		cFileName[2];	// 2 == dummy

	int64&	FileSize() { return *(int64 *)&nFileSizeLow; } // Low/Highの順序
	void	SetFileSize(int64 file_size) { *(int64 *)&nFileSizeLow = file_size; }
	void	SetLinkData(DWORD *data) { memcpy(digest, data, sizeof(DWORD) * 3); }
	DWORD	*GetLinkData() { return (DWORD *)digest; }
	int64&	CreateTime() { return *(int64 *)&ftCreationTime;   }
	int64&	AccessTime() { return *(int64 *)&ftLastAccessTime; }
	int64&	WriteTime()  { return *(int64 *)&ftLastWriteTime; }
};

inline int64 WriteTime(const WIN32_FIND_DATAW &fdat) {
	return *(int64 *)&fdat.ftLastWriteTime;
}
inline int64 FileSize(const WIN32_FIND_DATAW &fdat) {
	return ((int64)fdat.nFileSizeHigh << 32) + fdat.nFileSizeLow; // High/Lowの順序
}
inline int64 FileSize(const BY_HANDLE_FILE_INFORMATION &bhi) {
	return ((int64)bhi.nFileSizeHigh << 32) + bhi.nFileSizeLow; // High/Lowの順序
}

class StatHash {
	FileStat	**hashTbl;
	size_t		hashNum;

public:
	StatHash() {}
	BOOL	Init(VBVec<FileStat *> *statIdxVec);
	FileStat *Search(WCHAR *upperName, DWORD hash_val);
};

struct DirStatTag {
	int			statNum;
	int			oldStatSize;
	FileStat	*statBuf;
	FileStat	**statIndex;
};

struct LinkObj : public THashObj {
	WCHAR	*path;
	DWORD	data[3];
	int		len;
	DWORD	nLinks;
	LinkObj(const WCHAR *_path, DWORD _nLinks, DWORD *_data, int len=-1) {
		if (len == -1) len = (int)wcslen(_path) + 1;
		int alloc_len = len * sizeof(WCHAR);
		path = (WCHAR *)malloc(alloc_len);
		memcpy(path, _path, alloc_len);
		memcpy(data, _data, sizeof(data));
		nLinks = _nLinks;
	}
	~LinkObj() { if (path) free(path); }
};

class TLinkHashTbl : public THashTbl {
public:
	virtual BOOL IsSameVal(THashObj *obj, const void *_data) {
		return	!memcmp(&((LinkObj *)obj)->data, _data, sizeof(((LinkObj *)obj)->data));
	}

public:
	TLinkHashTbl() : THashTbl() {}
	virtual ~TLinkHashTbl() {}
	virtual BOOL Init(int _num) { return THashTbl::Init(_num); }
	u_int	MakeHashId(DWORD volSerial, DWORD highIdx, DWORD lowIdx) {
		DWORD data[] = { volSerial, highIdx, lowIdx };
		return	MakeHashId(data);
	}
	u_int	MakeHashId(DWORD *data) { return MakeHash(data, sizeof(DWORD) * 3); }
};


typedef std::vector<RegExp *> RegExpVec;

class FastCopy {
public:
	enum Mode { DIFFCP_MODE, SYNCCP_MODE, MOVE_MODE, MUTUAL_MODE, DELETE_MODE, TEST_MODE };
	enum OverWrite { BY_NAME, BY_ATTR, BY_LASTEST, BY_CONTENTS, BY_ALWAYS };
	enum FsType {
		FSTYPE_NONE		= 0x0000,
		FSTYPE_NTFS		= 0x0001,
		FSTYPE_FAT		= 0x0002,
		FSTYPE_REFS		= 0x0004,
		FSTYPE_NETWORK	= 0x0010,
		FSTYPE_SSD		= 0x0020,
		FSTYPE_WEBDAV	= 0x0040,
		FSTYPE_ACL		= 0x0100,
		FSTYPE_STREAM	= 0x0200,
		FSTYPE_REPARSE	= 0x0400,
		FSTYPE_HARDLINK	= 0x0800,
	};
	enum Flags : uint64 {
		CREATE_OPENERR_FILE	=	0x0000000000000001,
		USE_OSCACHE_READ	=	0x0000000000000002,
		USE_OSCACHE_WRITE	=	0x0000000000000004,
		PRE_SEARCH			=	0x0000000000000008,
		SAMEDIR_RENAME		=	0x0000000000000010,
		SKIP_EMPTYDIR		=	0x0000000000000020,
		FIX_SAMEDISK		=	0x0000000000000040,
		FIX_DIFFDISK		=	0x0000000000000080,
		AUTOSLOW_IOLIMIT	=	0x0000000000000100,
		WITH_ACL			=	0x0000000000000200,
		WITH_ALTSTREAM		=	0x0000000000000400,
		OVERWRITE_DELETE	=	0x0000000000000800,
		OVERWRITE_DELETE_NSA=	0x0000000000001000,
		OVERWRITE_PARANOIA	=	0x0000000000002000,
		REPARSE_AS_NORMAL	=	0x0000000000004000,
		COMPARE_CREATETIME	=	0x0000000000008000,
		SERIAL_MOVE			=	0x0000000000010000,
		SERIAL_VERIFY_MOVE	=	0x0000000000020000,
		RESTORE_HARDLINK	=	0x0000000000040000,
		DEL_BEFORE_CREATE	=	0x0000000000080000,
		DELDIR_WITH_FILTER	=	0x0000000000100000,
		NET_BKUPWR_NOOVL	=	0x0000000000200000,
		NO_LARGE_FETCH		=	0x0000000000400000,
		WRITESHARE_OPEN		=	0x0000000000800000,
		NO_SACL				=	0x0000000100000000,
		//
		LISTING				=	0x0000000001000000,
		LISTING_ONLY		=	0x0000000002000000,
		//
		REPORT_ACL_ERROR	=	0x0000000020000000,
		REPORT_STREAM_ERROR	=	0x0000000040000000,
		//
	};
	enum DlsvtMode {
		DLSVT_NONE			=	0x00000000,
		DLSVT_FAT			=	0x00000001,
		DLSVT_ALWAYS		=	0x00000002,
	};
	enum VerifyFlags {
		VERIFY_MD5			=	0x00000001,
		VERIFY_SHA1			=	0x00000002,
		VERIFY_SHA256		=	0x00000004,
		VERIFY_XXHASH		=	0x00000020,
		VERIFY_FILE			=	0x00001000,
		VERIFY_REMOVE		=	0x00002000,
		VERIFY_INFO			=	0x00004000,
	};
	enum FileLogFlags {
		FILELOG_TIMESTAMP	=	0x00000001,
		FILELOG_FILESIZE	=	0x00000002,
	};
	enum DebugFlags {
		OVERWRITE_JUDGE_LOGGING	=	0x00000001,
		OVL_LOGGING				=	0x00000002,
	};

	struct Info {
		DWORD	ignoreEvent;	// (I/ )
		Mode	mode;			// (I/ )
		OverWrite overWrite;	// (I/ )
		int64	flags;			// (I/ )
		int		verifyFlags;	// (I/ )
		int		dlsvtMode;		// (I/ )
		int64	timeDiffGrace;	// (I )
		int		fileLogFlags;	// (I/ )
		int		debugFlags;		// (I/ )	// 1: timestamp debug
		size_t	bufSize;		// (I/ )
		int		maxOpenFiles;	// (I/ )
		size_t	maxOvlSize;		// (I/ )
		BOOL	IsNormalOvl(int64 fsize) { return fsize >= maxOvlSize * min(maxOvlNum, 20); }
		BOOL	IsMinOvl(int64 fsize) { return fsize >= MIN_BUF * 2; }
		DWORD	maxOvlNum;		// (I/ )
		DWORD	GenOvlSize(int64 fsize) {
			int64 size = fsize / min(maxOvlNum, 20);
			if (size > MIN_BUF) {
				auto i = get_nlz64(size);
				DWORD unit = 1 << i;
				if (unit > maxOvlSize) {
					unit = (DWORD)maxOvlSize;
				}
				return	unit;
			}
			return MIN_BUF;
		}
		size_t	maxAttrSize;	// (I/ )
		size_t	maxDirSize;		// (I/ )
		size_t	maxMoveSize;	// (I/ )
		size_t	maxDigestSize;	// (I/ )
		int		minSectorSize;	// (I/ ) 最小セクタサイズ
		int64	nbMinSizeNtfs;	// (I/ ) FILE_FLAG_NO_BUFFERING でオープンする最小サイズ
		int64	nbMinSizeFat;	// (I/ ) FILE_FLAG_NO_BUFFERING でオープンする最小サイズ (FAT用)
		int		maxLinkHash;	// (I/ ) Dest Hardlink 用 hash table サイズ
		int64	allowContFsize;	// (I/ )
		HWND	hNotifyWnd;		// (I/ )
		UINT	uNotifyMsg;		// (I/ )
		int		lcid;			// (I/ )
		int64	fromDateFilter;	// (I/ ) 最古日時フィルタ
		int64	toDateFilter;	// (I/ ) 最新日時フィルタ
		int64	minSizeFilter;	// (I/ ) 最低サイズフィルタ
		int64	maxSizeFilter;	// (I/ ) 最大サイズフィルタ
		char	driveMap[64];	// (I/ ) 物理ドライブマップ
		int		maxRunNum;		// (I/ ) 最大並列同時数（強制実行の場合は無視）
		int		netDrvMode;		// (I/ ) ネットワークドライブの同一判定（DriveMng::NetDrvMode参照）
		int		aclReset;		// (I/ ) Create/Remove等で ACLリセットをどこまで行うか
		BOOL	isRenameMode;	// ( /O) ...「複製します」ダイアログタイトル用情報（暫定）
	};							//			 将来的に、情報が増えれば、メンバから切り離し

	enum Notify { END_NOTIFY, CONFIRM_NOTIFY, RENAME_NOTIFY, LISTING_NOTIFY };
	struct Confirm {
		enum Result { CANCEL_RESULT, IGNORE_RESULT, CONTINUE_RESULT };
		const WCHAR	*message;		// (I/ )
		BOOL		allow_continue;	// (I/ )
		const WCHAR	*path;			// (I/ )
		DWORD		err_code;		// (I/ )
		Result		result;			// ( /O)
	};


	struct WaitCalc {
		int64	lastTick = 0;
		int		lastRemain = 0;
		bool	lowPriority = false;
		FsType	fsType = FSTYPE_NONE;
	};
	enum	{ AUTOSLOW_WAITLV=0x1, SUSPEND_WAITLV=0x7ffffff };
	DWORD	wcIdx;

public:
	FastCopy(void);
	virtual ~FastCopy();

	BOOL RegisterInfo(const PathArray *_srcArray, const PathArray *_dstArray, Info *_info,
			const PathArray *_includeArray=NULL, const PathArray *_excludeArray=NULL);
	BOOL Start(TransInfo *ti);
	BOOL End(void);

	BOOL TakeExclusivePriv(int force_mode=0, ShareInfo::CheckInfo *ci=NULL) {
		return shareInfo.TakeExclusive(useDrives, info.maxRunNum, force_mode, ci);
	}
	BOOL ReleaseExclusivePriv() { return shareInfo.ReleaseExclusive(); }
	BOOL IsTakeExclusivePriv(void) { return shareInfo.IsTaken(); }
	BOOL GetRunningCount(ShareInfo::CheckInfo *ci, uint64 use_drives=0) {
		return shareInfo.GetCount(ci, use_drives);
	}
	BOOL IsStarting(void) { return hReadThread || hWriteThread; }
	BOOL Suspend(void);
	BOOL Resume(void);
	void Aborting(BOOL is_auto=FALSE);
	BOOL WriteErrLog(const WCHAR *message, int len=-1);
	BOOL IsAborting(void) { return isAbort; }
	void SetWaitLv(DWORD lv) { waitLv = lv; }
	DWORD GetWaitLv() { return waitLv; }
	BOOL GetTransInfo(TransInfo *ti, BOOL fullInfo=TRUE);
	VBuf *GetErrBuf() { return &errBuf; }

protected:
	enum Command {
		CHECKCREATE_TOPDIR,
		WRITE_FILE, WRITE_BACKUP_FILE, WRITE_FILE_CONT, WRITE_ABORT, WRITE_BACKUP_ACL,
		WRITE_BACKUP_EADATA, WRITE_BACKUP_ALTSTREAM, WRITE_BACKUP_END, CASE_CHANGE,
		CREATE_HARDLINK, REMOVE_FILES, MKDIR, INTODIR, DELETE_FILES, RETURN_PARENT, REQ_EOF,
		REQ_NONE
	};
	enum PutListOpt {
			PL_NORMAL=0x1, PL_DIRECTORY=0x2, PL_HARDLINK=0x4, PL_REPARSE=0x8,
			PL_CASECHANGE=0x10, PL_DELETE=0x20,
			PL_COMPARE=0x10000, PL_ERRMSG=0x20000, PL_NOADD=0x40000,
	};
	enum LinkStatus { LINK_ERR=0, LINK_NONE=1, LINK_ALREADY=2, LINK_REGISTER=3 };
	enum ConfirmErrFlags { CEF_STOP=0x0001, CEF_NOAPI=0x0002, CEF_DATACHANGED=0x0004 };

// mainBuf allocation
//
//         I/O request(1)                                I/O request(2)
//  +-------------------------------+         +---------------------------------+
//  |                               |         |                                 |
//  v                               |         v                                 |
//  | (buf-area).......  | ReqHead(buf,...)   | (next-buf-area) | next-ReqHead(buf) ...|...
//  :                    :                    :
//  :                    :                    :
//  :                    :                    :
//  |-----(readSize)-----| (major case)
//  |-(readSize)-|..gap..| (minor case)
//                       |------(reqSize)-----|
//  |---------------(totalSize)---------------|


	struct ReqHead : public TListObj {	// request header
		int64		reqId;
		Command		cmd;
		bool		isFlush;
		DWORD		bufSize;
		DWORD		readSize;	// 普通ファイルのみ有効
		BYTE		*buf;
		int			reqSize;	// request header size
		FileStat	stat;		// 可変長
	};

	struct MkDirInfo {
		FileStat	*stat;
		int			dirLen;
		bool		needExtra;
		bool		isMkDir;
		bool		isReparse;
	};

	struct MoveObj {
		int64		fileID;
		int64		fileSize;
		int64		wTime;
		enum		Status { START, DONE, ERR } status;
		DWORD		dwAttr;
		WCHAR		path[1];
	};
	struct DigestObj {
		int64		fileID;
		int64		fileSize;
		int64		wTime;
		DWORD		dwAttr;
		enum		Status { OK, NG, PASS } status;
		BYTE		digest[SHA256_SIZE];
		int			pathLen;
		WCHAR		path[1];
	};
	struct DigestCalc {
		int64		fileID;
		int64		fileSize;
		int64		wTime;
		DWORD		dwAttr;
		enum		Status { INIT, CONT, DONE, PRE_ERR, ERR, PASS, FIN } status;
		int			dataSize;
		BYTE		digest[SHA256_SIZE];
		BYTE		*data;
		WCHAR		path[1]; // さらに dstSector境界後にデータが続く
	};

	struct OverLap : public TListObj {
		DWORD		orderSize;
		DWORD		transSize;
		bool		waiting;
		union {
			void		*ptr;
			ReqHead		*req;	// for rOvl only
			BYTE		*buf;	// for MakeDigest
			DigestCalc	*calc;  // for MakeDigestAsync/wOvl
		};
		OVERLAPPED	ovl;
		OverLap()  {
			ovl.hEvent	= ::CreateEvent(0, TRUE, FALSE, NULL);
			orderSize	= 0;
			transSize	= 0;
			waiting		= false;
			SetOvlOffset(0);
		}
		~OverLap() { ::CloseHandle(ovl.hEvent); }
		void SetOvlOffset(int64 offset) {	// 32bit環境では Pointer は32bitのため
			*(int64 *)&ovl.Pointer = offset;
		}
		int64 GetOvlOffset() {
			return	*(int64 *)&ovl.Pointer;
		}
	};

	struct OpenDat : public TListObj {
		VBuf		path;
		FileStat	*stat = NULL;
		int			dir_len = 0;
		int			name_len = 0;

		OpenDat() : path(PAGE_SIZE, MAX_WPATH*sizeof(WCHAR)) {
		}
	};

	struct RandomDataBuf {	// 上書き削除用
		BOOL	is_nsa;
		DWORD	base_size;
		DWORD	buf_size;
		BYTE	*buf[3];
	};

	// 基本情報
	DriveMng	driveMng;	// Drive 情報
	Info		info;		// オプション指定等
	uint64		useDrives;
	StatHash	hash;
	PathArray	srcArray;
	PathArray	dstArray;

	WCHAR	*src;			// src パス格納用
	WCHAR	*dst;			// dst パス格納用
	WCHAR	*confirmDst;	// 上書き確認調査用
	WCHAR	*hardLinkDst;	// ハードリンク用
	int		srcBaseLen;		// src パスの固定部分の長さ
	int		dstBaseLen;		// dst パスの固定部分の長さ
	int		srcPrefixLen;	// \\?\ or \\?\UNC\ の長さ
	int		dstPrefixLen;
	BOOL	isExtendDir;
	int		depthIdxOffset;
	BOOL	isListing;
	BOOL	isListingOnly;
	BOOL	isPreSearch;
	BOOL	isExec;
	int		maxStatSize;	// max size of FileStat
	int64	nbMinSize;		// struct Info 参照
	int64	timeDiffGrace;
	BOOL	enableAcl;
	BOOL	enableStream;
	BOOL	enableBackupPriv;
	BOOL	enableSecName;
	DWORD	acsSysSec; // 有効な場合のみACCESS_SYSTEM_SECURITYが入る

	FINDEX_INFO_LEVELS
			findInfoLv; // FindFirstFileEx用info level
	DWORD	findFlags;  // FindFirstFileEx用flags
	DWORD	frdFlags;	// ForceRemoveDirectoryW用flags

	// セクタ情報など
	int		srcSectorSize;
	int		dstSectorSize;
	int		sectorSize;
	DWORD	maxReadSize;
	DWORD	maxDigestReadSize;
	DWORD	MaxReadDigestBuf() { return maxDigestReadSize * info.maxOvlNum; }
	DWORD	maxWriteSize;
	FsType	srcFsType;
	FsType	dstFsType;
	DWORD	flagOvl;
	WCHAR	src_root[MAX_PATH];

	TotalTrans	preTotal;	// ファイルコピー統計情報
	TotalTrans	total;		// ファイルコピー統計情報
	TotalTrans	*curTotal;

	// filter
	inline DWORD FilterBits(int file_dir_idx, int inc_exc_idx) {
		return 1 << ((file_dir_idx << 1) | inc_exc_idx); // (0x0-0xf)
	}
	enum		{ FILE_REG=0, DIR_REG=1, MAX_FILEDIR_REG=2 };
	enum		{ INC_REG=0,  EXC_REG=1, MAX_INCEXC_REG=2  };
	enum		{ REL_REG=0,  ABS_REG=1, MAX_RELABS_REG=2  };
	enum		{ REG_FILTER=0xf, DATE_FILTER=0x10, SIZE_FILTER=0x20 };
	DWORD		filterMode;

	RegExpVec	regExpVec[MAX_FILEDIR_REG][MAX_INCEXC_REG][MAX_RELABS_REG];
	VBVec<int>	srcDepth;
	VBVec<int>	dstDepth;
	VBVec<int>	cnfDepth;

	// バッファ
	VBuf	mainBuf;		// Read/Write 用 buffer
	VBuf	fileStatBuf;	// src file stat 用 buffer
	VBuf	dirStatBuf;		// src dir stat 用 buffer
	VBuf	dstStatBuf;		// dst dir/file stat 用 buffer

	VBVec<FileStat *> dstStatIdxVec;	// dstStatBuf 内 entry の index sort 用
	VBVec<MkDirInfo>  mkdirQueVec;

	VBuf	dstDirExtBuf;
	VBuf	errBuf;
	VBuf	listBuf;
	VBuf	ntQueryBuf;

	// データ転送キュー関連
	TListEx<ReqHead>	readReqList;
	TListEx<ReqHead>	writeReqList;
	TListEx<ReqHead>	writeWaitList;
	TListEx<ReqHead>	rDigestReqList;

	typedef TRecycleListEx<OverLap> OvlList;
	OvlList		rOvl;
	OvlList		wOvl;

	BYTE		*usedOffset;
	BYTE		*freeOffset;
	ReqHead		*writeReq;
	BOOL		readInterrupt;	// mainBuf終端で Write側に制御を移管
	BOOL		writeInterrupt; // mainBuf終端で Read側に制御を移管 or Digest計算割り込み
	int64		reqSendCount;
	int64		nextFileID;
	int64		errRFileID;
	int64		errWFileID;

	FileStat	**openFiles;
	int			openFilesCnt;
	typedef TRecycleListEx<OpenDat> OpList;
	OpList		opList;
	BOOL		opRun;

	// スレッド関連
	HANDLE		hReadThread;
	HANDLE		hWriteThread;
	HANDLE		hRDigestThread;
	HANDLE		hWDigestThread;
	HANDLE		hOpenThreads[MAX_OPENTHREADS];
	int			openThreadCnt = 0;
	ShareInfo	shareInfo;
	Condition	cv;
	Condition	opCv;

	CRITICAL_SECTION errCs;
	CRITICAL_SECTION listCs;

	// 時間情報
	DWORD	startTick;
	DWORD	preTick;
	DWORD	endTick;
	DWORD	suspendTick;
	DWORD	waitLv;

	// モード・フラグ類
	BOOL	isAbort;
	BOOL	isSuspend;
	BOOL	isSameDrv;
	BOOL	isSameVol;
	BOOL	isRename;
	BOOL	isDlsvt;
	enum	DstReqKind { DSTREQ_NONE=0, DSTREQ_READSTAT=1, DSTREQ_DIGEST=2 } dstAsyncRequest;
	void	*dstAsyncInfo;
	BOOL	dstRequestResult;
	enum	RunMode { RUN_NORMAL, RUN_DIGESTREQ, RUN_FINISH } runMode;

	// ダイジェスト関連
	class DigestBuf : public TDigest {
	public:
		DigestBuf() : TDigest() {}
		VBuf	buf;
		BYTE	val[SHA256_SIZE];
	};
	DigestBuf	srcDigest;
	DigestBuf	dstDigest;

	struct WInfo {
		int64	file_size;
		Command	cmd;
		bool	is_reparse;
		bool	is_hardlink;
		bool	is_stream;
		bool	is_nonbuf;
	};

	DataList	digestList;	// ハッシュ/Open記録（WriteThread のみ利用）
	bool IsUsingDigestList() {
		return (info.verifyFlags & VERIFY_FILE) && (info.flags & LISTING_ONLY) == 0;
	}
	bool IsReparseEx(DWORD attr) {
		return IsReparse(attr) && (info.flags & REPARSE_AS_NORMAL) == 0;
	}
	bool NeedSymlinkDeref(const WIN32_FIND_DATAW *fdat) {	// symlink の実体参照が必要かどうか
		return (info.flags & REPARSE_AS_NORMAL) && !IsDir(fdat->dwFileAttributes)
				&& IsSymlink(fdat);
	}
	bool NeedSymlinkDeref(const FileStat *stat) {
		return (info.flags & REPARSE_AS_NORMAL) && !IsDir(stat->dwFileAttributes)
				&& IsSymlink(stat);
	}
	enum		CheckDigestMode { CD_NOWAIT, CD_WAIT, CD_FINISH };
	DataList	wDigestList; // ダイジェスト算出用（Read用バッファ含む）

	DWORD		TransSize(DWORD trans_size) {
		if (waitLv && (info.flags & AUTOSLOW_IOLIMIT)) {
			return	(waitLv > 10) ? WAITMIN_BUF : WAITMID_BUF;
		}
		return	trans_size;
	}

	// 移動関連
	DataList		moveList;		// 移動
	DataList::Head	*moveFinPtr;	// 書き込み終了ID位置

	TLinkHashTbl	hardLinkList;	// ハードリンク用リスト

	static unsigned WINAPI ReadThread(void *fastCopyObj);
	static unsigned WINAPI OpenThread(void *fastCopyObj);
	static unsigned WINAPI WriteThread(void *fastCopyObj);
	static unsigned WINAPI DeleteThread(void *fastCopyObj);
	static unsigned WINAPI RDigestThread(void *fastCopyObj);
	static unsigned WINAPI WDigestThread(void *fastCopyObj);
	static unsigned WINAPI TestThread(void *fastCopyObj);

	BOOL ReadThreadCore(void);
	BOOL OpenThreadCore(void);
	BOOL DeleteThreadCore(void);
	BOOL WriteThreadCore(void);
	BOOL RDigestThreadCore(void);
	BOOL WDigestThreadCore(void);
	BOOL VerifyErrPostProc(DigestCalc *calc);
	BOOL AddVerifyInfo(DigestCalc *calc);

	LinkStatus CheckHardLink(WCHAR *path, int len=-1, HANDLE hFileOrg=INVALID_HANDLE_VALUE,
				DWORD *data=NULL);
	BOOL RestoreHardLinkInfo(DWORD *link_data, WCHAR *path, int base_len);

	BOOL AllocBuf(void);
	BOOL RegisterRegFilter(const PathArray *incArray, const PathArray *excArray);
	BOOL RegisterRegFilterCore(const PathArray *_path, bool is_inc);
	BOOL CleanRegFilter();

	BOOL CheckAndCreateDestDir(int dst_len);
	BOOL FinishNotify(void);
	BOOL ClearNonSurrogateReparse(WIN32_FIND_DATAW *fdat);
//	BOOL PreSearch(void);
//	BOOL PreSearchProc(WCHAR *path, int prefix_len, int dir_len, FilterRes fr);
	BOOL PutMoveList(WCHAR *path, int path_len, FileStat *stat, MoveObj::Status status);
	void FlushMoveListCore(MoveObj *data);
	BOOL FlushMoveList(BOOL is_finish=FALSE);
	BOOL PushMkdirQueue(FileStat *stat, int dlen, bool is_mkdir, bool extra, bool is_reparse);
	BOOL ReadProc(int dir_len, BOOL confirm_dir, FilterRes fr);
	BOOL ReadProcFileEntry(int dir_len, BOOL confirm_local);
	BOOL ReadProcDirEntry(int dir_len, int dirst_start, BOOL confirm_dir, FilterRes fr);
	BOOL ExecMkDirQueue(void);
	ReqHead *GetDirExtData(FileStat *stat);
	BOOL IsOverWriteFile(FileStat *srcStat, FileStat *dstStat);
	int  MakeRenameName(WCHAR *new_fname, int count, WCHAR *org_fname, BOOL is_dir=FALSE);
	BOOL SetRenameCount(FileStat *stat, BOOL is_dir=FALSE);
	FilterRes FilterCheck(WCHAR *dir, int dlen, FileStat *st, FilterRes fr) {
		return FilterCheck(dir, dlen, st->dwFileAttributes, st->cFileName,
				st->WriteTime(), st->FileSize(),fr);
	}
	FilterRes FilterCheck(WCHAR *dir, int dlen, WIN32_FIND_DATAW *fdat, FilterRes fr) {
		return FilterCheck(dir, dlen, fdat->dwFileAttributes, fdat->cFileName,
				WriteTime(*fdat), FileSize(*fdat), fr);
	}
	FilterRes FilterCheck(WCHAR *dir, int dir_len, DWORD attr, const WCHAR *fname,
				int64 wtime, int64 fsize, FilterRes fr);
	BOOL ReadDirEntry(int dir_len, BOOL confirm_dir, FilterRes fr);
	HANDLE CreateFileWithRetry(WCHAR *path, DWORD mode, DWORD share, SECURITY_ATTRIBUTES *sa,
			DWORD cr_mode, DWORD flg, HANDLE hTempl, int retry_max=10);
	BOOL OpenFileProc(FileStat *stat, int dir_len);
	BOOL OpenFileProcCore(WCHAR *path, FileStat *stat, int dir_len, int name_len);
	BOOL OpenFileQueue(WCHAR *path, FileStat *stat, int dir_len, int name_len);
	BOOL OpenFileBackupProc(WCHAR *path, FileStat *stat, int src_len);
	BOOL OpenFileBackupStreamLocal(WCHAR *path, FileStat *stat, int src_len, int *altdata_cnt);
	BOOL OpenFileBackupStreamCore(WCHAR *path, int src_len, int64 size, WCHAR *altname,
			int altnamesize, int *altdata_cnt);
	BOOL ReadMultiFilesProc(int dir_len);
	BOOL CloseMultiFilesProc(int maxCnt=0);
	WCHAR *RestorePath(WCHAR *path, int idx, int dir_len);
	BOOL WaitOverlapped(HANDLE hFile, OverLap *ovl);
	void SetTotalErrInfo(BOOL is_stream, int64 err_trans, BOOL is_write=FALSE);
	BOOL ReadFileWithReduce(HANDLE hFile, void *buf, DWORD size, OverLap *ovl=NULL);
	BOOL ReadFileAltStreamProc(int *open_idx, int dir_len, FileStat *stat);
	BOOL ReadFileReparse(Command cmd, int idx, int dir_len, FileStat *stat);
	void IoAbortFile(HANDLE hFile, OvlList *ovl_list);
	BOOL ReadAbortFile(int cur_idx, Command cmd, int dir_len, BOOL is_stream, BOOL is_modify);
	BOOL ReadFileProc(int *open_idx, int dir_len);
	BOOL ReadFileProcCore(int cur_idx, int dir_len, Command cmd, FileStat *stat);
	BOOL DeleteProc(WCHAR *path, int dir_len, FilterRes fr);
	BOOL DeleteDirProc(WCHAR *path, int dir_len, WCHAR *fname, FileStat *stat, FilterRes fr);
	BOOL DeleteFileProc(WCHAR *path, int dir_len, WCHAR *fname, FileStat *stat);

	void SetupRandomDataBuf(void);
	void GenRandomName(WCHAR *path, int fname_len, int ext_len);
	BOOL RenameRandomFname(WCHAR *org_path, WCHAR *rename_path, int dir_len, int fname_len);
	BOOL WriteRandomData(WCHAR *path, FileStat *stat, BOOL skip_hardlink);

	BOOL WaitOvlIo(HANDLE fh, OverLap *ovl, int64 *total_size, int64 *order_total);
	BOOL IsSameContents(FileStat *srcStat, FileStat *dstStat);
	BOOL MakeDigest(WCHAR *path, DigestBuf *dbuf, FileStat *stat);

	void DstRequest(DstReqKind kind, void *info=NULL);
	BOOL WaitDstRequest(void);
	BOOL CheckDstRequest(void);
	BOOL ReadDstStat(int dir_len);
	void FreeDstStat(void);
	static int SortStatFunc(const void *stat1, const void *stat2);
	BOOL WriteProc(int dir_len);
	BOOL CaseAlignProc(int dir_len=-1, BOOL need_log=FALSE);
	BOOL WriteDirProc(int dir_len);
	BOOL SetDirExtData(FileStat *stat);
	DigestCalc *GetDigestCalc(DigestObj *obj, DWORD require_size);
	BOOL PutDigestCalc(DigestCalc *obj, DigestCalc::Status status);
	BOOL MakeDigestAsync(DigestObj *obj);
	BOOL CheckDigests(CheckDigestMode mode);
	BOOL WriteFileWithReduce(HANDLE hFile, void *buf, DWORD size, OverLap *ovl);
	BOOL WriteDigestProc(int dst_len, FileStat *stat, DigestObj::Status status);
	BOOL WriteFileProc(int dst_len);
	BOOL WriteFileProcCore(HANDLE *_fh, FileStat *stat, WInfo *wi);
	BOOL WriteFileCore(HANDLE fh, FileStat *stat, WInfo *wi, DWORD mode, DWORD share, DWORD flg);
	BOOL WriteFileBackupProc(HANDLE fh, int dst_len);
	BOOL ChangeToWriteModeCore(BOOL is_finish=FALSE);
	BOOL ChangeToWriteMode(BOOL is_finish=FALSE);
	ReqHead *AllocReqBuf(int req_size, int64 _data_size, int64 fsize, int64 *buf_remain=NULL);
	ReqHead *PrepareReqBuf(int req_size, int64 data_size, int64 file_id, int64 fsize=0,
							int64 *buf_remain=NULL);
	BOOL CancelReqBuf(ReqHead *req);
	BOOL SendRequest(Command cmd, ReqHead *buf=NULL, FileStat *stat=NULL);
	BOOL SendRequestCore(Command cmd, ReqHead *buf, FileStat *stat);
	BOOL RecvRequest(BOOL keepCur=FALSE, BOOL freeLast=FALSE);
	void WriteReqDone(ReqHead *req);
	void SetErrRFileID(int64 file_id);
	void SetErrWFileID(int64 file_id);
	BOOL SetFinishFileID(int64 _file_id, MoveObj::Status status);
	BOOL EndCore(void);

	BOOL SetUseDriveMap(const WCHAR *path);
	BOOL InitSrcPath(int idx);
	BOOL InitDeletePath(int idx);
	BOOL InitDstPath(void);
	BOOL InitDepthBuf(void);
	FsType	GetFsType(const WCHAR *root_dir);
//	BOOL	IsLocalNTFS(FsType fstype) {
//		return (IsNTFS(fstype) && !IsNetFs(fstype)) ? TRUE : FALSE;
//	}
	BOOL	IsLocalModernFs(FsType fstype) {
		return (IsModernFs(fstype) && !IsNetFs(fstype)) ? TRUE : FALSE;
	}
//	BOOL	IsNTFS(FsType fstype) { return (fstype & FSTYPE_NTFS) ? TRUE : FALSE; }
//	BOOL	IsReFS(FsType fstype) { return (fstype & FSTYPE_REFS) ? TRUE : FALSE; }
	BOOL	IsNetFs(FsType fstype) { return (fstype & FSTYPE_NETWORK) ? TRUE : FALSE; }
	BOOL	IsModernFs(FsType fstype) { return (fstype & (FSTYPE_NTFS|FSTYPE_REFS)) ? TRUE:FALSE; }
	BOOL	IsLocalFs(FsType fstype) { return !IsNetFs(fstype); }
	BOOL	IsSSD(FsType fstype) { return (fstype & FSTYPE_SSD) ? TRUE : FALSE; }
	BOOL	IsWebDAV(FsType fstype) { return (fstype & FSTYPE_WEBDAV) ? TRUE : FALSE; }
	BOOL	IsAclFs(FsType fstype) { return (fstype & FSTYPE_ACL) ? TRUE : FALSE; }
	BOOL	IsStreamFs(FsType fstype) { return (fstype & FSTYPE_STREAM) ? TRUE : FALSE; }
	BOOL	IsReparseFs(FsType fsType) { return (fsType & FSTYPE_REPARSE) ? TRUE : FALSE; }
	BOOL	IsHardlinkFs(FsType fsType) { return (fsType & FSTYPE_HARDLINK) ? TRUE : FALSE; }
	BOOL	UseHardlink() {
		return (info.flags & RESTORE_HARDLINK) &&
				IsHardlinkFs(srcFsType) && IsHardlinkFs(dstFsType);
	}
	int GetSectorSize(const WCHAR *root_dir, BOOL use_cluster=FALSE);
	int MakeUnlimitedPath(WCHAR *buf);
	BOOL PutList(WCHAR *path, DWORD opt, DWORD lastErr=0, int64 wtime=-1, int64 size=-1,
			BYTE *digest=NULL);

	VBVec<int> &FindDepth(WCHAR *path) {
		return	(path == src) ? srcDepth : (path == dst) ? dstDepth : cnfDepth;
	}
	BOOL PushDepth(WCHAR *path, int len) {
		if ((filterMode & REG_FILTER) == 0) return FALSE;
		return FindDepth(path).Push(len);
	}
	BOOL PopDepth(WCHAR *path) {
		if ((filterMode & REG_FILTER) == 0) return FALSE;
		return FindDepth(path).Pop();
	}
	inline BOOL IsParentOrSelfDirs(WCHAR *name) {
		return name[0] == '.' && (name[1] == 0 || (name[1] == '.' && name[2] == 0));
	}
	int FdatToFileStat(WIN32_FIND_DATAW *fdat, FileStat *st, BOOL usehash, FilterRes fr=FR_NONE);
	Confirm::Result ConfirmErr(const WCHAR *msg, const WCHAR *path=NULL, DWORD flags=0);
	BOOL ConvertExternalPath(const WCHAR *path, WCHAR *buf, int buf_len);
	void WaitCheck();
	void CvWait(Condition *targ_cv, DWORD tick);

	BOOL TestWrite();
	void TestWriteEnd();
	void OvlLog(OverLap *ovl, const void *buf, const WCHAR *fmt,...); // for debug
};

void MakeVerifyStr(WCHAR *buf, BYTE *digest1, BYTE *digest2, DWORD digest_len);
BOOL DisableLocalBuffering(HANDLE hFile);
BOOL ModifyRealFdat(const WCHAR *path, WIN32_FIND_DATAW *fdat, BOOL backup_flg);
BOOL GetFileInformation(const WCHAR *path, BY_HANDLE_FILE_INFORMATION *bhi, BOOL backup_flg);
inline int wcscpy_with_aster(WCHAR *dst, WCHAR *src) {
	int	len = wcscpyz(dst, src);
	return	len + (int)wcscpyz(dst + len, L"\\*");
}

//	RegisterInfoProc
//	InitConfigProc
//	StartProc


