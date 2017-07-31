static char *version_id =
	"@(#)Copyright (C) 2004-2017 H.Shirouzu	Version.cpp ver3.3.1.42";
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Module Name				: Version
	Create					: 2010-06-13(Sun)
	Update					: 2017-07-30(Sun)
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

static char version_str[32];
static char copyright_str[128];
static char libcopyright_str[128];
static char mender_str[128];
static char admin_str[32];

void SetVersionStr(BOOL is_admin, BOOL is_noui)
{
	sprintf(version_str, "%.20s", strstr(version_id, "ver"));

	if (is_admin) {
		strcpy(admin_str, " (Admin)");
	}
	if (is_noui) {
		strcpy(admin_str, " (NoUI)");
	}
}

const char *GetVersionStr()
{
	if (version_str[0] == 0) {
		SetVersionStr();
	}
	return	version_str;
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
		strcpy(libcopyright_str, "xxHash Library:\r\nCopyright (c) 2012-2014, Yann Collet");
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

