FastCopy-M
===========
"FastCopy-M" 是免费开源软件 "FastCopy" 的一个二次开发分支。<br />
"FastCopy-M" is a branch of open source soft "FastCopy", "M" is Multilanguage or Mapaler.

现在拥有如下语言：<br />
Now has the following language:

| Language | Detail |
| --- | --- |
| Japanese | 日本語,Original |
| English | Official translation by H.Shirouzu |
| Chinese, simplified | 简体中文，从英文翻译而来 |
| Chinese, traditional | 繁体中文，用MS Word從簡中轉換而來，稍加修訂 |

## Original FastCopy feature | 原版FastCopy特点

* FastCopy 是Windows上最快的复制/删除软件。<br />
FastCopy is the Fastest Copy/Delete Software on Windows.

* 它支持UNICODE和超过MAX_PATH(260字符)的文件路径名。<br />
It supports UNICODE and over MAX_PATH (260 characters) file pathnames.

* 它会根据来源与目录在相同或不同的硬盘自动选择不同的方法。<br />

| 不同硬盘 | 相同硬盘 |
| --- | --- |
| 读写分别由单独的线程并行处理。 | 首先做连续读取直到充满缓冲区。当缓冲区填满时，才开始大块数据写入。 |

It automatically selects different methods according to whether Source and DestDir are in the same or different HDD(or SSD).<br />

| Diff HDD | Same HDD |
| --- | --- |
| Reading and writing are processed respectively in parallel by separate threads. | Reading is processed until the big buffer fills. When the big buffer is filled, writing is started and processed in bulk. |
	

* 因为根本没有使用操作系统缓存来处理读/写，其他应用程序就不容易变得缓慢。<br />
Reading/Writing are processed with no OS cache, as such other applications do not become slow.

* 它可以实现接近设备极限的读写性能。<br />
It can achieve Reading/Writing performance that is close to device limit.

* 可以使用 包含/排除 筛选器 (UNIX通配符样式)。<br />
Include/Exclude Filter (UNIX Wildcard style) can be specified. ver3.0 or later, Relative Path can be specified.

* 仅使用Win32 API和C 运行时，没有使用MFC等框架，因此可以轻量、紧凑、轻快的运行。（注：XP下也不需要安装运行库）——所以也导致无法支持手机的MTP模式<br />
It runs fast and does not hog resources, because MFC is not used. (Designed using Win32 API and C Runtime only)

* 你可以修改此款软件，所有源代码都以GPLv3许可开源。<br />
You can modify this software, because all source code has been opened to the public under the GPL ver3 license.

## FastCopy-M feature | FastCopy-M特点

* 汉化并支持更加完整的多国语言显示，添加语言只需要修改资源文件即可。<br />
FastCopy Chinesization and modify to support other language more overall, add language only need add new resources

