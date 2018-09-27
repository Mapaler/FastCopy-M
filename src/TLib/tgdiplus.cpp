static char *tgdiplus_id = 
	"@(#)Copyright (C) 2018 H.Shirouzu		tgdiplus.cpp	Ver0.99";
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: GDI Plus
	Create					: 2018-04-17(Tue)
	Update					: 2018-04-17(Tue)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "tlib.h"
#include "tgdiplus.h"

using namespace std;
using namespace Gdiplus;
#pragma comment (lib, "gdiplus.lib")

///  TGdiplusInit ///
void TGdiplusInit()
{
	static GdiplusStartupInput	gdisi;
	static ULONG_PTR			gditk;

	static BOOL once = [&]() {
		::GdiplusStartup(&gditk, &gdisi, NULL);
		return	TRUE;
	}();
}

///  TGif ///
TGif::TGif()
{
}

TGif::~TGif()
{
}

BOOL TGif::Init(const WCHAR *fname)
{
	TGdiplusInit();

	img = unique_ptr<Image>(Image::FromFile(fname));
	if (!img) {
		return FALSE;
	}

	return	GetInfo();
}

BOOL TGif::Init(int resId)
{
	TGdiplusInit();

	HRSRC	hRes = ::FindResource(TApp::hInst(), (LPCSTR)(LONG_PTR)resId, "GIF");
	if (!hRes) return FALSE;

	DWORD	size = ::SizeofResource(TApp::hInst(), hRes);
	HGLOBAL	hGl = ::LoadResource(TApp::hInst(), hRes);
	void	*pResData = ::LockResource(hGl);

	gbuf.Init(size);
	memcpy(gbuf.Buf(), pResData, size);

	IStream *is = NULL;
	::CreateStreamOnHGlobal(gbuf.Handle(), FALSE, &is);
	if (!is) return FALSE;
	
	img = unique_ptr<Image>(Image::FromStream(is));
	is->Release();

	return	GetInfo();
}

BOOL TGif::GetInfo()
{
	// 真面目にやるなら img->GetFrameDimensionsCount();
	if (!img) return FALSE;

	img->GetFrameDimensionsList(&guid, 1);
	frameNum = img->GetFrameCount(&guid);
	delays.resize(frameNum);

	if (frameNum == 0) return FALSE;

	auto fdsize = img->GetPropertyItemSize(PropertyTagFrameDelay);
	if (auto buf = make_unique<char[]>(fdsize)) {
		auto pi = (PropertyItem *)buf.get();
		img->GetPropertyItem(PropertyTagFrameDelay, fdsize, pi);

		auto	val = (DWORD *)pi->value;
		for (int i = 0; i < frameNum; i++) {
			delays[i] = val[i] * 10; // 1/100sec to 1ms
		}
	}

	auto lcsize = img->GetPropertyItemSize(PropertyTagLoopCount);
	if (auto buf = make_unique<char[]>(lcsize)) {
		auto pi = (PropertyItem *)buf.get();
		img->GetPropertyItem(PropertyTagLoopCount, lcsize, pi);
		loopCount = *(SHORT *)pi->value;
	}
	return	TRUE;
}

DWORD TGif::SetFrame()
{
	img->SelectActiveFrame(&guid, curIdx);
	curDelay = delays[curIdx];

	return	curDelay;
}

void TGif::IncrFrameIdx()
{
	if (++curIdx == frameNum) {
		curLoop++;
		curIdx = 0;
	}
}


///  TGifWin ///
TGifWin::TGifWin(TWin *_parent) : TWin(_parent)
{
}

TGifWin::~TGifWin()
{
}

BOOL TGifWin::Create(long x, long y, DWORD style)
{
	if (gif.frameNum == 0) return FALSE;

	auto sz = gif.GetSize();
	rect.Init(x, y, sz.cx, sz.cy);
	timerID = 0;

	return	TWin::CreateW(NULL, L"", style);
}

BOOL TGifWin::Play(UINT _uFinMsg, BOOL _endReset)
{
	uFinMsg = _uFinMsg;
	endReset = _endReset;
	timerID = 0;

	gif.ResetCnt();
	Draw();
	gif.IncrFrameIdx();

	if (gif.curDelay && !gif.IsFin()) {
		timerID = SetTimer(TGIFWIN_TIMER, gif.curDelay);
		return	TRUE;
	}
	else {
		uFinMsg = 0;
	}
	return	FALSE;
}

BOOL TGifWin::IsPlay()
{
	return	uFinMsg ? TRUE : FALSE;
}

BOOL TGifWin::IsPause()
{
	return	(uFinMsg && timerID == 0) ? TRUE : FALSE;
}

BOOL TGifWin::Pause()
{
	if (gif.curDelay && !gif.IsFin()) {
		KillTimer(TGIFWIN_TIMER);
		timerID = 0;
		return	TRUE;
	}
	return	FALSE;
}

BOOL TGifWin::Resume()
{
	if (gif.curDelay && !gif.IsFin()) {
		timerID = SetTimer(TGIFWIN_TIMER, gif.curDelay);
		return	TRUE;
	}
	return	FALSE;
}

void TGifWin::Stop()
{
	KillTimer(TGIFWIN_TIMER);
	timerID = 0;
	uFinMsg = 0;
}

BOOL TGifWin::Draw()
{
	if (!gif.frameNum) return FALSE;

	gif.SetFrame();

	if (HDC hDc = ::GetDC(hWnd)) {
		Paint(hDc);
		::ReleaseDC(hWnd, hDc);
	}

	return	TRUE;
}

void TGifWin::Paint(HDC hDc)
{
	Graphics g(hDc);

	g.DrawImage(gif.img.get(), 0, 0, gif.img->GetWidth(), gif.img->GetHeight());
}

BOOL TGifWin::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	return	FALSE;
}

BOOL TGifWin::EvCreate(LPARAM lParam)
{
	return	FALSE;
}

BOOL TGifWin::EvTimer(WPARAM _timerID, TIMERPROC proc)
{
	if (_timerID == TGIFWIN_TIMER) {
		KillTimer(_timerID);
		timerID = 0;

		if (gif.IsFin()) {
			if (endReset) {
				gif.ResetCnt();
				Draw();
			}
			if (parent && uFinMsg) {
				parent->PostMessage(uFinMsg, 0, 0);
				uFinMsg = 0;
			}
			return	TRUE;
		}
		Draw();

		gif.IncrFrameIdx();
		if (gif.curDelay) {
			timerID = SetTimer(TGIFWIN_TIMER, gif.curDelay);
		}
		return	TRUE;
	}
	return	FALSE;
}

BOOL TGifWin::EvPaint(void)
{
	PAINTSTRUCT	ps;

	if (HDC hDc = ::BeginPaint(hWnd, &ps)) {
		Paint(hDc);
		::EndPaint(hWnd, &ps);
	}
	return	TRUE;
}