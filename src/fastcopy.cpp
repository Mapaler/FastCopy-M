static char *fastcopy_id = 
	"@(#)Copyright (C) 2004-2012 H.Shirouzu		fastcopy.cpp	ver2.10";
/* ========================================================================
	Project  Name			: Fast Copy file and directory
	Create					: 2004-09-15(Wed)
	Update					: 2012-06-17(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "fastcopy.h"
#include <stdarg.h>
#include <stddef.h>
#include <process.h>
#include <stdio.h>

void *NTFS_STR_V;		// "NTFS"
void *FMT_RENAME_V;		// ".%03d"
void *FMT_PUTLIST_V;		//
void *FMT_STRNL_V;
void *FMT_REDUCEMSG_V;	//
void *PLSTR_LINK_V;
void *PLSTR_REPARSE_V;
void *PLSTR_REPDIR_V;
void *PLSTR_COMPARE_V;
void *PLSTR_CASECHANGE_V;

#define STRMID_OFFSET	offsetof(WIN32_STREAM_ID, cStreamName)
#define MAX_ALTSTREAM	1000
#define REDUCE_SIZE		(1024 * 1024)

#define IsDir(attr) (attr & FILE_ATTRIBUTE_DIRECTORY)
#define IsReparse(attr) (attr & FILE_ATTRIBUTE_REPARSE_POINT)
#define IsNoReparseDir(attr) (IsDir(attr) && !IsReparse(attr))

static BOOL (WINAPI *pSetFileValidData)(HANDLE hFile, LONGLONG ValidDataLength);

/*=========================================================================
  クラス ： FastCopy
  概  要 ： マルチスレッドなファイルコピー管理クラス
  説  明 ： 
  注  意 ： 
=========================================================================*/
FastCopy::FastCopy()
{
	hReadThread = hWriteThread = hRDigestThread = hWDigestThread = NULL;

	TSetPrivilege(SE_BACKUP_NAME, TRUE);
	TSetPrivilege(SE_RESTORE_NAME, TRUE);
	TSetPrivilege(SE_CREATE_SYMBOLIC_LINK_NAME, TRUE);

//	if (!pSetFileValidData) {
//		pSetFileValidData = (BOOL (WINAPI *)(HANDLE, LONGLONG))GetProcAddress(
//											GetModuleHandle("kernel32.dll"), "SetFileValidData");
//		if (pSetFileValidData && !TSetPrivilege(SE_MANAGE_VOLUME_NAME, TRUE)) {
//			pSetFileValidData = NULL;
//		}
//	}

	::InitializeCriticalSection(&errCs);
	::InitializeCriticalSection(&listCs);

	hRunMutex = ::CreateMutex(NULL, FALSE, FASTCOPY_MUTEX);

	if (IS_WINNT_V) {
		src = new WCHAR [MAX_PATHLEN_V + MAX_PATH];
		dst = new WCHAR [MAX_PATHLEN_V + MAX_PATH];
		confirmDst = new WCHAR [MAX_PATHLEN_V + MAX_PATH];
		NTFS_STR_V			= L"NTFS";
		FMT_INT_STR_V		= L"%d";
		FMT_RENAME_V		= L"%.*s(%d)%s";
		FMT_PUTLIST_V		= L"%c %s%s%s%s\r\n";
		FMT_STRNL_V			= L"%s\r\n";
		FMT_REDUCEMSG_V		= L"Reduce MaxIO(%c) size (%dMB -> %dMB)";
		PLSTR_LINK_V		= L" =>";
		PLSTR_REPARSE_V 	= L" ->";
		PLSTR_REPDIR_V  	= L"\\ ->";
		PLSTR_CASECHANGE_V	= L" (CaseChanged)";
		PLSTR_COMPARE_V		= L" !!";
	}
	else {
		src = new char [MAX_PATHLEN_V + MAX_PATH_EX];
		dst = new char [MAX_PATHLEN_V + MAX_PATH_EX];
		confirmDst = new char [MAX_PATHLEN_V + MAX_PATH_EX];
		NTFS_STR_V			= "NTFS";
		FMT_INT_STR_V		= "%d";
		FMT_RENAME_V		= "%.*s(%d)%s";
		FMT_PUTLIST_V		= "%c %s%s%s%s\r\n";
		FMT_STRNL_V			= "%s\r\n";
		FMT_REDUCEMSG_V		= "Reduce MaxIO(%c) size (%dMB -> %dMB)";
		PLSTR_LINK_V		= " =>";
		PLSTR_REPARSE_V		= " ->";
		PLSTR_REPDIR_V		= "\\ ->";
		PLSTR_CASECHANGE_V	= " (CaseChanged)";
		PLSTR_COMPARE_V		= " !!";
	}
	maxStatSize = (MAX_PATH * CHAR_LEN_V) * 2 + offsetof(FileStat, cFileName) + 8;
	waitTick = 0;
	hardLinkDst = NULL;
}

FastCopy::~FastCopy()
{
	delete [] confirmDst;
	delete [] dst;
	delete [] src;

	if (hRunMutex) {
		::CloseHandle(hRunMutex);
	}
	::DeleteCriticalSection(&listCs);
	::DeleteCriticalSection(&errCs);
}

FastCopy::FsType FastCopy::GetFsType(const void *root_dir)
{
	if (GetDriveTypeV(root_dir) == DRIVE_REMOTE)
		return	FSTYPE_NETWORK;

	DWORD	serial, max_fname, fs_flags;
	WCHAR	vol[MAX_PATH], fs[MAX_PATH];

	if (GetVolumeInformationV(root_dir, vol, MAX_PATH, &serial, &max_fname, &fs_flags,
			fs, MAX_PATH) == FALSE)
		return	ConfirmErr("GetVolumeInformation", root_dir, CEF_STOP), FSTYPE_NONE;

	return	lstrcmpiV(fs, NTFS_STR_V) == 0 ? FSTYPE_NTFS : FSTYPE_FAT;
}

int FastCopy::GetSectorSize(const void *root_dir)
{
	DWORD	spc, bps, fc, cl;

	if (GetDiskFreeSpaceV(root_dir, &spc, &bps, &fc, &cl) == FALSE) {
//		if (IS_WINNT_V)
//			ConfirmErr("GetDiskFreeSpace", root_dir, CEF_STOP);
		return	OPT_SECTOR_SIZE;
	}
	return	bps;
}

BOOL FastCopy::IsSameDrive(const void *root1, const void *root2)
{
	if (GetChar(root1, 1) == ':' && GetChar(root2, 1) == ':')
		return	driveMng.IsSameDrive(GetChar(root1, 0), GetChar(root2, 0));

	if (GetChar(root1, 1) != GetChar(root2, 1) || GetDriveTypeV(root1) != GetDriveTypeV(root2))
		return	FALSE;

	return	lstrcmpiV(root1, root2) == 0 ? TRUE : FALSE;
}

int FastCopy::MakeUnlimitedPath(WCHAR *buf)
{
	int		prefix_len;
	WCHAR	*prefix;
	BOOL	isUNC = (*buf == '\\') ? TRUE : FALSE;

	prefix		= isUNC ? PATH_UNC_PREFIX : PATH_LOCAL_PREFIX;
	prefix_len	= isUNC ? PATH_UNC_PREFIX_LEN : PATH_LOCAL_PREFIX_LEN;

	// (isUNC ? 1 : 0) ... PATH_UNC_PREFIX の場合、\\server -> \\?\UNC\server 
	//  にするため、\\server の頭の \ を一つ潰す。
	memmove(buf + prefix_len - (isUNC ? 1 : 0), buf, (strlenV(buf) + 1) * CHAR_LEN_V);
	memcpy(buf, prefix, prefix_len * CHAR_LEN_V);
	return	prefix_len;
}

BOOL FastCopy::InitDstPath(void)
{
	DWORD	attr;
	WCHAR	wbuf[MAX_PATH_EX];
	void	*buf = wbuf, *fname = NULL;
	const void	*org_path = dstArray.Path(0), *dst_root;

	// dst の確認/加工
	if (GetChar(org_path, 1) == ':' && GetChar(org_path, 2) != '\\')
		return	ConfirmErr(GetLoadStr(IDS_BACKSLASHERR), org_path, CEF_STOP|CEF_NOAPI), FALSE;

	if (GetFullPathNameV(org_path, MAX_PATHLEN_V, dst, &fname) == 0)
		return	ConfirmErr("GetFullPathName", org_path, CEF_STOP), FALSE;

	GetRootDirV(dst, buf);
	dstArray.RegisterPath(buf);
	dst_root = dstArray.Path(dstArray.Num() -1);

	attr = GetFileAttributesV(dst);

	if ((attr = GetFileAttributesV(dst)) == 0xffffffff) {
		info.overWrite = BY_ALWAYS;	// dst が存在しないため、調査の必要がない
//		if (isListing) PutList(dst, PL_DIRECTORY);
	}
	if (!IsDir(attr))	// 例外的に reparse point も dir 扱い
		return	ConfirmErr("Not a directory", dst, CEF_STOP|CEF_NOAPI), FALSE;

	strcpyV(buf, dst);
	MakePathV(dst, buf, EMPTY_STR_V);
	// src自体をコピーするか（dst末尾に \ がついている or 複数指定）
	isExtendDir = strcmpV(buf, dst) == 0 || srcArray.Num() > 1 ? TRUE : FALSE;
	dstPrefixLen = IS_WINNT_V ? MakeUnlimitedPath((WCHAR *)dst) : 0;
	dstBaseLen = strlenV(dst);

	// dst ファイルシステム情報取得
	dstSectorSize = GetSectorSize(dst_root);
	dstFsType = GetFsType(dst_root);
	nbMinSize = dstFsType == FSTYPE_NTFS ? info.nbMinSizeNtfs : info.nbMinSizeFat;

	// 差分コピー用dst先ファイル確認
	strcpyV(confirmDst, dst);

	return	TRUE;
}

BOOL FastCopy::InitSrcPath(int idx)
{
	DWORD		attr;
	BYTE		src_root_cur[MAX_PATH];
	WCHAR		wbuf[MAX_PATH_EX];
	void		*buf = wbuf, *fname = NULL;
	const void	*dst_root = dstArray.Path(dstArray.Num() -1);
	const void	*org_path = srcArray.Path(idx);
	DWORD		cef_flg = IsStarting() ? 0 : CEF_STOP;

	// src の確認/加工
	if (GetChar(org_path, 1) == ':' && GetChar(org_path, 2) != '\\')
		return	ConfirmErr(GetLoadStr(IDS_BACKSLASHERR), org_path, cef_flg|CEF_NOAPI), FALSE;

	if (GetFullPathNameV(org_path, MAX_PATHLEN_V, src, &fname) == 0)
		return	ConfirmErr("GetFullPathName", org_path, cef_flg), FALSE;
	GetRootDirV(src, src_root_cur);

	isMetaSrc = FALSE;

	if ((attr = GetFileAttributesV(src)) == 0xffffffff) {
		isMetaSrc = TRUE;
	}
	else if (IsDir(attr)) {
		// 親ディレクトリ自体をコピーしない場合、\* を付与
		strcpyV(buf, src);
		MakePathV(src, buf, ASTERISK_V);
		if (lstrcmpiV(buf, src_root_cur) && (isExtendDir || ((info.flags & DIR_REPARSE) == 0
		&& IsReparse(attr))))
			SetChar(src, strlenV(src) - 2, 0);	// 末尾に \* を付けない
		else
			isMetaSrc = TRUE;
	}
	srcPrefixLen = IS_WINNT_V ? MakeUnlimitedPath((WCHAR *)src) : 0;

	if (GetFullPathNameV(src, MAX_PATH_EX, buf, &fname) == 0 || fname == NULL)
		return	ConfirmErr("GetFullPathName2", MakeAddr(src, srcPrefixLen), cef_flg), FALSE;

	// 確認用dst生成
	strcpyV(MakeAddr(confirmDst, dstBaseLen), fname);

	// 同一パスでないことの確認
	if (lstrcmpiV(buf, confirmDst) == 0) {
		if (info.mode != DIFFCP_MODE || (info.flags & SAMEDIR_RENAME) == 0) {
			ConfirmErr(GetLoadStr(IDS_SAMEPATHERR), MakeAddr(confirmDst, dstBaseLen),
				CEF_STOP|CEF_NOAPI);
			return	FALSE;
		}
		strcpyV(MakeAddr(confirmDst, dstBaseLen), ASTERISK_V);
		isRename = TRUE;
	}
	else
		isRename = FALSE;

	if (info.mode == MOVE_MODE && IsNoReparseDir(attr)) {	// 親から子への移動は認めない
		int	end_offset = 0;
		if (GetChar(fname, 0) == '*' || attr == 0xffffffff) {
			SetChar(fname, 0, 0);
			end_offset = 1;
		}
		int		len = IS_WINNT_V ? strlenV(buf) : ::lstrlenA((char *)buf);
		if (strnicmpV(buf, confirmDst, len) == 0) {
			DWORD ch = GetChar(confirmDst, len - end_offset);
			if (ch == 0 || ch == '\\') {
				ConfirmErr(GetLoadStr(IDS_PARENTPATHERR), MakeAddr(buf, srcPrefixLen),
					CEF_STOP|CEF_NOAPI);
				return	FALSE;
			}
		}
	}

	SetChar(fname, 0, 0);
	srcBaseLen = strlenV(buf);

	// src ファイルシステム情報取得
	if (lstrcmpiV(src_root_cur, src_root)) {
		srcSectorSize = GetSectorSize(src_root_cur);
		srcFsType = GetFsType(src_root_cur);

		sectorSize = max(srcSectorSize, dstSectorSize);		// 大きいほうに合わせる
		sectorSize = max(sectorSize, MIN_SECTOR_SIZE);

		// 同一物理ドライブかどうかの調査
		if (info.flags & FIX_SAMEDISK)
			isSameDrv = TRUE;
		else if (info.flags & FIX_DIFFDISK)
			isSameDrv = FALSE;
		else
			isSameDrv = IsSameDrive(src_root_cur, dst_root);
		if (info.mode == MOVE_MODE)
			isSameVol = strcmpV(src_root_cur, dst_root);
	}

	enableAcl = (info.flags & WITH_ACL) && srcFsType != FSTYPE_FAT && dstFsType != FSTYPE_FAT;
	enableStream = (info.flags & WITH_ALTSTREAM) && srcFsType != FSTYPE_FAT
					&& dstFsType != FSTYPE_FAT;

	strcpyV(src_root, src_root_cur);

	// 最大転送サイズ
	DWORD tmpSize = isSameDrv ? info.bufSize : info.bufSize / 4;
	maxReadSize = min(tmpSize, maxReadSize);
	maxReadSize = max(MIN_BUF, maxReadSize);
	maxReadSize = maxReadSize / BIGTRANS_ALIGN * BIGTRANS_ALIGN;
	maxWriteSize = min(maxReadSize, maxWriteSize);
	maxDigestReadSize = min(maxReadSize, maxDigestReadSize);

	return	TRUE;
}

BOOL FastCopy::InitDeletePath(int idx)
{
	DWORD		attr;
	WCHAR		wbuf[MAX_PATH_EX];
	void		*buf = wbuf, *fname = NULL;
	const void	*org_path = srcArray.Path(idx);
	BYTE		dst_root[MAX_PATH];
	DWORD		cef_flg = IsStarting() ? 0 : CEF_STOP;

	// delete 用 path の確認/加工
	if (GetChar(org_path, 1) == ':' && GetChar(org_path, 2) != '\\')
		return	ConfirmErr(GetLoadStr(IDS_BACKSLASHERR), org_path, cef_flg|CEF_NOAPI), FALSE;

	if (GetFullPathNameV(org_path, MAX_PATHLEN_V, dst, &fname) == 0)
		return	ConfirmErr("GetFullPathName", org_path, cef_flg), FALSE;

	attr = GetFileAttributesV(dst);

	if (attr != 0xffffffff && IsNoReparseDir(attr)
	|| (info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA))) {
		GetRootDirV(dst, dst_root);
		if (info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA)) {
			dstSectorSize = GetSectorSize(dst_root);
			dstFsType = GetFsType(dst_root);
			nbMinSize = dstFsType == FSTYPE_NTFS ? info.nbMinSizeNtfs : info.nbMinSizeFat;
		}
	}

	isMetaSrc = FALSE;

	if (attr == 0xffffffff) {
		isMetaSrc = TRUE;
	}
	else if (IsDir(attr)) {
		strcpyV(buf, dst);
		// root_dir は末尾に "\*" を付与、それ以外は末尾の "\"を削除
		MakePathV(dst, buf, ASTERISK_V);
		if (!IsReparse(attr) && lstrcmpiV(buf, dst_root) == 0)
			isMetaSrc = TRUE;
		else
			SetChar(dst, strlenV(dst) - 2, 0);
	}
	dstPrefixLen = IS_WINNT_V ? MakeUnlimitedPath((WCHAR *)dst) : 0;

	if (GetFullPathNameV(dst, MAX_PATH_EX, buf, &fname) == 0 || fname == NULL)
		return	ConfirmErr("GetFullPathName2", MakeAddr(dst, dstPrefixLen), cef_flg), FALSE;
	SetChar(fname, 0, 0);
	dstBaseLen = strlenV(buf);

	if (info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA)) {
		strcpyV(confirmDst, dst);	// for renaming before deleting

		// 最大転送サイズ
		maxWriteSize = min((DWORD)info.bufSize, maxWriteSize);
		maxWriteSize = max(MIN_BUF, maxWriteSize);
	}

	return	TRUE;
}

BOOL FastCopy::RegisterInfo(const PathArray *_srcArray, const PathArray *_dstArray, Info *_info,
	const PathArray *_includeArray, const PathArray *_excludeArray)
{
	info = *_info;

	isAbort = FALSE;
	isRename = FALSE;
	isMetaSrc = FALSE;
	filterMode = 0;
	isListingOnly = (info.flags & LISTING_ONLY) ? TRUE : FALSE;
	isListing = (info.flags & LISTING) || isListingOnly ? TRUE : FALSE;
	endTick = 0;

	SetChar(src_root, 0, 0);

	if (isListingOnly) info.flags &= ~PRE_SEARCH;

	// 最大転送サイズ上限（InitSrcPath で再設定）
	maxReadSize = maxWriteSize = maxDigestReadSize = info.maxTransSize;

	// filter
	PathArray	pathArray[] = { *_includeArray, *_excludeArray };
	void		*path = NULL;

	for (int kind=0; kind < MAX_KIND_EXP; kind++) {
		for (int ftype=0; ftype < MAX_FTYPE_EXP; ftype++)
			regExp[kind][ftype].Init();

		for (int idx=0; idx < pathArray[kind].Num(); idx++) {
			int		len = lstrlenV(path = pathArray[kind].Path(idx)), targ = FILE_EXP;

			if (GetChar(path, len -1) == '\\') {
				SetChar(path, len -1, 0);
				targ = DIR_EXP;
			}
			if (!regExp[kind][targ].RegisterWildCard(path, RegExp::CASE_INSENSE))
				return	ConfirmErr("Bad or Too long windcard string",
							path, CEF_STOP|CEF_NOAPI), FALSE;
		}
	}
	if (path) filterMode |= REG_FILTER;
	if (info.fromDateFilter != 0  || info.toDateFilter  != 0)  filterMode |= DATE_FILTER;
	if (info.minSizeFilter  != -1 || info.maxSizeFilter != -1) filterMode |= SIZE_FILTER;

	if (!isListingOnly &&
		(info.mode != DELETE_MODE || (info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA))) &&
#ifdef _WIN64
		(                          info.bufSize < MIN_BUF * 2))
#else
		(info.bufSize > MAX_BUF || info.bufSize < MIN_BUF * 2))
#endif
		return	ConfirmErr("Too large or small Main Buffer.", NULL, CEF_STOP), FALSE;

	if ((info.flags & (DIR_REPARSE|FILE_REPARSE))
	&& (info.mode == MOVE_MODE || info.mode == DELETE_MODE)) {
		return	ConfirmErr("Illega Flags (junction/symlink)", NULL, CEF_STOP|CEF_NOAPI), FALSE;
	}

	if ((info.flags & RESTORE_HARDLINK) && !CreateHardLinkV) {
		return	ConfirmErr("Illega Flags (CreateHardLink)", NULL, CEF_STOP|CEF_NOAPI), FALSE;
	}

	// command
	if (info.mode == DELETE_MODE) {
		srcArray = *_srcArray;
		if (InitDeletePath(0) == FALSE)
			return	FALSE;
	}
	else {
		srcArray = *_srcArray;
		dstArray = *_dstArray;

		driveMng.SetDriveMap(info.driveMap);

		if (InitDstPath() == FALSE)
			return	FALSE;
		if (InitSrcPath(0) == FALSE)
			return	FALSE;
	}
	_info->isRenameMode = isRename;

	return	!isAbort;
}

