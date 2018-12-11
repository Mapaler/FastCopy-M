/* @(#)Copyright (C) 1996-2017 H.Shirouzu		tapi32ex.h	Ver0.99 */
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Main Header
	Create					: 2005-04-10(Sun)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef TAPI32EX_H
#define TAPI32EX_H

#include <richedit.h>

#define BFFM_SETSELECTIONW (WM_USER + 103)
#define LVM_INSERTITEMW (LVM_FIRST + 77)
DEFINE_GUID(IID_IShellLinkW, 0x000214F9, \
	0x0000, 0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);

#ifndef REGSTR_SHELLFOLDERS
#define REGSTR_SHELLFOLDERS		REGSTR_PATH_EXPLORER "\\Shell Folders"
#define REGSTR_MYDOCUMENT		"Personal"
#define REGSTR_MYDOCUMENT_W		L"Personal"
#endif

#define REGSTR_VAL_UNINSTALLER_PUBLISHER		"Publisher"
#define REGSTR_VAL_UNINSTALLER_DISPLAYICON		"DisplayIcon"
#define REGSTR_VAL_UNINSTALLER_DISPLAYVER		"DisplayVersion"
#define REGSTR_VAL_UNINSTALLER_ESTIMATESIZE		"EstimatedSize"
#define REGSTR_VAL_UNINSTALLER_HELPLINK			"HelpLink"
#define REGSTR_VAL_UNINSTALLER_URLUPDATEINFO	"URLUpdateInfo"
#define REGSTR_VAL_UNINSTALLER_URLINFOABOUT		"URLInfoAbout"
#define REGSTR_VAL_UNINSTALLER_COMMENTS			"Comments"

/* NTDLL */
//typedef LONG NTSTATUS, *PNTSTATUS;

typedef struct _IO_STATUS_BLOCK {
  union {
    NTSTATUS Status;
    PVOID    Pointer;
  };
  unsigned long *Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef enum _FILE_INFORMATION_CLASS {
  FileDirectoryInformation = 1,
  FileFullDirectoryInformation,
  FileBothDirectoryInformation,
  FileBasicInformation,
  FileStandardInformation,
  FileInternalInformation,
  FileEaInformation,
  FileAccessInformation,
  FileNameInformation,
  FileRenameInformation,
  FileLinkInformation,
  FileNamesInformation,
  FileDispositionInformation,
  FilePositionInformation,
  FileFullEaInformation,
  FileModeInformation,
  FileAlignmentInformation,
  FileAllInformation,
  FileAllocationInformation,
  FileEndOfFileInformation,
  FileAlternateNameInformation,
  FileStreamInformation,
  FilePipeInformation,
  FilePipeLocalInformation,
  FilePipeRemoteInformation,
  FileMailslotQueryInformation,
  FileMailslotSetInformation,
  FileCompressionInformation,
  FileObjectIdInformation,
  FileCompletionInformation,
  FileMoveClusterInformation,
  FileQuotaInformation,
  FileReparsePointInformation,
  FileNetworkOpenInformation,
  FileAttributeTagInformation,
  FileTrackingInformation,
  FileIdBothDirectoryInformation,
  FileIdFullDirectoryInformation,
  FileValidDataLengthInformation,
  FileShortNameInformation,
  FileIoCompletionNotificationInformation,
  FileIoStatusBlockRangeInformation,
  FileIoPriorityHintInformation,
  FileSfioReserveInformation,
  FileSfioVolumeInformation,
  FileHardLinkInformation,
  FileProcessIdsUsingFileInformation,
  FileNormalizedNameInformation,
  FileNetworkPhysicalNameInformation,
  FileIdGlobalTxDirectoryInformation,
  FileIsRemoteDeviceInformation,
  FileAttributeCacheInformation,
  FileNumaNodeInformation,
  FileStandardLinkInformation,
  FileRemoteProtocolInformation,
  FileMaximumInformation 
} FILE_INFORMATION_CLASS, *PFILE_INFORMATION_CLASS;

#define BACKUP_SPARSE_BLOCK 0x00000009

typedef struct _FILE_STREAM_INFORMATION {
  ULONG         NextEntryOffset;
  ULONG         StreamNameLength;
  LARGE_INTEGER StreamSize;
  LARGE_INTEGER StreamAllocationSize;
  WCHAR         StreamName[1];
} FILE_STREAM_INFORMATION, *PFILE_STREAM_INFORMATION;

extern NTSTATUS (WINAPI *pNtQueryInformationFile)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG,
	FILE_INFORMATION_CLASS);

extern NTSTATUS (WINAPI *pZwFsControlFile)(HANDLE, HANDLE, void *, PVOID, PIO_STATUS_BLOCK,
	ULONG, PVOID, ULONG, PVOID, ULONG);
#define IOCTL_LMR_DISABLE_LOCAL_BUFFERING 0x140390

u_int  MakeHash(const void *data, size_t size, u_int iv=0);
uint64 MakeHash64(const void *data, size_t size, uint64 iv=0);


#define SHA256_SIZE   32
#define SHA1_SIZE     20
#define MD5_SIZE      16
#define XXHASH_SIZE    8

class TDigest {
protected:
	HCRYPTPROV		hProv;
	HCRYPTHASH		hHash;
	bool			updated;
	bool			used;
	void			*xxHashState;

public:
	enum Type { SHA1, MD5, SHA256, XXHASH } type;

	TDigest();
	~TDigest();
	BOOL Init(Type _type=SHA1);
	BOOL Reset();
	BOOL Update(void *data, DWORD size);
	BOOL GetVal(void *data);
	BOOL GetRevVal(void *data);
	int  GetDigestSize() { return	type == MD5    ? MD5_SIZE    :
									type == SHA1   ? SHA1_SIZE   :
									type == SHA256 ? SHA256_SIZE : XXHASH_SIZE;
									}
	void GetEmptyVal(void *data);
};

#ifdef UNICODE
#define LoadStr LoadStrW
#else
#define LoadStr LoadStrA
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

#ifndef REPARSE_DATA_BUFFER_HEADER_SIZE
#define REPARSE_DATA_BUFFER_HEADER_SIZE  FIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer)
#endif

#ifndef REPARSE_GUID_DATA_BUFFER_HEADER_SIZE
#define REPARSE_GUID_DATA_BUFFER_HEADER_SIZE \
			FIELD_OFFSET(REPARSE_GUID_DATA_BUFFER, GenericReparseBuffer)
#endif

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

BOOL TLibInit_Ntdll();
BOOL TGenRandom(void *buf, size_t len);
BOOL TGenRandomMT(void *buf, size_t len);

BOOL TGetMachineId(GUID *guid);
const char *TGetMachineIdStr();
uint64 TGetHashedMachineId();
const char *TGetHashedMachineIdStr();
BOOL TGetHashedMachineId2(BYTE *md5);
const char *TGetHashedMachineIdStr2();

void TSetPubKeyBlob(BYTE *n, int n_size, int e, DynBuf *keyblob);

#endif
