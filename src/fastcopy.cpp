static char *fastcopy_id = 
	"@(#)Copyright (C) 2004-2015 H.Shirouzu		fastcopy.cpp	ver3.03";
/* ========================================================================
	Project  Name			: Fast Copy file and directory
	Create					: 2004-09-15(Wed)
	Update					: 2015-08-30(Sun)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	Modify					: Mapaler 2015-08-23
	======================================================================== */

#include "fastcopy.h"
#include <stdarg.h>
#include <stddef.h>
#include <process.h>
#include <stdio.h>
#include <time.h>


#define STRMID_OFFSET	offsetof(WIN32_STREAM_ID, cStreamName)
#define MAX_ALTSTREAM	1000
#define REDUCE_SIZE		(1024 * 1024)

//static BOOL (WINAPI *pSetFileValidData)(HANDLE hFile, LONGLONG ValidDataLength);

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

	TLibInit_Ntdll();

//	if (!pSetFileValidData) {
//		pSetFileValidData = (BOOL (WINAPI *)(HANDLE, LONGLONG))GetProcAddress(
//											GetModuleHandle("kernel32.dll"), "SetFileValidData");
//		if (pSetFileValidData && !TSetPrivilege(SE_MANAGE_VOLUME_NAME, TRUE)) {
//			pSetFileValidData = NULL;
//		}
//	}

	::InitializeCriticalSection(&errCs);
	::InitializeCriticalSection(&listCs);

	shareInfo.Init(&driveMng);
	driveMng.SetShareInfo(&shareInfo);

	src = new WCHAR [MAX_WPATH + MAX_PATH];
	dst = new WCHAR [MAX_WPATH + MAX_PATH];
	confirmDst = new WCHAR [MAX_WPATH + MAX_PATH];

	maxStatSize = (MAX_PATH * sizeof(WCHAR)) * 2 + offsetof(FileStat, cFileName) + 8;
	waitTick = 0;
	hardLinkDst = NULL;
}

FastCopy::~FastCopy()
{
	delete [] confirmDst;
	delete [] dst;
	delete [] src;

	::DeleteCriticalSection(&listCs);
	::DeleteCriticalSection(&errCs);
}

FastCopy::FsType FastCopy::GetFsType(const WCHAR *root_dir)
{
	if (::GetDriveTypeW(root_dir) == DRIVE_REMOTE)
		return	FSTYPE_NETWORK;

	DWORD	serial, max_fname, fs_flags;
	WCHAR	vol[MAX_PATH], fs[MAX_PATH];

	if (::GetVolumeInformationW(root_dir, vol, MAX_PATH, &serial, &max_fname, &fs_flags,
			fs, MAX_PATH) == FALSE)
		return	ConfirmErr(L"GetVolumeInformation", root_dir, CEF_STOP), FSTYPE_NONE;

	return	wcsicmp(fs, NTFS_STR) == 0 ? FSTYPE_NTFS : FSTYPE_FAT;
}

int FastCopy::GetSectorSize(const WCHAR *root_dir)
{
	DWORD	spc, bps, fc, cl;

	if (::GetDiskFreeSpaceW(root_dir, &spc, &bps, &fc, &cl) == FALSE) {
		return	BIG_SECTOR_SIZE;
	}
	return	bps;
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
	memmove(buf + prefix_len - (isUNC ? 1 : 0), buf, (wcslen(buf) + 1) * sizeof(WCHAR));
	memcpy(buf, prefix, prefix_len * sizeof(WCHAR));
	return	prefix_len;
}

BOOL FastCopy::InitDstPath(void)
{
	DWORD	attr;
	WCHAR	wbuf[MAX_PATH_EX];
	WCHAR	*buf = wbuf, *fname = NULL;
	const WCHAR	*org_path = dstArray.Path(0), *dst_root;

	// dst の確認/加工
	if (org_path[1] == ':' && org_path[2] != '\\')
		return	ConfirmErr(GetLoadStrW(IDS_BACKSLASHERR), org_path, CEF_STOP|CEF_NOAPI), FALSE;

	if (::GetFullPathNameW(org_path, MAX_WPATH, dst, &fname) == 0)
		return	ConfirmErr(L"GetFullPathName", org_path, CEF_STOP), FALSE;

	GetRootDirW(dst, buf);
	dstArray.RegisterPath(buf);
	dst_root = dstArray.Path(dstArray.Num() -1);

	attr = ::GetFileAttributesW(dst);

	if ((attr = ::GetFileAttributesW(dst)) == 0xffffffff) {
		info.overWrite = BY_ALWAYS;	// dst が存在しないため、調査の必要がない
//		if (isListing) PutList(dst, PL_DIRECTORY);
	}
	if (!IsDir(attr))	// 例外的に reparse point も dir 扱い
		return	ConfirmErr(L"Not a directory", dst, CEF_STOP|CEF_NOAPI), FALSE;

	wcscpy(buf, dst);
	MakePathW(dst, buf, L"");
	// src自体をコピーするか（dst末尾に \ がついている or 複数指定）
	isExtendDir = wcscmp(buf, dst) == 0 || srcArray.Num() > 1 ? TRUE : FALSE;
	dstPrefixLen = MakeUnlimitedPath((WCHAR *)dst);
	dstBaseLen = (int)wcslen(dst);

	// dst ファイルシステム情報取得
	dstSectorSize = GetSectorSize(dst_root);
	dstFsType = GetFsType(dst_root);
	nbMinSize = (dstFsType == FSTYPE_NTFS) ? info.nbMinSizeNtfs : info.nbMinSizeFat;

	// 差分コピー用dst先ファイル確認
	wcscpy(confirmDst, dst);

	return	TRUE;
}

BOOL FastCopy::InitDepthBuf()
{
	if ((filterMode & REG_FILTER) == 0 || !srcDepth.Buf()) return FALSE;

	srcDepth.SetUsedNum(0);
	dstDepth.SetUsedNum(0);
	cnfDepth.SetUsedNum(0);

	srcDepth[0] = srcBaseLen;
	dstDepth[0] = cnfDepth[0] = dstBaseLen;

	return	TRUE;
}

BOOL FastCopy::InitSrcPath(int idx)
{
	WCHAR		src_root_cur[MAX_PATH];
	WCHAR		wbuf[MAX_PATH_EX];
	WCHAR		*buf = wbuf, *fname = NULL;
	const WCHAR	*dst_root = dstArray.Path(dstArray.Num() -1);
	const WCHAR	*org_path = srcArray.Path(idx);
	DWORD		cef_flg = IsStarting() ? 0 : CEF_STOP;
	DWORD		attr;

	// src の確認/加工
	if (org_path[1] == ':' && org_path[2] != '\\')
		return	ConfirmErr(GetLoadStrW(IDS_BACKSLASHERR), org_path, cef_flg|CEF_NOAPI), FALSE;

	if (::GetFullPathNameW(org_path, MAX_WPATH, src, &fname) == 0)
		return	ConfirmErr(L"GetFullPathName", org_path, cef_flg), FALSE;
	GetRootDirW(src, src_root_cur);

	depthIdxOffset = 0;
	if ((attr = ::GetFileAttributesW(src)) != 0xffffffff && IsDir(attr)) {
		// 親ディレクトリ自体をコピーしない場合、\* を付与
		wcscpy(buf, src);
		MakePathW(src, buf, L"*");
		if (wcsicmp(buf, src_root_cur) && (isExtendDir || IsReparseEx(attr))) {
			src[wcslen(src) - 2] = 0;	// 末尾に \* を付けない
			depthIdxOffset = 1;
		}
	}
	srcPrefixLen = MakeUnlimitedPath((WCHAR *)src);

	if (::GetFullPathNameW(src, MAX_PATH_EX, buf, &fname) == 0 || fname == NULL)
		return	ConfirmErr(L"GetFullPathName2", src + srcPrefixLen, cef_flg), FALSE;

	// 確認用dst生成
	wcscpy(confirmDst + dstBaseLen, fname);

	// 同一パスでないことの確認
	if (wcsicmp(buf, confirmDst) == 0) {
		if (info.mode != DIFFCP_MODE || (info.flags & SAMEDIR_RENAME) == 0) {
			ConfirmErr(GetLoadStrW(IDS_SAMEPATHERR), confirmDst + dstBaseLen,
				CEF_STOP|CEF_NOAPI);
			return	FALSE;
		}
		wcscpy(confirmDst + dstBaseLen, L"*");
		isRename = TRUE;
	}
	else
		isRename = FALSE;

	if (info.mode == MOVE_MODE && IsNoReparseDir(attr)) {	// 親から子への移動は認めない
		int	end_offset = 0;
		if (fname[0] == '*' || attr == 0xffffffff) {
			fname[0] = 0;
			end_offset = 1;
		}
		int		len = (int)wcslen(buf);
		if (wcsnicmp(buf, confirmDst, len) == 0) {
			DWORD ch = confirmDst[len - end_offset];
			if (ch == 0 || ch == '\\') {
				ConfirmErr(GetLoadStrW(IDS_PARENTPATHERR), buf + srcPrefixLen,
					CEF_STOP|CEF_NOAPI);
				return	FALSE;
			}
		}
	}

	fname[0] = 0;
	srcBaseLen = (int)wcslen(buf);

	// src ファイルシステム情報取得
	if (wcsicmp(src_root_cur, src_root)) {
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
			isSameDrv = driveMng.IsSameDrive(src_root_cur, dst_root);
		if (info.mode == MOVE_MODE)
			isSameVol = wcscmp(src_root_cur, dst_root);
	}

	enableAcl = (info.flags & WITH_ACL) && srcFsType != FSTYPE_FAT && dstFsType != FSTYPE_FAT;
	enableStream = (info.flags & WITH_ALTSTREAM) && srcFsType != FSTYPE_FAT
					&& dstFsType != FSTYPE_FAT;
	wcscpy(src_root, src_root_cur);

	// 最大転送サイズ
	size_t tmpSize = size_t(isSameDrv ? info.bufSize : info.bufSize / 4);
	if (tmpSize < maxReadSize) maxReadSize = (DWORD)tmpSize;
	maxReadSize = max(MIN_BUF, maxReadSize);
	maxReadSize = maxReadSize / BIGTRANS_ALIGN * BIGTRANS_ALIGN;
	maxWriteSize = min(maxReadSize, maxWriteSize);
	maxDigestReadSize = min(maxReadSize, maxDigestReadSize);

	// タイムスタンプ同一判断猶予時間設定
	// 片方が NTFS でない場合、1msec 未満の誤差は許容（UDF 対策）
#define UDF_GRACE	10000
	timeDiffGrace = info.timeDiffGrace;
	if ((srcFsType != FSTYPE_NTFS || dstFsType != FSTYPE_NTFS) && UDF_GRACE > timeDiffGrace) {
		timeDiffGrace = UDF_GRACE;
	}

	InitDepthBuf();

	return	TRUE;
}

BOOL FastCopy::SetUseDriveMap(const WCHAR *path)
{
	WCHAR	root[MAX_PATH], *fname = NULL;

	if (path[1] == ':' && path[2] != '\\') return FALSE;
	if (::GetFullPathNameW(path, MAX_WPATH, src, &fname) <= 0) return FALSE;

	GetRootDirW(src, root);
	int idx = driveMng.SetDriveID(root);
	if (idx >= 0) useDrives |= 1ULL << idx;

	return	TRUE;
}



BOOL FastCopy::InitDeletePath(int idx)
{
	WCHAR		wbuf[MAX_PATH_EX];
	WCHAR		*buf = wbuf, *fname = NULL;
	const WCHAR	*org_path = srcArray.Path(idx);
	WCHAR		dst_root[MAX_PATH];
	DWORD		cef_flg = IsStarting() ? 0 : CEF_STOP;
	DWORD		attr;

	// delete 用 path の確認/加工
	if (org_path[1] == ':' && org_path[2] != '\\')
		return	ConfirmErr(GetLoadStrW(IDS_BACKSLASHERR), org_path, cef_flg|CEF_NOAPI), FALSE;

	if (::GetFullPathNameW(org_path, MAX_WPATH, dst, &fname) == 0)
		return	ConfirmErr(L"GetFullPathName", org_path, cef_flg), FALSE;

	attr = ::GetFileAttributesW(dst);

	if (attr != 0xffffffff && IsNoReparseDir(attr)
	|| (info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA))) {
		GetRootDirW(dst, dst_root);
		if (info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA)) {
			dstSectorSize = GetSectorSize(dst_root);
			dstFsType = GetFsType(dst_root);
			nbMinSize = dstFsType == FSTYPE_NTFS ? info.nbMinSizeNtfs : info.nbMinSizeFat;
		}
	}

	depthIdxOffset = 0;
	if (attr != 0xffffffff && IsDir(attr)) {
		wcscpy(buf, dst);
		// root_dir は末尾に "\*" を付与、それ以外は末尾の "\"を削除
		MakePathW(dst, buf, L"*");
		if (IsReparse(attr) || wcsicmp(buf, dst_root)) {
			dst[wcslen(dst) - 2] = 0;
			depthIdxOffset = 1;
		}
	}
	dstPrefixLen = MakeUnlimitedPath((WCHAR *)dst);

	if (::GetFullPathNameW(dst, MAX_PATH_EX, buf, &fname) == 0 || fname == NULL)
		return	ConfirmErr(L"GetFullPathName2", dst + dstPrefixLen, cef_flg), FALSE;
	fname[0] = 0;
	dstBaseLen = (int)wcslen(buf);

	if (info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA)) {
		wcscpy(confirmDst, dst);	// for renaming before deleting

		// 最大転送サイズ
		if (info.bufSize < maxWriteSize) maxWriteSize = (DWORD)info.bufSize;
		maxWriteSize = max(MIN_BUF, maxWriteSize);
	}

	InitDepthBuf();

	return	TRUE;
}

BOOL FastCopy::CleanRegFilter()
{
	for (int i=0; i < MAX_FILEDIR_REG; i++) {
		for (int j=0; j < MAX_INCEXC_REG; j++) {
			for (int k=0; k < MAX_RELABS_REG; k++) {
				RegExpVec &targ = regExpVec[i][j][k];

				while (targ.size() > 0) {
					if (targ.back()) delete targ.back();
					targ.pop_back();
				}
				targ.clear();
			}
		}
	}
	return	TRUE;
}

int CountPathDepth(const WCHAR *path)
{
	int		count = 0;
	for ( ; *path; path++) if (*path == '/' || *path == '\\') count++;
	return	count;
}

BOOL FastCopy::RegisterRegFilterCore(const PathArray *_pathArray, bool is_inc)
{
	PathArray	pathArray(*_pathArray);
	int			incexc_idx  = is_inc ? INC_REG : EXC_REG;
	int			idx; // for ERROR

	for (idx=0; idx < pathArray.Num(); idx++) {
		WCHAR	*path = pathArray.Path(idx);
		int		len   = pathArray.PathLen(idx);
		int		filedir_idx = FILE_REG;
		int		absreg_idx  = REL_REG;

		if (path[0] == '\\' || path[0] == '/') {
			memmove(path, path + 1, len * sizeof(WCHAR));
			absreg_idx = ABS_REG;
			if (--len <= 0) goto ERR;
		}
		if (path[len -1] == '\\' || path[len -1] == '/') {
			path[len -1] = 0;
			filedir_idx = DIR_REG;
			if (--len <= 0) goto ERR;
		}
		if (len > 0) {
			int			depth = CountPathDepth(path);
			RegExpVec	&targ = regExpVec[filedir_idx][incexc_idx][absreg_idx];
			if (targ.size() < depth + 1) targ.resize(depth + 1, NULL);
			if (targ[depth] == NULL) targ[depth] = new RegExp();
			if (!targ[depth]->RegisterWildCard(path, RegExp::CASE_INSENSE_SLASH)) goto ERR;
			filterMode |= FilterBits(filedir_idx, incexc_idx);
		}
	}
	return	TRUE;

ERR:
	ConfirmErr(L"Bad or Too long windcard string", _pathArray->Path(idx), CEF_STOP|CEF_NOAPI);
	return	FALSE;
}

BOOL FastCopy::RegisterRegFilter(const PathArray *incArray, const PathArray *excArray)
{
	CleanRegFilter();

	if (!RegisterRegFilterCore(incArray, true))  return FALSE;
	if (!RegisterRegFilterCore(excArray, false)) return FALSE;
	return	TRUE;
}

BOOL FastCopy::RegisterInfo(const PathArray *_srcArray, const PathArray *_dstArray, Info *_info,
	const PathArray *_includeArray, const PathArray *_excludeArray)
{
	info = *_info;

	isAbort = FALSE;
	isRename = FALSE;
	filterMode = 0;
	useDrives = 0;
	timeDiffGrace = info.timeDiffGrace; // InitSrcPath で最終値に更新

	isListingOnly = (info.flags & LISTING_ONLY) ? TRUE : FALSE;
	isListing = (info.flags & LISTING) || isListingOnly ? TRUE : FALSE;
	endTick = 0;
	depthIdxOffset = 0;

	src_root[0] = 0;

	if (isListingOnly) info.flags &= ~PRE_SEARCH;

	// 最大転送サイズ上限（InitSrcPath で再設定）
	if (info.maxOvlSize <= 0) {
		info.maxOvlSize = (info.maxTransSize / info.maxOvlNum) + MIN_BUF - 1;
		info.maxOvlSize = info.maxOvlSize / MIN_BUF * MIN_BUF;
	}
	maxReadSize = maxWriteSize = maxDigestReadSize = info.maxTransSize = info.maxOvlSize;

	// reg filter
	RegisterRegFilter(_includeArray, _excludeArray);

	if (info.fromDateFilter != 0  || info.toDateFilter  != 0)  filterMode |= DATE_FILTER;
	if (info.minSizeFilter  != -1 || info.maxSizeFilter != -1) filterMode |= SIZE_FILTER;

	int64	need_size = (int64)info.maxOvlSize * info.maxOvlNum * BUFIO_SIZERATIO;
	if (!isListingOnly &&
		(info.mode != DELETE_MODE || (info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA))) &&
		(info.bufSize < need_size || info.bufSize < MIN_BUF * 2
#ifdef _WIN64
		)) {
		return	ConfirmErr(FmtW(L"Too small Main Buffer.\r\n need %I64dMB or over",
			need_size/1024/1024), NULL, CEF_STOP|CEF_NOAPI), FALSE;
#else
		|| info.bufSize > MAX_BUF)) {
		return	ConfirmErr(FmtW(L"Too large or small Main Buffer.\r\n need %I64dMB or over and"
			L" under 1023MB", need_size/1024/1024), NULL, CEF_STOP|CEF_NOAPI), FALSE;
#endif
	}

	if ((info.flags & REPARSE_AS_NORMAL) && (info.mode == MOVE_MODE || info.mode == DELETE_MODE)) {
		return	ConfirmErr(L"Illega Flags (junction/symlink)", NULL, CEF_STOP|CEF_NOAPI), FALSE;
	}

	if (info.flags & RESTORE_HARDLINK) {
		return	ConfirmErr(L"Illega Flags (CreateHardLink)", NULL, CEF_STOP|CEF_NOAPI), FALSE;
	}

	driveMng.Init((DriveMng::NetDrvMode)info.netDrvMode);
	driveMng.SetDriveMap(info.driveMap);

	// set useDrives
	for (int i=0; i < _srcArray->Num(); i++) SetUseDriveMap(_srcArray->Path(i));
	if (info.mode != DELETE_MODE) SetUseDriveMap(_dstArray->Path(0));
	useDrives |= driveMng.OccupancyDrives(useDrives);
	flagOvl = (info.maxOvlNum >= 2) ? FILE_FLAG_OVERLAPPED : 0;

	// command
	if (info.mode == DELETE_MODE) {
		srcArray = *_srcArray;
		if (InitDeletePath(0) == FALSE)
			return	FALSE;
	}
	else {
		srcArray = *_srcArray;
		dstArray = *_dstArray;

		if (InitDstPath() == FALSE)
			return	FALSE;
		if (InitSrcPath(0) == FALSE)
			return	FALSE;
	}
	_info->isRenameMode = isRename;

	return	!isAbort;
}

BOOL FastCopy::AllocBuf(void)
{
	size_t	allocSize = size_t(isListingOnly ? MAX_LIST_BUF : info.bufSize + PAGE_SIZE * 4);
	BOOL	need_mainbuf = info.mode != DELETE_MODE ||
					((info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA)) && !isListingOnly);

	rOvl.Init(info.maxOvlNum);
	wOvl.Init(info.maxOvlNum);

	// メインリングバッファ確保
	if (need_mainbuf && mainBuf.AllocBuf(allocSize) == FALSE) {
		return	ConfirmErr(L"Can't alloc memory(mainBuf)", NULL, CEF_STOP), FALSE;
	}
	usedOffset = freeOffset = mainBuf.Buf();	// リングバッファ用オフセット初期化
#ifdef _DEBUG
	if (need_mainbuf /*&& info.mode == TESTWRITE_MODE*/) {
		uint64 *end = (uint64 *)(mainBuf.Buf() + allocSize);
		uint64 val  = 0xf7f6f5f4f3f2f1f0ULL;
		for (uint64 *p = (uint64 *)mainBuf.Buf(); p < end; p++) {
			*p = val++;
		}
	}