BOOL FastCopy::TakeExclusivePriv(void)
{
	return	!hRunMutex || ::WaitForSingleObject(hRunMutex, 0) == WAIT_OBJECT_0 ? TRUE : FALSE;
}

BOOL FastCopy::AllocBuf(void)
{
	int		allocSize = isListingOnly ? MAX_LIST_BUF : info.bufSize + PAGE_SIZE * 4;
	BOOL	need_mainbuf = info.mode != DELETE_MODE ||
					((info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA)) && !isListingOnly);

	// メインリングバッファ確保
	if (need_mainbuf && mainBuf.AllocBuf(allocSize) == FALSE) {
		return	ConfirmErr("Can't alloc memory(mainBuf)", NULL, CEF_STOP), FALSE;
	}
	usedOffset = freeOffset = mainBuf.Buf();	// リングバッファ用オフセット初期化

	if (errBuf.AllocBuf(MIN_ERR_BUF, MAX_ERR_BUF) == FALSE) {
		return	ConfirmErr("Can't alloc memory(errBuf)", NULL, CEF_STOP), FALSE;
	}
	if (isListing) {
		if (listBuf.AllocBuf(MIN_PUTLIST_BUF, MAX_PUTLIST_BUF) == FALSE)
			return	ConfirmErr("Can't alloc memory(listBuf)", NULL, CEF_STOP), FALSE;
	}
	if (info.mode == DELETE_MODE) {
		if (need_mainbuf) SetupRandomDataBuf();
		return	TRUE;
	}

	openFiles = new FileStat *[info.maxOpenFiles + MAX_ALTSTREAM]; /* for Alternate Stream */
	openFilesCnt = 0;

	if (info.flags & RESTORE_HARDLINK) {
		hardLinkDst = new WCHAR [MAX_PATHLEN_V + MAX_PATH];
		memcpy(hardLinkDst, dst, dstBaseLen * CHAR_LEN_V);
		if (!hardLinkList.Init(info.maxLinkHash) || !hardLinkDst)
			return	ConfirmErr("Can't alloc memory(hardlink)", NULL, CEF_STOP), FALSE;
	}

	if (IsUsingDigestList()) {
		digestList.Init(MIN_DIGEST_LIST, MAX_DIGEST_LIST, MIN_DIGEST_LIST);
		int require_size = (maxReadSize + PAGE_SIZE) * 4;
		if (require_size > allocSize) require_size = allocSize;

		maxDigestReadSize = (require_size / 4 - PAGE_SIZE) / BIGTRANS_ALIGN * BIGTRANS_ALIGN;
		maxDigestReadSize = max(MIN_BUF, maxDigestReadSize);

		if (!wDigestList.Init(MIN_BUF, require_size, PAGE_SIZE))
			return	ConfirmErr("Can't alloc memory(digest)", NULL, CEF_STOP), FALSE;
	}

	if (info.flags & VERIFY_FILE) {
		srcDigest.Init((info.flags & VERIFY_MD5) ? TDigest::MD5 : TDigest::SHA1);
		dstDigest.Init((info.flags & VERIFY_MD5) ? TDigest::MD5 : TDigest::SHA1);

		if (isListingOnly) {
			srcDigestBuf.AllocBuf(0, maxReadSize);
			dstDigestBuf.AllocBuf(0, maxReadSize);
		}
	}

	if (info.mode == MOVE_MODE) {
		if (!moveList.Init(MIN_MOVEPATH_LIST, MAX_MOVEPATH_LIST, MIN_MOVEPATH_LIST))
			return	ConfirmErr("Can't alloc memory(moveList)", NULL, CEF_STOP), FALSE;
	}

	if (info.flags & WITH_ALTSTREAM) {
		ntQueryBuf.AllocBuf(MAX_NTQUERY_BUF, MAX_NTQUERY_BUF);
		if (!pNtQueryInformationFile) {
			TLibInit_Ntdll();
			if (!pNtQueryInformationFile) {
				ConfirmErr("Can't load NtQueryInformationFile", NULL, CEF_STOP|CEF_NOAPI);
				return FALSE;
			}
		}
	}

	// src/dst dir-entry/attr 用バッファ確保
	dirStatBuf.AllocBuf(MIN_ATTR_BUF, info.maxDirSize);
	if (info.flags & SKIP_EMPTYDIR)
		mkdirQueueBuf.AllocBuf(MIN_MKDIRQUEUE_BUF, MAX_MKDIRQUEUE_BUF);
	dstDirExtBuf.AllocBuf(MIN_DSTDIREXT_BUF, MAX_DSTDIREXT_BUF);

//	VBuf	reserveBuf(0, RESERVE_BUF);
//	for (int size=MAX_BASE_BUF; size >= MIN_BASE_BUF; size -= MIN_BASE_BUF) {
//		if (baseBuf.AllocBuf(0, size)) break;
//	}
//	reserveBuf.FreeBuf();
//
//	int remain_size = baseBuf.MaxSize() - baseBuf.UsedSize() - (3 * PAGE_SIZE);
//	int max_attr = (int)(remain_size / ((info.overWrite == BY_ALWAYS && !isRename) ? 1.0 : 2.04));
//	max_attr = max_attr / PAGE_SIZE * PAGE_SIZE;
//
//	if (max_attr >= info.maxAttrSize) {
//		fileStatBuf.AllocBuf(MIN_ATTR_BUF, max_attr, &baseBuf);
//		if (info.overWrite != BY_ALWAYS) {
//			dstStatBuf.AllocBuf(MIN_ATTR_BUF, max_attr, &baseBuf);
//			dstStatIdxBuf.AllocBuf(MIN_ATTRIDX_BUF, MAX_ATTRIDX_BUF(max_attr), &baseBuf);
//		}
//	}

	fileStatBuf.AllocBuf(MIN_ATTR_BUF, info.maxAttrSize);
	dstStatBuf.AllocBuf(MIN_ATTR_BUF, info.maxAttrSize);
	dstStatIdxBuf.AllocBuf(MIN_ATTRIDX_BUF, MAX_ATTRIDX_BUF(info.maxAttrSize));

	if (!fileStatBuf.Buf() || !dirStatBuf.Buf() || !dstStatBuf.Buf() || !dstStatIdxBuf.Buf()
	|| (info.flags & SKIP_EMPTYDIR) && !mkdirQueueBuf.Buf() || !dstDirExtBuf.Buf()
	|| (info.flags & WITH_ALTSTREAM) && !ntQueryBuf.Buf()) {
		return	ConfirmErr("Can't alloc memory(misc stat)", NULL, CEF_STOP), FALSE;
	}
	return	TRUE;
}

BOOL FastCopy::Start(TransInfo *ti)
{
	u_int	id;

	memset(&total, 0, sizeof(total));
	if (info.flags & PRE_SEARCH)
		total.isPreSearch = TRUE;

	isAbort = FALSE;
	writeReq = NULL;
	isSuspend = FALSE;
	dstAsyncRequest = DSTREQ_NONE;
	nextFileID = 1;
	errFileID = 0;
	openFiles = NULL;
	moveFinPtr = NULL;
	runMode = RUN_NORMAL;
	nThreads = info.mode == DELETE_MODE ? 1 : IsUsingDigestList() ? 4 : 2;
	hardLinkDst = NULL;

	cv.Initialize(nThreads);
	readReqList.Init();
	writeReqList.Init();
	rDigestReqList.Init();

	if (AllocBuf() == FALSE) goto ERR;

	startTick = ::GetTickCount();
	if (ti) GetTransInfo(ti, FALSE);

#define FASTCOPY_STACKSIZE (8 * 1024)

	if (info.mode == DELETE_MODE) {
		if (!(hReadThread = (HANDLE)_beginthreadex(0, 0, FastCopy::DeleteThread, this,
				FASTCOPY_STACKSIZE, &id)))
			goto ERR;
		return	TRUE;
	}

	if (!(hWriteThread = (HANDLE)_beginthreadex(0, 0,
										FastCopy::WriteThread, this, FASTCOPY_STACKSIZE, &id))
	||  !(hReadThread  = (HANDLE)_beginthreadex(0, 0,
										FastCopy::ReadThread, this, FASTCOPY_STACKSIZE, &id))
	|| IsUsingDigestList() && !(hRDigestThread = (HANDLE)_beginthreadex(0, 0,
										FastCopy::RDigestThread, this, FASTCOPY_STACKSIZE, &id))
	|| IsUsingDigestList() && !(hWDigestThread = (HANDLE)_beginthreadex(0, 0,
										FastCopy::WDigestThread, this, FASTCOPY_STACKSIZE, &id))
	)
		goto ERR;

	return	TRUE;

ERR:
	End();
	return	FALSE;
}

/*=========================================================================
  関  数 ： ReadThread
  概  要 ： Read 処理
  説  明 ： 
  注  意 ： 
=========================================================================*/
unsigned WINAPI FastCopy::ReadThread(void *fastCopyObj)
{
	return	((FastCopy *)fastCopyObj)->ReadThreadCore();
}

BOOL FastCopy::ReadThreadCore(void)
{
	BOOL	isSameDrvOld;
	int		done_cnt = 0;

	if (info.flags & PRE_SEARCH)
		PreSearch();

	for (int i=0; i < srcArray.Num(); i++) {
		if (InitSrcPath(i)) {
			if (done_cnt >= 1 && isSameDrvOld != isSameDrv) {
				ChangeToWriteMode();
			}
			ReadProc(srcBaseLen, info.overWrite == BY_ALWAYS ? FALSE : TRUE);
			isSameDrvOld = isSameDrv;
			done_cnt++;
		}
		if (isAbort)
			break;
	}
	SendRequest(REQ_EOF);

	if (isSameDrv) {
		ChangeToWriteMode(TRUE);
	}
	if (info.mode == MOVE_MODE && !isAbort) {
		FlushMoveList(TRUE);
	}

	while (hWriteThread || hRDigestThread || hWDigestThread) {
		if (hWriteThread) {
			if (::WaitForSingleObject(hWriteThread, 1000) != WAIT_TIMEOUT) {
				::CloseHandle(hWriteThread);
				hWriteThread = NULL;
			}
		}
		else if (hRDigestThread) {
			if (::WaitForSingleObject(hRDigestThread, 1000) != WAIT_TIMEOUT) {
				::CloseHandle(hRDigestThread);
				hRDigestThread = NULL;
			}
		}
		else if (hWDigestThread) {
			if (::WaitForSingleObject(hWDigestThread, 1000) != WAIT_TIMEOUT) {
				::CloseHandle(hWDigestThread);
				hWDigestThread = NULL;
			}
		}
	}

	FinishNotify();

	return	TRUE;
}

BOOL FastCopy::PreSearch(void)
{
	BOOL	is_delete = info.mode == DELETE_MODE;
	BOOL	(FastCopy::*InitPath)(int) = is_delete ?
				&FastCopy::InitDeletePath : &FastCopy::InitSrcPath;
	void	*&path = is_delete ? dst : src;
	int		&prefix_len = is_delete ? dstPrefixLen : srcPrefixLen;
	int		&base_len = is_delete ? dstBaseLen : srcBaseLen;
	BOOL	ret = TRUE;

	for (int i=0; i < srcArray.Num(); i++) {
		if ((this->*InitPath)(i)) {
			if (!PreSearchProc(path, prefix_len, base_len))
				ret = FALSE;
		}
		if (isAbort)
			break;
	}
	total.isPreSearch = FALSE;
	startTick = ::GetTickCount();

	return	ret && !isAbort;
}

BOOL FastCopy::FilterCheck(const void *path, const void *fname, DWORD attr, _int64 write_time,
	_int64 file_size)
{
	int	targ = IsDir(attr) ? DIR_EXP : FILE_EXP;

	if (targ == FILE_EXP) {
		if (write_time < info.fromDateFilter
		|| (info.toDateFilter && write_time > info.toDateFilter)) {
			return	FALSE;
		}
		if (file_size < info.minSizeFilter
		|| (info.maxSizeFilter != -1 && file_size > info.maxSizeFilter)) {
			return	FALSE;
		}
	}

	if ((filterMode & REG_FILTER) == 0)
		return	TRUE;

	if (regExp[EXC_EXP][targ].IsMatch(fname))
		return	FALSE;

	if (!regExp[INC_EXP][targ].IsRegistered())
		return	TRUE;

	if (regExp[INC_EXP][targ].IsMatch(fname))
		return	TRUE;

	return	FALSE;
}

BOOL FastCopy::PreSearchProc(void *path, int prefix_len, int dir_len)
{
	HANDLE		hDir;
	BOOL		ret = TRUE;
	WIN32_FIND_DATAW fdat;

	if (waitTick) Wait(1);

	if ((hDir = FindFirstFileV(path, &fdat)) == INVALID_HANDLE_VALUE) {
		return	ConfirmErr("FindFirstFile(pre)", MakeAddr(path, prefix_len)), FALSE;
	}

	do {
		if (IsParentOrSelfDirs(fdat.cFileName))
			continue;

		// src ディレクトリ自体に対しては、フィルタ対象にしない
		if ((dir_len != srcBaseLen || isMetaSrc
		|| !IsDir(fdat.dwFileAttributes))
				&& !FilterCheck(path, fdat.cFileName, fdat.dwFileAttributes, WriteTime(fdat),
					FileSize(fdat)))
			continue;

		if (IsDir(fdat.dwFileAttributes)) {
			total.preDirs++;
			if (!IsReparse(fdat.dwFileAttributes) || (info.flags & DIR_REPARSE)) {
				int len = sprintfV(MakeAddr(path, dir_len), FMT_CAT_ASTER_V, fdat.cFileName) -1;
				ret = PreSearchProc(path, prefix_len, dir_len + len);
			}
		}
		else {
			total.preFiles++;
			total.preTrans += FileSize(fdat);
		}
	}
	while (!isAbort && FindNextFileV(hDir, &fdat));

	if (!isAbort && ret && ::GetLastError() != ERROR_NO_MORE_FILES) {
		ret = FALSE;
		ConfirmErr("FindNextFile(pre)", MakeAddr(path, prefix_len));
	}

	::FindClose(hDir);

	return	ret && !isAbort;
}

BOOL FastCopy::PutList(void *path, DWORD opt, DWORD lastErr, BYTE *digest)
{
	BOOL	add_backslash = GetChar(path, 0) == '\\' && GetChar(path, 1) != '\\';

	if (!listBuf.Buf()) return FALSE;

	::EnterCriticalSection(&listCs);

	int require_size = MAX_PATHLEN_V + 1024;

	if (listBuf.RemainSize() < require_size) {
		if (listBuf.Size() < listBuf.MaxSize()) {
			listBuf.Grow(MIN_PUTLIST_BUF);
		}
		if (listBuf.RemainSize() < require_size) {
			::SendMessage(info.hNotifyWnd, info.uNotifyMsg, LISTING_NOTIFY, 0);
		}
	}

	if (listBuf.RemainSize() >= require_size) {
		int	len = 0;

		if (opt & PL_ERRMSG) {
			len = sprintfV(listBuf.Buf() + listBuf.UsedSize(), FMT_STRNL_V, path);
		}
		else {
			WCHAR	wbuf[64];
			if (digest) {
				if (IS_WINNT_V) {
					memcpy(wbuf, L"  :", 6);
					bin2hexstrW(digest, dstDigest.GetDigestSize(), wbuf + 3);
				}
				else {
					memcpy(wbuf, "  :", 3);
					bin2hexstr(digest, dstDigest.GetDigestSize(), (char *)wbuf + 3);
				}
			}

			len = sprintfV(listBuf.Buf() + listBuf.UsedSize(), FMT_PUTLIST_V,
							(opt & PL_NOADD) ? ' ' : (opt & PL_DELETE) ? '-' : '+',
							add_backslash ? BACK_SLASH_V : EMPTY_STR_V,
							path,
							(opt & PL_DIRECTORY) && (opt & PL_REPARSE) ? PLSTR_REPDIR_V :
							(opt & PL_DIRECTORY) ? BACK_SLASH_V :
							(opt & PL_REPARSE) ? PLSTR_REPARSE_V :
							(opt & PL_HARDLINK) ? PLSTR_LINK_V :
							(opt & PL_CASECHANGE) ? PLSTR_CASECHANGE_V :
							(opt & PL_COMPARE) || lastErr ? PLSTR_COMPARE_V : EMPTY_STR_V,
							digest ? wbuf : EMPTY_STR_V
							);
		}
		listBuf.AddUsedSize(len * CHAR_LEN_V);
	}

	::LeaveCriticalSection(&listCs);
	return	TRUE;
}

BOOL FastCopy::MakeDigest(void *path, VBuf *vbuf, TDigest *digest, BYTE *val, _int64 *fsize)
{
	DWORD	flg = ((info.flags & USE_OSCACHE_READVERIFY) ? 0 : FILE_FLAG_NO_BUFFERING)
					| FILE_FLAG_SEQUENTIAL_SCAN;
	DWORD	share = FILE_SHARE_READ|FILE_SHARE_WRITE;
	HANDLE	hFile = CreateFileWithRetry(path, GENERIC_READ, share, 0, OPEN_EXISTING, flg, 0, 5);
	BOOL	ret = FALSE;

	memset(val, 0, digest->GetDigestSize());
	digest->Reset();

	if (hFile == INVALID_HANDLE_VALUE)
		return	FALSE;

	if ((DWORD)vbuf->Size() >= maxReadSize || vbuf->Grow(maxReadSize)) {
		BY_HANDLE_FILE_INFORMATION	bhi;
		if (::GetFileInformationByHandle(hFile, &bhi)) {
			_int64	remain_size = FileSize(bhi);
			if (fsize) {
				*fsize = remain_size;
			}
			while (remain_size > 0 && !isAbort) {
				DWORD	trans_size = 0;
				if (ReadFileWithReduce(hFile, vbuf->Buf(), maxReadSize, &trans_size, NULL)
				&& trans_size > 0) {
					digest->Update(vbuf->Buf(), trans_size);
					remain_size -= trans_size;
					total.verifyTrans += trans_size;
				}
				else {
					break;
				}
			}
			if (remain_size == 0) {
				ret = digest->GetVal(val);
			}
		}
	}

	::CloseHandle(hFile);
	return	ret;
}

void MakeVerifyStr(char *buf, BYTE *digest1, BYTE *digest2, DWORD digest_len)
{
	char	*p = buf + sprintf(buf, "Verify Error src:");

	p += bin2hexstr(digest1, digest_len, p);
	p += sprintf(p, " dst:");
	p += bin2hexstr(digest2, digest_len, p);
	p += sprintf(p, " ");
}

BOOL FastCopy::IsSameContents(FileStat *srcStat, FileStat *dstStat)
{
/*	if (srcStat->FileSize() != dstStat->FileSize()) {
		return;
	}
*/
	if (!isSameDrv) {
		DstRequest(DSTREQ_DIGEST);
	}

	BOOL	src_ret = MakeDigest(src, &srcDigestBuf, &srcDigest, srcDigestVal);
	BOOL	dst_ret = isSameDrv ? MakeDigest(confirmDst, &dstDigestBuf, &dstDigest, dstDigestVal)
						: WaitDstRequest();
	BOOL	ret = src_ret && dst_ret && memcmp(srcDigestVal, dstDigestVal,
										srcDigest.GetDigestSize()) == 0 ? TRUE : FALSE;

	if (ret) {
		total.verifyFiles++;
		if ((info.flags & DISABLE_COMPARE_LIST) == 0) {
			PutList(MakeAddr(confirmDst, dstPrefixLen), PL_NOADD, 0, srcDigestVal);
		}
	}
	else {
		total.errFiles++;
//		PutList(MakeAddr(src,        srcPrefixLen), PL_COMPARE|PL_NOADD, 0, srcDigestVal);
//		PutList(MakeAddr(confirmDst, dstPrefixLen), PL_COMPARE|PL_NOADD, 0, dstDigestVal);
		if (src_ret && dst_ret) {
			char	buf[512];
			MakeVerifyStr(buf, srcDigestVal, dstDigestVal, dstDigest.GetDigestSize());
			ConfirmErr(buf, MakeAddr(confirmDst, dstPrefixLen), CEF_NOAPI);
		}
		else if (!src_ret) {
			ConfirmErr("Can't get src digest", MakeAddr(src, srcPrefixLen), CEF_NOAPI);
		}
		else if (!dst_ret) {
			ConfirmErr("Can't get dst digest", MakeAddr(confirmDst, dstPrefixLen), CEF_NOAPI);
		}
	}

	return	ret;
}

