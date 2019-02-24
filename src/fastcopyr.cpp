static char *fastcopyr_id = 
	"@(#)Copyright (C) 2004-2018 H.Shirouzu and FastCopy Lab, LLC.	fastcopyr.cpp	ver3.50";
/* ========================================================================
	Project  Name			: Fast Copy file and directory (read)
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
	WaitCalc	wc;

	::TlsSetValue(wcIdx, &wc);
	wc.fsType = srcFsType;

	BOOL	isSameDrvOld;

//	if (info.flags & PRE_SEARCH)
//		PreSearch();

	while (!isAbort) {
		SendRequest(CHECKCREATE_TOPDIR);

		for (int i=0; i < srcArray.Num(); i++) {
			if (InitSrcPath(i)) {
				if (i >= 1 && isSameDrvOld != isSameDrv) {
					ChangeToWriteMode();
				}
				ReadProc(srcBaseLen, info.overWrite == BY_ALWAYS ? FALSE : TRUE,
							(FilterBits(DIR_REG, INC_REG) & filterMode) ? FR_CONT : FR_MATCH);
				isSameDrvOld = isSameDrv;
			}
			if (isAbort) {
				break;
			}
		}
		if (!isPreSearch || isAbort) {
			break;
		}
		if (isSameDrv) {
			ChangeToWriteMode();
		}

		BOOL	is_wait = TRUE;
		cv.Lock();
		while (!isAbort && is_wait) {
			is_wait =	!readReqList.IsEmpty() || !rDigestReqList.IsEmpty() ||
						!writeReqList.IsEmpty() || !writeWaitList.IsEmpty() ||
						writeReq || runMode == RUN_DIGESTREQ;
			if (is_wait) {
				CvWait(&cv, CV_WAIT_TICK);
			}
		}
		cv.UnLock();

		if (info.mode == MOVE_MODE && !isAbort) {
			FlushMoveList(TRUE);
		}

		Debug("pre -> exec\n");
		isPreSearch = FALSE;
		curTotal = &total;
		isExec = TRUE;
		preTick = ::GetTick();
		if (UseHardlink()) {
			if (hardLinkList.GetRegisterNum()) {
				hardLinkList.Reset();
			}
		}
	}

	SendRequest(REQ_EOF);

	if (isSameDrv) {
		ChangeToWriteMode(TRUE);
	}
	if (info.mode == MOVE_MODE && !isAbort) {
		FlushMoveList(TRUE);
	}

	while (hWriteThread || hRDigestThread || hWDigestThread || openThreadCnt) {
		DWORD	wret = 0;
		if (hWriteThread) {
			if ((wret = ::WaitForSingleObject(hWriteThread, 1000)) == WAIT_OBJECT_0) {
				::CloseHandle(hWriteThread);
				hWriteThread = NULL;
			}
			else if (wret != WAIT_TIMEOUT) {
				ConfirmErr(FmtW(L"Illegal WaitForSingleObject w(%x) %p ab=%d",
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
				ConfirmErr(FmtW(L"Illegal WaitForSingleObject rd(%x) %p ab=%d",
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
				ConfirmErr(FmtW(L"Illegal WaitForSingleObject wd(%x) %p ab=%d",
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

	FinishNotify();

	return	TRUE;
}

//BOOL FastCopy::PreSearch(void)
//{
//	BOOL	is_delete = info.mode == DELETE_MODE;
//	BOOL	(FastCopy::*InitPath)(int) = is_delete ?
//				&FastCopy::InitDeletePath : &FastCopy::InitSrcPath;
//	WCHAR	*&path = is_delete ? dst : src;
//	int		&prefix_len = is_delete ? dstPrefixLen : srcPrefixLen;
//	int		&base_len = is_delete ? dstBaseLen : srcBaseLen;
//	BOOL	ret = TRUE;
//
//	for (int i=0; i < srcArray.Num(); i++) {
//		if ((this->*InitPath)(i)) {
//			if (!PreSearchProc(path, prefix_len, base_len,
//							   (FilterBits(DIR_REG, INC_REG) & filterMode) ? FR_CONT : FR_MATCH)) {
//				ret = FALSE;
//			}
//		}
//		if (isAbort)
//			break;
//	}
//	startTick = ::GetTick();
//
//	return	ret && !isAbort;
//}

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
	int		depthNum = (int)depthVec.UsedNum() - depthIdxOffset;
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
		if (depthIdx < excAbs.size()) {
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

		if (depthIdx < incAbs.size()) {
			if ((regExp = incAbs[depthIdx])) {
				if (regExp->IsMatch(dir + depth[0])) return FR_MATCH;
			}
		}
		// 絶対指定incのみでかつ、現在階層以上での部分一致もない場合は探索終了
		if (is_dir && incRel.size() == 0 && incAbs.size() > 0) {
			bool	abs_need_cont = false;
			for (int i=depthIdx+1; i < incAbs.size(); i++) {
				if ((regExp = incAbs[i])) {
					bool	is_mid = false;
					regExp->IsMatch(dir + depth[0], &is_mid);
					if (is_mid) {
						abs_need_cont = true;
						break;
					}
				}
			}
			if (!abs_need_cont) return	FR_UNMATCH;
		}
		return	is_dir ? FR_CONT : FR_UNMATCH;
	}

	return fr;
}

BOOL FastCopy::ClearNonSurrogateReparse(WIN32_FIND_DATAW *fdat)
{
	if (!IsReparse(fdat->dwFileAttributes) || IsReparseTagNameSurrogate(fdat->dwReserved0)) {
		return	FALSE;	// 通常ファイル or symlink/junction等
	}

	fdat->dwFileAttributes &= ~FILE_ATTRIBUTE_REPARSE_POINT;
	return	TRUE;
}

BOOL FastCopy::ReadDirEntry(int dir_len, BOOL confirm_dir, FilterRes fr)
{
	HANDLE	fh;
	BOOL	ret = TRUE;
	int		len;
	WIN32_FIND_DATAW fdat;

	fileStatBuf.SetUsedSize(0);

	if ((fh = ::FindFirstFileExW(src, findInfoLv, &fdat, FindExSearchNameMatch,
		NULL, findFlags)) == INVALID_HANDLE_VALUE) {
		curTotal->rErrDirs++;
		return	ConfirmErr(L"FindFirstFileEx", src + srcPrefixLen), FALSE;
	}
	do {
		if (IsParentOrSelfDirs(fdat.cFileName))
			continue;

		ClearNonSurrogateReparse(&fdat);

		FilterRes	cur_fr = FilterCheck(src, dir_len, &fdat, fr);
		if (cur_fr == FR_UNMATCH) {
			curTotal->filterSrcSkips++;
			continue;
		}
		// ディレクトリ＆ファイル情報の蓄積
		if (IsDir(fdat.dwFileAttributes)) {
			len = FdatToFileStat(&fdat, (FileStat *)dirStatBuf.UsedEnd(), confirm_dir, cur_fr);
			dirStatBuf.AddUsedSize(len);
			if (dirStatBuf.RemainSize() <= maxStatSize && !dirStatBuf.Grow(MIN_ATTR_BUF)) {
				ConfirmErr(L"Can't alloc memory(dirStatBuf)", NULL, CEF_STOP);
				break;
			}
		}
		else {
			if (NeedSymlinkDeref(&fdat)) { // del/moveでは REPARSE_AS_NORMAL は存在しない
				wcscpyz(src + dir_len, fdat.cFileName);
				ModifyRealFdat(src, &fdat, enableBackupPriv);
			}
			len = FdatToFileStat(&fdat, (FileStat *)fileStatBuf.UsedEnd(), confirm_dir, cur_fr);
			fileStatBuf.AddUsedSize(len);
			if (fileStatBuf.RemainSize() <= maxStatSize && !fileStatBuf.Grow(MIN_ATTR_BUF)) {
				ConfirmErr(L"Can't alloc memory(fileStatBuf)", NULL, CEF_STOP);
				break;
			}
		}
	}
	while (!isAbort && ::FindNextFileW(fh, &fdat));

	if (!isAbort && ::GetLastError() != ERROR_NO_MORE_FILES) {
		curTotal->rErrDirs++;
		ret = FALSE;
		ConfirmErr(L"FindNextFile", src + srcPrefixLen);
	}
	::FindClose(fh);

	return	ret && !isAbort;
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
		curTotal->verifyFiles++;
		PutList(confirmDst + dstPrefixLen, PL_NOADD|(IsReparseEx(srcStat->dwFileAttributes) ?
			PL_REPARSE : 0), 0, dstStat->WriteTime(), dstStat->FileSize(), srcDigest.val);
	}
	else {
		curTotal->rErrFiles++;
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

#define MOVE_OPEN_MAX 10
#define WAIT_OPEN_MAX 10

BOOL FastCopy::ReadProc(int dir_len, BOOL confirm_dir, FilterRes fr)
{
	BOOL	ret = TRUE;
	int		curDirStatSize = (int)dirStatBuf.UsedSize(); // カレントのサイズを保存
	BOOL	confirm_local = confirm_dir || isRename;
	int		confirm_len = dir_len + (dstBaseLen - srcBaseLen);

	WaitCheck();

	if (confirm_local && !isSameDrv) DstRequest(DSTREQ_READSTAT, (void *)(LONG_PTR)confirm_len);
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
	FileStat	*statEnd = (FileStat *)fileStatBuf.UsedEnd();

	for (FileStat *srcStat = (FileStat *)fileStatBuf.Buf(); srcStat < statEnd;
			srcStat = (FileStat *)((BYTE *)srcStat + srcStat->size)) {
		FileStat	*dstStat = confirm_local ? hash.Search(srcStat->upperName, srcStat->hashVal)
											 : NULL;
		int			path_len = 0;
		srcStat->fileID = nextFileID++;

		if (info.mode == MOVE_MODE || UseHardlink()) {
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
/* 比較モード */	if (isListingOnly && (info.verifyFlags & VERIFY_FILE)) {
						wcscpy(confirmDst + confirm_len, srcStat->cFileName);
						wcscpy(src + dir_len, srcStat->cFileName);
						if (!IsSameContents(srcStat, dstStat) && !isAbort) {
//							PutList(confirmDst + dstPrefixLen, PL_COMPARE|PL_NOADD);
						}
/* 比較モード */	}
					if (info.mode == MOVE_MODE) {
						 PutMoveList(src, path_len, srcStat, MoveObj::DONE);
					}
					curTotal->skipFiles++;
					curTotal->skipTrans += dstStat->FileSize();
					if (UseHardlink()) {
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
		curTotal->readFiles++;
		if (mkdirQueVec.UsedNum()) {
			ExecMkDirQueue();
		}
		if (!OpenFileProc(srcStat, dir_len)
			/*|| IsNetFs(srcFsType)*/
			|| info.mode == MOVE_MODE && openFilesCnt >= MOVE_OPEN_MAX
			|| waitLv && openFilesCnt >= WAIT_OPEN_MAX || openFilesCnt >= info.maxOpenFiles) {
			ReadMultiFilesProc(dir_len);
			CloseMultiFilesProc();
		}
		if (isAbort) break;
	}
	ReadMultiFilesProc(dir_len);
	CloseMultiFilesProc();

	return	!isAbort;
}

