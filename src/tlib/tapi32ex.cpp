static char *tap32ex_id = 
	"@(#)Copyright (C) 1996-2015 H.Shirouzu		tap32ex.cpp	Ver0.99";
/* ========================================================================
	Project  Name			: Win32 Lightweight  Class Library Test
	Module Name				: Application Frame Class
	Create					: 1996-06-01(Sat)
	Update					: 2015-06-22(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "tlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>

NTSTATUS (WINAPI *pNtQueryInformationFile)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG,
	FILE_INFORMATION_CLASS);

NTSTATUS (WINAPI *pZwFsControlFile)(HANDLE, HANDLE, void *, PVOID, PIO_STATUS_BLOCK,
	ULONG, PVOID , ULONG , PVOID , ULONG);

BOOL TLibInit_Ntdll()
{
	HINSTANCE	ntdll = ::GetModuleHandle("ntdll.dll");

	pNtQueryInformationFile =
		(NTSTATUS (WINAPI *)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, FILE_INFORMATION_CLASS))
		::GetProcAddress(ntdll, "NtQueryInformationFile");

	pZwFsControlFile =
		(NTSTATUS (WINAPI *)(HANDLE, HANDLE, void *, PVOID, PIO_STATUS_BLOCK, ULONG, PVOID,
		 ULONG, PVOID , ULONG))::GetProcAddress(ntdll, "ZwFsControlFile");

	return	TRUE;
}

TDigest::TDigest()
{
	hProv = NULL;
	hHash = NULL;
	updateSize = 0;
}

TDigest::~TDigest()
{
	if (hHash)	::CryptDestroyHash(hHash);
	if (hProv)	::CryptReleaseContext(hProv, 0);
}

BOOL TDigest::Init(TDigest::Type _type)
{
	type = _type;

	if (hProv == NULL) {
		if (!::CryptAcquireContext(&hProv, NULL, MS_DEF_DSS_PROV, PROV_DSS, 0)) {
			::CryptAcquireContext(&hProv, NULL, MS_DEF_DSS_PROV, PROV_DSS, CRYPT_NEWKEYSET);
		}
	}
	if (hHash) {
		::CryptDestroyHash(hHash);
		hHash = NULL;
	}
	updateSize = 0;

	return	::CryptCreateHash(hProv, type == SHA1 ? CALG_SHA : CALG_MD5, 0, 0, &hHash);
}

BOOL TDigest::Reset()
{
	if (updateSize > 0) {
		return	Init(type);
	}
	return	TRUE;
}

BOOL TDigest::Update(void *data, int size)
{
	updateSize += size;
	return	::CryptHashData(hHash, (BYTE *)data, size, 0);
}

BOOL TDigest::GetVal(void *data)
{
	DWORD	size = GetDigestSize();

	return	::CryptGetHashParam(hHash, HP_HASHVAL, (BYTE *)data, &size, 0);
}

BOOL TDigest::GetRevVal(void *data)
{
	if (!GetVal(data)) return FALSE;

	rev_order((BYTE *)data, GetDigestSize());
	return	TRUE;
}

void TDigest::GetEmptyVal(void *data)
{
#define EMPTY_MD5	"\xd4\x1d\x8c\xd9\x8f\x00\xb2\x04\xe9\x80\x09\x98\xec\xf8\x42\x7e"
#define EMPTY_SHA1	"\xda\x39\xa3\xee\x5e\x6b\x4b\x0d\x32\x55\xbf\xef\x95\x60\x18\x90" \
					"\xaf\xd8\x07\x09"

	if (type == MD5) {
		memcpy(data, EMPTY_MD5,  sizeof(EMPTY_MD5));
	}
	else {
		memcpy(data, EMPTY_SHA1, sizeof(EMPTY_SHA1));
	}
}

BOOL TGenRandom(void *buf, int len)
{
	static HCRYPTPROV hProv;

	if (hProv == NULL) {
		if (!::CryptAcquireContext(&hProv, NULL, MS_DEF_DSS_PROV, PROV_DSS, 0)) {
			::CryptAcquireContext(&hProv, NULL, MS_DEF_DSS_PROV, PROV_DSS, CRYPT_NEWKEYSET);
		}
	}

	if (hProv && ::CryptGenRandom(hProv, (DWORD)len, (BYTE *)buf))
		return	TRUE;

	for (int i=0; i < len; i++) {
		*((BYTE *)buf + i) = (BYTE)(rand() >> 8);
	}

	return	 TRUE;
}


#define THASH_RAND64_NUM1  757 /* prime number */
#define THASH_RAND_NUM1   1511  /* prime number */

