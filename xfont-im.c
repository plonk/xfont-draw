#define PROGRAM_NAME "xfont-input"
/**
 * キーボードから文字を入力できるテキストビューア。
 */

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
// XLookupString関数の為に Xutil.h をインクルードする。
#include <X11/Xutil.h>
#include <X11/extensions/Xdbe.h>
#include <sys/time.h>
#include <locale.h>
#include <alloca.h>

#include "util.h"
#include "font.h"

#define DEFAULT_FONT "-gnu-unifont-medium-r-normal-sans-16-160-75-75-c-80-iso10646-1"

#define MAX_PAGES 2048

#define CURSOR_WIDTH 2

// グローバル変数
static Display		*disp;
static XIM		 im;
static XIC		 ic;
static Window		 win;
static XdbeBackBuffer	 back_buffer;
static GC		 default_gc;
static GC		 control_gc; // 制御文字を描画する為の GC
static GC		 margin_gc;
static GC		 cursor_gc; // カーソルを描画する為の GC
static XFontStruct	*default_font;

// テキスト情報
static XChar2b		*text;
static size_t		 text_length;

// 文字位置情報
static XPoint *character_positions;
static XPoint eof_position;

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
    XColor bright_green;
} Color;

XChar2b char_a = { 0, 66 };

static bool cursor_on;

void InsertCharacter(size_t position, XChar2b character);
void InvalidateWindow();
void UpdateCursor();
Page *GetCurrentPage();
Page *GetPage(size_t position);
size_t FillPage(size_t start, Page *page);
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

    // printf("text_length = %zu\n", text_length);
    do {
	if (current_page - pages == MAX_PAGES) { fprintf(stderr, "too many pages"); abort(); }

        previous_end = FillPage(previous_end, current_page);
	// printf("page: start=%zu, end=%zu\n", current_page->start, current_page->end);
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
    XChar2b font_code;
    XFontStruct *font = SelectFont(ch, &font_code);
    return GetCharInfo16(font, font_code.byte1, font_code.byte2)->width;
}

void InspectChar(XChar2b ch)
{
    printf("byte1 = 0x%02x, byte2 = 0x%02x\n", ch.byte1, ch.byte2);
}


// ページのサイズ。
static int LEFT_MARGIN;
static int RIGHT_MARGIN;
static int TOP_MARGIN;
static int BOTTOM_MARGIN;

void GetWindowSize() {
    XWindowAttributes attrs;
    XGetWindowAttributes(disp, win, &attrs);

    LEFT_MARGIN		= 50;
    RIGHT_MARGIN	= attrs.width - LEFT_MARGIN;
    TOP_MARGIN		= 50;
    BOTTOM_MARGIN	= attrs.height - TOP_MARGIN;
}

void DrawEOF(Drawable d, int x, int baseline)
{
    GC gc = XCreateGC(disp, win, 0, NULL);
    XCopyGC(disp, default_gc, GCFont, gc);
    XSetForeground(disp, gc, Color.skyblue.pixel);
    XSetBackground(disp, gc, WhitePixel(disp, 0));
    XDrawImageString(disp, d, gc,
		     x, baseline,
		     "[EOF]", 5);
    XFreeGC(disp, gc);
}

static inline void SetCharPos(size_t i, short x, short y) {
    character_positions[i].x = x;
    character_positions[i].y = y;
}

