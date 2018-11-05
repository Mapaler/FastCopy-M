static char *fastcopy_id = 
	"@(#)Copyright (C) 2004-2018 H.Shirouzu and FastCopy Lab, LLC.	fastcopy.cpp	ver3.50";
/* ========================================================================
	Project  Name			: Fast Copy file and directory
	Create					: 2004-09-15(Wed)
	Update					: 2018-05-28(Mon)
	Copyright				: H.Shirouzu and FastCopy Lab, LLC.
	License					: GNU General Public License version 3
	======================================================================== */

#include "fastcopy.h"
#include <stdarg.h>
#include <stddef.h>
#include <process.h>
#include <stdio.h>
#include <time.h>
#include <random>

using namespace std;

/*=========================================================================
  クラス ： FastCopy
  概  要 ： マルチスレッドなファイルコピー管理クラス
  説  明 ： 
  注  意 ： 
=========================================================================*/
FastCopy::FastCopy()
{
	hReadThread = hWriteThread = hRDigestThread = hWDigestThread = NULL;
	memset(hOpenThreads, 0, sizeof(hOpenThreads));

	enableBackupPriv = TRUE;
	if (!TSetPrivilege(SE_BACKUP_NAME, TRUE)) {
		enableBackupPriv = FALSE;
	}
	if (!TSetPrivilege(SE_RESTORE_NAME, TRUE)) {
		enableBackupPriv = FALSE;
	}
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

//	for (char c = 'C'; c <= 'Z'; c++) {
//		WCHAR	root[MAX_PATH] = L"C:\\";
//		root[0] = c;
//		driveMng.SetDriveID(root);
//	}

	src = new WCHAR [MAX_WPATH + MAX_PATH];
	dst = new WCHAR [MAX_WPATH + MAX_PATH];
	confirmDst = new WCHAR [MAX_WPATH + MAX_PATH];

	*src = *dst = *confirmDst = 0;
	srcPrefixLen = srcBaseLen = 0;
	dstPrefixLen = dstBaseLen = 0;
	memset(&preTotal, 0, sizeof(preTotal));
	memset(&total, 0, sizeof(total));
	curTotal = &total;

	maxStatSize = (MAX_PATH * sizeof(WCHAR)) * 2 + offsetof(FileStat, cFileName) + 8;
	startTick = 0;
	preTick = 0;
	suspendTick = 0;
	endTick = 0;
	waitLv = 0;
	hardLinkDst = NULL;
	findInfoLv = IsWin7() ? FindExInfoBasic : FindExInfoStandard;
	findFlags  = IsWin7() ? FIND_FIRST_EX_LARGE_FETCH : 0;
	frdFlags = FMF_EMPTY_RETRY;

	wcIdx = ::TlsAlloc();

	InitDumpExceptArea();
	mainBuf.EnableDumpExcept();
	fileStatBuf.EnableDumpExcept();
	dirStatBuf.EnableDumpExcept();
	dstStatBuf.EnableDumpExcept();

	dstStatIdxVec.EnableDumpExcept();
	mkdirQueVec.EnableDumpExcept();

	dstDirExtBuf.EnableDumpExcept();
	errBuf.EnableDumpExcept();
	listBuf.EnableDumpExcept();
	ntQueryBuf.EnableDumpExcept();

	digestList.EnableDumpExcept();
	wDigestList.EnableDumpExcept();
	moveList.EnableDumpExcept();

	errBuf.AllocBuf(MIN_ERR_BUF, MAX_ERR_BUF);
}

FastCopy::~FastCopy()
{
	if (hWDigestThread) {
		::TerminateThread(hWDigestThread, 0);
	}
	if (hRDigestThread) {
		::TerminateThread(hRDigestThread, 0);
	}
	if (hWriteThread) {
		::TerminateThread(hWriteThread, 0);
	}
	if (hReadThread) {
		::TerminateThread(hReadThread, 0);
	}
	for (int i=openThreadCnt-1; i >= 0; i--) {
		::TerminateThread(hOpenThreads[i], 0);
		openThreadCnt--;
	}

	delete [] confirmDst;
	delete [] dst;
	delete [] src;

	::DeleteCriticalSection(&listCs);
	::DeleteCriticalSection(&errCs);

	::TlsFree(wcIdx);
}

FastCopy::FsType FastCopy::GetFsType(const WCHAR *root_dir)
{
	FsType	fstype = FSTYPE_NONE;

	if (driveMng.IsSSD(root_dir)) {
		fstype = FsType(fstype | FSTYPE_SSD);
	}
	if (driveMng.IsWebDAV(root_dir)) {
		fstype = FsType(fstype | FSTYPE_WEBDAV);
	}

	if (::GetDriveTypeW(root_dir) == DRIVE_REMOTE) {
		fstype = FsType(fstype | FSTYPE_NETWORK);
	}

	DWORD	serial, max_fname, fs_flags;
	WCHAR	vol[MAX_PATH];
	WCHAR	fs[MAX_PATH];

	if (::GetVolumeInformationW(root_dir, vol, MAX_PATH, &serial, &max_fname, &fs_flags,
			fs, MAX_PATH) == FALSE) {
		if (fstype & FSTYPE_NETWORK) {
			return	fstype;	// FS_NONE で成功扱い
		}
		ConfirmErr(L"GetVolumeInformation", root_dir, CEF_STOP);
		return	fstype;
	}

	fstype = FsType(fstype | ((wcsicmp(fs, NTFS_STR) ? FSTYPE_FAT : FSTYPE_NTFS)));
	return	fstype;
}

