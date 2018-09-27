/* @(#)Copyright (C) 1996-2017 H.Shirouzu		texcept.h	Ver0.99 */
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Main Header
	Create					: 1996-06-01(Sat)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef TEXCEPT_H
#define TEXCEPT_H

#define EXTRACE2 ExTrace("[%s (%d) %7.2f] ", __FUNCTION__, __LINE__, \
	((double)(GetTick() % 10000000))/1000), ExTrace
#define EXTRACE ExTrace("[%7.2f] ", ((double)(GetTick() % 10000000))/1000), ExTrace

#if defined(_DEBUG)
#ifndef EX_TRACE
#define EX_TRACE
#endif
#endif

#ifdef EX_TRACE
#define DBT  EXTRACE
#define DBT2 EXTRACE2
void InitExTrace(int trace_len);
BOOL ExTrace(const char *fmt,...);
#else
#define DBT(...)
#define DBT2(...)
#define InitExTrace(...)
#define ExTrace(...)
#endif

void ForceFlushExceptionLog();
BOOL InstallExceptionFilter(const char *title, const char *info, const char *fname=NULL,
	const char *dump=NULL, DWORD dump_flags=0);
BOOL RegisterDumpExceptArea(void *ptr, size_t size);
BOOL ModifyDumpExceptArea(void *ptr, size_t size);
BOOL RemoveDumpExceptArea(void *ptr);
void InitDumpExceptArea();

enum { ODC_ALLOC=0, ODC_PARENT=1, ODC_NONE=2 };
void OpenDebugConsole(DWORD odc_mode=ODC_ALLOC);
void CloseDebugConsole();
BOOL DebugConsoleEnabled();

void OpenDebugFile(const char *logfile, BOOL is_cont=FALSE);
void CloseDebugFile();


void Debug(const char *fmt,...);
void DebugW(const WCHAR *fmt,...);
void DebugU8(const char *fmt,...);
const char *Fmt(const char *fmt,...);
const WCHAR *FmtW(const WCHAR *fmt,...);
void OutW(const WCHAR *fmt,...);

inline void NullFunc() {}

#define TrcW(...) (DebugConsoleEnabled() ? DebugW(__VA_ARGS__) : NullFunc())
#define Trc(...) (DebugConsoleEnabled() ? Debug(__VA_ARGS__) : NullFunc())
#define TrcU8(...) (DebugConsoleEnabled() ? DebugU8(__VA_ARGS__) : NullFunc())

#if defined(_DEBUG)
#define DBG  Debug
#define DBGW DebugW
#define DBG8 DebugU8
#else
#define DBG(...)  NullFunc()
#define DBGW(...) NullFunc()
#define DBG8(...) NullFunc()
#endif

#endif

