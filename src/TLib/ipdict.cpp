static char *ipdict_id = 
	"@(#)Copyright (C) H.Shirouzu 2017   ipdict.cpp	Ver4.50";
/* ========================================================================
	Project  Name			: IPDict
	Module Name				: Serialize/Deserialize Dict
	Create					: 2017-02-04(Sat)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "tlib.h"

using namespace std;

IPDict::IPDict(const BYTE *s, size_t len)
{
	if (s && len) {
		unpack(s, len);
	}
}

IPDict::~IPDict()
{
	clear();
}

void IPDict::clear()
{
//	for (auto itr=dict.begin(); itr != dict.end(); itr++) {
//		delete itr->second;
//	}
	dict.clear();
	keys.clear();
}

bool IPDict::put_int(const char *key, int64 val)
{
	BYTE	buf[64];
	size_t	pack_len = ipdict_pack_int(val, buf, sizeof(buf));

	return	set(key, buf, pack_len);
}

bool IPDict::put_float(const char *key, double val)
{
	BYTE	buf[64];
	size_t	pack_len = ipdict_pack_float(val, buf, sizeof(buf));

	return	set(key, buf, pack_len);
}

bool IPDict::put_str(const char *key, const char *s, int slen)
{
	if (slen == -1) {
		slen = (int)strlen(s);
	}
	size_t	full_len = ipdict_pack_str(0, slen, 0, 0) + 1;
	DynBuf	*dbuf = new DynBuf(full_len);

	dbuf->SetUsedSize(ipdict_pack_str(s, slen, dbuf->Buf(), full_len));

	return	set_core(key, dbuf);
}

bool IPDict::put_bytes(const char *key, const BYTE *buf, size_t len)
{
	size_t	full_len = ipdict_pack_bytes(0, len, 0, 0);
	DynBuf	*dbuf = new DynBuf(full_len);

	dbuf->SetUsedSize(ipdict_pack_bytes(buf, len, dbuf->Buf(), full_len));

	return	set_core(key, dbuf);
}

bool IPDict::put_dict(const char *key, const IPDict &d)
{
	DynBuf	*dbuf = new DynBuf(d.content_size() + 1); // add \0 margin

	dbuf->SetUsedSize(d.pack_content(dbuf->Buf()));

	return	set_core(key, dbuf);
}

bool IPDict::put_ipdict(const char *key, const IPDict &d)
{
	DynBuf	*dbuf = new DynBuf(d.pack_size());

	dbuf->SetUsedSize(d.pack(dbuf->Buf(), dbuf->Size()));

	return	set_core(key, dbuf);
}

bool IPDict::put_int_list(const char *key, const IPDictIntList &val)
{
	size_t						content_len = 0;
	list<shared_ptr<DynBuf>>	vlist;

	for (auto &i: val) {
		DynBuf	*dbuf = new DynBuf(ipdict_pack_int(i, 0, 0) + 1);
		size_t	len = ipdict_pack_int(i, dbuf->Buf(), dbuf->Size());
		dbuf->SetUsedSize(len);
		vlist.push_back(shared_ptr<DynBuf>(dbuf));
		content_len += len + 2 + len_to_hexlen(len);
	}

	return	put_list_core(key, vlist, content_len);
}

bool IPDict::put_float_list(const char *key, const IPDictFloatList &val)
{
	size_t						content_len = 0;
	list<shared_ptr<DynBuf>>	vlist;

	for (auto &i: val) {
		DynBuf	*dbuf = new DynBuf(ipdict_pack_float(i, 0, 0) + 1);
		size_t	len = ipdict_pack_float(i, dbuf->Buf(), dbuf->Size());
		dbuf->SetUsedSize(len);
		vlist.push_back(shared_ptr<DynBuf>(dbuf));
		content_len += len + 2 + len_to_hexlen(len);
	}

	return	put_list_core(key, vlist, content_len);
}

bool IPDict::put_str_list(const char *key, const IPDictStrList &val)
{
	size_t						content_len = 0;
	list<shared_ptr<DynBuf>>	vlist;

	for (auto &d: val) {
		DynBuf	*dbuf = new DynBuf(ipdict_pack_str(0, d->Len(), 0, 0) + 1);
		size_t	len = ipdict_pack_str(d->s(), d->Len(), dbuf->Buf(), dbuf->Size());
		dbuf->SetUsedSize(len);
		vlist.push_back(shared_ptr<DynBuf>(dbuf));
		content_len += len + 2 + len_to_hexlen(len);
	}

	return	put_list_core(key, vlist, content_len);
}

bool IPDict::put_bytes_list(const char *key, const IPDictBufList &val)
{
	size_t						content_len = 0;
	list<shared_ptr<DynBuf>>	vlist;

	for (auto &d: val) {
		DynBuf	*dbuf = new DynBuf(ipdict_pack_bytes(0, d->UsedSize(), 0, 0) + 1);
		size_t	len = ipdict_pack_bytes(d->Buf(), d->UsedSize(), dbuf->Buf(), dbuf->Size());
		dbuf->SetUsedSize(len);
		vlist.push_back(shared_ptr<DynBuf>(dbuf));
		content_len += len + 2 + len_to_hexlen(len);
	}

	return	put_list_core(key, vlist, content_len);
}

bool IPDict::put_dict_list(const char *key, const IPDictList &val)
{
	size_t						content_len = 0;
	list<shared_ptr<DynBuf>>	vlist;

	for (auto &d: val) {
		DynBuf	*dbuf = new DynBuf(d->content_size() + 1);
		size_t	len = d->pack_content(dbuf->Buf());
		dbuf->SetUsedSize(len);
		vlist.push_back(shared_ptr<DynBuf>(dbuf));
		content_len += len + 2 + len_to_hexlen(len);
	}

	return	put_list_core(key, vlist, content_len);
}

bool IPDict::put_ipdict_list(const char *key, const IPDictList &val)
{
	size_t						content_len = 0;
	list<shared_ptr<DynBuf>>	vlist;

	for (auto &d: val) {
		DynBuf	*dbuf = new DynBuf(d->pack_size() + 1);
		size_t	len = d->pack(dbuf->Buf(), dbuf->Size());
		dbuf->SetUsedSize(len);
		vlist.push_back(shared_ptr<DynBuf>(dbuf));
		content_len += len + 2 + len_to_hexlen(len);
	}

	return	put_list_core(key, vlist, content_len);
}

bool IPDict::put_list_core(const char *key, list<shared_ptr<DynBuf>> &vlist, size_t content_len)
{
	DynBuf	*dbuf = new DynBuf(content_len + 1);
	char	*s = (char *)dbuf->Buf();

	for (auto itr=vlist.begin(); itr != vlist.end(); itr++) {
		if (itr != vlist.begin()) {
			*s++ = ':';
		}
		s += sprintf(s, "%zx:", (*itr)->UsedSize());
		memcpy(s, (*itr)->Buf(), (*itr)->UsedSize());
		s += (*itr)->UsedSize();
	}

	char	*top = (char *)dbuf->Buf();
	dbuf->Buf()[s - top] = 0;
	dbuf->SetUsedSize(int(s - top));

	return	set_core(key, dbuf);
}

bool IPDict::get_int(const char *key, int64 *val) const
{
	auto	itr = dict.find(key);
	if (itr == dict.end()) {
		return false;
	}

	if (!ipdict_parse_int(itr->second->Buf(), itr->second->UsedSize(), val)) {
		return	false;
	}
	return	true;
}

bool IPDict::get_float(const char *key, double *val) const
{
	auto	itr = dict.find(key);
	if (itr == dict.end()) {
		return false;
	}

	if (!ipdict_parse_float(itr->second->Buf(), itr->second->UsedSize(), val)) {
		return	false;
	}
	return	true;
}

bool IPDict::get_str(const char *key, U8str *str) const
{
	auto	itr = dict.find(key);
	if (itr == dict.end()) {
		return false;
	}

	size_t	len = itr->second->UsedSize();
	str->Init((int)len + 1);

	if (len > 0) {
		len = ipdict_parse_str(itr->second->Buf(), len, str->Buf(), len + 1);
		if (len == 0) return false;
	}
	str->Buf()[len] = 0;
	return	true;
}

bool IPDict::get_bytes(const char *key, DynBuf *dbuf) const
{
	auto	itr = dict.find(key);
	if (itr == dict.end()) {
		return false;
	}
	auto	&ibuf = itr->second;
	size_t	max_len = ibuf->UsedSize();
	dbuf->Alloc(max_len);

	if (max_len > 0) {
		size_t len = ipdict_parse_bytes(ibuf->Buf(), ibuf->UsedSize(), dbuf->Buf(), max_len);
		if (len == 0) return false;
		dbuf->SetUsedSize(len);
	}
	return	true;
}

bool IPDict::get_dict(const char *key, IPDict *d) const
{
	auto	itr = dict.find(key);
	if (itr == dict.end()) {
		return false;
	}
	d->clear();
	auto	&dbuf = itr->second;
	if (dbuf->UsedSize() == 0) return true;

	return	d->unpack_core(dbuf->Buf(), dbuf->UsedSize()) == dbuf->UsedSize();
}

bool IPDict::get_ipdict(const char *key, IPDict *d) const
{
	auto	itr = dict.find(key);
	if (itr == dict.end()) {
		return false;
	}

	return	d->unpack(itr->second->Buf(), itr->second->UsedSize()) == itr->second->UsedSize();
}

bool IPDict::get_int_list(const char *key, IPDictIntList *val) const
{
	IPDictBufList	vlist;

	if (!parse_list_core(key, &vlist)) return false;

	val->clear();
	for (auto &d: vlist) {
		int64	v;
		bool ret = ipdict_parse_int(d->Buf(), d->UsedSize(), &v);
		if (!ret) return false;
		val->push_back(v);
	}
	return	true;
}

bool IPDict::get_float_list(const char *key, IPDictFloatList *val) const
{
	IPDictBufList	vlist;

	if (!parse_list_core(key, &vlist)) return false;

	val->clear();
	for (auto &d: vlist) {
		double	v;
		bool ret = ipdict_parse_float(d->Buf(), d->UsedSize(), &v);
		if (!ret) return false;
		val->push_back(v);
	}
	return	true;
}

bool IPDict::get_str_list(const char *key, IPDictStrList *val) const
{
	IPDictBufList	vlist;

	if (!parse_list_core(key, &vlist)) return false;

	val->clear();
	for (auto &d: vlist) {
		size_t	len = d->UsedSize() + 1;
		U8str	*u8 = new U8str((int)len);

		if (d->UsedSize() > 0) {
			len = ipdict_parse_str(d->Buf(), d->UsedSize(), u8->Buf(), len);
			if (len == 0) return false;
		} else {
			len = 0;
		}

		u8->Buf()[len] = 0;
		val->push_back(shared_ptr<U8str>(u8));
	}
	return	true;
}

bool IPDict::get_bytes_list(const char *key, IPDictBufList *val) const
{
	IPDictBufList	vlist;

	if (!parse_list_core(key, &vlist)) return false;

	val->clear();
	for (auto &d: vlist) {
		size_t	len = d->UsedSize();
		DynBuf	*dbuf = new DynBuf(len);

		if (d->UsedSize() > 0) {
			len = ipdict_parse_bytes(d->Buf(), d->UsedSize(), dbuf->Buf(), len);
			if (len == 0) return false;
		} else {
			len = 0;
		}

		dbuf->SetUsedSize(len);
		val->push_back(shared_ptr<DynBuf>(dbuf));
	}
	return	true;
}

bool IPDict::get_dict_list(const char *key, IPDictList *val) const
{
	IPDictBufList	vlist;

	if (!parse_list_core(key, &vlist)) return false;

	val->clear();
	for (auto &d: vlist) {
		IPDict	*ipd = new IPDict();
		if (d->UsedSize() > 0) {
			size_t size = ipd->unpack_core(d->Buf(), d->UsedSize());
			if (size == 0) return false;
			if (size != d->UsedSize()) {
				DBG("internal size mismatch %zd %zd\n", size, d->UsedSize());
			}
		}
		val->push_back(shared_ptr<IPDict>(ipd));
	}
	return	true;
}

bool IPDict::get_ipdict_list(const char *key, IPDictList *val) const
{
	IPDictBufList	vlist;

	if (!parse_list_core(key, &vlist)) return false;

	val->clear();
	for (auto &d: vlist) {
		IPDict	*ipd = new IPDict();
		size_t size = ipd->unpack(d->Buf(), d->UsedSize());
		if (size == 0) return false;
		if (size != d->UsedSize()) {
			DBG("internal size mismatch(ip) %zd %zd\n", size, d->UsedSize());
		}
		val->push_back(shared_ptr<IPDict>(ipd));
	}
	return	true;
}

bool IPDict::has_key(const char *key) const
{
	return	dict.find(key) != dict.end();
}

bool IPDict::parse_list_core(const char *key, IPDictBufList *vlist) const
{
	auto	itr = dict.find(key);
	if (itr == dict.end()) {
		return false;
	}

	vlist->clear();
	BYTE	*s   = itr->second->Buf();
	BYTE	*end = s + itr->second->UsedSize();

	while (s < end) {
		size_t	data_len;
		size_t	list_len = ipdict_parse_list(s, end - s, &data_len);
		if (list_len == 0) return false;

		DynBuf *dbuf = new DynBuf(data_len, s + list_len - data_len);
		dbuf->SetUsedSize(data_len);
		vlist->push_back(shared_ptr<DynBuf>(dbuf));

		s += list_len;
		if (s == end) break;
		if (*s != ':') return false;
		s++;
	}
	return	true;
}

size_t IPDict::pack_size(size_t max_num) const
{
	return	pack_size_core(content_size(max_num));
}

size_t IPDict::pack_size_core(size_t content_len) const
{
	// IP2:(content_hexsize):(content):Z + '\0'
	return	content_len + 8 + len_to_hexlen(content_len);
}

size_t IPDict::content_size(size_t max_num) const
{
	size_t	content_len = 0;
	size_t	num = 0;
	max_num = min(keys.size(), max_num);

	for (auto &k: keys) {
		if (num >= max_num) {
			break;
		}
		num++;
		auto itr = dict.find(k);
		size_t	data_len = itr->second->UsedSize();		// key:(hexlen):(content)
		content_len += itr->first.Len() + 1 + len_to_hexlen(data_len) + 1 + data_len;
	}
	if (num >= 2) {
		content_len += num - 1;
	}

	return	content_len;
}

size_t IPDict::pack_content(BYTE *_s, size_t max_num) const
{
	char	*s  = (char *)_s;
	char	*sv = s;

	max_num = min(keys.size(), max_num);
	size_t	num = 0;

	for (auto k=keys.begin(); k != keys.end(); k++) {
		if (num >= max_num) {
			break;
		}
		num++;
		if (k != keys.begin()) {
			*s++ = ':';
		}
		auto itr = dict.find(*k);
		memcpy(s, itr->first.s(), itr->first.Len());
		s += itr->first.Len();
		s += sprintf(s, ":%zx:", itr->second->UsedSize());
		memcpy(s, itr->second->Buf(), itr->second->UsedSize());
		s += itr->second->UsedSize();
	}
	return	s - sv;
}

size_t IPDict::pack(BYTE *_s, size_t max_buf, size_t max_num) const
{
	size_t	content_len = content_size(max_num);

	if (pack_size_core(content_len) > max_buf) return 0;

	char	*s  = (char *)_s;
	char	*sv = s;

	s += sprintf(s, IPDICT_HEAD "%zx:", content_len);
	s += pack_content((BYTE *)s, max_num);
	s += strcpyz(s, IPDICT_FOOT);

	return	s - sv;
}

size_t IPDict::unpack_core(const BYTE *_s, size_t len)
{
	const char	*s = (const char *)_s;
	const char	*sv  = s;
	const char	*end = s + len;

	while (s < end) {
		const char *key = s;
		if (!(s = strnchr(s, ':', int(end - s))) || key == s) return 0;
		size_t	key_len = s - key;

		if (++s >= end) return 0;
		const char *hexlen = s;
		if (!(s = strnchr(s, ':', int(end - s))) || hexlen == s) return 0;
		size_t	val_len = strtoul(hexlen, 0, 16);

		const char	*val = ++s;
		if (s + val_len > end || val_len > end - s) {
			return 0;
		}

		set(key, (const BYTE *)val, val_len, key_len);

		s += val_len;
		if (s == end) break;
		if (*s++ != ':') return	0;
	}
	return	s - sv;
}

size_t IPDict::unpack(const BYTE *_s, size_t len)
{
	clear();

	// IP2:0::Z ... minimum
	if (len < 8) return 0;

	const char	*s = (const char *)_s;
	const char	*sv  = s;
	const char	*end = s + len;

	if (strncmp(s, IPDICT_HEAD, IPDICT_HEAD_LEN) != 0) return 0;
	s += IPDICT_HEAD_LEN;

	const char *hexlen = s;
	if (!(s = strnchr(s, ':', int(end - s))) || hexlen == s) return 0;
	size_t	content_len = strtoul(hexlen, 0, 16);
	if (++s + content_len + IPDICT_FOOT_LEN > end) return 0;
	if (strncmp(s + content_len, IPDICT_FOOT, IPDICT_FOOT_LEN) != 0) return 0;

	size_t ret = unpack_core((BYTE *)s, content_len);
	if (ret == 0 && content_len > 0) return 0;
	s += ret + IPDICT_FOOT_LEN;

	return	s - sv;
}


bool IPDict::set(const char *key, const BYTE *data, size_t len, size_t key_len)
{
	DynBuf *dbuf = new DynBuf(len);
	memcpy(dbuf->Buf(), data, len);
	dbuf->SetUsedSize(len);

	return	set_core(key, dbuf, key_len);
}

bool IPDict::set_core(const char *_key, DynBuf *dbuf, size_t key_len)
{
	U8str	key(_key, BY_UTF8, int(key_len > 0 ? key_len : -1));

	if (strchr(key.s(), ':')) {
		delete dbuf;
		return false;
	}

	auto	itr = dict.find(key);
	if (itr == dict.end()) {
		keys.push_back(key);
		dict[key] = shared_ptr<DynBuf>(dbuf);
	}
	else {
		itr->second = shared_ptr<DynBuf>(dbuf);
	}
	return	true;
}

bool IPDict::del_key(const char *_key)
{
	U8str	key(_key);
	auto	itr = dict.find(key);

	if (itr == dict.end()) {
		return	false;
	}
	
	dict.erase(itr);
	keys.remove(key);
	return	true;
}

size_t ipdict_pack_int(int64 val, BYTE *buf, size_t max_len)
{
	size_t	len = 0;
	char	tmp[128];

	if (!buf) {
		return	(val >= 0) ? len_to_hexlen(val) : len_to_hexlen(-val)+1;
	}

	if (val >= 0) {
		len = sprintf(tmp, "%llx", val);
	}
	else {
		len = sprintf(tmp, "-%llx", -val);
	}
	if (!buf) {
		return	len;
	}
	if (len + 1 > max_len) return 0;

	return	strcpyz((char *)buf, tmp);
}

size_t ipdict_pack_float(double val, BYTE *buf, size_t max_len)
{
	BYTE	*p = (BYTE *)&val;
	int		len = sizeof(double);

	for (int i=sizeof(double)-1; i >= 0; i--) {
		if (p[i]) break;
		len--;
	}
	return	ipdict_pack_bytes(p, len, buf, max_len);
}

size_t ipdict_pack_str(const char *s, size_t len, BYTE *buf, size_t max_len)
{
	return	ipdict_pack_bytes((BYTE *)s, len, buf, max_len);
}

size_t ipdict_pack_bytes(const BYTE *val, size_t len, BYTE *buf, size_t max_len)
{
	if (!buf) {
		return	len;
	}
	if (len > max_len || !val) return 0;

	memcpy(buf, val, len);

	return	len;
}

bool ipdict_parse_int(const BYTE *_s, size_t len, int64 *val)
{
	int64		sign = 1;
	const char	*s = (char *)_s;
	const char	*end = s + len;

	if (s >= end) return false;
	if (*s == '-') {
		s++;
		sign = -1;
	}
	if (s >= end) return false;

	*val = 0;
	size_t blen = hexstr2bin_revendian_ex(s, (BYTE *)val, 8, int(end - s));
	*val *= sign;

	return	blen ? true : false;
}

bool ipdict_parse_float(const BYTE *s, size_t len, double *val)
{
	int64		sign = 1;

	if (len > sizeof(double)) return false;
	*val = 0.0;
	memcpy(val, s, len);

	return	true;
}

size_t ipdict_parse_str(const BYTE *s, size_t len, char *data, size_t max_len)
{
	return	ipdict_parse_bytes(s, len, (BYTE *)data, max_len);
}

size_t ipdict_parse_bytes(const BYTE *s, size_t len, BYTE *data, size_t max_len)
{
	if (len > (size_t)max_len) return false;

	memcpy(data, s, len);

	return	len;
}

size_t ipdict_parse_list(const BYTE *_s, size_t size, size_t *rsize)
{
	const char	*s = (char *)_s;
	const char	*sv = s;
	const char	*end = s + size;

	if (!(s = strnchr(s, ':', int(end - s))) || s == sv) return 0;
	if ((*rsize = strtoul(sv, 0, 16)) == 0) {
		if (s - sv != 1 || *sv != '0') return 0;
	}
	s++;
	if (s + *rsize > end) return 0;
	return	s + *rsize - sv;
}

size_t ipdict_size_fetch(const BYTE *buf, size_t size)
{
	if (size < (IPDICT_HEAD_LEN + 2)
	 || memcmp(buf, IPDICT_HEAD, IPDICT_HEAD_LEN)
	 || !memchr(buf + IPDICT_HEAD_LEN, ':', size - IPDICT_HEAD_LEN)) {
		return 0;
	}
	return	strtoul((char *)buf + IPDICT_HEAD_LEN, 0, 16);
}

#ifdef USE_VERIFY
bool ipdict_verify(const IPDict *ipdict, const DynBuf *key_blob, HCRYPTPROV h_csp)
{
	if (!h_csp) {
		static HCRYPTPROV h_tmp = []() {
			HCRYPTPROV	hCsp = NULL;
			::CryptAcquireContext(&hCsp, NULL,
				IsWinVista() ? MS_ENH_RSA_AES_PROV : MS_ENH_RSA_AES_PROV_XP,
				PROV_RSA_AES, CRYPT_VERIFYCONTEXT);
			return	hCsp;
		}();
		h_csp = h_tmp;
	}

	DynBuf	dbuf;

	if (!ipdict->get_bytes(IPDICT_SIGN_KEY, &dbuf) || dbuf.UsedSize() != RSA2048_SIGN_SIZE) {
		return	FALSE;
	}

	BYTE	sign[RSA2048_SIGN_SIZE];
	memcpy(sign, dbuf, RSA2048_SIGN_SIZE);
	swap_s(sign, RSA2048_SIGN_SIZE);

	HCRYPTHASH	hHash = NULL;
	HCRYPTKEY	hExKey = 0;
	bool		ret = false;

	size_t		max_packnum = ipdict->key_num() - 1; // 末尾SIGNを除外
	dbuf.Alloc(ipdict->pack_size(max_packnum));
	DWORD	pack_size = (DWORD)ipdict->pack(dbuf.Buf(), dbuf.Size(), max_packnum);
	dbuf.SetUsedSize(pack_size);

	if (!::CryptImportKey(h_csp, (BYTE *)key_blob->s(), (DWORD)key_blob->UsedSize(), 0, 0,
		&hExKey)) {
		return	ret;
	}
	if (::CryptCreateHash(h_csp, CALG_SHA_256, 0, 0, &hHash)) {
		if (::CryptHashData(hHash, dbuf, pack_size, 0)) {
			if (::CryptVerifySignature(hHash, sign, RSA2048_SIGN_SIZE, hExKey, 0, 0)) {
				ret = true;
		//		DBG("CryptVerifySignature OK!\n");
			}
			else {
				DBG("CryptVerifySignature err=%x\n", GetLastError());
			}
		}
		::CryptDestroyHash(hHash);
	}
	::CryptDestroyKey(hExKey);

	return	ret;
}
#endif


#ifdef _DEBUG

static shared_ptr<DynBuf> db1(new DynBuf(20, "01234567890123456789"));
static shared_ptr<DynBuf> db2(new DynBuf(50, "01234567890123456789012345678901234567890123456789"));
static shared_ptr<DynBuf> dbs1(new DynBuf(30, "012345678901234567890123456789"));
static shared_ptr<DynBuf> dbs2(new DynBuf(60, "012345678901234567890123456789012345678901234567890123456789"));

static const int64	iv1 = 1234LL;
static const int64	iv2 = -1234567890123456789LL;
static const char	*sv1 = "str";
static const char	*sv2 = "strstr2";

/* primitive */