FastCopy::LinkStatus FastCopy::CheckHardLink(void *path, int len, HANDLE hFileOrg, DWORD *data)
{
	HANDLE		hFile;
	LinkStatus	ret = LINK_ERR;
	DWORD		share = FILE_SHARE_READ|FILE_SHARE_WRITE;

	if (hFileOrg == INVALID_HANDLE_VALUE) {
		if ((hFile = CreateFileV(path, 0, share, 0, OPEN_EXISTING, 0, 0))
				== INVALID_HANDLE_VALUE) {
//			DBGWriteW(L"CheckHardLink can't open(%s) %d\n", path);
			return	ret;
		}
	}
	else hFile = hFileOrg;

	BY_HANDLE_FILE_INFORMATION	bhi;
	if (::GetFileInformationByHandle(hFile, &bhi) && bhi.nNumberOfLinks >= 2) {
		DWORD	_data[3];
		if (!data) data = _data;

		data[0] = bhi.dwVolumeSerialNumber;
		data[1] = bhi.nFileIndexHigh;
		data[2] = bhi.nFileIndexLow;

		UINT	hash_id = hardLinkList.MakeHashId(data);
		LinkObj	*obj;

		if (!(obj = (LinkObj *)hardLinkList.Search(data, hash_id))) {
			if ((obj = new LinkObj(MakeAddr(path, srcBaseLen), bhi.nNumberOfLinks, data, len))) {
				hardLinkList.Register(obj, hash_id);
//				DBGWriteW(L"CheckHardLink %08x.%08x.%08x register (%s) %d\n", data[0], data[1],
//				data[2], MakeAddr(path, srcBaseLen), bhi.nNumberOfLinks);
				ret = LINK_REGISTER;
			}
			else {
				ConfirmErr("Can't malloc(CheckHardLink)", path, CEF_STOP);
			}
		}
		else {
			ret = LINK_ALREADY;
//			DBGWriteW(L"CheckHardLink %08x.%08x.%08x already %s (%s) %d\n", data[0], data[1],
//			data[2], MakeAddr(path, srcBaseLen), obj->path, bhi.nNumberOfLinks);
		}
	}
	else {
		ret = LINK_NONE;
	}

	if (hFileOrg == INVALID_HANDLE_VALUE) {
		::CloseHandle(hFile);
	}
	return	ret;
}

BOOL FastCopy::ReadProc(int dir_len, BOOL confirm_dir)
{
	BOOL		ret = TRUE;
	FileStat	*srcStat, *statEnd;
	FileStat	*dstStat = NULL;
	int			new_dir_len, curDirStatSize;
	int			confirm_len = dir_len + (dstBaseLen - srcBaseLen);
	BOOL		is_rename_local = isRename;
	BOOL		confirm_dir_local = confirm_dir || is_rename_local;

	isRename = FALSE;	// top level のみ効果を出す

	if (waitTick) Wait(1);

	// カレントのサイズを保存
	curDirStatSize = dirStatBuf.UsedSize();

	if (confirm_dir_local && !isSameDrv) DstRequest(DSTREQ_READSTAT);

	// ディレクトリエントリを先にすべて読み取る
	ret = ReadDirEntry(dir_len, confirm_dir_local);

	if (confirm_dir_local && (isSameDrv ? ReadDstStat() : WaitDstRequest()) == FALSE
	|| isAbort || !ret)
		return	FALSE;

	// ファイルを先に処理
	statEnd = (FileStat *)(fileStatBuf.Buf() + fileStatBuf.UsedSize());
	for (srcStat = (FileStat *)fileStatBuf.Buf(); srcStat < statEnd;
			srcStat = (FileStat *)((BYTE *)srcStat + srcStat->size)) {
		if (confirm_dir_local) {
			dstStat = hash.Search(srcStat->upperName, srcStat->hashVal);
		}
		int		path_len = 0;
		srcStat->fileID = nextFileID++;

		if (info.mode == MOVE_MODE || (info.flags & RESTORE_HARDLINK)) {
			path_len = dir_len + sprintfV(MakeAddr(src, dir_len), FMT_STR_V, srcStat->cFileName)
								+ 1;
		}

		if (dstStat) {
			if (is_rename_local) {
				SetRenameCount(srcStat);
			}
			else {
				dstStat->isExists = TRUE;
				srcStat->isCaseChanged = strcmpV(srcStat->cFileName, dstStat->cFileName);

				if (!IsOverWriteFile(srcStat, dstStat) &&
					(IsReparse(srcStat->dwFileAttributes) == IsReparse(dstStat->dwFileAttributes)
					|| (info.flags & FILE_REPARSE))) {
/* 比較モード */
					if (isListingOnly && (info.flags & VERIFY_FILE)) {
						strcpyV(MakeAddr(confirmDst, confirm_len), srcStat->cFileName);
						strcpyV(MakeAddr(src, dir_len), srcStat->cFileName);
						if (!IsSameContents(srcStat, dstStat) && !isAbort) {
//							PutList(MakeAddr(confirmDst, dstPrefixLen), PL_COMPARE|PL_NOADD);
						}
					}
/* 比較モード */

					if (info.mode == MOVE_MODE) {
						 PutMoveList(srcStat->fileID, src, path_len, srcStat->FileSize(),
						 	srcStat->dwFileAttributes, MoveObj::DONE);
					}
					total.skipFiles++;
					total.skipTrans += dstStat->FileSize();
					if (info.flags & RESTORE_HARDLINK) {
						CheckHardLink(src, path_len);
					}
					if (srcStat->isCaseChanged) {
						SendRequest(CASE_CHANGE, 0, srcStat);
					}
					continue;
				}
			}
		}
		total.readFiles++;

#define MOVE_OPEN_MAX 10
#define WAIT_OPEN_MAX 10

		if (!(ret = OpenFileProc(srcStat, dir_len)) == FALSE
			|| srcFsType == FSTYPE_NETWORK
			|| info.mode == MOVE_MODE && openFilesCnt >= MOVE_OPEN_MAX
			|| waitTick && openFilesCnt >= WAIT_OPEN_MAX
			|| openFilesCnt >= info.maxOpenFiles) {
			ReadMultiFilesProc(dir_len);
			CloseMultiFilesProc();
		}
		if (isAbort) break;
	}

	ReadMultiFilesProc(dir_len);
	CloseMultiFilesProc();
	if (isAbort)
		goto END;

	statEnd = (FileStat *)(dirStatBuf.Buf() + dirStatBuf.UsedSize());

	// ディレクトリの存在確認
	if (confirm_dir_local) {
		for (srcStat = (FileStat *)(dirStatBuf.Buf() + curDirStatSize); srcStat < statEnd;
				srcStat = (FileStat *)((BYTE *)srcStat + srcStat->size)) {
			if ((dstStat = hash.Search(srcStat->upperName, srcStat->hashVal)) != NULL) {
				srcStat->isCaseChanged = strcmpV(srcStat->cFileName, dstStat->cFileName);
				if (is_rename_local)
					SetRenameCount(srcStat);
				else
					srcStat->isExists = dstStat->isExists = TRUE;
			}
			else
				srcStat->isExists = FALSE;
		}
	}

	// SYNCモードの場合、コピー元に無いファイルを削除
	if (confirm_dir_local && info.mode == SYNCCP_MODE) {
		int		max = dstStatIdxBuf.UsedSize() / sizeof(FileStat *);
		for (int i=0; i < max; i++) {
			if ((dstStat = ((FileStat **)dstStatIdxBuf.Buf())[i])->isExists)
				continue;

			if (!FilterCheck(confirmDst, dstStat->cFileName, dstStat->dwFileAttributes,
					dstStat->WriteTime(), dstStat->FileSize())) {
				total.filterDstSkips++;
				continue;
			}

			if (isSameDrv) {
				if (IsDir(dstStat->dwFileAttributes))
					ret = DeleteDirProc(confirmDst, confirm_len, dstStat->cFileName, dstStat);
				else
					ret = DeleteFileProc(confirmDst, confirm_len, dstStat->cFileName, dstStat);
			}
			else {
				SendRequest(DELETE_FILES, 0, dstStat);
			}
			if (isAbort)
				goto END;
		}
	}

	// ディレクトリ処理
	for (srcStat = (FileStat *)(dirStatBuf.Buf() + curDirStatSize);
			srcStat < statEnd; srcStat = (FileStat *)((BYTE *)srcStat + srcStat->size)) {
		BOOL	is_reparse = IsReparse(srcStat->dwFileAttributes)
								&& (info.flags & DIR_REPARSE) == 0;
		int		cur_skips = total.filterSrcSkips;
		total.readDirs++;
		ReqBuf	req_buf = {0};
		new_dir_len = dir_len + sprintfV(MakeAddr(src, dir_len), FMT_STR_V, srcStat->cFileName);
		srcStat->fileID = nextFileID++;

		if (confirm_dir && srcStat->isExists)
			sprintfV(MakeAddr(confirmDst, confirm_len), FMT_CAT_ASTER_V, srcStat->cFileName);

		if (!isListingOnly) {
			if (enableAcl || is_reparse) {
				GetDirExtData(&req_buf, srcStat);
			}
			if (is_reparse && srcStat->rep == NULL && (!confirm_dir || !srcStat->isExists)) {
				continue;
			}
		}

		sprintfV(MakeAddr(src, new_dir_len), FMT_CAT_ASTER_V, L"");
		if (SendRequest(confirm_dir && srcStat->isExists ? INTODIR : MKDIR,
				req_buf.buf ? &req_buf : 0, srcStat), isAbort)
			goto END;

		if (!is_reparse) {
			if (ReadProc(new_dir_len + 1, confirm_dir && srcStat->isExists), isAbort)
				goto END;
			if (SendRequest(RETURN_PARENT), isAbort)
				goto END;
		}

		if (info.mode == MOVE_MODE) {
			SetChar(src, new_dir_len, 0);
			if (cur_skips == total.filterSrcSkips) {
				PutMoveList(srcStat->fileID, src, new_dir_len + 1, 0, srcStat->dwFileAttributes,
				/*srcStat->isExists ? MoveObj::DONE : MoveObj::START*/ MoveObj::DONE);
			}
		}
	}

END:
	// カレントの dir用Buf サイズを復元
	dirStatBuf.SetUsedSize(curDirStatSize);
	return	ret && !isAbort;
}

BOOL FastCopy::PutMoveList(_int64 fileID, void *path, int path_len, _int64 file_size, DWORD attr,
	MoveObj::Status status)
{
	int	path_size = path_len * CHAR_LEN_V;

	moveList.Lock();
	DataList::Head	*head = moveList.Alloc(NULL, 0, sizeof(MoveObj) + path_size);

	if (head) {
		MoveObj	*data = (MoveObj *)head->data;
		data->fileID = fileID;
		data->fileSize = file_size;
		data->dwFileAttributes = attr;
		data->status = status;
		memcpy(data->path, path, path_size);
	}
	if (moveList.IsWait()) {
		moveList.Notify();
	}
	moveList.UnLock();

	if (!head) {
		ConfirmErr("Can't alloc memory(moveList)", NULL, CEF_STOP);
	}
	else {
		FlushMoveList(FALSE);
	}

	return	head ? TRUE : FALSE;
}

BOOL FastCopy::FlushMoveList(BOOL is_finish)
{
	BOOL	is_nowait = FALSE;
	BOOL	is_nextexit = FALSE;
	BOOL	require_sleep = FALSE;

	if (!is_finish) {
		if (moveList.RemainSize() > moveList.MinMargin()) {	// Lock不要
			if ((info.flags & SERIAL_MOVE) == 0) {
				return	TRUE;
			}
			is_nowait = TRUE;
		}
	}

	while (!isAbort && moveList.Num() > 0) {
		DWORD			done_cnt = 0;
		DataList::Head	*head;

		moveList.Lock();
		if (require_sleep) moveList.Wait(CV_WAIT_TICK);

		while ((head = moveList.Fetch()) && !isAbort) {
			MoveObj	*data = (MoveObj *)head->data;

			if (data->status == MoveObj::START) {
				break;
			}
			if (data->status == MoveObj::DONE) {
				int 	prefix_len = IS_WINNT_V ? GetChar(data->path, 5) == ':' ?
										PATH_LOCAL_PREFIX_LEN : PATH_UNC_PREFIX_LEN : 0;
				BOOL	is_reparse = IsReparse(data->dwFileAttributes);

				moveList.UnLock();

				if (!isListingOnly && (data->dwFileAttributes & FILE_ATTRIBUTE_READONLY)) {
					SetFileAttributesV(data->path,
						data->dwFileAttributes & ~FILE_ATTRIBUTE_READONLY);
				}
				if (data->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
					if (!isListingOnly && RemoveDirectoryV(data->path) == FALSE) {
						total.errDirs++;
						ConfirmErr("RemoveDirectory(move)", MakeAddr(data->path, prefix_len));
					}
					else {
						total.deleteDirs++;
						if (isListing) {
							PutList(MakeAddr(data->path, prefix_len),
								PL_DIRECTORY|PL_DELETE|(is_reparse ? PL_REPARSE : 0));
						}
					}
				}
				else {
					if (!isListingOnly && DeleteFileV(data->path) == FALSE) {
						total.errFiles++;
						ConfirmErr("DeleteFile(move)", MakeAddr(data->path, prefix_len));
					}
					else {
						total.deleteFiles++;
						total.deleteTrans += data->fileSize;
						if (isListing) {
							PutList(MakeAddr(data->path, prefix_len),
								PL_DELETE|(is_reparse ? PL_REPARSE : 0));
						}
					}
				}
				done_cnt++;

				if (waitTick) Wait((waitTick + 9) / 10);

				moveList.Lock();
			}

			if (head == moveFinPtr) {
				moveFinPtr = NULL;
			}
			moveList.Get();
		}
		moveList.UnLock();

		if (moveList.Num() == 0 || is_nowait || isAbort
		|| !is_finish && moveList.RemainSize() > moveList.MinMargin()
				&& (!isSameDrv || is_nextexit)) {
			break;
		}

		cv.Lock();
		if ((info.flags & VERIFY_FILE) && runMode != RUN_DIGESTREQ) {
			runMode = RUN_DIGESTREQ;
			cv.Notify();
		}
		if (isSameDrv) {
			ChangeToWriteModeCore();
			is_nextexit = TRUE;
		}
		else {
			require_sleep = TRUE;
		}
		cv.UnLock();
	}

	return	!isAbort;
}

BOOL FastCopy::GetDirExtData(ReqBuf *req_buf, FileStat *stat)
{
	HANDLE	fh;
	WIN32_STREAM_ID	sid;
	DWORD	size, lowSeek, highSeek;
	void	*context = NULL;
	BYTE	streamName[MAX_PATH * sizeof(WCHAR)];
	BOOL	ret = TRUE;
	BOOL	is_reparse = IsReparse(stat->dwFileAttributes) && (info.flags & DIR_REPARSE) == 0;
	int		used_size_save = dirStatBuf.UsedSize();
	DWORD	share = FILE_SHARE_READ|FILE_SHARE_WRITE;
	DWORD	flg = FILE_FLAG_BACKUP_SEMANTICS | (is_reparse ? FILE_FLAG_OPEN_REPARSE_POINT : 0);

	if ((fh = CreateFileV(src, GENERIC_READ|READ_CONTROL, share, 0, OPEN_EXISTING, flg , 0))
			== INVALID_HANDLE_VALUE)
		return	FALSE;

	if (is_reparse) {
		size = MAXIMUM_REPARSE_DATA_BUFFER_SIZE;
		if (dirStatBuf.RemainSize() <= (int)size + maxStatSize
		&& dirStatBuf.Grow(ALIGN_SIZE(size + maxStatSize, MIN_ATTR_BUF)) == FALSE) {
			ret = FALSE;
			ConfirmErr("Can't alloc memory(dirStatBuf)", NULL, CEF_STOP);
		}
		else if ((size = ReadReparsePoint(fh, dirStatBuf.Buf() + dirStatBuf.UsedSize(), size))
				<= 0) {
			ret = FALSE;
			total.errDirs++;
			ConfirmErr("ReadReparsePoint(Dir)", MakeAddr(src, srcPrefixLen));
		}
		else {
			stat->rep = dirStatBuf.Buf() + dirStatBuf.UsedSize();
			stat->repSize = size;
			dirStatBuf.AddUsedSize(size);
		}
	}

	while (ret && enableAcl) {
		if (!(ret = ::BackupRead(fh, (LPBYTE)&sid, STRMID_OFFSET, &size, FALSE, TRUE, &context))
		|| size != 0 && size != STRMID_OFFSET) {
			if (info.flags & REPORT_ACL_ERROR)
				ConfirmErr("BackupRead(DIR)", MakeAddr(src, srcPrefixLen));
			break;
		}
		if (size == 0) break;	// 通常終了

		if (sid.dwStreamNameSize && !(ret = ::BackupRead(fh, streamName, sid.dwStreamNameSize,
				&size, FALSE, TRUE, &context))) {
			if (info.flags & REPORT_ACL_ERROR)
				ConfirmErr("BackupRead(name)", MakeAddr(src, srcPrefixLen));
			break;
		}
		if (sid.dwStreamId == BACKUP_SECURITY_DATA || sid.dwStreamId == BACKUP_EA_DATA) {
			BYTE    *&data = sid.dwStreamId==BACKUP_SECURITY_DATA ? stat->acl     : stat->ead;
			int &data_size = sid.dwStreamId==BACKUP_SECURITY_DATA ? stat->aclSize : stat->eadSize;

			if (data || sid.Size.HighPart) {	// すでに格納済み
				if (info.flags & REPORT_ACL_ERROR)
					ConfirmErr("Duplicate or Too big ACL/EADATA(dir)",
						MakeAddr(src, srcPrefixLen));
				break;
			}
			data = dirStatBuf.Buf() + dirStatBuf.UsedSize();
			data_size = sid.Size.LowPart + STRMID_OFFSET;
			dirStatBuf.AddUsedSize(data_size);
			if (dirStatBuf.RemainSize() <= maxStatSize
			&& !dirStatBuf.Grow(ALIGN_SIZE(maxStatSize + data_size, MIN_ATTR_BUF))) {
				ConfirmErr("Can't alloc memory(dirStat(ACL/EADATA))",
					MakeAddr(src, srcPrefixLen), FALSE);
				break;
			}
			memcpy(data, &sid, STRMID_OFFSET);
			if (!(ret = ::BackupRead(stat->hFile, data + STRMID_OFFSET, sid.Size.LowPart, &size,
						FALSE, TRUE, &context)) || size <= 0) {
				if (info.flags & REPORT_ACL_ERROR)
					ConfirmErr("BackupRead(DirACL/EADATA)", MakeAddr(src, srcPrefixLen));
				break;
			}
			sid.Size.LowPart = 0;
		}
		if ((sid.Size.LowPart || sid.Size.HighPart)
		&& !(ret = ::BackupSeek(fh, sid.Size.LowPart, sid.Size.HighPart, &lowSeek, &highSeek,
					&context))) {
			if (info.flags & REPORT_ACL_ERROR)
				ConfirmErr("BackupSeek(DIR)", MakeAddr(src, srcPrefixLen));
			break;
		}
	}

	if (enableAcl) {
		::BackupRead(fh, NULL, NULL, NULL, TRUE, TRUE, &context);
	}
	::CloseHandle(fh);

	if (ret && (size = stat->aclSize + stat->eadSize + stat->repSize) > 0) {
		if ((ret = PrepareReqBuf(offsetof(ReqHeader, stat) + stat->minSize, size, stat->fileID,
				req_buf))) {
			BYTE	*data = req_buf->buf;
			if (stat->acl) {
				memcpy(data, stat->acl, stat->aclSize);
				stat->acl = data;
				data += stat->aclSize;
			}
			if (stat->ead) {
				memcpy(data, stat->ead, stat->eadSize);
				stat->ead = data;
				data += stat->eadSize;
			}
			if (stat->rep) {
				memcpy(data, stat->rep, stat->repSize);
				stat->rep = data;
				data += stat->repSize;
			}
		}
	}

	dirStatBuf.SetUsedSize(used_size_save);

	return	ret && !isAbort;
}


