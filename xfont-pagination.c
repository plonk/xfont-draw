/**
 * xfont-pagination: テキストファイルをページに区切って表示するビューア。
 */
#define PROGRAM_NAME "xfont-pagination"

#include <assert.h>
#include <ctype.h>
#include <iconv.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
// time関数の為に time.h をインクルードする。
#include <time.h>
#include <X11/Xlib.h>

#include "util.h"

#define FONT "-gnu-unifont-medium-r-normal-sans-16-160-75-75-c-80-iso10646-1"

#define MAX_PAGES 2048

#define CURSOR_WIDTH 2

// グローバル変数
static Display		*disp;
static Window		 win;
static GC		 gc;
static GC		 control_gc; // 制御文字を描画する為の GC
static GC		 margin_gc;
static GC		 cursor_gc; // カーソルを描画する為の GC
static XFontStruct	*font;

// テキスト情報
static XChar2b		*text;
static size_t		 text_length;

// カーソル情報
//
//   文書の最後では text_length に等しくなることに注意。その場合
//   text[cursor_position] にアクセスすることはできない。
static size_t		 cursor_position;

// ページ情報
//
//   ページは text へのインデックスを持つ構造体で表わす。
typedef struct {
    size_t start;
    size_t end;
} Page;
static Page		 pages[MAX_PAGES];
static size_t		 npages;

struct {
    XColor skyblue;
    XColor gray50;
    XColor gray80;
    XColor green;
} Color;

size_t FillPage(size_t start, Page *page, bool draw);
bool EqAscii2b(XChar2b a, unsigned char b);
bool EqCodePoint(XChar2b a, int codepoint);
bool IsPrint(XChar2b a);

bool EqCodePoint(XChar2b a, int codepoint)
{
    assert (0 <= codepoint && codepoint <= 0xffff );

    return (codepoint >> 8) == a.byte1 && (codepoint & 0xff) == a.byte2;
}

bool IsPrint(XChar2b a)
{
    // 正しくはない。

    if (a.byte1 != 0) return true;
    if (a.byte2 <= 0x7f && isprint(a.byte2)) return true;
    return false;
}

// ページ区切り位置を計算して、pages, npages を変更する。
void Paginate()
{
    Page *current_page = pages;
    size_t previous_end = 0;

    printf("text_length = %zu\n", text_length);
    do {
	if (current_page - pages == MAX_PAGES) { fprintf(stderr, "too many pages"); abort(); }

        previous_end = FillPage(previous_end, current_page, false);
	printf("page: start=%zu, end=%zu\n", current_page->start, current_page->end);
	current_page++;
    } while (previous_end < text_length);

    // この時点で current_page は最後の次の場所を指している。
    npages = current_page - pages;
}

bool ForbiddenAtStart(XChar2b ch)
{
    // 0x3001 [、]
    // 0x3002 [。]

    return EqCodePoint(ch, 0x3001) || EqCodePoint(ch, 0x3002);
}

int GetCharWidth(XChar2b ch)
{
    XCharStruct *csp = GetCharInfo16(font, ch.byte1, ch.byte2);

    return csp->width;
}