FastCopy::LinkStatus FastCopy::CheckHardLink(WCHAR *path, int len, HANDLE hFileOrg, DWORD *data)
{
	HANDLE		hFile;
	LinkStatus	ret = LINK_ERR;
	DWORD		share = FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;

	//DebugW(L"CreateFile(CheckHardLink) %d %s\n", isExec, path);

	if (hFileOrg == INVALID_HANDLE_VALUE) {
		hFile = ::CreateFileW(path, 0, share, 0, OPEN_EXISTING,
			(enableBackupPriv ? FILE_FLAG_BACKUP_SEMANTICS : 0), 0);
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

BOOL FastCopy::ReadProcDirEntry(int dir_len, int dirst_start, BOOL confirm_dir, FilterRes fr)
{
	BOOL		ret = TRUE;
	int			confirm_len = dir_len + (dstBaseLen - srcBaseLen);
	BOOL		confirm_local = confirm_dir || isRename;
	FileStat	*statEnd = (FileStat *)dirStatBuf.UsedEnd();

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
				curTotal->filterDstSkips++;
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
		int		cur_skips = curTotal->filterSrcSkips;
		curTotal->readDirs++;

		// -1 is remove '*'
		int		new_dir_len = dir_len + wcscpy_with_aster(src + dir_len, srcStat->cFileName) - 1;
		bool	need_extra = false;

		srcStat->fileID = nextFileID++;
		PushDepth(src, new_dir_len);
		if (confirm_dir && srcStat->isExists) {
			int len = wcscpy_with_aster(confirmDst + confirm_len, srcStat->cFileName) - 1;
			PushDepth(confirmDst, confirm_len + len);
		}
		if (isExec && (enableAcl || is_reparse)) {
			need_extra = true;
		}
		if (!PushMkdirQueue(srcStat, new_dir_len-1, (!confirm_dir || !srcStat->isExists),
							need_extra, is_reparse)) {
			return FALSE;	// VBuf error
		}
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
			if (cur_skips == curTotal->filterSrcSkips) {
				PutMoveList(src, new_dir_len, srcStat,
				/*srcStat->isExists ? MoveObj::DONE : MoveObj::START*/ MoveObj::DONE);
			}
		}
	}
	return	ret && !isAbort;
}

