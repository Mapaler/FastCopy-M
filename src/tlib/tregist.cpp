static char *tregist_id = 
	"@(#)Copyright (C) 1996-2011 H.Shirouzu		tregist.cpp	Ver0.99";
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Registry Class
	Create					: 1996-06-01(Sat)
	Update					: 2011-03-27(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include <stdio.h>
#include "tlib.h"

TRegistry::TRegistry(LPCSTR company, LPSTR appName, StrMode mode)
{
	openCnt = 0;
	strMode = mode;
	ChangeApp(company, appName);
}

TRegistry::TRegistry(LPCWSTR company, LPCWSTR appName, StrMode mode)
{
	openCnt = 0;
	strMode = mode;
	ChangeAppV(company, appName);
}

TRegistry::TRegistry(HKEY top_key, StrMode mode)
{
	topKey = top_key;
	strMode = mode;
	openCnt = 0;
}

TRegistry::~TRegistry(void)
{
	while (openCnt > 0) {
		CloseKey();
	}
}

BOOL TRegistry::ChangeApp(LPCSTR company, LPSTR appName)
{
	if (!IS_WINNT_V) return	ChangeAppV(company, appName);

	Wstr	company_w(company, strMode);
	Wstr	appName_w(appName, strMode);

	return	ChangeAppV(company_w, appName_w);
}

BOOL TRegistry::ChangeAppV(const void *company, const void *appName)
{
	while (openCnt > 0) {
		CloseKey();
	}

	topKey = HKEY_CURRENT_USER;

	WCHAR	wbuf[100];
	sprintfV(wbuf, IS_WINNT_V ? (void *)L"software\\%s" : (void *)"software\\%s", company);

	if (appName && GetChar(appName, 0)) {
		sprintfV(wbuf + strlenV(wbuf), L"\\%s", appName);
	}

	return	CreateKeyV(wbuf);
}

void TRegistry::ChangeTopKey(HKEY top_key)
{
	while (openCnt > 0)
		CloseKey();

	topKey = top_key;
}

BOOL TRegistry::OpenKey(LPCSTR subKey, BOOL createFlg)
{
	if (!IS_WINNT_V) return	OpenKeyV(subKey, createFlg);

	Wstr	subkey_w(subKey, strMode);
	return	OpenKeyV(subkey_w, createFlg);
}

BOOL TRegistry::OpenKeyV(const void *subKey, BOOL createFlg)
{
	HKEY	parentKey = (openCnt == 0 ? topKey : hKey[openCnt -1]);

	if (openCnt >= MAX_KEYARRAY) {
		return	FALSE;
	}
	DWORD	tmp;
	LONG	status;

	if (createFlg) {
		status = ::RegCreateKeyExV(parentKey, subKey, 0, NULL, REG_OPTION_NON_VOLATILE,
					KEY_ALL_ACCESS, NULL, &hKey[openCnt], &tmp);
	}
	else {
		if ((status = ::RegOpenKeyExV(parentKey, subKey, 0, KEY_ALL_ACCESS, &hKey[openCnt]))
				!= ERROR_SUCCESS)
			status = ::RegOpenKeyExV(parentKey, subKey, 0, KEY_READ, &hKey[openCnt]);
	}

	if (status == ERROR_SUCCESS)
		return	openCnt++, TRUE;
	else
		return	FALSE;
}

BOOL TRegistry::CloseKey(void)
{
	if (openCnt <= 0) {
		return	FALSE;
	}
	::RegCloseKey(hKey[--openCnt]);

	return	TRUE;
}

BOOL TRegistry::GetInt(LPCSTR subKey, int *val)
{
	return	GetLong(subKey, (long *)val);
}

BOOL TRegistry::GetIntV(const void *subKey, int *val)
{
	return	GetLongV(subKey, (long *)val);
}

BOOL TRegistry::SetInt(LPCSTR subKey, int val)
{
	return	SetLong(subKey, (long)val);
}

BOOL TRegistry::SetIntV(const void *subKey, int val)
{
	return	SetLongV(subKey, (long)val);
}

BOOL TRegistry::GetLong(LPCSTR subKey, long *val)
{
	if (!IS_WINNT_V) return	GetLongV(subKey, val);

	Wstr	subKey_w(subKey, strMode);
	return	GetLongV(subKey_w, val);
}

BOOL TRegistry::GetLongV(const void *subKey, long *val)
{
	DWORD	type = REG_DWORD, dw_size = sizeof(long);

	if (::RegQueryValueExV(hKey[openCnt -1], subKey, 0, &type, (BYTE *)val, &dw_size)
			== ERROR_SUCCESS) {
		return	TRUE;
	}
// 昔の互換性用
	WCHAR	wbuf[100];
	long	size_byte = sizeof(wbuf);

	if (::RegQueryValueV(hKey[openCnt -1], subKey, wbuf, &size_byte) != ERROR_SUCCESS)
		return	FALSE;
	*val = strtolV(wbuf, 0, 10);
	return	TRUE;
}

BOOL TRegistry::SetLong(LPCSTR subKey, long val)
{
	if (!IS_WINNT_V) return	SetLongV(subKey, val);

	Wstr	subKey_w(subKey, strMode);
	return	SetLongV(subKey_w, val);
}

BOOL TRegistry::SetLongV(const void *subKey, long val)
{
	return	::RegSetValueExV(hKey[openCnt -1], subKey, 0, REG_DWORD,
			(const BYTE *)&val, sizeof(val)) == ERROR_SUCCESS;
}

BOOL TRegistry::GetStr(LPCSTR subKey, LPSTR str, int size_byte)
{
	if (!IS_WINNT_V) return	GetStrV(subKey, str, size_byte);

	Wstr	subKey_w(subKey, strMode);
	Wstr	str_w(size_byte);

	if (!GetStrV(subKey_w, str_w.Buf(), size_byte)) {
		return	FALSE;
	}
	WtoS(str_w, str, size_byte, strMode);
	return	TRUE;
}

BOOL TRegistry::GetStrA(LPCSTR subKey, LPSTR str, int size)
{
	DWORD	type = REG_SZ;

	if (::RegQueryValueExA(hKey[openCnt -1], subKey, 0, &type, (BYTE *)str, (DWORD *)&size) != ERROR_SUCCESS
	&&  ::RegQueryValueA(hKey[openCnt -1], subKey, str, (LPLONG)&size) != ERROR_SUCCESS)
		return	FALSE;

	return	TRUE;
}

BOOL TRegistry::GetStrV(const void *subKey, void *str, int size_byte)
{
	DWORD	type = REG_SZ;

	if (::RegQueryValueExV(hKey[openCnt -1], subKey, 0, &type, (BYTE *)str,
			(DWORD *)&size_byte) != ERROR_SUCCESS
	&&  ::RegQueryValueV(hKey[openCnt -1], subKey, str, (LPLONG)&size_byte) != ERROR_SUCCESS)
		return	FALSE;

	return	TRUE;
}

BOOL TRegistry::SetStr(LPCSTR subKey, LPCSTR str)
{
	if (!IS_WINNT_V) return	SetStrV(subKey, str);

	Wstr	subKey_w(subKey, strMode), str_w(str, strMode);

	return	SetStrV(subKey_w, str_w);
}

BOOL TRegistry::SetStrA(LPCSTR subKey, LPCSTR str)
{
	return	::RegSetValueExA(hKey[openCnt -1], subKey, 0, REG_SZ, (const BYTE *)str, (DWORD)strlen(str) +1) == ERROR_SUCCESS;
}

BOOL TRegistry::SetStrV(const void *subKey, const void *str)
{
	return	::RegSetValueExV(hKey[openCnt -1], subKey, 0, REG_SZ, (const BYTE *)str,
			(strlenV(str) +1) * CHAR_LEN_V) == ERROR_SUCCESS;
}

BOOL TRegistry::GetByte(LPCSTR subKey, BYTE *data, int *size)
{
	if (!IS_WINNT_V) return	GetByteV(subKey, data, size);

	Wstr	subKey_w(subKey, strMode);

	return	GetByteV(subKey_w, data, size);
}

BOOL TRegistry::GetByteV(const void *subKey, BYTE *data, int *size)
{
	DWORD	type = REG_BINARY;

	return	::RegQueryValueExV(hKey[openCnt -1], subKey, 0, &type,
			(BYTE *)data, (LPDWORD)size) == ERROR_SUCCESS;
}

BOOL TRegistry::SetByte(LPCSTR subKey, const BYTE *data, int size)
{
	if (!IS_WINNT_V) return	SetByteV(subKey, data, size);

	Wstr	subKey_w(subKey, strMode);
	return	SetByteV(subKey_w, data, size);
}

BOOL TRegistry::SetByteV(const void *subKey, const BYTE *data, int size)
{
	return	::RegSetValueExV(hKey[openCnt -1], subKey, 0, REG_BINARY, data, size)
			== ERROR_SUCCESS;
}

BOOL TRegistry::DeleteKey(LPCSTR subKey)
{
	if (!IS_WINNT_V) return	DeleteKeyV(subKey);

	Wstr	subKey_w(subKey, strMode);
	return	DeleteKeyV(subKey_w);
}

BOOL TRegistry::DeleteKeyV(const void *subKey)
{
	return	::RegDeleteKeyV(hKey[openCnt -1], subKey) == ERROR_SUCCESS;
}

BOOL TRegistry::DeleteValue(LPCSTR subValue)
{
	if (!IS_WINNT_V) return	DeleteValueV(subValue);

	Wstr	subValue_w(subValue, strMode);
	return	DeleteValueV(subValue_w);
}

BOOL TRegistry::DeleteValueV(const void *subValue)
{
	return	::RegDeleteValueV(hKey[openCnt -1], subValue) == ERROR_SUCCESS;
}

BOOL TRegistry::EnumKey(DWORD cnt, LPSTR buf, int size)
{
	if (!IS_WINNT_V) return	EnumKeyV(cnt, buf, size);

	Wstr	buf_w(size);

	if (!EnumKeyV(cnt, buf_w.Buf(), size)) return FALSE;

	WtoS(buf_w, buf, size, strMode);
	return	TRUE;
}

BOOL TRegistry::EnumKeyV(DWORD cnt, void *buf, int size)
{
	return	::RegEnumKeyExV(hKey[openCnt -1], cnt, buf, (DWORD *)&size, 0, 0, 0, 0)
			== ERROR_SUCCESS;
}

BOOL TRegistry::EnumValue(DWORD cnt, LPSTR buf, int size, DWORD *type)
{
	if (!IS_WINNT_V) return	EnumValueV(cnt, buf, size, type);

	Wstr	buf_w(size);

	if (!EnumValueV(cnt, buf_w.Buf(), size, type)) return FALSE;

	WtoS(buf_w, buf, size, strMode);

	return	TRUE;
}

BOOL TRegistry::EnumValueV(DWORD cnt, void *buf, int size, DWORD *type)
{
	return	::RegEnumValueV(hKey[openCnt -1], cnt, buf, (DWORD *)&size, 0, type, 0, 0)
			== ERROR_SUCCESS;
}

/*
	subKey を指定した場合は subkey を含むキー以下を削除
	subkey が NULL の場合、カレント の配下を削除
*/
BOOL TRegistry::DeleteChildTree(LPCSTR subKey)
{
	if (!IS_WINNT_V) return	DeleteChildTreeV(subKey);

	Wstr	subKey_w(subKey, strMode);
	return	DeleteChildTreeV(subKey_w);
}

BOOL TRegistry::DeleteChildTreeV(const void *subKey)
{
	WCHAR	wbuf[256];
	BOOL	ret = TRUE;

	if (subKey && !OpenKeyV(subKey)) {
		return	FALSE;
	}

	while (EnumKeyV(0, wbuf, sizeof(wbuf) / CHAR_LEN_V))
	{
		if (!(ret = DeleteChildTreeV(wbuf)))
			break;
	}
	if (subKey)
	{
		CloseKey();
		ret = DeleteKeyV(subKey) ? ret : FALSE;
	}
	else {
		while (EnumValueV(0, wbuf, sizeof(wbuf) / CHAR_LEN_V))
		{
			if (!DeleteValueV(wbuf))
			{
				ret = FALSE;
				break;
			}
		}
	}
	return	ret;
}

