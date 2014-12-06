static char *utility_id = 
	"@(#)Copyright (C) 2004-2012 H.Shirouzu		utility.cpp	ver2.10";
/* ========================================================================
	Project  Name			: general routine
	Create					: 2004-09-15(Wed)
	Update					: 2012-06-17(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include <stdio.h>
#include <stddef.h>
#include "utility.h"

/*=========================================================================
	拡張 strtok()
		"" に出くわすと、"" の中身を取り出す
		token の前後に空白があれば取り除く
		それ以外は、strtok_r() と同じ
=========================================================================*/
void *strtok_pathV(void *str, const void *sep, void **p, BOOL remove_quote)
{
	const void	*quote=QUOTE_V, *org_sep = sep;

	if (str)
		*p = str;
	else
		str = *p;

	if (!*p)
		return	NULL;

	// 頭だし
	while (GetChar(str, 0) && (strchrV(sep, GetChar(str, 0)) || GetChar(str, 0) == ' '))
		str = MakeAddr(str, 1);
	if (GetChar(str, 0) == 0)
		return	NULL;

	// 終端検出
	void	*in = str, *out = str;
	for ( ; GetChar(in, 0); in = MakeAddr(in, 1)) {
		BOOL	is_set = FALSE;

		if (sep == org_sep) {	// 通常 mode
			if (strchrV(sep, GetChar(in, 0))) {
				break;
			}
			else if (GetChar(in, 0) == '"') {
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
			if (GetChar(in, 0) == '"') {
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
			SetChar(out, 0, GetChar(in, 0));
			out = MakeAddr(out, 1);
		}
	}
	*p = GetChar(in, 0) ? MakeAddr(in, 1) : NULL;
	SetChar(out, 0, 0);

	// 末尾の空白を取り除く
	for (out = MakeAddr(out, -1); out >= str && GetChar(out, 0) == ' '; out = MakeAddr(out, -1))
		SetChar(out, 0, 0);

	return	str;
}

/*=========================================================================
	コマンドライン解析（CommandLineToArgvW API の ANSI版）
		CommandLineToArgvW() と同じく、返り値の開放は呼び元ですること
=========================================================================*/
void **CommandLineToArgvV(void *cmdLine, int *_argc)
{
#define MAX_ARG_ALLOC	16
	int&	argc = *_argc;
	void	**argv = NULL, *p;
	void	*separantor = IS_WINNT_V ? (char *)L" \t" : " \t";

	argc = 0;
	while (1) {
		if ((argc % MAX_ARG_ALLOC) == 0)
			argv = (void **)realloc(argv, (argc + MAX_ARG_ALLOC) * sizeof(void *));
		if ((argv[argc] = strtok_pathV(argc ? NULL : cmdLine, separantor, &p)) == NULL)
			break;
		argc++;
	}

	return	argv;
}

/*=========================================================================
	PathArray
=========================================================================*/
PathArray::PathArray(void) : THashTbl(1000)
{
	num = 0;
	pathArray = NULL;
	flags = 0;
}

PathArray::PathArray(const PathArray &src) : THashTbl(1000)
{
	num = 0;
	pathArray = NULL;
	*this = src;
	flags = 0;
}

PathArray::~PathArray()
{
	Init();
}

void PathArray::Init(void)
{
	while (--num >= 0) {
		delete pathArray[num];
	}
	free(pathArray);
	num = 0;
	pathArray = NULL;
}

BOOL PathArray::PathObj::Set(const void *_path, int _len)
{
	path = NULL;
	len = 0;

	if (_path) {
		if (_len < 0) _len = strlenV(_path);
		len = _len;
		int	alloc_len = len + 1;
		path = malloc(alloc_len * CHAR_LEN_V);
		memcpy(path, _path, alloc_len * CHAR_LEN_V);
	}
	return	TRUE;
}

int PathArray::RegisterMultiPath(const void *_multi_path, const void *separator)
{
	void	*multi_path = strdupV(_multi_path);
	void	*tok, *p;
	int		cnt = 0;
	BOOL	is_remove_quote = (flags & NO_REMOVE_QUOTE) == 0 ? TRUE : FALSE;

	for (tok = strtok_pathV(multi_path, separator, &p, is_remove_quote);
			tok; tok = strtok_pathV(NULL, separator, &p, is_remove_quote)) {
		if (RegisterPath(tok))	cnt++;
	}
	free(multi_path);
	return	cnt;
}

int PathArray::GetMultiPath(void *multi_path, int max_len,
	const void *separator, const void *escape_char)
{
	void	*buf = multi_path;
	void	*FMT_STRSTR_V = IS_WINNT_V ? (void *)L"%s%s" : (void *)"%s%s";
	int		sep_len = strlenV(separator);
	int		total_len = 0;
	int		escape_val = escape_char ? GetChar(escape_char, 0) : 0;

	for (int i=0; i < num; i++) {
		BOOL	is_escape = escape_val && strchrV(pathArray[i]->path, escape_val);
		int		need_len = pathArray[i]->len + 1 + (is_escape ? 2 : 0) + (i ? sep_len : 0);

		if (max_len - total_len < need_len) {
			SetChar(multi_path, total_len, 0);
			return -1;
		}
		if (i) {
			memcpy(MakeAddr(buf, total_len), separator, sep_len * CHAR_LEN_V);
			total_len += sep_len;
		}
		if (is_escape) {
			SetChar(buf, total_len, '"');
			total_len++;
		}
		memcpy(MakeAddr(buf, total_len), pathArray[i]->path, pathArray[i]->len * CHAR_LEN_V);
		total_len += pathArray[i]->len;
		if (is_escape) {
			SetChar(buf, total_len, '"');
			total_len++;
		}
	}
	SetChar(multi_path, total_len, 0);
	return	total_len;
}

int PathArray::GetMultiPathLen(const void *separator, const void *escape_char)
{
	void	*FMT_STRSTR_V = IS_WINNT_V ? (void *)L"%s%s" : (void *)"%s%s";
	int		total_len = 0;
	int		sep_len = strlenV(separator);
	int		escape_val = escape_char ? GetChar(escape_char, 0) : 0;

	for (int i=0; i < num; i++) {
		BOOL	is_escape = escape_val /* && strchrV(pathArray[i]->path, escape_val)*/;
		total_len += pathArray[i]->len + (is_escape ? 2 : 0) + (i ? sep_len : 0);
	}
	return	total_len + 1;
}

BOOL PathArray::SetPath(int idx, const void *path, int len)
{
	if (len < 0) len = strlenV(path);
	pathArray[idx] = new PathObj(path, len);
	Register(pathArray[idx], MakeHashId(pathArray[idx]));
	return	TRUE;
}

/*
BOOL PathArray::PathObj *PathArray::SearchPathObj(const void *path)
{
	THashObj *top = hashTbl + (MakeHash(id) % hashNum);

	for (THashObj *obj=top->nextHash; obj != top; obj=obj->nextHash) {
		if (obj->id == id)
			return obj;
	}
	return	NULL;
}*/

BOOL PathArray::RegisterPath(const void *path)
{
	if (!path || !GetChar(path, 0)) return	FALSE;

	int len = strlenV(path);

	if ((flags & ALLOW_SAME) == 0 && Search(path, MakeHashId(path, len))) return FALSE;

#define MAX_ALLOC	100
	if ((num % MAX_ALLOC) == 0) {
		pathArray = (PathObj **)realloc(pathArray, (num + MAX_ALLOC) * sizeof(void *));
	}
	SetPath(num++, path, len);

	return	TRUE;
}

BOOL PathArray::ReplacePath(int idx, void *new_path)
{
	if (idx >= num)
		return	FALSE;

	if (pathArray[idx]) {
		free(pathArray[idx]);
	}
	SetPath(idx, new_path);
	return	TRUE;
}

PathArray& PathArray::operator=(const PathArray& init)
{
	Init();

	pathArray = (PathObj **)malloc(((((num = init.num) / MAX_ALLOC) + 1) * MAX_ALLOC)
				* sizeof(void *));

	for (int i=0; i < init.num; i++) {
		SetPath(i, init.pathArray[i]->path, init.pathArray[i]->len);
	}

	return	*this;
}

/*=========================================================================
	DriveMng
=========================================================================*/
DriveMng::DriveMng()
{
	memset(drvID, 0, sizeof(drvID));
	noIdCnt = 0;
	*driveMap = 0;
}

DriveMng::~DriveMng()
{
	Init();
}

void DriveMng::Init()
{
	for (int i=0; i < MAX_DRIVE_LETTER; i++) {
		if (drvID[i].data) {
			delete [] drvID[i].data;
		}
	}
	memset(drvID, 0, sizeof(drvID));
	*driveMap = 0;
}

BOOL DriveMng::RegisterDriveID(int idx, void *data, int len)
{
	drvID[idx].data = new BYTE [len];
	memcpy(drvID[idx].data, data, drvID[idx].len = len);

	return	TRUE;
}

void DriveMng::SetDriveMap(char *_driveMap)
{
	if (strcmp(_driveMap, driveMap) == 0) return;

	DWORD	val = 1;
	Init();
	strcpy(driveMap, _driveMap);

	for (char *c=driveMap; *c; c++) {
		if (*c == ',')	val++;
		else 			RegisterDriveID(LetterToIndex(*c), &val, sizeof(val));
	}
}

BOOL DriveMng::SetDriveID(int drvLetter)
{
	TRegistry	reg(HKEY_LOCAL_MACHINE);
	BYTE		buf[1024];
	WCHAR		*wbuf = (WCHAR *)buf;
	char		reg_path[MAX_PATH * 2];
	int			size, idx = LetterToIndex(drvLetter);
	DWORD		val = 0;

// NT 系
	if (IS_WINNT_V) {
		if (reg.OpenKey(MOUNTED_DEVICES)) {
			::sprintf(reg_path, FMT_DOSDEVICES, drvLetter);
			size = sizeof(buf);
			if (reg.GetByte(reg_path, buf, &size)) {
				if (wcsncmp(wbuf, L"\\??\\", 4) == 0 && (wbuf = wcschr(wbuf, '#'))
				&& (wbuf = wcschr(wbuf+1, '#')) && (wbuf = wcschr(wbuf, '&'))) {
					val = wcstoul(wbuf+1, 0, 16);
				}
				else {
					val = *(DWORD *)buf;
				}
				if (val <= 30) val |= 0x88000000;
				return	RegisterDriveID(idx, &val, sizeof(val));
			}
		}
		return	RegisterDriveID(idx, &val, 1);
	}

// 95 系
	TRegistry	dynReg(HKEY_DYN_DATA);
	char		dyn_path[MAX_PATH];
	int			no_id_cnt = 0;

	if (reg.OpenKey(ENUM_DEVICES)) {
		for (int i=0; reg.EnumKey(i, reg_path, MAX_PATH); i++) {	// SCSI, etc...
			if (reg.OpenKey(reg_path) == FALSE)
				continue;
			char *ctrl = reg_path + strlen(reg_path);
			*ctrl++ = '\\';

			for (int j=0; reg.EnumKey(j, ctrl, MAX_PATH); j++) { // Controller
				if (reg.OpenKey(ctrl) == FALSE)
					continue;
				char *dev = ctrl + strlen(ctrl);
				*dev++ = '\\';

				for (int k=0; reg.EnumKey(k, dev, MAX_PATH); k++) { // Devices...
					if (reg.OpenKey(dev) == FALSE)
						continue;

					val = 0;
					if (reg.GetStr(DRIVE_LETTERS, (char *)buf, sizeof(buf))) {
						if (*buf == 0)
							no_id_cnt++;	// format 後、reboot するまで registry に未反映らしい
						for (int l=0; buf[l]; l++) {
							if (drvLetter == buf[l]) {
								val = 1;
								break;
							}
						}
					}
					if (val && dynReg.OpenKey(CONFIG_ENUM)) {
						for (int m=0; dynReg.EnumKey(m, dyn_path, sizeof(dyn_path)); m++) {
							if (dynReg.OpenKey(dyn_path) == FALSE)
								continue;
							if (dynReg.GetStr(HARDWARE_KEY, (char *)buf, sizeof(buf))) {
								if (strcmp(reg_path, (char *)buf) == 0) {
									return	RegisterDriveID(idx, buf, (int)strlen((char *)buf));
								}
							}
							dynReg.CloseKey();
						}
						dynReg.CloseKey();
					}
					reg.CloseKey();
				}
				reg.CloseKey();
			}
			reg.CloseKey();
		}
	}
	if (noIdCnt == 0)
		noIdCnt = no_id_cnt;
	val = 0;
	return	RegisterDriveID(idx, &val, sizeof(val));
}

BOOL DriveMng::IsSameDrive(int drvLetter1, int drvLetter2)
{
	drvLetter1 = toupper(drvLetter1);
	drvLetter2 = toupper(drvLetter2);

	if (drvLetter1 == drvLetter2)
		return	TRUE;

	int	idx1 = LetterToIndex(drvLetter1);
	int	idx2 = LetterToIndex(drvLetter2);

	if (drvID[idx1].len == 0)
		SetDriveID(drvLetter1);

	if (drvID[idx2].len == 0)
		SetDriveID(drvLetter2);

	return	drvID[idx1].len == drvID[idx2].len
		&&	memcmp(drvID[idx1].data, drvID[idx2].data, drvID[idx1].len) == 0
		&&	(IS_WINNT_V ? drvID[idx1].len != 1 : (drvID[idx1].len == 0 && noIdCnt == 1));
}

// ワード単位ではなく、文字単位で折り返すための EDIT Control 用 CallBack
int CALLBACK EditWordBreakProc(LPTSTR str, int cur, int len, int action)
{
	switch (action) {
	case WB_LEFT:
		return	cur + 1;
	case WB_RIGHT:
		return	cur - 1;
	case WB_ISDELIMITER:
		return	TRUE;
	}
	return	0;
}

BOOL GetRootDirV(const void *path, void *root_dir)
{
	if (GetChar(path, 0) == '\\') {	// "\\server\volname\" 4つ目の \ を見つける
		DWORD	ch;
		int		backslash_cnt = 0, offset;

		for (offset=0; (ch = GetChar(path, offset)) != 0 && backslash_cnt < 4; offset++) {
			if (ch == '\\')
				backslash_cnt++;
		}
		memcpy(root_dir, path, offset * CHAR_LEN_V);
		if (backslash_cnt < 4)					// 4つの \ がない場合は、末尾に \ を付与
			SetChar(root_dir, offset++, '\\');	// （\\server\volume など）
		SetChar(root_dir, offset, 0);	// NULL terminate
	}
	else {	// "C:\" 等
		memcpy(root_dir, path, 3 * CHAR_LEN_V);
		SetChar(root_dir, 3, 0);	// NULL terminate
	}
	return	TRUE;
}

/*
	ネットワークプレースを UNC path に変換 (src == dst 可）
*/

BOOL NetPlaceConvertV(void *src, void *dst)
{
	IShellLink		*shellLink;		// VC4 には IShellLink ANSI 版しか定義がないための暫定処置
	IPersistFile	*persistFile;	// （実際は NT系では IShellLinkW を呼び出し）
	WCHAR	wSrcBuf[MAX_PATH], wDstBuf[MAX_PATH];
	WCHAR	*wSrc = IS_WINNT_V ? (WCHAR *)src : wSrcBuf;
	WCHAR	*wDst = IS_WINNT_V ? wDstBuf : (WCHAR *)dst;
	BOOL	ret = FALSE;
	DWORD	attr, attr_mask = FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_READONLY;

	if (SHGetPathFromIDListV == NULL)	// NT4.0 系は無視
		return	FALSE;

	if ((attr = GetFileAttributesV(src)) == 0xffffffff || (attr & attr_mask) != attr_mask)
		return	FALSE;	// ディレクトリかつronly でないものは関係ない

	if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
				  IID_IShellLinkV, (void **)&shellLink))) {
		if (SUCCEEDED(shellLink->QueryInterface(IID_IPersistFile, (void **)&persistFile))) {
			if (!IS_WINNT_V)
				AtoW((char *)src, wSrc, MAX_PATH);

			if (SUCCEEDED(persistFile->Load(wSrc, STGM_READ))) {
				if (SUCCEEDED(shellLink->GetPath((char *)wDst, MAX_PATH, 0, SLGP_UNCPRIORITY))) {
					if (!IS_WINNT_V)
						WtoA(wDst, (char *)wDstBuf, MAX_PATH);
					MakePathV(dst, wDstBuf, EMPTY_STR_V);
					ret = TRUE;
				}
			}
			persistFile->Release();
		}
		shellLink->Release();
	}

	return	ret;
}