int FastCopy::MakeRenameName(void *buf, int count, void *fname)
{
	void	*ext = strrchrV(fname, '.');
	int		body_limit = ext ? DiffLen(ext, fname) : MAX_PATH;

	return	sprintfV(buf, FMT_RENAME_V, body_limit, fname, count, ext ? ext : EMPTY_STR_V);
}

BOOL FastCopy::SetRenameCount(FileStat *stat)
{
	while (1) {
		WCHAR	new_name[MAX_PATH];
		int		len = MakeRenameName(new_name, ++stat->renameCount, stat->upperName) + 1;
		DWORD	hash_val = MakeHash(new_name, len * CHAR_LEN_V);
		if (hash.Search(new_name, hash_val) == NULL)
			break;
	}
	return	TRUE;
}

/*
	上書き判定
*/
BOOL FastCopy::IsOverWriteFile(FileStat *srcStat, FileStat *dstStat)
{
	if (info.overWrite == BY_NAME)
		return	FALSE;

	if (info.overWrite == BY_ATTR) {
		// サイズが等しく、かつ...
		if (dstStat->FileSize() == srcStat->FileSize()) {
			if (dstStat->WriteTime() == srcStat->WriteTime() &&		// 更新日付が完全に等しい
				((info.flags & COMPARE_CREATETIME) == 0
			|| dstStat->CreateTime() == srcStat->CreateTime())) {	// 作成日付が完全に等しい
				return	FALSE;
			}

			// どちらかが NTFS でない場合（ネットワークドライブを含む）
			if (srcFsType != FSTYPE_NTFS || dstFsType != FSTYPE_NTFS) {
				// 片方が NTFS でない場合、1msec 未満の誤差は許容した上で、比較（UDF 対策）
				if (dstStat->WriteTime() + 10000 >= srcStat->WriteTime() &&
					dstStat->WriteTime() - 10000 <= srcStat->WriteTime() &&
					((info.flags & COMPARE_CREATETIME) == 0
				||	dstStat->CreateTime() + 10000 >= srcStat->CreateTime() &&
					dstStat->CreateTime() - 10000 <= srcStat->CreateTime())) {
					return	FALSE;
				}
				// src か dst のタイムスタンプの最小単位が 1秒以上（FAT/SAMBA 等）かつ
				if ((srcStat->WriteTime() % 10000000) == 0
				||  (dstStat->WriteTime() % 10000000) == 0) {
					// タイムスタンプの差が 2 秒以内なら、
					// 同一タイムスタンプとみなして、上書きしない
					if (dstStat->WriteTime() + 20000000 >= srcStat->WriteTime() &&
						dstStat->WriteTime() - 20000000 <= srcStat->WriteTime() &&
						((info.flags & COMPARE_CREATETIME) == 0
					||	dstStat->CreateTime() + 10000000 >= srcStat->CreateTime() &&
						dstStat->CreateTime() - 10000000 <= srcStat->CreateTime()))
						return	FALSE;
				}
			}
		}
		return	TRUE;
	}

	if (info.overWrite == BY_LASTEST) {
		// 更新日付が dst と同じか古い場合は更新しない
		if (dstStat->WriteTime() >= srcStat->WriteTime() &&
			((info.flags & COMPARE_CREATETIME) == 0
		||	dstStat->CreateTime()  >= srcStat->CreateTime())) {
			return	FALSE;
		}

		// どちらかが NTFS でない場合（ネットワークドライブを含む）
		if (srcFsType != FSTYPE_NTFS || dstFsType != FSTYPE_NTFS) {
			// 片方が NTFS でない場合、1msec 未満の誤差は許容した上で、比較（UDF 対策）
			if (dstStat->WriteTime() + 10000 >= srcStat->WriteTime() &&
				((info.flags & COMPARE_CREATETIME) == 0 ||
				dstStat->CreateTime()  + 10000 >= srcStat->CreateTime())) {
				return	FALSE;
			}
			// src か dst のタイムスタンプの最小単位が 1秒以上（FAT/SAMBA 等）かつ
			if ((srcStat->WriteTime() % 10000000) == 0
			||	(dstStat->WriteTime() % 10000000) == 0) {
				// タイムスタンプの差に 2 秒のマージンを付けた上で、 
				// 更新日付が dst と同じか古い場合は、上書きしない
				if (dstStat->WriteTime() + 20000000 >= srcStat->WriteTime() &&
					((info.flags & COMPARE_CREATETIME) == 0
				||	dstStat->CreateTime() + 20000000 >= srcStat->CreateTime()))
					return	FALSE;
			}
		}
		return	TRUE;
	}

	if (info.overWrite == BY_ALWAYS)
		return	TRUE;

	return	ConfirmErr("Illegal overwrite mode", 0, CEF_STOP|CEF_NOAPI), FALSE;
}

BOOL FastCopy::ReadDirEntry(int dir_len, BOOL confirm_dir)
{
	HANDLE	fh;
	BOOL	ret = TRUE;
	int		len;
	WIN32_FIND_DATAW fdat;

	fileStatBuf.SetUsedSize(0);

	if ((fh = FindFirstFileV(src, &fdat)) == INVALID_HANDLE_VALUE) {
		total.errDirs++;
		return	ConfirmErr("FindFirstFile", MakeAddr(src, srcPrefixLen)), FALSE;
	}
	do {
		if (IsParentOrSelfDirs(fdat.cFileName))
			continue;

		// src ディレクトリ自体に対しては、フィルタ対象にしない
		if ((dir_len != srcBaseLen || isMetaSrc || !IsDir(fdat.dwFileAttributes))
		&& !FilterCheck(src, fdat.cFileName, fdat.dwFileAttributes, WriteTime(fdat),
				FileSize(fdat))) {
			total.filterSrcSkips++;
			continue;
		}

		// ディレクトリ＆ファイル情報の蓄積
		if (IsDir(fdat.dwFileAttributes)) {
			len = FdatToFileStat(&fdat, (FileStat *)(dirStatBuf.Buf() + dirStatBuf.UsedSize()),
					confirm_dir);
			dirStatBuf.AddUsedSize(len);
			if (dirStatBuf.RemainSize() <= maxStatSize && !dirStatBuf.Grow(MIN_ATTR_BUF)) {
				ConfirmErr("Can't alloc memory(dirStatBuf)", NULL, CEF_STOP);
				break;
			}
		}
		else {
			len = FdatToFileStat(&fdat, (FileStat *)(fileStatBuf.Buf() + fileStatBuf.UsedSize()),
					 confirm_dir);
			fileStatBuf.AddUsedSize(len);
			if (fileStatBuf.RemainSize() <= maxStatSize && !fileStatBuf.Grow(MIN_ATTR_BUF)) {
				ConfirmErr("Can't alloc memory(fileStatBuf)", NULL, CEF_STOP);
				break;
			}
		}
	}
	while (!isAbort && FindNextFileV(fh, &fdat));

	if (!isAbort && ::GetLastError() != ERROR_NO_MORE_FILES) {
		total.errDirs++;
		ret = FALSE;
		ConfirmErr("FindNextFile", MakeAddr(src, srcPrefixLen));
	}
	::FindClose(fh);

	return	ret && !isAbort;
}

BOOL FastCopy::OpenFileProc(FileStat *stat, int dir_len)
{
	DWORD	name_len = strlenV(stat->cFileName);
	memcpy(MakeAddr(src, dir_len), stat->cFileName, ((name_len + 1) * CHAR_LEN_V));

	openFiles[openFilesCnt++] = stat;

	if (waitTick) Wait((waitTick + 9) / 10);

	if (isListingOnly)	return	TRUE;

	BOOL	is_backup = enableAcl || enableStream;
	BOOL	is_reparse = IsReparse(stat->dwFileAttributes) && (info.flags & FILE_REPARSE) == 0;
	BOOL	is_open = is_backup || is_reparse || stat->FileSize() > 0;
	BOOL	ret = TRUE;

	if (is_open) {
		DWORD	mode = GENERIC_READ;
		DWORD	flg = ((info.flags & USE_OSCACHE_READ) ?
						0 : FILE_FLAG_NO_BUFFERING) | FILE_FLAG_SEQUENTIAL_SCAN;
		DWORD	share = FILE_SHARE_READ|FILE_SHARE_WRITE;

		if (is_backup) {
			mode |= READ_CONTROL;
			flg  |= FILE_FLAG_BACKUP_SEMANTICS;
		}

		if (is_reparse) {
			flg  |= FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT;
		}

		if ((stat->hFile = CreateFileV(src, mode, share, 0, OPEN_EXISTING, flg, 0))
				== INVALID_HANDLE_VALUE) {
			stat->lastError = ::GetLastError();
			ret = FALSE;
		}
	}

	if (ret && is_backup) {
		ret = OpenFileBackupProc(stat, dir_len + name_len);
	}

	return	ret;

}

BOOL FastCopy::OpenFileBackupProc(FileStat *stat, int src_len)
{
	WIN32_STREAM_ID	sid;
	BOOL	ret = FALSE;
	DWORD	size = 0;
	DWORD	lowSeek, highSeek;
	void	*context = NULL;
	int		altdata_cnt = 0;
	BOOL	altdata_local = FALSE;
	BYTE	streamName[MAX_PATH * sizeof(WCHAR)];
	DWORD	share = FILE_SHARE_READ|FILE_SHARE_WRITE;
	DWORD	flg = ((info.flags & USE_OSCACHE_READ) ? 0 :
				FILE_FLAG_NO_BUFFERING) | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_BACKUP_SEMANTICS;

	while (1) {
		if (!(ret = ::BackupRead(stat->hFile, (LPBYTE)&sid, STRMID_OFFSET, &size, FALSE, TRUE,
				&context)) || size != 0 && size != STRMID_OFFSET) {
			if (info.flags & (REPORT_ACL_ERROR|REPORT_STREAM_ERROR))
				ConfirmErr("BackupRead(head)", MakeAddr(src, srcPrefixLen));
			break;
		}
		if (size == 0) break;

		if (sid.dwStreamNameSize) {
			if (!(ret = ::BackupRead(stat->hFile, streamName, sid.dwStreamNameSize, &size,
					FALSE, TRUE, &context))) {
				if (info.flags & REPORT_STREAM_ERROR)
					ConfirmErr("BackupRead(name)", MakeAddr(src, srcPrefixLen));
				break;
			}
			// terminate されないため（dwStreamNameSize はバイト数）
			lSetCharV((LPBYTE)streamName + sid.dwStreamNameSize, 0, 0);
		}

		if (sid.dwStreamId == BACKUP_ALTERNATE_DATA && enableStream && !altdata_local) {
			ret = OpenFileBackupStreamCore(src_len, &sid.Size, streamName,
											sid.dwStreamNameSize, &altdata_cnt);
		}
		else if (sid.dwStreamId == BACKUP_SPARSE_BLOCK) {
			if (altdata_cnt == 0 && enableStream) {
				altdata_local = ret = OpenFileBackupStreamLocal(stat, src_len, &altdata_cnt);
			}
			break; // BACKUP_SPARSE_BLOCK can't be used BackupSeek...
		}
		else if ((sid.dwStreamId == BACKUP_SECURITY_DATA || sid.dwStreamId == BACKUP_EA_DATA)
		&& enableAcl) {
			BYTE	*&data = sid.dwStreamId==BACKUP_SECURITY_DATA ? stat->acl     : stat->ead;
			int &data_size = sid.dwStreamId==BACKUP_SECURITY_DATA ? stat->aclSize : stat->eadSize;
			if (data || sid.Size.HighPart) {	// すでに格納済み
				if (info.flags & REPORT_ACL_ERROR)
					ConfirmErr("Duplicate or Too big ACL/EADATA", MakeAddr(src, srcPrefixLen));
				break;
			}
			data = fileStatBuf.Buf() + fileStatBuf.UsedSize();
			data_size = sid.Size.LowPart + STRMID_OFFSET;
			fileStatBuf.AddUsedSize(data_size);
			if (fileStatBuf.RemainSize() <= maxStatSize
			&& !fileStatBuf.Grow(ALIGN_SIZE(maxStatSize + data_size, MIN_ATTR_BUF))) {
				ConfirmErr("Can't alloc memory(fileStat(ACL/EADATA))",
					MakeAddr(src, srcPrefixLen), CEF_STOP);
				break;
			}
			memcpy(data, &sid, STRMID_OFFSET);
			if (!(ret = ::BackupRead(stat->hFile, data + STRMID_OFFSET, sid.Size.LowPart,
							&size, FALSE, TRUE, &context)) || size <= 0) {
				if (info.flags & REPORT_ACL_ERROR)
					ConfirmErr("BackupRead(ACL/EADATA)", MakeAddr(src, srcPrefixLen));
				break;
			}
			sid.Size.LowPart = 0;
		}

		if ((sid.Size.LowPart || sid.Size.HighPart)
		&& !(ret = ::BackupSeek(stat->hFile, sid.Size.LowPart, sid.Size.HighPart,
					&lowSeek, &highSeek, &context))) {
			if (info.flags & (REPORT_ACL_ERROR|REPORT_STREAM_ERROR))
				ConfirmErr("BackupSeek", MakeAddr(src, srcPrefixLen));
			break;
		}
	}
	::BackupRead(stat->hFile, 0, 0, 0, TRUE, FALSE, &context);

	if (ret)
		total.readAclStream++;
	else
		total.errAclStream++;

	return	ret;
}


BOOL FastCopy::OpenFileBackupStreamLocal(FileStat *stat, int src_len, int *altdata_cnt)
{
	IO_STATUS_BLOCK	is;

	if (pNtQueryInformationFile(stat->hFile, &is, ntQueryBuf.Buf(),
								ntQueryBuf.Size(), FileStreamInformation) < 0) {
	 	if (info.flags & (REPORT_STREAM_ERROR))
			ConfirmErr("NtQueryInformationFile", MakeAddr(src, srcPrefixLen));
		return	FALSE;
	}

	BOOL	ret = TRUE;
	FILE_STREAM_INFORMATION	*fsi, *next_fsi;

	for (fsi = (FILE_STREAM_INFORMATION *)ntQueryBuf.Buf(); fsi; fsi = next_fsi) {
		next_fsi = fsi->NextEntryOffset == 0 ? NULL :
					(FILE_STREAM_INFORMATION *)((BYTE *)fsi + fsi->NextEntryOffset);

		if (fsi->StreamName[1] == ':') continue; // skip main stream

		if (!OpenFileBackupStreamCore(src_len, &fsi->StreamSize, fsi->StreamName,
									fsi->StreamNameLength, altdata_cnt)) {
			ret = FALSE;
			break;
		}
	}
	return	ret;
}

BOOL FastCopy::OpenFileBackupStreamCore(int src_len, LARGE_INTEGER *size, void *altname, int altnamesize, int *altdata_cnt)
{
	if (++(*altdata_cnt) >= MAX_ALTSTREAM) {
		if (info.flags & REPORT_STREAM_ERROR)
			ConfirmErr("Too Many AltStream", MakeAddr(src, srcPrefixLen), CEF_NOAPI);
		return	FALSE;
	}

	FileStat	*subStat = (FileStat *)(fileStatBuf.Buf() + fileStatBuf.UsedSize());
	DWORD		share = FILE_SHARE_READ|FILE_SHARE_WRITE;
	DWORD		flg = ((info.flags & USE_OSCACHE_READ) ? 0 : FILE_FLAG_NO_BUFFERING)
						| FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_BACKUP_SEMANTICS;

	openFiles[openFilesCnt++] = subStat;
	subStat->fileID = nextFileID++;
	subStat->hFile = INVALID_HANDLE_VALUE;
	subStat->nFileSizeLow = size->LowPart;
	subStat->nFileSizeHigh = size->HighPart;
	subStat->dwFileAttributes = 0;	// ALTSTREAM
	subStat->renameCount = 0;
	subStat->lastError = 0;
	subStat->size = altnamesize + CHAR_LEN_V + offsetof(FileStat, cFileName);
	subStat->minSize = ALIGN_SIZE(subStat->size, 8);

	fileStatBuf.AddUsedSize(subStat->minSize);
	if (fileStatBuf.RemainSize() <= maxStatSize && !fileStatBuf.Grow(MIN_ATTR_BUF)) {
		ConfirmErr("Can't alloc memory(fileStatBuf2)", NULL, CEF_STOP);
		return	FALSE;
	}
	memcpy(subStat->cFileName, altname, altnamesize + CHAR_LEN_V);
	memcpy(MakeAddr(src, src_len), subStat->cFileName, altnamesize + CHAR_LEN_V);

	if ((subStat->hFile = CreateFileV(src, GENERIC_READ|READ_CONTROL, share, 0,
							OPEN_EXISTING, flg, 0)) == INVALID_HANDLE_VALUE) {
		if (info.flags & REPORT_STREAM_ERROR)
			ConfirmErr("OpenFile(Stream)", MakeAddr(src, srcPrefixLen));
		subStat->lastError = ::GetLastError();
		return	FALSE;
	}
	return	TRUE;
}


BOOL FastCopy::ReadMultiFilesProc(int dir_len)
{
	for (int i=0; !isAbort && i < openFilesCnt; ) {
		ReadFileProc(i, &i, dir_len);
	}
	return	!isAbort;
}

BOOL FastCopy::CloseMultiFilesProc(int maxCnt)
{
	if (maxCnt == 0) {
		maxCnt = openFilesCnt;
		openFilesCnt = 0;
	}
	if (!isListingOnly) {
		for (int i=0; i < maxCnt; i++) {
			if (openFiles[i]->hFile != INVALID_HANDLE_VALUE) {
				::CloseHandle(openFiles[i]->hFile);
				openFiles[i]->hFile = INVALID_HANDLE_VALUE;
			}
		}
	}

	return	!isAbort;
}

void *FastCopy::RestoreOpenFilePath(void *path, int idx, int dir_len)
{
	FileStat	*stat = openFiles[idx];
	BOOL		is_stream = stat->dwFileAttributes ? FALSE : TRUE;

	if (is_stream) {
		int	i;
		for (i=idx-1; i >= 0; i--) {
			if (openFiles[i]->dwFileAttributes) {
				dir_len += sprintfV(MakeAddr(path, dir_len), FMT_STR_V, openFiles[i]->cFileName);
				break;
			}
		}
		if (i < 0) {
			ConfirmErr("RestoreOpenFilePath", MakeAddr(path, srcPrefixLen),
					CEF_STOP|CEF_NOAPI);
		}
	}
	sprintfV(MakeAddr(path, dir_len), FMT_STR_V, stat->cFileName);
	return	path;
}

BOOL FastCopy::ReadFileWithReduce(HANDLE hFile, void *buf, DWORD size, DWORD *reads, OVERLAPPED *overwrap)
{
	DWORD	trans = 0;
	DWORD	total = 0;
	DWORD	maxReadSizeSv = maxReadSize;

	while ((trans = size - total) > 0) {
		DWORD	transed = 0;
		trans = min(trans, maxReadSize);
		if (!::ReadFile(hFile, (BYTE *)buf + total, trans, &transed, overwrap)) {
			if (::GetLastError() != ERROR_NO_SYSTEM_RESOURCES
			|| min(size, maxReadSize) <= REDUCE_SIZE) {
				return	FALSE;
			}
			maxReadSize -= REDUCE_SIZE;
			maxReadSize = ALIGN_SIZE(maxReadSize, REDUCE_SIZE);
			continue;
		}
		total += transed;
		if (transed != trans) {
			break;
		}
	}
	*reads = total;

	if (maxReadSize != maxReadSizeSv) {
		WCHAR buf[128];
		sprintfV(buf, FMT_REDUCEMSG_V, 'R', maxReadSizeSv / 1024/1024, maxReadSize / 1024/1024);
		WriteErrLog(buf);
	}

	return	TRUE;
}

