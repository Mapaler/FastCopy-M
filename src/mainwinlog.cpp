static char *mainwinlog_id = 
	"@(#)Copyright (C) 2015-2017 H.Shirouzu		mainwinlog.cpp	ver3.27";
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Create					: 2015-05-28(Thu)
	Update					: 2017-01-23(Mon)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	Modify					: Mapaler 2017-02-09
	======================================================================== */

#include "mainwin.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>

/*=========================================================================
  File Log for TMainDlg
=========================================================================*/
BOOL TMainDlg::StartFileLog()
{
	WCHAR	logDir[MAX_PATH] = L"";
	WCHAR	wbuf[MAX_PATH]   = L"";

	if (fileLogMode == NO_FILELOG || (!isListLog && IsListing())) return FALSE;

	if (fileLogMode == AUTO_FILELOG || wcschr(fileLogPath, '\\') == 0) {
		MakePathW(logDir, cfg.userDir, LoadStrW(IDS_FILELOG_SUBDIR));
		if (::GetFileAttributesW(logDir) == 0xffffffff)
			::CreateDirectoryW(logDir, NULL);

		if (fileLogMode == FIX_FILELOG && *fileLogPath) {
			wcscpy(wbuf, fileLogPath);
			MakePathW(fileLogPath, logDir, wbuf);
		}
	}

	if (fileLogMode == AUTO_FILELOG || *fileLogPath == 0) {
		SYSTEMTIME	st = startTm;
		for (int i=0; i < 100; i++) {
			swprintf(wbuf, LoadStrW(IDS_FILELOGNAME),
				st.wYear, st.wMonth, st.wDay, st.wHour,
				st.wMinute, st.wSecond, i);
			MakePathW(fileLogPath, logDir, wbuf);

			hFileLog = ::CreateFileW(fileLogPath, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
						0, CREATE_NEW, 0, 0);
			if (hFileLog != INVALID_HANDLE_VALUE) break;
		}
	}
	else {
		hFileLog = ::CreateFileW(fileLogPath, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
					0, OPEN_ALWAYS, 0, 0);
	}

	if (hFileLog == INVALID_HANDLE_VALUE) return FALSE;

	WriteLogHeader(hFileLog, FALSE);
	return	TRUE;
}

void TMainDlg::EndFileLog()
{
	if (hFileLog != INVALID_HANDLE_VALUE) {
		SetFileLogInfo();
		WriteLogFooter(hFileLog);
		::CloseHandle(hFileLog);
	}

	if (fileLogMode != FIX_FILELOG) {
		wcscpy(lastFileLog, fileLogPath);
		*fileLogPath = 0;
	}
	hFileLog = INVALID_HANDLE_VALUE;
}

void TMainDlg::SetFileLogInfo()
{
	DWORD	size = (DWORD)ti.listBuf->UsedSize();
//	DWORD	low, high;
//
//	if ((low = ::GetFileSize(hFileLog, &high)) == 0xffffffff && GetLastError() != NO_ERROR) return;
//	::LockFile(hFile, low, high, size, 0);
	::SetFilePointer(hFileLog, 0, 0, FILE_END);

	if (isUtf8Log) {
		U8str	str((WCHAR *)ti.listBuf->Buf());
		::WriteFile(hFileLog, str.s(), (int)strlen(str.s()), &size, NULL);
	}
	else {
		MBCSstr	str((WCHAR *)ti.listBuf->Buf());
		::WriteFile(hFileLog, str.s(), (int)strlen(str.s()), &size, NULL);
	}

//	::UnlockFile(hFile, low, high, size, 0);
	ti.listBuf->SetUsedSize(0);
	if (WCHAR *p = (WCHAR *)ti.listBuf->Buf()) {
		*p = 0;
	}
}


/*=========================================================================
  Error Log for TMainDlg
=========================================================================*/
BOOL TMainDlg::EnableErrLogFile(BOOL on)
{
	BOOL	ret = TRUE;

	if (on && hErrLog == INVALID_HANDLE_VALUE) {
		::WaitForSingleObject(hErrLogMutex, INFINITE);
		hErrLog = ::CreateFileW(errLogPath, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, 0,
					OPEN_ALWAYS, 0, 0);
		if (hErrLog == INVALID_HANDLE_VALUE) {
			::ReleaseMutex(hErrLogMutex);
			ret = FALSE;
		}
	}
	else if (!on && hErrLog != INVALID_HANDLE_VALUE) {
		::CloseHandle(hErrLog);
		hErrLog = INVALID_HANDLE_VALUE;
		::ReleaseMutex(hErrLogMutex);
	}

	return	ret;
}

BOOL TMainDlg::WriteErrLogCore()
{
	if (!errBufOffset) return FALSE;

	DWORD	len;

	if (cfg.isUtf8Log) {
		U8str	s(ti.errBuf->WBuf());
		::WriteFile(hErrLog, s.s(), (DWORD)strlen(s.s()), &len, 0);
	} else {
		MBCSstr	s(ti.errBuf->WBuf());
		::WriteFile(hErrLog, s.s(), (DWORD)strlen(s.s()), &len, 0);
	}

	return	TRUE;
}

void TMainDlg::WriteErrLogAtStart()
{
	EnableErrLogFile(TRUE);

	::GetLocalTime(&startTm);
	WriteLogHeader(hErrLog);

	if (!errBufOffset) {
		if (!ti.errBuf) {
			ti.errBuf = fastCopy.GetErrBuf();
			errBufOffset = (int)ti.errBuf->UsedSize();
		}
		if (errBufOffset) {
			WriteErrLogCore();
		}

		char  *msg = LoadStr(IDS_ErrLog_Initialize); //"Initialize Error\r\n\r\n"
		DWORD len = (DWORD)strlen(msg);
		::WriteFile(hErrLog, msg, len, &len, 0);
	}

	EnableErrLogFile(FALSE);
}

