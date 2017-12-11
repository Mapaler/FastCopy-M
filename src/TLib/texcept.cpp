static char *texcept_id = 
	"@(#)Copyright (C) 1996-2017 H.Shirouzu		texcept.cpp	Ver0.99";
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Application Frame Class
	Create					: 1996-06-01(Sat)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#define EX_TRACE
#include "tlib.h"

#include <atomic>

#pragma warning (push)
#pragma warning (disable : 4091) // dbghelp.h(1540): warning C4091: 'typedef ' ignored...
#include <dbghelp.h>
#pragma warning (pop)

#include <psapi.h>

using namespace std;

#pragma comment (lib, "Dbghelp.lib")

/*=========================================================================
	Debug
=========================================================================*/
static HANDLE hStdOut = NULL;
static HANDLE hLogFile = NULL;

void OpenDebugConsole(DWORD odc_mode)
{
	if (!hStdOut) {
		if (odc_mode == ODC_ALLOC) {
			::AllocConsole();
		}
		else if (odc_mode == ODC_PARENT) {
			::AttachConsole(ATTACH_PARENT_PROCESS);
		}
		else if (odc_mode == ODC_NONE) {
		}
		hStdOut = ::GetStdHandle(STD_OUTPUT_HANDLE);
		if (hStdOut == INVALID_HANDLE_VALUE) {
			hStdOut = NULL;
		}
	}
}

void CloseDebugConsole()
{
	if (hStdOut) {
		::FreeConsole();
		hStdOut = NULL;
	}
}