DWORD ReadReparsePoint(HANDLE hFile, void *buf, DWORD size)
{
	if (!::DeviceIoControl(hFile, FSCTL_GET_REPARSE_POINT, NULL, 0, buf, size, &size, NULL)) {
		size = 0;
	}
	return	size;
}

BOOL WriteReparsePoint(HANDLE hFile, void *buf, DWORD size)
{
	if (!::DeviceIoControl(hFile, FSCTL_SET_REPARSE_POINT, buf, size, 0, 0, &size, 0))
		return	0;

	return	TRUE;
}

BOOL IsReparseDataSame(void *d1, void *d2)
{
	REPARSE_DATA_BUFFER *r1 = (REPARSE_DATA_BUFFER *)d1;
	REPARSE_DATA_BUFFER *r2 = (REPARSE_DATA_BUFFER *)d2;

	return	r1->ReparseTag        == r2->ReparseTag
		&&	r1->ReparseDataLength == r2->ReparseDataLength
		&&	!memcmp(&r1->GenericReparseBuffer, &r2->GenericReparseBuffer,
			r1->ReparseDataLength + (IsReparseTagMicrosoft(r1->ReparseTag) ? 0 : sizeof(GUID)));
}

BOOL DeleteReparsePoint(HANDLE hFile, void *buf)
{

	REPARSE_DATA_BUFFER	rdbuf;
	DWORD	size = IsReparseTagMicrosoft(((REPARSE_DATA_BUFFER *)buf)->ReparseTag) ?
					REPARSE_DATA_BUFFER_HEADER_SIZE : REPARSE_GUID_DATA_BUFFER_HEADER_SIZE;

	memcpy(&rdbuf, buf, size);
	rdbuf.ReparseDataLength = 0;
	return	::DeviceIoControl(hFile, FSCTL_DELETE_REPARSE_POINT, &rdbuf, size, 0, 0, &size, NULL);
}


