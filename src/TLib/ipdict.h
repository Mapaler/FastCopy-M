/*	@(#)Copyright (C) H.Shirouzu 2017   ipdict.h	Ver4.50 */
/* ========================================================================
	Project  Name			: IPDict
	Module Name				: Serializer for IPMsg Next Generation
	Create					: 2017-02-04(Sat)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef IPDICTL_H
#define IPDICTL_H

#include <string>
#include <list>
#include <map>
#include <memory>
#include <algorithm>

/* ==============================================================================
   IPDict : Serializer for IP Messenger Next Generation

   Full Format:
    "IP2:(contents_hexlen):(contents):Z"

   Contents Format:
    "(key):(value_hexlen):(value)"

    int value format:       "(int_hex_string)"  (if negative: "-(int_hex_string)")
    str value format:       "(string_as_utf8)"
    bytes value format:     "(bytes_as_bytes)"
    bytes_str value format: "(bytes_as_base64)" ... (for avoid \0) optional

    list value format:  "(item1_hexlen):(item1_value):(item2_hexlen):(item2_value)..."
      (itemN_value) is one of int/str/bytes/list/dict/ipdict value

    dict value format:  "(item1_key):(item1_hexlen):(item1_value):(item2_key):..."
      (itemN_value) is one of int/str/bytes/list/dict/ipdict value

    (ipdict(IPDict) value is same as Full Format)

   Format Example:
     JSON:   { key1: 1000, key2: "str1", key3: { key31: -2000 }, key4: [1000,2000] }
     IPDict: IP2:3d:key1:3:3e8:key2:4:str1:key3:c:key31:4:-7d0:key4:b:3:3e8:3:7d0:Z
             IP2:3d:         (content_len is 0x3d)                               :Z
                    key1:3(= val_len is 0x3):3e8(=val is 0x3e8=1000)...

   Sample code:
     // Serialize
     IPDict  dict;
     dict.put_int("key1", 1000);
     dict.put_str("key2", "str1");

     IPDict  subdict;
     subdict.put_int("key31", -2000);
     dict.put_dict("key3", subdict);

     IPDictIntList ilist;
     ilist.push_back(1000);
     ilist.push_back(2000);
     dict.put_int_list("key4", ilist);

     BYTE *serial_buf = new BYTE [dict.pack_size()];
     size_t serial_size = dict.pack(serial_buf, dict.pack_size());

     // De-Serialize
     IPDict  dsdict(serial_buf, serial_size);

     int64	ival;
     dsdict.get_int("key1", &ival);

     U8str  str;
     dsdict.get_str("key2", &str);

     IPDict subval;
     dsdict.get_dict("key3", &subval);
     int64  isubval;
     subval.get_int("key31", &isubval);

     IPDictIntList ilval;
     dsdict.get_int_list("key4", &ilval);

     delete [] serial_buf;

   Attention:
    IPDict format has no type info.
    But get_XXX() gives type info to IPDict.
 ============================================================================== */

#define IPDICT_HEAD		"IP2:"
#define IPDICT_FOOT		":Z"
#define IPDICT_HEAD_LEN	(sizeof(IPDICT_HEAD)-1)
#define IPDICT_FOOT_LEN	(sizeof(IPDICT_FOOT)-1)

class IPDict;
typedef std::map<U8str, std::shared_ptr<DynBuf>> IPDictMap;
typedef std::list<U8str>                   IPDictKeyList;
typedef std::list<int64>                   IPDictIntList;
typedef std::list<std::shared_ptr<U8str>>  IPDictStrList;
typedef std::list<std::shared_ptr<DynBuf>> IPDictBufList;
typedef IPDictBufList                      IPDictBytesList;
typedef std::list<std::shared_ptr<IPDict>> IPDictList;

template <class T> bool deref_eq(const T& x, const T& y) {
	return	*x == *y;
}

template <class T> bool deref_map_eq(const T& x, const T& y) {
	return	(x.first == y.first) && (*x.second == *y.second);
}

class IPDict {
	IPDictKeyList	keys;	// for inserted order
	IPDictMap		dict;	// "key" : "serialed_val"
							//    --> pack() -> "key:(len):serialed_val"

public:
	IPDict(const BYTE *s=NULL, size_t len=0);
	IPDict(const IPDict &org) {
		*this = org;
	}
	~IPDict();

	bool put_int(const char *key, int64 val);
	bool put_str(const char *key, const char *s, int slen=-1);
	bool put_bytes(const char *key, const BYTE *buf, size_t len);
	bool put_bytes_str(const char *key, const BYTE *buf, size_t len);
	bool put_dict(const char *key, const IPDict &dict);   // store as dict
	bool put_ipdict(const char *key, const IPDict &dict); // store as nested ipdict