static uint64 rand64_data1[THASH_RAND64_NUM1] = {
0xf94e994dccb35a90,0xdc9a937eedd45ea8,0x953d9880d5b75beb,0xbf844f3220e6f8b3,
0xcb9b4968a4f1a69d,0x9fd75890743e7741,0xbfb6ca16b52742f3,0xce968df52dbcea5b,
0xf8a1299408873da7,0xf05b9a3bf9a8b872,0x2e756a5775529f35,0x6ab9747413b10bb9,
0xed6602acbebc1df7,0x6bf6bc7ec35ce856,0x239135ae63eb454f,0x1d989ca1ab6cd659,
0xda159917aa7bf72a,0xee29f489922955a1,0x6ce88182b31becd7,0x13a3645adb42daeb,
0x5180132639b2d7ef,0xda2b998389f0d96c,0x967ba400222d8898,0xdc0ad795dd3d7c9d,
0x1834e71ccf4b2ef0,0xc3fb51c7aac1873c,0xf0e82ce1fbc556df,0x60292b7f57bd6c78,
0xf293ce17611ba16a,0x47e5cacf904ea1b3,0x971a89e40d48f53a,0xe4fb6b7ae9c383f6,
0xdc3a141616002e74,0xab0f9dc0edc23ec6,0x1eaeba5930a82e6e,0x477549a425669153,
0xc74f6aaf04738907,0x04345debf73174c9,0x93692ae79e064b6a,0xd5386ec514da58f1,
0x6c8ed81ebd839ecb,0xe2f9aa4f34a89d87,0x9c56eb6186728084,0x928e99a2a0e3292b,
0xfc352f614364ae2f,0x8d510adb974c1dcd,0x6fef8a327e5cfc07,0x2fcf7cead8fc1d8e,
0x4404941571cba4d8,0x78ea5eab803ec402,0x9e2d4cc3baa3e8a1,0xd39453b9922a69db,
0xef7027143dde84b1,0x3949cb81a1c557ba,0x01b79bda9c1914e6,0xf59d0d1bfa1dc068,
0xe42c05f685a3e193,0xf30d1e35c056ca8f,0x5e97f74593a9866b,0xfa2f187161fd83ec,
0xfe752f06740a1844,0x84d6f1cecb502bba,0x3dbc988a3f67e187,0x43f3a3fbd1acd7c8,
0x8fd974bc60b62015,0xe1fa1d66910d4e7c,0x7e8ca5be5f5559c1,0xb2378eef06749742,
0x9d5f93ed7efa5566,0x9d51ca413291a0ff,0xd3f7ccdb334c9c55,0xb626c8990dd4be03,
0x5eb2dad04a231b59,0x542be31580c7327f,0x53b2c15ee0001e9b,0x766f52a0fcbcb7b3,
0x4cb4b7e2101300f2,0xdb3811d63d63cb38,0x79b36d301adb6c19,0x64452b5236562375,
0xe8613ecc8feaa4f5,0xdd893d4468c523ec,0x9dbccb434c5cc152,0x8fe098773254f675,
0xcbed29795a168b53,0xe97e5f516104e373,0x047001b3e9206ead,0x84b3e6fa22d90752,
0x0e61fe8edfa6157d,0xbe95d43eebfd3f3b,0x5a0f9e39533c9bde,0x342eb9b1ef04e6a4,
0x328f0e9046361e41,0x33b3ce643b94f7a1,0x7ac75cfaaddcb3c5,0xde08acbb1303d232,
0x06e809db8cbb2e64,0xb0d8b5d0e5b2048e,0xfc6290893ee139f2,0x0ed79b1955069ab8,
0xc7d990d25fc73f99,0x38746e4fc7f3bfe7,0x098bcf76f49c7bf4,0x5350eb70c7c64ec4,
0x3b7e01d28e844151,0xaa4da9e1de9c2874,0xe4b3370e58d811fa,0xf458dbb39ce9829d,
0xd8c20cb797f3655a,0xadb27a5c3e979073,0xa8efe4e50dd48796,0xba719ec38044d80d,
0x2c132443207f94b8,0x740f111c17e119fc,0xace9962adf9c1ec2,0xcf60316cc397d5ae,
0x0680cf766395ed4a,0x767e628a964fcf93,0xc26def1c1ceca6cd,0xa6ed7111e96f2acc,
0x7d640ff26a23b8cd,0xfcd42600020ea8cf,0xdc53b63587258ad9,0xfd0ea182e9a02f72,
0xfd7d13a280ed0737,0x542893aa846da01a,0x05b0ddf0b11bcba3,0xaf84b59a650c0d02,
0x9dc076441c7473b5,0x0d5cbada08723c23,0x2851f311b9156d30,0xd2295a670afc908d,
0x0be83844a670759f,0xda8f88f5add301cf,0x804945ad6fc317ba,0xdd6ff2e635f52694,
0x4db03421a4eab685,0x90948523dc3ee702,0xab14f6738fdd8e6b,0x2484eebb1fb1869a,
0x765504e2b90c957b,0x61367459c314333f,0x621f482111c90a63,0xd8a9aaa984e88a9a,
0x0ff6a57ef0a1990f,0xa1413e588c2cb8f6,0xff8e1aacb01daa82,0x378b78ca88b8d0cf,
0xd3f66e8cbd342075,0x56f40bd28c783f7c,0x1d110d9c6e1868eb,0xf4dbffd333406c8d,
0xea2dfdf8d1e65540,0xbba57d7f06f33378,0xf599350c28e7a639,0xc88f94b07489b620,
0x0e41cbaeef7d236a,0xdc52189f82ddd1bb,0xd570d282856762cf,0x6c265ae7f4db6118,
0x438d7220120589ec,0x5467f88e2a3e881e,0xce263ca42f66ae32,0xe66fbf424678c96c,
0xcd104745ce9de826,0x181e8fcb80445459,0x385ac9738ef3c2e0,0x8d38c67c521f0c24,
0x1b981b4203e71932,0x8517f192916a30bc,0x2ec3acec65fcc572,0x547f1a59a8d4cb38,
0x20a98b4f3cce42e1,0x4b32ad113b5e21b6,0xeef9bcd641c6af05,0x856f40617eab8cd7,
0x3957d93e756cc374,0xd7f639af6b2c0c01,0xbef9a9524db04c91,0x7a9565839bb45b11,
0x6472115d29563cda,0xfddd539ae4e9588d,0xf90c5848981caab5,0x74723a2572034669,
0x395ef736e58cf065,0xdeb9ef6c67db0edc,0xf2ca0604dd1ae5f0,0xf033fb1426bbbf2f,
0xced47c84e8711d92,0x6b52a838a7f395ff,0x73bf716c4459a759,0x751c7351d7bdaf11,
0xfacfb386ecb4ebdd,0xbf8c7c67d2e9d90e,0x7e5aa0ac75ceb2de,0x33902f5662d2f91c,
0x573cd27426ecfb1a,0xbde5fb75cc3f72d7,0x15bf0f8a73168b56,0x26a243b19c593c48,
0x6921c27795641c47,0x67c92bf684ccf2c6,0xa05f70aa8291fd67,0xc2fdddf1df56116e,
0x797c1c13d129a77b,0x62984591a779a27b,0xdd29f8564d595356,0xfcf6e68ee47636ff,
0xb47cd58f1c19cd0b,0xa78d47c5fee16043,0x258cc0a7ad474a28,0x35adfff1886f4623,
0x955c6434d0a3030c,0xfe548949803c3897,0xa893654663c59cb0,0x041d64655f8b80f1,
0x2afb603aa5f4a6cc,0x9bf001b773d099a3,0x774c77c231c195e2,0x4d61c8b4d4eebfee,
0xf56175a9c3d97f8b,0x9c713458a017484f,0x7a5f8f9de7f0bfab,0x6ed283287e8d1e65,
0x74b076c38e686077,0xa5bbb5dcacc4326c,0x464161f6ec882929,0x0156a785a9c8da2d,
0x42103a0290b9117e,0x7073a32135715b12,0xfb2ed039f55ad275,0xc15b6c63dbf2240c,
0x4502983f36f99863,0x47eaeefd33de3418,0x5256e739c77010fe,0x4b918848148a97d5,
0x5a8f86a34585aa9c,0x6652773d0bee8c18,0xf237375628063833,0x5936f2ec8b51a6f2,
0xc5acf97262da16da,0x704a28315834551f,0x846ed7ea90dadc5c,0xda53b68824716e69,
0x2044b46ed3e542b8,0xb230432a4888201b,0x2be318e49b9f8d32,0x9991af3c00710bbd,
0x527b43adc7e34176,0x1605d8ae39811cf9,0xc30f9f91364d946e,0x878d20046ec7b75d,
0xa9aab2b3dcd37213,0x38cf9fcb07d35477,0x23cdaf43220fbc78,0x80408ed1ddee100d,
0x8b8cc9f6b68b0fc8,0xe0f0000946f25005,0x8e0ff1c2c455dd62,0xb6982a08ebd7ae1c,
0xa0785ffd9cb6d63d,0xfcda177694c24cb5,0x441700e997515038,0xffdba2629576e5dc,
0xbd4578a353ee2ae3,0x3bfb483d46b20e76,0x3868925cc4a8eedb,0x25871c18d9f3ad89,
0xac5c3adf23f41150,0xdf476194fbf9b10b,0x2cd0fe5f8151748b,0x4f25d538eb3be9a2,
0xe7affab6f2ddafe2,0xc7668e2d52b2a2cc,0x26d5d1864e4a8a1d,0x9b4734f128d74e4d,
0x52287dfe52295cb6,0x6f9747963e1f31f6,0xd6651d9638958ada,0xe3f90de82db9d5e5,
0x700b83154d4afdcb,0x72220e0a4d13fb87,0x6d90dbfea1aec676,0xcca0b781c2e92ee9,
0xc40c0ffaa0a50805,0x961fc26b45234a9b,0xb0cc92a3e759103c,0x2a010165b5ae5cbb,
0x0308eb6beee9ae93,0xaf91ba920adf2f80,0x8defd70bf63fd0e1,0x59aac216cbe465fa,
0x2c786cd660480ae4,0x8af3df7d4cddbe9f,0xb48d88352f52754d,0x94b9b9451650a025,
0xace101b13957ad4f,0xb606d2ee56defb09,0xeff536918882a662,0x063e0db326e4553d,
0x6885af46b58a2db3,0x57ba00cf203f8f27,0xf087a49fe5c94e54,0x0c0f40b14e91d20e,
0x0378d5e7ef3f2e7a,0xa8caa12b2553e81e,0x0ba15fbb43e2da01,0x4e11aad4f3ef574f,
0xe9461613febc6556,0xdb9941662c304f35,0x7ab36d8a4bce30b4,0x9d0d426640f7ef50,
0x56f157c506c2fcb0,0xe01c32287206053f,0x42da40ac93f73612,0xa36c8c4108328adf,
0x4d492dec1104ee71,0xe379dc834806bfcd,0x751accb97aaad067,0x0e9f8ddbb794a5d7,
0xb96a803f99ca6ac6,0x3ab85d36ccbfb04b,0xb286ff59690d7f8d,0x00a44fb861222fe2,
0x909bac32578eaa29,0x2869786935f9db50,0xf47aecf98dd03fea,0xad8ca1599653ade5,
0x72a4065f04562f99,0xf2dd1e136e2d6cc0,0x7bec56943aa71de9,0x50d97e32c3749488,
0xad7102a195790bad,0x19e406170cd03b57,0xa6c185d99f9e2707,0x77e5fc459c295b87,
0xd189c27b94b6d311,0x7050ea944dbda218,0x0085281809600528,0x736f353accd71782,
0x19b1a64f0205ee41,0x53c3d998246fc05f,0x8ed29dc00b41ea9f,0xed9845d17fab1d45,
0x4a9be5109cdb1673,0x398c32e6ec0cdb9e,0xf85e9a0a2aa1ae6f,0xa88695ab7da4764e,
0x7db01a7fd0f7f8c7,0xf480b6da5918d6f3,0x589115241cf6a721,0x5d7cb9a30e9807b3,
0xbeb2a6947374201b,0x5773d27aeefdd8aa,0x7dbd098d60cd30bc,0xebee52262747c5e0,
0xde668b1dd8b8e436,0x4e3a0bacaa81e203,0x29aef491f0ac71a0,0x8578f4e0a8766e78,
0x401be1107c66f151,0xa874298fc71b40e8,0xe17ca3885c53c32e,0x519012055737a7bb,
0x10000484b6b4605d,0xd83de01080ef01fc,0x0c2cb288a5a7fa7f,0x385c723752799912,
0xef9eede066555cd4,0x579b3b3aef9b24bb,0xf55f9f5283315078,0x25570e9a5b364ba5,
0xd5da17e6d268b0ad,0x4c2aa2024c10c4c9,0x8ab9b3a9ecf92ba7,0xd16c137cf6004ec7,
0x6853c02acaf2f58e,0x31243d993f231323,0xf2a549b336651755,0xcbb54cd44c615915,
0x115c63e090a4bb01,0xa3828f6717e5dbfc,0x5881c03ac9daa329,0x580c3b216febbcb2,
0x1cf2de36f93942cd,0xe038280cf7050b43,0x75f3ed08d6692fbb,0x429b3aec6fe6f020,
0xeb78cf53ef5dad3f,0xeb2c162c3a34fcc3,0xd724a9bfa8b52e41,0xd33ecbb3db8e015a,
0x89cbce848a5a5886,0x4d83a75dd9d42509,0x05833fdd14db8b2c,0xfd7287de0adf2a0b,
0xafc6f833f906fbf0,0x48390fd5834dda45,0x1c332be357cbd31e,0x53f60cb6c2ff466d,
0xabdf90b1a0f1f892,0x8e5a56683a9f54b0,0x3fb0ef49babe5bf8,0x90232c7a698571b7,
0x95349d0dbe20240c,0xf61716b31decb496,0x923bd1b2e2eaac36,0x9a3ee201e7f5d025,
0xec955bb0a4405724,0x5693ec71a44ca9da,0xc5c4e52c36f48192,0x3541c5f2ce5efde0,
0xa4adc7307b8f67c2,0x1ab96cc4efb302f0,0xdb0d1fc9ce8a0240,0xde1ee6990aad7d77,
0x32abee93170b1d3d,0x68a75a177a5e6dfd,0x1d75fb491cd7417a,0xf9d3ad5dd1d6df76,
0xc618d949059c4f4d,0x2caaad0d4977abd2,0x99700d9cf55136e7,0xa4c08eb6cadfac74,
0x6376de0dae47d660,0x55ed202483bf76ce,0xcf8497b1c0699032,0x81c2176027a93444,
0x652e8d3ff04bace1,0xdf3c5ce8256cd17c,0x55033e0bbbb9625e,0x18f993a54012d7ed,
0xe543fa112b4700c5,0x12a244cad647a5fc,0xd1865e344d04325f,0xf525b8c576a49434,
0x4075eebea0ecb707,0x1d04837ac25678ac,0x2c593846368772d5,0x092fdd5a5212aa86,
0xc0a79eb36324dd78,0xa2920c37912dd3aa,0xb42426ca5239366e,0xc55203854232b811,
0xa60856555048e21e,0xd07beeeb39666544,0xa738c8e79c275c08,0x25bfdc48fac893a2,
0xa27bcb9a314b38bc,0xfc850a98f59b082f,0x1c46d1cdb9476375,0xbd79626cfbc1c320,
0x5e9d55bc5571c168,0xea80854faa1cd775,0x09f4af2817b90244,0x050c70c5cc056048,
0xbe95835030b88c76,0x346a21e025a37806,0x63474b3eaab9ab45,0x8398cff414c98259,
0xbf5d0f4cb0896351,0x612d130e42bc3d3d,0x9691306bad6920cc,0xb6cf8fcbb8333fa9,
0x12888fc2cc76fb22,0x021318b9c43e78c1,0xbbdf09af1424beec,0xaa175a9b222edec6,
0xc1fd4f6e3faf2613,0x377a8f26d6ba6117,0x64958b1629fc0bb6,0xd676e76b7d1c7196,
0x719f7b593a5f3e8a,0xf3612108cd62f07e,0x7f3bc383fe9249f2,0x873d60e6ea8d5e65,
0x510a4d25ec4c2e44,0x9ff3bc1108984b96,0x28b4ed93b886a236,0xc6d7c7a1f07e7ca4,
0x1ac102db98e965f3,0x2b0a155936a1c224,0x39575213dd13ce33,0x19ff5fe560044936,
0x4170927ec047ebf6,0x63b40ce32f5609b0,0x06df1a6b7b109584,0xb9329ce96e7ad4e7,
0x2cddb65e4266c58b,0x82af8fd3d0b19e89,0x36c3b3eac5a315a7,0x2c81827ff5998da0,
0xfa7645913f5831ff,0x365b4a217e08e125,0xfb10b2ccb70a3468,0x8fa2601ed51461c7,
0xc8b5eb23eb93c3dc,0xb9e9a4241bdff8fd,0x2d99191c7ff05e8a,0x3901a8cd3fb34c4d,
0x6a8f94a5c9f4d3c5,0x220bcdb0816610f1,0xbee09aebf9f394a5,0x050da3ea1bcf50aa,
0x47e2f37b01ce9778,0x2987811f4199c5bc,0x7befbad371ba4a26,0x8ab41468bad1d70c,
0x0e9e85bc3ea66a04,0x40c650ad790e1b30,0xd7b07537ca2a59af,0x7896cc39ffb25f31,
0x8f8029bf51e918cd,0x0c1b4612f3954429,0xb816d1dbfa7c0bc2,0x6c85b663cea4b77f,
0x32ddae28459192d5,0x1d910369206bbfcd,0x76659ad324900613,0x61211b6145f49252,
0x2a9bd7564a51432f,0x0d8195836c5fd0c0,0xfe1f9353b6f15c96,0x74131826e29464f2,
0xa42b531c2fd45ff4,0x3fc70d49f2437baf,0x851f61ea20988397,0xbab58ec5c5313aab,
0xd4c1782530b7c74c,0x38e5df03f5e573c6,0x38e0982c929ba8ce,0x6cb249b54cd5d188,
0x873ef452c50a0639,0x0247d11d5e975daf,0xd148158c3b6fe807,0x89b445edfd67210d,
0x4a4de893c1e8f23d,0x9c4158339663778f,0x65fbeac27e01f545,0x0cda16650478c096,
0x7f9595f8a7af7bd6,0x08788e40ada7f5a0,0x18994123edc6b487,0x3141b365637cca51,
0xa7314a9b2b12f337,0x1f547a76a24df2a1,0xb38a927646fac9bc,0x3831431b5775ed3e,
0x7a9f64c60cc261a8,0x7206fe10d0056906,0xf4a5b006068b8586,0x5681945f669270fb,
0xded5070922707e73,0x740f6d4d85111736,0x570f9274d81dd50a,0x570456a16b579f45,
0x766de5428bfd2280,0x6d56e7194c1c8f3d,0xd62bf2e85217996a,0x049f8b040600ac6b,
0x0381d9ff059910d6,0xa99834a7d9698694,0x5afe2c449d6168f3,0x319cac76f1782412,
0xc460f022b6d8fb1a,0x8f5d99f84bf977bf,0xc35ed175986396be,0xe69bb738d296c8a8,
0x59fe2af1305103ab,0x5a5a8bf4f57b1691,0xd035aa7f43943dc4,0x4164e59d66cb3817,
0xb77c8291ae8d69f8,0x23135d0f8f25e728,0x0bbff56f8e1be2aa,0x7e532a3bd4d13d19,
0x44073d07da284145,0x39cb218fff5ca6ee,0x7ea6e74827290366,0x001000ae7c065055,
0xe7b9fa7626ccd8cc,0xf83c67aa155bf26f,0x8ede36527af1dc64,0xcdc8ebc8a512fabc,
0xd1160f1b2ff293f5,0x9b2a806449fe32e7,0x3088629e9c3d9bf8,0x8c63cdee88764db7,
0xe42fc99b4fd1fe86,0xd1e524b8d045a507,0x9c7877249ad8913d,0x070df02973ebd8b1,
0xf2e6eb7da48e9682,0xb09f01e2d59bb1d0,0x331e238d322bbba6,0x94d794725f843f0a,
0x3c41ec2d803a4063,0xa7ae6b3c4888d282,0x4f445dc09476e7c8,0xadf0c7c5454361db,
0xa6878e57345cef0f,0x5e1a0200b18cfa6e,0x3027e332c59870bf,0xde192c0a38d7d092,
0x90d1e8bbeb24e3fb,0x1773cd2063e50625,0xc9cb07de275263f7,0xff8b09a3d093b1e2,
0xafc661bb357f7d17,0x3e9c43ac623213e9,0xa92e5b218f402417,0x0a2b70c94a7ebde0,
0x248c1b1f80cf2b7d,0x9fa050778b006ae6,0x509e221127e51a79,0xedcb1455d872fbf3,
0xdf4789a9f8345f64,0xea6ef78e04905daa,0x32df3b41f88fc8c6,0x2b1e7cd89a8b6118,
0xf7110fb57062aedf,0x5fa1bfe9e1f4558f,0x7938ca39fb4ebb4b,0x2406d2300e5a2f8a,
0x8aee660ff27cb60c,0xb96700861572963d,0x2ab9020cc9fe338c,0x1aae5ba748a99b0f,
0x87a312e2be607db3,0x63937052353940cb,0x53d2da6327aafdd2,0x4db4735c6c7f4f89,
0x0dfef99846a359db,0x87e4e9c393c1ae5c,0x0d4a19c99400ea39,0xbc83789a0f67ce35,
0x7e239e728c3cd18c,0x974be6f1114deeed,0x73772532d011f399,0xb8d1064048d242eb,
0x1dc32bab6d41b8dc,0x53cc57d70eef54c8,0x46133da93e422971,0x7551b4edd2305859,
0x0d64ae959dc059ea,0xab86ac8333142d54,0x42ea79db9e7c8f82,0xfa330a3c153307c9,
0x070b0054c2aadeb7,0x13be45b6bca2001b,0xb3c138f43f340984,0xea7bf52e4aaa19d0,
0xef199ee294c7b76d,0x0ba0e8e1eebeb7a2,0x0895b2257fee9661,0x5e5ef9241c9ad64c,
0x9fed573528ad946a,0xcc002e80d28425e5,0x77184751c10da8d4,0x12b170f68373e24c,
0x642eb7987adb39ab,0x49e7478f2f0c42a0,0x71386403df6c7f85,0xe87acd588a0d7669,
0x679ec80ef5e8b38b,0x198f3ff3df1267fd,0x39a4fc0c821dde57,0x71dac1d372c19fe0,
0xfabe81ef9b58e750,0xc78c1415d098ae58,0xef9c330c9d34b32c,0x0e24d57f90d496bf,
0xef79789e9d71cd79,0x62710696ea4fdea0,0x773924691143d179,0x82db2f2526fbe8c1,
0xb325497e1b4a90bb,0xfe9304f98ee3bb41,0xbe2e58479df0f708,0x335f8938bdecba69,
0xbaa597609a0ca000,0x32346b5263cba637,0xed9a6789389244aa,0x7f5e4926e9df2f96,
0x776854849f751295,0xfb2d5eec54902835,0x0bcbe82b8e408e9e,0x77ebe180abd26ffc,
0x8eb59fd87feedadf,0x648a7a5d1c92f8f9,0xef06fd9b0135aeb2,0x83598fd525f186e6,
0xa244c858b7d2f310,0xef0f7f5c1646aae4,0xf3a5275dac970378,0x326824c8a20770d3,
0xff80a534071c2e99,0xa7e6dd364690ba7e,0x881945e6447f3b13,0x33b6fe9dd2715286,
0x49ab77a187a34c86,0xad762de84d9f01a4,0x13cba201636ce66a,0x8b194d89315f0019,
0x0547b052e13bd4d1,0x0196528ca0b502f2,0x26dabd8f479c2b77,0xbb7eb9a4e65c3483,
0x738ba3d5953c9b83,0x49ebbb63243017cc,0x0e1241cca98d1ba7,0xa1e738cf2b57a40f,
0x1fbc5910769819d4,0xaa9065ded7bd3b79,0xd91f0e57bda3074b,0x752730f8ff7a67fb,
0x77a993828c4ee46a,0xf0c17fac42fa5ef6,0x02508b82d57c7cb3,0xabb15135c7767811,
0xf638a834b5e3374d,0x20ff07c78774e409,0x65b782fa01805410,0x5fd101d2ebf394ca,
0xf754a5715e30550c,0x919cd0ac1f74b262,0x20320d27ce289f69,0xec7e4e343a0510a4,
0x7e54ba43959629b0,0xce0a9c8581ed30db,0xdaa5dd7ec7f502b9,0x15c42fe0720f0f55,
0xf6bf3b5ab398eaa8,0x6e78bc44fa749f97,0x9103fa5bebb06a29,0xdc224779d5d48e31,
0xa4e9c34f7da41137,0xd8aafd49e96b9b63,0x7947b1964c92686f,0xa6ce1b4e260857fd,
0x6fcaa7821c045329,0xf1050f2de0e5c234,0xd994e5216e04b9ab,0xac4cfc059be78d48,
0x2e7e1f4ed248e706,0xaead0b52adaea623,0x8cda73a5057aa0f3,0xeafdbfeeca04a67a,
0x04056e7807d08686,0xa104625e92496aa7,0x8cba896306405180,0x606d15153e1a3c2f,
0x6b6ff26eb8dece9a,0x28761e3fbefcb7a0,0x242537bd9d518281,0xe698d7ddbec4e01a,
0x2c51f1c4b6d4d26f,
};

