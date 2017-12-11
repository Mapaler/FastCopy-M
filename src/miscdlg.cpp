static char *miscdlg_id = 
	"@(#)Copyright (C) 2005-2016 H.Shirouzu		miscdlg.cpp	ver3.20";
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Create					: 2005-01-23(Sun)
	Update					: 2016-09-28(Wed)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	Modify					: Mapaler 2015-09-22
	======================================================================== */

#include "mainwin.h"
#include <stdio.h>

#include "shellext/shelldef.h"

#include <shlwapi.h>

#define CONTROL_GROUP				2000
#define CONTROL_RADIOBUTTONLIST		2
#define CONTROL_RADIOBUTTON_Folder	1
#define CONTROL_RADIOBUTTON_File	2

IShellItem *defaultFolder = NULL;

/*
	About Dialog初期化処理
*/
TAboutDlg::TAboutDlg(TWin *_parent) : TDlg(ABOUT_DIALOG, _parent)
{
}

/*
	Window 生成時の CallBack
*/
BOOL TAboutDlg::EvCreate(LPARAM lParam)
{
	char	org[MAX_PATH], buf[MAX_PATH];

	GetDlgItemText(ABOUT_STATIC, org, sizeof(org));
	sprintf(buf, org, FASTCOPY_TITLE, GetVersionStr(), GetVerAdminStr(), GetCopyrightStr(), GetMenderStr());
	SetDlgItemText(ABOUT_STATIC, buf);

	SetDlgItemText(LIB_STATIC, GetLibCopyrightStr());

	if (rect.left == CW_USEDEFAULT)
	{
		GetWindowRect(&rect);
		int xsize = rect.cx();
		int	ysize = rect.cy();
		int	cx = ::GetSystemMetrics(SM_CXFULLSCREEN), cy = ::GetSystemMetrics(SM_CYFULLSCREEN);
		int	x = (cx - xsize)/2;
		int y = (cy - ysize)/2;

		MoveWindow((x < 0) ? 0 : x % (cx - xsize), (y < 0) ? 0 : y % (cy - ysize),
			xsize, ysize, FALSE);
	}
	else
		MoveWindow(rect.left, rect.top, rect.cx(), rect.cy(), FALSE);

	return	TRUE;
}

BOOL TAboutDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:
	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;

	case URL_BUTTON:
		::ShellExecuteW(NULL, NULL, LoadStrW(IDS_FASTCOPYURL), NULL, NULL, SW_SHOW);
		return	TRUE;
	}
	return	FALSE;
}

int TExecConfirmDlg::Exec(const WCHAR *_src, const WCHAR *_dst)
{
	src = _src;
	dst = _dst;

	return	TDlg::Exec();
}

BOOL TExecConfirmDlg::EvCreate(LPARAM lParam)
{
	if (title) SetWindowTextW(title);

	SendDlgItemMessageW(MESSAGE_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)EditWordBreakProcW);
	SetDlgItemTextW(SRC_EDIT, src);
	if (dst) SetDlgItemTextW(DST_EDIT, dst);

	if (rect.left == CW_USEDEFAULT) {
		GetWindowRect(&rect);
		rect.left += 30, rect.right += 30;
		rect.top += 30, rect.bottom += 30;
		MoveWindow(rect.left, rect.top, rect.cx(), rect.cy(), FALSE);
	}

	SetDlgItem(SRC_EDIT, XY_FIT);
	if (resId == COPYCONFIRM_DIALOG) SetDlgItem(DST_STATIC, LEFT_FIT|BOTTOM_FIT);
	if (resId == COPYCONFIRM_DIALOG) SetDlgItem(DST_EDIT, X_FIT|BOTTOM_FIT);
//	if (resId == COPYCONFIRM_DIALOG)
//		SetDlgItem(HELP_CONFIRM_BUTTON, LEFT_FIT|BOTTOM_FIT);
	if (resId == DELCONFIRM_DIALOG)  SetDlgItem(OWDEL_CHECK, LEFT_FIT|BOTTOM_FIT);
	SetDlgItem(IDOK, LEFT_FIT|BOTTOM_FIT);
	SetDlgItem(IDCANCEL, LEFT_FIT|BOTTOM_FIT);

	if (isShellExt && IsWinVista() && !::IsUserAnAdmin()) {
		HWND	hRunas = GetDlgItem(RUNAS_BUTTON);
		::SetWindowLong(hRunas, GWL_STYLE, ::GetWindowLong(hRunas, GWL_STYLE) | WS_VISIBLE);
		::SendMessage(hRunas, BCM_SETSHIELD, 0, 1);
		SetDlgItem(RUNAS_BUTTON, LEFT_FIT|BOTTOM_FIT);
	}

	Show();
	if (info->mode == FastCopy::DELETE_MODE) {
		CheckDlgButton(OWDEL_CHECK,
			(info->flags & (FastCopy::OVERWRITE_DELETE|FastCopy::OVERWRITE_DELETE_NSA)) != 0);
	}
	SetForceForegroundWindow();

	GetWindowRect(&orgRect);

	return	TRUE;
}

BOOL TExecConfirmDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case HELP_CONFIRM_BUTTON:
		ShowHelpW(0, cfg->execDir, LoadStrW(IDS_FASTCOPYHELP), L"#shellcancel");
		return	TRUE;

	case IDOK: case IDCANCEL: case RUNAS_BUTTON:
		EndDialog(wID);
		return	TRUE;

	case OWDEL_CHECK:
		{
			parent->CheckDlgButton(OWDEL_CHECK, IsDlgButtonChecked(OWDEL_CHECK));
			FastCopy::Flags	flags = cfg->enableNSA ?
				FastCopy::OVERWRITE_DELETE_NSA : FastCopy::OVERWRITE_DELETE;
			if (IsDlgButtonChecked(OWDEL_CHECK))
				info->flags |= flags;
			else
				info->flags &= ~(int64)flags;
		}
		return	TRUE;
	}
	return	FALSE;
}

BOOL TExecConfirmDlg::EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight)
{
	if (fwSizeType == SIZE_RESTORED || fwSizeType == SIZE_MAXIMIZED)
		return	FitDlgItems();
	return	FALSE;;
}


//
//   CLASS: CFileDialogEventHandler
//
//   PURPOSE: 
//   File Dialog Event Handler that responds to Events in Added Controls. The 
//   events handler provided by the calling process can implement 
//   IFileDialogControlEvents in addition to IFileDialogEvents. 
//   IFileDialogControlEvents enables the calling process to react to these events: 
//     1) PushButton clicked. 
//     2) CheckButton state changed. 
//     3) Item selected from a menu, ComboBox, or RadioButton list. 
//     4) Control activating. This is sent when a menu is about to display a 
//        drop-down list, in case the calling process wants to change the items in 
//        the list.
//
class CFileDialogEventHandler :
	public IFileDialogEvents,
	public IFileDialogControlEvents
{
public:

	// 
	// IUnknown methods
	// 

	IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv)
	{
		static const QITAB qit[] =
		{
			QITABENT(CFileDialogEventHandler, IFileDialogEvents),
			QITABENT(CFileDialogEventHandler, IFileDialogControlEvents),
			{ 0 }
		};
		return QISearch(this, qit, riid, ppv);
	}

	IFACEMETHODIMP_(ULONG) AddRef()
	{
		return InterlockedIncrement(&m_cRef);
	}
    IFACEMETHODIMP_(ULONG) Release()
    {
        long cRef = InterlockedDecrement(&m_cRef);
        if (!cRef)
        {
            delete this;
        }
        return cRef;
    }

    // 
    // IFileDialogEvents methods
    // 

    IFACEMETHODIMP OnFileOk(IFileDialog*)
    { return S_OK; }
    IFACEMETHODIMP OnFolderChange(IFileDialog*)
    { return S_OK; }
    IFACEMETHODIMP OnFolderChanging(IFileDialog*, IShellItem*)
    { return S_OK; }
    IFACEMETHODIMP OnHelp(IFileDialog*)
    { return S_OK; }
    IFACEMETHODIMP OnSelectionChange(IFileDialog*)
    { return S_OK; }
    IFACEMETHODIMP OnTypeChange(IFileDialog*)
    { return S_OK; }
    IFACEMETHODIMP OnShareViolation(IFileDialog*, IShellItem*, FDE_SHAREVIOLATION_RESPONSE*)
    { return S_OK; }
    IFACEMETHODIMP OnOverwrite(IFileDialog*, IShellItem*, FDE_OVERWRITE_RESPONSE*)
    { return S_OK; }

    // 
    // IFileDialogControlEvents methods
    // 

	IFACEMETHODIMP OnItemSelected(IFileDialogCustomize*pfdc, DWORD dwIDCtl, DWORD dwIDItem)
	{
		IFileDialog *pfd = NULL;
		HRESULT hr = pfdc->QueryInterface(&pfd);
		if (SUCCEEDED(hr))
		{
			if (dwIDCtl == CONTROL_RADIOBUTTONLIST)
			{
				DWORD dwOptions;
				//IShellItem *dwFolder = NULL;
				//PWSTR folderShellItemName = NULL;
				switch (dwIDItem)
				{
				case CONTROL_RADIOBUTTON_Folder:
					hr = pfd->GetOptions(&dwOptions);
					hr = pfd->GetFolder(&defaultFolder);
					//defaultFolder->GetDisplayName(SIGDN_FILESYSPATH, & folderShellItemName);
					//MessageBoxW(0, folderShellItemName, L"The selected folder is", MB_OK);

					if(!(dwOptions & FOS_PICKFOLDERS))
						hr = pfd->Close(hr);
					break;

				case CONTROL_RADIOBUTTON_File:
					hr = pfd->GetOptions(&dwOptions);
					hr = pfd->GetFolder(&defaultFolder);
					//defaultFolder->GetDisplayName(SIGDN_FILESYSPATH, &folderShellItemName);
					//MessageBoxW(0, folderShellItemName, L"The selected folder is", MB_OK);

					if (dwOptions & FOS_PICKFOLDERS)
						hr = pfd->Close(hr);
					break;
				}
			}
			pfd->Release();
		}
		return hr;
	}

	IFACEMETHODIMP OnButtonClicked(IFileDialogCustomize*, DWORD)
	{ return S_OK; }
	IFACEMETHODIMP OnControlActivating(IFileDialogCustomize*, DWORD)
	{ return S_OK; }
	IFACEMETHODIMP OnCheckButtonToggled(IFileDialogCustomize*, DWORD, BOOL)
	{ return S_OK; }

	CFileDialogEventHandler() : m_cRef(1) { }

