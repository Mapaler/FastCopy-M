======================================================================
                     FastCopy  ver3.54                 2018/08/09
                                            H.Shirouzu（白水啓章）
                                            FastCopy Lab, LLC.
                     FastCopy-M branch                 2018/12/11
                                            Mapaler   （楓谷劍仙）
======================================================================

特點：
FastCopy 是Windows上最快的複製/刪除軟體。

它支援UNICODE和超過MAX_PATH(260字元)的檔路徑名。 

它會根據來源與目錄在相同或不同的硬碟自動選擇不同的方法。

	不同硬碟
			讀寫分別由單獨的執行緒並行處理。
	相同硬碟
			首先做連續讀取直到充滿緩衝區。當緩衝區填滿時，才開始大塊資料寫入。

因為根本沒有使用作業系統緩存來處理讀/寫，其他應用程式就不容易變得緩慢。

它可以實現接近設備極限的讀寫性能。

可以使用 包含/排除 篩選器 (UNIX萬用字元樣式)。

僅使用Win32 API和C 運行時，沒有使用MFC等框架，因此可以輕量、緊湊、輕快的運行。

所有的原始程式碼已根據GPL v3協定開放給所有人，你可以使用VC++免費進行自訂。

FastCopy-M特點：
  支援更加完整的多國語言顯示

  支援調用網路URL作為説明檔

許可:
-------------------------------------------------------------------------
	FastCopy ver3.x
	  Copyright(C) 2004-2018 SHIROUZU Hiroaki
	    and FastCopy Lab, LLC. All rights reserved.

此程式是免費軟體。你可以將它根據GNU通用公共許可證第三版重新分發和/或修改。

詳細資訊請閱讀 license-gpl3.txt

	xxHash library
	  Copyright (C) 2012-2016 Mr.Yann Collet, All rights reserved.
	  more details: external/xxhash/LICENSE

-------------------------------------------------------------------------

使用方法:
請參閱 http://mapaler.github.io/FastCopy-M/help-cht.html

生成:
VS2017 或更高版本