BOOL FastCopy::ReadFileProc(int start_idx, int *end_idx, int dir_len)
{
	BOOL		ret = TRUE, ext_ret = TRUE;
	FileStat	*stat = openFiles[start_idx];
	_int64		file_size = stat->FileSize();
	DWORD		trans_size;
	ReqBuf		req_buf;
	Command		command = enableAcl || enableStream ? stat->dwFileAttributes ?
							WRITE_BACKUP_FILE : WRITE_BACKUP_ALTSTREAM : WRITE_FILE;
	BOOL		is_stream = command == WRITE_BACKUP_ALTSTREAM;
	BOOL		is_reparse = IsReparse(stat->dwFileAttributes) && (info.flags & FILE_REPARSE) ==0;
	BOOL		is_send_request = FALSE;
	int			&totalErrFiles = is_stream ? total.errAclStream   : total.errFiles;
	_int64		&totalErrTrans = is_stream ? total.errStreamTrans : total.errTrans;
	BOOL		is_digest = IsUsingDigestList() && !is_stream && !is_reparse;

	*end_idx = start_idx + 1;

//	ReadFile で対処
//	if (!is_reparse && IsReparse(stat->dwFileAttributes)) {		// reparse先をコピー
//		srcSectorSize = max(srcSectorSize, OPT_SECTOR_SIZE);
//		sectorSize = max(srcSectorSize, dstSectorSize);
//	}

	if ((info.flags & RESTORE_HARDLINK) && !is_stream) {	// include listing only
		if (CheckHardLink(RestoreOpenFilePath(src, start_idx, dir_len), -1, stat->hFile,
				stat->GetLinkData()) == LINK_ALREADY) {
			stat->SetFileSize(0);
			ret = SendRequest(CREATE_HARDLINK, 0, stat);
			goto END;
		}
	}

	if ((file_size == 0 && !is_reparse) || isListingOnly) {
		ret = SendRequest(command, 0, stat);
		is_send_request = TRUE;
		if (command != WRITE_BACKUP_FILE || isListingOnly) {
			goto END;
		}
	}

	if (stat->hFile == INVALID_HANDLE_VALUE) {
		if (!is_stream) {
			::SetLastError(stat->lastError);
			totalErrFiles++;
			totalErrTrans += file_size;
		}
		stat->SetFileSize(0);
		if (is_stream && !(info.flags & REPORT_STREAM_ERROR)
		|| ConfirmErr(is_stream ? "OpenFile(Stream)" : "OpenFile",
				MakeAddr(RestoreOpenFilePath(src, start_idx, dir_len), srcPrefixLen))
				== Confirm::CONTINUE_RESULT) {
			if ((info.flags & CREATE_OPENERR_FILE) && !is_send_request) {
				SendRequest(command, 0, stat);
				is_send_request = TRUE;
			}
		}
		if (is_send_request && command == WRITE_BACKUP_FILE) {
			SendRequest(WRITE_BACKUP_END, 0, 0);
		}
		ret = FALSE;
		goto END;
	}

	if (is_reparse) {
		BYTE	rd[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
		if ((stat->repSize = ReadReparsePoint(stat->hFile, rd, sizeof(rd))) <= 0
		|| PrepareReqBuf(offsetof(ReqHeader, stat) + stat->minSize, stat->repSize,
				stat->fileID, &req_buf) == FALSE) {
			ConfirmErr("ReadReparsePoint(File)",
				MakeAddr(RestoreOpenFilePath(src, start_idx, dir_len), srcPrefixLen));
			totalErrFiles++;
			totalErrTrans += file_size;
			ret = FALSE;
			goto END;
		}
		memcpy(req_buf.buf, rd, stat->repSize);
		SendRequest(command, &req_buf, stat);
		is_send_request = TRUE;
	}
	else {
		::SetFilePointer(stat->hFile, 0, NULL, FILE_BEGIN);
		BOOL	prepare_ret = TRUE;

		for (_int64 remain_size=file_size; remain_size > 0; remain_size -= trans_size) {
			trans_size = 0;
			while (1) {
				if ((ret = PrepareReqBuf(offsetof(ReqHeader, stat) +
							(is_send_request && !is_digest ? 0 : stat->minSize), remain_size,
							stat->fileID, &req_buf)) == FALSE) {
					prepare_ret = FALSE;
					break;
				}
				if ((ret = ReadFileWithReduce(stat->hFile, req_buf.buf, req_buf.bufSize,
						&trans_size, NULL)))
					break;
				if (::GetLastError() == ERROR_INVALID_PARAMETER || sectorSize < OPT_SECTOR_SIZE) {						// reparse point で別 volume に移動した場合用
					srcSectorSize = max(srcSectorSize, OPT_SECTOR_SIZE);
					sectorSize = max(srcSectorSize, dstSectorSize);
				}
				else break;
			}
			if (!ret || trans_size != (DWORD)req_buf.bufSize && trans_size < remain_size) {
				ret = FALSE;
				totalErrFiles++;
				totalErrTrans += file_size;
				if (is_stream && !(info.flags & REPORT_STREAM_ERROR)
				|| !prepare_ret /* dst でのエラー発生が原因の場合 */
				|| ConfirmErr(is_stream ? "ReadFile(stream)" : ret && !trans_size ?
						"ReadFile(truncate)" : "ReadFile",
						MakeAddr(RestoreOpenFilePath(src, start_idx, dir_len), srcPrefixLen))
						== Confirm::CONTINUE_RESULT) {
					stat->SetFileSize(0);
					req_buf.bufSize = 0;
					if ((info.flags & CREATE_OPENERR_FILE) || is_send_request) {
						SendRequest(is_send_request ? WRITE_ABORT : command, &req_buf,
							is_send_request && !is_digest ? 0 : stat);
						is_send_request = TRUE;
					}
				}
				break;
			}

			// 逐次移動チェック（並列処理時のみ）
			if (!isSameDrv && info.mode == MOVE_MODE && remain_size > trans_size) {
				FlushMoveList(FALSE);
			}

			total.readTrans += trans_size;
			SendRequest(is_send_request ? WRITE_FILE_CONT : command, &req_buf,
				is_send_request && !is_digest ? 0 : stat);
			is_send_request = TRUE;
			if (waitTick && remain_size > trans_size) Wait();
		}
	}

	if (command == WRITE_BACKUP_FILE) {
		while (ret && ext_ret && !isAbort && (*end_idx) < openFilesCnt
		&& openFiles[(*end_idx)]->dwFileAttributes == 0) {
			ext_ret = ReadFileProc(*end_idx, end_idx, dir_len);
		}
		if (ret && ext_ret && stat->acl) {
			if ((ext_ret = PrepareReqBuf(offsetof(ReqHeader, stat) + stat->minSize,
					stat->aclSize, stat->fileID, &req_buf))) {
				memcpy(req_buf.buf, stat->acl, stat->aclSize);
				ext_ret = SendRequest(WRITE_BACKUP_ACL, &req_buf, stat);
			}
		}
		if (ret && ext_ret && stat->ead) {
			if ((ext_ret = PrepareReqBuf(offsetof(ReqHeader, stat) + stat->minSize,
					stat->eadSize, stat->fileID, &req_buf))) {
				memcpy(req_buf.buf, stat->ead, stat->eadSize);
				ext_ret = SendRequest(WRITE_BACKUP_EADATA, &req_buf, stat);
			}
		}
		if (!isAbort && is_send_request) {
			SendRequest(WRITE_BACKUP_END, 0, 0);
		}
	}

END:
	if (ret && info.mode == MOVE_MODE && !is_stream) {
		CloseMultiFilesProc(*end_idx);
		int	path_len = dir_len + sprintfV(MakeAddr(src, dir_len), FMT_STR_V, stat->cFileName) + 1;
		PutMoveList(stat->fileID, src, path_len, file_size, stat->dwFileAttributes,
			MoveObj::START);
	}

	return	ret && ext_ret && !isAbort;
}

void FastCopy::DstRequest(DstReqKind kind)
{
	cv.Lock();
	dstAsyncRequest = kind;
	cv.Notify();
	cv.UnLock();
}

BOOL FastCopy::WaitDstRequest(void)
{
	cv.Lock();
	while (dstAsyncRequest != DSTREQ_NONE && !isAbort)
		cv.Wait(CV_WAIT_TICK);
	cv.UnLock();
	return	dstRequestResult;
}

BOOL FastCopy::CheckDstRequest(void)
{
	if (isAbort || dstAsyncRequest == DSTREQ_NONE)	return FALSE;

	if (!isSameDrv)
		cv.UnLock();

	switch (dstAsyncRequest) {
	case DSTREQ_READSTAT:
		dstRequestResult = ReadDstStat();
		break;

	case DSTREQ_DIGEST:
		dstRequestResult = MakeDigest(confirmDst, &dstDigestBuf, &dstDigest, dstDigestVal);
		break;
	}

	if (!isSameDrv) {
		cv.Lock();
		cv.Notify();
	}
	dstAsyncRequest = DSTREQ_NONE;

	return	dstRequestResult;
}

BOOL FastCopy::ReadDstStat(void)
{
	HANDLE		fh;
	int			num = 0, len;
	FileStat	*dstStat, **dstStatIdx;
	WIN32_FIND_DATAW	fdat;
	BOOL		ret = TRUE;

	dstStat = (FileStat *)dstStatBuf.Buf();
	dstStatIdx = (FileStat **)dstStatIdxBuf.Buf();
	dstStatBuf.SetUsedSize(0);
	dstStatIdxBuf.SetUsedSize(0);

	if ((fh = FindFirstFileV(confirmDst, &fdat)) == INVALID_HANDLE_VALUE) {
		if (::GetLastError() != ERROR_FILE_NOT_FOUND
		&& strcmpV(MakeAddr(confirmDst, dstBaseLen), ASTERISK_V) == 0) {
			ret = FALSE;
			total.errDirs++;
			ConfirmErr("FindFirstFile(stat)", MakeAddr(confirmDst, dstPrefixLen));
		}	// ファイル名を指定してのコピーで、コピー先が見つからない場合は、
			// エントリなしでの成功とみなす
		goto END;
	}
	do {
		if (IsParentOrSelfDirs(fdat.cFileName))
			continue;
		dstStatIdx[num++] = dstStat;
		len = FdatToFileStat(&fdat, dstStat, TRUE);
		dstStatBuf.AddUsedSize(len);

		// 次の stat 用 buffer のセット
		dstStat = (FileStat *)(dstStatBuf.Buf() + dstStatBuf.UsedSize());
		dstStatIdxBuf.AddUsedSize(sizeof(FileStat *));

		if (dstStatBuf.RemainSize() <= maxStatSize && dstStatBuf.Grow(MIN_ATTR_BUF) == FALSE) {
			ConfirmErr("Can't alloc memory(dstStatBuf)", NULL, CEF_STOP);
			break;
		}
		if (dstStatIdxBuf.RemainSize() <= sizeof(FileStat *)
		&& dstStatIdxBuf.Grow(MIN_ATTRIDX_BUF) == FALSE) {
			ConfirmErr("Can't alloc memory(dstStatIdxBuf)", NULL, CEF_STOP);
			break;
		}
	}
	while (!isAbort && FindNextFileV(fh, &fdat));

	if (!isAbort && ::GetLastError() != ERROR_NO_MORE_FILES) {
		total.errFiles++;
		ret = FALSE;
		ConfirmErr("FindNextFile(stat)", MakeAddr(confirmDst, dstPrefixLen));
	}
	::FindClose(fh);

END:
	if (ret)
		ret = MakeHashTable();

	return	ret;
}

BOOL FastCopy::MakeHashTable(void)
{
	int		num = dstStatIdxBuf.UsedSize() / sizeof(FileStat *);
	int		require_size = hash.RequireSize(num), grow_size;

	if ((grow_size = require_size - dstStatIdxBuf.RemainSize()) > 0
	&& dstStatIdxBuf.Grow(ALIGN_SIZE(grow_size, MIN_ATTRIDX_BUF)) == FALSE) {
		ConfirmErr("Can't alloc memory(dstStatIdxBuf2)", NULL, CEF_STOP);
		return	FALSE;
	}

	return	hash.Init((FileStat **)dstStatIdxBuf.Buf(), num,
				dstStatIdxBuf.Buf() + dstStatIdxBuf.UsedSize());
}

int StatHash::HashNum(int data_num)
{
	return	data_num | 1;
}

BOOL StatHash::Init(FileStat **data, int data_num, void *tbl_buf)
{
	hashNum = HashNum(data_num);
	hashTbl = (FileStat **)tbl_buf;
	memset(hashTbl, 0, hashNum * sizeof(FileStat *));

	for (int i=0; i < data_num; i++) {
		FileStat **stat = hashTbl+(data[i]->hashVal % hashNum);
		while (*stat) {
			stat = &(*stat)->next;
		}
		*stat = data[i];
	}
	return	TRUE;
}

FileStat *StatHash::Search(void *upperName, DWORD hash_val)
{
	for (FileStat *target = hashTbl[hash_val % hashNum]; target; target = target->next) {
		if (target->hashVal == hash_val && strcmpV(target->upperName, upperName) == 0)
			return	target;
	}

	return	NULL;
}

/*=========================================================================
  関  数 ： DeleteThread
  概  要 ： DELETE_MODE 処理
  説  明 ： 
  注  意 ： 
=========================================================================*/
unsigned WINAPI FastCopy::DeleteThread(void *fastCopyObj)
{
	return	((FastCopy *)fastCopyObj)->DeleteThreadCore();
}

BOOL FastCopy::DeleteThreadCore(void)
{
	if ((info.flags & PRE_SEARCH) && info.mode == DELETE_MODE)
		PreSearch();

	for (int i=0; i < srcArray.Num() && !isAbort; i++) {
		if (InitDeletePath(i))
			DeleteProc(dst, dstBaseLen);
	}
	FinishNotify();
	return	TRUE;
}

/*
	削除処理
*/
BOOL FastCopy::DeleteProc(void *path, int dir_len)
{
	HANDLE		hDir;
	BOOL		ret = TRUE;
	FileStat	stat;
	WIN32_FIND_DATAW fdat;

	if ((hDir = FindFirstFileV(path, &fdat)) == INVALID_HANDLE_VALUE) {
		total.errDirs++;
		return	ConfirmErr("FindFirstFile(del)", MakeAddr(path, dstPrefixLen)), FALSE;
	}

	do {
		if (IsParentOrSelfDirs(fdat.cFileName))
			continue;

		if (waitTick) Wait((waitTick + 9) / 10);

		// 削除指定したディレクトリ自体（ルート）でなくかつ、フィルター除外対象
		if ((dstBaseLen != dir_len || isMetaSrc || !IsDir(stat.dwFileAttributes))
		&& !FilterCheck(path, fdat.cFileName, fdat.dwFileAttributes, WriteTime(fdat),
				FileSize(fdat))) {
			total.filterDelSkips++;
			continue;
		}

		stat.nFileSizeLow		= fdat.nFileSizeLow;
		stat.nFileSizeHigh		= fdat.nFileSizeHigh;
		stat.dwFileAttributes	= fdat.dwFileAttributes;

		if (IsDir(stat.dwFileAttributes))
			ret = DeleteDirProc(path, dir_len, fdat.cFileName, &stat);
		else
			ret = DeleteFileProc(path, dir_len, fdat.cFileName, &stat);
	}
	while (!isAbort && FindNextFileV(hDir, &fdat));

	if (!isAbort && ret && ::GetLastError() != ERROR_NO_MORE_FILES) {
		ret = FALSE;
		ConfirmErr("FindNextFile(del)", MakeAddr(path, dstPrefixLen));
	}

	::FindClose(hDir);

	return	ret && !isAbort;
}

BOOL FastCopy::DeleteDirProc(void *path, int dir_len, void *fname, FileStat *stat)
{
	int		new_dir_len = dir_len + sprintfV(MakeAddr(path, dir_len), FMT_CAT_ASTER_V, fname) -1;
	BOOL	ret = TRUE;
	int		cur_skips = total.filterDelSkips;
	BOOL	is_reparse = IsReparse(stat->dwFileAttributes);

	if (info.mode == DELETE_MODE && (info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA))) {
		memcpy(MakeAddr(confirmDst, dir_len), MakeAddr(path, dir_len),
			(new_dir_len - dir_len + 2) * CHAR_LEN_V);
	}

	if (!is_reparse) {
		ret = DeleteProc(path, new_dir_len);
		if (isAbort) return	ret;
	}

	if (cur_skips != total.filterDelSkips
	|| (filterMode && info.mode == DELETE_MODE && (info.flags & DELDIR_WITH_FILTER) == 0))
		return	ret;

	SetChar(path, new_dir_len - 1, 0);

	if (!isListingOnly) {
		if (!is_reparse) {
			SetFileAttributesV(path, stat->dwFileAttributes & ~FILE_ATTRIBUTE_READONLY);
		}
		void	*target = path;

		if (info.mode == DELETE_MODE && (info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA))) {
			if (RenameRandomFname(path, confirmDst, dir_len, new_dir_len-dir_len-1)) {
				target = confirmDst;
			}
		}

		if ((ret = RemoveDirectoryV(target)) == FALSE) {
			total.errDirs++;
			return	ConfirmErr("RemoveDirectory", MakeAddr(target, dstPrefixLen)), FALSE;
		}
	}
	if (isListing) {
		PutList(MakeAddr(path, dstPrefixLen), PL_DIRECTORY|PL_DELETE|(is_reparse ? PL_REPARSE:0));
	}
	total.deleteDirs++;
	return	ret;
}

BOOL FastCopy::DeleteFileProc(void *path, int dir_len, void *fname, FileStat *stat)
{
	int		len = sprintfV(MakeAddr(path, dir_len), FMT_STR_V, fname);
	BOOL	is_reparse = IsReparse(stat->dwFileAttributes);

	if (!isListingOnly) {
		if (stat->dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
			SetFileAttributesV(path, FILE_ATTRIBUTE_NORMAL);
		}
		void	*target = path;

		if (info.mode == DELETE_MODE && (info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA))
		&& !is_reparse) {
			if (RenameRandomFname(target, confirmDst, dir_len, len)) {
				target = confirmDst;
			}
			if (stat->FileSize()) {
				if (WriteRandomData(target, stat, TRUE) == FALSE) {
					total.errFiles++;
					return	ConfirmErr("OverWrite", MakeAddr(target, dstPrefixLen)), FALSE;
				}
				total.writeFiles++;
			}
		}

		if (DeleteFileV(target) == FALSE) {
			total.errFiles++;
			return	ConfirmErr("DeleteFile", MakeAddr(target, dstPrefixLen)), FALSE;
		}
	}
	if (isListing) PutList(MakeAddr(path, dstPrefixLen), PL_DELETE|(is_reparse ? PL_REPARSE : 0));

	total.deleteFiles++;
	total.deleteTrans += stat->FileSize();
	return	TRUE;
}


/*=========================================================================
  関  数 ： RDigestThread
  概  要 ： RDigestThread 処理
  説  明 ： 
  注  意 ： 
=========================================================================*/
unsigned WINAPI FastCopy::RDigestThread(void *fastCopyObj)
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
	return	((FastCopy *)fastCopyObj)->RDigestThreadCore();
}

BOOL FastCopy::RDigestThreadCore(void)
{
	_int64	fileID = 0;
	_int64	remain_size;

	cv.Lock();

	while (!isAbort) {
		while (rDigestReqList.IsEmpty() && !isAbort) {
			cv.Wait(CV_WAIT_TICK);
		}
		ReqHeader *req = rDigestReqList.TopObj();
		if (!req || isAbort) {
			break;
		}
		Command cmd = req->command;

		FileStat	*stat = &req->stat;
		if ((cmd == WRITE_FILE || cmd == WRITE_BACKUP_FILE
				|| (cmd == WRITE_FILE_CONT && fileID == stat->fileID))
			&& stat->FileSize() > 0 && (!IsReparse(stat->dwFileAttributes)
			|| (info.flags & FILE_REPARSE))) {
			cv.UnLock();
//			Sleep(0);
			if (fileID != stat->fileID) {
				fileID = stat->fileID;
				remain_size = stat->FileSize();
				srcDigest.Reset();
			}

			int		trans_size = (int)(req->bufSize < remain_size ? req->bufSize : remain_size);
			srcDigest.Update(req->buf, trans_size);

			if ((remain_size -= trans_size) <= 0) {
				srcDigest.GetVal(stat->digest);
				if (remain_size < 0) {
					ConfirmErr("Internal Error(digest)", NULL, CEF_STOP|CEF_NOAPI);
				}
			}
			cv.Lock();
		}
		rDigestReqList.DelObj(req);

		if (isSameDrv) {
			readReqList.AddObj(req);
//			if (rDigestReqList.IsEmpty()) {
				cv.Notify();
//			}
		}
		else {
			writeReqList.AddObj(req);
			cv.Notify();
		}
		if (cmd == REQ_EOF) {
			break;
		}
	}

	cv.UnLock();

	return	isAbort;
}