void TMainDlg::WriteErrLogNormal()
{
	EnableErrLogFile(TRUE);

	WriteLogHeader(hErrLog);
	WriteErrLogCore();
	WriteLogFooter(hErrLog);

	EnableErrLogFile(FALSE);
}

void TMainDlg::WriteErrLogNoUI(const char *msg)
{
	EnableErrLogFile(TRUE);

	WriteLogHeader(hErrLog);

	DWORD	len, size;
	len = (DWORD)strlen(msg);

	::WriteFile(hErrLog, msg, len, &size, 0);
	::WriteFile(hErrLog, "\r\n\r\n", 4, &size, 0);

	if (::AttachConsole(ATTACH_PARENT_PROCESS)) {
		HANDLE	hStdErr = GetStdHandle(STD_ERROR_HANDLE);
		Wstr	wmsg(msg);
		::WriteConsoleW(hStdErr, L"FastCopy: ", 10, &size, 0);
		::WriteConsoleW(hStdErr, wmsg.s(), (DWORD)wcslen(wmsg.s()), &size, 0);
		::WriteConsoleW(hStdErr, L"\r\n\r\n", 4, &size, 0);
	}

	EnableErrLogFile(FALSE);
}

void TMainDlg::WriteLogHeader(HANDLE hFile, BOOL add_filelog)
{
	static const char *head_start = "=================================================";
	static const char *head_end   = "-------------------------------------------------";

	char	buf[1024];

	::SetFilePointer(hFile, 0, 0, FILE_END);

	DWORD len = sprintf(buf, "%s\r\nFastCopy(%s%s) %s %d/%02d/%02d %02d:%02d:%02d\r\n\r\n"
		, head_start
		, GetVersionStr()
		, GetVerAdminStr()
		, cfg.isUtf8Log ? AtoU8s(LoadStr(IDS_Log_StartAt)) : LoadStr(IDS_Log_StartAt) //start at
		, startTm.wYear
		, startTm.wMonth
		, startTm.wDay
		, startTm.wHour
		, startTm.wMinute
		, startTm.wSecond
		);

	::WriteFile(hFile, buf, len, &len, 0);
	if (pathLogBuf) {
		::WriteFile(hFile, pathLogBuf, (DWORD)strlen(pathLogBuf), &len, 0);
	}

	if (add_filelog && *fileLogPath) {
		len = sprintf(buf, "<%s> %s\r\n"
			, cfg.isUtf8Log ? AtoU8s(LoadStr(IDS_Log_FileLog)) : LoadStr(IDS_Log_FileLog) //FileLog
			, cfg.isUtf8Log ? WtoU8s(fileLogPath) : WtoAs(fileLogPath)
			);
		::WriteFile(hFile, buf, len, &len, 0);
	}

	if (finActIdx >= 1) {
		const WCHAR	*title = cfg.finActArray[finActIdx]->title;
		len = sprintf(buf, "<%s> %s\r\n%s\r\n"
			, cfg.isUtf8Log ? AtoU8s(LoadStr(IDS_Log_PostPrc)) : LoadStr(IDS_Log_PostPrc) //PostPrc
			, cfg.isUtf8Log ? WtoU8s(title) : WtoAs(title)
			, head_end
			);
		::WriteFile(hFile, buf, len, &len, 0);
	}
	else if (pathLogBuf || (add_filelog && *fileLogPath)) {
		len = sprintf(buf, "%s\r\n", head_end);
		::WriteFile(hFile, buf, len, &len, 0);
	}
}

BOOL TMainDlg::WriteLogFooter(HANDLE hFile)
{
	DWORD	len = 0;
	char	buf[1024];
	DWORD	len1 = 0;
	char	buf1[1024];
	LPSTR	str1;
	DWORD	len2 = 0;
	char	buf2[1024];
	LPSTR	str2;

	::SetFilePointer(hFile, 0, 0, FILE_END);

	if (errBufOffset == 0 && ti.total.errFiles == 0 && ti.total.errDirs == 0)
		len += sprintf(buf + len, "%s%s\r\n"
			, hFile == hFileLog ? "\r\n" : ""
			, cfg.isUtf8Log ? AtoU8s(LoadStr(IDS_Log_NoErrors)) : LoadStr(IDS_Log_NoErrors) //No Errors
			);

	len += sprintf(buf + len, "\r\n");

	len1 += GetDlgItemText(STATUS_EDIT, buf1 + len1, sizeof(buf1) - len1);
	str1 = (LPSTR)buf1;
	str1 = isUtf8Log ? AtoU8s(str1) : str1;
	len += sprintf(buf + len, "%s", str1);
	//len += GetDlgItemText(STATUS_EDIT, buf + len, sizeof(buf) - len); //记录速度信息等

	len += sprintf(buf + len, "\r\n\r\n%s : "
		, cfg.isUtf8Log ? AtoU8s(LoadStr(IDS_Log_Result)) : LoadStr(IDS_Log_Result) //Result
		);

	len2 += GetDlgItemText(ERRSTATUS_STATIC, buf2 + len2, sizeof(buf2) - len2);
	str2 = (LPSTR)buf2;
	str2 = isUtf8Log ? AtoU8s(str2) : str2;
	len += sprintf(buf + len, "%s", str2);
	//len += GetDlgItemText(ERRSTATUS_STATIC, buf + len, sizeof(buf) - len); //错误个数

	len += sprintf(buf + len, "%s", "\r\n\r\n");
	::WriteFile(hFile, buf, len, &len, 0);

	return	TRUE;
}