BOOL FastCopy::PushMkdirQueue(FileStat *stat, int dlen, bool is_mkdir, bool extra, bool is_reparse)
{
	size_t	idx = mkdirQueVec.UsedNum();
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
	size_t	max_num = mkdirQueVec.UsedNum();

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
	DWORD	mode = GENERIC_READ|READ_CONTROL|acsSysSec;
	DWORD	share = FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;
	DWORD	flg = FILE_FLAG_BACKUP_SEMANTICS | (is_reparse ? FILE_FLAG_OPEN_REPARSE_POINT : 0);
	ReqHead	*req=NULL;

	//DebugW(L"CreateFile(GetDirExtData) %d %s\n", isExec, src);

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
		if (dirStatBuf.RemainSize() <= (ssize_t)size + maxStatSize
		&& dirStatBuf.Grow(ALIGN_SIZE(size + maxStatSize, MIN_ATTR_BUF)) == FALSE) {
			ret = FALSE;
			ConfirmErr(L"Can't alloc memory(dirStatBuf)", NULL, CEF_STOP);
		}
		else if ((size = ReadReparsePoint(fh, dirStatBuf.UsedEnd(), size)) <= 0) {
			ret = FALSE;
			curTotal->rErrDirs++;
			ConfirmErr(L"ReadReparse(Dir)", src + srcPrefixLen);
		}
		else {
			stat->rep = dirStatBuf.UsedEnd();
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

			if (data || sid.Size.HighPart) {	// すでに格納済み（or 4GBを超えるデータ）
				if (info.flags & REPORT_ACL_ERROR)
					ConfirmErr(L"Duplicate or Too big ACL/EADATA(dir)", src + srcPrefixLen);
				break;
			}
			data = dirStatBuf.UsedEnd();
			data_size = sid.Size.LowPart + STRMID_OFFSET;
			if (dirStatBuf.RemainSize() <= maxStatSize + data_size
			&& !dirStatBuf.Grow(ALIGN_SIZE(maxStatSize + data_size, MIN_ATTR_BUF))) {
				ConfirmErr(L"Can't alloc memory(dirStat(ACL/EADATA))",
					src + srcPrefixLen, FALSE);
				break;
			}
			dirStatBuf.AddUsedSize(data_size);
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
			if (req->bufSize >= size) {
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
			else {	// MIN_SIZE(1MB) 以上の ACL/EAD のとき...
				stat->acl = stat->ead = stat->rep = NULL;
				stat->aclSize = stat->eadSize = stat->repSize = 0;
				if (info.flags & REPORT_ACL_ERROR) {
					ConfirmErr(L"Too big acl/ead/rep(DIR)", src + srcPrefixLen, CEF_NOAPI);
				}
			}
		}
	}
	dirStatBuf.SetUsedSize(used_size_save);

	return	req;
}


inline int64 safe_add(int64 v1, int64 v2) {
	int64	val = v1 + v2;
	if (val >= 0 || v1 <= 0 || v2 <= 0) return val;
	return _I64_MAX;
}

inline bool in_grace(int64 stm, int64 dtm, int64 grace)
{
	int64	stm_min = safe_add(stm, -grace);
	int64	stm_max = safe_add(stm,  grace);

	return	 (dtm >= stm_min && dtm <= stm_max);
}

inline bool in_newer_grace(int64 stm, int64 dtm, int64 grace)
{
	int64	stm_min = safe_add(stm, -grace);

	return	 dtm >= stm_min;
}

#define DLSVT_DIFF	(60LL * 60 * 1000 * 1000 * 10)	// 1時間(100ns単位)

inline bool in_dlsvt_grace(int64 stm, int64 dtm, int64 grace)
{
	return	in_grace(stm + DLSVT_DIFF, dtm, grace) ||
			in_grace(stm - DLSVT_DIFF, dtm, grace);
}

inline bool in_newer_dlsvt_grace(int64 stm, int64 dtm, int64 grace)
{
	return	in_newer_grace(stm - DLSVT_DIFF, dtm, grace);
}