static bool ipdic_prim_put(shared_ptr<IPDict> cmd)
{
	if (!cmd->put_int("int",  iv1))		return DBG("put_int err\n"),  false;
	if (!cmd->put_int("int2", iv2))		return DBG("put_int2 err\n"), false;
	if (!cmd->put_str("str",  sv1))		return DBG("put_str err\n"),  false;
	if (!cmd->put_str("str2", sv2))		return DBG("put_str2 err\n"), false;

	if (!cmd->put_bytes("bytes1", db1->Buf(), db1->UsedSize())) return DBG("put_bytes"), false;
	if (!cmd->put_bytes("bytes2", db2->Buf(), db2->UsedSize())) return DBG("put_bytes2"), false;

	return	true;
}

static bool ipdic_prim_get(shared_ptr<IPDict> cmd)
{
	int64	riv1 = -1;
	int64	riv2 = -1;
	U8str	rs1;
	U8str	rs2;
	DynBuf	rdb1;
	DynBuf	rdb2;
	DynBuf	rdbs1;
	DynBuf	rdbs2;

	if (!cmd->get_int("int",     &riv1) || riv1 != iv1)	return DBG("get_int err\n"),  false;
	if (!cmd->get_int("int2",    &riv2) || riv2 != iv2)	return DBG("get_int2 err\n"), false;
	if (!cmd->get_str("str",     &rs1)  || rs1 != sv1)	return DBG("get_str err\n"),  false;
	if (!cmd->get_str("str2",    &rs2)  || rs2 != sv2)	return DBG("get_str2 err\n"), false;
	if (!cmd->get_bytes("bytes1", &rdb1)|| rdb1 != *db1)	return DBG("get_bytes"),      false;
	if (!cmd->get_bytes("bytes2", &rdb2)|| rdb2 != *db2)	return DBG("get_bytes2"),     false;

	return	true;
}

