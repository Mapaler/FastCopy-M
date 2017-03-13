/* static char *shareinfo_id = 
	"@(#)Copyright (C) 2015 H.Shirouzu		shareinfo.h	Ver3.20"; */
/* ========================================================================
	Project  Name			: Fast Copy file and directory
	Create					: 2015-07-10(Fri)
	Update					: 2016-09-27(Tue)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	======================================================================== */
#ifndef SHAREINFO_H
#define SHAREINFO_H

#include "utility.h"

#define FINFO_MUTEX			"FastCopyInfoMutex"
#define FINFO_MMAP			"FastCopyInfoMMap"
#define FINFO_SIZE			8192
#define FINFO_MAXPROC		470
#define FINFO_MAXNETDRV		38	// 64 - (26:a-z)
#define FINFO_VERSION		1
#define FINFO_NETDRV_BASE	26

class DriveMng;

class ShareInfo {
public:
	struct Head {
		int		size;		// FINFO_SIZE
		int		ver;		// FINFO_VERSION
		u_short	total;		// data[]
		u_short	netDrvMax;
		u_short	mutexCount;
		u_short	dummy1;
		BYTE	dummy2[48];	// align 64byte

// offset: 64
		struct	NetDrive {
			uint64	hash;	// == MakeHash64(UNC_root or UNC_server)
			DWORD	last;
			DWORD	dummy;
		} netDrive[FINFO_MAXNETDRV];	// 16*38 = 608byte

// offset: 672
		struct Data {
			u_short	mutexCount;
			u_short	mode; // 1:running, 2:waiting
			DWORD	last;
			uint64	useDrives;
		} data[FINFO_MAXPROC];
	};

	struct CheckInfo {
		int		all_running;
		int		all_waiting;
		int		tgt_running;
		int		tgt_waiting;
		int		wait_top_idx;
		int		self_idx;
		uint64	use_drives;

		CheckInfo() { Init(); }
		void Init() {
			all_running = all_waiting = tgt_running = tgt_waiting = 0;
			self_idx = wait_top_idx = -1;
			use_drives = 0;
		}
	};

protected:
	HANDLE		hInfoMutex;
	//HANDLE	hInfoEvent;
	HANDLE		hInfoMap;
	HANDLE		hSelfMutex;
	u_short		selfCount;
	u_short		selfMode;
	Head		*head;
	DriveMng	*drvMng;
	uint64		netDrives;

public:
	enum TakeMode { NONE, TAKE, WAIT };
	ShareInfo();
	~ShareInfo();

	bool Init(DriveMng *_drvMng);
	void UnInit();
	bool InitHead();
	void MakeMutexName(char *name, u_short count);
	bool Lock(DWORD timeout=5000);
	void UnLock();
	bool ReleaseExclusive();
	bool TakeExclusive(uint64 use_drives, int max_running=2, int force_mode=0, CheckInfo *ci=NULL);
	bool IsTaken()   { return selfMode == TAKE; }
	bool IsWaiting() { return selfMode == WAIT; }
	bool GetCount(CheckInfo *ci, uint64 use_drives=0);
	int RegisterNetDrive(uint64 hash_val);

private:
	bool Checking(DWORD cur, uint64 use_drivers, CheckInfo *ci, bool include_myself=false);
	uint64 CleanupList(DWORD cur);
	bool ReleaseExclusiveCore();
	bool CleanupNetDrive(DWORD cur, int *vacant);
};

#endif