/*
 DataList ...  List with data(hash/fileID...)
*/
DataList::DataList(int size, int max_size, int _grow_size, VBuf *_borrowBuf, int _min_margin)
{
	num = 0;
	top = end = NULL;

	if (size) Init(size, max_size, _grow_size, _borrowBuf, _min_margin);
}

DataList::~DataList()
{
	UnInit();
}

BOOL DataList::Init(int size, int max_size, int _grow_size, VBuf *_borrowBuf, int _min_margin)
{
	grow_size = _grow_size;
	min_margin = _min_margin;
	cv.Initialize(2);

	BOOL ret = buf.AllocBuf(size, max_size, _borrowBuf);
	Clear();
	return	ret;
}

void DataList::UnInit()
{
	top = end = NULL;
	buf.FreeBuf();
	cv.UnInitialize();
}

void DataList::Clear()
{
	top = end = NULL;
	buf.SetUsedSize(0);
	num = 0;
}

DataList::Head *DataList::Alloc(void *data, int data_size, int need_size)
{
	Head	*cur = NULL;
	int		alloc_size = need_size + sizeof(Head);

	alloc_size = ALIGN_SIZE(alloc_size, 8);

	if (!top) {
		cur = top = end = (Head *)buf.Buf();
		cur->next = cur->prior = NULL;
	}
	else {
		if (top >= end) {
			cur = (Head *)((BYTE *)top + top->alloc_size);
			if ((BYTE *)cur + alloc_size < buf.Buf() + buf.MaxSize()) {
				int need_grow = (int)(((BYTE *)cur + alloc_size) - (buf.Buf() + buf.Size()));
				if (need_grow > 0) {
					if (!buf.Grow(ALIGN_SIZE(need_grow, PAGE_SIZE))) {
						MessageBox(0, "can't alloc mem", "", MB_OK);
						goto END;
					}
				}
			}
			else {
				if ((BYTE *)end < buf.Buf() + alloc_size) {	// for debug
					MessageBox(0, "buf is too small", "", MB_OK);
					goto END;
				}
				cur = (Head *)buf.Buf();
			}
		}
		else {
			if ((BYTE *)end < (BYTE *)top + top->alloc_size + alloc_size) {	// for debug
				MessageBox(0, "buf is too small2", "", MB_OK);
				goto END;
			}
			cur = (Head *)((BYTE *)top + top->alloc_size);
		}
		top->next = cur;
		cur->prior = top;
		cur->next = NULL;
		top = cur;
	}
	cur->alloc_size = alloc_size;
	cur->data_size = data_size;
	if (data) {
		memcpy(cur->data, data, data_size);
	}
	buf.AddUsedSize(alloc_size);
	num++;

END:
	return	cur;
}