/* dict */
static shared_ptr<IPDict> sd1(new IPDict);
static shared_ptr<IPDict> sd2(new IPDict);

static bool ipdic_dict_put(shared_ptr<IPDict> cmd)
{
	if (!ipdic_prim_put(sd2))  return false;

	if (!cmd->put_dict("sd1", *sd1)) return DBG("put_dict1"), false;
	if (!cmd->put_dict("sd2", *sd2)) return DBG("put_dict2"), false;

	return	true;
}

static bool ipdic_dict_get(shared_ptr<IPDict> cmd)
{
	shared_ptr<IPDict>	rsd1(new IPDict);
	shared_ptr<IPDict>	rsd2(new IPDict);

	if (!cmd->get_dict("sd1", rsd1.get()) || *rsd1 != *sd1) return DBG("get_dict1"), false;
	if (!cmd->get_dict("sd2", rsd2.get()) || *rsd2 != *sd2) return DBG("get_dict2"), false;

	if (!ipdic_prim_get(rsd2))  return false;

	return	true;
}

/* list */
static const IPDictIntList	il  = { 1, 2, 3, 4, 0, -5, INT64_MIN, INT64_MAX };
static const IPDictFloatList fl = { 0.0, 0.1, 2.2, -0.07, -100.3,
	DBL_MAX,
	DBL_MIN,
	numeric_limits<double>::infinity(),
//	numeric_limits<float>::quiet_NaN(),
//	numeric_limits<float>::signaling_NaN()
};
static const IPDictStrList	sl  = { make_shared<U8str>("str1"), make_shared<U8str>("strstr2") };
static const IPDictBufList	bl  = { shared_ptr<DynBuf>(db1), shared_ptr<DynBuf>(db2) };
static const IPDictList		dl  = { shared_ptr<IPDict>(sd1), shared_ptr<IPDict>(sd1) };
static const IPDictList		ipl = { shared_ptr<IPDict>(sd1), shared_ptr<IPDict>(sd2) };

