/* static char *shelldef_id = 
	"@(#)Copyright (C) 2005-2016 H.Shirouzu		shelldef.h	Ver3.20"; */
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Create					: 2008-06-05(Thu)
	Update					: 2016-09-28(Wed)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	======================================================================== */

#ifndef SHELLDEF_H
#define SHELLDEF_H

#define FASTCOPY		"FastCopy"
#define FASTCOPYUSER	"FastCopyUser"
#define FASTCOPY_EXE	"FastCopy.exe"
#define SHELLEXT_OPT	"/fc_shell_ext1"

#define MENU_FLAGS_OLD		"FastCopyMenuFlags"
#define MENU_FLAGS			"FastCopyMenuFlags2"
#define SHEXT_RIGHT_COPY	0x00000001
#define SHEXT_RIGHT_DELETE	0x00000002
#define SHEXT_RIGHT_PASTE	0x00000004
#define SHEXT_DD_COPY		0x00000010
#define SHEXT_DD_MOVE		0x00000020
#define SHEXT_SUBMENU_RIGHT	0x00001000
#define SHEXT_SUBMENU_DD	0x00002000
#define SHEXT_SUBMENU_NOSEP	0x00004000
#define SHEXT_ISSTOREOPT	0x00010000
#define SHEXT_NOCONFIRM		0x00020000
#define SHEXT_NOCONFIRMDEL	0x00040000
#define SHEXT_TASKTRAY		0x00080000
#define SHEXT_AUTOCLOSE		0x00100000
#define SHEXT_MENUICON		0x01000000
#define SHEXT_ISADMIN		0x40000000	// shex -> fastcopy に伝搬させる
#define SHEXT_MENUFLG_EX	0x80000000
#define SHEXT_MENU_DEFAULT	0xfff00fff
#define SHEXT_MENU_NEWDEF	0xfff00fcc

#define SHEXT_RIGHT_MENUS	(SHEXT_RIGHT_COPY|SHEXT_RIGHT_DELETE|SHEXT_RIGHT_PASTE)
#define SHEXT_DD_MENUS		(SHEXT_DD_COPY|SHEXT_DD_MOVE)

#define SHEXT_MENU_COPY		0x00000000
#define SHEXT_MENU_DELETE	0x00000001
#define SHEXT_MENU_PASTE	0x00000002
#define SHEXT_MENU_MAX		0x00000003


#endif

