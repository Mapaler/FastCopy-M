static char *inet_id = 
	"@(#)Copyright (C) H.Shirouzu 2017   inet.cpp	Ver4.61";
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: Internet Access
	Create					: 2017-08-10(Thr)
	Update					: 2017-08-10(Thr)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "tlib.h"
#include <process.h>

#pragma comment (lib, "wininet.lib")

DWORD TInetRequest(LPCSTR _host, LPCSTR _path, BYTE *data, int data_len, DynBuf *reply,
	U8str *errMsg, DWORD flags)
{
	HINTERNET	hInet = NULL;
	HINTERNET	hConn = NULL;
	HINTERNET	hReq = NULL;
	LPCSTR		head = "Content-Type: application/json";
	LPCSTR		http_vers = "HTTP/1.1";
	int			port = (flags & INETREQ_SECURE) ?
					INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
	DWORD 		inet_flags =
					INTERNET_FLAG_RELOAD |
					INTERNET_FLAG_DONT_CACHE |
					INTERNET_FLAG_EXISTING_CONNECT |
					((flags & INETREQ_SECURE) ? INTERNET_FLAG_SECURE : 0) |
					((flags & INETREQ_NOCERT) ?
				INTERNET_FLAG_IGNORE_CERT_CN_INVALID|INTERNET_FLAG_IGNORE_CERT_DATE_INVALID : 0);
	DWORD		code = 0;
	DWORD		code_len = sizeof(code);
#define CHUNK_SIZE	(32 * 1024)
#define MAX_DLSIZE	(10 * 1024 * 1024)
	VBuf		buf(CHUNK_SIZE, MAX_DLSIZE);

	U8str	host = _host;
	U8str	path = _path;

	if (strstr(host.s(), "://")) {
		BOOL	is_sec = FALSE;
		if (!TInetCrackUrl(_host, &host, &path, &port, &is_sec)) {
			errMsg->Init(Fmt("URL parse err %d\n", GetLastError()));
			goto END;
		}
		if (is_sec) inet_flags |= INTERNET_FLAG_SECURE;
		else        inet_flags &= ~INTERNET_FLAG_SECURE;
	}

	if (!(hInet = ::InternetOpen(TInetGetUserAgent(), INTERNET_OPEN_TYPE_PRECONFIG, 0, 0, 0))) {
		errMsg->Init(Fmt("InternetOpen err %d\n", GetLastError()));
		goto END;
	}

	if (!(hConn = ::InternetConnect(hInet, host.s(), port, 0, 0, INTERNET_SERVICE_HTTP, 0, 0))) {
		errMsg->Init(Fmt("InternetConnect err %d\n", GetLastError()));
		goto END;
	}

	if (!(hReq = ::HttpOpenRequest(hConn, (flags & INETREQ_GET) ? "GET" : "POST",
		path.s(), http_vers, NULL, NULL, inet_flags, 0))) {
		errMsg->Init(Fmt("HttpOpenRequest err %d\n", GetLastError()));
		goto END;
	}
	Debug("host=%s path=%s data_len=%d\n", host.s(), path.s(), data_len);

	if (*TGetAddHeadStr()) {
		::HttpAddRequestHeaders(hReq, TGetAddHeadStr(), -1,
			HTTP_ADDREQ_FLAG_REPLACE|HTTP_ADDREQ_FLAG_ADD);
	}

	if (!HttpSendRequest(hReq, head, (int)strlen(head), (void *)data, data_len)) {
		errMsg->Init(Fmt("HttpSendRequest err %d\n", GetLastError()));
		goto END;
	}

	HttpQueryInfo(hReq, HTTP_QUERY_STATUS_CODE|HTTP_QUERY_FLAG_NUMBER, &code, &code_len, 0);
	Debug("status=%d\n", code);

	while (1) {
		DWORD	rsize = 0;
		if (buf.RemainSize() < CHUNK_SIZE) {
			if (!buf.Grow(CHUNK_SIZE)) {
				errMsg->Init(Fmt("vbuf grow err %d\n", GetLastError()));
				goto END;
			}
		}
		if (!InternetReadFile(hReq, buf.Buf() + buf.UsedSize(), (DWORD)buf.RemainSize(), &rsize)) {
			errMsg->Init(Fmt("InternetReadFile err %d\n", GetLastError()));
			goto END;
		}
		buf.AddUsedSize(rsize);
		if (rsize == 0) {
			break;
		}
	}

	reply->Init(buf.Buf(), buf.UsedSize()+1);
	reply->Buf()[buf.UsedSize()] = 0;
	reply->SetUsedSize(buf.UsedSize());

END:
	if (hReq) ::InternetCloseHandle(hReq);
	if (hConn) ::InternetCloseHandle(hConn);
	if (hInet) ::InternetCloseHandle(hInet);

	return	code;
}