static bool ipdic_list_put(shared_ptr<IPDict> cmd)
{
	if (!cmd->put_int_list("il",         il)) return DBG("put_int_list"), false;
	if (!cmd->put_float_list("fl",       fl)) return DBG("put_float_list"), false;
	if (!cmd->put_str_list("sl",         sl)) return DBG("put_str_list"), false;
	if (!cmd->put_bytes_list("bl",       bl)) return DBG("put_bytes_list"), false;
	if (!cmd->put_dict_list("dl",        dl)) return DBG("put_dict_list"), false;
	if (!cmd->put_ipdict_list("ipl",    ipl)) return DBG("put_ipdict_list"), false;

	return	true;
}

static bool ipdic_list_get(shared_ptr<IPDict> cmd)
{
	IPDictIntList	ril;
	IPDictFloatList	rfl;
	IPDictStrList	rsl;
	IPDictBufList	rbl;
	IPDictList		rdl;
	IPDictList		ripl;

	if (!cmd->get_int_list("il",   &ril) || ril != il)
		return DBG("get_int_list\n"), false;

	if (!cmd->get_float_list("fl",  &rfl) || rfl != fl)
		return DBG("get_float_list\n"), false;

	if (!cmd->get_str_list("sl",   &rsl) ||
		!equal(rsl.begin(), rsl.end(), sl.begin(), deref_eq<shared_ptr<U8str>>))
		return DBG("get_str_list\n"), false;

	if (!cmd->get_bytes_list("bl", &rbl) ||
		!equal(rbl.begin(), rbl.end(), bl.begin(), deref_eq<shared_ptr<DynBuf>>))
		return DBG("get_bytes_list\n"), false;

	if (!cmd->get_dict_list("dl",  &rdl) ||
		!equal(rdl.begin(), rdl.end(), dl.begin(), deref_eq<shared_ptr<IPDict>>))
		return DBG("get_dict_list\n"), false;

	if (!cmd->get_ipdict_list("ipl", &ripl) ||
		!equal(ripl.begin(), ripl.end(), ipl.begin(), deref_eq<shared_ptr<IPDict>>))
		return DBG("get_ipdict_list\n"), false;

	return	true;
}

