======================================================================
                     FastCopy  ver3.02                 2015/08/20
                                            H.Shirouzu（白水啓章）
                     FastCopy-M branch              2015/08/23
                                            Mapaler（枫谷剑仙）
======================================================================

特徴：
	Windows系最速(?) のファイルコピー＆削除ツールです。

	NT系OS の場合、UNICODE でしか表現できないファイル名や MAX_PATH
	(260文字) を越えた位置のファイルもコピー（＆削除）できます。

	自動的に、コピー元とコピー先が、同一の物理HDD or 別HDD かを判定
	した後、以下の動作を行います。

		別HDD 間：
			マルチスレッドで、読み込みと書き込みを並列に行う

		同一HDD 間：
			コピー元から（バッファが一杯になるまで）連続読み込み後、
			コピー先に連続して書き込む

	Read/Write も、OS のキャッシュを全く使わないため、他のプロセス
	（アプリケーション）が重くなりにくくなっています。

	可能な限り大きな単位で Read/Write するため、デバイスの限界に
	近いパフォーマンスが出ます。

	Include/Exclude フィルタ（UNIXワイルドカード形式）が指定可能です。

	MFC 等のフレームワークを使わず、Win32API だけで作っていますので、
	軽量＆コンパクト＆軽快に動作します。

	FastCopy-M feature :
	More complete multilanguage supports.

	The main window icon displayed animation when runing copy process.
	
	Support use http url to replace "chm" help files.

ライセンス：
	全ソースコードは GPLv3 で公開しています。
	VC++ 等をお持ちであれば、ご自由にカスタマイズしてのご利用も
	可能です。（カスタマイズ版の配布をソースコード非公開で行いたい
	場合は別途、お問い合わせください）
	詳しくは同梱の license.txt をご覧ください。

使用法等：
	ヘルプ(fastcopy.chm) を参照してください。

ソース・ビルドについて：
	VS2013 以降でビルドできます。
	FastCopy-M used VS2015, if you want bulid on VS2013 or earlier you can replace the "*.sln", "*.vcxproj" to original version (or modify project version in these file) by your self.