/*
	上書き判定
*/
BOOL FastCopy::IsOverWriteFile(FileStat *srcStat, FileStat *dstStat)
{
	if (info.debugFlags & OVERWRITE_JUDGE_LOGGING) {
		WCHAR	buf[512];
		swprintf(buf, L"DBG(compare): mode:%d flag=%llx fs=%x/%x grace=%llx/%llx"
			L"mtime=%llx/%llx ctime=%llx/%llx size=%llx/%llx fname=%.99s"
			, info.overWrite, info.flags, srcFsType, dstFsType
			, timeDiffGrace, info.timeDiffGrace
			, srcStat->WriteTime(), dstStat->WriteTime()
			, srcStat->CreateTime(), dstStat->CreateTime()
			, srcStat->FileSize(), dstStat->FileSize(), srcStat->cFileName);
		PutList(buf, PL_ERRMSG);
	}

	if (info.overWrite == BY_NAME)
		return	FALSE;

	bool	all_modernfs = IsModernFs(srcFsType) && IsModernFs(dstFsType);
	bool	is_dlsvt = (info.dlsvtMode == DLSVT_ALWAYS ||
						info.dlsvtMode == DLSVT_FAT && !all_modernfs);
	bool	use_crtime = (info.flags & COMPARE_CREATETIME) ? true : false;

	int64&	stm = use_crtime ? srcStat->CreateTime() : srcStat->WriteTime();
	int64&	dtm = use_crtime ? dstStat->CreateTime() : dstStat->WriteTime();

	// どちらかが NTFS でない場合（ネットワークドライブを含む）2秒の猶予
	bool	all_modern_local = IsLocalModernFs(srcFsType) && IsLocalModernFs(dstFsType);
#define FAT_GRACE 20000000LL
	int64	grace = (all_modern_local || timeDiffGrace > FAT_GRACE ||
						((stm % 10000000) && (dtm % 10000000))) ? timeDiffGrace : FAT_GRACE;

	if (info.overWrite == BY_ATTR) {
		if (dstStat->FileSize() == srcStat->FileSize()) {	// サイズが等しく、かつ...
			// 更新日付が同じ場合は更新しない
			if (in_grace(stm, dtm, grace)) {
				return	FALSE;
			}
			if (is_dlsvt && in_dlsvt_grace(stm, dtm, grace)) {
				return	FALSE;
			}
		}
		return	TRUE;
	}
	if (info.overWrite == BY_LASTEST) {
		// 更新日付が dst と同じか古い場合は更新しない
		if (in_newer_grace(stm, dtm, grace)) {
			return	FALSE;
		}
		if (is_dlsvt && in_newer_dlsvt_grace(stm, dtm, grace)) {
			return	FALSE;
		}
		return	TRUE;
	}
	if (info.overWrite == BY_ALWAYS) {
		return	TRUE;
	}

	return	ConfirmErr(L"Illegal overwrite mode", 0, CEF_STOP|CEF_NOAPI), FALSE;
}


/*=========================================================================
  関  数 ： OpenFileProc / OpenThread
  概  要 ： Open 処理
  説  明 ： 
  注  意 ： 
=========================================================================*/
BOOL FastCopy::OpenFileProc(FileStat *stat, int dir_len)
{
	int	name_len = (int)wcslen(stat->cFileName);
	memcpy(src + dir_len, stat->cFileName, ((name_len + 1) * sizeof(WCHAR)));

	openFiles[openFilesCnt++] = stat;

	WaitCheck();

	if (!isExec) {
		return	TRUE;
	}

	if (enableStream) {
		return	OpenFileProcCore(src, stat, dir_len, name_len);
	}

	return	OpenFileQueue(src, stat, dir_len, name_len);

}

unsigned WINAPI FastCopy::OpenThread(void *fastCopyObj)
{
	return	((FastCopy *)fastCopyObj)->OpenThreadCore();
}

BOOL FastCopy::OpenThreadCore(void)
{
	opCv.Lock();
	while (opRun && !isAbort) {
		auto op = opList.GetObj(USED_LIST);
		if (!op) {
			opCv.Wait(500);
			continue;
		}
		opCv.UnLock();
		OpenFileProcCore(op->path.WBuf(), op->stat, op->dir_len, op->name_len);
		opCv.Lock();
		opList.PutObj(FREE_LIST, op);
		opCv.Notify();
	}
	opCv.UnLock();
	return	TRUE;
}

BOOL FastCopy::OpenFileQueue(WCHAR *path, FileStat *stat, int dir_len, int name_len)
{
	opCv.Lock();

	OpenDat	*op = NULL;
	while (!(op = opList.GetObj(FREE_LIST)) && !isAbort && opRun) {
		opCv.Wait(500);
	}
	if (op) {
		op->stat		= stat;
		op->dir_len		= dir_len;
		op->name_len	= name_len;

		size_t	need_len = (dir_len + name_len + 256) * sizeof(WCHAR);
		if (op->path.Size() < need_len) {
			op->path.Grow(need_len - op->path.Size());
		}
		auto	s = op->path.WBuf();
		memcpy(s, path, dir_len * sizeof(WCHAR));
		memcpy(s + dir_len, stat->cFileName, ((name_len + 1) * sizeof(WCHAR)));

		opList.PutObj(USED_LIST, op);
	}
	opCv.Notify();
	opCv.UnLock();

	return	TRUE;
}

BOOL FastCopy::OpenFileProcCore(WCHAR *path, FileStat *stat, int dir_len, int name_len)
{
	BOOL	is_backup = enableAcl || enableStream;
	BOOL	is_reparse = IsReparseEx(stat->dwFileAttributes);
	BOOL	is_open = is_backup || is_reparse || stat->FileSize() > 0;
	BOOL	useOvl = is_open && !is_reparse && flagOvl && info.IsMinOvl(stat->FileSize());
	BOOL	ret = TRUE;

	if (is_open) {
		DWORD	mode = GENERIC_READ;
		DWORD	flg = ((info.flags & USE_OSCACHE_READ) ? 0 : FILE_FLAG_NO_BUFFERING)
					| FILE_FLAG_SEQUENTIAL_SCAN
					| (enableBackupPriv ? FILE_FLAG_BACKUP_SEMANTICS : 0);
		DWORD	share = FILE_SHARE_READ | ((info.flags & WRITESHARE_OPEN) ?
			(FILE_SHARE_WRITE|FILE_SHARE_DELETE) : 0);

		stat->hOvlFile = INVALID_HANDLE_VALUE;

		if (is_backup) {
			mode |= READ_CONTROL|acsSysSec;
		}
		else if (useOvl) {
			flg |= flagOvl;
		}
		if (is_reparse) {
			flg |= FILE_FLAG_OPEN_REPARSE_POINT;
		}
		//DebugW(L"CreateFile(OpenFileProc) %d %s\n", isExec, patrh);

		if ((stat->hFile = ::CreateFileW(path, mode, share, 0, OPEN_EXISTING, flg, 0))
				== INVALID_HANDLE_VALUE) {
			stat->lastError = ::GetLastError();

			if (stat->lastError == ERROR_SHARING_VIOLATION && (share & FILE_SHARE_WRITE) == 0) {
				share |= FILE_SHARE_WRITE;
				stat->hFile = ::CreateFileW(path, mode, share, 0, OPEN_EXISTING, flg, 0);
			}
		}
		if (stat->hFile == INVALID_HANDLE_VALUE) {
			ret = FALSE;
		} else if ((share & FILE_SHARE_WRITE)) {
			stat->isWriteShare = true;
		}

		if (ret && useOvl) {
			if (is_backup) {	// エラーの場合は hFile が使われる
				stat->hOvlFile = ::CreateFileW(path, mode, share, 0, OPEN_EXISTING, flg|flagOvl,0);
			} else {
				stat->hOvlFile = stat->hFile;
			}
		}
		if (ret) {
			if ((info.flags & USE_OSCACHE_READ) == 0 && IsNetFs(srcFsType)) {
				DisableLocalBuffering(
					(stat->hOvlFile == INVALID_HANDLE_VALUE) ? stat->hFile : stat->hOvlFile);
			}
		}
	}
	if (ret && is_backup) {
		ret = OpenFileBackupProc(path, stat, dir_len + name_len);
	}

	return	ret;

}