	bool put_int_list(const char *key, const IPDictIntList &val);
	bool put_str_list(const char *key, const IPDictStrList &val);
	bool put_bytes_list(const char *key, const IPDictBufList &val);
	bool put_bytes_str_list(const char *key, const IPDictBufList &val);
	bool put_dict_list(const char *key, const IPDictList &val);
	bool put_ipdict_list(const char *key, const IPDictList &val);

	bool get_int(const char *key, int64 *val) const;
	bool get_str(const char *key, U8str *str) const;
	bool get_bytes(const char *key, DynBuf *dbuf) const;
	bool get_bytes_str(const char *key, DynBuf *dbuf) const;
	bool get_dict(const char *key,  IPDict *dict) const;
	bool get_ipdict(const char *key, IPDict *dict) const;

	bool get_int_list(const char *key, IPDictIntList *val) const;
	bool get_str_list(const char *key, IPDictStrList *val) const;
	bool get_bytes_list(const char *key, IPDictBufList *val) const;
	bool get_bytes_str_list(const char *key, IPDictBufList *val) const;
	bool get_dict_list(const char *key, IPDictList *val) const;
	bool get_ipdict_list(const char *key, IPDictList *val) const;

	bool has_key(const char *key) const;
	size_t key_num() const { return keys.size(); }
	bool del_key(const char *key);

	size_t pack(BYTE *s, size_t max_buf, size_t max_num=SIZE_MAX) const;
	size_t pack(DynBuf *dbuf) const {
		dbuf->Alloc(pack_size());
		size_t	ret = pack(dbuf->Buf(), dbuf->Size());
		dbuf->SetUsedSize(ret);
		return	ret;
	}
	size_t unpack(const BYTE *s, size_t len);
	size_t pack_size(size_t max_num=SIZE_MAX) const;
	void clear();

	IPDictMap *get_map() { return &dict; }
	IPDict& operator=(const IPDict& d) {
		dict.clear();
		keys.clear();
		for (auto &k: d.keys) {
			auto	&v = d.dict.find(k)->second;
			set(k.s(), v->Buf(), v->UsedSize());
		}
		return	*this;
	}
	bool operator==(const IPDict& d) const {
		return	std::equal(dict.begin(), dict.end(), d.dict.begin(),
				deref_map_eq<std::pair<U8str, std::shared_ptr<DynBuf>>>);
	}
	bool operator!=(const IPDict& d) const {
		return	!(*this == d);
	}

private:
	bool set(const char *key, const BYTE *data, size_t len, size_t keylen=0);
	bool set_core(const char *_key, DynBuf *dbuf, size_t key_len=0);

	size_t unpack_core(const BYTE *s, size_t len);

	size_t pack_size_core(size_t content_size) const;
	size_t pack_content(BYTE *s, size_t max_num=SIZE_MAX) const;
	size_t content_size(size_t max_num=SIZE_MAX) const;

	bool put_list_core(const char *key, std::list<std::shared_ptr<DynBuf>> &vlist, size_t len);
	bool parse_list_core(const char *key, IPDictBufList *vlist) const;
};

size_t ipdict_pack_int(int64 val, BYTE *buf, size_t max_len);
size_t ipdict_pack_str(const char *s, size_t len, BYTE *buf, size_t max_len);
size_t ipdict_pack_bytes(const BYTE *val, size_t len, BYTE *buf, size_t max_len);
size_t ipdict_pack_bytes_str(const BYTE *val, size_t len, BYTE *buf, size_t max_len);

bool ipdict_parse_int(const BYTE *s, size_t len, int64 *val);
//bool ipdict_parse_int(const BYTE *s, size_t len, int *val);
size_t ipdict_parse_str(const BYTE *s, size_t len, char *buf, size_t max_len);
size_t ipdict_parse_bytes(const BYTE *s, size_t len, BYTE *buf, size_t max_size);
size_t ipdict_parse_bytes_str(const BYTE *s, size_t len, BYTE *buf, size_t max_size);
bool ipdict_parse_dict(const BYTE *s, size_t len, IPDict *dict);
bool ipdict_parse_ipdict(const BYTE *s, size_t len, IPDict *dict);
size_t ipdict_parse_list(const BYTE *_s, size_t len, size_t *rlen);

size_t ipdict_size_fetch(const BYTE *buf, size_t size);

#define USE_VERIFY
#ifdef USE_VERIFY
bool ipdict_verify(const IPDict *ipdict, const DynBuf *key_blob, HCRYPTPROV h_csp=NULL);
#endif

bool ipdic_test();

#define IPDICT_SIGN_KEY			"SIGN"
#define RSA2048_KEY_SIZE		(2048 / 8)
#define RSA2048_SIGN_SIZE		RSA2048_KEY_SIZE
#define AES256_SIZE				(256 / 8)

#endif

