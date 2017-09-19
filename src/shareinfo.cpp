static char *shareinfo_id = 
	"@(#)Copyright (C) 2015-2016 H.Shirouzu		shareinfo.cpp	Ver3.20";
/* ========================================================================
	Project  Name			: Fast Copy file and directory
	Create					: 2015-07-10(Fri)
	Update					: 2016-09-28(Wed)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	======================================================================== */

#include "tlib/tlib.h"
#include "shareinfo.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#define CHECK_CYCLE_TICK 3000

ShareInfo::ShareInfo()
{
	hInfoMutex = hInfoMap = hSelfMutex = NULL;
	head = NULL;
	selfMode  = NONE;
	selfCount = 0;
	drvMng = NULL;
}

ShareInfo::~ShareInfo()
{
	if (selfMode != NONE) ReleaseExclusive();
	UnInit();
}

bool ShareInfo::Init(DriveMng *_drvMng)
{
	hInfoMutex = ::CreateMutex(NULL, FALSE, FINFO_MUTEX);
	hInfoMap   = ::CreateFileMapping((HANDLE)~0, NULL, PAGE_READWRITE, 0, FINFO_SIZE, FINFO_MMAP);
	if ((head = (Head *)::MapViewOfFile(hInfoMap, FILE_MAP_WRITE, 0, 0, FINFO_SIZE))) {
		if (head->ver != FINFO_VERSION) InitHead();
	}
	drvMng = _drvMng;
	return	head ? true : false;
}

void ShareInfo::UnInit()
{
	if (head) {
		::UnmapViewOfFile(head);
		head = NULL;
	}
	if (hInfoMap) {
		::CloseHandle(hInfoMap);
		hInfoMap = NULL;
	}
	if (hInfoMutex) {
		::CloseHandle(hInfoMutex);
		hInfoMutex = NULL;
	}
}

bool ShareInfo::InitHead()
{
	if (!Lock()) return false;

	bool	ret = true;
	if (head->ver < FINFO_VERSION) {
		head->ver			= FINFO_VERSION;
		head->size			= FINFO_SIZE;
		head->total			= 0;
		head->netDrvMax		= 0;
		head->mutexCount	= 0;
	} else if (head->ver > FINFO_VERSION) {
		UnInit();
		ret = false;
	}
	UnLock();
	return	ret;
}

void ShareInfo::MakeMutexName(char *name, u_short count)
{
	sprintf(name, "fastcopy_%x", count);
}

bool ShareInfo::Lock(DWORD timeout)
{
	if (!head || ::WaitForSingleObject(hInfoMutex, timeout) != WAIT_OBJECT_0) return false;
	return	true;
}
void ShareInfo::UnLock()
{
	::ReleaseMutex(hInfoMutex);
}

bool ShareInfo::ReleaseExclusive()
{
	if (selfMode == NONE) return false;
	if (!Lock()) return false;

	bool ret = ReleaseExclusiveCore();

	UnLock();

	return	ret;
}

bool ShareInfo::TakeExclusive(uint64 use_drives, int max_running, int force_mode,
	ShareInfo::CheckInfo *_ci)
{
	if (!head || IsTaken()) return false;
	if (!Lock()) false;

	bool		ret = false;
	DWORD		cur = ::GetTick();
	CheckInfo	ci_tmp;
	CheckInfo	&ci = _ci ? *_ci : ci_tmp;

	Checking(cur, use_drives, &ci);

	if (ci.self_idx == -1 && hSelfMutex || ci.self_idx >= 0 && !hSelfMutex) {	// 不整合検出
		ReleaseExclusiveCore();
	}
	if (!hSelfMutex) {
		char	mutex[100];
		MakeMutexName(mutex, selfCount = ++head->mutexCount);
		if (!(hSelfMutex = ::CreateMutex(0, TRUE, mutex))) goto END;
	}
	if (ci.self_idx == -1) {
		if (head->total >= FINFO_MAXPROC) goto END;
		ci.self_idx = head->total++;
	}
	Head::Data	&data = head->data[ci.self_idx];

	data.last		= cur;
	data.mutexCount	= selfCount;

	// force_mode == 0: 使用driveを占有＆max_run以下＆他に待ちが居ないなら TAKE
	// force_mode == 1: 常に TAKE
	// force_mode == 2: max_running以下の場合、常にTAKE
	data.mode = selfMode = (force_mode == 1 || (ci.all_running < max_running &&
		(force_mode == 2 || (ci.tgt_running == 0 &&
			(ci.wait_top_idx == -1 || ci.self_idx < ci.wait_top_idx)))))
		 ? TAKE : WAIT;
	data.useDrives = use_drives;
	ret = IsTaken();

END:
	UnLock();
	return	ret;
}

