static char *tini_id = 
	"@(#)Copyright (C) 1996-2017 H.Shirouzu		tini.cpp	Ver0.99";
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Registry Class
	Create					: 1996-06-01(Sat)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include <stdio.h>
#include <io.h>
#include <mbstring.h>
#include "tlib.h"

TInifile::TInifile(const WCHAR *_ini_file)
{
	iniFile = NULL;
	rootSec = curSec = NULL;
//	iniSize = 0;
	hMutex = NULL;
	if (_ini_file) Init(_ini_file);
}

TInifile::~TInifile(void)
{
	UnInit();
}

BOOL TInifile::Strip(const char *s, char *d, const char *strip_chars, const char *quote_chars)
{
	const char	*sv_s = s;
	const char	*e = s + strlen(s);
	int			len;
	char		start_quote = quote_chars[0];
	char		end_quote   = quote_chars[1];

	if (!d) d = (char *)s;

	while (*s && strchr(strip_chars, *s)) s++;

	if (*s == start_quote) {
		s++;
		while (s <= e && *e != end_quote) e--;
		if (s > e) goto ERR;
	}
	else {
		e--;
		while (s <= e && strchr(strip_chars, *e)) e--;
		if (s > e) goto ERR;
		e++;
	}

	len = (int)(e - s);
	memmove(d, s, len);
	d[len] = 0;
	return	TRUE;

ERR:
	if (sv_s != d) strcpy(d, sv_s);
	return	FALSE;
}

BOOL TInifile::Parse(const char *p, BOOL *is_section, char *name, char *val)
{
	while (*p && strchr(" \t\r\n", *p)) p++;

	if (*p == '[') {
		if (!Strip(p, name, " \t\r\n", "[]")) return FALSE;
		*is_section = TRUE;
		return	TRUE;
	}

	*is_section = FALSE;

	if (!isalnum(*p)) return FALSE;

	const char *e = p;
	while (*e && *e != '=') e++;
	if (*e != '=') return FALSE;

	int	len = (int)(e - p);
	memcpy(name, p, len);
	name[len] = 0;

	if (!Strip(name)) return FALSE;

	strcpy(val, e+1);
	return	Strip(val);
}

BOOL TInifile::GetFileInfo(const char *fname, FILETIME *ft, int *size)
{
	WIN32_FIND_DATA	fdat;
	HANDLE	hFind = FindFirstFile(fname, &fdat);

	if (hFind == INVALID_HANDLE_VALUE) return FALSE;

	*ft   = fdat.ftLastWriteTime;
	*size = fdat.nFileSizeLow;

	FindClose(hFind);
	return	TRUE;
}

BOOL TInifile::Lock()
{
	if (!hMutex) {
		WCHAR	buf[1024];
		WCHAR	*key = wcsrchr(iniFile, L'\\');

		key = key ? key+1 : iniFile;

		snwprintfz(buf, wsizeof(buf), L"%s_%x", key, MakeHash(iniFile,
			int(wcslen(iniFile) * sizeof(WCHAR)), 0));

		if (!(hMutex = ::CreateMutexW(NULL, FALSE, buf))) return FALSE;
	}

	return	::WaitForSingleObject(hMutex, INFINITE);
}

void TInifile::UnLock()
{
	if (hMutex) ReleaseMutex(hMutex);
}

void TInifile::UnInit()
{
	while (auto sec = TopObj()) {
		DelObj(sec);
		delete sec;
	}
	rootSec = curSec = NULL;

	free(iniFile);
	iniFile = NULL;

	if (hMutex) ::CloseHandle(hMutex);
	hMutex = NULL;
}

void TInifile::Init(const WCHAR *_ini_file)
{
	InitCore(wcsdup(_ini_file));
}

void TInifile::Init(const char *_ini_file)
{
	InitCore(IsUTF8(_ini_file) ? U8toW(_ini_file) : AtoW(_ini_file));
}

