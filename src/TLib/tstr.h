/* @(#)Copyright (C) 2018 H.Shirouzu		tstr.h	Ver0.99 */
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Main Header
	Update					: 2018-06-12(Tue)
	Update					: 2018-06-12(Tue)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef TLIBSTR_H
#define TLIBSTR_H

int MakePath(char *dest, const char *dir, const char *file, int max_len=INT_MAX);
int MakePathU8(char *dest, const char *dir, const char *file, int max_len=INT_MAX);
int MakePathW(WCHAR *dest, const WCHAR *dir, const WCHAR *file, int max_len=INT_MAX);

inline int AddPath(char *dest, const char *file, int max_len=INT_MAX) {
	return MakePath(dest, NULL, file, max_len);
}
inline int AddPathU8(char *dest, const char *file, int max_len=INT_MAX) {
	return MakePathU8(dest, NULL, file, max_len);
}
inline int AddPathW(WCHAR *dest, const WCHAR *file, int max_len=INT_MAX) {
	return MakePathW(dest, NULL, file, max_len);
}

int64 hex2ll(char *buf);
int bin2hexstr(const BYTE *bindata, size_t len, char *buf);
int bin2hexstr_revendian(const BYTE *bin, size_t len, char *buf);
int bin2hexstrW(const BYTE *bindata, size_t len, WCHAR *buf);
size_t hexstr2bin(const char *buf, BYTE *bindata, size_t maxlen);
size_t hexstr2bin_revendian(const char *buf, BYTE *bindata, size_t maxlen);
size_t hexstr2bin_revendian_ex(const char *buf, BYTE *bindata, size_t maxlen, int slen=-1);
BYTE hexstr2byte(const char *buf);
WORD hexstr2word(const char *buf);
DWORD hexstr2dword(const char *buf);
int64 hexstr2int64(const char *buf);

int bin2b64str(const BYTE *bindata, size_t len, char *buf);
int bin2b64str_revendian(const BYTE *bin, size_t len, char *buf);
size_t b64str2bin(const char *buf, BYTE *bindata, size_t maxlen);
size_t b64str2bin_ex(const char *buf, int buf_len, BYTE *bindata, size_t maxlen);
size_t b64str2bin_revendian(const char *buf, BYTE *bindata, size_t maxlen);

void swap_s(BYTE *s, size_t len);

int bin2urlstr(const BYTE *bindata, size_t len, char *str);
size_t urlstr2bin(const char *str, BYTE *bindata, size_t maxlen);

void rev_order(BYTE *data, size_t size);
void rev_order(const BYTE *src, BYTE *dst, size_t size);

void byte_xor(const BYTE *in1, const BYTE *in2, BYTE *out, size_t len);

char *strdupNew(const char *_s, int max_len=-1);
WCHAR *wcsdupNew(const WCHAR *_s, int max_len=-1);
int ReplaceCharW(WCHAR *s, WCHAR rep_in, WCHAR rep_out, int max_len);

int snprintfz(char *buf, int size, const char *fmt,...);
int vsnprintfz(char *buf, int size, const char *fmt, va_list ap);
int snwprintfz(WCHAR *buf, int wsize, const WCHAR *fmt,...);
int vsnwprintfz(WCHAR *buf, int wsize, const WCHAR *fmt, va_list ap);

int strcpyz(char *dest, const char *src);
int wcscpyz(WCHAR *dest, const WCHAR *src);
int strncpyz(char *dest, const char *src, int num);
int strncatz(char *dest, const char *src, int num);
const char *strnchr(const char *srs, char ch, int num);
int wcsncpyz(WCHAR *dest, const WCHAR *src, int num);
int wcsncatz(WCHAR *dest, const WCHAR *src, int num);
const WCHAR *wcsnchr(const WCHAR *src, WCHAR ch, int num);

WCHAR *strtok_pathW(WCHAR *str, const WCHAR *sep, WCHAR **p, BOOL remove_quote=TRUE);
WCHAR **CommandLineToArgvExW(WCHAR *cmdLine, int *_argc);

#endif

