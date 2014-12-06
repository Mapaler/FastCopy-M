/*static char *version_id = 
	"@(#)Copyright (C) 20004-2012 H.Shirouzu   version.cpp	ver2.10";
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Module Name				: Version
	Create					: 2010-06-13(Sun)
	Update					: 2012-06-17(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef VERSION_H
#define VERSION_H

void SetVersionStr(BOOL is_admin=FALSE);
const char *GetVersionStr();
const char *GetVerAdminStr();
const char *GetCopyrightStr(void);

#endif