#endif

	if (errBuf.AllocBuf(MIN_ERR_BUF, MAX_ERR_BUF) == FALSE) {
		return	ConfirmErr(L"Can't alloc memory(errBuf)", NULL, CEF_STOP), FALSE;
	}
	if (isListing) {
		if (listBuf.AllocBuf(MIN_PUTLIST_BUF, MAX_PUTLIST_BUF) == FALSE)
			return	ConfirmErr(L"Can't alloc memory(listBuf)", NULL, CEF_STOP), FALSE;
	}
	if (info.mode == DELETE_MODE) {
		if (need_mainbuf) SetupRandomDataBuf();
		goto END;
	}

	openFiles = new FileStat *[info.maxOpenFiles + MAX_ALTSTREAM]; /* for Alternate Stream */
	openFilesCnt = 0;

	if (info.flags & RESTORE_HARDLINK) {
		hardLinkDst = new WCHAR [MAX_WPATH + MAX_PATH];
		memcpy(hardLinkDst, dst, dstBaseLen * sizeof(WCHAR));
		if (!hardLinkList.Init(info.maxLinkHash) || !hardLinkDst)
			return	ConfirmErr(L"Can't alloc memory(hardlink)", NULL, CEF_STOP), FALSE;
	}

	if (IsUsingDigestList()) {
		digestList.Init(MIN_DIGEST_LIST, MAX_DIGEST_LIST, MIN_DIGEST_LIST);
		int require_size = (maxReadSize + BIGTRANS_ALIGN) * (info.maxOvlNum + 1);
		require_size = ALIGN_SIZE(require_size, BIGTRANS_ALIGN);
		if (!wDigestList.Init(MIN_BUF, require_size, PAGE_SIZE))
			return	ConfirmErr(L"Can't alloc memory(digest)", NULL, CEF_STOP), FALSE;
	}

	if (info.flags & VERIFY_FILE) {
		srcDigest.Init((info.flags & VERIFY_MD5) ? TDigest::MD5 : TDigest::SHA1);
		dstDigest.Init((info.flags & VERIFY_MD5) ? TDigest::MD5 : TDigest::SHA1);

		if (isListingOnly) {
			srcDigest.buf.AllocBuf(0, MaxReadDigestBuf());
			dstDigest.buf.AllocBuf(0, MaxReadDigestBuf());
		}
	}

	if (info.mode == MOVE_MODE) {
		if (!moveList.Init(MIN_MOVEPATH_LIST, MAX_MOVEPATH_LIST, MIN_MOVEPATH_LIST))
			return	ConfirmErr(L"Can't alloc memory(moveList)", NULL, CEF_STOP), FALSE;
	}

	if (info.flags & WITH_ALTSTREAM) {
		ntQueryBuf.AllocBuf(MAX_NTQUERY_BUF, MAX_NTQUERY_BUF);
		if (!pNtQueryInformationFile) {
			ConfirmErr(L"Can't load NtQueryInformationFile", NULL, CEF_STOP|CEF_NOAPI);
			return FALSE;
		}
	}

	// src/dst dir-entry/attr 用バッファ確保
	dirStatBuf.AllocBuf(MIN_ATTR_BUF, info.maxDirSize);
	mkdirQueVec.Init(MIN_MKDIRQUEVEC_NUM, MAX_MKDIRQUEVEC_NUM);
	dstDirExtBuf.AllocBuf(MIN_DSTDIREXT_BUF, MAX_DSTDIREXT_BUF);

	fileStatBuf.AllocBuf(MIN_ATTR_BUF, info.maxAttrSize);
	dstStatBuf.AllocBuf(MIN_ATTR_BUF, info.maxAttrSize);
	dstStatIdxVec.Init(MIN_ATTRIDX_BUF, MAX_ATTRIDX_BUF(info.maxAttrSize));

	if (!fileStatBuf.Buf() || !dirStatBuf.Buf() || !dstStatBuf.Buf() || !dstStatIdxVec.Buf()
		|| !mkdirQueVec.Buf() || !dstDirExtBuf.Buf()
		|| (info.flags & WITH_ALTSTREAM) && !ntQueryBuf.Buf()) {
		return	ConfirmErr(L"Can't alloc memory(misc stat)", NULL, CEF_STOP), FALSE;
	}