/*=========================================================================
  概  要 ： 上書き削除用ルーチン
=========================================================================*/
void FastCopy::SetupRandomDataBuf(void)
{
	RandomDataBuf	*data = (RandomDataBuf *)mainBuf.Buf();

	data->is_nsa = (info.flags & OVERWRITE_DELETE_NSA) ? TRUE : FALSE;
	data->base_size = max(PAGE_SIZE, dstSectorSize);
	data->buf_size = mainBuf.Size() - data->base_size;
	data->buf[0] = mainBuf.Buf() + data->base_size;

	if (data->is_nsa) {
		data->buf_size /= 3;
		data->buf_size = (data->buf_size / data->base_size) * data->base_size;
		data->buf_size = min(info.maxTransSize, data->buf_size);

		data->buf[1] = data->buf[0] + data->buf_size;
		data->buf[2] = data->buf[1] + data->buf_size;
		if (info.flags & OVERWRITE_PARANOIA) {
			if (!pCryptAcquireContext) {
				TLibInit_AdvAPI32();	// TGenRandom を使えるように
			}
			TGenRandom(data->buf[0], data->buf_size);	// CryptAPIのrandは遅い...
			TGenRandom(data->buf[1], data->buf_size);
		}
		else {
			for (int i=0, max=data->buf_size / sizeof(int) * 2; i < max; i++) {
				*((int *)data->buf[0] + i) = rand();
			}
		}
		memset(data->buf[2], 0, data->buf_size);
	}
	else {
		data->buf_size = min(info.maxTransSize, data->buf_size);
		if (info.flags & OVERWRITE_PARANOIA) {
			TGenRandom(data->buf[0], data->buf_size);	// CryptAPIのrandは遅い...
		}
		else {
			for (int i=0, max=data->buf_size / sizeof(int); i < max; i++) {
				*((int *)data->buf[0] + i) = rand();
			}
		}
	}
}

void FastCopy::GenRandomName(void *path, int fname_len, int ext_len)
{
	static char *char_dict = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

	for (int i=0; i < fname_len; i++) {
		SetChar(path, i, char_dict[(rand() >> 4) % 62]);
	}
	if (ext_len) {
		SetChar(path, fname_len - ext_len, '.');
	}
	SetChar(path, fname_len, 0);
}

BOOL FastCopy::RenameRandomFname(void *org_path, void *rename_path, int dir_len, int fname_len)
{
	void	*fname = MakeAddr(org_path, dir_len);
	void	*rename_fname = MakeAddr(rename_path, dir_len);
	void	*dot = strrchrV(fname, '.');
	int		ext_len = dot ? fname_len - DiffLen(dot, fname) : 0;

	for (int i=fname_len; i <= MAX_FNAME_LEN; i++) {
		for (int j=0; j < 128; j++) {
			GenRandomName(rename_fname, i, ext_len);
			if (MoveFileV(org_path, rename_path)) {
				return	TRUE;
			}
			else if (::GetLastError() != ERROR_ALREADY_EXISTS) {
				return	FALSE;
			}
		}
	}
	return	FALSE;
}

BOOL FastCopy::WriteRandomData(void *path, FileStat *stat, BOOL skip_hardlink)
{
	BOOL	is_nonbuf = dstFsType != FSTYPE_NETWORK
							&& (stat->FileSize() >= max(nbMinSize, PAGE_SIZE)
						|| (stat->FileSize() % dstSectorSize) == 0)
							&& (info.flags & USE_OSCACHE_WRITE) == 0 ? TRUE : FALSE;
	DWORD	flg = (is_nonbuf ? FILE_FLAG_NO_BUFFERING : 0) | FILE_FLAG_SEQUENTIAL_SCAN;
	DWORD	trans_size;
	DWORD	share = FILE_SHARE_READ|FILE_SHARE_WRITE;
	HANDLE	hFile = CreateFileV(path, GENERIC_WRITE, share, 0, OPEN_EXISTING, flg, 0);
	BOOL	ret = TRUE;

	if (hFile == INVALID_HANDLE_VALUE) {
		return	ConfirmErr("Write by Random Data(open)", MakeAddr(path, dstPrefixLen)), FALSE;
	}

	if (waitTick) Wait((waitTick + 9) / 10);

	BY_HANDLE_FILE_INFORMATION	bhi;

	if (!skip_hardlink || !::GetFileInformationByHandle(hFile, &bhi) || bhi.nNumberOfLinks <= 1) {
		RandomDataBuf	*data = (RandomDataBuf *)mainBuf.Buf();
		_int64			file_size = is_nonbuf ? ALIGN_SIZE(stat->FileSize(), dstSectorSize)
											:   stat->FileSize();

		for (int i=0, end=data->is_nsa ? 3 : 1; i < end && ret; i++) {
			::SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
			for (_int64 remain_size=file_size; remain_size > 0; remain_size -= trans_size) {
				int	max_trans = waitTick && (info.flags & AUTOSLOW_IOLIMIT) ?
								MIN_BUF : (int)maxWriteSize;
				max_trans = min(max_trans, data->buf_size);
				int	io_size = remain_size > max_trans ?
								max_trans : (int)ALIGN_SIZE(remain_size, dstSectorSize);

				if (!(ret = WrieFileWithReduce(hFile, data->buf[i], (DWORD)io_size,
						&trans_size, NULL))) {
					break;
				}
				total.writeTrans += trans_size;
				if (waitTick) Wait();
			}
			total.writeTrans -= file_size - stat->FileSize();
			::FlushFileBuffers(hFile);
		}

		::SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
		::SetEndOfFile(hFile);
	}

	::CloseHandle(hFile);
	return	ret;
}

/*=========================================================================
  関  数 ： WriteThread
  概  要 ： Write 処理
  説  明 ： 
  注  意 ： 
=========================================================================*/
unsigned WINAPI FastCopy::WriteThread(void *fastCopyObj)
{
	return	((FastCopy *)fastCopyObj)->WriteThreadCore();
}

BOOL FastCopy::WriteThreadCore(void)
{
	// トップレベルディレクトリが存在しない場合は作成
	CheckAndCreateDestDir(dstBaseLen);

	BOOL	ret = WriteProc(dstBaseLen);

	cv.Lock();
	writeReq = NULL;
	writeReqList.Init();
	cv.Notify();
	cv.UnLock();

	return	ret;
}

BOOL FastCopy::CheckAndCreateDestDir(int dst_len)
{
	BOOL	ret = TRUE;

	SetChar(dst, dst_len - 1, 0);

	if (GetFileAttributesV(dst) == 0xffffffff) {
		int	parent_dst_len = 0;

		if (IS_WINNT_V) {
			parent_dst_len = dst_len - 2;
			for ( ; parent_dst_len >= 9; parent_dst_len--) { // "\\?\c:\x\..."
				if (GetChar(dst, parent_dst_len -1) == '\\') break;
			}
			if (parent_dst_len < 9) parent_dst_len = 0;
		}
		else {
			const char *cur = (const char *)MakeAddr(dst, 0);;
			const char *end = (const char *)MakeAddr(dst, dst_len);

			while (cur < end) {
				if (lGetCharIncA(&cur) == '\\') {
					parent_dst_len = (int)(cur - (const char *)dst);
				}
			}
			if (parent_dst_len < 4) parent_dst_len = 0;
		}

		ret = parent_dst_len ? CheckAndCreateDestDir(parent_dst_len) : FALSE;

		if (isListingOnly || CreateDirectoryV(dst, NULL) && isListing) {
			PutList(MakeAddr(dst, dstPrefixLen), PL_DIRECTORY);
			total.writeDirs++;
		}
		else ret = FALSE;
	}
	SetChar(dst, dst_len - 1, '\\');

	return	ret;
}

BOOL FastCopy::FinishNotify(void)
{
	endTick = ::GetTickCount();
	return	::PostMessage(info.hNotifyWnd, info.uNotifyMsg, END_NOTIFY, 0);
}

BOOL FastCopy::WriteProc(int dir_len)
{
	BOOL	ret = TRUE;

	while (!isAbort && (ret = RecvRequest())) {
		if (writeReq->command == REQ_EOF) {
			if (IsUsingDigestList() && !isAbort) {
				CheckDigests(CD_FINISH);
			}
			break;
		}
		if (writeReq->command == WRITE_FILE || writeReq->command == WRITE_BACKUP_FILE
		|| writeReq->command == CREATE_HARDLINK) {
			int		new_dst_len;
			if (writeReq->stat.renameCount == 0)
				new_dst_len = dir_len + sprintfV(MakeAddr(dst, dir_len), FMT_STR_V,
										writeReq->stat.cFileName);
			else
				new_dst_len = dir_len + MakeRenameName(MakeAddr(dst, dir_len),
										writeReq->stat.renameCount, writeReq->stat.cFileName);

			if (mkdirQueueBuf.UsedSize()) {
				ExecDirQueue();
			}
			if (isListingOnly) {
				if (writeReq->command == CREATE_HARDLINK) {
					PutList(MakeAddr(dst, dstPrefixLen), PL_HARDLINK);
					total.linkFiles++;
				}
				else {
					PutList(MakeAddr(dst, dstPrefixLen), PL_NORMAL);
					total.writeFiles++;
					total.writeTrans += writeReq->stat.FileSize();
				}
				if (info.mode == MOVE_MODE) {
					SetFinishFileID(writeReq->stat.fileID, MoveObj::DONE);
				}
			}
			else if ((ret = WriteFileProc(new_dst_len)), isAbort) {
				break;
			}
		}
		else if (writeReq->command == CASE_CHANGE) {
			ret = CaseAlignProc(dir_len);
		}
		else if (writeReq->command == MKDIR || writeReq->command == INTODIR) {
			ret = WriteDirProc(dir_len);
		}
		else if (writeReq->command == DELETE_FILES) {
			if (IsDir(writeReq->stat.dwFileAttributes))
				ret = DeleteDirProc(dst, dir_len, writeReq->stat.cFileName, &writeReq->stat);
			else
				ret = DeleteFileProc(dst, dir_len, writeReq->stat.cFileName, &writeReq->stat);
		}
		else if (writeReq->command == RETURN_PARENT) {
			break;
		}
		else {
			switch (writeReq->command) {
			case WRITE_ABORT: case WRITE_FILE_CONT:
			case WRITE_BACKUP_ACL: case WRITE_BACKUP_EADATA:
			case WRITE_BACKUP_ALTSTREAM: case WRITE_BACKUP_END:
				break;

			default:
				ret = FALSE;
				WCHAR cmd[2] = { writeReq->command + '0', 0 };
				ConfirmErr("Illegal Request (internal error)", cmd, CEF_STOP|CEF_NOAPI);
				break;
			}
		}
	}
	return	ret && !isAbort;
}

BOOL FastCopy::CaseAlignProc(int dir_len)
{
	if (dir_len >= 0) {
		sprintfV(MakeAddr(dst, dir_len), FMT_STR_V, writeReq->stat.cFileName);
	}

//	PutList(MakeAddr(dst, dstPrefixLen), PL_CASECHANGE);

	if (isListingOnly) return TRUE;

	return	MoveFileV(dst, dst);
}

BOOL FastCopy::WriteDirProc(int dir_len)
{
	BOOL		ret = TRUE;
	BOOL		is_mkdir = writeReq->command == MKDIR;
	BOOL		is_reparse = IsReparse(writeReq->stat.dwFileAttributes)
							 && (info.flags & DIR_REPARSE) == 0;
	int			buf_size = writeReq->bufSize;
	int			new_dir_len;
	FileStat	sv_stat;

	memcpy(&sv_stat, &writeReq->stat, offsetof(FileStat, cFileName));

	if (writeReq->stat.renameCount == 0)
		new_dir_len = dir_len + sprintfV(MakeAddr(dst, dir_len), FMT_STR_V,
								writeReq->stat.cFileName);
	else
		new_dir_len = dir_len + MakeRenameName(MakeAddr(dst, dir_len),
								writeReq->stat.renameCount, writeReq->stat.cFileName);

	if (buf_size) {
		dstDirExtBuf.AddUsedSize(buf_size);
		if (dstDirExtBuf.RemainSize() < MIN_DSTDIREXT_BUF
		&& !dstDirExtBuf.Grow(MIN_DSTDIREXT_BUF)) {
			ConfirmErr("Can't alloc memory(dstDirExtBuf)", NULL, CEF_STOP);
			goto END;
		}
		memcpy(dstDirExtBuf.Buf() + dstDirExtBuf.UsedSize(), writeReq->buf, buf_size);
		sv_stat.acl = dstDirExtBuf.Buf() + dstDirExtBuf.UsedSize();
		sv_stat.ead = sv_stat.acl + sv_stat.aclSize;
		sv_stat.rep = sv_stat.ead + sv_stat.eadSize;
	}

	if (is_mkdir) {
		if (is_reparse || (info.flags & SKIP_EMPTYDIR) == 0) {
			if (mkdirQueueBuf.UsedSize()) {
				ExecDirQueue();
			}
			if (isListingOnly || CreateDirectoryV(dst, NULL)) {
				if (isListing && !is_reparse)
					PutList(MakeAddr(dst, dstPrefixLen), PL_DIRECTORY);
				total.writeDirs++;
			}
//			else total.skipDirs++;
		}
		else {
			if (mkdirQueueBuf.RemainSize() < sizeof(int)
			&& mkdirQueueBuf.Grow(MIN_MKDIRQUEUE_BUF) == FALSE) {
				ConfirmErr("Can't alloc memory(mkdirQueueBuf)", NULL, CEF_STOP);
				goto END;
			}
			*(int *)(mkdirQueueBuf.Buf() + mkdirQueueBuf.UsedSize()) = new_dir_len;
			mkdirQueueBuf.AddUsedSize(sizeof(int));
		}
	}
	strcpyV(MakeAddr(dst, new_dir_len), BACK_SLASH_V);

	if (!is_reparse) {
		if ((ret = WriteProc(new_dir_len + 1)), isAbort) {	// 再帰
			goto END;
		}
	}

	SetChar(dst, new_dir_len, 0);	// 末尾の '\\' を取る

	if (!is_mkdir || (info.flags & SKIP_EMPTYDIR) == 0 || mkdirQueueBuf.UsedSize() == 0
	|| is_reparse) {
		if (ret && sv_stat.isCaseChanged) CaseAlignProc();

		// タイムスタンプ/ACL/属性/リパースポイントのセット
		if (isListingOnly || (ret = SetDirExtData(&sv_stat))) {
			if (isListing && is_reparse && is_mkdir) {
				PutList(MakeAddr(dst, dstPrefixLen), PL_DIRECTORY|PL_REPARSE);
			}
		}
		else if (is_reparse && is_mkdir) {
			// 新規作成フォルダのリパースポイント化に失敗した場合は、フォルダ削除
			RemoveDirectoryV(dst);
		}
	}

	if (buf_size) {
		dstDirExtBuf.AddUsedSize(-buf_size);
	}
	if (mkdirQueueBuf.UsedSize()) {
		mkdirQueueBuf.AddUsedSize(-(int)sizeof(int));
	}

END:
	return	ret && !isAbort;
}

BOOL FastCopy::ExecDirQueue(void)
{
	for (int offset=0; offset < mkdirQueueBuf.UsedSize(); offset += sizeof(int)) {
		int dir_len = *(int *)(mkdirQueueBuf.Buf() + offset);
		SetChar(dst, dir_len, 0);
		if (isListingOnly || CreateDirectoryV(dst, NULL)) {
			if (isListing) PutList(MakeAddr(dst, dstPrefixLen), PL_DIRECTORY);
			total.writeDirs++;
		}
//		else total.skipDirs++;
		SetChar(dst, dir_len, '\\');
	}
	mkdirQueueBuf.SetUsedSize(0);
	return	TRUE;
}

BOOL FastCopy::SetDirExtData(FileStat *stat)
{
	if (stat->dwFileAttributes &
			(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_SYSTEM)) {
		SetFileAttributesV(dst, stat->dwFileAttributes);
	}
	HANDLE	fh;
	DWORD	mode = GENERIC_WRITE | (stat->acl && enableAcl ? (WRITE_OWNER|WRITE_DAC) : 0);
	BOOL	is_reparse = IsReparse(stat->dwFileAttributes) && (info.flags & DIR_REPARSE) == 0;
	BOOL	ret = TRUE;
	DWORD	share = FILE_SHARE_READ|FILE_SHARE_WRITE;
	DWORD	flg = FILE_FLAG_BACKUP_SEMANTICS|(is_reparse ? FILE_FLAG_OPEN_REPARSE_POINT : 0);

	if (!IS_WINNT_V
	|| (fh = CreateFileV(dst, mode, share, 0, OPEN_EXISTING, flg, 0)) == INVALID_HANDLE_VALUE) {
		if (is_reparse) {
			total.errDirs++;
		}
		return	FALSE;
	}

	if (is_reparse && stat->rep) {
		char	rp[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
		BOOL	is_set = TRUE;
		if (ReadReparsePoint(fh, rp, sizeof(rp)) > 0) {
			if (IsReparseDataSame(rp, stat->rep)) {
				is_set = FALSE;
			} else {
				DeleteReparsePoint(fh, rp);
			}
		}
		if (is_set) {
			if (WriteReparsePoint(fh, stat->rep, stat->repSize) <= 0) {
				ret = FALSE;
				total.errDirs++;
				ConfirmErr("WriteReparsePoint(dir)", MakeAddr(dst, dstPrefixLen));
			}
		}
	}

	if (stat->acl) {
		void	*backupContent = NULL;
		DWORD	size;
		if (!::BackupWrite(fh, stat->acl, stat->aclSize, &size, FALSE, TRUE, &backupContent)) {
			if (info.flags & REPORT_ACL_ERROR)
				ConfirmErr("BackupWrite(DIRACL)", MakeAddr(dst, dstPrefixLen));
		}
		::BackupWrite(fh, NULL, NULL, NULL, TRUE, TRUE, &backupContent);
	}
	::SetFileTime(fh, &stat->ftCreationTime, &stat->ftLastAccessTime, &stat->ftLastWriteTime);
	::CloseHandle(fh);

	return	ret;
}

/*=========================================================================
  関  数 ： WDigestThread
  概  要 ： WDigestThread 処理
  説  明 ： 
  注  意 ： 
=========================================================================*/
unsigned WINAPI FastCopy::WDigestThread(void *fastCopyObj)
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
	return	((FastCopy *)fastCopyObj)->WDigestThreadCore();
}

BOOL FastCopy::WDigestThreadCore(void)
{
	_int64			fileID = 0;
	BYTE			digest[SHA1_SIZE];
	DigestCalc		*calc = NULL;
	DataList::Head	*head;

	wDigestList.Lock();

	while (1) {
		while ((!(head = wDigestList.Fetch())
		|| (calc = (DigestCalc *)head->data)->status == DigestCalc::INIT) && !isAbort) {
			wDigestList.Wait(CV_WAIT_TICK);
		}
		if (isAbort) break;
		if (calc->status == DigestCalc::FIN) {
			wDigestList.Get();
			wDigestList.Notify();
			break;
		}

		if (calc->status == DigestCalc::CONT || calc->status == DigestCalc::DONE) {
			if (calc->dataSize) {
				wDigestList.UnLock();
//				Sleep(0);
				if (fileID != calc->fileID) {
					dstDigest.Reset();
				}
				dstDigest.Update(calc->data, calc->dataSize);
				if (calc->status == DigestCalc::DONE) {
					dstDigest.GetVal(digest);
				}
//				total.verifyTrans += calc->dataSize;
				wDigestList.Lock();
			}
			fileID = calc->fileID;
		}

		if (calc->status == DigestCalc::DONE) {
			if (calc->dataSize) {
				if (memcmp(calc->digest, digest, dstDigest.GetDigestSize()) == 0) {
					// compare OK
				}
				else {
					char	buf[512];
					MakeVerifyStr(buf, calc->digest, digest, dstDigest.GetDigestSize());
					ConfirmErr(buf, MakeAddr(calc->path, dstPrefixLen), CEF_NOAPI);
					calc->status = DigestCalc::ERR;
				}
			}
			else if (isListing) {
				dstDigest.GetEmptyVal(calc->digest);
			}
		}

		if (calc->status == DigestCalc::DONE) {
			if (isListing) {
				PutList(MakeAddr(calc->path, dstPrefixLen), PL_NORMAL, 0, calc->digest);
			}
			if (info.mode == MOVE_MODE) {
				SetFinishFileID(calc->fileID, MoveObj::DONE);
			}
			total.verifyFiles++;
		}
		else if (calc->status == DigestCalc::ERR) {
			if (info.mode == MOVE_MODE) {
				SetFinishFileID(calc->fileID, MoveObj::ERR);
			}
			total.errFiles++;
			if (isListing) {
				PutList(MakeAddr(calc->path, dstPrefixLen), PL_NORMAL|PL_COMPARE, 0,calc->digest);
			}
		}
		wDigestList.Get();	// remove from wDigestList
		wDigestList.Notify();
		calc = NULL;
	}
	wDigestList.UnLock();

	return	isAbort;
}


