static char *mainwinupd_id = 
	"@(#)Copyright (C) 2017-2018 H.Shirouzu		mainwinupd.cpp	ver3.51";
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Create					: 2004-09-15(Wed)
	Update					: 2018-01-27(Sat)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	======================================================================== */

#include "mainwin.h"
#include <time.h>

using namespace std;

static BOOL IsSilent = FALSE;

//#define UPDATE_DBG
#ifdef UPDATE_DBG
#undef  FASTCOPY_UPDATEINFO
#define FASTCOPY_UPDATEINFO	"fastcopy-update-tmp.dat"
#endif

void TMainDlg::UpdateCheck(BOOL is_silent)
{
	IsSilent = is_silent;
	TInetAsync(FASTCOPY_SITE, FASTCOPY_UPDATEINFO, hWnd, WM_FASTCOPY_UPDINFORES);
}

void TMainDlg::UpdateCheckRes(TInetReqReply *_irr)
{
	shared_ptr<TInetReqReply>	irr(_irr);

	if (irr->reply.UsedSize() == 0 || irr->code != HTTP_STATUS_OK) {
		if (!IsSilent) {
			SetDlgItemText(PATH_EDIT,
				Fmt("Can't access(%d) %s", irr->code, LoadStr(IDS_FASTCOPYURL)));
		}
		return;
	}

	IPDict	dict(irr->reply.Buf(), irr->reply.UsedSize());

	if (!ipdict_verify(&dict, &cfg.officialPub)) {
		if (!IsSilent) {
			SetDlgItemText(PATH_EDIT, "Verify error");
		}
		return;
	}

	U8str	ts;
	if (dict.get_str("time", &ts)) {
		Debug("time=%s\n", ts.s());
	}

#ifdef _WIN64
	const char *key = "x64";
#else
	const char *key = "x86";
#endif
	IPDict	data;

	if (!dict.get_dict(key, &data)) {
		if (!IsSilent) {
			SetDlgItemText(PATH_EDIT, Fmt("%s: key not found(%s)", FASTCOPY_UPDATEINFO, key));
		}
		return;
	}

	updData.DataInit();
	if (!data.get_str("ver", &updData.ver)) {
		if (!IsSilent) {
			SetDlgItemText(PATH_EDIT, Fmt("%s: ver not found", FASTCOPY_UPDATEINFO));
		}
		return;
	}

	if (!data.get_str("path", &updData.path)) {
		if (!IsSilent) {
			SetDlgItemText(PATH_EDIT, Fmt("%s: ver not found", FASTCOPY_UPDATEINFO));
		}
		return;
	}
	if (!data.get_bytes("hash", &updData.hash) || updData.hash.UsedSize() != SHA256_SIZE) {
		if (!IsSilent) {
			SetDlgItemText(PATH_EDIT, Fmt("%s: hash not found or size(%zd) err",
				FASTCOPY_UPDATEINFO, updData.hash.UsedSize()));
		}
		return;
	}
	if (!data.get_int("size", &updData.size)) {
		if (!IsSilent) {
			SetDlgItemText(PATH_EDIT, Fmt("%s: size not found err", FASTCOPY_UPDATEINFO));
		}
		return;
	}
	if (data.get_str_list("sites", &updData.sites)) {
		Debug("site num = %zd\n", updData.sites.size());
	}

	double	self_ver = VerStrToDouble(GetVersionStr(TRUE));
	double	new_ver  = VerStrToDouble(updData.ver.s());

	Debug("ver=%s path=%s hash=%zd size=%lld ver=%.8f/%.8f\n",
		updData.ver.s(), updData.path.s(), updData.hash.UsedSize(), updData.size,
		self_ver, new_ver);

	if (self_ver >= new_ver) {
		if (!IsSilent) {
			SetDlgItemText(PATH_EDIT, Fmt(LoadStr(IDS_UPDFMT_NOTNEED), GetVersionStr(TRUE),
				updData.ver.s()));
		}
		return;
	}

	if (fastCopy.IsStarting() || isDelay) {
		if (!IsSilent) {
			SetDlgItemText(PATH_EDIT, Fmt(LoadStr(IDS_UPDFMT_UPDBUSY), updData.ver.s()));
		}
		return;
	}

	bool is_auto_upd = (cfg.updCheck & 0x2) ? true : false;

	if (!IsSilent || !is_auto_upd || (TIsVirtualizedDirW(cfg.execDir) && !::IsUserAnAdmin())) {
		if (MessageBox(Fmt(LoadStr(IDS_UPDFMT_UPDMSG), updData.ver.s()), FASTCOPY, MB_OKCANCEL)
			== IDCANCEL) {
			return;
		}
		if (IsSilent && !is_auto_upd && cfg.updCheck) {
			cfg.updCheck |= 0x2;
			cfg.WriteIni();
		}
	}
	U8str	host = FASTCOPY_SITE;

	if (updData.sites.size() > 0) {
		auto	itr = updData.sites.begin();
		auto	idx = time(NULL) % updData.sites.size();
		while (idx-- > 0) itr++;
		host = (*itr)->s();
	}

	TInetAsync(host.s(), updData.path.s(), hWnd, WM_FASTCOPY_UPDDLRES);
}

