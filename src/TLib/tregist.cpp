static char *tregist_id = 
	"@(#)Copyright (C) 1996-2016 H.Shirouzu		tregist.cpp	Ver0.99";
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Registry Class
	Create					: 1996-06-01(Sat)
	Update					: 2015-06-22(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include <stdio.h>
#include "tlib.h"

TRegistry::TRegistry(LPCSTR company, LPSTR appName, StrMode mode)
{
	strMode = mode;
	ChangeApp(company, appName);
}

TRegistry::TRegistry(LPCWSTR company, LPCWSTR appName, StrMode mode)
{
	strMode = mode;
	ChangeAppW(company, appName);
}

TRegistry::TRegistry(HKEY top_key, StrMode mode)
{
	topKey = top_key;
	strMode = mode;
}

TRegistry::~TRegistry(void)
{
	while (openCnt > 0) {
		CloseKey();
	}
}

BOOL TRegistry::ChangeApp(LPCSTR company, LPSTR appName, BOOL openOnly)
{
	Wstr	company_w(company, strMode);
	Wstr	appName_w(appName, strMode);

	return	ChangeAppW(company_w.s(), appName_w.s(), openOnly);
}

BOOL TRegistry::ChangeAppW(const WCHAR *company, const WCHAR *appName, BOOL openOnly)
{
	while (openCnt > 0) {
		CloseKey();
	}

	topKey = HKEY_CURRENT_USER;

	WCHAR	wbuf[100];
	swprintf(wbuf, L"software\\%s", company);

	if (appName && *appName) {
		swprintf(wbuf + wcslen(wbuf), L"\\%s", appName);
	}

	return	openOnly ? OpenKeyW(wbuf) : CreateKeyW(wbuf);
}

void TRegistry::ChangeTopKey(HKEY top_key)
{
	while (openCnt > 0)
		CloseKey();

	topKey = top_key;
}

BOOL TRegistry::OpenKey(LPCSTR subKey, BOOL createFlg)
{
	Wstr	subkey_w(subKey, strMode);
	return	OpenKeyW(subKey ? subkey_w.s() : NULL, createFlg);
}

BOOL TRegistry::OpenKeyW(const WCHAR *subKey, BOOL createFlg)
{
	HKEY	parentKey = (openCnt == 0 ? topKey : hKey[openCnt -1]);

	if (openCnt >= MAX_KEYARRAY) {
		return	FALSE;
	}
	DWORD	tmp;
	LONG	status;

	if (createFlg) {
		status = ::RegCreateKeyExW(parentKey, subKey, 0, NULL, REG_OPTION_NON_VOLATILE,
				KEY_ALL_ACCESS|(keyForce64 ? KEY_WOW64_64KEY : 0), NULL, &hKey[openCnt], &tmp);
	}
	else {
		if ((status = ::RegOpenKeyExW(parentKey, subKey, 0,
			KEY_ALL_ACCESS|(keyForce64 ? KEY_WOW64_64KEY : 0), &hKey[openCnt])) != ERROR_SUCCESS)
			status = ::RegOpenKeyExW(parentKey, subKey, 0,
				KEY_READ|(keyForce64 ? KEY_WOW64_64KEY : 0), &hKey[openCnt]);
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

BOOL TRegistry::GetIntW(const WCHAR *subKey, int *val)
{
	return	GetLongW(subKey, (long *)val);
}

BOOL TRegistry::SetInt(LPCSTR subKey, int val)
{
	return	SetLong(subKey, (long)val);
}

BOOL TRegistry::SetIntW(const WCHAR *subKey, int val)
{
	return	SetLongW(subKey ? subKey : NULL, (long)val);
}

BOOL TRegistry::GetInt64(LPCSTR subKey, int64 *val)
{
	Wstr	subKey_w(subKey, strMode);

	return	GetInt64W(subKey ? subKey_w.s() : NULL, val);
}

BOOL TRegistry::GetInt64W(const WCHAR *subKey, int64 *val)
{
	DWORD	type = REG_QWORD;
	DWORD	dw_size = sizeof(int64);

	return	::RegQueryValueExW(hKey[openCnt -1], subKey, 0, &type, (BYTE *)val, &dw_size)
				== ERROR_SUCCESS;
}

BOOL TRegistry::SetInt64(LPCSTR subKey, int64 val)
{
	Wstr	subKey_w(subKey, strMode);

	return	SetInt64W(subKey ? subKey_w.s() : NULL, val);
}

BOOL TRegistry::SetInt64W(const WCHAR *subKey, int64 val)
{
	return	::RegSetValueExW(hKey[openCnt -1], subKey, 0, REG_QWORD,
			(const BYTE *)&val, sizeof(val)) == ERROR_SUCCESS;
}

BOOL TRegistry::GetLong(LPCSTR subKey, long *val)
{
	Wstr	subKey_w(subKey, strMode);
	return	GetLongW(subKey ? subKey_w.s() : NULL, val);
}

BOOL TRegistry::GetLongW(const WCHAR *subKey, long *val)
{
	DWORD	type = REG_DWORD, dw_size = sizeof(long);

	if (::RegQueryValueExW(hKey[openCnt -1], subKey, 0, &type, (BYTE *)val, &dw_size)
			== ERROR_SUCCESS) {
		return	TRUE;
	}
// 昔の互換性用
	WCHAR	wbuf[100];
	long	size_byte = sizeof(wbuf);

	if (::RegQueryValueW(hKey[openCnt -1], subKey, wbuf, &size_byte) != ERROR_SUCCESS)
		return	FALSE;
	*val = wcstol(wbuf, 0, 10);
	return	TRUE;
}

BOOL TRegistry::SetLong(LPCSTR subKey, long val)
{
	Wstr	subKey_w(subKey, strMode);
	return	SetLongW(subKey ? subKey_w.s() : NULL, val);
}

BOOL TRegistry::SetLongW(const WCHAR *subKey, long val)
{
	return	::RegSetValueExW(hKey[openCnt -1], subKey, 0, REG_DWORD,
			(const BYTE *)&val, sizeof(val)) == ERROR_SUCCESS;
}

BOOL TRegistry::GetStr(LPCSTR subKey, LPSTR str, int size_byte)
{
	Wstr	subKey_w(subKey, strMode);
	Wstr	str_w(size_byte);

	if (!GetStrW(subKey ? subKey_w.s() : NULL, str_w.Buf(), size_byte * 2)) {
		return	FALSE;
	}
	WtoS(str_w.s(), str, size_byte, strMode);
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

BOOL TRegistry::GetStrW(const WCHAR *subKey, WCHAR *str, int size_byte)
{
	DWORD	type = REG_SZ;

	if (::RegQueryValueExW(hKey[openCnt -1], subKey, 0, &type, (BYTE *)str,
			(DWORD *)&size_byte) != ERROR_SUCCESS
	&&  ::RegQueryValueW(hKey[openCnt -1], subKey, str, (LPLONG)&size_byte) != ERROR_SUCCESS)
		return	FALSE;

	return	TRUE;
}

BOOL TRegistry::GetStrMW(const char *subKey, WCHAR *str, int size_byte)
{
	Wstr	subKey_w(subKey, strMode);

	return	GetStrW(subKey ? subKey_w.s() : NULL, str, size_byte);
}

BOOL TRegistry::SetStr(LPCSTR subKey, LPCSTR str)
{
	Wstr	subKey_w(subKey, strMode), str_w(str, strMode);

	return	SetStrW(subKey ? subKey_w.s() : NULL, str_w.s());
}

BOOL TRegistry::SetStrA(LPCSTR subKey, LPCSTR str)
{
	return	::RegSetValueExA(hKey[openCnt -1], subKey, 0, REG_SZ, (const BYTE *)str, (DWORD)strlen(str) +1) == ERROR_SUCCESS;
}

BOOL TRegistry::SetStrW(const WCHAR *subKey, const WCHAR *str)
{
	return	::RegSetValueExW(hKey[openCnt -1], subKey, 0, REG_SZ, (const BYTE *)str,
			(DWORD((wcslen(str) +1) * sizeof(WCHAR)))) == ERROR_SUCCESS;
}

BOOL TRegistry::SetStrMW(const char *subKey, const WCHAR *str)
{
	Wstr	subKey_w(subKey, strMode);

	return	SetStrW(subKey ? subKey_w.s() : NULL, str);
}

BOOL TRegistry::GetByte(LPCSTR subKey, BYTE *data, int *size)
{
	Wstr	subKey_w(subKey, strMode);

	return	GetByteW(subKey ? subKey_w.s() : NULL, data, size);
}

BOOL TRegistry::GetByteW(const WCHAR *subKey, BYTE *data, int *size)
{
	DWORD	type = REG_BINARY;

	return	::RegQueryValueExW(hKey[openCnt -1], subKey, 0, &type,
			(BYTE *)data, (LPDWORD)size) == ERROR_SUCCESS;
}

BOOL TRegistry::SetByte(LPCSTR subKey, const BYTE *data, int size)
{
	Wstr	subKey_w(subKey, strMode);
	return	SetByteW(subKey ? subKey_w.s() : NULL, data, size);
}

BOOL TRegistry::SetByteW(const WCHAR *subKey, const BYTE *data, int size)
{
	return	::RegSetValueExW(hKey[openCnt -1], subKey, 0, REG_BINARY, data, size)
			== ERROR_SUCCESS;
}

BOOL TRegistry::DeleteKey(LPCSTR subKey)
{
	Wstr	subKey_w(subKey, strMode);
	return	DeleteKeyW(subKey ? subKey_w.s() : NULL);
}

BOOL TRegistry::DeleteKeyW(const WCHAR *subKey)
{
	return	::RegDeleteKeyW(hKey[openCnt -1], subKey) == ERROR_SUCCESS;
}

BOOL TRegistry::DeleteValue(LPCSTR subValue)
{
	Wstr	subValue_w(subValue, strMode);
	return	DeleteValueW(subValue ? subValue_w.s() : NULL);
}

BOOL TRegistry::DeleteValueW(const WCHAR *subValue)
{
	return	::RegDeleteValueW(hKey[openCnt -1], subValue) == ERROR_SUCCESS;
}

BOOL TRegistry::EnumKey(DWORD cnt, LPSTR buf, int size)
{
	Wstr	buf_w(size);

	if (!EnumKeyW(cnt, buf_w.Buf(), size)) return FALSE;

	WtoS(buf_w.s(), buf, size, strMode);
	return	TRUE;
}

BOOL TRegistry::EnumKeyW(DWORD cnt, WCHAR *buf, int size)
{
	return	::RegEnumKeyExW(hKey[openCnt -1], cnt, buf, (DWORD *)&size, 0, 0, 0, 0)
			== ERROR_SUCCESS;
}

BOOL TRegistry::EnumValue(DWORD cnt, LPSTR buf, int size, DWORD *type)
{
	Wstr	buf_w(size);

	if (!EnumValueW(cnt, buf_w.Buf(), size, type)) return FALSE;

	WtoS(buf_w.s(), buf, size, strMode);

	return	TRUE;
}

BOOL TRegistry::EnumValueW(DWORD cnt, WCHAR *buf, int size, DWORD *type)
{
	return	::RegEnumValueW(hKey[openCnt -1], cnt, buf, (DWORD *)&size, 0, type, 0, 0)
			== ERROR_SUCCESS;
}

/*
	subKey を指定した場合は subkey を含むキー以下を削除
	subkey が NULL の場合、カレント の配下を削除
*/
BOOL TRegistry::DeleteChildTree(LPCSTR subKey)
{
	Wstr	subKey_w(subKey, strMode);
	return	DeleteChildTreeW(subKey ? subKey_w.s() : NULL);
}

BOOL TRegistry::DeleteChildTreeW(const WCHAR *subKey)
{
	WCHAR	wbuf[256];
	BOOL	ret = TRUE;

	if (subKey && !OpenKeyW(subKey)) {
		return	FALSE;
	}

	while (EnumKeyW(0, wbuf, wsizeof(wbuf)))
	{
		if (!(ret = DeleteChildTreeW(wbuf)))
			break;
	}
	if (subKey)
	{
		CloseKey();
		ret = DeleteKeyW(subKey) ? ret : FALSE;
	}
	else {
		while (EnumValueW(0, wbuf, wsizeof(wbuf)))
		{
			if (!DeleteValueW(wbuf))
			{
				ret = FALSE;
				break;
			}
		}
	}
	return	ret;
}

