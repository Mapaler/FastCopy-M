/* static char *shellext_id = 
	"@(#)Copyright (C) 2005-2016 H.Shirouzu		shellext.h	Ver3.26"; */
/* ========================================================================
	Project  Name			: Fast/Force copy file and directory
	Create					: 2005-01-23(Sun)
	Update					: 2016-12-08(Thu)
	Copyright				: H.Shirouzu
	License					: GNU General Public License version 3
	======================================================================== */

#ifndef SHELLEXT_H
#define SHELLEXT_H

#ifndef CFSTR_PREFERREDDROPEFFECT
#define CFSTR_PREFERREDDROPEFFECT "Preferred DropEffect"
#endif

#define REG_SHELL_APPROVED	\
	"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved"

// Current CLSID for all file(global)	// {72FF462B-AB7D-427a-A268-E22E414933D7}
DEFINE_GUID(CLSID_FcShExt,     0x72ff462b, 0xab7d, 0x427a, 0xa2, 0x68, 0xe2, \
	0x2e, 0x41, 0x49, 0x33, 0xd7);
// Current CLSID for .lnk file(global)	{19FD02CE-C890-4c90-A591-CEF8BE893DFC}
DEFINE_GUID(CLSID_FcLnkExt, 0x19fd02ce, 0xc890, 0x4c90, 0xa5, 0x91, 0xce, \
	0xf8, 0xbe, 0x89, 0x3d, 0xfc);

// Current CLSID for all file(local)	// {CF6181BA-D469-441A-BE79-BB70A2EC3D0B}
DEFINE_GUID(CLSID_FcShExtLocal, \
	0xcf6181ba, 0xd469, 0x441a, 0xbe, 0x79, 0xbb, 0x70, 0xa2, 0xec, 0x3d, 0xb);

// Current CLSID for .lnk file(local)	// {5C187118-49D3-44B0-A929-AFC31BB75BA3}
DEFINE_GUID(CLSID_FcLnkExtLocal, \
	0x5c187118, 0x49d3, 0x44b0, 0xa9, 0x29, 0xaf, 0xc3, 0x1b, 0xb7, 0x5b, 0xa3);

//#define CLSID_FcShExt CLSID_FcShExtLocal
//#define CLSID_FcLnkExt CLSID_FcLnkExtLocal

BOOL WINAPI SetMenuFlags(BOOL isAdmin, int flags);
int WINAPI GetMenuFlags(BOOL isAdmin);
BOOL WINAPI IsRegistServer(BOOL isAdmin);
BOOL WINAPI SetAdminMode(BOOL isAdmin);

class PathArray {
protected:
	int		num;
	int		totalLen;
	WCHAR	**pathArray;

public:
	PathArray(void);
	~PathArray();
	void	Init(void);
	const WCHAR	*Path(int idx) const { return idx < num ? pathArray[idx] : NULL; }
	int		GetMultiPath(WCHAR *multi_path, int max_len);
	int		GetMultiPathLen();
	int		Num(void) const { return	num; }
	BOOL	RegisterPath(const WCHAR *path);
	PathArray &operator =(const PathArray &src) {
		Init();
		for(int i=0; i < src.Num(); i++) {
			RegisterPath(src.Path(i));
		}
		return *this;
	}
};

class ShellExt : public IContextMenu, IShellExtInit
{
protected:
	ULONG		refCnt;
	IDataObject	*dataObj;
	PathArray	srcArray;
	PathArray	dstArray;
	PathArray	clipArray;
	BOOL		isCut;
	BOOL		isAdmin;

public:
	ShellExt(BOOL _isAdmin);
	~ShellExt();

	BOOL	DeleteMenu(UINT *del_idx);
	BOOL	GetClipBoardInfo(PathArray *pathArray, BOOL *is_cut);
	BOOL	IsDir(const WCHAR *path, BOOL is_resolve);

	STDMETHODIMP Initialize(LPCITEMIDLIST pIDFolder, IDataObject *pDataObj, HKEY hRegKey);
	STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
	STDMETHODIMP QueryContextMenu(HMENU hMenu, UINT iMenu, UINT cmdFirst, UINT cmdLast, UINT flg);
	STDMETHODIMP InvokeCommand(CMINVOKECOMMANDINFO *info);
	STDMETHODIMP GetCommandString(UINT_PTR cmd, UINT flg, UINT *, char *name, UINT cchMax);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();
};