BOOL FastCopy::OpenFileBackupProc(WCHAR *path, FileStat *stat, int src_len)
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
				ConfirmErr(L"BackupRead(head)", path + srcPrefixLen);
			break;
		}
		if (size == 0) break;

		WCHAR	streamName[MAX_PATH];
		if (sid.dwStreamNameSize) {
			if (!(ret = ::BackupRead(stat->hFile, (BYTE *)streamName, sid.dwStreamNameSize, &size,
					FALSE, TRUE, &context))) {
				if (info.flags & REPORT_STREAM_ERROR)
					ConfirmErr(L"BackupRead(name)", path + srcPrefixLen);
				break;
			}
			// terminate されないため（dwStreamNameSize はバイト数）
			*(WCHAR *)((BYTE *)streamName + sid.dwStreamNameSize) = 0;
		}

		if (sid.dwStreamId == BACKUP_ALTERNATE_DATA && enableStream && !altdata_local) {
			ret = OpenFileBackupStreamCore(path, src_len, sid.Size.QuadPart, streamName,
				sid.dwStreamNameSize, &altdata_cnt);
		}
		else if (sid.dwStreamId == BACKUP_SPARSE_BLOCK) {
			if (altdata_cnt == 0 && enableStream) {
				altdata_local = ret = OpenFileBackupStreamLocal(path, stat, src_len, &altdata_cnt);
			}
			break; // BACKUP_SPARSE_BLOCK can't be used BackupSeek...
		}
		else if ((sid.dwStreamId == BACKUP_SECURITY_DATA || sid.dwStreamId == BACKUP_EA_DATA)
		&& enableAcl) {
			BYTE	*&data = sid.dwStreamId==BACKUP_SECURITY_DATA ? stat->acl     : stat->ead;
			int &data_size = sid.dwStreamId==BACKUP_SECURITY_DATA ? stat->aclSize : stat->eadSize;
			if (data || sid.Size.HighPart) {	// すでに格納済み
				if (info.flags & REPORT_ACL_ERROR)
					ConfirmErr(L"Duplicate or Too big ACL/EADATA", path + srcPrefixLen);
				break;
			}
			opCv.Lock();
			data = fileStatBuf.UsedEnd();
			data_size = sid.Size.LowPart + STRMID_OFFSET;
			if (fileStatBuf.RemainSize() <= maxStatSize + data_size
			&& !fileStatBuf.Grow(ALIGN_SIZE(maxStatSize + data_size, MIN_ATTR_BUF))) {
				opCv.UnLock();
				ConfirmErr(L"Can't alloc memory(fileStat(ACL/EADATA))",
					path + srcPrefixLen, CEF_STOP);
				break;
			}
			fileStatBuf.AddUsedSize(data_size);
			opCv.UnLock();

			memcpy(data, &sid, STRMID_OFFSET);
			if (!(ret = ::BackupRead(stat->hFile, data + STRMID_OFFSET, sid.Size.LowPart,
							&size, FALSE, TRUE, &context)) || size <= 0) {
				if (info.flags & REPORT_ACL_ERROR)
					ConfirmErr(L"BackupRead(ACL/EADATA)", path + srcPrefixLen);
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
					ConfirmErr(L"BackupSeek", path + srcPrefixLen);
				}
				break;
			}
		}
	}
	::BackupRead(stat->hFile, 0, 0, 0, TRUE, FALSE, &context);

	if (ret)	curTotal->readAclStream++;
	else		curTotal->rErrAclStream++;
	return	ret;
}


BOOL FastCopy::OpenFileBackupStreamLocal(WCHAR *path, FileStat *stat, int src_len,
	int *altdata_cnt)
{
	IO_STATUS_BLOCK	is;

	if (pNtQueryInformationFile(stat->hFile, &is, ntQueryBuf.Buf(),
								(int)ntQueryBuf.Size(), FileStreamInformation) < 0) {
	 	if (info.flags & (REPORT_STREAM_ERROR)) {
			ConfirmErr(L"NtQueryInformationFile", path + srcPrefixLen);
		}
		return	FALSE;
	}

	BOOL	ret = TRUE;
	FILE_STREAM_INFORMATION	*fsi, *next_fsi;

	for (fsi = (FILE_STREAM_INFORMATION *)ntQueryBuf.Buf(); fsi; fsi = next_fsi) {
		next_fsi = fsi->NextEntryOffset == 0 ? NULL :
			(FILE_STREAM_INFORMATION *)((BYTE *)fsi + fsi->NextEntryOffset);

		if (fsi->StreamName[1] == ':') continue; // skip main stream

		if (!OpenFileBackupStreamCore(path, src_len, fsi->StreamSize.QuadPart, fsi->StreamName,
				fsi->StreamNameLength, altdata_cnt)) {
			ret = FALSE;
			break;
		}
	}
	return	ret;
}

