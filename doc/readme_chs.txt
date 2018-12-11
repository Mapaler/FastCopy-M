======================================================================
                     FastCopy  ver3.54                 2018/08/09
                                            H.Shirouzu（白水啓章）
                                            FastCopy Lab, LLC.
                     FastCopy-M branch                 2018/12/11
                                            Mapaler  （枫谷剑仙）
======================================================================

特点：
FastCopy 是Windows上最快的复制/删除软件。

它支持UNICODE和超过MAX_PATH(260字符)的文件路径名。 

它会根据来源与目录在相同或不同的硬盘自动选择不同的方法。

	不同硬盘
			读写分别由单独的线程并行处理。
	相同硬盘
			首先做连续读取直到充满缓冲区。当缓冲区填满时，才开始大块数据写入。

因为根本没有使用操作系统缓存来处理读/写，其他应用程序就不容易变得缓慢。

它可以实现接近设备极限的读写性能。

可以使用 包含/排除 筛选器 (UNIX通配符样式)。

仅使用Win32 API和C 运行时，没有使用MFC等框架，因此可以轻量、紧凑、轻快的运行。

所有的源代码已根据GPL v3协议开放给所有人，你可以使用VC++免费进行自定义。

FastCopy-M特点：
  支持更加完整的多国语言显示

  支持调用网络URL作为帮助文件

许可:
-------------------------------------------------------------------------
	FastCopy ver3.x
	  Copyright(C) 2004-2018 SHIROUZU Hiroaki
	    and FastCopy Lab, LLC. All rights reserved.

此程序是免费软件。你可以将它根据GNU通用公共许可证第三版重新分发和/或修改。

详细信息请阅读 license-gpl3.txt

	xxHash library
	  Copyright (C) 2012-2016 Mr.Yann Collet, All rights reserved.
	  more details: external/xxhash/LICENSE

-------------------------------------------------------------------------

使用方法:
请参阅 http://mapaler.github.io/FastCopy-M/help-chs.html

生成:
VS2017 或更高版本