static char *tcmndlg_id = 
	"@(#)Copyright (C) 2018-2019 H.Shirouzu		tcmndlg.cpp	Ver0.99";
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Common Dialog Class
	Create					: 2018-04-26(Thu)
	Update					: 2019-01-12(Sat)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "tlib.h"
#include <shlwapi.h>

using namespace std;

BOOL GetSelectedItems(IFileDialog *fd, vector<Wstr> *res)
{
	IFolderView2 *fv = NULL;
	if (FAILED(IUnknown_QueryService(fd, SID_SFolderView, IID_PPV_ARGS(&fv)))) return FALSE;
	scope_defer([&](){ fv->Release(); });

	IShellItemArray *sia = NULL;
	if (FAILED(fv->GetSelection(TRUE, &sia))) return FALSE;
	scope_defer([&](){ sia->Release(); });

	DWORD	num = 0;
	if (FAILED(sia->GetCount(&num))) return FALSE;

	for (DWORD i=0; i < num; i++) {
		IShellItem	*si = NULL;

		if (FAILED(sia->GetItemAt(i, &si))) return FALSE;
		scope_defer([&](){ si->Release(); });

//		LPITEMIDLIST	idl;
//		DWORD			val = 0;
//		if (SUCCEEDED(::SHGetIDListFromObject(si, &idl))) {
//			PITEMID_CHILD ic = ILFindLastID(idl);
//			HRESULT hr = fv->GetSelectionState(ic, &val);
//			DBG("hr=%x val=%x\n", hr, val);
//		}

		PWSTR path = NULL;
		if (FAILED(si->GetDisplayName(SIGDN_FILESYSPATH, &path))) continue;
		scope_defer([&](){ ::CoTaskMemFree(path); });

		res->push_back(path);
		DBGW(L"path=%s\n", path);
	}
	return	TRUE;
}

const DWORD ID_SELECT     = 601;

class IFileDialogCb : public IFileDialogEvents, public IFileDialogControlEvents
{
	int				refCnt = 0;
	BOOL			once = FALSE;
	TFileDlgSub		*wnd = NULL;
	vector<Wstr>	*res = NULL;
	Wstr			*dir = NULL;
	const WCHAR		*label = NULL;

public:
	IFileDialogCb(TFileDlgSub *_wnd, vector<Wstr> *_res, Wstr *_dir,
			const WCHAR *_label) :
		wnd(_wnd), res(_res), dir(_dir), label(_label) {
		DBGW(L"IFileDialogCb\n");
	}
	IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) {
#pragma warning (push)
#pragma warning (disable : 4838)
		static const QITAB qit[] = {
			QITABENT(IFileDialogCb, IFileDialogEvents),
			QITABENT(IFileDialogCb, IFileDialogControlEvents),
			{0},
		};
#pragma warning (pop)
		DBGW(L"QueryInterface\n");
		return QISearch(this, qit, riid, ppv);
	}
	IFACEMETHODIMP_(ULONG) AddRef()  {
		DBGW(L"AddRef\n");
		return	++refCnt;
	}
	IFACEMETHODIMP_(ULONG) Release() {
		DBGW(L"Release\n");
		if (--refCnt == 0) {
			delete this;
		}
		return	0;
	}
	IFACEMETHODIMP OnFileOk(IFileDialog *fd) {
		DBGW(L"OnFileOk\n");

		return (GetSelectedItems(fd, res) && res->size()) ? S_OK : S_FALSE;
	}
	IFACEMETHODIMP OnFolderChanging(IFileDialog *fd, IShellItem *si) {
		DBGW(L"OnFolderChanging\n");

		if (!once) {
			once = TRUE;
			IOleWindow *ow = NULL;
			if (SUCCEEDED(fd->QueryInterface(&ow))) {
				HWND	hwnd = NULL;
				ow->GetWindow(&hwnd);
				ow->Release();
				wnd->AttachWnd(hwnd, label);
			}
		}
		return E_NOTIMPL;
	}
	IFACEMETHODIMP OnFolderChange(IFileDialog *fd) {
		DBGW(L"OnFolderChange\n");
		return E_NOTIMPL;
	}
	IFACEMETHODIMP OnSelectionChange(IFileDialog *fd) {
		IShellItem	*si = NULL;
		fd->GetFolder(&si);
		scope_defer([&](){ si->Release(); });

		PWSTR path = NULL;
		if (SUCCEEDED(si->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
			*dir = path;
			::CoTaskMemFree(path);
		}
		DBGW(L"OnSelectionChange(%s)\n", dir->s());
		return S_OK;
	}
	IFACEMETHODIMP OnShareViolation(IFileDialog *fd, IShellItem *si,
		FDE_SHAREVIOLATION_RESPONSE *res) {
		DBGW(L"OnShareViolation\n");
		return E_NOTIMPL;
	}
	IFACEMETHODIMP OnTypeChange(IFileDialog *fd) {
		DBGW(L"OnTypeChange\n");
		return E_NOTIMPL;
	}
	IFACEMETHODIMP OnOverwrite(IFileDialog *fd, IShellItem *si, FDE_OVERWRITE_RESPONSE *res) {
		DBGW(L"OnOverwrite\n");
		return E_NOTIMPL;
	}
	IFACEMETHODIMP OnItemSelected(IFileDialogCustomize *fdc, DWORD idCtl, DWORD idItem) {
		DBGW(L"OnItemSelected\n");
		return E_NOTIMPL;
	}
	IFACEMETHODIMP OnButtonClicked(IFileDialogCustomize *fdc, DWORD idCtl) {
		DBGW(L"OnButtonClicked\n");
		if (idCtl == ID_SELECT) {
			IFileDialog *fd = NULL;
			if (SUCCEEDED(fdc->QueryInterface(&fd))) {
				if (GetSelectedItems(fd, res) && res->size()) {
					fd->Close(S_OK);
					fd->Release();
				}
			}
		}
		return S_OK;
	}
	IFACEMETHODIMP OnCheckButtonToggled(IFileDialogCustomize *fdc, DWORD idCtl, BOOL bChecked) {
		DBGW(L"OnCheckButtonToggled\n");
		return E_NOTIMPL;
	}
	IFACEMETHODIMP OnControlActivating(IFileDialogCustomize *fdc, DWORD idCtl) {
		DBGW(L"OnControlActivating\n");
		return E_NOTIMPL;
	}
};

