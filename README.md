FastCopy-M
===========
"M" is Multilanguage

## FastCopy-M feature | FastCopy-M特点

* 汉化并支持更加完整的多国语言显示，添加语言只需要修改资源文件即可。<br />
FastCopy Chinesization and modify to support other language more overall, add language only need add new resources

* Vista以后支持新版文件/文件夹选择框（可多选）<br />
After Vista support new Shell common file dialog (Easy-to-use, and can select folder multiple)<br />
![Folder select](http://ww3.sinaimg.cn/large/6c84b2d6gw1ewb4gmu1hij20mc0gjjwt.jpg)
![File select](http://ww1.sinaimg.cn/large/6c84b2d6gw1ewb4guswqvj20lo0g4gqz.jpg)

* 主界面复制时图标也显示动画，并允许修改动画总帧数<br />
The main window icon displayed animation when runing copy process. And allows to modify the total number of animation frames.<br />
![Main window icon animation](http://ww3.sinaimg.cn/large/6c84b2d6gw1ewbcz4bxdbj20b00hnwhb.jpg)

* 支持调用网络URL作为帮助文件，资源文件内“IDS_FASTCOPYHELP”修改为网页url即可。<br />
Support use http url to replace "chm" help files, change "IDS_FASTCOPYHELP" in resource to your URL.<br />
![URL help](http://ww4.sinaimg.cn/large/6c84b2d6gw1ewbd1y0bygj20rw0le4bq.jpg)

* 外壳扩展加入图标。<br />
Shell Extension add icon.<br />
![Shell Extension icon](http://ww3.sinaimg.cn/large/6c84b2d6gw1ewbd5egv8jj20rj0hpjxc.jpg)

### Build | 编译
FastCopy-M采用VS2015，你可以自行替换工程到原始版本（或手动修改文件内版本号）以在VS2013或2012上编译。目前比官方多需要引用“shlwapi.lib”。<br />
FastCopy-M used VS2015, if you want bulid on VS2013 or earlier you can replace the "*.sln", "*.vcxproj" to original version (or modify project version in these file) by your self. Additional Dependencies need include "shlwapi.lib" after ver3.0.4.20.

## Release Download

http://www.mapaler.com/fastcopy-m/

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

### Download Tools

* [FastCopy-M Resources Rebuild & Replace Tools.zip](https://github.com/Mapaler/FastCopy-M/releases/download/v3.0.4.20/FastCopy-M.Resources.Rebuild.Replace.Tools.zip)
* [Auto Change Icon Tools.zip](https://github.com/Mapaler/FastCopy-M/releases/download/v3.0.4.20/Auto.Change.Icon.Tools.zip)

## Official website | FastCopy官方网站

http://ipmsg.org/tools/fastcopy.html
 
##Official License | FastCopy官方许可
<div class="help_license">
	FastCopy ver3.0 Copyright(C) 2004-2015 by SHIROUZU Hiroaki
	<br />
	<br />
	This program is free software. You can redistribute it and/or modify it under the GNU General Public License version 3(GPLv3).
	<br />
	<a href="http://ipmsg.org/tools/license-gpl3.txt">license-gpl3.txt</a>
	<br />
	<br />
	If you want to distribute your modified version, but you don't want to distribute the modified source code, or you want to request to develop bundle/special versoin, please contact me. (shirouzu@ipmsg.org)
</div>