BOOL FastCopy::OpenFileBackupStreamCore(WCHAR *path, int src_len, int64 size, WCHAR *altname,
	int altnamesize, int *altdata_cnt)
{
	if (++(*altdata_cnt) >= MAX_ALTSTREAM) {
		if (info.flags & REPORT_STREAM_ERROR) {
			ConfirmErr(L"Too Many AltStream", path + srcPrefixLen, CEF_NOAPI);
		}
		return	FALSE;
	}

	opCv.Lock();
	FileStat	*subStat = (FileStat *)fileStatBuf.UsedEnd();
	bool		useOvl = info.IsMinOvl(size) && flagOvl && IsLocalFs(srcFsType);
	DWORD		share = FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;
	DWORD		flg = ((info.flags & USE_OSCACHE_READ) ? 0 : FILE_FLAG_NO_BUFFERING)
					| FILE_FLAG_SEQUENTIAL_SCAN
					| (enableBackupPriv ? FILE_FLAG_BACKUP_SEMANTICS : 0)
					| (useOvl ? flagOvl : 0);

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
		opCv.UnLock();
		ConfirmErr(L"Can't alloc memory(fileStatBuf2)", NULL, CEF_STOP);
		return	FALSE;
	}
	opCv.UnLock();

	memcpy(subStat->cFileName, altname, altnamesize + sizeof(WCHAR));
	memcpy(path + src_len, subStat->cFileName, altnamesize + sizeof(WCHAR));

	//DebugW(L"CreateFile(OpenFileBackupStreamCore) %d %s\n", isExec, path);

	if ((subStat->hFile = ::CreateFileW(path,
			GENERIC_READ|READ_CONTROL|acsSysSec,
			share, 0, OPEN_EXISTING, flg, 0)) == INVALID_HANDLE_VALUE) {
		if (info.flags & REPORT_STREAM_ERROR) ConfirmErr(L"OpenFile(st)", path + srcPrefixLen);
		subStat->lastError = ::GetLastError();
		return	FALSE;
	}
	else if (useOvl) {
		subStat->hOvlFile = subStat->hFile;
		if ((info.flags & USE_OSCACHE_READ) == 0 && IsNetFs(srcFsType)) {
			DisableLocalBuffering(subStat->hFile);
		}
	}

	return	TRUE;
}


BOOL FastCopy::ReadMultiFilesProc(int dir_len)
{
	opCv.Lock();
	while (opList.Num(FREE_LIST) < opList.TotalNum() && !isAbort && opRun) {
		opCv.Wait(500);
	}
	opCv.UnLock();

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
	if (isExec) {
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

//BOOL rand_err()
//{
//	static int cnt=0;
//	if ((++cnt % 3) == 0) {
//		SetLastError(ERROR_INVALID_PARAMETER);
//		return TRUE;
//	}
//	return FALSE;
//}

#define OVL_LOG(...) if (info.debugFlags & OVL_LOGGING) { OvlLog(__VA_ARGS__); }

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
			OVL_LOG(ovl, buf, L"read 1");

			if (err == ERROR_NO_SYSTEM_RESOURCES) {
				if (min(size, maxReadSize) <= REDUCE_SIZE) {
					return FALSE;
				}
				maxReadSize -= REDUCE_SIZE;
				maxReadSize = ALIGN_SIZE(maxReadSize, REDUCE_SIZE);
				wait_ovl = TRUE;
				OVL_LOG(ovl, buf, L"read 2 maxReadSize=%d", maxReadSize);
				continue;
			}
			if (err == ERROR_IO_PENDING) {
				ovl->waiting = true;
				if (wait_ovl) {
					if (!WaitOverlapped(hFile, ovl)) {
						OVL_LOG(ovl, buf, L"read wait err");
						return FALSE;
					}
					OVL_LOG(ovl, buf, L"read manual wait OK");
				}
			}
			else return FALSE;
		}
		break;	// 1回の転送で出来る範囲で終了
	}
	OVL_LOG(ovl, buf, L"read OK");

	if (maxReadSize != maxReadSizeSv) {
		WCHAR buf[128];
		swprintf(buf, FMT_REDUCEMSG, 'R', maxReadSizeSv / 1024/1024, maxReadSize / 1024/1024);
		WriteErrLog(buf);
	}

	return	TRUE;
}

BOOL FastCopy::ReadFileAltStreamProc(int *_open_idx, int dir_len, FileStat *stat)
{
	int		&open_idx = *_open_idx;
	BOOL	ret = TRUE;
	ReqHead	*req = NULL;

	while (ret && !isAbort && open_idx < openFilesCnt
		&& openFiles[open_idx]->dwFileAttributes == 0) {	// attr == 0 is AltStream
		ret = ReadFileProc(&open_idx, dir_len);
	}
	if (ret && stat->acl) {
		req = PrepareReqBuf(offsetof(ReqHead, stat) + stat->minSize, stat->aclSize, stat->fileID);
		if (req) {
			if (req->bufSize >= stat->aclSize) {
				memcpy(req->buf, stat->acl, stat->aclSize);
			}
			else {
				stat->acl = NULL;
				stat->aclSize = 0;
				if (info.flags & REPORT_ACL_ERROR) {
					ConfirmErr(L"Too big acl", RestorePath(src, open_idx, dir_len), CEF_NOAPI);
				}
			}
			ret = SendRequest(WRITE_BACKUP_ACL, req, stat);
		} else ret = FALSE;
	}
	if (ret && stat->ead) {
		req = PrepareReqBuf(offsetof(ReqHead, stat) + stat->minSize, stat->eadSize, stat->fileID);
		if (req) {
			if (req->bufSize >= stat->eadSize) {
				memcpy(req->buf, stat->ead, stat->eadSize);
			}
			else {
				stat->ead = NULL;
				stat->eadSize = 0;
				if (info.flags & REPORT_ACL_ERROR) {
					ConfirmErr(L"Too big ead", RestorePath(src, open_idx, dir_len), CEF_NOAPI);
				}
			}
			ret = SendRequest(WRITE_BACKUP_EADATA, req, stat);
		} else ret = FALSE;
	}
	return	ret;
}