int FastCopy::GetSectorSize(const WCHAR *root_dir, BOOL use_cluster)
{
	DWORD	spc, bps, fc, cl;
	int		ret = BIG_SECTOR_SIZE;

	if (::GetDiskFreeSpaceW(root_dir, &spc, &bps, &fc, &cl)) {
		ret = bps;
		if (use_cluster && spc) {
			ret = bps * spc;
			if (ret > BIG_SECTOR_SIZE) {
				ret = max(bps, BIG_SECTOR_SIZE);
			}
		}
	}
	return	ret;
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
	WCHAR	*buf = wbuf;
	WCHAR	*fname = NULL;
	const WCHAR	*org_path = dstArray.Path(0);
	const WCHAR	*dst_root;

	// dst の確認/加工
	if (org_path[1] == ':' && org_path[2] != '\\')
		return	ConfirmErr(LoadStrW(IDS_BACKSLASHERR), org_path, CEF_STOP|CEF_NOAPI), FALSE;

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
	if (!IsDir(attr)) {	// 例外的に reparse point も dir 扱い
		return	ConfirmErr(L"Not a directory", dst, CEF_STOP|CEF_NOAPI), FALSE;
	}

	wcscpy(buf, dst);
	MakePathW(dst, buf, L"");
	// src自体をコピーするか（dst末尾に \ がついている or 複数指定）
	isExtendDir = wcscmp(buf, dst) == 0 || srcArray.Num() > 1 ? TRUE : FALSE;
	dstPrefixLen = MakeUnlimitedPath((WCHAR *)dst);
	dstBaseLen = (int)wcslen(dst);

	// dst ファイルシステム情報取得
	dstFsType = GetFsType(dst_root);
	dstSectorSize = GetSectorSize(dst_root,
		(IsNetFs(dstFsType) || info.minSectorSize) ? FALSE : TRUE);
	dstSectorSize = max(dstSectorSize, info.minSectorSize);
	nbMinSize = IsLocalNTFS(dstFsType) ? info.nbMinSizeNtfs : info.nbMinSizeFat;

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
	if (org_path[1] == ':' && org_path[2] != '\\') {
		ConfirmErr(LoadStrW(IDS_BACKSLASHERR), org_path, cef_flg|CEF_NOAPI);
		return	FALSE;
	}

	if (::GetFullPathNameW(org_path, MAX_WPATH, src, &fname) == 0) {
		ConfirmErr(L"GetFullPathName", org_path, cef_flg);
		return FALSE;
	}
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

	if (::GetFullPathNameW(src, MAX_PATH_EX, buf, &fname) == 0 || fname == NULL) {
		ConfirmErr(L"GetFullPathName2", src + srcPrefixLen, cef_flg);
		return	 FALSE;
	}

	// 確認用dst生成
	wcscpy(confirmDst + dstBaseLen, fname);

	// 同一パスでないことの確認
	if (wcsicmp(buf, confirmDst) == 0) {
		if (info.mode != DIFFCP_MODE || (info.flags & SAMEDIR_RENAME) == 0) {
			ConfirmErr(LoadStrW(IDS_SAMEPATHERR), confirmDst + dstBaseLen,
				CEF_STOP|CEF_NOAPI);
			return	FALSE;
		}
		wcscpy(confirmDst + dstBaseLen, L"*");
		isRename = TRUE;
	}
	else {
		isRename = FALSE;
	}

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
				ConfirmErr(LoadStrW(IDS_PARENTPATHERR), buf + srcPrefixLen,
					CEF_STOP|CEF_NOAPI);
				return	FALSE;
			}
		}
	}

	fname[0] = 0;
	srcBaseLen = (int)wcslen(buf);

	// src ファイルシステム情報取得
	if (wcsicmp(src_root_cur, src_root)) {
		srcFsType = GetFsType(src_root_cur);
		srcSectorSize = GetSectorSize(src_root_cur,
			(IsNetFs(srcFsType) || info.minSectorSize) ? FALSE : TRUE);
		srcSectorSize = max(srcSectorSize, info.minSectorSize);

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

	BOOL	ntfs_to_ntfs = IsNTFS(srcFsType) && IsNTFS(dstFsType);
	enableAcl    = ntfs_to_ntfs && (info.flags & WITH_ACL);
	enableStream = ntfs_to_ntfs && (info.flags & WITH_ALTSTREAM);
	wcscpy(src_root, src_root_cur);

	// 最大転送サイズ
	int64 tmpSize = ssize_t(isSameDrv ? info.bufSize : info.bufSize / 4);
	if (tmpSize < maxReadSize) maxReadSize = (DWORD)tmpSize;
	maxReadSize = max(MIN_BUF, maxReadSize);
	maxReadSize = maxReadSize / BIGTRANS_ALIGN * BIGTRANS_ALIGN;
	maxWriteSize = min(maxReadSize, maxWriteSize);
	maxDigestReadSize = min(maxReadSize, maxDigestReadSize);

	// タイムスタンプ同一判断猶予時間設定
	// 片方が NTFS でない場合、1msec 未満の誤差は許容（UDF 対策）
#define UDF_GRACE	10000
	timeDiffGrace = info.timeDiffGrace;
	if ((!ntfs_to_ntfs || IsNetFs(srcFsType) || IsNetFs(dstFsType)) && UDF_GRACE > timeDiffGrace) {
		timeDiffGrace = UDF_GRACE;
	}

	InitDepthBuf();

	return	TRUE;
}

BOOL FastCopy::SetUseDriveMap(const WCHAR *path)
{
	WCHAR	root[MAX_PATH];
	WCHAR	*fname = NULL;

	if (path[1] == ':' && path[2] != '\\') {
		return FALSE;
	}
	if (::GetFullPathNameW(path, MAX_WPATH, src, &fname) <= 0) {
		return FALSE;
	}

	GetRootDirW(src, root);

	int idx = driveMng.SetDriveID(root);
	if (idx >= 0) {
		useDrives |= 1ULL << idx;
	}

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
		return	ConfirmErr(LoadStrW(IDS_BACKSLASHERR), org_path, cef_flg|CEF_NOAPI), FALSE;

	if (::GetFullPathNameW(org_path, MAX_WPATH, dst, &fname) == 0)
		return	ConfirmErr(L"GetFullPathName", org_path, cef_flg), FALSE;

	attr = ::GetFileAttributesW(dst);

	if (attr != 0xffffffff && IsNoReparseDir(attr)
	|| (info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA))) {
		GetRootDirW(dst, dst_root);
		if (info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA)) {
			dstFsType = GetFsType(dst_root);
			dstSectorSize = GetSectorSize(dst_root,
				(IsNetFs(dstFsType) || info.minSectorSize) ? FALSE : TRUE);
			dstSectorSize = max(dstSectorSize, info.minSectorSize);
			nbMinSize = IsNTFS(dstFsType) ? info.nbMinSizeNtfs : info.nbMinSizeFat;
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
		return	ConfirmErr(L"GetFullPathName3", dst + dstPrefixLen, cef_flg), FALSE;
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
	bool	is_charclass = false;
	bool	is_escape = false;
	int		count = 0;

	for ( ; *path; path++) {
		if (is_charclass) {
			if (is_escape) {
				is_escape = false;
			} else {
				if (*path == ']') {
					is_charclass = false;
				} else if (*path == '\\') {
					is_escape = true;
				}
			}
		}
		else {
			if (*path == '[') {
				is_charclass = true;
			}
			else {
				if (*path == '/' || *path == '\\') count++;
			}
		}
	}
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

			if (targ.size() < depth + 1) {
				targ.resize(depth + 1, NULL);
			}
			if (targ[depth] == NULL) {
				targ[depth] = new RegExp();
			}
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

	if (!RegisterRegFilterCore(incArray, true)) {
		return FALSE;
	}
	if (!RegisterRegFilterCore(excArray, false)) {
		return FALSE;
	}
	return	TRUE;
}