void OpenDebugFile(const char *logfile, BOOL is_cont)
{
	CloseDebugFile();
	hLogFile = CreateFileU8(logfile, GENERIC_WRITE, FILE_SHARE_READ, 0,
				is_cont ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (hLogFile == INVALID_HANDLE_VALUE) {
		hLogFile = NULL;
	}
}

void CloseDebugFile()
{
	if (hLogFile) {
		::CloseHandle(hLogFile);
		hLogFile = NULL;
	}
}

BOOL DebugConsoleEnabled()
{
	return	hStdOut ? TRUE : FALSE;
}

static DWORD dbg_last = ::GetTick();

void Debug(const char *fmt,...)
{
	char buf[8192];

	DWORD	cur = ::GetTick();
	DWORD	diff = cur - dbg_last;
	DWORD	len = (DWORD)sprintf(buf, "%04d.%02d: ", (diff / 1000), (diff % 1000) / 10);

	va_list	ap;
	va_start(ap, fmt);
	len += (DWORD)vsnprintfz(buf + len, sizeof(buf) - len -1, fmt, ap);
	va_end(ap);
	::OutputDebugString(buf);

	if (hStdOut) {
		DWORD	wlen = len;
		::WriteConsole(hStdOut, buf, wlen, &wlen, 0);
	}
	if (hLogFile) {
		DWORD	wlen = len;
		::WriteFile(hLogFile, buf, wlen, &wlen, 0);
	}
}

void DebugW(const WCHAR *fmt,...)
{
	WCHAR buf[4096];

	DWORD	cur = ::GetTick();
	DWORD	diff = cur - dbg_last;
	DWORD	len = (DWORD)swprintf(buf, L"%04d.%02d: ", (diff / 1000), (diff % 1000) / 10);

	va_list	ap;
	va_start(ap, fmt);
	len += (DWORD)_vsnwprintf(buf + len, wsizeof(buf) - len, fmt, ap);
	va_end(ap);
	::OutputDebugStringW(buf);

	if (hStdOut) {
		DWORD	wlen = len;
		::WriteConsoleW(hStdOut, buf, wlen, &wlen, 0);
	}
	if (hLogFile) {
		U8str	u(buf);
		DWORD	wlen = u.Len();
		::WriteFile(hLogFile, u.Buf(), wlen, &wlen, 0);
	}
}


void DebugU8(const char *fmt,...)
{
	char buf[8192];

	DWORD	cur = ::GetTick();
	DWORD	diff = cur - dbg_last;
	size_t	len = sprintf(buf, "%04d.%02d: ", (diff / 1000), (diff % 1000) / 10);

	va_list	ap;
	va_start(ap, fmt);
	len += vsnprintfz(buf + len, int(sizeof(buf) - len), fmt, ap);
	va_end(ap);

	Wstr	w(buf);
	::OutputDebugStringW(w.s());

	if (hStdOut) {
		DWORD wlen = (DWORD)w.Len();
		::WriteConsoleW(hStdOut, w.s(), wlen, &wlen, 0);
	}
	if (hLogFile) {
		DWORD	wlen = (DWORD)len;
		::WriteFile(hLogFile, buf, wlen, &wlen, 0);
	}
}

#define FMT_BUF_NUM		8		// 2^n
#define FMT_BUF_SIZE	8192

const char *Fmt(const char *fmt,...)
{
	static std::atomic<DWORD>	idx;
	static char		buf[FMT_BUF_NUM][FMT_BUF_SIZE];	// TLS使うべき…

	char	*p = buf[idx++ % FMT_BUF_NUM];

	va_list	ap;
	va_start(ap, fmt);
	vsnprintfz(p, FMT_BUF_SIZE, fmt, ap);
	va_end(ap);

	return	p;
}

const WCHAR *FmtW(const WCHAR *fmt,...)
{
	static std::atomic<DWORD>	idx;
	static WCHAR buf[FMT_BUF_NUM][FMT_BUF_SIZE];

	WCHAR	*p = buf[idx++ % FMT_BUF_NUM];

	va_list	ap;
	va_start(ap, fmt);
	p[FMT_BUF_SIZE - 1] = 0;
	_vsnwprintf(p, FMT_BUF_SIZE-1, fmt, ap);
	va_end(ap);

	return	p;
}


/*=========================================================================
	例外情報取得
=========================================================================*/
static char *ExceptionTitle;
static char *ExceptionLogFile;
static char *ExceptionDumpFile;
static DWORD ExceptionDumpFlags;
static char *ExceptionShellArg;
static char *ExceptionLogInfo;
static char *ExceptionVerInfo;
static char *ExceptionTrace;
static char *ExceptionTracePtr;
static char *ExceptionTraceEnd;
static void *ExceptionModAddr;
static SYSTEMTIME ExceptionTm;

#define STACKDUMP_SIZE			256
#ifdef _WIN64
#define MAX_STACKDUMP_SIZE		2048
#define MAX_DUMPBUF_SIZE		4096
#else
#define MAX_STACKDUMP_SIZE		1024
#define MAX_DUMPBUF_SIZE		2048
#endif
#define MAX_PRE_STACKDUMP_SIZE	256

inline int reg_info_core(char *buf, const u_char *s, int size, const char *name)
{
	const u_char	*e = s + size;
	int				len = strcpyz(buf, name);

	for ( ; s < e; s+=4) {
		if (!::IsBadReadPtr(s, 4)) {
			len += sprintf(buf+len, " %02x%02x%02x%02x", s[0], s[1], s[2], s[3]);
		}
	}
	if (len < 10) len += strcpyz(buf+len, " ........"); // nameしか出力がない場合

	len += strcpyz(buf+len, "\r\n");
	return	len;
}

inline int reg_info(char *buf, DWORD_PTR target, const char *name)
{
	int len = 0;

	len += strcpyz(buf+len, "\r\n");
	len += reg_info_core(buf+len, (const u_char *)target - 32, 32, "   ");
	len += reg_info_core(buf+len, (const u_char *)target -  0, 32, name);
	len += reg_info_core(buf+len, (const u_char *)target + 32, 32, "   ");

	return	len < 50 ? 0 : len;	// target データがない場合は 0 に
}

void InitExTrace(int trace_len)
{
	if (ExceptionTrace) {
		free(ExceptionTrace);
		ExceptionTrace = ExceptionTracePtr = ExceptionTraceEnd = NULL;
	}
	if (trace_len <= 0) {
		return;
	}
	ExceptionTrace = (char *)calloc(1, trace_len);
	ExceptionTracePtr = ExceptionTrace;
	ExceptionTraceEnd = ExceptionTrace + trace_len;
}

BOOL ExTrace(const char *fmt,...)
{
	if (!ExceptionTrace) {
		return	FALSE;
	}

	char buf[8192];

	va_list	ap;
	va_start(ap, fmt);
	int len = vsnprintfz(buf, sizeof(buf) - 3, fmt, ap);
	va_end(ap);

	if (len <= 0) {
		return FALSE;
	}
	if (buf[0] != '[' && buf[len-1] != '\n') {
		len += strcpyz(buf + len, "\r\n");
	}

	if (ExceptionTracePtr + len <= ExceptionTraceEnd) {
		memcpy(ExceptionTracePtr, buf, len);
		ExceptionTracePtr += len;
		if (ExceptionTracePtr == ExceptionTraceEnd) {
			ExceptionTracePtr = ExceptionTrace;
		}
	}
	else {
		size_t	remain = ExceptionTraceEnd - ExceptionTracePtr;
		memcpy(ExceptionTracePtr, buf, remain);
		ExceptionTracePtr = ExceptionTrace;
		memcpy(ExceptionTracePtr, buf + remain, len - remain);
		ExceptionTracePtr += len - remain;
	}

	return	TRUE;
}

LONG WINAPI Local_UnhandledExceptionFilter(struct _EXCEPTION_POINTERS *info)
{
	static char	buf[MAX_DUMPBUF_SIZE];
	HANDLE		hFile;
	HANDLE		hDumpFile;
	SYSTEMTIME	&stm = ExceptionTm;
	SYSTEMTIME	tm;
	DWORD		len;
	DWORD		i, j;
	char		*esp;

	hFile = ::CreateFile(ExceptionLogFile, GENERIC_WRITE, 0, 0, OPEN_ALWAYS, 0, 0);
	::SetFilePointer(hFile, 0, 0, FILE_END);
	::GetLocalTime(&tm);

	len = sprintf(buf,
		"------ %.100s -----\r\n"
		" Date        : %d/%02d/%02d %02d:%02d:%02d\r\n"
		" Start       : %d/%02d/%02d %02d:%02d:%02d\r\n"
		" OS Infos    : %.100s\r\n"
		" Mod Addr    : %p\r\n"
			, ExceptionTitle
			, tm.wYear,  tm.wMonth,  tm.wDay,  tm.wHour,  tm.wMinute,  tm.wSecond
			, stm.wYear, stm.wMonth, stm.wDay, stm.wHour, stm.wMinute, stm.wSecond
			, ExceptionVerInfo, ExceptionModAddr);
	::WriteFile(hFile, buf, len, &len, 0);

	if (info) {
		CONTEXT	*ctx = info->ContextRecord;

		len = sprintf(buf,
#ifdef _WIN64
			" Code/Adr/DC : %X / %p / %p\r\n"
			" AX/BX/CX/DX : %p / %p / %p / %p\r\n"
			" SI/DI/BP/SP : %p / %p / %p / %p\r\n"
			" 08/09/10/11 : %p / %p / %p / %p\r\n"
			" 12/13/14/15 : %p / %p / %p / %p\r\n"
			" BT/BF/ET/EF : %p / %p / %p / %p\r\n"
			"------- pre stack info -----\r\n"
			, info->ExceptionRecord->ExceptionCode, (void *)info->ExceptionRecord->ExceptionAddress
			, (void *)ctx->DebugControl
			, (void *)ctx->Rax, (void *)ctx->Rbx, (void *)ctx->Rcx, (void *)ctx->Rdx
			, (void *)ctx->Rsi, (void *)ctx->Rdi, (void *)ctx->Rbp, (void *)ctx->Rsp
			, (void *)ctx->R8,  (void *)ctx->R9,  (void *)ctx->R10, (void *)ctx->R11
			, (void *)ctx->R12, (void *)ctx->R13, (void *)ctx->R14, (void *)ctx->R15
			, (void *)ctx->LastBranchToRip, (void *)ctx->LastBranchFromRip
			, (void *)ctx->LastExceptionToRip, (void *)ctx->LastExceptionFromRip
#else
			" Code/Addr   : %X / %p\r\n"
			" AX/BX/CX/DX : %08x / %08x / %08x / %08x\r\n"
			" SI/DI/BP/SP : %08x / %08x / %08x / %08x\r\n"
			"----- pre stack info ---\r\n"
			, info->ExceptionRecord->ExceptionCode, info->ExceptionRecord->ExceptionAddress
			, ctx->Eax, ctx->Ebx, ctx->Ecx, ctx->Edx
			, ctx->Esi, ctx->Edi, ctx->Ebp, ctx->Esp
#endif
			);
		::WriteFile(hFile, buf, len, &len, 0);

#ifdef _WIN64
			esp = (char *)ctx->Rsp;
#else
			esp = (char *)ctx->Esp;
#endif

		for (i=0; i < MAX_PRE_STACKDUMP_SIZE / STACKDUMP_SIZE; i++) {
			char *stack = (esp - MAX_PRE_STACKDUMP_SIZE) + (i * STACKDUMP_SIZE);
			if (::IsBadReadPtr(stack, STACKDUMP_SIZE)) continue;
			len = 0;
			for (j=0; j < STACKDUMP_SIZE / sizeof(void *); j++)
				len += sprintf(buf + len, "%p%s", ((void **)stack)[j],
								((j+1)%(32/sizeof(void *))) ? " " : "\r\n");
			::WriteFile(hFile, buf, len, &len, 0);
		}

		len = sprintf(buf, "------- stack info -----\r\n");
		::WriteFile(hFile, buf, len, &len, 0);

		for (i=0; i < MAX_STACKDUMP_SIZE / STACKDUMP_SIZE; i++) {
			char *stack = esp + (i * STACKDUMP_SIZE);
			if (::IsBadReadPtr(stack, STACKDUMP_SIZE))
				break;
			len = 0;
			for (j=0; j < STACKDUMP_SIZE / sizeof(void *); j++)
				len += sprintf(buf + len, "%p%s", ((void **)stack)[j],
								((j+1)%(32/sizeof(void *))) ? " " : "\r\n");
			::WriteFile(hFile, buf, len, &len, 0);
		}

		len = sprintf(buf, "---- reg point info ----");
#ifdef _WIN64
		len += reg_info(buf+len, ctx->Rax, "Rax"); len += reg_info(buf+len, ctx->Rbx, "Rbx");
		len += reg_info(buf+len, ctx->Rcx, "Rcx"); len += reg_info(buf+len, ctx->Rdx, "Rdx");
		len += reg_info(buf+len, ctx->Rsi, "Rsi"); len += reg_info(buf+len, ctx->Rdi, "Rdi");
		len += reg_info(buf+len, ctx->Rbp, "Rbp"); len += reg_info(buf+len, ctx->Rsp, "Rsp");
		len += reg_info(buf+len, ctx->R8 , "R8 "); len += reg_info(buf+len, ctx->R9 , "R9 ");
		len += reg_info(buf+len, ctx->R10, "R10"); len += reg_info(buf+len, ctx->R11, "R11");
		len += reg_info(buf+len, ctx->R12, "R12"); len += reg_info(buf+len, ctx->R13, "R13");
		len += reg_info(buf+len, ctx->R14, "R14"); len += reg_info(buf+len, ctx->R15, "R15");
		len += reg_info(buf+len, ctx->Rip, "Rip");
#else
		len += reg_info(buf+len, ctx->Eax, "Eax"); len += reg_info(buf+len, ctx->Ebx, "Ebx");
		len += reg_info(buf+len, ctx->Ecx, "Ecx"); len += reg_info(buf+len, ctx->Edx, "Edx");
		len += reg_info(buf+len, ctx->Esi, "Esi"); len += reg_info(buf+len, ctx->Edi, "Edi");
		len += reg_info(buf+len, ctx->Ebp, "Ebp"); len += reg_info(buf+len, ctx->Esp, "Esp");
		len += reg_info(buf+len, ctx->Eip, "Eip");
#endif
		::WriteFile(hFile, buf, len, &len, 0);
	}

	if (ExceptionTrace) {
		len = sprintf(buf, "---- trace log info ----\r\n");
		::WriteFile(hFile, buf, len, &len, 0);
		if (ExceptionTracePtr != ExceptionTrace && ExceptionTracePtr[0] == 0 &&
			ExceptionTracePtr[1]) {
			if ((len = (DWORD)(ExceptionTraceEnd - ExceptionTracePtr - 1)) > 0) {
				::WriteFile(hFile, ExceptionTracePtr + 1, len, &len, 0);
			}
		}
		if (ExceptionTrace[0]) {
			if ((len = (DWORD)(ExceptionTracePtr - ExceptionTrace)) > 0) {
				::WriteFile(hFile, ExceptionTrace, len, &len, 0);
			}
		}
		len = sprintf(buf, "\r\n");
		::WriteFile(hFile, buf, len, &len, 0);
	}

	if (ExceptionDumpFile) {
		static MINIDUMP_EXCEPTION_INFORMATION	mei;
		mei.ThreadId = ::GetCurrentThreadId();
		mei.ExceptionPointers = info;
		mei.ClientPointers = FALSE;

		len = sprintf(buf, "---- dump info ----\r\n");
		::WriteFile(hFile, buf, len, &len, 0);

		hDumpFile = ::CreateFile(ExceptionDumpFile, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
		while (1) {
			if (::MiniDumpWriteDump(::GetCurrentProcess(), ::GetCurrentProcessId(), hDumpFile,
				(MINIDUMP_TYPE)ExceptionDumpFlags, &mei, NULL, NULL)) {
				len = ::GetFileSize(hDumpFile, NULL);
				len = sprintf(buf, " size=%d flags=%x\r\n", len, ExceptionDumpFlags);
				::WriteFile(hFile, buf, len, &len, 0);
				break;
			}
			if (ExceptionDumpFlags == 0) {
				break;
			}
			ExceptionDumpFlags &= ~(1 << get_nlz(ExceptionDumpFlags));
		}
		::CloseHandle(hDumpFile);
	}

	// trace back
#define MAX_BACKTRACE	62
#define MAX_MODULE		256
	static void		*backTrace[MAX_BACKTRACE];
	static DWORD	btNum;

	len = sprintf(buf, "\r\n---- back trace info ----\r\n");
	if ((btNum = ::CaptureStackBackTrace(0, MAX_BACKTRACE, backTrace, NULL)) > 0) {
		static HMODULE	hMod[MAX_MODULE];
		static DWORD	modNum;
		static DWORD	baseIdx;
		static DWORD	idx;
		HANDLE	hCur = ::GetCurrentProcess();

		::EnumProcessModules(hCur, hMod, MAX_MODULE, &modNum);

		for (baseIdx=0; baseIdx < btNum; baseIdx++) {	// 例外ハンドラ関連は skip
			if (backTrace[baseIdx] == (void *)info->ExceptionRecord->ExceptionAddress) break;
		}
		if (baseIdx == btNum) baseIdx = 0;

		for (i=0; baseIdx + i < btNum && len < sizeof(buf)-1; i++) {
			static char			modName[MAX_PATH];
			static MODULEINFO	mi;
			void	*&bt = backTrace[baseIdx + i];

			for (j=0; j < modNum; j++) {
				if (::GetModuleInformation(hCur, hMod[j], &mi, sizeof(MODULEINFO))) {
					if (bt >= mi.lpBaseOfDll &&
						bt < (void *)((char *)mi.lpBaseOfDll + mi.SizeOfImage)) {
						break;
					}
				}
			}
			if (j < modNum && ::GetModuleBaseName(hCur, hMod[j], modName, sizeof(modName))) {
				len += snprintfz(buf+len, sizeof(buf)-len, "%2d: %p : %8zx : %s\r\n",
					i, bt, (char *)bt - (char *)mi.lpBaseOfDll, modName);
			}
			else {
				len += snprintfz(buf+len, sizeof(buf)-len, "%2d: %p\r\n", i, bt);
			}
		}
	}
	::WriteFile(hFile, buf, len, &len, 0);

	// end mark
	len = sprintf(buf, "------------------------\r\n\r\n");
	::WriteFile(hFile, buf, len, &len, 0);
	::CloseHandle(hFile);

	if (!info) {
		return EXCEPTION_EXECUTE_HANDLER;
	}

	static BOOL	once = FALSE;
	if (once) {
		return EXCEPTION_EXECUTE_HANDLER;
	}
	once = TRUE;

	snprintfz(buf, sizeof(buf), ExceptionLogInfo, ExceptionLogFile);
	if (::MessageBox(0, buf, ExceptionTitle, MB_OKCANCEL) == IDOK) {
		::ShellExecute(0, 0, "explorer", ExceptionShellArg, 0, SW_SHOW);
	}

	return	EXCEPTION_EXECUTE_HANDLER;
}

BOOL InstallExceptionFilter(const char *title, const char *info, const char *fname,
	const char *dump, DWORD dump_flags)
{
	::GetLocalTime(&ExceptionTm);

	char	buf[MAX_PATH_U8];

	if (fname && *fname) {
		strcpy(buf, fname);
	} else {
		::GetModuleFileName(NULL, buf, sizeof(buf));
		strcpy(strrchr(buf, '.'), "_exception.log");
	}

	ExceptionLogFile = strdup(buf);

	if (dump && *dump) {
		ExceptionDumpFile = strdup(dump);
		ExceptionDumpFlags = dump_flags;
	}

	snprintfz(buf, sizeof(buf), "/select,%s", ExceptionLogFile);
	ExceptionShellArg = strdup(buf);

	ExceptionTitle = strdup(title);
	ExceptionLogInfo = strdup(info);

	OSVERSIONINFOEX	ovi = { sizeof(OSVERSIONINFOEX) };
	::GetVersionEx((OSVERSIONINFO *)&ovi);
	snprintfz(buf, sizeof(buf), "%02x/%02x/%02x/%02x/%02x/%02x",
		ovi.dwMajorVersion, ovi.dwMinorVersion, ovi.dwBuildNumber,
		ovi.wServicePackMajor, ovi.wServicePackMinor, ovi.wSuiteMask);
	ExceptionVerInfo = strdup(buf);

	ExceptionModAddr = (void *)::GetModuleHandle(NULL);

	::SetUnhandledExceptionFilter(&Local_UnhandledExceptionFilter);
	return	TRUE;
}

void ForceFlushExceptionLog()
{
	Local_UnhandledExceptionFilter(NULL);
}