// 次のページの開始位置、あるいは文書の終端 (== text_length) を返す。
size_t FillPage(size_t start, Page *page)
{
    const XChar2b sp = { 0x00, ' ' };
    const int EM = GetCharWidth(sp);
    // 行の高さ。
    const int LINE_HEIGHT = 22;

    // 現在の文字の描画位置。y はベースライン位置。
    short x = LEFT_MARGIN;
    short y = TOP_MARGIN + default_font->ascent;

    size_t i;
    for (i = start; i < text_length; i++) {
	if (IsPrint(text[i])) {
	    // 印字可能文字の場合。

	    int width = GetCharWidth(text[i]);

	    // この文字を描画すると右マージンにかかるようなら改行する。
	    // ただし、描画領域が文字幅よりもせまくて行頭の場合はぶらさげる。
	    // また、行頭禁止文字である場合もぶらさげる。
	    if ( x + width > RIGHT_MARGIN && ! (ForbiddenAtStart(text[i]) || x == LEFT_MARGIN) ) {
		y += LINE_HEIGHT;
		x = LEFT_MARGIN;

		// ページにも収まらない場合、文字の座標を設定せずにこの位置ページを区切る。
		if (y + default_font->descent > BOTTOM_MARGIN) {
		    page->start = start;
		    page->end = i;
		    return i;
		}
	    }
	    SetCharPos(i, x, y);
	    x += width;
	} else {
	    SetCharPos(i, x, y);

	    // ラインフィードで改行する。
	    if (EqAscii2b(text[i], '\n')) {
		y += LINE_HEIGHT;
		x = LEFT_MARGIN;

		// 次の文字がページに収まらない場合、次の位置で終了する。
		// ページ区切り位置での改行は持ち越さない。
		if (y + default_font->descent > BOTTOM_MARGIN) {
		    if (i + 1 == text_length) {
			// ここで文書が終了する場合は、EOF だけのページを作らぬように、
			// EOF をぶらさげる。
			continue;
		    } else {
			page->start = start;
			page->end = i + 1;
			return i + 1;
		    }
		}
	    } else if (EqAscii2b(text[i], '\t')) {
		int tab = EM * 8;
		x = LEFT_MARGIN + (((x - LEFT_MARGIN) / tab) + 1) * tab;
	    } else {
		x += GetCharWidth(text[i]);
	    }
	}
    }
    // 全てのテキストを配置した。
    page->start = start;
    page->end = text_length;

    eof_position.x = x;
    eof_position.y = y;
    return text_length;
}

void DrawCursor(Drawable d, short x, short y)
{
    if (cursor_on) {
	XFillRectangle(disp, d, cursor_gc,
		       x - CURSOR_WIDTH / 2,
		       y - default_font->ascent,
		       CURSOR_WIDTH,
		       default_font->ascent + default_font->descent);
    } else {
	GC gc = XCreateGC(disp, d, 0, NULL);
	XSetForeground(disp, gc, BlackPixel(disp, 0));
	XFillRectangle(disp, d, gc,
		       x - CURSOR_WIDTH / 2,
		       y - default_font->ascent,
		       CURSOR_WIDTH,
		       default_font->ascent + default_font->descent);
	XFreeGC(disp, gc);
    }
	
}

void DrawCharacters(Page *page)
{
    size_t start = page->start;
    size_t i;
    for (i = start; i < page->end; i++) {
	short x = character_positions[i].x;
	short y = character_positions[i].y;

	if (IsPrint(text[i])) {
	draw: ;
	    XChar2b font_code;
	    XFontStruct *font = SelectFont(text[i], &font_code);
	    GC gc = XCreateGC(disp, back_buffer, 0, NULL);
	    XCopyGC(disp, default_gc, GCForeground | GCBackground, gc);
	    XSetFont(disp, gc, font->fid);
	    XDrawString16(disp, back_buffer, gc,
			  x, y + (font->ascent - default_font->ascent), &font_code, 1);
	    XFreeGC(disp, gc);
	} else {
	    if (EqAscii2b(text[i], '\n')) {
		// DOWNWARDS ARROW WITH TIP LEFTWARDS
		XChar2b symbol = { .byte1 = 0x21, .byte2 = 0xb2 };
		XDrawString16(disp, back_buffer, control_gc,
			      x, y,
			      &symbol, 1);
	    } else if (EqAscii2b(text[i], '\t')) {
		;
	    } else {
		goto draw;
	    }
	}
    }
}