#define rand_data1 ((u_int *)rand64_data1)
/*
	手抜きハッシュ生成ルーチン
*/
inline u_int MAKE_HASH_CORE(u_int sum, u_int data, int offset) {
	u_int	seed = (data ^ 0x3c0f9791) * 0x7b2fcbc3;
	return	((sum << 25) | (sum >> 7)) ^ seed
			^ rand_data1[(seed ^ offset) % THASH_RAND_NUM1];
}

u_int MakeHash(const void *data, int size, u_int iv)
{
	u_int	val = rand_data1[(size ^ iv) % THASH_RAND_NUM1] ^ 0xe31a021d;
	u_int	offset = val ^ 0x8f8e053a;
	int		max_loop = size / sizeof(u_int);
	int		mod = size % sizeof(u_int);
	u_int	*p = (u_int *)data;

	for (u_int *end = p + max_loop; p < end; p++) {
		val = MAKE_HASH_CORE(val, *p, offset);
		offset+=11;
	}

	u_int	mod_val;
	switch (mod) {
	case 0:	mod_val = 0x44444444;                         break;
	case 1:	mod_val = 0x11111111; memcpy(&mod_val, p, 1); break;
	case 2:	mod_val = 0x22222222; memcpy(&mod_val, p, 2); break;
	case 3:	mod_val = 0x33333333; memcpy(&mod_val, p, 3); break;
	}

	return	MAKE_HASH_CORE(val, mod_val, offset + mod);
}