END:
	if (filterMode & REG_FILTER) {
		srcDepth.Init(MIN_DEPTH_NUM, MAX_DEPTH_NUM);
		dstDepth.Init(MIN_DEPTH_NUM, MAX_DEPTH_NUM);
		cnfDepth.Init(MIN_DEPTH_NUM, MAX_DEPTH_NUM);

		if (!srcDepth.Buf() || !dstDepth.Buf() || !cnfDepth.Buf()) {
			return	ConfirmErr(L"Can't alloc memory(filter)", NULL, CEF_STOP), FALSE;
		}
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
	readInterrupt = writeInterrupt = FALSE;
	dstAsyncRequest = DSTREQ_NONE;
	reqSendCount = 1;
	nextFileID = 1;
	errRFileID = 0;
	errWFileID = 0;
	openFiles = NULL;
	moveFinPtr = NULL;
	runMode = RUN_NORMAL;
	hardLinkDst = NULL;

	cv.Initialize();
	readReqList.Init();
	writeReqList.Init();
	writeWaitList.Init();
	rDigestReqList.Init();

	if (AllocBuf() == FALSE) goto ERR;

	startTick = ::GetTickCount();
	if (ti) GetTransInfo(ti, FALSE);

#define STACK_SIZE (8 * 1024)

	if (info.mode == DELETE_MODE) {
		hReadThread = (HANDLE)_beginthreadex(0, 0, DeleteThread, this, STACK_SIZE, &id);
		if (!hReadThread) goto ERR;
		return	TRUE;
	}

#ifdef _DEBUG
	if (info.mode == TESTWRITE_MODE) {
		if (isListingOnly) goto ERR;
		hWriteThread  = (HANDLE)_beginthreadex(0, 0, WriteThread, this, STACK_SIZE, &id);
		hReadThread   = (HANDLE)_beginthreadex(0, 0, TestThread, this, STACK_SIZE, &id);
		if (!hWriteThread || !hReadThread) goto ERR;
		return	TRUE;
	}
#endif

	hWriteThread  = (HANDLE)_beginthreadex(0, 0, WriteThread, this, STACK_SIZE, &id);
	hReadThread   = (HANDLE)_beginthreadex(0, 0, ReadThread, this, STACK_SIZE, &id);
	if (!hWriteThread || !hReadThread) goto ERR;

	if (IsUsingDigestList()) {
		hRDigestThread = (HANDLE)_beginthreadex(0, 0, RDigestThread, this, STACK_SIZE, &id);
		hWDigestThread = (HANDLE)_beginthreadex(0, 0, WDigestThread, this, STACK_SIZE, &id);
		if (!hRDigestThread || !hWDigestThread) goto ERR;
	}
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

	if (info.flags & PRE_SEARCH)
		PreSearch();

	for (int i=0; i < srcArray.Num(); i++) {
		if (InitSrcPath(i)) {
			if (i >= 1 && isSameDrvOld != isSameDrv) {
				ChangeToWriteMode();
			}
			ReadProc(srcBaseLen, info.overWrite == BY_ALWAYS ? FALSE : TRUE,
						(FilterBits(DIR_REG, INC_REG) & filterMode) ? FR_CONT : FR_MATCH);
			isSameDrvOld = isSameDrv;
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
	WCHAR	*&path = is_delete ? dst : src;
	int		&prefix_len = is_delete ? dstPrefixLen : srcPrefixLen;
	int		&base_len = is_delete ? dstBaseLen : srcBaseLen;
	BOOL	ret = TRUE;

	for (int i=0; i < srcArray.Num(); i++) {
		if ((this->*InitPath)(i)) {
			if (!PreSearchProc(path, prefix_len, base_len,
							   (FilterBits(DIR_REG, INC_REG) & filterMode) ? FR_CONT : FR_MATCH)) {
				ret = FALSE;
			}
		}
		if (isAbort)
			break;
	}
	total.isPreSearch = FALSE;
	startTick = ::GetTickCount();

	return	ret && !isAbort;
}

FilterRes FastCopy::FilterCheck(WCHAR *dir, int dir_len, DWORD attr, const WCHAR *fname,
								int64 wtime, int64 fsize, FilterRes fr)
{
	if (filterMode == 0) return	FR_MATCH;

	BOOL	is_dir = IsDir(attr);

	if (!is_dir) {
		if (wtime < info.fromDateFilter
		|| (info.toDateFilter && wtime > info.toDateFilter)) {
			return	FR_UNMATCH;
		}
		if (fsize < info.minSizeFilter
		|| (info.maxSizeFilter != -1 && fsize > info.maxSizeFilter)) {
			return	FR_UNMATCH;
		}
	}
	if ((filterMode & REG_FILTER) == 0) return FR_MATCH;   // reg filter なし
	if (!is_dir && fr != FR_MATCH)      return FR_UNMATCH; // OK なdir配下でない場合はNG

	wcscpy(dir + dir_len, fname);

	int			file_dir_idx = is_dir ? DIR_REG : FILE_REG;
	VBVec<int>&	depthVec = FindDepth(dir);

	// top level はチェックしない
	if (depthVec.UsedNum() <= depthIdxOffset) {
		if (!is_dir) {
			ConfirmErr(FmtW(L"Depth Error(%d)", depthIdxOffset), 0, CEF_STOP|CEF_NOAPI);
			return	FR_UNMATCH;
		}
		return fr;
	}
	int		*depth = &depthVec[depthIdxOffset];
	int		depthNum = depthVec.UsedNum() - depthIdxOffset;
	int		depthIdx = depthNum - 1;

	// excludeフィルタ
	if ((filterMode & FilterBits(file_dir_idx, EXC_REG))) {
		RegExpVec	&excRel = regExpVec[file_dir_idx][EXC_REG][REL_REG];
		RegExpVec	&excAbs = regExpVec[file_dir_idx][EXC_REG][ABS_REG];
		RegExp		*regExp = NULL;

		if (excRel.size()) {
			int max_idx = min((int)excRel.size(), depthNum) -1;
			for (int i=max_idx; i >= 0; i--) {
				if ((regExp = excRel[i])) {
					if (regExp->IsMatch(dir + depth[depthIdx-i])) return FR_UNMATCH;
				}
			}
		}
		if (excAbs.size() == depthNum) {
			if ((regExp = excAbs[depthIdx])) {
				if (regExp->IsMatch(dir + depth[0])) return FR_UNMATCH;
			}
		}
	}

	// FR_MATCH済みの dir 配下は exclude check が終わっていれば OK
	if (is_dir && fr == FR_MATCH) return FR_MATCH;

	// includeフィルタ (FR_MATCH(file/dir) FR_CONT(dir))
	if ((filterMode & FilterBits(file_dir_idx, INC_REG))) {
		RegExpVec	&incRel = regExpVec[file_dir_idx][INC_REG][REL_REG];
		RegExpVec	&incAbs = regExpVec[file_dir_idx][INC_REG][ABS_REG];
		RegExp		*regExp = NULL;

		if (incRel.size()) {
			int max_idx = min((int)incRel.size(), depthNum) -1;
			for (int i=max_idx; i >= 0; i--) {
				if ((regExp = incRel[i])) {
					if (regExp->IsMatch(dir + depth[depthIdx-i])) return FR_MATCH;
				}
			}
		}
		if (incAbs.size() == depthNum) {
			if ((regExp = incAbs[depthIdx])) {
				if (regExp->IsMatch(dir + depth[0])) return FR_MATCH;
			}
		}
		return	is_dir ? FR_CONT : FR_UNMATCH;
	}

	return fr;
}

inline int wcscpy_with_aster(WCHAR *dst, WCHAR *src)
{
	int	len = wcscpyz(dst, src);
	return	len + (int)wcscpyz(dst + len, L"\\*");
}

BOOL FastCopy::ClearNonSurrogateReparse(WIN32_FIND_DATAW *fdat)
{
	if (!IsReparse(fdat->dwFileAttributes) || IsReparseTagNameSurrogate(fdat->dwReserved0)) {
		return	FALSE;	// 通常ファイル or symlink/junction等
	}

	fdat->dwFileAttributes &= ~FILE_ATTRIBUTE_REPARSE_POINT;
	return	TRUE;
}

BOOL GetFileInformation(const WCHAR *path, BY_HANDLE_FILE_INFORMATION *bhi)
{
	DWORD	share = FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;
	HANDLE	hFile = ::CreateFileW(path, 0, share, 0, OPEN_EXISTING, 0, 0);

	if (hFile == INVALID_HANDLE_VALUE) return FALSE;

	BOOL ret = ::GetFileInformationByHandle(hFile, bhi);
	::CloseHandle (hFile);

	return	ret;
}

BOOL ModifyRealFdat(const WCHAR *path, WIN32_FIND_DATAW *fdat)
{
	BY_HANDLE_FILE_INFORMATION	bhi;

	if (!GetFileInformation(path, &bhi)) return FALSE;

	fdat->nFileSizeHigh		= bhi.nFileSizeHigh;
	fdat->nFileSizeLow		= bhi.nFileSizeLow;
	fdat->ftCreationTime	= bhi.ftCreationTime;
	fdat->ftLastAccessTime	= bhi.ftLastAccessTime;
	fdat->ftLastWriteTime	= bhi.ftLastWriteTime;
	return	TRUE;
}

BOOL FastCopy::PreSearchProc(WCHAR *path, int prefix_len, int dir_len, FilterRes fr)
{
	HANDLE		hDir;
	BOOL		ret = TRUE;
	WIN32_FIND_DATAW fdat;

	if (waitTick) Wait(1);

	if ((hDir = ::FindFirstFileW(path, &fdat)) == INVALID_HANDLE_VALUE) {
		return	ConfirmErr(L"FindFirstFile(pre)", path + prefix_len), FALSE;
	}

	do {
		if (IsParentOrSelfDirs(fdat.cFileName))
			continue;

		ClearNonSurrogateReparse(&fdat);

		FilterRes	cur_fr = FilterCheck(path, dir_len, &fdat, fr);
		if (cur_fr == FR_UNMATCH) continue;

		if (IsDir(fdat.dwFileAttributes)) {
			if (cur_fr == FR_MATCH) total.preDirs++;
			if (!IsReparseEx(fdat.dwFileAttributes)) {
				int new_len = dir_len + wcscpy_with_aster(path + dir_len, fdat.cFileName) -1;
				PushDepth(path, new_len);
				ret = PreSearchProc(path, prefix_len, new_len, cur_fr);
				PopDepth(path);
			}
		}
		else {
			total.preFiles++;
			if (NeedSymlinkDeref(&fdat)) { // del/moveでは REPARSE_AS_NORMAL は存在しない
				wcscpyz(path + dir_len, fdat.cFileName);
				ModifyRealFdat(path, &fdat);
			}
			if (!IsReparseEx(fdat.dwFileAttributes)) {
				total.preTrans += FileSize(fdat);
			}
		}
	}
	while (!isAbort && ::FindNextFileW(hDir, &fdat));

	if (!isAbort && ret && ::GetLastError() != ERROR_NO_MORE_FILES) {
		ret = FALSE;
		ConfirmErr(L"FindNextFile(pre)", path + prefix_len);
	}

	::FindClose(hDir);

	return	ret && !isAbort;
}

BOOL FastCopy::PutList(WCHAR *path, DWORD opt, DWORD lastErr, int64 wtime, int64 fsize,
	BYTE *digest)
{
	BOOL	add_backslash = path[0] == '\\' && path[1] != '\\';

	if (!listBuf.Buf()) return FALSE;

	::EnterCriticalSection(&listCs);

	int require_size = MAX_WPATH + 1024;

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
			WCHAR	*buf = (WCHAR *)(listBuf.Buf() + listBuf.UsedSize());
			len =  wcscpyz(buf,       path);
			len += wcscpyz(buf + len, L"\r\n");
		}
		else {	// 書式化は UI側コンテキストで行いたいところだが…
			WCHAR	wbuf[128];
			if (!(info.fileLogFlags & FILELOG_FILESIZE))  fsize = -1;
			if (!(info.fileLogFlags & FILELOG_TIMESTAMP)) wtime = -1;
			if (opt & PL_REPARSE) {
				fsize = -1;
				digest = NULL;
			}
			if (fsize >= 0 || wtime >= 0 || digest) {
				WCHAR	*start = wbuf + wcscpyz(wbuf, L"   <");
				WCHAR	*p = start;

				if (wtime >= 0) {
					__time64_t t = FileTime2UnixTime((FILETIME *)&wtime);
					struct tm tm;
					_localtime64_s(&tm, &t);
					p += swprintf(p, L"%04d%02d%02d-%02d%02d%02d", tm.tm_year+1900,
						tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
				}
				if (fsize >= 0) {
					if (p != start) *p++ = ' ';
					//p += wcscpyz(p, L"size=");
					p += comma_int64(p, fsize);
				}
				if (digest) {
					int len = dstDigest.GetDigestSize();
					if (p != start) *p++ = ' ';
					p += wcscpyz(p, (len == MD5_SIZE) ? L"md5=" : L"sha1=");
					p += bin2hexstrW(digest, len, p);
				}
				*p++ = '>';
				*p++ = 0;
			}
			else wbuf[0] = 0;

			len = swprintf((WCHAR *)(listBuf.Buf() + listBuf.UsedSize()), FMT_PUTLIST,
							(opt & PL_NOADD) ? ' ' : (opt & PL_DELETE) ? '-' : '+',
							add_backslash ? L"\\" : L"",
							path,
							(opt & PL_DIRECTORY) && (opt & PL_REPARSE) ? PLSTR_REPDIR :
							(opt & PL_DIRECTORY) ? L"\\" :
							(opt & PL_REPARSE) ? PLSTR_REPARSE :
							(opt & PL_HARDLINK) ? PLSTR_LINK :
							(opt & PL_CASECHANGE) ? PLSTR_CASECHANGE :
							(opt & PL_COMPARE) || lastErr ? PLSTR_COMPARE : L"",
							wbuf);
		}
		listBuf.AddUsedSize(len * sizeof(WCHAR));
	}

	::LeaveCriticalSection(&listCs);
	return	TRUE;
}

BOOL DisableLocalBuffering(HANDLE hFile)
{
	if (!pZwFsControlFile) return FALSE;

	IO_STATUS_BLOCK ib ={};
	return !::pZwFsControlFile(hFile, 0, 0, 0, &ib, IOCTL_LMR_DISABLE_LOCAL_BUFFERING, 0, 0, 0, 0);
}

BOOL FastCopy::WaitOverlapped(HANDLE hFile, OverLap *ovl)
{
	if (!ovl->waiting) return ovl->transSize > 0 ? TRUE : FALSE; // already done

	ovl->transSize = 0;
	while (::WaitForSingleObject(ovl->ovl.hEvent, 500) == WAIT_TIMEOUT) {
		if (isAbort) return FALSE;
	}
	ovl->waiting = false;
	return	::GetOverlappedResult(hFile, &ovl->ovl, &ovl->transSize, TRUE) && ovl->transSize;
}

BOOL FastCopy::WaitOvlIo(HANDLE fh, OverLap *ovl, int64 *total_size, int64 *order_total)
{
	if (!WaitOverlapped(fh, ovl)) return FALSE;

	*total_size  += ovl->transSize;
	*order_total -= ovl->orderSize;
	return	TRUE;
}

BOOL FastCopy::MakeDigest(WCHAR *path, DigestBuf *dbuf, FileStat *stat)
{
	int64	file_size = stat->FileSize();
	bool	is_src = (dbuf == &srcDigest);
	bool	is_ovl = (flagOvl && file_size > info.maxOvlSize);
	DWORD	flg = ((info.flags & USE_OSCACHE_READ) ? 0 : FILE_FLAG_NO_BUFFERING)
				| FILE_FLAG_SEQUENTIAL_SCAN | (is_ovl ? flagOvl : 0);
	DWORD	share = FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;
	BOOL	ret = FALSE;
	OvlList	&ovl_list = is_src ? rOvl : wOvl;

	if (ovl_list.TopObj(USED_LIST)) {
		ConfirmErr(L"Not clear ovl_list in MakeDigest", NULL, CEF_STOP);
		return FALSE;
	}
	dbuf->Reset();
	if (file_size == 0 || IsReparseEx(stat->dwFileAttributes)) {
		dbuf->GetEmptyVal(dbuf->val);
		return TRUE;
	}
	memset(dbuf->val, 0, dbuf->GetDigestSize());

	HANDLE	hFile = CreateFileWithRetry(path, GENERIC_READ, share, 0, OPEN_EXISTING, flg, 0, 5);
	if (hFile == INVALID_HANDLE_VALUE) return FALSE;
	if (is_ovl) DisableLocalBuffering(hFile);

	if ((DWORD)dbuf->buf.Size() < MaxReadDigestBuf() && !dbuf->buf.Grow(MaxReadDigestBuf()))
		goto END;

	int64	total_size = 0;
	int64	order_total = 0, dummy;
	int64	&verifyTrans = is_src ? total.verifyTrans : dummy;	// src/dstダブルカウント避け
	DWORD	count = 0;

	while (total_size < file_size && !isAbort) {
		OverLap	*ovl = ovl_list.GetObj(FREE_LIST);

		ovl_list.PutObj(USED_LIST, ovl);
		ovl->buf = dbuf->buf.Buf() + (maxDigestReadSize * (count++ % info.maxOvlNum));
		ovl->SetOvlOffset(total_size + order_total);

		if (!(ret = ReadFileWithReduce(hFile, ovl->buf, maxDigestReadSize, ovl))) break;
		order_total += ovl->orderSize;

		BOOL is_empty_ovl = ovl_list.IsEmpty(FREE_LIST);
		BOOL is_flush = (total_size + order_total >= file_size)
			|| (is_src ? readInterrupt : writeInterrupt) || !ovl->waiting;
		if (!is_empty_ovl && !is_flush) continue;

		while (OverLap	*ovl_tmp = ovl_list.GetObj(USED_LIST)) {
			ovl_list.PutObj(FREE_LIST, ovl_tmp);
			if (!(ret = WaitOvlIo(hFile, ovl_tmp, &total_size, &order_total))) break;
			ret = dbuf->Update(ovl_tmp->buf, ovl_tmp->transSize);
			verifyTrans += ovl_tmp->transSize; 
			if (!is_flush || !ret || isAbort) break;
		}
		if (!ret || isAbort) break;
	}
	if (total_size == file_size) {
		ret = dbuf->GetVal(dbuf->val);
	}
	else if (ret) {	// size over
		ret = FALSE;
	}

	if (ovl_list.TopObj(USED_LIST)) IoAbortFile(hFile, &ovl_list);

END:
	::CloseHandle(hFile);
	return	ret;
}

void MakeVerifyStr(WCHAR *buf, BYTE *digest1, BYTE *digest2, DWORD digest_len)
{
	WCHAR	*p = buf + wcscpyz(buf, L"Verify Error src:");

	p += bin2hexstrW(digest1, digest_len, p);
	p += wcscpyz(p, L" dst:");
	p += bin2hexstrW(digest2, digest_len, p);
	p += wcscpyz(p, L" ");
}

BOOL FastCopy::IsSameContents(FileStat *srcStat, FileStat *dstStat)
{
/*	if (srcStat->FileSize() != dstStat->FileSize()) {
		return;
	}
*/
	if (!isSameDrv) {
		DstRequest(DSTREQ_DIGEST, dstStat);
	}

	BOOL	src_ret = MakeDigest(src, &srcDigest, srcStat);
	BOOL	dst_ret = isSameDrv ? MakeDigest(confirmDst, &dstDigest, dstStat) : WaitDstRequest();
	BOOL	ret = src_ret && dst_ret && memcmp(srcDigest.val, dstDigest.val,
				srcDigest.GetDigestSize()) == 0 ? TRUE : FALSE;

	if (ret) {
		total.verifyFiles++;
		PutList(confirmDst + dstPrefixLen, PL_NOADD|(IsReparseEx(srcStat->dwFileAttributes) ?
			PL_REPARSE : 0), 0, dstStat->WriteTime(), dstStat->FileSize(), srcDigest.val);
	}
	else {
		total.errFiles++;
//		PutList(src + srcPrefixLen, PL_COMPARE|PL_NOADD, 0, srcDigest.val);
//		PutList(confirmDst + dstPrefixLen, PL_COMPARE|PL_NOADD, 0, dstDigest.val);
		if (src_ret && dst_ret) {
			WCHAR	buf[512];
			MakeVerifyStr(buf, srcDigest.val, dstDigest.val, dstDigest.GetDigestSize());
			ConfirmErr(buf, confirmDst + dstPrefixLen, CEF_NOAPI);
		}
		else if (!src_ret) {
			ConfirmErr(L"Can't get src digest", src + srcPrefixLen, CEF_NOAPI);
		}
		else if (!dst_ret) {
			ConfirmErr(L"Can't get dst digest", confirmDst + dstPrefixLen, CEF_NOAPI);
		}
	}

	return	ret;
}

FastCopy::LinkStatus FastCopy::CheckHardLink(WCHAR *path, int len, HANDLE hFileOrg, DWORD *data)
{
	HANDLE		hFile;
	LinkStatus	ret = LINK_ERR;
	DWORD		share = FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;

	if (hFileOrg == INVALID_HANDLE_VALUE) {
		hFile = ::CreateFileW(path, 0, share, 0, OPEN_EXISTING, 0, 0);
		if (hFile == INVALID_HANDLE_VALUE){
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
			if ((obj = new LinkObj(path + srcBaseLen, bhi.nNumberOfLinks, data, len))) {
				hardLinkList.Register(obj, hash_id);
//				DBGWriteW(L"CheckHardLink %08x.%08x.%08x register (%s) %d\n", data[0], data[1],
//				data[2], path + srcBaseLen, bhi.nNumberOfLinks);
				ret = LINK_REGISTER;
			}
			else {
				ConfirmErr(L"Can't malloc(CheckHardLink)", path, CEF_STOP);
			}
		}
		else {
			ret = LINK_ALREADY;
//			DBGWriteW(L"CheckHardLink %08x.%08x.%08x already %s (%s) %d\n", data[0], data[1],
//			data[2], path + srcBaseLen, obj->path, bhi.nNumberOfLinks);
		}
	}
	else {
		ret = LINK_NONE;
	}

	if (hFileOrg == INVALID_HANDLE_VALUE) { // 新たに開いた場合のみクローズ
		::CloseHandle(hFile);
	}
	return	ret;
}

#define MOVE_OPEN_MAX 10
#define WAIT_OPEN_MAX 10

BOOL FastCopy::ReadProc(int dir_len, BOOL confirm_dir, FilterRes fr)
{
	BOOL	ret = TRUE;
	int		curDirStatSize = (int)dirStatBuf.UsedSize(); // カレントのサイズを保存
	BOOL	confirm_local = confirm_dir || isRename;
	int		confirm_len = dir_len + (dstBaseLen - srcBaseLen);

	if (waitTick) Wait(1);

	if (confirm_local && !isSameDrv) DstRequest(DSTREQ_READSTAT, (void *)confirm_len);
	// ディレクトリエントリを先にすべて読み取る
	ret = ReadDirEntry(dir_len, confirm_local, fr);

	if (confirm_local) {
		if (isSameDrv) {
			if (ret) ret = ReadDstStat(confirm_len);
		}
		else {
			if (!WaitDstRequest()) ret = FALSE;
		}
	}
	if (isAbort || !ret) return	FALSE;

	// ファイルを先に処理
	ReadProcFileEntry(dir_len, confirm_local);
	if (isAbort) goto END;

	ReadProcDirEntry(dir_len, curDirStatSize, confirm_dir, fr);
	if (isAbort) goto END;

END:
	// カレントの dir用Buf サイズを復元
	dirStatBuf.SetUsedSize(curDirStatSize);
	return	ret && !isAbort;
}

BOOL FastCopy::ReadProcFileEntry(int dir_len, BOOL confirm_local)
{
	int			confirm_len = dir_len + (dstBaseLen - srcBaseLen);
	FileStat	*statEnd = (FileStat *)(fileStatBuf.Buf() + fileStatBuf.UsedSize());

	for (FileStat *srcStat = (FileStat *)fileStatBuf.Buf(); srcStat < statEnd;
			srcStat = (FileStat *)((BYTE *)srcStat + srcStat->size)) {
		FileStat	*dstStat = confirm_local ? hash.Search(srcStat->upperName, srcStat->hashVal)
											 : NULL;
		int			path_len = 0;
		srcStat->fileID = nextFileID++;

		if (info.mode == MOVE_MODE || (info.flags & RESTORE_HARDLINK)) {
			path_len = dir_len + wcscpyz(src + dir_len, srcStat->cFileName) + 1;
		}

		if (dstStat) {
			if (isRename) {
				SetRenameCount(srcStat);
			} else {
				dstStat->isExists = true;
				srcStat->isCaseChanged = !!wcscmp(srcStat->cFileName, dstStat->cFileName);

				if (!IsOverWriteFile(srcStat, dstStat) && ((info.flags & REPARSE_AS_NORMAL) ||
				(IsReparse(srcStat->dwFileAttributes) == IsReparse(dstStat->dwFileAttributes)))) {
/* 比較モード */	if (isListingOnly && (info.flags & VERIFY_FILE)) {
						wcscpy(confirmDst + confirm_len, srcStat->cFileName);
						wcscpy(src + dir_len, srcStat->cFileName);
						if (!IsSameContents(srcStat, dstStat) && !isAbort) {
//							PutList(confirmDst + dstPrefixLen, PL_COMPARE|PL_NOADD);
						}
/* 比較モード */	}
					if (info.mode == MOVE_MODE) {
						 PutMoveList(src, path_len, srcStat, MoveObj::DONE);
					}
					total.skipFiles++;
					total.skipTrans += dstStat->FileSize();
					if (info.flags & RESTORE_HARDLINK) {
						CheckHardLink(src, path_len);
					}
					if (srcStat->isCaseChanged) {
						if (mkdirQueVec.UsedNum()) {
							ExecMkDirQueue();
						}
						SendRequest(CASE_CHANGE, 0, srcStat);
					}
					continue;
				}
				if (IsReparse(dstStat->dwFileAttributes) && (!IsReparse(srcStat->dwFileAttributes)
					|| (info.flags & REPARSE_AS_NORMAL))) {
					srcStat->isNeedDel = true;
				}
			}
		}
		total.readFiles++;
		if (mkdirQueVec.UsedNum()) {
			ExecMkDirQueue();
		}
		if (!OpenFileProc(srcStat, dir_len) || srcFsType == FSTYPE_NETWORK
			|| info.mode == MOVE_MODE && openFilesCnt >= MOVE_OPEN_MAX
			|| waitTick && openFilesCnt >= WAIT_OPEN_MAX || openFilesCnt >= info.maxOpenFiles) {
			ReadMultiFilesProc(dir_len);
			CloseMultiFilesProc();
		}
		if (isAbort) break;
	}
	ReadMultiFilesProc(dir_len);
	CloseMultiFilesProc();

	return	!isAbort;
}

BOOL FastCopy::ReadProcDirEntry(int dir_len, int dirst_start, BOOL confirm_dir, FilterRes fr)
{
	BOOL		ret = TRUE;
	int			confirm_len = dir_len + (dstBaseLen - srcBaseLen);
	BOOL		confirm_local = confirm_dir || isRename;
	FileStat	*statEnd = (FileStat *)(dirStatBuf.Buf() + dirStatBuf.UsedSize());

	// ディレクトリの存在確認
	if (confirm_local) {
		for (FileStat *srcStat = (FileStat *)(dirStatBuf.Buf() + dirst_start); srcStat < statEnd;
				srcStat = (FileStat *)((BYTE *)srcStat + srcStat->size)) {
			FileStat	*dstStat = hash.Search(srcStat->upperName, srcStat->hashVal);

			if (dstStat) {
				srcStat->isCaseChanged = !!wcscmp(srcStat->cFileName, dstStat->cFileName);
				if (isRename)	SetRenameCount(srcStat, TRUE);
				else			srcStat->isExists = dstStat->isExists = true;
			}
			else srcStat->isExists = false;
		}
	}

	// SYNCモードの場合、コピー元に無いファイルを削除
	if (confirm_local && info.mode == SYNCCP_MODE) {
		for (int i=0; i < dstStatIdxVec.UsedNum(); i++) {
			FileStat	*dstStat = dstStatIdxVec.Get(i);
			if (dstStat->isExists) continue;

			FilterRes	cur_fr = FilterCheck(confirmDst, confirm_len, dstStat, fr);
			if (cur_fr != FR_MATCH) {
				total.filterDstSkips++;
				continue;
			}
			if (isSameDrv) {
				if (IsDir(dstStat->dwFileAttributes)) {
					ret = DeleteDirProc(confirmDst, confirm_len, dstStat->cFileName, dstStat,
										cur_fr);
				} else {
					ret = DeleteFileProc(confirmDst, confirm_len, dstStat->cFileName, dstStat);
				}
			} else {
				if (mkdirQueVec.UsedNum()) {
					ExecMkDirQueue();	// into dir
				}
				SendRequest(DELETE_FILES, 0, dstStat);
			}
			if (isAbort) return FALSE;
		}
	}

	// ディレクトリ処理
	isRename = FALSE;	// top level より下では無効
	for (FileStat *srcStat = (FileStat *)(dirStatBuf.Buf() + dirst_start);
			srcStat < statEnd; srcStat = (FileStat *)((BYTE *)srcStat + srcStat->size)) {
		bool	is_reparse = IsReparseEx(srcStat->dwFileAttributes);
		int		cur_skips = total.filterSrcSkips;
		total.readDirs++;

		// -1 is remove '*'
		int		new_dir_len = dir_len + wcscpy_with_aster(src + dir_len, srcStat->cFileName) - 1;
		bool	need_extra = false;

		srcStat->fileID = nextFileID++;
		PushDepth(src, new_dir_len);
		if (confirm_dir && srcStat->isExists) {
			int len = wcscpy_with_aster(confirmDst + confirm_len, srcStat->cFileName) - 1;
			PushDepth(confirmDst, confirm_len + len);
		}
		if (!isListingOnly && (enableAcl || is_reparse))  need_extra = true;

		if (!PushMkdirQueue(srcStat, new_dir_len-1, (!confirm_dir || !srcStat->isExists),
							need_extra, is_reparse)) return FALSE;	// VBuf error
		if (srcStat->filterRes == FR_MATCH) {
			if (is_reparse || ((info.flags & SKIP_EMPTYDIR) == 0)) {
				ExecMkDirQueue();
			}
		}
		if (!is_reparse) {
			ReadProc(new_dir_len, confirm_dir && srcStat->isExists, srcStat->filterRes);
			if (isAbort) return FALSE;
			if (mkdirQueVec.UsedNum() == 0) SendRequest(RETURN_PARENT);
			if (isAbort) return FALSE;
		}
		PopDepth(src);
		if (confirm_dir && srcStat->isExists) {
			PopDepth(confirmDst);
		}
		if (mkdirQueVec.UsedNum() > 0) mkdirQueVec.Pop();

		if (info.mode == MOVE_MODE) {
			src[new_dir_len-1] = 0;
			if (cur_skips == total.filterSrcSkips) {
				PutMoveList(src, new_dir_len, srcStat,
				/*srcStat->isExists ? MoveObj::DONE : MoveObj::START*/ MoveObj::DONE);
			}
		}
	}
	return	ret && !isAbort;
}

BOOL FastCopy::PushMkdirQueue(FileStat *stat, int dlen, bool is_mkdir, bool extra, bool is_reparse)
{
	int	idx = mkdirQueVec.UsedNum();
	if (!mkdirQueVec.Aquire(idx)) {
		ConfirmErr(L"Can't alloc memory(mkdirQueVec)", NULL, CEF_STOP);
		return	FALSE;
	}
	MkDirInfo &mkdir_info = mkdirQueVec.Get(idx);
	mkdir_info.stat      = stat;
	mkdir_info.dirLen    = dlen;
	mkdir_info.isMkDir   = is_mkdir;
	mkdir_info.needExtra = extra;
	mkdir_info.isReparse = is_reparse;
	return	TRUE;
}

BOOL FastCopy::ExecMkDirQueue(void)
{
	int		max_num = mkdirQueVec.UsedNum();

	for (int i=0; i < max_num; i++) {
		MkDirInfo	&info = mkdirQueVec.Get(i);
		ReqHead		*req = NULL;

		if (info.needExtra) {
			src[info.dirLen] = 0;
			req = GetDirExtData(info.stat); // extraを読めずとも mkdir は発行する
			src[info.dirLen] = '\\';
			// if (info.isReparse && info.stat->rep == NULL) return FALSE;
		}
		SendRequest(info.isMkDir ? MKDIR : INTODIR, req, info.stat);
		if (isAbort) return FALSE;
	}
	mkdirQueVec.SetUsedNum(0);
	return	TRUE;
}

BOOL FastCopy::PutMoveList(WCHAR *path, int path_len, FileStat *stat, MoveObj::Status status)
{
	int	path_size = path_len * sizeof(WCHAR);

	moveList.Lock();
	DataList::Head	*head = moveList.Alloc(NULL, 0, sizeof(MoveObj) + path_size);

	if (head) {
		MoveObj	*data = (MoveObj *)head->data;
		data->fileID = stat->fileID;
		data->fileSize = stat->FileSize();
		data->wTime = stat->WriteTime();
		data->dwAttr = stat->dwFileAttributes;
		data->status = status;
		memcpy(data->path, path, path_size);
	}
	if (moveList.IsWait()) {
		moveList.Notify();
	}
	moveList.UnLock();

	if (!head) {
		ConfirmErr(L"Can't alloc memory(moveList)", NULL, CEF_STOP);
		return	FALSE;
	}

	FlushMoveList(FALSE);
	return	TRUE;
}

void FastCopy::FlushMoveListCore(MoveObj *data)
{
	int 	prefix_len = (data->path[5] == ':') ? PATH_LOCAL_PREFIX_LEN : PATH_UNC_PREFIX_LEN;

	if (data->dwAttr & FILE_ATTRIBUTE_DIRECTORY) {
		if (!isListingOnly && (data->dwAttr & FILE_ATTRIBUTE_READONLY)) { // data->path's stat
			::SetFileAttributesW(data->path, FILE_ATTRIBUTE_DIRECTORY);
			// ResetAcl()
		}
		if (!isListingOnly && ForceRemoveDirectoryW(data->path, info.aclReset) == FALSE) {
			total.errDirs++;
			ConfirmErr(L"RemoveDirectory(move)", data->path + prefix_len);
		} else {
			total.deleteDirs++;
			if (isListing) {
				PutList(data->path + prefix_len, PL_DIRECTORY|PL_DELETE|
					(IsReparseEx(data->dwAttr) ? PL_REPARSE : 0), 0, data->wTime);
			}
		}
	} else {
		if (!isListingOnly && (data->dwAttr & FILE_ATTRIBUTE_READONLY)) { // data->path's stat
			::SetFileAttributesW(data->path, FILE_ATTRIBUTE_NORMAL);
			// ResetAcl()
		}
		if (!isListingOnly && ForceDeleteFileW(data->path, info.aclReset) == FALSE) {
			total.errFiles++;
			ConfirmErr(L"DeleteFile(move)", data->path + prefix_len);
		} else {
			total.deleteFiles++;
			total.deleteTrans += data->fileSize;
			if (isListing) {
				PutList(data->path + prefix_len, PL_DELETE|
					(IsReparseEx(data->dwAttr) ? PL_REPARSE : 0), 0,data->wTime, data->fileSize);
			}
		}
	}
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

		while ((head = moveList.Peek()) && !isAbort) {
			MoveObj	*data = (MoveObj *)head->data;

			if (data->status == MoveObj::START) break;
			if (data->status == MoveObj::DONE) {
				moveList.UnLock();
				FlushMoveListCore(data);
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

		if (moveList.Num() == 0 || is_nowait || isAbort || (!is_finish &&
			moveList.RemainSize() > moveList.MinMargin() && (!isSameDrv || is_nextexit))) {
			break;
		}
		cv.Lock();
		if ((info.flags & VERIFY_FILE) && runMode != RUN_DIGESTREQ) {
			if (runMode != RUN_FINISH) {
				runMode = RUN_DIGESTREQ;
			}
			cv.Notify();
		}
		if (isSameDrv) {
			ChangeToWriteModeCore(is_finish);
			is_nextexit = TRUE;
		} else {
			require_sleep = TRUE;
		}
		cv.UnLock();
	}

	return	!isAbort;
}

FastCopy::ReqHead *FastCopy::GetDirExtData(FileStat *stat)
{
	HANDLE	fh;
	WIN32_STREAM_ID	sid;
	DWORD	size, lowSeek, highSeek;
	void	*context = NULL;
	BYTE	streamName[MAX_PATH * sizeof(WCHAR)];
	BOOL	ret = TRUE;
	BOOL	is_reparse = IsReparseEx(stat->dwFileAttributes);
	int		used_size_save = (int)dirStatBuf.UsedSize();
	DWORD	mode = GENERIC_READ|READ_CONTROL;
	DWORD	share = FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;
	DWORD	flg = FILE_FLAG_BACKUP_SEMANTICS | (is_reparse ? FILE_FLAG_OPEN_REPARSE_POINT : 0);
	ReqHead	*req;

	fh = ::CreateFileW(src, mode, share, 0, OPEN_EXISTING, flg , 0);
	if (fh == INVALID_HANDLE_VALUE) {
		//  FILE_READ_EA|FILE_READ_ATTRIBUTES|SYNCHRONIZE
		mode &= ~GENERIC_READ;
		fh = ::CreateFileW(src, mode, share, 0, OPEN_EXISTING, flg , 0);
	}
	if (fh == INVALID_HANDLE_VALUE) {
		if (is_reparse || (info.flags & REPORT_ACL_ERROR)) {
			ConfirmErr(is_reparse ? L"OpenDir (reparse)" : L"OpenDir(ACL/EADATA)",
				src + srcPrefixLen);
		}
		return	NULL;
	}

	if (is_reparse) {
		size = MAXIMUM_REPARSE_DATA_BUFFER_SIZE;
		if (dirStatBuf.RemainSize() <= (int)size + maxStatSize
		&& dirStatBuf.Grow(ALIGN_SIZE(size + maxStatSize, MIN_ATTR_BUF)) == FALSE) {
			ret = FALSE;
			ConfirmErr(L"Can't alloc memory(dirStatBuf)", NULL, CEF_STOP);
		}
		else if ((size = ReadReparsePoint(fh, dirStatBuf.Buf() + dirStatBuf.UsedSize(), size))
				<= 0) {
			ret = FALSE;
			total.errDirs++;
			ConfirmErr(L"ReadReparse(Dir)", src + srcPrefixLen);
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
				ConfirmErr(L"BackupRead(DIR)", src + srcPrefixLen);
			break;
		}
		if (size == 0) break;	// 通常終了

		if (sid.dwStreamNameSize && !(ret = ::BackupRead(fh, streamName, sid.dwStreamNameSize,
				&size, FALSE, TRUE, &context))) {
			if (info.flags & REPORT_ACL_ERROR)
				ConfirmErr(L"BackupRead(name)", src + srcPrefixLen);
			break;
		}
		if (sid.dwStreamId == BACKUP_SECURITY_DATA || sid.dwStreamId == BACKUP_EA_DATA) {
			BYTE    *&data = sid.dwStreamId==BACKUP_SECURITY_DATA ? stat->acl     : stat->ead;
			int &data_size = sid.dwStreamId==BACKUP_SECURITY_DATA ? stat->aclSize : stat->eadSize;

			if (data || sid.Size.HighPart) {	// すでに格納済み
				if (info.flags & REPORT_ACL_ERROR)
					ConfirmErr(L"Duplicate or Too big ACL/EADATA(dir)", src + srcPrefixLen);
				break;
			}
			data = dirStatBuf.Buf() + dirStatBuf.UsedSize();
			data_size = sid.Size.LowPart + STRMID_OFFSET;
			dirStatBuf.AddUsedSize(data_size);
			if (dirStatBuf.RemainSize() <= maxStatSize
			&& !dirStatBuf.Grow(ALIGN_SIZE(maxStatSize + data_size, MIN_ATTR_BUF))) {
				ConfirmErr(L"Can't alloc memory(dirStat(ACL/EADATA))",
					src + srcPrefixLen, FALSE);
				break;
			}
			memcpy(data, &sid, STRMID_OFFSET);
			if (!(ret = ::BackupRead(stat->hFile, data + STRMID_OFFSET, sid.Size.LowPart, &size,
						FALSE, TRUE, &context)) || size <= 0) {
				if (info.flags & REPORT_ACL_ERROR)
					ConfirmErr(L"BackupRead(DirACL/EADATA)", src + srcPrefixLen);
				break;
			}
			sid.Size.LowPart = 0;
		}
		if ((sid.Size.LowPart || sid.Size.HighPart)
		&& !(ret = ::BackupSeek(fh, sid.Size.LowPart, sid.Size.HighPart, &lowSeek, &highSeek,
					&context))) {
			if (info.flags & REPORT_ACL_ERROR)
				ConfirmErr(L"BackupSeek(DIR)", src + srcPrefixLen);
			break;
		}
	}
	if (enableAcl) {
		::BackupRead(fh, NULL, NULL, NULL, TRUE, TRUE, &context);
	}
	::CloseHandle(fh);

	if (ret) {
		size = stat->aclSize + stat->eadSize + stat->repSize;
		req = PrepareReqBuf(offsetof(ReqHead, stat) + stat->minSize, size, stat->fileID);
		if (req && size > 0) {
			BYTE	*data = req->buf;
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

	return	req;
}


int FastCopy::MakeRenameName(WCHAR *buf, int count, WCHAR *fname, BOOL is_dir)
{
	WCHAR	*ext = is_dir ? NULL : wcsrchr(fname, '.');
	int		body_limit = ext ? int(ext - fname) : MAX_PATH;

	return	swprintf(buf, FMT_RENAME, body_limit, fname, count, ext ? ext : L"");
}

BOOL FastCopy::SetRenameCount(FileStat *stat, BOOL is_dir)
{
	while (1) {
		WCHAR	new_name[MAX_PATH];
		int		len = MakeRenameName(new_name, ++stat->renameCount, stat->upperName, is_dir) + 1;
		DWORD	hash_val = MakeHash(new_name, len * sizeof(WCHAR));
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
	if (info.debugFlags & OVERWRITE_JUDGE_LOGGING) {
		WCHAR	buf[512];
		swprintf(buf, L"DBG(compare): mode:%d flag=%x fs=%x/%x grace=%llx mtime=%llx/%llx"
			L" ctime=%llx/%llx size=%llx/%llx fname=%.99s", info.overWrite, info.flags
			, srcFsType, dstFsType, info.timeDiffGrace, srcStat->WriteTime()
			, dstStat->WriteTime(), srcStat->CreateTime(), dstStat->CreateTime()
			, srcStat->FileSize(), dstStat->FileSize(), srcStat->cFileName);
		PutList(buf, PL_ERRMSG);
	}

	if (info.overWrite == BY_NAME)
		return	FALSE;

	if (info.overWrite == BY_ATTR) {
		if (dstStat->FileSize() == srcStat->FileSize()) {	// サイズが等しく、かつ...
			// 更新日付が同じ場合は更新しない
			if (dstStat->WriteTime() + timeDiffGrace >= srcStat->WriteTime() &&
				dstStat->WriteTime() - timeDiffGrace <= srcStat->WriteTime() &&
				((info.flags & COMPARE_CREATETIME) == 0
			||	dstStat->CreateTime() + timeDiffGrace >= srcStat->CreateTime() &&
				dstStat->CreateTime() - timeDiffGrace <= srcStat->CreateTime())) {
				return	FALSE;
			}
			// どちらかが NTFS でない場合（ネットワークドライブを含む）
			if (srcFsType != FSTYPE_NTFS || dstFsType != FSTYPE_NTFS) {
				// src か dst のタイムスタンプの最小単位が 1秒以上（FAT/SAMBA 等）かつ
				if ((srcStat->WriteTime() % 10000000) == 0
				||  (dstStat->WriteTime() % 10000000) == 0) {
					// タイムスタンプの差が 2 秒以内なら、 同一タイムスタンプとみなす
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
		if (dstStat->WriteTime() + timeDiffGrace >= srcStat->WriteTime() &&
			((info.flags & COMPARE_CREATETIME) == 0 ||
			dstStat->CreateTime() + timeDiffGrace >= srcStat->CreateTime())) {
			return	FALSE;
		}
		// どちらかが NTFS でない場合（ネットワークドライブを含む）
		if (srcFsType != FSTYPE_NTFS || dstFsType != FSTYPE_NTFS) {
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

	return	ConfirmErr(L"Illegal overwrite mode", 0, CEF_STOP|CEF_NOAPI), FALSE;
}

BOOL FastCopy::ReadDirEntry(int dir_len, BOOL confirm_dir, FilterRes fr)
{
	HANDLE	fh;
	BOOL	ret = TRUE;
	int		len;
	WIN32_FIND_DATAW fdat;

	fileStatBuf.SetUsedSize(0);

	if ((fh = ::FindFirstFileW(src, &fdat)) == INVALID_HANDLE_VALUE) {
		total.errDirs++;
		return	ConfirmErr(L"FindFirstFile", src + srcPrefixLen), FALSE;
	}
	do {
		if (IsParentOrSelfDirs(fdat.cFileName))
			continue;

		ClearNonSurrogateReparse(&fdat);

		FilterRes	cur_fr = FilterCheck(src, dir_len, &fdat, fr);
		if (cur_fr == FR_UNMATCH) {
			total.filterSrcSkips++;
			continue;
		}
		// ディレクトリ＆ファイル情報の蓄積
		if (IsDir(fdat.dwFileAttributes)) {
			len = FdatToFileStat(&fdat, (FileStat *)(dirStatBuf.Buf() + dirStatBuf.UsedSize()),
								 confirm_dir, cur_fr);
			dirStatBuf.AddUsedSize(len);
			if (dirStatBuf.RemainSize() <= maxStatSize && !dirStatBuf.Grow(MIN_ATTR_BUF)) {
				ConfirmErr(L"Can't alloc memory(dirStatBuf)", NULL, CEF_STOP);
				break;
			}
		}
		else {
			if (NeedSymlinkDeref(&fdat)) { // del/moveでは REPARSE_AS_NORMAL は存在しない
				wcscpyz(src + dir_len, fdat.cFileName);
				ModifyRealFdat(src, &fdat);
			}
			len = FdatToFileStat(&fdat, (FileStat *)(fileStatBuf.Buf() + fileStatBuf.UsedSize()),
								 confirm_dir, cur_fr);
			fileStatBuf.AddUsedSize(len);
			if (fileStatBuf.RemainSize() <= maxStatSize && !fileStatBuf.Grow(MIN_ATTR_BUF)) {
				ConfirmErr(L"Can't alloc memory(fileStatBuf)", NULL, CEF_STOP);
				break;
			}
		}
	}
	while (!isAbort && ::FindNextFileW(fh, &fdat));

	if (!isAbort && ::GetLastError() != ERROR_NO_MORE_FILES) {
		total.errDirs++;
		ret = FALSE;
		ConfirmErr(L"FindNextFile", src + srcPrefixLen);
	}
	::FindClose(fh);

	return	ret && !isAbort;
}

BOOL FastCopy::OpenFileProc(FileStat *stat, int dir_len)
{
	DWORD	name_len = (DWORD)wcslen(stat->cFileName);
	memcpy(src + dir_len, stat->cFileName, ((name_len + 1) * sizeof(WCHAR)));

	openFiles[openFilesCnt++] = stat;

	if (waitTick) Wait((waitTick + 9) / 10);

	if (isListingOnly)	return	TRUE;

	BOOL	is_backup = enableAcl || enableStream;
	BOOL	is_reparse = IsReparseEx(stat->dwFileAttributes);
	BOOL	is_open = is_backup || is_reparse || stat->FileSize() > 0;
	BOOL	is_ovl = (is_open && !is_reparse && flagOvl && stat->FileSize() > info.maxOvlSize);
	BOOL	ret = TRUE;

	if (is_open) {
		DWORD	mode = GENERIC_READ;
		DWORD	flg = ((info.flags & USE_OSCACHE_READ) ?
					0 : FILE_FLAG_NO_BUFFERING) | FILE_FLAG_SEQUENTIAL_SCAN;
		DWORD	share = FILE_SHARE_READ | ((info.flags & WRITESHARE_OPEN) ?
			(FILE_SHARE_WRITE|FILE_SHARE_DELETE) : 0);

		stat->hOvlFile = INVALID_HANDLE_VALUE;

		if (is_backup) {
			mode |= READ_CONTROL;
			flg  |= FILE_FLAG_BACKUP_SEMANTICS;
		}
		else if (is_ovl) {
			flg |= flagOvl;
		}
		if (is_reparse) {
			flg |= FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT;
		}
		if ((stat->hFile = ::CreateFileW(src, mode, share, 0, OPEN_EXISTING, flg, 0))
				== INVALID_HANDLE_VALUE) {
			stat->lastError = ::GetLastError();

			if (stat->lastError == ERROR_SHARING_VIOLATION && (share & FILE_SHARE_WRITE) == 0) {
				share |= FILE_SHARE_WRITE;
				stat->hFile = ::CreateFileW(src, mode, share, 0, OPEN_EXISTING, flg, 0);
			}
		}
		if (stat->hFile == INVALID_HANDLE_VALUE) {
			ret = FALSE;
		} else if ((share & FILE_SHARE_WRITE)) {
			stat->isWriteShare = true;
		}

		if (ret && is_ovl) {
			if (is_backup) {	// エラーの場合は hFile が使われる
				stat->hOvlFile = ::CreateFileW(src, mode, share, 0, OPEN_EXISTING, flg|flagOvl, 0);
			} else {
				stat->hOvlFile = stat->hFile;
			}
			if (stat->hOvlFile != INVALID_HANDLE_VALUE) {
				DisableLocalBuffering(stat->hOvlFile);
			}
		}
	}
	if (ret && is_backup) {
		ret = OpenFileBackupProc(stat, dir_len + name_len);
	}

	return	ret;

}

BOOL FastCopy::OpenFileBackupProc(FileStat *stat, int src_len)
{
	BOOL	ret = FALSE;
	void	*context = NULL;
	int		altdata_cnt = 0;
	BOOL	altdata_local = FALSE;

	while (1) {
		DWORD			size = 0;
		WIN32_STREAM_ID	sid;

		ret = ::BackupRead(stat->hFile, (LPBYTE)&sid, STRMID_OFFSET, &size, FALSE, TRUE, &context);
		if (!ret || size != 0 && size != STRMID_OFFSET) {
			if (info.flags & (REPORT_ACL_ERROR|REPORT_STREAM_ERROR))
				ConfirmErr(L"BackupRead(head)", src + srcPrefixLen);
			break;
		}
		if (size == 0) break;

		WCHAR	streamName[MAX_PATH];
		if (sid.dwStreamNameSize) {
			if (!(ret = ::BackupRead(stat->hFile, (BYTE *)streamName, sid.dwStreamNameSize, &size,
					FALSE, TRUE, &context))) {
				if (info.flags & REPORT_STREAM_ERROR)
					ConfirmErr(L"BackupRead(name)", src + srcPrefixLen);
				break;
			}
			// terminate されないため（dwStreamNameSize はバイト数）
			*(WCHAR *)((BYTE *)streamName + sid.dwStreamNameSize) = 0;
		}

		if (sid.dwStreamId == BACKUP_ALTERNATE_DATA && enableStream && !altdata_local) {
			ret = OpenFileBackupStreamCore(src_len, sid.Size.QuadPart, streamName,
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
					ConfirmErr(L"Duplicate or Too big ACL/EADATA", src + srcPrefixLen);
				break;
			}
			data = fileStatBuf.Buf() + fileStatBuf.UsedSize();
			data_size = sid.Size.LowPart + STRMID_OFFSET;
			fileStatBuf.AddUsedSize(data_size);
			if (fileStatBuf.RemainSize() <= maxStatSize
			&& !fileStatBuf.Grow(ALIGN_SIZE(maxStatSize + data_size, MIN_ATTR_BUF))) {
				ConfirmErr(L"Can't alloc memory(fileStat(ACL/EADATA))",
					src + srcPrefixLen, CEF_STOP);
				break;
			}
			memcpy(data, &sid, STRMID_OFFSET);
			if (!(ret = ::BackupRead(stat->hFile, data + STRMID_OFFSET, sid.Size.LowPart,
							&size, FALSE, TRUE, &context)) || size <= 0) {
				if (info.flags & REPORT_ACL_ERROR)
					ConfirmErr(L"BackupRead(ACL/EADATA)", src + srcPrefixLen);
				break;
			}
			sid.Size.LowPart = 0;
		}
		if (sid.Size.LowPart || sid.Size.HighPart) {
			DWORD	lowSeek, highSeek;
			ret = ::BackupSeek(stat->hFile, sid.Size.LowPart, sid.Size.HighPart,
				&lowSeek, &highSeek, &context);
			if (!ret) {
				if (info.flags & (REPORT_ACL_ERROR|REPORT_STREAM_ERROR)) {
					ConfirmErr(L"BackupSeek", src + srcPrefixLen);
				}
				break;
			}
		}
	}
	::BackupRead(stat->hFile, 0, 0, 0, TRUE, FALSE, &context);

	if (ret)	total.readAclStream++;
	else		total.errAclStream++;
	return	ret;
}


BOOL FastCopy::OpenFileBackupStreamLocal(FileStat *stat, int src_len, int *altdata_cnt)
{
	IO_STATUS_BLOCK	is;

	if (pNtQueryInformationFile(stat->hFile, &is, ntQueryBuf.Buf(),
								(int)ntQueryBuf.Size(), FileStreamInformation) < 0) {
	 	if (info.flags & (REPORT_STREAM_ERROR)) {
			ConfirmErr(L"NtQueryInformationFile", src + srcPrefixLen);
		}
		return	FALSE;
	}

	BOOL	ret = TRUE;
	FILE_STREAM_INFORMATION	*fsi, *next_fsi;

	for (fsi = (FILE_STREAM_INFORMATION *)ntQueryBuf.Buf(); fsi; fsi = next_fsi) {
		next_fsi = fsi->NextEntryOffset == 0 ? NULL :
			(FILE_STREAM_INFORMATION *)((BYTE *)fsi + fsi->NextEntryOffset);

		if (fsi->StreamName[1] == ':') continue; // skip main stream

		if (!OpenFileBackupStreamCore(src_len, fsi->StreamSize.QuadPart, fsi->StreamName,
				fsi->StreamNameLength, altdata_cnt)) {
			ret = FALSE;
			break;
		}
	}
	return	ret;
}

BOOL FastCopy::OpenFileBackupStreamCore(int src_len, int64 size, WCHAR *altname, int altnamesize,
		int *altdata_cnt)
{
	if (++(*altdata_cnt) >= MAX_ALTSTREAM) {
		if (info.flags & REPORT_STREAM_ERROR) {
			ConfirmErr(L"Too Many AltStream", src + srcPrefixLen, CEF_NOAPI);
		}
		return	FALSE;
	}

	FileStat	*subStat = (FileStat *)(fileStatBuf.Buf() + fileStatBuf.UsedSize());
	bool		is_ovl = (size > info.maxOvlSize) && flagOvl;
	DWORD		share = FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;
	DWORD		flg = ((info.flags & USE_OSCACHE_READ) ? 0 : FILE_FLAG_NO_BUFFERING)
					| FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_BACKUP_SEMANTICS
					| (is_ovl ? flagOvl : 0);

	openFiles[openFilesCnt++] = subStat;
	subStat->fileID = nextFileID++;
	subStat->hFile = INVALID_HANDLE_VALUE;
	subStat->hOvlFile = INVALID_HANDLE_VALUE;
	subStat->SetFileSize(size);
	subStat->dwFileAttributes = 0;	// ALTSTREAM
	subStat->renameCount = 0;
	subStat->lastError = 0;
	subStat->size = altnamesize + sizeof(WCHAR) + offsetof(FileStat, cFileName);
	subStat->minSize = ALIGN_SIZE(subStat->size, 8);

	fileStatBuf.AddUsedSize(subStat->minSize);
	if (fileStatBuf.RemainSize() <= maxStatSize && !fileStatBuf.Grow(MIN_ATTR_BUF)) {
		ConfirmErr(L"Can't alloc memory(fileStatBuf2)", NULL, CEF_STOP);
		return	FALSE;
	}
	memcpy(subStat->cFileName, altname, altnamesize + sizeof(WCHAR));
	memcpy(src + src_len, subStat->cFileName, altnamesize + sizeof(WCHAR));

	if ((subStat->hFile = ::CreateFileW(src, GENERIC_READ|READ_CONTROL, share, 0, OPEN_EXISTING
			, flg, 0)) == INVALID_HANDLE_VALUE) {
		if (info.flags & REPORT_STREAM_ERROR) ConfirmErr(L"OpenFile(st)", src + srcPrefixLen);
		subStat->lastError = ::GetLastError();
		return	FALSE;
	}
	else if (is_ovl) {
		subStat->hOvlFile = subStat->hFile;
		DisableLocalBuffering(subStat->hFile);
	}

	return	TRUE;
}


BOOL FastCopy::ReadMultiFilesProc(int dir_len)
{
	for (int i=0; !isAbort && i < openFilesCnt; ) {
		ReadFileProc(&i, dir_len);
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
			FileStat	*&stat = openFiles[i];
			if (stat->hOvlFile == stat->hFile) {
				stat->hOvlFile = INVALID_HANDLE_VALUE;
			}
			if (stat->hFile != INVALID_HANDLE_VALUE) {
				::CloseHandle(stat->hFile);
				stat->hFile = INVALID_HANDLE_VALUE;
			}
			if (stat->hOvlFile != INVALID_HANDLE_VALUE) {
				::CloseHandle(stat->hOvlFile);
				stat->hOvlFile = INVALID_HANDLE_VALUE;
			}
		}
	}

	return	!isAbort;
}

WCHAR *FastCopy::RestorePath(WCHAR *path, int idx, int dir_len)
{
	FileStat	*stat = openFiles[idx];
	BOOL		is_stream = stat->dwFileAttributes ? FALSE : TRUE;

	if (is_stream) {
		int	i;
		for (i=idx-1; i >= 0; i--) {
			if (openFiles[i]->dwFileAttributes) {
				dir_len += wcscpyz(path + dir_len, openFiles[i]->cFileName);
				break;
			}
		}
		if (i < 0) {
			ConfirmErr(L"RestorePath", path + srcPrefixLen, CEF_STOP|CEF_NOAPI);
		}
	}
	wcscpyz(path + dir_len, stat->cFileName);
	return	path;
}

//BOOL rand_err()
//{
//	static int cnt=0;
//	if ((++cnt % 3) == 0) {
//		SetLastError(ERROR_INVALID_PARAMETER);
//		return TRUE;
//	}
//	return FALSE;
//}

BOOL FastCopy::ReadFileWithReduce(HANDLE hFile, void *buf, DWORD size, OverLap *ovl)
{
	DWORD	maxReadSizeSv = maxReadSize;
	BOOL	wait_ovl      = FALSE;

	ovl->waiting   = false;
	ovl->orderSize = 0;
	ovl->transSize = 0;

	while (1) {
		ovl->orderSize = min(size, maxReadSize);
		if (!::ReadFile(hFile, (BYTE *)buf, ovl->orderSize, &ovl->transSize, &ovl->ovl)) {
			DWORD	err = ::GetLastError();
			if (err == ERROR_NO_SYSTEM_RESOURCES) {
				if (min(size, maxReadSize) <= REDUCE_SIZE) {
					return FALSE;
				}
				maxReadSize -= REDUCE_SIZE;
				maxReadSize = ALIGN_SIZE(maxReadSize, REDUCE_SIZE);
				wait_ovl = TRUE;
				continue;
			}
			if (err == ERROR_IO_PENDING) {
				if (wait_ovl) {
					if (!WaitOverlapped(hFile, ovl)) return FALSE;
				}
				else {
					ovl->waiting   = true;
					break;	// success overlapped request
				}
			}
			else return FALSE;
		}
		break;	// 1回の転送で出来る範囲で終了
	}

	if (maxReadSize != maxReadSizeSv) {
		WCHAR buf[128];
		swprintf(buf, FMT_REDUCEMSG, 'R', maxReadSizeSv / 1024/1024, maxReadSize / 1024/1024);
		WriteErrLog(buf);
	}

	return	TRUE;
}

BOOL FastCopy::ReadFileAltStreamProc(int *open_idx, int dir_len, FileStat *stat)
{
	BOOL	ret = TRUE;
	ReqHead	*req = NULL;

	while (ret && !isAbort && (*open_idx) < openFilesCnt
		&& openFiles[(*open_idx)]->dwFileAttributes == 0) {	// attr == 0 is AltStream
		ret = ReadFileProc(open_idx, dir_len);
	}
	if (ret && stat->acl) {
		req = PrepareReqBuf(offsetof(ReqHead, stat) + stat->minSize, stat->aclSize, stat->fileID);
		if (req) {
			memcpy(req->buf, stat->acl, stat->aclSize);
			ret = SendRequest(WRITE_BACKUP_ACL, req, stat);
		} else ret = FALSE;
	}
	if (ret && stat->ead) {
		req = PrepareReqBuf(offsetof(ReqHead, stat) + stat->minSize, stat->eadSize, stat->fileID);
		if (req) {
			memcpy(req->buf, stat->ead, stat->eadSize);
			ret = SendRequest(WRITE_BACKUP_EADATA, req, stat);
		} else ret = FALSE;
	}
	return	ret;
}

void FastCopy::SetTotalErrInfo(BOOL is_stream, int64 err_trans)
{
	int		&totalErrFiles = is_stream ? total.errAclStream   : total.errFiles;
	int64	&totalErrTrans = is_stream ? total.errStreamTrans : total.errTrans;

	totalErrFiles++;
	totalErrTrans += err_trans;
}

BOOL FastCopy::ReadFilePeparse(Command cmd, int idx, int dir_len, FileStat *stat)
{
	BYTE	rd[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
	ReqHead	*req = NULL;
	BOOL	ret = FALSE;

	if ((stat->repSize = ReadReparsePoint(stat->hFile, rd, sizeof(rd))) <= 0) {
		ConfirmErr(L"ReadReparse(File)", RestorePath(src, idx, dir_len) + srcPrefixLen);
		SetTotalErrInfo(cmd == WRITE_BACKUP_ALTSTREAM, stat->FileSize());
		return	FALSE;
	}
	req = PrepareReqBuf(offsetof(ReqHead, stat) + stat->minSize, stat->repSize, stat->fileID);
	if (req) {
		memcpy(req->buf, rd, stat->repSize);
		ret = SendRequest(cmd, req, stat);
	}
	return	ret;
}

void FastCopy::IoAbortFile(HANDLE hFile, OvlList *ovl_list)
{
	::CancelIo(hFile);

	while (OverLap *ovl = ovl_list->GetObj(USED_LIST)) {
		WaitOverlapped(hFile, ovl);
		ovl_list->PutObj(FREE_LIST, ovl);
	}
}

BOOL FastCopy::ReadAbortFile(int cur_idx, Command cmd, int dir_len, BOOL is_stream, BOOL is_modify)
{
	BOOL		ret = TRUE;
	FileStat	*stat = openFiles[cur_idx];
	HANDLE		hIoFile = (stat->hOvlFile != INVALID_HANDLE_VALUE) ? stat->hOvlFile : stat->hFile;

	// REQ_NONE == 書き込み先abortによるエラー処理
	if (cmd != REQ_NONE && (!is_stream || (info.flags & REPORT_STREAM_ERROR))) {
		DWORD	flags = is_modify ? (CEF_NOAPI|CEF_DATACHANGED) : 0;

		Confirm::Result result = ConfirmErr(is_stream ? L"ReadFile(st)" : L"ReadFile",
			RestorePath(src, cur_idx, dir_len) + srcPrefixLen, flags);
		if (result != Confirm::CONTINUE_RESULT) {
			ret = FALSE; // abort
		}
	}

	IoAbortFile(hIoFile, &rOvl);

	SetTotalErrInfo(is_stream, stat->FileSize());
	stat->SetFileSize(0);

	if (cmd == WRITE_FILE_CONT) {
		SendRequest(WRITE_ABORT, NULL, stat);
	} else if (ret && (info.flags & CREATE_OPENERR_FILE)) {
		SendRequest(cmd, NULL, stat);
	}
	return	ret;
}

bool IsFileChanged(FileStat *stat)
{
	FILETIME wt;
	return !::GetFileTime(stat->hFile, 0, 0, &wt) || (*(int64 *)&wt != stat->WriteTime());
}

BOOL FastCopy::ReadFileProcCore(int cur_idx, int dir_len, Command cmd, FileStat *stat)
{
	int64	file_size = stat->FileSize();
	BOOL	is_stream  = (cmd == WRITE_BACKUP_ALTSTREAM);
	BOOL	is_digest  = IsUsingDigestList() && !is_stream;
	BOOL	is_modifyerr = FALSE;
	int64	order_total = 0;
	int64	total_size   = 0;
	HANDLE	hIoFile = (stat->hOvlFile != INVALID_HANDLE_VALUE) ? stat->hOvlFile : stat->hFile;
	BOOL	ret = TRUE;

	::SetFilePointer(hIoFile, 0, NULL, FILE_BEGIN);

	while (total_size < file_size && !isAbort) {
		OverLap	*ovl   = rOvl.GetObj(FREE_LIST);
		ovl->req = NULL;
		rOvl.PutObj(USED_LIST, ovl);

		while (1) {
			if (!(ovl->req = PrepareReqBuf(offsetof(ReqHead, stat) + stat->minSize,
					file_size - total_size, stat->fileID))) {
				cmd = REQ_NONE;
				ret = FALSE;
				break;
			}
			ovl->SetOvlOffset(total_size + order_total);
			if ((ret = ReadFileWithReduce(hIoFile, ovl->req->buf, ovl->req->bufSize, ovl))) break;

			// reparse point で別 volume に移動した場合用
			if (::GetLastError() == ERROR_INVALID_PARAMETER && sectorSize < BIG_SECTOR_SIZE) {
				srcSectorSize = max(srcSectorSize, BIG_SECTOR_SIZE);
				sectorSize = max(srcSectorSize, dstSectorSize);
				CancelReqBuf(ovl->req);
				ovl->req = NULL;
			}
			else break;
		}
		if (!ret || !ovl->req || isAbort) break;
		order_total += ovl->orderSize;

		BOOL is_empty_ovl = rOvl.IsEmpty(FREE_LIST);
		BOOL is_flush = (total_size + order_total >= file_size) || readInterrupt || !ovl->waiting;
		if (!is_empty_ovl && !is_flush) continue;

		while (OverLap	*ovl_tmp = rOvl.GetObj(USED_LIST)) {
			rOvl.PutObj(FREE_LIST, ovl_tmp);
			if (!(ret = WaitOvlIo(hIoFile, ovl_tmp, &total_size, &order_total))) break;

			total.readTrans += ovl_tmp->transSize; 
			ovl_tmp->req->readSize = ovl_tmp->transSize;

			if (total_size > file_size ||
				(total_size < file_size && ovl_tmp->orderSize != ovl_tmp->transSize) ||
				(total_size == file_size && stat->isWriteShare && IsFileChanged(stat))) {
				is_modifyerr = TRUE;
				ret = FALSE;
				break;
			}
			SendRequest(cmd, ovl_tmp->req, stat); // WRITE_FILE_CONT && !digest時stat不要…
			cmd = WRITE_FILE_CONT;	// 2回目以降のSendReqはCONT

			if (!isSameDrv && info.mode == MOVE_MODE && total_size < file_size) {
				FlushMoveList(FALSE);
			}
			if (waitTick && total_size < file_size) Wait();
			if (!is_flush || isAbort) break;
		}
		if (!ret || isAbort) break;
	}
	if (!ret || isAbort) {
		ReadAbortFile(cur_idx, cmd, dir_len, is_stream, is_modifyerr);
	}
	return	ret;
}

BOOL FastCopy::ReadFileProc(int *open_idx, int dir_len)
{
	BOOL	ret = TRUE;
	int		cur_idx = (*open_idx)++;
	FileStat *stat = openFiles[cur_idx];

	if (stat->hFile != INVALID_HANDLE_VALUE /* && stat->isWriteShare */) {
		::GetFileTime(stat->hFile, 0, 0, &stat->ftLastWriteTime);
		stat->nFileSizeLow = ::GetFileSize(stat->hFile, &stat->nFileSizeHigh);
	}
	int64	file_size = stat->FileSize();
	Command	cmd = (enableAcl || enableStream) ? stat->dwFileAttributes ?
				WRITE_BACKUP_FILE : WRITE_BACKUP_ALTSTREAM : WRITE_FILE;
	BOOL	is_stream  = (cmd == WRITE_BACKUP_ALTSTREAM);
	BOOL	is_reparse = IsReparseEx(stat->dwFileAttributes);
	int64	req_count  = reqSendCount;

	if ((info.flags & RESTORE_HARDLINK) && !is_stream) {	// include listing only
		if (CheckHardLink(RestorePath(src, cur_idx, dir_len), -1, stat->hFile,
				stat->GetLinkData()) == LINK_ALREADY) {
			stat->SetFileSize(0);
			ret = SendRequest(CREATE_HARDLINK, 0, stat);
			goto END;
		}
	}
	if ((file_size == 0 && !is_reparse) || isListingOnly) {
		ret = SendRequest(cmd, 0, stat);
		if (cmd != WRITE_BACKUP_FILE || isListingOnly) {
			goto END; // listing時はWRITE_BACKUP_END不要
		}
	}
	if (stat->hFile == INVALID_HANDLE_VALUE) {
		if (!is_stream) {
			::SetLastError(stat->lastError);
			SetTotalErrInfo(is_stream, file_size);
		}
		stat->SetFileSize(0);
		Confirm::Result	conf_res = Confirm::CONTINUE_RESULT;
		if (!is_stream || (info.flags & REPORT_STREAM_ERROR)) {
			conf_res = ConfirmErr(is_stream ? L"OpenFile(st)" : L"OpenFile"
				, RestorePath(src, cur_idx, dir_len) + srcPrefixLen);
		}
		if (conf_res == Confirm::CONTINUE_RESULT) {
			if ((info.flags & CREATE_OPENERR_FILE) && reqSendCount == req_count) {
				SendRequest(cmd, 0, stat);	// エラーでも空ファイルを作成する
			}
		}
		ret = FALSE;
		goto END;
	}
	if (is_reparse) {
		ret = ReadFilePeparse(cmd, cur_idx, dir_len, stat);
	}
	else {
		ret = ReadFileProcCore(cur_idx, dir_len, cmd, stat);
	}
	if (cmd == WRITE_BACKUP_FILE && ret && !isAbort) {
		ret = ReadFileAltStreamProc(open_idx, dir_len, stat);
	}

END:
	if (reqSendCount != req_count && cmd == WRITE_BACKUP_FILE) {
		SendRequest(WRITE_BACKUP_END);
	}
	if (ret && info.mode == MOVE_MODE && !is_stream) {
		CloseMultiFilesProc(*open_idx);
		int	len = dir_len + wcscpyz(src + dir_len, stat->cFileName) + 1;
		PutMoveList(src, len, stat, MoveObj::START);
	}
	return	ret && !isAbort;
}

void FastCopy::DstRequest(DstReqKind kind, void *info)
{
	cv.Lock();
	dstAsyncRequest = kind;
	dstAsyncInfo    = info;
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
		dstRequestResult = ReadDstStat((int)dstAsyncInfo);
		break;

	case DSTREQ_DIGEST:
		dstRequestResult = MakeDigest(confirmDst, &dstDigest, (FileStat *)dstAsyncInfo);
		break;
	}

	if (!isSameDrv) {
		cv.Lock();
		cv.Notify();
	}
	dstAsyncInfo    = NULL;
	dstAsyncRequest = DSTREQ_NONE;

	return	dstRequestResult;
}

BOOL FastCopy::ReadDstStat(int dir_len)
{
	HANDLE		fh;
	int			len;
	FileStat	*dstStat;
	BOOL		ret = TRUE;
	WIN32_FIND_DATAW	fdat;

	dstStat = (FileStat *)dstStatBuf.Buf();
	dstStatBuf.SetUsedSize(0);
	dstStatIdxVec.SetUsedNum(0);

	if ((fh = ::FindFirstFileW(confirmDst, &fdat)) == INVALID_HANDLE_VALUE) {
		if (::GetLastError() != ERROR_FILE_NOT_FOUND
		&& wcscmp(confirmDst + dstBaseLen, L"*") == 0) {
			ret = FALSE;
			total.errDirs++;
			ConfirmErr(L"FindFirstFile(stat)", confirmDst + dstPrefixLen);
		}	// ファイル名を指定してのコピーで、コピー先が見つからない場合は、
			// エントリなしでの成功とみなす
		goto END;
	}
	do {
		if (IsParentOrSelfDirs(fdat.cFileName)) continue;

		ClearNonSurrogateReparse(&fdat);

		if (!dstStatIdxVec.Push(dstStat)) {
			ConfirmErr(L"Can't alloc memory(dstStatIdxVec)", NULL, CEF_STOP);
			break;
		}

		if (NeedSymlinkDeref(&fdat)) { // del/moveでは REPARSE_AS_NORMAL は存在しない
			wcscpyz(confirmDst + dir_len, fdat.cFileName);
			ModifyRealFdat(confirmDst, &fdat);
		}
		len = FdatToFileStat(&fdat, dstStat, TRUE);
		dstStatBuf.AddUsedSize(len);

		if (dstStatBuf.RemainSize() <= maxStatSize && dstStatBuf.Grow(MIN_ATTR_BUF) == FALSE) {
			ConfirmErr(L"Can't alloc memory(dstStatBuf)", NULL, CEF_STOP);
			break;
		}
		dstStat = (FileStat *)(dstStatBuf.Buf() + dstStatBuf.UsedSize());
	}
	while (!isAbort && ::FindNextFileW(fh, &fdat));

	if (!isAbort && ::GetLastError() != ERROR_NO_MORE_FILES) {
		total.errFiles++;
		ret = FALSE;
		ConfirmErr(L"FindNextFile(stat)", confirmDst + dstPrefixLen);
	}
	::FindClose(fh);

END:
	if (ret) {
		if (!(ret = hash.Init(&dstStatIdxVec))) {
			ConfirmErr(L"Can't alloc memory(dstStatIdxVec2)", NULL, CEF_STOP);
			return	FALSE;
		}
	}

	return	ret;
}

BOOL StatHash::Init(VBVec<FileStat *> *statIdxVec)
{
	int		data_num = statIdxVec->UsedNum();
	hashNum = data_num | 7;

	// hash table を Used の直後に確保
	int	require_size = hashNum * sizeof(FileStat *);
	int	grow_size    = int(require_size - statIdxVec->RemainSize());

	if (grow_size > 0 && !statIdxVec->Grow(ALIGN_SIZE(grow_size, MIN_ATTRIDX_BUF))) return FALSE;

	hashTbl = &statIdxVec->Get(0) + data_num;
	memset(hashTbl, 0, hashNum * sizeof(FileStat *));

	for (int i=0; i < data_num; i++) {
		FileStat *data = statIdxVec->Get(i);
		FileStat **stat = hashTbl + (data->hashVal % hashNum);

		while (*stat) {
			stat = &(*stat)->next;
		}
		*stat = data;
	}
	return	TRUE;
}

FileStat *StatHash::Search(WCHAR *upperName, DWORD hash_val)
{
	for (FileStat *target = hashTbl[hash_val % hashNum]; target; target = target->next) {
		if (target->hashVal == hash_val && wcscmp(target->upperName, upperName) == 0)
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
		if (InitDeletePath(i)) {
			DeleteProc(dst, dstBaseLen,
						(FilterBits(DIR_REG, INC_REG) & filterMode) ? FR_CONT : FR_MATCH);
		}
	}
	FinishNotify();
	return	TRUE;
}

/*
	削除処理
*/
BOOL FastCopy::DeleteProc(WCHAR *path, int dir_len, FilterRes fr)
{
	HANDLE		hDir;
	BOOL		ret = TRUE;
	FileStat	stat;
	WIN32_FIND_DATAW fdat;

	if ((hDir = ::FindFirstFileW(path, &fdat)) == INVALID_HANDLE_VALUE) {
		total.errDirs++;
		return	ConfirmErr(L"FindFirstFile(del)", path + dstPrefixLen), FALSE;
	}

	do {
		if (IsParentOrSelfDirs(fdat.cFileName))
			continue;

		ClearNonSurrogateReparse(&fdat);

		if (waitTick) Wait((waitTick + 9) / 10);

		FilterRes	cur_fr = FilterCheck(path, dir_len, &fdat, fr);
		if (cur_fr == FR_UNMATCH) {
			total.filterDelSkips++;
			continue;
		}
		stat.ftLastWriteTime	= fdat.ftLastWriteTime;
		stat.nFileSizeLow		= fdat.nFileSizeLow;
		stat.nFileSizeHigh		= fdat.nFileSizeHigh;
		stat.dwFileAttributes	= fdat.dwFileAttributes;

		if (IsDir(stat.dwFileAttributes)) {
			ret = DeleteDirProc(path, dir_len, fdat.cFileName, &stat, cur_fr);
		}
		else
			ret = DeleteFileProc(path, dir_len, fdat.cFileName, &stat);
	}
	while (!isAbort && ::FindNextFileW(hDir, &fdat));

	if (!isAbort && ret && ::GetLastError() != ERROR_NO_MORE_FILES) {
		ret = FALSE;
		ConfirmErr(L"FindNextFile(del)", path + dstPrefixLen);
	}

	::FindClose(hDir);

	return	ret && !isAbort;
}

BOOL FastCopy::DeleteDirProc(WCHAR *path, int dir_len, WCHAR *fname, FileStat *stat, FilterRes fr)
{
	int		new_dir_len = dir_len + wcscpy_with_aster(path + dir_len, fname) -1;
	BOOL	ret = TRUE;
	int		cur_skips = total.filterDelSkips;
	BOOL	is_reparse = IsReparse(stat->dwFileAttributes);

	PushDepth(path, new_dir_len);

	if (!isListingOnly && (stat->dwFileAttributes & FILE_ATTRIBUTE_READONLY)) {
		path[new_dir_len-1] = 0;
		::SetFileAttributesW(path, FILE_ATTRIBUTE_DIRECTORY);
		path[new_dir_len-1] = '\\';
		// ResetAcl()
	}

	if (info.mode == DELETE_MODE && (info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA))) {
		memcpy(confirmDst + dir_len, path + dir_len, (new_dir_len - dir_len + 2) * sizeof(WCHAR));
	}
	if (!is_reparse) {
		ret = DeleteProc(path, new_dir_len, fr);
		if (isAbort) goto END;
	}
	if ((fr == FR_CONT) || cur_skips != total.filterDelSkips
		|| (filterMode && info.mode == DELETE_MODE && 
			(info.flags & DELDIR_WITH_FILTER) == 0 && (filterMode & FilterBits(FILE_REG, INC_REG)
				&& (filterMode & FilterBits(DIR_REG, INC_REG)) == 0)))
		goto END;

	path[new_dir_len - 1] = 0;

	if (!isListingOnly) {
		WCHAR	*target = path;

		if (info.mode == DELETE_MODE && (info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA))) {
			if (RenameRandomFname(path, confirmDst, dir_len, new_dir_len - dir_len - 1)) {
				target = confirmDst;
			}
		}
		if ((ret = ForceRemoveDirectoryW(target, info.aclReset)) == FALSE) {
			total.errDirs++;
			ConfirmErr(L"RemoveDirectory", target + dstPrefixLen);
			goto END;
		}
	}
	if (isListing) {
		PutList(path + dstPrefixLen, PL_DIRECTORY|PL_DELETE|(is_reparse ? PL_REPARSE : 0), 0,
			stat->WriteTime());
	}
	total.deleteDirs++;

END:
	PopDepth(path);
	return	ret;
}

BOOL FastCopy::DeleteFileProc(WCHAR *path, int dir_len, WCHAR *fname, FileStat *stat)
{
	int		len = wcscpyz(path + dir_len, fname);
	BOOL	is_reparse = IsReparse(stat->dwFileAttributes);

	if (!isListingOnly) {
		WCHAR	*target = path;

		if ((stat->dwFileAttributes & FILE_ATTRIBUTE_READONLY)) { // path's stat
			::SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);
			// ResetAcl()
		}
		if (info.mode == DELETE_MODE && (info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA))
		&& !is_reparse) {
			if (RenameRandomFname(target, confirmDst, dir_len, len)) {
				target = confirmDst;
			}
			if (stat->FileSize() && !IsReparse(stat->dwFileAttributes)) {
				if (WriteRandomData(target, stat, TRUE) == FALSE) {
					total.errFiles++;
					return	ConfirmErr(L"OverWrite", target + dstPrefixLen), FALSE;
				}
				total.writeFiles++;
			}
		}
		if (ForceDeleteFileW(target, info.aclReset) == FALSE) {
			total.errFiles++;
			return	ConfirmErr(L"DeleteFile", target + dstPrefixLen), FALSE;
		}
	}
	if (isListing) {
		PutList(path + dstPrefixLen, PL_DELETE|(is_reparse ? PL_REPARSE : 0), 0, stat->WriteTime(),
			stat->FileSize());
	}

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
	int64	fileID = 0;
	int64	remain_size;

	cv.Lock();

	while (!isAbort) {
		while (rDigestReqList.IsEmpty() && !isAbort) {
			cv.Wait(CV_WAIT_TICK);
		}
		ReqHead *req = rDigestReqList.TopObj();
		if (!req || isAbort) break;

		Command cmd = req->cmd;

		FileStat	*stat = &req->stat;
		if ((cmd == WRITE_FILE || cmd == WRITE_BACKUP_FILE
				|| (cmd == WRITE_FILE_CONT && fileID == stat->fileID))
			&& stat->FileSize() > 0 && !IsReparseEx(stat->dwFileAttributes)) {
			cv.UnLock();
//			Sleep(0);
			if (fileID != stat->fileID) {
				fileID = stat->fileID;
				remain_size = stat->FileSize();
				srcDigest.Reset();
			}

			srcDigest.Update(req->buf, req->readSize);

			if ((remain_size -= req->readSize) <= 0) {
				srcDigest.GetVal(stat->digest);
				if (remain_size < 0) {
					ConfirmErr(L"Internal Error(digest)", NULL, CEF_STOP|CEF_NOAPI);
				}
			}
			cv.Lock();
		}
		rDigestReqList.DelObj(req);

		if (isSameDrv)	readReqList.AddObj(req);
		else			writeReqList.AddObj(req);

		cv.Notify();
		if (cmd == REQ_EOF) break;
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
	data->buf_size = int(mainBuf.Size() - data->base_size);
	data->buf[0] = mainBuf.Buf() + data->base_size;

	if (data->is_nsa) {
		data->buf_size /= 3;
		data->buf_size = (data->buf_size / data->base_size) * data->base_size;
		data->buf_size = min(info.maxTransSize, data->buf_size);

		data->buf[1] = data->buf[0] + data->buf_size;
		data->buf[2] = data->buf[1] + data->buf_size;
		if (info.flags & OVERWRITE_PARANOIA) {
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

void FastCopy::GenRandomName(WCHAR *path, int fname_len, int ext_len)
{
	static char *char_dict = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

	for (int i=0; i < fname_len; i++) {
		path[i] = char_dict[(rand() >> 4) % 62];
	}
	if (ext_len) {
		path[fname_len - ext_len] =  '.';
	}
	path[fname_len] = 0;
}

BOOL FastCopy::RenameRandomFname(WCHAR *org_path, WCHAR *rename_path, int dir_len, int fname_len)
{
	WCHAR	*fname = org_path + dir_len;
	WCHAR	*rename_fname = rename_path + dir_len;
	WCHAR	*dot = wcsrchr(fname, '.');
	int		ext_len = dot ? int(fname_len - (dot - fname)) : 0;

	for (int i=fname_len; i <= MAX_FNAME_LEN; i++) {
		for (int j=0; j < 128; j++) {
			GenRandomName(rename_fname, i, ext_len);
			if (::MoveFileW(org_path, rename_path)) {
				return	TRUE;
			}
			else if (::GetLastError() != ERROR_ALREADY_EXISTS) {
				return	FALSE;
			}
		}
	}
	return	FALSE;
}

BOOL FastCopy::WriteRandomData(WCHAR *path, FileStat *stat, BOOL skip_hardlink)
{
	BOOL	is_nonbuf = dstFsType != FSTYPE_NETWORK
							&& (stat->FileSize() >= max(nbMinSize, PAGE_SIZE)
						|| (stat->FileSize() % dstSectorSize) == 0)
							&& (info.flags & USE_OSCACHE_WRITE) == 0 ? TRUE : FALSE;
	DWORD	flg = (is_nonbuf ? FILE_FLAG_NO_BUFFERING : 0) | FILE_FLAG_SEQUENTIAL_SCAN;
	DWORD	share = FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;

	HANDLE	hFile = ForceCreateFileW(path, GENERIC_WRITE, share, 0, OPEN_EXISTING, flg, 0,
		info.aclReset);	// ReadOnly attr のクリアは親で完了済み
	BOOL	ret = TRUE;

	if (hFile == INVALID_HANDLE_VALUE) {
		return	ConfirmErr(L"Write by Random Data(open)", path + dstPrefixLen), FALSE;
	}
	if (waitTick) Wait((waitTick + 9) / 10);

	BY_HANDLE_FILE_INFORMATION	bhi;
	if (skip_hardlink && ::GetFileInformationByHandle(hFile, &bhi) && bhi.nNumberOfLinks > 1)
		goto END;

	RandomDataBuf	*data = (RandomDataBuf *)mainBuf.Buf();
	int64			file_size = is_nonbuf ? ALIGN_SIZE(stat->FileSize(), dstSectorSize)
										:   stat->FileSize();
	OverLap	*ovl = wOvl.TopObj(FREE_LIST); // OverLap しない

	for (int i=0, end=data->is_nsa ? 3 : 1; i < end && ret; i++) {
		::SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
		int64	total_trans = 0;

		for (int64 total_trans=0; total_trans < file_size; total_trans += ovl->transSize) {
			int	max_trans = waitTick && (info.flags & AUTOSLOW_IOLIMIT) ?
							MIN_BUF : (int)maxWriteSize;
			max_trans = min(max_trans, data->buf_size);
			DWORD	io_size = (DWORD)(file_size - total_trans);
			io_size = (io_size > max_trans) ? max_trans : ALIGN_SIZE(io_size, dstSectorSize);

			ovl->SetOvlOffset(total_trans);
			if (!(ret = WriteFileWithReduce(hFile, data->buf[i], io_size, ovl))) break;

			total.writeTrans += ovl->transSize;
			if (waitTick) Wait();
		}
		total.writeTrans -= file_size - stat->FileSize();
		::FlushFileBuffers(hFile);
	}

	::SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
	::SetEndOfFile(hFile);

END:
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
	writeWaitList.Init();
	cv.Notify();
	cv.UnLock();

	return	ret;
}

BOOL FastCopy::CheckAndCreateDestDir(int dst_len)
{
	BOOL	ret = TRUE;

	dst[dst_len - 1] = 0;

	if (::GetFileAttributesW(dst) == 0xffffffff) {
		int	parent_dst_len = 0;

		parent_dst_len = dst_len - 2;
		for ( ; parent_dst_len >= 9; parent_dst_len--) { // "\\?\c:\x\..."
			if (dst[parent_dst_len -1] == '\\') break;
		}
		if (parent_dst_len < 9) parent_dst_len = 0;

		ret = parent_dst_len ? CheckAndCreateDestDir(parent_dst_len) : FALSE;

		if (isListingOnly || ::CreateDirectoryW(dst, NULL) && isListing) {
			PutList(dst + dstPrefixLen, PL_DIRECTORY);
			total.writeDirs++;
		}
		else ret = FALSE;
	}
	dst[dst_len - 1] = '\\';

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
		if (writeReq->cmd == REQ_EOF) {
			if (IsUsingDigestList() && !isAbort) {
				CheckDigests(CD_FINISH);
			}
			break;
		}
		if (writeReq->cmd == WRITE_FILE || writeReq->cmd == WRITE_BACKUP_FILE
		|| writeReq->cmd == CREATE_HARDLINK) {
			int		new_dst_len;
			if (writeReq->stat.renameCount == 0)
				new_dst_len = dir_len + wcscpyz(dst + dir_len, writeReq->stat.cFileName);
			else
				new_dst_len = dir_len + MakeRenameName(dst + dir_len,
										writeReq->stat.renameCount, writeReq->stat.cFileName);

			if (isListingOnly) {
				if (writeReq->cmd == CREATE_HARDLINK) {
					PutList(dst + dstPrefixLen, PL_HARDLINK);
					total.linkFiles++;
				}
				else {
					PutList(dst + dstPrefixLen, PL_NORMAL, 0, writeReq->stat.WriteTime(),
						writeReq->stat.FileSize());
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
		else if (writeReq->cmd == CASE_CHANGE) {
			ret = CaseAlignProc(dir_len);
		}
		else if (writeReq->cmd == MKDIR || writeReq->cmd == INTODIR) {
			PushDepth(dst, dir_len);
			ret = WriteDirProc(dir_len);
		}
		else if (writeReq->cmd == DELETE_FILES) {	// stat is dest stat from SendRequest
			if (IsDir(writeReq->stat.dwFileAttributes)) {
				ret = DeleteDirProc(dst, dir_len, writeReq->stat.cFileName, &writeReq->stat,
									FR_MATCH);
			}
			else
				ret = DeleteFileProc(dst, dir_len, writeReq->stat.cFileName, &writeReq->stat);
		}
		else if (writeReq->cmd == RETURN_PARENT) {
			PopDepth(dst);
			break;
		}
		else {
			switch (writeReq->cmd) {
			case WRITE_ABORT: case WRITE_FILE_CONT:
			case WRITE_BACKUP_ACL: case WRITE_BACKUP_EADATA:
			case WRITE_BACKUP_ALTSTREAM: case WRITE_BACKUP_END:
				break;

			default:
				ret = FALSE;
				WCHAR cmd[2] = { writeReq->cmd + '0', 0 };
				ConfirmErr(L"Illegal Request (internal error)", cmd, CEF_STOP|CEF_NOAPI);
				break;
			}
		}
	}
	return	ret && !isAbort;
}

BOOL FastCopy::CaseAlignProc(int dir_len)
{
	if (dir_len >= 0) {
		wcscpyz(dst + dir_len, writeReq->stat.cFileName);
	}

//	PutList(dst + dstPrefixLen, PL_CASECHANGE);

	if (isListingOnly) return TRUE;

	return	::MoveFileW(dst, dst);
}

BOOL FastCopy::WriteDirProc(int dir_len)
{
	BOOL		ret = TRUE;
	BOOL		is_mkdir = writeReq->cmd == MKDIR;
	BOOL		is_reparse = IsReparseEx(writeReq->stat.dwFileAttributes);
	int			buf_size = writeReq->bufSize;
	int			new_dir_len;
	FileStat	sv_stat;

	memcpy(&sv_stat, &writeReq->stat, offsetof(FileStat, cFileName));

	if (writeReq->stat.renameCount == 0)
		new_dir_len = dir_len + wcscpyz(dst + dir_len, writeReq->stat.cFileName);
	else
		new_dir_len = dir_len + MakeRenameName(dst + dir_len,
								writeReq->stat.renameCount, writeReq->stat.cFileName, TRUE);

	if (buf_size) {
		dstDirExtBuf.AddUsedSize(buf_size);
		if (dstDirExtBuf.RemainSize() < MIN_DSTDIREXT_BUF
		&& !dstDirExtBuf.Grow(MIN_DSTDIREXT_BUF)) {
			ConfirmErr(L"Can't alloc memory(dstDirExtBuf)", NULL, CEF_STOP);
			goto END;
		}
		memcpy(dstDirExtBuf.Buf() + dstDirExtBuf.UsedSize(), writeReq->buf, buf_size);
		sv_stat.acl = dstDirExtBuf.Buf() + dstDirExtBuf.UsedSize();
		sv_stat.ead = sv_stat.acl + sv_stat.aclSize;
		sv_stat.rep = sv_stat.ead + sv_stat.eadSize;
	}

	if (is_mkdir) {
		if (isListingOnly || ::CreateDirectoryW(dst, NULL)) {
			if (isListing && !is_reparse)
				PutList(dst + dstPrefixLen, PL_DIRECTORY, 0, sv_stat.WriteTime());
			total.writeDirs++;
		}
		else if (::GetLastError() != ERROR_ALREADY_EXISTS) {
			total.errDirs++;
			ConfirmErr(L"CreateDirectory", dst + dstPrefixLen);
		}
	}
	wcscpy(dst + new_dir_len, L"\\");

	if (!is_reparse) {
		if ((ret = WriteProc(new_dir_len + 1)), isAbort) {	// 再帰
			goto END;
		}
	}

	dst[new_dir_len] = 0;	// 末尾の '\\' を取る

	if (ret && sv_stat.isCaseChanged) CaseAlignProc();

	// タイムスタンプ/ACL/属性/リパースポイントのセット
	if (isListingOnly || (ret = SetDirExtData(&sv_stat))) {
		if (isListing && is_reparse && is_mkdir) {
			PutList(dst + dstPrefixLen, PL_DIRECTORY|PL_REPARSE, 0, sv_stat.WriteTime());
		}
	}
	else if (is_reparse && is_mkdir) {
		// 新規作成dirのリパースポイント化に失敗した場合は、dir削除
		ForceRemoveDirectoryW(dst, info.aclReset|FMF_ATTR);
	}

	if (buf_size) {
		dstDirExtBuf.AddUsedSize(-buf_size);
	}

END:
	return	ret && !isAbort;
}

BOOL FastCopy::SetDirExtData(FileStat *stat)
{
	HANDLE	fh;
	DWORD	mode = GENERIC_WRITE | (stat->acl && stat->aclSize && enableAcl ?
		(WRITE_OWNER|WRITE_DAC) : 0) | READ_CONTROL;
	BOOL	is_reparse = IsReparseEx(stat->dwFileAttributes);
	BOOL	ret = FALSE;
	DWORD	share = FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;
	DWORD	flg = FILE_FLAG_BACKUP_SEMANTICS|(is_reparse ? FILE_FLAG_OPEN_REPARSE_POINT : 0);

	fh = ForceCreateFileW(dst, mode, share, 0, OPEN_EXISTING, flg, 0, info.aclReset|FMF_ATTR);
	if (fh == INVALID_HANDLE_VALUE) {
		mode &= ~WRITE_OWNER;
		fh = ::CreateFileW(dst, mode, share, 0, OPEN_EXISTING, flg , 0);
	}
	if (fh == INVALID_HANDLE_VALUE) {
		if (is_reparse || (info.flags & REPORT_ACL_ERROR)) {
			ConfirmErr(is_reparse ? L"CreateDir(reparse)" : L"CreateDir(ACL)", dst + dstPrefixLen);
			total.errDirs++;
		}
		goto END;
	}
	if (is_reparse && stat->rep && stat->repSize) {
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
				total.errDirs++;
				ConfirmErr(L"WriteReparsePoint(dir)", dst + dstPrefixLen);
				goto END;
			}
		}
		ret = TRUE; // リパースポイント作成が成功した場合は ACL成功の有無は問わない
	}

	if ((stat->acl && stat->aclSize) && (mode & WRITE_OWNER)) {
		void	*backupContent = NULL;
		DWORD	size;
		if (!::BackupWrite(fh, stat->acl, stat->aclSize, &size, FALSE, TRUE, &backupContent)) {
			if (info.flags & REPORT_ACL_ERROR)
				ConfirmErr(L"BackupWrite(DIRACL)", dst + dstPrefixLen);
			goto END;
		}
		::BackupWrite(fh, NULL, NULL, NULL, TRUE, TRUE, &backupContent);
	}
	ret = TRUE;

END:
	if (fh != INVALID_HANDLE_VALUE) {
		::SetFileTime(fh, &stat->ftCreationTime, &stat->ftLastAccessTime, &stat->ftLastWriteTime);
		::CloseHandle(fh);
	}
	if (stat->dwFileAttributes &
			(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_SYSTEM)) {
		::SetFileAttributesW(dst, stat->dwFileAttributes);
	}
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
	int64			fileID = 0;
	DigestCalc		*calc = NULL;
	DataList::Head	*head;

	wDigestList.Lock();	// DataList は Get()後、UnLockで再利用される可能性が出るため、
						// Peek中に必要な処理を終えて、Getは remove処理として実施
	while (1) {
		while ((!(head = wDigestList.Peek())
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
					dstDigest.GetVal(dstDigest.val);
				}
//				total.verifyTrans += calc->dataSize;
				wDigestList.Lock();
			}
			fileID = calc->fileID;
		}

		if (calc->status == DigestCalc::DONE) {
			if (calc->dataSize) {
				if (memcmp(calc->digest, dstDigest.val, dstDigest.GetDigestSize()) == 0) {
					// compare OK
				}
				else {
					WCHAR	buf[512];
					MakeVerifyStr(buf, calc->digest, dstDigest.val, dstDigest.GetDigestSize());
					ConfirmErr(buf, calc->path + dstPrefixLen, CEF_NOAPI);
					calc->status = DigestCalc::ERR;
				}
			}
			else if (isListing) {
				dstDigest.GetEmptyVal(calc->digest);
			}
		}

		if (calc->status == DigestCalc::DONE) {
			DWORD ftype = IsReparseEx(calc->dwAttr) ? PL_REPARSE : PL_NORMAL;
			if (isListing) {
				PutList(calc->path + dstPrefixLen, ftype, 0, calc->wTime, calc->fileSize,
					calc->digest);
			}
			if (info.mode == MOVE_MODE) {
				SetFinishFileID(calc->fileID, MoveObj::DONE);
			}
			if (ftype != PL_REPARSE) total.verifyFiles++;
		}
		else if (calc->status == DigestCalc::ERR || calc->status == DigestCalc::PRE_ERR) {
			if (info.mode == MOVE_MODE) {
				SetFinishFileID(calc->fileID, MoveObj::ERR);
			}
			// PRE_ERR は WriteDigestProc(...DigestObj::NG) でカウント・出力済み
			if (calc->status == DigestCalc::ERR) {
				total.errFiles++;
				if (isListing) {
					PutList(calc->path + dstPrefixLen, PL_NORMAL|PL_COMPARE, 0, calc->wTime,
						calc->fileSize, calc->digest);
				}
			}
		}
		else if (calc->status == DigestCalc::PASS) {
			// pass for async io error
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
	if (wDigestList.Size() != wDigestList.MaxSize() && !isAbort) {
		BOOL	is_eof = FALSE;
		cv.Lock();
		if (writeReq && writeReq->cmd == REQ_EOF) {
			is_eof = TRUE;
		}
		cv.UnLock();
		if (is_eof) mainBuf.FreeBuf();	// 既にメイン処理が終了している場合は、メインバッファ解放

		if (!wDigestList.Grow(wDigestList.MaxSize() - wDigestList.Size())) {
			ConfirmErr(L"Can't alloc memory(digest)", NULL, CEF_STOP);
		}
		if (isAbort) return	NULL;
	}
	DataList::Head	*head = NULL;
	DigestCalc		*calc = NULL;
	int				require_size = sizeof(DigestCalc) + (obj ? obj->pathLen * sizeof(WCHAR) : 0);

	if (io_size) {
		require_size += io_size + dstSectorSize;
	}
	wDigestList.Lock();

	while (wDigestList.RemainSize() <= require_size && !isAbort) {
		wDigestList.Wait(CV_WAIT_TICK);
	}
	if (!isAbort && (head = wDigestList.Alloc(NULL, 0, require_size))) {
		calc = (DigestCalc *)head->data;
		calc->status = DigestCalc::INIT;
		if (obj) {
			calc->fileID   = obj->fileID;
			calc->fileSize = obj->fileSize;
			calc->wTime    = obj->wTime;
			calc->dwAttr   = obj->dwAttr;
			int64	val = (int64)(calc->path + obj->pathLen);
			calc->data = (BYTE *)ALIGN_SIZE(val, dstSectorSize);
			memcpy(calc->path, obj->path, obj->pathLen * sizeof(WCHAR));
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

	if (!isAbort && !head) {
		ConfirmErr(L"Can't alloc memory(wDigestList)", NULL, CEF_STOP);
	}

	return	calc;
}

BOOL FastCopy::PutDigestCalc(DigestCalc *obj, DigestCalc::Status status)
{
	wDigestList.Lock();
	if (obj) obj->status = status;
	wDigestList.Notify();
	wDigestList.UnLock();

	return	TRUE;
}

BOOL FastCopy::MakeDigestAsync(DigestObj *obj)
{
	BOOL	useOvl = (flagOvl && obj->fileSize > info.maxOvlSize);
	DWORD	flg = ((info.flags & USE_OSCACHE_READ) ? 0 : FILE_FLAG_NO_BUFFERING)
				| FILE_FLAG_SEQUENTIAL_SCAN | (useOvl ? flagOvl : 0);
	DWORD	share = FILE_SHARE_READ|FILE_SHARE_WRITE;
	HANDLE	hFile = INVALID_HANDLE_VALUE;
	int64	file_size = 0;

	if (wOvl.TopObj(USED_LIST)) {
		ConfirmErr(L"Not clear wOvl in MakeDigestAsync", NULL, CEF_STOP);
		return FALSE;
	}
	if (waitTick) Wait((waitTick + 9) / 10);

	if (obj->status == DigestObj::NG) goto ERR;

	if (obj->fileSize) {
		hFile = CreateFileWithRetry(obj->path, GENERIC_READ, share, 0, OPEN_EXISTING, flg, 0, 5);
		if (hFile == INVALID_HANDLE_VALUE) {
			ConfirmErr(L"OpenFile(digest)", obj->path + dstPrefixLen);
			goto ERR;
		}
		::GetFileSizeEx(hFile, (LARGE_INTEGER *)&file_size);
	}
	if (file_size != obj->fileSize) {
		ConfirmErr(L"Size is changed", obj->path + dstPrefixLen, CEF_NOAPI);
		goto ERR;
	}
	if (obj->fileSize == 0) {
		if (DigestCalc	*calc = GetDigestCalc(obj, 0)) {
			calc->dataSize = 0;
			PutDigestCalc(calc, DigestCalc::DONE);
		}
		goto END;
	}
	int64	total_size  = 0;
	int64	order_total = 0;

	while (total_size < file_size && !isAbort) {
		DWORD	max_trans = (waitTick && (info.flags & AUTOSLOW_IOLIMIT)) ?
					MIN_BUF : maxDigestReadSize;
		int64	remain = file_size - total_size;
		DWORD	io_size = DWORD((remain > max_trans) ? max_trans : remain);
		io_size = ALIGN_SIZE(io_size, dstSectorSize);
		BOOL	ret = TRUE;

		OverLap	*ovl = wOvl.GetObj(FREE_LIST);
		wOvl.PutObj(USED_LIST, ovl);
		ovl->SetOvlOffset(total_size + order_total);
		if (!(ovl->calc = GetDigestCalc(obj, io_size)) || isAbort) break;

		if (!(ret = ReadFileWithReduce(hFile, ovl->calc->data, io_size, ovl))) {
			ConfirmErr(L"ReadFile(digest)", obj->path + dstPrefixLen);
			break;
		}
		order_total += ovl->orderSize;
		BOOL is_empty_ovl = wOvl.IsEmpty(FREE_LIST);
		BOOL is_flush = (total_size + order_total >= file_size) || writeInterrupt || !ovl->waiting;
		if (!is_empty_ovl && !is_flush) continue;

		while (OverLap	*ovl_tmp = wOvl.GetObj(USED_LIST)) {
			wOvl.PutObj(FREE_LIST, ovl_tmp);
			if (!(ret = WaitOvlIo(hFile, ovl_tmp, &total_size, &order_total))) break;

			ovl_tmp->calc->dataSize = ovl_tmp->transSize;
			total.verifyTrans += ovl_tmp->transSize;
			PutDigestCalc(ovl_tmp->calc, total_size >= file_size ? DigestCalc::DONE
				: DigestCalc::CONT);
			if (waitTick) Wait();
			if (!ret || !is_flush || isAbort) break;
		}
		if (!ret || isAbort) break;
	}
	if (total_size < file_size) goto ERR;

END:
	if (hFile != INVALID_HANDLE_VALUE) ::CloseHandle(hFile);
	return TRUE;

ERR:
	PutDigestCalc(GetDigestCalc(obj, 0), (obj->status == DigestObj::NG) ?
		DigestCalc::PRE_ERR : DigestCalc::ERR);
	if (hFile != INVALID_HANDLE_VALUE) {
		if (wOvl.TopObj(USED_LIST)) {
			for (OverLap *ovl=wOvl.TopObj(USED_LIST); ovl; ovl=wOvl.NextObj(USED_LIST, ovl)) {
				if (ovl->calc) PutDigestCalc(ovl->calc, DigestCalc::PASS);
			}
		}
		IoAbortFile(hFile, &wOvl);
		::CloseHandle(hFile);
	}
	return	FALSE;
}

BOOL FastCopy::CheckDigests(CheckDigestMode mode)
{
	DataList::Head *head;
	BOOL			ret = TRUE;

	if (mode == CD_NOWAIT && !digestList.Peek()) return ret;

	while (!isAbort && (head = digestList.Get())) {
		if (!MakeDigestAsync((DigestObj *)head->data)) {
			ret = FALSE;
		}
	}
	digestList.Clear();

	cv.Lock();
	runMode = (mode == CD_FINISH) ? RUN_FINISH : RUN_NORMAL;
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

HANDLE FastCopy::CreateFileWithRetry(WCHAR *path, DWORD mode, DWORD share,
	SECURITY_ATTRIBUTES *sa, DWORD cr_mode, DWORD flg, HANDLE hTempl, int retry_max)
{
	HANDLE	fh = INVALID_HANDLE_VALUE;

	for (int i=0; i < retry_max && !isAbort; i++) {	// ウイルスチェックソフト対策
		if ((fh = ::CreateFileW(path, mode, share, sa, cr_mode, flg, hTempl))
				!= INVALID_HANDLE_VALUE)
			break;
		if (::GetLastError() != ERROR_SHARING_VIOLATION)
			break;
		::Sleep(i * i * 10);
		total.openRetry++;
	}
	return	fh;
}

BOOL FastCopy::WriteFileWithReduce(HANDLE hFile, void *buf, DWORD size, OverLap *ovl)
{
	DWORD	total_trans = 0;
	DWORD	maxWriteSizeSv = maxWriteSize;
	BOOL	wait_ovl = FALSE;
	BOOL	ret = TRUE;

	ovl->waiting   = false;
	ovl->orderSize = size;
	ovl->transSize = 0;

	while (total_trans < size) {
		DWORD io_size = min(size - total_trans, maxWriteSize);
		DWORD trans = 0;

		if (::WriteFile(hFile, (BYTE *)buf + total_trans, io_size, &trans, &ovl->ovl)) {
			total_trans += trans;
			ovl->SetOvlOffset(ovl->GetOvlOffset() + trans);
			continue;
		}
		DWORD	err = ::GetLastError();
		if (err == ERROR_NO_SYSTEM_RESOURCES) {
			if (min(size, maxWriteSize) <= REDUCE_SIZE) {
				ret = FALSE;
				break;
			}
			maxWriteSize -= REDUCE_SIZE;
			maxWriteSize = ALIGN_SIZE(maxWriteSize, REDUCE_SIZE);
			wait_ovl = TRUE;
			continue;
		}
		if (err == ERROR_IO_PENDING) {
			if (wait_ovl) {
				if (!WaitOverlapped(hFile, ovl)) return FALSE;
				total_trans += trans;
				ovl->SetOvlOffset(ovl->GetOvlOffset() + trans);
			}
			else {
				ovl->orderSize = io_size;
				ovl->waiting   = true;
				break;	// success overlapped request
			}
		}
		else {
			ret = FALSE;
			break;
		}
	}
	if (!ovl->waiting) {
		if (total_trans == size) {
			ovl->transSize = total_trans;
			ovl->orderSize = size;
		}
		else ret = FALSE;
	}

	if (maxWriteSize != maxWriteSizeSv) {
		WCHAR buf[128];
		swprintf(buf, FMT_REDUCEMSG, 'W', maxWriteSizeSv / 1024/1024, maxWriteSize / 1024/1024);
		WriteErrLog(buf);
	}
	return	ret;
}

BOOL FastCopy::RestoreHardLinkInfo(DWORD *link_data, WCHAR *path, int base_len)
{
	LinkObj	*obj;
	DWORD	hash_id = hardLinkList.MakeHashId(link_data);

	if (!(obj = (LinkObj *)hardLinkList.Search(link_data, hash_id))) {
		ConfirmErr(L"HardLinkInfo is gone(internal error)", path + base_len, CEF_NOAPI);
		return	FALSE;
	}

	wcscpy(path + base_len, obj->path);

	if (--obj->nLinks <= 1) {
		hardLinkList.UnRegister(obj);
		delete obj;
	}

	return	TRUE;
}

BOOL FastCopy::WriteDigestProc(int dst_len, FileStat *stat, DigestObj::Status status)
{
	int	path_len = dst_len + 1;

	DataList::Head *head = digestList.Alloc(NULL, 0, sizeof(DigestObj) + path_len * sizeof(WCHAR));
	if (!head) {
		ConfirmErr(L"Can't alloc memory(digestList)", NULL, CEF_STOP);
		return FALSE;
	}
	DigestObj *obj = (DigestObj *)head->data;
	obj->fileID = stat->fileID;
	obj->fileSize = (status == DigestObj::PASS) ? 0 : stat->FileSize();
	obj->wTime = stat->WriteTime();
	obj->dwAttr = stat->dwFileAttributes;
	obj->status = status;
	obj->pathLen = path_len;

	// writeReq will be chaned in big file
	memcpy(obj->digest, writeReq->stat.digest, dstDigest.GetDigestSize());
	memcpy(obj->path, dst, path_len * sizeof(WCHAR));

	BOOL is_empty_buf = digestList.RemainSize() <= digestList.MinMargin();
	if (is_empty_buf || (info.flags & SERIAL_VERIFY_MOVE)) {
		CheckDigests(is_empty_buf ? CD_WAIT : CD_NOWAIT); // empty なら wait
	}
	return TRUE;
}

BOOL FastCopy::WriteFileProcCore(HANDLE *_fh, int dst_len, FileStat *stat, WInfo *_wi)
{
	HANDLE	&fh = *_fh;
	WInfo	&wi = *_wi;
	DWORD	mode = GENERIC_WRITE;
	DWORD	share = FILE_SHARE_READ|FILE_SHARE_WRITE;
	DWORD	flg = (wi.is_nonbuf ? FILE_FLAG_NO_BUFFERING : 0) | FILE_FLAG_SEQUENTIAL_SCAN;
	BOOL	useOvl = (flagOvl && wi.file_size > info.maxOvlSize);

	if (wi.cmd == WRITE_BACKUP_FILE || wi.cmd == WRITE_BACKUP_ALTSTREAM) {
		if (stat->acl && stat->aclSize && enableAcl) mode |= WRITE_OWNER|WRITE_DAC;
		flg |= FILE_FLAG_BACKUP_SEMANTICS;
	}
	if (useOvl && wi.cmd != WRITE_BACKUP_FILE) {
		flg |= flagOvl;		// BackupWrite しないなら OVERLAPPED モードで開く
	}
	if (wi.is_reparse) {
		flg |= FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT;
	}
	fh = ForceCreateFileW(dst, mode, share, 0, CREATE_ALWAYS, flg, 0, FMF_ATTR);
	if (fh == INVALID_HANDLE_VALUE) {
		if (info.aclReset || enableAcl) {
			fh = ForceCreateFileW(dst, mode, share, 0, CREATE_ALWAYS, flg, 0,
				FMF_ACL|info.aclReset);
		}
	}
	if (fh == INVALID_HANDLE_VALUE) {
		if (!wi.is_stream || (info.flags & REPORT_STREAM_ERROR))
			ConfirmErr(wi.is_stream ? L"CreateFile(st)" : L"CreateFile", dst + dstPrefixLen);
		return	FALSE;
	}

	BOOL	ret = TRUE;
	if (wi.is_reparse) {
		if (WriteReparsePoint(fh, writeReq->buf, stat->repSize) <= 0) {
			ret = FALSE;
			ConfirmErr(L"WriteReparsePoint(File)", dst + dstPrefixLen);
		}
	} else {
		HANDLE	hIoFile = fh;
		if (useOvl && (flg & flagOvl) == 0) {
			hIoFile = ::CreateFileW(dst, mode, share, 0, CREATE_ALWAYS, flg|flagOvl, 0);
			if (hIoFile == INVALID_HANDLE_VALUE) hIoFile = fh;
		}
		if (useOvl) DisableLocalBuffering(hIoFile);

		ret = WriteFileCore(hIoFile, stat, &wi, mode, share, flg);

		if (useOvl && hIoFile != fh && hIoFile != INVALID_HANDLE_VALUE) {
			::CloseHandle(hIoFile);
		}
	}
	return	ret;
}

BOOL FastCopy::WriteFileProc(int dst_len)
{
	HANDLE		fh	= INVALID_HANDLE_VALUE;
	BOOL		ret = TRUE;
	WInfo		wi;
	FileStat	*stat = &writeReq->stat, sv_stat;

	wi.file_size = writeReq->stat.FileSize();
	wi.cmd	= writeReq->cmd;
	wi.is_reparse = IsReparseEx(stat->dwFileAttributes);
	wi.is_hardlink = wi.cmd == CREATE_HARDLINK;
	wi.is_stream = wi.cmd == WRITE_BACKUP_ALTSTREAM;
	wi.is_nonbuf = (dstFsType != FSTYPE_NETWORK &&
				(wi.file_size >= nbMinSize /* || (wi.file_size % dstSectorSize) == 0*/)
				&& (info.flags & USE_OSCACHE_WRITE) == 0 && !wi.is_reparse) ? TRUE : FALSE;

	BOOL	is_require_del = (stat->isNeedDel || (info.flags & (DEL_BEFORE_CREATE|RESTORE_HARDLINK
		|((info.flags & BY_ALWAYS) ? REPARSE_AS_NORMAL : 0)))) ? TRUE : FALSE;

	// writeReq の stat を待避して、それを利用する
	if (wi.cmd == WRITE_BACKUP_FILE || wi.file_size > writeReq->bufSize) {
		memcpy((stat = &sv_stat), &writeReq->stat, offsetof(FileStat, cFileName));
	}
	if (waitTick) Wait((waitTick + 9) / 10);

	if (is_require_del) {
		ForceDeleteFileW(dst, FMF_ATTR|info.aclReset);
	}
	if (wi.is_hardlink) {
		if ((ret = RestoreHardLinkInfo(writeReq->stat.GetLinkData(), hardLinkDst, dstBaseLen))) {
			if (!(ret = ::CreateHardLinkW(dst, hardLinkDst, NULL))) {
				ConfirmErr(L"CreateHardLink", dst + dstPrefixLen);
			}
		}
	}
	else {
		ret = WriteFileProcCore(&fh, dst_len, stat, &wi);
		if (!ret) SetErrWFileID(stat->fileID);
	}
	if (IsUsingDigestList() && !wi.is_stream && !isAbort) {	// digestList に error を含めて登録
		if (!WriteDigestProc(dst_len, stat,
			ret ? (wi.is_reparse ? DigestObj::PASS : DigestObj::OK) : DigestObj::NG)) {
			ret = FALSE;	 // false になるのはABORTレベル
		}
	}
	if (wi.cmd == WRITE_BACKUP_FILE) {
		/* ret = */ WriteFileBackupProc(fh, dst_len);	// ACL/EADATA/STREAM エラーは無視
	}
	if (!wi.is_hardlink) {
		if (ret && !wi.is_stream) {
			::SetFileTime(fh, &stat->ftCreationTime, &stat->ftLastAccessTime,
				&stat->ftLastWriteTime);
		}
		::CloseHandle(fh);
	}

	if (ret) {
		if (!wi.is_stream) {
			if (stat->isCaseChanged && !is_require_del) CaseAlignProc();
			if (stat->dwFileAttributes /* &
					(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_SYSTEM) */) {
				::SetFileAttributesW(dst, stat->dwFileAttributes);
			}
		}
		int	&totalFiles = wi.is_hardlink ? total.linkFiles :
							wi.is_stream ? total.writeAclStream : total.writeFiles;
		totalFiles++;
	}
	else {
		SetTotalErrInfo(wi.is_stream, wi.file_size);
		SetErrWFileID(stat->fileID);
		if (!wi.is_stream && // fh が無効かつERROR_NO_SYSTEM_RESOURCESはネットワーク作成後エラー
			(fh != INVALID_HANDLE_VALUE || ::GetLastError() == ERROR_NO_SYSTEM_RESOURCES)) {
			ForceDeleteFileW(dst, FMF_ATTR|info.aclReset);
		}
	}
	if (!wi.is_stream && info.mode == MOVE_MODE && (info.flags & VERIFY_FILE) == 0 && !isAbort) {
		SetFinishFileID(stat->fileID, ret ? MoveObj::DONE : MoveObj::ERR);
	}
	if ((isListingOnly || (isListing && !IsUsingDigestList())) && !wi.is_stream && ret) {
		DWORD flags = wi.is_hardlink ? PL_HARDLINK : wi.is_reparse ? PL_REPARSE : PL_NORMAL;
		PutList(dst + dstPrefixLen, flags, 0, stat->WriteTime(), stat->FileSize());
	}

	return	ret && !isAbort;
}

BOOL FastCopy::WriteFileCore(HANDLE fh, FileStat *stat, WInfo *_wi, DWORD mode, DWORD share,
	DWORD flg)
{
	BOOL	ret = TRUE;
	HANDLE	fh2 = INVALID_HANDLE_VALUE;
	WInfo	&wi = *_wi;
	int64	file_size = wi.file_size;
	int64	total_size = 0;
	int64	order_total = 0;
	BOOL	is_reopen = wi.is_nonbuf && (file_size % dstSectorSize);

	if (wOvl.TopObj(USED_LIST)) {
		ConfirmErr(L"Not clear wOvl in WriteFileCore", NULL, CEF_STOP);
		return FALSE;
	}
	if (is_reopen) {
		flg &= ~FILE_FLAG_NO_BUFFERING;
		fh2 = ::CreateFileW(dst, mode, share, 0, OPEN_EXISTING, flg, 0);
	}
	if (file_size > writeReq->readSize) {	// ファイルブロックをなるべく連続確保する
		int64	alloc_size = wi.is_nonbuf ? ALIGN_SIZE(file_size, dstSectorSize) : file_size;
		LONG	high_size = (LONG)(alloc_size >> 32);

		::SetFilePointer(fh, (LONG)alloc_size, &high_size, FILE_BEGIN);
		if (!::SetEndOfFile(fh) && ::GetLastError() == ERROR_DISK_FULL) {
			ConfirmErr(wi.is_stream ? L"SetEndOfFile(st)" : L"SetEndOfFile",
			 dst + dstPrefixLen, file_size >= info.allowContFsize ? 0 : CEF_STOP);
			ret = FALSE;
			goto END;
		}
		::SetFilePointer(fh, 0, NULL, FILE_BEGIN);
	}
	while (total_size < file_size) {
		DWORD	trans = 0;
		DWORD	write_size = writeReq->readSize;

		if (write_size + total_size > file_size) {
			ConfirmErr(L"Internal Error(write)", dst+dstPrefixLen, CEF_STOP);
			break;
		}
		OverLap	*ovl = wOvl.GetObj(FREE_LIST);
		wOvl.PutObj(USED_LIST, ovl);
		ovl->SetOvlOffset(total_size + order_total);

		if (wi.is_nonbuf) write_size = ALIGN_SIZE(write_size, dstSectorSize);
		ret = WriteFileWithReduce(fh, writeReq->buf, write_size, ovl);
		if (!ret || (!ovl->waiting && ovl->transSize == 0)) {
			ret = FALSE;
			if (!wi.is_stream || (info.flags & REPORT_STREAM_ERROR)) {
				ConfirmErr(wi.is_stream ? L"WriteFile(st)" : L"WriteFile", dst + dstPrefixLen,
					(::GetLastError() != ERROR_DISK_FULL || file_size >= info.allowContFsize
					 || wi.is_stream) ? 0 : CEF_STOP);
			}
			break;
		}
		order_total += ovl->orderSize;

		BOOL is_empty_ovl = wOvl.IsEmpty(FREE_LIST);
		BOOL is_flush = (total_size + order_total >= file_size) || writeInterrupt || !ovl->waiting;

		if (is_empty_ovl || is_flush) {
			while (OverLap	*ovl_tmp = wOvl.GetObj(USED_LIST)) {
				wOvl.PutObj(FREE_LIST, ovl_tmp);
				if (!(ret = WaitOvlIo(fh, ovl_tmp, &total_size, &order_total))) {
					if (!wi.is_stream || (info.flags & REPORT_STREAM_ERROR)) {
						ConfirmErr(wi.is_stream ? L"WriteFileWait(st)" : L"WriteFileWait",
							dst + dstPrefixLen);
					}
					break;
				}
				total.writeTrans += ovl_tmp->transSize;
				if (waitTick) Wait();
				if (!is_flush || isAbort) break;
			}
		}
		if (!ret || isAbort) break;
		if (total_size < file_size) {	// 続きがある
			if (!RecvRequest((BOOL)wOvl.TopObj(USED_LIST), is_empty_ovl)
			|| writeReq->cmd != WRITE_FILE_CONT) {
				ret = FALSE;
				total_size = file_size;
				if (!isAbort && writeReq->cmd != WRITE_ABORT) {
					WCHAR cmd[2] = { writeReq->cmd + '0', 0 };
					ConfirmErr(L"Illegal Request2 (internal error)", cmd, CEF_STOP|CEF_NOAPI);
				}
				break;
			}
		} else {
			total.writeTrans -= (total_size - file_size); // truncate予定分を引く
			break;
		}
	}
	if (ret && is_reopen && fh2 == INVALID_HANDLE_VALUE) {
		fh2 = CreateFileWithRetry(dst, mode, share, 0, OPEN_EXISTING, flg, 0, 10);
		if (fh2 == INVALID_HANDLE_VALUE && ::GetLastError() != ERROR_SHARING_VIOLATION) {
			flg  &= ~FILE_FLAG_BACKUP_SEMANTICS;
			mode &= ~(WRITE_OWNER|WRITE_DAC);
			fh2 = CreateFileWithRetry(dst, mode, share, 0, OPEN_EXISTING, flg, 0, 10);
		}
		if (fh2 == INVALID_HANDLE_VALUE) {
			ret = FALSE;
			if (!wi.is_stream || (info.flags & REPORT_STREAM_ERROR))
				ConfirmErr(wi.is_stream ? L"CreateFile2(st)" : L"CreateFile2", dst +dstPrefixLen);
		}
	}
	if (ret && total_size != file_size) {
		::SetFilePointer(fh2, stat->nFileSizeLow, (LONG *)&stat->nFileSizeHigh, FILE_BEGIN);
		if (!(ret = ::SetEndOfFile(fh2))) {
			if (!wi.is_stream || (info.flags & REPORT_STREAM_ERROR))
				ConfirmErr(wi.is_stream ? L"SetEndOfFile2(st)" : L"SetEndOfFile2",
					dst + dstPrefixLen);
		}
	}
END:
	if (!ret) {
		IoAbortFile(fh, &wOvl);
	}
	if (fh2 != INVALID_HANDLE_VALUE) ::CloseHandle(fh2);
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
				WCHAR cmd[2] = { writeReq->cmd + '0', 0 };
				ConfirmErr(L"Illegal Request3 (internal error)", cmd, CEF_STOP|CEF_NOAPI);
			}
			break;
		}
		switch (writeReq->cmd) {
		case WRITE_BACKUP_ACL: case WRITE_BACKUP_EADATA:
			SetLastError(0);
			if (!(ret = ::BackupWrite(fh, writeReq->buf, writeReq->cmd == WRITE_BACKUP_ACL ?
					writeReq->stat.aclSize : writeReq->stat.eadSize, &size, FALSE, TRUE,
					&backupContent))) {
				if (info.flags & REPORT_ACL_ERROR)
					ConfirmErr(L"BackupWrite(ACL/EADATA)", dst + dstPrefixLen);
			}
			break;

		case WRITE_BACKUP_ALTSTREAM:
			wcscpy(dst + dst_len, writeReq->stat.cFileName);
			ret = WriteFileProc(0);
			dst[dst_len] = 0;
			break;

		case WRITE_BACKUP_END:
			is_continue = FALSE;
			break;

		case WRITE_FILE_CONT:	// エラー時のみ
			break;

		case WRITE_ABORT:
			ret = FALSE;
			break;

		default:
			if (!isAbort) {
				WCHAR cmd[2] = { writeReq->cmd + '0', 0 };
				ConfirmErr(L"Illegal Request4 (internal error)", cmd, CEF_STOP|CEF_NOAPI);
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
	BOOL isResetRunMode = (runMode == RUN_DIGESTREQ);

	while ((!readReqList.IsEmpty() || !rDigestReqList.IsEmpty() || !writeReqList.IsEmpty()
			|| !writeWaitList.IsEmpty() || writeReq || runMode == RUN_DIGESTREQ) && !isAbort) {

		if (!readReqList.IsEmpty()) {
			writeReqList.MoveList(&readReqList);
			cv.Notify();
			if (isResetRunMode && runMode != RUN_FINISH) {
				runMode = RUN_DIGESTREQ;
			}
		}
		cv.Wait(CV_WAIT_TICK);

		if (isResetRunMode && readReqList.IsEmpty() && rDigestReqList.IsEmpty()
				&& writeReqList.IsEmpty() && writeReq) {
			if (runMode != RUN_FINISH) {
				runMode = RUN_DIGESTREQ;
			}
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

FastCopy::ReqHead *FastCopy::AllocReqBuf(int req_size, int64 _data_size)
{
	ReqHead	*req = NULL;
	size_t	max_trans  = (waitTick && (info.flags & AUTOSLOW_IOLIMIT)) ? MIN_BUF :
						 (flagOvl ? info.maxOvlSize : maxReadSize);
	size_t	data_size  = (_data_size > max_trans) ? max_trans : (size_t)_data_size;

	size_t	used_size        = usedOffset - mainBuf.Buf();
	if (data_size) used_size = ALIGN_SIZE(used_size, sectorSize);

	size_t	sector_data_size = ALIGN_SIZE(data_size, sectorSize);
	size_t	max_free         = mainBuf.Size() - used_size;
	size_t	align_req_size   = ALIGN_SIZE(req_size, 8);
	size_t	require_size     = sector_data_size + align_req_size;
	size_t	align_offset     = used_size;

	if (data_size && require_size < sector_data_size)
		require_size = sector_data_size;

	if (require_size > max_free) {
		if (max_free < MIN_BUF) {
			align_offset = 0;
			if (isSameDrv) {
				if (!ChangeToWriteModeCore()) return NULL; // Read -> Write 切り替え
			}
		}
		else {
			data_size = ((max_free - align_req_size) / BIGTRANS_ALIGN) * BIGTRANS_ALIGN;
			sector_data_size = sector_data_size = data_size;
			require_size = data_size + align_req_size;
		}
	}
	// isSameDrv == TRUE の場合、必ず Empty
	while ((!writeReqList.IsEmpty() || !writeWaitList.IsEmpty() || !rDigestReqList.IsEmpty())
	&& !isAbort) {
		if (align_offset == 0) {
			if (freeOffset < usedOffset && (freeOffset - mainBuf.Buf()) >= require_size) {
				break;
			}
		}
		else {
			if (freeOffset < usedOffset || mainBuf.Buf()+align_offset+require_size <= freeOffset) {
				break;
			}
		}
		cv.Wait(CV_WAIT_TICK);
	}
	req           = (ReqHead *)(mainBuf.Buf() + align_offset + sector_data_size);
	req->reqId    = 0;
	req->buf      = mainBuf.Buf() + align_offset;
	req->bufSize  = (int)sector_data_size;
	req->readSize = 0;
	req->reqSize  = req_size;
	readInterrupt = isSameDrv
		&& align_offset + require_size + BIGTRANS_ALIGN + MIN_BUF > mainBuf.Size();

	usedOffset = req->buf + require_size;

	return	req;
}

FastCopy::ReqHead *FastCopy::PrepareReqBuf(int req_size, int64 data_size, int64 file_id)
{
	int		err_cnt = 0;
	BOOL	ret = FALSE;
	ReqHead	*req = NULL;

	cv.Lock();

	if (errRFileID) {
		if (errRFileID == file_id) {
			err_cnt++;
			errRFileID = 0;
		}
	}
	if (errWFileID) {
		if (errWFileID == file_id) {
			err_cnt++;
			errWFileID = 0;
		}
	}
	if (err_cnt == 0) {
		req = AllocReqBuf(req_size, data_size);
	}

	cv.UnLock();

	return	req;
}

BOOL FastCopy::CancelReqBuf(ReqHead *req)
{
	cv.Lock();

	if (mainBuf.Buf() <= req->buf && (req->buf - mainBuf.Buf()) < mainBuf.Size()) {
		usedOffset = req->buf;
	}

	cv.UnLock();

	return	TRUE;
}

BOOL FastCopy::SendRequest(Command cmd, ReqHead *req, FileStat *stat)
{
	BOOL	ret = FALSE;

	cv.Lock();

	if (req == NULL) {
		req = AllocReqBuf(offsetof(ReqHead, stat) + (stat ? stat->minSize : 0), 0);
	}
	if (req) {
		if (!isAbort) {
			ret = SendRequestCore(cmd, req, stat);
		}
		cv.Notify();
	}
	cv.UnLock();

	return	ret && !isAbort;
}

BOOL FastCopy::SendRequestCore(Command cmd, ReqHead *req, FileStat *stat)
{
	reqSendCount++;
	req->cmd = cmd;
	req->reqId = reqSendCount;

	if (stat) {
		memcpy(&req->stat, stat, stat->minSize);
	}
	if (IsUsingDigestList()) {
		rDigestReqList.AddObj(req);
		cv.Notify();
	}
	else if (isSameDrv) {
		readReqList.AddObj(req);
	}
	else {
		writeReqList.AddObj(req);
		cv.Notify();
	}

	return	!isAbort;
}

// RecvRequest()
//  keepCur:   現在発行中の writeReq を writeWaitList に移動して解放を延期する
//  なお、keepCur==FALSE の場合、滞留writeWaitListがあれば全開放する
BOOL FastCopy::RecvRequest(BOOL keepCur, BOOL freeLast)
{
	cv.Lock();

	if (!keepCur || freeLast) { // !keepCur == clear all
		while (ReqHead *req = writeWaitList.TopObj()) {
			writeWaitList.DelObj(req);
			WriteReqDone(req);
			if (keepCur && freeLast) break;
		}
	}
	if (writeReq) {
		writeReqList.DelObj(writeReq);
		if (keepCur) {
			writeWaitList.AddObj(writeReq);
		} else {
			WriteReqDone(writeReq);
		}
		writeReq = NULL;
	}

	CheckDstRequest();
	BOOL	is_serial_mv = (info.flags & SERIAL_VERIFY_MOVE);

	while (writeReqList.IsEmpty() && !isAbort) {
		if (info.mode == MOVE_MODE && (info.flags & VERIFY_FILE) && writeWaitList.IsEmpty()) {
			if (runMode == RUN_DIGESTREQ || (is_serial_mv && digestList.Num() > 0)) {
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
	writeInterrupt = (isSameDrv && (writeReq && !writeReqList.NextObj(writeReq))
		|| (runMode == RUN_DIGESTREQ || (is_serial_mv && digestList.Num() > 0))) ? TRUE : FALSE;

	cv.UnLock();

	return	writeReq && !isAbort;
}

void FastCopy::WriteReqDone(ReqHead *req)
{
	freeOffset = (BYTE *)req + req->reqSize;
	if (!isSameDrv || (writeReqList.IsEmpty() && writeWaitList.IsEmpty())) {
		cv.Notify();
	}
}

void FastCopy::SetErrRFileID(int64 file_id)
{
	cv.Lock();
	errRFileID = file_id;
	cv.UnLock();
}

void FastCopy::SetErrWFileID(int64 file_id)
{
	cv.Lock();
	errWFileID = file_id;
	cv.UnLock();
}

BOOL FastCopy::SetFinishFileID(int64 _file_id, MoveObj::Status status)
{
	moveList.Lock();

	do {
		while (moveFinPtr = moveList.Peek(moveFinPtr)) {
			MoveObj *data = (MoveObj *)moveFinPtr->data;
			if (data->fileID == _file_id) {
				data->status = status;
				break;
			}
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

	shareInfo.ReleaseExclusive();

	delete [] openFiles;
	openFiles = NULL;

	mainBuf.FreeBuf();
	ntQueryBuf.FreeBuf();
	dstDirExtBuf.FreeBuf();
	mkdirQueVec.FreeBuf();
	dstStatIdxVec.FreeBuf();
	dstStatBuf.FreeBuf();
	dirStatBuf.FreeBuf();
	fileStatBuf.FreeBuf();
	listBuf.FreeBuf();
	errBuf.FreeBuf();
	srcDepth.FreeBuf();
	dstDepth.FreeBuf();
	cnfDepth.FreeBuf();
	srcDigest.buf.FreeBuf();
	dstDigest.buf.FreeBuf();
	digestList.UnInit();
	moveList.UnInit();
	hardLinkList.UnInit();
	wDigestList.UnInit();
	rOvl.UnInit();
	wOvl.UnInit();

	delete [] hardLinkDst;
	hardLinkDst = NULL;
	CleanRegFilter();

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
		ConvertExternalPath(dst + dstPrefixLen, ti->curPath, wsizeof(ti->curPath));
	}
	return	TRUE;
}

int FastCopy::FdatToFileStat(WIN32_FIND_DATAW *fdat, FileStat *stat, BOOL is_usehash, FilterRes fr)
{
	stat->fileID			= 0;
	stat->ftCreationTime	= fdat->ftCreationTime;
	stat->ftLastAccessTime	= fdat->ftLastAccessTime;
	stat->ftLastWriteTime	= fdat->ftLastWriteTime;
	stat->nFileSizeLow		= fdat->nFileSizeLow;
	stat->nFileSizeHigh		= fdat->nFileSizeHigh;
	stat->dwFileAttributes	= fdat->dwFileAttributes;
	stat->dwReserved0		= fdat->dwReserved0;
	stat->hFile				= INVALID_HANDLE_VALUE;
	stat->hOvlFile			= INVALID_HANDLE_VALUE;
	stat->lastError			= 0;
	stat->isExists			= false;
	stat->isCaseChanged		= false;
	stat->isWriteShare		= false;
	stat->isNeedDel			= false;
	stat->filterRes			= fr;
	stat->renameCount		= 0;
	stat->acl				= NULL;
	stat->aclSize			= 0;
	stat->ead				= NULL;
	stat->eadSize			= 0;
	stat->rep				= NULL;
	stat->repSize			= 0;
	stat->next				= NULL;
	memset(stat->digest, 0, SHA1_SIZE);

	int		len  = wcscpyz(stat->cFileName, fdat->cFileName) + 1;
	int		size = len * sizeof(WCHAR);
	stat->size = size + offsetof(FileStat, cFileName);
	stat->minSize = ALIGN_SIZE(stat->size, 8);

	if (is_usehash) {
		stat->upperName = stat->cFileName + len;
		memcpy(stat->upperName, stat->cFileName, size);
		::CharUpperW(stat->upperName);
		stat->hashVal = MakeHash(stat->upperName, size);
		stat->size += size;
	}
	stat->size = ALIGN_SIZE(stat->size, 8);

	return	stat->size;
}

BOOL FastCopy::ConvertExternalPath(const WCHAR *path, WCHAR *buf, int max_buf)
{
	if (path[0] == '\\' && path[1] != '\\') {	// UNC
		buf[0] = '\\';
		buf++;
		max_buf--;
	}
	wcsncpyz(buf, path, max_buf);

	return	TRUE;
}

FastCopy::Confirm::Result FastCopy::ConfirmErr(const WCHAR *msg, const WCHAR *path, DWORD flags)
{
	if (isAbort) return	Confirm::CANCEL_RESULT;

	BOOL	api_err = (flags & CEF_NOAPI) ? FALSE : TRUE;
	BOOL	is_abort = (flags & CEF_STOP);
	BOOL	is_data_modified = (flags & CEF_DATACHANGED);

#ifndef _DEBUG
	if (is_abort) isAbort = is_abort;
#endif

	WCHAR	path_buf[MAX_PATH_EX], msg_buf[MAX_PATH_EX + 100];
	WCHAR	*p = msg_buf;
	DWORD	err_code = api_err ? ::GetLastError() : 0;

	if (path) {
		ConvertExternalPath(path, path_buf, wsizeof(path_buf));
		path = path_buf;
	}
	p += wcscpyz(p, msg);

	if (is_data_modified) {
		p += wcscpyz(p, L"(file is modified)");
	}
	if (api_err && err_code) {
		p += wcscpyz(p, L"(");
		p += ::FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
				err_code, info.lcid > 0 ? LANGIDFROMLCID(info.lcid) : MAKELANGID(LANG_NEUTRAL,
				SUBLANG_DEFAULT), p, MAX_PATH_EX, NULL);
		if (p[-1] == '\n') {
			p -= 2;	// remove "\r\n"
		}
		p += swprintf(p, L"%u)", err_code);
	}
	p += swprintf(p, L" : %s", path ? path : L"");

	WriteErrLog(msg_buf, (int)(p - msg_buf));

	if (listBuf.Buf() && isListing) {
		PutList(msg_buf, PL_ERRMSG);
	}

	if (!is_abort && (info.ignoreEvent & FASTCOPY_ERROR_EVENT)) {
		return	Confirm::CONTINUE_RESULT;
	}

	Confirm	 confirm = { msg_buf, !is_abort, path, err_code, Confirm::CONTINUE_RESULT };

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
#ifndef _DEBUG
	if (is_abort) isAbort = is_abort;
#endif

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

BOOL FastCopy::WriteErrLog(const WCHAR *message, int len)
{
#define ERRMSG_SIZE		60
	::EnterCriticalSection(&errCs);

	BOOL	ret = TRUE;
	WCHAR	*msg_buf = (WCHAR *)(errBuf.Buf() + errBuf.UsedSize());

	if (len == -1)
		len = (int)wcslen(message);
	int	need_size = ((len + 3) * sizeof(WCHAR)) + ERRMSG_SIZE;

	if (errBuf.UsedSize() + need_size <= errBuf.MaxSize()) {
		if (errBuf.RemainSize() > need_size || errBuf.Grow(ALIGN_SIZE(need_size, PAGE_SIZE))) {
			memcpy(msg_buf, message, len * sizeof(WCHAR));
			msg_buf[len++] = '\r';
			msg_buf[len++] = '\n';
			errBuf.AddUsedSize(len * sizeof(WCHAR));
		}
	}
	else {
		if (errBuf.RemainSize() > ERRMSG_SIZE) {
			WCHAR	*err_msg = L" Too Many Errors...\r\n";
			wcscpyz(msg_buf, err_msg);
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
	WCHAR	*err_msg = GetLoadStrW(IDS_Err_Aborting);//L" Aborted by User"
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


#ifdef _DEBUG

unsigned WINAPI FastCopy::TestThread(void *fastCopyObj)
{
	return	((FastCopy *)fastCopyObj)->TestWrite();
}

/*
	テスト用ファイル作成
	サンプル例）1025byte のファイルを 10dir * 100個 = 計1000個作成
	 Source:  C:\temp\; 1k+1,10,100
	 DestDir: D:\test\
*/
int64 CalcFsize(const WCHAR *fsize_str)
{
	Wstr	targ(fsize_str);
	int64	other_fsize = 0;

	::CharUpperW(targ.Buf());

	if (WCHAR *p = wcspbrk(targ.Buf(), L"+-")) {
		int64	sign = (*p == '+') ? 1 : -1;
		*p = 0;
		other_fsize += sign * CalcFsize(p+1);
	}
	int64	fsize = _wtoi64(targ.s());

	if      (wcschr(targ.s(), 'G')) fsize *= 1024 * 1024 * 1024;
	else if (wcschr(targ.s(), 'M')) fsize *= 1024 * 1024;
	else if (wcschr(targ.s(), 'K')) fsize *= 1024;

	return	fsize + other_fsize;
}

BOOL FastCopy::TestWrite()
{
	int64	fsize = CalcFsize(wcstok(srcArray.Path(1), L", "));
	int		dnum  = _wtoi(wcstok(NULL, L", "));
	int		fnum  = _wtoi(wcstok(NULL, L", "));

	FILETIME			cur;
	UnixTime2FileTime(time(NULL), &cur);
	WIN32_FIND_DATAW	ddat = { FILE_ATTRIBUTE_DIRECTORY, cur, cur, cur };
	WIN32_FIND_DATAW	fdat = { FILE_ATTRIBUTE_NORMAL, cur, cur, cur };

	for (int i=0; i < dnum && !isAbort; i++) {
		FileStat	dstat;

		swprintf(ddat.cFileName, L"dname_%d", i);
		FdatToFileStat(&ddat, &dstat, FALSE);
		dstat.fileID = nextFileID++;
		SendRequest(MKDIR, NULL, &dstat);

		for (int j=0; j < fnum && !isAbort; j++) {
			FileStat	fstat;
			Command		cmd = WRITE_FILE;
			int64		remain_size = fsize;

			swprintf(fdat.cFileName, L"fname_%d", j);
			FdatToFileStat(&fdat, &fstat, FALSE);
			fstat.fileID = nextFileID++;
			fstat.SetFileSize(fsize);

			do {
				ReqHead	*req = PrepareReqBuf(offsetof(ReqHead, stat) + fstat.minSize,
					remain_size, fstat.fileID);
				if (req) {
					req->readSize = (remain_size > req->bufSize) ? req->bufSize : (int)remain_size;
					remain_size -= req->readSize;
					SendRequest(cmd, req, &fstat);
					cmd = WRITE_FILE_CONT;
				}
			} while (remain_size > 0 && !isAbort);
		}
		SendRequest(RETURN_PARENT);
	}
	SendRequest(REQ_EOF);

	ChangeToWriteMode(TRUE);
	while (hWriteThread) {
		if (hWriteThread) {
			if (::WaitForSingleObject(hWriteThread, 1000) != WAIT_TIMEOUT) {
				::CloseHandle(hWriteThread);
				hWriteThread = NULL;
			}
		}
	}
	FinishNotify();
	return TRUE;
}

#endif

