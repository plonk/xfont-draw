# Xlib で文字を扱う練習

## プログラム

* xfont-draw は文章をツメ組みで表示する。JIS X 0208 で k14 で表示するから半角は表示できない。
* xfont-info はコマンドラインで指定したフォントの情報(XFontStructに入って来る)をダンプする。
* xfont-eng は欧文ワードラップをやる。
* xfont-justify は両端揃えをやる。
* xfont-hyphen はハイフネーションをやる。
* xfont-unicode-cpp は GNU Unifont で UTF-8 のテキストファイルを表示する。
* xfont-font-combining はいろんな文字集合のフォントを組み合わせる。
* xfont-double-buffering はダブルバッファリングで再描画時のちらつきを抑える。
* xfont-input はキーボードで文字を入力できる。
* xfont-im は XIM で日本語を入力できる。
* xfont-draw-xft は xfont-draw の Xft 版。
* xfont-eng-xft は xfont-eng の Xft 版。
* xfont-editor-xft はエディタみたいなやつにしたい。
* draw は xfont-editor-xft と同じプログラムだけど Motif のインターフェイスが付いている。

## プログラム内部のこと

* 文字をオブジェクトで表わしたり、行をオブジェクトで表わしたりしたい。

線形のテキストを木にして、リーフノードが元のテキストと同じ順序で並ぶようにすればよいと思う。カーソルは常に？リーフノードを選択する感じ。

* ちらつきを抑えるためのダブルバッファリング。できた。

## これからやりたい事

* 行間、あるいは行の高さについてきちんと考える。

* ベースラインシフト。

べつにできなくてもいい。

* エディタのように一文字ずつ位置を指定して編集できるようにしたい。

* XLookupString とかでキーボード直接入力ができる。

* XIM対応。

* フォントに特定のグリフがあるかどうかの判定をする。スカスカの Unicode フォントもあるから。

XCharStruct で lbearing = rbearing = width = ascent = descent = attributes = 0 のようになるようだ。
他方空白文字は width などが設定されている。
xfd の CI_NONEXISTCHAR マクロを見てもそんなかんじ。attributes は見ていないけど。

### 英文組版

* Helvetica などのプロポーショナルフォントを表示する。
* 両端揃えで表示する。

### 文字の変形

ピクセル伸長アルゴリズム、大きなビットマップフォント(40ドットなど)のサブサンプリング。

* (後で書く)

## 文字コード

### ファイルの文字コード

UTF-8だけでもよい。他のエンコーディングに対応するにしても、Unicode へ変換する。

### フォントの文字コード

Unicodeから JIS X 0208 や ISO-8859-1 に変換する必要がある。Unicode の
中から、JIS や Latin1 でカバーされている部分や、プロパティ？(スクリプ
ト。Hiragana 等)によってフォントを指定したい。

そのまま Unicode フォントを使ったほうが簡単か…

### 文字クラス

始め括弧類
終わり括弧類
読点類
句点類
中点類

## 参考にしているサイト

* [Double Buffer Extension Library](http://www.x.org/releases/X11R7.7/doc/libXext/dbelib.html)
* [Chapter 11. Internationalized Text Input](http://menehune.opt.wfu.edu/Kokua/Irix_6.5.21_doc_cd/usr/share/Insight/library/SGI_bookshelves/SGI_Developer/books/XLib_PG/sgi_html/ch11.html)
* [X Window/Motif Programming](http://www.cs.cf.ac.uk/Dave/X_lecture/X_book_caller/)

## 