/*
	手抜きハッシュ生成ルーチン64bit版
*/
inline uint64 MAKE_HASH_CORE64(uint64 sum, uint64 data, int64 offset) {
	uint64	seed = (data ^ 0x57ff7a3ad28cb573) * 0xda5174b5a1d8414f;
	return	((sum << 47) | (sum >> 17)) ^ seed
			^ rand64_data1[(seed ^ offset) % THASH_RAND64_NUM1];
}

uint64 MakeHash64(const void *data, int size, uint64 iv)
{
	uint64	val = rand64_data1[(size ^ iv) % THASH_RAND64_NUM1] ^ 0x313b88a34190944b;
	uint64	offset = val ^ 0xa0f643cccf82b318;
	int		max_loop = size / sizeof(uint64);
	int		mod = size % sizeof(uint64);
	uint64	*p = (uint64 *)data;

	for (uint64 *end = p + max_loop; p < end; p++) {
		val = MAKE_HASH_CORE64(val, *p, offset);
		offset+=23;
	}

	uint64	mod_val;
	switch (mod) {
	case 0:	mod_val = 0x8888888888888888;                         break;
	case 1:	mod_val = 0x1111111111111111; memcpy(&mod_val, p, 1); break;
	case 2:	mod_val = 0x2222222222222222; memcpy(&mod_val, p, 2); break;
	case 3:	mod_val = 0x3333333333333333; memcpy(&mod_val, p, 3); break;
	case 4:	mod_val = 0x4444444444444444; memcpy(&mod_val, p, 4); break;
	case 5:	mod_val = 0x5555555555555555; memcpy(&mod_val, p, 5); break;
	case 6:	mod_val = 0x6666666666666666; memcpy(&mod_val, p, 6); break;
	case 7: mod_val = 0x7777777777777777; memcpy(&mod_val, p, 7); break;
	}

	return	MAKE_HASH_CORE64(val, mod_val, offset + mod);
}