FastCopy::DigestCalc *FastCopy::GetDigestCalc(DigestObj *obj, int io_size)
{
	DataList::Head	*head;
	DigestCalc		*calc = NULL;
	int				require_size = sizeof(DigestCalc) + (obj ? obj->pathLen : 0);

	if (wDigestList.Size() != wDigestList.MaxSize() && !isAbort) {
		BOOL	is_eof = FALSE;
		cv.Lock();
		if (writeReq && writeReq->command == REQ_EOF) {
			is_eof = TRUE;
		}
		cv.UnLock();

		if (is_eof) {
			mainBuf.FreeBuf();
		}

		if (!wDigestList.Grow(wDigestList.MaxSize() - wDigestList.Size())) {
			ConfirmErr("Can't alloc memory(digest)", NULL, CEF_STOP);
		}

		if (isAbort) return	NULL;
	}

	if (io_size) {
		require_size += io_size + dstSectorSize;
	}
	wDigestList.Lock();

	while (wDigestList.RemainSize() <= require_size && !isAbort) {
		wDigestList.Wait(CV_WAIT_TICK);
	}
	if (!isAbort && (head = wDigestList.Alloc(NULL, 0, require_size)) != NULL) {
		calc = (DigestCalc *)head->data;
		calc->status = DigestCalc::INIT;
		if (obj) {
			calc->fileID = obj->fileID;
			calc->data = (BYTE *)ALIGN_SIZE((DWORD)(calc->path + obj->pathLen), dstSectorSize);
			memcpy(calc->path, obj->path, obj->pathLen);
			if (obj->fileSize) {
				memcpy(calc->digest, obj->digest, dstDigest.GetDigestSize());
			}
		}
		else {
			calc->fileID = -1;
			calc->data = NULL;
		}
	}
	wDigestList.UnLock();

	return	calc;
}

BOOL FastCopy::PutDigestCalc(DigestCalc *obj, DigestCalc::Status status)
{
	wDigestList.Lock();
	obj->status = status;
	wDigestList.Notify();
	wDigestList.UnLock();

	return	TRUE;
}

BOOL FastCopy::MakeDigestAsync(DigestObj *obj)
{
	DWORD		flg = ((info.flags & USE_OSCACHE_READVERIFY) ? 0 : FILE_FLAG_NO_BUFFERING) |
						FILE_FLAG_SEQUENTIAL_SCAN;
	DWORD		share = FILE_SHARE_READ|FILE_SHARE_WRITE;
	HANDLE		hFile = INVALID_HANDLE_VALUE;
	_int64		remain_size = 0;
	BOOL		ret = TRUE;
	DigestCalc	*calc = NULL;

	if (waitTick) Wait((waitTick + 9) / 10);

	if (obj->fileSize) {
		BY_HANDLE_FILE_INFORMATION	bhi;
		if ((hFile = CreateFileWithRetry(obj->path, GENERIC_READ, share, 0, OPEN_EXISTING, flg,
						0, 5)) == INVALID_HANDLE_VALUE) {
			ConfirmErr("OpenFile(digest)", MakeAddr(obj->path, dstPrefixLen));
			ret = FALSE;
		}
		if (ret && (ret = ::GetFileInformationByHandle(hFile, &bhi))) {
			remain_size = FileSize(bhi);
		}
		else goto END;
	}

	if (remain_size != obj->fileSize) {
		ret = FALSE;
		ConfirmErr("Size is changed", MakeAddr(obj->path, dstPrefixLen), CEF_NOAPI);
		goto END;
	}
	if (obj->fileSize == 0) {
		calc = GetDigestCalc(obj, 0);
		if (calc) {
			calc->dataSize = 0;
			PutDigestCalc(calc, DigestCalc::DONE);
		}
		goto END;
	}

	while (remain_size > 0 && !isAbort) {
		int	max_trans = waitTick && (info.flags & AUTOSLOW_IOLIMIT) ? MIN_BUF : maxDigestReadSize;
		int	io_size = remain_size > max_trans ? max_trans : (int)remain_size;

		io_size = (int)ALIGN_SIZE(io_size, dstSectorSize);
		calc = GetDigestCalc(obj, io_size);

		if (calc) {
			DWORD	trans_size = 0;
			if (isAbort || io_size && (!ReadFileWithReduce(hFile, calc->data, io_size,
										&trans_size, NULL) || trans_size <= 0)) {
				ConfirmErr("ReadFile(digest)", MakeAddr(obj->path, dstPrefixLen));
				ret = FALSE;
				break;
			}
			calc->dataSize = trans_size;
			remain_size -= trans_size;
			total.verifyTrans += trans_size;
			PutDigestCalc(calc, remain_size > 0 ? DigestCalc::CONT : DigestCalc::DONE);
		}
		if (waitTick && remain_size > 0) Wait();
	}

END:
	if (hFile != INVALID_HANDLE_VALUE) {
		::CloseHandle(hFile);
	}

	if (!ret) {
		if (!calc) calc = GetDigestCalc(obj, 0);
		if (calc) PutDigestCalc(calc, DigestCalc::ERR);
	}

	return	ret;
}

BOOL FastCopy::CheckDigests(CheckDigestMode mode)
{
	DataList::Head *head;
	BOOL	ret = TRUE;

	if (mode == CD_NOWAIT && !digestList.Fetch()) return ret;

	while (!isAbort && (head = digestList.Get())) {
		if (!MakeDigestAsync((DigestObj *)head->data)) {
			ret = FALSE;
		}
	}
	digestList.Clear();

	cv.Lock();
	runMode = RUN_NORMAL;
	cv.Notify();
	cv.UnLock();

	if (mode == CD_FINISH) {
		DigestCalc	*calc = GetDigestCalc(NULL, 0);
		if (calc) PutDigestCalc(calc, DigestCalc::FIN);
	}

	if (mode == CD_WAIT || mode == CD_FINISH) {
		wDigestList.Lock();
		while (wDigestList.Num() > 0 && !isAbort) {
			wDigestList.Wait(CV_WAIT_TICK);
		}
		wDigestList.UnLock();
	}

	return	ret && !isAbort;
}

HANDLE FastCopy::CreateFileWithRetry(void *path, DWORD mode, DWORD share,
	SECURITY_ATTRIBUTES *sa, DWORD cr_mode, DWORD flg, HANDLE hTempl, int retry_max)
{
	HANDLE	fh = INVALID_HANDLE_VALUE;

	for (int i=0; i < retry_max && !isAbort; i++) {	// ウイルスチェックソフト対策
		if ((fh = CreateFileV(path, mode, share, sa, cr_mode, flg, hTempl))
				!= INVALID_HANDLE_VALUE)
			break;
		if (::GetLastError() != ERROR_SHARING_VIOLATION)
			break;
		::Sleep(i * i * 10);
		total.openRetry++;
	}
	return	fh;
}

BOOL FastCopy::WrieFileWithReduce(HANDLE hFile, void *buf, DWORD size, DWORD *written,
	OVERLAPPED *overwrap)
{
	DWORD	trans = 0;
	DWORD	total = 0;
	DWORD	maxWriteSizeSv = maxWriteSize;

	while ((trans = size - total) > 0) {
		DWORD	transed = 0;
		trans = min(trans, maxWriteSize);
		if (!::WriteFile(hFile, (BYTE *)buf + total, trans, &transed, overwrap)) {
			if (::GetLastError() != ERROR_NO_SYSTEM_RESOURCES
			|| min(size, maxWriteSize) <= REDUCE_SIZE) {
				return	FALSE;
			}
			maxWriteSize -= REDUCE_SIZE;
			maxWriteSize = ALIGN_SIZE(maxWriteSize, REDUCE_SIZE);
		}
		total += transed;
	}
	*written = total;

	if (maxWriteSize != maxWriteSizeSv) {
		WCHAR buf[128];
		sprintfV(buf, FMT_REDUCEMSG_V, 'W', maxWriteSizeSv / 1024/1024, maxWriteSize / 1024/1024);
		WriteErrLog(buf);
	}

	return	TRUE;
}

BOOL FastCopy::RestoreHardLinkInfo(DWORD *link_data, void *path, int base_len)
{
	LinkObj	*obj;
	DWORD	hash_id = hardLinkList.MakeHashId(link_data);

	if (!(obj = (LinkObj *)hardLinkList.Search(link_data, hash_id))) {
		ConfirmErr("HardLinkInfo is gone(internal error)", MakeAddr(path, base_len), CEF_NOAPI);
		return	FALSE;
	}

	strcpyV(MakeAddr(path, base_len), obj->path);

	if (--obj->nLinks <= 1) {
		hardLinkList.UnRegister(obj);
		delete obj;
	}

	return	TRUE;
}

BOOL FastCopy::WriteFileProc(int dst_len)
{
	HANDLE	fh	= INVALID_HANDLE_VALUE;
	HANDLE	fh2	= INVALID_HANDLE_VALUE;
	BOOL	ret = TRUE;
	_int64	file_size = writeReq->stat.FileSize();
	_int64	remain = file_size;
	DWORD	trans_size;
	FileStat *stat = &writeReq->stat, sv_stat;
	Command	command = writeReq->command;

	BOOL	is_reparse = IsReparse(stat->dwFileAttributes) && (info.flags & FILE_REPARSE) == 0;
	BOOL	is_hardlink = command == CREATE_HARDLINK;
	BOOL	is_nonbuf = dstFsType != FSTYPE_NETWORK &&
						(file_size >= nbMinSize || (file_size % dstSectorSize) == 0)
						&& (info.flags & USE_OSCACHE_WRITE) == 0 && !is_reparse ? TRUE : FALSE;
	BOOL	is_reopen = is_nonbuf && (file_size % dstSectorSize) && !is_reparse ? TRUE : FALSE;
	BOOL	is_stream = command == WRITE_BACKUP_ALTSTREAM;
	int		&totalFiles = is_hardlink ? total.linkFiles : is_stream ?
							total.writeAclStream : total.writeFiles;
	int		&totalErrFiles = is_stream ? total.errAclStream  : total.errFiles;
	_int64	&totalErrTrans = is_stream ? total.errStreamTrans : total.errTrans;
	BOOL	is_digest = IsUsingDigestList() && !is_stream && !is_reparse;
	BOOL	is_require_del = (info.flags & (DEL_BEFORE_CREATE|RESTORE_HARDLINK)) ? TRUE : FALSE;

	// writeReq の stat を待避して、それを利用する
	if (command == WRITE_BACKUP_FILE || file_size > writeReq->bufSize) {
		memcpy((stat = &sv_stat), &writeReq->stat, offsetof(FileStat, cFileName));
	}

	DWORD	mode = GENERIC_WRITE;
	DWORD	share = FILE_SHARE_READ|FILE_SHARE_WRITE;
	DWORD	flg = (is_nonbuf ? FILE_FLAG_NO_BUFFERING : 0) | FILE_FLAG_SEQUENTIAL_SCAN;

	if (command == WRITE_BACKUP_FILE || command == WRITE_BACKUP_ALTSTREAM) {
		if (stat->acl && enableAcl) mode |= WRITE_OWNER|WRITE_DAC;
		flg  |= FILE_FLAG_BACKUP_SEMANTICS;
	}
	if (is_reparse) {
		flg  |= FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT;
	}

	if (waitTick) Wait((waitTick + 9) / 10);

	if (is_require_del) {
		if (!DeleteFileV(dst) && ::GetLastError() != ERROR_FILE_NOT_FOUND) {
			SetFileAttributesV(dst, FILE_ATTRIBUTE_NORMAL);
			DeleteFileV(dst);
		}
	}

	if (is_hardlink) {
		if (!(ret = RestoreHardLinkInfo(writeReq->stat.GetLinkData(), hardLinkDst, dstBaseLen))) {
			goto END2;
		}
		if (!(ret = CreateHardLinkV(dst, hardLinkDst, NULL))) {
			ConfirmErr("CreateHardLink", MakeAddr(dst, dstPrefixLen));
		}
//		DBGWriteW(L"Hardlink %08x.%08x.%08x (%s -> %s) %d\n", data[0], data[1], data[2],
//		MakeAddr(dst, dstPrefixLen), MakeAddr(hardLinkDst, dstPrefixLen), ret);
		goto END2;
	}

	if ((fh = CreateFileV(dst, mode, share, 0, CREATE_ALWAYS, flg, 0)) == INVALID_HANDLE_VALUE) {
		SetFileAttributesV(dst, FILE_ATTRIBUTE_NORMAL);
		if (stat->acl && enableAcl) DeleteFileV(dst); // 削除をtryしていない
		fh = CreateFileV(dst, mode, share, 0, CREATE_ALWAYS, flg, 0);
	}

	if (fh == INVALID_HANDLE_VALUE) {
		SetErrFileID(stat->fileID);
		totalErrFiles++;
		totalErrTrans += file_size;
		if (!is_stream || (info.flags & REPORT_STREAM_ERROR))
			ConfirmErr(is_stream ? "CreateFile(stream)" : "CreateFile",
				MakeAddr(dst, dstPrefixLen));
		ret = FALSE;
		goto END;
	}

	if (is_reparse) {
		if (WriteReparsePoint(fh, writeReq->buf, stat->repSize) <= 0) {
			totalErrFiles++;
			totalErrTrans += file_size;
			ConfirmErr("WriteReparsePoint(File)", MakeAddr(dst, dstPrefixLen));
		}
	}
	else {
		if (is_reopen) {
			flg &= ~FILE_FLAG_NO_BUFFERING;
			fh2 = CreateFileV(dst, mode, share, 0, OPEN_EXISTING, flg, 0);
		}

		if (file_size > writeReq->bufSize) {
			_int64	alloc_size = is_nonbuf ? ALIGN_SIZE(file_size, dstSectorSize) : file_size;
			LONG	high_size = (LONG)(alloc_size >> 32);

			::SetFilePointer(fh, (LONG)alloc_size, &high_size, FILE_BEGIN);
			if (!::SetEndOfFile(fh) && GetLastError() == ERROR_DISK_FULL) {
				SetErrFileID(stat->fileID);
				ConfirmErr(is_stream ? "SetEndOfFile(stream)" : "SetEndOfFile",
				 MakeAddr(dst, dstPrefixLen), file_size >= info.allowContFsize ? 0 : CEF_STOP);
				ret = FALSE;
				goto END2;
			}
		//	if (pSetFileValidData && !pSetFileValidData(fh, alloc_size)) {
		//		ConfirmErr("SetFileValidData(File)", MakeAddr(dst, dstPrefixLen));
		//	}
			::SetFilePointer(fh, 0, NULL, FILE_BEGIN);
		}

		while (remain > 0) {
			DWORD	write_size = (remain >= writeReq->bufSize) ? writeReq->bufSize :
								(DWORD)(is_nonbuf ? ALIGN_SIZE(remain, dstSectorSize) : remain);
			if (!(ret = WrieFileWithReduce(fh, writeReq->buf, write_size, &trans_size, NULL))
			|| write_size != trans_size) {
				ret = FALSE;
				SetErrFileID(stat->fileID);
				if (!is_stream || (info.flags & REPORT_STREAM_ERROR))
					ConfirmErr(is_stream ? "WriteFile(stream)" : "WriteFile",
					 MakeAddr(dst, dstPrefixLen),
						(GetLastError() != ERROR_DISK_FULL || file_size >= info.allowContFsize
						 || is_stream) ? 0 : CEF_STOP);
				break;
			}
			if ((remain -= trans_size) > 0) {	// 続きがある
				total.writeTrans += trans_size;
				if (RecvRequest() == FALSE || writeReq->command != WRITE_FILE_CONT) {
					ret = FALSE;
					remain = 0;
					if (!isAbort && writeReq->command != WRITE_ABORT) {
						WCHAR cmd[2] = { writeReq->command + '0', 0 };
						ConfirmErr("Illegal Request2 (internal error)", cmd, CEF_STOP|CEF_NOAPI);
					}
					break;
				}
				if (waitTick) Wait();
			}
			else {
				total.writeTrans += trans_size + remain;	// remain は 0 か負値
			}
		}

		if (is_reopen && fh2 == INVALID_HANDLE_VALUE && ret) {
			fh2 = CreateFileWithRetry(dst, mode, share, 0, OPEN_EXISTING, flg, 0, 10);
			if (fh2 == INVALID_HANDLE_VALUE && GetLastError() != ERROR_SHARING_VIOLATION) {
				flg  &= ~FILE_FLAG_BACKUP_SEMANTICS;
				mode &= ~(WRITE_OWNER|WRITE_DAC);
				fh2 = CreateFileWithRetry(dst, mode, share, 0, OPEN_EXISTING, flg, 0, 10);
			}
			if (fh2 == INVALID_HANDLE_VALUE) {
				ret = FALSE;
				if (!is_stream || (info.flags & REPORT_STREAM_ERROR))
					ConfirmErr(is_stream ? "CreateFile2(stream)" : "CreateFile2",
						MakeAddr(dst, dstPrefixLen));
			}
		}

		if (ret && remain != 0) {
			::SetFilePointer(fh2, stat->nFileSizeLow, (LONG *)&stat->nFileSizeHigh, FILE_BEGIN);
			if ((ret = ::SetEndOfFile(fh2)) == FALSE) {
				if (!is_stream || (info.flags & REPORT_STREAM_ERROR))
					ConfirmErr(is_stream ? "SetEndOfFile2(stream)" : "SetEndOfFile2",
						MakeAddr(dst, dstPrefixLen));
			}
		}
	}

END2:
	if (fh2 != INVALID_HANDLE_VALUE) ::CloseHandle(fh2);

	if (ret && is_digest && !isAbort) {
		int path_size = (dst_len + 1) * CHAR_LEN_V;
		DataList::Head	*head = digestList.Alloc(NULL, 0, sizeof(DigestObj) + path_size);
		if (!head) {
			ConfirmErr("Can't alloc memory(digestList)", NULL, CEF_STOP);
		}
		DigestObj *obj = (DigestObj *)head->data;
		obj->fileID = stat->fileID;
		obj->fileSize = stat->FileSize();
		obj->dwFileAttributes = stat->dwFileAttributes;
		obj->pathLen = path_size;
		memcpy(obj->digest, writeReq->stat.digest, dstDigest.GetDigestSize());
		memcpy(obj->path, dst, path_size);

		BOOL is_empty_buf = digestList.RemainSize() <= digestList.MinMargin();
		if (is_empty_buf || (info.flags & SERIAL_VERIFY_MOVE))
			CheckDigests(is_empty_buf ? CD_WAIT : CD_NOWAIT); // empty なら wait
	}

	if (command == WRITE_BACKUP_FILE) {
		/* ret = */ WriteFileBackupProc(fh, dst_len);	// ACL/EADATA/STREAM エラーは無視
	}

	if (!is_hardlink) {
		if (ret && !is_stream) {
			::SetFileTime(fh, &stat->ftCreationTime, &stat->ftLastAccessTime,
				&stat->ftLastWriteTime);
		}
		::CloseHandle(fh);
	}

	if (ret) {
		if (!is_stream) {
			if (stat->isCaseChanged && !is_require_del) CaseAlignProc();
			if (stat->dwFileAttributes /* &
					(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_SYSTEM) */) {
				SetFileAttributesV(dst, stat->dwFileAttributes);
			}
		}
		totalFiles++;
	}
	else {
		totalErrFiles++;
		totalErrTrans += file_size;
		SetErrFileID(stat->fileID);
		if (!is_stream) {
			DeleteFileV(dst);
		}
	}

END:
	if (!is_stream && info.mode == MOVE_MODE && (info.flags & VERIFY_FILE) == 0 && !isAbort) {
		SetFinishFileID(stat->fileID, ret ? MoveObj::DONE : MoveObj::ERR);
	}
	if ((isListingOnly || isListing && !is_digest) && !is_stream && ret) {
		DWORD flags = is_hardlink ? PL_HARDLINK : is_reparse ? PL_REPARSE : PL_NORMAL;
		PutList(MakeAddr(dst, dstPrefixLen), flags);
	}

	return	ret && !isAbort;
}