void TMainDlg::UpdateDlRes(TInetReqReply *_irr)
{
	shared_ptr<TInetReqReply>	irr(_irr);
	BYTE	hash[SHA256_SIZE] = {};
	TDigest	d;

	if (irr->reply.UsedSize() <= 0) {
		SetDlgItemText(PATH_EDIT, Fmt("Update download err %zd", irr->reply.UsedSize()));
		return;
	}

	if (!d.Init(TDigest::SHA256)) {
		SetDlgItemText(PATH_EDIT, Fmt("Update digest init err"));
		return;
	}

	Debug("UpdateDlRes: reply size=%zd, code=%d\n", irr->reply.UsedSize(), irr->code);

	if (irr->code != HTTP_STATUS_OK) {
		SetDlgItemText(PATH_EDIT, Fmt("Download error status=%d len=%lld",
			irr->code, irr->reply.UsedSize()));
		return;
	}

	if ((int64)irr->reply.UsedSize() != updData.size) {
		SetDlgItemText(PATH_EDIT, Fmt("Update size not correct %lld / %lld",
			irr->reply.UsedSize(), updData.size));
		return;
	}

	if ((int64)irr->reply.UsedSize() != updData.size) {
		SetDlgItemText(PATH_EDIT, Fmt("Update size not correct %lld / %lld",
			irr->reply.UsedSize(), updData.size));
		return;
	}

	if (!d.Update(irr->reply.Buf(), (int)irr->reply.UsedSize())) {
		SetDlgItemText(PATH_EDIT, Fmt("Update digest update err"));
		return;
	}
	if (!d.GetVal(hash)) {
		SetDlgItemText(PATH_EDIT, Fmt("Update get digest err"));
		return;
	}

	if (memcmp(hash, updData.hash.Buf(), SHA256_SIZE)) {
		SetDlgItemText(PATH_EDIT, Fmt("Update verify error"));
		for (int i=0; i < SHA256_SIZE; i++) {
			Debug("%02d: %02x %02x\n", i, hash[i], updData.hash.Buf()[i]);
		}
		return;
	}
	updData.dlData = irr->reply;
	Debug("UpdateDlRes: OK %zd\n", irr->reply.UsedSize());

	UpdateExec();
}

void GenUpdateFileName(char *buf)
{
	char	path[MAX_PATH_U8] = "";
	char	dir[MAX_PATH_U8] = "";

	WCHAR	wdir[MAX_PATH] = L"";
	::GetTempPathW(wsizeof(wdir), wdir);
	WtoU8(wdir, dir, MAX_PATH_U8);

	MakePathU8(path, dir, UPDATE_FILENAME);

	strcpy(buf, path);
}

void TMainDlg::UpdateExec()
{
	char	path[MAX_PATH_U8];

	GenUpdateFileName(path);

	if (GetFileAttributesU8(path) != 0xffffffff) {
		if (!DeleteFileU8(path)) {
			char	bak[MAX_PATH_U8];
			snprintfz(bak, sizeof(bak), "%s.bak", path);
			MoveFileExU8(path, bak, MOVEFILE_REPLACE_EXISTING);
		}
	}

	HANDLE hFile = CreateFileU8(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, 0);

	if (hFile == INVALID_HANDLE_VALUE) {
		SetDlgItemTextU8(PATH_EDIT, Fmt("Update CreateFile err(%s) %d", path, GetLastError()));
		return;
	}
	DWORD	size = 0;
	if (!::WriteFile(hFile, updData.dlData.Buf(), (DWORD)updData.dlData.UsedSize(),
		&size, 0)) {
		::CloseHandle(hFile);
		SetDlgItemTextU8(PATH_EDIT, Fmt("Update WriteFile err(%s) %d", path, GetLastError()));
		return;
	}
	::CloseHandle(hFile);

	WCHAR	opt[MAX_PATH * 2];
	snwprintfz(opt, wsizeof(opt), L"%s /UPDATE /DIR=\"%s\"", U8toWs(path), TGetExeDirW());

	STARTUPINFOW		sui = { sizeof(sui) };
	PROCESS_INFORMATION pi = {};

#if 1
	if (!::CreateProcessW(NULL, opt, 0, 0, 0, CREATE_NO_WINDOW, 0, TGetExeDirW(), &sui, &pi)) {
		SetDlgItemTextU8(PATH_EDIT, Fmt("Update WriteFile err(%s) %d", path, GetLastError()));
		return;
	}
	::CloseHandle(pi.hThread);
	::CloseHandle(pi.hProcess);

#else
	Debug("CreateProcess skipped for debug\n");
#endif
	updData.DataInit();

	PostMessage(WM_CLOSE, 0, 0);
}

