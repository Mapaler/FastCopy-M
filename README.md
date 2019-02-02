*FastCopy-M*ultilanguage
===========
"FastCopy-M" 是免费开源软件 "FastCopy" 的一个二次开发分支。  
"FastCopy-M" is a fork of open source soft "FastCopy".

现在拥有如下语言：  
Now has the following language:

| Language | LCID Dec | Detail |
| --- | --- | --- |
| Japanese | 1041 | 日本語,Original |
| English | 1033 | Official translation by H.Shirouzu |
| Chinese, simplified | 2052 | 简体中文，从英文翻译而来 |
| Chinese, traditional | 1028 | 繁体中文，用MS Word從簡中轉換而來，稍加修訂 |

## Original FastCopy feature | 原版FastCopy特点
1. FastCopy 是Windows上最快的复制/备份软件。  
FastCopy is the Fastest Copy/Backup Software on Windows.
1. 它支持UNICODE和超过MAX_PATH(260字符)的文件路径名。  
It supports UNICODE and over MAX_PATH (260 characters) file pathnames.
1. 它使用多线程进行读/写/校验,重叠IO，直接IO, 所以它可以实现接近设备极限的读写性能。  
Because it uses multi-threads for Read/Write/Verify, Overlapped I/O, Direct I/O, so it brings out the best speed of devices.
1. 可以使用 UNIX通配符 样式的 包含/排除 筛选器。  
It supports Include/Exclude filter like a UNIX wildcard.
1. 它运行速度快，不占用资源，因为没有使用 MFC。（仅使用 Win32 API 和 C 运行时设计）  
It runs fast and does not hog resources, because MFC is not used. (Designed using Win32 API and C Runtime only)
## FastCopy-M feature | FastCopy-M特点
* 汉化并支持更加完整的多国语言显示，添加语言只需要修改资源文件。  
FastCopy Chinesization and modify to support other language more overall, add language only need add new resources
* 支持调用网络URL作为帮助文件，资源文件内“IDS_FASTCOPYHELP”修改为网页url即可。  
Support use http url to replace "chm" help files, change "IDS_FASTCOPYHELP" in resource to your URL.  
![URL help](http://ww4.sinaimg.cn/large/6c84b2d6gw1ewbd1y0bygj20rw0le4bq.jpg)

## How to Build | 如何编译
FastCopy-M采用[VS2017](https://www.visualstudio.com/zh-cn/downloads/download-visual-studio-vs.aspx) v141工具集。  
FastCopy-M used [VS2017](https://www.visualstudio.com/en-us/downloads/download-visual-studio-vs.aspx) with v141 tools.

### Auto zip release | 自动打包
1. 使用VS2017编译FastCopy项目的32位与64位的Release。  
Use VS2017 to build 32-bit and 64-bit release for FastCopy projects.
1. 在代码根目录建立`vendor`文件夹，并将[HTML Help Workshop](https://docs.microsoft.com/zh-cn/previous-versions/windows/desktop/htmlhelp/microsoft-html-help-downloads)里的`hhc.exe`、`hha.dll`和[7-Zip](https://sparanoid.com/lab/7z/)里的`7z.exe`三个文件放入其中。  
Create a `vendor` folder in the code root directory and place the `hhc.exe`, `hha.dll`([HTML Help Workshop](https://docs.microsoft.com/zh-cn/previous-versions/windows/desktop/htmlhelp/microsoft-html-help-downloads)) and `7z.exe`([7-Zip](https://sparanoid.com/lab/7z/)) three files in.
1. 执行`AutoZipRelease.vbs`。  
Execute `AutoZipRelease.vbs`.
1. 代码根目录下将会生成32位与64位两个zip压缩包。  
A 32-bit and 64-bit two zip compression packages will be generated under the code root directory.

## Website
* ### FastCopy-M website | FastCopy-M 网站
  http://mapaler.github.io/FastCopy-M/
* ### Official website | FastCopy 官方网站
  https://fastcopy.jp/

* ### Official GitHub Repository | FastCopy 官方版源代码仓库
  https://github.com/shirouzu/FastCopy
  >因为 FastCopy-M 在GitHub上传源代码早于 FastCopy 官方（以前仅在官网发布），因此本仓库并非直接克隆该仓库。  
  >Because FastCopy-M uploaded the source code at GitHub earlier than FastCopy official (previously only posted on the official website), this repository does not directly clone the official repository.
## Official License | FastCopy官方许可
>FastCopy ver3.x  
>&nbsp;Copyright(C) 2004-2019 SHIROUZU Hiroaki All rights reserved.  
>&nbsp;Copyright(C) 2018-2019 FastCopy Lab, LLC. All rights reserved.
> 
> This program is free software. You can redistribute it and/or modify it under the GNU General Public License version 3(GPLv3).  
> [License details](https://fastcopy.jp/help/license-gpl3.txt)

> xxHash Library Copyright (c) 2012-2016, Yann Collet All rights reserved.  
> [License details](https://fastcopy.jp/help/xxhash-LICENSE.txt).
