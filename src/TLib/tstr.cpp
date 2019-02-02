static char *tstr_id = 
	"@(#)Copyright (C) 2018 H.Shirouzu		tstr.cpp	Ver0.99";
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Application Frame Class
	Update					: 2018-06-12(Tue)
	Update					: 2018-06-12(Tue)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "tlib.h"

/*=========================================================================
	パス合成（ANSI 版）
=========================================================================*/
int MakePath(char *dest, const char *dir, const char *file, int max_len)
{
	if (!dir) {
		dir = dest;
	}

	int	len;
	if (dest == dir) {
		len = (int)strlen(dir);
	} else {
		len = strcpyz(dest, dir);
	}

	if (len > 0) {
		bool	need_sep = (dest[len -1] != '\\');

		if (len >= 2 && !need_sep) {	// 表などで終端の場合は sep必要
			BYTE	*p = (BYTE *)dest;
			while (*p) {
				if (IsDBCSLeadByte(*p) && *(p+1)) {
					p += 2;
					if (!*p) {
						need_sep = true;
					}
				} else {
					p++;
				}
			}
		}
		if (need_sep) {
			dest[len++] = '\\';
		}
	}
	return	len + strncpyz(dest + len, file, max_len - len);
}

/*=========================================================================
	パス合成（UTF-8 版）
=========================================================================*/
int MakePathU8(char *dest, const char *dir, const char *file, int max_len)
{
	if (!dir) {
		dir = dest;
	}

	int	len;

	if (dest == dir) {
		len = (int)strlen(dir);
	} else {
		len = strcpyz(dest, dir);
	}
	if (len > 0 && dest[len -1] != '\\') {
		dest[len++] = '\\';
	}
	return	len + strncpyz(dest + len, file, max_len - len);
}

/*=========================================================================
	パス合成（UNICODE 版）
=========================================================================*/
int MakePathW(WCHAR *dest, const WCHAR *dir, const WCHAR *file, int max_len)
{
	if (!dir) {
		dir = dest;
	}

	int	len;

	if (dest == dir) {
		len = (int)wcslen(dir);
	} else {
		len = wcscpyz(dest, dir);
	}
	if (len > 0 && dest[len -1] != '\\') {
		dest[len++] = '\\';
	}
	return	len + wcsncpyz(dest + len, file, max_len - len);
}


/*=========================================================================
	bin <-> hex
=========================================================================*/
static char  *hexstr   =  "0123456789abcdef";
static WCHAR *hexstr_w = L"0123456789abcdef";

inline u_char hexchar2char(u_char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'a' && ch <= 'z')
		return ch - 'a' + 10;
	if (ch >= 'A' && ch <= 'Z')
		return ch - 'A' + 10;
	return 0xff;
}

size_t hexstr2bin(const char *buf, BYTE *bindata, size_t maxlen)
{
	size_t	len = 0;

	for ( ; buf[0] && buf[1] && len < maxlen; buf+=2, len++)
	{
		u_char c1 = hexchar2char(buf[0]);
		u_char c2 = hexchar2char(buf[1]);
		if (c1 == 0xff || c2 == 0xff) break;
		bindata[len] = (c1 << 4) | c2;
	}
	return	len;
}

int bin2hexstr(const BYTE *bindata, size_t len, char *buf)
{
	for (const BYTE *end=bindata+len; bindata < end; bindata++)
	{
		*buf++ = hexstr[*bindata >> 4];
		*buf++ = hexstr[*bindata & 0x0f];
	}
	*buf = 0;
	return	int(len * 2);
}

int bin2hexstrW(const BYTE *bindata, size_t len, WCHAR *buf)
{
	for (const BYTE *end=bindata+len; bindata < end; bindata++)
	{
		*buf++ = hexstr_w[*bindata >> 4];
		*buf++ = hexstr_w[*bindata & 0x0f];
	}
	*buf = 0;
	return	int(len * 2);
}

/* little-endian binary to hexstr */
int bin2hexstr_revendian(const BYTE *bindata, size_t len, char *buf)
{
	size_t	sv_len = len;
	while (len-- > 0)
	{
		*buf++ = hexstr[bindata[len] >> 4];
		*buf++ = hexstr[bindata[len] & 0x0f];
	}
	*buf = 0;
	return	int(sv_len * 2);
}

size_t hexstr2bin_revendian(const char *s, BYTE *bindata, size_t maxlen)
{
	return	hexstr2bin_revendian_ex(s, bindata, maxlen);
}

