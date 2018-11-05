static char *fastcopyd_id = 
	"@(#)Copyright (C) 2004-2018 H.Shirouzu and FastCopy Lab, LLC.	fastcopyd.cpp	ver3.50";
/* ========================================================================
	Project  Name			: Fast Copy file and directory (del)
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
	WaitCalc	wc;

	::TlsSetValue(wcIdx, &wc);
	wc.fsType = srcFsType;

//	if ((info.flags & PRE_SEARCH) && info.mode == DELETE_MODE)
//		PreSearch();

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

	if ((hDir = ::FindFirstFileExW(path, findInfoLv, &fdat, FindExSearchNameMatch,
		NULL, findFlags)) == INVALID_HANDLE_VALUE) {
		curTotal->dErrDirs++;
		return	ConfirmErr(L"FindFirstFileEx(del)", path + dstPrefixLen), FALSE;
	}

	do {
		if (IsParentOrSelfDirs(fdat.cFileName))
			continue;

		ClearNonSurrogateReparse(&fdat);

		WaitCheck();

		FilterRes	cur_fr = FilterCheck(path, dir_len, &fdat, fr);
		if (cur_fr == FR_UNMATCH) {
			curTotal->filterDelSkips++;
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
	int		cur_skips = curTotal->filterDelSkips;
	BOOL	is_reparse = IsReparse(stat->dwFileAttributes);

	PushDepth(path, new_dir_len);

	if (isExec && (stat->dwFileAttributes & FILE_ATTRIBUTE_READONLY)) {
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

	if ((fr == FR_CONT) || cur_skips != curTotal->filterDelSkips
		|| (filterMode && info.mode == DELETE_MODE && 
			(info.flags & DELDIR_WITH_FILTER) == 0 && (filterMode & FilterBits(FILE_REG, INC_REG)
				&& (filterMode & FilterBits(DIR_REG, INC_REG)) == 0)))
		goto END;

	path[new_dir_len - 1] = 0;

	if (isExec) {
		WCHAR	*target = path;

		if (info.mode == DELETE_MODE && (info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA))) {
			if (RenameRandomFname(path, confirmDst, dir_len, new_dir_len - dir_len - 1)) {
				target = confirmDst;
			}
		}
		if ((ret = ForceRemoveDirectoryW(target, info.aclReset|frdFlags)) == FALSE) {
			curTotal->dErrDirs++;
			if (frdFlags && ::GetLastError() == ERROR_DIR_NOT_EMPTY) {
				frdFlags = 0;
			}
			ConfirmErr(L"RemoveDirectory", target + dstPrefixLen);
			goto END;
		}
	}
	if (isListing) {
		PutList(path + dstPrefixLen, PL_DIRECTORY|PL_DELETE|(is_reparse ? PL_REPARSE : 0), 0,
			stat->WriteTime());
	}
	curTotal->deleteDirs++;

END:
	PopDepth(path);
	return	ret;
}

BOOL FastCopy::DeleteFileProc(WCHAR *path, int dir_len, WCHAR *fname, FileStat *stat)
{
	int		len = wcscpyz(path + dir_len, fname);
	BOOL	is_reparse = IsReparse(stat->dwFileAttributes);

	if (isExec) {
		WCHAR	*target = path;

		if ((stat->dwFileAttributes & FILE_ATTRIBUTE_READONLY)) { // path's stat
			::SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);
			// ResetAcl()
		}
		if (info.mode == DELETE_MODE && (info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA))
		&& !is_reparse) {
			if (stat->FileSize()) {
				if (!WriteRandomData(path, stat, TRUE)) {
					curTotal->wErrFiles++;
					ConfirmErr(L"WriteRandomData(delete)", path + dstPrefixLen);
					return	FALSE;
				}
				curTotal->writeFiles++;
			}
			if (RenameRandomFname(path, confirmDst, dir_len, len)) {
				target = confirmDst;
			}
		}
		//DebugW(L"DeleteFileW(DeleteFileProc) %d %s\n", isExec, target);
		if (ForceDeleteFileW(target, info.aclReset) == FALSE) {
			curTotal->dErrFiles++;
			curTotal->dErrTrans += stat->FileSize();
			return	ConfirmErr(L"DeleteFile", target + dstPrefixLen), FALSE;
		}
	}
	else {
		if (info.mode == DELETE_MODE && (info.flags & (OVERWRITE_DELETE|OVERWRITE_DELETE_NSA))
		&& !is_reparse) {
			curTotal->writeFiles++;
			curTotal->writeTrans += stat->FileSize() *
				((info.flags & OVERWRITE_DELETE_NSA) ?  3 : 1);
		}
	}

	if (isListing) {
		PutList(path + dstPrefixLen, PL_DELETE|(is_reparse ? PL_REPARSE : 0), 0, stat->WriteTime(),
			stat->FileSize());
	}

	curTotal->deleteFiles++;
	curTotal->deleteTrans += stat->FileSize();
	return	TRUE;
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
	size_t	maxTransSize = info.maxOvlSize * info.maxOvlNum;

	if (data->is_nsa) {
		data->buf_size /= 3;
		data->buf_size = (data->buf_size / data->base_size) * data->base_size;
		data->buf_size = min((DWORD)maxTransSize, data->buf_size);

		data->buf[1] = data->buf[0] + data->buf_size;
		data->buf[2] = data->buf[1] + data->buf_size;
		if (info.flags & OVERWRITE_PARANOIA) {
			TGenRandom(data->buf[0], data->buf_size);	// CryptAPIのrandは遅い...
			TGenRandom(data->buf[1], data->buf_size);
		}
		else {
			TGenRandomMT(data->buf[0], data->buf_size);
			TGenRandomMT(data->buf[1], data->buf_size);
		}
		memset(data->buf[2], 0, data->buf_size);
	}
	else {
		data->buf_size = min((DWORD)maxTransSize, data->buf_size);
		if (info.flags & OVERWRITE_PARANOIA) {
			TGenRandom(data->buf[0], data->buf_size);	// CryptAPIのrandは遅い...
		}
		else {
			TGenRandomMT(data->buf[0], data->buf_size);
		}
	}
}

void FastCopy::GenRandomName(WCHAR *path, int fname_len, int ext_len)
{
	static char *char_dict = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz@$";
	static mt19937_64	mt64((random_device())());

	for (int i=0; i < fname_len; ) {
		u_int64	val = mt64();
		int		max_num = min((64/6), fname_len-i); // 64bit / 6bit == 10
		for (int j=0; j < max_num; i++, j++) {
			path[i] = char_dict[val & 0x3f];
			val >>= 6;
		}
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
	BOOL	is_nonbuf = IsLocalFs(dstFsType) && (stat->FileSize() >= max(nbMinSize, PAGE_SIZE)
			|| (stat->FileSize() % dstSectorSize) == 0) && (info.flags & USE_OSCACHE_WRITE) == 0 ?
				TRUE : FALSE;
	DWORD	flg = (is_nonbuf ? FILE_FLAG_NO_BUFFERING : 0) | FILE_FLAG_SEQUENTIAL_SCAN
					| (enableBackupPriv ? FILE_FLAG_BACKUP_SEMANTICS : 0);
	DWORD	share = FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;

	//DebugW(L"CreateFile(WriteRandomData) %d %s\n", isExec, path);

	HANDLE	hFile = ForceCreateFileW(path, GENERIC_WRITE, share, 0, OPEN_EXISTING, flg, 0,
		info.aclReset);	// ReadOnly attr のクリアは親で完了済み
	BOOL	ret = TRUE;

	if (hFile == INVALID_HANDLE_VALUE) {
		return	FALSE;
	}
	WaitCheck();

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
			int64	max_trans = TransSize(maxWriteSize);
			max_trans = min(max_trans, data->buf_size);
			int64	io_size = file_size - total_trans;
			io_size = (io_size > max_trans) ? max_trans : ALIGN_SIZE(io_size, dstSectorSize);

			ovl->SetOvlOffset(total_trans);
			if (!(ret = WriteFileWithReduce(hFile, data->buf[i], (DWORD)io_size, ovl))) {
				break;
			}

			curTotal->writeTrans += ovl->transSize;
			WaitCheck();
		}
		curTotal->writeTrans -= file_size - stat->FileSize();
		::FlushFileBuffers(hFile);
	}

	::SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
	::SetEndOfFile(hFile);

END:
	::CloseHandle(hFile);
	return	ret;
}
