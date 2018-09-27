/* static char *inet_id = 
	"@(#)Copyright (C) 2017 H.Shirouzu		inet.h	Ver4.70"; */
/* ========================================================================
	Project  Name			: Internet access
	Create					: 2017-08-10(Thr)
	Update					: 2017-08-10(Thr)
	Copyright				: H.Shirouzu
	======================================================================== */
#ifndef INET_H
#define INET_H

#include <wininet.h>

#define INETREQ_GET		0x0001
#define INETREQ_SECURE	0x0002
#define INETREQ_NOCERT	0x0004

#define MAX_HOSTBUF	1024
#define MAX_URLBUF	16384

struct TInetReqReply {
	int64	id;
	DWORD	code;
	DynBuf	reply;
	U8str	errMsg;
};

struct TInetReqParam { // 内部利用
	U8str	host;
	U8str	path;
	DynBuf	data;
	DWORD	flags;

	int64	id;
	HWND	hWnd;
	UINT	uMsg;
};

void TInetSetUserAgent(const char *key);
void TInetSetUserAgentRaw(const char *agent);
const char *TInetGetUserAgent();
const char *TGetAddHeadStr();
void TSetAddHeadStr(const char *head);

DWORD TInetRequest(LPCSTR host, LPCSTR path, BYTE *data, int data_len, DynBuf *reply,
	U8str *errMsg, DWORD flags=0);
void TInetRequestAsync(LPCSTR host, LPCSTR path, BYTE *data, int data_len,
	HWND hWnd, UINT uMsg, int64 id=0, DWORD flags=0);

// wrapper for TInetRequestAsync
void TInetAsync(LPCSTR host, LPCSTR path, HWND hWnd, UINT uMsg, int64 id=0);

BOOL TInetCrackUrl(LPCSTR url, U8str *host, U8str *path=NULL, int *port=NULL, BOOL *is_sec=NULL);

#endif