protected:

	~CFileDialogEventHandler() { }
	long m_cRef;
};


//
//   FUNCTION: CFileDialogEventHandler_CreateInstance(REFIID, void**)
//
//   PURPOSE:  CFileDialogEventHandler instance creation helper function.
//
HRESULT CFileDialogEventHandler_CreateInstance(REFIID riid, void **ppv)
{
	*ppv = NULL;
	CFileDialogEventHandler* pFileDialogEventHandler =
		new(std::nothrow)CFileDialogEventHandler();
	HRESULT hr = pFileDialogEventHandler ? S_OK : E_OUTOFMEMORY;
	if (SUCCEEDED(hr))
	{
		hr = pFileDialogEventHandler->QueryInterface(riid, ppv);
		pFileDialogEventHandler->Release();
	}
	return hr;
}


/*=========================================================================
  クラス ： BrowseDirDlgW
  概  要 ： ディレクトリブラウズ用コモンダイアログ拡張クラス
  説  明 ： SHBrowseForFolder のサブクラス化
  注  意 ： 
=========================================================================*/
BOOL BrowseDirDlgW(TWin *parentWin, UINT editCtl, WCHAR *title, int flg)
{
	WCHAR		fileBuf[MAX_PATH_EX] = L"";
	WCHAR		buf[MAX_PATH_EX] = L"";
	BOOL		ret = FALSE;
	PathArray	pathArray;

	parentWin->GetDlgItemTextW(editCtl, fileBuf, MAX_PATH_EX);
	pathArray.RegisterMultiPath(fileBuf, CRLF);

	if (pathArray.Num() > 0) {
		wcscpy(fileBuf, pathArray.Path(0));
	}
	else {
		WCHAR	*c_root_v = L"C:\\";
		wcscpy(fileBuf, c_root_v);
	}

	DirFileMode mode = (flg & BRDIR_INITFILESEL) ? FILESELECT : DIRSELECT;

	TBrowseDirDlgW	dirDlg(title, fileBuf, flg, parentWin);
	TOpenFileDlg	fileDlg(parentWin, TOpenFileDlg::MULTI_OPEN,
		OFDLG_DIRSELECT|((flg & BRDIR_TAILCR) ? OFDLG_TAILCR : 0));
		
	//新式选择文件夹
	HRESULT hr = S_OK;

	// Create a new common open file dialog.
	IFileOpenDialog *pfd = NULL;
	while (mode != SELECT_EXIT) {
		switch (mode) {
		case DIRSELECT:
			hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
				IID_PPV_ARGS(&pfd));
			if (SUCCEEDED(hr))
			{
				// Set the dialog as a folder picker.
				DWORD dwOptions;
				hr = pfd->GetOptions(&dwOptions);
				if (SUCCEEDED(hr))
				{
					if (flg & BRDIR_BACKSLASH)
						hr = pfd->SetOptions(dwOptions | FOS_PICKFOLDERS); //目标目录
					else
						hr = pfd->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_ALLOWMULTISELECT);
				}

				if (SUCCEEDED(hr) && defaultFolder)
				{
					hr = pfd->SetFolder(defaultFolder);
				}

				// Set the title of the dialog.
				if (SUCCEEDED(hr))
				{
					hr = pfd->SetTitle(title);
				}

				// Create an event handling object, and hook it up to the dialog.
				IFileDialogEvents *pfde = NULL;
				hr = CFileDialogEventHandler_CreateInstance(IID_PPV_ARGS(&pfde));
				if (SUCCEEDED(hr))
				{
					// Hook up the event handler.
					DWORD dwCookie = 0;
					hr = pfd->Advise(pfde, &dwCookie);
					if (SUCCEEDED(hr))
					{
						// Set up the customization.
						IFileDialogCustomize *pfdc = NULL;
						hr = pfd->QueryInterface(IID_PPV_ARGS(&pfdc));
						if (SUCCEEDED(hr) && !(flg & BRDIR_BACKSLASH))
						{
							// Create a visual group.
							hr = pfdc->StartVisualGroup(CONTROL_GROUP, LoadStrW(IDS_IFD_GROUP));
							if (SUCCEEDED(hr))
							{

								// Add a radio-button list.
								hr = pfdc->AddRadioButtonList(CONTROL_RADIOBUTTONLIST);
								if (SUCCEEDED(hr))
								{
									// Set the state of the added radio-button list.
									hr = pfdc->SetControlState(CONTROL_RADIOBUTTONLIST,
										CDCS_VISIBLE | CDCS_ENABLED);
								}

								// Add individual buttons to the radio-button list.
								if (SUCCEEDED(hr))
								{
									hr = pfdc->AddControlItem(CONTROL_RADIOBUTTONLIST,
										CONTROL_RADIOBUTTON_Folder, LoadStrW(IDS_DIRSELECT));
								}
								if (SUCCEEDED(hr))
								{
									hr = pfdc->AddControlItem(CONTROL_RADIOBUTTONLIST,
										CONTROL_RADIOBUTTON_File, LoadStrW(IDS_FILESELECT));
								}

								// Set the default selection to option 1.
								if (SUCCEEDED(hr))
								{
									hr = pfdc->SetSelectedControlItem(
										CONTROL_RADIOBUTTONLIST, CONTROL_RADIOBUTTON_Folder);
								}

								// End the visual group
								pfdc->EndVisualGroup();
							}
							pfdc->Release();
						}

						// Show the open file dialog.
						if (SUCCEEDED(hr))
						{
							hr = pfd->Show(parentWin->hWnd);
							if (SUCCEEDED(hr))
							{
								// Obtain the results of the user interaction.
								IShellItemArray *psiaResults = NULL;
								hr = pfd->GetResults(&psiaResults);
								if (SUCCEEDED(hr))
								{
									// Get the number of files being selected.
									DWORD dwFolderCount;
									hr = psiaResults->GetCount(&dwFolderCount);
									if (SUCCEEDED(hr))
									{

										// 处理选择多个文件夹
										if ((flg & BRDIR_CTRLADD) == 0 ||
											(::GetAsyncKeyState(VK_CONTROL) & 0x8000) == 0) {
											pathArray.Init();
										}
										for (DWORD i = 0; i < dwFolderCount; i++)
										{
											IShellItem *psi = NULL;
											if (SUCCEEDED(psiaResults->GetItemAt(i, &psi)))
											{
												// Retrieve the file path.
												PWSTR pszPath = NULL;
												if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH,
													&pszPath)))
												{
													wcscpy(fileBuf, pszPath);
													if (flg & BRDIR_BACKSLASH) {
														MakePathW(buf, fileBuf, L"");
														wcscpy(fileBuf, buf);
													}
													pathArray.RegisterPath(fileBuf);
													pathArray.GetMultiPath(fileBuf, MAX_PATH_EX);
													CoTaskMemFree(pszPath);
												}
												psi->Release();
											}
										}
										parentWin->SetDlgItemTextW(editCtl, fileBuf);
										ret = TRUE;
									}
									psiaResults->Release();
								}
							}
							else
							{
								if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
								{
									// User cancelled the dialog...
								}
							}
						}

						if (pfdc && !(flg & BRDIR_BACKSLASH))
						{
							// Determine which item in the combo box was selected.
							DWORD dwSelItem;

							hr = pfdc->GetSelectedControlItem(CONTROL_RADIOBUTTONLIST, &dwSelItem);

							if (SUCCEEDED(hr))
							{
								//if (CONTROL_RADIOBUTTON1 == dwSelItem)
									//MessageBox(0, "选择目录", NULL, 0);
								if (CONTROL_RADIOBUTTON_File == dwSelItem)
								{
									//PWSTR folderShellItemName = NULL;
									////hr = pfd->GetFolder(&defaultFolder);
									//if (defaultFolder) {
									//	defaultFolder->GetDisplayName(SIGDN_FILESYSPATH, &folderShellItemName);
									//	MessageBoxW(0, folderShellItemName, L"The selected folder is", MB_OK);
									//}
									mode = FILESELECT;
									continue;
								}
							}
						}

						// Unhook the event handler
						pfd->Unadvise(dwCookie);
					}
					pfde->Release();
				}
				pfd->Release();

			}

			// Report the error.
			if (FAILED(hr))
			{
				// If it's not that the user cancelled the dialog, report the error in a 
				// message box.
				if (hr != HRESULT_FROM_WIN32(ERROR_CANCELLED))
				{
					//XP不支持的情况
			if (dirDlg.Exec()) {
				if (fileBuf[0] == '\\') {
					GetRootDirW(fileBuf, buf);
					if (wcslen(buf) > wcslen(fileBuf)) { // netdrv root で末尾の \ がない
						wcscpy(fileBuf, buf);
					}
				}
				if ((flg & BRDIR_BACKSLASH)) {
					MakePathW(buf, fileBuf, NULW);
					wcscpy(fileBuf, buf);
				}
				if (flg & BRDIR_MULTIPATH) {
					if ((flg & BRDIR_CTRLADD) == 0 ||
						(::GetKeyState(VK_CONTROL) & 0x8000) == 0) {
						pathArray.Init();
					}
					pathArray.RegisterPath(fileBuf);
					pathArray.GetMultiPath(fileBuf, MAX_PATH_EX, CRLF, NULW,
						(flg & BRDIR_TAILCR) ? TRUE : FALSE);
				}
				parentWin->SetDlgItemTextW(editCtl, fileBuf);
				ret = TRUE;
			}
			else if (dirDlg.GetMode() == FILESELECT) {
				mode = FILESELECT;
				continue;
			}
				}
			}
			mode = SELECT_EXIT;
			break;

		case FILESELECT:

			hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
				IID_PPV_ARGS(&pfd));
			if (SUCCEEDED(hr))
			{
				// Set the dialog as a folder picker.
				DWORD dwOptions;
				hr = pfd->GetOptions(&dwOptions);
				if (SUCCEEDED(hr))
				{
					hr = pfd->SetOptions(dwOptions | FOS_ALLOWMULTISELECT);
				}

				if (SUCCEEDED(hr) && defaultFolder)
				{
					hr = pfd->SetFolder(defaultFolder);
				}

				// Create an event handling object, and hook it up to the dialog.
				IFileDialogEvents *pfde = NULL;
				hr = CFileDialogEventHandler_CreateInstance(IID_PPV_ARGS(&pfde));
				if (SUCCEEDED(hr))
				{
					// Hook up the event handler.
					DWORD dwCookie = 0;
					hr = pfd->Advise(pfde, &dwCookie);
					if (SUCCEEDED(hr))
					{
						// Set up the customization.
						IFileDialogCustomize *pfdc = NULL;
						hr = pfd->QueryInterface(IID_PPV_ARGS(&pfdc));
						if (SUCCEEDED(hr) && !(flg & BRDIR_BACKSLASH))
						{
							// Create a visual group.
							hr = pfdc->StartVisualGroup(CONTROL_GROUP, LoadStrW(IDS_IFD_GROUP));
							if (SUCCEEDED(hr))
							{

								// Add a radio-button list.
								hr = pfdc->AddRadioButtonList(CONTROL_RADIOBUTTONLIST);
								if (SUCCEEDED(hr))
								{
									// Set the state of the added radio-button list.
									hr = pfdc->SetControlState(CONTROL_RADIOBUTTONLIST,
										CDCS_VISIBLE | CDCS_ENABLED);
								}

								// Add individual buttons to the radio-button list.
								if (SUCCEEDED(hr))
								{
									hr = pfdc->AddControlItem(CONTROL_RADIOBUTTONLIST,
										CONTROL_RADIOBUTTON_Folder, LoadStrW(IDS_DIRSELECT));
								}
								if (SUCCEEDED(hr))
								{
									hr = pfdc->AddControlItem(CONTROL_RADIOBUTTONLIST,
										CONTROL_RADIOBUTTON_File, LoadStrW(IDS_FILESELECT));
								}

								// Set the default selection to option 1.
								if (SUCCEEDED(hr))
								{
									hr = pfdc->SetSelectedControlItem(
										CONTROL_RADIOBUTTONLIST, CONTROL_RADIOBUTTON_File);
								}

								// End the visual group
								pfdc->EndVisualGroup();
							}
							pfdc->Release();
						}

						// Show the open file dialog.
						if (SUCCEEDED(hr))
						{
							hr = pfd->Show(parentWin->hWnd);
							if (SUCCEEDED(hr))
							{
								// Obtain the results of the user interaction.
								IShellItemArray *psiaResults = NULL;
								hr = pfd->GetResults(&psiaResults);
								if (SUCCEEDED(hr))
								{
									// Get the number of files being selected.
									DWORD dwFolderCount;
									hr = psiaResults->GetCount(&dwFolderCount);
									if (SUCCEEDED(hr))
									{

										// 处理选择多个文件夹
										if ((flg & BRDIR_CTRLADD) == 0 ||
											(::GetAsyncKeyState(VK_CONTROL) & 0x8000) == 0) {
											pathArray.Init();
										}
										for (DWORD i = 0; i < dwFolderCount; i++)
										{
											IShellItem *psi = NULL;
											if (SUCCEEDED(psiaResults->GetItemAt(i, &psi)))
											{
												// Retrieve the file path.
												PWSTR pszPath = NULL;
												if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH,
													&pszPath)))
												{
													wcscpy(fileBuf, pszPath);
													if (flg & BRDIR_BACKSLASH) {
														MakePathW(buf, fileBuf, L"");
														wcscpy(fileBuf, buf);
													}
													pathArray.RegisterPath(fileBuf);
													pathArray.GetMultiPath(fileBuf, MAX_PATH_EX);
													CoTaskMemFree(pszPath);
												}
												psi->Release();
											}
										}
										parentWin->SetDlgItemTextW(editCtl, fileBuf);
										ret = TRUE;
									}
									psiaResults->Release();
								}
							}
							else
							{
								if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
								{
									// User cancelled the dialog...
								}
							}
						}

						if (pfdc && !(flg & BRDIR_BACKSLASH))
						{
							// Determine which item in the combo box was selected.
							DWORD dwSelItem;

							hr = pfdc->GetSelectedControlItem(CONTROL_RADIOBUTTONLIST, &dwSelItem);

							if (SUCCEEDED(hr))
							{
								if (CONTROL_RADIOBUTTON_Folder == dwSelItem)
								{
									//PWSTR folderShellItemName = NULL;
									//hr = pfd->GetFolder(&defaultFolder);
									//if (defaultFolder) {
									//	defaultFolder->GetDisplayName(SIGDN_FILESYSPATH, &folderShellItemName);
									//	MessageBoxW(0, folderShellItemName, L"The selected folder is", MB_OK);
									//}
									mode = DIRSELECT;
									continue;
								}
							}
						}

						// Unhook the event handler
						pfd->Unadvise(dwCookie);
					}
					pfde->Release();
				}
				pfd->Release();

			}

			// Report the error.
			if (FAILED(hr))
			{
				// If it's not that the user cancelled the dialog, report the error in a 
				// message box.
				if (hr != HRESULT_FROM_WIN32(ERROR_CANCELLED))
				{
					//XP不支持的情况
			buf[wcscpyz(buf, LoadStrW(IDS_ALLFILES_FILTER)) + 1] =  0;
			ret = fileDlg.Exec(editCtl, NULL, buf, fileBuf, fileBuf);
			if (fileDlg.GetMode() == DIRSELECT)
				mode = DIRSELECT;
				}
			}
			mode = SELECT_EXIT;
			break;
		}
	}

	return	ret;
}