BOOL FastCopy::ReadFileReparse(Command cmd, int idx, int dir_len, FileStat *stat)
{
	DynBuf	rd(MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
	ReqHead	*req = NULL;
	BOOL	ret = FALSE;

	if ((stat->repSize = ReadReparsePoint(stat->hFile, rd, (DWORD)rd.Size())) <= 0) {
		ConfirmErr(L"ReadReparse(File)", RestorePath(src, idx, dir_len) + srcPrefixLen);
		SetTotalErrInfo(cmd == WRITE_BACKUP_ALTSTREAM, stat->FileSize());
		return	FALSE;
	}
	req = PrepareReqBuf(offsetof(ReqHead, stat) + stat->minSize, stat->repSize, stat->fileID);
	if (req) {
		if (req->bufSize >= stat->repSize) {
			memcpy(req->buf, rd, stat->repSize);
			ret = SendRequest(cmd, req, stat);
		}
		else {
			ConfirmErr(L"ReadReparse(File) too big", RestorePath(src, idx, dir_len) + srcPrefixLen);
			CancelReqBuf(req);
			return	FALSE;
		}
	}
	return	ret;
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

	if (cmd != REQ_NONE) {
		SetTotalErrInfo(is_stream, stat->FileSize());
	}
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

	// 同期I/Oの場合も、ReadFile で OverLapped構造体でシーク位置指定するようになったため
	// BackupRead等の副作用に備えたシークセットは不要となった。
	// （これを呼び出すと非同期I/O + MediaProtectモードで、何故か書き込み警告が出る Win7-8.1）
	// ::SetFilePointer(hIoFile, 0, NULL, FILE_BEGIN);

	while (total_size < file_size && !isAbort) {
		int64	buf_remain = 0;
		OverLap	*ovl = rOvl.GetObj(FREE_LIST);
		ovl->req = NULL;
		rOvl.PutObj(USED_LIST, ovl);

		while (1) {
			if (!(ovl->req = PrepareReqBuf(offsetof(ReqHead, stat) + stat->minSize,
					file_size - total_size, stat->fileID, file_size, &buf_remain))) {
				cmd = REQ_NONE;
				ret = FALSE;
				break;
			}
			ovl->SetOvlOffset(total_size + order_total);
			ret = ReadFileWithReduce(hIoFile, ovl->req->buf, ovl->req->bufSize, ovl);

			WaitCheck();
			if (ret) break;

			// reparse point で別 volume に移動した場合用
			if (::GetLastError() == ERROR_INVALID_PARAMETER && sectorSize < BIG_SECTOR_SIZE) {
				srcSectorSize = max(srcSectorSize, BIG_SECTOR_SIZE);
				sectorSize = max(srcSectorSize, dstSectorSize);
				CancelReqBuf(ovl->req);
				ovl->req = NULL;
				OVL_LOG(ovl, NULL, L"sectorSize reduce sector=%d", sectorSize);
			}
			else break;
		}
		if (!ret || !ovl->req || isAbort) break;
		order_total += ovl->orderSize;

		BOOL need_dispatch = (isSameDrv && buf_remain < ovl->req->bufSize+MIN_BUF) ? TRUE : FALSE;
		BOOL is_empty_ovl = rOvl.IsEmpty(FREE_LIST);
		BOOL is_flush = (total_size + order_total >= file_size) || readInterrupt
						|| !ovl->waiting || need_dispatch;
		if (!is_empty_ovl && !is_flush && !waitLv) continue;

		while (OverLap	*ovl_tmp = rOvl.GetObj(USED_LIST)) {
			rOvl.PutObj(FREE_LIST, ovl_tmp);

			OVL_LOG(ovl_tmp, ovl_tmp->req->buf, L"wait io start");
			if (!(ret = WaitOvlIo(hIoFile, ovl_tmp, &total_size, &order_total))) break;
			OVL_LOG(ovl_tmp, ovl_tmp->req->buf, L"wait io done data=%s",
				AtoWs((char *)ovl_tmp->req->buf, 16));

			curTotal->readTrans += ovl_tmp->transSize; 
			ovl_tmp->req->readSize = ovl_tmp->transSize;

			if (total_size > file_size ||
				(total_size < file_size && ovl_tmp->orderSize != ovl_tmp->transSize) ||
				(total_size == file_size && stat->isWriteShare && IsFileChanged(stat))) {
				is_modifyerr = TRUE;
				ret = FALSE;
				break;
			}
			if (need_dispatch && rOvl.IsEmpty(USED_LIST)) {
				ovl_tmp->req->isFlush = true;
			}
			SendRequest(cmd, ovl_tmp->req, stat); // WRITE_FILE_CONT && !digest時stat不要…
			cmd = WRITE_FILE_CONT;	// 2回目以降のSendReqはCONT

			if (!isSameDrv && info.mode == MOVE_MODE && total_size < file_size) {
				FlushMoveList(FALSE);
			}
			if (total_size < file_size) WaitCheck();
			if ((!is_flush && !waitLv) || isAbort) break;
		}
		if (!ret || isAbort) break;
		if (need_dispatch) {
			ChangeToWriteMode();
		}
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
#ifdef _WIN64
		static FileStat *sv_stat;
		sv_stat = stat;
#endif
		::GetFileTime(stat->hFile, 0, 0, &stat->ftLastWriteTime);
#ifdef _WIN64
		if (stat != sv_stat) {
			WCHAR wbuf[128];
			swprintf(wbuf, L" rsi is changed(%p -> %p) idx=%d/%d stack=%p. recovered.",
				sv_stat, stat, cur_idx, openFilesCnt, &cur_idx);
			WriteErrLog(wbuf);
			stat = sv_stat;
//			static void    *stack[4];
//			static WCHAR	buf[1024];
//			memcpy(stack, (char *)&cur_idx - (0xb0 + 32), 32);
//			swprintf(buf, L"Detect rsi changed. stat(s/sv/o)=%p/%p/%p cur_p=%p "
//				L"(idx=%d/%d)\nv=%p/%p/%p/%p",
//				stat, sv_stat, openFiles[cur_idx], &cur_idx, cur_idx, openFilesCnt,
//				stack[0],stack[1],stack[2],stack[3]
//				);
//			ConfirmErr(buf, RestorePath(src, cur_idx, dir_len), CEF_STOP);
//			*(char *)0 = 0;	// 意図的に例外を発生させる
//			return FALSE;
		}
#endif
		stat->nFileSizeLow = ::GetFileSize(stat->hFile, &stat->nFileSizeHigh);
	}
	int64	file_size = stat->FileSize();
	Command	cmd = (enableAcl || enableStream) ? stat->dwFileAttributes ?
				WRITE_BACKUP_FILE : WRITE_BACKUP_ALTSTREAM : WRITE_FILE;
	BOOL	is_stream  = (cmd == WRITE_BACKUP_ALTSTREAM);
	BOOL	is_reparse = IsReparseEx(stat->dwFileAttributes);
	int64	req_count  = reqSendCount;

	if (UseHardlink() && !is_stream) {	// include listing only
		if (CheckHardLink(RestorePath(src, cur_idx, dir_len), -1, stat->hFile,
				stat->GetLinkData()) == LINK_ALREADY) {
			stat->SetFileSize(0);
			ret = SendRequest(CREATE_HARDLINK, 0, stat);
			goto END;
		}
	}
	if ((file_size == 0 && !is_reparse) || !isExec) {
		ret = SendRequest(cmd, 0, stat);
		if (!isExec && !is_reparse) {
			curTotal->readTrans += file_size;
			if (info.verifyFlags & VERIFY_FILE) {
				curTotal->verifyTrans += file_size;
				curTotal->verifyFiles++;
			}
		}
		if (cmd != WRITE_BACKUP_FILE || !isExec) {
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
		ret = ReadFileReparse(cmd, cur_idx, dir_len, stat);
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
		if (isExec && (data->dwAttr & FILE_ATTRIBUTE_READONLY)) { // data->path's stat
			::SetFileAttributesW(data->path, FILE_ATTRIBUTE_DIRECTORY);
			// ResetAcl()
		}
		if (isExec && ForceRemoveDirectoryW(data->path, info.aclReset|frdFlags) == FALSE) {
			curTotal->dErrDirs++;
			if (frdFlags && ::GetLastError() == ERROR_DIR_NOT_EMPTY) {
				frdFlags = 0;
			}
			ConfirmErr(L"RemoveDirectory(move)", data->path + prefix_len);
		} else {
			curTotal->deleteDirs++;
			if (isListing) {
				PutList(data->path + prefix_len, PL_DIRECTORY|PL_DELETE|
					(IsReparseEx(data->dwAttr) ? PL_REPARSE : 0), 0, data->wTime);
			}
		}
	} else {
		if (isExec && (data->dwAttr & FILE_ATTRIBUTE_READONLY)) { // data->path's stat
			::SetFileAttributesW(data->path, FILE_ATTRIBUTE_NORMAL);
			// ResetAcl()
		}
		//DebugW(L"flush delete(%d) %s\n", isExec, data->path + prefix_len);
		if (isExec && ForceDeleteFileW(data->path, info.aclReset) == FALSE) {
			curTotal->dErrFiles++;
			ConfirmErr(L"DeleteFile(move)", data->path + prefix_len);
		} else {
			curTotal->deleteFiles++;
			curTotal->deleteTrans += data->fileSize;
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
		if (require_sleep) CvWait(moveList.GetCv(), CV_WAIT_TICK);

		while ((head = moveList.Peek()) && !isAbort) {
			MoveObj	*data = (MoveObj *)head->data;

			if (data->status == MoveObj::START) break;
			if (data->status == MoveObj::DONE) {
				moveList.UnLock();
				FlushMoveListCore(data);
				done_cnt++;
				WaitCheck();
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
		if ((info.verifyFlags & VERIFY_FILE) && runMode != RUN_DIGESTREQ) {
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


FastCopy::ReqHead *FastCopy::AllocReqBuf(int req_size, int64 _data_size, int64 fsize,
	int64 *buf_remain)
{
	ReqHead	*req = NULL;
	DWORD	max_trans  = TransSize(maxReadSize);
	if (flagOvl && info.IsMinOvl(fsize) && !info.IsNormalOvl(fsize)) {
		auto ovl_size = info.GenOvlSize(fsize);
		max_trans = min(max_trans, ovl_size);
	}
	ssize_t	data_size  = (_data_size > max_trans) ? max_trans : (ssize_t)_data_size;

	ssize_t	used_size        = usedOffset - mainBuf.Buf();
	if (data_size) used_size = ALIGN_SIZE(used_size, sectorSize);

	ssize_t	sector_data_size = ALIGN_SIZE(data_size, sectorSize);
	ssize_t	max_free         = mainBuf.Size() - used_size;
	ssize_t	align_req_size   = ALIGN_SIZE(req_size, 8);
	ssize_t	require_size     = sector_data_size + align_req_size;
	ssize_t	align_offset     = used_size;

	if (data_size && require_size < sector_data_size) {
		require_size = sector_data_size;
	}
	if (require_size > max_free) {
		if (max_free < MINREMAIN_BUF + max(sectorSize, BIG_SECTOR_SIZE)) {
			align_offset = 0;
			if (isSameDrv) {
				Debug("ChangeToWriteModeCore in AllocReqBuf\n");
				if (!ChangeToWriteModeCore()){ // Read -> Write 切り替え
					 return NULL;
				}
			}
		}
		else {
			data_size = ((max_free - align_req_size) / BIGTRANS_ALIGN) * BIGTRANS_ALIGN;
			sector_data_size = data_size;
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
		CvWait(&cv, CV_WAIT_TICK);
	}
	req           = (ReqHead *)(mainBuf.Buf() + align_offset + sector_data_size);
	req->reqId    = 0;
	req->isFlush  = false;
	req->buf      = mainBuf.Buf() + align_offset;
	req->bufSize  = (DWORD)sector_data_size;
	req->readSize = 0;
	req->reqSize  = req_size;
	readInterrupt = isSameDrv
		&& align_offset + require_size + BIGTRANS_ALIGN + MINREMAIN_BUF > mainBuf.Size();

	usedOffset = req->buf + require_size;
	//Debug("off=%.1f size=%.1f\n", (double)align_offset / 1024, (double)sector_data_size / 1024);

	if (buf_remain) {
		if (usedOffset < freeOffset) {
			*buf_remain = freeOffset - usedOffset;
		} else {
			*buf_remain = mainBuf.Size() - (usedOffset - freeOffset);
		}
	}

	return	req;
}

FastCopy::ReqHead *FastCopy::PrepareReqBuf(int req_size, int64 data_size, int64 file_id,
	int64 file_size, int64 *buf_remain)
{
	int		err_cnt = 0;
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
		req = AllocReqBuf(req_size, data_size, file_size, buf_remain);
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
		req = AllocReqBuf(offsetof(ReqHead, stat) + (stat ? stat->minSize : 0), 0, 0);
	}
	if (req) {
		if (!isAbort) {
			ret = SendRequestCore(cmd, req, stat);
			//DebugW(L"SendRequest(%x) %d %s\n", cmd, isExec, stat ? stat->cFileName : L"");
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