DataList::Head *DataList::Get()
{
	Head	*cur = end;

	if (!cur) goto END;

	if (cur->next) {
		cur->next->prior = cur->prior;
	}
	else {
		top = cur->prior;
	}
	end = cur->next;

	num--;

END:
	return	cur;
}

DataList::Head *DataList::Fetch(Head *prior)
{
	Head	*cur = prior ? prior->next : end;

	return	cur;
}

int DataList::RemainSize()
{
	int ret = 0;

	if (top) {
		BYTE *top_end = (BYTE *)top + top->alloc_size;

		if (top >= end) {
			int size1 = (int)(buf.MaxSize() - (top_end - buf.Buf()));
			int size2 = (int)((BYTE *)end - buf.Buf());
			ret = max(size1, size2);
		}
		else {
			ret = (int)((BYTE *)end - top_end);
		}
	}
	else {
		ret = buf.MaxSize();
	}

	if (ret > 0) ret -= sizeof(Head);

	return	ret;
}

#ifdef DEBUG
void DBGWrite(char *fmt,...)
{
	static HANDLE	hDbgFile = INVALID_HANDLE_VALUE;

	if (hDbgFile == INVALID_HANDLE_VALUE) {
		hDbgFile = ::CreateFile("c:\\tlib_dbg.txt",
					GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, CREATE_ALWAYS, 0, 0);
	}

	char	buf[1024];
	va_list	va;
	va_start(va, fmt);
	DWORD	len = wvsprintf(buf, fmt, va);
	va_end(va);
	::WriteFile(hDbgFile, buf, len, &len, 0);
}

void DBGWriteW(WCHAR *fmt,...)
{
	static HANDLE	hDbgFile = INVALID_HANDLE_VALUE;

	if (hDbgFile == INVALID_HANDLE_VALUE) {
		hDbgFile = ::CreateFile("c:\\tlib_dbgw.txt",
					GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, CREATE_ALWAYS, 0, 0);
		DWORD	len = 2;
		::WriteFile(hDbgFile, "\xff\xfe", len, &len, 0);
	}

	WCHAR	buf[1024];
	va_list	va;
	va_start(va, fmt);
	DWORD	len = vswprintf(buf, fmt, va);
	va_end(va);
	::WriteFile(hDbgFile, buf, len * 2, &len, 0);
}
#endif