BOOL FastCopy::WriteFileBackupProc(HANDLE fh, int dst_len)
{
	BOOL	ret = TRUE, is_continue = TRUE;
	DWORD	size;
	void	*backupContent = NULL;	// for BackupWrite

	while (!isAbort && is_continue) {
		if (RecvRequest() == FALSE) {
			ret = FALSE;
			if (!isAbort) {
				WCHAR cmd[2] = { writeReq->command + '0', 0 };
				ConfirmErr("Illegal Request3 (internal error)", cmd, CEF_STOP|CEF_NOAPI);
			}
			break;
		}
		switch (writeReq->command) {
		case WRITE_BACKUP_ACL: case WRITE_BACKUP_EADATA:
			SetLastError(0);
			if (!(ret = ::BackupWrite(fh, writeReq->buf, writeReq->command == WRITE_BACKUP_ACL ?
						writeReq->stat.aclSize : writeReq->stat.eadSize, &size, FALSE, TRUE,
						&backupContent))) {
				if (info.flags & REPORT_ACL_ERROR)
					ConfirmErr("BackupWrite(ACL/EADATA)", MakeAddr(dst, dstPrefixLen));
			}
			break;

		case WRITE_BACKUP_ALTSTREAM:
			strcpyV(MakeAddr(dst, dst_len), writeReq->stat.cFileName);
			ret = WriteFileProc(0);
			lSetCharV(dst, dst_len, 0);
			break;

		case WRITE_BACKUP_END:
			is_continue = FALSE;
			break;

		case WRITE_FILE_CONT:	// エラー時のみ
			break;

		default:
			if (!isAbort) {
				WCHAR cmd[2] = { writeReq->command + '0', 0 };
				ConfirmErr("Illegal Request4 (internal error)", cmd, CEF_STOP|CEF_NOAPI);
			}
			ret = FALSE;
			break;
		}
	}

	if (backupContent) {
		::BackupWrite(fh, NULL, NULL, NULL, TRUE, TRUE, &backupContent);
	}
	return	ret && !isAbort;
}

BOOL FastCopy::ChangeToWriteModeCore(BOOL is_finish)
{
	BOOL	isResetRunMode = runMode == RUN_DIGESTREQ;

	while ((!readReqList.IsEmpty() || !rDigestReqList.IsEmpty() || !writeReqList.IsEmpty()
	|| writeReq || runMode == RUN_DIGESTREQ) && !isAbort) {
		if (!readReqList.IsEmpty()) {
			writeReqList.MoveList(&readReqList);
			cv.Notify();
			if (isResetRunMode) {
				runMode = RUN_DIGESTREQ;
			}
		}
		cv.Wait(CV_WAIT_TICK);
		if (isResetRunMode && readReqList.IsEmpty() && rDigestReqList.IsEmpty()
		&& writeReqList.IsEmpty() && writeReq) {
			runMode = RUN_DIGESTREQ;
			isResetRunMode = FALSE;
			cv.Notify();
		}
	}

	return	!isAbort;
}

BOOL FastCopy::ChangeToWriteMode(BOOL is_finish)
{
	cv.Lock();
	BOOL	ret = ChangeToWriteModeCore(is_finish);
	cv.UnLock();
	return	ret;
}

BOOL FastCopy::AllocReqBuf(int req_size, _int64 _data_size, ReqBuf *buf)
{
	int		max_free = (int)(mainBuf.Buf() + mainBuf.Size() - usedOffset);
	int		max_trans = waitTick && (info.flags & AUTOSLOW_IOLIMIT) ? MIN_BUF : maxReadSize;
	int		data_size = (_data_size > max_trans) ? max_trans : (int)_data_size;
	int		align_data_size = ALIGN_SIZE(data_size, 8);
	int		sector_data_size = ALIGN_SIZE(data_size, sectorSize);
	int		require_size = align_data_size + req_size;
	BYTE	*align_offset = data_size ?
							(BYTE *)ALIGN_SIZE((u_int)usedOffset, sectorSize) : usedOffset;

	max_free -= (int)(align_offset - usedOffset);

	if (data_size && align_data_size + req_size < sector_data_size)
		require_size = sector_data_size;

	if (require_size > max_free) {
		if (max_free < MIN_BUF) {
			align_offset = mainBuf.Buf();
			if (isSameDrv) {
				if (ChangeToWriteModeCore() == FALSE) {	// Read -> Write 切り替え
					return	FALSE;
				}
			}
		}
		else {
			data_size = max_free - req_size;
			align_data_size = data_size = (data_size / BIGTRANS_ALIGN) * BIGTRANS_ALIGN;
			require_size = data_size + req_size;
		}
	}
	buf->buf = align_offset;
	buf->bufSize = ALIGN_SIZE(data_size, sectorSize);
	buf->req = (ReqHeader *)(buf->buf + align_data_size);
	buf->reqSize = req_size;

												// isSameDrv == TRUE の場合、必ず Empty
	while ((!writeReqList.IsEmpty() || !rDigestReqList.IsEmpty()) && !isAbort) {
		if (buf->buf == mainBuf.Buf()) {
			if (freeOffset < usedOffset && (freeOffset - mainBuf.Buf()) >= require_size) {
				break;
			}
		}
		else {
			if (freeOffset < usedOffset || buf->buf + require_size <= freeOffset) {
				break;
			}
		}
		cv.Wait(CV_WAIT_TICK);
	}

	usedOffset = buf->buf + require_size;
	return	!isAbort;
}

BOOL FastCopy::PrepareReqBuf(int req_size, _int64 data_size, _int64 file_id, ReqBuf *buf)
{
	BOOL ret = TRUE;

	cv.Lock();

	if (errFileID) {
		if (errFileID == file_id)
			ret = FALSE;
		errFileID = 0;
	}

	if (ret)
		ret = AllocReqBuf(req_size, data_size, buf);

	cv.UnLock();

	return	ret;
}


BOOL FastCopy::SendRequest(Command command, ReqBuf *buf, FileStat *stat)
{
	BOOL	ret = TRUE;
	ReqBuf	tmp_buf;

	cv.Lock();

	if (buf == NULL) {
		buf = &tmp_buf;
		ret = AllocReqBuf(offsetof(ReqHeader, stat) + (stat ? stat->minSize : 0), 0, buf);
	}

	if (ret && !isAbort) {
		ReqHeader	*readReq;
		readReq				= buf->req;
		readReq->reqSize	= buf->reqSize;
		readReq->command	= command;
		readReq->buf		= buf->buf;
		readReq->bufSize	= buf->bufSize;
		if (stat) {
			memcpy(&readReq->stat, stat, stat->minSize);
		}

		if (IsUsingDigestList()) {
			rDigestReqList.AddObj(readReq);
			cv.Notify();
		}
		else if (isSameDrv) {
			readReqList.AddObj(readReq);
		}
		else {
			writeReqList.AddObj(readReq);
			cv.Notify();
		}
	}

	cv.UnLock();

	return	ret && !isAbort;
}

BOOL FastCopy::RecvRequest(void)
{
	cv.Lock();

	if (writeReq) {
		WriteReqDone();
	}

	CheckDstRequest();

	while (writeReqList.IsEmpty() && !isAbort) {
		if (info.mode == MOVE_MODE && (info.flags & VERIFY_FILE)) {
			if (runMode == RUN_DIGESTREQ
			|| (info.flags & SERIAL_VERIFY_MOVE) && digestList.Num() > 0) {
				cv.UnLock();
				CheckDigests(CD_WAIT);
				cv.Lock();
				continue;
			}
		}
		cv.Wait(CV_WAIT_TICK);
		CheckDstRequest();
	}
	writeReq = writeReqList.TopObj();

	cv.UnLock();

	return	writeReq && !isAbort;
}

void FastCopy::WriteReqDone(void)
{
	writeReqList.DelObj(writeReq);

	freeOffset = (BYTE *)writeReq + writeReq->reqSize;
	if (!isSameDrv || writeReqList.IsEmpty())
		cv.Notify();
	writeReq = NULL;
}

void FastCopy::SetErrFileID(_int64 file_id)
{
	cv.Lock();
	errFileID = file_id;
	cv.UnLock();
}

BOOL FastCopy::SetFinishFileID(_int64 _file_id, MoveObj::Status status)
{
	moveList.Lock();

	do {
		while (moveFinPtr = moveList.Fetch(moveFinPtr)) {
			MoveObj *data = (MoveObj *)moveFinPtr->data;
			if (data->fileID == _file_id) {
				data->status = status;
				break;
			}
	/*		else if (data->fileID > _file_id) {
				MessageBox(0, "Illegal fetch", "", MB_OK);
			}
			else if (data->status != MoveObj::DONE) {
				MessageBox(0, "Illegal fetch2", "", MB_OK);
			} */
		}
		if (moveFinPtr == NULL) {
			moveList.Wait(CV_WAIT_TICK);
		}
	} while (!isAbort && !moveFinPtr);

	if (moveList.IsWait()) {
		moveList.Notify();
	}
	moveList.UnLock();
	return	TRUE;
}


BOOL FastCopy::End(void)
{
	isAbort = TRUE;

	while (hWriteThread || hReadThread || hRDigestThread || hWDigestThread) {
		cv.Lock();
		cv.Notify();
		cv.UnLock();

		if (hReadThread) {
			if (::WaitForSingleObject(hReadThread, 1000) != WAIT_TIMEOUT) {
				::CloseHandle(hReadThread);
				hReadThread = NULL;
			}
		}
		else if (hWriteThread) {	// hReadThread が生きている場合は、hReadTread に Closeさせる
			if (::WaitForSingleObject(hWriteThread, 1000) != WAIT_TIMEOUT) {
				::CloseHandle(hWriteThread);
				hWriteThread = NULL;
			}
		}
		else if (hRDigestThread) {
			if (::WaitForSingleObject(hRDigestThread, 1000) != WAIT_TIMEOUT) {
				::CloseHandle(hRDigestThread);
				hRDigestThread = NULL;
			}
		}
		else if (hWDigestThread) {
			if (::WaitForSingleObject(hWDigestThread, 1000) != WAIT_TIMEOUT) {
				::CloseHandle(hWDigestThread);
				hWDigestThread = NULL;
			}
		}
	}

	if (hRunMutex) {
		::ReleaseMutex(hRunMutex);
	}
	delete [] openFiles;
	openFiles = NULL;

	mainBuf.FreeBuf();
//	baseBuf.FreeBuf();
	ntQueryBuf.FreeBuf();
	dstDirExtBuf.FreeBuf();
	mkdirQueueBuf.FreeBuf();
	dstStatIdxBuf.FreeBuf();
	dstStatBuf.FreeBuf();
	dirStatBuf.FreeBuf();
	fileStatBuf.FreeBuf();
	listBuf.FreeBuf();
	errBuf.FreeBuf();
	srcDigestBuf.FreeBuf();
	dstDigestBuf.FreeBuf();
	digestList.UnInit();
	moveList.UnInit();
	hardLinkList.UnInit();
	wDigestList.UnInit();
	delete [] hardLinkDst;
	hardLinkDst = NULL;

	return	TRUE;
}

BOOL FastCopy::Suspend(void)
{
	if (!hReadThread && !hWriteThread || isSuspend)
		return	FALSE;

	if (hReadThread)
		::SuspendThread(hReadThread);

	if (hWriteThread)
		::SuspendThread(hWriteThread);

	if (hRDigestThread)
		::SuspendThread(hRDigestThread);

	if (hWDigestThread)
		::SuspendThread(hWDigestThread);

	isSuspend = TRUE;
	suspendTick = ::GetTickCount();

	return	TRUE;
}

BOOL FastCopy::Resume(void)
{
	if (!hReadThread && !hWriteThread || !isSuspend)
		return	FALSE;

	isSuspend = FALSE;
	startTick += (::GetTickCount() - suspendTick);

	if (hReadThread)
		::ResumeThread(hReadThread);

	if (hWriteThread)
		::ResumeThread(hWriteThread);

	if (hRDigestThread)
		::ResumeThread(hRDigestThread);

	if (hWDigestThread)
		::ResumeThread(hWDigestThread);

	return	TRUE;
}

BOOL FastCopy::GetTransInfo(TransInfo *ti, BOOL fullInfo)
{
	ti->total = total;
	ti->listBuf = &listBuf;
	ti->listCs = &listCs;
	ti->errBuf = &errBuf;
	ti->errCs = &errCs;
	ti->isSameDrv = isSameDrv;
	ti->ignoreEvent = info.ignoreEvent;
	ti->waitTick = waitTick;
	ti->tickCount = (isSuspend ? suspendTick : endTick ? endTick : ::GetTickCount()) - startTick;
	if (fullInfo) {
		ConvertExternalPath(MakeAddr(dst, dstPrefixLen), ti->curPath,
			sizeof(ti->curPath)/CHAR_LEN_V);
	}
	return	TRUE;
}

int FastCopy::FdatToFileStat(WIN32_FIND_DATAW *fdat, FileStat *stat, BOOL is_usehash)
{
	stat->fileID			= 0;
	stat->ftCreationTime	= fdat->ftCreationTime;
	stat->ftLastAccessTime	= fdat->ftLastAccessTime;
	stat->ftLastWriteTime	= fdat->ftLastWriteTime;
	stat->nFileSizeLow		= fdat->nFileSizeLow;
	stat->nFileSizeHigh		= fdat->nFileSizeHigh;
	stat->dwFileAttributes	= fdat->dwFileAttributes;
	stat->hFile				= INVALID_HANDLE_VALUE;
	stat->lastError			= 0;
	stat->isExists			= FALSE;
	stat->isCaseChanged		= FALSE;
	stat->renameCount		= 0;
	stat->acl				= NULL;
	stat->aclSize			= 0;
	stat->ead				= NULL;
	stat->eadSize			= 0;
	stat->rep				= NULL;
	stat->repSize			= 0;
	stat->next				= NULL;
	memset(stat->digest, 0, SHA1_SIZE);

	int	len = (sprintfV(stat->cFileName, FMT_STR_V, fdat->cFileName) + 1) * CHAR_LEN_V;
	stat->size = len + offsetof(FileStat, cFileName);
	stat->minSize = ALIGN_SIZE(stat->size, 8);

	if (is_usehash) {
		stat->upperName = stat->cFileName + len;
		memcpy(stat->upperName, stat->cFileName, len);
		CharUpperV(stat->upperName);
		stat->hashVal = MakeHash(stat->upperName, len);
		stat->size += len;
	}
	stat->size = ALIGN_SIZE(stat->size, 8);

	return	stat->size;
}

BOOL FastCopy::ConvertExternalPath(const void *path, void *buf, int max_buf)
{
	if (IS_WINNT_V) {
		if (GetChar(path, 0) == '\\' && GetChar(path, 1) != '\\') {	// UNC
			SetChar(buf, 0, '\\');
			buf = MakeAddr(buf, 1);
			max_buf--;
		}
		sprintfV(buf, L"%.*s", max_buf, path);
	}
	else {
		sprintfV(buf, "%.*s", max_buf, path);
	}
	return	TRUE;
}

FastCopy::Confirm::Result FastCopy::ConfirmErr(const char *message, const void *path, DWORD flags)
{
	if (isAbort) return	Confirm::CANCEL_RESULT;

	BOOL	api_err = (flags & CEF_NOAPI) ? FALSE : TRUE;
	BOOL	allow_continue = (flags & CEF_STOP) ? FALSE : TRUE;

	WCHAR	path_buf[MAX_PATH_EX], msg_buf[MAX_PATH_EX + 100];
	DWORD	err_code = api_err ? ::GetLastError() : 0;
	int		len = 0;

	if (IS_WINNT_V) {
		if (path) {
			ConvertExternalPath(path, path_buf, sizeof(path_buf)/sizeof(WCHAR));
			path = path_buf;
		}
		len = AtoW(message, msg_buf, MAX_PATH_EX) -1;
		len += sprintfV(MakeAddr(msg_buf, len), L"(");
	}
	else {
		len = sprintfV(msg_buf, "%s(", message);
	}

	if (api_err && err_code) {
		len += FormatMessageV(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
				err_code, info.lcid > 0 ? LANGIDFROMLCID(info.lcid) : MAKELANGID(LANG_NEUTRAL,
				SUBLANG_DEFAULT), MakeAddr(msg_buf, len), MAX_PATH_EX, NULL);
		if (GetChar(msg_buf, len-1) == '\n') {
			len -= 2;	// remove "\r\n"
		}
	}

	void *fmt_us_v = IS_WINNT_V ? (void *)L"%u) : %s" : (void *)"%u) : %s";
	len += sprintfV(MakeAddr(msg_buf, len), fmt_us_v, err_code, path ? (void *)path :(void *)L"");

	WriteErrLog(msg_buf, len);

	if (listBuf.Buf() && isListing) {
		PutList(msg_buf, PL_ERRMSG);
	}

	if (allow_continue) {
		if (info.ignoreEvent & FASTCOPY_ERROR_EVENT) return	Confirm::CONTINUE_RESULT;
	}
	else isAbort = TRUE;

	Confirm	 confirm = { msg_buf, allow_continue, path, err_code, Confirm::CONTINUE_RESULT };

	if ((info.ignoreEvent & FASTCOPY_STOP_EVENT) == 0) {
		isSuspend = TRUE;
		suspendTick = ::GetTickCount();

		::SendMessage(info.hNotifyWnd, info.uNotifyMsg, CONFIRM_NOTIFY, (LPARAM)&confirm);

		isSuspend = FALSE;
		startTick += (::GetTickCount() - suspendTick);
	}
	else {
		confirm.result = Confirm::CANCEL_RESULT;
	}

	switch (confirm.result) {
	case Confirm::IGNORE_RESULT:
		info.ignoreEvent |= FASTCOPY_ERROR_EVENT;
		confirm.result    = Confirm::CONTINUE_RESULT;
		break;

	case Confirm::CANCEL_RESULT:
		isAbort = TRUE;
		break;
	}
	return	confirm.result;
}

BOOL FastCopy::WriteErrLog(void *message, int len)
{
#define ERRMSG_SIZE		60
	::EnterCriticalSection(&errCs);

	BOOL	ret = TRUE;
	void	*msg_buf = (char *)errBuf.Buf() + errBuf.UsedSize();

	if (len == -1)
		len = strlenV(message);
	int	need_size = ((len + 3) * CHAR_LEN_V) + ERRMSG_SIZE;

	if (errBuf.UsedSize() + need_size <= errBuf.MaxSize()) {
		if (errBuf.RemainSize() > need_size || errBuf.Grow(ALIGN_SIZE(need_size, PAGE_SIZE))) {
			memcpy(msg_buf, message, len * CHAR_LEN_V);
			SetChar(msg_buf, len++, '\r');
			SetChar(msg_buf, len++, '\n');
			errBuf.AddUsedSize(len * CHAR_LEN_V);
		}
	}
	else {
		if (errBuf.RemainSize() > ERRMSG_SIZE) {
			void *err_msg = IS_WINNT_V ? (void *)L" Too Many Errors...\r\n" :
										 (void *) " Too Many Errors...\r\n";
			sprintfV(msg_buf, FMT_STR_V, err_msg);
			errBuf.SetUsedSize(errBuf.MaxSize());
		}
		ret = FALSE;
	}
	::LeaveCriticalSection(&errCs);
	return	ret;
}

void FastCopy::Aborting(void)
{
	isAbort = TRUE;
	void *err_msg = IS_WINNT_V ? (void *)L" Aborted by User" : (void *)" Aborted by User";
	WriteErrLog(err_msg);
	if (!isListingOnly && isListing) PutList(err_msg, PL_ERRMSG);
}

BOOL FastCopy::Wait(DWORD tick)
{
#define SLEEP_UNIT	200
	int	svWaitTick = (int)waitTick;
	int	remain = (int)(tick ? tick : waitTick);

	while (remain > 0 && !isAbort) {
		int	unit = remain > SLEEP_UNIT ? SLEEP_UNIT : remain;
		::Sleep(unit);
		int	curWaitTick = waitTick;
		if (curWaitTick != SUSPEND_WAITTICK) {
			remain -= unit + (svWaitTick - curWaitTick);
			svWaitTick = curWaitTick;
		}
	}
	return	!isAbort;
}