TBrowseDirDlgW::TBrowseDirDlgW(WCHAR *title, WCHAR *_fileBuf, int _flg, TWin *parentWin)
{
	fileBuf = _fileBuf;
	flg = _flg;
	hBtn = NULL;

	iMalloc = NULL;
	SHGetMalloc(&iMalloc);

	brInfo.hwndOwner = parentWin->hWnd;
	brInfo.pidlRoot = 0;
	brInfo.pszDisplayName = fileBuf;
	brInfo.lpszTitle = title;
	brInfo.ulFlags = BIF_RETURNONLYFSDIRS|BIF_USENEWUI|BIF_UAHINT|BIF_RETURNFSANCESTORS
					 /*|BIF_SHAREABLE|BIF_BROWSEINCLUDEFILES*/;
	brInfo.lpfn = TBrowseDirDlgW::BrowseDirDlg_Proc;
	brInfo.lParam = (LPARAM)this;
	brInfo.iImage = 0;
}

TBrowseDirDlgW::~TBrowseDirDlgW()
{
	if (iMalloc)
		iMalloc->Release();
}

BOOL TBrowseDirDlgW::Exec()
{
	LPITEMIDLIST	pidlBrowse;
	BOOL			ret = FALSE;

	do {
		mode = DIRSELECT;
		if ((pidlBrowse = ::SHBrowseForFolderW(&brInfo)) != NULL) {
			if (::SHGetPathFromIDListW(pidlBrowse, fileBuf))
				ret = TRUE;
			iMalloc->Free(pidlBrowse);
			return	ret;
		}
	} while (mode == RELOAD);

	return	ret;
}

/*
	BrowseDirDlg用コールバック
*/
int CALLBACK TBrowseDirDlgW::BrowseDirDlg_Proc(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM data)
{
	switch (uMsg)
	{
	case BFFM_INITIALIZED:
		((TBrowseDirDlgW *)data)->AttachWnd(hWnd);
		break;

	case BFFM_SELCHANGED:
		if (((TBrowseDirDlgW *)data)->hWnd)
			((TBrowseDirDlgW *)data)->SetFileBuf(lParam);
		break;
	}
	return 0;
}

/*
	BrowseDlg用サブクラス生成
*/
BOOL TBrowseDirDlgW::AttachWnd(HWND _hWnd)
{
	BOOL	ret = TSubClass::AttachWnd(_hWnd);

// ディレクトリ設定
	DWORD	attr = ::GetFileAttributesW(fileBuf);
	if (attr != 0xffffffff && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0)
		GetParentDirW(fileBuf, fileBuf);

	LPITEMIDLIST pidl = ::ILCreateFromPathW(fileBuf);
	SendMessageW(BFFM_SETSELECTION, FALSE, (LPARAM)pidl);
	ILFree(pidl);

// ボタン作成
	TRect	ok_rect;
	::GetWindowRect(GetDlgItem(IDOK), &ok_rect);
	POINT	pt = { ok_rect.left, ok_rect.top };
	::ScreenToClient(hWnd, &pt);
	int		cx = (pt.x - 30) / 2;
	int		cy = ok_rect.cy();

//	::CreateWindow(BUTTON_CLASS, LoadStr(IDS_MKDIR), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
//		10, pt.y, cx, cy, hWnd, (HMENU)MKDIR_BUTTON, TApp::GetInstance(), NULL);
//	::CreateWindow(BUTTON_CLASS, LoadStr(IDS_RMDIR), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
//		18 + cx, pt.y, cx, cy, hWnd, (HMENU)RMDIR_BUTTON, TApp::GetInstance(), NULL);

	if (flg & BRDIR_FILESELECT) {
		GetClientRect(&rect);
		int	file_cx = cx * 3 / 2;
		hBtn = ::CreateWindowW(BUTTON_CLASS_W, LoadStrW(IDS_FILESELECT),
			WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_MULTILINE,
			rect.right - file_cx - 18, 10, file_cx, cy * 3 / 2 + 2,
			hWnd, (HMENU)FILESELECT_BUTTON, TApp::GetInstance(), NULL);

		::BringWindowToTop(hBtn);

		for (HWND hTmp=GetWindow(hWnd, GW_CHILD); hTmp; hTmp=::GetWindow(hTmp, GW_HWNDNEXT)) {
			::SetWindowLong(hTmp, GWL_STYLE, ::GetWindowLong(hTmp, GWL_STYLE)|WS_CLIPSIBLINGS);
		}
	}

	HFONT	hDlgFont = (HFONT)SendDlgItemMessage(IDOK, WM_GETFONT, 0, 0L);
	if (hDlgFont)
	{
//		SendDlgItemMessage(MKDIR_BUTTON, WM_SETFONT, (UINT)hDlgFont, 0L);
//		SendDlgItemMessage(RMDIR_BUTTON, WM_SETFONT, (UINT)hDlgFont, 0L);
		if (flg & BRDIR_FILESELECT) {
			::SendMessage(hBtn, WM_SETFONT, (LPARAM)hDlgFont, 0L);
		}
	}

	return	ret;
}