BOOL FastCopy::RegisterInfo(const PathArray *_srcArray, const PathArray *_dstArray, Info *_info,
	const PathArray *_includeArray, const PathArray *_excludeArray)
{
#ifdef TRACE_DBG
	trclog_init();
	Trl(__FUNCTIONW__, __LINE__);
#endif

	info = *_info;
	srcArray.Init();
	dstArray.Init();

	isAbort = FALSE;
	isRename = FALSE;
	filterMode = 0;
	useDrives = 0;
	timeDiffGrace = info.timeDiffGrace; // InitSrcPath で最終値に更新

	findFlags = (IsWin7() && !(info.flags & NO_LARGE_FETCH)) ? FIND_FIRST_EX_LARGE_FETCH : 0;
	frdFlags = FMF_EMPTY_RETRY;

	isListingOnly = (info.flags & LISTING_ONLY) ? TRUE : FALSE;
	isListing = (info.flags & LISTING) || isListingOnly ? TRUE : FALSE;
	isPreSearch = (info.flags & PRE_SEARCH) ? TRUE : FALSE;
	isExec = (isListingOnly || isPreSearch) ? FALSE : TRUE;

	endTick = 0;
	depthIdxOffset = 0;

	src_root[0] = 0;

	// 最大転送サイズ上限（InitSrcPath で再設定）
	maxReadSize = maxWriteSize = maxDigestReadSize = (DWORD)info.maxOvlSize;

	// reg filter
	RegisterRegFilter(_includeArray, _excludeArray);

	if (info.fromDateFilter != 0  || info.toDateFilter  != 0)  filterMode |= DATE_FILTER;
	if (info.minSizeFilter  != -1 || info.maxSizeFilter != -1) filterMode |= SIZE_FILTER;

	int64	need_size = FASTCOPY_BUFSIZE(info.maxOvlSize, info.maxOvlNum);

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
		return	ConfirmErr(L"Illegal Flags (junction/symlink)", NULL, CEF_STOP|CEF_NOAPI), FALSE;
	}

	driveMng.Init((DriveMng::NetDrvMode)info.netDrvMode);
	driveMng.SetDriveMap(info.driveMap);

	// set useDrives
	if (info.mode != TEST_MODE) {
		for (int i=0; i < _srcArray->Num(); i++) {
			SetUseDriveMap(_srcArray->Path(i));
		}
	}
	if (info.mode != DELETE_MODE) {
		SetUseDriveMap(_dstArray->Path(0));
	}
	useDrives |= driveMng.OccupancyDrives(useDrives);
	flagOvl = (info.maxOvlNum >= 2) ? FILE_FLAG_OVERLAPPED : 0;

	// command
	if (info.mode == DELETE_MODE) {
		srcArray = *_srcArray;
		if (InitDeletePath(0) == FALSE)
			return	FALSE;
	}
	else if (info.mode == TEST_MODE) {
		srcArray.RegisterPath(L"C:\\");	// 一旦 C:\ で初期化
		dstArray = *_dstArray;

		if (InitDstPath() == FALSE)
			return	FALSE;
		if (InitSrcPath(0) == FALSE)
			return	FALSE;
		srcArray = *_srcArray;
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
	if (errBuf.UsedSize() > 0) {
		errBuf.FreeBuf();
		errBuf.AllocBuf(MIN_ERR_BUF, MAX_ERR_BUF);
	}

	size_t	maxTransSize = info.maxOvlSize * info.maxOvlNum;
	ssize_t	allocSize = ssize_t(isListingOnly ?
		MAX_LIST_BUF : info.bufSize + (PAGE_SIZE * (4 + (info.bufSize / maxTransSize))));
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
//	if (need_mainbuf /*&& info.mode == TEST_MODE*/) {
//		uint64 *end = (uint64 *)(mainBuf.Buf() + allocSize);
//		uint64 val  = 0xf7f6f5f4f3f2f1f0ULL;
//		for (uint64 *p = (uint64 *)mainBuf.Buf(); p < end; p++) {
//			*p = val++;
//		}
//	}
#endif

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
	opList.Init(info.maxOpenFiles);

	if (UseHardlink()) {
		hardLinkDst = new WCHAR [MAX_WPATH + MAX_PATH];
		memcpy(hardLinkDst, dst, dstBaseLen * sizeof(WCHAR));
		if (!hardLinkList.Init(info.maxLinkHash))
			return	ConfirmErr(L"Can't alloc memory(hardlink)", NULL, CEF_STOP), FALSE;
	}

	if (IsUsingDigestList()) {
		digestList.Init(MIN_DIGEST_LIST, info.maxDigestSize, MIN_DIGEST_LIST);
		ssize_t require_size = ((ssize_t)maxReadSize + BIGTRANS_ALIGN) * (info.maxOvlNum + 1);
		require_size = ALIGN_SIZE(require_size, BIGTRANS_ALIGN);
		if (!wDigestList.Init(MIN_BUF, require_size, PAGE_SIZE))
			return	ConfirmErr(L"Can't alloc memory(digest)", NULL, CEF_STOP), FALSE;
	}

	if (info.verifyFlags & VERIFY_FILE) {
		TDigest::Type	hashType =
			(info.verifyFlags & VERIFY_MD5)    ? TDigest::MD5    :
			(info.verifyFlags & VERIFY_SHA1)   ? TDigest::SHA1   :
			(info.verifyFlags & VERIFY_SHA256) ? TDigest::SHA256 : TDigest::XXHASH;

		if (!srcDigest.Init(hashType) || !dstDigest.Init(hashType)) {
			return	ConfirmErr(L"Can't intialize hash", NULL, CEF_STOP), FALSE;
		}

		if (isListingOnly) {
			srcDigest.buf.AllocBuf(0, MaxReadDigestBuf());
			dstDigest.buf.AllocBuf(0, MaxReadDigestBuf());
		}
	}

	if (info.mode == MOVE_MODE) {
		if (!moveList.Init(MIN_MOVEPATH_LIST, info.maxMoveSize, MIN_MOVEPATH_LIST))
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

	memset(&preTotal, 0, sizeof(preTotal));
	memset(&total, 0, sizeof(total));
	curTotal = isPreSearch ? &preTotal : &total;

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
	opCv.Initialize();
	readReqList.Init();
	writeReqList.Init();
	writeWaitList.Init();
	rDigestReqList.Init();

	if (AllocBuf() == FALSE) goto ERR;

	startTick = ::GetTick();
	preTick = 0;

	if (ti) GetTransInfo(ti, FALSE);

	if (info.mode == DELETE_MODE) {
		hReadThread = (HANDLE)_beginthreadex(0, 0, DeleteThread, this, 0, &id);
		if (!hReadThread) goto ERR;
		return	TRUE;
	}

	if (info.mode == TEST_MODE) {
	//	if (isListingOnly) goto ERR;
		hWriteThread  = (HANDLE)_beginthreadex(0, 0, WriteThread, this, 0, &id);
		hReadThread   = (HANDLE)_beginthreadex(0, 0, TestThread, this, 0, &id);
		if (!hWriteThread || !hReadThread) goto ERR;
		return	TRUE;
	}

	hWriteThread  = (HANDLE)_beginthreadex(0, 0, WriteThread, this, 0, &id);
	hReadThread   = (HANDLE)_beginthreadex(0, 0, ReadThread, this, 0, &id);

	opRun = TRUE;
	openThreadCnt = 0;
	for (auto &i=openThreadCnt; i < MAX_OPENTHREADS; i++) {
		if (!(hOpenThreads[i] = (HANDLE)_beginthreadex(0, 0, OpenThread, this, 0, &id))) {
			break;
		}
	}

	if (!hWriteThread || !hReadThread) goto ERR;

	if (IsUsingDigestList()) {
		hRDigestThread = (HANDLE)_beginthreadex(0, 0, RDigestThread, this, 0, &id);
		hWDigestThread = (HANDLE)_beginthreadex(0, 0, WDigestThread, this, 0, &id);
		if (!hRDigestThread || !hWDigestThread) goto ERR;
	}
	return	TRUE;

ERR:
	EndCore();
	return	FALSE;
}

BOOL FastCopy::PutList(WCHAR *path, DWORD opt, DWORD lastErr, int64 wtime, int64 fsize,
	BYTE *digest)
{
	BOOL	add_backslash = path[0] == '\\' && path[1] != '\\';

	if (!listBuf.Buf() || isPreSearch) return FALSE;

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
			WCHAR	*buf = (WCHAR *)listBuf.UsedEnd();
			len =  wcscpyz(buf,       path);
			len += wcscpyz(buf + len, L"\r\n");
		}
		else {	// 書式化は UI側コンテキストで行いたいところだが…
			WCHAR	wbuf[128];
			if (!(info.fileLogFlags & FILELOG_FILESIZE))  fsize = -1;
			if (!(info.fileLogFlags & FILELOG_TIMESTAMP)) wtime = -1;
			if (opt & (PL_REPARSE | PL_HARDLINK)) {
				fsize = -1;
				digest = NULL;
			}
			if (fsize >= 0 || wtime >= 0 || digest) {
				WCHAR	*start = wbuf + wcscpyz(wbuf, L"   <");
				WCHAR	*p = start;

				if (wtime >= 0) {
					SYSTEMTIME	sst={};
					SYSTEMTIME	lst={};
					FileTimeToSystemTime((FILETIME *)&wtime, &sst);
					SystemTimeToTzSpecificLocalTime (NULL, &sst, &lst);
					p += swprintf(p, L"%04d%02d%02d-%02d%02d%02d",
						lst.wYear, lst.wMonth, lst.wDay, lst.wHour, lst.wMinute, lst.wSecond);
				}
				if (fsize >= 0) {
					if (p != start) *p++ = ' ';
					//p += wcscpyz(p, L"size=");
					p += comma_int64(p, fsize);
				}
				if (digest) {
					int len = dstDigest.GetDigestSize();
					if (p != start) *p++ = ' ';
					p += wcscpyz(p,
						(len == MD5_SIZE)    ? L"md5="    :
						(len == SHA1_SIZE)   ? L"sha1="   :
						(len == SHA256_SIZE) ? L"sha256=" :
						(len == XXHASH_SIZE) ? L"xxhash=" : L"none");
					p += bin2hexstrW(digest, len, p);
				}
				*p++ = '>';
				*p++ = 0;
			}
			else wbuf[0] = 0;

			len = swprintf((WCHAR *)listBuf.UsedEnd(), FMT_PUTLIST,
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

//	DBG("DisableLocalBuffering=%llx\n", hFile);

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
	bool	useOvl = (flagOvl && info.IsMinOvl(file_size));
	DWORD	flg = ((info.flags & USE_OSCACHE_READ) ? 0 : FILE_FLAG_NO_BUFFERING)
				| FILE_FLAG_SEQUENTIAL_SCAN | (useOvl ? flagOvl : 0)
				| (enableBackupPriv ? FILE_FLAG_BACKUP_SEMANTICS : 0);
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

	//DebugW(L"CreateFile(MakeDigest) %d %s\n", isExec, path);

	HANDLE	hFile = CreateFileWithRetry(path, GENERIC_READ, share, 0, OPEN_EXISTING, flg, 0, 5);
	if (hFile == INVALID_HANDLE_VALUE) return FALSE;

	FsType&	fsType = is_src ? srcFsType : dstFsType;
	if ((info.flags & USE_OSCACHE_READ) == 0 && IsNetFs(fsType)) {
		DisableLocalBuffering(hFile);
	}

	if ((DWORD)dbuf->buf.Size() < MaxReadDigestBuf() && !dbuf->buf.Grow(MaxReadDigestBuf()))
		goto END;

	int64	total_size = 0;
	int64	order_total = 0, dummy = 0;
	int64	&verifyTrans = is_src ? curTotal->verifyTrans : dummy;	// src/dstダブルカウント避け
	DWORD	count = 0;
	DWORD	max_trans = maxDigestReadSize;
	if (useOvl && !info.IsNormalOvl(file_size)) {
		max_trans = info.GenOvlSize(file_size);
	}

	while (total_size < file_size && !isAbort) {
		OverLap	*ovl = ovl_list.GetObj(FREE_LIST);

		ovl_list.PutObj(USED_LIST, ovl);
		ovl->buf = dbuf->buf.Buf() + (max_trans * (count++ % info.maxOvlNum));
		ovl->SetOvlOffset(total_size + order_total);

		if (!(ret = ReadFileWithReduce(hFile, ovl->buf, max_trans, ovl))) break;
		order_total += ovl->orderSize;

		BOOL is_empty_ovl = ovl_list.IsEmpty(FREE_LIST);
		BOOL is_flush = (total_size + order_total >= file_size)
			|| (is_src ? readInterrupt : writeInterrupt) || !ovl->waiting;
		if (!is_empty_ovl && !is_flush) continue;

		while (OverLap	*ovl_tmp = ovl_list.GetObj(USED_LIST)) {
			ovl_list.PutObj(FREE_LIST, ovl_tmp);
			if (!(ret = WaitOvlIo(hFile, ovl_tmp, &total_size, &order_total))) {
				ConfirmErr(L"MakeDigest(WaitOvl)", path + (is_src ? srcPrefixLen : dstPrefixLen));
				break;
			}
			ret = dbuf->Update(ovl_tmp->buf, ovl_tmp->transSize);
			verifyTrans += ovl_tmp->transSize; 
			if ((!is_flush && !waitLv) || !ret || isAbort) break;
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

int FastCopy::MakeRenameName(WCHAR *buf, int count, WCHAR *fname, BOOL is_dir)
{
	WCHAR	*ext = is_dir ? NULL : wcsrchr(fname, '.');
	int		body_limit = ext ? int(ext - fname) : MAX_PATH;

	return	swprintf(buf, FMT_RENAME, body_limit, fname, count, ext ? ext : L"");
}

void FastCopy::SetTotalErrInfo(BOOL is_stream, int64 err_trans, BOOL is_write)
{
	if (is_stream) {
		if (is_write) {
			curTotal->wErrAclStream++;
			curTotal->wErrStreamTrans += err_trans;
		}
		else {
			curTotal->rErrAclStream++;
			curTotal->rErrStreamTrans += err_trans;
		}
	}
	else {
		if (is_write) {
			curTotal->wErrFiles++;
			curTotal->wErrTrans += err_trans;
		}
		else {
			curTotal->rErrFiles++;
			curTotal->rErrTrans += err_trans;
		}
	}
}

void FastCopy::IoAbortFile(HANDLE hFile, OvlList *ovl_list)
{
	::CancelIo(hFile);

	while (OverLap *ovl = ovl_list->GetObj(USED_LIST)) {
		WaitOverlapped(hFile, ovl);
		ovl_list->PutObj(FREE_LIST, ovl);
	}
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
	while (dstAsyncRequest != DSTREQ_NONE && !isAbort) {
		CvWait(&cv, CV_WAIT_TICK);
	}

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
		dstRequestResult = ReadDstStat(INT_RDC(dstAsyncInfo));
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

BOOL ModifyRealFdat(const WCHAR *path, WIN32_FIND_DATAW *fdat, BOOL backup_flg)
{
	BY_HANDLE_FILE_INFORMATION	bhi;

	if (!GetFileInformation(path, &bhi, backup_flg)) return FALSE;

	fdat->nFileSizeHigh		= bhi.nFileSizeHigh;
	fdat->nFileSizeLow		= bhi.nFileSizeLow;
	fdat->ftCreationTime	= bhi.ftCreationTime;
	fdat->ftLastAccessTime	= bhi.ftLastAccessTime;
	fdat->ftLastWriteTime	= bhi.ftLastWriteTime;
	return	TRUE;
}

BOOL GetFileInformation(const WCHAR *path, BY_HANDLE_FILE_INFORMATION *bhi, BOOL backup_flg)
{
	DWORD	share = FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;
	HANDLE	hFile = ::CreateFileW(path, 0, share, 0, OPEN_EXISTING,
		(backup_flg ? FILE_FLAG_BACKUP_SEMANTICS : 0), 0);

	if (hFile == INVALID_HANDLE_VALUE) return FALSE;

	BOOL ret = ::GetFileInformationByHandle(hFile, bhi);
	::CloseHandle (hFile);

	return	ret;
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

	if ((fh = ::FindFirstFileExW(confirmDst, findInfoLv, &fdat, FindExSearchNameMatch,
		NULL, findFlags)) == INVALID_HANDLE_VALUE) {
		if (::GetLastError() != ERROR_FILE_NOT_FOUND
		&& wcscmp(confirmDst + dstBaseLen, L"*") == 0) {
			ret = FALSE;
			curTotal->wErrDirs++;
			ConfirmErr(L"FindFirstFileEx(stat)", confirmDst + dstPrefixLen);
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
			ModifyRealFdat(confirmDst, &fdat, enableBackupPriv);
		}
		len = FdatToFileStat(&fdat, dstStat, TRUE);
		dstStatBuf.AddUsedSize(len);

		if (dstStatBuf.RemainSize() <= maxStatSize && dstStatBuf.Grow(MIN_ATTR_BUF) == FALSE) {
			ConfirmErr(L"Can't alloc memory(dstStatBuf)", NULL, CEF_STOP);
			break;
		}
		dstStat = (FileStat *)dstStatBuf.UsedEnd();
	}
	while (!isAbort && ::FindNextFileW(fh, &fdat));

	if (!isAbort && ::GetLastError() != ERROR_NO_MORE_FILES) {
		curTotal->wErrFiles++;
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
	size_t	data_num = statIdxVec->UsedNum();
	hashNum = data_num | 7;

	// hash table を Used の直後に確保
	ssize_t	require_size = hashNum * sizeof(FileStat *);
	ssize_t	grow_size    = require_size - statIdxVec->RemainSize();

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
  関  数 ： RDigestThread
  概  要 ： RDigestThread 処理
  説  明 ： 
  注  意 ： 
=========================================================================*/
unsigned WINAPI FastCopy::RDigestThread(void *fastCopyObj)
{
	return	((FastCopy *)fastCopyObj)->RDigestThreadCore();
}

BOOL FastCopy::RDigestThreadCore(void)
{
	int64	fileID = 0;
	int64	remain_size = 0;

	cv.Lock();

	while (!isAbort) {
		while (rDigestReqList.IsEmpty() && !isAbort) {
			CvWait(&cv, CV_WAIT_TICK);
		}
		ReqHead *req = rDigestReqList.TopObj();
		if (!req || isAbort) break;

		Command cmd = req->cmd;

		FileStat	*stat = &req->stat;
		if ((cmd == WRITE_FILE || cmd == WRITE_BACKUP_FILE
				|| (cmd == WRITE_FILE_CONT && fileID == stat->fileID))
			&& stat->FileSize() > 0 && !IsReparseEx(stat->dwFileAttributes)) {
			cv.UnLock();
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
  関  数 ： WDigestThread
  概  要 ： WDigestThread 処理
  説  明 ： 
  注  意 ： 
=========================================================================*/
unsigned WINAPI FastCopy::WDigestThread(void *fastCopyObj)
{
	return	((FastCopy *)fastCopyObj)->WDigestThreadCore();
}

BOOL FastCopy::WDigestThreadCore(void)
{
	int64			fileID = 0;
	DigestCalc		*calc = NULL;

	wDigestList.Lock();	// DataList は Get()後、UnLockで再利用される可能性が出るため、
						// Peek中に必要な処理を終えて、Getは remove処理として実施
	while (1) {
		DataList::Head	*head;
		while ((!(head = wDigestList.Peek())
		|| (calc = (DigestCalc *)head->data)->status == DigestCalc::INIT) && !isAbort) {
			CvWait(wDigestList.GetCv(), CV_WAIT_TICK);
		}
		if (isAbort) {
			break;
		}
		if (calc->status == DigestCalc::FIN) {
			wDigestList.Get();
			wDigestList.Notify();
			break;
		}

		if (calc->status == DigestCalc::CONT || calc->status == DigestCalc::DONE) {
			if (calc->dataSize) {
				wDigestList.UnLock();
				if (fileID != calc->fileID) {
					dstDigest.Reset();
				}
				dstDigest.Update(calc->data, calc->dataSize);
				if (calc->status == DigestCalc::DONE) {
					dstDigest.GetVal(dstDigest.val);
				}
//				curTotal->verifyTrans += calc->dataSize;
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
					calc->status = DigestCalc::ERR;
					VerifyErrPostProc(calc);
				}
			}
			else if (isListing) {
				dstDigest.GetEmptyVal(calc->digest);
			}
		}

		if (calc->status == DigestCalc::DONE) {
			DWORD ftype = IsReparseEx(calc->dwAttr) ? PL_REPARSE :
				(calc->dwAttr == 0 && calc->fileSize == 0) ? PL_HARDLINK : PL_NORMAL;
			if (isListing) {
				PutList(calc->path + dstPrefixLen, ftype, 0, calc->wTime, calc->fileSize,
					calc->digest);
			}
			if (info.mode == MOVE_MODE) {
				SetFinishFileID(calc->fileID, MoveObj::DONE);
			}
			if (ftype != PL_REPARSE) {
				curTotal->verifyFiles++;
			}
		}
		else if (calc->status == DigestCalc::ERR || calc->status == DigestCalc::PRE_ERR) {
			if (info.mode == MOVE_MODE) {
				SetFinishFileID(calc->fileID, MoveObj::ERR);
			}
			// PRE_ERR は WriteDigestProc(...DigestObj::NG) でカウント・出力済み
			if (calc->status == DigestCalc::ERR) {
				curTotal->rErrFiles++;
				if (isListing) {
					PutList(calc->path + dstPrefixLen, PL_NORMAL|PL_COMPARE, 0, calc->wTime,
						calc->fileSize, calc->digest);
				}
			}
		}
		else if (calc->status == DigestCalc::PASS) {
			// pass for async io error
		}
		else if (calc->status == DigestCalc::CONT) {
			// cont
		}
//		else {
//			ConfirmErr(FmtW(L"WDigest calc status err(%d)", calc->status), calc->path, CEF_STOP);
//		}
		wDigestList.Get();	// remove from wDigestList
		wDigestList.Notify();
		calc = NULL;
	}
	wDigestList.UnLock();

	return	isAbort;
}

BOOL FastCopy::VerifyErrPostProc(DigestCalc *calc)
{
	int		len = (int)wcslen(calc->path);
	Wstr	wbuf((len + 20) + 1024); // path + digest_msg + misc_msg

	MakeVerifyStr(wbuf.Buf(), calc->digest, dstDigest.val, dstDigest.GetDigestSize());

	if (info.verifyFlags & VERIFY_REMOVE) {
		BOOL	ret = ForceDeleteFileW(dst, FMF_ATTR|info.aclReset);

		if (ret) {
			swprintf(wbuf.Buf() + wcslen(wbuf.s()),
				L"in %s and it was deleted.", calc->path + dstPrefixLen);
			ConfirmErr(wbuf.s(), calc->path + dstPrefixLen, CEF_NOAPI);
			return	ret;
		}
		swprintf(wbuf.Buf() + wcslen(wbuf.s()),
			L"in %s and it was tried to delete, but it was failed.\r\n So try to rename...",
			calc->path + dstPrefixLen);
		ConfirmErr(wbuf.s(), calc->path + dstPrefixLen);
		// try to rename...
	}

	Wstr	wname(len + 20);
	wcscpy(wname.Buf(), calc->path);
	wcscpy(wname.Buf() + len, L".fc_verify_err");

	BOOL	ret = ::MoveFileExW(calc->path, wname.s(), MOVEFILE_REPLACE_EXISTING);

	if (ret) {
		swprintf(wbuf.Buf() + wcslen(wbuf.s()),
			L"in %s and it was renamed.\r\n Please check later", calc->path + dstPrefixLen);
		ConfirmErr(wbuf.s(), wname.s() + dstPrefixLen, CEF_NOAPI);
	}
	else {
		swprintf(wbuf.Buf() + wcslen(wbuf.s()),
			L"in %s and it was tried to rename, but it was failed.\r\n Please check later",
			calc->path + dstPrefixLen);
		ConfirmErr(wbuf.s(), calc->path + dstPrefixLen);
	}

	return	ret;
}

FastCopy::DigestCalc *FastCopy::GetDigestCalc(DigestObj *obj, DWORD io_size)
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
	DWORD			require_size = sizeof(DigestCalc) + (obj ? obj->pathLen * sizeof(WCHAR) : 0);

	if (io_size) {
		require_size += io_size + dstSectorSize;
	}
	wDigestList.Lock();

	while (wDigestList.RemainSize() <= require_size && !isAbort) {
		CvWait(wDigestList.GetCv(), CV_WAIT_TICK);
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
		CvWait(&cv, CV_WAIT_TICK);

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
			CvWait(moveList.GetCv(), CV_WAIT_TICK);
		}
	} while (!isAbort && !moveFinPtr);

	if (moveList.IsWait()) {
		moveList.Notify();
	}
	moveList.UnLock();
	return	TRUE;
}


BOOL FastCopy::EndCore()
{
	isAbort = TRUE;

	while (hWriteThread || hReadThread || hRDigestThread || hWDigestThread) {
		cv.Lock();
		cv.Notify();
		cv.UnLock();

		DWORD wret = 0;
		if (hReadThread) {
			if ((wret = ::WaitForSingleObject(hReadThread, 1000)) == WAIT_OBJECT_0) {
				::CloseHandle(hReadThread);
				hReadThread = NULL;
			}
			else if (wret != WAIT_TIMEOUT) {
				ConfirmErr(FmtW(L"Illegal WaitForSingleObject r2(%x) %p ab=%d",
					wret, hReadThread, isAbort), NULL, CEF_STOP);
				break;
			}
		}
		else if (hWriteThread) {	// hReadThread が生きている場合は、hReadTread に Closeさせる
			if ((wret = ::WaitForSingleObject(hWriteThread, 1000)) == WAIT_OBJECT_0) {
				::CloseHandle(hWriteThread);
				hWriteThread = NULL;
			}
			else if (wret != WAIT_TIMEOUT) {
				ConfirmErr(FmtW(L"Illegal WaitForSingleObject w2(%x) %p ab=%d",
					wret, hWriteThread, isAbort), NULL, CEF_STOP);
				break;
			}
		}
		else if (hRDigestThread) {
			if ((wret = ::WaitForSingleObject(hRDigestThread, 1000)) == WAIT_OBJECT_0) {
				::CloseHandle(hRDigestThread);
				hRDigestThread = NULL;
			}
			else if (wret != WAIT_TIMEOUT) {
				ConfirmErr(FmtW(L"Illegal WaitForSingleObject rd2(%x) %p ab=%d",
					wret, hRDigestThread, isAbort), NULL, CEF_STOP);
				break;
			}
		}
		else if (hWDigestThread) {
			if ((wret = ::WaitForSingleObject(hWDigestThread, 1000)) == WAIT_OBJECT_0) {
				::CloseHandle(hWDigestThread);
				hWDigestThread = NULL;
			}
			else if (wret != WAIT_TIMEOUT) {
				ConfirmErr(FmtW(L"Illegal WaitForSingleObject wd2(%x) %p ab=%d",
					wret, hWDigestThread, isAbort), NULL, CEF_STOP);
				break;
			}
		}
		else if (openThreadCnt > 0) {
			opRun = FALSE;
			opCv.Lock();
			opCv.Notify();
			opCv.UnLock();
			for (int i=openThreadCnt-1; i >= 0; i--) {
				if ((wret = ::WaitForSingleObject(hOpenThreads[i], 1000)) == WAIT_OBJECT_0) {
					::CloseHandle(hOpenThreads[i]);
					hOpenThreads[i] = NULL;
					openThreadCnt--;
				}
				else if (wret != WAIT_TIMEOUT) {
					ConfirmErr(FmtW(L"Illegal WaitForSingleObject wd(%x) %p ab=%d",
						wret, hWDigestThread, isAbort), NULL, CEF_STOP);
					break;
				}
			}
			if (wret != WAIT_OBJECT_0 && wret != WAIT_TIMEOUT) break;
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
	opList.UnInit();
	srcArray.Init();
	dstArray.Init();

	delete [] hardLinkDst;
	hardLinkDst = NULL;
	CleanRegFilter();

	return	TRUE;
}

BOOL FastCopy::End(void)
{
	BOOL	ret = EndCore();

	if (errBuf.UsedSize() > 0) {
		errBuf.FreeBuf();
		errBuf.AllocBuf(MIN_ERR_BUF, MAX_ERR_BUF);
	}
	return	ret;
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
	suspendTick = ::GetTick();

	return	TRUE;
}

BOOL FastCopy::Resume(void)
{
	if (!hReadThread && !hWriteThread || !isSuspend)
		return	FALSE;

	isSuspend = FALSE;
	startTick += (::GetTick() - suspendTick);

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

void CalcTotalSum(TotalTrans *total)
{
	total->allErrFiles	=	total->rErrFiles +
							total->wErrFiles +
							total->dErrFiles;

	total->allErrDirs	=	total->rErrDirs +
							total->wErrDirs +
							total->dErrDirs;

	total->allErrTrans	=	total->rErrTrans +
							total->wErrTrans +
							total->dErrTrans;
}

BOOL FastCopy::GetTransInfo(TransInfo *ti, BOOL fullInfo)
{
	ti->isPreSearch = isPreSearch;
	ti->preTotal = preTotal;
	ti->total = total;
	ti->listBuf = &listBuf;
	ti->listCs = &listCs;
	ti->errBuf = &errBuf;
	ti->errCs = &errCs;
	ti->isSameDrv = isSameDrv;
	ti->ignoreEvent = info.ignoreEvent;
	ti->waitLv = waitLv;
	DWORD	last_tick = isSuspend ? suspendTick : endTick ? endTick : startTick ? ::GetTick() : 0;
	ti->fullTickCount = last_tick - startTick;
	ti->execTickCount = last_tick - max(preTick, startTick);

	CalcTotalSum(&ti->preTotal);
	CalcTotalSum(&ti->total);

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
	memset(stat->digest, 0, SHA256_SIZE);

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

	if (is_abort) {
		isAbort = is_abort;
	}
	if (!is_abort && isPreSearch) {
		return	Confirm::CONTINUE_RESULT;
	}

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
		suspendTick = ::GetTick();

		::SendMessage(info.hNotifyWnd, info.uNotifyMsg, CONFIRM_NOTIFY, (LPARAM)&confirm);

		isSuspend = FALSE;
		startTick += (::GetTick() - suspendTick);
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
		if (!isAbort) {
			Aborting((info.ignoreEvent & FASTCOPY_STOP_EVENT) ? TRUE : FALSE);
			curTotal->abortCnt++;
		}
		break;
	}
	return	confirm.result;
}

BOOL FastCopy::WriteErrLog(const WCHAR *message, int len)
{
#define ERRMSG_SIZE		60
	::EnterCriticalSection(&errCs);

	BOOL	ret = TRUE;
	WCHAR	*msg_buf = (WCHAR *)errBuf.UsedEnd();

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

void FastCopy::Aborting(BOOL is_auto)
{
	isAbort = TRUE;
	WCHAR	err_msg[256];
	swprintf(err_msg, L"%s%s", L" Aborted by User", is_auto ? L" (automatic)" : L"");
	WriteErrLog(err_msg);
	if (isExec && isListing) {
		PutList(err_msg, PL_ERRMSG);
	}
}



void FastCopy::CvWait(Condition *targ_cv, DWORD tick)
{
	WaitCalc	*wc = NULL;
	DWORD64		t1 = NULL;

	if (waitLv) {
		if (wc = (WaitCalc *)::TlsGetValue(wcIdx)) {
			t1 = ::GetTick64();
		}
	}

	targ_cv->Wait(tick);

	if (wc && wc->lastTick) {
		auto	t2 = ::GetTick64();
		wc->lastTick += t2 - t1;
		// if (t1 != t2) Debug("Add %lld\n", t2 - t1);
	}
}

void FastCopy::WaitCheck()
{
	WaitCalc	*wc = (WaitCalc *)::TlsGetValue(wcIdx);

	if (!wc) {
//		Debug("not set wc\n");
		return;
	}

	if ((waitLv && !wc->lowPriority) || (!waitLv && wc->lowPriority)) {
		::SetThreadPriority(GetCurrentThread(),
			waitLv ? THREAD_MODE_BACKGROUND_BEGIN : THREAD_MODE_BACKGROUND_END);
		wc->lowPriority = (waitLv ? true : false);
//		Debug("SetThreadPriority(%x) = %d\n", GetCurrentThreadId(), wc->lowPriority);
	}

	if (waitLv <= AUTOSLOW_WAITLV) {
		return;
	}
	int64	cur = (int64)GetTick64();
	auto	mainTick = cur - wc->lastTick;

	if (wc->lastTick == 0 || mainTick == 0) {
		wc->lastTick = cur;
		return;
	}
	wc->lastTick = cur;

	if (mainTick >= 1000) {
		mainTick = 1000;
	}

	auto	ratio = (100 - waitLv) / 100.0;
	if (IsNetFs(wc->fsType)) {
		ratio /= 1.4;
	}
	else if (!IsSSD(wc->fsType)) {	// HDDの場合、HDD cacheが効きすぎる
		ratio /= 2;
	}
	auto	remain = int(mainTick * (1-ratio) / ratio);

	remain += wc->lastRemain;

	remain = min(remain, 700);

	if (remain > 0) {
//		Debug("remain(%x)=%ld\n", GetCurrentThreadId(), remain);

		auto	lastCur = cur;
		while (remain > 0 && !isAbort) {
#define SLEEP_UNIT	200
			DWORD64	unit = remain > SLEEP_UNIT ? SLEEP_UNIT : 1;
			::Sleep((DWORD)unit);
			cur = (int64)GetTick64();
			if (waitLv != SUSPEND_WAITLV) {
				remain -= int(cur - lastCur);
				if (remain < -500) {	// suspend?
					remain = 0;
				}
			}
			lastCur = cur;
		}
		wc->lastTick  = cur;
		if (remain < 0) {
//			Debug("pool(%x)=%ld\n", GetCurrentThreadId(), remain);
		}
	}
	else {
//		Debug("skip(%x) %d\n", GetCurrentThreadId(), remain);
	}
	wc->lastRemain = remain;
}

unsigned WINAPI FastCopy::TestThread(void *fastCopyObj)
{
	return	((FastCopy *)fastCopyObj)->TestWrite();
}

/*
	テスト用ファイル作成
	サンプル例）1025byte のファイルを 10dir * 100個 = 計1000個作成
	 Source:  1k+1,10,100
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

	if      (wcschr(targ.s(), 'T')) fsize *= 1024LL * 1024 * 1024 * 1024;
	else if (wcschr(targ.s(), 'G')) fsize *= 1024 * 1024 * 1024;
	else if (wcschr(targ.s(), 'M')) fsize *= 1024 * 1024;
	else if (wcschr(targ.s(), 'K')) fsize *= 1024;

	return	fsize + other_fsize;
}

BOOL FastCopy::TestWrite()
{
	WaitCalc	wc;

	::TlsSetValue(wcIdx, &wc);
	wc.fsType = dstFsType;

	int64	fsize = 0;
	int		dnum  = 0;
	int		fnum  = 0;
	BOOL	ret = FALSE;

	FILETIME			cur;
	UnixTime2FileTime(time(NULL), &cur);
	WIN32_FIND_DATAW	ddat = { FILE_ATTRIBUTE_DIRECTORY, cur, cur, cur };
	WIN32_FIND_DATAW	fdat = { FILE_ATTRIBUTE_NORMAL, cur, cur, cur };

	WCHAR	*tok = wcstok(srcArray.Path(0), L"=");
	if (!tok || wcsicmp(tok, L"w") != 0) goto END;

	if (!(tok = wcstok(NULL, L", "))) goto END;
	if ((fsize = CalcFsize(tok)) < 0) goto END;

	if (!(tok = wcstok(NULL, L", "))) goto END;
	if ((dnum = _wtoi(tok)) <= 0) goto END;

	if (!(tok = wcstok(NULL, L", "))) goto END;
	if ((fnum = _wtoi(tok)) <= 0) goto END;

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

			if (!isExec) {
				SendRequest(cmd, 0, &fstat);
				continue;
			}
			do {
				ReqHead	*req = PrepareReqBuf(offsetof(ReqHead, stat) + fstat.minSize,
					remain_size, fstat.fileID, fsize);
				if (req) {
					req->readSize = (remain_size > req->bufSize) ?
						req->bufSize : (DWORD)remain_size;
					remain_size -= req->readSize;
					SendRequest(cmd, req, &fstat);
					cmd = WRITE_FILE_CONT;
				}
			} while (remain_size > 0 && !isAbort);
		}
		SendRequest(RETURN_PARENT);
		ret = isAbort ? FALSE : TRUE;
	}
	SendRequest(REQ_EOF);

END:
	if (!ret) {
		ConfirmErr(L"format err (w=file-size,n-dirs,n-files only, and r= is not implemented)"
			, NULL, CEF_STOP|CEF_NOAPI);
	}
	TestWriteEnd();
	return ret;
}

void FastCopy::TestWriteEnd()
{
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
}

void FastCopy::OvlLog(OverLap *ovl, const void *buf, const WCHAR *fmt,...)
{
	WCHAR	wbuf[512], *p = wbuf;
	p += swprintf(wbuf, L"ovl(%10lld/%8d/%8d/%d) %p err=%d: ",
		ovl->GetOvlOffset(), ovl->orderSize, ovl->transSize, ovl->waiting, buf, GetLastError());

	va_list	va;
	va_start(va, fmt);
	vswprintf(p, fmt, va);
	va_end(va);

	WriteErrLog(wbuf);
}

