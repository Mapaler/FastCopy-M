/*static char *version_id = 
	"@(#)Copyright (C) 20004-2015 H.Shirouzu   version.cpp	ver3.00";
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Module Name				: Version
	Create					: 2010-06-13(Sun)
	Update					: 2015-06-22(Mon)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	Modify					: Mapaler 2015-08-23
	======================================================================== */

#ifndef VERSION_H
#define VERSION_H

void SetVersionStr(BOOL is_admin=FALSE, BOOL is_noui=FALSE);
const char *GetVersionStr();
const char *GetVerAdminStr();
const char *GetCopyrightStr(void);
const char *GetMenderStr(void);

#endif