void ipdic_test_init()
{
	ipdic_prim_put(sd1);
	ipdic_prim_put(sd2);
}


/* whole test */
static bool ipdic_pack_test(DynBuf *d)
{
	auto cmd = make_shared<IPDict>();

	ipdic_test_init();

	// primitive type
	if (!ipdic_prim_put(cmd)) return false;

	// dict type
	if (!ipdic_dict_put(cmd)) return false;

	// list type
	if (!ipdic_list_put(cmd)) return false;

	d->Alloc(cmd->pack_size());
	d->SetUsedSize(cmd->pack(d->Buf(), d->Size()));

	DBG("pack=%s\n", d->Buf());

	return	d->UsedSize() > 0 ? true : false;
}

static bool ipdic_unpack_test(DynBuf *d)
{
	auto cmd = make_shared<IPDict>();

	if (!cmd->unpack(d->Buf(), d->UsedSize())) return DBG("unpack err\n"), false;

	// primitive type
	if (!ipdic_prim_get(cmd)) return false;

	// dict type
	if (!ipdic_dict_get(cmd)) return false;

	// list type
	if (!ipdic_list_get(cmd)) return false;

	DBG("unpack success\n");

	return	true;
}

bool ipdic_test()
{
//	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF|_CRTDBG_CHECK_ALWAYS_DF|_CRTDBG_CHECK_CRT_DF|_CRTDBG_LEAK_CHECK_DF );

	bool ret = false;
	DynBuf	d;

//	for (int i=0; i < 64; i++) {
//		uint64	d1 = 1ULL << i;
//		uint64	d2 = 0xffffffffffffffffULL << i;
//		uint64	d3 = 0xffffffffffffffffULL >> i;
//		DBG("%d: %d:%d %d:%d %d:%d\n", i,
//			get_nlz64(d1), get_ntz64(d1),
//			get_nlz64(d2), get_ntz64(d2),
//			get_nlz64(d3), get_ntz64(d3)
//			);
//	}

	if (ipdic_pack_test(&d)) {
		ret = ipdic_unpack_test(&d);
	}

	_ASSERTE( _CrtCheckMemory( ) );

	if (!ret) {
		DBG("test error");
		Sleep(100000000);
	}
	return	ret;
}
#endif


