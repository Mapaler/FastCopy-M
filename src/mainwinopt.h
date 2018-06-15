/* static char *mainwinopt_id = 
	"@(#)Copyright (C) 2015-2016 H.Shirouzu			mainwinopt.h	ver3.20";
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Create					: 2015-05-27(Wed)
	Update					: 2016-09-28(Wed)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	======================================================================== */

#ifndef MAINWINOPT_H
#define MAINWINOPT_H

#define USAGE_STR	\
	L"Usage: fastcopy.exe\r\n" \
	L"  [/cmd=(diff | noexist_only | update | force_copy | sync | move | delete)]\r\n\r\n" \
\
	L"  [/include=\"...\"] [/exclude=\"...\"]\r\n" \
	L"  [/from_date=\"...\"] [/to_date=\"...\"]\r\n" \
	L"  [/min_size=\"...\"] [/max_size=\"...\"]\r\n\r\n" \
\
	L"  [/srcfile=pathlist_file] [/srcfile_w=unicode_pathlist_file]\r\n" \
	L"  [/speed=(full|autoslow|9-1|suspend)] [/auto_slow]\r\n\r\n" \
\
	L"  [/auto_close] [/force_close] [/open_window]\r\n" \
	L"  [/error_stop] [/no_exec] [/force_start[=N]]\r\n" \
	L"  [/balloon[=true|false]]\r\n" \
	L"  [/no_ui] [/no_confirm_del] [/no_confirm_stop]\r\n\r\n" \
\
	L"  [/acl] [/stream] [/reparse] [/verify]\r\n" \
	L"  [/linkdest] [/recreate]\r\n" \
	L"  [/wipe_del] [/skip_empty_dir]\r\n\r\n" \
	L"  [/dlsvt=(none|auto|always)]\r\n" \
\
	L"  [/disk_mode=(diff|same|auto)]\r\n" \
	L"  [/bufsize=N(MB)] [/estimate]\r\n"  \
	L"  [/log] [/filelog] [/logfile=fname] [/utf8]\r\n" \
	L"  [/job=jobname] [/postproc=postproc_name]\r\n\r\n" \
\
	L" from_file_or_dir  [/to=dest_dir]" \
	L"" // dummy

#define RUNAS_STR			L"/runas="
#define INSTALL_STR			L"/install"
#define CMD_STR				L"/cmd="
#define BUFSIZE_STR			L"/bufsize="
#define LOG_STR				L"/log"
#define LOGFILE_STR			L"/logfile="
#define UTF8LOG_STR			L"/utf8"
#define FILELOG_STR			L"/filelog"
#define REPARSE_STR			L"/reparse"
#define FORCESTART_STR		L"/force_start"
#define SKIPEMPTYDIR_STR	L"/skip_empty_dir"
#define ERRSTOP_STR			L"/error_stop"
#define ESTIMATE_STR		L"/estimate"
#define VERIFY_STR			L"/verify"
#define AUTOSLOW_STR		L"/auto_slow"
#define SPEED_STR			L"/speed="
#define SPEED_FULL_STR		L"full"
#define SPEED_AUTOSLOW_STR	L"autoslow"
#define SPEED_SUSPEND_STR	L"suspend"
#define DISKMODE_STR		L"/disk_mode="
#define DISKMODE_SAME_STR	L"same"
#define DISKMODE_DIFF_STR	L"diff"
#define DISKMODE_AUTO_STR	L"auto"
#define OPENWIN_STR			L"/open_window"
#define AUTOCLOSE_STR		L"/auto_close"
#define FORCECLOSE_STR		L"/force_close"
#define NOEXEC_STR			L"/no_exec"
#define FCSHEXT1_STR		L"/fc_shell_ext1"
#define NOUI_STR			L"/no_ui"
#define NOCONFIRMDEL_STR	L"/no_confirm_del"
#define NOCONFIRMSTOP_STR	L"/no_confirm_stop"
#define BALLOON_STR			L"/balloon"
#define INCLUDE_STR			L"/include="
#define EXCLUDE_STR			L"/exclude="
#define FROMDATE_STR		L"/from_date="
#define TODATE_STR			L"/to_date="
#define MINSIZE_STR			L"/min_size="
#define MAXSIZE_STR			L"/max_size="
#define JOB_STR				L"/job="
#define OWDEL_STR			L"/overwrite_del" // WIPEDEL と同じ
#define WIPEDEL_STR			L"/wipe_del"
#define ACL_STR				L"/acl"
#define STREAM_STR			L"/stream"
#define LINKDEST_STR		L"/linkdest"
#define RECREATE_STR		L"/recreate"
#define SRCFILEW_STR		L"/srcfile_w="
#define SRCFILE_STR			L"/srcfile="
#define FINACT_STR			L"/postproc="
#define DLSVT_STR			L"/dlsvt="
#define UPDATED_STR			L"/updated"
#define TO_STR				L"/to="

#define CMD_COPY_NAME_STR		L"noexist_only"
#define CMD_COPY_ATTR_STR		L"diff"
#define CMD_COPY_LASTEST_STR	L"update"
#define CMD_COPY_FORCE_STR		L"force_copy"

#define CMD_SYNC_STR			L"sync"
#define CMD_SYNC_NAME_STR		L"sync_noexist"
#define CMD_SYNC_ATTR_STR		L"sync_diff"
#define CMD_SYNC_LASTEST_STR	L"sync_update"
#define CMD_SYNC_FORCE_STR		L"sync_force"

#define CMD_MOVE_STR			L"move"
//#define CMD_MOVE_NAME_STR		L"move_noexist"
#define CMD_MOVE_ATTR_STR		L"move_diff"
#define CMD_MOVE_LASTEST_STR	L"move_update"
#define CMD_MOVE_FORCE_STR		L"move_force"

#define CMD_DELETE_STR		L"delete"

#define CMD_WITH_NOEXIST	L"noexist"
#define CMD_WITH_ATTR		L"attr"
#define CMD_WITH_UPDATE		L"update"

#define CMD_MUTUAL_STR		L"mutual"
#define CMD_TEST_STR		L"test"

#define STANDBY_STR			L"Standby"
#define HIBERNATE_STR		L"Hibernate"
#define SHUTDOWN_STR		L"Shutdown"

#endif

