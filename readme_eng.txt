======================================================================
                   FastCopy  ver3.08                   2015/11/15
                                                 SHIROUZU Hiroaki
                   FastCopy-M branch                2015/09/22
                                                 Mapaler
======================================================================

	FastCopy is the Fastest Copy/Delete Software on Windows.

	It can copy/delete unicode and over MAX_PATH(260byte) pathname files.

	It always run by multi-threading.

	It don't use MFC, it is compact and don't requre mfcxx.dll.

	FastCopy is GPLv3 license, you can modify and use under GPLv3
	
	FastCopy-M feature :
    More complete multilanguage supports.
    
    After Vista support new Shell common file dialog (Easy-to-use, and can select folder multiple)

    The main window icon displayed animation when runing copy process. And allows to modify the total number of animation frames.
    
    Support use http url to replace "chm" help files.
    
    Shell Extension add icon.

License:
	-------------------------------------------------------------------------
	 FastCopy ver3.0
	 Copyright(C) 2004-2015 SHIROUZU Hiroaki All rights reserved.

	 This program is free software. You can redistribute it and/or modify
	 it under the GNU General Public License version 3.

	 If you want to distribute your modified version, but you don't want to
	 distribute the modified source code, please contact shirouzu@ipmsg.org

	 more details: license-gpl3.txt
	-------------------------------------------------------------------------

Usage：
	Please see fastcopy.chm

Build:
	VS2013 or later
	FastCopy-M used VS2015, if you want bulid on VS2013 or earlier you can replace the "*.sln", "*.vcxproj" to original version (or modify project version in these file) by your self. Additional Dependencies need include "shlwapi.lib" after ver3.0.4.20.