int ShareInfo::RegisterNetDrive(uint64 hash_val)
{
	if (!head || !Lock()) {
		return false;
	}

	int		targ_idx = -1;
	DWORD	cur = ::GetTick();

	for (int i=0; i < head->netDrvMax; i++) {
		if (head->netDrive[i].hash == 0) {
			if (targ_idx == -1) targ_idx = i;
			continue;
		}
		if (head->netDrive[i].hash == hash_val) {
			head->netDrive[i].last = cur;
			targ_idx = i;
			goto END;
		}
	}
	if (targ_idx == -1) {
		if (head->netDrvMax >= (FINFO_MAXNETDRV / 2)) {
			if (!CleanupNetDrive(cur, &targ_idx)) goto END;
		} else {
			targ_idx = head->netDrvMax++;
		}
	}
	head->netDrive[targ_idx].hash	= hash_val;
	head->netDrive[targ_idx].last	= cur;

END:
	UnLock();
	return	targ_idx >= 0 ? (FINFO_NETDRV_BASE + targ_idx) : -1;
}

bool ShareInfo::GetCount(CheckInfo *ci, uint64 use_drives)
{
	if (!Lock()) return false;
	bool ret = Checking(GetTick(), use_drives, ci);
	UnLock();
	return	ret;
}

////////////////////////////////////////////
// Internal functions (already locked)
////////////////////////////////////////////

bool ShareInfo::Checking(DWORD cur, uint64 use_drives, CheckInfo *ci, bool include_myself)
{
	ci->Init();

	uint64	total_used = CleanupList(cur);

	total_used = drvMng->OccupancyDrives(total_used);
	use_drives = drvMng->OccupancyDrives(use_drives);

	bool	need_wait = (total_used & use_drives) ? true : false;

	for (int i=0; i < head->total; i++) {
		Head::Data	&data = head->data[i];

		if (data.mutexCount == selfCount) {
			ci->self_idx = i;
			if (include_myself) {
				data.last = cur;
			}
			else continue;
		}
		bool	is_target = use_drives ? (data.useDrives & use_drives) != 0 : false;

		if (data.mode == TAKE) {
			if (is_target) ci->tgt_running++;
			ci->all_running++;
			ci->use_drives |= data.useDrives;
		}
		else if (data.mode == WAIT) {
			if (is_target) {
				ci->tgt_waiting++;
				if (ci->wait_top_idx == -1) {
					if ((total_used & data.useDrives) == 0 || need_wait) {
						ci->wait_top_idx = i;
					}
				}
			}
			ci->all_waiting++;
		}
	}
	return	true;
}

uint64 ShareInfo::CleanupList(DWORD cur)
{
	uint64	use_drives = 0;

	for (int i=0; i < head->total; i++) {
		Head::Data	&data = head->data[i];

		if (data.mutexCount == selfCount) continue;

		if (cur - data.last > CHECK_CYCLE_TICK) {
			char	mutex[100];
			MakeMutexName(mutex, data.mutexCount);
			HANDLE	hMutex = ::OpenMutex(SYNCHRONIZE, FALSE, mutex);

			if (!hMutex) {	// 存在確認できなかったときはエントリをクリア
				head->total--;
				memmove(head->data +i, head->data +i+1, (head->total-i) * sizeof(data));
				continue;
			}
			::CloseHandle(hMutex);
			data.last = cur;
		}
		if (data.mode == TAKE) {
			use_drives |= data.useDrives;
		}
	}
	return	use_drives;
}

bool ShareInfo::ReleaseExclusiveCore()
{
	bool 	ret = false;

	if (selfCount) {
		for (int i=0; i < head->total; i++) {
			Head::Data	&data = head->data[i];

			if (data.mutexCount != selfCount) continue;
			head->total--;
			memmove(head->data +i, head->data +i+1, (head->total-i) * sizeof(data));
			ret = true;
			break;
		}
	}
	if (hSelfMutex) {
		::CloseHandle(hSelfMutex);
		hSelfMutex = NULL;
	}
	selfCount = 0;
	selfMode  = NONE;
	return	ret;
}

bool ShareInfo::CleanupNetDrive(DWORD cur, int *_vacant)
{
	CheckInfo	ci;
	if (!Checking(cur, 0, &ci, true)) return false;

	int	&vacant = *_vacant;
	int	netdrv_max = 0;
	vacant = -1;

	for (int i=0; i < head->netDrvMax; i++) {
		if (head->netDrive[i].hash == 0) {
			if (vacant == -1) vacant = i;
			continue;
		}
		if ((cur - head->netDrive[i].last) > CHECK_CYCLE_TICK) {
			if (((1ULL << (i + FINFO_NETDRV_BASE)) & ci.use_drives) == 0) {
				head->netDrive[i].hash = 0;
				head->netDrive[i].last = 0;
				if (vacant == -1) vacant = i;
				continue;
			} else {
				head->netDrive[i].last = cur;
			}
		}
		netdrv_max = i+1;
	}
	head->netDrvMax = netdrv_max;

	if (vacant == -1 && head->netDrvMax < FINFO_MAXNETDRV) {
		vacant = head->netDrvMax++;
	}

	return	vacant >= 0;
}