// 次のページの開始位置、あるいは文書の終端 (== text_length) を返す。
size_t FillPage(size_t start, Page *page, bool draw)
{
    XWindowAttributes attrs;
    XGetWindowAttributes(disp, win, &attrs);

    // ページのサイズ。
    const int LEFT_MARGIN	= 50;
    const int RIGHT_MARGIN	= attrs.width - LEFT_MARGIN;
    const int TOP_MARGIN	= 50;
    const int BOTTOM_MARGIN	= attrs.height - TOP_MARGIN;

    const XChar2b sp = { 0x00, 0x21 };
    const int EM = GetCharWidth(sp);

    // 行の高さ。
    const int LINE_HEIGHT = 22;

    // 現在の文字の描画位置。
    int x = LEFT_MARGIN, y = TOP_MARGIN + font->ascent;

    size_t i;
    for (i = start; i < text_length; i++) {
	// カーソルの描画
	if (draw && i == cursor_position) {
	    XFillRectangle(disp, win, cursor_gc,
			   x, y - font->ascent,
			   CURSOR_WIDTH, font->ascent + font->descent);
	}

	if (IsPrint(text[i])) {
	    // 印字可能文字の場合。

	    int width = GetCharWidth(text[i]);

	    // この文字を描画すると右マージンにかかるようなら改行する。
	    // ただし、行頭に居る場合は改行しない。
	    if ( x + width > RIGHT_MARGIN &&
		 !ForbiddenAtStart(text[i]) && // 行頭禁止文字ならばぶらさげる
		 x != LEFT_MARGIN ) {
		y += LINE_HEIGHT;
		x = LEFT_MARGIN;

		// ページにも収まらない場合、この位置で終了する。
		if (y + font->descent > BOTTOM_MARGIN) {
		    page->start = start;
		    page->end = i;
		    return i;
		}
	    }

	    if (draw) XDrawString16(disp, win, gc,
				    x, y,
				    &text[i], 1);
	    x += width;
	} else {
	    // ラインフィードで改行する。
	    if (EqAscii2b(text[i], '\n')) {
		if (draw) {
		    // DOWNWARDS ARROW WITH TIP LEFTWARDS
		    XChar2b symbol = { .byte1 = 0x21, .byte2 = 0xb2 };
		    XDrawString16(disp, win, control_gc,
				  x, y,
				  &symbol, 1);
		}
		y += LINE_HEIGHT;
		x = LEFT_MARGIN;

		// ページにも収まらない場合、次の位置で終了する。
		// ページ区切り位置での改行は持ち越さない。
		if (y + font->descent > BOTTOM_MARGIN) {
		    page->start = start;
		    page->end = i + 1;
		    return i + 1;
		}
	    } else if (EqAscii2b(text[i], '\t')) {
		int tab = EM * 8;
		x = LEFT_MARGIN + (((x - LEFT_MARGIN) / tab) + 1) * tab;
	    }
	}
    }
    if (draw && i == cursor_position) {
	XFillRectangle(disp, win, cursor_gc,
		       x, y - font->ascent,
		       CURSOR_WIDTH, font->ascent + font->descent);
    }
    if (draw) XDrawString(disp, win, control_gc,
			  x, y,
			  "[EOF]", 5);
    // 全てのテキストを配置した。
    page->start = start;
    page->end = text_length;
    return text_length;
}

// 全てを再計算する。
void Recalculate()
{
    Paginate();
}

void DrawPage(Page *page)
{
    // 実際に描画するので第三引数に true を渡す。
    FillPage(page->start, page, true);
}

Page *GetCurrentPage()
{
    size_t i;
    for (i = 0; i < npages; i++) {
	if (pages[i].start <= cursor_position && cursor_position < pages[i].end)
	    return &pages[i];
    }
    if (cursor_position == text_length) {
	return &pages[npages - 1];
    }
    fprintf(stderr, "cursor position out of range?");
    abort();
}

void MarkMargins()
{
    XWindowAttributes attrs;
    XGetWindowAttributes(disp, win, &attrs);

    // ページのサイズ。
    const int LEFT_MARGIN	= 50 - 1;
    const int RIGHT_MARGIN	= attrs.width - LEFT_MARGIN + 1;
    const int TOP_MARGIN	= 50 - 1;
    const int BOTTOM_MARGIN	= attrs.height - TOP_MARGIN + 1;

    const int len = 10;

    // _|
    XDrawLine(disp, win, margin_gc, LEFT_MARGIN - len, TOP_MARGIN, LEFT_MARGIN, TOP_MARGIN); // horizontal
    XDrawLine(disp, win, margin_gc, LEFT_MARGIN, TOP_MARGIN - len, LEFT_MARGIN, TOP_MARGIN); // vertical

    //        |_
    XDrawLine(disp, win, margin_gc, RIGHT_MARGIN, TOP_MARGIN, RIGHT_MARGIN + len, TOP_MARGIN);
    XDrawLine(disp, win, margin_gc, RIGHT_MARGIN, TOP_MARGIN - len, RIGHT_MARGIN, TOP_MARGIN);

    // -|
    XDrawLine(disp, win, margin_gc, LEFT_MARGIN - len, BOTTOM_MARGIN, LEFT_MARGIN, BOTTOM_MARGIN);
    XDrawLine(disp, win, margin_gc, LEFT_MARGIN, BOTTOM_MARGIN, LEFT_MARGIN, BOTTOM_MARGIN + len);

    //        |-
    XDrawLine(disp, win, margin_gc, RIGHT_MARGIN, BOTTOM_MARGIN, RIGHT_MARGIN + len, BOTTOM_MARGIN);
    XDrawLine(disp, win, margin_gc, RIGHT_MARGIN, BOTTOM_MARGIN, RIGHT_MARGIN, BOTTOM_MARGIN + len);
}

