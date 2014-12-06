static char *tini_id = 
	"@(#)Copyright (C) 1996-2010 H.Shirouzu		tini.cpp	Ver0.97";
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Registry Class
	Create					: 1996-06-01(Sat)
	Update					: 2010-05-09(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include <stdio.h>
#include <io.h>
#include <mbstring.h>
#include "tlib.h"

TInifile::TInifile(const char *_ini_file)
{
	ini_file = NULL;
	if (_ini_file) Init(_ini_file);
	root_sec = cur_sec = NULL;
	ini_size = -1;
	hMutex = NULL;
}

TInifile::~TInifile(void)
{
	free(ini_file);
	if (hMutex) ::CloseHandle(hMutex);
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

BOOL TInifile::Parse(const char *buf, BOOL *is_section, char *name, char *val)
{
	const char	*p = buf;

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
		char	buf[1024];
		char	*key = (char *)_mbsrchr((u_char *)ini_file, '\\');

		key = key ? key+1 : ini_file;

		sprintf(buf, "%s_%x", key, MakeHash(ini_file, (int)strlen(ini_file), 0));

		if (!(hMutex = ::CreateMutex(NULL, FALSE, buf))) return FALSE;
	}

	return	::WaitForSingleObject(hMutex, INFINITE);
}

void TInifile::UnLock()
{
	if (hMutex) ReleaseMutex(hMutex);
}

void TInifile::Init(const char *_ini_file)
{
	if (_ini_file) ini_file = strdup(_ini_file);

	Lock();
	FILE	*fp = fopen(ini_file, "r");

	AddObj(root_sec = new TIniSection());

	if (fp) {
#define MAX_INI_LINE	(64 * 1024)
		char	*buf = new char [MAX_INI_LINE];
		char	*val = new char [MAX_INI_LINE];
		char	name[1024];
		BOOL	is_section;

		TIniSection	*target_sec=root_sec;

		while (fgets(buf, MAX_INI_LINE, fp)) {
			BOOL	ret = Parse(buf, &is_section, name, val);

			if (!ret) {
				target_sec->AddKey(NULL, buf);
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
		delete [] val;
		delete [] buf;
		fclose(fp);
//		GetFileInfo(ini_file, &ini_ft, &ini_size);
	}
	UnLock();
}

void TInifile::UnInit()
{
	for (TIniSection *sec; (sec = (TIniSection *)TopObj()); ) {
		DelObj(sec);
		delete sec;
	}
	root_sec = NULL;
}

BOOL TInifile::WriteIni()
{
	Lock();

	BOOL	ret = FALSE;
	FILE	*fp = fopen(ini_file, "w");

	if (fp) {
		for (TIniSection *sec = (TIniSection *)TopObj(); sec; sec = (TIniSection *)NextObj(sec)) {
			TIniKey *key = (TIniKey *)sec->TopObj();
			if (key) {
				if (sec->Name()) {
					if (fprintf(fp, "[%s]\n", sec->Name()) < 0) goto END;
				}
				while (key) {
					if (key->Key())	{
						if (fprintf(fp, "%s=\"%s\"\n", key->Key(), key->Val()) < 0) goto END;
					}
					else {
						if (fprintf(fp, "%s", key->Val()) < 0) goto END;
					}
					key = (TIniKey *)sec->NextObj(key);
				}
			}
		}
		ret = TRUE;
END:
		fclose(fp);
	}
	UnLock();

	return	ret;
}

BOOL TInifile::StartUpdate()
{
/*	FILETIME	ft;
	int			size;

	if (GetFileInfo(ini_file, &ft, &size) && (CompareFileTime(&ft, &ini_ft) || size != ini_size)){
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
	for (TIniSection *sec = root_sec; sec; sec = (TIniSection *)NextObj(sec)) {
		if (sec->Name() && strcmpi(sec->Name(), section) == 0) return sec;
	}
	return	NULL;
}

void TInifile::SetSection(const char *section)
{
	if (cur_sec && cur_sec != root_sec && !cur_sec->TopObj()) {
		DelObj(cur_sec);
		delete cur_sec;
	}

	if ((cur_sec = SearchSection(section)) == NULL) {
		cur_sec = new TIniSection();
		cur_sec->Set(section);
		AddObj(cur_sec);
	}
}

BOOL TInifile::DelSection(const char *section)
{
	TIniSection *sec = SearchSection(section);

	if (!sec) return FALSE;

	DelObj(sec);
	delete sec;
	if (sec == cur_sec) cur_sec = NULL;
	return	TRUE;
}

BOOL TInifile::DelKey(const char *key)
{
	return	cur_sec ? cur_sec->DelKey(key) : FALSE;
}

BOOL TInifile::SetStr(const char *key, const char *val)
{
	if (!val) return DelKey(key);
	return	cur_sec ? cur_sec->AddKey(key, val) : FALSE;
}

DWORD TInifile::GetStr(const char *key_name, char *val, int max_size, const char *default_val)
{
	TIniKey *key = cur_sec ? cur_sec->SearchKey(key_name) : NULL;
	return	sprintf(val, "%.*s", max_size, key ? key->Val() : default_val);
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

