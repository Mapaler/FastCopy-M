/* @(#)Copyright (C) 2018 H.Shirouzu		tcmndlg.cpp	Ver0.99 */
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Common Dialog Class
	Create					: 2018-04-26(Thu)
	Update					: 2018-04-26(Thu)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef TCMNDLG_H
#define TCMNDLG_H

#include <vector>

class TFileDlgSub : public TSubClass {
	HWND	hOk = NULL;
	HWND	hSel = NULL;

public:
	TFileDlgSub(TWin *parent) : TSubClass(parent) {
	}
	virtual BOOL AttachWnd(HWND hTarg, const WCHAR *label);
	virtual BOOL EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight);
	virtual BOOL EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam);

};

enum {
	FDOPT_NONE		= 0x0000,
	FDOPT_FILE		= 0x0001,
	FDOPT_DIR		= 0x0002,
	FDOPT_DIRFILE	= 0x0004,
	FDOPT_MULTI		= 0x0008,
	FDOPT_SAVE		= 0x0010,
};

BOOL TFileDlg(TWin *wnd, std::vector<Wstr> *res, Wstr *dir, DWORD opt,
		const WCHAR *title=NULL, const WCHAR *label=NULL);

#endif

