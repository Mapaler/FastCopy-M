static char *version_id =
	"@(#)Copyright (C) 2004-2018 H.Shirouzu	Version.cpp ver3.5.4.47"
#ifdef _DEBUG
	"d"
#endif
;
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Module Name				: Version
	Create					: 2010-06-13(Sun)
	Update					: 2018-05-28(Mon)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	Modify					: Mapaler 2017-03-06
	======================================================================== */

#include "tlib/tlib.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "version.h"

static char base_ver[32];
static char full_ver[32];
static char copyright_str[128];
static char libcopyright_str[128];
static char mender_str[128];
static char admin_str[32];

void SetVersionStr(BOOL is_admin, BOOL is_noui)
{
	sprintf(full_ver, "%.20s", strstr(version_id, "ver"));
	strncpyz(base_ver, full_ver + 3, sizeof(base_ver));

	if (is_admin) {
		strcpy(admin_str, " (Admin)");
	}
	if (is_noui) {
		strcpy(admin_str, " (NoUI)");
	}
}

const char *GetVersionStr(BOOL is_base)
{
	if (full_ver[0] == 0) {
		SetVersionStr();
	}
	return	is_base ? base_ver : full_ver;
}

const char *GetVerAdminStr()
{
	if (admin_str[0] == 0) {
		SetVersionStr();
	}
	return	admin_str;
}

const char *GetCopyrightStr(void)
{
	if (copyright_str[0] == 0) {
		char *s = strchr(version_id, 'C');
		char *e = strchr(version_id, '\t');
		if (s && e && s < e) {
			sprintf(copyright_str, "%.*s", int(e-s), s);
		}
	}
	return	copyright_str;
}

const char *GetLibCopyrightStr(void)
{
	if (libcopyright_str[0] == 0) {
		strcpy(libcopyright_str, "xxHash Library:\r\nCopyright (c) 2012-2016, Yann Collet");
	}
	return	libcopyright_str;
}

const char *GetMenderStr(void)
{
	if (mender_str[0] == 0) {
		sprintf(mender_str, "FastCopy-M branch By Mapaler");
	}
	return	mender_str;
}

double VerStrToDouble(const char *s)
{
	char *opt = NULL;

	double	ver = strtod(s, &opt);
	double	sub = 0.0001;

	if (opt && *opt) {
		switch (tolower(*opt)) {
		case 'a':
			sub = strtod(opt+1, NULL) * 0.00000001;
			break;
		case 'b':
			sub = strtod(opt+1, NULL) * 0.000001;
			break;
		case 'r':
			sub = strtod(opt+1, NULL) * 0.0001;
			break;
		default:
			Debug("VerStrToDouble: unknown %s opt=%s\n", s, opt);
			break;
		}
	}

	return	ver + sub;
}