size_t hexstr2bin_revendian_ex(const char *s, BYTE *bindata, size_t maxlen, int slen)
{
	size_t	len = 0;

	if (slen == -1) {
		slen = (int)strlen(s);
	}

	for ( ; slen >= 1 && len < maxlen; slen-=2, len++) {
		u_char c1 = hexchar2char(s[slen-1]);
		u_char c2 = (slen >= 2) ? hexchar2char(s[slen-2]) : 0;
		if (c1 == 0xff || c2 == 0xff) {
			return FALSE;
		}
		bindata[len] = c1 | (c2 << 4);
	}
	return	len;
}

BYTE hexstr2byte(const char *buf)
{
	BYTE	val = 0;

	hexstr2bin_revendian(buf, (BYTE *)&val, sizeof(val));

	return	val;
}

WORD hexstr2word(const char *buf)
{
	WORD	val = 0;

	hexstr2bin_revendian(buf, (BYTE *)&val, sizeof(val));

	return	val;
}

DWORD hexstr2dword(const char *buf)
{
	DWORD	val = 0;

	hexstr2bin_revendian(buf, (BYTE *)&val, sizeof(val));

	return	val;
}

int64 hexstr2int64(const char *buf)
{
	int64	val = 0;

	hexstr2bin_revendian(buf, (BYTE *)&val, sizeof(val));

	return	val;
}

int strip_crlf(const char *s, char *d)
{
	char	*sv = d;

	while (*s) {
		char	c = *s++;
		if (c != '\r' && c != '\n') *d++ = c;
	}
	*d = 0;
	return	(int)(d - sv);
}

/* base64 convert routine */
size_t b64str2bin(const char *s, BYTE *bindata, size_t maxsize)
{
	DWORD	size = (DWORD)maxsize;
	BOOL ret = ::CryptStringToBinary(s, 0, CRYPT_STRING_BASE64, bindata, &size, 0, 0);
	return	ret ? size : 0;
}

size_t b64str2bin_ex(const char *s, int len, BYTE *bindata, size_t maxsize)
{
	DWORD	size = (DWORD)maxsize;
	BOOL ret = ::CryptStringToBinary(s, len, CRYPT_STRING_BASE64, bindata, &size, 0, 0);
	return	ret ? size : 0;
}

int bin2b64str(const BYTE *bindata, size_t size, char *str)
{
	DWORD	len  = (DWORD)size * 2 + 5;
	char	*b64 = new char [len];

	if (!::CryptBinaryToString(bindata, (DWORD)size, CRYPT_STRING_BASE64, b64, &len)) {
		return 0;
	}
	len = strip_crlf(b64, str);

	delete [] b64;
	return	len;
}

size_t b64str2bin_revendian(const char *s, BYTE *bindata, size_t maxsize)
{
	size_t	size = b64str2bin(s, bindata, maxsize);
	rev_order(bindata, size);
	return	size;
}

int bin2b64str_revendian(const BYTE *bindata, size_t size, char *buf)
{
	BYTE *rev = new BYTE [size];

	if (!rev) return -1;

	rev_order(bindata, rev, size);
	int	ret = bin2b64str(rev, size, buf);
	delete [] rev;

	return	ret;
}

int bin2urlstr(const BYTE *bindata, size_t size, char *str)
{
	int ret = bin2b64str(bindata, size, str);

	// strip \r\n
	for (char *s=str, *d=str; *d = *s; s++) {
		if (*s != '\r' && *s != '\n') {
			d++;
			ret--;
		}
	}

	// replace char
	for (char *s=str; *s; s++) {
		switch (*s) {
		case '+':
			*s = '-';
			break;

		case '/':
			*s = '_';
			break;

		case '=':
			*s = 0;
			ret--;
			break;
		}
	}
	return	ret;
}

/*
0: 0
1: 2+2
2: 3+1
3: 4
4  6+2
*/

size_t urlstr2bin(const char *str, BYTE *bindata, size_t maxsize)
{
	int		len = (int)strlen(str);
	char	*b64 = new char [len + 4];

	strcpy(b64, str);
	for (char *s=b64; *s; s++) {
		switch (*s) {
		case '-': *s = '+'; break;
		case '_': *s = '/'; break;
		}
	}
	if (b64[len-1] != '\n' && (len % 4) && b64[len-1] != '=') {
		sprintf(b64 + len, "%.*s", int(4 - (len % 4)), "===");
	}

	size_t	size = b64str2bin(b64, bindata, maxsize);
	delete [] b64;

	return	size;
}