class ShellExtClassFactory : public IClassFactory
{
protected:
	ULONG	refCnt;
	BOOL	isAdmin;

public:
	ShellExtClassFactory(BOOL _isAdmin);
	~ShellExtClassFactory();

	STDMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObj);
	STDMETHODIMP LockServer(BOOL fLock);
	STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();
};

class TShellExtRegistry : public TRegistry {
public:
	BOOL	isAdmin;
	char	clsId[MAX_PATH];
	char	clsIdEx[MAX_PATH];

	TShellExtRegistry(BOOL _isAdmin, BOOL is_link=FALSE);
	BOOL CreateClsKey() { return CreateKey(clsIdEx); }
	BOOL OpenClsKey() { return OpenKey(clsIdEx); }
	BOOL DeleteClsKey();

	BOOL OpenMenuFlagKey();
};

class ShellExtSystem {
public:
	HINSTANCE	HInstance;
	int			DllRefAdminCnt;
	int			DllRefUserCnt;
	char		*DllName;
	char		*ExeName;
	HMENU		lastMenu;
	HBITMAP		hMenuBmp;

	ShellExtSystem(HINSTANCE hI);
	~ShellExtSystem();
	int AddDllRef(BOOL isAdmin) {
		int &ref = isAdmin ? DllRefAdminCnt : DllRefUserCnt;
		return	++ref;
	}
	int RelDllRef(BOOL isAdmin) {
		int &ref = isAdmin ? DllRefAdminCnt : DllRefUserCnt;
		return	--ref;
	}
	int GetDllRef(BOOL isAdmin) {
		return isAdmin ? DllRefAdminCnt : DllRefUserCnt;
	}
	int GetDllAllRef() {
		return DllRefAdminCnt + DllRefUserCnt;
	}
};

// for VC4
#ifndef CMF_NOVERBS
#define CMF_NOVERBS             0x00000008
#define CMF_CANRENAME           0x00000010
#define CMF_NODEFAULT           0x00000020
#define CMF_INCLUDESTATIC       0x00000040
#endif

//DEFINE_GUID(CLSID_ShellExtID1, 0x9bc20d5, 0x3ebc, 0x4f43, 0x89, 0x6b, 0xa1, 0x76, 0xba, \
//	0x63, 0x64, 0x6f);	// OLD1 CLSID	{09BC20D5-3EBC-4f43-896B-A176BA63646F}
//DEFINE_GUID(CLSID_ShellExtID2, 0x644aad22, 0x704b, 0x4b2d, 0x93, 0x50, 0xfe, 0x94, 0xfe, \
//	0x66, 0xae, 0x54);	// OLD2 CLSID for all file	{644AAD22-704B-4b2d-9350-FE94FE66AE54}
//DEFINE_GUID(CLSID_ShellExtLinkID2, 0xd52ba7ce, 0xe594, 0x4542, 0xb0, 0x42, 0xb1, 0xed, 0x70, \
//	0x8f, 0xe6, 0x4);	// OLD2 CLSID for .lnk file	{D52BA7CE-E594-4542-B042-B1ED708FE604}
//
//DEFINE_GUID(CLSID_ShellExtID3, 0x18edb5af, 0xa764, 0x4079, 0xa2, 0xd7, 0x8c, 0x30, 0x89, \
//	0xd4, 0x38, 0x46);	// OLD3 CLSID for all file	{18EDB5AF-A764-4079-A2D7-8C3089D43846}
//DEFINE_GUID(CLSID_ShellExtLinkID3, 0x95f3be2f, 0xdd53, 0x4bb3, 0xb1, 0xd1, 0x83, 0xc1, 0xc8, \
//	0x40, 0x40, 0x97);	// OLD3 CLSID for .lnk file	{95F3BE2F-DD53-4bb3-B1D1-83C1C8404097}
//
//DEFINE_GUID(CLSID_ShellExtID4, 0xa17026e8, 0x7c80, 0x48a5, 0xb6, 0x44, 0xbd, 0xb7, 0xf, \
//	0x92, 0xd1, 0x24);	// OLD4 CLSID for all file	{A17026E8-7C80-48a5-B644-BDB70F92D124}
//DEFINE_GUID(CLSID_ShellExtLinkID4, 0x915def58, 0x5635, 0x4cff, 0x86, 0xf9, 0x83, 0x11, 0x4f, \
//	0xdc, 0x66, 0xc8);	// OLD4 CLSID for .lnk file	{A17026E8-7C80-48a5-B644-BDB70F92D124}

#endif