//#define HASHQUALITY_CHECK
#ifdef HASHQUALITY_CHECK
extern void CheckHashQuality();
extern void CheckHashQuality64();
#define MAX_HASH_TBL 10000000

void tapi32_test()
{
	CheckHashQuality();
	CheckHashQuality64();
}

class THtest : public THashTbl {
public:
	virtual BOOL IsSameVal(THashObj *obj, const void *_data) { return TRUE; }
	THtest() : THashTbl(MAX_HASH_TBL, FALSE) {}
};

class THtest64 : public THashTbl64 {
public:
	virtual BOOL IsSameVal(THashObj64 *obj, const void *_data) { return TRUE; }
	THtest64() : THashTbl64(MAX_HASH_TBL, FALSE) {}
};

void CheckHashQuality()
{
	THtest		ht;
	int			col=0;
	THashObj	*obj = NULL;

#define MAX_HASH	5000000
//#define HASH_SPEED
//#define HASH_MD5

#ifndef HASH_SPEED
	obj = new THashObj[MAX_HASH];
	const char *mode="col_check";
#else
	const char *mode="speed_check";
#endif
#ifndef HASH_MD5
	const char	*hash_name = "hash32";
#else
	const char	*hash_name = "md5";
	TDigest md5;
	md5.Init(TDigest::MD5);
#endif

	Debug("Start %s mode=%s num=%d\n", hash_name, mode, MAX_HASH);

	char	buf[500];
	memset(buf, 0, sizeof(buf));
	int		len = 500;
	DWORD	&val = *(DWORD *)buf;
	DWORD	t = GetTickCount();

	for (int i=0; i < MAX_HASH; i++) {
#if 0
		len = sprintf(buf, "012345678901234567890123%08d", (int)i);
#else
		val = i;
#endif

#ifndef HASH_MD5
		u_int	hash_id = MakeHash(buf, len);
#else
		union {
			u_int	hash_id;
			char	data[16];
		};
		md5.Reset();
		md5.Update(buf, len);
		md5.GetVal(data);
#endif
		if (ht.Search(NULL, hash_id)) {
			if (col < 10) Debug(" collision val %08x (%d)\n", hash_id, len);
			col++;
		}
		else {
			ht.Register(obj + i, hash_id);
		}
	}
	Debug("hash col=%d of %d (%.2f sec)\n", col, MAX_HASH, (GetTickCount() - t) / (float)1000);
	delete [] obj;
}