void swap_s(BYTE *s, size_t len)
{
	if (len == 0) return;

	size_t	mid = len / 2;

	for (size_t i=0, e=len-1; i < mid; i++, e--) {
		BYTE	&c1 = s[i];
		BYTE	&c2 = s[e];
		BYTE	tmp = c1;
		c1 = c2;
		c2 = tmp;
	}
}

/*
	16進 -> long long
*/
int64 hex2ll(char *buf)
{
	int64	ret = 0;

	for ( ; *buf; buf++)
	{
		if (*buf >= '0' && *buf <= '9')
			ret = (ret << 4) | (*buf - '0');
		else if (toupper(*buf) >= 'A' && toupper(*buf) <= 'F')
			ret = (ret << 4) | (toupper(*buf) - 'A' + 10);
		else continue;
	}
	return	ret;
}

void rev_order(BYTE *data, size_t size)
{
	BYTE	*d1 = data;
	BYTE	*d2 = data + size - 1;

	for (BYTE *end = d1 + (size/2); d1 < end; ) {
		BYTE	sv = *d1;
		*d1++ = *d2;
		*d2-- = sv;
	}
}

void rev_order(const BYTE *src, BYTE *dst, size_t size)
{
	dst = dst + size - 1;

	for (const BYTE *end = src + size; src < end; ) {
		*dst-- = *src++;
	}
}

void byte_xor(const BYTE *in1, const BYTE *in2, BYTE *out, size_t len)
{
	while (len-- > 0) {
		*out++ = *in1++ ^ *in2++;
	}
}

int snprintfz(char *buf, int size, const char *fmt,...)
{
	va_list	ap;
	va_start(ap, fmt);
	int len = vsnprintfz(buf, size, fmt, ap);
	va_end(ap);

	return	len;
}

int vsnprintfz(char *buf, int size, const char *fmt, va_list ap)
{
	if (!buf) {
		return	(int)vsnprintf(NULL, size, fmt, ap);
	}

	if (size <= 0) return 0;

	int len = (int)vsnprintf(buf, size, fmt, ap);

	if (len <= 0 || len >= size) {
		buf[size -1] = 0;
		len = (int)strlen(buf);
	}

	va_end(ap);

	return	len;
}

int snwprintfz(WCHAR *buf, int wsize, const WCHAR *fmt,...)
{
	va_list	ap;
	va_start(ap, fmt);
	int len = vsnwprintfz(buf, wsize, fmt, ap);
	va_end(ap);

	return	len;
}

int vsnwprintfz(WCHAR *buf, int wsize, const WCHAR *fmt, va_list ap)
{
	if (!buf) {
		return	(int)vswprintf(NULL, wsize, fmt, ap);
	}
	if (wsize <= 0) return 0;

	int len = (int)vswprintf(buf, wsize, fmt, ap);

	if (len <= 0 || len >= wsize) {
		buf[wsize -1] = 0;
		len = (int)wcslen(buf);
	}

	va_end(ap);

	return	len;
}

/*
	nul文字を必ず付与する strcpy かつ return は 0 を除くコピー文字数
*/
int strcpyz(char *dest, const char *src)
{
	char	*sv_dest = dest;

	while (*src) {
		*dest++ = *src++;
	}
	*dest = 0;
	return	(int)(dest - sv_dest);
}

int wcscpyz(WCHAR *dest, const WCHAR *src)
{
	WCHAR	*sv_dest = dest;

	while (*src) {
		*dest++ = *src++;
	}
	*dest = 0;
	return	(int)(dest - sv_dest);
}

/*
	nul文字を必ず付与する strncpy かつ return は 0 を除くコピー文字数
*/
int strncpyz(char *dest, const char *src, int num)
{
	char	*sv_dest = dest;

	if (num <= 0) return 0;

	while (--num > 0 && *src) {
		*dest++ = *src++;
	}
	*dest = 0;
	return	(int)(dest - sv_dest);
}

int strncatz(char *dest, const char *src, int num)
{
	for ( ; *dest; dest++, num--)
		;
	return strncpyz(dest, src, num);
}

const char *strnchr(const char *s, char ch, int num)
{
	const char *end = s + num;

	for ( ; s < end && *s; s++) {
		if (*s == ch) return s;
	}
	return	NULL;
}

