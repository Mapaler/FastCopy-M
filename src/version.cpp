static char *version_id = 
	"@(#)Copyright (C) 2004-2012 H.Shirouzu	Version.cpp	ver2.11";
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Module Name				: Version
	Create					: 2010-06-13(Sun)
	Update					: 2012-06-18(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "tlib/tlib.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "version.h"

static char version_str[32];
static char copyright_str[128];
static char admin_str[32];

void SetVersionStr(BOOL is_admin)
{
	sprintf(version_str, "%.20s", strstr(version_id, "ver"));
	if (is_admin) strcpy(admin_str, " (Admin)");
}

const char *GetVersionStr()
{
	if (version_str[0] == 0)
		SetVersionStr();
	return	version_str;
}

const char *GetVerAdminStr()
{
	return	admin_str;
}

const char *GetCopyrightStr(void)
{
	if (copyright_str[0] == 0) {
		char *s = strchr(version_id, 'C');
		char *e = strchr(version_id, '\t');
		if (s && e && s < e) {
			sprintf(copyright_str, "%.*s", e-s, s);
		}
	}
	return	copyright_str;
}