// 次のページの開始位置、あるいは文書の終端 (== text_length) を返す。
void DrawPage(Page *page)
{
    DrawCharacters(page);
    if (GetPage(text_length) == page) {
	DrawEOF(back_buffer, eof_position.x, eof_position.y);
    }
    if (GetCurrentPage() == page) {
	XPoint pt;
	if (cursor_position == text_length) {
	    pt = eof_position;
	} else {
	    pt = character_positions[cursor_position];
	}
	DrawCursor(back_buffer, pt.x, pt.y);
    }
}

// 全てを再計算する。
void Recalculate()
{
    puts("recalc");
    GetWindowSize();
    Paginate();
    UpdateCursor();
}

#define CURSOR_ON_DURATION_MSEC 1000
#define CURSOR_OFF_DURATION_MSEC 1000

void UpdateCursor()
{
    static struct timeval last_change = { .tv_sec = 0, .tv_usec = 0 };
    struct timeval now;
    gettimeofday(&now, NULL);

    if (last_change.tv_sec == 0 && last_change.tv_usec == 0) {
	// 未初期化の場合は現在の時刻で初期化する。
	last_change = now;
	cursor_on = true;
	return;
    }

    int msec = (now.tv_sec - last_change.tv_sec) * 1000 + (now.tv_usec - last_change.tv_usec) / 1000;
    if (cursor_on) {
	if (msec >= CURSOR_ON_DURATION_MSEC) {
	    cursor_on = false;
	    last_change = now;
	    InvalidateWindow();
	}
    } else {
	if (msec >= CURSOR_OFF_DURATION_MSEC) {
	    cursor_on = true;
	    last_change = now;
	    InvalidateWindow();
	}
    }
}

Page *GetPage(size_t position)
{
    assert(0 <= position && position <= text_length);

    size_t i;
    for (i = 0; i < npages; i++) {
	if (pages[i].start <= position && position < pages[i].end)
	    return &pages[i];
    }
    if (position == text_length) {
	return &pages[npages - 1];
    } else {
	abort();
    }
}

Page *GetCurrentPage()
{
    return GetPage(cursor_position);
}

void MarkMargins()
{
    // ページのサイズ。
    const int lm	= LEFT_MARGIN - 1;
    const int rm	= RIGHT_MARGIN + 1;
    const int tm	= TOP_MARGIN - 1;
    const int bm	= BOTTOM_MARGIN + 1;

    // マークを構成する線分の長さ。
    const int len = 10;

    // _|
    XDrawLine(disp, back_buffer, margin_gc, lm - len, tm, lm, tm); // horizontal
    XDrawLine(disp, back_buffer, margin_gc, lm, tm - len, lm, tm); // vertical

    //        |_
    XDrawLine(disp, back_buffer, margin_gc, rm, tm, rm + len, tm);
    XDrawLine(disp, back_buffer, margin_gc, rm, tm - len, rm, tm);

    // -|
    XDrawLine(disp, back_buffer, margin_gc, lm - len, bm, lm, bm);
    XDrawLine(disp, back_buffer, margin_gc, lm, bm, lm, bm + len);

    //        |-
    XDrawLine(disp, back_buffer, margin_gc, rm, bm, rm + len, bm);
    XDrawLine(disp, back_buffer, margin_gc, rm, bm, rm, bm + len);
}

void Redraw()
{
    {
	XWindowAttributes attrs;
	XGetWindowAttributes(disp, win, &attrs);

	GC gc = XCreateGC(disp, win, 0, NULL);
	XSetForeground(disp, gc, WhitePixel(disp, 0));

	XFillRectangle(disp, back_buffer, gc,
		       0, 0,
		       attrs.width, attrs.height);
	XFreeGC(disp, gc);
    }

    MarkMargins();
    DrawPage(GetCurrentPage());

    // フロントバッファーとバックバッファーを入れ替える。
    // 操作後、バックバッファーの内容は未定義になる。
    XdbeSwapInfo swap_info = {
	.swap_window = win,
	.swap_action = XdbeUndefined,
    };
    XdbeSwapBuffers(disp, &swap_info, 1);

    XFlush(disp);
}