#define WM_POSTSIZE	(WM_APP + 111)

BOOL TFileDlgSub::AttachWnd(HWND hTarg, const WCHAR *label)
{
	if (!TSubClass::AttachWnd(hTarg)) return FALSE;

	if (!label) return TRUE;

	hOk = GetDlgItem(IDOK);

	for (HWND h=::GetWindow(hTarg, GW_CHILD); h; h= ::GetWindow(h, GW_HWNDNEXT)) {
		WCHAR wbuf[MAX_PATH];
		if (::GetWindowTextW(h, wbuf, wsizeof(wbuf)) && !wcscmp(wbuf, label)) {
			hSel = h;
			::ShowWindow(hOk, SW_HIDE);
			::EnableWindow(hOk, FALSE);
			::SendMessage(hSel, BM_SETSTYLE, (WPARAM)BS_DEFPUSHBUTTON, TRUE);
			break;
		}
	}
	return	TRUE;
}

BOOL TFileDlgSub::EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight)
{
	if (hSel) {
		PostMessage(WM_POSTSIZE, 0, 0);
	}
	return FALSE;
}

BOOL TFileDlgSub::EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_POSTSIZE) {
		DBGW(L"EventApp\n");
		TRect	orc;
		TRect	src;
		::GetWindowRect(hOk, &orc);
		::GetWindowRect(hSel, &src);
		orc.ScreenToClient(hWnd);
		src.ScreenToClient(hWnd);
		::MoveWindow(hSel, src.left, src.top, orc.right - src.left, src.bottom - src.top, TRUE);
//		InvalidateRect(0, FALSE);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TFileDlg(TWin *parent, vector<Wstr> *res, Wstr *dir, DWORD opt,
	const WCHAR *title, const WCHAR *label)
{
	auto wnd = make_unique<TFileDlgSub>(parent);

	res->clear();

	IFileDialog *fd = NULL;
	if (opt & FDOPT_SAVE) {
		IFileSaveDialog *fsd = NULL;
		CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&fsd));
		fd = fsd;
	}
	else {
		IFileOpenDialog *fod = NULL;
		CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&fod));
		fd = fod;
	}
	if (!fd) return FALSE;
	scope_defer([&]() { fd->Release(); });

	// fdcb will be deleted by Release()
	auto	fdcb = new IFileDialogCb(wnd.get(), res, dir, label);
	DWORD	cookie;

	IFileDialogCustomize *fdc;
	if (label && SUCCEEDED(fd->QueryInterface(&fdc))) {
		fdc->AddPushButton(ID_SELECT, label);
		fdc->Release();
	}

	if (FAILED(fd->Advise(fdcb, &cookie))) return FALSE;
	scope_defer([&]() { fd->Unadvise(cookie); });

	DWORD	fopt = 0;
	if (SUCCEEDED(fd->GetOptions(&fopt))) {
		fd->SetOptions(fopt | FOS_FORCEFILESYSTEM
			| (!(opt & FDOPT_SAVE) ? FOS_FILEMUSTEXIST : 0)
			| ((opt & FDOPT_DIR) ? FOS_PICKFOLDERS : 0)
			| ((opt & FDOPT_MULTI) ? FOS_ALLOWMULTISELECT : 0)
		);
	}

	if (dir->Len()) {
		IShellItem      *si = NULL;
		SHCreateItemFromParsingName(dir->s(), 0, IID_PPV_ARGS(&si));
		if (si) {
			fd->SetFolder(si);
			si->Release();
		}
	}

	if (title) {
		fd->SetTitle(title);
	}
	fd->Show(parent ? parent->hWnd : NULL);

	return	res->size() > 0 ? TRUE : FALSE;
}