void Redraw()
{
    XClearWindow(disp, win);

    MarkMargins();
    DrawPage(GetCurrentPage());
}

void Initialize()
{

    disp = XOpenDisplay(NULL); // open $DISPLAY
    win = XCreateSimpleWindow(disp,						// ディスプレイ
				DefaultRootWindow(disp),			// 親ウィンドウ
				0, 0,						// (x, y)
				640, 480,					// 幅・高さ
				0,						// border width
				0,						// border color
				WhitePixel(disp, DefaultScreen(disp)));		// background color
    XMapWindow(disp, win);

    /* ウィンドウに関連付けられたグラフィックコンテキストを作る */
    gc = XCreateGC(disp, win, 0, NULL);
    XSetForeground(disp, gc,
		   BlackPixel(disp, DefaultScreen(disp)));

    // 暴露イベントとキー押下イベントを受け取る。
    XSelectInput(disp, win, ExposureMask | KeyPressMask);

    font = XLoadQueryFont(disp, FONT);
    XSetFont(disp, gc, (font)->fid);

    Colormap colormap;
    colormap = DefaultColormap(disp, 0);

    XParseColor(disp, colormap, "#00AA00", &Color.green);
    XAllocColor(disp, colormap, &Color.green);

    XParseColor(disp, colormap, "gray50", &Color.gray50);
    XAllocColor(disp, colormap, &Color.gray50);

    XParseColor(disp, colormap, "gray80", &Color.gray80);
    XAllocColor(disp, colormap, &Color.gray80);

    XParseColor(disp, colormap, "cyan3", &Color.skyblue);
    XAllocColor(disp, colormap, &Color.skyblue);

    control_gc = XCreateGC(disp, win, 0, NULL);
    XSetFont(disp, control_gc, (font)->fid);
    XSetForeground(disp, control_gc, Color.skyblue.pixel);

    margin_gc = XCreateGC(disp, win, 0, NULL);
    XSetForeground(disp, margin_gc, Color.gray80.pixel);

    cursor_gc = XCreateGC(disp, win, 0, NULL);
    XSetForeground(disp, cursor_gc, Color.green.pixel);
}

void CleanUp()
{
    XUnloadFont(disp, font->fid);
    XFreeGC(disp, cursor_gc);
    XFreeGC(disp, margin_gc);
    XFreeGC(disp, control_gc);
    XFreeGC(disp, gc);
    XDestroyWindow(disp, win);
    XCloseDisplay(disp);
    assert(text != NULL);
    free(text);
}

void UsageExit()
{
    fprintf(stderr, "Usage: " PROGRAM_NAME " FILENAME\n");
    exit(1);
}