void TInifile::InitCore(WCHAR *_ini_file)
{
	iniFile = _ini_file;

	Lock();
	HANDLE	hFile = ::CreateFileW(iniFile, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

	AddObj(rootSec = new TIniSection());

	if (hFile != INVALID_HANDLE_VALUE) {
		DWORD	size  = ::GetFileSize(hFile, 0); // I don't care 4GB over :-)
		VBuf	vbuf(size + 1); // already 0 fill

		if (::ReadFile(hFile, vbuf, size, &size, 0)) {
#define MAX_INI_LINE	(64 * 1024)
			DynBuf		buf(MAX_INI_LINE);
			DynBuf		val(MAX_INI_LINE);
			char		name[1024], *tok;
			BOOL		is_section;
			TIniSection	*target_sec=rootSec;

			for (tok=strtok(vbuf, "\r\n"); tok; tok=strtok(NULL, "\r\n")) {
				BOOL	ret = Parse(tok, &is_section, name, val);
				if (!ret) { // AddKey reject over 64KB entry
					target_sec->AddKey(NULL, tok);
				}
				else if (is_section) {
					target_sec = new TIniSection();
					target_sec->Set(name);
					AddObj(target_sec);
				}
				else {
					target_sec->AddKey(name, val);
				}
			}
		}
		::CloseHandle(hFile);
//		GetFileInfo(iniFile, &iniFt, &iniSize);
	}
	UnLock();
}

char *NextBuf(VBuf *vbuf, size_t len, size_t require_min, size_t chunk_size)
{
	vbuf->AddUsedSize(len);
	if (vbuf->RemainSize() < require_min) {
		if (!vbuf->Grow(chunk_size)) return NULL;
	}
	return (char *)vbuf->UsedEnd();
}

BOOL TInifile::WriteIni()
{
	Lock();

	BOOL	ret = FALSE;
	HANDLE	hFile = ::CreateFileW(iniFile, GENERIC_WRITE, 0, 0, OPEN_ALWAYS,
									FILE_ATTRIBUTE_NORMAL, 0);

	if (hFile != INVALID_HANDLE_VALUE) {
		VBuf	vbuf(MIN_INI_ALLOC, MAX_INI_ALLOC);
		char	*p = (char *)vbuf.Buf();

		for (auto sec = TopObj(); sec && p; sec = NextObj(sec)) {
			auto key = sec->TopObj();
			if (key) {
				if (sec->Name()) {
					int len = sprintf(p, "[%s]\r\n", sec->Name());
					p = NextBuf(&vbuf, len, MIN_INI_ALLOC, MIN_INI_ALLOC);
				}
				while (key && p) {
					int	key_len = key->Key() ? (int)strlen(key->Key()) : 0;
					int	val_len = (int)strlen(key->Val());
					if (key_len + val_len >= MIN_INI_ALLOC -10) {
						Debug("Too long entry in TInifile::WriteIni\n");
						continue;
					}
					if (key->Key())	{
						int len = sprintf(p, "%s=\"%s\"\r\n", key->Key(), key->Val());
						p = NextBuf(&vbuf, len, MIN_INI_ALLOC, MIN_INI_ALLOC);
					}
					else {
						int len = sprintf(p, "%s\r\n", key->Val());
						p = NextBuf(&vbuf, len, MIN_INI_ALLOC, MIN_INI_ALLOC);
					}
					key = sec->NextObj(key);
				}
			}
		}
		if (p) {
			DWORD	size;
			ret = ::WriteFile(hFile, vbuf.Buf(), (DWORD)vbuf.UsedSize(), &size, 0);
			::SetEndOfFile(hFile);
		}
		::CloseHandle(hFile);
	}
	UnLock();

	return	ret;
}

BOOL TInifile::StartUpdate()
{
/*	FILETIME	ft;
	int			size;

	if (GetFileInfo(iniFile, &ft, &size) && (CompareFileTime(&ft, &iniFt) || size != iniSize)){
		UnInit();
		Init();
	}
*/	return	TRUE;
}

BOOL TInifile::EndUpdate()
{
	return	WriteIni();
}

TIniSection *TInifile::SearchSection(const char *section)
{
	if (!section || !*section) return rootSec;

	for (auto sec = rootSec; sec; sec = NextObj(sec)) {
		if (sec->Name() && strcmpi(sec->Name(), section) == 0) return sec;
	}
	return	NULL;
}

void TInifile::SetSection(const char *section)
{
	if (curSec && curSec != rootSec && !curSec->TopObj()) {	// 空セクション削除
		DelObj(curSec);
		delete curSec;
	}

	if ((curSec = SearchSection(section)) == NULL) {
		curSec = new TIniSection();
		curSec->Set(section);
		AddObj(curSec);
	}
}

BOOL TInifile::CurSection(char *section)
{
	strcpy(section, curSec && curSec->Name() ? curSec->Name() : "");
	return	TRUE;
}

BOOL TInifile::DelSection(const char *section)
{
	TIniSection *sec = SearchSection(section);

	if (!sec) return FALSE;

	if (sec == curSec) {
		curSec = NULL;
	}
	DelObj(sec);
	delete sec;
	return	TRUE;
}

BOOL TInifile::DelKey(const char *key)
{
	return	curSec ? curSec->DelKey(key) : FALSE;
}

BOOL TInifile::KeyToTop(const char *key)
{
	return	curSec ? curSec->KeyToTop(key) : FALSE;
}

BOOL TInifile::SetStr(const char *key, const char *val)
{
	if (!val) return DelKey(key);
	return	curSec ? curSec->AddKey(key, val) : FALSE;
}

DWORD TInifile::GetStr(const char *key_name, char *val, int max_size, const char *default_val, BOOL *use_default)
{
	TIniKey *key = curSec ? curSec->SearchKey(key_name) : NULL;
	if (use_default) *use_default = !key;
	return	sprintf(val, "%.*s", max_size, key ? key->Val() : default_val);
}

const char *TInifile::GetRawVal(const char *key_name)
{
	TIniKey *key = curSec ? curSec->SearchKey(key_name) : NULL;
	return	key ? key->Val() : NULL;
}

BOOL TInifile::SetInt(const char *key, int val)
{
	char	buf[100];
	sprintf(buf, "%d", val);
	return	SetStr(key, buf);
}

int TInifile::GetInt(const char *key, int default_val)
{
	char	buf[100];
	if (GetStr(key, buf, sizeof(buf), "") <= 0) return default_val;
	return	atoi(buf);
}

BOOL TInifile::SetInt64(const char *key, int64 val)
{
	char	buf[100];
	sprintf(buf, "%lld", val);
	return	SetStr(key, buf);
}

int64 TInifile::GetInt64(const char *key, int64 default_val)
{
	char	buf[100];
	if (GetStr(key, buf, sizeof(buf), "") <= 0) return default_val;
	return	strtoll(buf, 0, 10);
}