/*
	BrowseDlg用 WM_COMMAND 処理
*/
BOOL TBrowseDirDlgW::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID)
	{
	case MKDIR_BUTTON:
		{
			WCHAR	dirBuf[MAX_PATH_EX], path[MAX_PATH_EX];
			TInputDlg	dlg(dirBuf, this);
			if (dlg.Exec() == FALSE)
				return	TRUE;
			MakePathW(path, fileBuf, dirBuf);
			if (::CreateDirectoryW(path, NULL))
			{
				wcscpy(fileBuf, path);
				mode = RELOAD;
				PostMessage(WM_CLOSE, 0, 0);
			}
		}
		return	TRUE;

	case RMDIR_BUTTON:
		if (::RemoveDirectoryW(fileBuf))
		{
			GetParentDirW(fileBuf, fileBuf);
			mode = RELOAD;
			PostMessage(WM_CLOSE, 0, 0);
		}
		return	TRUE;

	case FILESELECT_BUTTON:
		mode = FILESELECT;
		PostMessage(WM_CLOSE, 0, 0);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TBrowseDirDlgW::EventCtlColor(UINT uMsg, HDC hDcCtl, HWND hWndCtl, HBRUSH *result)
{
	if (uMsg == WM_CTLCOLORBTN && hWndCtl == hBtn) {
	}
	return	FALSE;
}

#pragma comment (lib, "msimg32.lib")

void DrawGrad(HDC hDc, TRect rc, int gap)
{
	COLOR16 r=0xdddd, g=0xffff, b=0xdddd, r2=0x6ccc, g2=0xcfff, b2=0x6ccc;
	TRIVERTEX tx[] = {
			{ gap, gap, r, g, b, 0 },
			{ rc.cx() - gap, rc.cy() - gap, r2, g2, b2 }
	};
	GRADIENT_RECT gr[] = { { 0, 1 } };

	::GradientFill(hDc, tx, 2, &gr, 1, GRADIENT_FILL_RECT_V);
}

BOOL TBrowseDirDlgW::EvNotify(UINT ctlID, NMHDR *pNmHdr)
{
	NMCUSTOMDRAW	*ncd = (NMCUSTOMDRAW *)pNmHdr;
	if (ncd->dwDrawStage == CDDS_PREPAINT && !(ncd->uItemState & CDIS_HOT)) {
		if (pNmHdr->code == NM_CUSTOMDRAW && pNmHdr->hwndFrom == hBtn) {
			DrawGrad(ncd->hdc, ncd->rc, 2);
			return	CDRF_DODEFAULT;
		}
	}
	return	FALSE;
}

BOOL TBrowseDirDlgW::SetFileBuf(LPARAM list)
{
	return	::SHGetPathFromIDListW((LPITEMIDLIST)list, fileBuf);
}

/*
	親ディレクトリ取得（必ずフルパスであること。UNC対応）
*/
BOOL TBrowseDirDlgW::GetParentDirW(WCHAR *srcfile, WCHAR *dir)
{
	WCHAR	path[MAX_PATH_EX], *fname=NULL;

	if (::GetFullPathNameW(srcfile, MAX_PATH_EX, path, &fname) == 0 || fname == NULL)
		return	wcscpy(dir, srcfile), FALSE;

	if (fname - path > 3 || path[1] != ':')
		fname[-1] = 0;
	else
		fname[0] = 0;		// C:\ の場合

	wcscpy(dir, path);
	return	TRUE;
}


/*=========================================================================
  クラス ： TInputDlg
  概  要 ： １行入力ダイアログ
  説  明 ： 
  注  意 ： 
=========================================================================*/
BOOL TInputDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID)
	{
	case IDOK:
		GetDlgItemTextW(INPUT_EDIT, dirBuf, MAX_PATH_EX);
		EndDialog(wID);
		return	TRUE;

	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;
	}
	return	FALSE;
}

int TConfirmDlg::Exec(const WCHAR *_message, BOOL _allow_continue, TWin *_parent)
{
	message = _message;
	parent = _parent;
	allow_continue = _allow_continue;
	return	TDlg::Exec();
}

BOOL TConfirmDlg::EvCreate(LPARAM lParam)
{
	if (!allow_continue) {
		SetWindowText(LoadStr(IDS_ERRSTOP));
		::EnableWindow(GetDlgItem(IDOK), FALSE);
		::EnableWindow(GetDlgItem(IDIGNORE), FALSE);
		::SetFocus(GetDlgItem(IDCANCEL));
	}
	SendDlgItemMessageW(MESSAGE_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)EditWordBreakProcW);
	SetDlgItemTextW(MESSAGE_EDIT, message);

	if (rect.left == CW_USEDEFAULT) {
		GetWindowRect(&rect);
		rect.left += 30, rect.right += 30;
		rect.top += 30, rect.bottom += 30;
		MoveWindow(rect.left, rect.top, rect.cx(), rect.cy(), FALSE);
	}

	Show();
	SetForceForegroundWindow();
	return	TRUE;
}

/*
	ファイルダイアログ用汎用ルーチン
*/
BOOL TOpenFileDlg::Exec(UINT editCtl, WCHAR *title, WCHAR *filter, WCHAR *defaultDir,
	WCHAR *init_data)
{
#define	MAX_OFNBUF	(MAX_WPATH * 4)
	VBuf		vbuf(MAX_OFNBUF * sizeof(WCHAR));
	WCHAR		*buf = vbuf.WBuf();
	PathArray	pathArray;

	if (parent == NULL)
		return FALSE;

	parent->GetDlgItemTextW(editCtl, buf, MAX_OFNBUF);
	pathArray.RegisterMultiPath(buf, CRLF);

	if (init_data) {
		wcscpy(buf, init_data);
	}
	else if (pathArray.Num() > 0) {
		wcscpy(buf, pathArray.Path(0));
	}

	if (Exec(buf, title, filter, defaultDir) == FALSE)
		return	FALSE;

	if ((::GetKeyState(VK_CONTROL) & 0x8000) == 0) {
		pathArray.Init();
	}

	if (defaultDir) {
		int		dir_len = (int)wcslen(defaultDir);
		int		offset = dir_len + 1;
		if (buf[offset]) {  // 複数ファイル
			if (buf[wcslen(buf) -1] != '\\')	// ドライブルートのみ例外
				buf[dir_len++] = '\\';
			for (; buf[offset]; offset++) {
				offset += wcscpyz(buf + dir_len, buf + offset);
				pathArray.RegisterPath(buf);
			}
			if (pathArray.GetMultiPath(buf, MAX_OFNBUF, CRLF, NULW,
				(flg & OFDLG_TAILCR) ? TRUE : 0) >= 0) {
				parent->SetDlgItemTextW(editCtl, buf);
				return	TRUE;
			}
			else return	MessageBox("Too Many files...\n"), FALSE;
		}
	}
	pathArray.RegisterPath(buf);
	if (flg & OFDLG_WITHQUOTE) {	// FinAct用
		pathArray.GetMultiPath(buf, MAX_OFNBUF, CRLF, L" ");
	}
	else {
		pathArray.GetMultiPath(buf, MAX_OFNBUF, CRLF, NULW,
			(flg & OFDLG_TAILCR) ? TRUE : 0);
	}
	parent->SetDlgItemTextW(editCtl, buf);
	return	TRUE;
}

#ifndef OFN_ENABLESIZING
#define OFN_ENABLESIZING 0x00800000
#endif