static void InitializeColors()
{
    Colormap cm;
    cm = DefaultColormap(disp, 0);

    XParseColor(disp, cm, "#00DD00", &Color.bright_green);
    XAllocColor(disp, cm, &Color.bright_green);

    XParseColor(disp, cm, "#00AA00", &Color.green);
    XAllocColor(disp, cm, &Color.green);

    XParseColor(disp, cm, "gray50", &Color.gray50);
    XAllocColor(disp, cm, &Color.gray50);

    XParseColor(disp, cm, "gray80", &Color.gray80);
    XAllocColor(disp, cm, &Color.gray80);

    XParseColor(disp, cm, "cyan3", &Color.skyblue);
    XAllocColor(disp, cm, &Color.skyblue);
}

void InitializeBackBuffer()
{
    Status st;
    int major, minor;

    st = XdbeQueryExtension(disp, &major, &minor);
    if (st == (Status) 0) {
	fprintf(stderr, "Xdbe extension unsupported by server.\n");
	exit(1);
    }

    back_buffer = XdbeAllocateBackBufferName(disp, win, XdbeUndefined);
}

void InitializeGCs()
{
    /* ウィンドウに関連付けられたグラフィックコンテキストを作る */
    default_gc = XCreateGC(disp, win, 0, NULL);
    XSetForeground(disp, default_gc,
		   BlackPixel(disp, DefaultScreen(disp)));

    XSetFont(disp, default_gc, default_font->fid);

    control_gc = XCreateGC(disp, win, 0, NULL);
    XSetFont(disp, control_gc, (default_font)->fid);
    XSetForeground(disp, control_gc, Color.skyblue.pixel);

    margin_gc = XCreateGC(disp, win, 0, NULL);
    XSetForeground(disp, margin_gc, Color.gray80.pixel);

    cursor_gc = XCreateGC(disp, win, 0, NULL);
    XSetForeground(disp, cursor_gc, Color.green.pixel);
}

void Initialize()
{

    if ( setlocale(LC_ALL, "") == NULL ) {
	fprintf(stderr, "cannot set locale.\n");
	exit(1);
    }

    disp = XOpenDisplay(NULL); // open $DISPLAY

    if (!XSupportsLocale()) {
	fprintf(stderr, "locale is not supported by X.\n");
	exit(1);
    }

    if (XSetLocaleModifiers("") == NULL) {
	fprintf(stderr, "cannot set locale modifiers.\n");
    }

    im  = XOpenIM(disp, NULL, NULL, NULL);

    if (im == NULL) {
	fprintf(stderr, "could not open IM.\n");
	exit(1);
    }

    // ウィンドウの初期化。
    win = XCreateSimpleWindow(disp,						// ディスプレイ
				DefaultRootWindow(disp),			// 親ウィンドウ
				0, 0,						// (x, y)
				640, 480,					// 幅・高さ
				0,						// border width
				0,						// border color
				WhitePixel(disp, DefaultScreen(disp)));		// background color
    XMapWindow(disp, win);
    Atom WM_DELETE_WINDOW = XInternAtom(disp, "WM_DELETE_WINDOW", False); 
    XSetWMProtocols(disp, win, &WM_DELETE_WINDOW, 1);

    InitializeBackBuffer();

    // インプットコンテキストの初期化。
    ic = XCreateIC(im,
		   XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
		   XNClientWindow, win,
		   NULL);

    if (ic == NULL) {
	fprintf(stderr, "could not create IC.\n");
	exit(1);
    }
    XSetICFocus(ic);

    // 暴露イベントとキー押下イベントを受け取る。
    XSelectInput(disp, win, ExposureMask | KeyPressMask);


    InitializeColors();

    default_font = XLoadQueryFont(disp, DEFAULT_FONT);
    InitializeFonts(disp);

    InitializeGCs();
}