void LoadFile(const char *filepath)
{
    // 前半で UTF8 ファイルをロードし、後半で UCS2 に変換する。
    // ビッグエンディアンの UCS2 は XChar2b 構造体とバイナリ互換性を持つ。
    FILE *fp = fopen(filepath, "r");
    if ( fp == NULL ) {
	perror(filepath);
	exit(1);
    }
    struct stat st;
    if ( stat(filepath, &st) == -1 ) {
	perror(filepath);
	exit(1);
    }
    char *utf8 = alloca(st.st_size + 1);
    if ( fread(utf8, 1, st.st_size, fp) != st.st_size) {
	fprintf(stderr, "warning: size mismatch\n");
    }
    fclose(fp);

    iconv_t cd = iconv_open("UCS-2BE", "UTF-8");

    size_t inbytesleft = st.st_size;
    // UTF-8 を UCS2 に変換した場合、最大で二倍のバイト数を必要とする。
    // NUL 終端はしない。
    size_t outbytesleft = st.st_size * 2;
    text = malloc(outbytesleft);
    char *outptr = (char *) text;

    if ( iconv(cd, &utf8, &inbytesleft, &outptr, &outbytesleft) == -1) {
	perror(PROGRAM_NAME);
	exit(1);
    }
    text_length = (XChar2b *) outptr - text;
    iconv_close(cd);
}

#include <X11/keysym.h>

bool EqAscii2b(XChar2b a, unsigned char b)
{
    if (a.byte1 == 0 &&
	a.byte2 == b)
	return true;
    return false;
}

void HandleKeyPress(XKeyEvent *ev)
{
    bool needs_redraw = false;
    KeySym sym;

    sym = XLookupKeysym(ev, 0);

    switch (sym) {
    case XK_Right:
	if (cursor_position < text_length)
	    cursor_position++;
	needs_redraw = true;
	break;
    case XK_Left:
	if (cursor_position > 0)
	    cursor_position--;
	needs_redraw = true;
	break;
    case XK_Delete:
	if (cursor_position < text_length) {
	    memmove(&text[cursor_position], &text[cursor_position+1],
		    sizeof(text[0]) * (text_length - cursor_position - 1));
	    text_length--;
	    needs_redraw = true;
	}
	break;
    case XK_BackSpace:
	if (cursor_position > 0) {
	    memmove(&text[cursor_position-1], &text[cursor_position],
		    sizeof(text[0]) * (text_length - cursor_position));
	    text_length--;
	    cursor_position--;
	    needs_redraw = true;
	}
	break;
    case XK_Down:
	while (cursor_position != text_length &&
	       !(EqAscii2b(text[cursor_position], '\n'))) {
	    cursor_position++;
	}
	if (cursor_position < text_length) {
	    cursor_position++;
	}
	needs_redraw = true;
	break;
    case XK_Next:
	// 次のページへ移動する。
	{
	    Page *page = GetCurrentPage();

	    if (page - pages < npages - 1) {
		page++;
		cursor_position = page->start;
		needs_redraw = true;
	    }
	}
	break;
    case XK_Prior:
	// 前のページへ移動する。
	{
	    Page *page = GetCurrentPage();

	    if (page > pages) {
		page--;
		cursor_position = page->start;
		needs_redraw = true;
	    }
	}
	break;
#if 0
    case XK_Up:
	while (cursor_position > 0 && !(text[cursor_position].byte1 == 0 && text[cursor_position].byte2 == '\n')) {
	    cursor_position--;
	}
	if (cursor_position > 0) {
	    cursor_position--;
	}
	while (cursor_position > 0 && !(text[cursor_position].byte1 == 0 && text[cursor_position].byte2 == '\n')) {
	    cursor_position--;
	}
	if (cursor_position > 0) {
	    cursor_position++;
	}
	needs_redraw = true;
	break;
#endif
    default:
	;
    }
    printf("cursor = %zu\n", cursor_position);

    if (needs_redraw) {
	XExposeEvent expose_event;

	memset(&expose_event, 0, sizeof(expose_event));
	expose_event.type = Expose;
	expose_event.window = win;

	XSendEvent(disp, win, False, ExposureMask, (XEvent *) &expose_event);
	// XFlush(disp);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
	UsageExit();

    LoadFile(argv[1]);
    Initialize();

    XEvent ev;

    while (1) { // イベントループ
	XNextEvent(disp, &ev);

	switch (ev.type) {
	case Expose:
	    puts("recalc");
	    Recalculate();
	    puts ("redraw");
	    Redraw();
	    break;
	case KeyPress:
	    HandleKeyPress((XKeyEvent *) &ev);
	    break;
	default:
	    ;
	}
    }

    CleanUp();
}