BOOL TOpenFileDlg::Exec(WCHAR *target, WCHAR *title, WCHAR *filter, WCHAR *defaultDir)
{
	OPENFILENAMEW	ofn;
	VBuf	vbuf(MAX_WPATH * sizeof(WCHAR));
	WCHAR	szDirName[MAX_PATH] = L"";
	WCHAR	*szFile = vbuf.WBuf();
	WCHAR	*fname = NULL;

	mode = FILESELECT;

	if (target[0]) {
		DWORD	attr = ::GetFileAttributesW(target);
		if (attr != 0xffffffff && (attr & FILE_ATTRIBUTE_DIRECTORY))
			wcscpy(szDirName, target);
		else if (::GetFullPathNameW(target, MAX_PATH, szDirName, &fname) && fname) {
			fname[-1] = 0;
		}
	}
	if (szDirName[0] == 0 && defaultDir)
		wcscpy(szDirName, defaultDir);
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = parent ? parent->hWnd : NULL;
	ofn.lpstrFilter = (WCHAR *)filter;
	ofn.nFilterIndex = filter ? 1 : 0;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = MAX_WPATH;
	ofn.lpstrTitle = (WCHAR *)title;
	ofn.lpstrInitialDir = szDirName;
	ofn.lCustData = (LPARAM)this;
	ofn.lpfnHook = hook ? hook : (LPOFNHOOKPROC)OpenFileDlgProc;
	ofn.Flags = OFN_HIDEREADONLY|OFN_EXPLORER|OFN_ENABLEHOOK|OFN_ENABLESIZING;
	if (openMode == OPEN || openMode == MULTI_OPEN)
		ofn.Flags |= OFN_FILEMUSTEXIST | (openMode == MULTI_OPEN ? OFN_ALLOWMULTISELECT : 0);
	else
		ofn.Flags |= (openMode == NODEREF_SAVE ? OFN_NODEREFERENCELINKS : 0);

	WCHAR	orgDir[MAX_PATH];
	::GetCurrentDirectoryW(MAX_PATH, orgDir);

	BOOL	ret = (openMode == OPEN || openMode == MULTI_OPEN) ?
				 ::GetOpenFileNameW(&ofn) : ::GetSaveFileNameW(&ofn);

	::SetCurrentDirectoryW(orgDir);
	if (ret) {
		if (openMode == MULTI_OPEN)
			memcpy(target, szFile, MAX_WPATH * sizeof(WCHAR));
		else
			wcscpy(target, ofn.lpstrFile);

		if (defaultDir)
			wcscpy(defaultDir, ofn.lpstrFile);
	}

	return	ret;
}

UINT WINAPI TOpenFileDlg::OpenFileDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
		((TOpenFileDlg *)(((OPENFILENAMEW *)lParam)->lCustData))->AttachWnd(hdlg);
		return TRUE;
	}
	return FALSE;
}

int CalcLineCx(HDC hDc, const WCHAR *s)
{
	int		cx = 0;

	for (const WCHAR *p=s; p && *p; ) {
		const WCHAR *next_p = wcschr(p, '\n');
		if (next_p) {
			next_p++;
		}
		int	len = next_p ? int(next_p - p) : -1;
		TRect	rc;
		::DrawTextW(hDc, p, len, &rc, DT_LEFT|DT_CALCRECT|DT_SINGLELINE|DT_NOPREFIX);
		cx = max(cx, rc.cx());
		p = next_p;
	}
	return	cx;
}

BOOL CalcTextBoxSize(HDC hDc, const WCHAR *s, TSize *sz)
{
	int		max_cx = CalcLineCx(hDc, s);
	TRect	rc(0, 0, max_cx, 0);

	::DrawTextW(hDc, s, -1, &rc, DT_LEFT|DT_CALCRECT|DT_NOPREFIX|DT_WORDBREAK);

	sz->cx = rc.cx();
	sz->cy = rc.cy();

	return	TRUE;
}


/*
	TOpenFileDlg用サブクラス生成
*/
BOOL TOpenFileDlg::AttachWnd(HWND _hWnd)
{
	BOOL	ret = TSubClass::AttachWnd(::GetParent(_hWnd));

	if ((flg & OFDLG_DIRSELECT) == 0)
		return	ret;

	SetTimer(OFDLG_TIMER, 100, 0);

	return	ret;
}

BOOL TOpenFileDlg::EvTimer(WPARAM timerID, TIMERPROC proc)
{
	if (timerID == OFDLG_TIMER) {
		KillTimer(timerID);
		PostMessage(WM_OFN_AFTERPROC, 0, 0);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TOpenFileDlg::EventCtlColor(UINT uMsg, HDC hDcCtl, HWND hWndCtl, HBRUSH *result)
{
	if (uMsg == WM_CTLCOLORBTN && hWndCtl == hBtn) {
	}
	return	FALSE;
}

BOOL TOpenFileDlg::EvNotify(UINT ctlID, NMHDR *pNmHdr)
{
	NMCUSTOMDRAW	*ncd = (NMCUSTOMDRAW *)pNmHdr;
	if (ncd->dwDrawStage == CDDS_PREPAINT && !(ncd->uItemState & CDIS_HOT)) {
		if (pNmHdr->code == NM_CUSTOMDRAW && pNmHdr->hwndFrom == hBtn) {
			DrawGrad(ncd->hdc, ncd->rc, 2);
			return	CDRF_DODEFAULT;
		}
	}
	return	FALSE;
}

BOOL TOpenFileDlg::EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight)
{
	return FALSE;
}

BOOL TOpenFileDlg::EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_OFN_AFTERPROC) {
		GetWindowRect(&rect);

		MINMAXINFO	mi = {};
		SendMessage(WM_GETMINMAXINFO, 0, (LPARAM)&mi);
		int		base_size = mi.ptMinTrackSize.x;

	// ボタン作成
		TSize	sz;
		HFONT	hFont = (HFONT)SendDlgItemMessage(IDOK, WM_GETFONT, 0, 0L);
		HDC		hDc = ::GetDC(hWnd);
		HGDIOBJ	sv = ::SelectObject(hDc, hFont);
		const WCHAR	*btn_label = LoadStrW(IDS_DIRSELECT);

		CalcTextBoxSize(hDc, btn_label, &sz);
		::SelectObject(hDc, sv);
		::ReleaseDC(hWnd, hDc);

		sz.Inflate(20, 15);
		int	need_size = base_size + sz.cx - 70;

		if (rect.cx() < need_size) {
			MoveWindow(rect.left, rect.top, need_size, rect.cy(), TRUE);
			GetWindowRect(&rect);
		}

		hBtn = ::CreateWindowW(BUTTON_CLASS_W, btn_label,
			WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_MULTILINE,
			base_size - 90, 3, sz.cx, sz.cy,
			hWnd, (HMENU)DIRSELECT_BUTTON, TApp::GetInstance(), NULL);

		::SendMessage(hBtn, WM_SETFONT, (LPARAM)hFont, 0L);

		::BringWindowToTop(hBtn);

		// 子ウィンドウ同士のクリップを有効に
		for (HWND hTmp=GetWindow(hWnd, GW_CHILD); hTmp; hTmp=::GetWindow(hTmp, GW_HWNDNEXT)) {
			::SetWindowLong(hTmp, GWL_STYLE, ::GetWindowLong(hTmp, GWL_STYLE)|WS_CLIPSIBLINGS);
		}
		return	TRUE;
	}
	return	FALSE;
}


/*
	BrowseDlg用 WM_COMMAND 処理
*/
BOOL TOpenFileDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID)
	{
	case DIRSELECT_BUTTON:
		mode = DIRSELECT;
		PostMessage(WM_CLOSE, 0, 0);
		return	TRUE;
	}
	return	FALSE;
}


/*
	Job Dialog初期化処理
*/
TJobDlg::TJobDlg(Cfg *_cfg, TMainDlg *_parent) : TDlg(JOB_DIALOG, _parent)
{
	cfg = _cfg;
	mainParent = _parent;
}

/*
	Window 生成時の CallBack
*/
BOOL TJobDlg::EvCreate(LPARAM lParam)
{
	WCHAR	buf[MAX_PATH];

	if (rect.left == CW_USEDEFAULT)
	{
		GetWindowRect(&rect);
		int xsize = rect.right - rect.left;
		int	ysize = rect.bottom - rect.top;
		MoveWindow(rect.left + 30, rect.top + 50, xsize, ysize, FALSE);
	}

	for (int i=0; i < cfg->jobMax; i++)
		SendDlgItemMessageW(TITLE_COMBO, CB_ADDSTRING, 0, (LPARAM)cfg->jobArray[i]->title);

	if (mainParent->GetDlgItemTextW(JOBTITLE_STATIC, buf, MAX_PATH) > 0) {
		int idx = cfg->SearchJobW(buf);
		if (idx >= 0)
			SendDlgItemMessage(TITLE_COMBO, CB_SETCURSEL, idx, 0);
		else
			SetDlgItemTextW(TITLE_COMBO, buf);
	}
	::EnableWindow(GetDlgItem(JOBDEL_BUTTON), cfg->jobMax ? TRUE : FALSE);

	return	TRUE;
}

BOOL TJobDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:
		if (AddJob())
			EndDialog(wID);
		return	TRUE;

	case JOBDEL_BUTTON:
		DelJob();
		return	TRUE;

	case TASKSCHE_BUTTON:
		::ShellExecuteW(NULL, NULL, L"taskschd.msc", NULL, NULL, SW_SHOW);
		return	TRUE;

	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;

	case HELP_BUTTON:
		ShowHelpW(NULL, cfg->execDir, LoadStrW(IDS_FASTCOPYHELP), L"#job");
		return	TRUE;
	}
	return	FALSE;
}

