static char *mainwinlog_id = 
	"@(#)Copyright (C) 2015 H.Shirouzu		mainwinlog.cpp	ver3.03";
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Create					: 2015-05-28(Thu)
	Update					: 2015-08-30(Sun)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
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
		MakePathW(logDir, cfg.userDir, GetLoadStrW(IDS_FILELOG_SUBDIR));
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
			swprintf(wbuf, GetLoadStrW(IDS_FILELOGNAME),
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
}


/*=========================================================================
  Error Log for TMainDlg
=========================================================================*/
BOOL TMainDlg::EnableErrLogFile(BOOL on)
{
	if (on && hErrLog == INVALID_HANDLE_VALUE) {
		hErrLogMutex = ::CreateMutex(NULL, FALSE, FASTCOPYLOG_MUTEX);
		::WaitForSingleObject(hErrLogMutex, INFINITE);
		hErrLog = ::CreateFileW(errLogPath, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, 0,
					OPEN_ALWAYS, 0, 0);
	}
	else if (!on && hErrLog != INVALID_HANDLE_VALUE) {
		::CloseHandle(hErrLog);
		hErrLog = INVALID_HANDLE_VALUE;

		::CloseHandle(hErrLogMutex);
		hErrLogMutex = NULL;
	}
	return	TRUE;
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
		DWORD	len;
		char	*msg = "Initialize Error (Can't alloc memory or create/access DestDir)\r\n";

		::WriteFile(hErrLog, msg, (DWORD)strlen(msg), &len, 0);
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

	DWORD len = sprintf(buf, "%s\r\nFastCopy(%s%s) start at %d/%02d/%02d %02d:%02d:%02d\r\n\r\n",
		head_start, GetVersionStr(), GetVerAdminStr(),
		startTm.wYear, startTm.wMonth, startTm.wDay,
		startTm.wHour, startTm.wMinute, startTm.wSecond);

	::WriteFile(hFile, buf, len, &len, 0);
	if (pathLogBuf) {
		::WriteFile(hFile, pathLogBuf, (DWORD)strlen(pathLogBuf), &len, 0);
	}

	if (add_filelog && *fileLogPath) {
		len = sprintf(buf, "<FileLog> %s\r\n",
				cfg.isUtf8Log ? WtoU8s(fileLogPath) : WtoAs(fileLogPath));
		::WriteFile(hFile, buf, len, &len, 0);
	}

	if (finActIdx >= 1) {
		const WCHAR	*title = cfg.finActArray[finActIdx]->title;
		len = sprintf(buf, "<PostPrc> %s\r\n%s\r\n",
						cfg.isUtf8Log ? WtoU8s(title) : WtoAs(title), head_end);
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

	::SetFilePointer(hFile, 0, 0, FILE_END);

	if (errBufOffset == 0 && ti.total.errFiles == 0 && ti.total.errDirs == 0)
		len += sprintf(buf + len, "%s No Errors\r\n", hFile == hFileLog ? "\r\n" : "");

	len += sprintf(buf + len, "\r\n");
	len += GetDlgItemText(STATUS_EDIT, buf + len, sizeof(buf) - len);
	len += sprintf(buf + len, "\r\n\r\nResult : ");
	len += GetDlgItemText(ERRSTATUS_STATIC, buf + len, sizeof(buf) - len);
	len += sprintf(buf + len, "%s", "\r\n\r\n");
	::WriteFile(hFile, buf, len, &len, 0);

	return	TRUE;
}

