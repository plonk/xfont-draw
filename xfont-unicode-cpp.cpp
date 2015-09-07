/**
 * Helvetica で英文を表示するプログラム。両端揃え。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include "util.h"
// time関数の為に time.h をインクルードする。
#include <time.h>
#include <ctype.h>
#include <assert.h>
// for execlp
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <iconv.h>

#define FONT "-gnu-unifont-medium-r-normal-sans-16-160-75-75-c-80-iso10646-1"
#define CHAR_HYPHEN '-'
#define PROGRAM_NAME "xfont-unicode-cpp"

static Display *disp;
static Window win;
static GC gc;
static XFontStruct *font;
static XChar2b *text;
static size_t text_length;
static size_t cursor_position;

void Redraw(const XChar2b *buff)
{
    XClearWindow(disp, win);

    
    XWindowAttributes attrs;

    XGetWindowAttributes(disp, win, &attrs);

    const int LEFT_MARGIN = 50;
    const int LINE_HEIGHT = 22;
    const int RIGHT_MARGIN = attrs.width - LEFT_MARGIN;
    const int TOP_MARGIN = 50;
    int x = LEFT_MARGIN, y = TOP_MARGIN + font->ascent;

    size_t i;

    for (i = 0; i < text_length; i++) {
	if ( text[i].byte1 != 0 || isprint(text[i].byte2) ) {
	    // 印字可能文字の場合。

	    XCharStruct *csp = GetCharInfo16(font, text[i].byte1, text[i].byte2);

	    // この文字を描画すると右マージンにかかるようなら改行する。
	    // ただし、行頭に居る場合は改行しない。
	    if ( x + csp->width > RIGHT_MARGIN && x != LEFT_MARGIN ) {
		y += LINE_HEIGHT;
		x = LEFT_MARGIN;
	    }

	    if (cursor_position == i) {
		XFillRectangle(disp, win, gc, 
			       x, y - font->ascent,
			       1, font->ascent + font->descent);
	    }

	    XDrawString16(disp, win, gc,
			  x, y,
			  &text[i], 1);
	    x += csp->width;
	} else {
	    if (cursor_position == i) {
		XFillRectangle(disp, win, gc,
			       x, y - font->ascent,
			       1, font->ascent + font->descent);
	    }

	    if (text[i].byte1 == 0 && text[i].byte2 == '\n') {
		y += LINE_HEIGHT;
		x = LEFT_MARGIN;
	    }
	}
    }
    if (cursor_position == text_length) {
	XFillRectangle(disp, win, gc,
		       x, y - font->ascent,
		       1, font->ascent + font->descent);
    }
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
				WhitePixel(disp, DefaultScreen(disp)));	// background color
    XMapWindow(disp, win);

    /* ウィンドウに関連付けられたグラフィックコンテキストを作る */
    gc = XCreateGC(disp, win, 0, NULL);
    XSetForeground(disp, gc,
		   BlackPixel(disp, DefaultScreen(disp)));

    // 暴露イベントとキー押下イベントを受け取る。
    XSelectInput(disp, win, ExposureMask | KeyPressMask);

    font = XLoadQueryFont(disp, FONT);
    XSetFont(disp, gc, (font)->fid);
}

void CleanUp()
{
    XUnloadFont(disp, font->fid);
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

void HandleKeyPress(const XKeyEvent *ev)
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
    // case XK_Down:
    // case XK_Up:
    default:
	;
    }
    printf("cursor = %d\n", cursor_position);

    if (needs_redraw) {
	XExposeEvent expose_event;

	memset(&expose_event, 0, sizeof(expose_event));
	expose_event.type = Expose;
	expose_event.window = win;

	XSendEvent(disp, win, False, ExposureMask, reinterpret_cast<XEvent *>(&expose_event));
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
	    Redraw(text);
	    break;
	case KeyPress:
	    HandleKeyPress(reinterpret_cast<XKeyEvent *>(&ev));
	    break;
	default:
	    ;
	}
    }

    CleanUp();
}