BOOL TJobDlg::AddJob()
{
	WCHAR	title[MAX_PATH];

	if (GetDlgItemTextW(TITLE_COMBO, title, MAX_PATH) <= 0)
		return	FALSE;

	int	srcbuf_len = (int)mainParent->SendDlgItemMessageW(SRC_EDIT, WM_GETTEXTLENGTH, 0, 0) + 1;

	Job			job;
	int			idx = (int)mainParent->SendDlgItemMessage(MODE_COMBO, CB_GETCURSEL, 0, 0);
	CopyInfo	*info = mainParent->GetCopyInfo();
	DynBuf		src_buf(srcbuf_len * sizeof(WCHAR));
	WCHAR		dst[MAX_PATH_EX]=L"";
	DynBuf		inc;
	DynBuf		exc;
	WCHAR		from_date[MINI_BUF]=L"", to_date[MINI_BUF]=L"";
	WCHAR		min_size[MINI_BUF]=L"", max_size[MINI_BUF]=L"";

	job.bufSize = mainParent->GetDlgItemInt(BUFSIZE_EDIT);
	job.estimateMode = mainParent->IsDlgButtonChecked(ESTIMATE_CHECK);
	job.ignoreErr = mainParent->IsDlgButtonChecked(IGNORE_CHECK);
	job.enableOwdel = mainParent->IsDlgButtonChecked(OWDEL_CHECK);
	job.enableAcl = mainParent->IsDlgButtonChecked(ACL_CHECK);
	job.enableStream = mainParent->IsDlgButtonChecked(STREAM_CHECK);
	job.enableVerify = mainParent->IsDlgButtonChecked(VERIFY_CHECK);
	job.isFilter = mainParent->IsDlgButtonChecked(FILTER_CHECK);

	mainParent->GetDlgItemTextW(SRC_EDIT, src_buf, srcbuf_len);
	PathArray	srcArray;
	srcArray.RegisterMultiPath(src_buf, CRLF);
	int		src_len = srcArray.GetMultiPathLen();
	if (src_len >= MAX_HISTORY_BUF) {
		TMsgBox(this).Exec("Source is too long (max.8192 chars)");
		return	FALSE;
	}
	Wstr	src(src_len);
	srcArray.GetMultiPath(src.Buf(), src_len);

	if (info[idx].mode != FastCopy::DELETE_MODE) {
		mainParent->GetDlgItemTextW(DST_COMBO, dst, MAX_PATH_EX);
		HMENU	hMenu = ::GetSubMenu(::GetMenu(mainParent->hWnd), 2);
		if (::GetMenuState(hMenu, SAMEDISK_MENUITEM, MF_BYCOMMAND) & MF_CHECKED) {
			job.diskMode = 1;
		}
		else if (::GetMenuState(hMenu, DIFFDISK_MENUITEM, MF_BYCOMMAND) & MF_CHECKED) {
			job.diskMode = 2;
		}
	}

	if (job.isFilter) {
		auto	inc_len = ::GetWindowTextLengthW(mainParent->GetDlgItem(INCLUDE_COMBO)) + 1;
		auto	exc_len = ::GetWindowTextLengthW(mainParent->GetDlgItem(EXCLUDE_COMBO)) + 1;
		inc.Alloc(inc_len * sizeof(WCHAR));
		exc.Alloc(exc_len * sizeof(WCHAR));
		mainParent->GetDlgItemTextW(INCLUDE_COMBO, inc, inc_len);
		mainParent->GetDlgItemTextW(EXCLUDE_COMBO, exc, exc_len);

		mainParent->GetDlgItemTextW(FROMDATE_COMBO, from_date, MAX_PATH);
		mainParent->GetDlgItemTextW(TODATE_COMBO, to_date, MAX_PATH);
		mainParent->GetDlgItemTextW(MINSIZE_COMBO, min_size, MAX_PATH);
		mainParent->GetDlgItemTextW(MAXSIZE_COMBO, max_size, MAX_PATH);
	}

	job.SetString(title, src.Buf(), dst, info[idx].cmdline_name, inc, exc,
		from_date, to_date, min_size, max_size);
	if (cfg->AddJobW(&job)) {
		cfg->WriteIni();
		mainParent->SetDlgItemTextW(JOBTITLE_STATIC, title);
	}
	else TMsgBox(this).Exec("Add Job Error");

	return	TRUE;
}

BOOL TJobDlg::DelJob()
{
	WCHAR	buf[MAX_PATH], msg[MAX_PATH];

	if (GetDlgItemTextW(TITLE_COMBO, buf, MAX_PATH) > 0) {
		int idx = cfg->SearchJobW(buf);
		swprintf(msg, LoadStrW(IDS_JOBNAME), buf);
		if (idx >= 0
				&& TMsgBox(this).ExecW(msg, LoadStrW(IDS_DELCONFIRM), MB_OKCANCEL) == IDOK) {
			cfg->DelJobW(buf);
			cfg->WriteIni();
			SendDlgItemMessage(TITLE_COMBO, CB_DELETESTRING, idx, 0);
			SendDlgItemMessage(TITLE_COMBO, CB_SETCURSEL,
				idx == 0 ? 0 : idx < cfg->jobMax ? idx : idx -1, 0);
		}
	}
	::EnableWindow(GetDlgItem(JOBDEL_BUTTON), cfg->jobMax ? TRUE : FALSE);
	return	TRUE;
}

/*
	終了処理設定ダイアログ
*/
TFinActDlg::TFinActDlg(Cfg *_cfg, TMainDlg *_parent) : TDlg(FINACTION_DIALOG, _parent)
{
	cfg = _cfg;
	mainParent = _parent;
}

BOOL TFinActDlg::EvCreate(LPARAM lParam)
{
	if (rect.left == CW_USEDEFAULT)
	{
		GetWindowRect(&rect);
		int xsize = rect.right - rect.left;
		int	ysize = rect.bottom - rect.top;
		MoveWindow(rect.left + 30, rect.top + 50, xsize, ysize, FALSE);
	}

	for (int i=0; i < cfg->finActMax; i++) {
		SendDlgItemMessageW(TITLE_COMBO, CB_INSERTSTRING, i, (LPARAM)cfg->finActArray[i]->title);
	}

	for (int i=0; i < 3; i++) {
		SendDlgItemMessageW(FACMD_COMBO, CB_INSERTSTRING, i,
			(LPARAM)LoadStrW(IDS_FACMD_ALWAYS + i));
	}

	Reflect(max(mainParent->GetFinActIdx(), 0));

	return	TRUE;
}

BOOL TFinActDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case ADD_BUTTON:
		AddFinAct();
//		EndDialog(wID);
		return	TRUE;

	case DEL_BUTTON:
		DelFinAct();
		return	TRUE;

	case TITLE_COMBO:
		if (wNotifyCode == CBN_SELCHANGE) {
			Reflect((int)SendDlgItemMessage(TITLE_COMBO, CB_GETCURSEL, 0, 0));
		}
		return	TRUE;

	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;

	case SOUND_BUTTON:
		TOpenFileDlg(this).Exec(SOUND_EDIT);
		return	TRUE;

	case PLAY_BUTTON:
		{
			WCHAR	path[MAX_PATH];
			if (GetDlgItemTextW(SOUND_EDIT, path, MAX_PATH) > 0) {
				PlaySoundW(path, 0, SND_FILENAME|SND_ASYNC);
			}
		}
		return	TRUE;

	case CMD_BUTTON:
		TOpenFileDlg(this, TOpenFileDlg::OPEN, OFDLG_WITHQUOTE).Exec(CMD_EDIT);
		return	TRUE;

	case SUSPEND_CHECK:
	case HIBERNATE_CHECK:
	case SHUTDOWN_CHECK:
		CheckDlgButton(SUSPEND_CHECK,
			wID == SUSPEND_CHECK   && IsDlgButtonChecked(SUSPEND_CHECK)   ? 1 : 0);
		CheckDlgButton(HIBERNATE_CHECK,
			wID == HIBERNATE_CHECK && IsDlgButtonChecked(HIBERNATE_CHECK) ? 1 : 0);
		CheckDlgButton(SHUTDOWN_CHECK,
			wID == SHUTDOWN_CHECK  && IsDlgButtonChecked(SHUTDOWN_CHECK)  ? 1 : 0);
		return	TRUE;

	case HELP_BUTTON:
		ShowHelpW(NULL, cfg->execDir, LoadStrW(IDS_FASTCOPYHELP), L"#finact");
		return	TRUE;
	}
	return	FALSE;
}

BOOL TFinActDlg::Reflect(int idx)
{
	if (idx >= cfg->finActMax) return FALSE;

	FinAct *finAct = cfg->finActArray[idx];

	SetDlgItemTextW(TITLE_COMBO, finAct->title);
	SetDlgItemTextW(SOUND_EDIT, finAct->sound);
	SetDlgItemTextW(CMD_EDIT, finAct->command);

	char	shutdown_time[100] = "60";
	BOOL	is_stop = finAct->shutdownTime >= 0;

	if (is_stop) {
		sprintf(shutdown_time, "%d", finAct->shutdownTime);
	}
	SetDlgItemText(SHUTDOWNTIME_EDIT, shutdown_time);

	CheckDlgButton(SUSPEND_CHECK,     is_stop && (finAct->flags & FinAct::SUSPEND)      ? 1 : 0);
	CheckDlgButton(HIBERNATE_CHECK,   is_stop && (finAct->flags & FinAct::HIBERNATE)    ? 1 : 0);
	CheckDlgButton(SHUTDOWN_CHECK,    is_stop && (finAct->flags & FinAct::SHUTDOWN)     ? 1 : 0);
	CheckDlgButton(FORCE_CHECK,       is_stop && (finAct->flags & FinAct::FORCE)        ? 1 : 0);
	CheckDlgButton(SHUTDOWNERR_CHECK, is_stop && (finAct->flags & FinAct::ERR_SHUTDOWN) ? 1 : 0);

	CheckDlgButton(SOUNDERR_CHECK, (finAct->flags & FinAct::ERR_SOUND) ? 1 : 0);
	CheckDlgButton(WAITCMD_CHECK,  (finAct->flags & FinAct::WAIT_CMD)  ? 1 : 0);

	int	fidx =  (finAct->flags & FinAct::ERR_CMD) ?    2 : 
				(finAct->flags & FinAct::NORMAL_CMD) ? 1 : 0;
	SendDlgItemMessage(FACMD_COMBO, CB_SETCURSEL, fidx, 0);

	::EnableWindow(GetDlgItem(DEL_BUTTON), (finAct->flags & FinAct::BUILTIN) ? 0 : 1);

	return	TRUE;}