void CheckHashQuality64()
{
	THtest64	ht;
	int			col=0;

#define MAX_HASH64	5000000
//#define HASH_SPEED64
//#define HASH_MD5_64

	THashObj64	*obj = NULL;
	char	buf[500];
	memset(buf, 0, sizeof(buf));
	int		len = 500;
	uint64	&val = *(uint64 *)buf;
	uint64	hash_sum = 0;
	DWORD	t = GetTickCount();
#ifndef HASH_MD5_64
	const char *hash_name = "hash64";
#else
	const char	*hash_name = "md5";
	TDigest md5;
	md5.Init(TDigest::MD5);
#endif

#ifndef HASH_SPEED64
	obj = new THashObj64[MAX_HASH64];
	const char *mode="col_check";
#else
	const char *mode="speed_check";
#endif
	Debug("Start %s mode=%s num=%d\n", hash_name, mode, MAX_HASH64);

	for (uint64 i=0; i < MAX_HASH64; i++) {
#if 0
		len = sprintf(buf, "str___________________%08lldn", (int64)i) + 1;
#elif 1
		val = i;
#else
		val = (rand64_data1[i % THASH_RAND_NUM1]);
#endif

#ifndef HASH_SPEED64
#ifndef HASH_MD5_64
		uint64	hash_id = (uint64)(MakeHash64(buf, len) >> 32); // 32bit空間でテスト
#else
		uint64	hash_id;
		char	data[16];

		md5.Reset(); md5.Update(buf, len); md5.GetVal(data);
		hash_id = *(u_int *)data; // 32bit空間でテスト
#endif
		if (ht.Search(NULL, hash_id)) {
			if (col < 10) Debug("reduced val is %016llx (%d)\n", hash_id, len);
			col++;
		}
		else {
			ht.Register(obj + i, hash_id);
		}
#else
#ifndef HASH_MD5_64
		hash_sum |=
			MakeHash64(buf, len)|MakeHash64(buf, len)|MakeHash64(buf, len)|MakeHash64(buf, len);
#else
		union {
			u_int64	hash_id;
			char	data[16];
		};
		md5.Reset(); md5.Update(buf, len); md5.GetVal(data); hash_sum |= hash_id;
		md5.Reset(); md5.Update(buf, len); md5.GetVal(data); hash_sum |= hash_id;
		md5.Reset(); md5.Update(buf, len); md5.GetVal(data); hash_sum |= hash_id;
		md5.Reset(); md5.Update(buf, len); md5.GetVal(data); hash_sum |= hash_id;
#endif
#endif
	}
	Debug("%s mode=%s col=%d of %d (%.2f sec) %llx\n", hash_name, mode, col, MAX_HASH64, (GetTickCount() - t) / (float)1000, hash_sum);
	delete [] obj;
}
#endif

#if 0
void makerand() {
	DynBuf	buf(2000 * 8);

	TGenRandom(buf, buf.Size());
	uint64	*p = (uint64 *)(char *)buf;

	if (FILE *fp=fopen("c:\\temp\\rand1.txt", "w")) {
		fputs("uint64 rand64_data1[THASH_RAND64_NUM1] = {\n", fp);
		for (int i=0; i < 757; i++) {
			fprintf(fp, "0x%16.16llx,%s", *p++, ((i+1) % 4) == 0 ? "\n" : "");
		}
		fputs("\n};\n", fp);
		fclose(fp);
	}
	if (FILE *fp=fopen("c:\\temp\\rand2.txt", "w")) {
		fputs("uint64 rand64_data2[THASH_RAND64_NUM2] = {\n", fp);
		for (int i=0; i < 769; i++) {
			fprintf(fp, "0x%16.16llx,%s", *p++, ((i+1) % 4) == 0 ? "\n" : "");
		}
		fputs("\n};\n", fp);
		fclose(fp);
	}
}

#endif