int wcsncpyz(WCHAR *dest, const WCHAR *src, int num)
{
	WCHAR	*sv_dest = dest;

	if (num <= 0) return 0;

	while (--num > 0 && *src) {
		*dest++ = *src++;
	}

	if (IsSurrogateL(dest[-1])) { // surrogate includes IVS
		dest--;
	}
	*dest = 0;

	return	(int)(dest - sv_dest);
}

int wcsncatz(WCHAR *dest, const WCHAR *src, int num)
{
	for ( ; *dest; dest++, num--)
		;
	return wcsncpyz(dest, src, num);
}

const WCHAR *wcsnchr(const WCHAR *dest, WCHAR ch, int num)
{
	for ( ; num > 0 && *dest; num--, dest++) {
		if (*dest == ch) {
			return	dest;
		}
	}
	return	NULL;
}

char *strdupNew(const char *_s, int max_len)
{
	int		len = int((max_len == -1) ? strlen(_s) : strnlen(_s, max_len));
	char	*s = new char [len + 1];
	memcpy(s, _s, len);
	s[len] = 0;
	return	s;
}

WCHAR *wcsdupNew(const WCHAR *_s, int max_len)
{
	int		len = int((max_len == -1) ? wcslen(_s) : wcsnlen(_s, max_len));
	WCHAR	*s = new WCHAR [len + 1];
	memcpy(s, _s, len * sizeof(WCHAR));
	s[len] = 0;

	if (len > 0) {
		if (IsSurrogateL(s[len-1])) { // surrogate includes IVS
			s[len-1] = 0;
		}
	}

	return	s;
}

int ReplaceCharW(WCHAR *s, WCHAR rep_in, WCHAR rep_out, int max_len)
{
	WCHAR	*p = s;
	WCHAR	*end = s + max_len;

	for ( ; p < end && *p; p++) {
		if (*p == rep_in) {
			*p = rep_out;
		}
	}
	return	int(p - s);
}


/*=========================================================================
	拡張 strtok()
		"" に出くわすと、"" の中身を取り出す
		token の前後に空白があれば取り除く
		それ以外は、strtok_r() と同じ
=========================================================================*/
WCHAR *strtok_pathW(WCHAR *str, const WCHAR *sep, WCHAR **p, BOOL remove_quote)
{
	const WCHAR	*quote=L"\"";
	const WCHAR	*org_sep = sep;

	if (str)
		*p = str;
	else
		str = *p;

	if (!*p)
		return	NULL;

	// 頭だし
	while (str[0] && (wcschr(sep, str[0]) || str[0] == ' '))
		str++;
	if (str[0] == 0)
		return	NULL;

	// 終端検出
	WCHAR	*in = str, *out = str;
	for ( ; in[0]; in++) {
		BOOL	is_set = FALSE;

		if (sep == org_sep) {	// 通常 mode
			if (wcschr(sep, in[0])) {
				break;
			}
			else if (in[0] == '"') {
				if (!remove_quote) {
					is_set = TRUE;
				}
				sep = quote;	// quote mode に遷移
			}
			else {
				is_set = TRUE;
			}
		}
		else {					// quote mode
			if (in[0] == '"') {
				sep = org_sep;	// 通常 mode に遷移
				if (!remove_quote) {
					is_set = TRUE;
				}
			}
			else {
				is_set = TRUE;
			}
		}
		if (is_set) {
			out[0] = in[0];
			out++;
		}
	}
	*p = in[0] ? in+1 : NULL;
	out[0] = 0;

	// 末尾の空白を取り除く
	for (out--; out >= str && out[0] == ' '; out--)
		out[0] = 0;

	return	str;
}

/*=========================================================================
	コマンドライン解析（CommandLineToArgvW API の ANSI版）
		CommandLineToArgvW() と同じく、返り値の開放は呼び元ですること
=========================================================================*/
WCHAR **CommandLineToArgvExW(WCHAR *cmdLine, int *_argc)
{
#define MAX_ARG_ALLOC	16
	int&	argc = *_argc;
	WCHAR	**argv = NULL;
	WCHAR	*p = NULL;
	WCHAR	*separantor = L" \t";

	argc = 0;
	while (1) {
		if ((argc % MAX_ARG_ALLOC) == 0) {
			argv = (WCHAR **)realloc(argv, (argc + MAX_ARG_ALLOC) * sizeof(WCHAR *));
		}
		if ((argv[argc] = strtok_pathW(argc ? NULL : cmdLine, separantor, &p)) == NULL) {
			break;
		}
		argc++;
	}

	return	argv;
}