BOOL TFinActDlg::AddFinAct()
{
	FinAct	finAct;

	WCHAR	title[MAX_PATH];
	WCHAR	sound[MAX_PATH];
	WCHAR	command[MAX_PATH_EX];
	WCHAR	buf[MAX_PATH];

	if (GetDlgItemTextW(TITLE_COMBO, title, MAX_PATH) <= 0 || wcsicmp(title, L"FALSE") == 0)
		return	FALSE;

	int idx = cfg->SearchFinActW(title);

	GetDlgItemTextW(SOUND_EDIT, sound, MAX_PATH);
	GetDlgItemTextW(CMD_EDIT, command, MAX_PATH);
	finAct.SetString(title, sound, command);

	if (sound[0]) {
		finAct.flags |= IsDlgButtonChecked(SOUNDERR_CHECK) ? FinAct::ERR_SOUND : 0;
	}
	if (command[0]) {
		int	fidx = (int)SendDlgItemMessage(FACMD_COMBO, CB_GETCURSEL, 0, 0);
		finAct.flags |= (fidx == 1 ? FinAct::NORMAL_CMD : fidx == 2 ? FinAct::ERR_CMD : 0);
		finAct.flags |= IsDlgButtonChecked(WAITCMD_CHECK) ? FinAct::WAIT_CMD : 0;
	}

	if (idx >= 1 && (cfg->finActArray[idx]->flags & FinAct::BUILTIN)) {
		finAct.flags |= cfg->finActArray[idx]->flags &
			(FinAct::SUSPEND|FinAct::HIBERNATE|FinAct::SHUTDOWN);
	}
	else {
		finAct.flags |= IsDlgButtonChecked(SUSPEND_CHECK)   ? FinAct::SUSPEND   : 0;
		finAct.flags |= IsDlgButtonChecked(HIBERNATE_CHECK) ? FinAct::HIBERNATE : 0;
		finAct.flags |= IsDlgButtonChecked(SHUTDOWN_CHECK)  ? FinAct::SHUTDOWN  : 0;
	}

	if (finAct.flags & (FinAct::SUSPEND|FinAct::HIBERNATE|FinAct::SHUTDOWN)) {
		finAct.flags |= (IsDlgButtonChecked(FORCE_CHECK) ? FinAct::FORCE : 0);
		finAct.flags |= (IsDlgButtonChecked(SHUTDOWNERR_CHECK) ? FinAct::ERR_SHUTDOWN : 0);
		finAct.shutdownTime = 60;
		if (GetDlgItemTextW(SHUTDOWNTIME_EDIT, buf, MAX_PATH) >= 0) {
			finAct.shutdownTime = wcstoul(buf, 0, 10);
		}
	}

	if (cfg->AddFinActW(&finAct)) {
		cfg->WriteIni();
		if (SendDlgItemMessage(TITLE_COMBO, CB_GETCOUNT, 0, 0) < cfg->finActMax) {
			SendDlgItemMessageW(TITLE_COMBO, CB_INSERTSTRING, cfg->finActMax-1, (LPARAM)title);
		}
		Reflect(cfg->SearchFinActW(title));
	}
	else TMsgBox(this).Exec("Add FinAct Error");

	return	TRUE;
}

BOOL TFinActDlg::DelFinAct()
{
	WCHAR	buf[MAX_PATH], msg[MAX_PATH];

	if (GetDlgItemTextW(TITLE_COMBO, buf, MAX_PATH) > 0) {
		int idx = cfg->SearchFinActW(buf);
		swprintf(msg, LoadStrW(IDS_FINACTNAME), buf);
		if (cfg->finActArray[idx]->flags & FinAct::BUILTIN) {
			MessageBox("Can't delete buit-in Action", "Error");
		}
		else if (TMsgBox(this).ExecW(msg, LoadStrW(IDS_DELCONFIRM), MB_OKCANCEL) == IDOK) {
			cfg->DelFinActW(buf);
			cfg->WriteIni();
			SendDlgItemMessage(TITLE_COMBO, CB_DELETESTRING, idx, 0);
			idx = idx == cfg->finActMax ? idx -1 : idx;
			SendDlgItemMessage(TITLE_COMBO, CB_SETCURSEL, idx, 0);
			Reflect(idx);
		}
	}
	return	TRUE;
}


/*
	Message Box
*/
TMsgBox::TMsgBox(TWin *_parent) : TDlg(MESSAGE_DIALOG, _parent)
{
	msg = title = NULL;
	style = 0;
	isExecW = FALSE;
}

TMsgBox::~TMsgBox()
{
}

/*
	Window 生成時の CallBack
*/
BOOL TMsgBox::EvCreate(LPARAM lParam)
{
	if (rect.left == CW_USEDEFAULT)
	{
		GetWindowRect(&rect);
		int xsize = rect.right - rect.left;
		int	ysize = rect.bottom - rect.top;
		MoveWindow(rect.left + 30, rect.top + 50, xsize, ysize, FALSE);
	}

	if (isExecW) {
		SetWindowTextW(title);
		SetDlgItemTextW(MESSAGE_EDIT, msg);
	}
	else {
		SetWindowText((const char *)title);
		SetDlgItemText(MESSAGE_EDIT, (const char *)msg);
	}

	if ((style & MB_OKCANCEL) == 0) {
		::ShowWindow(GetDlgItem(IDCANCEL), SW_HIDE);
		RECT	ok_rect;
		HWND	ok_wnd = GetDlgItem(IDOK);
		::GetWindowRect(ok_wnd, &ok_rect);
		POINT	pt = { ok_rect.left, ok_rect.top };
		::ScreenToClient(hWnd, &pt);
		::SetWindowPos(ok_wnd, 0, pt.x + 50, pt.y, 0, 0, SWP_NOSIZE|SWP_NOZORDER);
	}

	GetWindowRect(&orgRect);

	SetDlgItem(MESSAGE_EDIT,	XY_FIT);
	SetDlgItem(IDOK,			LEFT_FIT|BOTTOM_FIT);
	SetDlgItem(IDCANCEL,		LEFT_FIT|BOTTOM_FIT);

	return	TRUE;
}

BOOL TMsgBox::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TMsgBox::EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight)
{
	if (fwSizeType == SIZE_RESTORED || fwSizeType == SIZE_MAXIMIZED)
		return	FitDlgItems();
	return	FALSE;;
}

UINT TMsgBox::ExecW(const WCHAR *_msg, const WCHAR *_title, UINT _style)
{
	msg = _msg;
	title = _title;
	style = _style;
	isExecW = TRUE;

	return	TDlg::Exec();
}

UINT TMsgBox::Exec(const char *_msg, const char *_title, UINT _style)
{
	msg   = (WCHAR *)_msg;
	title = (WCHAR *)_title;
	style = _style;
	isExecW = FALSE;

	return	TDlg::Exec();
}


TFinDlg::TFinDlg(TMainDlg *_parent) : TMsgBox(_parent), mainDlg(_parent)
{
}

TFinDlg::~TFinDlg()
{
}

UINT TFinDlg::Exec(int _sec, DWORD mainmsg_id)
{
	orgSec = sec = _sec;
	main_msg = LoadStr(mainmsg_id);
	proc_msg = LoadStr(IDS_WAITPROC_MSG);
	time_fmt = LoadStr(IDS_WAITTIME_MSG);
	return	TMsgBox::Exec("", "FastCopy", MB_OKCANCEL);
}


/*
	Window 生成時の CallBack
*/
BOOL TFinDlg::EvCreate(LPARAM lParam)
{
	TMsgBox::EvCreate(lParam);
	Update();
	SetWindowPos(HWND_TOPMOST, 0, 0 ,0, 0, SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE);
	SetTimer(FASTCOPY_FIN_TIMER, 1000, NULL);
	return	TRUE;
}

void TFinDlg::Update()
{
	char	buf[100];
	int		len = strcpyz(buf, main_msg);
	ShareInfo::CheckInfo ci;

	if (mainDlg->GetRunningCount(&ci) && (ci.all_running > 0 || ci.all_waiting > 0)) {
		sec = orgSec;
		strcpyz(buf+len, proc_msg);
	}
	else {
		sprintf(buf+len, time_fmt, sec--);
	}
	SetDlgItemText(MESSAGE_EDIT, buf);
}

BOOL TFinDlg::EvTimer(WPARAM timerID, TIMERPROC proc)
{
	if (sec > 0) {
		Update();
	}
	else {
		::KillTimer(hWnd, timerID);
		EndDialog(IDOK);
	}
	return	TRUE;
}

int SetSpeedLevelLabel(TWin *win, int level)
{
	if (level == -1)
		level = (int)win->SendDlgItemMessage(SPEED_SLIDER, TBM_GETPOS, 0, 0);
	else
		win->SendDlgItemMessage(SPEED_SLIDER, TBM_SETPOS, TRUE, level);

	char	buf[64];
	sprintf(buf,
			level == SPEED_FULL ?		LoadStr(IDS_FULLSPEED_DISP) :
			level == SPEED_AUTO ?		LoadStr(IDS_AUTOSLOW_DISP) :
			level == SPEED_SUSPEND ?	LoadStr(IDS_SUSPEND_DISP) :
			 							LoadStr(IDS_RATE_DISP),
			level * 10);
	win->SetDlgItemText(SPEED_STATIC, buf);
	return	level;
}


TListHeader::TListHeader(TWin *_parent) : TSubClassCtl(_parent)
{
	memset(&logFont, 0, sizeof(logFont));
}

BOOL TListHeader::AttachWnd(HWND _hWnd)
{
	BOOL ret = TSubClassCtl::AttachWnd(_hWnd);
	ChangeFontNotify();
	return	ret;
}

BOOL TListHeader::EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == HDM_LAYOUT) {
		HD_LAYOUT *hl = (HD_LAYOUT *)lParam;
		::CallWindowProcW((WNDPROC)oldProc, hWnd, uMsg, wParam, lParam);
//		Debug("HDM_LAYOUT(USER)2 top:%d/bottom:%d diff:%d cy:%d y:%d\n",
//			hl->prc->top, hl->prc->bottom, hl->prc->bottom - hl->prc->top,
//			hl->pwpos->cy, hl->pwpos->y);

		if (logFont.lfHeight) {
			int	height = abs(logFont.lfHeight) + 4;
			hl->prc->top = height;
			hl->pwpos->cy = height;
		}
		return	TRUE;
	}
	return	FALSE;
}