static void TInetRequestProc(void *_irp)
{
	TInetReqParam	*irp = (TInetReqParam *)_irp;
	TInetReqReply	*irr = new TInetReqReply;

	irr->id = irp->id;
	irr->code = TInetRequest(irp->host.s(), irp->path.s(), irp->data.Buf(),
		(int)irp->data.UsedSize(), &irr->reply, &irr->errMsg, irp->flags);

	DebugU8("payload(%zd/%d) = %.*s err=%s\n", irp->data.UsedSize(), irr->code,
		irp->data.UsedSize(), irp->data.s(), irr->errMsg.s());

	if (irp->hWnd) {
		PostMessage(irp->hWnd, irp->uMsg, 0, (LPARAM)irr);
	}

	delete irp;
}

void TInetRequestAsync(LPCSTR host, LPCSTR path, BYTE *data, int data_len, HWND hWnd, UINT uMsg,
	int64 id, DWORD flags)
{
	auto irp = new TInetReqParam;

	irp->host  = host;
	irp->path  = path;
	irp->data.Init(data, data_len);
	irp->flags = flags;
	irp->hWnd  = hWnd;
	irp->uMsg  = uMsg;
	irp->id    = id;

	_beginthread(TInetRequestProc, 0, (void *)irp);
}

void TInetAsync(LPCSTR host, LPCSTR path, HWND hWnd, UINT uMsg, int64 id)
{
	TInetRequestAsync(host, path, NULL, 0, hWnd, uMsg, id,
		(IsWinVista() ? INETREQ_SECURE : 0)|INETREQ_GET); // XPはhttps://ipmsg.orgに接続できない
}

BOOL TInetCrackUrl(LPCSTR url, U8str *host, U8str *path, int *port, BOOL *is_sec)
{
	DynBuf	hbuf(MAX_HOSTBUF);
	DynBuf	ubuf(MAX_URLBUF);

	URL_COMPONENTS	uc = { sizeof(uc) };

	uc.lpszHostName = (char *)hbuf.Buf();
	uc.dwHostNameLength = (DWORD)hbuf.Size();

	uc.lpszUrlPath  = (char *)ubuf.Buf();
	uc.dwUrlPathLength = (DWORD)ubuf.Size();

	if (!::InternetCrackUrl(url, (DWORD)strlen(url), 0, &uc)) {
		Debug("InternetCrackUrl err(%d)\n", GetLastError());
		return FALSE;
	}
	if (uc.nScheme == INTERNET_SCHEME_HTTPS) {
		if (is_sec) *is_sec = TRUE;
	}
	else if (uc.nScheme != INTERNET_SCHEME_HTTP) {
		Debug("Scheme not http/https\n");
		return FALSE;
	}

	if (host) *host = uc.lpszHostName;
	if (path) *path = uc.lpszUrlPath;
	if (port) *port = uc.nPort;

	return TRUE;
}

static U8str TUserAgentStr  = "";
static U8str TAddHeadStr = "";

const char *TInetGetUserAgent()
{
	return	TUserAgentStr.s();
}

void TInetSetUserAgentRaw(const char *agent)
{
	TUserAgentStr = agent;
}

const char *TGetAddHeadStr()
{
	return	TAddHeadStr.s();
}

void TSetAddHeadStr(const char *head)
{
	TAddHeadStr = head;
}

void TInetSetUserAgent(const char *key)
{
	TInetSetUserAgentRaw(Fmt("%s (Windows NT %d.%d.%d%s)", key,
			TOSVerInfo.dwMajorVersion, TOSVerInfo.dwMinorVersion,
			TOSVerInfo.dwBuildNumber,
#ifdef _WIN64
			"; Win64"
#else
			TIsWow64() ? "; WOW64" : ""
#endif
		));
}