void CleanUp()
{
    XFreeGC(disp, cursor_gc);
    XFreeGC(disp, margin_gc);
    XFreeGC(disp, control_gc);
    XFreeGC(disp, default_gc);

    ShutdownFonts(disp);
    XUnloadFont(disp, default_font->fid);

    XDestroyWindow(disp, win);

    XCloseDisplay(disp);

    free(text);
    free(character_positions);
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

    character_positions = malloc(text_length * sizeof(character_positions[0]));
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
    case XK_Up:
	// 2つ前の改行を見付けて、その改行の次の位置に移動する。
	// 途中で文書の先頭に来たら、止まる。
	{
	    int count = 0;
	    while (1) {
		if ( cursor_position == 0 )
		    break;

		if (count == 2) {
		    cursor_position++; // 大丈夫なはず。
		    break;
		}

		cursor_position--;
		if (EqAscii2b(text[cursor_position], '\n'))
		    count++;
	    }
	    needs_redraw = true;
	}
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
    case XK_Return:
	{
	    XChar2b ch = {
		.byte1 = 0,
		.byte2 = '\n'
	    };
	    InsertCharacter(cursor_position, ch);
	}
	break;
    default:
	{
	    char utf8[1024];
	    int len;

	    len = Xutf8LookupString(ic, ev, utf8, sizeof(utf8), NULL, NULL);
	    printf("'%.*s'\n", len, utf8);

	    char ucs2[1024];
	    iconv_t cd = iconv_open("UCS-2BE", "UTF-8");
	    char *inbuf = utf8, *outbuf = ucs2;
	    size_t inbytesleft = len, outbytesleft = 1024;

	    len = iconv(cd,
			&inbuf, &inbytesleft,
			&outbuf, &outbytesleft);
	    assert( len % 2 == 0 );
	    printf("len == %d\n", len);

	    char *p;
	    for (p = ucs2; p < outbuf; p += 2) {
		XChar2b ch = *((XChar2b*) p);
		InsertCharacter(cursor_position, ch);
	    }
	    
	    iconv_close(cd);
	}
    }
    printf("cursor = %zu\n", (unsigned long) cursor_position);

    if (needs_redraw) {
	InvalidateWindow();
    }
}

void InsertCharacter(size_t position, XChar2b character)
{
    text = realloc(text, sizeof(text[0]) * (text_length + 1));
    character_positions = realloc(character_positions, sizeof(character_positions[0]) * (text_length + 1));

    memmove(&text[cursor_position] + 1, &text[cursor_position], sizeof(text[0]) * (text_length - cursor_position));
    text[cursor_position] = character;

    cursor_position++;
    text_length++;
    InvalidateWindow();
}

void InvalidateWindow()
{
    XExposeEvent expose_event;

    memset(&expose_event, 0, sizeof(expose_event));
    expose_event.type = Expose;
    expose_event.window = win;

    XSendEvent(disp, win, False, ExposureMask, (XEvent *) &expose_event);
    XFlush(disp);
}

#include <sys/select.h>

int main(int argc, char *argv[])
{
    if (argc != 2)
	UsageExit();

    LoadFile(argv[1]);
    Initialize();

    XEvent ev;

    fd_set readfds;
    struct timeval timeout;

    timeout.tv_sec = 0;
    timeout.tv_usec = 500 * 1000;


    while (1) { // イベントループ
	struct timeval t = timeout;
	int num_ready;

	FD_ZERO(&readfds);
	FD_SET(ConnectionNumber(disp), &readfds);
	num_ready = select(ConnectionNumber(disp) + 1,
			   &readfds, NULL, NULL,
			   &t);

	// タイムアウトになった場合
	if (num_ready == 0)
	    Recalculate();

	while (XPending(disp)) {
	    XNextEvent(disp, &ev);
	    if (XFilterEvent(&ev, None))
		continue;

	    printf("event type = %d\n", ev.type);
	    switch (ev.type) {
	    case Expose:
		Recalculate();
		puts ("redraw");
		Redraw();
		break;
	    case KeyPress:
		HandleKeyPress((XKeyEvent *) &ev);
		break;
	    case ClientMessage:
		printf("WM_DELETE_WINDOW\n");
		goto Exit;
	    default:
		;
	    }
	}
    }

 Exit:
    CleanUp();

    return 0;
}