BOOL TListHeader::ChangeFontNotify()
{
	HFONT	hFont;

	if ((hFont = (HFONT)SendMessage(WM_GETFONT, 0, 0)) == NULL)
		return	FALSE;

	if (::GetObject(hFont, sizeof(LOGFONT), (void *)&logFont) == 0)
		return	FALSE;

//	Debug("lfHeight=%d\n", logFont.lfHeight);
	return	TRUE;
}

/*
	listview control の subclass化
	Focus を失ったときにも、選択色を変化させないための小細工
*/
#define INVALID_INDEX	-1
TListViewEx::TListViewEx(TWin *_parent) : TSubClassCtl(_parent)
{
	focus_index = INVALID_INDEX;
}

BOOL TListViewEx::EventFocus(UINT uMsg, HWND hFocusWnd)
{
	LV_ITEM	lvi;

	memset(&lvi, 0, sizeof(lvi));
	lvi.mask = LVIF_STATE;
	lvi.stateMask = LVIS_FOCUSED;
	int	maxItem = (int)SendMessage(LVM_GETITEMCOUNT, 0, 0);
	int itemNo;

	for (itemNo=0; itemNo < maxItem; itemNo++) {
		if (SendMessage(LVM_GETITEMSTATE, itemNo, (LPARAM)LVIS_FOCUSED) & LVIS_FOCUSED)
			break;
	}

	if (uMsg == WM_SETFOCUS)
	{
		if (itemNo == maxItem) {
			lvi.state = LVIS_FOCUSED;
			if (focus_index == INVALID_INDEX)
				focus_index = 0;
			SendMessage(LVM_SETITEMSTATE, focus_index, (LPARAM)&lvi);
			SendMessage(LVM_SETSELECTIONMARK, 0, focus_index);
		}
		return	FALSE;
	}
	else {	// WM_KILLFOCUS
		if (itemNo != maxItem) {
			SendMessage(LVM_SETITEMSTATE, itemNo, (LPARAM)&lvi);
			focus_index = itemNo;
		}
		return	TRUE;	// WM_KILLFOCUS は伝えない
	}
}

BOOL TListViewEx::EventButton(UINT uMsg, int nHitTest, POINTS pos)
{
	switch (uMsg)
	{
	case WM_RBUTTONDOWN:
		return	TRUE;
	case WM_RBUTTONUP:
		::PostMessage(parent->hWnd, uMsg, nHitTest, *(LPARAM *)&pos);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TListViewEx::EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (focus_index == INVALID_INDEX)
		return	FALSE;

	switch (uMsg) {
	case LVM_INSERTITEM:
		if (((LV_ITEM *)lParam)->iItem <= focus_index)
			focus_index++;
		break;
	case LVM_DELETEITEM:
		if ((int)wParam == focus_index)
			focus_index = INVALID_INDEX;
		else if ((int)wParam < focus_index)
			focus_index--;
		break;
	case LVM_DELETEALLITEMS:
		focus_index = INVALID_INDEX;
		break;
	}
	return	FALSE;
}

/*
	edit control の subclass化
*/
TEditSub::TEditSub(TWin *_parent) : TSubClassCtl(_parent)
{
}

#define EM_SETEVENTMASK			(WM_USER + 69)
#define EM_GETEVENTMASK			(WM_USER + 59)
#define ENM_LINK				0x04000000

BOOL TEditSub::AttachWnd(HWND _hWnd)
{
	if (!TSubClassCtl::AttachWnd(_hWnd))
		return	FALSE;

	// Protection for Visual C++ Resource editor problem...
	// RICHEDIT20W is correct, but VC++ changes to RICHEDIT20A, sometimes.
#ifdef _DEBUG
#define RICHED20A_TEST
#ifdef RICHED20A_TEST
	char	cname[64];
	if (::GetClassName(_hWnd, cname, sizeof(cname)) && stricmp(cname, "RICHEDIT20A") == 0) {
		static bool once = false;
		if (!once) {
			once = true;
			MessageBox("Change RichEdit20A to RichEdit20W in fastcopy.rc", "FastCopy Resource file problem");
		}
	}
#endif
#endif

//	DWORD	evMask = SendMessage(EM_GETEVENTMASK, 0, 0) | ENM_LINK;
//	SendMessage(EM_SETEVENTMASK, 0, evMask); 
//	dblClicked = FALSE;

	return	TRUE;
}

BOOL TEditSub::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case WM_UNDO:
	case WM_CUT:
	case WM_COPY:
	case WM_PASTE:
	case WM_CLEAR:
		SendMessage(wID, 0, 0);
		return	TRUE;

	case EM_SETSEL:
		SendMessage(wID, 0, -1);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TEditSub::EvContextMenu(HWND childWnd, POINTS pos)
{
	HMENU	hMenu = ::CreatePopupMenu();
	BOOL	is_readonly = (int)(GetWindowLong(GWL_STYLE) & ES_READONLY);
	UINT	flg = 0;

	flg = (is_readonly || !SendMessage(EM_CANUNDO, 0, 0)) ? MF_DISABLED|MF_GRAYED : 0;
	::AppendMenu(hMenu, MF_STRING|flg, WM_UNDO, LoadStr(IDS_UNDO));
	::AppendMenu(hMenu, MF_SEPARATOR, 0, 0);

	flg = is_readonly ? MF_DISABLED|MF_GRAYED : 0;
	::AppendMenu(hMenu, MF_STRING|flg, WM_CUT, LoadStr(IDS_CUT));
	::AppendMenu(hMenu, MF_STRING, WM_COPY, LoadStr(IDS_COPY));

	flg = (is_readonly || !SendMessage(EM_CANPASTE, 0, 0)) ? MF_DISABLED|MF_GRAYED : 0;
	::AppendMenu(hMenu, MF_STRING|flg, WM_PASTE, LoadStr(IDS_PASTE));

	flg = is_readonly ? MF_DISABLED|MF_GRAYED : 0;
	::AppendMenu(hMenu, MF_STRING|flg, WM_CLEAR, LoadStr(IDS_DELETE));
	::AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
	::AppendMenu(hMenu, MF_STRING, EM_SETSEL, LoadStr(IDS_SELECTALL));

	::TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pos.x, pos.y, 0, hWnd, NULL);
	::DestroyMenu(hMenu);

	return	TRUE;
}

int TEditSub::ExGetText(WCHAR *buf, int max_len, DWORD flags, UINT codepage)
{
	GETTEXTEX	ge;

	memset(&ge, 0, sizeof(ge));
	ge.cb = max_len;
	ge.flags = flags;
	ge.codepage = codepage;

	return	(int)SendMessageW(EM_GETTEXTEX, (WPARAM)&ge, (LPARAM)buf);
}

int TEditSub::ExSetText(const WCHAR *buf, int max_len, DWORD flags, UINT codepage)
{
	SETTEXTEX	se;

	se.flags = flags;
	se.codepage = codepage;

	return	(int)SendMessageW(EM_SETTEXTEX, (WPARAM)&se, (LPARAM)buf);
}

BOOL TSrcEdit::AttachWnd(HWND _hWnd)
{
	if (!TSubClassCtl::AttachWnd(_hWnd)) {
		return	FALSE;
	}
	SendMessage(EM_SETMARGINS, EC_LEFTMARGIN, 1);

	GetWindowRect(&orgRect);

	LOGFONT		lf = {};
	::GetObject((HFONT)SendMessage(WM_GETFONT, 0, 0L), sizeof(lf), (void *)&lf);

	marginCy = 10;
	baseCy = abs(lf.lfHeight); // 暫定...
	if (baseCy == 11) {
		baseCy += 2;
	}

//	HDC		hDc = ::GetDC(hWnd);
//	Debug("base=%d lf=%d caps=%d, rc=%d\n",
//		baseCy, lf.lfHeight, GetDeviceCaps(hDc, LOGPIXELSY), orgRect.cy());
//	::ReleaseDC(hWnd, hDc);

	return	TRUE;
}

int char_num(const WCHAR *str, WCHAR ch)
{
	int	cnt = 0;

	while (*str) {
		if (*str++ == ch) cnt++;
	}
	return	cnt;
}

int TSrcEdit::NeedDiffY()
{
	int		len = GetWindowTextLengthW() + 1;
	Wstr	wstr(len);

	if (GetWindowTextW(wstr.Buf(), len)) {
		int		crnum = max(min(char_num(wstr.s(), L'\n') + 1, maxCr), 2);
		int		needCy = baseCy * crnum + marginCy;

	//	Debug("need=%d org=%d cr=%d\n", needCy, orgRect.cy(), crnum);

		return	max(needCy - orgRect.cy(), 0);
	}
	return	0;
}

void TSrcEdit::Fit(BOOL allow_reduce)
{
	int	diff = NeedDiffY();

	GetWindowRect(&rect);
	int	cur_diff = rect.cy() - orgRect.cy();

	if (diff > cur_diff || allow_reduce && diff != cur_diff) {
	//	Debug("fit %d %d\n", diff, cur_diff);
		parent->PostMessage(WM_FASTCOPY_RESIZESRCEDIT, 0, diff);
	}
}

BOOL TSrcEdit::EvChar(WCHAR code, LPARAM keyData)
{
	switch (code) {
	case VK_RETURN:
		parent->PostMessage(WM_FASTCOPY_SRCEDITFIT, 0, 0);
		break;

	case 0x01: // Control-A
		SendMessage(EM_SETSEL, 0, -1);
		Debug("send emsetsel\n");
		break;
	}
	return	FALSE;
}

BOOL TSrcEdit::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	return	FALSE;
}

BOOL TSrcEdit::SetWindowTextW(const WCHAR *text)
{
	BOOL	ret = TWin::SetWindowTextW(text);

	parent->PostMessage(WM_FASTCOPY_SRCEDITFIT, 0, 1);
	return	ret;
}

BOOL TSrcEdit::EvCut()
{
	return	FALSE;
}

BOOL TSrcEdit::EvPaste()
{
	parent->PostMessage(WM_FASTCOPY_SRCEDITFIT, 0, 1);
	return	FALSE;
}

