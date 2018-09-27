/* @(#)Copyright (C) 2018 H.Shirouzu		tgdiplus.h	Ver0.99 */
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: GDI Plus
	Create					: 2018-04-17(Tue)
	Update					: 2018-04-17(Tue)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef TGDIPLUS_H
#define TGDIPLUS_H

#include <gdiplus.h>
#include <vector>

void TGdiplusInit();

#define TGIFWIN_TIMER	200

class TGif {
public:
	std::unique_ptr<Gdiplus::Image>	img;
	UINT	frameNum = 0;
	UINT	curIdx = 0;
	DWORD	loopCount = 0;
	UINT	curLoop = 0;

	std::vector<DWORD> delays;
	DWORD	curDelay = 0;
	GUID	guid;
	GBuf	gbuf;

	TGif();
	~TGif();

	BOOL Init(const WCHAR *fname);
	BOOL Init(int resId);

	BOOL	GetInfo();
	DWORD	SetFrame();
	void	IncrFrameIdx();
	DWORD	GetDelay() { return curDelay; }
	TSize	GetSize() {
		return TSize{ LONG(img->GetWidth()), LONG(img->GetHeight()) };
	}
	void	ResetCnt() {
		curIdx = 0;
		curLoop = 0;
	}
	BOOL	IsFin() {
		return loopCount && (curLoop >= loopCount);
	}
};

class TGifWin : public TWin {
protected:
	TGif		gif;
	BOOL		endReset = FALSE;
	UINT		uFinMsg = 0;
	UINT_PTR	timerID = 0;
	void		Paint(HDC hDc);

public:
	TGifWin(TWin *_parent);
	virtual ~TGifWin();

	BOOL SetGif(const WCHAR *fname) { return gif.Init(fname); }
	BOOL SetGif(int resId) { return gif.Init(resId); }
	BOOL Create(long x=0, long y=0, DWORD style = WS_CHILDWINDOW | WS_CLIPCHILDREN);
	BOOL Play(UINT _uMsg=0, BOOL _endReset=FALSE);
	BOOL Pause();
	BOOL Resume();
	void Stop();
	BOOL IsPlay();
	BOOL IsPause();
	BOOL Draw();

	virtual BOOL EvCreate(LPARAM lParam);
	virtual BOOL EvPaint(void);
	virtual BOOL EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL EvTimer(WPARAM timerID, TIMERPROC proc);
};


#endif