* Vista以后支持新版文件/文件夹选择框（可多选）<br />
After Vista support new Shell common file dialog (Easy-to-use, and can select folder multiple)<br />
![Folder select](http://ww3.sinaimg.cn/large/6c84b2d6gw1ewei65dy7bj20li0g8tdj.jpg)
![File select](http://ww3.sinaimg.cn/large/6c84b2d6gw1ewei561xzhj20kv0fu0wz.jpg)

* 主界面复制时图标也显示动画，并允许修改动画总帧数。<br />
The main window icon displayed animation when runing copy process. And allows to modify the total number of animation frames.<br />
图标来自(Icon from)：[(C75)[迷走ポタージュ]東方籠奴抄 - オプティカル セロファン](http://mesopota.net/roadshow/crossfade.html)<br />
![Main window icon animation](http://ww3.sinaimg.cn/large/6c84b2d6gw1ewbcz4bxdbj20b00hnwhb.jpg)

* 支持调用网络URL作为帮助文件，资源文件内“IDS_FASTCOPYHELP”修改为网页url即可。<br />
Support use http url to replace "chm" help files, change "IDS_FASTCOPYHELP" in resource to your URL.<br />
![URL help](http://ww4.sinaimg.cn/large/6c84b2d6gw1ewbd1y0bygj20rw0le4bq.jpg)

* ~~外壳扩展加入图标。~~ 3.2.6开始使用官方原生图标支持（其实是我不会改啦）<br />
~~Shell Extension add icon.~~ Official support after 3.2.6<br />
![Shell Extension icon](http://ww3.sinaimg.cn/large/6c84b2d6gw1ewbd5egv8jj20rj0hpjxc.jpg)

### Build | 编译
FastCopy-M采用[VS2015](https://www.visualstudio.com/zh-cn/downloads/download-visual-studio-vs.aspx)，暂不支持VS2017。<br />
FastCopy-M used [VS2015](https://www.visualstudio.com/en-us/downloads/download-visual-studio-vs.aspx), not support VS2017 now.

## Auto Change Icon | 自动更换图标 

* 解压“FastCopy-M Resources Rebuild & Replace Tools.zip”基本工具。<br />
Unzip "FastCopy-M Resources Rebuild & Replace Tools.zip" basic tools.
* 将“Auto Change Icon Tools.zip”解压到上述解压出来的文件夹中。<br />
Unzip this "Auto Change Icon Tools.zip" to previous folder.
* 在“My New Icon”文件夹内按照1、2、3……的顺序命名图标，最后一个图标是等待图标。<br />
Put your customize icon animate group, and rename to "1.ico","2.ico","3.ico"... Attention: The final icon file as wait icon.<br />
![Rename Icon](http://ww2.sinaimg.cn/large/6c84b2d6jw1ewbqj5xriyj20oa0cugoj.jpg)
* 然后将“FastCopy.exe”拖到“Change Icon.vbs”上即可。<br />
Then drag original "FastCopy.exe" and put on "Change Icon.vbs" . <br />
![Drag](http://ww4.sinaimg.cn/large/6c84b2d6jw1ewbqkhud8aj20hx0b6ace.jpg)
* 会自动编译和替换资源，并保存到“output\FastCopy.exe”。<br />
Then will auto begin rebuild and replace resources.  And generate to "output\FastCopy.exe".<br />
![New Icon](http://ww2.sinaimg.cn/large/6c84b2d6jw1ewbqkq4qh9j20f4089wfa.jpg)

### Download Tools | 工具下载

* [FastCopy-M Resources Rebuild & Replace Tools.zip](https://github.com/Mapaler/FastCopy-M/releases/download/v3.0.4.20/FastCopy-M.Resources.Rebuild.Replace.Tools.zip)<br />
资源源代码已改变，请先执行“Update resources source file.vbs”更新源代码。<br />
Resource source files has changed, please run "Update resources source file.vbs" update source at first.
* [Auto Change Icon Tools.zip](https://github.com/Mapaler/FastCopy-M/releases/download/v3.0.4.20/Auto.Change.Icon.Tools.zip)

## How to translate
You can edit the source and rebuild executable.<br />
Or more simple:
* Unzip "FastCopy-M Resources Rebuild & Replace Tools.zip".
* Use "Update resources source file.vbs" to update the newest source. Attention: Backup your edited file before use update.
* Use "Visual Studio" for IDE (VS2010 or later),or any Notepad to to edit "fastcopy.rc" directly.
* Visual Studio is Easy-to-use,ResEdit is not recommand (rc file saved by the soft , can't open in VS, and can't rebuild by my tools.Use it requir some processing), If you use Notepad edit manually. For example, open "fastcopy.rc" find and copy English part and past at the end.
```C
/////////////////////////////////////////////////////////////////////////////
// English (United States) [or 英語 (米国)] resources [about line 2391]

#if !defined(AFX_RESOURCE_DLL) || defined(AFX_TARG_ENU)
LANGUAGE LANG_ENGLISH, SUBLANG_ENGLISH_US

...

#endif    // English (United States) [or 英語 (米国)] resources [about line 3137]
/////////////////////////////////////////////////////////////////////////////
```
* Change "LANGUAGE LANG_ENGLISH, SUBLANG_ENGLISH_US" to your language and sub-language id. You can find them at "include\winnt.rh". No need to edit "defined(AFX_TARG_ENU)" (I don't know where can find it.)
* Edit this part to your language.
* Drag original "FastCopy.exe" and put on "rebuild.bat".
* Then will auto begin rebuild and replace resources.  And generate to "output\FastCopy.exe".
* Change "lcid" value in "FastCopy2.ini" to other decimal LCID, you can test other language on your OS. [LCID list](https://msdn.microsoft.com/en-us/goglobal/bb964664.aspx) 
If you want add your translate to FastCopy-M, please send your "fastcopy.rc" source to mapaler@163.com.

## Website
### FastCopy-M website | FastCopy-M网站

http://mapaler.github.io/FastCopy-M/

### Official website | FastCopy官方网站

http://ipmsg.org/tools/fastcopy.html
 
## Official License | FastCopy官方许可
> FastCopy ver3.x Copyright(C) 2004-2017 by SHIROUZU Hiroaki
> 
> This program is free software. You can redistribute it and/or modify it under the GNU General Public License version 3(GPLv3).<br />
> [license-gpl3.txt](http://ipmsg.org/tools/license-gpl3.txt)
> 
> If you want to distribute your modified version, but you don't want to distribute the modified source code, or you want to request to develop bundle/special versoin, please contact me. (shirouzu@ipmsg.org)


> xxHash Library Copyright (c) 2012-2014, Yann Collet All rights reserved.<br>
> [License details](https://ipmsg.org/tools/xxhash-LICENSE.txt).
