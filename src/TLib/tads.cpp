static char *tads_id = 
	"@(#)Copyright (C) 2015-2016 H.Shirouzu		tads.cpp	Ver0.99";
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Application Frame Class
	Create					: 1996-06-01(Sat)
	Update					: 2015-11-01(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "tlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <intrin.h>

/* =======================================================================
	Active Directory/Domain Functions
 ======================================================================= */
#include <Iads.h>
#include <Adshlp.h>
#include <Ntsecapi.h>

#pragma comment (lib, "ADSIid.lib")
#pragma comment (lib, "Activeds.lib")

BOOL IsDomainEnviron()
{
	LSA_OBJECT_ATTRIBUTES	attr={};
	LSA_HANDLE				ph;
	BOOL					ret = FALSE;

	if (::LsaOpenPolicy(NULL, &attr, POLICY_VIEW_LOCAL_INFORMATION, &ph)
		!= 0) {
		return	FALSE;
	}

	PPOLICY_PRIMARY_DOMAIN_INFO	ppdi = NULL;

	if (::LsaQueryInformationPolicy(ph, PolicyPrimaryDomainInformation, (void **)&ppdi) == 0) {
		if (ppdi->Sid) {	// ppdi->Name is domain name (LSA_UNICODE_STRING)
			ret = TRUE;
		}
		::LsaFreeMemory(ppdi);
	}
	::LsaClose(ph);

	return ret;
}

BOOL GetDomainAndUid(WCHAR *domain, WCHAR *uid)
{
	HANDLE		hToken;
	BYTE		info[1024];
	DWORD		infoSize = sizeof(info);
	TOKEN_USER	*tu = (TOKEN_USER *)info;
	DWORD		uidSize    = 200;	// KB 111544
	DWORD		domainSize = 200;	// KB 111544
	SID_NAME_USE snu;

	if (!::OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
		return	FALSE;
	}
	BOOL ret = ::GetTokenInformation(hToken,TokenUser, info, infoSize, &infoSize);
	::CloseHandle(hToken);

	if (!ret) {
		return	FALSE;
	}

	return	::LookupAccountSidW(NULL, tu->User.Sid, uid, &uidSize, domain, &domainSize, &snu);
}

BOOL GetDomainFullName(const WCHAR *domain, const WCHAR *uid, WCHAR *full_name)
{
	WCHAR	nt_domain[256];

	swprintf(nt_domain, L"WinNT://%s", domain);

	IADsContainer*	ads = NULL;
	if (::ADsGetObject(nt_domain, IID_IADsContainer, (void**)&ads) < S_OK) {
		return	FALSE;
	}

	BOOL		ret = FALSE;
	BSTR		buid = ::SysAllocString(uid);
	IDispatch	*udis = NULL;

	if (ads->GetObject(L"User", buid, &udis) >= S_OK) {
		IADsUser	*user = NULL;

		if (udis->QueryInterface(IID_IADsUser, (void**)&user) >= S_OK) {
			BSTR	bstr = NULL;

			if (user->get_FullName(&bstr) >= S_OK) {
				wcscpy(full_name, bstr);
				::SysFreeString(bstr);
				ret = TRUE;
			}
			user->Release();
		}
		udis->Release();
	}
	ads->Release();
	::SysFreeString(buid);

	return	ret;
}

BOOL IsAsciiStr(const WCHAR *s)
{
	for ( ; *s; s++) {
		if (*s >= 128) {
			return	FALSE;
		}
	}
	return	TRUE;
}

BOOL GetDomainGroup(const WCHAR *domain, const WCHAR *uid, WCHAR *group)
{
	WCHAR	nt_domain[256];

	swprintf(nt_domain, L"WinNT://%s", domain);

	IADsContainer*	ads = NULL;
	if (::ADsGetObject(nt_domain, IID_IADsContainer, (void**)&ads) < S_OK) {
		return	FALSE;
	}

	BOOL		ret = FALSE;
	BSTR		buid = ::SysAllocString(uid);
	IDispatch	*udis = NULL;

	if (ads->GetObject(L"User", buid, &udis) >= S_OK) {
		IADsUser	*user = NULL;

		if (udis->QueryInterface(IID_IADsUser, (void**)&user) >= S_OK) {
			IADsMembers	*grps = NULL;

			if (user->Groups(&grps) >= S_OK) {
				IUnknown *pUnk = NULL;

				if (grps->get__NewEnum(&pUnk) >= S_OK) {
					IEnumVARIANT	*pEnum = NULL;

					if (pUnk->QueryInterface(IID_IEnumVARIANT, (void **)&pEnum) >= S_OK) {
						VARIANT	var;
						ULONG	lFetch = 0;
						::VariantInit(&var);

						while (!ret && pEnum->Next(1, &var, &lFetch) == S_OK && lFetch == 1) {
							IDispatch	*pDisp = V_DISPATCH(&var);
							IADs		*grp = NULL;

							if (pDisp->QueryInterface(IID_IADs, (void **)&grp) >= S_OK) {
								BSTR	bstr = NULL;

								if (grp->get_Name(&bstr) >= S_OK) {
									if (!IsAsciiStr(bstr)) {
										wcscpy(group, bstr);
										ret = TRUE;
									}
									::SysFreeString(bstr);
								}
								grp->Release();
							}
							::VariantClear(&var);
							lFetch = 0;
						}
						pEnum->Release();
					}
					pUnk->Release();
				}
				grps->Release();
			}
			user->Release();
		}
		udis->Release();
	}
	ads->Release();
	::SysFreeString(buid);

	return	ret;
}

